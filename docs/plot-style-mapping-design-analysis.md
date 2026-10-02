# SketchCAD 打印样式映射（黑白出图线宽/灰度）：设计与深思

> 配套任务：为 `PrintEngine` 增加基于颜色的打印样式映射，确保黑白打印机及黑白 PDF 出图时
> **线宽与灰度符合 GB/T 14665**，杜绝「彩色线宽图打印必错」，达成预期的线宽分组效果。
> 范围：样式映射表（颜色/图层/线型 → 线宽/灰度/抖动）、简化 CTB 导入与 GB 预设、
> 打印链渲染前画笔替换、页面设置 UI 选择项、COM 批量出图透传。
> 约束：映射仅作用于打印链（屏幕显示不受影响）；未选样式表时保持现有 G3 线宽组默认行为；
> JSON 存档向后兼容（新增字段容缺）。
> 验收：A4/A3 黑白 PDF 样张目检签字；带/不带样式表双路打印回归比对；
> 自测新增 ≥4 断言覆盖映射解析与画笔替换纯函数层；构建零警告。

---

## 1. 问题的真正根因：不是「颜色错了」，而是「分组信息载体错了」

「彩色线宽图打印必错」这句话容易把人引向「把颜色映射成黑色」这个方向，但那只解决了一半。

真正的因果链是：

1. 屏幕上区分「粗实线 / 细实线 / 虚线 / 点画线」时，用户往往**同时**使用两种载体：
   **颜色**（红/蓝/绿/黄）与**线宽**（粗档/细档）；
2. 彩色显示器上两者都在，信息冗余，怎么画都不歧义；
3. 黑白打印机 / 黑白 PDF 上，**颜色这一整个维度瞬间归零**——红和绿都变成同一个中灰，
   原本靠颜色区分的线条在纸上完全无法分辨；
4. 若此时线宽又没有按 GB/T 14665 的粗/细两档显式给定，那么「线宽分组」这一唯一幸存的
   载体就退化成图元的**自动派生线宽**（`defaultWidthForStyle`），它由线型决定、与图幅线组
   有关但不受出图意图控制——于是「分组效果」不可控，表现为「打印必错」。

所以本设计的核心命题被定义为一句话：

> **灰度化只负责「去掉颜色」，线宽分组必须由样式映射表显式指定。**
> 二者是**正交**的两件事，绝不能用「转灰度」顺带「猜」线宽。

这条命题直接决定了后文所有数据结构与不变式。

---

## 2. 数据结构：三个匹配维度 + 三个正交属性

### 2.1 条目 `PlotStyleEntry`

在原有 `overrideColor/overrideWidth` 之上新增三个字段：

| 字段 | 含义 | JSON key | 旧档缺省 |
| --- | --- | --- | --- |
| `overrideGray` | 是否强制灰度化 | `overrideGray` | `false` |
| `gray` | 目标灰度 0=黑 1=白 | `gray` | `0.0` |
| `dither` | 灰度化后是否网点抖动 | `dither` | `false` |

**向后兼容的三个「不动点」**（`tests` 中逐条断言）：

1. 旧档 JSON 无这三个 key → `fromJson` 用 `value(...).toBool(false)` / `toDouble(0.0)` 兜底；
2. 兜底后 `overrideGray=false` → `resolve()` 不做灰度化 → 输出颜色与旧版**逐字相同**；
3. 未选样式表（`plotStyle.empty()` 且 `monochrome=false`）→ `effectivePlotStyle()` 返回空表
   → 所有 `resolve()` 走「沿用图元自身属性」分支 → 仍是既有 G3 线宽组默认行为。

也就是说，**本次改动对「不用样式表的用户」是零行为变更**，这是硬约束「未选择样式表时保持现有
G3 线宽组默认行为不变」的技术兑现方式。

### 2.2 匹配维度：图层 > 颜色 > 线型

原有实现只有两个维度（图层、颜色）。GB 黑白预设需要**按线型**分组（粗实/细实/虚线/点画线），
因为国标的粗细分类本身就是按线型定义的。故新增第三张表 `byStyle_`：

```cpp
QMap<QString, PlotStyleEntry> byLayer_;   // 图层名 → 条目
QMap<int,     PlotStyleEntry> byColor_;   // QColor::rgba() 整数键 → 条目
QMap<int,     PlotStyleEntry> byStyle_;   // (int)LineStyle → 条目
```

`resolve()` 采用**首个命中即返回**（不再向下匹配）：

```cpp
const PlotStyleEntry* hit = nullptr;
auto lit = byLayer_.find(el.layer);
if (lit != byLayer_.end())      hit = &lit.value();
else { /* 颜色 */ } else { /* 线型 */ }
```

> 为什么是「首个命中即返回」而不是「逐维度叠加合并」？叠加语义会让「图层给宽度、颜色给灰度」
> 这类组合产生难以预期的结果（用户无法预测最终线宽），而**优先级短路**的语义可预测性更强：
> 用户一旦为某图层设了条目，就完全掌控它，不受图元颜色干扰。这与 AutoCAD CTB「先按对象再按颜色」
> 的短路思路一致。

### 2.3 灰度化不改线宽（第 1 节命题的代码兑现）

```cpp
if (r.gray) r.color = plotGrayscale(r.color, r.grayValue);
// 注意：此行之后 widthMm 原封不动
```

`plotGrayscale` 是纯函数：

```cpp
const int v = int(std::lround(g * 255.0));   // g=0→0(黑)，g=1→255(白)
return QColor(v, v, v, c.alpha());           // 保留 alpha
```

自测中专门断言「灰度化后 `widthMm` 不变」，把这条不变式钉死，防止后来者「顺手」在灰度分支里
也去改线宽——那正是「彩色线宽图打印必错」的另一种复现。

---

## 3. GB 预设与简化 CTB

### 3.1 `gbPreset(LineWidthGroup)`：按线型分粗/细档

```cpp
const double thick = groupThickMm(group);   // G3 → 1.0
const double thin  = groupThinMm(group);    // G3 → 0.5
t.setStyleStyle(LineStyle::Solid,        mk(thick));   // 粗实线
t.setStyleStyle(LineStyle::ThickDashed,  mk(thick));   // 粗虚线
t.setStyleStyle(LineStyle::ThickDashDot, mk(thick));   // 粗点画线
t.setStyleStyle(LineStyle::Dashed,       mk(thin));    // 细虚线
... 其余细线型同样 mk(thin)
```

其中 `mk(w)` 统一设 `overrideGray=true, gray=0, dither=false`——**线宽图要锐利，不抖动**。
粗/细档直接取自 `LineWidthGroup.h`（GB/T 14665-2012 表 1，粗:细恒 2:1），预设本身不硬编码任何
线宽数值，因此切换图幅线组（G1..G5）后预设自动跟随。

`gbPresetGrayDither()` 在此基础上开全局灰度 + 抖动，用于剖面/填充等需要中间调层次的场景。

### 3.2 简化 CTB 文本格式

真实 AutoCAD CTB 是二进制，本项目取其「行式文本子集」，便于版本管理/审阅/交付：

```
# 注释
LAYER  轮廓   WIDTH=0.70 GRAY=0.0
COLOR  #FF0000 WIDTH=0.35 DITHER=1
STYLE  DASHED WIDTH=0.50
GRAYSCALE 0.0 DITHER=1
```

`fromCtb()` 采用**宽容解析**：

- 未知关键字 / 非法数值 → 记录错误、跳过该行，**已解析的行照常生效**；
- 行内任一 token 出错 → 整行丢弃（不写半成品条目）；
- 返回 `false` 并给出**第一条**错误串，供 UI 弹窗提示。

`toCtb()` 为其逆运算，便于把现场样式表导出给第三方或纳入存档审阅。

---

## 4. 三个真实踩坑（都是自测抓出来的，不是设想出来的）

这一节记录本次实现中**由自测断言暴露、并最终修复**的三个缺陷。它们都属于「看起来能跑、
实际全错」的类别，值得单独留档。

### 4.1 `QLocale::c().toDouble("0.70")` 对合法字符串回报失败

现象：CTB 导入全部失败，报 `WIDTH 非法: 0.70`，而打印出的字符码是 `48,46,55,48`
（即 `"0.70"`，完全合法）。

排查过程：写独立探针程序编译链接同样的 Qt 6.10.1/MinGW 13.1，发现**三种写法在探针里全部成功**：
`QLocale::c().toDouble` / `QString::toDouble` / `QString::fromLatin1().toDouble`。
再回到工程内联打印，发现 `QLocale::c().toDouble` **返回了正确的数值却把 `ok` 置为 `false`**，
而同一位置用 `QString::toDouble` 解析同一字符串则 `ok=true`。

结论：在本工具链组合下 `QLocale::c()`（C 区域、language 为空）这一路径的 `ok` 回报不可信。
**修复**：自写 `ctbToDouble()` 作为全工程唯一数值入口，先把 `,` 归一为 `.`（抵御 de_DE 类
逗号小数区域），再走 `QString::toDouble`：

```cpp
static bool ctbToDouble(const QString& s, double* out) {
    QString t = s.trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok && out) *out = v;
    return ok;
}
```

> 教训：**「解析成功与否」和「解析出的值」必须分别验证**。只看 `ok` 会被区域设置坑，
> 只看值又会被静默失败坑。

### 4.2 `#` 既是注释符又是颜色前缀

最初的行尾注释处理是 `ln.left(ln.indexOf('#'))`，这会把 `COLOR #FF0000 WIDTH=0.35` 从中间
截断成 `COLOR`，导致颜色映射整条失效。

修复规则：扫描每个 `#`，统计其后连续十六进制字符数，**长度为 3/6/8 视为颜色 token
（`#RGB`/`#RRGGBB`/`#AARRGGBB`），否则视为注释起点**。

### 4.3 `continue` 只跳出内层循环，半成品条目被写入

`for (int i = 2; ...)` 的 token 循环里遇到非法参数执行 `continue`，看似跳过了该 token，
但**外层的 `setLayerStyle(target, e)` 仍会执行**，于是一个 `WIDTH=abc` 的条目被以默认值
（0.25mm / 不灰度）写入表中——比直接报错更危险，因为用户看不出样式表已被污染。

修复：引入 `lineBad` 标志，循环结束后 `if (lineBad) continue;` 整行丢弃。

### 4.4 附带：`QPixmap::setPixel` 不可用

`makeDitherBrush` 最初用 `QPixmap::setPixelColor/setPixel` 写网点贴图，本工程编译单元报
`no member named 'setPixel'`。改用 `QImage`（`Format_RGB32`）逐像素写入后再
`QPixmap::fromImage()`，同时天然规避了「设备无关位图不能直接读写像素」的 Qt 约束。

### 4.5 样张工具：QImage 设备上的双重 painter + DPI 失配

生成双路回归图时首版输出**全白**（两路 PNG 字节数完全相同、彩色像素 0）。两个叠加原因：

1. **双重 painter**：我先 `QPainter p(&img)`，再调 `PrintEngine::render(&m, &img, s)`，
   而后者内部自行 `QPainter p(dev)` —— 同一设备已有活动 painter，`begin()` 失败，
   `render` 直接返回，图像保持初始白底。修复：不再提前激活 painter。
2. **DPI 失配**：`PrintEngine::devicePxPerMm()` 读 `dev->logicalDpiX()`，而 `QImage` 默认为
   **96dpi**。我按 300dpi 计算画布尺寸、引擎按 96dpi 换算可打印区，两者不一致。
   修复：`img.setDotsPerMeterX/Y(300/0.0254)` 显式声明 DPI。

> 这两条都属于「离线出图工具」与「真实打印设备（QPdfWriter 走 300dpi）」之间的环境差异，
> 提醒我们：**用 QImage 充当打印设备做回归时，必须显式对齐 DPI 与 painter 生命周期**。

### 4.6 对照组基线选错会让回归测试永远「通过」

首版对照组设了 `monochrome=true` 想表达「仅灰度」，但 `effectivePlotStyle()` 的语义是
「`monochrome` 且样式表为空 → 自动套 GB 预设」——于是对照组也拿到了同一份 GB 预设，
两路输出**逐像素相同**，L1=0，回归测试形式上「无差异」。

修复：对照组必须是**完全不带样式表、不带 monochrome** 的原始彩色出图，
即真正复现「彩色线宽图直接打到黑白打印机」这个错误基线。

> 教训：回归测试的对照组必须精确对应「修复前的真实行为」。用「另一个也开了修复开关的
> 配置」当对照组，会得到「无差异」的假通过。**断言 `PASS` 之前，先确认它在修复前会 `FAIL`。**

### 4.7 `QPageSize::size()` 不含纸张方向，样张右侧被裁掉 87mm

目检 `gray_A4_style.png` 时发现**第 4 列线段与外框右边完全不可见**，只剩 3 列。
初判是样张几何参数画得太宽（模型 225mm vs A4 可打印 190mm），于是把线段间距从
`k*60+45` 收到 `k*50+40`、外框从 `(-15,18)~(210,-100)` 收到 `(-12,16)~(205,-100)`——
**重编译后裁切依旧**，说明根因不在几何。

真正原因在样张工具的画布构造：

```cpp
const QSizeF szMm = size.size(QPageSize::Millimeter);   // A4 → 210×297（纵向！）
const int W = int(szMm.width()  * pxPerMm);            // 建出纵向画布
const int H = int(szMm.height() * pxPerMm);
```

`QPageSize::size()` 返回的是**纸张固有尺寸**（永远是纵向基态），与 `orientation` 无关。
而 `PrintEngine::render()` 内部走 `effectivePageW/H()`，横向下按 **297×210** 排版：

```cpp
return (s.orientation == QPageLayout::Landscape) ? ps.height() : ps.width();
```

于是**画布 210mm 宽、排版 297mm 宽**，右侧 87mm 直接落在画布外被裁。PNG 尺寸
`780×1054`（纵向）本身就是最直接的证据——而设置里明明写的是 `Landscape`。

修复：画布尺寸按方向交换宽高，与 `effectivePageW/H()` 保持同一套换算。

```cpp
const double pageWmm = (s.orientation == QPageLayout::Landscape) ? szMm.height() : szMm.width();
const double pageHmm = (s.orientation == QPageLayout::Landscape) ? szMm.width()  : szMm.height();
```

修复后 PNG 变为 `3507×2480`（横向），4 列线段与外框四边完整闭合。

> 教训：`QPageSize::size()` 是**纸张尺寸**，`QPageLayout` 才是**版面尺寸**，两者差一个
> 方向换算。凡是「离屏设备尺寸」与「打印设置」两处独立计算的地方，都必须复用引擎
> 内部的换算函数，否则必然出现静默裁切。**这类 bug 不报错、只丢内容**——第一反应
> 怀疑几何参数是本能误导，必须先看输出图的宽高比是否与设置的方向一致。
> 注意 PDF 路径不受影响：`QPdfWriter::setPageOrientation()` 自己处理了方向，
> 所以**只有离屏 QImage 回归图会错**，这也解释了为何 PDF 样张一直是好的。

---

## 5. 打印链接入：抖动画刷与设备无关的网点尺寸

### 5.1 调用点

```cpp
// PrintEngine.cpp
PlotRenderer::renderModel(&p, model, scaleDev, clipWorld,
                          s.effectivePlotStyle(), s.lineWidthScale);
```

灰度信息经 `resolve()` 已经在 `rs.color` 中了，**所有** `paint*` 分支
（线/剖面/标注/文字/块引用/表格/边界）因为统一使用 `rs.color` 而**自动继承灰度化**，
无需逐处改动。这是把「颜色决策」收敛到 `resolve()` 一处的收益。

### 5.2 抖动仅作用于填充面

```cpp
if (rs.gray && rs.dither) {
    p->setBrush(makeDitherBrush(rs.grayValue, c.scaleDev));   // 网点
} else {
    p->setBrush(rs.color);                                    // 实灰度
}
```

只接在 `paintHatch` 的 `SolidFill`（狭小区域涂黑）分支：线宽图的**轮廓线**若抖动会变毛、
失去锐度，而**面状填充**抖动才有意义（恢复中间灰层次）。这是按「笔画 vs 填充」区分抖动的必要性。

### 5.3 网点尺寸必须锚在设备空间

`QBrush` 的贴图 tile 默认随世界变换缩放。图幅放大时网点会跟着变粗、退化成灰块。
修复：`br.setTransform(QTransform::fromScale(1.0/scaleDev, 1.0/scaleDev))` 抵消世界变换，
使网点在**纸面上恒为固定物理尺寸**（默认 4 设备像素）。

密度定义为 **黑点数 ∝ (1 − gray)**：gray=0 全黑、gray=1 全白，方向与「gray 越大越浅」一致。

---

## 6. UI 与 COM 透传

### 6.1 页面设置

`PrintSettingsDialog` 的「打印样式 (简化 CTB)」组新增：

- **样式表预设**下拉：`无` / `GB 黑白出图` / `GB 灰度抖动` / `自定义`；
- **导入 CTB...** 按钮：读文件 → `fromCtb` → 解析有瑕疵时弹「已跳过哪些行」；
- **黑白出图(打印机/PDF)** 总开关：写入 `PrintSettings::monochrome`；
- 匹配维度加 **「按线型」**（弹中文列表，内部转 `plotStyleName`）；
- 条目编辑加 **灰度 / 抖动** 两个控件；
- 列表项描述附灰度后缀（如 `灰度0.30(抖动)`），**目检前无需点开每条就能确认黑白意图**。

`monochrome` 与预设的分工：`effectivePlotStyle()` 里**只有当表为空时**才套 GB 预设，
用户手工建过表就以手工表为准——避免「勾了黑白开关就把精心设的线宽分组抹掉」。

### 6.2 COM 批量出图

`BatchTask` 新增两个字段，`exportDrawing` 加同名默认参（ABI 友好）：

```cpp
QString ctbText;   // 简化 CTB 文本
QString preset;    // "gb" | "gbGray" | "none"
```

任务 JSON 里支持 `"styleTable"` 字段，**自动判别**是预设名还是 CTB 文本（含换行或
`LAYER`/`COLOR`/`STYLE`/`GRAYSCALE` 关键字 → CTB 文本）。`ctbText` 优先于 `preset`。

外部系统无需手工拼整份 `PrintSettings` JSON 即可为每张图指定黑白线宽/灰度。

---

## 7. 验收对照

### 7.1 编译与自测

| 项目 | 结果 |
| --- | --- |
| `SketchCAD` 构建 | EXIT=0，-Wall -Wextra 零警告 |
| `SketchSelfTest` 构建 | EXIT=0，零警告 |
| `SketchBenchmark` 构建 | EXIT=0，零警告 |
| `PlotStyleSamples` 构建 | EXIT=0，零警告 |
| 自测用例 | **通过 344 / 失败 0**（315 → 344，新增 `t_plotStyle()`） |

`315 → 344` 的 29 条新断言覆盖：映射解析（颜色→线宽）、优先级（图层>颜色>线型）、
灰度纯函数（0/0.5/1 三点）、灰度不改线宽、GB 预设粗细分组（G3: 1.0/0.5）、
CTB 四关键字解析、CTB 宽容容错、JSON 往返、旧档缺键兼容。

### 7.2 A4/A3 黑白样张（目检）

样张由 `PlotStyleSamples` 生成，四类线型各一行、每行四条同色线，**刻意用四个明显不同的彩色**
以复现「彩色线宽图」场景：

| 文件 | 用途 |
| --- | --- |
| `bw_A4_style.pdf` / `bw_A3_style.pdf` | 带 GB 样式表的**黑白**样张（待目检签字） |
| `bw_A4_nostyle.pdf` / `bw_A3_nostyle.pdf` | **对照组**：完全不带样式表、不带 monochrome 的原始彩色出图 |
| `gray_A4_style.png` / `gray_A4_nostyle.png` | 双路回归用的灰度位图（同一条 `PrintEngine::render` 链路） |
| `regression.txt` | 双路灰度直方图/墨量/彩色像素比对报告 |

对照组刻意取「原始彩色 + 自动派生线宽」，即**修复前彩色线宽图直接打印的真实行为**；
若对照组也开 `monochrome`，`effectivePlotStyle()` 会给它套同一份 GB 预设，两路将逐像素相同，
回归测试就成了假通过（详见 4.6）。

**目检要点**（对照 GB/T 14665）：

- 粗实线：最粗且纯黑；
- 细实线：约为粗线一半宽、纯黑；
- 虚线：细档、线段间断均匀；
- 点画线：细档、长短段交替。

> 签字确认由人工在打开 PDF 后完成（本记录为样张产出与自动化回归部分）。

### 7.3 双路打印回归比对

`PlotStyleSamples` 对「带样式表 / 不带样式表」两路各渲染一份灰度图（复用同一条
`PrintEngine::render` 链路，故比对的即是真实出图路径），统计：

- 平均灰度、总墨量 Σ(255−g) 及相对变化；
- 256 桶灰度直方图的 L1 差异；
- **彩色像素计数**（R≠G≠B 的抽样像素）——这是「黑白化是否彻底」的直接判据。

判定：两路差异可测（样式表确实改变了出图）且**带样式表组彩色像素为 0**（黑白化正确）。

**实测结果（A4 / A3 双 PASS，EXIT=0）**：

| 指标 | A4 带样式表 | A4 不带 | A3 带样式表 | A3 不带 |
| --- | --- | --- | --- | --- |
| 平均灰度 | 251.16 | 251.78 | 253.06 | 253.36 |
| 总墨量 Σ(255−g) | 33 362 717 | 28 020 543 | 33 804 707 | 28 462 547 |
| 墨量变化 | **+19.1%** | — | **+18.8%** | — |
| 灰度直方图 L1 | 119 626（1.3754%） | | 112 052（0.6442%） | |
| 彩色像素（抽样） | **0** | 15 471 | **0** | 14 522 |
| 判定 | **PASS** | | **PASS** | |

三点关键读数：

1. **彩色像素 0 vs 15471** —— 样式表组彻底灰度化，对照组保留原始彩色。黑白化正确。
2. **墨量 +19.1% / +18.8%** —— 差异方向正确且量级合理。GB 预设把粗实线拉到 1.0mm、
   细线类压到 0.5mm（2:1 分组），而对照组四类线全是同一自动派生宽度。
   带样式表组落墨更多 = **线宽分组真实生效**，而非仅仅「变成了灰色」。
3. **L1 差异 1.38% / 0.64%** —— 远超 `1e-4` 判定阈值，不是噪声。

**目检结论**（对 `gray_A4_style.png` / `bw_A4_style.pdf`）：

- 四列线段完整可见、外框四边闭合（横向 3507×2480，见 4.7 纸张方向修复）；
- 粗实线最粗且纯黑，细实线明显较细（约一半宽），虚线为细档间断，点画线为细档长短段交替；
- 四类线型纯灰度下仍可凭**线宽**清晰区分 —— 即本任务要达成的核心目标。

对照 `gray_A4_nostyle.png` 可见：四类线**粗细完全一致**，仅靠红/蓝/绿/黄区分。
这正是「彩色线宽图打印必错」的现场：一旦落到黑白输出机或黑白 PDF，颜色维度归零，
四类线将无法辨识。

> PDF 样张的人工**签字确认**仍待人工在阅读器中打开
> `bw_A4_style.pdf` / `bw_A3_style.pdf` 后完成。

---

## 8. 经验

- **把「颜色决策」收敛到 `resolve()` 一处**，让所有渲染分支自动继承，是本次改动能只动
  一行调用点就覆盖全部图元类型的原因；分散决策必然漏改某一类图元。
- **「灰度」与「线宽」正交**是本任务最容易做错的地方。一旦让灰度分支顺手改线宽（或反过来
  让线宽映射隐含改灰度），就会产出「看起来是黑白图、但分组是乱的」这种更难排查的问题。
- **宽容解析必须有「整行丢弃」兜底**：只跳过坏 token 会写出半成品条目，比严格报错更危险。
- **`ok` 标志不可单独信任**：本工程实测 `QLocale::c().toDouble` 返回真值却置 `ok=false`。
  涉及用户输入的数值解析，建议自写归一化包装并在自测中用「预期值」而非仅「ok」断言。
- **逗号 vs 句点小数点**是 CTB/配置文件解析的国际性坑点，归一化比依赖 `QLocale` 更稳。
- 验收样张要**刻意复现失败场景**（用四个高对比彩色画线），而不是用「本来就黑白」的图去验证
  黑白出图——后者会让回归测试永远通过。
- **`QPageSize::size()` 是纸张尺寸，不含方向**。离屏 QImage 当打印设备时，画布尺寸必须
  复用引擎的 `effectivePageW/H()` 换算，否则画布纵向、排版横向，静默裁掉 87mm。
  这类 bug 不报错只丢内容，且**第一反应怀疑几何参数是本能误导**——应先看输出图宽高比
  是否与设置方向一致。（`QPdfWriter` 路径自带方向处理，故只有离屏回归图会错。）

---

*文档生成：本任务交付物之一；配套代码 `src/core/PlotStyle.{h,cpp}`（灰度/抖动/GB预设/CTB）、
`src/ui/PrintEngine.{h,cpp}`（monochrome + effectivePlotStyle）、`src/ui/PlotRenderer.cpp`
（makeDitherBrush + 灰度画刷）、`src/ui/PrintSettingsDialog.{h,cpp}`（预设/导入/灰度控件）、
`src/com/BatchPlotEngine.{h,cpp}`（ctbText/preset 透传）、`bench/PlotStyleSamples.cpp`
（A4/A3 样张 + 双路回归）、自测 `t_plotStyle()`。*
