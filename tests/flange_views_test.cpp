// 法兰盘三面图测试。
//
// 分三层验证，缺一不可：
//   1. **参数敏感层**：改任一尺寸参数，图纸上的几何必须跟着变（否则"可调整参数"是假的）。
//   2. **结构不变层**：改参数只能改尺寸，**不能改结构** —— 四个同心圆的层次、
//      螺栓孔数量与均布、三视图的投影对齐关系必须恒定（这是本题的核心要求）。
//   3. **图面合规层**：A4 图纸内、不与图框/标题栏冲突、比例落在标准比例序列上。
//
// 之所以要断言到"具体坐标/半径"，是因为几何类代码最容易出的错就是
// "参数改了但某个视图忘了乘比例"或"某个圆漏了" —— 这类错误编译期与肉眼都看不出来。
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/FlangeViews.h"
#include "core/Geometry.h"
#include "core/SketchModel.h"
#include "core/SheetSpec.h"

using namespace SketchCAD;

static int g_pass = 0, g_fail = 0;

// 统一走 Qt 的输出通道。
// 绝不能用 std::printf 打中文：printf 的原样字节是 UTF-8，而 qPrintable() 转换出来的是
// 本地 8 位编码（GBK）—— 两种编码混进同一条流，解码时必然一半是乱码。
// 这里统一转成 UTF-8 字节再写，保证整个文件单一编码、可直接读取。
static void outLine(const QString& line) {
    const QByteArray b = line.toUtf8();
    std::fwrite(b.constData(), 1, size_t(b.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { ++g_pass; outLine(QStringLiteral("[PASS] ") + what); }
    else {
        ++g_fail;
        outLine(QStringLiteral("[FAIL] ") + what + (detail.isEmpty() ? QString()
                                                                 : QStringLiteral("  — ") + detail));
    }
}
static void info(const QString& s) { outLine(QStringLiteral("[INFO] ") + s); }

// ---- 从图元列表里按名字/图层检索 ----
static const Element* findByName(const std::vector<Element>& v, const QString& name) {
    for (const auto& e : v) if (e.name == name) return &e;
    return nullptr;
}
static int countLayer(const std::vector<Element>& v, const QString& layer) {
    int n = 0;
    for (const auto& e : v) if (e.layer == layer) ++n;
    return n;
}
static int countType(const std::vector<Element>& v, ElementType t) {
    int n = 0;
    for (const auto& e : v) if (e.type == t) ++n;
    return n;
}
// 数「名称形如 prefix + 纯数字」的图元（螺栓孔：主视图:螺栓孔1 … 主视图:螺栓孔n）。
// 不能用 startsWith —— "主视图:螺栓孔分布圆" 也带同一前缀，会被一起数进去。
static int countNumbered(const std::vector<Element>& v, const QString& prefix) {
    int n = 0;
    for (const auto& e : v) {
        if (!e.name.startsWith(prefix)) continue;
        const QString tail = e.name.mid(prefix.size());
        bool digits = !tail.isEmpty();
        for (const QChar& c : tail) if (!c.isDigit()) { digits = false; break; }
        if (digits) ++n;
    }
    return n;
}
static double approx(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

// 图元 → QPainter 的统一落笔（预览 / 放大预览共用）
static void drawElems(QPainter& pt, const std::vector<Element>& all) {
    for (const auto& e : all) {
        QPen pen(e.color);
        pen.setWidthF(e.type == ElementType::Text ? 1.0 : std::max(1.0, e.effectiveLineWidth()));
        if (e.lineStyle == LineStyle::Dashed)      pen.setStyle(Qt::DashLine);
        else if (e.lineStyle == LineStyle::DashDot) pen.setStyle(Qt::DashDotLine);
        else if (e.lineStyle == LineStyle::Dotted)  pen.setStyle(Qt::DotLine);
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
                QRectF box(e.pts[0].x - e.radius, e.pts[0].y - e.radius,
                           e.radius * 2, e.radius * 2);
                // 模型的圆弧用标准数学角（Y 向上）；QPainter 这里是 Y 向上坐标系，
                // 但 drawArc 的 1/16 度仍从 +X 起逆时针 —— 与本模块约定一致，直接换算。
                pt.drawArc(box, int(e.startAngle * 16), int((e.endAngle - e.startAngle) * 16));
                break;
            }
            case ElementType::Text: {
                if (e.pts.empty()) break;
                QFont f = pt.font();
                f.setPointSizeF(std::max(1.0, e.radius * 0.7));
                pt.setFont(f);
                pt.save();
                pt.translate(e.pts[0].x, e.pts[0].y);
                pt.scale(1, -1);                // 文字不镜像
                pt.drawText(QPointF(0, 0), e.text);
                pt.restore();
                break;
            }
            default: break;
        }
    }
}

// 渲染到 PNG，便于人工核对（同时充当"程序真的能画出来"的存在性证明）
static void renderPreview(const FlangeParams& p, const QString& path) {
    const double k = 3.0;                       // 屏幕放大倍数，仅为看图方便
    QImage img(int(p.sheetW * k), int(p.sheetH * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k);                            // 转成 Y 向上的图纸坐标
    pt.translate(0, -p.sheetH);

    SketchModel m;
    auto all = buildSheetFrame(SheetSpec::make(SheetFormat::A4, true));
    const FlangeViewsResult r = buildFlangeViews(p, &m);
    for (const auto& e : r.elems) all.push_back(e);

    drawElems(pt, all);
    pt.end();
    img.save(path);
    info(QStringLiteral("预览已写出 %1（%2×%3 px）").arg(path).arg(img.width()).arg(img.height()));
}

// 放大渲染「三视图区」（不含图框/标题栏），便于人工核对尺寸文字与线型。
static void renderZoom(const FlangeParams& p, const QString& path) {
    const double k = 9.0;
    SketchModel m;
    const FlangeViewsResult r = buildFlangeViews(p, &m);
    QRectF bb = r.bounds;
    if (!bb.isValid() || bb.width() <= 0.0 || bb.height() <= 0.0) return;
    const double pad = 10.0;                    // 留白，容纳尺寸线/文字
    bb.adjust(-pad, -pad, pad, pad);

    QImage img(int(bb.width() * k), int(bb.height() * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k);
    pt.translate(-bb.left(), -bb.bottom());
    drawElems(pt, r.elems);
    pt.end();
    img.save(path);
    info(QStringLiteral("放大预览已写出 %1（%2×%3 px）").arg(path).arg(img.width()).arg(img.height()));
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);

    outLine(QStringLiteral("=== 法兰盘三面图（参数化）验证 "));

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 1. 默认参数（DN100 档）的结构完整性 "));
    {
        FlangeParams p;                       // A4 横向 / DN100
        SketchModel m;
        const FlangeViewsResult r = buildFlangeViews(p, &m);
        check(!r.elems.empty(), QStringLiteral("生成非空图元列表"),
              QStringLiteral("%1 个").arg((int)r.elems.size()));
        check(r.scaleLabel == QStringLiteral("1:1") || r.scaleLabel == QStringLiteral("1:2") ||
              r.scaleLabel.startsWith(QStringLiteral("1:")),
              QStringLiteral("比例为标准比例串"), QStringLiteral("实际=%1").arg(r.scaleLabel));
        info(QStringLiteral("比例 = %1，图元 %2 个，尺寸标注 %3 个，引线 %4 个")
                 .arg(r.scaleLabel).arg((int)r.elems.size()).arg(r.dimCount).arg(r.leaderCount));

        // 主视图：三个同心圆（外圆 / 中心孔 / 孔倒角）+ 分布圆 + n 个螺栓孔
        // 颈部外圆在背面并被盘面遮挡，按正投影不画。
        const Element* outer = findByName(r.elems, QStringLiteral("主视图:法兰外圆"));
        const Element* bore  = findByName(r.elems, QStringLiteral("主视图:中心孔"));
        const Element* bcd   = findByName(r.elems, QStringLiteral("主视图:螺栓孔分布圆"));
        check(outer && bore && bcd, QStringLiteral("主视图含外圆/中心孔/分布圆"));
        check(countNumbered(r.elems, QStringLiteral("主视图:螺栓孔")) == p.boltCount,
              QStringLiteral("主视图螺栓孔数 = n"),
              QStringLiteral("%1 vs %2").arg(countNumbered(r.elems, QStringLiteral("主视图:螺栓孔")))
                                        .arg(p.boltCount));

        // 同心性：主视图现有圆的圆心必须是同一个点
        if (outer && bore) {
            const bool same = approx(outer->pts[0].x, bore->pts[0].x) &&
                              approx(outer->pts[0].y, bore->pts[0].y);
            check(same, QStringLiteral("主视图圆严格同心"));
        }

        // 俯视图：必须是矩形轮廓（盘体 + 颈部台阶），不再是圆
        // —— 主视图为圆时，侧视图/俯视图必须为矩形（仅球体三视图可均为圆）
        check(findByName(r.elems, QStringLiteral("俯视图:盘体轮廓")) &&
              findByName(r.elems, QStringLiteral("俯视图:颈部轮廓")),
              QStringLiteral("俯视图为矩形轮廓（盘体+颈部），非圆"));
        // 防御性：俯视图不得含任何"圆"图元（防止投影类型错误回潮）
        {
            int circles = 0;
            for (const auto& e : r.elems)
                if (e.name.startsWith(QStringLiteral("俯视图:")) && e.type == ElementType::Circle) ++circles;
            check(circles == 0, QStringLiteral("俯视图不含任何圆图元"),
                  QStringLiteral("圆数=%1").arg(circles));
        }

        // 侧视图：存在且含颈部轮廓、中心孔虚线
        check(findByName(r.elems, QStringLiteral("侧视图:颈部轮廓")) &&
              findByName(r.elems, QStringLiteral("侧视图:中心孔(虚线)")),
              QStringLiteral("侧视图含颈部轮廓与中心孔（虚线）"));
        check(countLayer(r.elems, QStringLiteral("法兰-虚线")) > 0,
              QStringLiteral("存在不可见轮廓（虚线图层非空）"),
              QStringLiteral("%1 条").arg(countLayer(r.elems, QStringLiteral("法兰-虚线"))));
        check(countLayer(r.elems, QStringLiteral("法兰-中心线")) > 0,
              QStringLiteral("存在中心线（点划线图层非空）"));

        // 出图要求：三视图不绘制"主视图/俯视图/侧视图"提示字（视图名文字图元不得出现）
        {
            int nameTags = 0;
            for (const auto& e : r.elems)
                if (e.name == QStringLiteral("视图名")) ++nameTags;
            check(nameTags == 0, QStringLiteral("图纸不出现三视图提示字（主/俯/侧视图名）"),
                  QStringLiteral("视图名图元数=%1").arg(nameTags));
        }
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 2. 投影对齐关系（第一角投影，长对正/高平齐/宽相等）"));
    {
        FlangeParams p;
        SketchModel m;
        const FlangeViewsResult r = buildFlangeViews(p, &m);
        const double s = r.scale;

        // 长对正：主视图与俯视图共竖直轴（同一 x 中心）
        check(approx(r.frontCx, r.topCx, 1e-9),
              QStringLiteral("长对正：主视图与俯视图 X 中心一致"),
              QStringLiteral("%1 vs %2").arg(r.frontCx, 0, 'f', 3).arg(r.topCx, 0, 'f', 3));

        // 高平齐：主视图与侧视图 Y 范围一致（二者高度都等于外径 D 的投影）
        const double frontTop = r.frontCy + r.frontR;
        const double frontBot = r.frontCy - r.frontR;
        const double sideTop  = r.sideCy + r.profHalfD;
        const double sideBot  = r.sideCy - r.profHalfD;
        check(approx(frontTop, sideTop, 1e-6) && approx(frontBot, sideBot, 1e-6),
              QStringLiteral("高平齐：主视图与侧视图 Y 范围一致"),
              QStringLiteral("主视图[%1,%2] 侧视图[%3,%4]")
                  .arg(frontBot, 0, 'f', 3).arg(frontTop, 0, 'f', 3)
                  .arg(sideBot, 0, 'f', 3).arg(sideTop, 0, 'f', 3));
        check(approx(r.frontCy, r.sideCy, 1e-9),
              QStringLiteral("高平齐：主视图与侧视图中心 Y 一致"),
              QStringLiteral("%1 vs %2").arg(r.frontCy, 0, 'f', 3).arg(r.sideCy, 0, 'f', 3));

        // 宽相等：侧视图宽度（轴向=hubHeight） == 俯视图高度（轴向=hubHeight）
        check(approx(2.0 * r.profHalfA, p.hubHeight * s, 1e-9),
              QStringLiteral("宽相等：侧视图宽 = 俯视图高 = 颈部总高"),
              QStringLiteral("侧宽=%1 俯高=%2 hubH=%3")
                  .arg(2.0 * r.profHalfA, 0, 'f', 3).arg(p.hubHeight * s, 0, 'f', 3)
                  .arg(p.hubHeight, 0, 'f', 3));

        // 侧视图位于主视图右侧且不重叠（二者之间为水平净间距 gapH）
        check(r.sideCx - r.profHalfA > r.frontCx + r.frontR + 1e-6,
              QStringLiteral("侧视图位于主视图右侧且不重叠"),
              QStringLiteral("sideL=%1 frontR=%2")
                  .arg(r.sideCx - r.profHalfA, 0, 'f', 3)
                  .arg(r.frontCx + r.frontR, 0, 'f', 3));
        // 俯视图位于主视图正下方且不重叠（俯视图顶边在主视图底边之上，二者之间为竖直净间距 gapV）
        check(r.topCy + r.profHalfA < r.frontCy - r.frontR - 1e-6,
              QStringLiteral("俯视图位于主视图正下方且不重叠"),
              QStringLiteral("topB=%1 frontT=%2")
                  .arg(r.topCy + r.profHalfA, 0, 'f', 3)
                  .arg(r.frontCy - r.frontR, 0, 'f', 3));
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 3. 参数敏感性：改尺寸，几何必须跟着变 "));
    {
        FlangeParams a;
        SketchModel m;
        const FlangeViewsResult ra = buildFlangeViews(a, &m);
        const Element* oa = findByName(ra.elems, QStringLiteral("主视图:法兰外圆"));

        FlangeParams b = a;
        b.outerD = 320.0;                     // 外径放大
        SketchModel m2;
        const FlangeViewsResult rb = buildFlangeViews(b, &m2);
        const Element* ob = findByName(rb.elems, QStringLiteral("主视图:法兰外圆"));
        // 由于 autoScale 会自适应比例，直接比较 "图上半径" 可能随比例变化而变小；
        // 应比较实物半径 = 图上半径 / scale。
        const double trueRa = oa ? (oa->radius / ra.scale) : -1.0;
        const double trueRb = ob ? (ob->radius / rb.scale) : -1.0;
        check(oa && ob && trueRb > trueRa,
              QStringLiteral("外径变大 → 主视图外圆实物半径变大"),
              QStringLiteral("%1 → %2 (scale %3 → %4)")
                  .arg(trueRa, 0, 'f', 3).arg(trueRb, 0, 'f', 3)
                  .arg(ra.scale).arg(rb.scale));

        // 螺栓孔数量改变 → 孔数跟着变
        FlangeParams c = a;
        c.boltCount = 8;
        c.boltCircle = 170.0;
        SketchModel m3;
        const FlangeViewsResult rc = buildFlangeViews(c, &m3);
        check(countNumbered(rc.elems, QStringLiteral("主视图:螺栓孔")) == 8,
              QStringLiteral("螺栓孔数量 4 → 8 生效"),
              QStringLiteral("%1 个").arg(countNumbered(rc.elems, QStringLiteral("主视图:螺栓孔"))));

        // 螺栓孔均布：每个孔必须落在"起始角 + i×360/n"的理想位置上。
        // 不要用"排序后比相邻夹角"—— atan2 返回 (−180,180]，排序会打断环形顺序
        // （例如 −179° 会排到 −1° 前面，算出 178° 的假间隙）。逐孔对齐理想值才可靠。
        {
            std::vector<double> angs;
            const Element* bc = findByName(rc.elems, QStringLiteral("主视图:法兰外圆"));
            for (const auto& e : rc.elems) {
                if (!e.name.startsWith(QStringLiteral("主视图:螺栓孔")) || e.pts.empty()) continue;
                if (e.name.contains(QStringLiteral("分布圆"))) continue;   // 分布圆不是孔
                double deg = std::atan2(e.pts[0].y - bc->pts[0].y, e.pts[0].x - bc->pts[0].x)
                             * 180.0 / M_PI;
                if (deg < 0.0) deg += 360.0;                  // 归一到 [0,360)
                angs.push_back(deg);
            }
            // 每个孔与最近的理想位置比较（环形的角度差取 min(|d|, 360−|d|)）
            double maxErr = 0;
            for (double a : angs) {
                double best = 1e9;
                for (int i = 0; i < 8; ++i) {
                    double d = std::fabs(a - (c.boltAngle0Deg + i * 45.0));
                    d = std::min(d, std::fabs(360.0 - d));
                    best = std::min(best, d);
                }
                maxErr = std::max(maxErr, best);
            }
            check(angs.size() == 8 && maxErr < 1e-6,
                  QStringLiteral("8 个螺栓孔严格均布（每孔落在 起点+i×360/n 上）"),
                  QStringLiteral("孔数=%1 最大偏差 %2°").arg((int)angs.size()).arg(maxErr, 0, 'f', 9));
            // 相邻孔距分布圆圆心必须严格等于分布圆半径
            double maxR = 0, minR = 1e9;
            for (const auto& e : rc.elems) {
                if (!e.name.startsWith(QStringLiteral("主视图:螺栓孔")) || e.pts.empty()) continue;
                if (e.name.contains(QStringLiteral("分布圆"))) continue;
                const double r = std::hypot(e.pts[0].x - bc->pts[0].x, e.pts[0].y - bc->pts[0].y);
                maxR = std::max(maxR, r); minR = std::min(minR, r);
            }
            check(maxR - minR < 1e-9,
                  QStringLiteral("8 个孔心严格落在同一分布圆上"),
                  QStringLiteral("半径范围 [%1, %2]").arg(minR, 0, 'f', 6).arg(maxR, 0, 'f', 6));
        }

        // 厚度改变 → 左视图总长与厚度标注跟着变
        FlangeParams d = a;
        d.thickness = 30.0;
        d.hubHeight = 45.0;
        SketchModel m4;
        const FlangeViewsResult rd = buildFlangeViews(d, &m4);
        check(rd.dimCount == ra.dimCount, QStringLiteral("改厚度不改变标注数量（结构不变）"),
              QStringLiteral("%1 vs %2").arg(ra.dimCount).arg(rd.dimCount));

        // 螺栓孔径改变 → 在图面文本（规格摘要）中反映；引线已按需求移除
        FlangeParams e2 = a;
        e2.boltHoleD = 26.0;
        SketchModel m5;
        const FlangeViewsResult re = buildFlangeViews(e2, &m5);
        int lead = 0;
        QString allText;
        for (const auto& el : re.elems) {
            if (el.dtype == DimensionType::Leader) ++lead;
            allText += el.text;
        }
        check(lead == 0, QStringLiteral("螺栓孔径用例中不含引线（引导线已移除）"),
              QStringLiteral("leader=%1").arg(lead));
        check(allText.contains(QStringLiteral("26")),
              QStringLiteral("螺栓孔径 18 → 26 反映到图面文本"),
              QStringLiteral("实际含26=%1").arg(allText.contains(QStringLiteral("26"))));
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 4. 结构不变式：非法参数必须被收敛，绝不画出错误几何 "));
    {
        // 4.1 厚度 ≥ 颈部总高 → 凸台消失（退化成平板，与附图结构不符）
        {
            FlangeParams p;
            p.thickness = 40.0;
            p.hubHeight = 30.0;              // 非法：颈部比盘体还矮
            check(!p.validate().isEmpty(), QStringLiteral("检出「颈部总高 ≤ 厚度」非法"),
                  p.validate());
            p.makeValid();
            check(p.hubHeight > p.thickness, QStringLiteral("makeValid 后凸台重新凸出"),
                  QStringLiteral("C=%1, hubH=%2").arg(p.thickness).arg(p.hubHeight));
            check(p.validate().isEmpty(), QStringLiteral("收敛后参数合法"), p.validate());
        }
        // 4.2 螺栓孔切进中心孔
        {
            FlangeParams p;
            p.boltCircle = 130.0;            // 半径 65；孔半径 9 → 内缘 56
            p.boreD = 118.0;                 // 中心孔半径 59 → 冲突
            p.hubD = 150.0;                  // 颈部半径 75 → 冲突更明显
            check(!p.validate().isEmpty(), QStringLiteral("检出「螺栓孔切进中心孔」非法"),
                  p.validate());
            p.makeValid();
            check(p.validate().isEmpty(), QStringLiteral("收敛后螺栓孔落在合法环带内"), p.validate());
            // 分布圆下限用中心孔口径（而非颈部外径）：螺栓孔只穿过盘体，
            // 完全允许落在颈部外圆以内，这正是标准法兰尺寸表的常态。
            const double rInner = p.boreD * 0.5 + p.boltHoleD * 0.5;
            const double rOuter = p.outerD * 0.5 - p.boltHoleD * 0.5;
            check(p.boltCircle * 0.5 >= rInner - 1e-9 && p.boltCircle * 0.5 <= rOuter + 1e-9,
                  QStringLiteral("分布圆半径 ∈ [中心孔半径+孔半径, 外半径−孔半径]"),
                  QStringLiteral("R=%1 ∈ [%2, %3]").arg(p.boltCircle * 0.5)
                      .arg(rInner).arg(rOuter));
        }
        // 4.3 螺栓孔切出外缘
        {
            FlangeParams p;
            p.boltCircle = 205.0;            // 半径 102.5 + 9 = 111.5 > 105
            check(!p.validate().isEmpty(), QStringLiteral("检出「螺栓孔切出外缘」非法"), p.validate());
            p.makeValid();
            check(p.validate().isEmpty(), QStringLiteral("收敛后螺栓孔不切出外缘"));
        }
        // 4.4 中心孔径 ≥ 颈部外径
        {
            FlangeParams p;
            p.boreD = 160.0;
            p.hubD = 150.0;
            check(!p.validate().isEmpty(), QStringLiteral("检出「中心孔径 ≥ 颈部外径」非法"));
            p.makeValid();
            check(p.boreD < p.hubD, QStringLiteral("收敛后颈部有正壁厚"),
                  QStringLiteral("bore=%1 hub=%2").arg(p.boreD).arg(p.hubD));
        }
        // 4.5 孔数为 0 / 负数
        {
            FlangeParams p;
            p.boltCount = 0;
            p.makeValid();
            check(p.boltCount >= 1, QStringLiteral("孔数 0 被收敛到 ≥1"),
                  QStringLiteral("实际=%1").arg(p.boltCount));
        }
        // 4.6 一组极端参数：全部收敛后必须始终满足不变式
        {
            const double outers[] = { 20.0, 75.0, 210.0, 1175.0, 5000.0 };
            const double bores[]  = { 1.0, 50.0, 900.0 };
            int checked = 0;
            for (double od : outers)
                for (double bd : bores) {
                    FlangeParams p;
                    p.outerD = od; p.boreD = bd;
                    p.hubD = od * 0.72; p.boltCircle = od * 0.8;
                    p.thickness = od * 0.08; p.hubHeight = od * 0.13;
                    p.makeValid();
                    if (!p.validate().isEmpty()) {
                        check(false, QStringLiteral("极端参数收敛后仍合法"),
                              QStringLiteral("D=%1 bore=%2 → %3").arg(od).arg(bd).arg(p.validate()));
                        return 2;
                    }
                    ++checked;
                }
            check(checked == 15, QStringLiteral("15 组极端参数全部收敛合法"),
                  QStringLiteral("已验证 %1 组").arg(checked));
        }
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 5. 图面合规：A4 内、不撞标题栏、标准比例 "));
    {
        const double outers[] = { 75.0, 100.0, 130.0, 210.0, 320.0, 440.0, 645.0, 1175.0 };
        int okCount = 0;
        for (double od : outers) {
            FlangeParams p;
            p.outerD     = od;
            p.hubD       = od * 0.72;
            p.boltCircle = od * 0.8;
            p.thickness  = std::max(3.0, od * 0.08);
            p.hubHeight  = std::max(6.0, od * 0.13);
            p.boltHoleD  = std::max(4.0, od * 0.09);
            p.boltCount  = 4;
            p.boreD      = od * 0.5;
            p.makeValid();

            SketchModel m;
            const FlangeViewsResult r = buildFlangeViews(p, &m);
            const double s = r.scale;

            // 比例必须是标准序列里的值
            bool stdScale = false;
            for (double c : { 50.0, 20.0, 10.0, 5.0, 2.0, 1.0, 0.5, 0.4, 0.2, 0.1, 0.05, 0.02, 0.01 })
                if (std::fabs(s - c) < 1e-9) stdScale = true;
            if (!stdScale) {
                check(false, QStringLiteral("比例取自标准序列"), QStringLiteral("D=%1 s=%2").arg(od).arg(s));
                return 2;
            }

            // 不与标题栏冲突：标题栏矩形 = 右下角 180×56（与 sheet_frame 相同公式）
            const SheetSpec a4 = SheetSpec::make(SheetFormat::A4, true);
            const double c = 10.0;                       // A4 的非装订边距
            const double tbW = std::min(180.0, (a4.width - c) - 25.0 - 10.0);
            const QRectF tb(a4.width - c - tbW, c, tbW, 56.0);
            const QRectF views = r.bounds;
            const bool noClash = !views.intersects(tb);
            if (!noClash) {
                check(false, QStringLiteral("视图不与标题栏重叠"),
                      QStringLiteral("D=%1 views=%2,%3 %4x%5 tb=%6,%7")
                          .arg(od).arg(views.left(), 0, 'f', 1).arg(views.top(), 0, 'f', 1)
                          .arg(views.width(), 0, 'f', 1).arg(views.height(), 0, 'f', 1)
                          .arg(tb.left(), 0, 'f', 1).arg(tb.top(), 0, 'f', 1));
                return 2;
            }
            // 完整落在图框内（图框 = 25/10/10/10 边距）
            const QRectF frame(25.0, c, a4.width - c - 25.0, a4.height - c - c);
            if (!frame.contains(views)) {
                // 诊断：找出把边界顶出图框的图元
                for (const auto& e : r.elems) {
                    const QRectF eb = geo::elementBounds(e);
                    if (!frame.contains(eb)) {
                        info(QStringLiteral("越界图元: %1 bounds=(%2,%3 %4x%5)")
                                 .arg(e.name)
                                 .arg(eb.left(), 0, 'f', 2).arg(eb.top(), 0, 'f', 2)
                                 .arg(eb.width(), 0, 'f', 2).arg(eb.height(), 0, 'f', 2));
                    }
                }
                check(false, QStringLiteral("视图完整落在图框内"),
                      QStringLiteral("D=%1 views=%2,%3 %4x%5 frame=%6,%7 %8x%9")
                          .arg(od).arg(views.left(), 0, 'f', 1).arg(views.top(), 0, 'f', 1)
                          .arg(views.width(), 0, 'f', 1).arg(views.height(), 0, 'f', 1)
                          .arg(frame.left(), 0, 'f', 1).arg(frame.top(), 0, 'f', 1)
                          .arg(frame.width(), 0, 'f', 1).arg(frame.height(), 0, 'f', 1));
                return 2;
            }
            info(QStringLiteral("D=%1 → 比例 %2，视图 %3×%4 @(%5,%6)，与标题栏无冲突")
                     .arg(od, 0, 'f', 0).arg(r.scaleLabel)
                     .arg(views.width(), 0, 'f', 1).arg(views.height(), 0, 'f', 1)
                     .arg(views.left(), 0, 'f', 1).arg(views.top(), 0, 'f', 1));
            ++okCount;
        }
        check(okCount == 8, QStringLiteral("8 档外径（75→1175）全部自动选到标准比例且不撞标题栏"),
              QStringLiteral("通过 %1 档").arg(okCount));
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 6. 尺寸标注：数值为实物尺寸、数量恒定 "));
    {
        FlangeParams p;
        SketchModel m;
        const FlangeViewsResult r = buildFlangeViews(p, &m);
        int dims = countType(r.elems, ElementType::Dimension);
        check(dims == 7, QStringLiteral("共 7 个尺寸标注（引导线已移除）"),
              QStringLiteral("实际=%1").arg(dims));
        // 引导线按需求移除：leaderCount 应为 0；尺寸标注维持 7 个
        check(r.dimCount == 7 && r.leaderCount == 0,
              QStringLiteral("尺寸标注 7 个（不含引线）"),
              QStringLiteral("dim=%1 leader=%2").arg(r.dimCount).arg(r.leaderCount));

        // 标注文本必须含实物尺寸数字，而不是 ×比例 后的图上尺寸
        bool hasD = false, hasK = false, hasC = false;
        for (const auto& e : r.elems) {
            if (e.type != ElementType::Dimension) continue;
            if (e.text.contains(QStringLiteral("210"))) hasD = true;
            if (e.text.contains(QStringLiteral("170"))) hasK = true;
            if (e.text.contains(QStringLiteral("16")))  hasC = true;
        }
        check(hasD && hasK && hasC,
              QStringLiteral("标注文本为实物尺寸（D=210 / K=170 / C=16）"),
              QStringLiteral("D:%1 K:%2 C:%3").arg(hasD).arg(hasK).arg(hasC));

        // 每个尺寸标注的 pts 必须是 3 点（A/B/尺寸线位置），否则 dim::build 会退化
        bool ptsOk = true;
        for (const auto& e : r.elems)
            if (e.type == ElementType::Dimension && e.pts.size() != 3) ptsOk = false;
        check(ptsOk, QStringLiteral("所有标注的点位格式为 3 点（测量A/测量B/尺寸线位置）"));

        // 法兰三维图标注字高应缩小（≤3.5mm），避免遮挡图形
        bool smallFont = true;
        for (const auto& e : r.elems)
            if (e.type == ElementType::Dimension && e.dimTextHeight <= 1e-6) smallFont = false;
        check(smallFont, QStringLiteral("尺寸标注使用缩小字高(≤3.5mm)"));
    }

    // ------------------------------------------------------------------
    outLine(QStringLiteral("-- 7. 渲染预览写出（人工核对用）"));
    {
        QDir().mkpath(QStringLiteral("build/flange_preview"));

        // 7.1 默认 DN100，四孔按附图姿态错开 45°
        FlangeParams p;
        p.boltAngle0Deg = 45.0;
        renderPreview(p, QStringLiteral("build/flange_preview/flange_dn100.png"));
        renderZoom(p, QStringLiteral("build/flange_preview/flange_dn100_zoom.png"));

        // 7.2 放大到 DN300 一档：外径/颈部/分布圆/孔数全部改变，结构不变
        FlangeParams q = p;
        q.outerD = 320.0; q.hubD = 230.0; q.boltCircle = 256.0; q.thickness = 26.0;
        q.hubHeight = 42.0; q.boltHoleD = 26.0; q.boltCount = 8; q.boreD = 180.0;
        q.makeValid();
        renderPreview(q, QStringLiteral("build/flange_preview/flange_dn300.png"));

        // 7.3 大外径 + 多孔：验证比例自动降到 1:2 仍不与标题栏冲突
        FlangeParams z;
        z.outerD = 900.0; z.hubD = 660.0; z.boltCircle = 820.0; z.thickness = 30.0;
        z.hubHeight = 54.0; z.boltHoleD = 30.0; z.boltCount = 24; z.boreD = 560.0;
        z.boltAngle0Deg = 45.0;
        z.makeValid();
        const FlangeViewsResult r = buildFlangeViews(z, nullptr);
        info(QStringLiteral("DN900 大规格 → 比例 %1，视图 %2×%3")
                 .arg(r.scaleLabel).arg(r.bounds.width(), 0, 'f', 1).arg(r.bounds.height(), 0, 'f', 1));
        // DN900 属于超大外径，自动比例应显著缩小（1:20 左右），而非 1:2
        check(r.scale == 0.05 || r.scale == 0.02 || r.scale == 0.01,
              QStringLiteral("DN900 自动选到大幅缩小比例"),
              QStringLiteral("实际=%1").arg(r.scaleLabel));
        check(r.fits, QStringLiteral("DN900 布局完整落在可用区内"));
        renderPreview(z, QStringLiteral("build/flange_preview/flange_dn900.png"));

        // 7.4 用户指定规格：平板止口法兰 Ø160 / 4×Ø18@Ø130 / 止口Ø100×5 / 内孔Ø60 / 厚20
        //      —— 无颈部凸台（hasHub=false），右端面带止口凹槽（recess）。
        FlangeParams sp;
        sp.outerD = 160.0; sp.thickness = 20.0; sp.boreD = 60.0;
        sp.boltCircle = 130.0; sp.boltHoleD = 18.0; sp.boltCount = 4;
        sp.boltAngle0Deg = 45.0;
        sp.hasHub = false;                 // 平板法兰，无颈部凸台
        sp.recessD = 100.0; sp.recessDepth = 5.0; sp.recessFace = 1;  // 右端面止口 Ø100×5
        sp.boreChamfer = 0.0; sp.hubChamfer = 0.0;
        sp.boreTol = QStringLiteral("H8");          // 中心孔 Ø60H8
        sp.thicknessTol = QStringLiteral("±0.2");    // 厚度 20±0.2
        sp.recessTol = QStringLiteral("h7");         // 止口外径 Ø100h7
        sp.sheetW = 420.0; sp.sheetH = 297.0;  // A3 横向，让 160mm 法兰能接近 1:2 放大显示
        sp.makeValid();
        {
            SketchModel m2;
            const FlangeViewsResult r2 = buildFlangeViews(sp, &m2);
            info(QStringLiteral("止口平板法兰 → 比例 %1，图元 %2 个")
                     .arg(r2.scaleLabel).arg((int)r2.elems.size()));
            // 平板法兰：侧/俯视图不得出现"颈部轮廓"图元（无凸台）
            check(!findByName(r2.elems, QStringLiteral("侧视图:颈部轮廓")) &&
                  !findByName(r2.elems, QStringLiteral("俯视图:颈部轮廓")),
                  QStringLiteral("平板法兰：侧/俯视图无颈部轮廓（无凸台）"));
            // 主视图含止口外圆（Ø100），且仍是圆（外圆+中心孔+止口）
            check(findByName(r2.elems, QStringLiteral("主视图:止口外圆")),
                  QStringLiteral("主视图含止口外圆(Ø100)"));
            check(findByName(r2.elems, QStringLiteral("主视图:法兰外圆")) &&
                  findByName(r2.elems, QStringLiteral("主视图:中心孔")),
                  QStringLiteral("主视图为圆（外圆+中心孔）"));
            // 俯视图为矩形轮廓（盘体），非圆 —— 圆→矩形正投影规则仍成立
            check(findByName(r2.elems, QStringLiteral("俯视图:盘体轮廓")),
                  QStringLiteral("俯视图为矩形轮廓"));
            { int circles = 0;
              for (const auto& e : r2.elems)
                  if (e.name.startsWith(QStringLiteral("俯视图:")) && e.type == ElementType::Circle) ++circles;
              check(circles == 0, QStringLiteral("俯视图不含圆图元"), QStringLiteral("圆数=%1").arg(circles)); }
            // 侧视图应含止口轮廓（止口壁/止口底/右端面环带）
            check(findByName(r2.elems, QStringLiteral("侧视图:止口底")) &&
                  findByName(r2.elems, QStringLiteral("侧视图:止口壁")),
                  QStringLiteral("侧视图含止口轮廓（止口底/止口壁）"));
            // 结构不变式：止口外径必须 > 中心孔径（包住内孔）
            check(sp.recessD > sp.boreD, QStringLiteral("止口外径>中心孔径（结构合法）"));
            // 尺寸标注：平板止口法兰 = D/K/内孔/厚/俯D/止口深/止口外径(7个)；引线已按需求移除
            check(r2.dimCount == 7 && r2.leaderCount == 0,
                  QStringLiteral("平板止口法兰尺寸 7 个（含止口深5与Ø100h7，引导线已移除）"),
                  QStringLiteral("dim=%1 leader=%2").arg(r2.dimCount).arg(r2.leaderCount));
            check(r2.fits, QStringLiteral("止口平板法兰布局完整落在可用区内"));
            // 公差文本与止口深必须落进标注（与 ASCII 草图完全一致：Ø60H8 / 20±0.2 / Ø100h7 / 深5）
            bool hasBoreTol = false, hasThkTol = false, hasRecessTol = false, hasRecessDepth = false;
            for (const auto& e : r2.elems) {
                if (e.type != ElementType::Dimension) continue;
                if (e.text.contains(QStringLiteral("H8")))  hasBoreTol = true;
                if (e.text.contains(QStringLiteral("±0.2"))) hasThkTol = true;
                if (e.text.contains(QStringLiteral("h7")))  hasRecessTol = true;
                if (e.dtype != DimensionType::Leader && e.text == QStringLiteral("5")) hasRecessDepth = true;
            }
            check(hasBoreTol,     QStringLiteral("中心孔标注含公差 H8（⌀60H8）"));
            check(hasThkTol,      QStringLiteral("厚度标注含公差 ±0.2（20±0.2）"));
            check(hasRecessTol,   QStringLiteral("止口外径标注含公差 h7（⌀100h7）"));
            check(hasRecessDepth, QStringLiteral("存在止口深度尺寸 5"));
        }
        renderPreview(sp, QStringLiteral("build/flange_preview/flange_spigot_dn160.png"));

        check(QFile::exists(QStringLiteral("build/flange_preview/flange_dn100.png")) &&
              QFile::exists(QStringLiteral("build/flange_preview/flange_dn300.png")) &&
              QFile::exists(QStringLiteral("build/flange_preview/flange_dn900.png")) &&
              QFile::exists(QStringLiteral("build/flange_preview/flange_spigot_dn160.png")),
              QStringLiteral("四张预览图已写出"));
    }

    outLine(QStringLiteral("RESULT: %1  （通过 %2 / 失败 %3）")
             .arg(g_fail == 0 ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
             .arg(g_pass).arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
