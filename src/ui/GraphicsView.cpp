#include "GraphicsView.h"
#include "ElementPath.h"   // buildPath/rawBounds 已下沉（无头复用）
#include "dnd.h"
#include "tools/ToolManager.h"
#include "tools/ITool.h"
#include "core/Geometry.h"
#include "core/DimStyle.h"
#include "core/Trim.h"
#include "core/Section.h"
#include "core/LineWidthGroup.h"
#include "core/LinePattern.h"
#include "core/SheetSpec.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QMenu>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QFont>
#include <QVector>
#include <QDebug>
#include <QHash>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <cmath>
#include <algorithm>
#include "core/ElementFactory.h"
#include "core/TextStyle.h"
#include "core/CommandPrompt.h"   // 选项关键字匹配（C/闭合、U/放弃）
#include "core/ConstraintSolver.h"   // 过约束/冲突诊断（需求④）：diagnose 重算冲突集合
#include "core/ShapeSelect.h"        // 栏选/圈围/圈交判定（fence/wp/cp）

// ============================ 平移排查调试日志 ============================
// 右键拖不动时，把每一步鼠标/右键事件的现场写入 %TEMP%/sketchcad_pan.log，
// 由用户运行新 exe 后把日志内容发回即可定位：事件是否到达、panning_ 是否为真。
// 出完问题后可移除本段与下方 #ifdef PAN_DEBUG 处的打点（或保持，对运行几乎无影响）。
#define PAN_DEBUG
#ifdef PAN_DEBUG
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
static QFile& panDbgFile() {
    static QFile f(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                   + QStringLiteral("/sketchcad_pan.log"));
    static bool inited = false;
    if (!inited) {
        inited = true;
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            QTextStream(&f) << QStringLiteral("=== SketchCAD 平移排查日志 启动 %1 (日志位于 %2) ===\n")
                               .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
                               .arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    }
    return f;
}
static void panDbg(const QString& msg) {
    QFile& f = panDbgFile();
    if (f.isOpen()) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << " " << msg << "\n";
        ts.flush();
    }
}
#else
static inline void panDbg(const QString&) {}
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {

static QPointF qpt(const Vec3& v) { return QPointF(v.x, v.y); }
static Vec3 toVec3(const QPointF& q) { return Vec3(q.x(), q.y()); }

// 图元「代表长度」(世界 mm)：用于 GB/T 14665-2012 4.2 线素最小长度与 4.4 相位对齐判定。
static double worldLengthOf(const Element& e) {
    const double PI = 3.14159265358979323846;
    switch (e.type) {
        case ElementType::Line:
            return e.pts.size() >= 2 ? e.pts[0].distTo(e.pts[1]) : 0.0;
        case ElementType::Rectangle:
            if (e.pts.size() >= 2)
                return std::fabs(e.pts[1].x - e.pts[0].x) + std::fabs(e.pts[1].y - e.pts[0].y);
            return 0.0;
        case ElementType::Circle:
            return 2.0 * PI * e.radius;
        case ElementType::Arc:
            return PI * e.radius * std::fabs(e.endAngle - e.startAngle) / 180.0;
        case ElementType::Polyline:
        case ElementType::Polygon: {
            double L = 0.0;
            const int n = (int)e.pts.size();
            for (int i = 0; i + 1 < n; ++i) L += e.pts[i].distTo(e.pts[i + 1]);
            if (e.closed && n >= 2) L += e.pts[n - 1].distTo(e.pts[0]);
            return L;
        }
        default:
            return e.pts.size() >= 2 ? e.pts[0].distTo(e.pts[1]) : 0.0;
    }
}

static const QColor GRID_MINOR(244, 182, 182); // 淡红 F4B6B6
static const QColor GRID_MAJOR(230, 138, 138); // 略深淡红 E68A8A
static const QColor AXIS_X(200, 60, 60);
static const QColor AXIS_Y(40, 150, 80);
static const QColor HL(31, 95, 139);           // 选中高亮蓝
static const QColor PREVIEW(31, 95, 139, 215);
static const QColor GUIDE(120, 140, 170, 170);
static const QColor WARN(214, 48, 49);     // 过约束/冲突高亮红

// 构造线(Xline)的兜底裁剪范围 —— 与 GraphicsView 构造里的
// scene_->setSceneRect(QRectF(-1e5,-1e5,2e5,2e5)) 保持一致。
// 无限直线先与此矩形求交得到「有界长线段」，交给场景索引与光栅器；
// 绘制阶段再按当前可见区域二次裁剪，实现「按视图边界裁剪显示、缩放平移同步更新」。
static const QRectF kXlineBigRect(-1e5, -1e5, 2e5, 2e5);

// ============================ ElementItem ============================

double ElementItem::viewZoom() const {
    if (scene() && !scene()->views().isEmpty()) {
        const double z = std::fabs(scene()->views().first()->transform().m11());
        if (z > 1e-9) return z;
    }
    return 4.0;
}

// 当前视图可见区域（世界坐标）。构造线用它做「按视图边界裁剪显示」。
QRectF ElementItem::visibleWorldRect() const {
    if (!scene() || scene()->views().isEmpty()) return QRectF();
    const QGraphicsView* v = scene()->views().first();
    if (!v->viewport()) return QRectF();
    return v->mapToScene(v->viewport()->rect()).boundingRect();
}

// 无限直线与矩形求交：算法已下沉到 ElementPath（视图无关，供打印引擎 / COM 自动化无头复用）
bool ElementItem::xlineClipToRect(const Element& e, const QRectF& r, Vec3& a, Vec3& b) {
    return ElementPath::xlineClipToRect(e, r, a, b);
}

// 把一个图元转成绘制路径（样条在这里被展开成三次贝塞尔）。
// 算法已下沉到 ElementPath（视图无关，供打印引擎 / COM 自动化无头复用）。
QPainterPath ElementItem::buildPath(const Element& e, const QRectF& xlineClip) {
    return ElementPath::buildPath(e, xlineClip);
}

// 图元的原始几何包围盒：算法已下沉到 ElementPath。
// model 语义：null → 无样式宽度因子、块引用不展开（与原临时 item 一致）。
QRectF ElementItem::rawBounds(const Element& e) const {
    return ElementPath::rawBounds(e, model);
}

QRectF ElementItem::boundingRect() const {
    // 构造线：包围盒 = 无限直线 ∩ 场景级大矩形（恒定、不随视图变化），保证平移/缩放时
    // 图元永不被场景索引剔除；实际绘制路径在 paint 阶段按当前可见区域二次裁剪。
    if (el.type == ElementType::Xline) {
        Vec3 a, b;
        QRectF r;
        if (xlineClipToRect(el, kXlineBigRect, a, b))
            r = QRectF(qpt(a), qpt(b)).normalized();
        else if (!el.pts.empty())
            r = QRectF(qpt(el.pts[0]), qpt(el.pts[0]));
        const double m = (std::max(6.0, mmToScreenPx(el.effectiveLineWidth()) + 4.0)) / viewZoom();
        return r.adjusted(-m, -m, m, m);
    }
    QRectF r = rawBounds(el);
    for (const auto& g : guides) r = r.united(rawBounds(g));
    // cosmetic 画笔的描边宽度以像素计，换算成场景单位后再留一点余量
    const double m = (std::max(6.0, mmToScreenPx(el.effectiveLineWidth()) + 4.0)) / viewZoom();
    return r.adjusted(-m, -m, m, m);
}

QPainterPath ElementItem::shape() const {
    const double z = viewZoom();
    // 文本 / 多行文字 / 表格：包围盒是矩形，直接返回矩形路径作为命中区域（比仅锚点更好点）
    if (el.type == ElementType::Text || el.type == ElementType::MText ||
        el.type == ElementType::Table) {
        QPainterPath d;
        QRectF r = rawBounds(el);
        d.addRect(r);
        return d;
    }
    if (el.type == ElementType::Hatch) {
        // 剖面区域以「填充区域 ∪ 边界描边」作为命中范围：点击内部或轮廓均可选中
        QPainterPath p = buildPath(el);
        QPainterPathStroker st;
        st.setWidth(std::max(mmToScreenPx(el.effectiveLineWidth()) + 2.0, 5.0) / z);
        return p.united(st.createStroke(p));
    }
    if (el.type == ElementType::BlockRef) {
        // 块引用以「展开后所有子图元路径的并 ∪ 描边」作为命中范围：
        // 点块内任一图形都能选中整个块（与 AutoCAD 选块一致）
        const QPainterPath p = blockPath(el);
        if (p.isEmpty()) {
            QPainterPath d;
            if (!el.pts.empty()) d.addEllipse(qpt(el.pts[0]), 7.0 / z, 7.0 / z);
            return d;
        }
        QPainterPathStroker st;
        st.setWidth(std::max(mmToScreenPx(el.effectiveLineWidth()), 7.0) / z);
        return p.united(st.createStroke(p));
    }
    QPainterPath p = buildPath(el);
    if (p.isEmpty()) {
        QPainterPath d;
        if (!el.pts.empty()) d.addEllipse(qpt(el.pts[0]), 7.0 / z, 7.0 / z);
        return d;
    }
    QPainterPathStroker st;
    st.setWidth(std::max(mmToScreenPx(el.effectiveLineWidth()), 7.0) / z);
    return st.createStroke(p);
}

void ElementItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    p->setBrush(Qt::NoBrush);

    // 构造辅助线（虚线细线）：让「圆心+半径」「三点定圆」「圆弧扫掠」的几何关系在拖动时可见
    if (!guides.empty()) {
        QPen gp(GUIDE);
        gp.setCosmetic(true);
        gp.setWidthF(1.0);
        gp.setStyle(Qt::DashLine);
        p->setPen(gp);
        for (const auto& g : guides) paintOne(p, g);
    }

    p->setPen(penFor(el));
    paintOne(p, el);
}

// 由图元推导画笔。预览固定虚线；正式图元按 GB/T 14665-2012 4.2 用线素公式设置
// dash pattern（单位 = 线宽 d），并按 4.4 相位对齐；短于最小长度回落细实线；
// 波浪线/双折线由 buildPath 自定义路径绘制，这里用实线笔（避免叠加虚线）。
// 抽取为独立函数：块引用内的子图元复用同一套笔参数，观感与独立图元完全一致。
QPen ElementItem::penFor(const Element& e) const {
    QPen pen(preview ? PREVIEW : (warn ? WARN : (highlight ? HL : e.color)));
    pen.setCosmetic(true);                       // 线宽按屏幕像素固定，缩放不改变视觉粗细
    const double effW = e.effectiveLineWidth();  // 图纸 mm（自动=当前线组粗/细档；手动=覆盖值）
    double wpx  = mmToScreenPx(std::max(0.05, effW));  // 换算为屏幕像素
    if (!showLineWeight) wpx = 1.0;              // 线宽显示关（LWDISPLAY）：一律细线
    pen.setWidthF(preview ? std::max(1.0, wpx)
                          : (warn ? wpx + 2.0 : (highlight ? wpx + 1.2 : wpx)));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (preview) {
        pen.setStyle(Qt::DashLine);
    } else if (e.lineStyle == LineStyle::Wave || e.lineStyle == LineStyle::DoubleBreak) {
        pen.setStyle(Qt::SolidLine);
        pen.setCapStyle(Qt::FlatCap);
    } else {
        const DashSpec spec = dashSpecFor(e.lineStyle);
        const double lenMm = worldLengthOf(e);
        if (spec.valid && !shouldFallbackToSolid(e.lineStyle, lenMm, effW)) {
            pen.setStyle(Qt::CustomDashLine);
            QVector<qreal> pat;
            pat.reserve((int)spec.pattern.size());
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

// 块引用：把引用按块表展开为世界坐标子图元（无模型 / 未定义块 → 空）
std::vector<Element> ElementItem::expandedBlock() const {
    if (!model || el.type != ElementType::BlockRef) return {};
    return model->expandBlockRef(el);
}

// 块引用的命中/取景路径：展开后各子图元路径的并（文本以矩形代替其字形路径）
QPainterPath ElementItem::blockPath(const Element& e) const {
    QPainterPath total;
    (void)e;
    for (const auto& sub : expandedBlock()) {
        if (sub.type == ElementType::BlockRef) continue;
        if (sub.type == ElementType::Text) { total.addRect(rawBounds(sub)); continue; }
        const QPainterPath sp = buildPath(sub);
        if (!sp.isEmpty()) total.addPath(sp);
    }
    return total;
}

// 绘制块引用：逐个子图元复用 paintOne（即"独立图元怎么画，块内就怎么画"）
void ElementItem::paintBlockRef(QPainter* p, const Element&) const {
    p->setBrush(Qt::NoBrush);
    for (const auto& sub : expandedBlock()) {
        if (sub.type == ElementType::BlockRef) continue;   // 展开即递归，理论上不出现
        p->setPen(penFor(sub));
        paintOne(p, sub);
    }
}

void ElementItem::paintOne(QPainter* p, const Element& e) const {
    const double z = viewZoom();

    if (e.type == ElementType::Constraint) {
        if (e.pts.empty()) return;
        const double r = 6.0 / z;
        p->drawEllipse(qpt(e.pts[0]), r, r);
        p->drawLine(QPointF(e.pts[0].x - r, e.pts[0].y), QPointF(e.pts[0].x + r, e.pts[0].y));
        return;
    }

    if (e.type == ElementType::Dimension) { paintDimension(p, e); return; }
    if (e.type == ElementType::Text || e.type == ElementType::MText) { paintText(p, e); return; }
    if (e.type == ElementType::Table)     { paintTable(p, e); return; }
    if (e.type == ElementType::Hatch)     { paintHatch(p, e); return; }
    if (e.type == ElementType::Boundary)  { paintBoundary(p, e); return; }
    if (e.type == ElementType::BlockRef)  { paintBlockRef(p, e); return; }
    if (e.type == ElementType::Point) {
        if (e.pts.empty()) return;
        const double r = 3.5 / z;                 // 屏幕恒定 3.5px，任何缩放下清晰可见
        const QPointF c = qpt(e.pts[0]);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(e.color, 1.2 / z, Qt::SolidLine, Qt::RoundCap));
        p->drawEllipse(c, r, r);                  // 点：小圆 + 十字准星，强调「节点」语义
        p->drawLine(QPointF(c.x() - 2 * r, c.y()), QPointF(c.x() + 2 * r, c.y()));
        p->drawLine(QPointF(c.x(), c.y() - 2 * r), QPointF(c.x(), c.y() + 2 * r));
        return;
    }

    QPainterPath path;
    if (e.type == ElementType::Xline)
        path = buildPath(e, visibleWorldRect());   // 无限线：每帧按当前可见区域裁剪
    else
        path = buildPath(e);
    if (!path.isEmpty()) p->drawPath(path);
}

// 剖面区域绘制（GB/T 4457.5-2013）：
//   Hatch     → 细实线剖面线（间隔相等、方向一致），裁剪到区域含内孔
//   SolidFill → 涂黑代替剖面线（狭小区域）
//   NoFill    → 不画剖面符号（相邻辅助零件 / 标准件 / 肋轮辐 / 实心轴纵向剖切），仅画轮廓
void ElementItem::paintHatch(QPainter* p, const Element& e) const {
    const SectionRegion region = hatchRegionFromElement(e);
    const HatchDraw mode = hatchDrawMode(region);
    const QPainterPath path = buildPath(e);   // 外边界 + 内孔，OddEvenFill

    // 外轮廓（剖切边界）线宽 = 图元自身线宽（填充后保留原粗实线，不变细）
    QPen outlinePen(highlight ? HL : e.color);
    outlinePen.setCosmetic(true);
    outlinePen.setStyle(Qt::SolidLine);
    const double effW = std::max(0.6, mmToScreenPx(e.effectiveLineWidth()));
    outlinePen.setWidthF(highlight ? effW + 1.0 : effW);

    // 剖面线（斜线族）为细实线（GB/T 4457.4 细线）；固定细线宽，不随外轮廓变粗
    QPen hatchPen = outlinePen;
    hatchPen.setWidthF(0.6);

    if (mode == HatchDraw::SolidFill) {
        // 狭小区域：涂黑代替剖面线（4.5 允许涂色/点阵代替剖面符号）
        p->setPen(outlinePen);
        p->setBrush(e.color);
        if (!path.isEmpty()) p->drawPath(path);
        return;
    }

    // 先画外轮廓（区域边界，含内孔轮廓）——线宽取原图元线宽
    p->setPen(outlinePen);
    p->setBrush(Qt::NoBrush);
    if (!path.isEmpty()) p->drawPath(path);

    if (mode == HatchDraw::NoFill) return;   // 豁免情形：不画剖面符号

    // 正常剖面线：裁剪到区域（内孔自动掏空）后逐条绘制
    const auto segs = generateHatchLines(region);
    if (!segs.empty()) {
        p->save();
        p->setClipPath(path);
        p->setPen(hatchPen);
        p->setBrush(Qt::NoBrush);
        for (const auto& s : segs)
            p->drawLine(qpt(s.a), qpt(s.b));
        p->restore();
    }

    // 点阵符号（玻璃/液体/混凝土）：区域内点阵
    const auto dots = generateHatchDots(region);
    if (!dots.empty()) {
        p->save();
        p->setClipPath(path);
        p->setPen(Qt::NoPen);
        p->setBrush(e.color);
        const double r = 0.9;   // 点半径（屏幕像素，cosmetic）
        for (const auto& d : dots)
            p->drawEllipse(qpt(d), r, r);
        p->restore();
    }
}

// 交互式自定义边界（BOUNDARY/BPOLY）：以虚线轮廓表示"用户指定的填充边界"，
// 不绘制剖面线（它只是边界定义，真正的剖面线由其后触发的剖面填充生成）。
// 虚线用以与已填充的实线 Hatch 在视觉上区分；高亮态加粗、改高亮色。
void ElementItem::paintBoundary(QPainter* p, const Element& e) const {
    const QPainterPath path = buildPath(e);   // 外边界 + 内孔，OddEvenFill
    if (path.isEmpty()) return;
    QPen pen(highlight ? HL : e.color);
    pen.setCosmetic(true);
    pen.setStyle(Qt::DashLine);
    const double effW = std::max(0.6, mmToScreenPx(e.effectiveLineWidth()));
    pen.setWidthF(highlight ? effW + 1.0 : effW);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
}

// GB/T 4458.4-2003 / ISO 129-1:2018：尺寸界线与尺寸线用细实线，
// 箭头形式、文字方向、公差注写按 Element::standard 自动切换。
void ElementItem::paintDimension(QPainter* p, const Element& e) const {
    dim::Style st;
    st.stdConf = DimensionStandardConfig::forStandard(e.standard);
    st.drawingScale = dimScale;          // 数字按实物尺寸（GB/T 14690）
    // T6：数字字高随当前图纸幅面派生（GB/T 14665-2012 5.6 表3：A0/A1→5，A2/A3/A4→3.5）
    st.textHeight = standardTextHeightMm(currentSheetFormat(), false);
    if (e.dimTextHeight > 1e-6) st.textHeight = e.dimTextHeight;  // 单图元字高覆盖（如各制图模板）
    const dim::Geom g = dim::build(e, st);
    if (!g.valid) return;
    const double z = viewZoom();

    // 细实线：尺寸要素用细实线，线宽约为粗实线的 1/2，不跟随图元 lineWidth 变粗。
    QPen thin = p->pen();
    thin.setCosmetic(true);
    thin.setWidthF(std::max(1.0, p->pen().widthF() * 0.5));
    thin.setStyle(Qt::SolidLine);        // 即便处于预览虚线状态，尺寸线本身也必须是实线
    p->setPen(thin);
    p->setBrush(Qt::NoBrush);

    for (const auto& s : g.ext)  p->drawLine(qpt(s.first), qpt(s.second));
    for (const auto& s : g.line) p->drawLine(qpt(s.first), qpt(s.second));
    if (g.arcPoly.size() >= 2) {
        QPolygonF poly;
        for (const auto& v : g.arcPoly) poly << qpt(v);
        p->drawPolyline(poly);
    }

    // 按所选标准绘制尺寸线终端（箭头 / 斜线 / 圆点）
    const ArrowForm af = st.arrowForm();
    for (const auto& a : g.arrows) {
        const double rad = a.dirDeg * M_PI / 180.0;
        const double cx = std::cos(rad), sx = std::sin(rad);
        const QPointF tip(a.tip.x, a.tip.y);
        // 尾端中心：沿反方向退 arrowLen
        const QPointF back(tip.x() - cx * st.arrowLen, tip.y() - sx * st.arrowLen);
        // 尾端左右偏移（垂直于箭头方向）
        const QPointF off(-sx * st.arrowWidth * 0.5, cx * st.arrowWidth * 0.5);
        const QPointF left  = back + off;
        const QPointF right = back - off;

        if (e.dtype == DimensionType::Ordinate) {
            // 坐标标注：在要素处画一个原点符号（小实心圆点）
            p->setBrush(thin.color());
            p->setPen(Qt::NoPen);
            p->drawEllipse(tip, st.arrowWidth * 0.35, st.arrowWidth * 0.35);
            continue;
        }

        switch (af) {
            case ArrowForm::ClosedFilled: {
                p->setBrush(thin.color());
                p->setPen(Qt::NoPen);
                QPolygonF head; head << tip << left << right;
                p->drawPolygon(head);
                break;
            }
            case ArrowForm::ClosedUnfilled: {
                p->setBrush(Qt::NoBrush);
                p->setPen(thin);
                QPolygonF head; head << tip << left << right;
                p->drawPolygon(head);
                break;
            }
            case ArrowForm::Open30:
            case ArrowForm::Open90: {
                p->setBrush(Qt::NoBrush);
                p->setPen(thin);
                p->drawLine(left, tip);
                p->drawLine(right, tip);
                break;
            }
            case ArrowForm::ObliqueStroke: {
                p->setBrush(Qt::NoBrush);
                p->setPen(thin);
                // 斜线终端：过 tip 画一条与尺寸线成 45° 的短划线
                const double lr = M_PI / 4.0;
                const double c45 = std::cos(rad + lr), s45 = std::sin(rad + lr);
                const QPointF p1(tip.x() - c45 * st.arrowLen * 0.5, tip.y() - s45 * st.arrowLen * 0.5);
                const QPointF p2(tip.x() + c45 * st.arrowLen * 0.5, tip.y() + s45 * st.arrowLen * 0.5);
                p->drawLine(p1, p2);
                break;
            }
            case ArrowForm::Dot: {
                p->setBrush(thin.color());
                p->setPen(Qt::NoPen);
                p->drawEllipse(tip, st.arrowWidth * 0.4, st.arrowWidth * 0.4);
                break;
            }
        }
    }
    p->setBrush(Qt::NoBrush);
    p->setPen(thin);

    if (g.label.isEmpty()) return;

    // 主数字 + 公差注写合并为一段文本（公差作为后缀或换行）
    QString fullText = g.label;
    if (!g.tolLabel.isEmpty()) fullText += g.tolLabel;

    // 文字：抵消视图 Y 翻转与缩放后按像素落字；字高按 3.5mm 换算成像素。
    // 旋转要在翻转之后施加，负号是因为翻转后局部系变成了 Y 向下。
    p->save();
    p->translate(g.textPos.x, g.textPos.y);
    p->scale(1.0 / z, -1.0 / z);
    p->rotate(-g.textRotDeg);
    QFont f = p->font();
    f.setPixelSize(std::max(8, (int)std::lround(st.textHeight * z)));
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double tw = fm.horizontalAdvance(fullText), th = fm.height();
    const QRectF box(-tw / 2 - 1, -th / 2, tw + 2, th);
    // 数字下垫一层底色：尺寸线穿过数字时保证可读（等效国标「尺寸线断开」）
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(245, 247, 250, 220));
    p->drawRect(box);
    p->setBrush(Qt::NoBrush);
    p->setPen(thin.color());
    p->drawText(box, Qt::AlignCenter, fullText);
    p->restore();
}

// 文本图元（标题栏栏目、说明文字等）：在锚点(pts[0])以左对齐、垂直居中的方式落字。
// 复用 Dimension 的「抵消视图 Y 翻转 + 缩放后按像素落字」手法，保证文字方向正确且随缩放变化。
// 文字高度(mm)存于 e.radius；pts[0] 为文字左下锚点（绘制区域 x∈[pts.x, pts.x+w], y 以 pts.y 为中心）。
void ElementItem::paintText(QPainter* p, const Element& e) const {
    if (e.pts.empty() || e.text.isEmpty()) return;
    const double z = viewZoom();
    const double th = e.radius > 0.1 ? e.radius : 3.5;   // 文字高度(mm)
    const bool isM = (e.type == ElementType::MText);

    // 样式驱动字体参数（STYLE 表 + 图元字段 合并）
    const ResolvedTextStyle rs = resolveTextStyle(e, model);

    p->save();
    p->translate(e.pts[0].x, e.pts[0].y);
    p->scale(1.0 / z, -1.0 / z);                          // 抵消视图 Y 轴翻转（与 Dimension 一致）
    QFont f = p->font();
    f.setPixelSize(std::max(8, (int)std::lround(th * z)));
    if (!rs.fontFamily.isEmpty()) f.setFamily(rs.fontFamily);
    f.setBold(rs.bold);
    f.setItalic(rs.italic);
    f.setStretch((int)std::lround(rs.widthFactor * 100)); // 宽度因子（样式×图元）
    p->setFont(f);
    // 倾斜（obliquingAngle）：用水平错切近似（Qt 无直接倾斜角 API）
    if (std::fabs(rs.obliquingAngle) > 1e-6)
        p->shear(std::tan(rs.obliquingAngle * M_PI / 180.0), 0.0);

    const QFontMetricsF fm(f);

    // 字段（FIELD）占位解析：%<CODE>% → 当前值（纯文本无占位则原样返回）
    const QString display = resolveFields(e.text, model);

    // 折行：MText 且指定了 mtextWidth 时按宽度自动折行，否则仅按显式 '\n' 分段
    QStringList lines;
    if (isM && e.mtextWidth > 0.1)
        lines = wrapText(display, e.mtextWidth, f);
    else
        lines = display.split('\n');

    const double lineHpx = th * z * (isM ? e.lineSpacing : 1.0);
    const int    align   = isM ? e.textAlign : 1;          // Text 维持原「中上」锚定
    const int    hA = align % 3, vA = align / 3;           // 0左/1中/右 ; 0上/1中/2下
    double maxW = 0;
    QVector<double> widths(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        widths[i] = fm.horizontalAdvance(lines[i]);
        if (widths[i] > maxW) maxW = widths[i];
    }
    const double totalH = lines.size() * lineHpx;
    const double startYv = (vA == 0) ? 0.0 : (vA == 1) ? totalH / 2.0 : totalH;  // 区块上沿(局部 y 向上)

    p->setPen(e.color);
    p->setBrush(Qt::NoBrush);
    const double hh = fm.height();
    for (int i = 0; i < lines.size(); ++i) {
        const double yc = startYv - (i + 0.5) * lineHpx;    // 行中心（局部 y 向上）
        double x0;
        if (hA == 0)      x0 = 0;
        else if (hA == 1) x0 = -(maxW - widths[i]) / 2;
        else              x0 = -(maxW - widths[i]);
        const QRectF box(x0, yc - hh / 2, widths[i], hh);
        p->drawText(box, Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }
    p->restore();
}

// 表格图元（明细表）：网格线（颜色/线型/线宽走图元通用样式）+ 单元格文字（radius 字高、
// textAlign 水平对齐 0=左/1=中/2=右，垂直一律居中，左右各留 1mm 内边距）。
// 坐标系：translate 到左上锚点后 scale(1/z, -1/z) 抵消视图 Y 翻转 —— 局部坐标 y 向下为正，
// 行 r 的顶边 y = r*rowHeight，与模型「行向下增长」的约定一致（打印版见 PlotRenderer::paintTable）。
void ElementItem::paintTable(QPainter* p, const Element& e) const {
    if (e.pts.empty()) return;
    const double z = viewZoom();
    const int    R  = std::max(1, e.tableRows);
    const int    C  = std::max(1, e.tableCols);
    const double rh = std::max(0.5, e.rowHeight);
    const double th = e.radius > 0.1 ? e.radius : 3.5;   // 单元格文字字高(mm)

    p->save();
    p->translate(e.pts[0].x, e.pts[0].y);
    p->scale(1.0 / z, -1.0 / z);                          // 抵消视图 Y 轴翻转 → 局部 y 向下为正

    // ---- 网格线：列边界累加 colWidths（缺项/非正回落 30mm）----
    QVector<double> xs(static_cast<int>(C) + 1);
    xs[0] = 0.0;
    for (int c = 0; c < C; ++c) xs[c + 1] = xs[c] + e.tableColWidth(c);
    const double totalW = xs[C];
    const double totalH = R * rh;

    QPen pen(preview ? PREVIEW : (highlight ? HL : e.color));
    pen.setCosmetic(true);                                // 网格线宽随屏幕像素，缩放不变粗细
    const double effW = std::max(0.6, mmToScreenPx(e.effectiveLineWidth()));
    pen.setWidthF(highlight ? effW + 1.0 : effW);
    pen.setStyle(toPenStyle(e.lineStyle));
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    // 横线（行边界 R+1 条）
    for (int r = 0; r <= R; ++r)
        p->drawLine(QPointF(xs[0], r * rh), QPointF(totalW, r * rh));
    // 竖线（列边界 C+1 条）
    for (int c = 0; c <= C; ++c)
        p->drawLine(QPointF(xs[c], 0.0), QPointF(xs[c], totalH));

    // ---- 单元格文字：字段占位渲染期求值（与 Text/MText 同一 resolveFields 通路）----
    QFont f = p->font();
    f.setPixelSize(std::max(6, (int)std::lround(th * z)));
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double pad = 1.0 * z;                           // 单元格内边距 1mm
    const double hh = fm.height();
    const int hA = std::clamp(e.textAlign, 0, 2);         // 0=左 1=中 2=右
    p->setPen(e.color);
    for (int r = 0; r < R; ++r) {
        for (int c = 0; c < C; ++c) {
            const QString raw = e.tableCell(r, c);
            if (raw.trimmed().isEmpty()) continue;
            const QString display = resolveFields(raw, model);
            const double tw = fm.horizontalAdvance(display);
            const double cellL = xs[c] + pad, cellR = xs[c + 1] - pad;
            double x0;
            if (hA == 0)      x0 = cellL;                                  // 左对齐
            else if (hA == 1) x0 = (cellL + cellR - tw) / 2.0;             // 居中（明细表惯例）
            else              x0 = std::max(cellL, cellR - tw);            // 右对齐
            const double yc = (r + 0.5) * rh;                              // 行中心（局部 y 向下）
            p->drawText(QRectF(x0, yc - hh / 2, tw, hh),
                        Qt::AlignLeft | Qt::AlignVCenter, display);
        }
    }
    p->restore();
}

// ============================ GraphicsView ============================

GraphicsView::GraphicsView(SketchModel* m, QWidget* parent)
    : QGraphicsView(parent), model_(m) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    // 关键：让平移/缩放完全由 applyTransform() 的 setTransform 控制，杜绝 QGraphicsView 的自动居中。
    // 真根因（对照 Qt 6.10.1 qgraphicsview.cpp 源码逐行确认）：
    //   QGraphicsView 的水平/垂直定位公式为 viewport = matrix.map(scene) - scroll，
    //   其中 scrollX = hbar->value() - leftIndent（updateScroll()）。矩阵平移分量能否生效
    //   完全取决于 indent 与滚动条值是否被 Qt 改写：
    //   a) 场景矩形映射后「小于视口」→ recalculateContentSize() 走 indent 分支：
    //      AlignLeft 时 leftIndent = -viewRect.left()，恰好把矩阵平移量整层消掉
    //      （旧代码 setSceneRect(QRectF()) 空矩形回退为 itemsBoundingRect，内容小必中此分支，
    //       表现为「矩阵在变、画面纹丝不动」）；
    //   b) 场景矩形「大于视口」→ indent=0，但 setTransform() 内部 centerView(AnchorViewCenter)
    //      会 centerOn(lastCenterPoint) 经滚动条把视图中心拉回，同样抵消矩阵平移。
    // 修复（三步，缺一不可）：
    // 1) 固定一个任何缩放/平移下都远大于视口的场景矩形 → 永远走 b) 分支且 indent 恒为 0；
    // 2) setTransformationAnchor(NoAnchor) → setTransform 不再经滚动条重定心，滚动条值恒为 0，
    //    scroll 恒为 0，矩阵平移成为唯一定位来源（双精度保留，1px 都不丢）；
    // 3) 关闭滚动条：既无可见滚动条 UI，也从源头杜绝任何滚动条值参与定位。
    // 对齐标志 AlignLeft|AlignTop 仅作保险（大矩形下对齐偏移为 0，取何值都无影响）。
    scene_->setSceneRect(QRectF(-1e5, -1e5, 2e5, 2e5));
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);  // 去掉边框，使视口与标尺在网格布局中完全对齐
    setMouseTracking(true);
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(QColor(245, 247, 250));
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setAcceptDrops(true);
    setContextMenuPolicy(Qt::NoContextMenu);   // 右键留给「结束连续绘制」
    // 事件实际来自视口，显式把 NoContextMenu 也设到视口，确保右键菜单手势永不抢占右键事件
    // （部分平台/Qt 版本下视口策略与视图不一致会导致右键按下/释放被上下文菜单流程吞掉）。
    if (viewport()) viewport()->setContextMenuPolicy(Qt::NoContextMenu);

    connect(model_, &SketchModel::modelChanged, this, &GraphicsView::onModelChanged);
    connect(model_, &SketchModel::selectionChanged, this, &GraphicsView::onSelectionChanged);
    connect(model_, &SketchModel::viewChanged, this, &GraphicsView::onViewChanged);
    connect(model_, &SketchModel::planeChanged, this, &GraphicsView::onModelChanged);
    connect(model_, &SketchModel::activeToolChanged, this, &GraphicsView::onActiveToolChanged);
    connect(model_, &SketchModel::currentLayerChanged, this, [this]() { viewport()->update(); });
    // 线宽显示开关（LWDISPLAY）：同步到渲染态，并刷新全部图元的线宽呈现。
    connect(model_, &SketchModel::showLineWeightChanged, this, [this](bool on) {
        showLineWeight_ = on;
        applyLineWeightToItems();
    });
    showLineWeight_ = model_->showLineWeight();
    // 动态输入 DYN 关闭时隐藏输入框（开着则等下一次 mouseMove 自然显示）
    connect(model_, &SketchModel::dynModeChanged, this, [this](bool on) {
        if (!on) hideDynBox();
    });
    // 创建 E/T/M/C/X 五个「捕捉切换」快捷键（默认禁用，仅绘制命令期间启用）。
    // 采用正规 QShortcut 重映射，而非事件过滤器拦截：进入绘制命令时临时禁用与之冲突的
    // 全局快捷键（如 C=圆），从根本上消除「按 C 被误判为切圆命令」的冲突（要求 4）。
    // 切换键表直接复用 snap::snapFieldDefs()（与 Shift+右键覆盖菜单、捕捉设置面板同一数据源），
    // 避免三处各自硬编码导致漂移。
    for (const auto& d : snap::snapFieldDefs()) {
        QShortcut* sc = new QShortcut(QKeySequence(d.key), this);
        sc->setContext(Qt::ApplicationShortcut);   // 应用级：无论焦点在何处都生效
        sc->setEnabled(false);                     // 默认禁用，进入绘制命令才启用
        connect(sc, &QShortcut::activated, this, [this, d]() { toggleSnap(d.field, d.label); });
        snapShortcuts_.append(sc);
    }

    setFocusPolicy(Qt::StrongFocus);

    // ---- Strategy Pattern：初始化工具管理器 ----
    // ToolContext 持有 model 和 view 指针，使 ITool 实例能通过 context 读写视图状态
    ToolContext tctx;
    tctx.model = model_;
    tctx.view = this;
    toolMgr_ = std::make_unique<ToolManager>(tctx);
    toolMgr_->setActiveTool("select");   // 初始工具

    fitView();
    rebuild();
}

GraphicsView::~GraphicsView() = default;

void GraphicsView::applyTransform() {
    // Qt 链式变换语义（渲染级像素测试实证，勿改顺序）：
    // fromTranslate(panX_,panY_).scale(zoom_,-zoom_) 对点实际施加「先缩放、后平移」——
    //   p' = (x·zoom_ + panX_, −y·zoom_ + panY_)，即 panX_/panY_ 是屏幕像素偏移，
    //   与 fitView/zoomBy/wheel 中 panX_ = c.x() − anchor.x()·zoom_ 的算式一致。
    // 若误写成 fromScale().translate()，平移量会被 zoom_ 放大（zoom=4 时拖 1px 画面跑 4px）。
    // Y 分量取负：世界系 Y 向上为正，映射到屏幕后视觉上就是向上，与标准笛卡尔系一致。
    // offX/offY 只作用于「本次构建矩阵」，**绝不回写 panX_/panY_**——回写会逐次累积（见下方推导）。
    const auto build = [this](double offX, double offY) {
        setTransform(QTransform::fromTranslate(panX_ + offX, panY_ + offY).scale(zoom_, -zoom_));
    };
    build(0.0, 0.0);

    // ---- 抵消 QGraphicsView 叠加在视口变换上的常量偏移 ----
    // 即使 setSceneRect 给了极大矩形、滚动条关闭、锚点 NoAnchor，Qt 的
    //   viewportTransform() = matrix × fromTranslate(−Sx, −Sy)      (S 为滚动条值，scene 单位)
    // 展开后： eff.dx = m31 − zoom·Sx ， eff.dy = m32 + zoom·Sy
    // 于是「真正生效的映射」与矩阵恒差一个量，二者永不可能相等；
    // 后果：fitToRect() 无法把整幅图框居中/框全——新建图纸后只看得到图框一角。
    //
    // 令 m31 = panX_ − shiftX（shiftX = eff.dx − m31 = −zoom·Sx）即可让 eff.dx = panX_。
    // 关键：这里从「零偏移」矩阵测量，故 shiftX/shiftY 每次算出的都是同一个值，
    // 补偿天然幂等；panX_ 保持纯逻辑状态，右键拖拽增量仍是精确 1:1。
    // （曾把偏差回写进 panX_：单次调用看起来正确，但每次 applyTransform 都会再减一个
    //   shift，连续平移/缩放后视图跑飞——ViewPanTest 的 ddx 从 60 变 -279 即此症状。）
    if (viewport() && viewport()->width() > 0 && viewport()->height() > 0) {
        const QTransform want = transform();
        const QTransform got  = viewportTransform();
        const double shiftX = got.dx() - want.dx();
        const double shiftY = got.dy() - want.dy();
        if (std::fabs(shiftX) > 0.01 || std::fabs(shiftY) > 0.01) build(-shiftX, -shiftY);
    }
    emit viewTransformChanged();    // 通知标尺刷新
}

// 所有可见图元的世界包围盒（不含坐标轴/提示文字覆盖层——它们本就不是 element）。
// 作为「适配窗口」与「窗口缩放」框选结算的统一几何来源。
QRectF GraphicsView::contentBounds() const {
    bool any = false;
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    for (const auto& e : model_->elements()) {
        if (!e.visible) continue;
        QRectF b;
        if (e.type == ElementType::BlockRef) {
            // 块引用的几何来自展开后的子图元并集
            // （geometryBounds 是静态函数、拿不到块表，故在此显式展开）
            bool got = false;
            for (const auto& sub : model_->expandBlockRef(e)) {
                const QRectF sb = ElementItem::geometryBounds(sub);
                if (sb.isNull()) continue;
                if (!got) { b = sb; got = true; } else b = b.united(sb);
            }
            if (!got && !e.pts.empty()) b = QRectF(e.pts[0].x, e.pts[0].y, 0, 0);
        } else {
            b = ElementItem::geometryBounds(e);   // 几何范围（不含描边余量，精确取景）
        }
        if (b.isNull()) continue;                 // 跳过退化/空包围盒（零面积且零宽高，如空文本）
        x0 = std::min(x0, b.left());  y0 = std::min(y0, b.top());
        x1 = std::max(x1, b.right()); y1 = std::max(y1, b.bottom());
        any = true;
    }
    if (!any) return QRectF();
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

// 将世界矩形以 margin 系数适配居中到视口（margin=1.0 即精确充满整个绘图工作区域）。
// 缩放保持等比（min 取较小边），避免图形被拉伸；居中用 panX_/panY_ 平移。
void GraphicsView::fitWorldRect(const QRectF& r, double margin) {
    if (!viewport() || r.width() < 1e-6 || r.height() < 1e-6) return;
    const double vw = viewport()->width(), vh = viewport()->height();
    double z = std::min(vw / r.width(), vh / r.height()) * margin;
    z = qBound(0.05, z, 500.0);
    zoom_ = z;
    const double cx = r.center().x(), cy = r.center().y();
    panX_ = vw / 2.0 - cx * z;
    panY_ = vh / 2.0 + cy * z;                       // Y 翻转：世界 Y 向上 → 屏幕 Y 向下
    applyTransform();
    model_->setViewScale(zoom_);
    viewport()->update();
}

void GraphicsView::fitView() {
    // 适配窗口：将所有可见图元（绘制的图形）精确缩放并充满整个绘图工作区域。
    // 坐标轴(drawAxes)与程序提示文字(HUD)是覆盖层、不计入 elements()，天然被排除，
    // 缩放/平移时保持屏幕常量显示不变。
    QRectF bb = contentBounds();
    if (bb.isEmpty() || bb.width() < 1e-6 || bb.height() < 1e-6) {
        // 无内容：回退到默认视图（原点居中、zoom=4），避免除零
        zoom_ = 4.0;
        panX_ = viewport() ? viewport()->width() / 2.0 : 400;
        panY_ = viewport() ? viewport()->height() / 2.0 : 300;
        applyTransform();
        model_->setViewScale(zoom_);
        if (viewport()) viewport()->update();
        return;
    }
    fitWorldRect(bb, 1.0);   // margin=1.0：精确缩放并充满整个绘图工作区域
}

// 将给定世界坐标矩形（如图纸幅面）适配居中到视口，新建图纸后整幅可见
void GraphicsView::fitToRect(const QRectF& r) {
    if (!viewport() || r.width() < 1e-6 || r.height() < 1e-6) { fitView(); return; }
    fitWorldRect(r, 0.92);   // 留一点边距，避免图框贴边
}

void GraphicsView::zoomBy(double f) {
    const QPointF c = viewport()->rect().center();
    const QPointF anchor = mapToScene(c.toPoint());
    zoom_ = qBound(0.05, zoom_ * f, 500.0);
    panX_ = c.x() - anchor.x() * zoom_;
    panY_ = c.y() + anchor.y() * zoom_;    // Y 翻转后此处符号与 X 相反
    applyTransform();
    model_->setViewScale(zoom_);
    viewport()->update();
}

void GraphicsView::rebuild() {
    scene_->clear();
    previewItem_ = nullptr;                // scene_->clear() 已把预览项一并销毁
    gripPreviewItem_ = nullptr;            // 夹点变换预览项同理
    for (const auto& e : model_->elements()) {
        if (!e.visible) continue;
        auto* it = new ElementItem(e);
        it->model = model_;                      // 块引用展开需要块表
        it->highlight = model_->isSelected(e.id);
        it->warn = conflictIds_.contains(e.id);
        it->showLineWeight = showLineWeight_;    // 线宽显示开关（LWDISPLAY）
        it->dimScale = model_->scaleRatio();     // 标注数字按实物尺寸显示
        // GB/T 14665-2012 4.3：重合图线按优先级叠放（可见轮廓最上，尺寸线最下）
        double z = (double)linePriority(e.lineStyle);
        if (e.type == ElementType::Dimension) z -= 10.0;
        it->setZValue(z);
        scene_->addItem(it);
    }
    viewport()->update();
}

void GraphicsView::applyLineWeightToItems() {
    if (!scene_) return;
    const auto items = scene_->items();
    for (QGraphicsItem* gi : items) {
        if (auto* it = dynamic_cast<ElementItem*>(gi)) {
            it->showLineWeight = showLineWeight_;
            it->update();
        }
    }
    viewport()->update();
}

// 约束图标的世界锚点：取两端图元中心的中点；单端约束（Fixed/FixedPoint/...）取被锁定控制点。
static Vec3 constraintAnchor(const SketchModel* m, const ConstraintRecord& r) {
    const Element* a = (r.idA >= 0) ? m->element(r.idA) : nullptr;
    const Element* b = (r.idB >= 0) ? m->element(r.idB) : nullptr;
    auto centerOf = [](const Element* e) -> Vec3 {
        if (!e) return Vec3();
        if (e->type == ElementType::Circle || e->type == ElementType::Arc) return e->pts[0];
        if (e->pts.empty()) return Vec3();
        Vec3 s; for (const auto& p : e->pts) s = s + p;
        return s * (1.0 / e->pts.size());
    };
    Vec3 ca = centerOf(a), cb = centerOf(b);
    if (a && b) return (ca + cb) * 0.5;
    if (a) {
        // 单端约束：优先取被显式锁定的控制点（FixedPoint/PointOn*/Midpoint）
        if (r.type == ConstraintType::FixedPoint || r.type == ConstraintType::PointOnLine ||
            r.type == ConstraintType::PointOnCircle || r.type == ConstraintType::Midpoint) {
            int idx = (r.idxA >= 0) ? r.idxA : 0;
            if (idx >= 0 && idx < (int)a->pts.size()) return a->pts[idx];
        }
        return ca;
    }
    return Vec3();
}

void GraphicsView::drawConstraintMarkers(QPainter* p) {
    const auto& recs = model_->constraints();
    if (recs.empty()) return;
    p->save();
    const double s = 5.0 / zoom_;                       // 图标半边长（场景单位，屏幕恒大小）
    for (const auto& r : recs) {
        Vec3 a = constraintAnchor(model_, r);
        // 仅当两端图元都不存在时才跳过（原点的真实约束仍要画）
        if (a.x == 0.0 && a.y == 0.0 && a.z == 0.0) {
            const Element* ea = (r.idA >= 0) ? model_->element(r.idA) : nullptr;
            const Element* eb = (r.idB >= 0) ? model_->element(r.idB) : nullptr;
            if (!ea && !eb) continue;
        }
        const double cx = a.x, cy = a.y;
        const QColor col = r.satisfied ? QColor(30, 150, 110) : QColor(220, 120, 30);
        QPen sp(col); sp.setWidthF(1.4 / zoom_); sp.setCosmetic(true);
        p->setPen(sp);
        p->setBrush(col);
        p->drawEllipse(QPointF(cx, cy), s * 0.55, s * 0.55);   // 统一的约束标识底座
        p->setBrush(Qt::NoBrush);
        // 类型标注（像素级正写，不随缩放变形）
        QString label = QString::fromUtf8(constraintTypeName(r.type));
        if (r.value > 0 &&
            (r.type == ConstraintType::Dimensional || r.type == ConstraintType::Distance ||
             r.type == ConstraintType::Angle)) {
            const QString unit = (r.type == ConstraintType::Angle) ? QStringLiteral("°") : QString();
            label += QStringLiteral(" %1%2").arg(r.value, 0, 'f', r.value < 10 ? 2 : 1).arg(unit);
        }
        drawPixelText(p, QPointF(cx + 9.0 / zoom_, cy + 7.0 / zoom_), label, col, 11, true);
    }
    p->restore();
}

void GraphicsView::drawConflictOverlay(QPainter* p) {
    if (conflictIds_.empty()) return;
    p->save();
    QStringList s; for (int id : conflictIds_) s << QString::number(id);
    const QString msg = QStringLiteral("⚠ 检测到过约束 / 冲突：图元 #%1（请移除矛盾或冗余约束）")
                           .arg(s.join(" #"));
    const QPointF tl = mapToScene(10, 10);            // 视口左上角，屏幕恒定位置
    drawPixelText(p, tl, msg, WARN, 13, true);
    p->restore();
}

// 约束表变化（添加/删除/修改）后重算冲突集合、刷新高亮与告警（需求④）
void GraphicsView::refreshConstraintDiagnostics() {
    const auto diag = ConstraintSolver::diagnose(*model_);
    conflictIds_.clear();
    for (int id : diag.overconstrainedIds) conflictIds_.insert(id);
    if (!conflictIds_.empty()) {
        QStringList s; for (int id : conflictIds_) s << QString::number(id);
        model_->log(QStringLiteral("约束诊断：%1（涉及图元 #%2）")
                       .arg(diag.stateText()).arg(s.join(" #")), "error");
    }
    rebuild();
}

// 把视图缩放适配到指定图元（约束栏「定位」用）
void GraphicsView::zoomToElement(int id) {
    const Element* e = model_->element(id);
    if (!e) return;
    QRectF b = ElementItem::geometryBounds(*e);
    if (b.isNull() || b.isEmpty()) {
        const QPointF c(e->pts.empty() ? QPointF() : QPointF(e->pts[0].x, e->pts[0].y));
        b = QRectF(c.x() - 1, c.y() - 1, 2, 2);
    }
    fitToRect(b.adjusted(-2, -2, 2, 2));
}

void GraphicsView::onModelChanged() { rebuild(); }
void GraphicsView::onSelectionChanged(int) { rebuild(); }
void GraphicsView::onViewChanged() { viewport()->update(); }

// ---------------------------- 背景与坐标系 ----------------------------

void GraphicsView::drawBackground(QPainter* p, const QRectF& rect) {
    p->fillRect(rect, QColor(245, 247, 250));

    if (model_->showGrid()) {
        // 一次 setPen + 一次 drawLines 画完一层网格。
        // 旧写法在循环里逐条 setPen，一屏几百条线就是几百次换笔，是鼠标移动时卡顿的主因。
        auto grid = [&](double step, const QColor& col, double px) {
            if (step * zoom_ < 3.0) return;             // 太密就不画，否则糊成一片还拖慢重绘
            QVector<QLineF> ls;
            for (double x = std::floor(rect.left() / step) * step; x <= rect.right(); x += step)
                ls << QLineF(x, rect.top(), x, rect.bottom());
            for (double y = std::floor(rect.top() / step) * step; y <= rect.bottom(); y += step)
                ls << QLineF(rect.left(), y, rect.right(), y);
            QPen pen(col, px);
            pen.setCosmetic(true);
            p->setPen(pen);
            p->drawLines(ls);
        };
        if (1.0 * zoom_ >= 3.0)       { grid(1.0,  GRID_MINOR, 1.0); grid(10.0,  GRID_MAJOR, 1.0); }
        else if (10.0 * zoom_ >= 3.0) { grid(10.0, GRID_MINOR, 1.0); grid(100.0, GRID_MAJOR, 1.0); }
        else                          { grid(100.0, GRID_MAJOR, 1.0); }
    }

    drawAxes(p);
}

void GraphicsView::drawAxes(QPainter* p) const {
    const double L    = 58.0 / zoom_;   // 坐标系指示器保持屏幕恒定尺寸
    const double head = 8.0 / zoom_;

    QPen ax(AXIS_X); ax.setCosmetic(true); ax.setWidthF(1.6);
    QPen ay(AXIS_Y); ay.setCosmetic(true); ay.setWidthF(1.6);

    p->setPen(ax);
    p->drawLine(QLineF(0, 0, L, 0));
    p->setBrush(AXIS_X);
    p->setPen(Qt::NoPen);
    QPolygonF hx;
    hx << QPointF(L, 0) << QPointF(L - head, head * 0.40) << QPointF(L - head, -head * 0.40);
    p->drawPolygon(hx);

    p->setPen(ay);
    p->drawLine(QLineF(0, 0, 0, L));    // Y 翻转后，+Y 在屏幕上就是朝上
    p->setBrush(AXIS_Y);
    p->setPen(Qt::NoPen);
    QPolygonF hy;
    hy << QPointF(0, L) << QPointF(head * 0.40, L - head) << QPointF(-head * 0.40, L - head);
    p->drawPolygon(hy);

    p->setBrush(QColor(70, 74, 84));
    p->drawEllipse(QPointF(0, 0), 2.4 / zoom_, 2.4 / zoom_);
    p->setBrush(Qt::NoBrush);

    drawPixelText(p, QPointF(L + 5.0 / zoom_, 0),          "X", AXIS_X, 12, true);
    drawPixelText(p, QPointF(5.0 / zoom_, L + 2.0 / zoom_), "Y", AXIS_Y, 12, true);
    drawPixelText(p, QPointF(4.0 / zoom_, -4.0 / zoom_),    "O", QColor(70, 74, 84), 11, false);
}

void GraphicsView::drawPixelText(QPainter* p, const QPointF& scenePos, const QString& s,
                                 const QColor& c, int px, bool bold) const {
    p->save();
    p->translate(scenePos);
    p->scale(1.0 / zoom_, -1.0 / zoom_);   // 抵消视图缩放与 Y 翻转，落到 1 单位 = 1 像素的正写坐标系
    QFont f = p->font();
    f.setPixelSize(px);
    f.setBold(bold);
    p->setFont(f);
    p->setPen(c);
    p->drawText(QPointF(0, 0), s);
    p->restore();
}

Vec3 GraphicsView::snapWorld(const Vec3& w) {
    // 命令行键盘输入坐标时跳过捕捉，使用精确坐标值
    if (keyboardInputActive_) return w;
    // 另一端点取已放置的最后一个点：画直线第二点时它用于求「向圆作切线的切点」；
    // 放第一点或自由点时 otherEnd 为空，只做端点/圆心/交点捕捉。
    const Vec3* other = nullptr;
    Vec3 otherBuf;
    if (!pendingPts_.empty()) { otherBuf = pendingPts_.back(); other = &otherBuf; }
    // 两点间中点：以最近放置的两个点作为参考点 a、b
    const Vec3* refA = nullptr, * refB = nullptr;
    Vec3 refABuf, refBBuf;
    if (snapCfg_.midBetween && pendingPts_.size() >= 2) {
        refABuf = pendingPts_[pendingPts_.size() - 2];
        refBBuf = pendingPts_.back();
        refA = &refABuf; refB = &refBBuf;
    }
    snapNow_ = snap::snap(model_->elements(), w, other, worldTol(), snapCfg_, refA, refB);

    // ---- 对象捕捉追踪（OTRACK）：OSNAP 命中的点「获取」为对齐锚点 ----
    // 仅绘制命令期累积；对象追踪关闭时清空锚点集（OTRACK 依赖 OSNAP，属其上层能力）。
    const bool otrOn = model_->trackConfig().objectTrack;
    if (!drawToolActive_ || !otrOn) clearOtrackAnchors();
    else if (snapNow_.snapped) addOtrackAnchor(snapNow_.point);

    // ---- 追踪（极轴 / 对象追踪 / 临时追踪点）：仅绘制命令期有效 ----
    trackNow_ = snap::TrackResult{};
    const auto& tc = model_->trackConfig();
    if (drawing_) {
        const Vec3* polarBase = nullptr;
        Vec3 pb;
        Vec3 lastDir; bool hasDir = false;
        if (!pendingPts_.empty()) {
            pb = pendingPts_.back(); polarBase = &pb;
            // 上一段方向：取最近两个已放置点连线（首段无上一段 → 用绝对极轴角）
            if (pendingPts_.size() >= 2) {
                lastDir = (pendingPts_.back() - pendingPts_[pendingPts_.size() - 2]).normalized();
                hasDir = true;
            }
        }
        trackNow_ = snap::track(w, tc, hasDir ? &lastDir : nullptr, polarBase,
                                model_->trackPoints(), otrackAnchors_, worldTol());
    }

    // ---- 落点优先级：OSNAP（显式几何特征）> 追踪（对齐引导）> 自由点 ----
    Vec3 result = w;
    if      (snapNow_.snapped) result = snapNow_.point;
    else if (trackNow_.hit)    result = trackNow_.point;

    // ---- 输入约束（正交/栅格吸附）：强制性，优先级**高于** OSNAP ----
    // 正交开启后即便 OSNAP 命中斜向点也必须投影回水平/垂直（画不出斜线）；
    // 基准点取最近已放置点，首点无基准时正交不生效（仅栅格吸附生效）。
    const Vec3* base = nullptr;
    Vec3 baseBuf;
    if (!pendingPts_.empty()) { baseBuf = pendingPts_.back(); base = &baseBuf; }
    inputNow_ = snap::applyInputConstraints(result, base, model_->orthoMode(),
                                            model_->gridSnap(), model_->gridSnapStep());
    return inputNow_.point;
}

// ---------------------------- 捕捉开关（E/T/M/C 快捷键） ----------------------------

bool GraphicsView::isDrawTool(const QString& t) const {
    return ToolRegistry::instance().isDrawTool(t);
}

// 各绘制命令的默认捕捉开关：命令进入时载入，期间由 E/T/M/C 切换，退出时自动恢复。
// 「各自维护独立的捕捉快捷键映射」即体现为：不同命令可给出不同的默认开关组合。
snap::SnapConfig GraphicsView::snapDefaultFor(const QString& t) const {
    snap::SnapConfig c;   // 默认：端点/圆心/交点/切线开，中点关
    if (t == QStringLiteral("circle") || t == QStringLiteral("circle_cr") ||
        t == QStringLiteral("circle_concentric")) {
        c.endpoint = false; c.center = true;  c.tangent = true; c.intersection = true; c.midpoint = false;
    } else if (t == QStringLiteral("circle3p") || t == QStringLiteral("circle_2p") ||
               t == QStringLiteral("circle_2pr") || t == QStringLiteral("circle_t1e2p")) {
        c.endpoint = true;  c.center = false; c.tangent = true; c.intersection = true; c.midpoint = false;
    } else if (t == QStringLiteral("circle_t2e1p") || t == QStringLiteral("circle_t2er") ||
               t == QStringLiteral("circle_t3e") || t == QStringLiteral("circle_inscribed") ||
               t == QStringLiteral("circle_byarc")) {
        c.endpoint = true;  c.center = true;  c.tangent = true; c.intersection = true; c.midpoint = false;
    } else if (t == QStringLiteral("arc") || ToolRegistry::instance().isArcSubTool(t)) {
        c.endpoint = true;  c.center = true;  c.tangent = true; c.intersection = true; c.midpoint = false;
    } else if (t == QStringLiteral("rect")) {
        c.endpoint = true;  c.center = true;  c.tangent = true; c.intersection = true; c.midpoint = false;
    } else if (t == QStringLiteral("poly") || t == QStringLiteral("spline")) {
        c.endpoint = true;  c.center = false; c.tangent = false; c.intersection = true; c.midpoint = true;
    } else if (ToolRegistry::instance().isPolySubTool(t)) {
        c.endpoint = true;  c.center = true;  c.tangent = false; c.intersection = true; c.midpoint = true;
    } else if (ToolRegistry::instance().isSplineSubTool(t)) {
        c.endpoint = true;  c.center = true;  c.tangent = true;  c.intersection = true; c.midpoint = false;
    }
    // line 及其它：沿用 c 默认（端点/圆心/交点/切线开，中点关）
    return c;
}

void GraphicsView::onActiveToolChanged() {
    const QString t = model_->activeTool();
    const bool newDraw = isDrawTool(t);
    const bool prevDraw = drawToolActive_;

    // 夹点热态与工具切换互斥：切换工具即取消热态（模型未落盘，直接清态）
    if (gripHot_) cancelGrip();

    // 区域拾取模式（剖面填充）与绘制命令互斥：启动绘制命令即退出拾取，
    // 避免左键语义冲突（拾取模式下左键被吞掉、不进入绘制）。
    if (regionPickMode_ && newDraw) {
        setRegionPickMode(false);
        emit regionPickCancelled();
    }

    // ---- Strategy Pattern：通知工具管理器切换活动工具 ----
    toolMgr_->setActiveTool(t);

    if (newDraw && !prevDraw) {
        // 进入绘制命令：①保存并临时禁用冲突的全局快捷键 ②启用本组切换键（要求 1、2）
        saveConflictingShortcuts();
        enableSnapShortcuts(true);
        // 活动捕捉配置以「全局持久配置」为基线（用户面板设定优先），而非命令内置默认；
        // 这样捕捉行为可由用户按场景精细控制。snapCfgBackup_ 记录该基线，退出/清除覆盖后恢复至此。
        const snap::SnapConfig base = model_->snapConfig();
        snapCfgBackup_ = base;
        snapCfg_ = base;
        snapOverrideActive_ = false;
    } else if (newDraw && prevDraw) {
        // 绘制命令之间互切：保持重映射状态，仅按全局配置重置捕捉开关
        const snap::SnapConfig base = model_->snapConfig();
        snapCfgBackup_ = base;
        snapCfg_ = base;
        snapOverrideActive_ = false;
    } else if (!newDraw && prevDraw) {
        // 退出绘制命令：禁用本组切换键并还原全局快捷键（要求 3）
        enableSnapShortcuts(false);
        restoreConflictingShortcuts();
        snapCfg_ = model_->snapConfig();   // 回到全局持久配置
        snapOverrideActive_ = false;
    }
    drawToolActive_ = newDraw;
    clearOtrackAnchors();                 // 换工具即丢弃已获取的对象追踪锚点
    trackNow_ = snap::TrackResult{};      // 追踪结果同步失效
    resetPending();                       // 保持原 activeToolChanged 的复位行为
    updateCommandPrompt();               // 设置新工具的初始分段提示
}

void GraphicsView::refreshSnapAtCursor() {
    const Vec3* other = nullptr;
    Vec3 buf;
    if (!pendingPts_.empty()) { buf = pendingPts_.back(); other = &buf; }
    const Vec3* refA = nullptr, * refB = nullptr;
    Vec3 refABuf, refBBuf;
    if (snapCfg_.midBetween && pendingPts_.size() >= 2) {
        refABuf = pendingPts_[pendingPts_.size() - 2];
        refBBuf = pendingPts_.back();
        refA = &refABuf; refB = &refBBuf;
    }
    snapNow_ = snap::snap(model_->elements(), lastMouseWorld_, other, worldTol(), snapCfg_, refA, refB);
    if (!pendingPts_.empty())
        updatePreview(snapNow_.snapped ? snapNow_.point : lastMouseWorld_);
    viewport()->update();
}

// 取反某个捕捉开关（由 E/T/M/C/X 快捷键触发），并记录日志、即时刷新标记。
void GraphicsView::toggleSnap(bool snap::SnapConfig::* field, const QString& name) {
    snapCfg_.*field = !(snapCfg_.*field);
    logSnapToggle(name, snapCfg_.*field);
    refreshSnapAtCursor();
}

// 临时捕捉覆盖（Shift+右键）：仅本次放置一个点采用指定的单一捕捉类型，
// 记录覆盖前的活动配置（snapRestore_），放置后由 clearSnapOverride 恢复，做到「操作结束自动还原」。
void GraphicsView::applySnapOverride(bool snap::SnapConfig::* field, const QString& name) {
    if (!drawToolActive_) return;
    snapRestore_ = snapCfg_;          // 记住覆盖前的活动配置，作为恢复目标（"原有配置"）
    snap::SnapConfig c;               // 全关
    c.*field = true;                  // 仅该类型生效
    snapCfg_ = c;
    snapOverrideActive_ = true;
    model_->log(QStringLiteral("临时捕捉覆盖：仅 %1（Shift+右键），放置一点后自动恢复").arg(name));
    refreshSnapAtCursor();
}

void GraphicsView::clearSnapOverride() {
    if (!snapOverrideActive_) return;
    snapCfg_ = snapRestore_;          // 恢复覆盖前的活动配置
    snapOverrideActive_ = false;
    model_->log(QStringLiteral("已清除临时捕捉覆盖，恢复原有捕捉配置"));
    refreshSnapAtCursor();
}

// 弹出「临时捕捉覆盖」菜单：列出全部捕捉类型，选中即 applySnapOverride（单次有效）。
void GraphicsView::showSnapOverrideMenu(const QPoint& globalPos) {
    QMenu menu(this);
    menu.setTitle(QStringLiteral("临时捕捉覆盖（仅本次生效）"));
    for (const auto& d : snap::snapFieldDefs()) {
        QAction* a = menu.addAction(d.label);
        connect(a, &QAction::triggered, this, [this, d]() { applySnapOverride(d.field, d.label); });
    }
    menu.addSeparator();
    QAction* off = menu.addAction(QStringLiteral("清除覆盖（恢复原有配置）"));
    connect(off, &QAction::triggered, this, [this]() { clearSnapOverride(); });
    menu.addSeparator();
    QAction* tk = menu.addAction(QStringLiteral("临时追踪点 (TK) — 光标处"));
    connect(tk, &QAction::triggered, this, [this]() { addTrackPointAtCursor(); });
    QAction* tkc = menu.addAction(QStringLiteral("清除全部临时追踪点"));
    connect(tkc, &QAction::triggered, this, [this]() { clearTrackPoints(); });
    menu.exec(globalPos);
}

// ---------------------------- 追踪（极轴 / 对象捕捉追踪 / 临时追踪点） ----------------------------

// 对象追踪锚点：去重并入并封顶（防止长命令期无限累积拖慢 track 计算）
void GraphicsView::addOtrackAnchor(const Vec3& p) {
    for (const auto& a : otrackAnchors_)
        if (a.distTo(p) < 1e-6) return;           // 已存在（同一 OSNAP 点反复命中）
    if (otrackAnchors_.size() >= 8) otrackAnchors_.erase(otrackAnchors_.begin());
    otrackAnchors_.push_back(p);
}

void GraphicsView::clearOtrackAnchors() {
    otrackAnchors_.clear();
}

// 临时追踪点（TK）：在当前光标世界坐标处放置一个瞬态对齐锚点，
// 参与对象追踪对齐线计算；不随图存档，绘制结束/清空图纸时丢弃。
void GraphicsView::addTrackPointAtCursor() {
    model_->addTrackPoint(lastMouseWorld_);
    model_->log(QStringLiteral("已放置临时追踪点 (TK)：(%1, %2)")
                    .arg(lastMouseWorld_.x, 0, 'f', 2).arg(lastMouseWorld_.y, 0, 'f', 2));
    refreshSnapAtCursor();
}

void GraphicsView::clearTrackPoints() {
    if (model_->trackPoints().empty()) return;
    model_->clearTrackPoints();
    model_->log(QStringLiteral("已清除全部临时追踪点 (TK)"));
    refreshSnapAtCursor();
}

// 统一启用/禁用本组 E/T/M/C/X 切换快捷键。
void GraphicsView::enableSnapShortcuts(bool on) {
    for (QShortcut* sc : snapShortcuts_) sc->setEnabled(on);
}

// 要求 1：进入绘制命令时，遍历所有顶层窗口的 QAction / QShortcut，
// 凡主键为 E/T/M/C/X（无修饰键）者，记录其原始映射后临时清空，避免与切换键冲突（要求 4）。
void GraphicsView::saveConflictingShortcuts() {
    savedShortcuts_.clear();
    const QList<Qt::Key> keys = {
        Qt::Key_E, Qt::Key_T, Qt::Key_M, Qt::Key_C, Qt::Key_X,   // 原有 5 种
        Qt::Key_Q, Qt::Key_P, Qt::Key_I, Qt::Key_N, Qt::Key_A,   // 新增：象限/垂足/插入/节点/最近
        Qt::Key_O, Qt::Key_L, Qt::Key_W, Qt::Key_B              // 新增：外观/平行/延伸/两点中点
    };
    for (QWidget* w : qApp->topLevelWidgets()) {
        for (QAction* a : w->findChildren<QAction*>()) {
            const QKeySequence s = a->shortcut();
            if (s.isEmpty()) continue;
            const QKeyCombination kc = s[0];
            if (kc.keyboardModifiers() != Qt::NoModifier) continue;  // 带修饰键者不视为冲突
            if (!keys.contains(kc.key())) continue;
            savedShortcuts_.append({a, s});
            a->setShortcut(QKeySequence());                 // 临时禁用，绘制期间交给我们
        }
        for (QShortcut* sc : w->findChildren<QShortcut*>()) {
            if (snapShortcuts_.contains(sc)) continue;      // 跳过本组切换键自身
            const QKeySequence s = sc->key();
            if (s.isEmpty()) continue;
            const QKeyCombination kc = s[0];
            if (kc.keyboardModifiers() != Qt::NoModifier) continue;
            if (!keys.contains(kc.key())) continue;
            savedShortcuts_.append({sc, s});
            sc->setKey(QKeySequence());
        }
    }
}

// 要求 3：退出绘制命令时，把被临时禁用的全局快捷键原样还原，使快捷键状态与绘制前完全一致。
void GraphicsView::restoreConflictingShortcuts() {
    for (const SavedShortcut& s : savedShortcuts_) {
        if (auto* a = qobject_cast<QAction*>(s.obj)) a->setShortcut(s.seq);
        else if (auto* sc = qobject_cast<QShortcut*>(s.obj)) sc->setKey(s.seq);
    }
    savedShortcuts_.clear();
}

void GraphicsView::logSnapToggle(const QString& name, bool on) {
    model_->log(QStringLiteral("捕捉 %1 已%2").arg(name).arg(on ? QStringLiteral("开启") : QStringLiteral("关闭")),
                QStringLiteral("info"));
}

void GraphicsView::drawForeground(QPainter* p, const QRectF& rect) {
    QGraphicsView::drawForeground(p, rect);

    p->save();
    drawHandles(p);          // 仍在场景坐标系下：控制点尺寸按 1/zoom 折算成屏幕恒定大小
    p->restore();

    // 框选橡皮筋：场景坐标系下的虚线矩形 + 半透明填充
    if (marqueeActive_) {
        p->save();
        QPen mp(QColor(70, 130, 255));
        mp.setStyle(Qt::DashLine);
        mp.setCosmetic(true);                 // 线宽按屏幕像素，不随缩放变化
        p->setPen(mp);
        p->setBrush(QColor(70, 130, 255, 28));
        p->drawRect(marquee_.normalized());
        p->restore();
    }

    // 窗口缩放框选橡皮筋：绿色虚线矩形 + 半透明填充（与多选框选区分配色）
    if (zoomBoxActive_) {
        p->save();
        QPen zp(QColor(30, 160, 90));
        zp.setStyle(Qt::DashLine);
        zp.setCosmetic(true);                 // 线宽按屏幕像素，不随缩放变化
        p->setPen(zp);
        p->setBrush(QColor(30, 160, 90, 24));
        p->drawRect(zoomBoxRect_.normalized());
        p->restore();
    }

    // 区域拾取预览（剖面填充）：高亮"将要填充的封闭区域"，让用户在点击前就能
    // 确认程序识别到的边界是否正确。外边界+内孔用奇偶规则（OddEvenFill）自动掏空。
    if (regionPickMode_ && !regionHighlight_.empty()) {
        p->save();
        QPainterPath hl;
        hl.setFillRule(Qt::OddEvenFill);
        for (const auto& poly : regionHighlight_) {
            if (poly.size() < 2) continue;
            QPolygonF qp;
            qp.reserve((int)poly.size());
            for (const auto& v : poly) qp << QPointF(v.x, v.y);
            hl.addPolygon(qp);
        }
        QPen hp(QColor(255, 140, 0));
        hp.setCosmetic(true);                 // 线宽按屏幕像素，不随缩放变化
        hp.setWidthF(1.6);
        hp.setStyle(Qt::DashLine);
        p->setPen(hp);
        p->setBrush(QColor(255, 140, 0, 40));
        if (!hl.isEmpty()) p->drawPath(hl);
        p->restore();
    }

    // 直线绘制智能捕捉标记：用不同颜色/形状区分捕捉类型，约 10px 恒定屏幕大小
    if (drawing_ && snapNow_.snapped) {
        p->save();
        const double s = 5.0 / zoom_;                 // 半边长（场景单位）
        const double cx = snapNow_.point.x, cy = snapNow_.point.y;
        QColor col;
        switch (snapNow_.type) {
            case snap::SnapType::Center:        col = QColor(200, 60, 200); break;
            case snap::SnapType::Intersection:  col = QColor(40, 170, 70);  break;
            case snap::SnapType::Tangent:       col = QColor(240, 150, 30); break;
            case snap::SnapType::Quadrant:      col = QColor(240, 90, 160); break;
            case snap::SnapType::Perpendicular: col = QColor(0, 160, 200);  break;
            case snap::SnapType::Insertion:     col = QColor(180, 120, 0);   break;
            case snap::SnapType::Node:          col = QColor(120, 0, 200);  break;
            case snap::SnapType::Nearest:       col = QColor(150, 150, 150);break;
            case snap::SnapType::Apparent:      col = QColor(120, 200, 0);   break;
            case snap::SnapType::Parallel:      col = QColor(0, 120, 180);  break;
            case snap::SnapType::Extension:     col = QColor(180, 90, 0);    break;
            case snap::SnapType::Midpoint:      col = QColor(80, 120, 220);  break;
            case snap::SnapType::MidBetween:    col = QColor(120, 60, 160);  break;
            default:                            col = QColor(60, 130, 240); break;  // Endpoint
        }
        QPen sp(col); sp.setWidthF(1.4 / zoom_); sp.setCosmetic(true);
        p->setPen(sp);
        switch (snapNow_.type) {
            case snap::SnapType::Intersection:
                p->drawLine(QPointF(cx - s, cy - s), QPointF(cx + s, cy + s));
                p->drawLine(QPointF(cx - s, cy + s), QPointF(cx + s, cy - s));
                break;
            case snap::SnapType::Tangent:
                p->drawEllipse(QPointF(cx, cy), s, s);
                break;
            case snap::SnapType::Node: {
                p->setBrush(col); p->setPen(Qt::NoPen);
                p->drawEllipse(QPointF(cx, cy), s * 0.6, s * 0.6);
                p->setPen(sp); p->setBrush(Qt::NoBrush);
                break;
            }
            case snap::SnapType::Quadrant: {
                QPolygonF dia; dia << QPointF(cx, cy - s) << QPointF(cx + s, cy)
                                   << QPointF(cx, cy + s) << QPointF(cx - s, cy);
                p->drawPolygon(dia);
                break;
            }
            case snap::SnapType::Parallel: {
                // 沿 dir 画一条短指示线，直观表达被吸附的平行方位（dir 为世界坐标系单位向量）
                const Vec3 d = snapNow_.dir;
                const double L = 3.0 * s;
                p->drawLine(QPointF(cx, cy), QPointF(cx + d.x * L, cy - d.y * L));
                p->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
                break;
            }
            case snap::SnapType::MidBetween:
                p->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));
                p->drawPoint(QPointF(cx, cy));
                break;
            case snap::SnapType::Perpendicular: {
                // ⊥ 符号：小方框内画直角，清晰表达「垂直 / 垂足」吸附
                const double w = s * 1.3;
                p->setPen(sp);
                p->drawRect(QRectF(cx - w, cy - w, 2 * w, 2 * w));   // 外框
                p->drawLine(QPointF(cx - w, cy), QPointF(cx, cy));   // 水平臂
                p->drawLine(QPointF(cx, cy), QPointF(cx, cy - w));   // 垂直臂（⊥）
                break;
            }
            default:
                p->drawRect(QRectF(cx - s, cy - s, 2 * s, 2 * s));   // Endpoint/Center/...：方框
                break;
        }
        // 命中提示：在标记右上方标注捕捉类型名（像素级正写文字，不随缩放变形）。
        // 名称取自 snap::snapTypeLabel()，与快捷键/菜单/设置面板同一数据源。
        drawPixelText(p, QPointF(cx + 9.0 / zoom_, cy + 7.0 / zoom_),
                      snap::snapTypeLabel(snapNow_.type), col, 11, true);
        p->restore();
    }

    // ---- 约束可视化：图标 + 类型标注（需求②）与过约束/冲突告警（需求④）----
    // 始终绘制（不限绘制命令），让用户在任意时刻都能看到受约束对象的约束状态。
    drawConstraintMarkers(p);
    drawConflictOverlay(p);

    // ---- 追踪引导（极轴射线 / 对象追踪对齐线 / 临时追踪点标记 / 追踪落点）----
    if (drawing_) {
        const auto& tc = model_->trackConfig();
        const double R = 1e5;   // 引导线长度：覆盖整个场景范围（sceneRect ±1e5）

        // ① 临时追踪点（TK）：青色小叉
        if (!model_->trackPoints().empty()) {
            p->save();
            QPen tkp(QColor(0, 170, 170));
            tkp.setWidthF(1.2 / zoom_); tkp.setCosmetic(true);
            p->setPen(tkp);
            const double s = 4.0 / zoom_;
            for (const auto& tp : model_->trackPoints()) {
                p->drawLine(QPointF(tp.x - s, tp.y - s), QPointF(tp.x + s, tp.y + s));
                p->drawLine(QPointF(tp.x - s, tp.y + s), QPointF(tp.x + s, tp.y - s));
            }
            p->restore();
        }

        // ② 对象追踪对齐线（已获取的 OSNAP 锚点）：蓝色水平/垂直虚线
        if (tc.objectTrack && !otrackAnchors_.empty()) {
            p->save();
            QPen op(QColor(0, 150, 220, 170));
            op.setStyle(Qt::DashLine); op.setWidthF(1.0 / zoom_); op.setCosmetic(true);
            p->setPen(op);
            for (const auto& a : otrackAnchors_) {
                p->drawLine(QPointF(a.x - R, a.y), QPointF(a.x + R, a.y));
                p->drawLine(QPointF(a.x, a.y - R), QPointF(a.x, a.y + R));
            }
            p->restore();
        }

        // ③ 极轴射线：从最近已放置点沿各极轴角方向（橙色虚线；含相对上一段方向的偏移）
        if (tc.polar && !pendingPts_.empty()) {
            const Vec3 base = pendingPts_.back();
            const Vec3* ld = nullptr;
            Vec3 lastDir;
            if (pendingPts_.size() >= 2) {
                lastDir = (pendingPts_.back() - pendingPts_[pendingPts_.size() - 2]).normalized();
                ld = &lastDir;
            }
            const auto angs = snap::polarAngles(tc, ld);
            p->save();
            QPen pp(QColor(220, 140, 0, 170));
            pp.setStyle(Qt::DashLine); pp.setWidthF(1.0 / zoom_); pp.setCosmetic(true);
            p->setPen(pp);
            for (double deg : angs) {
                const double a = deg * M_PI / 180.0;
                p->drawLine(QPointF(base.x, base.y),
                            QPointF(base.x + R * std::cos(a), base.y + R * std::sin(a)));
            }
            p->restore();
        }

        // ⑤ 正交引导线：从基点出发的水平/垂直双向虚线（正交开启时提示方向已被锁定）
        if (model_->orthoMode() && !pendingPts_.empty()) {
            const Vec3 base = pendingPts_.back();
            p->save();
            QPen op(QColor(90, 100, 120, 150));
            op.setStyle(Qt::DashLine); op.setWidthF(1.0 / zoom_); op.setCosmetic(true);
            p->setPen(op);
            p->drawLine(QPointF(base.x - R, base.y), QPointF(base.x + R, base.y));
            p->drawLine(QPointF(base.x, base.y - R), QPointF(base.x, base.y + R));
            p->restore();
        }

        // ⑥ 追踪落点标记：十字（颜色随模式区分）
        if (trackNow_.hit) {
            p->save();
            const double s = 6.0 / zoom_;
            const QColor c = (trackNow_.mode == snap::TrackMode::Polar)       ? QColor(230, 150, 0)
                           : (trackNow_.mode == snap::TrackMode::TempTK)      ? QColor(0, 170, 170)
                                                                              : QColor(0, 150, 220);
            QPen tp(c); tp.setWidthF(1.4 / zoom_); tp.setCosmetic(true);
            p->setPen(tp);
            p->drawLine(QPointF(trackNow_.point.x - s, trackNow_.point.y),
                        QPointF(trackNow_.point.x + s, trackNow_.point.y));
            p->drawLine(QPointF(trackNow_.point.x, trackNow_.point.y - s),
                        QPointF(trackNow_.point.x, trackNow_.point.y + s));
            // 追踪命中提示：标注是哪种追踪在起作用（极轴 / 对象追踪 / 临时追踪点）
            const QString tn = (trackNow_.mode == snap::TrackMode::Polar)
                                   ? QStringLiteral("极轴")
                               : (trackNow_.mode == snap::TrackMode::TempTK)
                                   ? QStringLiteral("临时追踪点")
                                   : QStringLiteral("对象追踪");
            drawPixelText(p, QPointF(trackNow_.point.x + 9.0 / zoom_, trackNow_.point.y + 7.0 / zoom_),
                          tn, c, 11, true);
            p->restore();
        }
    }

    p->save();
    p->resetTransform();                   // HUD 直接落在视口像素坐标上，不随缩放漂移
    QFont f = p->font();
    f.setPixelSize(12);
    p->setFont(f);
    p->setPen(QColor(108, 116, 132));
    p->drawText(10, 18, QString("工作平面 %1     坐标系 X→ Y↑（标准笛卡尔）     当前线宽 %2     拾取容差 %3px")
                            .arg(planeName(model_->plane()))
                            .arg(lineWidthName(model_->defaultWidth()))
                            .arg((int)PICK_TOL_PX));
    if (drawToolActive_) {
        const auto mk = [](bool on) { return on ? QStringLiteral("✓") : QStringLiteral("✗"); };
        p->setPen(QColor(70, 130, 255));
        p->drawText(10, 36, QString("捕捉开关  [E]端点%1  [T]切线%2  [M]中点%3  [C]圆心%4  [X]交点%5   （绘制中按对应键切换）")
                                .arg(mk(snapCfg_.endpoint))
                                .arg(mk(snapCfg_.tangent))
                                .arg(mk(snapCfg_.midpoint))
                                .arg(mk(snapCfg_.center))
                                .arg(mk(snapCfg_.intersection)));
        p->drawText(10, 54, QString("捕捉开关② [Q]象限%1 [P]垂足%2 [I]插入%3 [N]节点%4 [A]最近%5 [O]外观%6 [L]平行%7 [W]延伸%8 [B]两点中点%9")
                                .arg(mk(snapCfg_.quadrant)).arg(mk(snapCfg_.perpendicular)).arg(mk(snapCfg_.insertion))
                                .arg(mk(snapCfg_.node)).arg(mk(snapCfg_.nearest)).arg(mk(snapCfg_.apparent))
                                .arg(mk(snapCfg_.parallel)).arg(mk(snapCfg_.extension)).arg(mk(snapCfg_.midBetween)));
        // 追踪状态行：极轴（含增量角）/ 对象追踪 / 临时追踪点数量（命令行 polar/otrack/tk 可控制）
        const auto& tc = model_->trackConfig();
        p->setPen(QColor(190, 140, 40));
        p->drawText(10, 72, QString("追踪      极轴%1（增量 %2°%3）  对象追踪%4  临时追踪点 %5 个")
                                .arg(mk(tc.polar))
                                .arg(tc.polarIncrement, 0, 'f', 0)
                                .arg(tc.polarRelative ? QStringLiteral("·相对上一段") : QStringLiteral("·绝对"))
                                .arg(mk(tc.objectTrack))
                                .arg(model_->trackPoints().size()));
    }
    // ---- 输入约束状态行（正交 / 栅格吸附）：始终显示，与是否在绘制中无关 ----
    // 单一状态源在 SketchModel，三入口（F8/F9、状态栏按钮、命令 ortho/snap）改的都是它，
    // 因此这行文字即"当前真相"，任何入口改动都会即时反映到这里。
    {
        const auto on = [](bool b) { return b ? QStringLiteral("✓") : QStringLiteral("✗"); };
        p->setPen(QColor(80, 120, 90));
        p->drawText(10, drawToolActive_ ? 90 : 36,
                    QString("输入约束  [F8]正交%1  [F9]栅格吸附%2（步长 %3 mm）  捕捉容差 %4 px")
                        .arg(on(model_->orthoMode()))
                        .arg(on(model_->gridSnap()))
                        .arg(model_->gridSnapStep(), 0, 'f', 2)
                        .arg((int)PICK_TOL_PX));
    }

    // ---- 当前活动图层（醒目显示）：右上角彩色徽标，实时跟随切换 ----
    // 元素是覆盖层（不计入 elements()，不参与 fit/zoom），因此始终固定在屏幕右上角、
    // 不随缩放漂移；图层一切换即由 currentLayerChanged 信号触发本视图重绘而实时刷新。
    {
        const QString cl = model_->currentLayer();
        const Layer* L = model_->findLayer(cl);
        const QColor lc = (L && L->color.isValid()) ? L->color : QColor(26, 29, 40);
        const QString txt = QStringLiteral("当前图层：") + cl;
        QFont bf = p->font(); bf.setPixelSize(13); bf.setBold(true);
        QFontMetrics fm(bf);
        const int padX = 10, padY = 6, sw = 12;          // sw = 图层色块边长
        const int tw = fm.horizontalAdvance(txt);
        const int bw = tw + padX * 2 + sw + 6;
        const int bh = fm.height() + padY * 2;
        const int margin = 12;
        const int bx = viewport()->width() - bw - margin;
        const int by = margin;
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 235));        // 半透明白底
        p->drawRoundedRect(bx, by, bw, bh, 6, 6);
        p->setPen(QColor(120, 130, 150));               // 边框
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(bx, by, bw, bh, 6, 6);
        p->setPen(Qt::NoPen);
        p->setBrush(lc);                                // 图层色块
        p->drawRoundedRect(bx + padX, by + (bh - sw) / 2, sw, sw, 2, 2);
        p->setPen(QColor(28, 32, 42));
        p->setFont(bf);
        p->drawText(bx + padX + sw + 6, by + padY + fm.ascent(), txt);
    }
    p->restore();
}

// ---------------------------- 命中检测 ----------------------------

int GraphicsView::hitTest(const QPoint& viewPos) const {
    const Vec3 w = toVec3(mapToScene(viewPos));
    // 容差以屏幕像素定义再换算到世界单位 —— 无论放大到多少倍，"手感"始终一致
    const double tol = PICK_TOL_PX / std::max(1e-6, zoom_);
    int best = -1;
    double bestD = tol;
    const auto& es = model_->elements();
    // 逆序遍历：后创建的图元视觉在上层，距离相同时优先选中它
    for (int i = (int)es.size() - 1; i >= 0; --i) {
        if (!es[i].visible) continue;
        double d;
        if (es[i].type == ElementType::BlockRef) {
            // 块引用：点块内任一图形都应命中整个块 —— 对展开后的子图元取最小距离
            const auto subs = model_->expandBlockRef(es[i]);
            d = subs.empty()
                    ? (es[i].pts.empty() ? 1e18 : w.distTo(es[i].pts[0]))
                    : geo::distanceToElements(subs, w);
        } else {
            d = geo::distanceToElement(es[i], w);
        }
        if (d < bestD) { bestD = d; best = es[i].id; }
    }
    return best;
}

std::vector<int> GraphicsView::hitTestAll(const QPoint& viewPos) const {
    const Vec3 w = toVec3(mapToScene(viewPos));
    const double tol = PICK_TOL_PX / std::max(1e-6, zoom_);
    std::vector<std::pair<double, int>> hits;    // (距离, id)：近的优先
    const auto& es = model_->elements();
    for (const auto& e : es) {
        if (!e.visible) continue;
        double d;
        if (e.type == ElementType::BlockRef) {
            const auto subs = model_->expandBlockRef(e);
            d = subs.empty() ? (e.pts.empty() ? 1e18 : w.distTo(e.pts[0]))
                             : geo::distanceToElements(subs, w);
        } else {
            d = geo::distanceToElement(e, w);
        }
        if (d < tol) hits.push_back({d, e.id});
    }
    std::sort(hits.begin(), hits.end());
    std::vector<int> ids;
    ids.reserve(hits.size());
    for (const auto& h : hits) ids.push_back(h.second);
    return ids;
}

void GraphicsView::cycleSelection(const QPoint& vp, const std::vector<int>& ids) {
    if (ids.empty()) return;
    // 同一锚点（8px 内）且命中集合未变 → 继续循环；否则重建循环列表。
    // 首次跳到第 2 优先（最顶层单击本就能选中，循环的意义在于够到被压住的对象）。
    if (!cycleActive_ || (vp - cycleAnchor_).manhattanLength() > 8 || ids != cycleList_) {
        cycleActive_ = true;
        cycleAnchor_ = vp;
        cycleList_ = ids;
        cycleIdx_ = 1 % (int)ids.size();
    } else {
        cycleIdx_ = (cycleIdx_ + 1) % (int)ids.size();
    }
    model_->setSelected(cycleList_[cycleIdx_]);
    model_->log(QString("选择循环：第 %1 / %2 个（继续 Ctrl+点击切换，其他点击退出循环）")
                    .arg(cycleIdx_ + 1).arg(cycleList_.size()));
}

void GraphicsView::breakCycle() {
    cycleActive_ = false;
    cycleList_.clear();
    cycleIdx_ = 0;
}

QRectF GraphicsView::elementSceneBounds(const Element& e) const {
    // 复用 ElementItem 的几何包围盒计算（已覆盖标注的尺寸线/界线/引线），
    // 保证「框住即选中」与绘制的几何范围严格一致。
    ElementItem tmp(e);
    tmp.model = model_;          // 块引用：包围盒须来自展开后的子图元
    return tmp.boundingRect();
}

QRectF ElementItem::geometryBounds(const Element& e) {
    // 与 boundingRect() 同源，但**不**做 cosmetic 描边余量膨胀：
    // 「适配窗口/窗口缩放」要的是图形真实几何范围，多出的像素余量会让取景不满。
    // 算法已下沉到 ElementPath；静态语境无 model（与原临时 item 语义一致）。
    return ElementPath::rawBounds(e);
}

// angleDeg / lineIntersect 已消除（DRY）：
//   angleDeg(c, p)  →  (p - c).angleDeg()       （Vec3 成员方法）
//   lineIntersect() →  math::lineLineIntersect() （Vec.h 统一函数）

// ---------------------------- 预览 ----------------------------

void GraphicsView::showPreview(const Element& tmp, std::vector<Element> guides) {
    // 复用同一个预览项，只改数据。
    // 旧实现每次鼠标移动都 removeItem + new 一个新 item 且从不 delete，
    // 一次拖动会泄漏成百上千个 QGraphicsItem，既漏内存又拖慢场景索引 —— 这正是圆弧"跟不上手"的根源。
    if (!previewItem_) {
        previewItem_ = new ElementItem(tmp);
        previewItem_->preview = true;
        previewItem_->showLineWeight = showLineWeight_;
        previewItem_->dimScale = model_->scaleRatio();
        previewItem_->setZValue(1000);
        scene_->addItem(previewItem_);
    }
    previewItem_->setElement(tmp);
    previewItem_->setGuides(std::move(guides));
}

void GraphicsView::clearPreview() {
    if (previewItem_) {
        scene_->removeItem(previewItem_);
        delete previewItem_;
        previewItem_ = nullptr;
    }
}

void GraphicsView::trackArcAngle(double curAng) {
    if (!arcTracking_) { arcTracking_ = true; arcLastAng_ = curAng; arcSweep_ = 0.0; return; }
    // 逐帧把角增量「解缠」到 (-180,180] 再累加：
    // 直接用 atan2 之差会在 ±180° 处跳变 360°，表现为预览弧突然翻到另一侧。
    const double d = geo::wrapDeg180(curAng - arcLastAng_);
    arcSweep_ = qBound(-360.0, arcSweep_ + d, 360.0);
    arcLastAng_ = curAng;
}

void GraphicsView::updatePreview(const Vec3& cur) {
    const QString t = model_->activeTool();
    const auto& lp = model_->lineParams();

    // 栏选/圈围/圈交：实时预览采集折线（WP/CP 闭合首尾边）
    if (t == "fence" || t == "wp" || t == "cp") {
        if (pendingPts_.size() >= 2) {
            Element first = ElementFactory::createGuideLine(pendingPts_[0], pendingPts_[1]);
            std::vector<Element> guides;
            for (size_t i = 1; i + 1 < pendingPts_.size(); ++i)
                guides.push_back(ElementFactory::createGuideLine(pendingPts_[i], pendingPts_[i + 1]));
            if (t != "fence")
                guides.push_back(ElementFactory::createGuideLine(pendingPts_.back(), pendingPts_.front()));
            showPreview(first, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }

    // 角度直线：未拾取起点时，以光标为起点预览「按参数(angle,length)确定的直线」（要求 3）
    if (t == "line_angle" && pendingPts_.empty()) {
        if (lp.length > 1e-9) {
            Element tmp = ElementFactory::create(ElementType::Line, QStringLiteral("角度直线"), model_);
            tmp.pts = {cur, line::pointAtAngleLength(cur, lp.angleDeg, lp.length)};
            showPreview(tmp);
        } else {
            clearPreview();
        }
        return;
    }

    // 平行线：已选参考线、尚未点击方向时，按光标所在侧预览多条偏移平行线（要求 4）
    if (t == "line_parallel" && pickedIds_.size() == 1 && pendingPts_.empty()) {
        if (const Element* ref = model_->element(pickedIds_[0])) {
            line::ParallelCopies pc = line::parallelCopies(*ref, cur, lp.spacing, qMax(1, lp.count));
            if (pc.valid && !pc.lines.empty()) {
                Element tmp = ElementFactory::create(ElementType::Line, QStringLiteral("平行线"), model_);
                tmp.pts = {pc.lines[0].first, pc.lines[0].second};
                std::vector<Element> guides;
                for (size_t i = 1; i < pc.lines.size(); ++i) {
                    Element g = ElementFactory::createGuideLine(pc.lines[i].first, pc.lines[i].second);
                    guides.push_back(g);
                }
                showPreview(tmp, std::move(guides));
                return;
            }
        }
        clearPreview();
        return;
    }

    // 过点平行线：已选参考线、尚未点击点时，按光标位置预览对齐线（要求 5）
    if (t == "line_parallel_pt" && pickedIds_.size() == 1 && pendingPts_.empty()) {
        if (const Element* ref = model_->element(pickedIds_[0])) {
            auto r = line::parallelAligned(*ref, cur);
            if (r.valid) {
                Element tmp = ElementFactory::create(ElementType::Line, QStringLiteral("过点平行线"), model_);
                tmp.pts = {r.a, r.b};
                showPreview(tmp);
                return;
            }
        }
        clearPreview();
        return;
    }

    // ---- 圆形子模式预览（circle_2p / circle_cr / circle_t2e1p ... 共10种新工具） ----
    if (isCircleSubTool(t)) {
        const circle::Mode cm = circle::modeFromString(t);
        const circle::ReqCount rc = circle::requirements(cm);
        const auto& lp = model_->lineParams();
        const double rad = rc.usesParam ? lp.length : 0.0;
        circle::Result r = circle::compute(cm, pendingPts_, pickedIds_, cur,
                                           model_->elements(), rad);
        if (r.valid) {
            Element tmp = ElementFactory::create(ElementType::Circle, QStringLiteral("圆"), model_);
            tmp.pts = {r.center}; tmp.radius = r.radius;
            std::vector<Element> guides;
            // 备选解用虚线圆提示
            for (const auto& ac : r.altCenters) {
                Element g = ElementFactory::createGuideCircle(ac, r.radius);
                guides.push_back(g);
            }
            showPreview(tmp, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }

    // ---- 圆弧子模式预览（arc_3p / arc_tpr / arc_tan ... 共10种新工具） ----
    if (isArcSubTool(t)) {
        const arc::Mode am = arc::modeFromString(t);
        const arc::ReqCount rc = arc::requirements(am);
        const auto& lp = model_->lineParams();
        // 参数条取值：角度模式用 angleDeg，其余用 length（作为半径/弦长）
        const double param = rc.usesParam
            ? (ToolRegistry::instance().paramMode(t) == ParamBarMode::AngleLength
               ? lp.angleDeg : lp.length)
            : 0.0;
        arc::Result r = arc::compute(am, pendingPts_, pickedIds_, cur, model_->elements(), param);
        if (r.valid) {
            Element tmp = ElementFactory::create(ElementType::Arc, QStringLiteral("圆弧"), model_);
            tmp.pts = {r.center}; tmp.radius = r.radius;
            tmp.startAngle = r.startAngle; tmp.endAngle = r.endAngle;
            std::vector<Element> guides;
            guides.push_back(ElementFactory::createGuideCircle(r.center, r.radius));  // 定位圆
            for (const auto& ac : r.altCenters)
                guides.push_back(ElementFactory::createGuideCircle(ac, r.radius));
            showPreview(tmp, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }

    // ---- 椭圆子模式预览（ellipse_1pt / ellipse_axis / ellipse_foci / ellipse_4p / ellipse_c3p / ellipse_inscribed）----
    if (isEllipseSubTool(t)) {
        const ellipse::Mode em = ellipse::modeFromString(t);
        const ellipse::ReqCount rc = ellipse::requirements(em);
        const double param = rc.usesParam ? lp.length : 0.0;
        ellipse::Result r = ellipse::compute(em, pendingPts_, pickedIds_, cur,
                                             model_->elements(), param);
        if (r.valid) {
            Element tmp = ElementFactory::createEllipse(r.center, r.radius,
                                                        r.radiusY, r.rotation, model_);
            std::vector<Element> guides;
            // 已拾取的点用短十字参考线标出，便于看清拟合依据
            auto gCross = [&](const Vec3& p) {
                const double k = 6.0 / zoom_;
                guides.push_back(ElementFactory::createGuideLine(
                    Vec3(p.x - k, p.y, 0), Vec3(p.x + k, p.y, 0)));
                guides.push_back(ElementFactory::createGuideLine(
                    Vec3(p.x, p.y - k, 0), Vec3(p.x, p.y + k, 0)));
            };
            for (const auto& p : pendingPts_) gCross(p);
            // 长轴方向提示线
            const double ar = math::deg2rad(r.rotation);
            const Vec3 ax(r.center.x + r.radius * std::cos(ar),
                          r.center.y + r.radius * std::sin(ar), 0);
            const Vec3 bx(r.center.x - r.radiusY * std::sin(ar),
                          r.center.y + r.radiusY * std::cos(ar), 0);
            guides.push_back(ElementFactory::createGuideLine(r.center, ax));
            guides.push_back(ElementFactory::createGuideLine(r.center, bx));
            showPreview(tmp, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }

    // ---- 多边形子模式预览（poly_regular / poly_circumscribed / poly_edge / rect_chamfer / rect_fillet / poly_star）----
    if (isPolySubTool(t)) {
        const polygon::Mode pm = polygon::modeFromString(t);
        const auto& lp = model_->lineParams();
        const int sides = std::max(3, lp.count);
        const double param = lp.length;  // 倒角距离 / 圆角半径
        polygon::Result r = polygon::compute(pm, pendingPts_, pickedIds_, cur,
                                              model_->elements(), param, sides);
        if (r.valid) {
            Element tmp = ElementFactory::create(r.type, r.type == ElementType::Rectangle
                ? QStringLiteral("矩形") : QStringLiteral("多边形"), model_);
            tmp.pts = r.pts;
            std::vector<Element> guides;
            // 备选解用虚线多边形提示
            for (const auto& alt : r.altPolys) {
                Element g = ElementFactory::create(ElementType::Polygon, QString(), model_);
                g.pts = alt;
                guides.push_back(g);
            }
            showPreview(tmp, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }
    // 选完两个圆后，鼠标移动实时计算所有切线（外+内），高亮离鼠标最近的那条
    if (t == "line_tangent_cc" && pickedIds_.size() >= 2 && pendingPts_.empty()) {
        line::Result r = line::compute(line::Mode::TangentCC, {}, pickedIds_, cur,
                                       model_->elements(), 0.0);
        if (r.valid) {
            Element tmp = ElementFactory::create(ElementType::Line, QStringLiteral("公切线"), model_);
            tmp.pts = {r.a, r.b};
            std::vector<Element> guides;
            for (const auto& alt : r.altLines) {
                Element g = ElementFactory::createGuideLine(alt.first, alt.second);
                guides.push_back(g);
            }
            showPreview(tmp, std::move(guides));
        } else {
            clearPreview();
        }
        return;
    }

    // ---- 构造线预览（xline / xline_h / xline_v / xline_angle / xline_2pt / xline_bisector / xline_parallel）----
    if (t == "xline" || isXlineSubTool(t)) {
        previewXline(cur);
        return;
    }

    // 标注两步流程预览：已选图元，鼠标位置决定尺寸线偏置方向
    if (dimPending_) {
        previewDimension(cur);
        return;
    }

    if (pendingPts_.empty()) { clearPreview(); return; }
    const Vec3 a = pendingPts_[0];

    Element tmp = ElementFactory::create(ElementType::Line, QString(), model_);
    std::vector<Element> guides;
    auto gLine = [&](const Vec3& p, const Vec3& q) {
        Element g = ElementFactory::createGuideLine(p, q); guides.push_back(g);
    };
    auto gCircle = [&](const Vec3& c, double r) {
        Element g = ElementFactory::createGuideCircle(c, r); guides.push_back(g);
    };

    // 水平/垂直线：拖动第二点时实时显示水平线/垂直线（要求 2）
    if (t == "line_h" || t == "line_v") {
        const Vec3 b = (t == "line_h") ? Vec3(cur.x, a.y, 0) : Vec3(a.x, cur.y, 0);
        tmp.type = ElementType::Line; tmp.pts = {a, b};
        showPreview(tmp);
        return;
    }

    if (t == "line") {
        tmp.type = ElementType::Line; tmp.pts = {a, cur};
    } else if (isLineSubTool(t)) {
        line::Result r = line::compute(line::modeFromString(t), pendingPts_, pickedIds_, cur,
                                       model_->elements(), lineAngleParam_);
        if (r.valid) { tmp.type = ElementType::Line; tmp.pts = {r.a, r.b}; }
        else { tmp.type = ElementType::Line; tmp.pts = {a, cur}; }
    } else if (t == "rect") {
        tmp.type = ElementType::Rectangle; tmp.pts = {a, cur};
    } else if (t == "circle") {
        tmp.type = ElementType::Circle; tmp.pts = {a}; tmp.radius = a.distTo(cur);
        gLine(a, cur);                                   // 半径线，构造关系一眼可见
    } else if (t == "circle3p") {
        if (pendingPts_.size() == 1) {
            tmp.type = ElementType::Line; tmp.pts = {a, cur};
        } else {
            Vec3 c; double r = 0;
            if (geo::circleFrom3Points(a, pendingPts_[1], cur, c, r)) {
                tmp.type = ElementType::Circle; tmp.pts = {c}; tmp.radius = r;
                gLine(a, pendingPts_[1]); gLine(pendingPts_[1], cur);
            } else {                                      // 三点共线：退化成弦线提示
                tmp.type = ElementType::Line; tmp.pts = {a, cur};
            }
        }
    } else if (t == "arc") {
        if (pendingPts_.size() == 1) {
            // 只点了圆心时也给出反馈（旧版此阶段完全没有预览，容易以为没生效）
            tmp.type = ElementType::Circle; tmp.pts = {a}; tmp.radius = a.distTo(cur);
            gLine(a, cur);
        } else {
            const double r = a.distTo(pendingPts_[1]);
            const double s = (pendingPts_[1] - a).angleDeg();
            trackArcAngle((cur - a).angleDeg());
            tmp.type = ElementType::Arc; tmp.pts = {a}; tmp.radius = r;
            tmp.startAngle = s; tmp.endAngle = s + arcSweep_;
            gCircle(a, r);
            gLine(a, pendingPts_[1]);
            gLine(a, cur);                                // 辅助线一直连到光标，消除"弧与光标脱节"的观感
        }
    } else if (isSplineSubTool(t)) {
        // 样条子模式预览（抛物线 / 双曲线）
        const spline::Mode sm = spline::modeFromString(t);
        const spline::ReqCount src = spline::requirements(sm);
        if ((int)pendingPts_.size() >= src.pts) {
            // 已有足够点 + 光标 → 计算曲线预览
            spline::Result r = spline::compute(sm, pendingPts_, cur);
            if (r.valid) {
                tmp.type = ElementType::Spline;
                tmp.splineSubType = r.subType;
                tmp.pts = r.pts;
                tmp.closed = false;
            }
        } else if (!pendingPts_.empty()) {
            // 点不够时用辅助线提示已拾取的点
            tmp.type = ElementType::Line;
            tmp.pts = {pendingPts_.back(), cur};
            for (const auto& p : pendingPts_)
                gCircle(p, 2.0 / zoom_);
        }
    } else if (t == "poly" || t == "spline") {
        tmp.type = (t == "poly") ? ElementType::Polygon : ElementType::Spline;
        tmp.pts = pendingPts_;
        tmp.pts.push_back(cur);
    } else {
        bool isDim = false;
        const DimensionType dt = dimToolType(t, isDim);
        if (!isDim) { clearPreview(); return; }
        // 标注预览：把「已拾取点 + 当前光标」直接喂给国标生成器，
        // 所见即所得 —— 尺寸线往哪偏、数字朝哪转，拖动时就能看见。
        tmp.type = ElementType::Dimension;
        tmp.dtype = dt;
        tmp.standard = model_->dimensionStandard();
        tmp.pts = pendingPts_;
        tmp.pts.push_back(cur);
        const bool enough = (dt == DimensionType::Angle) ? (tmp.pts.size() >= 3)
                                                         : (tmp.pts.size() >= 2);
        if (!enough || !dim::build(tmp).valid) {   // 点还不够或退化，先用一条辅助线示意
            tmp.type = ElementType::Line;
            tmp.pts = {a, cur};
            tmp.dtype = DimensionType::Distance;
        }
    }
    showPreview(tmp, std::move(guides));
}

void GraphicsView::resetPending() {
    pendingPts_.clear();
    pickedIds_.clear();
    trimBounds_.clear();
    if (gripHot_) {   // 夹点热态一并复位（静默：日志由具体取消路径输出）
        gripHot_ = gripArmed_ = gripDragMoved_ = false;
        gripData_ = grip::Grips{};
    }
    clearGripPreview();
    drawing_ = false;
    panning_ = false;
    panRight_ = false;
    rightMoved_ = false;
    snapNow_ = snap::SnapResult();
    arcTracking_ = false;
    arcSweep_ = 0.0;
    chainHinted_ = false;
    marqueeActive_ = false;
    marquee_ = QRectF();
    dimPending_ = false;
    dimTargetId_ = -1;
    dimEdgeIdx_ = -1;
    clearPreview();
    hideDynBox();          // DYN 输入框随绘制会话结束而隐藏
    viewport()->update();
}

void GraphicsView::cancelDrawing() {
    if (regionPickMode_) {           // 区域拾取模式（剖面填充）也一并退出
        setRegionPickMode(false);
        emit regionPickCancelled();
    }
    const bool had = dimPending_ || drawing_ || !pendingPts_.empty() || !pickedIds_.empty() || !trimBounds_.empty();
    resetPending();
    model_->log(had ? QStringLiteral("已取消当前操作") : QStringLiteral("当前没有进行中的操作"),
                had ? "warn" : "info");
    updateCommandPrompt();
}

// ============================ 分段式命令提示 ============================

void GraphicsView::updateCommandPrompt() {
    // Strategy Pattern：提示文本由活动工具的 prompt() 方法生成，
    // 取代原先 140 行的 if-else 链
    const QString prompt = toolMgr_->dispatchPrompt();
    model_->setCommandPrompt(prompt);
}

void GraphicsView::injectPoint(const Vec3& p) {
    // 将世界坐标转换为视口坐标（供 hitTest / pickRefElement 使用）
    const QPointF scenePos(p.x, p.y);
    const QPoint viewPos = mapFromScene(scenePos);

    // 构造合成鼠标事件（Qt6 需提供 globalPos）
    QMouseEvent fakeEvent(QEvent::MouseButtonPress,
                          QPointF(viewPos),        // localPos (视口坐标)
                          mapToGlobal(viewPos),    // globalPos (屏幕坐标)
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    // 键盘输入时跳过智能捕捉，使用精确坐标
    keyboardInputActive_ = true;
    handlePress(p, &fakeEvent);
    keyboardInputActive_ = false;

    // 在注入点处更新预览
    if (drawing_) updatePreview(p);
}

// ---------------------------- 编辑类工具 ----------------------------

// 工具标识 -> 标注类型。ok=false 表示这不是一个标注工具。
DimensionType GraphicsView::dimToolType(const QString& tool, bool& ok) {
    DimensionType dt;
    ok = ToolRegistry::instance().tryDimensionType(tool, dt);
    return ok ? dt : DimensionType::Distance;
}

// ============================ 标注两步流程 ============================
// 流程：选【标注】命令 → 点击选图元 → 移动鼠标动态预览 → 点击确认位置
// 直线：保留对齐标注方式，点击位置决定尺寸线偏置方向
// 圆/弧：标注直径或半径，右键可切换（点击选择）
// 多边形/矩形：标注鼠标点击的那条边
void GraphicsView::handleDimension(const Vec3& w, DimensionType dt) {
    const QString tname = QString::fromUtf8(dimensionTypeName(dt));

    // ---- 角度标注：保留原有三点拾取流程 ----
    if (dt == DimensionType::Angle) {
        pendingPts_.push_back(w);
        if (pendingPts_.size() == 1) { model_->log(tname + QStringLiteral("：已拾取角顶点，请拾取第 1 条边上的点")); return; }
        if (pendingPts_.size() == 2) { model_->log(tname + QStringLiteral("：请拾取第 2 条边上的点")); return; }
        Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
        el.dtype = dt;
        el.standard = model_->dimensionStandard();
        el.pts = {pendingPts_[0], pendingPts_[1], pendingPts_[2]};
        const dim::Geom g = dim::build(el);
        if (!g.valid) {
            model_->log(tname + QStringLiteral("：两边重合或长度为零，无法标注"), "warn");
            resetPending();
            return;
        }
        const int id = commitElement(el);
        model_->log(QString("已创建%1 #%2 = %3").arg(tname).arg(id).arg(g.label));
        return;
    }

    // ---- 引线 / 坐标标注：自由点拾取（不走"选图元"流程）----
    // 复用 updatePreview 的通用标注预览分支（pendingPts_ + 光标直接喂给生成器），
    // 因此无需设置 dimPending_，移动鼠标时即可看到引线/坐标的实时预览。
    if (dt == DimensionType::Leader || dt == DimensionType::Ordinate) {
        pendingPts_.push_back(w);
        const int n = (int)pendingPts_.size();
        if (dt == DimensionType::Leader) {
            if (n < 2) { model_->log(tname + QStringLiteral("：已拾取起点，请拾取下一点（引线折点）")); return; }
        } else { // Ordinate
            if (n < 2) { model_->log(tname + QStringLiteral("：已拾取被测点，请拾取坐标基准点")); return; }
        }
        Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
        el.dtype = dt;
        el.standard = model_->dimensionStandard();
        el.pts = pendingPts_;
        if (dt == DimensionType::Ordinate) el.rotation = 0.0;   // 默认标注 X 坐标（后续可切换 Y）
        const dim::Geom g = dim::build(el);
        if (!g.valid) {
            model_->log(tname + QStringLiteral("：数据不足，无法标注"), "warn");
            resetPending();
            return;
        }
        const int id = commitElement(el);
        model_->log(QString("已创建%1 #%2").arg(tname).arg(id));
        return;
    }

    // ---- 第2步：确认标注位置（已选图元，等待点击确认）----
    if (dimPending_) {
        finalizeDimension(w);
        return;
    }

    // ---- 第1步：选择图元 ----
    const int hit = hitTest(mapFromScene(QPointF(w.x, w.y)));
    if (hit < 0) {
        model_->log(tname + QStringLiteral("：请点选要标注的图元"), "warn");
        return;
    }
    const Element* e = model_->element(hit);
    if (!e) return;

    if (e->type == ElementType::Line && e->pts.size() >= 2) {
        // 直线：保留对齐标注方式
        dimTargetId_ = hit;
        dimEdgeIdx_ = -1;
        // 基线/连续标注保留自身类型（其生成器内部按 e.radius 层叠偏移，不走 lineDimType 归一）
        dimPreviewType_ = (dt == DimensionType::Baseline || dt == DimensionType::Continue)
                              ? dt : geo::lineDimType(dt);
        dimPending_ = true;
        model_->log(QString("已选直线 #%1，移动鼠标预览标注，点击确认").arg(hit));
    } else if (e->type == ElementType::Circle || e->type == ElementType::Arc) {
        // 圆/弧：标注直径或半径
        dimTargetId_ = hit;
        dimEdgeIdx_ = -1;
        dimPreviewType_ = (dt == DimensionType::Radius) ? DimensionType::Radius
                            : (dt == DimensionType::Arc) ? DimensionType::Arc
                                                         : DimensionType::Diameter;
        dimPending_ = true;
        model_->log(QString("已选圆 #%1，移动鼠标预览方向，点击确认（右键切换直径/半径）").arg(hit));
    } else if (e->type == ElementType::Polygon || e->type == ElementType::Rectangle) {
        // 多边形/矩形：找离点击最近的边
        const int ei = findNearestEdge(*e, w);
        if (ei < 0) {
            model_->log(tname + QStringLiteral("：未找到有效边"), "warn");
            return;
        }
        dimTargetId_ = hit;
        dimEdgeIdx_ = ei;
        dimPreviewType_ = DimensionType::Aligned;
        dimPending_ = true;
        model_->log(QString("已选多边形 #%1 第 %2 条边，移动鼠标预览标注，点击确认")
                        .arg(hit).arg(ei + 1));
    } else {
        model_->log(tname + QStringLiteral("：暂不支持此图元类型"), "warn");
    }
}

// 第2步：点击确认 → 正式创建标注
void GraphicsView::finalizeDimension(const Vec3& w) {
    const Element* e = model_->element(dimTargetId_);
    if (!e) { resetPending(); return; }

    const QString tname = QString::fromUtf8(dimensionTypeName(dimPreviewType_));

    if (e->type == ElementType::Line && e->pts.size() >= 2) {
        // ---- 直线标注 ----
        const Vec3 A = e->pts[0], B = e->pts[1];
        Vec3 dir = B - A;
        const double L = std::hypot(dir.x, dir.y);
        if (L < 1e-9) { resetPending(); return; }
        dir = Vec3(dir.x / L, dir.y / L, 0);
        const Vec3 n(-dir.y, dir.x, 0);
        double perp = (w.x - A.x) * n.x + (w.y - A.y) * n.y;
        if (std::fabs(perp) < 5.0) perp = (perp >= 0 ? 1.0 : -1.0) * 12.0;
        const Vec3 pos = (A + B) * 0.5 + Vec3(n.x * perp, n.y * perp, 0);
        Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
        el.dtype = dimPreviewType_;
        el.standard = model_->dimensionStandard();
        el.refA = e->id;
        el.pts = {A, B, pos};
        const dim::Geom g = dim::build(el);
        if (g.valid) {
            const int id = commitElement(el);
            model_->log(QString("已创建%1 #%2 = %3 mm（%4）").arg(tname).arg(id).arg(g.label)
                        .arg(dimensionStandardName(model_->dimensionStandard())));
        }
    } else if (e->type == ElementType::Circle || e->type == ElementType::Arc) {
        // ---- 圆/弧标注：直径或半径 ----
        Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
        el.dtype = dimPreviewType_;
        el.standard = model_->dimensionStandard();
        el.radius = e->radius;
        el.refA = e->id;
        if (el.dtype == DimensionType::Arc) {
            // 弧长标注沿用源圆弧的起止角；若源是整圆则默认 0°~90° 一段弧
            el.startAngle = (e->type == ElementType::Arc) ? e->startAngle : 0.0;
            el.endAngle   = (e->type == ElementType::Arc) ? e->endAngle   : 90.0;
        }
        Vec3 dir = w - e->pts[0];
        if (dir.len() < 1e-9) dir = Vec3(1, 0, 0);
        el.pts = {e->pts[0], e->pts[0] + dir};
        const int id = commitElement(el);
        model_->log(QString("已创建%1 #%2：%3").arg(tname).arg(id).arg(dim::build(el, model_->dimStyle()).label));
    } else if ((e->type == ElementType::Polygon || e->type == ElementType::Rectangle)
               && dimEdgeIdx_ >= 0) {
        // ---- 多边形边标注 ----
        Vec3 A, B;
        edgeEndpoints(*e, dimEdgeIdx_, A, B);
        Vec3 dir = B - A;
        const double L = std::hypot(dir.x, dir.y);
        if (L < 1e-9) { resetPending(); return; }
        dir = Vec3(dir.x / L, dir.y / L, 0);
        const Vec3 n(-dir.y, dir.x, 0);
        double perp = (w.x - A.x) * n.x + (w.y - A.y) * n.y;
        if (std::fabs(perp) < 5.0) perp = (perp >= 0 ? 1.0 : -1.0) * 12.0;
        const Vec3 pos = (A + B) * 0.5 + Vec3(n.x * perp, n.y * perp, 0);
        Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
        el.dtype = DimensionType::Aligned;
        el.standard = model_->dimensionStandard();
        el.refA = e->id;
        el.pts = {A, B, pos};
        const dim::Geom g = dim::build(el);
        if (g.valid) {
            const int id = commitElement(el);
            model_->log(QString("已创建%1 #%2 = %3 mm（%4）").arg(tname).arg(id).arg(g.label)
                        .arg(dimensionStandardName(model_->dimensionStandard())));
        }
    }

    resetPending();
}

// 标注动态预览：根据已选图元 + 当前光标位置生成预览标注
void GraphicsView::previewDimension(const Vec3& cur) {
    const Element* e = model_->element(dimTargetId_);
    if (!e) { clearPreview(); return; }

    Element tmp = ElementFactory::create(ElementType::Dimension, QString(), model_);
    tmp.dtype = dimPreviewType_;
    tmp.standard = model_->dimensionStandard();

    if (e->type == ElementType::Line && e->pts.size() >= 2) {
        Vec3 A = e->pts[0], B = e->pts[1];
        Vec3 pos;
        if (alignedDimOffset(A, B, cur, pos)) {
            tmp.pts = {A, B, pos};
            tmp.refA = e->id;
        }
    } else if (e->type == ElementType::Circle || e->type == ElementType::Arc) {
        Vec3 dir = cur - e->pts[0];
        if (dir.len() < 1e-9) dir = Vec3(1, 0, 0);
        tmp.pts = {e->pts[0], e->pts[0] + dir};
        tmp.radius = e->radius;
        tmp.refA = e->id;
        if (tmp.dtype == DimensionType::Arc) {
            tmp.startAngle = (e->type == ElementType::Arc) ? e->startAngle : 0.0;
            tmp.endAngle   = (e->type == ElementType::Arc) ? e->endAngle   : 90.0;
        }
    } else if ((e->type == ElementType::Polygon || e->type == ElementType::Rectangle)
               && dimEdgeIdx_ >= 0) {
        Vec3 A, B;
        edgeEndpoints(*e, dimEdgeIdx_, A, B);
        Vec3 pos;
        if (alignedDimOffset(A, B, cur, pos)) {
            tmp.pts = {A, B, pos};
            tmp.refA = e->id;
        }
    }

    if (!tmp.pts.empty() && dim::build(tmp).valid)
        showPreview(tmp);
    else
        clearPreview();
}

// 对齐标注尺寸线偏置：由边 A-B 与光标 cur 计算尺寸线落点。
// 直线与多边形/矩形边共用同一定位规则（DRY：消除 previewDimension 中两处近似重复）。
bool GraphicsView::alignedDimOffset(const Vec3& A, const Vec3& B, const Vec3& cur, Vec3& pos) const {
    Vec3 dir = B - A;
    const double L = std::hypot(dir.x, dir.y);
    if (L <= 1e-9) return false;
    dir = Vec3(dir.x / L, dir.y / L, 0);
    const Vec3 n(-dir.y, dir.x, 0);
    double perp = (cur.x - A.x) * n.x + (cur.y - A.y) * n.y;
    if (std::fabs(perp) < 5.0) perp = (perp >= 0 ? 1.0 : -1.0) * 12.0;
    pos = (A + B) * 0.5 + Vec3(n.x * perp, n.y * perp, 0);
    return true;
}

// 多边形/矩形：找到离点击点 p 最近的边的索引
int GraphicsView::findNearestEdge(const Element& e, const Vec3& p) const {
    if (e.pts.size() < 2) return -1;
    const bool closed = e.polyClosed();
    const int n = (int)e.pts.size();
    int bestIdx = -1;
    double bestDist = 1e18;

    for (int i = 0; i < n - 1; ++i) {
        const double d = geo::distPointSegment(p, e.pts[i], e.pts[i + 1]);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    if (closed && n >= 3) {
        const double d = geo::distPointSegment(p, e.pts[n - 1], e.pts[0]);
        if (d < bestDist) { bestDist = d; bestIdx = n - 1; }
    }
    return bestIdx;
}

// 取多边形/矩形第 idx 条边的两个端点
void GraphicsView::edgeEndpoints(const Element& e, int idx, Vec3& a, Vec3& b) const {
    const int n = (int)e.pts.size();
    if (idx < 0 || idx >= n) { a = b = Vec3(); return; }
    if (idx == n - 1 && e.polyClosed()) {
        a = e.pts[n - 1];
        b = e.pts[0];
    } else if (idx + 1 < n) {
        a = e.pts[idx];
        b = e.pts[idx + 1];
    } else {
        a = b = Vec3();
    }
}

void GraphicsView::handleConstraint(int hitId) {
    if (hitId < 0) { model_->log(QStringLiteral("约束：请点选图元"), "warn"); return; }
    if (!pickedIds_.empty() && pickedIds_[0] == hitId) {
        model_->log(QStringLiteral("约束：请选择不同的第 2 个图元"), "warn");
        return;
    }
    pickedIds_.push_back(hitId);
    if (pickedIds_.size() < 2) {
        model_->log(QString("约束：已选中 #%1，请拾取第 2 个图元").arg(hitId));
        return;
    }
    Element el = ElementFactory::create(ElementType::Constraint, QString(), model_);
    el.ctype = ConstraintType::Coincident;
    el.name = QString("约束-") + constraintTypeName(el.ctype);
    el.refA = pickedIds_[0];
    el.refB = pickedIds_[1];
    const Element* ea = model_->element(el.refA);
    if (ea && !ea->pts.empty()) el.pts = {ea->pts[0]};
    const int id = commitElement(el);
    model_->log(QString("已建立约束 #%1：#%2 ↔ #%3（可在属性面板改类型；求解器为后续规划项）")
                    .arg(id).arg(el.refA).arg(el.refB));
}

void GraphicsView::handleTrimExtend(int hitId, const Vec3& w, bool /*extend*/) {
    // 对称「修剪 / 延伸」：首点点选第 1 条直线，次点点选第 2 条直线，
    // 计算两线所在直线交点后自动判定——线段已相交则修剪多余部分，
    // 线段不相交则分别延长到交点；每条只移动离点选端最远的端点（保留端不变）。
    const QString op = QStringLiteral("修剪/延伸");
    if (hitId < 0) { model_->log(op + QStringLiteral("：请点选直线"), "warn"); return; }
    const Element* e = model_->element(hitId);
    if (!e || e->type != ElementType::Line) {
        model_->log(op + QStringLiteral("：当前仅支持直线图元"), "warn");
        return;
    }
    if (pickedIds_.empty()) {
        pickedIds_.push_back(hitId);
        pendingPts_.push_back(w);
        model_->log(op + QStringLiteral("：已选第 1 条 #%1，请拾取第 2 条直线（点中选中的那一端会被保留）").arg(hitId));
        return;
    }
    if (pickedIds_[0] == hitId) {
        model_->log(op + QStringLiteral("：第 2 条不能是同一条直线"), "warn");
        resetPending();
        return;
    }
    const Element* a = model_->element(pickedIds_[0]);
    if (!a || a->pts.size() < 2) { resetPending(); return; }

    Element oa, ob; bool ca = false, cb = false; QString msg;
    if (!trim::extendTrimPair(*a, pendingPts_[0], *e, w, oa, ca, ob, cb, msg)) {
        model_->log(op + QStringLiteral("：") + msg, "warn");
        resetPending();
        return;
    }

    // 两次改动合并为一次撤销（Ctrl+Z 整体回退）
    model_->beginEdit(QStringLiteral("修改-修剪/延伸"));
    if (ca) model_->updateElementQuiet(a->id, [&](Element& el) { el.pts = oa.pts; });
    if (cb) model_->updateElementQuiet(e->id,  [&](Element& el) { el.pts = ob.pts; });

    Vec3 P; math::lineLineIntersect(a->pts[0], a->pts[1], e->pts[0], e->pts[1], P);
    const int n = (ca ? 1 : 0) + (cb ? 1 : 0);
    model_->setSelected(e->id);
    model_->log(QString("已%1 %2 条直线，交于 (%3, %4)")
                    .arg(op).arg(n).arg(P.x, 0, 'f', 2).arg(P.y, 0, 'f', 2));
    resetPending();
}

// 全图元剪切：直线 / 圆 / 圆弧 / 矩形 / 多边形 / 样条 均可剪。
// 交互分两档 ——
//   * 快速剪切：直接点要剪掉的那一段，场景里其余可见图元自动充当边界；
//   * 指定边界：Shift+点击先挑若干条边界线，再点目标，只用这些边界求交。
// 两档共用同一套算法，差别只是传给引擎的 cutters 集合。
void GraphicsView::handleTrimAny(int hitId, const Vec3& w, bool shift) {
    if (shift) {
        if (hitId < 0) { model_->log(QStringLiteral("剪切：Shift+点击用于指定边界，请点在图元上"), "warn"); return; }
        auto it = std::find(trimBounds_.begin(), trimBounds_.end(), hitId);
        if (it != trimBounds_.end()) {
            trimBounds_.erase(it);
            model_->log(QString("剪切：已取消边界 #%1（当前 %2 条）").arg(hitId).arg(trimBounds_.size()));
        } else {
            trimBounds_.push_back(hitId);
            model_->log(QString("剪切：已指定边界 #%1（当前 %2 条），再点击要剪掉的部分")
                            .arg(hitId).arg(trimBounds_.size()));
        }
        viewport()->update();
        return;
    }

    if (hitId < 0) {
        model_->log(QStringLiteral("剪切：请点击要剪掉的那一段（Shift+点击可先指定边界）"), "warn");
        return;
    }
    if (std::find(trimBounds_.begin(), trimBounds_.end(), hitId) != trimBounds_.end()) {
        model_->log(QStringLiteral("剪切：该图元已被指定为边界，不能同时作为剪切目标"), "warn");
        return;
    }
    const Element* tgt = model_->element(hitId);
    if (!tgt) return;

    std::vector<Element> cutters;
    if (!trimBounds_.empty()) {
        for (int bid : trimBounds_)
            if (const Element* b = model_->element(bid)) cutters.push_back(*b);
    } else {
        for (const auto& e : model_->elements())
            if (e.id != hitId && e.visible &&
                e.type != ElementType::Constraint && e.type != ElementType::Dimension)
                cutters.push_back(e);
    }

    std::vector<Element> parts;
    QString msg;
    if (!trim::trimElement(*tgt, cutters, w, parts, msg)) {
        model_->log(QStringLiteral("剪切：") + msg, "warn");
        return;
    }

    const QString tn = tgt->typeName();
    if (parts.empty()) {
        model_->removeElement(hitId);
        model_->log(QString("已剪切%1 #%2：整体位于边界之间，已移除").arg(tn).arg(hitId));
    } else {
        const auto ids = model_->replaceElement(hitId, parts, QStringLiteral("剪切") + tn);
        QStringList ss;
        for (int i : ids) ss << ("#" + QString::number(i));
        model_->log(QString("已剪切%1 #%2 → 保留 %3 段：%4")
                        .arg(tn).arg(hitId).arg(ids.size()).arg(ss.join(", ")));
    }
    // 边界是一次性的：剪完就清掉，避免用户以为还生效而误剪
    if (!trimBounds_.empty()) { trimBounds_.clear(); model_->log(QStringLiteral("剪切：指定边界已重置")); }
    viewport()->update();
}

// ---------------------------- 端点捕捉与联动拖拽 ----------------------------

void GraphicsView::refreshHover(const Vec3& w) {
    const bool wasOn = hoverHandle_;
    const Vec3 oldPos = hoverPos_;
    hoverHandle_ = false;
    if (model_->activeTool() == "select") {
        const auto h = model_->handleAt(w, worldTol());
        if (h.valid()) {
            const Element* e = model_->element(h.elemId);
            if (e) {
                const auto hs = geo::elementHandles(*e);
                if (h.index < (int)hs.size()) {
                    hoverHandle_ = true;
                    hoverPos_ = hs[h.index];
                    hoverLinked_ = (int)model_->collectLinked(hoverPos_, worldTol()).size();
                }
            }
        }
    }
    if (wasOn != hoverHandle_ ||
        (hoverHandle_ && (std::fabs(oldPos.x - hoverPos_.x) > 1e-12 ||
                          std::fabs(oldPos.y - hoverPos_.y) > 1e-12)))
        viewport()->update();
}

bool GraphicsView::beginGripHot(const Vec3& w) {
    const auto h = model_->handleAt(w, worldTol());
    if (!h.valid()) return false;
    const Element* e = model_->element(h.elemId);
    if (!e) return false;
    const auto hs = geo::elementHandles(*e);
    if (h.index >= (int)hs.size()) return false;

    gripData_ = grip::Grips{};
    gripData_.base = hs[h.index];
    gripData_.refCursor = w;

    // Stretch 联动引用在进入热态的瞬间锁定（与原端点拖拽同语义）：
    // 与该控制点重合的所有控制点一并迁移，保持拓扑连接；
    // 圆/弧圆心手柄不参与端点联动，但被直接抓取时单独补入。
    auto refs = model_->collectLinked(gripData_.base, worldTol());
    if (std::none_of(refs.begin(), refs.end(),
                     [&](const SketchModel::HandleRef& r) {
                         return r.elemId == h.elemId && r.index == h.index;
                     }))
        refs.push_back({h.elemId, h.index});

    gripData_.stretchRefs.clear();
    std::vector<int> stretchIds;
    for (const auto& r : refs) {
        gripData_.stretchRefs.push_back({r.elemId, r.index});
        if (std::find(stretchIds.begin(), stretchIds.end(), r.elemId) == stretchIds.end())
            stretchIds.push_back(r.elemId);
    }

    // 作用集快照：moveSet = 选中集（剔除 Constraint，变换无意义）；
    // stretchSet = 联动图元 ∪ 选中集（Stretch 可能带着未选中的相连图元走）。
    const auto& sel = model_->selection();
    for (int id : sel) {
        if (const Element* el = model_->element(id)) {
            if (el->type != ElementType::Constraint) gripData_.moveSet.push_back(*el);
            if (std::find(stretchIds.begin(), stretchIds.end(), id) == stretchIds.end())
                stretchIds.push_back(id);
        }
    }
    for (int id : stretchIds)
        if (const Element* el = model_->element(id)) gripData_.stretchSet.push_back(*el);

    model_->setSelected(h.elemId);
    gripHot_ = true;
    gripArmed_ = false;
    gripDragMoved_ = false;
    gripMode_ = grip::Mode::Stretch;
    model_->log(QStringLiteral("夹点拉伸：移动光标预览，按住拖拽或再次单击确认，Ctrl 切换模式，Esc 取消"));
    viewport()->update();
    return true;
}

void GraphicsView::updateGripPreview(const Vec3& w) {
    clearGripPreview();
    const grip::GripResult r = grip::apply(gripData_, gripMode_, w);
    if (r.reject.isEmpty() && !r.changes.empty()) {
        QPainterPath path;
        for (const auto& c : r.changes) {
            if (c.remove) continue;
            path.addPath(ElementItem::buildPath(c.replacement));
        }
        // 预览画成虚线橙色（区别于正式图元），且不进 scene 索引参与命中
        gripPreviewItem_ = scene_->addPath(path, QPen(QColor(230, 145, 56), 0, Qt::DashLine));
        gripPreviewItem_->setZValue(1001);
    }
    viewport()->update();
}

void GraphicsView::clearGripPreview() {
    if (gripPreviewItem_) {
        scene_->removeItem(gripPreviewItem_);
        delete gripPreviewItem_;
        gripPreviewItem_ = nullptr;
    }
}

void GraphicsView::commitGrip() {
    const Vec3 cursor = lastMouseWorld_;
    const grip::GripResult r = grip::apply(gripData_, gripMode_, cursor);
    clearGripPreview();
    const QString mName = grip::modeName(gripMode_);
    if (!r.reject.isEmpty()) {
        model_->log(r.reject, "warn");          // 边界拒绝：比例≤0 / 镜像轴退化
    } else if (r.changes.empty()) {
        model_->log(QStringLiteral("夹点%1：没有可作用的图元").arg(mName), "warn");
    } else {
        // 落盘三件套（translateSelection 同款）：beginEdit 压一次快照 + 全 Quiet 应用
        model_->beginEdit(QStringLiteral("夹点%1").arg(mName));
        modify::applyChanges(model_, r.changes, QString());
        model_->log(QString("夹点%1完成：%2 项变更，Ctrl+Z 可整组撤销")
                        .arg(mName).arg(r.changes.size()));
    }
    gripHot_ = gripArmed_ = gripDragMoved_ = false;
    gripData_ = grip::Grips{};
    refreshHover(lastMouseWorld_);
    viewport()->update();
}

void GraphicsView::cancelGrip() {
    clearGripPreview();
    gripHot_ = gripArmed_ = gripDragMoved_ = false;
    gripData_ = grip::Grips{};
    model_->log(QStringLiteral("已取消夹点编辑"), "warn");
    refreshHover(lastMouseWorld_);
    viewport()->update();
}

// 栏选/圈围/圈交结算：命中判定与画布/命令行共用 shape:: 纯几何层；
// BlockRef 展开子图元判定，任一命中即整块入选。结算一律**追加**进当前选区。
void GraphicsView::finalizeShapePick() {
    const QString t = model_->activeTool();
    const shape::Mode m = (t == "fence") ? shape::Mode::Fence
                        : (t == "wp")    ? shape::Mode::Window
                                         : shape::Mode::Crossing;
    const double tol = worldTol();
    std::vector<int> ids;
    if (pendingPts_.size() >= 2) {
        for (const auto& e : model_->elements()) {
            if (!e.visible) continue;
            bool hitIt = false;
            if (e.type == ElementType::BlockRef) {
                const auto subs = model_->expandBlockRef(e);
                for (const auto& s : subs)
                    if (shape::hit(s, pendingPts_, m, tol)) { hitIt = true; break; }
            } else {
                hitIt = shape::hit(e, pendingPts_, m, tol);
            }
            if (hitIt) ids.push_back(e.id);
        }
    }
    if (ids.empty()) {
        model_->log(QString("%1：没有图元被选中").arg(shape::modeName(m)), "warn");
    } else {
        model_->addToSelection(ids);
        model_->log(QString("%1完成：%2 个图元已并入选区")
                        .arg(shape::modeName(m)).arg(ids.size()));
    }
    clearPreview();
    resetPending();
    viewport()->update();
}

void GraphicsView::drawHandles(QPainter* p) const {
    const double r = 3.5 / zoom_;
    auto box = [&](const Vec3& v, const QColor& fill, const QColor& line, double k) {
        p->setBrush(fill);
        QPen pen(line); pen.setCosmetic(true); pen.setWidthF(1.2);
        p->setPen(pen);
        p->drawRect(QRectF(v.x - r * k, v.y - r * k, 2 * r * k, 2 * r * k));
    };

    // 选中图元的全部控制点
    if (const Element* s = model_->selected()) {
        for (const auto& v : geo::elementHandles(*s))
            box(v, QColor(255, 255, 255), HL, 1.0);
    }
    // 光标下的控制点：放大并染色；有联动时用暖色提示「拖它会带走别人」
    if (hoverHandle_ && !gripHot_) {
        const bool linked = hoverLinked_ > 1;
        box(hoverPos_, linked ? QColor(230, 145, 56) : HL, QColor(31, 95, 139), 1.6);
    }
    // 热夹点：实心橙块标识当前基准点（热态中恒定显示）
    if (gripHot_)
        box(gripData_.base, QColor(230, 145, 56), QColor(31, 95, 139), 1.8);
    // 已指定的剪切边界：描一层醒目的粗线，避免「我到底选了哪几条」全靠记
    if (!trimBounds_.empty()) {
        QPen bp(QColor(214, 69, 65, 200));
        bp.setCosmetic(true); bp.setWidthF(3.0); bp.setStyle(Qt::DashLine);
        p->setPen(bp);
        p->setBrush(Qt::NoBrush);
        for (int id : trimBounds_)
            if (const Element* e = model_->element(id))
                p->drawPath(ElementItem::buildPath(*e));
    }
    p->setBrush(Qt::NoBrush);
}

// ---------------------------- 绘制主流程 ----------------------------

void GraphicsView::handlePress(const Vec3& w, QMouseEvent* e) {
    // RAII 守卫：无论从哪个分支 return，都自动更新分段命令提示
    struct PromptGuard {
        GraphicsView* v;
        ~PromptGuard() { v->updateCommandPrompt(); }
    } guard{this};

    // Strategy Pattern：左键按下事件由活动工具的 onPress() 方法处理，
    // 取代原先 80 行的 if-else 分派链
    const size_t snapN0 = pendingPts_.size();
    toolMgr_->dispatchPress(w, e);
    // 临时捕捉覆盖「单次有效」：若本次点击放置了新点，则立即清除覆盖并恢复原配置。
    if (snapOverrideActive_ && pendingPts_.size() > snapN0)
        clearSnapOverride();
}

void GraphicsView::handleRightButton() {
    // RAII 守卫：右键操作后自动更新分段命令提示
    struct PromptGuard {
        GraphicsView* v;
        ~PromptGuard() { v->updateCommandPrompt(); }
    } guard{this};

    // 标注两步流程：右键切换圆的直径/半径，或取消标注
    // （标注状态由 GraphicsView 直接管理，不经过 ToolManager）
    if (dimPending_) {
        const Element* e = model_->element(dimTargetId_);
        // 仅直径/半径标注支持右键切换；弧长标注右键直接取消（避免把类型改坏）
        if (e && (e->type == ElementType::Circle || e->type == ElementType::Arc)
            && (dimPreviewType_ == DimensionType::Radius || dimPreviewType_ == DimensionType::Diameter)) {
            dimPreviewType_ = (dimPreviewType_ == DimensionType::Diameter)
                              ? DimensionType::Radius : DimensionType::Diameter;
            model_->log(QString("已切换为%1")
                            .arg(QString::fromUtf8(dimensionTypeName(dimPreviewType_))));
            return;
        }
        resetPending();
        model_->log(QStringLiteral("已取消标注"));
        return;
    }

    // Strategy Pattern：先让活动工具处理右键（如 poly/spline 终结、snake 终结）
    if (toolMgr_->dispatchRightButton()) return;

    // 通用兜底：结束当前绘制 / 退出到选择
    if (drawing_ || !pendingPts_.empty() || !pickedIds_.empty() || !trimBounds_.empty()) {
        const QString t = model_->activeTool();
        const bool chain = (t == "line" && chainHinted_);
        clearSnapOverride();          // 结束绘制手势同样终止临时覆盖
        clearOtrackAnchors();         // 结束绘制丢弃对象追踪锚点
        trackNow_ = snap::TrackResult{};
        resetPending();
        model_->log(chain ? QStringLiteral("已结束连续直线绘制")
                          : QStringLiteral("已结束当前绘制"));
        return;
    }
    if (model_->activeTool() != "select") {
        model_->setActiveTool("select");
        model_->log(QStringLiteral("已退出绘制工具，回到选择状态"));
        return;
    }
    // 选择/空闲态：右键弹出上下文菜单（取代此前的无操作）
    showContextMenu(QCursor::pos());
}

// 上下文相关右键菜单：按「当前选择集」与「空白处」两种上下文构建。
// 绘制命令期间的右键语义（结束/取消）不受影响——只在 select 态才会走到这里。
void GraphicsView::showContextMenu(const QPoint& globalPos) {
    QMenu menu(this);
    const auto& sel = model_->selection();
    if (!sel.empty()) {
        // ---- 选中上下文 ----
        menu.addAction(QStringLiteral("特性"), this, [this] { emit requestShowProperties(); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("复制"), this, [this] { model_->copySelected(); });
        menu.addAction(QStringLiteral("删除"), this, [this] { model_->deleteSelected(); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("反选"), this, [this] { model_->invertSelection(); });
        menu.addAction(QStringLiteral("全部不选"), this, [this] { model_->clearSelection(); });
    } else {
        // ---- 空白上下文 ----
        menu.addAction(QStringLiteral("全选"), this, [this] { model_->selectAll(); });
        menu.addAction(QStringLiteral("粘贴"), this, [this] { model_->paste(); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("缩放范围"), this, [this] { fitView(); });
        menu.addAction(model_->showGrid() ? QStringLiteral("隐藏栅格")
                                          : QStringLiteral("显示栅格"),
                       this, [this] { model_->setShowGrid(!model_->showGrid()); });
    }
    menu.exec(globalPos);
}

// 命令行选项关键字应用（绘制命令期间，CommandLine::optionEntered 转发而来）。
// 仅当当前绘制确实提供了该选项才响应（避免误触）；U=放弃上一点，C=闭合（多段线）。
void GraphicsView::applyCommandOption(const QString& key) {
    const QStringList opts = model_->commandOptions();
    if (CommandPrompt::matchOption(key, opts).isEmpty()) {
        model_->log(QStringLiteral("当前命令无选项 %1").arg(key), "warn");
        return;
    }
    const QString k = key.toUpper();
    if (k == QStringLiteral("U")) {
        if (!pendingPts_.empty()) {
            pendingPts_.pop_back();
            updatePreview(lastMouseWorld_);   // 以最近光标位置重绘预览
            updateCommandPrompt();
            model_->log(QStringLiteral("已放弃上一点"));
        } else {
            model_->log(QStringLiteral("没有可放弃的点"), "info");
        }
        return;
    }
    if (k == QStringLiteral("C")) {
        if (model_->activeTool() == QStringLiteral("polyline") && pendingPts_.size() >= 3) {
            pendingPts_.push_back(pendingPts_.front());   // 回到起点闭合
            finalizePoly(false);
            model_->log(QStringLiteral("已闭合多段线"));
        } else {
            model_->log(QStringLiteral("当前命令不支持闭合选项"), "warn");
        }
        return;
    }
    model_->log(QStringLiteral("选项 %1 暂未在该命令中实现").arg(key), "info");
}

// ======================= 动态输入 DYN =======================

// 惰性创建光标跟随输入框（viewport 子控件，不进场景）。
void GraphicsView::ensureDynBox() {
    if (dynBox_) return;
    dynBox_ = new QWidget(viewport());
    dynBox_->setAutoFillBackground(true);
    dynBox_->setStyleSheet(QStringLiteral(
        "background:#fffbe6; border:1px solid #d9a300; border-radius:2px;"));
    auto* lay = new QHBoxLayout(dynBox_);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(3);
    dynLen_ = new QLineEdit(dynBox_);
    dynAng_ = new QLineEdit(dynBox_);
    dynLen_->setFixedWidth(64);
    dynAng_->setFixedWidth(64);
    dynLen_->setPlaceholderText(QStringLiteral("mm"));
    dynAng_->setPlaceholderText(QStringLiteral("°"));
    dynLen_->setFrame(false);
    dynAng_->setFrame(false);
    lay->addWidget(new QLabel(QStringLiteral("距离"), dynBox_));
    lay->addWidget(dynLen_);
    lay->addWidget(new QLabel(QStringLiteral("角度"), dynBox_));
    lay->addWidget(dynAng_);
    connect(dynLen_, &QLineEdit::returnPressed, this, &GraphicsView::commitDynInput);
    connect(dynAng_, &QLineEdit::returnPressed, this, &GraphicsView::commitDynInput);
    dynBox_->hide();
}

// 按最近采集点 → 光标的极坐标刷新预填值，并把输入框放到光标右下方。
void GraphicsView::updateDynBox(const QPoint& vp) {
    ensureDynBox();
    if (pendingPts_.empty()) { dynBox_->hide(); return; }
    const Vec3& last = pendingPts_.back();
    const double dx = lastMouseWorld_.x - last.x;
    const double dy = lastMouseWorld_.y - last.y;
    const double len = std::hypot(dx, dy);
    const double ang = qRadiansToDegrees(std::atan2(dy, dx));
    // 焦点在输入框时暂停预填，避免覆盖用户正在键入的值
    if (!dynLen_->hasFocus() && !dynAng_->hasFocus()) {
        dynLen_->setText(QString::number(len, 'f', 2));
        dynAng_->setText(QString::number(ang, 'f', 1));
    }
    dynBox_->move(vp + QPoint(18, 18));
    dynBox_->adjustSize();
    if (!dynBox_->isVisible()) dynBox_->show();
    dynBox_->raise();
}

void GraphicsView::hideDynBox() {
    if (!dynBox_) return;
    dynBox_->hide();
    dynLen_->clearFocus();
    dynAng_->clearFocus();
}

// Enter：以「上一点 + 极坐标(距离, 角度°)」计算世界点，注入为拾取点。
// 复用 injectPoint 点击管线（追加采集点/更新预览/点数满足自动收尾）。
void GraphicsView::commitDynInput() {
    if (pendingPts_.empty()) return;
    bool okL = false, okA = false;
    const double len = dynLen_->text().toDouble(&okL);
    double ang = dynAng_->text().toDouble(&okA);
    if (!okL || len < 0) return;
    if (!okA) ang = 0.0;
    const Vec3& last = pendingPts_.back();
    const double rad = qDegreesToRadians(ang);
    const Vec3 pt(last.x + len * std::cos(rad), last.y + len * std::sin(rad), last.z);
    hideDynBox();
    model_->log(QString(">> 距离=%1 角度=%2  [DYN 输入]")
                    .arg(len, 0, 'f', 2).arg(ang, 0, 'f', 1), "cmd");
    injectPoint(pt);
}

// 子模式收尾模板（DRY）：各 finalize*Ex 的稳定尾部。
int GraphicsView::commitElement(Element el) {
    const int id = model_->addElement(el);   // 触发 rebuild()，预览项在那里被销毁
    model_->setSelected(id);
    resetPending();
    return id;
}

void GraphicsView::finalizeLine() {
    if (pendingPts_.size() < 2) return;
    Element el = ElementFactory::create(ElementType::Line, QStringLiteral("直线"), model_);
    el.pts = {pendingPts_[0], pendingPts_[1]};
    const Vec3 tail = pendingPts_[1];
    const double len = pendingPts_[0].distTo(tail);
    const int id = model_->addElement(el);      // 触发 rebuild()，预览项在那里被销毁
    model_->setSelected(id);
    model_->log(QString("已创建直线 #%1  长度 %2 mm").arg(id).arg(len, 0, 'f', 2));

    clearPreview();
    pendingPts_.clear();
    if (model_->chainLine()) {
        // 连续绘制链：上一段的终点直接成为下一段的起点，形成折线序列
        pendingPts_.push_back(tail);
        drawing_ = true;
        if (!chainHinted_) {
            chainHinted_ = true;
            model_->log(QStringLiteral("连续直线模式：继续点击可接着画，右键或 Esc 结束"));
        }
    } else {
        drawing_ = false;
    }
}

// ---------------------------- 直线子模式（菜单扩展） ----------------------------

bool GraphicsView::isLineSubTool(const QString& t) const {
    return ToolRegistry::instance().isLineSubTool(t);
}

int GraphicsView::pickRefElement(const QPoint& vp, const std::vector<ElementType>& allowed) const {
    const int id = hitTest(vp);
    if (id < 0) return -1;
    const Element* e = model_->element(id);
    if (!e) return -1;
    for (ElementType t : allowed) if (e->type == t) return id;
    return -1;
}

void GraphicsView::addLineSegment(const Vec3& a, const Vec3& b) {
    if (a.distTo(b) < 1e-9) {
        model_->log(QStringLiteral("线段长度为零，已忽略"), "warn");
        return;
    }
    Element el = ElementFactory::create(ElementType::Line, QStringLiteral("直线"), model_);
    el.pts = {a, b};
    const int id = model_->addElement(el);
    model_->setSelected(id);
    model_->log(QString("已创建直线 #%1  (%2, %3) → (%4, %5)")
                    .arg(id)
                    .arg(a.x, 0, 'f', 2).arg(a.y, 0, 'f', 2)
                    .arg(b.x, 0, 'f', 2).arg(b.y, 0, 'f', 2));
}

void GraphicsView::finalizeSnake() {
    if (pendingPts_.size() < 2) {
        model_->log(QStringLiteral("Snake 折线至少需要两个点"), "warn");
    } else {
        model_->log(QString("已结束 Snake 折线，共 %1 段").arg(pendingPts_.size() - 1));
    }
    resetPending();
}

void GraphicsView::handleLineSubTool(const Vec3& w, QMouseEvent* e) {
    const line::Mode m = line::modeFromString(model_->activeTool());
    const QPoint vp = e->position().toPoint();
    const auto& lp = model_->lineParams();

    // Snake 模式：逐段即时生成，右键/Enter 结束
    if (m == line::Mode::Snake || m == line::Mode::SnakeX || m == line::Mode::SnakeY) {
        if (pendingPts_.empty()) {
            pendingPts_.push_back(snapWorld(w));
            drawing_ = true;
        } else {
            line::Result r = line::compute(m, pendingPts_, pickedIds_, snapWorld(w),
                                           model_->elements(), lineAngleParam_);
            if (r.valid) {
                addLineSegment(r.a, r.b);
                pendingPts_.push_back(r.b);
            } else {
                model_->log(r.msg, "warn");
            }
        }
        updatePreview(w);
        return;
    }

    // 角度直线：选起点后按参数(angle,length)直接绘制（参数输入条，要求 3）
    if (m == line::Mode::Angle) {
        if (lp.length < 1e-9) {
            model_->log(QStringLiteral("角度直线：长度无效，请在参数输入条输入长度"), "warn");
            return;
        }
        const Vec3 a = snapWorld(w);
        const Vec3 b = line::pointAtAngleLength(a, lp.angleDeg, lp.length);
        addLineSegment(a, b);
        resetPending();
        return;
    }

    // 平行线：选参考线 → 点击方向决定偏移方向，沿垂直方向按间隔生成指定条数（要求 4）
    if (m == line::Mode::Parallel) {
        if (pickedIds_.empty()) {
            const int id = pickRefElement(vp, {ElementType::Line, ElementType::Rectangle});
            if (id < 0) { model_->log(QStringLiteral("请拾取合适的参考直线/矩形边"), "warn"); return; }
            pickedIds_.push_back(id);
            model_->log(QString("已选择参考线 #%1").arg(id));
            drawing_ = true;
            updatePreview(w);
            return;
        }
        const Element* ref = model_->element(pickedIds_[0]);
        if (!ref) { model_->log(QStringLiteral("参考图元已不存在"), "warn"); resetPending(); return; }
        line::ParallelCopies pc = line::parallelCopies(*ref, snapWorld(w), lp.spacing, qMax(1, lp.count));
        if (!pc.valid) { model_->log(pc.msg, "warn"); resetPending(); return; }
        for (const auto& seg : pc.lines) addLineSegment(seg.first, seg.second);
        model_->log(QString("已创建 %1 条平行线（间隔 %2 mm，参考线 #%3）")
                        .arg((int)pc.lines.size()).arg(lp.spacing, 0, 'f', 2).arg(pickedIds_[0]));
        resetPending();
        return;
    }

    // 其余模式：先按需收集参考图元，再收集定位点，最后生成
    const line::ReqCount rcLine = line::requirements(m);
    const int reqRefs = rcLine.refs;
    const std::vector<ElementType> allowed = line::allowedTypes(m);

    if ((int)pickedIds_.size() < reqRefs) {
        if (!collectRef(vp, allowed)) return;
        updatePreview(w);
        return;
    }

    // 收集定位点（参考线在 pickedIds_，定位点在 pendingPts_）
    pendingPts_.push_back(snapWorld(w));
    drawing_ = true;
    // 分段式命令反馈：记录已拾取的定位点
    model_->log(QString("已拾取定位点 %1: (%2, %3)").arg(pendingPts_.size())
                    .arg(pendingPts_.back().x, 0, 'f', 2)
                    .arg(pendingPts_.back().y, 0, 'f', 2));

    // 判断是否足以生成：两点定线需要 2 个定位点；其余有参考线、1 个定位点即可
    bool enough = false;
    switch (m) {
    case line::Mode::TwoPoint:
    case line::Mode::Horizontal:
    case line::Mode::Vertical:
    case line::Mode::AngleFromLine:
    case line::Mode::RelativeAngle:
        enough = (pendingPts_.size() >= 2); break;
    case line::Mode::ParallelThroughPoint:
    case line::Mode::Orthogonal:
    case line::Mode::OrthogonalFromLine:
    case line::Mode::FromPointToLine:
    case line::Mode::TangentPC:
    case line::Mode::Bisector:
    case line::Mode::TangentCC:
    case line::Mode::TangentOrthogonal:
        enough = (pendingPts_.size() >= 1); break;
    default:
        break;
    }

    if (!enough) {
        updatePreview(w);
        return;
    }

    line::Result r = line::compute(m, pendingPts_, pickedIds_, snapWorld(w),
                                   model_->elements(), lineAngleParam_);
    if (r.valid) {
        addLineSegment(r.a, r.b);
        resetPending();
    } else {
        model_->log(r.msg, "warn");
        pendingPts_.pop_back();   // 允许重新拾取定位点
    }
}

// ---------------------------- 构造线（Xline）子模式 ----------------------------

bool GraphicsView::isXlineSubTool(const QString& t) const {
    return ToolRegistry::instance().isXlineSubTool(t);
}

// 提交一条构造线并保持连续绘制：与 commitElement 的差别在于**不** resetPending ——
// 构造线常成组布置（中心线/辅助线），命令保持活跃直到用户右键/Esc 主动退出。
void GraphicsView::commitXlineAndContinue(const Vec3& through, double angleDeg, const QString& desc) {
    Element el = ElementFactory::createXline(through, angleDeg, model_);
    const int id = model_->addElement(el);      // 触发 rebuild()，预览项在那里被销毁
    model_->setSelected(id);
    model_->log(QString("已创建构造线 #%1  %2 通过(%3, %4) 角度 %5°")
                    .arg(id).arg(desc)
                    .arg(through.x, 0, 'f', 2).arg(through.y, 0, 'f', 2)
                    .arg(angleDeg, 0, 'f', 1));
    // 连续绘制：清空本轮采集但保持命令活跃；右键/Esc 走 handleRightButton 兜底退出
    pendingPts_.clear();
    pickedIds_.clear();
    drawing_ = true;
    clearPreview();
    updateCommandPrompt();
}

// 构造线动态预览：与 line:: 共用几何计算，结果线段 (r.a,r.b) 经
// 「通过点 r.a + 方向角 (r.b-r.a)」转为无限直线预览。
void GraphicsView::previewXline(const Vec3& cur) {
    const QString t = model_->activeTool();
    const line::Mode m = line::modeFromString(t);
    const auto& lp = model_->lineParams();

    // 角度构造线：未拾取基点时，以光标为基点按参数角度预览（参数输入条提供角度）
    if (m == line::Mode::Angle) {
        showPreview(ElementFactory::createXline(cur, lp.angleDeg, model_));
        return;
    }

    // 平行构造线：已选参考线、未点通过点时，按光标位置预览（方向取参考线方向）
    if (m == line::Mode::Parallel && pickedIds_.size() == 1 && pendingPts_.empty()) {
        line::Result r = line::compute(m, {}, pickedIds_, cur, model_->elements(), 0.0);
        if (r.valid) showPreview(ElementFactory::createXline(r.a, (r.b - r.a).angleDeg(), model_));
        else clearPreview();
        return;
    }

    if (pendingPts_.empty()) { clearPreview(); return; }
    line::Result r = line::compute(m, pendingPts_, pickedIds_, cur,
                                   model_->elements(), lineAngleParam_);
    if (r.valid) showPreview(ElementFactory::createXline(r.a, (r.b - r.a).angleDeg(), model_));
    else clearPreview();
}

void GraphicsView::handleXlineSubTool(const Vec3& w, QMouseEvent* e) {
    const line::Mode m = line::modeFromString(model_->activeTool());
    const QPoint vp = e->position().toPoint();
    const auto& lp = model_->lineParams();

    // 角度构造线（A）：点击基点即按参数角度生成（长度无意义，方向无限延伸）
    if (m == line::Mode::Angle) {
        commitXlineAndContinue(snapWorld(w), lp.angleDeg, QStringLiteral("角度"));
        return;
    }

    // 平行构造线（O）：先选参考直线，再点通过点；方向 = 参考线方向
    if (m == line::Mode::Parallel) {
        if (pickedIds_.empty()) {
            if (!collectRef(vp, {ElementType::Line, ElementType::Rectangle})) return;
            updatePreview(w);
            return;
        }
        line::Result r = line::compute(m, {}, pickedIds_, snapWorld(w),
                                       model_->elements(), 0.0);
        if (!r.valid) { model_->log(r.msg, "warn"); resetPending(); return; }
        commitXlineAndContinue(r.a, (r.b - r.a).angleDeg(), QStringLiteral("平行"));
        return;
    }

    // 通例（水平/竖直/两点/角平分线）：按需收集参考图元（角平分线需 2 条），再收集定位点
    const line::ReqCount rc = line::requirements(m);
    if ((int)pickedIds_.size() < rc.refs) {
        if (!collectRef(vp, line::allowedTypes(m))) return;
        updatePreview(w);
        return;
    }

    pendingPts_.push_back(snapWorld(w));
    drawing_ = true;
    model_->log(QString("已拾取点 %1: (%2, %3)").arg(pendingPts_.size())
                    .arg(pendingPts_.back().x, 0, 'f', 2)
                    .arg(pendingPts_.back().y, 0, 'f', 2));

    // 判断是否足以生成：两点定线需 2 个定位点；角平分线有 2 条参考线 + 1 个点即可
    const int needPts = std::max(rc.pts, 1);
    if ((int)pendingPts_.size() < needPts) {
        updatePreview(w);
        return;
    }

    line::Result r = line::compute(m, pendingPts_, pickedIds_, snapWorld(w),
                                   model_->elements(), lineAngleParam_);
    if (!r.valid) {
        model_->log(r.msg, "warn");
        pendingPts_.pop_back();   // 允许重新拾取定位点
        return;
    }
    const QString desc = (m == line::Mode::Horizontal) ? QStringLiteral("水平")
                       : (m == line::Mode::Vertical)   ? QStringLiteral("竖直")
                       : (m == line::Mode::Bisector)   ? QStringLiteral("角平分")
                                                       : QStringLiteral("两点");
    commitXlineAndContinue(r.a, (r.b - r.a).angleDeg(), desc);
}

void GraphicsView::finalizeCircle() {
    if (pendingPts_.size() < 2) return;
    const double r = pendingPts_[0].distTo(pendingPts_[1]);
    if (r < 1e-6) { model_->log(QStringLiteral("圆：半径为 0，已忽略"), "warn"); resetPending(); return; }
    Element el = ElementFactory::create(ElementType::Circle, QStringLiteral("圆"), model_);
    el.pts = {pendingPts_[0]};
    el.radius = r;
    const int id = commitElement(el);
    model_->log(QString("已创建圆 #%1  r=%2 mm（圆心+半径）").arg(id).arg(r, 0, 'f', 2));
}

void GraphicsView::finalizeCircle3P() {
    if (pendingPts_.size() < 3) return;
    Vec3 c; double r = 0;
    if (!geo::circleFrom3Points(pendingPts_[0], pendingPts_[1], pendingPts_[2], c, r)) {
        model_->log(QStringLiteral("三点圆：三点共线，无法确定唯一圆，请重新拾取"), "warn");
        resetPending();
        return;
    }
    Element el = ElementFactory::create(ElementType::Circle, QStringLiteral("圆"), model_);
    el.pts = {c};
    el.radius = r;
    const int id = commitElement(el);
    model_->log(QString("已创建圆 #%1  圆心(%2, %3) r=%4 mm（三点定圆）")
                    .arg(id).arg(c.x, 0, 'f', 2).arg(c.y, 0, 'f', 2).arg(r, 0, 'f', 2));
}

// ============================ 圆形子模式（12种绘制方式） ============================

bool GraphicsView::isCircleSubTool(const QString& t) const {
    return ToolRegistry::instance().isCircleSubTool(t);
}

// ---- 子模式采集机械（DRY）：圆/弧/多边形/样条/椭圆及直线通例共用 ----

bool GraphicsView::collectRef(const QPoint& vp, const std::vector<ElementType>& allowed) {
    const int id = pickRefElement(vp, allowed);
    if (id < 0) { model_->log(QStringLiteral("请拾取合适的参考图元"), "warn"); return false; }
    pickedIds_.push_back(id);
    model_->log(QString("已选择参考图元 #%1").arg(id));
    drawing_ = true;
    return true;
}

void GraphicsView::collectSketchPoint(const Vec3& w, int need) {
    const Vec3 p = snapWorld(w);
    pendingPts_.push_back(p);
    drawing_ = true;
    // 分段式命令反馈：记录已拾取的第 need 个点
    model_->log(QString("已拾取点 %1/%2: (%3, %4)")
                    .arg((int)pendingPts_.size()).arg(need)
                    .arg(p.x, 0, 'f', 2).arg(p.y, 0, 'f', 2));
}

void GraphicsView::handleCircleSubTool(const Vec3& w, QMouseEvent* e) {
    const circle::Mode m = circle::modeFromString(model_->activeTool());
    const circle::ReqCount rc = circle::requirements(m);
    const QPoint vp = e->position().toPoint();
    const auto& lp = model_->lineParams();
    const double param = rc.usesParam ? lp.length : 0.0;

    // ---- Step 1: 收集参考图元 ----
    if ((int)pickedIds_.size() < rc.refs) {
        const auto allowed = circle::allowedTypes(m);
        if (!collectRef(vp, allowed)) return;

        // ByArc & Inscribed：拾取完参考图元即可自动完成（无需点输入）
        if ((int)pickedIds_.size() == rc.refs && rc.pts == 0 &&
            (m == circle::Mode::ByArc || m == circle::Mode::Inscribed)) {
            circle::Result r = circle::compute(m, pendingPts_, pickedIds_, w,
                                               model_->elements(), param);
            if (r.valid) finalizeCircleEx(r, model_->activeTool());
            else model_->log(r.msg, "warn");
            return;
        }

        // Concentric 有参数时自动完成
        if (m == circle::Mode::Concentric && param > 1e-6) {
            circle::Result r = circle::compute(m, pendingPts_, pickedIds_, w,
                                               model_->elements(), param);
            if (r.valid) finalizeCircleEx(r, model_->activeTool());
            else model_->log(r.msg, "warn");
            return;
        }

        updatePreview(w);
        return;
    }

    // ---- Step 2: 收集几何点 ----
    if ((int)pendingPts_.size() < rc.pts) {
        collectSketchPoint(w, rc.pts);

        // 单解工具：收够点即可自动完成（最后一个点同时充当光标选解）
        if ((int)pendingPts_.size() == rc.pts) {
            bool autoFinalize = false;
            switch (m) {
            case circle::Mode::TwoPoint:       // 两点画圆，唯一解
            case circle::Mode::TwoPointRadius: // 两点+半径，光标选近解
            case circle::Mode::CenterRadius:   // 圆心+参数半径，唯一解
                autoFinalize = true;
                break;
            default: break;
            }
            if (autoFinalize) {
                circle::Result r = circle::compute(m, pendingPts_, pickedIds_, w,
                                                   model_->elements(), param);
                if (r.valid) finalizeCircleEx(r, model_->activeTool());
                else model_->log(r.msg, "warn");
                return;
            }
        }
        updatePreview(w);
        return;
    }

    // ---- Step 3: 多解工具的选解点击 ----
    circle::Result r = circle::compute(m, pendingPts_, pickedIds_, w,
                                       model_->elements(), param);
    if (r.valid) finalizeCircleEx(r, model_->activeTool());
    else model_->log(r.msg.isEmpty() ? QStringLiteral("无法创建圆") : r.msg, "warn");
}

void GraphicsView::finalizeCircleEx(const circle::Result& r, const QString& method) {
    if (!r.valid || r.radius < 1e-6) {
        model_->log(QStringLiteral("圆：半径为 0 或无效，已忽略"), "warn");
        resetPending();
        return;
    }
    Element el = ElementFactory::create(ElementType::Circle, QStringLiteral("圆"), model_);
    el.pts = {r.center};
    el.radius = r.radius;
    const int id = commitElement(el);
    model_->log(QString("已创建圆 #%1  圆心(%2, %3) r=%4 mm（%5）")
                    .arg(id).arg(r.center.x, 0, 'f', 2).arg(r.center.y, 0, 'f', 2)
                    .arg(r.radius, 0, 'f', 2).arg(method));
}

void GraphicsView::finalizeArc() {
    if (pendingPts_.size() < 3) return;
    const Vec3 c = pendingPts_[0];
    const double r = c.distTo(pendingPts_[1]);
    if (r < 1e-6) { model_->log(QStringLiteral("圆弧：半径为 0，已忽略"), "warn"); resetPending(); return; }
    const double s = (pendingPts_[1] - c).angleDeg();
    trackArcAngle((pendingPts_[2] - c).angleDeg());   // 把最后一次点击也计入扫掠
    double sweep = arcSweep_;
    if (std::fabs(sweep) < 1e-6) sweep = geo::wrapDeg180((pendingPts_[2] - c).angleDeg() - s);
    if (std::fabs(sweep) < 1e-6) sweep = 360.0;

    Element el = ElementFactory::create(ElementType::Arc, QStringLiteral("圆弧"), model_);
    el.pts = {c};
    el.radius = r;
    el.startAngle = s;
    el.endAngle = s + sweep;
    const int id = commitElement(el);
    model_->log(QString("已创建圆弧 #%1  r=%2 mm  起始 %3°  扫掠 %4°")
                    .arg(id).arg(r, 0, 'f', 2).arg(s, 0, 'f', 1).arg(sweep, 0, 'f', 1));
}

// ============================ 圆弧子模式（11种绘制方式） ============================

bool GraphicsView::isArcSubTool(const QString& t) const {
    return ToolRegistry::instance().isArcSubTool(t);
}

bool GraphicsView::isPolySubTool(const QString& t) const {
    return ToolRegistry::instance().isPolySubTool(t);
}

bool GraphicsView::isSplineSubTool(const QString& t) const {
    return ToolRegistry::instance().isSplineSubTool(t);
}

void GraphicsView::handleArcSubTool(const Vec3& w, QMouseEvent* e) {
    const arc::Mode m      = arc::modeFromString(model_->activeTool());
    const arc::ReqCount rc = arc::requirements(m);
    const QPoint vp        = e->position().toPoint();
    const auto& lp         = model_->lineParams();
    const bool  useAngle   = (ToolRegistry::instance().paramMode(model_->activeTool())
                              == ParamBarMode::AngleLength);
    const double param     = rc.usesParam ? (useAngle ? lp.angleDeg : lp.length) : 0.0;

    // ---- Step 1: 收集参考图元 ----
    if ((int)pickedIds_.size() < rc.refs) {
        const auto allowed = arc::allowedTypes(m);
        if (!collectRef(vp, allowed)) return;

        // 无需点输入且参数已给足的模式：拾取完即自动完成
        if ((int)pickedIds_.size() == rc.refs && rc.pts == 0 && param > 1e-6) {
            arc::Result r = arc::compute(m, pendingPts_, pickedIds_, w, model_->elements(), param);
            if (r.valid) finalizeArcEx(r, model_->activeTool());
            else model_->log(r.msg, "warn");
            return;
        }
        updatePreview(w);
        return;
    }

    // ---- Step 2: 收集几何点 ----
    if ((int)pendingPts_.size() < rc.pts) {
        collectSketchPoint(w, rc.pts);

        if ((int)pendingPts_.size() == rc.pts) {
            bool autoFinalize = false;
            switch (m) {
            case arc::Mode::CenterPointAngles:
            case arc::Mode::ThreePoint:
            case arc::Mode::TwoPointBulge:
            case arc::Mode::TwoPointRadius:
            case arc::Mode::TwoPointAngle:
            case arc::Mode::TwoPointLength:
            case arc::Mode::EllipseArcAxis:
            case arc::Mode::EllipseArc1Pt:
                autoFinalize = true; break;
            default: break;
            }
            if (autoFinalize) {
                arc::Result r = arc::compute(m, pendingPts_, pickedIds_, w, model_->elements(), param);
                if (r.valid) finalizeArcEx(r, model_->activeTool());
                else model_->log(r.msg, "warn");
                return;
            }
        }
        updatePreview(w);
        return;
    }

    // ---- Step 3: 多解工具的选解点击 ----
    arc::Result r = arc::compute(m, pendingPts_, pickedIds_, w, model_->elements(), param);
    if (r.valid) finalizeArcEx(r, model_->activeTool());
    else model_->log(r.msg.isEmpty() ? QStringLiteral("无法创建圆弧") : r.msg, "warn");
}

void GraphicsView::finalizeArcEx(const arc::Result& r, const QString& method) {
    if (!r.valid || r.radius < 1e-6) {
        model_->log(QStringLiteral("圆弧：半径为 0 或无效，已忽略"), "warn");
        resetPending();
        return;
    }
    Element el = ElementFactory::create(ElementType::Arc, QStringLiteral("圆弧"), model_);
    el.pts = {r.center};
    el.radius     = r.radius;
    el.startAngle = r.startAngle;
    el.endAngle   = r.endAngle;
    const double sweep = r.endAngle - r.startAngle;
    const int id = commitElement(el);
    model_->log(QString("已创建圆弧 #%1  圆心(%2, %3) r=%4 mm  起始 %5°  扫掠 %6°（%7）")
                    .arg(id)
                    .arg(r.center.x, 0, 'f', 2).arg(r.center.y, 0, 'f', 2)
                    .arg(r.radius, 0, 'f', 2)
                    .arg(r.startAngle, 0, 'f', 1).arg(sweep, 0, 'f', 1)
                    .arg(method));
    if (!r.msg.isEmpty() && (r.msg.contains(QStringLiteral("椭圆弧")) || r.msg.contains(QStringLiteral("等效")))) {
        model_->log(r.msg, "info");
    }
}

// ============================ 多边形子模式 ============================

void GraphicsView::handlePolySubTool(const Vec3& w, QMouseEvent* e) {
    (void)e;  // 多边形子模式不需要参考图元拾取
    const polygon::Mode m = polygon::modeFromString(model_->activeTool());
    const polygon::ReqCount rc = polygon::requirements(m);
    const auto& lp = model_->lineParams();
    const int sides = std::max(3, lp.count);
    const double param = lp.length;  // 倒角距离 / 圆角半径

    // ---- Step 1: 收集几何点 ----
    if ((int)pendingPts_.size() < rc.pts) {
        collectSketchPoint(w, rc.pts);
        updatePreview(w);
        return;
    }

    // ---- Step 2: 点击确认（光标位置参与计算）----
    polygon::Result r = polygon::compute(m, pendingPts_, pickedIds_, w,
                                         model_->elements(), param, sides);
    if (r.valid) finalizePolyEx(r, model_->activeTool());
    else model_->log(r.msg.isEmpty() ? QStringLiteral("无法创建多边形") : r.msg, "warn");
}

void GraphicsView::finalizePolyEx(const polygon::Result& r, const QString& method) {
    if (!r.valid || r.pts.size() < 2) {
        model_->log(QStringLiteral("多边形：顶点数不足，已忽略"), "warn");
        resetPending();
        return;
    }
    const QString name = (r.type == ElementType::Rectangle) ? QStringLiteral("矩形")
                                                             : QStringLiteral("多边形");
    Element el = ElementFactory::create(r.type, name, model_);
    el.pts = r.pts;
    el.closed = true;
    const int id = commitElement(el);
    model_->log(QString("已创建%1 #%2（%3 个顶点）（%4）")
                    .arg(name).arg(id).arg((int)el.pts.size()).arg(method));
}

// ============================ 样条子模式 ============================

void GraphicsView::handleSplineSubTool(const Vec3& w, QMouseEvent* e) {
    (void)e;  // 样条子模式不需要参考图元拾取
    const spline::Mode m = spline::modeFromString(model_->activeTool());
    const spline::ReqCount rc = spline::requirements(m);

    // ---- Step 1: 收集几何点 ----
    if ((int)pendingPts_.size() < rc.pts) {
        collectSketchPoint(w, rc.pts);
        updatePreview(w);
        return;
    }

    // ---- Step 2: 点击确认（光标位置参与计算）----
    spline::Result r = spline::compute(m, pendingPts_, w);
    if (r.valid) finalizeSplineEx(r, model_->activeTool());
    else model_->log(r.msg.isEmpty() ? QStringLiteral("无法创建曲线") : r.msg, "warn");
}

void GraphicsView::finalizeSplineEx(const spline::Result& r, const QString& method) {
    if (!r.valid || r.pts.size() < 2) {
        model_->log(QStringLiteral("曲线：定义点不足，已忽略"), "warn");
        resetPending();
        return;
    }
    const QString name = spline::stringFromMode(spline::modeFromString(method));
    Element el = ElementFactory::create(ElementType::Spline, name, model_);
    el.pts = r.pts;
    el.splineSubType = r.subType;
    el.closed = false;  // 样条曲线一律开放
    const int id = commitElement(el);
    model_->log(QString("已创建%1 #%2（%3 个定义点）（%4）")
                    .arg(name).arg(id).arg((int)el.pts.size()).arg(method));
}

// ============================ 椭圆子模式（6种绘制方式） ============================

bool GraphicsView::isEllipseSubTool(const QString& t) const {
    return ToolRegistry::instance().isEllipseSubTool(t);
}

void GraphicsView::handleEllipseSubTool(const Vec3& w, QMouseEvent* e) {
    const ellipse::Mode m  = ellipse::modeFromString(model_->activeTool());
    const ellipse::ReqCount rc = ellipse::requirements(m);
    const QPoint vp            = e->position().toPoint();
    const auto& lp             = model_->lineParams();
    const double param         = rc.usesParam ? lp.length : 0.0;

    // ---- Step 1: 收集参考图元（目前仅「内切」需要） ----
    if ((int)pickedIds_.size() < rc.refs) {
        const auto allowed = ellipse::allowedTypes(m);
        if (!collectRef(vp, allowed)) return;

        // 内切椭圆：拾取完外框即可完成，不再需要点输入
        if ((int)pickedIds_.size() == rc.refs && rc.pts == 0) {
            ellipse::Result r = ellipse::compute(m, pendingPts_, pickedIds_, w,
                                                 model_->elements(), param);
            if (r.valid) finalizeEllipseEx(r, model_->activeTool());
            else model_->log(r.msg, "warn");
            return;
        }
        updatePreview(w);
        return;
    }

    // ---- Step 2: 收集几何点 ----
    if ((int)pendingPts_.size() < rc.pts) {
        collectSketchPoint(w, rc.pts);

        // 注意：1点/轴/焦点三种模式是「光标驱动」的——requirements 的 pts 只包含已拾取的点，
        // 最后一步的长轴/短轴/终判还要靠光标位置定。若在此处收够 pts 就自动定形，
        // cur 恰为最后一次点击本身（Axis→v≈0、Foci→twoA≈焦距、1点→a=0），必然报错。
        // 因此这三种模式一律落到 Step 3，由用户再移一次光标点击确认。
        //
        // 拟合类（4点 / 中心+3点）点数收满即唯一确定，不必再点，故在此直接定形。
        if ((int)pendingPts_.size() == rc.pts &&
            (m == ellipse::Mode::FourPoint || m == ellipse::Mode::Center3Points)) {
            // 拟合不需要光标，用最后拾取的点占位即可
            ellipse::Result r = ellipse::compute(m, pendingPts_, pickedIds_,
                                                 pendingPts_.back(), model_->elements(), param);
            if (r.valid) finalizeEllipseEx(r, model_->activeTool());
            else model_->log(r.msg, "warn");
            return;
        }
        updatePreview(w);
        return;
    }

    // ---- Step 3: 多解工具的选解点击 ----
    ellipse::Result r = ellipse::compute(m, pendingPts_, pickedIds_, w,
                                         model_->elements(), param);
    if (r.valid) finalizeEllipseEx(r, model_->activeTool());
    else model_->log(r.msg.isEmpty() ? QStringLiteral("无法创建椭圆") : r.msg, "warn");
}

void GraphicsView::finalizeEllipseEx(const ellipse::Result& r, const QString& method) {
    if (!r.valid || r.radius < 1e-6 || r.radiusY < 1e-6) {
        model_->log(QStringLiteral("椭圆：半轴为 0 或无效，已忽略"), "warn");
        resetPending();
        return;
    }
    Element el = ElementFactory::createEllipse(r.center, r.radius, r.radiusY,
                                               r.rotation, model_);
    const int id = commitElement(el);
    model_->log(QString("已创建椭圆 #%1  中心(%2, %3)  %4（%5）")
                    .arg(id).arg(r.center.x, 0, 'f', 2).arg(r.center.y, 0, 'f', 2)
                    .arg(el.sizeDesc()).arg(method));
}

void GraphicsView::finalizeRect() {
    if (pendingPts_.size() < 2) return;
    Element el = ElementFactory::create(ElementType::Rectangle, QStringLiteral("矩形"), model_);
    el.pts = {pendingPts_[0], pendingPts_[1]};
    const int id = commitElement(el);
    model_->log("已创建矩形 #" + QString::number(id) + "  " + el.sizeDesc());
}

void GraphicsView::finalizePoly(bool spline) {
    if (pendingPts_.size() < 2) { resetPending(); return; }
    Element el = ElementFactory::create(spline ? ElementType::Spline : ElementType::Polygon,
                            spline ? QStringLiteral("样条曲线") : QStringLiteral("多边形"), model_);
    el.pts = pendingPts_;
    const int id = commitElement(el);
    model_->log(QString("已创建%1 #%2（%3 个控制点）")
                    .arg(el.name).arg(id).arg((int)el.pts.size()));
}

// ---------------------------- 事件 ----------------------------

// 最底层事件钩子：QGraphicsView 的鼠标/右键/滚轮事件实际先经过 viewportEvent()（而非 view 的 event()），
// 故这里重写 viewportEvent 才能抓到完整的原始派发。若这里都收不到右键按下，说明事件在更上层（父控件/OS）被吞；
// 若收到但 handler 里没平移，则说明逻辑分支问题。
bool GraphicsView::viewportEvent(QEvent* e) {
#ifdef PAN_DEBUG
    switch (e->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove: {
        QMouseEvent* me = static_cast<QMouseEvent*>(e);
        panDbg(QStringLiteral("[raw] %1 button=%2 buttons=%3 pos=(%4,%5)")
                   .arg(e->type() == QEvent::MouseButtonPress ? "Press"
                        : e->type() == QEvent::MouseButtonRelease ? "Release" : "Move")
                   .arg((int)me->button()).arg((int)me->buttons())
                   .arg(me->position().x(), 0, 'f', 1).arg(me->position().y(), 0, 'f', 1));
        break;
    }
    case QEvent::ContextMenu:
        panDbg(QStringLiteral("[raw] ContextMenu 事件触发（右键菜单手势）"));
        break;
    case QEvent::Wheel:
        panDbg(QStringLiteral("[raw] Wheel"));
        break;
    default:
        break;
    }
#endif
    return QGraphicsView::viewportEvent(e);
}

void GraphicsView::setPointPickMode(bool on) {
    pointPickMode_ = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

// ---- 区域拾取模式（【修改>剖面填充】：点选封闭区域）----
void GraphicsView::setRegionPickMode(bool on) {
    regionPickMode_ = on;
    if (!on) regionHighlight_.clear();      // 退出即清除预览高亮
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void GraphicsView::setRegionHighlight(const std::vector<std::vector<Vec3>>& polys) {
    regionHighlight_ = polys;
    viewport()->update();
}

// ---- 窗口缩放框选模式（【视图>窗口缩放】）：进入后左键拖拽框选目标区域，
//      释放后该区域精确缩放并充满整个绘图工作区域；右键/Esc 取消并自动退出模式。 ----
void GraphicsView::setZoomBoxMode(bool on) {
    zoomBoxMode_ = on;
    if (!on) { zoomBoxActive_ = false; zoomBoxRect_ = QRectF(); }   // 退出即清除框选状态
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    viewport()->update();
}

void GraphicsView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && spaceDown_)) {
        panning_ = true;
        lastPan_ = e->position().toPoint();
        e->accept();
        return;
    }
    if (e->button() == Qt::RightButton) {
        // 窗口缩放框选模式下：右键直接取消并退出模式（不触发平移）
        if (zoomBoxMode_) {
            setZoomBoxMode(false);
            e->accept();
            return;
        }
        // Shift+右键：弹出「临时捕捉覆盖」菜单（仅绘制命令期间有意义），选中的类型仅对下一次
        // 放置的点生效，放置后立即恢复原有配置；不触发平移、不结束命令。
        if ((e->modifiers() & Qt::ShiftModifier) && drawToolActive_) {
            showSnapOverrideMenu(mapToGlobal(e->position().toPoint()));
            e->accept();
            return;
        }
        // 右键按下：先进入平移预备态，移动则平移视图，未移动（单击）则保留原「结束/取消」语义
        panning_ = true;
        panRight_ = true;
        rightMoved_ = false;
        lastPan_ = e->position().toPoint();
        panDbg(QStringLiteral("[press] 右键 -> panning_=true panRight_=true lastPan=(%1,%2) tool=%3")
                   .arg(lastPan_.x()).arg(lastPan_.y()).arg(model_->activeTool()));
        e->accept();
        return;
    }
    if (e->button() != Qt::LeftButton)  { QGraphicsView::mousePressEvent(e); return; }
    // 夹点热态：左键按下被热态状态机消费（确认单击 / 拖拽起点），不派发给工具
    if (gripHot_) { e->accept(); return; }
    // 中心点拾取模式（环形阵列等）：左键直接取点回抛，不进入绘制/选择逻辑。
    if (pointPickMode_) {
        const Vec3 w = toVec3(mapToScene(e->position().toPoint()));
        emit canvasPointPicked(w.x, w.y);
        e->accept();
        return;
    }
    // 区域拾取模式（剖面填充）：左键点选 → 交上层识别"该点所在的封闭区域"，
    // 不进入绘制/选择逻辑（否则会误改选择集或启动绘图命令）。
    if (regionPickMode_) {
        const Vec3 w = toVec3(mapToScene(e->position().toPoint()));
        emit regionPickRequested(w.x, w.y);
        e->accept();
        return;
    }
    // 窗口缩放框选模式：左键按下即开始拖拽框选矩形（不进入绘制/选择逻辑）
    if (zoomBoxMode_) {
        const Vec3 w = toVec3(mapToScene(e->position().toPoint()));
        zoomBoxActive_ = true;
        zoomBoxStart_ = QPointF(w.x, w.y);
        zoomBoxRect_  = QRectF(zoomBoxStart_, zoomBoxStart_);
        setCursor(Qt::CrossCursor);
        viewport()->update();
        e->accept();
        return;
    }
    handlePress(toVec3(mapToScene(e->position().toPoint())), e);
    e->accept();
}

void GraphicsView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint vp = e->position().toPoint();
    const Vec3 w = toVec3(mapToScene(vp));
    lastMouseWorld_ = w;            // 记录最近鼠标世界坐标，供捕捉快捷键切换后即时刷新
    emit cursorMoved(w.x, w.y, 0.0);
    // DYN 动态输入：绘制命令期已有采集点时，光标跟随显示「距离/角度」输入框
    if (drawToolActive_ && model_->dynMode() && !pendingPts_.empty())
        updateDynBox(vp);
    else if (dynBox_ && dynBox_->isVisible())
        dynBox_->hide();
    // 区域拾取模式（剖面填充）：平移优先；否则只做"将要填充区域"的实时预览，
    // 不进入绘制/捕捉悬停逻辑（避免与点选语义混淆）。
    if (regionPickMode_ && !panning_) {
        emit regionHoverMoved(w.x, w.y);
        return;
    }
    // 窗口缩放框选进行中：实时更新框选矩形并刷新视图（必须早于绘制预览分支，
    // 否则若当前恰好是绘制子工具会被 updatePreview 抢先 return，框选矩形不再跟随光标）。
    if (zoomBoxActive_) {
        zoomBoxRect_.setBottomRight(QPointF(w.x, w.y));
        viewport()->update();
        return;
    }
    if ((e->buttons() & Qt::RightButton) && !panning_)
        panDbg(QStringLiteral("[move] ⚠ 右键按住但 panning_=false! buttons=%1 pendingPts=%2 tool=%3")
                   .arg((int)e->buttons()).arg(pendingPts_.size()).arg(model_->activeTool()));
    // 卡死态兜底恢复：panning_ 为真却没有按键按住（且非「空格+左键」拖动），
    // 说明上一轮释放事件被上下文菜单/系统吞掉、panning_ 未复位。这里立刻复位，避免视图卡在平移态。
    if (panning_ && e->buttons() == Qt::NoButton && !(spaceDown_ && (e->buttons() & Qt::LeftButton))) {
        panning_ = false;
        panRight_ = false;
        rightMoved_ = false;
        setCursor(Qt::ArrowCursor);
        panDbg(QStringLiteral("[move] 兜底复位 panning_（卡死态恢复）buttons=%1 spaceDown_=%2")
                   .arg((int)e->buttons()).arg(spaceDown_));
        return;
    }
    if (panning_) {
        setCursor(Qt::ClosedHandCursor);          // 平移中显示「抓手」光标，明确当前在平移视图
        const double dx = vp.x() - lastPan_.x();
        const double dy = vp.y() - lastPan_.y();
        const double px = panX_, py = panY_;
        panX_ += dx;
        panY_ += dy;
        lastPan_ = vp;
        if (panRight_) rightMoved_ = true;   // 右键拖拽发生位移
        applyTransform();
        viewport()->update();                 // 确保平移后即刻重绘（与 fitView/zoomBy 一致）
        panDbg(QStringLiteral("[move] 平移中 dx=%1 dy=%2 pan(%3,%4)->(%5,%6) zoom=%7")
                   .arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1)
                   .arg(px, 0, 'f', 1).arg(py, 0, 'f', 1)
                   .arg(panX_, 0, 'f', 1).arg(panY_, 0, 'f', 1).arg(zoom_, 0, 'f', 2));
        return;
    }
    if (gripHot_) {
        // 夹点热态：按住左键拖拽 = 持续预览（落盘延迟到确认）；
        // 未按键移动（armed 态）也刷新预览，跟随光标。
        if (e->buttons() & Qt::LeftButton) gripDragMoved_ = true;
        updateGripPreview(w);
        return;
    }
    // 图元平移拖拽：首次移动压撤销快照，后续帧增量平移全部走 Quiet
    if (dragMovePending_) {
        const Vec3 delta = w - dragMoveLast_;
        if (delta.len() > 1e-6) {
            // 首次发生有效位移 → 正式进入拖拽，压一次撤销快照
            model_->beginEdit(QStringLiteral("平移图元"));
            draggingEntities_ = true;
            dragMovePending_ = false;
        }
    }
    if (draggingEntities_) {
        const Vec3 delta = w - dragMoveLast_;
        if (delta.len() > 1e-9) {
            for (int id : model_->selection()) {
                model_->updateElementQuiet(id, [&delta](Element& e) {
                    for (auto& p : e.pts) p = p + delta;
                });
            }
            dragMoveLast_ = w;
        }
        return;
    }
    // 标注两步流程：已选图元 → 鼠标移动时实时刷新标注预览
    if (dimPending_) { updatePreview(snapWorld(w)); return; }
    // 只要有已拾取的点就刷新预览（不再要求 drawing_），
    // 这样圆弧在「点完圆心」之后立刻就有跟手反馈。
    if (!pendingPts_.empty()) { updatePreview(snapWorld(w)); return; }
    // 直线子模式（角度/平行线等）在未拾取点时也要跟手预览（光标即起点/方向），
    // 故即使 pendingPts_ 为空也刷新。
    if (isLineSubTool(model_->activeTool())) { updatePreview(snapWorld(w)); return; }
    // 框选进行中：实时更新矩形并刷新视图（选中结果在释放时一次性结算）
    if (marqueeActive_) {
        marquee_.setBottomRight(QPointF(w.x, w.y));
        viewport()->update();
        return;
    }
    // 自由手绘等需要实时拖拽采样的工具：派发 move 事件（无拖拽时 onMove 为 no-op）
    toolMgr_->dispatchMove(w, e);
    refreshHover(w);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent* e) {
    if (panning_ && (e->button() == Qt::MiddleButton ||
                     (e->button() == Qt::LeftButton && spaceDown_))) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);          // 平移结束，复位光标
        e->accept();
        return;
    }
    // 右键释放：若发生过拖动则视为「右键拖拽平移」（不再触发取消逻辑），否则仍是原「结束/取消」单击
    if (panning_ && e->button() == Qt::RightButton) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);          // 平移结束（或右键单击），复位光标
        const bool wasClick = !rightMoved_;
        panDbg(QStringLiteral("[release] 右键释放 panning_=true wasClick=%1 rightMoved_=%2 -> %3")
                   .arg(wasClick).arg(rightMoved_)
                   .arg(wasClick ? QStringLiteral("执行 handleRightButton(结束/取消)")
                                : QStringLiteral("仅结束平移")));
        panRight_ = false;
        rightMoved_ = false;
        if (wasClick) {
            if (regionPickMode_) {           // 区域拾取模式：右键单击 = 退出（不触发工具取消）
                setRegionPickMode(false);
                emit regionPickCancelled();
            } else {
                handleRightButton();
            }
        }
        e->accept();
        return;
    }
    // 图元平移释放：单击（未移动）则仅清状态不压撤销；拖拽过则记录日志
    if (dragMovePending_ || draggingEntities_) {
        const bool wasDrag = draggingEntities_;
        dragMovePending_ = false;
        draggingEntities_ = false;
        if (wasDrag) {
            const Vec3 w = toVec3(mapToScene(e->position().toPoint()));
            model_->log(QString("已平移 %1 个图元至 (%2, %3)")
                            .arg(model_->selection().size())
                            .arg(w.x, 0, 'f', 2).arg(w.y, 0, 'f', 2));
        }
        e->accept();
        return;
    }
    // 框选结算：释放时一次性选中矩形内的所有可见图元
    if (marqueeActive_ && e->button() == Qt::LeftButton) {
        marqueeActive_ = false;
        const QRectF r = marquee_.normalized();
        const bool additive = e->modifiers().testFlag(Qt::ControlModifier);
        const double tol = worldTol();
        if (r.width() < tol && r.height() < tol) {
            // 几乎是一次点击（未拖出有效矩形）：空白点击 -> 清空选择（加选/取消模式除外）
            if (!additive && !deselectMarquee_) model_->clearSelection();
        } else if (deselectMarquee_) {
            // 窗口取消：移除矩形内图元的选中
            model_->deselectWindow(r);
        } else {
            // 统一走核心层 selectWindow，不在交互层另写一份 intersects 判定：
            // 「框选命中」只应有一份真相，菜单/命令行与画布拖拽必须同语义，否则两条路径
            // 会各自漂移。核心层按「几何包围盒 + 1e-6 双侧膨胀」判定，零面积包围盒
            // （水平线 / 垂直线）也能命中；结算日志同样由核心层统一输出。
            model_->selectWindow(r, additive);
        }
        marquee_ = QRectF();
        viewport()->update();
        e->accept();
        return;
    }
    // 窗口缩放框选结算：释放左键 → 该区域精确缩放并充满整个绘图工作区域
    if (zoomBoxActive_ && e->button() == Qt::LeftButton) {
        zoomBoxActive_ = false;
        const QRectF r = zoomBoxRect_.normalized();
        zoomBoxRect_ = QRectF();
        const double tol = worldTol();
        if (r.width() < tol && r.height() < tol) {
            // 几乎未拖出有效矩形（视为单击）：回退「适配窗口」作为兜底，并退出框选模式
            setZoomBoxMode(false);
            fitView();
        } else {
            setZoomBoxMode(false);          // 框选完成，自动退出模式
            fitWorldRect(r, 1.0);           // 该区域精确缩放并充满整个绘图工作区域
        }
        viewport()->update();
        e->accept();
        return;
    }
    if (gripHot_ && e->button() == Qt::LeftButton) {
        // 按住拖拽过 → 释放即确认（保留旧「拖完就走」手感）；
        // 单击进入热态（未拖拽）→ armed，等待模式切换/移动/确认单击。
        if (gripDragMoved_ || gripArmed_) {
            commitGrip();
        } else {
            gripArmed_ = true;
            updateGripPreview(lastMouseWorld_);
        }
        e->accept();
        return;
    }

    // 自由手绘：松开左键即结束绘制（左键拖拽采样后派发 release 事件）
    {
        const Vec3 wRel = toVec3(mapToScene(e->position().toPoint()));
        toolMgr_->dispatchRelease(wRel, e);
    }
}

void GraphicsView::mouseDoubleClickEvent(QMouseEvent* e) {
    const QString t = model_->activeTool();
    if ((t == "poly" || t == "spline") && pendingPts_.size() >= 2) {
        finalizePoly(t == "spline");
        e->accept();
        return;
    }
    // 栏选/圈围/圈交：双击 = 结束采集并结算
    if ((t == "fence" || t == "wp" || t == "cp") && pendingPts_.size() >= 2) {
        finalizeShapePick();
        e->accept();
        return;
    }
    // 双击文字图元（Text/MText）→ 打开多行文字编辑框；双击表格（Table）→ 打开表格编辑器
    if (e->button() == Qt::LeftButton) {
        const int id = hitTest(e->position().toPoint());
        if (id >= 0) {
            const Element* el = model_->element(id);
            if (el && (el->type == ElementType::Text || el->type == ElementType::MText ||
                       el->type == ElementType::Table)) {
                emit editTextRequested(id);
                e->accept();
                return;
            }
        }
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

// 右键菜单手势：一律忽略，把右键完整让给「拖拽平移 / 单击结束绘制」逻辑。
// 即便将来某处把视图或视口的 contextMenuPolicy 改成了 DefaultContextMenu，
// 这里也会吃掉上下文菜单事件，杜绝它抢占右键按下/释放、导致「拖不动」。
void GraphicsView::contextMenuEvent(QContextMenuEvent* e) {
    panDbg(QStringLiteral("[ctx] contextMenuEvent 触发并 ignore()（理论上 NoContextMenu 下不应出现）"));
    // 若上下文菜单在平移过程中抢占（导致 release 未被派发），这里兜底复位平移态，避免卡死。
    if (panning_) {
        panning_ = false;
        panRight_ = false;
        rightMoved_ = false;
        setCursor(Qt::ArrowCursor);
        panDbg(QStringLiteral("[ctx] 平移中触发上下文菜单，兜底复位 panning_"));
    }
    e->ignore();
}

void GraphicsView::wheelEvent(QWheelEvent* e) {
    const QPointF screen = e->position();
    const QPointF anchor = mapToScene(screen.toPoint());
    zoom_ = qBound(0.05, zoom_ * (e->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1), 500.0);
    panX_ = screen.x() - anchor.x() * zoom_;
    panY_ = screen.y() + anchor.y() * zoom_;   // Y 翻转
    applyTransform();
    model_->setViewScale(zoom_);
    viewport()->update();
    e->accept();
}

void GraphicsView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    // 视口尺寸变化会改变 applyTransform 里那个「常量偏移 C」的大小，必须重算一次，
    // 否则窗口缩放后画面会整体偏移（C 与视口尺寸相关）。
    applyTransform();
    // 视口大小变化后，可见世界坐标范围也变了，标尺需要刷新
    emit viewTransformChanged();
}

void GraphicsView::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space) { spaceDown_ = true; return; }
    // 夹点热态：Ctrl 循环模式 / Enter 确认 / Esc 取消（优先级高于其它全局按键）
    if (gripHot_) {
        if (e->key() == Qt::Key_Control) {
            gripMode_ = grip::nextMode(gripMode_);
            gripData_.refCursor = lastMouseWorld_;   // 模式零参考重置：预览从恒等起步
            gripDragMoved_ = false;
            model_->log(QString("夹点模式：%1（Ctrl 继续切换，移动预览，单击/Enter 确认，Esc 取消）")
                            .arg(grip::modeName(gripMode_)));
            updateGripPreview(lastMouseWorld_);
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
            commitGrip();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Escape) {
            cancelGrip();
            e->accept();
            return;
        }
    }
    if (e->key() == Qt::Key_Escape) {
        if (regionPickMode_) {         // 区域拾取模式（剖面填充）：Esc 退出并通知上层
            setRegionPickMode(false);
            emit regionPickCancelled();
            e->accept();
            return;
        }
        if (zoomBoxMode_ || zoomBoxActive_) {   // 窗口缩放框选：Esc 取消并退出模式
            setZoomBoxMode(false);
            e->accept();
            return;
        }
        if (dimPending_ || drawing_ || !pendingPts_.empty() || !pickedIds_.empty() ||
            !trimBounds_.empty() || marqueeActive_) {
            resetPending();
            model_->log(QStringLiteral("已取消当前操作"), "warn");
            e->accept();
            return;
        }
        if (pointPickMode_) {          // 拾取模式下的 Esc：取消拾取，复位光标
            pointPickMode_ = false;
            setCursor(Qt::ArrowCursor);
            emit pointPickCancelled();
            e->accept();
            return;
        }
    }
    // 区域拾取模式：Enter 请求打开剖面参数对话框（角度/间距/材料/豁免）。
    if (regionPickMode_ && (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)) {
        emit regionPickParamsRequested();
        e->accept();
        return;
    }
    if ((e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) && !pendingPts_.empty()) {
        const QString t = model_->activeTool();
        if (t == "fence" || t == "wp" || t == "cp") { finalizeShapePick(); e->accept(); return; }
        if (t == "poly" || t == "spline") { finalizePoly(t == "spline"); e->accept(); return; }
        if (isLineSubTool(t) && line::modeFromString(t) == line::Mode::Snake) { finalizeSnake(); e->accept(); return; }
        if (isLineSubTool(t) && line::modeFromString(t) == line::Mode::SnakeX) { finalizeSnake(); e->accept(); return; }
        if (isLineSubTool(t) && line::modeFromString(t) == line::Mode::SnakeY) { finalizeSnake(); e->accept(); return; }
        if (t == "line") { resetPending(); model_->log(QStringLiteral("已结束连续直线绘制")); e->accept(); return; }
    }
    if (e->key() == Qt::Key_Backspace && (!pendingPts_.empty() || !pickedIds_.empty())) {
        if (!pendingPts_.empty()) {
            pendingPts_.pop_back();
            model_->log(QStringLiteral("已回退一个拾取点"), "warn");
        } else {
            pickedIds_.pop_back();
            model_->log(QStringLiteral("已回退一个参考图元"), "warn");
        }
        if (pendingPts_.empty() && pickedIds_.empty()) resetPending();
        else                                             clearPreview();
        e->accept();
        return;
    }
    QGraphicsView::keyPressEvent(e);
}

void GraphicsView::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space) spaceDown_ = false;
    QGraphicsView::keyReleaseEvent(e);
}

// ---------------------------- 拖拽创建 ----------------------------

void GraphicsView::createDefaultAt(const QString& type, const Vec3& w) {
    // 统一以英文工具标识为准（元素面板拖拽 / 工具条 / 命令行共用同一套 id），
    // 同时兼容中文别名，避免上游传入显示名时静默退化成直线。
    static const QHash<QString, QString> alias = {
        {"直线", "line"}, {"圆", "circle"}, {"三点圆", "circle3p"}, {"圆弧", "arc"},
        {"矩形", "rect"}, {"多边形", "poly"}, {"样条曲线", "spline"},
        {"抛物线", "spline_parabola"}, {"双曲线", "spline_hyperbola"},
        {"抛物线(4点)", "spline_parabola_4p"}, {"抛物线(焦点准线)", "spline_parabola_fd"},
        {"样条过点", "spline_through_points"}, {"自由手绘", "freehand_line"},
        {"由多段线创建样条", "spline_from_polyline"}, {"追加样条点", "spline_append_pt"},
        {"插入样条点", "spline_insert_pts"}, {"删除样条点", "spline_remove_pts"},
        {"删除两点之间", "spline_remove_between"}, {"分解样条", "spline_explode"},
        {"约束", "constraint"}, {"标注", "dim"},
        {"线性标注", "dim"}, {"对齐标注", "dim_aligned"}, {"角度标注", "dim_angle"},
        {"半径标注", "dim_radius"}, {"直径标注", "dim_diameter"},
        {"水平标注", "dim_h"}, {"竖直标注", "dim_v"},
        {"智能标注", "dim_smart"},
        {"椭圆(1点)", "ellipse_1pt"}, {"椭圆(轴)", "ellipse_axis"},
        {"椭圆(焦点)", "ellipse_foci"}, {"椭圆(4点)", "ellipse_4p"},
        {"椭圆(中心+3点)", "ellipse_c3p"}, {"椭圆(内切)", "ellipse_inscribed"}
    };
    const QString t = alias.value(type, type.toLower());

    Element e;
    if (t == "line")            { e = ElementFactory::create(ElementType::Line, "直线", model_);      e.pts = {w, w + Vec3(20, 0, 0)}; }
    else if (t == "circle" ||
             t == "circle3p")   { e = ElementFactory::create(ElementType::Circle, "圆", model_);      e.pts = {w}; e.radius = 10; }
    else if (t == "arc")        { e = ElementFactory::create(ElementType::Arc, "圆弧", model_);        e.pts = {w}; e.radius = 10; e.startAngle = 0; e.endAngle = 180; }
    else if (t == "rect")       { e = ElementFactory::create(ElementType::Rectangle, "矩形", model_);  e.pts = {w, w + Vec3(20, 20, 0)}; }
    else if (t == "poly")       { e = ElementFactory::create(ElementType::Polygon, "多边形", model_);  e.pts = {w, w + Vec3(15, 0, 0), w + Vec3(15, 15, 0), w + Vec3(0, 15, 0)}; }
    else if (t == "spline")     { e = ElementFactory::create(ElementType::Spline, "样条曲线", model_); e.pts = {w, w + Vec3(10, 12, 0), w + Vec3(22, -6, 0), w + Vec3(34, 8, 0)}; }
    else if (t == "spline_parabola")  { e = ElementFactory::create(ElementType::Spline, "抛物线", model_); e.splineSubType = 1; e.pts = {w, w + Vec3(15, 20, 0), w + Vec3(30, 0, 0)}; e.closed = false; }
    else if (t == "spline_hyperbola") { e = ElementFactory::create(ElementType::Spline, "双曲线", model_); e.splineSubType = 2; e.pts = {w + Vec3(-10, 0, 0), w + Vec3(10, 0, 0), w + Vec3(15, 8, 0)}; e.closed = false; }
    else if (t == "spline_parabola_4p") { e = ElementFactory::create(ElementType::Spline, "抛物线(4点)", model_); e.splineSubType = 3; e.pts = {w, w + Vec3(10, 12, 0), w + Vec3(22, -6, 0), w + Vec3(34, 8, 0)}; e.closed = false; }
    else if (t == "spline_parabola_fd") { e = ElementFactory::create(ElementType::Spline, "抛物线(焦点准线)", model_); e.splineSubType = 4; e.pts = {w + Vec3(0, 12, 0), w + Vec3(-10, 0, 0), w + Vec3(10, 0, 0)}; e.closed = false; }
    else if (t == "spline_through_points") { e = ElementFactory::create(ElementType::Spline, "样条过点", model_); e.splineSubType = 5; e.pts = {w, w + Vec3(10, 12, 0), w + Vec3(22, -6, 0), w + Vec3(34, 8, 0)}; e.closed = false; }
    else if (t == "freehand_line") { e = ElementFactory::create(ElementType::Spline, "自由手绘线", model_); e.splineSubType = 6; e.pts = {w, w + Vec3(10, 12, 0), w + Vec3(22, -6, 0), w + Vec3(34, 8, 0)}; e.closed = false; }
    else if (t == "ellipse_1pt" || t == "ellipse_axis" || t == "ellipse_foci" ||
             t == "ellipse_4p" || t == "ellipse_c3p" || t == "ellipse_inscribed") {
        // 椭圆系列在拖拽落点时统一生成默认形态：中心 w，长轴 20mm、短轴 10mm、不旋转
        e = ElementFactory::createEllipse(w, 20.0, 10.0, 0.0, model_);
    }
    else if (t == "constraint") { e = ElementFactory::create(ElementType::Constraint, "约束-固定", model_); e.ctype = ConstraintType::Fixed; e.pts = {w}; }
    else {
        bool isDim = false;
        const DimensionType dt = dimToolType(t, isDim);
        if (!isDim) { model_->log("未知图元类型: " + type + "，已忽略", "warn"); return; }
        e = ElementFactory::create(ElementType::Dimension, QString::fromUtf8(dimensionTypeName(dt)), model_);
        e.dtype = dt;
        e.standard = model_->dimensionStandard();
        // text 留空，数值由 dim::formatValue 按实测自动生成，
        // 这样后续拖动测量点时数字会跟着变，而不是永远停在创建那一刻的旧值。
        if (dt == DimensionType::Radius || dt == DimensionType::Diameter) {
            e.radius = 10;
            e.pts = {w, w + Vec3(10, 10, 0)};
        } else if (dt == DimensionType::Angle) {
            e.pts = {w, w + Vec3(24, 0, 0), w + Vec3(17, 17, 0)};
        } else {
            // 第 3 点给出尺寸线位置
            e.pts = {w, w + Vec3(20, 0, 0), w + Vec3(10, 12, 0)};
        }
    }
    const int id = model_->addElement(e);
    model_->setSelected(id);
    model_->log("拖拽创建: " + e.name + " #" + QString::number(id));
}

void GraphicsView::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasFormat(ELEMENT_MIME)) e->acceptProposedAction();
    else e->ignore();
}

void GraphicsView::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasFormat(ELEMENT_MIME)) { e->ignore(); return; }
    const QString type = QString::fromUtf8(e->mimeData()->data(ELEMENT_MIME));
    const Vec3 w = toVec3(mapToScene(e->position().toPoint()));
    createDefaultAt(type, w);
    e->acceptProposedAction();
}

} // namespace SketchCAD
