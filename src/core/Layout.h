#pragma once
#include <QPageSize>
#include <QPageLayout>
#include <QRectF>
#include <QMarginsF>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "core/PlotStyle.h"

namespace SketchCAD {

// ============================================================================
// 布局 / 视口 / 页面设置
// ----------------------------------------------------------------------------
// 布局体系（多布局 + 布局内多视口 + 视口独立比例）：
//   * PrintSettings 充当「页面设置」：页面参数 + 样式映射 + 视口集合。
//     视口为空 → 模型空间打印（整体图形直接出图，无需切换布局，历史行为不变）。
//   * PlotViewport 描述「模型空间的哪一块矩形区域(modelRect) 以何种比例映射到
//     图纸的哪一块矩形(paperRect)」；paperRect 为空 → 映射到整块可打印区
//     （MVP 兼容行为）。
//   * PlotLayoutDef = 布局名 + 一份完整页面设置；由 SketchModel 的布局表
//     （layouts_）持有并随图存档，实现多布局。
// 本头文件位于 core 层，不得引入 ui/ 依赖（QPdfWriter/QPrinter 等留在 PrintEngine）。
// ============================================================================

// 视口：模型空间中的一块矩形区域，及其到图纸的映射比例
struct PlotViewport {
    QString name;          // 视口名称（便于识别）
    QRectF  modelRect;     // 模型空间世界坐标(mm, y-up)；空矩形 → 整体图形范围
    double  scale = 1.0;   // 视口比例：视口内 1 个模型 mm 在图纸上占 scale mm；<=0 沿用布局整体比例
    double  rotationDeg = 0.0;
    // 图纸目标区（mm）：相对可打印区左上角、y 向下；空矩形 → 整块可打印区（MVP 行为）。
    // 视口平铺：布局内多个视口各占 paperRect 一块，实现矩形排布。
    QRectF  paperRect;

    QJsonObject toJson() const;
    static PlotViewport fromJson(const QJsonObject& o);
};

// 当前设计把「布局」直接由 PrintSettings 表达，此处仅提供一个构造整体图形单视口的
// 便捷工厂，供 PrintSettings::defaults() 使用。
struct PlotLayout {
    static PlotViewport fullModelViewport() {
        PlotViewport vp;
        vp.name = QStringLiteral("模型空间");
        vp.modelRect = QRectF();   // 空 → 使用整体范围
        vp.scale = 1.0;
        return vp;
    }
};

// ============================================================================
// 页面设置（原定义于 ui/PrintEngine.h，为使 core 层 SketchModel 可持有布局表，
// 下沉至 core/Layout.h；字段与语义不变）
// ============================================================================
struct PrintSettings {
    QPageSize pageSize = QPageSize(QPageSize::A4);
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QMarginsF marginsMm = QMarginsF(10.0, 10.0, 10.0, 10.0);  // 左/上/右/下 (mm)
    double scale = 1.0;          // 自定义比例：1.0 = 1:1（模型 mm → 图纸 mm）
    // 打印线宽比例：打印时对图元线宽(mm)的整体缩放。<1 变细（默认 0.5，细一档，避免 A4/A3
    // 上 0.5~2.0mm 的国标屏幕线宽印出来过粗失真）；1.0 = 按图纸 mm 原值输出。
    double lineWidthScale = 0.5;
    bool   fitToPage = true;     // 自适应铺满可打印区（优先于 scale）
    bool   centerH = true;       // 水平居中
    bool   centerV = true;       // 垂直居中
    bool   drawBorder = true;    // 绘制可打印区边框（便于预览对齐）
    // 黑白出图总开关：true 时若无自定义样式表，自动套用 GB 预设（按线型分粗/细线宽 + 纯黑）。
    // 用于「黑白打印机 / 黑白 PDF」一键出图，确保线宽分组效果符合 GB/T 14665。
    // 序列化键 "monochrome"（缺省 false → 旧档行为不变）。
    bool   monochrome = false;
    PlotStyleTable plotStyle;    // 简化 CTB 映射（灰度/线宽/抖动）
    QList<PlotViewport> viewports; // 空 = 模型空间打印；非空 = 按视口布局

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
    static PrintSettings defaults();

    // 渲染前把「黑白开关 + 样式表」解析为最终样式表：
    //   monochrome=true 且 plotStyle 为空 → 套用 GB 预设（按当前线宽组）；
    //   否则原样返回 plotStyle（自定义表优先，黑白总开关不覆盖用户表）。
    PlotStyleTable effectivePlotStyle() const;
};

// 布局定义：布局名 + 一份完整页面设置（含视口集）。由 SketchModel 布局表持有。
struct PlotLayoutDef {
    QString name;
    PrintSettings settings;

    QJsonObject toJson() const;
    static PlotLayoutDef fromJson(const QJsonObject& o);
};

// 纯函数：解析某视口最终出图比例（模型 mm → 图纸 mm）。
// 与 PrintEngine::render 的历史公式逐字节同源：
//   vp.scale > 0 → 视口独立比例（1:2 出图 = 0.5，2:1 出图 = 2.0）；
//   否则 fitToPage → 适配可打印区（留 2% 边距），再否则用布局整体 scale。
double viewportScaleMm(const PrintSettings& s, const PlotViewport& vp,
                       const QRectF& mb, double printW, double printH);

} // namespace SketchCAD
