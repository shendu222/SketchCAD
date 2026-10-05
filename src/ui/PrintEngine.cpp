#include "PrintEngine.h"
#include "ElementPath.h"    // ElementPath::rawBounds（静态、视图无关、无头复用）
#include "core/TextMetrics.h"  // 文字度量单一数据源：取景与屏幕/打印三处同源
#include <QPdfWriter>
#include <QPrinter>
#include <QPainter>
#include <QTransform>
#include <QRectF>
#include <QPen>
#include <QStringList>
#include <QFileInfo>
#include <QDate>
#include <QTime>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QCoreApplication>
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
// frozenLayers: per-viewport 冻结名单（nullptr = 不冻结，历史行为）。
// rotationDeg:  视口内容旋转角（绕模型矩形锚点逆时针，度）；0 = 恒等（历史行为）。
void renderViewport(SketchModel* model, QPainter& p, const PrintSettings& s,
                    double pxPerMm, double tx, double ty, double tw, double th,
                    const QRectF& mb, double scaleMm, bool centerH, bool centerV,
                    const QStringList* frozenLayers = nullptr, double rotationDeg = 0.0) {
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
    // 视口旋转：绕世界锚点 (anchorX, anchorY) 旋转 rotationDeg（世界系逆时针）。
    // ⚠️ 直接构造合成矩阵，规避 QTransform::operator*/方法链的组合顺序歧义：
    //   世界点 w（dx=wx−ax, dy=wy−ay）先绕锚点旋转再走基础映射（含 y 翻转）：
    //     X = originX + sS·( cs·dx − sn·dy)  →  m11=sS·cs  m21=−sS·sn
    //     Y = originY − sS·( sn·dx + cs·dy)  →  m12=−sS·sn m22=−sS·cs
    //   θ=0 时逐系数退化为基础矩阵 t（恒等，零回归）。
    if (std::fabs(rotationDeg) > 1e-9) {
        const double rad = rotationDeg * (M_PI / 180.0);
        const double cs = std::cos(rad), sn = std::sin(rad);
        t = QTransform(scaleDev * cs, -scaleDev * sn,
                       -scaleDev * sn, -scaleDev * cs,
                       originX - scaleDev * (cs * anchorX - sn * anchorY),
                       originY + scaleDev * (sn * anchorX + cs * anchorY));
    }
    p.save();
    p.setTransform(t);

    // 目标区裁剪：设备矩形 → 世界矩形
    QTransform inv = t.inverted();
    const QRectF clipDev(tx * pxPerMm, ty * pxPerMm, tw * pxPerMm, th * pxPerMm);
    const QRectF clipWorld = inv.mapRect(clipDev);

    // 旋转视口：内容可能越出目标矩形（未旋转时映射天然贴合，历史行为无此问题），
    // 需裁剪防「旋出纸面/侵入邻视口」。⚠️ setTransform 已生效，clipDev 是设备坐标——
    // 直接 setClipRect 会被变换再次映射而裁错；先经 inv 映射回世界坐标（精确的旋转矩形）
    // 再 setClipPath，painter 变换后恰好还原为设备目标矩形。仅旋转时启用 → 未旋转路径逐字节不变。
    if (std::fabs(rotationDeg) > 1e-9) {
        QPolygonF devPoly;
        devPoly << QPointF(clipDev.left(),  clipDev.top())
                << QPointF(clipDev.right(), clipDev.top())
                << QPointF(clipDev.right(), clipDev.bottom())
                << QPointF(clipDev.left(),  clipDev.bottom());
        QPainterPath clipPath;
        clipPath.addPolygon(inv.map(devPoly));
        clipPath.closeSubpath();
        p.setClipPath(clipPath);
    }

    // 打印样式：黑白总开关 + 简化 CTB 映射在渲染前统一解析（见 effectivePlotStyle）
    PlotRenderer::renderModel(&p, model, scaleDev, clipWorld, s.effectivePlotStyle(), s.lineWidthScale, frozenLayers);
    p.restore();
}

// ---- 打印戳记（plot stamp）：页面边角信息栏；stamp.enabled=false 时零绘制调用 ----
// 无头环境（进程只有 QCoreApplication，如 SketchSelfTest）：QFontDatabase 不可用
// （drawText 会 qFatal，项目铁律「无头不依赖字体」，与 TextMetrics 同款处理）——
// 此时以确定性占位块绘制（宽 20mm × 行数×字高），位置/字高/开关语义不变、可测。
void drawPlotStamp(SketchModel* model, QPainter& p, const PrintSettings& s,
                   const QString& layoutName) {
    if (!s.stamp.enabled) return;
    const double pxPerMm = PrintEngine::devicePxPerMm(p.device());
    QStringList lines;
    const QString dp = model ? model->drawingPath() : QString();
    if (s.stamp.showDrawingName && !dp.isEmpty())
        lines << QFileInfo(dp).completeBaseName();
    if (s.stamp.showFileName && !dp.isEmpty())
        lines << dp;
    if (s.stamp.showLayoutName && !layoutName.isEmpty())
        lines << QStringLiteral("布局：%1").arg(layoutName);
    if (s.stamp.showScale) {
        if (s.fitToPage)                lines << QStringLiteral("比例：自适应");
        else if (s.scale >= 1.0 - 1e-9) lines << QStringLiteral("比例：%1:1").arg(s.scale, 0, 'g', 4);
        else                            lines << QStringLiteral("比例：1:%1").arg(1.0 / s.scale, 0, 'g', 4);
    }
    if (s.stamp.showDate) lines << QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    if (s.stamp.showTime) lines << QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    if (lines.isEmpty()) return;

    const double pageW = effectivePageW(s), pageH = effectivePageH(s);
    const double mL = s.marginsMm.left(), mT = s.marginsMm.top();
    const double mR = s.marginsMm.right(), mB = s.marginsMm.bottom();
    const double pad = 3.0 * pxPerMm;   // 距可打印区角 3mm
    const int pos = std::clamp(s.stamp.position, 0, 3);
    const bool left = (pos == 0 || pos == 2);
    const bool top  = (pos == 0 || pos == 1);
    const double anchorX = left ? (mL * pxPerMm + pad) : ((pageW - mR) * pxPerMm - pad);
    const double anchorY = top  ? (mT * pxPerMm + pad) : ((pageH - mB) * pxPerMm - pad);
    const double lineHpx = s.stamp.textHeightMm * pxPerMm;

    p.save();
    p.setPen(QPen(Qt::black));
    p.setBrush(Qt::NoBrush);

    // 无头占位块：进程未构造 QGuiApplication → 不触碰 QFontDatabase。
    // ⚠️ 不能用 QGuiApplication::instance() 判空——它对 QCoreApplication 也返回非空
    // （只是 instance() 的转型别名）；须 qobject_cast 判真实类型。
    if (qobject_cast<const QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
        const double w = 20.0 * pxPerMm, h = lines.size() * lineHpx;
        QRectF box(left ? anchorX : anchorX - w,
                   top  ? anchorY : anchorY - h, w, h);
        p.fillRect(box, Qt::black);
        p.restore();
        return;
    }

    QFont f = p.font();
    if (!s.stamp.fontFamily.isEmpty()) f.setFamily(s.stamp.fontFamily);
    f.setPixelSize(std::max(6, (int)std::lround(s.stamp.textHeightMm * pxPerMm)));
    const QFontMetricsF fm(f);
    const double lineH = fm.height();

    p.setFont(f);
    for (int i = 0; i < lines.size(); ++i) {
        const double y = top ? anchorY + i * lineH
                             : anchorY - (lines.size() - i) * lineH;
        const QRectF box(left ? anchorX : anchorX - 1e6, y, 1e6, lineH);
        p.drawText(box, (left ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, lines[i]);
    }
    p.restore();
}

// 单页渲染主体：边框 + 模型空间/视口布局 + 打印戳记。render() 与 exportPagesPdf 共用。
// layoutName: 布局名（戳记「布局」行消费；模型空间/未指名 → 空 = 省略该行）。
void renderPageBody(SketchModel* model, QPainter& p, const PrintSettings& s,
                    const QString& layoutName = QString()) {
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
        // 模型空间打印：整体包围盒 → 整块可打印区（历史行为不变）。
        // 空图纸不提前 return：戳记仍要叠加（空模型 + 戳记 = 唯一墨源）。
        const QRectF mb = PrintEngine::modelBounds(model);
        if (!mb.isNull() && mb.width() > 0 && mb.height() > 0) {
            const double fit = std::min(printW / mb.width(), printH / mb.height()) * 0.98;
            const double scaleMm = s.fitToPage ? fit : s.scale;
            renderViewport(model, p, s, pxPerMm, mL, mT, printW, printH,
                          mb, scaleMm, s.centerH, s.centerV);
        }
    } else {
        // 视口布局：每个视口映射到自身 paperRect（相对可打印区左上、y-down）；
        // paperRect 为空 → 整块可打印区（MVP 兼容行为不变）。
        // per-viewport 冻结/旋转：仅作用于本视口（其余视口与模型空间不受影响）。
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
            renderViewport(model, p, s, pxPerMm, tx, ty, tw, th, mb, scaleMm, true, true,
                           &vp.frozenLayers, vp.rotationDeg);
        }
    }

    // 打印戳记：最后叠加（设备坐标，不受视口变换影响）；关闭时零绘制调用
    drawPlotStamp(model, p, s, layoutName);
}

} // namespace

double PrintEngine::devicePxPerMm(QPaintDevice* dev) {
    const int dpi = dev ? dev->logicalDpiX() : 96;
    return dpi / 25.4;
}

// 取景与屏幕渲染、打印渲染三处同源：文本图元一律调用 TextMetrics::elementBoundsMm
// （含 9 点对齐、mtextWidth 真实折行、宽度因子），因此 fit-to-page 取景与最终出图
// 文字外接框逐字节一致。

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
        if (e.type == ElementType::Text || e.type == ElementType::MText) {
            grow(TextMetrics::elementBoundsMm(e, model)); // 三处同源：统一度量
            continue;
        }
        if (e.type == ElementType::BlockRef) {
            // 块引用：取展开子图元的并集（与 paintBlockRef 渲染范围同源）
            for (const auto& sub : model->expandBlockRef(e)) {
                if (sub.type == ElementType::BlockRef) continue;
                if (sub.type == ElementType::Text || sub.type == ElementType::MText) {
                    grow(TextMetrics::elementBoundsMm(sub, model));
                    continue;
                }
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
// 布局表为空 → 回落单页模型空间打印（fit-to-page），与历史单页出图行为等价（走 exportPdf
// 原路径逐字节不变），返回 1。多布局委托 exportPagesPdf（与 GUI 发布/COM 批量同一裁决）。
int PrintEngine::exportPdfLayouts(SketchModel* model, const QString& filePath) {
    if (!model) return 0;
    if (model->layoutCount() == 0) {
        PrintSettings s;          // 视口空 = 模型空间打印；fitToPage 默认 true
        s.viewports.clear();
        s.fitToPage = true;
        return exportPdf(model, s, filePath) ? 1 : 0;
    }
    QList<PublishPage> pages;
    for (int i = 0; i < model->layoutCount(); ++i) {
        const PlotLayoutDef* def = model->layoutAt(i);
        if (!def) continue;
        PublishPage pg;
        pg.name = def->name;
        pg.settings = def->settings;
        pages.append(pg);
    }
    return exportPagesPdf(model, pages, filePath,
                          QStringLiteral("SketchCAD 打印输出（全部布局）"));
}

// 页列表导出单个多页 PDF：每页独立纸张/方向/边距（QPdfWriter 在 newPage/首页落笔时
// 读取当前页面布局）。空页列表返回 0。
int PrintEngine::exportPagesPdf(SketchModel* model, const QList<PublishPage>& pages,
                                const QString& filePath, const QString& title) {
    if (!model || pages.isEmpty()) return 0;
    QPdfWriter pdf(filePath);
    pdf.setResolution(300);
    pdf.setTitle(title.isEmpty() ? QStringLiteral("SketchCAD 发布输出") : title);
    QPainter p;
    bool first = true;
    int done = 0;
    for (const PublishPage& pg : pages) {
        pdf.setPageSize(pg.settings.pageSize);
        pdf.setPageOrientation(pg.settings.orientation);
        pdf.setPageMargins(pg.settings.marginsMm, QPageLayout::Millimeter);
        if (first) {
            if (!p.begin(&pdf)) return done;
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            first = false;
        } else {
            pdf.newPage();
        }
        renderPageBody(model, p, pg.settings, pg.name);
        ++done;
    }
    if (!first) p.end();
    return done;
}

// 页列表发送物理打印机：每页独立页面设置（QPrinter 语义：页面参数对下一页生效），
// 页间 newPage。返回成功渲染的页数。
int PrintEngine::printPages(SketchModel* model, const QList<PublishPage>& pages,
                            QPrinter* printer) {
    if (!model || !printer || pages.isEmpty()) return 0;
    QPainter p;
    bool first = true;
    int done = 0;
    for (const PublishPage& pg : pages) {
        printer->setPageSize(pg.settings.pageSize);
        printer->setPageOrientation(pg.settings.orientation);
        printer->setPageMargins(pg.settings.marginsMm, QPageLayout::Millimeter);
        if (first) {
            if (!p.begin(printer)) return done;
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            first = false;
        } else {
            printer->newPage();
        }
        renderPageBody(model, p, pg.settings, pg.name);
        ++done;
    }
    if (!first) p.end();
    return done;
}

bool PrintEngine::print(SketchModel* model, const PrintSettings& s, QPrinter* printer) {
    if (!printer) return false;
    printer->setPageSize(s.pageSize);
    printer->setPageOrientation(s.orientation);
    printer->setPageMargins(s.marginsMm, QPageLayout::Millimeter);
    return render(model, printer, s);
}

} // namespace SketchCAD
