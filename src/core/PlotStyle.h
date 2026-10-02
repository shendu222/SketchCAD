#pragma once
#include <QColor>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include "core/Element.h"
#include "core/LineWidthGroup.h"   // currentLineWidthGroup() / groupThickMm / groupThinMm

namespace SketchCAD {

// ============================================================================
// 简化版 CTB（颜色相关打印样式）映射
// ----------------------------------------------------------------------------
// 支持三种匹配维度（优先级由高到低）：
//   1) 按图层名（layer）            —— 优先级最高
//   2) 按图元颜色（color rgba 键）   —— 其次
//   3) 按线型（LineStyle）          —— 最低（GB 预设用，见 gbPreset()）
// 每个条目可分别覆盖「输出颜色」「输出线宽(mm)」「灰度」「抖动」，未覆盖的维度沿用图元自身属性。
// 这是 AutoCAD plot style table 的极简子集，足以满足「按颜色/图层指定线宽与颜色」需求。
//
// 黑白出图（灰度）语义：
//   * gray = 0 → 纯黑，gray = 1 → 纯白；中间值按线性灰度化。
//   * 灰度化只改「颜色」，不自动改线宽 —— 线宽分组效果必须由 widthMm 显式指定，
//     这正是「彩色线宽图打印必错」的根因：彩色下靠颜色区分的粗/细，黑白下需靠线宽区分。
//   * dither = true 时对灰度化后的中间调启用抖动（网点），使细密填充/剖面线在黑白纸上有层次；
//     dither = false 则直接用实灰度（适合线宽图，轮廓更锐利）。
// ============================================================================

// 单条映射规则
struct PlotStyleEntry {
    bool   overrideColor = false;   // 是否覆盖输出颜色
    QColor color;                    // 覆盖后的输出颜色
    bool   overrideWidth = false;   // 是否覆盖输出线宽
    double widthMm = 0.25;           // 覆盖后的输出线宽(mm)
    // ---- 灰度 / 抖动（黑白出图；向后兼容：旧档无此两键时保持 false/0，行为=沿用彩色）----
    bool   overrideGray = false;    // 是否强制灰度化（黑白打印机/PDF）
    double gray = 0.0;               // 目标灰度 0=黑 1=白（overrideGray=true 时生效）
    bool   dither = false;          // 灰度化后是否启用网点抖动

    QJsonObject toJson() const;
    static PlotStyleEntry fromJson(const QJsonObject& o);
};

// 已展开为最终输出属性
struct ResolvedPlotStyle {
    QColor color;
    double widthMm = 0.25;
    bool   gray = false;    // 是否已灰度化（供渲染层决定是否抖动/如何取色）
    double grayValue = 0.0; // 0=黑 1=白
    bool   dither = false;  // 是否抖动
};

// 颜色 → 整数键（用于 QMap 索引）
inline int plotColorKey(const QColor& c) { return (int)c.rgba(); }

class PlotStyleTable {
public:
    void setLayerStyle(const QString& layer, const PlotStyleEntry& e) { byLayer_[layer] = e; }
    void setColorStyle(const QColor& color, const PlotStyleEntry& e)  { byColor_[plotColorKey(color)] = e; }
    // 按线型映射（GB 预设用：粗实线/细实线/虚线/点画线各自线宽与灰度）
    void setStyleStyle(LineStyle ls, const PlotStyleEntry& e)          { byStyle_[(int)ls] = e; }
    void clearLayer(const QString& layer) { byLayer_.remove(layer); }
    void clearColor(const QColor& color)  { byColor_.remove(plotColorKey(color)); }
    void clearStyle(LineStyle ls)         { byStyle_.remove((int)ls); }
    void clearAll() { byLayer_.clear(); byColor_.clear(); byStyle_.clear(); }
    bool empty() const { return byLayer_.isEmpty() && byColor_.isEmpty() && byStyle_.isEmpty(); }

    // 全局灰度开关（不针对具体条目）：为 true 时所有图元输出前统一灰度化。
    // 供「黑白打印机 + 任意彩色图」场景一键出图；条目级 overrideGray 优先级更高。
    void setGlobalGray(bool on, double v = 0.0, bool dither = false) {
        globalGray_ = on; globalGrayValue_ = v; globalDither_ = dither;
    }
    bool globalGrayOn() const { return globalGray_; }

    // 给定图元，按「图层 > 颜色 > 线型」解析最终输出属性。
    ResolvedPlotStyle resolve(const Element& el) const;

    const QMap<QString, PlotStyleEntry>& layerStyles() const { return byLayer_; }
    const QMap<int, PlotStyleEntry>&     colorStyles() const { return byColor_; }
    const QMap<int, PlotStyleEntry>&     styleStyles() const { return byStyle_; }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
    static PlotStyleTable defaultTable();   // 空表：全部沿用图元自身属性

    // ---- GB/T 14665 黑白预设 ----
    // gbPreset：按线型给出标准黑白线宽（图幅组别由 group 决定），灰度默认 0（黑）不抖动。
    // gbPresetBW：灰度预设（图层名含关键字时给不同灰度），用于区分图层而非线宽。
    static PlotStyleTable gbPreset(LineWidthGroup group = currentLineWidthGroup());
    static PlotStyleTable gbPresetGrayDither();

    // ---- 简化 CTB 文本导入 ----
    // 支持行式语法（大小写不敏感，'#' 起始为注释）：
    //   LAYER  <名字>  [WIDTH=<mm>] [GRAY=<0..1>] [DITHER=0|1] [COLOR=#rrggbb]
    //   COLOR  <#rrggbb> [WIDTH=<mm>] [GRAY=<0..1>] [DITHER=0|1] [COLOR=#rrggbb]
    //   STYLE  <线型名> [WIDTH=<mm>] [GRAY=<0..1>] [DITHER=0|1] [COLOR=#rrggbb]
    //   GRAYSCALE <0..1> [DITHER=0|1]        —— 全局灰度开关
    // 解析失败（未知关键字/缺值）时返回 false 并给出 err；已解析的行保留（宽容策略）。
    static bool fromCtb(const QString& text, PlotStyleTable* out, QString* err = nullptr);

    // 导出为简化 CTB 文本（与 fromCtb 互逆，便于存档/交付给第三方）
    QString toCtb() const;

private:
    QMap<QString, PlotStyleEntry> byLayer_;
    QMap<int, PlotStyleEntry>     byColor_;
    QMap<int, PlotStyleEntry>     byStyle_;
    bool   globalGray_ = false;
    double globalGrayValue_ = 0.0;
    bool   globalDither_ = false;
};

// 纯函数：把颜色按灰度值线性灰度化（gray=0→黑, 1→白）。保留 alpha。
QColor plotGrayscale(const QColor& c, double gray);

// 纯函数：线型名 ↔ LineStyle（CTB 文本用）。未知返回 false。
bool plotStyleFromName(const QString& name, LineStyle* out);
QString plotStyleName(LineStyle ls);

} // namespace SketchCAD
