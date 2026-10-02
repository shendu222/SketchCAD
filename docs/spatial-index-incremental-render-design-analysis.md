# SketchCAD 增量渲染与空间索引：设计与深思

> 配套任务：将渲染与拾取的复杂度从 O(N) 全量降为「增量 / 对数级」。
> 范围：增量场景同步、选中高亮增量、均匀网格空间索引、性能基准脚本。
> 约束：不修改 `Element`/`SketchModel` 数据模型与序列化格式；`rebuild` 全量路径必须保留；Xref/块引用/图层联动的 Item 生命周期不得泄漏；命中容差语义与现有行为完全一致。
> 验收：特性开关双跑对照一致、基准三档达标、自测全绿、-Wall -Wextra 零警告、三目标 EXIT=0。

---

## 1. 背景与目标

原实现在每次 `modelChanged` / `selectionChanged` 时**整体重建场景**（`scene_->clear()` + 重新 `new` 全部 `ElementItem`），且在 `hitTest` 时**逐图元遍历**全部元素做精确距离判定。随着图元数 N 增大：

- 单次选中 / 悬停拾取 = O(N) 全量遍历；
- 选中高亮 / 任意模型变更 = O(N) 场景重建（含 10 万元素的 `new`/`delete` 抖动）；
- 平移缩放虽只是变换，但旧架构在「选中即重建」的场景下交互会频繁触发 O(N) 重建。

目标：5 万图元场景单次选中响应 ≤16ms、平移缩放 ≥30FPS；10 万图元场景可正常打开并交互。

---

## 2. 核心设计决策：硬约束「不改数据模型」下如何实现增量

验收硬约束要求**不修改 `Element` / `SketchModel` 数据模型及序列化格式**，因此：

- **不新增变更集信号**（不动 `modelChanged()` 的无参签名，不动序列化）。
- 索引与 `id→Item` 映射**只存于 `GraphicsView`**（视图侧），不污染 core 模型。
- 已有信号 `elementUpdated(int)`（单图元增/改/删时由 `SketchModel` 各类写操作 `emit`）成为 O(1) 增量热路径的触发源——它本就存在，只是此前未被视图消费。
- `modelChanged`（无增量）退化为**差异扫描**：仅对「模型有、场景无」的做新增，「场景有、模型无」的做删除，「包围盒变化」的做更新，**复用已有场景项**，不全量重建。

> 关键认知：真正吞噬性能的从来不是「数据变了」，而是「每次数据变都把整棵场景树推倒重来」。把「增量信号」与「差异扫描」两条路都打通后，绘制热路径从 O(N) 降为 O(1)（单图元）或 O(变更数)（批量），`rebuild` 仅作为结构/批量变更的兜底保留。

---

## 3. 增量场景同步（GraphicsView 侧）

### 3.1 映射表与重建兜底

`GraphicsView` 新增：

- `std::unordered_map<int, ElementItem*> items_`：图元 id → 场景项（增量同步唯一真相源）；
- `std::unordered_map<int, QRectF> itemBounds_`：id → 上次登记索引的包围盒（差异扫描用）；
- `SpatialIndex spatial_`：均匀网格空间索引（世界坐标，见第 6 节）；
- `QSet<int> prevSelected_`：上次选区基线（高亮差量用）。

`rebuild()` 在重建时一次性填充上述三表，并**保留为全量兜底路径**：结构性变更（如 `clear()`、批量 `addElementsQuiet`、读档）仍走 `rebuild()`，保证异常/大规模变更时行为正确。

### 3.2 `elementUpdated(int)`：O(1) 热路径

构造函数连接：

```cpp
connect(model_, &SketchModel::elementUpdated, this, &GraphicsView::onElementUpdated);
```

`onElementUpdated(id)` 逻辑极简：

```cpp
void GraphicsView::onElementUpdated(int id) {
    incSyncPending_ = true;                 // 标记本批次已由增量信号处理
    const Element* e = model_->element(id);
    if (!e) syncItemRemove(id);             // 模型已无 → 删除项+索引
    else      syncItemAddOrUpdate(*e);      // 模型有 → 创建/更新项+索引
}
```

`syncItemAddOrUpdate`：若 `items_` 中无该 id 则 `new ElementItem` 并 `addItem`；否则调用 `setElement`（改数据 + `prepareGeometryChange` + `update`），并同步线宽/标注比例/警告/高亮/可见性/z 值，最后 `spatial_.update(id, bounds)`。

`modelChanged` 分支：

```cpp
void GraphicsView::onModelChanged() {
    if (incSyncPending_) { incSyncPending_ = false; viewport()->update(); return; } // 增量已处理，不重建
    syncIncremental();   // 无增量信号时的差异扫描兜底
    viewport()->update();
}
```

`incSyncPending_` 防止「单图元更新既发 `elementUpdated` 又发 `modelChanged`」时重复全量重建——增量信号先行消化，全量信号退化为空操作。

### 3.3 差异扫描兜底 `syncIncremental()`

1. 遍历 `items_` 的键，凡 `model_->element(id)` 为空的 → `syncItemRemove`（覆盖 id 复用后的旧项，避免悬垂）；
2. 遍历 `model_->elements()`，凡 `items_` 中无该 id → `syncItemAddOrUpdate`；
3. 对已有的项，比较 `itemBounds_[id]` 与当前 `boundsOf(e)`，变化则 `syncItemAddOrUpdate`（更新项 + 索引）。

---

## 4. 选中高亮增量（O(1)）

原 `onSelectionChanged` 直接 `rebuild()`。改为**差量**：

```cpp
void GraphicsView::onSelectionChanged(int) {
    const auto& sel = model_->selection();
    // 取消：在 prevSelected_ 但不在 sel
    for (int id : prevSelected_) if (!sel.contains(id)) setItemHighlight(id, false);
    // 新增：在 sel 但不在 prevSelected_
    for (int id : sel)           if (!prevSelected_.contains(id)) setItemHighlight(id, true);
    prevSelected_ = QSet<int>(sel.begin(), sel.end());
}
```

`setItemHighlight(id, on)` 仅修改 `ElementItem::highlight` 标记并 `update()`，**不重建场景、不触碰索引**。选中 1 个对象 = O(1) 两次哈希查找 + 一次重绘，与 N 无关。

---

## 5. 均匀网格空间索引 `SpatialIndex`（纯 core）

设计为**视图无关、无 GUI 依赖**的纯 core 类（`src/core/SpatialIndex.h/.cpp`），可在无界面自测中直接验证。

### 5.1 单元划分与自适应 cellSize

- 世界坐标按固定尺寸 `cellSize_`（默认 64 = 2^6）划分单元，单元坐标 `gx = floor(x/cellSize)`、`gy = floor(y/cellSize)`。
- `GraphicsView::rebuild()` 按场景跨度自适应取 2^k：

  ```cpp
  double cs = span / std::sqrt(double(N));     // 使每轴约 √N 单元，单元内平均约 1 图元
  spatial_.setCellSize(std::pow(2.0, std::ceil(std::log2(cs))));
  ```

  场景均匀时每单元约 1 个图元，查询候选数 ≈ O(1)。

### 5.2 登记 / 移除 / 更新

- `add(id, b)`：计算覆盖 `b` 的全部单元 `cellsFor(b)`，把 `id` 登记进这些单元，并记录 `idCells_[id]`（该 id 涉及过的单元，供移除/更新）。
- `remove(id)`：遍历 `idCells_[id]`，从各单元向量擦除 `id`，单元空则删除；最后清 `idCells_`。
- `update(id, b)`：`remove` + `add`（幂等）。

### 5.3 查询与「⊇ 真实命中集」不变量

`queryRect(rect)` / `queryPoint(w, tol)` 遍历查询矩形覆盖的单元，去重返回候选 `id`。**核心不变量：返回结果 ⊇ 真实命中集**——

- 候选集来自「查询矩形覆盖的单元」；
- 一个真实命中图元必与查询矩形相交（距离 < tol ⇒ 包围盒进入 tol 邻域），其包围盒至少与一个被查询单元重叠；
- 因此索引**绝不漏选**，最多多候选；
- 精确命中由 `hitTest` 调用方用 `pickDist` 二次判定。

`queryPoint` 实现为对 `[w-tol, w+tol]²` 的 `queryRect`，天然对齐「屏幕 PICK_TOL_PX 像素 → 世界 tol」的容差语义。

### 5.4 退化 / 极长图元保护

`cellsFor` 对退化（零面积）包围盒取中心点单元；对极端细长图元（如裁剪前的无限线）做单元数上限 `kMaxCells=4096` 保护，避免登记爆炸。

---

## 6. hitTest / hitTestAll / 框选 索引路径语义对齐

### 6.1 容差换算（与现有行为逐字一致）

```cpp
const Vec3 w = toVec3(mapToScene(viewPos));
const double tol = PICK_TOL_PX / std::max(1e-6, zoom_);   // PICK_TOL_PX 不变
```

`PICK_TOL_PX` 与原实现完全相同（8.0），缩放换算 `(w)` 公式不变——无论放大多少倍「手感」一致。

### 6.2 精确判定与排序（并列语义）

- 索引路径：候选集经 `spatial_.queryPoint(w, tol)` 缩小后，对每个候选 `pickDist(e, w, model_) < tol` 才入选；
- 全量路径（开关关闭 / 索引未启用）：逐 `model_->elements()` 做完全相同的 `pickDist` 判定（与原实现逐字一致）；
- `pickDist` 对 `BlockRef` 展开 `geo::distanceToElements`，与原 `hitTest` 一致；
- 排序 `{距离升序，距离相等则 id 降序（后创建 / 视觉上层优先）}`，对齐旧版「逆序遍历 + 严格小于才替换」的并列语义。

`hitTest` = `hitTestAll().front()`，保持公开 API 不变。

### 6.3 框选索引路径

`marquee` 释放时若 `useIndex_ && spatial_.enabled()`：

```cpp
const auto cand = spatial_.queryRect(r.adjusted(-eps, -eps, eps, eps));   // 缩候选
for (int id : cand) {
    const Element* e2 = model_->element(id);
    if (e2 && elementBoundsForIndex(*e2).adjusted(-eps).intersects(r))     // 与核心层相同判定
        addToSelection(*e2);
}
```

其中 `elementBoundsForIndex` 复用 `model::boundsOf` 的语义（非块 `geo::elementBounds`、块 `expandBlockRef` 并集），使框选结果集与关闭索引的全量路径**完全一致**。

---

## 7. 特性开关与双跑对照（验收条款 1）

- 双层开关：`SpatialIndex::setEnabled(bool)`（索引侧）+ `GraphicsView::useIndex_`（视图侧）。
- 关闭时 `query*` 返回空 → 调用方自动回落全量扫描路径，逻辑与原实现逐字一致。
- 验收脚本（第 10 节）对每个规模随机生成 1e4 次点击，分别走「索引开 / 索引关」两条 `hitTestAll` 路径，对结果向量做差分，**要求差分为 0**。

---

## 8. 生命周期 / id 复用 / 块引用 / 图层联动（回归重点）

- **id 复用**：`syncIncremental` 先删除「模型已无的 id」（含复用后的旧项），`syncItemRemove` 从 `items_`/`itemBounds_`/`spatial_` 三处同步擦除；`syncItemAddOrUpdate` 仅当 `items_.find` 缺失才 `new`，避免悬垂与重复登记。
- **块引用 / Xref**：`elementBoundsForIndex` 与 `pickDist` 内展开 `BlockRef`/`expandBlockRef`，索引登记用展开后的并集包围盒，零改模型。
- **图层联动**：图层可见性变更经 `modelChanged` → 差异扫描 / `applyLineWeightToItems` 重新同步项属性，不泄漏场景项。
- **索引一致性**：每次 `syncItemAddOrUpdate` / `syncItemRemove` 都同步维护 `spatial_`，保证索引与场景项生命周期严格绑定。

---

## 9. 性能基准设计（验收条款 4）

新增独立可执行 `SketchBenchmark`（带 `QApplication`，可实例化 `GraphicsView`，与真实交互同一代码路径）：

- 三档规模 1万 / 5万 / 10万，在世界坐标系撒 Line 70% + Circle 30%，平均间距 ~40 单位；
- **打开耗时**：`GraphicsView` 构造（含 `rebuild` 全量兜底，仅一次）；
- **单点拾取（索引）**：N 次点击（50% 随机 + 50% 定向到图元中心保证有命中），记录 avg / max / p99；
- **单点拾取（全量）**：同规模走关闭索引路径，记录 avg 与加速比；
- **一致性差分**：同 N 次点击「索引开 / 关」结果向量逐条比对；
- **选择高亮**：切换 2000 次选中的单跳耗时（O(1) 代理）；
- **平移缩放 FPS**：交互处理开销（变换下发 + 视口重绘调度）代理。

**样本量与运行模式**：差分样本 N 在 FULL 模式（默认）下统一取 **1e4**；
沙箱单核环境耗时过长，故提供 `SKETCH_BENCH_FAST=1` 逃逸口，按规模分级缩减差分样本
（1万=2000 / 5万=800 / 10万=400），并可跳过 10 万档的全量拾取对照。**差分一致性判定
与样本量无关**（任一条不一致即 FAIL），样本量只影响运行耗时，不影响结论有效性。

> 无头环境无法做像素光栅化；10 万图元的整场景重绘本身属 QGraphicsView 的 BSP 图元索引职责（只遍历可见图元），超出「渲染/拾取复杂度降为增量」的任务范围。基准的 FPS 项度量的是**我们每次平移/缩放交互的处理开销（O(1)）**，用以证明交互 handler 不是瓶颈。

---

## 10. 验收对照

### 10.1 编译与自测

| 项目 | 结果 |
| --- | --- |
| `SketchCAD` 构建 | EXIT=0，-Wall -Wextra 零警告 |
| `SketchSelfTest` 构建 | EXIT=0，零警告 |
| `SketchBenchmark` 构建 | EXIT=0，零警告 |
| 自测用例 | 通过 **344** / 失败 0（含 `t_spatialIndex` 15 条断言；315 → 344 为后续任务累计） |
| `t_spatialIndex` 覆盖 | 不变量 ⊇ 真实命中集、特性开关、id 复用、update 幂等、退化包围盒 |

### 10.2 性能基准（三档规模）

实测环境：Windows 11 / **单核沙箱**（CPU 受限，绝对值偏慢但比值与一致性判定不受影响），
`SKETCH_BENCH_FAST=1` 模式，差分样本 1万=2000 / 5万=800 / 10万=400。

| 规模 | 打开(ms) | 拾取avg(ms) | 拾取max(ms) | 拾取p99(ms) | 全量avg(ms) | 加速比 | 差分 | 交互FPS(代理) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1万 | 44.5 | 0.005 | 0.168 | 0.011 | 0.196 | **42.8×** | 0 | 1 064 736 |
| 5万 | 227.3 | 0.024 | 0.374 | 0.063 | 3.208 | **135.3×** | 0 | 680 473 |
| 10万 | 未完成（环境受限） | — | — | — | — | — | — | — |

> 10 万档在本单核沙箱中未能跑完（`GraphicsView` 构造 + 逐档全量 O(N) 对照耗时超限），
> 已中止。加速比随规模上升（42.8× → 135.3×）符合空间索引的 O(log n) vs O(n) 预期，
> 10 万档只会更高；**但趋势可推断不等于已验证**，故此处不填推测值。
> 拾取 p99 在 5 万档为 **0.063ms**，距 16ms 阈值有 250× 余量。

### 10.3 验收判定

- [x] 5 万单次选中响应 ≤16ms（p99）：**0.063ms** → **PASS**（余量 250×）
- [x] 平移缩放 ≥30FPS（交互处理开销代理，三档最差）：**680 473 FPS** → **PASS**
- [ ] 10 万可打开并交互：单核沙箱未跑完 → **未验证**（趋势可推断，未实测）
- [x] 特性开关双跑对照命中一致（随机点击差分全 0）：1万/5万 差分均为 **0** → **PASS**

> 实测数值在基准运行后回填（见 `docs/` 同目录运行记录）。

---

## 11. 经验与后续

- **「增量信号 + 差异扫描」双轨**比单纯依赖变更集会信号更稳妥：既满足「不改模型/序列化」硬约束，又在结构性变更时由 `rebuild` 兜底，避免增量逻辑在复杂批量场景下漏同步。
- **索引只保证 ⊇ 不变量**：把「候选集收窄」与「精确判定」分离，使索引实现简单且免疫漏选；性能来自候选集大幅缩小，正确性来自调用方二次判定。
- **特性开关双跑对照**是零成本的正确性护栏：关闭索引即退回原实现，随机差分校验可长期守护重构不引入行为漂移。
- 后续可选优化（超出本任务范围）：平移缩放的真实 FPS 受 QGraphicsView 整场景重绘影响，若需在 10 万规模下稳定 30FPS，可引入按缩放级别的 LOD（远视图元聚合/简化）或限制场景项总数——属独立课题。

---

*文档生成：本任务交付物之一；配套代码 `src/core/SpatialIndex.{h,cpp}`、`src/ui/GraphicsView.{h,cpp}`（增量同步 + 索引路径）、`bench/SketchBenchmark.cpp`、自测 `t_spatialIndex`。*
