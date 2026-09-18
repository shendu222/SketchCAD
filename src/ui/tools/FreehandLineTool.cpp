#include "ui/tools/FreehandLineTool.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"
#include "core/SplineModes.h"

namespace SketchCAD {

QString FreehandLineTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return drawing_
        ? QStringLiteral("自由手绘 — 按住左键移动绘制，松开结束")
        : QStringLiteral("自由手绘 — 按住左键开始绘制");
}

bool FreehandLineTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return false;
    drawing_ = true;
    pts_.clear();
    pts_.push_back(ctx.snapWorld(w));
    preview(ctx);
    ctx.setDrawing(true);
    return true;
}

bool FreehandLineTool::onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    if (!drawing_) return false;
    if (!(e->buttons() & Qt::LeftButton)) return false;
    const Vec3 p = ctx.snapWorld(w);
    // 最小采样间隔：世界坐标 1mm（避免点过于密集）
    if (!pts_.empty() && p.distTo(pts_.back()) < 1.0) return true;
    pts_.push_back(p);
    preview(ctx);
    return true;
}

bool FreehandLineTool::onRelease(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)w;
    if (e->button() != Qt::LeftButton || !drawing_) return false;
    finish(ctx);
    return true;
}

void FreehandLineTool::onDeactivate(ToolContext& ctx) {
    if (drawing_) {
        drawing_ = false;
        pts_.clear();
        ctx.clearPreview();
    }
}

void FreehandLineTool::preview(ToolContext& ctx) {
    if (pts_.size() < 2) return;
    Element tmp = ElementFactory::create(ElementType::Spline, QStringLiteral("自由手绘线"), ctx.model);
    tmp.pts = pts_;
    tmp.splineSubType = static_cast<int>(spline::SubType::Freehand);
    tmp.closed = false;
    ctx.showPreview(tmp);
}

void FreehandLineTool::finish(ToolContext& ctx) {
    drawing_ = false;
    ctx.clearPreview();
    if (pts_.size() < 2) {
        pts_.clear();
        return;
    }
    Element el = ElementFactory::create(ElementType::Spline, QStringLiteral("自由手绘线"), ctx.model);
    el.pts = std::move(pts_);
    el.splineSubType = static_cast<int>(spline::SubType::Freehand);
    el.closed = false;
    const int id = ctx.model->addElement(el);
    ctx.model->setSelected(id);
    ctx.model->log(QString("已创建自由手绘线 #%1（%2 个采样点）").arg(id).arg((int)el.pts.size()));
}

} // namespace SketchCAD
