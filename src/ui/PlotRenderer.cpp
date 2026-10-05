#include "PlotRenderer.h"
#include "ElementPath.h"           // ElementPath::buildPath / rawBounds（静态、视图无关、无头复用）
#include "core/LinePattern.h"      // dashSpecFor / shouldFallbackToSolid / dashPhaseOffset
#include "core/Section.h"          // hatchRegionFromElement / hatchDrawMode / generateHatchLines / generateHatchDots
#include "core/Geometry.h"         // geo::Seg
#include "core/DimStyle.h"         // dim::build / dim::Style / dim::Geom / dim::Arrow
#include "core/Gdt.h"              // buildMLeader / buildFcf：多重引线与形位公差框格打印
#include "core/DimensionStandard.h"// DimensionStandardConfig / ArrowForm
#include "core/SheetSpec.h"        // standardTextHeightMm / currentSheetFormat
#include <QPainterPath>
#include "core/TextStyle.h"         // resolveTextStyle / resolveFields / wrapText
#include "core/TextMetrics.h"     // 文字度量单一数据源：屏幕/打印/取景三处同源
#include <QPen>
#include <QBrush>
#include <QPixmap>
#include <QImage>
#include <QTransform>
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
QPen makePen(const Element& e, double scaleDev, const ResolvedPlotStyle& rs, double lineWidthScale, double ltscale) {
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
        // LTSCALE：线素长度整体缩放（dashSpecScaled 同步缩放 pattern 与 minLengthD）
        const DashSpec spec = dashSpecScaled(e.lineStyle, ltscale);
        const double lenMm = worldLengthOf(e);
        if (spec.valid && lenMm >= spec.minLengthD * effW) {
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

// 灰度有序抖动（4×4 Bayer 矩阵）：为「已灰度化 + dither=true」的图元生成网点画刷。
// 黑白打印机/黑白 PDF 上，中间灰若无抖动会被压成纯黑或纯白，丢失剖面/填充层次；
// 抖动用「黑点密度 = 1 - gray」在白底上逼近目标灰度：gray=0 全黑、gray=1 全白。
// 网点间距按「设备像素」定义（tileDevicePx），再按 1/scaleDev 反缩放贴回世界坐标，
// 使网点在纸面上恒为固定物理尺寸，不随图幅缩放而变粗/变细。
QBrush makeDitherBrush(double gray, double scaleDev, int tileDevicePx = 4) {
    if (gray <= 0.001) return QBrush(Qt::black);
    if (gray >= 0.999) return QBrush(Qt::white);
    static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    const int n = std::clamp(tileDevicePx, 2, 8);
    QImage img(n, n, QImage::Format_RGB32);
    img.fill(qRgb(255, 255, 255));
    // 黑点数 ∝ (1 - gray)：gray 越大（越浅）→ 黑点越少 → 视觉越淡。
    const int onCount = int(std::lround((1.0 - gray) * n * n));
    for (int i = 0; i < n * n; ++i) {
        if (bayer[i % 16] * (n * n / 16) < onCount)
            img.setPixel(i % n, i / n, qRgb(0, 0, 0));
    }
    QPixmap pm = QPixmap::fromImage(img);
    QBrush br(pm);
    // 抵消世界变换：贴图 tile 以「设备像素」为单位，不随 scaleDev 放大。
    if (scaleDev > 1e-9) br.setTransform(QTransform::fromScale(1.0 / scaleDev, 1.0 / scaleDev));
    return br;
}

struct Ctx {
    double scaleDev = 1.0;        // 设备 px / 世界 mm
    double lineWidthScale = 0.5;  // 打印线宽比例（细一档），避免过粗失真
    SketchModel* model = nullptr;
    QRectF clip;                  // 世界坐标裁剪矩形（Xline 用）
    const PlotStyleTable* style = nullptr;
    double ltscale = 1.0;         // 全局线型比例（LTSCALE），随图元虚线长度缩放
    const QStringList* frozenLayers = nullptr;  // per-viewport 冻结（nullptr = 不冻结）
};

void paintHatch(QPainter* p, const Element& e, Ctx& c);
void paintGdt(QPainter* p, const Element& e, Ctx& c);   // 多重引线 / 形位公差框格（GD&T）
void paintBoundary(QPainter* p, const Element& e, Ctx& c);
void paintDimension(QPainter* p, const Element& e, Ctx& c);
void paintText(QPainter* p, const Element& e, Ctx& c);
void paintBlockRef(QPainter* p, const Element& e, Ctx& c);
void paintTable(QPainter* p, const Element& e, Ctx& c);

void paintOne(QPainter* p, const Element& e, Ctx& c) {
    if (e.type == ElementType::Constraint) return;   // 求解约束标记不打印
    // per-viewport 冻结：该视口内冻结层零输出（模型空间与其他视口不受影响）；
    // 块引用展开后逐子图元走 paintOne，冻结语义对块内同层子图元同样生效
    if (c.frozenLayers && c.frozenLayers->contains(e.layer)) return;
    // 图层动态语义：冻结层或关闭打印层 → 打印零输出（命中零、重算零、打印零）
    if (c.model) {
        const Layer* L = c.model->findLayer(e.layer);
        if (L && (L->frozen || !L->plot)) return;
    }
    // ByLayer 动态解析：与渲染侧共用 model->resolveElement，保证改层属性全层实时联动
    const Element& ee = (c.model ? c.model->resolveElement(e) : e);
    if (ee.type == ElementType::Dimension) { paintDimension(p, ee, c); return; }
    if (ee.type == ElementType::MLeader || ee.type == ElementType::Fcf) { paintGdt(p, ee, c); return; }
    if (ee.type == ElementType::Text)      { paintText(p, ee, c);      return; }
    if (ee.type == ElementType::Table)     { paintTable(p, ee, c);     return; }
    if (ee.type == ElementType::Hatch)     { paintHatch(p, ee, c);     return; }
    if (ee.type == ElementType::Boundary)  { paintBoundary(p, ee, c);  return; }
    if (ee.type == ElementType::BlockRef)  { paintBlockRef(p, ee, c);  return; }

    const ResolvedPlotStyle rs = c.style->resolve(ee);
    QPainterPath path;
    if (ee.type == ElementType::Xline)
        path = ElementPath::buildPath(ee, c.clip);     // 无限线：按页面裁剪
    else
        path = ElementPath::buildPath(ee);
    if (!path.isEmpty()) {
        p->setPen(makePen(ee, c.scaleDev, rs, c.lineWidthScale, c.ltscale));
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
    const QPainterPath path = ElementPath::buildPath(e);   // 外边界 + 内孔 (OddEvenFill)
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
    const QPainterPath path = ElementPath::buildPath(e);   // 外边界 + 内孔 (OddEvenFill)
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
        if (rs.gray && rs.dither) {
            // 黑白出图 + 抖动：按灰度生成网点画刷（灰度<1 时保留层次，不糊成纯黑）
            p->setBrush(makeDitherBrush(rs.grayValue, c.scaleDev));
        } else {
            p->setBrush(rs.color);
        }
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

// GD&T 打印：多重引线（实心箭头 + 引线 + 基线 + 文字）与形位公差框格（框线 + 符号 + 文字）。
// 线框/符号笔画统一走 ElementPath::buildPath（与渲染同源），此处补实心箭头与文字。
void paintGdt(QPainter* p, const Element& e, Ctx& c) {
    const ResolvedPlotStyle rs = c.style->resolve(e);
    const bool isLeader = (e.type == ElementType::MLeader);
    const MLeaderGeom ml = isLeader ? buildMLeader(e, 1.0) : MLeaderGeom();
    const FcfGeom     fg = (!isLeader) ? buildFcf(e, 1.0) : FcfGeom();

    // 线框 / 符号 / 引线 / 基线
    QPainterPath path = ElementPath::buildPath(e, c.clip);
    if (!path.isEmpty()) {
        p->setPen(makePen(e, c.scaleDev, rs, c.lineWidthScale, c.ltscale));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
    }

    // 多重引线实心箭头
    if (isLeader && ml.valid && !ml.leader.empty()) {
        const Vec3 tip = ml.arrowTip;
        Vec3 d = ml.leader.front().b - ml.leader.front().a;
        double L = std::hypot(d.x, d.y);
        if (L < 1e-9) { d = Vec3(1, 0, 0); L = 1.0; }
        d.x /= L; d.y /= L;
        const Vec3 base(tip.x + d.x * ml.arrowLen, tip.y + d.y * ml.arrowLen, tip.z);
        const Vec3 n(-d.y * ml.arrowWidth, d.x * ml.arrowWidth, tip.z);
        QPolygonF tri;
        tri << QPointF(tip.x, tip.y)
            << QPointF(base.x + n.x, base.y + n.y)
            << QPointF(base.x - n.x, base.y - n.y);
        p->setPen(Qt::NoPen);
        p->setBrush(rs.color);
        p->drawPolygon(tri);
        p->setBrush(Qt::NoBrush);
    }

    // 文字（多重引线多行 + 框格各格）
    const std::vector<GdtText>& texts = isLeader ? ml.texts : fg.texts;
    if (!texts.empty()) {
        p->save();
        p->setPen(rs.color);
        for (const auto& t : texts) {
            if (t.text.isEmpty()) continue;
            p->save();
            p->translate(t.anchor.x, t.anchor.y);
            p->scale(1.0 / c.scaleDev, -1.0 / c.scaleDev);
            QFont f = p->font();
            f.setPixelSize(std::max(6, (int)std::lround(t.height * c.scaleDev)));
            p->setFont(f);
            p->drawText(QPointF(0, 0), t.text);
            p->restore();
        }
        p->restore();
    }
}

void paintText(QPainter* p, const Element& e, Ctx& c) {
    if (e.pts.empty() || e.text.isEmpty()) return;
    const ResolvedPlotStyle rs = c.style->resolve(e);
    const ResolvedTextStyle ts = resolveTextStyle(e, c.model);
    const double th = ts.height;                            // 文字高度(mm)：样式×图元合并
    const bool isM = (e.type == ElementType::MText);

    // 字段（FIELD）占位解析
    const QString display = resolveFields(e.text, c.model);
    // 三处同源：用 TextMetrics 统一布局(mm)，再乘 scaleDev 映射到设备像素。
    const double mw = (isM && e.mtextWidth > 0.1) ? e.mtextWidth : 0.0;
    const TextLayout lay = TextMetrics::layout(ts, display, th, mw, isM ? e.lineSpacing : 1.0);

    p->save();
    p->translate(e.pts[0].x, e.pts[0].y);
    p->scale(1.0 / c.scaleDev, -1.0 / c.scaleDev);        // 抵消世界→设备变换的 Y 翻转
    QFont f = p->font();
    f.setPixelSize(std::max(6, (int)std::lround(th * c.scaleDev)));
    if (!ts.fontFamily.isEmpty()) f.setFamily(ts.fontFamily);
    f.setBold(ts.bold);
    f.setItalic(ts.italic);
    f.setStretch((int)std::lround(ts.widthFactor * 100));
    if (std::fabs(ts.obliquingAngle) > 1e-6)
        p->shear(std::tan(ts.obliquingAngle * M_PI / 180.0), 0.0);
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double hh = fm.height();
    const double lineHpx = lay.lineHeightMm * c.scaleDev;
    const int    align   = isM ? e.textAlign : 1;
    const int    hA = align % 3, vA = align / 3;
    const double maxWpx = lay.widthMm * c.scaleDev;
    const double totalHpx = lay.heightMm * c.scaleDev;
    const double startYv = (vA == 0) ? 0.0 : (vA == 1) ? totalHpx / 2.0 : totalHpx;
    p->setPen(rs.color);
    p->setBrush(Qt::NoBrush);
    for (int i = 0; i < lay.lines.size(); ++i) {
        const double wpx = lay.lines[i].widthMm * c.scaleDev;
        const double yc = startYv - (i + 0.5) * lineHpx;
        double x0;
        if (hA == 0)      x0 = 0;
        else if (hA == 1) x0 = -(maxWpx - wpx) / 2;
        else              x0 = -(maxWpx - wpx);
        const QRectF box(x0, yc - hh / 2, wpx, hh);
        p->drawText(box, Qt::AlignLeft | Qt::AlignVCenter, lay.lines[i].text);
    }
    p->restore();
}

// 表格图元（明细表）打印版：与画布 paintTable 同一几何（网格线 + 单元格文字），
// 但线宽走打印样式表（CTB）+ lineWidthScale，文字像素按 scaleDev 换算纸面 mm。
void paintTable(QPainter* p, const Element& e, Ctx& c) {
    if (e.pts.empty()) return;
    const ResolvedPlotStyle rs = c.style->resolve(e);
    const ResolvedTextStyle ts = resolveTextStyle(e, c.model);
    const int    R  = std::max(1, e.tableRows);
    const int    C  = std::max(1, e.tableCols);
    const double rh = std::max(0.5, e.rowHeight);
    const double th = e.radius > 0.1 ? e.radius : 3.5;

    p->save();
    p->translate(e.pts[0].x, e.pts[0].y);
    p->scale(1.0 / c.scaleDev, -1.0 / c.scaleDev);       // 抵消世界→设备变换的 Y 翻转

    // 列宽：显式 colWidths 优先，否则按真实度量自适应（与屏幕/取景同源）。
    const QVector<double> colW = TextMetrics::tableColumnWidths(e, c.model);
    QVector<double> xs(static_cast<int>(C) + 1);
    xs[0] = 0.0;
    for (int i = 0; i < C; ++i) xs[i + 1] = xs[i] + (i < colW.size() ? colW[i] : e.tableColWidth(i));
    const double totalW = xs[C];
    const double totalH = R * rh;

    QPen pen(rs.color);
    pen.setCosmetic(false);                              // 打印线宽 = 纸面 mm，不随视图缩放
    pen.setStyle(toPenStyle(e.lineStyle));
    const double rawW = rs.widthMm * c.lineWidthScale;
    pen.setWidthF(std::max(0.15, std::clamp(rawW, kMinPrintMm, kMaxPrintMm)));
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    for (int r = 0; r <= R; ++r)
        p->drawLine(QPointF(xs[0], r * rh), QPointF(totalW, r * rh));
    for (int i = 0; i <= C; ++i)
        p->drawLine(QPointF(xs[i], 0.0), QPointF(xs[i], totalH));

    // 单元格文字（字段占位打印时求值为快照）
    QFont f = p->font();
    f.setPixelSize(std::max(6, (int)std::lround(th * c.scaleDev)));
    if (!ts.fontFamily.isEmpty()) f.setFamily(ts.fontFamily);
    f.setBold(ts.bold);
    f.setItalic(ts.italic);
    f.setStretch((int)std::lround(ts.widthFactor * 100));
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double pad = 1.0;                              // 内边距 1mm（逻辑坐标）
    const double hh = fm.height();
    const int hA = std::clamp(e.textAlign, 0, 2);
    p->setPen(rs.color);
    ResolvedTextStyle crs;
    crs.fontFamily  = ts.fontFamily;
    crs.widthFactor = ts.widthFactor;
    crs.bold        = ts.bold;
    crs.italic      = ts.italic;
    crs.height      = th;
    for (int r = 0; r < R; ++r) {
        for (int i = 0; i < C; ++i) {
            const QString raw = e.tableCell(r, i);
            if (raw.trimmed().isEmpty()) continue;
            QString display = resolveFields(raw, c.model);
            const double cellL = xs[i] + pad, cellR = xs[i + 1] - pad;
            const double avail = cellR - cellL;
            if (TextMetrics::lineWidthMm(crs, display, th) > avail)
                display = TextMetrics::elideToWidthMm(crs, display, th, avail);
            const double tw = fm.horizontalAdvance(display);
            double x0;
            if (hA == 0)      x0 = cellL;
            else if (hA == 1) x0 = (cellL + cellR - tw) / 2.0;
            else              x0 = std::max(cellL, cellR - tw);
            const double yc = (r + 0.5) * rh;
            p->drawText(QRectF(x0, yc - hh / 2, tw, hh),
                        Qt::AlignLeft | Qt::AlignVCenter, display);
        }
    }
    p->restore();
}

} // namespace

void PlotRenderer::renderModel(QPainter* p, SketchModel* model, double scaleDev,
                              const QRectF& clipWorld, const PlotStyleTable& style,
                              double lineWidthScale, const QStringList* frozenLayers) {
    if (!p || !model) return;
    Ctx c{ scaleDev, lineWidthScale, model, clipWorld, &style };
    c.ltscale = model->ltscale();   // 注入全局线型比例（与渲染侧同源）
    c.frozenLayers = frozenLayers;  // per-viewport 冻结（nullptr = 历史行为）
    for (const auto& e : model->elements())
        paintOne(p, e, c);
}

void PlotRenderer::renderElement(QPainter* p, const Element& e, SketchModel* model,
                                double scaleDev, const QRectF& clipWorld,
                                const PlotStyleTable& style, double lineWidthScale) {
    Ctx c{ scaleDev, lineWidthScale, model, clipWorld, &style };
    c.ltscale = model ? model->ltscale() : 1.0;
    paintOne(p, e, c);
}

} // namespace SketchCAD
