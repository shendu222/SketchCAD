// 剖面填充「闭合性」回归测试（GB/T 4457.5-2013）
// ----------------------------------------------------------------------------
// 锁定本次修复的核心不变量：
//   1) 开放折线 / 开放样条 / 孤立圆弧（非整圆）→ 必须被安全跳过（不产生 Hatch）；
//      —— 修复「开放样条被当闭合区域 → 填充渗漏/溢出真实轮廓」的历史缺陷。
//   2) 整圆弧（360°）→ 视为闭合。
//   3) 多图元（直线 / 圆弧 / 开放折线 / 开放样条）首尾相接围成的闭环 → 必须被识别
//      并成功生成（filledRegions==1，落盘后 Hatch 数量==1）。
//      —— 修复「多图元围成的封闭边界完全无法填充」的区域遗漏缺陷。
//   4) 生成的所有剖面线段两端点都必须落在边界上/内，且中点在边界内
//      —— 这是「不渗漏 / 不溢出」的硬验证。
//   5) 带内孔的环：孔心区域无剖面线，壁体中线有剖面线。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>

// ---- 本文件专有的构造辅助 ----
static Element makeSpline(const std::vector<Vec3>& pts, int id = 0) {
    Element e; e.type = ElementType::Spline; e.pts = pts; e.id = id; return e;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充闭合性回归（GB/T 4457.5-2013）===\n");

    // ---- 1. 开放折线：必须被安全跳过 ----
    {
        Element pl = makeOpenPolyline({ Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0) }, 10);
        std::vector<Vec3> out;
        check(!sectionOutline(pl, out), "开放折线 → sectionOutline 返回 false");

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements({ pl }, 0.5, &st);
        check(loops.empty(), "开放折线 → 0 个闭环");
        check(st.openChains == 1, "开放折线 → 计入 1 处未闭合链",
              QString::number(st.openChains));

        auto chs = makeSectionChanges({ pl }, SectionStyle(), false);
        check(chs.empty(), "开放折线 → 不产生任何剖面动作");
    }

    // ---- 2. 开放样条：必须被安全跳过（修复 D1 渗漏）----
    {
        Element sp = makeSpline({ Vec3(0, 0, 0), Vec3(5, 2, 0), Vec3(10, -2, 0), Vec3(15, 0, 0) }, 20);
        std::vector<Vec3> out;
        check(!sectionOutline(sp, out), "开放样条 → sectionOutline 返回 false（D1 修复）");

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements({ sp }, 0.5, &st);
        check(loops.empty(), "开放样条 → 0 个闭环");
        auto chs = makeSectionChanges({ sp }, SectionStyle(), false);
        check(chs.empty(), "开放样条 → 不产生任何剖面动作");

        // 防御：即便调用方（错误地）把开放曲线采样点列直接塞给 hatchLines，也必须拒绝填充。
        auto openSamples = geo::sampleSpline(sp.pts, 16);
        auto badSegs = geo::hatchLines(openSamples, {}, 45.0, 3.0, 3.0);
        check(badSegs.empty(), "hatchLines 对疑似开放点列拒绝填充（下游防御）");
    }

    // ---- 3. 孤立圆弧（90°，开放）跳过；整圆圆弧（360°）视为闭合 ----
    {
        std::vector<Vec3> out;
        Element q = makeArc(Vec3(0, 0, 0), 5.0, 0.0, 90.0, 30);
        check(!sectionOutline(q, out), "90° 圆弧（开放）→ 不构成剖面区域");
        SectionClosureStats st;
        check(buildClosedLoopsFromElements({ q }, 0.5, &st).empty(), "90° 圆弧 → 0 个闭环");
        check(st.openChains == 1, "90° 圆弧 → 计入未闭合链");

        Element full = makeArc(Vec3(0, 0, 0), 5.0, 0.0, 360.0, 31);
        check(sectionOutline(full, out) && out.size() >= 3, "整圆圆弧（360°）→ 闭合轮廓",
              QString::number(out.size()));
    }

    // ---- 4. 4 条直线首尾相接成环 → 成功填充（修复 D3 区域遗漏）----
    {
        std::vector<Element> sq;
        sq.push_back(makeLine(Vec3(0, 0, 0),   Vec3(10, 0, 0),  101));
        sq.push_back(makeLine(Vec3(10, 0, 0),  Vec3(10, 10, 0), 102));
        sq.push_back(makeLine(Vec3(10, 10, 0), Vec3(0, 10, 0),  103));
        sq.push_back(makeLine(Vec3(0, 10, 0),  Vec3(0, 0, 0),   104));

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(sq, 0.5, &st);
        check(loops.size() == 1, "4 条直线 → 1 个闭环", QString::number(loops.size()));
        check(st.chainedRegions == 1, "4 条直线 → 计入 1 个链式成环区域");

        auto r = makeSectionChangesEx(sq, SectionStyle(), false);
        check(r.filledRegions == 1, "4 条直线 → 成功生成 1 个剖面区域");
        int nHatch = 0, nRemove = 0;
        for (const auto& c : r.changes) {
            if (c.remove) ++nRemove;
            else if (c.replacement.type == ElementType::Hatch) ++nHatch;
        }
        check(nHatch == 1 && nRemove == 3, "动作 = 1 个 Hatch 替换 + 3 个合并删除",
              QString("hatch=%1 remove=%2").arg(nHatch).arg(nRemove));

        // 真实落盘：合并后仅剩 1 个 Hatch
        SketchModel model;
        std::vector<int> ids;
        for (auto& e : sq) ids.push_back(model.addElement(e));
        std::vector<Element> srcs;
        for (int id : ids) if (const Element* e = model.element(id)) srcs.push_back(*e);
        auto rr = makeSectionChangesEx(srcs, SectionStyle(), false);
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, rr.changes, QString());
        check(model.elements().size() == 1, "落盘后仅剩 1 个图元",
              QString::number(model.elements().size()));
        const Element* h = model.element(ids[0]);
        check(h && h->type == ElementType::Hatch, "首图元 ID 变为 Hatch");

        // 硬验证：所有剖面线端点在边界上、中点在内（不渗漏/不溢出）
        const std::vector<Vec3> loopPoly = square(0, 0, 10, 10);
        auto segs = generateHatchLines(hatchRegionFromElement(*h));
        check(!segs.empty(), "链式成环区域能生成剖面线", QString::number(segs.size()));
        bool allIn = true;
        for (const auto& s : segs) { QString why; if (!segWithinLoop(s, loopPoly, &why)) { allIn = false; break; } }
        check(allIn, "所有剖面线段均落在闭合边界内（端点在上、中点在内部）");
    }

    // ---- 5. 混合图元成环：2 段圆弧 + 2 条直线（跑道形）→ 成功且 hatchCount==1 ----
    {
        std::vector<Element> ring;
        ring.push_back(makeArc(Vec3(-5, 0, 0), 5.0, 90.0, 270.0, 201));   // 左半圆：(-5,5)→(-5,-5)
        ring.push_back(makeLine(Vec3(-5, -5, 0), Vec3(5, -5, 0), 202));   // 下边
        ring.push_back(makeArc(Vec3(5, 0, 0), 5.0, -90.0, 90.0, 203));    // 右半圆：(5,-5)→(5,5)
        ring.push_back(makeLine(Vec3(5, 5, 0), Vec3(-5, 5, 0), 204));     // 上边

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(ring, 0.5, &st);
        check(loops.size() == 1, "2 圆弧 + 2 直线 → 1 个闭环", QString::number(loops.size()));

        auto r = makeSectionChangesEx(ring, SectionStyle(), false);
        check(r.filledRegions == 1, "跑道形 → 成功生成 1 个剖面区域");

        SketchModel model;
        std::vector<int> ids;
        for (auto& e : ring) ids.push_back(model.addElement(e));
        std::vector<Element> srcs;
        for (int id : ids) if (const Element* e = model.element(id)) srcs.push_back(*e);
        auto rr = makeSectionChangesEx(srcs, SectionStyle(), false);
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, rr.changes, QString());
        int hatchCount = 0;
        for (const auto& e : model.elements()) if (e.type == ElementType::Hatch) ++hatchCount;
        check(hatchCount == 1, "跑道形落盘后 Hatch 数量 == 1", QString::number(hatchCount));
    }

    // ---- 6. 直线 + 样条混合成环 → 成功填充 ----
    {
        std::vector<Element> mix;
        mix.push_back(makeLine(Vec3(0, 0, 0), Vec3(20, 0, 0), 301));                        // 下边
        mix.push_back(makeSpline({ Vec3(20, 0, 0), Vec3(30, 0, 0), Vec3(30, 10, 0) }, 302)); // 右弧
        mix.push_back(makeLine(Vec3(30, 10, 0), Vec3(10, 10, 0), 303));                     // 上边
        mix.push_back(makeSpline({ Vec3(10, 10, 0), Vec3(0, 10, 0), Vec3(0, 0, 0) }, 304));  // 左弧
        auto r = makeSectionChangesEx(mix, SectionStyle(), false);
        check(r.filledRegions == 1, "直线 + 样条混合 → 成功生成 1 个剖面区域",
              QString::number(r.filledRegions));
    }

    // ---- 7. 带内孔的环：孔心无剖面线、壁体有剖面线 ----
    {
        SectionRegion reg;
        reg.boundary = square(0, 0, 20, 20);
        reg.holes = { square(8, 8, 12, 12) };
        reg.style.angleDeg = 0.0;
        reg.style.spacingMm = 2.0;      // 保证 y=10 是扫描线
        auto segs = generateHatchLines(reg);
        check(!segs.empty(), "带孔区域生成的剖面线非空", QString::number(segs.size()));

        bool coversHoleCenter = false, coversWall = false;
        for (const auto& s : segs) {
            if (!near2(s.a.y, 10.0, 1e-6) || !near2(s.b.y, 10.0, 1e-6)) continue;
            const double lo = std::min(s.a.x, s.b.x), hi = std::max(s.a.x, s.b.x);
            if (10.0 > lo + 1e-6 && 10.0 < hi - 1e-6) coversHoleCenter = true;   // 孔心被覆盖 = 错
            if (3.0 > lo + 1e-6 && 3.0 < hi - 1e-6) coversWall = true;           // 壁体被覆盖 = 对
        }
        check(!coversHoleCenter, "内孔中心 (10,10) 未被剖面线覆盖");
        check(coversWall, "壁体 (3,10) 处有剖面线");
        bool allIn = true;
        for (const auto& s : segs) if (!segWithinLoop(s, reg.boundary)) { allIn = false; break; }
        check(allIn, "带孔区域所有剖面线段均在边界内");
    }

    // ---- 8. 下游防御不得误拒「合法闭合多边形」（修复 regionLooksClosed 契约不一致）----
    // 背景：细长针状 / 月牙状闭合多边形只有一条主导收尾缝，其 seam 必然远大于内部最长边。
    // 若下游仍按"seam > 6×内部最长边 → 疑似开放曲线"启发式否决，就会出现
    // "上游判闭合、回报成功，下游却一条线都不画"的静默漏填（违反验收标准 #3）。
    // 修复后：generateHatchLines(受信路径) 对权威校验过的边界不再二次否决。
    {
        // 8.1 针状锯齿闭合多边形（QA 复现的最小用例）
        std::vector<Vec3> needlePts;
        needlePts.push_back(Vec3(0, 0, 0));
        for (int i = 1; i <= 8; ++i)
            needlePts.push_back(Vec3((double)i, (i % 2 == 0) ? -0.2 : 0.2, 0));
        Element needle;
        needle.type = ElementType::Polygon; needle.closed = true;
        needle.pts = needlePts; needle.id = 90;

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements({ needle }, kDefaultClosureTol, &st);
        check(loops.size() == 1, "针状闭合多边形 → 上游判为 1 个闭环", QString::number(loops.size()));

        auto r = makeSectionChangesEx({ needle }, SectionStyle(), false);
        check(r.filledRegions == 1, "针状闭合多边形 → filledRegions == 1");

        // 确为该形态：收尾缝远大于内部最长边（正是旧启发式的触发条件）
        double maxInner = 0.0;
        for (size_t i = 0; i + 1 < loops[0].size(); ++i)
            maxInner = std::max(maxInner, loops[0][i].distTo(loops[0][i + 1]));
        const double seam = loops[0].front().distTo(loops[0].back());
        check(seam > 6.0 * maxInner,
              "针状多边形：收尾缝 > 6×内部最长边（旧启发式的误拒触发条件）",
              QString("seam=%1 6×maxInner=%2").arg(seam).arg(6.0 * maxInner));

        SectionRegion reg;
        reg.boundary = loops[0];
        reg.style.angleDeg = 45.0; reg.style.spacingMm = 3.0;
        auto segs = generateHatchLines(reg);
        check(!segs.empty(), "针状闭合多边形 → 生成剖面线非空（不再静默漏填）",
              QString::number(segs.size()));
        bool allIn = true;
        for (const auto& s : segs) { QString why; if (!segWithinLoop(s, loops[0], &why)) { allIn = false; break; } }
        check(allIn, "针状多边形：所有剖面线段仍在边界内（不越界）");

        // 经 makeSectionChangesEx 产出的 Hatch 也必须能画出线
        if (!r.changes.empty()) {
            const Element h = r.changes[0].replacement;
            check(h.type == ElementType::Hatch, "针状多边形动作类型为 Hatch");
            auto hs = generateHatchLines(hatchRegionFromElement(h));
            check(!hs.empty(), "针状多边形 Hatch → 剖面线非空", QString::number(hs.size()));
        }

        // 8.2 防回归：普通细长板（60×8）同样必须正常出线
        Element plate;
        plate.type = ElementType::Polygon; plate.closed = true; plate.id = 91;
        plate.pts = { Vec3(0, 0, 0), Vec3(60, 0, 0), Vec3(60, 8, 0), Vec3(0, 8, 0) };
        auto lp = buildClosedLoopsFromElements({ plate }, kDefaultClosureTol);
        check(lp.size() == 1, "细长板(60×8) → 1 个闭环");
        SectionRegion rp;
        rp.boundary = lp.empty() ? plate.pts : lp[0];
        rp.style.angleDeg = 45.0; rp.style.spacingMm = 3.0;
        auto sp = generateHatchLines(rp);
        check(!sp.empty(), "细长板(60×8) → 剖面线非空", QString::number(sp.size()));
        bool pIn = true;
        for (const auto& s : sp) { QString why; if (!segWithinLoop(s, rp.boundary, &why)) { pIn = false; break; } }
        check(pIn, "细长板(60×8)：所有剖面线段在边界内（不越界）");
    }

    printf(g_fail == 0 ? "SectionClosureTest ALL PASS\n" : "SectionClosureTest FAILED (%d)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
