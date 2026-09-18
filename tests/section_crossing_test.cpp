// 剖面填充「边界交叉细分」回归测试（GB/T 4457.5-2013）
// ----------------------------------------------------------------------------
// 锁定两类修复诉求（根因：成环只按端点相接，不感知边界线之间的交叉点）：
//   图1：四边形的边在内部相互交叉(端点并不相接)，且内部有一圆 ——
//        修复前只有圆能被点选，圆外区域拾取失败；修复后圆外子区域可正常拾取，
//        且圆自动成为所点子区域的内孔。
//   图2：自闭合矩形被一条贯穿竖线(端点超出矩形上下边)分隔 ——
//        修复前只能整体填充，无法只填竖线某一侧；修复后左右两侧各自成环、
//        点哪侧填哪侧、互不吞并、不渗漏。
// 另含回归保护：内部未触及边界的直线不应误分区域(仅真正交叉/触碰边界者才分隔)。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>

// 点是否在多段折线(隐式闭合)列出的所有顶点 x 坐标都 <= xMax
static bool allXLe(const std::vector<Vec3>& poly, double xMax) {
    for (const auto& p : poly) if (p.x > xMax + 1e-6) return false;
    return true;
}
static bool allXGe(const std::vector<Vec3>& poly, double xMin) {
    for (const auto& p : poly) if (p.x < xMin - 1e-6) return false;
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充边界交叉细分回归 ===\n");

    // ================= 图1：四边形边内部交叉 + 内部圆 =================
    // 边界由 4 条独立直线构成，底边是一条"斜穿"线：它在内部(非端点)分别穿过左边与
    // 右边，因此端点链无法闭合四边形(修复前只有圆可选)。圆位于四边形内部。
    {
        printf("\n-- 图1. 四边形边内部交叉 + 内部圆 --\n");
        Vec3 A(0, 0, 0), B(100, 0, 0);
        std::vector<Element> elems = {
            makeLine(A, B, 1),                                  // 上边
            makeLine(A, Vec3(0, 100, 0), 2),                    // 左边
            makeLine(B, Vec3(100, 100, 0), 3),                   // 右边
            makeLine(Vec3(-20, 100, 0), Vec3(120, 60, 0), 4)     // 底边斜穿：内部过 (0,94.3) 与 (100,65.7)
        };
        elems.push_back(makeCircle(Vec3(50, 50, 0), 20.0, 6)); // 内部圆

        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.size() >= 2, "细分后能解析出 ≥2 个闭环(四边形面 + 圆)",
              QString::number(set.loops.size()));

        // 点圆内部 → 命中圆本身
        const auto inCircle = pickSectionRegionAt(elems, Vec3(50, 50, 0));
        check(inCircle.ok(), "点圆内部 → 命中(圆本身可选，符合预期)");
        check(sameIdSet(inCircle.boundarySrcIds, { 6 }), "圆内部边界 = 圆", idsToStr(inCircle.boundarySrcIds));

        // 点四边形内部、圆外 → 必须能命中(修复前此处失败)
        const auto inQuad = pickSectionRegionAt(elems, Vec3(50, 15, 0));
        check(inQuad.ok(), "点四边形内部/圆外 → 命中区域(修复前此步失败)",
              inQuad.reason == RegionPickReason::NoLoopAtPoint ? "NoLoopAtPoint" : "");
        check(inQuad.area > 6700.0, "圆外区域净面积 ≈ 四边形面(8000) − 圆孔(≈1257) ≈ 6743",
              QString::number(inQuad.area));
        check(std::find(inQuad.holeSrcIds.begin(), inQuad.holeSrcIds.end(), 6) != inQuad.holeSrcIds.end(),
              "圆成为所点子区域的内孔(even-odd 掏空)", idsToStr(inQuad.holeSrcIds));

        // 落盘硬验证：圆外子区域生成的 Hatch 不进入圆内
        SketchModel model;
        for (const auto& e : elems) model.addElement(e);
        const std::vector<Element> srcs = model.elements();
        const auto rePick = pickSectionRegionAt(srcs, Vec3(50, 15, 0));
        const auto res = makeSectionChangeForPick(rePick, srcs, SectionStyle());
        check(res.filledRegions == 1, "圆外子区域 → 产生 1 个填充动作");
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res.changes, QString());
        const Element* h = nullptr;
        for (const auto& e : model.elements()) if (e.type == ElementType::Hatch) h = &e;
        check(h && h->type == ElementType::Hatch, "圆外子区域 → 落盘为 Hatch");
        if (h) {
            const SectionRegion back = hatchRegionFromElement(*h);
            const auto segs = generateHatchLines(back);
            check(!segs.empty(), "圆外子区域生成剖面线段", QString::number(segs.size()));
            bool ok = true; QString why;
            const Vec3 c(50, 50, 0);
            for (const auto& s : segs) {
                const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
                // 圆内(中心 (50,50) r20)不应有剖面线中点
                const double dmid = std::hypot(mid.x - c.x, mid.y - c.y);
                if (dmid < 20.0 - 1e-6) { ok = false; why = QStringLiteral("segment inside circle hole"); break; }
            }
            check(ok, "全部剖面线段避开圆孔(不渗漏进圆内)", why);
        }
    }

    // ================= 图2：矩形被贯穿竖线分隔 =================
    {
        printf("\n-- 图2. 矩形 + 贯穿竖线(两侧独立填充) --\n");
        std::vector<Element> elems = {
            makeRect(0, 0, 100, 60, 11),                 // 矩形
            makeLine(Vec3(50, -20, 0), Vec3(50, 80, 0), 12) // 竖线穿过上下边(端点超出)
        };
        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.size() == 2, "竖线把矩形切成 2 个闭环(左右各一)",
              QString::number(set.loops.size()));

        // 点左侧
        const auto left = pickSectionRegionAt(elems, Vec3(25, 30, 0));
        check(left.ok(), "点左侧区域 → 命中");
        check(left.area > 2900.0 && left.area < 3100.0, "左侧面积 ≈ 50×60=3000",
              QString::number(left.area));
        check(allXLe(left.boundary, 50.0), "左侧 Hatch 边界全部 x≤50(不越过竖线)",
              QString::number(left.boundary.empty() ? -1 : left.boundary.front().x));

        // 点右侧
        const auto right = pickSectionRegionAt(elems, Vec3(75, 30, 0));
        check(right.ok(), "点右侧区域 → 命中");
        check(allXGe(right.boundary, 50.0), "右侧 Hatch 边界全部 x≥50(不越过竖线)");
        check(right.area > 2900.0 && right.area < 3100.0, "右侧面积 ≈ 3000",
              QString::number(right.area));

        // 两侧互不吞并：左侧边界点不在右侧
        bool leftHasRightSide = false;
        for (const auto& p : left.boundary) if (p.x > 50.0 + 1e-6) leftHasRightSide = true;
        check(!leftHasRightSide, "点左侧不会把右侧也填进来(相邻区域隔离)");

        // 落盘：单侧 Hatch 几何严格落在竖线一侧
        SketchModel model;
        for (const auto& e : elems) model.addElement(e);
        const std::vector<Element> srcs = model.elements();
        const auto rePick = pickSectionRegionAt(srcs, Vec3(25, 30, 0));
        const auto res = makeSectionChangeForPick(rePick, srcs, SectionStyle());
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, res.changes, QString());
        const Element* h = nullptr;
        for (const auto& e : model.elements()) if (e.type == ElementType::Hatch) h = &e;
        check(h && h->type == ElementType::Hatch, "左侧 → 落盘为单个 Hatch");
        if (h) {
            check(allXLe(h->pts, 50.0), "落盘 Hatch 仅覆盖左侧(竖线为界，不渗漏到右侧)");
            const SectionRegion back = hatchRegionFromElement(*h);
            const auto segs = generateHatchLines(back);
            check(!segs.empty(), "左侧生成剖面线段", QString::number(segs.size()));
        }

        // === 本次修复锁定：填充左侧不得"裁掉"右侧（共享边必须保留）===
        // 注：model.addElement() 会重新分配 ID，故按图元类型判定而非硬编码 id。
        {
            const std::vector<Element> after = model.elements();
            int nRect = 0, nLine = 0, nHatch = 0;
            for (const auto& e : after) {
                if (e.type == ElementType::Rectangle)   ++nRect;
                else if (e.type == ElementType::Line)   ++nLine;
                else if (e.type == ElementType::Hatch)  ++nHatch;
            }
            check(nRect == 1 && nLine == 1 && nHatch == 1,
                  "填充左侧后，矩形与竖线仍保留、仅新增 1 个 Hatch(右侧未被一并裁掉)",
                  QStringLiteral("rect=%1 line=%2 hatch=%3").arg(nRect).arg(nLine).arg(nHatch));

            bool anyRemove = false;
            for (const auto& c : res.changes) if (c.remove) anyRemove = true;
            check(!anyRemove && res.changes.size() == 1,
                  "填充动作仅 1 个新增 Hatch(无删除/无替换共享边)",
                  QString::number(res.changes.size()));

            const auto rightAfter = pickSectionRegionAt(after, Vec3(75, 30, 0));
            check(rightAfter.ok() && rightAfter.area > 2900.0 && rightAfter.area < 3100.0,
                  "填充左侧后，右侧区域仍可完整拾取(面积≈3000)",
                  QString::number(rightAfter.area));
            const auto resR = makeSectionChangeForPick(rightAfter, after, SectionStyle());
            check(resR.filledRegions == 1, "填充左侧后，右侧仍可独立生成填充");
        }
    }

    // ================= 回归保护：内部未触及边界的直线不应误分 =================
    {
        printf("\n-- 回归：内部短直线不误分矩形 --\n");
        std::vector<Element> elems = {
            makeRect(0, 0, 100, 60, 21),
            makeLine(Vec3(10, 30, 0), Vec3(90, 30, 0), 22)  // 两端均在矩形内部，不触碰边界
        };
        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.size() == 1, "内部短直线不触发细分 → 矩形仍是 1 个闭环",
              QString::number(set.loops.size()));
        const auto p = pickSectionRegionAt(elems, Vec3(50, 45, 0));
        check(p.ok() && std::fabs(p.area - 6000.0) < 50.0, "点矩形内部 → 整体区域(未被误分)",
              QString::number(p.area));
    }

    // ================= 回归保护：不交叉的非闭合图元集合安全跳过 =================
    {
        printf("\n-- 回归：开放交叉但不成环 --\n");
        // 两条互相交叉的开放直线，但没有任何闭合边界 → 不应产生可填充区域
        std::vector<Element> elems = {
            makeLine(Vec3(0, 0, 0), Vec3(100, 100, 0), 31),
            makeLine(Vec3(0, 100, 0), Vec3(100, 0, 0), 32)
        };
        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.empty(), "仅两条交叉开放直线、无闭合边界 → 0 个可填充区域(安全跳过)");
    }

    // ================= 用户场景：竖线端点恰好落在矩形上下边上(捕捉对齐) =================
    {
        printf("\n-- 竖线端点落在矩形上下边上(捕捉对齐画法) --\n");
        std::vector<Element> elems = {
            makeRect(0, 0, 100, 60, 51),
            makeLine(Vec3(50, 0, 0), Vec3(50, 60, 0), 52)  // 端点在矩形下边/上边，非超出
        };
        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.size() == 2, "端点在边上的竖线仍把矩形切成 2 个闭环(左右各一)",
              QString::number(set.loops.size()));
        const auto left = pickSectionRegionAt(elems, Vec3(25, 30, 0));
        check(left.ok() && left.area > 2900.0 && left.area < 3100.0,
              "点左侧 → 命中左半区域(面积≈3000，不被整块吞并)", QString::number(left.area));
        const auto right = pickSectionRegionAt(elems, Vec3(75, 30, 0));
        check(right.ok() && right.area > 2900.0 && right.area < 3100.0,
              "点右侧 → 命中右半区域(面积≈3000)", QString::number(right.area));

        // === 锁定用户核心诉求：填充左侧后竖线(端点落在边上)必须保留，不被剪切 ===
        // model.addElement() 会重新分配 ID，故按图元类型判定而非硬编码 id。
        {
            SketchModel model;
            for (const auto& e : elems) model.addElement(e);
            const std::vector<Element> srcs = model.elements();
            const auto lp = pickSectionRegionAt(srcs, Vec3(25, 30, 0));
            const auto res = makeSectionChangeForPick(lp, srcs, SectionStyle());
            model.beginEdit(QStringLiteral("剖面填充"));
            modify::applyChanges(&model, res.changes, QString());

            const std::vector<Element> after = model.elements();
            int nRect = 0, nLine = 0, nHatch = 0;
            for (const auto& e : after) {
                if (e.type == ElementType::Rectangle)   ++nRect;
                else if (e.type == ElementType::Line)   ++nLine;
                else if (e.type == ElementType::Hatch)  ++nHatch;
            }
            check(nRect == 1 && nLine == 1 && nHatch == 1,
                  "端点落在边上的竖线：填充左侧后矩形+竖线保留、仅新增 1 个 Hatch(竖线未被剪切)",
                  QStringLiteral("rect=%1 line=%2 hatch=%3").arg(nRect).arg(nLine).arg(nHatch));

            bool anyRemove = false;
            for (const auto& c : res.changes) if (c.remove) anyRemove = true;
            check(!anyRemove && res.changes.size() == 1,
                  "端点落在边上的竖线：填充动作仅 1 个新增 Hatch(无删除/无替换共享边)",
                  QString::number(res.changes.size()));

            const auto rAfter = pickSectionRegionAt(after, Vec3(75, 30, 0));
            check(rAfter.ok() && rAfter.area > 2900.0 && rAfter.area < 3100.0,
                  "填充左侧后，右侧仍可完整拾取(面积≈3000)", QString::number(rAfter.area));
            const auto resR = makeSectionChangeForPick(rAfter, after, SectionStyle());
            check(resR.filledRegions == 1, "填充左侧后，右侧仍可独立生成填充");
        }
    }

    // ================= 用户真实场景：斜边四边形(平行四边形) + 斜分割线 =================
    // 截图核心：一个带斜边的四边形(右斜边)被一条分隔线分成左右两半，二者只应填鼠标一侧，
    // 分隔线作为共享边界必须保留。此前测试全是规则矩形(无斜边)，未覆盖本形状。
    {
        printf("\n-- 斜边四边形(右斜边) + 竖直分割线(端点落在斜边) --\n");
        // 平行四边形：A(0,0)->B(100,0)->C(130,60)->D(30,60)，右斜边。面积=底100×高60=6000。
        // 分割线 x=65（底边中点→顶边中点），端点在斜的底/顶边上。左右面积各≈3000。
        std::vector<Element> elems = {
            makePolygon({ Vec3(0, 0, 0), Vec3(100, 0, 0), Vec3(130, 60, 0), Vec3(30, 60, 0) }, 61),
            makeLine(Vec3(65, 0, 0), Vec3(65, 60, 0), 62)
        };
        const auto set = buildSectionLoopSet(elems, kDefaultClosureTol);
        check(set.loops.size() == 2, "斜边四边形被竖线切成 2 个闭环(左右各一)",
              QString::number(set.loops.size()));
        const auto left = pickSectionRegionAt(elems, Vec3(32, 30, 0));
        check(left.ok() && left.area > 2900.0 && left.area < 3100.0,
              "斜边四边：点左 → 命中左半(面积≈3000)", QString::number(left.area));
        const auto right = pickSectionRegionAt(elems, Vec3(97, 30, 0));
        check(right.ok() && right.area > 2900.0 && right.area < 3100.0,
              "斜边四边：点右 → 命中右半(面积≈3000)", QString::number(right.area));

        // 落盘：填充左侧后，斜四边形+分割线保留，仅新增 1 个 Hatch（分割线不被剪切）。
        {
            SketchModel model;
            for (const auto& e : elems) model.addElement(e);
            const std::vector<Element> srcs = model.elements();
            const auto lp = pickSectionRegionAt(srcs, Vec3(32, 30, 0));
            const auto res = makeSectionChangeForPick(lp, srcs, SectionStyle());
            model.beginEdit(QStringLiteral("剖面填充"));
            modify::applyChanges(&model, res.changes, QString());

            const std::vector<Element> after = model.elements();
            int nPoly = 0, nLine = 0, nHatch = 0;
            for (const auto& e : after) {
                if (e.type == ElementType::Polygon) ++nPoly;
                else if (e.type == ElementType::Line) ++nLine;
                else if (e.type == ElementType::Hatch) ++nHatch;
            }
            check(nPoly == 1 && nLine == 1 && nHatch == 1,
                  "斜边四边填充左：斜四边形+分割线保留、仅新增 1 个 Hatch(分割线未剪切)",
                  QStringLiteral("poly=%1 line=%2 hatch=%3").arg(nPoly).arg(nLine).arg(nHatch));

            bool anyRemove = false;
            for (const auto& c : res.changes) if (c.remove) anyRemove = true;
            check(!anyRemove && res.changes.size() == 1,
                  "斜边四边填充左：动作仅 1 个新增 Hatch(无删除/无替换共享边)",
                  QString::number(res.changes.size()));
        }
    }

    printf("\n=== 汇总：PASS=%d FAIL=%d ===\n", g_pass, g_fail);
    if (g_fail == 0) { printf("RESULT: PASS\n"); return 0; }
    printf("RESULT: FAIL\n");
    return 1;
}
