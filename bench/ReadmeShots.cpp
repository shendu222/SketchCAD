// ============================================================================
// README 配图生成器（程序离屏渲染真实图纸 → docs/images/*.png）
// ----------------------------------------------------------------------------
// 用途：README.md「图文并茂」配图。全部图由本程序的真实渲染链
// （模板生成器 → SketchModel → PrintEngine::render → QImage）产出，
// 与用户在 GUI 中打印/预览所见为同一渲染路径——不是手绘示意图。
//
// 运行：ReadmeShots <输出目录>
//   产出：flange_views.png        法兰盘三面图（带颈平焊，A4 默认参数）
//         housing_views.png       箱体三视图（剖视 + 尺寸，A3 默认参数）
//         bearing_views.png       轴承座三面图（默认参数）
//         flangehub_views.png     法兰轮毂零件图（A4 默认参数）
//         floorplan_views.png     户型平面图（9200×11756 默认参数）
//         aircraft_threeview.png  飞机三面图（默认参数）
// ⚠️ 本工具不参与 dist 打包，仅供文档出图；GUI 截图与 GB 黑白样张
//    由人工截图 / PlotStyleSamples 产出，不在此生成。
// ============================================================================
#include <QApplication>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QImage>
#include <QDir>
#include <QTextStream>

#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Vec.h"
#include "core/FlangeViews.h"
#include "core/HousingViews.h"
#include "core/BearingViews.h"
#include "core/FlangeHubViews.h"
#include "core/FloorPlanViews.h"
#include "core/AircraftViews.h"
#include "ui/PrintEngine.h"

using namespace SketchCAD;

static QTextStream g_out(stdout);

// 渲染模型到 PNG（白底、彩色、fitToPage 居中）。
// ⚠️ 与 PlotStyleSamples 相同的三个坑（详见其源码注释）：
//   1) QImage 必须 setDotsPerMeter——PrintEngine::devicePxPerMm 读 logicalDpi，
//      默认 96dpi 会导致可打印区换算与画布尺寸不一致；
//   2) 画布宽高必须**按方向交换**——QPageSize::size() 返回纸张固有尺寸
//      （纵向基态），横向时直接用会右侧静默出界被裁；
//   3) 不可提前对 img 激活 QPainter——render 内部自行 begin。
static bool renderToPng(SketchModel& m, const QPageSize& size,
                        QPageLayout::Orientation ori, const QString& path) {
    PrintSettings s = PrintSettings::defaults();
    s.pageSize = size;
    s.orientation = ori;
    s.marginsMm = QMarginsF(10, 10, 10, 10);
    s.fitToPage = true;
    s.centerH = s.centerV = true;
    s.drawBorder = false;          // 纯图纸展示，不叠加打印边框
    s.lineWidthScale = 1.0;

    const double dpi = 200.0;      // README 配图 200dpi：清晰且体积可控
    const double pxPerMm = dpi / 25.4;
    const QSizeF szMm = size.size(QPageSize::Millimeter);
    const double pageWmm = (ori == QPageLayout::Landscape) ? szMm.height() : szMm.width();
    const double pageHmm = (ori == QPageLayout::Landscape) ? szMm.width()  : szMm.height();
    const int W = int(pageWmm * pxPerMm);
    const int H = int(pageHmm * pxPerMm);

    QImage img(W, H, QImage::Format_RGB32);
    img.setDotsPerMeterX(int(dpi / 0.0254));
    img.setDotsPerMeterY(int(dpi / 0.0254));
    img.fill(Qt::white);
    PrintEngine::render(&m, &img, s);
    return img.save(path);
}

// 生成器图元装入模型（模板返回的 elems 尚未写入模型）
static void loadElems(SketchModel& m, std::vector<Element> elems) {
    for (auto& e : elems) m.addElement(e);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString outDir = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                      : QStringLiteral("docs/images");
    QDir().mkpath(outDir);

    g_out << "=== README 配图生成（模板生成器 → PrintEngine 真实渲染）===\n";
    g_out << "输出目录: " << outDir << "\n\n";

    int ok = 0, fail = 0;
    auto emitShot = [&](const char* tag, SketchModel& m, const QPageSize& size,
                        QPageLayout::Orientation ori) {
        const QString path = QStringLiteral("%1/%2.png").arg(outDir, QString::fromLatin1(tag));
        const bool good = renderToPng(m, size, ori, path);
        g_out << "[" << tag << "] " << m.elements().size() << " 图元 → "
              << path << "  " << (good ? "OK" : "FAIL") << "\n";
        (good ? ok : fail)++;
    };

    // 1) 法兰盘三面图（带颈平焊法兰，A4 横向默认参数）
    {
        SketchModel m;
        FlangeParams p = FlangeParams::forA4();
        p.boreTol = "H8";                       // 展示公差标注能力
        p.thicknessTol = QStringLiteral("±0.2");
        loadElems(m, buildFlangeViews(p, nullptr).elems);
        emitShot("flange_views", m, QPageSize(QPageSize::A4), QPageLayout::Landscape);
    }

    // 2) 箱体三视图（剖视表达 + GB 材料 19 值剖面线，A3 默认参数）
    {
        SketchModel m;
        loadElems(m, buildHousingViews(HousingParams::forA3(), nullptr).elems);
        emitShot("housing_views", m, QPageSize(QPageSize::A3), QPageLayout::Landscape);
    }

    // 3) 轴承座三面图（默认参数）
    {
        SketchModel m;
        loadElems(m, buildBearingViews(BearingParams{}, nullptr).elems);
        emitShot("bearing_views", m, QPageSize(QPageSize::A4), QPageLayout::Landscape);
    }

    // 4) 法兰轮毂零件图（A4 默认参数）
    {
        SketchModel m;
        loadElems(m, buildFlangeHubViews(FlangeHubParams::forA4(), nullptr).elems);
        emitShot("flangehub_views", m, QPageSize(QPageSize::A4), QPageLayout::Landscape);
    }

    // 5) 户型平面图（9200×11756 默认参数，竖向图幅）
    {
        SketchModel m;
        loadElems(m, buildFloorPlanViews(FloorPlanParams{}, nullptr).elems);
        emitShot("floorplan_views", m, QPageSize(QPageSize::A4), QPageLayout::Portrait);
    }

    // 6) 飞机三面图（默认参数：机身 38.9m / 翼展 35.8m，fitToPage 自适配）
    {
        SketchModel m;
        loadElems(m, aircraft::buildThreeViews(aircraft::AircraftViewsParams{}, nullptr));
        emitShot("aircraft_threeview", m, QPageSize(QPageSize::A4), QPageLayout::Landscape);
    }

    g_out << QString("\n完成: %1 成功 / %2 失败\n").arg(ok).arg(fail);
    return fail == 0 ? 0 : 1;
}
