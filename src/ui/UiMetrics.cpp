#include "UiMetrics.h"

#include <QGuiApplication>
#include <QScreen>

namespace SketchCAD {
namespace ui {

UiMetrics::Density UiMetrics::densityFor(int availableWidth) {
    // 1440 是两行工具条 + 参数条 + 左右停靠面板仍能完整展开的经验阈值。
    // 低于该宽度切换到紧凑密度：缩小图标/内边距/控件宽度，避免 ">>" 溢出与横向滚动。
    return availableWidth < 1440 ? Density::Compact : Density::Comfortable;
}

UiMetrics::Density UiMetrics::screenDensity() {
    QScreen* screen = QGuiApplication::primaryScreen();
    const int width = screen ? screen->availableGeometry().width() : 1920;
    return densityFor(width);
}

QSize UiMetrics::toolBarIconSize(Density d) {
    return d == Density::Compact ? QSize(18, 18) : QSize(20, 20);
}

int UiMetrics::toolBarMarginH(Density d) { return d == Density::Compact ? 3 : 6; }
int UiMetrics::toolBarMarginV(Density d) { return d == Density::Compact ? 1 : 2; }
int UiMetrics::toolBarSpacing(Density d) { return d == Density::Compact ? 1 : 2; }
int UiMetrics::toolButtonMinWidth(Density d) { return d == Density::Compact ? 56 : 64; }

int UiMetrics::paramMarginH(Density d) { return d == Density::Compact ? 4 : 8; }
int UiMetrics::paramMarginV(Density d) { return d == Density::Compact ? 2 : 4; }
int UiMetrics::paramSpacing(Density d) { return d == Density::Compact ? 5 : 8; }
int UiMetrics::angleSpinWidth(Density d) { return d == Density::Compact ? 72 : 80; }
int UiMetrics::valueSpinWidth(Density d) { return d == Density::Compact ? 78 : 90; }
int UiMetrics::styleComboMinWidth(Density d) { return d == Density::Compact ? 104 : 120; }

int UiMetrics::leftDockWidth(Density d) { return d == Density::Compact ? 200 : 240; }
int UiMetrics::rightDockWidth(Density d) { return d == Density::Compact ? 260 : 300; }

} // namespace ui
} // namespace SketchCAD
