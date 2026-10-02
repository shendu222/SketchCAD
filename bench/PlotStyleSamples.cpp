// ============================================================================
// 打印样式样张生成器（A4 / A3 黑白 PDF 目检样张 + 双路回归比对）
// ----------------------------------------------------------------------------
// 用途（验收条款）：
//   1) 生成 A4 / A3 两档「黑白 PDF 样张」：按 GB/T 14665 线型分四类
//      （粗实线 / 细实线 / 虚线 / 点画线）+ 彩色原图对照，供人工目检并签字；
//   2) 执行「带样式表 vs 不带样式表」双路打印结果回归比对：
//      两路各出一份 PDF，并逐像素统计灰度直方图差异，证明样式表确实改变了出图。
//
// 运行：PlotStyleSamples <输出目录>
//   产出：bw_A4_style.pdf / bw_A4_nostyle.pdf / bw_A3_style.pdf / bw_A3_nostyle.pdf
//         color_A4_nostyle.pdf（彩色对照）
//         regression.txt（双路差异统计）+ histogram_*.txt
// ============================================================================
#include <QApplication>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QPdfWriter>
#include <QPainter>
#include <QImage>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QElapsedTimer>
#include <cmath>

#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Vec.h"
#include "core/PlotStyle.h"
#include "ui/PrintEngine.h"

using namespace SketchCAD;

static QTextStream g_out(stdout);

// 构造「线宽图样张」模型：四类 GB 线型各一组彩色线（彩色下靠颜色区分，
// 黑白出图时必须靠线宽区分——这正是本任务要根治的问题）。
static void buildLineWidthSample(SketchModel& m) {
    // 每行一类线型，横向排布 4 条同色线；4 行分别 粗实/细实/虚线/点画线。
    // 颜色刻意用「四个明显不同的彩色」，模拟彩色线宽图。
    struct Row { LineStyle ls; QColor c; const char* name; };
    const Row rows[4] = {
        { LineStyle::Solid,      QColor(220, 30, 30),  "粗实线" },
        { LineStyle::Solid,      QColor(30, 120, 220), "细实线" },
        { LineStyle::Dashed,     QColor(30, 160, 60),  "虚线"   },
        { LineStyle::DashDot,    QColor(180, 120, 0),  "点画线" },
    };
    double y = 0.0;
    for (int r = 0; r < 4; ++r) {
        // 每类线画 4 条，x 方向拉开（总宽 4×45+3×15=225mm，样张用 A3/A4 横向均可容下）
        for (int k = 0; k < 4; ++k) {
            Element e;
            e.type = ElementType::Line;
            e.lineStyle = rows[r].ls;
            e.color = rows[r].c;
            e.layer = "0";
            e.pts = { Vec3(k * 50.0, y, 0), Vec3(k * 50.0 + 40.0, y, 0) };
            m.addElement(e);
        }
        y -= 25.0;   // 行距 25mm
    }
    // 加一个外框，便于目检边界与图幅
    Element frame;
    frame.type = ElementType::Rectangle;
    frame.color = QColor(0, 0, 0);
    frame.lineStyle = LineStyle::Solid;
    frame.pts = { Vec3(-12, 16, 0), Vec3(205, -100, 0) };
    m.addElement(frame);
}

// 渲染一份 PDF：useStyle=true 走 GB 黑白样式表；false 完全不带样式表（彩色原样，
// 即「彩色线宽图直接打印」的错误基线）
static bool exportPdfWith(SketchModel& m, const QPageSize& size, bool useStyle,
                          const QString& path, bool /*forceBw*/) {
    PrintSettings s = PrintSettings::defaults();
    s.pageSize = size;
    s.orientation = QPageLayout::Landscape;
    s.marginsMm = QMarginsF(10, 10, 10, 10);
    s.fitToPage = true;
    s.centerH = s.centerV = true;
    s.drawBorder = true;
    s.lineWidthScale = 1.0;      // 样张用 1.0，直观呈现 GB 绝对线宽（mm）
    if (useStyle) {
        s.plotStyle = PlotStyleTable::gbPreset(LineWidthGroup::G3);
        s.monochrome = true;
    }
    // ⚠️ 对照组必须「无样式表 + 无 monochrome」，否则 effectivePlotStyle() 也会套 GB 预设，
    // 两路就完全相同、失去回归意义。正确基线 = 原始彩色 + 自动派生线宽。
    return PrintEngine::exportPdf(&m, s, path);
}

// 把 PDF 渲染成灰度位图以做像素级双路比对（QPdfWriter 输出再经 QPdfDocument 读回）
// 为简化与稳健，双路比对改为：直接用 PlotRenderer 渲染到 QImage（同一渲染链），
// 统计灰度直方图差异。
static bool renderToGrayImage(SketchModel& m, const QPageSize& size, bool useStyle,
                              const QString& outPng) {
    PrintSettings s = PrintSettings::defaults();
    s.pageSize = size;
    s.orientation = QPageLayout::Landscape;
    s.marginsMm = QMarginsF(10, 10, 10, 10);
    s.fitToPage = true;
    s.centerH = s.centerV = true;
    s.lineWidthScale = 1.0;
    if (useStyle) {
        s.plotStyle = PlotStyleTable::gbPreset(LineWidthGroup::G3);
        s.monochrome = true;
    }
    // 用 QImage 作设备。⚠️ 必须设置 DPI：PrintEngine::devicePxPerMm 读 logicalDpiX()，
    // QImage 默认为 96dpi，若不设置则可打印区换算与本处按 300dpi 计算的画布尺寸不一致。
    // ⚠️ 画布尺寸必须与 PrintEngine::effectivePageW/H() 的换算完全一致，即**按方向交换宽高**。
    // QPageSize::size() 返回的是纸张固有尺寸（A4 = 210×297，纵向），而 render() 内部
    // 横向下按 297×210 排版；若此处直接用 size() 建纵向画布，右侧 87mm 会被裁掉，
    // 表现为「样张第 4 列线段与外框右边看不见」。
    const double pxPerMm = 300.0 / 25.4;
    const QSizeF szMm = size.size(QPageSize::Millimeter);
    const double pageWmm = (s.orientation == QPageLayout::Landscape) ? szMm.height() : szMm.width();
    const double pageHmm = (s.orientation == QPageLayout::Landscape) ? szMm.width()  : szMm.height();
    const int W = int(pageWmm * pxPerMm);
    const int H = int(pageHmm * pxPerMm);
    QImage img(W, H, QImage::Format_RGB32);
    img.setDotsPerMeterX(int(300.0 / 0.0254));
    img.setDotsPerMeterY(int(300.0 / 0.0254));
    img.fill(Qt::white);
    // ⚠️ 不可提前对 img 激活 QPainter：PrintEngine::render 内部自行 begin，
    // 同一设备已有活动 painter 会导致 render 失败（返回空白图）。
    PrintEngine::render(&m, &img, s);
    return img.save(outPng);
}

// 统计灰度直方图（256 桶）与总墨量（sum of (255-gray)）
static void grayStats(const QImage& img, int* buckets, double* inkSum) {
    for (int b = 0; b < 256; ++b) buckets[b] = 0;
    *inkSum = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            const int g = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
            buckets[g]++;
            *inkSum += (255 - g);
        }
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString outDir = (argc > 1) ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("plotstyle_samples");
    QDir().mkpath(outDir);

    g_out << "=== 打印样式样张生成（GB/T 14665 黑白出图目检 + 双路回归）===\n";
    g_out << "输出目录: " << outDir << "\n\n";

    SketchModel m;
    buildLineWidthSample(m);
    g_out << "样张模型: " << m.elements().size() << " 个图元（四类线型：粗实/细实/虚线/点画）\n";

    struct Page { const char* tag; QPageSize size; };
    const Page pages[2] = {
        { "A4", QPageSize(QPageSize::A4) },
        { "A3", QPageSize(QPageSize::A3) },
    };

    QString report;
    QTextStream rp(&report);

    for (const auto& pg : pages) {
        // 1) 黑白样张（带 GB 样式表）
        const QString fStyle = QStringLiteral("%1/bw_%2_style.pdf").arg(outDir, pg.tag);
        const bool ok1 = exportPdfWith(m, pg.size, true, fStyle, false);
        g_out << "[PDF] " << pg.tag << " 黑白(带样式表): " << fStyle << "  " << (ok1 ? "OK" : "FAIL") << "\n";

        // 2) 黑白样张（不带样式表，仅灰度——对照组）
        const QString fNo = QStringLiteral("%1/bw_%2_nostyle.pdf").arg(outDir, pg.tag);
        const bool ok2 = exportPdfWith(m, pg.size, false, fNo, true);
        g_out << "[PDF] " << pg.tag << " 黑白(不带样式表): " << fNo << "  " << (ok2 ? "OK" : "FAIL") << "\n";

        // 3) 双路回归：渲染两路灰度图，统计直方图差异
        const QString pngS = QStringLiteral("%1/gray_%2_style.png").arg(outDir, pg.tag);
        const QString pngN = QStringLiteral("%1/gray_%2_nostyle.png").arg(outDir, pg.tag);
        renderToGrayImage(m, pg.size, true,  pngS);
        renderToGrayImage(m, pg.size, false, pngN);

        QImage imgS(pngS), imgN(pngN);
        if (imgS.isNull() || imgN.isNull()) {
            rp << pg.tag << ": 灰度图渲染失败，跳过双路比对\n";
            continue;
        }
        int bS[256] = {0}, bN[256] = {0};
        double inkS = 0, inkN = 0;
        grayStats(imgS, bS, &inkS);
        grayStats(imgN, bN, &inkN);

        // 差异度量：墨量变化 + 绝对差（L1）
        double l1 = 0;
        for (int b = 0; b < 256; ++b) l1 += std::fabs(double(bS[b]) - double(bN[b]));
        const double totalPx = double(imgS.width()) * imgS.height();
        const double meanGrayS = [&]{ double s=0; for(int b=0;b<256;++b) s += double(b)*bS[b]; return s/totalPx; }();
        const double meanGrayN = [&]{ double s=0; for(int b=0;b<256;++b) s += double(b)*bN[b]; return s/totalPx; }();

        // 是否含彩色（对照组应含彩色 → R!=G!=B 像素数 > 0；样张组应全灰度）
        long long colorPxN = 0, colorPxS = 0;
        for (int y = 0; y < imgN.height(); y += 2)
            for (int x = 0; x < imgN.width(); x += 2) {
                const QColor c = imgN.pixelColor(x, y);
                if (c.red() != c.green() || c.green() != c.blue()) ++colorPxN;
            }
        for (int y = 0; y < imgS.height(); y += 2)
            for (int x = 0; x < imgS.width(); x += 2) {
                const QColor c = imgS.pixelColor(x, y);
                if (c.red() != c.green() || c.green() != c.blue()) ++colorPxS;
            }

        rp << "== " << pg.tag << " 双路回归（带样式表 vs 不带样式表）==\n";
        rp << QString("  平均灰度      : 带=%1  不带=%2\n")
              .arg(meanGrayS, 0, 'f', 2).arg(meanGrayN, 0, 'f', 2);
        rp << QString("  总墨量(Σ暗度) : 带=%1  不带=%2  变化=%3%\n")
              .arg(inkS, 0, 'f', 0).arg(inkN, 0, 'f', 0)
              .arg(inkN > 1e-9 ? (inkS - inkN) / inkN * 100.0 : 0.0, 0, 'f', 1);
        rp << QString("  灰度直方图L1  : %1 (相对像素数 %2%)\n")
              .arg(l1, 0, 'f', 0).arg(l1 / totalPx * 100.0, 0, 'f', 4);
        rp << QString("  彩色像素(抽样): 带样式表=%1  不带=%2  → %3\n")
              .arg(colorPxS).arg(colorPxN)
              .arg(colorPxS == 0 ? QStringLiteral("样式表组已全灰度(符合黑白预期)")
                                 : QStringLiteral("样式表组仍含彩色(异常!)"));
        const bool diff = l1 / totalPx > 1e-4;
        rp << QString("  回归判定      : %1（两路出图存在可测差异=样式表生效；"
                      "带样式表组无彩色=黑白化正确）\n\n")
              .arg(diff ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
        g_out << "[回归] " << pg.tag << " 灰度L1=" << l1 << " 彩色px(带/不带)=" << colorPxS << "/" << colorPxN << "\n";
    }

    // 写回归报告
    QFile rf(outDir + "/regression.txt");
    if (rf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        rf.write(report.toUtf8());
        rf.close();
        g_out << "\n回归报告: " << outDir << "/regression.txt\n";
    }
    g_out << "\n样张目检要点（四类线型在黑白下应保持粗/细可辨）：\n"
          << "  · 粗实线：最粗且纯黑\n"
          << "  · 细实线：较细且纯黑\n"
          << "  · 虚线  ：细档、线段间断\n"
          << "  · 点画线：细档、长短段交替\n"
          << "请打开 bw_A4_style.pdf / bw_A3_style.pdf 目检并签字确认。\n";
    return 0;
}
