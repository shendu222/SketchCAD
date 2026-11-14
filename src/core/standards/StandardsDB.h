#ifndef STANDARDS_DB_H
#define STANDARDS_DB_H

#include <QString>
#include <QStringList>
#include <QJsonValue>
#include <QJsonObject>

namespace SketchCAD {

// 标准知识库：内嵌《机械制图手册（第四版）》国标数据的 12 类表。
// 数据以 JSON 形式经 qt_add_resources 编译进可执行文件（:/standards/<name>.json），
// 运行时通过 QResource 读出解析，部署零附件。
// 设计约束：不改变任何既有契约；数据全部外置 JSON，C++ 侧仅做装载/查询，
// 不引入任何巨型静态聚合初始化（规避 GCC 13.1 静默崩溃）。
class StandardsDB {
public:
    StandardsDB();

    // 全局单例访问器：进程内懒装载并共享同一份已装载的 12 类表。
    // 命令栏、LLM 工具、浏览面板共用此实例，避免重复装载与状态分叉。
    // 注意：构造方不得在本函数构造期再调 instance()（magic statics 再入死锁）。
    static StandardsDB& instance();

    // 通过 QResource 读取 :/standards/<name>.json 装载全部 12 类标准表。
    // 返回是否成功装载到至少一个表。
    bool load();

    // 查询某表某键的值；表或键不存在时返回 QJsonValue()（null/undefined）。
    QJsonValue query(const QString& table, const QString& key) const;

    // 便捷查询：将 query() 结果归一为可读字符串。
    // 命中标量返回其字符串化值；命中对象/数组返回 Compact JSON 文本；
    // 未命中（表或键不存在 / 值为 null）返回空 QString（调用方应以 query() 区分）。
    QString queryString(const QString& table, const QString& key) const;

    // 返回全部已装载的表名（应为 12 类）。
    QStringList allTables() const;

    // 返回某表的原始 JSON 对象（探针 / 调试用）；不存在返回空对象。
    QJsonObject tableObject(const QString& table) const;

    // ---- 面向 LLM 的「高频国标知识摘要」（数据派生，不新增第 13 张资源表）----
    // 仅列高频规则 + 标准号（全部经 query() 从已装载的 12 类表读取），
    // 并明确要求精确数值走 queryStandard 工具；供 NlAssistant 构造时拼入
    // system prompt，控制上下文体积（不放全量 12 表数据）。
    QString summary() const;

private:
    QHash<QString, QJsonObject> tables_;
};

} // namespace SketchCAD

#endif // STANDARDS_DB_H
