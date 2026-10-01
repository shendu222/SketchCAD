#include "SdkContext.h"
#include "SketchModel.h"
#include "ElementFactory.h"
#include "Element.h"
#include "Vec.h"
#include "CommandInterpreter.h"
#include "SketchCAD/sdk_version.h"   // SKETCHCAD_SDK_ABI_VERSION / VERSION_INT
#include <functional>

namespace SketchCAD {

// ====================== SdkModel（IModel 实现） ======================

int SdkModel::addLine(double x1, double y1, double x2, double y2) {
    if (!m_) return -1;
    Element e = ElementFactory::createLine(Vec3(x1, y1, 0), Vec3(x2, y2, 0), m_);
    return m_->addElement(e);
}

int SdkModel::addCircle(double cx, double cy, double r) {
    if (!m_ || r <= 0) return -1;
    Element e = ElementFactory::createCircle(Vec3(cx, cy, 0), r, m_);
    return m_->addElement(e);
}

int SdkModel::addArc(double cx, double cy, double r, double startDeg, double endDeg) {
    if (!m_ || r <= 0) return -1;
    Element e = ElementFactory::createArc(Vec3(cx, cy, 0), r, startDeg, endDeg, m_);
    return m_->addElement(e);
}

int SdkModel::addRect(double x, double y, double w, double h) {
    if (!m_) return -1;
    Element e = ElementFactory::createRect(Vec3(x, y, 0), Vec3(x + w, y + h, 0), m_);
    return m_->addElement(e);
}

int SdkModel::addPoint(double x, double y) {
    if (!m_) return -1;
    Element e;
    e.type = ElementType::Point;
    e.pts  = { Vec3(x, y, 0) };
    return m_->addElement(e);
}

int SdkModel::addText(double x, double y, const char* text, double heightMm) {
    if (!m_) return -1;
    Element e = ElementFactory::createText(Vec3(x, y, 0),
                                           QString::fromUtf8(text ? text : ""), heightMm);
    return m_->addElement(e);
}

int SdkModel::elementCount() const {
    return m_ ? m_->elementCount() : 0;
}

bool SdkModel::elementExists(int id) const {
    return m_ ? (m_->element(id) != nullptr) : false;
}

bool SdkModel::elementInfo(int id, sdk::SdkElementInfo* out) const {
    if (!out || !m_) return false;
    const Element* e = m_->element(id);
    if (!e) return false;
    out->id = e->id;
    out->typeName = SketchCAD::elementTypeName(e->type);   // 全局函数（字面量，永久有效）；类成员同名遮蔽须限定
    layerBuf_ = e->layer.toUtf8();
    out->layer = layerBuf_.constData();
    out->pointCount = (int)e->pts.size();
    return true;
}

const char* SdkModel::elementTypeName(int id) const {
    const Element* e = m_ ? m_->element(id) : nullptr;
    return e ? SketchCAD::elementTypeName(e->type) : "";
}

const char* SdkModel::elementLayer(int id) const {
    const Element* e = m_ ? m_->element(id) : nullptr;
    if (!e) { layerBuf_.clear(); return ""; }
    layerBuf_ = e->layer.toUtf8();
    return layerBuf_.constData();
}

int SdkModel::elementPointCount(int id) const {
    const Element* e = m_ ? m_->element(id) : nullptr;
    return e ? (int)e->pts.size() : 0;
}

bool SdkModel::elementPoint(int id, int index, sdk::SdkPoint* out) const {
    const Element* e = m_ ? m_->element(id) : nullptr;
    if (!e || !out || index < 0 || index >= (int)e->pts.size()) return false;
    const Vec3& p = e->pts[index];
    out->x = p.x; out->y = p.y; out->z = p.z;
    return true;
}

bool SdkModel::removeElement(int id) {
    return m_ ? m_->removeElement(id) : false;
}

void SdkModel::log(const char* msg) {
    if (m_) m_->log(QString::fromUtf8(msg ? msg : ""), QStringLiteral("plugin"));
}

// ====================== SdkApplication（IApplication 实现） ======================

SdkApplication::SdkApplication(SketchModel* m, CommandInterpreter* interp)
    : m_(m), interp_(interp), modelBridge_(m) {}

void SdkApplication::log(const char* msg) {
    if (m_) m_->log(QString::fromUtf8(msg ? msg : ""), QStringLiteral("plugin"));
}

bool SdkApplication::registerCommand(const char* name, sdk::SdkCommandFn fn) {
    if (!interp_ || !name || !fn) return false;
    QString nm = QString::fromUtf8(name).toLower();
    if (nm.isEmpty()) return false;
    // 把 C 函数指针包成 std::function，传给解释器的外部命令表。
    // args 经 UTF-8 往返（命令行本就是 UTF-8 文本）。
    std::function<bool(const QString&)> wrapper =
        [fn](const QString& args) -> bool {
            return fn(args.toUtf8().constData());
        };
    return interp_->registerExternalCommand(nm, wrapper);
}

const char* SdkApplication::appVersion() const {
    return "SketchCAD 1.0";
}

int SdkApplication::sdkVersion() const {
    return SKETCHCAD_SDK_VERSION_INT;
}

int SdkApplication::sdkAbiVersion() const {
    return SKETCHCAD_SDK_ABI_VERSION;
}

} // namespace SketchCAD
