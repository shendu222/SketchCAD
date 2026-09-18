// 剖面填充「闭合性」对抗性边界回归（QA 独立验证，GB/T 4457.5-2013）
// ----------------------------------------------------------------------------
// 目的：对工程师的「真闭合判定 + 端点容差链式成环 + 弦高自适应离散」修复做证伪，
//       不重复其结论，而是从**边界/反例**角度独立施压：
//   A. 容差边界：缺口恰 0.5mm（=kDefaultClosureTol）/0.49（略小）/0.51（略大）
//      → 判定必须与 `首尾距 <= tol` 语义一致，且可重复（无边界抖动）。
//   B. 自交边界：零面积蝴蝶结（应被安全跳过）+ 非零面积自交四边形
//      → 下游 even-odd 不得产生越界线段。
//   C. 多环：一次选中两片互不相交的闭环 → 恰 2 个区域，互不干扰。
//   D. 方向不一致：外环 CW + 内孔 CCW / 同向 → 内孔仍被正确掏空。
//   E. 圆角矩形（4 弧 + 4 直线）→ 成功填充，且**所有剖面线点都落在真实圆角矩形内**
//      （这是「弦高自适应采样是否真的消除圆角溢出」的关键证据）。
//   F. 退化输入：三点共线闭合链 / 重复点 / 零面积 / 零半径 / 零扫角 → 安全跳过不崩。
//   G. 回归：带内孔环（Hatch 往返）→ 孔心无剖面线、壁体有剖面线。
//   H. 防御回归：直接把**开放**点列喂给 geo::hatchLines → 必须返回空集。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>

// 对某个「真闭合」区域调用 generateHatchLines（受信路径），断言非空且所有线段不越界。
static void checkRegionNoOverflow(const char* name, const std::vector<Vec3>& bpoly,
                                  const std::vector<std::vector<Vec3>>& holes,
                                  double angle = 45.0, double spacing = 2.0) {
    SectionRegion reg; reg.boundary = bpoly; reg.holes = holes;
    reg.style.angleDeg = angle; reg.style.spacingMm = spacing;
    auto segs = generateHatchLines(reg);
    bool allIn = !segs.empty();
    for (const auto& s : segs) { QString why; if (!segWithinLoop(s, bpoly, &why)) allIn = false; }
    check(allIn, name, QString("segs=%1").arg(segs.size()));
}

// 真实圆角矩形包含判定（对「理想几何」而非折线化结果施压）：W×H、圆角 r。
static bool pointInRoundedRect(const Vec3& p, double W, double H, double r, double eps = 1e-6) {
    if (p.x < -eps || p.x > W + eps || p.y < -eps || p.y > H + eps) return false;
    if (p.x < r && p.y < r)     return p.distTo(Vec3(r, r, 0))     <= r + eps;   // 左下角
    if (p.x > W - r && p.y < r) return p.distTo(Vec3(W - r, r, 0)) <= r + eps;   // 右下角
    if (p.x > W - r && p.y > H - r) return p.distTo(Vec3(W - r, H - r, 0)) <= r + eps;  // 右上角
    if (p.x < r && p.y > H - r) return p.distTo(Vec3(r, H - r, 0)) <= r + eps;   // 左上角
    return true;
}

// 一个 10×10 正方形、在左上角留下长度 = g 的缺口（缺口即链的收尾缝）。
static std::vector<Element> gappedSquare(double g) {
    std::vector<Element> v;
    v.push_back(makeLine(Vec3(0, 0, 0),  Vec3(10, 0, 0),  1));   // 下边
    v.push_back(makeLine(Vec3(10, 0, 0), Vec3(10, 10, 0), 2));   // 右边
    v.push_back(makeLine(Vec3(10, 10, 0), Vec3(g, 10, 0), 3));   // 上边（左端收在 g）
    v.push_back(makeLine(Vec3(0, 10, 0), Vec3(0, 0, 0),   4));   // 左边
    return v;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充闭合性对抗性边界回归（QA 独立验证）===\n");

    // =====================================================================
    // A. 容差边界：缺口恰 0.5mm / 0.49 / 0.51
    //    语义：链首尾端点距 <= kDefaultClosureTol 才算闭合。
    // =====================================================================
    {
        check(near2(kDefaultClosureTol, 0.5, 1e-12), "kDefaultClosureTol 默认 = 0.5mm",
              QString::number(kDefaultClosureTol));

        // 0.49（略小于容差）→ 闭合
        {
            auto sq = gappedSquare(0.49);
            SectionClosureStats st;
            auto loops = buildClosedLoopsFromElements(sq, kDefaultClosureTol, &st);
            check(loops.size() == 1, "缺口 0.49mm → 判为闭合（1 环）", QString::number(loops.size()));
            check(st.chainedRegions == 1, "缺口 0.49mm → 计入 1 个链式区域");
        }
        // 恰好 0.5（= 容差）→ 闭合（<= 语义）
        {
            auto sq = gappedSquare(0.5);
            SectionClosureStats st;
            auto loops = buildClosedLoopsFromElements(sq, kDefaultClosureTol, &st);
            check(loops.size() == 1, "缺口 恰 0.5mm → 判为闭合（<= 语义）", QString::number(loops.size()));
            auto r = makeSectionChangesEx(sq, SectionStyle(), false, kDefaultClosureTol);
            check(r.filledRegions == 1, "缺口 恰 0.5mm → 生成 1 个剖面区域");
        }
        // 0.51（略大于容差）→ 不闭合，安全跳过
        {
            auto sq = gappedSquare(0.51);
            SectionClosureStats st;
            auto loops = buildClosedLoopsFromElements(sq, kDefaultClosureTol, &st);
            check(loops.empty(), "缺口 0.51mm → 判为未闭合（0 环）", QString::number(loops.size()));
            check(st.openChains == 1, "缺口 0.51mm → 计入 1 处未闭合链", QString::number(st.openChains));
            auto r = makeSectionChangesEx(sq, SectionStyle(), false, kDefaultClosureTol);
            check(r.filledRegions == 0, "缺口 0.51mm → 不生成剖面区域（拒绝填充而非溢出）");
            check(r.skippedOpenChains == 1, "缺口 0.51mm → 回报 1 处跳过");
        }
        // 可重复性：连续 3 次结果一致（无边界抖动）
        {
            bool stable = true;
            size_t ref = 999;
            for (int i = 0; i < 3; ++i) {
                auto loops = buildClosedLoopsFromElements(gappedSquare(0.5), kDefaultClosureTol);
                if (ref == 999) ref = loops.size();
                else if (loops.size() != ref) stable = false;
            }
            check(stable, "容差边界判定连续 3 次一致（无随机抖动）");
        }
    }

    // =====================================================================
    // B. 自交边界
    // =====================================================================
    {
        // B1) 零面积蝴蝶结（Polygon 自身闭合）→ 安全处理，不产生越界填充
        {
            auto bow = makePolygon({ Vec3(0, 0, 0), Vec3(10, 10, 0),
                                     Vec3(10, 0, 0), Vec3(0, 10, 0) }, 40);
            SectionClosureStats st;
            auto loops = buildClosedLoopsFromElements({ bow }, 0.5, &st);
            check(loops.empty(), "零面积蝴蝶结 → 不作为可填充区域（安全跳过）",
                  QString::number(loops.size()));
            check(st.degenerate >= 1, "零面积蝴蝶结 → 计入退化", QString::number(st.degenerate));
            auto r = makeSectionChangesEx({ bow }, SectionStyle(), false);
            check(r.filledRegions == 0, "零面积蝴蝶结 → 不生成剖面区域");
        }
        // B2) 非零面积自交四边形 → 下游 even-odd 不得产生越界线段
        {
            // 边 (12,0)-(2,8) 与 (10,8)-(0,0) 相交于 (u=0.6 / v=0.4)，Shoelace 面积 = 16 ≠ 0
            std::vector<Vec3> selfX = { Vec3(0, 0, 0), Vec3(12, 0, 0),
                                        Vec3(2, 8, 0), Vec3(10, 8, 0) };
            auto segs = geo::hatchLines(selfX, {}, 0.0, 2.0, 2.0);
            check(!segs.empty(), "非零面积自交四边形 → 仍可生成剖面线", QString::number(segs.size()));
            bool inBox = true, midInside = true;
            for (const auto& s : segs) {
                for (const Vec3& p : { s.a, s.b })
                    if (p.x < -1e-6 || p.x > 12 + 1e-6 || p.y < -1e-6 || p.y > 8 + 1e-6) inBox = false;
                const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
                if (!geo::pointInPolygon(mid, selfX)) midInside = false;
            }
            check(inBox, "自交四边形：无任何剖面线端点越出包围盒");
            check(midInside, "自交四边形：所有剖面线中点满足 even-odd 在内");
        }
    }

    // =====================================================================
    // C. 多环：两片互不相交的闭环（各 4 直线链式成环）→ 恰 2 个区域
    // =====================================================================
    {
        std::vector<Element> two;
        two.push_back(makeLine(Vec3(0, 0, 0),    Vec3(10, 0, 0),  11));
        two.push_back(makeLine(Vec3(10, 0, 0),   Vec3(10, 10, 0), 12));
        two.push_back(makeLine(Vec3(10, 10, 0),  Vec3(0, 10, 0),  13));
        two.push_back(makeLine(Vec3(0, 10, 0),   Vec3(0, 0, 0),   14));
        two.push_back(makeLine(Vec3(100, 100, 0), Vec3(110, 100, 0), 15));
        two.push_back(makeLine(Vec3(110, 100, 0), Vec3(110, 110, 0), 16));
        two.push_back(makeLine(Vec3(110, 110, 0), Vec3(100, 110, 0), 17));
        two.push_back(makeLine(Vec3(100, 110, 0), Vec3(100, 100, 0), 18));

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(two, 0.5, &st);
        check(loops.size() == 2, "两片不相交闭环 → 恰 2 个区域", QString::number(loops.size()));
        check(st.chainedRegions == 2, "两片闭环均计入链式成环");

        // 每个环落在各自的 bbox、互不干扰
        int inA = 0, inB = 0;
        for (const auto& lp : loops) {
            double xmin = 1e100, ymin = 1e100, xmax = -1e100, ymax = -1e100;
            for (const auto& p : lp) {
                xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
                ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
            }
            if (xmax <= 10.001 && ymax <= 10.001) ++inA;
            if (xmin >= 99.999 && ymin >= 99.999) ++inB;
        }
        check(inA == 1 && inB == 1, "两环各自落在 [0,10] 与 [100,110]，互不干扰");

        auto r = makeSectionChangesEx(two, SectionStyle(), false);
        check(r.filledRegions == 2, "两片闭环 → 成功生成 2 个剖面区域",
              QString::number(r.filledRegions));
        int nHatch = 0;
        for (const auto& c : r.changes) if (!c.remove) ++nHatch;
        check(nHatch == 2, "两片闭环 → 2 个 Hatch 替换动作", QString::number(nHatch));
    }

    // =====================================================================
    // D. 方向不一致：外环 CW + 内孔 CCW / 同向 → 内孔仍被掏空
    // =====================================================================
    {
        // 外环顺时针、内孔逆时针
        SectionRegion reg1;
        reg1.boundary = { Vec3(0, 0, 0), Vec3(0, 20, 0), Vec3(20, 20, 0), Vec3(20, 0, 0) };  // CW
        reg1.holes = { square(8, 8, 12, 12) };                                                // CCW
        reg1.style.angleDeg = 0.0; reg1.style.spacingMm = 2.0;
        // 外环顺时针、内孔也顺时针（同向）
        SectionRegion reg2;
        reg2.boundary = { Vec3(0, 0, 0), Vec3(0, 20, 0), Vec3(20, 20, 0), Vec3(20, 0, 0) };
        reg2.holes = { { Vec3(8, 8, 0), Vec3(8, 12, 0), Vec3(12, 12, 0), Vec3(12, 8, 0) } };   // CW
        reg2.style.angleDeg = 0.0; reg2.style.spacingMm = 2.0;

        auto eval = [&](const SectionRegion& reg) -> std::pair<bool, bool> {
            auto segs = generateHatchLines(reg);
            bool coversHoleCenter = false, coversWall = false, allIn = true;
            for (const auto& s : segs) {
                if (!near2(s.a.y, 10.0, 1e-6) || !near2(s.b.y, 10.0, 1e-6)) continue;
                const double lo = std::min(s.a.x, s.b.x), hi = std::max(s.a.x, s.b.x);
                if (10.0 > lo + 1e-6 && 10.0 < hi - 1e-6) coversHoleCenter = true;
                if (2.0 > lo + 1e-6 && 2.0 < hi - 1e-6) coversWall = true;
            }
            for (const auto& s : segs) { QString why; if (!segWithinLoop(s, reg.boundary, &why)) allIn = false; }
            return { coversHoleCenter, coversWall && allIn };
        };

        auto r1 = eval(reg1);
        check(!r1.first, "外环 CW + 内孔 CCW：孔心 (10,10) 未被剖面线覆盖");
        check(r1.second, "外环 CW + 内孔 CCW：壁体有剖面线且不越界");

        auto r2 = eval(reg2);
        check(!r2.first, "外环 CW + 内孔同向 CW：孔心仍未被覆盖（even-odd 与方向无关）");
        check(r2.second, "外环 CW + 内孔同向 CW：壁体有剖面线且不越界");

        // Hatch 元素往返后仍正确掏空
        Element hEl = makeHatchElement(reg1, QStringLiteral("剖面区域"));
        SectionRegion back = hatchRegionFromElement(hEl);
        auto segsBack = generateHatchLines(back);
        bool holeCovered = false;
        for (const auto& s : segsBack) {
            if (!near2(s.a.y, 10.0, 1e-6)) continue;
            const double lo = std::min(s.a.x, s.b.x), hi = std::max(s.a.x, s.b.x);
            if (10.0 > lo + 1e-6 && 10.0 < hi - 1e-6) holeCovered = true;
        }
        check(!holeCovered, "Hatch 往返后内孔仍被掏空（孔心无剖面线）");
        check(back.holes.size() == 1, "Hatch 往返保留内孔");
    }

    // =====================================================================
    // E. 圆角矩形（4 弧 + 4 直线）→ 成功填充且圆角处无溢出
    // =====================================================================
    {
        const double W = 40.0, H = 24.0, r = 6.0;
        std::vector<Element> round;
        round.push_back(makeLine(Vec3(r, 0, 0),     Vec3(W - r, 0, 0),     51));  // 下边
        round.push_back(makeArc(Vec3(W - r, r, 0),  r, 270.0, 360.0, 52));        // 右下圆角
        round.push_back(makeLine(Vec3(W, r, 0),     Vec3(W, H - r, 0),     53));  // 右边
        round.push_back(makeArc(Vec3(W - r, H - r, 0), r, 0.0, 90.0, 54));        // 右上圆角
        round.push_back(makeLine(Vec3(W - r, H, 0), Vec3(r, H, 0),         55));  // 上边
        round.push_back(makeArc(Vec3(r, H - r, 0),  r, 90.0, 180.0, 56));         // 左上圆角
        round.push_back(makeLine(Vec3(0, H - r, 0), Vec3(0, r, 0),         57));  // 左边
        round.push_back(makeArc(Vec3(r, r, 0),      r, 180.0, 270.0, 58));        // 左下圆角

        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements(round, 0.5, &st);
        check(loops.size() == 1, "圆角矩形（4弧+4直线）→ 恰 1 个闭环", QString::number(loops.size()));

        auto rres = makeSectionChangesEx(round, SectionStyle(), false);
        check(rres.filledRegions == 1, "圆角矩形 → 成功生成 1 个剖面区域",
              QString::number(rres.filledRegions));

        // 真实落盘 → 取 Hatch 边界点列
        SketchModel model;
        std::vector<int> ids;
        for (auto& e : round) ids.push_back(model.addElement(e));
        std::vector<Element> srcs;
        for (int id : ids) if (const Element* e = model.element(id)) srcs.push_back(*e);
        auto rr = makeSectionChangesEx(srcs, SectionStyle(), false);
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, rr.changes, QString());
        int hatchCount = 0;
        for (const auto& e : model.elements()) if (e.type == ElementType::Hatch) ++hatchCount;
        check(hatchCount == 1, "圆角矩形落盘后 Hatch 数量 == 1", QString::number(hatchCount));

        const Element* h = model.element(ids[0]);
        check(h && h->type == ElementType::Hatch, "圆角矩形首图元变为 Hatch");
        if (h && h->type == ElementType::Hatch) {
            const std::vector<Vec3>& bpoly = h->pts;
            // (1) 边界折线本身必须落在真实圆角矩形内（弦高自适应未外凸）
            bool bpolyIn = true;
            for (const auto& p : bpoly) if (!pointInRoundedRect(p, W, H, r)) bpolyIn = false;
            check(bpolyIn, "圆角矩形边界折线点全部落在真实圆角矩形内（自适应未外凸）");

            auto segs = generateHatchLines(hatchRegionFromElement(*h));
            check(!segs.empty(), "圆角矩形能生成剖面线", QString::number(segs.size()));

            // (2) 硬验证：每条剖面线端点 + 中点都不得越出真实圆角矩形（圆角无溢出）
            bool allInTrue = true, allInPoly = true;
            for (const auto& s : segs) {
                const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
                for (const Vec3& p : { s.a, s.b, mid })
                    if (!pointInRoundedRect(p, W, H, r)) allInTrue = false;
                QString why; if (!segWithinLoop(s, bpoly, &why)) allInPoly = false;
            }
            check(allInTrue, "圆角矩形：所有剖面线端点/中点都在真实圆角矩形内（圆角零溢出）");
            check(allInPoly, "圆角矩形：所有剖面线段在折线边界上/内（不渗漏）");
        }
    }

    // =====================================================================
    // F. 退化输入：必须安全跳过、不崩
    // =====================================================================
    {
        // F1) 三点共线闭合链 → 零面积 → 退化跳过
        {
            auto col = makePolygon({ Vec3(0, 0, 0), Vec3(5, 0, 0), Vec3(10, 0, 0) }, 60);
            SectionClosureStats st;
            auto loops = buildClosedLoopsFromElements({ col }, 0.5, &st);
            check(loops.empty(), "三点共线闭合链 → 0 环（退化）", QString::number(loops.size()));
            check(st.degenerate >= 1, "三点共线闭合链 → 计入退化");
            check(makeSectionChanges({ col }, SectionStyle(), false).empty(),
                  "三点共线闭合链 → 空动作");
        }
        // F2) 重复点 / 零面积
        {
            auto dup = makePolygon({ Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0) }, 61);
            auto line = makePolygon({ Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 0, 0), Vec3(0, 0, 0) }, 62);
            check(buildClosedLoopsFromElements({ dup }, 0.5).empty(), "全重复点闭合链 → 0 环");
            check(buildClosedLoopsFromElements({ line }, 0.5).empty(), "退化成线段的闭合链 → 0 环");
        }
        // F3) 退化矩形 / 零半径圆 / 零扫角圆弧 → sectionOutline 拒绝，且不崩
        {
            std::vector<Vec3> out;
            Element rect; rect.type = ElementType::Rectangle; rect.pts = { Vec3(0, 0, 0), Vec3(0, 5, 0) };
            check(!sectionOutline(rect, out), "退化矩形（共线对角）→ sectionOutline false");

            Element c0; c0.type = ElementType::Circle; c0.pts = { Vec3(0, 0, 0) }; c0.radius = 0.0;
            check(!sectionOutline(c0, out), "零半径圆 → sectionOutline false");

            Element a0 = makeArc(Vec3(0, 0, 0), 5.0, 45.0, 45.0, 63);   // 零扫角
            check(!sectionOutline(a0, out), "零扫角圆弧 → sectionOutline false");
            SectionClosureStats st;
            check(buildClosedLoopsFromElements({ a0 }, 0.5, &st).empty(), "零扫角圆弧 → 0 环");
            check(st.openElements >= 1, "零扫角圆弧 → 计入孤立开放图元");
        }
        // F4) 两端点重合的零长直线 → 不崩、不产生区域
        {
            Element zl = makeLine(Vec3(1, 1, 0), Vec3(1, 1, 0), 64);
            check(buildClosedLoopsFromElements({ zl }, 0.5).empty(), "零长直线 → 0 环（不崩）");
        }
    }

    // =====================================================================
    // G. 回归：makeRingSection 生成的环形断面（带内孔 + 壁上小孔）
    // =====================================================================
    {
        // 直接用 Hatch 造一个环形（与 makeRingSection 的镂空策略一致）
        SectionRegion ring;
        ring.boundary = square(0, 0, 40, 40);
        ring.holes = { square(16, 16, 24, 24) };       // 中心孔
        ring.style.angleDeg = 0.0;
        ring.style.spacingMm = 2.0;
        auto segs = generateHatchLines(ring);
        check(!segs.empty(), "环形区域生成剖面线非空", QString::number(segs.size()));
        bool holeClear = true, wallCovered = false, inBounds = true;
        for (const auto& s : segs) {
            if (near2(s.a.y, 20.0, 1e-6) && near2(s.b.y, 20.0, 1e-6)) {
                const double lo = std::min(s.a.x, s.b.x), hi = std::max(s.a.x, s.b.x);
                if (20.0 > lo + 1e-6 && 20.0 < hi - 1e-6) holeClear = false;   // 孔覆盖 = 错
                if (5.0 > lo + 1e-6 && 5.0 < hi - 1e-6) wallCovered = true;    // 壁覆盖 = 对
            }
            QString why; if (!segWithinLoop(s, ring.boundary, &why)) inBounds = false;
        }
        check(holeClear, "环形：中心孔 (20,20) 无剖面线");
        check(wallCovered, "环形：壁体 (5,20) 有剖面线");
        check(inBounds, "环形：所有剖面线段在边界内");
    }

    // =====================================================================
    // H. 防御回归：直接把「开放」点列喂给 geo::hatchLines → 必须返回空
    // =====================================================================
    {
        // H1) 半圆（开放）点列：收尾缝 = 2R = 20mm，内部相邻边 ≈ 0.35mm
        //     → seam > 6×内部最长边 且 > 2mm → 拒绝填充
        {
            std::vector<Vec3> half;
            const int N = 100;
            for (int i = 0; i <= N; ++i) {
                const double a = M_PI * (double)i / N;      // 0..180°
                half.push_back(Vec3(10.0 * std::cos(a), 10.0 * std::sin(a), 0));
            }
            auto bad = geo::hatchLines(half, {}, 0.0, 2.0, 2.0);
            check(bad.empty(), "开放半圆点列 → hatchLines 拒绝填充（下游防御生效）",
                  QString::number(bad.size()));
        }
        // H2) 开放样条采样点列 → 同样拒绝
        {
            Element sp; sp.type = ElementType::Spline;
            sp.pts = { Vec3(0, 0, 0), Vec3(5, 2, 0), Vec3(10, -2, 0), Vec3(15, 0, 0) };
            auto openSamples = geo::sampleSpline(sp.pts, 16);
            auto bad = geo::hatchLines(openSamples, {}, 45.0, 3.0, 3.0);
            check(bad.empty(), "开放样条采样点列 → hatchLines 拒绝填充");
        }
        // H3) 反向对照：真正闭合的采样圆点列 / 方形点列 → 不得被误拒
        {
            std::vector<Vec3> circle;
            const int N = 100;
            for (int i = 0; i < N; ++i) {                    // 不重复首点
                const double a = 2.0 * M_PI * (double)i / N;
                circle.push_back(Vec3(10.0 * std::cos(a), 10.0 * std::sin(a), 0));
            }
            check(!geo::hatchLines(circle, {}, 0.0, 2.0, 2.0).empty(),
                  "闭合采样圆点列 → 不被误拒（防御不过度）");
            check(!geo::hatchLines(square(0, 0, 10, 10), {}, 0.0, 3.0, 1.0).empty(),
                  "方形点列 → 不被误拒");
        }
    }

    // =====================================================================
    // I. 跳过原因回报（SectionResult 各字段与原因分类一致）
    // =====================================================================
    {
        // 开放折线（3 点）→ 未闭合链
        {
            Element pl; pl.type = ElementType::Polyline; pl.closed = false;
            pl.pts = { Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0) }; pl.id = 70;
            auto r = makeSectionChangesEx({ pl }, SectionStyle(), false);
            check(r.filledRegions == 0 && r.skippedOpenChains == 1,
                  "开放折线 → filledRegions=0、skippedOpenChains=1");
        }
        // 非边界图元（标注）→ 孤立开放图元
        {
            Element dim; dim.type = ElementType::Dimension; dim.id = 71;
            auto r = makeSectionChangesEx({ dim }, SectionStyle(), false);
            check(r.filledRegions == 0 && r.skippedOpenElements >= 1,
                  "标注图元 → filledRegions=0、skippedOpenElements≥1");
        }
        // 混合：1 圆（成功）+ 1 开放直线（跳过）→ 部分成功
        {
            Element c; c.type = ElementType::Circle; c.pts = { Vec3(0, 0, 0) }; c.radius = 5.0; c.id = 72;
            Element l = makeLine(Vec3(50, 0, 0), Vec3(60, 0, 0), 73);
            auto r = makeSectionChangesEx({ c, l }, SectionStyle(), false);
            check(r.filledRegions == 1, "圆+开放直线 → 部分成功（1 个区域）");
            check((r.skippedOpenChains + r.skippedOpenElements) >= 1,
                  "圆+开放直线 → 回报被跳过图元");
        }
    }

    // =====================================================================
    // J. 第2轮（硬断言）：细长锯齿闭合多边形必须被修复
    //    修复前：loops=1/filledRegions=1 但 segs=0（下游 regionLooksClosed 误拒）。
    //    修复后：segs>0，且所有线段不越界（修复不得以「放宽到溢出」为代价）。
    // =====================================================================
    {
        std::vector<Vec3> sliver;
        const int n = 8; double y = 0.0;
        sliver.push_back(Vec3(0, 0, 0));
        for (int i = 1; i <= n; ++i) {
            y = (i % 2 == 0) ? -0.2 : 0.2;
            sliver.push_back(Vec3((double)i, y, 0));
        }
        auto poly = makePolygon(sliver, 80);
        SectionClosureStats st;
        auto loops = buildClosedLoopsFromElements({ poly }, 0.5, &st);
        check(loops.size() == 1, "细长锯齿闭合板 → 判为 1 个闭环", QString::number(loops.size()));

        auto r = makeSectionChangesEx({ poly }, SectionStyle(), false);
        check(r.filledRegions == 1, "细长锯齿闭合板 → filledRegions==1");

        const std::vector<Vec3>& bl = loops.empty() ? sliver : loops[0];
        SectionRegion reg; reg.boundary = bl;
        reg.style.angleDeg = 45.0; reg.style.spacingMm = 3.0;
        auto segs = generateHatchLines(reg);
        check(!segs.empty(), "细长锯齿闭合板 → 修复后确实出线（segs>0）", QString::number(segs.size()));
        bool allIn = true;
        for (const auto& s : segs) { QString why; if (!segWithinLoop(s, bl, &why)) allIn = false; }
        check(allIn, "细长锯齿闭合板 → 所有剖面线段端点/中点不越界");

        // 对照（防回归）：60×8 细长板
        std::vector<Vec3> plate = { Vec3(0,0,0), Vec3(60,0,0), Vec3(60,8,0), Vec3(0,8,0) };
        auto rp = makeSectionChangesEx({ makePolygon(plate, 81) }, SectionStyle(), false);
        check(rp.filledRegions == 1, "对照 60×8 细长板 → filledRegions==1");
        checkRegionNoOverflow("对照 60×8 细长板 → 出线且不越界", plate, {}, 45.0, 3.0);
    }

    // =====================================================================
    // K. 第2轮核心：trustedClosed 放行后「防御不得被削弱」（重新对全部形状施压）
    // =====================================================================
    {
        // K1) 同一条开放半圆点列：未受信(5参) → 仍拒绝；受信(6参 trusted=true) → 放行。
        //     既 pin 住 API 语义，也证明后续 H 段确实走的是**未受信**路径。
        {
            std::vector<Vec3> half;
            const int N = 100;
            for (int i = 0; i <= N; ++i) {
                const double a = M_PI * (double)i / N;
                half.push_back(Vec3(10.0 * std::cos(a), 10.0 * std::sin(a), 0));
            }
            auto untrusted = geo::hatchLines(half, {}, 0.0, 2.0, 2.0);          // 5 参
            auto trusted   = geo::hatchLines(half, {}, 0.0, 2.0, 2.0, true);    // 6 参
            check(untrusted.empty(), "未受信(5参)开放半圆 → 仍拒绝填充（防御未被削弱）",
                  QString::number(untrusted.size()));
            check(!trusted.empty(), "受信(6参)开放半圆 → 放行（API 语义对照）",
                  QString::number(trusted.size()));
        }

        // K2) 一批「真闭合」边界经 generateHatchLines（受信）→ 一律非空且不越界
        checkRegionNoOverflow("受信:方形 不越界", square(0, 0, 10, 10), {});
        {   // 采样圆
            std::vector<Vec3> c; const int N = 120;
            for (int i = 0; i < N; ++i) { const double a = 2.0*M_PI*i/N;
                c.push_back(Vec3(10.0*std::cos(a), 10.0*std::sin(a), 0)); }
            checkRegionNoOverflow("受信:采样圆 不越界", c, {});
        }
        checkRegionNoOverflow("受信:环形(带孔) 不越界", square(0,0,20,20), { square(8,8,12,12) });

        // 圆角矩形（4弧+4直线）的**真实闭合闭环** → 受信路径不越界
        {
            const double W = 40.0, H = 24.0, rr = 6.0;
            std::vector<Element> round;
            round.push_back(makeLine(Vec3(rr, 0, 0), Vec3(W - rr, 0, 0), 51));
            round.push_back(makeArc(Vec3(W - rr, rr, 0), rr, 270.0, 360.0, 52));
            round.push_back(makeLine(Vec3(W, rr, 0), Vec3(W, H - rr, 0), 53));
            round.push_back(makeArc(Vec3(W - rr, H - rr, 0), rr, 0.0, 90.0, 54));
            round.push_back(makeLine(Vec3(W - rr, H, 0), Vec3(rr, H, 0), 55));
            round.push_back(makeArc(Vec3(rr, H - rr, 0), rr, 90.0, 180.0, 56));
            round.push_back(makeLine(Vec3(0, H - rr, 0), Vec3(0, rr, 0), 57));
            round.push_back(makeArc(Vec3(rr, rr, 0), rr, 180.0, 270.0, 58));
            auto loops = buildClosedLoopsFromElements(round, 0.5);
            check(loops.size() == 1, "受信扫描:圆角矩形 → 1 闭环", QString::number(loops.size()));
            if (!loops.empty()) checkRegionNoOverflow("受信:圆角矩形 不越界", loops[0], {}, 45.0, 2.5);
        }

        // 两片不相交链式闭环 → 每个环受信不越界
        {
            std::vector<Element> two;
            two.push_back(makeLine(Vec3(0,0,0), Vec3(10,0,0), 11));
            two.push_back(makeLine(Vec3(10,0,0), Vec3(10,10,0), 12));
            two.push_back(makeLine(Vec3(10,10,0), Vec3(0,10,0), 13));
            two.push_back(makeLine(Vec3(0,10,0), Vec3(0,0,0), 14));
            two.push_back(makeLine(Vec3(100,100,0), Vec3(110,100,0), 15));
            two.push_back(makeLine(Vec3(110,100,0), Vec3(110,110,0), 16));
            two.push_back(makeLine(Vec3(110,110,0), Vec3(100,110,0), 17));
            two.push_back(makeLine(Vec3(100,110,0), Vec3(100,100,0), 18));
            auto loops = buildClosedLoopsFromElements(two, 0.5);
            check(loops.size() == 2, "受信扫描:两片链式闭环 → 2 环", QString::number(loops.size()));
            for (size_t i = 0; i < loops.size(); ++i)
                checkRegionNoOverflow("受信:链式闭环 不越界", loops[i], {}, 45.0, 2.0);
        }
        // 针状锯齿（最极端：单一主导收尾缝）→ 受信不越界
        {
            std::vector<Vec3> sl; sl.push_back(Vec3(0,0,0));
            const int n = 8;
            for (int i = 1; i <= n; ++i) sl.push_back(Vec3((double)i, (i%2==0)?-0.2:0.2, 0));
            checkRegionNoOverflow("受信:针状锯齿板 不越界", sl, {}, 45.0, 3.0);
        }

        // K3) 信息性对照（不计失败）：把**开放**边界直接做成 SectionRegion 经受信路径，
        //     说明受信层已不再自我防御——该风险仅当外部文件注入畸形 Hatch 边界时可能存在。
        {
            std::vector<Vec3> half;
            const int N = 100;
            for (int i = 0; i <= N; ++i) {
                const double a = M_PI * (double)i / N;
                half.push_back(Vec3(10.0 * std::cos(a), 10.0 * std::sin(a), 0));
            }
            SectionRegion reg; reg.boundary = half;
            reg.style.angleDeg = 0.0; reg.style.spacingMm = 2.0;
            auto segs = generateHatchLines(reg);
            int offBoundary = 0;
            for (const auto& s : segs) if (distToPolygon(s.a, half) > 1e-6) ++offBoundary;
            printf("  [[INFO]] 受信路径对开放边界: segs=%zu（其中端点离边界>1e-6 的 %d 个）"
                   "—— 仅外部文件注入畸形 Hatch 时可达\n", segs.size(), offBoundary);
        }
    }

    printf(g_fail == 0 ? "SectionClosureEdgeTest ALL PASS (%d checks)\n"
                       : "SectionClosureEdgeTest FAILED (%d) [pass=%d]\n", g_fail == 0 ? g_pass : g_fail,
           g_pass);
    return g_fail == 0 ? 0 : 1;
}
