// 对称「修剪 / 延伸」两条直线（trim::extendTrimPair）单元测试。
//
// 覆盖用户需求：
//   1) 两条线段已相交 → 修剪交点之外多余部分，仅保留「点选端 → 交点」；
//   2) 两条线段不相交 → 分别延长到交点，两条线恰交于一点；
//   3) 平行 / 共线 → 返回 false 并给出区分提示，且不修改；
//   4) 非直线图元 → 拒绝；
//   5) 浮点误差：近平行（方向夹角正弦 < 1e-9）按平行处理；
//   6) 仅移动「离点选端最远的端点」（保留端不变）；保留端已贴交点则保持原状。
//
// 纯核心层，无需 GUI。
#include <QCoreApplication>
#include <cstdio>
#include <cmath>

#include "core/Element.h"
#include "core/Trim.h"
#include "core/Vec.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
static bool ptEq(const Vec3& p, double x, double y) { return near(p.x, x) && near(p.y, y); }

static Element makeLine(double x0, double y0, double x1, double y1) {
    Element e;
    e.type = ElementType::Line;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) };
    return e;
}
// 某条结果线是否以 (x,y) 为端点之一
static bool hasEnd(const Element& e, double x, double y) {
    if (e.pts.size() < 2) return false;
    return ptEq(e.pts[0], x, y) || ptEq(e.pts[1], x, y);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    (void)argc; (void)argv;

    printf("=== 1. 已相交 → 修剪（保留点选端到交点）===\n");
    {
        Element a = makeLine(0, 0, 100, 0);     // 水平线
        Element b = makeLine(50, -50, 50, 50);   // 竖直线
        Element oa, ob; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0),    // 点选 a 的左端
                                             b, Vec3(50, -50, 0), // 点选 b 的下端
                                             oa, ca, ob, cb, msg);
        check(ok, "extendTrimPair 返回成功");
        check(ca && cb, "两条线都被修改");
        // a：保留 (0,0)，远端 (100,0)→交点(50,0)
        check(hasEnd(oa, 0, 0) && hasEnd(oa, 50, 0), "a 修剪为 (0,0)→(50,0)",
              QString("a.pts=(%1,%2)-(%3,%4)").arg(oa.pts[0].x).arg(oa.pts[0].y)
                .arg(oa.pts[1].x).arg(oa.pts[1].y));
        // b：保留 (50,-50)，远端 (50,50)→交点(50,0)
        check(hasEnd(ob, 50, -50) && hasEnd(ob, 50, 0), "b 修剪为 (50,-50)→(50,0)");
        // 交点一致
        check(ptEq(oa.pts[0], 0, 0) || ptEq(oa.pts[1], 0, 0), "a 保留端 (0,0) 未被移动");
    }

    printf("=== 2. 不相交 → 延伸（两条线交于一点）===\n");
    {
        Element a = makeLine(0, 0, 40, 0);    // 短水平线，右端不到 x=80
        Element b = makeLine(80, -10, 80, 10); // 竖直线在 x=80
        Element oa, ob; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0),
                                             b, Vec3(80, -10, 0),
                                             oa, ca, ob, cb, msg);
        check(ok, "extendTrimPair 返回成功");
        check(ca && cb, "两条线都被延伸");
        // 交点应为 (80,0)
        check(hasEnd(oa, 0, 0) && hasEnd(oa, 80, 0), "a 延伸至 (0,0)→(80,0)");
        check(hasEnd(ob, 80, -10) && hasEnd(ob, 80, 0), "b 延伸至 (80,-10)→(80,0)");
        // 两线确实交于同一点
        Vec3 pa = hasEnd(oa, 80, 0) ? Vec3(80, 0, 0) : Vec3();
        check(ptEq(pa, 80, 0), "a 的远端落在交点 (80,0)");
        check(hasEnd(ob, 80, 0), "b 的远端也落在交点 (80,0)");
    }

    printf("=== 3. 平行 → 提示且不修改 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 20, 100, 20);
        Element oa = a, ob = b; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0), b, Vec3(0, 20, 0), oa, ca, ob, cb, msg);
        check(!ok, "平行返回 false");
        check(msg.contains(QStringLiteral("平行")), "提示含「平行」", msg);
        check(!ca && !cb, "两条线均未被修改");
        check(oa.pts[1].x == 100 && ob.pts[1].x == 100, "平行时几何保持不变");
    }

    printf("=== 4. 共线 → 提示且不修改 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(30, 0, 200, 0);   // 与 a 同一直线 y=0
        Element oa = a, ob = b; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0), b, Vec3(30, 0, 0), oa, ca, ob, cb, msg);
        check(!ok, "共线返回 false");
        check(msg.contains(QStringLiteral("共线")), "提示含「共线」", msg);
        check(!ca && !cb, "两条线均未被修改");
    }

    printf("=== 5. 非直线图元 → 拒绝 ===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element c; c.type = ElementType::Circle; c.pts = { Vec3(0, 0, 0) }; c.radius = 5;
        Element oa = a, ob = c; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0), c, Vec3(0, 5, 0), oa, ca, ob, cb, msg);
        check(!ok, "非直线返回 false");
        check(msg.contains(QStringLiteral("不是直线")), "提示「不是直线」", msg);
    }

    printf("=== 6. 浮点容差：近平行按平行处理 ===\n");
    {
        // b 相对 a 仅偏转 ~1e-8 量级，方向夹角正弦 < 1e-9 → 视为平行
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(0, 1, 100, 1 + 1e-8);
        Element oa = a, ob = b; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(0, 0, 0), b, Vec3(0, 1, 0), oa, ca, ob, cb, msg);
        check(!ok, "近平行返回 false");
        check(msg.contains(QStringLiteral("平行")), "近平行提示「平行」", msg);
    }

    printf("=== 7. 仅动远端；保留端已贴交点则保持原状 ===\n");
    {
        // a 已有端点恰在交点 (50,0)，点选该端 → 该线不动；b 正常修剪
        Element a = makeLine(50, 0, 100, 0);   // 端点 (50,0) 即交点
        Element b = makeLine(50, -50, 50, 50);
        Element oa = a, ob = b; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(50, 0, 0),    // 点选 a 的端点(50,0)
                                             b, Vec3(50, -50, 0),
                                             oa, ca, ob, cb, msg);
        check(ok, "整体成功（b 仍被修改）");
        check(!ca, "a 保留端已贴交点 → 不修改");
        check(cb, "b 照常修剪");
        check(ptEq(oa.pts[0], 50, 0) && ptEq(oa.pts[1], 100, 0), "a 几何保持原状");
        check(hasEnd(ob, 50, -50) && hasEnd(ob, 50, 0), "b 修剪为 (50,-50)→(50,0)");
    }

    printf("=== 8. 点选另一端 → 保留该端（修剪方向随点选翻转）===\n");
    {
        Element a = makeLine(0, 0, 100, 0);
        Element b = makeLine(50, -50, 50, 50);
        // 这次点选 a 的右端 (100,0)：保留右端，左端 (0,0) 被修剪到交点
        Element oa, ob; bool ca = false, cb = false; QString msg;
        const bool ok = trim::extendTrimPair(a, Vec3(100, 0, 0),
                                             b, Vec3(50, -50, 0),
                                             oa, ca, ob, cb, msg);
        check(ok, "返回成功");
        check(hasEnd(oa, 100, 0) && hasEnd(oa, 50, 0), "a 改为 (100,0)→(50,0)（保留右端）");
        check(ptEq(oa.pts[0], 100, 0) || ptEq(oa.pts[1], 100, 0), "a 右端 (100,0) 未被移动");
    }

    printf("\n%s\n", g_fail == 0 ? "TrimExtendTest: ALL PASS" : "TrimExtendTest: FAILED");
    return g_fail == 0 ? 0 : 1;
}
