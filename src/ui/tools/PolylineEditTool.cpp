#include "ui/tools/PolylineEditTool.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"
#include "core/Geometry.h"
#include "core/PolylineOps.h"
#include <QMouseEvent>
#include <QInputDialog>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// 阈值：节点拾取容差 = 3mm
static constexpr double kNodeHitRadius   = 3.0;
static constexpr double kSegmentHitRadius = 3.0;
static constexpr double kFromMergeRadius  = 5.0;   // from_segments 时相邻段端点合并阈值

PolylineEditTool::PolylineEditTool(QString id) : id_(std::move(id)) {}

QString PolylineEditTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    if (id_ == "polyline_add_node")
        return QStringLiteral("添加节点 — 选择折线并点击段上要插入的位置:");
    if (id_ == "polyline_append_node")
        return QStringLiteral("追加节点 — 选择折线并点击要追加的位置（自动选最近的端点）:");
    if (id_ == "polyline_delete_node")
        return QStringLiteral("删除节点 — 选择折线并点击要删除的节点:");
    if (id_ == "polyline_delete_between")
        return firstId_ < 0
            ? QStringLiteral("删除两点之间 — 点击第一个节点:")
            : QStringLiteral("删除两点之间 — 点击第二个节点（同一折线）:");
    if (id_ == "polyline_trim_segments")
        return QStringLiteral("修剪段 — 选择折线并点击要删除的段:");
    if (id_ == "polyline_equidistant")
        return QStringLiteral("等距折线 — 选择源折线:");
    if (id_ == "polyline_from_segments")
        return fromIds_.empty()
            ? QStringLiteral("从已有段生成 — 点击第一条线段/弧（右键结束）:")
            : QStringLiteral("从已有段生成 — 点击下一条（右键结束）:");
    if (id_ == "polyline_arcs_to_chords")
        return QStringLiteral("弧段转弦 — 选择折线（将所有弧段改为直线段）:");
    if (id_ == "polyline_change_segment_type") {
        if (firstId_ < 0)
            return QStringLiteral("切换段类型 — 点击第一段:");
        return QStringLiteral("切换段类型 — 点击第二段（同一折线，可切换两段）:");
    }
    if (id_ == "polyline_fit")
        return QStringLiteral("拟合曲线 — 选择折线（平滑通过所有顶点）:");
    if (id_ == "polyline_spline")
        return QStringLiteral("样条化 — 选择折线（顶点作为样条控制点）:");
    if (id_ == "polyline_width")
        return QStringLiteral("设置线宽 — 选择折线（将弹出宽度输入）:");
    if (id_ == "polyline_close")
        return QStringLiteral("闭合/打开 — 选择折线（切换闭合状态）:");
    if (id_ == "polyline_reverse")
        return QStringLiteral("反转 — 选择折线:");
    if (id_ == "polyline_join")
        return firstId_ < 0
            ? QStringLiteral("合并 — 选择目标折线（开放）:")
            : QStringLiteral("合并 — 点击相邻的直线/圆弧/折线（右键结束）:");
    return id_ + QStringLiteral(" — 请操作:");
}

bool PolylineEditTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return false;
    const QPoint vp = e->position().toPoint();

    // from_segments：选 line/arc/polyline 段
    if (id_ == "polyline_from_segments") {
        const Element* seg = pickSegmentLike(ctx, vp);
        if (!seg) { ctx.model->log(QStringLiteral("请选择线段、圆弧或折线"), "warn"); return true; }
        fromSegments(ctx, seg->id);
        return true;
    }

    // 合并：第一步选目标折线，之后选相邻来源段（line/arc/polyline）
    if (id_ == "polyline_join") {
        if (firstId_ < 0) {
            const Element* t = pickPolyline(ctx, vp);
            if (!t) { ctx.model->log(QStringLiteral("请选择目标折线"), "warn"); return true; }
            firstId_ = t->id;
            ctx.model->log(QString("已选目标折线 #%1，点击相邻直线/圆弧/折线（右键结束）").arg(firstId_));
            return true;
        }
        const Element* seg = pickSegmentLike(ctx, vp);
        if (!seg) { ctx.model->log(QStringLiteral("请选择直线/圆弧/折线"), "warn"); return true; }
        join(ctx, *seg, w);
        return true;
    }

    // 其它模式：必须先选 polyline
    const Element* eptr = pickPolyline(ctx, vp);
    if (!eptr) {
        ctx.model->log(QStringLiteral("请选择折线"), "warn");
        return true;
    }
    const Vec3 p = ctx.snapWorld(w);

    if      (id_ == "polyline_add_node")          addNode(ctx, *eptr, p);
    else if (id_ == "polyline_append_node")       appendNode(ctx, *eptr, p);
    else if (id_ == "polyline_delete_node")       deleteNode(ctx, *eptr, p);
    else if (id_ == "polyline_delete_between")    deleteBetween(ctx, *eptr, p);
    else if (id_ == "polyline_trim_segments")     trimSegments(ctx, *eptr, p);
    else if (id_ == "polyline_equidistant")       equidistant(ctx, *eptr, p);
    else if (id_ == "polyline_arcs_to_chords")    arcsToChords(ctx, *eptr);
    else if (id_ == "polyline_change_segment_type") changeSegmentType(ctx, *eptr, p);
    else if (id_ == "polyline_fit")               fit(ctx, *eptr);
    else if (id_ == "polyline_spline")            spline(ctx, *eptr);
    else if (id_ == "polyline_width")             setWidth(ctx, *eptr);
    else if (id_ == "polyline_close")             closePolyline(ctx, *eptr);
    else if (id_ == "polyline_reverse")           reverse(ctx, *eptr);
    return true;
}

bool PolylineEditTool::onRightButton(ToolContext& ctx) {
    if (id_ == "polyline_from_segments" && !fromIds_.empty()) {
        // 用已累积的 ids 立刻成图并清空缓冲
        fromIds_.clear();
        firstId_ = -1; firstIdx_ = -1; firstSeg_ = -1;
        ctx.model->log(QStringLiteral("已完成「从段生成折线」"));
        return true;
    }
    if (id_ == "polyline_join" && firstId_ >= 0) {
        // 结束合并，清空目标缓冲
        firstId_ = -1; firstIdx_ = -1; firstSeg_ = -1;
        ctx.model->log(QStringLiteral("已完成「合并」"));
        return true;
    }
    return false;
}

void PolylineEditTool::onDeactivate(ToolContext& ctx) {
    firstId_ = -1; firstIdx_ = -1; firstSeg_ = -1;
    fromIds_.clear();
    ctx.clearPreview();
}

const Element* PolylineEditTool::pickPolyline(ToolContext& ctx, const QPoint& vp) {
    const int id = ctx.hitTest(vp);
    if (id < 0) return nullptr;
    const Element* e = ctx.model->element(id);
    return (e && e->type == ElementType::Polyline) ? e : nullptr;
}

const Element* PolylineEditTool::pickSegmentLike(ToolContext& ctx, const QPoint& vp) {
    const int id = ctx.hitTest(vp);
    if (id < 0) return nullptr;
    const Element* e = ctx.model->element(id);
    if (!e) return nullptr;
    if (e->type == ElementType::Line    ||
        e->type == ElementType::Arc     ||
        e->type == ElementType::Polyline) return e;
    return nullptr;
}

int PolylineEditTool::nearestNode(const Element& e, const Vec3& p) {
    if (e.pts.empty()) return -1;
    int best = 0;
    double bestD = p.distTo(e.pts[0]);
    for (int i = 1; i < (int)e.pts.size(); ++i) {
        const double d = p.distTo(e.pts[i]);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int PolylineEditTool::nearestSeg(const Element& e, const Vec3& p) {
    const int n = (int)e.pts.size();
    if (n < 2) return -1;
    const int nSegs = e.closed ? n : n - 1;
    int best = -1;
    double bestD = 1e100;
    for (int i = 0; i < nSegs; ++i) {
        const Vec3& a = e.pts[i];
        const Vec3& b = e.pts[(i + 1) % n];
        // 弧段上：取 arcThrough 圆心+半径后投影到圆周上再算距离
        const bool isArc = (i < (int)e.arcFlags.size()) && e.arcFlags[i];
        double d;
        if (isArc) {
            geo::ArcSeg ar = geo::arcThrough(e.pts, i, e.closed);
            if (ar.valid) {
                const double ang = std::atan2(p.y - ar.center.y, p.x - ar.center.x);
                const Vec3 q(ar.center.x + ar.radius * std::cos(ang),
                             ar.center.y + ar.radius * std::sin(ang), 0);
                d = p.distTo(q);
            } else {
                d = geo::distPointSegment(p, a, b);
            }
        } else {
            d = geo::distPointSegment(p, a, b);
        }
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ============================ 模式实现 ============================

void PolylineEditTool::addNode(ToolContext& ctx, const Element& e, const Vec3& p) {
    const int n = (int)e.pts.size();
    if (n < 2) return;
    const int nSegs = e.closed ? n : n - 1;
    int bestSeg = -1;
    double bestD = 1e100;
    Vec3 bestProj;
    for (int i = 0; i < nSegs; ++i) {
        const Vec3& a = e.pts[i];
        const Vec3& b = e.pts[(i + 1) % n];
        const bool isArc = (i < (int)e.arcFlags.size()) && e.arcFlags[i];
        double d; Vec3 proj;
        if (isArc) {
            geo::ArcSeg ar = geo::arcThrough(e.pts, i, e.closed);
            if (ar.valid) {
                // 投影到弧中点：取 p 在圆周上的最近点
                const double ang = std::atan2(p.y - ar.center.y, p.x - ar.center.x);
                proj = Vec3(ar.center.x + ar.radius * std::cos(ang),
                            ar.center.y + ar.radius * std::sin(ang), 0);
                d = p.distTo(proj);
            } else {
                proj = math::projPointLine(p, a, b);
                d = p.distTo(proj);
            }
        } else {
            proj = math::projPointLine(p, a, b);
            d = p.distTo(proj);
        }
        if (d < bestD) { bestD = d; bestSeg = i; bestProj = proj; }
    }
    if (bestSeg < 0) return;

    ctx.model->updateElement(e.id, [&](Element& out) {
        // 在 bestSeg+1 处插入；同步把该段拆成两段同类型（弧段仍为弧段）
        out.pts.insert(out.pts.begin() + bestSeg + 1, bestProj);
        // 旧 flags 长度需 +1：若原 flags[bestSeg] 是弧，则拆后两段都是弧
        std::vector<bool> nf;
        nf.reserve(out.arcFlags.size() + 1);
        for (int i = 0; i < bestSeg; ++i) nf.push_back(out.arcFlags[i]);
        const bool wasArc = (bestSeg < (int)out.arcFlags.size()) && out.arcFlags[bestSeg];
        nf.push_back(wasArc);
        nf.push_back(wasArc);
        for (int i = bestSeg + 1; i < (int)out.arcFlags.size(); ++i) nf.push_back(out.arcFlags[i]);
        out.arcFlags = std::move(nf);
    });
    ctx.model->log(QString("已在折线 #%1 第 %2 段插入节点").arg(e.id).arg(bestSeg + 1));
}

void PolylineEditTool::appendNode(ToolContext& ctx, const Element& e, const Vec3& p) {
    if (e.pts.empty()) return;
    const double dF = p.distTo(e.pts.front());
    const double dB = p.distTo(e.pts.back());
    const bool atFront = (dF < dB);
    ctx.model->updateElement(e.id, [&](Element& out) {
        if (atFront) out.pts.insert(out.pts.begin(), p);
        else         out.pts.push_back(p);
        // 在最外端插入节点时，原有 flags 不动（首/末不新增段）
    });
    ctx.model->log(QString("已在折线 #%1 的%2端追加节点")
                       .arg(e.id).arg(atFront ? QStringLiteral("首") : QStringLiteral("末")));
}

void PolylineEditTool::deleteNode(ToolContext& ctx, const Element& e, const Vec3& p) {
    if (e.pts.size() <= 2) {
        ctx.model->removeElement(e.id);
        ctx.model->log(QString("折线 #%1 节点过少，已删除").arg(e.id));
        return;
    }
    const int idx = nearestNode(e, p);
    if (idx < 0) return;
    ctx.model->updateElement(e.id, [&](Element& out) {
        out.pts.erase(out.pts.begin() + idx);
        // 同步 flags：删除节点 i 使段数减 1
        //   - 删除首节点（i==0）：旧段 0 消失，其余上移
        //   - 删除末节点（i==n-1）：旧段 n-2 消失
        //   - 删除中间节点（i in 1..n-2）：旧段 i 消失
        std::vector<bool> nf;
        nf.reserve(out.arcFlags.size());
        const int n = (int)out.pts.size();
        if (idx == 0) {
            for (int k = 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        } else {
            for (int k = 0; k < idx && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            for (int k = idx + 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        }
        out.arcFlags = std::move(nf);
        // 调整 closed 标记：若原 closed=true 且节点数 2/3 不应自动断开
        (void)n;
    });
    ctx.model->log(QString("已删除折线 #%1 的节点 %2").arg(e.id).arg(idx));
}

void PolylineEditTool::deleteBetween(ToolContext& ctx, const Element& e, const Vec3& p) {
    const int idx = nearestNode(e, p);
    if (idx < 0) return;

    if (firstId_ < 0) {
        firstId_  = e.id;
        firstIdx_ = idx;
        ctx.model->log(QString("已选择折线 #%1 节点 %2").arg(e.id).arg(idx));
        return;
    }
    if (firstId_ != e.id) {
        ctx.model->log(QStringLiteral("请选择同一折线上的两个节点"), "warn");
        firstId_ = -1; firstIdx_ = -1;
        return;
    }
    int a = firstIdx_, b = idx;
    if (a > b) std::swap(a, b);
    if (b - a <= 1) {
        ctx.model->log(QStringLiteral("两个节点相邻，无需删除"), "warn");
        firstId_ = -1; firstIdx_ = -1;
        return;
    }
    ctx.model->updateElement(e.id, [&](Element& out) {
        std::vector<Vec3> nv;
        nv.reserve(out.pts.size());
        for (int i = 0; i <= a; ++i) nv.push_back(out.pts[i]);
        for (int i = b; i < (int)out.pts.size(); ++i) nv.push_back(out.pts[i]);
        out.pts = std::move(nv);
        // flags：删除中间 [a+1, b-1] 节点，对应段 [a+1, b-1] 消失
        std::vector<bool> nf;
        nf.reserve(out.arcFlags.size());
        for (int k = 0; k <= a && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        for (int k = b; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        out.arcFlags = std::move(nf);
    });
    ctx.model->log(QString("已删除折线 #%1 节点 %2 到 %3 之间的部分").arg(e.id).arg(a).arg(b));
    firstId_ = -1; firstIdx_ = -1;
}

void PolylineEditTool::trimSegments(ToolContext& ctx, const Element& e, const Vec3& p) {
    const int n = (int)e.pts.size();
    if (n < 3) { ctx.model->log(QStringLiteral("节点不足，无法修剪"), "warn"); return; }
    int seg = nearestSeg(e, p);
    if (seg < 0) return;
    // 删除该段的"终点"节点（=pts[(seg+1)%n]），使段数 -1
    const int delNode = (seg + 1) % n;
    if (n - 1 < 2) { ctx.model->log(QStringLiteral("修剪后节点不足"), "warn"); return; }
    ctx.model->updateElement(e.id, [&](Element& out) {
        out.pts.erase(out.pts.begin() + delNode);
        std::vector<bool> nf;
        nf.reserve(out.arcFlags.size());
        for (int k = 0; k < seg && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        for (int k = seg + 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
        out.arcFlags = std::move(nf);
    });
    ctx.model->log(QString("已修剪折线 #%1 的第 %2 段").arg(e.id).arg(seg + 1));
}

void PolylineEditTool::equidistant(ToolContext& ctx, const Element& e, const Vec3& p) {
    // 偏移距离：取鼠标到折线质心的距离作为 d 偏移量（用户可由此精确控制）
    Vec3 c(0, 0, 0);
    for (const auto& v : e.pts) c = c + v;
    c = c * (1.0 / std::max(1, (int)e.pts.size()));
    double d = p.distTo(c);
    if (d < 0.1) d = 5.0;    // 太近时给个 5mm 默认值

    const int n = (int)e.pts.size();
    if (n < 2) return;
    std::vector<Vec3> np(n);
    for (int i = 0; i < n; ++i) np[i] = e.pts[i];
    // 直线段：每段两端沿法向偏移（取两邻接段法向的均值作节点法向）
    for (int i = 0; i < n; ++i) {
        const Vec3& prev = e.pts[(i - 1 + n) % n];
        const Vec3& cur  = e.pts[i];
        const Vec3& next = e.pts[(i + 1) % n];
        // 段 i-1 方向（cur - prev）
        const double dx1 = cur.x - prev.x, dy1 = cur.y - prev.y;
        const double l1 = std::hypot(dx1, dy1);
        const double dx2 = next.x - cur.x, dy2 = next.y - cur.y;
        const double l2 = std::hypot(dx2, dy2);
        if (l1 < 1e-9 || l2 < 1e-9) continue;
        // 段法向（左旋 90°）：(-dy, dx)/l
        const double nx1 = -dy1 / l1, ny1 = dx1 / l1;
        const double nx2 = -dy2 / l2, ny2 = dx2 / l2;
        // 节点法向 = 两邻接法向平均
        double nx = (nx1 + nx2) * 0.5, ny = (ny1 + ny2) * 0.5;
        const double ln = std::hypot(nx, ny);
        if (ln < 1e-9) continue;
        nx /= ln; ny /= ln;
        np[i] = Vec3(cur.x + nx * d, cur.y + ny * d, cur.z);
    }
    // flags 不变（直线 → 仍直线；弧段 → 仍弧段，圆心外扩）
    Element out = ElementFactory::createPolyline(np, e.arcFlags, e.closed, ctx.model);
    out.name = e.name + QStringLiteral("(等距)");
    const int newId = ctx.model->addElement(out);
    ctx.model->setSelected(newId);
    ctx.model->log(QString("已创建等距折线 #%1（d=%2 mm）").arg(newId).arg(d, 0, 'f', 2));
}

void PolylineEditTool::fromSegments(ToolContext& ctx, int pickedId) {
    fromIds_.push_back(pickedId);
    if (fromIds_.size() == 1) {
        ctx.model->log(QString("已选第 1 段（#%1），点击下一段或右键结束").arg(pickedId));
        return;
    }
    // 检查端点是否接近上一段末端 → 决定是否真合并（v1 不强制，仅拼接节点）
    // 简化：把所有段的两个端点按拾取顺序串起来
    std::vector<Vec3> pts;
    for (int id : fromIds_) {
        const Element* s = ctx.model->element(id);
        if (!s) continue;
        if (s->type == ElementType::Line) {
            if (s->pts.size() >= 2) {
                if (pts.empty()) { pts.push_back(s->pts[0]); pts.push_back(s->pts[1]); }
                else pts.push_back(s->pts[1]);
            }
        } else if (s->type == ElementType::Arc) {
            if (!s->pts.empty() && s->radius > 0) {
                const double a0 = math::deg2rad(s->startAngle);
                const double a1 = math::deg2rad(s->endAngle);
                const Vec3 a(s->pts[0].x + s->radius * std::cos(a0),
                             s->pts[0].y + s->radius * std::sin(a0), 0);
                const Vec3 b(s->pts[0].x + s->radius * std::cos(a1),
                             s->pts[0].y + s->radius * std::sin(a1), 0);
                if (pts.empty()) { pts.push_back(a); pts.push_back(b); }
                else pts.push_back(b);
            }
        } else if (s->type == ElementType::Polyline) {
            for (const auto& v : s->pts) pts.push_back(v);
        }
    }
    if (pts.size() < 2) {
        ctx.model->log(QStringLiteral("所选段节点不足"), "warn");
        fromIds_.clear();
        return;
    }
    std::vector<bool> flags(pts.size() - 1, false);
    Element el = ElementFactory::createPolyline(pts, flags, /*closed=*/false, ctx.model);
    const int newId = ctx.model->addElement(el);
    ctx.model->setSelected(newId);
    // 清空缓冲，开始新一轮
    fromIds_.clear();
    ctx.model->log(QString("已合并 %1 段为折线 #%2（节点 %3）")
                       .arg((int)fromIds_.size() + 1).arg(newId).arg((int)pts.size()));
}

void PolylineEditTool::arcsToChords(ToolContext& ctx, const Element& e) {
    int nArc = 0;
    for (bool a : e.arcFlags) if (a) ++nArc;
    ctx.model->updateElement(e.id, [&](Element& out) {
        std::fill(out.arcFlags.begin(), out.arcFlags.end(), false);
    });
    ctx.model->log(QString("已将折线 #%1 的 %2 段弧转为直线").arg(e.id).arg(nArc));
}

void PolylineEditTool::changeSegmentType(ToolContext& ctx, const Element& e, const Vec3& p) {
    const int seg = nearestSeg(e, p);
    if (seg < 0) return;

    if (firstId_ < 0) {
        firstId_ = e.id;
        firstSeg_ = seg;
        const bool cur = (seg < (int)e.arcFlags.size()) && e.arcFlags[seg];
        ctx.model->log(QString("折线 #%1 第 %2 段：当前 %3，单击第二段切换两段（右键取消）")
                           .arg(e.id).arg(seg + 1)
                           .arg(cur ? QStringLiteral("弧") : QStringLiteral("直线")));
        return;
    }
    if (firstId_ != e.id) {
        ctx.model->log(QStringLiteral("请选择同一折线上的两段"), "warn");
        firstId_ = -1; firstSeg_ = -1;
        return;
    }
    int a = firstSeg_, b = seg;
    if (a > b) std::swap(a, b);
    ctx.model->updateElement(e.id, [&](Element& out) {
        const int nSegs = (int)out.arcFlags.size();
        for (int k = a; k <= b; ++k) {
            if (k < nSegs) out.arcFlags[k] = !out.arcFlags[k];
        }
    });
    ctx.model->log(QString("已切换折线 #%1 第 %2 到 %3 段的类型").arg(e.id).arg(a + 1).arg(b + 1));
    firstId_ = -1; firstSeg_ = -1;
}

// ====================== PEDIT 式六操作 ======================

void PolylineEditTool::fit(ToolContext& ctx, const Element& e) {
    polyops::fit(*ctx.model, e.id);
}

void PolylineEditTool::spline(ToolContext& ctx, const Element& e) {
    polyops::spline(*ctx.model, e.id);
}

void PolylineEditTool::setWidth(ToolContext& ctx, const Element& e) {
    bool ok = false;
    const double cur = e.lineWidth;
    const double w = QInputDialog::getDouble(nullptr,
                                            QStringLiteral("设置线宽"),
                                            QStringLiteral("线宽 (mm/px):"),
                                            cur, 0.0, 1000.0, 2, &ok);
    if (!ok) { ctx.model->log(QStringLiteral("已取消设置线宽")); return; }
    polyops::setWidth(*ctx.model, e.id, w);
}

void PolylineEditTool::closePolyline(ToolContext& ctx, const Element& e) {
    // PEDIT Close 为切换：已闭合则打开，否则闭合
    polyops::setClosed(*ctx.model, e.id, !e.closed);
}

void PolylineEditTool::reverse(ToolContext& ctx, const Element& e) {
    polyops::reverse(*ctx.model, e.id);
}

void PolylineEditTool::join(ToolContext& ctx, const Element& e, const Vec3& p) {
    (void)p;
    if (firstId_ < 0) return;
    if (!polyops::join(*ctx.model, firstId_, e.id, 1e-3)) {
        // 失败信息已在 polyops 内 log；保持 target 以便重试
    }
}

} // namespace SketchCAD
