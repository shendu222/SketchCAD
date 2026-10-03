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
// PrintSettings（页面设置）与 PlotViewport / PlotLayoutDef / viewportScaleMm
// 定义于 core/Layout.h（为使 SketchModel 布局表可下沉到 core 层持有）。
//   * 视口为空            → 模型空间打印（直接从模型空间输出，无需切换布局）。
//   * 视口非空            → 按视口布局（vp.paperRect 为空 → 映射整块可打印区）。
// render() 可作用于任意 QPaintDevice（QPdfWriter / QPrinter / QPixmap / QWidget），
// 因此预览、PDF、物理打印共用同一套绘制逻辑，保证一致。
// ============================================================================
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

    // 按布局表导出 PDF：每布局一页。返回成功渲染的页数；
    // 布局表为空 → 回落单页模型空间打印（fit-to-page），返回 1。
    static int exportPdfLayouts(SketchModel* model, const QString& filePath);

    // 便捷：打印到 QPrinter（物理打印机或系统 PDF 虚拟打印机）
    static bool print(SketchModel* model, const PrintSettings& s, QPrinter* printer);
};

} // namespace SketchCAD
