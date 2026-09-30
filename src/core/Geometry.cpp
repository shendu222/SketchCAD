#include "Geometry.h"
#include "SplineModes.h"
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace geo {

static const double kBig = std::numeric_limits<double>::max() / 4;

double distPointSegment(const Vec3& p, const Vec3& a, const Vec3& b) {
    const double vx = b.x - a.x, vy = b.y - a.y;
    const double len2 = vx * vx + vy * vy;
    if (len2 < 1e-12) return p.distTo(a);           // 退化为点
    double t = ((p.x - a.x) * vx + (p.y - a.y) * vy) / len2;
    t = std::max(0.0, std::min(1.0, t));            // 夹到线段内，而不是无限长直线
    const double qx = a.x + t * vx, qy = a.y + t * vy;
    return std::hypot(p.x - qx, p.y - qy);
}

double wrapDeg360(double d) {
    double r = std::fmod(d, 360.0);
    if (r < 0) r += 360.0;
    return r;
}

double wrapDeg180(double d) {
    double r = wrapDeg360(d);
    if (r > 180.0) r -= 360.0;
    return r;
}

bool angleInSweep(double a, double start, double sweep) {
    if (std::fabs(sweep) >= 359.999) return true;   // 整圆
    if (sweep >= 0) return wrapDeg360(a - start) <= sweep + 1e-9;
    return wrapDeg360(start - a) <= -sweep + 1e-9;
}

std::vector<Vec3> catmullRomToBezier(const std::vector<Vec3>& pts) {
    std::vector<Vec3> out;
    const int n = (int)pts.size();
    if (n < 2) return out;
    out.reserve(1 + 3 * (n - 1));
    out.push_back(pts[0]);
    for (int i = 0; i < n - 1; ++i) {
        // 端点处用自身替代不存在的邻居，等价于「端点切向由相邻两点决定」
        const Vec3& p0 = pts[std::max(i - 1, 0)];
        const Vec3& p1 = pts[i];
        const Vec3& p2 = pts[i + 1];
        const Vec3& p3 = pts[std::min(i + 2, n - 1)];
        // 张力 0.5 的均匀 Catmull-Rom，其等价贝塞尔控制点为 ±(相邻点差)/6
        Vec3 c1(p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0,
                p1.z + (p2.z - p0.z) / 6.0);
        Vec3 c2(p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0,
                p2.z - (p3.z - p1.z) / 6.0);
        out.push_back(c1);
        out.push_back(c2);
        out.push_back(p2);
    }
    return out;
}

std::vector<Vec3> sampleSpline(const std::vector<Vec3>& pts, int perSeg) {
    std::vector<Vec3> out;
    const std::vector<Vec3> bz = catmullRomToBezier(pts);
    if (bz.size() < 4) return pts;
    if (perSeg < 1) perSeg = 1;
    const int segs = (int)(bz.size() - 1) / 3;
    out.reserve(segs * perSeg + 1);
    out.push_back(bz[0]);
    for (int s = 0; s < segs; ++s) {
        const Vec3& p0 = bz[3 * s];
        const Vec3& c1 = bz[3 * s + 1];
        const Vec3& c2 = bz[3 * s + 2];
        const Vec3& p1 = bz[3 * s + 3];
        for (int k = 1; k <= perSeg; ++k) {
            const double t = (double)k / perSeg, u = 1.0 - t;
            const double w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
            out.push_back(Vec3(w0 * p0.x + w1 * c1.x + w2 * c2.x + w3 * p1.x,
                               w0 * p0.y + w1 * c1.y + w2 * c2.y + w3 * p1.y,
                               w0 * p0.z + w1 * c1.z + w2 * c2.z + w3 * p1.z));
        }
    }
    return out;
}

std::vector<Vec3> sampleEllipse(const Vec3& center, double a, double b,
                                double rotationDeg, int n) {
    std::vector<Vec3> out;
    if (a < 1e-9 || b < 1e-9) return out;
    if (n < 3) n = 3;
    const double rad = math::deg2rad(rotationDeg);
    const double cosr = std::cos(rad), sinr = std::sin(rad);
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double t = 2.0 * M_PI * i / n;
        const double lx = a * std::cos(t), ly = b * std::sin(t);
        out.push_back(Vec3(center.x + lx * cosr - ly * sinr,
                           center.y + lx * sinr + ly * cosr, 0));
    }
    return out;
}

// ============================ 折线（Polyline）========================
// 弧段几何：arcThrough 取段 i (pts[i]→pts[i+1])，用三相邻节点 (pts[i-1],pts[i],pts[i+1])
// 的外接圆作为该弧的圆（边界段用端点为圆心）。返回 startDeg 为 atan2(pts[i]-center)，
// sweepDeg 恒使弧终点 = pts[i+1]（端点保持不变式，渲染/命中/导出/测量三源同源）。
// 方向：两条候选弧（CCW/CW）中选**不经过前驱节点 c** 的那条——弧凸向远离 c 的一侧，
// 与「c,a,b 为曲线上相继节点」的连续语义一致（曲线在 a→b 段继续弯离 c）。
ArcSeg arcThrough(const std::vector<Vec3>& pts, int segIdx, bool closed) {
    ArcSeg r{ Vec3(0,0,0), 0, 0, 0, false };
    const int n = (int)pts.size();
    if (n < 2) return r;
    const int nSegs = closed ? n : n - 1;
    if (segIdx < 0 || segIdx >= nSegs) return r;
    const Vec3& a = pts[segIdx];
    const Vec3& b = pts[(segIdx + 1) % n];

    // 中间段：用三节点外接圆（开放折线任何段都至少有一对邻接，闭合折线循环）
    //   segIdx=0:    prev=pts[n-1]（开）/pts[segIdx-1]（闭，但 nSegs=n 时不存在 prev）
    //   segIdx=last: next=pts[0]（开时不进入）/pts[(segIdx+1)%n]（闭时）
    // 简化为：n>=3 时尝试用邻接三点外接圆（包括端点段，邻接节点用前驱=上一节点或末节点），
    // 仅在 n==2 时退化为以端点为圆心。
    if (n >= 3) {
        const int prevIdx = (segIdx - 1 + n) % n;
        const int nextIdx = (segIdx + 1) % n;
        const Vec3& c = pts[prevIdx];
        (void)nextIdx;
        Vec3 cen; double rad;
        if (circleFrom3Points(c, a, b, cen, rad) && rad > 1e-6) {
            r.center = cen; r.radius = rad;
            r.startDeg = std::atan2(a.y - cen.y, a.x - cen.x) * 180.0 / M_PI;
            const double endDeg = std::atan2(b.y - cen.y, b.x - cen.x) * 180.0 / M_PI;
            const double cDeg  = std::atan2(c.y - cen.y, c.x - cen.x) * 180.0 / M_PI;
            // CCW 张角与 c 的相对角位置都归一化到 [0, 360)
            double dCcw = std::fmod(endDeg - r.startDeg, 360.0);
            if (dCcw < 0) dCcw += 360.0;
            double dC = std::fmod(cDeg - r.startDeg, 360.0);
            if (dC < 0) dC += 360.0;
            // c 落在 CCW 弧的角区间内（不含端点）→ 走 CW 那条（dCcw-360，凸向远离 c）；
            // 否则 c 在 CW 弧区间内（或与 a/b 重合）→ 走 CCW 那条。终点恒为 b。
            constexpr double kAngEps = 1e-9;
            r.sweepDeg = (dC > kAngEps && dC < dCcw - kAngEps) ? (dCcw - 360.0) : dCcw;
            r.valid = true;
            return r;
        }
        // 退化：与 chord 端点之一共线
    }
    // 退化（n==2 或三点共线）：以段端点为圆心
    r.center = a;
    r.radius = a.distTo(b);
    r.startDeg = std::atan2(b.y - a.y, b.x - a.x) * 180.0 / M_PI - 90.0; // 垂直于弦
    r.sweepDeg = 180.0;                                                 // 半圆
    r.valid = r.radius > 1e-6;
    return r;
}

std::vector<Vec3> samplePolyline(const std::vector<Vec3>& pts,
                                 const std::vector<bool>& arcFlags,
                                 bool closed, int perSeg) {
    std::vector<Vec3> out;
    const int n = (int)pts.size();
    if (n < 2) return out;
    if (perSeg < 1) perSeg = 1;
    const int nSegs = closed ? n : n - 1;
    out.reserve(nSegs * perSeg + 1);
    for (int i = 0; i < nSegs; ++i) {
        const Vec3& a = pts[i];
        const bool isArc = (i < (int)arcFlags.size()) && arcFlags[i];
        if (isArc) {
            ArcSeg ar = arcThrough(pts, i, closed);
            if (ar.valid) {
                const double s0 = ar.startDeg;
                const double s1 = s0 + ar.sweepDeg;
                for (int k = 0; k < perSeg; ++k) {
                    const double t = (double)k / perSeg;
                    const double ang = s0 + (s1 - s0) * t;
                    const double rad = math::deg2rad(ang);
                    out.push_back(Vec3(ar.center.x + ar.radius * std::cos(rad),
                                       ar.center.y + ar.radius * std::sin(rad), 0));
                }
            } else {
                out.push_back(a);
            }
        } else {
            out.push_back(a);
        }
    }
    // 闭合时不重复尾点；开放时补末端点
    if (closed) {
        if (!out.empty()) out.push_back(out.front());
    } else {
        out.push_back(pts.back());
    }
    return out;
}

// 折线「拟合曲线」（PEDIT Fit）：Catmull-Rom 插值，平滑通过所有顶点。
std::vector<Vec3> samplePolylineFit(const std::vector<Vec3>& pts, bool closed, int perSeg) {
    if (perSeg < 1) perSeg = 1;
    const int n = (int)pts.size();
    if (n < 2) return pts;
    if (!closed) return sampleSpline(pts, perSeg);   // 开放：直接走 Catmull-Rom
    // 闭合：循环 Catmull-Rom（每段用环绕相邻点定切向）
    std::vector<Vec3> out;
    out.reserve(n * perSeg + 1);
    for (int i = 0; i < n; ++i) {
        const Vec3& p0 = pts[(i - 1 + n) % n];
        const Vec3& p1 = pts[i];
        const Vec3& p2 = pts[(i + 1) % n];
        const Vec3& p3 = pts[(i + 2) % n];
        Vec3 c1(p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0, 0);
        Vec3 c2(p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0, 0);
        if (i == 0) out.push_back(p1);
        for (int k = 1; k <= perSeg; ++k) {
            const double t = (double)k / perSeg, u = 1.0 - t;
            const double w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
            out.push_back(Vec3(w0 * p1.x + w1 * c1.x + w2 * c2.x + w3 * p2.x,
                               w0 * p1.y + w1 * c1.y + w2 * c2.y + w3 * p2.y, 0));
        }
    }
    if (!out.empty()) out.push_back(out.front());
    return out;
}

// 折线条样曲线（PEDIT Spline）：顶点作为三次 B 样条控制点的逼近曲线。
std::vector<Vec3> samplePolylineSpline(const std::vector<Vec3>& P, bool closed, int perSeg) {
    if (perSeg < 1) perSeg = 1;
    const int n = (int)P.size();
    if (n < 3) return P;   // 退化：不足以构成样条，退回原始点
    // 均匀三次 B 样条基函数（t∈[0,1]）：
    //   b0=(1-t)^3/6, b1=(3t^3-6t^2+4)/6, b2=(-3t^3+3t^2+3t+1)/6, b3=t^3/6
    auto eval = [](double t, const Vec3& c0, const Vec3& c1, const Vec3& c2, const Vec3& c3) {
        const double u = 1.0 - t;
        const double b0 = u * u * u / 6.0;
        const double b1 = (3 * t * t * t - 6 * t * t + 4) / 6.0;
        const double b2 = (-3 * t * t * t + 3 * t * t + 3 * t + 1) / 6.0;
        const double b3 = t * t * t / 6.0;
        return Vec3(b0 * c0.x + b1 * c1.x + b2 * c2.x + b3 * c3.x,
                    b0 * c0.y + b1 * c1.y + b2 * c2.y + b3 * c3.y, 0);
    };
    std::vector<Vec3> out;
    if (closed) {
        const int segCount = n;
        out.reserve(segCount * perSeg + 1);
        for (int s = 0; s < segCount; ++s) {
            const Vec3 c0 = P[(s - 1 + n) % n];
            const Vec3 c1 = P[s % n];
            const Vec3 c2 = P[(s + 1) % n];
            const Vec3 c3 = P[(s + 2) % n];
            if (s == 0) out.push_back(c1);
            for (int k = 1; k <= perSeg; ++k)
                out.push_back(eval((double)k / perSeg, c0, c1, c2, c3));
        }
        if (!out.empty()) out.push_back(out.front());
        return out;
    }
    // 开放：端点外插「幻影控制点」C_{-1}=2P0-P1、C_n=2P_{n-1}-P_{n-2}，
    // 使曲线在首尾处严格通过 P0 / P_{n-1}（B(t=0)=(C_{-1}+4P0+P1)/6=P0）。
    const Vec3 cPrev = P[0] * 2.0 - P[1];
    const Vec3 cNext = P[n - 1] * 2.0 - P[n - 2];
    const int segCount = n - 1;
    out.reserve(segCount * perSeg + 1);
    for (int s = 0; s < segCount; ++s) {
        const Vec3 c0 = (s == 0) ? cPrev : P[s - 1];
        const Vec3 c1 = P[s];
        const Vec3 c2 = P[s + 1];
        const Vec3 c3 = (s == segCount - 1) ? cNext : P[s + 2];
        if (s == 0) out.push_back(c1);
        for (int k = 1; k <= perSeg; ++k)
            out.push_back(eval((double)k / perSeg, c0, c1, c2, c3));
    }
    return out;
}

bool circleFrom3Points(const Vec3& A, const Vec3& B, const Vec3& C,
                       Vec3& center, double& radius) {
    const double ax = A.x, ay = A.y, bx = B.x, by = B.y, cx = C.x, cy = C.y;
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::fabs(d) < 1e-9) return false;          // 三点共线，外接圆不存在
    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    const double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
    const double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
    center = Vec3(ux, uy, 0);
    radius = center.distTo(A);
    return true;
}

// 折线（可选闭合）到点的最短距离
static double distPolyline(const std::vector<Vec3>& v, const Vec3& p, bool closed) {
    if (v.empty()) return kBig;
    if (v.size() == 1) return p.distTo(v[0]);
    double best = kBig;
    for (size_t i = 0; i + 1 < v.size(); ++i)
        best = std::min(best, distPointSegment(p, v[i], v[i + 1]));
    if (closed && v.size() >= 3)
        best = std::min(best, distPointSegment(p, v.back(), v.front()));
    return best;
}

double distanceToElement(const Element& e, const Vec3& p) {
    switch (e.type) {
        case ElementType::Line:
            return e.pts.size() >= 2 ? distPointSegment(p, e.pts[0], e.pts[1]) : kBig;

        case ElementType::Circle: {
            if (e.pts.empty()) return kBig;
            // 只命中圆周本身，不命中圆内空白区域 —— 这正是原先包围盒命中最反直觉的地方
            return std::fabs(p.distTo(e.pts[0]) - e.radius);
        }

        case ElementType::Arc: {
            if (e.pts.empty()) return kBig;
            const Vec3& c = e.pts[0];
            const double ang = std::atan2(p.y - c.y, p.x - c.x) * 180.0 / M_PI;
            const double sweep = e.endAngle - e.startAngle;
            if (angleInSweep(ang, e.startAngle, sweep))
                return std::fabs(p.distTo(c) - e.radius);
            // 角度不在弧段内时，退化为到两个端点的距离
            const double a0 = e.startAngle * M_PI / 180.0;
            const double a1 = e.endAngle * M_PI / 180.0;
            const Vec3 s(c.x + e.radius * std::cos(a0), c.y + e.radius * std::sin(a0), c.z);
            const Vec3 t(c.x + e.radius * std::cos(a1), c.y + e.radius * std::sin(a1), c.z);
            return std::min(p.distTo(s), p.distTo(t));
        }

        case ElementType::Ellipse: {
            if (e.pts.empty() || e.radius < 1e-9 || e.radiusY < 1e-9) return kBig;
            // 椭圆没有解析距离闭式解，采样成闭合折线后用点到折线距离近似，
            // 足够选中/悬停使用（与样条命中检测同源）。
            return distPolyline(sampleEllipse(e.pts[0], e.radius, e.radiusY,
                                              e.rotation, 160), p, true);
        }

        case ElementType::Polyline: {
            // 折线含直线/弧段混合：按对应模式采样成密集点列再用 distPolyline
            if (e.pts.size() < 2) return kBig;
            if (e.splineSubType == 1)
                return distPolyline(samplePolylineFit(e.pts, e.closed, 12), p, e.closed);
            if (e.splineSubType == 2)
                return distPolyline(samplePolylineSpline(e.pts, e.closed, 12), p, e.closed);
            return distPolyline(samplePolyline(e.pts, e.arcFlags, e.closed, 12), p, e.closed);
        }

        case ElementType::Rectangle: {
            if (e.pts.size() < 2) return kBig;
            const double x0 = e.pts[0].x, y0 = e.pts[0].y;
            const double x1 = e.pts[1].x, y1 = e.pts[1].y;
            const std::vector<Vec3> corners = {
                Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
            return distPolyline(corners, p, e.polyClosed());
        }

        case ElementType::Polygon:
            return distPolyline(e.pts, p, e.polyClosed());

        case ElementType::Spline:
            // 命中的是屏幕上那条平滑曲线，而不是控制多边形，所以必须按采样点算
            if (e.splineSubType == 1 && e.pts.size() >= 3)
                return distPolyline(spline::sampleParabola(e.pts[0], e.pts[1], e.pts[2]), p, false);
            if (e.splineSubType == 2 && e.pts.size() >= 3)
                return distPolyline(spline::sampleHyperbola(e.pts[0], e.pts[1], e.pts[2]), p, false);
            if (e.splineSubType == 3 && e.pts.size() >= 4)
                return distPolyline(spline::sampleParabola4P(e.pts), p, false);
            if (e.splineSubType == 4 && e.pts.size() >= 3)
                return distPolyline(spline::sampleParabolaFD(e.pts[0], e.pts[1], e.pts[2]), p, false);
            return distPolyline(sampleSpline(e.pts, 12), p, false);

        case ElementType::Constraint:
            return e.pts.empty() ? kBig : p.distTo(e.pts[0]);

        case ElementType::Dimension:
            return e.pts.size() >= 2 ? distPointSegment(p, e.pts[0], e.pts[1])
                                     : (e.pts.empty() ? kBig : p.distTo(e.pts[0]));

        case ElementType::Text:
        case ElementType::MText:
            // 文本拾取主路径走 ElementItem::shape()（包围盒矩形，精确到整段文字）；
            // 此距离函数仅用于「最近实体」检索，返回到锚点的距离即可。
            return e.pts.empty() ? kBig : p.distTo(e.pts[0]);

        case ElementType::Table: {
            // 表格：矩形内距离 0（可点内部选中），矩形外取到矩形的最短距离。
            // （精确拾取同样走 shape() 的包围盒矩形；此处仅服务「最近实体」检索。）
            if (e.pts.empty()) return kBig;
            const double W = e.tableWidthMm();
            const double H = std::max(1, e.tableRows) * std::max(0.5, e.rowHeight);
            const double dx = std::max({ 0.0, e.pts[0].x - p.x, p.x - (e.pts[0].x + W) });
            const double dy = std::max({ 0.0, (e.pts[0].y - H) - p.y, p.y - e.pts[0].y });
            return std::sqrt(dx * dx + dy * dy);
        }

        case ElementType::Hatch: {
            // 剖面区域：取到外边界与各内孔轮廓的最短距离（精确拾取走 shape() 的填充区域）
            if (e.pts.size() < 3) return kBig;
            double best = distPolyline(e.pts, p, true);
            for (const auto& h : e.holes) best = std::min(best, distPolyline(h, p, true));
            return best;
        }

        case ElementType::Boundary: {
            // 交互式自定义边界：几何与 Hatch 同构（外环 + 内孔），命中取其轮廓距离。
            if (e.pts.size() < 3) return kBig;
            double best = distPolyline(e.pts, p, true);
            for (const auto& h : e.holes) best = std::min(best, distPolyline(h, p, true));
            return best;
        }

        case ElementType::BlockRef:
            // 块引用的精确命中需要块表（本函数是纯几何、无模型依赖），故由调用方
            // （GraphicsView::hitTest）展开后走 distanceToElements；这里返回到插入点的
            // 距离作为兜底，保证"未定义块"仍可被近距选中。
            return e.pts.empty() ? kBig : p.distTo(e.pts[0]);

        case ElementType::Point:
            return e.pts.empty() ? kBig : p.distTo(e.pts[0]);

        case ElementType::Xline: {
            // 构造线：点到无限直线的距离（方向向量是单位向量，叉积绝对值即点线距离）。
            // 命中整条延伸线 —— 与"无限延伸"的显示一致，交点捕捉/近距拾取都依赖它。
            if (e.pts.empty()) return kBig;
            const double rad = e.rotation * M_PI / 180.0;
            const double dx = std::cos(rad), dy = std::sin(rad);
            return std::fabs(dx * (p.y - e.pts[0].y) - dy * (p.x - e.pts[0].x));
        }
    }
    return kBig;
}

double distanceToElements(const std::vector<Element>& es, const Vec3& p) {
    double best = kBig;
    for (const auto& e : es) best = std::min(best, distanceToElement(e, p));
    return best;
}

// ============================ 直线极坐标属性 ============================

double segLength(const Vec3& a, const Vec3& b) { return std::hypot(b.x - a.x, b.y - a.y); }

double segAngleDeg(const Vec3& a, const Vec3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    if (std::fabs(dx) < 1e-12 && std::fabs(dy) < 1e-12) return 0.0;
    return wrapDeg360(std::atan2(dy, dx) * 180.0 / M_PI);
}

Vec3 endpointFrom(const Vec3& s, double length, double angleDeg) {
    const double a = angleDeg * M_PI / 180.0;
    return Vec3(s.x + length * std::cos(a), s.y + length * std::sin(a), s.z);
}

double measureDimension(const Element& e) {
    switch (e.dtype) {
        case DimensionType::Radius:   return e.radius;
        case DimensionType::Diameter: return e.radius * 2.0;
        case DimensionType::Angle: {
            if (e.pts.size() < 3) return 0.0;
            const double a1 = segAngleDeg(e.pts[0], e.pts[1]);
            const double a2 = segAngleDeg(e.pts[0], e.pts[2]);
            return std::fabs(wrapDeg180(a2 - a1));       // 恒取不大于 180° 的那个角
        }
        case DimensionType::Horizontal:
            return e.pts.size() >= 2 ? std::fabs(e.pts[1].x - e.pts[0].x) : 0.0;
        case DimensionType::Vertical:
            return e.pts.size() >= 2 ? std::fabs(e.pts[1].y - e.pts[0].y) : 0.0;
        case DimensionType::Aligned:
            return e.pts.size() >= 2 ? segLength(e.pts[0], e.pts[1]) : 0.0;
        case DimensionType::Distance: {
            if (e.pts.size() < 2) return 0.0;
            const double dx = std::fabs(e.pts[1].x - e.pts[0].x);
            const double dy = std::fabs(e.pts[1].y - e.pts[0].y);
            return dx >= dy ? dx : dy;                   // 自动线性：取投影较长的方向
        }
        case DimensionType::Baseline:
        case DimensionType::Continue:
            return e.pts.size() >= 2 ? segLength(e.pts[0], e.pts[1]) : 0.0;
        case DimensionType::Arc: {
            if (e.pts.empty() || e.radius <= 1e-9) return 0.0;
            const double sweep = std::fabs(wrapDeg180(e.endAngle - e.startAngle));
            return e.radius * sweep * M_PI / 180.0;      // 弧长 = R·θ
        }
        case DimensionType::Leader:
            return 0.0;                                 // 引线标注无数值测量
        case DimensionType::Ordinate: {
            if (e.pts.size() < 2) return 0.0;
            const bool isX = (std::fabs(std::sin(e.rotation * M_PI / 180.0)) < 0.707);
            return isX ? std::fabs(e.pts[0].x - e.pts[1].x)
                       : std::fabs(e.pts[0].y - e.pts[1].y);
        }
    }
    return 0.0;
}

// ============================ 控制点 ============================

std::vector<Vec3> elementHandles(const Element& e) {
    switch (e.type) {
        case ElementType::Line:
            return e.pts.size() >= 2 ? std::vector<Vec3>{e.pts[0], e.pts[1]} : e.pts;

        case ElementType::Rectangle: {
            if (e.pts.size() < 2) return e.pts;
            const double x0 = e.pts[0].x, y0 = e.pts[0].y;
            const double x1 = e.pts[1].x, y1 = e.pts[1].y;
            return {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
        }

        case ElementType::Circle: {
            if (e.pts.empty()) return {};
            const Vec3& c = e.pts[0];
            return {c, Vec3(c.x + e.radius, c.y, c.z)};
        }

        case ElementType::Arc: {
            if (e.pts.empty()) return {};
            const Vec3& c = e.pts[0];
            const double a0 = e.startAngle * M_PI / 180.0, a1 = e.endAngle * M_PI / 180.0;
            return {c,
                    Vec3(c.x + e.radius * std::cos(a0), c.y + e.radius * std::sin(a0), c.z),
                    Vec3(c.x + e.radius * std::cos(a1), c.y + e.radius * std::sin(a1), c.z)};
        }

        case ElementType::Ellipse: {
            if (e.pts.empty()) return {};
            const Vec3& c = e.pts[0];
            const double rad = math::deg2rad(e.rotation);
            const double cosr = std::cos(rad), sinr = std::sin(rad);
            const Vec3 maj(c.x + e.radius * cosr,  c.y + e.radius * sinr,  c.z);  // 长轴端点
            const Vec3 min(c.x - e.radiusY * sinr, c.y + e.radiusY * cosr, c.z);  // 短轴端点
            return {c, maj, min};
        }

        case ElementType::Polyline:
            // 控制点 = 所有节点（弧段几何由相邻 3 节点决定，拖任一节点自动影响邻接弧段）
            return e.pts;

        default:
            return e.pts;   // 多边形 / 样条 / 标注 / 约束：控制点即 pts
    }
}

bool setElementHandle(Element& e, int idx, const Vec3& p) {
    if (idx < 0) return false;
    switch (e.type) {
        case ElementType::Line:
            if (idx > 1 || (int)e.pts.size() < 2) return false;
            e.pts[idx] = p;
            return true;

        case ElementType::Rectangle: {
            if (idx > 3 || (int)e.pts.size() < 2) return false;
            // 矩形只存对角两点，拖角要按角的归属改写对应分量，
            // 否则拖右下角会把整个矩形拽走而不是改宽高。
            switch (idx) {
                case 0: e.pts[0].x = p.x; e.pts[0].y = p.y; break;   // 左下 = pts[0]
                case 1: e.pts[1].x = p.x; e.pts[0].y = p.y; break;   // 右下
                case 2: e.pts[1].x = p.x; e.pts[1].y = p.y; break;   // 右上 = pts[1]
                case 3: e.pts[0].x = p.x; e.pts[1].y = p.y; break;   // 左上
                default: return false;
            }
            return true;
        }

        case ElementType::Circle: {
            if (e.pts.empty()) return false;
            if (idx == 0)      { e.pts[0] = p; return true; }        // 平移圆
            if (idx == 1)      { e.radius = std::max(1e-6, p.distTo(e.pts[0])); return true; }
            return false;
        }

        case ElementType::Arc: {
            if (e.pts.empty()) return false;
            const Vec3 c = e.pts[0];
            if (idx == 0) { e.pts[0] = p; return true; }
            if (idx == 1 || idx == 2) {
                const double ang = wrapDeg360(std::atan2(p.y - c.y, p.x - c.x) * 180.0 / M_PI);
                const double sweep = e.endAngle - e.startAngle;
                if (idx == 1) {
                    // 拖起点：保持扫掠方向与角度不变，整体旋转弧
                    e.startAngle = ang;
                    e.endAngle = ang + sweep;
                } else {
                    // 拖终点：起点不动，只改扫掠量，并保持原有旋向
                    double ns = wrapDeg360(ang - e.startAngle);
                    if (sweep < 0) ns -= 360.0;
                    e.endAngle = e.startAngle + ns;
                }
                e.radius = std::max(1e-6, p.distTo(c));
                return true;
            }
            return false;
        }

        case ElementType::Ellipse: {
            if (e.pts.empty()) return false;
            if (idx == 0) { e.pts[0] = p; return true; }          // 平移椭圆
            const Vec3 c = e.pts[0];
            const double rad = math::deg2rad(e.rotation);
            const double cosr = std::cos(rad), sinr = std::sin(rad);
            if (idx == 1) {                                        // 拖长轴端点 → 改长轴与旋向
                const double ang = wrapDeg360(std::atan2(p.y - c.y, p.x - c.x) * 180.0 / M_PI);
                e.rotation = ang;
                e.radius = std::max(1e-6, p.distTo(c));
                return true;
            }
            if (idx == 2) {                                        // 拖短轴端点 → 改短轴
                const double v = (p.x - c.x) * (-sinr) + (p.y - c.y) * cosr;
                e.radiusY = std::max(1e-6, std::fabs(v));
                return true;
            }
            return false;
        }

        case ElementType::Polyline:
            // 折线控制点 = 节点；拖节点时同时记下原始位置，arcFlags 由邻接 3 节点动态求取
            if (idx >= (int)e.pts.size()) return false;
            e.pts[idx] = p;
            return true;

        default:
            if (idx >= (int)e.pts.size()) return false;
            e.pts[idx] = p;
            return true;
    }
}

// ============================ 离散化与求交 ============================

std::vector<Seg> flatten(const Element& e, int splineSeg) {
    std::vector<Seg> out;
    auto pushPoly = [&](const std::vector<Vec3>& v, bool closed) {
        for (size_t i = 0; i + 1 < v.size(); ++i) out.push_back({v[i], v[i + 1]});
        if (closed && v.size() >= 3) out.push_back({v.back(), v.front()});
    };
    switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() >= 2) out.push_back({e.pts[0], e.pts[1]});
            break;
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) break;
            const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
            pushPoly({Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)},
                     e.polyClosed());
            break;
        }
        case ElementType::Polygon:
            pushPoly(e.pts, e.polyClosed());
            break;
        case ElementType::Spline:
            if (e.splineSubType == 1 && e.pts.size() >= 3)
                pushPoly(spline::sampleParabola(e.pts[0], e.pts[1], e.pts[2]), false);
            else if (e.splineSubType == 2 && e.pts.size() >= 3)
                pushPoly(spline::sampleHyperbola(e.pts[0], e.pts[1], e.pts[2]), false);
            else
                pushPoly(sampleSpline(e.pts, std::max(1, splineSeg)), false);
            break;
        case ElementType::Dimension:
            if (e.pts.size() >= 2) out.push_back({e.pts[0], e.pts[1]});
            break;
        case ElementType::Ellipse:
            if (!e.pts.empty() && e.radius > 1e-9 && e.radiusY > 1e-9)
                pushPoly(sampleEllipse(e.pts[0], e.radius, e.radiusY, e.rotation, 160), true);
            break;
        case ElementType::Polyline:
            // 折线：按对应模式离散为短段（弧段 = perSeg 段折线；Fit/Spline 按曲线采样）
            if (e.pts.size() >= 2) {
                std::vector<Vec3> dense;
                if (e.splineSubType == 1)
                    dense = samplePolylineFit(e.pts, e.closed, std::max(1, splineSeg));
                else if (e.splineSubType == 2)
                    dense = samplePolylineSpline(e.pts, e.closed, std::max(1, splineSeg));
                else
                    dense = samplePolyline(e.pts, e.arcFlags, e.closed, std::max(1, splineSeg));
                pushPoly(dense, false);
            }
            break;
        default:
            break;      // 圆 / 圆弧 / 约束
    }
    return out;
}

bool segSegIntersect(const Vec3& p1, const Vec3& p2, const Vec3& q1, const Vec3& q2, Vec3& out) {
    const double rx = p2.x - p1.x, ry = p2.y - p1.y;
    const double sx = q2.x - q1.x, sy = q2.y - q1.y;
    const double den = rx * sy - ry * sx;
    if (std::fabs(den) < 1e-12) return false;             // 平行或共线
    const double t = ((q1.x - p1.x) * sy - (q1.y - p1.y) * sx) / den;
    const double u = ((q1.x - p1.x) * ry - (q1.y - p1.y) * rx) / den;
    if (t < -1e-9 || t > 1 + 1e-9 || u < -1e-9 || u > 1 + 1e-9) return false;
    out = Vec3(p1.x + t * rx, p1.y + t * ry, 0);
    return true;
}

int segCircleIntersect(const Vec3& p1, const Vec3& p2, const Vec3& c, double r, Vec3 out[2]) {
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double a = dx * dx + dy * dy;
    if (a < 1e-18 || r <= 1e-12) return 0;
    const double fx = p1.x - c.x, fy = p1.y - c.y;
    const double b = 2.0 * (fx * dx + fy * dy);
    const double cc = fx * fx + fy * fy - r * r;
    double disc = b * b - 4 * a * cc;
    if (disc < 0) return 0;
    disc = std::sqrt(disc);
    const double t1 = (-b - disc) / (2 * a), t2 = (-b + disc) / (2 * a);
    int n = 0;
    for (double t : {t1, t2}) {
        if (t < -1e-9 || t > 1 + 1e-9) continue;
        Vec3 p(p1.x + t * dx, p1.y + t * dy, 0);
        // 相切时两根几乎重合，只保留一个
        if (n == 1 && p.distTo(out[0]) < 1e-7) continue;
        out[n++] = p;
        if (n == 2) break;
    }
    return n;
}

int circleCircleIntersect(const Vec3& c1, double r1, const Vec3& c2, double r2, Vec3 out[2]) {
    const double dx = c2.x - c1.x, dy = c2.y - c1.y;
    const double d = std::hypot(dx, dy);
    if (d < 1e-12) return 0;                               // 同心
    if (d > r1 + r2 + 1e-9 || d < std::fabs(r1 - r2) - 1e-9) return 0;
    const double a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
    double h2 = r1 * r1 - a * a;
    if (h2 < 0) h2 = 0;
    const double h = std::sqrt(h2);
    const double mx = c1.x + a * dx / d, my = c1.y + a * dy / d;
    if (h < 1e-9) { out[0] = Vec3(mx, my, 0); return 1; }  // 相切
    out[0] = Vec3(mx + h * dy / d, my - h * dx / d, 0);
    out[1] = Vec3(mx - h * dy / d, my + h * dx / d, 0);
    return 2;
}

// 交点是否真的落在圆弧的角度扫掠范围内（圆则恒真）
static bool onArcSpan(const Element& e, const Vec3& p) {
    if (e.type != ElementType::Arc) return true;
    if (e.pts.empty()) return false;
    const double ang = std::atan2(p.y - e.pts[0].y, p.x - e.pts[0].x) * 180.0 / M_PI;
    return angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle);
}

static void pushUnique(std::vector<Vec3>& v, const Vec3& p) {
    for (const auto& q : v) if (q.distTo(p) < 1e-7) return;
    v.push_back(p);
}

std::vector<Vec3> intersectElements(const Element& A, const Element& B) {
    std::vector<Vec3> res;
    auto isRound = [](const Element& e) {
        return e.type == ElementType::Circle || e.type == ElementType::Arc;
    };
    auto isXline = [](const Element& e) { return e.type == ElementType::Xline; };

    // 构造线（Xline）× 任意图元：把无限直线展开为「场景级长线段」（±1e5，与场景矩形同级）
    // 后按常规路径求交 —— 交点捕捉是构造线的核心用途，必须参与。
    if (isXline(A) || isXline(B)) {
        const Element& xl = isXline(A) ? A : B;
        const Element& ot = isXline(A) ? B : A;
        if (xl.pts.empty()) return res;
        const double rad = xl.rotation * M_PI / 180.0;
        const Vec3 d(std::cos(rad), std::sin(rad), 0);
        const Vec3 a(xl.pts[0].x - d.x * 1e5, xl.pts[0].y - d.y * 1e5, 0);
        const Vec3 b(xl.pts[0].x + d.x * 1e5, xl.pts[0].y + d.y * 1e5, 0);
        if (isXline(ot)) {                          // 构造线 × 构造线：两无限直线求交
            if (ot.pts.empty()) return res;
            const double rad2 = ot.rotation * M_PI / 180.0;
            const Vec3 d2(std::cos(rad2), std::sin(rad2), 0);
            Vec3 ip;
            if (math::lineLineIntersect(xl.pts[0], xl.pts[0] + d,
                                        ot.pts[0], ot.pts[0] + d2, ip))
                pushUnique(res, ip);
            return res;
        }
        if (isRound(ot)) {                          // 构造线 × 圆(弧)
            if (ot.pts.empty()) return res;
            Vec3 buf[2];
            const int n = segCircleIntersect(a, b, ot.pts[0], ot.radius, buf);
            for (int i = 0; i < n; ++i)
                if (onArcSpan(ot, buf[i])) pushUnique(res, buf[i]);
            return res;
        }
        Vec3 ip;                                    // 构造线 × 折线族
        for (const auto& s : flatten(ot))
            if (segSegIntersect(a, b, s.a, s.b, ip)) pushUnique(res, ip);
        return res;
    }

    const bool ra = isRound(A), rb = isRound(B);

    if (!ra && !rb) {                                    // 折线 × 折线
        const auto sa = flatten(A), sb = flatten(B);
        Vec3 ip;
        for (const auto& s1 : sa)
            for (const auto& s2 : sb)
                if (segSegIntersect(s1.a, s1.b, s2.a, s2.b, ip)) pushUnique(res, ip);
        return res;
    }
    if (ra && rb) {                                      // 圆(弧) × 圆(弧)
        if (A.pts.empty() || B.pts.empty()) return res;
        Vec3 buf[2];
        const int n = circleCircleIntersect(A.pts[0], A.radius, B.pts[0], B.radius, buf);
        for (int i = 0; i < n; ++i)
            if (onArcSpan(A, buf[i]) && onArcSpan(B, buf[i])) pushUnique(res, buf[i]);
        return res;
    }
    // 折线 × 圆(弧)
    const Element& poly = ra ? B : A;
    const Element& cir  = ra ? A : B;
    if (cir.pts.empty()) return res;
    Vec3 buf[2];
    for (const auto& s : flatten(poly)) {
        const int n = segCircleIntersect(s.a, s.b, cir.pts[0], cir.radius, buf);
        for (int i = 0; i < n; ++i)
            if (onArcSpan(cir, buf[i])) pushUnique(res, buf[i]);
    }
    return res;
}

// ============================ 几何包围盒 ============================

QRectF elementBounds(const Element& e) {
    double minX = kBig, minY = kBig, maxX = -kBig, maxY = -kBig;
    auto inc = [&](double x, double y) {
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
    };
    if (e.type == ElementType::Circle || e.type == ElementType::Arc) {
        // 圆/弧：按角度采样整段（圆弧可能比其端点更外凸，纯端点包围盒会漏选）
        if (!e.pts.empty() && e.radius > 1e-9) {
            const Vec3 c = e.pts[0];
            double a0, sweep;
            if (e.type == ElementType::Circle) { a0 = 0.0; sweep = 360.0; }
            else {
                a0 = e.startAngle;
                sweep = e.endAngle - e.startAngle;
                if (sweep <= 0.0) sweep += 360.0;   // 归一化到正扫掠
            }
            const int N = 64;
            for (int i = 0; i <= N; ++i) {
                const double a = (a0 + sweep * i / N) * M_PI / 180.0;
                inc(c.x + e.radius * std::cos(a), c.y + e.radius * std::sin(a));
            }
        }
    } else {
        // 其余图元：离散后的线段端点 + 控制点
        for (const auto& s : flatten(e)) { inc(s.a.x, s.a.y); inc(s.b.x, s.b.y); }
    }
    for (const auto& h : elementHandles(e)) inc(h.x, h.y);
    if (minX > maxX) return QRectF();   // 空图元
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

// ============================ 折线参数化 ============================

double polylineLength(const std::vector<Vec3>& v, bool closed) {
    double L = 0;
    for (size_t i = 0; i + 1 < v.size(); ++i) L += v[i].distTo(v[i + 1]);
    if (closed && v.size() >= 3) L += v.back().distTo(v.front());
    return L;
}

// 弧感知精确周长：弧段 = |sweep|·r（arcThrough 外接圆），直线段 = 弦长。
double polylineArcLength(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed) {
    const int n = (int)pts.size();
    if (n < 2) return 0;
    const int nSegs = closed ? n : n - 1;
    double L = 0;
    for (int i = 0; i < nSegs; ++i) {
        const Vec3& a = pts[i];
        const Vec3& b = pts[(i + 1) % n];
        const bool isArc = (i < (int)arcFlags.size()) && arcFlags[i];
        if (isArc) {
            const ArcSeg ar = arcThrough(pts, i, closed);
            if (ar.valid && ar.radius > 1e-12)
                L += std::fabs(ar.sweepDeg) * math::deg2rad(1.0) * ar.radius;
            else
                L += a.distTo(b);
        } else {
            L += a.distTo(b);
        }
    }
    return L;
}

// 弧感知有向面积（CCW 为正）。推导见 Geometry.h 注释：
//   直线段贡献 ½(xₐy_b − x_b yₐ)；
//   弧段 a→b（圆心 c、半径 r、有向扫掠 Δ）贡献 ½[r²Δ + cx(b.y−a.y) + cy(a.x−b.x)]。
double polylineSignedArea(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed) {
    const int n = (int)pts.size();
    if (n < 3) return 0;
    const int nSegs = closed ? n : n - 1;
    double A = 0;
    for (int i = 0; i < nSegs; ++i) {
        const Vec3& a = pts[i];
        const Vec3& b = pts[(i + 1) % n];
        const bool isArc = (i < (int)arcFlags.size()) && arcFlags[i];
        if (isArc) {
            const ArcSeg ar = arcThrough(pts, i, closed);
            if (ar.valid && ar.radius > 1e-12) {
                const double dPhi = ar.sweepDeg * math::deg2rad(1.0);
                A += 0.5 * (ar.radius * ar.radius * dPhi
                            + ar.center.x * (b.y - a.y)
                            + ar.center.y * (a.x - b.x));
                continue;
            }
        }
        A += 0.5 * (a.x * b.y - b.x * a.y);
    }
    return A;
}

// 内部：把折线按「是否闭合」展开成一串顶点（闭合时首点补到末尾）
static std::vector<Vec3> expand(const std::vector<Vec3>& v, bool closed) {
    std::vector<Vec3> w = v;
    if (closed && v.size() >= 3) w.push_back(v.front());
    return w;
}

double polylineParam(const std::vector<Vec3>& v, bool closed, const Vec3& p) {
    const std::vector<Vec3> w = expand(v, closed);
    double acc = 0, bestT = 0, bestD = kBig;
    for (size_t i = 0; i + 1 < w.size(); ++i) {
        const double vx = w[i + 1].x - w[i].x, vy = w[i + 1].y - w[i].y;
        const double L2 = vx * vx + vy * vy;
        const double segL = std::sqrt(L2);
        double t = 0;
        if (L2 > 1e-18) {
            t = ((p.x - w[i].x) * vx + (p.y - w[i].y) * vy) / L2;
            t = std::max(0.0, std::min(1.0, t));
        }
        const double qx = w[i].x + t * vx, qy = w[i].y + t * vy;
        const double d = std::hypot(p.x - qx, p.y - qy);
        if (d < bestD) { bestD = d; bestT = acc + t * segL; }
        acc += segL;
    }
    return bestT;
}

Vec3 polylinePointAt(const std::vector<Vec3>& v, bool closed, double t) {
    const std::vector<Vec3> w = expand(v, closed);
    if (w.empty()) return Vec3();
    if (w.size() == 1 || t <= 0) return w.front();
    double acc = 0;
    for (size_t i = 0; i + 1 < w.size(); ++i) {
        const double segL = w[i].distTo(w[i + 1]);
        if (t <= acc + segL || i + 2 == w.size()) {
            const double u = segL < 1e-12 ? 0.0 : std::max(0.0, std::min(1.0, (t - acc) / segL));
            return Vec3(w[i].x + u * (w[i + 1].x - w[i].x),
                        w[i].y + u * (w[i + 1].y - w[i].y),
                        w[i].z + u * (w[i + 1].z - w[i].z));
        }
        acc += segL;
    }
    return w.back();
}

// 取 [t0, t1] 区间的折线（含端点插值），t0 < t1
static std::vector<Vec3> subPolyline(const std::vector<Vec3>& v, bool closed,
                                     double t0, double t1) {
    const std::vector<Vec3> w = expand(v, closed);
    std::vector<Vec3> out;
    if (w.size() < 2 || t1 - t0 < 1e-9) return out;
    out.push_back(polylinePointAt(v, closed, t0));
    double acc = 0;
    for (size_t i = 0; i + 1 < w.size(); ++i) {
        const double segL = w[i].distTo(w[i + 1]);
        const double vEnd = acc + segL;
        // 落在区间内部的原始顶点保留，保证折线形状不被端点插值抹平
        if (vEnd > t0 + 1e-9 && vEnd < t1 - 1e-9) out.push_back(w[i + 1]);
        acc = vEnd;
    }
    out.push_back(polylinePointAt(v, closed, t1));
    return out;
}

std::vector<std::vector<Vec3>> splitPolyline(const std::vector<Vec3>& v, bool closed,
                                             std::vector<double> cuts, double tClick) {
    std::vector<std::vector<Vec3>> res;
    const double total = polylineLength(v, closed);
    if (total < 1e-9) return res;

    // 排序并去重；开放折线上落在两端的切点没有切分意义，直接丢弃
    std::sort(cuts.begin(), cuts.end());
    std::vector<double> cs;
    for (double c : cuts) {
        if (c < -1e-7 || c > total + 1e-7) continue;
        if (!closed && (c < 1e-7 || c > total - 1e-7)) continue;
        if (!cs.empty() && std::fabs(c - cs.back()) < 1e-7) continue;
        cs.push_back(c);
    }

    if (closed) {
        // 环状参数空间：至少两个切点才能切下一段
        if (cs.size() < 2) return res;
        // 找到 tClick 所在的环区间 [cs[i], cs[i+1])（末段回绕到 cs[0]）
        size_t k = cs.size() - 1;
        for (size_t i = 0; i + 1 < cs.size(); ++i)
            if (tClick >= cs[i] && tClick < cs[i + 1]) { k = i; break; }
        // 剩余部分 = 从被删区间的终点，绕一圈回到被删区间的起点
        const double a = cs[(k + 1) % cs.size()];        // 删除段终点
        const double b = cs[k];                          // 删除段起点
        std::vector<Vec3> keep;
        if (k + 1 < cs.size()) {
            keep = subPolyline(v, true, a, total);
            const auto tail = subPolyline(v, true, 0.0, b);
            for (size_t i = (keep.empty() ? 0 : 1); i < tail.size(); ++i) keep.push_back(tail[i]);
        } else {
            // 被删的是「最后一个切点 → 回绕到第一个切点」那一段
            keep = subPolyline(v, true, cs.front(), cs.back());
        }
        if (keep.size() >= 2) res.push_back(keep);
        return res;
    }

    // 开放折线：0 与 total 是天然端点，中间是有效切点
    if (cs.empty()) return res;                           // 没有有效切点，无法剪切
    std::vector<double> br;
    br.push_back(0.0);
    for (double c : cs) br.push_back(c);
    br.push_back(total);

    // 找出点击落在哪个区间
    size_t k = 0;
    for (size_t i = 0; i + 1 < br.size(); ++i)
        if (tClick >= br[i] - 1e-9 && tClick <= br[i + 1] + 1e-9) { k = i; break; }

    // 只删被点中的那一段，两侧各自整体保留 —— 不能按每个交点把线全部切碎，
    // 那不是 CAD 里「修剪」的语义（AutoCAD/SolidWorks 都只移除点中的那一跨）。
    if (br[k] > 1e-7) {
        const auto left = subPolyline(v, false, br.front(), br[k]);
        if (left.size() >= 2) res.push_back(left);
    }
    if (br[k + 1] < total - 1e-7) {
        const auto right = subPolyline(v, false, br[k + 1], br.back());
        if (right.size() >= 2) res.push_back(right);
    }
    return res;
}

// ============================ 质心计算 ============================

Vec3 elementCentroid(const Element& e) {
    switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() >= 2)
                return Vec3((e.pts[0].x + e.pts[1].x) * 0.5,
                            (e.pts[0].y + e.pts[1].y) * 0.5);
            return e.pts.empty() ? Vec3() : e.pts[0];
        case ElementType::Circle:
        case ElementType::Arc:
            return e.pts.empty() ? Vec3() : e.pts[0];
        case ElementType::Rectangle:
            if (e.pts.size() >= 2)
                return Vec3((e.pts[0].x + e.pts[1].x) * 0.5,
                            (e.pts[0].y + e.pts[1].y) * 0.5);
            return e.pts.empty() ? Vec3() : e.pts[0];
        default: {
            // 多边形/样条/标注：顶点算术平均
            Vec3 c;
            for (const auto& p : e.pts) { c.x += p.x; c.y += p.y; }
            if (!e.pts.empty()) { c.x /= e.pts.size(); c.y /= e.pts.size(); }
            return c;
        }
    }
}

std::vector<Vec3> scaleAroundCentroid(const std::vector<Vec3>& pts, const Vec3& centroid, double factor) {
    std::vector<Vec3> out;
    out.reserve(pts.size());
    for (const auto& p : pts) {
        out.push_back(Vec3(centroid.x + (p.x - centroid.x) * factor,
                           centroid.y + (p.y - centroid.y) * factor));
    }
    return out;
}

std::vector<Vec3> rotateAroundCentroid(const std::vector<Vec3>& pts, const Vec3& centroid, double angleDeg) {
    const double rad = angleDeg * M_PI / 180.0;
    const double cs = std::cos(rad), sn = std::sin(rad);
    std::vector<Vec3> out;
    out.reserve(pts.size());
    for (const auto& p : pts) {
        const double dx = p.x - centroid.x, dy = p.y - centroid.y;
        out.push_back(Vec3(centroid.x + dx * cs - dy * sn,
                           centroid.y + dx * sn + dy * cs));
    }
    return out;
}

// ============================ 约束关系检测 ============================

bool isTangentLineCircle(const Element& line, const Element& circle, double tol) {
    if (line.type != ElementType::Line || line.pts.size() < 2) return false;
    if (circle.type != ElementType::Circle && circle.type != ElementType::Arc) return false;
    if (circle.pts.empty()) return false;
    const double d = distPointSegment(circle.pts[0], line.pts[0], line.pts[1]);
    return std::fabs(d - circle.radius) < tol;
}

bool isPerpendicularLines(const Element& la, const Element& lb, double tol) {
    if (la.type != ElementType::Line || la.pts.size() < 2) return false;
    if (lb.type != ElementType::Line || lb.pts.size() < 2) return false;
    const double angA = segAngleDeg(la.pts[0], la.pts[1]);
    const double angB = segAngleDeg(lb.pts[0], lb.pts[1]);
    double diff = std::fabs(wrapDeg180(angA - angB));
    // 垂直时差值应接近 90 或 -90
    const double d90 = std::fabs(diff - 90.0);
    const double d270 = std::fabs(diff - 270.0);
    return std::min(d90, d270) < tol;
}

bool isParallelLines(const Element& la, const Element& lb, double tol) {
    if (la.type != ElementType::Line || la.pts.size() < 2) return false;
    if (lb.type != ElementType::Line || lb.pts.size() < 2) return false;
    const double angA = segAngleDeg(la.pts[0], la.pts[1]);
    const double angB = segAngleDeg(lb.pts[0], lb.pts[1]);
    double diff = std::fabs(wrapDeg180(angA - angB));
    return diff < tol || std::fabs(diff - 180.0) < tol;
}

bool isTangentCircles(const Element& ca, const Element& cb, double tol) {
    if ((ca.type != ElementType::Circle && ca.type != ElementType::Arc) || ca.pts.empty()) return false;
    if ((cb.type != ElementType::Circle && cb.type != ElementType::Arc) || cb.pts.empty()) return false;
    const double d = ca.pts[0].distTo(cb.pts[0]);
    const double r1 = ca.radius, r2 = cb.radius;
    // 外切：d ≈ r1 + r2；内切：d ≈ |r1 - r2|
    return std::fabs(d - (r1 + r2)) < tol || std::fabs(d - std::fabs(r1 - r2)) < tol;
}

bool checkConstraint(const Element& a, const Element& b, ConstraintType ct, double tol) {
    switch (ct) {
        case ConstraintType::Tangent: {
            // 尝试直线-圆相切
            if (a.type == ElementType::Line && (b.type == ElementType::Circle || b.type == ElementType::Arc))
                return isTangentLineCircle(a, b, tol);
            if (b.type == ElementType::Line && (a.type == ElementType::Circle || a.type == ElementType::Arc))
                return isTangentLineCircle(b, a, tol);
            // 尝试圆-圆相切
            if ((a.type == ElementType::Circle || a.type == ElementType::Arc) &&
                (b.type == ElementType::Circle || b.type == ElementType::Arc))
                return isTangentCircles(a, b, tol);
            return false;
        }
        case ConstraintType::Perpendicular:
            return isPerpendicularLines(a, b, tol);
        case ConstraintType::Parallel:
            return isParallelLines(a, b, tol);
        case ConstraintType::Concentric: {
            // 同心：双方均为圆/弧，且圆心重合（半径不必相等）
            const bool aC = (a.type == ElementType::Circle || a.type == ElementType::Arc);
            const bool bC = (b.type == ElementType::Circle || b.type == ElementType::Arc);
            if (!aC || !bC || a.pts.empty() || b.pts.empty()) return false;
            return a.pts[0].distTo(b.pts[0]) <= tol;
        }
        default:
            return false;   // Symmetric/Dimensional 需额外参数（轴/目标值），由 ConstraintSolver 单独处理
    }
}

// ============================ 约束维持 ============================

bool restoreConstraint(const Element& a, Element& b, ConstraintType ct, double tol) {
    (void)tol;  // 容差参数保留给未来扩展（如带容差的近似恢复），当前实现用解析法精确恢复
    if (ct == ConstraintType::Tangent) {
        // 直线变 → 调整圆（移动圆心使距离恢复为半径）
        if (a.type == ElementType::Line && (b.type == ElementType::Circle || b.type == ElementType::Arc)) {
            if (a.pts.size() < 2 || b.pts.empty()) return false;
            // 将圆心投影到直线上，然后沿法线方向移动到距直线 radius 处
            const Vec3& p1 = a.pts[0], p2 = a.pts[1];
            const Vec3& center = b.pts[0];
            const double vx = p2.x - p1.x, vy = p2.y - p1.y;
            const double len2 = vx * vx + vy * vy;
            if (len2 < 1e-12) return false;
            double t = ((center.x - p1.x) * vx + (center.y - p1.y) * vy) / len2;
            t = std::max(0.0, std::min(1.0, t));
            const Vec3 foot(p1.x + t * vx, p1.y + t * vy);
            const Vec3 dir(center.x - foot.x, center.y - foot.y);
            const double d = dir.len();
            if (d < 1e-9) return false;
            // 沿原方向移动到距直线 = radius 的位置
            const Vec3 newPos(foot.x + dir.x / d * b.radius,
                              foot.y + dir.y / d * b.radius);
            b.pts[0] = newPos;
            return true;
        }
        // 圆变 → 调整直线（平移直线使距离恢复为新半径）
        if ((a.type == ElementType::Circle || a.type == ElementType::Arc) &&
            b.type == ElementType::Line) {
            if (b.pts.size() < 2 || a.pts.empty()) return false;
            // 计算圆心到直线的距离，将直线平移使距离 = 新半径
            const Vec3& center = a.pts[0];
            const Vec3& p1 = b.pts[0], p2 = b.pts[1];
            const double len = (p2 - p1).len2D();
            if (len < 1e-12) return false;
            // 法线方向（垂直于直线方向）
            const double nx = -(p2.y - p1.y) / len, ny = (p2.x - p1.x) / len;
            const double d = (center.x - p1.x) * nx + (center.y - p1.y) * ny;
            // 将直线沿法线方向移动，使距离变为 a.radius
            const double delta = a.radius - std::fabs(d);
            const int sign = (d >= 0) ? 1 : -1;
            b.pts[0] = Vec3(p1.x + sign * delta * nx, p1.y + sign * delta * ny);
            b.pts[1] = Vec3(p2.x + sign * delta * nx, p2.y + sign * delta * ny);
            return true;
        }
        // 圆-圆相切：调整 b 的圆心
        if ((a.type == ElementType::Circle || a.type == ElementType::Arc) &&
            (b.type == ElementType::Circle || b.type == ElementType::Arc)) {
            if (a.pts.empty() || b.pts.empty()) return false;
            const double d = a.pts[0].distTo(b.pts[0]);
            if (d < 1e-9) return false;
            // 判断原本是外切还是内切
            const bool wasExternal = (d > std::max(a.radius, b.radius));
            const double targetD = wasExternal ? (a.radius + b.radius) : std::fabs(a.radius - b.radius);
            const double ratio = targetD / d;
            b.pts[0] = Vec3(a.pts[0].x + (b.pts[0].x - a.pts[0].x) * ratio,
                            a.pts[0].y + (b.pts[0].y - a.pts[0].y) * ratio);
            return true;
        }
        return false;
    }

    if (ct == ConstraintType::Perpendicular) {
        if (b.type != ElementType::Line || b.pts.size() < 2) return false;
        if (a.type != ElementType::Line || a.pts.size() < 2) return false;
        // 旋转 b 使其与 a 垂直：以 b 的中点为基点旋转
        const double angA = segAngleDeg(a.pts[0], a.pts[1]);
        const double angB = segAngleDeg(b.pts[0], b.pts[1]);
        const double targetAngle = wrapDeg360(angA + 90.0);
        double delta = wrapDeg180(targetAngle - angB);
        const Vec3 mid((b.pts[0].x + b.pts[1].x) * 0.5, (b.pts[0].y + b.pts[1].y) * 0.5);
        b.pts = rotateAroundCentroid(b.pts, mid, delta);
        return true;
    }

    if (ct == ConstraintType::Parallel) {
        if (b.type != ElementType::Line || b.pts.size() < 2) return false;
        if (a.type != ElementType::Line || a.pts.size() < 2) return false;
        // 旋转 b 使其与 a 平行：以 b 的中点为基点旋转
        const double angA = segAngleDeg(a.pts[0], a.pts[1]);
        const double angB = segAngleDeg(b.pts[0], b.pts[1]);
        double delta = wrapDeg180(angA - angB);
        // 选择最近方向（0 或 180）
        if (std::fabs(delta) > 90.0) delta = wrapDeg180(delta + 180.0);
        const Vec3 mid((b.pts[0].x + b.pts[1].x) * 0.5, (b.pts[0].y + b.pts[1].y) * 0.5);
        b.pts = rotateAroundCentroid(b.pts, mid, delta);
        return true;
    }

    if (ct == ConstraintType::Concentric) {
        // 同心：把 b 的圆心移到 a 的圆心（半径保持不变，故同心≠等半径）
        const bool aC = (a.type == ElementType::Circle || a.type == ElementType::Arc);
        const bool bC = (b.type == ElementType::Circle || b.type == ElementType::Arc);
        if (!aC || !bC || a.pts.empty() || b.pts.empty()) return false;
        b.pts[0] = a.pts[0];
        return true;
    }

    return false;
}

// ============================ 剖面线填充（GB/T 4457.5-2013）============================

bool pointInPolygon(const Vec3& p, const std::vector<Vec3>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double yi = poly[i].y, yj = poly[j].y;
        const double xi = poly[i].x, xj = poly[j].x;
        // 射线法（even-odd）：从 p 向右发出射线，统计与边相交次数。
        // 用 (yi > p.y) != (yj > p.y) 排除「顶点恰好落在射线上」的重复计数。
        const bool intersect = ((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// 【防御】判断"外部边界"点列是否存在异常闭合缝。
// hatchLines 约定：region 是隐式闭合的多边形点列（首尾不重复），闭合边 = last→first。
// 若该闭合缝长度远大于点列内部最长边，说明点列很可能来自一条"开放曲线"被误当作
// 闭合轮廓（历史缺陷：开放样条被 sectionOutline 判为闭合，hatchLines 又无条件首尾
// 相连 → 填充溢出真实轮廓）。这类边界一律拒绝填充——宁可漏填，也不溢出。
// 判据：闭合缝 > 6×内部最长边 且 > 2mm（绝对下限抑制数值噪声的误判）。
static bool regionLooksClosed(const std::vector<Vec3>& poly) {
    const size_t n = poly.size();
    if (n < 3) return false;
    double maxInner = 0.0;
    for (size_t i = 0; i + 1 < n; ++i)          // 只统计内部相邻边，排除闭合缝
        maxInner = std::max(maxInner, poly[i].distTo(poly[i + 1]));
    if (maxInner <= 1e-12) return false;
    const double seam = poly.front().distTo(poly.back());
    if (seam <= 2.0) return true;               // 小尺度区域不做激进判定
    return seam <= 6.0 * maxInner;
}

std::vector<Seg> hatchLines(const std::vector<Vec3>& region,
                            const std::vector<std::vector<Vec3>>& holes,
                            double angleDeg, double spacing, double margin,
                            bool trustedClosed) {
    std::vector<Seg> out;
    if (region.size() < 3 || spacing <= 1e-9) return out;
    // 防御（仅对未受信调用方）：边界首尾未真正闭合（疑似开放曲线）时拒绝填充，
    // 避免渗漏/溢出。受信调用方（上游已权威校验闭合）跳过此启发式，防止误拒
    // "单一主导收尾缝"的合法闭合多边形（细长针状/月牙状）。
    if (!trustedClosed && !regionLooksClosed(region)) return out;

    const double rad = angleDeg * M_PI / 180.0;
    const double c = std::cos(rad), s = std::sin(rad);
    // 旋转到「剖面线水平」局部系：x' =  x*c + y*s ; y' = -x*s + y*c
    auto toLocal = [&](const Vec3& p) -> Vec3 {
        return Vec3(p.x * c + p.y * s, -p.x * s + p.y * c, 0.0);
    };
    // 逆旋转回世界系：x = x'*c - y'*s ; y = x'*s + y'*c
    auto toWorld = [&](double xp, double yp) -> Vec3 {
        return Vec3(xp * c - yp * s, xp * s + yp * c, 0.0);
    };

    struct Edge { double x1, y1, x2, y2; };
    std::vector<Edge> edges;
    double ymin =  kBig, ymax = -kBig, xmin = kBig, xmax = -kBig;
    auto updateBox = [&](double x, double y) {
        if (x < xmin) xmin = x;
        if (x > xmax) xmax = x;
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
    };
    auto addPoly = [&](const std::vector<Vec3>& poly) {
        if (poly.size() < 2) return;
        std::vector<Vec3> lp; lp.reserve(poly.size());
        for (const auto& p : poly) lp.push_back(toLocal(p));
        const size_t m = poly.size();   // 区域/孔均按闭合处理
        for (size_t i = 0; i < m; ++i) {
            const Vec3& a = lp[i];
            const Vec3& b = lp[(i + 1) % m];
            edges.push_back({a.x, a.y, b.x, b.y});
            updateBox(a.x, a.y);
        }
    };
    addPoly(region);
    for (const auto& h : holes) addPoly(h);

    if (ymin > ymax) return out;
    const double step = spacing;
    const double y0 = ymin - margin;
    const double y1 = ymax + margin;
    for (double yy = y0; yy <= y1 + 1e-9; yy += step) {
        std::vector<double> xs;
        for (const auto& e : edges) {
            // 严格跨过扫描线（避免共享顶点的重复计数）
            const bool aUp = (e.y1 <= yy);
            const bool bUp = (e.y2 <= yy);
            if (aUp == bUp) continue;          // 不跨线
            const double t = (yy - e.y1) / (e.y2 - e.y1);
            xs.push_back(e.x1 + t * (e.x2 - e.x1));
        }
        std::sort(xs.begin(), xs.end());
        // even-odd 配对：相邻两个交点之间的线段在区域内（内孔同样参与 → 自动掏空）
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double xa = xs[i], xb = xs[i + 1];
            if (xb - xa < 1e-9) continue;
            out.push_back(Seg{toWorld(xa, yy), toWorld(xb, yy)});
        }
    }
    (void)xmin; (void)xmax;
    return out;
}

} // namespace geo
} // namespace SketchCAD
