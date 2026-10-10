#pragma once
#include "IConstraintSolver.h"

namespace SketchCAD {

// 把抽象接口的方法调用 1:1 转发给现有 ConstraintSolver（即「修复模型」）。
//
// 这是特性开关关闭时的回落实现，也是 Phase 2 Skt 适配器的对照基线：二者必须满足
// 「同一组输入 → 等价几何结果」的契约，自测 t_sktIntegration 据此校验集成缝正确性。
// 本类保持无状态：所有几何变更都发生在传入的 SketchModel / Element 容器上。
class LegacyConstraintSolver : public IConstraintSolver {
public:
    std::vector<int> solveAfterChange(const std::vector<Element>& beforeElems,
                                      std::vector<Element>& afterElems,
                                      int changedId, double tol) const override;
    SolveReport solveAll(SketchModel& m, int maxIterations) const override;
    ConstraintDiagnosis diagnose(const SketchModel& m) const override;
    QString addConstraint(SketchModel& m, ConstraintType type, int idA, int idB,
                          double value, int axisId, int idxA, int idxB) const override;
    QString linkDimension(SketchModel& m, int dimId, const QString& paramRef) const override;
    QString setDimensionValue(SketchModel& m, int dimId, double value) const override;
    QString driveParameter(SketchModel& m, const QString& param, double val) const override;
};

} // namespace SketchCAD
