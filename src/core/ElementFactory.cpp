#include "ElementFactory.h"
#include "SketchModel.h"

namespace SketchCAD {

Element ElementFactory::create(ElementType t, const QString& name, const SketchModel* model) {
    Element e;
    e.type = t;
    e.name = name;
    e.id = 0;
    // 套用模型默认样式（颜色/线宽/线型），保证命令行、工具条、属性面板
    // 三条输入通路画出来的图元样式完全一致。
    if (model) {
        model->applyDefaultStyle(e);
        model->applyActiveLayer(e);   // 归属当前活动图层并继承其属性（默认层"0"不覆盖）
    }
    return e;
}

Element ElementFactory::createLine(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* model) {
    Element e = create(ElementType::Line, name, model);
    e.pts = {a, b};
    return e;
}

Element ElementFactory::createLine(const Vec3& a, const Vec3& b, const SketchModel* model) {
    return createLine(a, b, QStringLiteral("直线"), model);
}

Element ElementFactory::createCircle(const Vec3& center, double radius, const SketchModel* model) {
    Element e = create(ElementType::Circle, QStringLiteral("圆"), model);
    e.pts = {center};
    e.radius = radius;
    return e;
}

Element ElementFactory::createArc(const Vec3& center, double radius, double startDeg, double endDeg, const SketchModel* model) {
    Element e = create(ElementType::Arc, QStringLiteral("圆弧"), model);
    e.pts = {center};
    e.radius = radius;
    e.startAngle = startDeg;
    e.endAngle = endDeg;
    return e;
}

Element ElementFactory::createEllipse(const Vec3& center, double radiusX, double radiusY, double rotationDeg, const SketchModel* model) {
    Element e = create(ElementType::Ellipse, QStringLiteral("椭圆"), model);
    e.pts = {center};
    e.radius = radiusX;      // 长半轴
    e.radiusY = radiusY;     // 短半轴
    e.rotation = rotationDeg;
    return e;
}

Element ElementFactory::createRect(const Vec3& a, const Vec3& b, const SketchModel* model) {
    Element e = create(ElementType::Rectangle, QStringLiteral("矩形"), model);
    e.pts = {a, b};
    return e;
}

Element ElementFactory::createPolygon(const std::vector<Vec3>& pts, const SketchModel* model) {
    Element e = create(ElementType::Polygon, QStringLiteral("多边形"), model);
    e.pts = pts;
    return e;
}

Element ElementFactory::createSpline(const std::vector<Vec3>& pts, const SketchModel* model) {
    Element e = create(ElementType::Spline, QStringLiteral("样条曲线"), model);
    e.pts = pts;
    e.closed = false;  // 样条天然开放
    return e;
}

Element ElementFactory::createPolyline(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed, const SketchModel* model) {
    Element e = create(ElementType::Polyline, QStringLiteral("折线"), model);
    e.pts = pts;
    e.arcFlags = arcFlags;     // 每段类型（false=直线，true=圆弧），size == 段数
    e.closed = closed;          // 用户绘制时是否首尾闭合
    return e;
}

Element ElementFactory::createDimension(DimensionType dt, const SketchModel* model) {
    Element e = create(ElementType::Dimension, QString::fromUtf8(dimensionTypeName(dt)), model);
    e.dtype = dt;
    return e;
}

Element ElementFactory::createText(const Vec3& pos, const QString& text, double heightMm,
                                  const QColor& color) {
    Element e;
    e.type = ElementType::Text;
    e.name = QStringLiteral("文本");
    e.id = 0;
    e.pts = {pos};
    e.text = text;
    e.radius = heightMm;     // 文本高度(mm) 复用 radius 字段
    e.color = color;
    e.lineWidth = 1.0;
    e.lineWidthAuto = true;
    e.lineStyle = LineStyle::Solid;
    return e;
}

Element ElementFactory::createBlockRef(const QString& blockName, const Vec3& insertPos,
                                       double rotationDeg, double sx, double sy,
                                       const SketchModel* model) {
    Element e = create(ElementType::BlockRef, blockName, model);
    e.blockName = blockName;
    e.pts = {insertPos};
    e.rotation = rotationDeg;
    e.scaleX = sx;
    e.scaleY = sy;
    return e;
}

// ---- 引导线 ----

Element ElementFactory::createGuideLine(const Vec3& a, const Vec3& b) {
    Element g;
    g.type = ElementType::Line;
    g.pts = {a, b};
    return g;
}

Element ElementFactory::createGuideCircle(const Vec3& center, double radius) {
    Element g;
    g.type = ElementType::Circle;
    g.pts = {center};
    g.radius = radius;
    return g;
}

} // namespace SketchCAD
