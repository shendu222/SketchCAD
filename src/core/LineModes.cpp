#include "LineModes.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace line {

// 从图元中提取一条代表性的直线（用于平行/垂直/角平分线参考）。
// 对矩形，返回离 nearPoint 最近的边所在无限直线。
static bool getRefLine(const Element& e, const Vec3& nearPoint, Vec3& outA, Vec3& outB) {
    if (e.type == ElementType::Line && e.pts.size() >= 2) {
        outA = e.pts[0]; outB = e.pts[1]; return true;
    }
    if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
        const Vec3 a = e.pts[0];
        const Vec3 c = e.pts[1];
        const Vec3 b(a.x, c.y, 0);
        const Vec3 d(c.x, a.y, 0);
        const Vec3 edges[4][2] = {{a, b}, {b, c}, {c, d}, {d, a}};
        int best = 0; double bestD = 1e300;
        for (int i = 0; i < 4; ++i) {
            const double dd = math::pointLineDist(nearPoint, edges[i][0], edges[i][1]);
            if (dd < bestD) { bestD = dd; best = i; }
        }
        outA = edges[best][0]; outB = edges[best][1]; return true;
    }
    return false;
}

static bool getCircle(const Element& e, Vec3& c, double& r) {
    if ((e.type == ElementType::Circle || e.type == ElementType::Arc) && !e.pts.empty()) {
        c = e.pts[0]; r = e.radius; return true;
    }
    return false;
}

// ---------- 基础模式 ----------
Result twoPoint(const Vec3& a, const Vec3& b) { return {true, a, b, {}, {}}; }
Result horizontal(const Vec3& a, const Vec3& b) { return {true, a, Vec3(b.x, a.y, 0), {}, {}}; }
Result vertical(const Vec3& a, const Vec3& b) { return {true, a, Vec3(a.x, b.y, 0), {}, {}}; }
Result angle(const Vec3& a, const Vec3& b, double angleDeg) {
    const double L = a.distTo(b);
    if (L < 1e-9) return {false, a, a, "长度为零", {}};
    const double th = math::deg2rad(angleDeg);
    return {true, a, Vec3(a.x + L * std::cos(th), a.y + L * std::sin(th), 0), {}, {}};
}

// ---------- 方向约束模式 ----------
Result parallelThroughPoint(const Element& refLine, const Vec3& p, const Vec3& cur) {
    Vec3 a, b;
    if (!getRefLine(refLine, p, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 dir = (b - a).normalized();
    const Vec3 proj = math::projPointLine(cur, p, p + dir);
    return {true, p, proj, {}, {}};
}

Result orthogonalThroughPoint(const Element& refLine, const Vec3& p, const Vec3& cur) {
    Vec3 a, b;
    if (!getRefLine(refLine, p, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 dir = (b - a).perp().normalized();
    const Vec3 proj = math::projPointLine(cur, p, p + dir);
    return {true, p, proj, {}, {}};
}

// 过点平行：保持参考线长度与方向，沿其垂直方向把整条线平移到「与该点对齐」的位置。
// 偏移量 = 该点到参考线起点的垂直分量 t；新线为 (a + n·t, b + n·t)，
// 恰好经过该点（沿垂直方向对齐），方向与长度与参考线完全一致。
Result parallelAligned(const Element& refLine, const Vec3& p) {
    Vec3 a, b;
    if (!getRefLine(refLine, p, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 dir = (b - a).normalized();
    const Vec3 n = dir.perp();                 // 垂直单位向量
    const double t = (p - a).dot2D(n);          // 该点到参考线的有符号垂直偏移
    return {true, a + n * t, b + n * t, {}, {}};
}

// 角度 + 长度 → 第二点：用于「参数输入条」的角度直线（选起点后直接按参数绘制）。
Vec3 pointAtAngleLength(const Vec3& a, double angleDeg, double length) {
    const double th = math::deg2rad(angleDeg);
    return Vec3(a.x + length * std::cos(th), a.y + length * std::sin(th), 0);
}

// 平行复制：参考线 + 方向点 → 沿垂直方向按间隔生成多条同长度平行线。
ParallelCopies parallelCopies(const Element& refLine, const Vec3& sidePoint,
                              double spacing, int count) {
    ParallelCopies res;
    Vec3 a, b;
    if (!getRefLine(refLine, sidePoint, a, b)) { res.msg = QStringLiteral("参考图元不是直线或矩形"); return res; }
    if (count <= 0) { res.msg = QStringLiteral("平行线条数必须 >= 1"); return res; }
    const Vec3 dir = (b - a).normalized();
    const Vec3 n = dir.perp();                  // 垂直单位向量
    // 方向点投影到垂直轴的有符号距离决定整体偏移方向（点在参考线哪一侧）。
    const double t = (sidePoint - a).dot2D(n);
    const double sign = (t >= 0) ? 1.0 : -1.0;
    const double sp = (spacing > 0) ? spacing : 20.0;
    for (int k = 1; k <= count; ++k) {
        const double off = sign * sp * k;
        res.lines.emplace_back(a + n * off, b + n * off);
    }
    res.valid = true;
    return res;
}

Result relativeAngle(const Element& refLine, const Vec3& p, const Vec3& cur, double angleDeg) {
    Vec3 a, b;
    if (!getRefLine(refLine, p, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const double base = geo::segAngleDeg(a, b);
    return angle(p, cur, base + angleDeg);
}

Result orthogonalFromLine(const Element& refLine, const Vec3& pOnLine, const Vec3& cur) {
    // 先找到参考线上离 pOnLine 最近的点作为垂足，再沿垂线方向延伸到 cur 的投影。
    Vec3 a, b;
    if (!getRefLine(refLine, pOnLine, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 foot = math::projPointLine(pOnLine, a, b);
    const Vec3 dir = (b - a).perp().normalized();
    const double t = (cur - foot).dot2D(dir);
    return {true, foot, foot + dir * t, {}, {}};
}

Result fromPointToLine(const Element& refLine, const Vec3& p) {
    Vec3 a, b;
    if (!getRefLine(refLine, p, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 foot = math::projPointLine(p, a, b);
    return {true, p, foot, {}, {}};
}

// ---------- 角平分线 ----------
Result bisector(const Element& l1, const Element& l2, const Vec3& p, const Vec3& cur) {
    Vec3 a1, b1, a2, b2;
    if (!getRefLine(l1, p, a1, b1) || !getRefLine(l2, p, a2, b2))
        return {false, {}, {}, "角平分线需要两条直线/矩形边作为参考", {}};
    // 求交点（无限直线）
    const Vec3 d1 = b1 - a1, d2 = b2 - a2;
    const double den = d1.x * d2.y - d1.y * d2.x;
    if (std::fabs(den) < 1e-12) return {false, {}, {}, "两参考线平行，无角平分线", {}};
    const double t = ((a2.x - a1.x) * d2.y - (a2.y - a1.y) * d2.x) / den;
    const Vec3 ip = a1 + d1 * t;

    const Vec3 u1 = (b1 - a1).normalized();
    const Vec3 u2 = (b2 - a2).normalized();
    // 两个平分线方向；选终点离 cur 更近的那条。
    Vec3 dA = (u1 + u2).normalized();
    Vec3 dB = (u1 - u2).normalized();
    if (dA.len() < 1e-9) dA = u1.perp();
    if (dB.len() < 1e-9) dB = u1.perp();
    // 让方向指向 p 所在侧
    if ((p - ip).dot2D(dA) < 0) dA = dA * -1.0;
    if ((p - ip).dot2D(dB) < 0) dB = dB * -1.0;
    const double dist = p.distTo(ip);
    const Vec3 endA = ip + dA * dist;
    const Vec3 endB = ip + dB * dist;
    const Vec3 end = (cur.distTo(endA) < cur.distTo(endB)) ? endA : endB;
    return {true, ip, end, {}, {}};
}

// ---------- 切线 ----------
Result tangentPC(const Element& circle, const Vec3& p, const Vec3& cur) {
    Vec3 C; double r;
    if (!getCircle(circle, C, r)) return {false, {}, {}, "请先选择圆或圆弧", {}};
    const Vec3 d = p - C;
    const double D = d.len();
    if (D < r - 1e-9) return {false, {}, {}, "点在圆内，无实切线", {}};
    if (D < 1e-9) return {false, {}, {}, "点与圆心重合", {}};
    const double phi = std::atan2(d.y, d.x);
    if (D < r + 1e-9) {
        // 点在圆上：切线唯一，切点即 p
        return {true, p, p + d.perp().normalized() * 20.0, {}, {}}; // 默认长度 20
    }
    const double alpha = std::acos(r / D);
    Vec3 t1(C.x + r * std::cos(phi + alpha), C.y + r * std::sin(phi + alpha), 0);
    Vec3 t2(C.x + r * std::cos(phi - alpha), C.y + r * std::sin(phi - alpha), 0);
    // 对圆弧，过滤不在弧段内的切点
    if (circle.type == ElementType::Arc) {
        auto inArc = [&](const Vec3& q) {
            const double ang = math::rad2deg(std::atan2(q.y - C.y, q.x - C.x));
            return geo::angleInSweep(ang, circle.startAngle, circle.endAngle - circle.startAngle);
        };
        const bool ok1 = inArc(t1), ok2 = inArc(t2);
        if (ok1 && !ok2) { t2 = t1; }
        else if (ok2 && !ok1) { t1 = t2; }
        else if (!ok1 && !ok2) return {false, {}, {}, "光标附近无有效弧切点", {}};
    }
    const Vec3 end = (cur.distTo(t1) <= cur.distTo(t2)) ? t1 : t2;
    return {true, p, end, {}, {}};
}

// 两圆公切线：计算所有外切线和内切线，返回离光标最近的作为主解，其余存入 altLines。
//
// 几何原理：
//   外公切线（两圆同侧）：法向量 n 满足 |n·d| = |r1-r2| / D（delta = r1-r2）
//     条件：D > |r1-r2|（两圆不内含/内切）
//   内公切线（两圆异侧）：法向量 n 满足 |n·d| = (r1+r2) / D（sigma = r1+r2）
//     条件：D > r1+r2（两圆不相交/外切）
//
// 最多 4 条切线：2 条外公切线 + 2 条内公切线。
// 圆弧情况：切点不在弧范围内的会被过滤掉。
Result tangentCC(const Element& c1e, const Element& c2e, const Vec3& cur) {
    Vec3 C1, C2; double r1, r2;
    if (!getCircle(c1e, C1, r1) || !getCircle(c2e, C2, r2))
        return {false, {}, {}, "请选择两个圆或圆弧", {}};
    const Vec3 d = C2 - C1;
    const double D = d.len();
    if (D < 1e-9) return {false, {}, {}, "两圆心重合", {}};
    const Vec3 u = d.normalized();    // 连心线方向
    const Vec3 v = d.perp().normalized();  // 垂直方向

    // 圆弧切点过滤
    auto inArc = [&](const Element& e, const Vec3& pt) -> bool {
        if (e.type != ElementType::Arc) return true;
        const double ang = math::rad2deg(std::atan2(pt.y - e.pts[0].y, pt.x - e.pts[0].x));
        return geo::angleInSweep(ang, e.startAngle, e.endAngle - e.startAngle);
    };

    // 收集所有有效切线
    std::vector<std::pair<Vec3, Vec3>> allTans;

    // ---- 外公切线 ----
    const double delta = r1 - r2;
    if (D > std::fabs(delta) + 1e-9) {
        const double L = std::sqrt(std::max(0.0, D * D - delta * delta));
        const double s = delta / D;   // sin(gamma)
        const double c = L / D;       // cos(gamma)
        // 两个外切线方向的法向量
        Vec3 n1(-s * u.x + c * v.x, -s * u.y + c * v.y, 0);
        Vec3 n2( s * u.x + c * v.x,  s * u.y + c * v.y, 0);
        Vec3 a1 = C1 + n1 * r1, b1 = C2 + n1 * r2;
        Vec3 a2 = C1 + n2 * r1, b2 = C2 + n2 * r2;
        if (inArc(c1e, a1) && inArc(c2e, b1)) allTans.push_back({a1, b1});
        if (inArc(c1e, a2) && inArc(c2e, b2)) allTans.push_back({a2, b2});
    }

    // ---- 内公切线 ----
    const double sigma = r1 + r2;
    if (D > sigma + 1e-9) {
        const double Li = std::sqrt(std::max(0.0, D * D - sigma * sigma));
        const double si = sigma / D;  // sin(gamma_int)
        const double ci = Li / D;     // cos(gamma_int)
        // 两个内切线方向的法向量（两圆切点在连心线异侧）
        Vec3 n1(-si * u.x + ci * v.x, -si * u.y + ci * v.y, 0);
        Vec3 n2( si * u.x + ci * v.x,  si * u.y + ci * v.y, 0);
        // 内切线：c1 的切点在 +n 方向，c2 的切点在 -n 方向（异侧）
        Vec3 a1 = C1 + n1 * r1,  b1 = C2 - n1 * r2;
        Vec3 a2 = C1 + n2 * r1,  b2 = C2 - n2 * r2;
        if (inArc(c1e, a1) && inArc(c2e, b1)) allTans.push_back({a1, b1});
        if (inArc(c1e, a2) && inArc(c2e, b2)) allTans.push_back({a2, b2});
    }

    if (allTans.empty())
        return {false, {}, {}, "两圆无公切线（内含或相交）", {}};

    // 按切线中点到光标的距离排序，取最近的为主解
    auto distToCur = [&](const std::pair<Vec3, Vec3>& tan) {
        const Vec3 mid((tan.first.x + tan.second.x) * 0.5,
                       (tan.first.y + tan.second.y) * 0.5);
        return cur.distTo(mid);
    };
    std::sort(allTans.begin(), allTans.end(),
              [&](const auto& a, const auto& b) { return distToCur(a) < distToCur(b); });

    Result res;
    res.valid = true;
    res.a = allTans[0].first;
    res.b = allTans[0].second;
    for (size_t i = 1; i < allTans.size(); ++i)
        res.altLines.push_back(allTans[i]);
    return res;
}

Result tangentOrthogonal(const Element& circle, const Element& refLine, const Vec3& cur) {
    Vec3 C; double r;
    if (!getCircle(circle, C, r)) return {false, {}, {}, "请先选择圆或圆弧", {}};
    Vec3 a, b;
    if (!getRefLine(refLine, C, a, b)) return {false, {}, {}, "参考图元不是直线或矩形", {}};
    const Vec3 dir = (b - a).normalized();
    // 切线垂直于参考线 → 切点处半径平行于参考线 → 切点有两个：C ± r*dir
    Vec3 t1 = C + dir * r;
    Vec3 t2 = C - dir * r;
    if (circle.type == ElementType::Arc) {
        auto inArc = [&](const Vec3& q) {
            const double ang = math::rad2deg(std::atan2(q.y - C.y, q.x - C.x));
            return geo::angleInSweep(ang, circle.startAngle, circle.endAngle - circle.startAngle);
        };
        const bool ok1 = inArc(t1), ok2 = inArc(t2);
        if (ok1 && !ok2) t2 = t1;
        else if (ok2 && !ok1) t1 = t2;
        else if (!ok1 && !ok2) return {false, {}, {}, "无有效弧切点", {}};
    }
    const Vec3 t = (cur.distTo(t1) <= cur.distTo(t2)) ? t1 : t2;
    // 切线方向垂直于参考线，从切点向外延伸默认长度 20
    const Vec3 perpDir = dir.perp().normalized();
    const double s = (cur - t).dot2D(perpDir);
    return {true, t, t + perpDir * s, {}, {}};
}

// ---------- Snake ----------
Result snakeSegment(const std::vector<Vec3>& pts, const Vec3& cur, bool xFirst) {
    if (pts.empty()) return {false, {}, {}, "Snake 需要至少一个已有点", {}};
    const Vec3 last = pts.back();
    bool horizontal;
    if (pts.size() == 1) {
        horizontal = xFirst;
    } else {
        const Vec3 prev = pts[pts.size() - 2];
        const bool lastWasHoriz = (std::fabs(prev.y - last.y) < 1e-9);
        horizontal = !lastWasHoriz;
    }
    if (horizontal) return {true, last, Vec3(cur.x, last.y, 0), {}, {}};
    return {true, last, Vec3(last.x, cur.y, 0), {}, {}};
}

// ---------- 主入口 ----------
Result compute(Mode m, const std::vector<Vec3>& pts, const std::vector<int>& refs,
               const Vec3& cur, const std::vector<Element>& elems, double param) {
    auto find = [&](int id) -> const Element* {
        for (const auto& e : elems) if (e.id == id) return &e;
        return nullptr;
    };

    switch (m) {
    case Mode::TwoPoint:
        if (pts.size() < 2) return {false, {}, {}, "需要两个点", {}};
        return twoPoint(pts[0], pts[1]);
    case Mode::AngleFromLine:
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点", {}};
        if (auto* ref = find(refs[0])) return relativeAngle(*ref, pts.back(), cur, param);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::Horizontal:
        if (pts.size() < 2) return {false, {}, {}, "需要两个点", {}};
        return horizontal(pts[0], pts[1]);
    case Mode::Vertical:
        if (pts.size() < 2) return {false, {}, {}, "需要两个点", {}};
        return vertical(pts[0], pts[1]);
    case Mode::Angle:
        if (pts.size() < 2) return {false, {}, {}, "需要两个点", {}};
        return angle(pts[0], pts[1], param);
    case Mode::Parallel:
        // 单条「过点平行」：过 pts 中那一点，长度方向由 cur 决定（命令行 line_parallel 沿用）。
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点确定平行线位置", {}};
        if (auto* ref = find(refs[0])) return parallelThroughPoint(*ref, pts.back(), cur);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::ParallelThroughPoint:
        // 过点平行且保持参考线长度与方向（菜单 Parallel through point / 命令行 line_parallel_pt）。
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点确定平行线位置", {}};
        if (auto* ref = find(refs[0])) return parallelAligned(*ref, pts.back());
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::Orthogonal:
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点确定垂线位置", {}};
        if (auto* ref = find(refs[0])) return orthogonalThroughPoint(*ref, pts.back(), cur);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::RelativeAngle:
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点", {}};
        if (auto* ref = find(refs[0])) return relativeAngle(*ref, pts.back(), cur, param);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::OrthogonalFromLine:
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要在参考线上拾取一点", {}};
        if (auto* ref = find(refs[0])) return orthogonalFromLine(*ref, pts.back(), cur);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::FromPointToLine:
        if (refs.empty()) return {false, {}, {}, "需要先选择参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取外部点", {}};
        if (auto* ref = find(refs[0])) return fromPointToLine(*ref, pts.back());
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::Bisector:
        if (refs.size() < 2) return {false, {}, {}, "需要选择两条参考线", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取一点确定平分线长度", {}};
        { const Element* l1 = find(refs[0]); const Element* l2 = find(refs[1]);
          if (!l1 || !l2) return {false, {}, {}, "参考图元不存在", {}};
          return bisector(*l1, *l2, pts.back(), cur); }
    case Mode::TangentPC:
        if (refs.empty()) return {false, {}, {}, "需要先选择圆", {}};
        if (pts.empty()) return {false, {}, {}, "需要拾取外部点", {}};
        if (auto* ref = find(refs[0])) return tangentPC(*ref, pts.back(), cur);
        return {false, {}, {}, "参考图元不存在", {}};
    case Mode::TangentCC:
        if (refs.size() < 2) return {false, {}, {}, "需要选择两个圆", {}};
        { const Element* c1 = find(refs[0]); const Element* c2 = find(refs[1]);
          if (!c1 || !c2) return {false, {}, {}, "参考图元不存在", {}};
          return tangentCC(*c1, *c2, cur); }
    case Mode::TangentOrthogonal:
        if (refs.size() < 2) return {false, {}, {}, "需要选择圆和参考线", {}};
        { const Element* c = find(refs[0]); const Element* l = find(refs[1]);
          if (!c || !l) return {false, {}, {}, "参考图元不存在", {}};
          return tangentOrthogonal(*c, *l, cur); }
    case Mode::Snake:
        return snakeSegment(pts, cur, /*xFirst=*/(pts.size() % 2 == 1));
    case Mode::SnakeX:
        return snakeSegment(pts, cur, /*xFirst=*/true);
    case Mode::SnakeY:
        return snakeSegment(pts, cur, /*xFirst=*/false);
    }
    return {false, {}, {}, "未实现的直线模式", {}};
}

Mode modeFromString(const QString& s) {
    const QString t = s.toLower();
    if (t == "line_2pt" || t == "line") return Mode::TwoPoint;
    if (t == "line_h") return Mode::Horizontal;
    if (t == "line_v") return Mode::Vertical;
    if (t == "line_angle") return Mode::Angle;
    if (t == "line_parallel") return Mode::Parallel;
    if (t == "line_parallel_pt") return Mode::ParallelThroughPoint;
    if (t == "line_bisector") return Mode::Bisector;
    if (t == "line_tangent_pc") return Mode::TangentPC;
    if (t == "line_tangent_cc") return Mode::TangentCC;
    if (t == "line_tangent_ortho") return Mode::TangentOrthogonal;
    if (t == "line_orthogonal") return Mode::Orthogonal;
    if (t == "line_rel_angle") return Mode::RelativeAngle;
    if (t == "line_snake") return Mode::Snake;
    if (t == "line_snake_x") return Mode::SnakeX;
    if (t == "line_snake_y") return Mode::SnakeY;
    if (t == "line_angle_from_line") return Mode::AngleFromLine;
    if (t == "line_orthogonal_from_line") return Mode::OrthogonalFromLine;
    if (t == "line_from_point_to_line") return Mode::FromPointToLine;
    // 构造线（Xline）复用直线几何计算：H/V/A/2pt/B/O → 对应 Mode，
    // 结果线段 (a,b) 经 through-point a + 方向角(b-a) 转为无限直线。
    if (t == "xline")        return Mode::TwoPoint;
    if (t == "xline_h" || t == "xline_horizontal")      return Mode::Horizontal;
    if (t == "xline_v" || t == "xline_vertical")        return Mode::Vertical;
    if (t == "xline_angle" || t == "xline_a")           return Mode::Angle;
    if (t == "xline_2pt" || t == "xline_2points")       return Mode::TwoPoint;
    if (t == "xline_bisector" || t == "xline_b")        return Mode::Bisector;
    if (t == "xline_parallel" || t == "xline_o")        return Mode::Parallel;
    return Mode::TwoPoint;
}

ReqCount requirements(Mode m) {
    switch (m) {
    // 参考图元 + 定位点
    case Mode::ParallelThroughPoint:
    case Mode::Orthogonal:
    case Mode::OrthogonalFromLine:
    case Mode::FromPointToLine:          return {1, 1, false};
    case Mode::RelativeAngle:            return {1, 2, false};
    case Mode::AngleFromLine:            return {1, 2, false};
    case Mode::TangentPC:                return {1, 1, false};
    // 参考图元 + 固定步数（UsePoint 由参考图元环节解释）
    case Mode::TangentCC:                return {2, 0, false};
    case Mode::TangentOrthogonal:        return {2, 0, false};
    case Mode::Bisector:                 return {2, 1, false};
    // 反向 = 两定位点
    case Mode::TwoPoint:
    case Mode::Horizontal:
    case Mode::Vertical:                 return {0, 2, false};
    // 右键结束
    case Mode::Snake:
    case Mode::SnakeX:
    case Mode::SnakeY:                   return {0, 0, true};
    // 无参直接绘制
    case Mode::Angle:
    case Mode::Parallel:                 return {0, 0, false};
    }
    return {0, 0, false};
}

std::vector<ElementType> allowedTypes(Mode m) {
    switch (m) {
    case Mode::Parallel:
    case Mode::ParallelThroughPoint:
    case Mode::Orthogonal:
    case Mode::OrthogonalFromLine:
    case Mode::FromPointToLine:
    case Mode::Bisector:
    case Mode::RelativeAngle:
    case Mode::AngleFromLine:
        return {ElementType::Line, ElementType::Rectangle};
    case Mode::TangentPC:
    case Mode::TangentCC:
    case Mode::TangentOrthogonal:
        return {ElementType::Circle, ElementType::Arc};
    default:
        return {};
    }
}

QString stringFromMode(Mode m) {
    switch (m) {
    case Mode::TwoPoint: return "line_2pt";
    case Mode::Horizontal: return "line_h";
    case Mode::Vertical: return "line_v";
    case Mode::Angle: return "line_angle";
    case Mode::Parallel: return "line_parallel";
    case Mode::ParallelThroughPoint: return "line_parallel_pt";
    case Mode::Bisector: return "line_bisector";
    case Mode::TangentPC: return "line_tangent_pc";
    case Mode::TangentCC: return "line_tangent_cc";
    case Mode::TangentOrthogonal: return "line_tangent_ortho";
    case Mode::Orthogonal: return "line_orthogonal";
    case Mode::RelativeAngle: return "line_rel_angle";
    case Mode::Snake: return "line_snake";
    case Mode::SnakeX: return "line_snake_x";
    case Mode::SnakeY: return "line_snake_y";
    case Mode::AngleFromLine: return "line_angle_from_line";
    case Mode::OrthogonalFromLine: return "line_orthogonal_from_line";
    case Mode::FromPointToLine: return "line_from_point_to_line";
    }
    return "line_2pt";
}

} // namespace line
} // namespace SketchCAD
