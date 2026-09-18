# 视图平移"右键拖不动"——根因分析与修复

> **症状**：鼠标右键拖动应实现视图平移，但运行时屏幕无任何反应。
> 运行时日志 `%TEMP%/sketchcad_pan.log` 显示平移数据在持续变化；既有测试 `ViewPanTest`、`ViewRenderTest` 通过；但画面纹丝不动。
> **最终修复**：3 行配置（`setSceneRect` 大矩形 + `setTransformationAnchor(NoAnchor)` + 关闭滚动条），详见末尾。

---

## 1. 排查方法论

整个排查围绕一个核心区分：**"矩阵正确 ≠ 画面跟随"**。
Qt 的 `QGraphicsView` 在你调 `setTransform` 写入矩阵后，**渲染层的水平/垂直定位并不是矩阵平移分量**，而是由"滚动条值 - indent"决定。这正是日志/矩阵测试全绿、但屏幕不动的根因所在。

排查分四步：
1. **先排除表象**——四个原始假设逐一证伪。
2. **直接读 Qt 源码**——从 code.qt.io 拉取 v6.10.1 的 `qgraphicsview.cpp` 确认机制。
3. **写像素级渲染测试**——这是唯一能区分"矩阵变了"与"画面动了"的方法。
4. **修源码 + 跑全套测试**——确保零回归。

---

## 2. 四个原始假设的排除与确认

### 假设 ① viewport 设置了 Interactive 等导致 transform 被忽略 → **排除**

`QGraphicsView::setInteractive(bool)` 仅控制 item 是否响应鼠标事件（拾取、拖动），
不影响变换的渲染。代码里也未调用任何 `setInteractive(false)` / `setDragMode(...)`，
且 `ViewPanTest` 直接读 `view.transform()` 已确认矩阵在写。
**结论：与症状无关。**

### 假设 ② sceneRect 被动态更新导致视图重置 → **部分成立（次因）**

`grep setSceneRect src/` 仅一处——`GraphicsView` 构造函数第 322 行：

```cpp
scene_->setSceneRect(QRectF());   // 空矩形
```

`QGraphicsScene::sceneRect()` 对"被设置过的空矩形"的处理是回退到 `itemsBoundingRect()`，
意味着**有效场景矩形 = 所有图元的包围盒**。对一份小型草图（约 60×20 世界单位），
在 zoom=4 下映射为约 252×80 像素——远小于 800×600 视口。
这是参与抵消的必要条件，但不是充分条件。

### 假设 ③ translate 与 setTransform 的调用顺序及矩阵组合 → **排除**

`applyTransform()` 写成：

```cpp
setTransform(QTransform::fromTranslate(panX_, panY_).scale(zoom_, -zoom_));
```

直觉上 `fromTranslate().scale()` 像是"先平移后缩放"（平移会被缩放），
但**Qt 链式调用采用左乘**（`M.scale(s) ≡ s·M`），所以此式对点的实际映射是
`p' = p·S·T = (x·zoom + panX, −y·zoom + panY)`——**平移分量恰为屏幕像素偏移**，与
`fitView/zoomBy/wheel` 中 `panX_ = c.x() − anchor.x()·zoom` 的算式完全一致。

直接实证（`ViewRenderTest` Part A）：

```
setTransform(fromTranslate(120,80).scale(4,-4))
view.transform().m31() = 120.0   ← 平移未缩放
```

**结论：矩阵组合正确，勿改顺序。**（在源码中已加注释防误改。）

### 假设 ④ 需要在 setTransform 后手动 viewport()->update() → **排除**

平移分支已有：

```cpp
applyTransform();
viewport()->update();
```

且 `QGraphicsView::setTransform` 内部本来就会调 `updateAll()` 触发全量重绘。
**与症状无关。**

---

## 3. 真根因：Qt 渲染层定位公式不认矩阵平移

四个假设全排除后，唯一的怀疑对象是 Qt 内部。从 code.qt.io 取
`qtbase/src/widgets/graphicsview/qgraphicsview.cpp @ v6.10.1`，关键函数：

### 3.1 渲染映射公式（`mapToScene` 第 3887–3893 行）

```cpp
QPointF QGraphicsViewPrivate::mapToScene(const QPointF &point) const
{
    QPointF p = point;
    p.rx() += horizontalScroll();   // ← 滚动条值
    p.ry() += verticalScroll();
    return identityMatrix ? p : matrix.inverted().map(p);
}
```

反推场景→视口：**`viewport = matrix.map(scene) − (scrollX, scrollY)`**。
水平/垂直定位完全由 **scroll** 决定，与矩阵平移分量无关。

### 3.2 `horizontalScroll()` 的来源（第 519–585 行）

```cpp
qint64 QGraphicsViewPrivate::horizontalScroll() const
{
    if (dirtyScroll) const_cast<QGraphicsViewPrivate *>(this)->updateScroll();
    return scrollX;
}
void QGraphicsViewPrivate::updateScroll()
{
    scrollX = qint64(-leftIndent);
    ...
    scrollX += hbar->value();      // ← indent + 滚动条值
    scrollY = qint64(vbar->value() - topIndent);
    dirtyScroll = false;
}
```

### 3.3 `scrollX/scrollY` 在两种情况下抵消矩阵平移

`recalculateContentSize()`（第 345–470 行）在每次变换变化后被 `setTransform` 调用，
根据场景矩形映射后是否"装得进"视口走两个分支：

| 分支 | 触发条件 | `leftIndent` 计算（AlignLeft） | 效果 |
|---|---|---|---|
| **A · 小场景** | `viewRect.width() ≤ viewport.width()` | `leftIndent = −viewRect.left()` <br> = `−(rect.x·zoom + panX·zoom)` | **`scrollX = −leftIndent = zoom·(rect.x + panX)`**，渲染时抵消 `panX·zoom` |
| **B · 大场景** | `viewRect.width() > viewport.width()` | `leftIndent = 0`，给滚动条设真实量程 | `setTransform` 随后调 `centerView(AnchorViewCenter)` → `centerOn(lastCenterPoint)` → 调整滚动条值把中心拉回，**矩阵平移被滚动条偏移抵消** |

**当前代码踩的就是分支 A**：空 `sceneRect` 回退成小的 `itemsBoundingRect`，
`leftIndent = −(rect.x·zoom + panX·zoom)` 把 `panX` 整层吃掉；用户的"修复"（改 AlignCenter 为
AlignLeft|AlignTop）只是把分支内的对齐方向换了——`leftIndent` 仍然存在、仍然抵消平移。
而若改用大 sceneRect 走到分支 B，`centerView` 会通过滚动条继续抵消。

---

## 4. 像素级实证——为什么现有测试通过但屏幕不动

`ViewPanTest` / `ViewRenderTest` 只断言 `view.transform()`（矩阵成员），从不渲染到像素。
**这正是"测试全绿 + 实际坏了"的经典盲区**。

新写的 `tests/view_render_test2.cpp`（CMake target: `ViewRenderTest2`）：
在 offscreen 下渲染两帧 `QImage`，求红色标记物像素质心，对比拖拽前后位移。

### 4.1 未修复代码的运行结果

```
[INFO] sceneRect=(-31.5,-10.0,63.0,20.0)   ← 空矩形 → 回退为 itemsBoundingRect
[F0] 红色标记质心 = (125.9, 40.3)          ← 矩阵说在视口中心 (399, 299)
                                              实际渲染在 (125.9, 40.3)
                                              4×(0 − (-31.5)) = 126.0  逐位吻合分支 A 公式
[F1] 红色标记质心 = (125.9, 40.3)          ← 拖拽 +40,+25 后矩阵动了，画面纹丝不动
[DELTA] 质心位移 = (0.00, 0.00)            ← 期望 (40.00, 25.00)
RESULT: STUCK(BUG 复现)
```

### 4.2 修复后（加上 NoAnchor）的运行结果

```
[INFO] sceneRect=(-100000.0,-100000.0,200000.0,200000.0)
[F0] 红色标记质心 = (717.9, 537.5)
[F1] 红色标记质心 = (757.5, 560.0)
[DELTA] 质心位移 = (39.63, 22.53)          ← 与期望 (40.00, 25.00) 吻合（微小偏差来自坐标轴像素在视口边缘的可见长度变化）
RESULT: PASS
```

---

## 5. 修复

仅在 `GraphicsView` 构造函数加 3 行：

```cpp
scene_->setSceneRect(QRectF(-1e5, -1e5, 2e5, 2e5));   // ① 永远走分支 B（indent=0）
setAlignment(Qt::AlignLeft | Qt::AlignTop);           //   （大矩形下对齐偏移恒为 0）
setTransformationAnchor(QGraphicsView::NoAnchor);     // ② setTransform 不再 centerOn → 滚动条值保持 0
setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // ③ 关闭滚动条 UI（值仍可用，但恒为 0）
```

三步缺一不可：
- 缺 ①：场景矩形小 → 分支 A `leftIndent` 抵消平移；
- 缺 ②：分支 B 下 `centerView(AnchorViewCenter)` 会把中心拉回；
- 缺 ③：滚动条值参与定位偏移（即使 AlwaysOff 也存在），且滚动条 UI 与 CAD 画布交互习惯冲突。

矩阵组合和 `applyTransform()` 不动——已实证正确。

---

## 6. 验证（修复后跑全部测试）

| 测试 | 覆盖范围 | 结果 |
|---|---|---|
| `ViewPanTest` | 矩阵层：右键拖拽 → `view.transform()` 精确变化 | **PASS** (ddx=40,25 / 60,40) |
| `ViewRenderTest` | 矩阵层：`setTransform` 写入精确 | **PASS** (m31=120,80) |
| **`ViewRenderTest2`** | **像素层：两帧真实渲染质心位移** | **PASS** (Δ=(39.63,22.53)≈(40,25)) |
| `SketchSelfTest` | 核心层 174 个回归用例 | **PASS** (174/0) |

**结论：画面跟手平移，与日志/矩阵变化一致，零回归。**

---

## 7. 教训

1. **"矩阵层测试通过" ≠ "渲染正确"**。对任何 GUI 框架里写变换的代码，必须有端到端的
   像素对比测试（`grab()` / `render()` 比对），否则中心化抵消机制会让你绿到生产环境。
2. **QGraphicsView 的平移正经做法**：`applyTransform` 只放缩放/旋转，平移交给滚动条
   `centerOn` / `hbar->setValue`；若必须矩阵内 pan，必须 `setTransformationAnchor(NoAnchor)`
   + 大 sceneRect + 滚动条恒为 0——三者缺一不可。
3. 排查 GUI 渲染问题时，**直接读框架源码**比反复试 API 更高效——本次
   拉 `qgraphicsview.cpp` v6.10.1 用 5 分钟确定了 `centerView + recalculateContentSize`
   的双分支抵消机制，省去了大量猜测试错。

---

## 附录：关键源文件与改动

| 文件 | 改动 |
|---|---|
| `src/ui/GraphicsView.cpp`（构造函数，第 317–329 行） | 加 `setTransformationAnchor(NoAnchor)` 与大 sceneRect、滚动条策略；注释改为对照 Qt 源码的根因说明 |
| `src/ui/GraphicsView.cpp`（`applyTransform`，第 369–378 行） | 仅澄清注释（Qt 链式调用左乘语义），不改代码 |
| `tests/view_render_test2.cpp`（新增） | 像素级根因验证；同样输入与 `ViewPanTest`，从矩阵层跨到渲染层 |
| `CMakeLists.txt` | 新增 `ViewRenderTest2` target，链接与 `ViewRenderTest` 完全一致 |

附导出的画面（拖拽前）：`%TEMP%/pan_f0.png`，拖拽后：`%TEMP%/pan_f1.png`。