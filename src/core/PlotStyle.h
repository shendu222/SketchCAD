#pragma once
#include <QColor>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include "core/Element.h"

namespace SketchCAD {

// ============================================================================
// 简化版 CTB（颜色相关打印样式）映射
// ----------------------------------------------------------------------------
// 支持两种匹配维度：
//   1) 按图层名（layer）            —— 优先级最高
//   2) 按图元颜色（color rgba 键）   —— 其次
// 每个条目可分别覆盖「输出颜色」与「输出线宽(mm)」；未覆盖的维度沿用图元自身属性。
// 这是 AutoCAD plot style table 的极简子集，足以满足「按颜色/图层指定线宽与颜色」需求。
// ============================================================================

// 单条映射规则
struct PlotStyleEntry {
    bool   overrideColor = false;   // 是否覆盖输出颜色
    QColor color;                    // 覆盖后的输出颜色
    bool   overrideWidth = false;   // 是否覆盖输出线宽
    double widthMm = 0.25;           // 覆盖后的输出线宽(mm)

    QJsonObject toJson() const;
    static PlotStyleEntry fromJson(const QJsonObject& o);
};

// 已展开为最终输出属性
struct ResolvedPlotStyle {
    QColor color;
    double widthMm = 0.25;
};

// 颜色 → 整数键（用于 QMap 索引）
inline int plotColorKey(const QColor& c) { return (int)c.rgba(); }

class PlotStyleTable {
public:
    void setLayerStyle(const QString& layer, const PlotStyleEntry& e) { byLayer_[layer] = e; }
    void setColorStyle(const QColor& color, const PlotStyleEntry& e)  { byColor_[plotColorKey(color)] = e; }
    void clearLayer(const QString& layer) { byLayer_.remove(layer); }
    void clearColor(const QColor& color)  { byColor_.remove(plotColorKey(color)); }
    bool empty() const { return byLayer_.isEmpty() && byColor_.isEmpty(); }

    // 给定图元，按「图层优先、其次颜色」解析最终输出属性。
    ResolvedPlotStyle resolve(const Element& el) const;

    const QMap<QString, PlotStyleEntry>& layerStyles() const { return byLayer_; }
    const QMap<int, PlotStyleEntry>&     colorStyles() const { return byColor_; }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
    static PlotStyleTable defaultTable();   // 空表：全部沿用图元自身属性

private:
    QMap<QString, PlotStyleEntry> byLayer_;
    QMap<int, PlotStyleEntry>     byColor_;
};

} // namespace SketchCAD
