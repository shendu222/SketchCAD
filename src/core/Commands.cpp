#include "Commands.h"
#include "MacroScript.h"
#include "SketchModel.h"
#include "Geometry.h"
#include "Inquiry.h"
#include "Trim.h"
#include "LineModes.h"
#include "CircleModes.h"
#include "ArcModes.h"
#include "PolyModes.h"
#include "SplineModes.h"
#include "EllipseModes.h"
#include "ElementFactory.h"
#include "QSelect.h"        // 快速选择（QSELECT）四维过滤引擎
#include "Modify.h"
#include "ChamferFillet.h"
#include "Clipboard.h"
#include "Xref.h"
#include "ConstraintSolver.h"
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

// ============================================================
// 构造线（Xline）命令族：精确 "xline" + 前缀 "xline_"
// 无限直线 = 通过点 + 方向角（ElementFactory::createXline）。
// 复用 line:: 几何计算，结果线段 (a,b) 经「通过点 a + 方向角 (b-a)」转无限直线。
// ============================================================

bool XlineCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;

    auto makeXline = [&](const Vec3& through, double angleDeg, const QString& desc) {
        Element e = ElementFactory::createXline(through, angleDeg, m);
        const int id = m->addElement(e);
        m->log(QString("已创建构造线 #%1  %2 通过(%3, %4) 角度 %5°")
                   .arg(id).arg(desc)
                   .arg(through.x, 0, 'f', 2).arg(through.y, 0, 'f', 2)
                   .arg(angleDeg, 0, 'f', 1));
    };
    auto fromResult = [&](const line::Result& r, const QString& desc) -> bool {
        if (!r.valid) { m->log(r.msg, "warn"); return false; }
        makeXline(r.a, (r.b - r.a).angleDeg(), desc);
        return true;
    };

    const line::Mode lm = line::modeFromString(ctx.cmd);
    switch (lm) {
    case line::Mode::TwoPoint: {
        // xline x1,y1 x2,y2（两点，默认方式）；xline x,y <角度> 单点+角度自动识别
        //（"45" 不是合法点 → 走角度分支；"30,40" 是点 → 走两点分支）
        if (ctx.cmd == QStringLiteral("xline") && ctx.toks.size() == 3) {
            Vec3 p;
            bool ok = false;
            const double ang = ctx.toks[2].toDouble(&ok);
            if (ctx.parsePoint(1, p) && ok) {
                makeXline(p, ang, QStringLiteral("角度")); return true;
            }
        }
        auto pts = ctx.parsePts(1, 2);
        if (pts.size() < 2) { m->log(QString("用法: %1 x1,y1 x2,y2 或 %1 x,y 角度").arg(ctx.cmd), "warn"); return false; }
        return fromResult(line::twoPoint(pts[0], pts[1]), QStringLiteral("两点")); }
    case line::Mode::Horizontal: {
        auto pts = ctx.parsePts(1, 1);
        if (pts.empty()) { m->log("用法: xline_h x,y", "warn"); return false; }
        makeXline(pts[0], 0.0, QStringLiteral("水平")); return true; }
    case line::Mode::Vertical: {
        auto pts = ctx.parsePts(1, 1);
        if (pts.empty()) { m->log("用法: xline_v x,y", "warn"); return false; }
        makeXline(pts[0], 90.0, QStringLiteral("竖直")); return true; }
    case line::Mode::Angle: {
        // xline_angle x,y [角度]（缺省 30°，与 line_angle 默认一致）
        double ang = 30.0;
        if (ctx.toks.size() >= 3) {
            bool ok = false;
            const double v = ctx.toks.back().toDouble(&ok);
            if (ok) ang = v;
        }
        auto pts = ctx.parsePts(1, 1);
        if (pts.empty()) { m->log("用法: xline_angle x,y [角度]", "warn"); return false; }
        makeXline(pts[0], ang, QStringLiteral("角度")); return true; }
    case line::Mode::Bisector: {
        if (ctx.toks.size() < 4) { m->log("用法: xline_bisector refId1 refId2 x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 2); auto pts = ctx.parsePts(3, 1);
        if (refs.size() < 2 || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* l1 = m->element(refs[0]); const Element* l2 = m->element(refs[1]);
        if (!l1 || !l2) { m->log("参考图元不存在", "warn"); return false; }
        return fromResult(line::bisector(*l1, *l2, pts[0], pts[0]), QStringLiteral("角平分")); }
    case line::Mode::Parallel: {
        if (ctx.toks.size() < 3) { m->log("用法: xline_parallel refId x,y", "warn"); return false; }
        auto refs = ctx.parseRefs(1, 1); auto pts = ctx.parsePts(2, 1);
        if (refs.empty() || pts.empty()) { m->log("参数解析失败", "warn"); return false; }
        const Element* ref = m->element(refs[0]);
        if (!ref) { m->log("参考图元不存在", "warn"); return false; }
        // 平行过点：方向取参考线方向，过点位置由用户给定（cur 取同一点，无偏移歧义）
        return fromResult(line::parallelThroughPoint(*ref, pts[0], pts[0]), QStringLiteral("平行")); }
    default:
        m->log("未实现的构造线子命令: " + ctx.cmd, "warn"); return false;
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

    // 目标集：无参 → 全部图元；带 id 参数 → 指定图元（无效 id 明确报错）
    std::vector<Element> targets;
    if (ctx.toks.size() >= 2) {
        for (int i = 1; i < ctx.toks.size(); ++i) {
            bool ok = false;
            const int id = ctx.toks[i].toInt(&ok);
            const Element* e = ok ? m->element(id) : nullptr;
            if (!e) { m->log("list: #" + ctx.toks[i] + " 不存在", "warn"); return false; }
            targets.push_back(*e);
        }
    } else {
        targets = m->elements();
    }

    if (targets.empty()) { m->log("模型为空：无图元可列出", "warn"); return true; }
    m->log(QString("---- 对象列表（共 %1 个）----").arg(targets.size()));
    for (const auto& e : targets) {
        // 首行：id + 类型 + 图层 + 主尺寸描述
        m->log(QString("#%1 %2  [图层 %3]  %4")
                   .arg(e.id).arg(e.typeName(), e.layer, e.sizeDesc()));
        // 次行（封闭图元）：面积 / 周长 / 形心 —— 与 AREA/MASSPROP 命令同源
        const auto ar = inquiry::areaOfElement(e);
        if (ar.ok) {
            m->log(QString("     面积 %1 mm² / 周长 %2 mm / 形心 (%3, %4)")
                       .arg(ar.area, 0, 'f', 2).arg(ar.perimeter, 0, 'f', 2)
                       .arg(ar.centroid.x, 0, 'f', 2).arg(ar.centroid.y, 0, 'f', 2));
        }
    }
    return true;
}

// ============================================================
// 查询测量（Inquiry）：dist / area / massprop / id
// ------------------------------------------------------------
//   dist     x1,y1 x2,y2                       两点距离 + 增量 + 方向角
//   area     <封闭图元id>                      对象面积/周长
//   area     x1,y1 x2,y2 [x3,y3 ...]           点列围成多边形
//   area     （无参）                           当前选区逐个统计
//   massprop [id ...]                          质量特性（无参用选区）
//   id       x,y                               点坐标 + 最近图元
// 几何计算全部委托 Inquiry 核心层；本层只做参数解析 / 分派 / 日志格式。
// ============================================================
bool DistCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) {
        m->log("用法: dist x1,y1 x2,y2", "warn");
        return false;
    }
    Vec3 a, b;
    if (!ctx.parsePoint(1, a) || !ctx.parsePoint(2, b)) {
        m->log("dist: 点解析失败（格式 x,y）", "warn");
        return false;
    }
    const auto r = inquiry::distTwoPoints(a, b);
    m->log(QString("距离 = %1 mm    ΔX = %2    ΔY = %3    XY平面角 = %4°")
               .arg(r.distance, 0, 'f', 4).arg(r.dx, 0, 'f', 2)
               .arg(r.dy, 0, 'f', 2).arg(r.angleDeg, 0, 'f', 2));
    return true;
}

bool AreaCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;

    // ---- 点列模式：任一参数含 "," 即视为点列 ----
    bool pointMode = ctx.toks.size() >= 2 && ctx.toks[1].contains(',');
    if (pointMode) {
        std::vector<Vec3> ring;
        for (int i = 1; i < ctx.toks.size(); ++i) {
            Vec3 p;
            if (!ctx.parsePoint(i, p)) {
                m->log("area: 第 " + QString::number(i) + " 个点解析失败（" + ctx.toks[i] + "）", "warn");
                return false;
            }
            ring.push_back(p);
        }
        const auto r = inquiry::areaOfPolygon(ring);
        if (!r.ok) { m->log("area: " + r.error, "warn"); return false; }
        m->log(QString("点列模式（%1 个顶点）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)")
                   .arg(ring.size()).arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                   .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
        return true;
    }

    // ---- 对象模式：id 参数 / 无参用选区 ----
    std::vector<Element> targets;
    if (ctx.toks.size() >= 2) {
        for (int i = 1; i < ctx.toks.size(); ++i) {
            bool ok = false;
            const int id = ctx.toks[i].toInt(&ok);
            const Element* e = ok ? m->element(id) : nullptr;
            if (!e) { m->log("area: #" + ctx.toks[i] + " 不存在", "warn"); return false; }
            targets.push_back(*e);
        }
    } else {
        for (int id : m->selection()) {
            if (const Element* e = m->element(id)) targets.push_back(*e);
        }
        if (targets.empty()) {
            m->log("用法: area <封闭图元id> | area x1,y1 x2,y2 [x3,y3 ...]（或先选中图元/一组首尾相接的直线）", "warn");
            return false;
        }
    }

    // ---- 直线链模式：目标全为直线 → 尝试按"首尾相接"围成封闭多边形求面积 ----
    // 典型场景：在画布上按参考图逐边描线（离散 Line 围成多边形），选中后 area 直接出面积。
    const bool allLines = targets.size() >= 3 && std::all_of(
        targets.begin(), targets.end(),
        [](const Element& e) { return e.type == ElementType::Line; });
    if (allLines) {
        const auto r = inquiry::areaOfClosedLineChain(targets);
        if (!r.ok) { m->log("area: " + r.error, "warn"); return false; }
        m->log(QString("直线链模式（%1 条边围成多边形）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)")
                   .arg(targets.size()).arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                   .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
        return true;
    }

    double total = 0;
    int counted = 0;
    for (const auto& e : targets) {
        const auto r = inquiry::areaOfElement(e);
        if (!r.ok) {
            m->log(QString("area: #%1 %2 —— %3").arg(e.id).arg(e.typeName(), r.error), "warn");
            continue;
        }
        ++counted;
        total += r.area;
        m->log(QString("#%1 %2：面积 = %3 mm²    周长 = %4 mm    形心 = (%5, %6)")
                   .arg(e.id).arg(e.typeName())
                   .arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                   .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
    }
    if (counted >= 2)
        m->log(QString("合计 %1 个封闭区域：总面积 = %2 mm²").arg(counted).arg(total, 0, 'f', 4));
    return counted > 0;
}

bool MassPropCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;

    std::vector<Element> targets;
    if (ctx.toks.size() >= 2) {
        for (int i = 1; i < ctx.toks.size(); ++i) {
            bool ok = false;
            const int id = ctx.toks[i].toInt(&ok);
            const Element* e = ok ? m->element(id) : nullptr;
            if (!e) { m->log("massprop: #" + ctx.toks[i] + " 不存在", "warn"); return false; }
            targets.push_back(*e);
        }
    } else {
        for (int id : m->selection()) {
            if (const Element* e = m->element(id)) targets.push_back(*e);
        }
        if (targets.empty()) {
            m->log("用法: massprop <封闭图元id> ...（或先选中图元）", "warn");
            return false;
        }
    }

    for (const auto& e : targets) {
        const auto r = inquiry::massPropOfElement(e);
        if (!r.ok) {
            m->log(QString("massprop: #%1 %2 —— %3").arg(e.id).arg(e.typeName(), r.error), "warn");
            continue;
        }
        m->log(QString("---- #%1 %2 质量特性 ----").arg(e.id).arg(e.typeName()));
        m->log(QString("  面积        %1 mm²").arg(r.area, 0, 'f', 4));
        m->log(QString("  周长        %1 mm").arg(r.perimeter, 0, 'f', 4));
        m->log(QString("  包围盒      (%1, %2) ~ (%3, %4)    %5 × %6")
                   .arg(r.bbox.left(), 0, 'f', 2).arg(r.bbox.bottom(), 0, 'f', 2)
                   .arg(r.bbox.right(), 0, 'f', 2).arg(r.bbox.top(), 0, 'f', 2)
                   .arg(r.bbox.width(), 0, 'f', 2).arg(r.bbox.height(), 0, 'f', 2));
        m->log(QString("  形心        (%1, %2)")
                   .arg(r.centroid.x, 0, 'f', 3).arg(r.centroid.y, 0, 'f', 3));
        m->log(QString("  惯性矩(形心) Ix = %1    Iy = %2    Ixy = %3 mm⁴")
                   .arg(r.ixBar, 0, 'f', 2).arg(r.iyBar, 0, 'f', 2).arg(r.ixyBar, 0, 'f', 2));
    }
    return true;
}

bool IdCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 2) {
        m->log("用法: id x,y", "warn");
        return false;
    }
    Vec3 p;
    if (!ctx.parsePoint(1, p)) {
        m->log("id: 点解析失败（格式 x,y）", "warn");
        return false;
    }
    const auto r = inquiry::idPoint(p, m->elements());
    m->log(QString("坐标: X = %1    Y = %2    Z = %3")
               .arg(p.x, 0, 'f', 4).arg(p.y, 0, 'f', 4).arg(p.z, 0, 'f', 4));
    if (r.nearest)
        m->log(QString("最近图元: #%1 %2（距离 %3 mm）")
                   .arg(r.nearest->id).arg(r.nearest->typeName())
                   .arg(r.nearestDist, 0, 'f', 4));
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
//   mirror      point <x> <y> | line <x1> <y1> <x2> <y2> | h | v（新增镜像副本）
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
        if (ctx.toks.size() < 2) {
            m->log("用法: mirror point <x> <y> | mirror line <x1> <y1> <x2> <y2> | mirror h | mirror v", "warn");
            return false;
        }
        const QString kind = ctx.toks[1].toLower();
        auto src = selectedElems();
        if (src.empty()) { m->log("无可镜像图元", "warn"); return false; }
        bool isPoint = false;
        Vec3 b1, b2(1, 0, 0);
        if (kind == "point") {
            if (ctx.toks.size() < 4) { m->log("用法: mirror point <x> <y>", "warn"); return false; }
            if (!ctx.parsePoint(2, b1)) { m->log("中心点解析失败", "warn"); return false; }
            isPoint = true;
        } else if (kind == "line") {
            if (ctx.toks.size() < 6) { m->log("用法: mirror line <x1> <y1> <x2> <y2>", "warn"); return false; }
            if (!ctx.parsePoint(2, b1) || !ctx.parsePoint(4, b2)) { m->log("轴点解析失败", "warn"); return false; }
            isPoint = false;
        } else if (kind == "h" || kind == "horizontal" || kind == "v" || kind == "vertical") {
            // 便捷：关于「选区所有控制点的几何中心」的水平 / 竖直直线镜像
            double cx = 0, cy = 0, n = 0;
            for (const auto& e : src) for (const auto& p : e.pts) { cx += p.x; cy += p.y; ++n; }
            if (n > 0) { cx /= n; cy /= n; }
            if (kind == "h" || kind == "horizontal") { b1 = Vec3(0, cy, 0); b2 = Vec3(1, cy, 0); }
            else                                     { b1 = Vec3(cx, 0, 0); b2 = Vec3(cx, 1, 0); }
            isPoint = false;
        } else { m->log("未知镜像方式: " + kind, "warn"); return false; }
        auto changes = modify::makeMirror(src, isPoint, b1, b2);
        m->beginEdit(QStringLiteral("修改-镜像"));
        auto added = modify::applyChanges(m, changes, QString());
        m->log(QString("已生成 %1 个镜像图元（%2）").arg(added.size()).arg(kind));
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
    if (cmd == "chamfer") {
        // 倒角：chamfer <id1> <id2> <d1> <d2> [angleDeg]
        //   * 仅给 d1,d2  → 距离-距离倒角（沿两线各回退 d1/d2，补直线连接）
        //   * 额外给 angleDeg → 距离-角度倒角（沿 a 回退 d1，倒角线与 a 成 angleDeg，自动求 b 退距）
        // 几何全部走 corner 核心层，model 仅按 Change 落盘，可独立调用、易于扩展。
        if (ctx.toks.size() < 5) {
            m->log(QStringLiteral("用法: chamfer <id1> <id2> <d1> <d2> [angleDeg]"), "warn");
            return false;
        }
        bool ok1, ok2; int id1 = ctx.toks[1].toInt(&ok1), id2 = ctx.toks[2].toInt(&ok2);
        if (!ok1 || !ok2) { m->log(QStringLiteral("id 解析失败"), "warn"); return false; }
        double d1 = ctx.toks[3].toDouble(), d2 = ctx.toks[4].toDouble();
        const Element* ea = m->element(id1);
        const Element* eb = m->element(id2);
        if (!ea || !eb) { m->log(QStringLiteral("图元不存在"), "warn"); return false; }
        corner::CornerResult res;
        if (ctx.toks.size() >= 6) {
            double ang = ctx.toks[5].toDouble();
            res = corner::chamferPairDistAngle(*ea, *eb, d1, ang);
        } else {
            res = corner::chamferPair(*ea, *eb, d1, d2);
        }
        if (!res.ok) { m->log(QStringLiteral("倒角失败：") + res.msg, "warn"); return false; }
        auto changes = modify::makeCornerChanges(id1, id2, res);
        m->beginEdit(QStringLiteral("修改-倒角"));
        auto added = modify::applyChanges(m, changes, QString());
        QStringList ids; for (int id : added) ids << ("#" + QString::number(id));
        m->log(QString("已对 #%1 与 #%2 做倒角（d1=%3, d2=%4），新增 %5：%6")
                   .arg(id1).arg(id2).arg(d1, 0, 'f', 2).arg(d2, 0, 'f', 2)
                   .arg(added.size()).arg(ids.join(", ")));
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
    if (cmd == "join") {
        // 连合：把选区中同层、端点相接的 Line/Arc 连成 Polyline。
        //   join [tol]    tol 缺省 0.5mm（GUI 菜单入口传视图捕捉容差 worldTol()）
        auto src = selectedElems();
        if (src.empty()) { m->log("无可连合图元（请先选中 Line/Arc）", "warn"); return false; }
        double tol = modify::kJoinDefaultTol;
        if (ctx.toks.size() >= 2) {
            bool ok = false; tol = ctx.toks[1].toDouble(&ok);
            if (!ok || tol <= 0) { m->log("容差须为正数（mm）", "warn"); return false; }
        }
        auto changes = modify::makeJoin(src, tol);
        if (changes.empty()) { m->log("未发现可连合的端点相接链（同层 Line/Arc 且端点距 ≤ 容差）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-连合"));
        auto added = modify::applyChanges(m, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        m->log(QString("已连合为 %1 条折线（容差 %2 mm）：%3")
                   .arg(added.size()).arg(tol, 0, 'f', 3).arg(ids.join(", ")));
        return true;
    }
    if (cmd == "arrayrect") {
        // 矩形阵列：arrayrect <rows> <cols> <rowSpacing> <colSpacing>
        if (ctx.toks.size() < 5) { m->log("用法: arrayrect <行数> <列数> <行间距> <列间距>", "warn"); return false; }
        bool ok1, ok2, ok3, ok4;
        const int rows = ctx.toks[1].toInt(&ok1), cols = ctx.toks[2].toInt(&ok2);
        const double rsp = ctx.toks[3].toDouble(&ok3), csp = ctx.toks[4].toDouble(&ok4);
        if (!ok1 || !ok2 || !ok3 || !ok4 || rows < 1 || cols < 1) {
            m->log("参数无效（行/列 ≥1，间距可为 0）", "warn"); return false;
        }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可阵列图元（请先选中）", "warn"); return false; }
        auto changes = modify::makeRectangularArray(src, rows, cols, rsp, csp);
        if (changes.empty()) { m->log("1×1 阵列无副本（原图元保留）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-矩形阵列"));
        auto added = modify::applyChanges(m, changes, QString());
        m->log(QString("矩形阵列 %1×%2（间距 %3/%4 mm）：新增 %5 个副本")
                   .arg(rows).arg(cols).arg(rsp, 0, 'f', 2).arg(csp, 0, 'f', 2).arg(added.size()));
        return true;
    }
    if (cmd == "arraypolar") {
        // 环形阵列：arraypolar <cx,cy> <count> [angleRange]
        // 基点语义与 AutoCAD 对齐：选中单个圆/弧且未显式给中心时，默认取其圆心。
        auto src = selectedElems();
        if (src.empty()) { m->log("无可阵列图元（请先选中）", "warn"); return false; }
        Vec3 center;
        int argIdx = 1;
        bool centerGiven = (ctx.toks.size() >= 2 && ctx.parsePoint(1, center));
        if (centerGiven) argIdx = 2;
        if (!centerGiven) {
            const Element* first = nullptr;
            for (const auto& e : src)
                if (e.type == ElementType::Circle || e.type == ElementType::Arc) { first = &e; break; }
            if (first && !first->pts.empty()) {
                center = first->pts[0];   // 默认捕捉圆心
            } else {
                m->log("用法: arraypolar <cx,cy> <count> [角度范围]（选中圆/弧时可省略中心=圆心）", "warn");
                return false;
            }
        }
        if (ctx.toks.size() <= argIdx) { m->log("缺少数量参数 <count>", "warn"); return false; }
        bool ok = false;
        const int count = ctx.toks[argIdx].toInt(&ok);
        if (!ok || count < 1) { m->log("数量须 ≥1", "warn"); return false; }
        const double range = ctx.toks.size() > argIdx + 1 ? ctx.toks[argIdx + 1].toDouble() : 360.0;
        auto changes = modify::makePolarArray(src, center, count, range);
        if (changes.empty()) { m->log("count<2 无副本（原图元保留）", "warn"); return false; }
        m->beginEdit(QStringLiteral("修改-环形阵列"));
        auto added = modify::applyChanges(m, changes, QString());
        m->log(QString("环形阵列 %1 份 / %2°（中心 %3,%4）：新增 %5 个副本")
                   .arg(count).arg(range, 0, 'f', 2)
                   .arg(center.x, 0, 'f', 2).arg(center.y, 0, 'f', 2).arg(added.size()));
        return true;
    }
    if (cmd == "arraypath") {
        // 路径阵列：arraypath <pathId> <spacing>   Measure 语义（每 spacing 一个）
        //            arraypath divide <pathId> <count>   Divide 语义（count 等分全长）
        bool divide = (ctx.toks.size() >= 2 && ctx.toks[1].toLower() == "divide");
        const int baseIdx = divide ? 2 : 1;
        if (ctx.toks.size() < baseIdx + 2) {
            m->log("用法: arraypath <路径id> <间距> | arraypath divide <路径id> <数量>", "warn");
            return false;
        }
        bool ok = false;
        const int pathId = ctx.toks[baseIdx].toInt(&ok);
        if (!ok) { m->log("路径 id 无效", "warn"); return false; }
        const Element* path = m->element(pathId);
        if (!path) { m->log("路径图元不存在", "warn"); return false; }
        // 路径离散：Polyline 用弧感知采样，Spline 用样条采样；其余类型不支持
        std::vector<Vec3> pathPts;
        if (path->type == ElementType::Polyline && path->pts.size() >= 2) {
            pathPts = geo::samplePolyline(path->pts, path->arcFlags, path->closed, 32);
        } else if (path->type == ElementType::Spline && path->pts.size() >= 2) {
            pathPts = geo::sampleSpline(path->pts, 32);
        } else {
            m->log("路径仅支持折线（Polyline）或样条（Spline）", "warn"); return false;
        }
        double spacing = 0.0; int count = 0;
        if (divide) {
            count = ctx.toks[baseIdx + 1].toInt(&ok);
            if (!ok || count < 2) { m->log("数量须 ≥2", "warn"); return false; }
        } else {
            spacing = ctx.toks[baseIdx + 1].toDouble(&ok);
            if (!ok || spacing < 0) { m->log("间距须 ≥0（0 = 按数量均分，需用 divide）", "warn"); return false; }
        }
        auto src = selectedElems();
        if (src.empty()) { m->log("无可阵列图元（请先选中）", "warn"); return false; }
        auto changes = modify::makePathArray(src, pathPts, spacing, count);
        if (changes.empty()) {
            m->log(divide ? "路径退化或数量不足，未生成副本"
                          : "路径长度小于间距，放不下第二个项目（未生成副本）", "warn");
            return false;
        }
        m->beginEdit(QStringLiteral("修改-路径阵列"));
        auto added = modify::applyChanges(m, changes, QString());
        m->log(QString("路径阵列沿 #%1：%2 个位置点，新增 %3 个副本")
                   .arg(pathId).arg(changes.size() / (int)src.size() + 1).arg(added.size()));
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

// 约束命令：显式几何约束的添加 / 求解 / 列表 / 冲突检测 / 诊断 / 清空。
//   constraint add <类型> <idA> <idB> [值|对称轴id]   添加（并立即求解一次）
//   constraint solve                                   求解全部显式约束
//   constraint list                                    列出全部约束及满足状态
//   constraint check                                   冲突 / 过约束检测
//   constraint diagnose                                逐实体自由度与欠/过约束诊断（只读）
//   constraint clear                                   清空全部
//   兼容旧用法：constraint <类型> <id1> [id2]（等价于 add）
// 类型：coincident 重合 / parallel 平行 / perpendicular 垂直 / tangent 相切 /
//       concentric 同心 / symmetric 对称 / dimensional 尺寸 / equal 相等 /
//       horizontal 水平 / vertical 竖直 / fixed 固定
bool ConstraintCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    static const QMap<QString, ConstraintType> cmap = {
        {"coincident", ConstraintType::Coincident},
        {"parallel", ConstraintType::Parallel},
        {"perpendicular", ConstraintType::Perpendicular},
        {"equal", ConstraintType::Equal},
        {"tangent", ConstraintType::Tangent},
        {"horizontal", ConstraintType::Horizontal},
        {"vertical", ConstraintType::Vertical},
        {"fixed", ConstraintType::Fixed},
        {"concentric", ConstraintType::Concentric},
        {"symmetric", ConstraintType::Symmetric},
        {"dimensional", ConstraintType::Dimensional},
        {"collinear", ConstraintType::Collinear},
        {"equallength", ConstraintType::EqualLength},
        {"equalradius", ConstraintType::EqualRadius},
        {"pointonline", ConstraintType::PointOnLine},
        {"pointoncircle", ConstraintType::PointOnCircle},
        {"fixedpoint", ConstraintType::FixedPoint},
        {"midpoint", ConstraintType::Midpoint},
        {"distance", ConstraintType::Distance},
        {"angle", ConstraintType::Angle},
        {"symmetricpoint", ConstraintType::SymmetricPoint}};
    const auto cname = [](ConstraintType t) { return QString::fromUtf8(constraintTypeName(t)); };

    if (ctx.toks.size() < 2) {
        m->log(QStringLiteral("用法：constraint add <类型> <idA> <idB> [值|轴id] | solve | list | check | diagnose | clear"), "warn");
        return false;
    }
    const QString sub = ctx.toks[1].toLower();

    // ---- help：列出全部支持的约束类型（来自统一注册表）----
    if (sub == "help") {
        const auto descs = ConstraintSolver::constraintDescriptors();
        m->log(QStringLiteral("共 %1 种几何约束：").arg(descs.size()), "info");
        for (const auto& d : descs)
            m->log(QStringLiteral("  %1  %2 | 适用：%3 | 参数：%4")
                       .arg(QString::fromUtf8(d.id), -14).arg(QString::fromUtf8(d.name))
                       .arg(QString::fromUtf8(d.applicable)).arg(QString::fromUtf8(d.params)),
                   "info");
        return true;
    }
    // ---- list：列出全部显式约束 ----
    if (sub == "list") {
        const auto& cs = m->constraints();
        if (cs.empty()) { m->log(QStringLiteral("当前无显式几何约束"), "info"); return true; }
        m->log(QStringLiteral("显式几何约束 %1 条：").arg(cs.size()), "info");
        for (size_t i = 0; i < cs.size(); ++i) {
            const auto& r = cs[i];
            QString extra;
            if (r.axisId >= 0) extra += QStringLiteral(" 轴=#%1").arg(r.axisId);
            if (r.value > 0)   extra += QStringLiteral(" 值=%1").arg(r.value, 0, 'f', 2);
            if (r.idxA >= 0)   extra += QStringLiteral(" idxA=%1").arg(r.idxA);
            if (r.idxB >= 0)   extra += QStringLiteral(" idxB=%1").arg(r.idxB);
            m->log(QStringLiteral("  %1. 「%2」 主动#%3 → 被动#%4%5  %6")
                       .arg(i + 1).arg(cname(r.type)).arg(r.idA).arg(r.idB).arg(extra)
                       .arg(r.satisfied ? QStringLiteral("✓已满足") : QStringLiteral("✗未满足")),
                   r.satisfied ? "info" : "warn");
        }
        return true;
    }
    // ---- check：冲突 / 过约束检测 ----
    if (sub == "check") {
        const auto conf = ConstraintSolver::detectConflicts(*m);
        if (conf.empty()) { m->log(QStringLiteral("冲突检测：未发现冲突或过约束"), "info"); return true; }
        m->log(QStringLiteral("冲突检测：发现 %1 处问题").arg(conf.size()), "warn");
        for (const auto& s : conf) m->log(QStringLiteral("  ⚠ %1").arg(s), "warn");
        return false;
    }
    // ---- diagnose：逐实体自由度与欠/过约束诊断（只读）----
    if (sub == "diagnose" || sub == "dof") {
        const auto d = ConstraintSolver::diagnose(*m);
        m->log(QStringLiteral("约束诊断：%1（总自由度=%2，被约束图元剩余=%3，冲突/冗余=%4 条）")
                   .arg(d.stateText()).arg(d.totalDof).arg(d.freeDof).arg(d.overDof),
               d.state == ConstraintDiagnosis::WellConstrained ? "info" : "warn");
        for (const auto& ei : d.elementInfo)
            m->log(QStringLiteral("  #%1(%2)  固有=%3 可控=%4 已约束=%5 剩余=%6%7")
                       .arg(ei.id).arg(QString::fromUtf8(elementTypeName(ei.type)))
                       .arg(ei.naturalDof).arg(ei.activeVars).arg(ei.appliedRank).arg(ei.remainingDof)
                       .arg(ei.overconstrained ? QStringLiteral("  ⚠过约束") : QString()),
                   "info");
        for (const auto& s : d.messages)
            m->log(QStringLiteral("  → %1").arg(s),
                   d.state == ConstraintDiagnosis::WellConstrained ? "info" : "warn");
        return d.state == ConstraintDiagnosis::WellConstrained ||
               d.state == ConstraintDiagnosis::UnderConstrained;
    }
    // ---- solve：求解全部 ----
    if (sub == "solve") {
        const auto rep = ConstraintSolver::solveAll(*m);
        m->log(QStringLiteral("约束求解：共 %1 条，已满足 %2 条，失败 %3 条；自由度(DOF)=%4，有效约束秩=%5，残差=%.3e")
                   .arg(rep.total).arg(rep.solved).arg(rep.failures.size())
                   .arg(rep.diag.dof).arg(rep.diag.rank).arg(rep.diag.residualNorm),
               rep.failures.empty() ? "info" : "warn");
        for (const auto& f : rep.failures) m->log(QStringLiteral("  ✗ %1").arg(f.reason), "error");
        if (!rep.diag.redundantIdx.empty())
            m->log(QStringLiteral("  ⓘ 检测到 %1 条冗余约束（下标：%2），可删除以简化系统")
                       .arg(rep.diag.redundantIdx.size())
                       .arg([&](){ QString s; for (int v : rep.diag.redundantIdx) s += QString::number(v) + " "; return s; }()),
                   "info");
        return rep.failures.empty();
    }
    // ---- clear：清空 ----
    if (sub == "clear") {
        if (m->constraints().empty()) { m->log(QStringLiteral("当前无显式几何约束"), "info"); return true; }
        m->clearConstraints();
        m->log(QStringLiteral("已清空全部显式几何约束"), "info");
        return true;
    }

    // ---- dimension：尺寸驱动子命令（Request M 新增）----
    if (sub == "dimension") {
        if (ctx.toks.size() < 3) {
            m->log(QStringLiteral("用法：constraint dimension link <标注id> [参数名] | setvalue <标注id> <值>"), "warn");
            return false;
        }
        const QString dsub = ctx.toks[2].toLower();
        if (dsub == "link") {
            if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法：constraint dimension link <标注id> [参数名]"), "warn"); return false; }
            bool ok = false; const int dimId = ctx.toks[3].toInt(&ok);
            if (!ok) { m->log(QStringLiteral("无效的标注 id：%1").arg(ctx.toks[3]), "warn"); return false; }
            const QString param = (ctx.toks.size() > 4) ? ctx.toks[4] : QString();
            const QString err = ConstraintSolver::linkDimension(*m, dimId, param);
            if (!err.isEmpty()) { m->log(QStringLiteral("关联尺寸标注失败：%1").arg(err), "error"); return false; }
            m->log(QStringLiteral("已关联尺寸标注 #%1%2，修改其数值即可驱动几何重构")
                       .arg(dimId).arg(param.isEmpty() ? QString() : QStringLiteral("（参数 %1）").arg(param)), "info");
            return true;
        }
        if (dsub == "setvalue") {
            if (ctx.toks.size() < 5) { m->log(QStringLiteral("用法：constraint dimension setvalue <标注id> <值>"), "warn"); return false; }
            bool ok = false; const int dimId = ctx.toks[3].toInt(&ok);
            if (!ok) { m->log(QStringLiteral("无效的标注 id：%1").arg(ctx.toks[3]), "warn"); return false; }
            bool okv = false; const double val = ctx.toks[4].toDouble(&okv);
            if (!okv) { m->log(QStringLiteral("无效的数值：%1").arg(ctx.toks[4]), "warn"); return false; }
            const QString err = ConstraintSolver::setDimensionValue(*m, dimId, val);
            if (!err.isEmpty()) { m->log(QStringLiteral("设置标注值失败：%1").arg(err), "error"); return false; }
            m->log(QStringLiteral("已将标注 #%1 设为 %2，几何已联动重构").arg(dimId).arg(val, 0, 'f', 3), "info");
            return true;
        }
        m->log(QStringLiteral("未知 dimension 子命令：%1（支持 link / setvalue）").arg(dsub), "warn");
        return false;
    }

    // ---- param：参数表子命令（Request M 新增）----
    if (sub == "param") {
        if (ctx.toks.size() < 3) {
            m->log(QStringLiteral("用法：constraint param set <名称> <值> | list"), "warn");
            return false;
        }
        const QString psub = ctx.toks[2].toLower();
        if (psub == "list") {
            const auto& ps = m->params();
            if (ps.empty()) { m->log(QStringLiteral("参数表为空"), "info"); return true; }
            m->log(QStringLiteral("参数表（%1 项）：").arg(ps.size()), "info");
            for (const auto& kv : ps)
                m->log(QStringLiteral("  %1 = %2").arg(kv.first).arg(kv.second, 0, 'f', 3), "info");
            return true;
        }
        if (psub == "set") {
            if (ctx.toks.size() < 5) { m->log(QStringLiteral("用法：constraint param set <名称> <值>"), "warn"); return false; }
            const QString name = ctx.toks[3];
            bool ok = false; const double val = ctx.toks[4].toDouble(&ok);
            if (!ok) { m->log(QStringLiteral("无效的数值：%1").arg(ctx.toks[4]), "warn"); return false; }
            const QString err = ConstraintSolver::driveParameter(*m, name, val);
            if (!err.isEmpty()) { m->log(QStringLiteral("参数驱动失败：%1").arg(err), "warn"); return false; }
            m->log(QStringLiteral("已将参数 %1 设为 %2 并联动驱动全部关联标注/几何").arg(name).arg(val, 0, 'f', 3), "info");
            return true;
        }
        m->log(QStringLiteral("未知 param 子命令：%1（支持 set / list）").arg(psub), "warn");
        return false;
    }

    // ---- add（或旧用法）：constraint [add] <类型> <idA> <idB> [值|轴id] [idxA] [idxB] ----
    const bool hasAdd = (sub == "add");
    const int iType = hasAdd ? 2 : 1;
    if (ctx.toks.size() <= iType) {
        m->log(QStringLiteral("用法：constraint add <类型> <idA> <idB> [值|轴id] [idxA] [idxB]（fixed/fixedpoint 可省略 idB）"), "warn");
        return false;
    }
    const QString t = ctx.toks[iType].toLower();
    if (!cmap.contains(t)) {
        m->log(QStringLiteral("未知约束类型：%1（输入 constraint help 查看全部 %2 种）").arg(t).arg(kConstraintTypeCount), "warn");
        return false;
    }
    const ConstraintType ct = cmap[t];
    const bool singleEnd = (ct == ConstraintType::Fixed || ct == ConstraintType::FixedPoint);
    if (ctx.toks.size() <= iType + 1) {
        m->log(QStringLiteral("用法：constraint add %1 <idA> [idB] [值|轴id] [idxA] [idxB]").arg(t), "warn");
        return false;
    }
    bool okA = false;
    const int idA = ctx.toks[iType + 1].toInt(&okA);
    if (!okA) { m->log(QStringLiteral("无效的图元 id：%1").arg(ctx.toks[iType + 1]), "warn"); return false; }
    int idB = -1;
    int argPos = iType + 2;                       // 当前待解析的可选参数位置
    if (singleEnd) {
        // fixed/fixedpoint：idB 可省；若紧接着是数字则仍当作 idB（兼容旧式）
        if (ctx.toks.size() > argPos) {
            bool ok = false; const int v = ctx.toks[argPos].toInt(&ok);
            if (ok) { idB = v; ++argPos; }
        }
    } else {
        if (ctx.toks.size() <= argPos) {
            m->log(QStringLiteral("用法：constraint add %1 <idA> <idB> [值|轴id] [idxA] [idxB]").arg(t), "warn");
            return false;
        }
        bool okB = false; idB = ctx.toks[argPos].toInt(&okB);
        if (!okB) { m->log(QStringLiteral("无效的图元 id：%1").arg(ctx.toks[argPos]), "warn"); return false; }
        ++argPos;
    }

    // 随后的参数：symmetric/symmetricpoint → 轴id；dimensional/distance/angle → 值；其余 → idxA/idxB
    int axisId = -1; double value = 0.0; int idxA = -1, idxB = -1;
    if (ctx.toks.size() > argPos) {
        const QString v = ctx.toks[argPos];
        if (ct == ConstraintType::Symmetric || ct == ConstraintType::SymmetricPoint) {
            bool ok = false; const int ax = v.toInt(&ok);
            if (!ok) { m->log(QStringLiteral("无效的对称轴图元 id：%1").arg(v), "warn"); return false; }
            axisId = ax;
        } else if (ct == ConstraintType::Dimensional || ct == ConstraintType::Distance || ct == ConstraintType::Angle) {
            bool ok = false; const double dv = v.toDouble(&ok);
            if (!ok) { m->log(QStringLiteral("无效的数值参数：%1").arg(v), "warn"); return false; }
            value = dv;
        } else {
            bool ok = false; const int iv = v.toInt(&ok);
            if (!ok) { m->log(QStringLiteral("无效的控制点索引：%1").arg(v), "warn"); return false; }
            idxA = iv;
        }
        ++argPos;
    }
    if (ctx.toks.size() > argPos) { bool ok=false; idxB = ctx.toks[argPos].toInt(&ok); if (!ok) idxB = -1; ++argPos; }

    const QString err = ConstraintSolver::addConstraint(*m, ct, idA, idB, value, axisId, idxA, idxB);
    if (!err.isEmpty()) { m->log(QStringLiteral("约束添加失败：%1").arg(err), "error"); return false; }
    m->log(QStringLiteral("已添加「%1」约束：主动#%2 → 被动#%3%4%5%6，并已求解")
               .arg(cname(ct)).arg(idA).arg(idB)
               .arg(axisId >= 0 ? QStringLiteral("（对称轴 #%1）").arg(axisId) : QString())
               .arg(value > 0 ? QStringLiteral("（目标值 %1）").arg(value, 0, 'f', 2) : QString())
               .arg(idxA >= 0 ? QStringLiteral("（idxA=%1 idxB=%2）").arg(idxA).arg(idxB) : QString()),
           "info");
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
// 快速选择命令（QSELECT）：四维过滤并选中匹配图元
// 交互 GUI 走 MainWindow::openQSelect()（QSelectDialog）；本命令为无头 / 脚本等价入口，
// 复用 QSelect 引擎（qselectByDims），不改动其 API 与语义。
// ============================================================
bool QSelectCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (!m) return false;

    // 逐维解析关键字（单行形式，等价于"逐维提示、回车=不过滤该维"的无头等价物）
    QString type, layer, color, linestyle;
    for (int i = 1; i < ctx.toks.size(); ++i) {
        const QString t = ctx.toks[i];
        const int eq = t.indexOf('=');
        if (eq <= 0) continue;                       // 跳过非法 token
        const QString key = t.left(eq).toLower();
        const QString val = t.mid(eq + 1).trimmed();
        if      (key == "type")      type = val;
        else if (key == "layer")     layer = val;
        else if (key == "color")     color = val;
        else if (key == "linestyle") linestyle = val;
        else m->log(QStringLiteral("快速选择：未知关键字 '%1'（可用 type/layer/color/linestyle）").arg(key), "warn");
    }

    // 全空（未指定任何维度）= 不执行选择，避免盲目全选，并给出用法提示。
    if (type.isEmpty() && layer.isEmpty() && color.isEmpty() && linestyle.isEmpty()) {
        m->log(QStringLiteral("快速选择：未指定任何过滤条件，未改变选择集。"
                              "用法: qselect type=直线 [layer=0] [color=#ff0000] [linestyle=实线]"), "warn");
        return true;   // 非致命：仅提示，不改变选择集
    }

    const std::vector<int> hits = qselectByDims(m, type, layer, color, linestyle);
    m->applySelectionDelta(hits, QSelectAction::Replace);   // 单次落地，一次 selectionChanged
    m->log(QStringLiteral("快速选择：命中 %1 个对象（已替换选择集）").arg(hits.size()));
    return true;
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
    // ---- 图层动态语义：冻结 / 打印 / 透明度（独立子命令 + set 字段双入口）----
    if (sub == "frozen") {
        if (t.size() < 3) { m->log("用法: layer frozen <名称> [on|off]", "warn"); return false; }
        bool on = (t.size() < 4) || (t[3].toLower() != "off" && t[3].toLower() != "0");
        return m->setLayerFrozen(t[2], on);
    }
    if (sub == "plot") {
        if (t.size() < 3) { m->log("用法: layer plot <名称> [on|off]", "warn"); return false; }
        bool on = (t.size() < 4) || (t[3].toLower() != "off" && t[3].toLower() != "0");
        return m->setLayerPlot(t[2], on);
    }
    if (sub == "alpha") {
        if (t.size() < 4) { m->log("用法: layer alpha <名称> <0..1>", "warn"); return false; }
        bool ok; double v = t[3].toDouble(&ok);
        if (!ok || v < 0.0 || v > 1.0) { m->log(QStringLiteral("无效透明度(需0..1): %1").arg(t[3]), "warn"); return false; }
        return m->setLayerAlpha(t[2], v);
    }
    if (sub == "set") {
        if (t.size() < 5) { m->log("用法: layer set <名称> <color|width|style|frozen|plot|alpha> <值>", "warn"); return false; }
        const QString name = t[2];
        const Layer* l = m->findLayer(name);
        if (!l) { m->log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
        const QString field = t[3].toLower();
        if (field == "color") {
            QColor nc = QColor(t[4]);
            if (!nc.isValid()) { m->log(QStringLiteral("无效颜色: %1").arg(t[4]), "warn"); return false; }
            return m->setLayerStyle(name, &nc, nullptr, nullptr);   // 只改颜色，同步本层颜色
        } else if (field == "width") {
            bool ok; double v = t[4].toDouble(&ok);
            if (!ok) { m->log(QStringLiteral("无效线宽: %1").arg(t[4]), "warn"); return false; }
            return m->setLayerStyle(name, nullptr, &v, nullptr);   // 只改线宽，同步本层线宽
        } else if (field == "style") {
            LineStyle ls = parseLineStyleArg(t[4]);
            return m->setLayerStyle(name, nullptr, nullptr, &ls);   // 只改线型，同步本层线型
        } else if (field == "frozen") {
            bool on = (t[4].toLower() != "off" && t[4].toLower() != "0");
            return m->setLayerFrozen(name, on);
        } else if (field == "plot") {
            bool on = (t[4].toLower() != "off" && t[4].toLower() != "0");
            return m->setLayerPlot(name, on);
        } else if (field == "alpha") {
            bool ok; double v = t[4].toDouble(&ok);
            if (!ok || v < 0.0 || v > 1.0) { m->log(QStringLiteral("无效透明度(需0..1): %1").arg(t[4]), "warn"); return false; }
            return m->setLayerAlpha(name, v);
        } else {
            m->log(QStringLiteral("未知字段: %1").arg(t[3]), "warn"); return false;
        }
    }
    m->log(QStringLiteral("未知图层命令: layer %1").arg(sub), "warn");
    return false;
}

// ============================================================
// 块（Block）命令族
// ============================================================

namespace {
// 选择集的世界包围盒（用于 block create 未指定基点时的默认基点 = 包围盒中心）
bool selectionBounds(SketchModel* m, QRectF& out) {
    bool any = false;
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    for (int id : m->selection()) {
        const Element* e = m->element(id);
        if (!e) continue;
        const QRectF b = geo::elementBounds(*e);
        if (b.isNull()) continue;
        x0 = std::min(x0, b.left());  y0 = std::min(y0, b.top());
        x1 = std::max(x1, b.right()); y1 = std::max(y1, b.bottom());
        any = true;
    }
    if (!any) return false;
    out = QRectF(x0, y0, x1 - x0, y1 - y0);
    return true;
}
} // namespace

bool BlockCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QStringList& t = ctx.toks;
    const QString cmd = t[0].toLower();

    // ---------------- attdef：为块定义追加属性定义 ----------------
    if (cmd == "attdef") {
        if (t.size() < 3) {
            m->log("用法: attdef <块名> <标签> [提示] [x,y] [字高]", "warn");
            return false;
        }
        BlockDef* d = m->findBlockDef(t[1]);
        if (!d) { m->log(QStringLiteral("块 %1 不存在").arg(t[1]), "warn"); return false; }
        AttributeDef a;
        a.tag = t[2];
        for (const auto& e : d->attdefs)
            if (e.tag == a.tag) { m->log(QStringLiteral("属性 %1 已存在").arg(a.tag), "warn"); return false; }
        if (t.size() >= 4) a.prompt = t[3];
        a.defaultValue = a.tag;                       // 默认值 = 标签（便于插入后立即辨认）
        Vec3 pos(0, 0, 0);
        if (t.size() >= 5) { Vec3 p; if (CommandContext::tryParsePoint(t[4], p)) pos = p; }
        a.pos = pos;
        if (t.size() >= 6) { bool ok; double h = t[5].toDouble(&ok); if (ok) a.height = h; }
        // 通过整份定义替换触发一次可撤销写入
        BlockDef copy = *d;
        copy.attdefs.push_back(a);
        m->redefineBlock(copy);
        m->log(QStringLiteral("块 %1 已新增属性定义 %2（位置 %3,%4，字高 %5）")
                   .arg(t[1]).arg(a.tag).arg(a.pos.x, 0, 'f', 1).arg(a.pos.y, 0, 'f', 1)
                   .arg(a.height, 0, 'f', 1));
        return true;
    }

    // ---------------- attedit：编辑某引用的属性值 ----------------
    if (cmd == "attedit") {
        if (t.size() < 4) {
            m->log("用法: attedit <引用id> <标签> <值>", "warn");
            return false;
        }
        bool ok; const int id = t[1].toInt(&ok);
        if (!ok) { m->log(QStringLiteral("无效的引用 id: %1").arg(t[1]), "warn"); return false; }
        // 值可含空格：从 t[3] 起拼接
        QString value = t[3];
        for (int i = 4; i < t.size(); ++i) value += QStringLiteral(" ") + t[i];
        return m->setBlockAttribute(id, t[2], value);
    }

    // ---------------- attlist：属性提取 ----------------
    if (cmd == "attlist") {
        auto show = [&](int id) {
            const auto attrs = m->extractBlockAttributes(id);
            if (attrs.empty()) return;
            QString line = QStringLiteral("块引用 #%1 属性: ").arg(id);
            for (const auto& a : attrs)
                line += QStringLiteral("[%1=%2] ").arg(a.tag, a.value);
            m->log(line);
        };
        if (t.size() >= 2) {
            bool ok; const int id = t[1].toInt(&ok);
            if (!ok) { m->log(QStringLiteral("无效的引用 id: %1").arg(t[1]), "warn"); return false; }
            show(id);
            return true;
        }
        int n = 0;
        for (const auto& e : m->elements()) {
            if (e.type != ElementType::BlockRef) continue;
            show(e.id);
            ++n;
        }
        if (n == 0) m->log(QStringLiteral("当前图纸没有块引用"), "warn");
        else        m->log(QStringLiteral("属性提取完成：%1 个块引用").arg(n));
        return true;
    }

    // ---------------- dynset：设置动态参数值 / 可见性状态 ----------------
    if (cmd == "dynset") {
        if (t.size() < 4) {
            m->log("用法: dynset <引用id> <参数名> <值|状态名>", "warn");
            return false;
        }
        bool ok; const int id = t[1].toInt(&ok);
        if (!ok) { m->log(QStringLiteral("无效的引用 id: %1").arg(t[1]), "warn"); return false; }
        const QString pname = t[2];
        const QString vraw  = t[3];
        // 优先按数值解释；若该参数是可见性（或数值解析失败且有同名状态），则按状态名处理
        bool numOk = false;
        const double num = vraw.toDouble(&numOk);
        const Element* ref = m->element(id);
        bool isVis = false;
        QString stateForVis;
        if (ref && ref->type == ElementType::BlockRef) {
            if (const BlockDef* d = m->findBlockDef(ref->blockName)) {
                for (const auto& p : d->dynParams) {
                    if (p.name != pname) continue;
                    if (p.type == DynParamType::Visibility) { isVis = true; stateForVis = vraw; }
                    break;
                }
            }
        }
        if (isVis) return m->setBlockDynState(id, pname, stateForVis);
        if (!numOk) {
            m->log(QStringLiteral("参数值「%1」既不是数值、也不是已声明的可见性状态").arg(vraw), "warn");
            return false;
        }
        return m->setBlockDynValue(id, pname, num);
    }

    // ---------------- dynlist：列出块定义的动态参数 ----------------
    if (cmd == "dynlist") {
        if (t.size() < 2) { m->log("用法: dynlist <块名>", "warn"); return false; }
        const BlockDef* d = m->findBlockDef(t[1]);
        if (!d) { m->log(QStringLiteral("块 %1 不存在").arg(t[1]), "warn"); return false; }
        if (d->dynParams.empty()) { m->log(QStringLiteral("块 %1 没有动态参数").arg(t[1])); return true; }
        for (const auto& p : d->dynParams) {
            QString s = QStringLiteral("动态参数 %1（%2）：影响 %3 个图元 默认 %4")
                            .arg(p.name, QString::fromUtf8(dynParamTypeName(p.type)))
                            .arg(p.affected.size()).arg(p.defaultValue, 0, 'f', 2);
            if (p.type == DynParamType::Visibility) {
                s += QStringLiteral(" 状态[");
                for (const auto& st : p.states) s += st.name + QStringLiteral(" ");
                s += QStringLiteral("]");
            }
            m->log(s);
        }
        return true;
    }

    // ---------------- block 子命令 ----------------
    if (t.size() < 2) {
        m->log("用法: block <create|insert|list|info|redefine|explode|delete|rename> ...", "warn");
        return false;
    }
    const QString sub = t[1].toLower();

    if (sub == "list") {
        if (m->blockCount() == 0) { m->log(QStringLiteral("当前没有块定义")); return true; }
        QString s = QStringLiteral("块表(%1): ").arg(m->blockCount());
        for (const auto& d : m->blockTable())
            s += QStringLiteral("[%1 图元%2 属性%3 参数%4] ")
                     .arg(d.name).arg(d.entities.size()).arg(d.attdefs.size()).arg(d.dynParams.size());
        m->log(s);
        return true;
    }

    if (sub == "info") {
        if (t.size() < 3) { m->log("用法: block info <块名>", "warn"); return false; }
        const BlockDef* d = m->findBlockDef(t[2]);
        if (!d) { m->log(QStringLiteral("块 %1 不存在").arg(t[2]), "warn"); return false; }
        m->log(QStringLiteral("块 %1：基点(%2,%3)，块内 %4 个图元，属性 %5，动态参数 %6%7")
                   .arg(d->name).arg(d->basePoint.x, 0, 'f', 2).arg(d->basePoint.y, 0, 'f', 2)
                   .arg(d->entities.size()).arg(d->attdefs.size()).arg(d->dynParams.size())
                   .arg(d->description.isEmpty() ? QString() : QStringLiteral(" 说明：") + d->description));
        for (const auto& a : d->attdefs)
            m->log(QStringLiteral("  属性 %1（%2）默认=%3 @(%4,%5) 字高%6")
                       .arg(a.tag, a.prompt, a.defaultValue)
                       .arg(a.pos.x, 0, 'f', 1).arg(a.pos.y, 0, 'f', 1).arg(a.height, 0, 'f', 1));
        return true;
    }

    if (sub == "create") {
        if (t.size() < 3) {
            m->log("用法: block create <块名> [基点x,y] [delete|keep]（delete 默认：用块引用替换原图元）", "warn");
            return false;
        }
        const QString name = t[2];
        bool baseSet = false, deleteSource = true;
        Vec3 base(0, 0, 0);
        for (int i = 3; i < t.size(); ++i) {
            const QString a = t[i].toLower();
            if (a == "delete") { deleteSource = true;  continue; }
            if (a == "keep")   { deleteSource = false; continue; }
            Vec3 p;
            if (CommandContext::tryParsePoint(t[i], p)) { base = p; baseSet = true; }
        }
        if (!baseSet) {
            QRectF b;
            if (selectionBounds(m, b)) base = Vec3(b.center().x(), b.center().y(), 0);
        }
        const bool existed = m->hasBlock(name);
        const int refId = m->createBlockFromSelection(name, base, deleteSource);
        if (!existed && m->hasBlock(name)) {
            m->log(QStringLiteral("块 %1 创建成功（基点 %2,%3%4）")
                       .arg(name).arg(base.x, 0, 'f', 2).arg(base.y, 0, 'f', 2)
                       .arg(deleteSource ? QStringLiteral("，已放置块引用") : QString()));
            (void)refId;
            return true;
        }
        return false;
    }

    if (sub == "redefine") {
        if (t.size() < 3) { m->log("用法: block redefine <块名>（用当前选择集替换块内几何）", "warn"); return false; }
        return m->redefineBlockFromSelection(t[2]);
    }

    if (sub == "insert") {
        if (t.size() < 4) {
            m->log("用法: block insert <块名> <x,y> [旋转角] [sx] [sy]", "warn");
            return false;
        }
        Vec3 pos;
        if (!CommandContext::tryParsePoint(t[3], pos)) {
            m->log(QStringLiteral("无效插入点: %1").arg(t[3]), "warn");
            return false;
        }
        double rot = 0, sx = 1, sy = 1;
        if (t.size() >= 5) {
            bool ok; const double v = t[4].toDouble(&ok);
            if (!ok) { m->log(QStringLiteral("无效旋转角: %1").arg(t[4]), "warn"); return false; }
            rot = v;
        }
        if (t.size() >= 6) { bool ok; const double v = t[5].toDouble(&ok); if (ok) sx = v; }
        if (t.size() >= 7) { bool ok; const double v = t[6].toDouble(&ok); if (ok) sy = v; }
        return m->insertBlock(t[2], pos, rot, sx, sy) >= 0;
    }

    if (sub == "explode") {
        if (t.size() < 3) { m->log("用法: block explode <引用id>", "warn"); return false; }
        bool ok; const int id = t[2].toInt(&ok);
        if (!ok) { m->log(QStringLiteral("无效的引用 id: %1").arg(t[2]), "warn"); return false; }
        return !m->explodeBlockRef(id).empty();
    }

    if (sub == "delete") {
        if (t.size() < 3) { m->log("用法: block delete <块名>", "warn"); return false; }
        return m->removeBlock(t[2]);
    }

    if (sub == "rename") {
        if (t.size() < 4) { m->log("用法: block rename <旧名> <新名>", "warn"); return false; }
        return m->renameBlock(t[2], t[3]);
    }

    m->log(QStringLiteral("未知块命令: block %1").arg(sub), "warn");
    return false;
}

// ============================================================
// 外部参照（Xref）：附加 / 分离 / 列表 / 路径诊断
// ============================================================
// 与 block 命令族同构（单一入口 + 子命令分派）。所有错误信息由 SketchModel 统一
// 产出并写日志，此处只做参数解析与结果判真，避免出现"两处各说一半"的错乱提示。
//
// ============================================================
// 剪贴板族（copyclip / copybase / pasteclip / pasteblock）
// ------------------------------------------------------------
// 数据链：选区 → clipboard::Payload → 版本化 JSON → QClipboard 自定义 mime。
// 粘贴：mime → JSON → Payload → pasteInto（合并块表 + Quiet 落盘，单步撤销）。
// 版本不符 / 坏 JSON / 无内容一律 log 提示并返回 false，绝不越界解析。
bool ClipCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QString& cmd = ctx.cmd;

    if (cmd == "copyclip" || cmd == "copybase") {
        std::vector<Element> src;
        for (int id : m->selection())
            if (const Element* e = m->element(id)) src.push_back(*e);
        if (src.empty()) { m->log("无可复制图元（请先选中）", "warn"); return false; }

        clipboard::Payload p;
        if (cmd == "copybase") {
            if (ctx.toks.size() < 2) { m->log("用法: copybase <x,y>", "warn"); return false; }
            if (!ctx.parsePoint(1, p.base)) { m->log("基点解析失败", "warn"); return false; }
        } else {
            // copyclip 基点 = 选区包围盒左下角（对齐 AutoCAD 默认基点语义）
            bool first = true;
            double minX = 0, minY = 0;
            for (const auto& e : src) {
                const QRectF b = geo::elementBounds(e);
                if (first) { minX = b.left(); minY = b.bottom(); first = false; }
                else { minX = std::min(minX, b.left()); minY = std::min(minY, b.bottom()); }
            }
            p.base = Vec3(minX, minY, 0);
        }
        p.elements = std::move(src);

        // 依赖的块定义随包携带（跨实例粘贴保留块表）：收集选区内 BlockRef 引用的
        // 块名，把它们（及其嵌套依赖）从块表拷出。实现收敛在 clipboard 层，
        // 命令行与 GUI 菜单共用同一份，语义不会漂移。
        p.blocks = clipboard::collectBlockDeps(*m, p.elements);

        if (!clipboard::putToClipboard(p)) { m->log("写入剪贴板失败", "warn"); return false; }
        m->log(QString("已复制 %1 个图元到剪贴板（基点 %2,%3%4）")
                   .arg(p.elements.size()).arg(p.base.x, 0, 'f', 2).arg(p.base.y, 0, 'f', 2)
                   .arg(p.blocks.empty() ? QString() : QStringLiteral("，含块 %1 个").arg(p.blocks.size())));
        return true;
    }

    if (cmd == "pasteclip" || cmd == "pasteblock") {
        clipboard::Payload p;
        QString err;
        if (!clipboard::getFromClipboard(&p, &err)) {
            m->log(err.isEmpty() ? QStringLiteral("剪贴板内容不可用") : err, "warn");
            return false;
        }
        Vec3 to(0, 0, 0);
        const int ptIdx = 1;
        if (ctx.toks.size() > ptIdx) {
            if (!ctx.parsePoint(ptIdx, to)) { m->log("落点解析失败", "warn"); return false; }
        } else if (cmd == "pasteblock") {
            m->log("用法: pasteblock <x,y>", "warn"); return false;
        }
        const bool asBlock = (cmd == "pasteblock");
        m->beginEdit(asBlock ? QStringLiteral("剪贴板-粘贴为块") : QStringLiteral("剪贴板-粘贴"));
        QString msg;
        if (!clipboard::pasteInto(m, p, to, asBlock, &msg)) {
            m->undo();   // 空转回滚：粘贴失败不留 beginEdit 快照
            m->log(msg.isEmpty() ? QStringLiteral("粘贴失败") : msg, "warn");
            return false;
        }
        m->log(msg);
        return true;
    }

    m->log("未知剪贴板子命令: " + cmd, "warn");
    return false;
}

bool XrefCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    const QStringList& t = ctx.toks;
    if (t.size() < 2) {
        m->log("用法: xref <attach|detach|list|path> ...", "warn");
        return false;
    }
    const QString sub = t[1].toLower();

    if (sub == "list") {
        if (m->xrefCount() == 0) { m->log(QStringLiteral("当前图纸没有外部参照")); return true; }
        m->log(QStringLiteral("外部参照(%1):").arg(m->xrefCount()));
        for (const auto& d : m->blockTable()) {
            if (!d.isXref || d.xrefNested) continue;
            m->log(QStringLiteral("  %1  [%2]  %3  图元%4  ← %5")
                       .arg(d.name)
                       .arg(QString::fromUtf8(xrefStatusName((XrefStatus)d.xrefStatus)))
                       .arg(QString::fromUtf8(xrefPathTypeName((XrefPathType)d.xrefPathType)))
                       .arg(d.entities.size())
                       .arg(d.xrefPath));
        }
        const int nestedCount = m->xrefCount(true) - m->xrefCount();
        if (nestedCount > 0)
            m->log(QStringLiteral("（另有 %1 个嵌套参照，只能在直接附加它的宿主图中分离）")
                       .arg(nestedCount));
        return true;
    }

    if (sub == "path") {
        if (t.size() < 3) { m->log("用法: xref path <块名>", "warn"); return false; }
        const QString p = m->xrefResolvedPath(t[2]);
        if (p.isEmpty()) { m->log(QStringLiteral("%1 不是外部参照").arg(t[2]), "warn"); return false; }
        m->log(QStringLiteral("外部参照 %1 的绝对路径：%2").arg(t[2], p));
        return true;
    }

    if (sub == "attach") {
        if (t.size() < 3) {
            m->log("用法: xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy]", "warn");
            return false;
        }
        XrefPathType pt = XrefPathType::FullPath;
        Vec3 ins(0, 0, 0);
        double rot = 0.0, sx = 1.0, sy = 1.0;
        for (int i = 3; i < t.size(); ++i) {
            bool asType = false;
            const XrefPathType cand = xrefPathTypeFromName(t[i], &asType);
            if (asType) { pt = cand; continue; }
            Vec3 p;
            if (CommandContext::tryParsePoint(t[i], p)) { ins = p; continue; }
            bool ok = false; const double v = t[i].toDouble(&ok);
            if (!ok) continue;                       // 无法识别的 token 忽略（保持宽松）
            if (rot == 0.0)      rot = v;             // 第 1 个裸数值 = 旋转角
            else if (sx == 1.0)  sx  = v;             // 第 2 个 = X 缩放
            else                 sy  = v;             // 第 3 个 = Y 缩放
        }
        return m->attachXref(t[2], pt, ins, rot, sx, sy) >= 0;
    }

    if (sub == "detach") {
        if (t.size() < 3) { m->log("用法: xref detach <块名>", "warn"); return false; }
        return m->detachXref(t[2]);
    }

    m->log(QStringLiteral("未知外部参照命令: xref %1").arg(sub), "warn");
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
    m->log("编辑: delete <id> / trim <id> <x,y> [边界id..] / undo / redo / clear");
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
    m->log("块: block create <块名> [基点x,y] [delete|keep]（从选择集建块，delete=用块引用替换原图元）");
    m->log("      block insert <块名> <x,y> [旋转] [sx] [sy] / block list / block info <块名>");
    m->log("      block redefine <块名>（用当前选择集重定义块，所有引用同步） / block explode <引用id>");
    m->log("      block delete <块名> / block rename <旧名> <新名>");
    m->log("属性块: attdef <块名> <标签> [提示] [x,y] [字高] / attedit <引用id> <标签> <值> / attlist [引用id]");
    m->log("动态块: dynset <引用id> <参数名> <值|状态名> / dynlist <块名>");
    m->log("查询: list [id..] / dist x1,y1 x2,y2 / area <封闭图元id> | x1,y1 x2,y2 [x3,y3..]");
    m->log("      massprop <封闭图元id> ... / id x,y（封闭区域含圆/椭圆/矩形/多边形/闭合折线/剖面区域）");
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

// ---- 约束自动生成命令（对接 ConstraintSolver 的几何关系「自动计算 + 生成」）----
// 生成的新图元已绑定 ctype/refA/refB，进入求解器维护范围，后续移动端点时约束被自动维持。
bool PerpCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：perp <直线Id> <x,y>"), QStringLiteral("error")); return false; }
    bool ok; int idA = ctx.toks[1].toInt(&ok);
    if (!ok) { m->log(QStringLiteral("perp：直线Id 无效"), QStringLiteral("error")); return false; }
    Vec3 P; if (!ctx.parsePoint(2, P)) { m->log(QStringLiteral("perp：点坐标无效"), QStringLiteral("error")); return false; }
    const Element* ref = nullptr;
    for (const auto& e : m->elements()) if (e.id == idA) { ref = &e; break; }
    if (!ref || ref->type != ElementType::Line) { m->log(QStringLiteral("perp：未找到指定直线"), QStringLiteral("error")); return false; }
    Element el = ConstraintSolver::drawPerpendicularTo(*ref, P);
    const int nid = m->addElement(el);
    m->log(QString("perp：已生成垂线 #%1（过 %2,%3 垂直于 #%4）").arg(nid).arg(P.x).arg(P.y).arg(idA), QStringLiteral("info"));
    return true;
}

bool Tangent2Command::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：tangent2 <圆IdA> <圆IdB>"), QStringLiteral("error")); return false; }
    bool ok1, ok2; int idA = ctx.toks[1].toInt(&ok1), idB = ctx.toks[2].toInt(&ok2);
    if (!ok1 || !ok2) { m->log(QStringLiteral("tangent2：圆Id 无效"), QStringLiteral("error")); return false; }
    const Element* A = nullptr; const Element* B = nullptr;
    for (const auto& e : m->elements()) { if (e.id == idA) A = &e; if (e.id == idB) B = &e; }
    if (!A || !B || (A->type != ElementType::Circle && A->type != ElementType::Arc) ||
        (B->type != ElementType::Circle && B->type != ElementType::Arc)) {
        m->log(QStringLiteral("tangent2：需两个圆/圆弧"), QStringLiteral("error")); return false;
    }
    std::vector<Element> outs;
    const int n = ConstraintSolver::commonTangents(*A, *B, outs);
    for (auto& el : outs) m->addElement(el);
    m->log(QString("tangent2：生成 %1 条公切线（#%2 ↔ #%3）").arg(n).arg(idA).arg(idB), QStringLiteral("info"));
    return true;
}

bool QConnectCommand::execute(const CommandContext& ctx) {
    SketchModel* m = ctx.model;
    if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：qconnect <目标圆Id> <x,y>"), QStringLiteral("error")); return false; }
    bool ok; int idT = ctx.toks[1].toInt(&ok);
    if (!ok) { m->log(QStringLiteral("qconnect：目标Id 无效"), QStringLiteral("error")); return false; }
    Vec3 from; if (!ctx.parsePoint(2, from)) { m->log(QStringLiteral("qconnect：点坐标无效"), QStringLiteral("error")); return false; }
    const Element* tgt = nullptr;
    for (const auto& e : m->elements()) if (e.id == idT) { tgt = &e; break; }
    if (!tgt) { m->log(QStringLiteral("qconnect：未找到目标图元"), QStringLiteral("error")); return false; }
    Element el = ConstraintSolver::quadrantConnector(*tgt, from);
    const int nid = m->addElement(el);
    m->log(QString("qconnect：已生成象限连接 #%1（从 %2,%3 连到 #%4 最近象限点）").arg(nid).arg(from.x).arg(from.y).arg(idT), QStringLiteral("info"));
    return true;
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

    // ---- 点实体（供 Node 节点捕捉：绘制离散点，可被 Node 捕捉吸附）----
    registerExact("point", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            int n = 0;
            for (int i = 1; i < ctx.toks.size(); ++i) {
                Vec3 p; if (!ctx.parsePoint(i, p)) continue;
                Element e; e.type = ElementType::Point; e.pts = {p};
                e.name = QStringLiteral("点"); e.color = QColor(0, 0, 0);
                m->addElement(e); ++n;
            }
            if (n == 0) { m->log(QStringLiteral("用法：point <x,y> [x,y ...]"), QStringLiteral("error")); return false; }
            m->log(QString("point：已创建 %1 个点实体（可被 Node 节点捕捉）").arg(n), QStringLiteral("info"));
            return true;
        }));

    // ---- 直线子模式（前缀分派，所有 line_* 共用一个 LineModeCommand） ----
    registerPrefix("line_", std::make_shared<LineModeCommand>());

    // ---- 构造线（Xline）：精确 "xline" + 前缀 "xline_" 家族共用 XlineCommand ----
    registerExact("xline",  std::make_shared<XlineCommand>());
    registerPrefix("xline_", std::make_shared<XlineCommand>());

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

    // ---- 查询测量（Inquiry）----
    registerExact("dist",     std::make_shared<DistCommand>());
    registerExact("area",     std::make_shared<AreaCommand>());
    registerExact("massprop", std::make_shared<MassPropCommand>());
    registerExact("id",       std::make_shared<IdCommand>());

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
        registerExact("chamfer",           mod);   // 倒角：距离-距离 / 距离-角度（参数化，走 corner 核心层）
        registerExact("explode",           mod);
        registerExact("align",             mod);
        registerExact("stretch",           mod);
        // ---- 批量编辑三件套（JOIN / 命令行阵列）----
        registerExact("join",              mod);   // 连合：同层 Line/Arc → Polyline
        registerExact("arrayrect",         mod);   // 矩形阵列（命令行等价于 GUI 阵列对话框）
        registerExact("arraypolar",        mod);   // 环形阵列（基点默认捕捉圆心）
        registerExact("arraypath",         mod);   // 路径阵列（沿 Polyline/Spline）
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
    // ---- 约束自动生成（几何关系自动计算 + 生成）----
    registerExact("perp",     std::make_shared<PerpCommand>());       // 过指定点作垂线
    registerExact("tangent2", std::make_shared<Tangent2Command>());   // 两圆公切线
    registerExact("qconnect", std::make_shared<QConnectCommand>());   // 象限连接

    // ---- 输入约束（正交 ORTHO / 栅格吸附 SNAP）：三入口之「命令」----
    // 与 F8/F9 快捷键、状态栏按钮写入同一个 SketchModel 状态源，状态自动同步。
    // ortho [on|off]：切换/设定正交模式（无参 = 取反）
    registerExact("ortho", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            bool on = !ctx.model->orthoMode();
            if (ctx.toks.size() >= 2) {
                const QString v = ctx.toks[1].toLower();
                if      (v == "on"  || v == "1" || v == "true")  on = true;
                else if (v == "off" || v == "0" || v == "false") on = false;
                else { ctx.model->log(QStringLiteral("用法：ortho [on|off]"), QStringLiteral("error")); return false; }
            }
            ctx.model->setOrthoMode(on);
            ctx.model->log(QStringLiteral("正交模式 (ORTHO)：%1")
                               .arg(on ? QStringLiteral("开") : QStringLiteral("关")), QStringLiteral("info"));
            return true;
        }));
    // snap [on|off|<步长>]：切换/设定栅格吸附（数字参数 = 同时设定吸附步长 mm）
    registerExact("snap", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            bool on = !ctx.model->gridSnap();
            if (ctx.toks.size() >= 2) {
                const QString v = ctx.toks[1].toLower();
                if      (v == "on"  || v == "1" || v == "true")  on = true;
                else if (v == "off" || v == "0" || v == "false") on = false;
                else {
                    bool ok = false;
                    const double step = v.toDouble(&ok);
                    if (!ok || step <= 0) {
                        ctx.model->log(QStringLiteral("用法：snap [on|off|<步长mm>]"), QStringLiteral("error"));
                        return false;
                    }
                    ctx.model->setGridSnapStep(step);
                    on = true;
                }
            }
            ctx.model->setGridSnap(on);
            ctx.model->log(QStringLiteral("栅格吸附 (SNAP)：%1（步长 %2 mm）")
                               .arg(on ? QStringLiteral("开") : QStringLiteral("关"))
                               .arg(ctx.model->gridSnapStep(), 0, 'f', 2), QStringLiteral("info"));
            return true;
        }));

    // dyn [on|off]：切换/设定动态输入 DYN（绘制命令期光标跟随的距离/角度输入框）
    registerExact("dyn", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            bool on = !ctx.model->dynMode();
            if (ctx.toks.size() >= 2) {
                const QString v = ctx.toks[1].toLower();
                if      (v == "on"  || v == "1" || v == "true")  on = true;
                else if (v == "off" || v == "0" || v == "false") on = false;
                else { ctx.model->log(QStringLiteral("用法：dyn [on|off]"), QStringLiteral("error")); return false; }
            }
            ctx.model->setDynMode(on);
            ctx.model->log(QStringLiteral("动态输入 (DYN)：%1")
                               .arg(on ? QStringLiteral("开") : QStringLiteral("关")), QStringLiteral("info"));
            return true;
        }));

    // ---- 追踪（极轴 POLAR / 对象捕捉追踪 OTRACK / 临时追踪点 TK）----
    // polar  [on|off|<角度>]：切换/设定极轴追踪（无参 = 取反；数字 = 同时设增量角）
    registerExact("polar", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            snap::TrackConfig c = ctx.model->trackConfig();
            if (ctx.toks.size() >= 2) {
                const QString v = ctx.toks[1].toLower();
                if (v == "on" || v == "1" || v == "true")        c.polar = true;
                else if (v == "off" || v == "0" || v == "false") c.polar = false;
                else {
                    bool ok = false;
                    const double inc = v.toDouble(&ok);
                    if (ok && inc > 0.0 && inc < 360.0) { c.polar = true; c.polarIncrement = inc; }
                    else { ctx.model->log(QStringLiteral("用法：polar [on|off|<增量角>]"), QStringLiteral("error")); return false; }
                }
            } else {
                c.polar = !c.polar;
            }
            ctx.model->setTrackConfig(c);
            ctx.model->log(QStringLiteral("极轴追踪 (POLAR)：%1（增量 %2°）")
                               .arg(c.polar ? QStringLiteral("开") : QStringLiteral("关"))
                               .arg(c.polarIncrement, 0, 'f', 0), QStringLiteral("info"));
            return true;
        }));
    // otrack [on|off]：切换/设定对象捕捉追踪
    registerExact("otrack", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            snap::TrackConfig c = ctx.model->trackConfig();
            if (ctx.toks.size() >= 2) {
                const QString v = ctx.toks[1].toLower();
                if (v == "on" || v == "1" || v == "true")        c.objectTrack = true;
                else if (v == "off" || v == "0" || v == "false") c.objectTrack = false;
                else { ctx.model->log(QStringLiteral("用法：otrack [on|off]"), QStringLiteral("error")); return false; }
            } else {
                c.objectTrack = !c.objectTrack;
            }
            ctx.model->setTrackConfig(c);
            ctx.model->log(QStringLiteral("对象捕捉追踪 (OTRACK)：%1")
                               .arg(c.objectTrack ? QStringLiteral("开") : QStringLiteral("关")), QStringLiteral("info"));
            return true;
        }));
    // tk <x,y>：放置临时追踪点（瞬态，不随图存档；参与对齐线追踪）
    registerExact("tk", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：tk <x,y>（放置临时追踪点）；tkclear 清除全部"), QStringLiteral("error"));
                return false;
            }
            Vec3 p;
            if (!ctx.parsePoint(1, p)) return false;
            ctx.model->addTrackPoint(p);
            ctx.model->log(QStringLiteral("已放置临时追踪点 (TK)：(%1, %2)，共 %3 个")
                               .arg(p.x, 0, 'f', 2).arg(p.y, 0, 'f', 2)
                               .arg(ctx.model->trackPoints().size()), QStringLiteral("info"));
            return true;
        }));
    // tkclear：清空全部临时追踪点
    registerExact("tkclear", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.model->trackPoints().empty()) {
                ctx.model->log(QStringLiteral("当前无临时追踪点"), QStringLiteral("info"));
                return true;
            }
            ctx.model->clearTrackPoints();
            ctx.model->log(QStringLiteral("已清除全部临时追踪点 (TK)"), QStringLiteral("info"));
            return true;
        }));

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

    // ---- 快速选择（QSELECT）：四维过滤引擎接线（别名 q → qselect 不再悬空）----
    // GUI 交互由 MainWindow::openQSelect() 打开 QSelectDialog；此处为命令行 / 脚本 / 无头等价入口。
    registerExact("qselect", std::make_shared<QSelectCommand>());

    // ---- group：命名编组（GROUP）命令行入口 ----
    // 编组 = groupName 相同的图元集合（引用式，无独立编组表），随图元 JSON 随图存档。
    //   group create <名>   用当前选中对象创建编组（Xref 块引用自动剔除）
    //   group add <名>      把当前选中对象加入编组
    //   group remove        把当前选中对象移出其所属编组
    //   group list          列出全部编组及成员数
    //   group ungroup <名>  解散编组（对象保留）
    registerExact("group", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            if (ctx.toks.size() < 2) {
                m->log(QStringLiteral("用法：group [create <名> | add <名> | remove | list | ungroup <名>]"),
                       QStringLiteral("error"));
                return false;
            }
            const QString sub = ctx.toks[1].toLower();
            if (sub == "create" || sub == "创建") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：group create <编组名>（先选中对象）"), QStringLiteral("error")); return false; }
                return m->createGroup(ctx.toks[2], m->selection());
            }
            if (sub == "add" || sub == "加入") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：group add <编组名>（先选中对象）"), QStringLiteral("error")); return false; }
                const int n = m->addToGroup(ctx.toks[2], m->selection());
                if (n <= 0) { m->log(QStringLiteral("没有对象被加入编组（检查是否已选中对象/已属该组）"), QStringLiteral("warn")); return false; }
                return true;
            }
            if (sub == "remove" || sub == "移除") {
                const int n = m->removeFromGroup(m->selection());
                if (n <= 0) { m->log(QStringLiteral("没有对象被移出编组（先选中编组成员）"), QStringLiteral("warn")); return false; }
                return true;
            }
            if (sub == "list" || sub == "列表") {
                const QStringList gs = m->groupNames();
                if (gs.isEmpty()) { m->log(QStringLiteral("当前没有编组"), QStringLiteral("info")); return true; }
                for (const QString& g : gs)
                    m->log(QStringLiteral("编组「%1」：%2 个对象").arg(g).arg(m->elementCountInGroup(g)),
                           QStringLiteral("info"));
                return true;
            }
            if (sub == "ungroup" || sub == "解散") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：group ungroup <编组名>（对象保留）"), QStringLiteral("error")); return false; }
                return m->ungroup(ctx.toks[2]) > 0;
            }
            m->log(QStringLiteral("未知子命令 %1（可用：create/add/remove/list/ungroup）").arg(sub), QStringLiteral("error"));
            return false;
        }));

    // ---- filter：FILTER 预选器（作用于随后一次选择操作，用后即清）----
    // 复用 QSelect 引擎判定（matchFilter，引擎 API 不改），四维条件同 qselect：
    //   filter type=直线 layer=0 color=#ff0000 linestyle=实线   设预选器（空维度忽略）
    //   filter                                                 查看当前预选器
    //   filter none|off|clear                                  清除预选器
    registerExact("filter", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            if (ctx.toks.size() < 2) {
                if (!m->hasActivePickFilter()) {
                    m->log(QStringLiteral("当前无 FILTER 预选器"), QStringLiteral("info"));
                } else {
                    QStringList desc;
                    for (const auto& c : m->activePickFilter().conds)
                        desc << QStringLiteral("%1=%2").arg(qselectPropName(c.prop), c.value);
                    m->log(QStringLiteral("当前预选器：%1（作用于下一次拾取/框选/栏选）").arg(desc.join(QStringLiteral(" AND "))),
                           QStringLiteral("info"));
                }
                return true;
            }
            const QString sub = ctx.toks[1].toLower();
            if (sub == "none" || sub == "off" || sub == "clear" || sub == "清除") {
                m->clearActivePickFilter();
                m->log(QStringLiteral("已清除 FILTER 预选器"), QStringLiteral("info"));
                return true;
            }
            // key=value 四维解析（与 qselect 同口径；缺省维度不限制）
            QString type, layer, color, lineStyle;
            for (int i = 1; i < ctx.toks.size(); ++i) {
                const QString t = ctx.toks[i];
                const int eq = t.indexOf('=');
                if (eq <= 0) continue;
                const QString k = t.left(eq).toLower();
                const QString v = t.mid(eq + 1).trimmed();
                if (v.isEmpty()) continue;
                if      (k == "type" || k == "类型")     type = v;
                else if (k == "layer" || k == "图层")    layer = v;
                else if (k == "color" || k == "颜色")    color = v;
                else if (k == "linestyle" || k == "线型") lineStyle = v;
            }
            if (type.isEmpty() && layer.isEmpty() && color.isEmpty() && lineStyle.isEmpty()) {
                m->log(QStringLiteral("用法：filter [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型] | none"),
                       QStringLiteral("error"));
                return false;
            }
            FilterDef f;
            f.matchAll = true;
            if (!type.isEmpty())      f.conds.push_back({QSelectProp::Type,      QSelectOp::Eq, type});
            if (!layer.isEmpty())     f.conds.push_back({QSelectProp::Layer,     QSelectOp::Eq, layer});
            if (!color.isEmpty())     f.conds.push_back({QSelectProp::Color,     QSelectOp::Eq, color});
            if (!lineStyle.isEmpty()) f.conds.push_back({QSelectProp::LineStyle, QSelectOp::Eq, lineStyle});
            m->setActivePickFilter(f);
            return true;
        }));

    // ---- layout：布局族（多布局 = 名字 + 页面设置 + 视口集，随图存档）----
    //   layout new <名>                 新建布局（A4 默认页面设置，视口空 = 打印时回落模型空间取景）
    //   layout set <名> | model         切换当前布局（model = 模型空间）
    //   layout list                     列出全部布局（含当前标记）
    //   layout rename <旧> <新>
    //   layout delete <名>
    //   layout copy <源> <新>           复制布局（页面设置与视口集一并复制）
    //   layout vpscale <视口名> <比例>  改当前布局中某视口比例（"1:2" 或 0.5；1:2 = 出图缩小一半）
    registerExact("layout", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            // 比例解析："a:b"（图纸:模型，1:2 → 0.5）或正小数（0.5）
            auto parseRatio = [](const QString& s, double& out) -> bool {
                const int colon = s.indexOf(':');
                if (colon > 0) {
                    bool okA = false, okB = false;
                    const double a = s.left(colon).toDouble(&okA);
                    const double b = s.mid(colon + 1).toDouble(&okB);
                    if (okA && okB && a > 0 && b > 0) { out = a / b; return true; }
                    return false;
                }
                bool ok = false;
                const double v = s.toDouble(&ok);
                if (ok && v > 0) { out = v; return true; }
                return false;
            };
            if (ctx.toks.size() < 2) {
                const QString cur = m->currentLayout().isEmpty()
                    ? QStringLiteral("模型空间") : m->currentLayout();
                m->log(QStringLiteral("当前：%1，共 %2 个布局（用法：layout new/set/list/rename/delete/copy/vpscale）")
                           .arg(cur).arg(m->layoutCount()), QStringLiteral("info"));
                return true;
            }
            const QString sub = ctx.toks[1].toLower();
            if (sub == "new" || sub == "新建") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：layout new <布局名>"), QStringLiteral("error")); return false; }
                if (!m->addLayout(ctx.toks[2], PrintSettings())) {
                    m->log(QStringLiteral("新建布局失败（名字为空或已存在）"), QStringLiteral("error"));
                    return false;
                }
                m->setCurrentLayout(ctx.toks[2]);
                return true;
            }
            if (sub == "set" || sub == "切换") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：layout set <布局名|model>"), QStringLiteral("error")); return false; }
                const QString nm = ctx.toks[2].toLower();
                if (nm == "model" || nm == "模型" || nm == "模型空间") {
                    m->setCurrentLayout(QString());
                    m->log(QStringLiteral("已切换到模型空间"), QStringLiteral("info"));
                    return true;
                }
                if (!m->setCurrentLayout(ctx.toks[2])) {
                    m->log(QStringLiteral("布局「%1」不存在").arg(ctx.toks[2]), QStringLiteral("error"));
                    return false;
                }
                return true;
            }
            if (sub == "list" || sub == "列表") {
                m->log(QStringLiteral("模型空间%1").arg(m->currentLayout().isEmpty() ? QStringLiteral("（当前）") : QString()),
                       QStringLiteral("info"));
                for (int i = 0; i < m->layoutCount(); ++i) {
                    const PlotLayoutDef* d = m->layoutAt(i);
                    m->log(QStringLiteral("布局「%1」：%2 个视口%3")
                               .arg(d->name).arg(d->settings.viewports.size())
                               .arg(m->currentLayout() == d->name ? QStringLiteral("（当前）") : QString()),
                           QStringLiteral("info"));
                }
                return true;
            }
            if (sub == "rename" || sub == "改名") {
                if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法：layout rename <旧名> <新名>"), QStringLiteral("error")); return false; }
                if (!m->renameLayout(ctx.toks[2], ctx.toks[3])) {
                    m->log(QStringLiteral("改名失败（旧名不存在/新名为空或重名）"), QStringLiteral("error"));
                    return false;
                }
                return true;
            }
            if (sub == "delete" || sub == "删除") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：layout delete <布局名>"), QStringLiteral("error")); return false; }
                if (!m->removeLayout(ctx.toks[2])) {
                    m->log(QStringLiteral("布局「%1」不存在").arg(ctx.toks[2]), QStringLiteral("error"));
                    return false;
                }
                return true;
            }
            if (sub == "copy" || sub == "复制") {
                if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法：layout copy <源布局> <新名>"), QStringLiteral("error")); return false; }
                const PlotLayoutDef* src = m->findLayout(ctx.toks[2]);
                if (!src) { m->log(QStringLiteral("布局「%1」不存在").arg(ctx.toks[2]), QStringLiteral("error")); return false; }
                if (!m->addLayout(ctx.toks[3], src->settings)) {
                    m->log(QStringLiteral("复制失败（新名为空或已存在）"), QStringLiteral("error"));
                    return false;
                }
                return true;
            }
            if (sub == "vpscale" || sub == "视口比例") {
                if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法：layout vpscale <视口名> <比例>（如 1:2 或 0.5）"), QStringLiteral("error")); return false; }
                const PlotLayoutDef* def = m->currentLayoutDef();
                if (!def) { m->log(QStringLiteral("当前处于模型空间（先 layout set <布局名>）"), QStringLiteral("error")); return false; }
                double sc = 0.0;
                if (!parseRatio(ctx.toks[3], sc)) {
                    m->log(QStringLiteral("比例格式错误（示例：1:2 → 出图缩小一半；2:1 → 放大一倍）"), QStringLiteral("error"));
                    return false;
                }
                PrintSettings s = def->settings;
                bool hit = false;
                for (auto& vp : s.viewports) {
                    if (vp.name.compare(ctx.toks[2], Qt::CaseInsensitive) == 0) {
                        vp.scale = sc;
                        hit = true;
                    }
                }
                if (!hit) { m->log(QStringLiteral("当前布局中无视口「%1」").arg(ctx.toks[2]), QStringLiteral("error")); return false; }
                m->updateLayout(def->name, s);
                m->log(QStringLiteral("视口「%1」比例 = %2（模型 mm → 图纸 mm）").arg(ctx.toks[2]).arg(sc, 0, 'f', 4),
                       QStringLiteral("info"));
                return true;
            }
            m->log(QStringLiteral("未知子命令 %1（可用：new/set/list/rename/delete/copy/vpscale）").arg(sub), QStringLiteral("error"));
            return false;
        }));

    // ---- mview：在当前布局中新建浮动视口（视口平铺的基础入口）----
    //   mview <图纸X> <图纸Y> <宽> <高> [比例]
    // paperRect 相对可打印区左上角、y 向下（mm）；比例 "1:2"/"0.5"，缺省 = 沿用布局整体比例。
    // 无当前布局 → 自动创建并切换到默认布局「布局1」。
    registerExact("mview", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            if (ctx.toks.size() < 5) {
                m->log(QStringLiteral("用法：mview <图纸X> <图纸Y> <宽> <高> [比例]，如 mview 10 10 90 60 1:2"),
                       QStringLiteral("error"));
                return false;
            }
            if (m->currentLayout().isEmpty()) {
                if (!m->hasLayout(QStringLiteral("布局1")))
                    m->addLayout(QStringLiteral("布局1"), PrintSettings());
                m->setCurrentLayout(QStringLiteral("布局1"));
                m->log(QStringLiteral("已自动创建并切换到布局「布局1」"), QStringLiteral("info"));
            }
            bool ok = false;
            const double px = ctx.toks[1].toDouble(&ok);  if (!ok) { m->log(QStringLiteral("参数 1 不是数字"), QStringLiteral("error")); return false; }
            const double py = ctx.toks[2].toDouble(&ok);  if (!ok) { m->log(QStringLiteral("参数 2 不是数字"), QStringLiteral("error")); return false; }
            const double pw = ctx.toks[3].toDouble(&ok);  if (!ok || pw <= 0) { m->log(QStringLiteral("视口宽度必须为正数"), QStringLiteral("error")); return false; }
            const double ph = ctx.toks[4].toDouble(&ok);  if (!ok || ph <= 0) { m->log(QStringLiteral("视口高度必须为正数"), QStringLiteral("error")); return false; }
            double sc = 0.0;   // 0 = 沿用布局整体比例（fit-to-page / 自定义）
            if (ctx.toks.size() >= 6) {
                const int colon = ctx.toks[5].indexOf(':');
                if (colon > 0) {
                    bool okA = false, okB = false;
                    const double a = ctx.toks[5].left(colon).toDouble(&okA);
                    const double b = ctx.toks[5].mid(colon + 1).toDouble(&okB);
                    if (!okA || !okB || a <= 0 || b <= 0) { m->log(QStringLiteral("比例格式错误（示例：1:2）"), QStringLiteral("error")); return false; }
                    sc = a / b;
                } else {
                    const double v = ctx.toks[5].toDouble(&ok);
                    if (!ok || v <= 0) { m->log(QStringLiteral("比例必须为正数"), QStringLiteral("error")); return false; }
                    sc = v;
                }
            }
            const PlotLayoutDef* def = m->currentLayoutDef();
            PrintSettings s = def->settings;
            PlotViewport vp;
            vp.name = QStringLiteral("视口%1").arg(s.viewports.size() + 1);
            vp.paperRect = QRectF(px, py, pw, ph);
            vp.scale = sc;
            s.viewports.append(vp);
            m->updateLayout(def->name, s);
            m->log(QStringLiteral("已在布局「%1」新建 %2：paper(%3, %4, %5×%6 mm)%7")
                       .arg(def->name, vp.name)
                       .arg(px, 0, 'f', 1).arg(py, 0, 'f', 1)
                       .arg(pw, 0, 'f', 1).arg(ph, 0, 'f', 1)
                       .arg(sc > 0 ? QStringLiteral("，比例 %1").arg(sc, 0, 'f', 4) : QStringLiteral("，比例沿用布局")),
                   QStringLiteral("info"));
            return true;
        }));

    // ---- 图层管理 ----
    registerExact("layer", std::make_shared<LayerCommand>());

    // ---- 图层动态语义：LTSCALE 全局线型比例 + LAYISO / LAYUNISO / LAYMCH 治理命令 ----
    // ltscale <比例>：设定全局线型比例（控制所有虚线/点画线/双点画线的线素长度）。
    //   0.5 → 线素缩短一半；2.0 → 线素拉长一倍。随图存档，不入撤销栈。
    registerExact("ltscale", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("当前全局线型比例 LTSCALE = %1")
                                   .arg(ctx.model->ltscale(), 0, 'f', 3), QStringLiteral("info"));
                return true;
            }
            bool ok = false;
            const double v = ctx.toks[1].toDouble(&ok);
            if (!ok || v <= 0.0) {
                ctx.model->log(QStringLiteral("用法：ltscale <正数>（如 0.5 / 1 / 2）"), QStringLiteral("error"));
                return false;
            }
            ctx.model->setLtscale(v);
            ctx.model->log(QStringLiteral("全局线型比例 LTSCALE = %1").arg(v, 0, 'f', 3), QStringLiteral("info"));
            return true;
        }));
    // layiso：隔离选中图元所属图层——冻结其余所有图层（冻结层零命中/零打印；layuniso 可逆）
    registerExact("layiso", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            QStringList keep;
            for (int id : m->selection()) {
                const Element* e = m->element(id);
                if (e && !keep.contains(e->layer)) keep.append(e->layer);
            }
            if (keep.isEmpty()) keep.append(m->currentLayer());   // 无选中：孤立当前活动层
            int n = 0;
            for (int i = 0; i < m->layerCount(); ++i) {
                const Layer& l = m->layerAt(i);
                if (!keep.contains(l.name) && m->setLayerFrozen(l.name, true)) ++n;
            }
            m->log(QStringLiteral("已隔离 %1 个图层（冻结其余 %2 个）").arg(keep.size()).arg(n), QStringLiteral("info"));
            return true;
        }));
    // layuniso：解冻所有图层（解除 layiso 的隔离）
    registerExact("layuniso", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            int n = 0;
            for (int i = 0; i < m->layerCount(); ++i) {
                const Layer& l = m->layerAt(i);
                if (l.frozen && m->setLayerFrozen(l.name, false)) ++n;
            }
            m->log(QStringLiteral("已解冻 %1 个图层").arg(n), QStringLiteral("info"));
            return true;
        }));
    // laymch <目标层>：把选中图元改属目标图层（图层匹配变更，大图治理高频命令）
    registerExact("laymch", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            if (ctx.toks.size() < 2) {
                m->log(QStringLiteral("用法：laymch <目标图层名>"), QStringLiteral("error"));
                return false;
            }
            const QString target = ctx.toks[1];
            if (!m->hasLayer(target)) {
                m->log(QStringLiteral("目标图层 %1 不存在").arg(target), QStringLiteral("warn"));
                return false;
            }
            const std::vector<int> ids = m->selection();
            if (ids.empty()) { m->log(QStringLiteral("没有选中任何图元"), QStringLiteral("warn")); return false; }
            const bool ok = m->setElementLayers(ids, target, /*adoptStyle=*/false);
            m->log(QStringLiteral("已将 %1 个图元改属图层 %2").arg(ids.size()).arg(target), QStringLiteral("info"));
            return ok;
        }));

    // ---- hatchedit：填充（剖面区域）参数编辑 —— 图案 / 比例 / 角度 / 关联开关 ----
    // 遵循「就地改字段、不重建 Hatch」既有范式：一次 beginEdit 覆盖全部选中填充，
    // 保留边界 pts/holes 与材质/颜色等；改边界+改填充 = 单步撤销。
    registerExact("hatchedit", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            std::vector<int> hids;
            for (int id : m->selection()) {
                const Element* e = m->element(id);
                if (e && e->type == ElementType::Hatch) hids.push_back(id);
            }
            if (hids.empty()) {
                m->log(QStringLiteral("请先选中至少一个剖面区域（填充）"), QStringLiteral("warn"));
                return false;
            }
            if (ctx.toks.size() < 2) {
                const Element* e = m->element(hids.front());
                m->log(QStringLiteral("剖面区域 #%1：图案=%2 比例=%3 角度=%4° 关联=%5%6")
                        .arg(hids.front())
                        .arg(e->hatchPattern.isEmpty() ? QStringLiteral("(按材料默认)") : e->hatchPattern)
                        .arg(e->hatchScale, 0, 'f', 3)
                        .arg(e->rotation, 0, 'f', 1)
                        .arg(e->hatchAssociative ? QStringLiteral("开") : QStringLiteral("关"))
                        .arg(e->hatchInvalid ? QStringLiteral("（边界已失效）") : QString()),
                        QStringLiteral("info"));
                return true;
            }
            const QString opt = ctx.toks[1].toLower();
            auto usage = [&]() {
                m->log(QStringLiteral("用法：hatchedit [scale <倍数> | pattern <名|none> | angle <度> | assoc on|off]（先选中剖面区域）"),
                       QStringLiteral("error"));
                return false;
            };
            QString pat; bool setPat = false;
            double scale = 0.0;                 // <=0 表示不动
            double angle = 0.0; bool setAngle = false;
            int assoc = -1;                     // -1 不动 / 0 关 / 1 开
            if (opt == "scale" || opt == "比例") {
                if (ctx.toks.size() < 3) return usage();
                bool ok = false; scale = ctx.toks[2].toDouble(&ok);
                if (!ok || scale <= 0.0) { m->log(QStringLiteral("比例须为正数"), QStringLiteral("error")); return false; }
            } else if (opt == "pattern" || opt == "pat" || opt == "图案") {
                if (ctx.toks.size() < 3) return usage();
                pat = ctx.toks[2]; setPat = true;
                if (pat.compare("none", Qt::CaseInsensitive) == 0 || pat == "-") pat.clear();
            } else if (opt == "angle" || opt == "角度") {
                if (ctx.toks.size() < 3) return usage();
                bool ok = false; angle = ctx.toks[2].toDouble(&ok);
                if (!ok) { m->log(QStringLiteral("角度须为数值"), QStringLiteral("error")); return false; }
                setAngle = true;
            } else if (opt == "assoc" || opt == "关联") {
                if (ctx.toks.size() < 3) return usage();
                const QString a = ctx.toks[2].toLower();
                if (a == "on" || a == "1" || a == "开") assoc = 1;
                else if (a == "off" || a == "0" || a == "关") assoc = 0;
                else { m->log(QStringLiteral("关联开关只接受 on/off"), QStringLiteral("error")); return false; }
            } else return usage();

            // 单步撤销：一次 beginEdit 覆盖"改图案/比例/角度/关联"（全部就地改）
            m->beginEdit(QStringLiteral("HATCHEDIT"));
            int n = 0;
            for (int id : hids) {
                const Element* cur = m->element(id);
                if (!cur || cur->type != ElementType::Hatch) continue;
                if (setPat || scale > 0.0 || assoc != -1) {
                    const QString  usePat   = setPat ? pat : cur->hatchPattern;
                    const double   useScale = (scale > 0.0) ? scale : cur->hatchScale;
                    m->setHatchPatternOverride(id, usePat, useScale, assoc);
                }
                if (setAngle) m->updateElementQuiet(id, [angle](Element& dst){ dst.rotation = angle; });
                ++n;
            }
            m->log(QStringLiteral("HATCHEDIT：已就地修改 %1 个剖面区域（单步撤销）").arg(n), QStringLiteral("info"));
            return n > 0;
        }));

    // ---- dimstyle：命名标注样式表（一图多标准 GB/T 4458.4 + ISO 129-1 并存）----
    // 图元经 dimStyleName 按名引用表中条目；改样式 → 引用它的标注实时联动（经 dimStyleFor 解析）。
    registerExact("dimstyle", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            SketchModel* m = ctx.model;
            // dimstyle（无参数 / list）→ 列出全部样式 + 当前样式
            if (ctx.toks.size() < 2 || ctx.toks[1].toLower() == "list") {
                QStringList lines;
                for (int i = 0; i < m->dimStyleCount(); ++i) {
                    const dim::DimStyleEntry* d = m->dimStyleAt(i);
                    if (!d) continue;
                    const bool cur = (d->name == m->currentDimStyle());
                    lines << QString("%1%2  字高 %3 / 箭头 %4 / 标准 %5")
                                 .arg(cur ? QStringLiteral("* ") : QString(), d->name)
                                 .arg(d->style.textHeight, 0, 'f', 2)
                                 .arg(d->style.arrowLen, 0, 'f', 2)
                                 .arg(d->style.standard() == DimensionStandard::ISO ? QStringLiteral("ISO 129-1") : QStringLiteral("GB/T 4458.4"));
                }
                m->log(QStringLiteral("标注样式表（* = 当前）：\n%1\n当前样式 = %2")
                           .arg(lines.join(QLatin1Char('\n')), m->currentDimStyle()), QStringLiteral("info"));
                return true;
            }
            const QString sub = ctx.toks[1].toLower();
            // dimstyle new <名> [gbt|iso] → 新建（可指定标准预设）
            if (sub == "new" || sub == "create") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：dimstyle new <名> [gbt|iso]"), QStringLiteral("error")); return false; }
                dim::DimStyleEntry d;
                d.name = ctx.toks[2];
                if (ctx.toks.size() >= 4) {
                    const std::string std_ = ctx.toks[3].toLower().toStdString();
                    if (std_ == "iso")      d.style.stdConf = DimensionStandardConfig::isoDefault();
                    else if (std_ == "gbt") d.style.stdConf = DimensionStandardConfig::gbtDefault();
                    else { m->log(QStringLiteral("标准只接受 gbt / iso"), QStringLiteral("error")); return false; }
                }
                return m->addDimStyle(d);
            }
            // dimstyle set <名> → 置当前（新建标注默认用当前样式）
            if (sub == "set" || sub == "current") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：dimstyle set <名>"), QStringLiteral("error")); return false; }
                return m->setCurrentDimStyle(ctx.toks[2]);
            }
            // dimstyle rename <旧名> <新名> → 重命名（同步更新引用）
            if (sub == "rename") {
                if (ctx.toks.size() < 4) { m->log(QStringLiteral("用法：dimstyle rename <旧名> <新名>"), QStringLiteral("error")); return false; }
                return m->renameDimStyle(ctx.toks[2], ctx.toks[3]);
            }
            // dimstyle apply <样式名> [图元id...] → 把选中/指定标注改引用该样式
            if (sub == "apply") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：dimstyle apply <样式名> [图元id...]"), QStringLiteral("error")); return false; }
                const QString name = ctx.toks[2];
                if (!m->hasDimStyle(name)) { m->log(QStringLiteral("标注样式 %1 不存在").arg(name), QStringLiteral("warn")); return false; }
                std::vector<int> ids;
                if (ctx.toks.size() >= 4) {
                    for (int i = 3; i < ctx.toks.size(); ++i) {
                        bool ok = false; const int id = ctx.toks[i].toInt(&ok);
                        if (ok) ids.push_back(id);
                    }
                } else {
                    for (int id : m->selection()) {
                        const Element* e = m->element(id);
                        if (e && (e->type == ElementType::Dimension || e->type == ElementType::MLeader)) ids.push_back(id);
                    }
                }
                if (ids.empty()) { m->log(QStringLiteral("没有可应用样式的标注（请先选中标注）"), QStringLiteral("warn")); return false; }
                m->beginEdit(QStringLiteral("应用标注样式 ") + name);
                int n = 0;
                for (int id : ids) {
                    const Element* e = m->element(id);
                    if (!e || (e->type != ElementType::Dimension && e->type != ElementType::MLeader)) continue;
                    m->updateElementQuiet(id, [&name](Element& dst) {
                        if (dst.type == ElementType::MLeader) dst.mleaderStyleName = name;
                        else                                        dst.dimStyleName = name;
                    });
                    ++n;
                }
                m->log(QStringLiteral("已将 %1 个标注改用样式 %2（随该样式联动）").arg(n).arg(name), QStringLiteral("info"));
                return n > 0;
            }
            // dimstyle delete <名> → 删除（引用者回落当前样式）
            if (sub == "delete" || sub == "del" || sub == "remove") {
                if (ctx.toks.size() < 3) { m->log(QStringLiteral("用法：dimstyle delete <名>"), QStringLiteral("error")); return false; }
                return m->removeDimStyle(ctx.toks[2]);
            }
            m->log(QStringLiteral("用法：dimstyle [list | new <名> [gbt|iso] | set <名> | rename <旧> <新> | apply <名> [id...] | delete <名>]"), QStringLiteral("error"));
            return false;
        }));

    // ---- 块（Block）命令族：块定义 / 插入 / 重定义 / 分解 / 属性块 / 动态块 ----
    {
        auto blk = std::make_shared<BlockCommand>();
        registerExact("block",   blk);   // block create|insert|list|info|redefine|explode|delete|rename
        registerExact("attdef",  blk);   // 属性定义（属性块）
        registerExact("attedit", blk);   // 属性值编辑
        registerExact("attlist", blk);   // 属性提取
        registerExact("dynset",  blk);   // 动态参数赋值 / 可见性状态切换
        registerExact("dynlist", blk);   // 动态参数列表
    }

    // ---- 剪贴板族：copyclip / copybase / pasteclip / pasteblock ----
    {
        auto clip = std::make_shared<ClipCommand>();
        registerExact("copyclip",  clip);
        registerExact("copybase",  clip);
        registerExact("pasteclip", clip);
        registerExact("pasteblock",clip);
    }

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

    // ---- 外部参照（Xref）：附加 / 分离 ----
    registerExact("xref", std::make_shared<XrefCommand>());

    // ---- 飞机三面图（航空制图模板）----
    registerExact("aircraft_views", std::make_shared<AircraftViewsCommand>());

    // ---- 帮助 ----
    registerExact("help", std::make_shared<HelpCommand>());

    // ---- 动作宏 / SCR 脚本（macrorec/macrostop/macrosave/macroshow/macroplay/scr）----
    // 传入 *this：registerBuiltins 运行在 instance() 静态初始化器内，不可再入。
    registerMacroCommands(*this);

    // ---- 命令别名（acad.pgp 兼容）与界面定制（CUIx）----
    registerAliasCommands(*this);
    registerCuiCommands(*this);
}

} // namespace SketchCAD
