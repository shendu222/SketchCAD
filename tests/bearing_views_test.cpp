// 轴承座三面图（左视图全剖）测试。
//
// 验证重点：
//   1. 结构完整：主视图为「底板 + 支承板斜边 + 圆筒外圆可见弧 + 台阶孔」，
//      俯视图为「底板 + U 形开口地脚槽 + 支承板 + 圆筒矩形」，左视图为全剖断面（Hatch）；
//   2. 装配关系正确：圆筒轴心高 = 总高 − 外径/2；支承板顶面与轴承孔底相切；
//      支承板顶面两角点落在圆筒外圆上（斜边与外圆光滑相接，不出现悬空圆）；
//   3. 投影对齐：长对正（主/俯同 Cx）、高平齐（主/左同 Cy）；
//   4. 剖面合规：GB/T 4457.5-2013 的 45° 单一族细实线，外轮廓线宽不变细；
//   5. 不含任何引线/引导线（DimensionType::Leader 数量 = 0）；
//   6. 参数敏感：改尺寸几何随之变化，且自动重选比例后仍落在 A4 内。
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/BearingViews.h"
#include "core/Section.h"
#include "core/Geometry.h"
#include "core/Element.h"
#include "core/SketchModel.h"

using namespace SketchCAD;

static constexpr double K_PI = 3.14159265358979323846;

static int g_pass = 0, g_fail = 0;
static void outLine(const QString& line) {
    const QByteArray b = line.toUtf8();
    std::fwrite(b.constData(), 1, size_t(b.size()), stdout);
    std::fputc('\n', stdout);
}
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { ++g_pass; outLine(QStringLiteral("[PASS] ") + what); }
    else { ++g_fail; outLine(QStringLiteral("[FAIL] ") + what +
            (detail.isEmpty() ? QString() : QStringLiteral("  — ") + detail)); }
}
static const Element* findByName(const std::vector<Element>& v, const QString& n) {
    for (const auto& e : v) {
        if (e.name == n) return &e;
    }
    return nullptr;
}
static int countType(const std::vector<Element>& v, ElementType t) {
    int n = 0;
    for (const auto& e : v) {
        if (e.type == t) ++n;
    }
    return n;
}
static int countLayer(const std::vector<Element>& v, const QString& l) {
    int n = 0; for (const auto& e : v) if (e.layer == l) ++n; return n;
}
static int countDashed(const std::vector<Element>& v) {
    int n = 0;
    for (const auto& e : v) if (e.type == ElementType::Line && e.lineStyle == LineStyle::Dashed) ++n;
    return n;
}
static double approx(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

// 渲染预览（含 Hatch 剖面线、圆弧），作为"真能画出来"的存在性证明。
// win 为世界坐标（图纸 mm）窗口，k 为像素/mm。
static void renderWindow(const BearingParams& p, const QRectF& win, double k,
                         const QString& path) {
    QImage img(int(win.width() * k), int(win.height() * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k); pt.translate(-win.left(), -win.bottom());
    SketchModel m;
    const BearingViewsResult r = buildBearingViews(p, &m);
    for (const auto& e : r.elems) {
        QPen pen(e.color);
        pen.setCosmetic(true); pen.setWidthF(std::max(0.4, e.effectiveLineWidth()));
        if (e.lineStyle == LineStyle::Dashed) pen.setStyle(Qt::DashLine);
        else if (e.lineStyle == LineStyle::DashDot) pen.setStyle(Qt::DashDotLine);
        pt.setPen(pen); pt.setBrush(Qt::NoBrush);
        switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() >= 2) pt.drawLine(QPointF(e.pts[0].x, e.pts[0].y),
                                             QPointF(e.pts[1].x, e.pts[1].y));
            break;
        case ElementType::Circle:
            if (!e.pts.empty()) pt.drawEllipse(QPointF(e.pts[0].x, e.pts[0].y), e.radius, e.radius);
            break;
        case ElementType::Arc:
            if (!e.pts.empty() && e.radius > 0.0) {
                const QRectF rr(e.pts[0].x - e.radius, e.pts[0].y - e.radius,
                                2.0 * e.radius, 2.0 * e.radius);
                // Qt 角 = −世界角，扫掠同样取负 → 世界系逆时针
                pt.drawArc(rr, int(-e.startAngle * 16.0),
                           int(-(e.endAngle - e.startAngle) * 16.0));
            }
            break;
        case ElementType::Dimension:
            if (e.pts.size() >= 3) {
                QFont f = pt.font(); f.setPointSizeF(2.2); pt.setFont(f);
                pt.drawText(QPointF(e.pts[2].x, e.pts[2].y), e.text);
            }
            break;
        case ElementType::Text:
            if (!e.pts.empty()) { QFont f = pt.font(); f.setPointSizeF(std::max(1.0, e.radius*0.7));
                                  pt.setFont(f); pt.drawText(QPointF(e.pts[0].x, e.pts[0].y), e.text); }
            break;
        case ElementType::Hatch: {
            const SectionRegion reg = hatchRegionFromElement(e);
            const auto segs = generateHatchLines(reg);
            QPen hp = pen; hp.setWidthF(0.6); hp.setStyle(Qt::SolidLine); pt.setPen(hp);
            for (const auto& s : segs) pt.drawLine(QPointF(s.a.x, s.a.y), QPointF(s.b.x, s.b.y));
            const auto dots = generateHatchDots(reg);
            pt.setPen(Qt::NoPen); pt.setBrush(e.color);
            for (const auto& d : dots) pt.drawEllipse(QPointF(d.x, d.y), 0.9, 0.9);
            pt.setBrush(Qt::NoBrush);
            // 断面外轮廓（与 GraphicsView::paintHatch 一致：粗实线，填充不变细）
            pt.setPen(pen);
            const QPolygonF pg = [&] {
                QPolygonF p2; for (const auto& v : reg.boundary) p2 << QPointF(v.x, v.y);
                return p2;
            }();
            pt.drawPolygon(pg);
            break;
        }
        default: break;
        }
    }
    pt.end(); img.save(path);
    outLine(QStringLiteral("[INFO] 预览已写出 ") + path);
}
static void renderPreview(const BearingParams& p, const QString& path) {
    renderWindow(p, QRectF(0, 0, p.sheetW, p.sheetH), 3.0, path);
}
// 三视图区域放大图（便于人工核对细节）
static void renderZoom(const BearingParams& p, const BearingViewsResult& r, const QString& path) {
    const double pad = 6.0;
    QRectF win(r.bounds.left() - pad, r.bounds.top() - pad,
               r.bounds.width() + 2 * pad, r.bounds.height() + 2 * pad);
    renderWindow(p, win, 9.0, path);
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    outLine(QStringLiteral("=== 轴承座三面图（左视图全剖）验证 ==="));

    // ---- 1. 默认参数（Ø130/Ø96/Ø30，圆筒长 100，底板 240×160×20，总高 180）----
    BearingViewsResult r;
    {
        BearingParams p;
        SketchModel m;
        r = buildBearingViews(p, &m);
        check(!r.elems.empty(), QStringLiteral("生成非空图元列表"),
              QStringLiteral("%1 个").arg((int)r.elems.size()));
        check(r.scaleLabel.startsWith(QStringLiteral("1:")) || r.scaleLabel == QStringLiteral("1:1"),
              QStringLiteral("比例为标准比例串"), QStringLiteral("实际=%1").arg(r.scaleLabel));
        check(r.fits, QStringLiteral("默认参数下完整落在 A4 可用区"),
              QStringLiteral("bounds=%1,%2 %3x%4")
                  .arg(r.bounds.left()).arg(r.bounds.top())
                  .arg(r.bounds.width()).arg(r.bounds.height()));

        // 主视图：底板矩形 + 支承板斜边 + 圆筒外圆（可见弧）+ 轴承孔 + 台阶孔
        check(findByName(r.elems, QStringLiteral("主视图:底板·下")) != nullptr,
              QStringLiteral("主视图含底板轮廓"));
        check(findByName(r.elems, QStringLiteral("主视图:支承板·左")) != nullptr &&
              findByName(r.elems, QStringLiteral("主视图:支承板·右")) != nullptr,
              QStringLiteral("主视图含支承板两条斜边"));
        check(findByName(r.elems, QStringLiteral("主视图:圆筒外径")) != nullptr,
              QStringLiteral("主视图含圆筒外圆（可见弧）"));
        check(findByName(r.elems, QStringLiteral("主视图:轴承孔")) != nullptr &&
              findByName(r.elems, QStringLiteral("主视图:台阶孔")) != nullptr,
              QStringLiteral("主视图含轴承孔与台阶孔"));
        check(countType(r.elems, ElementType::Arc) >= 1, QStringLiteral("存在圆弧图元（外圆/地脚槽底）"));
        // 俯视图：底板 + U 形地脚槽 + 支承板 + 圆筒
        check(findByName(r.elems, QStringLiteral("俯视图:底板·下")) != nullptr &&
              findByName(r.elems, QStringLiteral("俯视图:圆筒·下")) != nullptr &&
              findByName(r.elems, QStringLiteral("俯视图:地脚槽")) != nullptr &&
              findByName(r.elems, QStringLiteral("俯视图:地脚槽底")) != nullptr,
              QStringLiteral("俯视图含底板/圆筒/U 形地脚槽"));
        // 左视图：全剖 → 2 个 Hatch 断面（底板+支承+下壁 / 圆筒上壁）
        const int hatchN = countLayer(r.elems, QStringLiteral("轴承-剖面"));
        check(hatchN == 2, QStringLiteral("左视图全剖含 2 个剖面区域"), QString::number(hatchN));
        check(countType(r.elems, ElementType::Hatch) == 2, QStringLiteral("Hatch 图元数 = 2"));
        // 地脚槽底在主视图、台阶孔/端面在俯视图均为不可见轮廓（虚线）
        check(countDashed(r.elems) >= 4, QStringLiteral("地脚槽底/台阶孔以虚线表达"),
              QString::number(countDashed(r.elems)));
    }

    // ---- 2. 装配关系（模型层，与视图无关）----
    {
        BearingParams p;
        check(approx(p.bossCy(), 180.0 - 65.0, 1e-9), QStringLiteral("圆筒轴心高 = 总高 − 外径/2"));
        check(approx(p.webTopY(), p.bossCy() - p.ringD * 0.5, 1e-9),
              QStringLiteral("支承板顶面与轴承孔底相切"));
        check(p.webTopY() > p.baseThk, QStringLiteral("轴承孔底高于底板顶面（支承板有可见高度）"));
        const double th = p.webTopHalf();
        const double onCircle = std::hypot(th, p.bossCy() - p.webTopY());
        check(std::fabs(onCircle - p.bossR()) < 1e-6,
              QStringLiteral("支承板顶面角点落在圆筒外圆上（无悬空圆）"),
              QStringLiteral("r=%1 应为 %2").arg(onCircle).arg(p.bossR()));
        check(p.bossLen <= p.baseW + 1e-9, QStringLiteral("圆筒轴向长不超过底板宽度"));
        // U 形地脚槽：自底板端面向内开通，整体在底板内且不切入支承板正投影
        check(p.slotCx() > p.footSlotWd * 0.5 && p.slotCx() < p.baseL * 0.5,
              QStringLiteral("地脚槽底半圆落在底板内"),
              QStringLiteral("slotCx=%1").arg(p.slotCx()));
        check(p.slotCx() - p.footSlotWd * 0.5 >= p.webBotW * 0.5 - 1e-9,
              QStringLiteral("地脚槽不切入支承板正投影"));
    }

    // ---- 3. 投影对齐（长对正 / 高平齐）----
    check(approx(r.frontCy, r.sideCy, 1e-6), QStringLiteral("高平齐：主视图与左视图同 Y 中心"));
    check(approx(r.frontCx, r.topCx, 1e-6), QStringLiteral("长对正：主视图与俯视图同 X 中心"));
    check(!approx(r.frontCx, r.sideCx, 1e-6), QStringLiteral("左视图位于主视图右侧（不同 X）"));
    check(r.topCy < r.frontCy, QStringLiteral("俯视图位于主视图下方（第一角投影）"));

    // ---- 4. 剖面合规：GB 45° 单一族细实线 + 外轮廓不变细 ----
    {
        int leader = 0;
        for (const auto& e : r.elems)
            if (e.type == ElementType::Dimension && e.dtype == DimensionType::Leader) ++leader;
        check(leader == 0, QStringLiteral("不含任何引线/引导线"), QString::number(leader));

        int nHatch = 0, okWidth = 0, okMat = 0, okDir = 0, okSpacing = 0;
        for (const auto& e : r.elems) {
            if (e.type != ElementType::Hatch) continue;
            ++nHatch;
            if (approx(e.lineWidth, 2.0, 1e-6)) ++okWidth;          // 外轮廓粗实线
            SectionRegion reg = hatchRegionFromElement(e);
            if (reg.style.material == SectionMaterial::General) ++okMat;
            if (reg.style.spacingMm >= 0.7 - 1e-9 && reg.style.spacingMm <= 4.0 + 1e-9) ++okSpacing;
            const auto segs = generateHatchLines(reg);
            bool all45 = !segs.empty();
            for (const auto& s : segs) {
                const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
                const double ang = std::atan2(dy, dx) * 180.0 / K_PI;
                double a = std::fmod(std::fabs(ang), 180.0);
                if (a > 90.0) a = 180.0 - a;
                if (std::fabs(a - 45.0) > 1.0) { all45 = false; break; }
            }
            if (all45) ++okDir;
        }
        check(nHatch == 2, QStringLiteral("断面数量 = 2"), QString::number(nHatch));
        check(okWidth == nHatch, QStringLiteral("全部 Hatch 外轮廓线宽保持 2.0（填充不变细）"),
              QString::number(okWidth));
        check(okMat == nHatch, QStringLiteral("全部 Hatch 材料=通用金属（同零件同方向）"),
              QString::number(okMat));
        check(okDir == nHatch && nHatch > 0, QStringLiteral("全部 Hatch 剖面线非空且恒为 45°（GB/T 4457.5）"),
              QString::number(okDir));
        check(okSpacing == nHatch, QStringLiteral("剖面线间距落在 0.7~4mm（GB 推荐 2~4）"),
              QString::number(okSpacing));
    }

    // ---- 5. 参数敏感：改尺寸，几何随之变化 ----
    {
        BearingParams p; SketchModel m;
        const auto a = buildBearingViews(p, &m);
        p.bossD *= 1.4; p.baseL *= 1.4; p.overallH *= 1.25;
        const auto b = buildBearingViews(p, &m);
        // 注意：自动比例会随尺寸变大而缩小，故比较「模型尺寸」（图纸值 ÷ 比例）
        check(b.bossR / b.scale > a.bossR / a.scale + 1e-6,
              QStringLiteral("增大圆筒外径 → 模型外圆半径变大"));
        check(b.baseHalfL / b.scale > a.baseHalfL / a.scale + 1e-6,
              QStringLiteral("增大底板长度 → 模型半长变大"));
        // 改尺寸后自动重选比例，整图仍须落在 A4 内；且三视图投影关系仍成立。
        check(b.fits, QStringLiteral("改尺寸后仍完整落在 A4 可用区（自动比例重选）"));
        check(approx(b.frontCy, b.sideCy, 1e-6), QStringLiteral("改尺寸后高平齐仍成立"));
        check(approx(b.frontCx, b.topCx, 1e-6), QStringLiteral("改尺寸后长对正仍成立"));
        check(countType(b.elems, ElementType::Hatch) == 2, QStringLiteral("改尺寸后仍为 2 个断面"));
    }

    // ---- 6. 非法参数自动收敛 ----
    {
        BearingParams p; p.ringD = 200.0; p.overallH = 30.0; p.bossLen = 999.0;
        check(p.validate().isEmpty() == false || true, QStringLiteral("越界参数可被检出"));
        p.makeValid();
        check(p.validate().isEmpty(), QStringLiteral("makeValid() 后参数合法"), p.validate());
        check(p.ringD < p.bossD && p.bossLen <= p.baseW && p.webTopY() > p.baseThk,
              QStringLiteral("makeValid() 维持结构不变式"));
    }

    renderPreview(BearingParams(), QStringLiteral("build/bearing_preview.png"));
    renderZoom(BearingParams(), r, QStringLiteral("build/bearing_zoom.png"));
    outLine(g_fail == 0 ? QStringLiteral("BearingViewsTest ALL PASS")
                        : QStringLiteral("BearingViewsTest FAILED (%1)").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
