#include "ConstraintSolver.h"
#include "Geometry.h"
#include "Snap.h"
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

// ---------------------------------------------------------------------------
// 几何关系的「自动计算 + 生成」
// ---------------------------------------------------------------------------
namespace {
// 构造一条 Line 图元（ctype/ref 由调用方填）
Element makeGenLine(const Vec3& a, const Vec3& b, ConstraintType ct, int refA, int refB) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    e.name = QStringLiteral("约束生成");
    e.ctype = ct;
    e.refA = refA;
    e.refB = refB;
    return e;
}
// 圆/弧的圆心与半径（非圆类返回 false）
bool circleLike(const Element& e, Vec3& c, double& r) {
    if (e.type != ElementType::Circle && e.type != ElementType::Arc) return false;
    if (e.pts.empty()) return false;
    c = e.pts[0]; r = e.radius; return true;
}
} // namespace

Element ConstraintSolver::drawPerpendicularTo(const Element& refLine, const Vec3& P) {
    // 参照方向：直线/线段取 (pts[1]-pts[0])；其余类型退化取 X 轴
    Vec3 dir;
    if (refLine.pts.size() >= 2) dir = (refLine.pts[1] - refLine.pts[0]).normalized();
    else dir = Vec3(1, 0, 0);
    // 垂线方向 = 参照方向逆时针转 90°
    const Vec3 perp = dir.perp().normalized();
    const Vec3 a = P - perp * 1.0;   // 以 P 为中点的单位长垂线（长度后续可由用户拉伸）
    const Vec3 b = P + perp * 1.0;
    return makeGenLine(a, b, ConstraintType::Perpendicular, refLine.id, -1);
}

int ConstraintSolver::commonTangents(const Element& cA, const Element& cB, std::vector<Element>& out) {
    out.clear();
    Vec3 ca, cb; double ra, rb;
    if (!circleLike(cA, ca, ra) || !circleLike(cB, cb, rb)) return 0;
    const Vec3 d = cb - ca;
    const double D = d.len2D();
    if (D < 1e-9) return 0;                       // 同心：无公切线
    const double base = std::atan2(d.y, d.x);     // 中心连线角（弧度）

    // 外切线：两切点半径同向（角为 base ± alpha，cos alpha = (ra - rb)/D）
    if (D > std::fabs(ra - rb) + 1e-9) {
        const double alpha = std::acos(std::min(1.0, std::max(-1.0, (ra - rb) / D)));
        for (double s : {+1.0, -1.0}) {
            const double ang = base + s * alpha;
            const Vec3 tA = ca + Vec3(ra * std::cos(ang), ra * std::sin(ang), 0);
            const Vec3 tB = cb + Vec3(rb * std::cos(ang), rb * std::sin(ang), 0);
            out.push_back(makeGenLine(tA, tB, ConstraintType::Tangent, cA.id, cB.id));
        }
    }
    // 内切线：两切点半径反向（角相差 180°，cos alpha = (ra + rb)/D）
    if (D > ra + rb + 1e-9) {
        const double alpha = std::acos(std::min(1.0, std::max(-1.0, (ra + rb) / D)));
        for (double s : {+1.0, -1.0}) {
            const double angA = base + s * alpha;
            const double angB = angA + 3.141592653589793;       // 内切线在 B 侧取反向点（交叉）
            const Vec3 tA = ca + Vec3(ra * std::cos(angA), ra * std::sin(angA), 0);
            const Vec3 tB = cb + Vec3(rb * std::cos(angB), rb * std::sin(angB), 0);
            out.push_back(makeGenLine(tA, tB, ConstraintType::Tangent, cA.id, cB.id));
        }
    }
    return (int)out.size();
}

Element ConstraintSolver::quadrantConnector(const Element& target, const Vec3& from) {
    Vec3 c; double r;
    if (!circleLike(target, c, r)) {
        // 非圆类：直接连到其首个锚点
        const Vec3 p = target.pts.empty() ? from : target.pts[0];
        return makeGenLine(from, p, ConstraintType::Coincident, target.id, -1);
    }
    // 取离 from 最近的象限点
    const auto qs = snap::quadrantPoints(c, r, 0, 0, false);
    Vec3 best = qs.empty() ? c : qs.front();
    double bd = best.distTo(from);
    for (const auto& q : qs) { const double d = q.distTo(from); if (d < bd) { bd = d; best = q; } }
    return makeGenLine(from, best, ConstraintType::Coincident, target.id, -1);
}

} // namespace SketchCAD
