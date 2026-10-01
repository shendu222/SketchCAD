#include "Element.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {

QString Element::sizeDesc() const {
    switch (type) {
        case ElementType::Line:
            return pts.size() >= 2
                ? QString("长度 %1 mm").arg(pts[0].distTo(pts[1]), 0, 'f', 2)
                : QString();
        case ElementType::Circle:
        case ElementType::Arc:
            return QString("半径 %1 mm").arg(radius, 0, 'f', 2);
        case ElementType::Ellipse:
            return QString("长轴 %1 / 短轴 %2 mm").arg(radius, 0, 'f', 2).arg(radiusY, 0, 'f', 2);
        case ElementType::Rectangle:
            if (pts.size() >= 2) {
                double w = std::fabs(pts[1].x - pts[0].x);
                double h = std::fabs(pts[1].y - pts[0].y);
                return QString("%1 × %2 mm").arg(w, 0, 'f', 2).arg(h, 0, 'f', 2);
            }
            return QString();
        case ElementType::Polygon:
        case ElementType::Spline: {
            if (type == ElementType::Polygon) {
                // 多边形：顶点数 + 总长（边为直线段，chord 和即精确值）
                return QString("顶点数 %1 / 总长 %2 mm")
                       .arg(pts.size())
                       .arg(geo::polylineLength(pts, true), 0, 'f', 2);
            }
            // 样条：顶点数 + 曲线总长（flatten 离散，6 种子类型统一与渲染/命中同源）
            double slen = 0;
            if (pts.size() >= 2) {
                for (const auto& s : geo::flatten(*this)) slen += s.a.distTo(s.b);
            }
            return QString("顶点数 %1 / 总长 %2 mm").arg(pts.size()).arg(slen, 0, 'f', 2);
        }
        case ElementType::Polyline: {
            // 折线：节点数 + 段数 + 弧段数 + 精确总长
            //   （弧段 = |sweep|·r 外接圆弧长，与渲染采样同源；取代旧「弦长×1.05」粗估）
            const int nNodes = (int)pts.size();
            const int nSegs  = nNodes - (closed ? 0 : 1);
            int nArcs = 0;
            for (bool a : arcFlags) if (a) ++nArcs;
            const double total = geo::polylineArcLength(pts, arcFlags, closed);
            return QString("节点 %1 / 段 %2 / 弧段 %3 / 总长 %4 mm")
                   .arg(nNodes).arg(nSegs).arg(nArcs).arg(total, 0, 'f', 2);
        }
        case ElementType::Constraint:
            return constraintTypeName(ctype);
        case ElementType::Dimension:
            return text.isEmpty() ? QStringLiteral("标注") : text;
        case ElementType::Text:
            return QString("文本 高度 %1 mm").arg(radius, 0, 'f', 2);
        case ElementType::MText: {
            const int nLines = (text.isEmpty() ? 0 : text.split('\n').size());
            return QString("多行文字 高度 %1 mm / %2 行 / 对齐 %3 / 样式 %4")
                   .arg(radius, 0, 'f', 2).arg(nLines)
                   .arg(QString::fromUtf8(mtextAlignName(textAlign)))
                   .arg(styleName.isEmpty() ? QStringLiteral("默认") : styleName);
        }
        case ElementType::Hatch:
            return QString("剖面区域 角度 %1° / 间距 %2 mm / 内孔 %3")
                   .arg(rotation, 0, 'f', 1).arg(radius, 0, 'f', 2).arg(holes.size());
        case ElementType::Table: {
            // 表格：行列规模 + 总宽×总高 + 非空单元格数
            int filled = 0;
            for (const QString& s : tableCells) if (!s.trimmed().isEmpty()) ++filled;
            return QString("表格 %1×%2（%3 × %4 mm）/ 已填 %5 格")
                   .arg(tableRows).arg(tableCols)
                   .arg(tableWidthMm(), 0, 'f', 1)
                   .arg(tableRows * rowHeight, 0, 'f', 1)
                   .arg(filled);
        }
        case ElementType::Boundary:
            return QString("边界 角度 %1° / 间距 %2 mm / 内孔 %3")
                   .arg(rotation, 0, 'f', 1).arg(radius, 0, 'f', 2).arg(holes.size());
        case ElementType::BlockRef:
            // 块引用：块名 + 插入点 + 缩放 + 属性数（几何无副本，展开见 expandBlockRef）
            return QString("块 %1 @(%2,%3) 缩放 %4×%5%6")
                   .arg(blockName)
                   .arg(pts.empty() ? 0.0 : pts[0].x, 0, 'f', 1)
                   .arg(pts.empty() ? 0.0 : pts[0].y, 0, 'f', 1)
                   .arg(scaleX, 0, 'f', 2).arg(scaleY, 0, 'f', 2)
                   .arg(blockAttrs.empty()
                            ? QString()
                            : QString(" / 属性 %1").arg(blockAttrs.size()));
        case ElementType::Xline:
            // 构造线：无限延伸，仅显示通过点与方向角（不由长度统计，参与 fit 取景时排除）。
            return QString("构造线 通过(%1,%2) 角度 %3°")
                   .arg(pts.empty() ? 0.0 : pts[0].x, 0, 'f', 1)
                   .arg(pts.empty() ? 0.0 : pts[0].y, 0, 'f', 1)
                   .arg(rotation, 0, 'f', 1);
        case ElementType::Point:
            return QString("点 @(%1,%2)").arg(pts.empty() ? 0.0 : pts[0].x, 0, 'f', 1)
                                          .arg(pts.empty() ? 0.0 : pts[0].y, 0, 'f', 1);
    }
    return QString();
}

// ============================================================================
// 图元序列化（与 MainWindow 原实现字段一一对应，并新增块引用字段）
// ============================================================================
QJsonObject elementToJson(const Element& e) {
    QJsonObject o;
    o["id"] = e.id; o["type"] = (int)e.type; o["name"] = e.name; o["layer"] = e.layer;
    o["color"] = e.color.name(); o["lineWidth"] = e.lineWidth; o["lineWidthAuto"] = e.lineWidthAuto; o["lineStyle"] = (int)e.lineStyle; o["radius"] = e.radius;
    o["startAngle"] = e.startAngle; o["endAngle"] = e.endAngle;
    o["radiusY"] = e.radiusY;
    o["rotation"] = e.rotation;
    o["ctype"] = (int)e.ctype; o["dtype"] = (int)e.dtype;
    o["closed"] = e.closed;                 // 剪切过的矩形/多边形是开放折线，必须存盘
    o["splineSubType"] = e.splineSubType;   // 样条子类型（0=Catmull-Rom, 1=抛物线, 2=双曲线）
    o["text"] = e.text; o["refA"] = e.refA; o["refB"] = e.refB;
    o["standard"] = (int)e.standard; o["tolUpper"] = e.tolUpper; o["tolLower"] = e.tolLower; o["fitCode"] = e.fitCode;
    // ---- 尺寸驱动 / 参数化建模字段（Request M 新增；旧存档无键 → 读端回落默认，无副作用）----
    o["dimValue"]  = e.dimValue;
    o["dimDriven"] = e.dimDriven;
    o["dimParam"]  = e.dimParam;
    o["dimLinked"] = e.dimLinked;
    if (!e.groupName.isEmpty()) o["groupName"] = e.groupName;   // 编组归属（空串不写盘，旧档缺键回落未编组）
    o["pts"] = json::writeVec3Array(e.pts);
    // 折线段类型标记（仅 Polyline 写入；空数组也写入以便读端判定）
    QJsonArray arcs;
    for (bool a : e.arcFlags) arcs.append(a);
    o["arcFlags"] = arcs;
    // 剖面区域（Hatch）专用字段：内孔 / 豁免类别 / 材料类别
    // （角度复用 rotation、间距复用 radius、线宽复用 lineWidth，已在上方写入）
    o["holes"] = json::writeVec3Rings(e.holes);
    o["sectionExempt"] = e.sectionExempt;
    o["sectionMaterial"] = e.sectionMaterial;
    // ---- 多行文字（MText）专用字段（旧存档无这些键 → 读端回落默认，无副作用）----
    if (e.type == ElementType::MText) {
        o["widthFactor"] = e.widthFactor;
        o["textAlign"]   = e.textAlign;
        o["styleName"]   = e.styleName;
        o["lineSpacing"] = e.lineSpacing;
        o["mtextWidth"]  = e.mtextWidth;
    }
    // ---- 表格（Table）专用字段（旧存档无这些键 → 读端回落默认，无副作用）----
    if (e.type == ElementType::Table) {
        o["tableRows"] = e.tableRows;
        o["tableCols"] = e.tableCols;
        o["rowHeight"] = e.rowHeight;
        QJsonArray cw;
        for (double w : e.colWidths) cw.append(w);
        o["colWidths"] = cw;
        QJsonArray cells;
        for (const QString& s : e.tableCells) cells.append(s);
        o["tableCells"] = cells;
    }
    // ---- 块引用（BlockRef）专用字段（旧存档无这些键 → 读端回落默认，无副作用）----
    if (e.type == ElementType::BlockRef) {
        o["blockName"] = e.blockName;
        o["scaleX"] = e.scaleX;
        o["scaleY"] = e.scaleY;
        QJsonArray attrs;
        for (const auto& a : e.blockAttrs) {
            QJsonObject ao; ao["tag"] = a.tag; ao["value"] = a.value;
            attrs.append(ao);
        }
        o["blockAttrs"] = attrs;
        QJsonArray dyns;
        for (const auto& d : e.dynValues) {
            QJsonObject dobj; dobj["name"] = d.name; dobj["value"] = d.value; dobj["state"] = d.state;
            dyns.append(dobj);
        }
        o["dynValues"] = dyns;
    }
    return o;
}

Element elementFromJson(const QJsonObject& o) {
    Element e;
    e.id = o["id"].toInt();
    e.type = (ElementType)o["type"].toInt();
    e.name = o["name"].toString();
    e.layer = o["layer"].toString();
    e.color = QColor(o["color"].toString());
    // 兼容旧存档：没有 lineWidth 字段时回落到 1px，而不是 0（0 在 Qt 里是"最细可见线"，
    // 语义上可用，但会让属性面板显示成 0，容易被误解为"无线宽"）
    e.lineWidth = json::getDouble(o, "lineWidth", 1.0);
    // 旧存档没有 lineWidthAuto 字段：默认按"手动覆盖"处理，保留老图纸原有的线宽外观
    e.lineWidthAuto = json::getBool(o, "lineWidthAuto", false);
    e.lineStyle = (LineStyle)json::getInt(o, "lineStyle", (int)LineStyle::Solid);
    e.radius = o["radius"].toDouble();
    e.startAngle = o["startAngle"].toDouble();
    e.endAngle = o["endAngle"].toDouble();
    // 旧存档没有 ellipse 专用字段：默认退化成长轴=短轴=0 的圆
    e.radiusY = json::getDouble(o, "radiusY", e.radius);
    e.rotation = json::getDouble(o, "rotation", 0.0);
    e.ctype = (ConstraintType)o["ctype"].toInt();
    e.dtype = (DimensionType)o["dtype"].toInt();
    // 旧存档没有 closed 字段：那时矩形/多边形一律闭合，默认 true 正好向后兼容
    e.closed = json::getBool(o, "closed", true);
    e.splineSubType = json::getInt(o, "splineSubType", 0);
    e.text = o["text"].toString();
    e.refA = o["refA"].toInt();
    e.refB = o["refB"].toInt();
    e.standard = (DimensionStandard)json::getInt(o, "standard", (int)DimensionStandard::GBT);
    e.tolUpper = o["tolUpper"].toDouble();
    e.tolLower = o["tolLower"].toDouble();
    e.fitCode  = o["fitCode"].toString();
    // ---- 尺寸驱动 / 参数化建模字段（Request M 新增；旧存档缺键 → 回落默认，向后兼容）----
    e.dimValue  = json::getDouble(o, "dimValue", 0.0);
    e.dimDriven = json::getBool(o, "dimDriven", false);
    e.dimParam  = json::getString(o, "dimParam", QString());
    e.dimLinked = json::getBool(o, "dimLinked", false);
    e.groupName = json::getString(o, "groupName", QString());
    e.pts = json::readVec3Array(o, "pts");
    // 旧存档没有 arcFlags 字段：默认空数组（全部直线段）
    for (const auto& v : o["arcFlags"].toArray()) e.arcFlags.push_back(v.toBool());
    // 剖面区域（Hatch）专用字段（旧存档无键时回落默认：无内孔 / 无豁免 / 通用金属）
    e.holes = json::readVec3Rings(o, "holes");
    e.sectionExempt   = json::getInt(o, "sectionExempt", 0);
    e.sectionMaterial = json::getInt(o, "sectionMaterial", 0);
    // ---- 多行文字（MText）专用字段（旧存档缺键 → 回落默认，向后兼容）----
    e.widthFactor = json::getDouble(o, "widthFactor", 1.0);
    e.textAlign   = json::getInt(o, "textAlign", 1);
    e.styleName   = json::getString(o, "styleName", QString());
    e.lineSpacing = json::getDouble(o, "lineSpacing", 1.3);
    e.mtextWidth  = json::getDouble(o, "mtextWidth", 0.0);
    // ---- 表格（Table）专用字段（旧存档缺键 → 回落默认，向后兼容）----
    // 行列数钳到 [1,200]：防损坏数据把渲染/导出撑爆
    e.tableRows = std::clamp(json::getInt(o, "tableRows", 3), 1, 200);
    e.tableCols = std::clamp(json::getInt(o, "tableCols", 4), 1, 200);
    e.rowHeight = json::getDouble(o, "rowHeight", 8.0);
    if (e.rowHeight < 0.5) e.rowHeight = 8.0;
    for (const auto& v : o["colWidths"].toArray()) e.colWidths.push_back(v.toDouble(30.0));
    for (const auto& v : o["tableCells"].toArray()) e.tableCells.push_back(v.toString());
    // ---- 块引用（BlockRef）专用字段（旧存档无这些键 → 保持默认）----
    e.blockName = o["blockName"].toString();
    e.scaleX = json::getDouble(o, "scaleX", 1.0);
    e.scaleY = json::getDouble(o, "scaleY", 1.0);
    for (const auto& v : o["blockAttrs"].toArray()) {
        const QJsonObject ao = v.toObject();
        e.blockAttrs.push_back(BlockAttrValue{ao["tag"].toString(), ao["value"].toString()});
    }
    for (const auto& v : o["dynValues"].toArray()) {
        const QJsonObject dobj = v.toObject();
        BlockDynValue dv;
        dv.name  = dobj["name"].toString();
        dv.value = dobj["value"].toDouble(0.0);
        dv.state = dobj["state"].toString();
        e.dynValues.push_back(dv);
    }
    return e;
}

} // namespace SketchCAD
