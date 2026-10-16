# 标注尺寸可编辑（尺寸驱动几何联动）设计分析

> **任务**：修订绘图程序，实现标注尺寸的可编辑功能——直接修改图元上的尺寸标注数值，该图元及相连/受约束相邻图元按几何关联自动调整位置与形状；所有受影响图元的尺寸标注、引线、文字内容同步更新，标注数值与实际几何保持一致；联动实时生效且数据完整（单步撤销）。
> **状态**：✅ 已实现并验证（双目标零警告，全量自测通过，新增 t_dimDriven 19 断言）
> **日期**：2026-10-15
> **关联文档**：`docs/sketchcad-gap-analysis-remediation-2026-10-15.md`（G-A 约束求解器/参数化的近期可落地子集）、`docs/field-table-formula-design-analysis.md`（FIELD 域刷新机制）

---

## 一、思考过程（Thinking）

### 1.1 需求拆解

| 需求点 | 分解 |
|---|---|
| 直接修改图元上的标注数值 | 需要画布级直接编辑入口（双击）+ 命令行入口，且改的是**数值**而非仅显示文本 |
| 该图元自身调整 | 标注绑定的源几何（线终点/圆半径/角度边）按新值重算 |
| 相邻连接/约束图元联动 | 端点重合即相连（collectLinked 传播）+ 隐式约束恢复 + 显式约束重解 |
| 受影响的标注/引线/文字同步 | 同测点标注随动、覆盖文本清除、FIELD 域刷新、填充边界随动 |
| 实时生效、数据完整 | 单次 modelChanged；单次 pushMemento（一步撤销全恢复）；Quiet API 不重复压栈 |

### 1.2 现状核查（代码实况证据）

| # | 事实 | 证据 |
|---|---|---|
| 1 | `SketchModel::driveDimension(dimId, value)` 已存在：支持对齐/距离/水平/竖直（改线终点，保持方向）与半径/直径（改圆弧半径），走 collectLinked 端点传播 | SketchModel.cpp:752（修订前） |
| 2 | **缺口 A**：无画布直接改值入口——双击标注只弹"覆盖文本"输入（不改几何）；ddedit 只认 Text/MText；无 dimedit 命令 | MainWindow.cpp:2034-2045、Commands.cpp:4510 |
| 3 | **缺口 B**：驱动后不做约束闭环——updateElementLinked 有 solveAfterChange + solveAll，driveDimension 没有 | SketchModel.cpp:734-745 vs :810-840 |
| 4 | **缺口 C**：角度标注 `default: return false` 不支持驱动；且角度标注创建时**不设 refA**（pts={顶点,边1点,边2点}），被 :754 的 `refA < 0` 前置直接挡掉 | Commands.cpp:2060、SketchModel.cpp:754 |
| 5 | **缺口 D**：驱动后不刷新关联填充（refreshHatchBoundaries）与 FIELD 域（refreshFields） | 对比 moveHandles:670 / applyChanges:Modify.cpp:1098 |
| 6 | **缺口 E**：被联动带动的其它标注若带覆盖文本，文字不随几何更新（显示值≠实测值） | collectLinked 会移动 Dimension 的 pts（elementHandles default 分支返回 pts），但不清 text |
| 7 | 属性面板"测量值"编辑已走 driveDimension（可复用语义） | PropertyPanel.cpp:512-519 |
| 8 | 批量联动撤销范式：一次 pushMemento + 全 Quiet API | SketchModel.h:48-52、driveDimension:810 |

### 1.3 方案比选

**方案一（选定）：强化既有 `driveDimension` 为单一驱动入口，三个入口（双击/dimedit/属性面板）全部汇入。**
- 优点：属性面板已验证该语义；传播/撤销/信号链路现成；改动集中（1 个核心函数 + 2 个薄入口 + 1 条命令）。
- 缺点：驱动能力受"单一标注→单一源几何"模型限制（不能解方程组）。

**方案二（否决）：新建通用几何求解器（方程组迭代求解）。**
- 即差距报告 G-A 的完整形态（solvespace 集成）。工程量数周级、需许可评审，远超本需求边界；且 driveDimension 的事后模型对本需求（单尺寸驱动）已够用。**作为后续演进路线保留**：本特性把三个入口汇到 driveDimension 单点，未来换求解内核时入口层零改动。

**方案三（否决）：每个编辑入口各自实现联动逻辑。**
- 违反"同一语义两处手工拼装⇒一处更新一处漏"铁律（BatchPlotEngine JSON 拼装漏 sheet 的实证教训）。

**角度驱动的子决策**：角度标注无 refA，两种取法——
- (a) 要求先 `dim angle` 时绑定腿线 id（改创建命令 + 序列化兼容）：侵入创建链，旧档无绑定不可驱动；
- (b) **按坐标锚定**（选定）：顶点 pts[0]、边1点 pts[1] 不动，把"与旧边2点 pts[2] 重合的所有图元端点"绕顶点旋转到新角。旧档立即可用，联动语义与"端点重合即相连"的既有约定一致。保持原旋向（当前有符号角为负则反向旋转）与 |p2−顶点| 半径，避免跳变。

**覆盖文本的同步策略**：被联动的标注一律 `text.clear()`（覆盖文本本质是"冻结显示值"，几何变了冻结值必然失真）。用户若真要固定文本，可在驱动后重新覆盖——可预期性优先。

### 1.4 实测迭代：隐式约束修复与端点联动的冲突（本轮实证）

首轮自测暴露一处真实交互缺陷（诊断输出实证）：驱动对齐标注 100→50 后，
相邻竖线端点未贴合到新端点，而是落在 (75,-12.016)。
根因链（三层叠加）：
1. collectLinked 传播只移动邻线**共享端点**（另一端留在原位）→ 邻线倾斜（(50,0)-(100,40)，38.66°）；
2. `solveAfterChange` 检出驱动前满足的隐式**垂直**关系被破坏（isPerpendicularLines 按方向角判定）；
3. 其修复策略是"绕邻线**自身中点**旋转到垂直"（Geometry.cpp restoreConstraint Perpendicular 分支）→ 邻线恢复竖直但**脱开**源线（足点落在 (75,-12.016)）。

**修复（刚体平移优先）**：传播时先查驱动前源线与邻线的隐式关系
（`geo::checkConstraint(Perpendicular/Parallel, 0.5°)`，取 beforeElems 快照判定）——
若为垂直/平行，则**整体平移邻线**（方向保持 ⇒ 垂直/平行自动成立、端点贴合不破坏），
替代"只挪共享端点"；其余图元（多段线/标注/引线）仍走既有的"共享端点跟随"语义。
修复后 solveAfterChange 检查关系已满足 → 零动作，不再产生二次破坏。

### 1.5 风险与放弃

- **放弃**：基线/连续/弧长/引线/坐标标注的驱动（注释型，无独立源几何语义）——显式 `return false` 并在 dimedit 中给出可读提示，而非静默失败。
- **风险**：solveAfterChange/solveAll 在驱动路径上引入额外开销 → 仅在有约束时执行（constraints_.empty() 短路），与 updateElementLinked 同规。
- **风险**：角度驱动旋转 collectLinked 命中集时可能连带移动恰好压在边2点上的无关图元 → 与直线驱动的既有传播语义一致（AutoCAD 拉伸同理），容差 1e-6 下误命中概率可忽略。

---

## 二、评测结论（Assessment）

**改造前 vs 改造后**（对标 AutoCAD 2025 的"尺寸驱动编辑/夹点数值输入"）：

| 能力 | 改造前 | 改造后 | AutoCAD 2025 |
|---|---|---|---|
| 双击标注 | 仅改覆盖文本（显示与几何脱钩） | 数值→几何联动；留空→恢复测量；文本→覆盖 | 数值输入驱动几何 ✅ |
| 命令行驱动 | 无 | `dimedit <id> <值>` / 选中后 `dimedit <值>`，容忍 R/⌀/° 前后缀 | 无直接等价（用特性/约束）✅ |
| 相邻图元联动 | 驱动直线时端点重合者随动 | 同左 + 隐式约束恢复 + 显式约束重解 | 约束求解 ✅（简化版） |
| 角度标注驱动 | ❌ 拒绝 | ✅ 边2绕顶点旋到目标角，邻边随动 | ✅ |
| 标注/文字同步 | 仅驱动标注自身 | 同测点标注测量同步 + 被联动标注覆盖文本清除 | ✅ |
| FIELD 域/填充 | 不刷新 | PERIM/AREA/PROP 域 + 关联填充随动 | ✅（域） |
| 撤销 | 单步 | 仍单步（一次快照覆盖全部联动） | ✅ |

一句话：SketchCAD 在"单尺寸驱动"粒度上补齐了 AutoCAD 的尺寸驱动编辑体验；多尺寸联合求解仍属 G-A 参数化求解器课题。

---

## 三、改动方案（Plan）

### 3.1 核心层 `src/core/SketchModel.cpp` — `driveDimension` 重写（单一驱动入口）

| 改什么 | 不改什么 |
|---|---|
| 前置校验放宽：仅 Radius/Diameter/直线类要求 refA；**Angle 按 pts 三点坐标锚定**；新增非法值守卫（长度 ≤0、角度 ∉(0,360) 拒绝） | 函数签名、返回值语义（bool） |
| **新增角度驱动**：边2点绕顶点旋转到目标角（保持旋向与半径） | 对齐/水平/竖直/半径/直径的目标几何计算逻辑 |
| 联动传播收集 `touched` 集合；**被联动的 Dimension 清除覆盖文本**；**隐式垂直/平行的相邻直线刚体平移跟随** | collectLinked 容差（1e-6）与传播结构 |
| 传播后 **solveAfterChange（隐式约束恢复）+ solveAll（显式约束重解，仅有约束时）**，失败写 log warn | updateElementLinked 既有同规逻辑 |
| 收尾 **refreshHatchBoundaries(touched) + refreshFields(touched)**（去重后传入） | 单次 pushMemento("驱动尺寸") + 单次 modelChanged 的撤销/信号契约 |

### 3.2 命令层 `src/core/Commands.cpp` — 新增 `dimedit` 命令

- `dimedit <标注id> <新值>` 或 选中标注后 `dimedit <新值>`；`cmdArgsWithReuse` 支持空回车重复。
- 数值解析容忍前后缀（`R20`/`⌀50`/`45°`）：QRegularExpression 提取首个数值。
- 拒绝路径显式 log 可支持类型清单（对齐/距离/水平/竖直/半径/直径/角度）。
- `CommandCatalog.cpp` 登记（zhName=尺寸驱动，category=标注，usage 与实现同源），help 总表经 commandNames() 自动收录。

### 3.3 界面层 `src/ui/MainWindow.cpp` — `editElementText` 标注分支重写（双击入口）

- **Dimension**：QInputDialog 预填当前显示值；输入**纯数值**（全串正则判定）→ `driveDimension`（失败弹警告框）；**留空** → 清覆盖恢复测量值；**其它文本** → 旧行为写覆盖。
- **MLeader/Fcf**：保持原"覆盖文本"行为（无几何驱动语义）。
- 新增 `#include <QRegularExpression>`。

### 3.4 自测 `tests/SketchSelfTest.cpp` — 新增 `t_dimDriven`（19 CHECK）

| 组 | 断言 |
|---|---|
| ① 直线驱动 | 对齐标注 100→50：源线终点、相邻竖线底端随动、覆盖文本清除、双标注实测=50、单步撤销全恢复 |
| ② 圆驱动 | 半径 20→50：圆半径、标注实测、`%<PERIM:%>%` 域 314.16 同步、单步撤销恢复 |
| ③ 角度驱动 | 90°→60°：邻边端点旋至 (15, 25.98)、实测=60、坐标锚定（refA=-1 可驱动） |
| ④ 守卫 | 引线型拒绝；值 0/负拒绝 |
| ⑤ 命令入口 | `dimedit R25`（R 前缀解析）驱动成功；非数值被拒 |

---

## 四、验证（Verify）

```bash
# 构建（双目标，-j1 防 OOM）
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.10.1/mingw_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build D:/01-CAD/02-avticationCAD/sketch-cad-cpp/build --target SketchCAD SketchSelfTest -j 1

# 全量自测
PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH" ./build/SketchSelfTest.exe
```

**验收门**：双目标 `-Wall -Wextra` 零警告；全量自测 EXIT=0（既有 2469 断言零回归 + t_dimDriven 23 断言全过）。**实测：通过 2493 / 失败 0，EXIT=0。**

**人工验证场景清单**（GUI 侧，供发布核对）：
1. 双击对齐标注 → 输入 50 → 直线缩短、相连竖线底端跟随、标注即时显示 50.00；
2. 双击半径标注 → 输入 ⌀ 圆驱动后，引用该圆的 `%<PERIM:...>%` 文字与关联剖面填充同步；
3. 双击角度标注 → 输入 60 → 邻边旋转、角度实测更新；
4. Ctrl+Z 一步恢复全部联动结果；
5. `dimedit` 选中标注后输入 `R25`；help 可检索到"尺寸驱动"条目；命令面板可发现 dimedit。

---

## 五、遗留与演进

- **多尺寸联合求解**（改一个参数联动全图约束网）→ G-A 参数化求解器课题（solvespace 集成），本特性的三入口汇单点（driveDimension）为未来换内核预留了无缝升级位。
- **引线（MLeader）附着**：引线仅按端点重合随动，无 refA 绑定；如需"引线钉死在圆周上"需给 MLeader 增加引用字段（ElementType 只增不改铁律下可加字段不破坏旧档）。
