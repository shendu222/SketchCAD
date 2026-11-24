#include "PenRecipes.h"

#include <algorithm>

namespace SketchCAD {
namespace ui {

QPen PenRecipes::thinOutline(const QPen& source) {
    QPen thin = source;
    thin.setCosmetic(true);
    thin.setStyle(Qt::SolidLine);
    thin.setWidthF(std::max(1.0, source.widthF() * 0.5));
    return thin;
}

QPen PenRecipes::guide(const QColor& color, double zoom, Qt::PenStyle style) {
    QPen pen(color);
    pen.setStyle(style);
    pen.setWidthF(1.0 / std::max(zoom, 0.05));
    pen.setCosmetic(true);
    return pen;
}

} // namespace ui
} // namespace SketchCAD
