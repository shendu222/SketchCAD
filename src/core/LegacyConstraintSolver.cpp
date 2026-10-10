#include "LegacyConstraintSolver.h"
#include "ConstraintSolver.h"

namespace SketchCAD {

std::vector<int> LegacyConstraintSolver::solveAfterChange(
    const std::vector<Element>& beforeElems, std::vector<Element>& afterElems,
    int changedId, double tol) const {
    return ConstraintSolver::solveAfterChange(beforeElems, afterElems, changedId, tol);
}

SolveReport LegacyConstraintSolver::solveAll(SketchModel& m, int maxIterations) const {
    return ConstraintSolver::solveAll(m, maxIterations);
}

ConstraintDiagnosis LegacyConstraintSolver::diagnose(const SketchModel& m) const {
    return ConstraintSolver::diagnose(m);
}

QString LegacyConstraintSolver::addConstraint(SketchModel& m, ConstraintType type,
        int idA, int idB, double value, int axisId, int idxA, int idxB) const {
    return ConstraintSolver::addConstraint(m, type, idA, idB, value, axisId, idxA, idxB);
}

QString LegacyConstraintSolver::linkDimension(SketchModel& m, int dimId, const QString& paramRef) const {
    return ConstraintSolver::linkDimension(m, dimId, paramRef);
}

QString LegacyConstraintSolver::setDimensionValue(SketchModel& m, int dimId, double value) const {
    return ConstraintSolver::setDimensionValue(m, dimId, value);
}

QString LegacyConstraintSolver::driveParameter(SketchModel& m, const QString& param, double val) const {
    return ConstraintSolver::driveParameter(m, param, val);
}

} // namespace SketchCAD
