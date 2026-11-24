#include "CommandCatalogConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

namespace SketchCAD {
namespace {

QString at(const QString& pointer, const QString& msg) {
    return QStringLiteral("JSON 指针 %1：%2").arg(pointer, msg);
}

bool isStringType(const QJsonValue& v) { return v.isString() || v.isNull() || v.isUndefined(); }
bool isStringListType(const QJsonValue& v) {
    if (!v.isArray()) return false;
    for (const QJsonValue& item : v.toArray())
        if (!item.isString()) return false;
    return true;
}
bool matchesType(const QString& type, const QJsonValue& v) {
    return type == QStringLiteral("string") ? isStringType(v) : isStringListType(v);
}
bool isEmptyValue(const QString& type, const QJsonValue& v) {
    if (type == QStringLiteral("string"))
        return v.toString().trimmed().isEmpty();
    return v.toArray().isEmpty();
}

} // namespace

QJsonObject CatalogParameterMeta::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("order"), order);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("type"), type);
    o.insert(QStringLiteral("default"), defaultValue);
    o.insert(QStringLiteral("required"), required);
    o.insert(QStringLiteral("description"), description);
    return o;
}

bool CatalogParameterMeta::fromJson(const QJsonObject& obj, CatalogParameterMeta& out, QString* error) {
    if (!obj.value(QStringLiteral("order")).isDouble()) {
        if (error) *error = QStringLiteral("order 必须是整数");
        return false;
    }
    out.order = obj.value(QStringLiteral("order")).toInt();
    out.name = obj.value(QStringLiteral("name")).toString();
    out.type = obj.value(QStringLiteral("type")).toString();
    out.defaultValue = obj.value(QStringLiteral("default"));
    out.required = obj.value(QStringLiteral("required")).toBool();
    out.description = obj.value(QStringLiteral("description")).toString();
    if (!obj.value(QStringLiteral("required")).isBool()) {
        if (error) *error = QStringLiteral("required 必须是布尔值");
        return false;
    }
    return true;
}

QJsonObject CatalogParameterValue::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("order"), order);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("value"), value);
    return o;
}

bool CatalogParameterValue::fromJson(const QJsonObject& obj, CatalogParameterValue& out, QString* error) {
    if (!obj.value(QStringLiteral("order")).isDouble()) {
        if (error) *error = QStringLiteral("order 必须是整数");
        return false;
    }
    out.order = obj.value(QStringLiteral("order")).toInt();
    out.name = obj.value(QStringLiteral("name")).toString();
    out.value = obj.value(QStringLiteral("value"));
    if (!obj.contains(QStringLiteral("value"))) {
        if (error) *error = QStringLiteral("缺少 value 字段");
        return false;
    }
    return true;
}

QJsonObject CatalogCommandDef::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("key"), key);
    QJsonArray arr;
    for (const CatalogParameterValue& p : parameters) arr.append(p.toJson());
    o.insert(QStringLiteral("parameters"), arr);
    return o;
}

bool CatalogCommandDef::fromJson(const QJsonObject& obj, CatalogCommandDef& out, QString* error) {
    out.key = obj.value(QStringLiteral("key")).toString();
    if (out.key.isEmpty()) {
        if (error) *error = QStringLiteral("key 不能为空");
        return false;
    }
    const QJsonArray arr = obj.value(QStringLiteral("parameters")).toArray();
    if (arr.isEmpty()) {
        if (error) *error = QStringLiteral("parameters 数组为空");
        return false;
    }
    for (int i = 0; i < arr.size(); ++i) {
        CatalogParameterValue p;
        QString e;
        if (!p.fromJson(arr.at(i).toObject(), p, &e)) {
            if (error) *error = at(QStringLiteral("parameters[%1]").arg(i), e);
            return false;
        }
        out.parameters.append(p);
    }
    return true;
}

QJsonObject CatalogDocument::toJson() const {
    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), schemaVersion);
    o.insert(QStringLiteral("description"), description);
    QJsonArray schemaArr;
    for (const CatalogParameterMeta& m : parameterSchema) schemaArr.append(m.toJson());
    o.insert(QStringLiteral("parameterSchema"), schemaArr);
    QJsonArray cmdArr;
    for (const CatalogCommandDef& c : commands) cmdArr.append(c.toJson());
    o.insert(QStringLiteral("commands"), cmdArr);
    return o;
}

bool CatalogDocument::fromJson(const QJsonObject& obj, CatalogDocument& out, QString* error) {
    out.schemaVersion = obj.value(QStringLiteral("schemaVersion")).toInt(1);
    out.description = obj.value(QStringLiteral("description")).toString();

    const QJsonArray schemaArr = obj.value(QStringLiteral("parameterSchema")).toArray();
    if (schemaArr.isEmpty()) {
        if (error) *error = at(QStringLiteral("/parameterSchema"), QStringLiteral("数组为空"));
        return false;
    }
    for (int i = 0; i < schemaArr.size(); ++i) {
        CatalogParameterMeta m;
        QString e;
        if (!m.fromJson(schemaArr.at(i).toObject(), m, &e)) {
            if (error) *error = at(QStringLiteral("/parameterSchema[%1]").arg(i), e);
            return false;
        }
        out.parameterSchema.append(m);
    }

    const QJsonArray cmdArr = obj.value(QStringLiteral("commands")).toArray();
    if (cmdArr.isEmpty()) {
        if (error) *error = at(QStringLiteral("/commands"), QStringLiteral("数组为空"));
        return false;
    }
    for (int i = 0; i < cmdArr.size(); ++i) {
        CatalogCommandDef c;
        QString e;
        if (!c.fromJson(cmdArr.at(i).toObject(), c, &e)) {
            if (error) *error = at(QStringLiteral("/commands[%1]").arg(i), e);
            return false;
        }
        out.commands.append(c);
    }
    return true;
}

QString CommandCatalogConfig::defaultPath() {
    const QByteArray env = qgetenv("SKETCHCAD_COMMAND_CATALOG");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env.constData());
    return QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("config/command_catalog.json"));
}

bool CommandCatalogConfig::load(const QString& path, QString* error) {
    QFile file(path);
    if (!file.exists()) {
        if (error) {
            *error = QStringLiteral("命令目录配置缺失。\n路径：%1\n请恢复 config/command_catalog.json 或设置 SKETCHCAD_COMMAND_CATALOG。")
                         .arg(path);
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("命令目录配置无法读取。\n路径：%1\n系统错误：%2")
                         .arg(path, file.errorString());
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("命令目录配置 JSON 格式错误。\n路径：%1\n行：%2，列：%3，偏移：%4\n原因：%5")
                         .arg(path)
                         .arg(pe.offset >= 0 ? 1 + bytes.left(pe.offset).count('\n') : 0)
                         .arg(pe.offset >= 0 ? pe.offset - bytes.lastIndexOf('\n', pe.offset - 1) : 0)
                         .arg(pe.offset)
                         .arg(pe.errorString());
        }
        return false;
    }
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("命令目录配置根节点必须是 JSON 对象。\n路径：%1").arg(path);
        return false;
    }

    CatalogDocument parsed;
    QString parseErr;
    if (!CatalogDocument::fromJson(doc.object(), parsed, &parseErr)) {
        if (error) *error = QStringLiteral("命令目录配置结构错误。\n路径：%1\n%2").arg(path, parseErr);
        return false;
    }
    const QString validation = validate(parsed);
    if (!validation.isEmpty()) {
        if (error) *error = QStringLiteral("命令目录配置校验失败。\n路径：%1\n%2").arg(path, validation);
        return false;
    }

    doc_ = parsed;
    return true;
}

bool CommandCatalogConfig::save(const QString& path, QString* error) const {
    const QString validation = validate(doc_);
    if (!validation.isEmpty()) {
        if (error) *error = validation;
        return false;
    }
    const QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法写入命令目录配置。\n路径：%1\n系统错误：%2")
                                .arg(path, file.errorString());
        return false;
    }
    const QByteArray bytes = QJsonDocument(doc_.toJson()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (error) *error = QStringLiteral("命令目录配置写入不完整。\n路径：%1").arg(path);
        return false;
    }
    if (!file.commit()) {
        if (error) *error = QStringLiteral("命令目录配置提交失败。\n路径：%1\n系统错误：%2")
                                .arg(path, file.errorString());
        return false;
    }
    return true;
}

QString CommandCatalogConfig::validate(const CatalogDocument& doc) {
    if (doc.schemaVersion != 1)
        return at(QStringLiteral("/schemaVersion"), QStringLiteral("当前仅支持 schemaVersion=1"));
    if (doc.parameterSchema.isEmpty())
        return at(QStringLiteral("/parameterSchema"), QStringLiteral("参数定义不能为空"));
    if (doc.commands.isEmpty())
        return at(QStringLiteral("/commands"), QStringLiteral("命令清单不能为空"));

    QSet<QString> metaNames;
    QSet<int> metaOrders;
    for (int i = 0; i < doc.parameterSchema.size(); ++i) {
        const CatalogParameterMeta& m = doc.parameterSchema.at(i);
        const QString p = QStringLiteral("/parameterSchema[%1]").arg(i);
        if (m.order <= 0) return at(p + QStringLiteral("/order"), QStringLiteral("order 必须为正整数"));
        if (m.name.isEmpty()) return at(p + QStringLiteral("/name"), QStringLiteral("参数名不能为空"));
        if (metaNames.contains(m.name)) return at(p + QStringLiteral("/name"), QStringLiteral("参数名重复：") + m.name);
        if (metaOrders.contains(m.order)) return at(p + QStringLiteral("/order"), QStringLiteral("order 重复：") + QString::number(m.order));
        if (m.type != QStringLiteral("string") && m.type != QStringLiteral("stringList"))
            return at(p + QStringLiteral("/type"), QStringLiteral("type 仅支持 string / stringList"));
        if (m.description.trimmed().isEmpty())
            return at(p + QStringLiteral("/description"), QStringLiteral("说明不能为空"));
        if (!matchesType(m.type, m.defaultValue))
            return at(p + QStringLiteral("/default"), QStringLiteral("默认值类型与 type 不一致"));
        metaNames.insert(m.name);
        metaOrders.insert(m.order);
    }

    QSet<QString> commandKeys;
    for (int i = 0; i < doc.commands.size(); ++i) {
        const CatalogCommandDef& c = doc.commands.at(i);
        const QString cp = QStringLiteral("/commands[%1]").arg(i);
        if (c.key.isEmpty()) return at(cp + QStringLiteral("/key"), QStringLiteral("命令名不能为空"));
        if (commandKeys.contains(c.key)) return at(cp + QStringLiteral("/key"), QStringLiteral("命令名重复：") + c.key);
        commandKeys.insert(c.key);
        if (c.parameters.size() != doc.parameterSchema.size())
            return at(cp + QStringLiteral("/parameters"),
                      QStringLiteral("参数项数量(%1)与参数定义数量(%2)不一致")
                          .arg(c.parameters.size()).arg(doc.parameterSchema.size()));

        QSet<QString> seenNames;
        for (int j = 0; j < c.parameters.size(); ++j) {
            const CatalogParameterValue& pv = c.parameters.at(j);
            const QString pp = cp + QStringLiteral("/parameters[%1]").arg(j);
            const CatalogParameterMeta* meta = nullptr;
            for (const CatalogParameterMeta& m : doc.parameterSchema)
                if (m.name == pv.name) { meta = &m; break; }
            if (!meta) return at(pp + QStringLiteral("/name"), QStringLiteral("未知参数：") + pv.name);
            if (seenNames.contains(pv.name)) return at(pp + QStringLiteral("/name"), QStringLiteral("同一命令中参数重复：") + pv.name);
            if (pv.order != meta->order)
                return at(pp + QStringLiteral("/order"),
                          QStringLiteral("顺序(%1)与参数定义(%2)不一致").arg(pv.order).arg(meta->order));
            if (!matchesType(meta->type, pv.value))
                return at(pp + QStringLiteral("/value"), QStringLiteral("取值类型与参数定义不一致"));
            if (meta->required && isEmptyValue(meta->type, pv.value))
                return at(pp + QStringLiteral("/value"), QStringLiteral("必填参数为空：") + meta->name);
            seenNames.insert(pv.name);
        }
    }
    return QString();
}

const CatalogCommandDef* CommandCatalogConfig::findCommand(const CatalogDocument& doc, const QString& key) {
    for (const CatalogCommandDef& c : doc.commands)
        if (c.key == key) return &c;
    return nullptr;
}

const CatalogParameterValue* CommandCatalogConfig::findParameter(const CatalogCommandDef& cmd, const QString& name) {
    for (const CatalogParameterValue& p : cmd.parameters)
        if (p.name == name) return &p;
    return nullptr;
}

} // namespace SketchCAD
