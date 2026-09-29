#include "ElementFactory.h"
#include "SketchModel.h"
#include <algorithm>

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

Element ElementFactory::createMText(const Vec3& pos, const QString& text, double heightMm,
                                  const QColor& color, int textAlign, double widthFactor,
                                  const QString& styleName, double lineSpacing, double mtextWidth) {
    Element e;
    e.type = ElementType::MText;
    e.name = QStringLiteral("多行文字");
    e.id = 0;
    e.pts = {pos};
    e.text = text;
    e.radius = heightMm;            // 字高(mm) 复用 radius 字段
    e.color = color;
    e.lineWidth = 1.0;
    e.lineWidthAuto = true;
    e.lineStyle = LineStyle::Solid;
    // 多行文字专属字段（其余走 Element 默认值）
    e.textAlign    = (textAlign >= 0 && textAlign <= 8) ? textAlign : 1;
    e.widthFactor  = widthFactor > 0 ? widthFactor : 1.0;
    e.styleName    = styleName;
    e.lineSpacing  = lineSpacing > 0 ? lineSpacing : 1.3;
    e.mtextWidth   = mtextWidth > 0 ? mtextWidth : 0.0;
    return e;
}

// 表格（Table）：topLeft=左上角锚点(世界坐标，行向下增长)。
// 列宽均分 30mm、单元格全空；字高/对齐/网格样式走 Element 默认（radius=3.5、textAlign=1 居中）。
// 单元格内容由 TableDialog 填充（预填 initial 时也由对话框回读）。
Element ElementFactory::createTable(const Vec3& topLeft, int rows, int cols, const SketchModel* model) {
    Element e = create(ElementType::Table, QStringLiteral("表格"), model);
    e.pts = {topLeft};
    e.tableRows = std::clamp(rows, 1, 200);
    e.tableCols = std::clamp(cols, 1, 200);
    e.radius = 3.5;                                  // 单元格文字字高(mm)
    e.textAlign = 1;                                 // 单元格文字水平居中
    e.colWidths.assign(e.tableCols, 30.0);
    e.tableCells.assign(size_t(e.tableRows) * size_t(e.tableCols), QString());
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

// 构造线（Xline）：无限延伸，render 时按当前视图边界裁剪。复用 pts[0]=通过点 + rotation=方向角。
Element ElementFactory::createXline(const Vec3& through, double angleDeg, const QString& name, const SketchModel* model) {
    Element e = create(ElementType::Xline, name, model);
    e.pts = {through};
    e.rotation = angleDeg;
    return e;
}

Element ElementFactory::createXline(const Vec3& through, double angleDeg, const SketchModel* model) {
    return createXline(through, angleDeg, QStringLiteral("构造线"), model);
}

// 边界（BOUNDARY / BPOLY）：把用户交互定义的闭合环（外环 + 可选内孔）封装成独立图元。
// 几何直接落在 pts / holes（与 Hatch 同字段），样式落在 rotation / radius / 专用字段，
// 因此既能被 hatchRegionFromElement 直接消费生成剖面区域，又能在属性面板里继续编辑。
// 注意：create() 已套用模型默认样式（颜色/线宽/线型）。边界轮廓默认用实线；剖面线角度/间距
// 用与 Hatch 工厂一致的默认（rotation=45、radius=3），后续可在属性面板或区域管理器覆盖。
Element ElementFactory::createBoundary(const std::vector<Vec3>& pts,
                                     const std::vector<std::vector<Vec3>>& holes,
                                     const SketchModel* model) {
    Element e = create(ElementType::Boundary, QStringLiteral("边界"), model);
    e.pts = pts;
    e.holes = holes;
    e.closed = true;
    e.rotation = 45.0;     // 剖面线方向角（与 Hatch 默认一致）
    e.radius  = 3.0;       // 剖面线间距(mm)（与 Hatch 默认一致）
    e.sectionExempt = 0;
    e.sectionMaterial = 0;
    e.lineStyle = LineStyle::Solid;
    return e;
}

} // namespace SketchCAD
