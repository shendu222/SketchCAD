// 剖面填充「曲线分割」复现诊断（临时，不提交）
#include "section_test_helpers.h"
#include <QCoreApplication>

static void dumpPick(const char* tag, const RegionPickResult& p) {
    printf("  [%s] reason=%d found=%d area=%.2f verts=%d srcIds=[%s] holes=%d excl=[%s]\n",
           tag, (int)p.reason, (int)p.found, p.area, (int)p.boundary.size(),
           idsToStr(p.boundarySrcIds).toUtf8().constData(), (int)p.holes.size(),
           idsToStr(p.exclusiveSrcIds).toUtf8().constData());
}
static void dumpLoop(const char* tag, const std::vector<Vec3>& lp) {
    printf("    %s verts=%d:", tag, (int)lp.size());
    for (const auto& v : lp) printf(" (%.2f,%.2f)", v.x, v.y);
    printf("\n");
}
static double polyArea2(const std::vector<Vec3>& poly) {
    size_t n = poly.size(); if (n < 3) return 0.0;
    double a2 = 0.0;
    for (size_t i = 0; i < n; ++i) { const Vec3& a = poly[i]; const Vec3& b = poly[(i+1)%n]; a2 += a.x*b.y - b.x*a.y; }
    return a2;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 曲线分割复现诊断 ===\n");

    // 场景A：矩形 + 右凸样条(两端落在矩形上下边上，作为分割线)
    {
        printf("\n-- A. 矩形(0,0)-(100,60) + 样条(50,0)->右凸->(50,60) --\n");
        std::vector<Vec3> ctrl = { Vec3(50,0,0), Vec3(85,15,0), Vec3(85,45,0), Vec3(50,60,0) };
        Element sp; sp.type = ElementType::Spline; sp.pts = ctrl; sp.id = 2;
        std::vector<Element> elems = { makeRect(0,0,100,60,1), sp };
        SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
        printf("  loops=%d\n", (int)set.loops.size());
        for (size_t i = 0; i < set.loops.size(); ++i)
            dumpLoop((QStringLiteral("loop[%1]").arg(i)).toUtf8().constData(), set.loops[i]);
        RegionPickResult pl = pickSectionRegionAt(elems, Vec3(25,30,0));
        RegionPickResult pr = pickSectionRegionAt(elems, Vec3(90,30,0));
        dumpPick("pick left(25,30)", pl);
        dumpPick("pick right(90,30)", pr);
        // 关键：右区边界不应包含任何 x < 50 的点（若有 → 出现多余的「直线」跨回 x=50，等于溢出）
        if (pr.ok()) {
            bool bad = false;
            for (const auto& v : pr.boundary) if (v.x < 50.0 - 1e-6) { bad = true; break; }
            printf("  [检查] 右区边界是否含 x<50 的越界点: %s\n", bad ? "YES(溢出!)" : "no");
        }
    }

    // 场景B：矩形 + 开放样条横向分隔(左端/右端落在左右边上，中间下凹)
    {
        printf("\n-- B. 矩形(0,0)-(100,60) + 横向样条(0,30)->下凹->(100,30) --\n");
        std::vector<Vec3> ctrl = { Vec3(0,30,0), Vec3(30,8,0), Vec3(70,8,0), Vec3(100,30,0) };
        Element sp; sp.type = ElementType::Spline; sp.pts = ctrl; sp.id = 4;
        std::vector<Element> elems = { makeRect(0,0,100,60,3), sp };
        SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
        printf("  loops=%d\n", (int)set.loops.size());
        for (size_t i = 0; i < set.loops.size(); ++i)
            dumpLoop((QStringLiteral("loop[%1]").arg(i)).toUtf8().constData(), set.loops[i]);
        RegionPickResult pt = pickSectionRegionAt(elems, Vec3(50,50,0));
        RegionPickResult pb = pickSectionRegionAt(elems, Vec3(50,15,0));
        dumpPick("pick top(50,50)", pt);
        dumpPick("pick bottom(50,15)", pb);
    }

    // 场景C：圆弧(开放)作为「右凸」分割线 + 矩形
    {
        printf("\n-- C. 矩形(0,0)-(100,60) + 圆弧(圆心(50,30) r=40, 右半弧) --\n");
        // 右半弧：从(50,-10)? 不对。用「半圆弧」横跨矩形：圆心(50,30),r=40，右侧弧从(50,-10)到(50,70)
        // 实际右半圆弧 x=50+40cos, y=30+40sin；起始角 90°->y=70，扫到 -90°->y=-10，右半。端点(50,70),(50,-10) 在矩形上下延伸处。
        Element ar = makeArc(Vec3(50,30,0), 40.0, 90.0, -90.0, 5); // start=90 end=-90 扫掠 -180 => 右侧半圆
        std::vector<Element> elems = { makeRect(0,0,100,60,6), ar };
        SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
        printf("  loops=%d\n", (int)set.loops.size());
        for (size_t i = 0; i < set.loops.size(); ++i)
            dumpLoop((QStringLiteral("loop[%1]").arg(i)).toUtf8().constData(), set.loops[i]);
        RegionPickResult pl = pickSectionRegionAt(elems, Vec3(25,30,0));
        RegionPickResult pr = pickSectionRegionAt(elems, Vec3(88,30,0));
        dumpPick("pick left(25,30)", pl);
        dumpPick("pick right(88,30)", pr);
    }

    // 场景D：一条样条分隔线同时「跨过」两个并排矩形（跨区域）
    {
        printf("\n-- D. 两个并排矩形 + 一条竖样条分隔线跨两区 --\n");
        // 下部矩形 (0,0)-(100,30)，上部矩形 (0,35)-(100,65)，竖样条 x=50 从 y=-10 到 y=75，右凸
        std::vector<Vec3> ctrl = { Vec3(50,-10,0), Vec3(65,10,0), Vec3(65,20,0), Vec3(50,35,0),
                                   Vec3(65,50,0), Vec3(65,65,0), Vec3(50,75,0) };
        Element sp; sp.type = ElementType::Spline; sp.pts = ctrl; sp.id = 9;
        std::vector<Element> elems = { makeRect(0,0,100,30,7), makeRect(0,35,100,65,8), sp };
        SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
        printf("  loops=%d\n", (int)set.loops.size());
        for (size_t i = 0; i < set.loops.size(); ++i) {
            printf("    loop[%d] area=%.2f srcIds=[%s]", (int)i, set.loopArea[i],
                   idsToStr(set.loopSrcIds[i]).toUtf8().constData());
            printf("\n");
        }
        // 点左上区域 (25,50)
        RegionPickResult p = pickSectionRegionAt(elems, Vec3(25,50,0));
        dumpPick("pick 左上(25,50)", p);
        if (p.ok()) {
            // 落盘并检查中间分隔线是否被剪切
            SketchModel model;
            for (const auto& e : elems) model.addElement(e);
            std::vector<Element> srcs = model.elements();
            RegionPickResult rp = pickSectionRegionAt(srcs, Vec3(25,50,0));
            SectionResult res = makeSectionChangeForPick(rp, srcs, SectionStyle());
            printf("  changes=%d:\n", (int)res.changes.size());
            for (const auto& c : res.changes)
                printf("    Change id=%d remove=%d type=%d\n", c.id, (int)c.remove, (int)c.replacement.type);
        }
    }

    printf("\nPASS=%d FAIL=%d\n", g_pass, g_fail);
    return 0;
}