#include "LineWidthGroup.h"

namespace SketchCAD {

// 表1 图线组别：{粗线 mm, 细线 mm}
static const double kThickMm[] = { 2.0, 1.4, 1.0, 0.7, 0.5 };
static const double kThinMm[]  = { 1.0, 0.7, 0.5, 0.35, 0.25 };

static LineWidthGroup g_current = LineWidthGroup::G3;   // 默认第 3 组

int lineWidthGroupCount() { return 5; }

double groupThickMm(LineWidthGroup g) {
    const int i = (int)g;
    return (i >= 0 && i < 5) ? kThickMm[i] : kThickMm[2];
}

double groupThinMm(LineWidthGroup g) {
    const int i = (int)g;
    return (i >= 0 && i < 5) ? kThinMm[i] : kThinMm[2];
}

QString lineWidthGroupName(LineWidthGroup g) {
    const int i = (int)g;
    if (i < 0 || i >= 5) return QString();
    return QStringLiteral("第%1组 (粗%2/细%3 mm)")
        .arg(i + 1).arg(kThickMm[i], 0, 'g', 3).arg(kThinMm[i], 0, 'g', 3);
}

LineWidthGroup currentLineWidthGroup() { return g_current; }
void setCurrentLineWidthGroup(LineWidthGroup g) {
    const int i = (int)g;
    if (i >= 0 && i < 5) g_current = g;
}

// 屏幕换算系数：默认组 G3 粗线 1.0mm → 3px（与旧版 Solid=3px 观感一致）。
static constexpr double kMmToScreenPx = 3.0;
double mmToScreenPx(double mm) { return mm * kMmToScreenPx; }

} // namespace SketchCAD
