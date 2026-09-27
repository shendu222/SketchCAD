// 多段线 PEDIT 式交互编辑单元测试（polyops 纯模型层）
//
// 覆盖需求：基于 PolylineEditTool 交互式实现的六个操作
//   Fit / Spline / Width / Close / Join / Reverse
// 验证它们正确修改多段线的几何与属性，并与 PEDIT 命令行预期行为一致。
//
// 纯核心层（不依赖 UI 头文件），在构建环境以 exit code 为权威判定（stdout 可能丢失）。
#include <QCoreApplication>
#include <cstdio>
#include <cmath>

#include "core/Element.h"
#include "core/Vec.h"
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Geometry.h"
#include "core/PolylineOps.h"

using namespace SketchCAD;
using namespace SketchCAD::polyops;

static int g_fail = 0;
static FILE* g_log = nullptr;
static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fflush(g_log);
}
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); logf("  [PASS] %s\n", what); }
    else   { printf("  [FAIL] %s %s\n", what,
                     detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData());
            logf("  [FAIL] %s\n", what); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
static bool ptEq(const Vec3& p, double x, double y) { return near(p.x, x, 1e-6) && near(p.y, y, 1e-6); }
static Element makeLine(double x0, double y0, double x1, double y1);

// 点到折线点列的最小距离（用于判定「曲线是否经过某顶点」）
static double distToPolylinePts(const Vec3& p, const std::vector<Vec3>& pts) {
    double best = 1e100;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        best = std::min(best, geo::distPointSegment(p, pts[i], pts[i + 1]));
    if (pts.size() == 1) best = std::min(best, p.distTo(pts[0]));
    return best;
}

static Element makeOpenPoly(const std::vector<Vec3>& pts, const std::vector<bool>& flags) {
    SketchModel m;
    Element e = ElementFactory::createPolyline(pts, flags, /*closed=*/false, &m);
    return e;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    (void)argc; (void)argv;
    g_log = fopen("polyline_edit_ops_result.txt", "w");
    if (g_log) logf("=== PolylineEditOpsTest ===\n");

    // ==================== 1. Close / 打开 ====================
    printf("=== 1. 闭合 / 打开 (PEDIT Close) ===\n");
    {
        SketchModel m;
        std::vector<Vec3> pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0) };
        Element e = ElementFactory::createPolyline(pts, {false,false}, false, &m);
        const int id = m.addElement(e);

        check(setClosed(m, id, true), "setClosed(true) 返回成功");
        {
            const Element* p = m.element(id);
            check(p && p->closed, "折线已闭合");
            check(p && (int)p->arcFlags.size() == (int)p->pts.size(),
                  "闭合后 arcFlags 长度 == 顶点数",
                  p ? QString("arcFlags=%1 pts=%2").arg(p->arcFlags.size()).arg(p->pts.size()) : "");
        }
        check(setClosed(m, id, true), "重复 setClosed(true) 幂等成功");
        check(setClosed(m, id, false), "setClosed(false) 返回成功");
        {
            const Element* p = m.element(id);
            check(p && !p->closed, "折线已打开");
            check(p && (int)p->arcFlags.size() == (int)p->pts.size() - 1,
                  "打开后 arcFlags 长度 == 顶点数-1",
                  p ? QString("arcFlags=%1 pts=%2").arg(p->arcFlags.size()).arg(p->pts.size()) : "");
        }
        check(!setClosed(m, id + 999, true), "对非折线 id 返回失败");
    }

    // ==================== 2. Reverse ====================
    printf("=== 2. 反转 (Reverse) ===\n");
    {
        SketchModel m;
        std::vector<Vec3> pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(20,5,0) };
        Element e = ElementFactory::createPolyline(pts, {true,false}, false, &m);
        const int id = m.addElement(e);
        check(reverse(m, id), "reverse 返回成功");
        const Element* p = m.element(id);
        check(p && ptEq(p->pts[0], 20,5) && ptEq(p->pts[1], 10,0) && ptEq(p->pts[2], 0,0),
              "顶点逆序正确");
        check(p && p->arcFlags[0] == false && p->arcFlags[1] == true,
              "段类型标志随之逆序（段0↔原段1）",
              p ? QString("flags=%1,%2").arg(p->arcFlags[0]).arg(p->arcFlags[1]) : "");

        // 闭合折线反转：pts 循环逆序，段数不变
        SketchModel m2;
        std::vector<Vec3> cpts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0) };
        Element ec = ElementFactory::createPolyline(cpts, {false,true,false,true}, true, &m2);
        const int cid = m2.addElement(ec);
        check(reverse(m2, cid), "闭合折线 reverse 成功");
        const Element* pc = m2.element(cid);
        check(pc && (int)pc->pts.size() == 4 && (int)pc->arcFlags.size() == 4,
              "闭合反转后顶点/段数保持 4");
        check(pc && ptEq(pc->pts[0], 0,10), "闭合反转后首点变为原末点");
    }

    // ==================== 3. Width (PEDIT Width) ====================
    printf("=== 3. 设置线宽 (PEDIT Width) ===\n");
    {
        SketchModel m;
        Element e = ElementFactory::createPolyline({Vec3(0,0,0),Vec3(10,0,0)}, {false}, false, &m);
        const int id = m.addElement(e);
        check(setWidth(m, id, 5.0), "setWidth(5) 成功");
        {
            const Element* p = m.element(id);
            check(p && !p->lineWidthAuto, "lineWidthAuto 已关闭（使用手动线宽）");
            check(p && near(p->lineWidth, 5.0), "lineWidth == 5.0",
                  p ? QString("w=%1").arg(p->lineWidth) : "");
        }
        check(setWidth(m, id, -3.0), "setWidth(-3) 仍成功（负值钳为 0）");
        {
            const Element* p = m.element(id);
            check(p && near(p->lineWidth, 0.0), "负线宽被钳为 0");
        }
        // 渲染生效：effectiveLineWidth() 应返回手动线宽
        check(near(m.element(id)->effectiveLineWidth(), 5.0) || near(m.element(id)->effectiveLineWidth(), 0.0),
              "effectiveLineWidth 反映手动线宽");
    }

    // ==================== 4. Fit (PEDIT Fit) ====================
    printf("=== 4. 拟合曲线 (PEDIT Fit) ===\n");
    {
        SketchModel m;
        // 折角点：曲线应平滑通过所有顶点
        std::vector<Vec3> pts = { Vec3(0,0,0), Vec3(10,10,0), Vec3(20,0,0), Vec3(30,10,0) };
        Element e = ElementFactory::createPolyline(pts, {false,false,false}, false, &m);
        const int id = m.addElement(e);
        check(fit(m, id), "fit 返回成功");
        check(m.element(id)->splineSubType == 1, "splineSubType == 1 (Fit)");

        // 采样曲线应严格通过每个顶点（Catmull-Rom 插值）
        std::vector<Vec3> curve = geo::samplePolylineFit(pts, /*closed=*/false, 16);
        check(!curve.empty(), "Fit 采样非空");
        bool allThrough = true;
        for (const Vec3& v : pts) {
            if (distToPolylinePts(v, curve) > 1e-6) { allThrough = false; break; }
        }
        check(allThrough, "Fit 曲线通过全部原始顶点");
    }

    // ==================== 5. Spline (PEDIT Spline) ====================
    printf("=== 5. 样条化 (PEDIT Spline) ===\n");
    {
        SketchModel m;
        std::vector<Vec3> pts = { Vec3(0,0,0), Vec3(10,20,0), Vec3(20,0,0), Vec3(30,20,0) };
        Element e = ElementFactory::createPolyline(pts, {false,false,false}, false, &m);
        const int id = m.addElement(e);
        check(spline(m, id), "spline 返回成功");
        check(m.element(id)->splineSubType == 2, "splineSubType == 2 (Spline)");

        // 三次 B 样条（顶点为控制点）：曲线端点靠近首尾控制点，但不一定穿过中间顶点
        std::vector<Vec3> curve = geo::samplePolylineSpline(pts, /*closed=*/false, 16);
        check(!curve.empty(), "Spline 采样非空");
        check(distToPolylinePts(pts.front(), curve) < 1e-6, "Spline 起点靠近首控制点");
        check(distToPolylinePts(pts.back(),  curve) < 1e-6, "Spline 终点靠近末控制点");
        // 中间控制点一般不被曲线穿过（逼近型）：选一个明显偏离的，验证距离>0
        const double dMid = distToPolylinePts(pts[1], curve);
        check(dMid > 1e-4, "Spline 为逼近型（中间控制点未被严格穿过）",
              QString("dist=%1").arg(dMid));
    }

    // ==================== 6. Join (PEDIT Join) ====================
    printf("=== 6. 合并 (PEDIT Join) ===\n");
    {
        // 6a. 在末端追加直线
        SketchModel m;
        Element tgt = ElementFactory::createPolyline(
            {Vec3(0,0,0), Vec3(10,0,0), Vec3(20,0,0)}, {false,false}, false, &m);
        const int tid = m.addElement(tgt);
        Element src = makeLine(20,0, 30,0);   // 共享端点 (20,0)
        const int sid = m.addElement(src);
        check(join(m, tid, sid, 1e-3), "末端追加直线 join 成功");
        {
            const Element* p = m.element(tid);
            check(p && (int)p->pts.size() == 4 && ptEq(p->pts.back(), 30,0),
                  "目标折线扩展为 4 点，末点 (30,0)",
                  p ? QString("n=%1 last=(%2,%3)").arg(p->pts.size())
                        .arg(p->pts.back().x).arg(p->pts.back().y) : "");
            check(p && (int)p->arcFlags.size() == (int)p->pts.size() - 1,
                  "join 后 arcFlags 长度保持一致");
            check(m.element(sid) == nullptr, "来源直线已从模型删除");
        }

        // 6b. 在首端前插直线
        SketchModel m2;
        Element tgt2 = ElementFactory::createPolyline(
            {Vec3(0,0,0), Vec3(10,0,0)}, {false}, false, &m2);
        const int tid2 = m2.addElement(tgt2);
        Element src2 = makeLine(-10,0, 0,0);  // 共享端点 (0,0)
        const int sid2 = m2.addElement(src2);
        check(join(m2, tid2, sid2, 1e-3), "首端前插直线 join 成功");
        {
            const Element* p = m2.element(tid2);
            check(p && ptEq(p->pts.front(), -10,0) && (int)p->pts.size() == 3,
                  "目标折线首点变为 (-10,0)，共 3 点");
        }

        // 6c. 端点不重合 → 失败，无改动
        SketchModel m3;
        Element tgt3 = ElementFactory::createPolyline(
            {Vec3(0,0,0), Vec3(10,0,0)}, {false}, false, &m3);
        const int tid3 = m3.addElement(tgt3);
        Element src3 = makeLine(50,0, 60,0);  // 不重合
        const int sid3 = m3.addElement(src3);
        check(!join(m3, tid3, sid3, 1e-3), "端点不重合 join 返回失败");
        check(m3.element(sid3) != nullptr, "失败时来源保留在模型中");

        // 6d. 合并圆弧（采样为直线段接入）
        SketchModel m4;
        Element tgt4 = ElementFactory::createPolyline(
            {Vec3(0,0,0), Vec3(10,0,0), Vec3(20,0,0)}, {false,false}, false, &m4);
        const int tid4 = m4.addElement(tgt4);
        Element arc;
        arc.type = ElementType::Arc;
        arc.pts = { Vec3(20,10,0) };   // 圆心
        arc.radius = 10;
        arc.startAngle = 270;          // 端点 (20,0)
        arc.endAngle   = 360;          // 端点 (30,10)
        const int aid = m4.addElement(arc);
        check(join(m4, tid4, aid, 1e-3), "合并圆弧 join 成功");
        {
            const Element* p = m4.element(tid4);
            check(p && (int)p->pts.size() >= 4, "合并圆弧后目标点数增加（采样段）",
                  p ? QString("n=%1").arg(p->pts.size()) : "");
            check(ptEq(p->pts.back(), 30,10), "合并圆弧末点正确 (30,10)");
            check(m4.element(aid) == nullptr, "来源圆弧已从模型删除");
        }
    }

    // ==================== 7. 命中检测尊重曲线模式 ====================
    printf("=== 7. 命中检测 (distanceToElement) 尊重 Fit/Spline ===\n");
    {
        SketchModel m;
        std::vector<Vec3> pts = { Vec3(0,0,0), Vec3(10,10,0), Vec3(20,0,0), Vec3(30,10,0) };
        Element e = ElementFactory::createPolyline(pts, {false,false,false}, false, &m);
        const int id = m.addElement(e);
        fit(m, id);
        // 顶点应在曲线上（距离≈0）
        const Vec3 midVert = pts[2];
        const double dVert = geo::distanceToElement(*m.element(id), midVert);
        check(dVert < 1e-4, "Fit 状态下顶点命中距离≈0", QString("d=%1").arg(dVert));
    }

    if (g_fail == 0) {
        printf("\nALL PASS (PolylineEditOpsTest)\n");
        logf("ALL PASS\n");
    } else {
        printf("\nFAILED: %d\n", g_fail);
        logf("FAILED: %d\n", g_fail);
    }
    if (g_log) fclose(g_log);
    return g_fail == 0 ? 0 : 1;
}

static Element makeLine(double x0, double y0, double x1, double y1) {
    Element e; e.type = ElementType::Line;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) };
    return e;
}
