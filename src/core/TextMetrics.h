#pragma once
// ============================================================================
// TextMetrics —— 文字度量单一数据源（屏幕 / 打印 / 取景 三处同源）
// ----------------------------------------------------------------------------
// 历史：屏幕(GraphicsView::paintText) 与打印(PlotRenderer::paintText) 已使用
//   QFontMetricsF 真实度量，但「取景」(PrintEngine::textBoxOf) 用 CJK≈1em /
//   ASCII≈0.55em 的粗估，且忽略宽度因子/斜体/倾角、不按 mtextWidth 折行、不处理
//   9 点对齐 —— 这就是「屏幕看着对、打印/导出位移换行错」的根因。
// 本模块把所有文字度量收敛为同一套 API：
//   - 真实度量：QFontMetricsF（按字体族/宽度因子/粗斜体），带 LRU 缓存。
//   - 无头安全：无 QGuiApplication（如 SketchSelfTest 的 QCoreApplication）或字体
//     缺失时确定性降级为估算（系数可配，默认 0.62，与既有 Gdt/DimStyle 估算一致），
//     并写日志（不静默退化）。
//   - 单位统一为「世界 mm」，与视图缩放无关；各渲染站点拿到 mm 布局后再按自身
//     scale(z 或 scaleDev) 映射到设备像素，从而三处像素结果逐字节一致。
// ============================================================================
#include <QString>
#include <QStringList>
#include <QRectF>
#include <QVector>
#include "TextStyle.h"   // ResolvedTextStyle / resolveTextStyle / resolveFields

namespace SketchCAD {

class Element;
class SketchModel;

// 单行布局结果（mm）
struct TextLayoutLine {
    QString text;       // 该行（已按 \n 或 mtextWidth 折行切分）的内容
    double  widthMm = 0; // 该行渲染宽度(mm)
};

// 多行文字布局（全部以世界 mm 为单位）
struct TextLayout {
    QVector<TextLayoutLine> lines;
    double widthMm      = 0;   // 最宽行宽度(mm)
    double heightMm     = 0;   // 总行高(mm) = lines.size() * lineHeightMm
    double lineHeightMm = 0;   // 单行高度(mm) = heightMm * lineSpacing(MText)
};

class TextMetrics {
public:
    // 字体签名（度量缓存 key 的组成）：公开以便匿名命名空间内的 LRU 缓存辅助函数访问。
    struct FontSig {
        QString family;
        int     stretch = 100;   // = round(widthFactor*100)
        bool    bold = false;
        bool    italic = false;
        bool operator==(const FontSig& o) const {
            return family == o.family && stretch == o.stretch &&
                   bold == o.bold && italic == o.italic;
        }
    };
    // ---- 无头/字体缺失降级估算系数（单字符平均宽 / 字高，默认 0.62）----
    static void   setFallbackCharWidth(double ratio);
    static double fallbackCharWidth();

    // ---- 字体度量是否可用：需要 QGuiApplication；无头 QCoreApplication 视为不可用 ----
    // setHeadlessOverride：测试专用强制开关（true=强制视为无头，走确定性估算路径）。
    // 供自测进程（可能持有 QApplication 但仍需回归"无头降级"路径）钉住判定；
    // 主程序从不调用，行为零变化。
    static bool isGuiAvailable();
    static void setHeadlessOverride(bool on) { g_headlessOverride = on; }
private:
    inline static bool g_headlessOverride = false;   // 覆盖开关（inline：头文件内联 setter 直写）
public:

    // ---- 单行文字宽度(mm)。无 GUI 或字体缺失 → 确定性估算（写日志）。----
    static double lineWidthMm(const ResolvedTextStyle& rs, const QString& s, double heightMm);

    // ---- 多行布局(mm)：先按显式 \n 分段，再按 mtextWidthMm(>0) 真实折行
    //      （含 CJK 任意断点 + 拉丁按空格/连字符断词）；lineSpacing 作用于行高。----
    static TextLayout layout(const ResolvedTextStyle& rs, const QString& text,
                            double heightMm, double mtextWidthMm = 0.0,
                            double lineSpacing = 1.0);

    // ---- Text/MText 图元的世界包围盒(mm)，含 9 点对齐偏移。
    //      屏幕渲染、打印渲染、modelBounds 取景 三处同源调用此函数。----
    static QRectF elementBoundsMm(const Element& e, const SketchModel* model);

    // ---- 表格列宽决策：显式 colWidths（非空且全正）优先；否则按真实度量自适应。
    //      屏幕/打印/取景（ElementPath）均调用此函数，保证列宽三处一致。----
    static QVector<double> tableColumnWidths(const Element& e, const SketchModel* model,
                                             double padMm = 1.0);

    // ---- 表格列宽自适应（真实度量）：每列建议宽 = 该列最长单元格内容宽 + 2*padMm。----
    static QVector<double> autoFitTableColumns(const Element& e, const SketchModel* model,
                                               double padMm = 1.0);

    // ---- 单元格文字按 maxWidthMm 省略溢出（超出则尾部加 …）。----
    static QString elideToWidthMm(const ResolvedTextStyle& rs, const QString& s,
                                  double heightMm, double maxWidthMm);

    // ---- 堆叠分数（MText {\S 上/下;} 或 ^I/^J 上下堆叠）本轮不支持：
    //      检测后给出明确提示（绝不静默退化）。----
    static bool containsStackedFraction(const QString& mtext);

    // ---- 不支持特性提示回调（默认 qWarning；测试可替换以捕获）。----
    using WarnFn = void (*)(const QString&);
    static void setWarnHandler(WarnFn fn);

    // ---- 清空 LRU 缓存（字体变更/测试隔离后调用）。----
    static void clearCache();

private:
    // 参考像素尺寸(1000)下的字形 advance 缓存（LRU）
    static double charAdvancePx1000(const FontSig& sig, QChar ch);
    static double estimateLineWidthMm(const ResolvedTextStyle& rs, const QString& s, double heightMm);
    static void   warn(const QString& msg);
    static bool   familyOk(const QString& family);
};

} // namespace SketchCAD
