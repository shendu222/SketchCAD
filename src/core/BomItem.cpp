#include "BomItem.h"

namespace SketchCAD {

std::vector<Element> buildBalloon(int seq, const Vec3& anchor,
                                  const QString& layer, double r) {
    std::vector<Element> out;
    out.reserve(2);

    // ① 球标圆：细实线（几何恒为圆，非点画线/中心线）
    Element circle;
    circle.type = ElementType::Circle;
    circle.pts = { anchor };            // 圆心（Circle 仅用 pts[0] + radius，见 Element.h）
    circle.radius = r;
    circle.name = QStringLiteral("序号球标");
    circle.layer = layer;
    circle.color = QColor(26, 29, 40);   // 默认深灰 1A1D28，与全局默认一致
    circle.lineStyle = LineStyle::Solid;
    circle.lineWidthAuto = false;        // 球标线宽固定，不随线型派生
    circle.lineWidth = 0.5;             // 细实线（GB 装配图序号圆为细线）
    circle.byLayer = false;              // 自带样式，不随图层漂移
    out.push_back(circle);

    // ② 编号文字：居中于圆心（textAlign=4 中心）
    Element txt;
    txt.type = ElementType::Text;
    txt.pts = { anchor };                // Text 锚点 = 球标圆心
    txt.radius = r * 0.9;               // 字高：略小于半径，保证圆内不溢出
    txt.text = QString::number(seq);     // 球标编号
    txt.name = QStringLiteral("序号");
    txt.layer = layer;
    txt.color = QColor(26, 29, 40);
    txt.lineStyle = LineStyle::Solid;
    txt.lineWidthAuto = true;
    txt.byLayer = false;
    txt.rotation = 0;
    txt.textAlign = 4;                  // 中心对齐（0=左上 1=中上 2=右上 3=左中 4=中心 …）
    out.push_back(txt);

    return out;
}

} // namespace SketchCAD
