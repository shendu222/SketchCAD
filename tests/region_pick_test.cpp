// 剖面填充「区域点选」回归测试（GB/T 4457.5-2013）
// ----------------------------------------------------------------------------
// 交互契约：点击【修改>剖面填充】进入区域选择模式，鼠标点在哪个封闭区域内，
// 就只填充那个区域。本测试锁定以下不变量：
//   1) 命中 = 包含该点的「最内层」闭环 —— 相邻区域不会被连带填充（A 段）；
//   2) 多图元（直线 / 圆弧 / 开放折线 / 样条）首尾相接围成的闭环可被点选（B / H 段）；
//   3) 嵌套闭环自动成为内孔（even-odd 掏空），且"点外环内部 = 环形区域、
//      点内环内部 = 内环本身"两者互不混淆（C / E 段）；
//   4) 点在封闭区域之外 / 开放轮廓处 → 安全跳过并给出原因（D / F 段）；
//   5) 已填充区域不重复填充（G 段）；
//   6) 生成的所有剖面线段端点/中点都落在区域内、且避开内孔（不渗漏 / 不溢出 / 不越孔）；
//   7) 极窄区域的"整块空白"由自动加密兜底（I 段）。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>

// 4 条直线首尾相接围成的方框（多图元链式成环）
static std::vector<Element> squareLines(double x0, double y0, double x1, double y1, int id0) {
    return { makeLine(Vec3(x0, y0, 0), Vec3(x1, y0, 0), id0 + 0),
             makeLine(Vec3(x1, y0, 0), Vec3(x1, y1, 0), id0 + 1),
             makeLine(Vec3(x1, y1, 0), Vec3(x0, y1, 0), id0 + 2),
             makeLine(Vec3(x0, y1, 0), Vec3(x0, y0, 0), id0 + 3) };
}

// （sameIdSet / idsToStr / findHatch / distToPolygon 已上移到 section_test_helpers.h）
// 硬验证：剖面线段完全落在区域内（不渗漏 / 不溢出），且不在内孔中。
static bool segInsideRegion(const geo::Seg& s, const SectionRegion& r, QString* why = nullptr) {
    const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
    if (!geo::pointInPolygon(mid, r.boundary)) { if (why) *why = QStringLiteral("midpoint outside boundary"); return false; }
    for (const auto& h : r.holes)
        if (geo::pointInPolygon(mid, h)) { if (why) *why = QStringLiteral("midpoint inside hole"); return false; }
    for (const Vec3& p : { s.a, s.b }) {
        if (!geo::pointInPolygon(p, r.boundary) && distToPolygon(p, r.boundary) > 1e-6) {
            if (why) *why = QStringLiteral("endpoint off boundary");
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充区域点选回归（GB/T 4457.5-2013）===\n");

    // ================= A. 相邻区域隔离：点哪填哪（核心诉求） =================
    {
        printf("\n-- A. 相邻区域隔离 --\n");
        std::vector<Element> elems = { makeRect(0, 0, 10, 10, 101),     // 左
                                       makeRect(10, 0, 20, 10, 102) };  // 右（共享边 x=10）
        {
            const auto p = pickSectionRegionAt(elems, Vec3(5, 5, 0));
            check(p.ok(), "点左矩形内部 → 命中区域");
            check(sameIdSet(p.boundarySrcIds, { 101 }), "只命中左矩形（不牵连右矩形）",
                  idsToStr(p.boundarySrcIds));
            check(p.holes.empty(), "无内孔");
        }
        {
            const auto p = pickSectionRegionAt(elems, Vec3(15, 5, 0));
            check(sameIdSet(p.boundarySrcIds, { 102 }), "点右矩形内部 → 只命中右矩形",
                  idsToStr(p.boundarySrcIds));
        }
        {
            const auto p = pickSectionRegionAt(elems, Vec3(25, 5, 0));
            check(!p.found && p.reason == RegionPickReason::NoLoopAtPoint,
                  "点在所有区域之外 → 安全跳过（NoLoopAtPoint）");
            check(p.totalLoops == 2, "全图闭环数 = 2", QString::number(p.totalLoops));
        }
        // 落盘后：只有左矩形变成 Hatch，右矩形必须是原样的 Rectangle（不填到相邻区域）
        {
            SketchModel model;
            const int lid = model.addElement(elems[0]);
            const int rid = model.addElement(elems[1]);
            std::vector<Element> srcs = model.elements();
            const auto p = pickSectionRegionAt(srcs, Vec3(5, 5, 0));
            const auto res = makeSectionChangeForPick(p, srcs, SectionStyle());
            check(res.filledRegions == 1, "产生 1 个填充动作");
            check(res.changes.size() == 1, "单图元区域 → 只有 1 个 update 动作",
                  QString::number(res.changes.size()));
            model.beginEdit(QStringLiteral("剖面填充"));
            modify::applyChanges(&model, res.changes, QString());

            const Element* a = model.element(lid);
            const Element* b = model.element(rid);
            check(a && a->type == ElementType::Hatch, "左矩形 → 已变为 Hatch");
            check(b && b->type == ElementType::Rectangle, "右矩形 → 未被改动（仍是矩形）");
            check(model.elements().size() == 2, "图元总数不变",
                  QString::number(model.elements().size()));
        }
    }

    // ================= B. 多图元链式成环（4 条直线） =================
    {
        printf("\n-- B. 链式成环 --\n");
        std::vector<Element> elems = squareLines(0, 0, 20, 10, 201);
        const auto p = pickSectionRegionAt(elems, Vec3(10, 5, 0));
        check(p.ok(), "4 条直线围成的方框 → 命中区域");
        check(sameIdSet(p.boundarySrcIds, { 201, 202, 203, 204 }),
              "外边界由 4 个图元链式成环", idsToStr(p.boundarySrcIds));
        check(std::fabs(p.area - 200.0) < 0.2, "净面积 ≈ 200", QString::number(p.area));

        const auto res = makeSectionChangeForPick(p, elems, SectionStyle());
        check(res.filledRegions == 1, "链式成环 → 1 个区域");
        check(res.chainedRegions == 1, "标记为链式成环区域");
        // 1 个 update（宿主）+ 3 个 remove（并入）
        int upd = 0, rem = 0;
        for (const auto& c : res.changes) { if (c.remove) ++rem; else ++upd; }
        check(upd == 1 && rem == 3, "1 update + 3 remove（避免残留重复轮廓）",
              QStringLiteral("%1/%2").arg(upd).arg(rem));
    }

    // ================= C. 嵌套闭环 → 内孔（环形区域） =================
    {
        printf("\n-- C. 嵌套 → 内孔 --\n");
        std::vector<Element> elems = squareLines(0, 0, 40, 40, 401);
        elems.push_back(makeCircle(Vec3(20, 20, 0), 6.0, 405));

        const auto ring = pickSectionRegionAt(elems, Vec3(20, 11, 0));   // 方框内、圆外
        check(ring.ok(), "点「环形」区域 → 命中");
        check(sameIdSet(ring.boundarySrcIds, { 401, 402, 403, 404 }), "外边界 = 方框 4 条直线");
        check(ring.holes.size() == 1, "嵌套的圆 → 1 个内孔",
              QString::number(ring.holes.size()));
        check(sameIdSet(ring.holeSrcIds, { 405 }), "内孔源图元 = 圆");
        check(std::fabs(ring.area - (1600.0 - 3.14159265 * 36.0)) < 2.0,
              "净面积 = 方框 − 圆", QString::number(ring.area));

        const auto disc = pickSectionRegionAt(elems, Vec3(20, 20, 0));   // 圆内
        check(disc.ok(), "点内环内部 → 命中");
        check(sameIdSet(disc.boundarySrcIds, { 405 }), "内环内部 → 边界就是圆本身（不误判为环形区域）",
              idsToStr(disc.boundarySrcIds));
        check(disc.holes.empty(), "内环内部无内孔");

        // 落盘 + 硬验证：剖面线在环内、且不进入内孔
        // 注意：SketchModel::addElement 会重新分配图元 id，故拾取必须基于模型里
        // 的实际图元（含真实 id），否则 Change 会指向不存在的图元（静默空更新）。
        SketchModel model;
        for (const auto& e : elems) model.addElement(e);
        const std::vector<Element> srcs = model.elements();
        const auto rePick = pickSectionRegionAt(srcs, Vec3(20, 11, 0));
        int circleId = -1;
        for (const auto& e : srcs) if (e.type == ElementType::Circle) circleId = e.id;
        check(sameIdSet(rePick.holeSrcIds, { circleId }), "内孔源图元 = 模型中的圆",
              idsToStr(rePick.holeSrcIds));
        const auto res = makeSectionChangeForPick(rePick, srcs, SectionStyle());
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res.changes, QString());
        check(model.elements().size() == 1, "方框 + 圆 → 合并为 1 个 Hatch",
              QString::number(model.elements().size()));

        const Element* h = findHatch(model);
        check(h && h->type == ElementType::Hatch, "落盘为 Hatch");
        if (h) {
            check(h->holes.size() == 1, "Hatch 携带 1 个内孔（往返保留）",
                  QString::number(h->holes.size()));
            const SectionRegion back = hatchRegionFromElement(*h);
            const auto segs = generateHatchLines(back);
            check(!segs.empty(), "环形断面生成剖面线段", QString::number(segs.size()));
            bool allIn = true; QString why;
            for (const auto& s : segs) if (!segInsideRegion(s, back, &why)) { allIn = false; break; }
            check(allIn, "全部剖面线段在环内且避开内孔（不渗漏/不越孔）", why);
        }
    }

    // ================= D. 多个内孔 + 最内层优先 =================
    {
        printf("\n-- D. 多内孔 / 最内层优先 --\n");
        std::vector<Element> elems = squareLines(0, 0, 40, 40, 501);
        elems.push_back(makeRect(5, 5, 15, 15, 505));      // 内孔 1
        elems.push_back(makeRect(25, 25, 35, 35, 506));    // 内孔 2
        const auto p = pickSectionRegionAt(elems, Vec3(20, 20, 0));
        check(p.ok() && p.holes.size() == 2, "命中区域含 2 个内孔",
              QString::number(p.holes.size()));
        check(sameIdSet(p.holeSrcIds, { 505, 506 }), "内孔源图元正确", idsToStr(p.holeSrcIds));

        const auto inHole = pickSectionRegionAt(elems, Vec3(10, 10, 0));
        check(sameIdSet(inHole.boundarySrcIds, { 505 }), "点在内孔内部 → 边界是内孔本身（最内层优先）",
              idsToStr(inHole.boundarySrcIds));
        check(inHole.holes.empty(), "内孔内部无更深内孔");
    }

    // ================= E. 容差边界（链的缺口） =================
    {
        printf("\n-- E. 容差边界 --\n");
        // 缺口 0.4mm（< 0.5 容差）→ 视为相连
        std::vector<Element> tight = {
            makeLine(Vec3(0, 0, 0), Vec3(10, 0, 0), 601),
            makeLine(Vec3(10, 0, 0), Vec3(10, 10, 0), 602),
            makeLine(Vec3(10, 10, 0), Vec3(0, 10, 0), 603),
            makeLine(Vec3(0, 10, 0), Vec3(0, 0.4, 0), 604) };   // 收尾留 0.4mm 缝
        const auto a = pickSectionRegionAt(tight, Vec3(5, 5, 0));
        check(a.ok(), "缺口 0.4mm（≤ 容差 0.5）→ 仍判定为封闭区域");

        // 缺口 1.5mm（> 容差）→ 不构成封闭区域
        std::vector<Element> loose = {
            makeLine(Vec3(0, 0, 0), Vec3(10, 0, 0), 611),
            makeLine(Vec3(10, 0, 0), Vec3(10, 10, 0), 612),
            makeLine(Vec3(10, 10, 0), Vec3(0, 10, 0), 613),
            makeLine(Vec3(0, 10, 0), Vec3(0, 1.5, 0), 614) };
        const auto b = pickSectionRegionAt(loose, Vec3(5, 5, 0));
        check(!b.found && b.reason == RegionPickReason::NoLoopAtPoint,
              "缺口 1.5mm（> 容差）→ 安全跳过，不填充");
    }

    // ================= F. 开放轮廓 =================
    {
        printf("\n-- F. 开放轮廓 --\n");
        std::vector<Element> elems = {
            makeOpenPolyline({ Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0) }, 701) };
        const auto p = pickSectionRegionAt(elems, Vec3(8, 3, 0));
        check(!p.found && p.totalLoops == 0, "开放折线 → 无任何闭环，安全跳过");
        check(makeSectionChangeForPick(p, elems, SectionStyle()).filledRegions == 0,
              "开放式拾取结果 → 不产生填充动作");
    }

    // ================= G. 已填充区域不重复填充 =================
    {
        printf("\n-- G. 已填充检测 --\n");
        SketchModel model;
        const int rid = model.addElement(makeRect(0, 0, 20, 20));
        const auto p1 = pickSectionRegionAt(model.elements(), Vec3(10, 10, 0));
        const auto res = makeSectionChangeForPick(p1, model.elements(), SectionStyle());
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res.changes, QString());

        const auto p2 = pickSectionRegionAt(model.elements(), Vec3(10, 10, 0));
        check(!p2.found && p2.reason == RegionPickReason::AlreadyFilled,
              "再次点击同一区域 → AlreadyFilled（不重复填充）");
        check(makeSectionChangeForPick(p2, model.elements(), SectionStyle()).filledRegions == 0,
              "已填充区域 → 不产生任何动作");
        const Element* e = model.element(rid);
        check(e && e->type == ElementType::Hatch, "原图元仍是那一个 Hatch（未叠加）");
        check(model.elements().size() == 1, "图元总数仍为 1");
    }

    // ================= H. 圆弧参与的链式成环（跑道形） =================
    {
        printf("\n-- H. 圆弧 + 直线成环 --\n");
        std::vector<Element> elems = {
            makeArc(Vec3(10, 0, 0), 5.0, -90.0, 90.0, 801),    // 右半圆
            makeArc(Vec3(-10, 0, 0), 5.0, 90.0, 270.0, 802),   // 左半圆
            makeLine(Vec3(-10, 5, 0), Vec3(10, 5, 0), 803),    // 上直线
            makeLine(Vec3(10, -5, 0), Vec3(-10, -5, 0), 804) };// 下直线
        const auto p = pickSectionRegionAt(elems, Vec3(0, 0, 0));
        check(p.ok(), "两弧 + 两直线（跑道形）→ 命中区域");
        check(sameIdSet(p.boundarySrcIds, { 801, 802, 803, 804 }), "外边界为 4 个图元");

        // 落盘：同样基于模型分配后的真实 id
        SketchModel model;
        for (const auto& e : elems) model.addElement(e);
        const std::vector<Element> srcs = model.elements();
        const auto rePick = pickSectionRegionAt(srcs, Vec3(0, 0, 0));
        const auto res = makeSectionChangeForPick(rePick, srcs, SectionStyle());
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res.changes, QString());
        const Element* h = findHatch(model);
        check(h && h->type == ElementType::Hatch, "跑道形 → 落盘为 Hatch");
        check(model.elements().size() == 1, "跑道形 4 图元 → 合并为 1 个 Hatch",
              QString::number(model.elements().size()));
        if (h) {
            const SectionRegion back = hatchRegionFromElement(*h);
            const auto segs = generateHatchLines(back);
            check(!segs.empty(), "跑道形生成剖面线段", QString::number(segs.size()));
            bool allIn = true; QString why;
            for (const auto& s : segs) if (!segInsideRegion(s, back, &why)) { allIn = false; break; }
            check(allIn, "圆弧段区域零溢出（自适应采样生效）", why);
        }
    }

    // ================= I. 极窄区域：自动加密兜底（不出现整块空白） =================
    {
        printf("\n-- I. 极窄区域自动加密 --\n");
        const double thks[] = { 4.0, 2.0, 1.2, 0.8, 0.5 };
        for (double t : thks) {
            std::vector<Element> elems = { makeRect(0, 0, 60, t, 901) };
            SectionStyle st;
            st.angleDeg    = 90.0;    // 水平剖面线：区域厚度即法向跨度（故意远小于 3mm 间距）
            st.spacingMm   = 3.0;
            const auto p = pickSectionRegionAt(elems, Vec3(30, t * 0.5, 0));
            const auto res = makeSectionChangeForPick(p, elems, st);
            bool nonEmpty = false;
            double usedSpacing = 0.0;
            if (res.filledRegions == 1 && !res.changes.empty()) {
                const Element& h = res.changes.front().replacement;
                usedSpacing = h.radius;
                nonEmpty = !generateHatchLines(hatchRegionFromElement(h)).empty();
            }
            check(nonEmpty, qPrintable(QStringLiteral("厚 %1mm 的窄条 → 自动加密后仍有剖面线")
                                           .arg(t, 0, 'f', 1)),
                  QStringLiteral("spacing=%1").arg(usedSpacing, 0, 'f', 2));
            check(usedSpacing <= st.spacingMm + 1e-9, "生效间距不超过用户设定（只加密不放松）");
        }
    }

    // ================= J. 拾取与"按图元选"两条路径互不干扰 =================
    {
        printf("\n-- J. 与既有接口兼容 --\n");
        std::vector<Element> elems = { makeRect(0, 0, 10, 10, 1001), makeRect(10, 0, 20, 10, 1002) };
        // 既有接口（选区 → 填充）语义不变：仍会把两个矩形各填一块
        const auto legacy = makeSectionChangesEx(elems, SectionStyle(), false);
        check(legacy.filledRegions == 2, "makeSectionChangesEx 仍是「选几个就填几个」（2 个区域）",
              QString::number(legacy.filledRegions));
        // 点选接口只作用于命中的那一个
        const auto picked = makeSectionChangeForPick(pickSectionRegionAt(elems, Vec3(5, 5, 0)),
                                                     elems, SectionStyle());
        check(picked.filledRegions == 1, "点选接口只填命中的 1 个区域",
              QString::number(picked.filledRegions));
    }

    printf("\n=== 汇总：PASS=%d FAIL=%d ===\n", g_pass, g_fail);
    if (g_fail == 0) { printf("RESULT: PASS\n"); return 0; }
    printf("RESULT: FAIL\n");
    return 1;
}
