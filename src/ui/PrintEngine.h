#pragma once
#include <QPaintDevice>
#include <QString>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QList>
#include "core/Element.h"
#include "core/SketchModel.h"
#include "core/PlotStyle.h"
#include "core/Layout.h"
#include "ui/PlotRenderer.h"

class QPdfWriter;
class QPrinter;

namespace SketchCAD {

// ============================================================================
// 打印 / 出图引擎
// ----------------------------------------------------------------------------
// PrintSettings 同时充当「布局」的最小定义：一张页面的页面参数 + 视口集合 + 样式映射。
//   * 视口为空            → 模型空间打印（直接从模型空间输出，无需切换布局）。
//   * 视口非空            → 按视口布局（每个视口将自身 modelRect 映射到整块可打印区）。
// render() 可作用于任意 QPaintDevice（QPdfWriter / QPrinter / QPixmap / QWidget），
// 因此预览、PDF、物理打印共用同一套绘制逻辑，保证一致。
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
    PlotStyleTable plotStyle;    // 简化 CTB 映射
    QList<PlotViewport> viewports; // 空 = 模型空间打印；非空 = 按视口布局

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
    static PrintSettings defaults();
};

class PrintEngine {
public:
    // 设备像素 / 毫米
    static double devicePxPerMm(QPaintDevice* dev);

    // 模型整体世界包围盒（供 fit-to-page / 默认视口使用）
    static QRectF modelBounds(SketchModel* model);

    // 把模型渲染到任意 QPaintDevice。返回是否成功。
    static bool render(SketchModel* model, QPaintDevice* dev, const PrintSettings& s);

    // 便捷：导出 PDF（优先使用 QPdfWriter）
    static bool exportPdf(SketchModel* model, const PrintSettings& s, const QString& filePath);

    // 便捷：打印到 QPrinter（物理打印机或系统 PDF 虚拟打印机）
    static bool print(SketchModel* model, const PrintSettings& s, QPrinter* printer);
};

} // namespace SketchCAD
