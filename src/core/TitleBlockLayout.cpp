#include "TitleBlockLayout.h"
#include "Element.h"
#include "SketchModel.h"
#include "TextMetrics.h"
#include "TextStyle.h"
#include <QString>
#include <cmath>
#include <limits>

namespace SketchCAD {

// 收缩下限：单元格再窄也不截断（elide 省略号会破坏「完整显示」）
static constexpr double kFitMinHeightMm = 1.5;
// 网格重建的共线容差(mm)
static constexpr double kGridEps = 1e-6;

// 收缩 + 矩形内居中的公共实现：fitCellText / fitCellLabelText / fitCellFieldValue
// 三入口共用（单一数据源：度量 TextMetrics::lineWidthMm，收缩下限 kFitMinHeightMm）。
static CellTextFit fitInZone(const QString& text, double wantHeight,
                             double left, double right, double bottom, double top,
                             double inset) {
    CellTextFit f;
    double h = (wantHeight > 0.1) ? wantHeight : 3.5;
    const double avail = std::max(0.5, right - left - 2.0 * inset);
    if (!text.isEmpty()) {
        // 默认构造的 ResolvedTextStyle 即「无样式名图元」的渲染解析结果
        //（height=字高、空 fontFamily、wf=1、非粗斜体），与 resolveTextStyle 一致
        const ResolvedTextStyle rs;
        const double w = TextMetrics::lineWidthMm(rs, text, h);
        if (w > avail && w > 1e-9)
            h = std::max(kFitMinHeightMm, h * avail / w);
    }
    f.shrunk = (h < wantHeight - 1e-9);
    f.height = h;
    // 锚点 = 指定子区中心。⚠️ plain Text 的渲染锚点语义（GraphicsView::paintText 与
    // PlotRenderer 同源实测）：textAlign=4（中心）时锚点即文字块的视觉中心。
    // 旧实现按「块左下角」假设返回 left+inset，而默认 align=1 实际把锚点当
    // 水平中心渲染 → 全部标题栏文字向左越出竖边线（用户可见 bug）。改为
    // 居中放置后，创建端必须同步设 textAlign=4（sheet_frame.cpp 各 lambda）。
    f.x = (left + right) / 2.0;
    f.y = (bottom + top) / 2.0;
    return f;
}

// 标签带/值区的统一 strip 推导：两入口必须用同一公式，才能严格不重叠。
// strip ≤ 半格：高度不足 8mm 的退化单元格自适应减半，两区绝不产生负高度。
static inline double labelStripOf(double bottom, double top) {
    return std::min(kTitleBlockLabelStripMm,
                    std::max(0.5, (top - bottom) * 0.5));
}

CellTextFit fitCellText(const QString& text, double wantHeight,
                        double left, double right, double bottom, double top,
                        bool stickTop, double inset) {
    Q_UNUSED(stickTop);   // 历史参数：旧「贴顶/贴底」语义已收敛为居中，保留仅为 ABI/源兼容
    return fitInZone(text, wantHeight, left, right, bottom, top, inset);
}

CellTextFit fitCellLabelText(const QString& text, double wantHeight,
                             double left, double right, double bottom, double top,
                             double inset) {
    const double strip = labelStripOf(bottom, top);
    return fitInZone(text, wantHeight, left, right, top - strip, top, inset);
}

CellTextFit fitCellFieldValue(const QString& text, double wantHeight,
                              double left, double right, double bottom, double top,
                              double inset) {
    const double strip = labelStripOf(bottom, top);
    return fitInZone(text, wantHeight, left, right, bottom, top - strip, inset);
}

QRectF titleBlockCellAt(const SketchModel* model, double x, double y) {
    if (!model) return QRectF();
    const QString tbLayer = QStringLiteral("标题栏");
    double L = -std::numeric_limits<double>::infinity();
    double R =  std::numeric_limits<double>::infinity();
    double B = -std::numeric_limits<double>::infinity();
    double T =  std::numeric_limits<double>::infinity();
    for (const auto& e : model->elements()) {
        if (e.layer != tbLayer) continue;
        if (e.type == ElementType::Line && e.pts.size() >= 2) {
            const double x1 = e.pts[0].x, y1 = e.pts[0].y;
            const double x2 = e.pts[1].x, y2 = e.pts[1].y;
            if (std::fabs(x1 - x2) < kGridEps) {           // 竖线：须横跨该点 Y
                const double ya = std::min(y1, y2), yb = std::max(y1, y2);
                if (y >= ya - kGridEps && y <= yb + kGridEps) {
                    if (x1 <= x + kGridEps && x1 > L) L = x1;
                    if (x1 >= x - kGridEps && x1 < R) R = x1;
                }
            } else if (std::fabs(y1 - y2) < kGridEps) {    // 横线：须横跨该点 X
                const double xa = std::min(x1, x2), xb = std::max(x1, x2);
                if (x >= xa - kGridEps && x <= xb + kGridEps) {
                    if (y1 <= y + kGridEps && y1 > B) B = y1;
                    if (y1 >= y - kGridEps && y1 < T) T = y1;
                }
            }
        } else if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
            const double xa = e.pts[0].x, ya = e.pts[0].y;
            const double xb = e.pts[1].x, yb = e.pts[1].y;
            const double l = std::min(xa, xb), r = std::max(xa, xb);
            const double b = std::min(ya, yb), t = std::max(ya, yb);
            if (y >= b - kGridEps && y <= t + kGridEps) {
                if (l <= x + kGridEps && l > L) L = l;
                if (r >= x - kGridEps && r < R) R = r;
            }
            if (x >= l - kGridEps && x <= r + kGridEps) {
                if (b <= y + kGridEps && b > B) B = b;
                if (t >= y - kGridEps && t < T) T = t;
            }
        }
    }
    if (!std::isfinite(L) || !std::isfinite(R) ||
        !std::isfinite(B) || !std::isfinite(T))
        return QRectF();
    if (R - L < 0.5 || T - B < 0.5) return QRectF();   // 退化单元格 → 放弃
    // 注意 QRectF 语义：top()=最小 Y（本工程惯例「top()=最小 Y」），
    // 与 sheet_frame / buildInsertedTitleBlock 的用法一致。
    return QRectF(QPointF(L, B), QPointF(R, T));
}

bool fitTitleBlockText(Element& e, const QRectF& cell) {
    if (!cell.isValid() || cell.isEmpty()) return false;
    if (e.type != ElementType::Text) return false;
    // 两区分离写回（单一收敛点）：tb:field:* 字段图元 → 「值区」中心（去顶部
    // 标签带），与灰色标签永不重叠（填写值覆盖标签缺陷修复）；其余文本
    // （旧档标签/普通文本）保持整格居中。stickTop 判别仅保留语义兼容。
    // ⚠️ QRectF 语义：top()=最小 Y（世界底）、bottom()=最大 Y（世界顶），
    // 故传参 bottom=cell.top()、top=cell.bottom()（与 buildSheetFrame 一致）。
    const bool isField = e.name.startsWith(QLatin1String("tb:field:"));
    const CellTextFit f = isField
        ? fitCellFieldValue(e.text, e.radius, cell.left(), cell.right(),
                            cell.top(), cell.bottom())
        : fitCellText(e.text, e.radius, cell.left(), cell.right(),
                      cell.top(), cell.bottom(),
                      e.name.startsWith(QLatin1String("tb:label:")));
    if (e.pts.empty()) e.pts.push_back(Vec3(f.x, f.y, 0));
    else { e.pts[0].x = f.x; e.pts[0].y = f.y; }
    const bool changed = std::fabs(e.radius - f.height) > 1e-9;
    e.radius = f.height;
    return changed;
}

} // namespace SketchCAD
