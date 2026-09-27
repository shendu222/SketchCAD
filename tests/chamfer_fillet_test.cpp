// 倒角（Chamfer）/ 倒圆（Fillet）核心层单元测试（corner 命名空间）。
//
// 覆盖需求：
//   1) 倒角（距离-距离）：沿两线各回退 d1/d2，补直线连接裁断点；
//   2) 倒角（距离-角度）：沿 a 回退 d，倒角线与 a 夹角 angleDeg，自动求 b 上退距；
//   3) 倒圆（半径）：补与两线相切的圆弧，圆心在角平分线上；
//   4) 边界：平行/共线/夹角过小/参数非法/距离越界 → 返回 false 且给出提示；
//   5) 保留端不变（自动模式保留远离角点一端；点选模式保留离 pick 最近一端）；
//   6) 输出几何：lineA/lineB/corner 三段齐备，类型正确（Line/Arc）。
//
// 纯核心层，无需 GUI。
#include <QCoreApplication>
#include <cstdio>
#include <cstdarg>
#include <cmath>

#include "core/Element.h"
#include "core/ChamferFillet.h"
#include "core/Vec.h"
#include "core/SketchModel.h"
#include "core/Modify.h"

using namespace SketchCAD;

static int g_fail = 0;
static FILE* g_log = nullptr;   // 沙箱 stdout 可能丢失，同时落盘到文件做持久证据
static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fflush(g_log);
}
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); logf("  [PASS] %s\n", what); }
    else   { const QString d = detail.isEmpty() ? "" : ("(" + detail + ")");
              printf("  [FAIL] %s %s\n", what, d.toUtf8().constData());
              logf("  [FAIL] %s %s\n", what, d.toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
static bool ptEq(const Vec3& p, double x, double y) { return near(p.x, x) && near(p.y, y); }
static bool hasEnd(const Element& e, double x, double y) {
    if (e.pts.size() < 2) return false;
    return ptEq(e.pts[0], x, y) || ptEq(e.pts[1], x, y);
}
static Element makeLine(double x0, double y0, double x1, double y1) {
    Element e; e.type = ElementType::Line; e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) }; return e;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    (void)argc; (void)argv;
    g_log = fopen("chamfer_fillet_result.txt", "w");
    if (g_log) logf("=== ChamferFilletTest ===\n");

    printf("=== 1. 倒角（距离-距离）===\n");
    {
        // 正交 L 角：a 沿 +x（0,0)->(100,0），b 沿 +y（0,0)->(0,100）
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);
        auto r = corner::chamferPair(a, b, 10.0, 20.0, /*byPick=*/false);
        check(r.ok, "chamferPair 成功");
        check(r.lineA.type == ElementType::Line && r.lineB.type == ElementType::Line, "两条裁短线为 Line");
        check(r.corner.type == ElementType::Line, "倒角段为 Line");
        // a 保留远离角的 (100,0)，裁断点 (10,0)；b 保留 (0,100)，裁断点 (0,20)
        check(hasEnd(r.lineA, 100, 0) && hasEnd(r.lineA, 10, 0), "lineA = (100,0)->(10,0)");
        check(hasEnd(r.lineB, 0, 100) && hasEnd(r.lineB, 0, 20), "lineB = (0,100)->(0,20)");
        check(hasEnd(r.corner, 10, 0) && hasEnd(r.corner, 0, 20), "倒角段连接 (10,0)-(0,20)");
    }

    printf("=== 2. 倒角（距离-角度）===\n");
    {
        // a 沿 +x，b 沿 +y；沿 a 退 10，倒角线相对 a 夹角 45°（朝 b 一侧）→ b 上退距=10（等腰）
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);
        auto r = corner::chamferPairDistAngle(a, b, 10.0, 45.0, false);
        check(r.ok, "chamferPairDistAngle 成功");
        check(hasEnd(r.corner, 10, 0) && hasEnd(r.corner, 0, 10), "45° 倒角 → b 上退距=10，连接 (10,0)-(0,10)");
        // 角度 30°：几何上 d2 = d·tan(ang) = 10·tan30° ≈ 5.7735（角点处 Δ 三角形）
        auto r2 = corner::chamferPairDistAngle(a, b, 10.0, 30.0, false);
        check(r2.ok, "30° 倒角成功");
        if (r2.ok) {
            const double expect = 10.0 * std::tan(30.0 * M_PI / 180.0);
            check(near(r2.lineB.pts[1].y, expect, 1e-4),
                  "30° 时 b 退距 = d·tan30° ≈ 5.7735", QString::number(r2.lineB.pts[1].y));
        }
    }

    printf("=== 3. 倒圆（半径）===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);
        auto r = corner::filletPair(a, b, 15.0, false);
        check(r.ok, "filletPair 成功");
        check(r.corner.type == ElementType::Arc, "圆角段为 Arc");
        check(near(r.corner.radius, 15.0), "圆弧半径 == 15");
        // 圆心应在 (15,15) 附近（r/sin45 * bis，bis=(√2/2,√2/2)，r√2≈21.21 → (15,15)）
        const Vec3 C = r.corner.pts[0];
        check(near(C.x, 15.0, 1e-4) && near(C.y, 15.0, 1e-4), "圆心 ≈ (15,15)",
              QString("%1,%2").arg(C.x).arg(C.y));
        // 裁断点：沿 +x 退 d = r/tan45 = 15 → (15,0)；沿 +y 退 15 → (0,15)
        check(hasEnd(r.lineA, 100, 0) && hasEnd(r.lineA, 15, 0), "lineA 裁断于 (15,0)");
        check(hasEnd(r.lineB, 0, 100) && hasEnd(r.lineB, 0, 15), "lineB 裁断于 (0,15)");
    }

    printf("=== 4. 平行 → 失败提示 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 50, 100, 50);
        auto r = corner::chamferPair(a, b, 5, 5, false);
        check(!r.ok, "平行返回 false");
        check(r.msg.contains(QStringLiteral("平行")), "提示含「平行」", r.msg);
    }

    printf("=== 5. 参数非法 / 距离越界 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);
        auto r1 = corner::filletPair(a, b, -5.0, false);
        check(!r1.ok && r1.msg.contains(QStringLiteral("正数")), "负半径被拒");
        auto r2 = corner::chamferPair(a, b, 200.0, 5.0, false);   // d1 超出 a 长度
        check(!r2.ok, "倒角距离超出线段长度被拒");
        check(r2.msg.contains(QStringLiteral("d1")), "提示定位 d1", r2.msg);
    }

    printf("=== 6. 角度越界（>= 两线夹角）===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);   // 与 a 夹角 90°
        auto r = corner::chamferPairDistAngle(a, b, 10.0, 95.0, false); // >90°
        check(!r.ok, "倒角角度超过两线夹角被拒");
        check(r.msg.contains(QStringLiteral("夹角")), "提示含「夹角」", r.msg);
    }

    printf("=== 7. 保留端：点选模式 ===\n");
    {
        // 两条线都越过角点（交叉 X 形），端点非对称以便「远离角点」判定无歧义：
        // a (-50,0)-(100,0) 远端点 (100,0)；b (0,-50)-(0,100) 远端点 (0,100)
        Element a = makeLine(-50, 0, 100, 0);
        Element b = makeLine(0, -50, 0, 100);
        // 点选 a 的 (100,0) 端 → 保留 (100,0)；点选 b 的 (0,100) 端 → 保留 (0,100)
        auto r = corner::chamferPair(a, b, 10.0, 10.0, /*byPick=*/true,
                                    Vec3(100, 0, 0), Vec3(0, 100, 0));
        check(r.ok, "点选模式成功");
        check(hasEnd(r.lineA, 100, 0), "a 保留端 (100,0) 不变");
        check(hasEnd(r.lineB, 0, 100), "b 保留端 (0,100) 不变");
        // 自动模式（无 pick）：保留远离角点端，同样应保留 (100,0)/(0,100)
        auto r2 = corner::chamferPair(a, b, 10.0, 10.0, false);
        check(hasEnd(r2.lineA, 100, 0) && hasEnd(r2.lineB, 0, 100), "自动模式保留远端");
    }

    printf("=== 8. 非直线图元被拒 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element c; c.type = ElementType::Circle; c.pts = {Vec3(0,0,0)}; c.radius = 5;
        auto r = corner::chamferPair(a, c, 5, 5, false);
        check(!r.ok, "非直线返回 false");
        check(r.msg.contains(QStringLiteral("直线")), "提示「直线」", r.msg);
    }

    printf("=== 9. 模型集成：makeCornerChanges + applyChanges ===\n");
    {
        // 把几何结果经 modify 适配层落到真实 SketchModel，验证端到端可用（命令行/菜单共用此路径）。
        SketchModel model;
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 0, 0, 100);
        const int id1 = model.addElement(a);
        const int id2 = model.addElement(b);
        check(model.elementCount() == 2, "初始 2 条线");

        auto res = corner::chamferPair(*model.element(id1), *model.element(id2), 10.0, 20.0);
        check(res.ok, "模型内倒角几何成功");
        auto changes = modify::makeCornerChanges(id1, id2, res);
        check(changes.size() == 5, "5 个 Change（2 删 + 2 裁短 + 1 倒角）", QString::number(changes.size()));
        model.beginEdit(QStringLiteral("测试-倒角"));
        auto added = modify::applyChanges(&model, changes, QString());
        check(added.size() == 3, "应用后新增 3 个图元", QString::number(added.size()));
        check(model.element(id1) == nullptr && model.element(id2) == nullptr, "原两直线已从模型移除");
        bool foundChamfer = false;
        for (const auto& e : model.elements()) {
            if (e.type == ElementType::Line && hasEnd(e, 10, 0) && hasEnd(e, 0, 20)) { foundChamfer = true; break; }
        }
        check(foundChamfer, "模型中存在倒角段 (10,0)-(0,20)");
        check(model.elementCount() == 3, "模型净增 1 个图元（2→3）", QString::number(model.elementCount()));
    }

    const char* verdict = g_fail == 0 ? "ChamferFilletTest: ALL PASS" : "ChamferFilletTest: FAILED";
    printf("\n%s\n", verdict);
    if (g_log) { logf("\n%s (fail=%d)\n", verdict, g_fail); fclose(g_log); g_log = nullptr; }
    return g_fail == 0 ? 0 : 1;
}
