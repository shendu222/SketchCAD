#include "I18n.h"
#include "I18nDict.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace SketchCAD {
namespace I18n {

namespace {
// 单一翻译器实例：卸旧装新共用一份，避免多实例叠加导致译文错乱。
QTranslator* g_translator = nullptr;
QString g_activeLang = QStringLiteral("zh-CN");

const QChar kCjkMin = QChar(0x3400);
const QChar kCjkMax = QChar(0x9fff);

inline bool isCjk(const QChar c) {
    return c >= kCjkMin && c <= kCjkMax;
}

bool hasCjk(const QString& s) {
    for (const QChar c : s)
        if (isCjk(c)) return true;
    return false;
}

// 术语级最长匹配清洗：把任意文本里残留的中文 UI 词替换为英文。
// 精确词典未命中时用它兜底，覆盖运行时拼接、数字/占位符已代入的模板、
// 以及「英文模板 + 中文术语实参」这类混合串。
QString scrubByTerms(const QString& text) {
    if (!hasCjk(text)) return text;                 // 纯英文/ASCII 直接返回
    const auto& terms = Runtime::termMap();
    const int maxLen = Runtime::maxTermLength();
    QString out;
    out.reserve(text.size() * 2);
    int i = 0;
    const int n = text.size();
    while (i < n) {
        if (!isCjk(text.at(i))) { out.append(text.at(i)); ++i; continue; }
        bool hit = false;
        const int room = qMin(maxLen, n - i);
        for (int len = room; len >= 1; --len) {
            auto it = terms.constFind(text.mid(i, len));
            if (it == terms.constEnd()) continue;
            const QString& en = it.value();
            if (!en.isEmpty()) {
                // 词间留空格；紧邻 ASCII 标点时由后置规整统一收紧
                if (!out.isEmpty() && !out.at(out.size() - 1).isSpace())
                    out.append(QLatin1Char(' '));
                out.append(en);
                if (!out.isEmpty() && !out.at(out.size() - 1).isSpace())
                    out.append(QLatin1Char(' '));
            }
            i += len;
            hit = true;
            break;
        }
        if (!hit) { out.append(text.at(i)); ++i; }   // 用户数据里的生僻汉字：保留
    }
    return out.simplified();
}
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

QString t(const QString& zhText) {
    if (g_activeLang != QLatin1String("en")) return zhText;   // 源语言：逐字不变
    if (zhText.isEmpty() || !hasCjk(zhText)) return zhText;   // 已译英文 / ASCII 直通
    const auto& dict = Runtime::exactMap();
    auto it = dict.constFind(zhText);
    if (it != dict.constEnd()) return it.value();
    return scrubByTerms(zhText);
}

QString tUtf8(const char* zhUtf8) {
    if (!zhUtf8) return QString();
    return t(QString::fromUtf8(zhUtf8));
}

QStringList dictionaryKeys() {
    return Runtime::exactMap().keys();
}

QString termScrub(const QString& text) {
    if (g_activeLang != QLatin1String("en")) return text;
    return scrubByTerms(text);
}

void setRuntimeLanguageForTest(const QString& resolvedLang) {
    g_activeLang = resolveLanguage(resolvedLang);
}

} // namespace I18n
} // namespace SketchCAD
