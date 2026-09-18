#include "SurfaceRoughness.h"
#include <cmath>

namespace SketchCAD {
namespace {

constexpr double kSqrt3 = 1.73205080756887729353;   // 2·sin60°

// 细实线（GB/T 4457.4：符号线宽约为 h/10，落在细线档）
Element mkSymLine(const Vec3& a, const Vec3& b, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 0.6; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}

Element mkSymText(const Vec3& pos, const QString& t, double h,
                  const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Text;
    e.pts = {pos};
    e.text = t;
    e.radius = h;                    // 文本高度存 radius（程序约定）
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 1.0; e.lineWidthAuto = true;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}

Vec3 rot(const Vec3& p, const Vec3& c, double deg) {
    const double r = deg * 3.14159265358979323846 / 180.0;
    const double cs = std::cos(r), sn = std::sin(r);
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Vec3(c.x + dx * cs - dy * sn, c.y + dx * sn + dy * cs, 0);
}

} // namespace

std::vector<Element> buildRoughnessSymbol(const RoughnessSpec& spec) {
    std::vector<Element> out;
    const double h = spec.textHeight > 0.5 ? spec.textHeight : 3.5;
    if (h <= 0.0) return out;

    const QString name = QStringLiteral("表面粗糙度");
    const Vec3& tip = spec.anchor;                 // 尖端（落在标注面上）

    // 符号各部尺寸（GB/T 131-2006，按字高 h 派生）
    const double H1 = 1.4 * h;                     // 短腿长
    const double H2 = 3.0 * h;                     // 完整符号右腿长（给数值文本留位）
    // 腿与水平成 60°：水平位移 = 长度·cos60° = 长度/2，垂直位移 = 长度·sin60°
    const double dx1 = H1 * 0.5, dy1 = H1 * kSqrt3 * 0.5;
    const double dx2 = H2 * 0.5, dy2 = H2 * kSqrt3 * 0.5;

    const Vec3 leftTop (tip.x - dx1, tip.y + dy1, 0);   // 左腿顶
    const bool  full   = !spec.value.isEmpty();         // 完整符号（注写数值）
    const Vec3 rightTop(tip.x + (full ? dx2 : dx1),
                        tip.y + (full ? dy2 : dy1), 0); // 右腿顶

    auto addLine = [&](const Vec3& a, const Vec3& b) {
        out.push_back(mkSymLine(spec.rotationDeg == 0.0 ? a : rot(a, tip, spec.rotationDeg),
                                spec.rotationDeg == 0.0 ? b : rot(b, tip, spec.rotationDeg),
                                spec.layer, name));
    };

    addLine(tip, leftTop);                            // 左腿
    addLine(tip, rightTop);                           // 右腿
    if (spec.removeMaterial)                          // 去除材料：顶部闭合横线
        addLine(leftTop, Vec3(tip.x + (full ? H1 * 0.5 : dx1),
                              tip.y + (full ? H1 * kSqrt3 * 0.5 : dy1), 0));
    else {                                            // 不去除材料：尖端内接小圆（直径≈H1）
        Element c;
        c.type = ElementType::Circle;
        c.pts = {tip};
        c.radius = H1 * 0.5;
        c.layer = spec.layer; c.lineStyle = LineStyle::Solid;
        c.lineWidth = 0.6; c.lineWidthAuto = false;
        c.color = QColor(26, 29, 40);
        c.name = name;
        out.push_back(std::move(c));
    }

    if (full) {                                       // 完整符号：横线之上注写数值
        const Vec3 barL = spec.rotationDeg == 0.0 ? leftTop : rot(leftTop, tip, spec.rotationDeg);
        const Vec3 barR0(tip.x + dx1, tip.y + dy1, 0);
        const Vec3 barR = spec.rotationDeg == 0.0 ? barR0 : rot(barR0, tip, spec.rotationDeg);
        // 横线（左腿顶 → 右腿上同高处）——若与闭合横线重复则跳过
        if (!spec.removeMaterial) out.push_back(mkSymLine(barL, barR, spec.layer, name));
        // 数值文本：左对齐于左腿顶上方
        Vec3 tp(barL.x, barL.y + h * 0.35, 0);
        if (spec.rotationDeg != 0.0) tp = rot(tp, tip, spec.rotationDeg);
        out.push_back(mkSymText(tp, spec.value, h, spec.layer, name));
    }

    return out;
}

} // namespace SketchCAD
