// GB/T 14665-2012 / 关联标准 图框与文字回归（核心层，无需 GUI）：
//   * GB/T 14689 表4 图框边距：a=25，A0/A1/A2 的 c=10，A3/A4/A5 的 c=5（T1）；
//   * GB/T 14689 6.1 对中符号：四边中点各一条 5mm 伸入、线宽≥0.5mm（T17）；
//   * GB/T 14665-2012 第7章 表5 投影法识别符号（T10）；
//   * GB/T 14665-2012 5.6 表3 字高与图幅（T6）；
//   * GB/T 14690 比例系列判定（T16）。
#include "core/SheetSpec.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
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

// 渲染图框到 PNG（人工核对：幅面边距 / 对中符号 / 投影法符号 / 标题栏）
static void renderSheet(SheetFormat f, bool landscape, ProjectionMethod pm, const QString& path) {
    SheetSpec spec = SheetSpec::make(f, landscape);
    spec.projection = pm;
    const auto elems = buildSheetFrame(spec);
    const double k = 1008.0 / spec.width;                 // 固定输出宽 ≈1008 px
    QImage img(int(spec.width * k), int(spec.height * k), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter pt(&img);
    pt.setRenderHint(QPainter::Antialiasing, true);
    pt.scale(k, -k);                                      // Y 向上的图纸坐标
    pt.translate(0, -spec.height);
    for (const auto& e : elems) {
        QPen pen(e.color);
        pen.setWidthF(std::max(0.5, e.effectiveLineWidth()));
        if (e.lineStyle == LineStyle::Dashed)       pen.setStyle(Qt::DashLine);
        else if (e.lineStyle == LineStyle::DashDot) pen.setStyle(Qt::DashDotLine);
        else if (e.lineStyle == LineStyle::Dotted)  pen.setStyle(Qt::DotLine);
        pt.setPen(pen);
        pt.setBrush(Qt::NoBrush);
        switch (e.type) {
            case ElementType::Line:
                if (e.pts.size() >= 2) pt.drawLine(QPointF(e.pts[0].x, e.pts[0].y),
                                                  QPointF(e.pts[1].x, e.pts[1].y));
                break;
            case ElementType::Rectangle:
                if (e.pts.size() >= 2) pt.drawRect(QRectF(QPointF(e.pts[0].x, e.pts[0].y),
                                                          QPointF(e.pts[1].x, e.pts[1].y)).normalized());
                break;
            case ElementType::Circle:
                if (!e.pts.empty()) pt.drawEllipse(QPointF(e.pts[0].x, e.pts[0].y), e.radius, e.radius);
                break;
            case ElementType::Text: {
                if (e.pts.empty()) break;
                QFont fnt = pt.font();
                fnt.setPointSizeF(std::max(1.0, e.radius * 0.7));
                pt.setFont(fnt);
                pt.save();
                pt.translate(e.pts[0].x, e.pts[0].y);
                pt.scale(1, -1);                          // 文字不镜像
                pt.drawText(QPointF(0, 0), e.text);
                pt.restore();
                break;
            }
            default: break;
        }
    }
    pt.end();
    img.save(path);
    printf("  [INFO] 图框预览已写出 %s（%dx%d px）\n",
           path.toUtf8().constData(), img.width(), img.height());
}

// 图框矩形（粗实线，lineWidth>1.5）
static bool frameRect(const std::vector<Element>& v, double& x0, double& y0, double& x1, double& y1) {
    for (const auto& e : v)
        if (e.type == ElementType::Rectangle && e.layer == QStringLiteral("图框")
            && e.lineWidth > 1.5 && e.pts.size() >= 2) {
            x0 = std::min(e.pts[0].x, e.pts[1].x); y0 = std::min(e.pts[0].y, e.pts[1].y);
            x1 = std::max(e.pts[0].x, e.pts[1].x); y1 = std::max(e.pts[0].y, e.pts[1].y);
            return true;
        }
    return false;
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    // ==================== T1：图框边距 c（GB/T 14689 表4）====================
    struct Case { SheetFormat f; double expC; };
    const Case cases[] = { {SheetFormat::A0, 10.0}, {SheetFormat::A1, 10.0}, {SheetFormat::A2, 10.0},
                           {SheetFormat::A3, 5.0},  {SheetFormat::A4, 5.0},  {SheetFormat::A5, 5.0} };
    for (const auto& cs : cases) {
        const SheetSpec spec = SheetSpec::make(cs.f, true);
        auto elems = buildSheetFrame(spec);
        double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
        const bool found = frameRect(elems, x0, y0, x1, y1);
        check(found, "存在图框矩形");
        check(near(x0, 25.0) && near(y0, cs.expC) &&
              near(x1, spec.width - cs.expC) && near(y1, spec.height - cs.expC),
              qPrintable(QStringLiteral("图框边距 a=25/c=%1 正确 (%2)").arg(cs.expC).arg(spec.formatName())),
              QString("x0=%1 y0=%2 x1=%3 y1=%4").arg(x0).arg(y0).arg(x1).arg(y1));
    }

    // ==================== T17：对中符号（6.1）====================
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A3, true);   // 420×297, a=25, c=5
        auto elems = buildSheetFrame(spec);
        double fx0 = 0.0, fy0 = 0.0, fx1 = 0.0, fy1 = 0.0;
        frameRect(elems, fx0, fy0, fx1, fy1);          // 图框（粗实线矩形）

        std::vector<const Element*> marks;
        for (const auto& e : elems) {
            if (e.type != ElementType::Line || e.layer != QStringLiteral("图框")) continue;
            if (!e.name.isEmpty() || !near(e.lineWidth, 0.5) || e.pts.size() < 2) continue;
            // 必须有一端落在图纸边界上（自纸边界画起）
            auto onPaper = [&](const Vec3& p) {
                return near(p.x, 0.0) || near(p.x, spec.width) ||
                       near(p.y, 0.0) || near(p.y, spec.height);
            };
            if (onPaper(e.pts[0]) || onPaper(e.pts[1])) marks.push_back(&e);
        }
        check((int)marks.size() == 4, "四边中点各一条对中符号（自纸边界画起、线宽 0.5）",
              QString::number((int)marks.size()));

        bool posOk = true, depthOk = true;
        for (const Element* m : marks) {
            const Vec3& p0 = m->pts[0];
            const Vec3& p1 = m->pts[1];
            if (near(p0.y, p1.y)) {                       // 水平线 → 左/右边（中点 Y=H/2）
                if (!near(p0.y, spec.height / 2.0)) posOk = false;
                const double inner = near(std::min(p0.x, p1.x), 0.0) ? std::max(p0.x, p1.x)
                                                                     : std::min(p0.x, p1.x);
                const double edge = (inner < spec.width * 0.5) ? fx0 : fx1;
                if (!near(std::fabs(inner - edge), 5.0)) depthOk = false;
            } else {                                      // 垂直线 → 上/下边（中点 X=W/2）
                if (!near(p0.x, spec.width / 2.0)) posOk = false;
                const double inner = near(std::min(p0.y, p1.y), 0.0) ? std::max(p0.y, p1.y)
                                                                     : std::min(p0.y, p1.y);
                const double edge = (inner < spec.height * 0.5) ? fy0 : fy1;
                if (!near(std::fabs(inner - edge), 5.0)) depthOk = false;
            }
        }
        check(posOk, "对中符号位于图纸各边中点（x=W/2 / y=H/2）");
        check(depthOk, "对中符号自纸边界伸入图框内 5mm");
    }

    // ==================== T10：投影法识别符号（第7章 表5）====================
    {
        SheetSpec spec = SheetSpec::make(SheetFormat::A3, true);
        spec.projection = ProjectionMethod::FirstAngle;
        auto a = buildSheetFrame(spec);
        int nCircle = 0; bool hasFirst = false;
        for (const auto& e : a) {
            if (e.type == ElementType::Circle && e.name.contains(QStringLiteral("投影符号"))) {
                ++nCircle;
                if (near(e.radius, 5.0)) { /* ok */ }
            }
            if (e.type == ElementType::Text && e.text == QStringLiteral("第一角画法")) hasFirst = true;
        }
        check(nCircle == 1, "生成 1 个投影法识别符号（圆）", QString::number(nCircle));
        check(hasFirst, "第一角画法标注正确");

        spec.projection = ProjectionMethod::ThirdAngle;
        auto b = buildSheetFrame(spec);
        bool hasThird = false;
        for (const auto& e : b)
            if (e.type == ElementType::Text && e.text == QStringLiteral("第三角画法")) hasThird = true;
        check(hasThird, "第三角画法标注正确");
    }

    // ==================== T6：字高与图幅（5.6 表3）====================
    {
        check(near(standardTextHeightMm(SheetFormat::A0, true), 7.0) &&
              near(standardTextHeightMm(SheetFormat::A1, true), 7.0),
              "A0/A1 汉字字高 7");
        check(near(standardTextHeightMm(SheetFormat::A0, false), 5.0),
              "A0/A1 字母数字字高 5");
        check(near(standardTextHeightMm(SheetFormat::A2, true), 5.0) &&
              near(standardTextHeightMm(SheetFormat::A3, true), 5.0) &&
              near(standardTextHeightMm(SheetFormat::A4, true), 5.0),
              "A2/A3/A4 汉字字高 5");
        check(near(standardTextHeightMm(SheetFormat::A3, false), 3.5) &&
              near(standardTextHeightMm(SheetFormat::A4, false), 3.5),
              "A2/A3/A4 字母数字字高 3.5");
    }

    // ==================== T16：比例系列（GB/T 14690）====================
    {
        check(isStandardDrawingScale(1.0) && isStandardDrawingScale(0.5) && isStandardDrawingScale(0.2)
              && isStandardDrawingScale(0.1) && isStandardDrawingScale(0.05) && isStandardDrawingScale(0.01),
              "优先系列判定正确（1:1/1:2/1:5/1:10/1:20/1:100）");
        check(isStandardDrawingScale(2.0) && isStandardDrawingScale(5.0) && isStandardDrawingScale(10.0),
              "优先系列放大（2:1/5:1/10:1）判定正确");
        check(isStandardDrawingScale(0.4) && isStandardDrawingScale(1.0 / 3.0) &&
              isStandardDrawingScale(0.25) && isStandardDrawingScale(1.0 / 6.0) &&
              isStandardDrawingScale(2.5) && isStandardDrawingScale(4.0),
              "允许系列判定正确（1:2.5/1:3/1:4/1:6/2.5:1/4:1）");
        check(!isStandardDrawingScale(0.7) && !isStandardDrawingScale(3.0) && !isStandardDrawingScale(0.0),
              "非标准比例（0.7/3:1/0）判为非标准");
    }

    // ==================== 预览渲染（人工核对用）====================
    renderSheet(SheetFormat::A3, true,  ProjectionMethod::FirstAngle,
                QStringLiteral("build/gb_sheet_a3_first.png"));
    renderSheet(SheetFormat::A0, true,  ProjectionMethod::ThirdAngle,
                QStringLiteral("build/gb_sheet_a0_third.png"));

    printf("\nGB 图框/文字/比例测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
