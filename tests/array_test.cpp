// 阵列复制（矩形 / 环形）核心层回归测试
//   * makeRectangularArray：原图元不动、仅新增平移副本、副本数 = rows·cols − 1、
//     副本几何平移正确、图层/颜色/线宽/线型/文本等属性完整继承；
//   * makePolarArray：原图元不动、仅新增绕 center 旋转副本、副本数 = count − 1、
//     旋转后几何位置正确、Arc 起止角与 Ellipse 长轴方向随旋转同步 +angle、属性完整继承。
// 运行平台：纯核心层，无需 GUI。
#include "core/Modify.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>
#include <cmath>

using namespace SketchCAD;

static const double kPi = 3.14159265358979323846;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) printf("  [PASS] %s\n", what);
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near2(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

static Element mkLine(double x1, double y1, double x2, double y2) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}

int main() {
    // ============ 矩形阵列 ============
    {
        Element src = mkLine(0, 0, 10, 0);
        src.layer = QStringLiteral("螺栓孔");
        src.color = QColor(200, 30, 30);
        src.lineWidth = 2.0; src.lineWidthAuto = false;
        src.lineStyle = LineStyle::Dashed;
        src.text = QStringLiteral("标注文字");

        const int rows = 3, cols = 2;
        const double rsp = 20.0, csp = 10.0;
        auto ch = modify::makeRectangularArray({src}, rows, cols, rsp, csp);

        // 1) 副本数 = rows·cols − 1
        check((int)ch.size() == rows * cols - 1,
              "矩形阵列：副本数 = 行×列−1",
              QStringLiteral("got %1 want %2").arg(ch.size()).arg(rows * cols - 1));

        // 2) 全部为新增副本（id < 0），原图元未被改动
        bool allNew = true;
        for (const auto& c : ch) if (c.id >= 0 || c.replacement.id >= 0) allNew = false;
        check(allNew, "矩形阵列：所有副本为新增（id<0）");
        check(near2(src.pts[0].x, 0) && near2(src.pts[1].x, 10),
              "矩形阵列：原图元几何未被修改");

        // 3) 属性完整继承
        bool attrOk = true;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (e.layer != src.layer) attrOk = false;
            if (e.color != src.color) attrOk = false;
            if (e.lineWidth != src.lineWidth || e.lineWidthAuto != src.lineWidthAuto) attrOk = false;
            if (e.lineStyle != src.lineStyle) attrOk = false;
            if (e.text != src.text) attrOk = false;
        }
        check(attrOk, "矩形阵列：副本完整继承图层/颜色/线宽/线型/文本");

        // 4) 几何平移正确：(r,c) → 偏移 (c·csp, r·rsp)
        //    取 (r=1,c=1) 副本，应平移 (10,20)
        bool found = false;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, 10) && near2(e.pts[0].y, 20) &&
                near2(e.pts[1].x, 20) && near2(e.pts[1].y, 20)) { found = true; break; }
        }
        check(found, "矩形阵列：副本 (1,1) 平移到 (10,20)~(20,20)");

        // 5) 不含与原件重合的 (0,0) 副本
        bool hasOrigin = false;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, 0) && near2(e.pts[0].y, 0) &&
                near2(e.pts[1].x, 10) && near2(e.pts[1].y, 0)) hasOrigin = true;
        }
        check(!hasOrigin, "矩形阵列：不含与原件重合的 (0,0) 副本");

        // 6) 1×1 不产生任何副本
        check(modify::makeRectangularArray({src}, 1, 1, 10, 10).empty(),
              "矩形阵列：1×1 时不产生副本");

        // 7) 多源：N 个源 → N·(rows·cols−1) 个副本
        Element src2 = mkLine(100, 100, 110, 100);
        auto ch2 = modify::makeRectangularArray({src, src2}, rows, cols, rsp, csp);
        check((int)ch2.size() == 2 * (rows * cols - 1),
              "矩形阵列：多源副本数 = N·(行×列−1)");
    }

    // ============ 环形阵列 ============
    {
        // 源：一条位于 center 右侧的水平线段 (10,0)-(20,0)；center=(0,0)
        Element src = mkLine(10, 0, 20, 0);
        src.layer = QStringLiteral("轮齿");
        src.color = QColor(30, 120, 200);
        src.lineStyle = LineStyle::DashDotDot;

        const Vec3 center(0, 0, 0);
        const int count = 4;
        const double range = 360.0;
        auto ch = modify::makePolarArray({src}, center, count, range);

        // 1) 副本数 = count − 1
        check((int)ch.size() == count - 1,
              "环形阵列：副本数 = count−1",
              QStringLiteral("got %1 want %2").arg(ch.size()).arg(count - 1));

        // 2) 全部新增副本，原图元未改
        bool allNew = true;
        for (const auto& c : ch) if (c.id >= 0 || c.replacement.id >= 0) allNew = false;
        check(allNew, "环形阵列：所有副本为新增（id<0）");
        check(near2(src.pts[0].x, 10) && near2(src.pts[0].y, 0),
              "环形阵列：原图元几何未被修改");

        // 3) 属性完整继承
        bool attrOk = true;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (e.layer != src.layer) attrOk = false;
            if (e.color != src.color) attrOk = false;
            if (e.lineStyle != src.lineStyle) attrOk = false;
            if (e.radius != src.radius) attrOk = false;
        }
        check(attrOk, "环形阵列：副本完整继承图层/颜色/线型");

        // 4) 几何旋转正确：k=1 → 90°，(10,0)→(0,10)、(20,0)→(0,20)（竖直线）
        bool found90 = false;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, 0) && near2(e.pts[0].y, 10) &&
                near2(e.pts[1].x, 0) && near2(e.pts[1].y, 20)) { found90 = true; break; }
        }
        check(found90, "环形阵列：副本 k=1（90°）旋转到 (0,10)~(0,20)");

        // 5) k=2 → 180°，应到 (-10,0)~(-20,0)
        bool found180 = false;
        for (const auto& c : ch) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, -10) && near2(e.pts[0].y, 0) &&
                near2(e.pts[1].x, -20) && near2(e.pts[1].y, 0)) { found180 = true; break; }
        }
        check(found180, "环形阵列：副本 k=2（180°）旋转到 (-10,0)~(-20,0)");

        // 6) count < 2 → 空
        check(modify::makePolarArray({src}, center, 1, range).empty(),
              "环形阵列：count<2 时不产生副本");
        check(modify::makePolarArray({src}, center, 0, range).empty(),
              "环形阵列：count=0 时不产生副本");

        // 7) 八等分整圆（count=8, range=360）：副本在 45/90/.../315°
        auto ch8 = modify::makePolarArray({src}, center, 8, 360.0);
        check((int)ch8.size() == 7, "环形阵列：count=8 产生 7 个副本");
        // 取 k=1（45°）：(10,0)→(10cos45, 10sin45)=(7.071, 7.071)
        bool found45 = false;
        for (const auto& c : ch8) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, 10 * std::cos(kPi / 4)) &&
                near2(e.pts[0].y, 10 * std::sin(kPi / 4))) { found45 = true; break; }
        }
        check(found45, "环形阵列：count=8 时 k=1 落在 45°（≈7.071,7.071）");

        // 8) 非整圆角度范围：count=3, range=120 → 增量 40°，k=1 在 40°
        auto ch3 = modify::makePolarArray({src}, center, 3, 120.0);
        check((int)ch3.size() == 2, "环形阵列：count=3/120° 产生 2 个副本");
        bool found40 = false;
        for (const auto& c : ch3) {
            const Element& e = c.replacement;
            if (near2(e.pts[0].x, 10 * std::cos(40 * kPi / 180)) &&
                near2(e.pts[0].y, 10 * std::sin(40 * kPi / 180))) { found40 = true; break; }
        }
        check(found40, "环形阵列：count=3/120° 时 k=1 落在 40°");
    }

    // ============ Arc / Ellipse 角度随旋转同步 ============
    {
        // Arc：圆心 (10,0)，start=0, end=90；绕 (0,0) 旋转，count=4,range=360 → 步长 90°（k=1 在 90°）
        Element arc;
        arc.type = ElementType::Arc;
        arc.pts = {Vec3(10, 0, 0)};
        arc.radius = 5.0;
        arc.startAngle = 0.0; arc.endAngle = 90.0;
        auto chA = modify::makePolarArray({arc}, Vec3(0, 0, 0), 4, 360.0);
        check(chA.size() == 3, "Arc 环形：3 个副本（count=4）");
        const Element& ea = chA[0].replacement;   // k=1 → 90°
        check(near2(ea.pts[0].x, 0) && near2(ea.pts[0].y, 10),
              "Arc 环形：圆心旋转到 (0,10)");
        check(near2(ea.radius, 5.0), "Arc 环形：半径不变");
        check(near2(ea.startAngle, 90.0) && near2(ea.endAngle, 180.0),
              "Arc 环形：起止角随旋转 +90°（0→90, 90→180）");

        // Ellipse：圆心 (10,0)，rotation=0；绕 (0,0) 旋转，count=4 → 步长 90°（k=1 在 90°）
        Element ell;
        ell.type = ElementType::Ellipse;
        ell.pts = {Vec3(10, 0, 0)};
        ell.radius = 8.0; ell.radiusY = 4.0; ell.rotation = 0.0;
        auto chE = modify::makePolarArray({ell}, Vec3(0, 0, 0), 4, 360.0);
        check(chE.size() == 3, "Ellipse 环形：3 个副本（count=4）");
        const Element& ee = chE[0].replacement;   // k=1 → 90°
        check(near2(ee.pts[0].x, 0) && near2(ee.pts[0].y, 10),
              "Ellipse 环形：圆心旋转到 (0,10)");
        check(near2(ee.radius, 8.0) && near2(ee.radiusY, 4.0),
              "Ellipse 环形：半轴不变");
        check(near2(ee.rotation, 90.0),
              "Ellipse 环形：长轴方向 rotation 随旋转 +90°");
    }

    printf("\nArrayTest: %s (%d failures)\n", g_fail == 0 ? "ALL PASS" : "HAS FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
