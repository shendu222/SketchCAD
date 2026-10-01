# 夹点编辑与增强选择（Grip Edit & Selection Enhancement）设计深思

> 第七阶段落地文档。覆盖：夹点热操作（Stretch/Move/Rotate/Scale/Mirror）、栏选/圈围/圈交三种增强选择、选择循环。
> 验证基线：自测 56/0（新增 t_shapeselect + t_gripops），SketchCAD / SketchSelfTest / SamplePlugin / SketchCADAutomation / AbiProbePlugin 全目标零警告。

---

## 1. 需求分析

| # | 需求 | 验收要点 |
|---|------|----------|
| R1 | **夹点编辑** | 选中图元显示夹点；单击夹点进入热态；支持 5 种热操作：拉伸、移动、旋转、缩放、镜像；按 Ctrl 循环切换当前夹点的操作模式；全程动态预览；支持确认（拖拽释放/再次单击/Enter）与取消（Esc） |
| R2 | **增强选择** | 新增栏选（Fence）、圈围（WP）、圈交（CP）三种选择方式，可批量选取不规则范围内的对象 |
| R3 | **选择循环** | 位置重叠的多个对象之间可循环切换选取目标 |
| R4 | **一致性 + 边界** | 与现有交互风格一致（命令行/菜单/快捷键/提示文案）；正确处理镜像轴方向、旋转基准点、缩放比例零或负值等边界 |
| R5 | **设计文档** | 输出需求分析、功能设计思路、交互流程与关键决策说明（即本文） |

### 1.1 与现有架构的耦合点（摸底结论）

- 夹点绘制与拖拽入口：`SelectTool::onPress` 首行 `ctx.beginHandleDrag`，GraphicsView 侧 `handleAt` 命中后 `collectLinked + beginEdit + moveHandles` 静默联动。
- 几何变换引擎完备：`modify::makeRotate / makeScale / makeMirror / makeMoveCopy / makeStretch` 均产出 `Change{id, replacement}`，经 `modify::applyChanges` 一次落盘（一个 Ctrl+Z 步）。
- 命令行激活工具无需 ICommand：`MainWindow::runToken` 回退 `ToolRegistry::find(cmd) → model_->setActiveTool(cmd)`。
- 右键兜底分支天然承接采集期取消（「已结束当前绘制」）。

---

## 2. 功能设计思路

### 2.1 架构分层

```
┌─ ui 层（状态机 + 呈现）─────────────────────────────┐
│ GraphicsView  夹点热态状态机 / 预览通道 / 选择循环   │
│ SelectTool    onPress 优先级：热态 > Ctrl循环 > 选取 │
│ ShapeSelectTool  Fence/WP/CP 采集（一类三实例）      │
├─ core 层（纯几何，无 UI 依赖，可无头单测）──────────┤
│ GripOps       5 模式 → Change 翻译 + 边界拒绝        │
│ ShapeSelect   Fence/WP/CP 三语义命中判定             │
├─ Modify 层（复用，不重写几何）──────────────────────┤
│ makeRotate / makeScale / makeMirror / makeMoveCopy   │
└─────────────────────────────────────────────────────┘
```

core 层零 Qt Widgets 依赖，`GripOps::apply` 与 `shape::hit/pick` 均为纯函数，直接被 `t_gripops` / `t_shapeselect` 无头验证。

### 2.2 GripOps：复用 Modify 层而非重写几何

| 模式 | 实现 | 作用集 |
|------|------|--------|
| Move | `makeMoveCopy(delta, copy=false)` | moveSet（选中集） |
| Rotate | `makeRotate(moveSet, base, Δθ)`，Δθ = 方位角差 wrap 到 (-180,180] | moveSet |
| Scale | `makeScale(moveSet, base, f)`，f = dist(cursor,base)/dist(refCursor,base) | moveSet |
| Mirror | `makeMirror(moveSet, isPointMirror=false, base, cursor)`，轴 = base→cursor | moveSet |
| Stretch | 不用 `makeStretch`（其单端点迁移无联动语义）→ 按 `(elemId, handleIdx)` 引用 + `geo::setElementHandle` 精确迁移 | stretchRefs（collectLinked 锁定 + 自身 handle）∪ stretchSet |

**作用集拆分**是关键决策：

- `moveSet`：选中集，**剔除 Constraint 类型**（约束不参与变换，保持求解器语义）。
- `stretchSet`：联动集 ∪ 选中集——联动可能包含**未选中**图元（拖端点带动相邻线另一端，拓扑保持）。
- `stretchRefs`：`collectLinked` 返回的锁定引用 + 自身 handle 补入，保证「拉伸的图元自身」也被迁移。

### 2.3 零参考光标（refCursor）机制

热态进入时 `refCursor = 夹点位置`。之后每帧预览均以**相对量**计算：

- Rotate：`Δθ = angle(cursor−base) − angle(refCursor−base)` → 预览从恒等起步，无跳变。
- Scale：`f = dist(cursor,base) / dist(refCursor,base)` → 初始 f=1。
- Move：`delta = cursor − refCursor` → 初始位移为零。
- Mirror：轴 = base→cursor（重合 < 1e-9 拒绝）。

**Ctrl 切模式时 `refCursor = lastMouseWorld_` 重置**——新模式的预览从「当前位置的恒等」重新出发，避免切换瞬间图形跳变；同时 `gripDragMoved_ = false`，允许切模式后重新走双段确认。

### 2.4 夹点热态状态机（双段确认）

```
            单击夹点(press#1)
                 │ beginGripHot(w)
                 ▼
   ┌──────── 热态 gripHot_ ────────┐
   │  move: 更新预览 + 拖拽标记     │
   │  Ctrl: nextMode + 重置参考     │
   │  Enter: commit  Esc: cancel    │
   └───────┬───────────┬───────────┘
   release#1 拖拽过        release#1 未拖拽
           │                     │ gripArmed_=true (armed)
           ▼                     ▼
        commit ──────────── release#2 → commit
```

- press#1 在夹点上 → 进入热态；热态中所有左键 press 被 `mousePressEvent` 拦截不传工具。
- release#1 已拖拽 → 直接 commit（保留旧「拖完就走」手感）。
- release#1 未拖拽 → armed，等待第二次释放或 Enter 确认；期间移动光标仍实时预览。
- 落盘三件套与 `translateSelection` 同款：`beginEdit("夹点N") + modify::applyChanges`，一次 Ctrl+Z 可整体撤销。
- 任何状态破坏路径（resetPending / 工具切换 / rebuild）都会 cancel 并清预览项。

### 2.5 ShapeSelect 三语义判定矩阵

| 模式 | 几何判定 | 退化形状 |
|------|----------|----------|
| Fence 栏选 | 图元线段与 Fence 折线相交 **或** 采样点落在 Fence 段近邻（tol 内） | 点数 < 2 → 不命中 |
| WP 圈围 | **全部**采样点在多边形内（pointInPolygonTol，边界 1e-6 容差） | 点数 < 3 → 不命中 |
| CP 圈交 | 任一线段穿多边形 **或** 任一采样点在多边形内 | 点数 < 3 → 不命中 |

- `segments`：优先 `geo::flatten`（与求交同源，避免离散差异）；圆/弧 flatten 为空时用 24 段圆周逼近补齐。
- `samplePoints` 白名单：Circle 24 等分、Arc 沿弧 25 点、Ellipse=sampleEllipse、Spline=sampleSpline，其余类型回落 `e.pts`。
- **BlockRef 结算**：core 层 `pick` 不处理块；UI 层 `finalizeShapePick` 对 BlockRef 展开子图元逐个 `shape::hit`，任一命中整块入选。
- 命中图元一律 `addToSelection`（**加选语义**），不替换现有选区。

### 2.6 选择循环锚点机制

- `hitTestAll`：容差内返回全部命中，按距离升序。
- Ctrl+点击重叠区（>1 命中）→ `cycleSelection`：
  - 锚点判据：新点击与 `cycleAnchor_` 屏幕距离 < 8px 且命中集合未变 → 在 `cycleList_` 上递进；
  - 首次进入 `idx = 1`（跳过距离最近的顶层——它本就是普通单击的结果）；
  - 普通选择 / 空白点击 → `breakCycle` 清态。
- Shift 保持 toggle 语义不变。

---

## 3. 交互流程

### 3.1 夹点热操作

1. 选中图元 → 夹点显示（端点/圆心/中点等，沿用既有 handle 规则）。
2. 单击夹点 → 热态：夹点变实心橙块，状态栏提示「夹点拉伸：移动光标预览，按住拖拽或再次单击确认，Ctrl 切换模式，Esc 取消」。
3. 移动光标 → 独立 `QGraphicsPathItem` 虚线橙预览（Z=1001，不进模型、不落盘）。
4. Ctrl → 模式循环 Stretch→Move→Rotate→Scale→Mirror→Stretch，提示当前模式；参考光标重置，预览从恒等起步。
5. 确认：拖拽后释放 / armed 态再次单击 / Enter → `beginEdit + applyChanges` 一次落盘，日志「夹点N：完成，M 个图元已更新」。
6. 取消：Esc / 工具切换 → 丢弃预览，模型零改动。

### 3.2 Fence / WP / CP 采集

1. 命令行输入 `fence` / `wp` / `cp`（或菜单「选择」栏），自动回退激活对应工具——与 Line 等绘图命令同通路。
2. 左键依次取点（`snapWorld` 捕捉），画布实时预览：Fence 串折线不闭合；WP/CP 串折线并自动闭合。
3. Enter / 双击结算 → 遍历图元（BlockRef 展开）三语义判定 → 命中者并入选区；空结果提示「没有图元被选中」。
4. Esc / 右键取消，清预览与 pending 点。

### 3.3 选择循环

1. Ctrl+点击重叠区 → 日志「选择循环：第 2/3 个」，选区切至次近对象。
2. 继续按住 Ctrl 点击同一位置 → 循环递进。
3. 移开后普通单击 → 正常选择并 breakCycle。

---

## 4. 关键决策说明

| 决策 | 理由 |
|------|------|
| **Stretch 不复用 makeStretch** | 其语义是「单端点迁移」，无 collectLinked 联动；改为按 `(elemId, handleIdx)` 引用 + `geo::setElementHandle`，联动端点由 refs 列表精确表达。 |
| **Mirror 保留源** | `makeMirror(moveSet, false, base, cursor)` 与菜单「镜像」语义一致（副本 + 新增），夹点镜像不吞原图。 |
| **Constraint 剔出 moveSet** | 约束元素有自己的求解通路，被几何变换硬改会污染 `solveAfterChange` 的隐式约束维持。 |
| **fence/wp/cp 为加选** | 与 Ctrl 加选、框选加选的既有直觉一致；用户可用 Esc 前的选区继续叠加操作。 |
| **Ctrl 在重叠区语义让位选择循环** | Ctrl+点击原语义是「追加 toggle」，重叠区时切换为循环更符合「重叠对象间切换选取」的需求主诉；非重叠区 Ctrl 行为不变。 |
| **命令行回退而非 ICommand** | 三工具无参数、无状态机命令面，走 `ToolRegistry::find → setActiveTool` 零成本接入，与绘图工具同构。 |
| **预览不进模型** | 夹点预览用独立 path item（Z=1001）；Fence 预览复用 `showPreview + createGuideLine` 引导层——两条通道都不落盘，Esc 零残留。 |
| **退化形状静默不命中** | Fence<2 点、WP/CP<3 点直接返回空集，不报错——与「单击空白取消选择」的宽容语义对齐。 |
| **Scale/Mirror 边界拒绝** | factor ≤ 0 或非有限 → reject「缩放比例必须为正数」；镜像轴两点重合 < 1e-9 → reject「镜像轴两点重合」。拒绝文案经日志呈现，模型零改动。 |
| **Rotate 角度 wrap (-180,180]** | 避免累计多圈；方位角差经 fmod 归一，与 Modify 层旋转语义一致。 |
| **Scale 基距 < 1e-9 返回 1** | 光标与参考点重合时因子退化为 1（恒等），而非除零爆炸。 |

---

## 5. 过程陷阱（供后续同类开发参考）

1. **ToolRegistry 注册残留**：Edit 的 old_string 只锚定首行导致原文尾部两行残留为语法错误——多行插入必须让 old_string 覆盖完整锚定段。
2. **`#include <iterator>`**：`std::back_inserter` 所在头，缺它 range-for/back_inserter 编译失败。
3. **`geo::Seg` 命名空间限定**：`Seg` 定义在 `geo` 内非全局，头/实现/局部变量三处均须写 `geo::Seg`。
4. **测试断言须先手算几何**：fence (6,6)-(12,12) 实际穿过 lineD（y=10 与 y=x 交于 (10,10) 在段内）——凭直觉断言 `!has(4)` 即错；Scale 场景 changes 里圆元素没有 pts[1]，误用即 UB。断言必须对照场景逐条验算。

---

## 6. 验证结果

| 项 | 结果 |
|----|------|
| 自测 | **56 / 0**（基线 54 → +2：t_shapeselect、t_gripops） |
| t_shapeselect | 三模式 modeName；fence 命中 line/circle/poly/lineD、不中远线；wp 仅圆+点；cp 五图元；退化形状三断言；segments 圆 24 段/直线 1 段 |
| t_gripops | nextMode 五循环；Move/Rotate/Scale/Mirror/Stretch 几何终值；Scale 非正拒绝；Mirror 退化拒绝；Stretch 端点迁移+他端不动 |
| 构建 | SketchCAD / SketchSelfTest / SamplePlugin / SketchCADAutomation / AbiProbePlugin 全目标 EXIT=0，`-Wall -Wextra` 零警告 |
| 交互回归 | 旧「拖完就走」手感保留（release 已拖拽即 commit）；Enter/Esc/双击/右键链路无冲突；工具切换自动 cancel 热态 |
