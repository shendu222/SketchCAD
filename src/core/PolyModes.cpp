#include "PolyModes.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace polygon {

// ============================ 内部工具 ============================

namespace {

// 限制边数在合理范围 [3, 100]
int clampSides(int sides) {
    return std::max(3, std::min(100, sides));
}

} // anonymous namespace

// ============================ 1. 正多边形(内接) ============================

Result regularInscribed(const Vec3& center, const Vec3& vertex, int sides) {
    Result r;
    sides = clampSides(sides);
    const double radius = center.distTo(vertex);
    if (radius < 1e-6) { r.msg = QStringLiteral("半径为 0"); return r; }

    const double startAng = (vertex - center).angleRad();
    const double step = 2.0 * M_PI / sides;

    r.valid = true;
    r.type = ElementType::Polygon;
    r.pts.reserve(sides);
    for (int i = 0; i < sides; ++i) {
        const double ang = startAng + i * step;
        r.pts.emplace_back(center.x + radius * std::cos(ang),
                           center.y + radius * std::sin(ang), 0);
    }
    return r;
}

// ============================ 2. 正多边形(外切) ============================

Result regularCircumscribed(const Vec3& center, const Vec3& edgeMid, int sides) {
    Result r;
    sides = clampSides(sides);
    const double apothem = center.distTo(edgeMid);  // 边心距（圆心到边中点的距离）
    if (apothem < 1e-6) { r.msg = QStringLiteral("边心距为 0"); return r; }

    // 外接圆半径 R = apothem / cos(π/n)
    const double halfStep = M_PI / sides;
    const double radius = apothem / std::cos(halfStep);
    // 顶点角度从边中点角度偏移 π/n（边中点在两条顶点角度的中间）
    const double midAng = (edgeMid - center).angleRad();
    const double startAng = midAng + halfStep;
    const double step = 2.0 * M_PI / sides;

    r.valid = true;
    r.type = ElementType::Polygon;
    r.pts.reserve(sides);
    for (int i = 0; i < sides; ++i) {
        const double ang = startAng + i * step;
        r.pts.emplace_back(center.x + radius * std::cos(ang),
                           center.y + radius * std::sin(ang), 0);
    }
    return r;
}

// ============================ 3. 正多边形(边长) ============================

Result regularEdge(const Vec3& p1, const Vec3& p2, int sides, const Vec3& cur) {
    Result r;
    sides = clampSides(sides);
    const double edgeLen = p1.distTo(p2);
    if (edgeLen < 1e-6) { r.msg = QStringLiteral("边长为 0"); return r; }

    // 外接圆半径 R = edgeLen / (2 * sin(π/n))
    const double halfStep = M_PI / sides;
    const double radius = edgeLen / (2.0 * std::sin(halfStep));
    // 边心距 = R * cos(π/n)
    const double apothem = radius * std::cos(halfStep);

    // 边中点
    const Vec3 mid((p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5, 0);
    // 边方向的垂直方向（两种，选离光标近的）
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double len = std::hypot(dx, dy);
    const Vec3 perp(-dy / len, dx / len, 0);

    // 两个候选圆心（边的两侧）
    const Vec3 c1(mid.x + perp.x * apothem, mid.y + perp.y * apothem, 0);
    const Vec3 c2(mid.x - perp.x * apothem, mid.y - perp.y * apothem, 0);

    // 选择离光标近的圆心
    const bool useC1 = c1.distTo(cur) <= c2.distTo(cur);
    const Vec3 center = useC1 ? c1 : c2;

    // 起始角度 = 从圆心到 p1 的角度
    const double startAng = (p1 - center).angleRad();
    const double step = 2.0 * M_PI / sides;

    r.valid = true;
    r.type = ElementType::Polygon;
    r.pts.reserve(sides);
    for (int i = 0; i < sides; ++i) {
        const double ang = startAng + i * step;
        r.pts.emplace_back(center.x + radius * std::cos(ang),
                           center.y + radius * std::sin(ang), 0);
    }

    // 备选解（另一侧）
    {
        const Vec3 altC = useC1 ? c2 : c1;
        const double altStart = (p1 - altC).angleRad();
        std::vector<Vec3> alt;
        alt.reserve(sides);
        for (int i = 0; i < sides; ++i) {
            const double ang = altStart + i * step;
            alt.emplace_back(altC.x + radius * std::cos(ang),
                             altC.y + radius * std::sin(ang), 0);
        }
        r.altPolys.push_back(std::move(alt));
    }
    return r;
}

// ============================ 4. 矩形 ============================

Result rectangle(const Vec3& p1, const Vec3& p2) {
    Result r;
    if (p1.distTo(p2) < 1e-6) { r.msg = QStringLiteral("对角点重合"); return r; }
    r.valid = true;
    r.type = ElementType::Rectangle;
    r.pts = {p1, p2};  // 矩形仅存对角两点，与现有 finalizeRect 一致
    return r;
}

// ============================ 5. 倒角矩形 ============================

Result rectChamfer(const Vec3& p1, const Vec3& p2, double chamfer) {
    Result r;
    if (p1.distTo(p2) < 1e-6) { r.msg = QStringLiteral("对角点重合"); return r; }
    if (chamfer < 1e-6) {
        // 倒角为 0 退化为普通矩形
        return rectangle(p1, p2);
    }

    const double w = std::abs(p2.x - p1.x);
    const double h = std::abs(p2.y - p1.y);
    // 倒角不能超过短边的一半
    const double c = std::min(chamfer, std::min(w, h) * 0.5);

    // 四个角点（按顺时针）
    const double xmin = std::min(p1.x, p2.x);
    const double ymin = std::min(p1.y, p2.y);
    const double xmax = std::max(p1.x, p2.x);
    const double ymax = std::max(p1.y, p2.y);

    // 每个角用两个点替代，倒角距离 c 沿两条边各偏移
    // 左下角 → 右下角 → 右上角 → 左上角（顺时针）
    r.valid = true;
    r.type = ElementType::Polygon;
    r.pts = {
        Vec3(xmin + c, ymin, 0), Vec3(xmax - c, ymin, 0),   // 底边
        Vec3(xmax, ymin + c, 0), Vec3(xmax, ymax - c, 0),   // 右边
        Vec3(xmax - c, ymax, 0), Vec3(xmin + c, ymax, 0),   // 顶边
        Vec3(xmin, ymax - c, 0), Vec3(xmin, ymin + c, 0)    // 左边
    };
    return r;
}

// ============================ 6. 圆角矩形 ============================

Result rectFillet(const Vec3& p1, const Vec3& p2, double filletRadius) {
    Result r;
    if (p1.distTo(p2) < 1e-6) { r.msg = QStringLiteral("对角点重合"); return r; }
    if (filletRadius < 1e-6) {
        return rectangle(p1, p2);
    }

    const double w = std::abs(p2.x - p1.x);
    const double h = std::abs(p2.y - p1.y);
    // 圆角半径不能超过短边的一半
    const double fr = std::min(filletRadius, std::min(w, h) * 0.5);

    const double xmin = std::min(p1.x, p2.x);
    const double ymin = std::min(p1.y, p2.y);
    const double xmax = std::max(p1.x, p2.x);
    const double ymax = std::max(p1.y, p2.y);

    // 四个圆角的圆心：向内偏移 fr
    // 圆心按顺时针：左下 → 右下 → 右上 → 左上
    struct CornerArc { Vec3 center; double startAng, sweep; };
    const CornerArc arcs[] = {
        {Vec3(xmin + fr, ymin + fr, 0), M_PI,           M_PI * 0.5},        // 左下：180° → 270°
        {Vec3(xmax - fr, ymin + fr, 0), M_PI * 1.5,     M_PI * 0.5},        // 右下：270° → 360°
        {Vec3(xmax - fr, ymax - fr, 0), 0.0,            M_PI * 0.5},        // 右上：0° → 90°
        {Vec3(xmin + fr, ymax - fr, 0), M_PI * 0.5,     M_PI * 0.5}         // 左上：90° → 180°
    };

    r.valid = true;
    r.type = ElementType::Polygon;

    // 每个圆角弧用 8 段近似
    const int arcSegs = 8;
    for (const auto& arc : arcs) {
        for (int i = 0; i < arcSegs; ++i) {
            const double t = static_cast<double>(i) / arcSegs;
            const double ang = arc.startAng + t * arc.sweep;
            r.pts.emplace_back(arc.center.x + fr * std::cos(ang),
                               arc.center.y + fr * std::sin(ang), 0);
        }
    }
    return r;
}

// ============================ 7. 星形 ============================

Result star(const Vec3& center, const Vec3& outerVertex, int points, double innerRatio) {
    Result r;
    points = std::max(3, std::min(50, points));
    const double outerR = center.distTo(outerVertex);
    if (outerR < 1e-6) { r.msg = QStringLiteral("半径为 0"); return r; }
    const double innerR = outerR * innerRatio;

    const double startAng = (outerVertex - center).angleRad();
    const double step = M_PI / points;  // 半角步进（外/内顶点交替）
    const int totalVerts = points * 2;

    r.valid = true;
    r.type = ElementType::Polygon;
    r.pts.reserve(totalVerts);
    for (int i = 0; i < totalVerts; ++i) {
        const double ang = startAng + i * step;
        const double rad = (i % 2 == 0) ? outerR : innerR;
        r.pts.emplace_back(center.x + rad * std::cos(ang),
                           center.y + rad * std::sin(ang), 0);
    }
    return r;
}

// ============================ 主入口 ============================

Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param,
               int sides) {
    (void)refs; (void)elems;  // 多边形模式目前不需要参考图元

    Result r;

    switch (m) {
    case Mode::Free: {
        // 自由多边形：pts 为已拾取的顶点，cur 为光标（预览用）
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) return r;  // 至少 3 个点才能成多边形
        r.valid = true;
        r.type = ElementType::Polygon;
        r.pts = pp;
        return r;
    }
    case Mode::RegularInscribed: {
        // 圆心 + 顶点（光标预览）
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return regularInscribed(pp[0], pp[1], sides);
    }
    case Mode::RegularCircumscribed: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return regularCircumscribed(pp[0], pp[1], sides);
    }
    case Mode::RegularEdge: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return regularEdge(pp[0], pp[1], sides, cur);
    }
    case Mode::Rectangle: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return rectangle(pp[0], pp[1]);
    }
    case Mode::RectChamfer: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        const double chf = (param > 1e-6) ? param : 2.0;  // 默认 2mm
        return rectChamfer(pp[0], pp[1], chf);
    }
    case Mode::RectFillet: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        const double fr = (param > 1e-6) ? param : 3.0;  // 默认 3mm
        return rectFillet(pp[0], pp[1], fr);
    }
    case Mode::Star: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) return r;
        return star(pp[0], pp[1], sides);
    }
    }
    return r;
}

// ============================ 工具映射 ============================

Mode modeFromString(const QString& s) {
    if (s == "poly")                   return Mode::Free;
    if (s == "poly_regular")           return Mode::RegularInscribed;
    if (s == "poly_circumscribed")     return Mode::RegularCircumscribed;
    if (s == "poly_edge")              return Mode::RegularEdge;
    if (s == "rect")                   return Mode::Rectangle;
    if (s == "rect_chamfer")           return Mode::RectChamfer;
    if (s == "rect_fillet")            return Mode::RectFillet;
    if (s == "poly_star")              return Mode::Star;
    return Mode::Free;
}

QString stringFromMode(Mode m) {
    switch (m) {
    case Mode::Free:                  return "poly";
    case Mode::RegularInscribed:      return "poly_regular";
    case Mode::RegularCircumscribed:  return "poly_circumscribed";
    case Mode::RegularEdge:           return "poly_edge";
    case Mode::Rectangle:             return "rect";
    case Mode::RectChamfer:           return "rect_chamfer";
    case Mode::RectFillet:            return "rect_fillet";
    case Mode::Star:                  return "poly_star";
    }
    return "poly";
}

ReqCount requirements(Mode m) {
    switch (m) {
    case Mode::Free:                  return {0, 0, false, true};   // 自由顶点，右键结束
    case Mode::RegularInscribed:      return {0, 1, true, false};   // 圆心 + 光标，用边数
    case Mode::RegularCircumscribed:  return {0, 1, true, false};   // 圆心 + 光标，用边数
    case Mode::RegularEdge:           return {0, 2, true, false};   // 边起终点，用边数
    case Mode::Rectangle:             return {0, 1, false, false};  // 对角两点
    case Mode::RectChamfer:           return {0, 1, true, false};   // 对角两点 + 倒角尺寸
    case Mode::RectFillet:            return {0, 1, true, false};   // 对角两点 + 圆角半径
    case Mode::Star:                  return {0, 1, true, false};   // 圆心 + 外顶点，用角数
    }
    return {0, 0, false, false};
}

std::vector<ElementType> allowedTypes(Mode m) {
    (void)m;
    return {};  // 多边形模式不需要拾取参考图元
}

} // namespace polygon
} // namespace SketchCAD
