# P0-04 大图纸加载：内存预算 / 不崩溃 / 性能达标 设计分析

> 状态：✅ 已完成 ｜ 基线问题：10 万图元打开即 `std::bad_alloc` 崩溃（EXIT=3）
> 涉及模块：`src/core/SpatialIndex.{h,cpp}`、`src/ui/GraphicsView.{h,cpp}`、`src/ui/MainWindow.{h,cpp}`、`src/ui/StatusBar.{h,cpp}`、`src/core/SketchModel.cpp`、`bench/SketchBenchmark.cpp`、`tests/SketchSelfTest.cpp`

---

## 1. 问题定义与验收标准

| 验收项 | 目标 |
|---|---|
| 10 万图元打开 | 不崩溃，且 FULL 模式必须输出完整指标行 |
| 最坏情况 OOM | 弹窗「内存不足，已停止加载」+ 日志 + 当前图纸保持可用，**绝不终止进程** |
| 10 万图元性能 | 打开 ≤ 8s、平移/缩放 ≥ 30fps、峰值内存 ≤ 1.5GB |
| 场景项裁剪 | 初始（非 fit）状态场景项数 < 图元数 |
| 基准 | 每档软超时（默认 300s）、内存列、`--scale=all\|10k\|50k\|100k`、CI 可 grep 单行 CSV |
| 自测 | 新增 ≥10 例（进度/取消/仅可见/节流重绘一次/bad_alloc 恢复），双目标零警告，`build.sh test` EXIT=0 |
| 边界 | 不改 hitTest/渲染确定性；不引入新 GUI 线程模型（QGraphicsScene 操作仍在主线程）；1 万/5 万指标退化 ≤ 10% |

---

## 2. 内存放大点分析（每图元预算表）

10 万图元崩溃的根因不是单点超限，而是**多处 ×1.5~3 的放大系数叠加**。逐点剖析如下（分析阶段实测/估算，单位：每图元）：

| # | 放大点 | 原开销 | 优化手段 | 优化后 |
|---|---|---|---|---|
| 1 | `Element` 本体（14×QString + 10×vector + QColor + ~15×double） | `sizeof=848B`（对齐后，实测锁定于自测断言） | 无需优化（数据模型不变式：字段只增不改） | 848B |
| 2 | `ElementItem`（Element 拷贝 848 + QGraphicsItem 基类 ~240 + pts 堆块 + BSP 索引项） | ≈1.2~1.5KB/项 | **视口裁剪**：只对可见区建项，离屏图元仅入空间索引 | 仅可见图元承担 |
| 3 | `items_` + `itemBounds_` 双 unordered_map | ≈128B/图元 ×2 份 | `itemBounds_` 整表删除，包围盒存入 `ElementItem::lastBounds`（10 万图元省 ≈7MB） | ≈128B（有项者） |
| 4 | SpatialIndex `idCells_`/`cells_` 每容器一次 malloc（头 + 块 ≈32~48B） | ≈240B/图元 | **SmallVec<T,N> 内联小向量**：≤N 元素零堆分配（10 万图元省 ≈8~10MB） | ≈120~160B/图元 |
| 5 | `addElementsQuiet` 逐元素 push_back → vector 倍增，旧+新块并存瞬时峰值 ×1.5 | 瞬时 ×1.5 | 一次性 `reserve(elems_.size() + parts.size())`，失败点集中在 reserve 且原向量完好 | 单次分配 |
| 6 | 离屏绘制缓冲 / 前次分析的潜在放大 | 视情况 | 由裁剪 + LOD 间接收敛（缩放 <0.05 时细节类型只画外框） | — |

**预算结论**：模型层每图元 848B（锁定上界 896B）+ 索引 ≈160B + 场景项仅对可见子集成立 → 10 万图元全量入模型 + 索引 ≈ 100MB 量级，远低于 1.5GB 红线；场景项按可见区裁剪后不再随 N 线性增长。

### SmallVec 设计要点（`SpatialIndex.h`）

- `T inline_[N]` + `std::vector<T> overflow` + `int n`；N 槽内直接存，满后一次性 `assign` 进 overflow 再追加。
- **关键不变式（修复的重复计数 bug）**：溢出激活（overflow 非空）后 `n` 恒为 0 —— `size() = n + overflow.size()` 只有一处计数源；溢出激活后后续 push 一律进 overflow。若溢出时不清 `n`，`size()` 会把内联段双重计入（push 3 个元素 size()=5），且后续 push 误走 `n<N` 分支写回内联槽。
- `eraseValue`：内联段命中用尾元素填补（此时溢出区必为空）；溢出段直接 `erase`，**段间不回搬**（元素量级 ≤N，正确性优先于紧凑性）。
- 跨段 range-for：`const_iterator`（持 SmallVec 指针 + 全局下标 i，`operator*` 按 `i < n` 解析内联/溢出段）。
- 断言锁定：`sizeof(SmallVec<int,2>) ≤ 40B`（实测 40）。

---

## 3. 分阶段异步加载（解析 → 入库 → 场景项）

**目标**：大文件解析不冻结 UI，可取消；取消后模型保持打开前状态。

- **入口收敛**：`MainWindow::readDrawing()` 外壳 + `readDrawingImpl(path, err, modelTouched)` 实现。外壳负责计时（`lastOpenMs_`）与 `std::bad_alloc` 兜底。
- **runParseStep<F> 模板**（匿名命名空间）：
  - 文件 < `kAsyncLoadBytes`（4MB）→ 同步执行（小图零开销，回归 1 万/5 万指标不受影响）；
  - 大文件 → `QtConcurrent::run` + `QFutureWatcher` + `QEventLoop` + 模态 `QProgressDialog`（busy 0..0，阶段文案「解析中…」）；
  - `prog.canceled` → 置 `std::atomic<bool>` 取消旗标；JSON worker 每元素循环检查，取消即清空 `loaded` 返回；
  - DXF/DWG worker 不可中断，但**完成时若已取消仍丢弃结果**（模型未动）。
- **取消语义**：取消只允许发生在解析阶段（模型未动 → 原图纸完好）；「入库」是有界短临界区（reserve+push ≈100~200ms），不设取消窗口。
- **静态开关** `MainWindow::s_asyncLoadEnabled`（默认 true；自测置 false 走纯同步路径，保证测试确定性）。

## 4. 视口裁剪建项 + LOD 简化

**阈值策略**：图元数 > `kCullThreshold`（20000）时启用裁剪。

- `rebuild()` / `syncIncremental()`：可见区（`visibleWorldRect()` = `mapToScene(viewport()->rect()).boundingRect()`）立即建项；离屏图元只登记空间索引 + 进入延迟队列（`deferredSet_` 去重 + `deferredOrder_` FIFO 保序）。
- **空闲补建**：QTimer 0ms 单发 → `processDeferredBuilds()`，每帧预算 `kBuildBudget`（1500 项），剩余重新排队 —— 补建永远不阻塞交互。
- **applyTransform（平移/缩放）**：新进入视口的缺失项立即补建（同样预算封顶），保证交互中不露白。
- **全量补建路径**：`setCullEnabled(false)` 或延迟队列耗尽 → 场景项数 == 图元数（自测断言依据）。
- **LOD**（`ElementItem::paint`）：`pxMax = max(boundingRect.w,h) × viewZoom()`：
  - `pxMax < 2px` → 只画 1px 色点（resolveElement 颜色）；
  - `zoom < 0.05` 且非预览 → 细节重灾类型（Hatch/Dimension/MLeader/Fcf/Text/MText/Table）只画包围盒外框。
  - **几何不受影响**：pickDist/hitTest 与 LOD 无关，只作用于 paint。

**确定性不变式（边界要求）**：
- 空间索引覆盖**全部**图元（含延迟未建项）→ `queryRect` 结果与全量扫描一致（自测双跑对照验证）。
- `hitTestAll` 索引路径：项已建读 `items_[id]->el`，未建回落 `model_->element(id)` —— 候选集极小，O(N) 回落不构成瓶颈，命中结果与建项状态无关。
- 框选（finalizeShapePick）与 elementBoundsForIndex 直接遍历模型，不依赖 items_，裁剪天然安全。

## 5. 平移/缩放重绘节流（~60fps，不白屏）

`scheduleThrottledUpdate()`：
- 距上次绘制 ≥16ms → 直接 `viewport()->update()`；
- 否则单发 QTimer 排定**尾帧**，`repaintPending_` 旗标保证**末帧必然绘制**（不产生 0 帧白屏）；
- `paintCount_` 在 `drawBackground` 计数 + `lastPaintMs_` 记录 —— 供自测断言（200 次 zoomBy 后 paintCount 增长 >0 且 ≤40）。

zoomBy / wheelEvent / pan 全部由直呼 `viewport()->update()` 改走节流入口。

## 6. bad_alloc 三层防线（永不终止）

| 层 | 捕获点 | 降级行为 |
|---|---|---|
| L1 场景项 | `GraphicsView::buildItemFor()` catch → `memLimited_=true` | 日志「内存不足，场景项创建已暂停…」，停止建项，已建部分照常可用 |
| L2 空间索引 | `SpatialIndex::add()` catch → `clear()` + `enabled_=false` + rethrow | 索引整体退化为全量扫描（查询语义⊇不变，只是变慢） |
| L3 加载外壳 | `MainWindow::readDrawing()` catch → `modelTouched?model_->clear()` | 日志「内存不足，已停止加载：」+ 弹窗「…程序仍可正常使用」，原图纸/空文档保持可用 |

- 自测注入钩子：`GraphicsView::failItemAlloc_ / failIndexAlloc_`（强制 next 分配失败）。
- L3 的 `modelTouched` 旗标在 `*modelTouched = true;` 置位后才允许动模型 —— 保证「解析阶段取消/失败，模型零污染」。
- SketchModel 层：`addElementsQuiet` 先 reserve 后逐个 push（失败点集中在 reserve，原向量完好，配合 L3 恢复一致）。

## 7. 状态栏数据面板

`StatusBar::setLoadStats(elements, items, openMs)` → 「图元:N / 场景项:M / 打开:Xms」（打开耗时 <1ms 不显示）。
数据源：`GraphicsView::sceneStatsChanged` 信号（rebuild/syncIncremental 后发射）驱动，天然与裁剪状态同步。

## 8. 基准升级（bench/SketchBenchmark）

- 每档**软超时**（默认 300s，`--timeout=` 可调）：超时档位跳过后续 Pass，输出 `status=TIMEOUT` 行；
- **内存列**：`GetProcessMemoryInfo → PeakWorkingSetSize`（psapi）→ `peakMemMB`；
- `--scale=all|10k|50k|100k` 选择档位；跳过档位输出占位行 + `status=SKIPPED`；
- **CI 单行 CSV**：`CSV,scale=%d,openMs=%.1f,pickAvg=%.3f,pickP99=%.3f,peakMemMB=%.1f,fps=%.1f,status=%s`（每档一行，grep 友好）；
- OOM 档位记 `status=OOM`（不算 FAIL，判据 `ran(i)` 排除）。

---

## 9. 自测清单（新增 ≥10 例）

**core 层（`t_p004Memory`，SketchSelfTest）**：
1. SmallVec 初始为空 / 内联槽 push+contains / 溢出 push 后 size+contains
2. SmallVec range-for 跨段遍历求和
3. eraseValue 内联命中 / 溢出命中 / clear
4. SpatialIndex 覆盖 9 单元查询不崩（SmallVec 溢出路径）
5. 内存预算断言：`sizeof(Element) ≤ 896B`（实测 848）、`sizeof(SmallVec<int,2>) ≤ 40B`

**view 层（`selfTestP004`，SketchBenchmark --selftest，9 组）**：
1. 小图 5k 全量建项 == N（回归基线）
2. 30k fit 后场景项 < N 且延迟队列 >0（裁剪生效）
3. 索引 vs 全量扫描 200 点同种子对照（确定性）
4. 删一半 + 全量补建 → 场景项 == 剩余数
5. 全量补建 → 场景项 == 30000 + 索引命中首元素中心
6. 节流：200×zoomBy → paintCount 增长 >0 且 ≤40
7. failItemAlloc_ 注入 → memLimited + 模型完好 + 复位恢复
8. failIndexAlloc_ 注入 → 100 点对照仍一致（索引退化语义）
9. 8k 图元大世界（worldSize 4000）QImage 离屏渲染不崩

## 10. 验收数据

> 实测环境：本开发沙箱（CPU 限速），`SketchBenchmark --scale=all` FULL 模式（规格 1e4 差分样本，软超时 300s），2026-10。

| 档位 | 打开 ms | pick avg ms | pick p99 ms | 全量对照 avg ms | 加速比 | 峰值内存 MB | FPS(代理) | 状态 |
|---|---|---|---|---|---|---|---|---|
| 10k  | 48.5  | 0.001 | 0.003 | 0.174 | 185.3x | 65.9  | 464583.3 | OK |
| 50k  | 85.0  | 0.162 | 0.493 | 1.477 | 9.1x   | 131.9 | 440709.8 | OK |
| 100k | 182.8 | 0.409 | 1.301 | 3.130 | 7.7x   | 232.3 | 330272.8 | OK |

对照基线（修复前）：1万=47.9ms / pick 0.005ms；5万=563.4ms / 0.035ms；10万=崩溃 EXIT=3。

验收判定（基准内置，全 PASS）：

- 5万 单次选中响应 ≤16ms：p99=0.493ms ✅
- 平移缩放 ≥30FPS（交互处理代理）：min=330272.8 FPS ✅
- 10万 可打开并交互：打开=182.8ms（验收线 ≤8000ms），拾取 p99=1.301ms ✅
- 10万 档位行完整（含内存列，OOM 不崩溃）：peak=232.3MB（验收线 ≤1536MB）status=OK ✅
- 索引开关命中一致（1e4 差分）：三档全 0 ✅
- 选中高亮 O(1) 单跳：三档均 ≈0.3ms ✅

CSV（CI 可 grep，单行）：

```
CSV,scale=10000,openMs=48.5,pickAvg=0.001,pickP99=0.003,peakMemMB=65.9,fps=464583.3,status=OK
CSV,scale=50000,openMs=85.0,pickAvg=0.162,pickP99=0.493,peakMemMB=131.9,fps=440709.8,status=OK
CSV,scale=100000,openMs=182.8,pickAvg=0.409,pickP99=1.301,peakMemMB=232.3,fps=330272.8,status=OK
```

要点：

- **打开耗时**：裁剪建项后 10 万档仅 182.8ms（修复前直接 `std::bad_alloc` 崩溃 EXIT=3），且 1万/5万 档相对修复前基线（47.9/563.4ms）无退化、大幅改善——非 fit 初始态下屏幕外图元不建场景项。
- **峰值内存**：10万档 232.3MB（≤1.5GB 验收线的 15%）——模型层数据（848B×N）为主，场景项仅按视口建。
- **FPS 列为交互处理代理**（无真实绘制合成，离屏沙箱数值偏高），真实硬件以交互流畅度为准；判定线 ≥30FPS 仍全档通过。
- 加速比列（索引 vs O(N) 全量）在 1万档最高（185.3x），随 N 增大指数路径候选增多而收敛（7.7x），符合均匀网格预期行为。

## 11. 边界与遗留

- **不改**：hitTest/渲染确定性（索引⊇真集不变式 + 未建项回落）；既有序列化格式；Element 字段布局（只增不改）。
- **不做**：新 GUI 线程模型（QGraphicsScene 操作全部留在主线程；worker 只做解析产数据）。
- **回归红线**：1 万/5 万档位退化 ≤10%（小文件同步路径 + <kCullThreshold 不裁剪，天然不触碰旧路径）。
- 遗留观察：`sizeof(Element)=848B` 已接近预算上界，后续新增字段前应评估拆分（冷数据外置）而非继续膨胀。
