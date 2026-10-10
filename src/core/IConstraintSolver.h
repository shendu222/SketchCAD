#pragma once
#include <vector>
#include <QString>
#include "Element.h"
#include "ConstraintSolver.h"   // 复用既有 SolveReport / ConstraintDiagnosis / ConstraintType

namespace SketchCAD {

class SketchModel;

// ===========================================================================
// 约束求解器抽象层（SKT 集成的 Phase B 接口）
// ---------------------------------------------------------------------------
// 设计目标：把「调用约束求解」与「具体求解实现」解耦。当前内置两种实现：
//   * LegacyConstraintSolver —— 委托现有 ConstraintSolver（即任务所指「修复模型」），
//     作为特性关闭时的回落实现，也是 Phase 2 Skt 适配器的对照基线；
//   * （Phase 2）SktConstraintSolver —— 接入 avcad 的 SketchEngine 求解核，
//     经 ConstraintSolverFacade 作为可切换实现注入。
//
// 抽象接口的方法集与现有 ConstraintSolver 的静态方法一一对应（除纯只读辅助），
// 现有显式约束表的 ConstraintType 与求解器约束类型 1:1 映射（见文件末尾映射表）。
// 任何新增实现都必须覆盖同一组约束类型，保证存档/行为向后兼容。
// ===========================================================================
class IConstraintSolver {
public:
    virtual ~IConstraintSolver() = default;

    // 图元参数修改后，检测并恢复修改前满足的「隐式约束」（相切/垂直/平行/同心）。
    // 语义与 ConstraintSolver::solveAfterChange 完全一致。
    virtual std::vector<int> solveAfterChange(const std::vector<Element>& beforeElems,
                                              std::vector<Element>& afterElems,
                                              int changedId,
                                              double tol) const = 0;

    // 求解全部「显式约束」（迭代多轮处理约束链），返回求解报告。
    virtual SolveReport solveAll(SketchModel& m, int maxIterations) const = 0;

    // 逐实体欠约束/过约束诊断（只读）。
    virtual ConstraintDiagnosis diagnose(const SketchModel& m) const = 0;

    // 添加一条显式约束；成功返回空串，失败返回原因（可直接 log 给用户）。
    virtual QString addConstraint(SketchModel& m, ConstraintType type,
                                  int idA, int idB, double value,
                                  int axisId, int idxA, int idxB) const = 0;

    // 尺寸标注 ↔ 几何约束 双向关联（尺寸驱动）。
    virtual QString linkDimension(SketchModel& m, int dimId, const QString& paramRef) const = 0;

    // 设定标注驱动值并立即重解几何。
    virtual QString setDimensionValue(SketchModel& m, int dimId, double value) const = 0;

    // 参数表联动驱动。
    virtual QString driveParameter(SketchModel& m, const QString& param, double val) const = 0;
};

// ---------------------------------------------------------------------------
// 约束类型 ↔ 求解器约束 映射表（单一事实来源，新增实现须实现全部）
// ---------------------------------------------------------------------------
// 现有 ConstraintType（Element.h）与求解器内部约束 1:1 对应：
//   Coincident       → 端点/点重合（点基约束，idxA/idxB 控制点索引）
//   Parallel         → 两直线平行
//   Perpendicular    → 两直线垂直
//   Equal            → 等长/等径（由图元几何类型自判）
//   Tangent          → 直线↔圆 / 圆↔圆 相切
//   Horizontal       → 直线水平
//   Vertical         → 直线竖直
//   Fixed            → 固定图元（主动方与 Fixed 不被移动）
//   Concentric       → 两圆/弧同心
//   Symmetric        → 关于 axisId 直线对称（两图元）
//   Dimensional      → 尺寸（长度/半径），目标值 = value (mm)
//   EqualLength      → 两线段等长
//   EqualRadius      → 两圆/弧等径
//   PointOnLine      → 点落在直线上（点基，idxA 指定点）
//   PointOnCircle    → 点落在圆上（点基，idxA 指定点）
//   FixedPoint       → 指定控制点固定（idxA 控制点索引）
//   Midpoint         → 点在另一图元中点（点基）
//   Distance         → 两点/图元间距离 = value (mm)
//   Angle            → 两直线夹角 = value (°)
//   SymmetricPoint   → 点关于 axisId 直线对称（点基）
// 隐式约束（tangent/perpendicular/parallel/concentric）由 detect/restore 自动维持；
// 其余为显式声明，经 addConstraint 登记后由数值求解器解到满足。
// 新增求解实现必须对上述每种类型产生等价几何结果，否则视为不兼容、禁止注入为活动实现。
// ---------------------------------------------------------------------------

} // namespace SketchCAD
