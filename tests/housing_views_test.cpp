// 箱体附图能力回归（核心层 + 预览渲染）：
//   * SurfaceRoughness：GB/T 131-2006 粗糙度符号（去除/不去除材料、完整符号、比例派生）；
//   * SectionCutSymbol：剖切符号（位置粗短线 + 箭头 + 字母）与剖视图名 A—A；
//   * SectionDrawing：SectionViewBuilder 参数化剖视图 + makeRingSection 环形断面；
//   * HousingViews：箱体视图模板（主视图全剖 / 俯视图 B / A—A 断面 / C 向视图）。
// 预览：build/housing_views_a3.png（人工核对用，须从工程根运行）。
#include "core/SurfaceRoughness.h"
#include "core/SectionCutSymbol.h"
#include "core/SectionDrawing.h"
#include "core/HousingViews.h"
#include "core/Section.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QFont>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) printf("  [PASS] %s\n", what);
    else { printf("  [FAIL] %s %s\n", what,
                  detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double e = 1e-6) { return std::fabs(a - b) <= e; }
static int countType(const std::vector<Element>& v, ElementType t) {
    int n = 0;
    for (const auto& e : v) if (e.type == t) ++n;
    return n;
}
static bool hasText(const std::vector<Element>& v, const QString& t) {
    for (const auto& e : v) if (e.type == ElementType::Text && e.text == t) return true;
    return false;
}
// 引线/直径标注以 Dimension（dtype=Leader/...）承载文本，单独扫描
static bool hasAnyText(const std::vector<Element>& v, const QString& t) {
    for (const auto& e : v)
        if ((e.type == ElementType::Text || e.type == ElementType::Dimension) && e.text == t)
            return true;
    return false;
}
static bool hasDimText(const std::vector<Element>& v, const QString& t) {
    for (const auto& e : v) if (e.type == ElementType::Dimension && e.text == t) return true;
    return false;
}

// ---------------- 预览渲染（含剖面线与标注的简化绘制） ----------------
static void renderPreview(const std::vector<Element>& elems, const QRectF& area,
                          const QString& path) {
    const double pad = 12.0;
    const double w = area.width() + 2 * pad, h = area.height() + 2 * pad;
    const double k = 1400.0 / w;                       // 固定输出宽 ≈1400 px
    QImage img(int(w * k), int(h * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k);
    pt.translate(-(area.left() - pad), -(area.top() + area.height() + pad));
    for (const auto& e : elems) {
        QPen pen(e.color);
        pen.setWidthF(std::max(0.4, e.effectiveLineWidth()) * 0.7);
        if (e.lineStyle == LineStyle::Dashed)            pen.setStyle(Qt::DashLine);
        else if (e.lineStyle == LineStyle::DashDot)      pen.setStyle(Qt::DashDotLine);
        else if (e.lineStyle == LineStyle::Dotted)       pen.setStyle(Qt::DotLine);
        pt.setPen(pen);
        pt.setBrush(Qt::NoBrush);
        switch (e.type) {
            case ElementType::Line:
                if (e.pts.size() >= 2) pt.drawLine(QPointF(e.pts[0].x, e.pts[0].y),
                                                   QPointF(e.pts[1].x, e.pts[1].y));
                break;
            case ElementType::Circle:
                if (!e.pts.empty()) pt.drawEllipse(QPointF(e.pts[0].x, e.pts[0].y),
                                                   e.radius, e.radius);
                break;
            case ElementType::Arc: {
                if (e.pts.empty()) break;
                // Qt 角 = −世界角（程序约定，见 GraphicsView）
                const double span = -(e.endAngle - e.startAngle);
                QRectF rect(e.pts[0].x - e.radius, e.pts[0].y - e.radius,
                            e.radius * 2, e.radius * 2);
                pt.drawArc(rect, int(-e.startAngle * 16), int(span * 16));
                break;
            }
            case ElementType::Rectangle:
                if (e.pts.size() >= 2) pt.drawRect(QRectF(QPointF(e.pts[0].x, e.pts[0].y),
                                                          QPointF(e.pts[1].x, e.pts[1].y)).normalized());
                break;
            case ElementType::Hatch: {
                // 真实剖面线（复用剖面引擎裁剪）
                const SectionRegion r = hatchRegionFromElement(e);
                for (const auto& seg : generateHatchLines(r))
                    pt.drawLine(QPointF(seg.a.x, seg.a.y), QPointF(seg.b.x, seg.b.y));
                break;
            }
            case ElementType::Dimension: {
                for (size_t i = 1; i < e.pts.size(); ++i)
                    pt.drawLine(QPointF(e.pts[i-1].x, e.pts[i-1].y),
                                QPointF(e.pts[i].x, e.pts[i].y));
                if (!e.pts.empty()) {
                    QFont fnt = pt.font();
                    fnt.setPointSizeF(std::max(2.0, (e.dimTextHeight > 0 ? e.dimTextHeight : 3.5) * 0.8));
                    pt.setFont(fnt);
                    pt.save();
                    pt.translate(e.pts.back().x, e.pts.back().y);
                    pt.scale(1, -1);
                    pt.drawText(QPointF(0, 0), e.text);
                    pt.restore();
                }
                break;
            }
            case ElementType::Text: {
                if (e.pts.empty()) break;
                QFont fnt = pt.font();
                fnt.setPointSizeF(std::max(2.0, e.radius * 0.8));
                pt.setFont(fnt);
                pt.save();
                pt.translate(e.pts[0].x, e.pts[0].y);
                pt.scale(1, -1);
                pt.drawText(QPointF(0, 0), e.text);
                pt.restore();
                break;
            }
            default: break;
        }
    }
    pt.end();
    img.save(path);
    printf("  [INFO] 箱体视图预览已写出 %s（%dx%d px）\n",
           path.toUtf8().constData(), img.width(), img.height());
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);

    // ==================== 1. SurfaceRoughness（GB/T 131-2006）====================
    {
        RoughnessSpec s;
        s.anchor = Vec3(0, 0, 0);
        s.textHeight = 3.5;
        s.value = QStringLiteral("Ra 1.6");
        auto v = buildRoughnessSymbol(s);
        // 完整符号：左腿 + 右腿(加长) + 闭合横线 + 数值文本 = 3 线 + 1 文本
        check(countType(v, ElementType::Line) == 3, "完整符号 3 条细实线",
              QString::number(countType(v, ElementType::Line)));
        check(countType(v, ElementType::Text) == 1 && hasText(v, QStringLiteral("Ra 1.6")),
              "数值文本 Ra 1.6 存在");
        // 左腿长 = 1.4h（比例派生）
        bool legOk = false;
        for (const auto& e : v)
            if (e.type == ElementType::Line && near(e.pts[0].x, 0.0) && near(e.pts[0].y, 0.0))
                if (near(std::hypot(e.pts[1].x - 0.0, e.pts[1].y - 0.0), 1.4 * 3.5, 1e-4))
                    legOk = true;
        check(legOk, "短腿长 = 1.4×字高（GB 比例）");
        // 去除材料：存在水平闭合线（两端 y 相同）
        bool closed = false;
        for (const auto& e : v)
            if (e.type == ElementType::Line && near(e.pts[0].y, e.pts[1].y)) closed = true;
        check(closed, "去除材料符号顶部闭合");

        RoughnessSpec s2;
        s2.anchor = Vec3(0, 0, 0);
        s2.removeMaterial = false;               // 不去除材料
        auto v2 = buildRoughnessSymbol(s2);
        check(countType(v2, ElementType::Circle) == 1, "不去除材料符号尖端带小圆");
    }

    // ==================== 2. SectionCutSymbol（GB/T 17452 / 4458.6）====================
    {
        CutSymbolSpec s;
        s.markPoint = Vec3(0, 0, 0);
        s.viewDir = CutViewDir::Down;
        s.letter = QStringLiteral("A");
        auto v = buildCutSymbolEnd(s);
        // 位置粗短线 1 + 箭杆 1 + 箭头翼 2 = 4 线 + 1 字母（GB/T 4458.6 单端剖切符号）
        check(countType(v, ElementType::Line) == 4, "单端剖切符号 4 条线（位置线+箭杆+两翼）",
              QString::number(countType(v, ElementType::Line)));
        check(countType(v, ElementType::Text) == 1, "单端剖切符号含字母 A");
        bool thickMark = false;
        for (const auto& e : v)
            if (e.type == ElementType::Line && e.lineWidth >= 2.0) thickMark = true;
        check(thickMark, "剖切位置线为粗实线（≥2px）");

        auto both = buildCutAnnotation(Vec3(-50, 0, 0), Vec3(50, 0, 0),
                                       CutViewDir::Down, QStringLiteral("A"), 5.0);
        check(countType(both, ElementType::Line) == 8, "两端剖切符号 8 条线",
              QString::number(countType(both, ElementType::Line)));
        auto label = buildSectionViewLabel(Vec3(0, 10, 0), QStringLiteral("A"), 5.0);
        check(label.type == ElementType::Text && label.text == QStringLiteral("A—A"),
              "剖视图名 A—A 正确");
    }

    // ==================== 3. SectionDrawing（SectionViewBuilder + 环形断面）====================
    {
        SectionViewBuilder sb(QStringLiteral("测试"));
        sb.addCutRect(Vec3(0, 0, 0), Vec3(30, 10, 0), true);   // 矩形断面
        sb.addVisibleLine(Vec3(-5, 5, 0), Vec3(0, 5, 0));
        sb.addHiddenLine(Vec3(0, -2, 0), Vec3(30, -2, 0));
        sb.addCenterLine(Vec3(15, -4, 0), Vec3(15, 14, 0));
        sb.addDim(DimensionType::Horizontal, Vec3(0, 0, 0), Vec3(30, 0, 0),
                  Vec3(15, -6, 0), QStringLiteral("30"));
        const auto r = sb.build();
        int hatch = 0, hidden = 0, center = 0;
        for (const auto& e : r.elems) {
            if (e.type == ElementType::Hatch) ++hatch;
            if (e.lineStyle == LineStyle::Dashed) ++hidden;
            if (e.lineStyle == LineStyle::DashDot) ++center;
        }
        check(hatch == 1, "Builder 生成 1 个剖面区域", QString::number(hatch));
        check(r.hatchCount == 1 && r.dimCount == 1, "结果统计正确");
        check(hidden >= 1 && center >= 1, "虚线/中心线图层区分正确");
        // 剖面线确实落在矩形内（取一条线段中点验证）
        bool inRect = false;
        for (const auto& e : r.elems)
            if (e.type == ElementType::Hatch) {
                const auto segs = generateHatchLines(hatchRegionFromElement(e));
                check(!segs.empty(), "矩形断面生成剖面线段");
                if (!segs.empty()) {
                    const auto& s0 = segs[0];
                    const double mx = (s0.a.x + s0.b.x) / 2, my = (s0.a.y + s0.b.y) / 2;
                    inRect = mx > 0 && mx < 30 && my > 0 && my < 10;
                }
            }
        check(inRect, "剖面线段落在剖面区域内");

        // 环形断面（附图 A—A 同型）
        RingSectionSpec rs;
        rs.center = Vec3(100, 100, 0);
        rs.outerD = 82.0;
        rs.innerD = 64.0;
        rs.wallHoles = { {270.0, 7.0}, {150.0, 7.0}, {30.0, 7.0} };
        rs.outerDimText = QStringLiteral("⌀82");
        rs.innerDimText = QStringLiteral("⌀64H8");
        SectionViewResult rr;
        auto ring = makeRingSection(rs, &rr);
        check(countType(ring, ElementType::Circle) == 2 + 3, "环形断面：内外圆 + 3 壁孔圆",
              QString::number(countType(ring, ElementType::Circle)));
        check(countType(ring, ElementType::Hatch) == 1, "环形断面 1 个剖面区域");
        bool holeAvoided = true;
        for (const auto& e : ring)
            if (e.type == ElementType::Hatch) {
                const auto segs = generateHatchLines(hatchRegionFromElement(e));
                check(!segs.empty(), "环形断面生成剖面线段");
                // 剖面线不得穿入壁孔内部：沿线段采样，任一点落入「孔半径−0.5」圆内即失败。
                // 注意壁环本身是实体、环中线（rMid）处有剖面线属正常，故只判孔内。
                const double rMid = (41.0 + 32.0) * 0.5;
                for (const auto& hole : rs.wallHoles) {
                    const double a = hole.angleDeg * 3.14159265358979323846 / 180.0;
                    const Vec3 hc(100.0 + rMid * std::cos(a), 100.0 + rMid * std::sin(a), 0);
                    const double rIn = hole.d * 0.5 - 0.5;
                    for (const auto& seg : segs) {
                        for (int k = 0; k <= 20; ++k) {
                            const double t = k / 20.0;
                            const double px = seg.a.x + t * (seg.b.x - seg.a.x);
                            const double py = seg.a.y + t * (seg.b.y - seg.a.y);
                            if (std::hypot(px - hc.x, py - hc.y) < rIn) holeAvoided = false;
                        }
                    }
                }
            }
        check(holeAvoided, "剖面线避让壁孔（3×Ø7）");
        check(rr.hatchCount == 1 && rr.dimCount == 2, "环形断面统计（1 剖面 / 2 尺寸）");
        check(hasDimText(ring, QStringLiteral("⌀64H8")), "内径公差标注 ⌀64H8");
    }

    // ==================== 4. HousingViews（箱体视图模板）====================
    {
        HousingParams p = HousingParams::forA3();
        check(p.validate().isEmpty(), "默认参数合法", p.validate());
        p.makeValid();
        check(near(p.wallThk(), 9.0) && p.wallHoleD <= p.wallThk() - 1.0,
              "壁厚 9mm、壁孔不越界");

        const auto R = buildHousingViews(p, nullptr);
        check(!R.elems.empty(), "生成图元非空");
        check(R.fits, "视图完整落在可用区内",
              QString("bounds=%1,%2 %3x%4 area=%5,%6 %7x%8")
                  .arg(R.bounds.left()).arg(R.bounds.top())
                  .arg(R.bounds.width()).arg(R.bounds.height())
                  .arg(R.area.left()).arg(R.area.top())
                  .arg(R.area.width()).arg(R.area.height()));
        check(R.hatchCount >= 2, "主视图 + A—A 至少 2 个剖面区域",
              QString::number(R.hatchCount));
        check(R.cutSymbolCount > 0, "剖切符号已生成");
        check(R.roughnessCount == 3, "粗糙度符号 3 处（腔底/筒顶/其余）",
              QString::number(R.roughnessCount));
        check(R.dimCount >= 6, "尺寸标注充分（≥6）", QString::number(R.dimCount));
        check(hasText(R.elems, QStringLiteral("A—A")), "A—A 断面图名存在");
        check(hasText(R.elems, QStringLiteral("技术要求")), "技术要求文本存在");
        check(hasText(R.elems, QStringLiteral("其余")), "「其余」粗糙度存在");
        check(hasAnyText(R.elems, QStringLiteral("4×M4-7H")), "螺孔规格引线 4×M4-7H");
        check(hasAnyText(R.elems, QStringLiteral("3×Ø7")), "壁孔规格引线 3×Ø7");
        // 参数变化仍满足不变式
        HousingParams q = p;
        q.outerD = 60; q.boreD = 50; q.wallHoleD = 5; q.makeValid();
        check(q.validate().isEmpty(), "参数收敛后仍合法", q.validate());

        printf("  [INFO] 比例=%s(%.4g) bounds=(%.1f,%.1f %.1fx%.1f) area=(%.1f,%.1f %.1fx%.1f)\n",
               R.scaleLabel.toUtf8().constData(), R.scale,
               R.bounds.left(), R.bounds.top(), R.bounds.width(), R.bounds.height(),
               R.area.left(), R.area.top(), R.area.width(), R.area.height());
        renderPreview(R.elems, R.area, QStringLiteral("build/housing_views_a3.png"));
    }

    printf("\n箱体附图能力测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
