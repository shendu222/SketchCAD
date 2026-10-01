// ============================================================
// ShapeSelect 实现 —— 栏选 / 圈围 / 圈交 纯几何判定
// ============================================================
#include "ShapeSelect.h"

#include <cmath>
#include <iterator>
#include "Element.h"

namespace SketchCAD {
namespace shape {

QString modeName(Mode m) {
    switch (m) {
    case Mode::Fence:    return QStringLiteral("栏选");
    case Mode::Window:   return QStringLiteral("圈围");
    case Mode::Crossing: return QStringLiteral("圈交");
    }
    return QStringLiteral("选择");
}

// ------------------------------------------------------------
// 采样点集：WP「全部点在多边形内」与点图元近邻判定用。
// 白名单给出解析类型，其余类型统一回落 pts 全体（新增图元类型
// 无需同步此处 —— 退化为顶点判定，语义安全）。
// ------------------------------------------------------------
std::vector<Vec3> samplePoints(const Element& e) {
    std::vector<Vec3> out;
    switch (e.type) {
    case ElementType::Circle: {
        out.reserve(24);
        for (int i = 0; i < 24; ++i) {
            const double a = i * (2.0 * M_PI) / 24.0;
            out.push_back(Vec3(e.pts[0].x + e.radius * std::cos(a),
                               e.pts[0].y + e.radius * std::sin(a), 0));
        }
        return out;
    }
    case ElementType::Arc: {
        // 圆心 pts[0] + 起止角；沿弧均布 24 点（含两端）
        out.reserve(25);
        const double a0 = e.startAngle * M_PI / 180.0;
        const double a1 = e.endAngle   * M_PI / 180.0;
        for (int i = 0; i <= 24; ++i) {
            const double a = a0 + (a1 - a0) * (double(i) / 24.0);
            out.push_back(Vec3(e.pts[0].x + e.radius * std::cos(a),
                               e.pts[0].y + e.radius * std::sin(a), 0));
        }
        return out;
    }
    case ElementType::Ellipse: {
        if (e.radius > 1e-12 && e.radiusY > 1e-12) {
            const std::vector<Vec3> pts = geo::sampleEllipse(
                e.pts[0], e.radius, e.radiusY, e.rotation, 24);
            out.assign(pts.begin(), pts.end());
            return out;
        }
        break;
    }
    case ElementType::Spline: {
        if (e.pts.size() >= 2) {
            const std::vector<Vec3> pts = geo::sampleSpline(e.pts, 24);
            out.assign(pts.begin(), pts.end());
            return out;
        }
        break;
    }
    default:
        break;
    }
    out = e.pts;    // 通用回落：顶点集合
    return out;
}

// ------------------------------------------------------------
// 线段化：优先 geo::flatten（与求交/剪切同源）；圆/弧 24 段逼近。
// ------------------------------------------------------------
std::vector<geo::Seg> segments(const Element& e) {
    std::vector<geo::Seg> out = geo::flatten(e);
    if (!out.empty()) return out;
    if (e.type == ElementType::Circle && e.radius > 1e-12 && !e.pts.empty()) {
        out.reserve(24);
        for (int i = 0; i < 24; ++i) {
            const double a0 = i * (2.0 * M_PI) / 24.0;
            const double a1 = (i + 1) * (2.0 * M_PI) / 24.0;
            out.push_back({Vec3(e.pts[0].x + e.radius * std::cos(a0),
                                e.pts[0].y + e.radius * std::sin(a0), 0),
                           Vec3(e.pts[0].x + e.radius * std::cos(a1),
                                e.pts[0].y + e.radius * std::sin(a1), 0)});
        }
    } else if (e.type == ElementType::Arc && e.radius > 1e-12 && !e.pts.empty()) {
        out.reserve(24);
        const double a0 = e.startAngle * M_PI / 180.0;
        const double a1 = e.endAngle   * M_PI / 180.0;
        for (int i = 0; i < 24; ++i) {
            const double t0 = a0 + (a1 - a0) * (double(i) / 24.0);
            const double t1 = a0 + (a1 - a0) * (double(i + 1) / 24.0);
            out.push_back({Vec3(e.pts[0].x + e.radius * std::cos(t0),
                                e.pts[0].y + e.radius * std::sin(t0), 0),
                           Vec3(e.pts[0].x + e.radius * std::cos(t1),
                                e.pts[0].y + e.radius * std::sin(t1), 0)});
        }
    }
    return out;
}

// ------------------------------------------------------------
// 线段 × 多边形任一边
// ------------------------------------------------------------
bool segCrossesPolygon(const Vec3& a, const Vec3& b, const std::vector<Vec3>& poly) {
    const size_t n = poly.size();
    if (n < 2) return false;
    for (size_t i = 0; i < n; ++i) {
        const Vec3& q1 = poly[i];
        const Vec3& q2 = poly[(i + 1) % n];   // 闭合多边形；折线 fence 由调用方展开
        Vec3 hit;
        if (geo::segSegIntersect(a, b, q1, q2, hit)) return true;
    }
    return false;
}

// 点 p 到线段 ab 的最短距离（近邻/贴合判定用）
static inline double pointSegDist(const Vec3& p, const Vec3& a, const Vec3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double l2 = dx * dx + dy * dy;
    if (l2 < 1e-18) return p.distTo(a);
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

// 点在多边形内（含边界）：geo::pointInPolygon 或贴合任一边
bool pointInPolygonTol(const Vec3& p, const std::vector<Vec3>& poly, double tol) {
    const size_t n = poly.size();
    if (n < 3) return false;
    if (geo::pointInPolygon(p, poly)) return true;
    for (size_t i = 0; i < n; ++i)
        if (pointSegDist(p, poly[i], poly[(i + 1) % n]) <= tol) return true;
    return false;
}

// ------------------------------------------------------------
// 单图元命中判定
// ------------------------------------------------------------
bool hit(const Element& e, const std::vector<Vec3>& shapePts, Mode m, double tol) {
    const size_t n = shapePts.size();

    if (m == Mode::Fence) {
        // 栏选：图元任一段与栏线任一边相交；线段化失败（点图元）或
        // 无真相交时，用采样点近邻兜底（fence 端点压线、点图元点选）。
        if (n < 2) return false;
        const auto segs = segments(e);
        for (const auto& s : segs)
            for (size_t i = 0; i + 1 < n; ++i) {
                Vec3 hitPt;
                if (geo::segSegIntersect(s.a, s.b, shapePts[i], shapePts[i + 1], hitPt))
                    return true;
            }
        for (const Vec3& p : samplePoints(e))
            for (size_t i = 0; i + 1 < n; ++i)
                if (pointSegDist(p, shapePts[i], shapePts[i + 1]) <= tol) return true;
        return false;
    }

    // 圈围 / 圈交：形状必须是多边形（≥3 顶点）
    if (n < 3) return false;

    if (m == Mode::Window) {
        // 全包含：全部采样点（含边界）都在多边形内
        const auto pts = samplePoints(e);
        if (pts.empty()) return false;
        for (const Vec3& p : pts)
            if (!pointInPolygonTol(p, shapePts, 1e-6)) return false;
        return true;
    }

    // Crossing：任一段穿过边界，或任一采样点落在多边形内
    for (const auto& s : segments(e))
        if (segCrossesPolygon(s.a, s.b, shapePts)) return true;
    for (const Vec3& p : samplePoints(e))
        if (pointInPolygonTol(p, shapePts, 1e-6)) return true;
    return false;
}

// ------------------------------------------------------------
// 批量判定
// ------------------------------------------------------------
std::vector<int> pick(const std::vector<Element>& elems,
                      const std::vector<Vec3>& shapePts, Mode m, double tol) {
    std::vector<int> ids;
    for (const auto& e : elems) {
        if (!e.visible) continue;
        if (hit(e, shapePts, m, tol)) ids.push_back(e.id);
    }
    return ids;
}

} // namespace shape
} // namespace SketchCAD
