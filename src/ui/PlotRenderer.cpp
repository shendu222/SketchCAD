#include "PlotRenderer.h"
#include "GraphicsView.h"          // ElementItem::buildPath / geometryBounds（静态、视图无关）
#include "core/LinePattern.h"      // dashSpecFor / shouldFallbackToSolid / dashPhaseOffset
#include "core/Section.h"          // hatchRegionFromElement / hatchDrawMode / generateHatchLines / generateHatchDots
#include "core/Geometry.h"         // geo::Seg
#include "core/DimStyle.h"         // dim::build / dim::Style / dim::Geom / dim::Arrow
#include "core/DimensionStandard.h"// DimensionStandardConfig / ArrowForm
#include "core/SheetSpec.h"        // standardTextHeightMm / currentSheetFormat
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFontMetricsF>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace {

inline QPointF qpt(const Vec3& v) { return QPointF(v.x, v.y); }

// 图元「代表长度」(世界 mm) —— 与 GraphicsView 中 worldLengthOf 一致，用于线型相位对齐
double worldLengthOf(const Element& e) {
    const double PI = 3.14159265358979323846;
    switch (e.type) {
        case ElementType::Line:
            return e.pts.size() >= 2 ? e.pts[0].distTo(e.pts[1]) : 0.0;
        case ElementType::Rectangle:
            if (e.pts.size() >= 2)
                return std::fabs(e.pts[1].x - e.pts[0].x) + std::fabs(e.pts[1].y - e.pts[0].y);
            return 0.0;
        case ElementType::Circle:   return 2.0 * PI * e.radius;
        case ElementType::Arc:      return PI * e.radius * std::fabs(e.endAngle - e.startAngle) / 180.0;
        case ElementType::Polyline:
        case ElementType::Polygon: {
            double L = 0.0; const int n = (int)e.pts.size();
            for (int i = 0; i + 1 < n; ++i) L += e.pts[i].distTo(e.pts[i + 1]);
            if (e.closed && n >= 2) L += e.pts[n - 1].distTo(e.pts[0]);
            return L;
        }
        default:
            return e.pts.size() >= 2 ? e.pts[0].distTo(e.pts[1]) : 0.0;
    }
}

// 打印线宽极值钳制（mm）：无论图元线宽多粗，打印时都不超过 kMaxPrintMm，避免糊成黑块；
// 下限保证极细线仍可见。kLineWidthScale（来自 PrintSettings.lineWidthScale，默认 0.5）先整体细一档。
constexpr double kMinPrintMm = 0.05;
constexpr double kMaxPrintMm = 0.8;

// 由图元 + 已解析样式推导画笔。
// ⚠️ 线宽语义（失真修复的核心）：打印画笔为非 cosmetic（setCosmetic(false)），其宽度单位是
// **逻辑坐标（世界 mm）**，QPainter 的世界→设备变换（scaleDev）会自动把它缩放一次 ——
// 纸面线宽恰好 = effW mm。**绝不能再乘 scaleDev**，否则纸面线宽 = effW×scaleDev²
// （300dpi/A4 适配下 0.5mm 线被放大成数毫米的墨团，且虚线 dash 同步拉长连成实带）。
// 屏幕渲染（ElementItem::penFor）用的是 cosmetic 笔 + 固定像素宽，因此两边单位语义不同、
// 各自正确；dash 图案（Qt 单位=线宽 d）在两种语义下均与几何保持同一比例。
QPen makePen(const Element& e, double scaleDev, const ResolvedPlotStyle& rs, double lineWidthScale) {
    (void)scaleDev;                               // 宽度走逻辑 mm，scaleDev 仅由变换承担
    QPen pen(rs.color);
    pen.setCosmetic(false);
    const double rawMm = rs.widthMm * lineWidthScale;        // 先整体缩放（默认细一档）
    const double effW  = std::clamp(rawMm, kMinPrintMm, kMaxPrintMm);  // 再极值钳制
    pen.setWidthF(effW);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (e.lineStyle == LineStyle::Wave || e.lineStyle == LineStyle::DoubleBreak) {
        pen.setStyle(Qt::SolidLine);             // 波浪/双折线由 buildPath 自定义路径绘制
        pen.setCapStyle(Qt::FlatCap);
    } else {
        const DashSpec spec = dashSpecFor(e.lineStyle);
        const double lenMm = worldLengthOf(e);
        if (spec.valid && !shouldFallbackToSolid(e.lineStyle, lenMm, effW)) {
            pen.setStyle(Qt::CustomDashLine);
            QVector<qreal> pat; pat.reserve((int)spec.pattern.size());
            for (double v : spec.pattern) pat.push_back(v);
            pen.setDashPattern(pat);
            pen.setDashOffset(dashPhaseOffset(lenMm, effW, spec));
            pen.setCapStyle(Qt::FlatCap);
        } else {
            const Qt::PenStyle ps = toPenStyle(e.lineStyle);
            pen.setStyle(ps);
            pen.setCapStyle(ps == Qt::SolidLine ? Qt::FlatCap : Qt::RoundCap);
        }
    }
    return pen;
}

struct Ctx {
    double scaleDev = 1.0;        // 设备 px / 世界 mm
    double lineWidthScale = 0.5;  // 打印线宽比例（细一档），避免过粗失真
    SketchModel* model = nullptr;
    QRectF clip;                  // 世界坐标裁剪矩形（Xline 用）
    const PlotStyleTable* style = nullptr;
};

void paintHatch(QPainter* p, const Element& e, Ctx& c);
void paintBoundary(QPainter* p, const Element& e, Ctx& c);
void paintDimension(QPainter* p, const Element& e, Ctx& c);
void paintText(QPainter* p, const Element& e, Ctx& c);
void paintBlockRef(QPainter* p, const Element& e, Ctx& c);

void paintOne(QPainter* p, const Element& e, Ctx& c) {
    if (e.type == ElementType::Constraint) return;   // 求解约束标记不打印
    if (e.type == ElementType::Dimension) { paintDimension(p, e, c); return; }
    if (e.type == ElementType::Text)      { paintText(p, e, c);      return; }
    if (e.type == ElementType::Hatch)     { paintHatch(p, e, c);     return; }
    if (e.type == ElementType::Boundary)  { paintBoundary(p, e, c);  return; }
    if (e.type == ElementType::BlockRef)  { paintBlockRef(p, e, c);  return; }

    const ResolvedPlotStyle rs = c.style->resolve(e);
    QPainterPath path;
    if (e.type == ElementType::Xline)
        path = ElementItem::buildPath(e, c.clip);     // 无限线：按页面裁剪
    else
        path = ElementItem::buildPath(e);
    if (!path.isEmpty()) {
        p->setPen(makePen(e, c.scaleDev, rs, c.lineWidthScale));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
    }
}

void paintBlockRef(QPainter* p, const Element& e, Ctx& c) {
    p->setBrush(Qt::NoBrush);
    if (!c.model) return;
    const auto subs = c.model->expandBlockRef(e);     // 展开块引用为世界坐标子图元
    for (const auto& sub : subs) {
        if (sub.type == ElementType::BlockRef) continue;
        paintOne(p, sub, c);                          // 子图元走同一套渲染
    }
}

// 交互式自定义边界（BOUNDARY/BPOLY）：打印时以虚线轮廓表示，不绘制剖面线
// （它只是边界定义，真正的剖面线由后续的剖面填充生成）。
void paintBoundary(QPainter* p, const Element& e, Ctx& c) {
    const QPainterPath path = ElementItem::buildPath(e);   // 外边界 + 内孔 (OddEvenFill)
    if (path.isEmpty()) return;
    const ResolvedPlotStyle rs = c.style->resolve(e);
    QPen pen(rs.color);
    pen.setCosmetic(false);
    pen.setStyle(Qt::DashLine);
    const double rawW = rs.widthMm * c.lineWidthScale;
    const double effW = std::clamp(rawW, kMinPrintMm, kMaxPrintMm);
    pen.setWidthF(std::max(0.15, effW));
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
}

void paintHatch(QPainter* p, const Element& e, Ctx& c) {
    const SectionRegion region = hatchRegionFromElement(e);
    const HatchDraw mode = hatchDrawMode(region);
    const QPainterPath path = ElementItem::buildPath(e);   // 外边界 + 内孔 (OddEvenFill)
    const ResolvedPlotStyle rs = c.style->resolve(e);

    QPen outlinePen(rs.color);
    outlinePen.setCosmetic(false);
    outlinePen.setStyle(Qt::SolidLine);
    const double rawW = rs.widthMm * c.lineWidthScale;
    const double effW = std::clamp(rawW, kMinPrintMm, kMaxPrintMm);
    outlinePen.setWidthF(std::max(0.15, effW));   // 逻辑 mm（世界变换单次缩放），外轮廓保留最小可见度

    QPen hatchPen = outlinePen;
    hatchPen.setWidthF(0.25);                     // 剖面线为细线（纸面 0.25mm，GB 细实线档）

    if (mode == HatchDraw::SolidFill) {                    // 狭小区域：涂黑代替剖面线
        p->setPen(outlinePen);
        p->setBrush(rs.color);
        if (!path.isEmpty()) p->drawPath(path);
        return;
    }
    // 先画外轮廓（含内孔）
    p->setPen(outlinePen);
    p->setBrush(Qt::NoBrush);
    if (!path.isEmpty()) p->drawPath(path);
    if (mode == HatchDraw::NoFill) return;                 // 豁免情形：仅画轮廓

    const auto segs = generateHatchLines(region);
    if (!segs.empty()) {
        p->save();
        p->setClipPath(path);
        p->setPen(hatchPen);
        p->setBrush(Qt::NoBrush);
        for (const auto& s : segs) p->drawLine(qpt(s.a), qpt(s.b));
        p->restore();
    }
    const auto dots = generateHatchDots(region);
    if (!dots.empty()) {
        p->save();
        p->setClipPath(path);
        p->setPen(Qt::NoPen);
        p->setBrush(rs.color);
        const double r = 0.3;   // 点半径 0.3mm（逻辑坐标，勿乘 scaleDev——否则双重缩放成巨点）
        for (const auto& d : dots) p->drawEllipse(qpt(d), r, r);
        p->restore();
    }
}

void paintDimension(QPainter* p, const Element& e, Ctx& c) {
    dim::Style st;
    st.stdConf = DimensionStandardConfig::forStandard(e.standard);
    st.drawingScale = c.model ? c.model->scaleRatio() : 1.0;   // 数字按实物尺寸（GB/T 14690）
    st.textHeight = standardTextHeightMm(currentSheetFormat(), false);
    if (e.dimTextHeight > 1e-6) st.textHeight = e.dimTextHeight;
    const dim::Geom g = dim::build(e, st);
    if (!g.valid) return;
    const ResolvedPlotStyle rs = c.style->resolve(e);

    QPen thin(rs.color);
    thin.setCosmetic(false);
    thin.setWidthF(0.25);   // 尺寸要素用细线：逻辑 mm，纸面恒 0.25mm（勿乘 scaleDev）
    thin.setStyle(Qt::SolidLine);
    p->setPen(thin);
    p->setBrush(Qt::NoBrush);

    for (const auto& s : g.ext)  p->drawLine(qpt(s.first), qpt(s.second));
    for (const auto& s : g.line) p->drawLine(qpt(s.first), qpt(s.second));
    if (g.arcPoly.size() >= 2) {
        QPolygonF poly;
        for (const auto& v : g.arcPoly) poly << qpt(v);
        p->drawPolyline(poly);
    }

    const ArrowForm af = st.arrowForm();
    for (const auto& a : g.arrows) {
        const double rad = a.dirDeg * M_PI / 180.0;
        const double cx = std::cos(rad), sx = std::sin(rad);
        const QPointF tip(a.tip.x, a.tip.y);
        const QPointF back(tip.x() - cx * st.arrowLen, tip.y() - sx * st.arrowLen);
        const QPointF off(-sx * st.arrowWidth * 0.5, cx * st.arrowWidth * 0.5);
        const QPointF left  = back + off;
        const QPointF right = back - off;

        if (e.dtype == DimensionType::Ordinate) {
            p->setBrush(thin.color());
            p->setPen(Qt::NoPen);
            p->drawEllipse(tip, st.arrowWidth * 0.35, st.arrowWidth * 0.35);
            continue;
        }
        switch (af) {
            case ArrowForm::ClosedFilled: {
                p->setBrush(thin.color()); p->setPen(Qt::NoPen);
                QPolygonF head; head << tip << left << right; p->drawPolygon(head); break;
            }
            case ArrowForm::ClosedUnfilled: {
                p->setBrush(Qt::NoBrush); p->setPen(thin);
                QPolygonF head; head << tip << left << right; p->drawPolygon(head); break;
            }
            case ArrowForm::Open30:
            case ArrowForm::Open90: {
                p->setBrush(Qt::NoBrush); p->setPen(thin);
                p->drawLine(left, tip); p->drawLine(right, tip); break;
            }
            case ArrowForm::ObliqueStroke: {
                p->setBrush(Qt::NoBrush); p->setPen(thin);
                const double lr = M_PI / 4.0;
                const double c45 = std::cos(rad + lr), s45 = std::sin(rad + lr);
                const QPointF p1(tip.x() - c45 * st.arrowLen * 0.5, tip.y() - s45 * st.arrowLen * 0.5);
                const QPointF p2(tip.x() + c45 * st.arrowLen * 0.5, tip.y() + s45 * st.arrowLen * 0.5);
                p->drawLine(p1, p2); break;
            }
            case ArrowForm::Dot: {
                p->setBrush(thin.color()); p->setPen(Qt::NoPen);
                p->drawEllipse(tip, st.arrowWidth * 0.4, st.arrowWidth * 0.4); break;
            }
        }
    }
    p->setBrush(Qt::NoBrush);
    p->setPen(thin);

    if (g.label.isEmpty()) return;
    QString fullText = g.label;
    if (!g.tolLabel.isEmpty()) fullText += g.tolLabel;

    // 文字：抵消世界→设备变换的 Y 翻转与缩放后按像素落字（与屏幕渲染同手法）
    p->save();
    p->translate(g.textPos.x, g.textPos.y);
    p->scale(1.0 / c.scaleDev, -1.0 / c.scaleDev);
    QFont f = p->font();
    f.setPixelSize(std::max(6, (int)std::lround(st.textHeight * c.scaleDev)));
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double tw = fm.horizontalAdvance(fullText), th = fm.height();
    const QRectF box(-tw / 2 - 1, -th / 2, tw + 2, th);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(245, 247, 250, 220));   // 数字下垫底色，保证可读
    p->drawRect(box);
    p->setBrush(Qt::NoBrush);
    p->setPen(thin.color());
    p->drawText(box, Qt::AlignCenter, fullText);
    p->restore();
}

void paintText(QPainter* p, const Element& e, Ctx& c) {
    if (e.pts.empty() || e.text.isEmpty()) return;
    const ResolvedPlotStyle rs = c.style->resolve(e);
    const double th = e.radius > 0.1 ? e.radius : 3.5;   // 文字高度(mm)
    p->save();
    p->translate(e.pts[0].x, e.pts[0].y);
    p->scale(1.0 / c.scaleDev, -1.0 / c.scaleDev);        // 抵消世界→设备变换的 Y 翻转
    QFont f = p->font();
    f.setPixelSize(std::max(6, (int)std::lround(th * c.scaleDev)));
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double tw = fm.horizontalAdvance(e.text), hh = fm.height();
    const QRectF box(0, -hh / 2, tw, hh);
    p->setPen(rs.color);
    p->setBrush(Qt::NoBrush);
    p->drawText(box, Qt::AlignLeft | Qt::AlignVCenter, e.text);
    p->restore();
}

} // namespace

void PlotRenderer::renderModel(QPainter* p, SketchModel* model, double scaleDev,
                              const QRectF& clipWorld, const PlotStyleTable& style,
                              double lineWidthScale) {
    if (!p || !model) return;
    Ctx c{ scaleDev, lineWidthScale, model, clipWorld, &style };
    for (const auto& e : model->elements())
        paintOne(p, e, c);
}

void PlotRenderer::renderElement(QPainter* p, const Element& e, SketchModel* model,
                                double scaleDev, const QRectF& clipWorld,
                                const PlotStyleTable& style, double lineWidthScale) {
    Ctx c{ scaleDev, lineWidthScale, model, clipWorld, &style };
    paintOne(p, e, c);
}

} // namespace SketchCAD
