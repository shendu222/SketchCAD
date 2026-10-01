#pragma once
#include <QString>
#include <QStringList>

namespace SketchCAD {

/// 分段命令「选项关键字提示」工具集（纯函数，headless 可测）。
///
/// 约定：可选项以 "KEY/标签" 形式给出，例如：
///   "C/闭合"   "U/放弃"   "L/长度"   "A/角度"
/// - KEY 为英文短键（命令行输入，大小写不敏感）
/// - 标签 为中文可读名（也支持中文直接输入）
class CommandPrompt {
public:
    /// 取选项的英文键（"/" 之前的部分），如 "C/闭合" -> "C"。
    static QString optionKey(const QString& opt) {
        int i = opt.indexOf(QChar('/'));
        return (i < 0) ? opt : opt.left(i);
    }
    /// 取选项的中文标签（"/" 之后的部分），如 "C/闭合" -> "闭合"。
    static QString optionLabel(const QString& opt) {
        int i = opt.indexOf(QChar('/'));
        return (i < 0) ? opt : opt.mid(i + 1);
    }

    /// 将选项列表格式化为状态栏/命令行提示文本，如 "[C/闭合 U/放弃]"。
    /// 空列表返回空字符串（不显示方括号）。
    static QString formatOptionPrompt(const QStringList& opts) {
        if (opts.isEmpty()) return QString();
        return QStringLiteral("[") + opts.join(QChar(' ')) + QStringLiteral("]");
    }

    /// 匹配用户输入到某个选项，返回该选项的英文键（大写原样），未命中返回空串。
    /// 匹配规则（大小写不敏感）：输入 == KEY，或输入 == 标签。
    /// 例如 matchOption("c", {"C/闭合","U/放弃"}) -> "C"；
    ///       matchOption("闭合", {...})            -> "C"。
    static QString matchOption(const QString& input, const QStringList& opts) {
        const QString s = input.trimmed();
        if (s.isEmpty()) return QString();
        const QString sl = s.toLower();
        for (const QString& opt : opts) {
            const QString key = optionKey(opt);
            const QString label = optionLabel(opt);
            if (key.toLower() == sl) return key;
            if (!label.isEmpty() && label.toLower() == sl) return key;
        }
        return QString();
    }
};

} // namespace SketchCAD
