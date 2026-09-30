#include "ConstraintSolver.h"
#include "Geometry.h"
#include "Snap.h"
#include "SketchModel.h"
#include <algorithm>
#include <unordered_set>
#include <map>
#include <set>
#include <cmath>
#include <limits>

namespace SketchCAD {

// 需要检测的约束类型列表
static const ConstraintType kDetectableTypes[] = {
    ConstraintType::Tangent,
    ConstraintType::Perpendicular,
    ConstraintType::Parallel,
    ConstraintType::Concentric      // 同心：两圆心重合，两图元即可判定，故可自动检测
};
static const int kNumTypes = 4;

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

// ===========================================================================
// 显式约束：添加 / 求解 / 冲突检测
// ===========================================================================
namespace {

const double kSolveTol = 0.5;   // 求解判定容差（mm）

bool isCircleLikeE(const Element& e) {
    return (e.type == ElementType::Circle || e.type == ElementType::Arc) && !e.pts.empty();
}
bool isLineLikeE(const Element& e) {
    return e.type == ElementType::Line && e.pts.size() >= 2;
}

// 图元的"自身尺寸"：直线 = 长度，圆/弧 = 半径。用于 Equal / Dimensional。
bool measureSize(const Element& e, double& v) {
    if (isLineLikeE(e))            { v = e.pts[0].distTo(e.pts[1]); return true; }
    if (isCircleLikeE(e))          { v = e.radius;                  return true; }
    return false;
}
// 把图元缩放到指定尺寸（直线：沿原方向改第二端点；圆/弧：改半径）
bool setSize(Element& e, double v) {
    if (v <= 0) return false;
    if (isLineLikeE(e)) {
        const Vec3 d = e.pts[1] - e.pts[0];
        const double L = d.len2D();
        if (L < 1e-12) return false;
        e.pts[1] = e.pts[0] + d * (v / L);
        return true;
    }
    if (isCircleLikeE(e)) { e.radius = v; return true; }
    return false;
}

// 点 p 关于直线 (a,b) 的镜像：先求垂足 foot，再取 P' = 2·foot − P
Vec3 mirrorPoint(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 d = b - a;
    const double L2 = d.x * d.x + d.y * d.y;
    if (L2 < 1e-18) return p;                       // 轴退化为一个点：镜像无定义，原样返回
    const double t = ((p.x - a.x) * d.x + (p.y - a.y) * d.y) / L2;
    const Vec3 foot(a.x + t * d.x, a.y + t * d.y, 0);
    return Vec3(2.0 * foot.x - p.x, 2.0 * foot.y - p.y, 0);
}

void rotatePtsAbout(std::vector<Vec3>& pts, const Vec3& pivot, double deg) {
    const double a = deg * M_PI / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    for (auto& p : pts) {
        const double dx = p.x - pivot.x, dy = p.y - pivot.y;
        p = Vec3(pivot.x + dx * c - dy * s, pivot.y + dx * s + dy * c, p.z);
    }
}

// 点基约束取用的"锚点"：圆/弧取圆心；否则取 idx 指定的控制点（idx<0 或越界取首点）
Vec3 anchorPoint(const Element& e, int idx) {
    if ((e.type == ElementType::Circle || e.type == ElementType::Arc) && !e.pts.empty())
        return e.pts[0];
    if (idx >= 0 && idx < (int)e.pts.size()) return e.pts[idx];
    return e.pts.empty() ? Vec3() : e.pts[0];
}

// 有向直线 (a,b) 到点 p 的带符号距离（二维）：|b-a|·sin(theta)
double signedDistPointToLine(const Vec3& p, const Vec3& a, const Vec3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double L = std::hypot(dx, dy);
    if (L < 1e-12) return p.distTo(a);
    return ((p.x - a.x) * dy - (p.y - a.y) * dx) / L;
}

// 两直线夹角（度，[0,180]），取方向向量
double angleBetweenLinesDeg(const Element& a, const Element& b) {
    const Vec3 da = a.pts[1] - a.pts[0];
    const Vec3 db = b.pts[1] - b.pts[0];
    const double dot = da.x * db.x + da.y * db.y;
    const double crs = da.x * db.y - da.y * db.x;
    double ang = std::atan2(std::fabs(crs), std::fabs(dot)) * 180.0 / M_PI;
    return ang;
}

// 图元"自身尺寸"：直线=长度，圆/弧=半径
double sizeOf(const Element& e) {
    double v = 0; return measureSize(e, v) ? v : 0.0;
}

// 类型相容性校验：返回空串表示可行，否则返回可直接呈现给用户的原因
QString compatError(ConstraintType ct, const Element* a, const Element* b,
                    const Element* axis, double value) {
    const QString na = QStringLiteral("#%1").arg(a ? a->id : -1);
    const QString nb = QStringLiteral("#%1").arg(b ? b->id : -1);
    switch (ct) {
        case ConstraintType::Parallel:
        case ConstraintType::Perpendicular:
        case ConstraintType::Collinear:
        case ConstraintType::EqualLength:
        case ConstraintType::Angle:
            if (!a || !isLineLikeE(*a) || !b || !isLineLikeE(*b))
                return QStringLiteral("「%1」约束要求两端均为直线：%2 或 %3 不是直线")
                       .arg(QString::fromUtf8(constraintTypeName(ct))).arg(na).arg(nb);
            return {};
        case ConstraintType::Tangent:
            if (!a || !b ||
                !((isLineLikeE(*a) && isCircleLikeE(*b)) || (isCircleLikeE(*a) && isLineLikeE(*b)) ||
                  (isCircleLikeE(*a) && isCircleLikeE(*b))))
                return QStringLiteral("「相切」约束要求「直线与圆」或「圆与圆」：%1 与 %2 的组合不支持").arg(na).arg(nb);
            return {};
        case ConstraintType::Concentric:
        case ConstraintType::EqualRadius:
            if (!a || !isCircleLikeE(*a) || !b || !isCircleLikeE(*b))
                return QStringLiteral("「%1」约束要求两端均为圆/弧：%1 或 %2 不是圆/弧")
                       .arg(QString::fromUtf8(constraintTypeName(ct))).arg(na).arg(nb);
            return {};
        case ConstraintType::Coincident:
            if (!a || a->pts.empty() || !b || b->pts.empty())
                return QStringLiteral("「重合」约束要求两端图元都有端点：%1 或 %2 无端点").arg(na).arg(nb);
            return {};
        case ConstraintType::Equal:
        case ConstraintType::Dimensional: {
            if (!b) return QStringLiteral("「%1」约束缺少被动方图元").arg(QString::fromUtf8(constraintTypeName(ct)));
            double sb;
            if (!measureSize(*b, sb))
                return QStringLiteral("「%1」约束要求被动方可度量（直线长度或圆半径）：%2 不支持")
                       .arg(QString::fromUtf8(constraintTypeName(ct))).arg(nb);
            if (ct == ConstraintType::Dimensional && value <= 0)
                return QStringLiteral("「尺寸」约束需要给出正数目标值（直线=长度，圆/弧=半径）");
            return {};
        }
        case ConstraintType::Horizontal:
        case ConstraintType::Vertical:
            if (!b || !isLineLikeE(*b))
                return QStringLiteral("「%1」约束要求被动方为直线：%2 不是直线")
                       .arg(QString::fromUtf8(constraintTypeName(ct))).arg(nb);
            return {};
        case ConstraintType::Symmetric:
        case ConstraintType::SymmetricPoint:
            if (!axis) return QStringLiteral("「%1」约束必须指定对称轴图元（第三个参数）").arg(QString::fromUtf8(constraintTypeName(ct)));
            if (!isLineLikeE(*axis)) return QStringLiteral("对称轴图元 #%1 不是直线").arg(axis->id);
            if (ct == ConstraintType::Symmetric) {
                if (!a || !b || a->pts.size() != b->pts.size() || a->pts.empty())
                    return QStringLiteral("「对称」约束要求两端图元点数相同且非空：%1 有 %2 个点、%3 有 %4 个点")
                           .arg(na).arg(a?a->pts.size():0).arg(nb).arg(b?b->pts.size():0);
            } else {
                if (!a || a->pts.empty() || !b || b->pts.empty())
                    return QStringLiteral("「点对称」约束要求两端图元都有端点：%1 或 %2 无端点").arg(na).arg(nb);
            }
            return {};
        case ConstraintType::PointOnLine:
            if (!a || a->pts.empty()) return QStringLiteral("「点在线上」约束的主动方需有点：%1 无端点").arg(na);
            if (!b || !isLineLikeE(*b)) return QStringLiteral("「点在线上」约束的被动方需为直线：%1 不是直线").arg(nb);
            return {};
        case ConstraintType::PointOnCircle:
            if (!a || a->pts.empty()) return QStringLiteral("「点在圆/弧上」约束的主动方需有点：%1 无端点").arg(na);
            if (!b || !isCircleLikeE(*b)) return QStringLiteral("「点在圆/弧上」约束的被动方需为圆/弧：%1 不是圆/弧").arg(nb);
            return {};
        case ConstraintType::FixedPoint:
            if (!a || a->pts.empty()) return QStringLiteral("「固定点」约束需指定一个有点可锁定的图元：%1 无端点").arg(na);
            return {};
        case ConstraintType::Midpoint:
            if (!a || a->pts.empty()) return QStringLiteral("「中点」约束的主动方需有点（中点）：%1 无端点").arg(na);
            if (!b || !isLineLikeE(*b)) return QStringLiteral("「中点」约束的被动方需为直线：%1 不是直线").arg(nb);
            return {};
        case ConstraintType::Distance:
            if (!a || a->pts.empty()) return QStringLiteral("「距离」约束需指定主动方锚点：%1 无端点").arg(na);
            if (!b || b->pts.empty()) return QStringLiteral("「距离」约束需指定被动方锚点：%1 无端点").arg(nb);
            if (value <= 0) return QStringLiteral("「距离」约束需要给出正数目标间距（mm）");
            return {};
        case ConstraintType::Fixed:
            return {};
    }
    return QStringLiteral("未知约束类型");
}

// 单条约束是否已满足
bool checkOne(const Element& a, const Element& b, const ConstraintRecord& r,
              const Element* axis) {
    switch (r.type) {
        case ConstraintType::Fixed:       return true;
        case ConstraintType::Coincident: {
            double best = 1e30;
            for (const auto& pa : a.pts) for (const auto& pb : b.pts) best = std::min(best, pa.distTo(pb));
            return best <= kSolveTol;
        }
        case ConstraintType::Equal: {
            double sa, sb;
            if (!measureSize(a, sa) || !measureSize(b, sb)) return false;
            return std::fabs(sa - sb) <= kSolveTol;
        }
        case ConstraintType::Horizontal:
            return isLineLikeE(b) && std::fabs(b.pts[1].y - b.pts[0].y) <= kSolveTol;
        case ConstraintType::Vertical:
            return isLineLikeE(b) && std::fabs(b.pts[1].x - b.pts[0].x) <= kSolveTol;
        case ConstraintType::Symmetric: {
            if (!axis || axis->pts.size() < 2) return false;
            const Vec3& ax0 = axis->pts[0]; const Vec3& ax1 = axis->pts[1];
            if (a.pts.size() != b.pts.size()) return false;
            for (size_t i = 0; i < a.pts.size(); ++i)
                if (b.pts[i].distTo(mirrorPoint(a.pts[i], ax0, ax1)) > kSolveTol) return false;
            return true;
        }
        case ConstraintType::Dimensional: {
            double sb;
            if (!measureSize(b, sb)) return false;
            return std::fabs(sb - r.value) <= kSolveTol;
        }
        case ConstraintType::Collinear: {
            if (!isLineLikeE(a) || !isLineLikeE(b)) return false;
            const Vec3 da = a.pts[1] - a.pts[0], db = b.pts[1] - b.pts[0];
            const double cross = da.x * db.y - da.y * db.x;
            if (std::fabs(cross) > kSolveTol) return false;        // 不平行
            return std::fabs(signedDistPointToLine(b.pts[0], a.pts[0], a.pts[1])) <= kSolveTol;
        }
        case ConstraintType::EqualLength:
            return isLineLikeE(a) && isLineLikeE(b) &&
                   std::fabs(a.pts[0].distTo(a.pts[1]) - b.pts[0].distTo(b.pts[1])) <= kSolveTol;
        case ConstraintType::EqualRadius:
            return isCircleLikeE(a) && isCircleLikeE(b) &&
                   std::fabs(a.radius - b.radius) <= kSolveTol;
        case ConstraintType::PointOnLine: {
            if (!isLineLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            return std::fabs(signedDistPointToLine(pa, b.pts[0], b.pts[1])) <= kSolveTol;
        }
        case ConstraintType::PointOnCircle: {
            if (!isCircleLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            return std::fabs(pa.distTo(b.pts[0]) - b.radius) <= kSolveTol;
        }
        case ConstraintType::FixedPoint:
            return true;   // 锁定型约束：由数值求解器把该点锚定，视为已满足
        case ConstraintType::Midpoint: {
            if (!isLineLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            const Vec3 mid = (b.pts[0] + b.pts[1]) * 0.5;
            return pa.distTo(mid) <= kSolveTol;
        }
        case ConstraintType::Distance: {
            const Vec3 pa = anchorPoint(a, r.idxA), pb = anchorPoint(b, r.idxB);
            return std::fabs(pa.distTo(pb) - r.value) <= kSolveTol;
        }
        case ConstraintType::Angle: {
            if (!isLineLikeE(a) || !isLineLikeE(b)) return false;
            return std::fabs(angleBetweenLinesDeg(a, b) - r.value) <= kSolveTol;
        }
        case ConstraintType::SymmetricPoint: {
            if (!axis || axis->pts.size() < 2) return false;
            const Vec3 pa = anchorPoint(a, r.idxA), pb = anchorPoint(b, r.idxB);
            const Vec3 mid = (pa + pb) * 0.5;
            if (std::fabs(signedDistPointToLine(mid, axis->pts[0], axis->pts[1])) > kSolveTol) return false;
            const Vec3 ab = pb - pa, ad = axis->pts[1] - axis->pts[0];
            const double L = ad.len2D();
            return L < 1e-12 || std::fabs(ab.x * ad.x + ab.y * ad.y) / L <= kSolveTol;
        }
        default:
            return geo::checkConstraint(a, b, r.type, kSolveTol);   // 平行/垂直/相切/同心
    }
}

// 施加单条约束：把被动方 b 解到满足为止。返回是否发生了修改。
bool applyOne(const Element& a, Element& b, const ConstraintRecord& r, const Element* axis) {
    switch (r.type) {
        case ConstraintType::Fixed:
            return false;                                   // 固定：不做任何移动（由冲突检测保证不被要求移动）
        case ConstraintType::Coincident: {
            // 取最近的一对端点，平移 b 使其重合（整条 b 刚体平移，形状不变）
            int ia = 0, ib = 0; double best = 1e30;
            for (size_t i = 0; i < a.pts.size(); ++i)
                for (size_t j = 0; j < b.pts.size(); ++j) {
                    const double d = a.pts[i].distTo(b.pts[j]);
                    if (d < best) { best = d; ia = (int)i; ib = (int)j; }
                }
            if (best > 1e29) return false;
            const Vec3 delta = a.pts[ia] - b.pts[ib];
            for (auto& p : b.pts) p = p + delta;
            return true;
        }
        case ConstraintType::Equal: {
            double sa, sb;
            if (!measureSize(a, sa) || !measureSize(b, sb)) return false;
            if (std::fabs(sa - sb) <= kSolveTol) return false;
            return setSize(b, sa);
        }
        case ConstraintType::Horizontal: {
            if (!isLineLikeE(b)) return false;
            const double cur = std::atan2(b.pts[1].y - b.pts[0].y, b.pts[1].x - b.pts[0].x) * 180.0 / M_PI;
            if (std::fabs(cur) <= 1e-6 || std::fabs(std::fabs(cur) - 180.0) <= 1e-6) return false;
            rotatePtsAbout(b.pts, b.pts[0], -cur);           // 转到 0°
            return true;
        }
        case ConstraintType::Vertical: {
            if (!isLineLikeE(b)) return false;
            const double cur = std::atan2(b.pts[1].y - b.pts[0].y, b.pts[1].x - b.pts[0].x) * 180.0 / M_PI;
            if (std::fabs(cur - 90.0) <= 1e-6 || std::fabs(cur + 90.0) <= 1e-6) return false;
            rotatePtsAbout(b.pts, b.pts[0], 90.0 - cur);     // 转到 90°
            return true;
        }
        case ConstraintType::Symmetric: {
            if (!axis || axis->pts.size() < 2) return false;
            const Vec3& ax0 = axis->pts[0]; const Vec3& ax1 = axis->pts[1];
            if (a.pts.size() != b.pts.size()) return false;
            b.pts.clear();
            for (const auto& p : a.pts) b.pts.push_back(mirrorPoint(p, ax0, ax1));
            return true;
        }
        case ConstraintType::Dimensional:
            return setSize(b, r.value);
        case ConstraintType::Collinear: {
            if (!isLineLikeE(a) || !isLineLikeE(b)) return false;
            const Vec3 da = a.pts[1] - a.pts[0], db = b.pts[1] - b.pts[0];
            const double cur = std::atan2(db.y, db.x) * 180.0 / M_PI;
            const double tgt = std::atan2(da.y, da.x) * 180.0 / M_PI;
            rotatePtsAbout(b.pts, b.pts[0], tgt - cur);                 // 先转平行
            const Vec3 foot = b.pts[0] + db * ((a.pts[0]-b.pts[0]).dot2D(db) / db.len2D()); // 落到直线 A 上
            const Vec3 delta = a.pts[0] - foot;
            for (auto& p : b.pts) p = p + delta;
            return true;
        }
        case ConstraintType::EqualLength:
            return isLineLikeE(a) ? setSize(b, sizeOf(a)) : false;
        case ConstraintType::EqualRadius:
            return isCircleLikeE(a) ? (b.radius = a.radius, true) : false;
        case ConstraintType::PointOnLine: {
            if (!isLineLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            const Vec3 d = b.pts[1] - b.pts[0];
            const double L2 = d.len2D();
            if (L2 < 1e-12) return false;
            const Vec3 foot = b.pts[0] + d * ((pa - b.pts[0]).dot2D(d) / L2);
            const Vec3 delta = pa - foot;
            for (auto& p : b.pts) p = p + delta;
            return true;
        }
        case ConstraintType::PointOnCircle: {
            if (!isCircleLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            Vec3 dir = b.pts[0] - pa;
            if (dir.len2D() < 1e-12) dir = Vec3(1, 0, 0);
            b.pts[0] = pa + dir.normalized() * b.radius;
            return true;
        }
        case ConstraintType::FixedPoint:
            return false;
        case ConstraintType::Midpoint: {
            if (!isLineLikeE(b)) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            const Vec3 mid = (b.pts[0] + b.pts[1]) * 0.5;
            const Vec3 delta = pa - mid;
            for (auto& p : b.pts) p = p + delta;
            return true;
        }
        case ConstraintType::Distance: {
            const Vec3 pa = anchorPoint(a, r.idxA);
            Vec3 pb = anchorPoint(b, r.idxB);
            Vec3 dir = pb - pa;
            if (dir.len2D() < 1e-12) dir = Vec3(1, 0, 0);
            const Vec3 pbNew = pa + dir.normalized() * r.value;
            const Vec3 delta = pbNew - pb;
            for (auto& p : b.pts) p = p + delta;
            return true;
        }
        case ConstraintType::Angle: {
            if (!isLineLikeE(a) || !isLineLikeE(b)) return false;
            const Vec3 da = a.pts[1] - a.pts[0], db = b.pts[1] - b.pts[0];
            const double cur = std::atan2(db.y, db.x) * 180.0 / M_PI;
            const double tgt = std::atan2(da.y, da.x) * 180.0 / M_PI + r.value;
            rotatePtsAbout(b.pts, b.pts[0], tgt - cur);
            return true;
        }
        case ConstraintType::SymmetricPoint: {
            if (!axis || axis->pts.size() < 2) return false;
            const Vec3 pa = anchorPoint(a, r.idxA);
            const Vec3 mp = mirrorPoint(pa, axis->pts[0], axis->pts[1]);
            const int j = (r.idxB >= 0 && r.idxB < (int)b.pts.size()) ? r.idxB : 0;
            if (b.pts.empty()) return false;
            b.pts[j] = mp;
            return true;
        }
        default:
            return geo::restoreConstraint(a, b, r.type, kSolveTol);   // 平行/垂直/相切/同心
    }
}

// 失败原因文案：把「哪两个图元、什么约束、为什么」说清楚，便于用户定位
QString failReason(const ConstraintRecord& r, const QString& detail) {
    return QStringLiteral("#%1 与 #%2 的「%3」约束无法满足：%4")
           .arg(r.idA).arg(r.idB).arg(QString::fromUtf8(constraintTypeName(r.type))).arg(detail);
}

} // namespace

QString ConstraintSolver::addConstraint(SketchModel& m, ConstraintType type,
                                        int idA, int idB, double value, int axisId,
                                        int idxA, int idxB) {
    const Element* a = m.element(idA);
    if (!a) return QStringLiteral("图元 #%1 不存在").arg(idA);
    const Element* b = (idB >= 0) ? m.element(idB) : nullptr;
    // 固定/固定点允许锁定同一图元自身；其余约束两端必须是不同图元
    if (type != ConstraintType::Fixed && type != ConstraintType::FixedPoint && idA == idB)
        return QStringLiteral("约束的两端不能是同一个图元 #%1").arg(idA);
    if (idB >= 0 && !b) return QStringLiteral("图元 #%1 不存在").arg(idB);
    const Element* axis = (axisId >= 0) ? m.element(axisId) : nullptr;
    if (axisId >= 0 && !axis) return QStringLiteral("对称轴图元 #%1 不存在").arg(axisId);

    const QString err = compatError(type, a, b, axis, value);
    if (!err.isEmpty()) return err;

    ConstraintRecord r;
    r.type = type; r.idA = idA; r.idB = idB; r.axisId = axisId;
    r.value = value; r.idxA = idxA; r.idxB = idxB;

    // 已满足 → 直接登记；否则登记后走数值求解器一次性解到满足
    if (checkOne(*a, b ? *b : *a, r, axis)) {
        r.satisfied = true;
        m.addConstraintRecord(r);
        return {};
    }
    m.addConstraintRecord(r);
    m.beginEdit(QStringLiteral("添加几何约束"));
    // 垂直/角度约束在两端"恰好平行"时退化：夹角残差在平行处梯度为零，LM 无法决定旋转方向。
    // 求解前给被动直线一个极小初始转角（≈3°）打破对称性；最终仍收敛到目标角，用户无感。
    if ((type == ConstraintType::Perpendicular || type == ConstraintType::Angle) &&
        b && isLineLikeE(*b) && isLineLikeE(*a)) {
        const Vec3 da = a->pts[1] - a->pts[0];
        const Vec3 db = b->pts[1] - b->pts[0];
        const double L = da.len2D() * db.len2D();
        if (L > 1e-9 && std::fabs(da.x * db.y - da.y * db.x) < 1e-6 * L) {
            Element bSeed = *b;
            rotatePtsAbout(bSeed.pts, bSeed.pts[0], 3.0);
            m.updateElementQuiet(b->id, [&](Element& e) { e = bSeed; });
            b = m.element(idB);
        }
    }
    const auto diag = solveAllNumeric(m);
    // 再判一次：解完仍不满足（例如与其上已有约束冲突）则给出可定位原因
    const Element* a2 = m.element(idA);
    const Element* b2 = (idB >= 0) ? m.element(idB) : nullptr;
    const bool ok = a2 && checkOne(*a2, b2 ? *b2 : *a2, r, axis);
    if (!ok) {
        // 求解器已把能解的都解了；此处仅给出可定位的失败原因（satisfied 由 solveAllNumeric 写回）
        return failReason(r, QStringLiteral("几何上无法解出满足该约束的位置（可能与其上已有的约束冲突，可用 constraint check 查看）"));
    }
    (void)diag;
    return {};
}

std::vector<int> ConstraintSolver::constraintsInvolving(const SketchModel& m, int id) {
    std::vector<int> out;
    const auto& recs = m.constraints();
    for (int i = 0; i < (int)recs.size(); ++i) {
        const auto& r = recs[i];
        if (r.idA == id || r.idB == id || r.axisId == id)
            out.push_back(i);
    }
    return out;
}

// ==================== 尺寸驱动 / 参数化建模（Request M 新增）====================
// 把"标注自然量"(mm 或 °，按用户视角)换算成"约束目标值"。
// 直径标注的自然量是直径，但 Dimensional(圆)约束的是半径 → 需 /2。
static double dimValueToConstraintValue(DimensionType dt, double v) {
    if (dt == DimensionType::Diameter) return v * 0.5;   // 直径 → 半径
    return v;
}
// 把"约束目标值"换算回"标注自然量"（用于把求解结果回填 dimValue 显示）。
static double constraintValueToDimValue(DimensionType dt, double c) {
    if (dt == DimensionType::Diameter) return c * 2.0;
    return c;
}
// 把几何量格式化为带前缀的尺寸文本（R / ⌀ / ° / ⌒），最多 2 位小数去尾随零。
static QString fmtDimNum(double v) {
    QString s = QString::number(v, 'f', 2);
    if (s.contains('.')) { while (s.endsWith('0')) s.chop(1); if (s.endsWith('.')) s.chop(1); }
    return s;
}
static QString formatDimText(DimensionType dt, double v) {
    switch (dt) {
        case DimensionType::Radius:   return QStringLiteral("R") + fmtDimNum(v);
        case DimensionType::Diameter: return QString::fromUtf8("\xE2\x8C\x80") + fmtDimNum(v);  // ⌀
        case DimensionType::Angle:    return fmtDimNum(v) + QString::fromUtf8("\xC2\xB0");      // °
        case DimensionType::Arc:      return QStringLiteral("\xE2\x8C\x92") + fmtDimNum(v);      // ⌒
        default: return fmtDimNum(v);
    }
}

double ConstraintSolver::measureDimension(const SketchModel& m, int dimId) {
    const Element* dim = m.element(dimId);
    if (!dim || dim->type != ElementType::Dimension) return std::numeric_limits<double>::quiet_NaN();
    switch (dim->dtype) {
        case DimensionType::Radius: {
            const Element* a = m.element(dim->refA);
            return (a && isCircleLikeE(*a)) ? a->radius : std::numeric_limits<double>::quiet_NaN();
        }
        case DimensionType::Diameter: {
            const Element* a = m.element(dim->refA);
            return (a && isCircleLikeE(*a)) ? (2.0 * a->radius) : std::numeric_limits<double>::quiet_NaN();
        }
        case DimensionType::Angle: {
            const Element* a = m.element(dim->refA);
            const Element* b = m.element(dim->refB);
            if (a && b && isLineLikeE(*a) && isLineLikeE(*b)) return angleBetweenLinesDeg(*a, *b);
            return std::numeric_limits<double>::quiet_NaN();
        }
        case DimensionType::Distance:
        case DimensionType::Aligned:
        case DimensionType::Horizontal:
        case DimensionType::Vertical: {
            const Element* a = m.element(dim->refA);
            if (!a) return std::numeric_limits<double>::quiet_NaN();
            const Element* b = (dim->refB >= 0) ? m.element(dim->refB) : nullptr;
            if (!b) {
                if (isLineLikeE(*a)) return a->pts[0].distTo(a->pts[1]);
                if (isCircleLikeE(*a)) return a->radius;
                return std::numeric_limits<double>::quiet_NaN();
            }
            const Vec3 pa = anchorPoint(*a, -1), pb = anchorPoint(*b, -1);
            return pa.distTo(pb);
        }
        default:
            return std::numeric_limits<double>::quiet_NaN();
    }
}

QString ConstraintSolver::linkDimension(SketchModel& m, int dimId, const QString& paramRef) {
    // 读取标注字段（捕获为局部标量，避免后续求解导致 vector 重排后指针失效）
    QString dimParam; bool dimDriven = false; double dimValue0 = 0.0; DimensionType dtype = DimensionType::Distance;
    int refA = -1, refB = -1;
    bool isDim = false;
    m.updateElementQuiet(dimId, [&](Element& e){
        if (e.type != ElementType::Dimension) return;
        isDim = true; dimParam = e.dimParam; dimDriven = e.dimDriven; dimValue0 = e.dimValue;
        dtype = e.dtype; refA = e.refA; refB = e.refB;
        if (!paramRef.isEmpty()) { e.dimParam = paramRef; dimParam = paramRef; }  // 登记参数关联
    });
    if (!isDim) return QStringLiteral("标注 #%1 不存在或非尺寸标注").arg(dimId);
    if (refA < 0) return QStringLiteral("标注 #%1 未引用任何几何（refA 无效）").arg(dimId);

    // 计算目标约束值：已设置且非零则取用户值，否则取当前几何测量值
    const bool useStored = dimDriven && dimValue0 != 0.0;
    const double baseVal = useStored ? dimValue0 : measureDimension(m, dimId);
    const double target = dimValueToConstraintValue(dtype, baseVal);

    // 依 dtype 决定约束类型与两端
    ConstraintType ct; int idA = refA, idB = -1;
    bool ok = true; QString err;
    switch (dtype) {
        case DimensionType::Radius:
        case DimensionType::Diameter: {
            // 单几何尺寸：被度量的圆/弧置于 idB（Dimensional 残差作用于 sizeOf(*b)），
            // idA 置为同一图元（compatError 仅校验 idB；数值求解只动 idB 的半径/长度）。
            ct = ConstraintType::Dimensional; idA = refA; idB = refA;
            const Element* a = m.element(idA);
            if (!a || !isCircleLikeE(*a)) { ok = false; err = QStringLiteral("半径/直径标注 #%1 必须引用圆或圆弧").arg(dimId); }
            break;
        }
        case DimensionType::Angle: {
            ct = ConstraintType::Angle; idA = refA; idB = refB;
            const Element* a = m.element(idA); const Element* b = m.element(idB);
            if (!a || !b || !isLineLikeE(*a) || !isLineLikeE(*b)) { ok = false; err = QStringLiteral("角度标注 #%1 必须引用两条直线").arg(dimId); }
            break;
        }
        case DimensionType::Distance:
        case DimensionType::Aligned:
        case DimensionType::Horizontal:
        case DimensionType::Vertical: {
            const Element* a = m.element(refA);
            const Element* b = (refB >= 0) ? m.element(refB) : nullptr;
            if (!a) { ok = false; err = QStringLiteral("标注 #%1 引用的几何 #%2 不存在").arg(dimId).arg(refA); break; }
            if (!b) {
                // 单几何尺寸：被度量的图元置于 idB（Dimensional 残差作用于 sizeOf(*b)）
                if (isLineLikeE(*a) || isCircleLikeE(*a)) { ct = ConstraintType::Dimensional; idA = refA; idB = refA; }
                else { ok = false; err = QStringLiteral("长度标注 #%1 必须引用直线或圆").arg(dimId); }
            } else {
                ct = ConstraintType::Distance; idA = refA; idB = refB;
            }
            break;
        }
        default:
            ok = false; err = QStringLiteral("标注类型 %1 暂不支持尺寸驱动").arg((int)dtype);
    }
    if (!ok) return err;

    const Element* a = m.element(idA);
    const Element* b = (idB >= 0) ? m.element(idB) : nullptr;
    QString cerr = compatError(ct, a, b, nullptr, target);
    if (!cerr.isEmpty()) return cerr;

    // 幂等：若已有关联约束则更新其值，否则新建一条
    auto recs = m.constraints();
    bool foundRec = false;
    for (auto& r : recs) {
        if (r.driverId == dimId) { r.value = target; r.paramRef = dimParam; foundRec = true; }
    }
    if (!foundRec) {
        ConstraintRecord r;
        r.type = ct; r.idA = idA; r.idB = idB; r.value = target;
        r.driverId = dimId; r.paramRef = dimParam;
        recs.push_back(r);
        // 角度约束在两端恰好平行时退化：预旋转被动直线 3° 打破对称性（与 addConstraint 一致）
        if (ct == ConstraintType::Angle && b && isLineLikeE(*b) && isLineLikeE(*a)) {
            const Vec3 da = a->pts[1] - a->pts[0], db = b->pts[1] - b->pts[0];
            const double L = da.len2D() * db.len2D();
            if (L > 1e-9 && std::fabs(da.x * db.y - da.y * db.x) < 1e-6 * L) {
                Element bSeed = *b; rotatePtsAbout(bSeed.pts, bSeed.pts[0], 3.0);
                m.updateElementQuiet(b->id, [&](Element& e){ e = bSeed; });
                b = m.element(idB);
            }
        }
    }
    m.setConstraints(recs);
    m.beginEdit(QStringLiteral("关联尺寸标注"));
    solveAllNumeric(m);
    // 标注状态回写 + 数值/文本刷新
    m.updateElementQuiet(dimId, [&](Element& e){
        e.dimLinked = true; e.dimDriven = true;
        e.dimValue = constraintValueToDimValue(e.dtype, target);
    });
    syncDimensionValues(m);
    return {};
}

QString ConstraintSolver::setDimensionValue(SketchModel& m, int dimId, double value) {
    bool isDim = false;
    m.updateElementQuiet(dimId, [&](Element& e){
        if (e.type == ElementType::Dimension) { e.dimValue = value; e.dimDriven = true; e.dimLinked = true; isDim = true; }
    });
    if (!isDim) return QStringLiteral("标注 #%1 不存在或非尺寸标注").arg(dimId);

    auto recs = m.constraints();
    bool hasDriver = false;
    for (auto& r : recs) if (r.driverId == dimId) hasDriver = true;

    if (!hasDriver) {
        // 尚未关联：先建立关联（linkDimension 会读取上面写好的 dimValue 作为目标）
        return linkDimension(m, dimId);
    }
    // 已关联：改目标值并重解
    const Element* dim = m.element(dimId);
    if (!dim) return QStringLiteral("标注 #%1 不存在").arg(dimId);
    const double target = dimValueToConstraintValue(dim->dtype, value);
    for (auto& r : recs) if (r.driverId == dimId) r.value = target;
    m.setConstraints(recs);
    m.beginEdit(QStringLiteral("修改标注值"));
    solveAllNumeric(m);
    syncDimensionValues(m);
    return {};
}

void ConstraintSolver::syncDimensionValues(SketchModel& m) {
    for (const auto& e : m.elements()) {
        if (e.type != ElementType::Dimension || !e.dimLinked) continue;
        const double measured = measureDimension(m, e.id);
        if (std::isnan(measured)) continue;
        m.updateElementQuiet(e.id, [&](Element& d){
            d.dimValue = measured;     // 数据层回填：标注值 = 当前几何量
            d.text = formatDimText(d.dtype, measured);  // 文本层刷新（dim::build 以此为准）
        });
    }
}

QString ConstraintSolver::driveParameter(SketchModel& m, const QString& param, double val) {
    m.setParam(param, val);
    auto recs = m.constraints();
    bool changed = false;
    for (auto& r : recs) {
        if (r.paramRef != param) continue;
        const Element* dim = m.element(r.driverId);
        if (!dim) continue;
        r.value = dimValueToConstraintValue(dim->dtype, val);
        changed = true;
    }
    for (const auto& e : m.elements()) {
        if (e.type == ElementType::Dimension && e.dimParam == param) {
            m.updateElementQuiet(e.id, [&](Element& d){ d.dimValue = val; d.dimDriven = true; d.dimLinked = true; });
            changed = true;
        }
    }
    if (!changed) return QStringLiteral("参数 %1 未关联到任何标注").arg(param);
    m.setConstraints(recs);
    m.beginEdit(QStringLiteral("参数驱动"));
    solveAllNumeric(m);
    syncDimensionValues(m);
    return {};
}

// 注：solveAll 已统一委托数值求解器 solveAllNumeric（见文件末尾定义），
// 解析法 applyOne/checkOne 保留作为单类即时预检与兜底。

std::vector<QString> ConstraintSolver::detectConflicts(const SketchModel& m) {
    std::vector<QString> out;
    const auto& recs = m.constraints();
    const auto name = [](ConstraintType t) { return QString::fromUtf8(constraintTypeName(t)); };

    // ① 同一对图元上的互斥方向约束
    for (size_t i = 0; i < recs.size(); ++i)
        for (size_t j = i + 1; j < recs.size(); ++j) {
            const auto& A = recs[i]; const auto& B = recs[j];
            const bool samePair = (A.idA == B.idA && A.idB == B.idB) || (A.idA == B.idB && A.idB == B.idA);
            if (!samePair) continue;
            if (A.type == B.type)
                out.push_back(QStringLiteral("#%1 与 #%2 之间存在重复的同类型约束「%3」")
                              .arg(A.idA).arg(A.idB).arg(name(A.type)));
            const auto isP = [](ConstraintType t) { return t == ConstraintType::Parallel; };
            const auto isV = [](ConstraintType t) { return t == ConstraintType::Perpendicular; };
            if ((isP(A.type) && isV(B.type)) || (isV(A.type) && isP(B.type)))
                out.push_back(QStringLiteral("#%1 与 #%2 同时被约束为「平行」和「垂直」，二者互斥")
                              .arg(A.idA).arg(A.idB));
            const auto isH = [](ConstraintType t) { return t == ConstraintType::Horizontal; };
            const auto isVe = [](ConstraintType t) { return t == ConstraintType::Vertical; };
            if ((isH(A.type) && isVe(B.type)) || (isVe(A.type) && isH(B.type)))
                out.push_back(QStringLiteral("#%1 与 #%2 同时被约束为「水平」和「竖直」，二者互斥")
                              .arg(A.idA).arg(A.idB));
            if ((A.type == ConstraintType::Concentric && B.type == ConstraintType::Tangent) ||
                (A.type == ConstraintType::Tangent && B.type == ConstraintType::Concentric))
                out.push_back(QStringLiteral("#%1 与 #%2 同时被约束为「同心」和「相切」：同心圆仅当一圆退化为点时才相切，通常无解")
                              .arg(A.idA).arg(A.idB));
        }

    // ② 同一图元被要求既水平又竖直（跨不同约束对）
    for (size_t i = 0; i < recs.size(); ++i)
        for (size_t j = i + 1; j < recs.size(); ++j) {
            if (recs[i].idB != recs[j].idB) continue;
            const auto isH = [](ConstraintType t) { return t == ConstraintType::Horizontal; };
            const auto isVe = [](ConstraintType t) { return t == ConstraintType::Vertical; };
            if ((isH(recs[i].type) && isVe(recs[j].type)) || (isVe(recs[i].type) && isH(recs[j].type)))
                out.push_back(QStringLiteral("图元 #%1 同时被约束为「水平」和「竖直」，二者互斥").arg(recs[i].idB));
        }

    // ③ Fixed 图元又被当作被动方（求解时不允许移动它 → 该约束必然失败）
    for (const auto& f : recs) {
        if (f.type != ConstraintType::Fixed) continue;
        for (const auto& r : recs) {
            if (&r == &f) continue;
            if (r.idB == f.idA || r.idB == f.idB)
                out.push_back(QStringLiteral("图元 #%1 已被「固定」约束锁定，但又是 #%2 与 #%3 的「%4」约束的被动方（求解需移动它）")
                              .arg(r.idB).arg(r.idA).arg(r.idB).arg(name(r.type)));
        }
    }

    // ④(扩展) 同一图元被多个「尺寸类」约束施加了不一致的目标尺寸 → 过约束
    //   Dimensional(value) 直接给出目标；Equal/EqualLength/EqualRadius 以"另一端图元尺寸"为隐含目标。
    std::map<int, std::vector<double>> imposed;   // 图元 id → 目标尺寸列表
    for (const auto& c : recs) {
        if (c.type == ConstraintType::Dimensional && c.value > 0) {
            imposed[c.idB].push_back(c.value);
        } else if ((c.type == ConstraintType::Equal || c.type == ConstraintType::EqualLength ||
                    c.type == ConstraintType::EqualRadius) && c.idB >= 0) {
            const Element* ea = m.element(c.idA);
            double s = 0;
            if (ea && measureSize(*ea, s)) imposed[c.idB].push_back(s);
        } else if (c.type == ConstraintType::Distance && c.value > 0) {
            // 距离约束对两端施加了"相距 value"的耦合，与另一个不同距离值冲突
            imposed[c.idA].push_back(c.value);
            imposed[c.idB].push_back(c.value);
        }
    }
    for (const auto& kv : imposed) {
        const auto& vals = kv.second;
        for (size_t p = 0; p + 1 < vals.size(); ++p)
            for (size_t q = p + 1; q < vals.size(); ++q)
                if (std::fabs(vals[p] - vals[q]) > kSolveTol)
                    out.push_back(QStringLiteral("图元 #%1 被多个尺寸/距离类约束施加了不一致的目标值（%2 与 %3），过约束")
                                  .arg(kv.first).arg(vals[p], 0, 'f', 2).arg(vals[q], 0, 'f', 2));
    }

    // ⑤ 同一对图元上的 Distance/Angle 给出不同目标值 → 冲突
    for (size_t i = 0; i < recs.size(); ++i)
        for (size_t j = i + 1; j < recs.size(); ++j) {
            const auto& A = recs[i]; const auto& B = recs[j];
            const bool samePair = (A.idA == B.idA && A.idB == B.idB) || (A.idA == B.idB && A.idB == B.idA);
            if (!samePair) continue;
            const bool bothDist = A.type == ConstraintType::Distance && B.type == ConstraintType::Distance;
            const bool bothAng  = A.type == ConstraintType::Angle   && B.type == ConstraintType::Angle;
            if ((bothDist || bothAng) && std::fabs(A.value - B.value) > kSolveTol)
                out.push_back(QStringLiteral("#%1 与 #%2 同时存在目标值不同的「%3」约束（%4 与 %5），冲突")
                              .arg(A.idA).arg(A.idB).arg(name(A.type)).arg(A.value,0,'f',2).arg(B.value,0,'f',2));
        }

    // 去重（同一条问题可能被多条规则命中）
    std::vector<QString> uniq;
    for (const auto& s : out) if (std::find(uniq.begin(), uniq.end(), s) == uniq.end()) uniq.push_back(s);
    return uniq;
}

// ===========================================================================
// 数值约束求解器：雅可比 + Levenberg-Marquardt（统一承载全部 21 种约束）
// ===========================================================================
namespace {

// 解对称正定线性方程组 A x = b（A 为 n×n，带部分主元的高斯消元）。
// 返回 false 表示奇异（Damping λI 通常会避免）。
bool solveLinearSystem(const std::vector<std::vector<double>>& A,
                       const std::vector<double>& b, std::vector<double>& x) {
    const int n = (int)A.size();
    if (n == 0) return true;
    std::vector<std::vector<double>> M(n, std::vector<double>(n + 1, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) M[i][j] = A[i][j];
        M[i][n] = b[i];
    }
    for (int col = 0; col < n; ++col) {
        int piv = col; double best = std::fabs(M[col][col]);
        for (int r = col + 1; r < n; ++r) {
            if (std::fabs(M[r][col]) > best) { best = std::fabs(M[r][col]); piv = r; }
        }
        if (best < 1e-14) return false;
        if (piv != col) std::swap(M[col], M[piv]);
        const double d = M[col][col];
        for (int r = col + 1; r < n; ++r) {
            const double f = M[r][col] / d;
            if (f == 0.0) continue;
            for (int k = col; k <= n; ++k) M[r][k] -= f * M[col][k];
        }
    }
    x.assign(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double s = M[i][n];
        for (int j = i + 1; j < n; ++j) s -= M[i][j] * x[j];
        x[i] = s / M[i][i];
    }
    return true;
}

// 对称矩阵的特征值（Jacobi 旋转法，循环扫描）。仅取对角元作为特征值。
std::vector<double> jacobiEigenvalues(const std::vector<std::vector<double>>& Ain, int maxSweeps = 120) {
    const int n = (int)Ain.size();
    std::vector<std::vector<double>> A = Ain;
    const double eps = 1e-13;
    for (int sweep = 0; sweep < maxSweeps; ++sweep) {
        int p = 0, q = 1; double maxv = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (std::fabs(A[i][j]) > maxv) { maxv = std::fabs(A[i][j]); p = i; q = j; }
        if (maxv < eps) break;
        const double app = A[p][p], aqq = A[q][q], apq = A[p][q];
        const double theta = 0.5 * std::atan2(2.0 * apq, aqq - app);
        const double c = std::cos(theta), s = std::sin(theta);
        for (int k = 0; k < n; ++k) {
            if (k == p || k == q) continue;
            const double akp = A[k][p], akq = A[k][q];
            A[k][p] = A[p][k] = c * akp - s * akq;
            A[k][q] = A[q][k] = s * akp + c * akq;
        }
        A[p][p] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
        A[q][q] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
        A[p][q] = A[q][p] = 0.0;
    }
    std::vector<double> ev(n);
    for (int i = 0; i < n; ++i) ev[i] = A[i][i];
    return ev;
}

// 由 (JᵀJ) 的秩推断自由度：零空间维数 = 变量数 − 秩。
int rankOfSymmetric(const std::vector<std::vector<double>>& S, double thresh) {
    const auto ev = jacobiEigenvalues(S);
    int rank = 0;
    for (double e : ev) if (std::fabs(e) > thresh) ++rank;
    return rank;
}

// 数值求解核心产出：全局自由度/冗余/冲突 + 逐实体自由度诊断。
// 既被 solveAllNumeric（writeBack=true，写回模型）使用，
// 也被 diagnose（writeBack=false，仅作只读分析）复用，避免重复实现。
struct NumericResult {
    bool converged = false;
    int iterations = 0, variables = 0, rank = 0, dof = 0;
    double residualNorm = 0;
    std::vector<int> redundantIdx, infeasibleIdx;
    std::vector<ElementConstraintInfo> elementInfo;
    std::vector<int> underconstrainedIds, overconstrainedIds;
    int totalDof = 0, freeDof = 0, overDof = 0;
    ConstraintDiagnosis::SystemState state = ConstraintDiagnosis::WellConstrained;
};

// 数值约束求解核心：在 (work, recs) 上构建变量集、残差、雅可比，运行 LM，
// 并产出全局/逐实体诊断。writeBack=true 时把解出的几何与 satisfied 写回 m（m 可为 nullptr）。
static NumericResult numericAnalyze(SketchModel* m, std::vector<Element>& work,
                                    std::vector<ConstraintRecord>& recs, bool writeBack,
                                    const SolveOptions& opt) {
    NumericResult result;
    SolveDiagnostics diag;
    diag.converged = false;
    if (recs.empty()) return result;

    // 工作副本 + id→索引映射
    std::map<int, int> idxOf;
    for (size_t i = 0; i < work.size(); ++i) idxOf[work[i].id] = (int)i;

    // 收集决策变量：被任一约束引用的图元的"全部几何分量"都纳入，
    // 但整图元被 Fixed 锁定的、或被 FixedPoint 锁定的单个点除外。
    std::set<int> wholeFixed;
    std::set<std::pair<int,int>> lockedPts;
    for (const auto& r : recs) {
        if (r.type == ConstraintType::Fixed) wholeFixed.insert(r.idA);
        if (r.type == ConstraintType::FixedPoint)
            lockedPts.insert({r.idA, r.idxA >= 0 ? r.idxA : 0});
    }
    std::map<VarKey, int> varIndex;
    std::vector<VarKey> varKeys;
    auto addVar = [&](const VarKey& k) {
        if (!varIndex.count(k)) { varIndex[k] = (int)varKeys.size(); varKeys.push_back(k); }
    };
    for (const auto& r : recs) {
        int ids[3] = {r.idA, r.idB, r.axisId};
        for (int id : ids) {
            if (id < 0) continue;
            if (wholeFixed.count(id)) continue;
            auto it = idxOf.find(id);
            if (it == idxOf.end()) continue;
            const Element& e = work[it->second];
            if (e.type == ElementType::Circle || e.type == ElementType::Arc) addVar({id, 2, 0});
            for (int i = 0; i < (int)e.pts.size(); ++i) {
                if (lockedPts.count({id, i})) continue;
                addVar({id, 0, i}); addVar({id, 1, i});
            }
        }
    }

    const int V = (int)varKeys.size();
    if (V == 0) {
        // 没有可动变量（例如仅 Fixed/FixedPoint 把一切锁死）：直接用 checkOne 标记 satisfied
        for (auto& r : recs) {
            auto ia = idxOf.find(r.idA);
            const Element* a = (ia != idxOf.end()) ? &work[ia->second] : nullptr;
            const Element* b = nullptr;
            if (r.idB >= 0) { auto ib = idxOf.find(r.idB); if (ib != idxOf.end()) b = &work[ib->second]; }
            const Element* axis = nullptr;
            if (r.axisId >= 0) { auto iy = idxOf.find(r.axisId); if (iy != idxOf.end()) axis = &work[iy->second]; }
            r.satisfied = a && checkOne(*a, b ? *b : *a, r, axis);
        }
        if (m && writeBack) m->setConstraints(recs);
        diag.converged = true; diag.iterations = 0; diag.variables = 0; diag.dof = 0; diag.rank = 0; diag.residualNorm = 0;
        for (const auto& e : work) {
            if (e.type == ElementType::Constraint || e.type == ElementType::Dimension) continue;
            ElementConstraintInfo info;
            info.id = e.id; info.type = e.type; info.naturalDof = ConstraintSolver::naturalDof(e);
            info.activeVars = 0; info.appliedRank = 0; info.remainingDof = info.naturalDof;
            result.elementInfo.push_back(info);
            result.totalDof += info.naturalDof;
        }
        result.state = ConstraintDiagnosis::WellConstrained;
        return result;
    }

    // 变量读写
    auto getVar = [&](const VarKey& k) -> double {
        const Element& e = work[idxOf.at(k.elemId)];
        if (k.comp == 2) return e.radius;
        return (k.comp == 0) ? e.pts[k.idx].x : e.pts[k.idx].y;
    };
    auto setVar = [&](const VarKey& k, double v) {
        Element& e = work[idxOf.at(k.elemId)];
        if (k.comp == 2) e.radius = v;
        else if (k.comp == 0) e.pts[k.idx].x = v;
        else e.pts[k.idx].y = v;
    };

    // 求解前捕捉 FixedPoint 锚点与相切分支（内部/外部）
    std::vector<Vec3> fixedAnchor(recs.size());
    std::vector<char> tanExternal(recs.size(), 1);
    for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        auto ia = idxOf.find(r.idA);
        if (r.type == ConstraintType::FixedPoint && ia != idxOf.end())
            fixedAnchor[i] = anchorPoint(work[ia->second], r.idxA);
        if (r.type == ConstraintType::Tangent) {
            auto ja = idxOf.find(r.idA), jb = idxOf.find(r.idB);
            if (ja != idxOf.end() && jb != idxOf.end() &&
                isCircleLikeE(work[ja->second]) && isCircleLikeE(work[jb->second])) {
                const Element& A = work[ja->second]; const Element& B = work[jb->second];
                const double D = A.pts[0].distTo(B.pts[0]);
                tanExternal[i] = (D >= (A.radius + B.radius) - kSolveTol) ? 1 : 0;
            }
        }
    }

    // 残差函数：把 x 解包进 work，再按各约束累加残差（cidx 记录归属约束下标）
    auto residuals = [&](const std::vector<double>& xv, std::vector<double>& out, std::vector<int>& cidx) {
        for (const auto& k : varKeys) setVar(k, xv[varIndex.at(k)]);
        out.clear(); cidx.clear();
        auto push = [&](double v, int ci) { out.push_back(v); cidx.push_back(ci); };
        for (size_t i = 0; i < recs.size(); ++i) {
            const auto& r = recs[i];
            const int ci = (int)i;
            auto ia = idxOf.find(r.idA);
            auto ib = (r.idB >= 0) ? idxOf.find(r.idB) : idxOf.end();
            auto iy = (r.axisId >= 0) ? idxOf.find(r.axisId) : idxOf.end();
            const Element* a = (ia != idxOf.end()) ? &work[ia->second] : nullptr;
            const Element* b = (ib != idxOf.end()) ? &work[ib->second] : nullptr;
            const Element* axis = (iy != idxOf.end()) ? &work[iy->second] : nullptr;
            if (!a) continue;
            switch (r.type) {
                case ConstraintType::Coincident: {
                    const Vec3 pa = anchorPoint(*a, r.idxA), pb = anchorPoint(*b, r.idxB);
                    push(pa.x - pb.x, ci); push(pa.y - pb.y, ci); break;
                }
                case ConstraintType::Parallel:
                case ConstraintType::Collinear:
                case ConstraintType::Angle: {
                    const Vec3 da = a->pts[1] - a->pts[0], db = b->pts[1] - b->pts[0];
                    if (r.type == ConstraintType::Angle) {
                        const Vec3 da = a->pts[1] - a->pts[0], db = b->pts[1] - b->pts[0];
                        const double la = da.len2D(), lb = db.len2D();
                        const double cosT = (la > 1e-9 && lb > 1e-9) ? (da.x * db.x + da.y * db.y) / (la * lb) : 1.0;
                        push(cosT - std::cos(r.value * M_PI / 180.0), ci);   // cos(夹角) = cos(目标)
                    }
                    else if (r.type == ConstraintType::Parallel) push(da.x * db.y - da.y * db.x, ci);   // 平行：叉积=0
                    else {   // Collinear：平行 且 b 落到直线 A 上（共点）
                        push(da.x * db.y - da.y * db.x, ci);
                        push(signedDistPointToLine(b->pts[0], a->pts[0], a->pts[1]), ci);
                    }
                    break;
                }
                case ConstraintType::Perpendicular: {
                    {
                        // 方向点积 cos(夹角) 作残差：目标 0，全区间光滑（无夹角峰值尖点），
                        // 配合 addConstraint 中"近似平行时预旋转"打破对称性，LM 可稳定收敛到 90°。
                        const Vec3 da = a->pts[1] - a->pts[0], db = b->pts[1] - b->pts[0];
                        const double la = da.len2D(), lb = db.len2D();
                        if (la > 1e-9 && lb > 1e-9) push((da.x * db.x + da.y * db.y) / (la * lb), ci);
                    }
                    break;
                }
                case ConstraintType::Equal:
                case ConstraintType::EqualLength:
                case ConstraintType::EqualRadius:
                case ConstraintType::Dimensional: {
                    double target = (r.type == ConstraintType::Dimensional) ? r.value : sizeOf(*a);
                    push(sizeOf(*b) - target, ci); break;
                }
                case ConstraintType::Tangent: {
                    if (isLineLikeE(*a) && isCircleLikeE(*b)) {
                        const double d = signedDistPointToLine(b->pts[0], a->pts[0], a->pts[1]);
                        push(d * d - b->radius * b->radius, ci);
                    } else if (isCircleLikeE(*a) && isLineLikeE(*b)) {
                        const double d = signedDistPointToLine(a->pts[0], b->pts[0], b->pts[1]);
                        push(d * d - a->radius * a->radius, ci);
                    } else if (isCircleLikeE(*a) && isCircleLikeE(*b)) {
                        const double D = a->pts[0].distTo(b->pts[0]);
                        const double target = tanExternal[ci] ? (a->radius + b->radius) : std::fabs(a->radius - b->radius);
                        push(D - target, ci);
                    }
                    break;
                }
                case ConstraintType::Horizontal:
                    push(b->pts[1].y - b->pts[0].y, ci); break;
                case ConstraintType::Vertical:
                    push(b->pts[1].x - b->pts[0].x, ci); break;
                case ConstraintType::Concentric:
                    push(a->pts[0].x - b->pts[0].x, ci); push(a->pts[0].y - b->pts[0].y, ci); break;
                case ConstraintType::Symmetric: {
                    for (size_t k = 0; k < a->pts.size() && k < b->pts.size(); ++k) {
                        const Vec3 mp = mirrorPoint(a->pts[k], axis->pts[0], axis->pts[1]);
                        push(b->pts[k].x - mp.x, ci); push(b->pts[k].y - mp.y, ci);
                    }
                    break;
                }
                case ConstraintType::PointOnLine: {
                    const Vec3 pa = anchorPoint(*a, r.idxA);
                    push(signedDistPointToLine(pa, b->pts[0], b->pts[1]), ci); break;
                }
                case ConstraintType::PointOnCircle: {
                    const Vec3 pa = anchorPoint(*a, r.idxA);
                    push(pa.distTo(b->pts[0]) - b->radius, ci); break;
                }
                case ConstraintType::FixedPoint: {
                    const Vec3 pa = anchorPoint(*a, r.idxA);
                    push(pa.x - fixedAnchor[ci].x, ci); push(pa.y - fixedAnchor[ci].y, ci); break;
                }
                case ConstraintType::Midpoint: {
                    const Vec3 pa = anchorPoint(*a, r.idxA);
                    const Vec3 mid = (b->pts[0] + b->pts[1]) * 0.5;
                    push(pa.x - mid.x, ci); push(pa.y - mid.y, ci); break;
                }
                case ConstraintType::Distance: {
                    const Vec3 pa = anchorPoint(*a, r.idxA), pb = anchorPoint(*b, r.idxB);
                    push(pa.distTo(pb) - r.value, ci); break;
                }
                case ConstraintType::SymmetricPoint: {
                    const Vec3 pa = anchorPoint(*a, r.idxA), pb = anchorPoint(*b, r.idxB);
                    const Vec3 mid = (pa + pb) * 0.5;
                    const Vec3 ad = axis->pts[1] - axis->pts[0];
                    push(signedDistPointToLine(mid, axis->pts[0], axis->pts[1]), ci);   // 中点在轴上
                    const double L = ad.len2D();
                    push(L < 1e-12 ? 0.0 : ((pb.x - pa.x) * ad.x + (pb.y - pa.y) * ad.y) / L, ci); // AB⊥轴
                    break;
                }
                case ConstraintType::Fixed:
                default:
                    break;   // 固定：无残差；其余类型不应出现于此
            }
        }
    };

    std::vector<double> x(V);
    for (int k = 0; k < V; ++k) x[k] = getVar(varKeys[k]);

    // 雅可比构造（有限差分）闭包：给定 x，填充残差 r、归属 cidx 与雅可比 J
    auto buildJacobian = [&](const std::vector<double>& xv, std::vector<double>& r,
                             std::vector<int>& cidx, std::vector<std::vector<double>>& J) {
        residuals(xv, r, cidx);
        const int R = (int)r.size();
        J.assign(R, std::vector<double>(V, 0.0));
        std::vector<double> rp, rm; std::vector<int> dum;
        for (int k = 0; k < V; ++k) {
            const double h = opt.step * std::max(1.0, std::fabs(xv[k]));
            std::vector<double> xp = xv, xm = xv;
            xp[k] += h; xm[k] -= h;
            residuals(xp, rp, dum); residuals(xm, rm, dum);
            for (int m = 0; m < R; ++m) J[m][k] = (rp[m] - rm[m]) / (2.0 * h);
        }
    };

    double lambda = opt.lambda0;
    int it = 0;
    const double sqTol = opt.tol * opt.tol;
    std::vector<double> r; std::vector<int> cidx;
    std::vector<std::vector<double>> J;
    for (it = 0; it < opt.maxIterations; ++it) {
        buildJacobian(x, r, cidx, J);
        const int R = (int)r.size();
        if (R == 0) { diag.converged = true; break; }
        double cost = 0; for (double v : r) cost += v * v;
        if (cost < sqTol) { diag.converged = true; break; }
        // 正规方程 A = JᵀJ + λI，g = Jᵀr
        std::vector<std::vector<double>> A(V, std::vector<double>(V, 0.0));
        std::vector<double> g(V, 0.0);
        for (int kk = 0; kk < V; ++kk) {
            double s = 0;
            for (int mm = 0; mm < R; ++mm) s += J[mm][kk] * r[mm];
            g[kk] = s;
            for (int ll = 0; ll < V; ++ll) {
                double acc = 0;
                for (int mm = 0; mm < R; ++mm) acc += J[mm][kk] * J[mm][ll];
                A[kk][ll] = acc + (kk == ll ? lambda : 0.0);
            }
        }
        std::vector<double> dx;
        if (!solveLinearSystem(A, g, dx)) { lambda = std::min(lambda * 10.0, 1e12); continue; }
        std::vector<double> xNew = x;
        for (int k = 0; k < V; ++k) xNew[k] -= dx[k];
        std::vector<double> rNew; std::vector<int> dum2;
        residuals(xNew, rNew, dum2);
        double costNew = 0; for (double v : rNew) costNew += v * v;
        if (costNew < cost) {
            x = xNew; lambda = std::max(lambda / 10.0, 1e-12);
            if (costNew < sqTol) { diag.converged = true; break; }
        } else {
            lambda = std::min(lambda * 10.0, 1e12);
        }
    }
    diag.iterations = it;

    // 解包回 work，并写回模型
    for (const auto& k : varKeys) setVar(k, x[varIndex.at(k)]);
    std::set<int> touched;
    for (const auto& k : varKeys) touched.insert(k.elemId);
    for (int id : touched) {
        auto it = idxOf.find(id);
        if (it == idxOf.end()) continue;
        if (m && writeBack) m->updateElementQuiet(id, [&](Element& e) { e = work[it->second]; });
    }

    // 残差/满足度/自由度/冗余/冲突（用终态 x 重新构造 J）
    buildJacobian(x, r, cidx, J);
    const int R = (int)r.size();
    double cost = 0; for (double v : r) cost += v * v;
    diag.residualNorm = std::sqrt(cost);

    std::vector<double> perC(R > 0 ? *std::max_element(cidx.begin(), cidx.end()) + 1 : 0, 0.0);
    for (int m = 0; m < R; ++m) perC[cidx[m]] += r[m] * r[m];
    for (size_t i = 0; i < recs.size(); ++i)
        recs[i].satisfied = (std::sqrt(perC[(int)i]) <= kSolveTol);

    // 雅可比有效秩与自由度
    std::vector<std::vector<double>> A(V, std::vector<double>(V, 0.0));
    if (R > 0) {
        for (int kk = 0; kk < V; ++kk)
            for (int ll = 0; ll < V; ++ll) {
                double acc = 0;
                for (int mm = 0; mm < R; ++mm) acc += J[mm][kk] * J[mm][ll];
                A[kk][ll] = acc;
            }
        double maxDiag = 1.0;
        for (int k = 0; k < V; ++k) maxDiag = std::max(maxDiag, std::fabs(A[k][k]));
        const double thresh = 1e-6 * maxDiag;
        const int rank = rankOfSymmetric(A, thresh);
        diag.rank = rank; diag.variables = V; diag.dof = V - rank;

        // 冗余：去掉该约束整组残差后秩不变 → 约束冗余
        for (size_t ci = 0; ci < recs.size(); ++ci) {
            std::vector<std::vector<double>> Asub(V, std::vector<double>(V, 0.0));
            for (int kk = 0; kk < V; ++kk)
                for (int ll = 0; ll < V; ++ll) {
                    double acc = 0;
                    for (int mm = 0; mm < R; ++mm) if (cidx[mm] != (int)ci) acc += J[mm][kk] * J[mm][ll];
                    Asub[kk][ll] = acc;
                }
            const int subRank = rankOfSymmetric(Asub, thresh);
            if (subRank == rank) diag.redundantIdx.push_back((int)ci);
        }
    }
    // 冲突/过约束：求解后残差仍超容差
    for (size_t i = 0; i < recs.size(); ++i)
        if (!recs[i].satisfied) diag.infeasibleIdx.push_back((int)i);

    if (m && writeBack) m->setConstraints(recs);

    // ---- 逐实体自由度与欠/过约束分析 ----
    // 按图元收集雅可比列下标
    std::map<int, std::vector<int>> elemCols;
    for (int k = 0; k < V; ++k) elemCols[varKeys[k].elemId].push_back(k);

    // 哪些约束涉及哪个图元（用于提示与 overconstrained 标记）
    std::map<int, std::vector<int>> elemConstraints;
    for (size_t ci = 0; ci < recs.size(); ++ci) {
        const auto& rr = recs[ci];
        int ids[3] = {rr.idA, rr.idB, rr.axisId};
        for (int id : ids) if (id >= 0) elemConstraints[id].push_back((int)ci);
    }
    // 冲突/冗余约束涉及到的图元集合
    std::set<int> conflictElems, redundantElems;
    for (int ci : diag.infeasibleIdx) {
        const auto& rr = recs[ci];
        conflictElems.insert(rr.idA); if (rr.idB >= 0) conflictElems.insert(rr.idB); if (rr.axisId >= 0) conflictElems.insert(rr.axisId);
    }
    for (int ci : diag.redundantIdx) {
        const auto& rr = recs[ci];
        redundantElems.insert(rr.idA); if (rr.idB >= 0) redundantElems.insert(rr.idB); if (rr.axisId >= 0) redundantElems.insert(rr.axisId);
    }

    for (const auto& e : work) {
        if (e.type == ElementType::Constraint || e.type == ElementType::Dimension) continue;
        ElementConstraintInfo info;
        info.id = e.id;
        info.type = e.type;
        info.naturalDof = ConstraintSolver::naturalDof(e);
        auto it = elemCols.find(e.id);
        info.activeVars = (it != elemCols.end()) ? (int)it->second.size() : 0;
        info.constraints = elemConstraints.count(e.id) ? elemConstraints[e.id] : std::vector<int>{};
        if (info.activeVars > 0 && R > 0) {
            const auto& cols = it->second;
            const int av = info.activeVars;
            std::vector<std::vector<double>> M(av, std::vector<double>(av, 0.0));
            for (int a = 0; a < av; ++a)
                for (int b = 0; b < av; ++b) {
                    double acc = 0;
                    const int ca = cols[a], cb = cols[b];
                    for (int mm = 0; mm < R; ++mm) acc += J[mm][ca] * J[mm][cb];
                    M[a][b] = acc;
                }
            double maxDiag = 1.0; for (int a = 0; a < av; ++a) maxDiag = std::max(maxDiag, std::fabs(M[a][a]));
            info.appliedRank = rankOfSymmetric(M, 1e-6 * maxDiag);
        } else {
            info.appliedRank = 0;
        }
        info.remainingDof = (info.activeVars > 0) ? (info.activeVars - info.appliedRank) : info.naturalDof;
        // 过约束/冲突定位：参与「解不动」冲突约束的图元直接定位；
        // 仅冗余（秩不变）则要求自由度已用尽才算过约束。
        info.overconstrained = conflictElems.count(e.id) > 0 ||
                               (redundantElems.count(e.id) > 0 && info.remainingDof == 0);
        result.elementInfo.push_back(info);

        // 统计
        result.totalDof += info.naturalDof;
        if (info.activeVars > 0) {
            result.freeDof += info.remainingDof;
            if (info.remainingDof > 0) result.underconstrainedIds.push_back(e.id);
        }
        if (info.overconstrained) result.overconstrainedIds.push_back(e.id);
    }
    result.overDof = (int)diag.infeasibleIdx.size() + (int)diag.redundantIdx.size();
    // 系统状态判定
    {
        const bool hasConflict = !diag.infeasibleIdx.empty();
        const bool hasRedundant = !diag.redundantIdx.empty();
        const bool hasUnder = !result.underconstrainedIds.empty();
        if (hasConflict && hasUnder) result.state = ConstraintDiagnosis::Mixed;
        else if (hasConflict) result.state = ConstraintDiagnosis::Conflicting;
        else if (hasRedundant) result.state = ConstraintDiagnosis::OverConstrained;
        else if (hasUnder) result.state = ConstraintDiagnosis::UnderConstrained;
        else result.state = ConstraintDiagnosis::WellConstrained;
    }
    result.converged = diag.converged;
    result.iterations = diag.iterations;
    result.variables = diag.variables;
    result.rank = diag.rank;
    result.dof = diag.dof;
    result.residualNorm = diag.residualNorm;
    result.redundantIdx = diag.redundantIdx;
    result.infeasibleIdx = diag.infeasibleIdx;
    return result;
}

} // anonymous namespace

// 数值求解入口：把核心委托给 numericAnalyze（写回模式）。
SolveDiagnostics ConstraintSolver::solveAllNumeric(SketchModel& m, const SolveOptions& opt) {
    std::vector<Element> work = m.elements();
    auto recs = m.constraints();
    NumericResult res = numericAnalyze(&m, work, recs, true, opt);
    SolveDiagnostics d;
    d.converged = res.converged;
    d.iterations = res.iterations;
    d.variables = res.variables;
    d.rank = res.rank;
    d.dof = res.dof;
    d.residualNorm = res.residualNorm;
    d.redundantIdx = std::move(res.redundantIdx);
    d.infeasibleIdx = std::move(res.infeasibleIdx);
    d.elementInfo = std::move(res.elementInfo);
    return d;
}

// 图元固有几何自由度（与求解器变量基一致：点=2、线=4、圆/弧=3、多点图元=2×点数等）
int ConstraintSolver::naturalDof(const Element& e) {
    const int nPts = (int)e.pts.size();
    switch (e.type) {
        case ElementType::Point:      return 2;                 // x, y
        case ElementType::Line:       return 4;                 // 两端点各 2
        case ElementType::Circle:
        case ElementType::Arc:        return 3;                 // 圆心(2) + 半径(1)
        case ElementType::Rectangle:
        case ElementType::Polygon:
        case ElementType::Polyline:
        case ElementType::Spline:
        case ElementType::Boundary:   return 2 * nPts;          // 每个控制点 2
        case ElementType::Ellipse:    return 5;                 // 圆心(2)+长半轴(1)+短半轴(1)+转角(1)
        case ElementType::Xline:      return 3;                 // 通过点(2)+方向角(1)
        case ElementType::Hatch:      return 2 * nPts + 2;      // 边界(2n)+线角(1)+间距(1)
        case ElementType::Text:
        case ElementType::MText:
        case ElementType::Table:
        case ElementType::BlockRef:   return 2;                 // 锚点(2)；旋转/缩放不参与数值约束
        case ElementType::Constraint:
        case ElementType::Dimension:  return 0;
    }
    return 2 * nPts;
}

// 逐实体欠约束/过约束诊断：只读——在模型数据副本上跑同一套数值核心，不写回模型。
ConstraintDiagnosis ConstraintSolver::diagnose(const SketchModel& m) {
    SolveOptions opt;
    std::vector<Element> work = m.elements();          // 副本：诊断过程不触碰原模型
    auto recs = m.constraints();
    NumericResult res = numericAnalyze(nullptr, work, recs, false, opt);

    ConstraintDiagnosis d;
    d.state = res.state;
    d.totalDof = res.totalDof;
    d.freeDof = res.freeDof;
    d.overDof = res.overDof;
    d.elementInfo = std::move(res.elementInfo);
    d.underconstrainedIds = std::move(res.underconstrainedIds);
    d.overconstrainedIds = std::move(res.overconstrainedIds);

    const auto nameOf = [&work](int id) -> QString {
        for (const auto& e : work)
            if (e.id == id)
                return QString("#%1(%2)").arg(id).arg(QString::fromUtf8(elementTypeName(e.type)));
        return QString("#%1").arg(id);
    };
    const auto touchedConstraints = [&recs](const ElementConstraintInfo* info) -> QString {
        QString s; bool first = true;
        if (info)
            for (int ci : info->constraints) {
                if (ci < 0 || ci >= (int)recs.size()) continue;
                if (!first) s += QStringLiteral("、");
                s += QString::fromUtf8(constraintTypeName(recs[ci].type));
                first = false;
            }
        return s;
    };
    const auto findInfo = [&d](int id) -> const ElementConstraintInfo* {
        for (const auto& ei : d.elementInfo) if (ei.id == id) return &ei;
        return nullptr;
    };

    for (int id : d.underconstrainedIds) {
        const ElementConstraintInfo* info = findInfo(id);
        d.messages.push_back(QStringLiteral("图元 %1 约束不足：剩余 %2 个自由度（已有约束：%3）")
                                 .arg(nameOf(id)).arg(info ? info->remainingDof : 0)
                                 .arg(touchedConstraints(info)));
    }
    for (int id : d.overconstrainedIds) {
        const ElementConstraintInfo* info = findInfo(id);
        d.messages.push_back(QStringLiteral("图元 %1 过约束/冲突：约束条件数已达到甚至超过其自由度，仍存在不满足的约束（参与约束：%2），请移除矛盾或冗余项")
                                 .arg(nameOf(id)).arg(touchedConstraints(info)));
    }
    if (d.messages.empty()) {
        switch (d.state) {
            case ConstraintDiagnosis::WellConstrained:
                d.messages.push_back(QStringLiteral("所有被约束引用的图元均已充分且相容地约束，无欠约束/过约束。"));
                break;
            case ConstraintDiagnosis::Conflicting:
                d.messages.push_back(QStringLiteral("存在互相冲突的约束：求解后残差仍不满足，请检查约束值是否矛盾。"));
                break;
            case ConstraintDiagnosis::OverConstrained:
                d.messages.push_back(QStringLiteral("存在冗余约束：可移除部分约束而不改变解集。"));
                break;
            default:
                break;
        }
    }
    return d;
}

// 系统状态的一句中文描述
QString ConstraintDiagnosis::stateText() const {
    switch (state) {
        case WellConstrained:  return QStringLiteral("完全约束：被引用图元自由度恰好用尽，无冲突。");
        case UnderConstrained: return QStringLiteral("欠约束：存在自由度未被约束限制的图元。");
        case OverConstrained:  return QStringLiteral("过约束：存在冗余约束。");
        case Conflicting:      return QStringLiteral("冲突：约束之间互相矛盾，无法同时满足。");
        case Mixed:            return QStringLiteral("混合状态：既有欠约束图元，又存在过约束/冲突约束。");
    }
    return QStringLiteral("未知状态");
}
SolveReport ConstraintSolver::solveAll(SketchModel& m, int maxIterations) {
    SolveOptions opt;
    opt.maxIterations = std::max(maxIterations, 40);   // 数值迭代通常需要更多轮次
    const SolveDiagnostics d = solveAllNumeric(m, opt);
    SolveReport rep;
    auto recs = m.constraints();
    rep.total = (int)recs.size();
    rep.diag.ran = true;
    rep.diag.converged = d.converged;
    rep.diag.iterations = d.iterations;
    rep.diag.rank = d.rank;
    rep.diag.dof = d.dof;
    rep.diag.residualNorm = d.residualNorm;
    rep.diag.redundantCount = (int)d.redundantIdx.size();
    rep.diag.redundantIdx = d.redundantIdx;
    rep.diag.infeasibleIdx = d.infeasibleIdx;
    for (const auto& r : recs) {
        if (r.satisfied) rep.solved++;
        else rep.failures.push_back({r.idA, r.idB, r.type,
                                     failReason(r, QStringLiteral("迭代求解后仍未满足（可能过约束或与其上其它约束冲突）"))});
    }
    return rep;
}

std::vector<ConstraintDescriptor> ConstraintSolver::constraintDescriptors() {
    return {
        {ConstraintType::Coincident,    "coincident",    "重合",       "任意有点图元（线/圆/弧/点…）",      "idxA/idxB：参与重合的控制点索引",          "两端指定控制点重合"},
        {ConstraintType::Parallel,      "parallel",      "平行",       "两直线",                           "—",                                      "两直线方向平行"},
        {ConstraintType::Perpendicular, "perpendicular", "垂直",       "两直线",                           "—",                                      "两直线互相垂直"},
        {ConstraintType::Equal,         "equal",         "相等",       "可度量图元（直线长度/圆半径）",     "—",                                      "两端尺寸相等（长度或半径）"},
        {ConstraintType::Tangent,       "tangent",       "相切",       "直线↔圆 / 圆↔圆",                  "—",                                      "相切接触（切于一点）"},
        {ConstraintType::Horizontal,    "horizontal",    "水平",       "直线",                             "—",                                      "直线水平（Δy=0）"},
        {ConstraintType::Vertical,      "vertical",      "竖直",       "直线",                             "—",                                      "直线竖直（Δx=0）"},
        {ConstraintType::Fixed,         "fixed",         "固定",       "任意图元",                         "—",                                      "整体锁定，求解器不可移动"},
        {ConstraintType::Concentric,    "concentric",    "同心",       "两圆/弧",                          "—",                                      "圆心重合"},
        {ConstraintType::Symmetric,     "symmetric",     "对称",       "两同构图元",                       "axisId：对称轴（直线）",                    "两图元关于轴线整体镜像"},
        {ConstraintType::Dimensional,   "dimensional",   "尺寸",       "直线/圆/弧",                       "value：目标长度/半径",                      "锁定图元自身尺寸"},
        {ConstraintType::Collinear,     "collinear",     "共线",       "两直线",                           "—",                                      "两线落在同一直线上（平行且共点）"},
        {ConstraintType::EqualLength,   "equallength",   "等长",       "两线段",                           "—",                                      "两线段长度相等"},
        {ConstraintType::EqualRadius,   "equalradius",   "等半径",     "两圆/弧",                          "—",                                      "两图元半径相等"},
        {ConstraintType::PointOnLine,   "pointonline",   "点在线上",   "点 ↔ 直线",                        "idxA：点所在图元的控制点索引",              "指定点落在直线上"},
        {ConstraintType::PointOnCircle, "pointoncircle", "点在圆/弧上", "点 ↔ 圆/弧",                       "idxA：点索引",                             "指定点到圆心距离等于半径"},
        {ConstraintType::FixedPoint,    "fixedpoint",    "固定点",     "任意有点图元",                     "idxA：锁定的控制点索引",                    "把某控制点锚定在当前位置"},
        {ConstraintType::Midpoint,      "midpoint",      "中点",       "点 ↔ 线段",                        "idxA：中点所在图元的控制点索引",            "指定点恰为线段中点"},
        {ConstraintType::Distance,      "distance",      "距离",       "两点/锚点",                        "value：目标间距(mm)；idxA/idxB：锚点索引",  "两锚点间距锁定为给定值"},
        {ConstraintType::Angle,         "angle",         "角度",       "两直线",                           "value：目标夹角(度)",                       "两直夹角锁定为给定度数"},
        {ConstraintType::SymmetricPoint,"symmetricpoint","点对称",     "两点 ↔ 轴",                        "axisId：对称轴；idxA/idxB：两点索引",       "两点关于轴线互成镜像"},
    };
}

const ConstraintDescriptor* ConstraintSolver::constraintDescriptor(ConstraintType t) {
    static const auto v = constraintDescriptors();
    for (const auto& d : v) if (d.type == t) return &d;
    return nullptr;
}

} // namespace SketchCAD
