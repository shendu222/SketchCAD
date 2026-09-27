#include "Element.h"
#include "Geometry.h"
#include <cmath>

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
        case ElementType::Hatch:
            return QString("剖面区域 角度 %1° / 间距 %2 mm / 内孔 %3")
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
    QJsonArray pts;
    for (const auto& p : e.pts) pts.append(QJsonArray{p.x, p.y, p.z});
    o["pts"] = pts;
    // 折线段类型标记（仅 Polyline 写入；空数组也写入以便读端判定）
    QJsonArray arcs;
    for (bool a : e.arcFlags) arcs.append(a);
    o["arcFlags"] = arcs;
    // 剖面区域（Hatch）专用字段：内孔 / 豁免类别 / 材料类别
    // （角度复用 rotation、间距复用 radius、线宽复用 lineWidth，已在上方写入）
    QJsonArray holesArr;
    for (const auto& h : e.holes) {
        QJsonArray ring;
        for (const auto& p : h) ring.append(QJsonArray{p.x, p.y, p.z});
        holesArr.append(ring);
    }
    o["holes"] = holesArr;
    o["sectionExempt"] = e.sectionExempt;
    o["sectionMaterial"] = e.sectionMaterial;
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
    e.lineWidth = o.contains("lineWidth") ? o["lineWidth"].toDouble(1.0) : 1.0;
    // 旧存档没有 lineWidthAuto 字段：默认按"手动覆盖"处理，保留老图纸原有的线宽外观
    e.lineWidthAuto = o.contains("lineWidthAuto") ? o["lineWidthAuto"].toBool(false) : false;
    e.lineStyle = o.contains("lineStyle") ? (LineStyle)o["lineStyle"].toInt(0) : LineStyle::Solid;
    e.radius = o["radius"].toDouble();
    e.startAngle = o["startAngle"].toDouble();
    e.endAngle = o["endAngle"].toDouble();
    // 旧存档没有 ellipse 专用字段：默认退化成长轴=短轴=0 的圆
    e.radiusY = o.contains("radiusY") ? o["radiusY"].toDouble() : e.radius;
    e.rotation = o.contains("rotation") ? o["rotation"].toDouble() : 0.0;
    e.ctype = (ConstraintType)o["ctype"].toInt();
    e.dtype = (DimensionType)o["dtype"].toInt();
    // 旧存档没有 closed 字段：那时矩形/多边形一律闭合，默认 true 正好向后兼容
    e.closed = o.contains("closed") ? o["closed"].toBool(true) : true;
    e.splineSubType = o.contains("splineSubType") ? o["splineSubType"].toInt(0) : 0;
    e.text = o["text"].toString();
    e.refA = o["refA"].toInt();
    e.refB = o["refB"].toInt();
    e.standard = o.contains("standard") ? (DimensionStandard)o["standard"].toInt((int)DimensionStandard::GBT) : DimensionStandard::GBT;
    e.tolUpper = o["tolUpper"].toDouble();
    e.tolLower = o["tolLower"].toDouble();
    e.fitCode  = o["fitCode"].toString();
    for (const auto& v : o["pts"].toArray()) {
        QJsonArray a = v.toArray();
        e.pts.push_back(Vec3(a[0].toDouble(), a[1].toDouble(), a[2].toDouble()));
    }
    // 旧存档没有 arcFlags 字段：默认空数组（全部直线段）
    for (const auto& v : o["arcFlags"].toArray()) e.arcFlags.push_back(v.toBool());
    // 剖面区域（Hatch）专用字段（旧存档无键时回落默认：无内孔 / 无豁免 / 通用金属）
    for (const auto& ringVal : o["holes"].toArray()) {
        std::vector<Vec3> ring;
        for (const auto& v : ringVal.toArray()) {
            QJsonArray a = v.toArray();
            ring.push_back(Vec3(a[0].toDouble(), a[1].toDouble(), a[2].toDouble()));
        }
        e.holes.push_back(ring);
    }
    e.sectionExempt   = o.contains("sectionExempt")   ? o["sectionExempt"].toInt(0)   : 0;
    e.sectionMaterial = o.contains("sectionMaterial") ? o["sectionMaterial"].toInt(0) : 0;
    // ---- 块引用（BlockRef）专用字段（旧存档无这些键 → 保持默认）----
    e.blockName = o["blockName"].toString();
    e.scaleX = o.contains("scaleX") ? o["scaleX"].toDouble(1.0) : 1.0;
    e.scaleY = o.contains("scaleY") ? o["scaleY"].toDouble(1.0) : 1.0;
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
