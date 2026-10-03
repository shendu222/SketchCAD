// ============================================================================
// 视图无关的图元几何路径 / 包围盒 —— 实现
// ----------------------------------------------------------------------------
// 算法自 GraphicsView.cpp 的 ElementItem::buildPath / xlineClipToRect /
// rawBounds **原样下沉**（仅把 ElementItem:: 前缀改为本命名空间、
// expandedBlock() 实例调用替换为 model 参数展开），几何行为逐字节一致。
// ============================================================================
#include "ElementPath.h"
#include "core/Geometry.h"
#include "core/SplineModes.h"
#include "core/DimStyle.h"
#include "core/Gdt.h"       // buildMLeader / buildFcf：多重引线与形位公差框格路径
#include "core/TextStyle.h"
#include "core/SketchModel.h"

#include <QFont>
#include <QFontMetricsF>
#include <QStringList>
#include <cmath>

namespace SketchCAD {
namespace ElementPath {

namespace {

// 世界坐标 → QPointF（与 GraphicsView 内 qpt 同一语义）
inline QPointF qpt(const Vec3& v) { return QPointF(v.x, v.y); }

// 构造线裁剪回落矩形：与 GraphicsView 的 kXlineBigRect 同值
static const QRectF kXlineBigRect(-1e5, -1e5, 2e5, 2e5);

} // namespace

// ---------------------------------------------------------------------------
// 构造线 ∩ 矩形 求交（原 ElementItem::xlineClipToRect）
// ---------------------------------------------------------------------------
bool xlineClipToRect(const Element& e, const QRectF& r, Vec3& a, Vec3& b) {
    if (e.type != ElementType::Xline || e.pts.empty() || r.isNull()) return false;
    const double rad = math::deg2rad(e.rotation);
    const Vec3 d(std::cos(rad), std::sin(rad), 0);
    const Vec3& p0 = e.pts[0];
    double t0 = -1e18, t1 = 1e18;
    const double p[2]  = { p0.x, p0.y };
    const double dd[2] = { d.x, d.y };
    const double lo[2] = { r.left(), r.top() };
    const double hi[2] = { r.right(), r.bottom() };
    for (int i = 0; i < 2; ++i) {
        if (std::fabs(dd[i]) < 1e-12) {
            if (p[i] < lo[i] || p[i] > hi[i]) return false;   // 平行且在界外
        } else {
            double ta = (lo[i] - p[i]) / dd[i];
            double tb = (hi[i] - p[i]) / dd[i];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) return false;
        }
    }
    a = Vec3(p0.x + d.x * t0, p0.y + d.y * t0, 0);
    b = Vec3(p0.x + d.x * t1, p0.y + d.y * t1, 0);
    return true;
}

// ---------------------------------------------------------------------------
// 图元 → 绘制路径（原 ElementItem::buildPath）
// ---------------------------------------------------------------------------
QPainterPath buildPath(const Element& e, const QRectF& xlineClip) {
    QPainterPath path;
    switch (e.type) {
        case ElementType::Xline: {
            // 构造线：无限直线，按裁剪矩形（优先当前可见区域，空则回落场景级大矩形）
            // 画出交线段。缩放/平移时 paintOne 每帧传入最新可见区域 → 显示自动同步。
            const QRectF clip = xlineClip.isNull() ? kXlineBigRect : xlineClip;
            Vec3 a, b;
            if (xlineClipToRect(e, clip, a, b)) {
                path.moveTo(qpt(a));
                path.lineTo(qpt(b));
            }
            break;
        }

        case ElementType::Line:
            if (e.pts.size() >= 2) {
                // GB/T 14665-2012 4.1：波浪线（断裂）与双折线 Qt 无内建画笔，用自定义折线路径绘制。
                if (e.lineStyle == LineStyle::Wave || e.lineStyle == LineStyle::DoubleBreak) {
                    const Vec3 a = e.pts[0], b = e.pts[1];
                    const double dx = b.x - a.x, dy = b.y - a.y;
                    const double L = std::hypot(dx, dy);
                    if (L < 1e-9) { path.moveTo(qpt(a)); path.lineTo(qpt(b)); break; }
                    const double ux = dx / L, uy = dy / L;
                    const double nx = -uy, ny = ux;                 // 法向单位向量
                    const double amp = std::max(1.0, L * 0.015);    // 波幅
                    const int n = std::max(8, (int)std::lround(L / 2.0));
                    path.moveTo(qpt(a));
                    if (e.lineStyle == LineStyle::Wave) {
                        const double k = std::max(1.0, std::round(L / 10.0));  // 波数
                        for (int i = 1; i <= n; ++i) {
                            const double t = (double)i / n;
                            const double s = amp * std::sin(2.0 * 3.14159265358979323846 * k * t);
                            path.lineTo(qpt(Vec3(a.x + dx * t + nx * s, a.y + dy * t + ny * s, 0)));
                        }
                    } else {   // 双折线：两处折角的 Z 形
                        const double ts[] = { 0.42, 0.50, 0.58 };
                        const double ss[] = { 0.0, -amp, 0.0 };
                        for (int i = 0; i < 3; ++i)
                            path.lineTo(qpt(Vec3(a.x + dx * ts[i] + nx * ss[i],
                                                 a.y + dy * ts[i] + ny * ss[i], 0)));
                        path.lineTo(qpt(b));
                    }
                } else {
                    path.moveTo(qpt(e.pts[0])); path.lineTo(qpt(e.pts[1]));
                }
            }
            break;

        case ElementType::Circle:
            if (!e.pts.empty() && e.radius > 1e-9)
                path.addEllipse(qpt(e.pts[0]), e.radius, e.radius);
            break;

        case ElementType::Arc: {
            if (e.pts.empty() || e.radius <= 1e-9) break;
            const QRectF r(e.pts[0].x - e.radius, e.pts[0].y - e.radius,
                           2 * e.radius, 2 * e.radius);
            // Qt 的圆弧角约定是「在未翻转坐标系中逆时针为正」，即取点公式为 (cx+r·cosθ, cy−r·sinθ)。
            // 本视图已把 Y 轴翻转成数学正方向，世界角 a 对应的 Qt 角就是 −a，否则圆弧会上下镜像。
            path.arcMoveTo(r, -e.startAngle);
            path.arcTo(r, -e.startAngle, -(e.endAngle - e.startAngle));
            break;
        }

        case ElementType::Ellipse: {
            if (e.pts.empty() || e.radius <= 1e-9 || e.radiusY <= 1e-9) break;
            const QPointF c = qpt(e.pts[0]);
            // 先在局部系（长轴沿 X）画标准椭圆，再以世界长轴角旋转、平移到圆心。
            // qpt 直接返回世界坐标，画家变换负责 Y 翻转，故这里用标准数学旋转（逆时针为正）。
            QPainterPath ep;
            ep.addEllipse(QPointF(0, 0), e.radius, e.radiusY);
            QTransform rot;
            rot.rotate(e.rotation);            // 标准数学旋转：CCW 为正，与 e.rotation 语义一致
            QTransform tr;
            tr.translate(c.x(), c.y());
            path.addPath(ep * rot * tr);
            break;
        }

        case ElementType::Rectangle:
            if (e.pts.size() >= 2)
                path.addRect(QRectF(qpt(e.pts[0]), qpt(e.pts[1])).normalized());
            break;

        case ElementType::Point:
            break;          // 点实体：无连续几何路径；渲染与拾取由 paintOne / distanceToElement 处理

        case ElementType::Dimension:
            // 标注的可见几何完全由 dim::build 生成（尺寸线/界线/箭头/数字），
            // 这里只给出「测量线」用于包围盒与命中，实际绘制见 paintOne。
            if (e.pts.size() >= 2) { path.moveTo(qpt(e.pts[0])); path.lineTo(qpt(e.pts[1])); }
            break;

        case ElementType::Polygon: {
            if (e.pts.size() < 2) break;
            QPolygonF poly;
            for (const auto& v : e.pts) poly << qpt(v);
            path.addPolygon(poly);
            // 被剪切过的多边形是开放折线，强行闭合会凭空补出一条并不存在的边
            if (e.polyClosed()) path.closeSubpath();
            break;
        }

        case ElementType::Polyline: {
            // 折线：直线段 lineTo，弧段走 arcThrough 算出的圆心+起止角后 addEllipse 子路径
            if (e.pts.size() < 2) break;
            // 拟合 / 样条化：smooth 曲线（顶点为节点或控制点），与 PEDIT Fit/Spline 一致
            if (e.splineSubType == 1 && e.pts.size() >= 2) {
                const std::vector<Vec3> curve = geo::samplePolylineFit(e.pts, e.closed, 16);
                if (!curve.empty()) {
                    path.moveTo(qpt(curve[0]));
                    for (size_t i = 1; i < curve.size(); ++i) path.lineTo(qpt(curve[i]));
                    if (e.closed) path.closeSubpath();
                }
                break;
            }
            if (e.splineSubType == 2 && e.pts.size() >= 3) {
                const std::vector<Vec3> curve = geo::samplePolylineSpline(e.pts, e.closed, 16);
                if (!curve.empty()) {
                    path.moveTo(qpt(curve[0]));
                    for (size_t i = 1; i < curve.size(); ++i) path.lineTo(qpt(curve[i]));
                    if (e.closed) path.closeSubpath();
                }
                break;
            }
            const int n = (int)e.pts.size();
            const int nSegs = e.closed ? n : n - 1;
            for (int i = 0; i < nSegs; ++i) {
                const Vec3& a = e.pts[i];
                const Vec3& b = e.pts[(i + 1) % n];
                if (i == 0) path.moveTo(qpt(a));
                const bool isArc = (i < (int)e.arcFlags.size()) && e.arcFlags[i];
                if (isArc) {
                    geo::ArcSeg ar = geo::arcThrough(e.pts, i, e.closed);
                    if (ar.valid && ar.radius > 1e-6) {
                        const QRectF r(ar.center.x - ar.radius,
                                       ar.center.y - ar.radius,
                                       2 * ar.radius, 2 * ar.radius);
                        path.arcMoveTo(r, -ar.startDeg);
                        path.arcTo(r, -ar.startDeg, -ar.sweepDeg);
                        // arcThrough 端点保持：弧终点恒为 b，后续 lineTo/closeSubpath 自然衔接
                    } else {
                        path.lineTo(qpt(b));
                    }
                } else {
                    path.lineTo(qpt(b));
                }
            }
            if (e.closed) path.closeSubpath();
            break;
        }

        case ElementType::Spline: {
            if (e.splineSubType == 1 && e.pts.size() >= 3) {
                // 抛物线（二次贝塞尔）：P0 → 控制点 P1 → 终点 P2
                path.moveTo(qpt(e.pts[0]));
                path.quadTo(qpt(e.pts[1]), qpt(e.pts[2]));
            } else if (e.splineSubType == 2 && e.pts.size() >= 3) {
                // 双曲线：采样后绘制折线（F1, F2, P → 离散点序列）
                const auto samples = spline::sampleHyperbola(e.pts[0], e.pts[1], e.pts[2]);
                if (!samples.empty()) {
                    path.moveTo(qpt(samples[0]));
                    for (size_t i = 1; i < samples.size(); ++i)
                        path.lineTo(qpt(samples[i]));
                }
            } else if (e.splineSubType == 3 && e.pts.size() >= 4) {
                // 抛物线(4点)：拟合二次贝塞尔后采样
                const auto samples = spline::sampleParabola4P(e.pts);
                if (!samples.empty()) {
                    path.moveTo(qpt(samples[0]));
                    for (size_t i = 1; i < samples.size(); ++i)
                        path.lineTo(qpt(samples[i]));
                }
            } else if (e.splineSubType == 4 && e.pts.size() >= 3) {
                // 抛物线(焦点准线)
                const auto samples = spline::sampleParabolaFD(e.pts[0], e.pts[1], e.pts[2]);
                if (!samples.empty()) {
                    path.moveTo(qpt(samples[0]));
                    for (size_t i = 1; i < samples.size(); ++i)
                        path.lineTo(qpt(samples[i]));
                }
            } else if (e.splineSubType == 5 || e.splineSubType == 6) {
                // 样条过点 / 自由手绘：Catmull-Rom 插值
                const std::vector<Vec3> bz = geo::catmullRomToBezier(e.pts);
                if (bz.size() >= 4) {
                    path.moveTo(qpt(bz[0]));
                    for (size_t i = 1; i + 2 < bz.size(); i += 3)
                        path.cubicTo(qpt(bz[i]), qpt(bz[i + 1]), qpt(bz[i + 2]));
                } else if (e.pts.size() >= 2) {
                    path.moveTo(qpt(e.pts[0]));
                    for (size_t i = 1; i < e.pts.size(); ++i) path.lineTo(qpt(e.pts[i]));
                }
            } else {
                // 默认：控制点 -> Catmull-Rom -> 三次贝塞尔
                const std::vector<Vec3> bz = geo::catmullRomToBezier(e.pts);
                if (bz.size() >= 4) {
                    path.moveTo(qpt(bz[0]));
                    for (size_t i = 1; i + 2 < bz.size(); i += 3)
                        path.cubicTo(qpt(bz[i]), qpt(bz[i + 1]), qpt(bz[i + 2]));
                } else if (e.pts.size() >= 2) {
                    path.moveTo(qpt(e.pts[0]));
                    for (size_t i = 1; i < e.pts.size(); ++i) path.lineTo(qpt(e.pts[i]));
                }
            }
            break;
        }

        case ElementType::Constraint:
            break;   // 约束标记按屏幕像素画，见 paintOne

        case ElementType::Text:
        case ElementType::MText:
            break;   // 文本由 paintText 绘制，几何包围盒在 rawBounds 里按文字尺寸展开
        case ElementType::Table:
            break;   // 表格由 paintTable 绘制（网格线+单元格文字），包围盒见 rawBounds

        case ElementType::BlockRef:
            // 块引用自身没有几何：展开后的子路径由 blockPath()（shape）与 paintBlockRef 处理
            break;

        case ElementType::Hatch: {
            // 剖面区域：外边界 + 内孔（子路径）。采用 OddEvenFill，使内孔被掏空
            // （带孔剖面如法兰螺栓孔即靠此正确留白）。实际剖面线绘制见 paintHatch。
            if (e.pts.size() < 3) break;
            QPolygonF outer;
            for (const auto& v : e.pts) outer << qpt(v);
            path.addPolygon(outer);
            path.closeSubpath();
            for (const auto& h : e.holes) {
                if (h.size() < 3) continue;
                QPolygonF ring;
                for (const auto& v : h) ring << qpt(v);
                path.addPolygon(ring);
                path.closeSubpath();
            }
            path.setFillRule(Qt::OddEvenFill);
            break;
        }

        case ElementType::Boundary: {
            // 交互式自定义边界（BOUNDARY/BPOLY）：几何结构与 Hatch 完全一致
            // （pts=外环、holes=内孔），仅语义为"待填充/已指定边界"，故复用同一几何路径；
            // 渲染则只描边（虚线边界）而不画剖面线，见 paintOne。
            if (e.pts.size() < 3) break;
            QPolygonF outer;
            for (const auto& v : e.pts) outer << qpt(v);
            path.addPolygon(outer);
            path.closeSubpath();
            for (const auto& h : e.holes) {
                if (h.size() < 3) continue;
                QPolygonF ring;
                for (const auto& v : h) ring << qpt(v);
                path.addPolygon(ring);
                path.closeSubpath();
            }
            path.setFillRule(Qt::OddEvenFill);
            break;
        }

        case ElementType::MLeader: {
            // 多重引线：引线折线 + 基线（文字不在路径内，由 paintMLeader 单独绘）
            const MLeaderGeom g = buildMLeader(e);
            if (!g.valid) break;
            for (const auto& s : g.leader) { path.moveTo(qpt(s.a)); path.lineTo(qpt(s.b)); }
            for (const auto& s : g.landing) { path.moveTo(qpt(s.a)); path.lineTo(qpt(s.b)); }
            break;
        }

        case ElementType::Fcf: {
            // 形位公差框格：外框 + 符号笔画（各格矩形已在 Gdt 几何中算好）
            const FcfGeom g = buildFcf(e);
            if (!g.valid) break;
            for (const auto& s : g.frame)      { path.moveTo(qpt(s.a)); path.lineTo(qpt(s.b)); }
            for (const auto& s : g.symbolStrokes) { path.moveTo(qpt(s.a)); path.lineTo(qpt(s.b)); }
            break;
        }
    }
    return path;
}

// ---------------------------------------------------------------------------
// 几何包围盒（原 ElementItem::rawBounds；model 可空，见头文件语义说明）
// ---------------------------------------------------------------------------
QRectF rawBounds(const Element& e, const SketchModel* model) {
    // 构造线：几何范围只取「空矩形」——「适配窗口」不被 ±1e5 的无限线撑爆（fit 排除），
    // 命中/拾取走 geo::distanceToElement 的点到无限直线距离；剔除用的包围盒由
    // boundingRect() 的 Xline 专案（直线 ∩ 场景级大矩形）另行给出。
    if (e.type == ElementType::Xline) return QRectF();
    QRectF r;
    bool has = false;
    const QPainterPath p = buildPath(e);
    if (!p.isEmpty()) { r = p.boundingRect(); has = true; }
    // 用显式 min/max 展开控制点。注意不能用 QRectF::united —— 零尺寸矩形被判为 null，
    // united 会直接丢弃它，旧实现正是因此让所有直线的包围盒退化成原点附近的固定小方块，
    // 平移出可视范围后图元会被场景索引剔除而「消失」。
    auto grow = [&](const Vec3& v) {
        if (!has) { r = QRectF(v.x, v.y, 0, 0); has = true; return; }
        if (v.x < r.left())   r.setLeft(v.x);
        if (v.x > r.right())  r.setRight(v.x);
        if (v.y < r.top())    r.setTop(v.y);
        if (v.y > r.bottom()) r.setBottom(v.y);
    };
    for (const auto& v : e.pts) grow(v);
    // 标注的尺寸线/界线/数字常常伸到测量点之外，包围盒必须把它们算进去，
    // 否则视图平移时标注会被场景索引提前剔除而「半截消失」。
    if (e.type == ElementType::Dimension) {
        const dim::Geom g = dim::build(e);
        if (g.valid) {
            for (const auto& s : g.ext)  { grow(s.first); grow(s.second); }
            for (const auto& s : g.line) { grow(s.first); grow(s.second); }
            for (const auto& v : g.arcPoly) grow(v);
            grow(g.textPos);
        }
    }
    // 文本 / 多行文字：按文字尺寸展开包围盒（世界 mm，与视图缩放无关），保证选中/拾取命中整段文字而非仅锚点。
    // 在固定参考像素尺寸(1000)下度量字宽/字高，再按 (文字高度mm / 1000) 换算回世界 mm。
    if ((e.type == ElementType::Text || e.type == ElementType::MText)
        && !e.pts.empty() && !e.text.isEmpty()) {
        const double th = e.radius > 0.1 ? e.radius : 3.5;   // 文字高度(mm)，存于 radius 字段
        QFont tf;
        tf.setPixelSize(1000);
        const QFontMetricsF fm(tf);
        const double lineHmm = th * (e.type == ElementType::MText ? e.lineSpacing : 1.0);
        // 宽度因子：图元 widthFactor × 样式 widthFactor
        double wf = e.widthFactor > 0 ? e.widthFactor : 1.0;
        if (model && !e.styleName.isEmpty()) {
            if (const TextStyle* ts = model->findTextStyle(e.styleName)) wf *= ts->widthFactor;
        }
        // 自动换行（仅 MText 且指定了 mtextWidth）或仅按显式换行折行
        QStringList lines;
        if (e.type == ElementType::MText && e.mtextWidth > 0.1)
            lines = wrapText(e.text, e.mtextWidth, tf);
        else
            lines = e.text.split('\n');
        double maxWmm = 0;
        for (const auto& ln : lines) {
            const double w = fm.horizontalAdvance(ln) / 1000.0 * th * wf;
            if (w > maxWmm) maxWmm = w;
        }
        const double totalHmm = lines.size() * lineHmm;
        // 9 点对齐 → 区块相对锚点的偏移（局部 y 向上）
        const int align = (e.type == ElementType::MText) ? e.textAlign : 1; // Text 维持原「中上」锚定
        const int hA = align % 3, vA = align / 3;
        const double startYv = (vA == 0) ? 0.0 : (vA == 1) ? totalHmm / 2.0 : totalHmm;
        const double yTop = startYv, yBot = startYv - totalHmm;
        double xL, xR;
        if (hA == 0)      { xL = 0;          xR = maxWmm; }
        else if (hA == 1) { xL = -maxWmm/2;  xR = maxWmm/2; }
        else              { xL = -maxWmm;    xR = 0; }
        grow(Vec3(e.pts[0].x + xL, e.pts[0].y + yBot, 0));
        grow(Vec3(e.pts[0].x + xR, e.pts[0].y + yTop, 0));
    }
    // 表格：包围盒 = 左上锚点 + (总宽, rows×rowHeight)，行向下增长（世界 Y 向上 → y 减小方向）
    if (e.type == ElementType::Table && !e.pts.empty()) {
        const double W = e.tableWidthMm();
        const double H = std::max(1, e.tableRows) * std::max(0.5, e.rowHeight);
        grow(Vec3(e.pts[0].x,      e.pts[0].y - H, 0));
        grow(Vec3(e.pts[0].x + W,  e.pts[0].y,     0));
    }
    // 块引用：几何范围来自"展开后的子图元并集"（而非插入点）。
    // 展开结果一定是基础图元（嵌套块已在 block::expandRef 内递归展开），不会自我递归。
    // 无模型（无头场景）→ 退化为插入点，保证仍可被取景。
    if (e.type == ElementType::BlockRef) {
        const std::vector<Element> subs = model ? model->expandBlockRef(e)
                                                : std::vector<Element>{};
        for (const auto& sub : subs) {
            if (sub.type == ElementType::BlockRef) continue;
            const QRectF sb = rawBounds(sub, model);
            if (!has) { r = sb; has = true; }
            else { r = r.united(sb); }
        }
        // 未定义块 / 空块：退化为插入点，保证仍可被选中
        if (!has && !e.pts.empty()) { r = QRectF(e.pts[0].x, e.pts[0].y, 0, 0); has = true; }
    }
    if (!has) r = QRectF(-1, -1, 2, 2);
    if (r.width()  < 1e-6) r.adjust(-1e-3, 0, 1e-3, 0);
    if (r.height() < 1e-6) r.adjust(0, -1e-3, 0, 1e-3);
    return r;
}

} // namespace ElementPath
} // namespace SketchCAD
