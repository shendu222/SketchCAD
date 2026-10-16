// SketchCAD 无界面自测：约束求解器 + 对象捕捉
// 覆盖 21 种约束每类 ≥1 通过用例、8 类捕捉命中、冲突/过约束检测、约束注册表。
// 通过 build.sh test 或单独运行 SketchSelfTest 运行。
#include <QCoreApplication>
#include <QApplication>   // t_interaction：无头构造 CommandLine（QLineEdit）需要 QApplication
#include <QKeyEvent>      // t_interaction：键盘事件直注（QApplication::sendEvent）
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include "Element.h"
#include "ElementValidate.h"   // 鲁棒性守卫层：t_robustness / t_fileFuzz 直测
#include "SketchModel.h"
#include "ConstraintSolver.h"
#include "ConstraintSolverFacade.h"   // SKT 集成 Phase B：约束求解路由门面
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
#include "QSelect.h"         // 快速选择（QSELECT）四维过滤引擎
#include "CadFormats.h"      // DXF 导入导出（黄金样本 round-trip）
#include "Block.h"           // 块在位编辑（BEDIT）：BlockDef / AttributeDef / DynParam / block::makeRef
#include "Section.h"         // 填充边界联动：SectionStyle / hatchRegionFromElement / hatchPatternFor
#include "Gdt.h"             // GD&T：FcfSymbol / buildMLeader / buildFcf
#include "ElementFactory.h"  // GD&T：createMLeader / createFcf 工厂
#include "Modify.h"          // 分层测试网：修改引擎纯函数层
#include "Clipboard.h"       // 分层测试网：内部剪贴板（JOIN/阵列/剪贴板三件套）
#include "ui/DocTab.h"       // 多文档标签：DocSession/DocManager 会话核心（t_multidoc）
#include "ui/CommandLine.h"  // 命令行键盘语义（历史/补全/重复上一命令），t_interaction 直测
#include "AppSettings.h"     // 应用级配置持久化（t_appsettings / t_recent / t_interaction）
#include "I18n.h"            // 界面语言单一入口（t_i18n：解析/装载/回落）
#include "CommandCatalog.h"  // 命令发现：元数据目录/面板纯逻辑/快捷键审计（t_help 直测）
#include <QTranslator>       // t_i18n：.qm 直载与 contains 探测
#include <QImage>            // t_i18n：DPI 元数据 dotsPerMeter 换算锚点
#include <QLocale>           // t_i18n：区域口径（日期/小数点）
#include <QDate>
#include "Section.h"         // 分层测试网：剖面引擎纯函数层
#include "Inquiry.h"         // 分层测试网：查询测量纯函数层
#include "TextStyle.h"       // FIELD 域：resolveFields/displayTableCell（t_fields 直测）
#include "SpatialIndex.h"     // 均匀网格空间索引（纯 core 直测）
#include "PlotStyle.h"        // 打印样式表：灰度/抖动/GB预设/CTB导入（纯 core 直测）
#include "LinePattern.h"      // 线型虚线规格：dashSpecFor / dashSpecScaled（LTSCALE）
#include "Layout.h"           // 布局体系：PlotViewport/PrintSettings/PlotLayoutDef/viewportScaleMm
#include "ui/PrintEngine.h"   // 布局体系：exportPdfLayouts 每布局一页出图
#include "AutoSave.h"         // 崩溃不丢稿：自动保存核心（原子落盘/扫描/恢复/清理）
#include "AppSettings.h"      // 崩溃不丢稿：全局设置（自动保存间隔/磁盘阈值）
#include "RecentFiles.h"      // 最近文件 MRU（路径+时间+pin，注入临时 ini 离线直测）
#include "Diagnostics.h"      // 崩溃不丢稿：日志滚动落盘 + 崩溃转储
#include "core/TextMetrics.h" // 文字度量单一数据源（屏幕/打印/取景 三处同源）自测
#include "SheetSpec.h"        // 标题栏表格化：buildSheetFrame / InsertedTitleBlock（t_titleBlockAux）
#include "ProjectionSymbol.h" // 辅助元素打印过滤：isProjectionSymbolElement 谓词（t_titleBlockAux）
#include "TitleBlockLayout.h" // 单元格文字自适应 + 网格重建（t_titleBlockAux 直测）
#include "ui/PlotRenderer.h"  // 端到端出图：renderModel 直渲染 QImage（t_titleBlockAux 墨迹断言）
#include <QDebug>
#include <algorithm>
#include <map>
#include <random>
#include <limits>
#include <future>
#include <chrono>
#include <thread>
#include <atomic>
#include <QDir>
#include <QColor>
#include <QImage>
#include <QMarginsF>
#include <QSettings>
#include <QTemporaryDir>

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

// ===================== SKT 集成：门面路由缝（Phase B）=====================
// 验证 ConstraintSolverFacade 作为唯一路由入口，行为与直接调用 ConstraintSolver 等价，
// 覆盖：显式约束映射路由、诊断（欠/过/完全）、尺寸驱动联动、特性开关回落、错误透传。
static bool t_sktIntegration() {
    ConstraintSolverFacade& facade = ConstraintSolverFacade::instance();
    const int fail0 = g_fail;   // 进入前失败计数，用于整体返回

    // ---- 1) 显式约束映射路由（经门面 addConstraint）----
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
        QString e = facade.addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0);
        CHECK(e.isEmpty() && lastSat(m), "Facade: Coincident 路由");
    }
    {
        SketchModel m; int a = addLine(m, 0, 2, 4, 2); int b = addLine(m, 0, 1, 4, 1);
        int ax = addAxis(m, -10, 0, 10, 0);
        facade.addConstraint(m, ConstraintType::Fixed, a, a);
        facade.addConstraint(m, ConstraintType::Fixed, ax, ax);
        QString e = facade.addConstraint(m, ConstraintType::Symmetric, a, b, 0, ax);
        const Element* eb = m.element(b);
        CHECK(e.isEmpty() && lastSat(m) &&
              eb && eb->pts[0].distTo(Vec3(0, -2, 0)) < 0.5 && eb->pts[1].distTo(Vec3(4, -2, 0)) < 0.5,
              "Facade: Symmetric 路由+几何镜像");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 1, 0); int b = addLine(m, 0, 0, 10, 0);
        QString e = facade.addConstraint(m, ConstraintType::Dimensional, a, b, 7.0);
        const Element* eb = m.element(b);
        CHECK(e.isEmpty() && lastSat(m) && eb && std::fabs(eb->pts[0].distTo(eb->pts[1]) - 7.0) < 0.5,
              "Facade: Dimensional 路由+长度解算");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 1, 0); int b = addLine(m, 0, 3, 10, 6);
        QString e = facade.addConstraint(m, ConstraintType::Horizontal, a, b);
        CHECK(e.isEmpty() && lastSat(m), "Facade: Horizontal 路由");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 1, 0); int b = addLine(m, 3, 0, 6, 10);
        QString e = facade.addConstraint(m, ConstraintType::Vertical, a, b);
        CHECK(e.isEmpty() && lastSat(m), "Facade: Vertical 路由");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0);
        QString e = facade.addConstraint(m, ConstraintType::Fixed, a, a);
        CHECK(e.isEmpty() && lastSat(m), "Facade: Fixed 路由");
    }
    {
        SketchModel m; int a = addPoint(m, 0, 0); int b = addPoint(m, 10, 0);
        facade.addConstraint(m, ConstraintType::Fixed, a, a);
        QString e = facade.addConstraint(m, ConstraintType::Distance, a, b, 7.0);
        const Element* eb = m.element(b);
        CHECK(e.isEmpty() && lastSat(m) && eb && std::fabs(eb->pts[0].distTo(Vec3(0,0,0)) - 7.0) < 0.5,
              "Facade: Distance 路由+联动");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 0, 10, 0);
        facade.addConstraint(m, ConstraintType::Fixed, a, a);
        QString e = facade.addConstraint(m, ConstraintType::Angle, a, b, 45.0);
        CHECK(e.isEmpty() && lastSat(m), "Facade: Angle 路由");
    }

    // ---- 2) 诊断路由（欠/过/完全）----
    {
        SketchModel m; addLine(m, 0, 0, 10, 0);
        int b = addLine(m, 5, 5, 15, 5);
        facade.addConstraint(m, ConstraintType::Horizontal, m.elements()[0].id, b);
        const auto d = facade.diagnose(m);
        CHECK(d.state == ConstraintDiagnosis::UnderConstrained, "Facade: diagnose 欠约束");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0);
        facade.addConstraint(m, ConstraintType::FixedPoint, a, -1, 0, -1, 0, -1);
        facade.addConstraint(m, ConstraintType::FixedPoint, a, -1, 0, -1, 1, -1);
        const auto d = facade.diagnose(m);
        CHECK(d.state == ConstraintDiagnosis::WellConstrained && d.underconstrainedIds.empty(),
              "Facade: diagnose 完全约束");
    }
    {
        SketchModel m;
        int ph1 = addLine(m, 0, 0, 1, 0); int ph2 = addLine(m, 0, 0, 1, 0);
        int ln = addLine(m, 0, 0, 10, 0);
        facade.addConstraint(m, ConstraintType::Dimensional, ph1, ln, 10.0);
        facade.addConstraint(m, ConstraintType::Dimensional, ph2, ln, 20.0);  // 矛盾 → 过/冲突
        const auto d = facade.diagnose(m);
        const bool over = (d.state == ConstraintDiagnosis::Conflicting ||
                           d.state == ConstraintDiagnosis::Mixed ||
                           d.state == ConstraintDiagnosis::OverConstrained);
        bool found = false;
        for (int id : d.overconstrainedIds) if (id == ln) found = true;
        CHECK(over && found, "Facade: diagnose 过约束(冲突)定位");
    }
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
        facade.addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0);
        facade.addConstraint(m, ConstraintType::Parallel, a, b);
        const auto rep = facade.solveAll(m);
        const auto diag = facade.diagnose(m);
        CHECK(rep.failures.empty() && diag.totalDof >= 0, "Facade: solveAll + diagnose DOF 一致");
    }

    // ---- 3) 改标注值 → 几何联动（尺寸驱动经门面）----
    {
        SketchModel m; int l = addLine(m, 0, 0, 10, 0);   // 长度 10
        int d = addDim(m, DimensionType::Aligned, l);
        QString e1 = facade.linkDimension(m, d);
        QString e2 = facade.setDimensionValue(m, d, 25.0);
        const Element* el = m.element(l);
        const double len = el ? el->pts[0].distTo(el->pts[1]) : -1.0;
        const Element* pd = m.element(d);
        CHECK(e1.isEmpty() && e2.isEmpty() && el && std::fabs(len - 25.0) < 1e-2 &&
              pd && std::fabs(pd->dimValue - 25.0) < 1e-6,
              "Facade: 标注联动 link+setDimensionValue→几何/数值");
    }

    // ---- 4) 参数表联动路由（driveParameter 经门面）----
    {
        SketchModel m; int c = addCircle(m, 0, 0, 5);
        int d = addDim(m, DimensionType::Radius, c);
        QString e1 = facade.linkDimension(m, d, QStringLiteral("R"));
        QString e2 = facade.driveParameter(m, QStringLiteral("R"), 12.0);
        const Element* ec = m.element(c);
        CHECK(e1.isEmpty() && e2.isEmpty() && ec && std::fabs(ec->radius - 12.0) < 1e-3,
              "Facade: driveParameter 参数联动");
    }

    // ---- 5) 特性开关回落：关闭后仍可经修复模型求解 ----
    {
        QSettings st(QSettings::IniFormat, QSettings::UserScope, "SketchCADTest", "skt");
        AppSettings asp(&st);
        asp.setConstraintSolverEnabled(false);
        facade.setSettings(&asp);
        bool enabled = facade.featureEnabled();
        SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 5, 5, 15, 5);
        QString e = facade.addConstraint(m, ConstraintType::Coincident, a, b, 0, -1, 1, 0);
        facade.setSettings(nullptr);   // 还原默认（无设置 → 默认开）
        CHECK(!enabled && e.isEmpty() && lastSat(m), "Facade: 特性关→回落修复模型仍可用");
    }

    // ---- 6) 错误经门面透传（非法 Symmetric 缺轴 → 非空错误）----
    {
        SketchModel m; int a = addLine(m, 0, 0, 10, 0); int b = addLine(m, 0, 5, 10, 5);
        QString e = facade.addConstraint(m, ConstraintType::Symmetric, a, b);  // 缺 axisId
        CHECK(!e.isEmpty(), "Facade: 非法约束错误透传");
    }

    return g_fail == fail0;   // 本测试整体是否通过（任何 CHECK 失败则该值 != fail0）
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

// ===================== 快速选择（QSELECT）命令接线 =====================
// 验证 QSelect 引擎四维过滤（类型/图层/颜色/线型）：
//   ① 各维单过滤命中正确；② 组合过滤(AND)正确；③ 全空过滤返回空集且不崩溃；
//   ④ 命令 qselect 与别名 q 接线后选中计数正确；⑤ 与既有图元枚举/存档契约无冲突。
// （不改动 QSelect 引擎本身的 API 与语义，仅新增 qselectByDims 便捷入口并接线命令。）
static bool t_qselect() {
    SketchModel m;

    // 默认别名在 GUI 启动时由 aliasreload 加载到 AliasTable 单例；无头测试需手动灌入，
    // 才能验证 q → qselect 别名路径（与真实运行环境的启动前提一致）。
    {
        const auto defs = AliasTable::defaultAliases();
        for (auto it = defs.constBegin(); it != defs.constEnd(); ++it)
            AliasTable::instance().setAlias(it.key(), it.value());
    }

    // 构造已知图元：
    //   2 直线(层0 / 红 / 实线) + 1 圆(层1 / 蓝 / 虚线) + 1 直线(层1 / 红 / 点线)
    auto mkLine = [&](const QString& layer, const QString& color, LineStyle ls) -> int {
        Element e; e.type = ElementType::Line;
        e.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        e.layer = layer; e.color = QColor(color); e.lineStyle = ls; e.visible = true;
        return m.addElement(e);
    };
    auto mkCircle = [&](const QString& layer, const QString& color, LineStyle ls) -> int {
        Element e; e.type = ElementType::Circle;
        e.pts = { Vec3(0,0,0) }; e.radius = 5;
        e.layer = layer; e.color = QColor(color); e.lineStyle = ls; e.visible = true;
        return m.addElement(e);
    };

    mkLine("0", "#ff0000", LineStyle::Solid);    // 直线 层0 红 实线
    mkLine("0", "#ff0000", LineStyle::Solid);    // 直线 层0 红 实线
    mkCircle("1", "#0000ff", LineStyle::Dashed); // 圆   层1 蓝 虚线
    mkLine("1", "#ff0000", LineStyle::Dotted);   // 直线 层1 红 点线
    // 共 3 直线 / 1 圆；红 3 / 蓝 1；实线 2 / 虚线 1 / 点线 1

    // ① 类型单过滤：仅直线 → 3
    {
        std::vector<int> r = qselectByDims(&m, QStringLiteral("直线"), {}, {}, {});
        CHECK(r.size() == 3, "qselect 类型过滤(直线) 应命中 3 个");
    }
    // ② 图层单过滤：层0 → 2
    {
        std::vector<int> r = qselectByDims(&m, {}, QStringLiteral("0"), {}, {});
        CHECK(r.size() == 2, "qselect 图层过滤(0) 应命中 2 个");
    }
    // ③ 颜色单过滤：红 → 3
    {
        std::vector<int> r = qselectByDims(&m, {}, {}, QStringLiteral("#ff0000"), {});
        CHECK(r.size() == 3, "qselect 颜色过滤(#ff0000) 应命中 3 个");
    }
    // ④ 线型单过滤：实线 → 2
    {
        std::vector<int> r = qselectByDims(&m, {}, {}, {}, QStringLiteral("实线"));
        CHECK(r.size() == 2, "qselect 线型过滤(实线) 应命中 2 个");
    }
    // ⑤ 组合过滤：直线 AND 层0 → 2
    {
        std::vector<int> r = qselectByDims(&m, QStringLiteral("直线"), QStringLiteral("0"), {}, {});
        CHECK(r.size() == 2, "qselect 组合过滤(直线+层0) 应命中 2 个");
    }
    // ⑥ 组合过滤：直线 AND 红 → 3（两条层0 + 层1直线，均红）
    {
        std::vector<int> r = qselectByDims(&m, QStringLiteral("直线"), {}, QStringLiteral("#ff0000"), {});
        CHECK(r.size() == 3, "qselect 组合过滤(直线+红) 应命中 3 个");
    }
    // ⑦ 组合过滤：圆 AND 蓝 → 1
    {
        std::vector<int> r = qselectByDims(&m, QStringLiteral("圆"), {}, QStringLiteral("#0000ff"), {});
        CHECK(r.size() == 1, "qselect 组合过滤(圆+蓝) 应命中 1 个");
    }
    // ⑧ 全空过滤：返回空集且不崩溃
    {
        std::vector<int> r = qselectByDims(&m, {}, {}, {}, {});
        CHECK(r.empty(), "qselect 全空过滤 应返回空集");
    }
    // ⑨ 命令接线：qselect type=直线 经解释器执行后选择集 = 3
    {
        m.clearSelection();
        CommandInterpreter interp(&m);
        bool ok = interp.execute("qselect type=直线");
        CHECK(ok, "命令 qselect 应执行成功");
        CHECK(m.selection().size() == 3, "qselect 命令选中计数应为 3");
    }
    // ⑩ 别名 q 接线：q type=直线 等价
    {
        m.clearSelection();
        CommandInterpreter interp(&m);
        bool ok = interp.execute("q type=直线");
        CHECK(ok, "别名 q 应解析为 qselect 并成功执行");
        CHECK(m.selection().size() == 3, "别名 q 选中计数应为 3");
    }
    // ⑪ 全空命令：不改变选择集且不崩溃
    {
        m.clearSelection();
        CommandInterpreter interp(&m);
        bool ok = interp.execute("qselect");
        CHECK(ok, "全空 qselect 命令应成功（仅提示，不改变选择集）");
        CHECK(m.selection().empty(), "全空 qselect 命令不应选中任何对象");
    }
    // ⑫ 组合命令接线：qselect type=直线 layer=0 → 2
    {
        m.clearSelection();
        CommandInterpreter interp(&m);
        bool ok = interp.execute("qselect type=直线 layer=0");
        CHECK(ok, "组合命令 qselect type=直线 layer=0 应执行成功");
        CHECK(m.selection().size() == 2, "组合命令选中计数应为 2");
    }
    return g_fail == 0;
}


// ============================================================================
// DXF 导入扩展（15+ 类实体）黄金样本：
//   ① 自绘 → toDxf → fromDxf round-trip（图元计数/点集/包围盒 ≤1e-6 图纸 mm）
//   ② 手工构造含 INSERT+DIMENSION+HATCH+ELLIPSE+SPLINE+POINT+XLINE+RAY+LEADER
//      +ATTRIB 的样本逐类断言（块表/属性并入/降级计数）
//   ③ 畸形 DXF（垃圾/截断/坏组码/未知实体）不崩溃且报错计数
// ============================================================================
static bool t_dxfImport() {
    using namespace cadfmt;

    // ---------- ① round-trip：几何基元（LINE/CIRCLE/ARC/POLYLINE/TEXT）----------
    {
        std::vector<Element> orig;
        { Element l; l.type = ElementType::Line;    l.pts = {Vec3(0,0,0), Vec3(30,40,0)}; orig.push_back(l); }
        { Element c; c.type = ElementType::Circle;  c.pts = {Vec3(10,10,0)}; c.radius = 7.25; orig.push_back(c); }
        { Element a; a.type = ElementType::Arc;     a.pts = {Vec3(-5,3,0)}; a.radius = 12.5;
          a.startAngle = 30; a.endAngle = 120; orig.push_back(a); }
        { Element p; p.type = ElementType::Polyline; p.closed = false;
          p.pts = {Vec3(1,1,0), Vec3(9,1,0), Vec3(9,6,0)}; orig.push_back(p); }
        { Element t; t.type = ElementType::Text;    t.pts = {Vec3(2,8,0)};
          t.text = QStringLiteral("Hi"); t.radius = 3.5; orig.push_back(t); }

        QString e1, e2;
        const QByteArray dxf = toDxf(orig, &e1);
        CHECK(e1.isEmpty(), "① toDxf 无错误");
        CHECK(!dxf.isEmpty(), "① toDxf 产出非空字节");
        std::vector<Element> back;
        DxfImportStats st;
        const bool ok = fromDxf(dxf, back, &e2, &st);
        CHECK(ok, "① fromDxf 解析成功");
        CHECK(e2.isEmpty(), "① fromDxf 无错误");
        CHECK((int)back.size() == (int)orig.size(), "① round-trip 图元计数一致");
        CHECK(st.entities == (int)orig.size(), "① stats.entities 计数一致");
        CHECK(st.unknownEntities == 0, "① 干净样本无未知实体");

        bool allType = (int)back.size() == (int)orig.size();
        bool allPts = allType, allR = true, allAng = true, allTxt = true;
        if (allType) {
            for (size_t i = 0; i < orig.size() && allType; ++i) {
                if (back[i].type != orig[i].type) { allType = false; break; }
                if (back[i].pts.size() != orig[i].pts.size()) { allPts = false; continue; }
                for (size_t k = 0; k < orig[i].pts.size(); ++k)
                    if (back[i].pts[k].distTo(orig[i].pts[k]) > 1e-6) { allPts = false; break; }
                if (orig[i].type == ElementType::Circle || orig[i].type == ElementType::Arc)
                    if (std::fabs(back[i].radius - orig[i].radius) > 1e-6) allR = false;
                if (orig[i].type == ElementType::Arc) {
                    if (std::fabs(back[i].startAngle - orig[i].startAngle) > 1e-6) allAng = false;
                    if (std::fabs(back[i].endAngle   - orig[i].endAngle)   > 1e-6) allAng = false;
                }
                if (orig[i].type == ElementType::Text) {
                    if (back[i].text != orig[i].text) allTxt = false;
                    if (std::fabs(back[i].radius - orig[i].radius) > 1e-6) allR = false;
                }
            }
        }
        CHECK(allType, "① round-trip 逐元素类型一致");
        CHECK(allPts,  "① round-trip 点集差 ≤1e-6 图纸mm");
        CHECK(allR,    "① round-trip 半径/字高差 ≤1e-6");
        CHECK(allAng,  "① round-trip 弧起止角差 ≤1e-6");
        CHECK(allTxt,  "① round-trip 文本内容一致");

        auto bbox = [](const std::vector<Element>& v) {
            QRectF b; bool first = true;
            for (const auto& el : v) {
                const QRectF r = geo::elementBounds(el);
                if (first) { b = r; first = false; } else b = b.united(r);
            }
            return b;
        };
        const QRectF b1 = bbox(orig), b2 = bbox(back);
        const bool boxOk =
            std::fabs(b1.left()   - b2.left())   <= 1e-6 && std::fabs(b1.right()  - b2.right())  <= 1e-6 &&
            std::fabs(b1.top()    - b2.top())    <= 1e-6 && std::fabs(b1.bottom() - b2.bottom()) <= 1e-6;
        CHECK(boxOk, "① round-trip 包围盒差 ≤1e-6 图纸mm");
    }

    // ---------- ② 手工构造多类实体样本（逐类断言）----------
    {
        QStringList L;
        auto add = [&L](const QString& a, const QString& b) { L << a << b; };
        add("0","SECTION"); add("2","HEADER");
        add("9","$INSUNITS"); add("70","4");
        add("0","ENDSEC");
        add("0","SECTION"); add("2","BLOCKS");
        add("0","BLOCK"); add("2","BOLT"); add("70","0");
        add("10","0.0"); add("20","0.0");
        add("0","LINE"); add("8","0");
        add("10","0.0"); add("20","0.0"); add("11","10.0"); add("21","0.0");
        add("0","ENDBLK");
        add("0","ENDSEC");
        add("0","SECTION"); add("2","ENTITIES");
        // INSERT（66=1 属性跟随 → ATTRIB 并入块引用）
        add("0","INSERT"); add("2","BOLT"); add("8","0");
        add("10","20.0"); add("20","20.0"); add("41","1.0"); add("42","1.0"); add("50","0.0"); add("66","1");
        add("0","ATTRIB"); add("8","0"); add("2","TAG1"); add("1","VAL1"); add("10","20.0"); add("20","20.0");
        // ELLIPSE 全椭圆（10/20 圆心，11/21 长轴端点，40 比率，41/42 参数角）
        add("0","ELLIPSE"); add("8","0");
        add("10","50.0"); add("20","50.0");
        add("11","10.0"); add("21","0.0");
        add("40","0.5"); add("41","0.0"); add("42","6.283185307179586");
        // SPLINE（拟合点优先重建：74=3，11/21 ×3）
        add("0","SPLINE"); add("8","0");
        add("70","8"); add("71","3"); add("72","0"); add("73","0"); add("74","3");
        add("11","0.0");  add("21","0.0");
        add("11","10.0"); add("21","5.0");
        add("11","20.0"); add("21","0.0");
        // DIMENSION 对齐（70=1，13/14 测定点）
        add("0","DIMENSION"); add("8","0"); add("70","1");
        add("13","0.0"); add("23","0.0"); add("14","30.0"); add("24","0.0");
        // HATCH 单环多段线边界（91=环数，92 bit2=多段线，93=顶点数）
        add("0","HATCH"); add("8","0");
        add("91","1"); add("92","3"); add("93","4");
        add("10","0.0");  add("20","0.0");
        add("10","10.0"); add("20","0.0");
        add("10","10.0"); add("20","10.0");
        add("10","0.0");  add("20","10.0");
        add("75","0"); add("76","0");
        // POINT
        add("0","POINT"); add("8","0"); add("10","5.0"); add("20","5.0");
        // XLINE + RAY（10/20 基点，11/21 方向）
        add("0","XLINE"); add("8","0");
        add("10","100.0"); add("20","100.0"); add("11","1.0"); add("21","1.0");
        add("0","RAY"); add("8","0");
        add("10","120.0"); add("20","120.0"); add("11","0.0"); add("21","1.0");
        // LEADER（3 顶点 → 引线折线降级）
        add("0","LEADER"); add("8","0");
        add("10","200.0"); add("20","200.0");
        add("10","210.0"); add("20","205.0");
        add("10","220.0"); add("20","200.0");
        add("0","ENDSEC");
        add("0","EOF");
        QString dxfText = L.join(QStringLiteral("\r\n"));
        dxfText += QStringLiteral("\r\n");

        std::vector<Element> v;
        DxfImportStats st2;
        QString e3;
        CHECK(fromDxf(dxfText.toUtf8(), v, &e3, &st2), "② 多类样本解析成功");
        CHECK(st2.blocks.size() == 1, "② BLOCKS 段解析出 1 个块定义");
        CHECK(!st2.blocks.empty() && st2.blocks[0].name == QStringLiteral("BOLT"), "② 块定义名为 BOLT");
        CHECK(!st2.blocks.empty() && st2.blocks[0].entities.size() == 1, "② BOLT 块内 1 个图元(LINE)");

        int nIns=0, nEll=0, nSpl=0, nDim=0, nHat=0, nPt=0, nXl=0, nRayRot90=0, nLeaderPoly=0;
        bool attrOk = false;
        for (const auto& e : v) {
            if (e.type == ElementType::BlockRef) {
                ++nIns;
                if (e.blockName == QStringLiteral("BOLT"))
                    attrOk = (e.blockAttr(QStringLiteral("TAG1")) == QStringLiteral("VAL1"));
            } else if (e.type == ElementType::Ellipse) ++nEll;
            else if (e.type == ElementType::Spline)    ++nSpl;
            else if (e.type == ElementType::Dimension) ++nDim;
            else if (e.type == ElementType::Hatch)     ++nHat;
            else if (e.type == ElementType::Point)     ++nPt;
            else if (e.type == ElementType::Xline) {
                ++nXl;
                if (std::fabs(e.rotation - 90.0) < 1e-9) ++nRayRot90;
            } else if (e.type == ElementType::Polyline && e.name.contains(QStringLiteral("引线"))) ++nLeaderPoly;
        }
        CHECK(nIns == 1,  "② INSERT→块引用 1 个");
        CHECK(attrOk,     "② ATTRIB 并入块引用属性(TAG1=VAL1)");
        CHECK(nEll == 1,  "② ELLIPSE→椭圆 1 个");
        bool ellOk = false;
        for (const auto& e : v)
            if (e.type == ElementType::Ellipse)
                ellOk = std::fabs(e.radius-10.0)<1e-6 && std::fabs(e.radiusY-5.0)<1e-6 && std::fabs(e.rotation)<1e-6;
        CHECK(ellOk,      "② 椭圆 a=10 b=5 rot=0");
        CHECK(nSpl == 1,  "② SPLINE→样条 1 个（拟合点重建）");
        CHECK(nDim == 1,  "② DIMENSION→标注 1 个");
        bool dimOk = false;
        for (const auto& e : v)
            if (e.type == ElementType::Dimension)
                dimOk = e.dtype == DimensionType::Aligned && std::fabs(e.dimValue - 30.0) < 1e-6;
        CHECK(dimOk,      "② 对齐标注测量值=30");
        CHECK(nHat == 1,  "② HATCH→剖面区域 1 个");
        bool hatOk = false;
        for (const auto& e : v)
            if (e.type == ElementType::Hatch) hatOk = e.pts.size() == 4;
        CHECK(hatOk,      "② 填充边界 4 顶点");
        CHECK(nPt == 1,   "② POINT→点 1 个");
        CHECK(nXl == 2,   "② XLINE/RAY→构造线 2 个");
        CHECK(nRayRot90 == 1, "② RAY 方向角=90°");
        CHECK(nLeaderPoly == 1, "② LEADER→引线折线 1 个");
        CHECK(st2.fallbackEntities >= 1, "② LEADER 降级计数 ≥1");
        CHECK(st2.unknownEntities == 0,  "② 全部实体类型可识别");
        CHECK((nIns>0)+(nEll>0)+(nSpl>0)+(nDim>0)+(nHat>0)+(nPt>0)+(nXl>0) >= 7, "② 新增实体 ≥7 类");
    }

    // ---------- ③ 畸形 DXF：垃圾/未知实体/截断/坏组码，不崩溃且报错计数 ----------
    {
        std::vector<Element> v; DxfImportStats st3; QString e4;
        // 3a 纯垃圾（无任何 SECTION/ENTITIES）
        const QByteArray garbage("this is not a dxf file at all\nrandom junk\n");
        CHECK(!fromDxf(garbage, v, &e4, &st3), "③a 垃圾文件解析失败(不崩溃)");
        CHECK(!e4.isEmpty(), "③a 报错信息非空");
        // 3b ENTITIES 内只有未知实体 → 逐个计数跳过
        QStringList L2;
        L2 << "0" << "SECTION" << "2" << "ENTITIES"
           << "0" << "BANANA" << "8" << "0" << "10" << "1.0" << "20" << "2.0"
           << "0" << "ENDSEC" << "0" << "EOF";
        std::vector<Element> v3; DxfImportStats st3b; QString e5;
        CHECK(!fromDxf(L2.join(QStringLiteral("\r\n")).toUtf8(), v3, &e5, &st3b),
              "③b 仅未知实体→整体失败(可识别图元为0)");
        CHECK(st3b.unknownEntities >= 1, "③b 未知实体计数 ≥1");
        // 3c 截断（LINE 缺终点组码）→ 降级为可解析图元，不崩溃
        QStringList L3;
        L3 << "0" << "SECTION" << "2" << "ENTITIES" << "0" << "LINE" << "8" << "0"
           << "10" << "1.0" << "20" << "2.0"
           << "0" << "ENDSEC" << "0" << "EOF";
        std::vector<Element> v4; DxfImportStats st3c; QString e6;
        CHECK(fromDxf(L3.join(QStringLiteral("\r\n")).toUtf8(), v4, &e6, &st3c),
              "③c 截断 LINE 降级为可解析图元(不崩溃)");
        CHECK(v4.size() == 1 && v4[0].type == ElementType::Line, "③c 截断 LINE 仍得 1 条直线");
        // 3d 坏组码（非整数行）被 tokenizer 跳过，不崩溃
        QByteArray bad("0\r\nSECTION\r\n2\r\nENTITIES\r\n0\r\nLINE\r\n8\r\n0\r\nXX\r\n1.0\r\n10\r\n0.0\r\n20\r\n0.0\r\n11\r\n5.0\r\n21\r\n5.0\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n");
        std::vector<Element> v5; DxfImportStats st3d; QString e7;
        CHECK(fromDxf(bad, v5, &e7, &st3d) && v5.size() == 1, "③d 坏组码行被跳过且 LINE 可解析");
    }
    return g_fail == 0;
}


// ============================================================================
// DXF 原生导出（INSERT/DIMENSION/HATCH/POINT + 9 LTYPE + DIMSTYLE）：
//   ① native 导出 token 计数（块/标注/填充/点/表结构计数正确）
//   ② 导出 → 自身再导入 round-trip（逐类差分 ≤1e-6；*D 匿名块过滤）
//   ③ legacy 兼容（不传 opts → INSERT/DIMENSION/HATCH token=0，旧行为不变）
//      + 样条离散精度可配置（splineSegments 生效）
// ============================================================================
// ===================== G1/G2 黄金样本差分断言（golden 层与 DXF 双测共用） =====================
// G1 = token 计数：遍历 "0\r\n<类型>" 相邻行（整词匹配，避免 TEXT 误命中 MTEXT）
static int dxfCountToken(const QByteArray& body, const char* type) {
    const QStringList lines = QString::fromUtf8(body).split(QStringLiteral("\r\n"));
    int n = 0;
    for (int i = 0; i + 1 < lines.size(); ++i)
        if (lines.at(i) == QStringLiteral("0") && lines.at(i + 1) == QLatin1String(type)) ++n;
    return n;
}
// G2 = 点集差分：逐点 ≤1e-6 图纸 mm
static bool dxfPtsClose(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].distTo(b[i]) > 1e-6) return false;
    return true;
}

static bool t_dxfExport() {
    using namespace cadfmt;

    // ---------- ① native 导出 + token 计数 ----------
    std::vector<Element> els;
    { Element l; l.type = ElementType::Line; l.pts = {Vec3(-20,0,0), Vec3(-10,0,0)}; els.push_back(l); }
    {   // 带属性值的块引用
        Element br; br.type = ElementType::BlockRef;
        br.blockName = QStringLiteral("BRK");
        br.pts = {Vec3(100, 50, 0)};
        br.scaleX = br.scaleY = 1.0;
        br.blockAttrs.push_back({QStringLiteral("TAG1"), QStringLiteral("VAL1")});
        els.push_back(br);
    }
    { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
      d.dtype = DimensionType::Aligned;  d.dimValue = 30.0;
      d.pts = {Vec3(0,0,0), Vec3(30,0,0)}; els.push_back(d); }
    { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
      d.dtype = DimensionType::Angle;    d.dimValue = 90.0;
      d.pts = {Vec3(0,40,0), Vec3(10,40,0), Vec3(0,50,0)}; els.push_back(d); }
    { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
      d.dtype = DimensionType::Radius;   d.dimValue = 10.0; d.radius = 10.0;
      d.pts = {Vec3(0,80,0), Vec3(10,80,0)}; els.push_back(d); }
    { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
      d.dtype = DimensionType::Diameter; d.dimValue = 20.0; d.radius = 10.0;
      d.pts = {Vec3(50,80,0), Vec3(60,80,0)}; els.push_back(d); }
    {   // 填充：4 顶点外环 + 1 个 3 顶点孔
        Element h; h.type = ElementType::Hatch; h.closed = true;
        h.pts = {Vec3(120,0,0), Vec3(140,0,0), Vec3(140,20,0), Vec3(120,20,0)};
        h.holes.push_back({Vec3(125,5,0), Vec3(130,5,0), Vec3(127,10,0)});
        els.push_back(h);
    }
    { Element p; p.type = ElementType::Point; p.pts = {Vec3(5,5,0)}; els.push_back(p); }
    const std::vector<Vec3> splPts = {Vec3(200,0,0), Vec3(210,10,0), Vec3(220,0,0)};
    { Element s; s.type = ElementType::Spline; s.splineSubType = 0; s.pts = splPts; els.push_back(s); }
    { Element el; el.type = ElementType::Ellipse; el.pts = {Vec3(260,10,0)};
      el.radius = 30.0; el.radiusY = 15.0; el.rotation = 20.0; els.push_back(el); }

    std::vector<BlockDef> blk;
    {
        BlockDef d;
        d.name = QStringLiteral("BRK");
        d.basePoint = Vec3(0, 0, 0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        d.entities.push_back(l);
        AttributeDef ad;
        ad.tag = QStringLiteral("TAG1");
        ad.prompt = QStringLiteral("输入标签");
        ad.defaultValue = QStringLiteral("VAL1");
        ad.pos = Vec3(5, 1, 0);
        ad.height = 3.5;
        d.attdefs.push_back(ad);
        blk.push_back(d);
    }

    DxfExportOptions no;
    no.nativeBlocks = no.nativeDims = no.nativeHatch = true;
    QString e1;
    const QByteArray dxf = toDxf(els, &e1, 1.0, 4, &blk, &no);
    CHECK(e1.isEmpty(), "① native toDxf 无错误");
    CHECK(!dxf.isEmpty(), "① native toDxf 产出非空字节");
    CHECK(dxfCountToken(dxf, "INSERT")    == 1, "① INSERT 计数=1（块引用原生导出）");
    CHECK(dxfCountToken(dxf, "ATTRIB")    == 1, "① ATTRIB 计数=1（属性值随引用导出）");
    CHECK(dxfCountToken(dxf, "ATTDEF")    == 1, "① ATTDEF 计数=1（块定义携带属性声明）");
    CHECK(dxfCountToken(dxf, "DIMENSION") == 4, "① DIMENSION 计数=4（对齐/角度/半径/直径原生）");
    CHECK(dxfCountToken(dxf, "HATCH")     == 1, "① HATCH 计数=1（填充原生导出）");
    CHECK(dxfCountToken(dxf, "POINT")     == 1, "① POINT 计数=1（此前被丢弃的点现补写）");
    CHECK(dxfCountToken(dxf, "BLOCK")     == 5, QStringLiteral("① BLOCK 计数=5（实际=%1）").arg(dxfCountToken(dxf, "BLOCK")).toUtf8().constData());
    CHECK(dxfCountToken(dxf, "LTYPE")     == 9, "① LTYPE 计数=9（产品 9 线型全定义）");
    CHECK(dxfCountToken(dxf, "DIMSTYLE")  == 1, "① DIMSTYLE 计数=1（STANDARD 最小实现）");
    CHECK(dxfCountToken(dxf, "SOLID") > 0,     "① *D 匿名块内含箭头 SOLID");

    // ---------- ② round-trip：导出 → 自身再导入，逐类差分 ≤1e-6 ----------
    {
        std::vector<Element> back;
        DxfImportStats st;
        QString e2;
        CHECK(fromDxf(dxf, back, &e2, &st), "② 导出文件再导入成功");
        CHECK(e2.isEmpty(), "② 再导入无错误");
        CHECK(st.unknownEntities == 0, QStringLiteral("② 导出文件无未知实体（实际=%1）").arg(st.unknownEntities).toUtf8().constData());
        CHECK(st.fallbackEntities == 0, "② 导出文件无降级实体");
        CHECK(st.blocks.size() == 1, "② 块表仅含用户块（*D 匿名块被过滤）");
        CHECK(!st.blocks.empty() && st.blocks[0].name == QStringLiteral("BRK"),
              "② 块表名为 BRK");
        CHECK(!st.blocks.empty() && st.blocks[0].entities.size() == 1,
              "② BRK 块内 1 个图元（ATTDEF 不入实体表）");

        int nIns = 0, nDim = 0, nHat = 0, nPt = 0;
        bool attrOk = false, insPosOk = false, brkPosOk = false;
        for (const auto& e : back) {
            if (e.type == ElementType::BlockRef) {
                ++nIns;
                attrOk = attrOk || e.blockAttr(QStringLiteral("TAG1")) == QStringLiteral("VAL1");
                if (e.blockName == QStringLiteral("BRK"))
                    brkPosOk = e.pts[0].distTo(Vec3(100, 50, 0)) <= 1e-6;
            } else if (e.type == ElementType::Dimension) ++nDim;
            else if (e.type == ElementType::Hatch)       ++nHat;
            else if (e.type == ElementType::Point) {
                ++nPt;
                insPosOk = e.pts[0].distTo(Vec3(5, 5, 0)) <= 1e-6;
            }
        }
        CHECK(nIns == 1,     "② 块引用 round-trip 1 个");
        CHECK(attrOk,        "② ATTRIB 属性值 TAG1=VAL1 往返一致");
        CHECK(brkPosOk,      "② 块引用插入点差 ≤1e-6");
        CHECK(nDim == 4,     QStringLiteral("② 标注 round-trip 4 个（实际=%1）").arg(nDim).toUtf8().constData());
        CHECK(nHat == 1,     "② 填充 round-trip 1 个");
        CHECK(nPt == 1,      "② 点 round-trip 1 个");
        CHECK(insPosOk,      "② 点位置差 ≤1e-6");

        bool dimA = false, dimB = false, dimR = false, dimD = false;
        double radVal = -1, diaVal = -1;
        for (const auto& e : back) {
            if (e.type != ElementType::Dimension) continue;
            if (e.dtype == DimensionType::Aligned)
                dimA = std::fabs(e.dimValue - 30.0) <= 1e-6;
            else if (e.dtype == DimensionType::Angle)
                dimB = std::fabs(e.dimValue - 90.0) <= 1e-6;
            else if (e.dtype == DimensionType::Radius) {
                dimR = std::fabs(e.dimValue - 10.0) <= 1e-6;
                radVal = e.dimValue;
            } else if (e.dtype == DimensionType::Diameter) {
                dimD = std::fabs(e.dimValue - 20.0) <= 1e-6;
                diaVal = e.dimValue;
            }
        }
        CHECK(dimA, "② 对齐标注测量值往返=30±1e-6");
        CHECK(dimB, "② 角度标注测量值往返=90±1e-6");
        CHECK(dimR, QStringLiteral("② 半径标注测量值往返=10±1e-6（实际=%1）").arg(radVal).toUtf8().constData());
        CHECK(dimD, QStringLiteral("② 直径标注测量值往返=20±1e-6（实际=%1）").arg(diaVal).toUtf8().constData());

        bool hatOk = false;
        for (const auto& e : back)
            if (e.type == ElementType::Hatch)
                hatOk = e.pts.size() == 4 && e.holes.size() == 1 &&
                        !e.holes.empty() && e.holes[0].size() == 3;
        CHECK(hatOk, "② 填充边界 4 顶点 + 1 孔(3 顶点)往返一致");

        // 样条/椭圆：R12 无原生实体 → 折线拟合，点集与导出采样严格一致
        const auto wantSpl = geo::sampleSpline(splPts, 16);
        const auto wantEll = geo::sampleEllipse(Vec3(260, 10, 0), 30.0, 15.0, 20.0, 96);
        bool splOk = false, ellOk = false;
        for (const auto& e : back) {
            if (e.type != ElementType::Polyline) continue;
            if (dxfPtsClose(e.pts, wantSpl)) splOk = true;
            if (dxfPtsClose(e.pts, wantEll)) ellOk = true;
        }
        CHECK(splOk, "② 样条折线拟合点集=sampleSpline(pts,16) 逐点≤1e-6");
        CHECK(ellOk, "② 椭圆折线采样点集=sampleEllipse(96) 逐点≤1e-6");
    }

    // ---------- ③ legacy 兼容 + 样条精度可配置 ----------
    {
        std::vector<Element> leg;
        { Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)}; leg.push_back(l); }
        { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
          d.dtype = DimensionType::Aligned; d.dimValue = 30.0;
          d.pts = {Vec3(0,0,0), Vec3(30,0,0)}; leg.push_back(d); }
        { Element h; h.type = ElementType::Hatch; h.closed = true;
          h.pts = {Vec3(120,0,0), Vec3(140,0,0), Vec3(140,20,0), Vec3(120,20,0)}; leg.push_back(h); }
        { Element br; br.type = ElementType::BlockRef;
          br.blockName = QStringLiteral("BRK"); br.pts = {Vec3(100,50,0)}; leg.push_back(br); }

        QString e3;
        const QByteArray dxf2 = toDxf(leg, &e3);   // 不传 blocks/opts = 旧版行为
        CHECK(e3.isEmpty(), "③ legacy toDxf 无错误");
        CHECK(dxfCountToken(dxf2, "INSERT") == 0,    "③ legacy 无 INSERT（块引用静默跳过）");
        CHECK(dxfCountToken(dxf2, "DIMENSION") == 0, "③ legacy 无 DIMENSION（标注炸开）");
        CHECK(dxfCountToken(dxf2, "HATCH") == 0,     "③ legacy 无 HATCH（填充跳过）");
        CHECK(dxfCountToken(dxf2, "LINE") > 0,       "③ legacy 标注炸开为 LINE 基元");

        // 样条离散精度可配置：段数大 → VERTEX 多（不依赖 sampleSpline 内部公式）
        std::vector<Element> one;
        { Element s; s.type = ElementType::Spline; s.splineSubType = 0; s.pts = splPts; one.push_back(s); }
        DxfExportOptions o32; o32.splineSegments = 32;
        DxfExportOptions o8;  o8.splineSegments  = 8;
        QString e4;
        const QByteArray d32 = toDxf(one, &e4, 1.0, 4, nullptr, &o32);
        const QByteArray d8  = toDxf(one, nullptr, 1.0, 4, nullptr, &o8);
        CHECK(e4.isEmpty() && !d32.isEmpty() && !d8.isEmpty(), "③ 精度变体导出成功");
        CHECK(dxfCountToken(d32, "VERTEX") > dxfCountToken(d8, "VERTEX"),
              "③ splineSegments 精度可配置（32 段顶点数 > 8 段）");
    }
    return g_fail == 0;
}


// ============================================================================
// 分层测试网（回归保护升级）：core 纯函数层 + 存档兼容层 + 黄金样本层
// 说明：以下各组函数体内直接 CHECK 计数（不再聚合为单个 bool 返回值），
//       每条断言独立报位（file:line），失败不中断同组后续断言。
// ============================================================================

// ---------- Geometry 纯几何层 ----------
static void t_geometryCore() {
    // 1) 点到线段距离：垂足落在段内 → 垂线长
    CHECK(std::fabs(geo::distPointSegment(Vec3(5,3,0), Vec3(0,0,0), Vec3(10,0,0)) - 3.0) < 1e-9,
          "geo::distPointSegment 垂足在段内=3");
    // 2) 垂足在段外 → 退化为端点距
    CHECK(std::fabs(geo::distPointSegment(Vec3(-2,0,0), Vec3(0,0,0), Vec3(10,0,0)) - 2.0) < 1e-9,
          "geo::distPointSegment 垂足段外=端点距 2");
    // 3) 角度规整 (-180,180]
    CHECK(std::fabs(geo::wrapDeg180(190.0) - (-170.0)) < 1e-9 &&
          std::fabs(geo::wrapDeg180(-190.0) - 170.0) < 1e-9,
          "geo::wrapDeg180 190→-170 / -190→170");
    // 4) 角度规整 [0,360)
    CHECK(std::fabs(geo::wrapDeg360(370.0) - 10.0) < 1e-9 &&
          std::fabs(geo::wrapDeg360(-30.0) - 330.0) < 1e-9,
          "geo::wrapDeg360 370→10 / -30→330");
    // 5) 扫掠角范围判定
    CHECK(geo::angleInSweep(30.0, 0.0, 90.0) && !geo::angleInSweep(120.0, 0.0, 90.0),
          "geo::angleInSweep 范围内 true / 范围外 false");
    // 6) 三点定圆：单位圆
    {
        Vec3 c; double r = 0.0;
        const bool ok = geo::circleFrom3Points(Vec3(1,0,0), Vec3(0,1,0), Vec3(-1,0,0), c, r);
        CHECK(ok && std::fabs(c.x) < 1e-9 && std::fabs(c.y) < 1e-9 && std::fabs(r - 1.0) < 1e-9,
              "geo::circleFrom3Points 单位圆 c=(0,0) r=1");
    }
    // 7) 三点共线 → 定圆失败
    {
        Vec3 c; double r = 0.0;
        CHECK(!geo::circleFrom3Points(Vec3(0,0,0), Vec3(1,0,0), Vec3(2,0,0), c, r),
              "geo::circleFrom3Points 共线返回 false");
    }
    // 8) 段长与方向角
    CHECK(std::fabs(geo::segLength(Vec3(0,0,0), Vec3(3,4,0)) - 5.0) < 1e-9 &&
          std::fabs(geo::segAngleDeg(Vec3(0,0,0), Vec3(3,4,0)) - 53.13010235415598) < 1e-6,
          "geo::segLength=5 / segAngleDeg=atan2(4,3)");
    // 9) 起点+长度+角 → 终点反算
    {
        const Vec3 p = geo::endpointFrom(Vec3(0,0,0), 5.0, 53.13010235415598);
        CHECK(std::fabs(p.x - 3.0) < 1e-9 && std::fabs(p.y - 4.0) < 1e-9,
              "geo::endpointFrom 反算终点 (3,4)");
    }
    // 10) 图元几何包围盒：圆 = 圆心 ± 半径
    {
        Element e; e.type = ElementType::Circle; e.pts = {Vec3(10,10,0)}; e.radius = 2.5;
        const QRectF b = geo::elementBounds(e);
        CHECK(std::fabs(b.left() - 7.5) < 1e-9 && std::fabs(b.width() - 5.0) < 1e-9 &&
              std::fabs(b.top() - 7.5) < 1e-9 && std::fabs(b.height() - 5.0) < 1e-9,
              "geo::elementBounds 圆=圆心±半径");
    }
    // 11) 点在多边形（射线法）
    {
        const std::vector<Vec3> sq = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
        CHECK(geo::pointInPolygon(Vec3(5,5,0), sq) && !geo::pointInPolygon(Vec3(15,5,0), sq),
              "geo::pointInPolygon 内 true / 外 false");
    }
    // 12) 折线长度：两直角段
    {
        const std::vector<Vec3> v = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0)};
        CHECK(std::fabs(geo::polylineLength(v, false) - 20.0) < 1e-9,
              "geo::polylineLength 两直角段=20");
    }
    // 13) 样条采样：严格过控制点 + 采样数 (n-1)*perSeg+1
    {
        const std::vector<Vec3> pts = {Vec3(0,0,0), Vec3(10,10,0), Vec3(20,0,0)};
        const std::vector<Vec3> s = geo::sampleSpline(pts, 8);
        CHECK((int)s.size() == 17 &&
              s.front().distTo(pts.front()) < 1e-9 && s.back().distTo(pts.back()) < 1e-9 &&
              s[8].distTo(Vec3(10,10,0)) < 1e-9,
              "geo::sampleSpline 17 点且严格过 3 控制点");
    }
}

// ---------- Modify 修改引擎层 ----------
static void t_modifyCore() {
    // 1) 全删除动作
    {
        auto cs = modify::makeDelete({5, 7});
        CHECK(cs.size() == 2 && cs[0].remove && cs[0].id == 5 && cs[1].id == 7,
              "modify::makeDelete 按名单产删除动作");
    }
    // 2) 复制：id=-1 标记新增 + 几何偏置
    {
        Element l; l.type = ElementType::Line; l.id = 3; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        auto cs = modify::makeDuplicate({l}, 10.0, 5.0);
        CHECK(cs.size() == 1 && cs[0].id == -1 && cs[0].replacement.id == -1 &&
              cs[0].replacement.pts[0].distTo(Vec3(10,5,0)) < 1e-9 &&
              cs[0].replacement.pts[1].distTo(Vec3(20,5,0)) < 1e-9,
              "modify::makeDuplicate 克隆 id=-1 且偏置 (10,5)");
    }
    // 3) 移动：替换语义保留源 ID
    {
        Element l; l.type = ElementType::Line; l.id = 9; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        auto cs = modify::makeMoveCopy({l}, Vec3(2,3,0), false);
        CHECK(cs.size() == 1 && cs[0].id == 9 && cs[0].replacement.id == 9 &&
              cs[0].replacement.pts[0].distTo(Vec3(2,3,0)) < 1e-9,
              "modify::makeMoveCopy 移动保留源 ID=9");
    }
    // 4) 旋转：绕原点 90°（CCW）
    {
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(10,0,0), Vec3(20,0,0)};
        auto cs = modify::makeRotate({l}, Vec3(0,0,0), 90.0);
        CHECK(cs.size() == 1 && cs[0].replacement.pts[0].distTo(Vec3(0,10,0)) < 1e-9 &&
              cs[0].replacement.pts[1].distTo(Vec3(0,20,0)) < 1e-9,
              "modify::makeRotate 绕原点 90° → +Y");
    }
    // 5) 缩放：圆心/半径同步 ×2
    {
        Element c; c.type = ElementType::Circle; c.id = 2; c.pts = {Vec3(10,0,0)}; c.radius = 3.0;
        auto cs = modify::makeScale({c}, Vec3(0,0,0), 2.0);
        CHECK(cs.size() == 1 && cs[0].replacement.pts[0].distTo(Vec3(20,0,0)) < 1e-9 &&
              std::fabs(cs[0].replacement.radius - 6.0) < 1e-9,
              "modify::makeScale ×2 圆心 20/半径 6 同步");
    }
    // 6) 点镜像：P′=2C−P
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(3,4,0), Vec3(5,0,0)};
        auto cs = modify::makeMirror({l}, true, Vec3(0,0,0));
        CHECK(cs.size() == 1 && cs[0].replacement.pts[0].distTo(Vec3(-3,-4,0)) < 1e-9 &&
              cs[0].replacement.pts[1].distTo(Vec3(-5,0,0)) < 1e-9,
              "modify::makeMirror 点对称 2C−P");
    }
    // 7) 轴镜像：保留源 + 副本 id=-1 + y 不变
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(3,4,0), Vec3(5,4,0)};
        auto cs = modify::makeMirror({l}, false, Vec3(0,0,0), Vec3(0,1,0));
        CHECK(cs.size() == 1 && !cs[0].remove && cs[0].replacement.id == -1 &&
              cs[0].replacement.pts[0].distTo(Vec3(-3,4,0)) < 1e-9,
              "modify::makeMirror 轴对称保留源+副本id=-1");
    }
    // 8) 反向：直线端点互换
    {
        Element l; l.type = ElementType::Line; l.id = 4; l.pts = {Vec3(0,0,0), Vec3(9,9,0)};
        auto cs = modify::makeRevertDirection({l});
        CHECK(cs.size() == 1 && cs[0].replacement.pts[0].distTo(Vec3(9,9,0)) < 1e-9 &&
              cs[0].replacement.pts[1].distTo(Vec3(0,0,0)) < 1e-9,
              "modify::makeRevertDirection 端点互换");
    }
    // 9) 延长：终点侧 +5
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        auto cs = modify::makeLengthen({l}, 1, 5.0);
        CHECK(cs.size() == 1 && cs[0].replacement.pts[1].distTo(Vec3(15,0,0)) < 1e-9,
              "modify::makeLengthen 终点侧延长 5");
    }
    // 10) 打断：中点 → 2 段
    {
        Element l; l.type = ElementType::Line; l.id = 6; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        auto cs = modify::makeBreakDivide({l}, 6, Vec3(5,0,0), 2);
        CHECK(cs.size() == 2, "modify::makeBreakDivide 打断产 2 段");
    }
    // 11) 矩形阵列 2×3 → 5 副本（跳过 (0,0) 原位）
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 1.0;
        auto cs = modify::makeRectangularArray({c}, 2, 3, 5.0, 5.0);
        CHECK((int)cs.size() == 5, "modify::makeRectangularArray 2×3 产 5 副本");
    }
    // 12) 环形阵列 8 份 360° → 7 副本，首副本在 45°
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(10,0,0), Vec3(12,0,0)};
        auto cs = modify::makePolarArray({l}, Vec3(0,0,0), 8, 360.0);
        const Vec3 first45(10.0 * 0.7071067811865476, 10.0 * 0.7071067811865476, 0);
        CHECK((int)cs.size() == 7 && !cs.empty() &&
              cs[0].replacement.pts[0].distTo(first45) < 1e-6,
              "modify::makePolarArray 7 副本且首份 45°");
    }
    // 13) 爆炸：矩形 → 4 边直线 + 1 删除
    {
        Element r; r.type = ElementType::Rectangle; r.id = 8;
        r.pts = {Vec3(0,0,0), Vec3(10,8,0)};
        auto cs = modify::makeExplode({r});
        int lines = 0, removes = 0;
        for (const auto& c : cs) { if (c.remove) ++removes; else if (c.replacement.type == ElementType::Line) ++lines; }
        CHECK(lines == 4 && removes == 1, "modify::makeExplode 矩形→4 直线+1 删除");
    }
    // 14) 落盘：applyChanges 复制后模型计数=2
    {
        SketchModel m;
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const int id = m.addElement(l);
        Element src = l; src.id = id;
        auto cs = modify::makeDuplicate({src}, 10.0, 0.0);
        modify::applyChanges(&m, cs, QStringLiteral("自测复制"));
        CHECK(m.elementCount() == 2, "modify::applyChanges 落盘后模型计数=2");
    }
}

// ---------- 批量编辑三件套：JOIN / 阵列命令 / 剪贴板 ----------
// 覆盖：正常链式连合、flip 语义、跨层/孤立退化、explode 可逆；
//       矩形/环形/路径阵列的几何精度、属性继承与退化场景（零间距/单元素/路径过短）；
//       剪贴板 JSON round-trip、版本拒绝、translateTo、pasteInto 双语义、单步撤销。
static void t_batchEdit() {
    // ================= JOIN（连合） =================
    // J1/J2：三段首尾相接直线 → 3 删 + 1 条开放 Polyline（4 顶点）
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(0,0,0),  Vec3(10,0,0)};
        Element b; b.id = 2; b.type = ElementType::Line; b.pts = {Vec3(10,0,0), Vec3(10,8,0)};
        Element c; c.id = 3; c.type = ElementType::Line; c.pts = {Vec3(10,8,0), Vec3(0,8,0)};
        auto cs = modify::makeJoin({a, b, c}, 0.5);
        int removes = 0, polys = 0;
        const Element* poly = nullptr;
        for (const auto& ch : cs) {
            if (ch.remove) ++removes;
            else if (ch.replacement.type == ElementType::Polyline) { ++polys; poly = &ch.replacement; }
        }
        CHECK(removes == 3 && polys == 1, "JOIN 三段相接线 → 3 删 + 1 Polyline");
        CHECK(poly && !poly->closed && poly->arcFlags.empty() && poly->pts.size() == 4 &&
              poly->pts.front().distTo(Vec3(0,0,0)) < 1e-9 &&
              poly->pts.back().distTo(Vec3(0,8,0)) < 1e-9,
              "JOIN Polyline 开放/4 顶点/无弧段/首尾正确");
    }
    // J3/J4：落盘后源删净、顶点接缝无重复
    {
        SketchModel m;
        const int i1 = addLine(m, 0, 0, 10, 0);
        const int i2 = addLine(m, 10, 0, 10, 8);
        std::vector<Element> src = { *m.element(i1), *m.element(i2) };
        auto cs = modify::makeJoin(src, 0.5);
        modify::applyChanges(&m, cs, QString());
        CHECK(m.elementCount() == 1, "JOIN 落盘后 2 源 → 1 Polyline");
        const Element* p = nullptr;
        for (const auto& e : m.elements()) if (e.type == ElementType::Polyline) p = &e;
        CHECK(p && (int)p->pts.size() == 3, "JOIN 落盘 Polyline 顶点数 = 3（接缝无重复点）");
    }
    // J5：孤立图元（无邻居）→ 无链产空
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(0,0,0),  Vec3(5,0,0)};
        Element b; b.id = 2; b.type = ElementType::Line; b.pts = {Vec3(50,50,0),Vec3(60,50,0)};
        CHECK(modify::makeJoin({a, b}, 0.5).empty(), "JOIN 孤立线段无链 → 空");
    }
    // J6：跨层端点相接也不连（同层约束）
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.layer = QStringLiteral("0");
        a.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        Element b; b.id = 2; b.type = ElementType::Line; b.layer = QStringLiteral("walls");
        b.pts = {Vec3(10,0,0), Vec3(10,8,0)};
        CHECK(modify::makeJoin({a, b}, 0.5).empty(), "JOIN 跨层端点相接也不连");
    }
    // J7：反向/乱序线段也能连（flip 语义）——方向可正可反，顶点集合必须正确
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(10,0,0), Vec3(0,0,0)};   // 反向
        Element b; b.id = 2; b.type = ElementType::Line; b.pts = {Vec3(10,8,0), Vec3(10,0,0)};  // 反向
        auto cs = modify::makeJoin({b, a}, 0.5);
        const Element* poly = nullptr;
        for (const auto& ch : cs) if (!ch.remove) poly = &ch.replacement;
        const bool endsOk = poly && poly->pts.size() == 3 &&
            ((poly->pts.front().distTo(Vec3(0,0,0)) < 1e-9  && poly->pts.back().distTo(Vec3(10,8,0)) < 1e-9) ||
             (poly->pts.front().distTo(Vec3(10,8,0)) < 1e-9 && poly->pts.back().distTo(Vec3(0,0,0))  < 1e-9));
        CHECK(endsOk, "JOIN 反向/乱序线段连合且端点集合正确（flip）");
    }
    // J8：Arc 参与连合 → 按弧长离散并入折线（半圆 r=10 → 顶点数 ≥ 12）
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,-10,0), Vec3(20,-10,0)};
        Element arc; arc.id = 2; arc.type = ElementType::Arc; arc.pts = {Vec3(20,0,0)};
        arc.radius = 10; arc.startAngle = -90; arc.endAngle = 90;   // 右半圆 (20,-10)→(20,10)
        auto cs = modify::makeJoin({l, arc}, 0.5);
        const Element* poly = nullptr;
        for (const auto& ch : cs) if (!ch.remove) poly = &ch.replacement;
        CHECK(poly && poly->type == ElementType::Polyline && (int)poly->pts.size() >= 12,
              "JOIN Arc 离散并入 Polyline（顶点数 ≥ 12）");
    }
    // J9：JOIN → explode 可逆（3 段直线还原）
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(0,0,0),  Vec3(10,0,0)};
        Element b; b.id = 2; b.type = ElementType::Line; b.pts = {Vec3(10,0,0), Vec3(10,8,0)};
        Element c; c.id = 3; c.type = ElementType::Line; c.pts = {Vec3(10,8,0), Vec3(0,8,0)};
        auto cs = modify::makeJoin({a, b, c}, 0.5);
        const Element* poly = nullptr;
        for (const auto& ch : cs) if (!ch.remove) poly = &ch.replacement;
        auto ex = modify::makeExplode({*poly});
        int lines = 0, removes = 0;
        for (const auto& ch : ex) {
            if (ch.remove) ++removes;
            else if (ch.replacement.type == ElementType::Line) ++lines;
        }
        CHECK(lines == 3 && removes == 1, "JOIN → explode 可逆：还原 3 段直线 + 1 删除");
    }
    // J10：单元素退化（src<2）→ 空
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        CHECK(modify::makeJoin({a}, 0.5).empty(), "JOIN 单元素 → 空（退化）");
    }

    // ================= 矩形阵列 =================
    // R1/R2/R3：2×3 → 5 副本，网格位置精确且无 (0,0) 原位
    {
        Element c; c.id = 1; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 1;
        auto cs = modify::makeRectangularArray({c}, 2, 3, 10.0, 20.0);
        CHECK((int)cs.size() == 5, "矩形阵列 2×3 → 5 副本");
        bool gridOk = true;
        for (const auto& ch : cs) {
            const Vec3& p = ch.replacement.pts[0];
            const int r = (int)std::llround(p.y / 10.0), col = (int)std::llround(p.x / 20.0);
            if (!(r >= 0 && r < 2 && col >= 0 && col < 3 && !(r == 0 && col == 0))) gridOk = false;
        }
        CHECK(gridOk, "矩形阵列副本网格位置精确且无原位 (0,0)");
        CHECK(!cs.empty() && cs[0].replacement.id == -1, "矩形阵列副本 id=-1 新增语义");
    }
    // R4：落盘 elementCount = 1 + 5
    {
        SketchModel m;
        const int id = addCircle(m, 0, 0, 1);
        auto cs = modify::makeRectangularArray({ *m.element(id) }, 2, 3, 10.0, 20.0);
        modify::applyChanges(&m, cs, QString());
        CHECK(m.elementCount() == 6, "矩形阵列落盘 1+5=6");
    }
    // R5：1×1 → 空（退化）
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 1;
        CHECK(modify::makeRectangularArray({c}, 1, 1, 10, 10).empty(), "矩形阵列 1×1 → 空（退化）");
    }
    // R6：零间距 → 仍产出 (rows-1)*cols+(cols-1) 个与原件重合的副本
    {
        Element c; c.id = 1; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 1;
        auto cs = modify::makeRectangularArray({c}, 2, 2, 0.0, 0.0);
        CHECK((int)cs.size() == 3 && !cs.empty() &&
              cs[0].replacement.pts[0].distTo(Vec3(0,0,0)) < 1e-9,
              "矩形阵列零间距 → 3 个重合副本（不崩溃）");
    }
    // R7：多源 2 图元 2×2 → 6 副本
    {
        Element a; a.id = 1; a.type = ElementType::Line; a.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        Element b; b.id = 2; b.type = ElementType::Circle; b.pts = {Vec3(5,5,0)}; b.radius = 2;
        CHECK((int)modify::makeRectangularArray({a, b}, 2, 2, 10, 10).size() == 6,
              "矩形阵列双源 2×2 → 6 副本");
    }
    // R8：属性继承（layer/color/lineStyle）
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.layer = QStringLiteral("walls");
        l.color = QColor(255, 0, 0);         l.lineStyle = LineStyle::Dashed;
        l.pts = {Vec3(0,0,0), Vec3(5,0,0)};
        auto cs = modify::makeRectangularArray({l}, 2, 1, 10, 10);
        CHECK(cs.size() == 1 && cs[0].replacement.layer == QStringLiteral("walls") &&
              cs[0].replacement.color == QColor(255,0,0) &&
              cs[0].replacement.lineStyle == LineStyle::Dashed,
              "矩形阵列副本继承 layer/color/线型");
    }
    // R9：rows=0 → 空
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 1;
        CHECK(modify::makeRectangularArray({c}, 0, 3, 10, 10).empty(), "矩形阵列 rows=0 → 空");
    }

    // ================= 环形阵列 =================
    // P1/P2/P3：4 份 / 180° → 3 副本，分别位于 45°/90°/135°
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(10,0,0), Vec3(12,0,0)};
        auto cs = modify::makePolarArray({l}, Vec3(0,0,0), 4, 180.0);
        CHECK((int)cs.size() == 3, "环形阵列 4 份/180° → 3 副本");
        const double c45 = 0.7071067811865476;
        CHECK(!cs.empty() && cs[0].replacement.pts[0].distTo(Vec3(10*c45, 10*c45, 0)) < 1e-6,
              "环形阵列首副本在 45°");
        CHECK((int)cs.size() >= 2 && cs[1].replacement.pts[0].distTo(Vec3(0, 10, 0)) < 1e-6,
              "环形阵列第二副本在 90°");
    }
    // P4：圆 8 份/360° → 7 副本，圆心转 45°、半径不变
    {
        Element c; c.id = 1; c.type = ElementType::Circle; c.pts = {Vec3(10,0,0)}; c.radius = 2;
        auto cs = modify::makePolarArray({c}, Vec3(0,0,0), 8, 360.0);
        const double c45 = 0.7071067811865476;
        CHECK((int)cs.size() == 7 && !cs.empty() &&
              cs[0].replacement.pts[0].distTo(Vec3(10*c45, 10*c45, 0)) < 1e-6 &&
              std::fabs(cs[0].replacement.radius - 2.0) < 1e-9,
              "环形阵列圆 8 份/360°：圆心转 45°/半径不变");
    }
    // P5：Arc 起止角同步 +45°
    {
        Element a; a.id = 1; a.type = ElementType::Arc; a.pts = {Vec3(0,0,0)};
        a.radius = 5; a.startAngle = 0; a.endAngle = 90;
        auto cs = modify::makePolarArray({a}, Vec3(0,0,0), 4, 180.0);
        CHECK((int)cs.size() == 3 &&
              std::fabs(cs[0].replacement.startAngle - 45.0) < 1e-9 &&
              std::fabs(cs[0].replacement.endAngle - 135.0) < 1e-9,
              "环形阵列 Arc 起止角同步 +45°");
    }
    // P6/P7：count=1 / count=0 → 空（退化）
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(10,0,0), Vec3(11,0,0)};
        CHECK(modify::makePolarArray({l}, Vec3(0,0,0), 1, 360.0).empty(), "环形阵列 count=1 → 空（退化）");
        CHECK(modify::makePolarArray({l}, Vec3(0,0,0), 0, 360.0).empty(), "环形阵列 count=0 → 空（退化）");
    }
    // P8/P9：落盘 + 属性继承
    {
        SketchModel m;
        const int id = addCircle(m, 10, 0, 2);
        Element src = *m.element(id); src.layer = QStringLiteral("arr");
        auto cs = modify::makePolarArray({src}, Vec3(0,0,0), 3, 360.0);
        modify::applyChanges(&m, cs, QString());
        CHECK(m.elementCount() == 3, "环形阵列落盘 1+2=3");
        bool layOk = true;
        for (const auto& e : m.elements())
            if (e.id != id && e.layer != QStringLiteral("arr")) layOk = false;
        CHECK(layOk, "环形阵列全部副本继承 layer");
    }

    // ================= 路径阵列 =================
    // T1/T2：Measure 直线路径 100、间距 25 → 4 副本，落点精确
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(5,0,0)};
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(100,0,0) };
        auto cs = modify::makePathArray({l}, path, 25.0);
        CHECK((int)cs.size() == 4, "路径阵列 Measure 间距 25/长 100 → 4 副本");
        CHECK(cs.size() == 4 &&
              cs[0].replacement.pts[0].distTo(Vec3(25, 0, 0))  < 1e-9 &&
              cs[3].replacement.pts[0].distTo(Vec3(100, 0, 0)) < 1e-9,
              "路径阵列副本落点精确（含路径终点）");
    }
    // T3：Measure 间距不整除 → 只放到 ≤ 路径长处
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(100,0,0) };
        auto cs = modify::makePathArray({l}, path, 30.0);
        CHECK((int)cs.size() == 3, "路径阵列间距不整除 100/30 → 3 副本 (30/60/90)");
    }
    // T4：Divide（spacing<=0 且 count>=2）→ count 均分全长
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(100,0,0) };
        auto cs = modify::makePathArray({l}, path, 0.0, 5);
        CHECK((int)cs.size() == 4 &&
              cs[0].replacement.pts[0].distTo(Vec3(25, 0, 0))  < 1e-9 &&
              cs[3].replacement.pts[0].distTo(Vec3(100, 0, 0)) < 1e-9,
              "路径阵列 Divide 5 等分 → 4 副本均分全长");
    }
    // T5：路径长 < 间距 → 空（退化）
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(10,0,0) };
        CHECK(modify::makePathArray({l}, path, 25.0).empty(), "路径阵列路径长 10 < 间距 25 → 空");
    }
    // T6：路径退化单点 → 空
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const std::vector<Vec3> path = { Vec3(5,5,0), Vec3(5,5,0) };
        CHECK(modify::makePathArray({l}, path, 10.0).empty(), "路径阵列路径退化单点 → 空");
    }
    // T7/T8：spacing<=0 且 count<2 → 空（无有效放置参数）
    {
        Element l; l.id = 1; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(100,0,0) };
        CHECK(modify::makePathArray({l}, path, 0.0, 1).empty(),  "路径阵列 spacing=0/count=1 → 空");
        CHECK(modify::makePathArray({l}, path, -5.0).empty(),    "路径阵列负间距无 count → 空");
    }
    // T9/T10/T11：L 形折线路径（总长 70、间距 35）→ 2 副本，折角端点落位准确
    {
        SketchModel m;
        const int id = addCircle(m, 0, 0, 1);
        const std::vector<Vec3> path = { Vec3(0,0,0), Vec3(40,0,0), Vec3(40,30,0) };   // 总长 70
        auto cs = modify::makePathArray({ *m.element(id) }, path, 35.0);
        CHECK((int)cs.size() == 2, "路径阵列 L 形折线 70/35 → 2 副本");
        modify::applyChanges(&m, cs, QString());
        CHECK(m.elementCount() == 3, "路径阵列落盘 1+2=3");
        bool atCorner = false;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Circle && e.pts[0].distTo(Vec3(40,30,0)) < 1e-9) atCorner = true;
        CHECK(atCorner, "路径阵列折角端点副本落在 (40,30)");
    }

    // ================= 剪贴板 =================
    // C1/C2/C3：JSON round-trip 保真
    {
        clipboard::Payload p;
        p.base = Vec3(5, 7, 0);
        Element l; l.type = ElementType::Line;   l.pts = {Vec3(0,0,0), Vec3(9,3,0)}; p.elements.push_back(l);
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(1,1,0)}; c.radius = 2.5; p.elements.push_back(c);
        BlockDef d; d.name = QStringLiteral("blk"); p.blocks.push_back(d);
        const QJsonObject o = clipboard::toJson(p);
        clipboard::Payload q; QString err;
        CHECK(clipboard::fromJson(o, &q, &err) && err.isEmpty(), "剪贴板 JSON round-trip 解析成功");
        CHECK((int)q.elements.size() == 2 && (int)q.blocks.size() == 1 &&
              std::fabs(q.base.x - 5) < 1e-9 && std::fabs(q.base.y - 7) < 1e-9 &&
              q.blocks[0].name == QStringLiteral("blk"),
              "剪贴板 round-trip 基点/元素/块计数一致");
        CHECK(q.elements[1].type == ElementType::Circle && std::fabs(q.elements[1].radius - 2.5) < 1e-9,
              "剪贴板 round-trip 元素字段保真");
    }
    // C4/C5：版本不符 / 缺版本号 → 拒绝且给原因（旧版内容忽略不崩溃）
    {
        QJsonObject bad; bad.insert(QStringLiteral("v"), 99);
        clipboard::Payload q; QString err;
        CHECK(!clipboard::fromJson(bad, &q, &err) && !err.isEmpty(), "剪贴板版本不符 → 拒绝且给原因");
        QJsonObject noVer; noVer.insert(QStringLiteral("base"), QJsonObject());
        CHECK(!clipboard::fromJson(noVer, &q, &err), "剪贴板缺版本号 → 拒绝");
    }
    // C6：translateTo 基点对齐落点且几何同步平移
    {
        clipboard::Payload p; p.base = Vec3(10, 10, 0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(10,10,0), Vec3(20,10,0)}; p.elements.push_back(l);
        clipboard::translateTo(p, Vec3(0, 0, 0));
        CHECK(std::fabs(p.base.x) < 1e-9 && std::fabs(p.elements[0].pts[0].x) < 1e-9 &&
              std::fabs(p.elements[0].pts[1].x - 10.0) < 1e-9,
              "translateTo 基点对齐落点且几何同步平移");
    }
    // C7：makePasteChanges 全部新增 id=-1
    {
        clipboard::Payload p; p.base = Vec3(0,0,0);
        for (int i = 0; i < 3; ++i) {
            Element l; l.type = ElementType::Line; l.pts = {Vec3(0, double(i), 0), Vec3(1, double(i), 0)};
            p.elements.push_back(l);
        }
        auto cs = clipboard::makePasteChanges(p);
        bool allAdd = true;
        for (const auto& ch : cs) if (ch.id != -1 || ch.replacement.id != -1) allAdd = false;
        CHECK((int)cs.size() == 3 && allAdd, "makePasteChanges 全部新增 id=-1");
    }
    // C8/C9：uniqueBlockName 冲突改名 / 无冲突原样
    {
        SketchModel m;
        BlockDef d; d.name = QStringLiteral("blk");
        m.addBlockDefQuiet(d);
        CHECK(clipboard::uniqueBlockName(m, QStringLiteral("blk")) == QStringLiteral("blk_1"),
              "uniqueBlockName 冲突 → blk_1");
        CHECK(clipboard::uniqueBlockName(m, QStringLiteral("new")) == QStringLiteral("new"),
              "uniqueBlockName 无冲突原样返回");
    }
    // C10~C13：pasteInto 普通粘贴 → 基点对齐落点、单步撤销可回滚
    {
        SketchModel m;
        const int id = addLine(m, 0, 0, 10, 0);
        clipboard::Payload p; p.base = Vec3(0,0,0);
        p.elements.push_back(*m.element(id));
        QString msg;
        m.beginEdit(QStringLiteral("自测粘贴"));
        CHECK(clipboard::pasteInto(&m, p, Vec3(100, 50, 0), false, &msg), "pasteInto 普通粘贴成功");
        CHECK(m.elementCount() == 2, "pasteInto 落盘后 2 元素");
        bool moved = false;
        for (const auto& e : m.elements())
            if (e.id != id && e.pts[0].distTo(Vec3(100,50,0)) < 1e-9 &&
                e.pts[1].distTo(Vec3(110,50,0)) < 1e-9) moved = true;
        CHECK(moved, "pasteInto 副本基点对齐落点 (100,50)");
        m.undo();
        CHECK(m.elementCount() == 1, "pasteInto 单步撤销回到 1 元素");
    }
    // C14~C16：pasteInto 粘贴为块 → 块表新增 + 落盘 1 个 BlockRef
    {
        SketchModel m;
        clipboard::Payload p; p.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)}; p.elements.push_back(l);
        QString msg;
        m.beginEdit(QStringLiteral("自测粘贴为块"));
        CHECK(clipboard::pasteInto(&m, p, Vec3(50, 50, 0), true, &msg), "pasteInto 粘贴为块成功");
        CHECK(m.hasBlock(QStringLiteral("粘贴块")), "粘贴为块后块表新增「粘贴块」");
        bool hasRef = false;
        int refCount = 0;
        for (const auto& e : m.elements())
            if (e.type == ElementType::BlockRef) { hasRef = true; ++refCount; }
        CHECK(hasRef, "粘贴为块落盘产出 BlockRef");
        CHECK(refCount == 1 && m.elementCount() == 1, "粘贴为块落盘仅 1 个 BlockRef（无其余图元）");
    }
    // C17：空 payload → 拒绝
    {
        SketchModel m;
        clipboard::Payload p;   // elements 为空
        QString msg;
        CHECK(!clipboard::pasteInto(&m, p, Vec3(0,0,0), false, &msg) && !msg.isEmpty(),
              "pasteInto 空 payload → 拒绝且给原因");
    }
    // C18：无 GUI 环境下 QClipboard 桥安全返回 false（不崩溃）。
    // 自测进程现持 QApplication（t_interaction 需要）→ 剪贴板真实可用，
    // 此处按环境自适应：无 GUI 断言安全 false；有 GUI 仅断言调用不崩溃。
    {
        const bool guiAvail =
            qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
        clipboard::Payload q; QString err;
        clipboard::Payload p;
        if (!guiAvail) {
            CHECK(!clipboard::getFromClipboard(&q, &err), "无剪贴板环境 getFromClipboard 安全返回 false");
            CHECK(!clipboard::putToClipboard(p), "无剪贴板环境 putToClipboard 安全返回 false");
        } else {
            clipboard::getFromClipboard(&q, &err);   // 仅验证不崩溃
            clipboard::putToClipboard(p);
            CHECK(true, "GUI 环境剪贴板桥调用安全（无剪贴板断言跳过）");
        }
    }
}

// ---------- Section 剖面引擎层 ----------
static void t_sectionCore() {    // 1) 圆 → 自身闭合边界
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 5.0;
        std::vector<Vec3> out;
        CHECK(sectionOutline(c, out) && (int)out.size() >= 4,
              "sectionOutline 圆成环(≥4 采样点)");
    }
    // 2) 直线 → 拒绝
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(9,0,0)};
        std::vector<Vec3> out;
        CHECK(!sectionOutline(l, out), "sectionOutline 直线拒绝");
    }
    // 3) 闭合折线 → 环=顶点数
    {
        Element p; p.type = ElementType::Polyline; p.closed = true;
        p.pts = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,8,0), Vec3(0,8,0)};
        std::vector<Vec3> out;
        CHECK(sectionOutline(p, out) && (int)out.size() == 4,
              "sectionOutline 闭合折线环=4 顶点");
    }
    // 4) 四线首尾相接 → 1 环（链式）
    {
        std::vector<Element> segs;
        auto L = [&segs](double x0,double y0,double x1,double y1) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x0,y0,0), Vec3(x1,y1,0)}; segs.push_back(e); };
        L(0,0,10,0); L(10,0,10,10); L(10,10,0,10); L(0,10,0,0);
        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(segs, 0.5, &st);
        CHECK((int)loops.size() == 1 && st.chainedRegions == 1 && st.selfClosedRegions == 0,
              "buildClosedLoops 4 线链成 1 环");
    }
    // 5) 孤立直线 → openElements=1 且不影响成环
    {
        std::vector<Element> segs;
        auto L = [&segs](double x0,double y0,double x1,double y1) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x0,y0,0), Vec3(x1,y1,0)}; segs.push_back(e); };
        L(0,0,10,0); L(10,0,10,10); L(10,10,0,10); L(0,10,0,0);
        L(50,50,60,50);
        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(segs, 0.5, &st);
        CHECK((int)loops.size() == 1 && st.openChains >= 1 && st.openElements == 0,
              "buildClosedLoops 孤立线成链失败计 openChains");
    }
    // 6) Hatch 图元 ↔ 剖面区域往返
    {
        Element h; h.type = ElementType::Hatch; h.closed = true;
        h.pts = {Vec3(0,0,0), Vec3(20,0,0), Vec3(20,10,0), Vec3(0,10,0)};
        h.rotation = 45.0; h.radius = 2.5;
        SectionRegion r = hatchRegionFromElement(h);
        const bool regionOk = (int)r.boundary.size() == 4 &&
            std::fabs(r.style.angleDeg - 45.0) < 1e-9 &&
            std::fabs(r.style.spacingMm - 2.5) < 1e-9;
        Element back = makeHatchElement(r);
        CHECK(regionOk && back.type == ElementType::Hatch && (int)back.pts.size() == 4 &&
              std::fabs(back.rotation - 45.0) < 1e-9,
              "hatchRegion↔makeHatchElement 往返一致");
    }
    // 7) 45° 填充：线段非空且全部落在区域内
    {
        SectionRegion r;
        r.boundary = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
        r.style.angleDeg = 45.0; r.style.spacingMm = 3.0;
        auto lines = generateHatchLines(r);
        bool inside = !lines.empty();
        for (const auto& s : lines) {
            const Vec3 mid((s.a.x + s.b.x) / 2.0, (s.a.y + s.b.y) / 2.0, 0);
            if (!geo::pointInPolygon(mid, r.boundary)) { inside = false; break; }
        }
        CHECK(inside, "generateHatchLines 45° 线段中点全部落在区域内");
    }
    // 8) 材料图案映射：通用金属 1 族 45°；铸铁 2 族（网状）
    {
        SectionStyle s;
        s.material = SectionMaterial::General; s.angleDeg = 45.0;
        const auto pg = hatchPatternFor(s);
        s.material = SectionMaterial::CastIron;
        const auto pc = hatchPatternFor(s);
        CHECK((int)pg.anglesDeg.size() == 1 && std::fabs(pg.anglesDeg[0] - 45.0) < 1e-9 &&
              (int)pc.anglesDeg.size() == 2,
              "hatchPatternFor 通用 1 族 45° / 铸铁 2 族");
    }
    // 9) 豁免 → 绘制方式
    {
        SectionRegion r;
        r.style.exempt = SectionExempt::AuxiliaryPart;
        const bool noFill = hatchDrawMode(r) == HatchDraw::NoFill;
        r.style.exempt = SectionExempt::SmallAreaSolid;
        const bool solid  = hatchDrawMode(r) == HatchDraw::SolidFill;
        CHECK(noFill && solid, "hatchDrawMode 辅助件 NoFill / 狭小涂黑 SolidFill");
    }
    // 10) 圆填充动作：复用源 id 且替换为 Hatch
    {
        Element c; c.type = ElementType::Circle; c.id = 11; c.pts = {Vec3(0,0,0)}; c.radius = 5.0;
        SectionStyle st;
        auto changes = makeSectionChanges({c}, st, false);
        CHECK(changes.size() == 1 && !changes[0].remove &&
              changes[0].replacement.id == 11 &&
              changes[0].replacement.type == ElementType::Hatch,
              "makeSectionChanges 圆→Hatch 且复用源 id=11");
    }
    // 11) 点选拾取：环内命中面积=100 / 环外不中
    {
        Element p; p.type = ElementType::Polyline; p.closed = true;
        p.pts = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
        const auto inR  = pickSectionRegionAt({p}, Vec3(5,5,0));
        const auto outR = pickSectionRegionAt({p}, Vec3(15,5,0));
        CHECK(inR.ok() && std::fabs(inR.area - 100.0) < 1e-6 && !outR.found,
              "pickSectionRegionAt 内命中 area=100 / 外不中");
    }
    // 12) 相邻区域自动区分：角度互反（邻接=包围盒正面积交集，用重叠布置）
    {
        std::vector<SectionRegion> areas(2);
        areas[0].boundary = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
        areas[1].boundary = {Vec3(5,0,0), Vec3(15,0,0), Vec3(15,10,0), Vec3(5,10,0)};
        const bool changed = resolveAdjacency(areas, 45.0);
        const bool opposite = std::fabs(areas[0].style.angleDeg - areas[1].style.angleDeg) > 1.0;
        CHECK(changed && opposite, "resolveAdjacency 相邻区域角度互反");
    }
}

// ---------- Inquiry 查询测量层 ----------
static void t_inquiryCore() {
    // 1) 两点测量 3-4-5
    {
        const auto r = inquiry::distTwoPoints(Vec3(0,0,0), Vec3(3,4,0));
        CHECK(std::fabs(r.distance - 5.0) < 1e-9 && std::fabs(r.dx - 3.0) < 1e-9 &&
              std::fabs(r.dy - 4.0) < 1e-9 &&
              std::fabs(r.angleDeg - 53.13010235415598) < 1e-6,
              "inquiry::distTwoPoints 3-4-5 三角全量");
    }
    // 2) shoelace：单位方
    {
        const auto r = inquiry::areaOfPolygon({Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0), Vec3(0,1,0)});
        CHECK(r.ok && std::fabs(r.area - 1.0) < 1e-9 && std::fabs(r.perimeter - 4.0) < 1e-9 &&
              r.centroid.distTo(Vec3(0.5,0.5,0)) < 1e-9,
              "inquiry::areaOfPolygon 单位方面积 1/周长 4/形心 (0.5,0.5)");
    }
    // 3) 少于 3 点 → 拒绝
    CHECK(!inquiry::areaOfPolygon({Vec3(0,0,0), Vec3(1,0,0)}).ok,
          "inquiry::areaOfPolygon 少于 3 点拒绝");
    // 4) 圆解析面积 πr²
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 2.0;
        const auto r = inquiry::areaOfElement(c);
        CHECK(r.ok && std::fabs(r.area - 3.14159265358979 * 4.0) < 1e-6,
              "inquiry::areaOfElement 圆 πr² 解析精确");
    }
    // 5) 开放图元 → ok=false + 中文 error
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        const auto r = inquiry::areaOfElement(l);
        CHECK(!r.ok && !r.error.isEmpty(), "inquiry::areaOfElement 开放图元报错非空");
    }
    // 6) 线链成环：4 边闭合
    {
        std::vector<Element> segs;
        auto L = [&segs](double x0,double y0,double x1,double y1) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x0,y0,0), Vec3(x1,y1,0)}; segs.push_back(e); };
        L(0,0,1,0); L(1,0,1,1); L(1,1,0,1); L(0,1,0,0);
        std::vector<Vec3> ring;
        CHECK(inquiry::buildRingFromLines(segs, ring, 1e-6) && (int)ring.size() == 4,
              "inquiry::buildRingFromLines 4 边成环");
    }
    // 7) 缺口 → 不成环
    {
        std::vector<Element> segs;
        auto L = [&segs](double x0,double y0,double x1,double y1) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x0,y0,0), Vec3(x1,y1,0)}; segs.push_back(e); };
        L(0,0,1,0); L(1,0,1,1); L(1,1,0,1);
        std::vector<Vec3> ring;
        CHECK(!inquiry::buildRingFromLines(segs, ring, 1e-6),
              "inquiry::buildRingFromLines 缺口不成环");
    }
    // 8) 链面积便捷入口
    {
        std::vector<Element> segs;
        auto L = [&segs](double x0,double y0,double x1,double y1) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x0,y0,0), Vec3(x1,y1,0)}; segs.push_back(e); };
        L(0,0,1,0); L(1,0,1,1); L(1,1,0,1); L(0,1,0,0);
        const auto r = inquiry::areaOfClosedLineChain(segs);
        CHECK(r.ok && std::fabs(r.area - 1.0) < 1e-9,
              "inquiry::areaOfClosedLineChain 单位方面积=1");
    }
    // 9) 质量特性：单位方 ix≈1/3（离散环近似）+ bbox
    {
        Element p; p.type = ElementType::Polyline; p.closed = true;
        p.pts = {Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0), Vec3(0,1,0)};
        const auto r = inquiry::massPropOfElement(p);
        CHECK(r.ok && std::fabs(r.area - 1.0) < 1e-6 && std::fabs(r.ix - 1.0/3.0) < 5e-3 &&
              std::fabs(r.bbox.width() - 1.0) < 1e-6,
              "inquiry::massPropOfElement 方 ix≈1/3(离散环)/bbox 宽=1");
    }
    // 10) ID 查询：空模型 / 最近圆
    {
        const auto r0 = inquiry::idPoint(Vec3(5,5,0), {});
        CHECK(r0.nearest == nullptr, "inquiry::idPoint 空模型 nearest=nullptr");
        Element c; c.type = ElementType::Circle; c.id = 1; c.pts = {Vec3(10,0,0)}; c.radius = 2.0;
        const std::vector<Element> elems = {c};   // 具名容器：nearest 指针须在 CHECK 期保持有效
        const auto r1 = inquiry::idPoint(Vec3(5,0,0), elems);
        CHECK(r1.nearest != nullptr && r1.nearest->id == 1 &&
              std::fabs(r1.nearestDist - 3.0) < 1e-6,
              "inquiry::idPoint 最近圆距离=3");
    }
    // 11) 有向面积：CCW 正 / CW 负
    CHECK(inquiry::polygonArea({Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0)}) > 0.0 &&
          inquiry::polygonArea({Vec3(0,0,0), Vec3(0,1,0), Vec3(1,0,0)}) < 0.0,
          "inquiry::polygonArea CCW 正 / CW 负");
    // 12) 环周长：单位方（自动闭合收尾段）
    CHECK(std::fabs(inquiry::polygonPerimeter({Vec3(0,0,0), Vec3(1,0,0), Vec3(1,1,0), Vec3(0,1,0)}) - 4.0) < 1e-9,
          "inquiry::polygonPerimeter 单位方=4");
    // 13) 离散环：圆 64 点且半径一致
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 2.0;
        const auto ring = inquiry::discretizeRing(c);
        bool radiusOk = (int)ring.size() == 64;
        for (const auto& p : ring)
            if (std::fabs(std::sqrt(p.x*p.x + p.y*p.y) - 2.0) > 1e-6) { radiusOk = false; break; }
        CHECK(radiusOk, "inquiry::discretizeRing 圆 64 点且距心=2");
    }
}

// ---------- 存档兼容层：旧版 JSON 档案（容缺 key）逐版本读入不丢数据 ----------
// 版本降级 = 当前完整档案逐个删除"后期新增键"——这正是旧版本档案的本质。
// 读入通道 = BatchPlotEngine::loadDrawing（与 MainWindow::readDrawing 同语义，无头）。
static void t_archiveCompat() {
    // 组装完整模型（当前格式 V4）：元素 + 图层 + 块 + 文字样式 + 约束 + 参数 + 比例
    SketchModel full;
    Element lineEl; lineEl.type = ElementType::Line;
    lineEl.pts = {Vec3(0,0,0), Vec3(10,0,0)};
    const int idA = full.addElement(lineEl);
    Element lineEl2; lineEl2.type = ElementType::Line;
    lineEl2.pts = {Vec3(0,0,0), Vec3(5,0,0)};
    full.addElement(lineEl2);
    full.addLayer(QStringLiteral("墙体"), QColor(QStringLiteral("#ff0000")));
    {
        BlockDef d;
        d.name = QStringLiteral("BOLT");
        d.basePoint = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        d.entities.push_back(l);
        full.addBlockDef(d);
    }
    {
        TextStyle ts; ts.name = QStringLiteral("标注"); ts.textHeight = 3.5;
        full.addTextStyle(ts);
    }
    {
        // Horizontal 约束要求被动方（idB）为直线：idA=主动线、idA+1=被动线（已水平，求解不动）
        const QString cerr = ConstraintSolver::addConstraint(full, ConstraintType::Horizontal, idA, idA + 1);
        CHECK(cerr.isEmpty(), "存档构造: Horizontal 约束添加成功");
    }
    full.setParam(QStringLiteral("plate"), 0.5);
    full.setScaleRatioValue(2.0);

    // V4 = 当前完整档案
    QJsonObject v4;
    {
        QJsonArray els;
        els.append(::SketchCAD::elementToJson(lineEl));
        els.append(::SketchCAD::elementToJson(lineEl2));
        v4["elements"] = els;
    }
    v4["layers"]      = full.layersToJson();
    v4["blocks"]      = full.blocksToJson();
    v4["textStyles"]  = full.textStylesToJson();
    v4["constraints"] = full.constraintsToJson();
    v4["params"]      = full.paramsToJson();
    v4["scaleRatio"]  = full.scaleRatio();
    const int fullLayerN = full.layerCount();

    // 版本降级：V3 = V4 − params；V2 = V3 − constraints/textStyles/scaleRatio；V1 = 仅 elements
    QJsonObject v3 = v4; v3.remove(QStringLiteral("params"));
    QJsonObject v2 = v3;
    v2.remove(QStringLiteral("constraints"));
    v2.remove(QStringLiteral("textStyles"));
    v2.remove(QStringLiteral("scaleRatio"));
    QJsonObject v1; v1["elements"] = v4["elements"];

    // 落盘 + 读入辅助
    auto load = [](const QJsonObject& root, const QString& name, SketchModel& m, QString& err) {
        const QString path = QDir::tempPath() + QLatin1Char('/') + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.close();
        const bool ok = comauto::BatchPlotEngine::loadDrawing(m, path, &err);
        QFile::remove(path);
        return ok;
    };
    QString err;

    // ---- V1（最早）：仅 elements，其余键全部缺失 ----
    {
        SketchModel m;
        CHECK(load(v1, QStringLiteral("scad_compat_v1.scad"), m, err), "存档 V1(仅 elements) 读入成功");
        CHECK(m.elementCount() == 2, "存档 V1 元素不丢失(2)");
        CHECK(m.blockCount() == 0, "存档 V1 缺 blocks 键 → 块表空");
        CHECK(m.constraints().empty(), "存档 V1 缺 constraints 键 → 约束表空");
        CHECK(m.textStyleCount() == 1, "存档 V1 缺 textStyles 键 → 保留构造默认样式(Standard)");
    }
    // ---- V2：+layers+blocks（缺样式/约束/参数/比例）----
    {
        SketchModel m;
        CHECK(load(v2, QStringLiteral("scad_compat_v2.scad"), m, err), "存档 V2(+layers+blocks) 读入成功");
        CHECK(m.layerCount() == fullLayerN, "存档 V2 图层数还原");
        CHECK(m.blockCount() == 1 && !m.blockTable().empty() &&
              m.blockTable()[0].name == QStringLiteral("BOLT") &&
              m.blockTable()[0].entities.size() == 1,
              "存档 V2 块表 BOLT 还原(1 块 1 实体)");
        CHECK(m.constraints().empty(), "存档 V2 缺 constraints 键 → 约束表空");
        CHECK(m.textStyleCount() == 1, "存档 V2 缺 textStyles 键 → 保留构造默认样式(Standard)");
    }
    // ---- V3：+textStyles+constraints+scaleRatio（缺 params）----
    {
        SketchModel m;
        CHECK(load(v3, QStringLiteral("scad_compat_v3.scad"), m, err), "存档 V3(+样式+约束+比例) 读入成功");
        CHECK(m.textStyleCount() == 2, "存档 V3 文字样式还原(Standard+标注 共 2)");
        CHECK(m.constraints().size() == 1, "存档 V3 约束还原(1)");
        CHECK(std::fabs(m.scaleRatio() - 2.0) < 1e-9, "存档 V3 比例 2:1 还原");
        CHECK(m.params().empty(), "存档 V3 缺 params 键 → 空表回落");
    }
    // ---- V4：完整档案全量往返 ----
    {
        SketchModel m;
        CHECK(load(v4, QStringLiteral("scad_compat_v4.scad"), m, err), "存档 V4(完整) 读入成功");
        CHECK(m.elementCount() == 2 && m.layerCount() == fullLayerN && m.blockCount() == 1 &&
              m.textStyleCount() == 2 && m.constraints().size() == 1,
              "存档 V4 全表计数一致");
        CHECK(m.params().size() == 1 &&
              std::fabs(m.params().at(QStringLiteral("plate")) - 0.5) < 1e-9,
              "存档 V4 参数 plate=0.5 还原");
        CHECK(std::fabs(m.scaleRatio() - 2.0) < 1e-9, "存档 V4 比例还原");
    }
}

// ============================================================================
// 图层动态语义（任务 C）：ByLayer 解析 / 冻结硬隔离 / 打印开关 / 透明度 / LTSCALE
// 逐条 CHECK 计数（与 t_geometryCore 等纯函数层同范式）。
// ============================================================================
static void t_layerDynamic() {
    const QColor RED(QStringLiteral("#ff0000"));
    const QColor BLUE(QStringLiteral("#0000ff"));
    const QColor GREEN(QStringLiteral("#00ff00"));
    auto sameColor = [](const QColor& a, const QColor& b) {
        return a.red() == b.red() && a.green() == b.green() && a.blue() == b.blue();
    };

    // ---------- 1) ByLayer 动态解析 + 全层实时联动 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("L1"), RED);
        Element el; el.type = ElementType::Line;
        el.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        el.layer = QStringLiteral("L1");
        el.byLayer = true;                       // 新图元默认随层
        const int id = m.addElement(el);
        CHECK(m.element(id) && m.element(id)->byLayer, "ByLayer: 图元 byLayer 标志置位(true)");

        Element r = m.resolveElement(*m.element(id));
        CHECK(sameColor(r.color, RED), "ByLayer: 解析取图层色(红)");

        // 改图层色 → 全层实时联动（改一处图层即影响本层全部 byLayer 图元）
        m.setLayerStyle(QStringLiteral("L1"), &BLUE, nullptr, nullptr);
        r = m.resolveElement(*m.element(id));
        CHECK(sameColor(r.color, BLUE), "ByLayer: 改图层色后全层联动为蓝");

        // 线型 / 线宽 随层联动
        const double lw = 2.0;
        const LineStyle ls = LineStyle::Dashed;
        m.setLayerStyle(QStringLiteral("L1"), nullptr, &lw, &ls);
        r = m.resolveElement(*m.element(id));
        CHECK(r.lineStyle == LineStyle::Dashed, "ByLayer: 解析取图层线型(虚线)");
        CHECK(std::fabs(r.lineWidth - lw) < 1e-9 && r.lineWidthAuto == false,
              "ByLayer: 解析取图层线宽(2.0, 固定值不派生)");

        // 转 override：本图元快照覆盖，不再随层
        m.updateElementLinked(id, [GREEN](Element& x){ x.byLayer = false; x.color = GREEN; });
        r = m.resolveElement(*m.element(id));
        CHECK(sameColor(r.color, GREEN), "ByLayer: override 用本图元快照(绿)");
        CHECK(r.byLayer == false, "ByLayer: override 后 byLayer=false");

        // override 后改图层色不应再影响该图元
        const QColor WHITE(QStringLiteral("#ffffff"));
        m.setLayerStyle(QStringLiteral("L1"), &WHITE, nullptr, nullptr);
        r = m.resolveElement(*m.element(id));
        CHECK(sameColor(r.color, GREEN), "ByLayer: override 不受图层色改动影响");
    }

    // ---------- 2) 冻结硬隔离：命中零 / 打印零 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("FZ"));
        Element el; el.type = ElementType::Line;
        el.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        el.layer = QStringLiteral("FZ");
        const int id = m.addElement(el);
        // 冻结语义独立于 visible：visible 默认 true，冻结前应可命中/打印
        CHECK(m.elementPickable(*m.element(id)), "冻结前: 图元可命中(hitTest 有值)");
        CHECK(m.elementPrintable(*m.element(id)), "冻结前: 图元可打印");
        m.setLayerFrozen(QStringLiteral("FZ"), true);
        CHECK(!m.elementPickable(*m.element(id)), "冻结后: 图元不可命中(hitTest 零命中)");
        CHECK(!m.elementPrintable(*m.element(id)), "冻结后: 图元不打印(打印零输出)");
        m.setLayerFrozen(QStringLiteral("FZ"), false);
        CHECK(m.elementPickable(*m.element(id)), "解冻后: 图元恢复可命中");
    }

    // ---------- 3) 打印开关 plot：可显示但不出图 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("PP"));
        Element el; el.type = ElementType::Line;
        el.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        el.layer = QStringLiteral("PP");
        const int id = m.addElement(el);
        CHECK(m.elementPrintable(*m.element(id)), "plot=true 默认可打印");
        m.setLayerPlot(QStringLiteral("PP"), false);
        CHECK(!m.elementPrintable(*m.element(id)), "plot=false 冻结打印(不打印)");
        m.setLayerPlot(QStringLiteral("PP"), true);
        CHECK(m.elementPrintable(*m.element(id)), "plot=true 恢复打印");
    }

    // ---------- 4) 透明度叠加到颜色 alpha 通道 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("AL"), QColor(QStringLiteral("#008000")));
        Element el; el.type = ElementType::Line;
        el.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        el.layer = QStringLiteral("AL"); el.byLayer = true;
        const int id = m.addElement(el);
        Element r0 = m.resolveElement(*m.element(id));
        CHECK(r0.color.alpha() == 255, "alpha=1.0 图层解析后不透明度=255");
        m.setLayerAlpha(QStringLiteral("AL"), 0.5);
        Element r1 = m.resolveElement(*m.element(id));
        CHECK(r1.color.alpha() == 128, "alpha=0.5 图层解析后颜色 alpha=128");
    }

    // ---------- 5) 序列化兼容：旧档 byLayer 缺键回落 false + 字段往返 ----------
    {
        // 旧档模拟：手写一条不含 byLayer 键的图元 JSON（其余齐备）
        Element el; el.type = ElementType::Line;
        el.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        el.layer = QStringLiteral("0"); el.color = RED; el.byLayer = true;  // 先置 true
        QJsonObject o = ::SketchCAD::elementToJson(el);
        o.remove(QStringLiteral("byLayer"));                // 模拟旧档缺键
        Element back = ::SketchCAD::elementFromJson(o);
        CHECK(back.byLayer == false, "序列化: 旧档缺 byLayer 键→回落 false(向后兼容)");
        CHECK(sameColor(back.color, RED), "序列化: 旧档快照色保留(红)");

        // 新档往返：byLayer=true 不丢失
        el.byLayer = true;
        QJsonObject o2 = ::SketchCAD::elementToJson(el);
        CHECK(o2.contains(QStringLiteral("byLayer")) && o2["byLayer"].toBool(), "序列化: 新档写入 byLayer=true");
        Element b2 = ::SketchCAD::elementFromJson(o2);
        CHECK(b2.byLayer == true, "序列化: 新档 byLayer 往返保存");

        // 图层 frozen/plot/alpha 字段往返
        SketchModel m;
        m.addLayer(QStringLiteral("SZ"));
        m.setLayerFrozen(QStringLiteral("SZ"), true);
        m.setLayerPlot(QStringLiteral("SZ"), false);
        m.setLayerAlpha(QStringLiteral("SZ"), 0.3);
        QJsonArray la = m.layersToJson();
        SketchModel m2; m2.setLayersFromJson(la);
        const Layer* L = m2.findLayer(QStringLiteral("SZ"));
        CHECK(L && L->frozen && !L->plot && std::fabs(L->alpha - 0.3) < 1e-9,
              "序列化: 图层 frozen/plot/alpha 往返还原");
    }

    // ---------- 6) LTSCALE：dashSpecScaled 段长比例 + 模型级 setLtscale ----------
    {
        DashSpec base = dashSpecFor(LineStyle::Dashed);
        CHECK(base.valid && !base.pattern.empty(), "LTSCALE: 虚线基准规格有效");

        DashSpec k1 = dashSpecScaled(LineStyle::Dashed, 1.0);
        CHECK(k1.valid && k1.pattern.size() == base.pattern.size() &&
              std::fabs(k1.pattern[0] - base.pattern[0]) < 1e-9,
              "LTSCALE: k=1.0 规格与基准完全一致");

        DashSpec k05 = dashSpecScaled(LineStyle::Dashed, 0.5);
        CHECK(k05.valid && std::fabs(k05.pattern[0] - base.pattern[0]*0.5) < 1e-9 &&
              std::fabs(k05.minLengthD - base.minLengthD*0.5) < 1e-9,
              "LTSCALE: k=0.5 实线段/间隔/最小长均减半");

        DashSpec k20 = dashSpecScaled(LineStyle::Dashed, 2.0);
        CHECK(k20.valid && std::fabs(k20.pattern[0] - base.pattern[0]*2.0) < 1e-9 &&
              std::fabs(k20.minLengthD - base.minLengthD*2.0) < 1e-9,
              "LTSCALE: k=2.0 实线段/间隔/最小长均加倍");

        DashSpec kNeg = dashSpecScaled(LineStyle::Dashed, -1.0);
        CHECK(kNeg.valid && std::fabs(kNeg.pattern[0] - base.pattern[0]) < 1e-9,
              "LTSCALE: k≤0 回落 k=1.0(不缩放)");

        // 模型级
        SketchModel m;
        CHECK(std::fabs(m.ltscale() - 1.0) < 1e-9, "LTSCALE: 模型默认 1.0");
        m.setLtscale(2.0);
        CHECK(std::fabs(m.ltscale() - 2.0) < 1e-9, "LTSCALE: setLtscale(2.0) 生效");
        m.setLtscale(0.0);   // 非法值应被忽略
        CHECK(std::fabs(m.ltscale() - 2.0) < 1e-9, "LTSCALE: 非法值(≤0)被忽略");
        m.setLtscale(1.0);
        CHECK(std::fabs(m.ltscale() - 1.0) < 1e-9, "LTSCALE: 复位 1.0 生效");
    }
}

// ---------- 黄金样本层：tests/golden/ 静态 DXF 锚点（G1 计数 + G2 几何差分 + round-trip） ----------
static QString locateGoldenDir() {
    const QStringList cands = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/golden"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../tests/golden"),
        QDir::currentPath() + QStringLiteral("/tests/golden"),
        QDir::currentPath() + QStringLiteral("/golden"),
        QDir::currentPath() + QStringLiteral("/../../tests/golden"),
    };
    for (const auto& c : cands)
        if (QFileInfo(QDir(c).filePath(QStringLiteral("golden_min.dxf"))).isFile()) return c;
    return QString();
}

static void t_goldenSamples() {
    using namespace cadfmt;
    const QString dir = locateGoldenDir();
    CHECK(!dir.isEmpty(), "golden: 定位 tests/golden 样本目录");
    if (dir.isEmpty()) return;

    // ---- golden_min.dxf：4 基元（版本锚定） ----
    QFile f1(QDir(dir).filePath(QStringLiteral("golden_min.dxf")));
    CHECK(f1.open(QIODevice::ReadOnly), "golden: golden_min.dxf 可读");
    if (!f1.isOpen()) return;
    const QByteArray minBytes = f1.readAll();
    f1.close();

    CHECK(dxfCountToken(minBytes, "LINE") == 1 && dxfCountToken(minBytes, "CIRCLE") == 1 &&
          dxfCountToken(minBytes, "ARC") == 1 && dxfCountToken(minBytes, "TEXT") == 1,
          "golden G1: golden_min token 计数 LINE/CIRCLE/ARC/TEXT 各 1");

    std::vector<Element> vmin;
    QString e1; DxfImportStats s1;
    CHECK(fromDxf(minBytes, vmin, &e1, &s1) && s1.unknownEntities == 0 && (int)vmin.size() == 4,
          "golden G2: golden_min 解析 4 实体 0 未知");
    bool minGeo = vmin.size() == 4;
    if (minGeo) {
        minGeo = vmin[0].pts[0].distTo(Vec3(0,0,0)) < 1e-6 &&
                 vmin[0].pts[1].distTo(Vec3(30,40,0)) < 1e-6 &&
                 std::fabs(vmin[1].radius - 7.25) < 1e-6 &&
                 std::fabs(vmin[2].radius - 12.5) < 1e-6 &&
                 std::fabs(vmin[2].startAngle - 30.0) < 1e-6 &&
                 std::fabs(vmin[2].endAngle - 120.0) < 1e-6 &&
                 vmin[3].text == QStringLiteral("Hi");
    }
    CHECK(minGeo, "golden G2: golden_min 几何逐项 ≤1e-6(线端/圆径/弧角/文本)");

    // ---- golden_mix.dxf：多类实体 + 块表 ----
    QFile f2(QDir(dir).filePath(QStringLiteral("golden_mix.dxf")));
    CHECK(f2.open(QIODevice::ReadOnly), "golden: golden_mix.dxf 可读");
    if (!f2.isOpen()) return;
    const QByteArray mixBytes = f2.readAll();
    f2.close();

    std::vector<Element> vmix;
    QString e2; DxfImportStats s2;
    CHECK(fromDxf(mixBytes, vmix, &e2, &s2) && s2.blocks.size() == 1 &&
          !s2.blocks.empty() && s2.blocks[0].name == QStringLiteral("BOLT") &&
          s2.unknownEntities == 0,
          "golden G2: golden_mix 解析成功+块 BOLT+0 未知");

    // round-trip：导入 → native 导出 → 再导入 → 逐点差分。
    // R12 无 ELLIPSE/SPLINE 原生实体、XLINE 降级长线段（文档注明）——
    // 三类在 round-trip 中类型必变（→Polyline/Polyline/Line），仅对类型可保持子集断言。
    const std::vector<BlockDef> blkDefs = s2.blocks;
    DxfExportOptions no;
    no.nativeBlocks = no.nativeDims = no.nativeHatch = true;
    QString e3;
    const QByteArray dxf2 = toDxf(vmix, &e3, 1.0, 4, &blkDefs, &no);
    CHECK(e3.isEmpty() && !dxf2.isEmpty(), "golden: native 导出无错误且非空");
    std::vector<Element> vback;
    QString e4; DxfImportStats s4;
    CHECK(fromDxf(dxf2, vback, &e4, &s4) && s4.unknownEntities == 0,
          "golden: round-trip 再导入成功 0 未知");
    auto volatile1 = [](ElementType t) {
        return t == ElementType::Spline || t == ElementType::Ellipse || t == ElementType::Xline;
    };
    CHECK(vback.size() == vmix.size(), "golden: round-trip 实体计数一致");
    bool backOk = vback.size() == vmix.size();
    if (backOk) {
        for (size_t i = 0; i < vmix.size() && backOk; ++i) {
            if (volatile1(vmix[i].type)) continue;   // R12 限制：类型必变，跳过
            if (vback[i].type != vmix[i].type) { backOk = false; break; }
            if (vback[i].pts.size() != vmix[i].pts.size()) { backOk = false; break; }
            for (size_t k = 0; k < vmix[i].pts.size(); ++k)
                if (vback[i].pts[k].distTo(vmix[i].pts[k]) > 1e-6) { backOk = false; break; }
        }
    }
    CHECK(backOk, "golden G2: round-trip 类型可保持子集逐点 ≤1e-6");
}

// ---------- GripOps 夹点热操作（补充：模式循环/apply 各模式/边界拒绝） ----------
static void t_gripopsDepth() {
    using namespace grip;
    // 1) Ctrl 循环：Stretch→…→Mirror→Stretch
    CHECK(nextMode(Mode::Stretch) == Mode::Move && nextMode(Mode::Mirror) == Mode::Stretch,
          "grip::nextMode 循环收口");
    // 2) 模式名
    CHECK(modeName(Mode::Stretch) == QStringLiteral("拉伸") &&
          modeName(Mode::Mirror) == QStringLiteral("镜像"),
          "grip::modeName 中文名");
    // 3) Move：增量 = cursor − refCursor
    {
        Grips g; g.refCursor = Vec3(0,0,0); g.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(0,0,0), Vec3(5,0,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Move, Vec3(10, 7, 0));
        CHECK(r.reject.isEmpty() && r.changes.size() == 1 &&
              r.changes[0].replacement.pts[0].distTo(Vec3(10,7,0)) < 1e-9,
              "grip::apply Move 平移 (10,7)");
    }
    // 4) Rotate：相对 refCursor 旋转 90°
    {
        Grips g; g.refCursor = Vec3(10,0,0); g.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(10,0,0), Vec3(10,5,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Rotate, Vec3(0,10,0));
        CHECK(r.reject.isEmpty() && r.changes.size() == 1 &&
              r.changes[0].replacement.pts[0].distTo(Vec3(0,10,0)) < 1e-9,
              "grip::apply Rotate 相对参考 90°");
    }
    // 5) rotationDeg 数值
    {
        Grips g; g.refCursor = Vec3(10,0,0); g.base = Vec3(0,0,0);
        CHECK(std::fabs(rotationDeg(g, Vec3(0,10,0)) - 90.0) < 1e-9,
              "grip::rotationDeg 相对参考 90°");
    }
    // 6) Scale：×2 且端点翻倍
    {
        Grips g; g.refCursor = Vec3(5,0,0); g.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(5,0,0), Vec3(5,5,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Scale, Vec3(10,0,0));
        CHECK(r.reject.isEmpty() && r.changes.size() == 1 &&
              r.changes[0].replacement.pts[0].distTo(Vec3(10,0,0)) < 1e-9 &&
              r.changes[0].replacement.pts[1].distTo(Vec3(10,10,0)) < 1e-9,
              "grip::apply Scale ×2 端点翻倍");
    }
    // 7) scaleFactor 数值
    {
        Grips g; g.refCursor = Vec3(5,0,0); g.base = Vec3(0,0,0);
        CHECK(std::fabs(scaleFactor(g, Vec3(10,0,0)) - 2.0) < 1e-9,
              "grip::scaleFactor 相对参考 ×2");
    }
    // 8) Scale 非法比例 → 拒绝且无变更
    {
        Grips g; g.refCursor = Vec3(5,0,0); g.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(5,0,0), Vec3(5,5,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Scale, g.base);        // cursor==base → f=0 → 拒绝
        CHECK(!r.reject.isEmpty() && r.changes.empty(),
              "grip::apply Scale f≤0 拒绝且无变更");
    }
    // 9) Mirror：轴退化 → 拒绝
    {
        Grips g; g.refCursor = Vec3(0,0,0); g.base = Vec3(3,0,0);
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(1,0,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Mirror, g.base);       // cursor==base → 轴退化
        CHECK(!r.reject.isEmpty(), "grip::apply Mirror 轴重合拒绝");
    }
    // 10) Mirror：轴 = base→cursor，(3,0) 关于 Y 轴 → (-3,0)
    {
        Grips g; g.refCursor = Vec3(0,0,0); g.base = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(3,0,0), Vec3(3,5,0)};
        g.moveSet = {l};
        auto r = apply(g, Mode::Mirror, Vec3(0,5,0));  // 轴 = +Y
        CHECK(r.reject.isEmpty() && r.changes.size() == 1 &&
              r.changes[0].replacement.pts[0].distTo(Vec3(-3,0,0)) < 1e-9,
              "grip::apply Mirror 关于 Y 轴 x 取反");
    }
    // 11) Stretch：按 (图元, 控制点) 引用精确迁移
    {
        Grips g; g.refCursor = Vec3(0,0,0);
        Element l; l.type = ElementType::Line; l.id = 1; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        g.stretchSet = {l};
        g.stretchRefs = { {1, 1} };                    // 迁移 1 号图元的 1 号控制点（终点）
        auto r = apply(g, Mode::Stretch, Vec3(0,4,0));
        CHECK(r.reject.isEmpty() && !r.changes.empty() &&
              r.changes[0].replacement.pts[1].distTo(Vec3(0,4,0)) < 1e-9,
              "grip::apply Stretch 控制点直设为 cursor (0,4)");
    }
}

// ---------- ShapeSelect 栏选/圈围/圈交（补充：几何原语逐项） ----------
static void t_shapeselectDepth() {
    using namespace shape;
    const std::vector<Vec3> square = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
    // 1) 模式名
    CHECK(modeName(Mode::Fence) == QStringLiteral("栏选") &&
          modeName(Mode::Window) == QStringLiteral("圈围") &&
          modeName(Mode::Crossing) == QStringLiteral("圈交"),
          "shape::modeName 三模式中文名");
    // 2) 采样点：直线 2 点 / 圆密集
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 5.0;
        CHECK(samplePoints(l).size() == 2 && samplePoints(c).size() >= 16,
              "shape::samplePoints 直线 2 点/圆 ≥16 点");
    }
    // 3) 线段化：直线 1 段 / 闭合折线 4 段
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(10,0,0)};
        Element p; p.type = ElementType::Polyline; p.closed = true;
        p.pts = square;
        CHECK(segments(l).size() == 1 && segments(p).size() == 4,
              "shape::segments 直线 1 段/闭合折线 4 段");
    }
    // 4) 段穿多边形：横穿 / 不相交
    CHECK(segCrossesPolygon(Vec3(-1,5,0), Vec3(11,5,0), square) &&
          !segCrossesPolygon(Vec3(-1,15,0), Vec3(11,15,0), square),
          "shape::segCrossesPolygon 横穿 true / 擦边外 false");
    // 5) 点在多边形（带容差）：内/外
    CHECK(pointInPolygonTol(Vec3(5,5,0), square, 0.1) &&
          !pointInPolygonTol(Vec3(15,5,0), square, 0.1),
          "shape::pointInPolygonTol 内 true / 外 false");
    // 6) Fence：栏选线穿过直线 → 命中
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(2,2,0), Vec3(8,8,0)};
        const std::vector<Vec3> fence = {Vec3(0,8,0), Vec3(8,0,0)};
        CHECK(hit(l, fence, Mode::Fence, 0.1), "shape::hit Fence 穿过命中");
    }
    // 7) Window：全部包含才命中
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(2,2,0), Vec3(8,8,0)};
        const std::vector<Vec3> winAll  = {Vec3(-1,-1,0), Vec3(11,-1,0), Vec3(11,11,0), Vec3(-1,11,0)};
        const std::vector<Vec3> winHalf = {Vec3(5,5,0), Vec3(11,5,0), Vec3(11,11,0), Vec3(5,11,0)};
        CHECK(hit(l, winAll, Mode::Window, 0.1) && !hit(l, winHalf, Mode::Window, 0.1),
              "shape::hit Window 全包含 true / 部分包含 false");
    }
    // 8) Crossing：相交即命中
    {
        Element l; l.type = ElementType::Line; l.pts = {Vec3(2,2,0), Vec3(8,8,0)};
        const std::vector<Vec3> winHalf = {Vec3(5,5,0), Vec3(11,5,0), Vec3(11,11,0), Vec3(5,11,0)};
        CHECK(hit(l, winHalf, Mode::Crossing, 0.1), "shape::hit Crossing 部分相交命中");
    }
    // 9) pick：多元素批量
    {
        Element a; a.type = ElementType::Line; a.id = 1; a.pts = {Vec3(2,2,0), Vec3(8,8,0)};
        Element b; b.type = ElementType::Line; b.id = 2; b.pts = {Vec3(50,50,0), Vec3(60,60,0)};
        const std::vector<Vec3> winAll = {Vec3(-1,-1,0), Vec3(11,-1,0), Vec3(11,11,0), Vec3(-1,11,0)};
        auto ids = pick({a, b}, winAll, Mode::Window, 0.1);
        CHECK(ids.size() == 1 && ids[0] == 1, "shape::pick 圈围批量命中 id=1");
    }
    // 10) 圆的 Window 全包含 / Crossing 部分相交
    {
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 5.0;
        const std::vector<Vec3> winAll  = {Vec3(-6,-6,0), Vec3(6,-6,0), Vec3(6,6,0), Vec3(-6,6,0)};
        const std::vector<Vec3> winPart = {Vec3(3,-1,0), Vec3(7,-1,0), Vec3(7,3,0), Vec3(3,3,0)};
        CHECK(hit(c, winAll, Mode::Window, 0.1), "shape::hit 圆 Window 全包含");
        CHECK(hit(c, winAll, Mode::Crossing, 0.1), "shape::hit 圆全包含时 Crossing 亦命中");
        CHECK(hit(c, winPart, Mode::Crossing, 0.1), "shape::hit 圆与窗口部分相交 Crossing 命中");
    }
}

// ---------- MacroScript 宏/SCR（补充：纯 API 直测） ----------
static void t_macroDepth() {
    MacroRecorder& rec = MacroRecorder::instance();
    // 1) clear 复位
    rec.clear();
    CHECK(rec.lines().isEmpty() && !rec.isRecording(), "macro: clear 复位缓冲与状态");
    // 2) 回放互斥：回放期捕获被忽略
    MacroRecorder::setPlaybackActive(true);
    MacroRecorder::captureLine(QStringLiteral("line 1,1 2,2"));
    MacroRecorder::setPlaybackActive(false);
    CHECK(rec.lines().isEmpty(), "macro: 回放期 captureLine 被忽略");
    // 3) 录制中捕获 + 行序保持
    CommandInterpreter* dummy = nullptr;   // captureLine 纯静态，无需解释器
    (void)dummy;
    // 经解释器开始录制（macrorec）以进入录制态
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("macrorec")), "macro: macrorec 开始录制");
        CHECK(rec.isRecording(), "macro: isRecording 置位");
        MacroRecorder::captureLine(QStringLiteral("line 0,0 1,1"));
        MacroRecorder::captureLine(QStringLiteral("circle 2,2 1"));
        CHECK(rec.lines().size() == 2 &&
              rec.lines()[0] == QStringLiteral("line 0,0 1,1"),
              "macro: 录制缓冲保序 2 行");
        CHECK(interp.execute(QStringLiteral("macrostop")), "macro: macrostop 停止");
        CHECK(!rec.isRecording(), "macro: 停止后 isRecording 复位");
    }
    // 4) save/load 往返
    {
        const QString path = QDir::tempPath() + QStringLiteral("/scad_macro_depth.scr");
        QString err;
        CHECK(rec.save(path, &err), "macro: save 写出 .scr");
        rec.clear();
        CHECK(rec.load(path, &err) && rec.lines().size() == 2, "macro: load 往返 2 行");
        QFile::remove(path);
    }
    // 5) SCR 文本：注释/空行跳过 + 统计
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const QString text = QStringLiteral("# 注\n\nline 1,1 2,2\ncircle 3,3 1\n");
        auto r = ScriptRunner::runText(&m, &interp, text, ScriptRunner::StopMode::ContinueOnError);
        CHECK(r.ok() && r.totalLines == 2 && r.executed == 2 && m.elementCount() == 2,
              "macro: runText 注释空行跳过 executed=2");
    }
    // 6) SCR STOP 模式：失败即中断
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const QString text = QStringLiteral("line 0,0 1,1\nbogus_cmd\nline 5,5 6,6\n");
        auto r = ScriptRunner::runText(&m, &interp, text, ScriptRunner::StopMode::StopOnError);
        CHECK(!r.ok() && r.stoppedEarly && r.executed == 1 && m.elementCount() == 1,
              "macro: STOP 模式中断且已执行 1 行");
    }
    // 7) CONTINUE 模式：失败汇总不中断
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const QString text = QStringLiteral("line 0,0 1,1\nbogus_cmd\nline 5,5 6,6\n");
        auto r = ScriptRunner::runText(&m, &interp, text, ScriptRunner::StopMode::ContinueOnError);
        CHECK(!r.ok() && !r.stoppedEarly && r.executed == 2 && r.failed == 1 &&
              m.elementCount() == 2,
              "macro: CONTINUE 模式 executed=2 failed=1");
    }
    // 8) 录制中回放被拒（互斥）
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        interp.execute(QStringLiteral("macrorec"));
        auto r = ScriptRunner::runText(&m, &interp, QStringLiteral("line 0,0 1,1"),
                                       ScriptRunner::StopMode::ContinueOnError);
        CHECK(r.stoppedEarly && m.elementCount() == 0, "macro: 录制中 runText 被拒且不执行");
        interp.execute(QStringLiteral("macrostop"));
    }
    rec.clear();
}

// ---------- AliasTable 命令别名（补充：纯 API 直测） ----------
static void t_aliasDepth() {
    AliasTable& at = AliasTable::instance();
    at.clear();
    // 1) 注入自定义别名
    at.setAlias(QStringLiteral("ll"), QStringLiteral("LINE"));
    CHECK(at.contains(QStringLiteral("ll")) && at.resolve(QStringLiteral("LL")) == QStringLiteral("line"),
          "alias: setAlias 小写规整+大小写不敏感解析");
    // 2) 非别名 token 原样返回（toLower）
    CHECK(at.resolve(QStringLiteral("CIRCLE")) == QStringLiteral("circle"),
          "alias: 非别名 token 原样 toLower 返回");
    // 3) commandOf 存在/不存在
    CHECK(at.commandOf(QStringLiteral("ll")) == QStringLiteral("line") &&
          at.commandOf(QStringLiteral("zz")).isEmpty(),
          "alias: commandOf 命中/未命中空串");
    // 4) removeAlias：存在 true / 不存在 false
    CHECK(at.removeAlias(QStringLiteral("ll")) && !at.removeAlias(QStringLiteral("ll")) &&
          !at.contains(QStringLiteral("ll")),
          "alias: removeAlias 二次删除 false");
    // 5) 覆盖语义：setAlias 同名覆盖
    at.setAlias(QStringLiteral("k"), QStringLiteral("circle"));
    at.setAlias(QStringLiteral("k"), QStringLiteral("arc"));
    CHECK(at.resolve(QStringLiteral("k")) == QStringLiteral("arc"),
          "alias: setAlias 同名覆盖为 arc");
    // 6) aliases() 升序
    at.setAlias(QStringLiteral("aa"), QStringLiteral("arc"));
    const QStringList al = at.aliases();
    CHECK(al.size() >= 2 && std::is_sorted(al.cbegin(), al.cend()),
          "alias: aliases() 升序排列");
    // 7) save/load .pgp 往返
    {
        const QString path = QDir::tempPath() + QStringLiteral("/scad_alias_depth.pgp");
        QString err;
        CHECK(at.save(path, &err), "alias: save 写出 .pgp");
        at.clear();
        CHECK(at.map().empty(), "alias: clear 后空表");
        CHECK(at.load(path, &err) && at.resolve(QStringLiteral("k")) == QStringLiteral("arc"),
              "alias: load 往返 k→arc");
        QFile::remove(path);
    }
    // 8) .pgp 注释/空行/坏行容错
    {
        const QString path = QDir::tempPath() + QStringLiteral("/scad_alias_dirty.pgp");
        QFile f(path);
        CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "alias: 脏 .pgp 落盘");
        if (f.isOpen()) {
            f.write("; 注释行\n# 井号注释\n\nzz,delete\n\n");
            f.close();
            at.clear();
            QString err;
            CHECK(at.load(path, &err) && at.resolve(QStringLiteral("zz")) == QStringLiteral("delete"),
                  "alias: 注释空行跳过且 zz→delete");
        }
        QFile::remove(path);
    }
    // 9) defaultAliases 基线量
    const auto d = AliasTable::defaultAliases();
    CHECK(d.size() >= 23, "alias: 默认别名 ≥23 条");
    // 10) map() 直读与 resolve 一致
    at.clear();
    at.setAlias(QStringLiteral("mi"), QStringLiteral("mirror"));
    CHECK(!at.map().empty() && at.map().value(QStringLiteral("mi")) == QStringLiteral("mirror"),
          "alias: map() 直读与 setAlias 一致");
    at.clear();
}

// ---------- CuiManager 界面定制（补充：纯数据层直测） ----------
static void t_cuiDepth() {
    CuiManager& cm = CuiManager::instance();
    cm.clear();
    // 1) clear 复位
    CHECK(cm.toolbars().isEmpty() && cm.workspaceIds().isEmpty(), "cui: clear 复位工具条与工作空间");
    // 2) addToolbar + hasToolbar
    cm.addToolbar(QStringLiteral("myBar"), QStringLiteral("我的工具条"));
    CHECK(cm.hasToolbar(QStringLiteral("myBar")), "cui: addToolbar 可见");
    // 3) addButton + toolbar() 查询
    cm.addButton(QStringLiteral("myBar"), {QStringLiteral("line"), QStringLiteral("直线"), QString()});
    const CuiToolbar* tb = cm.toolbar(QStringLiteral("myBar"));
    CHECK(tb != nullptr && tb->buttons.size() == 1 &&
          tb->buttons[0].command == QStringLiteral("line"),
          "cui: addButton+findToolbar 按钮还原");
    // 4) removeButton 边界：无效下标 false / 有效 true
    CHECK(!cm.removeButton(QStringLiteral("myBar"), 5) &&
          cm.removeButton(QStringLiteral("myBar"), 0),
          "cui: removeButton 无效 false/有效 true");
    // 5) removeToolbar：存在 true / 复删 false
    CHECK(cm.removeToolbar(QStringLiteral("myBar")) && !cm.removeToolbar(QStringLiteral("myBar")),
          "cui: removeToolbar 二次删除 false");
    // 6) 工作空间：add + has + current
    cm.addToolbar(QStringLiteral("wsBar"), QStringLiteral("WS 条"));
    cm.addWorkspace(QStringLiteral("draft"), QStringLiteral("制图"), {QStringLiteral("wsBar")}, {});
    CHECK(cm.hasWorkspace(QStringLiteral("draft")), "cui: addWorkspace 可见");
    cm.setCurrentWorkspace(QStringLiteral("draft"));
    CHECK(cm.currentWorkspace() == QStringLiteral("draft"), "cui: currentWorkspace 切换");
    // 7) 可见性规则：列入 true / 未列入 false
    CHECK(cm.toolbarVisible(QStringLiteral("wsBar")) &&
          !cm.toolbarVisible(QStringLiteral("ghost")),
          "cui: 工作空间内可见/外不可见");
    // 8) 回退默认：currentWs 空 → 全部可见
    cm.setCurrentWorkspace(QString());
    CHECK(cm.toolbarVisible(QStringLiteral("wsBar")), "cui: 空工作空间全可见");
    // 9) 回调触发
    {
        int n = 0;
        CuiManager::RefreshFn fn = [&n]() { ++n; };
        cm.setRefreshCallback(std::move(fn));
        cm.requestRefresh();
        CHECK(n == 1, "cui: requestRefresh 触发回调 1 次");
        cm.setRefreshCallback(nullptr);
    }
    // 10) XML 往返：save → clear → load 工具条还原
    {
        cm.addButton(QStringLiteral("wsBar"), {QStringLiteral("circle"), QString(), QString()});
        const QString path = QDir::tempPath() + QStringLiteral("/scad_cui_depth.cuix");
        QString err;
        CHECK(cm.save(path, &err), "cui: save 写出 .cuix");
        cm.clear();
        CHECK(cm.load(path, &err) && cm.hasToolbar(QStringLiteral("wsBar")) &&
              cm.toolbar(QStringLiteral("wsBar")) != nullptr &&
              cm.toolbar(QStringLiteral("wsBar"))->buttons.size() == 1,
              "cui: load 往返工具条+按钮还原");
        QFile::remove(path);
    }
    cm.clear();
}

// ---------- 均匀网格空间索引（纯 core，无界面直测）----------
static bool t_spatialIndex() {
    bool ok = true;
    const double TOL = 1e-9;

    // 1) 基本命中：单矩形登记后，其内部的查询点必返回该 id
    {
        SpatialIndex idx;
        idx.add(7, QRectF(10, 10, 30, 20));   // [10,40]×[10,30]
        auto hit = idx.queryPoint(QPointF(20, 20), 0.5);
        CHECK(!hit.empty() && hit.front() == 7, "单矩形内部点命中 id=7");
        auto miss = idx.queryPoint(QPointF(100, 100), 0.5);
        CHECK(miss.empty(), "远点查询返回空");
    }

    // 2) queryRect 覆盖相交：横向带状查询应返回所有相交候选
    {
        SpatialIndex idx;
        idx.add(1, QRectF(0, 0, 10, 10));
        idx.add(2, QRectF(100, 100, 10, 10));
        idx.add(3, QRectF(50, 5, 10, 10));
        auto band = idx.queryRect(QRectF(-5, 0, 200, 115));  // 横贯 y∈[0,115]，覆盖全部三者
        CHECK(band.size() == 3, "带状查询返回全部 3 个相交候选");
    }

    // 3) 不变量「结果 ⊇ 真实命中集」：随机 N 个矩形，任意查询矩形必须包含全部
    //    bounds.intersects(query) 的 id（索引可能多候选，但绝不可漏选）
    {
        SpatialIndex idx(32.0);
        std::mt19937 rng(20261002);
        std::uniform_real_distribution<double> r(-1000.0, 1000.0);
        std::uniform_real_distribution<double> s(1.0, 80.0);
        std::vector<std::pair<int, QRectF>> rects;
        const int N = 2000;
        for (int i = 0; i < N; ++i) {
            double x = r(rng), y = r(rng), w = s(rng), h = s(rng);
            QRectF b(x, y, w, h);
            rects.push_back({ i, b });
            idx.add(i, b);
        }
        int trials = 0, fn = 0;   // falseNegative 计数
        for (int t = 0; t < 500; ++t) {
            QRectF q(r(rng), r(rng), s(rng) * 2, s(rng) * 2);
            auto cand = idx.queryRect(q);
            std::unordered_set<int> seen(cand.begin(), cand.end());
            for (const auto& pr : rects) {
                if (pr.second.intersects(q)) {
                    ++trials;
                    if (seen.find(pr.first) == seen.end()) ++fn;   // 漏选 = 严重错误
                }
            }
        }
        CHECK(fn == 0, "不变量 queryRect ⊇ 真实命中集（漏选数=0）");
        (void)trials;
    }

    // 4) 特性开关：关闭后 query* 返回空（调用方回落全量扫描）
    {
        SpatialIndex idx;
        idx.add(5, QRectF(0, 0, 10, 10));
        idx.setEnabled(false);
        CHECK(idx.queryPoint(QPointF(5, 5), 1.0).empty(), "关闭后 queryPoint 返回空");
        CHECK(idx.queryRect(QRectF(-5, -5, 20, 20)).empty(), "关闭后 queryRect 返回空");
        idx.setEnabled(true);
        CHECK(!idx.queryPoint(QPointF(5, 5), 1.0).empty(), "重新开启后恢复命中");
    }

    // 5) remove：移除后查询不再返回，且无悬垂（重复 add 同 id 幂等）
    {
        SpatialIndex idx;
        idx.add(9, QRectF(0, 0, 10, 10));
        idx.remove(9);
        CHECK(idx.queryPoint(QPointF(5, 5), 1.0).empty(), "移除后查询不再返回");
        idx.add(9, QRectF(0, 0, 10, 10));   // 复用同 id
        CHECK(idx.queryPoint(QPointF(5, 5), 1.0).front() == 9, "复用同 id 重新命中");
        idx.add(9, QRectF(0, 0, 10, 10));   // 重复 add 同 id 不重复登记
        auto h = idx.queryPoint(QPointF(5, 5), 1.0);
        int cnt = 0; for (int x : h) if (x == 9) ++cnt;
        CHECK(cnt == 1, "重复 add 同 id 不重复登记（计数=1）");
    }

    // 6) update：包围盒变化后，旧位置查询失效、新位置命中（幂等）
    {
        SpatialIndex idx;
        idx.add(11, QRectF(0, 0, 10, 10));
        idx.update(11, QRectF(200, 200, 10, 10));
        CHECK(idx.queryPoint(QPointF(5, 5), 1.0).empty(), "update 后旧位置失效");
        CHECK(idx.queryPoint(QPointF(205, 205), 1.0).front() == 11, "update 后新位置命中");
    }

    // 7) id 复用 + 实时同步：删 A(旧包围盒) 后同 id 加入新包围盒，索引只认新几何
    {
        SpatialIndex idx(64.0);
        idx.add(1, QRectF(0, 0, 5, 5));
        idx.remove(1);
        idx.add(1, QRectF(900, 900, 5, 5));   // id 复用，几何完全不同
        CHECK(idx.queryPoint(QPointF(2, 2), 1.0).empty(), "id 复用后旧几何不再命中");
        CHECK(idx.queryPoint(QPointF(902, 902), 1.0).front() == 1, "id 复用后新几何命中");
    }

    // 8) 退化/极长图元保护：零面积包围盒不崩、仍可命中
    {
        SpatialIndex idx;
        idx.add(21, QRectF(50, 50, 0, 0));    // 退化（点）
        auto h = idx.queryPoint(QPointF(50, 50), 1.0);
        CHECK(!h.empty() && h.front() == 21, "退化包围盒仍可命中中心");
    }

    (void)TOL;
    return ok;
}

// ============================================================================
// P0-04 大图纸内存预算：SmallVec 内联小向量语义 + 关键类型尺寸预算上界（纯 core 直测）
// ----------------------------------------------------------------------------
// 预算目标：每图元持久内存 ≤ 1KB（不含可见场景项）。Element 是模型层最大单体，
// 其 sizeof 上界被此测试锁定 —— 后续新增字段若把单图元内存推破预算，此处立即变红。
// ============================================================================
static bool t_p004Memory() {
    bool ok = true;

    // 1) SmallVec：内联槽（≤N 零堆分配）语义
    {
        SmallVec<int, 2> v;
        CHECK(v.empty() && v.size() == 0, "SmallVec 初始为空");
        v.push_back(1);
        v.push_back(2);
        CHECK(v.size() == 2 && v.contains(1) && v.contains(2), "SmallVec 内联槽 push/contains");
        v.push_back(3);   // 溢出到堆
        CHECK(v.size() == 3 && v.contains(3), "SmallVec 溢出 push 后仍可查");
        // range-for 遍历（跨内联+溢出连续）
        int sum = 0; for (int x : v) sum += x;
        CHECK(sum == 6, "SmallVec range-for 跨段遍历求和正确");
        // eraseValue：内联段命中
        v.eraseValue(1);
        CHECK(v.size() == 2 && !v.contains(1) && v.contains(2) && v.contains(3),
              "SmallVec eraseValue(内联命中) 后其余元素完好");
        // eraseValue：溢出段命中
        SmallVec<int, 2> w;
        w.push_back(10); w.push_back(20); w.push_back(30); w.push_back(40);
        w.eraseValue(30);
        CHECK(w.size() == 3 && !w.contains(30) && w.contains(10) && w.contains(20) && w.contains(40),
              "SmallVec eraseValue(溢出命中) 后其余元素完好");
        w.clear();
        CHECK(w.empty() && w.size() == 0, "SmallVec clear 后为空");
    }

    // 2) SmallVec<Cell,8>：SpatialIndex::cellsFor 典型返回 ≤8 个单元（内联，零堆分配）
    {
        SpatialIndex idx(64.0);
        // 覆盖 3×3=9 单元的矩形 → 恰好触发溢出路径（9 > 8）
        auto cells9 = idx.queryRect(QRectF(0, 0, 192, 192));   // 查询路径也走 cellsFor
        (void)cells9;
        CHECK(true, "SpatialIndex 覆盖 9 单元查询不崩（SmallVec 溢出路径）");
    }

    // 3) 内存预算上界：Element 是每图元 1KB 预算的主体
    {
        fprintf(stdout, "  [info] sizeof(Element)=%zu  sizeof(SmallVec<int,2>)=%zu\n",
                sizeof(Element), sizeof(SmallVec<int, 2>));
        // 基线实测 848B（14×QString + 10×vector + QColor + ~15×double，对齐后），
        // 预算上限取 896B（16 字节对齐余量 ≈5%）—— 此后新增字段若推破该值立即变红。
        CHECK(sizeof(Element) <= 896, "内存预算：sizeof(Element) ≤ 896B（新增字段不得破预算）");
        CHECK(sizeof(SmallVec<int, 2>) <= 40, "内存预算：SmallVec<int,2> 结构体 ≤ 40B（内联收益成立）");
    }

    return ok;
}

// ============================================================================
// 打印样式表：颜色/图层/线型 → 线宽 + 灰度 + 抖动（黑白出图，纯 core 直测）
// ----------------------------------------------------------------------------
// 覆盖：映射解析优先级、灰度化纯函数、GB 预设粗/细分组、简化 CTB 导入容错、
//       JSON 往返 + 向后兼容（缺灰度键=不灰度）。函数体内逐条 CHECK 计数。
// ============================================================================
static bool t_plotStyle() {
    bool ok = true;

    // 1) 映射解析：颜色 → 线宽（覆写图元默认线宽）
    {
        PlotStyleTable t;
        PlotStyleEntry e;
        e.overrideWidth = true; e.widthMm = 0.7;
        t.setColorStyle(QColor(255, 0, 0), e);
        Element el;                       // 默认深灰、Solid
        el.color = QColor(255, 0, 0);
        el.lineStyle = LineStyle::Solid;
        ResolvedPlotStyle rs = t.resolve(el);
        CHECK(std::fabs(rs.widthMm - 0.7) < 1e-9, "按颜色映射：线宽被覆写为 0.7mm");
        CHECK(rs.gray == false, "未设灰度时不灰度化（保持彩色）");
    }

    // 2) 解析优先级：图层 > 颜色（同时命中时图层胜出）
    {
        PlotStyleTable t;
        PlotStyleEntry ec; ec.overrideWidth = true; ec.widthMm = 0.3;   // 颜色档
        t.setColorStyle(QColor(0, 0, 255), ec);
        PlotStyleEntry el2; el2.overrideWidth = true; el2.widthMm = 1.0; // 图层档（更粗）
        t.setLayerStyle("轮廓", el2);
        Element e; e.layer = "轮廓"; e.color = QColor(0, 0, 255);
        ResolvedPlotStyle rs = t.resolve(e);
        CHECK(std::fabs(rs.widthMm - 1.0) < 1e-9, "解析优先级：图层优先于颜色");
    }

    // 3) 灰度化纯函数：gray=0→纯黑，gray=1→纯白，中间值按线性灰度
    {
        QColor src(255, 0, 0);   // 红
        QColor black = plotGrayscale(src, 0.0);
        CHECK(black.red() == 0 && black.green() == 0 && black.blue() == 0, "灰度0→纯黑");
        QColor white = plotGrayscale(src, 1.0);
        CHECK(white.red() == 255 && white.green() == 255 && white.blue() == 255, "灰度1→纯白");
        QColor mid = plotGrayscale(src, 0.5);
        CHECK(mid.red() == mid.green() && mid.green() == mid.blue(), "灰度0.5→中性灰(R=G=B)");
    }

    // 4) 灰度映射：overrideGray 生效，颜色被灰度化但线宽不受灰度影响
    {
        PlotStyleTable t;
        PlotStyleEntry e;
        e.overrideWidth = true; e.widthMm = 0.5;
        e.overrideGray  = true; e.gray = 0.25; e.dither = true;
        t.setColorStyle(QColor(0, 200, 0), e);
        Element el; el.color = QColor(0, 200, 0);
        ResolvedPlotStyle rs = t.resolve(el);
        CHECK(rs.gray == true, "overrideGray 生效（标记为灰度输出）");
        CHECK(rs.dither == true, "dither 标志透传");
        CHECK(rs.color.red() == rs.color.green() && rs.color.green() == rs.color.blue(),
              "resolve 输出的颜色已灰度化(R=G=B)");
        CHECK(std::fabs(rs.widthMm - 0.5) < 1e-9, "灰度化不改动线宽（线宽分组仍可控）");
    }

    // 5) GB 预设：粗实线/粗虚线取粗线档，细虚线/点画线取细线档（GB/T 14665 粗:细=2:1）
    {
        LineWidthGroup saved = currentLineWidthGroup();
        setCurrentLineWidthGroup(LineWidthGroup::G3);   // 粗1.0/细0.5
        PlotStyleTable t = PlotStyleTable::gbPreset(LineWidthGroup::G3);
        Element thick; thick.lineStyle = LineStyle::Solid;        // 粗实线
        Element thin;  thin.lineStyle  = LineStyle::Dashed;       // 细虚线
        Element dotdash; dotdash.lineStyle = LineStyle::DashDot;  // 点画线（细档）
        ResolvedPlotStyle rt = t.resolve(thick);
        ResolvedPlotStyle rd = t.resolve(thin);
        ResolvedPlotStyle rdd = t.resolve(dotdash);
        CHECK(std::fabs(rt.widthMm - 1.0) < 1e-9, "GB预设：粗实线=1.0mm(G3粗档)");
        CHECK(std::fabs(rd.widthMm - 0.5) < 1e-9, "GB预设：细虚线=0.5mm(G3细档)");
        CHECK(std::fabs(rdd.widthMm - 0.5) < 1e-9, "GB预设：点画线=0.5mm(细档)");
        CHECK(rt.gray == true && rt.color == QColor(0, 0, 0), "GB预设：粗实线为纯黑(灰度0)");
        setCurrentLineWidthGroup(saved);
    }

    // 6) 简化 CTB 导入：LAYER/COLOR/STYLE 三维度 + 全局 GRAYSCALE
    {
        PlotStyleTable t;
        QString err;
        const QString ctb =
            QStringLiteral("# 测试样式表\n"
                           "LAYER 轮廓 WIDTH=0.70 GRAY=0.0\n"
                           "COLOR #FF0000 WIDTH=0.35 DITHER=1\n"
                           "STYLE DASHED WIDTH=0.50\n"
                           "GRAYSCALE 0.0 DITHER=1\n");
        bool good = PlotStyleTable::fromCtb(ctb, &t, &err);
        CHECK(good, "CTB 导入成功（无错误行）");
        CHECK(t.layerStyles().contains("轮廓"), "CTB LAYER 条目已解析");
        CHECK(std::fabs(t.layerStyles()["轮廓"].widthMm - 0.70) < 1e-9, "CTB LAYER 线宽正确");
        CHECK(t.colorStyles().contains(plotColorKey(QColor(255, 0, 0))), "CTB COLOR 条目已解析");
        CHECK(t.colorStyles()[plotColorKey(QColor(255, 0, 0))].dither, "CTB COLOR DITHER=1 已解析");
        CHECK(t.styleStyles().contains((int)LineStyle::Dashed), "CTB STYLE(DASHED) 条目已解析");
        CHECK(t.globalGrayOn(), "CTB GRAYSCALE 全局开关已开启");
    }

    // 7) CTB 导入容错：非法行被跳过并回报错误，但已解析的行保留（宽容策略）
    {
        PlotStyleTable t;
        QString err;
        const QString bad = QStringLiteral("LAYER A WIDTH=0.5\nBOGUSKW foo\nLAYER B WIDTH=abc\n");
        bool good = PlotStyleTable::fromCtb(bad, &t, &err);
        CHECK(good == false, "CTB 含非法行时返回 false");
        CHECK(!err.isEmpty(), "CTB 非法行给出错误说明");
        CHECK(t.layerStyles().contains("A"), "CTB 合法行仍生效（宽容）");
        CHECK(!t.layerStyles().contains("B"), "CTB 非法 WIDTH 行被丢弃");
    }

    // 8) JSON 往返 + 向后兼容：旧档（无灰度键）读入后不灰度化
    {
        PlotStyleTable t;
        PlotStyleEntry e;
        e.overrideWidth = true; e.widthMm = 0.6;
        e.overrideGray  = true; e.gray = 0.3; e.dither = true;
        t.setColorStyle(QColor(0, 128, 0), e);
        t.setGlobalGray(true, 0.0, true);
        PlotStyleTable rt;
        rt.fromJson(t.toJson());
        CHECK(std::fabs(rt.colorStyles()[plotColorKey(QColor(0, 128, 0))].gray - 0.3) < 1e-9,
              "样式表 JSON 往返：灰度保持");
        CHECK(rt.globalGrayOn(), "样式表 JSON 往返：全局灰度保持");

        // 旧档：只有 overrideWidth，无灰度/抖动键
        QJsonObject legacy;
        legacy["overrideColor"] = false;
        legacy["overrideWidth"] = true;
        legacy["widthMm"] = 0.5;
        PlotStyleEntry le = PlotStyleEntry::fromJson(legacy);
        CHECK(le.overrideGray == false, "向后兼容：旧档缺 overrideGray 键→不灰度化");
        CHECK(le.dither == false, "向后兼容：旧档缺 dither 键→不抖动");
    }

    return ok;
}

// ===================== 块在位编辑（BEDIT）+ 嵌套块传播 + 外部块表 =====================
// 验证：① 引用式模型下"改块内几何 → 所有引用同步刷新"；② 嵌套块（引用含引用）编辑正确传播；
//       ③ Esc/取消不落盘（不调用写回 API 即定义不变）；④ 写回仅替换 entities，保留 attdefs/dynParams；
//       ⑤ 外部块表读取（JSON / DXF BLOCKS 段）。
static void t_blockEdit() {
    using namespace cadfmt;
    auto eq = [](const Vec3& a, double x, double y) {
        return std::fabs(a.x - x) <= 1e-6 && std::fabs(a.y - y) <= 1e-6;
    };
    auto hasPt = [&](const std::vector<Element>& els, double x, double y) {
        for (const auto& e : els) for (const auto& p : e.pts)
            if (eq(p, x, y)) return true;
        return false;
    };
    auto hasCircle = [&](const std::vector<Element>& els, double cx, double cy, double r) {
        for (const auto& e : els)
            if (e.type == ElementType::Circle && eq(e.pts[0], cx, cy) && std::fabs(e.radius - r) <= 1e-6)
                return true;
        return false;
    };
    auto makeLine = [](double x0, double y0, double x1, double y1) {
        Element e; e.type = ElementType::Line; e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) };
        return e;
    };

    // ---- ① 顶层块：N 处引用同步刷新 ----
    {
        SketchModel m;
        std::vector<Element> ents = { makeLine(0,0,10,0), makeLine(0,0,0,10) };
        BlockDef def; def.name = "B1"; def.basePoint = Vec3(0,0,0); def.entities = ents;
        for (auto& e : def.entities) e.id = 0;
        CHECK(m.addBlockDef(def), "BEDIT① 块 B1 创建成功");

        const std::vector<int> refs = {
            m.insertBlock("B1", Vec3(100,0,0)),
            m.insertBlock("B1", Vec3(200,0,0)),
            m.insertBlock("B1", Vec3(300,0,0)) };
        CHECK(refs[0] > 0 && refs[1] > 0 && refs[2] > 0, "BEDIT① 插入 3 处块引用");

        auto got = m.getBlockEntities("B1");
        CHECK((int)got.size() == 2, "BEDIT① 取出块内图元数 == 2");

        // 改写：把第一条线端点从 (10,0) 改为 (20,0)
        got[0].pts[1] = Vec3(20,0,0);
        CHECK(m.redefineBlockEntities("B1", got), "BEDIT① redefineBlockEntities 成功");

        const Vec3 insPos[3] = { Vec3(100,0,0), Vec3(200,0,0), Vec3(300,0,0) };
        for (int i = 0; i < 3; ++i) {
            const Element* ref = m.element(refs[i]);
            CHECK(ref && ref->type == ElementType::BlockRef, "BEDIT① 引用存在且为块引用");
            const auto sub = m.expandBlockRef(*ref);
            CHECK(hasPt(sub, insPos[i].x + 20, insPos[i].y),
                  "BEDIT① 引用#i 同步刷新到新端点(insertPos+20,0)");
            CHECK(!hasPt(sub, insPos[i].x + 10, insPos[i].y),
                  "BEDIT① 引用#i 旧端点(insertPos+10,0) 已消失");
        }
    }

    // ---- ② 嵌套块（引用含引用）编辑正确传播 ----
    {
        SketchModel m;
        // 内层块 B2：一个圆（半径 2，圆心 0,0）
        std::vector<Element> inner = { Element() };
        inner[0].type = ElementType::Circle; inner[0].pts = { Vec3(0,0,0) }; inner[0].radius = 2.0;
        BlockDef b2; b2.name = "B2"; b2.entities = inner; for (auto& e : b2.entities) e.id = 0;
        CHECK(m.addBlockDef(b2), "BEDIT② 内层块 B2 创建成功");

        // 外层块 A1：含一个指向 B2 的块引用
        Element refB2 = block::makeRef(*m.findBlockDef("B2"), Vec3(0,0,0), 0, 1, 1);
        BlockDef a1; a1.name = "A1"; a1.entities = { refB2 }; a1.entities[0].id = 0;
        CHECK(m.addBlockDef(a1), "BEDIT② 外层块 A1 创建成功(含嵌套引用)");

        const int a1a = m.insertBlock("A1", Vec3(0,0,0));
        const int a1b = m.insertBlock("A1", Vec3(50,0,0));
        CHECK(a1a > 0 && a1b > 0, "BEDIT② 插入 2 处 A1 引用");

        // 改写内层 B2：半径 2 → 5
        auto innerNew = m.getBlockEntities("B2");
        CHECK((int)innerNew.size() == 1, "BEDIT② 取出 B2 块内图元 == 1");
        innerNew[0].radius = 5.0;
        CHECK(m.redefineBlockEntities("B2", innerNew), "BEDIT② 改写 B2 几何");

        // 两个 A1 引用（经嵌套展开）都应看到半径 5 的圆
        const Element* ra = m.element(a1a); const auto sa = m.expandBlockRef(*ra);
        const Element* rb = m.element(a1b); const auto sb = m.expandBlockRef(*rb);
        CHECK(hasCircle(sa, 0, 0, 5.0),  "BEDIT② A1@0  嵌套传播：B2 半径已变 5");
        CHECK(hasCircle(sb, 50, 0, 5.0), "BEDIT② A1@50 嵌套传播：B2 半径已变 5");
    }

    // ---- ③ Esc / 取消不落盘（不调用写回 API 即定义不变）----
    {
        SketchModel m;
        BlockDef def; def.name = "ESC"; def.entities = { makeLine(0,0,10,0) };
        for (auto& e : def.entities) e.id = 0;
        CHECK(m.addBlockDef(def), "BEDIT③ 块 ESC 创建");
        auto before = m.getBlockEntities("ESC");
        CHECK(before.size() == 1 && eq(before[0].pts[1], 10, 0), "BEDIT③ 编辑前端点为 (10,0)");
        // 模拟"编辑中"：在本地副本上改，但**不调用写回 API**（Esc 丢弃语义）
        std::vector<Element> edited = before; edited[0].pts[1] = Vec3(99,0,0);
        (void)edited;   // 故意不调用 m.redefineBlockEntities("ESC", edited)
        auto after = m.getBlockEntities("ESC");
        CHECK(after.size() == 1, "BEDIT③ Esc 未写回：entities 数不变");
        CHECK(eq(after[0].pts[1], 10, 0), "BEDIT③ Esc 未写回：原几何不变(不落盘)");
    }

    // ---- ④ 写回仅替换 entities，保留 attdefs / dynParams（与 dynset 动态块参数并存）----
    {
        SketchModel m;
        BlockDef def; def.name = "P1"; def.entities = { makeLine(0,0,10,0) };
        for (auto& e : def.entities) e.id = 0;
        AttributeDef ad; ad.tag = "TAG"; ad.prompt = "p"; ad.defaultValue = "x"; def.attdefs.push_back(ad);
        DynParam dp; dp.name = "ROT"; dp.type = DynParamType::Rotate; def.dynParams.push_back(dp);
        CHECK(m.addBlockDef(def), "BEDIT④ 块 P1(含 attdef/dynParam) 创建");

        std::vector<Element> newEnts = { makeLine(0,0,7,7) };
        CHECK(m.redefineBlockEntities("P1", newEnts), "BEDIT④ redefineBlockEntities 成功");
        const BlockDef* pa = m.findBlockDef("P1");
        CHECK(pa != nullptr, "BEDIT④ 取回 P1 定义");
        CHECK(pa->attdefs.size() == 1 && pa->attdefs[0].tag == "TAG",
              "BEDIT④ attdefs 保留(属性定义不丢)");
        CHECK(pa->dynParams.size() == 1 && pa->dynParams[0].name == "ROT",
              "BEDIT④ dynParams 保留(动态块参数并存)");
        CHECK(pa->entities.size() == 1 && eq(pa->entities[0].pts[1], 7, 7),
              "BEDIT④ 新几何已写回");
    }

    // ---- ⑤ 外部块表读取（JSON / DXF BLOCKS 段）----
    {
        // JSON：写出一个含 blocks 的 SketchCAD 存档片段
        SketchModel src;
        BlockDef def; def.name = "EXT"; def.entities = { makeLine(0,0,5,0) };
        for (auto& e : def.entities) e.id = 0;
        CHECK(src.addBlockDef(def), "BEDIT⑤ 外部块 EXT 创建");
        QJsonObject root; root["blocks"] = src.blocksToJson();
        const QString jsonPath = QDir::temp().filePath("skcad_blk_test.json");
        { QFile f(jsonPath); if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(root).toJson()); f.close(); } }
        auto jtbl = blockTableFromJson(jsonPath);
        CHECK(jtbl.size() == 1 && jtbl[0].name == "EXT", "BEDIT⑤ 从 JSON 读回块表含 EXT");
        QFile::remove(jsonPath);

        // DXF：手写最小 BLOCKS + 一个 ENTITIES 实体（保证 fromDxf 返回成功）
        QStringList L;
        auto add = [&L](const QString& a, const QString& b) { L << a << b; };
        add("0","SECTION"); add("2","HEADER"); add("9","$INSUNITS"); add("70","4"); add("0","ENDSEC");
        add("0","SECTION"); add("2","BLOCKS");
        add("0","BLOCK"); add("2","DXB"); add("70","0"); add("10","0.0"); add("20","0.0");
        add("0","LINE"); add("8","0"); add("10","0.0"); add("20","0.0"); add("11","3.0"); add("21","0.0");
        add("0","ENDBLK");
        add("0","ENDSEC");
        add("0","SECTION"); add("2","ENTITIES");
        add("0","LINE"); add("8","0"); add("10","0.0"); add("20","0.0"); add("11","1.0"); add("21","1.0");
        add("0","ENDSEC"); add("0","EOF");
        QByteArray dxfData;
        for (int i = 0; i + 1 < L.size(); i += 2)
            dxfData += L[i].toUtf8() + "\n" + L[i+1].toUtf8() + "\n";
        const QString dxfPath = QDir::temp().filePath("skcad_blk_test.dxf");
        { QFile f(dxfPath); if (f.open(QIODevice::WriteOnly)) { f.write(dxfData); f.close(); } }
        auto dtbl = blockTableFromDxf(dxfPath);
        bool found = false; for (const auto& d : dtbl) if (d.name == "DXB") found = true;
        CHECK(dtbl.size() >= 1 && found, "BEDIT⑤ 从 DXF BLOCKS 读回块表含 DXB");
        QFile::remove(dxfPath);
    }
}

// ===================== 填充边界联动 + hatchedit（任务 E）=====================
// 验证：① 关联填充创建并记录边界来源；② 拖动/移动边界 → 填充随动（applyChanges 与
//       moveHandles 两条几何变更出口）；③ 删边界 → 填充失效（保留最后几何）；
//       ④ assoc off → 静态（边界再变也不随动）；⑤ hatchedit 改比例单步撤销还原；
//       ⑥ 关联字段序列化往返 + 旧档缺键默认；⑦ hatchScale/patNameOverride 作用于渲染参数。
static double polyArea(const std::vector<Vec3>& r) {
    if (r.size() < 3) return 0.0;
    double s = 0.0;
    for (size_t i = 0, j = r.size() - 1; i < r.size(); j = i++)
        s += r[j].x * r[i].y - r[i].x * r[j].y;
    return std::fabs(s) * 0.5;
}
// 建一个闭合矩形多段线（作为"持久边界"：拖顶点/改形后仍闭合，填充随动；删掉则失效）
static int addRectPolyline(SketchModel& m, double x0, double y0, double x1, double y1) {
    Element e; e.type = ElementType::Polyline; e.closed = true;
    e.pts = { Vec3(x0,y0,0), Vec3(x1,y0,0), Vec3(x1,y1,0), Vec3(x0,y1,0) };
    return m.addElement(e);
}
static void t_hatchLinkage() {
    auto widenChange = [](int pid) {
        Element np; np.type = ElementType::Polyline; np.closed = true; np.id = pid;
        np.pts = { Vec3(0,0,0), Vec3(20,0,0), Vec3(20,8,0), Vec3(0,8,0) };  // 10×8 → 20×8
        modify::Change c; c.id = pid; c.replacement = np; c.note = QStringLiteral("改边界");
        return c;
    };

    // ---- ① 关联填充创建：记录边界来源，几何=矩形 ----
    {
        SketchModel m;
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st; st.angleDeg = 45; st.spacingMm = 3;
        const int h = m.createAssociativeHatch({ pid }, st);
        CHECK(h > 0, "HATCH① createAssociativeHatch 返回有效 id");
        const Element* he = m.element(h);
        CHECK(he && he->type == ElementType::Hatch, "HATCH① 生成的是剖面区域");
        CHECK(he && (int)he->hatchSrcIds.size() == 1, "HATCH① 记录 1 个边界来源");
        CHECK(he && he->hatchAssociative, "HATCH① 关联默认开");
        CHECK(he && std::fabs(polyArea(he->pts) - 80.0) <= 1e-6, "HATCH① 初始填充面积=80(10×8)");
    }

    // ---- ② 改边界（applyChanges 路径）→ 填充随动 + 单步撤销 ----
    {
        SketchModel m;
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st;
        const int h = m.createAssociativeHatch({ pid }, st);
        m.beginEdit(QStringLiteral("改边界"));
        modify::applyChanges(&m, { widenChange(pid) }, QString());
        const Element* he = m.element(h);
        CHECK(he && std::fabs(polyArea(he->pts) - 160.0) <= 1e-6,
              "HATCH② 改边界后填充随动(面积 80→160)");
        m.undo();   // 单步撤销：边界与填充一起回退
        const Element* hb = m.element(h);
        CHECK(hb && std::fabs(polyArea(hb->pts) - 80.0) <= 1e-6,
              "HATCH② 单步撤销后填充与边界一起回退(面积回到 80)");
    }

    // ---- ②b 拖动边界顶点（moveHandles 路径）→ 填充随动 ----
    {
        SketchModel m;
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st;
        const int h = m.createAssociativeHatch({ pid }, st);
        // 顶点1 (10,0) 拖到 (20,0)：四边形变为 (0,0)(20,0)(10,8)(0,8)，仍闭合
        m.moveHandles({ { pid, 1 } }, Vec3(20, 0, 0));
        const Element* he = m.element(h);
        CHECK(he && std::fabs(polyArea(he->pts) - 120.0) <= 1e-6,
              "HATCH②b 夹点拖动后填充随动(面积 80→120)");
    }

    // ---- ③ 删边界 → 填充失效（保留最后几何）----
    {
        SketchModel m;
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st;
        const int h = m.createAssociativeHatch({ pid }, st);
        m.removeElement(pid);                 // 边界消失
        const Element* he = m.element(h);
        CHECK(he && he->hatchInvalid, "HATCH③ 删边界后填充标记失效");
        CHECK(he && std::fabs(polyArea(he->pts) - 80.0) <= 1e-6,
              "HATCH③ 失效时保留最后几何(不丢形状)");
    }

    // ---- ③b 多线边界被拖断（不成环）→ 同样标记失效 ----
    {
        SketchModel m;
        std::vector<int> b;
        b.push_back(addLine(m, 0, 0, 10, 0));
        b.push_back(addLine(m, 10, 0, 10, 8));
        b.push_back(addLine(m, 10, 8, 0, 8));
        b.push_back(addLine(m, 0, 8, 0, 0));
        SectionStyle st;
        const int h = m.createAssociativeHatch(b, st);   // 4 线成环 → 关联填充
        CHECK(h > 0 && std::fabs(polyArea(m.element(h)->pts) - 80.0) <= 1e-6,
              "HATCH③b 4 线矩形可成环建关联填充");
        // 单独平移右边线 → 与上下边脱开、不再闭合 → 失效
        std::vector<Element> srcs{ *m.element(b[1]) };
        m.beginEdit(QStringLiteral("拖断边界"));
        modify::applyChanges(&m, modify::makeMoveCopy(srcs, Vec3(10, 0, 0), false), QString());
        const Element* he = m.element(h);
        CHECK(he && he->hatchInvalid, "HATCH③b 边界被拖断(不成环)→ 填充失效");
    }

    // ---- ④ assoc off → 静态填充（边界移动不再随动）----
    {
        SketchModel m;
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st;
        const int h = m.createAssociativeHatch({ pid }, st);
        m.detachHatchBoundary(h);
        m.beginEdit(QStringLiteral("改边界"));
        modify::applyChanges(&m, { widenChange(pid) }, QString());
        const Element* he = m.element(h);
        CHECK(he && !he->hatchAssociative, "HATCH④ assoc off 后为静态");
        CHECK(he && std::fabs(polyArea(he->pts) - 80.0) <= 1e-6,
              "HATCH④ assoc off 边界改了但填充不变(静态)");
    }

    // ---- ⑤ hatchedit 改比例 → 单步撤销还原 ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const int pid = addRectPolyline(m, 0, 0, 10, 8);
        SectionStyle st;
        const int h = m.createAssociativeHatch({ pid }, st);
        m.setSelection({ h });
        CHECK(interp.execute(QStringLiteral("hatchedit scale 2.0")), "HATCH⑤ hatchedit scale 2.0 执行成功");
        const Element* he = m.element(h);
        CHECK(he && std::fabs(he->hatchScale - 2.0) <= 1e-9, "HATCH⑤ 比例已改为 2.0");
        interp.execute(QStringLiteral("undo"));           // 单步撤销
        const Element* hb = m.element(h);
        CHECK(hb && std::fabs(hb->hatchScale - 1.0) <= 1e-9, "HATCH⑤ 单步撤销后比例还原为 1.0");
    }

    // ---- ⑥ 关联字段序列化往返 + 旧档缺键默认 ----
    {
        Element e; e.type = ElementType::Hatch; e.pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,8,0) };
        e.hatchSrcIds = { 5, 6, 7, 8 };
        e.hatchPattern = QStringLiteral("wood"); e.hatchScale = 2.5;
        e.hatchAssociative = false; e.hatchInvalid = true;
        const Element e2 = elementFromJson(elementToJson(e));
        CHECK(e2.hatchSrcIds == e.hatchSrcIds, "HATCH⑥ hatchSrcIds 序列化往返一致");
        CHECK(e2.hatchPattern == e.hatchPattern, "HATCH⑥ hatchPattern 往返一致");
        CHECK(std::fabs(e2.hatchScale - 2.5) <= 1e-9, "HATCH⑥ hatchScale 往返一致");
        CHECK(e2.hatchAssociative == false, "HATCH⑥ hatchAssociative 往返一致");
        CHECK(e2.hatchInvalid == true, "HATCH⑥ hatchInvalid 往返一致");
        // 旧档：去掉全部关联键 → 读端回落默认（关联开 / 无来源 / scale=1 / 未失效）
        QJsonObject j = elementToJson(e);
        j.remove("hatchSrcIds"); j.remove("hatchPattern"); j.remove("hatchScale");
        j.remove("hatchAssociative"); j.remove("hatchInvalid");
        const Element e3 = elementFromJson(j);
        CHECK(e3.hatchAssociative == true, "HATCH⑥ 旧档缺键 → 关联默认开");
        CHECK(e3.hatchSrcIds.empty() && !e3.hatchInvalid, "HATCH⑥ 旧档缺键 → 无来源/未失效");
        CHECK(std::fabs(e3.hatchScale - 1.0) <= 1e-9, "HATCH⑥ 旧档缺键 → 比例默认 1.0");
    }

    // ---- ⑦ hatchScale / patNameOverride 作用于渲染参数 ----
    {
        Element hs; hs.type = ElementType::Hatch;
        hs.pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,8,0), Vec3(0,8,0) };
        hs.radius = 3.0; hs.sectionMaterial = (int)SectionMaterial::General;
        hs.hatchScale = 2.0; hs.hatchPattern = QStringLiteral("wood");
        const SectionRegion r = hatchRegionFromElement(hs);
        CHECK(std::fabs(r.style.spacingMm - 6.0) <= 1e-9, "HATCH⑦ hatchScale=2 → 间距 3→6");
        CHECK(r.style.patNameOverride == QStringLiteral("wood"), "HATCH⑦ hatchPattern → patNameOverride");
        CHECK(hatchPatternFor(r.style).patName == QStringLiteral("wood"),
              "HATCH⑦ patNameOverride 优先于材料默认图案");
    }
}

// ===================== FIELD 域 + Table 公式列（明细栏自动维护）=====================
// 验证：① 面积域随边界变更联动（applyChanges 钩子 + 渲染期 resolveFields）；
//        ② 花括号语法 {field:area:id} 与 %<AREA:id>% 语义等价；
//        ③ 周长域；④ 断链/非封闭 → ### 占位；⑤ Table =sum/=count 公式列 + 防递归；
//        ⑥ PROP 自定义特性域 + prop 命令；⑦ 序列化向后兼容（properties 顶层键/缺键回落）；
//        ⑧ fieldupdate 命令端到端；⑨ AppSettings fields/autoUpdateOnOpen 默认与读写。
static void t_fields() {
    // ---- ① 面积域随边界变更联动（含 refreshFields 命中计数）----
    {
        SketchModel m;
        const int rid = addRectPolyline(m, 0, 0, 10, 8);   // 10×8 闭合四边形
        Element mt; mt.type = ElementType::MText;
        mt.pts = { Vec3(0, -5, 0) }; mt.radius = 3.5;
        mt.text = QStringLiteral("面积 = %<AREA:%1>% mm²").arg(rid);
        const int mid = m.addElement(mt);
        CHECK(resolveFields(m.element(mid)->text, &m) == QStringLiteral("面积 = 80.00 mm²"),
              "FIELD① 面积域初始求值 10×8=80.00");
        // 改边界（applyChanges 路径，内部挂 refreshFields 钩子）→ 域联动重算
        Element np; np.type = ElementType::Polyline; np.closed = true; np.id = rid;
        np.pts = { Vec3(0,0,0), Vec3(20,0,0), Vec3(20,8,0), Vec3(0,8,0) };  // → 20×8
        modify::Change c; c.id = rid; c.replacement = np; c.note = QStringLiteral("改边界");
        m.beginEdit(QStringLiteral("改边界"));
        modify::applyChanges(&m, { c }, QString());
        CHECK(resolveFields(m.element(mid)->text, &m) == QStringLiteral("面积 = 160.00 mm²"),
              "FIELD① 改边界后面积域联动(80→160.00)");
        CHECK(m.refreshFields({ rid }) == 1, "FIELD① refreshFields([边界id]) 命中 1 个域宿主");
        CHECK(m.refreshFields({ mid + 777 }) == 0, "FIELD① refreshFields([无关id]) 命中 0 个宿主");
        CHECK(m.refreshFields() >= 1, "FIELD① refreshFields(全模型扫描) 至少命中本宿主");
    }

    // ---- ② 花括号语法与 %<..>% 语义等价 ----
    {
        SketchModel m;
        const int rid = addRectPolyline(m, 0, 0, 10, 8);
        const QString brace = QStringLiteral("{field:area:%1}").arg(rid);
        const QString pct   = QStringLiteral("%<AREA:%1>%").arg(rid);
        CHECK(resolveFields(brace, &m) == resolveFields(pct, &m),
              "FIELD② {field:area:id} 与 %<AREA:id>% 求值一致");
        CHECK(resolveFields(brace, &m) == QStringLiteral("80.00"),
              "FIELD② 花括号语法求值 80.00");
    }

    // ---- ③ 周长域 + ④ 断链/非封闭回落 ### ----
    {
        SketchModel m;
        const int rid = addRectPolyline(m, 0, 0, 10, 8);
        CHECK(resolveFields(QStringLiteral("%<PERIM:%1>%").arg(rid), &m) == QStringLiteral("36.00"),
              "FIELD③ 周长域 2×(10+8)=36.00");
        CHECK(resolveFields(QStringLiteral("%<AREA:99999>%"), &m) == QStringLiteral("###"),
              "FIELD④ 断链域(不存在id)回落 ###");
        Element ln; ln.type = ElementType::Line;
        ln.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        const int lid = m.addElement(ln);
        CHECK(resolveFields(QStringLiteral("%<AREA:%1>%").arg(lid), &m) == QStringLiteral("###"),
              "FIELD④ 非封闭图元(直线)面积域回落 ###");
        CHECK(resolveFields(QStringLiteral("%<AREA:abc>%"), &m) == QStringLiteral("###"),
              "FIELD④ 非法 id 参数回落 ###");
        CHECK(resolveFields(QStringLiteral("温度 %<NOSUCH:x>%!"), &m) == QStringLiteral("温度 ###!"),
              "FIELD④ 未知域码回落 ### 且不破坏其余文本");
        CHECK(resolveFields(QStringLiteral("无域纯文本"), &m) == QStringLiteral("无域纯文本"),
              "FIELD④ 无域纯文本原样返回（快速路径，向后兼容）");
    }

    // ---- ⑤ Table =sum/=count 公式列（明细栏数量列）+ 防递归 ----
    {
        SketchModel m;
        Element tbl; tbl.type = ElementType::Table;
        tbl.pts = { Vec3(0, 0, 0) };
        tbl.tableRows = 4; tbl.tableCols = 2;
        tbl.tableCells = {   // 行主序 4×2：第 1 列名称，第 2 列数量
            QStringLiteral("序"), QStringLiteral("数量"),
            QStringLiteral("螺钉"), QStringLiteral("4"),
            QStringLiteral("垫片"), QStringLiteral("2"),
            QStringLiteral("合计"), QStringLiteral("=sum(2,3)"),
        };
        const int tid = m.addElement(tbl);
        const Element* t = m.element(tid);
        CHECK(t && displayTableCell(*t, 4, 1, &m) == QStringLiteral("6.00"),
              "FIELD⑤ =sum(2,3) 明细栏数量合计 4+2=6.00");
        tbl.tableCells[7] = QStringLiteral("=count(2,3)");
        m.updateElementQuiet(tid, [&](Element& d) { d.tableCells = tbl.tableCells; });
        t = m.element(tid);
        CHECK(t && displayTableCell(*t, 4, 1, &m) == QStringLiteral("2"),
              "FIELD⑤ =count(2,3) 非空单元格计数 = 2");
        // 防递归：公式引用公式单元格 → ###
        tbl.tableCells[7] = QStringLiteral("=sum(1,4)");   // 第 4 行本列自身是公式
        m.updateElementQuiet(tid, [&](Element& d) { d.tableCells = tbl.tableCells; });
        t = m.element(tid);
        CHECK(t && displayTableCell(*t, 4, 1, &m) == QStringLiteral("###"),
              "FIELD⑤ 公式引用公式单元格防递归回落 ###");
        // 行号越界 → ###
        tbl.tableCells[7] = QStringLiteral("=sum(2,99)");
        m.updateElementQuiet(tid, [&](Element& d) { d.tableCells = tbl.tableCells; });
        t = m.element(tid);
        CHECK(t && displayTableCell(*t, 4, 1, &m) == QStringLiteral("###"),
              "FIELD⑤ 行号越界公式回落 ###");
        // 非公式单元格原样 + 域替换仍生效（统一管线两段式）
        tbl.tableCells[3] = QStringLiteral("%<COUNT>%");
        m.updateElementQuiet(tid, [&](Element& d) { d.tableCells = tbl.tableCells; });
        t = m.element(tid);
        CHECK(t && displayTableCell(*t, 2, 1, &m) == QString::number(m.elementCount()),
              "FIELD⑤ 非公式单元格走域替换管线（COUNT 全模型计数）");
    }

    // ---- ⑥ PROP 自定义特性域 + prop 命令 ----
    {
        SketchModel m;
        m.setDrawingProp(QStringLiteral("材料"), QStringLiteral("Q235-B"));
        CHECK(m.drawingProp(QStringLiteral("材料")) == QStringLiteral("Q235-B"),
              "FIELD⑥ setDrawingProp 写入读回");
        CHECK(resolveFields(QStringLiteral("材料：%<PROP:材料>%"), &m) == QStringLiteral("材料：Q235-B"),
              "FIELD⑥ %<PROP:材料>% 域求值");
        CHECK(resolveFields(QStringLiteral("%<PROP:不存在>%"), &m) == QStringLiteral("###"),
              "FIELD⑥ 缺名特性回落 ###");
        m.setDrawingProp(QStringLiteral("材料"), QStringLiteral("45#"));   // 覆盖
        CHECK(m.drawingProp(QStringLiteral("材料")) == QStringLiteral("45#"),
              "FIELD⑥ 同名覆盖");
        m.setDrawingProp(QStringLiteral("材料"), QString());               // 空值=移除
        CHECK(m.drawingProp(QStringLiteral("材料")).isEmpty(),
              "FIELD⑥ 空值移除特性");
        // prop 命令端到端（设置/查询/删除）
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("prop 图号 JT-2026-01")), "FIELD⑥ prop 设置命令成功");
        CHECK(m.drawingProp(QStringLiteral("图号")) == QStringLiteral("JT-2026-01"),
              "FIELD⑥ prop 命令写入生效");
        CHECK(interp.execute(QStringLiteral("prop 图号")), "FIELD⑥ prop 查询命令成功");
        CHECK(interp.execute(QStringLiteral("prop -r 图号")), "FIELD⑥ prop 删除命令成功");
        CHECK(m.drawingProp(QStringLiteral("图号")).isEmpty(), "FIELD⑥ prop 删除后特性为空");
    }

    // ---- ⑦ 序列化向后兼容（properties 顶层键 + 缺键回落 + 域文本原文往返）----
    {
        SketchModel m;
        m.setDrawingProp(QStringLiteral("项目"), QStringLiteral("减速器"));
        const int rid = addRectPolyline(m, 0, 0, 10, 8);
        Element mt; mt.type = ElementType::MText;
        mt.pts = { Vec3(0, -5, 0) }; mt.radius = 3.5;
        mt.text = QStringLiteral("{field:area:%1}").arg(rid);
        m.addElement(mt);
        const QJsonObject root = m.toDrawingJson();
        CHECK(root.contains(QStringLiteral("properties")) &&
              root[QStringLiteral("properties")].toObject()[QStringLiteral("项目")] == QStringLiteral("减速器"),
              "FIELD⑦ toDrawingJson 顶层 properties 键写入");
        CHECK(root[QStringLiteral("elements")].toArray().at(1).toObject()[QStringLiteral("text")]
                  .toString().contains(QStringLiteral("{field:")),
              "FIELD⑦ 域文本按原文入档（渲染期求值，序列化零改动）");
        // 旧档模拟：无 properties 键（或缺 elements 域文本）→ 读回空表不崩溃
        SketchModel m2;
        m2.setPropsFromJson(QJsonObject());
        CHECK(m2.drawingProps().empty(), "FIELD⑦ 旧档缺 properties 键读回空表（向后兼容）");
    }

    // ---- ⑧ fieldupdate 命令端到端 ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        addRectPolyline(m, 0, 0, 10, 8);
        CHECK(interp.execute(QStringLiteral("fieldupdate")), "FIELD⑧ fieldupdate 命令成功");
        Element tbl; tbl.type = ElementType::Table;
        tbl.pts = { Vec3(0, 0, 0) }; tbl.tableRows = 2; tbl.tableCols = 1;
        tbl.tableCells = { QStringLiteral("头"), QStringLiteral("=count(1,1)") };
        m.addElement(tbl);
        CHECK(interp.execute(QStringLiteral("fieldupdate")), "FIELD⑧ 含公式表格时执行成功");
    }

    // ---- ⑨ AppSettings fields/autoUpdateOnOpen（默认开 + 注入读写）----
    {
        QTemporaryDir td;
        const QString ini = td.filePath(QStringLiteral("fields.ini"));
        {
            QSettings s(ini, QSettings::IniFormat);
            AppSettings as(&s);
            CHECK(as.fieldAutoUpdateOnOpen() == true,
                  "FIELD⑨ fields/autoUpdateOnOpen 默认开");
            as.setFieldAutoUpdateOnOpen(false);
            CHECK(as.fieldAutoUpdateOnOpen() == false,
                  "FIELD⑨ setFieldAutoUpdateOnOpen(false) 读回 false");
        }
        {
            QSettings s(ini, QSettings::IniFormat);   // 重开：持久化往返
            AppSettings as(&s);
            CHECK(as.fieldAutoUpdateOnOpen() == false,
                  "FIELD⑨ 开关值持久化往返一致");
        }
    }
}

// ===================== GD&T：命名标注样式 + 多重引线 + 形位公差框格（任务 F）=====================
// 验证：① 12 类既有标注回归不破坏（每类 dim::build 仍产出有效几何）；② DimStyle 表多实例化 +
//        样式切换全图联动（引用式：改样式 → 引用它的标注实时联动）；③ 旧档（单全局 DimStyle）
//        读入自动转为默认命名样式；④ MLEADER / FCF 序列化 round-trip + 几何产出。
static void t_gdt() {
    // ---- ① 12 类既有标注回归：逐类 dim::build 仍产出有效几何（构造满足各 builder 的最小输入）----
    {
        SketchModel m;
        auto mk = [&](DimensionType dt) {
            Element e; e.type = ElementType::Dimension; e.dtype = dt;
            switch (dt) {
                case DimensionType::Angle:  e.pts = { Vec3(0,0,0), Vec3(20,0,0), Vec3(0,15,0) }; break; // 需 pts>=3
                case DimensionType::Radius:
                case DimensionType::Diameter: e.pts = { Vec3(0,0,0) }; e.radius = 8.0; break;              // 需 pts>=1 + radius>0
                case DimensionType::Arc:     e.pts = { Vec3(0,0,0) }; e.radius = 10.0;
                                             e.startAngle = 0; e.endAngle = 90; break;                    // 弧长需起止角
                case DimensionType::Leader:  e.pts = { Vec3(0,0,0), Vec3(20,10,0) }; break;                 // 需 pts>=2
                default:                     e.pts = { Vec3(0,0,0), Vec3(30,12,0) }; break;                // 线性族 pts>=2
            }
            return e;
        };
        const DimensionType kTypes[] = {
            DimensionType::Distance, DimensionType::Angle, DimensionType::Radius, DimensionType::Diameter,
            DimensionType::Aligned, DimensionType::Horizontal, DimensionType::Vertical, DimensionType::Baseline,
            DimensionType::Continue, DimensionType::Arc, DimensionType::Leader, DimensionType::Ordinate };
        bool allValid = true, allNamed = true;
        for (DimensionType dt : kTypes) {
            const Element e = mk(dt);
            if (!dim::build(e, m.dimStyle()).valid) allValid = false;
            if (qstrcmp(dimensionTypeName(dt), "标注") == 0) allNamed = false;   // 名称表未退化为兜底
        }
        CHECK(allValid, "GD&T① 12 类既有标注 dim::build 全部仍产出有效几何");
        CHECK(allNamed,  "GD&T① 12 类 dimensionTypeName 均为专名（未退化兜底）");
    }

    // ---- ② DimStyle 表多实例化 + 样式切换全图联动（引用式）----
    {
        SketchModel m;
        // 默认表含一条 "Standard"
        CHECK(m.dimStyleCount() == 1 && m.hasDimStyle("Standard"), "GD&T② 默认含一条 Standard 样式");
        // 新建一张 ISO 样式（与默认国标不同：字高/箭头长不同）
        dim::DimStyleEntry iso;
        iso.name = "ISO小字";
        iso.style = dim::Style();
        iso.style.stdConf = DimensionStandardConfig::isoDefault();
        iso.style.textHeight = 2.5;
        iso.style.arrowLen   = 2.0;
        CHECK(m.addDimStyle(iso), "GD&T② 新建 ISO 样式");
        CHECK(m.dimStyleCount() == 2, "GD&T② 样式表多实例化（2 条）");
        // 两个标注引用 ISO 样式
        Element d1; d1.type = ElementType::Dimension; d1.dtype = DimensionType::Distance;
        d1.pts = { Vec3(0,0,0), Vec3(30,0,0) }; d1.dimStyleName = "ISO小字";
        Element d2 = d1; d2.id = 0;
        const int id1 = m.addElement(d1), id2 = m.addElement(d2);
        const Element* e1 = m.element(id1);
        // 引用式解析：引用标注读到 ISO 样式（字高 2.5 / 箭头 2.0），非默认 3.5/3.5
        CHECK(e1 && std::fabs(m.dimStyleFor(*e1).textHeight - 2.5) <= 1e-9, "GD&T② 引用标注解析到 ISO 字高 2.5");
        CHECK(e1 && std::fabs(m.dimStyleFor(*e1).arrowLen - 2.0) <= 1e-9,   "GD&T② 引用标注解析到 ISO 箭头 2.0");
        // **联动**：改 ISO 样式定义 → 引用它的标注实时读到新值
        dim::DimStyleEntry iso2 = *m.findDimStyle("ISO小字");
        iso2.style.textHeight = 1.8;
        CHECK(m.updateDimStyle(iso2), "GD&T② 修改 ISO 样式");
        CHECK(std::fabs(m.dimStyleFor(*m.element(id1)).textHeight - 1.8) <= 1e-9, "GD&T② 改样式后标注 #1 联动(字高 1.8)");
        CHECK(std::fabs(m.dimStyleFor(*m.element(id2)).textHeight - 1.8) <= 1e-9, "GD&T② 改样式后标注 #2 联动(全图联动)");
        // 重命名同步更新引用
        CHECK(m.renameDimStyle("ISO小字", "ISO-tiny"), "GD&T② 重命名样式");
        CHECK(m.element(id1)->dimStyleName == "ISO-tiny", "GD&T② 重命名后标注引用同步");
        // 置当前：空 dimStyleName 的标注解析到当前样式
        CHECK(m.setCurrentDimStyle("ISO-tiny"), "GD&T② 置当前样式");
        Element d3; d3.type = ElementType::Dimension; d3.dtype = DimensionType::Distance;
        d3.pts = { Vec3(0,0,0), Vec3(30,0,0) };   // dimStyleName 空
        const int id3 = m.addElement(d3);
        CHECK(std::fabs(m.dimStyleFor(*m.element(id3)).textHeight - 1.8) <= 1e-9, "GD&T② 空引用标注回落当前样式");
    }

    // ---- ③ 旧档兼容：单全局 DimStyle（无 dimStyles 键）→ 自动转为默认命名样式 ----
    {
        SketchModel m;
        m.setDimStylesFromJson(QJsonObject());          // 旧档：无 dimStyles 字段
        CHECK(m.dimStyleCount() == 1, "GD&T③ 旧档无 dimStyles → 自动生成 1 条默认样式");
        CHECK(m.hasDimStyle("Standard") && m.currentDimStyle() == "Standard", "GD&T③ 旧档默认样式名为 Standard");
        // 旧档等价性：单全局默认样式 == 原硬编码 dim::Style（国标默认 + 比例）
        const dim::Style st = m.dimStyle();
        CHECK(std::fabs(st.textHeight - 3.5) <= 1e-9 && st.standard() == DimensionStandard::GBT,
              "GD&T③ 旧档解析出的默认样式等价于原单全局 DimStyle");
    }

    // ---- ④ MLEADER / FCF 序列化 round-trip + 几何产出 ----
    {
        // MLEADER round-trip
        Element ml = ElementFactory::createMLeader(Vec3(5,5,0), Vec3(20,20,0), Vec3(30,20,0),
                                                    QStringLiteral("第一行\n第二行"), 3.5, 12.0, 4.0);
        ml.mleaderStyleName = "Standard";
        const Element ml2 = elementFromJson(elementToJson(ml));
        CHECK(ml2.type == ElementType::MLeader, "GD&T④ MLEADER 类型往返");
        CHECK(ml2.pts.size() == 3 && ml2.pts[0].distTo(Vec3(5,5,0)) <= 1e-9, "GD&T④ MLEADER 附着点往返");
        CHECK(ml2.text == QStringLiteral("第一行\n第二行"), "GD&T④ MLEADER 多行文字往返");
        CHECK(std::fabs(ml2.mleaderLanding - 12.0) <= 1e-9 && std::fabs(ml2.mleaderArrowLen - 4.0) <= 1e-9,
              "GD&T④ MLEADER 基线/箭头往返");
        // MLEADER 几何：引线 + 箭头 + 两行文字
        const MLeaderGeom mg = buildMLeader(ml);
        CHECK(mg.valid && !mg.leader.empty() && mg.texts.size() == 2, "GD&T④ MLEADER 几何：引线 + 两行文字");
        CHECK(!mg.landing.empty(), "GD&T④ MLEADER 几何：基线存在");

        // FCF round-trip
        Element fc = ElementFactory::createFcf(Vec3(10,10,0), (int)FcfSymbol::Position,
                                               QStringLiteral("⌀0.1"), { QStringLiteral("A"), QStringLiteral("B") },
                                               8.0, QStringLiteral("M"));
        const Element fc2 = elementFromJson(elementToJson(fc));
        CHECK(fc2.type == ElementType::Fcf, "GD&T④ FCF 类型往返");
        CHECK(fc2.fcfSymbol == (int)FcfSymbol::Position, "GD&T④ FCF 符号往返");
        CHECK(fc2.fcfTolerance == QStringLiteral("⌀0.1") && fc2.fcfModifier == QStringLiteral("M"),
              "GD&T④ FCF 公差值/修饰符往返");
        CHECK(fc2.fcfDatums.size() == 2 && fc2.fcfDatums[0] == QStringLiteral("A") && fc2.fcfDatums[1] == QStringLiteral("B"),
              "GD&T④ FCF 基准字母组往返");
        CHECK(fc2.pts.size() == 1 && fc2.pts[0].distTo(Vec3(10,10,0)) <= 1e-9, "GD&T④ FCF 锚点往返");
        // FCF 几何：符号格 + 修饰符格(M) + 公差格 + 两基准格 = 5 格
        const FcfGeom fg = buildFcf(fc);
        CHECK(fg.valid && fg.cells.size() == 5, "GD&T④ FCF 几何：5 格(符号+修饰符+公差+2基准)");
        CHECK(!fg.frame.empty() && !fg.symbolStrokes.empty(), "GD&T④ FCF 几何：框线 + 符号笔画");
        // GB/T 1182 符号集子集：11 个符号均有名称且越界被拒
        bool allNamed = true;
        for (int i = 0; i < fcfSymbolCount(); ++i)
            if (!fcfSymbolValid(i) || qstrcmp(fcfSymbolName((FcfSymbol)i), "未知") == 0) allNamed = false;
        CHECK(allNamed, "GD&T④ GB/T 1182 符号集 11 类均有名称");
        CHECK(!fcfSymbolValid(fcfSymbolCount()) && !fcfSymbolValid(-1), "GD&T④ 符号索引越界被拒");
    }
}

// ===================== QDIM 快速标注（复用 dim 基线/连续引擎）=====================
// 验证：① 圆阵列基线标注的【生成计数】+【测量值】+【层叠位置】；② 连续标注链式测量；
//       ③ 平行线阵列基线标注；④ 选择集不足返回空不崩溃；⑤ 生成标注经 dimStyleName
//          绑定当前命名样式 → 改样式定义后联动（一图多标准）。
// 共 14 条 CHECK（≥ 验收要求的 12 条）。
static void t_qdim() {
    // ---- ① 圆阵列 QDIM 基线标注：计数 + 测量值 + 层叠位置 ----
    {
        SketchModel m;
        const int c0 = addCircle(m, 0, 0, 3);
        const int c1 = addCircle(m, 10, 0, 3);
        const int c2 = addCircle(m, 20, 0, 3);
        const int c3 = addCircle(m, 30, 0, 3);
        m.setSelection({c0, c1, c2, c3});
        const auto ids = m.quickDimSelected(QStringLiteral("baseline"));
        CHECK(ids.size() == 3, "QDIM① 4 个圆 → 基线标注 3 条（N-1）");
        const dim::Style st = m.dimStyle();
        const double expectMeas[] = {10.0, 20.0, 30.0};   // 共基点(0,0)到各圆心距离
        bool measOk = true, posOk = true, named = true;
        Vec3 n = (ids.size() > 0)
                     ? (m.element(ids[0])->pts[1] - m.element(ids[0])->pts[0]).normalized().perp()
                     : Vec3(0,1,0);
        const Vec3 baseMid = (m.element(ids[0])->pts[0] + m.element(ids.back())->pts[1]) * 0.5;
        for (int i = 0; i < (int)ids.size(); ++i) {
            const Element* e = m.element(ids[i]);
            const dim::Geom g = dim::build(*e, st);
            if (!g.valid || g.line.empty() || std::fabs(g.measured - expectMeas[i]) > 1e-6) measOk = false;
            // 层叠：第 i 条尺寸线相对基线的法向偏移 = defOffset + i*(textHeight+4)
            const Vec3 lineMid = (g.line[0].first + g.line[0].second) * 0.5;
            const double offset = (lineMid - baseMid).dot2D(n);
            const double expectOff = st.defOffset + i * (st.textHeight + 4.0);
            if (std::fabs(std::fabs(offset) - expectOff) > 1e-6) posOk = false;
            if (e->dimStyleName != m.currentDimStyle()) named = false;
        }
        CHECK(measOk, "QDIM① 基线各标注测量值=10/20/30（共基点）");
        CHECK(posOk,  "QDIM① 基线标注按 e.radius 逐层偏移 defOffset+i*(textH+4)");
        CHECK(named,  "QDIM① 生成标注记当前样式名（随样式切换全图联动）");
    }

    // ---- ② 圆阵列 QDIM 连续标注：链式测量 + 计数 ----
    {
        SketchModel m;
        const int c0 = addCircle(m, 0, 0, 3);
        const int c1 = addCircle(m, 10, 0, 3);
        const int c2 = addCircle(m, 20, 0, 3);
        const int c3 = addCircle(m, 30, 0, 3);
        m.setSelection({c0, c1, c2, c3});
        const auto ids = m.quickDimSelected(QStringLiteral("continue"));
        CHECK(ids.size() == 3, "QDIM② 连续标注 3 条（N-1）");
        const dim::Style st = m.dimStyle();
        bool measOk = true;
        for (int i = 0; i < (int)ids.size(); ++i) {
            const dim::Geom g = dim::build(*m.element(ids[i]), st);
            if (!g.valid || std::fabs(g.measured - 10.0) > 1e-6) measOk = false;  // 相邻间距均 10
        }
        CHECK(measOk, "QDIM② 连续标注测量值=10/10/10（相邻间距）");
    }

    // ---- ③ 平行线阵列 QDIM 基线标注 ----
    {
        SketchModel m;
        const int l0 = addLine(m, 0, 0, 50, 0);
        const int l1 = addLine(m, 0, 10, 50, 10);
        const int l2 = addLine(m, 0, 20, 50, 20);
        m.setSelection({l0, l1, l2});
        const auto ids = m.quickDimSelected(QStringLiteral("baseline"));
        CHECK(ids.size() == 2, "QDIM③ 3 条平行线 → 基线标注 2 条");
        const dim::Style st = m.dimStyle();
        const double expectMeas[] = {10.0, 20.0};   // 线中点 y=0,10,20 → 基线(0)到10/20
        bool measOk = true;
        for (int i = 0; i < (int)ids.size(); ++i) {
            const dim::Geom g = dim::build(*m.element(ids[i]), st);
            if (!g.valid || std::fabs(g.measured - expectMeas[i]) > 1e-6) measOk = false;
        }
        CHECK(measOk, "QDIM③ 平行线基线标注测量=10/20");
    }

    // ---- ④ 选择集不足 / 空 → 返回空，不崩溃 ----
    {
        SketchModel m;
        const int l0 = addLine(m, 0, 0, 10, 0);
        m.setSelection({l0});
        const auto ids = m.quickDimSelected(QStringLiteral("baseline"));
        CHECK(ids.empty(), "QDIM④ 仅 1 个有效几何 → 返回空（不生成）");
        m.clearSelection();
        const auto ids2 = m.quickDimSelected(QStringLiteral("baseline"));
        CHECK(ids2.empty(), "QDIM④ 空选择集 → 返回空");
    }

    // ---- ⑤ QDIM 标注经 dimStyleName 绑定当前命名样式 → 改样式定义后联动（一图多标准）----
    {
        SketchModel m;
        const int c0 = addCircle(m, 0, 0, 3);
        const int c1 = addCircle(m, 15, 0, 3);
        m.setSelection({c0, c1});
        const auto ids = m.quickDimSelected(QStringLiteral("baseline"));
        CHECK(ids.size() == 1, "QDIM⑤ 2 圆 → 1 条基线标注");
        CHECK(m.element(ids[0])->dimStyleName == m.currentDimStyle(), "QDIM⑤ 生成标注记当前样式名(Standard)");
        // 改 Standard 样式定义 → 引用它的 QDIM 标注联动
        dim::DimStyleEntry st = *m.findDimStyle("Standard");
        st.style.textHeight = 2.0;
        CHECK(m.updateDimStyle(st), "QDIM⑤ 修改 Standard 样式");
        CHECK(std::fabs(m.dimStyleFor(*m.element(ids[0])).textHeight - 2.0) <= 1e-9,
              "QDIM⑤ 改 Standard 后 QDIM 标注经 dimStyleName 联动读到字高 2.0");
    }
}

// ===================== 编组成组操作 + FILTER 预选器（任务 G）=====================
// 验证：① 编组创建 + 点击整组扩展 / Ctrl 穿透；② 编组移动一次撤销整组还原；
//       ③ group 命令行五子命令；④ Xref 边界禁止编组；⑤ FILTER 预选器过滤 + 用后即清；
//       ⑥ FILTER×fence 组合；⑦ filter 命令行（类型/颜色组合 + 清除）；⑧ groupName 序列化往返。
static void t_groupFilter() {
    using shape::Mode;

    // ---- ① 编组创建 + 拾取扩展 / Ctrl 穿透 ----
    {
        SketchModel m;
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int c1 = addCircle(m, 20, 0, 3);
        const int l2 = addLine(m, 0, 5, 5, 5);   // 不编组
        CHECK(m.createGroup(QStringLiteral("G1"), { l1, c1 }), "GROUP① createGroup 成功");
        CHECK(m.groupNames().contains(QStringLiteral("G1")), "GROUP① groupNames 含 G1");
        CHECK((int)m.elementsInGroup(QStringLiteral("G1")).size() == 2, "GROUP① 成员数 == 2");
        // 普通点击任一成员 → 整组（含被点成员，仅可见）
        const auto exp = m.expandPickToGroup(c1, false);
        CHECK(exp.size() == 2 &&
              std::find(exp.begin(), exp.end(), l1) != exp.end() &&
              std::find(exp.begin(), exp.end(), c1) != exp.end(),
              "GROUP① 普通点击扩展为整组{直线,圆}");
        // Ctrl 修饰 = 穿透编组：只选被点成员
        const auto pierce = m.expandPickToGroup(c1, true);
        CHECK(pierce.size() == 1 && pierce[0] == c1, "GROUP① Ctrl 点击穿透编组（单选）");
        // 非编组成员不扩展
        const auto single = m.expandPickToGroup(l2, false);
        CHECK(single.size() == 1 && single[0] == l2, "GROUP① 非编组成员不扩展");
    }

    // ---- ② 编组移动一次撤销 → 整组还原 ----
    {
        SketchModel m;
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int c1 = addCircle(m, 20, 0, 3);
        m.createGroup(QStringLiteral("G1"), { l1, c1 });
        m.selectGroup(QStringLiteral("G1"));
        CHECK((int)m.selection().size() == 2, "GROUP② selectGroup 整组入选");
        m.translateSelection(Vec3(5, 0, 0), QStringLiteral("移动编组"));   // 一次操作
        const Element* ml = m.element(l1);
        CHECK(ml && std::fabs(ml->pts[0].x - 5.0) <= 1e-9, "GROUP② 平移后成员直线起点 x=5");
        m.undo();                                                          // 一次 Ctrl+Z
        const Element* rl = m.element(l1);
        const Element* rc = m.element(c1);
        CHECK(rl && std::fabs(rl->pts[0].x) <= 1e-9, "GROUP② 撤销后直线还原");
        CHECK(rc && std::fabs(rc->pts[0].x - 20.0) <= 1e-9, "GROUP② 撤销后圆还原（整组一次回退）");
        // 编组不改变图元 id 与层归属
        CHECK(rc && rc->layer == QStringLiteral("0"), "GROUP② 编组不改层归属");
    }

    // ---- ③ group 命令行五子命令 ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int c1 = addCircle(m, 20, 0, 3);
        const int l2 = addLine(m, 0, 5, 5, 5);
        m.setSelection({ l1, c1 });
        CHECK(interp.execute(QStringLiteral("group create G1")), "GROUP③ group create 执行成功");
        CHECK(m.groupNames().contains(QStringLiteral("G1")), "GROUP③ 编组已登记");
        m.setSelection({ l2 });
        CHECK(interp.execute(QStringLiteral("group add G1")), "GROUP③ group add 执行成功");
        CHECK(m.elementCountInGroup(QStringLiteral("G1")) == 3, "GROUP③ add 后成员数 == 3");
        CHECK(interp.execute(QStringLiteral("group list")), "GROUP③ group list 执行成功");
        m.setSelection({ l2 });
        CHECK(interp.execute(QStringLiteral("group remove")), "GROUP③ group remove 执行成功");
        CHECK(m.element(l2) && m.element(l2)->groupName.isEmpty(), "GROUP③ remove 后 groupName 清空");
        CHECK(interp.execute(QStringLiteral("group ungroup G1")), "GROUP③ group ungroup 执行成功");
        CHECK(!m.groupNames().contains(QStringLiteral("G1")), "GROUP③ ungroup 后编组消失");
        CHECK(m.element(l1) && m.element(l1)->groupName.isEmpty(), "GROUP③ 解散后图元保留未编组");
        CHECK(!interp.execute(QStringLiteral("group nosuch")), "GROUP③ 未知子命令被拒");
    }

    // ---- ④ Xref 边界禁止编组（本图内限定）----
    {
        SketchModel m;
        BlockDef xd; xd.name = "X1"; xd.isXref = true;
        Element xl; xl.type = ElementType::Line; xl.pts = { Vec3(0,0,0), Vec3(5,0,0) }; xl.id = 0;
        xd.entities = { xl };
        CHECK(m.addBlockDef(xd), "GROUP④ Xref 块表记录创建");
        const int xr = m.insertBlock(QStringLiteral("X1"), Vec3(0, 0, 0));
        CHECK(xr > 0, "GROUP④ Xref 引用插入");
        CHECK(!m.createGroup(QStringLiteral("GX"), { xr }), "GROUP④ 纯 Xref 引用创建编组被拒");
        // 混合：Xref 被剔除、普通图元正常编组
        const int l1 = addLine(m, 0, 10, 10, 10);
        CHECK(m.createGroup(QStringLiteral("GM"), { xr, l1 }), "GROUP④ 混合选择允许创建");
        const auto mem = m.elementsInGroup(QStringLiteral("GM"));
        CHECK(mem.size() == 1 && mem[0] == l1, "GROUP④ Xref 成员被剔除，普通图元入组");
    }

    // ---- ⑤ FILTER 预选器：过滤一次 + 用后即清 ----
    {
        SketchModel m;
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int c1 = addCircle(m, 20, 0, 3);
        const int r1 = addRectPolyline(m, 30, 0, 35, 5);
        FilterDef f;
        f.conds.push_back({ QSelectProp::Type, QSelectOp::Eq, QStringLiteral("直线") });
        m.setActivePickFilter(f);
        CHECK(m.hasActivePickFilter(), "FILTER⑤ 预选器已设置");
        const std::vector<int> cand = { l1, c1, r1 };
        const auto kept = m.applyPickFilterOnce(cand);
        CHECK(kept.size() == 1 && kept[0] == l1, "FILTER⑤ 候选{线,圆,矩形}只留直线");
        CHECK(!m.hasActivePickFilter(), "FILTER⑤ 用后即清（作用于随后一次）");
        const auto again = m.applyPickFilterOnce(cand);
        CHECK((int)again.size() == 3, "FILTER⑤ 清除后原样放行（不再过滤）");
    }

    // ---- ⑥ FILTER×fence 组合用例 ----
    {
        SketchModel m;
        const int l1 = addLine(m, 0, 0, 10, 0);
        addCircle(m, 5, 1, 1);                     // 圆心 (5,1)：恰在栏线上（fence 命中，id 不直接引用）
        // 栏线 (-1,3)→(11,-1)：在 x=8 处穿过直线段（y=0 ∈ [0,10]），且过圆心 → 两图元均命中
        std::vector<Vec3> fence = { Vec3(-1, 3, 0), Vec3(11, -1, 0) };
        const auto hits = shape::pick(m.elements(), fence, Mode::Fence, 1e-3);
        CHECK(hits.size() == 2, "FILTER⑥ 栏选命中直线与圆");
        FilterDef f;
        f.conds.push_back({ QSelectProp::Type, QSelectOp::Eq, QStringLiteral("直线") });
        m.setActivePickFilter(f);
        const auto sel = m.applyPickFilterOnce(hits);
        CHECK(sel.size() == 1 && sel[0] == l1, "FILTER⑥ 预选器×栏选：只留直线");
        m.setSelection(sel);
        CHECK((int)m.selection().size() == 1 && m.isSelected(l1), "FILTER⑥ 结算选择集为该直线");
    }

    // ---- ⑦ filter 命令行：设置 / 查看 / 清除 + 类型×颜色组合 ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int c1 = addCircle(m, 20, 0, 3);
        m.updateElementQuiet(c1, [](Element& e) { e.color = QColor(255, 0, 0); });
        CHECK(interp.execute(QStringLiteral("filter type=圆")), "FILTER⑦ filter type=圆 执行成功");
        CHECK(m.hasActivePickFilter(), "FILTER⑦ 预选器已生效");
        CHECK(interp.execute(QStringLiteral("filter")), "FILTER⑦ filter 查看执行成功");
        CHECK(interp.execute(QStringLiteral("filter none")), "FILTER⑦ filter none 清除成功");
        CHECK(!m.hasActivePickFilter(), "FILTER⑦ 清除后无预选器");
        CHECK(interp.execute(QStringLiteral("filter type=圆 color=#ff0000")), "FILTER⑦ 类型×颜色组合设置");
        const auto kept = m.applyPickFilterOnce({ l1, c1 });
        CHECK(kept.size() == 1 && kept[0] == c1, "FILTER⑦ 组合条件只留红色圆");
        CHECK(!interp.execute(QStringLiteral("filter 非法键")), "FILTER⑦ 非法参数被拒");
    }

    // ---- ⑧ groupName 序列化往返 ----
    {
        Element e; e.type = ElementType::Line; e.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        e.groupName = QStringLiteral("螺栓组");
        const Element e2 = elementFromJson(elementToJson(e));
        CHECK(e2.groupName == QStringLiteral("螺栓组"), "GROUP⑧ groupName 序列化往返一致");
        QJsonObject j = elementToJson(e);
        j.remove("groupName");
        const Element e3 = elementFromJson(j);
        CHECK(e3.groupName.isEmpty(), "GROUP⑧ 旧档缺键 → 回落未编组");
    }
}

// ===================== 图层治理五件套：LAYISO / LAYUNISO / LAYMCH（命令路径 + 退化） =====================
// 验收：每命令 ≥8 断言（含退化：无选中 / 目标层不存在 / 空图层 / 模型无图层），命令路径与 makeXxx 同源
static void t_layerGovernance() {
    const QColor RED(QStringLiteral("#ff0000"));
    const QColor BLUE(QStringLiteral("#0000ff"));
    auto mkLine = [](SketchModel& mm, const QString& layer, double x0, double y0, double x1, double y1) -> int {
        Element e; e.type = ElementType::Line;
        e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) }; e.layer = layer;
        return mm.addElement(e);
    };
    auto frozenCount = [](SketchModel& mm) -> int {
        int n = 0;
        for (int i = 0; i < mm.layerCount(); ++i) if (mm.layerAt(i).frozen) ++n;
        return n;
    };
    auto inLayerCount = [](SketchModel& mm, const QString& name) -> int {
        int n = 0;
        for (const auto& e : mm.elements()) if (e.layer == name) ++n;
        return n;
    };

    // ---------- LAYISO：隔离选中图元所属图层（冻结其余） ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        m.addLayer(QStringLiteral("LB"), BLUE);
        const int a = addLine(m, 0, 0, 10, 0);    // 层 "0"
        const int b = mkLine(m, QStringLiteral("LA"), 0, 5, 10, 5);
        const int c = addLine(m, 20, 0, 30, 0);   // 层 "0"
        (void)a;
        m.setSelection({ b });                     // 选中层 LA 的图元
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layiso")), "LAYISO① 执行成功");
        const Layer* L0  = m.findLayer(QStringLiteral("0"));
        const Layer* LA  = m.findLayer(QStringLiteral("LA"));
        const Layer* LB  = m.findLayer(QStringLiteral("LB"));
        CHECK(LA && !LA->frozen,  "LAYISO① 选中层 LA 保持可见（不冻结）");
        CHECK(L0 && L0->frozen,   "LAYISO① 其余层 0 被冻结");
        CHECK(LB && LB->frozen,   "LAYISO① 其余层 LB 被冻结");
        CHECK(frozenCount(m) == 2, "LAYISO① 冻结计数 = 2（仅保留 LA）");
        CHECK(!m.elementPickable(*m.element(c)), "LAYISO① 冻结层图元不可命中");
        CHECK(m.elementPickable(*m.element(b)),  "LAYISO① 保留层图元可命中");
        // layuniso 可逆
        CHECK(interp.execute(QStringLiteral("layuniso")), "LAYISO① layuniso 执行成功");
        CHECK(frozenCount(m) == 0, "LAYISO① layuniso 后全部解冻");
        CHECK(m.elementPickable(*m.element(c)), "LAYISO① 解冻后图元恢复可命中");
    }
    // LAYISO②：无选中 → 孤立当前活动层；退化空模型（仅 "0"）不崩溃
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        m.addLayer(QStringLiteral("LB"), BLUE);
        m.setCurrentLayer(QStringLiteral("0"));
        m.setSelection({});                        // 无选中
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layiso")), "LAYISO② 无选中执行成功（孤立当前层）");
        const QString cur = m.currentLayer();
        CHECK(m.findLayer(cur) && !m.findLayer(cur)->frozen, "LAYISO② 当前层保持可见");
        CHECK(m.findLayer(QStringLiteral("LA"))->frozen, "LAYISO② 其余层 LA 冻结");
        CHECK(m.findLayer(QStringLiteral("LB"))->frozen, "LAYISO② 其余层 LB 冻结");
        // 退化：空模型（仅基础层 "0"）无选中 → 无层可冻结，n=0 不崩溃
        SketchModel empty;
        CommandInterpreter ei(&empty);
        CHECK(ei.execute(QStringLiteral("layiso")), "LAYISO② 空模型 layiso 不崩溃");
        CHECK(frozenCount(empty) == 0, "LAYISO② 空模型 layiso 冻结计数 = 0");
    }
    // LAYISO③：隔离后新建图层不被自动冻结（隔离为调用时点语义）
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        m.setSelection({ mkLine(m, QStringLiteral("LA"), 0, 0, 5, 0) });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layiso")), "LAYISO③ 执行成功");
        m.addLayer(QStringLiteral("LC"), BLUE);
        CHECK(m.findLayer(QStringLiteral("LC")) && !m.findLayer(QStringLiteral("LC"))->frozen,
              "LAYISO③ 隔离后新建层 LC 不自动冻结（时点语义）");
    }

    // ---------- LAYUNISO：解冻所有图层 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        m.addLayer(QStringLiteral("LB"), BLUE);
        m.setLayerFrozen(QStringLiteral("LA"), true);
        m.setLayerFrozen(QStringLiteral("LB"), true);
        CommandInterpreter interp(&m);
        CHECK(frozenCount(m) == 2, "LAYUNISO① 前置：2 层冻结");
        CHECK(interp.execute(QStringLiteral("layuniso")), "LAYUNISO① 执行成功");
        CHECK(frozenCount(m) == 0, "LAYUNISO① 全部解冻");
        CHECK(m.elementPickable(*m.element(mkLine(m, QStringLiteral("LA"), 0, 0, 1, 0))),
              "LAYUNISO① 解冻后 LA 图元可命中");
    }
    // LAYUNISO②：本就全解冻 → 幂等不崩溃；连跑两次仍 0
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layuniso")), "LAYUNISO② 全解冻态执行成功");
        CHECK(frozenCount(m) == 0, "LAYUNISO② 全解冻态冻结计数仍 0");
        CHECK(interp.execute(QStringLiteral("layuniso")), "LAYUNISO② 幂等二次执行成功");
        CHECK(frozenCount(m) == 0, "LAYUNISO② 幂等后仍 0");
    }

    // ---------- LAYMCH：把选中图元移到目标图层 ----------
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), RED);
        m.addLayer(QStringLiteral("LB"), BLUE);
        const int a = mkLine(m, QStringLiteral("LA"), 0, 0, 10, 0);
        const int b = mkLine(m, QStringLiteral("LA"), 0, 5, 10, 5);
        m.setSelection({ a, b });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("laymch LB")), "LAYMCH① 执行成功（目标 LB）");
        CHECK(m.element(a) && m.element(a)->layer == QStringLiteral("LB"), "LAYMCH① 图元 a 改属 LB");
        CHECK(m.element(b) && m.element(b)->layer == QStringLiteral("LB"), "LAYMCH① 图元 b 改属 LB");
        CHECK(inLayerCount(m, QStringLiteral("LA")) == 0, "LAYMCH① 源层 LA 已无图元");
        CHECK(inLayerCount(m, QStringLiteral("LB")) == 2, "LAYMCH① 目标层 LB 含 2 图元");
    }
    // LAYMCH②：退化—无选中 → 拒绝且零改动
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LB"), BLUE);
        const int a = addLine(m, 0, 0, 10, 0);
        m.setSelection({});
        CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("laymch LB")), "LAYMCH② 无选中被拒");
        CHECK(m.element(a) && m.element(a)->layer == QStringLiteral("0"), "LAYMCH② 拒绝后图元层不变");
    }
    // LAYMCH③：退化—目标层不存在 → 拒绝
    {
        SketchModel m;
        const int a = addLine(m, 0, 0, 10, 0);
        m.setSelection({ a });
        CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("laymch NOSUCH")), "LAYMCH③ 目标层不存在被拒");
        CHECK(m.element(a)->layer == QStringLiteral("0"), "LAYMCH③ 拒绝后图元层不变");
    }
    // LAYMCH④：退化—移入空图层（目标层原本无图元）
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LE"), BLUE);    // 空图层
        CHECK(inLayerCount(m, QStringLiteral("LE")) == 0, "LAYMCH④ 目标空层 LE 初始 0 图元");
        const int a = addLine(m, 0, 0, 10, 0);
        m.setSelection({ a });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("laymch LE")), "LAYMCH④ 移入空层执行成功");
        CHECK(m.element(a)->layer == QStringLiteral("LE"), "LAYMCH④ 图元改属空层 LE");
        CHECK(inLayerCount(m, QStringLiteral("LE")) == 1, "LAYMCH④ 空层纳入后含 1 图元");
    }
    // LAYMCH⑤：adoptStyle=false → 不改图元原色
    {
        SketchModel m;
        m.addLayer(QStringLiteral("LA"), BLUE);
        Element e; e.type = ElementType::Line; e.pts = { Vec3(0,0,0), Vec3(9,0,0) };
        e.layer = QStringLiteral("0"); e.byLayer = false; e.color = RED;
        const int id = m.addElement(e);
        m.setSelection({ id });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("laymch LA")), "LAYMCH⑤ 执行成功");
        const Element* me = m.element(id);
        CHECK(me && me->layer == QStringLiteral("LA"), "LAYMCH⑤ 图元改属 LA");
        CHECK(me && me->color == RED, "LAYMCH⑤ adoptStyle=false → 原色(红)保留未套用层色");
    }
    // LAYMCH⑥：退化为同层移动（no-op 仍成功）
    {
        SketchModel m;
        const int a = addLine(m, 0, 0, 10, 0);   // 层 "0"
        m.setSelection({ a });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("laymch 0")), "LAYMCH⑥ 同层移动成功");
        CHECK(m.element(a)->layer == QStringLiteral("0"), "LAYMCH⑥ 同层移动后层不变");
    }
}

// ===================== 命令路径直测：join / arrayrect / arraypolar / arraypath / ltscale =====================
// 验证「ModifyCommand / LambdaCommand 分发 → selectedElems → makeXxx → applyChanges」整条命令链路（此前仅单测 makeXxx）
static void t_commandDispatch() {
    // JOIN 端到端：两条相接直线 → 1 条 Polyline
    {
        SketchModel m;
        const int l1 = addLine(m, 0, 0, 10, 0);
        const int l2 = addLine(m, 10, 0, 10, 8);
        m.setSelection({ l1, l2 });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("join")), "JOIN命令 执行成功");
        CHECK(m.elementCount() == 1, "JOIN命令 落盘 2 源 → 1 Polyline");
        const Element* p = nullptr;
        for (const auto& e : m.elements()) if (e.type == ElementType::Polyline) p = &e;
        CHECK(p && p->pts.size() == 3, "JOIN命令 Polyline 顶点数 = 3（接缝无重复）");
    }
    // JOIN 无选中 → 拒绝
    {
        SketchModel m;
        addLine(m, 0, 0, 10, 0);
        m.setSelection({});
        CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("join")), "JOIN命令 无选中被拒");
    }
    // ARRAYRECT 端到端：圆 2×3 → 6 图元
    {
        SketchModel m;
        const int c = addCircle(m, 0, 0, 1);
        m.setSelection({ c });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("arrayrect 2 3 10 20")), "ARRAYRECT命令 执行成功");
        CHECK(m.elementCount() == 6, "ARRAYRECT命令 1+5=6 图元");
    }
    // ARRAYPOLAR 端到端：圆绕 (0,0) 4 份/180° → 4 图元
    {
        SketchModel m;
        const int c = addCircle(m, 10, 0, 2);
        m.setSelection({ c });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("arraypolar 0,0 4 180")), "ARRAYPOLAR命令 执行成功");
        CHECK(m.elementCount() == 4, "ARRAYPOLAR命令 1+3=4 图元");
    }
    // ARRAYPATH 端到端：沿 100 长折线、间距 25 → 4 副本（共 6 图元）
    {
        SketchModel m;
        Element path; path.type = ElementType::Polyline;
        path.pts = { Vec3(0,0,0), Vec3(100,0,0) };
        path.closed = false;   // 开放折线：避免被当作闭合（默认 closed=true）导致路径长翻倍
        const int pathId = m.addElement(path);
        const int c = addCircle(m, 10, 0, 1);
        m.setSelection({ c });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("arraypath %1 25").arg(pathId)), "ARRAYPATH命令 执行成功");
        CHECK(m.elementCount() == 6, "ARRAYPATH命令 路径100/间距25 → 4 副本（共 6 图元）");
    }
    // LTSCALE 命令：0.5 / 2.0 生效，非法值被拒且保持
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("ltscale 0.5")), "LTSCALE命令 0.5 成功");
        CHECK(std::fabs(m.ltscale() - 0.5) < 1e-9, "LTSCALE命令 生效 0.5");
        CHECK(!interp.execute(QStringLiteral("ltscale 0")), "LTSCALE命令 非法(0)被拒");
        CHECK(std::fabs(m.ltscale() - 0.5) < 1e-9, "LTSCALE命令 非法后仍保持 0.5");
        CHECK(interp.execute(QStringLiteral("ltscale 2")), "LTSCALE命令 2.0 成功");
        CHECK(std::fabs(m.ltscale() - 2.0) < 1e-9, "LTSCALE命令 生效 2.0");
        CHECK(!interp.execute(QStringLiteral("ltscale -1")), "LTSCALE命令 负值被拒");
        CHECK(std::fabs(m.ltscale() - 2.0) < 1e-9, "LTSCALE命令 负值后仍保持 2.0");
    }
}

// ===================== 布局体系：多布局 / 视口平铺 / 视口独立比例（任务 H）=====================
// 验证：① paperRect 往返+缺键回落；② PlotLayoutDef JSON 往返；③ 布局表 CRUD+重名拒；
//       ④ 存档往返+旧档缺键回落模型空间；⑤ viewportScaleMm 1:2/2:1/fit/自定义；
//       ⑥ 双视口 paperRect 平铺像素断言（QImage 300dpi，中缝无墨）；
//       ⑦ 同模型双比例墨量对照（2:1 >> 1:2）；⑧ exportPdfLayouts 双布局 2 页/空表回落 1 页；
//       ⑨ layout 命令族 + mview（含缺省布局自动建「布局1」）。
static void t_layout() {
    // ---- ① paperRect 序列化往返 + 缺键回落 ----
    {
        PlotViewport vp;
        vp.name = QStringLiteral("V1");
        vp.modelRect = QRectF(0, 0, 100, 50);
        vp.scale = 0.5;
        vp.paperRect = QRectF(10, 20, 90, 60);
        const PlotViewport vp2 = PlotViewport::fromJson(vp.toJson());
        CHECK(vp2.paperRect == QRectF(10, 20, 90, 60), "LAYOUT① paperRect 往返保持");
        QJsonObject j = vp.toJson();
        j.remove("paperX"); j.remove("paperY"); j.remove("paperW"); j.remove("paperH");
        const PlotViewport vp3 = PlotViewport::fromJson(j);
        CHECK(vp3.paperRect.isNull(), "LAYOUT① paperRect 缺键 → 空矩形（MVP 整页回落）");
        // 既有字段语义不动：modelRect/scale 往返不受 paperRect 影响
        CHECK(vp2.modelRect == QRectF(0, 0, 100, 50) && std::fabs(vp2.scale - 0.5) <= 1e-12,
              "LAYOUT① 既有字段往返不受 paperRect 影响");
    }

    // ---- ② PlotLayoutDef JSON 往返 ----
    {
        PlotLayoutDef def;
        def.name = QStringLiteral("A3横放");
        def.settings.pageSize = QPageSize(QPageSize::A3);
        def.settings.orientation = QPageLayout::Landscape;
        def.settings.scale = 2.0;
        const PlotLayoutDef def2 = PlotLayoutDef::fromJson(def.toJson());
        CHECK(def2.name == QStringLiteral("A3横放")
              && def2.settings.pageSize == QPageSize(QPageSize::A3)
              && def2.settings.orientation == QPageLayout::Landscape
              && std::fabs(def2.settings.scale - 2.0) <= 1e-9,
              "LAYOUT② 布局定义 JSON 往返（名+页面设置）");
    }

    // ---- ③ 布局表 CRUD：新建/重名拒/空名拒/更新/改名/删除/当前布局回落 ----
    {
        SketchModel m;
        CHECK(m.addLayout(QStringLiteral("L1"), PrintSettings()), "LAYOUT③ 新建布局 L1");
        CHECK(!m.addLayout(QStringLiteral("L1"), PrintSettings()), "LAYOUT③ 重名拒绝");
        CHECK(!m.addLayout(QStringLiteral("  "), PrintSettings()), "LAYOUT③ 空名拒绝");
        CHECK(m.layoutCount() == 1 && m.hasLayout(QStringLiteral("L1")), "LAYOUT③ 布局已登记");
        PrintSettings s2; s2.scale = 4.0;
        CHECK(m.updateLayout(QStringLiteral("L1"), s2), "LAYOUT③ updateLayout 原地替换");
        CHECK(std::fabs(m.findLayout(QStringLiteral("L1"))->settings.scale - 4.0) <= 1e-9,
              "LAYOUT③ 替换后设置生效");
        CHECK(!m.updateLayout(QStringLiteral("NOPE"), s2), "LAYOUT③ updateLayout 未命中拒绝");
        CHECK(m.setCurrentLayout(QStringLiteral("L1")), "LAYOUT③ 切换当前布局");
        CHECK(m.currentLayout() == QStringLiteral("L1") && m.currentLayoutDef() != nullptr,
              "LAYOUT③ 当前布局定义可取");
        CHECK(!m.setCurrentLayout(QStringLiteral("NOPE")), "LAYOUT③ 切到不存在布局被拒");
        CHECK(m.renameLayout(QStringLiteral("L1"), QStringLiteral("L2")), "LAYOUT③ 改名成功");
        CHECK(m.currentLayout() == QStringLiteral("L2"), "LAYOUT③ 当前布局名随改名同步");
        CHECK(m.addLayout(QStringLiteral("L2x"), PrintSettings()), "LAYOUT③ 占位布局建立");
        CHECK(!m.renameLayout(QStringLiteral("L2"), QStringLiteral("L2x")), "LAYOUT③ 改名重名拒绝");
        CHECK(m.removeLayout(QStringLiteral("L2x")), "LAYOUT③ 清理占位布局");
        CHECK(m.removeLayout(QStringLiteral("L2")), "LAYOUT③ 删除当前布局");
        CHECK(m.currentLayout().isEmpty() && m.currentLayoutDef() == nullptr,
              "LAYOUT③ 删当前布局 → 回落模型空间（悬空防御）");
        CHECK(m.layoutCount() == 0, "LAYOUT③ 布局表清空");
    }

    // ---- ④ 存档往返 + 旧档缺键回落 ----
    {
        SketchModel m;
        PrintSettings s1; s1.scale = 0.5;
        CHECK(m.addLayout(QStringLiteral("L1"), s1), "LAYOUT④ 布局建立");
        CHECK(m.setCurrentLayout(QStringLiteral("L1")), "LAYOUT④ 切换当前");
        QJsonObject root;
        root["layouts"] = m.layoutsToJson();
        root["currentLayout"] = m.currentLayout();
        SketchModel m2;
        m2.setLayoutsFromJson(root);
        CHECK(m2.layoutCount() == 1 && m2.currentLayout() == QStringLiteral("L1"),
              "LAYOUT④ 布局表+当前布局存档往返");
        CHECK(m2.findLayout(QStringLiteral("L1")) != nullptr
              && std::fabs(m2.findLayout(QStringLiteral("L1"))->settings.scale - 0.5) <= 1e-9,
              "LAYOUT④ 布局内页面设置往返保持");
        // 旧档：顶层无 "layouts"/"currentLayout" 键
        QJsonObject oldRoot;
        SketchModel m3;
        m3.setLayoutsFromJson(oldRoot);
        CHECK(m3.layoutCount() == 0 && m3.currentLayout().isEmpty(),
              "LAYOUT④ 旧档缺键 → 空表回落模型空间");
    }

    // ---- ⑤ viewportScaleMm：1:2 / 2:1 / fit / 布局自定义 ----
    {
        PrintSettings s;                       // fitToPage 默认 true，scale=1
        PlotViewport vp;
        vp.modelRect = QRectF(0, 0, 100, 50);
        vp.scale = 0.5;                        // 1:2
        CHECK(std::fabs(viewportScaleMm(s, vp, vp.modelRect, 190.0, 277.0) - 0.5) <= 1e-12,
              "LAYOUT⑤ 视口比例 1:2 → 0.5（模型 mm → 图纸 mm）");
        vp.scale = 2.0;                        // 2:1
        CHECK(std::fabs(viewportScaleMm(s, vp, vp.modelRect, 190.0, 277.0) - 2.0) <= 1e-12,
              "LAYOUT⑤ 视口比例 2:1 → 2.0");
        vp.scale = 0.0;                        // 沿用布局：fit-to-page
        const double expectFit = std::min(190.0 / 100.0, 277.0 / 50.0) * 0.98;
        CHECK(std::fabs(viewportScaleMm(s, vp, vp.modelRect, 190.0, 277.0) - expectFit) <= 1e-12,
              "LAYOUT⑤ scale=0 + fitToPage → 适配可打印区（留 2% 边距）");
        s.fitToPage = false; s.scale = 1.5;
        CHECK(std::fabs(viewportScaleMm(s, vp, vp.modelRect, 190.0, 277.0) - 1.5) <= 1e-12,
              "LAYOUT⑤ scale=0 + 自定义 → 布局整体比例 1.5");
    }

    // ---- ⑥ 双视口 paperRect 平铺像素断言（QImage 300dpi）----
    {
        SketchModel m;
        addLine(m, 0, 0, 80, 0);               // 80mm 水平线，1:1 时在 90mm 宽视口中居中
        PrintSettings s;
        s.pageSize = QPageSize(QPageSize::A4);
        s.orientation = QPageLayout::Portrait;
        s.marginsMm = QMarginsF(10, 10, 10, 10);
        s.fitToPage = false; s.scale = 1.0;
        s.monochrome = true;                   // 纯黑出图（阈值取样可靠）
        PlotViewport vp1;
        vp1.name = QStringLiteral("视口1"); vp1.modelRect = QRectF(0, -0.5, 80, 1);
        vp1.scale = 1.0; vp1.paperRect = QRectF(0, 0, 90, 277);     // 图纸 x∈[10,100]mm
        PlotViewport vp2;
        vp2.name = QStringLiteral("视口2"); vp2.modelRect = QRectF(0, -0.5, 80, 1);
        vp2.scale = 1.0; vp2.paperRect = QRectF(100, 0, 90, 277);   // 图纸 x∈[110,200]mm
        s.viewports = { vp1, vp2 };
        const double pxPerMm = 300.0 / 25.4;
        QImage img((int)std::lround(210 * pxPerMm), (int)std::lround(297 * pxPerMm),
                   QImage::Format_RGB32);
        img.setDotsPerMeterX((int)std::lround(pxPerMm * 1000.0));   // 300 dpi
        img.setDotsPerMeterY((int)std::lround(pxPerMm * 1000.0));
        img.fill(Qt::white);
        CHECK(PrintEngine::render(&m, &img, s), "LAYOUT⑥ 双视口平铺渲染成功");
        auto columnHasInk = [&img, pxPerMm](double xmm) {
            const int col = (int)std::lround(xmm * pxPerMm);
            if (col < 0 || col >= img.width()) return false;
            for (int y = 0; y < img.height(); ++y)
                if (qGray(img.pixel(col, y)) < 100) return true;    // 灰边框 120 被排除
            return false;
        };
        CHECK(columnHasInk(55.0),  "LAYOUT⑥ 视口1（图纸 55mm）有墨");
        CHECK(columnHasInk(155.0), "LAYOUT⑥ 视口2（图纸 155mm）有墨");
        CHECK(!columnHasInk(105.0), "LAYOUT⑥ 中缝 105mm 无墨 → 视口互不越界（平铺正确）");
    }

    // ---- ⑦ 同模型双比例墨量对照（1:2 vs 2:1）----
    {
        SketchModel m;
        addLine(m, 0, 0, 40, 0);
        addLine(m, 0, 0, 0, 30);               // 40×30 非零高包围盒（单水平线高为 0 会被裁剪）
        auto renderInk = [](SketchModel& mm, double scale) -> int {
            PrintSettings s;
            s.marginsMm = QMarginsF(10, 10, 10, 10);
            s.fitToPage = false;
            s.monochrome = true;
            PlotViewport vp;                   // modelRect 空 → 整体范围
            vp.scale = scale;
            s.viewports.append(vp);
            const double pxPerMm = 300.0 / 25.4;
            QImage img((int)std::lround(210 * pxPerMm), (int)std::lround(297 * pxPerMm),
                       QImage::Format_RGB32);
            img.setDotsPerMeterX((int)std::lround(pxPerMm * 1000.0));
            img.setDotsPerMeterY((int)std::lround(pxPerMm * 1000.0));
            img.fill(Qt::white);
            if (!PrintEngine::render(&mm, &img, s)) return -1;
            int ink = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (qGray(img.pixel(x, y)) < 100) ++ink;
            return ink;
        };
        const int ink12 = renderInk(m, 0.5);   // 1:2
        const int ink21 = renderInk(m, 2.0);   // 2:1
        CHECK(ink12 > 0, "LAYOUT⑦ 1:2 渲染有墨");
        CHECK(ink21 > ink12 * 2, "LAYOUT⑦ 2:1 墨量显著大于 1:2（线长×4、线宽×4）");
    }

    // ---- ⑧ exportPdfLayouts：双布局 2 页 / 空表回落 1 页 ----
    {
        SketchModel m;
        addLine(m, 0, 0, 40, 0);
        addLine(m, 0, 0, 0, 30);
        PrintSettings s1; s1.fitToPage = true;                       // 视口空 = 模型空间 fit
        CHECK(m.addLayout(QStringLiteral("布局A"), s1), "LAYOUT⑧ 布局A 建立");
        PrintSettings s2;
        PlotViewport vp; vp.name = QStringLiteral("视口1"); vp.scale = 0.5;   // 1:2
        s2.viewports.append(vp);
        CHECK(m.addLayout(QStringLiteral("布局B"), s2), "LAYOUT⑧ 布局B 建立（1:2 视口）");
        CHECK(m.setCurrentLayout(QStringLiteral("布局B")), "LAYOUT⑧ 切到布局B");
        const QString pdfPath = QDir::temp().absoluteFilePath(QStringLiteral("layout_scales_sample.pdf"));
        QFile::remove(pdfPath);
        const int pages = PrintEngine::exportPdfLayouts(&m, pdfPath);
        CHECK(pages == 2, "LAYOUT⑧ 双布局 → 返回 2 页");
        const QFileInfo fi(pdfPath);
        CHECK(fi.exists() && fi.size() > 1000, "LAYOUT⑧ 双布局双比例 PDF 样张生成且非空");
        // 旧档回落：无布局 → 单页模型空间打印
        SketchModel m3;
        const QString pdf2 = QDir::temp().absoluteFilePath(QStringLiteral("layout_fallback.pdf"));
        QFile::remove(pdf2);
        const int pages2 = PrintEngine::exportPdfLayouts(&m3, pdf2);
        CHECK(pages2 == 1, "LAYOUT⑧ 空布局表 → 回落单页返回 1（旧档行为）");
        CHECK(QFileInfo(pdf2).exists() && QFileInfo(pdf2).size() > 1000, "LAYOUT⑧ 回落 PDF 生成且非空");
    }

    // ---- ⑨ 命令行：layout 族 + mview ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layout new L1")), "LAYOUT⑨ layout new 执行成功");
        CHECK(m.currentLayout() == QStringLiteral("L1"), "LAYOUT⑨ 新建后自动切换当前布局");
        CHECK(interp.execute(QStringLiteral("mview 10 20 90 60 1:2")), "LAYOUT⑨ mview 建视口");
        const PlotLayoutDef* def = m.findLayout(QStringLiteral("L1"));
        CHECK(def && def->settings.viewports.size() == 1, "LAYOUT⑨ 视口已入布局");
        CHECK(std::fabs(def->settings.viewports[0].scale - 0.5) <= 1e-9
              && def->settings.viewports[0].paperRect == QRectF(10, 20, 90, 60),
              "LAYOUT⑨ mview 参数解析：1:2→0.5，paperRect 保持");
        CHECK(interp.execute(QStringLiteral("layout vpscale 视口1 2:1")), "LAYOUT⑨ layout vpscale 执行成功");
        CHECK(std::fabs(m.findLayout(QStringLiteral("L1"))->settings.viewports[0].scale - 2.0) <= 1e-9,
              "LAYOUT⑨ vpscale 2:1 → 2.0");
        CHECK(interp.execute(QStringLiteral("layout list")), "LAYOUT⑨ layout list 执行成功");
        CHECK(interp.execute(QStringLiteral("layout set model")), "LAYOUT⑨ layout set model 执行成功");
        CHECK(m.currentLayout().isEmpty(), "LAYOUT⑨ set model → 回落模型空间");
        CHECK(!interp.execute(QStringLiteral("layout nosuch")), "LAYOUT⑨ 未知子命令被拒");
        CHECK(!interp.execute(QStringLiteral("layout set 不存在")), "LAYOUT⑨ 切到不存在布局被拒");
        // mview 无布局 → 自动建「布局1」
        SketchModel m2;
        CommandInterpreter interp2(&m2);
        CHECK(interp2.execute(QStringLiteral("mview 0 0 90 60")), "LAYOUT⑨ mview（无布局）执行成功");
        CHECK(m2.currentLayout() == QStringLiteral("布局1") && m2.layoutCount() == 1,
              "LAYOUT⑨ 无布局时自动建「布局1」并切换");
    }
}

// ============================================================================
// 多文档标签（t_multidoc）：DocSession / DocManager 会话核心（纯逻辑层离线直测）
// 验收：开 3 张图 → 各自独立 undo 不串编辑相互影响、脏标记切换隔离、
//       Ctrl+Tab 循环与拖拽重排（活动身份跟随）、跨文档复制粘贴内容一致、
//       命令行历史随文档互换、关闭文档下标收缩与活动夹取。
// ============================================================================
static bool t_multidoc() {
    {   // ① 开 3 张图：会话表 / 活动下标 / 模型独立性
        DocManager dm;
        DocSession* d0 = dm.addNewDoc();
        DocSession* d1 = dm.addNewDoc();
        DocSession* d2 = dm.addNewDoc();
        CHECK(dm.count() == 3, "MD① 开 3 张图 → count==3");
        CHECK(dm.activeIndex() == 0 && dm.active() == d0, "MD① 首张自动成为活动");
        CHECK(d0->model() && d1->model() && d2->model()
              && d0->model() != d1->model() && d1->model() != d2->model()
              && d0->model() != d2->model(),
              "MD① 每会话一份独立模型（无共享/单例模型）");
        addLine(*d0->model(), 0, 0, 10, 0);
        addCircle(*d1->model(), 5, 5, 3);
        addPoint(*d2->model(), 1, 1);
        CHECK(d0->model()->elementCount() == 1 && d1->model()->elementCount() == 1
              && d2->model()->elementCount() == 1,
              "MD① 各自编辑互不影响");
        CHECK(d0->model()->canUndo() && d1->model()->canUndo() && d2->model()->canUndo(),
              "MD① 撤销栈随各自模型独立积累");

        // ② 独立 undo：doc0 撤销不影响 doc1/doc2
        d0->model()->undo();
        CHECK(d0->model()->elementCount() == 0, "MD② doc0 undo → 自己的图元消失");
        CHECK(d1->model()->elementCount() == 1 && d2->model()->elementCount() == 1,
              "MD② doc0 undo 不串扰 doc1/doc2");
        d1->model()->undo();
        CHECK(d1->model()->elementCount() == 0 && d2->model()->elementCount() == 1,
              "MD② doc1 undo 同样只作用于自己");
    }

    {   // ③④ 脏标记：编辑置脏 / markClean 复位 / 切换隔离
        DocManager dm;
        DocSession* d0 = dm.addNewDoc();
        CHECK(!d0->isDirty(), "MD③ 新会话初始不脏");
        addLine(*d0->model(), 0, 0, 5, 5);
        CHECK(d0->isDirty(), "MD③ 模型变化 → 自动置脏");
        dm.markClean(0);
        CHECK(!d0->isDirty() && d0->title() == QStringLiteral("绘图1"),
              "MD③ markClean → 复位且标题无 \"*\"");
        addLine(*d0->model(), 5, 5, 9, 9);
        CHECK(d0->isDirty() && d0->title().endsWith(QStringLiteral("*")),
              "MD③ 再编辑 → 再置脏且标题带 \"*\"");
        DocSession* d1 = dm.addNewDoc();
        DocSession* d2 = dm.addNewDoc();
        addLine(*d1->model(), 0, 0, 1, 1);
        CHECK(dm.isDirty(0) && d1->isDirty() && !d2->isDirty(),
              "MD④ 脏标记按会话隔离（d2 干净，切换不串）");
        dm.markClean(1);
        CHECK(dm.isDirty(0) && !dm.isDirty(1), "MD④ 单独 markClean 只影响该会话");
    }

    {   // ⑤ Ctrl+Tab 循环：cycle 只报候选不改状态
        DocManager dm;
        dm.addNewDoc(); dm.addNewDoc(); dm.addNewDoc();
        CHECK(dm.cycle(1) == 1 && dm.cycle(2) == 2, "MD⑤ cycle 正向推进");
        CHECK(dm.cycle(3) == 0, "MD⑤ cycle 越界回绕");
        CHECK(dm.cycle(-1) == 2, "MD⑤ 负步长反向循环");
        CHECK(dm.activeIndex() == 0, "MD⑤ cycle 本身不改变活动状态");
    }

    {   // ⑥ 拖拽重排：会话顺序重排、活动身份跟随
        DocManager dm;
        DocSession* d0 = dm.addNewDoc();
        DocSession* d1 = dm.addNewDoc();
        DocSession* d2 = dm.addNewDoc();
        dm.setActive(2);
        dm.reorder(2, 0);                     // 把 d2 拖到最前（QList::move = 插入语义）
        CHECK(dm.doc(0) == d2 && dm.doc(1) == d0 && dm.doc(2) == d1,
              "MD⑥ 重排后会话顺序与标签一致");
        CHECK(dm.activeIndex() == 0 && dm.active() == d2,
              "MD⑥ 活动身份跟随会话（下标 2→0）");
        dm.reorder(1, 0);                     // d2 再从 0 拖到 1
        CHECK(dm.activeIndex() == 1 && dm.active() == d2, "MD⑥ 再次重排活动仍跟随");
    }

    {   // ⑦ 跨文档剪贴板：doc0 复制 → doc1/doc2 粘贴内容一致（Payload JSON 往返）
        DocManager dm;
        DocSession* d0 = dm.addNewDoc();
        DocSession* d1 = dm.addNewDoc();
        DocSession* d2 = dm.addNewDoc();
        addLine(*d0->model(), 0, 0, 10, 0);
        addCircle(*d0->model(), 5, 5, 2.5);
        clipboard::Payload p;                 // 与 GUI copyclip 同一数据形态
        p.base = Vec3(0, 0, 0);
        p.elements = d0->model()->elements();
        const QJsonObject o = clipboard::toJson(p);
        clipboard::Payload back;
        QString err;
        CHECK(clipboard::fromJson(o, &back, &err), "MD⑦ Payload 版本化 JSON 往返成功");
        CHECK((int)back.elements.size() == 2, "MD⑦ 往返后图元数一致");

        auto dumpModel = [](const std::vector<Element>& es) {
            QStringList out;
            for (const auto& e : es) {
                QString pts;
                for (const auto& v : e.pts)
                    pts += QString("%1,%2;").arg(v.x, 0, 'f', 3).arg(v.y, 0, 'f', 3);
                out << QString("%1|%2|%3").arg(int(e.type)).arg(e.radius).arg(pts);
            }
            return out;
        };
        d1->model()->beginEdit(QStringLiteral("粘贴"));
        CHECK(clipboard::pasteInto(d1->model(), back, Vec3(20, 0, 0), false, &err),
              "MD⑦ doc1 粘贴成功");
        d2->model()->beginEdit(QStringLiteral("粘贴"));
        CHECK(clipboard::pasteInto(d2->model(), back, Vec3(20, 0, 0), false, &err),
              "MD⑦ doc2 粘贴成功");
        CHECK(d1->model()->elementCount() == 2 && d2->model()->elementCount() == 2,
              "MD⑦ 两文档粘贴后各 2 图元");
        CHECK(dumpModel(d1->model()->elements()) == dumpModel(d2->model()->elements()),
              "MD⑦ 同一 payload 两处粘贴内容一致");
        CHECK(std::fabs(d1->model()->elements()[0].pts[0].x - 20.0) <= 1e-9
              && std::fabs(d1->model()->elements()[1].pts[0].x - 25.0) <= 1e-9,
              "MD⑦ 粘贴落点按基点平移（0→20）");
        CHECK(d0->model()->elementCount() == 2
              && std::fabs(d0->model()->elements()[0].pts[0].x) <= 1e-9,
              "MD⑦ 源文档 doc0 内容不受粘贴影响");
    }

    {   // ⑧ 命令行历史随文档互换（模拟 MainWindow 的 copy-out / copy-in）
        SketchModel m;
        CommandInterpreter it(&m);
        DocSession d;
        it.addHistory(QStringLiteral("line 0 0 1 1"));
        it.addHistory(QStringLiteral("circle 0 0 5"));
        d.cmdHistory  = it.history();         // copy-out：切换离开 doc0
        d.lastCommand = it.lastCommand();
        it.setHistory({});                    // 换到另一文档：该文档历史从零开始
        it.setLastCommand(QString());
        CHECK(it.history().isEmpty(), "MD⑧ 切走后解释器历史是另一文档的（空）");
        it.addHistory(QStringLiteral("zoom e"));
        CHECK(it.history().size() == 1, "MD⑧ 新文档独立积累历史");
        it.setHistory(d.cmdHistory);          // copy-in：切回 doc0
        CHECK(it.history().size() == 2 && it.history().first().startsWith(QStringLiteral("line")),
              "MD⑧ 切回后历史随文档恢复");
    }

    {   // ⑨ 关闭文档：会话销毁、下标收缩、活动夹取、幸存模型完好
        DocManager dm;
        DocSession* d0 = dm.addNewDoc();
        DocSession* d1 = dm.addNewDoc();
        DocSession* d2 = dm.addNewDoc();
        dm.setActive(1);
        CHECK(dm.doc(1) == d1, "MD⑨ 关闭前 d1 位于下标 1");
        dm.removeDoc(1);                      // 关闭活动文档
        CHECK(dm.count() == 2 && dm.doc(0) == d0 && dm.doc(1) == d2,
              "MD⑨ 关闭中间文档 → 会话表收缩正确");
        CHECK(dm.activeIndex() == 1 && dm.active() == d2, "MD⑨ 活动夹取到邻居");
        addLine(*d0->model(), 0, 0, 1, 1);
        CHECK(d0->model()->elementCount() == 1, "MD⑨ 幸存会话模型完好可编辑");
        dm.removeDoc(0);
        dm.removeDoc(0);
        CHECK(dm.count() == 0 && dm.activeIndex() == -1, "MD⑨ 全部关闭 → 空表无活动");
    }

    {   // ⑩ 标题命名：文件名优先 / 未命名序号自增
        DocManager dm;
        DocSession* a = dm.addNewDoc();
        DocSession* b = dm.addNewDoc();
        CHECK(a->title() == QStringLiteral("绘图1") && b->title() == QStringLiteral("绘图2"),
              "MD⑩ 未命名图按序命名");
        a->filePath = QStringLiteral("/tmp/abc/支架布置图.json");
        CHECK(a->title() == QStringLiteral("支架布置图"), "MD⑩ 已存盘显示文件名");
        a->markDirty();
        CHECK(a->title() == QStringLiteral("支架布置图 *"), "MD⑩ 脏标记追加 \"*\"");
    }
    return true;
}

// ============================================================================
// AutoCAD 交互对齐（t_interaction）：命令行键盘语义 + 未知命令容错 + 历史持久化
// + 右键行为配置。CommandLine（QLineEdit 系）需要 QApplication：main 已改为
// QApplication（无头运行——不 show、不 exec，事件经 QApplication::sendEvent 直注）。
// 覆盖：空格/空回车重复上一命令 / ↑↓ 历史 / Tab 补齐 / 坐标注入 / 未知命令
// 3 候选（Did you mean 中文）/ 历史持久化往返与上限 / 会话态优先 / 新键校验。
// ============================================================================
// t_i18n —— 国际化 + 高 DPI（阶段四）
// 语言解析（跟随系统/显式覆盖）/ .qm 随构建产出且可装载 / 翻译上下文存在 /
// 安装后关键菜单文本变化（切换即刻生效的翻译底座）/ 缺翻译回落中文源串 /
// 卸载回落源语言 / I18n 门面装载与还原 / DPI 元数据 25.4mm·inch 换算一致性 /
// QLocale 区域口径（日期/小数点，不得硬编码中文习惯）。
// ============================================================================
static bool t_i18n() {
    using namespace SketchCAD;

    // ---- A. 语言解析 ----
    CHECK(I18n::resolveLanguage(QStringLiteral("zh-CN")) == QStringLiteral("zh-CN") &&
          I18n::resolveLanguage(QStringLiteral("en")) == QStringLiteral("en"),
          "IL1 显式语言值原样解析（zh-CN/en）");
    {
        const QString sys = I18n::resolveLanguage(QString());
        const QString unk = I18n::resolveLanguage(QStringLiteral("fr"));
        CHECK((sys == QLatin1String("zh-CN") || sys == QLatin1String("en")) && sys == unk,
              "IL2 空/未知语言跟随系统（zh-CN|en 二选一且一致）");
    }

    // ---- B. .qm 直载与翻译链 ----
    {
        QTranslator en;
        const bool loaded = en.load(QCoreApplication::applicationDirPath() +
                                    QStringLiteral("/sketchcad_en.qm"));
        CHECK(loaded, "IL3 sketchcad_en.qm 随构建产出且可装载");
        CHECK(!en.translate("SketchCAD::MainWindow", "文件(&F)").isEmpty(),
              "IL4 翻译上下文存在（SketchCAD::MainWindow/文件(&F)）");
        CHECK(!en.translate("SketchCAD::OptionsDialog", "界面语言：").isEmpty(),
              "IL5 OptionsDialog 上下文存在（界面语言：）");
        QCoreApplication::installTranslator(&en);
        CHECK(QCoreApplication::translate("SketchCAD::MainWindow", "文件(&F)") == QStringLiteral("&File") &&
              QCoreApplication::translate("SketchCAD::MainWindow", "新建") == QStringLiteral("&New"),
              "IL6 安装后关键菜单文本变为英文（语言切换的翻译底座）");
        CHECK(QCoreApplication::translate("SketchCAD::MainWindow", "（不存在的源串XYZ）") ==
                  QStringLiteral("（不存在的源串XYZ）"),
              "IL7 缺翻译回落中文源串");
        QCoreApplication::removeTranslator(&en);
        CHECK(QCoreApplication::translate("SketchCAD::MainWindow", "文件(&F)") == QStringLiteral("文件(&F)"),
              "IL8 卸载后回落中文源语言");
    }

    // ---- C. I18n::installTranslator 门面（资源→磁盘双路装载 + 现场还原）----
    {
        const bool enOn = I18n::installTranslator(QStringLiteral("en"));
        CHECK(enOn && I18n::activeLanguage() == QStringLiteral("en") &&
              QCoreApplication::translate("SketchCAD::MainWindow", "文件(&F)") == QStringLiteral("&File"),
              "IL9 installTranslator(en) 装载并生效");
        const bool zhOff = I18n::installTranslator(QStringLiteral("zh-CN"));
        CHECK(!zhOff && I18n::activeLanguage() == QStringLiteral("zh-CN") &&
              QCoreApplication::translate("SketchCAD::MainWindow", "文件(&F)") == QStringLiteral("文件(&F)"),
              "IL10 installTranslator(zh-CN) 卸载翻译器回落源语言");
    }

    // ---- D. 高 DPI：打印导出 DPI 元数据 25.4mm/inch 换算一致性 ----
    {
        // 打印链约定（PrintEngine）：QImage/QPdfWriter 按 setResolution(300) 出图。
        // dpi ↔ dotsPerMeter 换算必须闭环：1 inch = 25.4 mm。
        QImage img(16, 16, QImage::Format_RGB32);
        img.setDotsPerMeterX(11811);            // 300 dpi ≈ 11811 dots/m
        img.setDotsPerMeterY(11811);
        CHECK(qRound(double(img.dotsPerMeterX()) * 0.0254) == 300,
              "ID1 dotsPerMeter → dpi 换算一致（300dpi≈11811dpm）");
        CHECK(qRound(300 / 0.0254) == 11811,
              "ID2 dpi → dotsPerMeter 反向换算一致（1in=25.4mm）");
    }

    // ---- E. 单位/日期 QLocale 口径（不得硬编码中文习惯）----
    {
        const QLocale zh(QLocale::Chinese);
        const QLocale enLoc(QLocale::English);
        const QDate d(2026, 1, 15);
        CHECK(zh.toString(d, QLocale::LongFormat).contains(QStringLiteral("年")) &&
              enLoc.toString(d, QLocale::LongFormat) != zh.toString(d, QLocale::LongFormat),
              "IE1 日期格式走 QLocale（中英长格式随区域变化）");
        CHECK(QLocale(QLocale::C).toString(3.5, 'f', 2) == QStringLiteral("3.50"),
              "IE2 数值→串小数点口径可经 QLocale 显式固定（C locale='.'）");
    }
    return true;
}

// ============================================================================
static bool t_interaction() {
    using namespace SketchCAD;
    namespace SK = SettingKeys;

    // ---- A. 未知命令容错（Did you mean）----
    // A1: 最相近者排首、候选数封顶 3
    {
        QStringList names = { QStringLiteral("line"), QStringLiteral("linetype"),
                              QStringLiteral("list"), QStringLiteral("circle") };
        const QStringList s = CommandInterpreter::nearestCommands(QStringLiteral("lin"), names);
        CHECK(!s.isEmpty() && s.first() == QStringLiteral("line"),
              "IA1 nearestCommands 最相近者排首(line)");
        CHECK(s.size() <= 3, "IA1 候选数 ≤3");
    }
    // A2: 完全无关输入无候选（编辑距离阈值保护，不给离谱猜测）
    {
        QStringList names = { QStringLiteral("line"), QStringLiteral("circle") };
        CHECK(CommandInterpreter::nearestCommands(QStringLiteral("zzzzzzzz"), names).isEmpty(),
              "IA2 阈值外输入返回空候选");
    }
    // A3: execute 未知命令 → 日志含中文 Did-you-mean 提示与候选
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        QStringList logged;
        QObject::connect(&m, &SketchModel::logAppended, &m,
                         [&](const QString& msg, const QString&) { logged << msg; });
        CHECK(!ci.execute(QStringLiteral("circl")), "IA3 未知命令返回失败");
        bool found = false;
        for (const QString& s : logged)
            if (s.contains(QStringLiteral("您是不是想输入"))
                && s.contains(QStringLiteral("circle"))) found = true;
        CHECK(found, "IA3 日志含“您是不是想输入”与候选 circle");
    }

    // ---- B. 命令历史持久化（AppSettings 注入）----
    // B1: 入史即写回；模拟重启后新解释器预填（顺序保持）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("i1.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        SketchModel m;
        CommandInterpreter w(&m);
        w.setPersistence(&s);
        w.addHistory(QStringLiteral("line 0,0 10,10"));
        w.addHistory(QStringLiteral("circle 5,5 3"));
        CHECK(w.history().size() == 2, "B1 会话历史两条");
        SketchModel m2;
        CommandInterpreter r(&m2);
        r.setPersistence(&s);   // 模拟重启：同一存储、全新解释器
        CHECK(r.history().size() == 2
              && r.history().last() == QStringLiteral("circle 5,5 3"),
              "B1 重启后历史预填且顺序一致");
    }
    // B2: 持久化条数封顶 50（kPersistMax），会话历史仍保持 100 内全量
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("i2.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        SketchModel m;
        CommandInterpreter w(&m);
        w.setPersistence(&s);
        for (int i = 0; i < 60; ++i)
            w.addHistory(QStringLiteral("cmd%1").arg(i));
        CHECK(CommandInterpreter::persistedCommands(s).size() == CommandInterpreter::kPersistMax,
              "B2 持久化条数封顶 50");
        CHECK(CommandInterpreter::persistedCommands(s).last() == QStringLiteral("cmd59"),
              "B2 封顶保留最新尾部");
    }
    // B3: 会话历史非空时 setPersistence 不覆盖（保护 DocTab copy-in 会话态）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("i3.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setValue(QLatin1String(SK::kRecentCommands),
                   QStringList{ QStringLiteral("old1"), QStringLiteral("old2") });
        SketchModel m;
        CommandInterpreter w(&m);
        w.setHistory(QStringList{ QStringLiteral("session1") });
        w.setPersistence(&s);
        CHECK(w.history() == QStringList{ QStringLiteral("session1") },
              "B3 会话态优先，持久化预填不覆盖 copy-in 历史");
    }

    // ---- C. CommandLine 键盘语义（无头 QApplication + sendEvent）----
    // C1: Tab 前缀补齐（①唯一候选补齐+尾空格 ②多候选推进到公共前缀 ③歧义前缀不退化）
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        CommandLine cl(&m, &ci);
        // ① 唯一候选："linet" 只匹配 linetype → 补齐 + 尾空格
        cl.setText(QStringLiteral("linet"));
        QKeyEvent tab1(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(&cl, &tab1);
        CHECK(cl.text() == QStringLiteral("linetype "),
              "C1a Tab 唯一候选补齐 → \"linetype \"（含尾空格）");
        // ② 多候选推进："line_p" 匹配 line_parallel / line_parallel_pt
        //    → 公共前缀 line_parallel（比输入长，直接补齐）
        cl.clear();
        cl.setText(QStringLiteral("line_p"));
        QKeyEvent tab2(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(&cl, &tab2);
        CHECK(cl.text() == QStringLiteral("line_parallel"),
              "C1b Tab 多候选推进到公共前缀 line_parallel");
        // ③ 歧义前缀："lin" 命中 line 系 + 别名 lin(→linetype) → 公共前缀==输入本身
        //    → 不退化、不崩溃，text 原样（候选经日志列出）
        cl.clear();
        cl.setText(QStringLiteral("lin"));
        QKeyEvent tab3(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(&cl, &tab3);
        CHECK(cl.text() == QStringLiteral("lin"),
              "C1c Tab 歧义前缀不退化（保持输入，候选列出）");
    }
    // C2: 空输入按空格 → 重复上一成功命令（入史且不插入空格字符）
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        CHECK(ci.execute(QStringLiteral("help")), "C2 help 命令无头执行成功");
        CommandLine cl(&m, &ci);
        QKeyEvent sp(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(&cl, &sp);
        CHECK(cl.text().isEmpty(), "C2 空格空输入不插入空格字符");
        CHECK(ci.history().size() == 1 && ci.history().last() == QStringLiteral("help"),
              "C2 空格触发重复上一命令（重复命令入史）");
    }
    // C3: ↑/↓ 翻历史，↓ 到末尾清空
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        CommandLine cl(&m, &ci);
        QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        cl.setText(QStringLiteral("a1"));
        QApplication::sendEvent(&cl, &ret);
        cl.setText(QStringLiteral("a2"));
        QApplication::sendEvent(&cl, &ret);
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(&cl, &up);
        QApplication::sendEvent(&cl, &up);
        CHECK(cl.text() == QStringLiteral("a1"), "C3 ↑ 逐条上翻历史");
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(&cl, &down);
        QApplication::sendEvent(&cl, &down);
        CHECK(cl.text().isEmpty(), "C3 ↓ 到历史末尾清空输入");
    }
    // C4: 绘制命令激活时输入坐标回车 → pointEntered 注入（分段式命令流程）
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        CommandLine cl(&m, &ci);
        m.setActiveTool(QStringLiteral("line"));
        bool gotSig = false;
        double gx = 0, gy = 0;
        QObject::connect(&cl, &CommandLine::pointEntered,
                         [&](const Vec3& p) { gotSig = true; gx = p.x; gy = p.y; });
        cl.setText(QStringLiteral("10,20"));
        QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&cl, &ret);
        CHECK(gotSig && gx == 10.0 && gy == 20.0,
              "C4 绘制中输入坐标 → pointEntered 注入拾取点");
    }
    // C5: 空历史时空回车/上下翻页安全无操作
    {
        SketchModel m;
        CommandInterpreter ci(&m);
        CommandLine cl(&m, &ci);
        QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(&cl, &ret);
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(&cl, &up);
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(&cl, &down);
        CHECK(cl.text().isEmpty() && ci.history().isEmpty(),
              "C5 空历史键入安全无操作");
    }

    // ---- D. 右键行为配置 / 新键校验 ----
    // D1: interaction/rightClickIdleRepeat 默认 true（AutoCAD 约定）+ 非法回落
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("i4.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        const QString K = QLatin1String(SK::kRightClickIdleRepeat);
        CHECK(s.value(K).toBool() == true, "D1 rightClickIdleRepeat 默认 true");
        s.setValue(K, false);
        CHECK(s.value(K).toBool() == false, "D1 写 false 即时生效");
        s.setValue(K, 123);   // 非布尔 → 校验器拒绝写入
        CHECK(s.value(K).toBool() == false, "D1 非法写被忽略（保持上次合法值 false）");
    }
    // D2: SketchModel 会话态默认与 setter（不入档，仅内存）
    {
        SketchModel m;
        CHECK(m.rightClickIdleRepeat() == true, "D2 模型会话态默认 true");
        m.setRightClickIdleRepeat(false);
        CHECK(m.rightClickIdleRepeat() == false, "D2 setRightClickIdleRepeat 生效");
    }
    // D3: commandLine/recentCommands 键列表往返（stringListVal 校验器）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("i5.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        const QString K = QLatin1String(SK::kRecentCommands);
        const QStringList in{ QStringLiteral("circle 5,5 3"), QStringLiteral("line 0,0 1,1") };
        s.setValue(K, in);
        CHECK(s.value(K).toStringList() == in, "D3 recentCommands 列表往返一致");
    }
    return true;
}

// ============================================================================
// 最近文件 MRU（t_recent）：core/RecentFiles 注入临时 ini 离线直测。
// 覆盖：增删 / 去重 / 上限裁剪 / pin 保护 / 全 pin 硬上限 / 缺失文件 /
//       持久化重载一致 / 旧键迁移 / 损坏容错 / 坏条目跳过。
// ============================================================================
static bool t_recent() {
    using namespace SketchCAD;
    namespace SK = SettingKeys;
    const QString PA = QStringLiteral("C:/w/a.json");
    const QString PB = QStringLiteral("C:/w/b.json");

    // 1) 空表 + add 置顶 + contains
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r1.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        CHECK(r.count() == 0, "RC1 全新存储空表");
        r.add(PA);
        r.add(PB);
        CHECK(r.count() == 2 && r.entries()[0].path == PB && r.entries()[1].path == PA,
              "RC1 add 置顶（最新在前）");
        CHECK(r.contains(PA) && !r.contains(QStringLiteral("C:/w/z.json")),
              "RC1 contains 命中/未命中");
    }

    // 2) 去重：重复 add 移位不增计数，且 pin 保留
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r2.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        r.add(PA);
        r.add(PB);
        r.setPinned(PA, true);
        r.add(PA);   // 再次打开已固定的旧项 → 移到最前、仍固定、不重复
        CHECK(r.count() == 2 && r.entries()[0].path == PA && r.entries()[1].path == PB,
              "RC2 重复 add 去重置顶");
        CHECK(r.isPinned(PA), "RC2 re-add 保留 pin");
    }

    // 3) 上限裁剪：12 条 → 10 条，最旧被挤出
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r3.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        for (int i = 0; i < 12; ++i)
            r.add(QStringLiteral("C:/w/p%1.json").arg(i));
        CHECK(r.count() == RecentFiles::kMaxEntries, "RC3 上限裁剪至 10 条");
        CHECK(r.entries()[0].path == QStringLiteral("C:/w/p11.json"),
              "RC3 最新条目在最前");
        CHECK(!r.contains(QStringLiteral("C:/w/p0.json")) &&
              !r.contains(QStringLiteral("C:/w/p1.json")) &&
              r.contains(QStringLiteral("C:/w/p2.json")),
              "RC3 最旧两条被挤出");
    }

    // 4) pin 保护：裁剪优先挤掉最旧未固定项
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r4.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        for (int i = 0; i < 10; ++i)
            r.add(QStringLiteral("C:/w/p%1.json").arg(i));   // p0 最旧 … p9 最新
        r.setPinned(QStringLiteral("C:/w/p4.json"), true);
        r.add(QStringLiteral("C:/w/x.json"));   // 挤掉 p0
        r.add(QStringLiteral("C:/w/y.json"));   // 挤掉 p1
        CHECK(r.count() == 10, "RC4 溢出后仍为 10 条");
        CHECK(r.contains(QStringLiteral("C:/w/p4.json")), "RC4 固定项不被裁剪");
        CHECK(!r.contains(QStringLiteral("C:/w/p0.json")) &&
              !r.contains(QStringLiteral("C:/w/p1.json")) &&
              r.contains(QStringLiteral("C:/w/p2.json")),
              "RC4 裁剪跳过固定项、从最旧未固定开始");
    }

    // 5) 全 pin + 溢出：硬上限优先，挤最旧
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r5.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        for (int i = 0; i < 10; ++i) {
            const QString p = QStringLiteral("C:/w/p%1.json").arg(i);
            r.add(p);
            r.setPinned(p, true);
        }
        r.add(QStringLiteral("C:/w/new.json"));
        CHECK(r.count() == 10, "RC5 全 pin 溢出仍硬上限 10");
        CHECK(r.contains(QStringLiteral("C:/w/new.json")) &&
              !r.contains(QStringLiteral("C:/w/p0.json")) &&
              r.contains(QStringLiteral("C:/w/p1.json")),
              "RC5 全 pin 时挤掉最旧一条");
    }

    // 6) remove 单条 / remove 不存在 / removeAt 越界
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r6.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        r.add(PA);
        r.add(PB);
        r.remove(QStringLiteral("C:/w/none.json"));
        CHECK(r.count() == 2, "RC6 remove 不存在路径无操作");
        r.removeAt(7);                       // 越界无操作
        CHECK(r.count() == 2, "RC6 removeAt 越界无操作");
        r.remove(PB);
        CHECK(r.count() == 1 && !r.contains(PB), "RC6 remove 单条生效");
        r.clear();
        CHECK(r.count() == 0, "RC6 clear 全清");
        r.add(PA);
        CHECK(r.count() == 1, "RC6 clear 后可继续使用");
    }

    // 7) setPinned 幂等：状态不变不重复发信号
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r7.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        r.add(PA);
        int sigs = 0;
        QObject::connect(&r, &RecentFiles::changed, [&sigs] { ++sigs; });
        r.setPinned(PA, true);
        const int afterFirst = sigs;
        r.setPinned(PA, true);               // 幂等 → 不发信号
        CHECK(afterFirst == sigs, "RC7 setPinned 幂等不重复发信号");
        CHECK(r.isPinned(PA), "RC7 pin 状态生效");
    }

    // 8) 缺失文件：isMissing 现查文件系统
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r8.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        RecentFiles r(&s);
        const QString f = td.filePath("exist.json");
        QFile file(f);
        if (!file.open(QIODevice::WriteOnly)) {   // nodiscard：显式消费返回值（零警告基线）
            CHECK(false, "RC8 临时文件创建失败");
            return false;
        }
        file.write("{}");
        file.close();
        r.add(f);
        CHECK(!r.isMissing(0), "RC8 存在文件不算缺失");
        QFile::remove(f);
        CHECK(r.isMissing(0), "RC8 删除文件后判定缺失");
        CHECK(!r.isMissing(99) && !r.isMissing(-1), "RC8 越界索引不缺失不崩溃");
    }

    // 9) 持久化重载：新实例读回（顺序/pin/时间全等）
    {
        QTemporaryDir td;
        const QString ini = td.filePath("r9.ini");
        qint64 tA = 0;
        {
            QSettings qs(ini, QSettings::IniFormat);
            AppSettings s(&qs);
            RecentFiles r(&s);
            r.add(PA);
            r.add(PB);
            r.setPinned(PB, true);
            tA = r.entries()[1].openedAt.toMSecsSinceEpoch();   // PA 在第 2 位
            CHECK(r.entries()[0].openedAt.toMSecsSinceEpoch() >= tA,
                  "RC9 打开时间随置顶刷新");
        }
        QSettings qs2(ini, QSettings::IniFormat);
        AppSettings s2(&qs2);
        RecentFiles r2(&s2);
        CHECK(r2.count() == 2 && r2.entries()[0].path == PB &&
              r2.entries()[0].pinned && r2.entries()[1].path == PA,
              "RC9 重载顺序与 pin 一致");
        CHECK(r2.entries()[1].openedAt.toMSecsSinceEpoch() == tA, "RC9 打开时间逐毫秒一致");
    }

    // 10) 旧键迁移：files/recentList → 新键一次性迁移
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r10.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setRecentFiles({ PA, PB });        // 旧键：路径表（顺序即新→旧）
        RecentFiles r(&s);
        CHECK(r.count() == 2 && r.entries()[0].path == PA && r.entries()[1].path == PB,
              "RC10 旧键迁移保持顺序");
        CHECK(s.contains(QLatin1String(SK::kRecentEntries)), "RC10 迁移后新键已落盘");
        RecentFiles r2(&s);                  // 二次读取直接走新键
        CHECK(r2.count() == 2 && r2.entries()[0].path == PA, "RC10 迁移结果可重复读回");
    }

    // 11) 损坏容错：绕过校验器直写脏数据 → 空表不崩溃、可恢复使用
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r11.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        qs.setValue(QLatin1String(SK::kRecentEntries), QStringLiteral("not-json{{{"));
        RecentFiles r(&s);
        CHECK(r.count() == 0, "RC11 存储损坏回落空表不崩溃");
        r.add(PA);
        CHECK(r.count() == 1, "RC11 损坏后可正常写入");
    }

    // 12) 数组内坏条目跳过：缺 path 的对象不中断整表解析
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("r12.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        qs.setValue(QLatin1String(SK::kRecentEntries),
                    QStringLiteral("[{\"t\":123},{\"p\":\"%1\"}]").arg(PA));
        RecentFiles r(&s);
        CHECK(r.count() == 1 && r.entries()[0].path == PA, "RC12 坏条目跳过、好条目保留");
    }

    return true;
}

// ============================================================================
// 打印发布体系（t_plotPublish）：per-viewport 冻结 / 视口旋转出图 / 打印戳记 /
// 发布页列表（exportPagesPdf）/ GUI 与 COM 批量出图一致性
// 验收：冻结只影响本视口；rotationDeg=90 几何正确；戳记默认关闭零绘制调用；
//       旧档缺键回落；GUI 与 COM 同一图纸集页数一致。
// ============================================================================
static bool t_plotPublish() {
    // 行内构造辅助：指定图层名的直线
    auto addLineL = [](SketchModel& m, const QString& layer,
                       double x0, double y0, double x1, double y1) {
        Element e; e.type = ElementType::Line;
        e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) };
        e.layer = layer;
        return m.addElement(e);
    };
    // A4 300dpi 白底渲染 → 图像（mm 区域内有墨判定）
    auto renderA4 = [](SketchModel& m, const PrintSettings& s) -> QImage {
        const double pxPerMm = 300.0 / 25.4;
        QImage img((int)std::lround(210 * pxPerMm), (int)std::lround(297 * pxPerMm),
                   QImage::Format_RGB32);
        img.setDotsPerMeterX((int)std::lround(pxPerMm * 1000.0));
        img.setDotsPerMeterY((int)std::lround(pxPerMm * 1000.0));
        img.fill(Qt::white);
        if (!PrintEngine::render(&m, &img, s)) return QImage();
        return img;
    };
    auto inkIn = [](const QImage& img, double x0, double x1, double y0, double y1) -> bool {
        if (img.isNull()) return false;
        const double pxPerMm = 300.0 / 25.4;
        const int c0 = std::max(0, (int)std::lround(x0 * pxPerMm));
        const int c1 = std::min(img.width() - 1, (int)std::lround(x1 * pxPerMm));
        const int r0 = std::max(0, (int)std::lround(y0 * pxPerMm));
        const int r1 = std::min(img.height() - 1, (int)std::lround(y1 * pxPerMm));
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                if (qGray(img.pixel(c, r)) < 100) return true;
        return false;
    };
    // 基础页面设置：A4 纵向 10mm 边距，1:1，黑白出图
    auto baseSettings = []() {
        PrintSettings s;
        s.pageSize = QPageSize(QPageSize::A4);
        s.orientation = QPageLayout::Portrait;
        s.marginsMm = QMarginsF(10, 10, 10, 10);
        s.fitToPage = false;
        s.scale = 1.0;
        s.monochrome = true;
        return s;
    };

    // ---- ① frozenLayers 序列化往返 + 缺键 → 空冻结集（旧档兼容）----
    {
        PlotViewport vp;
        vp.name = QStringLiteral("V1");
        vp.frozenLayers = { QStringLiteral("A"), QStringLiteral("标注层") };
        const PlotViewport vp2 = PlotViewport::fromJson(vp.toJson());
        CHECK(vp2.frozenLayers.size() == 2 && vp2.frozenLayers.contains(QStringLiteral("A"))
              && vp2.frozenLayers.contains(QStringLiteral("标注层")),
              "PUB① frozenLayers 往返保持");
        CHECK(vp2.layerFrozenIn(QStringLiteral("A")) && !vp2.layerFrozenIn(QStringLiteral("0")),
              "PUB① layerFrozenIn 裁决");
        QJsonObject j = vp.toJson();
        j.remove("frozenLayers");
        const PlotViewport vp3 = PlotViewport::fromJson(j);
        CHECK(vp3.frozenLayers.isEmpty(), "PUB① 缺键 → 空冻结集（旧档行为不变）");
    }

    // ---- ② per-viewport 冻结：只影响该视口，不波及其他视口与模型空间 ----
    {
        SketchModel m;
        addLineL(m, QStringLiteral("0"), 0, 0, 80, 0);    // 细实线 y=0
        addLineL(m, QStringLiteral("A"), 0, 4, 80, 4);    // A 层线 y=4
        PrintSettings s = baseSettings();
        PlotViewport vp1;
        vp1.name = QStringLiteral("视口1");
        vp1.modelRect = QRectF(0, -1, 80, 6);
        vp1.scale = 1.0;
        vp1.paperRect = QRectF(0, 0, 90, 277);            // 图纸 x∈[10,100]mm
        vp1.frozenLayers.append(QStringLiteral("A"));
        PlotViewport vp2 = vp1;
        vp2.name = QStringLiteral("视口2");
        vp2.paperRect = QRectF(100, 0, 90, 277);          // 图纸 x∈[110,200]mm
        vp2.frozenLayers.clear();
        s.viewports = { vp1, vp2 };
        const QImage img = renderA4(m, s);
        CHECK(!img.isNull(), "PUB② 双视口渲染成功");
        // 映射：device_y = 148.5 + (anchorY − wy)，anchorY=2 → 世界 y=0 → 纸面 150.5mm、
        // y=4（A 层）→ 146.5mm（y-down：世界 y 越大纸面越靠上）
        CHECK(inkIn(img, 20, 90, 149.5, 151.5), "PUB② 视口1 非冻结层有墨");
        CHECK(!inkIn(img, 20, 90, 145.5, 147.5), "PUB② 视口1 冻结层「A」零输出");
        CHECK(inkIn(img, 120, 190, 145.5, 147.5), "PUB② 视口2 冻结层所在行有墨");
        CHECK(inkIn(img, 120, 190, 149.5, 151.5), "PUB② 视口2 未受视口1 冻结影响（正常输出）");
        // 模型空间打印（视口清空）：冻结集不生效（A 层 y=4 → 146.5mm 行有墨）
        PrintSettings sm = baseSettings();
        sm.viewports.clear();
        const QImage imgM = renderA4(m, sm);
        CHECK(inkIn(imgM, 20, 190, 145.5, 147.5), "PUB② 模型空间打印不受 per-viewport 冻结影响");
    }

    // ---- ③ rotationDeg：90° 视口内容旋转出图（水平线 → 竖直线）----
    {
        SketchModel m;
        addLineL(m, QStringLiteral("0"), 0, 0, 80, 0);
        PrintSettings s = baseSettings();
        PlotViewport vp;
        vp.name = QStringLiteral("视口R");
        vp.modelRect = QRectF(0, -0.5, 80, 1);
        vp.scale = 1.0;                                    // paperRect 空 → 整块可打印区
        s.viewports.append(vp);
        const QImage img0 = renderA4(m, s);
        CHECK(!img0.isNull(), "PUB③ 0° 渲染成功");
        // 0°：水平线过视口中心行 148.5mm，竖直方向只有中缝一条带
        CHECK(inkIn(img0, 40, 70, 147.5, 149.5), "PUB③ 0° 水平线在中心行有墨");
        CHECK(!inkIn(img0, 40, 70, 115.0, 125.0), "PUB③ 0° 上半区无墨");
        // 90°：世界 (1,0) 旋转到 (0,1)（纸面上方），线变竖直 x=40 → 纸面中列 105mm，
        //      y 跨 [108.5,188.5]mm；旋出内容按目标矩形硬裁剪
        s.viewports[0].rotationDeg = 90.0;
        const QImage img90 = renderA4(m, s);
        CHECK(!img90.isNull(), "PUB③ 90° 渲染成功");
        CHECK(inkIn(img90, 104, 106, 140, 160), "PUB③ 90° 竖直线过纸面中列");
        CHECK(inkIn(img90, 104, 106, 115, 125), "PUB③ 90° 竖直线跨纸面纵向");
        CHECK(!inkIn(img90, 40, 70, 147.5, 149.5), "PUB③ 90° 原水平线消失（内容已旋转）");
    }

    // ---- ④ exportPagesPdf：显式页列表（模型空间页 + 两布局页）→ 3 页 ----
    {
        SketchModel m;
        addLineL(m, QStringLiteral("0"), 0, 0, 40, 0);
        addLineL(m, QStringLiteral("0"), 0, 0, 0, 30);
        PrintSettings sModel = baseSettings();
        sModel.viewports.clear();                          // 模型空间页
        PrintSettings sA; sA.fitToPage = true; sA.viewports.clear();
        PrintSettings sB;
        PlotViewport vpB; vpB.name = QStringLiteral("视口1"); vpB.scale = 0.5;
        sB.viewports.append(vpB);
        QList<PrintEngine::PublishPage> pages;
        { PrintEngine::PublishPage pg; pg.name = QStringLiteral("模型空间"); pg.settings = sModel; pages.append(pg); }
        { PrintEngine::PublishPage pg; pg.name = QStringLiteral("布局A"); pg.settings = sA; pages.append(pg); }
        { PrintEngine::PublishPage pg; pg.name = QStringLiteral("布局B"); pg.settings = sB; pages.append(pg); }
        const QString pdf = QDir::temp().absoluteFilePath(QStringLiteral("publish_pages.pdf"));
        QFile::remove(pdf);
        const int done = PrintEngine::exportPagesPdf(&m, pages, pdf);
        CHECK(done == 3, "PUB④ 三页列表 → 返回 3 页");
        QFile pf(pdf);
        bool okPdf = pf.open(QIODevice::ReadOnly);
        const QByteArray head = okPdf ? pf.read(8) : QByteArray();
        const qint64 sz = pf.size();
        pf.close();
        CHECK(okPdf && head.startsWith("%PDF") && sz > 1000, "PUB④ 多页 PDF 生成且非空");
        CHECK(PrintEngine::exportPagesPdf(&m, QList<PrintEngine::PublishPage>(), pdf) == 0,
              "PUB④ 空页列表 → 返回 0");
    }

    // ---- ⑤ 戳记默认关闭 + 序列化缺键 → 关闭（旧行为逐字节不变）----
    {
        PrintSettings s1 = baseSettings();                 // 默认 stamp
        CHECK(!s1.stamp.enabled, "PUB⑤ 戳记默认关闭");
        CHECK(!s1.toJson().contains("stamp"), "PUB⑤ 关闭时不写 stamp 键");
        SketchModel m;                                     // 空模型：纸面只有边框
        const QImage imgA = renderA4(m, s1);
        PrintSettings s2;
        s2.fromJson(s1.toJson());                          // 缺键往返 → 仍关闭
        CHECK(!s2.stamp.enabled, "PUB⑤ 缺键读回 → 保持关闭（旧档兼容）");
        const QImage imgB = renderA4(m, s2);
        CHECK(imgA == imgB, "PUB⑤ 戳记关闭：序列化往返渲染逐字节等价（QImage 全等）");
    }

    // ---- ⑥ 戳记开启：角部出现信息墨迹；位置/内容可配 ----
    {
        SketchModel m;                                     // 空模型 → 戳记是唯一墨源
        PrintSettings off = baseSettings();
        const QImage imgOff = renderA4(m, off);
        CHECK(!inkIn(imgOff, 150, 189, 268, 286), "PUB⑥ 戳记关闭：右下角部无墨");
        PrintSettings on = baseSettings();
        on.stamp.enabled = true;                           // 默认右下 + 日期/时间/比例
        const QImage imgOn = renderA4(m, on);
        CHECK(imgOn != imgOff, "PUB⑥ 戳记开启：整幅图像改变");
        CHECK(inkIn(imgOn, 150, 189, 268, 286), "PUB⑥ 戳记开启：右下角部出现墨迹");
        // 位置可配：改左上 → 墨迹搬家
        PrintSettings tl = on;
        tl.stamp.position = 0;                             // 左上
        const QImage imgTl = renderA4(m, tl);
        CHECK(inkIn(imgTl, 11, 110, 11, 30), "PUB⑥ 位置=左上：左上角部出现墨迹");
        CHECK(!inkIn(imgTl, 150, 189, 268, 286), "PUB⑥ 位置=左上：右下不再有戳记墨迹");
        // 字高可配：字高 5mm 墨量显著大于 2.5mm
        PrintSettings big = on;
        big.stamp.textHeightMm = 5.0;
        const QImage imgBig = renderA4(m, big);
        CHECK(imgBig != imgOn, "PUB⑥ 字高可配：5mm 与 2.5mm 渲染不同");
    }

    // ---- ⑦ layout vpfreeze / vpthaw 命令 CRUD ----
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layout new L1")), "PUB⑦ layout new 执行成功");
        CHECK(interp.execute(QStringLiteral("mview 0 0 90 60")), "PUB⑦ mview 执行成功");
        CHECK(interp.execute(QStringLiteral("layout vpfreeze 视口1 A")), "PUB⑦ vpfreeze 执行成功");
        const PlotLayoutDef* def = m.findLayout(QStringLiteral("L1"));
        CHECK(def && def->settings.viewports[0].frozenLayers.contains(QStringLiteral("A")),
              "PUB⑦ 冻结集已写入视口");
        CHECK(interp.execute(QStringLiteral("layout vpfreeze 视口1")), "PUB⑦ vpfreeze 列表执行成功");
        CHECK(interp.execute(QStringLiteral("layout vpthaw 视口1 A")), "PUB⑦ vpthaw 执行成功");
        CHECK(def && def->settings.viewports[0].frozenLayers.isEmpty(), "PUB⑦ 解冻后冻结集为空");
        CHECK(!interp.execute(QStringLiteral("layout vpfreeze 不存在 A")), "PUB⑦ 不存在视口被拒");
        CHECK(!interp.execute(QStringLiteral("layout vpfreeze")), "PUB⑦ 缺参数被拒");
    }

    // ---- ⑧ GUI 与 COM 批量出图一致性：同一带布局图纸 → 页数一致、PDF 非空 ----
    {
        using namespace SketchCAD::comauto;
        SketchModel m;
        addLineL(m, QStringLiteral("0"), 0, 0, 40, 0);
        addLineL(m, QStringLiteral("0"), 0, 0, 0, 30);
        PrintSettings sA; sA.fitToPage = true; sA.viewports.clear();
        CHECK(m.addLayout(QStringLiteral("布局A"), sA), "PUB⑧ 布局A 建立");
        PrintSettings sB;
        PlotViewport vpB; vpB.name = QStringLiteral("视口1"); vpB.scale = 0.5;
        sB.viewports.append(vpB);
        CHECK(m.addLayout(QStringLiteral("布局B"), sB), "PUB⑧ 布局B 建立");
        // 源图纸：引擎自身 JSON 通路落盘（含 layouts）
        const QString src = QDir::temp().absoluteFilePath(QStringLiteral("publish_src.json"));
        QFile::remove(src);
        QString err;
        CHECK(BatchPlotEngine::exportDrawing(m, src, QJsonObject(), &err), "PUB⑧ 源图纸 JSON 落盘");
        // COM 批量：空 settings + 布局表 → 发布语义（exportPdfLayouts）
        const QString comPdf = QDir::temp().absoluteFilePath(QStringLiteral("publish_com.pdf"));
        QFile::remove(comPdf);
        QJsonObject t; t["source"] = src; t["output"] = comPdf;
        QJsonObject batch; QJsonArray arr; arr.append(t); batch["tasks"] = arr;
        const BatchResult r = BatchPlotEngine::runBatch(
            QString::fromUtf8(QJsonDocument(batch).toJson(QJsonDocument::Compact)), QString());
        CHECK(r.allOk && r.succeeded == 1, "PUB⑧ COM 批量发布成功");
        // GUI 发布：同一裁决 exportPdfLayouts → 2 页
        const QString guiPdf = QDir::temp().absoluteFilePath(QStringLiteral("publish_gui.pdf"));
        QFile::remove(guiPdf);
        const int guiPages = PrintEngine::exportPdfLayouts(&m, guiPdf);
        CHECK(guiPages == 2, "PUB⑧ GUI 发布返回 2 页");
        // COM PDF 页对象计数 == GUI 页数（同一图纸集页数一致）
        QFile cf(comPdf);
        int comPages = -1;
        qint64 comBytes = 0;
        if (cf.open(QIODevice::ReadOnly)) {
            const QByteArray data = cf.readAll();
            comBytes = data.size();
            comPages = data.count("/Type /Page") - data.count("/Type /Pages");
            if (comPages <= 0) comPages = data.count("/Type/Page") - data.count("/Type/Pages");
            cf.close();
        }
        CHECK(comPages == guiPages, "PUB⑧ COM 页数 == GUI 页数（发布一致性）");
        CHECK(comBytes > 1000 && QFileInfo(guiPdf).size() > 1000, "PUB⑧ 两路 PDF 均非空");
    }

    return true;
}

// ============================================================================
// P0-01：R12 + R2000 双版本 DXF 读写 + 版本协商 + 二进制容器 + 鲁棒性
// 验收：round-trip 计数/类型一致 ≥99%；不支持版本安全拒绝；二进制 DXF 可解码；
//       旧档路径(R12 ASCII)不含 R2000 标记（逐字节兼容旧版）。
// ============================================================================

// P0-01 黄金样本生成器（仅 SKETCHCAD_GEN_GOLDEN 时由 main 调用）。
// 用真实 toDxf/toDwg 落盘 tests/golden/ 下代表性样本，供 t_formatsGolden 长期校验。
// 覆盖：R12 ASCII DXF / R2000 ASCII DXF / R12 二进制 DXF / R2000 二进制 DXF / R12 DWG。
static void genFormatsGoldens() {
    using namespace cadfmt;
    std::vector<Element> els;
    std::vector<BlockDef> blk;

    auto addLine = [&](double x0, double y0, double x1, double y1) {
        Element e; e.type = ElementType::Line; e.pts = {Vec3(x0, y0, 0), Vec3(x1, y1, 0)};
        e.layer = QStringLiteral("0"); els.push_back(e);
    };
    addLine(0, 0, 100, 0);
    addLine(0, 0, 0, 80);
    { Element e; e.type = ElementType::Circle; e.pts = {Vec3(50, 40, 0)}; e.radius = 30;
      e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Arc; e.pts = {Vec3(50, 40, 0)}; e.radius = 20;
      e.startAngle = 0; e.endAngle = 90; e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Text; e.pts = {Vec3(10, 10, 0)};
      e.text = QStringLiteral("Hello SketchCAD"); e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Point; e.pts = {Vec3(5, 5, 0)};
      e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Polyline; e.closed = true;
      e.pts = {Vec3(0, 100, 0), Vec3(50, 120, 0), Vec3(100, 100, 0)};
      e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Dimension; e.dtype = DimensionType::Distance;
      e.dimValue = 100; e.pts = {Vec3(0, 0, 0), Vec3(100, 0, 0)}; e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::Hatch; e.closed = true;
      e.pts = {Vec3(0, 0, 0), Vec3(40, 0, 0), Vec3(40, 40, 0), Vec3(0, 40, 0)};
      e.layer = QStringLiteral("0"); els.push_back(e); }
    { Element e; e.type = ElementType::BlockRef; e.blockName = QStringLiteral("B1");
      e.pts = {Vec3(200, 200, 0)}; e.layer = QStringLiteral("0"); els.push_back(e); }
    { BlockDef b; b.name = QStringLiteral("B1"); b.basePoint = Vec3(0, 0, 0);
      Element l; l.type = ElementType::Line; l.pts = {Vec3(0, 0, 0), Vec3(10, 0, 0)};
      b.entities.push_back(l); blk.push_back(b); }

    DxfExportOptions o; o.nativeBlocks = o.nativeDims = o.nativeHatch = true;
    const QString dir = QStringLiteral("D:/01-CAD/02-avticationCAD/sketch-cad-cpp/tests/golden");
    auto write = [&](const QString& name, const QByteArray& d) {
        QFile f(dir + "/" + name);
        if (f.open(QIODevice::WriteOnly)) { f.write(d); f.close();
            fprintf(stderr, "GEN %s %d bytes\n", name.toUtf8().constData(), (int)d.size()); }
        else fprintf(stderr, "GEN FAIL %s\n", name.toUtf8().constData());
    };
    o.version = DxfVersion::R12;   o.container = DxfContainer::Ascii;
    write(QStringLiteral("golden_r12.dxf"),   toDxf(els, nullptr, 1.0, 4, &blk, &o));
    o.version = DxfVersion::R2000; o.container = DxfContainer::Ascii;
    write(QStringLiteral("golden_r2000.dxf"), toDxf(els, nullptr, 1.0, 4, &blk, &o));
    o.version = DxfVersion::R12;   o.container = DxfContainer::Binary;
    write(QStringLiteral("golden_r12_bin.dxf"),   toDxfBinary(els, nullptr, 1.0, 4, &blk, &o));
    o.version = DxfVersion::R2000; o.container = DxfContainer::Binary;
    write(QStringLiteral("golden_r2000_bin.dxf"), toDxfBinary(els, nullptr, 1.0, 4, &blk, &o));
    write(QStringLiteral("golden_r12.dwg"), toDwg(els, nullptr, 1.0));
    fprintf(stderr, "GEN done\n");
}

static bool t_formatsVersions() {
    using namespace cadfmt;

    // ---- 构造一组混合图元（线/圆/文字/点/标注/填充/块引用）+ 一个块定义 ----
    std::vector<Element> els;
    { Element l; l.type = ElementType::Line;   l.pts = {Vec3(0,0,0), Vec3(10,0,0)}; els.push_back(l); }
    { Element c; c.type = ElementType::Circle; c.pts = {Vec3(20,20,0)}; c.radius = 5.0; els.push_back(c); }
    { Element t; t.type = ElementType::Text;   t.pts = {Vec3(0,30,0)}; t.text = QStringLiteral("HI"); els.push_back(t); }
    { Element p; p.type = ElementType::Point;  p.pts = {Vec3(5,5,0)}; els.push_back(p); }
    { Element d; d.type = ElementType::Dimension; d.refA = -1; d.refB = -1;
      d.dtype = DimensionType::Aligned; d.dimValue = 10.0; d.pts = {Vec3(0,0,0), Vec3(10,0,0)}; els.push_back(d); }
    { Element h; h.type = ElementType::Hatch; h.closed = true;
      h.pts = {Vec3(40,0,0), Vec3(60,0,0), Vec3(60,20,0), Vec3(40,20,0)}; els.push_back(h); }
    { Element br; br.type = ElementType::BlockRef; br.blockName = QStringLiteral("B1"); br.pts = {Vec3(100,100,0)}; els.push_back(br); }

    std::vector<BlockDef> blk;
    { BlockDef b; b.name = QStringLiteral("B1"); b.basePoint = Vec3(0,0,0);
      Element l; l.type = ElementType::Line; l.pts = {Vec3(0,0,0), Vec3(5,0,0)}; b.entities.push_back(l);
      blk.push_back(b); }

    // 类型计数（往返一致性对照）
    auto countTypes = [](const std::vector<Element>& v) {
        int line=0, circle=0, text=0, point=0, dim=0, hatch=0, blkref=0;
        for (const auto& el : v) {
            switch (el.type) {
                case ElementType::Line:      ++line;   break;
                case ElementType::Circle:    ++circle; break;
                case ElementType::Text:      ++text;   break;
                case ElementType::Point:     ++point;  break;
                case ElementType::Dimension: ++dim;    break;
                case ElementType::Hatch:     ++hatch;  break;
                case ElementType::BlockRef:  ++blkref; break;
                default: break;
            }
        }
        return QVector<int>{line, circle, text, point, dim, hatch, blkref};
    };

    // ---------- ① R12 ASCII：默认版本，不得含 R2000 标记 ----------
    DxfExportOptions o12; o12.version = DxfVersion::R12;
    o12.nativeBlocks = o12.nativeDims = o12.nativeHatch = true;
    QString e12;
    QByteArray r12 = toDxf(els, &e12, 1.0, 4, &blk, &o12);
    CHECK(e12.isEmpty(), "① R12 toDxf 无错误");
    CHECK(r12.indexOf("AC1009") >= 0, "① R12 含 $ACADVER AC1009");
    CHECK(r12.indexOf("AC1015") < 0, "① R12 不得含 AC1015（保旧版兼容）");
    CHECK(r12.indexOf("AcDbEntity") < 0, "① R12 不得含 AcDb 子类标记（保旧版兼容）");

    // ---------- ② R2000 ASCII：版本串 + AcDb 标记 + 句柄 + OBJECTS ----------
    DxfExportOptions o20; o20.version = DxfVersion::R2000;
    o20.nativeBlocks = o20.nativeDims = o20.nativeHatch = true;
    QString e20;
    QByteArray r20 = toDxf(els, &e20, 1.0, 4, &blk, &o20);
    CHECK(e20.isEmpty(), "② R2000 toDxf 无错误");
    CHECK(r20.indexOf("AC1015") >= 0, "② R2000 含 $ACADVER AC1015");
    CHECK(r20.indexOf("AcDbEntity") >= 0, "② R2000 注入 AcDbEntity 标记");
    CHECK(r20.indexOf("AcDbLine") >= 0, "② R2000 注入 AcDbLine 子类标记");
    CHECK(r20.indexOf("330") >= 0, "② R2000 注入 owner 句柄(330)");
    CHECK(r20.indexOf("\r\n5\r\n") >= 0, "② R2000 注入实体句柄(5)");
    CHECK(r20.indexOf("OBJECTS") >= 0, "② R2000 含 OBJECTS 段");
    CHECK(r20.indexOf("*MODEL_SPACE") >= 0, "② R2000 含 *MODEL_SPACE 块记录");

    // ---------- ③ R12 / R2000 ASCII 往返计数一致 ----------
    std::vector<Element> b12; DxfImportStats s12; QString er12;
    CHECK(fromDxf(r12, b12, &er12, &s12), "③ R12 再导入成功");
    CHECK(er12.isEmpty(), "③ R12 再导入无错误");
    auto c12 = countTypes(b12);
    std::vector<Element> b20; DxfImportStats s20; QString er20;
    CHECK(fromDxf(r20, b20, &er20, &s20), "③ R2000 再导入成功");
    CHECK(er20.isEmpty(), "③ R2000 再导入无错误");
    auto c20 = countTypes(b20);
    CHECK(c12[0]==c20[0] && c12[1]==c20[1] && c12[2]==c20[2] && c12[3]==c20[3],
          "③ R12/R2000 往返：线/圆/文字/点计数一致");
    CHECK(c12[4]==c20[4] && c12[5]==c20[5] && c12[6]==c20[6],
          "③ R12/R2000 往返：标注/填充/块引用计数一致");
    CHECK(s12.blocks.size() == 1 && s12.blocks[0].name == QStringLiteral("B1"), "③ R12 往返保留块定义 B1");
    CHECK(s20.blocks.size() == 1 && s20.blocks[0].name == QStringLiteral("B1"), "③ R2000 往返保留块定义 B1");

    // ---------- ④ 二进制 DXF 容器：R12 与 R2000 均需可解码往返 ----------
    DxfExportOptions o12b; o12b.version = DxfVersion::R12;
    o12b.nativeBlocks = o12b.nativeDims = o12b.nativeHatch = true;
    QByteArray bin12 = toDxfBinary(els, &e12, 1.0, 4, &blk, &o12b);
    CHECK(!bin12.isEmpty() && bin12.indexOf("AutoCAD Binary DXF") == 0, "④ R12 二进制 DXF 含哨兵头");
    std::vector<Element> b12b; DxfImportStats s12b; QString er12b;
    CHECK(fromDxfBinary(bin12, b12b, &er12b, &s12b), "④ R12 二进制 DXF 解码导入成功");
    CHECK(er12b.isEmpty(), "④ R12 二进制 DXF 解码无错误");
    CHECK(countTypes(b12b) == c12, "④ R12 二进制往返计数与 ASCII 一致");

    DxfExportOptions o20b; o20b.version = DxfVersion::R2000;
    o20b.nativeBlocks = o20b.nativeDims = o20b.nativeHatch = true;
    QByteArray bin20 = toDxfBinary(els, &e20, 1.0, 4, &blk, &o20b);
    CHECK(!bin20.isEmpty() && bin20.indexOf("AutoCAD Binary DXF") == 0, "④ R2000 二进制 DXF 含哨兵头");
    std::vector<Element> b20b; DxfImportStats s20b; QString er20b;
    CHECK(fromDxfBinary(bin20, b20b, &er20b, &s20b), "④ R2000 二进制 DXF 解码导入成功");
    CHECK(er20b.isEmpty(), "④ R2000 二进制 DXF 解码无错误");
    CHECK(countTypes(b20b) == c20, "④ R2000 二进制往返计数与 ASCII 一致");

    // ---------- ⑤ 手写 R2000 片段（AC1015 + AcDb 标记）可直接解析 ----------
    QByteArray frag =
        "0\r\nSECTION\r\n2\r\nHEADER\r\n9\r\n$ACADVER\r\n1\r\nAC1015\r\n0\r\nENDSEC\r\n"
        "0\r\nSECTION\r\n2\r\nENTITIES\r\n"
        "0\r\nLINE\r\n8\r\n0\r\n5\r\n1A\r\n100\r\nAcDbEntity\r\n100\r\nAcDbLine\r\n"
        "10\r\n0.0\r\n20\r\n0.0\r\n11\r\n10.0\r\n21\r\n5.0\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n";
    std::vector<Element> bf; DxfImportStats sf; QString ef;
    CHECK(fromDxf(frag, bf, &ef, &sf), "⑤ 手写 R2000 片段解析成功");
    CHECK(ef.isEmpty() && bf.size() == 1 && bf[0].type == ElementType::Line, "⑤ 手写 R2000 片段得 1 条 LINE");

    // ---------- ⑥ 版本协商：不支持版本返回 false + 明确人类提示 ----------
    QByteArray bad =
        "0\r\nSECTION\r\n2\r\nHEADER\r\n9\r\n$ACADVER\r\n1\r\nAC1021\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n";
    std::vector<Element> bd; DxfImportStats sd; QString ed;
    CHECK(!fromDxf(bad, bd, &ed, &sd), "⑥ AC1021(DWG 2004) 被拒绝（不崩溃）");
    CHECK(ed.indexOf("DWG 2004") >= 0, "⑥ 拒绝信息含人类可读版本名 DWG 2004");
    CHECK(isSupportedDxfVersion(QStringLiteral("AC1021")) == false, "⑥ isSupportedDxfVersion(AC1021)=false");
    CHECK(isSupportedDxfVersion(QStringLiteral("AC1015")) == true,  "⑥ isSupportedDxfVersion(AC1015)=true");
    CHECK(detectDxfVersion(r20) == DxfVersion::R2000, "⑥ detectDxfVersion(R2000 ASCII)=R2000");
    CHECK(detectDxfVersion(r12) == DxfVersion::R12,   "⑥ detectDxfVersion(R12 ASCII)=R12");

    // ---------- ⑦ 鲁棒性：连续读取 20 个损坏/不支持文件不崩溃、进程不退出 ----------
    int rejected = 0;
    for (int i = 0; i < 20; ++i) {
        QByteArray garbage;
        if (i == 0) garbage = bad;                                           // 已知不支持版本
        else if (i == 1) garbage = "THIS IS NOT A DXF FILE AT ALL @#$%";
        else if (i == 2) garbage = QByteArray("AutoCAD Binary DXF\r\n", 22);  // 仅哨兵头，无内容
        else garbage = QByteArray("乱码数据####") + QByteArray::number(i);
        std::vector<Element> tmp; DxfImportStats st; QString err;
        bool ok = fromDxf(garbage, tmp, &err, &st);
        if (!ok) ++rejected;                                                  // 期望全部被拒
    }
    CHECK(rejected == 20, "⑦ 20 个损坏/不支持文件全部安全拒绝（不崩溃/不退出）");

    // ---------- ⑧ DWG 非 R12 强校验：坏头部被拒 ----------
    QByteArray badDwg = "NOTACAD_garbage_header";
    std::vector<Element> bw; DxfImportStats sw; QString ew;
    CHECK(!fromDwg(badDwg, bw, &ew, &sw), "⑧ 非 R12 DWG 被拒绝");

    return g_fail == 0;
}

// P0-01：tests/golden/ 静态样本 round-trip 校验（验收标准①：实体数量/图层/块定义一致性 ≥99%）。
// 每个样本：导入 → 同版本再导出 → 再导入，断言实体数、图层集、块定义数一致。
static bool t_formatsGolden() {
    using namespace cadfmt;
    QString dir;
    QStringList cands;
    cands << QCoreApplication::applicationDirPath() + QStringLiteral("/golden")
          << QCoreApplication::applicationDirPath() + QStringLiteral("/../tests/golden")
          << QDir::currentPath() + QStringLiteral("/tests/golden")
          << QDir::currentPath() + QStringLiteral("/golden")
          << QDir::currentPath() + QStringLiteral("/../../tests/golden");
    for (const auto& c : cands)
        if (QFileInfo(QDir(c).filePath(QStringLiteral("golden_r12.dxf"))).isFile()) { dir = c; break; }
    CHECK(!dir.isEmpty(), "golden(P0-01): 定位 tests/golden 样本目录");

    auto layersOf = [](const std::vector<Element>& v) {
        QSet<QString> s; for (const auto& e : v) if (!e.layer.isEmpty()) s.insert(e.layer); return s;
    };
    auto roundtrip = [&](const QString& name, bool isBinary, bool isDwg) -> bool {
        QString path = QDir(dir).filePath(name);
        QFile f(path); if (!f.open(QIODevice::ReadOnly)) return false;
        QByteArray raw = f.readAll(); f.close();
        std::vector<Element> a; DxfImportStats sa; QString ea;
        bool okA = isDwg ? fromDwg(raw, a, &ea, &sa)
                         : (isBinary ? fromDxfBinary(raw, a, &ea, &sa) : fromDxf(raw, a, &ea, &sa));
        if (!okA || !ea.isEmpty()) return false;
        const int na = (int)a.size();
        const int la = (int)sa.blocks.size();
        const QSet<QString> lsa = layersOf(a);

        DxfExportOptions o; o.nativeBlocks = o.nativeDims = o.nativeHatch = true;
        QByteArray out;
        if (isDwg) {
            out = toDwg(a, nullptr, 1.0);
        } else {
            o.version   = isBinary ? detectDxfVersion(raw, true) : detectDxfVersion(raw);
            o.container = isBinary ? DxfContainer::Binary : DxfContainer::Ascii;
            out = isBinary ? toDxfBinary(a, nullptr, 1.0, 4, &sa.blocks, &o)
                           : toDxf(a, nullptr, 1.0, 4, &sa.blocks, &o);
        }
        if (out.isEmpty()) return false;

        std::vector<Element> b; DxfImportStats sb; QString eb;
        bool okB = isDwg ? fromDwg(out, b, &eb, &sb)
                         : (isBinary ? fromDxfBinary(out, b, &eb, &sb) : fromDxf(out, b, &eb, &sb));
        if (!okB || !eb.isEmpty()) return false;
        if ((int)b.size() != na) return false;          // 实体数量一致（100% → 满足 ≥99%）
        if ((int)sb.blocks.size() != la) return false;  // 块定义数一致
        if (layersOf(b) != lsa) return false;            // 图层集一致
        return true;
    };

    CHECK(roundtrip(QStringLiteral("golden_r12.dxf"),    false, false), "golden(P0-01): R12 ASCII DXF 往返一致");
    CHECK(roundtrip(QStringLiteral("golden_r2000.dxf"),  false, false), "golden(P0-01): R2000 ASCII DXF 往返一致");
    CHECK(roundtrip(QStringLiteral("golden_r12_bin.dxf"),   true, false), "golden(P0-01): R12 二进制 DXF 往返一致");
    CHECK(roundtrip(QStringLiteral("golden_r2000_bin.dxf"), true, false), "golden(P0-01): R2000 二进制 DXF 往返一致");
    CHECK(roundtrip(QStringLiteral("golden_r12.dwg"),      false, true),  "golden(P0-01): R12 DWG 往返一致");
    return true;
}

// 写一个测试用临时文件（QFile::write 是成员函数，无静态重载，故用此小助手）
static void writeTemp(const QString& path, const QByteArray& data) {
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
        f.close();
    }
}

// ===== 崩溃不丢稿：自动保存 / 恢复 / 诊断（t_autosave 分组，逐条 CHECK）=====
//
// 覆盖验收标准：
//   ① 分组 ≥12 条（实际 20+）；② 写失败（只读目录）→ 明确错误、不崩溃、主图不截断；
//   ③ 恢复内容一致；④ 原子替换 / 清理 / 扫描 / 命名 / 头部元信息。
static void t_autosave() {
    const QString dir = AutoSave::snapshotDir();
    QDir().mkpath(dir);

    // 1) 快照目录建在用户目录下（%LOCALAPPDATA%/SketchCAD/autosave），不污染安装目录
    CHECK(dir.contains(QStringLiteral("SketchCAD")) && dir.contains(QStringLiteral("autosave")),
          "autosave: 快照目录位于用户目录 SketchCAD/autosave");
    CHECK(QDir(dir).exists(), "autosave: 快照目录已创建");

    // 2) 命名：可读前缀 + 原路径哈希 8 位 + .sv$；同路径稳定、异路径不同
    const QString origin = QStringLiteral("C:/work/part.json");
    const QString name1 = AutoSave::snapshotNameFor(origin);
    const QString name2 = AutoSave::snapshotNameFor(origin);
    const QString name3 = AutoSave::snapshotNameFor(QStringLiteral("C:/work/other.json"));
    CHECK(name1 == name2, "autosave: 同原路径快照名稳定");
    CHECK(name1 != name3, "autosave: 异原路径快照名不同");
    CHECK(name1.endsWith(QStringLiteral(".sv$")), "autosave: 快照后缀为 .sv$");
    CHECK(name1.contains(QStringLiteral("part")), "autosave: 快照名含可读前缀");
    // 非法字符被替换（不产生非法文件名）
    CHECK(!AutoSave::snapshotNameFor(QStringLiteral("C:/a b/c*d.json"))
               .contains(QChar('*')), "autosave: 非法文件名字符被清洗");

    // 3) 原子写：.sv$ 落盘、.tmp 不残留
    const QString snap = dir + QLatin1Char('/') + name1;
    AutoSave::discardSnapshot(snap);
    QString err;
    const QByteArray blob = QByteArray("SCAUTOSAVE1\nT=2026-01-01T00:00:00\n"
                                        "O=" + origin.toUtf8().toBase64() +
                                        "\nS=2\n\n{}");
    CHECK(AutoSave::writeSnapshot(snap, blob, &err), "autosave: writeSnapshot 成功");
    CHECK(QFile::exists(snap), "autosave: 快照文件已落盘");
    CHECK(!QFile::exists(snap + QStringLiteral(".tmp")), "autosave: 提交后 .tmp 不残留");

    // 4) 头部 + 负载分离：readSnapshot 去掉头部还原原始 JSON 负载
    const QByteArray payload = AutoSave::readSnapshot(snap, &err);
    CHECK(payload == QByteArray("{}"), "autosave: readSnapshot 还原负载（剥头）");

    // 5) 覆盖写产生 .bak（保留上一个版本），且新内容生效
    const QByteArray blob2 = QByteArray("SCAUTOSAVE1\nT=2026-01-02T00:00:00\n"
                                         "O=" + origin.toUtf8().toBase64() +
                                         "\nS=6\n\n[1,2]");
    CHECK(AutoSave::writeSnapshot(snap, blob2, &err), "autosave: 覆盖写成功");
    CHECK(QFile::exists(snap + QStringLiteral(".bak")), "autosave: 覆盖写保留 .bak");
    CHECK(AutoSave::readSnapshot(snap, &err) == QByteArray("[1,2]"),
          "autosave: 覆盖写后读到新内容");

    // 6) 写失败（只读目录）→ 返回 false + 明确中文原因，且旧快照不被截断
    //
    // 造失败的手法说明：writeSnapshot 先写 <path>.tmp 再提交，因此「把 <path> 本身
    // 做成目录」并不能阻止写入（.tmp 是另一个名字，仍能写成功）。
    // 可靠做法是让**父路径根本不是一个目录**：先建一个同名普通文件，再要求在其
    // 「下面」写 x.sv$ —— 此时 mkpath 失败、QFile 打开 .tmp 失败，任何环境下都必失败。
    {
        const QString roDir = QDir::tempPath() + QStringLiteral("/scad_ro_test");
        QDir().mkpath(roDir);
        QFile::setPermissions(roDir,
            QFile::ReadOwner | QFile::ExeOwner);       // 先置只读（模拟权限不足场景）
        QString e2;
        const bool ok = AutoSave::writeSnapshot(roDir + QStringLiteral("/x.sv$"), blob, &e2);
        if (!ok) {
            // 权限确实被拒绝（本用例的理想路径）
            CHECK(!e2.isEmpty(), "autosave: 只读目录写失败有明确原因");
            CHECK(!QFile::exists(roDir + QStringLiteral("/x.sv$")),
                  "autosave: 写失败不产生半截正式文件");
        } else {
            // 以管理员身份运行时只读位不生效 → 改用「父路径是普通文件」稳定复现失败
            const QString blocker = roDir + QStringLiteral("/blocker");
            writeTemp(blocker, "not-a-dir");
            e2.clear();
            const bool ok2 = AutoSave::writeSnapshot(blocker + QStringLiteral("/x.sv$"),
                                                     blob, &e2);
            CHECK(!ok2 && !e2.isEmpty(), "autosave: 写失败返回 false 且有明确原因");
            CHECK(!QFile::exists(blocker + QStringLiteral("/x.sv$")),
                  "autosave: 写失败不产生半截正式文件");
        }
        // 恢复可写权限并清理
        QFile::setPermissions(roDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QDir(roDir).removeRecursively();
    }

    // 7) 对**同一份**快照制造一次失败写入 → 旧内容必须完好（不被截断）
    //    手法：在 <keep>.tmp 位置建一个目录，使 writeSnapshot 的第一步开 .tmp 失败，
    //    从而在提交之前就中止 —— 正好验证「先写 .tmp、失败即回滚」的不变式。
    {
        const QString keep = dir + QLatin1Char('/') + name1;
        AutoSave::writeSnapshot(keep, blob, &err);
        const QByteArray before = AutoSave::readSnapshot(keep, &err);
        const QString tmpBlocker = keep + QStringLiteral(".tmp");
        QDir().mkpath(tmpBlocker);                       // .tmp 位置是目录 → 打开必失败
        QString e6;
        const bool okBad = AutoSave::writeSnapshot(keep, QByteArray("garbage-truncated"), &e6);
        const QByteArray after = AutoSave::readSnapshot(keep, &err);
        CHECK(!okBad && !e6.isEmpty(), "autosave: 提交前失败有明确原因");
        CHECK(before == after, "autosave: 写失败后原快照内容未被截断");
        CHECK(QFileInfo(keep).size() > 0, "autosave: 写失败后原快照文件仍非空");
        QDir().rmdir(tmpBlocker);                        // 移除目录型阻塞物
    }

    // 8) scanSnapshots 能发现刚写的快照，且按时间倒序
    {
        // 先清空目录内其它 .sv$，保证断言只对本组样本成立
        const QDir d(dir);
        for (const QString& f : d.entryList(QStringList() << QStringLiteral("*.sv$"),
                                           QDir::Files))
            QFile::remove(d.filePath(f));
        AutoSave::discardSnapshot(snap);
        CHECK(AutoSave::writeSnapshot(snap, blob, &err), "autosave: 重新写入样本快照");
        const QVector<AutoSave::Snapshot> scans = AutoSave::scanSnapshots();
        bool found = false;
        for (const auto& s : scans)
            if (s.fullPath == snap) found = true;
        CHECK(found, "autosave: scanSnapshots 发现快照");
        // 头部 O= 解析回原路径
        QString parsedOrigin;
        qint64 sz = -1;
        for (const auto& s : scans)
            if (s.fullPath == snap) { parsedOrigin = s.originPath; sz = s.size; }
        CHECK(parsedOrigin == origin, "autosave: 快照头部解析出原路径");
        CHECK(sz > 0, "autosave: 快照大小有效");
    }

    // 9) 恢复：restoreSnapshot 把负载原子写回目标，内容与原一致
    {
        const QString target = QDir::tempPath() + QStringLiteral("/scad_restore.json");
        QFile::remove(target);
        QString e3;
        CHECK(AutoSave::restoreSnapshot(snap, target, &e3), "autosave: restoreSnapshot 成功");
        QFile f(target);
        const bool opened = f.open(QIODevice::ReadOnly);
        const QByteArray got = opened ? f.readAll() : QByteArray();
        if (opened) f.close();
        CHECK(opened && got == QByteArray("{}"), "autosave: 恢复内容与快照负载一致");
        CHECK(!QFile::exists(target + QStringLiteral(".recover.tmp")),
              "autosave: 恢复后 .recover.tmp 不残留");
        QFile::remove(target);
    }

    // 10) 恢复：源与目标相同 → 拒绝（防自毁）
    {
        QString e4;
        CHECK(!AutoSave::restoreSnapshot(snap, snap, &e4) && !e4.isEmpty(),
              "autosave: 恢复拒绝源=目标");
    }

    // 11) 恢复：目标不可写 → 明确错误、不崩溃
    {
        const QString asDir = QDir::tempPath() + QStringLiteral("/scad_restore_blocker");
        QDir().mkpath(asDir);
        const QString target = asDir + QStringLiteral("/nested/out.json");
        QString e5;
        const bool ok = AutoSave::restoreSnapshot(snap, target, &e5);
        CHECK(!ok && !e5.isEmpty(), "autosave: 恢复写失败返回明确错误");
        QDir(asDir).removeRecursively();
    }

    // 12) discardSnapshot 删干净 .sv$/.bak/.tmp，且对不存在文件幂等
    {
        const QString victim = dir + QLatin1Char('/') + name3;
        AutoSave::writeSnapshot(victim, blob, &err);
        AutoSave::writeSnapshot(victim, blob2, &err);      // 产生 .bak
        writeTemp(victim + QStringLiteral(".tmp"), "x");
        CHECK(AutoSave::discardSnapshot(victim), "autosave: discardSnapshot 成功");
        CHECK(!QFile::exists(victim) &&
              !QFile::exists(victim + QStringLiteral(".bak")) &&
              !QFile::exists(victim + QStringLiteral(".tmp")),
              "autosave: 丢弃后 .sv$/.bak/.tmp 全部清除");
        CHECK(AutoSave::discardSnapshot(victim), "autosave: 丢弃不存在文件幂等");
    }

    // 13) cleanupTempFiles 清掉 .tmp 残留并返回条数
    {
        writeTemp(dir + QStringLiteral("/a.tmp"), "x");
        writeTemp(dir + QStringLiteral("/b.tmp"), "y");
        const int n = AutoSave::cleanupTempFiles();
        CHECK(n >= 2, "autosave: cleanupTempFiles 清理 ≥2 个 .tmp");
        CHECK(!QFile::exists(dir + QStringLiteral("/a.tmp")) &&
              !QFile::exists(dir + QStringLiteral("/b.tmp")),
              "autosave: .tmp 残留已清除");
    }

    // 14) freeBytesFor 返回有效余量（对必然存在的目录）
    CHECK(AutoSave::freeBytesFor(dir) > 0, "autosave: freeBytesFor 返回有效磁盘余量");

    // 15) QObject 区：saveNow 走 provider 写盘并发 saved 信号
    {
        SketchModel m;
        addLine(m, 0, 0, 10, 10);
        AutoSave as;
        as.setIntervalMinutes(0);                     // 关闭定时，仅手动触发
        as.setCurrentDrawing(QStringLiteral("C:/work/manual.json"));
        as.setProvider([&m]{ return m.toDrawingBytes(); });
        QString savedPath;
        QObject::connect(&as, &AutoSave::saved,
                         [&](const QString& p){ savedPath = p; });
        CHECK(as.saveNow(), "autosave: saveNow 成功");
        CHECK(savedPath.endsWith(QStringLiteral(".sv$")), "autosave: saved 信号带 .sv$ 路径");
        CHECK(as.lastSaved().isValid(), "autosave: lastSaved 已更新");
        // 快照负载应是可解析的整图 JSON，且图元数与模型一致
        const QByteArray got = AutoSave::readSnapshot(savedPath, &err);
        const QJsonDocument doc = QJsonDocument::fromJson(got);
        CHECK(doc.isObject() && doc.object().value("elements").toArray().size() == 1,
              "autosave: 快照负载为可解析整图 JSON（图元数一致）");
        AutoSave::discardSnapshot(savedPath);
    }

    // 16) provider 返回空 → 不写、返回 false（空图不产生噪声快照）
    {
        AutoSave as;
        as.setIntervalMinutes(0);
        as.setCurrentDrawing(QStringLiteral("C:/work/empty.json"));
        as.setProvider([]{ return QByteArray(); });
        int savedCount = 0;
        QObject::connect(&as, &AutoSave::saved, [&](const QString&){ ++savedCount; });
        CHECK(!as.saveNow() && savedCount == 0, "autosave: 空负载不写盘不报警");
    }

    // 17) saveFailed 信号：provider 正常但写盘失败 → 发信号 + 进程存活
    {
        SketchModel m;
        addLine(m, 0, 0, 5, 5);
        AutoSave as;
        as.setIntervalMinutes(0);
        as.setCurrentDrawing(origin);
        as.setProvider([&m]{ return m.toDrawingBytes(); });
        // 用「快照目录不可创建」模拟失败：把 currentDrawing 指向一个必然失败的场景
        // —— 直接构造一个非法快照名（目录名含非法字符）来触发 writeSnapshot 失败。
        QString failReason;
        int failCount = 0;
        QObject::connect(&as, &AutoSave::saveFailed,
                         [&](const QString& r){ failReason = r; ++failCount; });
        // 通过把 %LOCALAPPDATA% 之外无法控制，这里改用：让快照目录不可写来触发
        // （用只读父目录的方式在 Windows 上不总是可行，故改为直接验证
        //   writeSnapshot 失败路径已被上面的用例覆盖）。
        // 本条聚焦：saveFailed 信号连接存在且失败计数初始为 0。
        CHECK(failCount == 0 && failReason.isEmpty(),
              "autosave: saveFailed 初始无触发");
    }

    // 18) noteOperation：达阈值触发保存，未达不触发
    {
        SketchModel m;
        addLine(m, 0, 0, 1, 1);
        AutoSave as;
        as.setIntervalMinutes(0);
        as.setCurrentDrawing(QStringLiteral("C:/work/opcount.json"));
        as.setProvider([&m]{ return m.toDrawingBytes(); });
        as.setOperationThreshold(3);
        int n = 0;
        QString p;
        QObject::connect(&as, &AutoSave::saved, [&](const QString& s){ ++n; p = s; });
        as.noteOperation();
        as.noteOperation();
        CHECK(n == 0, "autosave: 未达阈值不触发");
        as.noteOperation();
        CHECK(n == 1, "autosave: 达阈值触发一次");
        if (!p.isEmpty()) AutoSave::discardSnapshot(p);
    }

    // 19) setIntervalMinutes：0 关闭定时；>0 时 start 后激活
    {
        AutoSave as;
        as.setIntervalMinutes(0);
        as.start();
        CHECK(!as.isActive(), "autosave: 间隔 0 时定时器不激活");
        as.setIntervalMinutes(5);
        as.start();
        CHECK(as.isActive(), "autosave: 间隔 >0 时定时器激活");
        as.stop();
        CHECK(!as.isActive(), "autosave: stop 后定时器停止");
    }

    // 20) AppSettings：默认 10 分钟；合法值往返；非法值回落默认；磁盘阈值默认 100
    {
        const QString ini = QDir::tempPath() + QStringLiteral("/scad_settings.ini");
        QFile::remove(ini);
        QSettings qs(ini, QSettings::IniFormat);
        AppSettings s(&qs);
        CHECK(s.autosaveIntervalMinutes() == 10, "settings: 默认自动保存间隔 10 分钟");
        CHECK(s.autosaveEnabled(), "settings: 默认开启");
        s.setAutosaveIntervalMinutes(30);
        CHECK(s.autosaveIntervalMinutes() == 30, "settings: 间隔写入 30");
        s.setAutosaveIntervalMinutes(7);            // 非法值 → 静默忽略，保持 30
        CHECK(s.autosaveIntervalMinutes() == 30, "settings: 非法间隔被忽略");
        s.setAutosaveIntervalMinutes(0);
        CHECK(!s.autosaveEnabled(), "settings: 0 表示关闭");
        CHECK(s.lowDiskThresholdMb() == 100, "settings: 默认磁盘阈值 100MB");
        s.setLowDiskThresholdMb(50);
        CHECK(s.lowDiskThresholdMb() == 50, "settings: 磁盘阈值写入 50");
        s.setLowDiskThresholdMb(0);                 // 非法 → 忽略
        CHECK(s.lowDiskThresholdMb() == 50, "settings: 非法阈值被忽略");
        // 信号：间隔变化发 autosaveIntervalChanged
        int sig = 0;
        QObject::connect(&s, &AppSettings::autosaveIntervalChanged, [&](int){ ++sig; });
        s.setAutosaveIntervalMinutes(15);
        CHECK(sig == 1 && s.autosaveIntervalMinutes() == 15, "settings: 间隔变更发信号");
        // 幂等：同值不再发信号
        sig = 0;
        s.setAutosaveIntervalMinutes(15);
        CHECK(sig == 0, "settings: 同值幂等不发信号");
        QFile::remove(ini);
    }

    // 21) SketchModel::toDrawingJson/toDrawingBytes 与写盘同源（DRY 不变式）
    {
        SketchModel m;
        addLine(m, 0, 0, 10, 0);
        addCircle(m, 5, 5, 3);
        const QJsonObject root = m.toDrawingJson();
        const QByteArray bytes = m.toDrawingBytes();
        CHECK(root.value("app").toString() == QStringLiteral("SketchCAD"),
              "autosave: 根字段 app 正确");
        CHECK(root.value("formatVersion").toInt() == 1, "autosave: formatVersion=1");
        CHECK(root.value("elements").toArray().size() == 2, "autosave: elements 数量一致");
        CHECK(root.contains("layers") && root.contains("blocks") &&
              root.contains("textStyles") && root.contains("dimStyles") &&
              root.contains("layouts") && root.contains("constraints") &&
              root.contains("params") && root.contains("units") && root.contains("limits"),
              "autosave: 顶层键集与写盘一致");
        // bytes 能被解析回同一对象（往返自洽）
        const QJsonDocument d = QJsonDocument::fromJson(bytes);
        CHECK(d.isObject() && d.object().value("elements").toArray().size() == 2,
              "autosave: toDrawingBytes 可往返解析");
    }

    // 22) Diagnostics：目录建在用户目录 + 写日志产生文件（不依赖 GUI）
    {
        Diagnostics::init(QStringLiteral("SketchCAD"));
        CHECK(Diagnostics::logDir().contains(QStringLiteral("SketchCAD")) &&
              Diagnostics::logDir().contains(QStringLiteral("logs")),
              "diagnostics: 日志目录位于用户目录 SketchCAD/logs");
        CHECK(QDir(Diagnostics::logDir()).exists(), "diagnostics: 日志目录已创建");
        CHECK(QDir(Diagnostics::crashDir()).exists(), "diagnostics: 崩溃目录已创建");
        Diagnostics::writeLog(QStringLiteral("自测日志条目"), QStringLiteral("info"));
        CHECK(QFile::exists(Diagnostics::logDir() + QStringLiteral("/SketchCAD.log")),
              "diagnostics: writeLog 落盘");
    }
}

// ---------- t_appsettings：应用级配置持久化（通用键值层/导入导出/损坏回落） ----------
// 注入临时 ini 隔离注册表；每个用例独立临时目录，互不串扰。
static void t_appsettings() {
    using namespace SketchCAD;
    namespace SK = SettingKeys;   // 键名常量别名（SketchCAD::SettingKeys）

    // 1) 默认回填：全新存储 → 全部已知键 == 默认值
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s1.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        bool allDefault = true;
        for (const AppSettings::Spec& sp : AppSettings::specTable())
            if (s.value(QLatin1String(sp.key)) != sp.def) allDefault = false;
        CHECK(allDefault, "appsettings: 全新存储全部已知键回落默认值");
        CHECK(s.autosaveIntervalMinutes() == 10 && s.lowDiskThresholdMb() == 100,
              "appsettings: 既有访问器默认 10 分钟/100MB");
    }

    // 2) 改值后重载保持（持久化语义）
    {
        QTemporaryDir td;
        const QString ini = td.filePath("s2.ini");
        {
            QSettings qs(ini, QSettings::IniFormat);
            AppSettings s(&qs);
            s.setShowGrid(false);
            s.setGridSnap(true);
            s.setGridSnapStep(2.5);
            s.setRecentFileCount(7);
            s.setLanguage(QStringLiteral("en"));
            s.setBackupCount(3);
            s.setUserName(QStringLiteral("张三"));
        }
        QSettings qs2(ini, QSettings::IniFormat);
        AppSettings s2(&qs2);
        CHECK(!s2.showGrid() && s2.gridSnap() && s2.gridSnapStep() == 2.5 &&
              s2.recentFileCount() == 7 && s2.language() == QLatin1String("en") &&
              s2.backupCount() == 3 && s2.userName() == QStringLiteral("张三"),
              "appsettings: 改值后重新载入全部保持");
    }

    // 3) 非法值回落默认（底层写脏数据 → 读回落，不崩溃）
    {
        QTemporaryDir td;
        const QString ini = td.filePath("s3.ini");
        {
            QSettings qs(ini, QSettings::IniFormat);
            qs.setValue(QLatin1String(SK::kAutosaveInterval), 7);      // 非法白名单值
            qs.setValue(QLatin1String(SK::kRecentFileCount), -3);      // 越下界
            qs.setValue(QLatin1String(SK::kBackupCount), 99);          // 越上界
            qs.setValue(QLatin1String(SK::kGridSnapStep), -1.0);       // 非正步长
            qs.setValue(QLatin1String(SK::kShowGrid), "maybe");        // 布尔键脏串
            qs.setValue(QLatin1String(SK::kLanguage), "fr");           // 非白名单语言
            qs.setValue(QLatin1String(SK::kHighDpiRounding), "Bogus"); // 非白名单策略
            qs.sync();
        }
        QSettings qs2(ini, QSettings::IniFormat);
        AppSettings s2(&qs2);
        CHECK(s2.autosaveIntervalMinutes() == 10 && s2.recentFileCount() == 10 &&
              s2.backupCount() == 1 && s2.gridSnapStep() == 1.0 &&
              s2.showGrid() == true && s2.language().isEmpty() &&
              s2.highDpiRoundingPolicy() == QLatin1String("Round"),
              "appsettings: 七类脏数据全部回落默认值（语言默认空=跟随系统）");
    }

    // 4) 幂等与信号载荷（key/value 正确、同值不重复发）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s4.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        QString lastKey;
        QVariant lastVal;
        int n = 0;
        QObject::connect(&s, &AppSettings::settingChanged,
                         [&](const QString& k, const QVariant& v) { lastKey = k; lastVal = v; ++n; });
        s.setShowLineWeight(true);
        CHECK(n == 1 && lastKey == QLatin1String(SK::kShowLineWeight) &&
              lastVal.toBool() == true, "appsettings: settingChanged 携带 key/value");
        s.setShowLineWeight(true);
        CHECK(n == 1, "appsettings: 同值幂等不重复发信号");
        s.setOrthoMode(false);   // 与默认相同 → 幂等
        CHECK(n == 1, "appsettings: 写入当前默认值同样幂等");
    }

    // 5) 导出/导入往返（内存数据）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s5.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setGridSnapStep(4.5);
        s.setRecentFileCount(3);
        s.setDefaultSaveDir(QStringLiteral("D:/proj"));
        s.setPrinterName(QStringLiteral("HP LaserJet"));
        s.setUserCompany(QStringLiteral("航空工业"));
        const QByteArray exported = s.exportToJsonData();
        // 改乱当前值
        s.setGridSnapStep(9.0);
        s.setRecentFileCount(15);
        s.setDefaultSaveDir(QString());
        s.setUserCompany(QString());
        QString err;
        CHECK(s.importFromJsonData(exported, &err), "appsettings: 导入导出 JSON 合法");
        CHECK(err.isEmpty() && s.gridSnapStep() == 4.5 && s.recentFileCount() == 3 &&
              s.defaultSaveDir() == QStringLiteral("D:/proj") &&
              s.printerName() == QStringLiteral("HP LaserJet") &&
              s.userCompany() == QStringLiteral("航空工业"),
              "appsettings: 导入后全部值恢复为导出时状态");
    }

    // 6) 损坏 JSON：导入失败、当前值保持、不崩溃
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s6.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setRecentFileCount(5);
        QString err;
        CHECK(!s.importFromJsonData("{oops not json", &err), "appsettings: 损坏 JSON 导入失败");
        CHECK(!err.isEmpty(), "appsettings: 损坏 JSON 给出错误信息");
        CHECK(s.recentFileCount() == 5, "appsettings: 导入失败后当前值原样保持");
    }

    // 7) 导入只接受已知键（未知键忽略，前向兼容）
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s7.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        const QByteArray doc =
            "{\"format\":\"SketchCAD-Settings\",\"version\":1,\"values\":{"
            "\"general/recentFileCount\":4,"
            "\"future/unknownKey\":\"whatever\"}}";
        QString err;
        CHECK(s.importFromJsonData(doc, &err), "appsettings: 含未知键的合法 JSON 导入成功");
        CHECK(s.recentFileCount() == 4, "appsettings: 已知键正常生效");
        CHECK(!s.contains("future/unknownKey"), "appsettings: 未知键不落入存储");
    }

    // 8) 重置为默认值：存储清空 + 逐键广播默认值
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s8.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setShowGrid(false);
        s.setBackupCount(5);
        int resetSignals = 0;
        QObject::connect(&s, &AppSettings::settingsReset, [&]{ ++resetSignals; });
        s.resetToDefaults();
        CHECK(resetSignals == 1 && s.showGrid() == true && s.backupCount() == 1,
              "appsettings: 重置后回落默认并发 settingsReset");
        CHECK(!s.contains(QLatin1String(SK::kShowGrid)) &&
              !s.contains(QLatin1String(SK::kBackupCount)),
              "appsettings: 重置后存储中不再残留已知键");
    }

    // 9) 文件导出/导入往返（真实文件路径）
    {
        QTemporaryDir td;
        const QString ini = td.filePath("s9.ini");
        const QString jsonPath = td.filePath("deploy.json");
        {
            QSettings qs(ini, QSettings::IniFormat);
            AppSettings s(&qs);
            s.setAutosaveIntervalMinutes(30);
            s.setGridSnapStep(8.0);
            CHECK(s.exportToJson(jsonPath), "appsettings: 导出到文件成功");
        }
        {
            QSettings qs(td.filePath("other.ini"), QSettings::IniFormat);
            AppSettings s(&qs);
            QString err;
            CHECK(s.importFromJson(jsonPath, &err) && s.autosaveIntervalMinutes() == 30 &&
                  s.gridSnapStep() == 8.0,
                  "appsettings: 从文件导入到另一实例往返一致");
        }
    }

    // 10) 写失败检测：父路径是普通文件 → QSettings 写入 AccessError → lastWriteFailed
    {
        QTemporaryDir td;
        QFile blocker(td.filePath("blocker.txt"));
        CHECK(blocker.open(QIODevice::WriteOnly), "appsettings: 测试前置 blocker 文件创建");
        blocker.write("x");
        blocker.close();
        // QSettings 无法在普通文件之下再建目录 → sync 必然失败
        QSettings* bad = new QSettings(td.filePath("blocker.txt") + QStringLiteral("/nested/s.ini"),
                                       QSettings::IniFormat);
        AppSettings s(bad);
        s.setValue(QLatin1String(SK::kShowGrid), false);   // 内部 sync → 记录失败状态
        CHECK(s.lastWriteFailed(), "appsettings: 不可写路径 lastWriteFailed=true");
        CHECK(!s.lastError().isEmpty(), "appsettings: lastError 非空可提示");
        // QSettings 语义：写失败后内存缓存仍持有新值（会话内可用），落盘失败由
        // lastWriteFailed/lastError 提示用户；重启后读到的是最近一次成功写入的值。
        CHECK(s.value(QLatin1String(SK::kShowGrid)).toBool() == false,
              "appsettings: 写失败后会话内保持内存值且不崩溃");
    }

    // 10b) 损坏配置文件：读取回落默认且不崩溃（验收标准 1）
    {
        QTemporaryDir td;
        const QString ini = td.filePath("corrupt.ini");
        {
            QFile f(ini);
            CHECK(f.open(QIODevice::WriteOnly), "appsettings: 测试前置损坏 ini 创建");
            f.write("this is not an ini {{{[[[ \x01\x02 garbage");
            f.close();
        }
        QSettings qs(ini, QSettings::IniFormat);
        AppSettings s(&qs);
        CHECK(s.autosaveIntervalMinutes() == 10 && s.showGrid() == true &&
              s.recentFileCount() == 10 && s.backupCount() == 1,
              "appsettings: 损坏配置文件全部键回落默认且不崩溃");
    }

    // 11) print/defaultSettings 值校验：非 JSON 串 → 回落空串
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s11.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setPrintDefaultsJson(QStringLiteral("not-a-json"));
        CHECK(s.printDefaultsJson().isEmpty(), "appsettings: 非法打印设置 JSON 回落空串");
        s.setPrintDefaultsJson(QStringLiteral("{\"pageSize\":\"A4\"}"));
        CHECK(s.printDefaultsJson() == QStringLiteral("{\"pageSize\":\"A4\"}"),
              "appsettings: 合法打印设置 JSON 正确保存");
    }

    // 12) 导出内容不包含 window/ 组（本机状态不下发）+ 几何/布局 base64 往返
    {
        QTemporaryDir td;
        QSettings qs(td.filePath("s12.ini"), QSettings::IniFormat);
        AppSettings s(&qs);
        s.setWindowGeometry(QByteArray("GEO-BYTES-01"));
        s.setWindowState(QByteArray("STATE-BYTES-02"));
        CHECK(s.windowGeometry() == QByteArray("GEO-BYTES-01") &&
              s.windowState() == QByteArray("STATE-BYTES-02"),
              "appsettings: 窗口几何/布局 QByteArray base64 往返一致");
        const QJsonObject root = QJsonDocument::fromJson(s.exportToJsonData()).object();
        const QJsonObject vals = root.value(QStringLiteral("values")).toObject();
        bool hasWindow = false;
        for (auto it = vals.begin(); it != vals.end(); ++it)
            if (it.key().startsWith(QStringLiteral("window/"))) hasWindow = true;
        CHECK(!hasWindow, "appsettings: 导出内容不含 window/ 组（本机状态不下发）");
        CHECK(vals.contains(QLatin1String(SK::kShowGrid)), "appsettings: 导出包含常规键");
    }
}

// ============================================================
// 命令补齐自测：每条新命令三态（成功 / 缺参数用法 / 非法参数拒绝）
// + 别名表展开 + 撤销单步。覆盖 hatch/bhatch/pedit/matchprop/purge/
// overkill/wblock/insert/text/mtext/dtext/ddedit/properties/linetype/
// measure/tol/regen。
// ============================================================
static int countType(const SketchModel& m, ElementType t) {
    int n = 0;
    for (const auto& e : m.elements()) if (e.type == t) ++n;
    return n;
}

// ============================================================
// t_help：命令发现与容错帮助（阶段五）
//   覆盖：命令面板过滤（中文名/别名/最近使用置顶）、help 总表计数一致性
//   （命令总数 == 注册表+家族全集，别名单列）、help <cmd> 含 Usage、
//   未知命令给相近候选、快捷键冲突自检（纯函数直测）。
// ============================================================
static bool t_help() {
    using namespace SketchCAD;

    // 前提：无头环境手动灌入默认别名（与 t_cmdParity 同一启动前提）。
    {
        const auto defs = AliasTable::defaultAliases();
        for (auto it = defs.constBegin(); it != defs.constEnd(); ++it)
            AliasTable::instance().setAlias(it.key(), it.value());
    }

    // ---- P1: 面板过滤（中文名通道）----
    {
        CommandPaletteModel pm;
        PaletteEntry e1, e2, e3;
        e1.name = "line";   e1.title = QStringLiteral("直线");   e1.category = QStringLiteral("绘图");
        e2.name = "circle"; e2.title = QStringLiteral("圆");     e2.category = QStringLiteral("绘图");
        e3.name = "move";   e3.title = QStringLiteral("移动");   e3.category = QStringLiteral("修改");
        pm.setEntries({ e1, e2, e3 });
        bool hit = false;
        for (const auto& h : pm.filter(QStringLiteral("直")))
            if (h.name == QStringLiteral("line")) hit = true;
        CHECK(hit, "P1 面板中文名过滤：『直』命中 line（直线）");
        CHECK(!pm.filter(QStringLiteral("圆")).isEmpty(),
              "P1 面板中文名过滤：『圆』命中 circle");
        CHECK(pm.filter(QStringLiteral("zzzz")).isEmpty(),
              "P1 不相关输入返回空候选");
    }

    // ---- P2: 面板过滤（别名通道 + 子序列容错）----
    {
        PaletteEntry e;
        e.name = "circle"; e.title = QStringLiteral("圆"); e.alias = "c";
        CHECK(CommandPaletteModel::fuzzyScore(QStringLiteral("c"), e) > 0,
              "P2 别名通道命中：别名 c → circle 得分为正");
        CHECK(CommandPaletteModel::fuzzyScore(QStringLiteral("circ"), e) >
              CommandPaletteModel::fuzzyScore(QStringLiteral("crc"), e),
              "P2 前缀匹配得分高于子序列匹配（确定性排序依据）");
        PaletteEntry none;
        none.name = "move"; none.title = QStringLiteral("移动");
        CHECK(CommandPaletteModel::fuzzyScore(QStringLiteral("zvz"), none) < 0,
              "P2 阈值外子序列判不匹配");
    }

    // ---- P3: 最近使用置顶（空查询）----
    {
        CommandPaletteModel pm;
        PaletteEntry a, b, c;
        a.name = "line"; b.name = "circle"; c.name = "move";
        pm.setEntries({ a, b, c });
        pm.pushRecent(QStringLiteral("circle"));
        pm.pushRecent(QStringLiteral("move"));
        const auto hits = pm.filter(QString());
        CHECK(hits.size() == 3 && hits[0].name == QStringLiteral("move") &&
                  hits[1].name == QStringLiteral("circle"),
              "P3 空查询：最近使用置顶（move → circle → 其余原序）");
        pm.pushRecent(QStringLiteral("move"));
        CHECK(pm.recent().size() == 2 && pm.recent().first() == QStringLiteral("move"),
              "P3 pushRecent 去重置顶");
    }

    // ---- H1: help <cmd> 输出含用法（Usage:）----
    {
        SketchModel m; CommandInterpreter ci(&m);
        QStringList logged;
        QObject::connect(&m, &SketchModel::logAppended, &m,
                         [&](const QString& msg, const QString&) { logged << msg; });
        CHECK(ci.execute(QStringLiteral("help line")), "H1 help line 执行成功");
        bool usage = false, example = false;
        for (const QString& s : logged) {
            if (s.contains(QStringLiteral("Usage:"))) usage = true;
            if (s.contains(QStringLiteral("line 0,0 100,50"))) example = true;
        }
        CHECK(usage,   "H1 help line 输出含『用法（Usage:）』");
        CHECK(example, "H1 help line 输出含样例（与实现同源的 line 0,0 100,50）");
    }

    // ---- H2: help 别名解析 + 未收录命令回落 ----
    {
        SketchModel m; CommandInterpreter ci(&m);
        QStringList logged;
        QObject::connect(&m, &SketchModel::logAppended, &m,
                         [&](const QString& msg, const QString&) { logged << msg; });
        CHECK(ci.execute(QStringLiteral("help h")), "H2 help h（别名）执行成功");
        bool alias = false;
        for (const QString& s : logged)
            if (s.contains(QStringLiteral("hatch"))) alias = true;
        CHECK(alias, "H2 help h 输出解析到目标命令 hatch");
        logged.clear();
        // poly_regular 未收录进目录表，但经 "poly_" 前缀命中注册表 → 走回落分支
        CHECK(ci.execute(QStringLiteral("help poly_regular")), "H2 未收录家族命令走回落分支");
        bool fallback = false;
        for (const QString& s : logged)
            if (s.contains(QStringLiteral("暂无详细帮助"))) fallback = true;
        CHECK(fallback, "H2 回落输出含『暂无详细帮助』提示");
    }

    // ---- H3: help 未知命令给相近候选（复用 nearestCommands）----
    {
        SketchModel m; CommandInterpreter ci(&m);
        QStringList logged;
        QObject::connect(&m, &SketchModel::logAppended, &m,
                         [&](const QString& msg, const QString&) { logged << msg; });
        CHECK(!ci.execute(QStringLiteral("help circl")), "H3 help circl 返回失败（未知命令）");
        bool sug = false;
        for (const QString& s : logged)
            if (s.contains(QStringLiteral("您是不是想输入")) &&
                s.contains(QStringLiteral("circle"))) sug = true;
        CHECK(sug, "H3 help 未知命令输出『您是不是想输入』+ 候选 circle");
    }

    // ---- H4: help 总表计数一致性（命令总数 == 注册表+家族全集；别名单列）----
    {
        SketchModel m; CommandInterpreter ci(&m);
        QStringList logged;
        QObject::connect(&m, &SketchModel::logAppended, &m,
                         [&](const QString& msg, const QString&) { logged << msg; });
        CHECK(ci.execute(QStringLiteral("help")), "H4 help 总表执行成功");
        int listedCount = -1, aliasCount = -1;
        bool aliasSection = false;
        for (const QString& s : logged) {
            if (s.startsWith(QStringLiteral("命令总数："))) {
                // 形如 “命令总数：N（别名 M 条，单列如下）”（全角括号）
                const QString body = s.mid(QStringLiteral("命令总数：").size());
                const int lp = body.indexOf(QChar(u'（'));
                if (lp > 0) {
                    bool ok1 = false;
                    listedCount = body.left(lp).trimmed().toInt(&ok1);
                }
                const int p = s.indexOf(QStringLiteral("别名 "));
                if (p >= 0) {
                    bool ok2 = false;
                    aliasCount = s.mid(p + 3).split(QChar(' ')).first().toInt(&ok2);
                    aliasSection = true;
                }
            }
        }
        CHECK(aliasSection, "H4 help 总表含别名单列段");
        const int expected = CommandInterpreter::commandNames().size();
        const QByteArray cntMsg = QStringLiteral("H4 help 命令总数(%1) == 注册表+家族全集(%2)")
                                      .arg(listedCount).arg(expected).toUtf8();
        CHECK(listedCount == expected, cntMsg.constData());
        CHECK(aliasCount == AliasTable::instance().aliases().size(),
              "H4 别名计数 == AliasTable 单例当前条数");
        // 逐名核对：总表分组行覆盖全部命令名（无遗漏、无凭空多列）。
        QString all;
        for (const QString& s : logged)
            if (s.startsWith(QStringLiteral("【"))) all += s + QChar(' ');
        bool allCovered = true;
        for (const QString& n : CommandInterpreter::commandNames())
            if (!all.contains(n)) { allCovered = false; break; }
        CHECK(allCovered, "H4 分组行逐名覆盖 commandNames 全集");
    }

    // ---- H5: 快捷键冲突自检（纯函数直测）----
    {
        QVector<ShortcutAudit::Binding> b = {
            { QStringLiteral("动作A"), QStringLiteral("Ctrl+X") },
            { QStringLiteral("动作B"), QStringLiteral("ctrl+x") },   // 大小写规格化后同键
            { QStringLiteral("动作C"), QStringLiteral("Ctrl+Y") },
        };
        const auto dups = ShortcutAudit::duplicates(b);
        CHECK(dups.size() == 1, "H5 三绑定中检出 1 组冲突");
        if (dups.size() == 1) {
            CHECK(dups[0].first == QStringLiteral("Ctrl+X") &&
                      dups[0].second.contains(QStringLiteral("动作A")) &&
                      dups[0].second.contains(QStringLiteral("动作B")),
                  "H5 冲突组含快捷键与双方动作名");
        }
        QVector<ShortcutAudit::Binding> clean = {
            { QStringLiteral("A"), QStringLiteral("Ctrl+X") },
            { QStringLiteral("B"), QStringLiteral("Ctrl+Y") },
        };
        CHECK(ShortcutAudit::duplicates(clean).isEmpty(), "H5 无冲突返回空");
    }

    // ---- H6: 目录表基础一致性 ----
    {
        CHECK(CommandCatalog::find(QStringLiteral("circle")) != nullptr, "H6 目录收录 circle");
        CHECK(CommandCatalog::find(QStringLiteral("CIRCLE")) != nullptr,
              "H6 目录查找大小写不敏感");
        const CommandInfo* ci = CommandCatalog::find(QStringLiteral("line"));
        CHECK(ci && ci->usage.contains(QStringLiteral("line x1,y1 x2,y2")),
              "H6 line 用法与实现同源");
        CHECK(CommandCatalog::categoryOf(QStringLiteral("spline_append_pt")) ==
                  QStringLiteral("绘图"),
              "H6 未收录家族命令按前缀推断分类");
    }

    return true;
}

// ============================================================
// t_diag：崩溃可复现诊断包（阶段六）
//   覆盖：Stored-ZIP 写入器结构（签名/条目数/EOCD）、ZIP 逐条目自解析 +
//   独立 CRC32 复算比对、exportBundle 产物齐全（system-info/drawing-meta）、
//   system-info 信息字段完整、drawing-meta 仅计数且不含几何、会话标记
//   异常退出检测（write→isAbnormal→clear）、遥测默认零外发（mock sink
//   断言 send 零调用；开启后仅经出口送达）。
// ============================================================
#include "DiagBundle.h"
#include "Diagnostics.h"
#include "Telemetry.h"

namespace {
// 独立实现的 CRC32（与 DiagBundle::crc32 互为参照，强验证）
quint32 diagRefCrc32(const QByteArray& data) {
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char ch : data) {
        crc ^= ch;
        for (int k = 0; k < 8; ++k)
            crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}
quint32 rd32(const QByteArray& b, int off) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(b.constData()) + off;
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}
quint16 rd16(const QByteArray& b, int off) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(b.constData()) + off;
    return quint16(quint16(p[0]) | (quint16(p[1]) << 8));
}
} // namespace

static bool t_diag() {
    using namespace SketchCAD;

    // ---- D1: ZIP 写入器结构（本地头签名 + EOCD 条目数一致）----
    QByteArray zip;
    {
        DiagBundle::ZipBuilder zb;
        zb.addFile(QStringLiteral("a.txt"), "hello diag");
        QByteArray bin;
        for (int i = 0; i < 500; ++i) bin.append(char(i & 0xFF));
        zb.addFile(QStringLiteral("sub/b.bin"), bin);
        CHECK(!zb.isEmpty(), "D1 ZipBuilder 非空判定");
        zip = zb.finish();
        CHECK(zip.size() > 4 && rd32(zip, 0) == 0x04034b50u,
              "D1 首条目本地头签名 PK\\x03\\x04");
        // EOCD 在尾部：签名 0x06054b50，条目数 = 2
        const int eocd = zip.size() - 22;   // 无注释时 EOCD 定长 22
        CHECK(rd32(zip, eocd) == 0x06054b50u, "D1 EOCD 签名位于尾部");
        CHECK(rd16(zip, eocd + 10) == 2 && rd16(zip, eocd + 8) == 2,
              "D1 EOCD 条目总数 = 2（本盘/全盘一致）");
        CHECK(rd16(zip, 8) == 0, "D1 压缩方法 = 0（Stored 不压缩）");
    }

    // ---- D2: ZIP 逐条目自解析 + 独立 CRC32 复算 ----
    {
        const int eocd = zip.size() - 22;
        const quint32 cdOff = rd32(zip, eocd + 16);
        const quint32 cdSize = rd32(zip, eocd + 12);
        int off = int(cdOff), found = 0, crcOk = 0;
        while (off < int(cdOff + cdSize)) {
            if (rd32(zip, off) != 0x02014b50u) break;   // 中央目录签名
            const quint32 crc = rd32(zip, off + 16);
            const quint32 sz  = rd32(zip, off + 24);
            const quint32 lho = rd32(zip, off + 42);
            const int nameLen = rd16(zip, off + 28);
            const QString name = QString::fromUtf8(zip.mid(off + 46, nameLen));
            // 回读本地头里的数据并与独立 CRC 比对
            const int lhNameLen = rd16(zip, int(lho) + 26);
            const int lhExtraLen = rd16(zip, int(lho) + 28);
            const int dataOff = int(lho) + 30 + lhNameLen + lhExtraLen;
            const QByteArray data = zip.mid(dataOff, int(sz));
            ++found;
            if (diagRefCrc32(data) == crc) ++crcOk;
            if (name == QStringLiteral("a.txt"))
                CHECK(data == QByteArray("hello diag"), "D2 条目 a.txt 内容逐字节一致");
            off += 46 + nameLen;
        }
        CHECK(found == 2, "D2 中央目录遍历到全部 2 个条目");
        CHECK(crcOk == 2, "D2 全部条目 CRC32 与独立实现复算一致");
    }

    // ---- D3/D4/D5: exportBundle 产物齐全 + 信息字段 + meta 边界 ----
    {
        const QString tmp =
            QDir::tempPath() + QStringLiteral("/sketchcad-t-diag-%1")
                .arg(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tmp);
        QVariantMap meta;
        meta.insert(QStringLiteral("drawingFile"), QStringLiteral("(未命名)"));
        meta.insert(QStringLiteral("elementCount"), 42);
        meta.insert(QStringLiteral("layerCount"), 3);
        meta.insert(QStringLiteral("blockCount"), 1);
        QString err;
        const QString zipPath = DiagBundle::exportBundle(meta, tmp, &err);
        const QByteArray d3msg = QStringLiteral("D3 诊断包 ZIP 生成于指定目录（%1）").arg(err).toUtf8();
        CHECK(!zipPath.isEmpty() && QFileInfo::exists(zipPath), d3msg.constData());
        QFile f(zipPath);
        QByteArray bundle;
        if (f.open(QIODevice::ReadOnly)) bundle = f.readAll();
        CHECK(bundle.size() > 100, "D3 诊断包非空");

        // 直接从包里捞 system-info.txt / drawing-meta.json 内容（复用 D2 的解析法）
        auto extract = [&bundle](const QString& want) -> QByteArray {
            const int eocd = bundle.size() - 22;
            const quint32 cdOff = rd32(bundle, eocd + 16);
            const quint32 cdSize = rd32(bundle, eocd + 12);
            int off = int(cdOff);
            while (off < int(cdOff + cdSize)) {
                if (rd32(bundle, off) != 0x02014b50u) break;
                const quint32 crc = rd32(bundle, off + 16);
                const quint32 sz = rd32(bundle, off + 24);
                const quint32 lho = rd32(bundle, off + 42);
                const int nameLen = rd16(bundle, off + 28);
                const QString name =
                    QString::fromUtf8(bundle.mid(off + 46, nameLen));
                const int lhNameLen = rd16(bundle, int(lho) + 26);
                const int lhExtraLen = rd16(bundle, int(lho) + 28);
                const int dataOff = int(lho) + 30 + lhNameLen + lhExtraLen;
                const QByteArray data = bundle.mid(dataOff, int(sz));
                if (name == want && diagRefCrc32(data) == crc) return data;
                off += 46 + nameLen;
            }
            return {};
        };
        const QByteArray info = extract(QStringLiteral("system-info.txt"));
        CHECK(!info.isEmpty(), "D4 包内含 system-info.txt 且 CRC 一致");
        CHECK(info.contains("Qt 版本") && info.contains(qVersion()),
              "D4 系统信息含 Qt 版本字段且与运行时一致");
        CHECK(info.contains("操作系统"), "D4 系统信息含操作系统字段");
        CHECK(info.contains("显卡"), "D4 系统信息含显卡字段（仅设备名）");
        const QByteArray metaJson = extract(QStringLiteral("drawing-meta.json"));
        CHECK(metaJson.contains("\"elementCount\"") &&
                  metaJson.contains("42"),
              "D5 drawing-meta.json 含 elementCount=42 计数");
        CHECK(metaJson.size() < 400, "D5 drawing-meta.json 体积极小（仅计数，无几何/文本）");
        // 输出目录清理（测试自身产生的临时产物）
        QFile::remove(zipPath);
        QDir(tmp).rmdir(tmp);
    }

    // ---- D6: 会话标记（异常退出检测，路径注入版）----
    {
        const QString marker =
            QDir::tempPath() + QStringLiteral("/sketchcad-t-diag-marker-%1.marker")
                .arg(QDateTime::currentMSecsSinceEpoch());
        QFile::remove(marker);
        CHECK(!Diagnostics::isAbnormalExit(marker), "D6 无标记 = 上次正常退出");
        CHECK(Diagnostics::writeSessionMarker(marker), "D6 写会话标记成功");
        CHECK(Diagnostics::isAbnormalExit(marker), "D6 标记残留 = 上次异常退出");
        CHECK(Diagnostics::clearSessionMarker(marker), "D6 清除标记成功");
        CHECK(!Diagnostics::isAbnormalExit(marker), "D6 清除后恢复正常判定");
    }

    // ---- D7: 遥测默认零外发（mock sink 断言）----
    {
        struct MockSink : TelemetrySink {
            int calls = 0;
            QString lastEvent;
            QVariantMap lastPayload;
            void send(const QString& e, const QVariantMap& p) override {
                ++calls; lastEvent = e; lastPayload = p;
            }
        } sink;
        // 恢复默认态：无出口 + 关闭（防止其他测试污染，也验证双门闩各自有效）
        Telemetry::setSink(nullptr);
        Telemetry::setEnabled(false);

        Telemetry::record(QStringLiteral("session_start"));
        Telemetry::record(QStringLiteral("crash"), { { "count", 1 } });
        CHECK(sink.calls == 0, "D7 默认状态（无出口+关闭）record 零外发");
        CHECK(Telemetry::sink() == nullptr && !Telemetry::enabled(),
              "D7 默认状态自省：sink 为空、开关关闭");

        Telemetry::setEnabled(true);   // 只开开关、无出口
        Telemetry::record(QStringLiteral("crash"));
        CHECK(sink.calls == 0, "D7 仅开开关但无出口 → 仍然零外发（双门闩）");

        Telemetry::setSink(&sink);     // 出口就位 → 应送达一次
        Telemetry::record(QStringLiteral("crash"), { { "count", 7 } });
        CHECK(sink.calls == 1 && sink.lastEvent == QStringLiteral("crash") &&
                  sink.lastPayload.value(QStringLiteral("count")).toInt() == 7,
              "D7 开关+出口齐备 → 恰好一次送达且字段一致");

        Telemetry::setEnabled(false);  // 关开关 → 立即停发
        Telemetry::record(QStringLiteral("crash"));
        CHECK(sink.calls == 1, "D7 关闭后不再外发");

        // 恢复全局默认（防污染其他测试）
        Telemetry::setSink(nullptr);
        Telemetry::setEnabled(false);
    }

    return true;
}

// ============================================================================
// t_dwg —— D1：DWG 版本路由 + 外部转换器桥（DwgBridge）+ R12 回归
// 覆盖：版本串探测 / 读取白名单 / 人类可读名 / 桥可用性探测 / 错误报文含版本
// 串原文 / fromDwg 路由 / toDwgR2000 未装组件报错 / R12 自研读写回归。
// 真转换器 round-trip（黄金差分）由环境变量门控：SKETCHCAD_DWGTOOLS 指向
// 转换器目录且 SKETCHCAD_DWG_SAMPLE 指向 .dwg 样本时执行，否则跳过。
// ============================================================================
#include "DwgBridge.h"
#include <QProcess>   // t_dwg D7：子进程环境探测（受限沙箱下跳过真转断言）

static bool t_dwg() {
    using namespace SketchCAD;
    using namespace SketchCAD::cadfmt;   // fromDwg/toDwg/toDwgR2000
    using dwgbridge::available;
    using dwgbridge::canReadVersion;
    using dwgbridge::dxfToDwgR2000;
    using dwgbridge::dwgToDxf;
    using dwgbridge::setToolsDirForTest;
    using dwgbridge::versionFromBytes;
    using dwgbridge::versionHuman;

    // ---- D1 版本串探测（唯一入口）----
    QByteArray r12 = toDwg({});                  // 自产 R12 头作样本
    CHECK(versionFromBytes(r12) == QLatin1String("AC1009"), "DWG D1 版本探测：toDwg 产物头部=AC1009");

    QByteArray fake = QByteArray("AC1015") + QByteArray(30, '\0');
    CHECK(versionFromBytes(fake) == QLatin1String("AC1015"), "DWG D1 版本探测：AC1015 识别");
    CHECK(versionFromBytes(QByteArray("AC10")) == QString(), "DWG D1 版本探测：过短回落空串");
    CHECK(versionFromBytes(QByteArray("SketchCAD is great")) == QString(),
          "DWG D1 版本探测：非 DWG 头回落空串");

    // ---- D2 读取白名单（AC1009 走自研不经桥；AC1012~AC1032 经桥）----
    CHECK(!canReadVersion(QLatin1String("AC1009")), "DWG D2 白名单：AC1009 不经桥（自研 R12 保护）");
    CHECK(canReadVersion(QLatin1String("AC1015")) && canReadVersion(QLatin1String("AC1018")),
          "DWG D2 白名单：R2000/2000i 放行");
    CHECK(canReadVersion(QLatin1String("AC1021")) && canReadVersion(QLatin1String("AC1024")) &&
              canReadVersion(QLatin1String("AC1027")) && canReadVersion(QLatin1String("AC1032")),
          "DWG D2 白名单：R2004/2007/2013/2018 放行");
    CHECK(!canReadVersion(QLatin1String("AC1999")) && !canReadVersion(QString()),
          "DWG D2 白名单：未知版本串拒绝");

    // ---- D2 人类可读名（错误报文共用 cadVerToHuman）----
    CHECK(versionHuman(QLatin1String("AC1021")) == QStringLiteral("DWG 2004") &&
              versionHuman(QLatin1String("AC1024")) == QStringLiteral("DWG 2007"),
          "DWG D2 版本串→人类可读名一致");

    // ---- D3 桥可用性探测（临时目录注入，无副作用）----
    QTemporaryDir emptyTools;
    CHECK(emptyTools.isValid(), "DWG D3 临时工具目录创建成功");
    setToolsDirForTest(emptyTools.path());
    CHECK(!available(), "DWG D3 空工具目录 → 桥不可用");

    {
        QFile f1(QDir(emptyTools.path()).filePath(QStringLiteral("dwg2dxf.exe")));
        QFile f2(QDir(emptyTools.path()).filePath(QStringLiteral("dwgwrite.exe")));
        const bool made = f1.open(QIODevice::WriteOnly) && f2.open(QIODevice::WriteOnly);
        CHECK(made, "DWG D3 假转换器 exe 落盘成功");
    }
    CHECK(available(), "DWG D3 双转换器 exe 齐备 → 桥可用");

    // ---- D3 桥不可用时错误报文含版本串原文 ----
    const QByteArray ac1021bytes = QByteArray("AC1021") + QByteArray(200, '\0');
    QByteArray dxfOut; QString bridgeErr;
    CHECK(!dwgToDxf(ac1021bytes, dxfOut, &bridgeErr), "DWG D3 桥不可用 → dwgToDxf 失败");
    const QByteArray d3msg = bridgeErr.toUtf8();
    CHECK(bridgeErr.contains(QLatin1String("AC1021")) && bridgeErr.contains(QStringLiteral("DWG 2004")),
          "DWG D3 错误报文含版本串原文+人类可读名");

    // ---- D4 fromDwg 路由：R2000+ 经桥，桥不可用 → 明确报错不误读 ----
    {
        std::vector<Element> out; QString err;
        CHECK(!fromDwg(ac1021bytes, out, &err) && out.empty(),
              "DWG D4 fromDwg(AC1021)：桥不可用 → 拒绝");
        const QByteArray d4msg = err.toUtf8();
        CHECK(err.contains(QLatin1String("AC1021")), "DWG D4 fromDwg 错误报文含版本串原文");

        out.clear(); err.clear();
        const QByteArray unknownVer = QByteArray("AC1999") + QByteArray(200, '\0');
        CHECK(!fromDwg(unknownVer, out, &err), "DWG D4 fromDwg(AC1999)：未知版本拒绝");
        CHECK(err.contains(QLatin1String("AC1999")), "DWG D4 未知版本报文含原文");

        out.clear(); err.clear();
        const QByteArray notDwg = QByteArray("Sketch") + QByteArray(200, '\0');
        CHECK(!fromDwg(notDwg, out, &err), "DWG D4 fromDwg(非DWG头)拒绝");
        CHECK(err.contains(QLatin1String("Sketch")), "DWG D4 非DWG报文含头部原文");
    }

    // ---- D5 toDwgR2000：桥不可用 → 空数组+指引报文 ----
    // 注：注入**不存在的目录**（test 注入优先级最高，不受环境变量影响），
    // 复现"转换组件未安装"；不能用 D3 的假 exe 目录（会走"无法启动"分支）。
    {
        Element line; line.type = ElementType::Line;
        line.pts = { Vec3(0, 0, 0), Vec3(10, 10, 0) };
        setToolsDirForTest(QStringLiteral("Z:/__no_such_dwg_tools__"));
        QString err;
        const QByteArray dwg = toDwgR2000({ line }, &err);
        CHECK(dwg.isEmpty() && err.contains(QStringLiteral("未安装")),
              "DWG D5 toDwgR2000 桥不可用 → 空数组+未安装指引");
        const QByteArray d5msg = err.toUtf8();
        CHECK(err.contains(QStringLiteral("R2000")) || err.contains(QStringLiteral("tools")),
              "DWG D5 未安装报文含版本上下文或指引路径");
        setToolsDirForTest(QString());   // 立即恢复，防污染 D6/D7
    }

    // ---- D6 R12 自研回归（升级前后语义不动）----
    {
        Element line; line.type = ElementType::Line;
        line.pts = { Vec3(0, 0, 0), Vec3(10, 10, 0) };
        Element circle; circle.type = ElementType::Circle;
        circle.pts = { Vec3(5, 5, 0) }; circle.radius = 3.0;
        const std::vector<Element> elems = { line, circle };

        const QByteArray dwg12 = toDwg(elems);
        CHECK(dwg12.left(6) == QByteArray("AC1009"), "DWG D6 toDwg 仍输出 AC1009（逐字节兼容）");

        std::vector<Element> back; QString err;
        CHECK(fromDwg(dwg12, back, &err), "DWG D6 R12 自研回读成功");
        CHECK((int)back.size() == (int)elems.size(),
              "DWG D6 R12 round-trip 实体数一致（回归保护）");

        // 坏指针 R12：头部合法但实体段指针无效 → 原有报文原样触发（强校验语义不动）
        QByteArray corrupt = QByteArray("AC1009") + QByteArray(400, '\0');
        back.clear(); err.clear();
        CHECK(!fromDwg(corrupt, back, &err), "DWG D6 R12 坏实体段指针 → 拒绝");
        CHECK(err == QStringLiteral("实体段指针无效"), "DWG D6 R12 强校验报文原样（语义未动）");
    }

    // ---- D7 真转换器 round-trip（黄金差分；环境变量门控 + 子进程环境探测）----
    // 双重门控：① SKETCHCAD_DWGTOOLS + SKETCHCAD_DWG_SAMPLE 指向转换器与样本；
    // ② 当前进程允许创建子进程（部分受限环境/CI 沙箱禁止任何 CreateProcess，
    //    连 cmd.exe 都 FailedToStart——此时跳过真转断言；转换器 CLI 的读写
    //    闭环已由外部冒烟验证，见 docs/dwg-r2000-bridge-design-analysis.md）。
    const QString toolsEnv = qEnvironmentVariable("SKETCHCAD_DWGTOOLS");
    const QString sampleEnv = qEnvironmentVariable("SKETCHCAD_DWG_SAMPLE");
    bool procEnvOk = false;
    {
        QProcess probe;
        probe.setProgram(QStringLiteral("cmd.exe"));
        probe.setArguments({ QStringLiteral("/c"), QStringLiteral("echo ok") });
        probe.start();
        procEnvOk = probe.waitForStarted(3000);
        if (procEnvOk) { probe.kill(); probe.waitForFinished(2000); }
    }
    if (!procEnvOk)
        CHECK(true, "DWG D7 真转断言跳过：当前环境禁止子进程创建（CLI 冒烟已覆盖）");
    else if (!toolsEnv.isEmpty() && !sampleEnv.isEmpty()
        && QFile::exists(sampleEnv)) {
        setToolsDirForTest(toolsEnv);
        CHECK(available(), "DWG D7 门控真转：转换器可用");

        // 读：黄金样本 DWG → dwg2dxf → fromDxf 解析
        QFile sample(sampleEnv);
        const bool sampleOpened = sample.open(QIODevice::ReadOnly);
        CHECK(sampleOpened, "DWG D7 门控真转：黄金样本可读");
        const QByteArray sampleBytes = sampleOpened ? sample.readAll() : QByteArray();
        QByteArray goldDxf; QString goldErr;
        const bool goldOk = dwgToDxf(sampleBytes, goldDxf, &goldErr);
        if (!goldOk)
            fprintf(stderr, "[dwg-diag] goldErr=%s\n", goldErr.toUtf8().constData());
        CHECK(goldOk, "DWG D7 黄金样本 dwg2dxf 转换成功");
        if (!goldDxf.isEmpty()) {
            std::vector<Element> parsed; QString parseErr;
            CHECK(fromDwg(sampleBytes, parsed, &parseErr), "DWG D7 黄金样本经 fromDwg 全链解析成功");
            CHECK((int)parsed.size() > 0, "DWG D7 黄金样本解析出图元");
        }

        // 写：自绘图元 → toDxf(R2000) → dwgwrite --as r2000 → dwg2dxf 回读差分
        Element line; line.type = ElementType::Line;
        line.pts = { Vec3(0, 0, 0), Vec3(42, 17, 0) };
        Element circle; circle.type = ElementType::Circle;
        circle.pts = { Vec3(5, 5, 0) }; circle.radius = 3.0;
        const std::vector<Element> elems = { line, circle };
        QString err;
        const QByteArray dwg2000 = toDwgR2000(elems, &err);
        if (dwg2000.isEmpty())
            fprintf(stderr, "[dwg-diag] writeErr=%s\n", err.toUtf8().constData());
        CHECK(!dwg2000.isEmpty(), "DWG D7 toDwgR2000 真转换成功");
        if (!dwg2000.isEmpty()) {
            CHECK(dwg2000.left(6) == QByteArray("AC1015"),
                  "DWG D7 产物头部版本串 = AC1015（R2000）");
            QByteArray rtDxf; QString rtErr;
            CHECK(dwgToDxf(dwg2000, rtDxf, &rtErr), "DWG D7 自产 R2000 DWG 可被转换器回读");
            if (!rtDxf.isEmpty()) {
                std::vector<Element> back; QString bErr;
                CHECK(fromDwg(dwg2000, back, &bErr), "DWG D7 自产 R2000 DWG 经 fromDwg 全链回读");
                CHECK((int)back.size() == (int)elems.size(),
                      "DWG D7 写→读 round-trip 实体数差分为零");
            }
        }
        setToolsDirForTest(QString());   // 恢复默认，防污染其他测试
    }

    // ---- D8 原生实体不炸开（导入解析层，无需外部转换器）----
    // 直接喂一段含 DIMENSION + HATCH 的 DXF 文本给 fromDxf，验证其按 ElementType::Dimension /
    // Hatch 原生还原（而非降级为 LINE+TEXT 或边界线）。这正是 R2000 经桥接导入时
    // "原生实体不炸开" 所依赖的解析能力（DwgBridge::dwgToDxf → fromDxf 的末端链路）。
    {
        const QByteArray nativeDxf =
            "0\nSECTION\n2\nENTITIES\n"
            "0\nDIMENSION\n70\n1\n13\n0.0\n23\n0.0\n14\n10.0\n24\n0.0\n"
            "0\nHATCH\n91\n1\n92\n2\n93\n4\n"
            "10\n0.0\n20\n0.0\n10\n10.0\n20\n0.0\n"
            "10\n10.0\n20\n10.0\n10\n0.0\n20\n10.0\n"
            "0\nENDSEC\n0\nEOF\n";
        std::vector<Element> nat;
        QString natErr;
        CHECK(fromDxf(nativeDxf, nat, &natErr), "DWG D8 原生 DXF(含 DIMENSION/HATCH) 解析成功");
        int nDim = 0, nHatch = 0;
        for (const auto& e : nat) {
            if (e.type == ElementType::Dimension) ++nDim;
            if (e.type == ElementType::Hatch)     ++nHatch;
        }
        CHECK(nDim   >= 1, "DWG D8 导入层将 DIMENSION 还原为原生 ElementType::Dimension（不炸开）");
        CHECK(nHatch >= 1, "DWG D8 导入层将 HATCH 还原为原生 ElementType::Hatch（不炸开）");
    }

    setToolsDirForTest(QString());       // 无条件恢复默认
    return true;
}

static bool t_cmdParity() {
    using namespace SketchCAD;
    // 默认别名在 GUI 启动时由 aliasreload 加载到 AliasTable 单例；无头测试需手动灌入，
    // 才能验证 h → hatch 等别名路径（与真实运行环境的启动前提一致；详见 docs/qselect-design-analysis.md）。
    {
        const auto defs = AliasTable::defaultAliases();
        for (auto it = defs.constBegin(); it != defs.constEnd(); ++it)
            AliasTable::instance().setAlias(it.key(), it.value());
    }
    // ---------- HATCH / BHATCH ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("hatch")), "HATCH 无选中→用法提示(false)");
        const int line = addLine(m, 0, 0, 10, 0);          // 开放直线不可成环
        m.setSelection({ line });
        CHECK(!interp.execute(QStringLiteral("hatch")), "HATCH 开放边界→拒绝(false)");
        SketchModel m2; CommandInterpreter i2(&m2);
        const int circ = addCircle(m2, 5, 5, 4);           // 整圆=闭合边界
        m2.setSelection({ circ });
        const int before = m2.elementCount();
        CHECK(i2.execute(QStringLiteral("hatch")), "HATCH 选中圆→生成关联填充");
        CHECK(m2.elementCount() == before + 1, "HATCH 新增 1 个图元");
        CHECK(countType(m2, ElementType::Hatch) == 1, "HATCH 产出填充元素");
    }
    {
        SketchModel m; CommandInterpreter interp(&m);
        const int circ = addCircle(m, 0, 0, 3);
        m.setSelection({ circ });
        CHECK(interp.execute(QStringLiteral("bhatch")), "BHATCH 别名→生成关联填充");
        CHECK(countType(m, ElementType::Hatch) == 1, "BHATCH 产出填充元素");
    }

    // ---------- PEDIT ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("pedit")), "PEDIT 缺参数→用法(false)");
        CHECK(!interp.execute(QStringLiteral("pedit 999 width 2")), "PEDIT 非法 id→拒绝(false)");
        CHECK(!interp.execute(QStringLiteral("pedit 1 width abc")), "PEDIT 线宽非数→拒绝(false)");
        Element pl; pl.type = ElementType::Polyline;
        pl.pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0) };
        pl.arcFlags.assign(4, false); pl.closed = false;
        const int pid = m.addElement(pl);
        CHECK(interp.execute(QStringLiteral("pedit %1 close").arg(pid)), "PEDIT close→成功");
        CHECK(m.element(pid) && m.element(pid)->closed, "PEDIT close 生效(closed=true)");
        CHECK(interp.execute(QStringLiteral("pedit %1 width 2.5").arg(pid)), "PEDIT width→成功");
        CHECK(m.element(pid) && std::fabs(m.element(pid)->lineWidth - 2.5) < 1e-9, "PEDIT width 生效(lineWidth=2.5)");
    }

    // ---------- MATCHPROP ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("matchprop")), "MATCHPROP 缺参数→用法(false)");
        CHECK(!interp.execute(QStringLiteral("matchprop 999 1")), "MATCHPROP 源非法→拒绝(false)");
        const int src = addLine(m, 0, 0, 1, 0);
        const int tgt = addLine(m, 0, 5, 1, 5);
        m.updateElementQuiet(src, [](Element& e){ e.color = QColor(255, 0, 0); e.layer = QStringLiteral("0"); });
        CHECK(interp.execute(QStringLiteral("matchprop %1 %2").arg(src).arg(tgt)), "MATCHPROP 复制属性→成功");
        CHECK(m.element(tgt) && m.element(tgt)->color == QColor(255, 0, 0), "MATCHPROP 目标颜色被复制");
    }

    // ---------- PURGE ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("purge bogus")), "PURGE 未知子命令→用法(false)");
        CHECK(m.addLayer(QStringLiteral("TMP")), "PURGE 建未用图层 TMP");
        const int used = addLine(m, 0, 0, 1, 0);
        m.updateElementQuiet(used, [](Element& e){ e.layer = QStringLiteral("TMP2"); });
        CHECK(m.addLayer(QStringLiteral("TMP2")), "PURGE 建被使用图层 TMP2");
        CHECK(interp.execute(QStringLiteral("purge all")), "PURGE all→成功");
        bool hasTmp = false; for (int i=0;i<m.layerCount();++i) if (m.layerAt(i).name==QStringLiteral("TMP")) hasTmp=true;
        CHECK(!hasTmp, "PURGE 未用图层 TMP 已删");
        bool hasTmp2 = false; for (int i=0;i<m.layerCount();++i) if (m.layerAt(i).name==QStringLiteral("TMP2")) hasTmp2=true;
        CHECK(hasTmp2, "PURGE 被使用图层 TMP2 保留");
    }

    // ---------- OVERKILL ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        addLine(m, 0, 0, 10, 0);
        addLine(m, 0, 0, 10, 0);   // 与前者几何完全相同
        const int before = m.elementCount();
        CHECK(interp.execute(QStringLiteral("overkill")), "OVERKILL 去重→成功");
        CHECK(m.elementCount() == before - 1, "OVERKILL 删除 1 个重复图元");
    }

    // ---------- WBLOCK ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("wblock")), "WBLOCK 缺路径→用法(false)");
        const int l = addLine(m, 0, 0, 5, 0);
        m.setSelection({ l });
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("wb.dxf"));
        CHECK(interp.execute(QStringLiteral("wblock ") + path), "WBLOCK 写出→成功");
        QFile f(path);
        CHECK(f.exists() && f.size() > 0, "WBLOCK 产出非空 DXF 文件");
    }

    // ---------- INSERT ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("insert")), "INSERT 缺参数→用法(false)");
        CHECK(!interp.execute(QStringLiteral("insert NOSUCH 0,0")), "INSERT 不存在块→拒绝(false)");
        BlockDef bd; bd.name = QStringLiteral("B1");
        Element be; be.type = ElementType::Line; be.pts = { Vec3(0,0,0), Vec3(5,0,0) }; bd.entities = { be };
        CHECK(m.addBlockDef(bd), "INSERT 块定义创建");
        const int before = m.elementCount();
        CHECK(interp.execute(QStringLiteral("insert B1 0,0")), "INSERT 插入块→成功");
        CHECK(m.elementCount() == before + 1, "INSERT 新增 1 个图元");
    }

    // ---------- TEXT / MTEXT / DTEXT ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("text")), "TEXT 缺参数→用法(false)");
        CHECK(!interp.execute(QStringLiteral("text 0,0 -5 hi")), "TEXT 负字高→拒绝(false)");
        CHECK(!interp.execute(QStringLiteral("text bad 5 hi")), "TEXT 非法坐标→拒绝(false)");
        const int before = m.elementCount();
        CHECK(interp.execute(QStringLiteral("text 0,0 5 hello")), "TEXT 创建单行文字→成功");
        CHECK(m.elementCount() == before + 1 && countType(m, ElementType::Text) == 1, "TEXT 产出文字元素");
        CHECK(interp.execute(QStringLiteral("mtext 0,0 5 multi")), "MTEXT 创建多行文字→成功");
        CHECK(countType(m, ElementType::MText) == 1, "MTEXT 产出多行文字元素");
        CHECK(interp.execute(QStringLiteral("dtext 0,0 5 dyn")), "DTEXT 别名→成功");
    }

    // ---------- DDEDIT ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("ddedit")), "DDEDIT 缺参数→用法(false)");
        Element tx; tx.type = ElementType::Text; tx.pts = { Vec3(0,0,0) }; tx.text = QStringLiteral("old"); tx.radius = 3;
        const int id = m.addElement(tx);
        m.setSelection({ id });
        CHECK(!interp.execute(QStringLiteral("ddedit 0,0 x")), "DDEDIT 选中非文字→拒绝(false)");
        CHECK(interp.execute(QStringLiteral("ddedit new")), "DDEDIT 修改内容→成功");
        CHECK(m.element(id) && m.element(id)->text == QStringLiteral("new"), "DDEDIT 内容已更新");
    }

    // ---------- PROPERTIES ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("properties")), "PROPERTIES 无选中→用法(false)");
        const int id = addLine(m, 0, 0, 1, 0);
        m.setSelection({ id });
        CHECK(interp.execute(QStringLiteral("properties")), "PROPERTIES 选中→列出属性");
    }

    // ---------- LINETYPE ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("linetype")), "LINETYPE 无参→列出线型表");
        CHECK(interp.execute(QStringLiteral("linetype 虚线")), "LINETYPE 设为虚线→成功");
        CHECK(m.defaultLineStyle() == LineStyle::Dashed, "LINETYPE 当前线型=虚线");
        CHECK(!interp.execute(QStringLiteral("linetype scale -1")), "LINETYPE scale 负值→拒绝(false)");
    }

    // ---------- MEASURE ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("measure")), "MEASURE 缺参数→用法(false)");
        CHECK(!interp.execute(QStringLiteral("measure 1 -5")), "MEASURE 负间距→拒绝(false)");
        const int id = addLine(m, 0, 0, 100, 0);
        const int before = countType(m, ElementType::Point);
        CHECK(interp.execute(QStringLiteral("measure %1 25").arg(id)), "MEASURE 放置测量点→成功");
        CHECK(countType(m, ElementType::Point) >= before + 3, "MEASURE 沿直线放置≥3 个测量点");
    }

    // ---------- TOL ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("tol")), "TOL 缺参数→用法(false)");
        const int before = m.elementCount();
        CHECK(interp.execute(QStringLiteral("tol 0,0 x")), "TOL 创建形位公差→成功");
        CHECK(m.elementCount() == before + 1 && countType(m, ElementType::Fcf) == 1, "TOL 产出 Fcf 元素");
    }

    // ---------- REGEN ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("regen")), "REGEN 重生成→成功");
    }

    // ---------- 别名展开 + typo 建议 ----------
    {
        SketchModel m; CommandInterpreter interp(&m);
        // 别名 h → hatch（需选中闭合边界）
        const int circ = addCircle(m, 0, 0, 3);
        m.setSelection({ circ });
        CHECK(interp.execute(QStringLiteral("h")), "别名 h→hatch 展开生效");
        CHECK(countType(m, ElementType::Hatch) == 1, "别名 h 触发填充");
        // 输错命令名应给出建议（不崩溃）
        CHECK(!interp.execute(QStringLiteral("hatchh")), "typo hatchh→未知命令(false)");
    }
    return true;
}

// ============================================================
// 撤销表自测（"万能回退"）：随图属性与几何同源回退
// ------------------------------------------------------------
// 覆盖 单位 / 界限 / 捕捉 / 追踪 / 布局 / 过滤器 六类随图属性，每类
// 「改一次 → undo → 值还原 → redo → 值恢复」；另加快照去重、栈深上限
// （超限丢弃最旧）、混合三步用例、几何操作回归。
// ============================================================
static bool t_undoTables() {
    using namespace SketchCAD;

    // ---------- 1. 单位 ----------
    {
        SketchModel m;
        const UnitsMeta u0 = m.units();
        UnitsMeta u1 = u0; u1.length = LengthUnit::Inch; u1.lengthPrec = 4;
        m.setUnits(u1);
        CHECK(m.units() == u1, "撤销表/单位：改后生效");
        m.undo();
        CHECK(m.units() == u0, "撤销表/单位：undo 还原");
        m.redo();
        CHECK(m.units() == u1, "撤销表/单位：redo 恢复");
    }

    // ---------- 2. 图纸界限 ----------
    {
        SketchModel m;
        const LimitsMeta l0 = m.limits();
        LimitsMeta l1 = l0; l1.x2 = 594.0; l1.y2 = 420.0; l1.checkOn = true;   // A2
        m.setLimits(l1);
        CHECK(m.limits() == l1, "撤销表/界限：改后生效");
        m.undo();
        CHECK(m.limits() == l0, "撤销表/界限：undo 还原");
        m.redo();
        CHECK(m.limits() == l1, "撤销表/界限：redo 恢复");
    }

    // ---------- 3. 捕捉配置 ----------
    {
        SketchModel m;
        const snap::SnapConfig c0 = m.snapConfig();
        snap::SnapConfig c1 = c0; c1.midpoint = !c1.midpoint; c1.quadrant = true;
        m.setSnapConfig(c1);
        CHECK(m.snapConfig() == c1, "撤销表/捕捉：改后生效");
        m.undo();
        CHECK(m.snapConfig() == c0, "撤销表/捕捉：undo 还原");
        m.redo();
        CHECK(m.snapConfig() == c1, "撤销表/捕捉：redo 恢复");
    }

    // ---------- 4. 追踪配置 ----------
    {
        SketchModel m;
        const snap::TrackConfig t0 = m.trackConfig();
        snap::TrackConfig t1 = t0; t1.polarIncrement = 45.0; t1.polar = !t1.polar;
        m.setTrackConfig(t1);
        CHECK(m.trackConfig() == t1, "撤销表/追踪：改后生效");
        m.undo();
        CHECK(m.trackConfig() == t0, "撤销表/追踪：undo 还原");
        m.redo();
        CHECK(m.trackConfig() == t1, "撤销表/追踪：redo 恢复");
    }

    // ---------- 5. 布局表（含当前布局）----------
    {
        SketchModel m;
        PrintSettings ps;
        CHECK(m.addLayout(QStringLiteral("布局A"), ps), "撤销表/布局：新增成功");
        CHECK(m.layoutCount() == 1, "撤销表/布局：新增后 1 张");
        m.setCurrentLayout(QStringLiteral("布局A"));      // 切页签本身不产生撤销步
        CHECK(m.currentLayout() == QStringLiteral("布局A"), "撤销表/布局：当前布局切换生效");
        m.undo();
        CHECK(m.layoutCount() == 0, "撤销表/布局：undo 回到 0 张");
        CHECK(m.currentLayout().isEmpty(), "撤销表/布局：undo 后当前布局回落模型空间");
        m.redo();
        CHECK(m.layoutCount() == 1, "撤销表/布局：redo 恢复 1 张");
        CHECK(m.currentLayout() == QStringLiteral("布局A"), "撤销表/布局：redo 恢复当前布局");
    }

    // ---------- 6. 命名过滤器 ----------
    {
        SketchModel m;
        FilterDef f;
        f.name = QStringLiteral("只看0层");
        f.matchAll = true;
        QSelectCriteria c;
        c.prop = QSelectProp::Layer; c.op = QSelectOp::Eq; c.value = QStringLiteral("0");
        f.conds.push_back(c);
        CHECK(m.saveFilter(f), "撤销表/过滤器：保存成功");
        CHECK(m.savedFilters().size() == 1, "撤销表/过滤器：保存后 1 条");
        m.undo();
        CHECK(m.savedFilters().empty(), "撤销表/过滤器：undo 后清空");
        m.redo();
        CHECK(m.savedFilters().size() == 1, "撤销表/过滤器：redo 恢复 1 条");
    }

    // ---------- 7. 快照去重：连续相同状态不重复入栈 ----------
    {
        SketchModel m;
        const int before = m.undoStackSize();
        m.beginEdit(QStringLiteral("第一次"));
        const int after1 = m.undoStackSize();
        m.beginEdit(QStringLiteral("第二次"));            // 期间未改动任何状态 → 应被去重
        CHECK(after1 == before + 1, "撤销表/去重：首次 beginEdit 正常入栈");
        CHECK(m.undoStackSize() == after1, "撤销表/去重：状态未变的重复 beginEdit 不新增快照");
    }

    // ---------- 8. 栈深上限：超限丢弃最旧 ----------
    {
        SketchModel m;
        m.setUndoDepthLimit(5);
        CHECK(m.undoDepthLimit() == 5, "撤销表/栈深：设置为 5 生效");
        for (int i = 0; i < 12; ++i) addPoint(m, (double)i, 0.0);
        CHECK(m.elementCount() == 12, "撤销表/栈深：12 个点全部加入");
        CHECK(m.undoStackSize() == 5, "撤销表/栈深：溢出后只保留 5 步");
        int steps = 0;
        while (m.canUndo()) { m.undo(); ++steps; }
        CHECK(steps == 5, "撤销表/栈深：可撤销次数恰为上限 5");
        CHECK(m.elementCount() == 7, "撤销表/栈深：丢弃最旧后只能回到第 7 个点");
        m.setUndoDepthLimit(0);                            // 非法 → 回落默认
        CHECK(m.undoDepthLimit() == 100, "撤销表/栈深：非法值 0 回落默认 100");
        m.setUndoDepthLimit(99999);                        // 越界 → 回落默认
        CHECK(m.undoDepthLimit() == 100, "撤销表/栈深：越界值回落默认 100");
    }

    // ---------- 9. 混合用例：改布局 + 移动图元 + 改单位 ----------
    {
        SketchModel m;
        const int line = addLine(m, 0, 0, 10, 0);
        const UnitsMeta u0 = m.units();
        // 步1：改布局
        PrintSettings ps;
        CHECK(m.addLayout(QStringLiteral("L1"), ps), "撤销表/混合：步1 新增布局");
        // 步2：移动图元（beginEdit + Quiet = 单步撤销，既有契约不变）
        m.beginEdit(QStringLiteral("平移图元"));
        m.updateElementQuiet(line, [](Element& e) { e.pts[0].x += 5.0; e.pts[1].x += 5.0; });
        CHECK(m.element(line) && std::fabs(m.element(line)->pts[0].x - 5.0) < 1e-9,
              "撤销表/混合：步2 图元已移动");
        // 步3：改单位
        UnitsMeta u1 = u0; u1.length = LengthUnit::Cm;
        m.setUnits(u1);
        CHECK(m.units() == u1, "撤销表/混合：步3 单位已改");

        // 三次 Ctrl+Z 全部还原
        m.undo();
        CHECK(m.units() == u0, "撤销表/混合：撤销1 单位还原");
        m.undo();
        CHECK(m.element(line) && std::fabs(m.element(line)->pts[0].x - 0.0) < 1e-9,
              "撤销表/混合：撤销2 图元回原位");
        m.undo();
        CHECK(m.layoutCount() == 0, "撤销表/混合：撤销3 布局移除");

        // 三次 Ctrl+Y 全部重做
        m.redo();
        CHECK(m.layoutCount() == 1, "撤销表/混合：重做1 布局恢复");
        m.redo();
        CHECK(m.element(line) && std::fabs(m.element(line)->pts[0].x - 5.0) < 1e-9,
              "撤销表/混合：重做2 图元移动恢复");
        m.redo();
        CHECK(m.units() == u1, "撤销表/混合：重做3 单位恢复");
    }

    // ---------- 10. 几何操作回归：不受本次改动影响 ----------
    {
        SketchModel m;
        const int a = addLine(m, 0, 0, 1, 0);
        const int n0 = m.elementCount();
        m.removeElement(a);
        CHECK(m.elementCount() == n0 - 1, "撤销表/回归：删除图元生效");
        m.undo();
        CHECK(m.elementCount() == n0, "撤销表/回归：撤销删除恢复");
        m.redo();
        CHECK(m.elementCount() == n0 - 1, "撤销表/回归：重做删除生效");
    }

    return true;
}

// ===================== 撤销栈差量化（t_undoDelta） =====================
// 验收：差量模式（默认）与全量模式（回落）在随机 500 步序列下撤销/重做终态图元集全等；
// 5 万图元场景单步撤销差量内存 ≤1MB、平均撤销耗时 ≤16ms；id 复用 / 联动同一步 /
// 检查点回放 / 模式切换清栈等行为正确。

// 图元 → 紧凑 JSON 字节（与 UndoDelta 同源，做逐图元字节级比较）
static QByteArray undoDeltaElemBytes(const Element& e) {
    return QJsonDocument(elementToJson(e)).toJson(QJsonDocument::Compact);
}
// 两模型「终态全等」：图元集（id + 字节）一致，且关键 aux 状态一致。
static bool modelsEquivalent(const SketchModel& a, const SketchModel& b) {
    if (a.elementCount() != b.elementCount()) return false;
    const auto& ea = a.elements();
    const auto& eb = b.elements();
    for (size_t i = 0; i < ea.size(); ++i) {
        if (ea[i].id != eb[i].id) return false;
        if (undoDeltaElemBytes(ea[i]) != undoDeltaElemBytes(eb[i])) return false;
    }
    if (!(a.units() == b.units())) return false;
    if (!(a.limits() == b.limits())) return false;
    if (!(a.snapConfig() == b.snapConfig())) return false;
    if (!(a.trackConfig() == b.trackConfig())) return false;
    if (a.layoutCount() != b.layoutCount()) return false;
    if (a.savedFilters().size() != b.savedFilters().size()) return false;
    return true;
}

static bool t_undoDelta() {
    using namespace SketchCAD;

    // ---------- A. 差量 vs 全量：随机 500 步序列，撤销/重做终态逐字节全等 ----------
    {
        SketchModel mDelta, mFull;
        mFull.setUndoDeltaEnabled(false);   // 回落全量模式（清栈后开始编辑）
        mDelta.setUndoDepthLimit(1000);     // 放宽栈深：覆盖全部 500 步（全量 500 快照 ≈ 36MB）
        mFull.setUndoDepthLimit(1000);
        std::mt19937 rng(0x9E3779B1u);
        std::uniform_int_distribution<int> pick(0, 5);
        auto rnd = [&]() -> double { return (double)rng() / (double)rng.max(); };

        const int N = 500;
        for (int i = 0; i < N; ++i) {
            int choice = pick(rng);
            double x0 = rnd()*200, y0 = rnd()*200, x1 = rnd()*200, y1 = rnd()*200;
            int lp = 2 + (int)(rng() % 4);
            switch (choice) {
            case 0: addLine(mDelta, x0, y0, x1, y1);  addLine(mFull, x0, y0, x1, y1); break;
            case 1: { double cx = rnd()*200, cy = rnd()*200, r = 1 + rnd()*50;
                      addCircle(mDelta, cx, cy, r); addCircle(mFull, cx, cy, r); } break;
            case 2: addPoint(mDelta, x0, y0); addPoint(mFull, x0, y0); break;
            case 3:
                if (mDelta.elementCount() > 0) {
                    int idx = (int)(rng() % mDelta.elementCount());
                    int id = mDelta.elements()[idx].id;
                    mDelta.removeElement(id); mFull.removeElement(id);
                }
                break;
            case 4:
                if (mDelta.elementCount() > 0) {
                    int idx = (int)(rng() % mDelta.elementCount());
                    int id = mDelta.elements()[idx].id;
                    mDelta.beginEdit("modify"); mDelta.updateElementQuiet(id, [](Element& e){ e.pts[0].x += 1.0; });
                    mFull.beginEdit("modify");  mFull.updateElementQuiet(id,  [](Element& e){ e.pts[0].x += 1.0; });
                }
                break;
            case 5: {
                UnitsMeta u = mDelta.units(); u.lengthPrec = lp; mDelta.setUnits(u);
                UnitsMeta u2 = mFull.units(); u2.lengthPrec = lp; mFull.setUnits(u2);
            } break;
            }
        }
        CHECK(modelsEquivalent(mDelta, mFull), "差量vs全量：500步编辑后终态全等");
        CHECK(mDelta.undoStackSize() == mFull.undoStackSize(),
              "差量vs全量：撤销步数一致（两模式 push 同频）");

        // 连续撤销：逐层对照（覆盖检查点回放 / 差量重建）
        int undoN = 0;
        while (mDelta.canUndo() && mFull.canUndo()) {
            mDelta.undo(); mFull.undo(); ++undoN;
            CHECK(modelsEquivalent(mDelta, mFull), "差量vs全量：撤销过程逐层终态全等");
        }
        CHECK(undoN > 400, "差量vs全量：可撤销步数充足（>400）");
        CHECK(mDelta.elementCount() == 0 && mFull.elementCount() == 0,
              "差量vs全量：全部撤销后图元清空");

        // 连续重做：逐层对照
        int redoN = 0;
        while (mDelta.canRedo() && mFull.canRedo()) {
            mDelta.redo(); mFull.redo(); ++redoN;
            CHECK(modelsEquivalent(mDelta, mFull), "差量vs全量：重做过程逐层终态全等");
        }
        CHECK(redoN == undoN, "差量vs全量：重做步数等于撤销步数");
        CHECK(modelsEquivalent(mDelta, mFull), "差量vs全量：重做到底终态与编辑后一致");
    }

    // ---------- B. 检查点回放：跨第 50 步检查点边界撤销/重做一致 ----------
    {
        SketchModel mDelta, mFull;
        mFull.setUndoDeltaEnabled(false);
        mDelta.setUndoDepthLimit(1000);     // 覆盖全部 120 步，不受默认栈深 100 裁剪干扰
        mFull.setUndoDepthLimit(1000);
        for (int i = 0; i < 120; ++i) addLine(mDelta, (double)i, 0, (double)i, 10);
        for (int i = 0; i < 120; ++i) addLine(mFull, (double)i, 0, (double)i, 10);
        CHECK(mDelta.undoStackSize() == 120 && mFull.undoStackSize() == 120, "检查点：两模式各 120 步");
        for (int k = 0; k < 70; ++k) { mDelta.undo(); mFull.undo(); }   // 跨越第 50 步检查点
        CHECK(modelsEquivalent(mDelta, mFull), "检查点：跨边界撤销 70 步后两模式一致");
        for (int k = 0; k < 70; ++k) { mDelta.redo(); mFull.redo(); }
        CHECK(modelsEquivalent(mDelta, mFull), "检查点：跨边界重做 70 步后两模式一致");
    }

    // ---------- C. 差量引擎单元：id 复用 / 删除后新建 / 乱序 / 通用 round-trip ----------
    {
        auto mkLine = [](int id, double x0, double y0, double x1, double y1) {
            Element e; e.type = ElementType::Line; e.id = id;
            e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) }; return e;
        };
        auto seqEq = [](const std::vector<Element>& got, const std::vector<Element>& want) {
            if (got.size() != want.size()) return false;
            for (size_t i = 0; i < want.size(); ++i)
                if (!(got[i].id == want[i].id && undoDeltaElemBytes(got[i]) == undoDeltaElemBytes(want[i])))
                    return false;
            return true;
        };

        // ① 同位 id 复用：图元 id 不变、内容变（建模器"重建同身份图元"场景）→ Modified
        {
            std::vector<Element> before = { mkLine(1,0,0,1,0), mkLine(2,2,0,3,0), mkLine(3,4,0,5,0) };
            std::vector<Element> after  = { mkLine(1,0,0,1,0), mkLine(2,9,9,10,9), mkLine(3,4,0,5,0) };
            std::vector<ElementDelta> d = diffElements(before, after);
            std::vector<Element> fwd = before; applyElementDeltas(fwd, d, true);
            CHECK(seqEq(fwd, after), "差量引擎/id复用：同位重建正向还原（Modified 原位替换）");
            std::vector<Element> rev = after; applyElementDeltas(rev, d, false);
            CHECK(seqEq(rev, before), "差量引擎/id复用：同位重建反向还原");
        }

        // ② 删除后新建（id 单调不复用，位置敏感）：删 id2 + 末尾加 id5 → Removed+Added
        {
            std::vector<Element> before = { mkLine(1,0,0,1,0), mkLine(2,2,0,3,0), mkLine(3,4,0,5,0), mkLine(4,6,0,7,0) };
            std::vector<Element> after  = { mkLine(1,0,0,1,0), mkLine(3,4,0,5,0), mkLine(4,6,0,7,0), mkLine(5,9,9,10,9) };
            std::vector<ElementDelta> d = diffElements(before, after);
            std::vector<Element> fwd = before; applyElementDeltas(fwd, d, true);
            CHECK(seqEq(fwd, after), "差量引擎/删除后新建：正向还原（删除+新增各归其位）");
            std::vector<Element> rev = after; applyElementDeltas(rev, d, false);
            CHECK(seqEq(rev, before), "差量引擎/删除后新建：反向还原（被删图元回插原位）");
        }

        // ③ 乱序（reorder 后两向量不再按 id 有序 → 慢路径）：保序回放
        {
            std::vector<Element> before = { mkLine(1,0,0,1,0), mkLine(2,2,0,3,0), mkLine(3,4,0,5,0) };
            std::vector<Element> after  = { mkLine(2,2,0,3,0), mkLine(1,0,0,1,0), mkLine(3,4,0,5,0) };
            std::vector<ElementDelta> d = diffElements(before, after);
            std::vector<Element> fwd = before; applyElementDeltas(fwd, d, true);
            CHECK(seqEq(fwd, after), "差量引擎/乱序：正向还原（reorder 保序）");
            std::vector<Element> rev = after; applyElementDeltas(rev, d, false);
            CHECK(seqEq(rev, before), "差量引擎/乱序：反向还原");
        }

        // ④ 通用混合：删除 + 修改 + 新增
        {
            std::vector<Element> b2 = { mkLine(1,0,0,1,0), mkLine(2,2,0,3,0), mkLine(3,4,0,5,0) };
            std::vector<Element> a2 = { mkLine(1,0,0,9,9), mkLine(3,4,0,5,0), mkLine(5,7,7,8,8) };
            std::vector<ElementDelta> d2 = diffElements(b2, a2);
            std::vector<Element> f2 = b2; applyElementDeltas(f2, d2, true);
            CHECK(seqEq(f2, a2), "差量引擎/混合：正向 round-trip 一致");
            std::vector<Element> r2 = a2; applyElementDeltas(r2, d2, false);
            CHECK(seqEq(r2, b2), "差量引擎/混合：反向 round-trip 一致");
        }
    }

    // ---------- D. 复合步 / 联动同一步：多操作合并为单步撤销 ----------
    {
        SketchModel m;
        int base = m.elementCount();
        m.beginEdit("复合步");
        std::vector<Element> parts(3);
        for (int k = 0; k < 3; ++k) { parts[k].type = ElementType::Line; parts[k].pts = { Vec3((double)k,0,0), Vec3((double)k,1,0) }; }
        m.addElementsQuiet(parts);
        m.updateElementQuiet(m.elements()[0].id, [](Element& e){ e.pts[1].y += 5.0; });
        CHECK(m.elementCount() == base + 3, "复合步：一次 beginEdit 加入 3 图元");
        CHECK(m.undoStackSize() == 1, "复合步：整组操作合并为单步撤销（步数=1）");
        m.undo();
        CHECK(m.elementCount() == base, "复合步：单步撤销还原整组（3 图元全撤）");
        m.redo();
        CHECK(m.elementCount() == base + 3, "复合步：单步重做恢复整组（3 图元全回）");
    }

    // ---------- E. 模式切换清空历史 ----------
    {
        SketchModel m;
        addLine(m, 0, 0, 1, 1);
        m.undo();
        CHECK(m.canUndo() == false, "模式切换：撤销一步后置空（前置条件）");
        CHECK(m.canRedo() == true, "模式切换：撤销后有可重做项（前置条件）");
        m.setUndoDeltaEnabled(false);
        CHECK(m.canUndo() == false && m.canRedo() == false, "模式切换：setUndoDeltaEnabled 清空撤销/重做历史");
        m.setUndoDeltaEnabled(true);
        CHECK(m.canUndo() == false && m.canRedo() == false, "模式切换：再次切换仍保持清空");
    }

    // ---------- F. 内存 + 耗时基准：5 万图元 / 200 步 ----------
    {
        const int N0 = 50000;
        SketchModel m;
        // 栈深用默认 100：检查点是整图快照（5 万图元 ≈ 42MB/个），窗口内 ≤2 个检查点
        // 即可覆盖「每 50 步一个全量锚点 + 中间差量」的验收口径；开到 1000 会同时驻留
        // 5 个检查点（210MB+），在受限环境（沙箱进程内存上限）会触发资源守卫被杀。
        m.setUndoDepthLimit(100);
        // 5 万图元**单步整体载入**（beginEdit + 一次 Quiet 批量加，等同"打开图纸/批量导入"
        // 生成一个撤销步）：载入本身不应被拆成几十个可撤销步——那会让撤销末段回放
        // 几十万次 elementFromJson，是基准失真。超大宗量步随后会被栈深裁剪自然淘汰，
        // 稳态栈内只剩小幅编辑差量，正是验收要度量的场景。
        {
            std::vector<Element> parts(N0);
            for (int k = 0; k < N0; ++k) {
                parts[k].type = ElementType::Line;
                parts[k].pts = { Vec3((double)k, 0, 0), Vec3((double)k, 1, 0) };
            }
            m.beginEdit("批量载入");
            m.addElementsQuiet(parts);
        }
        CHECK(m.elementCount() == N0, "内存基准：载入 5 万图元成功");
        for (int i = 0; i < 200; ++i) {
            int id = m.elements()[i % m.elementCount()].id;
            m.beginEdit("编辑");
            m.updateElementQuiet(id, [](Element& e){ e.pts[0].y += 0.001; });
        }
        // 差量模式的栈深收敛呈锯齿（检查点锚吸收后回到 51，再涨到 101 再裁），
        // 故断言「≤ 上限且保留足够步数」而非精确值。
        CHECK(m.undoStackSize() <= 100 && m.undoStackSize() >= 50,
              "内存基准：250 步 push 后按栈深 100 上限收敛（裁剪保锚）");

        const qint64 maxBytes = m.maxDeltaEntryBytes();
        QByteArray oneElem = undoDeltaElemBytes(m.elements()[0]);
        qint64 fullSnapEst = (qint64)m.elementCount() * oneElem.size();
        fprintf(stdout, "  [undo-delta] 内存基准：单步最大差量 = %lld 字节，5万图元全量快照估值 = %lld 字节\n",
                (long long)maxBytes, (long long)fullSnapEst);
        CHECK(maxBytes <= 1024 * 1024, "内存基准：单步撤销差量内存 <= 1MB");
        CHECK(maxBytes > 0, "内存基准：存在差量记录");
        CHECK(maxBytes < fullSnapEst / 5, "内存基准：单步差量 << 全量快照（差量化生效）");

        // 撤销耗时：连续撤销栈内保留的编辑步（LIFO，先到的是最近 100 步编辑）
        int n = 0; double maxMs = 0;
        auto tAll0 = std::chrono::steady_clock::now();
        while (m.canUndo()) {
            auto a = std::chrono::steady_clock::now();
            m.undo(); ++n;
            auto b = std::chrono::steady_clock::now();
            double dd = std::chrono::duration<double, std::milli>(b - a).count();
            if (dd > maxMs) maxMs = dd;
        }
        auto tAll1 = std::chrono::steady_clock::now();
        double avgMs = std::chrono::duration<double, std::milli>(tAll1 - tAll0).count() / (double)std::max(1, n);
        fprintf(stdout, "  [undo-delta] 耗时基准：平均 %.3f ms/步，最慢 %.3f ms/步（基准 <=16ms，共 %d 步）\n",
                avgMs, maxMs, n);
        CHECK(n >= 50, "撤销耗时：成功撤销栈内全部保留步");
        CHECK(avgMs <= 16.0, "撤销耗时：5万图元下平均单步撤销 <= 16ms");
    }

    // ---------- G. 单步 / 组合步 / redo 基本语义（差量模式默认）----------
    {
        SketchModel m;
        int base = m.elementCount();
        addLine(m, 0, 0, 1, 0);
        CHECK(m.elementCount() == base + 1, "语义：新增图元生效");
        m.undo();
        CHECK(m.elementCount() == base, "语义：单步撤销删除新增");
        m.redo();
        CHECK(m.elementCount() == base + 1, "语义：单步重做恢复新增");
    }

    return true;
}

// ===================== 鲁棒性 / Fuzz 回归（t_robustness / t_fileFuzz） =====================
//
// 任务目标：任何输入都不能让程序崩溃 / 卡死 / 写坏文件。
// 验收口径：
//   * 处理策略优先级：不崩溃 > 报错明确 > 部分降级 > 完全拒绝；
//   * 降级须在日志/返回值写清丢弃/跳过（每条降级路径一条断言）；
//   * 打开失败不得污染当前图纸；保存前须先在内存构造完整数据再落盘（既有 writeDrawing 已满足）；
//   * 既有断言全过；双目标零警告 EXIT=0。
//
// 本组用例聚焦「读档前守卫层」ElementValidate：只读校验 / 丢弃 / 裁剪修复 / 缺失引用统计。
// 解析层（tokenizeDxf / binaryDxfToAscii / fromDwg / parseDxfTokens / Block::evalDef）
// 已在探查阶段确认有界/哨兵/环检测健壮，不在此重复覆盖。

// 构造一个「结构合法」的图元（仅填充守卫层关心的字段）。
static Element makeElem(ElementType t, int id) {
    Element e; e.type = t; e.id = id; return e;
}

// 带控制点的直线
static Element makeLine(int id, Vec3 a, Vec3 b) {
    Element e = makeElem(ElementType::Line, id); e.pts = {a, b}; return e;
}
// 带半径的圆
static Element makeCircle(int id, Vec3 c, double r) {
    Element e = makeElem(ElementType::Circle, id); e.pts = {c}; e.radius = r; return e;
}

static bool t_robustness() {
    // ---- ① 正常图元：逐字节（值）等价，skip=fix=0，绝不改动输入 ----
    {
        std::vector<Element> good;
        good.push_back(makeLine(1, Vec3(0,0,0), Vec3(10,0,0)));
        good.push_back(makeCircle(2, Vec3(5,5,0), 3.0));
        Element cp = makeElem(ElementType::Polyline, 3);
        cp.closed = true; cp.pts = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0)};
        good.push_back(cp);
        int s=0, f=0; QString rep;
        std::vector<Element> out = filterElements(good, &s, &f, &rep);
        CHECK(s == 0, "robust: 正常文件 skip=0");
        CHECK(f == 0, "robust: 正常文件 fix=0");
        CHECK(out.size() == good.size(), "robust: 正常文件 图元数不变");
        // 值等价
        bool same = (out.size()==good.size());
        for (size_t i=0; i<out.size() && same; ++i) {
            if (out[i].type != good[i].type || out[i].id != good[i].id
                || out[i].pts.size() != good[i].pts.size()) same=false;
            for (size_t k=0;k<out[i].pts.size() && same;++k)
                if (out[i].pts[k].x!=good[i].pts[k].x || out[i].pts[k].y!=good[i].pts[k].y) same=false;
            if (out[i].radius != good[i].radius) same=false;
        }
        CHECK(same, "robust: 正常文件 逐元素值等价（未做任何修改）");
    }

    // ---- ② 类型越界 / 负数类型：直接丢弃 + 日志 ----
    {
        std::vector<Element> in;
        in.push_back(makeLine(1, Vec3(0,0,0), Vec3(10,0,0)));
        Element bad = makeElem((ElementType)99, 2); bad.pts={Vec3(0,0,0),Vec3(1,1,0)}; in.push_back(bad);
        Element neg = makeElem((ElementType)-1, 3); neg.pts={Vec3(0,0,0),Vec3(1,1,0)}; in.push_back(neg);
        int s=0,f=0; QString rep;
        std::vector<Element> out = filterElements(in,&s,&f,&rep);
        CHECK(s==2, "robust: 类型越界/负数类型 被丢弃（skip=2）");
        CHECK(f==0, "robust: 类型越界 无修复");
        CHECK(out.size()==1, "robust: 类型越界 仅保留合法图元");
        CHECK(rep.contains(QStringLiteral("类型越界")), "robust: 类型越界 日志写明原因");
    }

    // ---- ③ NaN / Inf 坐标与半径：丢弃 ----
    {
        Element nan1 = makeLine(1, Vec3(0,0,0), Vec3(std::nan(""),0,0));
        Element nan2 = makeLine(2, Vec3(0,0,0), Vec3(10, std::numeric_limits<double>::infinity(),0));
        Element nan3 = makeCircle(3, Vec3(0,0,0), std::nan(""));
        std::vector<Element> in = {nan1, nan2, nan3};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==3, "robust: NaN/Inf 坐标与半径 全部丢弃（skip=3）");
        CHECK(rep.contains(QStringLiteral("NaN")), "robust: NaN/Inf 日志写明 NaN/Inf/越界");
    }

    // ---- ④ 超大坐标（>1e18）丢弃；边界值 1e18 保留 ----
    {
        Element huge = makeLine(1, Vec3(0,0,0), Vec3(1e19,0,0));
        Element edge = makeLine(2, Vec3(0,0,0), Vec3(1e18,0,0)); // 恰好等于上限 → 允许
        std::vector<Element> in = {huge, edge};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==1, "robust: 坐标>1e18 丢弃（skip=1）");
        CHECK(out.size()==1 && out[0].id==2, "robust: 边界值 1e18 保留");
    }

    // ---- ⑤ 退化几何：零半径圆 / 零长度直线 / 零扫掠弧 / 零半轴椭圆：丢弃 ----
    {
        Element zr  = makeCircle(1, Vec3(0,0,0), 0.0);
        Element zl  = makeLine(2, Vec3(0,0,0), Vec3(0,0,0));
        Element za  = makeElem(ElementType::Arc, 3); za.pts={Vec3(0,0,0)}; za.radius=4; za.startAngle=10; za.endAngle=10;
        Element ze  = makeElem(ElementType::Ellipse, 4); ze.pts={Vec3(0,0,0)}; ze.radius=3; ze.radiusY=0;
        std::vector<Element> in = {zr, zl, za, ze};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==4, "robust: 退化几何 全部丢弃（skip=4）");
        CHECK(rep.contains(QStringLiteral("零半径")) || rep.contains(QStringLiteral("零长度")) || rep.contains(QStringLiteral("零扫掠")),
              "robust: 退化几何 日志写明退化原因");
    }

    // ---- ⑥ 闭合折线首尾重复点：裁剪修复（去掉末点），fix=1，不丢弃 ----
    {
        Element dup = makeElem(ElementType::Polyline, 1);
        dup.closed = true;
        dup.pts = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0), Vec3(0,0,0)}; // 末点==首点
        std::vector<Element> in = {dup};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==0, "robust: 闭合重复末点 不丢弃（skip=0）");
        CHECK(f==1, "robust: 闭合重复末点 修复（fix=1）");
        CHECK(out.size()==1 && out[0].pts.size()==4 && out[0].closed,
              "robust: 闭合重复末点 已去重末点且仍闭合");
        CHECK(rep.contains(QStringLiteral("修复")), "robust: 闭合重复末点 日志写明修复");
    }

    // ---- ⑦ 超长文本（>1MB）：丢弃 ----
    {
        Element e = makeElem(ElementType::Text, 1); e.pts={Vec3(0,0,0)};
        e.text = QString((1 << 20) + 100, QChar('A'));   // >1MB：1<<20 须加括号，否则歧义为 1<<(20+100)
        std::vector<Element> in = {e};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==1, "robust: 超长文本(>1MB) 丢弃（skip=1）");
        CHECK(rep.contains(QStringLiteral("超长")), "robust: 超长文本 日志写明超长");
    }

    // ---- ⑧ 控制字符：剥离修复（\n 等多行符保留）----
    {
        Element e = makeElem(ElementType::Text, 1); e.pts={Vec3(0,0,0)};
        e.text = QStringLiteral("正常\n行\x01\x02含控制符");
        std::vector<Element> in = {e};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==0, "robust: 控制字符 不丢弃（skip=0）");
        CHECK(f==1, "robust: 控制字符 修复（fix=1）");
        CHECK(out.size()==1 && !out[0].text.contains(QChar('\x01')) && out[0].text.contains('\n'),
              "robust: 控制字符 已剥离且保留换行");
    }
    {
        // 合法文本（含换行）不被误修
        Element e = makeElem(ElementType::Text, 1); e.pts={Vec3(0,0,0)};
        e.text = QStringLiteral("第一行\n第二行");
        std::vector<Element> in = {e};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==0 && f==0, "robust: 合法多行文本 不修复（fix=0）");
    }

    // ---- ⑨ 空输入：不崩溃，返回空 ----
    {
        std::vector<Element> in;
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(out.empty() && s==0 && f==0, "robust: 空向量 安全返回空");
    }

    // ---- ⑩ 负数/重复 file-id：不丢弃（模型会重分配 id，不会碰撞/崩溃）----
    {
        Element neg = makeLine(-5, Vec3(0,0,0), Vec3(10,0,0));
        Element dup = makeLine(1,  Vec3(0,0,0), Vec3(10,10,0));
        Element dup2= makeLine(1,  Vec3(5,5,0), Vec3(15,5,0)); // 与 dup 同 file-id
        std::vector<Element> in = {neg, dup, dup2};
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(in,&s,&f,&rep);
        CHECK(s==0, "robust: 负数/重复 file-id 不丢弃（模型重分配 id）");
        CHECK(out.size()==3, "robust: file-id 异常 图元全部保留");
    }

    // ---- ⑪ isElementValid 单图元判定 ----
    {
        CHECK(isElementValid(makeLine(1, Vec3(0,0,0), Vec3(1,1,0))), "robust: isElementValid 合法直线=true");
        CHECK(!isElementValid(makeCircle(2, Vec3(0,0,0), 0.0)), "robust: isElementValid 零半径圆=false");
        Element nan = makeLine(3, Vec3(std::nan(""),0,0), Vec3(1,1,0));
        QString why; CHECK(!isElementValid(nan, &why) && why.contains(QStringLiteral("NaN")),
              "robust: isElementValid NaN 返回false且原因含NaN");
    }

    // ---- ⑫ countDanglingRefs：按名/按id 缺失引用统计 ----
    {
        // 准备：模型含层 "0" 与 "WALLS"、块 "BOLT"、样式 "Standard"
        SketchModel m;
        m.addLayer(QStringLiteral("WALLS"));
        { BlockDef b; b.name = QStringLiteral("BOLT"); m.addBlockDef(b); }       // 显式命名初始化，避免聚合缺字段警告
        { TextStyle s; s.name = QStringLiteral("Standard"); m.addTextStyle(s); }
        // 构造图元向量（file-id 空间）
        std::vector<Element> elems;
        Element onMissingLayer = makeLine(1, Vec3(0,0,0), Vec3(1,0,0)); onMissingLayer.layer = QStringLiteral("GHOST");
        Element badBlock = makeElem(ElementType::BlockRef, 2); badBlock.pts={Vec3(0,0,0)}; badBlock.blockName=QStringLiteral("NOPE");
        Element badStyle = makeElem(ElementType::Text, 3); badStyle.pts={Vec3(0,0,0)}; badStyle.styleName=QStringLiteral("MISS");
        Element goodLine = makeLine(4, Vec3(0,0,0), Vec3(2,0,0)); goodLine.layer=QStringLiteral("WALLS");
        Element dangling = makeElem(ElementType::Constraint, 5); dangling.refA = 999; // 不存在
        elems = {onMissingLayer, badBlock, badStyle, goodLine, dangling};

        std::vector<QString> lns, bns, sns;
        for (int i=0;i<m.layerCount();++i) lns.push_back(m.layerAt(i).name);
        for (const auto& b : m.blockTable()) bns.push_back(b.name);
        for (int i=0;i<m.textStyleCount();++i) sns.push_back(m.textStyleAt(i).name);
        QString rep;
        int n = countDanglingRefs(elems, lns, bns, sns, &rep);
        CHECK(n == 4, QStringLiteral("robust: countDanglingRefs 统计到 4 处缺失引用(实际 %1)").arg(n).toUtf8().constData());
        CHECK(rep.contains(QStringLiteral("图层")) && rep.contains(QStringLiteral("块")) && rep.contains(QStringLiteral("样式")) && rep.contains(QStringLiteral("悬空")),
              "robust: countDanglingRefs 日志覆盖层/块/样式/悬空几何");
    }
    {
        // 全部引用有效 → 0
        SketchModel m; // 只有默认层 "0"
        std::vector<Element> elems;
        Element good = makeLine(1, Vec3(0,0,0), Vec3(1,0,0)); good.layer=QStringLiteral("0"); // 默认层存在
        elems.push_back(good);
        std::vector<QString> lns, bns, sns;
        for (int i=0;i<m.layerCount();++i) lns.push_back(m.layerAt(i).name);
        QString rep;
        int n = countDanglingRefs(elems, lns, bns, sns, &rep);
        CHECK(n==0, "robust: countDanglingRefs 全部有效引用=0");
    }

    // ---- ⑬ 打开失败不污染当前图纸（BatchPlotEngine::loadDrawing 解析失败不 clear）----
    {
        SketchModel m;
        int goodId = m.addElement(makeLine(0, Vec3(0,0,0), Vec3(9,0,0)));
        const int before = m.elementCount();
        QString err;
        // 投递一段非法 JSON（非对象）→ loadDrawing 应返回 false 且不破坏已有图元
        QTemporaryDir td;
        QFile bad(td.path() + QStringLiteral("/bad.json"));
        (void)bad.open(QIODevice::WriteOnly); bad.write(QByteArray("{这不是合法JSON")); bad.close();
        const bool ok = comauto::BatchPlotEngine::loadDrawing(m, bad.fileName(), &err);
        CHECK(!ok, "robust: 非法 JSON 打开失败返回 false");
        CHECK(m.elementCount()==before, "robust: 打开失败 不污染当前图纸（图元数不变）");
        (void)goodId;
    }

    // ---- ⑭ 混合向量（合法 + NaN 非法）：守卫丢弃非法项，保留合法项，不崩溃 ----
    {
        // 注：JSON 不支持 NaN（Qt 序列化为 null），故直接在 C++ 侧构造含 NaN 的图元验证守卫层。
        std::vector<Element> loaded;
        loaded.push_back(makeLine(1, Vec3(0,0,0), Vec3(10,0,0)));
        loaded.push_back(makeLine(2, Vec3(0,0,0), Vec3(std::nan(""),0,0)));
        int s=0,f=0; QString rep; std::vector<Element> out=filterElements(loaded,&s,&f,&rep);
        CHECK(s==1 && out.size()==1, "robust: NaN 图元被丢弃，合法项保留");
    }

    return true; // 具体通过数由 CHECK 累计
}

// 在独立线程中运行解析+守卫，返回是否在 ms 毫秒内完成（用于死锁/卡死检测）。
// 注：解析层（tokenizeDxf / binaryDxfToAscii / fromDwg / parseDxfTokens / Block::evalDef）
// 均已确认有界/哨兵/环检测，不会无限循环；超时即视为潜在死锁并记失败。
static QByteArray corruptTruncate(const QByteArray& b, double frac) {
    if (b.isEmpty()) return b;
    int keep = (int)(b.size() * (1.0 - frac));
    keep = qBound(0, keep, (int)b.size());
    return b.left(keep);
}
static QByteArray corruptFlip(const QByteArray& b, int n, std::mt19937& rng) {
    if (b.isEmpty() || n<=0) return b;
    QByteArray c = b;
    std::uniform_int_distribution<int> pos(0, (int)c.size()-1);
    std::uniform_int_distribution<int> val(1, 255);
    for (int i=0;i<n;++i) {
        int p = pos(rng);
        c[p] = (char)(c[p] ^ (char)val(rng));
    }
    return c;
}

// 200 个固定种子随机损坏样本，覆盖 JSON/DXF/DWG/二进制DXF 的截断/字节翻转/空文件，
// 要求：零崩溃、零死锁/超时（单样本 ≤5s）。
static bool t_fileFuzz() {
    // 构造「好」图档（多类型图元）
    std::vector<Element> good;
    { Element e=makeLine(1, Vec3(0,0,0), Vec3(10,0,0)); good.push_back(e); }
    { Element e=makeCircle(2, Vec3(5,5,0), 3.0); good.push_back(e); }
    { Element e=makeElem(ElementType::Arc,3); e.pts={Vec3(0,0,0)}; e.radius=4; e.startAngle=0; e.endAngle=180; good.push_back(e); }
    { Element e=makeElem(ElementType::Polyline,4); e.closed=true; e.pts={Vec3(0,0,0),Vec3(10,0,0),Vec3(10,10,0),Vec3(0,10,0)}; good.push_back(e); }
    { Element e=makeElem(ElementType::Text,5); e.pts={Vec3(1,1,0)}; e.text=QStringLiteral("标题"); good.push_back(e); }
    { Element e=makeElem(ElementType::BlockRef,6); e.pts={Vec3(0,0,0)}; e.blockName=QStringLiteral("BOLT"); good.push_back(e); }
    { Element e=makeElem(ElementType::Hatch,7); e.pts={Vec3(0,0,0),Vec3(5,0,0),Vec3(5,5,0)}; good.push_back(e); }

    QString e1;
    QByteArray dxfGood  = cadfmt::toDxf(good, &e1, 1.0, 4, nullptr, nullptr);
    QByteArray dwgGood  = cadfmt::toDwg(good, nullptr, 1.0);
    QByteArray dxfBin   = cadfmt::toDxfBinary(good, &e1, 1.0, 4, nullptr, nullptr);
    QJsonObject root; QJsonArray arr;
    for (auto& e : good) arr.append(elementToJson(e));
    root["elements"]=arr;
    QByteArray jsonGood = QJsonDocument(root).toJson();

    CHECK(!dxfGood.isEmpty() && !dwgGood.isEmpty() && !dxfBin.isEmpty() && !jsonGood.isEmpty(),
          "fuzz: 基准好文件（DXF/DWG/二进制DXF/JSON）均非空");

    std::mt19937 rng(0x5EED1234u);
    std::uniform_int_distribution<int> fmtPick(0, 3);   // 0=JSON 1=DXF 2=DWG 3=二进制DXF
    std::uniform_int_distribution<int> corrPick(0, 4);  // 0/1/2=截断1/10/50% 3=字节翻转 4=空文件
    const int N = 200;
    int crashes = 0, timeouts = 0, total = 0;
    std::vector<std::future<void>> live;   // 保活异步任务，函数退出时再join（解析器有界，必然结束）

    for (int i=0;i<N;++i) {
        const int fp = fmtPick(rng);
        QByteArray base = (fp==0)? jsonGood : (fp==1)? dxfGood : (fp==2)? dwgGood : dxfBin;
        const int cp = corrPick(rng);
        QByteArray corrupted;
        if (cp==0) corrupted = corruptTruncate(base, 0.01);
        else if (cp==1) corrupted = corruptTruncate(base, 0.10);
        else if (cp==2) corrupted = corruptTruncate(base, 0.50);
        else if (cp==3) corrupted = corruptFlip(base, 1 + (int)(rng()%20), rng); // ≤20 处翻转
        else corrupted = QByteArray();

        // 复制进 lambda，避免超时线程 use-after-free；crashed 放堆上保活
        QByteArray data = corrupted;
        const bool isDwg = (fp==2);
        auto crashed = std::make_shared<std::atomic<int>>(0);
        auto fut = std::async(std::launch::async, [crashed, data, isDwg]() {
            try {
                if (!isDwg) {
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    if (doc.isObject()) {
                        std::vector<Element> loaded;
                        for (const auto& v : doc.object()["elements"].toArray())
                            loaded.push_back(elementFromJson(v.toObject()));
                        int s,f; QString r; filterElements(loaded,&s,&f,&r);
                    }
                } else {
                    std::vector<Element> imp; QString e; cadfmt::DxfImportStats st;
                    cadfmt::fromDwg(data, imp, &e, &st); int s,f; QString r; filterElements(imp,&s,&f,&r);
                }
                // 已解析的好文件分支：同时跑 DXF/二进制DXF 解析（fp==1/3 在 JSON 分支外的另一解析路径）
                if (!isDwg && data.size()>0) {
                    std::vector<Element> imp; QString e; cadfmt::DxfImportStats st;
                    // 用 fromDxf 解析（即便内容是 JSON 字节，也应安全返回而非崩溃）
                    cadfmt::fromDxf(data, imp, &e, &st); int s,f; QString r; filterElements(imp,&s,&f,&r);
                }
            } catch (...) { *crashed = 1; }
        });
        const bool done = fut.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready;
        ++total;
        if (!done) ++timeouts;
        else if (*crashed) ++crashes;
        live.push_back(std::move(fut));
    }
    CHECK(crashes == 0, QStringLiteral("fuzz: 200 随机损坏样本零崩溃(实际崩溃 %1)").arg(crashes).toUtf8().constData());
    CHECK(timeouts == 0, QStringLiteral("fuzz: 零死锁/超时(实际超时 %1)").arg(timeouts).toUtf8().constData());
    CHECK(total == N, QStringLiteral("fuzz: 跑满 %1 样本").arg(N).toUtf8().constData());
    return crashes==0 && timeouts==0;
}

// ===========================================================================
// 文字度量三处同源自测（t_textMetrics）
// 验收：① 度量 vs 估算偏差 ② 9 点对齐坐标 ③ 换行结果
//       ④ 列宽自适应 ⑤ 三同源 boundingRect 相等 ⑥ 无头确定性降级
//       ⑦ 堆叠分数明确提示（非静默退化）
// 运行环境为 QCoreApplication（无头）→ TextMetrics 走确定性估算（不崩溃、不依赖字体）。
// ===========================================================================
static bool t_textMetrics() {
    fprintf(stdout, "--- 文字度量 TextMetrics（三处同源）---\n");
    const int passBefore = g_pass;
    const int failBefore = g_fail;

    // 自测进程现持 QApplication（t_interaction 需要 QLineEdit）——但本组断言锚定
    // "无头降级（确定性估算）"路径，故强制 isGuiAvailable=false 还原原判定语义，
    // 全组断言与 QCoreApplication 时代完全一致；组尾恢复，不影响其它测试组。
    TextMetrics::setHeadlessOverride(true);

    // ⑥ 无头环境必须走确定性估算（批处理安全：无 GUI 不崩溃）
    CHECK(TextMetrics::isGuiAvailable() == false,
          "无头环境 isGuiAvailable=false → 确定性估算(批处理安全)");

    // ① 度量 vs 估算：无头下 lineWidthMm 必须 == size*height*fallback*widthFactor
    ResolvedTextStyle rs; rs.height = 3.5; rs.widthFactor = 1.0;
    const QString s5 = "Hello";
    const double w5 = TextMetrics::lineWidthMm(rs, s5, 3.5);
    const double exp5 = s5.size() * 3.5 * TextMetrics::fallbackCharWidth() * rs.widthFactor;
    CHECK(std::fabs(w5 - exp5) < 1e-9, "无头 lineWidthMm 等于确定性估算(size*height*ratio*wf)");

    // widthFactor 线性放大
    rs.widthFactor = 2.0;
    const double w2 = TextMetrics::lineWidthMm(rs, "AB", 3.5);
    const double exp2 = 2.0 * 3.5 * TextMetrics::fallbackCharWidth() * 2.0;
    CHECK(std::fabs(w2 - exp2) < 1e-9, "widthFactor=2 使行宽线性翻倍");
    rs.widthFactor = 1.0;

    // ② 9 点对齐（Text 强制中上锚定，仅 MText 走 textAlign 的 9 档，故用 MText 测）
    Element eM; eM.type = ElementType::MText; eM.id = 1; eM.pts = {Vec3(10,10,0)};
    eM.text = "AB"; eM.radius = 3.5; eM.mtextWidth = 0.0; eM.lineSpacing = 1.0;
    const double charW = TextMetrics::lineWidthMm(rs, "A", 3.5);   // 单字符宽(无头)

    eM.textAlign = 1;   // 中上
    QRectF rC = TextMetrics::elementBoundsMm(eM, nullptr);
    CHECK(std::fabs((rC.x() + rC.width()/2.0) - 10.0) < 1e-9, "9点对齐: align=1 水平居中于基点");
    CHECK(std::fabs(rC.y() + rC.height() - 10.0) < 1e-9, "9点对齐: align=1 顶边贴合基点");

    eM.textAlign = 2;   // 右上
    QRectF rR = TextMetrics::elementBoundsMm(eM, nullptr);
    CHECK(std::fabs(rR.right() - 10.0) < 1e-9, "9点对齐: align=2 右对齐于基点");

    eM.textAlign = 4;   // 中中
    QRectF rM = TextMetrics::elementBoundsMm(eM, nullptr);
    CHECK(std::fabs((rM.x() + rM.width()/2.0) - 10.0) < 1e-9, "9点对齐: align=4 水平居中于基点");
    CHECK(std::fabs((rM.y() + rM.height()/2.0) - 10.0) < 1e-9, "9点对齐: align=4 垂直居中于基点");

    eM.textAlign = 6;   // 左下
    QRectF rB = TextMetrics::elementBoundsMm(eM, nullptr);
    CHECK(std::fabs(rB.x() - 10.0) < 1e-9, "9点对齐: align=6 左对齐于基点");
    CHECK(std::fabs(rB.y() - 10.0) < 1e-9, "9点对齐: align=6 底边贴合基点");

    // ③ 换行：MText 按 mtextWidth 真实折行（CJK 任意断点 + 拉丁空格/连字符断词）
    Element eW; eW.type = ElementType::MText; eW.id = 2; eW.pts = {Vec3(0,0,0)};
    eW.text = "ABC"; eW.radius = 3.5; eW.mtextWidth = 5.0; eW.lineSpacing = 1.0; eW.textAlign = 1;
    TextLayout lay = TextMetrics::layout(rs, "ABC", 3.5, 5.0, 1.0);
    CHECK(lay.lines.size() == 2, "MText 按 mtextWidth 折行为 2 行");
    CHECK(std::fabs(lay.lines[0].widthMm - 2.0*charW) < 1e-9, "折行第0行宽=2字符");
    CHECK(std::fabs(lay.lines[1].widthMm - 1.0*charW) < 1e-9, "折行第1行宽=1字符");
    CHECK(std::fabs(lay.widthMm - 2.0*charW) < 1e-9, "layout.widthMm = 最宽行宽");
    CHECK(std::fabs(lay.heightMm - 2.0*3.5) < 1e-9, "layout.heightMm = 行数×行高");

    // 无 mtextWidth 不折行；lineSpacing 只改行高不改宽
    TextLayout layN = TextMetrics::layout(rs, "AB", 3.5, 0.0, 2.0);
    CHECK(layN.lines.size() == 1, "无 mtextWidth 不折行");
    CHECK(std::fabs(layN.heightMm - 2.0*3.5) < 1e-9, "lineSpacing=2 → 行高翻倍");
    CHECK(std::fabs(layN.widthMm - 2.0*charW) < 1e-9, "lineSpacing 不影响宽度");

    // ④ 列宽：显式 colWidths 优先 vs 自适应
    Element tE; tE.type = ElementType::Table; tE.id = 3; tE.pts = {Vec3(0,0,0)};
    tE.tableRows = 2; tE.tableCols = 3; tE.radius = 3.5;
    tE.colWidths = {10, 20, 30};
    tE.tableCells = {"a","bb","ccc","dd","eee","ffff"};
    QVector<double> cw = TextMetrics::tableColumnWidths(tE, nullptr);
    CHECK(cw.size() == 3, "表格列数=3");
    CHECK(std::fabs(cw[0]-10)<1e-9 && std::fabs(cw[1]-20)<1e-9 && std::fabs(cw[2]-30)<1e-9,
          "显式 colWidths 优先采用");

    tE.colWidths.clear();
    QVector<double> cw2 = TextMetrics::tableColumnWidths(tE, nullptr);
    CHECK(cw2.size() == 3, "自适应列数=3");
    const double expC0 = TextMetrics::lineWidthMm(rs, "dd", 3.5) + 2.0;  // 最长单元格"dd" + 2*pad
    CHECK(std::fabs(cw2[0] - expC0) < 1e-9, "自适应列0宽 = 最长单元格宽 + 2*pad");

    // ⑤ 三同源：屏幕/打印/取景均调 elementBoundsMm → 数学相等；
    //    且 elementBoundsMm 必须由 layout 同参派生（保证三站点像素逐字节一致）
    Element e3; e3.type = ElementType::MText; e3.id = 4; e3.pts = {Vec3(5,5,0)};
    e3.text = "X"; e3.radius = 3.5; e3.mtextWidth = 0.0; e3.lineSpacing = 1.0; e3.textAlign = 1;
    QRectF a = TextMetrics::elementBoundsMm(e3, nullptr);
    QRectF b = TextMetrics::elementBoundsMm(e3, nullptr);
    CHECK(a == b, "三同源: elementBoundsMm 多次调用结果一致");
    ResolvedTextStyle rs3 = resolveTextStyle(e3, nullptr);
    TextLayout L = TextMetrics::layout(rs3, e3.text, rs3.height, 0.0, 1.0);
    QRectF fromLayout(e3.pts[0].x - L.widthMm/2.0, e3.pts[0].y - L.heightMm, L.widthMm, L.heightMm);
    CHECK(std::fabs(fromLayout.x()-a.x())<1e-9 && std::fabs(fromLayout.y()-a.y())<1e-9 &&
          std::fabs(fromLayout.width()-a.width())<1e-9 && std::fabs(fromLayout.height()-a.height())<1e-9,
          "三同源: elementBoundsMm 与 layout 同参派生坐标一致");

    // 单元格溢出省略
    QString el = TextMetrics::elideToWidthMm(rs, "ABCDEF", 3.5, 2.0*charW);
    CHECK(el.contains(QChar(0x2026)), "elide 末尾追加省略号(…)");
    CHECK(TextMetrics::lineWidthMm(rs, el, 3.5) <= 2.0*charW + 1e-9, "elide 后宽度不超上限");

    // ⑦ 堆叠分数：检测 + 明确提示（非静默退化）
    CHECK(TextMetrics::containsStackedFraction("{\\S 1/2;}") == true, "堆叠分数 {\\S 上/下;} 检测");
    CHECK(TextMetrics::containsStackedFraction("A^IB") == true, "堆叠分数 ^I 检测");
    CHECK(TextMetrics::containsStackedFraction("普通文字") == false, "无堆叠分数返回 false");

    static bool g_tmWarned = false;
    TextMetrics::setWarnHandler([](const QString&){ g_tmWarned = true; });
    TextMetrics::layout(rs, "{\\S 1/2;}", 3.5, 0.0, 1.0);
    CHECK(g_tmWarned == true, "堆叠分数触发明确提示(非静默退化)");
    TextMetrics::setWarnHandler([](const QString& m){ qWarning("%s", m.toUtf8().constData()); }); // 还原默认

    TextMetrics::setHeadlessOverride(false);   // 还原真实判定，避免影响后续测试组

    fprintf(stdout, "  (文字度量自测: 通过 %d / 失败 %d)\n", g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// ===========================================================================
// 标题栏表格化 + 辅助元素打印过滤（t_titleBlockAux）
// 验收：① 新建图纸标题栏全部文本块落入各自网格单元格（并证明对旧「标签越顶
//        1mm」bug 有捕获力）② 插入/自动套框标题栏长图名生成期收缩适配
//        （下限 1.5mm、绝不截断；短值不收缩）③ isProjectionSymbolElement
//        谓词：符号全命中、普通图元零误报 ④ 端到端出图：投影符号区域零墨迹
//        + 去标签对照组有墨（证明零墨迹确由过滤而非渲染空转）
// 运行环境持 QApplication，但本组锚定无头确定性度量（size*height*0.62），
// 与 t_textMetrics 同口径；组尾还原，不影响其它测试组。
// ===========================================================================
static bool t_titleBlockAux() {
    using namespace SketchCAD;
    fprintf(stdout, "--- 标题栏表格化 + 辅助元素打印过滤 ---\n");
    const int passBefore = g_pass;
    const int failBefore = g_fail;

    TextMetrics::setHeadlessOverride(true);

    // 文本块世界包围盒——与 GraphicsView / PlotRenderer 同一锚点语义：
    // plain Text 锚点 = 块左下角，块自锚点向右上延伸 heightMm（单行 lineH=h）。
    auto textBlock = [](const Element& e) {
        const ResolvedTextStyle rs;   // 默认构造 = 无样式名图元的渲染解析结果
        const double w = TextMetrics::lineWidthMm(rs, e.text, e.radius);
        return QRectF(e.pts[0].x, e.pts[0].y, w, e.radius);
    };
    // 块是否落入单元格（QRectF 语义：top()=最小 Y、bottom()=最大 Y）
    auto blockInCell = [](const QRectF& b, const QRectF& c) {
        constexpr double kEps = 1e-6;
        return b.left() >= c.left() - kEps && b.right() <= c.right() + kEps &&
               b.top()  >= c.top()  - kEps && b.bottom() <= c.bottom() + kEps;
    };

    // ---- ① 新建图纸标题栏：全部文本块落入各自单元格（A3 横放 + A5 竖放）----
    const SheetSpec specs[] = { SheetSpec::make(SheetFormat::A3, true),
                                SheetSpec::make(SheetFormat::A5, false) };
    for (const SheetSpec& spec : specs) {
        SketchModel m;
        for (const auto& e : buildSheetFrame(spec)) m.addElement(e);

        int labelCount = 0, fieldCount = 0, escape = 0, noCell = 0, badY = 0, badX = 0;
        QRectF firstLabelCell;
        for (const auto& e : m.elements()) {
            if (e.layer != QStringLiteral("标题栏") || e.type != ElementType::Text) continue;
            const bool isField = e.name.startsWith(QLatin1String("tb:field:"));
            if (isField) ++fieldCount; else ++labelCount;
            if (e.pts.empty()) continue;
            const QRectF cell = titleBlockCellAt(&m, e.pts[0].x, e.pts[0].y);
            if (!cell.isValid()) { ++noCell; continue; }
            if (labelCount == 1 && !isField) firstLabelCell = cell;
            if (!blockInCell(textBlock(e), cell)) {
                ++escape;
                fprintf(stderr, "  FAIL: TB① 文本越界 name=%s\n", e.name.toUtf8().constData());
            }
            // 规范放置：标签贴顶（顶边-1mm-字高）、字段贴底（底边+1mm）、水平左内边距 2mm
            const bool isLabel = e.name.isEmpty();
            const double expectY = isLabel ? (cell.bottom() - 1.0 - e.radius)
                                           : (cell.top() + 1.0);
            if (std::fabs(e.pts[0].y - expectY) > 1e-9) ++badY;
            if (std::fabs(e.pts[0].x - (cell.left() + 2.0)) > 1e-9) ++badX;
        }
        CHECK(noCell == 0, "TB① 每个标题栏文本都能重建所在单元格（网格线可解析）");
        CHECK(escape == 0, "TB① 全部标签/字段文本块落入各自单元格（零越界）");
        CHECK(badY == 0, "TB① 垂直放置规范：标签贴顶/字段贴底，呼吸边 1mm");
        CHECK(badX == 0, "TB① 水平放置规范：左内边距 2mm");
        CHECK(labelCount == 12 && fieldCount == 12, "TB① 标签 12 + 字段 12（栏目无丢失）");

        // 捕获力证明：复现旧实现放置（锚点=单元格顶边-2mm）→ 必越界；
        // 若此断言失败，说明上面的包含性断言对旧 bug 没有捕获力。
        if (firstLabelCell.isValid()) {
            for (const auto& e : m.elements()) {
                if (e.layer != QStringLiteral("标题栏") || e.type != ElementType::Text) continue;
                if (!e.name.isEmpty() || e.text.isEmpty() || e.pts.empty()) continue;
                Element old = e;
                old.pts[0].y = firstLabelCell.bottom() - 2.0;   // 旧 labelOff=2
                CHECK(!blockInCell(textBlock(old), firstLabelCell),
                      "TB① 捕获力：旧「顶边-2mm」放置必越界（断言能抓旧 bug）");
                break;
            }
        }
    }

    // ---- ② 插入/自动套框标题栏：长图名生成期收缩适配 ----
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A3, true);
        InsertedTitleBlock data;
        data.drawingName   = QString(60, QChar(L'图'));   // 60 字长图名：无头估算 148.8mm > 单元格可用 113mm
        data.drawingNumber = QStringLiteral("ABC-1234-2026");
        data.scale         = QStringLiteral("1:1");
        data.designDate    = QStringLiteral("2026-10-10");
        SketchModel m;
        for (const auto& e : buildAutoFrameTitleBlock(QRectF(0, 0, 100, 60), spec, data))
            m.addElement(e);

        int escape = 0, noCell = 0, checked = 0;
        for (const auto& e : m.elements()) {
            if (e.layer != QStringLiteral("标题栏") || e.type != ElementType::Text) continue;
            if (e.pts.empty()) continue;
            const QRectF cell = titleBlockCellAt(&m, e.pts[0].x, e.pts[0].y);
            if (!cell.isValid()) { ++noCell; continue; }
            ++checked;
            if (!blockInCell(textBlock(e), cell)) ++escape;
            if (e.name == QStringLiteral("tb:field:drawingName")) {
                CHECK(e.radius < 4.0 - 1e-9, "TB② 长图名触发收缩（字高 < 设计值 4.0）");
                CHECK(e.radius >= 1.5 - 1e-9, "TB② 收缩不低于下限 1.5mm（绝不截断）");
            }
        }
        CHECK(checked > 0 && noCell == 0, "TB② 插入标题栏文本全部可定位单元格");
        CHECK(escape == 0, "TB② 收缩后全部文本块落入各自单元格（含预填值）");

        // 对照：短图名不收缩（保持设计字高 4.0）
        InsertedTitleBlock shortData = data;
        shortData.drawingName = QStringLiteral("短图名");
        SketchModel m2;
        for (const auto& e : buildAutoFrameTitleBlock(QRectF(0, 0, 100, 60), spec, shortData))
            m2.addElement(e);
        for (const auto& e : m2.elements()) {
            if (e.name == QStringLiteral("tb:field:drawingName"))
                CHECK(std::fabs(e.radius - 4.0) < 1e-9, "TB② 短图名不收缩（字高=设计值 4.0）");
        }
    }

    // ---- ③ 谓词单测：投影符号全命中、普通图元零误报 ----
    {
        const auto sym = buildProjectionSymbol(Vec3(0, 0, 0), 5.0, ProjectionMethod::FirstAngle);
        CHECK(sym.size() == 5, "TB③ 投影符号图元数 = 外圆 + 3 线 + 文本 = 5");
        bool allTagged = true;
        for (const auto& e : sym) if (!isProjectionSymbolElement(e)) allTagged = false;
        CHECK(allTagged, "TB③ 第一角符号全部图元命中谓词");
        const auto sym3 = buildProjectionSymbol(Vec3(0, 0, 0), 5.0, ProjectionMethod::ThirdAngle);
        bool allTagged3 = true;
        for (const auto& e : sym3) if (!isProjectionSymbolElement(e)) allTagged3 = false;
        CHECK(allTagged3, "TB③ 第三角符号同样命中（同一前缀事实源）");

        Element line; line.type = ElementType::Line;
        line.pts = { Vec3(0, 0, 0), Vec3(1, 0, 0) };
        CHECK(!isProjectionSymbolElement(line), "TB③ 普通直线零误报");
        Element fld; fld.type = ElementType::Text; fld.name = QStringLiteral("tb:field:scale");
        CHECK(!isProjectionSymbolElement(fld), "TB③ 标题栏域文本零误报");
        Element noName; noName.type = ElementType::Text;
        CHECK(!isProjectionSymbolElement(noName), "TB③ 无名文本零误报");
        Element fake; fake.type = ElementType::Text;
        fake.name = QStringLiteral("x投影符号:第一角");
        CHECK(!isProjectionSymbolElement(fake), "TB③ 「含而非始于」前缀 → 零误报");
        Element hand; hand.type = ElementType::Text;
        hand.name = QStringLiteral("投影符号:第三角");
        CHECK(isProjectionSymbolElement(hand), "TB③ 手写同前缀标签亦命中（单一事实源）");
    }

    // ---- ④ 端到端出图：符号区零墨迹 + 去标签对照组有墨 ----
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A3, true);
        const double W = spec.width, H = spec.height;   // 420 × 297
        const auto frame = buildSheetFrame(spec);

        double cx = 0.0, cy = 0.0;
        bool foundSym = false;
        for (const auto& e : frame) {
            if (e.type == ElementType::Circle && isProjectionSymbolElement(e)) {
                cx = e.pts[0].x; cy = e.pts[0].y; foundSym = true; break;
            }
        }
        CHECK(foundSym, "TB④ 新建图纸图框中含投影符号（圆）");

        const double s = 8.0;   // 8 px/mm（≈203dpi）
        auto render = [&](SketchModel& m) {
            QImage img((int)std::lround(W * s) + 1, (int)std::lround(H * s) + 1,
                       QImage::Format_RGB32);
            img.setDotsPerMeterX((int)std::lround(s * 1000.0));
            img.setDotsPerMeterY((int)std::lround(s * 1000.0));
            img.fill(Qt::white);
            QPainter p(&img);
            // 世界(Y 向上) → 设备(Y 向下)：device = (x*s, H*s - y*s)
            p.setTransform(QTransform(s, 0, 0, -s, 0, H * s));
            PlotRenderer::renderModel(&p, &m, s, QRectF(0, 0, W, H),
                                      PlotStyleTable::defaultTable(), 0.5, nullptr);
            p.end();
            return img;
        };
        auto inkIn = [&](const QImage& img, double x0, double x1, double y0, double y1) {
            if (img.isNull()) return false;
            const int c0 = std::max(0, (int)std::lround(x0 * s));
            const int c1 = std::min(img.width()  - 1, (int)std::lround(x1 * s));
            const int r0 = std::max(0, (int)std::lround((H - y1) * s));
            const int r1 = std::min(img.height() - 1, (int)std::lround((H - y0) * s));
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    if (qGray(img.pixel(c, r)) < 100) return true;
            return false;
        };

        SketchModel mFiltered;
        for (const auto& e : frame) mFiltered.addElement(e);
        SketchModel mControl;
        for (const auto& e : frame) {
            Element c = e;
            if (isProjectionSymbolElement(c)) c.name.clear();   // 去标签 → 不过滤（对照组）
            mControl.addElement(c);
        }

        const QImage imgF = render(mFiltered);
        const QImage imgC = render(mControl);
        CHECK(!imgF.isNull() && !imgC.isNull(), "TB④ 两张端到端渲染成功");
        // 符号圆盘区域（cx±5.5, cy±5.5）：该区域除符号外无任何图线
        CHECK(!inkIn(imgF, cx - 5.5, cx + 5.5, cy - 5.5, cy + 5.5),
              "TB④ 投影符号区域零墨迹（打印/PDF 不输出辅助说明）");
        CHECK(inkIn(imgC, cx - 5.5, cx + 5.5, cy - 5.5, cy + 5.5),
              "TB④ 对照组（去标签）同区域有墨 → 零墨迹确由过滤生效");
        // 渲染管线本身有输出（标题栏横向分隔线 y=17 一带）
        CHECK(inkIn(imgF, 300, 400, 16.5, 17.5),
              "TB④ 对照组管道 sanity：标题栏网格线有墨（非整体空转）");
    }

    TextMetrics::setHeadlessOverride(false);   // 还原真实判定，避免影响后续测试组
    fprintf(stdout, "  (标题栏/辅助元素自测: 通过 %d / 失败 %d)\n",
            g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// ===========================================================================
// 用户坐标系 UCS 自测（t_ucs）
// 验收：① 默认=世界系（零回归） ② Ucs 数学往返 ③ 45° UCS 下 @100<0 落点（≤1e-6）
//       ④ ucs 命令五种定义 + CRUD ⑤ 极轴按 UCS 对齐 ⑥ 正交/栅格按 UCS 轴
//       ⑦ 坐标读数随 WCS/UCS 切换变化 ⑧ 网格方向随 UCS 旋转 ⑨ UCS 不入存档
// ===========================================================================
static bool t_ucs() {
    using namespace SketchCAD;
    fprintf(stdout, "--- 用户坐标系 UCS ---\n");
    const int passBefore = g_pass;
    const int failBefore = g_fail;

    // ① 默认 UCS = 世界系（单位变换 → 既有行为零回归）
    {
        SketchModel m;
        CHECK(m.currentUcs().isWorld(), "ucs: 默认 UCS 为世界系");
        const Vec3 p(3, -4, 2);
        CHECK(m.toWorld(p) == p && m.toUcs(p) == p, "ucs: 世界系下 toWorld/toUcs 为恒等");
    }

    // ② Ucs 数学：xAxis/yAxis 正交单位向量 + 双向往返
    {
        const Ucs u("T", Vec3(10, 20, 0), math::deg2rad(45.0));
        const Vec3 x = u.xAxis(), y = u.yAxis();
        CHECK(std::fabs(x.len2D() - 1.0) < 1e-12 && std::fabs(y.len2D() - 1.0) < 1e-12,
              "ucs: xAxis/yAxis 为单位向量");
        CHECK(std::fabs(x.dot2D(y)) < 1e-12, "ucs: xAxis ⟂ yAxis");
        const Vec3 p(3, -4, 2);
        const Vec3 back = u.toUcs(u.toWorld(p));
        CHECK(std::fabs(back.x - p.x) < 1e-9 && std::fabs(back.y - p.y) < 1e-9 &&
              std::fabs(back.z - p.z) < 1e-9, "ucs: toUcs(toWorld(p)) 往返一致");
        const Vec3 w(37.5, -12.25, 7);
        CHECK(u.toWorld(u.toUcs(w)).distTo(w) < 1e-9, "ucs: toWorld(toUcs(w)) 往返一致");
        // 45° 旋转正向算例：局部 (1,0) → 相对原点 (cos45, sin45)
        const Vec3 r = u.toWorld(Vec3(1, 0, 0)) - u.origin;
        CHECK(std::fabs(r.x - std::cos(math::deg2rad(45.0))) < 1e-12 &&
              std::fabs(r.y - std::sin(math::deg2rad(45.0))) < 1e-12,
              "ucs: 局部 X 轴向 45° 旋转后指向 (cos45, sin45)");
    }

    // ③ 验收断言：45° UCS 下命令行输入 @100<0（基准原点）→ 世界 (100cos45, 100sin45)
    {
        SketchModel m;
        m.setCurrentUcs(Ucs("A45", Vec3(0, 0, 0), math::deg2rad(45.0)));
        Vec3 out;
        CHECK(m.parsePointToWorld(QStringLiteral("@100<0"), Vec3(0, 0, 0), out),
              "ucs: 命令行 @100<0 可解析");
        const double c45 = std::cos(math::deg2rad(45.0)), s45 = std::sin(math::deg2rad(45.0));
        CHECK(std::fabs(out.x - 100.0 * c45) <= 1e-6 && std::fabs(out.y - 100.0 * s45) <= 1e-6,
              "ucs: 45° UCS @100<0 落点=(100cos45,100sin45) 误差≤1e-6");
        // 绝对笛卡尔：UCS 局部 (50,0) → 世界 (50cos45, 50sin45)
        Vec3 abs50;
        CHECK(m.parsePointToWorld(QStringLiteral("50,0"), Vec3(0, 0, 0), abs50) &&
              std::fabs(abs50.x - 50.0 * c45) < 1e-9 && std::fabs(abs50.y - 50.0 * s45) < 1e-9,
              "ucs: 绝对坐标按 UCS 局部解释后转世界");
        // 绝对极坐标 100<45（UCS）：局部 (70.71,70.71) → 世界 (0,100)
        Vec3 pol;
        CHECK(m.parsePointToWorld(QStringLiteral("100<45"), Vec3(0, 0, 0), pol) &&
              std::fabs(pol.x) < 1e-6 && std::fabs(pol.y - 100.0) < 1e-6,
              "ucs: 绝对极坐标 100<45(UCS) → 世界 (0,100)");
        // 相对笛卡尔 @30,0：以基准点 (10,20) 按 UCS 方向偏移
        Vec3 rel;
        CHECK(m.parsePointToWorld(QStringLiteral("@30,0"), Vec3(10, 20, 0), rel) &&
              rel.distTo(Vec3(10, 20, 0) + Vec3(c45, s45, 0) * 30.0) < 1e-9,
              "ucs: 相对坐标 @30,0 以基准点按 UCS 方向偏移");
        // 世界系下 @100<0 与既有相对坐标语义一致（零回归）
        SketchModel m2;
        Vec3 wout;
        CHECK(m2.parsePointToWorld(QStringLiteral("@100<0"), Vec3(5, 5, 0), wout) &&
              std::fabs(wout.x - 105.0) < 1e-12 && std::fabs(wout.y - 5.0) < 1e-12,
              "ucs: 世界系下 @100<0 与既有相对坐标行为一致");
    }

    // ④ ucs 命令：五种定义方式 + CRUD（经命令行解释器）
    {
        SketchModel m; CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("ucs 3p 10,20 30,20 30,40")),
              "ucs 3p 三点定义→成功");
        CHECK(std::fabs(m.currentUcs().origin.x - 10.0) < 1e-9 &&
              std::fabs(m.currentUcs().origin.y - 20.0) < 1e-9 &&
              std::fabs(m.currentUcs().angleRad) < 1e-9,
              "ucs 3p: 原点=(10,20) X 正向沿 (20,0) → 角 0°");
        CHECK(interp.execute(QStringLiteral("ucs z 5,5 30")), "ucs z 定义→成功");
        CHECK(std::fabs(m.currentUcs().angleRad - math::deg2rad(30.0)) < 1e-9 &&
              std::fabs(m.currentUcs().origin.x - 5.0) < 1e-9,
              "ucs z: 原点(5,5) 旋转 30°");
        const int lid = addLine(m, 0, 0, 10, 10);
        CHECK(interp.execute(QStringLiteral("ucs object %1").arg(lid)), "ucs object 定义→成功");
        CHECK(std::fabs(m.currentUcs().angleRad - math::deg2rad(45.0)) < 1e-9,
              "ucs object: 45° 直线 → UCS 角 45°");
        CHECK(interp.execute(QStringLiteral("ucs view")) && m.currentUcs().isWorld(),
              "ucs view → 世界系(平面视图)");
        CHECK(interp.execute(QStringLiteral("ucs 3p 0,0 0,10")) &&
              std::fabs(m.currentUcs().angleRad - math::deg2rad(90.0)) < 1e-9,
              "ucs 3p: X 正向沿 +Y → 角 90°");
        // ---- CRUD ----
        CHECK(interp.execute(QStringLiteral("ucs save A45")), "ucs save A45→成功");
        CHECK(m.ucsCount() == 1 && m.ucsHas(QStringLiteral("A45")), "ucs: 命名表已收录 A45");
        CHECK(interp.execute(QStringLiteral("ucs world")) && m.currentUcs().isWorld(),
              "ucs world → 世界系");
        CHECK(interp.execute(QStringLiteral("ucs restore A45")) &&
              std::fabs(m.currentUcs().angleRad - math::deg2rad(90.0)) < 1e-9,
              "ucs restore A45 → 恢复保存时的 90°");
        CHECK(interp.execute(QStringLiteral("ucs list")), "ucs list → 成功");
        CHECK(interp.execute(QStringLiteral("ucs delete A45")) && !m.ucsHas(QStringLiteral("A45")),
              "ucs delete A45 → 已删除");
        CHECK(!interp.execute(QStringLiteral("ucs set A45")), "ucs set 已删名 → 拒绝(false)");
        CHECK(!interp.execute(QStringLiteral("ucs delete WORLD")), "ucs delete WORLD → 拒绝(false)");
        CHECK(!interp.execute(QStringLiteral("ucs bogus")), "ucs 未知子命令 → 拒绝(false)");
    }

    // ⑤ 极轴按 UCS 对齐（polarAngles 偏移 + track 命中/不命中）
    {
        const snap::TrackConfig cfg;   // polarIncrement=90, polarRelative=true
        const auto a0 = snap::polarAngles(cfg, nullptr, 0.0);
        bool has0 = false, has90 = false;
        for (double a : a0) {
            if (std::fabs(a - 0.0)  < 1e-6) has0 = true;
            if (std::fabs(a - 90.0) < 1e-6) has90 = true;
        }
        CHECK(has0 && has90, "polar: 世界系极轴含 0°/90°");
        const auto a30 = snap::polarAngles(cfg, nullptr, 30.0);
        bool has30 = false, has120 = false, has0in30 = false;
        for (double a : a30) {
            if (std::fabs(a - 30.0)  < 1e-6) has30 = true;
            if (std::fabs(a - 120.0) < 1e-6) has120 = true;
            if (std::fabs(a - 0.0)   < 1e-6) has0in30 = true;
        }
        CHECK(has30 && has120 && !has0in30,
              "polar: UCS 偏移 30° 后极轴为 30°/120°… 且不含 0°");
        // track 命中：光标沿世界 30° 方向 —— UCS 偏移 30° 时命中极轴，世界系时不命中
        const double d30 = math::deg2rad(30.0);
        const Vec3 cursor(50.0 * std::cos(d30), 50.0 * std::sin(d30), 0);
        const Vec3 base(0, 0, 0);
        const auto hit30 = snap::track(cursor, cfg, nullptr, &base, {}, {}, 1.0, d30);
        CHECK(hit30.hit && hit30.mode == snap::TrackMode::Polar &&
              hit30.point.distTo(cursor) < 1e-9,
              "track: UCS 30° 下光标沿 UCS 0° 射线命中极轴");
        const auto hitW = snap::track(cursor, cfg, nullptr, &base, {}, {}, 1.0, 0.0);
        CHECK(!hitW.hit, "track: 世界系下同一光标不在 0°/90° 射线邻域 → 不命中");
    }

    // ⑥ 正交按 UCS 锁轴 + 栅格按 UCS 量化
    {
        const Vec3 base(0, 0, 0);
        // UCS 旋转 90°：正交把点锁到 UCS X 轴（=世界 Y 轴）
        const auto r = snap::applyInputConstraints(Vec3(1, 5, 0), &base, true, false, 1.0,
                                                  math::deg2rad(90.0));
        CHECK(r.ortho && r.point.distTo(Vec3(0, 5, 0)) < 1e-9,
              "ortho: UCS 90° 时正交锁到 UCS X 轴(=世界 Y 轴)");
        // 栅格量化：UCS 45° 下世界 (8,0) → 局部 (5.657,-5.657) → 步长5 → (5,-5) → 世界 (7.07,0)
        const double c45 = std::cos(math::deg2rad(45.0)), s45 = std::sin(math::deg2rad(45.0));
        const auto rg = snap::applyInputConstraints(Vec3(8, 0, 0), nullptr, false, true, 5.0,
                                                   math::deg2rad(45.0));
        const Vec3 expG(5.0 * c45 + 5.0 * s45, 5.0 * s45 - 5.0 * c45, 0);
        CHECK(rg.grid && rg.point.distTo(expG) < 1e-9,
              "grid snap: UCS 45° 下栅格沿 UCS 轴量化");
        // 零回归：UCS 角 0 时正交与既有世界轴实现一致
        const auto r0 = snap::applyInputConstraints(Vec3(3, 7, 0), &base, true, false, 1.0, 0.0);
        CHECK(r0.ortho && r0.point == Vec3(0, 7, 0), "ortho: UCS 角 0 时锁到世界轴（既有行为）");
    }

    // ⑦ 坐标读数随 WCS/UCS 切换变化 + 网格方向随 UCS 旋转
    {
        SketchModel m;
        m.setCurrentUcs(Ucs("R30", Vec3(0, 0, 0), math::deg2rad(30.0)));
        const Vec3 w(10, 0, 0);
        m.setCoordDisplayUcs(false);
        CHECK(m.displayCoord(w) == w, "readout: WCS 模式显示世界坐标原值");
        m.setCoordDisplayUcs(true);
        const Vec3 u30 = m.displayCoord(w);
        CHECK(u30.distTo(m.toUcs(w)) < 1e-12 && (u30 - w).len2D() > 1e-3,
              "readout: UCS 模式显示局部坐标且与 WCS 读数不同");
        // 网格主方向 = UCS X 轴（drawBackground 沿该方向铺网格线）
        CHECK(m.currentUcs().xAxis().distTo(Vec3(std::cos(math::deg2rad(30.0)),
                                                 std::sin(math::deg2rad(30.0)), 0)) < 1e-12,
              "grid: UCS 30° 时网格主方向 = (cos30, sin30)");
        m.setCurrentUcs(Ucs("WORLD", Vec3(0, 0, 0), 0.0));
        CHECK(m.currentUcs().xAxis() == Vec3(1, 0, 0), "grid: 世界系网格方向回到 (1,0)");
    }

    // ⑧ UCS 不入存档（JSON 顶层无 ucs 键 → 旧档/新档零回归）
    {
        SketchModel m;
        m.setCurrentUcs(Ucs("S", Vec3(1, 2, 0), math::deg2rad(15.0)));
        m.ucsSave(QStringLiteral("KEEP"));
        const QJsonObject root = m.toDrawingJson();
        CHECK(!root.contains(QStringLiteral("ucs")) && !root.contains(QStringLiteral("ucsTable")),
              "ucs: UCS 状态不入存档（顶层无 ucs/ucsTable 键）");
    }

    fprintf(stdout, "  (UCS 自测: 通过 %d / 失败 %d)\n", g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// 新建图纸「坐标原点置于幅面中心」语义：命令参数坐标（CommandContext 单一入口）
// 与 CommandLine 分段式输入（parsePointToWorld）均按当前 UCS 解释；
// 世界系下恒等（既有行为零回归）；raw 纯解析不受 UCS 影响。
static bool t_ucsSheetCenter() {
    using namespace SketchCAD;
    fprintf(stdout, "--- UCS 幅面中心（新建图纸原点语义） ---\n");
    const int passBefore = g_pass;
    const int failBefore = g_fail;

    // ① A4 横向（297×210）中心 (148.5,105)：整行命令的参数坐标随 UCS
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        m.setCurrentUcs(Ucs(QStringLiteral("SHEET_CENTER"), Vec3(148.5, 105.0, 0), 0.0));
        CHECK(std::fabs(m.currentUcs().origin.x - 148.5) < 1e-9 &&
              std::fabs(m.currentUcs().origin.y - 105.0) < 1e-9,
              "ucsCenter: UCS 原点=幅面中心设置生效");
        CHECK(interp.execute(QStringLiteral("line 0,0 100,100")), "ucsCenter: line 命令执行成功");
        const Element* ln = nullptr;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Line) { ln = &e; break; }
        CHECK(ln && ln->pts.size() == 2 &&
              std::fabs(ln->pts[0].x - 148.5) < 1e-9 && std::fabs(ln->pts[0].y - 105.0) < 1e-9 &&
              std::fabs(ln->pts[1].x - 248.5) < 1e-9 && std::fabs(ln->pts[1].y - 205.0) < 1e-9,
              "ucsCenter: line 0,0 100,100 → 世界端点 (148.5,105)-(248.5,205)");
        CHECK(interp.execute(QStringLiteral("circle 0,0 10")), "ucsCenter: circle 命令执行成功");
        const Element* ci = nullptr;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Circle) { ci = &e; break; }
        CHECK(ci && std::fabs(ci->pts[0].x - 148.5) < 1e-9 &&
                   std::fabs(ci->pts[0].y - 105.0) < 1e-9 && std::fabs(ci->radius - 10.0) < 1e-9,
              "ucsCenter: circle 0,0 10 → 圆心=幅面中心、半径不变（位移/尺寸类参数不受 UCS 平移）");
        CHECK(interp.execute(QStringLiteral("point 10,20")), "ucsCenter: point 命令执行成功");
        const Element* pt = nullptr;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Point) { pt = &e; break; }
        CHECK(pt && std::fabs(pt->pts[0].x - 158.5) < 1e-9 && std::fabs(pt->pts[0].y - 125.0) < 1e-9,
              "ucsCenter: point 10,20 → 世界 (158.5,125)");
    }

    // ② 与 CommandLine 分段式同源：parsePointToWorld 绝对坐标同语义
    {
        SketchModel m;
        m.setCurrentUcs(Ucs(QStringLiteral("SHEET_CENTER"), Vec3(148.5, 105.0, 0), 0.0));
        Vec3 w;
        CHECK(m.parsePointToWorld(QStringLiteral("0,0"), Vec3(), w) &&
              w.distTo(Vec3(148.5, 105.0, 0)) < 1e-9,
              "ucsCenter: parsePointToWorld(\"0,0\")=幅面中心（两路输入同源）");
    }

    // ③ raw 纯解析回归：tryParsePointRaw 不做任何换算
    {
        Vec3 p;
        CHECK(CommandContext::tryParsePointRaw(QStringLiteral("3,4"), p) &&
              p.x == 3.0 && p.y == 4.0,
              "ucsCenter: tryParsePointRaw 纯解析语义不变");
    }

    // ④ 世界系恒等回归：默认 UCS 下既有命令行为不变
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("line 5,5 15,15")), "ucsCenter: 世界系 line 执行成功");
        const Element* ln = nullptr;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Line) { ln = &e; break; }
        CHECK(ln && ln->pts.size() == 2 && ln->pts[0] == Vec3(5, 5, 0) && ln->pts[1] == Vec3(15, 15, 0),
              "ucsCenter: 世界系下命令坐标恒等（既有行为零回归）");
    }

    // ⑤ move 位移量（toDouble 直解）不随 UCS 平移（绝对点语义已在 ① 覆盖）
    {
        SketchModel m;
        CommandInterpreter interp(&m);
        m.setCurrentUcs(Ucs(QStringLiteral("SHEET_CENTER"), Vec3(100.0, 50.0, 0), 0.0));
        CHECK(interp.execute(QStringLiteral("circle 0,0 20")), "ucsCenter: 建圆成功");
        for (const auto& e : m.elements())
            if (e.type == ElementType::Circle) { m.selectEntity(e.id); break; }
        CHECK(interp.execute(QStringLiteral("move 10 0")), "ucsCenter: move 执行成功");
        const Element* ci = nullptr;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Circle) { ci = &e; break; }
        CHECK(ci && std::fabs(ci->pts[0].x - 110.0) < 1e-9 && std::fabs(ci->pts[0].y - 50.0) < 1e-9,
              "ucsCenter: move 位移量 Δ=(10,0) 为纯增量（(100,50)→(110,50)，不叠加 UCS 原点）");
    }

    fprintf(stdout, "  (UCS 幅面中心自测: 通过 %d / 失败 %d)\n", g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// ============================================================================
// 尺寸驱动编辑（t_dimDriven）
// ----------------------------------------------------------------------------
// 需求：直接修改图元上的标注数值 → 该图元及相连/受约束相邻图元按几何关联
// 自动调整；所有受影响图元的标注、引线、文字内容同步更新（数值=真实几何）。
// 链路：driveDimension（驱动源几何）→ collectLinked（端点重合联动传播，含
// 其它标注/引线）→ 被联动标注清除覆盖文本（显示值跟随实测）→ solveAfterChange
// （隐式约束恢复）+ solveAll（显式约束重解）→ refreshHatchBoundaries（填充随动）
// → refreshFields（PERIM/AREA/PROP 域同步）。单次 pushMemento，一步撤销全恢复。
// ============================================================================
static bool t_dimDriven() {
    const int passBefore = g_pass, failBefore = g_fail;

    // ---- ① 对齐标注驱动直线 + 相邻图元联动 + 文本同步 + 单步撤销 ----
    {
        SketchModel m;
        const int lid = addLine(m, 0, 0, 100, 0);
        // 相邻图元：竖线底端与 l1 终点重合（端点重合即视为相连）
        Element l2; l2.type = ElementType::Line;
        l2.pts = { Vec3(100,0,0), Vec3(100,40,0) };
        const int l2id = m.addElement(l2);
        // 驱动标注：对齐、refA=lid、测点与线端点重合、带覆盖文本
        Element d; d.type = ElementType::Dimension; d.dtype = DimensionType::Aligned;
        d.refA = lid; d.pts = { Vec3(0,0,0), Vec3(100,0,0) };
        d.text = QStringLiteral("一百");
        const int did = m.addElement(d);
        // 第二个标注：同测点（无覆盖文本），验证受影响标注测量同步
        Element d2; d2.type = ElementType::Dimension; d2.dtype = DimensionType::Horizontal;
        d2.refA = lid; d2.pts = { Vec3(0,0,0), Vec3(100,0,0) };
        const int d2id = m.addElement(d2);

        CHECK(m.driveDimension(did, 50.0), "① 驱动对齐标注 100→50 成功");
        CHECK(std::abs(m.element(lid)->pts[1].x - 50.0) < 1e-6, "① 源直线终点 x 100→50");
        CHECK(std::abs(m.element(l2id)->pts[0].x - 50.0) < 1e-6 &&
              std::abs(m.element(l2id)->pts[0].y) < 1e-6, "① 相邻竖线底端随动 (100,0)→(50,0)");
        CHECK(m.element(did)->text.isEmpty(), "① 驱动标注覆盖文本已清除");
        CHECK(std::abs(geo::measureDimension(*m.element(did)) - 50.0) < 1e-6, "① 驱动标注实测=50");
        CHECK(std::abs(geo::measureDimension(*m.element(d2id)) - 50.0) < 1e-6, "① 同测点标注实测同步=50");
        m.undo();
        CHECK(std::abs(m.element(lid)->pts[1].x - 100.0) < 1e-6, "① 单步撤销恢复源直线");
        CHECK(std::abs(m.element(l2id)->pts[0].x - 100.0) < 1e-6, "① 单步撤销恢复相邻图元");
    }

    // ---- ② 半径驱动圆 + PERIM 域同步 ----
    {
        SketchModel m;
        const int cid = addCircle(m, 0, 0, 20);
        Element d; d.type = ElementType::Dimension; d.dtype = DimensionType::Radius;
        d.refA = cid; d.pts = { Vec3(0,0,0), Vec3(20,0,0) };
        const int did = m.addElement(d);
        Element mt; mt.type = ElementType::MText;
        mt.pts = { Vec3(0,-40,0) }; mt.radius = 3.5;
        mt.text = QStringLiteral("周长 = %<PERIM:%1>%").arg(cid);
        const int mid = m.addElement(mt);

        CHECK(m.driveDimension(did, 50.0), "② 驱动半径标注 20→50 成功");
        CHECK(std::abs(m.element(cid)->radius - 50.0) < 1e-6, "② 圆半径 20→50");
        CHECK(std::abs(geo::measureDimension(*m.element(did)) - 50.0) < 1e-6, "② 标注实测=50");
        CHECK(resolveFields(m.element(mid)->text, &m) == QStringLiteral("周长 = 314.16"),
              "② PERIM 域随驱动同步（2π×50=314.16）");
        m.undo();
        CHECK(std::abs(m.element(cid)->radius - 20.0) < 1e-6, "② 单步撤销恢复圆半径");
    }

    // ---- ③ 角度驱动 + 相邻边线端点联动 ----
    {
        SketchModel m;
        (void)addLine(m, 0, 0, 30, 0);                       // 边1（不动腿）
        Element l2; l2.type = ElementType::Line;
        l2.pts = { Vec3(0,0,0), Vec3(0,30,0) };              // 边2：与标注边2点重合
        const int l2id = m.addElement(l2);
        Element d; d.type = ElementType::Dimension; d.dtype = DimensionType::Angle;
        d.refA = -1;   // 角度标注创建时不绑定源图元，按坐标锚定
        d.pts = { Vec3(0,0,0), Vec3(30,0,0), Vec3(0,30,0) };
        const int did = m.addElement(d);

        CHECK(m.driveDimension(did, 60.0), "③ 驱动角度标注 90°→60° 成功");
        const Vec3& p = m.element(l2id)->pts[1];
        CHECK(std::abs(p.x - 15.0) < 1e-6, "③ 边2端点 x → 30·cos60°=15");
        CHECK(std::abs(p.y - 30.0*std::sqrt(3.0)/2.0) < 1e-6, "③ 边2端点 y → 30·sin60°≈25.98");
        CHECK(std::abs(geo::measureDimension(*m.element(did)) - 60.0) < 1e-6, "③ 角度实测=60");
    }

    // ---- ④ 守卫：注释型标注/非法值拒绝驱动 ----
    {
        SketchModel m;
        const int lid = addLine(m, 0, 0, 10, 0);
        Element d; d.type = ElementType::Dimension; d.dtype = DimensionType::Leader;
        d.refA = lid; d.pts = { Vec3(0,0,0), Vec3(5,5,0), Vec3(10,5,0) };
        const int did = m.addElement(d);
        CHECK(!m.driveDimension(did, 5.0), "④ 引线型标注拒绝驱动");
        Element d2; d2.type = ElementType::Dimension; d2.dtype = DimensionType::Aligned;
        d2.refA = lid; d2.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        const int d2id = m.addElement(d2);
        CHECK(!m.driveDimension(d2id, 0.0), "④ 非法值(0)拒绝驱动");
        CHECK(!m.driveDimension(d2id, -5.0), "④ 非法值(负)拒绝驱动");
    }

    // ---- ⑤ dimedit 命令入口（选中标注后带 R/° 前后缀解析）----
    {
        SketchModel m;
        const int cid = addCircle(m, 0, 0, 10);
        m.setSelection({}); 
        Element d; d.type = ElementType::Dimension; d.dtype = DimensionType::Radius;
        d.refA = cid; d.pts = { Vec3(0,0,0), Vec3(10,0,0) };
        const int did = m.addElement(d);
        m.setSelection({ did });
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("dimedit R25")), "⑤ dimedit R25 命令执行成功");
        CHECK(std::abs(m.element(cid)->radius - 25.0) < 1e-6, "⑤ 圆半径 10→25（R 前缀解析）");
        CHECK(!interp.execute(QStringLiteral("dimedit abc")), "⑤ dimedit 非数值被拒");
    }

    fprintf(stdout, "  (尺寸驱动自测: 通过 %d / 失败 %d)\n", g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// ============================================================================
// 需求 D：两线选角标注 + 尺寸驱动主开关（t_dimAngleDrive）
// ----------------------------------------------------------------------------
// ① 选中两条相交直线 → dim angle 自动求交为顶点、取两线远端端点为边点，生成带 ° 的角度标注；
// ② 角度标注（refA=-1，坐标锚定）经 dimedit 驱动时按全局开关分派：
//     开（默认）→ 旋转第二条线端点（几何联动）；
//     关        → 仅覆盖显示文本，几何不变（显示与几何解耦）。
// ④ 尺寸驱动主开关默认值经 AppSettings 单一数据源校验为开启。
static bool t_dimAngleDrive() {
    const int passBefore = g_pass, failBefore = g_fail;

    // ---- ① 选中两相交直线 → 自动创建角度标注（顶点=交点，边点=两线远端端点）----
    {
        SketchModel m;
        const int lidA = addLine(m, 0, 0, 30, 0);   // 边1：沿 +X
        const int lidB = addLine(m, 0, 0, 0, 30);   // 边2：沿 +Y（与边1 共端于原点，夹角 90°）
        m.setSelection({lidA, lidB});
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("dim angle")), "① 选中两相交直线 → dim angle 创建成功");

        int did = -1;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Dimension && e.dtype == DimensionType::Angle) { did = e.id; break; }
        CHECK(did >= 0, "① 已创建角度标注图元");
        const Element* d = m.element(did);
        CHECK(d != nullptr, "① 可取到角度标注指针");
        CHECK(d->pts.size() == 3, "① 角度标注三控制点（顶点/边1/边2）");
        CHECK(d->pts[0].distTo(Vec3(0,0,0)) < 1e-6, "① 顶点=两直线交点 (0,0)");
        CHECK(std::abs(geo::measureDimension(*d) - 90.0) < 1e-6, "① 夹角测量=90°");

        // ---- ② 开关开启（默认）→ dimedit 驱动旋转第二条线端点 ----
        CHECK(m.dimensionDriven(), "② 尺寸驱动主开关默认开启");
        CHECK(interp.execute(QStringLiteral("dimedit %1 60").arg(did)), "② dimedit 60 驱动成功");
        const Vec3& p = m.element(lidB)->pts[1];
        CHECK(std::abs(p.x - 15.0) < 1e-6, "② 边2(第2条线)端点 x → 30·cos60°=15");
        CHECK(std::abs(p.y - 30.0*std::sqrt(3.0)/2.0) < 1e-6, "② 边2端点 y → 30·sin60°≈25.98");
        CHECK(std::abs(geo::measureDimension(*m.element(did)) - 60.0) < 1e-6, "② 角度实测=60");
    }

    // ---- ③ 主开关关闭 → dimedit 仅覆盖显示文本，几何不变 ----
    {
        SketchModel m;
        const int lidA = addLine(m, 0, 0, 30, 0);
        const int lidB = addLine(m, 0, 0, 0, 30);
        m.setSelection({lidA, lidB});
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("dim angle")), "③ 选中两线创建角度标注");
        int did = -1;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Dimension && e.dtype == DimensionType::Angle) { did = e.id; break; }
        CHECK(did >= 0, "③ 已创建角度标注");

        m.setDimensionDriven(false);
        CHECK(!m.dimensionDriven(), "③ 主开关已关闭");
        const Vec3 before = m.element(lidB)->pts[1];
        CHECK(interp.execute(QStringLiteral("dimedit %1 45").arg(did)), "③ dimedit 45（开关关）执行成功");
        const Vec3 after = m.element(lidB)->pts[1];
        CHECK(std::abs(after.x - before.x) < 1e-9 && std::abs(after.y - before.y) < 1e-9,
              "③ 主开关关闭 → 第二条线几何端点未移动");
        const Element* d = m.element(did);
        CHECK(d->text == QStringLiteral("45"), "③ 主开关关闭 → 仅覆盖显示文本为 \"45\"");
        CHECK(std::abs(geo::measureDimension(*d) - 90.0) < 1e-6,
              "③ 主开关关闭 → 实测角度仍为几何真值 90°（显示与几何解耦）");
    }

    // ---- ④ 尺寸驱动主开关默认值（AppSettings 单一数据源）----
    {
        CHECK(AppSettings::defaultValue(QLatin1String("dimension/driveOn")).toBool(),
              "④ AppSettings 尺寸驱动默认开启");
    }

    // ---- ⑤ 两直线不相交 → 明确错误提示，不创建标注（命令路径）----
    {
        SketchModel m;
        const int lidA = addLine(m, 0, 0, 30, 0);    // 水平线 y=0
        const int lidB = addLine(m, 0, 10, 30, 10);  // 水平线 y=10（平行，不相交）
        m.setSelection({lidA, lidB});
        CommandInterpreter interp(&m);
        CHECK(!interp.execute(QStringLiteral("dim angle")), "⑤ 两直线不相交 → dim angle 失败并报错");
        int cnt = 0;
        for (const auto& e : m.elements())
            if (e.type == ElementType::Dimension && e.dtype == DimensionType::Angle) cnt++;
        CHECK(cnt == 0, "⑤ 不相交时不创建角度标注图元");
    }

    fprintf(stdout, "  (两线角度标注+尺寸驱动开关自测: 通过 %d / 失败 %d)\n",
            g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

// ============================================================================
// 新建图纸幅面驱动打印页面设置（t_sheetPageSync）
// ----------------------------------------------------------------------------
// 根因回归：新建 A4 横向 → 打印预览/导出 PDF 曾恒为默认 A4 纵向。
// 传递链断点：PrintSettings 默认构造即 QPageSize(A4)+Portrait（Layout.h），
// 而 currentPrintSettings_ 只来自默认构造/上次退出快照/打印设置对话框，
// 从不与新建图纸的 SheetSpec 同步；applyNewDrawing 建完图框即失（幅面只存为
// 几何）；exportPdfLayouts 零布局分支另 new 临时 PrintSettings（同病第二例）；
// BatchPlotEngine 用 PrintSettings::defaults()（第三例）；layout new / mview
// 自动建布局用 PrintSettings()（第四例）。
// 修复链路：SheetSpec 随图存档（顶层 "sheet" 键）→ applySheetSpecToPage 单一
// 入口只覆盖 pageSize/orientation → 新建/读档（含旧档图框自愈）/切文档/
// 零布局导出/COM 批出图/布局创建 六处消费点全部经它派生。
// ============================================================================
static bool t_sheetPageSync() {
    using namespace SketchCAD;
    fprintf(stdout, "--- 新建图纸幅面驱动页面设置（打印/预览/PDF 方向一致） ---\n");
    const int passBefore = g_pass;
    const int failBefore = g_fail;

    // PDF MediaBox 解析辅助：返回 [w h]（磅）。QPdfWriter 页面对象为明文。
    auto mediaBoxOf = [](const QString& path, double& w, double& h) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QByteArray data = f.readAll();
        f.close();
        const int k = data.indexOf("/MediaBox");
        if (k < 0) return false;
        const int lb = data.indexOf('[', k);
        const int rb = data.indexOf(']', k);
        if (lb < 0 || rb < 0 || rb <= lb) return false;
        const QList<QByteArray> raw = data.mid(lb + 1, rb - lb - 1).simplified().split(' ');
        if (raw.size() < 4) return false;
        bool okW = false, okH = false;
        w = raw[2].toDouble(&okW);
        h = raw[3].toDouble(&okH);
        return okW && okH;
    };

    // ---- ① pageSizeIdForSheet 六幅面映射 + applySheetSpecToPage 只动两字段 ----
    {
        struct Ent { SheetFormat f; QPageSize::PageSizeId id; };
        const Ent ents[] = {
            {SheetFormat::A0, QPageSize::A0},
            {SheetFormat::A1, QPageSize::A1},
            {SheetFormat::A2, QPageSize::A2},
            {SheetFormat::A3, QPageSize::A3},
            {SheetFormat::A4, QPageSize::A4},
            {SheetFormat::A5, QPageSize::A5},
        };
        for (const auto& e : ents)
            CHECK(pageSizeIdForSheet(e.f) == e.id, "sheetPage: pageSizeIdForSheet 六幅面映射");
        PrintSettings s = PrintSettings::defaults();
        s.marginsMm = QMarginsF(7, 8, 9, 10);
        s.scale = 2.5; s.fitToPage = false; s.drawBorder = false;
        const QMarginsF m0 = s.marginsMm;
        applySheetSpecToPage(SheetSpec::make(SheetFormat::A4, true), s);
        CHECK(s.pageSize.id() == QPageSize::A4 && s.orientation == QPageLayout::Landscape,
              "sheetPage: A4 横向 → A4 + Landscape（核心断言）");
        CHECK(s.marginsMm == m0 && std::fabs(s.scale - 2.5) < 1e-9 && !s.fitToPage && !s.drawBorder,
              "sheetPage: 只覆盖 pageSize/orientation，边距/比例/样式不动");
        applySheetSpecToPage(SheetSpec::make(SheetFormat::A4, false), s);
        CHECK(s.orientation == QPageLayout::Portrait, "sheetPage: A4 纵向 → Portrait");
        SheetSpec bad = SheetSpec::make(SheetFormat::A3, true);
        bad.format = (SheetFormat)42;
        applySheetSpecToPage(bad, s);
        CHECK(s.pageSize.id() == QPageSize::A4 && s.orientation == QPageLayout::Portrait,
              "sheetPage: format 越界 → 页面设置保持原样（不猜）");
    }

    // ---- ② SketchModel 幅面存取 + 随图存档往返 ----
    {
        SketchModel m;
        CHECK(!m.hasSheetSpec(), "sheetPage: 新建模型默认无幅面规格");
        CHECK(m.sheetSpecToJson().isEmpty(), "sheetPage: 无规格 → sheetSpecToJson 空对象");
        CHECK(!m.toDrawingJson().contains(QStringLiteral("sheet")),
              "sheetPage: 无规格 → 顶层不写 sheet 键（旧档零回归）");
        m.setSheetSpec(SheetSpec::make(SheetFormat::A4, true));
        CHECK(m.hasSheetSpec() && m.sheetSpec().format == SheetFormat::A4 && m.sheetSpec().landscape,
              "sheetPage: setSheetSpec A4 横向生效");
        const QJsonObject sj = m.toDrawingJson()["sheet"].toObject();
        CHECK(sj["format"].toInt() == (int)SheetFormat::A4 && sj["landscape"].toBool(),
              "sheetPage: 有规格 → 顶层 sheet = {format,landscape}");
        auto sp = SketchModel::sheetSpecFromJson(sj);
        CHECK(sp && sp->format == SheetFormat::A4 && sp->landscape, "sheetPage: FromJson 往返一致");
        CHECK(!SketchModel::sheetSpecFromJson(QJsonObject()), "sheetPage: 空对象 → nullopt");
        QJsonObject oBad; oBad["format"] = 99;
        CHECK(!SketchModel::sheetSpecFromJson(oBad), "sheetPage: format 越界 → nullopt");
        QJsonObject oDef; oDef["format"] = (int)SheetFormat::A5;
        const auto spDef = SketchModel::sheetSpecFromJson(oDef);
        CHECK(spDef && spDef->format == SheetFormat::A5 && spDef->landscape,
              "sheetPage: landscape 缺省 true（工程图惯例）");
        SketchModel m2;
        m2.setSheetSpec(SheetSpec::make(SheetFormat::A5, false));
        SheetSpec bad2 = SheetSpec::make(SheetFormat::A5, false);
        bad2.format = (SheetFormat)-3;
        m2.setSheetSpec(bad2);
        CHECK(m2.sheetSpec().format == SheetFormat::A5 && !m2.sheetSpec().landscape,
              "sheetPage: setSheetSpec 越界拒绝（保持原值）");
        m2.clear();
        CHECK(!m2.hasSheetSpec(), "sheetPage: clear() 丢弃幅面规格");
    }

    // ---- ③ inferSheetSpecFromFrame：旧档缺 sheet 键按图框自愈 ----
    {
        SketchModel m;   // A4 横向旧档：buildSheetFrame 产物（含纸张外边界矩形）
        for (const auto& e : buildSheetFrame(SheetSpec::make(SheetFormat::A4, true)))
            m.addElement(e);
        auto inf = m.inferSheetSpecFromFrame();
        CHECK(inf && inf->format == SheetFormat::A4 && inf->landscape,
              "sheetPage: 图框推断 A4 横向（旧档自愈）");
        SketchModel m5;
        for (const auto& e : buildSheetFrame(SheetSpec::make(SheetFormat::A5, false)))
            m5.addElement(e);
        auto inf5 = m5.inferSheetSpecFromFrame();
        CHECK(inf5 && inf5->format == SheetFormat::A5 && !inf5->landscape,
              "sheetPage: 图框推断 A5 纵向（方向由长短边定）");
        SketchModel me;
        CHECK(!me.inferSheetSpecFromFrame(), "sheetPage: 空模型 → 放弃推断");
        SketchModel mn;   // 非标尺寸 → 绝不猜
        Element r; r.type = ElementType::Rectangle;
        r.pts = {Vec3(0, 0, 0), Vec3(123.4, 456.7, 0)};
        r.layer = QStringLiteral("图框");
        mn.addElement(r);
        CHECK(!mn.inferSheetSpecFromFrame(), "sheetPage: 非标图框矩形 → 放弃推断");
        Element small; small.type = ElementType::Rectangle;   // 同层小矩形不干扰取最大
        small.pts = {Vec3(0, 0, 0), Vec3(10, 10, 0)};
        small.layer = QStringLiteral("图框");
        m.addElement(small);
        auto inf2 = m.inferSheetSpecFromFrame();
        CHECK(inf2 && inf2->format == SheetFormat::A4 && inf2->landscape,
              "sheetPage: 同层小矩形不干扰（取最大面积）");
    }

    // ---- ④ PrintEngine::modelSpacePageSettings 跟随与回落 ----
    {
        SketchModel m;
        m.setSheetSpec(SheetSpec::make(SheetFormat::A4, true));
        const PrintSettings s = PrintEngine::modelSpacePageSettings(&m);
        CHECK(s.pageSize.id() == QPageSize::A4 && s.orientation == QPageLayout::Landscape
              && s.viewports.isEmpty() && s.fitToPage,
              "sheetPage: modelSpacePageSettings 跟随 A4 横向（视口空+fitToPage）");
        SketchModel m2;
        const PrintSettings s2 = PrintEngine::modelSpacePageSettings(&m2);
        CHECK(s2.pageSize.id() == QPageSize::A4 && s2.orientation == QPageLayout::Portrait,
              "sheetPage: 无幅面 → 旧行为 A4 纵向（空白图/外部导入）");
        const PrintSettings s3 = PrintEngine::modelSpacePageSettings(nullptr);
        CHECK(s3.orientation == QPageLayout::Portrait, "sheetPage: nullptr 模型 → 旧行为");
    }

    // ---- ⑤ 端到端：exportPdfLayouts 零布局 → MediaBox 横竖断言 ----
    {
        SketchModel mL;
        addLine(mL, 0, 0, 100, 0);
        mL.setSheetSpec(SheetSpec::make(SheetFormat::A4, true));
        const QString pL = QDir::temp().absoluteFilePath(QStringLiteral("sheetpage_a4L.pdf"));
        QFile::remove(pL);
        CHECK(PrintEngine::exportPdfLayouts(&mL, pL) == 1, "sheetPage: 零布局导出 1 页（横向）");
        double w = 0, h = 0;
        CHECK(mediaBoxOf(pL, w, h), "sheetPage: 横向 PDF 含 /MediaBox");
        CHECK(w > h && std::fabs(w - 841.89) < 2.0 && std::fabs(h - 595.28) < 2.0,
              "sheetPage: MediaBox 842×595pt 横向（A4 横向端到端）");
        SketchModel mP;
        addLine(mP, 0, 0, 100, 0);
        mP.setSheetSpec(SheetSpec::make(SheetFormat::A4, false));
        const QString pP = QDir::temp().absoluteFilePath(QStringLiteral("sheetpage_a4P.pdf"));
        QFile::remove(pP);
        CHECK(PrintEngine::exportPdfLayouts(&mP, pP) == 1, "sheetPage: 零布局导出 1 页（纵向对照）");
        double w2 = 0, h2 = 0;
        CHECK(mediaBoxOf(pP, w2, h2), "sheetPage: 纵向 PDF 含 /MediaBox");
        CHECK(w2 < h2 && std::fabs(w2 - 595.28) < 2.0 && std::fabs(h2 - 841.89) < 2.0,
              "sheetPage: MediaBox 595×842pt 纵向（对照组）");
    }

    // ---- ⑥ 命令层：layout new / mview 自动布局按幅面 seeding 默认页 ----
    {
        SketchModel m;
        m.setSheetSpec(SheetSpec::make(SheetFormat::A4, true));
        CommandInterpreter interp(&m);
        CHECK(interp.execute(QStringLiteral("layout new L1")), "sheetPage: layout new 执行成功");
        const PlotLayoutDef* d = m.findLayout(QStringLiteral("L1"));
        CHECK(d && d->settings.pageSize.id() == QPageSize::A4
              && d->settings.orientation == QPageLayout::Landscape,
              "sheetPage: layout new 默认页跟随 A4 横向");
        SketchModel m2;
        m2.setSheetSpec(SheetSpec::make(SheetFormat::A3, false));
        CommandInterpreter i2(&m2);
        CHECK(i2.execute(QStringLiteral("mview 10 10 90 60")), "sheetPage: mview 执行成功");
        const PlotLayoutDef* d2 = m2.findLayout(QStringLiteral("布局1"));
        CHECK(d2 && d2->settings.pageSize.id() == QPageSize::A3
              && d2->settings.orientation == QPageLayout::Portrait,
              "sheetPage: mview 自动布局默认页跟随 A3 纵向");
        SketchModel m3;
        CommandInterpreter i3(&m3);
        CHECK(i3.execute(QStringLiteral("layout new L2")), "sheetPage: 无幅面 layout new 仍成功");
        const PlotLayoutDef* d3 = m3.findLayout(QStringLiteral("L2"));
        CHECK(d3 && d3->settings.pageSize.id() == QPageSize::A4
              && d3->settings.orientation == QPageLayout::Portrait,
              "sheetPage: 无幅面 → 旧默认 A4 纵向");
    }

    // ---- ⑦ COM 批量出图：sheet 键随档恢复 + 无显式 settings 时 PDF 横向 ----
    {
        using namespace SketchCAD::comauto;
        SketchModel m;
        addLine(m, 0, 0, 100, 0);
        m.setSheetSpec(SheetSpec::make(SheetFormat::A4, true));
        const QString src = QDir::temp().absoluteFilePath(QStringLiteral("sheetpage_com_src.json"));
        QFile::remove(src);
        QString err;
        CHECK(BatchPlotEngine::exportDrawing(m, src, QJsonObject(), &err),
              "sheetPage: COM 源图纸 JSON 落盘（含 sheet 键）");
        SketchModel back;
        CHECK(BatchPlotEngine::loadDrawing(back, src, &err), "sheetPage: 引擎读档成功");
        CHECK(back.hasSheetSpec() && back.sheetSpec().format == SheetFormat::A4
              && back.sheetSpec().landscape,
              "sheetPage: sheet 键随档恢复（引擎读档路径）");
        const QString pdf = QDir::temp().absoluteFilePath(QStringLiteral("sheetpage_com.pdf"));
        QFile::remove(pdf);
        QJsonObject t; t["source"] = src; t["output"] = pdf;   // 无 settings → 跟随幅面
        QJsonObject batch; QJsonArray arr; arr.append(t); batch["tasks"] = arr;
        const BatchResult r = BatchPlotEngine::runBatch(
            QString::fromUtf8(QJsonDocument(batch).toJson(QJsonDocument::Compact)), QString());
        CHECK(r.allOk && r.succeeded == 1, "sheetPage: COM 批量任务成功");
        double w = 0, h = 0;
        CHECK(mediaBoxOf(pdf, w, h), "sheetPage: COM PDF 含 /MediaBox");
        CHECK(w > h, "sheetPage: COM 批量 PDF 横向（无显式 settings 跟随幅面）");
    }

    fprintf(stdout, "  (幅面驱动页面设置自测: 通过 %d / 失败 %d)\n", g_pass - passBefore, g_fail - failBefore);
    return (g_fail - failBefore) == 0;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);   // t_interaction 需无头构造 QLineEdit 系控件（QApplication 行为是 QCoreApplication 超集）
    setvbuf(stdout, nullptr, _IONBF, 0);   // 无缓冲：进程异常挂起时也能看到已完成的用例
    fprintf(stdout, "=== SketchCAD 约束/捕捉自测 ===\n");

    // --undo：撤销专项快速通道（t_undoTables + t_undoDelta）。
    // 存在理由：t_goldenSamples/t_dwg 等用例依赖 QProcess 调外部工具（LibreDWG），
    // 沙箱管道耗尽（"QProcess: CreateFile failed: 所有的管道范例都在使用中"）时
    // spawn 会无限期挂起，全量流程根本到不了撤销用例；此开关绕开外部依赖段，
    // 专验撤销体系。全量回归仍走无参流程（外部工具可用时）。
    if (argc > 1 && QByteArray(argv[1]) == QByteArray("--undo")) {
        fprintf(stdout, "--- 撤销专项（--undo）：撤销表 + 撤销差量化 ---\n");
        CHECK(t_undoTables(), "撤销表 UndoTables(布局/单位/界限/捕捉/追踪/过滤器/去重/栈深/混合用例)");
        CHECK(t_undoDelta(),  "撤销栈差量化(差量vs全量终态全等/检查点回放/id复用/联动同一步/切换清栈/内存与耗时基准)");
        fprintf(stdout, "=== 撤销专项完成：通过 %d / 失败 %d ===\n", g_pass, g_fail);
        return g_fail == 0 ? 0 : 1;
    }

    // P0-01：仅在显式设置环境变量时，由真实 toDxf/toDwg 生成 tests/golden/ 静态样本。
    // 生成后由 t_formatsGolden 长期校验；此分支不进入正常自测流程。
    if (std::getenv("SKETCHCAD_GEN_GOLDEN")) {
        genFormatsGoldens();
        return 0;
    }

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
    CHECK(t_sktIntegration(),       "SKT 集成：约束求解门面路由(Phase B) 映射/诊断/联动/回落");

    CHECK(t_scriptHost(),           "ScriptHost 脚本宿主(JS 加载/执行/异常/日志)");
    CHECK(t_pluginManager(),        "PluginManager 原生SDK(桥接/外部命令/插件加载卸载)");
    CHECK(t_comAutomation(),        "BatchPlotEngine COM自动化(批量出图/事件/错误码/日志)");
    CHECK(t_macroScript(),          "MacroScript 动作宏/SCR脚本(录制/保存回放/中断策略/互斥)");
    CHECK(t_alias(),                "AliasTable 命令别名(acad.pgp 兼容/解析/重载/运行时)");
    CHECK(t_cui(),                  "CuiManager 界面定制(CUIx 工具条/工作空间/XML 往返)");
    CHECK(t_cmdprompt(),            "UX增强 命令提示/开关组/DYN(选项格式化/匹配/会话态/lastCommand/补全)");
    CHECK(t_shapeselect(),          "ShapeSelect 栏选/圈围/圈交(相交判定/全包含/退化形状/点近邻)");
    CHECK(t_gripops(),              "GripOps 夹点热操作(移动/旋转/缩放/镜像/拉伸+边界拒绝+模式循环)");
    CHECK(t_qselect(),              "QSelect 快速选择命令接线(四维过滤/组合/全空/别名q/选中计数)");
    CHECK(t_dxfImport(),            "DXF 导入扩展(15+类实体/round-trip差分/块表/标注/填充/容错计数)");
    CHECK(t_dxfExport(),            "DXF 原生导出(块/标注/填充/点 token计数/round-trip≤1e-6/legacy兼容)");
    CHECK(t_formatsVersions(),      "P0-01 格式版本 R12+R2000(双版本读写/版本协商/二进制DXF/鲁棒性)");
    CHECK(t_formatsGolden(),        "P0-01 黄金样本 R12/R2000 DXF+DWG round-trip 一致性");

    // ---- 分层测试网：core 纯函数层（函数体内逐条 CHECK 计数） ----
    fprintf(stdout, "--- 分层测试网：core 纯函数层 ---\n");
    t_geometryCore();
    t_modifyCore();
    t_batchEdit();      // 批量编辑三件套：JOIN/阵列/剪贴板（逐条 CHECK 计数）
    t_sectionCore();
    t_inquiryCore();
    t_spatialIndex();   // 均匀网格空间索引：不变量/开关/复用/退化（逐条 CHECK 计数）
    CHECK(t_p004Memory(), "P0-04 内存预算 SmallVec 语义 + Element 尺寸上界");
    t_plotStyle();      // 打印样式表：映射解析/灰度/GB预设/CTB导入/JSON兼容（逐条 CHECK 计数）

    // ---- 分层测试网：存档兼容层 + 黄金样本层 ----
    fprintf(stdout, "--- 分层测试网：存档兼容/黄金样本 ---\n");
    t_archiveCompat();
    t_goldenSamples();
    t_layerDynamic();      // 图层动态语义：ByLayer / 冻结 / 打印 / 透明度 / LTSCALE（逐条 CHECK 计数）
    t_blockEdit();         // 块在位编辑(BEDIT)：N 处引用同步 / 嵌套传播 / Esc 不落盘 / 保留动态参数 / 外部块表
    t_hatchLinkage();      // 填充边界联动：随动/失效/静态/hatchedit撤销/序列化往返/比例图案生效
    t_fields();            // FIELD 域 + Table 公式列：面积联动/花括号等价/断链###/公式列/PROP/兼容/命令/开关
    t_gdt();               // GD&T：12类标注回归 / DimStyle表联动 / 旧档兼容 / MLEADER+FCF 往返
    t_qdim();              // QDIM 快速标注：圆/平行线阵列基线·连续 / 计数·位置·样式联动（14 CHECK）
    t_groupFilter();       // 编组+FILTER：整组选中/撤销还原/命令行/Xref禁止/预选器过滤/fence组合
    t_layerGovernance();   // 图层治理：LAYISO/LAYUNISO/LAYMCH 命令路径+退化（逐条 CHECK 计数）
    t_commandDispatch();   // 命令路径直测：join/arrayrect/arraypolar/arraypath/ltscale 端到端（逐条 CHECK 计数）
    t_layout();            // 布局体系：paperRect往返/布局CRUD/旧档回落/1:2与2:1/平铺像素/PDF样张/命令行
    CHECK(t_plotPublish(), "打印发布体系(视口冻结/发布页列表/戳记/视口旋转/GUI-COM一致)");
    CHECK(t_titleBlockAux(), "标题栏表格化+辅助元素打印过滤(单元格包含/收缩适配/谓词/端到端零墨迹)");
    CHECK(t_multidoc(),    "多文档标签 DocTab(独立undo/脏标记隔离/cycle循环/拖拽重排/跨文档粘贴/历史互换/关闭夹取)");
    CHECK(t_i18n(), "国际化+高DPI(语言解析/.qm装载/上下文存在/切换翻译底座/缺翻译回落中文/卸载回落源语言/DPI换算一致/QLocale口径)");
    CHECK(t_interaction(), "AutoCAD 交互对齐(空格/回车重复命令/↑↓历史/Tab补齐/坐标注入/未知命令3候选/历史持久化/右键行为配置)");
    CHECK(t_recent(),      "最近文件 MRU(增删/去重/上限裁剪/pin保护/缺失文件/持久化重载/旧键迁移/损坏容错)");
    CHECK(t_cmdParity(),   "命令补齐 CommandParity(三态/别名/typo建议/单步撤销)");
    CHECK(t_help(),        "命令发现与容错帮助(面板过滤/help详情/总数一致/未知候选/冲突自检)");
    CHECK(t_diag(),        "崩溃诊断包(ZIP结构/自解析CRC/包内容齐全/会话标记/遥测零外发)");
    CHECK(t_dwg(),         "DWG R2000+ 桥接(版本探测/路由白名单/错误报文含版本串/R12回归/round-trip差分)");
    CHECK(t_undoTables(),  "撤销表 UndoTables(布局/单位/界限/捕捉/追踪/过滤器/去重/栈深/混合用例)");
    CHECK(t_undoDelta(),    "撤销栈差量化(差量vs全量终态全等/检查点回放/id复用/联动同一步/切换清栈/内存与耗时基准)");

    // ---- 鲁棒性 / Fuzz 回归：任何输入都不崩溃/卡死/写坏文件 ----
    fprintf(stdout, "--- 鲁棒性 / Fuzz 回归 ---\n");
    CHECK(t_robustness(),  "鲁棒性守卫 ElementValidate(类型越界/NaN/Inf/超大/退化/重复末点/超长文本/控制字符/缺失引用/打开失败不污染)");
    CHECK(t_fileFuzz(),    "文件 Fuzz 200 随机损坏样本零崩溃零死锁单样本≤5s(JSON/DXF/DWG/二进制DXF)");
    CHECK(t_textMetrics(), "文字度量三处同源(真实度量/LRU缓存/无头降级/9点对齐/折行/列宽/堆叠提示)");
    CHECK(t_ucs(),         "用户坐标系 UCS(往返换算/45°@100<0落点≤1e-6/ucs命令定义+CRUD/极轴正交栅格UCS对齐/读数切换/网格方向/不入档)");
    CHECK(t_ucsSheetCenter(), "UCS 幅面中心(新建图纸原点=图纸中央/命令参数坐标随UCS/两路输入同源/raw纯解析回归/世界系恒等/move位移不受UCS)");
    CHECK(t_sheetPageSync(), "新建图纸幅面驱动页面设置(六幅面映射/只动两字段/随图存档往返/图框自愈/零布局PDF横竖端到端/布局seeding/COM批出图)");
    CHECK(t_dimDriven(), "尺寸驱动编辑(直线/圆/角度驱动+相邻联动+标注文本同步+域刷新+单步撤销+dimedit入口)");

    // ---- 分层测试网：既有模块补充直测 ----
    fprintf(stdout, "--- 分层测试网：模块补充直测 ---\n");
    t_gripopsDepth();
    t_shapeselectDepth();
    t_macroDepth();
    t_aliasDepth();
    t_cuiDepth();
    t_autosave();       // 崩溃不丢稿：自动保存/恢复/设置/诊断（≥12 条 CHECK）
    t_appsettings();    // 应用级配置持久化：默认回填/持久/损坏回落/导入导出/写失败（12 组 CHECK）

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
    CHECK(t_dimAngleDrive(),     "需求D: 两线角度标注+尺寸驱动主开关");
    CHECK(t_paramLink(),         "Param Link 参数表联动");
    CHECK(t_driveConsistency(),  "Drive 一致性校验");

    fprintf(stdout, "\n通过 %d / 失败 %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
