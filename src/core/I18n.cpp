#include "I18n.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace SketchCAD {
namespace I18n {

namespace {
// 单一翻译器实例：卸旧装新共用一份，避免多实例叠加导致译文错乱。
QTranslator* g_translator = nullptr;
QString g_activeLang = QStringLiteral("zh-CN");
} // namespace

QString resolveLanguage(const QString& setting) {
    if (setting == QLatin1String("zh-CN")) return QStringLiteral("zh-CN");
    if (setting == QLatin1String("en"))    return QStringLiteral("en");
    // 空 / 未知值：跟随系统。中文系统用源语言（不装翻译器），其余回落英语。
    const QString sys = QLocale::system().name();   // 例 "zh_CN"
    if (sys.startsWith(QLatin1String("zh"), Qt::CaseInsensitive))
        return QStringLiteral("zh-CN");
    return QStringLiteral("en");
}

QString activeLanguage() { return g_activeLang; }

bool installTranslator(const QString& resolvedLang) {
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) return false;                          // 无应用实例（极端早于 main）不做事

    // 卸旧：无论切换到哪个语言，先摘掉现有翻译器（幂等）
    if (g_translator) {
        QCoreApplication::removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }

    g_activeLang = resolveLanguage(resolvedLang);    // 入参兜底再解析一次（防未知值直入）
    if (g_activeLang == QLatin1String("zh-CN"))
        return false;                                // 源语言：串直出，无需翻译器

    // en：装载英文翻译。资源内嵌（部署零附件）→ exe 同目录磁盘文件（测试直读）
    g_translator = new QTranslator(app);
    bool ok = g_translator->load(QStringLiteral(":/i18n/sketchcad_en.qm"));
    if (!ok) {
        const QString exeDir = QCoreApplication::applicationDirPath();
        ok = g_translator->load(QStringLiteral("sketchcad_en.qm"), exeDir);
    }
    if (!ok) {
        delete g_translator;                         // 装载失败：回落源语言，不留半成品
        g_translator = nullptr;
        g_activeLang = QStringLiteral("zh-CN");
        return false;
    }
    QCoreApplication::installTranslator(g_translator);
    return true;
}

} // namespace I18n
} // namespace SketchCAD
