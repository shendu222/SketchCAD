// ============================================================================
// Inquiry.cpp —— 查询测量核心层实现
// ----------------------------------------------------------------------------
// 全部为只读纯函数。几何事实与渲染同源：
//   * 折线弧段走 geo::arcThrough 外接圆（与 GraphicsView 渲染一致）；
//   * 采样密度：圆/椭圆 64 段、弧 16 段/弧、闭合样条 Catmull-Rom 16 段/节点。
// 精度策略见 Inquiry.h 头注释。
// ============================================================================
#include "core/Inquiry.h"

#include <cmath>
#include <algorithm>
#include <limits>

#include "core/Geometry.h"

namespace SketchCAD::inquiry {

static constexpr double kPi = 3.14159265358979323846;

// ============================================================
// 多边形环底层（shoelace 家族）
// ============================================================

double polygonArea(const std::vector<Vec3>& ring) {
    const int n = (int)ring.size();
    if (n < 3) return 0;
    double A = 0;
    for (int i = 0; i < n; ++i) {
        const Vec3& a = ring[i];
        const Vec3& b = ring[(i + 1) % n];
        A += a.x * b.y - b.x * a.y;
    }
    return A * 0.5;
}

double polygonPerimeter(const std::vector<Vec3>& ring) {
    const int n = (int)ring.size();
    if (n < 2) return 0;
    double L = 0;
    for (int i = 0; i < n; ++i) L += ring[i].distTo(ring[(i + 1) % n]);
    return L;
}

Vec3 polygonCentroid(const std::vector<Vec3>& ring) {
    const int n = (int)ring.size();
    if (n == 0) return Vec3();
    if (n < 3) {   // 退化：顶点均值
        Vec3 c;
        for (const auto& p : ring) { c.x += p.x; c.y += p.y; }
        return Vec3(c.x / n, c.y / n, 0);
    }
    double A2 = 0, cx = 0, cy = 0;
    for (int i = 0; i < n; ++i) {
        const Vec3& a = ring[i];
        const Vec3& b = ring[(i + 1) % n];
        const double cr = a.x * b.y - b.x * a.y;
        A2 += cr;
        cx += (a.x + b.x) * cr;
        cy += (a.y + b.y) * cr;
    }
    if (std::fabs(A2) < 1e-12) {   // 零面积退化：顶点均值
        Vec3 c;
        for (const auto& p : ring) { c.x += p.x; c.y += p.y; }
        return Vec3(c.x / n, c.y / n, 0);
    }
    return Vec3(cx / (3.0 * A2), cy / (3.0 * A2), 0);
}

// 有符号版本（不归一化方向）：CCW 为正。供带孔区域的负贡献复用。
static void polygonInertiaSigned(const std::vector<Vec3>& ring,
                                 double& ix, double& iy, double& ixy) {
    const int n = (int)ring.size();
    ix = iy = ixy = 0;
    for (int i = 0; i < n; ++i) {
        const Vec3& a = ring[i];
        const Vec3& b = ring[(i + 1) % n];
        const double cr = a.x * b.y - b.x * a.y;
        ix  += (a.y * a.y + a.y * b.y + b.y * b.y) * cr;
        iy  += (a.x * a.x + a.x * b.x + b.x * b.x) * cr;
        ixy += (a.x * b.y + 2.0 * a.x * a.y + 2.0 * b.x * b.y + b.x * a.y) * cr;
    }
    ix /= 12.0;
    iy /= 12.0;
    ixy /= 24.0;
}

void polygonInertia(const std::vector<Vec3>& ring,
                    double& ix, double& iy, double& ixy) {
    ix = iy = ixy = 0;
    if (ring.size() < 3) return;
    std::vector<Vec3> r = ring;
    if (polygonArea(r) < 0) std::reverse(r.begin(), r.end());   // 归一 CCW
    polygonInertiaSigned(r, ix, iy, ixy);
}

// ============================================================
// 离散环（渲染同源）
// ============================================================

std::vector<Vec3> discretizeRing(const Element& e) {
    std::vector<Vec3> ring;
    switch (e.type) {
        case ElementType::Circle: {
            if (e.radius <= 0 || e.pts.empty()) break;
            constexpr int N = 64;
            ring.reserve(N);
            for (int i = 0; i < N; ++i) {
                const double t = 2.0 * kPi * i / N;
                ring.emplace_back(e.pts[0].x + e.radius * std::cos(t),
                                  e.pts[0].y + e.radius * std::sin(t), 0);
            }
            break;
        }
        case ElementType::Ellipse: {
            if (e.radius <= 0 || e.radiusY <= 0 || e.pts.empty()) break;
            const double rot = math::deg2rad(e.rotation);
            const double ca = std::cos(rot), sa = std::sin(rot);
            constexpr int N = 64;
            ring.reserve(N);
            for (int i = 0; i < N; ++i) {
                const double t = 2.0 * kPi * i / N;
                const double lx = e.radius * std::cos(t);
                const double ly = e.radiusY * std::sin(t);
                ring.emplace_back(e.pts[0].x + lx * ca - ly * sa,
                                  e.pts[0].y + lx * sa + ly * ca, 0);
            }
            break;
        }
        case ElementType::Rectangle: {
            if (!e.closed || e.pts.size() < 2) break;
            const double x0 = std::min(e.pts[0].x, e.pts[1].x);
            const double x1 = std::max(e.pts[0].x, e.pts[1].x);
            const double y0 = std::min(e.pts[0].y, e.pts[1].y);
            const double y1 = std::max(e.pts[0].y, e.pts[1].y);
            ring = { Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0) };
            break;
        }
        case ElementType::Polygon:
        case ElementType::Hatch:
            if (!e.closed && e.type == ElementType::Polygon) break;
            if (e.pts.size() < 3) break;
            ring = e.pts;
            break;
        case ElementType::Polyline: {
            if (!e.closed || e.pts.size() < 3) break;
            // 弧段 16 段/弧（与渲染 buildPath 一致）；环由 (i+1)%n 自动闭合
            ring = geo::samplePolyline(e.pts, e.arcFlags, true, 16);
            break;
        }
        default:
            break;
    }
    return ring;
}

// ============================================================
// DIST：两点测量
// ============================================================

DistResult distTwoPoints(const Vec3& a, const Vec3& b) {
    DistResult r;
    r.dx = b.x - a.x;
    r.dy = b.y - a.y;
    r.distance = std::hypot(r.dx, r.dy);
    r.angleDeg = geo::wrapDeg180(math::rad2deg(std::atan2(r.dy, r.dx)));
    return r;
}

// ============================================================
// AREA：封闭区域面积与周长
// ============================================================

AreaResult areaOfPolygon(const std::vector<Vec3>& ring) {
    AreaResult r;
    if (ring.size() < 3) { r.error = "点列模式至少需要 3 个点"; return r; }
    const double A = polygonArea(ring);
    if (std::fabs(A) < 1e-9) { r.error = "点列退化（共线或零面积）"; return r; }
    r.ok = true;
    r.area = std::fabs(A);
    r.perimeter = polygonPerimeter(ring);
    r.centroid = polygonCentroid(ring);
    return r;
}

// ---- 直线链围合区域：端点匹配串环 → shoelace -------------------------------
bool buildRingFromLines(const std::vector<Element>& lines,
                        std::vector<Vec3>& ring, double tol) {
    ring.clear();
    if (lines.size() < 3) return false;                 // 围成区域至少 3 条边
    for (const auto& e : lines)
        if (e.type != ElementType::Line || e.pts.size() < 2) return false;

    const double tol2 = tol * tol;
    auto close2 = [](const Vec3& a, const Vec3& b, double t2) {
        const double dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy <= t2;
    };

    // 贪心串环：任取一条作起点，反复找端点与"当前末端"相接的未用线段（自动换向）。
    // 环 = [起点, 每次接入线段的共享顶点, ...]：接入 D-E 时 D 是共享顶点、E 是新末端，
    // 环里要记录的是 D（多边形角点），新末端 E 留给下一次接入时再记录。
    std::vector<char> used(lines.size(), 0);
    const Vec3 start = lines[0].pts[0];
    Vec3 cur = lines[0].pts[1];
    ring.push_back(start);
    used[0] = 1;
    for (size_t got = 1; got < lines.size(); ++got) {
        bool found = false;
        for (size_t i = 0; i < lines.size() && !found; ++i) {
            if (used[i]) continue;
            const Vec3& a = lines[i].pts[0];
            const Vec3& b = lines[i].pts[1];
            if (close2(a, cur, tol2))      { ring.push_back(cur); cur = b; used[i] = 1; found = true; }
            else if (close2(b, cur, tol2)) { ring.push_back(cur); cur = a; used[i] = 1; found = true; }
        }
        if (!found) return false;                       // 存在缺口/分叉，串不起来
    }
    return close2(cur, start, tol2);                    // 末端须回到起点（封闭）
}

AreaResult areaOfClosedLineChain(const std::vector<Element>& segs) {
    AreaResult r;
    std::vector<Vec3> ring;
    if (!buildRingFromLines(segs, ring)) {
        r.error = QStringLiteral("所选直线未能首尾相接围成封闭环"
                                 "（需 ≥3 条直线且端点一一相接、无缺口/分叉）");
        return r;
    }
    return areaOfPolygon(ring);
}

AreaResult areaOfElement(const Element& e) {
    AreaResult r;
    switch (e.type) {
        case ElementType::Circle: {
            if (e.radius <= 0) { r.error = "圆半径非正"; return r; }
            r.ok = true;
            r.area = kPi * e.radius * e.radius;
            r.perimeter = 2.0 * kPi * e.radius;
            r.centroid = e.pts.empty() ? Vec3() : e.pts[0];
            return r;
        }
        case ElementType::Ellipse: {
            if (e.radius <= 0 || e.radiusY <= 0) { r.error = "椭圆半轴非正"; return r; }
            const double a = e.radius, b = e.radiusY;
            r.ok = true;
            r.area = kPi * a * b;
            // Ramanujan 周长近似（相对误差 < 5e-5）
            r.perimeter = kPi * (3.0 * (a + b) - std::sqrt((3.0 * a + b) * (a + 3.0 * b)));
            r.centroid = e.pts.empty() ? Vec3() : e.pts[0];
            return r;
        }
        case ElementType::Rectangle:
        case ElementType::Polygon: {
            if (!e.closed) { r.error = "图元已被剪切为开放轮廓，无法计算面积"; return r; }
            const auto ring = discretizeRing(e);
            if (ring.size() < 3) { r.error = "顶点不足"; return r; }
            r.ok = true;
            r.area = std::fabs(polygonArea(ring));
            r.perimeter = polygonPerimeter(ring);
            r.centroid = polygonCentroid(ring);
            return r;
        }
        case ElementType::Polyline: {
            if (!e.closed) { r.error = "折线未闭合，无法计算面积（可先执行 PEDIT Close）"; return r; }
            if (e.pts.size() < 3) { r.error = "折线顶点不足"; return r; }
            const double A = geo::polylineSignedArea(e.pts, e.arcFlags, true);
            if (std::fabs(A) < 1e-9) { r.error = "折线退化（零面积）"; return r; }
            r.ok = true;
            r.area = std::fabs(A);
            r.perimeter = geo::polylineArcLength(e.pts, e.arcFlags, true);
            const auto ring = discretizeRing(e);
            if (ring.size() >= 3) r.centroid = polygonCentroid(ring);
            return r;
        }
        case ElementType::Hatch: {
            if (e.pts.size() < 3) { r.error = "剖面区域外环顶点不足"; return r; }
            // 外环归一 CCW，内孔归一 CW（负贡献），有向公式统一处理
            std::vector<Vec3> outer = e.pts;
            if (polygonArea(outer) < 0) std::reverse(outer.begin(), outer.end());
            double A = polygonArea(outer);
            if (std::fabs(A) < 1e-9) { r.error = "剖面区域退化（零面积）"; return r; }
            double P = polygonPerimeter(outer);
            const Vec3 cOut = polygonCentroid(outer);
            double mx = A * cOut.x, my = A * cOut.y;   // 一阶矩
            for (const auto& h : e.holes) {
                if (h.size() < 3) continue;
                std::vector<Vec3> hole = h;
                if (polygonArea(hole) > 0) std::reverse(hole.begin(), hole.end()); // CW
                const double Ah = polygonArea(hole);
                A += Ah;                              // 扣孔
                P += polygonPerimeter(hole);          // 周长含内孔
                const Vec3 ch = polygonCentroid(hole);
                mx += Ah * ch.x;
                my += Ah * ch.y;
            }
            if (std::fabs(A) < 1e-9) { r.error = "剖面区域退化（扣孔后零面积）"; return r; }
            r.ok = true;
            r.area = std::fabs(A);
            r.perimeter = P;
            r.centroid = Vec3(mx / A, my / A, 0);
            return r;
        }
        default:
            r.error = QString("%1 不是封闭区域（支持：圆/椭圆/矩形/多边形/闭合折线/剖面区域）")
                          .arg(e.typeName());
            return r;
    }
}

// ============================================================
// MASSPROP：质量特性
// ============================================================

namespace {

// 环/复合区域 → 惯性矩（绕世界轴 + 形心轴）公共后处理
struct InertiaOut { double ix = 0, iy = 0, ixy = 0, ixBar = 0, iyBar = 0, ixyBar = 0; };

// 单环（先归一 CCW）：绕原点 → 平行轴移到形心
InertiaOut inertiaOfRing(const std::vector<Vec3>& ring, const Vec3& c, double area) {
    InertiaOut o;
    double ix0, iy0, ixy0;
    polygonInertia(ring, ix0, iy0, ixy0);
    o.ix = ix0;  o.iy = iy0;  o.ixy = ixy0;
    o.ixBar = ix0 - area * c.y * c.y;
    o.iyBar = iy0 - area * c.x * c.x;
    o.ixyBar = ixy0 - area * c.x * c.y;
    return o;
}

// 环的包围盒
QRectF bboxOfRing(const std::vector<Vec3>& ring) {
    double x0 = std::numeric_limits<double>::max(), y0 = x0;
    double x1 = -x0, y1 = -y0;
    for (const auto& p : ring) {
        x0 = std::min(x0, p.x); y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
    }
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

} // namespace

MassPropResult massPropOfElement(const Element& e) {
    MassPropResult r;
    const AreaResult ar = areaOfElement(e);
    if (!ar.ok) { r.error = ar.error; return r; }
    r.ok = true;
    r.area = ar.area;
    r.perimeter = ar.perimeter;
    r.centroid = ar.centroid;

    switch (e.type) {
        case ElementType::Circle: {
            const double cr = e.radius;
            r.bbox = QRectF(ar.centroid.x - cr, ar.centroid.y - cr, 2 * cr, 2 * cr);
            // 解析：Ix = Iy = πr⁴/4（形心轴），Ixy = 0；世界轴用平行轴定理
            r.ixBar = r.iyBar = kPi * cr * cr * cr * cr / 4.0;
            r.ixyBar = 0;
            r.ix  = r.ixBar  + r.area * ar.centroid.y * ar.centroid.y;
            r.iy  = r.iyBar  + r.area * ar.centroid.x * ar.centroid.x;
            r.ixy = r.ixyBar + r.area * ar.centroid.x * ar.centroid.y;
            return r;
        }
        case ElementType::Ellipse: {
            // 解析包围盒（旋转椭圆半轴投影）
            const double rot = math::deg2rad(e.rotation);
            const double a = e.radius, b = e.radiusY;
            const double ex = std::sqrt(a * a * std::cos(rot) * std::cos(rot)
                                      + b * b * std::sin(rot) * std::sin(rot));
            const double ey = std::sqrt(a * a * std::sin(rot) * std::sin(rot)
                                      + b * b * std::cos(rot) * std::cos(rot));
            r.bbox = QRectF(ar.centroid.x - ex, ar.centroid.y - ey, 2 * ex, 2 * ey);
            // 惯性矩：64 段离散环（任意旋转角下统一正确）
            const auto ring = discretizeRing(e);
            if (ring.size() >= 3) {
                const InertiaOut o = inertiaOfRing(ring, ar.centroid,
                                                   std::fabs(polygonArea(ring)));
                r.ix = o.ix; r.iy = o.iy; r.ixy = o.ixy;
                r.ixBar = o.ixBar; r.iyBar = o.iyBar; r.ixyBar = o.ixyBar;
            }
            return r;
        }
        default: {
            const auto ring = discretizeRing(e);
            if (ring.size() < 3) { r.ok = false; r.error = "无法离散封闭轮廓"; return r; }
            r.bbox = bboxOfRing(ring);
            if (e.type == ElementType::Hatch) {
                // 复合区域：外环 CCW 正贡献 + 内孔 CW 负贡献（绕原点叠加后平行轴移轴）
                std::vector<Vec3> outer = ring;
                if (polygonArea(outer) < 0) std::reverse(outer.begin(), outer.end());
                double ix0, iy0, ixy0;
                polygonInertiaSigned(outer, ix0, iy0, ixy0);
                double A0 = polygonArea(outer);
                for (const auto& h : e.holes) {
                    if (h.size() < 3) continue;
                    std::vector<Vec3> hole = h;
                    if (polygonArea(hole) > 0) std::reverse(hole.begin(), hole.end());
                    double hx, hy, hxy;
                    polygonInertiaSigned(hole, hx, hy, hxy);
                    ix0 += hx; iy0 += hy; ixy0 += hxy;
                    A0  += polygonArea(hole);
                }
                if (std::fabs(A0) < 1e-9) { r.ok = false; r.error = "剖面区域退化（扣孔后零面积）"; return r; }
                r.ix = ix0; r.iy = iy0; r.ixy = ixy0;
                r.ixBar  = ix0  - r.area * r.centroid.y * r.centroid.y;
                r.iyBar  = iy0  - r.area * r.centroid.x * r.centroid.x;
                r.ixyBar = ixy0 - r.area * r.centroid.x * r.centroid.y;
            } else {
                const double Aring = std::fabs(polygonArea(ring));
                const InertiaOut o = inertiaOfRing(ring, ar.centroid, Aring);
                r.ix = o.ix; r.iy = o.iy; r.ixy = o.ixy;
                r.ixBar = o.ixBar; r.iyBar = o.iyBar; r.ixyBar = o.ixyBar;
            }
            return r;
        }
    }
}

// ============================================================
// ID：点坐标查询
// ============================================================

IdResult idPoint(const Vec3& p, const std::vector<Element>& elements) {
    IdResult r;
    r.point = p;
    double best = std::numeric_limits<double>::max();
    for (const auto& e : elements) {
        const double d = geo::distanceToElement(e, p);
        if (d < best) { best = d; r.nearest = &e; }
    }
    if (r.nearest) r.nearestDist = best;
    return r;
}

} // namespace SketchCAD::inquiry
