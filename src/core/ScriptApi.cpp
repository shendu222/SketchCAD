#include "ScriptApi.h"
#include "SketchModel.h"
#include "CommandInterpreter.h"
#include "ElementFactory.h"
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

ScriptApi::ScriptApi(SketchModel* model, CommandInterpreter* interp, QObject* parent)
    : QObject(parent), model_(model), interp_(interp) {}

int ScriptApi::addLine(double x1, double y1, double x2, double y2) {
    if (!model_) return -1;
    Element e = ElementFactory::createLine(Vec3(x1, y1, 0), Vec3(x2, y2, 0), model_);
    return model_->addElement(e);
}

int ScriptApi::addCircle(double cx, double cy, double r) {
    if (!model_ || r <= 0) return -1;
    Element e = ElementFactory::createCircle(Vec3(cx, cy, 0), r, model_);
    return model_->addElement(e);
}

int ScriptApi::addArc(double cx, double cy, double r, double startDeg, double endDeg) {
    if (!model_ || r <= 0) return -1;
    Element e = ElementFactory::createArc(Vec3(cx, cy, 0), r, startDeg, endDeg, model_);
    return model_->addElement(e);
}

int ScriptApi::addRect(double x, double y, double w, double h) {
    if (!model_) return -1;
    Element e = ElementFactory::createRect(Vec3(x, y, 0), Vec3(x + w, y + h, 0), model_);
    return model_->addElement(e);
}

int ScriptApi::addPoint(double x, double y) {
    if (!model_) return -1;
    Element e;
    e.type = ElementType::Point;
    e.pts  = { Vec3(x, y, 0) };
    return model_->addElement(e);
}

int ScriptApi::addText(double x, double y, const QString& text, double heightMm) {
    if (!model_) return -1;
    Element e = ElementFactory::createText(Vec3(x, y, 0), text, heightMm);
    return model_->addElement(e);
}

int ScriptApi::elementCount() const {
    return model_ ? model_->elementCount() : 0;
}

QString ScriptApi::elementInfo(int id) const {
    if (!model_) return QString();
    const Element* e = model_->element(id);
    if (!e) return QStringLiteral("无此图元 #%1").arg(id);
    return QStringLiteral("类型=%1 图层=%2 点数=%3")
        .arg(QString::fromUtf8(elementTypeName(e->type)))
        .arg(e->layer)
        .arg((int)e->pts.size());
}

bool ScriptApi::removeElement(int id) {
    return model_ ? model_->removeElement(id) : false;
}

bool ScriptApi::run(const QString& command) {
    if (!interp_) return false;
    return interp_->execute(command);
}

void ScriptApi::log(const QString& msg) {
    if (model_) model_->log(msg, QStringLiteral("script"));
    emit scriptOutput(msg);
}

} // namespace SketchCAD
