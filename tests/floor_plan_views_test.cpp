// 户型平面图模板回归（核心层 + 预览渲染）：
//   * FloorPlanViews 参数化生成：墙体（剪力墙涂黑 + 双线内墙）、门（开启弧）、
//     窗、家具图块、房间标注、四周建筑斜短线尺寸链、右侧装饰图签；
//   * 结构不变式（非法参数必须收敛而非画出错误几何）；
//   * 比例选取（A3 + 图签避让 → 1:50）与落框检查（fits）；
//   * 参数敏感性（总尺寸/门宽/开关项）。
// 预览：build/floor_plan_a3.png（人工核对用，须从工程根运行）。
#include "core/FloorPlanViews.h"
#include "core/Section.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
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
static int countType(const std::vector<Element>& v, ElementType t) {
    int n = 0;
    for (const auto& e : v) if (e.type == t) ++n;
    return n;
}

// ---------------- 预览渲染（涂黑墙体/家具灰/文字） ----------------
static void renderPreview(const std::vector<Element>& elems, const QRectF& area,
                          const QString& path) {
    const double pad = 6.0;
    const double w = area.width() + 2 * pad, h = area.height() + 2 * pad;
    const double k = 2400.0 / w;                       // 固定输出宽 ≈2400 px
    QImage img(int(w * k), int(h * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k);
    pt.translate(-(area.left() - pad), -(area.top() + area.height() + pad));
    for (const auto& e : elems) {
        QPen pen(e.color);
        pen.setWidthF(std::max(0.25, e.effectiveLineWidth() * 0.5));
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
            case ElementType::Ellipse:
                if (!e.pts.empty())
                    pt.drawEllipse(QPointF(e.pts[0].x, e.pts[0].y), e.radius,
                                   e.radiusY > 0 ? e.radiusY : e.radius);
                break;
            case ElementType::Rectangle:
                if (e.pts.size() >= 2) pt.drawRect(QRectF(QPointF(e.pts[0].x, e.pts[0].y),
                                                          QPointF(e.pts[1].x, e.pts[1].y)).normalized());
                break;
            case ElementType::Hatch: {
                if (e.pts.size() < 3) break;
                QPolygonF poly;
                for (const auto& v : e.pts) poly << QPointF(v.x, v.y);
                if (e.sectionExempt == int(SectionExempt::SmallAreaSolid)) {
                    // 剪力墙涂黑（与渲染层 SolidFill 通道一致）
                    pt.setPen(Qt::NoPen);
                    pt.setBrush(e.color);
                    pt.drawPolygon(poly);
                    pt.setBrush(Qt::NoBrush);
                } else {
                    const SectionRegion r = hatchRegionFromElement(e);
                    pt.save();
                    QPainterPath clip;
                    clip.addPolygon(poly);
                    clip.closeSubpath();
                    pt.setClipPath(clip);
                    for (const auto& seg : generateHatchLines(r))
                        pt.drawLine(QPointF(seg.a.x, seg.a.y), QPointF(seg.b.x, seg.b.y));
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

    // ==================== 默认参数：合法性 + 生成 + 落框 ====================
    {
        FloorPlanParams p = FloorPlanParams::forA3();
        check(p.validate().isEmpty(), "默认参数合法", p.validate());
        p.makeValid();
        check(p.validate().isEmpty(), "makeValid 后合法", p.validate());

        const auto R = buildFloorPlanViews(p, nullptr);
        check(!R.elems.empty(), "生成图元非空");
        check(R.fits, "平面图内容完整落在图框内",
              QString("bounds=(%1,%2 %3x%4) area=(%5,%6 %7x%8)")
                  .arg(R.bounds.left(), 0, 'f', 1).arg(R.bounds.top(), 0, 'f', 1)
                  .arg(R.bounds.width(), 0, 'f', 1).arg(R.bounds.height(), 0, 'f', 1)
                  .arg(R.area.left(), 0, 'f', 1).arg(R.area.top(), 0, 'f', 1)
                  .arg(R.area.width(), 0, 'f', 1).arg(R.area.height(), 0, 'f', 1));
        check(near(R.scale, 0.02, 1e-9) && R.scaleLabel == QStringLiteral("1:50"),
              "A3 + 图签避让 → 自动比例 1:50",
              R.scaleLabel + QString(" (%1)").arg(R.scale));

        // ---- 图元统计 ----
        check(R.wallRunCount >= 20, "实墙段充分（外墙洞口切分 + 内墙）",
              QString::number(R.wallRunCount));
        check(R.pocheCount == 14, "剪力墙涂黑区域 = 14（外墙 4 面按洞口切分）",
              QString::number(R.pocheCount));
        check(R.doorCount == 8, "门 8 樘（7 平开门 + 1 推拉门）",
              QString::number(R.doorCount));
        check(R.windowCount == 9, "窗 9 樘", QString::number(R.windowCount));
        check(R.furnitureCount >= 15, "家具图块 ≥15", QString::number(R.furnitureCount));
        check(R.dimChainCount == 8, "尺寸链 8 条（四边 × 两级）",
              QString::number(R.dimChainCount));
        check(R.dimCount >= 20, "尺寸分格 ≥20", QString::number(R.dimCount));
        check(R.labelCount == 11, "房间名标注 11 处", QString::number(R.labelCount));
        check(R.titleFieldCount >= 10, "图签栏目 ≥10", QString::number(R.titleFieldCount));

        // ---- 涂黑通道：所有 Hatch 均为 SmallAreaSolid（SolidFill）----
        int hatchBad = 0;
        for (const auto& e : R.elems)
            if (e.type == ElementType::Hatch && e.sectionExempt != int(SectionExempt::SmallAreaSolid))
                ++hatchBad;
        check(hatchBad == 0, "全部墙体填充走 SolidFill 涂黑通道");
        check(countType(R.elems, ElementType::Arc) == 7, "平开门开启弧 7 处（推拉门无弧）",
              QString::number(countType(R.elems, ElementType::Arc)));

        // ---- 文本内容 ----
        check(hasText(R.elems, QStringLiteral("客厅")) && hasText(R.elems, QStringLiteral("主卧")),
              "房间名标注存在");
        check(hasText(R.elems, QStringLiteral("9200")) && hasText(R.elems, QStringLiteral("11460")),
              "总尺寸链数字（9200 / 11460）存在");
        check(hasText(R.elems, p.drawingName), "图签图纸名称存在");
        check(hasText(R.elems, p.company), "图签设计单位存在");

        // ---- 几何：平面图纸面尺寸 = 模型 × 比例，四周留出尺寸链带宽 ----
        const double s = R.scale;
        check(near(R.planW, p.outerW * s, 1e-6) && near(R.planH, p.outerH * s, 1e-6),
              "平面图纸面尺寸 = 总尺寸 × 比例");
        check(R.bounds.width() > R.planW + 20.0 && R.bounds.width() < R.planW + 40.0 &&
              R.bounds.height() > R.planH + 20.0 && R.bounds.height() < R.planH + 40.0,
              "内容包围盒 = 平面图 + 两侧尺寸链带宽",
              QString("%1 x %2 vs plan %3 x %4")
                  .arg(R.bounds.width(), 0, 'f', 1).arg(R.bounds.height(), 0, 'f', 1)
                  .arg(R.planW, 0, 'f', 1).arg(R.planH, 0, 'f', 1));

        printf("  [INFO] 比例=%s 图元数=%d\n", R.scaleLabel.toUtf8().constData(),
               int(R.elems.size()));
        renderPreview(R.elems, R.area, QStringLiteral("build/floor_plan_a3.png"));
    }

    // ==================== 参数敏感 1：总开间增大 → 纸面宽增大 ====================
    {
        FloorPlanParams a = FloorPlanParams::forA3();
        FloorPlanParams b = a; b.outerW = 11200.0;
        const auto Ra = buildFloorPlanViews(a, nullptr);
        const auto Rb = buildFloorPlanViews(b, nullptr);
        check(Rb.planW > Ra.planW + 1e-6 && Rb.fits && Ra.fits,
              "总开间 9200→11200 → 纸面宽增大且仍落框",
              QString("%1 vs %2").arg(Rb.planW, 0, 'f', 1).arg(Ra.planW, 0, 'f', 1));
    }

    // ==================== 参数敏感 2：门宽 → 开启弧半径随之变化 ====================
    {
        FloorPlanParams a = FloorPlanParams::forA3();
        FloorPlanParams b = a; b.doorW = 1200.0;
        const auto R = buildFloorPlanViews(b, nullptr);
        double maxArcR = 0;
        for (const auto& e : R.elems)
            if (e.type == ElementType::Arc) maxArcR = std::max(maxArcR, e.radius);
        check(near(maxArcR, 1200.0 * R.scale, 1e-3),
              "最大开启弧半径 = 门宽 × 比例（门宽参数生效）",
              QString("%1 vs %2").arg(maxArcR, 0, 'f', 2).arg(1200.0 * R.scale, 0, 'f', 2));
    }

    // ==================== 参数敏感 3：开关项 ====================
    {
        FloorPlanParams q = FloorPlanParams::forA3();
        q.showDims = false; q.showFurniture = false; q.showLabels = false;
        const auto R = buildFloorPlanViews(q, nullptr);
        check(R.dimChainCount == 0 && R.dimCount == 0, "showDims=false → 无尺寸链");
        check(R.furnitureCount == 0, "showFurniture=false → 无家具图块");
        check(R.labelCount == 0, "showLabels=false → 无房间标注");
        check(R.fits, "关闭标注/家具后仍落框");
    }

    // ==================== 结构不变式：非法参数收敛 ====================
    {
        FloorPlanParams q = FloorPlanParams::forA3();
        q.outerW = 100; q.outerH = 100;             // 远低于下限
        q.shearWallT = 10; q.partWallT = 5;         // 墙厚越界
        q.doorW = 100; q.bathDoorW = 50;            // 门宽过小
        q.dimTextH = 0.1;                           // 字高过小
        q.makeValid();
        check(q.validate().isEmpty(), "极端非法参数收敛后仍合法", q.validate());
        check(q.outerW >= 1000 && q.shearWallT >= 50 && q.doorW >= 600 && q.dimTextH >= 1.2,
              "收敛后各参数回到可行域");
        const auto R = buildFloorPlanViews(q, nullptr);
        check(!R.elems.empty() && R.fits, "收敛后仍可生成并落框");
    }

    // ==================== 小幅面收敛：A5 + 图签收缩 → 自动降比例 ====================
    {
        FloorPlanParams q = FloorPlanParams::forA3();
        q.sheetW = 210.0; q.sheetH = 148.0;         // A5 竖放
        q.makeValid();
        const auto R = buildFloorPlanViews(q, nullptr);
        check(R.scale < 0.02, "小幅面自动降比例（< 1:50）",
              R.scaleLabel + QString(" (%1)").arg(R.scale));
        check(R.fits, "小幅面下内容仍落在图框内",
              QString("bounds=(%1,%2 %3x%4)")
                  .arg(R.bounds.left(), 0, 'f', 1).arg(R.bounds.top(), 0, 'f', 1)
                  .arg(R.bounds.width(), 0, 'f', 1).arg(R.bounds.height(), 0, 'f', 1));
    }

    printf("\n户型平面图测试：%s（%d 项失败）\n",
           g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
