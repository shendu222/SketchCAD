#include "ui/tools/PolylineTool.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"

namespace SketchCAD {

QString PolylineTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    if (pts_.empty())
        return QStringLiteral("折线 — 点击第一个节点:");
    if (pts_.size() == 1)
        return QStringLiteral("折线 — 点击下一个节点（点击首点附近可闭合，右键结束）:");
    return QStringLiteral("折线 — 点击下一个节点（点击首点附近可闭合，右键结束）");
}

bool PolylineTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return false;
    const Vec3 p = ctx.snapWorld(w);
    // 自动闭合：距首点 < 3mm
    if (pts_.size() >= 3) {
        if (p.distTo(pts_.front()) < kCloseRadius) {
            finish(ctx, /*closed=*/true);
            return true;
        }
    }
    pts_.push_back(p);
    drawing_ = true;
    ctx.setDrawing(true);
    preview(ctx, p);
    return true;
}

bool PolylineTool::onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    if (!drawing_ || pts_.empty()) return false;
    preview(ctx, w);
    return true;
}

bool PolylineTool::onRightButton(ToolContext& ctx) {
    if (pts_.size() < 2) {
        ctx.clearPreview();
        pts_.clear();
        drawing_ = false;
        ctx.setDrawing(false);
        return true;
    }
    finish(ctx, /*closed=*/false);
    return true;
}

void PolylineTool::onDeactivate(ToolContext& ctx) {
    pts_.clear();
    drawing_ = false;
    ctx.clearPreview();
    ctx.setDrawing(false);
}

void PolylineTool::preview(ToolContext& ctx, const Vec3& cur) {
    if (pts_.empty()) return;
    std::vector<Vec3> previewPts = pts_;
    previewPts.push_back(cur);    // 临时拉一条到鼠标当前位置的引导段
    std::vector<bool> previewFlags(previewPts.size() - 1, false);
    Element tmp = ElementFactory::createPolyline(previewPts, previewFlags,
                                                /*closed=*/false, ctx.model);
    ctx.showPreview(tmp);
}

void PolylineTool::finish(ToolContext& ctx, bool closed) {
    if (pts_.size() < 2) { pts_.clear(); drawing_ = false; ctx.setDrawing(false); return; }
    std::vector<bool> flags(pts_.size() - (closed ? 0 : 1), false);
    Element el = ElementFactory::createPolyline(pts_, flags, closed, ctx.model);
    const int id = ctx.model->addElement(el);
    ctx.model->setSelected(id);
    ctx.model->log(QString("已创建折线 #%1（节点 %2，%3）")
                       .arg(id).arg((int)pts_.size())
                       .arg(closed ? QStringLiteral("闭合") : QStringLiteral("开放")));
    pts_.clear();
    drawing_ = false;
    ctx.setDrawing(false);
    ctx.clearPreview();
}

} // namespace SketchCAD
