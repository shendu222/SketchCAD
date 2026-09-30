#include "Snap.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>
#include <QKeySequence>
#include <QJsonObject>

namespace SketchCAD {
namespace snap {

// 捕捉优先级：数值越大越优先。当多个类型同时落入容差时先按优先级、同优先级再比距离。
int snapPriority(SnapType t) {
    switch (t) {
        // 显式「点状」捕捉（语义最明确，压在其它之上）
        case SnapType::Node:          return 100;
        case SnapType::Insertion:     return 100;
        case SnapType::Quadrant:      return 100;
        case SnapType::Center:        return 95;
        case SnapType::Endpoint:      return 95;
        // 严格几何交点
        case SnapType::Intersection:  return 90;
        case SnapType::Apparent:      return 85;
        // 需推导的切/垂
        case SnapType::Tangent:       return 80;
        case SnapType::Perpendicular: return 80;
        // 延伸 / 平行 / 中点：依赖上下文推导，低于点状捕捉
        case SnapType::Extension:     return 70;
        case SnapType::Parallel:      return 70;
        case SnapType::Midpoint:      return 60;
        case SnapType::MidBetween:    return 60;
        // 兜底最近点（永远不该压过点状捕捉）
        case SnapType::Nearest:       return 10;
        case SnapType::None:          return -1;
    }
    return -1;
}

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
        case ElementType::Point:
            // 点实体：唯一的几何锚点即其位置，供 Node 捕捉
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
    const double phi   = (P - C).angleRad();
    const double alpha = std::acos(std::min(1.0, r / d));
    int n = 0;
    for (double ang : {phi + alpha, phi - alpha}) {
        out[n++] = Vec3(C.x + r * std::cos(ang), C.y + r * std::sin(ang), 0);
        if (n == 2) break;
    }
    return n;
}

std::vector<Vec3> quadrantPoints(const Vec3& c, double r, double startDeg, double endDeg, bool isArc) {
    std::vector<Vec3> out;
    const double q[4] = {0.0, 90.0, 180.0, 270.0};
    for (double ang : q) {
        if (isArc && !geo::angleInSweep(ang, startDeg, endDeg - startDeg)) continue;
        const double a = ang * M_PI / 180.0;
        out.push_back(Vec3(c.x + r * std::cos(a), c.y + r * std::sin(a), c.z));
    }
    return out;
}

// 取图元的「代表性方向线段」两端（用于外观交点的无限延长求交）。
// 圆/弧不取（单独走直线×整圆分支）。
static bool segEnds(const Element& e, Vec3& a, Vec3& b) {
    switch (e.type) {
        case ElementType::Line:
        case ElementType::Xline:
        case ElementType::Rectangle:
        case ElementType::Polygon:
        case ElementType::Spline:
        case ElementType::Polyline:
        case ElementType::Boundary:
            if (e.pts.size() < 2) return false;
            a = e.pts[0]; b = e.pts[1];
            return true;
        default:
            return false;
    }
}

// 无限直线与整圆的全部交点（外观交点用：不受线段范围限制）
static std::vector<Vec3> lineCircleAll(const Vec3& p1, const Vec3& p2, const Vec3& c, double r) {
    std::vector<Vec3> out;
    const Vec3 d = p2 - p1;
    const double L2 = d.x * d.x + d.y * d.y;
    if (L2 < 1e-18) return out;
    const Vec3 f = p1 - c;
    const double bb = 2.0 * (d.x * f.x + d.y * f.y);
    const double cc = (f.x * f.x + f.y * f.y) - r * r;
    const double disc = bb * bb - 4.0 * L2 * cc;   // 二次项系数 = L2
    if (disc < 0) return out;
    const double sq = std::sqrt(disc);
    for (double t : {(-bb + sq) / (2.0 * L2), (-bb - sq) / (2.0 * L2)})
        out.push_back(p1 + d * t);
    return out;
}

// 段上的最近点（带归一化参数 t：0=pts[0] 端，1=pts[1] 端；不钳制，可越界用于延伸判定）
static Vec3 projOnSegParam(const Vec3& w, const Vec3& a, const Vec3& b, double& t) {
    const Vec3 ab = b - a;
    const double L2 = ab.x * ab.x + ab.y * ab.y;
    if (L2 < 1e-18) { t = 0.0; return a; }
    t = ((w.x - a.x) * ab.x + (w.y - a.y) * ab.y) / L2;
    return a + ab * t;
}

// 图元上离 w 最近的点（Nearest 用：端点也可命中）。nr 返回法线方向（单位向量，圆/弧为径向）。
static Vec3 nearestPointOnElement(const Element& e, const Vec3& w, Vec3& nr) {
    nr = Vec3(1, 0, 0);
    const auto segNearest = [&](const Vec3& a, const Vec3& b) -> Vec3 {
        double t; Vec3 p = projOnSegParam(w, a, b, t);
        Vec3 d = (b - a).normalized();
        nr = d.perp();   // 段的法线方向
        return p;
    };
    switch (e.type) {
        case ElementType::Line:
        case ElementType::Xline:
            if (e.pts.size() >= 2) return segNearest(e.pts[0], e.pts[1]);
            break;
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) break;
            const Vec3 p0 = e.pts[0], p1 = e.pts[1];
            const Vec3 corners[4] = {p0, Vec3(p1.x, p0.y, 0), p1, Vec3(p0.x, p1.y, 0)};
            Vec3 best; double bd = 1e30; Vec3 bn;
            for (int k = 0; k < 4; ++k) {
                Vec3 p = segNearest(corners[k], corners[(k + 1) % 4]);
                const double d = p.distTo(w);
                if (d < bd) { bd = d; best = p; bn = nr; }
            }
            nr = bn; return best;
        }
        case ElementType::Polygon:
        case ElementType::Spline:
        case ElementType::Boundary:
        case ElementType::Polyline: {
            Vec3 best; double bd = 1e30; Vec3 bn;
            const size_t n = e.pts.size();
            const bool closed = e.type == ElementType::Polygon || e.type == ElementType::Boundary || e.closed;
            for (size_t k = 0; k + 1 < n; ++k) {
                Vec3 p = segNearest(e.pts[k], e.pts[k + 1]);
                const double d = p.distTo(w);
                if (d < bd) { bd = d; best = p; bn = nr; }
            }
            if (closed && n >= 2) {
                Vec3 p = segNearest(e.pts[n - 1], e.pts[0]);
                const double d = p.distTo(w);
                if (d < bd) { bd = d; best = p; bn = nr; }
            }
            nr = bn; return best;
        }
        case ElementType::Circle:
        case ElementType::Arc: {
            if (e.pts.empty()) break;
            const Vec3 c = e.pts[0];
            Vec3 dir = (w - c);
            if (dir.len2D() < 1e-12) { nr = Vec3(1, 0, 0); return c + Vec3(e.radius, 0, 0); }
            const Vec3 rad = dir.normalized();
            nr = rad;   // 圆的法线 = 径向
            Vec3 p = c + rad * e.radius;
            if (e.type == ElementType::Arc) {
                const double ang = rad.angleDeg();
                if (!geo::angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle))
                    p = c;   // 落在弧段外：回退圆心（由 Endpoint 兜底）
            }
            return p;
        }
        case ElementType::Ellipse: {
            // 椭圆：采样取最近（采样密度够用于捕捉）
            const auto pts = geo::sampleEllipse(e.pts.empty() ? Vec3() : e.pts[0], e.radius, e.radiusY, e.rotation, 96);
            Vec3 best; double bd = 1e30;
            for (const auto& p : pts) { const double d = p.distTo(w); if (d < bd) { bd = d; best = p; } }
            if (!pts.empty()) nr = (best - e.pts[0]).normalized();
            return best;
        }
        case ElementType::Point:
            if (!e.pts.empty()) { nr = Vec3(1, 0, 0); return e.pts[0]; }
            break;
        default:
            break;
    }
    return w;
}

bool perpendicularFoot(const Vec3& w, const Element& e, Vec3& foot) {
    switch (e.type) {
        case ElementType::Line:
        case ElementType::Xline:
            if (e.pts.size() >= 2) {
                double t; foot = projOnSegParam(w, e.pts[0], e.pts[1], t);
                return t > 1e-6 && t < 1.0 - 1e-6;   // 严格落在段内，端点交给 Endpoint
            }
            return false;
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) return false;
            const Vec3 p0 = e.pts[0], p1 = e.pts[1];
            const Vec3 corners[4] = {p0, Vec3(p1.x, p0.y, 0), p1, Vec3(p0.x, p1.y, 0)};
            double bd = 1e30; bool ok = false;
            for (int k = 0; k < 4; ++k) {
                double t; Vec3 p = projOnSegParam(w, corners[k], corners[(k + 1) % 4], t);
                if (t > 1e-6 && t < 1.0 - 1e-6) {
                    const double d = p.distTo(w);
                    if (d < bd) { bd = d; foot = p; ok = true; }
                }
            }
            return ok;
        }
        case ElementType::Polygon:
        case ElementType::Spline:
        case ElementType::Boundary:
        case ElementType::Polyline: {
            Vec3 best; double bd = 1e30; bool ok = false;
            const size_t n = e.pts.size();
            const bool closed = e.type == ElementType::Polygon || e.type == ElementType::Boundary || e.closed;
            for (size_t k = 0; k + 1 < n; ++k) {
                double t; Vec3 p = projOnSegParam(w, e.pts[k], e.pts[k + 1], t);
                if (t > 1e-6 && t < 1.0 - 1e-6) {
                    const double d = p.distTo(w);
                    if (d < bd) { bd = d; best = p; ok = true; }
                }
            }
            if (closed && n >= 2) {
                double t; Vec3 p = projOnSegParam(w, e.pts[n - 1], e.pts[0], t);
                if (t > 1e-6 && t < 1.0 - 1e-6) {
                    const double d = p.distTo(w);
                    if (d < bd) { bd = d; best = p; ok = true; }
                }
            }
            if (ok) foot = best;
            return ok;
        }
        case ElementType::Circle:
        case ElementType::Arc: {
            if (e.pts.empty()) return false;
            const Vec3 c = e.pts[0];
            Vec3 dir = w - c;
            if (dir.len2D() < 1e-12) return false;
            const Vec3 rad = dir.normalized();
            Vec3 p = c + rad * e.radius;
            if (e.type == ElementType::Arc) {
                const double ang = rad.angleDeg();
                if (!geo::angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle)) return false;
            }
            foot = p;
            return true;
        }
        case ElementType::Ellipse: {
            const auto pts = geo::sampleEllipse(e.pts.empty() ? Vec3() : e.pts[0], e.radius, e.radiusY, e.rotation, 96);
            Vec3 best; double bd = 1e30; bool ok = false;
            for (const auto& p : pts) { const double d = p.distTo(w); if (d < bd) { bd = d; best = p; ok = true; } }
            if (ok) foot = best;
            return ok;
        }
        default:
            return false;
    }
}

SnapResult snap(const std::vector<Element>& elems, const Vec3& w,
               const Vec3* otherEnd, double tol, const SnapConfig& cfg,
               const Vec3* refA, const Vec3* refB) {
    SnapResult best;
    best.point = w;
    double bestD = tol;
    int    bestRank = -1;

    const auto consider = [&](const Vec3& p, SnapType tp, int a, int b = -1, const Vec3& dir = Vec3()) {
        const double d = p.distTo(w);
        if (d > tol) return;
        const int r = snapPriority(tp);
        if (r > bestRank || (r == bestRank && d < bestD)) {
            bestRank = r; bestD = d;
            best.point = p; best.snapped = true; best.type = tp;
            best.elemA = a; best.elemB = b; best.dir = dir;
        }
    };

    const auto isAnno = [](ElementType t) {
        return t == ElementType::Constraint || t == ElementType::Dimension;
    };
    // 各捕捉分支已各自内联计算方向（平行/延伸/垂足/最近），无需统一 segDir 辅助

    // 1) 端点 / 圆心 / 插入点 / 节点 捕捉
    if (cfg.endpoint || cfg.center || cfg.insertion || cfg.node) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            // 节点：仅 Point 实体
            if (e.type == ElementType::Point) {
                if (cfg.node && !e.pts.empty()) consider(e.pts[0], SnapType::Node, e.id);
                continue;
            }
            const bool isCenter = (e.type == ElementType::Circle || e.type == ElementType::Arc);
            const std::vector<Vec3> hs = snapHandles(e);
            for (size_t i = 0; i < hs.size(); ++i) {
                if (isCenter && i == 0) { if (cfg.center)    consider(hs[i], SnapType::Center, e.id); }
                else                      { if (cfg.endpoint) consider(hs[i], SnapType::Endpoint, e.id); }
            }
            // 插入点：块引用 / 文字 / 多行文字 / 标注
            if (cfg.insertion && (e.type == ElementType::BlockRef || e.type == ElementType::Text ||
                                  e.type == ElementType::MText || e.type == ElementType::Dimension)) {
                if (!e.pts.empty()) consider(e.pts[0], SnapType::Insertion, e.id);
            }
        }
    }

    // 2) 象限点（圆/弧：0/90/180/270° 方位）
    if (cfg.quadrant) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            if (e.type == ElementType::Circle && !e.pts.empty()) {
                for (const Vec3& q : quadrantPoints(e.pts[0], e.radius, 0, 0, false))
                    consider(q, SnapType::Quadrant, e.id);
            } else if (e.type == ElementType::Arc && !e.pts.empty()) {
                for (const Vec3& q : quadrantPoints(e.pts[0], e.radius, e.startAngle, e.endAngle, true))
                    consider(q, SnapType::Quadrant, e.id);
            }
        }
    }

    // 3) 单段中点（已有类型）
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

    // 4) 两点间中点（需两个参考点）
    if (cfg.midBetween && refA && refB) {
        consider(Vec3((refA->x + refB->x) / 2.0, (refA->y + refB->y) / 2.0, 0.0),
                 SnapType::MidBetween, -1, -1);
    }

    // 5) 交点捕捉（粗筛：两个图元都需离光标足够近，避免 O(n^2) 全量求交）
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

    // 6) 切点捕捉（需已知另一端点：由 otherEnd 向圆/弧作切线）
    if (cfg.tangent && otherEnd) {
        for (const auto& e : elems) {
            if (!e.visible) continue;
            if (e.type != ElementType::Circle && e.type != ElementType::Arc) continue;
            if (e.pts.empty()) continue;
            Vec3 out[2];
            const int n = tangentPointsFromPointToCircle(*otherEnd, e.pts[0], e.radius, out);
            for (int k = 0; k < n; ++k) {
                if (e.type == ElementType::Arc) {
                    const double ang = (out[k] - e.pts[0]).angleDeg();
                    if (!geo::angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle)) continue;
                }
                consider(out[k], SnapType::Tangent, e.id);
            }
        }
    }

    // 7) 垂足捕捉（由光标向图元作垂线）
    if (cfg.perpendicular) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            Vec3 foot;
            if (perpendicularFoot(w, e, foot)) consider(foot, SnapType::Perpendicular, e.id);
        }
    }

    // 8) 最近点捕捉（兜底）
    if (cfg.nearest) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            Vec3 nr;
            const Vec3 p = nearestPointOnElement(e, w, nr);
            if (p.distTo(w) > 1e-9) consider(p, SnapType::Nearest, e.id);
        }
    }

    // 9) 外观交点：两图元「无限延长」后相交、但线段本身不相交处（平面投影交点）
    if (cfg.apparent) {
        const double broad = tol * 6.0;
        for (size_t i = 0; i < elems.size(); ++i) {
            const Element& A = elems[i];
            if (!A.visible || isAnno(A.type)) continue;
            if (geo::distanceToElement(A, w) > broad) continue;
            for (size_t j = i + 1; j < elems.size(); ++j) {
                const Element& B = elems[j];
                if (!B.visible || isAnno(B.type)) continue;
                if (geo::distanceToElement(B, w) > broad) continue;
                if (!geo::intersectElements(A, B).empty()) continue;   // 真相交交给 Intersection
                // 取两图元方向用于无限延长求交
                Vec3 a0, a1, b0, b1;
                if (!segEnds(A, a0, a1) || !segEnds(B, b0, b1)) continue;
                // 直线×直线：无限延长相交
                Vec3 ip;
                if (math::lineLineIntersect(a0, a1, b0, b1, ip))
                    consider(ip, SnapType::Apparent, A.id, B.id);
                // 直线×圆：整圆与无限直线相交（即便线段不穿圆）
                else if (A.type == ElementType::Circle || A.type == ElementType::Arc) {
                    std::vector<Vec3> cps = lineCircleAll(b0, b1, A.pts[0], A.radius);
                    for (const Vec3& p : cps) consider(p, SnapType::Apparent, A.id, B.id);
                } else if (B.type == ElementType::Circle || B.type == ElementType::Arc) {
                    std::vector<Vec3> cps = lineCircleAll(a0, a1, B.pts[0], B.radius);
                    for (const Vec3& p : cps) consider(p, SnapType::Apparent, A.id, B.id);
                }
            }
        }
    }

    // 10) 延伸：直线/弧端点沿其方向延长线上的点
    if (cfg.extension) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            if (e.type == ElementType::Line || e.type == ElementType::Xline) {
                if (e.pts.size() < 2) continue;
                const Vec3 d = (e.pts[1] - e.pts[0]).normalized();
                // 从两端点出发、沿两段方向的延长射线
                const Vec3 rays[2][2] = { {e.pts[1], d}, {e.pts[0], -d} };
                for (const auto& ray : rays) {
                    double t; Vec3 p = projOnSegParam(w, ray[0], ray[0] + ray[1], t);
                    if (t > 1e-3) consider(p, SnapType::Extension, e.id);   // t>0 表示越过端点
                }
            } else if (e.type == ElementType::Arc && !e.pts.empty()) {
                // 弧：从两端点沿该处切线方向延伸
                const Vec3 c = e.pts[0];
                const double a0 = e.startAngle * M_PI / 180.0, a1 = e.endAngle * M_PI / 180.0;
                const Vec3 p0 = Vec3(c.x + e.radius * std::cos(a0), c.y + e.radius * std::sin(a0), 0);
                const Vec3 p1 = Vec3(c.x + e.radius * std::cos(a1), c.y + e.radius * std::sin(a1), 0);
                const Vec3 d0 = Vec3(-std::sin(a0), std::cos(a0), 0);   // 起端切线
                const Vec3 d1 = Vec3(-std::sin(a1), std::cos(a1), 0);   // 末端切线
                const Vec3 rays[2][2] = { {p0, d0}, {p1, d1} };
                for (const auto& ray : rays) {
                    double t; Vec3 p = projOnSegParam(w, ray[0], ray[0] + ray[1], t);
                    if (t > 1e-3) consider(p, SnapType::Extension, e.id);
                }
            }
        }
    }

    // 11) 平行：需已知首点(otherEnd) + 参照图元方向，结果 = 光标在某条过 otherEnd 且
    //     平行于参照的直线上的投影；dir 携带该方向供绘制命令锁定平行方位。
    if (cfg.parallel && otherEnd) {
        for (const auto& e : elems) {
            if (!e.visible || isAnno(e.type)) continue;
            Vec3 d0, d1;
            if (e.type == ElementType::Line || e.type == ElementType::Xline ||
                e.type == ElementType::Polygon || e.type == ElementType::Spline ||
                e.type == ElementType::Polyline || e.type == ElementType::Boundary) {
                if (e.pts.size() < 2) continue;
                const Vec3 d = (e.pts[1] - e.pts[0]).normalized();
                const Vec3 p = math::projPointLine(w, *otherEnd, *otherEnd + d);
                if (p.distTo(w) <= tol) consider(p, SnapType::Parallel, e.id, -1, d);
            } else if (e.type == ElementType::Circle || e.type == ElementType::Arc) {
                if (e.pts.empty()) continue;
                // 圆/弧的「平行」语义：以切线方向为参照（光标在过 otherEnd 的切向延长线上）
                const Vec3 d = Vec3(1, 0, 0);   // 退化：圆本身无单一方向，跳过方向锁定
                (void)d;
            }
        }
    }

    return best;
}

// 捕捉字段定义：捕捉切换快捷键、Shift+右键临时覆盖菜单、捕捉设置面板三处共用。
// jsonKey 与 SnapConfig 字段名严格一致，保证 toJson/fromJson 与面板/菜单永远同步。
std::vector<SnapFieldDef> snapFieldDefs() {
    return {
        { Qt::Key_E, QStringLiteral("端点(E)"),     "endpoint",     &SnapConfig::endpoint,      SnapType::Endpoint },
        { Qt::Key_T, QStringLiteral("切点(T)"),     "tangent",      &SnapConfig::tangent,       SnapType::Tangent },
        { Qt::Key_M, QStringLiteral("中点(M)"),     "midpoint",     &SnapConfig::midpoint,      SnapType::Midpoint },
        { Qt::Key_C, QStringLiteral("圆心(C)"),     "center",       &SnapConfig::center,        SnapType::Center },
        { Qt::Key_X, QStringLiteral("交点(X)"),     "intersection", &SnapConfig::intersection,  SnapType::Intersection },
        { Qt::Key_Q, QStringLiteral("象限(Q)"),     "quadrant",     &SnapConfig::quadrant,      SnapType::Quadrant },
        { Qt::Key_P, QStringLiteral("垂足(P)"),     "perpendicular",&SnapConfig::perpendicular, SnapType::Perpendicular },
        { Qt::Key_I, QStringLiteral("插入(I)"),     "insertion",    &SnapConfig::insertion,     SnapType::Insertion },
        { Qt::Key_N, QStringLiteral("节点(N)"),     "node",         &SnapConfig::node,          SnapType::Node },
        { Qt::Key_A, QStringLiteral("最近(A)"),     "nearest",      &SnapConfig::nearest,       SnapType::Nearest },
        { Qt::Key_O, QStringLiteral("外观(O)"),     "apparent",     &SnapConfig::apparent,      SnapType::Apparent },
        { Qt::Key_L, QStringLiteral("平行(L)"),     "parallel",     &SnapConfig::parallel,      SnapType::Parallel },
        { Qt::Key_W, QStringLiteral("延伸(W)"),     "extension",    &SnapConfig::extension,     SnapType::Extension },
        { Qt::Key_B, QStringLiteral("两点中点(B)"), "midBetween",   &SnapConfig::midBetween,    SnapType::MidBetween },
    };
}

QString snapTypeLabel(SnapType t) {
    for (const auto& d : snapFieldDefs())
        if (d.type == t) return d.label;
    return QStringLiteral("捕捉");
}

std::vector<SnapDescriptor> snapTypeDescriptors() {
    // 优先级沿用 snapPriority()，数值越大越优先；同优先级比距离。
    // 吸附判定：在容差范围内收集所有启用捕捉命中的候选点，先按优先级、再按到光标距离取最优。
    return {
        {SnapType::Endpoint,     "endpoint",     "端点",     "直线/弧/矩形/多边形/样条/折线的端点或顶点",     snapPriority(SnapType::Endpoint),
         "取图元控制点（端点/顶点）中与光标最近者；点状特征，优先级最高一档"},
        {SnapType::Center,       "center",       "圆心",     "圆/圆弧",                                       snapPriority(SnapType::Center),
         "取圆心；与 Endpoint 同档（点状特征）"},
        {SnapType::Intersection, "intersection", "交点",     "任意两图元真实相交处（线×线/线×圆/圆×圆…）",     snapPriority(SnapType::Intersection),
         "取两图元几何相交点；严格交点，优先级次之"},
        {SnapType::Apparent,     "apparent",     "外观交点", "两图元延长线/投影相交处（本体不相交）",           snapPriority(SnapType::Apparent),
         "取两图元无限延长（投影）的交点；仅当延长后相交时命中，优先级低于真实交点"},
        {SnapType::Tangent,      "tangent",      "切点",     "由已知另一端点向圆/弧作切线的切点",               snapPriority(SnapType::Tangent),
         "以 otherEnd 为起点向圆/弧作切线，返回切点；需绘制命令提供另一端点"},
        {SnapType::Perpendicular,"perpendicular","垂足",     "由光标向图元作垂线的垂足",                       snapPriority(SnapType::Perpendicular),
         "取光标到图元的最近垂足；落在线段/弧段内才有效"},
        {SnapType::Quadrant,     "quadrant",     "象限点",   "圆/弧的 0°/90°/180°/270° 方位",                 snapPriority(SnapType::Quadrant),
         "取圆/弧半径手柄四方位（象限点）"},
        {SnapType::Insertion,    "insertion",    "插入点",   "块引用/文字/多行文字/标注的插入点",             snapPriority(SnapType::Insertion),
         "取图元插入锚点（pts[0]）"},
        {SnapType::Node,         "node",         "节点",     "Point 图元的节点位置",                          snapPriority(SnapType::Node),
         "取 Point 图元的位置点"},
        {SnapType::Extension,    "extension",    "延伸点",   "直线/弧端点沿其方向的延长线上点",               snapPriority(SnapType::Extension),
         "沿图元方向从端点向外延伸的射线上点"},
        {SnapType::Parallel,     "parallel",     "平行",     "与参照图元平行的方位",                           snapPriority(SnapType::Parallel),
         "取沿参照图元方向的投影点 + 方向向量，用于绘制平行线"},
        {SnapType::Midpoint,     "midpoint",     "中点",     "单段线段/折边的中点",                           snapPriority(SnapType::Midpoint),
         "取线段中点"},
        {SnapType::MidBetween,   "midbetween",   "两点中点", "两参考点 a、b 的中点",                          snapPriority(SnapType::MidBetween),
         "取 refA 与 refB 两参考点的中点（需开启且提供两参考点）"},
        {SnapType::Nearest,      "nearest",      "最近点",   "图元上离光标最近的任意点（兜底）",              snapPriority(SnapType::Nearest),
         "取图元上离光标最近的点；优先级最低，作为其它捕捉均未命中时的兜底"},
    };
}

const SnapDescriptor* snapTypeDescriptor(SnapType t) {
    static const auto v = snapTypeDescriptors();
    for (const auto& d : v) if (d.type == t) return &d;
    return nullptr;
}

QJsonObject SnapConfig::toJson() const {
    QJsonObject o;
    for (const auto& d : snapFieldDefs())
        o[QLatin1String(d.jsonKey)] = this->*(d.field);
    return o;
}

SnapConfig SnapConfig::fromJson(const QJsonObject& o) {
    SnapConfig c;
    for (const auto& d : snapFieldDefs())
        if (o.contains(QLatin1String(d.jsonKey)))
            c.*(d.field) = o[QLatin1String(d.jsonKey)].toBool();
    return c;
}

// ======================== 追踪系统几何（极轴 / 对象追踪 / 临时追踪点）========================

std::vector<double> polarAngles(const TrackConfig& cfg, const Vec3* baseDir) {
    std::vector<double> out;
    auto add = [&](double a) {
        a = std::fmod(a, 360.0);
        if (a < 0) a += 360.0;
        for (double x : out)
            if (std::fabs(x - a) < 1e-6 || std::fabs(x - a - 360.0) < 1e-6 ||
                std::fabs(x - a + 360.0) < 1e-6) return;
        out.push_back(a);
    };
    double base = 0.0;
    // 相对上一段方向：以该方向的世界角为基准叠加增量角（首段无上一段则用绝对基准）
    if (cfg.polarRelative && baseDir && baseDir->len2D() > 1e-9)
        base = baseDir->angleDeg();
    for (double a = 0.0; a < 360.0 - 1e-6; a += cfg.polarIncrement)
        add(base + a);
    // 始终补齐正交 0/90/180/270 作为兜底引导（即便增量角不整除 90°）
    for (double o : {0.0, 90.0, 180.0, 270.0}) add(base + o);
    return out;
}

TrackResult track(const Vec3& w, const TrackConfig& cfg,
                  const Vec3* lastSegDir, const Vec3* polarBase,
                  const std::vector<Vec3>& trackPoints,
                  const std::vector<Vec3>& osnapAnchors, double tol) {
    TrackResult best;                       // 默认 hit=false
    double bestD = tol + 1e-9;              // 仅当候选落点在容差内才视为命中
    const auto consider = [&](const Vec3& p, TrackMode m, const Vec3& dir, int axis,
                              const Vec3& a, const Vec3& b = Vec3()) {
        const double d = p.distTo(w);
        if (d >= bestD) return;
        bestD = d; best.hit = true; best.point = p; best.mode = m;
        best.dir = dir; best.axis = axis; best.anchorA = a; best.anchorB = b;
    };

    // ---- POLAR：从基点沿各极轴角方向发出射线，光标投影到射线邻域即吸附 ----
    if (cfg.polar && polarBase) {
        const auto angs = polarAngles(cfg, lastSegDir);
        for (double deg : angs) {
            const double a = deg * M_PI / 180.0;
            const Vec3 dir(std::cos(a), std::sin(a), 0.0);
            const Vec3 v = w - *polarBase;
            const double proj = v.x * dir.x + v.y * dir.y;   // 沿射线投影长度
            if (proj <= 0) continue;                         // 仅取射线前方
            const Vec3 foot = *polarBase + dir * proj;       // 投影点（保持当前光标距离）
            const double perp = (v - dir * proj).len2D();    // 到射线的垂距
            if (perp <= tol) consider(foot, TrackMode::Polar, dir, -1, *polarBase);
        }
    }

    // ---- OTRACK + TK：锚点 = 已获取的对象追踪点 ∪ 用户放置的临时追踪点 ----
    if (cfg.objectTrack || !trackPoints.empty()) {
        struct Tagged { Vec3 p; TrackMode m; };
        std::vector<Tagged> anchors;
        if (cfg.objectTrack)
            for (const auto& a : osnapAnchors) anchors.push_back({a, TrackMode::ObjectTrack});
        for (const auto& t : trackPoints) anchors.push_back({t, TrackMode::TempTK});

        for (size_t i = 0; i < anchors.size(); ++i) {
            const Vec3& A = anchors[i].p;
            const TrackMode m = anchors[i].m;
            // 水平对齐线：y = A.y
            if (std::fabs(w.y - A.y) <= tol)
                consider(Vec3(w.x, A.y, 0), m, Vec3(1, 0, 0), 0, A);
            // 垂直对齐线：x = A.x
            if (std::fabs(w.x - A.x) <= tol)
                consider(Vec3(A.x, w.y, 0), m, Vec3(0, 1, 0), 1, A);
            // 与另一锚点的正交对齐线交点（A 水平 × B 垂直 → (B.x, A.y)；A 垂直 × B 水平 → (A.x, B.y)）
            for (size_t j = 0; j < anchors.size(); ++j) {
                if (j == i) continue;
                const Vec3& B = anchors[j].p;
                consider(Vec3(B.x, A.y, 0), m, Vec3(1, 0, 0), 0, A, B);
                consider(Vec3(A.x, B.y, 0), m, Vec3(0, 1, 0), 1, A, B);
            }
        }
    }
    return best;
}

// ======================== 输入约束几何（正交 / 栅格吸附）========================

InputResult applyInputConstraints(const Vec3& p, const Vec3* base,
                                  bool ortho, bool grid, double step) {
    InputResult r;
    r.point = p;
    if (step <= 1e-9) grid = false;               // 步长非法 → 栅格吸附退化为不生效
    const bool canOrtho = ortho && base;          // 正交需要基准点（首点无基准时不生效）
    if (!canOrtho && !grid) return r;             // ④ 都不开

    if (canOrtho && !grid) {
        // ① 仅正交：保留绝对值较大的分量，另一分量归零
        const Vec3 d = p - *base;
        if (std::fabs(d.x) >= std::fabs(d.y)) r.point = Vec3(p.x, base->y, 0);
        else                                  r.point = Vec3(base->x, p.y, 0);
        r.ortho = true;
        return r;
    }
    if (!canOrtho && grid) {
        // ② 仅栅格：两分量各自量化
        const double sx = std::round(p.x / step) * step;
        const double sy = std::round(p.y / step) * step;
        r.point = Vec3(sx, sy, 0);
        r.grid = true;
        return r;
    }
    // ③ 两者同时开：正交定方向（锁轴），栅格定长度（量化保留分量）
    const Vec3 d = p - *base;
    if (std::fabs(d.x) >= std::fabs(d.y)) {
        const double sx = std::round(p.x / step) * step;
        r.point = Vec3(sx, base->y, 0);           // 水平：x 量化、y 保持基准
    } else {
        const double sy = std::round(p.y / step) * step;
        r.point = Vec3(base->x, sy, 0);           // 竖直：y 量化、x 保持基准
    }
    r.ortho = true; r.grid = true;
    return r;
}

QJsonObject TrackConfig::toJson() const {
    QJsonObject o;
    o["polar"]         = polar;
    o["objectTrack"]   = objectTrack;
    o["polarIncrement"]= polarIncrement;
    o["polarRelative"] = polarRelative;
    return o;
}

TrackConfig TrackConfig::fromJson(const QJsonObject& o) {
    TrackConfig c;
    if (o.contains("polar"))          c.polar         = o["polar"].toBool();
    if (o.contains("objectTrack"))    c.objectTrack   = o["objectTrack"].toBool();
    if (o.contains("polarIncrement")) c.polarIncrement= o["polarIncrement"].toDouble(c.polarIncrement);
    if (o.contains("polarRelative"))  c.polarRelative = o["polarRelative"].toBool();
    return c;
}

} // namespace snap
} // namespace SketchCAD
