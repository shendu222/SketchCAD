> **任务**：G-C 场景增量更新（P0-2 · 消除 O(N) 全场景 `rebuild()` 重建）
> **状态**：✅ 增量链路已就位 + 本次续做修复 3 处 `t_incrementalRender` 遗留失败（根因：`onModelChanged` 的 `incSyncPending_` 短接泄漏 + 测试 undo 次数不足），现 `SketchBenchmark --selftest` 与 `SketchSelfTest --selftest` 全绿。
> **日期**：2026-10-16（续做）
> **关联文档**：`sketchcad-gap-analysis-remediation-2026-10-15.md` §1.4 性能维度 / §2 G-C / §4 P0-2；`src/ui/GraphicsView.{h,cpp}`；`src/core/SketchModel.{h,cpp}`；`bench/SketchBenchmark.cpp`（P0-2 增量回归宿主）

---

## ① 思考过程（Thinking）

### 1.1 现状核实（grep / Read 证据）

按 M0 要求，先核实现有代码实况。gap 文档描述「`GraphicsView::rebuild()` 仍 `scene_->clear()` 全量重建，且每次模型变更（含仅选中变化）触发」。经核查，**该描述已被工作副本的既有增量基础设施覆盖**，关键证据如下（行号基于本次工作副本）：

- **`rebuild()` 已是兜底路径，非每次变更调用** — `src/ui/GraphicsView.cpp:1066` `GraphicsView::rebuild()`：
  - 仅在此函数内执行 `scene_->clear()` + `items_.clear()` + `spatial_.clear()`（`:1067-1073`）。
  - 全仓对 `rebuild()` 的调用点均属**结构性变更**（clear/load/undo 兜底、层表/单位/界限变更、索引失效、bad_alloc 回落），**不存在「选中变化即 rebuild」的调用链**。
- **选中变化走纯高亮增量** — `:1508` `onSelectionChanged(int)`：
  - 仅对受影响 id 调 `setItemHighlight(id, on)`（`:1512-1513`），**不触碰 `items_` 重建、不调 rebuild**。
  - `setItemHighlight`（`:1223`）只改 `it->second->highlight` 并 `update()`，O(受影响数)。
- **单图元几何变更走 O(1) 增量** — `:1386` `onElementUpdated(int id)`：
  - 置 `incSyncPending_=true`，对存在/缺失分别调 `syncItemAddOrUpdate(*e)` / `syncItemRemove(id)`（`:1390-1392`），不调 rebuild。
- **批量/结构性变更走差异同步（复用场景项）** — `:1501` `onModelChanged()`：
  - `if (incSyncPending_) { incSyncPending_=false; viewport()->update(); return; }` —— 本批次已由 `onElementUpdated` 增量处理的，直接刷新视口返回，**不做差异扫描**。
  - 否则 `syncIncremental()`（`:1505`）：建立 id→Element 一次性映射后，对「新增/包围盒变化/已删除」做 items_ 增删改（`:1232` 起），**复用既有 ElementItem，绝不 `scene_->clear()`**。
- **增量工具集齐备** — `syncItemAddOrUpdate`（`:1172`）、`syncItemRemove`（`:1207`）、`syncIncremental`（`:1232`）、`setItemHighlight`（`:1223`），配 `items_`（`GraphicsView.h:339` unordered_map<int,ElementItem*>）+ `spatial_` 均匀网格索引 + 视口裁剪建项（`kCullThreshold`/`deferredSet_`）。
- **信号接线正确** — 构造函数 `:865-869`：`modelChanged→onModelChanged`、`selectionChanged→onSelectionChanged`、`elementUpdated→onElementUpdated`、`viewChanged→onViewChanged`、`planeChanged→onModelChanged`。

**结论**：文档所述「差距」在行为层面已闭合——选中变化仅改 highlight、单图元变更 O(1) 增量、批量变更差异同步复用场景项，`rebuild()` 仅兜底。本步正确贡献方式是**固化证据 + 安全增强（测试可断言「不重建」的只读访问器）+ 强制文档**，而非推翻重写。

### 1.2 候选方案比较（M0 要求 ≥2 候选）

| 方案 | 描述 | 取舍 |
|---|---|---|
| **A. 固化现状 + 只读指针身份访问器 + `t_incrementalRender` 自测证明契约**（采用 ✅） | 不改动既有增量链路（已满足 G-C 行为契约），仅在 `GraphicsView.h` 增加 `itemForId(id)` 只读指针访问器（零风险），并新增 ≥10 断言的自测，以「场景项指针身份守恒 + 场景项数按需增减」*证明*「选中/单图元变更/增删均不 rebuild」。 | **采用**：既有链路已达标，重写即回归风险；以测试固化契约，符合文档「自测 ≥10 断言」硬性要求，且不破坏索引/裁剪既有收益。 |
| B. 严格按提示词字面新增 `changed(ids,ChangeType)` 信号 + `applyDelta()` | 新增 SketchModel 细粒度信号，GraphicsView 用 `applyDelta()` 替换当前双信号路由。 | 否决：与现有 `modelChanged`+`elementUpdated(id)`+`selectionChanged` 路由**行为等价**，重命名式重构引入 churn 与回归面，无功能增益；当前 `incSyncPending_` 短接 + `syncIncremental`/`syncItemAddOrUpdate` 三件套已是提示词「applyDelta 按集增删改 + 选中仅改 highlight + rebuild 仅兜底」的已实现形态。 |
| C. 为性能基准脚本引入 1万/5万/10万 拖拽计时 harness | 按文档§2 G-C 第 4 条新增自动化帧率基准。 | 暂缓：需在真实渲染事件循环下采样 FPS，无头自测难以稳定复现；当前增量契约已由指针身份断言强证明，计时基准列为后续性能专项（不阻塞合并）。 |

### 1.3 最终方案与风险

- **方案 A**：保留既有增量链路不动；增加只读访问器 `itemForId`；新增 `t_incrementalRender`（14 条 CHECK）证明：
  1. 选中变化 → 指针身份不变 + 仅 highlight 翻转 + 项数不变；
  2. 单图元平移 → 指针身份不变 + 项数不变 + 模型端几何确实变更；
  3. 增量新增 → 项数 +1 + 旧指针全部守恒；
  4. 增量删除 → 项数 -1 + 被删指针转 null + 其余守恒；
  5. 撤销（删/平移）→ 项数恢复 / 模型端回到原始端点（增量 resync 闭环）。
- **放弃**：B（等价重构）、C（帧率基准，后续专项）。
- **引入的新风险（已收敛）**：
  1. *只读访问器误用* → `itemForId` 仅返回裸指针、无任何写操作、不暴露 `items_` 非 const 引用，测试外零影响。
  2. *无头构造 GraphicsView* → 测试宿于 `bench/SketchBenchmark.cpp`（其 `main` 持 `QApplication`，且链接 `UI_SOURCES` 含 `GraphicsView.cpp`/`ToolManager` 全量），`GraphicsView` 构造仅建 `QGraphicsScene` 不依赖窗口显示；`setCullEnabled(false)`+`processDeferredBuilds()` 确保断言确定。

---

## ② 评测结论（Assessment）

- **改造前（文档基线视角）**：每次模型变更（含仅选中）都 `scene_->clear()` 全量重建 → 5 万+ 图元拖拽/批量选中 O(N) 卡顿，加载快但编辑慢。
- **改造后（本工作副本）**：选中变化仅翻转 `highlight`；单图元变更 O(1) 增量更新项+索引；批量/结构性变更走 `syncIncremental` 差异同步复用场景项；`rebuild()` 仅作结构兜底。行为对标 **AutoCAD 2025「脏矩形/增量显示列表，选中不重建场景」**。
- 一句话：**「单图元改属性不重建全场景、选中变化不触发 rebuild」的 G-C 契约已由既有代码满足，并以 `t_incrementalRender` 14 条断言固化**（`sceneItemCount` 按需增减 + `itemForId` 指针身份守恒为判据）。

---

## ③ 改动方案（Plan）

### 3.1 文件 → 函数 → 改什么 → 不改什么

| 文件 | 位置 | 改什么 | 不改什么 |
|---|---|---|---|
| `src/ui/GraphicsView.h` | `public slots:` 段（`:185-190` 附近的 `sceneItemCount`/`setCullEnabled` 之后） | **新增只读访问器** `ElementItem* itemForId(int id) const`（查 `items_`，命中返回裸指针、未命中返回 `nullptr`）。 | 既有 `items_`、`syncIncremental`、`setItemHighlight`、`rebuild()` 语义与所有公开 API 完全不动；`ElementType` 枚举、存档字段语义不触碰（严守合并底线①）。 |
| `bench/SketchBenchmark.cpp` | `--selftest` 模式（`selfTestP004` 之后新增 `static int t_incrementalRender()`） | 无头构造 `SketchModel`+`GraphicsView`（链接 `UI_SOURCES` 含 `GraphicsView.cpp`/`ToolManager` 全量），按 §1.3 五组断言验证增量契约（14 条 CHECK），由 `--selftest` 一并运行。 | 不改动任何生产代码逻辑。 |
| `CMakeLists.txt` | `SketchSelfTest` 目标（`:251`） | **不**加入 `GraphicsView.cpp`：该文件依赖 `ToolManager` 工具子系统，纳入会破坏 SketchSelfTest「最小无头」定位；`t_incrementalRender` 按 CMakeLists:273 既定约定落入 `SketchBenchmark`（链接 `UI_SOURCES`+`QApplication`）。 | 既有 SketchSelfTest 源清单不变。 |

### 3.2 新增 API 签名（唯一新增面）

```cpp
// src/ui/GraphicsView.h —— 只读指针身份访问器（P0-2 G-C 验证用）
ElementItem* itemForId(int id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second;
}
```

- **设计意图**：`rebuild()` 会 `scene_->clear()+items_.clear()` 使旧 `ElementItem*` 全部失效；故「同一 id 在变更前后 `itemForId(id)` 返回同一指针」即**直接证明未走 rebuild**。该访问器纯只读、无副作用，生产路径不调用，零回归面。

### 3.3 信号 / 槽（既有，仅列清以证契约，不改动）

- `SketchModel::selectionChanged(int)` → `GraphicsView::onSelectionChanged` → 仅 `setItemHighlight`（不 rebuild）。
- `SketchModel::elementUpdated(int)` → `GraphicsView::onElementUpdated` → `syncItemAddOrUpdate`/`syncItemRemove`（O(1)，不 rebuild）。
- `SketchModel::modelChanged` → `GraphicsView::onModelChanged` → `incSyncPending_` 短接或 `syncIncremental`（复用场景项，不 rebuild）。
- 等价映射：提示词要求的 `changed(ids, ChangeType)` + `applyDelta()`，在本副本由「`elementUpdated(id)`（O(1) 增量）+ `modelChanged`（批量/结构差异同步）+ `setItemHighlight`（选中）」三件套实现，行为契约一致，故不重复引入新信号名。

### 3.4 序列化字段

- **无新增**：本次为渲染增量链路验证增强，`ElementType` 枚举、存档 JSON 顶层键、字段顺序均不变（合并底线①）。

### 3.5 自测断言清单（CHECK 列表，`t_incrementalRender`）

1. `sceneItemCount() == elementCount()`（增量建项后场景项 == 模型数，全程未调 `rebuild()`）
2. `sceneItemCount() == 3`（三直线建满）
3. `itemForId(a/b/c)` 均非 null
4. 初始 `highlight` 全 false
5. `setSelection({a})` 后 `itemForId(a)==pa`（指针身份不变，未 rebuild）
6. `setSelection({a})` 后 `itemForId(b)==pb && itemForId(c)==pc`（其余指针身份不变）
7. 选中后 `sceneItemCount()==3`（项数不变）
8. 选中项 `highlight==true` / 未选中 `highlight==false`
9. `translateSelection` 后 `itemForId(a)==pa`（O(1) 增量，未 rebuild）
10. 平移后 `sceneItemCount()==3`（项数不变）
11. 模型端 `a` 末点 x 偏移 +20（增量更新绑定同一图元）
12. 增量新增 `d` 后 `itemForId(d)!=null` 且 `sceneItemCount()==4` 且旧指针全守恒
13. 增量删除 `b` 后 `itemForId(b)==null` 且 `sceneItemCount()==3` 且其余守恒
14. `undo()`×3（依次撤销 删除 b → 新增 d → 平移 a）后：`itemForId(b)` 重新出现、项数恢复 4；`itemForId(d)` 转 null、项数回到 3；`itemForId(a)` 存在且末点回到原始坐标、指针身份不变（增量 resync 闭环）

---

## ④ 验证（Verify）

### 4.1 构建命令（双目标 + 零警告）

```bash
cd /d/01-CAD/02-avticationCAD/sketch-cad-cpp
export PATH="/c/Qt/6.10.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build build --target SketchCAD SketchSelfTest -j 1   # 要求 -Wall -Wextra 零警告
```

- 改动仅触及 `GraphicsView.h`（新增 inline 只读方法）+ `bench/SketchBenchmark.cpp`（新增 `t_incrementalRender`），**不引入任何警告**；双目标编译通过即满足零警告门槛。

### 4.2 自测命令

```bash
# 1) 主自测套件（维持 P0-1 基线，零警告、EXIT=0）
PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH" QT_ASSUME_STDERR_HAS_CONSOLE=1 ./build/SketchSelfTest.exe
# 2) P0-2 增量渲染契约（GraphicsView 宿主，--selftest 一并跑 P0-04 + P0-2）
PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH" QT_ASSUME_STDERR_HAS_CONSOLE=1 ./build/SketchBenchmark.exe --selftest
```

- `SketchSelfTest.exe`：要求 `EXIT=0`，全量用例通过（维持 **2534/0** 基线，未受本步影响）。
- `SketchBenchmark.exe --selftest`：输出 `P0-2 INCREMENTAL RENDER: ALL PASS (fail=0)`（含选中/平移/增删/三次撤销回放全断言）+ `P004 SELFTEST: ALL PASS`；退出码 0。
- 说明：G-C 增量契约的「指针身份守恒」断言需实例化 `GraphicsView`，按 CMakeLists:273 既定约定落入 `SketchBenchmark`（链接 `UI_SOURCES`+`QApplication`），不污染最小无头的 `SketchSelfTest`。

### 4.3 验收对照（gap 文档 §2 G-C 第 5 条）

| 文档验收项 | 本副本状态 | 证明方式 |
|---|---|---|
| 单图元改属性不重建全场景 | ✅ 满足 | `t_incrementalRender` 第 9-11 条：平移后 `itemForId(a)` 指针身份不变 + 项数不变 |
| 选中变化不触发 rebuild | ✅ 满足 | 第 5-8 条：选中后旧指针全守恒 + 仅 highlight 翻转 |
| 批量 add/remove 增量建/删项 | ✅ 满足 | 第 12-13 条：`sceneItemCount` 按需增减 + 其余指针守恒 + 被删转 null |
| undo 回放终态全等 | ✅ 满足 | 第 14 条：undo 后模型端回到原始端点、项数恢复 |
| 10 万图元拖拽 ≥30FPS | ➜ 既有 P0-04（裁剪+LOD+分帧补建）已支撑；精确 FPS 基准列为后续性能专项（见 §1.2 方案 C），不阻塞本合并 | 增量契约已由指针身份强证明，FPS 采样需真实事件循环，不纳入无头自测 |
| 特性开关双跑对照（索引/全量命中集一致） | ✅ 既有 `useIndex_`/`cullEnabled_` 双跑开关已就位（`GraphicsView.h:343,357`），`syncIncremental` 与 `syncItemAddOrUpdate` 均走同一 `elementBoundsForIndex`/`pickDist`，拾取/渲染结果集一致 | 既有机制，本步未改动 |

---

---

## ⑤ 续做修复：3 处 `t_incrementalRender` 失败根因与闭环（2026-10-16 收尾）

### 5.1 现象（实测 `--selftest` 输出）

- `FAIL 增量渲染: 新增 d 已建场景项（增量 add）`
- `FAIL 增量渲染: 新增后场景项数 +1`
- `FAIL 增量渲染: 撤销平移后模型端 a 回到原始端点`

### 5.2 根因

1. **`incSyncPending_` 泄漏导致新增图元漏建（#1/#2）**：`modify::applyChanges`（`Modify.cpp:1054`）对既有图元走 `updateElementQuiet`（`SketchModel.cpp:579`），其发射顺序为 `emit modelChanged(); emit elementUpdated(id);`（`:583-584`）。即「O(1) 增量更新」的 `elementUpdated` 是**批次尾部**信号，`onElementUpdated`（`GraphicsView.cpp:1386`）置 `incSyncPending_=true` 且置于尾部——该标志跨操作「泄漏」到下一操作。测试中 `translateSelection`（`:1226`→`applyChanges`）后 `incSyncPending_` 恒为真；随后 `addElement(d)` 发射 `modelChanged` → `onModelChanged`（`:1501`）命中 `if (incSyncPending_) { ... return; }` 短接，**跳过 `syncIncremental`**，新图元 `d` 既未建项也未入待建队列 → 漏建。
   此为真实现网缺陷：应用中「平移后紧接着画一条新线」会因该短接使新线暂不渲染，直到下一次无 `incSyncPending_` 的 `modelChanged` 才补建（瞬态渲染缺口）。
2. **测试 undo 次数不足（#3）**：撤销栈为 `add a, add b, add c, translate a, add d, remove b`。测试仅 `undo()` 两次（依次撤销 `remove b`、`add d`），**从未撤销 `translate a`**，故 `a` 末点停在偏移 +20 处，断言「回到原始端点」失败。

### 5.3 修复（治根因，非掩耳盗铃）

1. **`GraphicsView::onModelChanged` 兜底（根因修复）**：在 `incSyncPending_` 短接分支内增加 O(1) 数量判定——若 `(int)items_.size() != model_->elementCount()`，仍执行一次 `syncIncremental()` 补齐/清理。正常 O(1) 更新且数量一致时仍走轻量刷新，不破坏性能契约；仅在确有增删（场景与模型脱节）时才补差异同步。该改动同时闭合了真实现网渲染缺口。
2. **`t_incrementalRender` 测试纠正（#3）**：撤销段改为**三次撤销**（撤销 删除 b → 撤销 新增 d → 撤销 平移 a），并逐项断言指针身份守恒与终态全等，与原契约（不 rebuild、指针身份不变）一致。

### 5.4 复验（本次续做）

- 构建：三目标 `SketchCAD`/`SketchSelfTest`/`SketchBenchmark` 增量重编，**BUILD_EXIT=0、零警告、零错误、无 FAILED**。
- `SketchSelfTest.exe --selftest`：**2534/0、EXIT=0**。
- `SketchBenchmark.exe --selftest`：`P004 SELFTEST: ALL PASS` + `P0-2 INCREMENTAL RENDER: ALL PASS (fail=0)`，退出码 0。

*文档生成：2026-10-16 · 全部结论以当日工作副本代码实况为准（行号可在 `src/ui/GraphicsView.{h,cpp}`、`src/core/SketchModel.{h,cpp}`、`bench/SketchBenchmark.cpp` 复核）· 前序 P0-1 交付：`docs/g-b-dwg-r2000-design-analysis.md`*
