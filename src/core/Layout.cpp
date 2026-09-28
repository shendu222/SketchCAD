#include "Layout.h"

namespace SketchCAD {

QJsonObject PlotViewport::toJson() const {
    QJsonObject o;
    o["name"]       = name;
    o["x"]          = modelRect.x();
    o["y"]          = modelRect.y();
    o["w"]          = modelRect.width();
    o["h"]          = modelRect.height();
    o["scale"]      = scale;
    o["rotationDeg"] = rotationDeg;
    return o;
}

PlotViewport PlotViewport::fromJson(const QJsonObject& o) {
    PlotViewport vp;
    vp.name        = o.value("name").toString();
    const double x = o.value("x").toDouble(0.0);
    const double y = o.value("y").toDouble(0.0);
    const double w = o.value("w").toDouble(0.0);
    const double h = o.value("h").toDouble(0.0);
    vp.modelRect   = (w <= 0 || h <= 0) ? QRectF() : QRectF(x, y, w, h);
    vp.scale       = o.value("scale").toDouble(1.0);
    vp.rotationDeg = o.value("rotationDeg").toDouble(0.0);
    return vp;
}

} // namespace SketchCAD
