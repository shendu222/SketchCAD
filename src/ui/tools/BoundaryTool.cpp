#include "ui/tools/BoundaryTool.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"

namespace SketchCAD {

QString BoundaryTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    if (phase_ == 0) {
        if (outer_.empty())
            return QStringLiteral("边界 — 点击外环第一个节点:");
        if (outer_.size() == 1)
            return QStringLiteral("边界 — 点击外环下一个节点（点击首点附近闭合并进入内孔，右键直接结束）:");
        return QStringLiteral("边界 — 继续点击外环节点（点击首点附近闭合并进入内孔，右键直接结束）");
    }
    // 阶段 1：内孔
    if (holePts_.empty())
        return QStringLiteral("边界 — 外环已完成，点击内孔第一个节点（右键结束，无内孔直接生成）:");
    return QStringLiteral("边界 — 继续点击内孔节点（点击该内孔首点附近完成当前内孔，右键结束并生成边界）");
}

bool BoundaryTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return false;
    const Vec3 p = ctx.snapWorld(w);
    if (phase_ == 0) {
        if (outer_.size() >= 3 && p.distTo(outer_.front()) < kCloseRadius) {
            finishOuter(ctx);                 // 自动闭合外环 → 进入内孔阶段
            return true;
        }
        outer_.push_back(p);
        drawing_ = true;
        previewOuter(ctx, p);
        return true;
    }
    // 阶段 1：内孔
    if (holePts_.size() >= 3 && p.distTo(holePts_.front()) < kCloseRadius) {
        finishHole();                         // 完成当前内孔，继续拾取下一个
        return true;
    }
    holePts_.push_back(p);
    drawing_ = true;
    previewHole(ctx, p);
    return true;
}

bool BoundaryTool::onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    if (!drawing_) return false;
    if (phase_ == 0)
        previewOuter(ctx, w);
    else
        previewHole(ctx, w);
    return true;
}

bool BoundaryTool::onRightButton(ToolContext& ctx) {
    if (phase_ == 0) {
        if (outer_.size() < 3) { reset(ctx); return true; }   // 不足 3 点 = 取消
        commit(ctx);                                          // 闭合外环、无内孔
        return true;
    }
    // 阶段 1：右键结束
    if (holePts_.size() >= 3) finishHole();                   // 收尾当前内孔
    if (outer_.size() < 3) { reset(ctx); return true; }
    commit(ctx);
    return true;
}

void BoundaryTool::onDeactivate(ToolContext& ctx) { reset(ctx); }

void BoundaryTool::previewOuter(ToolContext& ctx, const Vec3& cur) {
    if (outer_.empty()) return;
    std::vector<Vec3> previewPts = outer_;
    previewPts.push_back(cur);
    Element tmp = ElementFactory::createBoundary(previewPts, {}, ctx.model);
    ctx.showPreview(tmp);
}

void BoundaryTool::previewHole(ToolContext& ctx, const Vec3& cur) {
    if (holePts_.empty()) return;
    std::vector<Vec3> previewPts = holePts_;
    previewPts.push_back(cur);
    Element tmp = ElementFactory::createBoundary(previewPts, {}, ctx.model);
    ctx.showPreview(tmp);
}

void BoundaryTool::finishOuter(ToolContext& ctx) {
    phase_ = 1;
    holePts_.clear();
    drawing_ = false;
    ctx.clearPreview();
    ctx.setDrawing(false);
    ctx.model->log(QStringLiteral("边界外环已完成（%1 节点），继续拾取内孔，右键结束").arg((int)outer_.size()));
}

void BoundaryTool::finishHole() {
    if (holePts_.size() >= 3) holes_.push_back(holePts_);
    holePts_.clear();
}

void BoundaryTool::commit(ToolContext& ctx) {
    if (outer_.size() < 3) { reset(ctx); return; }
    Element el = ElementFactory::createBoundary(outer_, holes_, ctx.model);
    const int id = ctx.model->addElement(el);
    ctx.model->setSelected(id);
    ctx.model->log(QString("已创建边界 #%1（外环 %2 节点，内孔 %3）")
                       .arg(id).arg((int)outer_.size()).arg((int)holes_.size()));
    reset(ctx);
}

void BoundaryTool::reset(ToolContext& ctx) {
    phase_ = 0;
    outer_.clear();
    holePts_.clear();
    holes_.clear();
    drawing_ = false;
    ctx.clearPreview();
    ctx.setDrawing(false);
}

} // namespace SketchCAD
