#pragma once

#include <QColor>
#include <QSize>

namespace SketchCAD {
namespace ui {

// 界面密度：所有自适应尺寸的共同输入。
// 阈值集中在本模块，避免菜单、工具条、参数条、停靠面板各自判断屏幕宽度。
class UiMetrics {
public:
    enum class Density { Compact, Comfortable };

    static Density densityFor(int availableWidth);
    static Density screenDensity();

    static QSize toolBarIconSize(Density d);
    static int toolBarMarginH(Density d);
    static int toolBarMarginV(Density d);
    static int toolBarSpacing(Density d);
    static int toolButtonMinWidth(Density d);

    static int paramMarginH(Density d);
    static int paramMarginV(Density d);
    static int paramSpacing(Density d);
    static int angleSpinWidth(Density d);
    static int valueSpinWidth(Density d);
    static int styleComboMinWidth(Density d);

    static int leftDockWidth(Density d);
    static int rightDockWidth(Density d);
    static int leftDockVertical() { return 660; }
    static int rightDockVertical() { return 800; }
};

} // namespace ui
} // namespace SketchCAD
