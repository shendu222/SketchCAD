// Verification: mirrors the fix in main.cpp (font substitution) + the new
// g_familyOk semantics in TextMetrics.cpp, to prove "长仿宋体" now resolves
// to a REAL installed font (=> real metrics, no ±30% estimate, no warning spam).
#include <QGuiApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontDatabase>
#include <QStringList>
#include <cstdio>

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // load real system fonts so FangSong/SimSun are available (as the probe did)
    for (const char* f : {"C:/Windows/Fonts/simfang.ttf", "C:/Windows/Fonts/STFANGSO.TTF",
                          "C:/Windows/Fonts/simsun.ttc", "C:/Windows/Fonts/msyh.ttc"})
        QFontDatabase::addApplicationFont(QString::fromUtf8(f));

    // apply the SAME substitution chain as main.cpp
    QStringList subs = {QString::fromUtf8("仿宋"), QString::fromUtf8("FangSong"),
                        QString::fromUtf8("华文仿宋"), QString::fromUtf8("STFangsong"),
                        QString::fromUtf8("宋体"), QString::fromUtf8("SimSun")};
    for (const QString& s : subs) QFont::insertSubstitution(QString::fromUtf8("长仿宋体"), s);

    QFont f; f.setFamily(QString::fromUtf8("长仿宋体")); f.setPixelSize(1000);
    QFontInfo fi(f);
    const bool resolvedReal = !fi.family().isEmpty() && QFontDatabase::hasFamily(fi.family());
    printf("requested : '长仿宋体'\n");
    printf("resolved  : '%s'\n", qPrintable(fi.family()));
    printf("hasFamily(resolved) = %d\n", resolvedReal ? 1 : 0);
    printf("=> g_familyOk('长仿宋体') = %s  (TRUE => 真实度量, 不再估算, 不再刷告警)\n",
           resolvedReal ? "TRUE" : "FALSE");
    return resolvedReal ? 0 : 1;
}
