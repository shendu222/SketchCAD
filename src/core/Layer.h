#pragma once
#include <QString>
#include <QColor>
#include "Element.h"   // 复用 LineStyle 枚举
#include "LineWidthGroup.h"   // standardLayers() 按当前线组派生粗/细线宽(mm)

namespace SketchCAD {

// 图层（Layer）：图纸的"属性分组容器"。
// 一个图层除了名字外，还携带着该层的"默认样式"（颜色 / 线型 / 线宽）与显示/锁定状态。
// 图元通过 Element::layer 字符串归属到某一图层；对图层执行"批量改属性"或"显示控制"
// 时，只遍历 e.layer == 该层名 的图元，其他图层图元的内容与显示状态保持不动。
//
// 图元与图层的交互规则（满足"互不干扰 / 可单独编辑 / 图层变更与已有图元解耦"）：
//   * 把图元移入图层时【保留图元原有属性】，不套用图层样式（assignSelectionToLayer 默认 adoptStyle=false）；
//   * 【解耦】修改图层属性（setLayerStyle）时，仅更新该图层定义（颜色/线宽/线型）；
//     本层已有图元的属性保持不变（不反向推送）；只有此后新建的图元才快照该层最新属性；
//     （若需强制套用，调用 applyLayerStyleToElements）。其他图层图元不受影响；
//   * 单个图元的属性仍可在属性面板单独编辑，只作用于该图元本身，不反向改变图层定义；
//   * setLayerVisible() 批量翻转本层图元的 Element::visible（渲染层已直接读 visible 决定绘制）。
// 以上都只迭代本层图元，因此天然隔离。
struct Layer {
    QString   name      = QStringLiteral("0");   // 图层名（唯一键；默认层固定为 "0"）
    QColor    color     = QColor(26, 29, 40);     // 图层默认颜色（深灰 1A1D28）
    double    lineWidth = 1.0;                     // 图层默认线宽(px)
    LineStyle lineStyle = LineStyle::Solid;         // 图层默认线型
    bool      visible   = true;                    // 是否显示（false = 整层隐藏）
    bool      locked    = false;                   // 是否锁定（UI 据此禁止编辑本层图元）
    // ---- 图层动态语义（仅追加字段，不改动既有字段：满足"Layer 结构只增不改"约束）----
    // 冻结（frozen）：独立于 visible 的"硬隔离"。冻结层图元不参与命中/重算/打印，
    //   无论 visible 如何都不绘制、不可拾取、不出图（与 AutoCAD FROZEN 语义一致）。
    //   visible 是"软隐藏"（仍参与重算与打印，仅不绘制）；frozen 是"硬冻结"（彻底排除）。
    // 打印开关（plot）：false = 该层不打印（图层可显示但出图时跳过）。
    // 透明度（alpha）：0~1，1 为完全不透明；渲染时叠加到本层图元颜色 alpha 通道。
    bool      frozen    = false;                   // 是否冻结（true = 整层硬冻结，不参与命中/重算/打印）
    bool      plot      = true;                    // 是否打印（false = 该层不输出到图纸）
    double    alpha     = 1.0;                      // 图层透明度（1.0 = 不透明；0.0 = 全透明）

    // GB/T 14665-2012 第 8 章 表6：图层标识号（"01"~"15"）与标准名。
    // 非标准层（默认层 "0"、图框/标题栏等）留空，向后兼容、无副作用。
    QString   code         = QString();            // 标识号，如 "01"
    QString   standardName = QString();            // 表6 标准名，如 "粗实线"

    Layer() = default;
    explicit Layer(const QString& n) : name(n) {}

    // 把本层样式套到一个图元上：颜色/线型/线宽写入 e，并把线宽设为"手动覆盖"，
    // 使图层指定的线宽被渲染采用（与 styleSelectedEx 的语义一致）。
    void applyTo(Element& e) const {
        if (color.isValid()) e.color = color;
        e.lineStyle = lineStyle;
        e.lineWidth = lineWidth;
        e.lineWidthAuto = false;     // 手动覆盖：线宽以 layer 指定值为准
    }
};

// GB/T 14665-2012 第 8 章 表6「图线的分层标识」标准图层模板（01~12）。
// 每层给出标识号 code、标准名 standardName 与建议线型；线宽按【当前线组】
// (LineWidthGroup, GB/T 14665-2012 表1) 的粗/细档派生，单位 mm。
// 用于「新建图纸」时预置图层表，使图层可交换、可按标准标识批量出图。
inline std::vector<Layer> standardLayers() {
    struct Def { const char* code; const char* name; LineStyle ls; bool thick; };
    static const Def kDefs[] = {
        {"01", "粗实线",                                     LineStyle::Solid,      true },
        {"02", "细实线、波浪线、双折线",                     LineStyle::Solid,      false},
        {"03", "粗虚线",                                     LineStyle::Dashed,     true },
        {"04", "细虚线",                                     LineStyle::Dashed,     false},
        {"05", "细点画线",                                   LineStyle::DashDot,    false},
        {"06", "粗点画线",                                   LineStyle::DashDot,    true },
        {"07", "细双点画线",                                 LineStyle::DashDotDot, false},
        {"08", "尺寸线、尺寸界线、尺寸终端与符号、尺寸和公差", LineStyle::Solid,      false},
        {"09", "参考圆",                                     LineStyle::DashDot,    false},
        {"10", "剖面符号",                                   LineStyle::Solid,      false},
        {"11", "文本（字母、数字、符号）",                   LineStyle::Solid,      false},
        {"12", "文本（汉字）",                               LineStyle::Solid,      false},
    };
    const double wThick = groupThickMm(currentLineWidthGroup());   // 表1：粗线(mm)
    const double wThin  = groupThinMm(currentLineWidthGroup());    // 表1：细线(mm)
    std::vector<Layer> out;
    out.reserve(sizeof(kDefs) / sizeof(kDefs[0]));
    for (const auto& d : kDefs) {
        const QString code = QString::fromUtf8(d.code);
        Layer l(QString::fromUtf8(d.code) + QStringLiteral(" ") + QString::fromUtf8(d.name));
        l.code = code;
        l.standardName = QString::fromUtf8(d.name);
        l.lineStyle = d.ls;
        l.lineWidth = d.thick ? wThick : wThin;     // mm
        out.push_back(std::move(l));
    }
    return out;
}

} // namespace SketchCAD
