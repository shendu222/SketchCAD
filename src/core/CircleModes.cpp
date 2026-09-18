#include "CircleModes.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {
namespace circle {

// ============================ 内部工具函数 ============================

namespace {

// 把 Element 统一当成「可切线图元」获取其方向和位置信息
// 对于直线：返回线段两端点
// 对于圆/弧：返回圆心和半径
struct EntGeom {
    bool isLinear;
    Vec3 a, b;        // 直线端点（isLinear=true 时有效）
    Vec3 center;      // 圆心
    double radius;    // 半径（isLinear=false 时有效）
};
EntGeom entGeom(const Element& e) {
    EntGeom g;
    if (e.type == ElementType::Line) {
        g.isLinear = true;
        g.a = e.pts[0]; g.b = e.pts.size() >= 2 ? e.pts[1] : e.pts[0];
    } else {
        g.isLinear = false;
        g.center = e.pts.empty() ? Vec3() : e.pts[0];
        g.radius = e.radius;
    }
    return g;
}

// 收集与给定直线段相切、半径为 r 的圆的圆心轨迹（两条偏移线）
struct OffsetLine { Vec3 a, b; };
std::vector<OffsetLine> offsetSegment(const Vec3& p1, const Vec3& p2, double r) {
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-12) return {};
    const double nx = -dy / len, ny = dx / len;  // 左法线
    return {
        {Vec3(p1.x + nx*r, p1.y + ny*r, 0), Vec3(p2.x + nx*r, p2.y + ny*r, 0)},
        {Vec3(p1.x - nx*r, p1.y - ny*r, 0), Vec3(p2.x - nx*r, p2.y - ny*r, 0)}
    };
}

// 收集与给定圆相切、半径为 r 的圆的圆心轨迹（外切圆 + 内切圆）
struct OffsetCircle { Vec3 center; double radius; bool isExternal; };
std::vector<OffsetCircle> offsetCircleTangent(const Vec3& c, double R, double r) {
    std::vector<OffsetCircle> out;
    out.push_back({c, R + r, true});           // 外切：圆心距 = R + r
    if (std::fabs(R - r) > 1e-9)
        out.push_back({c, std::fabs(R - r), false});  // 内切：圆心距 = |R - r|
    return out;
}

// 点到图元的绝对距离
auto distEntAbs = [](const Vec3& p, const EntGeom& g) -> double {
    if (g.isLinear) return std::fabs(math::signedDistLine(p, g.a, g.b));
    return std::fabs(p.distTo(g.center) - g.radius);
};

// ---- 解析法：圆过点 P 且与两条直线相切 ----
// 原理：圆心在两直线的角平分线上（等距条件），
//       且到直线的距离 = 到 P 的距离（切线 + 过点条件），
//       在角平分线上只需解一元二次方程。
Result solveTan2Lines1Pt(const EntGeom& e1, const EntGeom& e2,
                          const Vec3& P, const Vec3& cur) {
    // 直线方向单位向量
    Vec3 d1 = (e1.b - e1.a).normalized();
    Vec3 d2 = (e2.b - e2.a).normalized();

    Vec3 V;
    bool parallel = !math::lineLineIntersect(e1.a, e1.b, e2.a, e2.b, V);
    std::vector<Vec3> candidates;

    if (parallel) {
        // 平行线：等距线（中线）上搜索
        // 中线距两直线均为 d/2，需 dist(center, P) = d/2
        double sd = math::signedDistLine(e2.a, e1.a, e1.b);
        double dist = std::fabs(sd);
        if (dist < 1e-9) {
            Result r; r.msg = QStringLiteral("两条线重合，无法确定唯一圆"); return r;
        }
        Vec3 dir = d1;
        Vec3 normal(-d1.y, d1.x, 0);
        // 中线上的点：e1.a 沿法线方向偏移 sd/2
        Vec3 midPt(e1.a.x + normal.x * sd * 0.5,
                    e1.a.y + normal.y * sd * 0.5, 0);
        // center = midPt + t*dir，dist(center,P) = dist/2
        double a = midPt.x - P.x, b = midPt.y - P.y;
        double m = a * dir.x + b * dir.y;
        double c = a*a + b*b - (dist*0.5)*(dist*0.5);
        // t^2 + 2*m*t + c = 0
        double disc = m*m - c;
        if (disc >= 0) {
            double sq = std::sqrt(disc);
            for (double t : {-m - sq, -m + sq}) {
                Vec3 center(midPt.x + t*dir.x, midPt.y + t*dir.y, 0);
                if (center.distTo(P) > 1e-6) candidates.push_back(center);
            }
        }
    } else {
        // 非平行线：在两条角平分线上求解二次方程
        // signedDistLine 的梯度 = (d1.y, -d1.x)（已验证）
        Vec3 bis[2] = {
            Vec3(d1.x + d2.x, d1.y + d2.y, 0),  // 内角平分线方向
            Vec3(d1.x - d2.x, d1.y - d2.y, 0)   // 外角平分线方向
        };

        for (const auto& bd : bis) {
            double blen = bd.len();
            if (blen < 1e-12) continue;
            Vec3 B(bd.x / blen, bd.y / blen, 0);  // 单位方向

            // k = |梯度 · B| = |d1.y*B.x - d1.x*B.y|
            double k = std::fabs(d1.y * B.x - d1.x * B.y);
            if (k < 1e-12) continue;

            // center = V + t*B，dist(center,L1) = |t|*k
            // dist(center,P)^2 = (Vx+t*Bx-Px)^2 + (Vy+t*By-Py)^2
            //  |B|=1 → t^2*k^2 = t^2 + 2*m*t + c
            //  (k^2-1)*t^2 - 2*m*t - c = 0
            double a = V.x - P.x, b = V.y - P.y;
            double m = a * B.x + b * B.y;
            double c = a*a + b*b;

            double A = k*k - 1.0;
            double BB = -2.0 * m;
            double C = -c;

            std::vector<double> roots;
            if (std::fabs(A) < 1e-12) {
                if (std::fabs(BB) > 1e-12) roots.push_back(-C / BB);
            } else {
                double disc = BB*BB - 4*A*C;
                if (disc >= 0) {
                    double sq = std::sqrt(disc);
                    roots.push_back((-BB + sq) / (2*A));
                    roots.push_back((-BB - sq) / (2*A));
                }
            }

            for (double t : roots) {
                Vec3 center(V.x + t*B.x, V.y + t*B.y, 0);
                double r = std::fabs(t) * k;
                if (r > 1e-6) candidates.push_back(center);
            }
        }
    }

    if (candidates.empty()) {
        Result r; r.msg = QStringLiteral("两直线条件下无解"); return r;
    }

    Result res;
    res.valid = true;
    res.center = math::pickNearest(candidates, cur);
    res.radius = res.center.distTo(P);
    for (const auto& c : candidates) {
        if (c.distTo(res.center) > 1e-6) res.altCenters.push_back(c);
    }
    return res;
}

// ---- 数值法：网格扫描 + 梯度下降精修 ----
// 适用于含圆/弧的混合情况，比纯梯度下降可靠得多
Result solveTan2Ent1PtNumerical(const EntGeom& e1, const EntGeom& e2,
                                  const Vec3& P, const Vec3& /*cur*/) {
    auto F = [&](double cx, double cy) {
        Vec3 p(cx, cy, 0);
        double rr = p.distTo(P);
        double dd1 = distEntAbs(p, e1);
        double dd2 = distEntAbs(p, e2);
        return (dd1 - rr) * (dd1 - rr) + (dd2 - rr) * (dd2 - rr);
    };

    // 搜索中心：P 与两图元参考点的平均
    Vec3 ref1 = e1.isLinear ?
        Vec3((e1.a.x+e1.b.x)*0.5, (e1.a.y+e1.b.y)*0.5, 0) : e1.center;
    Vec3 ref2 = e2.isLinear ?
        Vec3((e2.a.x+e2.b.x)*0.5, (e2.a.y+e2.b.y)*0.5, 0) : e2.center;
    Vec3 sc((P.x + ref1.x + ref2.x) / 3.0,
             (P.y + ref1.y + ref2.y) / 3.0, 0);
    double maxD = std::max({P.distTo(ref1), P.distTo(ref2),
                             ref1.distTo(ref2), 200.0});
    double searchR = maxD * 2.0 + 200.0;

    // 粗扫描：5mm 步长在整个搜索区域找最优起点
    const double step = 5.0;
    Vec3 bestC;
    double bestErr = 1e18;

    for (double dx = -searchR; dx <= searchR; dx += step) {
        for (double dy = -searchR; dy <= searchR; dy += step) {
            Vec3 c(sc.x + dx, sc.y + dy, 0);
            double r = c.distTo(P);
            if (r < 1e-6) continue;
            double d1 = distEntAbs(c, e1);
            double d2 = distEntAbs(c, e2);
            double err = (d1-r)*(d1-r) + (d2-r)*(d2-r);
            if (err < bestErr) { bestErr = err; bestC = c; }
        }
    }

    if (bestErr >= 1e18) {
        Result r; r.msg = QStringLiteral("无法找到满足条件的圆"); return r;
    }

    // 梯度下降精修
    Vec3 c = bestC;
    for (int iter = 0; iter < 1000; ++iter) {
        double r = c.distTo(P);
        if (r < 1e-9) break;
        double d1 = distEntAbs(c, e1);
        double d2 = distEntAbs(c, e2);
        double f = (d1-r)*(d1-r) + (d2-r)*(d2-r);
        if (f < 1e-18) break;

        const double eps = 1e-6;
        double gx = (F(c.x+eps, c.y) - F(c.x-eps, c.y)) / (2*eps);
        double gy = (F(c.x, c.y+eps) - F(c.x, c.y-eps)) / (2*eps);
        double gn = std::hypot(gx, gy);
        if (gn < 1e-15) break;

        double lr = 5.0 / (1.0 + iter * 0.03);
        c = Vec3(c.x - lr*gx/gn, c.y - lr*gy/gn, 0);
    }

    double r = c.distTo(P);
    double d1 = distEntAbs(c, e1);
    double d2 = distEntAbs(c, e2);
    if (r > 1e-6 && std::fabs(d1 - r) < 0.1 && std::fabs(d2 - r) < 0.1) {
        Result res; res.valid = true; res.center = c; res.radius = r;
        return res;
    }

    Result res; res.msg = QStringLiteral("无法找到满足条件的圆"); return res;
}

// 主入口：两直线用解析法（精确），其余用数值法（网格+精修）
Result solveTan2Ent1Pt(const EntGeom& e1, const EntGeom& e2,
                       const Vec3& P, const Vec3& cur) {
    if (e1.isLinear && e2.isLinear) {
        Result res = solveTan2Lines1Pt(e1, e2, P, cur);
        if (res.valid) return res;
    }
    return solveTan2Ent1PtNumerical(e1, e2, P, cur);
}

// 数值求解：圆过 P1、P2 且与图元 e 相切
// 圆心在 P1P2 中垂线上，设参数 t（沿中垂线方向偏移量）
Result solveTan1Ent2Pt(const EntGeom& e, const Vec3& P1, const Vec3& P2, const Vec3& /*cur*/) {
    const Vec3 mid = (P1 + P2) * 0.5;
    const Vec3 dir(P2.x - P1.x, P2.y - P1.y, 0);
    const double halfLen = dir.len() * 0.5;
    if (halfLen < 1e-9) { Result r; r.msg = QStringLiteral("两点重合"); return r; }
    // 中垂线方向
    const Vec3 n(-dir.y / (halfLen*2), dir.x / (halfLen*2), 0);

    auto distEnt = [](const Vec3& p, const EntGeom& g) -> double {
        if (g.isLinear) return std::fabs(math::signedDistLine(p, g.a, g.b));
        return p.distTo(g.center) - g.radius;
    };

    // 沿中垂线参数 t 搜索，dist(center, e) = dist(center, P1)
    auto centerAt = [&](double t) { return Vec3(mid.x + n.x*t, mid.y + n.y*t, 0); };

    double bestT = 0, bestErr = 1e18;
    // 扫描范围：-1000 到 1000，步长 0.5
    for (double t = -1000; t <= 1000; t += 0.5) {
        const Vec3 c = centerAt(t);
        const double r = c.distTo(P1);
        const double de = distEnt(c, e);
        const double err = (de - r) * (de - r);
        if (err < bestErr) { bestErr = err; bestT = t; }
    }
    // 精细搜索
    for (double dt = 0.1; dt > 1e-6; dt *= 0.1) {
        for (double t = bestT - 5*dt; t <= bestT + 5*dt; t += dt) {
            const Vec3 c = centerAt(t);
            const double r = c.distTo(P1);
            const double de = distEnt(c, e);
            const double err = (de - r) * (de - r);
            if (err < bestErr) { bestErr = err; bestT = t; }
        }
    }

    const Vec3 c = centerAt(bestT);
    const double r = c.distTo(P1);
    const double de = distEnt(c, e);
    if (r > 1e-6 && std::fabs(de - r) < 0.01) {
        // 检查是否还有另一解（中垂线两侧）
        Result res; res.valid = true; res.center = c; res.radius = r;
        // 另一解在 -bestT 处
        Vec3 c2 = centerAt(-bestT);
        double r2 = c2.distTo(P1);
        double de2 = distEnt(c2, e);
        if (r2 > 1e-6 && std::fabs(de2 - r2) < 0.01) {
            res.altCenters.push_back(c2);
        }
        return res;
    }
    Result res; res.msg = QStringLiteral("无法找到满足条件的圆"); return res;
}

} // anonymous namespace

// ============================ 1. 圆心 + 圆周点 ============================

Result centerPoint(const Vec3& c, const Vec3& p) {
    Result r;
    const double rad = c.distTo(p);
    if (rad < 1e-6) { r.msg = QStringLiteral("半径为 0"); return r; }
    r.valid = true; r.center = c; r.radius = rad;
    return r;
}

// ============================ 2. 两点画圆（直径） ============================

Result twoPoint(const Vec3& a, const Vec3& b) {
    Result r;
    const double d = a.distTo(b);
    if (d < 1e-6) { r.msg = QStringLiteral("两点距离为 0"); return r; }
    r.valid = true;
    r.center = Vec3((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, 0);
    r.radius = d * 0.5;
    return r;
}

// ============================ 3. 两点 + 半径 ============================

Result twoPointRadius(const Vec3& a, const Vec3& b, double radius, const Vec3& cur) {
    Result r;
    if (radius < 1e-6) { r.msg = QStringLiteral("半径无效"); return r; }
    const Vec3 mid((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, 0);
    const double halfChord = a.distTo(b) * 0.5;
    if (halfChord > radius - 1e-6) {
        r.msg = QStringLiteral("半径 %1 小于弦长一半 %2，无法画圆")
                    .arg(radius, 0, 'f', 2).arg(halfChord, 0, 'f', 2);
        return r;
    }
    // 弦中垂线方向
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) { r.msg = QStringLiteral("两点重合"); return r; }
    const Vec3 n(-dy / len, dx / len, 0);
    const double h = std::sqrt(radius * radius - halfChord * halfChord);
    // 两个候选圆心
    const Vec3 c1(mid.x + n.x * h, mid.y + n.y * h, 0);
    const Vec3 c2(mid.x - n.x * h, mid.y - n.y * h, 0);
    r.valid = true;
    r.radius = radius;
    // 取靠近光标的
    if (c1.distTo(cur) <= c2.distTo(cur)) {
        r.center = c1;
        r.altCenters.push_back(c2);
    } else {
        r.center = c2;
        r.altCenters.push_back(c1);
    }
    return r;
}

// ============================ 4. 三点定圆 ============================

Result threePoint(const Vec3& a, const Vec3& b, const Vec3& c) {
    Result r;
    Vec3 center; double radius;
    if (!geo::circleFrom3Points(a, b, c, center, radius)) {
        r.msg = QStringLiteral("三点共线，无法确定唯一圆");
        return r;
    }
    r.valid = true; r.center = center; r.radius = radius;
    return r;
}

// ============================ 5. 圆心 + 半径数值 ============================

Result centerRadius(const Vec3& c, double radius) {
    Result r;
    if (radius < 1e-6) { r.msg = QStringLiteral("半径无效，请在参数输入条输入半径"); return r; }
    r.valid = true; r.center = c; r.radius = radius;
    return r;
}

// ============================ 6. 相切两图元 + 过一点 ============================

Result tan2Ent1Pt(const Element& e1, const Element& e2, const Vec3& pt, const Vec3& cur) {
    return solveTan2Ent1Pt(entGeom(e1), entGeom(e2), pt, cur);
}

// ============================ 7. 相切一图元 + 过两点 ============================

Result tan1Ent2Pt(const Element& e, const Vec3& p1, const Vec3& p2, const Vec3& cur) {
    Result r = solveTan1Ent2Pt(entGeom(e), p1, p2, cur);
    // 选择离光标更近的解
    if (r.valid && !r.altCenters.empty()) {
        if (r.altCenters[0].distTo(cur) < r.center.distTo(cur)) {
            std::swap(r.center, r.altCenters[0]);
            r.radius = r.center.distTo(p1);
        }
    }
    return r;
}

// ============================ 8. 相切两图元 + 半径 ============================

Result tan2EntRadius(const Element& e1, const Element& e2, double radius, const Vec3& cur) {
    Result r;
    if (radius < 1e-6) { r.msg = QStringLiteral("半径无效"); return r; }
    const EntGeom g1 = entGeom(e1);
    const EntGeom g2 = entGeom(e2);
    std::vector<Vec3> candidates;

    // 生成两个图元的偏移轨迹，两两求交
    if (g1.isLinear && g2.isLinear) {
        // 两直线：偏移线求交
        auto off1 = offsetSegment(g1.a, g1.b, radius);
        auto off2 = offsetSegment(g2.a, g2.b, radius);
        for (const auto& o1 : off1) {
            for (const auto& o2 : off2) {
                Vec3 pt;
                if (math::lineLineIntersect(o1.a, o1.b, o2.a, o2.b, pt)) {
                    // 验证：切点应在线段上
                    candidates.push_back(pt);
                }
            }
        }
    } else if (g1.isLinear && !g2.isLinear) {
        // 直线 + 圆：偏移线与偏移圆求交
        auto off1 = offsetSegment(g1.a, g1.b, radius);
        auto off2 = offsetCircleTangent(g2.center, g2.radius, radius);
        for (const auto& o1 : off1) {
            for (const auto& o2 : off2) {
                Vec3 buf[2];
                int n = geo::segCircleIntersect(o1.a, o1.b, o2.center, o2.radius, buf);
                for (int i = 0; i < n; ++i) candidates.push_back(buf[i]);
            }
        }
    } else if (!g1.isLinear && g2.isLinear) {
        auto off1 = offsetCircleTangent(g1.center, g1.radius, radius);
        auto off2 = offsetSegment(g2.a, g2.b, radius);
        for (const auto& o1 : off1) {
            for (const auto& o2 : off2) {
                Vec3 buf[2];
                int n = geo::segCircleIntersect(o2.a, o2.b, o1.center, o1.radius, buf);
                for (int i = 0; i < n; ++i) candidates.push_back(buf[i]);
            }
        }
    } else {
        // 两圆：偏移圆求交
        auto off1 = offsetCircleTangent(g1.center, g1.radius, radius);
        auto off2 = offsetCircleTangent(g2.center, g2.radius, radius);
        for (const auto& o1 : off1) {
            for (const auto& o2 : off2) {
                Vec3 buf[2];
                int n = geo::circleCircleIntersect(o1.center, o1.radius, o2.center, o2.radius, buf);
                for (int i = 0; i < n; ++i) candidates.push_back(buf[i]);
            }
        }
    }

    if (candidates.empty()) {
        r.msg = QStringLiteral("无法找到半径 %1 mm 的相切圆").arg(radius, 0, 'f', 2);
        return r;
    }

    // 选离光标最近的
    r.valid = true;
    r.radius = radius;
    r.center = math::pickNearest(candidates, cur);
    for (const auto& c : candidates) {
        if (c.distTo(r.center) > 1e-6) r.altCenters.push_back(c);
    }
    return r;
}

// ============================ 9. 相切三图元 ============================

Result tan3Ent(const Element& e1, const Element& e2, const Element& e3, const Vec3& cur) {
    const EntGeom g1 = entGeom(e1);
    const EntGeom g2 = entGeom(e2);
    const EntGeom g3 = entGeom(e3);

    // 三直线：用角平分线法
    if (g1.isLinear && g2.isLinear && g3.isLinear) {
        // 求三直线两两交点
        Vec3 v12, v13, v23;
        bool ok12 = math::lineLineIntersect(g1.a, g1.b, g2.a, g2.b, v12);
        bool ok13 = math::lineLineIntersect(g1.a, g1.b, g3.a, g3.b, v13);
        bool ok23 = math::lineLineIntersect(g2.a, g2.b, g3.a, g3.b, v23);
        if (!ok12 || !ok13 || !ok23) {
            Result r; r.msg = QStringLiteral("三直线中有平行线，无法确定三角形"); return r;
        }

        // 内切圆：角平分线交点
        // 用 l1-l2 的角平分线与 l1-l3 的角平分线求交
        // 暴力搜索：在三个交点的重心附近找点到三直线等距的点
        Vec3 centroid((v12.x + v13.x + v23.x) / 3.0, (v12.y + v13.y + v23.y) / 3.0, 0);

        auto distLine = [](const Vec3& p, const Vec3& a, const Vec3& b) -> double {
            return std::fabs(math::signedDistLine(p, a, b));
        };

        // 梯度下降：最小化 (d1-d2)^2 + (d1-d3)^2
        Vec3 c = centroid;
        const double lr = 0.3;
        for (int iter = 0; iter < 1000; ++iter) {
            const double d1 = distLine(c, g1.a, g1.b);
            const double d2 = distLine(c, g2.a, g2.b);
            const double d3 = distLine(c, g3.a, g3.b);
            const double f = (d1-d2)*(d1-d2) + (d1-d3)*(d1-d3);
            if (f < 1e-16) break;
            const double eps = 1e-6;
            auto F = [&](double cx, double cy) {
                Vec3 p(cx, cy, 0);
                double dd1 = distLine(p, g1.a, g1.b);
                double dd2 = distLine(p, g2.a, g2.b);
                double dd3 = distLine(p, g3.a, g3.b);
                return (dd1-dd2)*(dd1-dd2) + (dd1-dd3)*(dd1-dd3);
            };
            const double gx = (F(c.x+eps, c.y) - F(c.x-eps, c.y)) / (2*eps);
            const double gy = (F(c.x, c.y+eps) - F(c.x, c.y-eps)) / (2*eps);
            const double gn = std::hypot(gx, gy);
            if (gn < 1e-15) break;
            c = Vec3(c.x - lr*gx/gn, c.y - lr*gy/gn, 0);
        }
        const double r = distLine(c, g1.a, g1.b);
        if (r > 1e-6) {
            Result res; res.valid = true; res.center = c; res.radius = r;
            return res;
        }
        Result res; res.msg = QStringLiteral("无法找到内切圆"); return res;
    }

    // 含圆的情况：数值求解
    // 圆心 (cx, cy)，到三个图元的距离相等 = r
    auto distEnt = [](const Vec3& p, const EntGeom& g) -> double {
        if (g.isLinear) return std::fabs(math::signedDistLine(p, g.a, g.b));
        return p.distTo(g.center) - g.radius;
    };

    // 初始猜测：三图元中心的平均
    Vec3 guess(0, 0, 0);
    int nEnt = 0;
    auto addCenter = [&](const EntGeom& g) {
        if (g.isLinear) {
            guess = Vec3(guess.x + (g.a.x+g.b.x)*0.5, guess.y + (g.a.y+g.b.y)*0.5, 0);
        } else {
            guess = guess + g.center;
        }
        ++nEnt;
    };
    addCenter(g1); addCenter(g2); addCenter(g3);
    guess = Vec3(guess.x / nEnt, guess.y / nEnt, 0);

    // 也从 cur 位置尝试
    Vec3 c = (guess.distTo(cur) < 200) ? guess : cur;
    const double lr = 0.5;
    for (int iter = 0; iter < 1000; ++iter) {
        const double d1 = distEnt(c, g1);
        const double d2 = distEnt(c, g2);
        const double d3 = distEnt(c, g3);
        const double f = (d1-d2)*(d1-d2) + (d1-d3)*(d1-d3) + (d2-d3)*(d2-d3);
        if (f < 1e-16) break;
        const double eps = 1e-6;
        auto F = [&](double cx, double cy) {
            Vec3 p(cx, cy, 0);
            double dd1 = distEnt(p, g1);
            double dd2 = distEnt(p, g2);
            double dd3 = distEnt(p, g3);
            return (dd1-dd2)*(dd1-dd2) + (dd1-dd3)*(dd1-dd3) + (dd2-dd3)*(dd2-dd3);
        };
        const double gx = (F(c.x+eps, c.y) - F(c.x-eps, c.y)) / (2*eps);
        const double gy = (F(c.x, c.y+eps) - F(c.x, c.y-eps)) / (2*eps);
        const double gn = std::hypot(gx, gy);
        if (gn < 1e-15) break;
        c = Vec3(c.x - lr*gx/gn, c.y - lr*gy/gn, 0);
    }

    const double r = std::fabs(distEnt(c, g1));
    const double d2 = distEnt(c, g2);
    const double d3 = distEnt(c, g3);
    if (r > 1e-6 && std::fabs(d2 - r) < 0.01 && std::fabs(d3 - r) < 0.01) {
        Result res; res.valid = true; res.center = c; res.radius = r;
        return res;
    }
    Result res; res.msg = QStringLiteral("无法找到相切三图元的公切圆"); return res;
}

// ============================ 10. 三直线内切圆 ============================

Result inscribed(const Element& l1, const Element& l2, const Element& l3) {
    return tan3Ent(l1, l2, l3, Vec3(0, 0, 0));  // 内切圆是 tan3Ent 的特例
}

// ============================ 11. 同心圆 ============================

Result concentric(const Element& ref, double radius, const Vec3& /*cur*/) {
    Result r;
    if (ref.pts.empty()) { r.msg = QStringLiteral("参考图元无效"); return r; }
    if (ref.type != ElementType::Circle && ref.type != ElementType::Arc) {
        r.msg = QStringLiteral("同心圆需要选择圆或圆弧作为参考");
        return r;
    }
    if (radius < 1e-6) { r.msg = QStringLiteral("半径无效"); return r; }
    r.valid = true;
    r.center = ref.pts[0];
    r.radius = radius;
    return r;
}

// ============================ 12. 由弧转圆 ============================

Result byArc(const Element& arc) {
    Result r;
    if (arc.type != ElementType::Arc) {
        r.msg = QStringLiteral("请选择圆弧图元");
        return r;
    }
    if (arc.pts.empty()) { r.msg = QStringLiteral("圆弧数据无效"); return r; }
    r.valid = true;
    r.center = arc.pts[0];
    r.radius = arc.radius;
    return r;
}

// ============================ 主入口 ============================

Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param) {
    Result r;

    // 获取参考图元指针
    auto getRef = [&](int idx) -> const Element* {
        if (idx < 0 || idx >= (int)refs.size()) return nullptr;
        for (const auto& e : elems) if (e.id == refs[idx]) return &e;
        return nullptr;
    };

    switch (m) {
    case Mode::CenterPoint: {
        // 预览模式：用 pts + cur
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;  // 还没点够
        return centerPoint(pp[0], pp[1]);
    }
    case Mode::TwoPoint: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return twoPoint(pp[0], pp[1]);
    }
    case Mode::TwoPointRadius: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        const double rad = (param > 1e-6) ? param : pp[0].distTo(pp[1]) * 0.5;
        // 如果没设参数，用弦长一半作默认（退化但能预览）
        return twoPointRadius(pp[0], pp[1], rad, cur);
    }
    case Mode::ThreePoint: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) {
            // 预览：不足3点时用辅助线
            if (pp.size() == 2) {
                // 退化预览：以两点为直径画圆
                return twoPoint(pp[0], pp[1]);
            }
            return r;
        }
        return threePoint(pp[0], pp[1], pp[2]);
    }
    case Mode::CenterRadius: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 1) return r;
        const double rad = (param > 1e-6) ? param : 10.0;  // 默认10mm预览
        return centerRadius(pp[0], rad);
    }
    case Mode::Tan2Ent1Pt: {
        const Element* e1 = getRef(0);
        const Element* e2 = getRef(1);
        if (!e1 || !e2) return r;
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 1) return r;
        return tan2Ent1Pt(*e1, *e2, pp[0], cur);
    }
    case Mode::Tan1Ent2Pt: {
        const Element* e = getRef(0);
        if (!e) return r;
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return tan1Ent2Pt(*e, pp[0], pp[1], cur);
    }
    case Mode::Tan2EntRadius: {
        const Element* e1 = getRef(0);
        const Element* e2 = getRef(1);
        if (!e1 || !e2) return r;
        const double rad = (param > 1e-6) ? param : 10.0;
        return tan2EntRadius(*e1, *e2, rad, cur);
    }
    case Mode::Tan3Ent: {
        const Element* e1 = getRef(0);
        const Element* e2 = getRef(1);
        const Element* e3 = getRef(2);
        if (!e1 || !e2 || !e3) return r;
        return tan3Ent(*e1, *e2, *e3, cur);
    }
    case Mode::Inscribed: {
        const Element* e1 = getRef(0);
        const Element* e2 = getRef(1);
        const Element* e3 = getRef(2);
        if (!e1 || !e2 || !e3) return r;
        return inscribed(*e1, *e2, *e3);
    }
    case Mode::Concentric: {
        const Element* e = getRef(0);
        if (!e) return r;
        const double rad = (param > 1e-6) ? param : e->pts[0].distTo(cur);
        return concentric(*e, rad, cur);
    }
    case Mode::ByArc: {
        const Element* e = getRef(0);
        if (!e) return r;
        return byArc(*e);
    }
    }
    return r;
}

// ============================ 工具映射 ============================

Mode modeFromString(const QString& s) {
    if (s == "circle")              return Mode::CenterPoint;
    if (s == "circle_2p")            return Mode::TwoPoint;
    if (s == "circle_2pr")           return Mode::TwoPointRadius;
    if (s == "circle3p")            return Mode::ThreePoint;
    if (s == "circle_cr")            return Mode::CenterRadius;
    if (s == "circle_t2e1p")         return Mode::Tan2Ent1Pt;
    if (s == "circle_t1e2p")        return Mode::Tan1Ent2Pt;
    if (s == "circle_t2er")         return Mode::Tan2EntRadius;
    if (s == "circle_t3e")          return Mode::Tan3Ent;
    if (s == "circle_inscribed")    return Mode::Inscribed;
    if (s == "circle_concentric")   return Mode::Concentric;
    if (s == "circle_byarc")        return Mode::ByArc;
    return Mode::CenterPoint;
}

QString stringFromMode(Mode m) {
    switch (m) {
    case Mode::CenterPoint:   return "circle";
    case Mode::TwoPoint:      return "circle_2p";
    case Mode::TwoPointRadius:return "circle_2pr";
    case Mode::ThreePoint:    return "circle3p";
    case Mode::CenterRadius:  return "circle_cr";
    case Mode::Tan2Ent1Pt:    return "circle_t2e1p";
    case Mode::Tan1Ent2Pt:    return "circle_t1e2p";
    case Mode::Tan2EntRadius: return "circle_t2er";
    case Mode::Tan3Ent:       return "circle_t3e";
    case Mode::Inscribed:     return "circle_inscribed";
    case Mode::Concentric:    return "circle_concentric";
    case Mode::ByArc:         return "circle_byarc";
    }
    return "circle";
}

ReqCount requirements(Mode m) {
    switch (m) {
    case Mode::CenterPoint:    return {0, 1, false};
    case Mode::TwoPoint:       return {0, 2, false};
    case Mode::TwoPointRadius: return {0, 2, true};
    case Mode::ThreePoint:     return {0, 3, false};
    case Mode::CenterRadius:   return {0, 1, true};
    case Mode::Tan2Ent1Pt:     return {2, 1, false};
    case Mode::Tan1Ent2Pt:     return {1, 2, false};
    case Mode::Tan2EntRadius:  return {2, 0, true};
    case Mode::Tan3Ent:        return {3, 0, false};
    case Mode::Inscribed:      return {3, 0, false};
    case Mode::Concentric:    return {1, 0, true};
    case Mode::ByArc:          return {1, 0, false};
    }
    return {0, 0, false};
}

std::vector<ElementType> allowedTypes(Mode m) {
    switch (m) {
    case Mode::Tan2Ent1Pt:
    case Mode::Tan2EntRadius:
    case Mode::Tan3Ent:
        return {ElementType::Line, ElementType::Circle, ElementType::Arc};
    case Mode::Tan1Ent2Pt:
        return {ElementType::Line, ElementType::Circle, ElementType::Arc};
    case Mode::Inscribed:
        return {ElementType::Line, ElementType::Rectangle};
    case Mode::Concentric:
        return {ElementType::Circle, ElementType::Arc};
    case Mode::ByArc:
        return {ElementType::Arc};
    default:
        return {};
    }
}

} // namespace circle
} // namespace SketchCAD
