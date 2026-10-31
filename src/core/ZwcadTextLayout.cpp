#include "ZwcadTextLayout.h"
#include "TextMetrics.h"
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace ZwcadText {

// ---------------------------------------------------------------------------
// 内容解析：MText 行内码状态机
// ---------------------------------------------------------------------------
namespace {

// 解析 \p...; 的单个 token（如 "i1.5l" / "c-2" / "t12,24"）→ 缩进值(mm)。
// 返回 false = 非 i/c token（如制表 t，本轮跳过）。单位：l = 行槽高倍数，缺省 = mm。
bool parseIndentToken(const QString& tok, double slotHeightMm, double& outMm) {
    if (tok.isEmpty()) return false;
    const QChar kind = tok[0];
    if (kind != QLatin1Char('i') && kind != QLatin1Char('c')) return false;
    // 数字部分：[+-]?数字[.数字]
    int j = 1, n = tok.size();
    if (j < n && (tok[j] == QLatin1Char('+') || tok[j] == QLatin1Char('-'))) ++j;
    int numEnd = j;
    while (numEnd < n && (tok[numEnd].isDigit() || tok[numEnd] == QLatin1Char('.'))) ++numEnd;
    bool ok = false;
    const double v = tok.mid(1, numEnd - 1).toDouble(&ok);
    if (!ok) return false;
    const bool inLines = (numEnd < n && tok[numEnd] == QLatin1Char('l'));
    outMm = v * (inLines ? slotHeightMm : 1.0);
    return true;
}

} // namespace

QVector<Paragraph> parseParagraphs(const QString& content, double slotHeightMm) {
    QVector<Paragraph> paras;               // flush 追加；空内容 → 单空段
    QString cur;
    double firstInd = 0.0, leftInd = 0.0;

    auto flush = [&]() {
        Paragraph p;
        p.text = cur;
        p.firstIndentMm = firstInd;
        p.leftIndentMm  = leftInd;
        paras.append(p);
        cur.clear();
        firstInd = 0.0;                     // 缩进只作用于其所在段落（ZWCAD 语义）
        leftInd  = 0.0;
    };

    for (int i = 0; i < content.size(); ++i) {
        const QChar ch = content[i];
        if (ch != QLatin1Char('\\')) {
            if (ch == QLatin1Char('\n')) { flush(); continue; }   // 裸 \n = 段落分隔（与 \P 同义）
            if (ch == QLatin1Char('{') || ch == QLatin1Char('}')) continue;  // 分组括号剥离
            cur += ch;
            continue;
        }
        // ---- 转义 / 行内格式码 ----
        if (i + 1 >= content.size()) { cur += QChar('\\'); break; }          // 尾随反斜杠保字面
        const QChar c = content[++i];
        switch (c.unicode()) {
        case 'P':                                              // \P 段落分隔
            flush();
            break;
        case 'p': {                                            // \p...; 段落属性（缩进）
            int end = content.indexOf(QLatin1Char(';'), i + 1);
            if (end < 0) end = content.size();
            const QString opts = content.mid(i + 1, end - i - 1);
            const QStringList toks = opts.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString& raw : toks) {
                const QString tok = raw.trimmed();
                double v = 0.0;
                if (parseIndentToken(tok, slotHeightMm, v)) {
                    if (tok[0] == QLatin1Char('i')) firstInd = v;
                    else                            leftInd  = v;
                }
                // t(制表位) 等其余 token 本轮跳过（绝不按字面渲染）
            }
            i = end;                                           // 消费到 ';'（含）
            break;
        }
        case '\\': cur += QChar('\\'); break;                  // \\ → 字面反斜杠
        case '{':  cur += QChar('{');  break;                  // \{ \} → 字面花括号
        case '}':  cur += QChar('}');  break;
        case '~':  cur += QChar(' ');  break;                  // \~ → 不换行空格
        case 'L': case 'l':                                    // 下划线开关
        case 'O': case 'o':                                    // 上划线开关
        case 'K':                                              // 删除线开关
        case 'N':                                              // \N 换列（本轮跳过）
            break;
        case 'C': case 'c':                                    // 颜色
        case 'H':                                              // 字高
        case 'W':                                              // 宽度
        case 'Q':                                              // 倾角
        case 'T':                                              // 字距
        case 'A': {                                            // 段内对齐
            int end = content.indexOf(QLatin1Char(';'), i + 1);
            if (end < 0) end = content.size();
            i = end;                                           // 整码剥离
            break;
        }
        case 'F': {                                            // 字体（可含 | 大字体）
            int end = content.indexOf(QLatin1Char(';'), i + 1);
            if (end < 0) end = content.size();
            i = end;
            break;
        }
        default:                                               // 未知码：保字面（宽容）
            cur += QChar('\\');
            cur += c;
            break;
        }
    }
    flush();

    // ZWCAD 语义：内容结尾的 \P 不产生尾部空段（末段为空且非首段时移除——
    // 仅当源自显式尾随分隔符；\P\P 的中间空段保留）。此处末段空 + 原文以分隔
    // 结尾即等价于「\P 结尾」，统一移除末空段；纯空内容保持单空段。
    if (paras.size() > 1 && paras.last().text.isEmpty())
        paras.removeLast();
    return paras;
}

// ---------------------------------------------------------------------------
// 多行布局（MTEXT 规范）
// ---------------------------------------------------------------------------
Result layoutMText(const QString& content, const Format& f, const ResolvedTextStyle& style) {
    Result out;
    const double h = (f.heightMm > 0.05) ? f.heightMm : 3.5;
    out.slotHeightMm = clampSpacingFactor(f.lineSpacingFactor) * kSingleSpacing * h;
    if (content.isEmpty() || h <= 0.0) return out;

    if (TextMetrics::containsStackedFraction(content))
        TextMetrics::warnUnsupported(QStringLiteral(
            "[ZwcadText] MText 堆叠分数({\\S 上/下;}) 本轮不支持，按原文字面渲染（未静默退化）"));

    const auto paras = parseParagraphs(content, out.slotHeightMm);
    const bool   wrap = f.wrapWidthMm > 0.1;
    const int    hA = f.attach % 3, vA = f.attach / 3;   // 0左/中/右 ; 0上/中/下

    struct Placed { QString text; double w; double x; };
    QVector<Placed> placed;
    double maxW = 0.0;

    for (const Paragraph& para : paras) {
        const double avail = wrap ? std::max(0.5, f.wrapWidthMm - para.leftIndentMm) : 0.0;
        const auto lines = TextMetrics::wrapParagraph(style, para.text, h, avail);
        for (int k = 0; k < lines.size(); ++k) {
            const bool paraFirst = (k == 0);
            const double colL = para.leftIndentMm + (paraFirst ? para.firstIndentMm : 0.0);
            double x;
            if (hA == 0)      x = colL;                                   // 左：行首=列左缘+缩进
            else if (hA == 1) x = wrap ? colL + (f.wrapWidthMm - colL - lines[k].widthMm) / 2.0
                                       : -lines[k].widthMm / 2.0;          // 中：列内居中
            else              x = (wrap ? f.wrapWidthMm : 0.0) - lines[k].widthMm; // 右：列右缘
            placed.append({lines[k].text, lines[k].widthMm, x});
            maxW = std::max(maxW, lines[k].widthMm);
        }
    }

    const int n = placed.size();
    const double H = n * out.slotHeightMm;
    const double blockTop = (vA == 0) ? 0.0 : (vA == 1) ? -H / 2.0 : -H;  // Y 向下：上=0 / 中=-H/2 / 下=-H
    out.lines.reserve(n);
    for (int i = 0; i < n; ++i) {
        LineBox lb;
        lb.text      = placed[i].text;
        lb.widthMm   = placed[i].w;
        lb.xMm       = placed[i].x;
        lb.yCenterMm = blockTop + (i + 0.5) * out.slotHeightMm;   // 首行最靠顶（行序自上而下）
        out.lines.append(lb);
    }
    out.widthMm  = maxW;
    out.heightMm = H;
    return out;
}

// ---------------------------------------------------------------------------
// 单行布局（TEXT 规范）：行槽 = 字高（无行距概念），9 点对齐同 MTEXT
// ---------------------------------------------------------------------------
Result layoutSingleLine(const QString& content, const Format& f, const ResolvedTextStyle& style) {
    Result out;
    const double h = (f.heightMm > 0.05) ? f.heightMm : 3.5;
    out.slotHeightMm = h;
    if (content.isEmpty()) return out;

    // TEXT 为单行实体：内嵌换行按 ZWCAD 行为折叠为空格（绝不渲染成两行）
    const QString oneLine = QString(content).replace(QLatin1Char('\n'), QChar(' '));
    LineBox lb;
    lb.text    = oneLine;
    lb.widthMm = TextMetrics::lineWidthMm(style, oneLine, h);
    const int hA = f.attach % 3, vA = f.attach / 3;
    lb.xMm       = (hA == 0) ? 0.0 : (hA == 1) ? -lb.widthMm / 2.0 : -lb.widthMm;
    lb.yCenterMm = (vA == 0) ? h / 2.0 : (vA == 1) ? 0.0 : -h / 2.0;   // 上=字就在锚点下方 / 中 / 下
    out.lines.append(lb);
    out.widthMm  = lb.widthMm;
    out.heightMm = h;
    return out;
}

} // namespace ZwcadText
} // namespace SketchCAD
