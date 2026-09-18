// 剖面填充「点选异常复现诊断」(GB/T 4457.5-2013)
// ----------------------------------------------------------------------------
// 本文件是【复现诊断】而非修复：把用户报告的三个(及一个补充)T 形/分割线异常行为
// 写成可运行场景，记录「期望 vs 实际」的真实行为，供后续决策。绝不直接修改核心代码。
//
// 场景 1：矩形 + 贯穿竖线，填充左侧后再点同一区域 —— 用户报告「已填充区域重新进入时被重新划分」。
// 场景 2：菱形 + 斜贯穿线 —— 用户报告「拆两半填充时仍被当作一个整体」。
// 场景 3：分割线完整性 —— 用户报告「填充完成后分割线被错误剪切」。
// 场景 4（补充）：分割线端点恰好落在矩形边上(T 形) —— 当前未提交改动针对的情况，区分两种变体。
//
// 每个场景用 printf 输出详细诊断：pick.reason、边界顶点数、area、boundarySrcIds、
// holeSrcIds、exclusiveSrcIds、每个 modify::Change(id/remove/replacement 类型)。
// 纯核心层，依赖 QCoreApplication(SketchModel 为 QObject)，无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>
#include <QColor>

static const char* reasonStr(RegionPickReason r) {
    switch (r) {
        case RegionPickReason::Ok:           return "Ok";
        case RegionPickReason::NoLoopAtPoint: return "NoLoopAtPoint";
        case RegionPickReason::AlreadyFilled: return "AlreadyFilled";
        case RegionPickReason::Degenerate:    return "Degenerate";
    }
    return "?";
}

// 打印一个拾取结果的完整诊断
static void dumpPick(const char* tag, const RegionPickResult& p) {
    printf("  [%s] reason=%s found=%d loopIndex=%d totalLoops=%d\n",
           tag, reasonStr(p.reason), (int)p.found, p.loopIndex, p.totalLoops);
    printf("      boundaryVerts=%d area=%.4f holes=%d\n",
           (int)p.boundary.size(), p.area, (int)p.holes.size());
    printf("      boundarySrcIds=[%s]\n", idsToStr(p.boundarySrcIds).toUtf8().constData());
    printf("      holeSrcIds=[%s]\n", idsToStr(p.holeSrcIds).toUtf8().constData());
    printf("      exclusiveSrcIds=[%s]\n", idsToStr(p.exclusiveSrcIds).toUtf8().constData());
}

// 打印一组 Change 的明细
static void printChanges(const std::vector<modify::Change>& chs) {
    for (size_t i = 0; i < chs.size(); ++i) {
        const auto& c = chs[i];
        const char* rtype = c.replacement.type == ElementType::Hatch ? "Hatch"
                          : c.replacement.type == ElementType::Rectangle ? "Rectangle"
                          : c.replacement.type == ElementType::Line ? "Line"
                          : c.replacement.type == ElementType::Polygon ? "Polygon"
                          : c.replacement.type == ElementType::Polyline ? "Polyline"
                          : c.replacement.type == ElementType::Circle ? "Circle"
                          : "?";
        printf("      Change[%d] id=%d remove=%d replType=%s replId=%d\n",
               (int)i, c.id, (int)c.remove, rtype, c.replacement.id);
    }
}

// 统计模型里各类型图元数量
static void countTypes(const SketchModel& m, int& nRect, int& nLine, int& nHatch, int& nPoly, int& nOther) {
    nRect = nLine = nHatch = nPoly = nOther = 0;
    for (const auto& e : m.elements()) {
        if (e.type == ElementType::Rectangle) ++nRect;
        else if (e.type == ElementType::Line) ++nLine;
        else if (e.type == ElementType::Hatch) ++nHatch;
        else if (e.type == ElementType::Polygon || e.type == ElementType::Polyline) ++nPoly;
        else ++nOther;
    }
}

// 找模型中第一条 Line 的端点（用于"分割线完整性"诊断）；找不到则 found=false
static void findLineEndpoints(const SketchModel& m, bool& found, Vec3& a, Vec3& b) {
    found = false;
    for (const auto& e : m.elements()) {
        if (e.type == ElementType::Line) { found = true; a = e.pts[0]; b = e.pts[1]; return; }
    }
}

// 多边形有向面积的两倍（鞋带公式），用于诊断 Hatch 边界面积。
static double polyArea2(const std::vector<Vec3>& poly) {
    const size_t n = poly.size();
    if (n < 3) return 0.0;
    double a2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % n];
        a2 += a.x * b.y - b.x * a.y;
    }
    return a2;
}

static Vec3 g_s1LineA, g_s1LineB;     // 场景1 原始分割线端点
static Vec3 g_s2LineA, g_s2LineB;     // 场景2 原始分割线端点

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充点选异常复现诊断（GB/T 4457.5-2013）===\n");

    // ================= 场景1：矩形 + 贯穿竖线，填充后再进入同一区域 =================
    // 期望：第一次 pick(30,25) → Ok, area≈3000；落盘后重入 pick(30,25) → AlreadyFilled；
    //       pick(80,25) 仍 Ok, area≈2000。若重入不报 AlreadyFilled，则视为异常(可能重复填充)。
    {
        printf("\n========== 场景1: 矩形(0,0)-(100,50) + 贯穿竖线(60,-20)->(60,70) ==========\n");
        SketchModel model;
        int rid = model.addElement(makeRect(0, 0, 100, 50));
        int lid = model.addElement(makeLine(Vec3(60, -20, 0), Vec3(60, 70, 0)));
        g_s1LineA = Vec3(60, -20, 0); g_s1LineB = Vec3(60, 70, 0);
        printf("  原始: rect id=%d, line id=%d\n", rid, lid);

        // 第一次 pick (30,25)
        std::vector<Element> srcs = model.elements();
        RegionPickResult p1 = pickSectionRegionAt(srcs, Vec3(30, 25, 0));
        dumpPick("第一次pick(30,25)", p1);
        check(p1.ok(), "场景1 第一次pick(30,25) → Ok");
        check(std::fabs(p1.area - 3000.0) < 50.0, "场景1 第一次pick area≈3000", QString::number(p1.area));

        // 落盘（真实 SketchModel 路径）
        SectionResult res1 = makeSectionChangeForPick(p1, srcs, SectionStyle());
        printf("  第一次落盘 changes=%d:\n", (int)res1.changes.size());
        printChanges(res1.changes);
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res1.changes, QString());
        {
            int nRect, nLine, nHatch, nPoly, nOther;
            countTypes(model, nRect, nLine, nHatch, nPoly, nOther);
            printf("  第一次落盘后模型: rect=%d line=%d hatch=%d poly=%d other=%d total=%d\n",
                   nRect, nLine, nHatch, nPoly, nOther, (int)model.elements().size());
        }

        // 重新 build + pick (30,25)
        std::vector<Element> srcs2 = model.elements();
        SectionLoopSet set2 = buildSectionLoopSet(srcs2, kDefaultClosureTol);
        printf("  重解析 loopSet: loops=%d\n", (int)set2.loops.size());
        for (size_t i = 0; i < set2.loops.size(); ++i) {
            printf("    loop[%d] verts=%d area=%.2f srcIds=[%s] hatchId=%d\n",
                   (int)i, (int)set2.loops[i].size(), set2.loopArea[i],
                   idsToStr(set2.loopSrcIds[i]).toUtf8().constData(), set2.loopHatchId[i]);
        }
        RegionPickResult p2 = pickRegionAt(set2, Vec3(30, 25, 0));
        dumpPick("重入pick(30,25)", p2);
        if (p2.reason == RegionPickReason::AlreadyFilled) {
            printf("  [结论] 场景1 重入返回 AlreadyFilled —— 符合期望，未复现「重新划分」异常。\n");
            check(true, "场景1 重入 → AlreadyFilled（期望，无异常）");
        } else {
            printf("  [!!异常] 场景1 重入未返回 AlreadyFilled（实为 %s）；进一步生成 changes：\n",
                   reasonStr(p2.reason));
            check(false, "场景1 重入 → AlreadyFilled（实际未触发，异常复现）", reasonStr(p2.reason));
            SectionResult res2 = makeSectionChangeForPick(p2, srcs2, SectionStyle());
            printChanges(res2.changes);
            SketchModel m2;
            for (const auto& e : srcs2) m2.addElement(e);
            m2.beginEdit(QStringLiteral("重入"));
            modify::applyChanges(&m2, res2.changes, QString());
            int nH = 0; for (const auto& e : m2.elements()) if (e.type == ElementType::Hatch) ++nH;
            printf("  重入落盘后 Hatch 数=%d（>1 表示产生重复填充）\n", nH);
            check(nH <= 1, "场景1 重入落盘后 Hatch 数 ≤ 1（未重复填充）", QString::number(nH));
        }

        // 右侧 pick (80,25)
        RegionPickResult pR = pickSectionRegionAt(srcs2, Vec3(80, 25, 0));
        dumpPick("右侧pick(80,25)", pR);
        check(pR.ok(), "场景1 右侧(80,25) → 仍 Ok");
        check(std::fabs(pR.area - 2000.0) < 50.0, "场景1 右侧 area≈2000", QString::number(pR.area));

        // 保存模型供场景3 检查分割线存活
        {
            bool found; Vec3 a, b;
            findLineEndpoints(model, found, a, b);
            printf("  场景1 落盘后分割线 found=%d a=(%.4f,%.4f) b=(%.4f,%.4f)\n",
                   (int)found, a.x, a.y, b.x, b.y);
        }
    }

    // ================= 场景2：菱形 + 斜贯穿线 =================
    // 期望：菱形被斜线切成两半，每半 ~4 顶点、area≈菱形一半；填充一侧后分割线不被删/改；
    //      另一侧仍可独立拾取。若菱形被当成整体(6 顶点/area≈2400)，则异常复现。
    {
        printf("\n========== 场景2: 菱形(闭合Polyline) + 斜贯穿线(-50,-10)->(50,25) ==========\n");
        std::vector<Vec3> diamond = { Vec3(0, 40, 0), Vec3(30, 0, 0),
                                      Vec3(0, -40, 0), Vec3(-30, 0, 0) };
        SketchModel model;
        int did = model.addElement(makeClosedPolyline(diamond));
        int lid = model.addElement(makeLine(Vec3(-50, -10, 0), Vec3(50, 25, 0)));
        g_s2LineA = Vec3(-50, -10, 0); g_s2LineB = Vec3(50, 25, 0);
        printf("  原始: diamond id=%d, line id=%d\n", did, lid);

        std::vector<Element> srcs = model.elements();

        // ---- 诊断探针：菱形是否被识别为自闭合 + 与分割线的 transverse 交点数 ----
        {
            Element dE = makeClosedPolyline(diamond);
            std::vector<Vec3> dout;
            bool sc = sectionOutline(dE, dout);   // 内部调用 sectionSelfClosedOutline
            printf("[PROBE] sectionOutline(diamond)=%d outPts=%d\n", (int)sc, (int)dout.size());
            Element lE = makeLine(Vec3(-50, -10, 0), Vec3(50, 25, 0));
            int hits = 0;
            const int na = (int)dout.size();
            for (int i = 0; i < na; ++i) {
                const Vec3 pa = dout[i], pb = dout[(i + 1) % na];
                Vec3 P;
                if (geo::segSegIntersect(pa, pb, lE.pts[0], lE.pts[1], P)) {
                    const Vec3 d1 = pb - pa, d2 = lE.pts[1] - lE.pts[0];
                    if (std::fabs(d1.x * d2.y - d1.y * d2.x) < 1e-9) continue;
                    ++hits;
                    printf("[PROBE]   hit#%d (%.4f,%.4f) seg[%d]\n", hits, P.x, P.y, i);
                }
            }
            printf("[PROBE] diamond x line transverse hits = %d\n", hits);
        }

        SectionLoopSet set = buildSectionLoopSet(srcs, kDefaultClosureTol);
        printf("  解析 loopSet: loops=%d（期望 2：被斜线切成两半）\n", (int)set.loops.size());
        check(set.loops.size() == 2, "场景2 菱形被斜线切成 2 个闭环", QString::number(set.loops.size()));
        for (size_t i = 0; i < set.loops.size(); ++i) {
            printf("    loop[%d] verts=%d area=%.2f srcIds=[%s]\n",
                   (int)i, (int)set.loops[i].size(), set.loopArea[i],
                   idsToStr(set.loopSrcIds[i]).toUtf8().constData());
            printf("      coords:");
            for (const auto& v : set.loops[i])
                printf(" (%.3f,%.3f)", v.x, v.y);
            printf("\n");
        }

        // 取每个环的中心点作为"该半区"的拾取点
        std::vector<Vec3> centers;
        for (const auto& lp : set.loops) {
            if (lp.size() < 3) continue;
            double cx = 0, cy = 0;
            for (const auto& p : lp) { cx += p.x; cy += p.y; }
            Vec3 c(cx / (double)lp.size(), cy / (double)lp.size(), 0);
            if (geo::pointInPolygon(c, lp)) centers.push_back(c);
        }
        printf("  可拾取半区中心数=%d\n", (int)centers.size());

        RegionPickResult pHalf1, pHalf2;
        if (centers.size() >= 1) { pHalf1 = pickSectionRegionAt(srcs, centers[0]); dumpPick("半区1 pick", pHalf1); }
        if (centers.size() >= 2) { pHalf2 = pickSectionRegionAt(srcs, centers[1]); dumpPick("半区2 pick", pHalf2); }

        // 补充：用户报告的点 (-12,0) 落在哪一侧
        RegionPickResult pUser = pickSectionRegionAt(srcs, Vec3(-12, 0, 0));
        dumpPick("用户点(-12,0) pick", pUser);

        // 期望：被斜线切成两个独立半区（各为有效闭合环、面积>0），两半面积之和≈菱形
        // 总面积 2400；不再被当成整体(单环 6 顶点 / area≈2400)。注：斜线未过菱形中心，
        // 两半面积并不相等，故只校验"之和=2400"而非各自=1200。
        if (centers.size() >= 1) {
            check(pHalf1.boundary.size() >= 3, "场景2 半区1 为有效闭合环(顶点≥3)",
                  QString::number(pHalf1.boundary.size()));
            check(pHalf1.area > 100.0, "场景2 半区1 area>0（有效区域）",
                  QString::number(pHalf1.area));
        }
        if (centers.size() >= 2) {
            check(pHalf2.boundary.size() >= 3, "场景2 半区2 为有效闭合环(顶点≥3)",
                  QString::number(pHalf2.boundary.size()));
            const double sum = pHalf1.area + pHalf2.area;
            check(std::fabs(sum - 2400.0) < 5.0, "场景2 两半面积之和≈菱形总面积2400",
                  QString::number(sum));
        } else {
            printf("  [!!异常] 菱形未被细分，所有拾取点都命中整体（verts=%d area=%.2f）\n",
                   (int)pUser.boundary.size(), pUser.area);
            check(false, "场景2 菱形被正确细分为两半（实际未细分，异常复现）");
        }

        // 落盘半区1，检查 Hatch 边界面积与分割线是否被删/改
        if (pHalf1.ok()) {
            SectionResult resH1 = makeSectionChangeForPick(pHalf1, srcs, SectionStyle());
            printf("  半区1 落盘 changes=%d:\n", (int)resH1.changes.size());
            printChanges(resH1.changes);
            model.beginEdit(QStringLiteral("剖面填充-半区1"));
            modify::applyChanges(&model, resH1.changes, QString());
            {
                int nRect, nLine, nHatch, nPoly, nOther;
                countTypes(model, nRect, nLine, nHatch, nPoly, nOther);
                printf("  半区1 落盘后模型: rect=%d line=%d hatch=%d poly=%d other=%d total=%d\n",
                       nRect, nLine, nHatch, nPoly, nOther, (int)model.elements().size());
            }
            // Hatch 边界是半区还是整个菱形？
            const Element* h = nullptr;
            for (const auto& e : model.elements()) if (e.type == ElementType::Hatch) { h = &e; break; }
            if (h) {
                const SectionRegion back = hatchRegionFromElement(*h);
                double harea = std::fabs(polyArea2(back.boundary)) * 0.5;
                printf("  Hatch 边界 verts=%d area=%.4f（半区应≈1200，整体≈2400）\n",
                       (int)back.boundary.size(), harea);
                // 分割线是否被删/改？
                bool found; Vec3 a, b;
                findLineEndpoints(model, found, a, b);
                bool intact = found && ((a.distTo(g_s2LineA) <= 1e-6 && b.distTo(g_s2LineB) <= 1e-6) ||
                                       (a.distTo(g_s2LineB) <= 1e-6 && b.distTo(g_s2LineA) <= 1e-6));
                printf("  落盘后分割线 found=%d 端点吻合原始=%d a=(%.4f,%.4f) b=(%.4f,%.4f)\n",
                       (int)found, (int)intact, a.x, a.y, b.x, b.y);
                check(intact, "场景2 半区1 落盘后分割线未被剪切（端点吻合原始）");
                if (!found) printf("  [!!异常] 场景2 分割线被删除！\n");
            }
            // 再对另一半区 pick
            std::vector<Element> srcs2 = model.elements();
            SectionLoopSet set2 = buildSectionLoopSet(srcs2, kDefaultClosureTol);
            printf("  半区1 落盘后重解析 loops=%d\n", (int)set2.loops.size());
            RegionPickResult pOther;
            if (centers.size() >= 2) pOther = pickSectionRegionAt(srcs2, centers[1]);
            else pOther = pUser;
            dumpPick("另一半区 pick(落盘后)", pOther);
            check(pOther.ok(), "场景2 另一半区仍可独立拾取");
        }
    }

    // ================= 场景3：分割线完整性（场景1 & 场景2 落盘后） =================
    // 期望：分割线图元仍存在，且两端点与原始端点一致(±1e-6)。若被删/几何改变则报告是哪个 Change。
    {
        printf("\n========== 场景3: 分割线完整性（场景1/2 落盘后） ==========\n");
        // 重新构造场景1 的模型并落盘（与场景1 同构，保证独立可验证）
        SketchModel m1;
        m1.addElement(makeRect(0, 0, 100, 50));
        m1.addElement(makeLine(g_s1LineA, g_s1LineB));
        {
            std::vector<Element> s = m1.elements();
            RegionPickResult p = pickSectionRegionAt(s, Vec3(30, 25, 0));
            SectionResult r = makeSectionChangeForPick(p, s, SectionStyle());
            m1.beginEdit(QStringLiteral("剖面填充"));
            modify::applyChanges(&m1, r.changes, QString());
        }
        bool f1; Vec3 a1, b1;
        findLineEndpoints(m1, f1, a1, b1);
        bool ok1 = f1 && ((a1.distTo(g_s1LineA) <= 1e-6 && b1.distTo(g_s1LineB) <= 1e-6) ||
                          (a1.distTo(g_s1LineB) <= 1e-6 && b1.distTo(g_s1LineA) <= 1e-6));
        printf("  场景1 分割线 found=%d 端点吻合=%d a=(%.4f,%.4f) b=(%.4f,%.4f)\n",
               (int)f1, (int)ok1, a1.x, a1.y, b1.x, b1.y);
        check(ok1, "场景3 场景1 落盘后分割线存活且端点吻合（未被剪切）");

        // 重新构造场景2 的模型并落盘半区1
        SketchModel m2;
        std::vector<Vec3> diamond = { Vec3(0, 40, 0), Vec3(30, 0, 0),
                                      Vec3(0, -40, 0), Vec3(-30, 0, 0) };
        m2.addElement(makeClosedPolyline(diamond));
        m2.addElement(makeLine(g_s2LineA, g_s2LineB));
        {
            std::vector<Element> s = m2.elements();
            SectionLoopSet set = buildSectionLoopSet(s, kDefaultClosureTol);
            Vec3 center; bool has = false;
            for (const auto& lp : set.loops) {
                if (lp.size() < 3) continue;
                double cx = 0, cy = 0; for (const auto& p : lp) { cx += p.x; cy += p.y; }
                Vec3 c(cx / (double)lp.size(), cy / (double)lp.size(), 0);
                if (geo::pointInPolygon(c, lp)) { center = c; has = true; break; }
            }
            if (has) {
                RegionPickResult p = pickSectionRegionAt(s, center);
                SectionResult r = makeSectionChangeForPick(p, s, SectionStyle());
                m2.beginEdit(QStringLiteral("剖面填充-半区1"));
                modify::applyChanges(&m2, r.changes, QString());
            }
        }
        bool f2; Vec3 a2, b2;
        findLineEndpoints(m2, f2, a2, b2);
        bool ok2 = f2 && ((a2.distTo(g_s2LineA) <= 1e-6 && b2.distTo(g_s2LineB) <= 1e-6) ||
                          (a2.distTo(g_s2LineB) <= 1e-6 && b2.distTo(g_s2LineA) <= 1e-6));
        printf("  场景2 分割线 found=%d 端点吻合=%d a=(%.4f,%.4f) b=(%.4f,%.4f)\n",
               (int)f2, (int)ok2, a2.x, a2.y, b2.x, b2.y);
        check(ok2, "场景3 场景2 落盘后分割线存活且端点吻合（未被剪切）");
    }

    // ================= 场景4（补充）：分割线端点恰好落在矩形边上(T 形) =================
    // 区分两种变体：A) 下端点落在矩形内部(60,25)-(60,80)；B) 下端点恰好落在上边(60,50)-(60,80)。
    // 期望/观察：端点落边/落内的竖线只与矩形有 1 个边界交点 → 不应把矩形切开，pick 应命中整个矩形。
    {
        printf("\n========== 场景4: T 形（分割线端点落边） ==========\n");
        // 变体 A：下端点(60,25) 在矩形内部
        {
            printf("  -- 变体A: 矩形(0,0)-(100,50) + 竖线(60,25)->(60,80)（下端点落在矩形内部）--\n");
            std::vector<Element> elems = { makeRect(0, 0, 100, 50),
                                           makeLine(Vec3(60, 25, 0), Vec3(60, 80, 0)) };
            SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
            printf("  解析 loops=%d（若=1 表示矩形未被切开；=2 表示被切开）\n", (int)set.loops.size());
            for (size_t i = 0; i < set.loops.size(); ++i)
                printf("    loop[%d] verts=%d area=%.2f srcIds=[%s]\n",
                       (int)i, (int)set.loops[i].size(), set.loopArea[i],
                       idsToStr(set.loopSrcIds[i]).toUtf8().constData());
            RegionPickResult p = pickSectionRegionAt(elems, Vec3(60, 35, 0)); // 线内部、矩形内部
            dumpPick("变体A pick(60,35)", p);
            check(set.loops.size() >= 1, "场景4A 解析出 ≥1 闭环");
        }
        // 变体 B：下端点(60,50) 恰好落在上边
        {
            printf("  -- 变体B: 矩形(0,0)-(100,50) + 竖线(60,50)->(60,80)（下端点恰好落在上边）--\n");
            std::vector<Element> elems = { makeRect(0, 0, 100, 50),
                                           makeLine(Vec3(60, 50, 0), Vec3(60, 80, 0)) };
            SectionLoopSet set = buildSectionLoopSet(elems, kDefaultClosureTol);
            printf("  解析 loops=%d（若=1 表示矩形未被切开；=2 表示被切开）\n", (int)set.loops.size());
            for (size_t i = 0; i < set.loops.size(); ++i)
                printf("    loop[%d] verts=%d area=%.2f srcIds=[%s]\n",
                       (int)i, (int)set.loops[i].size(), set.loopArea[i],
                       idsToStr(set.loopSrcIds[i]).toUtf8().constData());
            RegionPickResult p = pickSectionRegionAt(elems, Vec3(60, 30, 0)); // 矩形内部
            dumpPick("变体B pick(60,30)", p);
            check(set.loops.size() >= 1, "场景4B 解析出 ≥1 闭环");
        }
        printf("  说明：T 形竖线仅与矩形有 1 个边界交点 → 当前 transverseHit 阈值(≥2 交点)不触发细分，\n");
        printf("        两变体均预期命中整个矩形；若某变体被切开成两半属需要进一步确认的行为。\n");
    }

    printf("\n=== 诊断汇总：PASS=%d FAIL=%d ===\n", g_pass, g_fail);
    printf("（注：本测试为复现诊断，FAIL 表示「期望行为未被满足」，可能是异常复现，需结合上文[结论]/[!!异常]判断。）\n");
    if (g_fail == 0) { printf("RESULT: ALL_EXPECTED_BEHAVIOR_HOLD\n"); return 0; }
    printf("RESULT: SOME_CHECKS_FAILED\n");
    return 1;
}
