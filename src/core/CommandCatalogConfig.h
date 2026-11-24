#pragma once

#include <QJsonValue>
#include <QString>
#include <QVector>

namespace SketchCAD {

/// 一条 add 参数的元信息（不是命令行 usage 参数，而是 add(...) 的形参描述）。
struct CatalogParameterMeta {
    int order = 0;
    QString name;
    QString type;              // string / stringList
    QJsonValue defaultValue;   // string -> 字符串；stringList -> 字符串数组
    bool required = false;
    QString description;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, CatalogParameterMeta& out, QString* error);
};

/// 某条命令中一个 add 形参的实际取值。
struct CatalogParameterValue {
    int order = 0;
    QString name;
    QJsonValue value;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, CatalogParameterValue& out, QString* error);
};

/// 一条命令的 add 注册配置。
struct CatalogCommandDef {
    QString key;
    QVector<CatalogParameterValue> parameters;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, CatalogCommandDef& out, QString* error);
};

/// 完整命令目录配置文档。
struct CatalogDocument {
    int schemaVersion = 1;
    QString description;
    QVector<CatalogParameterMeta> parameterSchema;
    QVector<CatalogCommandDef> commands;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, CatalogDocument& out, QString* error);
};

/// JSON 命令目录的读取、校验与落盘。纯 Core 层，不依赖 QtWidgets。
class CommandCatalogConfig {
public:
    /// 默认路径：<程序目录>/config/command_catalog.json。
    /// 可用环境变量 SKETCHCAD_COMMAND_CATALOG 覆盖（便于测试/部署定位）。
    static QString defaultPath();

    bool load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    CatalogDocument& document() { return doc_; }
    const CatalogDocument& document() const { return doc_; }

    /// 结构校验；返回空字符串表示通过，否则返回带 JSON 指针的错误描述。
    static QString validate(const CatalogDocument& doc);

    /// 查找命令/参数，供 UI 与自测复用。
    static const CatalogCommandDef* findCommand(const CatalogDocument& doc, const QString& key);
    static const CatalogParameterValue* findParameter(const CatalogCommandDef& cmd, const QString& name);

private:
    CatalogDocument doc_;
};

} // namespace SketchCAD
