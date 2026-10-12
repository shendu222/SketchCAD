# 标题栏表格化 + 辅助元素打印过滤 设计分析

- 项目：sketch-cad-cpp（Qt 6.10.1 / C++17 / MinGW 13.1 / Ninja，双目标 SketchCAD + SketchSelfTest）
- 日期：2026-10-10
- 任务来源：用户截图反馈两个缺陷——
  1. **标题栏文字超出单元格边界**，未正确显示在格子内；要求改为表格形式布局，所有文字在对应单元格内正确对齐并完整显示；
  2. **打印/导出 PDF 时辅助说明内容被输出**（"第一角画法"投影标志图标 + 文字）；要求打印/PDF 场景隐藏。
- 结论先行：两处缺陷均已修复，新增自测组 `t_titleBlockAux`（4 组验收、30 断言），全量自测 **2421/0**、双目标 `-Wall -Wextra` 零警告。首轮自测曾抓出"中文前缀用 `QLatin1String` 比较静默失效"的谓词缺陷并修复（详见 §4.2 实录）。

---

## 1. 根因分析

### 1.1 缺陷一：标题栏文字越界——不是"差了 1mm"，而是锚点语义被置反

标题栏文本由 `src/core/sheet_frame.cpp` 生成。旧实现的放置逻辑是：

```cpp
const double labelOff = 2.0;   // 标签距单元格顶边
const double fieldOff = 4.0;   // 字段距单元格底边
auto labelInCell = [&](double left, double top, const QString& s, double h) {
    label(left + inset, top - labelOff, s, h);      // 锚点 = 顶边 - 2mm
};
auto fieldInCell = [&](double left, double bottom, const QString& key, double h) {
    field(left + inset, bottom + fieldOff, key, h); // 锚点 = 底边 + 4mm
};
```

注释写着"标签贴单元格顶部"，即设计意图是**块自锚点向下垂挂**。但三个渲染站点（屏幕 `ElementItem::paintText`、打印 `PlotRenderer::paintText`、`TextMetrics` 同源布局）对 plain Text 的真实语义是：

- `align = isM ? e.textAlign : 1` → plain Text 恒为 align=1：`hA=1`（单行时 `wpx==maxWpx`，`x0=0` → **左对齐于锚点**）、`vA=0`；
- 变换链 `p->translate(锚点); p->scale(1/z, -1/z)`（抵消视图 Y 翻转）后，局部 `+y = 世界 -y`，首行中心落在局部 `-0.5·lineH` → **世界 `锚点 + 0.5·h`**；
- 因此块的实际范围是 `[锚点, 锚点+h]`（向上）× `[锚点.x, 锚点.x+W]`（向右）：**锚点 = 块左下角，块自锚点向右上延伸**。

两个直接后果：

1. **标签越顶**：锚点取"顶边−2mm"后，块顶 = 顶边−2+3.26 ≈ **顶边+1.26mm**，向上越出单元格约 1mm——截图中的文字压线/出格即此；
2. **长值横向溢出**：字段值（用户填写或 `InsertedTitleBlock` 预填）按固定字高渲染，宽度只增不减，直接冲出单元格右边界；空占位字段不显示，所以新建时只看见第 1 类、填表后暴露第 2 类。

> 附带发现（本轮不修，记录在案）：`TextMetrics::elementBoundsMm` 对 plain Text 单行按"块自锚点向下 + 水平居中"解释，与两个渲染器相反（仅影响取景/`modelBounds`，不影响显示与打印）。收敛它属于独立课题。

### 1.2 缺陷二：辅助说明元素无打印过滤

投影法识别符号（GB/T 14665-2012 第 7 章表5，"第一角/第三角画法"图标 + 文字）由 `buildProjectionSymbol` 生成，每个图元带 `name = "投影符号:第一角"` 标签（此前只作标识，无消费方）。打印链没有任何地方识别它：

- `PlotRenderer::paintOne` 是打印/预览/PDF/物理打印/COM 批量出图的**单一绘制咽喉**（已有"Constraint 不打印"先例），但不认识这个标签 → 符号随图纸一起输出。

---

## 2. 设计方案

### 2.1 单元格文字自适应：度量 → 收缩 → 定位（单一入口）

新增 `src/core/TitleBlockLayout.{h,cpp}`，把"把文字放进单元格"收敛为唯一实现，生成期（sheet_frame）、编辑期（填写标题栏表单）、自测三处共用：

| 步骤 | 做法 | 理由 |
|------|------|------|
| 度量 | `TextMetrics::lineWidthMm` | 屏幕/打印/取景三处同源的单一数据源；无头确定性降级（`size·h·0.62`），自测与字体无关 |
| 收缩 | 超宽按比例降字高，下限 `kFitMinHeightMm = 1.5mm` | **绝不截断/省略**：`elideToWidthMm` 的省略号违反"完整显示"要求；按比例降高保持宽高比，视觉不走形 |
| 定位 | 水平左内边距 `inset=2mm` 起；垂直标签贴顶 / 字段贴底，呼吸边 `kCellBreathMm = 1mm` | 锚点=块左下的语义下，`顶边-1mm-字高` 即"贴顶且不越界"；`底边+1mm` 即"贴底" |

配套 `titleBlockCellAt(model, x, y)`：从"标题栏"图层网格线（Line 竖/横 + Rectangle 四边）**重建**包含某点的单元格——竖线须横跨该点 Y、横线须横跨该点 X（`图号|比例` 这类部分高度竖线不会误伤底带单元格）；任一侧无界返回 invalid QRectF，调用方**放弃适配也不猜**。编辑期表单据此按真实单元格收缩；自测据此做包含性断言。

### 2.2 辅助元素打印过滤：前缀谓词 + 咽喉零输出

- `ProjectionSymbol.h/.cpp`：tag 前缀提为共享常量 `kProjectionSymbolTagPrefix = "投影符号:"`，新增谓词 `isProjectionSymbolElement(const Element&)`（`name.startsWith(前缀)`，单一事实源，与 `tb:field:` 域标签同风格）；
- `PlotRenderer::paintOne`：在 Constraint 过滤之后加 `if (isProjectionSymbolElement(e)) return;`——打印/预览/PDF/物理打印/COM 批量出图全走此咽喉，统一零输出；屏幕显示不受影响（投影符号是给看图人确认投影法的，理应留在屏幕上）。

### 2.3 为什么这样改（取舍记录）

| 备选方案 | 否决理由 |
|----------|----------|
| 打印时把符号图层设为不打印 | 符号与图框同在"图框"层，会误伤图框本身；且依赖用户记得改层设置 |
| 生成时就不画符号 | 违反 GB/T 14665-2012（图纸应有投影法识别符号），且屏幕核对价值丢失 |
| 超宽文本省略号截断 | 违反"完整显示"要求 |
| 固定小字号 | 长值仍会溢出（宽度不随内容收敛），且浪费窄格空间 |
| 物理平移图框实现"原点居中"式的整体重排 | （上一轮已否决）破坏模板坐标系共享；本轮只动文本放置，不动网格几何 |
| 只在生成期修复，不管编辑期 | 用户填表后的长值仍会溢出——截图场景恰是填表后可见，必须编辑期兜底 |

---

## 3. 修改位置与原因（逐文件）

### 3.1 `src/core/TitleBlockLayout.h` / `.cpp`（新增，CMakeLists.txt 同步登记）

- `fitCellText(text, wantHeight, left, right, bottom, top, stickTop, inset=2.0)`：三步法核心，返回 `CellTextFit{x, y, height, shrunk}`；
- `titleBlockCellAt(model, x, y)`：网格线重建单元格（编辑期/自测共用）；
- `fitTitleBlockText(Element&, cell)`：图元级封装——`name` 以 `tb:label:` 开头判为标签（贴顶），其余（`tb:field:`）判为字段值（贴底）；同时把锚点复位到规范位置（**顺带修复旧档错位**）并按需降字高。

### 3.2 `src/core/ProjectionSymbol.h` / `.cpp`（修改）

- 新增 `inline constexpr const char* kProjectionSymbolTagPrefix = "投影符号:"` 与 `bool isProjectionSymbolElement(const Element&)`；
- `buildProjectionSymbol` 内 tag 由字面量改为共享常量派生（改后缀名只动一处）。

### 3.3 `src/ui/PlotRenderer.cpp`（修改）

- `paintOne`（Constraint 检查之后）新增辅助元素零输出过滤；顶部 include `core/ProjectionSymbol.h`。
- 选此位置的原因：它是打印/预览/PDF/物理打印/COM 五条出图路径的唯一交汇点，一处过滤全部生效，与既有"Constraint 不打印"同一范式。

### 3.4 `src/core/sheet_frame.cpp`（修改）

- `buildSheetFrame` 与 `appendTitleBlockGrid`（插入标题栏/自动套框共享网格）的 `labelInCell` / `fieldInCell` 全改走 `fitCellText`：入参从 `(left, top/bottom)` 改为完整单元格 `(left, right, bottom, top)`，24 处调用点逐个核对所属单元格（图纸名称 `[bx0,xMid]×[y3,by1]`、图号 `[bx0,xNum]×[y2,y3]`、比例 `[xNum,xMid]×[y2,y3]`、共张第张 `[bx0,xMid]×[by0,y1]`、单位名称/设计/审核/批准/日期诸格，签名列按 `xMid/xNameDate` 切分）；
- 删除旧的 `labelOff/fieldOff/inset` 局部常量（内边距与呼吸边统一由 `TitleBlockLayout` 掌管，消除"同一语义两处手工拼装"）；
- `appendTitleBlockGrid` 的字段带入 `data` 预填值进 `fitCellText`——**长图名在生成期即收缩适配**，不必等用户填表。

### 3.5 `src/ui/TitleBlockForm.cpp`（修改）

- "填写标题栏"表单收集栏目时记录各字段锚点；确定时先用 `titleBlockCellAt` 按开表时的锚点重建单元格（模态对话框期间网格线不变，只读预计算，避开 `updateElement` 期间的模型变动），再在写回回调里 `fitTitleBlockText`——用户填的长值不再冲出单元格，旧档错位字段一并修复；单元格重建失败则原地不动（绝不猜）。

### 3.6 `tests/SketchSelfTest.cpp`（修改）

新增 `t_titleBlockAux`（4 组验收，详见下节）并注册。

---

## 4. 验证

### 4.1 自测组 `t_titleBlockAux`

锚定无头确定性度量（`setHeadlessOverride(true)`，与 `t_textMetrics` 同口径，组尾还原）：

1. **① 新建图纸全格包含**（A3 横放 + A5 竖放两种幅面）：标题栏 24 个文本（12 标签 + 12 字段）逐一重建单元格，断言块落入格内、垂直贴顶/贴底规范、水平左内边距 2mm、栏目数不丢失；并做**捕获力证明**——用旧实现的"顶边−2mm"放置复现越界，断言包含性检查确实抓得到（防止断言本身失效）；
2. **② 长图名收缩适配**：60 字长图名（无头估算 148.8mm > 单元格可用 113mm）经 `buildAutoFrameTitleBlock` 生成，断言字高被收缩且 ≥1.5mm 下限、收缩后块落入格内；对照组短图名字高保持 4.0 不收缩；
3. **③ 谓词单测**：第一角/第三角符号全部图元命中；普通直线、`tb:field:` 域文本、无名文本、"含而非始于"前缀的仿冒名零误报；
4. **④ 端到端出图**：`PlotRenderer::renderModel` 直渲染 QImage（8px/mm，世界→设备变换自建）——投影符号圆盘区域**零墨迹**；同图去标签对照组同区域**有墨**（证明零墨迹确由过滤生效而非渲染空转）；另断言标题栏网格线一带有墨（管道 sanity）。

### 4.2 构建与全量回归

- 双目标构建（`SketchCAD` + `SketchSelfTest`，`-Wall -Wextra`）零警告；
- 全量自测 **2421 / 0，EXIT=0**（新增组 30 断言全过）。

**首轮回归实录（自测即护栏的实证）**：第一轮全量自测 2415/6——`t_titleBlockAux` 5 条失败（谓词两命中、TB④ 符号存在性、零墨迹、组级汇总），全部同根因：谓词初版写成 `e.name.startsWith(QLatin1String(kProjectionSymbolTagPrefix))`，而前缀是**含中文的 UTF-8 字面量**——`QLatin1String` 按拉丁一逐字节解码，与 UTF-16 的 `QString::name` 永不匹配，过滤**静默失效**（符号照常打印）。对照组断言（去标签同区域有墨）当时即通过，证明渲染管道无恙、问题在过滤条件本身。修复：谓词与 tag 构造统一改 `QString::fromUtf8(kProjectionSymbolTagPrefix)`，前缀常量真正成为单一事实源。教训：**中文前后缀比较禁用 `QLatin1String`/`QLatin1Char`，一律 `QString::fromUtf8` 或 `QStringLiteral`**（与既有"CJK 标点比较用 `ch.unicode()==0xFF0C`"坑同族）。

---

## 5. 已知限制与后续

1. **旧档标签错位**：本修复生效前保存的图纸，其标题栏标签仍带 ~1mm 越顶（生成期修复只覆盖新建/插入）。缓解：编辑字段时表单会复位该字段锚点；彻底清理需"重插标题栏"或后续加修复命令（未立项）。
2. **`elementBoundsMm` 与渲染器锚点语义相反**（仅影响取景/`modelBounds`，不影响显示/打印）：独立课题，建议后续收敛到渲染器语义并补回归。
3. **A5 等小幅面上投影符号与标题栏左边界相接**（`sx = max(a+r+4, bx0-r-5)` 在窄幅面取前者时圆环会压到标题栏框线）：生成期布局问题，屏幕可见、打印已不可见；如需视觉完美可另立项微调符号避让。
4. **编辑期适配依赖网格线完整**：用户若删掉标题栏网格线，表单放弃收缩（保持原样），不猜、不崩。
