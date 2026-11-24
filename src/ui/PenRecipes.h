#pragma once

#include <QColor>
#include <QPen>

namespace SketchCAD {
namespace ui {

// 派生画笔配方集中化：与 ElementItem::penFor()（图元主体画笔）互补。
// 这些画笔不随图层线型变化，只承担尺寸要素与追踪引导线，故单独成族。
class PenRecipes {
public:
    // 尺寸线/引线/公差框等细实线：约为主体线宽的一半，且强制实线与 cosmetic。
    static QPen thinOutline(const QPen& source);

    // 对象追踪/极轴/正交引导线：固定 1 像素、按缩放比换算宽度，虚线 cosmetic。
    static QPen guide(const QColor& color, double zoom, Qt::PenStyle style = Qt::DashLine);
};

} // namespace ui
} // namespace SketchCAD
