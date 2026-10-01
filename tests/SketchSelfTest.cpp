// SketchCAD 无界面自测：约束求解器 + 对象捕捉
// 覆盖 21 种约束每类 ≥1 通过用例、8 类捕捉命中、冲突/过约束检测、约束注册表。
// 通过 build.sh test 或单独运行 SketchSelfTest 运行。
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>
#include <cmath>
#include <vector>
#include "Element.h"
#include "SketchModel.h"
#include "ConstraintSolver.h"
#include "Geometry.h"
#include "Snap.h"
#include "Vec.h"
#include "CommandInterpreter.h"
#include "ScriptHost.h"      // 脚本宿主（二次开发）
#include "SdkContext.h"      // 原生 SDK 桥接（IApplication/IModel 实现）
#include "PluginManager.h"   // 插件管理器（注册/加载/生命周期）
#include "com/BatchPlotEngine.h"  // COM 自动化批量出图引擎（无头层，不经 COM 直测）
#include "MacroScript.h"     // 动作宏录制 / SCR 脚本批处理
#include "AliasTable.h"      // 命令别名（二次开发，acad.pgp 兼容）
#include "CuiManager.h"      // 界面定制（二次开发，CUIx 兼容）
#include "CommandPrompt.h"   // 分段命令选项提示（UX 增强）
#include "ShapeSelect.h"     // 栏选/圈围/圈交（第七阶段）
#include "GripOps.h"         // 夹点热操作（第七阶段）
#include <QDir>

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

// ===================== 序列化往返（重构安全网） =====================
// 构造覆盖各类专用字段的代表性图元，验证「序列化 → 反序列化 → 再序列化」得到的
// JSON 完全一致（即 elementFromJson 是 elementToJson 在已写字段上的精确逆运算）。
// 任何对序列化读/写逻辑的重构只要改变了既有行为，此用例都会立刻变红。
static bool t_serializeRoundTrip() {
    std::vector<Element> samples;
    { Element e; e.type = ElementType::Line; e.id = 1; e.name = "L"; e.layer = "0";
      e.color = QColor("#ff0000"); e.lineWidth = 2.0; e.pts = {Vec3(0,0,0), Vec3(10,5,0)}; samples.push_back(e); }
    { Element e; e.type = ElementType::Circle; e.id = 2; e.pts = {Vec3(3,4,0)}; e.radius = 5.0; e.color = QColor("#00ff00"); samples.push_back(e); }
    { Element e; e.type = ElementType::Arc; e.id = 3; e.pts = {Vec3(0,0,0)}; e.radius = 4; e.startAngle = 30; e.endAngle = 120; e.color = QColor("#0000ff"); samples.push_back(e); }
    { Element e; e.type = ElementType::Polyline; e.id = 4; e.pts = {Vec3(0,0,0),Vec3(10,0,0),Vec3(10,10,0),Vec3(0,10,0)};
      // 注意：避免 std::vector<bool> 的 initializer_list 赋值（GCC -Warray-bounds 误报，
      // 源于 vector<bool> 位打包内部的 std::copy/memmove），改用 push_back 逐个写入。
      e.arcFlags.clear();
      e.arcFlags.push_back(false); e.arcFlags.push_back(true);
      e.arcFlags.push_back(false); e.arcFlags.push_back(false);
      e.closed = true; samples.push_back(e); }
    { Element e; e.type = ElementType::Hatch; e.id = 5; e.rotation = 45; e.radius = 2.5; e.lineWidth = 0.5;
      e.holes = {{Vec3(1,1,0),Vec3(2,1,0),Vec3(2,2,0),Vec3(1,2,0)}}; samples.push_back(e); }
    { Element e; e.type = ElementType::MText; e.id = 6; e.pts = {Vec3(1,1,0)}; e.text = "hi";
      e.widthFactor = 1.2; e.textAlign = 2; e.styleName = "S"; e.lineSpacing = 1.4; e.mtextWidth = 30; samples.push_back(e); }
    { Element e; e.type = ElementType::Table; e.id = 7; e.pts = {Vec3(0,0,0)};
      e.tableRows = 3; e.tableCols = 4; e.rowHeight = 9; e.colWidths = {30,40,50,60}; e.tableCells = {"a","b","c","d"}; samples.push_back(e); }
    { Element e; e.type = ElementType::BlockRef; e.id = 8; e.pts = {Vec3(0,0,0)}; e.blockName = "B1"; e.scaleX = 2; e.scaleY = 3;
      e.blockAttrs = {{"tag1","v1"}}; e.dynValues = {{"d1",1.5,"on"}}; samples.push_back(e); }
    { Element e; e.type = ElementType::Dimension; e.id = 9; e.dtype = DimensionType::Aligned; e.refA = 1; e.refB = 2;
      e.dimValue = 12.5; e.dimDriven = true; e.dimParam = "p"; e.dimLinked = true; samples.push_back(e); }

    for (const auto& e : samples) {
        QJsonObject j  = elementToJson(e);
        Element     e2 = elementFromJson(j);
        QJsonObject j2 = elementToJson(e2);
        if (j != j2) return false;
    }
    return true;
}

// ===================== 脚本宿主（二次开发：JS/Python/VBA） =====================
// 验证策略模式脚本宿主：① 默认 JavaScript 引擎可用；② 加载/执行/与内部对象交互
// （app.addLine 等确实增删图元）；③ print 输出被捕获；④ 异常被捕获（ok=false+错误信息）；
// ⑤ 不支持的语言返回失败。Python/VBA 默认关闭，不在此强制验证（见 design-analysis 文档）。
static bool t_scriptHost() {
    SketchModel m;
    CommandInterpreter interp(&m);
    ScriptHost host(&m, &interp);

    // 1) 默认引擎可用：JavaScript
    if (!host.isLanguageAvailable("javascript")) return false;

    // 2) 加载 + 执行 + 内部对象交互：画两条线，图元数应 +2
    ScriptResult r = host.runCode("javascript",
        "app.addLine(0,0,10,0); app.addLine(0,0,0,10);");
    if (!r.ok) return false;
    if (m.elementCount() != 2) return false;

    // 3) print 输出捕获
    ScriptResult r2 = host.runCode("javascript", "print('hello', 123);");
    if (!r2.ok) return false;
    if (!r2.output.contains("hello 123")) return false;

    // 4) 异常处理：抛出错误应被捕获，ok=false 且有错误信息
    ScriptResult r3 = host.runCode("javascript", "throw new Error('boom');");
    if (r3.ok) return false;
    if (r3.error.isEmpty()) return false;

    // 5) 与命令行解释器联动：app.run 委托执行命令（脚本本身执行成功）
    ScriptResult r4 = host.runCode("javascript", "var ok = app.run('help');");
    if (!r4.ok) return false;

    // 6) 不支持的语言应返回失败（而非崩溃）
    ScriptResult r5 = host.runCode("cobol", "x");
    if (r5.ok) return false;

    return true;
}

// ===================== 原生 SDK 与插件框架 =====================
// 验证：① SdkApplication/SdkModel 桥接（IApplication/IModel 增删查图元、版本协商）；
//       ② 外部命令注册（重名/内置冲突拒绝、命令行触发、Tab 补全覆盖）；
//       ③ PluginManager 负例（坏路径拒绝并上报 pluginError）；
//       ④ 真实示例插件 DLL 全链路（ABI 校验 → create → initialize → 命令执行 → 卸载）。
//       ④ 仅当 build/plugins/SamplePlugin.dll 存在时验证（未构建则跳过，不算失败）。
static bool t_pluginManager() {
    using namespace SketchCAD::sdk;
    SketchModel m;
    CommandInterpreter interp(&m);

    // ---- 1) 桥接直测（无 DLL 依赖）----
    SdkApplication app(&m, &interp);
    if (app.sdkAbiVersion() != SKETCHCAD_SDK_ABI_VERSION) return false;
    if (app.sdkVersion() != SKETCHCAD_SDK_VERSION_INT) return false;
    IModel* im = app.model();
    if (!im) return false;

    const int before = m.elementCount();
    const int idLine = im->addLine(0, 0, 10, 0);
    const int idCirc = im->addCircle(0, 0, 5);
    if (idLine <= 0 || idCirc <= 0) return false;
    if (m.elementCount() != before + 2) return false;
    if (!im->elementExists(idLine)) return false;

    SdkElementInfo info;
    if (!im->elementInfo(idLine, &info)) return false;
    if (info.id != idLine) return false;
    if (qstrcmp(info.typeName, "直线") != 0) return false;   // UTF-8 类型名
    if (im->elementPointCount(idLine) != 2) return false;
    SdkPoint pt;
    if (!im->elementPoint(idLine, 1, &pt)) return false;
    if (pt.x != 10.0 || pt.y != 0.0) return false;
    if (!im->removeElement(idLine)) return false;
    if (im->elementExists(idLine)) return false;
    if (im->addText(0, 0, "标题", 3.5) <= 0) return false;
    im->log("SDK bridge ok");

    // ---- 2) 外部命令注册 → 命令行触发 ----
    static int hookCalls = 0;
    hookCalls = 0;
    if (!interp.registerExternalCommand("sdkhook", [](const QString& args) {
            if (args == "1 2") ++hookCalls;
            return true; })) return false;
    // 重名外部命令拒绝
    if (interp.registerExternalCommand("sdkhook", [](const QString&) { return true; })) return false;
    // 与内置命令冲突拒绝（line 是内置命令）
    if (interp.registerExternalCommand("line", [](const QString&) { return true; })) return false;
    // 命令行触发：args = "1 2"
    if (!interp.execute("sdkhook 1 2")) return false;
    if (hookCalls != 1) return false;
    // Tab 补全覆盖外部命令
    if (!interp.completions("sdkh").contains("sdkhook")) return false;

    // ---- 3) PluginManager 负例：坏路径拒绝并上报 pluginError ----
    PluginManager pm(&m, &interp);
    bool errSeen = false;
    QObject::connect(&pm, &PluginManager::pluginError,
                     [&](const QString&, const QString&) { errSeen = true; });
    if (pm.loadPlugin(QStringLiteral("no_such_plugin.dll"))) return false;
    if (!errSeen) return false;

    // ---- 4) 真实示例插件 DLL 全链路（存在才验）----
    const QString plugDir = QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
    QString plugPath;
    for (const char* ext : { ".dll", ".so", ".dylib" }) {
        const QString cand = plugDir + QStringLiteral("/SamplePlugin") + ext;
        if (QFileInfo::exists(cand)) { plugPath = cand; break; }
    }
    if (!plugPath.isEmpty()) {
        bool loaded = false;
        QObject::connect(&pm, &PluginManager::pluginLoaded,
                         [&](const QString&, const QString&) { loaded = true; });
        if (!pm.loadPlugin(plugPath)) return false;
        if (!loaded) return false;
        if (pm.loadedPlugins().size() != 1) return false;
        // 插件注册的 drawGear：默认 4 圆 + 12 齿线 = 16 图元
        const int cnt0 = m.elementCount();
        if (!interp.execute("drawGear")) return false;
        if (m.elementCount() - cnt0 != 16) return false;
        // 重复加载被拒绝
        if (pm.loadPlugin(plugPath)) return false;
        // 卸载：插件移出列表，已绘制的图元保留
        if (!pm.unloadPlugin(plugPath)) return false;
        if (!pm.loadedPlugins().isEmpty()) return false;
        if (m.elementCount() != cnt0 + 16) return false;
    } else {
        fprintf(stdout, "[skip] SamplePlugin 未构建，跳过插件 DLL 全链路验证\n");
    }

    // ---- 5) ABI 运行时协商负例：来自未来的探针插件（ABI=99）必须被拒绝 ----
    // 验证协商策略：插件 ABI > 宿主 ABI → 拒绝且错误消息报告双方版本；
    // 拒绝发生在解析工厂符号之前（探针刻意不导出 create_plugin）。
    const QString probePath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/abiprobe/AbiProbePlugin.dll");
    if (QFileInfo::exists(probePath)) {
        QString probeErr;
        QObject::connect(&pm, &PluginManager::pluginError,
                         [&](const QString&, const QString& m) { probeErr = m; });
        if (pm.loadPlugin(probePath)) return false;                        // 必须拒绝
        if (!probeErr.contains(QStringLiteral("99"))) return false;        // 报告插件 ABI
        if (!probeErr.contains(QStringLiteral("过新"))) return false;       // 拒绝语义
        if (!pm.loadedPlugins().isEmpty()) return false;                   // 未污染注册表
    } else {
        fprintf(stdout, "[skip] AbiProbePlugin 未构建，跳过 ABI 协商负例\n");
    }

    return true;
}

// ============================================================================
// COM 自动化批量出图引擎（无头层直测，不经 COM）：
//   引擎导出 JSON → 批量 PDF/DXF/JSON 三格式出图 → 事件序列 → 日志文件
//   → 负例（文件不存在 / 任务 JSON 非法）→ 错误码语义
// ============================================================================
static bool t_comAutomation() {
    using namespace SketchCAD::comauto;
    const QString tmp = QCoreApplication::applicationDirPath();

    // ---- 1) 构造源图纸（引擎自身导出 JSON，保证 source 与读档通路自洽）----
    const QString src = tmp + QStringLiteral("/scadauto_src.json");
    QFile::remove(src);
    SketchModel model;
    addLine(model, 0, 0, 100, 50);
    addCircle(model, 50, 25, 20);
    QString e;
    if (!BatchPlotEngine::exportDrawing(model, src, QJsonObject(), &e)) {
        fprintf(stderr, "  exportDrawing json 失败: %s\n", qPrintable(e));
        return false;
    }
    if (!QFileInfo(src).exists() || QFileInfo(src).size() <= 0) return false;

    // ---- 2) 事件计数 sink（验证 onTaskStart/onTaskDone/onBatchFinished 序列）----
    int evStart = 0, evDone = 0, evFin = 0;
    BatchPlotEngine::setEventSink([&](const QString& ev, const QJsonObject&) {
        if (ev == QLatin1String("onTaskStart"))       ++evStart;
        else if (ev == QLatin1String("onTaskDone"))   ++evDone;
        else if (ev == QLatin1String("onBatchFinished")) ++evFin;
    });

    // ---- 3) 批量：PDF / DXF / JSON 三格式出图 + 日志 ----
    const QString outPdf  = tmp + QStringLiteral("/scadauto_out.pdf");
    const QString outDxf  = tmp + QStringLiteral("/scadauto_out.dxf");
    const QString outJson = tmp + QStringLiteral("/scadauto_out.json");
    const QString logPath = tmp + QStringLiteral("/scadauto.log");
    QFile::remove(outPdf); QFile::remove(outDxf); QFile::remove(outJson); QFile::remove(logPath);

    QJsonArray tasks;
    auto mkTask = [&src](const QString& out) {
        QJsonObject t; t["source"] = src; t["output"] = out; return t;
    };
    tasks.append(mkTask(outPdf));
    tasks.append(mkTask(outDxf));
    tasks.append(mkTask(outJson));
    QJsonObject batch; batch["tasks"] = tasks;
    const BatchResult r = BatchPlotEngine::runBatch(
        QString::fromUtf8(QJsonDocument(batch).toJson(QJsonDocument::Compact)), logPath);
    if (!r.allOk) { fprintf(stderr, "  批量未全成: %s\n", qPrintable(r.tasks.isEmpty() ? QString() : r.tasks[0].error)); return false; }
    if (r.total != 3 || r.succeeded != 3 || r.failed != 0) return false;
    if (evStart != 3 || evDone != 3 || evFin != 1) return false;

    // PDF：非空且魔数 %PDF
    {
        QFile pf(outPdf);
        if (!pf.open(QIODevice::ReadOnly)) return false;
        const QByteArray head = pf.read(8);
        pf.close();
        if (!head.startsWith("%PDF")) return false;
    }
    if (QFileInfo(outDxf).size() <= 0) return false;
    if (QFileInfo(logPath).size() <= 0) return false;
    // JSON 往返：elements 数量一致（2）
    {
        QFile jf(outJson);
        if (!jf.open(QIODevice::ReadOnly)) return false;
        const QJsonDocument d = QJsonDocument::fromJson(jf.readAll());
        jf.close();
        if (!d.isObject()) return false;
        if (d.object()["elements"].toArray().size() != 2) return false;
    }

    // ---- 4) 负例：源文件不存在 → 单任务失败 + SCAD_E_FILE_NOT_FOUND ----
    QJsonObject bt;
    bt["source"] = tmp + QStringLiteral("/no_such_drawing.json");
    bt["output"] = outPdf;
    QJsonArray badTasks; badTasks.append(bt);
    QJsonObject badBatch; badBatch["tasks"] = badTasks;
    const BatchResult r2 = BatchPlotEngine::runBatch(
        QString::fromUtf8(QJsonDocument(badBatch).toJson(QJsonDocument::Compact)), QString());
    if (r2.allOk || r2.failed != 1 || r2.succeeded != 0) return false;
    if (!r2.tasks[0].error.contains(QLatin1String("SCAD_E_FILE_NOT_FOUND"))) return false;
    if (evStart != 4 || evDone != 4 || evFin != 2) return false;

    // ---- 5) 负例：任务列表 JSON 非法 → SCAD_E_BAD_TASK 语义 ----
    const BatchResult r3 = BatchPlotEngine::runBatch(QStringLiteral("{not json"), QString());
    if (r3.total != 1 || r3.failed != 1) return false;
    if (!r3.tasks[0].error.contains(QLatin1String("SCAD_E_BAD_TASK"))) return false;

    // ---- 6) 单步读档负例：STEP 只读不支持回读 ----
    {
        SketchModel m2;
        if (BatchPlotEngine::loadDrawing(m2, tmp + QStringLiteral("/x.step"), nullptr)) return false;
    }

    BatchPlotEngine::setEventSink({});   // 清理事件槽，避免影响后续用例
    return true;
}

// ===================== 动作宏录制 / SCR 脚本批处理 =====================
// 验证：① 录制捕获点（execute 成功路径，宏控制命令不入缓冲）；
//       ② .scr 保存/加载往返；③ 回放（新模型图元一致）；
//       ④ 注释/空行跳过；⑤ STOP 中断策略（失败即停、行号定位）；
//       ⑥ CONTINUE 模式（全部执行 + 汇总）；⑦ 录制/回放互斥与嵌套拒绝；
//       ⑧ scr 命令经 execute 的带引号路径解析。
static bool t_macroScript() {
    // ---- 1) 录制：捕获成功命令，宏控制命令不入缓冲 ----
    SketchModel m;
    CommandInterpreter interp(&m);
    MacroRecorder& rec = MacroRecorder::instance();
    rec.clear();

    if (!interp.execute("macrorec")) return false;              // 开始录制
    if (!rec.isRecording()) return false;
    if (!interp.execute("line 0,0 10,0")) return false;
    if (!interp.execute("circle 5,5 2")) return false;
    if (interp.execute("nosuchcmd 1,1")) return false;          // 未知命令：预期失败，不入缓冲
    if (!interp.execute("macroshow")) return false;             // 宏命令自身不入缓冲
    if (!interp.execute("macrostop")) return false;             // 停止录制
    if (rec.isRecording()) return false;
    if (rec.lines().size() != 2) return false;
    if (rec.lines()[0] != QLatin1String("line 0,0 10,0")) return false;
    if (rec.lines()[1] != QLatin1String("circle 5,5 2")) return false;

    // ---- 2) 保存 / 加载往返 ----
    const QString scrPath = QDir::tempPath() + QStringLiteral("/scad_macro_selftest.scr");
    QString err;
    if (!rec.save(scrPath, &err)) return false;
    rec.clear();
    if (!rec.lines().isEmpty()) return false;
    if (!rec.load(scrPath, &err)) return false;
    if (rec.lines().size() != 2) return false;
    if (rec.lines()[1] != QLatin1String("circle 5,5 2")) return false;

    // ---- 3) 回放：新模型图元一致（macrostop 后缓冲保留）----
    {
        SketchModel m2;
        CommandInterpreter interp2(&m2);
        if (!interp2.execute(QString("macroplay \"%1\"").arg(scrPath))) return false;
        if (m2.elementCount() != 2) return false;
    }

    // ---- 4) 注释/空行跳过 + 回放结果统计 ----
    const QString scriptText = QStringLiteral(
        "# 标题注释\n"
        "; 分号注释\n"
        "// 斜杠注释\n"
        "\n"
        "line 1,1 2,2\n"
        "circle 3,3 1\n");
    {
        SketchModel m3;
        CommandInterpreter interp3(&m3);
        ScriptRunResult r = ScriptRunner::runText(
            &m3, &interp3, scriptText, ScriptRunner::StopMode::StopOnError);
        if (!r.ok() || r.totalLines != 2 || r.executed != 2) return false;
        if (m3.elementCount() != 2) return false;
    }

    // ---- 5) STOP 模式：未知命令处中断，后续行不执行 ----
    const QString stopText = QStringLiteral(
        "line 0,0 10,0\n"
        "bogus_cmd 1,1\n"
        "line 0,0 5,5\n");
    {
        SketchModel m4;
        CommandInterpreter interp4(&m4);
        ScriptRunResult r = ScriptRunner::runText(
            &m4, &interp4, stopText, ScriptRunner::StopMode::StopOnError);
        if (r.ok()) return false;
        if (!r.stoppedEarly || r.totalLines != 2) return false;  // totalLines=已处理行数（中断后不再计）
        if (r.failed != 1 || r.executed != 1) return false;
        if (r.failedLineNo.size() != 1 || r.failedLineNo[0] != 2) return false;
        if (m4.elementCount() != 1) return false;   // 第 3 行未执行
    }

    // ---- 6) CONTINUE 模式：全部执行 + 失败汇总 ----
    {
        SketchModel m5;
        CommandInterpreter interp5(&m5);
        ScriptRunResult r = ScriptRunner::runText(
            &m5, &interp5, stopText, ScriptRunner::StopMode::ContinueOnError);
        if (r.ok()) return false;
        if (r.stoppedEarly) return false;
        if (r.totalLines != 3 || r.executed != 2 || r.failed != 1) return false;
        if (m5.elementCount() != 2) return false;   // 第 3 行已执行
    }

    // ---- 7) 互斥：录制中拒绝回放；回放中不捕获；重复录制拒绝 ----
    if (!interp.execute("macrorec")) return false;
    {
        SketchModel m6;
        CommandInterpreter interp6(&m6);
        ScriptRunResult r = ScriptRunner::runText(
            &m6, &interp6, scriptText, ScriptRunner::StopMode::StopOnError);
        if (!r.stoppedEarly || r.totalLines != 0) return false;   // 拒绝执行
        if (interp6.execute("macrorec")) return false;            // 重复开始被拒
    }
    MacroRecorder::setPlaybackActive(true);
    MacroRecorder::captureLine("line 9,9 9,9");                   // 回放期捕获被忽略
    MacroRecorder::setPlaybackActive(false);
    if (!interp.execute("macrostop")) return false;
    if (rec.lines().size() != 0) return false;                    // 互斥期无新捕获
    rec.clear();

    // ---- 8) scr 命令经 execute：带引号路径 + 文件含未知命令 → 命令失败 ----
    const QString badPath = QDir::tempPath() + QStringLiteral("/scad_macro_bad.scr");
    {
        QFile bf(badPath);
        if (!bf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        bf.write("line 0,0 1,1\nghost_cmd\n");
    }
    {
        SketchModel m7;
        CommandInterpreter interp7(&m7);
        if (interp7.execute(QString("scr \"%1\"").arg(badPath))) return false;  // STOP 模式失败
        if (m7.elementCount() != 1) return false;
        if (interp7.execute(QString("scr \"%1\" continue").arg(badPath))) return false; // CONTINUE 仍统计失败
        if (m7.elementCount() != 2) return false;                 // 两行都执行了
    }
    QFile::remove(badPath);
    QFile::remove(scrPath);

    rec.clear();
    return true;
}

// ===================== 命令别名机制（acad.pgp 兼容） =====================
static bool t_alias() {
    SketchCAD::AliasTable& at = SketchCAD::AliasTable::instance();
    at.clear();

    // ---- 1) 默认别名集合注入 ----
    auto d = SketchCAD::AliasTable::defaultAliases();
    if (d.size() < 23) return false;
    if (d["l"] != "line" || d["c"] != "circle" || d["a"] != "arc") return false;
    if (d["rec"] != "rect" || d["pl"] != "polyline" || d["pli"] != "polyline") return false;
    if (d["e"] != "delete" || d["co"] != "duplicate" || d["cp"] != "duplicate") return false;

    // 注入默认别名作为「当前活动表」
    for (auto it = d.begin(); it != d.end(); ++it) at.setAlias(it.key(), it.value());

    // ---- 2) 单字母 + 多字母解析（大小写不敏感）----
    if (at.resolve("L")   != "line")    return false;
    if (at.resolve("c")   != "circle")  return false;
    if (at.resolve("A")   != "arc")     return false;
    if (at.resolve("REC") != "rect")    return false;
    if (at.resolve("pl")  != "polyline") return false;
    if (at.resolve("PLI") != "polyline") return false;

    // ---- 3) 非别名 token 原样返回（统一小写）----
    if (at.resolve("polyline") != "polyline") return false;
    if (at.resolve("ZoomAll")  != "zoomall")  return false;

    // ---- 4) 运行时 setAlias / removeAlias ----
    at.setAlias("xx", "mycmd");
    if (at.resolve("xx") != "mycmd") return false;
    if (!at.contains("xx")) return false;
    if (!at.removeAlias("xx")) return false;
    if (at.resolve("xx") != "xx") return false;   // 已删除 → 原样返回

    // ---- 5) .pgp 文件解析（注释/空行跳过，逗号分隔）----
    const QString pgp = QDir::tempPath() + QStringLiteral("/scad_alias_test.pgp");
    {
        QFile f(pgp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write("; 这是注释行\n");
        f.write("# 这也是注释\n");
        f.write("\n");                 // 空行跳过
        f.write("L,LINE\n");
        f.write("rec,rect\n");
        f.write("custom,mycommand\n");
    }
    QString err;
    if (!at.load(pgp, &err)) return false;
    if (at.map().size() != 3) return false;          // 仅 3 条有效
    if (at.resolve("custom") != "mycommand") return false;
    if (at.filePath() != pgp) return false;

    // ---- 6) save → 重新 load 往返一致性 ----
    const QString pgp2 = QDir::tempPath() + QStringLiteral("/scad_alias_test2.pgp");
    if (!at.save(pgp2, &err)) return false;
    at.clear();
    if (!at.load(pgp2, &err)) return false;
    if (at.map().size() != 3) return false;
    if (at.resolve("L") != "line") return false;
    QFile::remove(pgp);
    QFile::remove(pgp2);
    at.clear();
    return true;
}

// ===================== 界面定制（CUIx 兼容） =====================
static bool t_cui() {
    SketchCAD::CuiManager& cm = SketchCAD::CuiManager::instance();
    cm.clear();
#define CK(cond) do { if(!(cond)){ fprintf(stderr, "  [CUI] FAIL @ %d: %s\n", __LINE__, #cond); return false; } } while(0)

    // ---- 1) 工具条 CRUD ----
    cm.addToolbar("tb1", "My Bar");
    CK(cm.hasToolbar("tb1"));
    CK(cm.toolbar("tb1")->title == "My Bar");
    cm.addButton("tb1", SketchCAD::CuiButton{"line", "Line", "draw-line"});
    cm.addButton("tb1", SketchCAD::CuiButton{"circle", QString(), QString()});
    CK(cm.toolbar("tb1")->buttons.size() == 2);
    CK(cm.toolbar("tb1")->buttons[0].command == "line");
    CK(cm.removeButton("tb1", 1));
    CK(cm.toolbar("tb1")->buttons.size() == 1);

    cm.addToolbar("tb2", "Other Bar");
    CK(cm.toolbarIds().size() == 2);

    // ---- 2) 工作空间与可见性规则 ----
    // 无当前工作空间 → 全部可见
    CK(cm.toolbarVisible("tb1") && cm.toolbarVisible("tb2"));
    CK(cm.menuVisible("menuDraw"));

    cm.addWorkspace("ws1", "WS1", QStringList{"tb1"}, QStringList{"menuDraw", "menuEdit"});
    cm.setCurrentWorkspace("ws1");
    CK(cm.currentWorkspace() == "ws1");
    CK(cm.toolbarVisible("tb1"));
    CK(!cm.toolbarVisible("tb2"));
    CK(cm.menuVisible("menuDraw"));
    CK(cm.menuVisible("menuEdit"));
    CK(!cm.menuVisible("menuHelp"));

    // ---- 3) XML(.cuix) 保存 → 重新加载 往返一致性 ----
    const QString cuix = QDir::tempPath() + QStringLiteral("/scad_cui_test.cuix");
    CK(cm.save(cuix, nullptr));
    cm.clear();                 // 清空内存，从文件重建
    CK(cm.load(cuix, nullptr));
    CK(cm.toolbarIds().size() == 2);
    CK(cm.hasToolbar("tb1"));
    CK(cm.toolbar("tb1")->buttons.size() == 1);
    CK(cm.toolbar("tb1")->buttons[0].command == "line");
    CK(cm.hasWorkspace("ws1"));
    CK(cm.currentWorkspace() == "ws1");
    CK(cm.workspace("ws1")->toolbars.size() == 1);
    CK(cm.workspace("ws1")->menus.size() == 2);
#undef CK
    QFile::remove(cuix);
    cm.clear();
    return true;
}

// ===================== 用户体验增强（命令提示/开关组/DYN 状态源） =====================
static bool t_cmdprompt() {
    SketchModel m;
    CommandInterpreter interp(&m);
#define CK(cond) do { if(!(cond)){ fprintf(stderr, "  [CMDPROMPT] FAIL @ %d: %s\n", __LINE__, #cond); return false; } } while(0)

    // ---- 1) CommandPrompt 纯函数：格式化 / 取键取标签 ----
    CK(CommandPrompt::formatOptionPrompt(QStringList()).isEmpty());
    CK(CommandPrompt::formatOptionPrompt(QStringList{"C/闭合", "U/放弃"})
       == QStringLiteral("[C/闭合 U/放弃]"));
    CK(CommandPrompt::optionKey("C/闭合") == "C");
    CK(CommandPrompt::optionLabel("C/闭合") == QStringLiteral("闭合"));
    CK(CommandPrompt::optionKey("CLOSE") == "CLOSE");      // 无 "/" → 原样为键

    // ---- 2) matchOption：英文键（大小写不敏感）/ 中文标签 / 未命中 ----
    const QStringList opts = {"C/闭合", "U/放弃", "L/长度"};
    CK(CommandPrompt::matchOption("c", opts) == "C");
    CK(CommandPrompt::matchOption("C", opts) == "C");
    CK(CommandPrompt::matchOption("u", opts) == "U");
    CK(CommandPrompt::matchOption("闭合", opts) == "C");   // 中文标签直输
    CK(CommandPrompt::matchOption("放弃", opts) == "U");
    CK(CommandPrompt::matchOption("x", opts).isEmpty());
    CK(CommandPrompt::matchOption("", opts).isEmpty());

    // ---- 3) 模型会话态：commandOptions / showLineWeight / dynMode 单一状态源 + 信号 ----
    int optSig = 0, lwtSig = 0, dynSig = 0;
    QObject::connect(&m, &SketchModel::commandOptionsChanged,
                     [&](const QStringList&) { ++optSig; });
    QObject::connect(&m, &SketchModel::showLineWeightChanged, [&](bool) { ++lwtSig; });
    QObject::connect(&m, &SketchModel::dynModeChanged, [&](bool) { ++dynSig; });

    m.setCommandOptions(opts);
    CK(m.commandOptions() == opts);
    m.setCommandOptions(opts);                    // 幂等：同值不发信号
    CK(optSig == 1);
    m.setCommandOptions(QStringList());
    CK(m.commandOptions().isEmpty() && optSig == 2);

    CK(!m.showLineWeight());                      // 默认关（LWDISPLAY）
    m.setShowLineWeight(true);
    CK(m.showLineWeight() && lwtSig == 1);
    m.setShowLineWeight(true);                    // 幂等
    CK(lwtSig == 1);
    m.setShowLineWeight(false);
    CK(lwtSig == 2);

    CK(m.dynMode());                              // 默认开
    m.setDynMode(false);
    CK(!m.dynMode() && dynSig == 1);
    m.setDynMode(false);                          // 幂等
    CK(dynSig == 1);
    m.setDynMode(true);
    CK(dynSig == 2);

    // ---- 4) lastCommand：成功执行记录（别名展开后）命令名 ----
    CK(interp.lastCommand().isEmpty());           // 初始为空
    CK(interp.execute("line 0,0 10,0"));
    CK(interp.lastCommand() == "line");
    AliasTable::instance().setAlias("zz", "circle");
    CK(interp.execute("zz 5,5 2"));               // 经别名展开执行
    CK(interp.lastCommand() == "circle");         // 记录展开后的命令名
    CK(!interp.execute("nosuchcmd 0,0"));         // 失败不更新
    CK(interp.lastCommand() == "circle");
    AliasTable::instance().removeAlias("zz");

    // ---- 5) dyn 命令 + completions 纳入别名键 ----
    CK(interp.execute("dyn off"));
    CK(!m.dynMode());
    CK(interp.execute("dyn"));                    // 无参取反
    CK(m.dynMode());
    AliasTable::instance().setAlias("qq", "qselect");
    const QStringList comps = interp.completions("q");
    CK(comps.contains("qq"));                     // 别名键参与 Tab 补全
    AliasTable::instance().removeAlias("qq");

#undef CK
    return true;
}

// ===================== 栏选/圈围/圈交 + 夹点热操作 =====================
static bool t_shapeselect() {
    using namespace shape;
#define CK(cond) do { if(!(cond)){ fprintf(stderr, "  [SHAPE] FAIL @ %d: %s\n", __LINE__, #cond); return false; } } while(0)
    CK(modeName(Mode::Fence) == QStringLiteral("栏选"));
    CK(modeName(Mode::Window) == QStringLiteral("圈围"));
    CK(modeName(Mode::Crossing) == QStringLiteral("圈交"));

    // 场景：lineA 完全在选择区外；circleB 完全在内；polyC 部分在内；lineD 横穿
    Element lineA; lineA.type = ElementType::Line; lineA.id = 1;
    lineA.pts = {Vec3(0, 0, 0), Vec3(3, 3, 0)};
    Element circleB; circleB.type = ElementType::Circle; circleB.id = 2;
    circleB.pts = {Vec3(8, 8, 0)}; circleB.radius = 2.0;
    Element polyC; polyC.type = ElementType::Polygon; polyC.id = 3;
    polyC.pts = {Vec3(10, 10, 0), Vec3(20, 10, 0), Vec3(15, 18, 0)};
    Element lineD; lineD.type = ElementType::Line; lineD.id = 4;
    lineD.pts = {Vec3(0, 10, 0), Vec3(30, 10, 0)};
    Element pointE; pointE.type = ElementType::Point; pointE.id = 5;
    pointE.pts = {Vec3(8, 8, 0)};
    std::vector<Element> elems = {lineA, circleB, polyC, lineD, pointE};

    // ---- 栏选：折线 (6,6)→(12,12)（沿 y=x），穿过圆心/三角顶点/横穿线/点 ----
    const std::vector<Vec3> fence = {Vec3(6, 6, 0), Vec3(12, 12, 0)};
    {
        const auto ids = pick(elems, fence, Mode::Fence, 1e-3);
        const auto has = [&](int id) {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        };
        CK(has(2) && has(3) && has(4) && has(5)); // 圆、三角形、横穿线(y=10 交于(10,10))、点
        CK(!has(1));                              // 完全在外(3,3) 端距栏线 4.24 不中
    }
    // ---- 圈围：矩形 (5,5)-(15,15)，只有整圆与点全包含 ----
    const std::vector<Vec3> wp = {Vec3(5, 5, 0), Vec3(15, 5, 0),
                                  Vec3(15, 15, 0), Vec3(5, 15, 0)};
    {
        const auto ids = pick(elems, wp, Mode::Window, 1e-3);
        CK(ids.size() == 2);                     // 圆 + 点
        CK(std::find(ids.begin(), ids.end(), 2) != ids.end());
        CK(std::find(ids.begin(), ids.end(), 5) != ids.end());
    }
    // ---- 圈交：同多边形，横穿线与部分相交三角形也命中 ----
    {
        const auto ids = pick(elems, wp, Mode::Crossing, 1e-3);
        const auto has = [&](int id) {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        };
        CK(has(2) && has(3) && has(4) && has(5));
        CK(!has(1));                             // 完全在外依旧不命中
    }
    // ---- 退化形状：Fence < 2 点、WP/CP < 3 点一律空 ----
    CK(pick(elems, {Vec3(8, 8, 0)}, Mode::Fence, 1e-3).empty());
    CK(pick(elems, {Vec3(0, 0, 0), Vec3(1, 1, 0)}, Mode::Window, 1e-3).empty());
    CK(pick(elems, {}, Mode::Crossing, 1e-3).empty());
    // ---- 线段化：圆逼近 24 段；直线 1 段 ----
    CK(segments(circleB).size() == 24);
    CK(segments(lineA).size() == 1);
#undef CK
    return true;
}

static bool t_gripops() {
    using namespace grip;
#define CK(cond) do { if(!(cond)){ fprintf(stderr, "  [GRIP] FAIL @ %d: %s\n", __LINE__, #cond); return false; } } while(0)
    // ---- 模式循环与命名 ----
    CK(nextMode(Mode::Stretch) == Mode::Move);
    CK(nextMode(Mode::Mirror) == Mode::Stretch);   // 5 模式闭环
    CK(modeName(Mode::Rotate) == QStringLiteral("旋转"));
    CK(modeName(Mode::Scale) == QStringLiteral("缩放"));

    // 场景：直线 id=1 (0,0)-(10,0)；圆 id=2 圆心 (5,5) r=2
    Element line; line.type = ElementType::Line; line.id = 1;
    line.pts = {Vec3(0, 0, 0), Vec3(10, 0, 0)};
    Element circle; circle.type = ElementType::Circle; circle.id = 2;
    circle.pts = {Vec3(5, 5, 0)}; circle.radius = 2.0;

    Grips g;
    g.base = Vec3(0, 0, 0);
    g.refCursor = Vec3(10, 0, 0);
    g.moveSet = {line, circle};
    g.stretchSet = {line};
    g.stretchRefs = {{1, 1}};    // 直线 1 号控制点（端点）

    // ---- 移动：delta = cursor − refCursor = (0,5) ----
    {
        auto r = apply(g, Mode::Move, Vec3(10, 5, 0));
        CK(r.reject.isEmpty() && r.changes.size() == 2);
        CK(r.changes[0].id == 1);
        CK(r.changes[0].replacement.pts[1].distTo(Vec3(10, 5, 0)) < 1e-9);
    }
    // ---- 旋转：refCursor 方位 0° → cursor 方位 90°，端点 (10,0)→(0,10) ----
    {
        auto r = apply(g, Mode::Rotate, Vec3(0, 10, 0));
        CK(r.reject.isEmpty());
        CK(std::fabs(r.changes[0].replacement.pts[1].x) < 1e-9);
        CK(std::fabs(r.changes[0].replacement.pts[1].y - 10.0) < 1e-9);
    }
    // ---- 缩放：|cursor−base| / |ref−base| = 2 → 端点 ×2、圆半径 ×2；base 不动 ----
    {
        auto r = apply(g, Mode::Scale, Vec3(20, 0, 0));
        CK(r.reject.isEmpty());
        CK(r.changes[0].replacement.pts[1].distTo(Vec3(20, 0, 0)) < 1e-9);   // 直线终点 ×2
        CK(r.changes[1].replacement.pts[0].distTo(Vec3(10, 10, 0)) < 1e-9);  // 圆心 (5,5)→(10,10)
        CK(std::fabs(r.changes[1].replacement.radius - 4.0) < 1e-9);         // 半径 ×2
        // 边界：cursor 压在 base 上 → factor 0 → 拒绝
        auto bad = apply(g, Mode::Scale, Vec3(0, 0, 0));
        CK(!bad.reject.isEmpty() && bad.changes.empty());
    }
    // ---- 镜像：轴 base→cursor = Y 轴；语义 = 保留源 + 新增副本(id=-1) ----
    {
        auto r = apply(g, Mode::Mirror, Vec3(0, 10, 0));
        CK(r.reject.isEmpty() && r.changes.size() == 2);
        CK(r.changes[0].id == -1);
        CK(r.changes[0].replacement.pts[1].distTo(Vec3(-10, 0, 0)) < 1e-9);
        // 边界：两点重合 → 镜像轴退化 → 拒绝
        auto bad = apply(g, Mode::Mirror, Vec3(0, 0, 0));
        CK(!bad.reject.isEmpty() && bad.changes.empty());
    }
    // ---- 拉伸：按 (图元, 控制点) 引用迁移端点，另一端与圆不受影响 ----
    {
        auto r = apply(g, Mode::Stretch, Vec3(5, 5, 0));
        CK(r.reject.isEmpty() && r.changes.size() == 1);
        CK(r.changes[0].id == 1);
        CK(r.changes[0].replacement.pts[1].distTo(Vec3(5, 5, 0)) < 1e-9);
        CK(r.changes[0].replacement.pts[0].distTo(Vec3(0, 0, 0)) < 1e-9);
    }
#undef CK
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);   // 无缓冲：进程异常挂起时也能看到已完成的用例
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
    CHECK(t_serializeRoundTrip(),   "serialize RoundTrip (序列化幂等)");

    CHECK(t_scriptHost(),           "ScriptHost 脚本宿主(JS 加载/执行/异常/日志)");
    CHECK(t_pluginManager(),        "PluginManager 原生SDK(桥接/外部命令/插件加载卸载)");
    CHECK(t_comAutomation(),        "BatchPlotEngine COM自动化(批量出图/事件/错误码/日志)");
    CHECK(t_macroScript(),          "MacroScript 动作宏/SCR脚本(录制/保存回放/中断策略/互斥)");
    CHECK(t_alias(),                "AliasTable 命令别名(acad.pgp 兼容/解析/重载/运行时)");
    CHECK(t_cui(),                  "CuiManager 界面定制(CUIx 工具条/工作空间/XML 往返)");
    CHECK(t_cmdprompt(),            "UX增强 命令提示/开关组/DYN(选项格式化/匹配/会话态/lastCommand/补全)");
    CHECK(t_shapeselect(),          "ShapeSelect 栏选/圈围/圈交(相交判定/全包含/退化形状/点近邻)");
    CHECK(t_gripops(),              "GripOps 夹点热操作(移动/旋转/缩放/镜像/拉伸+边界拒绝+模式循环)");

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
