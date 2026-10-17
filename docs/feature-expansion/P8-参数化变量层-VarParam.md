# P8 参数化设计：变量表达式与驱动（VarParam + ConstraintSolverFacade 变量层）

> 功能扩充方案_参考CAD对照.md §3.3 —— P8。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch14「参数化设计：变量表达式与驱动（🆕）」：在约束求解门面上扩**变量表达式层**，实现「定义变量 → 驱动变量 → 几何重解 → 标注刷新」的参数化闭环。

提示词原文要点（§3.3）：

- **GOAL**：在 `ConstraintSolverFacade` 上扩变量层：`defineVariable(name, expr)`、`drive(name, val)`、显示尺寸表达式/附加尺寸/过约束尺寸开关。
- **CONTEXT（必注入）**：`src/core/ConstraintSolverFacade.h`、`src/core/ConstraintSolver.h`。
- **IO_SPEC**：变量表达式求值走既有或**轻量安全求值器**；驱动 = 改变量 → 重解约束 → 刷新关联标注。
- **ACCEPT**：① ≥8 断言（变量定义/驱动后几何变化/循环依赖检测/过约束标识）；② 退化：未定义变量/除零 → false + log；③ `tests/varparam_test.cpp` 注册。
- **FORBID**：改动既有约束求解语义；改枚举。
- **ESCALATE**：变量层与约束求解耦合导致语义变化时停止（G6）。

---

## 2. 思考与方案选型

### 2.1 分层：变量层只「算值」，应用全委托既有路径（FORBID / ESCALATE G6 的核心应对）

既有体系（Request M）已具备完整参数驱动链路：`linkDimension(m, dimId, paramRef)` 建关联 → `driveParameter(m, param, val)` 改值 → `solveAllNumeric` 重解 → `syncDimensionValues` 刷新标注。P8 若在求解器内部掺入表达式语义，必然触碰「既有约束求解语义」（FORBID）并可能触发 G6 停止线。

**选型：严格分层**——

```
变量层（P8 新增）                既有层（零改动）
defineVariable(name, expr)  →   变量表 + 安全求值器 + 拓扑重算
drive(m, name, val)         →   对「已关联变量」逐个调用既有 driveParameter
                                      （内部：setParam → 改约束目标 → solveAllNumeric → syncDimensionValues）
```

变量层职责边界：**名校验、表达式解析/求值、依赖拓扑、变化集计算、显示开关组装**。几何重解与标注刷新 100% 委托既有 `driveParameter`——本项对 `ConstraintSolver.cpp` **零改动**，从根上排除语义耦合（ESCALATE G6 不触发，见 §6 偏差表）。

### 2.2 值变量 / 表达式变量二分（驱动语义的无歧文化）

`defineVariable(name, expr)` 定义期即解析表达式：

- **不含变量引用**（如 `"50"`、`"2*25"`）→ 立即求值，转为**值变量**（可直驱）；
- **含变量引用**（如 `"2*W+10"`）→ **表达式变量**（不可直驱）。

直驱表达式变量会静默丢失其定义（下游全部失真），故 `drive` 对表达式变量**显式拒绝**（返回原因 + log），引导用户驱动其依赖变量。这是有意的语义收紧，替代「覆盖式驱动」的歧义设计。

### 2.3 轻量安全求值器（IO_SPEC）

代码库无既有表达式求值器（已检索确认），按 IO_SPEC 自写**轻量安全**递归下降求值器：

- 语法：`+ - * /`、括号、一元正负号、数值常量、变量引用——**无函数调用、无赋值、无逗号**（无 `eval` 式风险面）；
- 安全封顶：表达式 ≤ 512 字符、嵌套/连续一元号深度 ≤ 64（防栈耗尽）、变量名 ≤ 64；
- 数值：ASCII 手工累加解析（不经 `QString::toDouble`，规避本地化差异）；除零、非有限结果（溢出）显式报错；
- 变量名规则与既有 `paramRef`（"L"/"R" 风格）兼容，同时放开 Unicode 字母（中文变量名「长度」可用）。

### 2.4 先演算后提交（失败零改动）

`resolveDrive` 两阶段：① 在 staging 值映射上按拓扑序演算全部表达式变量；② 仅当全部成功才提交并输出**变化集**（拓扑序：驱动值在前、依赖随后）。任何失败（循环/除零/未定义）→ 整体放弃，变量表与几何**零改动**——满足 ACCEPT「退化 → false」且不坏状态。

### 2.5 三个显示开关（GOAL 第三句）

会话级 `VarDisplaySwitches`（默认全开）：

| 开关 | 语义 | 关闭效果 |
|------|------|----------|
| `showExpression` | 尺寸文本优先显示关联变量的表达式原文 | `valueText` 显示数值 |
| `showAdditional` | 标记「变量层附加」尺寸（dimParam 关联到已定义变量） | 不置 `additional` 位 |
| `overconstrained` | 过约束尺寸标识 + 驱动后过约束告警日志 | `overconstrainedDimensionIds` 返空、不告警 |

过约束判定**只读**：经既有 `diagnose()` 取 `overconstrainedIds` → 图元涉及约束（`constraintsInvolving`）→ 约束 `driverId` 回指尺寸标注。不引入新的求解判定逻辑。

---

## 3. 核心算法与设计思路

### 3.1 模块与 API

**新增 `src/core/VarParam.{h,cpp}`**（`VarParamTable`，会话级变量表）：

| API | 语义 |
|-----|------|
| `define(name, expr)` | 定义变量；常量表达式定义期求值转值变量；语法错/除零 → 原因 |
| `setValue(name, val)` | 纯数值变量（清除表达式） |
| `resolveDrive(name, val, changed, err)` | 设值 + 拓扑重算，输出变化集（先演算后提交） |
| `evalExpression(expr, out, err)` | 静态安全求值（无上下文；标识符 → 未定义） |
| `value(name)` / `expression(name)` / `isExpression(name)` / `names()` | 查询（求值失败 → NaN） |

**`ConstraintSolverFacade` 扩展**（门面成员 `VarParamTable vars_` + `VarDisplaySwitches switches_`）：

| API | 语义 |
|-----|------|
| `defineVariable(name, expr)` / `setVariableValue(name, val)` | 变量表维护（薄委托） |
| `drive(m, name, val)` | 完整驱动链（见下） |
| `dimensionDisplayInfo(m, dimId)` | 按开关组装尺寸显示信息 |
| `overconstrainedDimensionIds(m)` | 过约束尺寸 id 列表（只读） |

### 3.2 `drive` 完整链路

```
drive(m, name, val)
 ① has(name)?                       否 → 「变量未定义」warn + return err（零改动）
 ② isExpression(name)?              是 → 「表达式变量不能直驱」warn + return err（零改动）
 ③ resolveDrive(name, val)          失败（循环/除零/未定义引用）→ warn + return err（零改动）
 ④ changed 为空？                   是 → info「值未变化，无需重解」+ return {}
 ⑤ for (name,val) in changed:        // 拓扑序
      未关联模型 → 跳过（中间变量常见，非错误）
      已关联 → driveParameter(m, name, val)   // 既有路径：重解 + 刷新标注
               真错误上抛（warn + return err）
 ⑥ overconstrained 开关开 → diagnose 只读判定 → 过约束告警 warn（不改几何）
 ⑦ applied==0 → info「未关联任何标注」；否则 info「N 个变量已应用并重解」
```

### 3.3 拓扑重算与循环检测（Kahn）

- 定义期为每个表达式变量解析并存储**直接依赖集** `refs`；
- 驱动时对表达式变量子图做 Kahn 拓扑排序（边 dep→dependent；值变量为源，不计入）；
- 处理数 < 表达式变量数 ⇒ 存在环 ⇒ 失败并列出环上变量（`A → B` 形式）；
- 演算在 staging 上进行，`evalVar` 另有栈式循环检测作为第二道防线（`value()` 公开查询路径）。

### 3.4 退化用例（ACCEPT ②）

| 场景 | 行为 |
|------|------|
| 驱动未定义变量 | false + `model.log(warn)`「变量 %1 未定义」；零改动 |
| 表达式除零（定义期/驱动期） | false + 原因含「除零」；定义期直接拒绝定义 |
| 引用未定义变量 | 求值 false + 「未定义变量」；定义期允许（任意顺序定义），求值/驱动期暴露 |
| 循环依赖 | `value()` → NaN；`drive` 阻断 + 「循环依赖」+ 零改动 |
| 表达式变量直驱 | false + 「不能直接驱动」+ 零改动 |
| 驱动值无变化 | 成功但不重解（info 日志） |

---

## 4. 关键实现要点

**新增/修改文件**
- `src/core/VarParam.h`：`VarParamTable` 类（定义/查询/`resolveDrive`/静态 `evalExpression`/`isValidName` + 长度深度常量）。
- `src/core/VarParam.cpp`：`ExprParser`（递归下降，收集/求值双模式、深度守卫、ASCII 数字手工解析）+ 表逻辑（Kahn 拓扑、staging 演算、栈式循环检测）。
- `src/core/ConstraintSolverFacade.h`：`#include "VarParam.h"`；新增 P8 API 段（`defineVariable/setVariableValue/drive/hasVariable/variableExpression/isExpressionVariable/variableValue/variableNames/clearVariables/variableTable` + `VarDisplaySwitches` + `DimensionDisplayInfo` + `dimensionDisplayInfo` + `overconstrainedDimensionIds`）；私有成员 `vars_`/`switches_`。
- `src/core/ConstraintSolverFacade.cpp`：P8 方法实现 + 文件局部助手 `isParamLinkedInModel`（约束 paramRef / 标注 dimParam 命中判定）；`#include <QSet>`。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `VarParam.cpp`。
- `tests/SketchSelfTest.cpp`：`#include "VarParam.h"` + `static bool t_varparam()` + `CHECK` 注册。

**约束遵守**
- **FORBID「不改既有约束求解语义」**：`ConstraintSolver.cpp` / `IConstraintSolver` / `LegacyConstraintSolver` **零改动**；变量层应用一律经既有 `driveParameter`（路由/异常回落/撤销语义原样继承）。
- **FORBID「不改枚举」**：未触任何枚举。
- **ESCALATE G6**：分层设计下变量层与求解器无语义耦合（只传数值），不触发停止线；已在文档显式记录该判断依据。

---

## 5. 自测覆盖（`t_varparam`，全部断言）

1. **变量定义**：常量→值变量（value=50）；含引用→表达式变量（原文可查）；链式求值 L=2*W+10=110；求值器括号/优先级/一元号/小数；退化：除零/缺右括号/连续乘号/未定义标识符/超深嵌套(>64)；变量名校验（中文合法、数字开头/空格/空名非法）；define 非法名/常量除零 → 定义期拒绝；
2. **驱动后几何变化**：圆半径 5 → drive(R,12) → 12；measureDimension=12；模型参数表同步；drive 经 model.log 记录；
3. **依赖传播**：W=5、L=2*W+10 关联半径标注 → drive(W,6) → L=22 → 圆半径 22；表达式变量直驱 → 拒绝（含「表达式」）且几何不变；
4. **循环依赖**：A=B+1、B=A+1 → value(A)=NaN；drive(W,9) 被阻断（含「循环依赖」）+ warn 日志 + 零几何改动；
5. **过约束尺寸**：长度标注 + 矛盾 Dimensional(20) → `overconstrainedDimensionIds` 精确标出 d1；`dimensionDisplayInfo` 的 overconstrained/additional/expressionText/valueText；驱动仍成功且几何显著改变（矛盾约束下 LM 取残差折中 ≈22.5，非精确到达目标值）；过约束告警日志；开关全关 → 空标识 + valueText 显示数值；
6. **退化汇总**：驱动未定义变量 → false + warn + 零改动；setVariableValue/variableNames/clearVariables + NaN 回落。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 **2861 / 0**、`EXIT=0`；全量 `SketchCAD` 链接确认。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| `tests/varparam_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_varparam()` + `CHECK` 注册） | P1~P7 已据此偏离；项目基线为单文件聚合自测。 |
| ESCALATE「变量层与约束求解耦合导致语义变化时停止(G6)」 | 未停止：分层设计使耦合度为「只传数值」——`ConstraintSolver.cpp` 零改动，应用全委托既有 `driveParameter` | 无语义变化 ⇒ 不触发停止线；判断依据与边界在本文件 §2.1/§3.2 显式记录。 |
| 「驱动=改变量→重解约束→刷新关联标注」 | 逐变量委托既有 `driveParameter`（内部已含重解+刷新）；多变量时按拓扑序多次调用 | 复用既有链路保证 FORBID；代价是 N 个变量 = N 次重解（N 通常 ≤ 数个，可接受）。 |
| GOAL 未定义 OUT_OF_SCOPE / 表达式函数（sin/cos 等） | 求值器仅四则运算 + 括号 + 一元号，无函数表 | 「轻量安全」取舍：无函数调用即无注入面；三角函数需求可作后续扩展（需重新评估安全边界）。 |
| 「显示尺寸表达式/附加尺寸/过约束尺寸开关」 | 实现为会话级 `VarDisplaySwitches` 三开关 + `dimensionDisplayInfo` 只读组装 | 提示词未给 UI 形态；核心层提供开关与数据组装，UI 消费留给界面层。 |

**已知限制（Phase-1 范围）**
- 变量表为**会话级**（与门面同生命周期）：不随图纸存档/读档（模型侧 `params_`/`paramRef` 已随图存档，但表达式定义不持久化）——跨会话恢复表达式需另立存档键（G4 存档格式，未动）。
- 表中存在**任何**循环依赖即阻断一切驱动（方程系统不一致的整体语义）；须先删环。
- 过约束判定复用既有 `diagnose`（雅可比秩分析），大规模约束表的诊断开销随之继承。
- 求值器无函数/比较/条件运算；`drive` 对未关联模型的变量仅更新表值（info 日志），不入撤销栈（变量表现话态，与 commandOptions_ 同性质）。

---

## 7. 验收结论

- ✅ 真实功能扩充：`VarParam.{h,cpp}`（变量表 + 安全求值器 + 拓扑重算）+ 门面变量层 API（定义/驱动/查询/三显示开关/尺寸显示信息/过约束标识）。
- ✅ 驱动闭环：改变量 → 拓扑重算 → 既有 `driveParameter` 重解 → `syncDimensionValues` 刷新标注（自测以圆半径/直线长度验证几何变化）。
- ✅ ≥8 断言（实际 60+ 条）：变量定义、驱动后几何变化、循环依赖检测、过约束标识、显示开关正反、全部退化分支。
- ✅ 退化：未定义变量/除零/循环依赖/表达式变量直驱 → false + `model.log(warn)` + 零改动。
- ✅ FORBID：既有约束求解语义零改动（`ConstraintSolver.cpp` 未触）、枚举零改动。
- ⏳ 后续（已列入偏差）：表达式随图存档（G4）、求值器函数扩展、变量层 UI 消费、参数驱动并入撤销栈。
