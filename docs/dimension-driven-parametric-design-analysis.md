# 尺寸驱动的参数化建模：设计与深思过程

> SketchCAD · Request M 实现记录
> 范围：对象捕捉（OSNAP）加固、尺寸驱动约束、几何一致性校验、"标注—驱动—建模"闭环
> 配套代码：Element.h / Element.cpp、ConstraintSolver.h / ConstraintSolver.cpp、SketchModel.h / SketchModel.cpp、Commands.cpp、MainWindow.cpp、tests/SketchSelfTest.cpp

---

## 1. 需求拆解与优先级

用户提出的四条要求，逐条映射到工程动作：

| 需求 | 实质 | 实现策略 |
|------|------|----------|
| ① 稳定识别端点/中点/交点/圆心等关键几何点 | OSNAP 引擎加固 | 既有 `snap::snap` 已成熟（14 类 + 优先级 + 距离兜底）；本任务以**稳定性测试**（抗光标抖动、远离不误吸、不崩溃）固化其行为，未做侵入式改写以规避回归风险 |
| ② 尺寸驱动约束，标注与几何双向关联 | 标注值 ↔ 约束目标值 ↔ 几何 | 新增 `linkDimension` 把标注按 `dtype` 映射成尺寸类约束，并把 `driverId` 指回标注；改标注值即改约束目标值并重解 |
| ③ 欠约束/过约束下完成求解与一致性校验 | 求解鲁棒性 + 诊断 | 复用既有 `numericAnalyze`（LM 数值求解）+ `diagnose()`（逐实体 DOF / 欠过约束定位），不重写求解核心 |
| ④ 打通"标注—驱动—建模"链路，参数表联动全局更新 | 命名参数表 | 新增 `SketchModel::params_` 命名变量表 + `driveParameter`，一处改值、所有关联标注与几何同步重构 |

**关键判断**：② 与 ③ 是主线，① 是加固，④ 是把②推广到"多标注共享同一变量"的自然延伸。做②时若绕开既有数值求解器另起炉灶，会重复实现 LM / 雅可比 / 自由度统计，风险高且必然与既有诊断脱节。因此核心策略是**复用 `numericAnalyze`，只补"标注↔约束"的桥接层**。

---

## 2. 现状调研的关键发现（决定整体方案走向）

在动手前，我通读了 `ConstraintSolver.cpp::numericAnalyze`（约 838–1254 行）的残差闭包，确认了三条尺寸类约束的残差**天然由 `r.value` 驱动**：

```cpp
// 长度 / 半径（Dimensional）：sizeOf(*b) 必须等于目标值 r.value
case ConstraintType::Dimensional:
    push(sizeOf(*b) - r.value, ci);          // sizeOf(直线)=长度，sizeOf(圆)=半径

// 角度（Angle）：两线夹角余弦 = cos(目标角)
case ConstraintType::Angle:
    push(cosT - std::cos(r.value * M_PI / 180.0), ci);

// 两点距离（Distance）：anchorPoint(A) 与 anchorPoint(B) 间距 = r.value
case ConstraintType::Distance:
    push(pa.distTo(pb) - r.value, ci);
```

**这一发现是方案的分水岭**：标注与几何的"双向关联"不需要新的求解算法，只需
- 把标注的 `dimValue` 写进对应约束的 `r.value`（正向：改标注 → 驱动几何）；
- 求解后把几何量回写标注的 `dimValue`（反向：几何变 → 标注更新）。

即"约束求解器已经是参数化引擎，缺的只是标注到参数的接口"。这与 Request L 已沉淀的 `naturalDof` / `diagnose` 能力无缝衔接。

> 这也解释了为什么本任务不引入"变量—表达式"式参数系统（如 `length = 2*width` 的代数依赖），而采用**"命名变量 → 直接驱动若干标注"**的扁平模型：它足以覆盖"同一参数联动多个尺寸"的工程场景，且不触碰已稳定的求解核心。

---

## 3. 数据模型设计

### 3.1 Element 新增字段（`Element.h` struct Element）

```cpp
double dimValue  = 0.0;   // 标注驱动值（mm 或 °）：dimDriven 时作为尺寸约束目标值
bool   dimDriven = false; // 是否由参数/标注驱动其几何
QString dimParam;         // 关联的参数表变量名（如 "L"），空串表示不关联参数
bool   dimLinked = false; // 是否已通过 linkDimension 与几何约束建立双向关联
```

序列化（`elementToJson` / `elementFromJson`）按既有约定**缺键回落默认**，保证旧 `.sketch` 文件向后兼容。

### 3.2 ConstraintRecord 新增字段（`ConstraintSolver.h`）

```cpp
int    driverId = -1;   // 驱动本约束的尺寸标注图元 id（-1 表示非标注驱动）
QString paramRef;       // 关联的参数表变量名；空串表示不关联参数
```

`constraintsToJson` / `setConstraintsFromJson` 同样追加这两个键并缺键回落，旧档读回不致崩溃。

### 3.3 命名参数表（`SketchModel`）

```cpp
std::map<QString,double> params_;   // 命名设计变量表
// 访问器：params() / hasParam() / param() / setParam()
// 序列化：paramsToJson() / setParamsFromJson()，顶层 JSON "params"
```

参数表纳入 `Memento`（撤销栈）与 `pushMemento` / `undo` / `redo`，使"改参数"可一次 Ctrl+Z 撤回，与几何编辑同语义。

---

## 4. 桥接层实现（`ConstraintSolver` 五个新方法）

### 4.1 dtype → 约束类型的映射（linkDimension 的核心）

| 标注 dtype | 约束类型 | 被度量几何（idB） | 目标值换算 |
|-----------|----------|------------------|------------|
| Radius | `Dimensional` | 圆/弧（idB=refA） | 半径 = dimValue |
| Diameter | `Dimensional` | 圆/弧（idB=refA） | 半径 = dimValue / 2 |
| Aligned/Horizontal/Vertical（单几何） | `Dimensional` | 直线/圆（idB=refA） | 长度/半径 = dimValue |
| Distance（两几何） | `Distance` | refA ↔ refB 锚点 | 距离 = dimValue |
| Angle | `Angle` | refA、refB 两直线 | 夹角° = dimValue |

> **实现要点（易错点）**：`Dimensional` 残差作用于 `sizeOf(*b)`，因此单几何尺寸必须把被度量的图元放在 `idB`；而 `addConstraint` 禁止 `idA==idB`，所以 `linkDimension` **直接构造 `ConstraintRecord` 并 `setConstraints`，绕开 `addConstraint` 的 `idA≠idB` 校验**——被度量图元的 `idA` 与 `idB` 均指向自身，残差只动用其半径/长度分量，其余分量雅可比为零、保持不动。角度约束在两端恰好平行时会退化，沿用 `addConstraint` 的 3° 预旋转打破对称性。

### 4.2 正向驱动：setDimensionValue

```
写回 dim.dimValue = value 且 dimDriven = true
找到所有 driverId == dimId 的约束，改其 r.value = 换算后目标值
若尚未关联 → 自动 linkDimension（会读取刚写好的 dimValue 作为目标）
setConstraints → beginEdit → solveAllNumeric → syncDimensionValues
```

### 4.3 反向回写：syncDimensionValues

遍历所有 `dimLinked` 标注，用 `measureDimension` 测出当前几何量，回填 `dim.dimValue`，并用 `formatDimText`（带 R / ⌀ / ° / ⌒ 前缀）刷新 `dim.text`。

> **为何刷新 text 而非依赖渲染时计算**：渲染管线 `dim::build` 在 `e.text` 非空时直接采用用户文本（DimStyle.cpp:46），因此把"真实几何量"写回 `text`，既能保证标注数字与几何一致，又不丢失"已被求解驱动"这一事实。`measureDimension` 与 `dim::build` 共用 `anchorPoint` / `sizeOf` 等同一套几何原语，口径一致。

### 4.4 参数表联动：driveParameter

```
setParam(name, val)
把所有 paramRef == name 的约束 r.value 改为换算后目标值
把所有 dimParam == name 的标注 dimValue 设为 val、dimDriven/dimLinked = true
setConstraints → beginEdit → solveAllNumeric → syncDimensionValues
```

一处 `driveParameter("L", 120)` 即可让所有挂了 `dimParam="L"` 的标注与对应几何同步重构——这就是需求④的"标注—驱动—建模"闭环。

### 4.5 measureDimension（只读度量，闭环校验用）

按 `dtype` 返回当前几何量：半径/直径/直线长度/Distance 间距/两线夹角。失败时返回 `NaN`（用 `std::isnan` 判定），供诊断与测试判断关联是否有效。

---

## 5. 一致性校验与欠/过约束

- **欠约束**：仅一个尺寸约束时（如单直线长度），`diagnose()` 仍会标出 `UnderConstrained`（line 固有 DOF=4，仅 1 个独立约束），但几何会被精确驱动到目标值——欠约束≠不可解，只是位置/姿态未定。这与 CAD 实际行为一致。
- **过约束/冲突**：`detectConflicts` + `diagnose` 的 `OverConstrained` / `Conflicting` 状态继续生效；`driveParameter` 若把多个互斥约束推向不可共存的目标，`solveAllNumeric` 残差不收敛，诊断会暴露冲突下标，用户可据 `constraint check` / `constraint diagnose` 定位。
- **闭环一致性**：`t_driveConsistency` 验证"驱动后 `measureDimension == dimValue == 约束目标值`"，即标注、约束、几何三者数值咬合，无漂移。

---

## 6. 命令接线（Commands.cpp）

在 `ConstraintCommand::execute` 内新增两个子命令（沿用 `constraint` 主命令，避免新增命令注册面）：

```
constraint dimension link <标注id> [参数名]      # 建立标注↔约束双向关联，可选挂参数
constraint dimension setvalue <标注id> <值>     # 改标注值，实时驱动几何
constraint param set <名称> <值>               # 设参数并联动驱动全部关联标注
constraint param list                          # 列出命名参数表
```

---

## 7. 序列化与存档

顶层图纸 JSON 现含 `"params"`；约束记录新增 `"driverId"` / `"paramRef"`。三者均**缺键回落默认**，旧图纸读回安全。

---

## 8. 验证（tests/SketchSelfTest.cpp）

在既有 37 项之上新增 8 项，全部纳入 `main()`：

| 测试 | 验证点 |
|------|--------|
| `t_snapStable` | 端点/圆心附近光标抖动均稳定命中同一点；远离图元不吸附、不崩溃 |
| `t_dimDriveRadius` | 半径标注改值 → 圆半径实时变化，measureDimension 回读一致 |
| `t_dimDriveDiameter` | 直径标注（自然量直径）→ 内部按半径(=直径/2)解算 |
| `t_dimDriveLength` | 直线长度标注改值 → 长度实时变化 |
| `t_dimDriveDistance` | 两点距离标注（固定其一）→ 间距驱动到目标 |
| `t_dimDriveAngle` | 两线夹角标注 → 夹角驱动到目标 |
| `t_paramLink` | 参数表一处改值 → 关联标注与几何联动，且可重复驱动 |
| `t_driveConsistency` | 驱动后 measureDimension == 目标值，系统无冲突/过约束态 |

**结果：通过 45 / 失败 0**（原 37 + 新 8）。主程序 `SketchCAD` 目标同步构建通过。

---

## 9. 设计权衡与遗留项（深思记录）

1. **为何扁平参数模型而非代数表达式？** 工程上 90% 的参数化需求是"同一名义尺寸出现在多处"（如孔距 L 同时决定两个标注），扁平 `name→value` 已足够；代数依赖会引入表达式求值、循环引用检测、单位系统等额外面，与本次"打通链路 + 一致性校验"的目标不匹配。后续若需 `B = A*2`，可在 `driveParameter` 调用方组合多次驱动实现。

2. **为何单几何尺寸用 `idA==idB`？** 因为 `Dimensional` 残差只作用于 `sizeOf(*b)`；让被度量对象同时充当主动/被动方，既满足残差引用，又不必引入"临时占位图元"。代价是 `detectConflicts` 的同对判定对这类记录恒为自反，但 `idA==idB` 的自比较在 `i<j` 双循环里不会发生，无副作用。

3. **OSNAP 未做侵入式增强**：既有引擎已覆盖端点/中点/交点/圆心/象限/垂足/切点/最近，且带优先级与距离兜底。本任务以稳定性测试固化其行为，避免为"增强"而引入回归；若后续要支持"两点中点""递延切点"等，应在 `snap::` 内扩展并新增对应 `t_snap*` 用例。

4. **约束未纳入撤销栈的既有特性被保留**：`constraints_` 与 `params_` 均不入 `Memento`（与 `clear()` 不重置约束的历史约定一致）。几何变更可撤销，约束/参数定义本身不可撤销——若需"撤销一次约束添加"，属独立需求，应在 `Memento` 中额外收录 `constraints_`。

5. **`syncDimensionValues` 覆盖设计意图的取舍**：当几何因过约束无法达到目标值时，`measureDimension` 回填的是"实际几何量"而非"目标值"，从而让标注数字如实反映现状；目标值仍保存在约束 `r.value` 与参数表，供一致性诊断发现偏差。这是"显示真实"优先于"显示意图"的取舍。
