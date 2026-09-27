// 查询测量核心层测试（Inquiry：dist / area / massprop / id + list 命令层）
//
// 覆盖：
//   1. distTwoPoints 两点测量（距离/增量/方向角）
//   2. 多边形环底层（面积/周长/形心/惯性矩）
//   3. 弧感知折线精确周长/有向面积（geo::polylineArcLength / polylineSignedArea）
//   4. areaOfElement 各封闭图元分派（圆/矩形/多边形/闭合折线含弧段/剖面区域带孔/边界拒绝）
//   5. massPropOfElement 质量特性（解析惯性矩对照 + 带孔复合区域）
//   6. idPoint 最近图元
//   7. 命令层：dist/area/massprop/id/list 的参数解析、输出格式与无效输入处理
//
// 纯核心层（不依赖 UI 头文件），以 exit code 为权威判定（stdout 可能丢失 → 落盘）。
#include <QCoreApplication>
#include <QObject>
#include <cstdio>
#include <cmath>

#include "core/Element.h"
#include "core/Vec.h"
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Geometry.h"
#include "core/Inquiry.h"
#include "core/Commands.h"

using namespace SketchCAD;
using namespace SketchCAD::inquiry;

static int g_fail = 0;
static FILE* g_log = nullptr;
static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fflush(g_log);
}
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { logf("  [PASS] %s\n", what); }
    else   { logf("  [FAIL] %s (%s)\n", what, detail.toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

static constexpr double kPi = 3.14159265358979323846;

// 命令层日志收集器（连接 SketchModel::logAppended，无上下文重载）
struct LogCapture {
    QStringList lines;
    explicit LogCapture(SketchModel& m) {
        QObject::connect(&m, &SketchModel::logAppended,
                         [this](const QString& msg, const QString&) { lines << msg; });
    }
    bool anyContains(const QString& s) const {
        for (const auto& l : lines) if (l.contains(s)) return true;
        return false;
    }
};

// 执行一条命令行（经 CommandRegistry 分派，与真实入口一致）
static bool runCmd(SketchModel& m, const QString& line) {
    QStringList toks = line.split(' ', Qt::SkipEmptyParts);
    const QString cmd = toks[0].toLower();
    ICommand* c = CommandRegistry::instance().findExact(cmd);
    if (!c) return false;
    CommandContext ctx{ &m, toks, cmd };
    return c->execute(ctx);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    (void)argc; (void)argv;
    g_log = fopen("inquiry_test_result.txt", "w");
    if (g_log) logf("=== InquiryTest ===\n");

    // ==================== 1. DIST：两点测量 ====================
    logf("== 1. distTwoPoints ==\n");
    {
        auto r1 = distTwoPoints(Vec3(0, 0, 0), Vec3(10, 0, 0));
        check(near(r1.distance, 10) && near(r1.dx, 10) && near(r1.dy, 0) && near(r1.angleDeg, 0),
              "水平两点：距离 10 / ΔX=10 / 角 0°");

        auto r2 = distTwoPoints(Vec3(0, 0, 0), Vec3(3, 4, 0));
        check(near(r2.distance, 5) && near(r2.angleDeg, std::atan2(4.0, 3.0) * 180.0 / kPi),
              "3-4-5 斜线：距离 5 / 角度 atan2(4,3)");

        auto r3 = distTwoPoints(Vec3(0, 0, 0), Vec3(-1, -1, 0));
        check(near(r3.angleDeg, -135) && near(r3.distance, kPi * 0 + std::sqrt(2.0)),
              "第三象限：角 -135° / 距离 √2");

        auto r4 = distTwoPoints(Vec3(7, 8, 0), Vec3(7, 8, 0));
        check(near(r4.distance, 0) && near(r4.angleDeg, 0), "重合点：距离 0");
    }

    // ==================== 2. 多边形环底层 ====================
    logf("== 2. polygon 底层 ==\n");
    {
        std::vector<Vec3> sq = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,20,0), Vec3(0,20,0) };
        check(near(polygonArea(sq), 200), "矩形环有向面积 200");
        check(near(polygonPerimeter(sq), 60), "矩形环周长 60");
        const Vec3 c = polygonCentroid(sq);
        check(near(c.x, 5) && near(c.y, 10), "矩形环形心 (5,10)");

        std::vector<Vec3> sqCW(sq.rbegin(), sq.rend());
        check(near(polygonArea(sqCW), -200), "CW 方向 → 有向面积 -200");

        double ix, iy, ixy;
        polygonInertia(sq, ix, iy, ixy);   // 绕原点
        // 解析：绕原点 Ix = w·h³/12 + A·cy² = 10·20³/12 + 200·10² = 26666.67
        check(near(ix, 10.0 * 20.0 * 20.0 * 20.0 / 12.0 + 200.0 * 100.0, 1e-4),
              "矩形绕原点 Ix = wh³/12 + A·cy²", QString::number(ix));
        check(near(iy, 20.0 * 10.0 * 10.0 * 10.0 / 12.0 + 200.0 * 25.0, 1e-4),
              "矩形绕原点 Iy = hw³/12 + A·cx²", QString::number(iy));
        check(near(ixy, 200.0 * 5.0 * 10.0, 1e-4),
              "矩形绕原点 Ixy = A·cx·cy（形心轴为 0）", QString::number(ixy));
    }

    // ==================== 3. 弧感知折线（Geometry 新函数）====================
    logf("== 3. polylineArcLength / polylineSignedArea ==\n");
    {
        // 无弧段闭合正方形：与 chord 和一致
        std::vector<Vec3> sq = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0) };
        check(near(geo::polylineArcLength(sq, {}, true), 40), "无弧段闭合正方形周长 40");
        check(near(geo::polylineSignedArea(sq, {}, true), 100), "无弧段闭合正方形面积 100");

        // 圆上三点 A(10,0) B(0,10) C(-10,0)，闭合，段 0(A→B) 为弧：
        //   外接圆 = 单位圆心原点 r=10；修复后弧段 CCW +90°（凸向远离 C）
        std::vector<Vec3> arc = { Vec3(10,0,0), Vec3(0,10,0), Vec3(-10,0,0) };
        std::vector<bool> flags = { true, false, false };
        const double arcLen = 90.0 / 180.0 * kPi * 10.0;   // 5π
        check(near(geo::polylineArcLength(arc, flags, true), arcLen + 10.0 * std::sqrt(2.0) + 20.0, 1e-9),
              "弧段周长 = 5π + 10√2 + 20",
              QString::number(geo::polylineArcLength(arc, flags, true)));
        // 面积 = 弦三角形 100 − 弦贡献 50 + 弧贡献 25π = 50 + 25π
        check(near(geo::polylineSignedArea(arc, flags, true), 50.0 + 25.0 * kPi, 1e-9),
              "弧段面积 = 50 + 25π（弓形修正精确）",
              QString::number(geo::polylineSignedArea(arc, flags, true)));

        // 端点保持不变式：弧段采样终点 = b
        const geo::ArcSeg as = geo::arcThrough(arc, 0, true);
        check(as.valid && near(as.sweepDeg, 90.0, 1e-9), "arcThrough 段 0 sweep = +90°（端点保持）",
              QString::number(as.sweepDeg));
        auto dense = geo::samplePolyline(arc, flags, true, 64);
        // 段 0 的最后一个采样点应落在 b=(0,10) 附近（±1 个角步长）
        if (dense.size() > 66) {
            const Vec3& nearEnd = dense[63];
            check(nearEnd.distTo(Vec3(0, 10, 0)) < 2.0 * kPi * 10.0 / 64.0 + 1e-9,
                  "弧段采样终点逼近 b（端点连续）",
                  QString("d=%1").arg(nearEnd.distTo(Vec3(0, 10, 0))));
        }
    }

    // ==================== 4. AREA：图元分派 ====================
    logf("== 4. areaOfElement ==\n");
    {
        SketchModel m;
        auto circle = ElementFactory::createCircle(Vec3(5, 5, 0), 10, &m);
        auto r = areaOfElement(circle);
        check(r.ok && near(r.area, 100 * kPi, 1e-9) && near(r.perimeter, 20 * kPi, 1e-9)
              && near(r.centroid.x, 5) && near(r.centroid.y, 5),
              "圆 r=10：面积 100π / 周长 20π / 形心=圆心");

        auto rect = ElementFactory::createRect(Vec3(0, 0, 0), Vec3(10, 20, 0), &m);
        r = areaOfElement(rect);
        check(r.ok && near(r.area, 200) && near(r.perimeter, 60)
              && near(r.centroid.x, 5) && near(r.centroid.y, 10),
              "矩形 10×20：面积 200 / 周长 60 / 形心 (5,10)");

        auto tri = ElementFactory::createPolygon(
            { Vec3(0,0,0), Vec3(10,0,0), Vec3(0,10,0) }, &m);
        r = areaOfElement(tri);
        check(r.ok && near(r.area, 50) && near(r.perimeter, 20.0 + 10.0 * std::sqrt(2.0)),
              "三角形：面积 50 / 周长 20+10√2");

        // 闭合折线含弧段（§3 同一构型）
        auto plArc = ElementFactory::createPolyline(
            { Vec3(10,0,0), Vec3(0,10,0), Vec3(-10,0,0) }, { true, false, false }, true, &m);
        r = areaOfElement(plArc);
        check(r.ok && near(r.area, 50.0 + 25.0 * kPi, 1e-9)
              && near(r.perimeter, 5.0 * kPi + 10.0 * std::sqrt(2.0) + 20.0, 1e-9),
              "闭合折线含弧段：精确面积/周长", QString("A=%1 P=%2").arg(r.area).arg(r.perimeter));

        // 剖面区域带孔（孔的方向归一化：CW 与 CCW 两种给法结果一致）
        auto hatchCCW = ElementFactory::create(ElementType::Hatch, QStringLiteral("剖面"), &m);
        hatchCCW.pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0) };
        hatchCCW.holes = { { Vec3(3,3,0), Vec3(7,3,0), Vec3(7,7,0), Vec3(3,7,0) } };  // CCW 孔
        r = areaOfElement(hatchCCW);
        check(r.ok && near(r.area, 84) && near(r.perimeter, 56)
              && near(r.centroid.x, 5) && near(r.centroid.y, 5),
              "剖面区域 10×10 带 4×4 孔：面积 84 / 周长 56 / 净形心居中",
              QString("A=%1 P=%2").arg(r.area).arg(r.perimeter));

        auto hatchCW = hatchCCW;
        hatchCW.holes = { { Vec3(3,7,0), Vec3(7,7,0), Vec3(7,3,0), Vec3(3,3,0) } };  // CW 孔
        auto r2 = areaOfElement(hatchCW);
        check(r2.ok && near(r2.area, 84) && near(r2.perimeter, 56),
              "孔方向归一化：CW 孔与 CCW 孔结果一致");

        // 开放折线 / 直线：拒绝并给出原因
        auto plOpen = ElementFactory::createPolyline(
            { Vec3(0,0,0), Vec3(10,0,0) }, { false }, false, &m);
        r = areaOfElement(plOpen);
        check(!r.ok && r.error.contains(QStringLiteral("未闭合")), "开放折线拒绝并说明原因", r.error);

        auto line = ElementFactory::createLine(Vec3(0,0,0), Vec3(1,0,0), &m);
        r = areaOfElement(line);
        check(!r.ok && r.error.contains(QStringLiteral("封闭")), "直线拒绝并说明类型集", r.error);
    }

    // ==================== 5. MASSPROP：质量特性 ====================
    logf("== 5. massPropOfElement ==\n");
    {
        SketchModel m;
        // 矩形 10×20 位于原点：IxBar = 10·20³/12 = 6666.67，IyBar = 20·10³/12 = 1666.67
        auto rect = ElementFactory::createRect(Vec3(0, 0, 0), Vec3(10, 20, 0), &m);
        auto r = massPropOfElement(rect);
        check(r.ok && near(r.ixBar, 10.0 * 20.0 * 20.0 * 20.0 / 12.0, 1e-4)
              && near(r.iyBar, 20.0 * 10.0 * 10.0 * 10.0 / 12.0, 1e-4)
              && near(r.ixyBar, 0, 1e-4),
              "矩形形心轴惯性矩解析一致 (wh³/12)", QString("Ix=%1 Iy=%2").arg(r.ixBar).arg(r.iyBar));
        check(near(r.ix, r.ixBar + 200.0 * 100.0, 1e-4) && near(r.iy, r.iyBar + 200.0 * 25.0, 1e-4),
              "世界轴惯性矩 = 形心轴 + A·d²（平行轴）");
        // QRectF 为屏幕语义（y 向下）：top()=min-y 边、bottom()=max-y 边
        check(near(r.bbox.left(), 0) && near(r.bbox.top(), 0)
              && near(r.bbox.width(), 10) && near(r.bbox.height(), 20), "矩形包围盒",
              QString("L=%1 T=%2 W=%3 H=%4").arg(r.bbox.left()).arg(r.bbox.top())
                  .arg(r.bbox.width()).arg(r.bbox.height()));

        // 圆 r=10 圆心在原点：IxBar = IyBar = πr⁴/4
        auto circle = ElementFactory::createCircle(Vec3(0, 0, 0), 10, &m);
        r = massPropOfElement(circle);
        check(r.ok && near(r.ixBar, kPi * 10000.0 / 4.0, 1e-6)
              && near(r.iyBar, kPi * 10000.0 / 4.0, 1e-6) && near(r.ixyBar, 0, 1e-9),
              "圆解析惯性矩 πr⁴/4", QString("Ix=%1").arg(r.ixBar));
        check(near(r.ix, kPi * 10000.0 / 4.0, 1e-6) && near(r.bbox.width(), 20),
              "圆心在原点时世界轴 = 形心轴 / 包围盒 20×20");

        // 带孔复合区域：10×10 外框 − 4×4 孔(3,3)~(7,7)
        //   IxBar 净 = 10·10³/12 − 4·4³/12 = 812
        auto hatch = ElementFactory::create(ElementType::Hatch, QStringLiteral("剖面"), &m);
        hatch.pts = { Vec3(0,0,0), Vec3(10,0,0), Vec3(10,10,0), Vec3(0,10,0) };
        hatch.holes = { { Vec3(3,3,0), Vec3(7,3,0), Vec3(7,7,0), Vec3(3,7,0) } };
        r = massPropOfElement(hatch);
        check(r.ok && near(r.area, 84) && near(r.ixBar, 10.0 * 1000.0 / 12.0 - 4.0 * 64.0 / 12.0, 1e-4),
              "带孔复合区域：面积 84 / IxBar = 812（解析一致）",
              QString("A=%1 Ix=%2").arg(r.area).arg(r.ixBar));
    }

    // ==================== 6. ID：点查询 + 最近图元 ====================
    logf("== 6. idPoint ==\n");
    {
        SketchModel m;
        auto l1 = ElementFactory::createLine(Vec3(0,0,0), Vec3(10,0,0), &m);
        m.addElement(l1);
        auto c1 = ElementFactory::createCircle(Vec3(50, 50, 0), 5, &m);
        m.addElement(c1);

        const auto& es = m.elements();
        auto r = idPoint(Vec3(5, 1, 0), es);
        check(r.nearest != nullptr && r.nearest->type == ElementType::Line
              && near(r.nearestDist, 1.0), "点 (5,1) 最近图元 = 直线 / 距离 1");

        r = idPoint(Vec3(0, 0, 0), es);
        check(r.nearest != nullptr && r.nearest->type == ElementType::Line && near(r.nearestDist, 0),
              "点在直线上：距离 0");
    }

    // ==================== 7. 命令层：dist / area / massprop / id / list ====================
    logf("== 7. 命令层 ==\n");
    {
        SketchModel m;
        LogCapture cap(m);
        const int rectId = m.addElement(ElementFactory::createRect(Vec3(0, 0, 0), Vec3(10, 20, 0), &m));
        // 直线放在矩形轮廓之外（若与矩形下边共线，查询点到二者距离并列，
        // idPoint 按向量顺序取先者 → 最近对象不确定）
        const int lineId = m.addElement(ElementFactory::createLine(Vec3(30, 0, 0), Vec3(40, 0, 0), &m));

        // --- dist 正常 / 异常 ---
        cap.lines.clear();
        check(runCmd(m, "dist 0,0 10,0"), "dist 正常执行");
        check(cap.anyContains(QStringLiteral("距离 = 10.0000")) && cap.anyContains(QStringLiteral("ΔX = 10")),
              "dist 输出距离与增量");
        cap.lines.clear();
        check(!runCmd(m, "dist"), "dist 无参数 → 失败");
        check(cap.anyContains(QStringLiteral("用法")), "dist 无参数 → 用法提示");
        cap.lines.clear();
        check(!runCmd(m, "dist abc 10,0"), "dist 点解析失败 → 失败");
        check(cap.anyContains(QStringLiteral("解析失败")), "dist 点解析失败提示");

        // --- area 对象模式 ---
        cap.lines.clear();
        check(runCmd(m, "area " + QString::number(rectId)), "area <矩形id> 执行");
        check(cap.anyContains(QStringLiteral("面积 = 200.0000")) && cap.anyContains(QStringLiteral("周长 = 60.0000")),
              "area 输出面积与周长");
        cap.lines.clear();
        check(!runCmd(m, "area " + QString::number(lineId)), "area <直线id> → 失败（非封闭）");
        check(cap.anyContains(QStringLiteral("不是封闭区域")), "area 非封闭图元提示类型集");

        // --- area 点列模式 ---
        cap.lines.clear();
        check(runCmd(m, "area 0,0 10,0 10,8"), "area 点列模式执行");
        check(cap.anyContains(QStringLiteral("面积 = 40.0000")), "点列三角形面积 40",
              cap.lines.join(" | "));
        check(!runCmd(m, "area 0,0 10,0"), "点列不足 3 点 → 失败");
        check(!runCmd(m, "area 999"), "area 不存在 id → 失败");

        // --- area 选区模式 ---
        m.clearSelection();
        m.selectEntity(rectId);
        cap.lines.clear();
        check(runCmd(m, "area"), "area 无参用选区");
        check(cap.anyContains(QStringLiteral("面积 = 200")), "选区模式统计选中矩形");
        m.clearSelection();
        cap.lines.clear();
        check(!runCmd(m, "area"), "无参且无选区 → 用法提示");
        check(cap.anyContains(QStringLiteral("用法")), "area 空选区给出用法");

        // --- massprop ---
        cap.lines.clear();
        check(runCmd(m, "massprop " + QString::number(rectId)), "massprop <矩形id> 执行");
        check(cap.anyContains(QStringLiteral("质量特性")) && cap.anyContains(QStringLiteral("形心"))
              && cap.anyContains(QStringLiteral("惯性矩")) && cap.anyContains(QStringLiteral("包围盒")),
              "massprop 输出面积/周长/包围盒/形心/惯性矩");
        cap.lines.clear();
        check(!runCmd(m, "massprop"), "massprop 无参无选区 → 失败");
        check(cap.anyContains(QStringLiteral("用法")), "massprop 用法提示");

        // --- id ---
        cap.lines.clear();
        check(runCmd(m, "id 35,2"), "id 执行");
        check(cap.anyContains(QStringLiteral("坐标: X = 35.0000")), "id 输出坐标");
        check(cap.anyContains(QStringLiteral("最近图元")) && cap.anyContains(QString("#%1").arg(lineId)),
              "id 附带最近图元与距离");
        cap.lines.clear();
        check(!runCmd(m, "id"), "id 无参数 → 失败");
        check(cap.anyContains(QStringLiteral("用法")), "id 用法提示");

        // --- list（增强后）---
        cap.lines.clear();
        check(runCmd(m, "list"), "list 全量执行");
        check(cap.anyContains(QStringLiteral("共 2 个")), "list 头部含计数");
        check(cap.anyContains(QStringLiteral("[图层 0]")), "list 行含图层信息");
        check(cap.anyContains(QStringLiteral("面积 200.00")), "list 封闭图元附面积/周长/形心行");
        cap.lines.clear();
        check(runCmd(m, "list " + QString::number(rectId)), "list 指定 id 执行");
        check(cap.anyContains(QStringLiteral("矩形")), "list 单对象输出类型");
        cap.lines.clear();
        check(!runCmd(m, "list 999"), "list 不存在 id → 失败");
    }

    // ============================================================
    // 8. 直线链围合区域（首尾相接 Line → 闭合环 → 面积）
    // ============================================================
    {
        logf("\n--- 8. 直线链围合区域 ---\n");
        // 五边形（参考附图形状的规整化坐标，shoelace 手算校验）
        // 顶点: A(0,0) B(40,0) C(55,25) D(30,50) E(5,30)
        // shoelace: |Σ|/2 = (0 + 1000 + 2000 + 650 + 0)/2 = 1825
        const Vec3 A(0, 0, 0), B(40, 0, 0), C(55, 25, 0), D(30, 50, 0), E(5, 30, 0);

        // --- 8.1 buildRingFromLines：乱序+部分反向输入也能串环 ---
        {
            std::vector<Element> segs;
            segs.push_back(ElementFactory::createLine(C, D, nullptr));   // 反向存放
            segs.push_back(ElementFactory::createLine(E, A, nullptr));   // 反向存放
            segs.push_back(ElementFactory::createLine(A, B, nullptr));
            segs.push_back(ElementFactory::createLine(D, E, nullptr));
            segs.push_back(ElementFactory::createLine(B, C, nullptr));
            std::vector<Vec3> ring;
            check(inquiry::buildRingFromLines(segs, ring), "buildRingFromLines：乱序/反向直线串成闭合环");
            check(ring.size() == 5, "串环得到 5 个顶点（不重复首点）", QString::number(ring.size()));
            const auto r = inquiry::areaOfClosedLineChain(segs);
            check(r.ok && near(r.area, 1825.0), "areaOfClosedLineChain：五边形面积 = 1825 mm²",
                  QString::number(r.area));
            check(near(r.perimeter, 40 + std::hypot(15, 25) + std::hypot(25, 25)
                            + std::hypot(25, 20) + std::hypot(5, 30)),
                  "五边形周长（逐边解析和）", QString::number(r.perimeter));
        }

        // --- 8.2 缺口/非直线拒绝 ---
        {
            std::vector<Element> segs;
            segs.push_back(ElementFactory::createLine(A, B, nullptr));
            segs.push_back(ElementFactory::createLine(B, C, nullptr));
            segs.push_back(ElementFactory::createLine(C, D, nullptr));
            segs.push_back(ElementFactory::createLine(E, A, nullptr));   // D→E 缺一段
            std::vector<Vec3> ring;
            check(!inquiry::buildRingFromLines(segs, ring), "缺口链 → 串环失败");
            const auto r = inquiry::areaOfClosedLineChain(segs);
            check(!r.ok && r.error.contains(QStringLiteral("封闭环")), "缺口链 → error 指明未围成封闭环");
        }

        // --- 8.3 命令层：选中五条直线 → area 直线链模式 ---
        {
            SketchModel m;
            std::vector<int> ids;
            ids.push_back(m.addElement(ElementFactory::createLine(C, D, &m)));
            ids.push_back(m.addElement(ElementFactory::createLine(E, A, &m)));
            ids.push_back(m.addElement(ElementFactory::createLine(A, B, &m)));
            ids.push_back(m.addElement(ElementFactory::createLine(D, E, &m)));
            ids.push_back(m.addElement(ElementFactory::createLine(B, C, &m)));
            m.setSelection(ids);                 // selectEntity 是替换式选中，须用 setSelection 批量选中
            LogCapture cap(m);
            check(runCmd(m, "area"), "area（选区为 5 条首尾相接直线）执行");
            check(cap.anyContains(QStringLiteral("直线链模式")), "area 输出直线链模式标识");
            check(cap.anyContains(QStringLiteral("面积 = 1825")), "直线链面积 = 1825 mm²");
            check(cap.anyContains(QStringLiteral("5 条边")), "输出边数");

            // 指定 id 模式同样走直线链
            cap.lines.clear();
            check(runCmd(m, QStringLiteral("area %1 %2 %3 %4 %5")
                             .arg(ids[0]).arg(ids[1]).arg(ids[2]).arg(ids[3]).arg(ids[4])),
                  "area <5个直线id> 执行");
            check(cap.anyContains(QStringLiteral("面积 = 1825")), "指定 id 直线链面积一致");

            // 缺口选区 → 明确报错
            SketchModel m2;
            const int g1 = m2.addElement(ElementFactory::createLine(A, B, &m2));
            const int g2 = m2.addElement(ElementFactory::createLine(B, C, &m2));
            const int g3 = m2.addElement(ElementFactory::createLine(C, D, &m2));
            const int g4 = m2.addElement(ElementFactory::createLine(E, A, &m2));
            m2.setSelection({g1, g2, g3, g4});
            LogCapture cap2(m2);
            check(!runCmd(m2, "area"), "缺口直线选区 → area 失败");
            check(cap2.anyContains(QStringLiteral("封闭环")), "缺口报错指明未围成封闭环");
        }
    }

    if (g_log) {
        logf("\n=== %s（失败 %d 项）===\n", g_fail == 0 ? "ALL PASS" : "HAS FAILURES", g_fail);
        fclose(g_log);
    }
    return g_fail == 0 ? 0 : 1;
}
