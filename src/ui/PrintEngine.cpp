#include "PrintEngine.h"
#include "ElementPath.h"    // ElementPath::rawBounds（静态、视图无关、无头复用）
#include <QPdfWriter>
#include <QPrinter>
#include <QPainter>
#include <QTransform>
#include <QRectF>
#include <QPen>
#include <QStringList>
#include <cmath>

namespace SketchCAD {
namespace {

double effectivePageW(const PrintSettings& s) {
    const QSizeF ps = s.pageSize.size(QPageSize::Millimeter);
    return (s.orientation == QPageLayout::Landscape) ? ps.height() : ps.width();
}
double effectivePageH(const PrintSettings& s) {
    const QSizeF ps = s.pageSize.size(QPageSize::Millimeter);
    return (s.orientation == QPageLayout::Landscape) ? ps.width() : ps.height();
}

// 将一块模型矩形(modelRect) 以给定比例(模型mm→图纸mm)映射到图纸目标区
// (tx,ty,tw,th，均为图纸 mm，相对页面左上、y-down；模型空间打印=整块可打印区，
//  视口=vp.paperRect 平移到可打印区内)，应用居中并裁剪到目标区，调用 PlotRenderer 绘制。
void renderViewport(SketchModel* model, QPainter& p, const PrintSettings& s,
                    double pxPerMm, double tx, double ty, double tw, double th,
                    const QRectF& mb, double scaleMm, bool centerH, bool centerV) {
    if (mb.isNull() || mb.width() <= 0 || mb.height() <= 0) return;
    if (tw <= 0 || th <= 0) return;
    const double scaleDev = scaleMm * pxPerMm;   // 世界 mm → 设备 px

    const double anchorX = centerH ? (mb.x() + mb.width() / 2.0)  : mb.x();
    const double anchorY = centerV ? (mb.y() + mb.height() / 2.0) : mb.y();
    const double originX = centerH ? ((tx + tw / 2.0) * pxPerMm) : (tx * pxPerMm);
    const double originY = centerV ? ((ty + th / 2.0) * pxPerMm) : (ty * pxPerMm);

    QTransform t(scaleDev, 0, 0, -scaleDev,
                 originX - anchorX * scaleDev,
                 originY + anchorY * scaleDev);
    p.save();
    p.setTransform(t);

    // 目标区裁剪：设备矩形 → 世界矩形
    QTransform inv = t.inverted();
    const QRectF clipDev(tx * pxPerMm, ty * pxPerMm, tw * pxPerMm, th * pxPerMm);
    const QRectF clipWorld = inv.mapRect(clipDev);

    // 打印样式：黑白总开关 + 简化 CTB 映射在渲染前统一解析（见 effectivePlotStyle）
    PlotRenderer::renderModel(&p, model, scaleDev, clipWorld, s.effectivePlotStyle(), s.lineWidthScale);
    p.restore();
}

// 单页渲染主体：边框 + 模型空间/视口布局。render() 与 exportPdfLayouts 共用。
void renderPageBody(SketchModel* model, QPainter& p, const PrintSettings& s) {
    const double pxPerMm = PrintEngine::devicePxPerMm(p.device());
    const double pageW = effectivePageW(s);
    const double pageH = effectivePageH(s);
    const double mL = s.marginsMm.left(), mT = s.marginsMm.top();
    const double mR = s.marginsMm.right(), mB = s.marginsMm.bottom();
    const double printW = std::max(1.0, pageW - mL - mR);
    const double printH = std::max(1.0, pageH - mT - mB);

    // 可打印区边框（设备坐标，y-down）
    if (s.drawBorder) {
        p.save();
        p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(mL * pxPerMm, mT * pxPerMm, printW * pxPerMm, printH * pxPerMm));
        p.restore();
    }

    if (s.viewports.isEmpty()) {
        // 模型空间打印：整体包围盒 → 整块可打印区（历史行为不变）
        const QRectF mb = PrintEngine::modelBounds(model);
        if (mb.isNull() || mb.width() <= 0 || mb.height() <= 0) return;  // 空图纸：仅边框
        const double fit = std::min(printW / mb.width(), printH / mb.height()) * 0.98;
        const double scaleMm = s.fitToPage ? fit : s.scale;
        renderViewport(model, p, s, pxPerMm, mL, mT, printW, printH,
                      mb, scaleMm, s.centerH, s.centerV);
    } else {
        // 视口布局：每个视口映射到自身 paperRect（相对可打印区左上、y-down）；
        // paperRect 为空 → 整块可打印区（MVP 兼容行为不变）。
        for (const auto& vp : s.viewports) {
            const QRectF mb = vp.modelRect.isNull() ? PrintEngine::modelBounds(model) : vp.modelRect;
            double tx = mL, ty = mT, tw = printW, th = printH;
            if (!vp.paperRect.isNull() && vp.paperRect.width() > 0 && vp.paperRect.height() > 0) {
                tx = mL + vp.paperRect.x();
                ty = mT + vp.paperRect.y();
                tw = vp.paperRect.width();
                th = vp.paperRect.height();
            }
            const double scaleMm = viewportScaleMm(s, vp, mb, tw, th);
            renderViewport(model, p, s, pxPerMm, tx, ty, tw, th, mb, scaleMm, true, true);
        }
    }
}

} // namespace

double PrintEngine::devicePxPerMm(QPaintDevice* dev) {
    const int dpi = dev ? dev->logicalDpiX() : 96;
    return dpi / 25.4;
}

// 文本图元的世界包围盒（mm）：paintText 以 pts[0] 为左端点、垂直居中排字；
// 字高存于 radius（默认 3.5mm）。逐行按 CJK≈1em / ASCII≈0.55em 估宽，保证
// 「技术要求」等文本块参与 fit-to-page 取景，打印布局与屏幕一致。
static QRectF textBoxOf(const Element& e) {
    if (e.pts.empty() || e.text.isEmpty()) return QRectF();
    const double th = e.radius > 0.1 ? e.radius : 3.5;   // 文字高度(mm)
    const QStringList rows = e.text.split('\n');
    double maxW = 0.0;
    for (const QString& row : rows) {
        double w = 0.0;
        for (const QChar& ch : row)
            w += (ch.unicode() >= 0x2E80) ? th : th * 0.55;
        maxW = std::max(maxW, w);
    }
    const int n = rows.size();
    const double lineH = th * 1.35;                       // 行距（与 drawText 接近）
    const double h = n * lineH;
    return QRectF(e.pts[0].x, e.pts[0].y - h * 0.5, maxW, h).adjusted(-th*0.15, -th*0.15, th*0.15, th*0.15);
}

QRectF PrintEngine::modelBounds(SketchModel* model) {
    QRectF b;
    bool first = true;
    auto grow = [&b, &first](const QRectF& r) {
        if (r.isNull() || r.width() <= 0 || r.height() <= 0) return;
        if (first) { b = r; first = false; }
        else b = b.united(r);
    };
    for (const auto& e : model->elements()) {
        if (e.type == ElementType::Xline) continue;       // 无限线不参与取景（与屏幕 fit 一致）
        if (e.type == ElementType::Text) { grow(textBoxOf(e)); continue; }
        if (e.type == ElementType::BlockRef) {
            // 块引用：取展开子图元的并集（与 paintBlockRef 渲染范围同源）
            for (const auto& sub : model->expandBlockRef(e)) {
                if (sub.type == ElementType::BlockRef) continue;
                if (sub.type == ElementType::Text) { grow(textBoxOf(sub)); continue; }
                grow(ElementPath::rawBounds(sub));
            }
            continue;
        }
        grow(ElementPath::rawBounds(e));
    }
    return b;
}

bool PrintEngine::render(SketchModel* model, QPaintDevice* dev, const PrintSettings& s) {
    if (!model || !dev) return false;
    QPainter p(dev);
    if (!p.isActive() && !p.begin(dev)) return false;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    renderPageBody(model, p, s);
    p.end();
    return true;
}

bool PrintEngine::exportPdf(SketchModel* model, const PrintSettings& s, const QString& filePath) {
    QPdfWriter pdf(filePath);
    pdf.setResolution(300);                       // 高分辨率矢量/栅格混合输出
    pdf.setPageSize(s.pageSize);
    pdf.setPageOrientation(s.orientation);
    pdf.setPageMargins(s.marginsMm, QPageLayout::Millimeter);
    pdf.setTitle(QStringLiteral("SketchCAD 打印输出"));
    return render(model, &pdf, s);
}

// 按布局表导出 PDF：每布局一页（各布局独立纸张/方向/边距，视口按 paperRect 平铺）。
// 布局表为空 → 回落单页模型空间打印（fit-to-page），与历史单页出图行为等价，返回 1。
int PrintEngine::exportPdfLayouts(SketchModel* model, const QString& filePath) {
    if (!model) return 0;
    if (model->layoutCount() == 0) {
        PrintSettings s;          // 视口空 = 模型空间打印；fitToPage 默认 true
        s.viewports.clear();
        s.fitToPage = true;
        return exportPdf(model, s, filePath) ? 1 : 0;
    }

    QPdfWriter pdf(filePath);
    pdf.setResolution(300);
    pdf.setTitle(QStringLiteral("SketchCAD 打印输出（全部布局）"));
    QPainter p;
    bool first = true;
    int pages = 0;
    for (int i = 0; i < model->layoutCount(); ++i) {
        const PlotLayoutDef* def = model->layoutAt(i);
        if (!def) continue;
        // 每布局独立页面设置：QPdfWriter 在 newPage/首页落笔时读取当前页面布局
        pdf.setPageSize(def->settings.pageSize);
        pdf.setPageOrientation(def->settings.orientation);
        pdf.setPageMargins(def->settings.marginsMm, QPageLayout::Millimeter);
        if (first) {
            if (!p.begin(&pdf)) return pages;
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            first = false;
        } else {
            pdf.newPage();
        }
        renderPageBody(model, p, def->settings);
        ++pages;
    }
    if (!first) p.end();
    return pages;
}

bool PrintEngine::print(SketchModel* model, const PrintSettings& s, QPrinter* printer) {
    if (!printer) return false;
    printer->setPageSize(s.pageSize);
    printer->setPageOrientation(s.orientation);
    printer->setPageMargins(s.marginsMm, QPageLayout::Millimeter);
    return render(model, printer, s);
}

} // namespace SketchCAD
