# 对象捕捉（OSNAP）与约束求解自动生成 — 设计深度分析

> 本文档记录 SketchCAD 在既有「端点 / 圆心 / 交点 / 切线 / 中点」捕捉与「切 / 垂直 / 平行」约束求解基础上，
> 扩展 9 种新捕捉类型与 3 类几何关系自动生成的设计思考过程、关键决策与优先级判定逻辑。
> 对应源码：`src/core/Snap.{h,cpp}`、`src/core/ConstraintSolver.{h,cpp}`、`src/core/Commands.{h,cpp}`、`src/ui/GraphicsView.cpp`。

---

## 1. 需求拆解

原需求可拆为两条主线，外加一个隐含前提：

1. **捕捉类型扩展**：在既有 5 种基础上新增
   - 象限点（Quadrant）、垂足（Perpendicular）、插入点（Insertion）、节点（Node）、
     最近点（Nearest）、外观交点（Apparent）、平行（Parallel）、延伸（Extension）、两点间中点（MidBetween）。
2. **约束关系自动生成**：绘制垂线、公切线、象限连接等几何关系，由求解器自动计算并生成对应图元。
3. **隐含前提**：新增类型必须与既有 `Snap` 模块接口、行为、优先级逻辑保持一致；并"正确处理捕捉优先级的判定逻辑"。
4. **交付物**：把上述深思过程导出本文档。

---

## 2. 既有架构回顾（设计约束边界）

- **`Snap` 模块**（`src/core/Snap.cpp`）：纯几何层，把"鼠标附近可吸附的特征点"算清楚，UI 只负责画标记、替换拾取点。
  对外唯一入口 `snap(elems, w, otherEnd, tol, cfg)`，其中 `otherEnd` 用于切线/平行推导，`cfg` 是各类开关。
- **`SnapType` 枚举 + `SnapConfig` 开关**：每种捕捉一个枚举值，一个 `bool` 开关；绘制命令进入时按工具类型载入默认开关，期间由快捷键切换，退出自动复原。
- **`ConstraintSolver`**（`src/core/ConstraintSolver.cpp`）：基于 `geo::checkConstraint / restoreConstraint` 在图元参数改后检测并恢复约束（Tangent / Perpendicular / Parallel）。
- **ElementType 枚举「只增不改」纪律**：新增图元类型必须追加到末尾，序列化按整数索引，重排会破坏旧档。

这两条主线都必须"贴着既有接口长"，不能另起炉灶。

---

## 3. 对象捕捉扩展设计

### 3.1 类型语义与几何定义

| 类型 | 几何含义 | 关键算法 |
|------|----------|----------|
| Quadrant 象限点 | 圆/弧 0°/90°/180°/270° 方位点 | `quadrantPoints()`：方位角转点，弧按 `angleInSweep` 过滤 |
| Perpendicular 垂足 | 由光标向图元作垂线、垂足落在线段/弧段内 | `perpendicularFoot()`：带参数 `t` 的段投影，要求 `1e-6 < t < 1-1e-6`，端点交给 Endpoint |
| Insertion 插入点 | 块引用/文字/多行文字/标注的 `pts[0]` | 直接取锚点 |
| Node 节点 | `Point` 图元的几何锚点 | 见 §6 点实体支撑 |
| Nearest 最近点 | 图元上离光标最近的点（兜底） | `nearestPointOnElement()`：段投影 / 圆径向 / 椭圆采样 |
| Apparent 外观交点 | 两图元无限延长后相交、但线段本身不相交处 | `segEnds()` 取方向段 + `lineLineIntersect` / `lineCircleAll` |
| Parallel 平行 | 光标在某条"过首点且平行于参照"的直线上的投影 | `math::projPointLine`，`dir` 携带被吸附方向 |
| Extension 延伸 | 直线/弧端点沿其方向延长射线上的点 | 段投影参数 `t > 1e-3` |
| MidBetween 两点间中点 | 两个参考点的中点 | 需 `refA` / `refB` 两个参考点 |

### 3.2 优先级判定逻辑（核心）

这是需求里最强调的一点。"当一个光标同时落入多种捕捉的容差范围时，吸谁？"——这是 AutoCAD 式 OSNAP 的体验关键。

设计决策：**优先级数值 + 距离二级比较**，而非"谁先被遍历到谁赢"。

- 数值越大越优先。分层（见 `snapPriority()`）：
  - **100 — 显式点状**：Node / Insertion / Quadrant（语义最明确，压在最上）
  - **95 — 端点/圆心**：Endpoint / Center
  - **90 — 严格交点**：Intersection
  - **85 — 外观交点**：Apparent（线段不相交，弱于真交点）
  - **80 — 需推导的切/垂**：Tangent / Perpendicular
  - **70 — 依赖上下文推导**：Extension / Parallel
  - **60 — 中点系**：Midpoint / MidBetween
  - **10 — 兜底**：Nearest
  - **-1 — 无**：None
- 比较规则（主循环 `consider` lambda）：`if (r > bestRank || (r == bestRank && d < bestD))`。
  即**先比优先级，同级再比几何距离**。这让"光标压在端点上"时优先吸端点而非顺带的最近点，符合直觉。

**为什么不能简单用遍历顺序？**
旧实现用 `bestD` 单一距离比较，等价于"所有类型同优先级、谁近谁赢"。这在 Node/象限/插入点这类"明确点"与 Nearest 这类"兜底点"共存时会翻车——光标离一个最近点 0.3px、离一个端点 0.5px，结果吸了最近点而非端点，用户感知是"捕捉乱跳"。引入 `bestRank` 后，点状捕捉天然压过兜底，行为可预测。

### 3.3 `snap()` 主流程（11 步）

主函数不再是一长串 if-else，而是 11 个有序 `consider` 阶段，每阶段只在其开关开启时才参与竞争：

1. 端点 / 圆心 / 插入点 / 节点
2. 象限点
3. 单段中点
4. 两点间中点（需 `refA`/`refB`）
5. 交点（粗筛 `broad = tol*6` 避免 O(n²) 全量求交）
6. 切点（需 `otherEnd`）
7. 垂足
8. 最近点（兜底）
9. 外观交点（真相交的交给 Intersection，不重复）
10. 延伸
11. 平行（需 `otherEnd`，`dir` 携带方向）

`consider` 内部统一做容差过滤 + 优先级/距离竞争，保证"优先级逻辑"集中在一处、不会因分散而漂移。

### 3.4 几何算法要点与坑

- **Perpendicular 严格落段内**：`t` 落在 `(1e-6, 1-1e-6)`，把端点情形完全让给 Endpoint，避免两种捕捉抢同一点。
- **Apparent 与 Intersection 互斥**：外观交点分支先判断 `intersectElements(A,B)` 非空则跳过，真相交交给 Intersection，避免重复吸附。
- **Apparent 直线×圆**：用 `lineCircleAll`（无限直线 × 整圆）而非线段求交，即使线段本身不穿圆也能给出"延长后相交"的虚交点。
- **Parallel 方向携带**：`SnapResult.dir` 存单位方向向量（Z=0）。绘制命令里，被吸附点本就落在"过首点且平行于参照"的直线上，所以画出的线自动平行；`dir` 额外用于光标标记指示线，让平行方位可见。
- **Nearest 的 `p != w` 防护**：原先用 `p != w` 判断是否命中，但 `Vec3` 未重载 `operator!=`，改为 `p.distTo(w) > 1e-9`，否则编译失败（本次真实踩坑）。

---

## 4. 约束求解自动生成设计

目标：不仅"检测并恢复"约束，还要能"主动算出并生成"垂线、公切线、象限连接等几何关系。

### 4.1 三个生成器（均已绑定 `ctype` / `refA` / `refB`）

- **`drawPerpendicularTo(refLine, P)`**：过 `P` 作一条垂直于 `refLine` 的直线，`dir.perp()` 为垂线方向，`P` 为中点取单位长。`ctype = Perpendicular`，`refA = refLine.id`。
- **`commonTangents(cA, cB, out)`**：返回 0..4 条公切线。
  - 外切线：`cosα = (ra − rb)/D`，两切点半径同向；
  - 内切线：`cosα = (ra + rb)/D`，两切点半径反向（B 侧取 `ang + π`）；
  - 相交/包含/同心 → 返回 0。每条 `ctype = Tangent`，`refA/refB` 记录两圆。
- **`quadrantConnector(target, from)`**：从 `from` 连到 `target` 圆/弧最近的象限点；非圆类退化连 `pts[0]`。`ctype = Coincident`，`refA = target.id`。

### 4.2 与求解器维护范围的衔接（关键决策）

生成的图元**不是孤立的**——它们带 `ctype`/`refA`/`refB`，因此进入 `ConstraintSolver::detectConstraints` 的维护范围。
后续用户移动端点或参照图元时，求解器会因为几何关系仍成立而**自动维持**该垂线/公切线/象限连接，无需命令方手动重算。
这正是"约束求解模块能够自动计算并生成几何关系"的落点：生成即纳入约束系统。

---

## 5. UI 接线

- **快捷键扩展**：原 `defs[5]`（E/T/M/C/X）扩展为 `defs[]`（14 项），新增 Q=象限 / P=垂足 / I=插入 / N=节点 / A=最近 / O=外观 / L=平行 / W=延伸 / B=两点中点。
  这些键在绘制命令期间启用，并经由 `saveConflictingShortcuts()` 把同键的工具快捷键临时禁用，消除"按 Q 被误判为某工具"的冲突。
- **`snapWorld` / `refreshSnapAtCursor` 传 `refA`/`refB`**：仅当 `cfg.midBetween` 且已放置 ≥2 点时，取最近两点作为参考点，驱动 MidBetween（"两点间中点"）。
- **光标标记**：`switch(snapNow_.type)` 补全部新类型配色与形状（节点=实心小圆、象限=菱形、平行=方框+方向指示线、两点中点=方框+中心点、其余=方框），并把 `snapNow_.dir` 画成短指示线直观表达平行方位。
- **HUD**：在状态栏新增"捕捉开关②"一行，实时显示 9 个新开关的 ✓/✗ 状态。

---

## 6. 点实体支撑（Node 可落地的关键）

Node 捕捉需要"点"图元存在。既有 `ElementType` 没有 Point，故：

- 新增 `ElementType::Point`（枚举末尾追加，=17，遵守"只增不改"纪律）。
- `elementTypeName` 加"点"；`sizeDesc` 加 Point 分支；序列化走通用 `pts` 字段，无需特例。
- **渲染**（`paintOne`）：屏幕恒定 3.5px 的圆 + 十字准星，任何缩放下可见。
- **拾取**（`distanceToElement` / `buildPath`）：`distanceToElement` 返回到 `pts[0]` 的距离；`buildPath` 显式 `break`（空路径，避免 `-Wswitch` 警告），实际拾取由 `distanceToElement` 兜底。
- **创建命令**：新增 `point <x,y> [x,y ...]` 命令行，批量创建点实体，使 Node 捕捉真正可用。

---

## 7. 命令行入口（自动生成的可触发点）

在既有命令注册表加 4 条：

- `point x,y [x,y...]` — 创建点实体（Node 捕捉来源）
- `perp <直线Id> <x,y>` — 过指定点作垂线（调用 `drawPerpendicularTo`）
- `tangent2 <圆IdA> <圆IdB>` — 求两圆/弧全部公切线（调用 `commonTangents`）
- `qconnect <目标圆Id> <x,y>` — 从指定点连到目标圆最近象限点（调用 `quadrantConnector`）

三者生成的图元经 `model->addElement` 入库，并自动带有 `ctype`/`refA`/`refB`，进入约束维护范围。

---

## 8. 决策与权衡小结

1. **优先级数值化而非遍历顺序**：解决"捕捉乱跳"，行为可预测（§3.2）。
2. **`consider` 集中竞争逻辑**：优先级判定只有一处，便于一致与维护。
3. **生成器绑定 `ctype`/`refA`/`refB`**：自动生成 ≠ 一次性几何，而是纳入约束系统持续维护（§4.2）。
4. **Parallel 几何自动平行 + `dir` 仅用于指示**：绘制命令无需特殊分支即可得到平行结果，降低耦合。
5. **新增 Point 图元支撑 Node**：让"节点"捕捉从概念变为可创建、可拾取、可吸附的真实能力（§6）。

---

## 9. 验证与遗留

- **编译**：`-Wall -Wextra`、`-std=gnu++17`，无 `-Werror`；新增枚举产生的 `-Wswitch` 已在 `buildPath`/`distanceToElement`/`snapPriority` 等处显式覆盖或已有 `default`，不阻断构建。
- **行为**：新快捷键、HUD、光标标记、命令行入口均已接线；`perp/tangent2/qconnect` 生成的图元带约束元数据。
- **遗留 / 后续可增强**：
  - MidBetween 的 `refA`/`refB` 当前取"最近放置的两点"，更完整的实现可支持交互式指定两个任意参考实体（需小型状态机）。
  - 点实体的 DXF 导出（POINT 图元）尚未实现，当前仅内存与 JSON 工程文件支持。
  - 平行/延伸的"参照图元"目前取光标附近首个匹配图元，可增强为按优先级选择多条参照。
