#include "VarParam.h"
#include <cmath>
#include <limits>

namespace SketchCAD {
namespace {

// ---------------------------------------------------------------
// 轻量安全表达式求值器（递归下降，仅 + - * / 、括号、一元正负号、
// 数值常量、变量引用）。无函数调用/赋值/逗号；深度与长度双重封顶。
// 数字按 ASCII 手工累加解析（不经 QString::toDouble，规避本地化差异）。
// ---------------------------------------------------------------
class ExprParser {
public:
    ExprParser(const QString& s, int maxDepth, QSet<QString>* collect,
               const std::function<bool(const QString&, double&)>* resolve)
        : s_(s), maxDepth_(maxDepth), collect_(collect), resolve_(resolve) {}

    bool run(double& out, QString* err) {
        skipWs();
        if (!parseExpr(out, err)) return false;
        skipWs();
        if (pos_ != s_.size()) {
            if (err)
                *err = QStringLiteral("表达式语法错误：第 %1 字符处有多余内容「%2」")
                         .arg(pos_ + 1).arg(s_.mid(pos_));
            return false;
        }
        return true;
    }

private:
    void skipWs() {
        while (pos_ < s_.size() && s_.at(pos_).isSpace()) ++pos_;
    }
    bool fail(QString* err, const QString& msg) const {
        if (err) *err = msg;
        return false;
    }
    static bool isIdentStart(QChar c)  { return c.isLetter() || c == QLatin1Char('_'); }
    static bool isIdentCont(QChar c)   { return c.isLetterOrNumber() || c == QLatin1Char('_'); }
    static bool isAsciiDigit(QChar c)  { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); }

    bool parseExpr(double& out, QString* err) {
        if (!parseTerm(out, err)) return false;
        for (;;) {
            skipWs();
            if (pos_ >= s_.size()) break;
            const QChar c = s_.at(pos_);
            if (c != QLatin1Char('+') && c != QLatin1Char('-')) break;
            ++pos_;
            double rhs = 0.0;
            if (!parseTerm(rhs, err)) return false;
            out = (c == QLatin1Char('+')) ? out + rhs : out - rhs;
        }
        return true;
    }

    bool parseTerm(double& out, QString* err) {
        if (!parseFactor(out, err)) return false;
        for (;;) {
            skipWs();
            if (pos_ >= s_.size()) break;
            const QChar c = s_.at(pos_);
            if (c != QLatin1Char('*') && c != QLatin1Char('/')) break;
            ++pos_;
            double rhs = 0.0;
            if (!parseFactor(rhs, err)) return false;
            if (c == QLatin1Char('*')) {
                out = out * rhs;
            } else {
                if (rhs == 0.0) return fail(err, QStringLiteral("表达式除零（除数绝对值为 0）"));
                out = out / rhs;
            }
        }
        return true;
    }

    bool parseFactor(double& out, QString* err) {
        skipWs();
        if (pos_ >= s_.size()) return fail(err, QStringLiteral("表达式意外结束（缺少运算数）"));
        const QChar c = s_.at(pos_);
        if (c == QLatin1Char('-') || c == QLatin1Char('+')) {
            if (++depth_ > maxDepth_)
                return fail(err, QStringLiteral("表达式嵌套/连续一元号过深"));
            ++pos_;
            double v = 0.0;
            const bool ok = parseFactor(v, err);
            --depth_;
            if (!ok) return false;
            out = (c == QLatin1Char('-')) ? -v : v;
            return true;
        }
        return parsePrimary(out, err);
    }

    bool parsePrimary(double& out, QString* err) {
        skipWs();
        if (pos_ >= s_.size()) return fail(err, QStringLiteral("表达式意外结束（缺少运算数）"));
        const QChar c = s_.at(pos_);
        if (c == QLatin1Char('(')) {
            if (++depth_ > maxDepth_)
                return fail(err, QStringLiteral("表达式括号嵌套过深"));
            ++pos_;
            double v = 0.0;
            const bool ok = parseExpr(v, err);
            --depth_;
            if (!ok) return false;
            skipWs();
            if (pos_ >= s_.size() || s_.at(pos_) != QLatin1Char(')'))
                return fail(err, QStringLiteral("表达式缺少右括号"));
            ++pos_;
            out = v;
            return true;
        }
        if (isAsciiDigit(c) ||
            (c == QLatin1Char('.') && pos_ + 1 < s_.size() && isAsciiDigit(s_.at(pos_ + 1))))
            return parseNumber(out, err);
        if (isIdentStart(c)) return parseIdent(out, err);
        return fail(err, QStringLiteral("表达式语法错误：无法识别的字符「%1」").arg(c));
    }

    bool parseNumber(double& out, QString* err) {
        double ip = 0.0;
        bool any = false;
        while (pos_ < s_.size() && isAsciiDigit(s_.at(pos_))) {
            ip = ip * 10.0 + (s_.at(pos_).digitValue());
            any = true;
            ++pos_;
        }
        double fp = 0.0, scale = 1.0;
        if (pos_ < s_.size() && s_.at(pos_) == QLatin1Char('.')) {
            ++pos_;
            while (pos_ < s_.size() && isAsciiDigit(s_.at(pos_))) {
                fp = fp * 10.0 + (s_.at(pos_).digitValue());
                scale *= 10.0;
                any = true;
                ++pos_;
            }
        }
        if (!any) return fail(err, QStringLiteral("表达式数字格式错误"));
        out = ip + fp / scale;
        return true;
    }

    bool parseIdent(double& out, QString* err) {
        const int start = pos_;
        ++pos_;   // 首字符已由调用方校验
        while (pos_ < s_.size() && isIdentCont(s_.at(pos_))) ++pos_;
        const QString name = s_.mid(start, pos_ - start);
        if (collect_) {
            collect_->insert(name);
            out = 0.0;   // 收集模式：值无意义
            return true;
        }
        if (!resolve_) return fail(err, QStringLiteral("表达式引用未定义变量「%1」").arg(name));
        double v = 0.0;
        if (!(*resolve_)(name, v))
            return fail(err, QStringLiteral("表达式引用未定义变量「%1」").arg(name));
        out = v;
        return true;
    }

    const QString& s_;
    int pos_ = 0;
    int depth_ = 0;
    const int maxDepth_;
    QSet<QString>* collect_;   // 非空 = 收集引用模式（不求值）
    const std::function<bool(const QString&, double&)>* resolve_;
};

} // namespace

// ---------------------------------------------------------------
// VarParamTable
// ---------------------------------------------------------------
bool VarParamTable::isValidName(const QString& name) {
    if (name.isEmpty() || name.size() > kMaxNameLen) return false;
    if (!(name.at(0).isLetter() || name.at(0) == QLatin1Char('_'))) return false;
    for (const QChar& c : name)
        if (!(c.isLetterOrNumber() || c == QLatin1Char('_'))) return false;
    return true;
}

bool VarParamTable::collectRefs(const QString& expr, QSet<QString>& refs, QString* err) const {
    if (expr.size() > kMaxExprLen) {
        if (err) *err = QStringLiteral("表达式过长（> %1 字符）").arg(kMaxExprLen);
        return false;
    }
    ExprParser p(expr, kMaxParseDepth, &refs, nullptr);
    double dummy = 0.0;
    return p.run(dummy, err);
}

bool VarParamTable::evalExpression(const QString& expr,
                                   const std::function<bool(const QString&, double&)>& resolve,
                                   double& out, QString* err) {
    if (expr.size() > kMaxExprLen) {
        if (err) *err = QStringLiteral("表达式过长（> %1 字符）").arg(kMaxExprLen);
        return false;
    }
    ExprParser p(expr, kMaxParseDepth, nullptr, &resolve);
    return p.run(out, err);
}

bool VarParamTable::evalExpression(const QString& expr, double& out, QString* err) {
    // 无上下文：任何标识符一律视为未定义变量
    const std::function<bool(const QString&, double&)> reject =
        [](const QString&, double&) -> bool { return false; };
    return evalExpression(expr, reject, out, err);
}

QString VarParamTable::define(const QString& name, const QString& expr) {
    if (!isValidName(name))
        return QStringLiteral("变量名非法：「%1」（首字符须为字母/下划线，仅含字母数字下划线，长度 ≤ %2）")
                 .arg(name).arg(kMaxNameLen);
    const QString e = expr.trimmed();
    if (e.isEmpty()) return QStringLiteral("变量 %1 的表达式为空").arg(name);

    QSet<QString> refs;
    QString err;
    if (!collectRefs(e, refs, &err)) return err;   // 语法错误 → 定义失败

    Var v;
    v.expr    = e;
    v.refs    = refs;
    v.hasRefs = !refs.isEmpty();
    auto it = vars_.find(name);
    const bool existed = (it != vars_.end());
    if (existed) v.value = it->second.value;       // 重定义保留旧值（求值后更新）

    if (!v.hasRefs) {
        // 常量表达式：定义期即求值，转为「值变量」（除零等错误定义期暴露）
        double val = 0.0;
        if (!evalExpression(e, val, &err)) return err;
        v.value = val;
    }
    if (!existed) order_.push_back(name);
    vars_[name] = v;
    return {};
}

QString VarParamTable::setValue(const QString& name, double val) {
    if (!isValidName(name))
        return QStringLiteral("变量名非法：「%1」").arg(name);
    Var v;
    v.expr = QString::number(val, 'g', 12);   // 显示用定义文本
    v.value = val;
    v.hasRefs = false;
    if (vars_.find(name) == vars_.end()) order_.push_back(name);
    vars_[name] = v;
    return {};
}

bool VarParamTable::remove(const QString& name) {
    if (vars_.erase(name) == 0) return false;
    for (auto it = order_.begin(); it != order_.end(); ++it) {
        if (*it == name) { order_.erase(it); break; }
    }
    return true;
}

void VarParamTable::clear() {
    vars_.clear();
    order_.clear();
}

bool VarParamTable::has(const QString& name) const {
    return vars_.find(name) != vars_.end();
}

QString VarParamTable::expression(const QString& name) const {
    auto it = vars_.find(name);
    return (it != vars_.end()) ? it->second.expr : QString();
}

bool VarParamTable::isExpression(const QString& name) const {
    auto it = vars_.find(name);
    return (it != vars_.end()) && it->second.hasRefs;
}

double VarParamTable::value(const QString& name) const {
    auto it = vars_.find(name);
    if (it == vars_.end()) return std::numeric_limits<double>::quiet_NaN();
    if (!it->second.hasRefs) return it->second.value;
    double out = 0.0;
    QString err;
    QSet<QString> stack;
    const std::map<QString, double> staging;   // 空：依赖取各自当前值
    if (!evalVar(name, staging, stack, out, &err))
        return std::numeric_limits<double>::quiet_NaN();
    return out;
}

QStringList VarParamTable::names() const {
    QStringList out;
    for (const auto& n : order_) out << n;
    return out;
}

bool VarParamTable::evalVar(const QString& name, const std::map<QString, double>& staging,
                            QSet<QString>& stack, double& out, QString* err) const {
    if (stack.contains(name)) {
        if (err) *err = QStringLiteral("变量循环依赖：「%1」").arg(name);
        return false;
    }
    auto s = staging.find(name);
    if (s != staging.end()) { out = s->second; return true; }
    auto it = vars_.find(name);
    if (it == vars_.end()) {
        if (err) *err = QStringLiteral("变量「%1」未定义").arg(name);
        return false;
    }
    if (!it->second.hasRefs) { out = it->second.value; return true; }

    stack.insert(name);
    const std::function<bool(const QString&, double&)> resolve =
        [&](const QString& n, double& v) -> bool { return evalVar(n, staging, stack, v, err); };
    double v = 0.0;
    const bool ok = evalExpression(it->second.expr, resolve, v, err);
    stack.remove(name);
    if (!ok) return false;
    if (!std::isfinite(v)) {
        if (err) *err = QStringLiteral("变量「%1」求值结果非有限值（数值溢出？）").arg(name);
        return false;
    }
    out = v;
    return true;
}

bool VarParamTable::resolveDrive(const QString& name, double val,
                                 QVector<QPair<QString, double>>& changed, QString* err) {
    changed.clear();
    auto it = vars_.find(name);
    if (it == vars_.end()) {
        if (err) *err = QStringLiteral("变量 %1 未定义").arg(name);
        return false;
    }
    if (it->second.hasRefs) {
        if (err)
            *err = QStringLiteral("变量 %1 由表达式定义，不能直接驱动（请驱动其依赖变量，或用 define 重定义）")
                     .arg(name);
        return false;
    }

    // ---- 1) 拓扑排序（Kahn）：表达式变量子图，dep→dependent；环检测 ----
    std::map<QString, int> indeg;
    std::map<QString, QVector<QString>> dependents;
    QVector<QString> expVars;
    for (const auto& n : order_) {
        auto vit = vars_.find(n);
        if (vit == vars_.end() || !vit->second.hasRefs) continue;
        expVars.push_back(n);
        indeg[n] = 0;
    }
    for (const QString& n : expVars) {
        for (const QString& r : vars_.at(n).refs) {
            auto rit = vars_.find(r);
            if (rit == vars_.end() || !rit->second.hasRefs) continue;  // 值变量/未定义：非拓扑节点
            dependents[r].push_back(n);
            indeg[n] += 1;
        }
    }
    QVector<QString> queue;
    for (const QString& n : expVars) if (indeg[n] == 0) queue.push_back(n);
    QVector<QString> topo;
    for (int qi = 0; qi < queue.size(); ++qi) {
        const QString n = queue[qi];
        topo.push_back(n);
        for (const QString& d : dependents[n])
            if (--indeg[d] == 0) queue.push_back(d);
    }
    if ((int)topo.size() != (int)expVars.size()) {
        QStringList cyc;
        for (const QString& n : expVars) if (indeg[n] > 0) cyc << n;
        if (err) *err = QStringLiteral("变量循环依赖：%1").arg(cyc.join(QStringLiteral(" → ")));
        return false;
    }

    // ---- 2) 演算（staging 先算后提交；任何失败整体放弃，不改动既有值）----
    std::map<QString, double> staging;
    staging[name] = val;
    for (const QString& n : topo) {
        double v = 0.0;
        QSet<QString> stack;
        if (!evalVar(n, staging, stack, v, err)) return false;
        staging[n] = v;
    }

    // ---- 3) 提交：驱动值在前，依赖变量按拓扑序；仅收集真变化 ----
    auto commitIfChanged = [&](const QString& n, double v) {
        auto vit = vars_.find(n);
        if (vit == vars_.end()) return;
        if (vit->second.value != v) {
            vit->second.value = v;
            changed.push_back(qMakePair(n, v));
        }
    };
    commitIfChanged(name, val);
    for (const QString& n : topo) commitIfChanged(n, staging[n]);
    return true;
}

} // namespace SketchCAD
