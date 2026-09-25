// 剖面区域表示（GB/T 4457.5-2013）核心层回归测试
//   * geo::hatchLines：等距平行线裁剪（间距/角度/内孔 even-odd 掏空）；
//   * geo::pointInPolygon：even-odd 点在多边形内；
//   * Section 引擎：sectionOutline 闭合轮廓转换、hatchDrawMode 豁免决策、
//     resolveAdjacency 相邻区域自动区分（4.7）、Hatch 图元往返一致性。
// 纯核心层，无需 GUI。
#include "section_test_helpers.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static double segLen(const geo::Seg& s) { return s.a.distTo(s.b); }

int main(int argc, char** argv) {
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
        check(gl.dashes && gl.dashLongShort && gl.anglesDeg.size()==1 && near2(gl.anglesDeg[0],45.0,1e-6),
              "玻璃：45° 长短画线（断续线，非点阵）");

        const auto li = fam(SectionMaterial::Liquid);
        check(li.dashes && !li.dashLongShort && li.anglesDeg.size()==1 && near2(li.anglesDeg[0],0.0,1e-6),
              "液体：水平短画线（液面下，断续线）");

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

        // 玻璃：同一大区域内，45° 长短画线段数应远多于连续 45° 线（每条长线被切成多段断续线）。
        // 用小区域时单条线不足一个 dash 周期无法体现差异，故用 120mm 方区。
        SectionRegion rg; rg.boundary = square(0, 0, 120, 120); rg.style.material = SectionMaterial::Glass;
        const auto glassSegs = generateHatchLines(rg);
        check(!glassSegs.empty(), "玻璃生成剖面线（断续线）非空");
        const size_t nGenBig = generateHatchLines([&] { SectionRegion r;
            r.boundary = square(0, 0, 120, 120);
            r.style.material = SectionMaterial::General; r.style.angleDeg = 45.0; return r; }()).size();
        check(glassSegs.size() > nGenBig * 2,
              "玻璃：长短画线段数远多于连续 45° 线（断续切割）",
              QString("%1 vs %2").arg(glassSegs.size()).arg(nGenBig));
        // 液体：水平短画线段数同样远多于连续水平线（同区对比）
        SectionRegion rl; rl.boundary = square(0, 0, 120, 120); rl.style.material = SectionMaterial::Liquid;
        const auto liqSegs = generateHatchLines(rl);
        const size_t nGenH = generateHatchLines([&] { SectionRegion r;
            r.boundary = square(0, 0, 120, 120);
            r.style.material = SectionMaterial::General; r.style.angleDeg = 0.0; return r; }()).size();
        check(liqSegs.size() > nGenH * 2,
              "液体：水平短画线段数远多于连续水平线（断续切割）",
              QString("%1 vs %2").arg(liqSegs.size()).arg(nGenH));

        // ---- 10b. 新增材料：铝 / 砖墙 / 石材 / 保温层 / 胶合板（本次扩展）----
        // 铝（非铁金属）：与钢反向 45°（GB 4.7 相邻金属反向）
        const auto al = fam(SectionMaterial::Aluminum);
        check(al.anglesDeg.size() == 1 && near2(al.anglesDeg[0], -45.0, 1e-6),
              "铝：与钢反向的单族 -45°");

        // 胶合板：交叉层（水平 + 垂直）
        const auto py = fam(SectionMaterial::Plywood);
        check(py.anglesDeg.size() == 2 && near2(py.anglesDeg[0], 0.0) && near2(py.anglesDeg[1], 90.0),
              "胶合板：交叉层 0°+90°");

        // 砖墙 / 石材：砌体图案（brick=true，直接铺砌缝）
        const auto bk = fam(SectionMaterial::Brick);
        check(bk.brick && bk.brickStagger && !bk.brickIrregular, "砖墙：砌体图案（错缝、规则）");
        const auto stn = fam(SectionMaterial::Stone);
        check(stn.brick && stn.brickIrregular, "石材：砌体图案（不规则竖缝）");

        // 保温层：点阵
        const auto ins = fam(SectionMaterial::Insulation);
        check(ins.dots && ins.anglesDeg.empty(), "保温层：点阵符号");

        // 砖墙生成：同一方形区域，砖墙线数应显著多于通用金属（水平砌缝 + 竖缝）
        SectionRegion rb; rb.boundary = square(0, 0, 60, 60);
        rb.style.material = SectionMaterial::Brick; rb.style.spacingMm = 3.0;
        const auto brickSegs = generateHatchLines(rb);
        check(!brickSegs.empty(), "砖墙生成剖面线非空");
        const size_t nBrick = brickSegs.size();
        check(nBrick > nGen, "砖墙剖面线数多于通用金属", QString("%1 vs %2").arg(nBrick).arg(nGen));
        // 竖直缝存在：至少一条线段的两个端点 x 相同（水平砌缝两端 x 不同）
        bool hasVertical = false;
        for (const auto& s : brickSegs)
            if (near2(s.a.x, s.b.x, 1e-6) && !near2(s.a.y, s.b.y, 1e-6)) { hasVertical = true; break; }
        check(hasVertical, "砖墙包含竖直砌缝（错缝竖线）");
        // 水平砌缝存在：至少一条线段两端 y 相同
        bool hasHorizontal = false;
        for (const auto& s : brickSegs)
            if (near2(s.a.y, s.b.y, 1e-6) && !near2(s.a.x, s.b.x, 1e-6)) { hasHorizontal = true; break; }
        check(hasHorizontal, "砖墙包含水平砌缝（每皮砖）");
    }

    // ---- 10c. GB/T 4457.5-2013 表1 其余材料：线圈/叠钢片/型砂/泥土/钢筋混凝土/筛网 ----
    {
        auto fam = [&](SectionMaterial m) {
            SectionStyle st; st.material = m; st.angleDeg = 45.0; st.spacingMm = 3.0;
            return hatchPatternFor(st);
        };
        // 材料 → 剖面符号映射（hatchPatternFor）
        const auto cw = fam(SectionMaterial::CoilWinding);
        check(cw.anglesDeg.size()==1 && cw.dots, "线圈绕组元件：45° 线 + 点阵");

        const auto ls = fam(SectionMaterial::LaminatedSteel);
        check(ls.anglesDeg.size()==1 && !ls.dots, "叠钢片：薄 45° 线（单族，无点）");

        const auto sd = fam(SectionMaterial::Sand);
        check(sd.dots && sd.anglesDeg.empty(), "型砂/粉末冶金等：自由分布点阵");

        const auto so = fam(SectionMaterial::Soil);
        check(so.dots && so.anglesDeg.empty() && so.dotStepScale > 1.0, "基础周围泥土：疏点（步长放大）");

        const auto rc2 = fam(SectionMaterial::ReinforcedConcrete);
        check(rc2.anglesDeg.size()==1 && rc2.dots, "钢筋混凝土：45° 线 + 点（钢筋示意）");

        const auto ms = fam(SectionMaterial::Mesh);
        check(ms.anglesDeg.size()==2 && near2(ms.anglesDeg[0],0.0) && near2(ms.anglesDeg[1],90.0),
              "筛网/过滤网：交叉网格（0°+90°）");

        // 生成器冒烟：网格生成非空线段；型砂生成非空点阵
        SectionRegion rmesh; rmesh.boundary = square(0,0,10,10); rmesh.style.material = SectionMaterial::Mesh;
        check(!generateHatchLines(rmesh).empty(), "筛网生成网格线非空");
        SectionRegion rsand; rsand.boundary = square(0,0,10,10); rsand.style.material = SectionMaterial::Sand;
        check(!generateHatchDots(rsand).empty(), "型砂生成点阵非空");
        // 钢筋混凝土：同时产出斜线 + 点阵
        SectionRegion rrc; rrc.boundary = square(0,0,10,10); rrc.style.material = SectionMaterial::ReinforcedConcrete;
        check(!generateHatchLines(rrc).empty() && !generateHatchDots(rrc).empty(),
              "钢筋混凝土：斜线 + 点阵均非空");

        // 枚举上界与计数自洽（kSectionMaterialCount 含通用金属=0..18 共 19）
        check((int)SectionMaterial::Mesh == kSectionMaterialCount - 1,
              "Mesh 为当前最大枚举值（kSectionMaterialCount-1）");
    }

    // ---- 10d. PAT 填充图案文件支持（AutoCAD .pat，exe/hatch/ 加载）----
    {
        // (a) 解析：内联一份最小 .pat，校验头 / 线 / 笔段解析
        const QString sample =
            QStringLiteral("*TESTPAT, test pattern\n")
            + QStringLiteral("0, 0, 0, 10, 5, 8, -4, 6, -2\n")    // angle0, dx10 dy5, 笔段 8,-4,6,-2
            + QStringLiteral("90, 0, 0, 0, 7, 5, -3\n");          // angle90, 竖直族 spacing7, 笔段 5,-3
        const auto ps = HatchPatternLibrary::parsePatText(sample);
        check(ps.size() == 1, "parsePatText：单文件单图案解析");
        check(ps[0].name == QStringLiteral("TESTPAT"), "图案名解析为 TESTPAT");
        check(ps[0].description == QStringLiteral("test pattern"), "描述解析为 test pattern");
        check(ps[0].lines.size() == 2, "两条定义线", QString::number(ps[0].lines.size()));
        const auto& l0 = ps[0].lines[0];
        check(near2(l0.angleDeg, 0.0) && near2(l0.dx, 10.0) && near2(l0.dy, 5.0),
              "定义行 角度/dx/dy 解析");
        check(l0.dashes.size() == 4 && near2(l0.dashes[0], 8.0) && near2(l0.dashes[1], -4.0)
              && near2(l0.dashes[2], 6.0) && near2(l0.dashes[3], -2.0),
              "笔段序列解析（正负交替，从实段起）");
        // 长度≈0 的笔段应被丢弃（木纹类行尾占位 0）
        const QString sampleZ =
            QStringLiteral("*ZPAT, with trailing zeros\n0, 0, 0, 10, 5, 7, -3, 0, 0, 0\n");
        const auto pz = HatchPatternLibrary::parsePatText(sampleZ);
        check(pz.size() == 1 && pz[0].lines.size() == 1
              && pz[0].lines[0].dashes.size() == 2,
              "行尾长度0笔段被丢弃（仅保留 7,-3）",
              QString::number(pz.empty()?0:pz[0].lines.empty()?0:pz[0].lines[0].dashes.size()));

        // (b) 生成器：用单条 angle=0 的图案对矩形区域生成，校验几何（避免多定义行中
        //     存在的 angle=90 竖线干扰"水平"判定）。
        PatPattern singlePat; singlePat.name = QStringLiteral("SINGLE");
        PatLine sl; sl.angleDeg = 0.0; sl.x0 = 0.0; sl.y0 = 0.0; sl.dx = 10.0; sl.dy = 5.0;
        sl.dashes = { 8.0, -4.0, 6.0, -2.0 };
        singlePat.lines = { sl };
        SectionRegion rpat; rpat.boundary = square(0, 0, 100, 60);
        auto psegs = generateHatchLinesFromPat(singlePat, rpat);
        check(!psegs.empty(), "generateHatchLinesFromPat：生成非空");
        // angle=0 → 线段应水平（a.y≈b.y）
        bool horiz = true;
        for (const auto& s : psegs) if (!near2(s.a.y, s.b.y, 1e-6)) horiz = false;
        check(horiz, "PAT 角度0 → 剖面线水平");
        // 相邻水平线间距应≈ dy=5
        std::vector<double> ys; for (auto& s : psegs) ys.push_back(s.a.y);
        std::sort(ys.begin(), ys.end());
        std::vector<double> gaps;
        for (size_t i = 1; i < ys.size(); ++i) gaps.push_back(ys[i] - ys[i - 1]);
        gaps.erase(std::unique(gaps.begin(), gaps.end(),
                               [](double a, double b) { return near2(a, b, 1e-3); }), gaps.end());
        bool spacing5 = false;
        for (double g : gaps) if (near2(g, 5.0, 1e-3)) spacing5 = true;
        check(spacing5, "PAT 相邻线间距≈ dy=5",
              QString::number(gaps.empty() ? -1.0 : gaps.front()));
        // 断续：笔段把长线切成较短实段（明显短于区域宽 100）
        double maxLen = 0; for (auto& s : psegs) maxLen = std::max(maxLen, segLen(s));
        check(maxLen < 100.0, "PAT 笔段将长线切成较短实段（< 区域宽）", QString::number(maxLen));

        // (c) 材料映射：Wood → "wood"，生成改用 PAT
        SectionStyle stW; stW.material = SectionMaterial::Wood;
        check(hatchPatternFor(stW).patName == QStringLiteral("wood"),
              "Wood 材料映射到 PAT 名 \"wood\"");

        // (d) 加载器机制：临时目录写入合成 .pat，按内部名/文件名（大小写不敏感）查找
        const QString tmpDir = QDir::tempPath() + QStringLiteral("/sketchcad_hatch_test");
        QDir d2(tmpDir); d2.removeRecursively(); d2.mkpath(".");
        {
            QFile f(d2.filePath(QStringLiteral("test.pat")));
            (void)f.open(QIODevice::WriteOnly | QIODevice::Text);
            QTextStream ts(&f);
            ts << "*TPAT, synthetic\n0, 0, 0, 10, 5, 8, -4\n";
            f.close();
        }
        HatchPatternLibrary::instance().loadFromDir(tmpDir);    // 重新加载到单例
        check(HatchPatternLibrary::instance().find(QStringLiteral("TPAT")) != nullptr,
              "loadFromDir：按内部名查找 TPAT");
        check(HatchPatternLibrary::instance().find(QStringLiteral("tpat")) != nullptr,
              "find：图案名大小写不敏感");
        check(HatchPatternLibrary::instance().find(QStringLiteral("test")) != nullptr,
              "find：按文件名（去扩展名）查找");
        {
            const auto* pp = HatchPatternLibrary::instance().find(QStringLiteral("TPAT"));
            SectionRegion rt; rt.boundary = square(0, 0, 50, 50);
            const auto ts2 = pp ? generateHatchLinesFromPat(*pp, rt) : std::vector<geo::Seg>{};
            check(!ts2.empty(), "loadFromDir 后：合成图案生成非空");
        }
        d2.removeRecursively();

        // (e) 部署路径：从 exe 同级 hatch/ 加载真实 wood.pat（build/hatch/wood.pat 已部署）。
        // 取本测试 exe 所在目录（argv[0]），其下 hatch/ 即运行时部署形态。
        QString exeDir;
        if (argc > 0) exeDir = QFileInfo(QString::fromUtf8(argv[0])).dir().absolutePath();
        const QString hatchPath = exeDir + QStringLiteral("/hatch");
        if (QDir(hatchPath).exists()) {
            HatchPatternLibrary::instance().loadFromDir(hatchPath);  // 加载真实部署目录
            const auto* wp = HatchPatternLibrary::instance().find(QStringLiteral("wood"));
            check(wp != nullptr, "部署路径：exe/hatch/wood.pat 可被加载并按 \"wood\" 查找", hatchPath);
            if (wp) {
                SectionRegion rwood; rwood.boundary = square(0, 0, 80, 80);
                const auto ws = generateHatchLinesFromPat(*wp, rwood);
                check(!ws.empty(), "真实 wood.pat：生成木纹剖面线非空");
                bool wHoriz = true;
                for (const auto& s : ws) if (!near2(s.a.y, s.b.y, 1e-6)) { wHoriz = false; break; }
                check(wHoriz, "wood.pat 首图案(HOLZ-2)角度0 → 水平木纹线");
                double maxW = 0; for (auto& s : ws) maxW = std::max(maxW, segLen(s));
                check(maxW < 80.0, "wood.pat：笔段将长线切成短实段（< 区域宽）", QString::number(maxW));
            }
        } else {
            check(true, "（部署目录 exe/hatch 当前不可见，跳过真实 wood.pat 加载校验）");
        }
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
