#include "SectionCutSymbol.h"
#include <cmath>

namespace SketchCAD {
namespace {

constexpr double kPi = 3.14159265358979323846;

Element mkLine(const Vec3& a, const Vec3& b, double w, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = w; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}

Element mkText(const Vec3& pos, const QString& t, double h, const QString& layer, const QString& name) {
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

// 剖切位置粗短线（粗实线 ≥0.7mm）
Element mkCutLine(const Vec3& a, const Vec3& b, const QString& layer, const QString& name) {
    return mkLine(a, b, 2.0, layer, name);
}

} // namespace

std::vector<Element> buildCutSymbolEnd(const CutSymbolSpec& s) {
    std::vector<Element> out;
    if (s.lineLen <= 0.0 || s.arrowLen <= 0.0) return out;

    const QString name = QStringLiteral("剖切符号");
    const Vec3& c = s.markPoint;

    // 剖切位置线方向 = 垂直于剖视方向；箭头方向 = viewDir
    // Up/Down → 位置线水平；Left/Right → 位置线竖直
    const bool vertArrow = (s.viewDir == CutViewDir::Up || s.viewDir == CutViewDir::Down);
    const double hl = s.lineLen * 0.5;
    const Vec3 pA = vertArrow ? Vec3(c.x - hl, c.y, 0) : Vec3(c.x, c.y - hl, 0);
    const Vec3 pB = vertArrow ? Vec3(c.x + hl, c.y, 0) : Vec3(c.x, c.y + hl, 0);
    out.push_back(mkCutLine(pA, pB, s.layer, name));

    // 箭杆：自位置线中点指向 viewDir
    double ax = 0, ay = 0;
    switch (s.viewDir) {
        case CutViewDir::Up:    ax =  0; ay =  1; break;
        case CutViewDir::Down:  ax =  0; ay = -1; break;
        case CutViewDir::Left:  ax = -1; ay =  0; break;
        case CutViewDir::Right: ax =  1; ay =  0; break;
    }
    const Vec3 tail = c;
    const Vec3 head(c.x + ax * s.arrowLen, c.y + ay * s.arrowLen, 0);
    out.push_back(mkLine(tail, head, 0.7, s.layer, name));      // 箭杆（细实线）

    // 箭头（V 形两翼，半张角 15°）
    const double wing = s.arrowLen * 0.30;                       // 翼长
    const double aRad15 = 15.0 * kPi / 180.0;
    const double bx = -ax, by = -ay;                             // 反向
    // 两翼方向 = 反向旋转 ±15°
    auto wingTip = [&](double sign) {
        const double cs = std::cos(sign * aRad15), sn = std::sin(sign * aRad15);
        const double dx = bx * cs - by * sn, dy = bx * sn + by * cs;
        return Vec3(head.x + dx * wing, head.y + dy * wing, 0);
    };
    out.push_back(mkLine(head, wingTip(+1.0), 0.7, s.layer, name));
    out.push_back(mkLine(head, wingTip(-1.0), 0.7, s.layer, name));

    // 字母：注写在箭杆外侧旁（沿位置线方向偏移，避开图形）
    const double off = hl + s.textHeight * 0.9;
    Vec3 tp = vertArrow ? Vec3(c.x + off, c.y + ay * s.arrowLen * 0.5, 0)
                        : Vec3(c.x + ax * s.arrowLen * 0.5, c.y + off, 0);
    // 文本锚点按左对齐基线：竖直位置线时右移半个字宽
    if (!vertArrow) tp.x -= s.textHeight * 0.35;
    out.push_back(mkText(tp, s.letter, s.textHeight, s.layer, name));

    return out;
}

std::vector<Element> buildCutAnnotation(const Vec3& start, const Vec3& end,
                                        CutViewDir dir, const QString& letter,
                                        double textHeight, const QString& layer) {
    CutSymbolSpec s;
    s.viewDir = dir;
    s.letter = letter;
    s.textHeight = textHeight;
    s.layer = layer;

    std::vector<Element> out = buildCutSymbolEnd([&]{
        CutSymbolSpec a = s; a.markPoint = start; return a; }());
    const auto b = buildCutSymbolEnd([&]{
        CutSymbolSpec e = s; e.markPoint = end; return e; }());
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

Element buildSectionViewLabel(const Vec3& center, const QString& letter,
                              double textHeight, const QString& layer) {
    return mkText(center, letter + QString::fromUtf8("\xE2\x80\x94") + letter,   // A—A（em dash U+2014）
                  textHeight, layer, QStringLiteral("剖视图名"));
}

} // namespace SketchCAD
