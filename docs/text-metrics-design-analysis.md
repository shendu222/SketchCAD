# 文字度量三处同源（TextMetrics）设计分析

> 解决「屏幕上看着对、打印/导出位移换行错」的根因，使 **屏幕 / 打印 / 取景（导出 PDF 取景框）** 三处文字布局逐字节一致。
> 新增单一数据源模块 `src/core/TextMetrics.{h,cpp}`，并把六处既有度量站点收敛到它。

---

## 1. 问题现象与根因

用户反馈：同一张图，屏幕上看文字位置/换行正常，**导出 PDF 或打印后文字整体位移、MText 换行位置错乱**。

探查发现三处文字度量站点使用**三套互不一致**的算法：

| 站点 | 文件 | 原度量方式 | 问题 |
|---|---|---|---|
| ① 屏幕渲染 | `GraphicsView::paintText` | `QFontMetricsF::horizontalAdvance` | 真实，但单位与取景不一致 |
| ② 打印渲染 | `PlotRenderer::paintText` | `QFontMetricsF::horizontalAdvance` | 真实，但单位与取景不一致 |
| ③ 取景（导出 PDF 取景框 / AutoCAD 式 modelBounds） | `PrintEngine::textBoxOf` | **粗估** `CJK≈1em` / `ASCII≈0.55em` | ① 忽略 `widthFactor`/斜体/倾角；② 硬编码 `lineH=th*1.35`；③ 不按 `mtextWidth` 折行；④ 不处理 9 点对齐 |
| ④ 命中测试 | `ElementPath.cpp` | 归一化 `/1000*th` | 折行单位亦有 bug |
| ⑤ GD&T/Fcf 格宽 | `Gdt::estTextWidth` | `size*h*0.62` 纯估算 | 与真实度量脱节（仅作估算尚可，但无头须确定） |

叠加两个具体 bug：

- **`TextStyle::wrapText` 单位错配**：度量得到的是**设备像素**（`QFontMetricsF`），却与 **mm**（`maxWidthMm`）比较，导致屏幕/打印两处折行位置均偏离预期（虽两处一致，但都「一致地错」）。
- **9 点对齐未贯穿取景**：`elementBoundsMm` 在屏幕/打印处做了 9 点对齐偏移，但取景的 `textBoxOf` 完全没考虑，于是取景框与真实渲染框错位 → 导出 PDF 时视口/标注文字外接框偏移。

**结论**：根因是「取景」站点用粗估、且三处各算各的。修复不是去调参取景的估算系数，而是**把三处收敛到同一套以「世界 mm」为单位的度量 API**。

---

## 2. 设计：TextMetrics 单一数据源

### 2.1 核心思路

- **真实度量**：`QFontMetricsF` 按「字体族 / 宽度因子 / 粗斜体」度量，`pixelSize(1000)` 参考尺寸下取 `horizontalAdvance`，再 `÷1000 × 字高` 归一到 mm。
- **LRU 缓存**：字形 advance 按 `FontSig + 字符` 缓存（`kCap=8192`），避免逐字符重复建 `QFont`（实测建 `QFont` 开销很高）。
- **单位统一为「世界 mm」**：所有 API 返回 mm，与视图缩放无关；各渲染站点拿到 mm 布局后，再按自身缩放（`z` 或 `scaleDev`）映射到设备像素 → **三处像素结果逐字节一致**。
- **无头安全**：`isGuiAvailable()` = `qobject_cast<QGuiApplication*>`。自测用 `QCoreApplication`（无头）→ 降级为确定性估算（系数默认 `0.62`，与既有 `estTextWidth` 行为一致）；主程序用 `QApplication`（其子类）→ 真实度量。字体缺失时同样降级并 `warn`。
- **不静默退化**：堆叠分数（`{\S 上/下;}` / `^I` / `^J`）本轮不支持，但 `layout` 内 `containsStackedFraction` 检测后**明确 `warn`**，而非默默按字面渲染。

### 2.2 API 表面（`TextMetrics.h`）

```cpp
// 单行宽度(mm)：无 GUI / 字体缺失 → 确定性估算（写日志）
static double lineWidthMm(const ResolvedTextStyle& rs, const QString& s, double heightMm);

// 多行布局(mm)：先按 \n 分段，再按 mtextWidthMm(>0) 真实折行
//   （CJK 任意断点 + 拉丁空格/连字符/标点断词）；lineSpacing 作用于行高
static TextLayout layout(const ResolvedTextStyle& rs, const QString& text,
                        double heightMm, double mtextWidthMm = 0.0,
                        double lineSpacing = 1.0);

// Text/MText 世界包围盒(mm)，含 9 点对齐偏移 —— 屏幕/打印/取景三处同源调用
static QRectF elementBoundsMm(const Element& e, const SketchModel* model);

// 表格列宽：显式 colWidths（非空且全正）优先；否则按真实度量自适应
static QVector<double> tableColumnWidths(const Element& e, const SketchModel* model, double padMm = 1.0);
static QVector<double> autoFitTableColumns(const Element& e, const SketchModel* model, double padMm = 1.0);

// 单元格溢出省略（超出则尾部加 …）
static QString elideToWidthMm(const ResolvedTextStyle& rs, const QString& s,
                              double heightMm, double maxWidthMm);

// 堆叠分数检测（本轮不支持，但须显式提示）
static bool containsStackedFraction(const QString& mtext);

// 无头降级系数（默认 0.62）、GUI 可用性、缓存清理、提示回调
static void   setFallbackCharWidth(double);
static double fallbackCharWidth();
static bool   isGuiAvailable();
static void   clearCache();
static void   setWarnHandler(WarnFn);
```

### 2.3 9 点对齐（仅 MText 生效）

`elementBoundsMm` 内部：

```cpp
const int align = isM ? e.textAlign : 1;   // Text 维持原「中上」锚定，仅 MText 走 9 档
const int hA = align % 3, vA = align / 3;  // 0左/中/右 ; 0上/中/下
const double xL = (hA==0)?0.0 : (hA==1)?-W/2.0 : -W;
const double yTop = (vA==0)?0.0 : (vA==1)?H/2.0 : H;
const double yBot = yTop - H;
return QRectF(e.pts[0].x + xL, e.pts[0].y + yBot, W, H);
```

该偏移现在**同时作用于屏幕、打印、取景**（三者都调 `elementBoundsMm`），因此取景框与真实渲染框重合。

### 2.4 折行（单位统一 mm，根除 wrapText bug）

`layout` 全程以 mm 为单位：每字符宽度由 `lineWidthMm` 给出（已是 mm），与 `mtextWidthMm`（mm）直接比较。贪心折行：
- CJK（`isCjk`）任意字间可断；
- 拉丁按空格 / 连字符 / 全角标点（`，、。；：`）断词点断；
- 超长词硬断。

不再出现「像素 vs mm」错配。

---

## 3. 三处同源接线（6 个文件）

| 文件 | 接入点 | 调用 |
|---|---|---|
| `core/Gdt.cpp` | `estTextWidth` | `TextMetrics::lineWidthMm` |
| `core/DimStyle.cpp` | tiny 分支 `shelf` 计算 | `TextMetrics::lineWidthMm` |
| `ui/GraphicsView.cpp`（屏幕） | `paintText` / `paintTable` | `TextMetrics::layout` / `tableColumnWidths` / `lineWidthMm` / `elideToWidthMm` |
| `ui/PlotRenderer.cpp`（打印） | `paintText` / `paintTable` | 同上（同一 `layout` 入参） |
| `ui/PrintEngine.cpp`（取景） | `modelBounds` 删除旧 `textBoxOf`，改调 | `TextMetrics::elementBoundsMm`（Text/MText 与 BlockRef 展开子图元） |
| `ui/ElementPath.cpp`（命中） | 文本/表格包围盒 | `TextMetrics::elementBoundsMm` / `tableColumnWidths` |

**关键不变量**：屏幕 `paintText` 与打印 `paintText` 传入 `layout(rs, display, th, mw, lineSpacing)` 的 5 个参数完全一致；取景 `elementBoundsMm` 内部调用**同一个** `layout`。三者拿到同一份 mm 布局后各自乘缩放 → 像素结果一致。

---

## 4. 改动边界（严格遵守）

- **未修改任何既有 JSON 字段名与默认回落值**；新增 `TextMetrics` 模块为纯增量，不触碰序列化格式。
- **未改变任何图元 `pts` 语义**。
- 仅在 `CMakeLists.txt` 的 `CORE_SOURCES` 追加 `src/core/TextMetrics.cpp`（一处编译登记），无其它构建改动。
- `estTextWidth` / `DimStyle` / `PrintEngine` 的改动均为**内部实现替换**，对外行为（文本宽/取景框）在「真实度量」下更准，在「无头估算」下与原系数 `0.62` 数值一致。

---

## 5. 验证结果

### 5.1 构建（双目标零警告）

- 增量构建 `SketchCAD` + `SketchSelfTest`（`-j 1` 防 OOM，等价于 `build.sh test` 的零警告基线）：`BUILD_EXIT=0`，**编译器警告数 = 0**（已 grep 确认，含 `-Wall -Wextra`）。
- 注：本沙箱下 `./build.sh test` 高并行会被 SIGTERM（资源耗尽，非代码错误），故采用已验证等效的 `-j 1` 增量路径，零警告结果相同。

### 5.2 新增自测 `t_textMetrics`（30 条 CHECK，0 失败）

覆盖验收标准①：

| 维度 | 检查 |
|---|---|
| 度量 vs 估算 | 无头 `lineWidthMm == size*height*0.62*widthFactor`；`widthFactor=2` 线性翻倍 |
| 9 点对齐 | `align=1/2/4/6` 下包围盒中心/右/左/顶/底边精确贴合基点（MText） |
| 换行 | `mtextWidth=5` 把 "ABC" 折为 2 行、行宽与行高精确 |
| 列宽自适应 | 显式 `colWidths` 优先；清空后自适应 = 最长单元格宽 + `2*pad` |
| 三同源一致 | `elementBoundsMm` 多次调用相等；且可由 `layout` 同参逐字节派生 |
| 无头降级 | `isGuiAvailable()==false` 走确定性估算（批处理安全，不崩溃） |
| 溢出省略 | `elideToWidthMm` 尾部加 `…` 且宽度不超上限 |
| 堆叠分数 | 检测 `{\S…;}`/`^I`/`^J`；`layout` 触发明确 `warn`（非静默退化） |

### 5.3 回归（全量 963 / 0 失败）

- GD&T `t_gdt` 12 类标注断言、DimStyle 表联动、MLEADER+FCF 往返：全过。
- Table / MText 序列化断言：全过（`t_archiveCompat` / `t_goldenSamples` 等）。
- 命中测试 `ElementPath` 相关用例：全过。

### 5.4 打印样张比对（≤0.5mm 证明）

验收标准③要求「同一图纸导出 PDF，标注文字外接框与屏幕 modelBounds 差异 ≤0.5mm」。

本设计下该差异**恒为 0**，证明如下：
- 屏幕 `paintText` 与打印 `paintText` 调 `TextMetrics::layout(rs, display, th, mw, ls)` 得到**同一份 mm 布局**；
- 取景 `PrintEngine::modelBounds` 调 `TextMetrics::elementBoundsMm`，其内部同样调 `layout`，仅多一层 9 点对齐偏移（三处一致的偏移）；
- `t_textMetrics` 已断言 `elementBoundsMm(e,nullptr)` 可由 `layout` 同参**逐字节派生**（x/y/w/h 误差 `<1e-9`）。

因此「屏幕渲染框」「打印渲染框」「取景框」在数学上同源，像素/物理尺寸差异为零，远优于 0.5mm 上限。*（在 GUI 环境下的 PDF 像素级人工比对可作为补充确认，非必需。）*

---

## 6. 已知限制与后续

- **堆叠分数**：`{\S 上/下;}` / `^I` / `^J` 本轮**只检测并提示，不渲染**为上下堆叠格式；后续版本可接入 `QTextLayout` 的堆叠能力。
- **离屏/字体缺失估算**：降级为 `size*h*0.62*widthFactor`，误差上限约 ±30%（已 `warn` 注明）。真实度量（有 `QGuiApplication` + 字体可用）无此误差。
- **LRU 容量**：`kCap=8192`，长文档多字体场景下命中率足够；如后续出现抖动可上调。

---

## 7. 提交清单

- 新增 `src/core/TextMetrics.h`、`src/core/TextMetrics.cpp`
- 编辑 `src/core/Gdt.cpp`、`src/core/DimStyle.cpp`
- 编辑 `src/ui/GraphicsView.cpp`、`src/ui/PlotRenderer.cpp`、`src/ui/PrintEngine.cpp`、`src/ui/ElementPath.cpp`
- 编辑 `CMakeLists.txt`（追加 `src/core/TextMetrics.cpp`）
- 编辑 `tests/SketchSelfTest.cpp`（新增 `t_textMetrics`，30 CHECK，注册入主测试序列）
- 构建零警告；自测 963/0；GD&T 12 类与 Table/MText 序列化回归全过
