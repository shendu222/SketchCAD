#include "TextStyle.h"
#include "SketchModel.h"
#include "Inquiry.h"     // AREA/PERIM 域：复用查询测量层的封闭面积/周长（只读）
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QFontMetricsF>
#include <cmath>

namespace SketchCAD {

namespace {

// 域求值异常/断链/非法时的占位文本（需求约束：崩溃安全，回落 "###"）
constexpr const char* kFieldFallback = "###";

// 判断单元格文本是否为公式（=sum/=count 列内行号引用）
inline bool isFormulaCell(const QString& s) {
    return s.startsWith(QLatin1Char('='));
}

// 解析公式头之后的实参序列 "a,b" → 整数列表（允许空白；非数字 → 失败）
bool parseFormulaArgs(const QString& argStr, int& a, int& b) {
    const QStringList parts = argStr.split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (parts.size() != 2) return false;
    bool okA = false, okB = false;
    a = parts[0].trimmed().toInt(&okA);
    b = parts[1].trimmed().toInt(&okB);
    return okA && okB;
}

// 表格单元格原始文本（行号 1 基；越界返回空串）
QString cellAt(const Element& tbl, int row1based, int col) {
    return tbl.tableCell(row1based - 1, col);
}

} // namespace

// ---- 单域求值：AREA:id / PERIM:id / PROP:name / COUNT（其余返回 !ok）----
FieldEvalResult evalFieldCode(const QString& code, const SketchModel* model) {
    FieldEvalResult r;
    r.text = QString::fromLatin1(kFieldFallback);
    // 求值只读不改几何；任何异常路径都收敛到 "###"（崩溃安全约束）
    try {
        const int colon = code.indexOf(QLatin1Char(':'));
        const QString head = (colon >= 0 ? code.left(colon) : code).toUpper();
        const QString arg  = (colon >= 0 ? code.mid(colon + 1) : QString());

        if (head == QLatin1String("DATE") || head == QLatin1String("TIME") ||
            head == QLatin1String("FILENAME") || head == QLatin1String("FILEPATH")) {
            // 无参系统域：与 resolveFields 原行为一致（保留快速通道语义）
            const QDateTime now = QDateTime::currentDateTime();
            if (head == QLatin1String("DATE"))    { r.text = now.toString(QStringLiteral("yyyy-MM-dd")); r.ok = true; return r; }
            if (head == QLatin1String("TIME"))    { r.text = now.toString(QStringLiteral("HH:mm:ss"));   r.ok = true; return r; }
            const QString path = model ? model->drawingPath() : QString();
            const bool empty = path.isEmpty();
            if (head == QLatin1String("FILENAME")) r.text = empty ? QStringLiteral("未命名") : QFileInfo(path).fileName();
            else                                   r.text = empty ? QStringLiteral("未命名") : path;
            r.ok = true; return r;
        }

        if (head == QLatin1String("COUNT")) {
            r.text = QString::number(model ? model->elementCount() : 0);
            r.ok = true; return r;
        }

        if (!model) return r;   // 无模型上下文 → AREA/PERIM/PROP 一律 ###

        if (head == QLatin1String("PROP")) {
            if (arg.isEmpty()) return r;
            const QString v = model->drawingProp(arg);
            if (v.isEmpty()) return r;         // 缺名/空值 → ###（与断链同语义）
            r.text = v; r.ok = true; return r;
        }

        if (head == QLatin1String("AREA") || head == QLatin1String("PERIM")) {
            bool idOk = false;
            const int id = arg.toInt(&idOk);
            if (!idOk) return r;
            const Element* e = model->element(id);
            if (!e) return r;                  // 断链 → ###
            const inquiry::AreaResult a = inquiry::areaOfElement(*e);
            if (!a.ok) return r;               // 非封闭图元 → ###
            r.text = QString::number(head == QLatin1String("AREA") ? a.area : a.perimeter,
                                     'f', 2);
            r.ok = true; return r;
        }
    } catch (...) {
        // 任何意外异常 → 保持 "###" 回落（不向渲染/打印/导出扩散）
    }
    return r;
}

QString resolveFields(const QString& raw, const SketchModel* model) {
    // 快速路径：两种占位语法都没有 → 原样返回（无域旧档零开销，向后兼容）
    if (!raw.contains(QStringLiteral("%<")) &&
        !raw.contains(QStringLiteral("{field:")))
        return raw;

    QString out;
    out.reserve(raw.size() + 16);
    int i = 0;
    const int n = raw.size();
    while (i < n) {
        // 形式一：%<CODE>%
        if (raw.mid(i, 2) == QStringLiteral("%<")) {
            const int end = raw.indexOf(QStringLiteral(">%"), i + 2);
            if (end < 0) { out += raw[i]; ++i; continue; }   // 未闭合 → 原样透传
            const QString code = raw.mid(i + 2, end - (i + 2));
            const FieldEvalResult fr = evalFieldCode(code, model);
            out += fr.ok ? fr.text : QString::fromLatin1(kFieldFallback);
            i = end + 2;
            continue;
        }
        // 形式二：{field:code}（花括号语法，两种形式语义等价）
        if (raw.mid(i, 7) == QStringLiteral("{field:")) {
            const int end = raw.indexOf(QLatin1Char('}'), i + 7);
            if (end < 0) { out += raw[i]; ++i; continue; }
            const QString code = raw.mid(i + 7, end - (i + 7));
            const FieldEvalResult fr = evalFieldCode(code, model);
            out += fr.ok ? fr.text : QString::fromLatin1(kFieldFallback);
            i = end + 1;
            continue;
        }
        out += raw[i];
        ++i;
    }
    return out;
}

// ---- Table 公式单元格求值：=sum(a,b) / =count(a,b)（本列行号引用，1 基）----
FieldEvalResult evalTableCellFormula(const Element& tbl, int row, int col,
                                     const SketchModel* model) {
    FieldEvalResult r;
    r.text = QString::fromLatin1(kFieldFallback);
    const QString raw = tbl.tableCell(row - 1, col);
    if (!isFormulaCell(raw)) { r.text = raw; r.ok = true; return r; }

    try {
        const QString body = raw.mid(1).trimmed();
        const int lp = body.indexOf(QLatin1Char('('));
        const int rp = body.lastIndexOf(QLatin1Char(')'));
        if (lp < 0 || rp <= lp) return r;                    // 语法不完整 → ###
        const QString fn = body.left(lp).trimmed().toLower();
        int a = 0, b = 0;
        if (!parseFormulaArgs(body.mid(lp + 1, rp - lp - 1), a, b)) return r;
        if (a > b) std::swap(a, b);
        if (a < 1 || b > tbl.tableRows) return r;            // 行号越界 → ###

        if (fn == QLatin1String("sum")) {
            double total = 0.0;
            for (int rr = a; rr <= b; ++rr) {
                const QString cell = cellAt(tbl, rr, col);
                if (isFormulaCell(cell)) return r;           // 引用公式单元格 → 防递归 ###
                // 被引用单元格先过域解析（面积/周长域列合计场景），再做数值解析
                const QString cleaned = resolveFields(cell, model).simplified();
                bool numOk = false;
                const double v = QLocale::c().toDouble(cleaned, &numOk);
                if (numOk) total += v;                       // 非数字按 0 计（表头/文字行）
            }
            r.text = QString::number(total, 'f', 2);
            r.ok = true; return r;
        }
        if (fn == QLatin1String("count")) {
            int cnt = 0;
            for (int rr = a; rr <= b; ++rr) {
                const QString cell = cellAt(tbl, rr, col);
                if (isFormulaCell(cell)) return r;           // 引用公式单元格 → 防递归 ###
                if (!cell.trimmed().isEmpty()) ++cnt;
            }
            r.text = QString::number(cnt);
            r.ok = true; return r;
        }
    } catch (...) {
        // 崩溃安全：异常 → "###"
    }
    return r;   // 未知函数名 → ###
}

// ---- Table 单元格显示统一管线（渲染/打印/导出三路同源，防三处手工拼装漂移）----
QString displayTableCell(const Element& tbl, int row, int col, const SketchModel* model) {
    // 1) 公式先行：=sum/=count（列内行号引用；非公式原样返回）
    const FieldEvalResult fr = evalTableCellFormula(tbl, row, col, model);
    const QString stage1 = fr.ok ? fr.text : QString::fromLatin1(kFieldFallback);
    // 2) 域替换：%<...>% 与 {field:...}（非公式原文常含域；公式结果已无域语法，幂等无害）
    return resolveFields(stage1, model);
}

ResolvedTextStyle resolveTextStyle(const Element& e, const SketchModel* model) {
    ResolvedTextStyle r;
    const TextStyle* ts = nullptr;
    if (model && !e.styleName.isEmpty()) ts = model->findTextStyle(e.styleName);
    r.fontFamily     = ts ? ts->fontFamily : QString();
    r.widthFactor    = (ts ? ts->widthFactor : 1.0) * (e.widthFactor > 0 ? e.widthFactor : 1.0);
    r.obliquingAngle = ts ? ts->obliquingAngle : 0.0;
    r.bold           = ts ? ts->bold   : false;
    r.italic         = ts ? ts->italic : false;
    // 字高：样式有非零字高则优先（AutoCAD 语义），否则用图元自身 radius
    const double styleH = ts ? ts->textHeight : 0.0;
    r.height = (styleH > 0.1) ? styleH : (e.radius > 0.1 ? e.radius : 3.5);
    return r;
}

QStringList wrapText(const QString& text, double maxWidthMm, const QFont& font) {
    QStringList result;
    if (maxWidthMm <= 0.0) { result = text.split('\n'); return result; }
    const QFontMetricsF fm(font);
    const QStringList paras = text.split('\n');
    for (const QString& para : paras) {
        if (para.isEmpty()) { result.append(QString()); continue; }
        QString line;
        double width = 0.0;
        for (const QChar& ch : para) {
            const double cw = fm.horizontalAdvance(QString(ch));
            if (!line.isEmpty() && width + cw > maxWidthMm) {
                result.append(line);
                line.clear(); width = 0.0;
            }
            line += ch;
            width += cw;
        }
        if (!line.isEmpty()) result.append(line);
    }
    return result;
}

} // namespace SketchCAD
