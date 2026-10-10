#include "ConstraintSolverFacade.h"
#include "AppSettings.h"
#include "SketchModel.h"   // 回落日志需要完整类型（m->log）
#include <exception>
#include <string>

namespace SketchCAD {

ConstraintSolverFacade::ConstraintSolverFacade()
    : active_(&legacy_) {
}

ConstraintSolverFacade& ConstraintSolverFacade::instance() {
    static ConstraintSolverFacade inst;
    return inst;
}

void ConstraintSolverFacade::setSettings(AppSettings* s) { settings_ = s; }

void ConstraintSolverFacade::setActiveSolver(IConstraintSolver* solver) {
    active_ = (solver != nullptr) ? solver : &legacy_;
}

bool ConstraintSolverFacade::featureEnabled() const {
    if (!settings_) return true;   // 无设置注入 → 默认开启
    return settings_->constraintSolverEnabled();
}

// 路由：特性关 → 回落 Legacy；特性开 → 活动实现，异常则回落 Legacy 并日志。
// route 仅在本翻译单元内实例化（调用点都在本文件），故模板定义放在 .cpp 即可。
template<typename R, typename AF, typename FF>
R ConstraintSolverFacade::route(SketchModel* m, const char* op,
                                AF&& activeFn, FF&& fallbackFn) const {
    if (!featureEnabled()) {
        return fallbackFn();
    }
    try {
        return activeFn();
    } catch (const std::exception& e) {
        if (m) {
            m->log(QStringLiteral("约束求解「%1」异常，已回落修复模型：%2")
                     .arg(QLatin1String(op)).arg(QString::fromUtf8(e.what())),
                   QStringLiteral("warn"));
        }
        return fallbackFn();
    }
}

std::vector<int> ConstraintSolverFacade::solveAfterChange(
    const std::vector<Element>& beforeElems, std::vector<Element>& afterElems,
    int changedId, double tol) const {
    return route<std::vector<int>>(
        nullptr, "solveAfterChange",
        [&]() { return active_->solveAfterChange(beforeElems, afterElems, changedId, tol); },
        [&]() { return legacy_.solveAfterChange(beforeElems, afterElems, changedId, tol); });
}

SolveReport ConstraintSolverFacade::solveAll(SketchModel& m, int maxIterations) const {
    return route<SolveReport>(
        &m, "solveAll",
        [&]() { return active_->solveAll(m, maxIterations); },
        [&]() { return legacy_.solveAll(m, maxIterations); });
}

ConstraintDiagnosis ConstraintSolverFacade::diagnose(const SketchModel& m) const {
    // diagnose 只读：const_cast 仅用于回落路径的日志；活动/回落实现本身不改 model。
    return route<ConstraintDiagnosis>(
        const_cast<SketchModel*>(&m), "diagnose",
        [&]() { return active_->diagnose(m); },
        [&]() { return legacy_.diagnose(m); });
}

QString ConstraintSolverFacade::addConstraint(SketchModel& m, ConstraintType type,
        int idA, int idB, double value, int axisId, int idxA, int idxB) const {
    return route<QString>(
        &m, "addConstraint",
        [&]() { return active_->addConstraint(m, type, idA, idB, value, axisId, idxA, idxB); },
        [&]() { return legacy_.addConstraint(m, type, idA, idB, value, axisId, idxA, idxB); });
}

QString ConstraintSolverFacade::linkDimension(SketchModel& m, int dimId, const QString& paramRef) const {
    return route<QString>(
        &m, "linkDimension",
        [&]() { return active_->linkDimension(m, dimId, paramRef); },
        [&]() { return legacy_.linkDimension(m, dimId, paramRef); });
}

QString ConstraintSolverFacade::setDimensionValue(SketchModel& m, int dimId, double value) const {
    return route<QString>(
        &m, "setDimensionValue",
        [&]() { return active_->setDimensionValue(m, dimId, value); },
        [&]() { return legacy_.setDimensionValue(m, dimId, value); });
}

QString ConstraintSolverFacade::driveParameter(SketchModel& m, const QString& param, double val) const {
    return route<QString>(
        &m, "driveParameter",
        [&]() { return active_->driveParameter(m, param, val); },
        [&]() { return legacy_.driveParameter(m, param, val); });
}

} // namespace SketchCAD
