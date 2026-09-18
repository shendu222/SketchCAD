#include "ConstraintSolver.h"
#include "Geometry.h"
#include <algorithm>
#include <unordered_set>

namespace SketchCAD {

// 需要检测的约束类型列表
static const ConstraintType kDetectableTypes[] = {
    ConstraintType::Tangent,
    ConstraintType::Perpendicular,
    ConstraintType::Parallel
};
static const int kNumTypes = 3;

// 查找图元列表中指定 id 的图元索引
static int findIndex(const std::vector<Element>& elems, int id) {
    for (size_t i = 0; i < elems.size(); ++i)
        if (elems[i].id == id) return (int)i;
    return -1;
}

std::vector<ConstraintSolver::ConstraintPair>
ConstraintSolver::detectConstraints(const std::vector<Element>& elems,
                                    const std::vector<int>& changedIds,
                                    double tol) {
    std::vector<ConstraintPair> result;
    // 只检测 changedIds 中的图元与其他图元之间的约束
    // 未在 changedIds 中的图元对之间的约束不受本次修改影响，无需恢复
    for (int changedId : changedIds) {
        int idx = findIndex(elems, changedId);
        if (idx < 0) continue;
        const Element& changed = elems[idx];

        for (size_t j = 0; j < elems.size(); ++j) {
            if ((int)j == idx) continue;
            const Element& other = elems[j];
            if (!other.visible) continue;
            if (other.type == ElementType::Constraint || other.type == ElementType::Dimension) continue;

            for (int k = 0; k < kNumTypes; ++k) {
                ConstraintType ct = kDetectableTypes[k];
                if (geo::checkConstraint(changed, other, ct, tol)) {
                    result.push_back({changedId, other.id, ct});
                }
            }
        }
    }
    return result;
}

std::vector<int>
ConstraintSolver::restoreConstraints(std::vector<Element>& elems,
                                     const std::vector<ConstraintPair>& constraints,
                                     int changedId,
                                     double tol) {
    std::vector<int> modified;
    std::unordered_set<int> modifiedSet;

    // 已处理队列：用于多级传播
    std::vector<int> toProcess;
    toProcess.push_back(changedId);

    int iterations = 0;
    const int maxIterations = 10; // 防止无限循环

    while (!toProcess.empty() && iterations < maxIterations) {
        ++iterations;
        int currentId = toProcess.back();
        toProcess.pop_back();

        int currentIdx = findIndex(elems, currentId);
        if (currentIdx < 0) continue;

        for (const auto& cp : constraints) {
            // 只处理以 currentId 为主动方的约束
            if (cp.idA != currentId) continue;

            int bIdx = findIndex(elems, cp.idB);
            if (bIdx < 0) continue;

            Element& a = elems[currentIdx];
            Element& b = elems[bIdx];

            // 检查约束是否被破坏
            if (geo::checkConstraint(a, b, cp.ct, tol)) continue;

            // 尝试恢复约束
            if (geo::restoreConstraint(a, b, cp.ct, tol)) {
                if (modifiedSet.find(cp.idB) == modifiedSet.end()) {
                    modified.push_back(cp.idB);
                    modifiedSet.insert(cp.idB);
                    // 将被动方加入处理队列，实现多级传播
                    toProcess.push_back(cp.idB);
                }
            }
        }
    }

    return modified;
}

std::vector<int>
ConstraintSolver::solveAfterChange(const std::vector<Element>& beforeElems,
                                    std::vector<Element>& afterElems,
                                    int changedId,
                                    double tol) {
    // 修改前检测约束
    auto constraints = detectConstraints(beforeElems, {changedId}, tol);

    // 修改后恢复约束
    return restoreConstraints(afterElems, constraints, changedId, tol);
}

} // namespace SketchCAD
