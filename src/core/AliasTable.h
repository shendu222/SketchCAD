#pragma once
// ============================================================
// AliasTable —— 命令别名机制（二次开发生态，acad.pgp 兼容）
//
// 设计要点（详见 docs/cui-alias-design-analysis.md）：
//   1. 单例进程级别名表，命令行启动时由默认 .pgp 载入。
//   2. 解析点在 CommandInterpreter::execute() 分词之后、findExact 之前：
//      输入 token 若为别名则就地展开为完整命令名（支持别名→别名链，带深度保护）。
//   3. 兼容 acad.pgp 行格式：每行 "别名,完整命令"；';' / '#' 开头为注释，空行跳过。
//   4. 单字母（L/C/A）与多字母（REC/PLINE）别名并存，大小写不敏感。
//   5. 用户可手编 .pgp 文件，改后 aliasreload 重新加载立即生效（无需重启）。
//   6. 纯 C++ 模块，无 QtWidgets 依赖，可无头单测。
// ============================================================
#include <QString>
#include <QStringList>
#include <QMap>

namespace SketchCAD {

class AliasTable {
public:
    static AliasTable& instance();

    /// 从 .pgp 文件解析别名（acad.pgp 兼容）。解析失败返回 false 并通过 err 说明。
    /// 成功时记录 filePath_ 供 aliasreload 复用。
    bool load(const QString& path, QString* err = nullptr);
    /// 写回当前别名表为 .pgp 文本（UTF-8，带注释头）。path 为空则用 filePath_。
    bool save(const QString& path = QString(), QString* err = nullptr);
    void clear();

    /// 解析单个 token：若该 token 是别名则返回对应完整命令，否则原样返回（已 toLower）。
    QString resolve(const QString& token) const;

    bool contains(const QString& alias) const;
    /// 新增/覆盖一条别名（alias、command 均按小写规整）。
    void setAlias(const QString& alias, const QString& command);
    /// 删除别名，返回是否曾存在。
    bool removeAlias(const QString& alias);

    QStringList aliases() const;                 ///< 全部别名（升序）
    QString commandOf(const QString& alias) const;///< 别名对应的完整命令（不存在返回空）
    const QMap<QString, QString>& map() const { return map_; }

    QString filePath() const { return filePath_; }
    void setFilePath(const QString& p) { filePath_ = p; }

    /// 默认别名集合（首次无 .pgp 时注入，对齐用户需求示例）。
    static QMap<QString, QString> defaultAliases();

private:
    AliasTable() = default;
    QMap<QString, QString> map_;   // alias(lower) -> command(lower)
    QString filePath_;
};

} // namespace SketchCAD
