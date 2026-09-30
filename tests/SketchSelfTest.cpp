// SketchCAD 无界面自测：约束求解器 + 对象捕捉
// 覆盖 21 种约束每类 ≥1 通过用例、8 类捕捉命中、冲突/过约束检测、约束注册表。
// 通过 build.sh test 或单独运行 SketchSelfTest 运行。
#include <QCoreApplication>
#include <cstdio>
#include <cmath>
#include <vector>
#include "Element.h"
#include "SketchModel.h"
#include "ConstraintSolver.h"
#include "Geometry.h"
#include "Snap.h"
#include "Vec.h"

// 核心类型均位于 SketchCAD 命名空间，测试代码中统一引入以简化书写。
using namespace SketchCAD;

static int g_pass = 0;
static int g_fail = 0;
#define CHECK(cond, msg) do {                                            \
    if (cond) { ++g_pass; }                                             \
    else { ++g_fail; fprintf(stderr, "  FAIL: %s  [%s:%d]\n",           \
                              msg, __FILE__, __LINE__); }                \
} while (0)

// ---------- 几何构造辅助 ----------
static int addLine(SketchModel& m, double x0, double y0, double x1, double y1) {
    Element e; e.type = ElementType::Line;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) };
    return m.addElement(e);
}
static int addCircle(SketchModel& m, double cx, double cy, double r) {
    Element e; e.type = ElementType::Circle;
    e.pts = { Vec3(cx, cy, 0) }; e.radius = r;
    return m.addElement(e);
}
static int addPoint(SketchModel& m, double x, double y) {
    Element e; e.type = ElementType::Point;
    e.pts = { Vec3(x, y, 0) };
    return m.addElement(e);
}
static int addAxis(SketchModel& m, double x0, double y0, double x1, double y1) {
    return addLine(m, x0, y0, x1, y1);   // 对称轴本质是一条直线
}
// 构造一条尺寸标注图元（refA/refB 引用被标注几何；dimValue 为初始驱动值，0 表示未设）
static int addDim(SketchModel& m, DimensionType dt, int refA, int refB = -1, double dimValue = 0.0) {
    Element e; e.type = ElementType::Dimension; e.dtype = dt;
    e.refA = refA; e.refB = refB; e.dimValue = dimValue;
    return m.addElement(e);
}

// 取刚添加的约束是否 satisfied（每个测试用独立 model，故最后一条即目标）
static bool lastSat(SketchModel& m) {
    const auto& cs = m.constraints();
    return !cs.empty() && cs.back().satisfied;
}

// ===================== 21 种约束用例 =====================
static bool t_coincident() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0); // A端→B端
    return e.isEmpty() && lastSat(m);
}
static bool t_parallel() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 3, 8);   // 不平行
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Parallel, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_perpendicular() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 10, 5);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Perpendicular, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_equal() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 0, 12); // 长度 10 vs 12
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Equal, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_tangent() {
    SketchModel m; int l = addLine(m, -5, 4.5, 5, 4.5); int c = addCircle(m, 0, 0, 5);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Tangent, l, c);
    return e.isEmpty() && lastSat(m);
}
static bool t_horizontal() {
    SketchModel m; int a = addLine(m, 0, 0, 1, 0);   // 占位（idA 必须存在，但本约束只作用于 idB）
    int b = addLine(m, 0, 3, 10, 6);                // 倾斜，待水平化
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Horizontal, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_vertical() {
    SketchModel m; int a = addLine(m, 0, 0, 1, 0);
    int b = addLine(m, 3, 0, 6, 10);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Vertical, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_fixed() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Fixed, a, a);  // 固定自身
    return e.isEmpty() && lastSat(m);
}
static bool t_concentric() {
    SketchModel m; int a = addCircle(m, 0, 0, 5); int b = addCircle(m, 2, 2, 8);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Concentric, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_symmetric() {
    SketchModel m; int a = addLine(m, 0, 2, 4, 2); int b = addLine(m, 0, 1, 4, 1);
    int ax = addAxis(m, -10, 0, 10, 0);
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, a, a).isEmpty()) return false;  // 锁住原型
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, ax, ax).isEmpty()) return false; // 锁住对称轴
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Symmetric, a, b, 0, ax);
    const Element* eb = m.element(b);
    return e.isEmpty() && lastSat(m) &&
           eb && eb->pts[0].distTo(Vec3(0, -2, 0)) < 0.5 && eb->pts[1].distTo(Vec3(4, -2, 0)) < 0.5;
}
static bool t_dimensional() {
    SketchModel m; int a = addLine(m, 0, 0, 1, 0);    // 占位
    int b = addLine(m, 0, 0, 10, 0);                  // 长度 10，目标 7
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Dimensional, a, b, 7.0);
    const Element* eb = m.element(b);
    return e.isEmpty() && lastSat(m) && eb && std::fabs(eb->pts[0].distTo(eb->pts[1]) - 7.0) < 0.5;
}
static bool t_collinear() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 4, 9); // 平行但不在同一线
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Collinear, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_equalLength() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 0, 13);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::EqualLength, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_equalRadius() {
    SketchModel m; int a = addCircle(m, 0, 0, 5); int b = addCircle(m, 20, 0, 9);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::EqualRadius, a, b);
    return e.isEmpty() && lastSat(m);
}
static bool t_pointOnLine() {
    SketchModel m; int p = addPoint(m, 5, 3); int l = addLine(m, 0, 0, 10, 0);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::PointOnLine, p, l);
    return e.isEmpty() && lastSat(m);
}
static bool t_pointOnCircle() {
    SketchModel m; int p = addPoint(m, 3, 0); int c = addCircle(m, 0, 0, 5);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::PointOnCircle, p, c);
    return e.isEmpty() && lastSat(m);
}
static bool t_fixedPoint() {
    // 锁定 A 的起点 (0,0)，再把 A 的终点约束到 B 的起点；起点必须保持不动
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
    QString e1 = ConstraintSolver::addConstraint(m, ConstraintType::FixedPoint, a, -1, 0, -1, 0, -1);
    if (!e1.isEmpty()) return false;
    QString e2 = ConstraintSolver::addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0);
    const Element* ea = m.element(a);
    return e2.isEmpty() && ea && ea->pts[0].distTo(Vec3(0, 0, 0)) < 1e-3;
}
static bool t_midpoint() {
    SketchModel m; int p = addPoint(m, 5, 3); int l = addLine(m, 0, 0, 10, 0); // 中点 (5,0)
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, l, l).isEmpty()) return false;  // 锁住直线
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Midpoint, p, l);
    const Element* ep = m.element(p);
    return e.isEmpty() && lastSat(m) && ep && ep->pts[0].distTo(Vec3(5, 0, 0)) < 0.5;
}
static bool t_distance() {
    SketchModel m; int a = addPoint(m, 0, 0); int b = addPoint(m, 10, 0);
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, a, a).isEmpty()) return false;  // 锁住 a 于原点
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Distance, a, b, 7.0);
    const Element* eb = m.element(b);
    return e.isEmpty() && lastSat(m) && eb && std::fabs(eb->pts[0].distTo(Vec3(0, 0, 0)) - 7.0) < 0.5;
}
static bool t_angle() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 0, 10, 0);
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, a, a).isEmpty()) return false;  // 锁住基准线
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Angle, a, b, 45.0);
    return e.isEmpty() && lastSat(m);
}
static bool t_symmetricPoint() {
    SketchModel m; int a = addPoint(m, 1, 1); int b = addPoint(m, 3, 1);
    int ax = addAxis(m, -10, 0, 10, 0);
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, a, a).isEmpty()) return false;    // 锁住 a
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Fixed, ax, ax).isEmpty()) return false;  // 锁住对称轴
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::SymmetricPoint, a, b, 0, ax);
    const Element* eb = m.element(b);
    return e.isEmpty() && lastSat(m) && eb && eb->pts[0].distTo(Vec3(1, -1, 0)) < 0.5;
}

// ===================== 冲突 / 自由度 / 注册表 =====================
static bool t_conflict() {
    SketchModel m; int a = addLine(m, 0, 0, 1, 0);   // 占位
    int b = addLine(m, 0, 0, 10, 0);                // 同一直线同时被要求水平与竖直 → 互斥
    ConstraintSolver::addConstraint(m, ConstraintType::Horizontal, a, b);
    ConstraintSolver::addConstraint(m, ConstraintType::Vertical, a, b);
    auto c = ConstraintSolver::detectConflicts(m);
    return !c.empty();
}
static bool t_solveDof() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
    ConstraintSolver::addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0);
    ConstraintSolver::addConstraint(m, ConstraintType::Parallel, a, b);
    auto rep = ConstraintSolver::solveAll(m);
    return rep.failures.empty() && rep.diag.dof >= 0;
}
// ---- 逐实体自由度与欠/过约束诊断 ----
static bool t_naturalDof() {
    SketchModel m;
    const int ln = addLine(m, 0, 0, 10, 0);
    const int ci = addCircle(m, 0, 0, 5);
    const int pt = addPoint(m, 1, 1);
    for (const auto& e : m.elements()) {
        if (e.id == ln && ConstraintSolver::naturalDof(e) != 4) return false;   // 线=4
        if (e.id == ci && ConstraintSolver::naturalDof(e) != 3) return false;   // 圆=3（圆心2+半径1）
        if (e.id == pt && ConstraintSolver::naturalDof(e) != 2) return false;   // 点=2
    }
    return true;
}
// 欠约束：B 线只加水平约束 → 固有4、已约束1、剩余3，且被精确定位
//（参照线 A 作为主动方也被纳入变量集，其列块秩为 0 → 同样报告为完全自由，符合真实 CAD 语义）
static bool t_diagnoseUnder() {
    SketchModel m; addLine(m, 0, 0, 10, 0);
    int b = addLine(m, 5, 5, 15, 5);
    QString e = ConstraintSolver::addConstraint(m, ConstraintType::Horizontal, m.elements()[0].id, b);
    if (!e.isEmpty()) return false;
    const auto d = ConstraintSolver::diagnose(m);
    if (d.state != ConstraintDiagnosis::UnderConstrained) return false;
    bool hasB = false;
    for (int id : d.underconstrainedIds) if (id == b) hasB = true;
    if (!hasB) return false;
    for (const auto& ei : d.elementInfo)
        if (ei.id == b &&
            !(ei.naturalDof == 4 && ei.activeVars == 4 && ei.appliedRank == 1 && ei.remainingDof == 3))
            return false;
    return !d.messages.empty();
}
// 完全约束：两端点 FixedPoint 锁死 → 无欠约束（V=0 分支）
static bool t_diagnoseWell() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0);
    QString e1 = ConstraintSolver::addConstraint(m, ConstraintType::FixedPoint, a, -1, 0, -1, 0, -1);
    QString e2 = ConstraintSolver::addConstraint(m, ConstraintType::FixedPoint, a, -1, 0, -1, 1, -1);
    if (!e1.isEmpty() || !e2.isEmpty()) return false;
    const auto d = ConstraintSolver::diagnose(m);
    return d.state == ConstraintDiagnosis::WellConstrained && d.underconstrainedIds.empty();
}
// 冲突：同一线段同时被要求长度=10 与长度=20 → 定位到该线并报告冲突
static bool t_diagnoseConflict() {
    SketchModel m;
    int ph1 = addLine(m, 0, 0, 1, 0);   // Dimensional 的占位主动方
    int ph2 = addLine(m, 0, 0, 1, 0);
    int ln = addLine(m, 0, 0, 10, 0);
    QString e1 = ConstraintSolver::addConstraint(m, ConstraintType::Dimensional, ph1, ln, 10.0);
    if (!e1.isEmpty()) return false;
    ConstraintSolver::addConstraint(m, ConstraintType::Dimensional, ph2, ln, 20.0);  // 矛盾：允许登记失败
    const auto d = ConstraintSolver::diagnose(m);
    if (d.state != ConstraintDiagnosis::Conflicting && d.state != ConstraintDiagnosis::Mixed) return false;
    bool found = false;
    for (int id : d.overconstrainedIds) if (id == ln) found = true;
    return found;
}
static bool t_descriptors() {
    auto cd = ConstraintSolver::constraintDescriptors();
    if ((int)cd.size() != kConstraintTypeCount) return false;
    for (const auto& d : cd) if (d.type != ConstraintType::Fixed && !d.name) return false;
    return true;
}
static bool t_snapDescriptors() {
    auto sd = snap::snapTypeDescriptors();
    // 14 种非 None 捕捉（端点/圆心/交点/切点/中点/象限/垂足/插入/节点/最近/外观/平行/延伸/两点中点）
    return sd.size() == 14;
}

// ===================== 8 类捕捉命中 =====================
static bool t_snapEndpoint() {
    SketchModel m; addLine(m, 0, 0, 10, 0);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.endpoint = true;
    auto r = snap::snap(elems, Vec3(0.1, 0.1, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Endpoint;
}
static bool t_snapCenter() {
    SketchModel m; addCircle(m, 5, 5, 3);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.center = true;
    auto r = snap::snap(elems, Vec3(5.1, 5.0, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Center;
}
static bool t_snapIntersection() {
    SketchModel m; addLine(m, 0, 0, 10, 0); addLine(m, 0, -5, 0, 10);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.endpoint = false; cfg.center = false; cfg.tangent = false; cfg.intersection = true;
    auto r = snap::snap(elems, Vec3(0.1, 0.1, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Intersection;
}
static bool t_snapMidpoint() {
    SketchModel m; addLine(m, 0, 0, 10, 0);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.midpoint = true;
    auto r = snap::snap(elems, Vec3(5.0, 0.2, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Midpoint;
}
static bool t_snapQuadrant() {
    SketchModel m; addCircle(m, 5, 5, 5);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.quadrant = true;
    auto r = snap::snap(elems, Vec3(10.0, 5.2, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Quadrant;
}
static bool t_snapPerpendicular() {
    SketchModel m; addLine(m, 0, 0, 10, 0);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.endpoint = false; cfg.center = false;
    cfg.tangent = false; cfg.intersection = false; cfg.perpendicular = true;
    auto r = snap::snap(elems, Vec3(3.0, 1.0, 0), nullptr, 2.0, cfg);   // 垂足 (3,0) 距光标 1.0 < tol
    return r.snapped && r.type == snap::SnapType::Perpendicular;
}
static bool t_snapTangent() {
    SketchModel m; addCircle(m, 0, 0, 5);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.tangent = true;
    Vec3 other(10, 0, 0);
    auto r = snap::snap(elems, Vec3(2.5, 4.3, 0), &other, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Tangent;
}
static bool t_snapNearest() {
    SketchModel m; addLine(m, 0, 0, 10, 0);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.nearest = true;
    auto r = snap::snap(elems, Vec3(3.0, 2.0, 0), nullptr, 2.0, cfg);
    return r.snapped && r.type == snap::SnapType::Nearest;
}

// ===================== 尺寸驱动 / 参数化建模（Request M）=====================
// 对象捕捉稳定性：光标在捕捉点附近抖动时，应稳定命中同一几何点；远离图元时不吸附且不崩溃。
static bool t_snapStable() {
    SketchModel m; addLine(m, 0, 0, 10, 0); addCircle(m, 5, 5, 3);
    std::vector<Element> elems = m.elements();
    snap::SnapConfig cfg; cfg.endpoint = true; cfg.center = true; cfg.intersection = true; cfg.midpoint = true;
    bool ok = true;
    const Vec3 pend(0, 0, 0);     // 直线起点端点
    for (const Vec3& j : { Vec3(0.05, 0.05, 0), Vec3(-0.1, 0.08, 0), Vec3(0.02, -0.1, 0), Vec3(0.0, 0.0, 0) }) {
        auto r = snap::snap(elems, j, nullptr, 1.0, cfg);
        if (!r.snapped || r.type != snap::SnapType::Endpoint || r.point.distTo(pend) > 1e-6) ok = false;
    }
    const Vec3 pctr(5, 5, 0);     // 圆心
    for (const Vec3& j : { Vec3(5.05, 5.0, 0), Vec3(4.95, 5.1, 0), Vec3(5.0, 4.92, 0) }) {
        auto r = snap::snap(elems, j, nullptr, 1.0, cfg);
        if (!r.snapped || r.type != snap::SnapType::Center || r.point.distTo(pctr) > 1e-6) ok = false;
    }
    // 远离任何图元：不应吸附
    auto r2 = snap::snap(elems, Vec3(100, 100, 0), nullptr, 1.0, cfg);
    if (r2.snapped) ok = false;
    return ok;
}

// 半径标注 → 驱动圆几何：修改标注值实时改变半径
static bool t_dimDriveRadius() {
    SketchModel m; int c = addCircle(m, 0, 0, 5);
    int d = addDim(m, DimensionType::Radius, c);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 8.0).isEmpty()) return false;
    const Element* ec = m.element(c);
    if (!ec || std::fabs(ec->radius - 8.0) > 1e-3) return false;
    return std::fabs(ConstraintSolver::measureDimension(m, d) - 8.0) < 1e-3;
}

// 直径标注 → 驱动圆几何：标注自然量是直径，内部按半径 (=直径/2) 解算
static bool t_dimDriveDiameter() {
    SketchModel m; int c = addCircle(m, 0, 0, 5);
    int d = addDim(m, DimensionType::Diameter, c);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 20.0).isEmpty()) return false;  // 直径 20 → 半径 10
    const Element* ec = m.element(c);
    if (!ec || std::fabs(ec->radius - 10.0) > 1e-3) return false;
    return std::fabs(ConstraintSolver::measureDimension(m, d) - 20.0) < 1e-3;
}

// 直线长度标注 → 驱动直线几何：修改标注值实时改变长度
static bool t_dimDriveLength() {
    SketchModel m; int l = addLine(m, 0, 0, 10, 0);   // 长度 10
    int d = addDim(m, DimensionType::Aligned, l);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 25.0).isEmpty()) return false;
    const Element* el = m.element(l);
    if (!el) return false;
    const double len = el->pts[0].distTo(el->pts[1]);
    return std::fabs(len - 25.0) < 1e-2 && std::fabs(ConstraintSolver::measureDimension(m, d) - 25.0) < 1e-2;
}

// 距离标注（两点）→ 驱动：固定其中一点，另一点移动到目标间距
static bool t_dimDriveDistance() {
    SketchModel m; int p1 = addPoint(m, 0, 0); int p2 = addPoint(m, 3, 0);  // 间距 3
    ConstraintSolver::addConstraint(m, ConstraintType::Fixed, p1, p1);  // 锁住 p1
    int d = addDim(m, DimensionType::Distance, p1, p2);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 10.0).isEmpty()) return false;
    const Element* ep2 = m.element(p2);
    if (!ep2) return false;
    const double dist = ep2->pts[0].distTo(Vec3(0, 0, 0));
    return std::fabs(dist - 10.0) < 1e-2;
}

// 角度标注（两直线）→ 驱动：修改角度实时改变夹角
static bool t_dimDriveAngle() {
    SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 10, 5);  // 平行（0°）
    int d = addDim(m, DimensionType::Angle, a, b);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 60.0).isEmpty()) return false;
    return std::fabs(ConstraintSolver::measureDimension(m, d) - 60.0) < 1e-2;
}

// 参数表联动：把命名参数驱动到多个关联标注（本例单个半径标注），一处改值全局更新
static bool t_paramLink() {
    SketchModel m; int c = addCircle(m, 0, 0, 5);
    int d = addDim(m, DimensionType::Radius, c);
    if (!ConstraintSolver::linkDimension(m, d, QStringLiteral("R")).isEmpty()) return false;  // 关联参数 "R"
    if (!ConstraintSolver::driveParameter(m, QStringLiteral("R"), 12.0).isEmpty()) return false;
    const Element* ec = m.element(c);
    if (!ec || std::fabs(ec->radius - 12.0) > 1e-3) return false;
    if (std::fabs(m.param(QStringLiteral("R")) - 12.0) > 1e-9) return false;
    // 二次驱动到另一值，验证联动可重复
    if (!ConstraintSolver::driveParameter(m, QStringLiteral("R"), 15.0).isEmpty()) return false;
    const Element* ec2 = m.element(c);
    return ec2 && std::fabs(ec2->radius - 15.0) < 1e-3;
}

// 驱动后一致性：几何被驱动到目标值，measureDimension 与约束目标一致（无过约束冲突）
static bool t_driveConsistency() {
    SketchModel m; int c = addCircle(m, 0, 0, 5);
    int d = addDim(m, DimensionType::Radius, c);
    if (!ConstraintSolver::linkDimension(m, d).isEmpty()) return false;
    if (!ConstraintSolver::setDimensionValue(m, d, 7.0).isEmpty()) return false;
    const Element* ec = m.element(c);
    if (!ec || std::fabs(ec->radius - 7.0) > 1e-3) return false;
    // 一致性校验：measureDimension 必须等于驱动值（闭环）
    if (std::fabs(ConstraintSolver::measureDimension(m, d) - 7.0) > 1e-3) return false;
    // 系统不应进入冲突/过约束态
    ConstraintDiagnosis diag = ConstraintSolver::diagnose(m);
    return diag.state != ConstraintDiagnosis::Conflicting && diag.state != ConstraintDiagnosis::OverConstrained;
}

// 按图元检索其参与的约束下标集合（约束栏/冲突高亮共用）
static bool t_constraintInvolving() {
    SketchModel m;
    int a  = addLine(m, 0, 0, 10, 0);
    int b  = addLine(m, 0, 5, 10, 5);
    int c  = addLine(m, 0, 10, 10, 10);
    int ax = addAxis(m, -10, 0, 10, 0);
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Parallel, a, b).isEmpty()) return false;
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Parallel, b, c).isEmpty()) return false;
    if (!ConstraintSolver::addConstraint(m, ConstraintType::Symmetric, a, c, 0, ax).isEmpty()) return false;
    auto inv  = ConstraintSolver::constraintsInvolving(m, a);   // {0, 2}
    auto invB = ConstraintSolver::constraintsInvolving(m, b);   // {0, 1}
    auto invC = ConstraintSolver::constraintsInvolving(m, c);   // {1, 2}
    auto invAx = ConstraintSolver::constraintsInvolving(m, ax); // {2}
    auto none  = ConstraintSolver::constraintsInvolving(m, 999);
    auto has = [](const std::vector<int>& v, int x) { for (int y : v) if (y == x) return true; return false; };
    bool ok = has(inv, 0) && has(inv, 2) && inv.size() == 2;
    ok &= has(invB, 0) && has(invB, 1) && invB.size() == 2;
    ok &= has(invC, 1) && has(invC, 2) && invC.size() == 2;
    ok &= has(invAx, 2) && invAx.size() == 1;
    ok &= none.empty();
    return ok;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    fprintf(stdout, "=== SketchCAD 约束/捕捉自测 ===\n");

    CHECK(t_coincident(),     "Coincident");
    CHECK(t_parallel(),       "Parallel");
    CHECK(t_perpendicular(),  "Perpendicular");
    CHECK(t_equal(),          "Equal");
    CHECK(t_tangent(),        "Tangent");
    CHECK(t_horizontal(),     "Horizontal");
    CHECK(t_vertical(),      "Vertical");
    CHECK(t_fixed(),          "Fixed");
    CHECK(t_concentric(),    "Concentric");
    CHECK(t_symmetric(),     "Symmetric");
    CHECK(t_dimensional(),   "Dimensional");
    CHECK(t_collinear(),     "Collinear");
    CHECK(t_equalLength(),   "EqualLength");
    CHECK(t_equalRadius(),   "EqualRadius");
    CHECK(t_pointOnLine(),   "PointOnLine");
    CHECK(t_pointOnCircle(), "PointOnCircle");
    CHECK(t_fixedPoint(),    "FixedPoint");
    CHECK(t_midpoint(),      "Midpoint");
    CHECK(t_distance(),      "Distance");
    CHECK(t_angle(),         "Angle");
    CHECK(t_symmetricPoint(),"SymmetricPoint");

    CHECK(t_conflict(),       "detectConflicts (互斥)");
    CHECK(t_solveDof(),       "solveAll DOF/收敛");
    CHECK(t_naturalDof(),     "naturalDof 固有自由度");
    CHECK(t_diagnoseUnder(),  "diagnose 欠约束定位");
    CHECK(t_diagnoseWell(),   "diagnose 完全约束");
    CHECK(t_diagnoseConflict(),"diagnose 冲突定位");
    CHECK(t_descriptors(),    "constraintDescriptors 注册表");
    CHECK(t_snapDescriptors(),"snapTypeDescriptors 注册表");
    CHECK(t_constraintInvolving(), "constraintsInvolving 命中");

    CHECK(t_snapEndpoint(),     "Snap Endpoint");
    CHECK(t_snapCenter(),       "Snap Center");
    CHECK(t_snapIntersection(), "Snap Intersection");
    CHECK(t_snapMidpoint(),     "Snap Midpoint");
    CHECK(t_snapQuadrant(),     "Snap Quadrant");
    CHECK(t_snapPerpendicular(), "Snap Perpendicular");
    CHECK(t_snapTangent(),      "Snap Tangent");
    CHECK(t_snapNearest(),      "Snap Nearest");
    CHECK(t_snapStable(),       "Snap Stable (抗抖动/不误吸)");

    CHECK(t_dimDriveRadius(),    "DimDrive Radius");
    CHECK(t_dimDriveDiameter(),  "DimDrive Diameter");
    CHECK(t_dimDriveLength(),    "DimDrive Length");
    CHECK(t_dimDriveDistance(),  "DimDrive Distance");
    CHECK(t_dimDriveAngle(),     "DimDrive Angle");
    CHECK(t_paramLink(),         "Param Link 参数表联动");
    CHECK(t_driveConsistency(),  "Drive 一致性校验");

    fprintf(stdout, "\n通过 %d / 失败 %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
