// ============================================================
// ToolContext 实现 —— 桥接 ITool 与 GraphicsView 私有成员
// ============================================================
// 通过 friend 关系访问 GraphicsView 内部状态，
// 使工具类无需直接持有视图指针即可读写状态、调用方法。
#include "ui/tools/ITool.h"
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"

namespace SketchCAD {

// ---- 状态查询 ----
int ToolContext::pendingPtCount() const {
    return (int)view->pendingPts_.size();
}
int ToolContext::pickedIdCount() const {
    return (int)view->pickedIds_.size();
}
bool ToolContext::isChainHinted() const {
    return view->chainHinted_;
}
bool ToolContext::isDimPending() const {
    return view->dimPending_;
}
int ToolContext::dimTargetId() const {
    return view->dimTargetId_;
}
int ToolContext::dimEdgeIdx() const {
    return view->dimEdgeIdx_;
}
DimensionType ToolContext::dimPreviewType() const {
    return view->dimPreviewType_;
}
QString ToolContext::activeTool() const {
    return model->activeTool();
}

// ---- 状态操作 ----
void ToolContext::addPendingPt(const Vec3& p) {
    view->pendingPts_.push_back(p);
}
void ToolContext::setDrawing(bool d) {
    view->drawing_ = d;
}
void ToolContext::setArcTracking(bool on, double lastAng) {
    view->arcTracking_ = on;
    view->arcLastAng_ = lastAng;
    view->arcSweep_ = 0.0;
}
void ToolContext::beginArcSweep() {
    if (view->pendingPts_.size() >= 2) {
        view->arcTracking_ = true;
        view->arcLastAng_ = (view->pendingPts_[1] - view->pendingPts_[0]).angleDeg();
        view->arcSweep_ = 0.0;
    }
}
void ToolContext::clearPreview() {
    view->clearPreview();
}
void ToolContext::resetPending() {
    view->resetPending();
}
void ToolContext::updatePreview(const Vec3& cur) {
    view->updatePreview(cur);
}

// ---- 坐标与拾取 ----
Vec3 ToolContext::snapWorld(const Vec3& w) {
    return view->snapWorld(w);
}
int ToolContext::hitTest(const QPoint& vp) {
    return view->hitTest(vp);
}

// ---- 预览 ----
void ToolContext::showPreview(const Element& tmp, std::vector<Element> guides) {
    view->showPreview(tmp, std::move(guides));
}

// ---- 绘制终结 ----
void ToolContext::finalizeLine()     { view->finalizeLine(); }
void ToolContext::finalizeCircle()   { view->finalizeCircle(); }
void ToolContext::finalizeCircle3P() { view->finalizeCircle3P(); }
void ToolContext::finalizeArc()      { view->finalizeArc(); }
void ToolContext::finalizeRect()     { view->finalizeRect(); }
void ToolContext::finalizePoly(bool spline) { view->finalizePoly(spline); }
void ToolContext::finalizeSnake()    { view->finalizeSnake(); }

// ---- 子模式分派 ----
void ToolContext::handleLineSubTool(const Vec3& w, QMouseEvent* e)   { view->handleLineSubTool(w, e); }
void ToolContext::handleXlineSubTool(const Vec3& w, QMouseEvent* e)  { view->handleXlineSubTool(w, e); }
void ToolContext::handleCircleSubTool(const Vec3& w, QMouseEvent* e) { view->handleCircleSubTool(w, e); }
void ToolContext::handleArcSubTool(const Vec3& w, QMouseEvent* e)    { view->handleArcSubTool(w, e); }
void ToolContext::handlePolySubTool(const Vec3& w, QMouseEvent* e)   { view->handlePolySubTool(w, e); }
void ToolContext::handleSplineSubTool(const Vec3& w, QMouseEvent* e) { view->handleSplineSubTool(w, e); }
void ToolContext::handleEllipseSubTool(const Vec3& w, QMouseEvent* e) { view->handleEllipseSubTool(w, e); }
void ToolContext::handleDimension(const Vec3& w, DimensionType dt)   { view->handleDimension(w, dt); }
void ToolContext::handleConstraint(int hitId)                         { view->handleConstraint(hitId); }
void ToolContext::handleTrimAny(int hitId, const Vec3& w, bool shift){ view->handleTrimAny(hitId, w, shift); }
void ToolContext::handleTrimExtend(int hitId, const Vec3& w, bool extend) { view->handleTrimExtend(hitId, w, extend); }

// ---- 选择/拖拽 ----
bool ToolContext::beginHandleDrag(const Vec3& w) {
    return view->beginHandleDrag(w);
}
void ToolContext::setDragMovePending(bool on, const Vec3& w) {
    view->dragMovePending_ = on;
    view->dragMoveLast_ = w;
}
void ToolContext::setMarquee(const QPointF& start) {
    view->marqueeActive_ = true;
    view->marqueeStart_ = start;
    view->marquee_ = QRectF(start, start);
}
void ToolContext::setMarqueeMode(bool deselect) {
    view->deselectMarquee_ = deselect;
}

} // namespace SketchCAD
