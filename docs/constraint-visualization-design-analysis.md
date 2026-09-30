# 对象捕捉视觉增强与几何约束可视化设计分析

> 对应需求（Request N）：完善对象捕捉（OSNAP）与几何约束求解模块的可视化与诊断。
> 覆盖四大子目标：① OSNAP 关键点高亮吸附与视觉提示；② 图形视图中约束图标 + 类型标注；
> ③ 约束栏（Constraint Bar）面板；④ 过约束/冲突告警 + 高亮，且添加/删除/修改约束时实时更新。

---

## 1. 需求映射与实现策略

| 需求 | 落地方式 | 关键文件 |
| --- | --- | --- |
| ① 端点/中点/圆心/交点/垂足/切点 高亮吸附与视觉提示 | 复用既有 `snap::snap` 命中结果，在 `drawForeground` 按 `snapNow_.type` 画专属标记 + 类型标签；**新增垂足 ⊥ 专属形状** | `src/ui/GraphicsView.cpp` (`drawForeground`) |
| ② 受约束对象显示约束图标并标注类型 | 新增 `drawConstraintMarkers()`：遍历 `model_->constraints()`，按锚点画图标 + `constraintTypeName()` 标签，satisfied 着色 | `src/ui/GraphicsView.cpp` |
| ③ 约束栏面板展示选中对象的约束与状态 | 新增 `ConstraintBar` 面板（停靠窗），列出选中图元所有约束（类型/参与图元/值/满足态），支持「定位」「删除」 | `src/ui/ConstraintBar.h/.cpp` |
| ④ 过约束/冲突告警 + 高亮 + 实时更新 | `constraintsChanged` → `refreshConstraintDiagnostics()` 调 `ConstraintSolver::diagnose` → `conflictIds_` 集合 → 图元红色描边 + 顶部告警条 + 日志；信号链保证实时 | `src/ui/GraphicsView.cpp`, `MainWindow.cpp` |

设计原则：**不重写既有捕捉/求解内核**，只在「可视化层」与「少量查询/删除 API」上增量扩展，复用 `snap::snapTypeLabel`、`constraintTypeName`、`ConstraintSolver::diagnose` 等既有单一数据源，避免名称/语义漂移。

---

## 2. OSNAP 视觉增强（需求①）

命中标记全部在 `GraphicsView::drawForeground` 中绘制，使用 `snap::snapTypeLabel(snapNow_.type)` 作标签（与快捷键/菜单/设置面板同源）。各类型颜色与形状：

| 类型 | 颜色 | 形状 |
| --- | --- | --- |
| Endpoint 端点 | 蓝 (60,130,240) | 方框 |
| Center 圆心 | 品红 (200,60,200) | 方框 |
| Intersection 交点 | 绿 (40,170,70) | 双对角线 ✕ |
| Tangent 切点 | 橙 (240,150,30) | 椭圆 ○ |
| **Perpendicular 垂足** | 青 (0,160,200) | **新增 ⊥ 符号（小方框 + 直角）** |
| Midpoint 中点 | 蓝紫 (80,120,220) | 方框 + 点 |
| Quadrant 象限点 | 粉 (240,90,160) | 菱形 |
| 其余（Node/Nearest/Parallel/…） | 各自配色 | 对应几何符号 |

**本次关键补强**：原 `Perpendicular` 因 `drawForeground` 形状分支缺对应 `case`，落入 `default` 方框，与端点/圆心无区分；现补 `case snap::SnapType::Perpendicular` 绘制清晰 ⊥（外框 + 水平臂 + 垂直臂），明确表达「垂直/垂足」吸附。

**默认开关决策**：`SnapConfig::perpendicular` 维持默认 `false`（与 `midpoint` 一致，作为可切换模式，经 Shift+右键 / P 键开启）。原因：OSNAP 各类为互斥可选模式，若默认开启，其捕捉优先级（80）高于中点(60)/切点(70)/最近(10)，会在用户未要求时强制抢吸，既破坏既有 `Snap Midpoint/Tangent/Nearest` 自测，也不符合「按需开启」的交互直觉。切点、端点、圆心、交点仍默认开。

---

## 3. 约束图标 + 类型标注（需求②）

新增 `GraphicsView::drawConstraintMarkers(QPainter*)`，**始终绘制（不限绘制命令）**，让任意时刻都能看到受约束对象的状态。

- **锚点计算** `constraintAnchor(model, r)`：两端图元中心中点；单端约束（FixedPoint/PointOnLine/PointOnCircle/Midpoint）取被锁定控制点 `r.idxA`；圆/弧取圆心，其余取控制点均值。
- **图标**：以锚点为中心画小实心圆（恒定屏幕大小 `5px`），颜色按满足态：
  - `satisfied == true` → 绿 (30,150,110)
  - `satisfied == false` → 橙 (220,120,30)
- **类型标注**：`constraintTypeName(r.type)`；若 `value>0` 且为 Dimensional/Distance/Angle，追加数值与单位（`°` 用于角度）。用 `drawPixelText` 像素级正写，不随缩放变形。

数据源为 `SketchModel::constraints()`（显式约束表），与约束栏面板共用同一份记录，保证画布图标与面板列表一致。

---

## 4. 约束栏面板（需求③）

新增 `src/ui/ConstraintBar.h/.cpp`，以 `QDockWidget`（标题「约束栏」）停靠于右栏（属性面板与执行信息之间）。

- **刷新时机**：`SketchModel` 的 `constraintsChanged` / `selectionChanged` / `modelChanged` 均触发 `refresh()`。
- **内容**：
  - 有选中 → 仅列「选中图元参与的约束」（用 `ConstraintSolver::constraintsInvolving` 收集下标，取并集）；
  - 无选中 → 列全部约束。
  - 每行：`类型   #idA ↔ #idB [满足/未满足]`，含值（如 `= 50` / `= 90°`）。未满足项橙色前景。
- **交互**：
  - 「定位」→ 发射 `locateRequested(idA)` → `MainWindow` 转发 `view_->zoomToElement(idA)`（缩放适配该图元）。
  - 「删除」→ 发射 `deleteRequested(index)` → `model_->removeConstraint(index)`（按下标删除并 `emit constraintsChanged`，触发全链刷新）。
- **冲突横幅**：`refresh()` 内调 `ConstraintSolver::diagnose`，若 `overconstrainedIds` 非空或状态为 OverConstrained/Conflicting，显示红色横幅「⚠ <stateText> (图元 #…)」。

---

## 5. 过约束 / 冲突诊断与高亮（需求④）

### 5.1 数据流
```
constraintsChanged()                         (SketchModel 添加/删除/修改约束后 emit)
   ├─> ConstraintBar::refresh()              (面板刷新 + 冲突横幅)
   └─> GraphicsView::refreshConstraintDiagnostics()
          ├─ ConstraintSolver::diagnose(*model_)   (只读，重算 DOF/过约束)
          ├─ conflictIds_ ← diagnose.overconstrainedIds
          ├─ model_->log("约束诊断：…", "error")    (日志/状态栏告警)
          └─ rebuild()  → ElementItem.warn = conflictIds_.contains(id)
```

### 5.2 高亮呈现
- `ElementItem::warn`（新增成员）：在 `penFor()` 中优先于选中色，使用红色 `WARN(214,48,49)` 且加粗（`wpx + 2.0`）。
- `drawConflictOverlay()`：若 `conflictIds_` 非空，在视口左上角（`mapToScene(10,10)`，像素级正写）绘制红色告警条「⚠ 检测到过约束 / 冲突：图元 #…」。

### 5.3 实时性
- **几何更新**：`ConstraintSolver::addConstraint` 内部 `beginEdit` + 数值求解，会把被动方几何解到满足并 `emit modelChanged()`；视图 `onModelChanged → rebuild()` 即重绘，约束图标（读 `r.satisfied`）随之刷新。
- **约束表变化**：`addConstraintRecord` / `removeConstraint` / `setConstraints` 均 `emit constraintsChanged()`，驱动面板与高亮双重刷新。
- **修改约束**：经「删除旧 + 添加新」或参数表驱动（`driveParameter` 改 `value` 后重解），最终都落到 `constraintsChanged` / `modelChanged`，保证可视化状态同步。

---

## 6. 新增 / 修改 API 清单

| 符号 | 位置 | 说明 |
| --- | --- | --- |
| `ConstraintSolver::constraintsInvolving(const SketchModel&, int id)` | `ConstraintSolver.h/.cpp` | 返回涉及 `id` 的约束在约束表中的下标（`idA/idB/axisId == id`） |
| `SketchModel::removeConstraint(int index)` | `SketchModel.h/.cpp` | 按下标删除约束并 `emit constraintsChanged()` |
| `GraphicsView::refreshConstraintDiagnostics()` | `GraphicsView.h/.cpp` | 重算冲突集合、刷新高亮与日志（public，供主窗口接线） |
| `GraphicsView::zoomToElement(int id)` | `GraphicsView.h/.cpp` | 视图缩放适配指定图元（约束栏「定位」用，public） |
| `GraphicsView::conflictIds_` (QSet<int>) | `GraphicsView.h` | 冲突高亮集合 |
| `ElementItem::warn` (bool) | `GraphicsView.h` | 冲突红色描边标志 |
| `ConstraintBar` 类 | `src/ui/ConstraintBar.h/.cpp` | 约束栏面板（需加入 `CMakeLists.txt` 的 `UI_SOURCES`） |

**CMake**：`src/ui/ConstraintBar.cpp` 已加入 `UI_SOURCES`，AUTOMOC 自动处理其 `Q_OBJECT`。

---

## 7. 文件改动清单

- `src/core/Snap.h`：`perpendicular` 默认维持 `false`（决策见 §2）。
- `src/core/ConstraintSolver.h/.cpp`：新增 `constraintsInvolving`。
- `src/core/SketchModel.h/.cpp`：新增 `removeConstraint`。
- `src/ui/GraphicsView.h`：新增 `warn`、`conflictIds_`、四个方法声明、`#include <QSet>`。
- `src/ui/GraphicsView.cpp`：垂足 ⊥ 标记；`drawConstraintMarkers` / `drawConflictOverlay` / `refreshConstraintDiagnostics` / `zoomToElement`；`penFor` 与 `rebuild` 支持 `warn`；`#include "core/ConstraintSolver.h"`；新增 `WARN` 颜色。
- `src/ui/ConstraintBar.h/.cpp`：新增约束栏面板。
- `src/ui/MainWindow.h/.cpp`：`buildDocks` 增加约束栏停靠；`connectSignals` 接 `constraintsChanged`/`selectionChanged`/`modelChanged` → 面板与视图，及面板「定位/删除」信号。
- `CMakeLists.txt`：`UI_SOURCES` 加入 `src/ui/ConstraintBar.cpp`。
- `tests/SketchSelfTest.cpp`：新增 `t_constraintInvolving` 并注册。

---

## 8. 验证

- 构建：`SketchCAD` 与 `SketchSelfTest` 双目标均通过 Ninja 链接（`BUILD_DONE_EXIT=0`）。
- 自测：`SketchSelfTest` **46 / 0 通过**，新增 `constraintsInvolving` 命中用例，原 `Snap Midpoint/Tangent/Nearest` 用例不受影响。

---

## 9. 已知限制与后续

- **实时诊断成本**：`diagnose` 为只读全量分析，当前仅在 `constraintsChanged` 触发（约束集变化），不在每次 `modelChanged` 重算，避免拖拽卡顿；如需「拖动中实时显示过约束」可后续对 `modelChanged` 做节流后重算。
- **冲突弹窗**：当前以日志 + 画布告警条 + 红色描边表达，未用模态 `QMessageBox`，避免载入旧档或连续编辑时频繁弹窗；如需更强提示可在 `refreshConstraintDiagnostics` 内对「新增冲突」做一次性弹窗。
- **单端约束锚点**：Fixed/FixedPoint 等单端约束以 `r.idxA` 为锚，若未指定则回退图元中心；后续可扩展为取实际被锁定的几何特征点。
