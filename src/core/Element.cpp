#include "Element.h"
#include "Geometry.h"
#include "Gdt.h"   // FcfSymbol 名称/越界防护（sizeDesc 用；Gdt.h 只依赖 Element.h，无循环）
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
        case ElementType::MLeader:
            // 多重引线：附着点 + 行数 + 基线
            return QString("多重引线 @(%1,%2) / %3 行%4")
                   .arg(pts.empty() ? 0.0 : pts[0].x, 0, 'f', 1)
                   .arg(pts.empty() ? 0.0 : pts[0].y, 0, 'f', 1)
                   .arg(text.count(QLatin1Char('\n')) + 1)
                   .arg(mleaderLanding > 1e-9 ? QString(" / 基线 %1mm").arg(mleaderLanding, 0, 'f', 1) : QString());
        case ElementType::Fcf: {
            // 形位公差框格：符号 + 公差 + 基准（fcfDatums 是 vector<QString>，需手工拼接）
            const char* sym = fcfSymbolValid(fcfSymbol) ? fcfSymbolName((FcfSymbol)fcfSymbol) : "未知";
            QString datums;
            for (const auto& d : fcfDatums) { if (!datums.isEmpty()) datums += QLatin1Char('-'); datums += d; }
            return QString("形位公差 %1 / %2 / 基准 %3")
                   .arg(QString::fromUtf8(sym))
                   .arg(fcfTolerance.isEmpty() ? QString("—") : fcfTolerance)
                   .arg(datums.isEmpty() ? QString("—") : datums);
        }
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
// 全字段相等比较（撤销差量化 diffElements 用）
// 与 elementToJson 的字段集一一对应：序列化是全字段的，相等比较也必须是全字段的，
// 否则会出现「JSON 不同但 == 相等」或「JSON 相同但 == 不等」的漂移。
// 新增 Element 字段时：elementToJson 与本函数必须同步补齐（两处同源清单）。
// ============================================================================
bool Element::operator==(const Element& o) const {
    if (id != o.id || type != o.type) return false;
    if (name != o.name || layer != o.layer || color != o.color) return false;
    if (lineWidth != o.lineWidth || lineStyle != o.lineStyle || lineWidthAuto != o.lineWidthAuto) return false;
    if (pts != o.pts) return false;
    if (radius != o.radius || startAngle != o.startAngle || endAngle != o.endAngle) return false;
    if (radiusY != o.radiusY || rotation != o.rotation || closed != o.closed) return false;
    if (splineSubType != o.splineSubType || ctype != o.ctype || dtype != o.dtype) return false;
    if (standard != o.standard) return false;
    if (text != o.text || tolUpper != o.tolUpper || tolLower != o.tolLower) return false;
    if (dimTextHeight != o.dimTextHeight || fitCode != o.fitCode || symbolLibrary != o.symbolLibrary) return false;
    if (refA != o.refA || refB != o.refB || visible != o.visible || byLayer != o.byLayer) return false;
    if (dimValue != o.dimValue || dimDriven != o.dimDriven) return false;
    if (dimParam != o.dimParam || dimLinked != o.dimLinked) return false;
    if (widthFactor != o.widthFactor || textAlign != o.textAlign) return false;
    if (styleName != o.styleName || lineSpacing != o.lineSpacing || mtextWidth != o.mtextWidth) return false;
    if (groupName != o.groupName) return false;
    if (tableRows != o.tableRows || tableCols != o.tableCols || rowHeight != o.rowHeight) return false;
    if (colWidths != o.colWidths || tableCells != o.tableCells) return false;
    if (dimStyleName != o.dimStyleName) return false;
    if (mleaderStyleName != o.mleaderStyleName) return false;
    if (mleaderLanding != o.mleaderLanding || mleaderArrowLen != o.mleaderArrowLen) return false;
    if (fcfSymbol != o.fcfSymbol || fcfModifier != o.fcfModifier || fcfTolerance != o.fcfTolerance) return false;
    if (fcfDatums != o.fcfDatums) return false;
    if (arcFlags != o.arcFlags) return false;
    if (holes != o.holes) return false;
    if (sectionExempt != o.sectionExempt || sectionMaterial != o.sectionMaterial) return false;
    if (hatchSrcIds != o.hatchSrcIds || hatchPattern != o.hatchPattern) return false;
    if (hatchScale != o.hatchScale || hatchAssociative != o.hatchAssociative || hatchInvalid != o.hatchInvalid) return false;
    if (blockName != o.blockName || scaleX != o.scaleX || scaleY != o.scaleY) return false;
    if (blockAttrs != o.blockAttrs || dynValues != o.dynValues) return false;
    return true;
}

// ============================================================================
// 图元序列化（与 MainWindow 原实现字段一一对应，并新增块引用字段）
// ============================================================================
QJsonObject elementToJson(const Element& e) {
    QJsonObject o;
    o["id"] = e.id; o["type"] = (int)e.type; o["name"] = e.name; o["layer"] = e.layer;
    o["color"] = e.color.name(); o["lineWidth"] = e.lineWidth; o["lineWidthAuto"] = e.lineWidthAuto; o["lineStyle"] = (int)e.lineStyle; o["radius"] = e.radius;
    o["byLayer"] = e.byLayer;   // 图层动态语义：随图存档（旧档无此键 → 读端回落 false + 快照=现值，完全向后兼容）
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
    // 关联边界 / 图案覆盖（只增字段；旧档缺键 → 读端回落默认 = 静态填充）
    if (e.type == ElementType::Hatch) {
        QJsonArray srcIds;
        for (int sid : e.hatchSrcIds) srcIds.append(sid);
        o["hatchSrcIds"] = srcIds;
        o["hatchPattern"]    = e.hatchPattern;
        o["hatchScale"]      = e.hatchScale;
        o["hatchAssociative"] = e.hatchAssociative;
        o["hatchInvalid"]    = e.hatchInvalid;
    }
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
    // ---- 标注样式引用（DimStyle 表；空串不写盘 → 旧档/新档缺键均回落"当前样式"）----
    if (!e.dimStyleName.isEmpty()) o["dimStyleName"] = e.dimStyleName;
    // ---- 多重引线（MLeader）专用字段 ----
    if (e.type == ElementType::MLeader) {
        if (!e.mleaderStyleName.isEmpty()) o["mleaderStyleName"] = e.mleaderStyleName;
        o["mleaderLanding"]  = e.mleaderLanding;
        o["mleaderArrowLen"] = e.mleaderArrowLen;
    }
    // ---- 形位公差框格（Fcf）专用字段 ----
    if (e.type == ElementType::Fcf) {
        o["fcfSymbol"]    = e.fcfSymbol;
        o["fcfModifier"]  = e.fcfModifier;
        o["fcfTolerance"] = e.fcfTolerance;
        QJsonArray datums;
        for (const QString& d : e.fcfDatums) datums.append(d);
        o["fcfDatums"] = datums;
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
    // 旧存档没有 byLayer 字段：默认 false（快照式分组的历史行为——图元用自身 color/width/style）；
    // 新图元在 Element 构造时默认 true（随层）。这样"旧档逐字节语义兼容"自然成立。
    e.byLayer = json::getBool(o, "byLayer", false);
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
    // 关联边界 / 图案覆盖（旧存档缺键 → 回落默认：无来源 / 无覆盖 / scale=1 / 关联开 / 未失效）
    for (const auto& v : o["hatchSrcIds"].toArray()) e.hatchSrcIds.push_back(v.toInt());
    e.hatchPattern     = json::getString(o, "hatchPattern", QString());
    e.hatchScale       = json::getDouble(o, "hatchScale", 1.0);
    e.hatchAssociative = json::getBool(o, "hatchAssociative", true);
    e.hatchInvalid     = json::getBool(o, "hatchInvalid", false);
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
    // ---- 标注样式引用（空串 = 用当前样式；旧档缺键 → 空串，向后兼容）----
    e.dimStyleName = json::getString(o, "dimStyleName", QString());
    // ---- 多重引线（MLeader）专用字段（旧档缺键 → 回落默认）----
    e.mleaderStyleName = json::getString(o, "mleaderStyleName", QString());
    e.mleaderLanding   = json::getDouble(o, "mleaderLanding", 0.0);
    e.mleaderArrowLen  = json::getDouble(o, "mleaderArrowLen", 3.5);
    // ---- 形位公差框格（Fcf）专用字段（旧档缺键 → 回落默认）----
    e.fcfSymbol    = json::getInt(o, "fcfSymbol", 0);
    e.fcfModifier  = json::getString(o, "fcfModifier", QString());
    e.fcfTolerance = json::getString(o, "fcfTolerance", QString());
    for (const auto& v : o["fcfDatums"].toArray()) e.fcfDatums.push_back(v.toString());
    return e;
}

} // namespace SketchCAD
