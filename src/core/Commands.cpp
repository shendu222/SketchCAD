#include "Commands.h"
#include "SketchModel.h"
#include "Geometry.h"
#include "Trim.h"
#include "LineModes.h"
#include "CircleModes.h"
#include "ArcModes.h"
#include "PolyModes.h"
#include "SplineModes.h"
#include "EllipseModes.h"
#include "ElementFactory.h"
#include "Modify.h"
#include "AircraftViews.h"
#include <QRegularExpression>
#include <algorithm>

namespace SketchCAD {

// ============================================================
// CommandContext 实现
// ============================================================

bool CommandContext::tryParsePoint(const QString& s, Vec3& out) {
    QStringList parts = s.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;
    bool ok1, ok2, ok3 = true;
    double x = parts[0].toDouble(&ok1);
    double y = parts[1].toDouble(&ok2);
    double z = 0;
    if (parts.size() >= 3) { z = parts[2].toDouble(&ok3); }
    if (!ok1 || !ok2 || !ok3) return false;
    out = Vec3(x, y, z);
    return true;
}

bool CommandContext::parsePoint(int idx, Vec3& out) const {
    if (idx >= toks.size()) return false;
    return tryParsePoint(toks[idx], out);
}

std::vector<Vec3> CommandContext::parsePts(int start, int n) const {
    std::vector<Vec3> out;
    for (int i = 0; i < n; ++i) {
        Vec3 p;
        if (start + i >= toks.size() || !tryParsePoint(toks[start + i], p)) return {};
        out.push_back(p);
    }
    return out;
}

std::vector<int> CommandContext::parseRefs(int start, int n) const {
    std::vector<int> out;
    for (int i = 0; i < n; ++i) {
        bool ok; int id = 0;
        if (start + i >= toks.size() || !(id = toks[start + i].toInt(&ok)) || !ok) return {};
        out.push_back(id);
    }
    return out;
}

void CommandContext::makeLine(const Vec3& a, const Vec3& b) const {
    Element e = ElementFactory::create(ElementType::Line, "直线", model);
    e.pts = {a, b};
    int id = model->addElement(e);
    model->log(QString("已创建直线 #%1 (%2,%3)->(%4,%5)")
                   .arg(id).arg(a.x,0,'f',2).arg(a.y,0,'f',2).arg(b.x,0,'f',2).arg(b.y,0,'f',2));
}

// ============================================================
// 基础绘图命令
// ============================================================

bool LineCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log("用法: line x1,y1 x2,y2", "warn"); return false; }
    Vec3 a, b;
    if (!ctx.parsePoint(1, a) || !ctx.parsePoint(2, b)) { m->log("点解析失败", "warn"); return false; }
    Element e = ElementFactory::create(ElementType::Line, "直线", m); e.pts = {a, b};
    int id = m->addElement(e); m->log("已创建直线 #" + QString::number(id));
    return true;
}

bool CircleCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log("用法: circle cx,cy r", "warn"); return false; }
    Vec3 c; double r;
    if (!ctx.parsePoint(1, c)) { m->log("圆心解析失败", "warn"); return false; }
    bool ok; r = ctx.toks[2].toDouble(&ok);
    if (!ok || r <= 0) { m->log("半径无效", "warn"); return false; }
    Element e = ElementFactory::create(ElementType::Circle, "圆", m); e.pts = {c}; e.radius = r;
    int id = m->addElement(e); m->log("已创建圆 #" + QString::number(id));
    return true;
}

bool Circle3PCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 4) { m->log("用法: circle3p x1,y1 x2,y2 x3,y3", "warn"); return false; }
    Vec3 a, b, c;
    if (!ctx.parsePoint(1, a) || !ctx.parsePoint(2, b) || !ctx.parsePoint(3, c)) {
        m->log("点解析失败", "warn"); return false;
    }
    Vec3 ctr; double r = 0;
    if (!geo::circleFrom3Points(a, b, c, ctr, r)) {
        m->log("三点共线或重合，无法确定圆", "warn"); return false;
    }
    Element e = ElementFactory::create(ElementType::Circle, "圆", m); e.pts = {ctr}; e.radius = r;
    int id = m->addElement(e);
    m->log(QString("已创建三点圆 #%1  圆心(%2, %3) R=%4")
               .arg(id).arg(ctr.x, 0, 'f', 2).arg(ctr.y, 0, 'f', 2).arg(r, 0, 'f', 2));
    return true;
}

bool ArcCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 4) { m->log("用法: arc cx,cy r a0 a1", "warn"); return false; }
    Vec3 c; double r, a0, a1;
    if (!ctx.parsePoint(1, c)) { m->log("圆心解析失败", "warn"); return false; }
    bool ok; r = ctx.toks[2].toDouble(&ok); a0 = ctx.toks[3].toDouble(&ok);
    a1 = ctx.toks.size() >= 5 ? ctx.toks[4].toDouble(&ok) : a0 + 360.0;
    if (!ok || r <= 0) { m->log("参数无效", "warn"); return false; }
    Element e = ElementFactory::create(ElementType::Arc, "圆弧", m);
    e.pts = {c}; e.radius = r; e.startAngle = a0; e.endAngle = a1;
    int id = m->addElement(e); m->log("已创建圆弧 #" + QString::number(id));
    return true;
}

bool RectCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log("用法: rect x0,y0 x1,y1", "warn"); return false; }
    Vec3 a, b;
    if (!ctx.parsePoint(1, a) || !ctx.parsePoint(2, b)) { m->log("点解析失败", "warn"); return false; }
    Element e = ElementFactory::create(ElementType::Rectangle, "矩形", m); e.pts = {a, b};
    int id = m->addElement(e); m->log("已创建矩形 #" + QString::number(id));
    return true;
}

bool PolySplineCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log("用法: " + ctx.cmd + " p1 p2 ...", "warn"); return false; }
    std::vector<Vec3> ps;
    for (int i = 1; i < ctx.toks.size(); ++i) {
        Vec3 p; if (!CommandContext::tryParsePoint(ctx.toks[i], p)) { m->log("点解析失败: " + ctx.toks[i], "warn"); return false; }
        ps.push_back(p);
    }
    Element e = ElementFactory::create(type_, name_, m); e.pts = ps;
    int id = m->addElement(e);
    m->log("已创建" + name_ + " #" + QString::number(id) + " (" + QString::number(ps.size()) + " 顶点)");
    return true;
}

// ============================================================
// 直线子模式（line_* 前缀家族）
// ============================================================

bool LineModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const line::Mode lm = line::modeFromString(ctx.cmd);

    switch (lm) {
    case line::Mode::TwoPoint: {
        auto pts = ctx.parsePts(1, 2);
        if (pts.size() < 2) { m->log("用法: line_2pt x1,y1 x2,y2", "warn"); return false; }
        ctx.makeLine(pts[0], pts[1]); return true; }
    case line::Mode::Horizontal: {
        auto pts = ctx.parsePts(1, 2);
        if (pts.size() < 2) { m->log("用法: line_h x1,y1 x2,y2", "warn"); return false; }
        auto r = line::horizontal(pts[0], pts[1]); ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Vertical: {
        auto pts = ctx.parsePts(1, 2);
        if (pts.size() < 2) { m->log("用法: line_v x1,y1 x2,y2", "warn"); return false; }
        auto r = line::vertical(pts[0], pts[1]); ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Angle: {
        double ang = 30.0;
        if (ctx.toks.size() >= 4) { bool ok; ang = ctx.toks[ctx.toks.size()-1].toDouble(&ok); if (!ok) { m->log("角度无效", "warn"); return false; } }
        auto pts = ctx.parsePts(1, 2);
        if (pts.size() < 2) { m->log("用法: line_angle x1,y1 x2,y2 [angle]", "warn"); return false; }
        auto r = line::angle(pts[0], pts[1], ang); ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Parallel: {
        if (ctx.toks.size() < 4) { m->log("用法: line_parallel refId x1,y1 x2,y2", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 2);
        if (refs.empty() || pts.size() < 2) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::parallelThroughPoint(*ref, pts[0], pts[1]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::ParallelThroughPoint: {
        if (ctx.toks.size() < 3) { m->log("用法: line_parallel_pt refId x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 1);
        if (refs.empty() || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::parallelAligned(*ref, pts[0]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Orthogonal: {
        if (ctx.toks.size() < 4) { m->log("用法: line_orthogonal refId x1,y1 x2,y2", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 2);
        if (refs.empty() || pts.size() < 2) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::orthogonalThroughPoint(*ref, pts[0], pts[1]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::RelativeAngle:
    case line::Mode::AngleFromLine: {
        double ang = 30.0;
        if (ctx.toks.size() >= 5) { bool ok; ang = ctx.toks[ctx.toks.size()-1].toDouble(&ok); if (!ok) ang = 30.0; }
        if (ctx.toks.size() < 4) { m->log("用法: line_rel_angle refId x1,y1 x2,y2 [angle]", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 2);
        if (refs.empty() || pts.size() < 2) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::relativeAngle(*ref, pts[0], pts[1], ang);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::OrthogonalFromLine: {
        if (ctx.toks.size() < 4) { m->log("用法: line_orthogonal_from_line refId x1,y1 x2,y2", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 2);
        if (refs.empty() || pts.size() < 2) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::orthogonalFromLine(*ref, pts[0], pts[1]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::FromPointToLine: {
        if (ctx.toks.size() < 3) { m->log("用法: line_from_point_to_line refId x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 1);
        if (refs.empty() || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::fromPointToLine(*ref, pts[0]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Bisector: {
        if (ctx.toks.size() < 4) { m->log("用法: line_bisector refId1 refId2 x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 2); auto pts = ctx.parsePts(3, 1);
        if (refs.size() < 2 || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* l1 = m->element(refs[0]); const Element* l2 = m->element(refs[1]);
        if (!l1 || !l2) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::bisector(*l1, *l2, pts[0], pts[0]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::TangentPC: {
        if (ctx.toks.size() < 3) { m->log("用法: line_tangent_pc circleId x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 1);
        if (refs.empty() || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* c = m->element(refs[0]);
        if (!c) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::tangentPC(*c, pts[0], pts[0]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::TangentCC: {
        if (ctx.toks.size() < 3) { m->log("用法: line_tangent_cc circleId1 circleId2", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 2);
        if (refs.size() < 2) { m->log("参数解析失败", "warn"); return false; }
        const Element* c1 = m->element(refs[0]); const Element* c2 = m->element(refs[1]);
        if (!c1 || !c2) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::tangentCC(*c1, *c2, Vec3(0,0,0));
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::TangentOrthogonal: {
        if (ctx.toks.size() < 4) { m->log("用法: line_tangent_ortho circleId lineId x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 2); auto pts = ctx.parsePts(3, 1);
        if (refs.size() < 2 || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* c = m->element(refs[0]); const Element* l = m->element(refs[1]);
        if (!c || !l) { m->log("参考图元不存在", "warn"); return false; }
        auto r = line::tangentOrthogonal(*c, *l, pts[0]);
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        ctx.makeLine(r.a, r.b); return true; }
    case line::Mode::Snake:
    case line::Mode::SnakeX:
    case line::Mode::SnakeY: {
        if (ctx.toks.size() < 3) { m->log("用法: line_snake p1 p2 ...", "warn"); return false; }
        std::vector<Vec3> snakePts;
        for (int i = 1; i < ctx.toks.size(); ++i) {
            Vec3 p; if (!CommandContext::tryParsePoint(ctx.toks[i], p)) { m->log("点解析失败", "warn"); return false; }
            snakePts.push_back(p);
        }
        bool xFirst = (lm == line::Mode::SnakeX) ? true : (lm == line::Mode::SnakeY) ? false : true;
        for (size_t i = 1; i < snakePts.size(); ++i) {
            auto r = line::snakeSegment(std::vector<Vec3>(snakePts.begin(), snakePts.begin() + i), snakePts[i], xFirst);
            if (r.valid) ctx.makeLine(r.a, r.b);
        }
        return true; }
    default:
        m->log("未实现的直线子命令: " + ctx.cmd, "warn"); return false;
    }
}

// ---- 圆弧子模式（arc_* 前缀分派，与 LineModeCommand 同构）----
bool ArcModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const arc::Mode am = arc::modeFromString(ctx.cmd);
    const arc::ReqCount rc = arc::requirements(am);

    auto pts  = ctx.parsePts(1, rc.pts);
    auto refs = ctx.parseRefs(1, rc.refs);
    double param = 0.0;
    if (rc.usesParam && ctx.toks.size() > 1)
        param = ctx.toks.back().toDouble();

    // 用最后一个点作为光标位置（命令行无真实光标）
    Vec3 cur = pts.empty() ? Vec3(0, 0, 0) : pts.back();

    arc::Result r = arc::compute(am, pts, refs, cur, m->elements(), param);
    if (!r.valid) { m->log(r.msg.isEmpty()
        ? QStringLiteral("无法创建圆弧: ") + ctx.cmd : r.msg, "warn"); return true; }

    Element el = ElementFactory::createArc(r.center, r.radius, r.startAngle, r.endAngle, m);
    if (!r.msg.isEmpty() && r.msg.startsWith(QStringLiteral("椭圆弧"))) {
        el.name = QStringLiteral("椭圆弧");
    }
    m->addElement(el);
    return true;
}

// ============================================================
// 多边形子模式（poly_* / rect_chamfer / rect_fillet 前缀家族）
// ============================================================

bool PolyModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const polygon::Mode pm = polygon::modeFromString(ctx.cmd);
    const polygon::ReqCount rc = polygon::requirements(pm);

    auto pts = ctx.parsePts(1, rc.pts);
    if ((int)pts.size() < rc.pts) {
        m->log(QStringLiteral("用法: %1 %2 [参数]")
               .arg(ctx.cmd)
               .arg(QString(rc.pts, 'p').replace('p', "x,y ")), "warn");
        return false;
    }

    // 解析可选参数：边数或倒角/圆角尺寸
    int sides = 6;
    double param = 0.0;
    if (rc.usesParam && ctx.toks.size() > 1 + rc.pts) {
        const QString& last = ctx.toks.back();
        if (pm == polygon::Mode::RectChamfer || pm == polygon::Mode::RectFillet)
            param = last.toDouble();
        else
            sides = std::max(3, last.toInt());
    }

    // 用最后一个点作为光标位置（命令行无真实光标）
    Vec3 cur = pts.empty() ? Vec3(0, 0, 0) : pts.back();

    polygon::Result r = polygon::compute(pm, pts, {}, cur, m->elements(), param, sides);
    if (!r.valid) {
        m->log(r.msg.isEmpty()
            ? QStringLiteral("无法创建多边形: ") + ctx.cmd : r.msg, "warn");
        return true;
    }

    const QString name = (r.type == ElementType::Rectangle)
        ? QStringLiteral("矩形") : QStringLiteral("多边形");
    Element el = ElementFactory::create(r.type, name, m);
    el.pts = r.pts;
    el.closed = true;
    m->addElement(el);
    return true;
}

// ============================================================
// 样条家族命令（spline_ 前缀分派 + freehand_line）
// ------------------------------------------------------------
// 绘制（与参考图 FreeCAD Spline 菜单一一对应）：
//   spline_parabola f? 3 点切线式抛物线
//   spline_hyperbola F1 F2 P        双曲线（离心点式）
//   spline_parabola_4p P0..P3       4 点抛物线
//   spline_parabola_fd F D1 D2      焦点准线抛物线
//   spline_through_points P...      样条过点（≥2 点）
//   freehand_line P...              自由手绘线（≥2 点，subType=Freehand）
// 编辑：
//   spline_from_polyline <id>       折线/多边形/矩形 → 样条
//   spline_append_pt <id> x,y       追加控制点（自动判前端/后端）
//   spline_insert_pts <id> x,y      最近段投影插入控制点
//   spline_remove_pts <id> <idx>    删除指定索引控制点（0 起，须保留 ≥2 点）
//   spline_remove_between <id> <i> <j>  删除 i..j 之间的段
//   spline_explode <id>             炸开为多条独立线段
// ============================================================

bool SplineModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.cmd;

    // ---- 编辑命令：目标 id 一律为第一个参数 ----
    if (cmd == "spline_from_polyline" || cmd == "spline_append_pt" ||
        cmd == "spline_insert_pts"   || cmd == "spline_remove_pts" ||
        cmd == "spline_remove_between" || cmd == "spline_explode") {
        if (ctx.toks.size() < 2) { m->log(QStringLiteral("用法: %1 <id> ...").arg(cmd), "warn"); return false; }
        bool ok = false;
        const int id = ctx.toks[1].toInt(&ok);
        if (!ok) { m->log(QStringLiteral("id 无效"), "warn"); return false; }
        const Element* e = m->element(id);
        if (!e) { m->log(QStringLiteral("未找到图元 #") + ctx.toks[1], "warn"); return false; }

        if (cmd == "spline_from_polyline") {
            const std::vector<Vec3> pts = spline::polylineControlPoints(*e);
            if (pts.size() < 2) { m->log(QStringLiteral("源图元不是可转换的折线/多边形/矩形"), "warn"); return false; }
            Element sp = ElementFactory::create(ElementType::Spline, QStringLiteral("样条曲线"), m);
            sp.pts = pts;
            sp.closed = false;
            sp.splineSubType = static_cast<int>(spline::SubType::CatmullRom);
            const auto ids = m->replaceElement(id, {sp}, QStringLiteral("折线转样条"));
            if (!ids.empty())
                m->log(QString("已将 #%1 转换为样条 #%2（%3 个控制点）")
                           .arg(id).arg(ids.front()).arg((int)pts.size()));
            return true;
        }

        // 以下命令只作用于样条
        if (e->type != ElementType::Spline) {
            m->log(QString("图元 #%1 不是样条曲线").arg(id), "warn");
            return false;
        }

        if (cmd == "spline_append_pt") {
            Vec3 p;
            if (!ctx.parsePoint(2, p)) { m->log(QStringLiteral("用法: spline_append_pt <id> x,y"), "warn"); return false; }
            const bool atFront = p.distTo(e->pts.front()) < p.distTo(e->pts.back());
            m->updateElement(id, [&](Element& out) {
                if (atFront) out.pts.insert(out.pts.begin(), p);
                else         out.pts.push_back(p);
            });
            m->log(QString("已在样条 #%1 %2追加控制点").arg(id)
                       .arg(atFront ? QStringLiteral("前端") : QStringLiteral("后端")));
            return true;
        }
        if (cmd == "spline_insert_pts") {
            Vec3 p;
            if (!ctx.parsePoint(2, p)) { m->log(QStringLiteral("用法: spline_insert_pts <id> x,y"), "warn"); return false; }
            if (e->pts.size() < 2) { m->log(QStringLiteral("样条控制点不足"), "warn"); return false; }
            int bestSeg = -1; double bestD = 1e100; Vec3 bestProj;
            for (int i = 0; i + 1 < (int)e->pts.size(); ++i) {
                const Vec3 proj = math::projPointLine(p, e->pts[i], e->pts[i + 1]);
                const double d = p.distTo(proj);
                if (d < bestD) { bestD = d; bestSeg = i; bestProj = proj; }
            }
            if (bestSeg < 0) return false;
            m->updateElement(id, [&](Element& out) {
                out.pts.insert(out.pts.begin() + bestSeg + 1, bestProj);
            });
            m->log(QString("已在样条 #%1 第 %2 段插入控制点").arg(id).arg(bestSeg + 1));
            return true;
        }
        if (cmd == "spline_remove_pts") {
            if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: spline_remove_pts <id> <index>（0 起）"), "warn"); return false; }
            const int idx = ctx.toks[2].toInt();
            if (idx < 0 || idx >= (int)e->pts.size()) { m->log(QStringLiteral("索引越界"), "warn"); return false; }
            if (e->pts.size() <= 2) { m->log(QStringLiteral("样条至少保留 2 个控制点"), "warn"); return false; }
            m->updateElement(id, [&](Element& out) { out.pts.erase(out.pts.begin() + idx); });
            m->log(QString("已删除样条 #%1 的控制点 %2").arg(id).arg(idx));
            return true;
        }
        if (cmd == "spline_remove_between") {
            if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法: spline_remove_between <id> <i> <j>"), "warn"); return false; }
            int a = ctx.toks[2].toInt(), b = ctx.toks[3].toInt();
            if (a > b) std::swap(a, b);
            if (a < 0 || b >= (int)e->pts.size() || b - a <= 1) {
                m->log(QStringLiteral("索引无效或两点相邻（无需删除中间段）"), "warn");
                return false;
            }
            m->updateElement(id, [&](Element& out) {
                std::vector<Vec3> nv;
                nv.reserve(out.pts.size());
                for (int i = 0; i <= a; ++i) nv.push_back(out.pts[i]);
                for (int i = b; i < (int)out.pts.size(); ++i) nv.push_back(out.pts[i]);
                out.pts = std::move(nv);
            });
            m->log(QString("已删除样条 #%1 控制点 %2 到 %3 之间的部分").arg(id).arg(a).arg(b));
            return true;
        }
        // spline_explode
        {
            const auto segs = spline::explodeToLines(e->pts, e->splineSubType);
            if (segs.empty()) { m->log(QString("样条 #%1 无法分解为线段").arg(id), "warn"); return false; }
            std::vector<Element> lines;
            lines.reserve(segs.size());
            for (const auto& s : segs) {
                Element ln = ElementFactory::create(ElementType::Line, QStringLiteral("线段"), m);
                ln.pts = {s.first, s.second};
                ln.closed = false;
                lines.push_back(std::move(ln));
            }
            const auto ids = m->replaceElement(id, lines, QStringLiteral("样条分解为线段"));
            m->log(QString("已将样条 #%1 分解为 %2 条线段").arg(id).arg((int)ids.size()));
            return true;
        }
    }

    // ---- freehand_line：点列直存（subType=Freehand），与手绘工具同数据 ----
    if (cmd == "freehand_line") {
        std::vector<Vec3> pts;
        for (int i = 1; i < ctx.toks.size(); ++i) {
            Vec3 p;
            if (!CommandContext::tryParsePoint(ctx.toks[i], p)) {
                m->log(QStringLiteral("点解析失败: ") + ctx.toks[i], "warn");
                return false;
            }
            pts.push_back(p);
        }
        if (pts.size() < 2) { m->log(QStringLiteral("用法: freehand_line x1,y1 x2,y2 ...（≥2 点）"), "warn"); return false; }
        Element el = ElementFactory::create(ElementType::Spline, QStringLiteral("自由手绘线"), m);
        el.pts = std::move(pts);
        el.closed = false;
        el.splineSubType = static_cast<int>(spline::SubType::Freehand);
        m->addElement(el);
        return true;
    }

    // ---- 样条绘制子模式（spline_parabola / spline_hyperbola / ...）----
    const spline::Mode sm = spline::modeFromString(cmd);
    if (sm == spline::Mode::Free) {
        m->log(QStringLiteral("未实现的样条子命令: ") + cmd, "warn");
        return false;
    }
    const spline::ReqCount rc = spline::requirements(sm);

    std::vector<Vec3> pts;
    if (rc.pts > 0) {
        // 定点数模式：恰好解析 rc.pts 个点
        pts = ctx.parsePts(1, rc.pts);
        if ((int)pts.size() < rc.pts) {
            m->log(QStringLiteral("用法: %1 %2")
                   .arg(cmd)
                   .arg(QString(rc.pts, 'p').replace('p', "x,y ")), "warn");
            return false;
        }
    } else {
        // 可变点数模式（spline_through_points）：解析全部 token 为点，≥2 个
        for (int i = 1; i < ctx.toks.size(); ++i) {
            Vec3 p;
            if (!CommandContext::tryParsePoint(ctx.toks[i], p)) {
                m->log(QStringLiteral("点解析失败: ") + ctx.toks[i], "warn");
                return false;
            }
            pts.push_back(p);
        }
        if (pts.size() < 2) {
            m->log(QStringLiteral("用法: %1 x1,y1 x2,y2 ...（≥2 点）").arg(cmd), "warn");
            return false;
        }
        // 过点样条直存（不注入光标点，与 UI 行为一致：命令行无真实光标）
        Element el = ElementFactory::create(ElementType::Spline, spline::stringFromMode(sm), m);
        el.pts = std::move(pts);
        el.splineSubType = static_cast<int>(spline::SubType::ThroughPoints);
        el.closed = false;
        m->addElement(el);
        return true;
    }

    // 定点数模式：用最后一个点作为光标位置（命令行无真实光标）
    Vec3 cur = pts.empty() ? Vec3(0, 0, 0) : pts.back();

    spline::Result r = spline::compute(sm, pts, cur);
    if (!r.valid) {
        m->log(r.msg.isEmpty()
            ? QStringLiteral("无法创建曲线: ") + cmd : r.msg, "warn");
        return true;
    }

    const QString name = spline::stringFromMode(sm);
    Element el = ElementFactory::create(ElementType::Spline, name, m);
    el.pts = r.pts;
    el.splineSubType = r.subType;
    el.closed = false;
    m->addElement(el);
    return true;
}

// ============================================================
// 椭圆子模式命令（ellipse_1pt / ellipse_axis / ellipse_foci /
//                 ellipse_4p / ellipse_c3p / ellipse_inscribed）
// ============================================================
bool EllipseModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.cmd;

    const ellipse::Mode em = ellipse::modeFromString(cmd);
    // modeFromString 对未知串兜底为 OnePoint，这里做一次显式校验
    if (ellipse::stringFromMode(em) != cmd) {
        m->log(QStringLiteral("未实现的椭圆子命令: ") + cmd, "warn");
        return false;
    }
    const ellipse::ReqCount rc = ellipse::requirements(em);

    // ---- 参考图元模式（ellipse_inscribed）：第一个参数是框选图元 id ----
    if (rc.refs > 0) {
        if (ctx.toks.size() < 2) {
            m->log(QStringLiteral("用法: %1 <外框矩形/多边形id>").arg(cmd), "warn");
            return false;
        }
        bool ok = false;
        const int id = ctx.toks[1].toInt(&ok);
        if (!ok) { m->log(QStringLiteral("id 无效"), "warn"); return false; }
        const Element* e = m->element(id);
        if (!e) { m->log(QStringLiteral("未找到图元 #") + ctx.toks[1], "warn"); return false; }

        ellipse::Result r = ellipse::compute(em, {}, {id}, Vec3(0, 0, 0),
                                            m->elements(), 0.0);
        if (!r.valid) { m->log(r.msg.isEmpty() ? QStringLiteral("无法创建椭圆") : r.msg, "warn"); return false; }
        Element el = ElementFactory::createEllipse(r.center, r.radius, r.radiusY, r.rotation, m);
        const int nid = m->addElement(el);
        m->log(QString("已创建椭圆 #%1  %2（%3）").arg(nid).arg(el.sizeDesc()).arg(cmd));
        return true;
    }

    // ---- 点输入模式 ----
    // 光标驱动型（1点/轴/焦点）在 UI 里靠鼠标位置补最后一步，命令行没有真实光标，
    // 必须显式多给一个点，故所需点数 = rc.pts + 1；拟合型（4点/中心+3点）点数已足。
    const bool usesCursor = (em == ellipse::Mode::OnePoint ||
                             em == ellipse::Mode::Axis      ||
                             em == ellipse::Mode::FociPoint);
    const int need = usesCursor ? rc.pts + 1 : rc.pts;

    std::vector<Vec3> all = ctx.parsePts(1, need);
    if ((int)all.size() < need) {
        // 逐个拼出用法提示，例如 "x,y x,y x,y"
        QString usage;
        for (int i = 0; i < need; ++i) usage += QStringLiteral("x,y ");
        m->log(QStringLiteral("用法: %1 %2").arg(cmd, usage.trimmed()), "warn");
        return false;
    }

    // 末点充当「光标位置」，光标驱动型需把它从 inputs 里剔除，
    // 否则会被当成已拾取点而重复参与计算。
    Vec3 cur = all.back();
    std::vector<Vec3> inputs = all;
    if (usesCursor) inputs.pop_back();

    ellipse::Result r = ellipse::compute(em, inputs, {}, cur, m->elements(), 0.0);
    if (!r.valid) {
        m->log(r.msg.isEmpty() ? QStringLiteral("无法创建椭圆: ") + cmd : r.msg, "warn");
        return false;
    }
    Element el = ElementFactory::createEllipse(r.center, r.radius, r.radiusY, r.rotation, m);
    const int id = m->addElement(el);
    m->log(QString("已创建椭圆 #%1  中心(%2, %3)  %4（%5）")
               .arg(id).arg(r.center.x, 0, 'f', 2).arg(r.center.y, 0, 'f', 2)
               .arg(el.sizeDesc()).arg(cmd));
    return true;
}

// ============================================================
// 折线族命令：精确 "polyline" + 前缀 "polyline_"
// 9 个编辑子命令 + 1 个创建命令，全部走 PolylineModeCommand。
// 实现独立于 UI（不依赖 GraphicsView），几何算法与 ITool 端等价。
// ============================================================
bool PolylineModeCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.toks.value(0);

    // ---- 创建折线：polyline x,y x,y [x,y ...] ----
    // 首末点距离 < 1e-6 视为闭合；至少 2 个点。
    if (cmd == "polyline") {
        std::vector<Vec3> pts;
        pts.reserve(ctx.toks.size() - 1);
        for (int i = 1; i < ctx.toks.size(); ++i) {
            Vec3 p;
            if (!ctx.tryParsePoint(ctx.toks[i], p)) {
                m->log(QStringLiteral("点解析失败: ") + ctx.toks[i], "warn");
                return false;
            }
            pts.push_back(p);
        }
        if (pts.size() < 2) {
            m->log(QStringLiteral("用法: polyline x,y x,y [x,y ...]"), "warn");
            return false;
        }
        bool closed = (pts.size() >= 3 && pts.front().distTo(pts.back()) < 1e-6);
        if (closed) pts.pop_back();
        std::vector<bool> flags(pts.size() - (closed ? 0 : 1), false);
        Element el = ElementFactory::createPolyline(pts, flags, closed, m);
        const int id = m->addElement(el);
        m->setSelected(id);
        m->log(QString("已创建折线 #%1（%2 节点，%3）").arg(id).arg((int)pts.size())
                   .arg(closed ? QStringLiteral("闭合") : QStringLiteral("开放")));
        return true;
    }

    // ---- 编辑：所有子命令形如 <cmd> <id> [...] ----
    if (ctx.toks.size() < 2) {
        m->log(QStringLiteral("用法: ") + cmd + QStringLiteral(" <id> [...]"), "warn");
        return false;
    }
    bool okId = false;
    const int id = ctx.toks[1].toInt(&okId);
    if (!okId) { m->log(QStringLiteral("id 无效"), "warn"); return false; }

    // ---- from_segments：多源 ID 列表，不走「需先选折线」通用分支 ----
    if (cmd == "polyline_from_segments") {
        std::vector<Vec3> npts;
        std::vector<bool> nflags;
        for (int i = 1; i < ctx.toks.size(); ++i) {
            bool ok; int sid = ctx.toks[i].toInt(&ok);
            if (!ok) continue;
            const Element* s = m->element(sid);
            if (!s) continue;
            if (s->type == ElementType::Line) {
                if (s->pts.size() >= 2) {
                    if (npts.empty()) { npts.push_back(s->pts[0]); npts.push_back(s->pts[1]); }
                    else npts.push_back(s->pts[1]);
                    nflags.push_back(false);
                }
            } else if (s->type == ElementType::Arc) {
                if (!s->pts.empty() && s->radius > 0) {
                    const double a0 = math::deg2rad(s->startAngle);
                    const double a1 = math::deg2rad(s->endAngle);
                    const Vec3 a(s->pts[0].x + s->radius * std::cos(a0),
                                 s->pts[0].y + s->radius * std::sin(a0), 0);
                    const Vec3 b(s->pts[0].x + s->radius * std::cos(a1),
                                 s->pts[0].y + s->radius * std::sin(a1), 0);
                    if (npts.empty()) { npts.push_back(a); npts.push_back(b); }
                    else npts.push_back(b);
                    nflags.push_back(true);
                }
            } else if (s->type == ElementType::Polyline) {
                for (const auto& v : s->pts) npts.push_back(v);
                for (bool a : s->arcFlags) nflags.push_back(a);
            }
        }
        if (npts.size() < 2) { m->log(QStringLiteral("段节点不足"), "warn"); return false; }
        Element el = ElementFactory::createPolyline(npts, nflags, false, m);
        const int nid = m->addElement(el);
        m->setSelected(nid);
        m->log(QString("已合并 %1 段为折线 #%2").arg(ctx.toks.size() - 1).arg(nid));
        return true;
    }

    const Element* ep = m->element(id);
    if (!ep || ep->type != ElementType::Polyline) {
        m->log(QStringLiteral("未找到折线 #") + ctx.toks[1], "warn");
        return false;
    }
    const auto& pts = ep->pts;
    const auto& arcFlags = ep->arcFlags;

    if (cmd == "polyline_add_node") {
        if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: polyline_add_node <id> x,y"), "warn"); return false; }
        Vec3 p; if (!ctx.parsePoint(2, p)) { m->log(QStringLiteral("点解析失败"), "warn"); return false; }
        const int n = (int)pts.size();
        const int nSegs = ep->closed ? n : n - 1;
        int bestSeg = -1; double bestD = 1e100; Vec3 bestProj;
        for (int i = 0; i < nSegs; ++i) {
            const Vec3& a = pts[i];
            const Vec3& b = pts[(i + 1) % n];
            const bool isArc = (i < (int)arcFlags.size()) && arcFlags[i];
            Vec3 proj; double d;
            if (isArc) {
                geo::ArcSeg ar = geo::arcThrough(pts, i, ep->closed);
                if (ar.valid) {
                    const double ang = std::atan2(p.y - ar.center.y, p.x - ar.center.x);
                    proj = Vec3(ar.center.x + ar.radius * std::cos(ang),
                                ar.center.y + ar.radius * std::sin(ang), 0);
                    d = p.distTo(proj);
                } else { proj = math::projPointLine(p, a, b); d = p.distTo(proj); }
            } else {
                proj = math::projPointLine(p, a, b);
                d = p.distTo(proj);
            }
            if (d < bestD) { bestD = d; bestSeg = i; bestProj = proj; }
        }
        if (bestSeg < 0) return false;
        m->updateElement(id, [&](Element& out) {
            out.pts.insert(out.pts.begin() + bestSeg + 1, bestProj);
            std::vector<bool> nf;
            const bool wasArc = (bestSeg < (int)out.arcFlags.size()) && out.arcFlags[bestSeg];
            for (int i = 0; i < bestSeg; ++i) nf.push_back(out.arcFlags[i]);
            nf.push_back(wasArc); nf.push_back(wasArc);
            for (int i = bestSeg + 1; i < (int)out.arcFlags.size(); ++i) nf.push_back(out.arcFlags[i]);
            out.arcFlags = std::move(nf);
        });
        m->log(QString("已在折线 #%1 第 %2 段插入节点").arg(id).arg(bestSeg + 1));
        return true;
    }
    if (cmd == "polyline_append_node") {
        if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: polyline_append_node <id> x,y"), "warn"); return false; }
        Vec3 p; if (!ctx.parsePoint(2, p)) { m->log(QStringLiteral("点解析失败"), "warn"); return false; }
        m->updateElement(id, [&](Element& out) {
            if (p.distTo(out.pts.front()) < p.distTo(out.pts.back()))
                out.pts.insert(out.pts.begin(), p);
            else
                out.pts.push_back(p);
        });
        m->log(QString("已追加折线 #%1 节点").arg(id));
        return true;
    }
    if (cmd == "polyline_delete_node") {
        if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: polyline_delete_node <id> <idx>"), "warn"); return false; }
        bool ok; int idx = ctx.toks[2].toInt(&ok);
        if (!ok || idx < 0 || idx >= (int)pts.size()) { m->log(QStringLiteral("节点索引无效"), "warn"); return false; }
        if ((int)pts.size() <= 2) {
            m->removeElement(id);
            m->log(QString("折线 #%1 节点过少，已删除").arg(id));
            return true;
        }
        m->updateElement(id, [&](Element& out) {
            out.pts.erase(out.pts.begin() + idx);
            std::vector<bool> nf;
            if (idx == 0) {
                for (int k = 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            } else {
                for (int k = 0; k < idx && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
                for (int k = idx + 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            }
            out.arcFlags = std::move(nf);
        });
        m->log(QString("已删除折线 #%1 节点 %2").arg(id).arg(idx));
        return true;
    }
    if (cmd == "polyline_delete_between") {
        if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法: polyline_delete_between <id> <i> <j>"), "warn"); return false; }
        int a = ctx.toks[2].toInt(), b = ctx.toks[3].toInt();
        if (a > b) std::swap(a, b);
        if (b - a <= 1) { m->log(QStringLiteral("两节点相邻无需删除"), "warn"); return false; }
        m->updateElement(id, [&](Element& out) {
            std::vector<Vec3> nv;
            for (int i = 0; i <= a; ++i) nv.push_back(out.pts[i]);
            for (int i = b; i < (int)out.pts.size(); ++i) nv.push_back(out.pts[i]);
            out.pts = std::move(nv);
            std::vector<bool> nf;
            for (int k = 0; k <= a && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            for (int k = b; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            out.arcFlags = std::move(nf);
        });
        m->log(QString("已删除折线 #%1 节点 %2..%3 之间").arg(id).arg(a).arg(b));
        return true;
    }
    if (cmd == "polyline_trim_segments") {
        if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: polyline_trim_segments <id> <segIdx>"), "warn"); return false; }
        bool ok; int seg = ctx.toks[2].toInt(&ok);
        if (!ok || seg < 0 || seg >= (int)arcFlags.size()) { m->log(QStringLiteral("段索引无效"), "warn"); return false; }
        if ((int)pts.size() <= 2) { m->log(QStringLiteral("修剪后节点不足"), "warn"); return false; }
        const int n = (int)pts.size();
        const int delNode = (seg + 1) % n;
        m->updateElement(id, [&](Element& out) {
            out.pts.erase(out.pts.begin() + delNode);
            std::vector<bool> nf;
            for (int k = 0; k < seg && k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            for (int k = seg + 1; k < (int)out.arcFlags.size(); ++k) nf.push_back(out.arcFlags[k]);
            out.arcFlags = std::move(nf);
        });
        m->log(QString("已修剪折线 #%1 第 %2 段").arg(id).arg(seg + 1));
        return true;
    }
    if (cmd == "polyline_equidistant") {
        if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法: polyline_equidistant <id> <d>"), "warn"); return false; }
        bool ok; const double d = ctx.toks[2].toDouble(&ok);
        if (!ok) { m->log(QStringLiteral("距离无效"), "warn"); return false; }
        std::vector<Vec3> np(pts.size());
        for (int i = 0; i < (int)pts.size(); ++i) np[i] = pts[i];
        const int n = (int)pts.size();
        for (int i = 0; i < n; ++i) {
            const Vec3& prev = pts[(i - 1 + n) % n];
            const Vec3& cur  = pts[i];
            const Vec3& next = pts[(i + 1) % n];
            const double dx1 = cur.x - prev.x, dy1 = cur.y - prev.y;
            const double dx2 = next.x - cur.x, dy2 = next.y - cur.y;
            const double l1 = std::hypot(dx1, dy1), l2 = std::hypot(dx2, dy2);
            if (l1 < 1e-9 || l2 < 1e-9) continue;
            const double nx1 = -dy1 / l1, ny1 = dx1 / l1;
            const double nx2 = -dy2 / l2, ny2 = dx2 / l2;
            double nx = (nx1 + nx2) * 0.5, ny = (ny1 + ny2) * 0.5;
            const double ln = std::hypot(nx, ny);
            if (ln < 1e-9) continue;
            nx /= ln; ny /= ln;
            np[i] = Vec3(cur.x + nx * d, cur.y + ny * d, cur.z);
        }
        Element out = ElementFactory::createPolyline(np, arcFlags, ep->closed, m);
        out.name = ep->name + QStringLiteral("(等距)");
        const int nid = m->addElement(out);
        m->setSelected(nid);
        m->log(QString("已创建等距折线 #%1 (d=%2)").arg(nid).arg(d));
        return true;
    }
    if (cmd == "polyline_arcs_to_chords") {
        int nArc = 0; for (bool a : arcFlags) if (a) ++nArc;
        m->updateElement(id, [&](Element& out) {
            std::fill(out.arcFlags.begin(), out.arcFlags.end(), false);
        });
        m->log(QString("已将折线 #%1 的 %2 段弧转直线").arg(id).arg(nArc));
        return true;
    }
    if (cmd == "polyline_change_segment_type") {
        if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法: polyline_change_segment_type <id> <i> <j>"), "warn"); return false; }
        bool o1, o2; int a = ctx.toks[2].toInt(&o1), b = ctx.toks[3].toInt(&o2);
        if (!o1 || !o2) { m->log(QStringLiteral("索引无效"), "warn"); return false; }
        if (a > b) std::swap(a, b);
        m->updateElement(id, [&](Element& out) {
            for (int k = a; k <= b; ++k)
                if (k < (int)out.arcFlags.size()) out.arcFlags[k] = !out.arcFlags[k];
        });
        m->log(QString("已切换折线 #%1 第 %2..%3 段类型").arg(id).arg(a + 1).arg(b + 1));
        return true;
    }
    m->log(QStringLiteral("未实现的折线子命令: ") + cmd, "warn");
    return false;
}

// ============================================================
// 编辑命令
// ============================================================

bool DeleteCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) { m->log("用法: delete <id>", "warn"); return false; }
    bool ok; int id = ctx.toks[1].toInt(&ok);
    if (!ok) { m->log("id 无效", "warn"); return false; }
    if (!m->removeElement(id)) { m->log("未找到图元 #" + QString::number(id), "warn"); return false; }
    m->log("已删除图元 #" + QString::number(id));
    return true;
}

bool TrimCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) {
        m->log("用法: trim <目标id> <点击点x,y> [边界id ...]（边界省略=全场景为界）", "warn");
        return false;
    }
    const int tid = ctx.toks[1].toInt();
    const Element* tgt = m->element(tid);
    if (!tgt) { m->log("trim: 找不到图元 #" + ctx.toks[1], "warn"); return false; }
    Vec3 click;
    if (!ctx.parsePoint(2, click)) { m->log("trim: 点击点解析失败", "warn"); return false; }

    std::vector<Element> cutters;
    if (ctx.toks.size() >= 4) {
        for (int i = 3; i < ctx.toks.size(); ++i)
            if (const Element* b = m->element(ctx.toks[i].toInt())) cutters.push_back(*b);
    } else {
        for (const auto& el : m->elements())
            if (el.id != tid && el.visible &&
                el.type != ElementType::Constraint && el.type != ElementType::Dimension)
                cutters.push_back(el);
    }

    std::vector<Element> parts;
    QString msg;
    if (!trim::trimElement(*tgt, cutters, click, parts, msg)) {
        m->log("trim: " + msg, "warn");
        return false;
    }
    const QString tn = tgt->typeName();
    if (parts.empty()) {
        m->removeElement(tid);
        m->log(QString("已剪切%1 #%2：整体位于边界之间，已移除").arg(tn).arg(tid));
    } else {
        const auto ids = m->replaceElement(tid, parts, QStringLiteral("剪切") + tn);
        QStringList ss;
        for (int i : ids) ss << ("#" + QString::number(i));
        m->log(QString("已剪切%1 #%2 → 保留 %3 段：%4")
                   .arg(tn).arg(tid).arg(ids.size()).arg(ss.join(", ")));
    }
    return true;
}

bool ListCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    m->log("图元总数: " + QString::number(m->elementCount()));
    for (const auto& e : m->elements())
        m->log(QString("#%1 %2  %3").arg(e.id).arg(e.typeName(), e.sizeDesc()));
    return true;
}

// ============================================================
// 修改菜单（Modify）单一入口 ModifyCommand
// ------------------------------------------------------------
// 语法 / 分派：
//   move        <dx> <dy>                    移动当前选中的图元
//   copy        <dx> <dy>                    复制粘贴式（保留原图元）
//   duplicate                                 复制当前选中的图元（默认偏置 10, 0）
//   rotate      <cx> <cy> <angleDeg>         绕 (cx, cy) 旋转选中
//   scale       <cx> <cy> <factor>           绕 (cx, cy) 缩放选中（>0）
//   mirror      <axis: h|v|line <x1> <y1> <x2> <y2>>
//   revert                                   反向（直线交换端点 / 圆弧交换起止角 / 折线反转）
//   lengthen    <side: 0|1> <deltaMm>        延长 / 缩短直线端点
//   offset      <distMm>                     直线 / 圆 / 圆弧 偏移
//   break_at    <id> <px,py>                 在投影点打断一条线
//   divide      <id> <n>                     把直线 n 等分
//   fillet      <id1> <id2> <radiusMm>       圆角（radius=0 即倒角）
//   explode                                   分解（矩形/多边形/折线/样条 → 多段线）
//   align       <kind: left|centerx|right|topy|centery|bottom>
//   stretch     <basex,basey> <tox,toy>       拉伸 basePoint 端点到 toPoint
// ============================================================
bool ModifyCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.cmd;

    // 把当前选中集拍成 src 列表
    auto selectedElems = [&]() -> std::vector<Element> {
        std::vector<Element> out;
        for (int id : m->selection()) {
            if (const Element* e = m->element(id)) out.push_back(*e);
        }
        return out;
    };

    if (cmd == "move" || cmd == "move_copy") {
        if (ctx.toks.size() < 3) { m->log("用法: move <dx> <dy>", "warn"); return false; }
        bool copy = (cmd == "move_copy");
        bool ok1, ok2;
        double dx = ctx.toks[1].toDouble(&ok1), dy = ctx.toks[2].toDouble(&ok2);
        if (!ok1 || !ok2) { m->log("位移参数解析失败", "warn"); return false; }
        if (m->selection().empty()) { m->log("无可移动图元（请先选中）", "warn"); return false; }
        if (copy) {
            // move_copy：复制（新增）一份偏置后的副本，原图元不动
            auto src = selectedElems();
            auto changes = modify::makeMoveCopy(src, Vec3(dx, dy), /*copy=*/true);
            m->beginEdit(QStringLiteral("修改-复制移动"));
            auto added = modify::applyChanges(m, changes, QString());
            m->log(QString("已复制移动 %1 个图元（Δ=(%2,%3)），新增 %4 个")
                       .arg(src.size()).arg(dx, 0, 'f', 2).arg(dy, 0, 'f', 2)
                       .arg(added.size()));
            return true;
        }
        // 普通 move：整组平移（单一入口，与菜单「移动/复制」语义一致）
        const int n = m->selection().size();
        m->translateSelection(Vec3(dx, dy), QStringLiteral("修改-移动"));
        m->log(QString("已移动 %1 个图元（Δ=(%2,%3)）")
                   .arg(n).arg(dx, 0, 'f', 2).arg(dy, 0, 'f', 2));
        return true;
    }
    if (cmd == "duplicate" || cmd == "copy_dup") {
        // duplicate 默认偏置 (10, 0)
        const double dx = ctx.toks.size() >= 2 ? ctx.toks[1].toDouble() : 10.0;
        const double dy = ctx.toks.size() >= 3 ? ctx.toks[2].toDouble() : 0.0;
        auto src = selectedElems();
        if (src.empty()) { m->log("无可复制图元（请先选中）", "warn"); return false; }
        auto changes = modify::makeDuplicate(src, dx, dy);
        m->beginEdit(QStringLiteral("修改-复制"));
        auto added = modify::applyChanges(m, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        m->log(QString("已复制 %1 个图元（偏置=(%2,%3)）→ %4")
                   .arg(src.size()).arg(dx, 0, 'f', 2).arg(dy, 0, 'f', 2).arg(ids.join(", ")));
        return true;
    }
    if (cmd == "rotate") {
        if (ctx.toks.size() < 2) { m->log("用法: rotate <angleDeg> [cx cy]", "warn"); return false; }
        double a = ctx.toks[1].toDouble();
        // 默认绕"选中图元的几何质心"旋转；提供 cx,cy 时绕指定点旋转
        auto src = selectedElems();
        if (src.empty()) { m->log("无可旋转图元", "warn"); return false; }
        Vec3 c;
        if (ctx.toks.size() >= 4 && ctx.parsePoint(2, c)) {
            // 显式中心
        } else {
            // 默认选区质心
            int cnt = 0;
            for (const auto& s : src) {
                Vec3 sc = geo::elementCentroid(s);
                c = Vec3(c.x + sc.x, c.y + sc.y, 0);
                ++cnt;
            }
            if (cnt > 0) c = Vec3(c.x / cnt, c.y / cnt, 0);
        }
        auto changes = modify::makeRotate(src, c, a);
        m->beginEdit(QStringLiteral("修改-旋转"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已旋转 %1 个图元（中心=(%2,%3)，角度=%4°）")
                   .arg(src.size()).arg(c.x, 0, 'f', 2).arg(c.y, 0, 'f', 2).arg(a, 0, 'f', 2));
        return true;
    }
    if (cmd == "scale") {
        if (ctx.toks.size() < 2) { m->log("用法: scale <factor> [cx cy]", "warn"); return false; }
        double k = ctx.toks[1].toDouble();
        if (k <= 0) { m->log("缩放因子必须 > 0", "warn"); return false; }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可缩放图元", "warn"); return false; }
        Vec3 c;
        if (ctx.toks.size() >= 4 && ctx.parsePoint(2, c)) {
            // 显式中心
        } else {
            int cnt = 0;
            for (const auto& s : src) {
                Vec3 sc = geo::elementCentroid(s);
                c = Vec3(c.x + sc.x, c.y + sc.y, 0);
                ++cnt;
            }
            if (cnt > 0) c = Vec3(c.x / cnt, c.y / cnt, 0);
        }
        auto changes = modify::makeScale(src, c, k);
        m->beginEdit(QStringLiteral("修改-缩放"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已缩放 %1 个图元（中心=(%2,%3)，因子=%4）")
                   .arg(src.size()).arg(c.x, 0, 'f', 2).arg(c.y, 0, 'f', 2).arg(k, 0, 'f', 3));
        return true;
    }
    if (cmd == "mirror") {
        if (ctx.toks.size() < 2) { m->log("用法: mirror h|v|line <x1> <y1> <x2> <y2>", "warn"); return false; }
        const QString kind = ctx.toks[1].toLower();
        int axis = 0;
        Vec3 p1, p2(1, 0, 0);
        if (kind == "h" || kind == "horizontal") axis = 0;
        else if (kind == "v" || kind == "vertical") axis = 1;
        else if (kind == "line") {
            if (ctx.toks.size() < 6) { m->log("用法: mirror line <x1> <y1> <x2> <y2>", "warn"); return false; }
            if (!ctx.parsePoint(2, p1) || !ctx.parsePoint(4, p2)) {
                m->log("轴点解析失败", "warn"); return false;
            }
            axis = 2;
        } else { m->log("未知镜像轴: " + kind, "warn"); return false; }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可镜像图元", "warn"); return false; }
        auto changes = modify::makeMirror(src, axis, p1, p2);
        m->beginEdit(QStringLiteral("修改-镜像"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已镜像 %1 个图元（轴=%2）").arg(src.size()).arg(kind));
        return true;
    }
    if (cmd == "revert" || cmd == "revert_direction") {
        auto src = selectedElems();
        if (src.empty()) { m->log("无可反向图元", "warn"); return false; }
        auto changes = modify::makeRevertDirection(src);
        m->beginEdit(QStringLiteral("修改-反向"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已反向 %1 个图元").arg(src.size()));
        return true;
    }
    if (cmd == "lengthen") {
        if (ctx.toks.size() < 3) { m->log("用法: lengthen <side:0|1> <deltaMm>", "warn"); return false; }
        int side = ctx.toks[1].toInt();
        double d = ctx.toks[2].toDouble();
        if (side < 0 || side > 1) { m->log("side 必须为 0（起点）或 1（终点）", "warn"); return false; }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可延长图元", "warn"); return false; }
        auto changes = modify::makeLengthen(src, side, d);
        m->beginEdit(QStringLiteral("修改-延长"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已延长 %1 个图元（side=%2，Δ=%3 mm）").arg(src.size()).arg(side).arg(d, 0, 'f', 2));
        return true;
    }
    if (cmd == "offset") {
        if (ctx.toks.size() < 2) { m->log("用法: offset <distMm>", "warn"); return false; }
        double d = ctx.toks[1].toDouble();
        auto src = selectedElems();
        if (src.empty()) { m->log("无可偏移图元", "warn"); return false; }
        auto changes = modify::makeOffset(src, d);
        m->beginEdit(QStringLiteral("修改-偏移"));
        auto applied = modify::applyChanges(m, changes, QString());
        m->log(QString("已偏移 %1 个图元（Δ=%2 mm）").arg(applied.size()).arg(d, 0, 'f', 2));
        return true;
    }
    if (cmd == "break_at") {
        if (ctx.toks.size() < 3) { m->log("用法: break_at <id> <px,py>", "warn"); return false; }
        bool ok; int id = ctx.toks[1].toInt(&ok);
        if (!ok) { m->log("id 无效", "warn"); return false; }
        Vec3 p;
        if (!ctx.parsePoint(2, p)) { m->log("点解析失败", "warn"); return false; }
        // 单点打断：把 id 加入 src，然后 makeBreakDivide(src, id, p, 0)
        std::vector<Element> src;
        if (const Element* e = m->element(id)) src.push_back(*e);
        else { m->log("图元不存在", "warn"); return false; }
        auto changes = modify::makeBreakDivide(src, id, p, 0);
        if (changes.empty()) { m->log("打断失败（点不在线段内或为端点）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-打断"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已打断图元 #%1 于 (%2,%3)").arg(id).arg(p.x, 0, 'f', 2).arg(p.y, 0, 'f', 2));
        return true;
    }
    if (cmd == "divide") {
        if (ctx.toks.size() < 3) { m->log("用法: divide <id> <n>", "warn"); return false; }
        bool ok; int id = ctx.toks[1].toInt(&ok), n = ctx.toks[2].toInt(&ok);
        if (!ok || n < 2) { m->log("id 或段数无效", "warn"); return false; }
        std::vector<Element> src;
        if (const Element* e = m->element(id)) src.push_back(*e);
        else { m->log("图元不存在", "warn"); return false; }
        // 等分需要给个虚拟的切点；改 makeBreakDivide 走"等分"分支：传 n，cut 设为任意值
        auto changes = modify::makeBreakDivide(src, id, Vec3(0, 0, 0), n);
        if (changes.empty()) { m->log("等分失败", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-等分"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已把图元 #%1 等分为 %2 段").arg(id).arg(n));
        return true;
    }
    if (cmd == "fillet") {
        if (ctx.toks.size() < 4) { m->log("用法: fillet <id1> <id2> <radiusMm>（radius=0 即倒角）", "warn"); return false; }
        bool ok1, ok2; int id1 = ctx.toks[1].toInt(&ok1), id2 = ctx.toks[2].toInt(&ok2);
        if (!ok1 || !ok2) { m->log("id 解析失败", "warn"); return false; }
        double r = ctx.toks[3].toDouble();
        if (r < 0) { m->log("radius 必须 ≥ 0", "warn"); return false; }
        std::vector<Element> pool = m->elements();
        auto changes = modify::makeFillet(id1, id2, r, pool);
        if (changes.empty()) { m->log("圆角失败（两线平行或不相交）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-圆角"));
        auto added = modify::applyChanges(m, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        m->log(QString("已对 #%1 与 #%2 做%3（r=%4），新增 %5：%6")
                   .arg(id1).arg(id2).arg(r > 0 ? "圆角" : "倒角")
                   .arg(r, 0, 'f', 2).arg(added.size()).arg(ids.join(", ")));
        return true;
    }
    if (cmd == "explode") {
        auto src = selectedElems();
        if (src.empty()) { m->log("无可分解图元", "warn"); return false; }
        auto changes = modify::makeExplode(src);
        if (changes.empty()) { m->log("无可分解对象", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-分解"));
        auto added = modify::applyChanges(m, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        m->log(QString("已分解 %1 个图元 → 新增 %2 段：%3")
                   .arg(src.size()).arg(added.size()).arg(ids.join(", ")));
        return true;
    }
    if (cmd == "align") {
        if (ctx.toks.size() < 2) {
            m->log("用法: align left|centerx|right|topy|centery|bottom", "warn");
            return false;
        }
        int kind = -1;
        const QString k = ctx.toks[1].toLower();
        if      (k == "left" || k == "l")     kind = 0;
        else if (k == "centerx" || k == "cx") kind = 1;
        else if (k == "right" || k == "r")    kind = 2;
        else if (k == "topy" || k == "t")     kind = 3;
        else if (k == "centery" || k == "cy") kind = 4;
        else if (k == "bottom" || k == "b")   kind = 5;
        else { m->log("未知对齐方式: " + k, "warn"); return false; }
        // 取第一个选区元素作为 ref，其余为 src
        std::vector<Element> src;
        const Element* refE = nullptr;
        int idx = 0;
        for (int id : m->selection()) {
            const Element* e = m->element(id);
            if (!e) continue;
            if (!refE) refE = e;
            else { src.push_back(*e); ++idx; }
        }
        if (!refE || src.empty()) { m->log("至少需要 2 个选中图元（1 参考 + 1+ 待对齐）", "warn"); return false; }
        auto changes = modify::makeAlign(src, *refE, kind);
        m->beginEdit(QStringLiteral("修改-对齐"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已对齐 %1 个图元（方式=%2）").arg(src.size()).arg(k));
        return true;
    }
    if (cmd == "stretch") {
        if (ctx.toks.size() < 3) { m->log("用法: stretch <basex,basey> <tox,toy>", "warn"); return false; }
        Vec3 base, to;
        if (!ctx.parsePoint(1, base) || !ctx.parsePoint(2, to)) { m->log("点解析失败", "warn"); return false; }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可拉伸图元", "warn"); return false; }
        auto changes = modify::makeStretch(src, base, to);
        if (changes.empty()) { m->log("未选中任何端点（basePt 必须命中图元端点容差 1e-3 mm）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-拉伸"));
        modify::applyChanges(m, changes, QString());
        m->log(QString("已拉伸 %1 个端点").arg(changes.size()));
        return true;
    }
    m->log("未知 modify 子命令: " + cmd, "warn");
    return false;
}

// ============================================================
// 视图命令
// ============================================================

bool PlaneCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) { m->log("用法: plane xy|yz|xz", "warn"); return false; }
    QString p = ctx.toks[1].toLower();
    if (p == "xy") m->setPlane(Plane::XY);
    else if (p == "yz") m->setPlane(Plane::YZ);
    else if (p == "xz") m->setPlane(Plane::XZ);
    else { m->log("未知平面: " + p, "warn"); return false; }
    m->log("已切换到 " + QString(planeName(m->plane())) + " 工作平面");
    return true;
}

bool GridCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) {
        m->setShowGrid(!m->showGrid());
        m->log(QString("栅格已") + (m->showGrid() ? "显示" : "隐藏"));
        return true;
    }
    QString v = ctx.toks[1].toLower();
    if (v == "on" || v == "1" || v == "true")       m->setShowGrid(true);
    else if (v == "off" || v == "0" || v == "false") m->setShowGrid(false);
    else { m->log("用法: grid [on|off]（省略参数为切换）", "warn"); return false; }
    m->log(QString("栅格已") + (m->showGrid() ? "显示" : "隐藏"));
    return true;
}

bool ZoomCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) { m->log("用法: zoom <倍率>", "warn"); return false; }
    bool ok; double z = ctx.toks[1].toDouble(&ok);
    if (!ok || z <= 0) { m->log("倍率无效", "warn"); return false; }
    m->setViewScale(z); m->log("视图比例: " + QString::number(z, 'f', 2));
    return true;
}

// ============================================================
// 样式命令
// ============================================================

bool WidthCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) {
        m->log(QString("当前线宽: %1（用法: width 1|2|3|5）")
                   .arg(QString::fromUtf8(lineWidthName(m->defaultWidth()))));
        return true;
    }
    bool ok; double px = ctx.toks[1].toDouble(&ok);
    if (!ok || px <= 0 || px > 20) { m->log("线宽无效（取 1 / 2 / 3 / 5）", "warn"); return false; }
    m->setDefaultWidth(px);
    if (m->selectedId() >= 0 && m->styleSelected(nullptr, &px))
        m->log(QString("选中图元线宽 -> %1").arg(QString::fromUtf8(lineWidthName(px))));
    else
        m->log(QString("新图元线宽 -> %1").arg(QString::fromUtf8(lineWidthName(px))));
    return true;
}

bool ColorCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) {
        m->log(QString("当前颜色: %1（用法: color #RRGGBB | red | blue ...）")
                   .arg(m->defaultColor().name().toUpper()));
        return true;
    }
    QColor c(ctx.toks[1]);
    if (!c.isValid()) { m->log("颜色无效: " + ctx.toks[1], "warn"); return false; }
    m->setDefaultColor(c);
    if (m->selectedId() >= 0 && m->styleSelected(&c, nullptr))
        m->log(QString("选中图元颜色 -> %1").arg(c.name().toUpper()));
    else
        m->log(QString("新图元颜色 -> %1").arg(c.name().toUpper()));
    return true;
}

bool ChainCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) {
        m->setChainLine(!m->chainLine());
    } else {
        QString v = ctx.toks[1].toLower();
        if (v == "on" || v == "1" || v == "true")        m->setChainLine(true);
        else if (v == "off" || v == "0" || v == "false") m->setChainLine(false);
        else { m->log("用法: chain [on|off]（省略参数为切换）", "warn"); return false; }
    }
    m->log(QString("连续直线模式：%1").arg(m->chainLine() ? "开（右键结束）" : "关"));
    return true;
}

// ============================================================
// 标注 / 约束命令
// ============================================================

bool DimCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    static const QMap<QString, DimensionType> dmap = {
        {"dist", DimensionType::Distance}, {"distance", DimensionType::Distance},
        {"linear", DimensionType::Distance},
        {"aligned", DimensionType::Aligned}, {"align", DimensionType::Aligned},
        {"h", DimensionType::Horizontal}, {"horizontal", DimensionType::Horizontal},
        {"v", DimensionType::Vertical}, {"vertical", DimensionType::Vertical},
        {"angle", DimensionType::Angle}, {"ang", DimensionType::Angle},
        {"radius", DimensionType::Radius}, {"r", DimensionType::Radius},
        {"dia", DimensionType::Diameter}, {"diameter", DimensionType::Diameter},
        // 新增 5 种标注类型（对应 DimStyle::build 的基线/连续/弧长/引线/坐标生成器）
        {"baseline", DimensionType::Baseline}, {"base", DimensionType::Baseline},
        {"continue", DimensionType::Continue}, {"cont", DimensionType::Continue},
        {"arc", DimensionType::Arc}, {"arclen", DimensionType::Arc},
        {"leader", DimensionType::Leader}, {"note", DimensionType::Leader},
        {"ordinate", DimensionType::Ordinate}, {"ord", DimensionType::Ordinate}};
    if (ctx.toks.size() < 2) {
        m->log("用法: dim linear|aligned|h|v x1,y1 x2,y2 [尺寸线位置 px,py]", "warn");
        m->log("      dim angle 顶点 边1点 边2点 | dim radius|dia <圆id> [引出角度]", "warn");
        m->log("      dim baseline|continue x1,y1 x2,y2 | dim arc <圆id> [起角 终角]", "warn");
        m->log("      dim leader 起点 折点 [文本点] | dim ordinate 被测点 基准点 [0/90]", "warn");
        return false;
    }
    const QString t = ctx.toks[1].toLower();
    if (!dmap.contains(t)) { m->log("未知标注类型: " + t, "warn"); return false; }
    const DimensionType dt = dmap.value(t);
    Element e = ElementFactory::create(ElementType::Dimension, QString::fromUtf8(dimensionTypeName(dt)), m);
    e.dtype = dt;
    e.standard = m->dimensionStandard();   // 新建标注跟随当前标准（默认 GB/T 4458）

    if (dt == DimensionType::Radius || dt == DimensionType::Diameter) {
        if (ctx.toks.size() < 3) { m->log("用法: dim radius|dia <圆或圆弧 id> [引出角度]", "warn"); return false; }
        const Element* c = m->element(ctx.toks[2].toInt());
        if (!c || (c->type != ElementType::Circle && c->type != ElementType::Arc)) {
            m->log("dim: #" + ctx.toks[2] + " 不是圆或圆弧", "warn"); return false;
        }
        const double ang = (ctx.toks.size() >= 4) ? ctx.toks[3].toDouble() : 45.0;
        e.radius = c->radius;
        e.refA = c->id;
        e.pts = {c->pts[0], geo::endpointFrom(c->pts[0], c->radius, ang)};
    } else if (dt == DimensionType::Angle) {
        Vec3 v, p1, p2;
        if (ctx.toks.size() < 5 || !ctx.parsePoint(2, v) ||
            !ctx.parsePoint(3, p1) || !ctx.parsePoint(4, p2)) {
            m->log("用法: dim angle 顶点x,y 边1点x,y 边2点x,y", "warn"); return false;
        }
        e.pts = {v, p1, p2};
    } else if (dt == DimensionType::Arc) {
        // 弧长标注：引用一个圆或圆弧；圆没有起止角时用默认 0°~90° 扫掠
        if (ctx.toks.size() < 3) { m->log("用法: dim arc <圆或圆弧 id> [起始角 终止角]", "warn"); return false; }
        const Element* c = m->element(ctx.toks[2].toInt());
        if (!c || (c->type != ElementType::Circle && c->type != ElementType::Arc)) {
            m->log("dim: #" + ctx.toks[2] + " 不是圆或圆弧", "warn"); return false;
        }
        e.radius = c->radius;
        e.refA = c->id;
        e.pts = {c->pts[0]};
        if (c->type == ElementType::Arc) { e.startAngle = c->startAngle; e.endAngle = c->endAngle; }
        else { e.startAngle = 0.0; e.endAngle = 90.0; }
        if (ctx.toks.size() >= 5) {
            bool ok1 = false, ok2 = false;
            const double a0 = ctx.toks[3].toDouble(&ok1), a1 = ctx.toks[4].toDouble(&ok2);
            if (ok1 && ok2) { e.startAngle = a0; e.endAngle = a1; }
            else m->log("dim arc: 起始角/终止角解析失败，使用默认 0°~90°", "warn");
        }
    } else if (dt == DimensionType::Leader || dt == DimensionType::Ordinate) {
        // 引线标注：2~3 点（起点 + 折点 [+ 文本点]）；坐标标注：2 点（被测点 + 基准点）
        if (dt == DimensionType::Leader) {
            if (ctx.toks.size() < 4) { m->log("用法: dim leader 起点x,y 折点x,y [文本点x,y]", "warn"); return false; }
            Vec3 p0, p1, p2;
            if (!ctx.parsePoint(2, p0) || !ctx.parsePoint(3, p1)) {
                m->log("dim leader: 点解析失败", "warn"); return false;
            }
            e.pts = {p0, p1};
            if (ctx.toks.size() >= 5 && ctx.parsePoint(4, p2)) e.pts.push_back(p2);
        } else {
            if (ctx.toks.size() < 4) { m->log("用法: dim ordinate 被测点x,y 基准点x,y [旋转角]", "warn"); return false; }
            Vec3 p0, p1;
            if (!ctx.parsePoint(2, p0) || !ctx.parsePoint(3, p1)) {
                m->log("dim ordinate: 点解析失败", "warn"); return false;
            }
            e.pts = {p0, p1};
            if (ctx.toks.size() >= 5) {
                bool ok = false; const double rot = ctx.toks[4].toDouble(&ok);
                if (ok) e.rotation = rot;   // 0/180→X 坐标，90/270→Y 坐标
                else m->log("dim ordinate: 旋转角解析失败，默认 X 坐标", "warn");
            }
        }
    } else {
        Vec3 a, b, pos;
        if (ctx.toks.size() < 4 || !ctx.parsePoint(2, a) || !ctx.parsePoint(3, b)) {
            m->log("用法: dim " + t + " x1,y1 x2,y2 [尺寸线位置 px,py]", "warn"); return false;
        }
        e.pts = {a, b};
        if (ctx.toks.size() >= 5 && CommandContext::tryParsePoint(ctx.toks[4], pos)) e.pts.push_back(pos);
    }

    const double mv = geo::measureDimension(e);
    // 引线标注按设计无数值测量（measured=0），不应以"测量值为 0"为由拒绝创建
    if (dt != DimensionType::Leader && mv < 1e-9) { m->log("dim: 测量值为 0，标注未创建", "warn"); return false; }
    const int id = m->addElement(e);
    m->log(QString("已创建%1 #%2 = %3%4")
               .arg(QString::fromUtf8(dimensionTypeName(dt)))
               .arg(id).arg(mv, 0, 'f', 2)
               .arg(dt == DimensionType::Angle ? QStringLiteral("°") : QStringLiteral(" mm")));
    return true;
}

bool DimSmartCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const int id = m->smartDimension();
    if (id < 0) {
        m->log("dims: 请先选中单条直线(→对齐标注)或单个圆/圆弧(→直径标注)", "warn");
        return false;
    }
    return true;
}

bool ConstraintCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) { m->log("用法: constraint <type> <id1> [id2]", "warn"); return false; }
    static const QMap<QString, ConstraintType> cmap = {
        {"coincident", ConstraintType::Coincident},
        {"parallel", ConstraintType::Parallel},
        {"perpendicular", ConstraintType::Perpendicular},
        {"equal", ConstraintType::Equal},
        {"tangent", ConstraintType::Tangent},
        {"horizontal", ConstraintType::Horizontal},
        {"vertical", ConstraintType::Vertical},
        {"fixed", ConstraintType::Fixed}};
    QString t = ctx.toks[1].toLower();
    if (!cmap.contains(t)) { m->log("未知约束类型: " + t, "warn"); return false; }
    Element e = ElementFactory::create(ElementType::Constraint, QString("约束-") + constraintTypeName(cmap[t]), m);
    e.ctype = cmap[t];
    if (ctx.toks.size() >= 3) e.refA = ctx.toks[2].toInt();
    if (ctx.toks.size() >= 4) e.refB = ctx.toks[3].toInt();
    int id = m->addElement(e);
    m->log("已添加约束 #" + QString::number(id) + "（求解器后续接入）");
    return true;
}

// ============================================================
// 选择命令（对应选择菜单 / FreeCAD Sketcher 选择命令）
// ============================================================
bool SelectCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.cmd;
    if (cmd == "select_all") { m->selectAll(); return true; }
    if (cmd == "select_none") { m->clearSelection(); return true; }
    if (cmd == "invert") { m->invertSelection(); return true; }

    auto parseId = [&](int& out) -> bool {
        if (ctx.toks.size() < 2) { m->log("用法: " + cmd + " <图元id>", "warn"); return false; }
        bool ok = false; int id = ctx.toks[1].toInt(&ok);
        if (!ok) { m->log("无效的图元 id: " + ctx.toks[1], "warn"); return false; }
        out = id; return true;
    };

    if (cmd == "select_entity") {
        int id; if (!parseId(id)) return false; m->selectEntity(id); return true;
    }
    if (cmd == "deselect_entity") {
        int id; if (!parseId(id)) return false; m->deselectEntity(id); return true;
    }
    if (cmd == "select_contour") {
        int id; if (!parseId(id)) return false; m->selectContour(id); return true;
    }
    if (cmd == "toggle_contour") {
        int id; if (!parseId(id)) return false; m->toggleContour(id); return true;
    }
    if (cmd == "select_intersected") {
        int id; if (!parseId(id)) return false; m->selectIntersected(id); return true;
    }
    if (cmd == "deselect_intersected") {
        int id; if (!parseId(id)) return false; m->deselectIntersected(id); return true;
    }
    if (cmd == "select_layer") {
        if (ctx.toks.size() < 2) { m->log("用法: select_layer <图层名>", "warn"); return false; }
        m->selectLayer(ctx.toks[1]); return true;
    }
    if (cmd == "deselect_layer") {
        if (ctx.toks.size() < 2) { m->log("用法: deselect_layer <图层名>", "warn"); return false; }
        m->deselectLayer(ctx.toks[1]); return true;
    }
    if (cmd == "toggle_layer") {
        int id; if (!parseId(id)) return false; m->toggleLayerOf(id); return true;
    }
    m->log("未知选择命令: " + cmd, "warn");
    return false;
}

// ============================================================
// 图层管理命令
// ============================================================
namespace {
LineStyle parseLineStyleArg(const QString& s) {
    const QString t = s.toLower();
    if (t == "solid" || t == "实线" || t == "0") return LineStyle::Solid;
    if (t == "dashed" || t == "虚线" || t == "1") return LineStyle::Dashed;
    if (t == "dotted" || t == "点线" || t == "2") return LineStyle::Dotted;
    if (t == "dashdot" || t == "点划线" || t == "3") return LineStyle::DashDot;
    if (t == "dashdotdot" || t == "双点划线" || t == "4") return LineStyle::DashDotDot;
    return LineStyle::Solid;
}
} // namespace

bool LayerCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QStringList& t = ctx.toks;
    if (t.size() < 2) {
        m->log("用法: layer <add|delete|rename|visible|lock|set|assign|apply|list> ...", "warn");
        return false;
    }
    const QString sub = t[1].toLower();

    if (sub == "list") {
        QString info = QStringLiteral("图层表(%1): ").arg(m->layerCount());
        for (int i = 0; i < m->layerCount(); ++i) {
            const Layer& l = m->layerAt(i);
            info += QStringLiteral("[%1 %2 %3] ").arg(
                l.name,
                l.visible ? QStringLiteral("显示") : QStringLiteral("隐藏"),
                QString::fromUtf8(lineStyleName(l.lineStyle)));
        }
        m->log(info);
        return true;
    }
    if (sub == "add") {
        if (t.size() < 3) { m->log("用法: layer add <名称> [颜色] [线宽] [线型]", "warn"); return false; }
        QColor c; double w = 1.0; LineStyle ls = LineStyle::Solid;
        if (t.size() >= 4) { c = QColor(t[3]); if (!c.isValid()) c = QColor(); }
        if (t.size() >= 5) { bool ok; double v = t[4].toDouble(&ok); if (ok) w = v; }
        if (t.size() >= 6) ls = parseLineStyleArg(t[5]);
        return m->addLayer(t[2], c, w, ls);
    }
    if (sub == "delete") {
        if (t.size() < 3) { m->log("用法: layer delete <名称>", "warn"); return false; }
        return m->removeLayer(t[2]);
    }
    if (sub == "rename") {
        if (t.size() < 4) { m->log("用法: layer rename <旧名> <新名>", "warn"); return false; }
        return m->renameLayer(t[2], t[3]);
    }
    if (sub == "visible") {
        if (t.size() < 3) { m->log("用法: layer visible <名称> [on|off]", "warn"); return false; }
        bool on = (t.size() < 4) || (t[3].toLower() != "off" && t[3].toLower() != "0");
        return m->setLayerVisible(t[2], on);
    }
    if (sub == "lock") {
        if (t.size() < 3) { m->log("用法: layer lock <名称> [on|off]", "warn"); return false; }
        bool on = (t.size() < 4) || (t[3].toLower() != "off" && t[3].toLower() != "0");
        return m->setLayerLocked(t[2], on);
    }
    if (sub == "assign") {
        if (t.size() < 3) { m->log("用法: layer assign <名称>（把选中图元移入该层，保留图元原有属性）", "warn"); return false; }
        m->assignSelectionToLayer(t[2], false);
        return true;
    }
    if (sub == "apply") {
        if (t.size() < 3) { m->log("用法: layer apply <名称>（把图层样式套用到本层图元）", "warn"); return false; }
        m->applyLayerStyleToElements(t[2]);
        return true;
    }
    if (sub == "set") {
        if (t.size() < 5) { m->log("用法: layer set <名称> <color|width|style> <值>", "warn"); return false; }
        const QString name = t[2];
        const Layer* l = m->findLayer(name);
        if (!l) { m->log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
        const QString field = t[3].toLower();
        QColor c = l->color; double w = l->lineWidth; LineStyle ls = l->lineStyle;
        if (field == "color") {
            QColor nc = QColor(t[4]);
            if (!nc.isValid()) { m->log(QStringLiteral("无效颜色: %1").arg(t[4]), "warn"); return false; }
            c = nc;
            return m->setLayerStyle(name, &c, nullptr, nullptr);   // 只改颜色，同步本层颜色
        } else if (field == "width") {
            bool ok; double v = t[4].toDouble(&ok);
            if (!ok) { m->log(QStringLiteral("无效线宽: %1").arg(t[4]), "warn"); return false; }
            w = v;
            return m->setLayerStyle(name, nullptr, &w, nullptr);   // 只改线宽，同步本层线宽
        } else if (field == "style") {
            ls = parseLineStyleArg(t[4]);
            return m->setLayerStyle(name, nullptr, nullptr, &ls);   // 只改线型，同步本层线型
        } else {
            m->log(QStringLiteral("未知字段: %1").arg(t[3]), "warn"); return false;
        }
    }
    m->log(QStringLiteral("未知图层命令: layer %1").arg(sub), "warn");
    return false;
}

// ============================================================
// 飞机三面图（航空制图模板）
// ============================================================

bool AircraftViewsCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    aircraft::AircraftViewsParams p;
    // 未来可扩展：按 toks 解析 key=value 覆盖默认值
    const auto elems = aircraft::buildThreeViews(p, m);
    m->beginEdit(QStringLiteral("绘制飞机三面图"));
    m->addElementsQuiet(elems);
    m->log(QStringLiteral("已生成飞机三面图：%1 个图元（侧面图/俯视图/正面图）")
               .arg(elems.size()));
    return true;
}

// ============================================================
// 帮助命令
// ============================================================

bool HelpCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    m->log("绘制: line / circle / circle3p / arc / rect / poly / spline");
    m->log("多边形: poly_regular / poly_circumscribed / poly_edge / rect_chamfer / rect_fillet / poly_star");
    m->log("样条: spline_parabola_4p / spline_parabola_fd / spline_hyperbola / spline_parabola /"
           " spline_through_points / freehand_line（后接点列）");
    m->log("样条编辑: spline_from_polyline <id> / spline_append_pt <id> x,y / spline_insert_pts <id> x,y");
    m->log("            spline_remove_pts <id> <idx> / spline_remove_between <id> <i> <j> / spline_explode <id>");
    m->log("样式: width 1|2|3|5   color #RRGGBB|red   chain [on|off]");
    m->log("编辑: delete <id> / trim <id> <x,y> [边界id..] / undo / redo / clear / list");
    m->log("视图: plane xy|yz|xz / grid [on|off] / zoom <倍率>");
    m->log("标注: dim linear|aligned|h|v x1,y1 x2,y2 [px,py]（GB/T 4458.4-2003）");
    m->log("      dim baseline|continue|arc|leader|ordinate（新增 5 种）");
    m->log("      dim angle 顶点 边1点 边2点 | dim radius|dia <圆id> [角度]");
    m->log("      dim baseline|continue x1,y1 x2,y2 [px,py] | dim arc <圆/弧id> [起角 止角]");
    m->log("      dim leader 起点x,y 折点x,y [文本点x,y] | dim ordinate 被测点x,y 基准点x,y [旋转角]");
    m->log("      dims ：依据当前选区智能标注（直线→对齐，圆→直径）");
    m->log("椭圆: ellipse_1pt cx,cy px,py / ellipse_axis cx,cy ax,ay px,py / ellipse_foci f1x,f1y f2x,f2y px,py");
    m->log("      ellipse_4p x1,y1 x2,y2 x3,y3 x4,y4 / ellipse_c3p cx,cy x1,y1 x2,y2 x3,y3 / ellipse_inscribed <框id>");
    m->log("折线: polyline x,y x,y x,y...（首末重合=闭合）");
    m->log("      polyline_add_node|append_node <id> x,y");
    m->log("      polyline_delete_node <id> <idx> | delete_between <id> <i> <j> | trim_segments <id> <segIdx>");
    m->log("      polyline_equidistant <id> <d> | from_segments id1 id2... | arcs_to_chords <id> | change_segment_type <id> <i> <j>");
    m->log("约束: constraint <type> <id..>");
    m->log("选择: select_all / select_none / invert");
    m->log("      select_entity <id> / deselect_entity <id> / select_contour <id> / toggle_contour <id>");
    m->log("      select_intersected <id> / deselect_intersected <id> / select_layer <图层名> / toggle_layer <id>");
    m->log("图层: layer add <名称> [颜色] [线宽] [线型] / layer delete <名称> / layer rename <旧> <新>");
    m->log("      layer visible <名称> [on|off] / layer lock <名称> [on|off] / layer set <名称> <color|width|style> <值>");
    m->log("      layer assign <名称>（选中图元移入该层） / layer apply <名称>（图层样式套用到本层图元） / layer list");
    m->log("点格式: x,y 或 x y 或 x,y,z（单位 mm，Y 轴向上为正）");
    return true;
}

// ============================================================
// CommandRegistry 实现
// ============================================================

CommandRegistry::CommandRegistry() {
    registerBuiltins();
}

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry reg;
    return reg;
}

void CommandRegistry::registerExact(const QString& name, std::shared_ptr<ICommand> cmd) {
    exact_[name] = std::move(cmd);
}

void CommandRegistry::registerPrefix(const QString& prefix, std::shared_ptr<ICommand> cmd) {
    prefixes_.append({prefix, std::move(cmd)});
}

ICommand* CommandRegistry::findExact(const QString& name) const {
    auto it = exact_.constFind(name);
    return it != exact_.constEnd() ? it.value().get() : nullptr;
}

ICommand* CommandRegistry::findPrefixMatch(const QString& name) const {
    for (const auto& [prefix, cmd] : prefixes_)
        if (name.startsWith(prefix))
            return cmd.get();
    return nullptr;
}

QStringList CommandRegistry::exactNames() const {
    return exact_.keys();
}

void CommandRegistry::registerBuiltins() {
    // ---- 基础绘图 ----
    registerExact("line",     std::make_shared<LineCommand>());
    registerExact("circle",   std::make_shared<CircleCommand>());
    registerExact("circle3p", std::make_shared<Circle3PCommand>());
    registerExact("arc",      std::make_shared<ArcCommand>());
    registerExact("rect",     std::make_shared<RectCommand>());
    registerExact("poly",     std::make_shared<PolySplineCommand>(ElementType::Polygon, "多边形"));
    registerExact("spline",   std::make_shared<PolySplineCommand>(ElementType::Spline, "样条曲线"));

    // ---- 直线子模式（前缀分派，所有 line_* 共用一个 LineModeCommand） ----
    registerPrefix("line_", std::make_shared<LineModeCommand>());

    // ---- 圆弧子模式（前缀分派，所有 arc_* 共用一个 ArcModeCommand）----
    registerPrefix("arc_", std::make_shared<ArcModeCommand>());

    // ---- 多边形子模式（前缀分派，poly_* 和 rect_chamfer/rect_fillet 共用一个 PolyModeCommand）----
    registerPrefix("poly_", std::make_shared<PolyModeCommand>());
    registerPrefix("rect_chamfer", std::make_shared<PolyModeCommand>());
    registerPrefix("rect_fillet", std::make_shared<PolyModeCommand>());

    // ---- 编辑（undo/redo/clear 为一行命令，用 LambdaCommand） ----
    registerExact("delete", std::make_shared<DeleteCommand>());
    registerExact("trim",   std::make_shared<TrimCommand>());
    registerExact("list",   std::make_shared<ListCommand>());

    // ---- 修改菜单：所有 modify_* 子命令 + 顶层 modify_ 系别名 ----
    // 与 Edit 菜单 + LibreCAD Modify 菜单语义对齐：
    //   move, copy, duplicate, rotate, scale, mirror, revert, lengthen,
    //   offset, break_at, divide, fillet, explode, align, stretch
    {
        auto mod = std::make_shared<ModifyCommand>();
        registerExact("move",              mod);
        registerExact("move_copy",         mod);
        registerExact("duplicate",         mod);
        registerExact("copy_dup",          mod);  // 别名
        registerExact("rotate",            mod);
        registerExact("scale",             mod);
        registerExact("mirror",            mod);
        registerExact("revert",            mod);
        registerExact("revert_direction",  mod);  // 别名
        registerExact("lengthen",          mod);
        registerExact("offset",            mod);
        registerExact("break_at",          mod);
        registerExact("divide",            mod);
        registerExact("fillet",            mod);
        registerExact("explode",           mod);
        registerExact("align",             mod);
        registerExact("stretch",           mod);
    }
    registerExact("undo", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { ctx.model->undo(); return true; }));
    registerExact("redo", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { ctx.model->redo(); return true; }));
    registerExact("clear", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { ctx.model->clear(); ctx.model->log("已清空草图"); return true; }));

    // ---- 视图 ----
    registerExact("plane", std::make_shared<PlaneCommand>());
    registerExact("grid",  std::make_shared<GridCommand>());
    registerExact("zoom",  std::make_shared<ZoomCommand>());

    // ---- 样式 ----
    registerExact("width", std::make_shared<WidthCommand>());
    registerExact("color", std::make_shared<ColorCommand>());
    registerExact("chain", std::make_shared<ChainCommand>());

    // ---- 标注 / 约束 ----
    registerExact("dim",        std::make_shared<DimCommand>());
    registerExact("dims",       std::make_shared<DimSmartCommand>());
    registerExact("dim_smart",  std::make_shared<DimSmartCommand>());
    registerExact("constraint", std::make_shared<ConstraintCommand>());

    // ---- 选择（对应选择菜单 / FreeCAD Sketcher 选择命令）----
    {
        auto sel = std::make_shared<SelectCommand>();
        registerExact("select_all",          sel);
        registerExact("select_none",         sel);
        registerExact("invert",              sel);
        registerExact("select_entity",       sel);
        registerExact("deselect_entity",     sel);
        registerExact("select_contour",      sel);
        registerExact("toggle_contour",      sel);
        registerExact("select_intersected",  sel);
        registerExact("deselect_intersected",sel);
        registerExact("select_layer",        sel);
        registerExact("deselect_layer",      sel);
        registerExact("toggle_layer",        sel);
    }

    // ---- 图层管理 ----
    registerExact("layer", std::make_shared<LayerCommand>());

    // ---- 样条子模式（前缀分派：绘制 spline_parabola/spline_hyperbola/
    //      spline_parabola_4p/spline_parabola_fd/spline_through_points，
    //      编辑 spline_from_polyline/spline_append_pt/spline_insert_pts/
    //      spline_remove_pts/spline_remove_between/spline_explode）----
    // 注意：精确命令 "spline" 优先于前缀匹配，不受影响。
    registerPrefix("spline_", std::make_shared<SplineModeCommand>());
    registerExact("freehand_line", std::make_shared<SplineModeCommand>());

    // ---- 椭圆子模式（前缀分派：ellipse_1pt / ellipse_axis / ellipse_foci /
    //      ellipse_4p / ellipse_c3p / ellipse_inscribed）----
    registerPrefix("ellipse_", std::make_shared<EllipseModeCommand>());

    // ---- 折线（精确 polyline + 前缀 polyline_ 共 9 个编辑）----
    registerExact("polyline", std::make_shared<PolylineModeCommand>());
    registerPrefix("polyline_", std::make_shared<PolylineModeCommand>());

    // ---- 飞机三面图（航空制图模板）----
    registerExact("aircraft_views", std::make_shared<AircraftViewsCommand>());

    // ---- 帮助 ----
    registerExact("help", std::make_shared<HelpCommand>());
}

} // namespace SketchCAD
