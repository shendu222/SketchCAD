#include "ChamferFillet.h"
#include "Vec.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace corner {

// 浮点容差
static constexpr double kAngEps      = 1e-9;   // 方向夹角正弦阈值（平行判定）
static constexpr double kThetaMin    = 1e-3;   // 最小夹角（rad），过小视为平行
static constexpr double kThetaMax    = M_PI - 1e-3; // 最大夹角，过大视为共线
static constexpr double kEndEps      = 1e-9;   // 端点/参数下限

// 解析出的两线几何（交点、保留端、指向保留端的单位方向、夹角）
struct Geom {
    Vec3 P;            // 两线所在直线的交点（角点）
    Vec3 Ka, Kb;       // 两条线的保留端
    Vec3 u1, u2;       // 从 P 指向保留端 Ka / Kb 的单位方向
    double theta = 0;  // u1 与 u2 之间的夹角（rad，0..π）
    bool   ok = false;
    QString msg;
};

// 解析保留端：byPick 时取离 pick 最近端点；否则取离角点 P 最远的端点。
static bool resolveKeep(const Element& e, const Vec3& P, bool byPick,
                       const Vec3& pick, Vec3& keepEnd, Vec3& u) {
    if (e.pts.size() < 2) return false;
    const Vec3 A = e.pts[0], B = e.pts[1];
    Vec3 cand = byPick ? (pick.distTo(A) <= pick.distTo(B) ? A : B)
                       : (A.distTo(P) >= B.distTo(P) ? A : B);
    keepEnd = cand;
    Vec3 dir = cand - P;
    const double L = std::hypot(dir.x, dir.y);
    if (L < kEndEps) return false;   // 保留端与角点重合，无法倒角/倒圆
    u = Vec3(dir.x / L, dir.y / L, 0);
    return true;
}

// 公共几何解析：校验图元类型 / 平行 / 共线 / 夹角，解析交点与保留端。
static Geom resolveGeom(const Element& a, const Element& b, bool byPick,
                      const Vec3& pa, const Vec3& pb) {
    Geom g;
    if (a.type != ElementType::Line || b.type != ElementType::Line) {
        g.msg = QStringLiteral("倒角/倒圆仅支持直线图元"); return g;
    }
    if (a.pts.size() < 2 || b.pts.size() < 2) {
        g.msg = QStringLiteral("直线几何不完整"); return g;
    }
    if (!math::lineLineIntersect(a.pts[0], a.pts[1], b.pts[0], b.pts[1], g.P)) {
        g.msg = QStringLiteral("两直线平行，无交点"); return g;
    }
    if (!resolveKeep(a, g.P, byPick, pa, g.Ka, g.u1)) { g.msg = QStringLiteral("直线 a 端点与角点重合"); return g; }
    if (!resolveKeep(b, g.P, byPick, pb, g.Kb, g.u2)) { g.msg = QStringLiteral("直线 b 端点与角点重合"); return g; }
    const double dot = std::clamp(g.u1.x * g.u2.x + g.u1.y * g.u2.y, -1.0, 1.0);
    g.theta = std::acos(std::fabs(dot));
    if (g.theta < kThetaMin) { g.msg = QStringLiteral("两直线近似平行，无法倒角/倒圆"); return g; }
    if (g.theta > kThetaMax) { g.msg = QStringLiteral("两直线近似共线"); return g; }
    g.ok = true;
    return g;
}

// 把裁断点 T1/T2 与裁短线、倒角/倒圆几何组装进 res。
static void buildCorner(const Element& a, const Element& b,
                       const Vec3& P, const Vec3& Ka, const Vec3& u1,
                       const Vec3& Kb, const Vec3& u2,
                       double d1, double d2, bool isFillet, double r,
                       CornerResult& res) {
    const Vec3 T1 = P + u1 * d1;   // a 上的裁断点（距 P 为 d1）
    const Vec3 T2 = P + u2 * d2;   // b 上的裁断点

    Element ea = a; ea.pts = {Ka, T1};
    Element eb = b; eb.pts = {Kb, T2};

    if (isFillet) {
        // 圆心：u1 与 u2 的角平分线（指向夹角内侧）上，距 P 为 r/sin(θ/2)
        Vec3 bis = u1 + u2;
        const double Lb = bis.len2D();
        if (Lb < kEndEps) { res.msg = QStringLiteral("角平分线退化"); return; }
        bis = Vec3(bis.x / Lb, bis.y / Lb, 0);
        const double theta = std::acos(std::clamp(u1.x * u2.x + u1.y * u2.y, -1.0, 1.0));
        const double dC = r / std::sin(theta / 2.0);
        const Vec3 C = P + bis * dC;
        const double a0  = std::atan2(T1.y - C.y, T1.x - C.x) * 180.0 / M_PI;
        const double a1v = std::atan2(T2.y - C.y, T2.x - C.x) * 180.0 / M_PI;
        Element arc = a;
        arc.type = ElementType::Arc;
        arc.pts = {C};
        arc.radius = r;
        arc.startAngle = a0;
        arc.endAngle = a1v;
        arc.name = QStringLiteral("圆角");
        res.lineA = ea; res.lineB = eb; res.corner = arc; res.ok = true;
    } else {
        Element ch = a;
        ch.type = ElementType::Line;
        ch.pts = {T1, T2};
        ch.name = QStringLiteral("倒角");
        res.lineA = ea; res.lineB = eb; res.corner = ch; res.ok = true;
    }
}

// 越界检查 + 距离派生 + 组装，供三个公共函数复用。
static CornerResult finalize(const Element& a, const Element& b, const Geom& g,
                            double d1, double d2, bool isFillet, double r) {
    CornerResult res;
    if (!g.ok) { res.msg = g.msg; return res; }
    if (isFillet) {
        if (r < kEndEps) { res.msg = QStringLiteral("圆角半径必须为正数"); return res; }
        const double d = r / std::tan(g.theta / 2.0);
        d1 = d2 = d;
    }
    if (d1 < -kEndEps || d2 < -kEndEps) {
        res.msg = QStringLiteral("倒角距离必须为非负"); return res;
    }
    if (d1 > g.Ka.distTo(g.P) + 1e-6) { res.msg = QStringLiteral("距离 d1 超过直线 a 的可用长度"); return res; }
    if (d2 > g.Kb.distTo(g.P) + 1e-6) { res.msg = QStringLiteral("距离 d2 超过直线 b 的可用长度"); return res; }
    buildCorner(a, b, g.P, g.Ka, g.u1, g.Kb, g.u2, d1, d2, isFillet, r, res);
    return res;
}

CornerResult chamferPair(const Element& a, const Element& b, double d1, double d2,
                        bool byPick, const Vec3& pa, const Vec3& pb) {
    return finalize(a, b, resolveGeom(a, b, byPick, pa, pb), d1, d2, false, 0.0);
}

CornerResult filletPair(const Element& a, const Element& b, double r,
                       bool byPick, const Vec3& pa, const Vec3& pb) {
    return finalize(a, b, resolveGeom(a, b, byPick, pa, pb), 0.0, 0.0, true, r);
}

CornerResult chamferPairDistAngle(const Element& a, const Element& b, double d, double angleDeg,
                                 bool byPick, const Vec3& pa, const Vec3& pb) {
    Geom g = resolveGeom(a, b, byPick, pa, pb);
    CornerResult res;
    if (!g.ok) { res.msg = g.msg; return res; }
    if (d < -kEndEps) { res.msg = QStringLiteral("倒角距离必须为非负"); return res; }
    if (d > g.Ka.distTo(g.P) + 1e-6) { res.msg = QStringLiteral("距离 d 超过直线 a 的可用长度"); return res; }

    const double ang = angleDeg * M_PI / 180.0;
    // 夹角约束：倒角线与 a 的夹角 ang 必须介于 0 与两线夹角 θ 之间。
    if (ang < kThetaMin || ang > g.theta - 1e-6) {
        res.msg = QStringLiteral("倒角角度须介于 0 与两线夹角之间"); return res;
    }

    const Vec3 T1 = g.P + g.u1 * d;
    // 倒角线方向与 a 成 ang：两个候选方向（自 u1 旋 ±ang，线是无向的，故两解等价候选）。
    // 取与 b 正向射线（P→Kb）相交、且退距 d2>0 的那个；这样倒角必落在角内、切掉角尖。
    Vec3 bestT2; double bestD2 = -1; bool found = false;
    for (int sgn : {1, -1}) {
        const double ca = std::cos(sgn * ang), sa = std::sin(sgn * ang);
        const Vec3 cdir(ca * g.u1.x - sa * g.u1.y, sa * g.u1.x + ca * g.u1.y, 0);
        Vec3 T2;
        if (!math::lineLineIntersect(T1, T1 + cdir, g.P, g.P + g.u2, T2)) continue;
        const double d2 = (T2 - g.P).x * g.u2.x + (T2 - g.P).y * g.u2.y;
        if (d2 > 1e-6 && d2 > bestD2) { bestT2 = T2; bestD2 = d2; found = true; }
    }
    if (!found) {
        res.msg = QStringLiteral("倒角线无法落在第 2 条直线正向一侧，请检查角度"); return res;
    }
    return finalize(a, b, g, d, bestD2, false, 0.0);
}

} // namespace corner
} // namespace SketchCAD
