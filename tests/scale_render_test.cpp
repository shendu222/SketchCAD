// 渲染级验证：改绘图比例后，「图形」在屏幕上真的缩小了，而「图框/标题栏」纹丝不动。
//
// 为什么必须做像素级测试：本项目的核心教训是**矩阵层测试通过 ≠ 渲染正确**——
// 数据层（SketchModel）的缩放断言全绿，只能证明"坐标变了"，不能证明"画出来的样子变了"。
// 而用户的需求原话是"绘图也应该根据比例进行缩放"，说的是**看得见的图**。
// 同理也必须证明图框没跟着缩（图框代表固定纸张），这同样是"纸上"才看得出的契约。
//
// 手段：用 fitToRect(整幅图框) 把视图变换**完全钉死**（与内容大小无关），
// 这样内容缩小就会在像素上等比体现；再用颜色把被测图元与图框分离——
// 被测直线画成纯红，图框是黑/深色，网格是浅灰，用通道阈值即可干净区分。
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QScrollBar>
#include <QTransform>
#include <QColor>
#include <cstdio>
#include <cmath>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/ElementFactory.h"
#include "core/SheetSpec.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", qPrintable(what)); }
    else    { printf("  [FAIL] %s%s%s\n", qPrintable(what),
                     detail.isEmpty() ? "" : " (", qPrintable(detail.isEmpty() ? QString() : detail + ")")); ++g_fail; }
}

static QImage renderView(QGraphicsView* v, const QString& dump = QString()) {
    QImage img(v->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    { QPainter p(&img); v->viewport()->render(&p); }
    if (!dump.isEmpty()) img.save(dump);
    return img;
}

// 某类像素的包围盒（用于量"长度"）。match(x,y,color) 返回该像素是否计入。
struct Box { int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1; long n = 0;
             bool empty() const { return n == 0; }
             int w() const { return n ? x1 - x0 + 1 : 0; }
             int h() const { return n ? y1 - y0 + 1 : 0; }
             double cx() const { return n ? (x0 + x1) / 2.0 : -1; }
             double cy() const { return n ? (y0 + y1) / 2.0 : -1; } };

template <typename F>
static Box scan(const QImage& img, F match) {
    Box b;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            if (!match(x, y, c)) continue;
            if (x < b.x0) b.x0 = x;
            if (y < b.y0) b.y0 = y;
            if (x > b.x1) b.x1 = x;
            if (y > b.y1) b.y1 = y;
            ++b.n;
        }
    }
    return b;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    SketchModel model;
    GraphicsView view(&model);
    view.resize(900, 700);
    view.show();
    app.processEvents();

    // 一张 A3 图框（图元在 图框/标题栏 图层上，按契约不随比例缩放）
    const SheetSpec spec = SheetSpec::make(SheetFormat::A3, true);
    model.beginEdit(QStringLiteral("新建图纸"));
    model.addElementsQuiet(buildSheetFrame(spec));
    model.emitModelChanged();

    // 被测图元：一条长 100 个图纸单位的纯红粗线，**放在幅面内部**（否则会落在视口外，
    // 或与图框边线重叠而被盖住）。它是唯一的非图框内容，故缩放中心就是它的包围盒中心。
    Element line = ElementFactory::createLine(Vec3(110, 100, 0), Vec3(210, 100, 0), nullptr);
    line.color = QColor(255, 0, 255);
    line.lineWidthAuto = false;
    line.lineWidth = 3.0;
    model.addElement(line);
    app.processEvents();

    check(std::fabs(model.scaleRatio() - 1.0) < 1e-9, QStringLiteral("初始比例为 1:1"));

    // 把视图钉死在「整幅图框」上：变换只由这个矩形决定，与内容大小无关，
    // 所以内容缩小必然在像素上等比体现（若改用 fitView，变换会跟着内容自适应，
    // 缩小后又被放大回满屏，像素上反而量不出差别）。
    view.fitToRect(QRectF(0.0, 0.0, spec.width, spec.height));
    app.processEvents();

    // 品红：图框/标题栏/网格都不会用这个色，避免混入固定干扰像素（红黑阈值曾误收标题栏 ~90px）
    const auto isRed  = [](int, int, QRgb c) { return qRed(c) > 200 && qGreen(c) < 80 && qBlue(c) > 200; };
    const auto isDark = [](int, int, QRgb c) { return qRed(c) < 120 && qGreen(c) < 120 && qBlue(c) < 120; };

    const QImage img1 = renderView(&view, QStringLiteral("build/scale_render_1to1.png"));
    const Box red1  = scan(img1, isRed);
    const Box dark1 = scan(img1, isDark);
    printf("  [INFO] dark bbox x[%d..%d] y[%d..%d]\n", dark1.x0, dark1.x1, dark1.y0, dark1.y1);
    // ---- 回归守卫：fitToRect 必须真的把整幅图框框好 ----
    // 曾经缺陷：Qt 在 viewportTransform() 上再叠加一个常量偏移 C（vbar 值会跑到 200000
    // 这种越界值），使 fitToRect 的矩阵虽对、画面却偏出视口——新建图纸后只能看到图框一角。
    // 注意断言目标：**实际生效的映射** = 矩阵 ∘ +C，与矩阵恒差一个常量，永不可能相等，
    // 所以不能断言"二者相等"（那样写会把画面推得更偏）。要断言的是「框好了没有」。
    {
        const QTransform want = view.transform();
        const QTransform got  = view.viewportTransform();
        const double vw = view.viewport()->width(), vh = view.viewport()->height();
        const double z = want.m11();
        // fitToRect 的意图：世界矩形中心落在视口中心，0.92 留边
        const double idealDx = vw / 2.0 - (spec.width / 2.0) * z;
        const double idealDy = vh / 2.0 + (spec.height / 2.0) * z;
        printf("  [INFO] effective dx=%.2f dy=%.2f | ideal dx=%.2f dy=%.2f | matrix m31=%.2f m32=%.2f\n",
               got.dx(), got.dy(), idealDx, idealDy, want.m31(), want.m32());
        check(std::fabs(got.dx() - idealDx) < 1.0 && std::fabs(got.dy() - idealDy) < 1.0,
              QStringLiteral("实际生效的映射符合 fitToRect 意图（视口偏移已补偿）"),
              QStringLiteral("effective=(%1,%2) ideal=(%3,%4)")
                  .arg(got.dx(), 0, 'f', 2).arg(got.dy(), 0, 'f', 2)
                  .arg(idealDx, 0, 'f', 2).arg(idealDy, 0, 'f', 2));
    }
    {
        // fitToRect 的契约：整幅幅面完整落在视口内并居中，否则"新建图纸"后看不到整张图
        const QPointF p00 = view.mapFromScene(QPointF(0, 0));
        const QPointF pWH = view.mapFromScene(QPointF(spec.width, spec.height));
        const double x0 = std::min(p00.x(), pWH.x()), x1 = std::max(p00.x(), pWH.x());
        const double y0 = std::min(p00.y(), pWH.y()), y1 = std::max(p00.y(), pWH.y());
        const double vw = view.viewport()->width(), vh = view.viewport()->height();
        printf("  [INFO] sheet on screen: x[%.1f..%.1f] y[%.1f..%.1f]  viewport %gx%g\n",
               x0, x1, y0, y1, vw, vh);
        check(x0 >= 0 && y0 >= 0 && x1 <= vw && y1 <= vh,
              QStringLiteral("fitToRect 后整幅图框完整可见（新建图纸即可看到全图）"),
              QStringLiteral("x[%1..%2] y[%3..%4] 视口 %5x%6")
                  .arg(x0).arg(x1).arg(y0).arg(y1).arg(vw).arg(vh));
        check(std::fabs(x0 - (vw - x1)) < 0.06 * vw && std::fabs(y0 - (vh - y1)) < 0.06 * vh,
              QStringLiteral("图框在视口内居中对齐"),
              QStringLiteral("左右留白 %1/%2，上下留白 %3/%4")
                  .arg(x0, 0, 'f', 1).arg(vw - x1, 0, 'f', 1)
                  .arg(y0, 0, 'f', 1).arg(vh - y1, 0, 'f', 1));
    }
    printf("  [INFO] 1:1  红线像素包围盒 %dx%d (n=%ld, 中心 %.1f,%.1f)；深色(图框) %dx%d (n=%ld)\n",
           red1.w(), red1.h(), red1.n, red1.cx(), red1.cy(), dark1.w(), dark1.h(), dark1.n);
    check(!red1.empty() && red1.w() > 40, QStringLiteral("1:1 下红色直线已渲染出来"),
          QStringLiteral("宽 %1px").arg(red1.w()));
    check(!dark1.empty(), QStringLiteral("图框已渲染（深色像素存在）"));

    // ---- 回归守卫：applyTransform() 必须幂等（视口偏移补偿不得回写 panX_/panY_）----
    // 曾经缺陷：补偿写成 `panX_ -= shift; apply();`——单次调用结果正确，但每次 applyTransform
    // 都会再减掉同一个 shift，于是连续平移/缩放后画面跑飞（实测 ViewPanTest 里拖 60px 的矩阵
    // 增量从 60 变成 −279，ViewRenderTest 的增量断言随之中断）。
    // 判据：反复施加"无操作"变换后，实际生效的映射必须原地不动。
    {
        const QPointF p0 = view.mapFromScene(QPointF(0, 0));
        const double e0x = view.viewportTransform().dx(), e0y = view.viewportTransform().dy();
        for (int i = 0; i < 4; ++i) { view.zoomBy(1.0); app.processEvents(); }
        const QPointF p1 = view.mapFromScene(QPointF(0, 0));
        const double e1x = view.viewportTransform().dx(), e1y = view.viewportTransform().dy();
        printf("  [INFO] 连续 4 次无操作变换：映射 (%.2f,%.2f) → (%.2f,%.2f)  生效 dx/dy %.2f/%.2f → %.2f/%.2f\n",
               p0.x(), p0.y(), p1.x(), p1.y(), e0x, e0y, e1x, e1y);
        check(std::hypot(p1.x() - p0.x(), p1.y() - p0.y()) < 0.5,
              QStringLiteral("applyTransform 幂等：反复变换后视图原地不动（无逐次累积漂移）"),
              QStringLiteral("位移 %1px").arg(std::hypot(p1.x() - p0.x(), p1.y() - p0.y()), 0, 'f', 2));
    }

    // ---- 改比例 1:1 → 1:2：屏幕上的图形应缩短一半 ----
    model.setScaleRatio(0.5, false);
    app.processEvents();
    view.fitToRect(QRectF(0.0, 0.0, spec.width, spec.height));   // 变换不变，确保量的是内容变化
    app.processEvents();

    const QImage img2 = renderView(&view, QStringLiteral("build/scale_render_1to2.png"));
    const Box red2  = scan(img2, isRed);
    const Box dark2 = scan(img2, isDark);
    printf("  [INFO] 1:2  红线像素包围盒 %dx%d (n=%ld, 中心 %.1f,%.1f)；深色(图框) %dx%d (n=%ld)\n",
           red2.w(), red2.h(), red2.n, red2.cx(), red2.cy(), dark2.w(), dark2.h(), dark2.n);

    const double ratio = red1.w() ? double(red2.w()) / double(red1.w()) : -1.0;
    printf("  [INFO] 像素长度比 = %.4f（期望 0.5）\n", ratio);
    check(std::fabs(ratio - 0.5) < 0.08,
          QStringLiteral("比例改 1:2 后屏幕上的图形长度减半（像素级实证）"),
          QStringLiteral("比值 %1").arg(ratio, 0, 'f', 4));
    check(std::fabs(red2.cx() - red1.cx()) < 3.0 && std::fabs(red2.cy() - red1.cy()) < 3.0,
          QStringLiteral("缩放以内容包围盒中心为基准（像素中心不漂移）"),
          QStringLiteral("Δ=(%1,%2)").arg(red2.cx() - red1.cx(), 0, 'f', 2)
                                   .arg(red2.cy() - red1.cy(), 0, 'f', 2));

    // ---- 契约：图框代表固定纸张，屏幕上必须纹丝不动 ----
    check(dark1.x0 == dark2.x0 && dark1.y0 == dark2.y0 &&
          dark1.x1 == dark2.x1 && dark1.y1 == dark2.y1,
          QStringLiteral("图框/标题栏在屏幕上纹丝不动（代表固定纸张，不随比例缩放）"),
          QStringLiteral("1:1 bbox=(%1,%2)-(%3,%4)  1:2 bbox=(%5,%6)-(%7,%8)")
              .arg(dark1.x0).arg(dark1.y0).arg(dark1.x1).arg(dark1.y1)
              .arg(dark2.x0).arg(dark2.y0).arg(dark2.x1).arg(dark2.y1));

    // ---- 改回 1:1：图形应回到原来的长度（比例是相对换算，不是单向压缩）----
    model.setScaleRatio(1.0, false);
    app.processEvents();
    view.fitToRect(QRectF(0.0, 0.0, spec.width, spec.height));
    app.processEvents();
    const Box red3 = scan(renderView(&view, QStringLiteral("build/scale_render_back1to1.png")), isRed);
    printf("  [INFO] 回到 1:1 红线宽 %dpx（原始 %dpx）\n", red3.w(), red1.w());
    check(std::abs(red3.w() - red1.w()) <= 3,
          QStringLiteral("比例改回 1:1 后图形长度复原（无累积误差）"),
          QStringLiteral("%1px vs %2px").arg(red3.w()).arg(red1.w()));

    printf("RESULT: %s\n", g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
