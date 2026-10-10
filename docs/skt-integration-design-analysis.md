# SKT 草图求解器集成 — 设计分析（Phase B 路由门面）

> 对应任务：将 avcad 主项目已验证的 SKT 草图求解器集成到本仓库约束体系，实现「改标注值 → 几何全图联动」，并保留现修复模型为回落基线。
>
> 经与用户确认采用**混合路线**：**B 达标后尝试 A**。本文件记录 **Phase B（路由门面 + Legacy 委托零风险达标）** 的设计、落点与验收证据；Phase A（avcad `SketchEngine` 求解核 MinGW 移植 + 适配器）路径见第 9 节。
>
> 验收基线（本轮已达成）：双目标零警告构建 `BUILD_EXIT=0`；自测 `TEST_EXIT=0`，**通过 2339 / 失败 0**（含 `t_sktIntegration` 16 条 CHECK 全计 0 失败）。

---

## 1. 边界与现状分析

| 维度 | 现状（集成前） | Phase B（集成后） |
|------|----------------|-------------------|
| 求解实现 | `ConstraintSolver`（LM 数值求解 + DOF/诊断 + 尺寸联动），由 `SketchModel`/`Commands`/`ConstraintBar` 直接静态调用 | 同一 `ConstraintSolver` 经 `LegacyConstraintSolver` 委托，再由 `ConstraintSolverFacade` 统一路由 |
| 调用耦合 | 调用点直连具体求解器，无抽象层 | 调用点 → `Facade` → `IConstraintSolver` 接口，**与具体求解实现解耦** |
| 特性开关 | 无 | 新增 `solver/constraintEnabled`（默认开），关→回落修复模型，几何行为不变 |
| 可切换接缝 | 无 | `Facade` 单例预留 `setActiveSolver()`，Phase A 可注入 SKT 适配器 |
| 存档格式 | `ConstraintRecord` / `Element` 结构不变 | **不变**（仅增求解接缝，不动数据模型） |
| 单元测试 | `t_sktIntegration` 不存在 | 新增 `t_sktIntegration`（16 CHECK），注册于 `SketchSelfTest.cpp:8802` |

**关键结论**：本仓库**已具备完整求解能力**（`ConstraintSolver` 即任务所指「修复模型」）。因此 Phase B 的目标**不是另写求解器**，而是把散落在各处的调用收敛到一层可切换接缝，并套上特性开关与异常回落围栏——在不引入任何行为回归的前提下，为 Phase A 的 SKT 内核替换打开一个**零风险注入点**。由于 Phase B 的活动实现即 `LegacyConstraintSolver`（= 修复模型本身），特性开关「开」与「回落」在几何结果上严格等价。

---

## 2. 关键设计决策

1. **抽象层 `IConstraintSolver`（7 纯虚方法）**：把「调用约束求解」与「具体求解内核」解耦。接口方法集与既有 `ConstraintSolver` 静态方法**一一对应**（除纯只读辅助），现有显式约束表的 `ConstraintType` 与求解器约束类型 **1:1 映射**（见第 6 节映射表）。任何新增实现都必须覆盖同一组约束类型，保证存档/行为向后兼容。
2. **`LegacyConstraintSolver` 委托（无状态）**：每个方法 1:1 转发到 `ConstraintSolver::XXX(...)`，本身不持有几何状态——所有变更都发生在传入的 `SketchModel` / `Element` 容器上。它是特性关闭时的回落实现，也是 Phase A SKT 适配器的**对照基线**（二者须满足「同输入 → 等价几何结果」契约）。
3. **`ConstraintSolverFacade` 单例路由**：统一编排「调用点 → 活动求解实现」，承载特性开关与异常回落。构造时 `active_ = &legacy_`，故默认行为 = 修复模型。`setSettings(AppSettings*)` 注入应用设置；`setActiveSolver(IConstraintSolver*)` 预留给 Phase A。
4. **异常保护路由 `route<R>(...)` 模板**：特性关 → 直接回落 Legacy；特性开 → 调活动实现，若抛 `std::exception` 则捕获后回落 Legacy 并经 `model->log(...,"warn")` 记录。**几何结果始终由修复模型兜底**，SKT 适配器即使不稳定也不会破坏用户稿。
5. **`ElementType` / `ConstraintRecord` 不动**：严格遵循「只增不改」原则，存档格式、序列化、撤销重做管线全量复用，零回归面。
6. **隐式 4 类约束（相切/垂直/平行/同心）全保留**：仍由 `solveAfterChange` 的 detect/restore 自动维持；显式约束表既有行为不变。

---

## 3. 模块拆分与代码落点

| 文件 | 变更 | 说明 |
|------|------|------|
| `src/core/IConstraintSolver.h` | **新增** | 抽象接口 7 方法 + 约束类型↔求解器约束映射表注释（21 种） |
| `src/core/LegacyConstraintSolver.h` / `.cpp` | **新增** | 1:1 转发到 `ConstraintSolver`（修复模型委托实现），无状态 |
| `src/core/ConstraintSolverFacade.h` / `.cpp` | **新增** | 单例路由门面：特性开关 + `route<R>` 异常回落 + 7 个路由方法 |
| `src/core/AppSettings.h` | 增 1 键 | `kConstraintSolver = "solver/constraintEnabled"`（`:52`）；访问器声明（`:166`） |
| `src/core/AppSettings.cpp` | 增 3 处 | `using` 键（`:24`）、`specTable` 默认 `true`（`:183`）、访问器实现（`:500-501`） |
| `src/core/SketchModel.cpp` | 2 路由点 | `:733` `solveAfterChange`；`:740` `solveAll` 经 Facade |
| `src/ui/ConstraintBar.cpp` | 1 路由点 | `:93` `diagnose(*model_)` 经 Facade |
| `src/core/Commands.cpp` | 4 路由点 | `:2255` `linkDimension`；`:2267` `setDimensionValue`；`:2296` `driveParameter`；`:2365` `addConstraint` |
| `src/ui/MainWindow.cpp` | 1 注入点 | `:291` `ConstraintSolverFacade::instance().setSettings(appSettings_)` |
| `CMakeLists.txt` | 2 源接入 | `:63` `LegacyConstraintSolver.cpp`；`:64` `ConstraintSolverFacade.cpp`（经 `CORE_SOURCES` 接入双目标） |
| `tests/SketchSelfTest.cpp` | 1 测试 | `t_sktIntegration()`（`:339-482`，16 CHECK）注册于 `:8802` |

> 所有新增 `.cpp` 经 `CORE_SOURCES` 进入 `SketchCAD` 与 `SketchSelfTest` 双目标，双目标均 `-Wall -Wextra`，零警告基线。

---

## 4. 抽象层 `IConstraintSolver` 设计

接口与既有 `ConstraintSolver` 静态方法对齐（签名完全一致，含默认参数）：

```cpp
class IConstraintSolver {
public:
    virtual ~IConstraintSolver() = default;

    // 图元参数修改后，检测并恢复修改前满足的「隐式约束」（相切/垂直/平行/同心）
    virtual std::vector<int> solveAfterChange(const std::vector<Element>& beforeElems,
                                              std::vector<Element>& afterElems,
                                              int changedId, double tol) const = 0;

    // 求解全部「显式约束」，返回求解报告
    virtual SolveReport solveAll(SketchModel& m, int maxIterations) const = 0;

    // 逐实体欠约束/过约束诊断（只读）
    virtual ConstraintDiagnosis diagnose(const SketchModel& m) const = 0;

    // 添加一条显式约束；成功返回空串，失败返回原因（可直接 log 给用户）
    virtual QString addConstraint(SketchModel& m, ConstraintType type,
                                  int idA, int idB, double value,
                                  int axisId, int idxA, int idxB) const = 0;

    // 尺寸标注 ↔ 几何约束 双向关联（尺寸驱动）
    virtual QString linkDimension(SketchModel& m, int dimId, const QString& paramRef) const = 0;
    // 设定标注驱动值并立即重解几何
    virtual QString setDimensionValue(SketchModel& m, int dimId, double value) const = 0;
    // 参数表联动驱动
    virtual QString driveParameter(SketchModel& m, const QString& param, double val) const = 0;
};
```

**契约约束**：`addConstraint` 静态签名（`ConstraintSolver.h:192`）与 Facade/接口完全一致 —— `addConstraint(SketchModel& m, ConstraintType type, int idA, int idB, double value = 0.0, int axisId = -1, int idxA = -1, int idxB = -1)`，新增实现不得改动此签名，否则视为不兼容、禁止注入为活动实现。

---

## 5. Facade 路由模板与异常保护

`ConstraintSolverFacade` 是单例，构造时 `active_ = &legacy_`（默认指向内置 Legacy）。路由模板定义于 `.cpp`（调用点全在本 TU，无需在头暴露）：

```cpp
template<typename R, typename AF, typename FF>
R ConstraintSolverFacade::route(SketchModel* m, const char* op,
                                AF&& activeFn, FF&& fallbackFn) const {
    if (!featureEnabled()) {
        return fallbackFn();                       // 特性关 → 直接回落修复模型
    }
    try {
        return activeFn();                         // 特性开 → 调用活动实现
    } catch (const std::exception& e) {
        if (m) m->log(QStringLiteral("约束求解「%1」异常，已回落修复模型：%2")
                         .arg(QLatin1String(op)).arg(QString::fromUtf8(e.what())),
                     QStringLiteral("warn"));
        return fallbackFn();                        // 适配器异常 → 回落 + 记录
    }
}
```

七个对外路由方法（`solveAfterChange` / `solveAll` / `diagnose` / `addConstraint` / `linkDimension` / `setDimensionValue` / `driveParameter`）均经 `route<R>(m, op, activeFn, fallbackFn)` 分发：`activeFn` 调 `active_->XXX`，`fallbackFn` 调 `legacy_.XXX`。

**`featureEnabled()` 语义**：无 `AppSettings` 注入（`settings_ == nullptr`）→ 默认 `true`（开启）；否则取 `settings_->constraintSolverEnabled()`。回落路径对 `diagnose` 用 `const_cast<SketchModel*>(&m)` 仅供日志，活动/回落实现本身只读不改模型。

---

## 6. 约束类型 1:1 映射

现有 `ConstraintType`（21 种，见 `IConstraintSolver.h` 映射表注释）与求解器内部约束**一一对应**，新增实现必须产生等价几何结果：

| 约束类型 | 几何语义 |
|----------|----------|
| `Coincident` | 端点/点重合（点基，`idxA/idxB` 控制点索引） |
| `Parallel` / `Perpendicular` | 两直线平行 / 垂直 |
| `Equal` / `EqualLength` / `EqualRadius` | 等长/等径（由图元几何类型自判） |
| `Tangent` | 直线↔圆 / 圆↔圆 相切 |
| `Horizontal` / `Vertical` | 直线水平 / 竖直 |
| `Fixed` | 固定图元（主动方与 Fixed 不被移动） |
| `Concentric` | 两圆/弧同心 |
| `Symmetric` | 关于 `axisId` 直线对称（两图元，支持镜像） |
| `Dimensional` | 尺寸（长度/半径），目标值 = `value` (mm) |
| `PointOnLine` / `PointOnCircle` | 点落在直线 / 圆上（点基，`idxA` 指定点） |
| `FixedPoint` | 指定控制点固定（`idxA` 控制点索引） |
| `Midpoint` | 点在另一图元中点（点基） |
| `Distance` | 两点/图元间距离 = `value` (mm) |
| `Angle` | 两直线夹角 = `value` (°) |
| `SymmetricPoint` | 点关于 `axisId` 直线对称（点基） |

隐式约束（tangent/perpendicular/parallel/concentric）由 `detect/restore` 自动维持，不进入显式表；其余经 `addConstraint` 登记后由数值求解器解到满足。**映射表是唯一事实来源**，禁止在实现中私自增减语义。

---

## 7. 特性开关与回落策略

- **设置键**：`kConstraintSolver = "solver/constraintEnabled"`（`AppSettings.h:52`）。
- **默认值**：`specTable` 登记 `{ kConstraintSolver, true, boolVal }`（`AppSettings.cpp:183`），即**默认开启**。
- **访问器**：`constraintSolverEnabled()` / `setConstraintSolverEnabled(bool)`（`AppSettings.h:166` 声明，`.cpp:500-501` 实现），基于 `value()/setValue()` 统一校验回落。
- **注入时机**：`MainWindow` 构造并持有 `AppSettings*`，构造末尾调用 `ConstraintSolverFacade::instance().setSettings(appSettings_)`（`MainWindow.cpp:291`）。
- **回落矩阵**：

  | 情形 | 行为 |
  |------|------|
  | 特性开 + 活动实现 = Legacy（Phase B 默认） | 调 Legacy，几何 = 修复模型（与「关」等价，零回归） |
  | 特性关 | 直接 `fallbackFn()` → Legacy，几何行为不变 |
  | 特性开 + 活动实现抛 `std::exception`（Phase A 适配器故障） | 捕获 → 回落 Legacy + `warn` 日志，用户稿不受损 |
  | 无 `AppSettings` 注入 | `featureEnabled()` 返回 `true`（开启） |

- **测试覆盖**：`t_sktIntegration` 第 5 组显式 `setConstraintSolverEnabled(false)` → 验证「关→回落修复模型仍可用」；第 6 组验证非法 `Symmetric` 缺轴错误经门面非空透传。

---

## 8. 容差策略与一致性约束

- **命名常量**：`geo::kConstraintDetectTol`（= `0.5`，米制场景的隐式约束检测/几何判定容差），在 `Facade` 头经 `#include "Geometry.h"` 引入，`solveAfterChange` / `diagnose` 默认参数即 `double tol = geo::kConstraintDetectTol`。
- **禁魔数散布**：所有路由默认容差统一引用该命名常量，新增实现不得内联 `0.5` 等字面量；几何变化的判定阈值（如 Symmetric 镜像后坐标偏差 `< 0.5`、Dimensional 长度解算偏差 `< 0.5`）在自测中显式断言，与设计容差同源。
- **一致性约束**：`solveAll` 与 `diagnose` 必须给出自洽的 DOF 视图（`ConstraintDiagnosis::totalDof` / `freeDof` / `overDof`），自测第 4 组断言 `rep.failures.empty() && diag.totalDof >= 0`（注意结构体无 `dof` 成员，早期误用已修正为 `totalDof`）。

---

## 9. 验收对照与 Phase A SKT 移植路径

### 9.1 验收对照（Phase B，已达成）

| 验收项 | 结果 |
|--------|------|
| 双目标零警告构建（`SketchCAD` + `SketchSelfTest`） | `BUILD_EXIT=0`，`warning:` 计数 0、`FAILED` 计数 0 |
| 自测整体 | `TEST_EXIT=0`，**通过 2339 / 失败 0** |
| `t_sktIntegration` 断言数 | 16 条 CHECK，全部计入 0 失败 |
| 约束映射路由覆盖 | Coincident / Symmetric+镜像 / Dimensional / Horizontal / Vertical / Fixed / Distance / Angle（8 条） |
| 诊断路由覆盖 | UnderConstrained / WellConstrained / 冲突定位 / solveAll+diagnose 一致（4 条） |
| 联动路由覆盖 | 标注联动 `link+setDimensionValue`（几何 10→25mm，数值对齐 `1e-6`）、`driveParameter`（半径 5→12mm，`1e-3`） |
| 特性开关 | 关→回落修复模型仍可用；非法约束错误透传 |
| 存档格式 / `ElementType` / `ConstraintRecord` | 未改动，零回归面 |

### 9.2 Phase A：SKT 移植 spike 结果（已执行 → 建议 NO-GO）

对 avcad 主项目 `01-avticationCAD` 的 `SketchEngine` 求解核做了真实可行性探查（定位 `include/workbench/partdesign/SketchEngine.h` 与 `src/workbench/partdesign/SketchEngine.cpp`，逐层读取依赖链）。

**1) 编译器可移植性（MinGW 13.1）：结论 = 可行**
| 依赖 | 内容 | MinGW 判定 |
|------|------|-----------|
| `core/common/Global.h` | `UUID=std::string`、`Vector=std::vector`、各智能指针/容器全为 STL 别名；`AVCAD_API/CORE_API` 用 `__declspec(dllexport)` | **可移植**（GCC 在 Windows 目标支持 `__declspec`） |
| `core/common/Signal.h` | 仅 `std::function` + `std::mutex` | **可移植** |
| `SketchPlane.h` | 纯枚举，零依赖 | **可移植** |
| `SketchEngine.cpp::solve()` | Gauss-Seidel(80 轮) + Levenberg-Marquardt 混合，纯数学运算于 `std::vector`/`std::string` 对象模型 | **可移植** |
| partdesign 头文件 OCCT/Eigen/Qt/Windows.h 扫描 | `grep` 全空 | **无平台耦合** |

**2) 集成耦合性（是否应接入）：结论 = 不值得 / 建议 NO-GO**

| 维度 | 评估 |
|------|------|
| **功能目标已达成** | 本仓库既有 `ConstraintSolver` 已实现「改标注值 → 几何全图联动」（`t_sktIntegration` 16 CHECK 含 `linkDimension+setDimensionValue`→几何 25mm、`driveParameter`→半径 12mm）。Phase A 不带来新的用户可见能力。 |
| **算法同级而非更优** | avcad 求解器为「Gauss-Seidel + LM 残差/DOF」类，与本仓库现有 LM+雅可比求解器**同属一类**，并非符号/配置型等更强方法，边际收益极低。 |
| **约束枚举不对称** | avcad `SketchConstraintType`（18 种：含 `Collinear`/`PointOnCurve`/`ArcLength`/`Diameter`/`Radius`/`Fix`）≠ 本仓库 `ConstraintType`（21 种：含 `Fixed`/`FixedPoint`/`PointOnLine`/`PointOnCircle`/`Midpoint`/`SymmetricPoint`/`EqualLength`/`EqualRadius`/`Dimensional`）。二者**无 1:1 对应**，双向翻译有损且为长期维护负担。 |
| **工作平面/基准面耦合** | `DrawingWorkplane.h` 引入 `core/geometry/DatumTypes.h` + `GeometryTypes.h`，其 `.cpp` 用 OpenCASCADE `gp_Vec` 做任意工作平面正交化。接入 `SketchEngine` 会把 avcad 建模框架一片（DatumManager/DatumPlane/OCCT）拖入 sketch-cad-cpp，违背「只增不改、不引入重依赖」的约束精神。 |
| **适配器代价** | `SktConstraintSolver` 需把 `SketchModel`/`Element`/`ConstraintRecord` 整张重建为 avcad `Sketch`/`SketchElement`/`SketchConstraint`（含 19 个几何工厂、航空模板、DXF/翼型导入、阵列、`Signal` 事件、`DatumManager` 耦合），只为调用 `solve()` 再写回坐标——为单一方法付出过大表面积。 |

**3) 最终决策**
- **Phase A 不实施**：avcad `SketchEngine` 的求解数学本身可 MinGW 编译，但接入 sketch-cad-cpp 的成本/收益为负，**不建议移植**。
- **Phase B 的 Facade + Legacy 即为永久架构**：`ConstraintSolverFacade` 预留的 `setActiveSolver()` 接缝保留，未来若需更强求解器，应优先在 sketch-cad-cpp 内**原生新写独立 `SketchSolver`**（复用本仓库 `Element`/`ConstraintRecord`/`geo::` 数学，零 avcad 依赖），经同一接缝注入——而非搬运 `SketchEngine`。
- 主线不受阻：现有修复模型（`ConstraintSolver`）经 `LegacyConstraintSolver` 委托 + `Facade` 路由，已满足全部验收项（第 9.1 节）。

> 注：本探查未做实际跨项目编译（仅静态依赖审计 + 算法核对），因第 2 节耦合性结论已足以判定不值得。若后续确有「换更强求解内核」需求，应走「本仓库原生重写 `SketchSolver`」路线，届时复用本 Facade 接缝，等价性自测基线 = `LegacyConstraintSolver`。
