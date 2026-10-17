#pragma once
// P8（Ch14 参数化设计）：变量表达式层（VarParamTable）
// ------------------------------------------------------------
// 会话级「变量 → 表达式」表 + 轻量安全求值器 + 拓扑重算。
// 定位：只负责「算出值」；把值应用到模型（重解约束/刷新标注）由
// ConstraintSolverFacade::drive 委托**既有** driveParameter 完成
//（FORBID：不改既有约束求解语义；ESCALATE G6：无语义耦合，不触发停止线）。
//
// 安全求值器边界（IO_SPEC「轻量安全求值器」）：
//   * 仅支持 + - * / 、括号、一元正负号、数值常量、变量引用——无函数调用、无赋值、无逗号；
//   * 表达式长度 ≤ 512、括号/递归深度 ≤ 64（防栈耗尽）；
//   * 除零 / 数值非有限（溢出）→ 报错；
//   * 变量名规则：首字符为字母/下划线/非 ASCII 字符，其余为字母/数字/下划线/非 ASCII
//     （中文变量名可用，如「长度」），长度 ≤ 64。
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace SketchCAD {

class VarParamTable {
public:
    // ---- 定义 / 维护 ----
    // 定义变量 name = expr。expr 为常量表达式（不含变量引用）时立即求值并转为「值变量」；
    // 含变量引用时为「表达式变量」（不可直接驱动，须驱动其依赖变量）。
    // 成功返回空串；失败返回原因（名非法/语法错/除零/引用未定义变量——定义期允许引用
    // 尚未定义的变量，求值时才报未定义，便于任意顺序定义）。
    QString define(const QString& name, const QString& expr);

    // 把变量设为纯数值（清除表达式定义）。成功返回空串。
    QString setValue(const QString& name, double val);

    bool remove(const QString& name);
    void clear();

    // ---- 查询 ----
    bool         has(const QString& name) const;
    QString      expression(const QString& name) const;   // 定义原文（未定义返回空串）
    bool         isExpression(const QString& name) const; // true = 含变量引用的表达式变量
    double       value(const QString& name) const;        // 当前值（求值失败/未定义 → NaN）
    QStringList  names() const;                            // 定义顺序
    int          count() const { return (int)order_.size(); }

    // ---- 驱动准备：设 name=val → 拓扑重算全部表达式变量 ----
    // changed 输出「值发生变化的变量」列表（拓扑序：依赖在前）。
    // 循环依赖 / 除零 / 未定义引用 → 返回 false + err，且**不改动任何值**（先演算后提交）。
    // name 为表达式变量 → 返回 false + err（调用方应先行 isExpression 检查给更具体文案）。
    bool resolveDrive(const QString& name, double val,
                      QVector<QPair<QString, double>>& changed, QString* err = nullptr);

    // ---- 轻量安全表达式求值 ----
    // 无上下文直求（表达式内任何标识符 → 「未定义变量」错误）。
    static bool evalExpression(const QString& expr, double& out, QString* err = nullptr);

    // 带解析器的求值：resolve(name) 返回 false = 未定义变量。
    static bool evalExpression(const QString& expr,
                               const std::function<bool(const QString&, double&)>& resolve,
                               double& out, QString* err = nullptr);

    // 变量名/表达式静态校验（供 UI/命令层前置检查，与 define 内部同规则）
    static bool isValidName(const QString& name);
    static constexpr int kMaxNameLen   = 64;
    static constexpr int kMaxExprLen   = 512;
    static constexpr int kMaxParseDepth = 64;

private:
    struct Var {
        QString expr;          // 定义原文（值变量也保留，供「显示尺寸表达式」）
        double  value = 0.0;   // 当前值
        bool    hasRefs = false; // 表达式含变量引用（→ 表达式变量）
        QSet<QString> refs;    // 直接依赖的变量名（定义期解析，拓扑排序用）
    };

    // 定义期：解析 expr 收集引用集合（语法错 → false + err）。不求值。
    bool collectRefs(const QString& expr, QSet<QString>& refs, QString* err) const;

    // 演算：在 staging 值映射上求 name（stack 防循环）。false + err。
    bool evalVar(const QString& name, const std::map<QString, double>& staging,
                 QSet<QString>& stack, double& out, QString* err) const;

    std::vector<QString>        order_;   // 定义顺序（names() 用）
    std::map<QString, Var>      vars_;    // 名 → 定义（std::map 保序，遍历即名字序）
};

} // namespace SketchCAD
