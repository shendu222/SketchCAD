# 布局出图体系补齐（视口冻结 / PUBLISH 发布 / 打印戳记 / 视口旋转）设计分析

> 需求：把布局体系补齐到"能替代 AutoCAD 出图"的完整度——per-viewport 图层冻结、
> 批量发布（PUBLISH）、可选打印戳记、消费 `PlotViewport::rotationDeg` 实现视口内容旋转出图。
> 状态：已实现。自测 t_plotPublish 46 条，全量 1052/0，双目标零警告，BUILD_EXIT=0。

---

## 0. 改动边界（硬约束）

- `PlotViewport` **只追加字段**（`frozenLayers`），既有 4 字段不得调整；
- `PrintSettings` **只追加成员**（`PlotStamp stamp`）；
- 既有 `exportPdf` 单页路径**逐字节不变**；
- 旧档兼容：无 `frozenLayers` / 无 `stamp` 键的存档读回即默认值，渲染结果与改造前一致。

---

## 1. 现状勘察结论

1. **视口冻结能力缺失**：`PlotViewport`（Layout.h）仅有 name/paperRect/viewportScale/rotationDeg
   四字段；`PlotRenderer::paintOne` 的图层裁决只有全局 `frozen`/`plot` 开关，没有
   "本视口冻结某层、其他视口与模型空间照常输出"的 per-viewport 维度。
2. **`rotationDeg` 是死字段**：字段存在、序列化存在，但 `renderViewport` 从未消费——
   视口旋转出图是 AutoCAD 布局的基本能力，属"字段已备、引擎未接"。
3. **多布局发布无统一入口**：`exportPdfLayouts` 只能"全部布局 → 单个多页 PDF"，
   无法勾选布局子集、无法每布局一档、无法走物理打印机，也没有页面设置覆盖。
   COM 侧 `BatchPlotEngine::exportDrawing` 空 settings 时行为与 GUI 不同源。
4. **打印戳记（stamp）完全缺失**：图名/日期/比例/文件名等出图信息无处叠加。

## 2. 核心设计决策

### 2.1 只追加字段 + 缺键回落（旧档兼容的统一手法）

- `PlotViewport::frozenLayers`（QStringList）：`toJson` 非空才写 `QJsonArray`；
  `fromJson` 先 `clear()` 再读数组（过滤空串）。缺键 → 空集 = 旧档行为不变。
  附带 `layerFrozenIn(layer)` 便捷判定。
- `PrintSettings::stamp`（PlotStamp）：`toJson` 仅 `enabled` 时写 `"stamp"` 键；
  `PlotStamp::fromJson` 开头 `if (o.isEmpty()) return;`（缺键保持默认关闭），
  其余键逐项回落。**默认关闭 ⇒ 旧行为逐字节等价**（自测 PUB⑤ 覆盖）。
- PlotStamp 放 core 层（Layout.h），无 ui 依赖——命令行 / COM / GUI 三路可共享。

### 2.2 渲染裁决带默认参数扩展（不破坏既有调用点）

`PlotRenderer::renderModel` 签名追加 `const QStringList* frozenLayers = nullptr`：
- nullptr = 历史行为，所有既有调用点零改动；
- `Ctx` 加同名指针，`paintOne` 顶部裁决
  `c.frozenLayers && contains(e.layer) → return`；
- **块引用展开的子图元逐个走 paintOne** ⇒ 冻结语义对块内同层子图元天然生效，
  无需在 INSERT 展开处特判；
- 裁决位置放在 Constraint 过滤之后、图层 frozen/plot 全局裁决之前——
  per-viewport 冻结优先于全局开关，语义与 AutoCAD 一致（视口冻结是"本视口不显示"，
  与图层的打印开关是两个独立维度）。

### 2.3 视口旋转：矩阵直接构造，规避 QTransform 组合顺序歧义

绕世界锚点旋转 θ、再按 scaleDev/原点映射到纸面的**合成矩阵按系数直接写出**：

```
cs = cos(θ), sn = sin(θ), sS = scaleDev
m11 =  sS·cs      m12 = −sS·sn
m21 = −sS·sn      m22 = −sS·cs
dx  = originX − sS·(cs·ax − sn·ay)
dy  = originY + sS·(sn·ax + cs·ay)
```

- **θ=0 时逐系数退化为基础矩阵**（m11=m22=sS，m12=m21=0，dx/dy=origin±0）
  ⇒ 未旋转路径零回归；
- 弃用 `QTransform` 方法链（translate/rotate/translate）+ `operator*` 组合——
  Qt 方法是**前插语义**，链式组合的落点与直觉相反（实测 90° 锚点落错，见 §5）。

**旋转裁剪必须经逆变换回世界系**：`setTransform(t)` 生效后再 `setClipRect(设备矩形)`，
该矩形会被 painter 变换**再次映射**而裁错（实测吞掉全部墨迹）。正确做法：

```
QPolygonF devPoly = 四角设备矩形;
QTransform inv = t.inverted();
QPainterPath clipPath; clipPath.addPolygon(inv.map(devPoly)); clipPath.closeSubpath();
p.setClipPath(clipPath);   // painter 变换后恰好还原为设备目标矩形
```

仅旋转时启用裁剪 ⇒ 未旋转视口路径逐字节不变。

### 2.4 发布页列表抽象：PublishPage 统一三种目标

```
struct PublishPage { QString name; PrintSettings settings; };  // PrintEngine 内嵌套
```

- `exportPagesPdf(model, pages, filePath, title)`：QPdfWriter 每页独立
  setPageSize/Orientation/Margins，首页 begin + 后续 newPage，
  逐页 `renderPageBody(model, p, pg.settings, pg.name)`；
- `printPages(model, pages, printer)`：QPrinter 同构（每页设置 + newPage）；
- `exportPdfLayouts` 多布局路径**委托** exportPagesPdf（title="SketchCAD 打印输出（全部布局）"），
  空表回落仍走 exportPdf 原路径——**既有行为零改动**；
- PublishDialog 的三种目标（单多页 PDF / 每布局一档 / 物理打印机）全部消费
  同一个 `buildPages()` 页列表 ⇒ 目标之间只有输出设备差异，页面内容必然一致。

### 2.5 COM/GUI 同源（验收标准 ③ 的结构性保证）

`BatchPlotEngine::exportDrawing` PDF 分支开头：

```
if (settings.isEmpty() && model.layoutCount() > 0)
    → PrintEngine::exportPdfLayouts(...)   // 与 GUI"导出全部布局/发布"同一裁决
```

- 显式 settings 维持旧单页行为（不破坏既有 COM 调用方）；
- 同源性不是"两边各自实现对齐"，而是**同一函数**——页数一致由构造保证。

### 2.6 打印戳记：无头确定性占位块（头号陷阱）

自测进程只有 `QCoreApplication`，任何 `drawText` / QFontDatabase 访问即
qFatal（"Must construct a QGuiApplication"，EXIT=127 且无汇总输出）。
与 TextMetrics"无头确定性估算"同款对策：

- **判定**：`qobject_cast<const QGuiApplication*>(QCoreApplication::instance()) == nullptr`
  ⚠️ 不能用 `QGuiApplication::instance() == nullptr`——该别名对 QCoreApplication
  也返回非空指针（instance() 只是 instance() 的转型捷径，不做动态类型校验）；
- **无头路径**：`fillRect` 画确定性占位块（宽 20mm × 行数×行高）——
  位置 / 字高 / 开关语义全部可测（PUB⑥ 五条断言全走占位块）；
- **GUI 路径**：真文字，右对齐行用 `QRectF(anchorX−1e6, y, 1e6, lineH)` 大矩形右锚；
- 组行：图名（completeBaseName）/ 文件名全路径 / 布局名 / 比例
  （fitToPage→"自适应"，≥1→"%1:1"，否则"1:%1"）/ 日期 yyyy-MM-dd / 时间 HH:mm:ss；
- 锚点 = 可打印区角内缩 3mm，position 0=左上 1=右上 2=左下 3=右下（默认右下）。

### 2.7 空图纸不吞戳记

renderPageBody 模型空间分支空 `mb` 原先直接 `return`——戳记永远画不到。
重构为**守卫渲染块**（有内容才渲染）+ **戳记总在函数尾执行**。
戳记是页面级叠加，与图纸内容是否为空无关。

### 2.8 命令行与 GUI 双入口

- `layout vpfreeze <视口名>`：列出冻结层；`layout vpfreeze <视口名> <图层名>`：幂等冻结；
  `layout vpthaw <视口名> <图层名>`：解冻。视口名大小写不敏感匹配，
  改动经 `PrintSettings s = def->settings` 拷贝后 `updateLayout` 回写（走既有撤销链）；
- 视口属性面板（PrintSettingsDialog::onEditViewport）扩展：比例 → 旋转角
  （QInputDialog，−360..360）→ 冻结图层 QDialog（QScrollArea + 每层 QCheckBox）；
  视口列表条目尾追加 `旋转N°` / `冻结N层` 描述；
- 菜单"发布... (PUBLISH)"插入打印菜单（导出 PDF 全部布局与直接打印之间）。

## 3. 改动清单

| 文件 | 改动 |
|---|---|
| `src/core/Layout.h` | PlotViewport 追加 frozenLayers + layerFrozenIn；新增 PlotStamp 结构（toJson/fromJson，默认关闭）；PrintSettings 追加 stamp；include QStringList |
| `src/core/Layout.cpp` | PlotViewport 冻结集序列化（非空才写/缺键清空回落）；PrintSettings stamp 键（enabled 才写/fromJson 缺对象保持默认）；PlotStamp 实现 |
| `src/ui/PlotRenderer.h/.cpp` | renderModel 追加 frozenLayers 默认参数；Ctx 同名指针；paintOne 顶部 per-viewport 冻结裁决 |
| `src/ui/PrintEngine.h` | 新增 PublishPage；声明 exportPagesPdf / printPages |
| `src/ui/PrintEngine.cpp` | renderViewport 追加 frozenLayers/rotationDeg 参数；旋转矩阵直接构造 + 逆变换裁剪；drawPlotStamp（无头占位块/GUI 真文字）；renderPageBody 空图纸守卫 + 尾部戳记；exportPdfLayouts 委托 exportPagesPdf；exportPagesPdf/printPages 实现 |
| `src/com/BatchPlotEngine.cpp` | exportDrawing PDF 分支：空 settings && 有布局 → exportPdfLayouts（COM/GUI 同源） |
| `src/core/Commands.cpp` | layout vpfreeze/vpthaw 子命令（幂等、大小写不敏感、拒负例）；未知子命令提示更新 |
| `src/ui/PrintSettingsDialog.cpp` | onEditViewport 增旋转角/冻结图层编辑；syncViewportList 描述扩展 |
| `src/ui/PublishDialog.h/.cpp` | **新建**：布局勾选（模型空间恒含）+ 三目标 + 页面设置覆盖 + 文件名消毒（QRegularExpression，Qt6 无 QRegExp）+ 全程 model_->log 汇报 |
| `src/ui/MainWindow.h/.cpp` | openPublish 声明/实现；打印菜单插入"发布... (PUBLISH)" |
| `CMakeLists.txt` | UI_SOURCES 加 PublishDialog.cpp（自测目标不含） |
| `tests/SketchSelfTest.cpp` | 新增 t_plotPublish：8 组 46 条 CHECK |

## 4. 自测设计（t_plotPublish，8 组 46 条）

辅助 lambda：`addLineL`（指定图层画线）、`renderA4`（A4 300dpi 白底 QImage）、
`inkIn`（mm 区域 qGray<100 判墨）、`baseSettings`（A4 纵向 10mm 边距 1:1 黑白）。

| 组 | 内容 | 条数 |
|---|---|---|
| ① frozenLayers 序列化 | 往返相等 / layerFrozenIn / 缺键→空集 | 3 |
| ② 双视口冻结 | 视口1 冻结层零输出、视口2 同层正常、模型空间不受影响（device_y=148.5+(anchorY−wy)：y=0→150.5mm、y=4→146.5mm） | 5 |
| ③ rotationDeg=90° | 0° 水平线中心行有墨/上半区无墨；90° 后变竖直过纸面中列 105mm、跨纵向、原水平线消失 | 5 |
| ④ exportPagesPdf | 三页列表→3、%PDF 魔数非空、空列表→0 | 4 |
| ⑤ 戳记默认关闭 | 关闭不写 stamp 键、缺键读回关闭、开关两态渲染 QImage 全等（旧档兼容铁证） | 3 |
| ⑥ 戳记开启 | 右下墨迹出现、整幅改变、位置=左上搬家、字高 5mm≠2.5mm | 5 |
| ⑦ 命令 CRUD | vpfreeze/vpthaw 增查删 + 幂等 + 拒负 | 7 |
| ⑧ GUI-COM 一致性 | 源图纸 JSON 落盘 → COM runBatch 空 settings → exportPdfLayouts → PDF 页对象计数（`/Type /Page` − `/Type /Pages`）== GUI 页数 2、两路 PDF 均非空 | 5 |

## 5. 踩坑记录（四条关键陷阱）

1. **`QGuiApplication::instance()` 陷阱**：它对 QCoreApplication 进程也返回非空
   （只是 instance() 的静态转型捷径，不做动态类型检查）——用它判"无头"永远判不中，
   drawText 直接 qFatal EXIT=127。必须 `qobject_cast<const QGuiApplication*>(QCoreApplication::instance())`。
2. **QTransform 组合顺序歧义**：方法链（translate/rotate/translate）为前插语义，
   与 `operator*` 的书写顺序直觉相反，90° 旋转锚点落错。解法=按推导直接写合成矩阵系数，
   θ=0 逐系数退化保证零回归。
3. **setClipRect 双重变换**：`setTransform` 生效后 clip 矩形会被再映射一次，
   裁剪区错误（实测吞掉全部墨迹）。解法=逆变换把设备多边形转回世界系后 setClipPath。
4. **空图纸提前 return 吞戳记**：renderPageBody 模型空间分支空 mb 直接 return，
   戳记（页面级叠加）永不执行。解法=守卫渲染块 + 戳记总在尾部。

（次要：变量笔误 sDev→scaleDev 编译失败；y 映射方向 device_y=148.5+(anchorY−wy)
即世界 y 越大纸面越靠上，断言初版方向写反；Qt6 无 QRegExp 用 QRegularExpression。）

## 6. 验收对照（实测）

| 验收标准 | 结果 |
|---|---|
| ① t_plotPublish ≥ 10 条（冻结生效不影响他处 / 发布多布局页数正确 / 戳记默认关闭逐字节等价 / rotationDeg=90 几何正确） | ✅ 46 条（≥10），四项各设专组 |
| ② 旧档兼容（无 frozenLayers 读回空冻结集、渲染与改造前一致） | ✅ PUB①③ 缺键回落断言 + PUB⑤ 两态 QImage 全等 |
| ③ COM 与 GUI 同一图纸集页数一致、PDF 均非空 | ✅ PUB⑧ 页对象计数相等（2==2）、两路 %PDF 非空 |
| ④ 双目标零警告、build.sh test EXIT=0 | ✅ BUILD_EXIT=0、警告 0、通过 1052 / 失败 0（1006→1052）、SELFTEST_EXIT=0 |
| 改动边界 | ✅ PlotViewport 只追加 frozenLayers；exportPdf 单页路径未动；exportPdfLayouts 空表回落走原 exportPdf |

## 7. 已知边界

1. **无头占位块**：戳记在无头自测中渲染为确定性占位块（宽 20mm×行数×字高），
   位置/字高/开关语义可测；真文字效果仅 GUI 路径可见（与 TextMetrics 无头估算同款取舍）。
2. **printPages 页面参数时序**：QPrinter 的页面设置需在 newPage 之后、绘制之前逐页设置，
   部分物理驱动对运行中改纸张支持不一；物理打印未在无头自测覆盖（需真实打印设备）。
3. **COM 单页路径**：BatchPlotEngine 显式传 settings 时仍走旧单页 exportPdf
   （既有调用方契约不变）；发布语义仅空 settings + 有布局时启用。
4. **戳记不参与布局预览**：PrintSettingsDialog 的页面预览不含戳记叠加，
   最终效果以发布/打印输出为准。
