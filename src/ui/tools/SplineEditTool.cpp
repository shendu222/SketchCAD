#include "ui/tools/SplineEditTool.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"
#include "core/Geometry.h"
#include "core/SplineModes.h"
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

SplineEditTool::SplineEditTool(QString id) : id_(std::move(id)) {}

QString SplineEditTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    if (id_ == "spline_from_polyline")
        return QStringLiteral("样条来自折线 — 请选择要转换的折线/多边形/矩形:");
    if (id_ == "spline_append_pt")
        return QStringLiteral("追加样条点 — 选择样条并点击要追加的点:");
    if (id_ == "spline_insert_pts")
        return QStringLiteral("插入样条点 — 选择样条并点击段上位置:");
    if (id_ == "spline_remove_pts")
        return QStringLiteral("删除样条点 — 选择样条并点击要删除的控制点:");
    if (id_ == "spline_remove_between") {
        if (firstId_ < 0)
            return QStringLiteral("删除两点之间 — 点击第一个控制点:");
        return QStringLiteral("删除两点之间 — 点击第二个控制点:");
    }
    if (id_ == "spline_explode")
        return QStringLiteral("分解样条为线段 — 选择要炸开为线段的样条:");
    return id_ + QStringLiteral(" — 请操作:");
}

bool SplineEditTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)w;
    const QPoint vp = e->position().toPoint();

    if (id_ == "spline_from_polyline") {
        const Element* src = pickPolylineLike(ctx, vp);
        if (!src) {
            ctx.model->log(QStringLiteral("请选择折线、多边形或矩形"), "warn");
            return true;
        }
        fromPolyline(ctx, *src);
        return true;
    }

    const Element* eptr = pickSpline(ctx, vp);
    if (!eptr) {
        ctx.model->log(QStringLiteral("请选择样条曲线"), "warn");
        return true;
    }

    if (id_ == "spline_append_pt") {
        appendPoint(ctx, *eptr, ctx.snapWorld(w));
    } else if (id_ == "spline_insert_pts") {
        insertPoint(ctx, *eptr, ctx.snapWorld(w));
    } else if (id_ == "spline_remove_pts") {
        removePoint(ctx, *eptr, ctx.snapWorld(w));
    } else if (id_ == "spline_remove_between") {
        removeBetween(ctx, *eptr, ctx.snapWorld(w));
    } else if (id_ == "spline_explode") {
        explode(ctx, *eptr);
    }
    return true;
}

void SplineEditTool::onDeactivate(ToolContext& ctx) {
    firstId_ = -1;
    firstIdx_ = -1;
    ctx.clearPreview();
}

const Element* SplineEditTool::pickSpline(ToolContext& ctx, const QPoint& vp) {
    const int id = ctx.hitTest(vp);
    if (id < 0) return nullptr;
    const Element* e = ctx.model->element(id);
    return (e && e->type == ElementType::Spline) ? e : nullptr;
}

const Element* SplineEditTool::pickPolylineLike(ToolContext& ctx, const QPoint& vp) {
    const int id = ctx.hitTest(vp);
    if (id < 0) return nullptr;
    const Element* e = ctx.model->element(id);
    if (!e) return nullptr;
    if (e->type == ElementType::Line || e->type == ElementType::Polygon ||
        e->type == ElementType::Rectangle)
        return e;
    return nullptr;
}

int SplineEditTool::nearestControlPoint(const Element& e, const Vec3& p) {
    if (e.pts.empty()) return -1;
    int best = 0;
    double bestD = p.distTo(e.pts[0]);
    for (int i = 1; i < (int)e.pts.size(); ++i) {
        const double d = p.distTo(e.pts[i]);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void SplineEditTool::fromPolyline(ToolContext& ctx, const Element& src) {
    // 取点逻辑下沉到 spline::polylineControlPoints（UI / CLI / 自测共用）
    const std::vector<Vec3> pts = spline::polylineControlPoints(src);
    if (pts.size() < 2) {
        ctx.model->log(QStringLiteral("源图元顶点数不足，无法转样条"), "warn");
        return;
    }

    Element spline = ElementFactory::create(ElementType::Spline, QStringLiteral("样条曲线"), ctx.model);
    spline.pts = pts;
    spline.closed = false;
    spline.splineSubType = static_cast<int>(spline::SubType::CatmullRom);

    const auto ids = ctx.model->replaceElement(src.id, {spline}, QStringLiteral("折线转样条"));
    if (!ids.empty()) {
        ctx.model->setSelected(ids.front());
        ctx.model->log(QString("已将 #%1 转换为样条 #%2（%3 个控制点）")
                           .arg(src.id).arg(ids.front()).arg((int)pts.size()));
    }
}

void SplineEditTool::appendPoint(ToolContext& ctx, const Element& e, const Vec3& p) {
    if (e.pts.empty()) return;
    const bool atFront = (p.distTo(e.pts.front()) < p.distTo(e.pts.back()));
    ctx.model->updateElement(e.id, [&](Element& out) {
        if (atFront) out.pts.insert(out.pts.begin(), p);
        else         out.pts.push_back(p);
    });
    ctx.model->log(QString("已%1样条 #%2 的控制点").arg(atFront ? QStringLiteral("前端追加") : QStringLiteral("后端追加")).arg(e.id));
}

void SplineEditTool::insertPoint(ToolContext& ctx, const Element& e, const Vec3& p) {
    if (e.pts.size() < 2) return;
    int bestSeg = -1;
    double bestD = 1e100;
    Vec3 bestProj;
    for (int i = 0; i + 1 < (int)e.pts.size(); ++i) {
        const Vec3 proj = math::projPointLine(p, e.pts[i], e.pts[i + 1]);
        const double d = p.distTo(proj);
        if (d < bestD) { bestD = d; bestSeg = i; bestProj = proj; }
    }
    if (bestSeg < 0) return;

    ctx.model->updateElement(e.id, [&](Element& out) {
        out.pts.insert(out.pts.begin() + bestSeg + 1, bestProj);
    });
    ctx.model->log(QString("已在样条 #%1 第 %2 段插入控制点").arg(e.id).arg(bestSeg + 1));
}

void SplineEditTool::removePoint(ToolContext& ctx, const Element& e, const Vec3& p) {
    if (e.pts.size() <= 2) {
        ctx.model->removeElement(e.id);
        ctx.model->log(QString("样条 #%1 控制点过少，已删除").arg(e.id));
        return;
    }
    const int idx = nearestControlPoint(e, p);
    if (idx < 0) return;
    ctx.model->updateElement(e.id, [&](Element& out) {
        out.pts.erase(out.pts.begin() + idx);
    });
    ctx.model->log(QString("已删除样条 #%1 的控制点 %2").arg(e.id).arg(idx));
}

void SplineEditTool::removeBetween(ToolContext& ctx, const Element& e, const Vec3& p) {
    const int idx = nearestControlPoint(e, p);
    if (idx < 0) return;

    if (firstId_ < 0) {
        firstId_ = e.id;
        firstIdx_ = idx;
        ctx.model->log(QString("已选择样条 #%1 控制点 %2").arg(e.id).arg(idx));
        return;
    }

    if (firstId_ != e.id) {
        ctx.model->log(QStringLiteral("请选择同一根样条上的两个控制点"), "warn");
        firstId_ = -1; firstIdx_ = -1;
        return;
    }

    int a = firstIdx_, b = idx;
    if (a > b) std::swap(a, b);
    if (b - a <= 1) {
        ctx.model->log(QStringLiteral("两个控制点相邻，无需删除中间段"), "warn");
        firstId_ = -1; firstIdx_ = -1;
        return;
    }

    ctx.model->updateElement(e.id, [&](Element& out) {
        std::vector<Vec3> nv;
        nv.reserve(out.pts.size());
        for (int i = 0; i <= a; ++i) nv.push_back(out.pts[i]);
        for (int i = b; i < (int)out.pts.size(); ++i) nv.push_back(out.pts[i]);
        out.pts = std::move(nv);
    });
    ctx.model->log(QString("已删除样条 #%1 控制点 %2 到 %3 之间的部分")
                       .arg(e.id).arg(a).arg(b));
    firstId_ = -1;
    firstIdx_ = -1;
}

void SplineEditTool::explode(ToolContext& ctx, const Element& e) {
    // Explode spline to lines：炸开为多条独立线段（不再合并为折线）。
    // 插值类样条按节点弦分段；抛物线/双曲线等圆锥类按曲线采样离散，形状不丢失。
    const auto segs = spline::explodeToLines(e.pts, e.splineSubType);
    if (segs.empty()) {
        ctx.model->log(QString("样条 #%1 无法分解为线段（点数不足）").arg(e.id), "warn");
        return;
    }

    std::vector<Element> lines;
    lines.reserve(segs.size());
    for (const auto& s : segs) {
        Element ln = ElementFactory::create(ElementType::Line, QStringLiteral("线段"), ctx.model);
        ln.pts = {s.first, s.second};
        ln.closed = false;
        lines.push_back(std::move(ln));
    }

    const auto ids = ctx.model->replaceElement(e.id, lines, QStringLiteral("样条分解为线段"));
    if (!ids.empty()) {
        ctx.model->log(QString("已将样条 #%1 分解为 %2 条线段（#%3 ~ #%4）")
                           .arg(e.id).arg((int)ids.size())
                           .arg(ids.front()).arg(ids.back()));
    }
}

} // namespace SketchCAD
