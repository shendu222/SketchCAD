#include "PlotStyle.h"

namespace SketchCAD {

QJsonObject PlotStyleEntry::toJson() const {
    QJsonObject o;
    o["overrideColor"] = overrideColor;
    o["color"]        = color.name(QColor::HexArgb);
    o["overrideWidth"] = overrideWidth;
    o["widthMm"]      = widthMm;
    return o;
}

PlotStyleEntry PlotStyleEntry::fromJson(const QJsonObject& o) {
    PlotStyleEntry e;
    e.overrideColor = o.value("overrideColor").toBool(false);
    e.color         = QColor(o.value("color").toString());
    e.overrideWidth = o.value("overrideWidth").toBool(false);
    e.widthMm       = o.value("widthMm").toDouble(0.25);
    return e;
}

ResolvedPlotStyle PlotStyleTable::resolve(const Element& el) const {
    ResolvedPlotStyle r;
    r.color   = el.color;
    r.widthMm = el.effectiveLineWidth();   // 自动线宽 → 当前线组粗/细档；手动 → 覆盖值

    // 1) 图层优先
    auto lit = byLayer_.find(el.layer);
    if (lit != byLayer_.end()) {
        if (lit->overrideColor) r.color   = lit->color;
        if (lit->overrideWidth) r.widthMm = lit->widthMm;
        return r;
    }
    // 2) 否则按颜色
    auto cit = byColor_.find(plotColorKey(el.color));
    if (cit != byColor_.end()) {
        if (cit->overrideColor) r.color   = cit->color;
        if (cit->overrideWidth) r.widthMm = cit->widthMm;
    }
    return r;
}

QJsonObject PlotStyleTable::toJson() const {
    QJsonObject o;
    QJsonObject layers, colors;
    for (auto it = byLayer_.begin(); it != byLayer_.end(); ++it)
        layers[it.key()] = it->toJson();
    for (auto it = byColor_.begin(); it != byColor_.end(); ++it) {
        // 键以 16 进制字符串保存，避免 JSON 数字精度问题
        colors[QString::number(it.key(), 16)] = it->toJson();
    }
    o["layers"] = layers;
    o["colors"] = colors;
    return o;
}

void PlotStyleTable::fromJson(const QJsonObject& o) {
    byLayer_.clear();
    byColor_.clear();
    const QJsonObject layers = o.value("layers").toObject();
    for (auto it = layers.begin(); it != layers.end(); ++it)
        byLayer_[it.key()] = PlotStyleEntry::fromJson(it->toObject());
    const QJsonObject colors = o.value("colors").toObject();
    for (auto it = colors.begin(); it != colors.end(); ++it)
        byColor_[it.key().toInt(0, 16)] = PlotStyleEntry::fromJson(it->toObject());
}

PlotStyleTable PlotStyleTable::defaultTable() { return PlotStyleTable(); }

} // namespace SketchCAD
