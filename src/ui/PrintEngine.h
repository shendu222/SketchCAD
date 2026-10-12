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

    // 零布局（模型空间）发布的页面设置：纸张与方向**跟随图纸幅面规格**
    // （model->hasSheetSpec() 为真时经 applySheetSpecToPage 派生），其余默认、
    // 视口清空 + fit-to-page。无幅面规格（空白图/外部导入）→ 旧行为（A4 纵向默认）。
    // exportPdfLayouts 的空表回落分支与 COM 批出图共用本函数，保证同一裁决；
    // 提为公开静态以便无头自测直接断言（不必解析 PDF 字节）。
    static PrintSettings modelSpacePageSettings(const SketchModel* model);

    // 便捷：打印到 QPrinter（物理打印机或系统 PDF 虚拟打印机）
    static bool print(SketchModel* model, const PrintSettings& s, QPrinter* printer);

    // ---- 发布（PUBLISH）：显式页列表出图（GUI 发布与 COM 批量出图共用同一裁决）----
    // 一页 = 页名 + 完整页面设置；「模型空间」页 = 视口空设置；布局页 = 布局 settings 原样。
    struct PublishPage {
        QString name;            // 页名（布局名；模型空间页 = "模型空间"）
        PrintSettings settings;  // 该页完整页面设置（含视口集 / 戳记 / 冻结集）
    };

    // 页列表导出单个多页 PDF：每页独立纸张/方向/边距。返回成功渲染的页数。
    // 空页列表返回 0。exportPdfLayouts 即「布局表 → 页列表」后的本函数。
    static int exportPagesPdf(SketchModel* model, const QList<PublishPage>& pages,
                              const QString& filePath, const QString& title = QString());

    // 页列表发送物理打印机：每页独立页面设置，页间 newPage。返回成功渲染的页数。
    // 打印机页面参数在落笔前设置（QPrinter 语义：对下一页生效）。
    static int printPages(SketchModel* model, const QList<PublishPage>& pages,
                          QPrinter* printer);
};

} // namespace SketchCAD
