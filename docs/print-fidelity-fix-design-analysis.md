# 打印输出失真（与屏幕差异过大）深度分析与修复

## 1. 现象
- 屏幕显示（图1）：细线清晰、虚线正常、尺寸/剖面/文字可读。
- 打印预览与实际打印（图2）：线条粗大成墨团、细节糊死；虚线被拉长连成实带；布局比例与屏幕不一致。

## 2. 根因（深度分析）

### 2.1 主根因：打印画笔线宽被"双重缩放"（scaleDev 平方级放大）
- 屏幕渲染 `ElementItem::penFor`（GraphicsView.cpp:545）：`setCosmetic(true)` + 固定像素宽 —— cosmetic 笔宽不受 painter 变换影响，永远 1~3px 细线，所以屏幕正确。
- 打印渲染 `PlotRenderer::makePen`：`setCosmetic(false)` 且 `setWidthF(effW × scaleDev)`。
- **Qt 语义**：非 cosmetic 笔宽的单位是**逻辑坐标**，QPainter 的世界→设备变换（`QTransform(scaleDev,...)`）会自动再乘一次 scaleDev。
- 因此实际纸面线宽 = `effW × scaleDev²`。A4 适配 + 300dpi 时 scaleDev≈7~12：
  - 0.5mm 细线 → 纸面约 2~4mm；
  - 1.0mm 粗线 → 纸面约 4~9mm；
  - 虚线 dash 长度按笔宽为单位同步放大 → 整条虚线连成粗实带（图2 中灰色中心线即此）。

### 2.2 同族问题（同一双重缩放错误）
- 剖面外轮廓 `outlinePen.setWidthF(max(0.15,effW) × scaleDev)`；
- 剖面线 `hatchPen`（0.25 × scaleDev）→ 剖面线糊死；
- 剖面点 `drawEllipse` 半径（0.9 × scaleDev）→ 巨大墨点；
- 尺寸线 `thin`（0.25 × scaleDev）→ 尺寸要素变粗。

### 2.3 次要因素：fit-to-page 取景不含文字块
- `PrintEngine::modelBounds` 依赖 `ElementItem::geometryBounds`，对 Text 的包围盒不保证含文字尺寸；
- "技术要求"等文本块若被排除，自适应比例偏大、布局与屏幕不一致（文本还可能被挤出页面）。
- 图2 实为 A4 纵向页面顶部截图（左缘可见虚线页框），宽度受限适配本身正常，但文本取景仍需加固。

## 3. 修复方案（已落盘源码，待重新构建）

### 3.1 PlotRenderer.cpp —— 线宽改用"逻辑 mm、单一缩放"
- `makePen`：`pen.setWidthF(effW)`，其中 `effW = clamp(rs.widthMm × lineWidthScale, 0.05, 0.8)`（mm）。
  非 cosmetic 笔宽由世界变换精确缩放一次，纸面线宽 = effW mm，与分辨率/缩放无关。
- 剖面外轮廓：`max(0.15, effW)`（逻辑 mm）；剖面线：恒 0.25mm；剖面点半径：0.3mm；尺寸细线：0.25mm。
- dash 图案单位=线宽 d，修复后与几何保持同一比例（虚线恢复短虚线观感）。
- `lineWidthScale`（PrintSettings，默认 0.5，0.1~2.0 可调，对话框「打印线宽比例」）语义不变：
  0.5 = 细一档（0.25/0.5mm 纸面），1.0 = 按国标图纸毫米原值。

### 3.2 PrintEngine.cpp —— 取景与渲染质量
- `modelBounds` 重写：Xline 不参与取景；Text 用 `textBoxOf()`（插入点+字高+逐行估宽，CJK≈1em/ASCII≈0.55em）显式展开；BlockRef 展开子图元取并集 —— 与渲染范围同源，布局与屏幕一致。
- `render()` 增加 `TextAntialiasing`；文字抗锯齿与屏幕一致。

### 3.3 预览与实际打印一致性
- 预览（QPrintPreviewWidget,150dpi）、PDF（QPdfWriter,300dpi）、物理打印共用 `PrintEngine::render`；
- 线宽修复后为"纸面毫米"语义，与设备分辨率解耦 → 三条路径输出一致。

## 4. 验证与遗留
- 源码修改已核实落盘（grep 实测）：PlotRenderer.cpp（makePen/剖面/尺寸/点、`#include <algorithm>`）、PrintEngine.cpp（modelBounds/textBoxOf/TextAntialiasing/`#include <QStringList>`）。
- 重建已完成：`cmake -S . -B build -G Ninja` 配置 CFG=0；`cmake --build .../build --target SketchCAD -j 2` 重新编译 PlotRenderer.cpp / PrintEngine.cpp 并链接，**BUILD=0**；已 `cp -f build/SketchCAD.exe dist/`（dist/SketchCAD.exe 10.85 MB，时间戳 2026-09-28 16:51）。
- 真机核对项：①线宽与屏幕观感一致；②虚线恢复短虚线；③剖面线/剖面点粗细正常；④"技术要求"文本块参与取景；⑤预览=PDF=物理打印。

## 5. 经验教训
- QPainter 非 cosmetic 笔宽单位是逻辑坐标、会被变换缩放；"想要纸面 W mm 线宽"应 `setWidthF(W)` 后交给变换，而不是自己乘 scaleDev（那会平方放大）。
- 屏幕用 cosmetic（像素恒定）与打印用非 cosmetic（纸面毫米）是两套正确语义，不能互相照抄换算公式。
