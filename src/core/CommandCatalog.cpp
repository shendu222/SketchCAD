#include "CommandCatalog.h"
#include "CommandCatalogConfig.h"
#include "core/I18n.h"
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <algorithm>

namespace SketchCAD {

// ============================================================
// 命令元数据目录：JSON 驱动
// ============================================================
// CommandCatalog.cpp 不再硬编码命令条目。启动时读取 config/command_catalog.json，
// 遍历 commands[].parameters[] 并以 add(...) 的语义逐条构建 CommandInfo。
// help / AI 提示词 / 补全 / 面板仍只面对 CommandCatalog API。
// ============================================================

namespace {

struct CatalogCache {
    QVector<CommandInfo> table;
    QString lang;
    QString error;
    bool loaded = false;
};

CatalogCache& cache() {
    static CatalogCache c;
    return c;
}

QString stringParam(const CatalogCommandDef& cmd, const QString& name) {
    const CatalogParameterValue* p = CommandCatalogConfig::findParameter(cmd, name);
    return p ? p->value.toString() : QString();
}

QStringList listParam(const CatalogCommandDef& cmd, const QString& name) {
    const CatalogParameterValue* p = CommandCatalogConfig::findParameter(cmd, name);
    if (!p) return {};
    QStringList out;
    for (const QJsonValue& v : p->value.toArray())
        out << v.toString();
    return out;
}

QVector<CommandInfo> buildTable(const CatalogDocument& doc) {
    QVector<CommandInfo> t;

    // 与旧静态版本完全相同的 add 语义：兜底、翻译、字段顺序均保持一致。
    auto add = [&t](const QString& name, const QString& zh, const QString& cat,
                    const QString& sum, const QString& usage, const QString& params,
                    const QString& ex, const QStringList& rel, const QString& sc) {
        CommandInfo ci;
        ci.name = name;                       // 命令 id 恒 ASCII，绝不翻译
        ci.zhName = SketchCAD::I18n::t(zh);
        ci.category = SketchCAD::I18n::t(cat);
        ci.summary = SketchCAD::I18n::t(sum);
        ci.usage = SketchCAD::I18n::t(usage);
        ci.params = SketchCAD::I18n::t(params);
        ci.example = SketchCAD::I18n::t(ex);

        if (ci.params.isEmpty())
            ci.params = SketchCAD::I18n::t(QStringLiteral("按用法顺序输入参数；无参命令直接回车。"));
        if (ci.example.isEmpty())
            ci.example = ci.usage.isEmpty() ? ci.name : ci.usage;
        if (ci.usage.isEmpty())
            ci.usage = name;

        ci.related = rel;
        ci.shortcut = sc;
        t.append(ci);
    };

    // 循环遍历 JSON 配置中的命令与参数，自动完成所有 add 注册。
    for (const CatalogCommandDef& cmd : doc.commands) {
        add(stringParam(cmd, QStringLiteral("name")),
            stringParam(cmd, QStringLiteral("zhName")),
            stringParam(cmd, QStringLiteral("category")),
            stringParam(cmd, QStringLiteral("summary")),
            stringParam(cmd, QStringLiteral("usage")),
            stringParam(cmd, QStringLiteral("params")),
            stringParam(cmd, QStringLiteral("example")),
            listParam(cmd, QStringLiteral("related")),
            stringParam(cmd, QStringLiteral("shortcut")));
    }
    return t;
}

bool reloadFromDisk(QString* error) {
    CommandCatalogConfig cfg;
    QString err;
    const QString path = CommandCatalogConfig::defaultPath();
    if (!cfg.load(path, &err)) {
        cache().error = err;
        cache().loaded = false;
        cache().table.clear();
        if (error) *error = err;
        qCritical().noquote() << err;
        return false;
    }
    cache().table = buildTable(cfg.document());
    cache().lang = SketchCAD::I18n::activeLanguage();
    cache().error.clear();
    cache().loaded = true;
    if (error) error->clear();
    return true;
}

} // namespace

const QVector<CommandInfo>& CommandCatalog::entries() {
    const QString lang = SketchCAD::I18n::activeLanguage();
    if (cache().loaded && cache().lang == lang) return cache().table;

    if (!reloadFromDisk(nullptr)) {
        // 配置错误时返回空表：命令仍可由 Commands.cpp 执行，但 help/AI 会明确
        // 通过 lastLoadError() 暴露失败原因，而不是静默使用过期硬编码表。
        cache().lang = lang;
        return cache().table;
    }
    return cache().table;
}

bool CommandCatalog::reload(QString* error) {
    return reloadFromDisk(error);
}

QString CommandCatalog::lastLoadError() {
    return cache().error;
}

QString CommandCatalog::configPath() {
    return CommandCatalogConfig::defaultPath();
}

QStringList CommandCatalog::names() {
    QStringList out;
    out.reserve(entries().size());
    for (const CommandInfo& ci : entries()) out << ci.name;
    return out;
}

const CommandInfo* CommandCatalog::find(const QString& name) {
    const QString key = name.toLower();
    for (const CommandInfo& ci : entries())
        if (ci.name == key) return &ci;
    return nullptr;
}

QString CommandCatalog::categoryOf(const QString& name) {
    if (const CommandInfo* ci = find(name)) return ci->category;
    const QString n = name.toLower();
    if (n.startsWith(QLatin1String("line_")) || n.startsWith(QLatin1String("spline_")) ||
        n.startsWith(QLatin1String("ellipse_")) || n.startsWith(QLatin1String("polyline_")) ||
        n == QLatin1String("polyline") || n == QLatin1String("freehand_line"))
        return QStringLiteral("绘图");
    if (n.startsWith(QLatin1String("select_")) || n.startsWith(QLatin1String("deselect_")) ||
        n == QLatin1String("invert") || n == QLatin1String("toggle_contour") ||
        n == QLatin1String("toggle_layer"))
        return QStringLiteral("选择");
    if (n == QLatin1String("ellipse_1pt") || n.startsWith(QLatin1String("ellipse")))
        return QStringLiteral("绘图");
    return QStringLiteral("其他");
}

// ============================================================
// 命令面板纯逻辑层
// ============================================================

void CommandPaletteModel::pushRecent(const QString& name, int maxKeep) {
    const QString n = name.trimmed().toLower();
    if (n.isEmpty()) return;
    recent_.removeAll(n);
    recent_.prepend(n);
    while (recent_.size() > maxKeep) recent_.removeLast();
}

// 子序列匹配：query 的字符按序全部出现在 s 中（大小写不敏感）。
// 返回间隔代价（∑ 相邻命中字符间的跳过字符数），不匹配返回 -1。
static int subsequenceCost(const QString& s, const QString& q) {
    int cost = 0, idx = -1;
    for (const QChar qc : q) {
        const int next = s.indexOf(qc, idx + 1, Qt::CaseInsensitive);
        if (next < 0) return -1;
        cost += (idx >= 0) ? (next - idx - 1) : 0;
        idx = next;
    }
    return cost;
}

int CommandPaletteModel::fuzzyScore(const QString& query, const PaletteEntry& e) {
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) return 0;
    int best = -1;
    const QVector<QPair<QString, int>> channels = {
        { e.name.toLower(), 100 },
        { e.title.toLower(), 90 },
        { e.alias.toLower(), 80 },
    };
    for (const auto& ch : channels) {
        if (ch.first.isEmpty()) continue;
        int score = -1;
        if (ch.first == q) {
            score = ch.second * 2;                       // 完全相等最高
        } else if (ch.first.startsWith(q)) {
            score = qRound(ch.second * 1.2);             // 前缀匹配
        } else if (ch.first.contains(q, Qt::CaseInsensitive)) {
            score = ch.second;                           // 包含匹配
        } else {
            const int cost = subsequenceCost(ch.first, q);
            if (cost >= 0 && cost <= 8)                  // 子序列：间隔过大视为不匹配
                score = ch.second - cost * 5;
        }
        if (score > best) best = score;
    }
    return best;
}

QVector<PaletteEntry> CommandPaletteModel::filter(const QString& query) const {
    const QString q = query.trimmed();
    // 命中条目集合（去重：同一命令名的别名条目不重复占用展示位时，
    // 仍允许并存——别名入口与命令本体都是合法可执行路径）。
    QVector<PaletteEntry> hits;
    if (q.isEmpty()) {
        hits = entries_;
        // 最近使用置顶：按 recent_ 顺序提升；其余保持原序（名称/注册序）。
        int insertPos = 0;
        for (const QString& r : recent_) {
            for (int i = insertPos; i < hits.size(); ++i) {
                if (hits[i].name == r) {
                    const PaletteEntry e = hits.takeAt(i);
                    hits.insert(insertPos++, e);
                    break;
                }
            }
        }
        return hits;
    }
    struct Scored { int score; int order; PaletteEntry e; };
    QVector<Scored> scored;
    for (int i = 0; i < entries_.size(); ++i) {
        const int s = fuzzyScore(q, entries_[i]);
        if (s > 0) scored.append({ s, i, entries_[i] });
    }
    // 得分降序；同分保持注册/装配顺序（stable_sort 语义）。
    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored& a, const Scored& b) { return a.score > b.score; });
    for (const auto& sc : scored) hits.append(sc.e);
    return hits;
}

// ============================================================
// 快捷键冲突自检
// ============================================================

namespace ShortcutAudit {

QVector<QPair<QString, QString>> duplicates(const QVector<Binding>& bindings) {
    QHash<QString, QVector<int>> byKey;   // 规格化快捷键 → 参与冲突的 Binding 下标
    for (int i = 0; i < bindings.size(); ++i) {
        const QString key = bindings[i].shortcut.trimmed();
        if (key.isEmpty()) continue;
        byKey[key.toLower()] << i;
    }
    QVector<QPair<QString, QString>> out;
    // 输出按原始注册顺序（首个冲突成员的出现序）。
    QHash<QString, bool> reported;
    for (int i = 0; i < bindings.size(); ++i) {
        const QString key = bindings[i].shortcut.trimmed();
        if (key.isEmpty()) continue;
        const QString norm = key.toLower();
        const QVector<int>& members = byKey[norm];
        if (members.size() < 2 || reported.value(norm)) continue;
        reported.insert(norm, true);
        QStringList actions;
        for (int idx : members) actions << bindings[idx].action;
        out.append({ key, actions.join(QStringLiteral(" / ")) });
    }
    return out;
}

} // namespace ShortcutAudit

} // namespace SketchCAD
