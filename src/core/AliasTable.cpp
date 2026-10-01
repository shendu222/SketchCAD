#include "AliasTable.h"
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

namespace SketchCAD {

AliasTable& AliasTable::instance() {
    static AliasTable s;
    return s;
}

QMap<QString, QString> AliasTable::defaultAliases() {
    // 对齐用户需求示例：单字母 + 多字母别名。
    QMap<QString, QString> d;
    d["l"]   = "line";
    d["c"]   = "circle";
    d["a"]   = "arc";
    d["rec"] = "rect";
    d["pl"]  = "polyline";
    d["pli"] = "polyline";
    d["e"]   = "delete";
    d["m"]   = "move";
    d["co"]  = "duplicate";
    d["cp"]  = "duplicate";
    d["ro"]  = "rotate";
    d["tr"]  = "trim";
    d["ex"]  = "extend";
    d["mi"]  = "mirror";
    d["o"]   = "offset";
    d["f"]   = "fillet";
    d["sc"]  = "scale";
    d["z"]   = "zoom";
    d["la"]  = "layer";
    d["b"]   = "block";
    d["x"]   = "explode";
    d["dt"]  = "mtext";
    d["q"]   = "qselect";
    return d;
}

bool AliasTable::load(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开别名文件: ") + path;
        return false;
    }
    map_.clear();
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    int lineNo = 0;
    while (!in.atEnd()) {
        ++lineNo;
        QString raw = in.readLine();
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(';') || line.startsWith('#')) continue;  // 注释
        // 支持行内注释：逗号之后若还有 ';' 不算（保持简单，仅整行注释跳过）
        int comma = line.indexOf(',');
        if (comma < 0) continue;                                    // 非法行跳过
        QString alias = line.left(comma).trimmed().toLower();
        QString command = line.mid(comma + 1).trimmed().toLower();
        if (alias.isEmpty() || command.isEmpty()) continue;
        map_[alias] = command;
    }
    filePath_ = path;
    return true;
}

bool AliasTable::save(const QString& path, QString* err) {
    QString target = path.isEmpty() ? filePath_ : path;
    if (target.isEmpty()) {
        if (err) *err = QStringLiteral("未指定别名文件路径");
        return false;
    }
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法写入别名文件: ") + target;
        return false;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QStringLiteral("; SketchCAD 命令别名文件（acad.pgp 兼容）\n");
    out << QStringLiteral("; 每行格式：别名,完整命令   （';' 或 '#' 开头为注释）\n");
    for (auto it = map_.begin(); it != map_.end(); ++it)
        out << it.key() << "," << it.value() << "\n";
    filePath_ = target;
    return true;
}

void AliasTable::clear() { map_.clear(); }

QString AliasTable::resolve(const QString& token) const {
    QString t = token.trimmed().toLower();
    auto it = map_.find(t);
    return (it != map_.end()) ? it.value() : t;
}

bool AliasTable::contains(const QString& alias) const {
    return map_.contains(alias.trimmed().toLower());
}

void AliasTable::setAlias(const QString& alias, const QString& command) {
    map_[alias.trimmed().toLower()] = command.trimmed().toLower();
}

bool AliasTable::removeAlias(const QString& alias) {
    return map_.remove(alias.trimmed().toLower()) > 0;
}

QStringList AliasTable::aliases() const {
    QStringList r = map_.keys();
    r.sort();
    return r;
}

QString AliasTable::commandOf(const QString& alias) const {
    return map_.value(alias.trimmed().toLower(), QString());
}

} // namespace SketchCAD
