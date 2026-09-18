#include "core/SplineModes.h"
#include "core/Geometry.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace spline {

// ============================================================
// 字符串 ↔ 模式映射
// ============================================================

Mode modeFromString(const QString& s) {
    if (s == "spline_parabola")      return Mode::Parabola;
    if (s == "spline_hyperbola")     return Mode::Hyperbola;
    if (s == "spline_parabola_4p")   return Mode::Parabola4P;
    if (s == "spline_parabola_fd")   return Mode::ParabolaFocusDirectrix;
    if (s == "spline_through_points")return Mode::ThroughPoints;
    return Mode::Free;
}

QString stringFromMode(Mode m) {
    switch (m) {
        case Mode::Free:                  return QStringLiteral("样条曲线");
        case Mode::Parabola:              return QStringLiteral("抛物线");
        case Mode::Hyperbola:             return QStringLiteral("双曲线");
        case Mode::Parabola4P:            return QStringLiteral("抛物线(4点)");
        case Mode::ParabolaFocusDirectrix:return QStringLiteral("抛物线(焦点准线)");
        case Mode::ThroughPoints:         return QStringLiteral("样条过点");
    }
    return QStringLiteral("样条曲线");
}

ReqCount requirements(Mode m) {
    switch (m) {
        case Mode::Free:                   return {0, true};   // 自由顶点，右键结束
        case Mode::Parabola:               return {3, false};  // 3 个控制点
        case Mode::Hyperbola:              return {3, false};  // 2 焦点 + 1 点
        case Mode::Parabola4P:             return {4, false};  // 4 个点
        case Mode::ParabolaFocusDirectrix: return {3, false};  // 焦点 + 准线 2 点
        case Mode::ThroughPoints:          return {0, true};   // 逐点插值，右键结束
    }
    return {0, true};
}

// ============================================================
// 抛物线（二次贝塞尔）
// ============================================================

Result parabola(const Vec3& p0, const Vec3& p1, const Vec3& p2) {
    Result r;
    r.pts = {p0, p1, p2};
    r.subType = static_cast<int>(SubType::Parabola);
    r.valid = true;
    return r;
}

std::vector<Vec3> sampleParabola(const Vec3& p0, const Vec3& p1, const Vec3& p2,
                                  int samples) {
    std::vector<Vec3> out;
    out.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double t = static_cast<double>(i) / samples;
        const double u = 1.0 - t;
        out.push_back(p0 * (u * u) + p1 * (2.0 * u * t) + p2 * (t * t));
    }
    return out;
}

// ============================================================
// 双曲线
// ============================================================

Result hyperbola(const Vec3& f1, const Vec3& f2, const Vec3& p) {
    Result r;
    const double dF = f1.distTo(f2);       // 焦距
    const double d1 = p.distTo(f1);
    const double d2 = p.distTo(f2);
    const double twoA = std::fabs(d1 - d2); // 定差 2a

    if (dF < 1e-6) {
        r.msg = QStringLiteral("两焦点重合，无法定义双曲线");
        return r;
    }
    if (twoA < 1e-6) {
        r.msg = QStringLiteral("点到两焦点距离差为零，无法定义双曲线");
        return r;
    }
    const double c = dF / 2.0;
    const double a = twoA / 2.0;
    if (c <= a + 1e-9) {
        r.msg = QStringLiteral("焦点距离不足，无法构成双曲线（需 c > a）");
        return r;
    }

    r.pts = {f1, f2, p};
    r.subType = static_cast<int>(SubType::Hyperbola);
    r.valid = true;
    return r;
}

std::vector<Vec3> sampleHyperbola(const Vec3& f1, const Vec3& f2, const Vec3& p,
                                   int samples) {
    const double dF = f1.distTo(f2);
    if (dF < 1e-9) return {f1, f2};

    const double d1 = p.distTo(f1);
    const double d2 = p.distTo(f2);
    const double twoA = std::fabs(d1 - d2);
    const double a = twoA / 2.0;
    const double c = dF / 2.0;
    const double b = std::sqrt(std::max(0.0, c * c - a * a));
    if (a < 1e-9 || c <= a) return {f1, f2};

    const Vec3 center = (f1 + f2) * 0.5;
    const Vec3 xAxis = (f2 - f1).normalized();
    const Vec3 yAxis(-xAxis.y, xAxis.x, 0);

    const Vec3 dp = p - center;
    const double px = dp.dot2D(xAxis);
    const int sign = (px >= 0) ? 1 : -1;

    const double tP = std::acosh(std::max(1.0, std::fabs(px) / a));
    const double tMax = std::max(2.0, tP * 1.5);

    std::vector<Vec3> out;
    out.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double t = -tMax + 2.0 * tMax * i / samples;
        const double x = sign * a * std::cosh(t);
        const double y = b * std::sinh(t);
        out.push_back(center + xAxis * x + yAxis * y);
    }
    return out;
}

// ============================================================
// 4 点抛物线：弦长参数化最小二乘拟合二次贝塞尔
// ============================================================

Result parabola4P(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3) {
    Result r;
    if (p0.distTo(p3) < 1e-9) {
        r.msg = QStringLiteral("抛物线两端点重合");
        return r;
    }
    r.pts = {p0, p1, p2, p3};
    r.subType = static_cast<int>(SubType::Parabola4P);
    r.valid = true;
    return r;
}

std::vector<Vec3> sampleParabola4P(const std::vector<Vec3>& pts, int samples) {
    if (pts.size() < 4) return pts;
    const Vec3& p0 = pts[0];
    const Vec3& p3 = pts[3];

    // 弦长参数化
    const double l1 = p0.distTo(pts[1]);
    const double l2 = pts[1].distTo(pts[2]);
    const double l3 = pts[2].distTo(p3);
    const double L = l1 + l2 + l3;
    if (L < 1e-9) return {p0, p3};

    const double t[4] = {0.0, l1 / L, (l1 + l2) / L, 1.0};

    // 最小二乘求 P1：
    // B(ti) = (1-ti)^2 P0 + 2(1-ti)ti P1 + ti^2 P3
    // di = pi - (1-ti)^2 P0 - ti^2 P3
    // 目标 min ||2(1-ti)ti P1 - di||^2
    double sx = 0, sy = 0, sw = 0;
    for (int i = 1; i <= 2; ++i) {
        const double w = 2.0 * (1.0 - t[i]) * t[i];
        const double dx = pts[i].x - (1.0 - t[i]) * (1.0 - t[i]) * p0.x - t[i] * t[i] * p3.x;
        const double dy = pts[i].y - (1.0 - t[i]) * (1.0 - t[i]) * p0.y - t[i] * t[i] * p3.y;
        sx += w * dx; sy += w * dy; sw += w * w;
    }
    if (sw < 1e-12) {
        // 退化为直线：中点控制
        return sampleParabola(p0, (p0 + p3) * 0.5, p3, samples);
    }
    Vec3 p1(sx / sw, sy / sw, 0);
    return sampleParabola(p0, p1, p3, samples);
}

// ============================================================
// 焦点准线抛物线
// ============================================================

Result parabolaFocusDirectrix(const Vec3& focus, const Vec3& d1, const Vec3& d2) {
    Result r;
    if (d1.distTo(d2) < 1e-9) {
        r.msg = QStringLiteral("准线两点重合，无法定义准线");
        return r;
    }
    r.pts = {focus, d1, d2};
    r.subType = static_cast<int>(SubType::ParabolaFocusDirectrix);
    r.valid = true;
    return r;
}

std::vector<Vec3> sampleParabolaFD(const Vec3& focus, const Vec3& d1, const Vec3& d2,
                                    int samples) {
    const Vec3 dir = (d2 - d1).normalized();
    if (dir.len2D() < 1e-12) return {focus};

    const Vec3 n(dir.y, -dir.x, 0); // 准线法向量（指向 focus 侧）
    const double signedP = (focus - d1).dot2D(n); // 焦点到准线有符号距离
    const double p = std::fabs(signedP);
    if (p < 1e-9) return {focus};

    // 建立局部坐标：原点在焦点到准线垂足，X 轴指向焦点，Y 轴沿准线
    const Vec3 origin = focus - n * signedP;
    const Vec3 xAxis = n * (signedP >= 0 ? 1.0 : -1.0);
    const Vec3 yAxis = dir;

    // 标准抛物线：顶点在 (p/2, 0)，方程 v^2 = 2p(u - p/2)
    const double u0 = p * 0.5;
    // 采样范围：按焦点到准线距离决定开口跨度
    const double vMax = std::max(p * 2.0, 100.0);

    std::vector<Vec3> out;
    out.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double v = -vMax + 2.0 * vMax * i / samples;
        const double u = u0 + v * v / (2.0 * p);
        out.push_back(origin + xAxis * u + yAxis * v);
    }
    return out;
}

// ============================================================
// 过点样条
// ============================================================

Result throughPoints(const std::vector<Vec3>& pts) {
    Result r;
    if (pts.size() < 2) {
        r.msg = QStringLiteral("至少需要 2 个点");
        return r;
    }
    r.pts = pts;
    r.subType = static_cast<int>(SubType::ThroughPoints);
    r.valid = true;
    return r;
}

std::vector<Vec3> sampleThroughPoints(const std::vector<Vec3>& pts, int samples) {
    return geo::sampleSpline(pts, samples);
}

// ============================================================
// 样条编辑（折线取点 / 炸开为线段）
// ============================================================

std::vector<Vec3> polylineControlPoints(const Element& src) {
    std::vector<Vec3> pts;
    if (src.type == ElementType::Line && src.pts.size() >= 2) {
        pts = {src.pts[0], src.pts[1]};
    } else if (src.type == ElementType::Polygon) {
        pts = src.pts;
    } else if (src.type == ElementType::Rectangle && src.pts.size() >= 2) {
        const double x0 = src.pts[0].x, y0 = src.pts[0].y;
        const double x1 = src.pts[1].x, y1 = src.pts[1].y;
        pts = {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
    }
    return pts;
}

std::vector<std::pair<Vec3, Vec3>> explodeToLines(const std::vector<Vec3>& pts,
                                                   int subType, int samples) {
    std::vector<std::pair<Vec3, Vec3>> segs;
    if ((int)pts.size() < 2) return segs;
    if (samples < 1) samples = 1;

    // 先拿到离散折线：插值类直接用节点，圆锥曲线类按采样
    std::vector<Vec3> poly;
    switch (subType) {
        case (int)SubType::Parabola:
            poly = sampleParabola(pts[0], pts[1], pts[2], samples);
            break;
        case (int)SubType::Hyperbola:
            poly = sampleHyperbola(pts[0], pts[1], pts[2], samples);
            break;
        case (int)SubType::Parabola4P:
            poly = sampleParabola4P(pts, samples);
            break;
        case (int)SubType::ParabolaFocusDirectrix:
            poly = sampleParabolaFD(pts[0], pts[1], pts[2], samples);
            break;
        default:
            // CatmullRom / ThroughPoints / Freehand：控制点即曲线上的节点
            poly = pts;
            break;
    }

    for (int i = 0; i + 1 < (int)poly.size(); ++i)
        segs.push_back({poly[i], poly[i + 1]});
    return segs;
}

// ============================================================
// 主入口
// ============================================================

Result compute(Mode m, const std::vector<Vec3>& pts, const Vec3& cur) {
    switch (m) {
        case Mode::Parabola: {
            if (pts.size() >= 2)
                return parabola(pts[0], pts[1], cur);
            if (pts.size() == 1) {
                Result r;
                r.msg = QStringLiteral("请输入第二个点");
                return r;
            }
            Result r;
            r.msg = QStringLiteral("请输入起点");
            return r;
        }
        case Mode::Hyperbola: {
            if (pts.size() >= 2)
                return hyperbola(pts[0], pts[1], cur);
            if (pts.size() == 1) {
                Result r;
                r.msg = QStringLiteral("请输入第二个焦点");
                return r;
            }
            Result r;
            r.msg = QStringLiteral("请输入第一个焦点");
            return r;
        }
        case Mode::Parabola4P: {
            if (pts.size() >= 3) {
                // 已收集 3 个点，cur 作为第 4 个预览点
                return parabola4P(pts[0], pts[1], pts[2], cur);
            }
            if (pts.size() == 2) {
                Result r;
                r.msg = QStringLiteral("请输入第四个点");
                return r;
            }
            if (pts.size() == 1) {
                Result r;
                r.msg = QStringLiteral("请输入第二个点");
                return r;
            }
            Result r;
            r.msg = QStringLiteral("请输入第一个点");
            return r;
        }
        case Mode::ParabolaFocusDirectrix: {
            if (pts.size() >= 2)
                return parabolaFocusDirectrix(pts[0], pts[1], cur);
            if (pts.size() == 1) {
                Result r;
                r.msg = QStringLiteral("请输入准线上第一个点");
                return r;
            }
            Result r;
            r.msg = QStringLiteral("请输入焦点");
            return r;
        }
        case Mode::ThroughPoints: {
            Result r;
            r.valid = true;
            r.subType = static_cast<int>(SubType::ThroughPoints);
            r.pts = pts;
            if (!pts.empty()) r.pts.push_back(cur);
            return r;
        }
        case Mode::Free:
        default:
            // Free 模式由 PointCollectTool 处理，不走此入口
            Result r;
            r.valid = true;
            r.subType = static_cast<int>(SubType::CatmullRom);
            r.pts = pts;
            if (!pts.empty()) r.pts.push_back(cur);
            return r;
    }
}

} // namespace spline
} // namespace SketchCAD
