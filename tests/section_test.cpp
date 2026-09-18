// 剖面区域表示（GB/T 4457.5-2013）核心层回归测试
//   * geo::hatchLines：等距平行线裁剪（间距/角度/内孔 even-odd 掏空）；
//   * geo::pointInPolygon：even-odd 点在多边形内；
//   * Section 引擎：sectionOutline 闭合轮廓转换、hatchDrawMode 豁免决策、
//     resolveAdjacency 相邻区域自动区分（4.7）、Hatch 图元往返一致性。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"

static double segLen(const geo::Seg& s) { return s.a.distTo(s.b); }

int main() {
    printf("=== 剖面区域表示回归（GB/T 4457.5-2013）===\n");

    // ---- 1. 等距平行线裁剪（水平剖面线，间距 3，margin 1 → 扫描 y=-1,2,5,8,11）----
    {
        auto segs = geo::hatchLines(square(0, 0, 10, 10), {}, 0.0, 3.0, 1.0);
        check(segs.size() == 3, "10×10 方形、间距3：生成 3 条剖面线",
              QString::number(segs.size()));
        bool allHoriz = true, allFull = true;
        for (const auto& s : segs) {
            if (!near2(s.a.y, s.b.y, 1e-9)) allHoriz = false;      // 水平
            if (!near2(segLen(s), 10.0, 1e-6)) allFull = false;    // 贯穿整宽
        }
        check(allHoriz, "剖面线均为水平（角度 0°）");
        check(allFull,  "剖面线均贯穿整个方形宽度 (10)");
        // 相邻两条线间距 = spacing
        std::vector<double> ys; for (auto& s : segs) ys.push_back(s.a.y);
        std::sort(ys.begin(), ys.end());
        bool spacingOk = true;
        for (size_t i = 1; i < ys.size(); ++i) if (!near2(ys[i] - ys[i - 1], 3.0, 1e-9)) spacingOk = false;
        check(spacingOk, "相邻剖面线间距恒为 3");
    }

    // ---- 2. 间距越小，剖面线越密 ----
    {
        const size_t n2 = geo::hatchLines(square(0, 0, 10, 10), {}, 0.0, 2.0, 1.0).size();
        const size_t n4 = geo::hatchLines(square(0, 0, 10, 10), {}, 0.0, 4.0, 1.0).size();
        check(n2 > n4, "间距 2 比间距 4 生成更多剖面线",
              QString("%1 vs %2").arg(n2).arg(n4));
    }

    // ---- 3. 内孔 even-odd 掏空（方形 0..10 挖方孔 3..7，间距 3 → y=-1,2,5,8,11）----
    {
        auto segs = geo::hatchLines(square(0, 0, 10, 10), { square(3, 3, 7, 7) }, 0.0, 3.0, 1.0);
        // y=2 → 1 段(0..10)；y=5 → 2 段(0..3 / 7..10)；y=8 → 1 段(0..10)
        check(segs.size() == 4, "带内孔：生成 4 段剖面线", QString::number(segs.size()));
        double total = 0; for (auto& s : segs) total += segLen(s);
        check(near2(total, 26.0, 1e-6), "带内孔：剖面线总长 = 10+3+3+10 = 26",
              QString::number(total));
        // 孔洞中心 (5,5) 不应被剖面线覆盖
        bool coversHoleCenter = false;
        for (const auto& s : segs) {
            const double y = s.a.y;
            if (!near2(y, 5.0, 1e-9)) continue;
            const double lo = std::min(s.a.x, s.b.x), hi = std::max(s.a.x, s.b.x);
            if (5.0 > lo + 1e-9 && 5.0 < hi - 1e-9) coversHoleCenter = true;
        }
        check(!coversHoleCenter, "内孔中心 (5,5) 未被剖面线覆盖");
    }

    // ---- 4. 45° 剖面线方向正确 ----
    {
        auto segs = geo::hatchLines(square(0, 0, 10, 10), {}, 45.0, 3.0, 1.0);
        const double c = std::cos(45.0 * M_PI / 180.0), s = std::sin(45.0 * M_PI / 180.0);
        bool parallel = !segs.empty();
        for (const auto& g : segs) {
            const double dx = g.b.x - g.a.x, dy = g.b.y - g.a.y;
            const double cross = dx * s - dy * c;      // 与 (cos45,sin45) 的叉积
            if (std::fabs(cross) > 1e-6 * (std::fabs(dx) + std::fabs(dy) + 1e-9)) parallel = false;
        }
        check(parallel, "45° 剖面线均平行于 45° 方向");
        check(!segs.empty(), "45° 剖面线非空");
    }

    // ---- 5. pointInPolygon（even-odd）----
    {
        auto sq = square(0, 0, 10, 10);
        check(geo::pointInPolygon(Vec3(5, 5), sq),  "点在方形内");
        check(!geo::pointInPolygon(Vec3(15, 5), sq),"点在方形外");
        check(!geo::pointInPolygon(Vec3(-1, 5), sq),"点在方形左外侧");
    }

    // ---- 6. sectionOutline：闭合轮廓转换 ----
    {
        Element circle; circle.type = ElementType::Circle;
        circle.pts = { Vec3(0, 0, 0) }; circle.radius = 5;
        std::vector<Vec3> out;
        check(sectionOutline(circle, out) && out.size() >= 3, "圆 → 剖面轮廓（采样 ≥3 点）",
              QString::number(out.size()));

        Element rect; rect.type = ElementType::Rectangle;
        rect.pts = { Vec3(-2, -1, 0), Vec3(4, 3, 0) };
        check(sectionOutline(rect, out) && out.size() == 4, "矩形 → 4 角点",
              QString::number(out.size()));

        Element poly; poly.type = ElementType::Polygon; poly.closed = true;
        poly.pts = square(0, 0, 5, 5);
        check(sectionOutline(poly, out) && out.size() == 4, "闭合多边形 → 顶点");

        Element line; line.type = ElementType::Line;
        line.pts = { Vec3(0, 0, 0), Vec3(5, 0, 0) };
        check(!sectionOutline(line, out), "直线 → 不构成剖面区域（false）");

        Element openPl; openPl.type = ElementType::Polyline; openPl.closed = false;
        openPl.pts = { Vec3(0, 0, 0), Vec3(5, 0, 0), Vec3(5, 5, 0) };
        check(!sectionOutline(openPl, out), "开放折线 → 不构成剖面区域（false）");
    }

    // ---- 7. hatchDrawMode：豁免决策 ----
    {
        SectionRegion r;
        r.style.exempt = SectionExempt::None;                check(hatchDrawMode(r) == HatchDraw::Hatch,     "无豁免 → 画剖面线");
        r.style.exempt = SectionExempt::AuxiliaryPart;       check(hatchDrawMode(r) == HatchDraw::NoFill,    "辅助零件 → 不画（4.2）");
        r.style.exempt = SectionExempt::SolidBarLongitudinal;check(hatchDrawMode(r) == HatchDraw::NoFill,    "实心轴纵向剖切 → 不画");
        r.style.exempt = SectionExempt::RibSpoke;            check(hatchDrawMode(r) == HatchDraw::NoFill,    "肋/轮辐 → 不画");
        r.style.exempt = SectionExempt::StandardPart;        check(hatchDrawMode(r) == HatchDraw::NoFill,    "标准件 → 不画");
        r.style.exempt = SectionExempt::SmallAreaSolid;      check(hatchDrawMode(r) == HatchDraw::SolidFill, "狭小区域 → 涂黑");
    }

    // ---- 8. resolveAdjacency：相邻区域自动区分（4.7）----
    {
        // 两相邻（包围盒相交）区域 → 角度相反（45° / 135°）
        SectionRegion a, b;
        a.boundary = square(0, 0, 10, 10);
        b.boundary = square(5, 5, 15, 15);
        a.style.angleDeg = b.style.angleDeg = 45.0;
        std::vector<SectionRegion> adj = { a, b };
        const bool changed = resolveAdjacency(adj, 45.0);
        check(changed, "相邻区域：resolveAdjacency 报告发生修改");
        check(near2(adj[0].style.angleDeg, 45.0, 1e-6) &&
              near2(adj[1].style.angleDeg, 135.0, 1e-6),
              "相邻区域角度相反（45° / 135°）",
              QString("%1 / %2").arg(adj[0].style.angleDeg).arg(adj[1].style.angleDeg));

        // 两不相邻区域 → 角度保持一致（45°）
        SectionRegion c, d;
        c.boundary = square(0, 0, 10, 10);
        d.boundary = square(50, 50, 60, 60);
        std::vector<SectionRegion> far = { c, d };
        const bool changed2 = resolveAdjacency(far, 45.0);
        check(!changed2, "不相邻区域：resolveAdjacency 无修改");
        check(near2(far[0].style.angleDeg, 45.0, 1e-6) && near2(far[1].style.angleDeg, 45.0, 1e-6),
              "不相邻区域角度一致（均 45°）");
    }

    // ---- 9. Hatch 图元往返（makeHatchElement ↔ hatchRegionFromElement）----
    {
        SectionRegion r;
        r.boundary = square(0, 0, 20, 10);
        r.holes = { square(4, 2, 8, 6) };
        r.style.angleDeg = 135.0;
        r.style.spacingMm = 2.5;
        r.style.exempt = SectionExempt::None;
        r.style.material = SectionMaterial::CastIron;

        Element e = makeHatchElement(r, QStringLiteral("剖面区域"));
        check(e.type == ElementType::Hatch, "makeHatchElement 类型为 Hatch");
        check(e.pts.size() == 4 && e.holes.size() == 1, "外边界 4 点、内孔 1 个");
        check(near2(e.rotation, 135.0, 1e-9), "角度存入 rotation");
        check(near2(e.radius, 2.5, 1e-9), "间距存入 radius");
        check(e.sectionExempt == (int)SectionExempt::None, "豁免类别写入");
        check(e.sectionMaterial == (int)SectionMaterial::CastIron, "材料类别写入");
        check(!e.lineWidthAuto, "剖面线固定细实线线宽（非自动）");
        check(near2(e.lineWidth, 2.0, 1e-9), "外轮廓默认粗实线线宽 2.0（填充不变细）");

        SectionRegion back = hatchRegionFromElement(e);
        check(near2(back.style.angleDeg, 135.0, 1e-9), "往返后角度一致");
        check(near2(back.style.spacingMm, 2.5, 1e-9), "往返后间距一致");
        check(back.style.exempt == SectionExempt::None, "往返后豁免一致");
        check(back.style.material == SectionMaterial::CastIron, "往返后材料一致");
        check(back.holes.size() == 1, "往返后内孔保留");

        // generateHatchLines 对带孔区域返回非空且都落在区域内
        auto segs = generateHatchLines(back);
        check(!segs.empty(), "generateHatchLines 对带孔区域返回非空");
        bool allInside = true;
        for (const auto& s : segs) {
            const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
            if (!geo::pointInPolygon(mid, back.boundary)) allInside = false;
        }
        check(allInside, "所有剖面线中点都落在区域外边界内");
    }

    // ---- 10. 材料驱动剖面线族（GB/T 4457.5-2013 + 行业材料符号）----
    {
        auto fam = [&](SectionMaterial m) {
            SectionStyle st; st.material = m; st.angleDeg = 45.0; st.spacingMm = 3.0;
            return hatchPatternFor(st);
        };
        const auto g = fam(SectionMaterial::General);
        check(g.anglesDeg.size() == 1 && near2(g.anglesDeg[0], 45.0),
              "通用金属：单一 45° 斜线族");

        const auto ci = fam(SectionMaterial::CastIron);
        check(ci.anglesDeg.size() == 2 && near2(ci.anglesDeg[1], 135.0, 1e-6),
              "铸铁：45°+135° 网状剖面线");

        const auto nm = fam(SectionMaterial::NonMetal);
        check(nm.anglesDeg.size() == 2, "非金属：交叉网格（两族）");

        const auto wd = fam(SectionMaterial::Wood);
        check(wd.anglesDeg.size() == 1 && near2(wd.anglesDeg[0], 0.0, 1e-6),
              "木材：顺纹水平线族（近似木纹）");

        const auto gl = fam(SectionMaterial::Glass);
        check(gl.dots && gl.anglesDeg.empty(), "玻璃：点阵符号（无斜线族）");

        const auto li = fam(SectionMaterial::Liquid);
        check(li.dots && li.anglesDeg.empty(), "液体：点阵符号");

        const auto co = fam(SectionMaterial::Concrete);
        check(co.dots && co.anglesDeg.size() == 2, "混凝土：交叉线 + 碎石点阵");

        // 用户设定角度对通用金属生效（铸铁等锚定 45° 基准）
        SectionStyle stU; stU.material = SectionMaterial::General; stU.angleDeg = 30.0;
        check(near2(hatchPatternFor(stU).anglesDeg[0], 30.0, 1e-6), "通用金属尊重用户设定角度");

        // generateHatchLines 对铸铁返回双向交叉线（线数多于单一 45°）
        SectionRegion rc; rc.boundary = square(0, 0, 10, 10);
        rc.style.material = SectionMaterial::CastIron; rc.style.angleDeg = 45.0;
        const size_t nCi = generateHatchLines(rc).size();
        const size_t nGen = generateHatchLines([&] { SectionRegion r; r.boundary = square(0,0,10,10);
            r.style.material = SectionMaterial::General; r.style.angleDeg = 45.0; return r; }()).size();
        check(nCi > nGen, "铸铁剖面线数多于通用金属（多一族）", QString("%1 vs %2").arg(nCi).arg(nGen));

        // 点阵对玻璃落在区域内
        SectionRegion rg; rg.boundary = square(0, 0, 10, 10); rg.style.material = SectionMaterial::Glass;
        const auto dots = generateHatchDots(rg);
        check(!dots.empty(), "玻璃生成点阵非空");
        bool dotsInside = true;
        for (const auto& d : dots) if (!geo::pointInPolygon(d, rg.boundary)) dotsInside = false;
        check(dotsInside, "所有点阵点都落在区域内");
    }

    // ---- 11. 填充后外轮廓保留原线宽（修复"变细"）----
    {
        // 用一个 lineWidth=3.0 的源圆（闭合图元）走 makeSectionChanges
        Element src; src.type = ElementType::Circle;
        src.pts = { Vec3(0, 0, 0) }; src.radius = 20.0; src.id = 7;
        src.lineWidth = 3.0; src.lineWidthAuto = false; src.layer = QStringLiteral("L");
        const auto changes = makeSectionChanges({src}, SectionStyle(), /*autoDistinguish=*/false);
        check(changes.size() == 1, "闭合圆 → 1 个剖面替换动作");
        const Element& h = changes[0].replacement;
        check(near2(h.lineWidth, 3.0, 1e-6), "Hatch 外轮廓线宽继承源图元（3.0，不变细）");
        check(!h.lineWidthAuto, "Hatch 线宽非自动");
        check(h.id == 7, "replacement.id = 源 ID（落盘目标正确）");
    }

    printf(g_fail == 0 ? "SectionTest ALL PASS\n" : "SectionTest FAILED (%d)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
