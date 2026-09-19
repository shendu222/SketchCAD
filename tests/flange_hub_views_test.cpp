// 法兰盘轮毂视图模板回归（核心层 + 预览渲染）：
//   * 验证 FlangeHubViews 参数化生成：主视图全剖 A—A（剖面线/剖切符号/粗糙度/引线）+ 左视图圆形视图；
//   * 结构不变式（非法参数必须收敛而非画出错误几何）；
//   * 投影对齐（两视图轴线等高、几何随参数变化）；
//   * 图面合规（标准比例 + A4 内 + 不与标题栏冲突）。
// 预览：build/flange_hub_a4.png（人工核对用，须从工程根运行）。
#include "core/FlangeHubViews.h"
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
    printf("  [INFO] 预览已写出 %s（%dx%d px）\n",
           path.toUtf8().constData(), img.width(), img.height());
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);

    // ==================== 法兰盘轮毂视图模板 ====================
    {
        FlangeHubParams p = FlangeHubParams::forA4();
        check(p.validate().isEmpty(), "默认参数合法", p.validate());
        p.makeValid();
        check(p.validate().isEmpty(), "makeValid 后合法", p.validate());
        check(p.flangeD > p.hubD && p.hubD > p.cboreD && p.cboreD > p.boreD,
              "径向单调（法兰>轮毂>沉孔>通孔）");
        check(p.hubLen() > p.cboreDepth, "沉孔深不侵入法兰段");

        const auto R = buildFlangeHubViews(p, nullptr);
        check(!R.elems.empty(), "生成图元非空");
        check(R.fits, "视图完整落在可用区内",
              QString("bounds=(%1,%2 %3x%4) area=(%5,%6 %7x%8)")
                  .arg(R.bounds.left(), 0, 'f', 1).arg(R.bounds.top(), 0, 'f', 1)
                  .arg(R.bounds.width(), 0, 'f', 1).arg(R.bounds.height(), 0, 'f', 1)
                  .arg(R.area.left(), 0, 'f', 1).arg(R.area.top(), 0, 'f', 1)
                  .arg(R.area.width(), 0, 'f', 1).arg(R.area.height(), 0, 'f', 1));
        check(R.hatchCount == 2, "主视图剖面区域数 = 2（上/下两半）",
              QString::number(R.hatchCount));
        check(R.boltCircleCount == p.boltCount + p.locCount,
              "左视图孔圆总数 = 螺栓孔 + 定位孔", QString::number(R.boltCircleCount));
        check(R.dimCount >= 7, "尺寸标注充分（≥7：4 竖直 + 3 水平）",
              QString::number(R.dimCount));
        check(R.cutSymbolCount > 0, "剖切符号已生成", QString::number(R.cutSymbolCount));
        check(R.roughnessCount == 3, "粗糙度符号 3 处（孔面/法兰外圆/其余）",
              QString::number(R.roughnessCount));

        // 文本与标注内容
        check(hasText(R.elems, QStringLiteral("A—A")), "剖视图名 A—A 存在");
        check(hasText(R.elems, QStringLiteral("技术要求")), "技术要求文本存在");
        check(hasText(R.elems, QStringLiteral("其余")), "「其余」粗糙度存在");
        check(hasAnyText(R.elems, QStringLiteral("3-⌀11.5")), "螺栓孔规格引线 3-⌀11.5");
        check(hasAnyText(R.elems, QStringLiteral("C3×45°")), "轮毂倒角引线 C3×45°");
        check(hasAnyText(R.elems, QStringLiteral("R4")), "根部圆角引线 R4");

        // 几何敏感：锚点比例正确
        const double s = R.scale;
        check(near(R.frontHalfL, p.totalL * s * 0.5, 1e-3), "主视图轴向半长 = 总长·比例/2",
              QString("%1 vs %2").arg(R.frontHalfL).arg(p.totalL * s * 0.5));
        check(near(R.sideR, p.flangeD * s * 0.5, 1e-3), "左视图最大半径 = 法兰半径·比例",
              QString("%1 vs %2").arg(R.sideR).arg(p.flangeD * s * 0.5));

        printf("  [INFO] 比例=%s(%.4g) 视图数=2\n", R.scaleLabel.toUtf8().constData(), R.scale);
        renderPreview(R.elems, R.area, QStringLiteral("build/flange_hub_a4.png"));
    }

    // ==================== 参数敏感：放大法兰外径 → 左视图半径增大 ====================
    {
        FlangeHubParams a = FlangeHubParams::forA4();
        FlangeHubParams b = a; b.flangeD = 160.0;
        const auto Ra = buildFlangeHubViews(a, nullptr);
        const auto Rb = buildFlangeHubViews(b, nullptr);
        check(Rb.sideR > Ra.sideR + 1e-6, "法兰外径增大 → 左视图半径随之增大",
              QString("%1 vs %2").arg(Rb.sideR).arg(Ra.sideR));
    }

    // ==================== 结构不变式：非法参数收敛 ====================
    {
        FlangeHubParams q = FlangeHubParams::forA4();
        q.flangeD = 60; q.hubD = 80;     // 违反 法兰>轮毂
        q.boreD = 50; q.cboreD = 70;    // 违反 沉孔>通孔 且 轮毂>沉孔
        q.cboreDepth = 200;             // 沉孔深侵入法兰段
        q.chamfer = 40;                 // 倒角过大
        q.filletR = 40;                 // 圆角过大
        q.makeValid();
        check(q.validate().isEmpty(), "极端非法参数收敛后仍合法", q.validate());
        check(q.flangeD > q.hubD && q.hubD > q.cboreD && q.cboreD > q.boreD,
              "收敛后径向单调恢复");
        check(q.cboreDepth <= q.hubLen() + 1e-9, "收敛后沉孔深不侵入法兰段");
    }

    // ==================== 孔边界：螺栓孔越界时自动收回到环带内 ====================
    {
        FlangeHubParams q = FlangeHubParams::forA4();
        q.boltBCD = 90.0;   // 分布圆过大，孔将越出法兰外缘
        q.makeValid();
        check(q.validate().isEmpty(), "螺栓孔越界参数收敛后合法", q.validate());
        const double rIn = q.boreR() + 0.5;
        const double rOut = q.flangeR() - 0.5;
        check(q.boltBCD * 0.5 - q.boltD * 0.5 > rIn - 1e-9 &&
              q.boltBCD * 0.5 + q.boltD * 0.5 < rOut + 1e-9,
              "螺栓孔整体回落到 (通孔, 法兰外缘) 环带内");
    }

    printf("\n法兰盘轮毂视图测试：%s（%d 项失败）\n",
           g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
