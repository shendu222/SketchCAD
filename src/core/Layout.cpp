#include "Layout.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace SketchCAD {

// ============================================================================
// PlotViewport
// ============================================================================

QJsonObject PlotViewport::toJson() const {
    QJsonObject o;
    o["name"]       = name;
    o["x"]          = modelRect.x();
    o["y"]          = modelRect.y();
    o["w"]          = modelRect.width();
    o["h"]          = modelRect.height();
    o["scale"]      = scale;
    o["rotationDeg"] = rotationDeg;
    // paperRect：图纸目标区（相对可打印区左上、y-down）。空矩形不写键 →
    // 旧版读回为整块可打印区（MVP 行为不变）。
    if (!paperRect.isNull() && paperRect.width() > 0 && paperRect.height() > 0) {
        o["paperX"] = paperRect.x();
        o["paperY"] = paperRect.y();
        o["paperW"] = paperRect.width();
        o["paperH"] = paperRect.height();
    }
    // per-viewport 冻结：空集不写键 → 旧版读回为空（零冻结，行为不变）
    if (!frozenLayers.isEmpty()) {
        QJsonArray fl;
        for (const QString& l : frozenLayers) fl.append(l);
        o["frozenLayers"] = fl;
    }
    return o;
}

PlotViewport PlotViewport::fromJson(const QJsonObject& o) {
    PlotViewport vp;
    vp.name        = o.value("name").toString();
    const double x = o.value("x").toDouble(0.0);
    const double y = o.value("y").toDouble(0.0);
    const double w = o.value("w").toDouble(0.0);
    const double h = o.value("h").toDouble(0.0);
    vp.modelRect   = (w <= 0 || h <= 0) ? QRectF() : QRectF(x, y, w, h);
    vp.scale       = o.value("scale").toDouble(1.0);
    vp.rotationDeg = o.value("rotationDeg").toDouble(0.0);
    // 缺键（旧档 / MVP 视口）→ 空 paperRect = 整块可打印区
    const double pw = o.value("paperW").toDouble(0.0);
    const double ph = o.value("paperH").toDouble(0.0);
    if (pw > 0 && ph > 0) {
        const double px = o.value("paperX").toDouble(0.0);
        const double py = o.value("paperY").toDouble(0.0);
        vp.paperRect = QRectF(px, py, pw, ph);
    } else {
        vp.paperRect = QRectF();
    }
    // per-viewport 冻结：缺键（旧档）→ 空冻结集（渲染结果与改造前一致）
    vp.frozenLayers.clear();
    const QJsonArray fl = o.value("frozenLayers").toArray();
    for (const auto& v : fl) {
        const QString s = v.toString();
        if (!s.isEmpty()) vp.frozenLayers.append(s);
    }
    return vp;
}

// ============================================================================
// PrintSettings（实现自 ui/PrintEngine.cpp 迁入；逻辑逐行不变）
// ============================================================================

QJsonObject PrintSettings::toJson() const {
    QJsonObject o;
    o["pageSize"]   = (int)pageSize.id();
    o["orientation"]= (int)orientation;
    o["marginL"]    = marginsMm.left();
    o["marginT"]    = marginsMm.top();
    o["marginR"]    = marginsMm.right();
    o["marginB"]    = marginsMm.bottom();
    o["scale"]      = scale;
    o["lineWidthScale"] = lineWidthScale;
    o["fitToPage"]  = fitToPage;
    o["centerH"]    = centerH;
    o["centerV"]    = centerV;
    o["drawBorder"] = drawBorder;
    o["monochrome"] = monochrome;     // 新增：黑白出图总开关（缺省 false → 旧档不变）
    o["plotStyle"]  = plotStyle.toJson();
    // 打印戳记：仅开启时写键（关闭/旧档缺键 → 默认关闭，出图逐字节不变）
    if (stamp.enabled) o["stamp"] = stamp.toJson();
    QJsonArray vps;
    for (const auto& vp : viewports) vps.append(vp.toJson());
    o["viewports"] = vps;
    return o;
}

void PrintSettings::fromJson(const QJsonObject& o) {
    const QPageSize::PageSizeId id = (QPageSize::PageSizeId)o.value("pageSize").toInt((int)QPageSize::A4);
    pageSize = QPageSize(id);
    orientation = (QPageLayout::Orientation)o.value("orientation").toInt((int)QPageLayout::Portrait);
    marginsMm = QMarginsF(o.value("marginL").toDouble(10.0),
                          o.value("marginT").toDouble(10.0),
                          o.value("marginR").toDouble(10.0),
                          o.value("marginB").toDouble(10.0));
    scale      = o.value("scale").toDouble(1.0);
    lineWidthScale = o.value("lineWidthScale").toDouble(0.5);
    fitToPage  = o.value("fitToPage").toBool(true);
    centerH    = o.value("centerH").toBool(true);
    centerV    = o.value("centerV").toBool(true);
    drawBorder = o.value("drawBorder").toBool(true);
    monochrome = o.value("monochrome").toBool(false);   // 缺 key → false（旧档行为不变）
    plotStyle.fromJson(o.value("plotStyle").toObject());
    stamp.fromJson(o.value("stamp").toObject());        // 缺 key → 空对象 → 全默认（关闭）
    viewports.clear();
    const QJsonArray vps = o.value("viewports").toArray();
    for (const auto& v : vps) viewports.append(PlotViewport::fromJson(v.toObject()));
}

PrintSettings PrintSettings::defaults() {
    PrintSettings s;
    s.viewports.append(PlotLayout::fullModelViewport());
    return s;
}

PlotStyleTable PrintSettings::effectivePlotStyle() const {
    // 黑白总开关开启且用户未提供自定义表 → 套用 GB 预设（按当前线宽组 G1..G5 分粗/细）。
    // 用户已自定义表时以自定义表为准（黑白开关不覆盖，避免抹掉手工线宽分组）。
    if (monochrome && plotStyle.empty())
        return PlotStyleTable::gbPreset(currentLineWidthGroup());
    return plotStyle;
}

// ============================================================================
// PlotStamp：缺键逐项回落默认（enabled 缺 → false，旧行为不变）
// ============================================================================

QJsonObject PlotStamp::toJson() const {
    QJsonObject o;
    o["enabled"]    = enabled;
    o["drawingName"]= showDrawingName;
    o["date"]       = showDate;
    o["time"]       = showTime;
    o["layoutName"] = showLayoutName;
    o["scale"]      = showScale;
    o["fileName"]   = showFileName;
    o["fontFamily"] = fontFamily;
    o["textHeightMm"] = textHeightMm;
    o["position"]   = position;
    return o;
}

void PlotStamp::fromJson(const QJsonObject& o) {
    if (o.isEmpty()) return;      // 缺键 → 保持默认（关闭）
    enabled          = o.value("enabled").toBool(false);
    showDrawingName  = o.value("drawingName").toBool(true);
    showDate         = o.value("date").toBool(true);
    showTime         = o.value("time").toBool(true);
    showLayoutName   = o.value("layoutName").toBool(true);
    showScale        = o.value("scale").toBool(true);
    showFileName     = o.value("fileName").toBool(true);
    fontFamily       = o.value("fontFamily").toString();
    textHeightMm     = o.value("textHeightMm").toDouble(2.5);
    position         = o.value("position").toInt(3);
}

// ============================================================================
// PlotLayoutDef
// ============================================================================

QJsonObject PlotLayoutDef::toJson() const {
    QJsonObject o;
    o["name"]     = name;
    o["settings"] = settings.toJson();
    return o;
}

PlotLayoutDef PlotLayoutDef::fromJson(const QJsonObject& o) {
    PlotLayoutDef def;
    def.name = o.value("name").toString();
    def.settings.fromJson(o.value("settings").toObject());
    return def;
}

// ============================================================================
// viewportScaleMm：视口最终出图比例解析（与 PrintEngine::render 历史公式同源）
// ============================================================================

double viewportScaleMm(const PrintSettings& s, const PlotViewport& vp,
                       const QRectF& mb, double printW, double printH) {
    const double fit = (mb.width() > 0 && mb.height() > 0)
        ? std::min(printW / mb.width(), printH / mb.height()) * 0.98
        : 1.0;
    return (vp.scale > 0) ? vp.scale : (s.fitToPage ? fit : s.scale);
}

} // namespace SketchCAD
