# 对象捕捉与几何约束求解扩展 — 设计分析

> 对应需求：在保持既有接口风格、目录结构与行为兼容的前提下，完善对象捕捉（OSNAP）与几何约束求解模块：
> 1) 规范 8 类捕捉的优先级与吸附判定；2) 约束类型由 11 种扩展到 **21 种**（>20）；
> 3) 扩展约束方程与雅可比矩阵构建，支持冗余/冲突检测、自由度(DOF)统计；4) 统一注册、补充单元测试、支持撤销/重做。

---

## 1. 边界与现状分析

| 维度 | 现状（扩展前） | 扩展后 |
|------|----------------|--------|
| 约束类型 | 11 种（Coincident/Parallel/Perpendicular/Equal/Tangent/Horizontal/Vertical/Fixed/Concentric/Symmetric/Dimensional） | **21 种**（新增 10 种） |
| 求解策略 | 逐类解析法 `applyOne`（硬编码） | 统一**数值求解**（有限差分雅可比 + Levenberg-Marquardt），解析法降为兜底 |
| 自由度/冗余 | 无 | 雅可比秩 → DOF；行删除判定冗余；残差超容差判定冲突/过约束 |
| 对象捕捉 | 8 类（端点/中点/圆心/交点/垂足/切点/象限/最近）已存在 | 不变，新增 `snapTypeDescriptors()` 统一注册与优先级/吸附规范文档 |
| 单元测试 | 无 `tests/` 目录 | 新增 `tests/SketchSelfTest.cpp` + `SketchSelfTest` 目标（21 类约束每类 ≥1 用例 + 8 类捕捉命中） |

**关键结论**：对象捕捉的 8 类类型、`SnapType` 枚举、`snapPriority()`、`snapFieldDefs()` 均已齐备，**无需新增捕捉类型**，只需规范化文档并补统一注册。约束侧则需新增类型、统一注册、并引入数值求解层。

---

## 2. 关键设计决策

1. **枚举「只增不改」**：`ConstraintType` 在既有 11 值末尾**追加** 10 个新值（`kConstraintTypeCount = 21`）。序列化按整数序存档，旧档读回不串类型。
2. **统一数值求解框架**：每种约束只实现一个**残差函数** `r(x)`，由求解器统一做有限差分求雅可比、解正规方程。新增约束类型只需补残差分支，**不用再手写解析解**（解析 `applyOne` 仅作降维/兜底与 `addConstraint` 即时预检）。
3. **变量映射**：以「图元的可优化分量」为决策变量 —— 点 `x`/`y`、圆/弧 `radius`。仅在被任一约束引用、且未被 `Fixed`/`FixedPoint` 锁定的分量才进入变量集，自由变量最小化。
4. **自由度(DOF)**：对 `(JᵀJ)` 对称矩阵做 Jacobi 特征值分解，零空间维数 = 变量数 − 有效秩 = DOF；用于判断系统是否过约束或欠约束。
5. **双层冲突检测**：
   - *解析规则层* `detectConflicts()`：同对互斥（平行∧垂直、水平∧竖直）、同心∧相切、重复约束、尺寸类目标值不一致、同一图元被要求水平又竖直、Fixed 图元当被动方等。
   - *数值层*：求解后残差仍超容差 → 记为 `infeasible`；删除某约束整组残差后秩不变 → 记为 `redundant`。
6. **撤销/重做复用既有管线**：`addConstraint`/`solveAll` 都走 `beginEdit` + `updateElementQuiet`，写入受 `Memento`（上限 200）管控，无需新增撤销逻辑。

---

## 3. 模块拆分与代码落点

| 文件 | 变更 |
|------|------|
| `src/core/Element.h` | `ConstraintType` 枚举追加 10 值 + `kConstraintTypeCount`；`constraintTypeName()` 覆盖 21 种 |
| `src/core/ConstraintSolver.h` | `ConstraintRecord` 增 `idxA/idxB`；新增 `SolveReport::NumericDiagnostics`、`ConstraintDescriptor`、`SolveOptions`、`SolveDiagnostics`、`VarKey`；声明 `solveAllNumeric`/`constraintDescriptors` |
| `src/core/ConstraintSolver.cpp` | 新增数值求解器（LM + 有限差分雅可比 + DOF/冗余/冲突）、`constraintDescriptors()`、`checkOne`/`applyOne`/`compatError` 覆盖新类型；`solveAll` 改为委托数值求解；`detectConflicts` 扩展 |
| `src/core/SketchModel.cpp` | `constraintsToJson`/`setConstraintsFromJson` 持久化 `idxA/idxB`（旧档缺键回落 -1） |
| `src/core/Commands.cpp` | `constraint` 命令新增 10 类型名、`help` 子命令、`solve` 输出 DOF/冗余、`add` 支持 `idxA/idxB` 与 `fixed/fixedpoint` 省略 idB |
| `src/core/Snap.h` / `Snap.cpp` | 新增 `SnapDescriptor`/`snapTypeDescriptors()` 统一注册 14 种非 None 捕捉 |
| `CMakeLists.txt` | 新增 `SketchSelfTest` 可执行目标（复用 `CORE_SOURCES`，不链接 UI） |
| `tests/SketchSelfTest.cpp` | 21 类约束用例 + 8 类捕捉命中 + 冲突/注册表校验 |

---

## 4. 雅可比与 Levenberg-Marquardt 设计

### 4.1 残差函数（每类约束 → 残差向量）
| 约束 | 残差（降维数） | 说明 |
|------|----------------|------|
| Coincident | 2：`A.p − B.p` | 指定控制点重合（默认 idx 0） |
| Parallel / Collinear | 1：`da×db`（叉积） | 平行；共线额外 + `signedDist(B.p0, lineA)` |
| Perpendicular | 1：`cos(angle) = (da·db)/(|da|·|db|)`（目标 0，**全区间光滑**） | 垂直（两方向点积，归一化） |
| Equal / EqualLength / EqualRadius / Dimensional | 1：`size(B) − target` | 长度/半径/目标值 |
| Tangent | 1：线–圆 `(d²−r²)`；圆–圆 `D − (rA±rB)` | 分支按初始内外切选择 |
| Horizontal / Vertical | 1：`Δy` / `Δx` | |
| Concentric | 2：`ca − cb` | 圆心重合 |
| Symmetric | 2×点数：`B.pᵢ − mirror(A.pᵢ, axis)` | |
| PointOnLine | 1：`signedDist(P, lineB)` | |
| PointOnCircle | 1：`dist(P, centerB) − rB` | |
| FixedPoint | 2：`P − anchor`（求解起始锚定） | 锁定指定控制点 |
| Midpoint | 2：`P − mid(B)` | |
| Distance | 1：`dist(anchorA, anchorB) − value` | |
| Angle | 1：`cos(angle) − cos(value)`（归一化方向点积减目标余弦，°→rad，**全区间光滑**） | |
| SymmetricPoint | 2：`midpointOnAxis` + `AB⟂axis` | |

### 4.2 数值流程
1. 由约束表收集变量 `x₀`（工作副本 `work = m.elements()`，id→index 映射）。
2. 每轮：有限差分构造雅可比 `J (R×V)`（中心差分，步长 `h = step·max(1,|xₖ|)`）；正规方程 `A = JᵀJ + λI`，`g = Jᵀr`；高斯消元解 `dx`。
3. LM 阻尼：接受步长则 `λ←λ/10`，否则 `λ←10λ`，直至残差二范数 `< tol` 或达 `maxIterations`（默认 40）。
4. 收敛后写回 `work` → `updateElementQuiet`；按残差重算每条约束 `satisfied`，`setConstraints` 持久化。
5. 诊断：对终态 `J` 求 `(JᵀJ)` 特征值 → 有效秩、DOF；逐约束删除其残差行后秩不变 → 冗余；残差超容差 → 不可行（冲突/过约束）。

> 性能：变量数 `V` 与引用图元数成正比，典型草图 `V` 在数十~数百；每次迭代开销 `O(R·V)` 残差 + `O(V³)` 解正规方程。大规模草图建议约束局部化；`maxIterations`/步长可在 `SolveOptions` 调参。

### 4.3 角度类约束的「平行退化」与预旋转

**Perpendicular / Angle 残差必须采用光滑的 `cos(angle)` 形式，不能用 `angleBetweenLinesDeg`（锐角）形式。** 原因如下：

- **起点退化**：两直线「恰好平行」时，任意夹角函数 `f(θ)` 关于相对转角 `θ` 都是偶函数（`f(θ)=f(−θ)`），故在 `θ=0`（平行）处一阶导数为 0 → 有限差分雅可比在该点为 0，LM 无从决定旋转方向，**直接卡死**。
- **目标尖点**：`angleBetweenLinesDeg = atan2(|cross|,|dot|)` 在 90° 处存在不可导尖点，逼近 90° 时中央差分梯度再次归零，残差停在 `≈1°` 处无法收敛到 0。
- **光滑解**：采用归一化方向点积 `cos(angle) = (da·db)/(|da|·|db|)`，其全区间光滑、在目标角处梯度非零（`−sin(angle)`），既无起点退化也无目标尖点，LM 稳定收敛。

**预旋转（打破对称性）**：即便残差光滑，若被动直线初始就与基准直线平行（测试中常见），仍会在 `θ=0` 处零梯度卡死。因此在 `addConstraint` 内部、进入数值求解**之前**，对「近似平行（`|cross| < 1e-6·|da|·|db|`）」的被动直线绕其首点预旋转约 3°，人为制造非平行初值；最终仍收敛到目标角，用户无感。该 3° 扰动被同一 `beginEdit` 撤销步覆盖，不单独入撤销栈。

---

## 5. 对象捕捉规范（优先级与吸附判定）

`solve`/绘制命令中的吸附采用「**优先级 + 距离**」双因子竞争：同一容差内先比 `snapPriority()`（数值大者胜），同优先级再比到光标距离。

| 优先级 | 捕捉类型 | 吸附判定要点 |
|--------|----------|--------------|
| 100 | Node / Insertion / Quadrant | 点图元节点 / 插入锚点 / 圆四方位象限点 |
| 95 | Center / Endpoint | 圆心 / 端点顶点 |
| 90 | Intersection | 两图元真实相交点 |
| 85 | Apparent | 延长线/投影相交（本体不相交） |
| 80 | Tangent / Perpendicular | 由另一端作切线切点 / 垂足 |
| 70 | Extension / Parallel | 端点延长线 / 平行投影方位 |
| 60 | Midpoint / MidBetween | 单段中点 / 两参考点中点 |
| 10 | Nearest | 图元上最近点（兜底） |

新增 `snapTypeDescriptors()` 将上述 14 种非 None 捕捉集中注册（机器标识、中文名、适用图元、优先级、吸附说明），与 `snapFieldDefs()` 形成「开关字段」与「类型语义」双表，杜绝名称漂移；`constraint help` 同源列出全部 21 种约束。

---

## 6. 冗余与冲突检测

- **过约束**：独立约束数（雅可比有效秩）超过变量数 → 必有约束无法满足（`infeasible`）。
- **冗余约束**：删除该约束全部残差行后有效秩不变 → 该约束可由其它约束推出，可删除以简化系统（`redundant`）。
- **解析层冲突**（`detectConflicts`）：平行∧垂直、水平∧竖直、同心∧相切、同对重复、同一图元既水平又竖直、Fixed 当被动方、Dimensional/Equal/EqualLength/EqualRadius 施加不一的 Dimensional 目标值、同一对 Distance/Angle 目标值相异 —— 均给出可定位中文提示。
- `constraint check` 输出解析层问题；`constraint solve` 额外输出 DOF、有效秩、冗余下标。

### 6.1 逐实体自由度与欠/过约束诊断（diagnose）

在全局 DOF（`V − rank`）与约束下标级 redundant/infeasible 之上，新增**图元级**定位能力：

**接口（ConstraintSolver.h）**

```cpp
struct ElementConstraintInfo {
    int  id;             // 图元 id
    ElementType type;
    int  naturalDof;     // 固有几何自由度（naturalDof(e)：点=2、线=4、圆/弧=3、多点图元=2×点数、
                         //   椭圆=5、构造线=3、Hatch=2n+2、文本/表格/块引用=2、约束/标注=0）
    int  activeVars;     // 纳入求解器的可控自由度（被约束引用且未被 Fixed/FixedPoint 锁定）
    int  appliedRank;    // 作用于该图元的独立约束条件数 = 雅可比「该图元列块」的秩
    int  remainingDof;   // 剩余自由度 = activeVars − appliedRank（activeVars=0 时取 naturalDof）
    bool overconstrained;// 参与「解不动」冲突约束，或（冗余且自由度已用尽）
    std::vector<int> constraints;   // 涉及该图元的约束下标
};

struct ConstraintDiagnosis {
    enum SystemState { WellConstrained, UnderConstrained, OverConstrained, Conflicting, Mixed };
    SystemState state;        QString stateText() const;
    int totalDof;             // Σ naturalDof（全部几何图元）
    int freeDof;              // Σ remainingDof（被约束引用的图元）
    int overDof;              // infeasible + redundant 条数
    std::vector<ElementConstraintInfo> elementInfo;
    std::vector<int> underconstrainedIds, overconstrainedIds;
    std::vector<QString> messages;      // 人类可读提示（含图元定位）
};

static int                naturalDof(const Element& e);
static ConstraintDiagnosis diagnose(const SketchModel& m);   // 只读，不改模型
```

**实现要点**

- 核心重构：数值求解体从 `solveAllNumeric` 提取为匿名命名空间的 `numericAnalyze(m*, work, recs, writeBack, opt)`，返回 `NumericResult`（全局诊断 + elementInfo）。`solveAllNumeric` = 写回模式包装；`diagnose` = 在模型数据**副本**上跑同一核心（`writeBack=false`、`m=nullptr`），保证 `const SketchModel&` 只读契约。
- 逐实体秩：对该图元的雅可比列块 `J[:, cols]` 计算 `M = J_blkᵀ·J_blk`（av×av），`appliedRank = rankOfSymmetric(M)`，即「有多少独立约束条件压在该图元的可控参数上」。该定义自动处理耦合约束（如 Coincident 同时计入两端图元的秩）。
- 冲突定位：参与 `infeasibleIdx` 约束的图元直接进 `overconstrainedIds`（其上条件数已达/超自由度仍不满足）；仅冗余且 `remainingDof==0` 也判过约束。
- 状态判定：infeasible∧under → `Mixed`；infeasible → `Conflicting`；redundant → `OverConstrained`；under → `UnderConstrained`；否则 `WellConstrained`。
- 语义说明：约束的**主动方**（参照图元）同样纳入变量集且列块秩为 0 → 会被如实报告为「完全自由」。这与真实 CAD（如 Onshape 草图 DOF）一致：参照线确实不受该约束限制。
- 命令入口：`constraint diagnose`（别名 `dof`）输出系统状态、总/剩余/冲突计数、逐图元「固有/可控/已约束/剩余」四元组与提示列表。

---

## 7. 撤销/重做

`addConstraint` 与 `solveAll` 均在 `beginEdit` 下经 `updateElementQuiet` 写入，受既有 `Memento`/`undoStack_`/`redoStack_`（上限 200）管控，**无需新增撤销逻辑**，与拖动编辑、属性修改的撤销体验一致。

---

## 8. 新增约束使用方式（命令）

```
constraint add <类型> <idA> <idB> [值|轴id] [idxA] [idxB]   # fixed/fixedpoint 可省略 idB
constraint solve            # 求解全部并打印 DOF/冗余
constraint check            # 解析层冲突检测
constraint diagnose         # 逐实体自由度与欠/过约束诊断（只读，别名 dof）
constraint list             # 列出全部约束及满足态
constraint help             # 列出 21 种约束（来自统一注册表）
```

| 新增类型 | 命令字 | 主要参数 | 适用 | 局限 |
|----------|--------|----------|------|------|
| Collinear 共线 | `collinear` | — | 两直线 | 同时约束平行 + 共点，与 Parallel 部分相关 |
| EqualLength 等长 | `equallength` | — | 两线段 | 仅长度，不管方向 |
| EqualRadius 等半径 | `equalradius` | — | 两圆/弧 | 仅半径 |
| PointOnLine 点在线上 | `pointonline` | idxA | 点↔直线 | 取指定控制点 |
| PointOnCircle 点在圆/弧上 | `pointoncircle` | idxA | 点↔圆/弧 | 取指定控制点 |
| FixedPoint 固定点 | `fixedpoint` | idxA | 任意有点图元 | 锁定该控制点当前位置 |
| Midpoint 中点 | `midpoint` | idxA | 点↔线段 | 取指定控制点为中点 |
| Distance 距离 | `distance` | value | 两点/锚点 | 取 idxA/idxB 锚点，欧氏距离 |
| Angle 角度 | `angle` | value(°) | 两直线 | 取夹角绝对值 [0,180] |
| SymmetricPoint 点对称 | `symmetricpoint` | axisId, idxA/idxB | 两点↔轴 | 两点关于轴互成镜像 |

> 既有类型 `coincident/parallel/perpendicular/equal/tangent/horizontal/vertical/fixed/concentric/symmetric/dimensional` 用法不变。

---

## 9. 验证

```
build.sh test            # 内部构建并运行 SketchSelfTest
# 或
cmake --build build --target SketchSelfTest && ./build/SketchSelfTest
```
`tests/SketchSelfTest.cpp` 覆盖：21 类约束每类 ≥1 通过用例（`satisfied` 为真）、8 类捕捉命中、互斥冲突检测、DOF 收敛、约束注册表计数（`== kConstraintTypeCount`）、捕捉注册表计数（`== 14`）、以及逐实体诊断 4 例（`naturalDof` 取值、欠约束定位（水平线剩余 3 DOF）、完全约束（双 FixedPoint）、冲突定位（同线双 Dimensional 矛盾值））。全部通过则退出码 0（当前 37/0）。

---

## 10. 已知局限

- **相切分支**：圆–圆相切按求解起始的内/外切几何判断，若初始即跨分支可能无法跳变到另一支；建议初始配置接近目标相切。
- **有限差分代价**：极大草图（数百变量）下每次迭代 `O(V³)` 解正规方程偏重，必要时可限制单命令求解规模或改用稀疏求解。
- **多解约束**：Angle/Collinear/SymmetricPoint 等存在对称解，数值法收敛到其中一支（依赖初值），非全局唯一。
- **FixedPoint 语义**：把指定点锚定在「求解起始时的当前位置」；若先移动了其它约束再求解，该点保持其在求解起点处的坐标，符合常见 CAD 行为。
- **DOF 阈值**：秩判定用 `1e-6·maxDiag` 相对阈值，病态比例系统下可能有 1 级误判，属工程可接受范围。
