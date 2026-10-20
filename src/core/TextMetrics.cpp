// ============================================================================
// TextMetrics —— 文字度量单一数据源（屏幕 / 打印 / 取景 三处同源）
// 详见 TextMetrics.h 头部说明。所有度量以世界 mm 为单位，无头安全。
// ============================================================================
#include "TextMetrics.h"

#include "Element.h"      // Element / ElementType
#include "SketchModel.h"  // SketchModel（仅指针/前向，编译期依赖轻）

#include <QCoreApplication>
#include <QGuiApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QHash>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// ---------------------------------------------------------------------------
// LRU 缓存（key = FontSig+字符 的合并哈希）—— 必须在方法定义前声明
// ---------------------------------------------------------------------------
namespace {
struct LRUCache {
    static const int kCap = 8192;
    QHash<quint64, double> map;
    QVector<quint64>       order;   // 队首 = MRU
    double get(quint64 k) {
        auto it = map.find(k);
        if (it == map.end()) return -1.0;
        const int idx = order.indexOf(k);
        if (idx > 0) { order.remove(idx); order.prepend(k); }
        return *it;
    }
    void put(quint64 k, double v) {
        const int idx = order.indexOf(k);
        if (idx >= 0) { order.remove(idx); order.prepend(k); map[k] = v; return; }
        map.insert(k, v); order.prepend(k);
        if (order.size() > kCap) { const quint64 old = order.takeLast(); map.remove(old); }
    }
};
LRUCache g_cache;

quint64 makeKey(const TextMetrics::FontSig& sig, QChar ch) {
    quint64 h = (quint64(qHash(sig.family)) ^
                 (quint64(uint(sig.stretch) & 0xFFFF) << 16) ^
                 (quint64(sig.bold) << 8) ^
                 (quint64(sig.italic) << 9));
    h = h * 2654435761u + (quint64(ch.unicode()) * 40503u);
    return h;
}

bool isCjk(QChar c) {
    const uint u = c.unicode();
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0x3000 && u <= 0x303F) ||
           (u >= 0xFF00 && u <= 0xFFEF) || (u >= 0x3400 && u <= 0x4DBF);
}
} // namespace

// ---------------------------------------------------------------------------
// 静态状态
// ---------------------------------------------------------------------------
static double            g_fallbackCharWidth = 0.62;          // 单字符平均宽/字高（与既有估算一致）
static TextMetrics::WarnFn g_warn = [](const QString& m) {
    // 默认提示通道：Qt 警告（明确、可见、不静默）。
    qWarning("%s", m.toUtf8().constData());
};

void TextMetrics::setFallbackCharWidth(double ratio) {
    if (ratio > 0.0 && std::isfinite(ratio)) g_fallbackCharWidth = ratio;
}
double TextMetrics::fallbackCharWidth() { return g_fallbackCharWidth; }

void TextMetrics::setWarnHandler(WarnFn fn) { if (fn) g_warn = fn; }
void TextMetrics::warn(const QString& msg) { if (g_warn) g_warn(msg); }

void TextMetrics::clearCache() { g_cache.map.clear(); g_cache.order.clear(); }

// ---------------------------------------------------------------------------
// GUI 可用性 / 字体可用性
// ---------------------------------------------------------------------------
bool TextMetrics::isGuiAvailable() {
    // 自测强制覆盖：即便进程持有 QApplication（t_interaction 需要），也回归"无头"
    // 判定，使无头降级（确定性估算）路径的回归断言保持可执行。主程序从不设置。
    if (g_headlessOverride) return false;
    // SketchSelfTest 用 QCoreApplication（无头）→ 视为不可用 → 降级估算（确定性）。
    // 主程序用 QApplication（QGuiApplication 子类）→ 可用真实度量。
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
}

static bool g_familyOk(const QString& family) {
    if (family.isEmpty()) return true;            // 空 = 走系统默认字体
    static QHash<QString, bool> cache;
    auto it = cache.find(family);
    if (it != cache.end()) return *it;
    QFont f; f.setFamily(family); f.setPixelSize(1000);
    QFontInfo fi(f);
    // 字体缺失时 Qt 会回退到 substitute，fi.family() 与请求不一致 → 视为缺失。
    bool ok = (fi.family().compare(family, Qt::CaseInsensitive) == 0);
    cache.insert(family, ok);
    return ok;
}
bool TextMetrics::familyOk(const QString& family) { return g_familyOk(family); }

// 参考像素尺寸(1000)下的字形 advance（px）。无头返回确定性估算基准。
double TextMetrics::charAdvancePx1000(const FontSig& sig, QChar ch) {
    if (!isGuiAvailable()) {
        // 无头：统一按 fallback 系数，使估算 == size*h*ratio（与旧 estTextWidth 一致）。
        return 1000.0 * g_fallbackCharWidth;
    }
    const quint64 k = makeKey(sig, ch);
    const double cached = g_cache.get(k);
    if (cached >= 0.0) return cached;
    QFont f;
    if (!sig.family.isEmpty()) f.setFamily(sig.family);
    f.setPixelSize(1000);
    f.setBold(sig.bold);
    f.setItalic(sig.italic);
    f.setStretch(sig.stretch);
    const QFontMetricsF fm(f);
    const double v = fm.horizontalAdvance(ch);
    g_cache.put(k, v);
    return v;
}

// ---------------------------------------------------------------------------
// 行宽（mm）
// ---------------------------------------------------------------------------
double TextMetrics::estimateLineWidthMm(const ResolvedTextStyle& rs, const QString& s, double heightMm) {
    // 确定性估算：字符数 × 字高 × 系数 × 宽度因子。
    return s.size() * heightMm * g_fallbackCharWidth * rs.widthFactor;
}

double TextMetrics::lineWidthMm(const ResolvedTextStyle& rs, const QString& s, double heightMm) {
    if (s.isEmpty() || heightMm <= 0.0) return 0.0;
    if (!isGuiAvailable()) return estimateLineWidthMm(rs, s, heightMm);
    if (!familyOk(rs.fontFamily)) {
        warn(QStringLiteral("[TextMetrics] 字体「%1」不可用，按估算度量（误差上限约 ±30%）").arg(rs.fontFamily));
        return estimateLineWidthMm(rs, s, heightMm);
    }
    const FontSig sig{ rs.fontFamily, int(std::lround(rs.widthFactor * 100.0)),
                       rs.bold, rs.italic };
    double sum = 0.0;
    for (QChar ch : s) sum += charAdvancePx1000(sig, ch);
    return sum / 1000.0 * heightMm;   // 参考像素(1000) → 世界 mm
}

// ---------------------------------------------------------------------------
// 多行布局（mm）：\n 分段 + mtextWidth 真实折行
// ---------------------------------------------------------------------------
static bool canBreakBefore(const QString& s, int i) {
    if (i <= 0) return false;
    const QChar prev = s[i - 1], cur = s[i];
    if (isCjk(prev) || isCjk(cur)) return true;                 // CJK 任意字间可断
    const ushort u = cur.unicode();                             // 拉丁/全角标点断词点
    return u == ' '   || u == '-'   || u == 0xFF0C /*，*/ || u == 0x3001 /*、*/ ||
           u == 0x3002 /*。*/ || u == 0xFF1B /*；*/ || u == 0xFF1A /*：*/;
}

TextLayout TextMetrics::layout(const ResolvedTextStyle& rs, const QString& text,
                              double heightMm, double mtextWidthMm, double lineSpacing) {
    TextLayout out;
    out.lineHeightMm = heightMm * (lineSpacing > 0.0 ? lineSpacing : 1.0);
    if (text.isEmpty() || heightMm <= 0.0) { out.heightMm = 0.0; return out; }

    if (containsStackedFraction(text))
        warn(QStringLiteral("[TextMetrics] MText 堆叠分数({\\S 上/下;}) 本轮不支持，按原文字面渲染（未静默退化）"));

    const QStringList paras = text.split(QLatin1Char('\n'));
    double maxW = 0.0;
    bool wrap = mtextWidthMm > 0.1;

    for (const QString& para : paras) {
        if (!wrap || para.isEmpty()) {
            TextLayoutLine l;
            l.text    = para;
            l.widthMm = lineWidthMm(rs, para, heightMm);
            out.lines.append(l);
            maxW = std::max(maxW, l.widthMm);
            continue;
        }
        // 贪心折行：CJK 任意字间可断；拉丁按空格/连字符/标点断词；超长硬断。
        int  lineStart = 0;
        int  lastBreak = -1;            // 当前行内最近的可断点（空格/标点索引）
        double w = 0.0;
        for (int i = 0; i < para.size(); ++i) {
            const QChar ch = para[i];
            const double cw = lineWidthMm(rs, QString(ch), heightMm);
            if (canBreakBefore(para, i)) lastBreak = i;
            if (w + cw > mtextWidthMm && i > lineStart) {
                int breakAt;
                if (i > lineStart && (isCjk(ch) || isCjk(para[i - 1]))) {
                    breakAt = i;                                  // CJK：当前字前断
                } else if (lastBreak > lineStart) {
                    breakAt = lastBreak + 1;                      // 拉丁：最近空格/标点后断
                } else {
                    breakAt = i;                                  // 硬断（超长词）
                }
                TextLayoutLine l;
                l.text    = para.mid(lineStart, breakAt - lineStart);
                l.widthMm = lineWidthMm(rs, l.text, heightMm);
                out.lines.append(l);
                maxW = std::max(maxW, l.widthMm);
                lineStart = breakAt;
                w = lineWidthMm(rs, para.mid(lineStart, i - lineStart + 1), heightMm);
                lastBreak = -1;
            } else {
                w += cw;
            }
        }
        // 收尾行
        TextLayoutLine l;
        l.text    = para.mid(lineStart);
        l.widthMm = lineWidthMm(rs, l.text, heightMm);
        out.lines.append(l);
        maxW = std::max(maxW, l.widthMm);
    }

    out.widthMm  = maxW;
    out.heightMm = out.lines.size() * out.lineHeightMm;
    return out;
}

// ---------------------------------------------------------------------------
// Text / MText 世界包围盒（mm，含 9 点对齐）
// ---------------------------------------------------------------------------
QRectF TextMetrics::elementBoundsMm(const Element& e, const SketchModel* model) {
    if (e.pts.empty() || e.text.isEmpty()) return QRectF();
    const ResolvedTextStyle rs = resolveTextStyle(e, model);
    const double h = rs.height;                       // mm（已是 style×图元 合并后的字高）
    const QString display = resolveFields(e.text, model);
    const bool isM = (e.type == ElementType::MText);
    const double mw = (isM && e.mtextWidth > 0.1) ? e.mtextWidth : 0.0;
    const double ls = isM ? e.lineSpacing : 1.0;
    const TextLayout lay = layout(rs, display, h, mw, ls);

    const int  align = e.textAlign;                   // 9 点对齐（与 paint 两处同源）
    const int  hA = align % 3, vA = align / 3;       // 0左/中/右 ; 0上/中/下
    const double W = lay.widthMm, H = lay.heightMm;
    const double xL = (hA == 0) ? 0.0 : (hA == 1) ? -W / 2.0 : -W;
    const double yTop = (vA == 0) ? 0.0 : (vA == 1) ? H / 2.0 : H;
    const double yBot = yTop - H;
    return QRectF(e.pts[0].x + xL, e.pts[0].y + yBot, W, H);
}

// ---------------------------------------------------------------------------
// 表格列宽
// ---------------------------------------------------------------------------
QVector<double> TextMetrics::autoFitTableColumns(const Element& e, const SketchModel* model, double padMm) {
    const int C = std::max(1, e.tableCols);
    const int R = std::max(1, e.tableRows);
    const double th = e.radius > 0.1 ? e.radius : 3.5;
    const ResolvedTextStyle base = resolveTextStyle(e, model);
    ResolvedTextStyle crs;
    crs.fontFamily  = base.fontFamily;
    crs.widthFactor = base.widthFactor;
    crs.bold        = base.bold;
    crs.italic      = base.italic;
    crs.height      = th;
    QVector<double> w(C, 0.0);
    for (int c = 0; c < C; ++c) {
        double maxc = 0.0;
        for (int r = 0; r < R; ++r) {
            const QString raw = e.tableCell(r, c);
            if (raw.trimmed().isEmpty()) continue;
            const QString disp = resolveFields(raw, model);
            maxc = std::max(maxc, lineWidthMm(crs, disp, th));
        }
        w[c] = maxc + 2.0 * padMm;
    }
    return w;
}

QVector<double> TextMetrics::tableColumnWidths(const Element& e, const SketchModel* model, double padMm) {
    const int C = std::max(1, e.tableCols);
    if ((int)e.colWidths.size() == C) {
        bool allPos = true;
        for (double w : e.colWidths) if (!(w > 0.0)) { allPos = false; break; }
        if (allPos) {
            QVector<double> w(C);
            for (int c = 0; c < C; ++c) w[c] = e.tableColWidth(c);
            return w;
        }
    }
    return autoFitTableColumns(e, model, padMm);
}

QString TextMetrics::elideToWidthMm(const ResolvedTextStyle& rs, const QString& s,
                                    double heightMm, double maxWidthMm) {
    if (maxWidthMm <= 0.0) return s;
    if (lineWidthMm(rs, s, heightMm) <= maxWidthMm) return s;
    const QString ell = QStringLiteral("…");
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        const QString cand = out + s[i] + ell;
        if (lineWidthMm(rs, cand, heightMm) > maxWidthMm) break;
        out += s[i];
    }
    return out + ell;
}

// ---------------------------------------------------------------------------
// 堆叠分数检测
// ---------------------------------------------------------------------------
bool TextMetrics::containsStackedFraction(const QString& t) {
    if (t.contains(QLatin1String("{\\S"))) return true;   // {\S 上/下;}
    if (t.contains(QLatin1String("\\S")))  return true;   // \S...;
    for (int i = 0; i + 1 < t.size(); ++i)               // ^I / ^J 上下堆叠
        if (t[i] == QLatin1Char('^') &&
            (t[i + 1] == QLatin1Char('I') || t[i + 1] == QLatin1Char('J')))
            return true;
    return false;
}

} // namespace SketchCAD
