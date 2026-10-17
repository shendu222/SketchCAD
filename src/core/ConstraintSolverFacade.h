#pragma once
#include <vector>
#include <QString>
#include <QVector>
#include "Element.h"
#include "Geometry.h"              // geo::kConstraintDetectTol（命名容差常量，禁魔数）
#include "ConstraintSolver.h"      // SolveReport / ConstraintDiagnosis
#include "IConstraintSolver.h"
#include "LegacyConstraintSolver.h"  // 内置回落实现（成员 legacy_ 需要完整类型）
#include "VarParam.h"              // P8 变量表达式层（会话级变量表 + 轻量安全求值器）

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

    // ==================== P8 变量表达式层（参数化设计，Ch14）====================
    // 变量表为**会话级**（与门面同生命周期）：变量 → 表达式/数值；模型侧经既有
    // paramRef / dimParam 关联。驱动链路：改变量 → 拓扑重算依赖变量 → 对已关联
    // 变量走**既有 driveParameter**（重解约束 + syncDimensionValues 刷新标注）。
    //
    // FORBID 遵守：不改动既有约束求解语义——变量层只「算值」，应用一律委托既有
    // driveParameter；ESCALATE G6：无语义耦合，不触发停止线（已核 driveParameter 内部
    // 行为零改动，仅新增调用方）。
    //
    // 退化约定：未定义变量 / 循环依赖 / 除零 / 表达式变量直驱 → 返回非空原因并
    // 经 model.log(warn) 记录，且**不改动任何变量值与几何**。

    // 定义变量 name = expr。常量表达式定义期即求值并转为「值变量」；含变量引用为
    // 「表达式变量」。空串=成功，否则为原因（名非法/语法错/除零）。
    QString defineVariable(const QString& name, const QString& expr);
    // 把变量设为纯数值（清除表达式定义）。空串=成功。
    QString setVariableValue(const QString& name, double val);

    // 驱动：设 name=val → 拓扑重算依赖变量 → 已关联变量经既有 driveParameter 重解。
    // 空串=成功；失败原因见上方退化约定。驱动同名/同值（无变化）→ 成功但不重解。
    QString drive(SketchModel& m, const QString& name, double val);

    // 查询
    bool        hasVariable(const QString& name) const;
    QString     variableExpression(const QString& name) const;  // 定义原文（未定义=空串）
    bool        isExpressionVariable(const QString& name) const;// 含变量引用的表达式变量
    double      variableValue(const QString& name) const;       // 当前值（NaN=无定义/求值失败）
    QStringList variableNames() const;                           // 定义顺序
    void        clearVariables();
    const VarParamTable& variableTable() const { return vars_; }

    // ---- 显示开关：尺寸表达式 / 附加尺寸 / 过约束尺寸（会话级，默认全开）----
    struct VarDisplaySwitches {
        bool showExpression  = true;   // 尺寸文本优先显示关联变量的表达式原文
        bool showAdditional  = true;   // 标记「变量层附加」（参数关联到已定义变量）尺寸
        bool overconstrained = true;   // 过约束尺寸标识 + 驱动后过约束告警日志
    };
    void               setVarDisplaySwitches(const VarDisplaySwitches& s) { switches_ = s; }
    VarDisplaySwitches varDisplaySwitches() const { return switches_; }

    // 尺寸显示信息（只读，按开关组装；dimId 非尺寸标注 → isDimension=false）
    struct DimensionDisplayInfo {
        bool    isDimension    = false;
        QString valueText;       // 数值文本；showExpression 开且有关联表达式 → 表达式文本
        QString expressionText;  // 关联变量的定义原文（无关联/未定义为空）
        bool    additional      = false; // 参数关联到已定义变量（showAdditional 开才置位）
        bool    overconstrained = false; // 参与过约束/冲突（overconstrained 开才置位）
    };
    DimensionDisplayInfo dimensionDisplayInfo(const SketchModel& m, int dimId) const;

    // 过约束尺寸 id 列表（开关关 → 空；经既有 diagnose 只读判定，保元素序遍历序）
    QVector<int> overconstrainedDimensionIds(const SketchModel& m) const;

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
    VarParamTable     vars_;                  // P8 会话级变量表（表达式/数值）
    VarDisplaySwitches switches_;             // P8 显示开关（尺寸表达式/附加尺寸/过约束）
};

} // namespace SketchCAD
