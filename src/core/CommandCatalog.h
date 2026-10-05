#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <utility>

namespace SketchCAD {

// ============================================================
// CommandCatalog —— 命令元数据目录（help 详情 + 命令面板共用数据源）
// ============================================================
// 设计要点：
//   1. 纯数据 + 纯逻辑，无 QtWidgets 依赖，可无头直测（与 AliasTable 同层）。
//   2. name 必须与 CommandRegistry::registerExact 注册名一致（小写）；
//      未收录的命令仍会出现在 help 总表 / 命令面板（回落条目），只是没有详细字段。
//   3. usage 字符串全部取自各命令实现内的「用法：」错误提示或 Commands.h
//      家族注释，保证帮助文本与真实解析行为同源，不凭空编造。
//   4. 帮助文本只影响展示输出，不参与命令解析（HelpCommand 仍按注册名分派）。
// ============================================================

/// 单条命令的静态元数据。
struct CommandInfo {
    QString name;         ///< 命令名（小写，= 注册名）
    QString zhName;       ///< 中文名（面板标题 / 帮助展示）
    QString category;     ///< 分类：绘图/修改/查询/标注/约束/图层/块/视图/选择/工具/布局/脚本/帮助
    QString summary;      ///< 一句话简介
    QString usage;        ///< 用法（展示时统一加 "Usage:" 前缀；可空=无详细用法）
    QString params;       ///< 参数说明（可空）
    QString example;      ///< 样例（可空）
    QStringList related;  ///< 相关命令（可空）
    QString shortcut;     ///< 默认快捷键（可空，仅展示）
};

namespace CommandCatalog {

/// 静态元数据表（进程级共享，首次调用构造）。
const QVector<CommandInfo>& entries();

/// 按命令名精确查找（toLower 后匹配）；未收录返回 nullptr。
const CommandInfo* find(const QString& name);

/// 命令分类：收录命令取 category；未收录按前缀推断家族分类
/// （line_/spline_/ellipse_/polyline_→绘图，select_/deselect_/toggle_→选择，
///   modify 无前缀→修改），兜底「其他」。面板分组与 help 总表共用。
QString categoryOf(const QString& name);

} // namespace CommandCatalog

// ============================================================
// 命令面板纯逻辑层（无 QtWidgets 依赖，可无头直测）
// ============================================================

/// 面板条目：由 CommandCatalog + CommandInterpreter::commandNames() +
/// AliasTable 合并而来（合并逻辑在 UI 装配处，纯层只管过滤与排序）。
struct PaletteEntry {
    QString name;      ///< 回车执行的命令名（别名条目 = 解析后的目标命令）
    QString title;     ///< 展示标题（中文名优先，缺省 = 命令名）
    QString category;  ///< 分类（展示用，不参与搜索）
    QString summary;   ///< 简介
    QString shortcut;  ///< 快捷键（可空）
    QString alias;     ///< 非空 = 该条目是别名入口（展示为「别名 c → circle」）
};

class CommandPaletteModel {
public:
    void setEntries(QVector<PaletteEntry> e) { entries_ = std::move(e); }
    void setRecent(QStringList r) { recent_ = std::move(r); }
    const QStringList& recent() const { return recent_; }

    /// 最近使用：去重置顶，超出 maxKeep 截尾。
    void pushRecent(const QString& name, int maxKeep = 10);

    /// 模糊过滤：空查询 = 最近使用置顶 + 其余按名称升序；
    /// 非空 = name/title/alias 三通道模糊匹配，得分降序、同分按名称升序。
    QVector<PaletteEntry> filter(const QString& query) const;

    /// 单条目得分：-1 = 不匹配；越大越靠前。
    /// 计分规则（确定性，可测）：name/title/alias 依次为 100/90/80 基分；
    /// 前缀匹配 ×1.2（取整），包含匹配 ×1.0，子序列匹配 = 基分 - 间隔数 ×5，
    /// 子序列间隔超过 8 视为不匹配。任一通道命中即取该通道最高分。
    static int fuzzyScore(const QString& query, const PaletteEntry& e);

private:
    QVector<PaletteEntry> entries_;
    QStringList recent_;
};

// ============================================================
// 快捷键冲突自检（纯函数，可无头直测）
// ============================================================

namespace ShortcutAudit {

struct Binding {
    QString action;    ///< 动作描述（菜单文本 / 快捷键说明）
    QString shortcut;  ///< QKeySequence::toString() 形式（如 "Ctrl+Shift+P"）
};

/// 扫描重复绑定：返回 (快捷键, 冲突动作按注册顺序 ' / ' 连接) 列表；
/// 无冲突返回空。相同 shortcut 字符串（大小写不敏感）视为冲突。
QVector<QPair<QString, QString>> duplicates(const QVector<Binding>& bindings);

} // namespace ShortcutAudit

} // namespace SketchCAD
