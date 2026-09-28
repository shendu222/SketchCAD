#include "Snap.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {
namespace snap {

std::vector<Vec3> snapHandles(const Element& e) {
    std::vector<Vec3> out;
    if (!e.visible) return out;
    switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() >= 2) { out.push_back(e.pts[0]); out.push_back(e.pts[1]); }
            break;
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) break;
            const double x0 = e.pts[0].x, y0 = e.pts[0].y;
            const double x1 = e.pts[1].x, y1 = e.pts[1].y;
            out = {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
            break;
        }
        case ElementType::Circle:
            // 仅圆心是真实几何点；半径手柄是虚构的辅助点，不参与捕捉
            if (!e.pts.empty()) out.push_back(e.pts[0]);
            break;
        case ElementType::Arc: {
            if (e.pts.empty()) break;
            const Vec3& c = e.pts[0];
            const double a0 = e.startAngle * M_PI / 180.0;
            const double a1 = e.endAngle   * M_PI / 180.0;
            out.push_back(c);
            out.push_back(Vec3(c.x + e.radius * std::cos(a0), c.y + e.radius * std::sin(a0), c.z));
            out.push_back(Vec3(c.x + e.radius * std::cos(a1), c.y + e.radius * std::sin(a1), c.z));
            break;
        }
        case ElementType::Polygon:
        case ElementType::Spline:
        case ElementType::Boundary:
            out = e.pts;
            break;
        case ElementType::Xline:
            // 构造线：无限线无端点，仅通过点是真实几何点（可被端点捕捉/联动拖拽）
            if (!e.pts.empty()) out.push_back(e.pts[0]);
            break;
        default:
            break;   // Constraint / Dimension 为注释性图元，不参与几何捕捉
    }
    return out;
}

int tangentPointsFromPointToCircle(const Vec3& P, const Vec3& C, double r, Vec3 out[2]) {
    if (r <= 1e-12) return 0;
    const double dx = P.x - C.x, dy = P.y - C.y;
    const double d = std::hypot(dx, dy);
    if (d < r - 1e-9) return 0;                 // 点在圆内：无实切线
    if (d < 1e-12)    return 0;                 // 与圆心重合：退化
    // 设 phi = 圆心指向 P 的方向角；切点方向角 = phi ± alpha，alpha = acos(r/d)
    const double phi   = (P - C).angleRad();
    const double alpha = std::acos(std::min(1.0, r / d));
    int n = 0;
    for (double ang : {phi + alpha, phi - alpha}) {
        out[n++] = Vec3(C.x + r * std::cos(ang), C.y + r * std::sin(ang), 0);
        if (n == 2) break;
    }
    return n;
}

SnapResult snap(const std::vector<Element>& elems, const Vec3& w,
                const Vec3* otherEnd, double tol, const SnapConfig& cfg) {
    SnapResult best;
    best.point = w;
    double bestD = tol;

    const auto consider = [&](const Vec3& p, SnapType tp, int a, int b = -1) {
        const double d = p.distTo(w);
        if (d < bestD) {
            bestD = d;
            best.point = p;
            best.snapped = true;
            best.type = tp;
            best.elemA = a;
            best.elemB = b;
        }
    };

    const auto isAnno = [](ElementType t) {
        return t == ElementType::Constraint || t == ElementType::Dimension;
    };

    // 1) 端点 / 圆心 捕捉（受 endpoint / center 开关门控）
    if (cfg.endpoint || cfg.center) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            const std::vector<Vec3> hs = snapHandles(e);
            const bool isCenter = (e.type == ElementType::Circle || e.type == ElementType::Arc);
            for (size_t i = 0; i < hs.size(); ++i) {
                if (isCenter && i == 0) { if (cfg.center)       consider(hs[i], SnapType::Center, e.id); }
                else                    { if (cfg.endpoint)     consider(hs[i], SnapType::Endpoint, e.id); }
            }
        }
    }

    // 1.5) 中点捕捉（受 midpoint 开关门控）：直线段、矩形四边、多边形/样条折边的中点
    if (cfg.midpoint) {
        auto mid = [](const Vec3& p, const Vec3& q) -> Vec3 {
            return Vec3((p.x + q.x) / 2.0, (p.y + q.y) / 2.0, 0.0);
        };
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            if (e.type == ElementType::Line && e.pts.size() >= 2) {
                consider(mid(e.pts[0], e.pts[1]), SnapType::Midpoint, e.id);
            } else if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
                const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
                consider(mid(Vec3(x0, y0, 0), Vec3(x1, y0, 0)), SnapType::Midpoint, e.id);
                consider(mid(Vec3(x1, y0, 0), Vec3(x1, y1, 0)), SnapType::Midpoint, e.id);
                consider(mid(Vec3(x1, y1, 0), Vec3(x0, y1, 0)), SnapType::Midpoint, e.id);
                consider(mid(Vec3(x0, y1, 0), Vec3(x0, y0, 0)), SnapType::Midpoint, e.id);
            } else if ((e.type == ElementType::Polygon || e.type == ElementType::Spline) && e.pts.size() >= 2) {
                for (size_t k = 0; k + 1 < e.pts.size(); ++k)
                    consider(mid(e.pts[k], e.pts[k + 1]), SnapType::Midpoint, e.id);
            }
        }
    }

    // 2) 交点捕捉（粗筛：两个图元都需离光标足够近，避免 O(n^2) 全量求交；受 intersection 门控）
    if (cfg.intersection) {
        const double broad = tol * 6.0;
        for (size_t i = 0; i < elems.size(); ++i) {
            const Element& A = elems[i];
            if (!A.visible || isAnno(A.type)) continue;
            if (geo::distanceToElement(A, w) > broad) continue;
            for (size_t j = i + 1; j < elems.size(); ++j) {
                const Element& B = elems[j];
                if (!B.visible || isAnno(B.type)) continue;
                if (geo::distanceToElement(B, w) > broad) continue;
                const std::vector<Vec3> ips = geo::intersectElements(A, B);
                for (const Vec3& p : ips) consider(p, SnapType::Intersection, A.id, B.id);
            }
        }
    }

    // 3) 切点捕捉（需已知另一端点：由 otherEnd 向圆/弧作切线；受 tangent 门控）
    if (cfg.tangent && otherEnd) {
        for (const auto& e : elems) {
            if (!e.visible) continue;
            if (e.type != ElementType::Circle && e.type != ElementType::Arc) continue;
            if (e.pts.empty()) continue;
            Vec3 out[2];
            const int n = tangentPointsFromPointToCircle(*otherEnd, e.pts[0], e.radius, out);
            for (int k = 0; k < n; ++k) {
                // 弧：切点必须落在弧段角度范围内
                if (e.type == ElementType::Arc) {
                    const double ang = (out[k] - e.pts[0]).angleDeg();
                    if (!geo::angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle)) continue;
                }
                consider(out[k], SnapType::Tangent, e.id);
            }
        }
    }

    return best;
}

} // namespace snap
} // namespace SketchCAD
