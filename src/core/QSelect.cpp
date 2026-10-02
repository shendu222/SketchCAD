#include "QSelect.h"
#include "SketchModel.h"
#include <QSet>
#include <algorithm>

namespace SketchCAD {

// ---- JSON 往返（命名过滤器的持久化格式；缺键回落默认，向后兼容）----
QJsonObject FilterDef::toJson() const {
    QJsonObject o;
    o["name"]     = name;
    o["matchAll"] = matchAll;
    QJsonArray arr;
    for (const auto& c : conds) {
        QJsonObject co;
        co["prop"]  = qselectPropName(c.prop);
        co["op"]    = qselectOpName(c.op);
        co["value"] = c.value;
        arr.append(co);
    }
    o["conds"] = arr;
    return o;
}

FilterDef FilterDef::fromJson(const QJsonObject& o) {
    FilterDef f;
    f.name     = o["name"].toString();
    f.matchAll = json::getBool(o, "matchAll", true);
    for (const auto& v : o["conds"].toArray()) {
        const QJsonObject co = v.toObject();
        QSelectCriteria c;
        c.prop  = qselectPropFromName(co["prop"].toString());
        c.op    = qselectOpFromName(co["op"].toString());
        c.value = co["value"].toString();
        f.conds.push_back(c);
    }
    return f;
}

// ---- 展示名（界面与 JSON 持久化都用中文名做键，人工可读；改名会破坏旧过滤器存档，
//       故这些字符串视为持久化契约，只增不改）----
QString qselectPropName(QSelectProp p) {
    switch (p) {
        case QSelectProp::None:      return QStringLiteral("不限");
        case QSelectProp::Type:      return QStringLiteral("对象类型");
        case QSelectProp::Layer:     return QStringLiteral("图层");
        case QSelectProp::Color:     return QStringLiteral("颜色");
        case QSelectProp::LineStyle: return QStringLiteral("线型");
        case QSelectProp::LineWidth: return QStringLiteral("线宽");
        case QSelectProp::Text:      return QStringLiteral("文字内容");
        case QSelectProp::Name:      return QStringLiteral("名称");
        case QSelectProp::Group:     return QStringLiteral("编组");
        case QSelectProp::Style:     return QStringLiteral("文字样式");
        case QSelectProp::Visible:   return QStringLiteral("可见性");
    }
    return QStringLiteral("不限");
}

QString qselectOpName(QSelectOp op) {
    switch (op) {
        case QSelectOp::Eq:        return QStringLiteral("等于");
        case QSelectOp::Ne:        return QStringLiteral("不等于");
        case QSelectOp::Contains:  return QStringLiteral("包含");
        case QSelectOp::StartsWith:return QStringLiteral("开头为");
    }
    return QStringLiteral("等于");
}

QSelectProp qselectPropFromName(const QString& n) {
    static const QHash<QString, QSelectProp> k = {
        {QStringLiteral("不限"),     QSelectProp::None},
        {QStringLiteral("对象类型"), QSelectProp::Type},
        {QStringLiteral("图层"),     QSelectProp::Layer},
        {QStringLiteral("颜色"),     QSelectProp::Color},
        {QStringLiteral("线型"),     QSelectProp::LineStyle},
        {QStringLiteral("线宽"),     QSelectProp::LineWidth},
        {QStringLiteral("文字内容"), QSelectProp::Text},
        {QStringLiteral("名称"),     QSelectProp::Name},
        {QStringLiteral("编组"),     QSelectProp::Group},
        {QStringLiteral("文字样式"), QSelectProp::Style},
        {QStringLiteral("可见性"),   QSelectProp::Visible},
    };
    const auto it = k.constFind(n);
    return it != k.constEnd() ? it.value() : QSelectProp::None;
}

QSelectOp qselectOpFromName(const QString& n) {
    static const QHash<QString, QSelectOp> k = {
        {QStringLiteral("等于"),   QSelectOp::Eq},
        {QStringLiteral("不等于"), QSelectOp::Ne},
        {QStringLiteral("包含"),   QSelectOp::Contains},
        {QStringLiteral("开头为"), QSelectOp::StartsWith},
    };
    const auto it = k.constFind(n);
    return it != k.constEnd() ? it.value() : QSelectOp::Eq;
}

// ---- 属性求值（单一真源）。全部返回短字符串；空值 = 该图元无此属性 ----
QString evalElementProp(const Element& e, QSelectProp prop) {
    switch (prop) {
        case QSelectProp::None:      return QString();
        case QSelectProp::Type:      return QString::fromUtf8(elementTypeName(e.type));
        case QSelectProp::Layer:     return e.layer;
        case QSelectProp::Color:     return e.color.name();          // #rrggbb
        case QSelectProp::LineStyle: return QString::fromUtf8(lineStyleName(e.lineStyle));
        case QSelectProp::LineWidth: return QString::number(e.effectiveLineWidth(), 'f', 1);
        case QSelectProp::Name:      return e.name;
        case QSelectProp::Group:     return e.groupName;
        case QSelectProp::Style:     return e.styleName;
        case QSelectProp::Visible:   return e.visible ? QStringLiteral("可见") : QStringLiteral("隐藏");
        case QSelectProp::Text: {
            // 文字内容：Text/MText 取 text；标注取 text；表格取全部单元格拼接
            // （换行分隔——"包含"匹配在拼接串上与逐格匹配等价，且只构造一次字符串）
            if (e.type == ElementType::Table) {
                QString all;
                for (const QString& s : e.tableCells) { all += s; all += QLatin1Char('\n'); }
                return all;
            }
            return e.text;
        }
    }
    return QString();
}

// ---- 条件匹配 ----
bool matchCriteria(const Element& e, const QSelectCriteria& c) {
    if (c.prop == QSelectProp::None) return true;      // "不限"恒真（占位行）
    const QString v = evalElementProp(e, c.prop);
    switch (c.op) {
        case QSelectOp::Eq:
            return c.value.isEmpty() ? v.isEmpty() : v.compare(c.value, Qt::CaseInsensitive) == 0;
        case QSelectOp::Ne:
            return c.value.isEmpty() ? !v.isEmpty() : v.compare(c.value, Qt::CaseInsensitive) != 0;
        case QSelectOp::Contains:
            return v.contains(c.value, Qt::CaseInsensitive);
        case QSelectOp::StartsWith:
            return v.startsWith(c.value, Qt::CaseInsensitive);
    }
    return false;
}

bool matchFilter(const Element& e, const FilterDef& f) {
    if (f.conds.empty()) return true;
    if (f.matchAll) {                                  // AND：任一不满足即淘汰（短路）
        for (const auto& c : f.conds)
            if (!matchCriteria(e, c)) return false;
        return true;
    }
    for (const auto& c : f.conds)                      // OR：任一满足即命中
        if (matchCriteria(e, c)) return true;
    return false;
}

// ---- 筛选执行：单遍 O(n)。scope 非空时只在范围内检查（仍是对范围集合的单遍）----
std::vector<int> qselect(const SketchModel* m, const FilterDef& f,
                         const std::vector<int>* scopeSelection) {
    std::vector<int> out;
    if (!m) return out;
    if (scopeSelection) {
        // 范围筛选：id → 元素用顺序查找会是 O(n·k)，大图退化——
        // 先把范围集合装进 QSet（k 次哈希），再对 elements() 单遍扫描
        QSet<int> scope;
        scope.reserve(int(scopeSelection->size()) * 2);
        for (int id : *scopeSelection) scope.insert(id);
        for (const auto& e : m->elements()) {
            if (!e.visible || !scope.contains(e.id)) continue;
            if (matchFilter(e, f)) out.push_back(e.id);
        }
    } else {
        for (const auto& e : m->elements()) {
            if (!e.visible) continue;
            if (matchFilter(e, f)) out.push_back(e.id);
        }
    }
    return out;
}

// ---- 四维快速选择（命令行 / 脚本 / 测试共用入口）----
std::vector<int> qselectByDims(const SketchModel* m, const QString& type,
                               const QString& layer, const QString& color,
                               const QString& lineStyle) {
    if (!m) return {};
    // 全空：未指定任何过滤维度 → 返回空，不盲目全选（避免误选整图）
    if (type.isEmpty() && layer.isEmpty() && color.isEmpty() && lineStyle.isEmpty())
        return {};
    FilterDef f;
    f.name = QStringLiteral("QSELECT");
    f.matchAll = true;   // 多维之间为 AND 组合
    if (!type.isEmpty())       f.conds.push_back({QSelectProp::Type,      QSelectOp::Eq, type});
    if (!layer.isEmpty())      f.conds.push_back({QSelectProp::Layer,     QSelectOp::Eq, layer});
    if (!color.isEmpty())      f.conds.push_back({QSelectProp::Color,     QSelectOp::Eq, color});
    if (!lineStyle.isEmpty())  f.conds.push_back({QSelectProp::LineStyle, QSelectOp::Eq, lineStyle});
    return qselect(m, f, nullptr);
}

// ---- 候选值枚举：单遍收集 + QSet 去重 + 排序 ----
QStringList uniquePropValues(const SketchModel* m, QSelectProp prop) {
    QStringList out;
    if (!m || prop == QSelectProp::None) return out;
    QSet<QString> seen;
    for (const auto& e : m->elements()) {
        const QString v = evalElementProp(e, prop);
        if (!v.isEmpty() && !seen.contains(v)) {
            seen.insert(v);
            out << v;
        }
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

} // namespace SketchCAD
