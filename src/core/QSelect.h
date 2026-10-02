#pragma once
#include <QString>
#include <QStringList>
#include <vector>
#include <QJsonObject>
#include <QJsonArray>
#include "Element.h"

namespace SketchCAD {

class SketchModel;

// ============================================================================
// 快速选择 / 过滤器（QSELECT / FILTER）核心
// ----------------------------------------------------------------------------
// 设计要点（大图纸性能）：
//   * 筛选 = 对 elements() 向量的**单遍 O(n) 扫描**，无逐 id 反查、无逐图元堆分配
//     （属性求值返回 QString 不可避免，但每图元至多 1~2 次短字符串构造）；
//   * 候选值枚举（uniquePropValues）用 QSet 去重、一次性收集，供下拉框填充；
//   * 与选择集的一致性：调用方经 SketchModel::applySelectionDelta 一次性落地
//     （替换/追加/排除），全程只发一次 selectionChanged，避免大图 O(n) 次刷新。
// ============================================================================

// 应用到选择集的方式（与 AutoCAD QSELECT 的 How to apply 对应）
enum class QSelectAction { Replace = 0, Append = 1, Exclude = 2 };

// 参与筛选的对象特性（下拉框候选）
enum class QSelectProp {
    None = 0,       // 不限（仅类型条件）
    Type,           // 对象类型（"直线"/"圆"/...）
    Layer,          // 图层名
    Color,          // 颜色（#rrggbb，含命名）
    LineStyle,      // 线型名
    LineWidth,      // 实际线宽(mm，一位小数)
    Text,           // 文字内容（Text/MText 文本、标注文本、表格整体文本）
    Name,           // 图元名称
    Group,          // 所属编组名（GROUP）
    Style,          // 文字样式名（Text/MText）
    Visible         // 可见性（"可见"/"隐藏"）
};

// 比较运算符
enum class QSelectOp { Eq = 0, Ne = 1, Contains = 2, StartsWith = 3 };

// 单条筛选条件
struct QSelectCriteria {
    QSelectProp prop = QSelectProp::None;
    QSelectOp   op   = QSelectOp::Eq;
    QString     value;               // 比较值（空 = 不限制该条件恒真，便于增删行）
};

// 过滤器定义：多条件组合（全部满足 AND / 任一满足 OR）+ 命名（保存/复用）
struct FilterDef {
    QString name;
    bool    matchAll = true;         // true=AND（全部条件满足），false=OR（任一满足）
    std::vector<QSelectCriteria> conds;

    QJsonObject toJson() const;
    static FilterDef fromJson(const QJsonObject& o);
    bool isValid() const { return !conds.empty(); }
};

// ---- 展示名（对话框下拉框用）----
QString qselectPropName(QSelectProp p);
QString qselectOpName(QSelectOp op);
QSelectProp qselectPropFromName(const QString& n);
QSelectOp   qselectOpFromName(const QString& n);

// ---- 属性求值（单一真源：筛选与候选值枚举共用同一实现，杜绝两处漂移）----
QString evalElementProp(const Element& e, QSelectProp prop);

// ---- 条件匹配 ----
bool matchCriteria(const Element& e, const QSelectCriteria& c);
bool matchFilter(const Element& e, const FilterDef& f);

// ---- 筛选执行：单遍 O(n) 扫描，返回命中的图元 id 列表 ----
//   scopeSelection 非空 = 只在给定 id 集合内筛选（QSELECT"当前选择集"范围）；
//   空 = 整个图形。始终只返回可见图元（隐藏图元不参与选择，与框选/图层选择一致）。
std::vector<int> qselect(const SketchModel* m, const FilterDef& f,
                         const std::vector<int>* scopeSelection = nullptr);

// ---- 四维快速选择（命令行 / 脚本 / 测试共用，无 GUI 依赖）----
// 按 图元类型 / 图层 / 颜色 / 线型 四维过滤，各维度为空字符串 = 不过滤该维；
// 全部为空 = 未指定任何条件 → 返回空（不盲目全选，与 AutoCAD QSELECT 须至少选一类一致）。
// 这是 QSELECT 引擎对外最便捷的单次入口，命令 qselect 与自测均复用它（不改动底层引擎语义）。
std::vector<int> qselectByDims(const SketchModel* m,
                               const QString& type,       // 图元类型名（elementTypeName，如 "直线"）
                               const QString& layer,      // 图层名
                               const QString& color,      // 颜色 #rrggbb
                               const QString& lineStyle); // 线型名（lineStyleName，如 "实线"）

// ---- 候选值枚举（下拉框）：整个图形内某特性的去重取值（升序）----
QStringList uniquePropValues(const SketchModel* m, QSelectProp prop);

} // namespace SketchCAD
