#include "Trim.h"
#include "Geometry.h"
#include "Vec.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace trim {

// 继承目标图元的样式与归属，只换几何 —— 剪切不应该改变颜色/线宽/图层
static Element inherit(const Element& src, ElementType t) {
    Element e;
    e.type = t;
    e.name = src.name;
    e.layer = src.layer;
    e.color = src.color;
    e.lineWidth = src.lineWidth;
    e.visible = src.visible;
    e.symbolLibrary = src.symbolLibrary;
    return e;
}

std::vector<Vec3> gatherCuts(const Element& t, const std::vector<Element>& cutters) {
    std::vector<Vec3> pts;
    for (const auto& c : cutters) {
        if (c.id == t.id || !c.visible) continue;
        // 标注与约束是注释性图元，不参与几何求交
        if (c.type == ElementType::Constraint || c.type == ElementType::Dimension) continue;
        for (const auto& p : geo::intersectElements(t, c)) {
            bool dup = false;
            for (const auto& q : pts) if (q.distTo(p) < 1e-7) { dup = true; break; }
            if (!dup) pts.push_back(p);
        }
    }
    return pts;
}

// ---------------------------- 折线族（直线/矩形/多边形/样条）----------------------------

// 把折线类图元统一成 (顶点序列, 是否闭合)，剪切完再按原类型（必要时降级）装回去
static bool polylineOf(const Element& e, std::vector<Vec3>& v, bool& closed) {
    switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() < 2) return false;
            v = {e.pts[0], e.pts[1]};
            closed = false;
            return true;
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) return false;
            const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
            v = {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
            closed = e.polyClosed();
            return true;
        }
        case ElementType::Polygon:
            if (e.pts.size() < 2) return false;
            v = e.pts;
            closed = e.polyClosed();
            return true;
        case ElementType::Spline:
            if (e.pts.size() < 2) return false;
            // 样条按采样点参与剪切：交点必须落在屏幕上那条光滑曲线上，
            // 而不是控制多边形上，否则用户会看到「点在曲线上却剪不到」。
            v = geo::sampleSpline(e.pts, 24);
            closed = false;
            return true;
        default:
            return false;
    }
}

static bool trimPolyline(const Element& t, const std::vector<Vec3>& cuts,
                         const Vec3& click, std::vector<Element>& out, QString& msg) {
    std::vector<Vec3> v;
    bool closed = false;
    if (!polylineOf(t, v, closed)) { msg = QStringLiteral("图元几何不完整，无法剪切"); return false; }

    std::vector<double> cs;
    cs.reserve(cuts.size());
    for (const auto& p : cuts) cs.push_back(geo::polylineParam(v, closed, p));
    const double tc = geo::polylineParam(v, closed, click);

    const auto pieces = geo::splitPolyline(v, closed, cs, tc);
    if (pieces.empty() && !cs.empty() && closed) {
        // 闭合轮廓只有 1 个交点时切不动（需要至少两个断点才能取出一段）
        msg = QStringLiteral("闭合轮廓上的有效交点少于 2 个，无法确定要剪掉的一段");
        return false;
    }
    if (pieces.empty()) {
        msg = QStringLiteral("在点击处未找到可用的剪切边界交点");
        return false;
    }

    for (const auto& pc : pieces) {
        if (pc.size() < 2) continue;
        if (t.type == ElementType::Line && pc.size() == 2) {
            Element e = inherit(t, ElementType::Line);
            e.pts = pc;
            out.push_back(e);
        } else if (t.type == ElementType::Spline) {
            Element e = inherit(t, ElementType::Spline);
            e.pts = pc;                       // 采样点即新控制点，视觉上与原曲线重合
            e.closed = false;
            out.push_back(e);
        } else {
            // 矩形被剪后不再是矩形，降级为开放多边形（折线）
            Element e = inherit(t, ElementType::Polygon);
            if (t.type == ElementType::Rectangle) e.name = QStringLiteral("折线");
            e.pts = pc;
            e.closed = false;
            out.push_back(e);
        }
    }
    return true;
}

// ---------------------------- 圆 ----------------------------

static bool trimCircle(const Element& t, const std::vector<Vec3>& cuts,
                       const Vec3& click, std::vector<Element>& out, QString& msg) {
    if (t.pts.empty() || t.radius <= 1e-9) { msg = QStringLiteral("圆几何不完整"); return false; }
    const Vec3 C = t.pts[0];
    std::vector<double> as;
    for (const auto& p : cuts) as.push_back(geo::segAngleDeg(C, p));
    std::sort(as.begin(), as.end());
    std::vector<double> u;
    for (double a : as) if (u.empty() || std::fabs(a - u.back()) > 1e-6) u.push_back(a);
    if (u.size() < 2) {
        msg = QStringLiteral("圆周上的交点少于 2 个，无法确定要剪掉的圆弧段");
        return false;
    }
    const double ac = geo::segAngleDeg(C, click);
    // 找出点击落在哪一段圆弧间隙里（末段回绕）
    size_t k = u.size() - 1;
    for (size_t i = 0; i + 1 < u.size(); ++i)
        if (ac >= u[i] && ac < u[i + 1]) { k = i; break; }

    const double from = u[(k + 1) % u.size()];        // 被剪段的终点 = 剩余弧的起点
    const double to   = u[k];                          // 被剪段的起点 = 剩余弧的终点
    const double sweep = geo::wrapDeg360(to - from);
    if (sweep < 1e-6) { msg = QStringLiteral("剪切后不剩任何圆弧"); return false; }

    Element e = inherit(t, ElementType::Arc);
    e.name = QStringLiteral("圆弧");
    e.pts = {C};
    e.radius = t.radius;
    e.startAngle = from;
    e.endAngle = from + sweep;
    out.push_back(e);
    return true;
}

// ---------------------------- 圆弧 ----------------------------

static bool trimArc(const Element& t, const std::vector<Vec3>& cuts,
                    const Vec3& click, std::vector<Element>& out, QString& msg) {
    if (t.pts.empty() || t.radius <= 1e-9) { msg = QStringLiteral("圆弧几何不完整"); return false; }
    const Vec3 C = t.pts[0];
    const double sweep0 = t.endAngle - t.startAngle;
    const double span = std::fabs(sweep0);
    if (span < 1e-6) { msg = QStringLiteral("圆弧扫掠角为 0"); return false; }
    const double sgn = (sweep0 >= 0) ? 1.0 : -1.0;

    // 统一换算成「沿扫掠方向走过的角度」u ∈ [0, span]，这样正反向弧共用同一套切分逻辑
    auto toU = [&](const Vec3& p) {
        const double a = geo::segAngleDeg(C, p);
        return sgn > 0 ? geo::wrapDeg360(a - t.startAngle) : geo::wrapDeg360(t.startAngle - a);
    };

    std::vector<double> us;
    for (const auto& p : cuts) {
        const double u = toU(p);
        if (u > 1e-6 && u < span - 1e-6) us.push_back(u);
    }
    std::sort(us.begin(), us.end());
    std::vector<double> cs;
    for (double u : us) if (cs.empty() || std::fabs(u - cs.back()) > 1e-6) cs.push_back(u);
    if (cs.empty()) { msg = QStringLiteral("圆弧上未找到位于弧内的交点"); return false; }

    std::vector<double> br;
    br.push_back(0.0);
    for (double c : cs) br.push_back(c);
    br.push_back(span);

    const double uc = std::max(0.0, std::min(span, toU(click)));
    size_t k = 0;
    for (size_t i = 0; i + 1 < br.size(); ++i)
        if (uc >= br[i] - 1e-9 && uc <= br[i + 1] + 1e-9) { k = i; break; }

    auto pushArc = [&](double u0, double u1) {
        if (u1 - u0 < 1e-6) return;
        Element e = inherit(t, ElementType::Arc);
        e.pts = {C};
        e.radius = t.radius;
        e.startAngle = t.startAngle + sgn * u0;
        e.endAngle   = t.startAngle + sgn * u1;
        out.push_back(e);
    };
    pushArc(0.0, br[k]);
    pushArc(br[k + 1], span);
    if (out.empty()) { msg = QStringLiteral("剪切后不剩任何圆弧"); return false; }
    return true;
}

// ---------------------------- 对称修剪/延伸两条线段 ----------------------------

// 浮点容差：
//   kAngleEps  —— 两直线方向夹角的正弦小于它，视为平行（角度容差 ~1e-9 rad）
//   kCollinearEps —— 一条线上的点落到另一条线上的距离小于它，视为共线
//   kEndEps    —— 保留端离交点不足它，则整条线已在交点一侧，保持原状（避免零长线）
static constexpr double kAngleEps    = 1e-9;
static constexpr double kCollinearEps = 1e-6;
static constexpr double kEndEps      = 1e-7;

bool extendTrimPair(const Element& a, const Vec3& pa,
                    const Element& b, const Vec3& pb,
                    Element& oa, bool& aChanged,
                    Element& ob, bool& bChanged,
                    QString& msg) {
    oa = a; ob = b; aChanged = bChanged = false;
    msg.clear();

    auto openEnds = [](const Element& e, Vec3& A, Vec3& B) -> bool {
        if (e.type != ElementType::Line) return false;
        if (e.pts.size() < 2) return false;
        A = e.pts[0]; B = e.pts[1];
        return true;
    };
    Vec3 a0, a1, b0, b1;
    if (!openEnds(a, a0, a1)) { msg = QStringLiteral("修剪/延伸：第 1 条图元不是直线"); return false; }
    if (!openEnds(b, b0, b1)) { msg = QStringLiteral("修剪/延伸：第 2 条图元不是直线"); return false; }

    const Vec3 da = a1 - a0, db = b1 - b0;
    const double la = std::hypot(da.x, da.y), lb = std::hypot(db.x, db.y);
    if (la < kEndEps || lb < kEndEps) { msg = QStringLiteral("直线过短，几何不完整"); return false; }

    const double cross = da.x * db.y - da.y * db.x;
    // 方向夹角正弦 = |cross| / (|da|·|db|)；过小 → 平行（含共线）
    if (std::fabs(cross) / (la * lb) < kAngleEps) {
        const double dist = std::fabs(da.x * (a0.y - b0.y) - da.y * (a0.x - b0.x)) / la;
        msg = (dist < kCollinearEps) ? QStringLiteral("两条直线共线，无法修剪/延伸")
                                     : QStringLiteral("两条直线平行，无交点");
        return false;
    }

    Vec3 P;
    if (!math::lineLineIntersect(a0, a1, b0, b1, P)) {
        msg = QStringLiteral("两条直线平行，无交点");
        return false;
    }

    // 单条线处理：保留离 pick 最近的端点，移动远端到 P；保留端已贴交点则保持原状。
    auto one = [&](const Element& src, const Vec3& p, Vec3& A, Vec3& B,
                   Element& out, bool& changed) {
        const int keepIdx = (p.distTo(A) <= p.distTo(B)) ? 0 : 1;
        const int moveIdx = 1 - keepIdx;
        const Vec3 keep = (keepIdx == 0) ? A : B;
        if (keep.distTo(P) < kEndEps) return;   // 整条线已在交点一侧，无需改动
        if (moveIdx == 0) A = P; else B = P;     // 仅移动远端
        out = src;
        out.pts = {A, B};
        changed = true;
    };
    one(a, pa, a0, a1, oa, aChanged);
    one(b, pb, b0, b1, ob, bChanged);

    if (!aChanged && !bChanged) { msg = QStringLiteral("交点与选择端重合，无需修改"); return false; }
    return true;
}

// ---------------------------- 入口 ----------------------------

bool trimElement(const Element& target, const std::vector<Element>& cutters,
                 const Vec3& clickPt, std::vector<Element>& out, QString& msg) {
    out.clear();
    msg.clear();
    if (target.type == ElementType::Constraint || target.type == ElementType::Dimension) {
        msg = QStringLiteral("约束与标注不是几何图元，不支持剪切");
        return false;
    }
    const std::vector<Vec3> cuts = gatherCuts(target, cutters);
    if (cuts.empty()) {
        msg = QStringLiteral("该图元与所选边界没有交点");
        return false;
    }
    switch (target.type) {
        case ElementType::Circle: return trimCircle(target, cuts, clickPt, out, msg);
        case ElementType::Arc:    return trimArc(target, cuts, clickPt, out, msg);
        default:                  return trimPolyline(target, cuts, clickPt, out, msg);
    }
}

} // namespace trim
} // namespace SketchCAD
