# 打印 / 出图模块设计分析

> 在 SketchCAD 中新增「打印」功能模块，覆盖：页面设置、模型空间打印、简化版 CTB 样式映射、
> PDF 输出(QPdfWriter)、可视化预览、布局/视口 MVP。本文件说明设计要点与集成方式。

## 1. 需求 → 实现映射

| # | 需求 | 实现位置 |
|---|------|----------|
| 1 | 页面设置（纸张/方向/页边距/比例/居中） | `PrintSettingsDialog` + `PrintSettings`(`PrintEngine.h`) |
| 2 | 模型空间打印（免布局切换） | `PrintEngine::render` 视口为空分支：整体 `modelBounds` → 可打印区 |
| 3 | 简化版 CTB 映射（按颜色/图层指定线宽与颜色） | `PlotStyle.h/.cpp`（`PlotStyleTable`）+ `PlotRenderer` 解析应用 |
| 4 | PDF 输出（优先 QPdfWriter） | `PrintEngine::exportPdf` + 「导出 PDF...」入口 |
| 5 | 可视化打印预览 | `PrintPreviewDialog`（基于 `QPrintPreviewWidget`） |
| 6 | 布局/视口 MVP | `PlotViewport`(`Layout.h`) + `PrintSettings::viewports`；非空时逐视口映射到可打印区 |

## 2. 文件清单

核心（core，无 QtWidgets 依赖）：
- `src/core/PlotStyle.{h,cpp}` —— 简化 CTB：按图层名 / 图元颜色 rgba 键覆盖输出颜色与线宽(mm)。
- `src/core/Layout.{h,cpp}` —— 视口结构 `PlotViewport`（modelRect + scale + rotation）与 JSON 序列化。

UI（ui，复用 `GraphicsView` 静态绘制）：
- `src/ui/PlotRenderer.{h,cpp}` —— **视图无关**的单图元渲染器。复用 `ElementItem::buildPath`
  （静态、视图无关）生成几何路径，用传入的 `scaleDev`（设备 px / 世界 mm）替代屏幕缩放，
  并应用 CTB 解析结果。线型 dash 仍以「线宽 d」为单位（`dashSpecFor`/`shouldFallbackToSolid`/
  `dashPhaseOffset`），与屏幕渲染完全一致；剖面(`hatchRegionFromElement`/`generateHatchLines`/
  `generateHatchDots`)、标注(`dim::build`)、文本、块引用(`expandBlockRef`) 均复用既有国标算法。
- `src/ui/PrintEngine.{h,cpp}` —— 出图引擎。`render(model, QPaintDevice, settings)` 可作用于
  `QPdfWriter`/`QPrinter`/`QPixmap` 等任意设备；负责页面尺寸、可打印区、居中、裁剪与视口映射。
- `src/ui/PrintSettingsDialog.{h,cpp}` —— 打印设置对话框：页面参数 + CTB 编辑器 + 视口列表。
- `src/ui/PrintPreviewDialog.{h,cpp}` —— 打印预览（QPrintPreviewWidget）+ 导出 PDF 入口。
- `src/ui/icons.h` —— 新增 `print` 图标（矢量绘制，与现有图标库风格一致）。

## 3. 集成点

- **菜单**：`MainWindow::buildMenu` 新增顶级菜单【打印(&P)】，含
  `打印设置...` / `打印预览...` / `导出 PDF...` / `直接打印 (模型空间)`。
- **工具条**：第二行在【测量查询】后新增「打印」按钮（图标 `print`，打开预览）。
- **CMake**：`find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)`；
  `CORE_SOURCES` 增 `PlotStyle.cpp`/`Layout.cpp`，`UI_SOURCES` 增
  `PlotRenderer.cpp`/`PrintEngine.cpp`/`PrintSettingsDialog.cpp`/`PrintPreviewDialog.cpp`；
  `target_link_libraries` 增 `Qt6::PrintSupport`。

## 4. 关键设计决策

1. **渲染与视图解耦**：屏幕交互的 `ElementItem::paint*` 依赖 `viewZoom()`（场景视图），无法直接用于
   打印。改为 `PlotRenderer` 以 `scaleDev`（设备像素/世界毫米）为唯一缩放来源，逻辑与
   `ElementItem::paintOne/penFor` 一一对应，保证「所见即所印」，且不改动既有交互渲染。
2. **世界→设备变换**：`renderViewport` 构造 `QTransform(scaleDev, 0, 0, -scaleDev, dx, dy)`
   （Y 翻转，使世界 y-up 映射到设备 y-down）；文字/标注以 `scale(1/scaleDev, -1/scaleDev)`
   抵消翻转后按像素落字，与屏幕渲染同手法。
3. **CTB 解析优先级**：图层名优先于图元颜色；未覆盖维度沿用图元自身属性（`effectiveLineWidth()`）。
4. **PDF vs 预览**：「导出 PDF」用 `QPdfWriter`（满足显式要求，300dpi）；预览用 `QPrintPreviewWidget`
   + `QPrinter`，其内「打印...」走系统打印机/虚拟 PDF；两条路径共用同一 `PlotRenderer` 绘制逻辑。
5. **⚠️ 线宽单位语义（最易踩坑，见第 6 节）**：屏幕渲染用 `setCosmetic(true)` + 固定像素宽，
   打印渲染用 `setCosmetic(false)` + **逻辑 mm** 宽。两者单位语义不同，不能互相照抄换算公式——
   这正是打印失真修复的核心认知。

## 5. MVP 范围与已知边界

- 视口 MVP：每个视口将其 `modelRect` 独立映射到**整块可打印区**（等价于「每布局一个浮动视口」，
  可叠加对比）。更复杂的视口平铺/相邻排布、旋转、图纸空间嵌套留待后续迭代。
- 打印样式为 CTB（颜色相关）简化子集，未实现 STB（命名样式）与线型映射；按图层/颜色覆盖
  颜色与线宽已可用。
- 无测试程序（项目已于 2026-09-28 全工程清理，测试约定被显式覆盖）；正确性以 `cmake --build`
  全量通过为编译权威判定，GUI 行为需真机人工核对（预览/PDF/物理打印）。

## 6. 打印失真修复：深度分析

### 6.1 现象
- **屏幕显示（图1）**：细线清晰、虚线正常、尺寸/剖面/文字可读。
- **打印预览与实际打印（图2）**：线条粗大成墨团、细节糊死；虚线被拉长连成实带；布局比例与屏幕不一致。

### 6.2 第一轮修复（必要但不充分，含一次误判）
第一轮观察到「线太粗」，判断根因为：图元线宽值（单位实为**图纸毫米**，GB/T 14665 线宽组：G3 粗 1.0mm / 细 0.5mm；工具条标签 "1/2/3/5" 实为 mm）在打印时被直接乘以设备分辨率
（`scaleDev = 模型mm→图纸mm比例 × 设备px/mm`，PDF 300dpi ≈ 11.8px/mm），且无上限钳制。

据此引入：
- `PrintSettings::lineWidthScale`（默认 0.5，0.1~2.0 可调）+ `toJson/fromJson` 序列化；
  `PrintSettingsDialog` 新增「打印线宽比例」`QDoubleSpinBox`。
- `makePen`：`effW = clamp(rs.widthMm × lineWidthScale, 0.05mm, 0.8mm)`。

**该修复方向（线宽比例 + 钳制）保留并仍生效**，但当时对「被乘了谁、乘了几次」判断不完整——
只把 0.5/1.0mm 再缩一档，仍保留了 `×scaleDev` 的换算，因此**失真仍未消除**。

### 6.3 真正根因：打印画笔线宽「双重缩放」（scaleDev 平方级放大）
- 屏幕渲染 `ElementItem::penFor`（GraphicsView.cpp:545）：`setCosmetic(true)` + 固定像素宽 ——
  cosmetic 笔宽不受 painter 变换影响，永远 1~3px 细线，所以屏幕正确。
- 打印渲染 `PlotRenderer::makePen`：`setCosmetic(false)` 且 `setWidthF(effW × scaleDev)`。
- **Qt 语义**：非 cosmetic 笔宽的单位是**逻辑坐标**（世界 mm），QPainter 的世界→设备变换
  （`QTransform(scaleDev,...)`）会自动**再乘一次 scaleDev**。
- 因此实际纸面线宽 = `effW × scaleDev²`。A4 适配 + 300dpi 时 scaleDev≈7~12：
  - 0.5mm 细线 → 纸面约 2~4mm；
  - 1.0mm 粗线 → 纸面约 4~9mm；
  - 虚线 dash 长度按笔宽为单位同步放大 → 整条虚线连成粗实带（图2 中灰色中心线即此）。

> 关键认知：**想让纸面线宽 = W mm，只需 `setWidthF(W)`（逻辑 mm），由世界变换精确缩放一次即可，
> 绝不能再自己乘 scaleDev**——那会平方放大。屏幕（cosmetic 像素）与打印（非 cosmetic 逻辑 mm）
> 是两套正确的独立语义，不能照抄换算。

### 6.4 同族问题（同一双重缩放错误）
- 剖面外轮廓 `outlinePen.setWidthF(max(0.15, effW) × scaleDev)`；
- 剖面线 `hatchPen`（0.25 × scaleDev）→ 剖面线糊死；
- 剖面点 `drawEllipse` 半径（0.9 × scaleDev）→ 巨大墨点；
- 尺寸线 `thin`（0.25 × scaleDev）→ 尺寸要素变粗。

### 6.5 次要因素：fit-to-page 取景不含文字块
- `PrintEngine::modelBounds` 依赖 `ElementItem::geometryBounds`，对 Text 不保证含文字尺寸；
- 「技术要求」等文本块若被排除，自适应比例偏大、布局与屏幕不一致（文本还可能被挤出页面）。
- 图2 实为 A4 纵向页面顶部截图（左缘可见虚线页框），宽度受限适配本身正常，但文本取景仍需加固。

### 6.6 修复方案（已落盘源码，已构建并通过验证）
**PlotRenderer.cpp —— 线宽改用「逻辑 mm、单一缩放」：**
- `makePen`：`pen.setWidthF(effW)`，其中 `effW = clamp(rs.widthMm × lineWidthScale, 0.05, 0.8)`（mm）。
  非 cosmetic 笔宽交由世界变换精确缩放一次，纸面线宽 = effW mm，与分辨率/缩放无关。
- 剖面外轮廓：`max(0.15, effW)`（逻辑 mm）；剖面线：恒 0.25mm；剖面点半径：0.3mm；尺寸细线：0.25mm。
- dash 图案单位=线宽 d，修复后与几何保持同一比例（虚线恢复短虚线观感）。
- `lineWidthScale`（PrintSettings，默认 0.5，0.1~2.0 可调）经 `renderModel/renderElement` 参数
  与 `Ctx` 透传进 `makePen`。

**PrintEngine.cpp —— 取景与渲染质量：**
- `modelBounds` 重写：Xline 不参与取景；Text 用 `textBoxOf()`（插入点+字高+逐行估宽，CJK≈1em/ASCII≈0.55em）
  显式展开；BlockRef 展开子图元取并集 —— 与渲染范围同源，布局与屏幕一致。
- `render()` 增加 `TextAntialiasing`；文字抗锯齿与屏幕一致。
- 补 `#include <QStringList>` / `#include <algorithm>` 头。

### 6.7 验证
- 源码修改 grep 实测落盘：PlotRenderer.cpp（makePen/剖面/尺寸/点、`#include <algorithm>`）、
  PrintEngine.cpp（modelBounds/textBoxOf/TextAntialiasing/`#include <QStringList>`）。
- 重建：`cmake -S . -B build -G Ninja` 配置 CFG=0；
  `cmake --build .../build --target SketchCAD -j 2` 重新编译 PlotRenderer/PrintEngine 并链接，**BUILD=0**；
  已 `cp -f build/SketchCAD.exe dist/`（dist/SketchCAD.exe 10.85 MB）。
- 真机核对项：①线宽与屏幕观感一致；②虚线恢复短虚线；③剖面线/剖面点粗细正常；
  ④「技术要求」文本块参与取景；⑤预览 = PDF = 物理打印一致。

### 6.8 经验教训
- QPainter 非 cosmetic 笔宽单位是逻辑坐标、会被变换缩放；「想要纸面 W mm 线宽」应 `setWidthF(W)`
  交给变换，而不是自己再乘 scaleDev（那会平方放大）。
- 屏幕（cosmetic 像素恒定）与打印（非 cosmetic 纸面毫米）是两套正确语义，不能互相照抄换算公式。
- 排查「打印比屏幕粗」时，优先检查笔的 `cosmetic` 开关与宽度单位，而非急于加比例系数——
  比例系数（lineWidthScale）可作为用户微调手段保留，但不能替代单位语义的正确性。
- 工具通道偶发损坏时（命令被改写、回执路径错乱），应立即停止写操作并核实磁盘，避免把错误内容写入错误路径。

## 7. 预览与实际打印一致性

- 预览（`QPrintPreviewWidget`, 150dpi）、PDF（`QPdfWriter`, 300dpi）、物理打印共用 `PrintEngine::render`；
- 线宽修复后为「纸面毫米」语义，与设备分辨率解耦 → 三条路径输出一致。
- 注意：即便线宽一致，预览部件内部分辨率（150dpi）与导出 PDF（300dpi）下，极细线（<0.15mm）
  的抗锯齿浓淡可能略有差异，属正常栅格化现象，不影响几何与布局。
