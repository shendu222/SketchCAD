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
// 文字与单元格边的呼吸间距(mm)
static constexpr double kCellBreathMm = 1.0;
// 网格重建的共线容差(mm)
static constexpr double kGridEps = 1e-6;

CellTextFit fitCellText(const QString& text, double wantHeight,
                        double left, double right, double bottom, double top,
                        bool stickTop, double inset) {
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
    f.x = left + inset;
    f.y = stickTop ? (top - kCellBreathMm - h) : (bottom + kCellBreathMm);
    return f;
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
    const bool stickTop = e.name.startsWith(QLatin1String("tb:label:"));
    const CellTextFit f = fitCellText(e.text, e.radius, cell.left(), cell.right(),
                                      cell.top(), cell.bottom(), stickTop);
    if (e.pts.empty()) e.pts.push_back(Vec3(f.x, f.y, 0));
    else { e.pts[0].x = f.x; e.pts[0].y = f.y; }
    const bool changed = std::fabs(e.radius - f.height) > 1e-9;
    e.radius = f.height;
    return changed;
}

} // namespace SketchCAD
