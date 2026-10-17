#include "CenterLineGen.h"
#include <cmath>
#include <algorithm>
#include "Geometry.h"   // geo:: 几何原语（intersectElements / elementBounds / elementCentroid）
#include "Vec.h"

namespace SketchCAD {

namespace {

// 构造一条中心线（细点画线 = GB 中心线）。所有产物统一线型 DashDot，不新增枚举。
Element mkCenterLine(const Vec3& a, const Vec3& b, const QString& layer) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    e.layer = layer;
    e.lineStyle = LineStyle::DashDot;     // GB/T 4457.4：轴线/对称中心线 = 细点画线
    e.lineWidth = 0.6; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = QStringLiteral("中心线");
    return e;
}

// 提取圆信息：成功返回 true（c=圆心，r=半径）。半径<=0 视为退化。
bool circleInfo(const Element& e, Vec3& c, double& r) {
    if (e.type == ElementType::Circle && e.radius > 0.0) {
        c = e.pts.empty() ? Vec3() : e.pts[0];
        r = e.radius;
        return true;
    }
    return false;
}

// 提取直线端点。
bool lineEnds(const Element& e, Vec3& a, Vec3& b) {
    if (e.type == ElementType::Line && e.pts.size() >= 2) {
        a = e.pts[0]; b = e.pts[1];
        return true;
    }
    return false;
}

// 由圆中心 c 与半径 r 生成一组「过圆心的十字中心线」，每臂伸出 (r+gap+extend)。
std::vector<Element> crossThroughCircle(const Vec3& c, double r, const CLParams& p) {
    const double half = r + p.gap + p.extend;
    std::vector<Element> out;
    out.push_back(mkCenterLine(Vec3(c.x - half, c.y, 0), Vec3(c.x + half, c.y, 0), p.layer));
    out.push_back(mkCenterLine(Vec3(c.x, c.y - half, 0), Vec3(c.x, c.y + half, 0), p.layer));
    return out;
}

// 由点 c 生成一组「固定尺寸的十字中心线」，每臂长 = extend。
std::vector<Element> crossAtPoint(const Vec3& c, const CLParams& p) {
    std::vector<Element> out;
    out.push_back(mkCenterLine(Vec3(c.x - p.extend, c.y, 0), Vec3(c.x + p.extend, c.y, 0), p.layer));
    out.push_back(mkCenterLine(Vec3(c.x, c.y - p.extend, 0), Vec3(c.x, c.y + p.extend, 0), p.layer));
    return out;
}

// 单轴水平中心线：过圆中心，半长 = r+gap+extend。
Element hAxisThroughCircle(const Vec3& c, double r, const CLParams& p) {
    const double half = r + p.gap + p.extend;
    return mkCenterLine(Vec3(c.x - half, c.y, 0), Vec3(c.x + half, c.y, 0), p.layer);
}

} // namespace

std::vector<Element> makeCenterLine(CenterLineKind kind,
                                    const std::vector<Element>& refs,
                                    const CLParams& p,
                                    QString* reason) {
    auto fail = [&](const char* msg) -> std::vector<Element> {
        if (reason) *reason = QString::fromUtf8(msg);
        return {};
    };

    if (refs.empty()) return fail("refs 为空：中心线需要至少一个参考图元");

    switch (kind) {

    // ---- 中心线：沿轴线/直线延伸的单轴中心线 ----
    case CenterLineKind::kCenter: {
        Vec3 a, b;
        if (lineEnds(refs[0], a, b)) {                 // 参考为直线 → 沿该直线延伸
            const Vec3 d = (b - a).normalized();
            return { mkCenterLine(a - d * p.extend, b + d * p.extend, p.layer) };
        }
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r))                 // 参考为圆 → 水平轴线
            return { hAxisThroughCircle(c, r, p) };
        return fail("kCenter：参考既非直线亦非有效圆");
    }

    // ---- 十字中心线：过圆心（或交点/点）的一对垂直中心线 ----
    case CenterLineKind::kCross: {
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r)) return crossThroughCircle(c, r, p);
        Vec3 a, b;
        if (lineEnds(refs[0], a, b)) return crossAtPoint((a + b) * 0.5, p);   // 直线取中点
        if (refs[0].type == ElementType::Point && !refs[0].pts.empty())
            return crossAtPoint(refs[0].pts[0], p);                            // 点图元
        return fail("kCross：参考非圆/直线/点");
    }

    // ---- 圆心标记：圆/圆弧中心的小十字标记（支持多圆批量）----
    case CenterLineKind::kCircleMark: {
        std::vector<Element> out;
        for (const Element& e : refs) {
            Vec3 c; double r;
            if (!circleInfo(e, c, r)) continue;                 // 跳过无效圆
            const double m = std::min(std::max(r * 0.25, 2.0), 6.0);  // 标记半臂长，限幅
            out.push_back(mkCenterLine(Vec3(c.x - m, c.y, 0), Vec3(c.x + m, c.y, 0), p.layer));
            out.push_back(mkCenterLine(Vec3(c.x, c.y - m, 0), Vec3(c.x, c.y + m, 0), p.layer));
        }
        if (out.empty()) return fail("kCircleMark：refs 中无有效圆");
        return out;
    }

    // ---- 角点中心线：过两直线交点的角平分线 ----
    case CenterLineKind::kCorner: {
        if (refs.size() < 2) return fail("kCorner：需要两条相交直线");
        Vec3 a0, b0, a1, b1;
        if (!lineEnds(refs[0], a0, b0) || !lineEnds(refs[1], a1, b1))
            return fail("kCorner：参考非直线");
        std::vector<Vec3> xs = geo::intersectElements(refs[0], refs[1]);
        if (xs.empty()) return fail("kCorner：两直线无交点（平行/退化）");   // 平行无交点→空
        const Vec3 corner = xs.front();
        const Vec3 da = (b0 - a0).normalized();
        const Vec3 db = (b1 - a1).normalized();
        Vec3 bis = da + db;
        if (bis.len2D() < 1e-9) bis = da.perp();          // 反向平行 → 用垂线兜底
        bis = bis.normalized();
        return { mkCenterLine(corner - bis * p.extend, corner + bis * p.extend, p.layer) };
    }

    // ---- 对称板中心线：矩形（板）的双对称轴线 ----
    case CenterLineKind::kPlate: {
        // 矩形：取包围盒，画水平 + 垂直两条对称轴
        if (refs[0].type == ElementType::Rectangle) {
            const QRectF bb = geo::elementBounds(refs[0]);
            if (bb.isNull()) return fail("kPlate：矩形包围盒为空");
            const double cx = (bb.left() + bb.right()) * 0.5;
            const double cy = (bb.top() + bb.bottom()) * 0.5;
            std::vector<Element> out;
            out.push_back(mkCenterLine(Vec3(bb.left()  - p.extend, cy, 0),
                                      Vec3(bb.right() + p.extend, cy, 0), p.layer));
            out.push_back(mkCenterLine(Vec3(cx, bb.bottom() - p.extend, 0),
                                      Vec3(cx, bb.top()    + p.extend, 0), p.layer));
            return out;
        }
        // 退化兜底：若给的是圆，等同整圆十字
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r)) return crossThroughCircle(c, r, p);
        return fail("kPlate：参考非矩形");
    }

    // ---- 整圆十字中心线：过整圆的完整十字中心线 ----
    case CenterLineKind::kFullCircle: {
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r)) return crossThroughCircle(c, r, p);
        return fail("kFullCircle：参考非有效圆");
    }

    // ---- 角度中心线：沿角两臂延伸的中心线 ----
    case CenterLineKind::kAngle: {
        std::vector<Element> out;
        for (const Element& e : refs) {
            Vec3 a, b;
            if (!lineEnds(e, a, b)) continue;             // 跳过非直线
            const Vec3 d = (b - a).normalized();
            out.push_back(mkCenterLine(a - d * p.extend, b + d * p.extend, p.layer));
        }
        if (out.empty()) return fail("kAngle：refs 中无有效直线");
        return out;
    }

    // ---- 孔中心线：过孔中心的单轴中心线 ----
    case CenterLineKind::kHole: {
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r)) return { hAxisThroughCircle(c, r, p) };
        return fail("kHole：参考非有效圆（孔）");
    }

    // ---- 通孔中心线：过孔中心、向两侧长延伸（表示贯通）----
    case CenterLineKind::kThroughHole: {
        Vec3 c; double r;
        if (circleInfo(refs[0], c, r)) {
            const double half = r + p.gap + p.extend * 2.0;   // 比 kHole 更长，体现贯通
            return { mkCenterLine(Vec3(c.x - half, c.y, 0), Vec3(c.x + half, c.y, 0), p.layer) };
        }
        return fail("kThroughHole：参考非有效圆（孔）");
    }

    // ---- 两线交点十字线：过两已知直线交点的十字中心线 ----
    case CenterLineKind::kTwoLinesCross: {
        if (refs.size() < 2) return fail("kTwoLinesCross：需要两条相交直线");
        Vec3 a0, b0, a1, b1;
        if (!lineEnds(refs[0], a0, b0) || !lineEnds(refs[1], a1, b1))
            return fail("kTwoLinesCross：参考非直线");
        std::vector<Vec3> xs = geo::intersectElements(refs[0], refs[1]);
        if (xs.empty()) return fail("kTwoLinesCross：两直线无交点（平行/退化）");
        return crossAtPoint(xs.front(), p);
    }

    } // switch

    return fail("未知中心线类型");
}

} // namespace SketchCAD
