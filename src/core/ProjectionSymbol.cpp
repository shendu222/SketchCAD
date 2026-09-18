#include "ProjectionSymbol.h"

namespace SketchCAD {

std::vector<Element> buildProjectionSymbol(const Vec3& center, double r,
                                           ProjectionMethod method) {
    std::vector<Element> out;
    if (r <= 0.0) return out;

    const bool first = (method == ProjectionMethod::FirstAngle);
    const QString tag = first ? QStringLiteral("投影符号:第一角")
                              : QStringLiteral("投影符号:第三角");

    auto line = [&](double x1, double y1, double x2, double y2) {
        Element e;
        e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        e.layer = QStringLiteral("图框");
        e.lineStyle = LineStyle::Solid;
        e.lineWidth = 0.5; e.lineWidthAuto = false;   // 识别符号用细线（≥0.5mm）
        e.color = QColor(26, 29, 40);
        e.name = tag;
        out.push_back(std::move(e));
    };

    // 外圆（识别符号外框）
    {
        Element e;
        e.type = ElementType::Circle;
        e.pts = {center};
        e.radius = r;
        e.layer = QStringLiteral("图框");
        e.lineStyle = LineStyle::Solid;
        e.lineWidth = 0.5; e.lineWidthAuto = false;
        e.color = QColor(26, 29, 40);
        e.name = tag;
        out.push_back(std::move(e));
    }

    // 圆台：尖端方向区分投影法（first=朝下，third=朝上）
    const double apexY  = center.y + (first ? -r * 0.55 : r * 0.55);
    const double baseY  = center.y + (first ?  r * 0.55 : -r * 0.55);
    const double halfB = r * 0.42;
    line(center.x - halfB, baseY, center.x,            apexY);   // 左母线
    line(center.x + halfB, baseY, center.x,            apexY);   // 右母线
    line(center.x - halfB, baseY, center.x + halfB,    baseY);   // 底面

    // 方法标注文本（置于圆下方）
    {
        Element e;
        e.type = ElementType::Text;
        e.pts = {Vec3(center.x, center.y - r - 3.0, 0)};
        e.text = first ? QStringLiteral("第一角画法") : QStringLiteral("第三角画法");
        e.radius = 2.5;
        e.layer = QStringLiteral("图框");
        e.color = QColor(26, 29, 40);
        e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
        e.name = tag;
        out.push_back(std::move(e));
    }

    return out;
}

} // namespace SketchCAD
