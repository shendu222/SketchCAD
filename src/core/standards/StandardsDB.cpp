#include "core/standards/StandardsDB.h"

#include <QResource>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace SketchCAD {

namespace {

// 12 类标准表名，使用 builder lambda 逐条 add 填充，规避 GCC 13.1
// 巨型静态聚合初始化静默崩溃（此列表本身很小，但统一走 builder 范式）。
QStringList buildTableNames() {
    QStringList names;
    auto add = [&](const char* n) { names.append(QString::fromUtf8(n)); };
    add("sheet_sizes");
    add("scales");
    add("fonts");
    add("line_types");
    add("roughness");
    add("tolerances");
    add("geo_tol");
    add("thread");
    add("gear");
    add("bearing");
    add("weld");
    add("cad_layers");
    return names;
}

} // namespace

StandardsDB::StandardsDB() = default;

StandardsDB& StandardsDB::instance() {
    // Meyers 单例：C++11 保证静态局部初始化线程安全。
    // load() 不回调 instance()，不会触发 magic statics 再入死锁。
    static StandardsDB db;
    if (db.tables_.isEmpty()) {
        db.load();
    }
    return db;
}

bool StandardsDB::load() {
    tables_.clear();
    const QStringList names = buildTableNames();
    for (const QString& name : names) {
        const QString resPath = QStringLiteral(":/standards/") + name + QStringLiteral(".json");
        QResource res(resPath);
        if (!res.isValid()) {
            continue;
        }
        // QResource 已处理压缩态，data() 返回解压后的内存指针。
        QByteArray raw(reinterpret_cast<const char*>(res.data()), static_cast<int>(res.size()));
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (doc.isNull() || !doc.isObject()) {
            continue;
        }
        tables_.insert(name, doc.object());
    }
    return !tables_.isEmpty();
}

QJsonValue StandardsDB::query(const QString& table, const QString& key) const {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return QJsonValue(QJsonValue::Undefined);
    }
    const QJsonObject& obj = it.value();
    if (!obj.contains(key)) {
        return QJsonValue(QJsonValue::Undefined);
    }
    return obj.value(key);
}

QString StandardsDB::queryString(const QString& table, const QString& key) const {
    const QJsonValue v = query(table, key);
    if (v.isUndefined() || v.isNull()) {
        return QString();
    }
    if (v.isObject()) {
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }
    if (v.isArray()) {
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    }
    return v.toVariant().toString();
}

QStringList StandardsDB::allTables() const {
    return tables_.keys();
}

QJsonObject StandardsDB::tableObject(const QString& table) const {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return QJsonObject();
    }
    return it.value();
}

QString StandardsDB::summary() const {
    // 只从已装载的 12 类表经 query() 取「高频规则」标量条目，不塞任何整表 JSON。
    // 表名/键名是 ASCII，用 fromLatin1 即可；中文值原样来自资源，不做二次编码。
    auto v = [this](const char* table, const char* key) {
        const QString s = queryString(QString::fromLatin1(table), QString::fromLatin1(key));
        return s.isEmpty() ? QStringLiteral("(未装载)") : s;
    };

    QStringList lines;
    lines.append(QStringLiteral("【机械制图国标高频规则速查（数据源：内嵌 12 类标准表，仅摘要）】"));
    lines.append(QStringLiteral("线宽：粗:细 = %1（%2）；线宽系列 %3 mm；相邻平行线最小间隙 %4 mm。")
                     .arg(v("line_types", "thick_thin_ratio"), v("line_types", "_standard"),
                          v("line_types", "width_series"), v("line_types", "min_gap_mm")));
    lines.append(QStringLiteral("字体：汉字用 %1，最小字高 %2 mm，字宽比 %3（%4）。")
                     .arg(v("fonts", "hanzi_font"), v("fonts", "hanzi_min_height"),
                          v("fonts", "hanzi_width_ratio"), v("fonts", "_standard")));
    lines.append(QStringLiteral("幅面：A4 = %1 mm，A3 = %2 mm，留边 c = %3 mm（%4）。")
                     .arg(v("sheet_sizes", "A4"), v("sheet_sizes", "A3"),
                          v("sheet_sizes", "margin_c"), v("sheet_sizes", "_standard")));
    lines.append(QStringLiteral("比例：优先 1:1、2:1、1:2 等标准系列（%1）。")
                     .arg(v("scales", "_standard")));
    lines.append(QStringLiteral("公差配合：H 孔下偏差 EI = %1，h 轴上偏差 es = %2，JS/js 为 %3；等级 %4（%5）。")
                     .arg(v("tolerances", "H_lower"), v("tolerances", "h_upper"),
                          v("tolerances", "js_rule"), v("tolerances", "IT_grades"),
                          v("tolerances", "_standard")));
    lines.append(QStringLiteral("螺纹：M = %1，标注示例 %2；粗牙螺距系列见 %3（%4）。")
                     .arg(v("thread", "M"), v("thread", "example_M"),
                          v("thread", "_pitch_standard"), v("thread", "_standard")));
    lines.append(QStringLiteral("粗糙度：Ra 优先系列 %1 μm，符号夹角 %2°（%3）；焊缝 %4；图层 %5。")
                     .arg(v("roughness", "Ra_series"), v("roughness", "symbol_angle"),
                          v("roughness", "_standard"), v("weld", "_standard"),
                          v("cad_layers", "_standard")));
    lines.append(QStringLiteral("【必须遵守】以上只是高频规则摘要，不是精确数值表："
                                "凡涉及具体数值（如 M20 粗牙螺距、Φ30 IT7 标准公差、某幅面留边值），"
                                "一律用 queryStandard 精确查询，禁止凭记忆作答；"
                                "判断「当前图纸是否符合规范」必须调用 checkCompliance()，不得凭空推断。"));
    return lines.join(QLatin1Char('\n'));
}

} // namespace SketchCAD
