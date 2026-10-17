#include "ConstraintSolverFacade.h"
#include "AppSettings.h"
#include "SketchModel.h"   // 回落日志需要完整类型（m->log）
#include <QSet>
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

// ==================== P8 变量表达式层 ====================
namespace {

// 变量是否已被模型关联（约束 paramRef 或标注 dimParam 命中）——未关联变量不进入模型应用
bool isParamLinkedInModel(const SketchModel& m, const QString& name) {
    for (const auto& r : m.constraints())
        if (r.paramRef == name) return true;
    for (const auto& e : m.elements())
        if (e.type == ElementType::Dimension && e.dimParam == name) return true;
    return false;
}

} // namespace

QString ConstraintSolverFacade::defineVariable(const QString& name, const QString& expr) {
    return vars_.define(name, expr);
}

QString ConstraintSolverFacade::setVariableValue(const QString& name, double val) {
    return vars_.setValue(name, val);
}

QString ConstraintSolverFacade::drive(SketchModel& m, const QString& name, double val) {
    // ---- 退化：未定义变量 ----
    if (!vars_.has(name)) {
        const QString e = QStringLiteral("参数驱动失败：变量 %1 未定义（请先 defineVariable）").arg(name);
        m.log(e, QStringLiteral("warn"));
        return e;
    }
    // ---- 退化：表达式变量不能直驱（防止静默丢失表达式定义）----
    if (vars_.isExpression(name)) {
        const QString e = QStringLiteral("参数驱动失败：变量 %1 由表达式「%2」定义，不能直接驱动（请驱动其依赖变量，或用 defineVariable 重定义）")
                            .arg(name, vars_.expression(name));
        m.log(e, QStringLiteral("warn"));
        return e;
    }
    // ---- 设值 + 拓扑重算（循环依赖/除零/未定义引用 → 失败，零改动）----
    QVector<QPair<QString, double>> changed;
    QString err;
    if (!vars_.resolveDrive(name, val, changed, &err)) {
        m.log(QStringLiteral("参数驱动失败：%1").arg(err), QStringLiteral("warn"));
        return err;
    }
    if (changed.isEmpty()) {
        m.log(QStringLiteral("变量 %1 驱动值未变化（= %2），无需重解").arg(name).arg(val),
              QStringLiteral("info"));
        return {};
    }

    // ---- 应用到模型：仅对已关联变量走**既有** driveParameter（重解 + 刷新标注）----
    // FORBID：不改求解语义——本层只选变量、算值，重解/刷新全部委托既有路径。
    int applied = 0;
    for (const auto& c : changed) {
        if (!isParamLinkedInModel(m, c.first)) continue;   // 未关联 → 跳过（中间变量常见，非错误）
        const QString e = driveParameter(m, c.first, c.second);
        if (!e.isEmpty()) {                               // 真错误（如模型异常回落）上抛
            m.log(e, QStringLiteral("warn"));
            return e;
        }
        ++applied;
    }

    // ---- 过约束尺寸开关：驱动后只读诊断 + 告警日志（不改几何）----
    if (switches_.overconstrained) {
        const QVector<int> over = overconstrainedDimensionIds(m);
        if (!over.isEmpty()) {
            QStringList ids;
            for (int id : over) ids << QString::number(id);
            m.log(QStringLiteral("过约束告警：尺寸 #%1 所在约束冗余/冲突（过约束尺寸开关已开，请移除矛盾或冗余约束）")
                     .arg(ids.join(QStringLiteral(", "))), QStringLiteral("warn"));
        }
    }

    if (applied == 0)
        m.log(QStringLiteral("变量 %1 = %2 已更新，但未关联任何模型标注").arg(name).arg(val),
              QStringLiteral("info"));
    else
        m.log(QStringLiteral("参数驱动：%1 个变量已应用并重解约束").arg(applied), QStringLiteral("info"));
    return {};
}

bool ConstraintSolverFacade::hasVariable(const QString& name) const {
    return vars_.has(name);
}

QString ConstraintSolverFacade::variableExpression(const QString& name) const {
    return vars_.expression(name);
}

bool ConstraintSolverFacade::isExpressionVariable(const QString& name) const {
    return vars_.isExpression(name);
}

double ConstraintSolverFacade::variableValue(const QString& name) const {
    return vars_.value(name);
}

QStringList ConstraintSolverFacade::variableNames() const {
    return vars_.names();
}

void ConstraintSolverFacade::clearVariables() {
    vars_.clear();
}

QVector<int> ConstraintSolverFacade::overconstrainedDimensionIds(const SketchModel& m) const {
    QVector<int> out;
    if (!switches_.overconstrained) return out;   // 开关关 → 不标识
    // 只读判定：过约束图元 → 其涉及约束 → 约束的 driverId（尺寸标注）
    const ConstraintDiagnosis diag = diagnose(m);
    const auto& recs = m.constraints();
    QSet<int> dimIds;
    for (int id : diag.overconstrainedIds) {
        for (int ci : ConstraintSolver::constraintsInvolving(m, id)) {
            if (ci < 0 || ci >= (int)recs.size()) continue;
            const int drv = recs[ci].driverId;
            if (drv >= 0) dimIds.insert(drv);
        }
    }
    for (const auto& e : m.elements())
        if (e.type == ElementType::Dimension && dimIds.contains(e.id)) out.push_back(e.id);
    return out;
}

ConstraintSolverFacade::DimensionDisplayInfo
ConstraintSolverFacade::dimensionDisplayInfo(const SketchModel& m, int dimId) const {
    DimensionDisplayInfo info;
    const Element* e = m.element(dimId);
    if (!e || e->type != ElementType::Dimension) return info;   // isDimension=false
    info.isDimension = true;

    const QString param = e->dimParam;
    const QString expr  = param.isEmpty() ? QString() : vars_.expression(param);
    info.expressionText = expr;

    // 数值文本：开「尺寸表达式」且关联变量有定义原文 → 显示表达式；否则显示数值
    if (switches_.showExpression && !expr.isEmpty())
        info.valueText = expr;
    else
        info.valueText = QString::number(e->dimValue, 'g', 6);

    // 附加尺寸：参数关联到已定义变量（开关控制是否标记）
    if (switches_.showAdditional && !param.isEmpty() && vars_.has(param))
        info.additional = true;

    // 过约束标识（开关控制）
    if (switches_.overconstrained)
        info.overconstrained = overconstrainedDimensionIds(m).contains(dimId);
    return info;
}

} // namespace SketchCAD
