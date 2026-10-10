#pragma once
#include <vector>
#include <QString>
#include "Element.h"
#include "Geometry.h"              // geo::kConstraintDetectTol（命名容差常量，禁魔数）
#include "ConstraintSolver.h"      // SolveReport / ConstraintDiagnosis
#include "IConstraintSolver.h"
#include "LegacyConstraintSolver.h"  // 内置回落实现（成员 legacy_ 需要完整类型）

namespace SketchCAD {

class AppSettings;

// ===========================================================================
// 约束求解门面（单例）：统一路由「调用点 → 活动求解实现」，承载特性开关与回落逻辑。
// ===========================================================================
// 路由的三个调用点：
//   * SketchModel::updateElementLinked  —— solveAfterChange + solveAll
//   * ConstraintBar::refresh            —— diagnose
//   * Commands（constraint 命令）       —— addConstraint / linkDimension /
//                                          setDimensionValue / driveParameter
//
// 特性开关 setting keys/constraintSolver（默认开）：
//   * 开  → 调用当前活动实现（默认 LegacyConstraintSolver，Phase 2 可注入 Skt 适配器）；
//   * 关  → 直接回落到 LegacyConstraintSolver（修复模型），几何行为不变；
//   * 若活动实现在求解中抛 std::exception（Phase 2 适配器异常保护），捕获后回落
//     Legacy 并通过 model.log 记录，几何结果仍由修复模型保证。
//
// 由于 Phase B 的活动实现即 Legacy 本身，开关「开」与「回落」在几何结果上等价，
// 保证零行为回归；门面仅作为可切换接缝与审计边界存在，便于 Phase 2 平滑接入。
// ===========================================================================
class ConstraintSolverFacade {
public:
    static ConstraintSolverFacade& instance();

    // 注入应用设置（MainWindow 构造 AppSettings 后调用一次）。nullptr → 特性默认开启。
    void setSettings(AppSettings* s);
    // 设置当前活动求解实现（Phase 2 注入 Skt 适配器；默认指向内置 Legacy）。
    void setActiveSolver(IConstraintSolver* solver);
    // 特性是否开启（无 settings 注入时默认 true）。
    bool featureEnabled() const;

    // ---- 与 IConstraintSolver 对齐的路由方法（调用点统一经此入口）----
    std::vector<int> solveAfterChange(const std::vector<Element>& beforeElems,
                                      std::vector<Element>& afterElems,
                                      int changedId,
                                      double tol = geo::kConstraintDetectTol) const;
    SolveReport solveAll(SketchModel& m, int maxIterations = 6) const;
    ConstraintDiagnosis diagnose(const SketchModel& m) const;
    QString addConstraint(SketchModel& m, ConstraintType type, int idA, int idB,
                          double value = 0.0, int axisId = -1, int idxA = -1, int idxB = -1) const;
    QString linkDimension(SketchModel& m, int dimId, const QString& paramRef = QString()) const;
    QString setDimensionValue(SketchModel& m, int dimId, double value) const;
    QString driveParameter(SketchModel& m, const QString& param, double val) const;

private:
    ConstraintSolverFacade();
    ConstraintSolverFacade(const ConstraintSolverFacade&) = delete;
    ConstraintSolverFacade& operator=(const ConstraintSolverFacade&) = delete;

    // 异常保护路由：特性关→直接回落 Legacy；特性开→调活动实现，若抛异常则回落
    // Legacy 并经 model.log 记录。返回值类型 R 由调用处显式指定。
    template<typename R, typename AF, typename FF>
    R route(SketchModel* m, const char* op, AF&& activeFn, FF&& fallbackFn) const;

    AppSettings* settings_ = nullptr;
    IConstraintSolver* active_ = nullptr;     // 当前活动实现（默认 = &legacy_）
    LegacyConstraintSolver legacy_;           // 内置回落（修复模型）
};

} // namespace SketchCAD
