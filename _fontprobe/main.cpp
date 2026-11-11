// Headless rendering probe for GB/T 14691 LongFangSong default style.
// Faithfully replicates GraphicsView::paintText / paintDimension font setup:
//   f.setPixelSize(max(8, round(mm * zoom)))
//   if (!family.isEmpty()) f.setFamily(family)
//   f.setStretch(round(widthFactor * 100))   // LongFangSong: 0.707 -> 71
// Real system font files are loaded explicitly via QFontDatabase so CJK renders.
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <functional>
#include <cstdio>
#include <cmath>

static const int ZOOM = 20;

QFont makeFont(const QString& family, double wf, bool bold, double mm) {
    QFont f;
    f.setPixelSize(std::max(8, (int)std::lround(mm * ZOOM)));
    if (!family.isEmpty()) f.setFamily(family);
    f.setBold(bold);
    if (wf > 0) f.setStretch((int)std::lround(wf * 100));
    return f;
}

QRect tightBounds(const QString& text, const QFont& f) {
    QFontMetricsF fm(f);
    int w = (int)std::ceil(fm.horizontalAdvance(text)) + 8;
    int h = (int)std::ceil(fm.height()) + 8;
    if (w < 4) w = 4; if (h < 4) h = 4;
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(0);
    { QPainter p(&img); p.setFont(f); p.setPen(Qt::black);
      p.drawText(img.rect(), Qt::AlignLeft | Qt::AlignTop, text); }
    int minx = img.width(), miny = img.height(), maxx = -1, maxy = -1;
    for (int y = 0; y < img.height(); ++y) {
        const quint32* row = (const quint32*)img.constScanLine(y);
        for (int x = 0; x < img.width(); ++x)
            if ((row[x] & 0xFF000000u) != 0) {
                if (x < minx) minx = x; if (x > maxx) maxx = x;
                if (y < miny) miny = y; if (y > maxy) maxy = y; } }
    if (maxx < 0) return QRect();
    return QRect(minx, miny, maxx - minx + 1, maxy - miny + 1);
}
double aspectOf(const QString& ch, const QFont& f) {
    QRect r = tightBounds(ch, f);
    return r.isEmpty() ? 0.0 : (double)r.width() / (double)r.height();
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    for (const QString& f : {
         "C:/Windows/Fonts/simfang.ttf", "C:/Windows/Fonts/STFANGSO.TTF",
         "C:/Windows/Fonts/STSONG.TTF", "C:/Windows/Fonts/simsun.ttc",
         "C:/Windows/Fonts/simsunb.ttf", "C:/Windows/Fonts/msyh.ttc",
         "C:/Windows/Fonts/msyhbd.ttc", "C:/Windows/Fonts/msyhl.ttc" })
        if (QFontDatabase::addApplicationFont(f) < 0) printf("WARN cannot load %s\n", qPrintable(f));

    const QString LFS = QString::fromUtf8("长仿宋体");
    const double  WF  = 0.70710678;
    printf("=== Resolve '长仿宋体' (app requests this) ===\n");
    { QFont fa = makeFont(LFS, WF, false, 5.0); QFontInfo ia(fa);
      printf("req '长仿宋体' stretch=71 -> resolved='%s' exactMatch=%d\n",
             qPrintable(ia.family()), ia.exactMatch()?1:0); }

    printf("\n=== Glyph aspect width/height of '中' (GB/T14691 target ~0.707) ===\n");
    for (auto& c : std::initializer_list<std::pair<const char*,std::pair<QString,double>>>{
         {"LongFangSong req(长仿宋体,stretch71)", {LFS, WF}},
         {"FangSong stretch71", {QString::fromUtf8("FangSong"), WF}},
         {"STFangsong stretch71", {QString::fromUtf8("STFangsong"), WF}},
         {"SimSun stretch71", {QString::fromUtf8("SimSun"), WF}},
         {"Microsoft YaHei stretch71", {QString::fromUtf8("Microsoft YaHei"), WF}},
         {"长仿宋体 stretch100", {LFS, 0.0}},
         {"FangSong stretch100 (correct)", {QString::fromUtf8("FangSong"), 0.0}},
         {"SimSun stretch100", {QString::fromUtf8("SimSun"), 0.0}} }) {
        QFont f = makeFont(c.second.first, c.second.second, false, 5.0);
        QFontInfo fi(f);
        printf("  %-34s resolved='%s' aspect(中)=%.3f\n",
               c.first, qPrintable(fi.family()), aspectOf(QString::fromUtf8("中"), f));
    }

    const QStringList charSets = {
        QString::fromUtf8("技术要求 长仿宋体示例"),
        QString::fromUtf8("ABCdef Sample Text"),
        QString::fromUtf8("1234567890"),
        QString::fromUtf8("Φ±Δ℃①②③°≈≠") };
    const QVector<double> sizes = {3.5, 5.0, 7.0};

    auto buildComposite = [&](const QString& title,
                              const std::function<QFont(double,bool)>& fontFor) {
        // compute lines first to size image
        struct Ln { QString lbl; QString txt; QFont f; };
        QVector<Ln> lines;
        for (const QString& cs : charSets)
            for (double mm : sizes)
                for (bool bold : {false, true}) {
                    QFont f = fontFor(mm, bold);
                    QFontInfo fi(f);
                    QString lbl = QString::fromUtf8("%1 %2mm %3 [%4]").arg(cs.left(6))
                                  .arg(mm,0,'f',1).arg(bold?QStringLiteral("B"):QStringLiteral(""))
                                  .arg(fi.family());
                    lines.push_back({lbl, cs, f});
                }
        int W = 260 + 1750 + 40;
        int H = 70 + lines.size() * 210 + 20;
        QImage img(W, H, QImage::Format_ARGB32); img.fill(QColor(255,255,255));
        QPainter p(&img);
        QFont t; t.setPixelSize(22); t.setBold(true);
        p.setFont(t); p.setPen(Qt::black);
        p.drawText(12, 30, title);
        int y = 60;
        for (const Ln& ln : lines) {
            p.setFont(t); p.setPen(Qt::black); p.drawText(12, y + 20, ln.lbl);
            p.setFont(ln.f); p.setPen(Qt::black);
            p.drawText(260, y + 36 + ln.f.pixelSize(), ln.txt);
            y += 210;
        }
        p.end(); return img;
    };

    buildComposite(QString::fromUtf8("A. LongFangSong样式(family=长仿宋体,stretch=71) — 新建文字/标题栏/图框"),
                   [&](double mm,bool b){ return makeFont(LFS, WF, b, mm); })
        .save(QString::fromUtf8("_fontprobe/probe_A_longfangsong.png"));

    buildComposite(QString::fromUtf8("B. 尺寸标注当前实现(paintDimension: 无setFamily/无setStretch, 用系统默认UI字体代表)"),
                   [&](double mm,bool b){ return makeFont(QString::fromUtf8("Microsoft YaHei"), 0.0, b, mm); })
        .save(QString::fromUtf8("_fontprobe/probe_B_dimension_current.png"));

    buildComposite(QString::fromUtf8("C. 尺寸标注【若】接入LongFangSong(family=长仿宋体,stretch=71)对照"),
                   [&](double mm,bool b){ return makeFont(LFS, WF, b, mm); })
        .save(QString::fromUtf8("_fontprobe/probe_C_dimension_lfs.png"));

    // D: same Chinese with each REAL loaded family (stretch 71, 5mm)
    {
        const int pad = 12;
        const QString sample = QString::fromUtf8("技术要求 长仿宋体示例 ABC 123");
        QStringList fams = {LFS, QString::fromUtf8("FangSong"),
                            QString::fromUtf8("STFangsong"), QString::fromUtf8("SimSun"),
                            QString::fromUtf8("Microsoft YaHei")};
        int W = 1250, H = pad + 44 + fams.size() * 140;
        QImage img(W, H, QImage::Format_ARGB32); img.fill(QColor(255,255,255));
        QPainter p(&img); QFont t; t.setPixelSize(24); t.setBold(true);
        p.setFont(t); p.setPen(Qt::black);
        p.drawText(pad, pad+20, QString::fromUtf8("D. 同一段文字在不同family下的回退(stretch=71,5mm)"));
        int y = pad + 50;
        for (const QString& fam : fams) {
            QFont f = makeFont(fam, WF, false, 5.0); QFontInfo fi(f);
            p.setFont(t); p.setPen(Qt::black);
            p.drawText(pad, y+22, QString::fromUtf8("request='%1' -> '%2'").arg(fam).arg(fi.family()));
            p.setFont(f); p.setPen(Qt::black);
            p.drawText(420, y + 30 + f.pixelSize(), sample);
            y += 140;
        }
        p.end(); img.save(QString::fromUtf8("_fontprobe/probe_D_fallback_compare.png"));
    }

    // E: over-condensation demonstration: correct (FangSong stretch100=0.707) vs current LFS (stretch71=0.511)
    {
        const int pad = 12;
        const QString sample = QString::fromUtf8("机械设计 长仿宋体 技术要求 标注示例");
        int W = 1300, H = pad + 44 + 3 * 150;
        QImage img(W, H, QImage::Format_ARGB32); img.fill(QColor(255,255,255));
        QPainter p(&img); QFont t; t.setPixelSize(24); t.setBold(true);
        p.setFont(t); p.setPen(Qt::black);
        p.drawText(pad, pad+20, QString::fromUtf8("E. 字宽过窄演示：正确参考(仿宋 stretch100, aspect≈0.707) vs 当前LongFangSong(仿宋 stretch71, aspect≈0.511)"));
        int y = pad + 50;
        struct Row { QString cap; QFont f; };
        QVector<Row> rows = {
            {QString::fromUtf8("① 当前 LongFangSong: family=长仿宋体→FangSong, stretch=71 (过窄)"), makeFont(LFS, WF, false, 7.0)},
            {QString::fromUtf8("② 正确回退参考: family=FangSong, stretch=100 (aspect≈0.707, 符合国标)"), makeFont(QString::fromUtf8("FangSong"), 0.0, false, 7.0)},
            {QString::fromUtf8("③ 若真用宽体宋体 SimSun 作底+stretch71 (aspect≈0.707)"), makeFont(QString::fromUtf8("SimSun"), WF, false, 7.0)},
        };
        for (const Row& r : rows) {
            p.setFont(t); p.setPen(Qt::black); p.drawText(pad, y+22, r.cap);
            p.setFont(r.f); p.setPen(Qt::black);
            p.drawText(40, y + 36 + r.f.pixelSize(), sample);
            y += 150;
        }
        p.end(); img.save(QString::fromUtf8("_fontprobe/probe_E_overcondense.png"));
    }

    printf("\nSaved 5 PNGs.\n");
    return 0;
}
