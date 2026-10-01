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

// 将一块模型矩形(modelRect) 以给定比例(模型mm→图纸mm)映射到可打印区，
// 应用居中并裁剪到可打印区，调用 PlotRenderer 绘制。
void renderViewport(SketchModel* model, QPainter& p, const PrintSettings& s,
                    double pxPerMm, double mL, double mT, double printW, double printH,
                    const QRectF& mb, double scaleMm, bool centerH, bool centerV) {
    if (mb.isNull() || mb.width() <= 0 || mb.height() <= 0) return;
    const double scaleDev = scaleMm * pxPerMm;   // 世界 mm → 设备 px

    const double anchorX = centerH ? (mb.x() + mb.width() / 2.0)  : mb.x();
    const double anchorY = centerV ? (mb.y() + mb.height() / 2.0) : mb.y();
    const double originX = centerH ? ((mL + printW / 2.0) * pxPerMm) : (mL * pxPerMm);
    const double originY = centerV ? ((mT + printH / 2.0) * pxPerMm) : (mT * pxPerMm);

    QTransform t(scaleDev, 0, 0, -scaleDev,
                 originX - anchorX * scaleDev,
                 originY + anchorY * scaleDev);
    p.save();
    p.setTransform(t);

    // 可打印区裁剪：设备矩形 → 世界矩形
    QTransform inv = t.inverted();
    const QRectF clipDev(mL * pxPerMm, mT * pxPerMm, printW * pxPerMm, printH * pxPerMm);
    const QRectF clipWorld = inv.mapRect(clipDev);

    PlotRenderer::renderModel(&p, model, scaleDev, clipWorld, s.plotStyle, s.lineWidthScale);
    p.restore();
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

    const double pxPerMm = devicePxPerMm(dev);
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
        // 模型空间打印：整体包围盒 → 可打印区
        const QRectF mb = modelBounds(model);
        if (mb.isNull() || mb.width() <= 0 || mb.height() <= 0) return true;  // 空图纸：仅边框
        const double fit = std::min(printW / mb.width(), printH / mb.height()) * 0.98;
        const double scaleMm = s.fitToPage ? fit : s.scale;
        renderViewport(model, p, s, pxPerMm, mL, mT, printW, printH,
                      mb, scaleMm, s.centerH, s.centerV);
    } else {
        // 视口布局：每个视口独立映射到整块可打印区
        for (const auto& vp : s.viewports) {
            const QRectF mb = vp.modelRect.isNull() ? modelBounds(model) : vp.modelRect;
            const double fit = std::min(printW / mb.width(), printH / mb.height()) * 0.98;
            const double scaleMm = (vp.scale > 0) ? vp.scale : (s.fitToPage ? fit : s.scale);
            renderViewport(model, p, s, pxPerMm, mL, mT, printW, printH, mb, scaleMm, true, true);
        }
    }
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

bool PrintEngine::print(SketchModel* model, const PrintSettings& s, QPrinter* printer) {
    if (!printer) return false;
    printer->setPageSize(s.pageSize);
    printer->setPageOrientation(s.orientation);
    printer->setPageMargins(s.marginsMm, QPageLayout::Millimeter);
    return render(model, printer, s);
}

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
    o["plotStyle"]  = plotStyle.toJson();
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
    plotStyle.fromJson(o.value("plotStyle").toObject());
    viewports.clear();
    const QJsonArray vps = o.value("viewports").toArray();
    for (const auto& v : vps) viewports.append(PlotViewport::fromJson(v.toObject()));
}

PrintSettings PrintSettings::defaults() {
    PrintSettings s;
    s.viewports.append(PlotLayout::fullModelViewport());
    return s;
}

} // namespace SketchCAD
