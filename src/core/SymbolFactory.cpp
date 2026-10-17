#include "SymbolFactory.h"
#include <cmath>
#include "Vec.h"

namespace SketchCAD {
namespace SymbolFactory {
namespace {

constexpr double kPi = 3.14159265358979323846;

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
Element mkSymText(const Vec3& pos, const QString& t, double h, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Text;
    e.pts = {pos};
    e.text = t;
    e.radius = h;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 1.0; e.lineWidthAuto = true;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}
Element mkSymCircle(const Vec3& c, double r, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Circle;
    e.pts = {c};
    e.radius = r;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 0.6; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}
// 在 tip 处沿 dir(单位向量) 反向画 V 形箭头两翼（半张角 15°）
void addArrowHead(std::vector<Element>& out, const Vec3& tip, const Vec3& dir,
                 double wing, const QString& layer, const QString& name) {
    const double a = 15.0 * kPi / 180.0;
    const Vec3 back(-dir.x, -dir.y, 0);
    auto wingTip = [&](double sign) {
        const double cs = std::cos(sign * a), sn = std::sin(sign * a);
        const double dx = back.x * cs - back.y * sn;
        const double dy = back.x * sn + back.y * cs;
        return Vec3(tip.x + dx * wing, tip.y + dy * wing, 0);
    };
    out.push_back(mkSymLine(tip, wingTip(+1.0), layer, name));
    out.push_back(mkSymLine(tip, wingTip(-1.0), layer, name));
}
Vec3 rotAround(const Vec3& p, const Vec3& c, double deg) {
    const double r = deg * kPi / 180.0;
    const double cs = std::cos(r), sn = std::sin(r);
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Vec3(c.x + dx * cs - dy * sn, c.y + dx * sn + dy * cs, 0);
}

// ---- 基准符号：上尖三角 + 基准字母 ----
std::vector<Element> buildDatum(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("基准符号");
    const double h = 6.0, w = 4.0;
    const Vec3 apex (a.x,        a.y + h, 0);
    const Vec3 bl   (a.x - w,    a.y,     0);
    const Vec3 br   (a.x + w,    a.y,     0);
    std::vector<Element> out;
    out.push_back(mkSymLine(apex, bl, layer, name));
    out.push_back(mkSymLine(apex, br, layer, name));
    out.push_back(mkSymLine(bl,   br, layer, name));
    const QString letter = p.value(QStringLiteral("letter"), QStringLiteral("A")).toString();
    out.push_back(mkSymText(Vec3(a.x, a.y + h + 1.5, 0), letter, 3.5, layer, name));
    return out;
}

// ---- 焊接符号：参考线 + 箭头 + 接头形状(fillet/v/square) ----
std::vector<Element> buildWeld(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("焊接符号");
    const double L = 15.0;
    const QString type = p.value(QStringLiteral("type"), QStringLiteral("fillet")).toString();
    std::vector<Element> out;
    out.push_back(mkSymLine(Vec3(a.x - L, a.y, 0), Vec3(a.x + L, a.y, 0), layer, name));   // 参考线
    // 箭头（自右端指向下）
    const Vec3 tip(a.x + L, a.y - 8.0, 0);
    out.push_back(mkSymLine(Vec3(a.x + L, a.y, 0), tip, layer, name));
    addArrowHead(out, tip, Vec3(0, -1, 0), 2.5, layer, name);
    // 接头形状（落在参考线上方居中）
    const double s = 3.0;
    if (type == QStringLiteral("v")) {
        out.push_back(mkSymLine(Vec3(a.x - s, a.y, 0), Vec3(a.x, a.y - s * 1.4, 0), layer, name));
        out.push_back(mkSymLine(Vec3(a.x, a.y - s * 1.4, 0), Vec3(a.x + s, a.y, 0), layer, name));
    } else if (type == QStringLiteral("square")) {
        out.push_back(mkSymLine(Vec3(a.x - s, a.y, 0),       Vec3(a.x + s, a.y, 0),       layer, name));
        out.push_back(mkSymLine(Vec3(a.x + s, a.y, 0),       Vec3(a.x + s, a.y + 2 * s, 0), layer, name));
        out.push_back(mkSymLine(Vec3(a.x + s, a.y + 2 * s, 0), Vec3(a.x - s, a.y + 2 * s, 0), layer, name));
        out.push_back(mkSymLine(Vec3(a.x - s, a.y + 2 * s, 0), Vec3(a.x - s, a.y, 0),      layer, name));
    } else { // fillet（默认）：底边在参考线上的正三角
        out.push_back(mkSymLine(Vec3(a.x - s, a.y, 0), Vec3(a.x, a.y + s * 1.4, 0), layer, name));
        out.push_back(mkSymLine(Vec3(a.x, a.y + s * 1.4, 0), Vec3(a.x + s, a.y, 0), layer, name));
        out.push_back(mkSymLine(Vec3(a.x - s, a.y, 0), Vec3(a.x + s, a.y, 0), layer, name));
    }
    return out;
}

// ---- 锥度/斜度符号：直角斜三角形 + 比值文本 ----
std::vector<Element> buildTaper(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("锥度符号");
    const double w = 5.0, h = 4.0;
    std::vector<Element> out;
    out.push_back(mkSymLine(Vec3(a.x - w, a.y, 0), Vec3(a.x + w, a.y, 0), layer, name));      // 底边（水平）
    out.push_back(mkSymLine(Vec3(a.x + w, a.y, 0), Vec3(a.x - w, a.y + h, 0), layer, name));  // 斜边
    out.push_back(mkSymLine(Vec3(a.x - w, a.y + h, 0), Vec3(a.x - w, a.y, 0), layer, name));  // 竖边
    const QString ratio = p.value(QStringLiteral("ratio"), QStringLiteral("1:50")).toString();
    out.push_back(mkSymText(Vec3(a.x - w, a.y + h + 1.5, 0), ratio, 3.5, layer, name));
    return out;
}

// ---- 中心孔符号：小圆 + 引出线（可选旁注）----
std::vector<Element> buildCenterHole(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("中心孔符号");
    const double r = p.value(QStringLiteral("r"), 2.5).toDouble();
    std::vector<Element> out;
    out.push_back(mkSymCircle(a, r, layer, name));
    out.push_back(mkSymLine(Vec3(a.x + r, a.y, 0), Vec3(a.x + r + 8.0, a.y, 0), layer, name));  // 引出线
    const QString label = p.value(QStringLiteral("label"), QString()).toString();
    if (!label.isEmpty())
        out.push_back(mkSymText(Vec3(a.x + r + 9.0, a.y + 1.2, 0), label, 3.0, layer, name));
    return out;
}

// ---- 标高符号：上尖等边三角形 + 顶部水平线 + 标高值 ----
std::vector<Element> buildElevation(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("标高符号");
    const double w = 4.0, h = w * std::sqrt(3.0);   // 等边
    const Vec3 apex(a.x, a.y + h, 0);
    const Vec3 bl(a.x - w, a.y, 0);
    const Vec3 br(a.x + w, a.y, 0);
    std::vector<Element> out;
    out.push_back(mkSymLine(apex, bl, layer, name));
    out.push_back(mkSymLine(apex, br, layer, name));
    out.push_back(mkSymLine(bl, br, layer, name));
    out.push_back(mkSymLine(Vec3(a.x - w, a.y + h, 0), Vec3(a.x + w, a.y + h, 0), layer, name));  // 顶部水平线
    const QString value = p.value(QStringLiteral("value"), QStringLiteral("±0.000")).toString();
    out.push_back(mkSymText(Vec3(a.x, a.y + h + 1.5, 0), value, 3.5, layer, name));
    return out;
}

// ---- 旋转符号：圆 + 顶部箭头 + 角度文本 ----
std::vector<Element> buildRotation(const Vec3& a, const QVariantMap& p, const QString& layer) {
    const QString name = QStringLiteral("旋转符号");
    const double r = p.value(QStringLiteral("r"), 6.0).toDouble();
    std::vector<Element> out;
    out.push_back(mkSymCircle(a, r, layer, name));
    const Vec3 tip(a.x, a.y + r, 0);                 // 圆顶
    addArrowHead(out, tip, Vec3(0, 1, 0), 2.5, layer, name);   // 逆时针旋转示意
    const QString angle = p.value(QStringLiteral("angle"), QStringLiteral("360°")).toString();
    out.push_back(mkSymText(Vec3(a.x + r + 1.5, a.y + 1.2, 0), angle, 3.5, layer, name));
    return out;
}

} // namespace

std::vector<Element> mkSymbol(const QString& name, const Vec3& anchor, const QVariantMap& params) {
    const QString layer = params.value(QStringLiteral("layer"), QStringLiteral("标注")).toString();
    std::vector<Element> out;
    if (name == QStringLiteral("datum"))         out = buildDatum(anchor, params, layer);
    else if (name == QStringLiteral("weld"))     out = buildWeld(anchor, params, layer);
    else if (name == QStringLiteral("taper"))    out = buildTaper(anchor, params, layer);
    else if (name == QStringLiteral("centerhole")) out = buildCenterHole(anchor, params, layer);
    else if (name == QStringLiteral("elevation")) out = buildElevation(anchor, params, layer);
    else if (name == QStringLiteral("rotation")) out = buildRotation(anchor, params, layer);
    else return {};   // 未知 name → 退化空

    // 整体旋转（可选）：绕 anchor 旋转所有控制点
    const double rot = params.value(QStringLiteral("rot"), 0.0).toDouble();
    if (rot != 0.0) {
        for (auto& e : out) {
            if (e.pts.empty()) continue;
            for (auto& pt : e.pts) pt = rotAround(pt, anchor, rot);
            if (e.type == ElementType::Circle) {}  // 圆旋转中心不变
        }
    }
    return out;
}

} // namespace SymbolFactory
} // namespace SketchCAD
