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

} // namespace SketchCAD
