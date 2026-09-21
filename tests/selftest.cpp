// 无界面自测：验证核心层（模型 / 命令解释器 / 撤销重做 / 剪贴板 / 排序）
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <QRectF>
#include "core/SketchModel.h"
#include "core/CommandInterpreter.h"
#include "core/Element.h"
#include "core/Geometry.h"
#include "core/DimStyle.h"
#include "core/Trim.h"
#include "core/Snap.h"
#include "core/LineModes.h"
#include "core/SplineModes.h"
#include "core/EllipseModes.h"
#include "core/ToolRegistry.h"
#include "core/DimensionStandard.h"
#include "core/SheetSpec.h"
#include "core/Modify.h"
#include "core/ElementFactory.h"
#include "core/Commands.h"
#include "core/AircraftViews.h"
#include "core/CadFormats.h"
#include <algorithm>
#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace SketchCAD;

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const QString& name, const QString& detail = QString()) {
    if (ok) { ++g_pass; qInfo().noquote() << "  [PASS]" << name; }
    else    { ++g_fail; qCritical().noquote() << "  [FAIL]" << name << detail; }
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    SketchModel m;
    CommandInterpreter ci(&m);

    qInfo().noquote() << "\n=== 1. 命令行绘图 ===";
    check(ci.execute("line 0,0 100,0"),      "line 0,0 100,0");
    check(ci.execute("circle 50,50 25"),     "circle 50,50 25");
    check(ci.execute("arc 0,0 30 0 90"),     "arc 0,0 30 0 90");
    check(ci.execute("rect 0,0 60,40"),      "rect 0,0 60,40");
    check(ci.execute("poly 0,0 10,0 10,10"), "poly 三点多边形");
    check(m.elementCount() == 5,
          "图元总数 == 5", QString("实际=%1").arg(m.elementCount()));

    qInfo().noquote() << "\n=== 2. 几何数据正确性 ===";
    {
        const Element* e = m.element(1);
        check(e && e->type == ElementType::Line, "1# 为直线");
        check(e && qFuzzyCompare(e->pts[1].x, 100.0), "直线终点 X == 100");
        const Element* c = m.element(2);
        check(c && c->type == ElementType::Circle && qFuzzyCompare(c->radius, 25.0),
              "2# 为圆且半径 == 25");
        const Element* a = m.element(3);
        check(a && a->type == ElementType::Arc && qFuzzyCompare(a->endAngle, 90.0),
              "3# 为圆弧且终止角 == 90");
        // 矩形按「两个对角点」存储（命令行 / 鼠标 / 渲染三处一致）
        const Element* r = m.element(4);
        check(r && r->pts.size() == 2, "4# 矩形按两对角点存储",
              QString("实际点数=%1").arg(r ? (int)r->pts.size() : -1));
        check(r && qFuzzyCompare(r->pts[1].x, 60.0) && qFuzzyCompare(r->pts[1].y, 40.0),
              "矩形对角点 == (60,40)");
    }

    qInfo().noquote() << "\n=== 3. 非法命令拒绝 ===";
    int before = m.elementCount();
    check(!ci.execute("line"),            "缺参数的 line 被拒绝");
    check(!ci.execute("circle 0,0"),      "缺半径的 circle 被拒绝");
    check(!ci.execute("notacommand 1 2"), "未知命令被拒绝");
    check(m.elementCount() == before,     "非法命令未污染模型");

    qInfo().noquote() << "\n=== 4. 撤销 / 重做 ===";
    int n0 = m.elementCount();
    m.undo();
    check(m.elementCount() == n0 - 1, "undo 后减少 1 个图元",
          QString("实际=%1").arg(m.elementCount()));
    m.redo();
    check(m.elementCount() == n0, "redo 后恢复",
          QString("实际=%1").arg(m.elementCount()));
    check(ci.execute("undo") && ci.execute("redo"), "undo/redo 命令可执行");

    qInfo().noquote() << "\n=== 5. 复制 / 粘贴 / 删除 ===";
    m.setSelected(2);
    check(m.selected() && m.selected()->id == 2, "选中 2#");
    m.copySelected();
    m.paste();
    check(m.elementCount() == n0 + 1, "粘贴后 +1",
          QString("实际=%1").arg(m.elementCount()));
    m.deleteSelected();
    check(m.elementCount() == n0, "删除选中后回到 n0",
          QString("实际=%1").arg(m.elementCount()));

    qInfo().noquote() << "\n=== 6. BOM 拖拽重排 ===";
    {
        int firstId = m.elements().front().id;
        m.reorderElement(firstId, -1);
        check(m.elements().back().id == firstId, "首元素移到末尾");
        int lastId = m.elements().back().id;
        int newFirst = m.elements().front().id;
        m.reorderElement(lastId, newFirst);
        check(m.elements().front().id == lastId, "末元素移回开头");
    }

    qInfo().noquote() << "\n=== 7. 视图 / 平面 / 栅格命令 ===";
    check(ci.execute("plane yz") && m.plane() == Plane::YZ, "plane yz 切换生效");
    check(ci.execute("plane xy") && m.plane() == Plane::XY, "plane xy 切回");
    bool g0 = m.showGrid();
    check(ci.execute("grid") && m.showGrid() != g0, "grid 无参切换生效");
    check(ci.execute("grid on")  && m.showGrid(),  "grid on 生效");
    check(ci.execute("grid off") && !m.showGrid(), "grid off 生效");
    check(!ci.execute("grid maybe"), "grid 非法参数被拒绝");
    m.setShowGrid(g0);
    check(ci.execute("zoom 2") && qFuzzyCompare(m.viewScale(), 2.0), "zoom 2 生效");
    check(!ci.execute("zoom -1"), "zoom 负值被拒绝");

    qInfo().noquote() << "\n=== 8. 命令补全 & 历史 ===";
    {
        QStringList c = ci.completions("c");
        check(c.contains("circle") && c.contains("clear"), "前缀 c 补全含 circle/clear",
              c.join(","));
        QStringList c2 = ci.completions("li");
        check(c2.contains("line") && c2.contains("list") && c2.size() > 2,
              "前缀 li 补全含 line/list 及直线子命令", c2.join(","));
        QStringList c3 = ci.completions("sp");
        // 样条家族命令注册后，"sp" 补全应包含 spline 本体与全部 spline_* 子命令
        check(c3.contains("spline") && c3.contains("spline_parabola") &&
              c3.contains("spline_explode"),
              "前缀 sp 补全含 spline 及样条家族", c3.join(","));
        QStringList c4 = ci.completions("spline_remove");
        check(c4.contains("spline_remove_pts") && c4.contains("spline_remove_between") &&
              c4.size() == 2,
              "前缀 spline_remove 补全恰为 2 个编辑命令", c4.join(","));
        check(ci.completions("zzz").isEmpty(), "无匹配前缀返回空");
        ci.addHistory("line 0,0 1,1");
        ci.addHistory("circle 0,0 5");
        check(ci.history().size() >= 2 && ci.history().last() == "circle 0,0 5",
              "历史记录按序追加");
        check(ci.commandNames().size() >= 15, "命令表 >= 15 条",
              QString("实际=%1").arg(ci.commandNames().size()));
    }

    qInfo().noquote() << "\n=== 9. 属性更新（属性面板链路） ===";
    {
        int id = m.elements().front().id;
        bool ok = m.updateElement(id, [](Element& e) {
            e.name = "改名测试"; e.layer = "结构层"; e.color = QColor("#ff0000");
        });
        const Element* e = m.element(id);
        check(ok && e && e->name == "改名测试" && e->layer == "结构层",
              "updateElement 写入生效");
        check(e && e->color.name() == "#ff0000", "颜色写入生效");
        check(!e->sizeDesc().isEmpty(), "sizeDesc() 有输出", e ? e->sizeDesc() : "");
    }

    qInfo().noquote() << "\n=== 10. 信号联动 ===";
    {
        int sigModel = 0, sigSel = 0, sigLog = 0;
        QObject::connect(&m, &SketchModel::modelChanged,     [&] { ++sigModel; });
        QObject::connect(&m, &SketchModel::selectionChanged, [&](int) { ++sigSel; });
        QObject::connect(&m, &SketchModel::logAppended,      [&](const QString&, const QString&) { ++sigLog; });
        ci.execute("line 5,5 15,15");
        m.setSelected(m.elements().back().id);
        m.log("联动测试", "info");
        check(sigModel >= 1, "modelChanged 触发", QString::number(sigModel));
        check(sigSel   >= 1, "selectionChanged 触发", QString::number(sigSel));
        check(sigLog   >= 1, "logAppended 触发", QString::number(sigLog));
    }

    qInfo().noquote() << "\n=== 11. 约束 / 标注命令 ===";
    {
        int n = m.elementCount();
        check(ci.execute("constraint parallel 1 2"), "constraint parallel 可执行");
        check(ci.execute("dim dist 0,0 100,0"),      "dim dist 可执行");
        check(!ci.execute("constraint nosuch 1"),    "未知约束类型被拒绝");
        check(!ci.execute("dim nosuch 1"),           "未知标注类型被拒绝");
        check(m.elementCount() == n + 2, "仅合法的 2 条被记录",
              QString("实际=%1").arg(m.elementCount() - n));
        const Element* c = m.element(m.elements().back().id - 1);
        check(c && c->type == ElementType::Constraint && c->ctype == ConstraintType::Parallel,
              "约束类型正确落库");
    }

    qInfo().noquote() << "\n=== 12. 修剪 / 延伸 的交点几何 ===";
    {
        // 与 GraphicsView::lineIntersect 同一套公式，独立验证数值正确性
        auto isect = [](double x1, double y1, double x2, double y2,
                        double x3, double y3, double x4, double y4,
                        double& ox, double& oy) -> bool {
            double den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
            if (std::fabs(den) < 1e-9) return false;
            double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den;
            ox = x1 + t * (x2 - x1); oy = y1 + t * (y2 - y1);
            return true;
        };
        double ox = 0, oy = 0;
        check(isect(0, 0, 100, 0,  50, -50, 50, 50, ox, oy) &&
              qFuzzyCompare(ox, 50.0) && qFuzzyCompare(oy + 1.0, 1.0),
              "垂直相交 -> (50,0)", QString("得到 (%1,%2)").arg(ox).arg(oy));
        check(isect(0, 0, 100, 100,  0, 100, 100, 0, ox, oy) &&
              qFuzzyCompare(ox, 50.0) && qFuzzyCompare(oy, 50.0),
              "对角相交 -> (50,50)", QString("得到 (%1,%2)").arg(ox).arg(oy));
        check(!isect(0, 0, 100, 0,  0, 20, 100, 20, ox, oy), "平行线判定为无交点");
        // 交点在段外（延伸场景）：目标 0..40，边界在 x=80
        check(isect(0, 0, 40, 0,  80, -10, 80, 10, ox, oy) && ox > 40.0,
              "交点落在段外（应走延伸）", QString("ox=%1").arg(ox));
        // 参数 s 判定段内/段外
        auto param = [](double px, double x0, double x1) { return (px - x0) / (x1 - x0); };
        check(param(50, 0, 100) > 0 && param(50, 0, 100) < 1, "s=0.5 判定为段内");
        check(param(80, 0, 40) > 1, "s=2.0 判定为段外");
    }

    qInfo().noquote() << "\n=== 13. clear ===";
    m.clear();
    check(m.elementCount() == 0, "clear 后模型为空");

    // ----------------------------------------------------------------
    // 以下 4 个 section 覆盖本轮新增的核心能力（需求 1/2/3/4 的算法层）
    // ----------------------------------------------------------------

    qInfo().noquote() << "\n=== 14. 几何层：极坐标属性 / 角度规整 / 测量值（需求1）===";
    {
        const Vec3 A(0, 0, 0), B(3, 4, 0);
        check(qFuzzyCompare(geo::segLength(A, B), 5.0), "segLength(0,0 → 3,4) == 5");
        const double ang = geo::segAngleDeg(A, B);                 // atan2(4,3)
        check(std::fabs(ang - 53.130102) < 1e-3, "segAngleDeg(0,0 → 3,4) ≈ 53.13°", QString::number(ang));
        const Vec3 E = geo::endpointFrom(A, 5.0, ang);            // 由长度+夹角反算
        check(qFuzzyCompare(E.x, 3.0) && qFuzzyCompare(E.y, 4.0),
              "endpointFrom 反算回到 (3,4)", QString("(%1,%2)").arg(E.x).arg(E.y));

        // 角度规整（属性面板「改夹角」后会落回 [0,360)）
        check(qFuzzyCompare(geo::wrapDeg360(370.0),  10.0), "wrapDeg360(370)==10");
        check(qFuzzyCompare(geo::wrapDeg360(-10.0), 350.0), "wrapDeg360(-10)==350");
        check(std::fabs(geo::wrapDeg180( 190.0) - (-170.0)) < 1e-9, "wrapDeg180(190)==-170");
        check(std::fabs(geo::wrapDeg180(-190.0) -   170.0)  < 1e-9, "wrapDeg180(-190)==170");

        // 控制点读写（端点拖拽 / 属性面板编辑的底层）
        Element ln; ln.type = ElementType::Line; ln.pts = {A, B};
        auto hs = geo::elementHandles(ln);
        check(hs.size() == 2 && qFuzzyCompare(hs[1].x, 3.0), "直线控制点 = [起点,终点]");
        check(geo::setElementHandle(ln, 1, Vec3(200, 50, 0)), "setElementHandle 改写终点");
        check(qFuzzyCompare(ln.pts[1].x, 200.0) && qFuzzyCompare(ln.pts[1].y, 50.0),
              "终点已改为 (200,50)");

        // 测量值：属性面板显示的「长度/角度/半径/直径/角度」都来自这里
        auto dimEl = [](DimensionType dt, const std::vector<Vec3>& pts, double r = 0) {
            Element e; e.type = ElementType::Dimension; e.dtype = dt; e.pts = pts; e.radius = r; return e;
        };
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Distance,  {A, Vec3(100, 0, 0)})), 100.0), "线性(自动)测量=100");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Aligned,   {A, B})), 5.0),                "对齐测量=5");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Horizontal, {A, Vec3(100, 50, 0)})), 100.0), "水平Δx=100");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Vertical,   {A, Vec3(100, 50, 0)})), 50.0),  "竖直Δy=50");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Angle,      {A, Vec3(100,0,0), Vec3(0,100,0)})), 90.0), "角度=90°");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Radius,    {A, Vec3(25,0,0)}, 25)), 25.0), "半径=25");
        check(qFuzzyCompare(geo::measureDimension(dimEl(DimensionType::Diameter,   {A, Vec3(25,0,0)}, 25)), 50.0), "直径=50");
    }

    qInfo().noquote() << "\n=== 15. 尺寸标注几何生成（GB/T 4458.4-2003，需求2）===";
    {
        // 线性：A(0,0) B(100,0) → 2 尺寸界线 + 1 尺寸线 + 2 实心闭合箭头
        Element lin; lin.type = ElementType::Dimension; lin.dtype = DimensionType::Distance;
        lin.pts = {Vec3(0,0,0), Vec3(100,0,0)};
        dim::Geom g = dim::build(lin);
        check(g.valid, "线性标注 build 有效");
        check(g.ext.size() == 2, "线性标注含 2 条尺寸界线", QString::number(g.ext.size()));
        check(g.line.size() == 1, "线性标注含 1 条尺寸线", QString::number(g.line.size()));
        check(g.arrows.size() == 2, "线性标注含 2 个实心闭合箭头", QString::number(g.arrows.size()));
        check(qFuzzyCompare(g.measured, 100.0), "线性标注实测值=100");
        check(g.label == "100.00", "线性标注数字为 100.00", g.label);

        // 半径：标签以 R 开头
        Element rad; rad.type = ElementType::Dimension; rad.dtype = DimensionType::Radius;
        rad.pts = {Vec3(0,0,0), Vec3(25,0,0)}; rad.radius = 25;
        dim::Geom gr = dim::build(rad);
        check(gr.valid && gr.label.startsWith(QLatin1String("R")), "半径标注标签以 R 开头", gr.label);
        check(gr.arrows.size() == 1, "半径标注 1 个箭头", QString::number(gr.arrows.size()));

        // 直径：标签以 ⌀ 开头
        Element dia; dia.type = ElementType::Dimension; dia.dtype = DimensionType::Diameter;
        dia.pts = {Vec3(0,0,0), Vec3(25,0,0)}; dia.radius = 25;
        dim::Geom gd = dim::build(dia);
        check(gd.valid && gd.label.startsWith(dim::diameterSign()), "直径标注标签以 ⌀ 开头", gd.label);
        check(gd.arrows.size() == 2, "直径标注 2 个箭头（通过圆心）", QString::number(gd.arrows.size()));

        // 角度：标签以 ° 结尾，且生成圆弧尺寸线
        Element ang; ang.type = ElementType::Dimension; ang.dtype = DimensionType::Angle;
        ang.pts = {Vec3(0,0,0), Vec3(100,0,0), Vec3(0,100,0)};
        dim::Geom ga = dim::build(ang);
        check(ga.valid && ga.label.endsWith(QStringLiteral("°")) && qFuzzyCompare(ga.measured, 90.0),
              "角度标注=90° 且标签带 °", ga.label);
        check(!ga.arcPoly.empty(), "角度标注生成圆弧尺寸线", QString::number(ga.arcPoly.size()));

        // 对齐：量真实距离 5
        Element ali; ali.type = ElementType::Dimension; ali.dtype = DimensionType::Aligned;
        ali.pts = {Vec3(0,0,0), Vec3(3,4,0)};
        dim::Geom gal = dim::build(ali);
        check(gal.valid && qFuzzyCompare(gal.measured, 5.0), "对齐标注实测=5");

        // 【回归·直线长度标注修复】非轴对齐直线 (0,0)-(3,4)：
        //   Distance(线性自动) 退化成坐标轴投影 = max(|Δx|,|Δy|) = 4.0（旧错误行为）；
        //   Aligned 量真实长度 = 5.0（修复后点选直线应采用的量法）。
        // 通过 geo::lineDimType 把「默认线性/对齐/半径直径等」统一归一为 Aligned，
        // 仅显式水平/竖直保留投影；实测值因此应为 5.0 而非 4.0。
        {
            const Vec3 P(0, 0, 0), Q(3, 4, 0);
            Element dist; dist.type = ElementType::Dimension; dist.dtype = DimensionType::Distance; dist.pts = {P, Q};
            check(qFuzzyCompare(geo::measureDimension(dist), 4.0), "非轴对齐直线：Distance(投影)=4（被修复的旧行为）");
            check(geo::lineDimType(DimensionType::Distance)   == DimensionType::Aligned, "lineDimType(默认线性)→Aligned");
            check(geo::lineDimType(DimensionType::Aligned)    == DimensionType::Aligned, "lineDimType(对齐)→Aligned");
            check(geo::lineDimType(DimensionType::Horizontal) == DimensionType::Horizontal, "lineDimType(显式水平)→保持水平");
            check(geo::lineDimType(DimensionType::Vertical)   == DimensionType::Vertical,   "lineDimType(显式竖直)→保持竖直");

            Element fixed; fixed.type = ElementType::Dimension;
            fixed.dtype = geo::lineDimType(DimensionType::Distance);   // 模拟点选直线后的归一
            fixed.pts = {P, Q};
            dim::Geom gfx = dim::build(fixed);
            check(gfx.valid && qFuzzyCompare(gfx.measured, 5.0) && gfx.label == "5.00",
                  "直线标注修复后实测=5.00（真实长度）", gfx.label);
        }

        // 国标要素数值（细实线、实心箭头、字高）落在合理区间
        dim::Style st;
        check(qFuzzyCompare(st.arrowLen, 3.5) && qFuzzyCompare(st.textHeight, 3.5),
              "国标要素：箭头长/字高=3.5mm", QString("%1/%2").arg(st.arrowLen).arg(st.textHeight));
        check(st.extBeyond >= 2.0 && st.extBeyond <= 3.0, "尺寸界线超出 2~3mm", QString::number(st.extBeyond));
    }

    qInfo().noquote() << "\n=== 15b. 标注标准切换（GB/T 4458 ↔ ISO 129）===";
    {
        // 默认配置：未指定标准时默认国标；ISO 有独立的箭头/文字/公差默认
        const auto gbt = DimensionStandardConfig::gbtDefault();
        const auto iso = DimensionStandardConfig::isoDefault();
        check(gbt.standard == DimensionStandard::GBT && gbt.arrowForm == ArrowForm::ClosedFilled,
              "GB/T 默认：实心闭合箭头");
        check(gbt.textOrientation == TextOrientation::AlignedReadable, "GB/T 默认：对齐可读文字(方法1)");
        check(gbt.toleranceStyle == ToleranceStyle::None, "GB/T 默认：无公差");
        check(iso.standard == DimensionStandard::ISO && iso.arrowForm == ArrowForm::ClosedUnfilled,
              "ISO 默认：空心闭合箭头");
        check(iso.textOrientation == TextOrientation::AlignedISO, "ISO 默认：ISO 对齐文字");
        check(iso.toleranceStyle == ToleranceStyle::DeviationStacked, "ISO 默认：极限偏差");
        check(DimensionStandardConfig::forStandard(DimensionStandard::GBT).arrowForm == ArrowForm::ClosedFilled,
              "forStandard(GBT)→实心闭合");
        check(DimensionStandardConfig::forStandard(DimensionStandard::ISO).arrowForm == ArrowForm::ClosedUnfilled,
              "forStandard(ISO)→空心闭合");

        auto mkLin = [](DimensionStandard s, double up = 0, double lo = 0) {
            Element e; e.type = ElementType::Dimension; e.dtype = DimensionType::Distance;
            e.standard = s; e.pts = {Vec3(0,0,0), Vec3(100,0,0)};
            e.tolUpper = up; e.tolLower = lo; return e;
        };
        // 公差注写：GB/T 默认无公差 -> 空；ISO 默认极限偏差 -> 写入上/下偏差
        dim::Style sgbt; sgbt.stdConf = DimensionStandardConfig::gbtDefault();
        dim::Geom gGbt = dim::build(mkLin(DimensionStandard::GBT, 0.1, 0.05), sgbt);
        check(gGbt.tolLabel.isEmpty(), "GB/T 默认无公差注写（tolLabel 为空）", gGbt.tolLabel);
        dim::Style siso; siso.stdConf = DimensionStandardConfig::isoDefault();
        dim::Geom gIso = dim::build(mkLin(DimensionStandard::ISO, 0.10, 0.05), siso);
        check(!gIso.tolLabel.isEmpty(), "ISO 默认极限偏差注写非空", gIso.tolLabel);
        check(gIso.tolLabel.contains("0.10") && gIso.tolLabel.contains("0.05"),
              "ISO 极限偏差同时含上/下偏差(+0.10/-0.05)", gIso.tolLabel);

        // 文字方向：近竖直的尺寸线，GB/T 方法1 夹取到 ±90°（水平可读），ISO 对齐到线角
        {
            Element v; v.type = ElementType::Dimension; v.dtype = DimensionType::Aligned;
            v.pts = {Vec3(0,0,0), Vec3(3,100,0), Vec3(10,50,0)};
            dim::Style sg, si;
            sg.stdConf = DimensionStandardConfig::gbtDefault();
            si.stdConf = DimensionStandardConfig::isoDefault();
            const double rg = dim::build(v, sg).textRotDeg;
            const double ri = dim::build(v, si).textRotDeg;
            check(qFuzzyCompare(rg, 90.0), "GB/T 近竖直尺寸文字夹取为 90°(水平可读)", QString::number(rg));
            check(!qFuzzyCompare(ri, 90.0), "ISO 文字对齐尺寸线(非90°)", QString::number(ri));
        }

        // 各新增标注类型均可 build 出有效几何
        Element base; base.type = ElementType::Dimension; base.dtype = DimensionType::Baseline;
        base.pts = {Vec3(0,0,0), Vec3(100,0,0)};
        check(dim::build(base).valid, "基线标注 build 有效");
        Element cont; cont.type = ElementType::Dimension; cont.dtype = DimensionType::Continue;
        cont.pts = {Vec3(0,0,0), Vec3(100,0,0)};
        check(dim::build(cont).valid, "连续标注 build 有效");
        Element arc; arc.type = ElementType::Dimension; arc.dtype = DimensionType::Arc;
        arc.pts = {Vec3(0,0,0)}; arc.radius = 30; arc.startAngle = 0; arc.endAngle = 60;
        dim::Geom ga = dim::build(arc);
        check(ga.valid && ga.label.startsWith(QStringLiteral("⌒")), "弧长标注 build 有效且带 ⌒ 前缀", ga.label);
        Element lead; lead.type = ElementType::Dimension; lead.dtype = DimensionType::Leader;
        lead.pts = {Vec3(0,0,0), Vec3(20,10,0)}; lead.text = QStringLiteral("技术要求");
        check(dim::build(lead).valid, "引线标注 build 有效");
        Element ord; ord.type = ElementType::Dimension; ord.dtype = DimensionType::Ordinate;
        ord.pts = {Vec3(50,40,0), Vec3(0,0,0)};
        check(dim::build(ord).valid, "坐标标注 build 有效");
    }

    qInfo().noquote() << "\n=== 15c. 模型级标注标准切换（重戳已有标注）===";
    {
        SketchModel mm;
        Element ln; ln.type = ElementType::Dimension; ln.dtype = DimensionType::Distance;
        ln.pts = {Vec3(0,0,0), Vec3(100,0,0)};
        const int id = mm.addElement(ln);
        check(mm.dimensionStandard() == DimensionStandard::GBT, "模型默认标准=GB/T 4458");
        mm.setDimensionStandard(DimensionStandard::ISO);
        const Element* d = mm.element(id);
        check(d && d->standard == DimensionStandard::ISO, "切换标准后已有标注被重戳为 ISO");

        // 单标注标准覆盖：只改一个标注的 standard，模型默认不变
        mm.setDimensionStandard(DimensionStandard::GBT);
        Element onlyIso; onlyIso.type = ElementType::Dimension; onlyIso.dtype = DimensionType::Distance;
        onlyIso.pts = {Vec3(0,0,0), Vec3(50,0,0)};
        onlyIso.standard = DimensionStandard::ISO;
        const int oid = mm.addElement(onlyIso);
        mm.setDimensionStandard(DimensionStandard::ISO);   // 再次全局切换
        // 因 setDimensionStandard 会重戳所有已有标注，这里验证：
        const Element* e1 = mm.element(id);
        const Element* e2 = mm.element(oid);
        check(e1 && e1->standard == DimensionStandard::ISO, "全局切换→原 GBT 标注→ISO");
        check(e2 && e2->standard == DimensionStandard::ISO, "全局切换→原 ISO 标注仍为 ISO");

        // 公差字段持久化：写入后 build 应反映出上下偏差
        Element tolE; tolE.type = ElementType::Dimension; tolE.dtype = DimensionType::Distance;
        tolE.pts = {Vec3(0,0,0), Vec3(60,0,0)};
        tolE.standard = DimensionStandard::ISO;
        tolE.tolUpper = 0.20; tolE.tolLower = -0.10;
        tolE.fitCode  = QStringLiteral("H7");
        dim::Style stol; stol.stdConf = DimensionStandardConfig::isoDefault();
        dim::Geom gt = dim::build(tolE, stol);
        check(gt.tolLabel.contains("0.20") && gt.tolLabel.contains("0.10"),
              "ISO 公差同时含上偏差0.20与下偏差绝对值0.10", gt.tolLabel);

        // 配合代号 fitCode 用于 LimitSize / FitCode 注写场景的端到端串联
        Element fitE; fitE.type = ElementType::Dimension; fitE.dtype = DimensionType::Distance;
        fitE.pts = {Vec3(0,0,0), Vec3(80,0,0)};
        fitE.fitCode = QStringLiteral("H7/g6");
        dim::Style sf; sf.stdConf = DimensionStandardConfig::isoDefault();
        sf.stdConf.toleranceStyle = ToleranceStyle::FitCode;
        dim::Geom gf = dim::build(fitE, sf);
        check(gf.tolLabel.contains(QStringLiteral("H7/g6")), "配合代号 H7/g6 正确写入 tolLabel", gf.tolLabel);
    }

    qInfo().noquote() << "\n=== 15d. 新增标注类型（基线/连续/弧长/引线/坐标）工具注册与 CLI 闭环 ===";
    {
        const ToolRegistry& reg = ToolRegistry::instance();
        struct { const char* id; DimensionType want; const char* name; } cases[] = {
            {"dim_baseline",  DimensionType::Baseline,  "基线标注"},
            {"dim_continue",  DimensionType::Continue,  "连续标注"},
            {"dim_arc",       DimensionType::Arc,       "弧长标注"},
            {"dim_leader",    DimensionType::Leader,    "引线标注"},
            {"dim_ordinate",  DimensionType::Ordinate,  "坐标标注"},
        };
        for (const auto& c : cases) {
            DimensionType got = DimensionType::Distance;
            const bool ok = reg.tryDimensionType(QString::fromUtf8(c.id), got);
            check(ok && got == c.want,
                  QString("工具注册 %1 → %2").arg(QString::fromUtf8(c.id), QString::fromUtf8(c.name)));
            check(reg.isDimensionTool(QString::fromUtf8(c.id)),
                  QString("%1 归入标注工具分类").arg(QString::fromUtf8(c.name)));
        }

        // 命令行闭环：dim <类型> 能真正建出对应 dtype 的标注
        SketchModel md;
        CommandInterpreter cli(&md);
        check(cli.execute("dim baseline 0,0 40,0"),   "CLI dim baseline 建基线标注");
        check(cli.execute("dim continue 40,0 90,0"),  "CLI dim continue 建连续标注");
        check(cli.execute("dim leader 0,0 20,20 40,20"), "CLI dim leader 建引线标注");
        check(cli.execute("dim ordinate 0,0 30,0"),   "CLI dim ordinate 建坐标标注");

        const Element* eb = md.element(1);
        const Element* ec = md.element(2);
        const Element* el = md.element(3);
        const Element* eo = md.element(4);
        check(eb && eb->type == ElementType::Dimension && eb->dtype == DimensionType::Baseline,
              "1# 为基线标注");
        check(ec && ec->type == ElementType::Dimension && ec->dtype == DimensionType::Continue,
              "2# 为连续标注");
        check(el && el->type == ElementType::Dimension && el->dtype == DimensionType::Leader,
              "3# 为引线标注");
        check(eo && eo->type == ElementType::Dimension && eo->dtype == DimensionType::Ordinate,
              "4# 为坐标标注");

        // 实测值：基线/连续按线段长度；坐标按 Δx（rotation=0 → X 方向）
        if (eb) check(qFuzzyCompare(geo::measureDimension(*eb), 40.0), "基线标注实测=40");
        if (ec) check(qFuzzyCompare(geo::measureDimension(*ec), 50.0), "连续标注实测=50");
        if (eo) check(qFuzzyCompare(geo::measureDimension(*eo), 30.0), "坐标标注实测 Δx=30");

        // 新建标注自动盖上当前标准（默认 GB/T）
        check(eb && eb->standard == md.dimensionStandard(), "新建标注继承模型当前标准");

        // 弧长标注：命令行以「引用圆/圆弧 id [起角 终角]」给出
        SketchModel ma;
        CommandInterpreter cli2(&ma);
        check(cli2.execute("circle 0,0 50"), "CLI 建 r=50 的圆 #1（供弧长标注引用）");
        check(cli2.execute("dim arc 1 0 90"), "CLI dim arc 建弧长标注");
        const Element* ea = ma.element(2);
        check(ea && ea->dtype == DimensionType::Arc, "弧长标注 dtype=Arc");
        if (ea) {
            dim::Style sa; sa.stdConf = DimensionStandardConfig::forStandard(ea->standard);
            const dim::Geom ga = dim::build(*ea, sa);
            check(ga.valid, "弧长标注几何有效");
            check(ga.label.contains(QString::fromUtf8("⌒")), "弧长标签带 ⌒ 前缀", ga.label);
            check(qFuzzyCompare(ga.measured, 50.0 * M_PI / 2.0), "弧长实测 = 四分之一圆周",
                  QString::number(ga.measured));
        }
    }

    qInfo().noquote() << "\n=== 16. 剪切引擎 trim::trimElement（需求4）===";
    {
        // 直线 + 一条竖直边界：点击左段 → 保留右段 (40,0)-(100,0)
        // 注意：target 与 cutter 必须 id 不同，否则 gatherCuts 会跳过（同 id 视为自身）
        Element line; line.id = 1; line.type = ElementType::Line; line.pts = {Vec3(0,0,0), Vec3(100,0,0)};
        line.lineWidth = 3.0; line.color = QColor(Qt::red);
        Element vcut; vcut.id = 2; vcut.type = ElementType::Line; vcut.pts = {Vec3(40,-10,0), Vec3(40,10,0)};
        std::vector<Element> parts; QString msg;
        bool ok = trim::trimElement(line, {vcut}, Vec3(20,0,0), parts, msg);
        check(ok && parts.size() == 1, "直线被一条边界剪切为 1 段", msg + QString(" n=%1").arg(parts.size()));
        if (!parts.empty()) {
            check(qFuzzyCompare(parts[0].pts[0].x, 40.0) && qFuzzyCompare(parts[0].pts[1].x, 100.0),
                  "剩余直线为 (40,0)-(100,0)");
            check(qFuzzyCompare(parts[0].lineWidth, 3.0) && parts[0].color == QColor(Qt::red),
                  "剪切保留原线条样式(线宽/颜色)");
        }

        // 圆 + 一条弦：切成 1 段圆弧，继承半径
        Element circ; circ.id = 1; circ.type = ElementType::Circle; circ.pts = {Vec3(0,0,0)}; circ.radius = 25;
        Element chord; chord.id = 2; chord.type = ElementType::Line; chord.pts = {Vec3(-20,15,0), Vec3(20,15,0)};
        std::vector<Element> cp; QString m2;
        check(trim::trimElement(circ, {chord}, Vec3(0,25,0), cp, m2) &&
              cp.size() == 1 && cp[0].type == ElementType::Arc,
              "圆被一条弦切成 1 段圆弧", m2);
        if (!cp.empty()) check(qFuzzyCompare(cp[0].radius, 25.0), "剪切弧继承半径=25");

        // 圆只有 1 个切点 → 无法构成一段弧 → 拒绝
        Element tang; tang.id = 3; tang.type = ElementType::Line; tang.pts = {Vec3(25,-10,0), Vec3(25,10,0)}; // x=25 与 r=25 圆相切
        std::vector<Element> cp3; QString m3;
        check(!trim::trimElement(circ, {tang}, Vec3(0,25,0), cp3, m3), "圆仅 1 个切点 → 拒绝剪切", m3);

        // 闭合矩形被剪切后降级为开放折线（多边形）
        Element rect; rect.id = 1; rect.type = ElementType::Rectangle; rect.pts = {Vec3(0,0,0), Vec3(100,100,0)}; rect.closed = true;
        Element v1; v1.id = 2; v1.type = ElementType::Line; v1.pts = {Vec3(50,-10,0), Vec3(50,110,0)};
        std::vector<Element> rp; QString m4;
        check(trim::trimElement(rect, {v1}, Vec3(25,0,0), rp, m4) && rp.size() == 1,
              "矩形被剪切为 1 段", m4 + QString(" n=%1").arg(rp.size()));
        if (!rp.empty()) {
            check(rp[0].type == ElementType::Polygon, "矩形剪切后降级为多边形");
            check(rp[0].closed == false, "剪切后变为开放折线(closed=false)");
        }

        // 弧：用两个落在弧内的边界点切出段（截 30° 与 60° 处）
        Element arc; arc.id = 1; arc.type = ElementType::Arc; arc.pts = {Vec3(0,0,0)}; arc.radius = 25;
        arc.startAngle = 0; arc.endAngle = 90;     // 第一象限 1/4 弧
        const double c30 = 25.0 * std::cos(30.0 * M_PI / 180.0);   // ≈21.65
        const double s60 = 25.0 * std::sin(60.0 * M_PI / 180.0);   // ≈21.65
        Element b1; b1.id = 2; b1.type = ElementType::Line; b1.pts = {Vec3(c30,-50,0), Vec3(c30,50,0)}; // 过 30° 点
        Element b2; b2.id = 3; b2.type = ElementType::Line; b2.pts = {Vec3(-50,s60,0), Vec3(50,s60,0)}; // 过 60° 点
        std::vector<Element> ap; QString m5;
        check(trim::trimElement(arc, {b1, b2}, Vec3(18,18,0), ap, m5) && !ap.empty(),
              "圆弧被两段边界切出剩余弧", m5 + QString(" n=%1").arg(ap.size()));
        if (!ap.empty()) check(ap[0].type == ElementType::Arc, "圆弧剪切产物仍为圆弧");
    }

    qInfo().noquote() << "\n=== 17. 端点联动拖拽（handleAt/collectLinked/moveHandles/replaceElement，需求3）===";
    {
        SketchModel lm;
        auto mkLine = [](Vec3 a, Vec3 b) { Element e; e.type = ElementType::Line; e.pts = {a, b}; return e; };
        int id1 = lm.addElement(mkLine(Vec3(0,0,0),   Vec3(100,0,0)));   // 起点(0,0) 终点(100,0)
        int id2 = lm.addElement(mkLine(Vec3(100,0,0), Vec3(100,50,0)));  // 起点(100,0) 与 L1 终点相连
        int id3 = lm.addElement(mkLine(Vec3(0,0,0),   Vec3(0,50,0)));    // 起点(0,0) 与 L1 起点相连
        Element circ; circ.type = ElementType::Circle; circ.pts = {Vec3(0,0,0)}; circ.radius = 10;
        int idc = lm.addElement(circ);                                   // 圆心(0,0) 与 L1/L3 起点重合

        // handleAt：在 (100,0) 处应抓到某个端点
        SketchModel::HandleRef h = lm.handleAt(Vec3(100,0,0), 1e-6);
        check(h.valid(), "handleAt 在 (100,0) 抓到端点", QString("id=%1 idx=%2").arg(h.elemId).arg(h.index));

        // 联动：拖 (100,0) 应同时带动 L1 终点 与 L2 起点（2 个端点），不含圆心
        auto refs = lm.collectLinked(Vec3(100,0,0), 1e-6);
        check(refs.size() == 2, "(100,0) 仅关联 2 个直线端点", QString::number(refs.size()));
        int moved = lm.moveHandles(refs, Vec3(120,10,0));
        check(moved == 2, "moveHandles 实际改动 2 个图元", QString::number(moved));
        const Element* p1 = lm.element(id1);
        const Element* p2 = lm.element(id2);
        const Element* p3 = lm.element(id3);
        check(p1 && qFuzzyCompare(p1->pts[1].x, 120.0) && qFuzzyCompare(p1->pts[1].y, 10.0),
              "L1 终点跟随到 (120,10)");
        check(p2 && qFuzzyCompare(p2->pts[0].x, 120.0) && qFuzzyCompare(p2->pts[0].y, 10.0),
              "L2 起点跟随到 (120,10)（拓扑保持）");
        check(p3 && qFuzzyCompare(p3->pts[0].x, 0.0) && qFuzzyCompare(p3->pts[0].y, 0.0),
              "L3 起点(0,0) 不受影响");

        // 圆心排除：在 (0,0) 的联动不得包含圆心
        auto refs0 = lm.collectLinked(Vec3(0,0,0), 1e-6);
        bool hasCircle = false;
        for (const auto& r : refs0) if (r.elemId == idc) hasCircle = true;
        check(!hasCircle && refs0.size() == 2, "(0,0) 联动排除圆心，仅 2 个直线端点",
              QString::number(refs0.size()));

        // replaceElement（剪切内部用的删1加N，只压一次撤销快照）
        int before = lm.elementCount();
        auto ids = lm.replaceElement(id1, {mkLine(Vec3(0,0,0), Vec3(40,0,0)),
                                           mkLine(Vec3(40,0,0), Vec3(100,0,0))}, QStringLiteral("拆分"));
        check((int)ids.size() == 2 && lm.elementCount() == before + 1,
              "replaceElement 删1加2，总数+1", QString::number(lm.elementCount()));
        lm.undo();
        check(lm.elementCount() == before, "undo replaceElement 整体回退（单次快照）",
              QString::number(lm.elementCount()));
    }

    qInfo().noquote() << "\n=== 18. 多选集合 API 与选区智能标注（需求补充）===";
    {
        SketchModel msel;
        int la = msel.addElement([]{ Element e; e.type = ElementType::Line; e.pts = {Vec3(0,0,0), Vec3(100,0,0)}; return e; }());
        int lb = msel.addElement([]{ Element e; e.type = ElementType::Line; e.pts = {Vec3(0,10,0), Vec3(100,10,0)}; return e; }());
        int lc = msel.addElement([]{ Element e; e.type = ElementType::Line; e.pts = {Vec3(0,20,0), Vec3(100,20,0)}; return e; }());

        // 多选集合操作
        msel.setSelection({la, lb});
        check(msel.hasSelection() && msel.selection().size() == 2, "setSelection 选中 2 个图元", QString::number(msel.selection().size()));
        check(msel.isSelected(la) && !msel.isSelected(lc), "isSelected 反映选择集");
        msel.addToSelection(lc);
        check(msel.selection().size() == 3, "addToSelection 追加到 3 个", QString::number(msel.selection().size()));
        msel.toggleSelection(lb);
        check(msel.selection().size() == 2 && !msel.isSelected(lb), "toggleSelection 减选 lb（剩 2 个）", QString::number(msel.selection().size()));
        msel.clearSelection();
        check(!msel.hasSelection() && msel.selectedId() < 0, "clearSelection 清空选择集");

        // 选区智能标注：单条直线 -> 对齐标注（沿垂直方向引出，量真实长度）
        msel.setSelected(la);
        int dl = msel.smartDimension();
        check(dl >= 0, "选中直线 -> 智能创建标注", QString::number(dl));
        if (const Element* d = msel.element(dl)) {
            check(d->dtype == DimensionType::Aligned, "直线智能标注为「对齐标注」");
            const dim::Geom g = dim::build(*d);
            check(g.valid && qFuzzyCompare(g.measured, 100.0), "对齐标注测得直线长度 100mm", QString::number(g.measured));
            check(d->pts.size() == 3, "对齐标注含测量点A/B与尺寸线位置P");
        }

        // 选区智能标注：单个圆 -> 直径标注（⌀ 前缀）
        int cc = msel.addElement([]{ Element e; e.type = ElementType::Circle; e.pts = {Vec3(0,0,0)}; e.radius = 25; return e; }());
        msel.setSelected(cc);
        int dc = msel.smartDimension();
        check(dc >= 0, "选中圆 -> 智能创建直径标注", QString::number(dc));
        if (const Element* d = msel.element(dc)) {
            check(d->dtype == DimensionType::Diameter, "圆智能标注为「直径标注」");
            const dim::Geom g = dim::build(*d);
            check(g.valid && qFuzzyCompare(g.measured, 50.0), "直径标注测得 2R=50mm", QString::number(g.measured));
            check(!g.label.isEmpty() && g.label.startsWith(QStringLiteral("⌀")), "直径标注带 ⌀ 前缀", g.label);
        }

        // 无选区 / 多选区 / 不支持图元 -> 返回 -1，交由常规拾取
        msel.clearSelection();
        check(msel.smartDimension() < 0, "无选区时 smartDimension 返回 -1");
        msel.setSelection({la, lb});
        check(msel.smartDimension() < 0, "多选区时 smartDimension 返回 -1（需精确定位）");
        msel.setSelected(cc);
        // 圆存在则仍为直径；这里只验证「选中非直线/非圆」的情形（用矩形）
        int rectId = msel.addElement([]{ Element e; e.type = ElementType::Rectangle; e.pts = {Vec3(0,0,0), Vec3(30,30,0)}; return e; }());
        msel.setSelected(rectId);
        check(msel.smartDimension() < 0, "选中矩形时 smartDimension 返回 -1（无对应智能标注，走常规拾取）");
    }

    // =====================================================================
    qInfo().noquote() << QString("\n=== 19. 直线绘制智能捕捉（端点/圆心/交点/切点）===");
    {
        using SketchCAD::snap::SnapResult;
        using SketchCAD::snap::SnapType;
        using SketchCAD::snap::tangentPointsFromPointToCircle;
        SnapResult (*snapFn)(const std::vector<Element>&, const Vec3&, const Vec3*, double) = SketchCAD::snap::snap;

        auto mkLine = [](int id, Vec3 a, Vec3 b) {
            Element e; e.id = id; e.type = ElementType::Line; e.pts = {a, b}; e.visible = true; return e;
        };
        auto mkCircle = [](int id, Vec3 c, double r) {
            Element e; e.id = id; e.type = ElementType::Circle; e.pts = {c}; e.radius = r; e.visible = true; return e;
        };
        auto mkArc = [](int id, Vec3 c, double r, double s, double e1) {
            Element e; e.id = id; e.type = ElementType::Arc; e.pts = {c}; e.radius = r;
            e.startAngle = s; e.endAngle = e1; e.visible = true; return e;
        };

        // 19.1 由外部点向圆作切线：数量、切点落在圆上、PT ⟂ CT
        {
            Vec3 out[2];
            const int n = tangentPointsFromPointToCircle(Vec3(0, 50, 0), Vec3(0, 0, 0), 25.0, out);
            check(n == 2, "外部点→圆 返回 2 个切点", QString("n=%1").arg(n));
            for (int k = 0; k < n; ++k) {
                const double onC = std::fabs(out[k].distTo(Vec3(0,0,0)) - 25.0);
                const Vec3 CT = Vec3(out[k].x, out[k].y, 0);                 // 圆心→切点
                const Vec3 PT = Vec3(out[k].x - 0, out[k].y - 50, 0);        // 外部点→切点
                const double dot = CT.x * PT.x + CT.y * PT.y;                // 应≈0（垂直）
                check(onC < 1e-6, QString("切点%1 落在圆上(r=25)").arg(k), QString("err=%1").arg(onC));
                check(std::fabs(dot) < 1e-6, QString("切点%1 处 PT⟂CT（切线）").arg(k), QString("dot=%1").arg(dot));
            }
        }
        // 点在圆内 → 无切线
        {
            Vec3 tmp[2];
            check(tangentPointsFromPointToCircle(Vec3(5, 0, 0), Vec3(0, 0, 0), 25.0, tmp) == 0,
                  "圆内点 → 无实切线（返回 0）");
        }

        // 19.2 圆心捕捉：光标靠近圆 (100,100) r=40 的圆心
        {
            std::vector<Element> es{ mkCircle(1, Vec3(100, 100, 0), 40.0) };
            SnapResult r = snapFn(es, Vec3(103, 98, 0), nullptr, 6.0);
            check(r.snapped && r.type == SnapType::Center && qFuzzyCompare(r.point.x, 100.0) && qFuzzyCompare(r.point.y, 100.0),
                  "光标贴近圆心 → 捕捉为 Center 且坐标精确为 (100,100)",
                  QString("type=%1 pt=(%2,%3)").arg((int)r.type).arg(r.point.x).arg(r.point.y));
        }

        // 19.3 直线端点捕捉
        {
            std::vector<Element> es{ mkLine(2, Vec3(0, 0, 0), Vec3(80, 30, 0)) };
            SnapResult r = snapFn(es, Vec3(2, -1, 0), nullptr, 6.0);
            check(r.snapped && r.type == SnapType::Endpoint && qFuzzyCompare(r.point.x, 0.0) && qFuzzyCompare(r.point.y, 0.0),
                  "光标贴近直线起点 → 捕捉为 Endpoint 且坐标精确为 (0,0)",
                  QString("type=%1 pt=(%2,%3)").arg((int)r.type).arg(r.point.x).arg(r.point.y));
        }

        // 19.4 弧端点捕捉（起点/终点，而非圆心）
        {
            std::vector<Element> es{ mkArc(3, Vec3(0, 0, 0), 50.0, 0.0, 90.0) }; // 第一象限 1/4 弧
            const Vec3 start(50, 0, 0), end(0, 50, 0);
            SnapResult rs = snapFn(es, Vec3(52, 1, 0), nullptr, 6.0);   // 贴近起点
            SnapResult re = snapFn(es, Vec3(-1, 52, 0), nullptr, 6.0);  // 贴近终点
            check(rs.snapped && rs.type == SnapType::Endpoint && rs.point.distTo(start) < 1e-6, "弧起点捕捉为 Endpoint 且精确");
            check(re.snapped && re.type == SnapType::Endpoint && re.point.distTo(end)   < 1e-6, "弧终点捕捉为 Endpoint 且精确");
        }

        // 19.5 两直线交点捕捉（交点不与任一端点重合，避免与端点捕捉竞争）
        {
            std::vector<Element> es{ mkLine(10, Vec3(-100, 0, 0), Vec3(100, 0, 0)),  // X 轴，端点远离原点
                                     mkLine(11, Vec3(0, -50, 0), Vec3(0, 50, 0)) };   // Y 轴
            SnapResult r = snapFn(es, Vec3(3, -2, 0), nullptr, 6.0);                  // 接近原点（交点）
            check(r.snapped && r.type == SnapType::Intersection &&
                  qFuzzyCompare(r.point.x, 0.0) && qFuzzyCompare(r.point.y, 0.0) &&
                  r.elemA == 10 && r.elemB == 11,
                  "两直线交点 → 捕捉为 Intersection 且坐标为 (0,0)，记录两图元 id",
                  QString("type=%1 pt=(%2,%3) a=%4 b=%5").arg((int)r.type).arg(r.point.x).arg(r.point.y).arg(r.elemA).arg(r.elemB));
        }

        // 19.6 切点捕捉：已知首端点 P，第二点拖近某圆时吸附到「P 对该圆的切点」
        {
            // 圆 (0,0) r=25；首端点 P=(40,0)。由 P 向圆作切线：几何上切点使 PT⟂CT。
            // 此处只验证「在容差内、type=Tangent、切点落在圆上」三件事。
            std::vector<Element> es{ mkCircle(20, Vec3(0, 0, 0), 25.0) };
            const Vec3 P(40, 0, 0);
            // 先算理论切点之一（与 y>0 一侧），再把光标放到该切点附近，触发 Tangent 捕捉
            Vec3 tp[2]; int n = tangentPointsFromPointToCircle(P, Vec3(0,0,0), 25.0, tp);
            check(n >= 1, "切点捕捉前提：首端点能向圆作切线", QString("n=%1").arg(n));
            const Vec3 cur(tp[0].x + 1.0, tp[0].y + 1.0, 0);   // 光标略偏离切点，仍在容差内
            SnapResult r = snapFn(es, cur, &P, 6.0);
            check(r.snapped && r.type == SnapType::Tangent && r.point.distTo(tp[0]) < 1e-6 &&
                  std::fabs(r.point.distTo(Vec3(0,0,0)) - 25.0) < 1e-6,
                  "提供首端点 P 时，光标近圆 → 捕捉为 Tangent 且切点精确落在圆上",
                  QString("type=%1 pt=(%2,%3)").arg((int)r.type).arg(r.point.x).arg(r.point.y));
        }

        // 19.7 无捕捉：远离任何特征点时返回自由点（snapped=false，point==w）
        {
            std::vector<Element> es{ mkLine(30, Vec3(0, 0, 0), Vec3(10, 0, 0)) };
            const Vec3 w(500, 500, 0);
            SnapResult r = snapFn(es, w, nullptr, 6.0);
            check(!r.snapped && r.point.distTo(w) < 1e-9, "远离特征点 → 不捕捉，返回原自由点");
        }
    }

    // =====================================================================
    qInfo().noquote() << QString("\n=== 20. 捕捉开关（SnapConfig）门控与中点捕捉 ===");
    {
        using SketchCAD::snap::SnapResult;
        using SketchCAD::snap::SnapType;
        using SketchCAD::snap::SnapConfig;
        SnapResult (*snapCfg)(const std::vector<Element>&, const Vec3&, const Vec3*, double, const SnapConfig&) = SketchCAD::snap::snap;
        using SketchCAD::snap::tangentPointsFromPointToCircle;

        auto mkLine   = [](int id, Vec3 a, Vec3 b) { Element e; e.id=id; e.type=ElementType::Line;   e.pts={a,b};     e.visible=true; return e; };
        auto mkCircle = [](int id, Vec3 c, double r){ Element e; e.id=id; e.type=ElementType::Circle; e.pts={c}; e.radius=r; e.visible=true; return e; };

        // 20.1 SnapConfig 默认值：端点/圆心/交点/切线开，中点关（与历史行为一致）
        {
            SnapConfig d;
            check(d.endpoint && d.center && d.intersection && d.tangent && !d.midpoint,
                  "SnapConfig 默认：端点/圆心/交点/切线开，中点关");
        }

        // 20.2 中点捕捉开启：光标正好落在直线段中点 → Midpoint 且坐标精确
        {
            std::vector<Element> es{ mkLine(1, Vec3(0,0,0), Vec3(10,0,0)) }; // 中点 (5,0)
            SnapConfig cfg; cfg.endpoint=false; cfg.center=false; cfg.intersection=false; cfg.tangent=false; cfg.midpoint=true;
            SnapResult r = snapCfg(es, Vec3(5,0,0), nullptr, 1.0, cfg);
            check(r.snapped && r.type == SnapType::Midpoint &&
                  qFuzzyCompare(r.point.x,5.0) && qFuzzyCompare(r.point.y,0.0),
                  "中点开启：光标置于线段中点 → 捕捉为 Midpoint 且精确 (5,0)",
                  QString("type=%1 pt=(%2,%3)").arg((int)r.type).arg(r.point.x).arg(r.point.y));
        }

        // 20.3 中点关闭：同样光标 → 无候选（snapped=false），验证门控生效
        {
            std::vector<Element> es{ mkLine(2, Vec3(0,0,0), Vec3(10,0,0)) };
            SnapConfig cfg; cfg.endpoint=false; cfg.center=false; cfg.intersection=false; cfg.tangent=false; cfg.midpoint=false;
            SnapResult r = snapCfg(es, Vec3(5,0,0), nullptr, 1.0, cfg);
            check(!r.snapped, "中点关闭：同位置无捕捉（门控生效）");
        }

        // 20.4 端点门控：开启→捕捉端点，关闭→同光标不捕捉
        {
            std::vector<Element> es{ mkLine(3, Vec3(0,0,0), Vec3(80,30,0)) };
            SnapConfig on;  on.center=false; on.intersection=false; on.tangent=false; on.midpoint=false; on.endpoint=true;
            SnapConfig off; off.center=false; off.intersection=false; off.tangent=false; off.midpoint=false; off.endpoint=false;
            SnapResult r1 = snapCfg(es, Vec3(2,-1,0), nullptr, 6.0, on);
            SnapResult r2 = snapCfg(es, Vec3(2,-1,0), nullptr, 6.0, off);
            check(r1.snapped && r1.type == SnapType::Endpoint, "端点开启 → 捕捉为 Endpoint");
            check(!r2.snapped, "端点关闭 → 同光标不捕捉（门控生效）");
        }

        // 20.5 圆心门控
        {
            std::vector<Element> es{ mkCircle(4, Vec3(100,100,0), 40.0) };
            SnapConfig on;  on.endpoint=false; on.intersection=false; on.tangent=false; on.midpoint=false; on.center=true;
            SnapConfig off; off.endpoint=false; off.intersection=false; off.tangent=false; off.midpoint=false; off.center=false;
            SnapResult r1 = snapCfg(es, Vec3(103,98,0), nullptr, 6.0, on);
            SnapResult r2 = snapCfg(es, Vec3(103,98,0), nullptr, 6.0, off);
            check(r1.snapped && r1.type == SnapType::Center, "圆心开启 → 捕捉为 Center");
            check(!r2.snapped, "圆心关闭 → 同光标不捕捉（门控生效）");
        }

        // 20.6 切点门控：提供首端点 P 时光标近圆，开启→Tangent，关闭→不捕捉
        {
            std::vector<Element> es{ mkCircle(5, Vec3(0,0,0), 25.0) };
            const Vec3 P(40,0,0);
            Vec3 tp[2]; int n = tangentPointsFromPointToCircle(P, Vec3(0,0,0), 25.0, tp);
            const Vec3 cur(tp[0].x + 1.0, tp[0].y + 1.0, 0);
            SnapConfig on;  on.endpoint=false; on.center=false; on.intersection=false; on.midpoint=false; on.tangent=true;
            SnapConfig off; off.endpoint=false; off.center=false; off.intersection=false; off.midpoint=false; off.tangent=false;
            SnapResult r1 = snapCfg(es, cur, &P, 6.0, on);
            SnapResult r2 = snapCfg(es, cur, &P, 6.0, off);
            check(r1.snapped && r1.type == SnapType::Tangent, "切线开启 → 捕捉为 Tangent");
            check(!r2.snapped, "切线关闭 → 同光标不捕捉（门控生效）");
            (void)n;
        }

        // 20.7 交点门控：两交叉直线的交点，开启→Intersection，关闭→不捕捉
        {
            std::vector<Element> es{ mkLine(10, Vec3(-100, 0, 0), Vec3(100, 0, 0)),   // X 轴
                                     mkLine(11, Vec3(0, -50, 0), Vec3(0, 50, 0)) };   // Y 轴（交于原点）
            SnapConfig on;  on.endpoint=false; on.center=false; on.tangent=false; on.midpoint=false; on.intersection=true;
            SnapConfig off; off.endpoint=false; off.center=false; off.tangent=false; off.midpoint=false; off.intersection=false;
            SnapResult r1 = snapCfg(es, Vec3(3, -2, 0), nullptr, 6.0, on);
            SnapResult r2 = snapCfg(es, Vec3(3, -2, 0), nullptr, 6.0, off);
            check(r1.snapped && r1.type == SnapType::Intersection, "交点开启 → 捕捉为 Intersection");
            check(!r2.snapped, "交点关闭 → 同光标不捕捉（门控生效）");
        }
    }

    // ============================================================
    // §22 直线绘制子模式（菜单扩展）
    // ============================================================
    qInfo().noquote() << "\n§22 直线绘制子模式";
    {
        auto mkLineE = [](int id, const Vec3& a, const Vec3& b) {
            Element e; e.type = ElementType::Line; e.id = id; e.pts = {a, b}; return e;
        };
        auto mkCircle = [](int id, const Vec3& c, double r) {
            Element e; e.type = ElementType::Circle; e.id = id; e.pts = {c}; e.radius = r; return e;
        };

        // 22.1 水平线：终点 y 与起点相同
        {
            auto r = line::horizontal(Vec3(0, 10, 0), Vec3(50, 30, 0));
            check(r.valid && qFuzzyCompare(r.a.y, r.b.y) && qFuzzyCompare(r.b.x, 50.0),
                  "水平线保持 y 不变");
        }
        // 22.2 竖直线：终点 x 与起点相同
        {
            auto r = line::vertical(Vec3(0, 10, 0), Vec3(50, 30, 0));
            check(r.valid && qFuzzyCompare(r.a.x, r.b.x) && qFuzzyCompare(r.b.y, 30.0),
                  "竖直线保持 x 不变");
        }
        // 22.3 固定角度：30° 射线
        {
            auto r = line::angle(Vec3(0, 0, 0), Vec3(100, 0, 0), 30.0);
            check(r.valid && qFuzzyCompare(geo::segAngleDeg(r.a, r.b), 30.0),
                  "Angle 模式生成 30° 射线");
        }
        // 22.4 平行线：参考线沿 X 轴，过 (0,20) 平行于 X 轴
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto r = line::compute(line::Mode::Parallel, {Vec3(0, 20, 0)}, {1}, Vec3(80, 20, 0), es);
            check(r.valid && qFuzzyCompare(r.a.y, 20.0) && qFuzzyCompare(r.b.y, 20.0),
                  "Parallel 生成与参考线平行的直线");
        }
        // 22.5 垂直线：参考线沿 X 轴，过 (20,0) 垂直于 X 轴
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto r = line::compute(line::Mode::Orthogonal, {Vec3(20, 0, 0)}, {1}, Vec3(20, 50, 0), es);
            check(r.valid && qFuzzyCompare(r.a.x, 20.0) && qFuzzyCompare(r.b.x, 20.0),
                  "Orthogonal 生成与参考线垂直的直线");
        }
        // 22.6 点到直线垂线
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto r = line::compute(line::Mode::FromPointToLine, {Vec3(30, 40, 0)}, {1}, Vec3(0,0,0), es);
            check(r.valid && qFuzzyCompare(r.b.y, 0.0) && qFuzzyCompare(r.b.x, 30.0),
                  "From Point To Line 生成正确垂足");
        }
        // 22.7 点→圆切线
        {
            std::vector<Element> es{ mkCircle(1, Vec3(0, 0, 0), 30.0) };
            Vec3 P(60, 0, 0);
            auto r = line::compute(line::Mode::TangentPC, {P}, {1}, Vec3(30, 35, 0), es);
            check(r.valid && qFuzzyCompare(r.a.x, P.x) && qFuzzyCompare(r.a.y, P.y),
                  "Tangent(P,C) 从外部点出发");
            check(r.valid && std::fabs((r.b - Vec3(0,0,0)).len() - 30.0) < 1e-3,
                  "Tangent(P,C) 切点落在圆上");
        }
        // 22.8 两圆公切线
        {
            std::vector<Element> es{ mkCircle(1, Vec3(0, 0, 0), 20.0),
                                     mkCircle(2, Vec3(100, 0, 0), 20.0) };
            auto r = line::compute(line::Mode::TangentCC, {}, {1, 2}, Vec3(50, 25, 0), es);
            check(r.valid && qFuzzyCompare(std::fabs((r.b - r.a).y), 0.0),
                  "Tangent(C,C) 生成水平外公切线（等径圆）");
        }
        // 22.9 Snake 正交折线
        {
            std::vector<Vec3> pts{ Vec3(0, 0, 0) };
            auto r = line::snakeSegment(pts, Vec3(50, 30, 0), /*xFirst=*/true);
            check(r.valid && qFuzzyCompare(r.a.y, 0.0) && qFuzzyCompare(r.b.y, 0.0),
                  "Snake(X) 第一段为水平线");
        }
        // 22.10 命令行入口 line_h / line_v
        {
            CommandInterpreter cli(&m);
            check(cli.execute("line_h 0,0 100,50") && cli.execute("line_v 0,0 100,50"),
                  "CLI line_h / line_v 执行成功");
        }
    }

    // §23 直线参数输入条相关算法（角度直线 / 过点平行 / 平行复制）
    // ============================================================
    qInfo().noquote() << "\n§23 直线参数输入条算法";
    {
        auto mkLineE = [](int id, const Vec3& a, const Vec3& b) {
            Element e; e.type = ElementType::Line; e.id = id; e.pts = {a, b}; return e;
        };
        // 23.1 角度 + 长度 → 第二点
        {
            Vec3 b = line::pointAtAngleLength(Vec3(0, 0, 0), 30.0, 100.0);
            check(qFuzzyCompare(b.distTo(Vec3(0, 0, 0)), 100.0) &&
                  qFuzzyCompare(geo::segAngleDeg(Vec3(0, 0, 0), b), 30.0),
                  "pointAtAngleLength 生成指定角度/长度的点");
        }
        // 23.2 过点平行（保持参考线长度与方向，沿垂直方向对齐到该点）
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto r = line::parallelAligned(es[0], Vec3(0, 20, 0));
            check(r.valid && qFuzzyCompare(r.a.y, 20.0) && qFuzzyCompare(r.b.y, 20.0) &&
                  qFuzzyCompare(r.a.distTo(r.b), 100.0),
                  "parallelAligned 生成等长且与该点对齐的平行线");
        }
        // 23.3 平行复制：参考线上方点 → 正向偏移多条
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto pc = line::parallelCopies(es[0], Vec3(0, 20, 0), 20.0, 3);
            bool ok = pc.valid && pc.lines.size() == 3;
            for (size_t i = 0; ok && i < pc.lines.size(); ++i) {
                const double y = 20.0 * (double)(i + 1);   // 偏移 20/40/60（上方）
                ok = qFuzzyCompare(pc.lines[i].first.y, y) &&
                     qFuzzyCompare(pc.lines[i].second.y, y) &&
                     qFuzzyCompare(pc.lines[i].first.distTo(pc.lines[i].second), 100.0);
            }
            check(ok, "parallelCopies 沿垂直方向正向生成 3 条等长平行线");
        }
        // 23.4 平行复制：参考线下方点 → 负向偏移（方向由点击侧决定）
        {
            std::vector<Element> es{ mkLineE(1, Vec3(0, 0, 0), Vec3(100, 0, 0)) };
            auto pc = line::parallelCopies(es[0], Vec3(0, -15, 0), 20.0, 2);
            bool ok = pc.valid && pc.lines.size() == 2;
            for (size_t i = 0; ok && i < pc.lines.size(); ++i) {
                const double y = -20.0 * (double)(i + 1);   // 偏移 -20/-40（下方）
                ok = qFuzzyCompare(pc.lines[i].first.y, y) && qFuzzyCompare(pc.lines[i].second.y, y);
            }
            check(ok, "parallelCopies 点击在参考线下方 → 负向偏移");
        }
        // 23.5 命令行 line_parallel_pt：过点平行且保持参考线长度（用独立模型，id 可预期）
        {
            SketchModel ml;
            CommandInterpreter cli(&ml);
            check(cli.execute("line 0,0 100,0"), "CLI 建参考线 #1");
            check(cli.execute("line_parallel_pt 1 0,30"), "CLI line_parallel_pt 执行成功");
            const Element* e = ml.element(2);   // #1 参考线，#2 即过点平行线
            check(e && e->type == ElementType::Line &&
                  qFuzzyCompare(e->pts[0].y, 30.0) && qFuzzyCompare(e->pts[1].y, 30.0) &&
                  qFuzzyCompare(e->pts[0].distTo(e->pts[1]), 100.0),
                  "CLI line_parallel_pt 生成等长对齐平行线");
        }
        // 23.6 requirements/allowedTypes 元数据（驱动 handle 采集与提示阶段机）
        {
            // 图元类型元数据与 handleLineSubTool 重构前 switch 逐项对齐
            check(line::allowedTypes(line::Mode::Bisector).size() == 2
                  && line::allowedTypes(line::Mode::Bisector)[0] == ElementType::Line,
                  "Bisector 允许拾取直线/矩形");
            check(line::allowedTypes(line::Mode::TangentPC).size() == 2
                  && line::allowedTypes(line::Mode::TangentPC)[0] == ElementType::Circle,
                  "TangentPC 允许拾取圆/圆弧");
            check(line::allowedTypes(line::Mode::TwoPoint).empty(),
                  "TwoPoint 无需参考图元");
            // 需求计数
            check(line::requirements(line::Mode::Bisector).refs == 2
                  && line::requirements(line::Mode::Bisector).pts == 1,
                  "Bisector 需要 2 参考 + 1 定位点");
            check(line::requirements(line::Mode::TangentCC).refs == 2
                  && line::requirements(line::Mode::TangentCC).pts == 0,
                  "TangentCC 需要 2 参考且无定位点");
            check(line::requirements(line::Mode::TwoPoint).refs == 0
                  && line::requirements(line::Mode::TwoPoint).pts == 2,
                  "TwoPoint 需要 2 定位点");
            check(line::requirements(line::Mode::Snake).rightClickFinish,
                  "Snake 右键提前结束");
            check(!line::requirements(line::Mode::Angle).rightClickFinish,
                  "Angle 不支持右键提前结束");
        }
        // 23.7 收尾契约：CLI 创建（子模式/标注）后图元类型正确、状态不残留
        // —— 锁 P11 统一到 commitElement 时，GraphicsView 每题四元组（add+select+reset）
        //    收敛为单一提交；此处验证 CLI 层的对等契约：连续创建互不污染、id 顺序稳定。
        {
            SketchModel md;
            CommandInterpreter cli(&md);
            check(cli.execute("line 0,0 50,0"), "CLI 建参考线 #1");
            check(cli.execute("line_parallel_pt 1 0,20"), "CLI 子模式建过点平行线 #2");
            check(cli.execute("circle 30,30 10"), "CLI 建圆 #3");
            check(cli.execute("dim distance 0,0 60,0"), "CLI 建距离标注 #4");
            check(cli.execute("dim radius 3"), "CLI 对圆 #3 建半径标注 #5");
            const Element* e2 = md.element(2);
            check(e2 && e2->type == ElementType::Line, "2# 为直线（子模式产物）");
            const Element* e3 = md.element(3);
            check(e3 && e3->type == ElementType::Circle, "3# 为圆");
            const Element* e4 = md.element(4);
            check(e4 && e4->type == ElementType::Dimension && e4->dtype == DimensionType::Distance,
                  "4# 为距离标注");
            const Element* e5 = md.element(5);
            check(e5 && e5->type == ElementType::Dimension && e5->dtype == DimensionType::Radius,
                  "5# 为半径标注");
            check(md.elements().size() == 5, "连续五次创建 id 连续、无多余残留",
                  QString::number(md.elements().size()));
        }
    }

    // §24 样条家族（参考图 FreeCAD Spline 菜单 12 项的几何与 CLI 层）
    // ============================================================
    qInfo().noquote() << "\n§24 样条家族：绘制 6 种 / 编辑 6 种 / 炸开为线段";
    {
        // 24.1 CLI 绘制六式（各自落库 subType）
        {
            SketchModel ml;
            CommandInterpreter cli(&ml);
            check(cli.execute("spline_parabola 0,0 50,60 100,0"), "CLI spline_parabola 执行成功");
            check(cli.execute("spline_hyperbola -40,0 40,0 30,10"), "CLI spline_hyperbola 执行成功");
            check(cli.execute("spline_parabola_4p 0,0 30,20 70,20 100,0"), "CLI spline_parabola_4p 执行成功");
            check(cli.execute("spline_parabola_fd 0,20 -50,-20 50,-20"), "CLI spline_parabola_fd 执行成功");
            check(cli.execute("spline_through_points 0,0 30,40 60,10 90,50"), "CLI spline_through_points 执行成功");
            check(cli.execute("freehand_line 0,0 10,5 25,12 40,10"), "CLI freehand_line 执行成功");
            check(ml.elementCount() == 6, "6 条样条全部落库",
                  QString("实际=%1").arg(ml.elementCount()));

            std::vector<int> want = {(int)spline::SubType::Parabola,
                                     (int)spline::SubType::Hyperbola,
                                     (int)spline::SubType::Parabola4P,
                                     (int)spline::SubType::ParabolaFocusDirectrix,
                                     (int)spline::SubType::ThroughPoints,
                                     (int)spline::SubType::Freehand};
            bool ok = true;
            for (size_t i = 0; ok && i < want.size(); ++i) {
                const Element* e = ml.element((int)i + 1);
                ok = e && e->type == ElementType::Spline && e->splineSubType == want[i] && !e->closed;
            }
            check(ok, "六种绘制方式 subType 依次为 1/2/3/4/5/6 且开放");
            const Element* tp = ml.element(5);
            check(tp && tp->pts.size() == 4, "spline_through_points 过点数与输入一致",
                  tp ? QString::number(tp->pts.size()) : "null");
            const Element* fh = ml.element(6);
            check(fh && fh->pts.size() == 4, "freehand_line 手绘点列直存",
                  fh ? QString::number(fh->pts.size()) : "null");
        }
        // 24.2 CLI 编辑六式（独立模型，id 可预期）
        {
            SketchModel ml;
            CommandInterpreter cli(&ml);
            // 源折线：三点 poly → spline_from_polyline
            check(cli.execute("poly 0,0 100,0 50,80"), "CLI 建三点折线 #1");
            check(cli.execute("spline_from_polyline 1"), "CLI spline_from_polyline 执行成功");
            const Element* sp = ml.element(2);   // #1 折线被替换为 #2 样条
            check(sp && sp->type == ElementType::Spline && sp->pts.size() == 3 &&
                  sp->splineSubType == (int)spline::SubType::CatmullRom,
                  "折线已转为 3 控制点样条",
                  sp ? QString("type=%1 n=%2").arg((int)sp->type).arg(sp->pts.size()) : "null");

            // append：点 (200,0) 距末端 (50,80) 更近？ -> 距首 (0,0) 距离 200，距末 (50,80) 距离 ~158
            // 200 > 158 => 距末端更近 => push_back
            check(cli.execute("spline_append_pt 2 200,0"), "CLI spline_append_pt 执行成功");
            check(ml.element(2)->pts.size() == 4 &&
                  ml.element(2)->pts.back().distTo(Vec3(200, 0, 0)) < 1e-9,
                  "追加控制点落在末端（距末端更近）");

            // insert：点 (50,10) 投影到首段 (0,0)-(100,0) -> 插入位置 index=1
            check(cli.execute("spline_insert_pts 2 50,10"), "CLI spline_insert_pts 执行成功");
            const Element* si = ml.element(2);
            check(si->pts.size() == 5 && si->pts[1].distTo(Vec3(50, 0, 0)) < 1e-9,
                  "插入点为最近段上的投影 (50,0)");

            // remove_pts：删除 index 1（刚插入的），回到 4 点
            check(cli.execute("spline_remove_pts 2 1"), "CLI spline_remove_pts 执行成功");
            check(ml.element(2)->pts.size() == 4, "删除后回到 4 控制点");
            check(!cli.execute("spline_remove_pts 2 99"), "越界索引被拒绝");

            // remove_between：4 点 0,0 / 100,0 / 50,80 / 200,0，删 index 0..3 之间（保留首末）
            check(!cli.execute("spline_remove_between 2 1 2"), "相邻控制点（无中间段）被拒绝");
            check(cli.execute("spline_remove_between 2 0 3"), "CLI spline_remove_between 执行成功");
            const Element* rb = ml.element(2);
            check(rb->pts.size() == 2 &&
                  rb->pts[0].distTo(Vec3(0, 0, 0)) < 1e-9 &&
                  rb->pts[1].distTo(Vec3(200, 0, 0)) < 1e-9,
                  "中间段已删除，首末控制点保留");

            // explode：2 点样条 → 1 条线段
            check(cli.execute("spline_explode 2"), "CLI spline_explode 执行成功");
            const Element* ln = ml.element(3);
            check(ln && ln->type == ElementType::Line &&
                  ln->pts[0].distTo(Vec3(0, 0, 0)) < 1e-9 &&
                  ln->pts[1].distTo(Vec3(200, 0, 0)) < 1e-9,
                  "2 点样条炸开为 1 条线段，端点保留");
        }
        // 24.3 explodeToLines 几何：插值类按节点弦、圆锥类按采样
        {
            // CatmullRom 4 节点 → 3 段，端点逐一吻合
            std::vector<Vec3> knots{Vec3(0, 0, 0), Vec3(10, 20, 0), Vec3(20, 5, 0), Vec3(30, 30, 0)};
            auto segs = spline::explodeToLines(knots, (int)spline::SubType::CatmullRom);
            bool ok = segs.size() == 3;
            for (int i = 0; ok && i < 3; ++i)
                ok = segs[i].first.distTo(knots[i]) < 1e-9 && segs[i].second.distTo(knots[i + 1]) < 1e-9;
            check(ok, "插值样条按节点弦炸开为 n-1 段且端点吻合",
                  QString("段数=%1").arg(segs.size()));

            // 3 点抛物线采样：24 段；首末端点 = P0/P2；t=0.5 处点 = (P0+2P1+P2)/4
            const Vec3 P0(0, 0, 0), P1(50, 100, 0), P2(100, 0, 0);
            auto psegs = spline::explodeToLines({P0, P1, P2}, (int)spline::SubType::Parabola, 24);
            check(psegs.size() == 24, "抛物线采样炸开为 24 段",
                  QString("段数=%1").arg(psegs.size()));
            check(psegs.front().first.distTo(P0) < 1e-9 && psegs.back().second.distTo(P2) < 1e-9,
                  "采样炸开首末端点 = 抛物线两端点");
            const Vec3 mid = psegs[11].second;   // t = 12/24 = 0.5
            const Vec3 expect = (P0 + P1 * 2.0 + P2) * 0.25;
            check(mid.distTo(expect) < 1e-6, "t=0.5 采样点落在理论抛物线上",
                  QString("(%1,%2)").arg(mid.x).arg(mid.y));

            // 双曲线：全部采样点（线段端点）到两焦点距离差 ≈ 常数 2a。
            // （线段中点是弦上的点，本就不精确落在双曲线上，故只验端点。）
            const Vec3 F1(-40, 0, 0), F2(40, 0, 0), P(30, 10, 0);
            const double twoA = std::fabs(P.distTo(F1) - P.distTo(F2));
            auto hsegs = spline::explodeToLines({F1, F2, P}, (int)spline::SubType::Hyperbola, 32);
            ok = hsegs.size() == 32;
            for (int i = 0; ok && i < 32; ++i) {
                const double d = std::fabs(hsegs[i].first.distTo(F1) - hsegs[i].first.distTo(F2));
                ok = std::fabs(d - twoA) < 1e-6;
            }
            ok = ok && std::fabs(std::fabs(hsegs.back().second.distTo(F1) -
                                           hsegs.back().second.distTo(F2)) - twoA) < 1e-6;
            check(ok, "双曲线采样端点均满足定差 2a", QString("段数=%1").arg(hsegs.size()));

            // polylineControlPoints：Line→2 点、Polygon→顶点、Rectangle→4 角
            Element lnE; lnE.type = ElementType::Line;   lnE.pts = {Vec3(0,0,0), Vec3(9,9,0)};
            Element pgE; pgE.type = ElementType::Polygon; pgE.pts = knots;
            Element rcE; rcE.type = ElementType::Rectangle; rcE.pts = {Vec3(0,0,0), Vec3(10,5,0)};
            check(spline::polylineControlPoints(lnE).size() == 2, "polylineControlPoints(Line)=2 点");
            check(spline::polylineControlPoints(pgE).size() == 4, "polylineControlPoints(Polygon)=顶点");
            auto rp = spline::polylineControlPoints(rcE);
            check(rp.size() == 4 && rp[3].distTo(Vec3(0, 5, 0)) < 1e-9,
                  "polylineControlPoints(Rectangle) 展开为 4 角点");
        }
    }

    // ============================================================
    // 25. 椭圆绘制（6 种模式 + 几何层 + 命令行）
    // ============================================================
        // 拟合结果表示的归一：椭圆的 (a,b,rot) 与 (b,a,rot+90°) 是同一个椭圆，
        // 且旋转角以 180° 为周期。断言前先规范成「长半轴在前、角度落在 [0,180)」。
        auto canon = [](double a, double b, double rot) {
            if (b > a) { std::swap(a, b); rot += 90.0; }
            double m = std::fmod(rot, 180.0);
            if (m < 0) m += 180.0;
            return std::array<double, 3>{a, b, m};
        };

        qInfo().noquote() << QString("\n=== 25. 椭圆：6 种绘制模式与几何层 ===");
        {
            // 25.1 中心 + 椭圆上一点（该点作为长轴端点，短轴取长轴一半）
        {
            auto r = ellipse::onePoint(Vec3(0, 0, 0), Vec3(10, 0, 0));
            check(r.valid && qFuzzyCompare(r.radius, 10.0) && qFuzzyCompare(r.radiusY, 5.0),
                  "1点：a=10 b=5 rot=0",
                  QString("a=%1 b=%2 rot=%3").arg(r.radius).arg(r.radiusY).arg(r.rotation));
            auto r2 = ellipse::onePoint(Vec3(0, 0, 0), Vec3(0, 8, 0));
            check(r2.valid && qFuzzyCompare(r2.radius, 8.0) && qFuzzyCompare(r2.radiusY, 4.0) &&
                  qFuzzyCompare(r2.rotation, 90.0),
                  "1点：竖直方向时 rot=90°",
                  QString("rot=%1").arg(r2.rotation));
        }

        // 25.2 中心 + 长轴端点 + 椭圆上一点
        {
            // 中心(0,0) 长轴端点(10,0) → a=10 rot=0；过(0,5) → b=5
            auto r = ellipse::axis(Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(0, 5, 0));
            check(r.valid && qFuzzyCompare(r.radius, 10.0) && qFuzzyCompare(r.radiusY, 5.0) &&
                  qFuzzyCompare(r.rotation, 0.0),
                  "轴：a=10 b=5 rot=0",
                  QString("a=%1 b=%2 rot=%3").arg(r.radius).arg(r.radiusY).arg(r.rotation));
            // 反解验证：parameters u=a*cos45, v=b*sin45 的点必须落在椭圆上
            const double t = M_PI / 4;
            const Vec3 q(10 * std::cos(t), 5 * std::sin(t), 0);
            const double f = (q.x * q.x) / (r.radius * r.radius) +
                             (q.y * q.y) / (r.radiusY * r.radiusY);
            check(std::fabs(f - 1.0) < 1e-9, "轴：45° 参数点满足 x²/a²+y²/b²=1");
            // 第三点落在长轴上 → 无法确定短半轴，应报错
            auto bad = ellipse::axis(Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(5, 0, 0));
            check(!bad.valid, "轴：第三点落在长轴上应判失败", bad.msg);
        }

        // 25.3 两焦点 + 椭圆上一点（定义：到两焦点距离和 = 2a）
        {
            const Vec3 F1(-3, 0, 0), F2(3, 0, 0), P(0, 4, 0);
            auto r = ellipse::fociPoint(F1, F2, P);
            // d1=d2=5 → 2a=10 → a=5；焦距 6 → c=3 → b=√(25-9)=4
            check(r.valid && qFuzzyCompare(r.radius, 5.0) && qFuzzyCompare(r.radiusY, 4.0) &&
                  r.center.distTo(Vec3(0, 0, 0)) < 1e-9,
                  "焦点：a=5 b=4 中心在焦点中点",
                  QString("a=%1 b=%2 c=(%3,%4)").arg(r.radius).arg(r.radiusY)
                         .arg(r.center.x).arg(r.center.y));
            // 焦点在竖直方向时 rot 应为 90°
            auto rv = ellipse::fociPoint(Vec3(0, -2, 0), Vec3(0, 2, 0), Vec3(5, 0, 0));
            check(rv.valid && qFuzzyCompare(rv.rotation, 90.0),
                  "焦点：竖直焦点连线 → rot=90°", QString("rot=%1").arg(rv.rotation));
            // 点在焦点连线上 → 退化，应报错
            auto bad = ellipse::fociPoint(F1, F2, Vec3(0, 0, 0));
            check(!bad.valid, "焦点：最大距离和不大于焦距时判失败", bad.msg);
        }

        // 25.4 四点点拟合轴对齐椭圆
        {
            // 取标准椭圆 a=10,b=5 上的四个点（含两轴端点），应精确复原
            std::vector<Vec3> pts{Vec3(10, 0, 0), Vec3(0, 5, 0), Vec3(-10, 0, 0), Vec3(0, -5, 0)};
            auto r = ellipse::fourPoint(pts);
            check(r.valid && std::fabs(r.radius - 10.0) < 1e-6 && std::fabs(r.radiusY - 5.0) < 1e-6 &&
                  std::fabs(r.rotation) < 1e-6 && r.center.distTo(Vec3(0, 0, 0)) < 1e-9,
                  "4点：拟合复原 a=10 b=5 rot=0（轴端点）",
                  QString("a=%1 b=%2 rot=%3").arg(r.radius).arg(r.radiusY).arg(r.rotation));
            // 取椭圆上四个不对称的参数点（良定）：应唯一复原原椭圆
            std::vector<Vec3> pts2;
            for (double ang : {20.0, 80.0, 150.0, 250.0}) {
                const double t = ang * M_PI / 180.0;
                pts2.push_back(Vec3(10 * std::cos(t), 5 * std::sin(t), 0));
            }
            auto r2 = ellipse::fourPoint(pts2);
            check(r2.valid && std::fabs(r2.radius - 10.0) < 1e-6 && std::fabs(r2.radiusY - 5.0) < 1e-6,
                  "4点：一般位置四点唯一复原 a=10 b=5",
                  QString("a=%1 b=%2 %3").arg(r2.radius).arg(r2.radiusY).arg(r2.msg));
            // 欠定布局：四点凑成轴对齐矩形时四行方程退化成同一行，
            // 过它们有无穷多个轴对齐椭圆 —— 必须报错，不能随便挑一个。
            std::vector<Vec3> degen;
            for (double ang : {45.0, 135.0, 225.0, 315.0}) {
                const double t = ang * M_PI / 180.0;
                degen.push_back(Vec3(10 * std::cos(t), 5 * std::sin(t), 0));
            }
            auto rd = ellipse::fourPoint(degen);
            check(!rd.valid, "4点：退化布局（矩形四顶点）判为欠定而非产出假椭圆", rd.msg);
            // 点数不足
            check(!ellipse::fourPoint({Vec3(1,0,0), Vec3(0,1,0)}).valid, "4点：点数不足判失败");
        }

        // 25.5 中心 + 三点拟合旋转椭圆
        {
            // 构造 rot=30°, a=10, b=5 的椭圆并取三个一般位置的点
            const Vec3 C(2, 3, 0);
            const double rotDeg = 30.0, A = 10.0, B = 5.0;
            const double ar = rotDeg * M_PI / 180.0;
            std::vector<Vec3> pts;
            for (double ang : {20.0, 140.0, 260.0}) {
                const double t = ang * M_PI / 180.0;
                const double lx = A * std::cos(t), ly = B * std::sin(t);
                pts.push_back(Vec3(C.x + lx * std::cos(ar) - ly * std::sin(ar),
                                   C.y + lx * std::sin(ar) + ly * std::cos(ar), 0));
            }
            auto r = ellipse::center3Points(C, pts);
            // 长轴方向 ±180° 等价，且长短轴可互换表示，故按 canon 比较
            const auto got = canon(r.radius, r.radiusY, r.rotation);
            check(r.valid && std::fabs(got[0] - A) < 1e-3 && std::fabs(got[1] - B) < 1e-3 &&
                  std::fabs(got[2] - rotDeg) < 0.5,
                  "中心+3点：拟合复原 a=10 b=5 rot=30°",
                  QString("a=%1 b=%2 rot=%3").arg(r.radius).arg(r.radiusY).arg(r.rotation));
            // 三个输入点都必须落在拟合出的椭圆上（残差 → 0）
            bool onCurve = true;
            for (const auto& p : pts) {
                const Vec3 d = p - C;
                const double phi = std::atan2(d.y, d.x) - r.rotation * M_PI / 180.0;
                const double rad = d.len2D();
                const double u = rad * std::cos(phi), v = rad * std::sin(phi);
                const double f = (u * u) / (r.radius * r.radius) + (v * v) / (r.radiusY * r.radiusY);
                if (std::fabs(f - 1.0) > 1e-2) onCurve = false;
            }
            check(onCurve, "中心+3点：三个定义点均落在拟合椭圆上");
        }

        // 25.6 内切椭圆（矩形 / 多边形）
        {
            Element rc; rc.type = ElementType::Rectangle;
            rc.pts = {Vec3(0, 0, 0), Vec3(20, 10, 0)};
            auto r1 = ellipse::inscribed(rc);
            check(r1.valid && std::fabs(r1.radius - 10.0) < 1e-9 &&
                  std::fabs(r1.radiusY - 5.0) < 1e-9 &&
                  r1.center.distTo(Vec3(10, 5, 0)) < 1e-9,
                  "内切(矩形)：中心(10,5) a=10 b=5",
                  QString("center=(%1,%2) a=%3 b=%4").arg(r1.center.x).arg(r1.center.y)
                         .arg(r1.radius).arg(r1.radiusY));
            // 「内切」是与各边相切，并非穿过四个角：应验证四条边的中点落在椭圆上，
            // 而四个角点应当在椭圆之外（角点在椭圆内 = 未按定义内切）。
            bool tangent = true, cornersOutside = true;
            for (const Vec3& m : std::vector<Vec3>{Vec3(10,0,0), Vec3(20,5,0),
                                                   Vec3(10,10,0), Vec3(0,5,0)}) {
                const Vec3 d = m - r1.center;
                const double f = (d.x * d.x) / (r1.radius * r1.radius) +
                                 (d.y * d.y) / (r1.radiusY * r1.radiusY);
                if (std::fabs(f - 1.0) > 1e-9) tangent = false;
            }
            for (const Vec3& c : std::vector<Vec3>{Vec3(0,0,0), Vec3(20,0,0),
                                                   Vec3(20,10,0), Vec3(0,10,0)}) {
                const Vec3 d = c - r1.center;
                const double f = (d.x * d.x) / (r1.radius * r1.radius) +
                                 (d.y * d.y) / (r1.radiusY * r1.radiusY);
                if (f < 1.0 + 1e-9) cornersOutside = false;   // 角点必须在椭圆外
            }
            check(tangent, "内切(矩形)：四条边的中点均在椭圆上（与边相切）");
            check(cornersOutside, "内切(矩形)：四个角点在椭圆之外（未穿透外框）");

            Element pg; pg.type = ElementType::Polygon;
            pg.pts = {Vec3(0, 0, 0), Vec3(6, 0, 0), Vec3(6, 4, 0), Vec3(0, 4, 0)};
            auto r2 = ellipse::inscribed(pg);
            check(r2.valid && std::fabs(r2.radius - 3.0) < 1e-9 && std::fabs(r2.radiusY - 2.0) < 1e-9,
                  "内切(多边形)：按包围盒 a=3 b=2",
                  QString("a=%1 b=%2").arg(r2.radius).arg(r2.radiusY));
            // 非封闭图形应拒绝
            Element bad; bad.type = ElementType::Line; bad.pts = {Vec3(0,0,0), Vec3(1,1,0)};
            check(!ellipse::inscribed(bad).valid, "内切：直线等开放图形应判失败");
        }

        // 25.7 模式串 ↔ Mode 映射（菜单/工具/命令行共用的唯一入口）
        {
            struct { const char* s; ellipse::Mode m; } cases[] = {
                {"ellipse_1pt", ellipse::Mode::OnePoint},
                {"ellipse_axis", ellipse::Mode::Axis},
                {"ellipse_foci", ellipse::Mode::FociPoint},
                {"ellipse_4p", ellipse::Mode::FourPoint},
                {"ellipse_c3p", ellipse::Mode::Center3Points},
                {"ellipse_inscribed", ellipse::Mode::Inscribed},
            };
            bool ok = true;
            for (auto& c : cases) {
                if (ellipse::modeFromString(c.s) != c.m) ok = false;
                if (ellipse::stringFromMode(c.m) != QString(c.s)) ok = false;
            }
            check(ok, "模式串 ↔ Mode 双向映射一致（6 项）");
            // 只有内切需要拾取参考图元，且限定为矩形/多边形
            check(ellipse::requirements(ellipse::Mode::Inscribed).refs == 1 &&
                  ellipse::allowedTypes(ellipse::Mode::Inscribed).size() == 2,
                  "内切：需 1 个参考图元，允许矩形/多边形");
            check(ellipse::requirements(ellipse::Mode::OnePoint).pts == 1 &&
                  ellipse::requirements(ellipse::Mode::Axis).pts == 2 &&
                  ellipse::requirements(ellipse::Mode::FociPoint).pts == 2 &&
                  ellipse::requirements(ellipse::Mode::FourPoint).pts == 4 &&
                  ellipse::requirements(ellipse::Mode::Center3Points).pts == 4,
                  "各模式所需点数符合 6 种画法定义");
        }

        // 25.8 几何层：离散化 / 命中检测 / 控制点
        {
            Element el; el.type = ElementType::Ellipse;
            el.pts = {Vec3(0, 0, 0)}; el.radius = 10.0; el.radiusY = 5.0; el.rotation = 0.0;

            // 采样点应落在椭圆上，且数量为 n
            auto samples = geo::sampleEllipse(el.pts[0], el.radius, el.radiusY, el.rotation, 160);
            bool onCurve = samples.size() == 160;
            for (const auto& p : samples) {
                const double f = (p.x * p.x) / 100.0 + (p.y * p.y) / 25.0;
                if (std::fabs(f - 1.0) > 1e-9) onCurve = false;
            }
            check(onCurve, "sampleEllipse：160 个采样点全部落在椭圆上",
                  QString("点数=%1").arg(samples.size()));
            // 旋转后采样点仍在同一形状的旋转版本上
            auto rotS = geo::sampleEllipse(Vec3(0,0,0), 10.0, 5.0, 90.0, 64);
            const Vec3 tip = *std::max_element(rotS.begin(), rotS.end(),
                [](const Vec3& a, const Vec3& b) { return a.y < b.y; });
            check(std::fabs(tip.x) < 1e-9 && std::fabs(tip.y - 10.0) < 1e-9,
                  "sampleEllipse：rot=90° 时长轴端点转到正上方",
                  QString("top=(%1,%2)").arg(tip.x).arg(tip.y));

            // 命中：椭圆上的点距离 ≈ 0，内部的点距离 > 0
            const double dOn = geo::distanceToElement(el, Vec3(10, 0, 0));
            const double dOn2 = geo::distanceToElement(el, Vec3(0, 5, 0));
            check(dOn < 0.02 && dOn2 < 0.02, "命中检测：椭圆上的点距离 ≈ 0",
                  QString("d(%1,%2)").arg(dOn).arg(dOn2));
            const double dIn = geo::distanceToElement(el, Vec3(0, 0, 0));
            check(dIn > 1.0, "命中检测：只命中轮廓，不命中内部空白区域",
                  QString("d=%1").arg(dIn));

            // 控制点：中心 + 长轴端点 + 短轴端点
            auto hs = geo::elementHandles(el);
            check(hs.size() == 3 &&
                  hs[0].distTo(Vec3(0,0,0)) < 1e-9 &&
                  hs[1].distTo(Vec3(10,0,0)) < 1e-9 &&
                  hs[2].distTo(Vec3(0,5,0)) < 1e-9,
                  "控制点 = [中心, 长轴端点, 短轴端点]",
                  QString("点数=%1").arg(hs.size()));
            // 拖长轴端点改半径与旋向；拖短轴端点改短半轴
            Element e2 = el;
            geo::setElementHandle(e2, 1, Vec3(0, 20, 0));
            check(std::fabs(e2.radius - 20.0) < 1e-9 && qFuzzyCompare(e2.rotation, 90.0),
                  "拖长轴端点：a=20 且 rot=90°",
                  QString("a=%1 rot=%2").arg(e2.radius).arg(e2.rotation));
            Element e3 = el;
            geo::setElementHandle(e3, 2, Vec3(0, 8, 0));
            check(std::fabs(e3.radiusY - 8.0) < 1e-9 && std::fabs(e3.radius - 10.0) < 1e-9,
                  "拖短轴端点：b=8 且长轴不变",
                  QString("a=%1 b=%2").arg(e3.radius).arg(e3.radiusY));
            Element e4 = el;
            geo::setElementHandle(e4, 0, Vec3(5, 5, 0));
            check(e4.pts[0].distTo(Vec3(5, 5, 0)) < 1e-9, "拖中心：整体平移");

            // flatten：闭合折线 n 段（= 采样点数）
            auto segs = geo::flatten(el);
            check(segs.size() == 160 && segs.back().b.distTo(segs.front().a) < 1e-9,
                  "flatten：椭圆离散为 160 段闭合折线",
                  QString("段数=%1").arg(segs.size()));
        }

        // 25.9 命令行：6 种模式端到端
        {
            SketchModel mc;
            CommandInterpreter cli(&mc);
            // 1点：中心 + 椭圆上一点
            check(cli.execute("ellipse_1pt 0,0 10,0"), "CLI ellipse_1pt 执行成功");
            const Element* e1 = mc.element(1);
            check(e1 && e1->type == ElementType::Ellipse &&
                  std::fabs(e1->radius - 10.0) < 1e-6 && std::fabs(e1->radiusY - 5.0) < 1e-6,
                  "CLI ellipse_1pt → a=10 b=5",
                  e1 ? QString("a=%1 b=%2").arg(e1->radius).arg(e1->radiusY) : QString("无图元"));
            // 轴：中心 + 长轴端点 + 过点
            check(cli.execute("ellipse_axis 0,0 20,0 0,7"), "CLI ellipse_axis 执行成功");
            const Element* e2 = mc.element(2);
            check(e2 && std::fabs(e2->radius - 20.0) < 1e-6 && std::fabs(e2->radiusY - 7.0) < 1e-6,
                  "CLI ellipse_axis → a=20 b=7",
                  e2 ? QString("a=%1 b=%2").arg(e2->radius).arg(e2->radiusY) : QString("无图元"));
            // 焦点：F1 F2 + 椭圆上一点
            check(cli.execute("ellipse_foci -3,0 3,0 0,4"), "CLI ellipse_foci 执行成功");
            const Element* e3 = mc.element(3);
            check(e3 && std::fabs(e3->radius - 5.0) < 1e-6 && std::fabs(e3->radiusY - 4.0) < 1e-6,
                  "CLI ellipse_foci → a=5 b=4（焦点定义）",
                  e3 ? QString("a=%1 b=%2").arg(e3->radius).arg(e3->radiusY) : QString("无图元"));
            // 4 点
            check(cli.execute("ellipse_4p 10,0 0,5 -10,0 0,-5"), "CLI ellipse_4p 执行成功");
            const Element* e4 = mc.element(4);
            check(e4 && std::fabs(e4->radius - 10.0) < 1e-6 && std::fabs(e4->radiusY - 5.0) < 1e-6,
                  "CLI ellipse_4p → a=10 b=5",
                  e4 ? QString("a=%1 b=%2").arg(e4->radius).arg(e4->radiusY) : QString("无图元"));
            // 中心 + 3 点
            check(cli.execute("ellipse_c3p 0,0 10,0 0,5 -7.07,3.54"), "CLI ellipse_c3p 执行成功");
            const Element* e5 = mc.element(5);
            // 三点 (10,0),(0,5),(-7.07,3.54) 都在 a=10 b=5 rot=0 的椭圆上，
            // 但拟合可能返回等价的 (b,a,rot+90°)，故按 canon 比较
            check(e5 && e5->type == ElementType::Ellipse, "CLI ellipse_c3p 生成 Ellipse 图元");
            const auto c5 = e5 && e5->type == ElementType::Ellipse
                                ? canon(e5->radius, e5->radiusY, e5->rotation)
                                : std::array<double, 3>{0.0, 0.0, 0.0};
            check(e5 && e5->type == ElementType::Ellipse &&
                  std::fabs(c5[0] - 10.0) < 0.05 && std::fabs(c5[1] - 5.0) < 0.05 &&
                  (std::fabs(c5[2] - 0.0) < 0.5 || std::fabs(c5[2] - 180.0) < 0.5),
                  "CLI ellipse_c3p → 复原 a≈10 b≈5",
                  e5 ? QString("a=%1 b=%2 rot=%3").arg(e5->radius).arg(e5->radiusY).arg(e5->rotation)
                     : QString("无图元"));
            check(mc.elementCount() == 5, "CLI 共创建 5 个椭圆",
                  QString("实际=%1").arg(mc.elementCount()));
            // 参数不足应被拒绝且不产生图元
            const int before = mc.elementCount();
            cli.execute("ellipse_axis 0,0 20,0");
            check(mc.elementCount() == before, "CLI ellipse_axis 参数不足时被拒绝（不产生图元）");
            // 未实现的椭圆子命令不应被当作 OnePoint 静默兜底
            cli.execute("ellipse_foo 0,0 10,0");
            check(mc.elementCount() == before, "CLI ellipse_foo 未知子命令拒绝（不兜底为 1 点）");
        }

        // 25.10 内切：命令行选中矩形生成内切椭圆
        {
            SketchModel mr;
            CommandInterpreter cir(&mr);
            check(cir.execute("rect 0,0 20,10"), "CLI 建矩形（作为内切外框）");
            check(mr.elementCount() == 1, "矩形已加入场景",
                  QString("数量=%1").arg(mr.elementCount()));
            check(cir.execute("ellipse_inscribed 1"), "CLI ellipse_inscribed 执行成功");
            check(mr.elementCount() == 2, "内切椭圆已加入场景",
                  QString("数量=%1").arg(mr.elementCount()));
            const Element* box = mr.element(1);
            const Element* ins = mr.element(2);
            check(box && ins && ins->type == ElementType::Ellipse,
                  "新图元类型为 Ellipse");
            check(ins && std::fabs(ins->radius - 10.0) < 1e-9 &&
                  std::fabs(ins->radiusY - 5.0) < 1e-9 &&
                  !ins->pts.empty() && ins->pts[0].distTo(Vec3(10, 5, 0)) < 1e-9,
                  "内切椭圆：中心(10,5) a=10 b=5",
                  ins && !ins->pts.empty()
                      ? QString("c=(%1,%2) a=%3 b=%4").arg(ins->pts[0].x).arg(ins->pts[0].y)
                            .arg(ins->radius).arg(ins->radiusY) : QString("无图元"));
            // 外框矩形必须仍在（内切不应替换/删掉原图元）
            check(box && box->type == ElementType::Rectangle, "原矩形仍保留，未被内切替换");
            // 非法 id 应被拒绝
            const int before = mr.elementCount();
            cir.execute("ellipse_inscribed 999");
            check(mr.elementCount() == before, "ellipse_inscribed 非法 id 被拒绝（不产生图元）");
        }

        // 25.11 名称与序列化字段
        {
            check(QString::fromUtf8(elementTypeName(ElementType::Ellipse)) == QStringLiteral("椭圆"),
                  "elementTypeName(Ellipse) == 椭圆");
            Element e; e.type = ElementType::Ellipse;
            e.pts = {Vec3(0,0,0)}; e.radius = 12.0; e.radiusY = 6.0;
            check(e.sizeDesc().contains(QStringLiteral("12.00")) &&
                  e.sizeDesc().contains(QStringLiteral("6.00")),
                  "sizeDesc 同时给出长短半轴", e.sizeDesc());
        }
    }

    // ====================================================================
    qInfo().noquote() << QString("\n=== 26. 折线：10 种功能（绘制 + 9 编辑）与几何层 ===");
    {
        // 26.1 折线创建（CLI polyline 精确命令）
        SketchModel m;
        CommandInterpreter cir(&m);
        check(cir.execute("polyline 0,0 10,0 10,10 0,10"),
              "CLI polyline 4 点开放折线");
        check(m.elementCount() == 1, "创建后场景含 1 个图元");
        const Element* pe = m.element(1);
        check(pe && pe->type == ElementType::Polyline, "图元类型为 Polyline");
        check(pe && pe->pts.size() == 4, "4 节点", pe ? QString::number(pe->pts.size()) : QString());
        check(pe && pe->arcFlags.size() == 3, "3 段直线（arcFlags 全 false）");
        for (bool a : pe->arcFlags) check(!a, "所有段默认直线");
        check(QString::fromUtf8(elementTypeName(ElementType::Polyline)) == QStringLiteral("折线"),
              "elementTypeName(Polyline) == 折线");

        // 26.2 闭合：首末重合
        check(cir.execute("polyline 0,0 5,0 5,5 0,0"),
              "CLI polyline 首末重合 → 闭合");
        const Element* pc = m.element(2);
        check(pc && pc->closed && pc->pts.size() == 3, "闭合折线 3 节点", pc ? QString("n=%1 closed=%2").arg(pc->pts.size()).arg(pc->closed) : "null");

        // 26.3 点数不足应拒绝
        check(!cir.execute("polyline 0,0"), "CLI polyline 点数不足拒绝");
        check(m.elementCount() == 2, "拒绝时不产生图元");

        // 26.4 arcThrough 几何：3 节点共圆，弧段采样点都落在该圆上
        {
            // 取圆心 (5,0) 半径 5 的圆上 3 个点
            std::vector<Vec3> pts = {
                Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(5, 5, 0)
            };
            std::vector<bool> flags = {false, true};   // 段 0=直线，段 1=弧
            geo::ArcSeg ar = geo::arcThrough(pts, 1, false);
            check(ar.valid, "arcThrough 段 1 成功求外接圆");
            // 期望圆心 (5,0) 半径 5
            check(ar.valid && std::fabs(ar.center.x - 5.0) < 1e-6 && std::fabs(ar.center.y - 0.0) < 1e-6 &&
                  std::fabs(ar.radius - 5.0) < 1e-6,
                  "arcThrough 求出 (5,0) r=5",
                  ar.valid ? QString("c=(%1,%2) r=%3").arg(ar.center.x).arg(ar.center.y).arg(ar.radius) : "invalid");
        }

        // 26.5 samplePolyline 混合线段+弧段
        {
            std::vector<Vec3> pts = {Vec3(0,0,0), Vec3(10,0,0), Vec3(10,5,0), Vec3(0,5,0)};
            std::vector<bool> flags = {false, true, true};
            auto dense = geo::samplePolyline(pts, flags, true, 12);
            // 4 段，flags[0..2] 有效，第 4 段默认 false；弧段 2 个 → 总点 ~ 2*12 + 2*1 + 1 闭合 ≈ 27
            check(dense.size() >= 25, "samplePolyline 混合段采样长度合理",
                  QString("n=%1").arg(dense.size()));
            // 段 1 弧段（pts[1]→pts[2]）由 (0,0)(10,0)(10,5) 三点定外接圆：
            //   圆心 (5, 2.5) 半径 = sqrt(25+6.25) ≈ 5.590
            // 弧段 12 段采样点都应落在该圆上，验证第 6 个（半弧位置）
            const Vec3& m1 = dense[6];
            const double d1 = std::hypot(m1.x - 5.0, m1.y - 2.5);
            check(std::fabs(d1 - 5.59016994) < 0.01, "samplePolyline 弧段 1 采样点落在外接圆上",
                  QString("d=%1 期望 5.59").arg(d1, 0, 'f', 4));
            // 起止点闭合（dense 首末应在数值上相等）
            check(dense.size() > 2 && dense.front().distTo(dense.back()) < 1e-9,
                  "samplePolyline 闭合时首末重合");
        }

        // 26.6 add_node / append_node / delete_node CLI
        SketchModel m2;
        CommandInterpreter c2(&m2);
        check(c2.execute("polyline 0,0 10,0 10,10"), "建 3 节点折线（场景 2）");
        const int id2 = m2.element(1)->id;
        // add_node: 段 0 中点 (5,0) 附近 → 插点
        check(c2.execute(QString("polyline_add_node %1 5,0").arg(id2)), "add_node @ (5,0)");
        const Element* pa = m2.element(1);
        check(pa && pa->pts.size() == 4, "add_node 后 4 节点", pa ? QString::number(pa->pts.size()) : "null");
        // append_node: (20, 0) 距末 (10,10) 比距首 (0,0) 近 → 末端追加
        check(c2.execute(QString("polyline_append_node %1 20,0").arg(id2)), "append_node @ (20,0)");
        const Element* pb = m2.element(1);
        check(pb && pb->pts.size() == 5, "append_node 后 5 节点");
        // delete_node 索引 0
        check(c2.execute(QString("polyline_delete_node %1 0").arg(id2)), "delete_node idx=0");
        const Element* pd = m2.element(1);
        check(pd && pd->pts.size() == 4, "delete_node 后 4 节点");

        // 26.7 trim_segments
        SketchModel m3;
        CommandInterpreter c3(&m3);
        check(c3.execute("polyline 0,0 5,0 10,0 15,0 20,0"), "建 5 节点直线折线");
        const int id3 = m3.element(1)->id;
        // trim 段 0 → 删 pts[1] → 4 节点
        check(c3.execute(QString("polyline_trim_segments %1 0").arg(id3)), "trim_segments 段 0");
        const Element* pt1 = m3.element(1);
        check(pt1 && pt1->pts.size() == 4, "trim 后 4 节点",
              pt1 ? QString::number(pt1->pts.size()) : "null");
        // 再 trim 段 0 → 3 节点
        check(c3.execute(QString("polyline_trim_segments %1 0").arg(id3)), "trim_segments 段 0 第二次");
        const Element* pt2 = m3.element(1);
        check(pt2 && pt2->pts.size() == 3, "trim 第二次后 3 节点",
              pt2 ? QString::number(pt2->pts.size()) : "null");
        // trim 第三次 → 2 节点（边界）
        check(c3.execute(QString("polyline_trim_segments %1 0").arg(id3)), "trim_segments 段 0 第三次");
        const Element* pt3 = m3.element(1);
        check(pt3 && pt3->pts.size() == 2, "trim 第三次后 2 节点",
              pt3 ? QString::number(pt3->pts.size()) : "null");
        // trim 第四次 → CLI 在 pts.size()==2 时拒绝（节点不足）
        check(!c3.execute(QString("polyline_trim_segments %1 0").arg(id3)),
              "trim 在 2 节点时拒绝（避免折线退化到 1 节点）");
        // 用 delete 命令清理
        check(c3.execute(QString("delete %1").arg(id3)), "delete 命令清理");

        // 26.8 equidistant
        SketchModel m4;
        CommandInterpreter c4(&m4);
        check(c4.execute("polyline 0,0 10,0 10,10 0,10"), "建矩形折线");
        const int id4 = m4.element(1)->id;
        check(c4.execute(QString("polyline_equidistant %1 5").arg(id4)), "equidistant d=5");
        const Element* pe2 = m4.element(2);
        check(pe2 && pe2->name.contains(QStringLiteral("等距")), "等距折线已生成",
              pe2 ? pe2->name : QString());

        // 26.9 from_segments：合并 2 条直线
        SketchModel m5;
        CommandInterpreter c5(&m5);
        check(c5.execute("line 0,0 10,0"), "建直线 1");
        check(c5.execute("line 10,0 10,10"), "建直线 2");
        const int l1 = m5.element(1)->id;
        const int l2 = m5.element(2)->id;
        check(c5.execute(QString("polyline_from_segments %1 %2").arg(l1).arg(l2)), "from_segments 合并两线");
        const Element* pf = m5.element(3);
        check(pf && pf->type == ElementType::Polyline && pf->pts.size() == 3,
              "合并后 3 节点折线",
              pf ? QString("n=%1 type=%2").arg(pf->pts.size()).arg((int)pf->type) : "null");

        // 26.10 arcs_to_chords + change_segment_type
        SketchModel m6;
        CommandInterpreter c6(&m6);
        check(c6.execute("polyline 0,0 10,0 10,5 0,5 0,0"), "建 5 节点折线");
        const int id6 = m6.element(1)->id;
        // 先把段 1 标记为弧
        check(c6.execute(QString("polyline_change_segment_type %1 1 1").arg(id6)), "切换段 1 为弧");
        const Element* p6a = m6.element(1);
        check(p6a && p6a->arcFlags.size() == 4 && p6a->arcFlags[1], "段 1 已为弧",
              p6a ? QString("flags[1]=%1").arg(p6a->arcFlags[1] ? "true" : "false") : "null");
        // arcs_to_chords 全部转直线
        check(c6.execute(QString("polyline_arcs_to_chords %1").arg(id6)), "arcs_to_chords");
        const Element* p6b = m6.element(1);
        check(p6b && std::none_of(p6b->arcFlags.begin(), p6b->arcFlags.end(), [](bool x){ return x; }),
              "arcs_to_chords 后所有段都是直线");

        // 26.11 change_segment_type 范围 1..2（段 0..1 都切换）
        check(c6.execute(QString("polyline_change_segment_type %1 0 1").arg(id6)), "切换段 0..1");
        const Element* p6c = m6.element(1);
        check(p6c && p6c->arcFlags[0] && p6c->arcFlags[1] && !p6c->arcFlags[2] && !p6c->arcFlags[3],
              "段 0..1 切到弧，段 2..3 保持直线");

        // 26.12 错误路径
        SketchModel m7;
        CommandInterpreter c7(&m7);
        check(!c7.execute("polyline_add_node 999 0,0"), "add_node 不存在的 id 拒绝");
        check(!c7.execute("polyline_add_node"), "add_node 参数不足拒绝");
        check(!c7.execute("polyline_foo 1"), "未知 polyline_ 子命令拒绝");
    }

    // =====================================================================
    qInfo().noquote() << QString("\n=== 27. 高级选择功能（全选/框选/实体/轮廓/相交/图层/反选）===");
    {
        SketchModel ms;
        auto mkLine = [](double x1, double y1, double x2, double y2) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
            return e;
        };
        // 闭合方块四边 + 一条孤立直线（不相连、不相交）
        int A = ms.addElement(mkLine(0,  0,  10,  0));   // 上边
        int B = ms.addElement(mkLine(10, 0,  10, 10));   // 右边
        int C = ms.addElement(mkLine(10, 10, 0,  10));   // 下边
        int D = ms.addElement(mkLine(0,  10, 0,  0));    // 左边（闭合方块 A-B-C-D）
        int E = ms.addElement(mkLine(100, 100, 110, 100)); // 孤立线，远离方块

        // 27.1 全选 / 反选
        ms.selectAll();
        check(ms.selection().size() == 5, "selectAll 选中全部 5 个图元", QString::number(ms.selection().size()));
        ms.invertSelection();
        check(ms.selection().empty(), "全选后 invertSelection 变为空集");
        ms.setSelected(A);
        ms.invertSelection();
        check(ms.selection().size() == 4 && !ms.isSelected(A), "invertSelection 选中 A 外的 4 个图元",
              QString::number(ms.selection().size()));

        // 27.2 实体选择 / 取消
        ms.clearSelection();
        ms.selectEntity(B);
        check(ms.isSelected(B) && ms.selectedId() == B, "selectEntity 选中单个图元且设为主选择");
        ms.deselectEntity(B);
        check(!ms.isSelected(B), "deselectEntity 从选择集移除单个图元");

        // 27.3 轮廓选择（拓扑连通）：方块四边相互共享端点，孤立线 E 不连通
        ms.clearSelection();
        ms.selectContour(A);
        check(ms.selection().size() == 4, "selectContour 从 A 收集连通方块 4 边", QString::number(ms.selection().size()));
        check(!ms.isSelected(E), "selectContour 不误选不相连图元 E");

        // 27.4 错误路径：无效种子 id 不崩溃、不产生选择
        ms.clearSelection();
        ms.selectContour(99999);
        check(ms.selection().empty(), "selectContour 无效种子 -> 空选择（不崩溃）");

        // 27.5 相交选择（独立模型，避免几何歧义）
        SketchModel mi;
        int Hi = mi.addElement(mkLine(0,  0,   100, 0));   // 水平线
        int Vi = mi.addElement(mkLine(50, -10, 50,  10));  // 竖直线，交于 (50,0)
        int Wi = mi.addElement(mkLine(200, 200, 210, 210)); // 不相交陪衬
        mi.setSelection({Hi});
        mi.selectIntersected(Hi);
        check(mi.isSelected(Vi) && !mi.isSelected(Wi) && mi.selection().size() == 2,
              "selectIntersected 仅选中相交的 V，不误选不相交的 W",
              QString::number(mi.selection().size()));
        mi.deselectIntersected(Hi);
        check(mi.isSelected(Hi) && !mi.isSelected(Vi), "deselectIntersected 仅移除相交的 V，保留种子 H");

        // 27.6 图层选择：为 A、B 指定图层 "dim"，其余默认 "0"
        ms.clearSelection();
        ms.updateElement(A, [](Element& e) { e.layer = "dim"; });
        ms.updateElement(B, [](Element& e) { e.layer = "dim"; });
        ms.selectLayer("dim");
        check(ms.selection().size() == 2 && ms.isSelected(A) && ms.isSelected(B),
              "selectLayer 选中 dim 图层 2 个图元", QString::number(ms.selection().size()));
        ms.addToSelection(E);
        ms.deselectLayer("dim");
        check(!ms.isSelected(A) && !ms.isSelected(B) && ms.isSelected(E),
              "deselectLayer 移除 dim 图层图元，保留非 dim 的 E");
        ms.clearSelection();
        ms.selectLayerOf(E);   // E 为默认图层 "0"
        check(ms.selection().size() >= 1 && !ms.isSelected(A) && !ms.isSelected(B),
              "selectLayerOf 选中与 E 同图层（非 dim）的图元", QString::number(ms.selection().size()));

        // 27.7 错误路径：图层不存在 -> 空选择
        ms.clearSelection();
        ms.selectLayer("no_such_layer");
        check(ms.selection().empty(), "selectLayer 不存在的图层 -> 空选择");

        // 27.8 框选 / 框取消：窗口覆盖方块但不覆盖 E
        ms.clearSelection();
        ms.selectWindow(QRectF(0, 0, 20, 20));   // 覆盖方块(0..10)，不覆盖 E(100..110)
        check(ms.isSelected(A) && ms.isSelected(B) && ms.isSelected(C) && ms.isSelected(D),
              "selectWindow 选中窗口内方块四边");
        check(!ms.isSelected(E), "selectWindow 不选中窗口外 E");
        ms.selectWindow(QRectF(95, 95, 20, 20), /*additive=*/true);  // 加选含 E 的窗口
        check(ms.isSelected(E), "selectWindow(additive) 并入窗口外 E");
        ms.deselectWindow(QRectF(0, 0, 20, 20));
        check(!ms.isSelected(A) && !ms.isSelected(D) && ms.isSelected(E),
              "deselectWindow 移除窗口内方块，保留 E");

        // 27.9 (取消)选择轮廓（toggle 语义）：全选中则整条移除，否则整条选中
        ms.clearSelection();
        ms.toggleContour(A);                       // 方块未选中 -> 整条选中
        check(ms.selection().size() == 4 && ms.isSelected(A) && ms.isSelected(D) && !ms.isSelected(E),
              "toggleContour 首次点击选中整条轮廓 4 边", QString::number(ms.selection().size()));
        ms.toggleContour(B);                       // 已全选中 -> 整条移除
        check(ms.selection().empty(), "toggleContour 二次点击整条取消");
        ms.setSelection({A, E});                   // 部分选中 -> 仍执行「选中整条」，并保留语义上为替换
        ms.toggleContour(C);
        check(ms.selection().size() == 4 && !ms.isSelected(E),
              "toggleContour 部分选中时切换为整条选中（替换选择集）", QString::number(ms.selection().size()));

        // 27.10 (取消)选择图层（toggle 语义）：dim 层 2 个图元
        ms.clearSelection();
        ms.toggleLayerOf(A);                       // dim 层未选中 -> 整层选中
        check(ms.selection().size() == 2 && ms.isSelected(A) && ms.isSelected(B),
              "toggleLayerOf 首次点击选中 dim 整层 2 个图元", QString::number(ms.selection().size()));
        ms.toggleLayerOf(B);                       // 已全选中 -> 整层取消
        check(ms.selection().empty(), "toggleLayerOf 二次点击整层取消");
        ms.toggleLayerOf(99999);                   // 无效种子 -> 不崩溃、选择集不变
        check(ms.selection().empty(), "toggleLayerOf 无效种子 -> 空选择（不崩溃）");
    }

    // =====================================================================
    qInfo().noquote() << QString("\n=== 28. 选择命令解释器（CLI 路径，验证 #57 注册与模型联动）===");
    {
        SketchModel mc;
        auto mkLine = [](double x1, double y1, double x2, double y2) {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
            return e;
        };
        int A = mc.addElement(mkLine(0,  0,  10,  0));  // 上边
        int B = mc.addElement(mkLine(10, 0,  10, 10));  // 右边
        mc.addElement(mkLine(10, 10, 0,  10));          // 下边
        mc.addElement(mkLine(0,  10, 0,  0));           // 左边（闭合方块 A-B-C-D）
        mc.addElement(mkLine(100, 100, 110, 100));      // 孤立线（不相连、不在 dim 图层）
        mc.updateElement(A, [](Element& e) { e.layer = "dim"; });
        mc.updateElement(B, [](Element& e) { e.layer = "dim"; });
        CommandInterpreter cc(&mc);

        check(cc.execute("select_all"), "CLI select_all 执行");
        check(mc.selection().size() == 5, "CLI select_all 选中 5 个图元", QString::number(mc.selection().size()));
        check(cc.execute("select_none"), "CLI select_none 执行");
        check(!mc.hasSelection(), "CLI select_none 清空选择集");

        check(cc.execute(QString("select_entity %1").arg(A)), "CLI select_entity <id> 执行");
        check(mc.isSelected(A) && mc.selectedId() == A, "CLI select_entity 选中指定图元");
        check(cc.execute(QString("deselect_entity %1").arg(A)), "CLI deselect_entity <id> 执行");
        check(!mc.isSelected(A), "CLI deselect_entity 移除指定图元");

        check(cc.execute(QString("select_contour %1").arg(A)), "CLI select_contour <id> 执行");
        check(mc.selection().size() == 4, "CLI select_contour 收集连通方块 4 边", QString::number(mc.selection().size()));

        check(cc.execute("select_layer dim"), "CLI select_layer <图层> 执行");
        check(mc.selection().size() == 2 && mc.isSelected(A) && mc.isSelected(B),
              "CLI select_layer 选中 dim 图层 2 个图元", QString::number(mc.selection().size()));

        check(cc.execute("invert"), "CLI invert 执行");
        check(mc.selection().size() == 3, "CLI invert 反选（非 dim 的 3 个图元）", QString::number(mc.selection().size()));

        // CLI toggle_contour / toggle_layer：(取消) 切换语义
        check(cc.execute(QString("toggle_contour %1").arg(A)), "CLI toggle_contour <id> 执行");
        check(mc.selection().size() == 4, "CLI toggle_contour 选中整条轮廓 4 边", QString::number(mc.selection().size()));
        check(cc.execute(QString("toggle_contour %1").arg(A)), "CLI toggle_contour 二次执行");
        check(mc.selection().empty(), "CLI toggle_contour 二次执行整条取消");
        check(cc.execute(QString("toggle_layer %1").arg(A)), "CLI toggle_layer <id> 执行");
        check(mc.selection().size() == 2 && mc.isSelected(A) && mc.isSelected(B),
              "CLI toggle_layer 选中 dim 整层 2 个图元", QString::number(mc.selection().size()));
        check(cc.execute(QString("toggle_layer %1").arg(B)), "CLI toggle_layer 二次执行");
        check(mc.selection().empty(), "CLI toggle_layer 二次执行整层取消");

        // 错误路径：参数缺失 / 非法 id / 未知命令
        check(!cc.execute("select_layer"), "CLI select_layer 缺参数 -> 失败");
        check(!cc.execute(QString("select_entity abc")), "CLI select_entity 非法 id -> 失败");
        check(!cc.execute("select_foo"), "CLI 未知选择命令 -> 失败");
    }

    qInfo().noquote() << QString("\n=== 29. 新建图纸：图框 + 标准标题栏生成（GB/T 14689 / GB/T 10609.1）===");
    {
        const QVector<TitleBlockField>& fields = titleBlockFields();
        check(fields.size() == 12, "标题栏栏目定义 == 12 项",
              QString("实际=%1").arg(fields.size()));

        for (int fi = 0; fi < 6; ++fi) {
            SheetFormat fmt = (SheetFormat)fi;
            for (int ori = 0; ori < 2; ++ori) {
                bool land = (ori == 0);
                SheetSpec spec = SheetSpec::make(fmt, land);
                auto els = buildSheetFrame(spec);
                QString tag = QString("A%1-%2").arg(fi).arg(land ? "横" : "纵");

                // 纸张外边界矩形 (0,0)-(W,H)
                const Element* paper = nullptr;
                const Element* frame = nullptr;
                const Element* tb = nullptr;
                for (const auto& e : els) {
                    if (e.type != ElementType::Rectangle) continue;
                    if (qFuzzyCompare(e.pts[0].x, 0.0) && qFuzzyCompare(e.pts[0].y, 0.0) &&
                        qFuzzyCompare(e.pts[1].x, spec.width) && qFuzzyCompare(e.pts[1].y, spec.height))
                        paper = &e;
                    if (e.layer == QStringLiteral("图框")   && e.lineWidth > 1.5) frame = &e;
                    if (e.layer == QStringLiteral("标题栏") && e.lineWidth > 1.5) tb = &e;
                }
                check(paper != nullptr, tag + " 存在纸张外边界矩形(0,0,W,H)");
                check(frame != nullptr, tag + " 存在图框矩形");
                if (frame) {
                    const double a = 25.0;
                    // GB/T 14689 表4：A0/A1/A2 的 c=10，A3/A4/A5 的 c=5（其余三边）
                    const bool big = (fmt == SheetFormat::A0 || fmt == SheetFormat::A1 ||
                                      fmt == SheetFormat::A2);
                    const double c = big ? 10.0 : 5.0;
                    check(qFuzzyCompare(frame->pts[0].x, a) && qFuzzyCompare(frame->pts[0].y, c) &&
                          qFuzzyCompare(frame->pts[1].x, spec.width - c) && qFuzzyCompare(frame->pts[1].y, spec.height - c),
                          tag + " 图框边距符合 GB/T 14689（左a=25；A0/A1/A2 的 c=10，A3/A4/A5 的 c=5）",
                          QString("frame=(%1,%2)-(%3,%4)")
                              .arg(frame->pts[0].x).arg(frame->pts[0].y)
                              .arg(frame->pts[1].x).arg(frame->pts[1].y));
                }
                check(tb != nullptr, tag + " 存在标题栏外框");

                // 12 个可编辑栏目（name 以 "tb:field:" 开头）—— 与 TitleBlockForm 检索契约一致。
                // 契约是「按 name 检索」，故校验 12 个 key 的集合与定义完全一致（每个恰好出现一次），
                // 不要求生成顺序与 titleBlockFields() 显示顺序相同（生成按图框物理布局排布）。
                QStringList keys;
                int emptyFields = 0;
                for (const auto& e : els) {
                    if (e.type == ElementType::Text && e.name.startsWith(QStringLiteral("tb:field:"))) {
                        keys.append(e.name.mid(9));
                        if (e.text.isEmpty()) ++emptyFields;
                    }
                }
                check(keys.size() == 12, tag + " 标题栏含 12 个可编辑栏目",
                      QString("实际=%1").arg(keys.size()));
                QSet<QString> keySet(keys.begin(), keys.end());
                bool setOk = (keySet.size() == 12);
                for (const auto& f : fields)
                    if (!keySet.contains(f.key)) setOk = false;
                check(setOk, tag + " 栏目 key 集合与定义完全一致（每个恰好一次，TitleBlockForm 按 name 检索）");
                check(emptyFields == 12, tag + " 栏目初始为空（可手动填写并随图纸保存）",
                      QString("空=%1").arg(emptyFields));

                // 布局不变量：解析标题栏内部竖分隔线，校验各列宽度，防止 A4/A5 等小尺寸下文字重叠
                if (tb) {
                    const double by0r = tb->pts[0].y;
                    const double by1r = tb->pts[1].y;
                    const double y2r = by0r + 24.0;
                    const double y3r = by0r + 36.0;
                    double xMidPos = 0.0, xNameDatePos = 0.0, xNumPos = 0.0;
                    int vFull = 0, vShort = 0, vScale = 0;
                    for (const auto& e : els) {
                        if (e.type != ElementType::Line || e.layer != QStringLiteral("标题栏")) continue;
                        if (e.pts.size() < 2) continue;
                        const double x0 = e.pts[0].x, x1 = e.pts[1].x;
                        const double y0 = e.pts[0].y, y1 = e.pts[1].y;
                        if (!qFuzzyCompare(x0, x1)) continue;                 // 只取竖线
                        if (qFuzzyCompare(y0, by0r) && qFuzzyCompare(y1, by1r)) { xMidPos = x0; ++vFull; }
                        else if (qFuzzyCompare(y0, by0r) && qFuzzyCompare(y1, y3r)) { xNameDatePos = x0; ++vShort; }
                        else if (qFuzzyCompare(y0, y2r) && qFuzzyCompare(y1, y3r)) { xNumPos = x0; ++vScale; }
                    }
                    check(vFull == 1 && vShort == 1 && vScale == 1,
                          tag + " 标题栏含 3 条竖分隔线（字段/签名、姓名/日期、图号/比例）");
                    if (vFull && vShort && vScale) {
                        const double bx0r = tb->pts[0].x, bx1r = tb->pts[1].x;
                        const double drawingW = xNumPos - bx0r;
                        const double scaleWrt = xMidPos - xNumPos;
                        const double nameWrt  = xNameDatePos - xMidPos;
                        const double dateWrt  = bx1r - xNameDatePos;
                        // A4 及以上保证充裕列宽；A5 幅面较小，阈值相应放宽
                        const bool largeFmt = (fmt != SheetFormat::A5);
                        const double minDrawing = largeFmt ? 45.0 : 30.0;
                        const double minScale   = largeFmt ? 30.0 : 20.0;
                        const double minSign    = largeFmt ? 18.0 : 12.0;
                        const double minDate    = largeFmt ? 18.0 : 12.0;
                        check(drawingW >= minDrawing, tag + " 图号列宽度 >= " + QString::number(minDrawing, 'f', 0) + "mm", QString::number(drawingW, 'f', 1));
                        check(scaleWrt >= minScale,   tag + " 比例列宽度 >= " + QString::number(minScale, 'f', 0) + "mm",  QString::number(scaleWrt, 'f', 1));
                        check(nameWrt >= minSign,     tag + " 签名列宽度 >= " + QString::number(minSign, 'f', 0) + "mm",   QString::number(nameWrt, 'f', 1));
                        check(dateWrt >= minDate,     tag + " 日期列宽度 >= " + QString::number(minDate, 'f', 0) + "mm",   QString::number(dateWrt, 'f', 1));
                    }
                }
            }
        }
    }

    // ============================================================
    // 30. 修改菜单（Modify）核心引擎
    //   覆盖 makeDelete / makeDuplicate / makeMoveCopy / makeRotate /
    //        makeScale / makeMirror / makeRevertDirection / makeLengthen /
    //        makeOffset / makeBreakDivide / makeFillet / makeExplode /
    //        makeAlign / makeStretch + applyChanges 写入流程
    // ------------------------------------------------------------
    qInfo().noquote() << QString("\n=== 30. 修改菜单（Modify）核心引擎与 CLI 注册 ===");
    {
        SketchModel mm;
        CommandInterpreter ci(&mm);
        // 先准备两个选中直线：(0,0)-(100,0) 与 (100,0)-(100,100)
        ci.execute("line 0,0 100,0");
        ci.execute("line 100,0 100,100");
        ci.execute("circle 50,50 20");
        ci.execute("rect 10,10 60,60");
        const int total = mm.elementCount();

        // --- 1) DeleteCommand ---
        mm.clearSelection();
        mm.setSelected(1);
        check(ci.execute("delete 1"), "delete 1 命令成功");
        check(mm.elementCount() == total - 1, "删除后总数 -1",
              QString("实际=%1").arg(mm.elementCount()));
        ci.execute("undo");
        check(mm.elementCount() == total, "undo 恢复删除",
              QString("实际=%1").arg(mm.elementCount()));

        // --- 2) Move-Copy（CLI: move dx dy） ---
        mm.setSelection({1, 2});
        check(ci.execute("move 10 5"), "move 10 5 命令成功");
        const Element* e1 = mm.element(1);
        check(e1 && qFuzzyCompare(e1->pts[0].x, 10.0) && qFuzzyCompare(e1->pts[0].y, 5.0),
              "move 后图元 1 起点 (+10, +5)");
        ci.execute("undo");

        // --- 3) Duplicate（CLI: duplicate） ---
        mm.setSelection({1});
        check(ci.execute("duplicate"), "duplicate 命令成功");
        check(mm.elementCount() == total + 1, "duplicate 后总数 +1",
              QString("实际=%1").arg(mm.elementCount()));
        const Element* dup = mm.element(total + 1);
        check(dup && qFuzzyCompare(dup->pts[0].x, 10.0) && qFuzzyCompare(dup->pts[0].y, 0.0),
              "duplicate 偏置 (10, 0)");
        ci.execute("undo");

        // --- 4) Rotate ---
        mm.setSelection({1});
        check(ci.execute("rotate 90"), "rotate 90 命令成功（默认绕选区质心）");
        const Element* er = mm.element(1);
        // 选区只有一条 (0,0)-(100,0)，质心 (50, 0)。绕 (50, 0) 逆时针旋转 90°（CCW）：
        //   (0,  0) 相对 (-50, 0) → (0, -50) → 绝对 (50, -50)
        //   (100,0) 相对 ( 50, 0) → (0,  50) → 绝对 (50,  50)
        check(er && std::fabs(er->pts[0].x - 50.0) < 1e-6 && std::fabs(er->pts[0].y + 50.0) < 1e-6 &&
              std::fabs(er->pts[1].x - 50.0) < 1e-6 && std::fabs(er->pts[1].y - 50.0) < 1e-6,
              "rotate 90°（质心 (50,0)）：(0,0)→(50,-50), (100,0)→(50,50)",
              QString("p0=(%1,%2) p1=(%3,%4)")
                  .arg(er ? er->pts[0].x : 0).arg(er ? er->pts[0].y : 0)
                  .arg(er ? er->pts[1].x : 0).arg(er ? er->pts[1].y : 0));
        ci.execute("undo");

        // --- 5) Scale ---
        mm.setSelection({3});  // 圆
        check(ci.execute("scale 2.0"), "scale 2.0 命令成功（默认绕选区质心）");
        const Element* esc = mm.element(3);
        check(esc && qFuzzyCompare(esc->radius, 40.0),
              "scale 2.0：圆半径 20 → 40",
              QString("实际=%1").arg(esc ? esc->radius : 0));
        ci.execute("undo");

        // --- 6) Mirror 水平 ---
        mm.setSelection({1});
        check(ci.execute("mirror h"), "mirror h 命令成功");
        const Element* em = mm.element(1);
        check(em && qFuzzyCompare(em->pts[0].y, 0.0) && qFuzzyCompare(em->pts[1].y, 0.0),
              "mirror h：直线 y 坐标不变（关于 y 中位数翻转 → y 不变）");
        ci.execute("undo");

        // --- 7) Revert Direction ---
        mm.setSelection({1});
        const Element* pre = mm.element(1);
        const double preX0 = pre->pts[0].x;
        check(ci.execute("revert"), "revert 命令成功");
        const Element* post = mm.element(1);
        check(post && qFuzzyCompare(post->pts[1].x, preX0),
              "revert：起点终点互换（终点 == 原起点 X）");
        ci.execute("undo");

        // --- 8) Lengthen（延长终点）---
        mm.setSelection({1});
        check(ci.execute("lengthen 1 20"), "lengthen 1 20 命令成功（终点 +20 mm）");
        const Element* el = mm.element(1);
        check(el && qFuzzyCompare(el->pts[1].x, 120.0),
              "lengthen 后终点 X = 120");
        ci.execute("undo");

        // --- 9) Offset 直线 ---
        mm.setSelection({1});
        check(ci.execute("offset 5"), "offset 5 命令成功（直线朝左侧偏移 5 mm）");
        const Element* eo = mm.element(1);
        check(eo && qFuzzyCompare(eo->pts[0].y, 5.0) && qFuzzyCompare(eo->pts[1].y, 5.0),
              "offset 5：直线 y +5（朝上偏移）");
        ci.execute("undo");

        // --- 10) Offset 圆 ---
        mm.setSelection({3});
        check(ci.execute("offset 10"), "offset 10 命令成功（圆半径 +10）");
        const Element* eoc = mm.element(3);
        check(eoc && qFuzzyCompare(eoc->radius, 30.0),
              "offset 10：圆半径 20 → 30");
        ci.execute("undo");

        // --- 11) BreakDivide 等分（divide id 3）---
        check(ci.execute("divide 1 4"), "divide 1 4 命令成功（4 等分）");
        check(mm.elementCount() == total + 3, "4 等分后总数 +3（新增 3 段）",
              QString("实际=%1").arg(mm.elementCount()));
        ci.execute("undo");

        // --- 12) Fillet 圆角 ---
        // 准备两条垂直直线 (0,0)-(10,0) 与 (0,0)-(0,10)
        // 注意：SketchModel::clear() 只清空图元链表，不重置 ID 分配器，
        //       所以后续新建图元的 id 会从上次的最大值继续递增 —— 必须取实际 id。
        mm.clear();
        ci.execute("line 0,0 10,0");
        ci.execute("line 0,0 0,10");
        const int fl1 = mm.elements().at(0).id;
        const int fl2 = mm.elements().at(1).id;
        check(ci.execute(QString("fillet %1 %2 2").arg(fl1).arg(fl2)),
              "fillet <id1> <id2> 2 命令成功（圆角 r=2）");
        // makeFillet 的语义：两条原线被删除，新增 2 条截短线 + 1 条圆弧 → 净剩 3 个图元
        check(mm.elementCount() == 3, "圆角后：2 原线删除 + 3 新增（2 截短线 + 1 圆弧）",
              QString("实际=%1").arg(mm.elementCount()));
        // 验证存在 1 个圆弧
        int arcCount = 0;
        for (const auto& e : mm.elements()) if (e.type == ElementType::Arc) ++arcCount;
        check(arcCount == 1, "圆角结果中恰好 1 个圆弧", QString("实际=%1").arg(arcCount));
        ci.execute("undo");

        // --- 13) Fillet 倒角（radius=0）---
        check(ci.execute(QString("fillet %1 %2 0").arg(fl1).arg(fl2)), "fillet radius=0 倒角命令成功");
        int lineCount = 0;
        for (const auto& e : mm.elements()) if (e.type == ElementType::Line) ++lineCount;
        check(lineCount == 3, "倒角后：2 截短线 + 1 连接直线（无圆弧），2 原线已删除",
              QString("直线数=%1").arg(lineCount));
        ci.execute("undo");

        // --- 14) Explode 矩形 ---
        mm.clear();
        ci.execute("rect 0,0 10,10");
        const int rId = mm.elements().at(0).id;
        mm.setSelection({rId});
        check(ci.execute("explode"), "explode 命令成功（矩形 → 4 条边）");
        check(mm.elementCount() == 4, "矩形分解为 4 条边",
              QString("实际=%1").arg(mm.elementCount()));
        ci.execute("undo");

        // --- 15) Explode 多边形 ---
        mm.clear();
        ci.execute("poly 0,0 5,0 5,5");
        const int pId = mm.elements().back().id;   // 取刚创建的那个（back），不是 at(0)
        mm.setSelection({pId});
        check(ci.execute("explode"), "explode 多边形成功");
        check(mm.elementCount() == 3, "三角形分解为 3 条边",
              QString("实际=%1").arg(mm.elementCount()));
        ci.execute("undo");

        // --- 16) Align ---
        mm.clear();
        ci.execute("rect 0,0 10,10");       // 左 0, 右 10
        ci.execute("rect 50,30 60,40");     // 左 50, 右 60
        const int al1 = mm.elements().at(0).id;
        const int al2 = mm.elements().at(1).id;
        mm.setSelection({al1, al2});
        check(ci.execute("align right"), "align right 命令成功（待对齐 → 与 ref 右对齐）");
        // align 以选区第一个为参考（ref），其余为待对齐（src）；验证两者右边界一致
        const Element* r1 = mm.element(al1);
        const Element* r2 = mm.element(al2);
        const double right1 = r1 ? std::max(r1->pts[0].x, r1->pts[1].x) : -1e9;
        const double right2 = r2 ? std::max(r2->pts[0].x, r2->pts[1].x) : -1e9;
        check(r1 && r2 && qFuzzyCompare(right1, right2),
              "align right：两个矩形右边界对齐（右=10）",
              QString("右1=%1 右2=%2").arg(right1).arg(right2));
        ci.execute("undo");

        // --- 17) Stretch ---
        mm.clear();
        ci.execute("line 0,0 100,0");
        const int stId = mm.elements().at(0).id;
        mm.setSelection({stId});
        check(ci.execute("stretch 100,0 150,0"), "stretch 100,0 150,0 命令成功");
        const Element* es = mm.element(stId);
        check(es && qFuzzyCompare(es->pts[1].x, 150.0),
              "stretch 后终点 X = 150",
              QString("实际=%1").arg(es ? es->pts[1].x : 0));
        ci.execute("undo");

        // --- 18) 直接调用 makeFillet 测试复杂逻辑 ---
        {
            std::vector<Element> pool;
            Element a = ElementFactory::create(ElementType::Line, "L1", &mm);
            a.id = 1;  // ElementFactory::create 默认 id=0，测试需显式赋 id 供 makeFillet 查找
            a.pts = {Vec3(0, 0), Vec3(10, 0)};
            Element b = ElementFactory::create(ElementType::Line, "L2", &mm);
            b.id = 2;
            b.pts = {Vec3(0, 0), Vec3(0, 10)};
            pool.push_back(a); pool.push_back(b);
            auto changes = modify::makeFillet(1, 2, 3.0, pool);
            check(!changes.empty(), "makeFillet（直接 API）产出非空 Change 列表");
            int arcCount = 0, rmCount = 0;
            for (const auto& c : changes) {
                if (c.remove) ++rmCount;
                if (c.replacement.type == ElementType::Arc) ++arcCount;
            }
            check(rmCount == 2, "makeFillet 标记 2 个原线删除", QString("实际=%1").arg(rmCount));
            check(arcCount == 1, "makeFillet 产出 1 个圆弧", QString("实际=%1").arg(arcCount));
        }

        // --- 19) makeAlign/直接 makeRotate 直接 API ---
        {
            std::vector<Element> src;
            Element l = ElementFactory::create(ElementType::Line, "L", &mm);
            l.pts = {Vec3(0, 0), Vec3(100, 0)};
            src.push_back(l);
            auto cs = modify::makeRotate(src, Vec3(0, 0), 90);
            check(!cs.empty() && cs[0].replacement.pts[1].y > 99.9,
                  "makeRotate(90°): (100, 0) → (0, 100)",
                  QString("实际=%1").arg(cs.empty() ? 0.0 : cs[0].replacement.pts[1].y));
        }

        // --- 20) makeMirror 自定义直线 ---
        {
            std::vector<Element> src;
            Element l = ElementFactory::create(ElementType::Line, "L", &mm);
            l.pts = {Vec3(0, 1), Vec3(10, 1)};
            src.push_back(l);
            // 关于直线(0,0)→(1,0)（即 x 轴）翻转：(0,1)→(0,-1), (10,1)→(10,-1)
            auto cs = modify::makeMirror(src, false, Vec3(0, 0), Vec3(1, 0));
            check(!cs.empty() && qFuzzyCompare(cs[0].replacement.pts[0].y, -1.0),
                  "makeMirror 关于 x 轴：y=1 → y=-1",
                  QString("实际=%1").arg(cs.empty() ? 0.0 : cs[0].replacement.pts[0].y));
        }

        // --- 20b) makeMirror 点镜像：圆 → 圆心关于点对称，半径不变，新增副本 ---
        {
            std::vector<Element> src;
            Element c = ElementFactory::create(ElementType::Circle, "C", &mm);
            c.pts = {Vec3(10, 0, 0)}; c.radius = 7;
            c.layer = "2"; c.lineStyle = LineStyle::Dashed; c.color = QColor(255, 0, 0);
            src.push_back(c);
            // 关于 (0,0) 点对称：圆心 (10,0) → (-10,0)
            auto cs = modify::makeMirror(src, true, Vec3(0, 0));
            check(!cs.empty() && cs[0].id < 0, "点镜像：生成新增副本（id=-1）");
            const Element& m = cs[0].replacement;
            check(qFuzzyCompare(m.pts[0].x, -10.0) && qFuzzyCompare(m.pts[0].y, 0.0),
                  "点镜像圆：圆心 (10,0)→(-10,0)",
                  QString("cx=%1").arg(m.pts[0].x));
            check(qFuzzyCompare(m.radius, 7.0), "点镜像圆：半径不变 (7)");
            check(m.layer == "2" && m.lineStyle == LineStyle::Dashed && m.color == QColor(255, 0, 0),
                  "点镜像：图层/线型/颜色属性完整继承");
        }

        // --- 20c) makeMirror 线镜像：圆弧起止角按 2β−θ 反射（β = 镜像轴世界角）---
        {
            std::vector<Element> src;
            Element a = ElementFactory::create(ElementType::Arc, "A", &mm);
            a.pts = {Vec3(0, 0, 0)}; a.radius = 5; a.startAngle = 30; a.endAngle = 120;
            src.push_back(a);
            // 镜像轴取 x 轴 (0,0)→(1,0)，β=0 → 角度反射 θ'=−θ
            auto cs = modify::makeMirror(src, false, Vec3(0, 0), Vec3(1, 0));
            const Element& m = cs[0].replacement;
            check(qFuzzyCompare(m.startAngle, -30.0) && qFuzzyCompare(m.endAngle, -120.0),
                  "线镜像弧：起止角 30/120 → -30/-120（镜轴 x 轴，β=0）",
                  QString("s=%1 e=%2").arg(m.startAngle).arg(m.endAngle));
        }

        // --- 20d) makeMirror 线镜像：椭圆长轴角按 2β−θ 反射 ---
        {
            std::vector<Element> src;
            Element e = ElementFactory::create(ElementType::Ellipse, "E", &mm);
            e.pts = {Vec3(0, 0, 0)}; e.radius = 8; e.radiusY = 4; e.rotation = 30;
            src.push_back(e);
            // 镜像轴取 y 轴 (0,0)→(0,1)，β=90 → 角度反射 θ'=180−θ
            auto cs = modify::makeMirror(src, false, Vec3(0, 0), Vec3(0, 1));
            const Element& m = cs[0].replacement;
            check(qFuzzyCompare(m.rotation, 150.0),
                  "线镜像椭圆：长轴角 30 → 150（镜轴 y 轴，β=90）",
                  QString("rot=%1").arg(m.rotation));
            check(qFuzzyCompare(m.radius, 8.0) && qFuzzyCompare(m.radiusY, 4.0),
                  "线镜像椭圆：半轴不变");
        }

        // --- 20e) makeMirror 点镜像：椭圆长轴角 +180，半轴不变 ---
        {
            std::vector<Element> src;
            Element e = ElementFactory::create(ElementType::Ellipse, "E", &mm);
            e.pts = {Vec3(0, 0, 0)}; e.radius = 8; e.radiusY = 4; e.rotation = 30;
            src.push_back(e);
            auto cs = modify::makeMirror(src, true, Vec3(0, 0));
            const Element& m = cs[0].replacement;
            check(qFuzzyCompare(m.rotation, 210.0),
                  "点镜像椭圆：长轴角 30 → 210（绕点旋转 180°）",
                  QString("rot=%1").arg(m.rotation));
        }

        // --- 20f) makeMirror 线镜像：剖面区域（Hatch）边界 + 内孔 + 剖面线角同步反射 ---
        {
            std::vector<Element> src;
            Element h = ElementFactory::create(ElementType::Hatch, "H", &mm);
            h.pts = {Vec3(0, 2), Vec3(4, 2), Vec3(4, 6), Vec3(0, 6)};  // 边界
            h.holes = {{ Vec3(1, 3), Vec3(2, 3), Vec3(2, 4), Vec3(1, 4) }};  // 内孔
            h.rotation = 45;  // 剖面线方向角
            src.push_back(h);
            // 关于 x 轴 (0,0)→(1,0) 镜像：y → -y；剖面线角 β=0 → 45 → -45
            auto cs = modify::makeMirror(src, false, Vec3(0, 0), Vec3(1, 0));
            const Element& m = cs[0].replacement;
            check(qFuzzyCompare(m.pts[0].y, -2.0) && qFuzzyCompare(m.holes[0][0].y, -3.0),
                  "线镜像剖面：边界与内孔 y 同步取反",
                  QString("by=%1 hy=%2").arg(m.pts[0].y).arg(m.holes[0][0].y));
            check(qFuzzyCompare(m.rotation, -45.0),
                  "线镜像剖面：剖面线方向角 45 → -45",
                  QString("rot=%1").arg(m.rotation));
        }

        // --- 21) makeExplode 折线 ---
        {
            mm.clear();
            // 首末点重合 → 闭合折线（4 个节点 / 4 段）。开放折线 pts 数 n 只有 n-1 段。
            ci.execute("polyline 0,0 10,0 10,10 0,10 0,0");
            std::vector<Element> src;
            for (const auto& e : mm.elements()) if (e.type == ElementType::Polyline) src.push_back(e);
            check(!src.empty(), "已创建折线供 explode");
            auto cs = modify::makeExplode(src);
            int newSegs = 0;
            for (const auto& c : cs) if (!c.remove && c.replacement.type == ElementType::Line) ++newSegs;
            check(newSegs == 4, "折线分解为 4 条独立直线段",
                  QString("实际=%1").arg(newSegs));
        }

        // --- 22) 命令注册可发现 ---
        QStringList names = CommandRegistry::instance().exactNames();
        static const QStringList kModifyCmds = {
            "move", "move_copy", "duplicate", "rotate", "scale", "mirror",
            "revert", "revert_direction", "lengthen", "offset", "break_at",
            "divide", "fillet", "explode", "align", "stretch"
        };
        int hit = 0;
        for (const QString& n : kModifyCmds)
            if (names.contains(n)) ++hit;
        check(hit == (int)kModifyCmds.size(),
              QString("16 个 modify_* 命令全部注册到 CommandRegistry（hit=%1/%2）")
                  .arg(hit).arg(kModifyCmds.size()));
    }

    qInfo().noquote() << "\n=== 31. 飞机三面图模板 ===";
    {
        SketchModel mm;
        CommandInterpreter cii(&mm);
        const int before = mm.elementCount();
        check(cii.execute("aircraft_views"), "aircraft_views 命令成功执行");
        check(mm.elementCount() >= before + 30,
              "生成至少 30 个图元",
              QString("实际=%1").arg(mm.elementCount() - before));

        bool hasLine = false, hasCircle = false, hasArc = false, hasDim = false;
        for (const auto& e : mm.elements()) {
            if (e.type == ElementType::Line) hasLine = true;
            if (e.type == ElementType::Circle) hasCircle = true;
            if (e.type == ElementType::Arc) hasArc = true;
            if (e.type == ElementType::Dimension) {
                hasDim = true;
                if (e.text.contains("38.90") && e.text.contains("m")) {
                    check(true, "标注文本包含机身总长 38.90 m");
                }
            }
        }
        check(hasLine,   "三面图包含直线");
        check(hasCircle, "三面图包含圆");
        check(hasArc,    "三面图包含圆弧");
        check(hasDim,    "三面图包含尺寸标注");

        // 参数化 API：修改翼展后重新生成，图元数量应保持不变
        aircraft::AircraftViewsParams p;
        p.wingspan = 40.0;
        auto elems = aircraft::buildThreeViews(p, &mm);
        check(!elems.empty(), "buildThreeViews 返回非空图元列表");
        int dimCount = 0;
        for (const auto& e : elems)
            if (e.type == ElementType::Dimension) ++dimCount;
        check(dimCount >= 6, "至少生成 6 个尺寸标注",
              QString("实际=%1").arg(dimCount));

        // 一次 undo 应能完整撤掉整个三面图
        check(mm.canUndo(), "生成后存在可撤销事务");
        cii.execute("undo");
        check(mm.elementCount() == before,
              "undo 后模型恢复为空",
              QString("实际=%1").arg(mm.elementCount()));
    }

    qInfo().noquote() << "\n=== 32. 文件格式互操作（JSON / DXF / STEP / DWG）与绘图比例 ===";
    {
        using namespace SketchCAD::cadfmt;

        // --- 32.1 扩展名 → 格式分派（另存为/打开的唯一判据）---
        check(formatFromSuffix("json") == CadFormat::Json, "*.json → 草图 JSON");
        check(formatFromSuffix("dxf")  == CadFormat::Dxf,  "*.dxf → DXF");
        check(formatFromSuffix("DXF")  == CadFormat::Dxf,  "*.DXF 大小写不敏感 → DXF");
        check(formatFromSuffix("step") == CadFormat::Step, "*.step → STEP");
        check(formatFromSuffix("stp")  == CadFormat::Step, "*.stp → STEP（别名）");
        check(formatFromSuffix("dwg")  == CadFormat::Dwg,  "*.dwg → DWG");
        check(formatFromSuffix("txt")  == CadFormat::Unknown, "未知后缀 → Unknown");
        check(formatFromPath("C:/tmp/my part.DXF") == CadFormat::Dxf, "按完整路径判定扩展名");
        check(defaultSuffix(CadFormat::Step) == "step", "STEP 默认扩展名为 step");
        check(canImport(CadFormat::Json) && canImport(CadFormat::Dxf) && !canImport(CadFormat::Step),
              "可读回：JSON / DXF；STEP 只写");

        // --- 32.2 构造一张含各类图元的图纸 ---
        std::vector<Element> src;
        src.push_back(ElementFactory::createLine(Vec3(0, 0, 0), Vec3(100, 0, 0), nullptr));
        src.push_back(ElementFactory::createCircle(Vec3(50, 50, 0), 25.0, nullptr));
        src.push_back(ElementFactory::createArc(Vec3(0, 0, 0), 30.0, 0.0, 90.0, nullptr));
        src.push_back(ElementFactory::createRect(Vec3(200, 0, 0), Vec3(260, 40, 0), nullptr));
        src.push_back(ElementFactory::createEllipse(Vec3(300, 100, 0), 40.0, 20.0, 30.0, nullptr));
        src.push_back(ElementFactory::createPolygon({Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0)}, nullptr));
        src.push_back(ElementFactory::createSpline({Vec3(0, 200, 0), Vec3(50, 260, 0), Vec3(100, 180, 0)}, nullptr));
        src.push_back(ElementFactory::createPolyline({Vec3(0, 0, 0), Vec3(20, 0, 0), Vec3(20, 20, 0)},
                                                     {false, true}, false, nullptr));
        src.push_back(ElementFactory::createText(Vec3(10, 300, 0), QStringLiteral("SketchCAD"), 3.5));
        check(src.size() == 9, "测试图纸含 9 个图元（线/圆/弧/矩形/椭圆/多边形/样条/折线/文本）");

        // --- 32.3 DXF 写出 ---
        // toDxf 返回 QByteArray：DXF 是「组码行 + 值行」的字节格式，编码由文件内
        // $DWGCODEPAGE 声明（R12 不支持 UTF-8）。纯 ASCII 图纸用 ANSI_1252，字节即 ASCII。
        QString derr;
        const QByteArray dxfB = toDxf(src, &derr);
        check(!dxfB.isEmpty(), "toDxf 返回非空", derr);
        check(dxfB.startsWith("0\r\nSECTION"), "DXF 以 SECTION 开头");
        check(dxfB.contains("\r\nENTITIES\r\n"), "DXF 含 ENTITIES 段");
        check(dxfB.trimmed().endsWith("EOF"), "DXF 以 EOF 结尾");
        check(dxfB.contains("\r\n$ACADVER\r\n"), "DXF 头含 $ACADVER 版本声明");
        check(dxfB.contains("\r\nAC1009\r\n"), "DXF 声明为 R12（AC1009），与实际使用的实体自洽");
        check(dxfB.contains("\r\n$DWGCODEPAGE\r\n"), "DXF 头含 $DWGCODEPAGE 编码声明");
        check(dxfB.contains("\r\nANSI_1252\r\n"), "纯 ASCII 图纸声明 ANSI_1252（字节即 ASCII）");
        check(dxfB.contains("\r\nLTYPE\r\n"), "DXF 含 LTYPE 表（CONTINUOUS 线型定义）");
        check(dxfB.contains("\r\nLAYER\r\n"), "DXF 含 LAYER 表（图层登记）");
        // 断言全部针对 ASCII 内容，此处按 latin1 取回文本视图即可 1:1 对应字节
        const QString dxf = QString::fromLatin1(dxfB);
        const auto cnt = [&dxf](const QString& t) { return dxf.count("\r\n0\r\n" + t + "\r\n"); };
        check(cnt("LINE")  == 1, "DXF LINE 实体数 = 1",        QString("实际=%1").arg(cnt("LINE")));
        check(cnt("CIRCLE") == 1, "DXF CIRCLE 实体数 = 1",      QString("实际=%1").arg(cnt("CIRCLE")));
        check(cnt("ARC")   == 1, "DXF ARC 实体数 = 1",          QString("实际=%1").arg(cnt("ARC")));
        check(cnt("TEXT")  == 1, "DXF TEXT 实体数 = 1",         QString("实际=%1").arg(cnt("TEXT")));
        check(cnt("POLYLINE") == 5,
              "DXF POLYLINE = 5（矩形 / 多边形 / 椭圆采样 / 样条采样 / 含弧折线）",
              QString("实际=%1").arg(cnt("POLYLINE")));
        check(cnt("VERTEX") > 0 && cnt("SEQEND") == 5,
              "R12 多段线用 POLYLINE/VERTEX/SEQEND 结构（SEQEND 数 = 多段线数）",
              QString("VERTEX=%1 SEQEND=%2").arg(cnt("VERTEX")).arg(cnt("SEQEND")));

        // 回归守卫：DXF 是「组码行 + 值行」严格交替的文本格式，组码的值里绝不能含换行。
        // 标注数字是双行文本（"38.90 m\n(127.62 ft)"），曾因整段写进组码 1 而把整份 DXF
        // 写坏（值行被劈成两行 → 后续组码全部错位、文件不可解析）。这个缺陷自己的
        // 往返测试查不出来，是第三方解析器（ezdxf）读文件时报错才暴露的。
        {
            std::vector<Element> two;
            two.push_back(ElementFactory::createLine(Vec3(0, 0, 0), Vec3(10, 0, 0), nullptr));
            two.push_back(ElementFactory::createText(Vec3(5, 5, 0),
                                                    QStringLiteral("38.90 m\n(127.62 ft)"), 3.5));
            const QString d2 = QString::fromLatin1(toDxf(two, nullptr));
            const QStringList ln = d2.split(QStringLiteral("\r\n"));
            bool strict = !ln.isEmpty() && (ln.size() % 2 == 0);
            for (int i = 0; strict && i < ln.size(); i += 2) {
                bool ok = false;
                ln.at(i).trimmed().toInt(&ok);
                if (!ok) strict = false;
            }
            check(strict, "DXF 组码/值严格交替（多行文本不会写坏文件）",
                  QStringLiteral("总行数=%1").arg(ln.size()));
            check(d2.count("\r\n0\r\nTEXT\r\n") == 2,
                  "双行标注文本导出为 2 个 TEXT 实体（按行拆分）",
                  QStringLiteral("实际=%1").arg(d2.count("\r\n0\r\nTEXT\r\n")));
        }

        // --- 32.4 DXF 回读往返 ---
        std::vector<Element> back;
        QString ierr;
        check(fromDxf(dxfB, back, &ierr), "fromDxf 能解析本程序写出的 DXF", ierr);
        check(back.size() == 9, "回读得到 9 个图元（含 TEXT）",
              QString("实际=%1").arg(back.size()));
        const Element* bl = nullptr; const Element* bc = nullptr; const Element* ba = nullptr;
        for (const auto& e : back) {
            if (e.type == ElementType::Line   && !bl) bl = &e;
            if (e.type == ElementType::Circle && !bc) bc = &e;
            if (e.type == ElementType::Arc    && !ba) ba = &e;
        }
        check(bl && std::fabs(bl->pts[0].x) < 1e-6 && std::fabs(bl->pts[1].x - 100.0) < 1e-6,
              "回读直线的两端点与原件一致");
        check(bc && std::fabs(bc->radius - 25.0) < 1e-6 && std::fabs(bc->pts[0].x - 50.0) < 1e-6,
              "回读圆的圆心与半径一致");
        check(ba && std::fabs(ba->startAngle) < 1e-6 && std::fabs(ba->endAngle - 90.0) < 1e-6,
              "回读圆弧的起止角一致");
        int closedPolys = 0;
        for (const auto& e : back)
            if (e.type == ElementType::Polyline && e.closed) ++closedPolys;
        check(closedPolys == 3, "回读保留闭合标志（矩形 / 多边形 / 椭圆采样多段线）",
              QString("实际=%1").arg(closedPolys));

        // --- 32.5 DXF 异常输入 ---
        std::vector<Element> dummy;
        QString perr;
        check(!fromDxf(QByteArray("完全不是 DXF"), dummy, &perr), "非 DXF 文本被拒绝");
        check(!fromDxf(QByteArray("0\r\nSECTION\r\n2\r\nHEADER\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n"),
                       dummy, &perr), "无 ENTITIES 段的 DXF 被拒绝");
        check(!fromDxf(QByteArray("0\r\nSECTION\r\n2\r\nENTITIES\r\n0\r\nENDSEC\r\n0\r\nEOF\r\n"),
                       dummy, &perr), "空 ENTITIES 段被拒绝");

        // --- 32.6 STEP 写出（ISO 10303-21 / AP214 曲线集）---
        StepStats st;
        QString serr;
        const QString step = toStep(src, &st, &serr);
        check(!step.isEmpty(), "toStep 返回非空", serr);
        check(step.startsWith("ISO-10303-21;"), "STEP 以 ISO-10303-21 声明开头");
        check(step.contains("HEADER;") && step.contains("FILE_SCHEMA(('AUTOMOTIVE_DESIGN"),
              "STEP 头声明 AP214 应用协议");
        check(step.contains("DATA;") && step.contains("END-ISO-10303-21;"),
              "STEP 含 DATA 段与结束标记");
        check(step.contains("SI_UNIT(.MILLI.,.METRE.)"), "STEP 单位声明为毫米");
        check(step.contains("GEOMETRIC_CURVE_SET"), "STEP 含 GEOMETRIC_CURVE_SET");
        check(step.contains("SHAPE_REPRESENTATION"), "STEP 含 SHAPE_REPRESENTATION");
        check(step.contains("CARTESIAN_POINT"), "STEP 含 CARTESIAN_POINT");
        check(step.contains("=CIRCLE('',"),      "STEP 含 CIRCLE 实体");
        check(step.contains("TRIMMED_CURVE"),    "STEP 含 TRIMMED_CURVE（直线 / 圆弧 / 标注线段）");
        check(step.contains("=ELLIPSE('',"),     "STEP 含 ELLIPSE 实体");
        check(step.contains("POLYLINE('',"),     "STEP 含 POLYLINE 实体");
        check(st.curves == 8, "STEP 写出 8 条曲线实体", QString("实际=%1").arg(st.curves));
        check(st.skippedText == 1, "1 个文本注记不随 STEP 曲线集导出",
              QString("实际=%1").arg(st.skippedText));
        // 第三方解析器（steputils）暴露的问题：ISO 10303-21 的聚合列表**不允许尾随逗号**，
        // `(#1,#2,)` 会被严格解析器判为 "Unexpected parameter ')'"。自研读入器不校验语法，
        // 故必须显式守住这条（等价检查见 tools/validate_step.py）。
        check(!step.contains(",)\r\n") && !step.contains(",);"),
              "STEP 聚合列表无尾随逗号（ISO 10303-21 禁止）");
        check(step.count('(') == step.count(')'), "STEP 括号配平",
              QString("%1 ( vs %2 )").arg(step.count('(')).arg(step.count(')')));

        // --- 32.7 DWG 兼容写出 ---
        QString werr;
        check(toDwg(src, &werr) == dxfB,
              "DWG 兼容写出内容与 DXF 一致（原生二进制 DWG 需 ODA SDK，程序如实说明）");

        // --- 32.7b 中文 DXF 编码：R12 不支持 UTF-8，必须转 GBK 并声明 ANSI_936 ---
        // 这是第三方解析器（ezdxf）暴露的问题：若按 UTF-8 写出，中文在 R12 里会乱码。
        {
            std::vector<Element> cn;
            cn.push_back(ElementFactory::createText(Vec3(0, 0, 0), QStringLiteral("图框 比例 1:1"), 3.5));
            QString cerr;
            const QByteArray cdxf = toDxf(cn, &cerr);
            check(!cdxf.isEmpty(), "含中文的图纸仍能写出 DXF", cerr);
            check(cdxf.contains("\r\nANSI_936\r\n"),
                  "含中文时声明 $DWGCODEPAGE = ANSI_936（GBK）");
            check(!cdxf.contains(QStringLiteral("图框").toUtf8()),
                  "中文未按 UTF-8 写出（R12 不支持 UTF-8）");
            std::vector<Element> cback;
            QString cierr;
            check(fromDxf(cdxf, cback, &cierr), "中文 DXF 能被本程序读回", cierr);
            check(cback.size() == 1 && cback[0].text == QStringLiteral("图框 比例 1:1"),
                  "中文往返无损（GBK 编码 + 解码自动识别）",
                  cback.empty() ? QStringLiteral("无图元") : cback[0].text);
        }

        // --- 32.8 绘图比例：默认 1:1，改比例即按比例缩放已有图元 ---
        SketchModel sm;
        CommandInterpreter si(&sm);
        check(sm.scaleRatio() == 1.0 && sm.scaleLabel() == QStringLiteral("1:1"),
              "模型默认绘图比例 1:1");
        si.execute(QStringLiteral("line 0,0 100,0"));
        const auto len = [&sm] {
            const Element& e = sm.elements().at(0);
            return std::fabs(e.pts[1].x - e.pts[0].x);
        };
        check(std::fabs(len() - 100.0) < 1e-9, "1:1 下 100mm 直线长度为 100");
        sm.setScaleRatio(0.5, false);        // 1:1 → 1:2
        check(std::fabs(len() - 50.0) < 1e-9, "比例改为 1:2 后直线缩短为 50mm",
              QString("实际=%1").arg(len()));
        check(sm.scaleLabel() == QStringLiteral("1:2"), "比例标签为 1:2");
        sm.setScaleRatio(2.0, false);        // 1:2 → 2:1（相对当前比例放大 4 倍）
        check(std::fabs(len() - 200.0) < 1e-9, "比例改为 2:1 后直线长度相对放大 4 倍",
              QString("实际=%1").arg(len()));

        // --- 32.9 撤销/重做时「比例 + 几何」必须一起回退 ---
        SketchModel um;
        CommandInterpreter ui(&um);
        ui.execute(QStringLiteral("line 0,0 100,0"));
        um.setScaleRatio(0.5, true);
        check(um.canUndo(), "修改比例产生可撤销事务");
        ui.execute(QStringLiteral("undo"));
        {
            const Element& e = um.elements().at(0);
            check(std::fabs((e.pts[1].x - e.pts[0].x) - 100.0) < 1e-9, "撤销后几何恢复原长度");
        }
        check(um.scaleRatio() == 1.0 && um.scaleLabel() == QStringLiteral("1:1"),
              "撤销后比例同步回退到 1:1（几何与比例一致，不会二次错误缩放）",
              QString("实际=%1").arg(um.scaleLabel()));
        ui.execute(QStringLiteral("redo"));
        check(std::fabs(um.scaleRatio() - 0.5) < 1e-9, "重做后比例回到 1:2",
              QString("实际=%1").arg(um.scaleLabel()));

        // --- 32.10 读档专用：只恢复比例、不缩放几何 ---
        SketchModel vm;
        CommandInterpreter vi(&vm);
        vi.execute(QStringLiteral("line 0,0 100,0"));
        vm.setScaleRatioValue(0.5);
        {
            const Element& e = vm.elements().at(0);
            check(std::fabs((e.pts[1].x - e.pts[0].x) - 100.0) < 1e-9,
                  "setScaleRatioValue 只改比例、不动几何（读档不二次缩放）");
        }
        check(vm.scaleLabel() == QStringLiteral("1:2"), "读档比例标签恢复为 1:2");

        // --- 32.10b 改比例后「标注数值」必须仍是实物尺寸（GB/T 14690）---
        // 比例只改变图形大小，尺寸标注的数字永远等于实物尺寸。
        // 本模型里 pts 就是"图形坐标"，所以改比例缩放几何后，标注实测距离会跟着变，
        // 必须补回比例因子，否则 1:1 的 100mm 在 1:2 图纸上会标注成 50。
        {
            SketchModel dm2;
            CommandInterpreter di2(&dm2);
            di2.execute(QStringLiteral("line 0,0 100,0"));
            di2.execute(QStringLiteral("dim aligned 0,0 100,0"));
            // 模拟渲染/导出路径：必须用模型给出的样式（dimStyle() 已带当前比例）。
            // 若这里用默认 dim::Style()（比例 1.0），读数就会随图形一起缩小——这正是要防的。
            const auto dimLabel = [&dm2] {
                for (const auto& e : dm2.elements())
                    if (e.type == ElementType::Dimension) return dim::build(e, dm2.dimStyle()).label;
                return QString();
            };
            check(dimLabel() == QStringLiteral("100.00"),
                  "1:1 下 100mm 直线的标注读数为 100.00", dimLabel());
            dm2.setScaleRatio(0.5, false);
            check(dimLabel() == QStringLiteral("100.00"),
                  "比例改 1:2 后标注读数仍为实物 100.00（不随图形缩小）", dimLabel());
            dm2.setScaleRatio(200.0 / 100.0, false);
            check(dimLabel() == QStringLiteral("100.00"),
                  "比例改 2:1 后标注读数仍为实物 100.00（不随图形放大）", dimLabel());
            // 半径标注同样是长度量，也要换算；角度标注与比例无关，不应换算
            SketchModel rm;
            CommandInterpreter ri(&rm);
            ri.execute(QStringLiteral("circle 0,0 20"));
            ri.execute(QStringLiteral("dim radius 1"));
            const auto radLabel = [&rm] {
                for (const auto& e : rm.elements())
                    if (e.type == ElementType::Dimension) return dim::build(e, rm.dimStyle()).label;
                return QString();
            };
            check(radLabel() == QStringLiteral("R20.00"), "1:1 下 R20 圆标注读数为 R20.00", radLabel());
            rm.setScaleRatio(0.5, false);
            check(radLabel() == QStringLiteral("R20.00"),
                  "比例改 1:2 后半径标注仍为实物 R20.00", radLabel());
            // 防漂移守卫：标注缓存的 radius 必须与 pts 几何同步缩放。
            // （半径/直径/弧长标注用 e.radius，若只缩 pts 不缩 radius 就会算出 R40）
            for (const auto& e : rm.elements()) {
                if (e.type != ElementType::Dimension || e.pts.size() < 2) continue;
                const double geoR = std::hypot(e.pts[1].x - e.pts[0].x, e.pts[1].y - e.pts[0].y);
                check(std::fabs(geoR - e.radius) < 1e-9,
                      "缩放后标注缓存的 radius 与 pts 几何一致（两份数据不漂移）",
                      QStringLiteral("geoR=%1 radius=%2").arg(geoR).arg(e.radius));
            }

            // 导出路径也要带比例：DXF 里组码 1（TEXT 值）必须写实物尺寸。
            // 注意坐标用 6 位小数写出（"100.000000"），所以必须按整行精确匹配，
            // 否则 "100.00" 会被坐标行误命中。
            const QByteArray dx = toDxf(dm2.elements(), nullptr, dm2.scaleRatio());
            check(dx.contains("\r\n1\r\n100.00\r\n"),
                  "DXF 导出的标注数字是实物尺寸（比例已随导出传入）",
                  QStringLiteral("查 \"\\r\\n1\\r\\n100.00\\r\\n\""));
            // 反面守卫：1:2 图上几何是 50，若忘传比例就会导出成 50.00
            check(!dx.contains("\r\n1\r\n50.00\r\n"),
                  "DXF 未把标注数字写成缩放后的图纸尺寸（不传比例会写成 50.00）");
        }

        // --- 32.11 标题栏「比例」栏位 ---
        bool hasScaleField = false;
        for (const auto& e : buildSheetFrame(SheetSpec::make(SheetFormat::A3, true)))
            if (e.name == QStringLiteral("tb:field:scale")) hasScaleField = true;
        check(hasScaleField, "标题栏含「比例」栏（tb:field:scale），新建/改比例时自动填写");

        // --- 32.12 样例落盘（不影响断言）：便于用第三方 CAD / DXF 解析器独立校验 ---
        const auto dumpSample = [](const QString& path, const QByteArray& body) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(body) > 0;
        };
        const bool okDxf  = dumpSample(QStringLiteral("build/selftest_formats.dxf"), dxfB);
        const bool okStep = dumpSample(QStringLiteral("build/selftest_formats.step"), step.toUtf8());
        check(okDxf,  "混合图元样例已写出 build/selftest_formats.dxf（供第三方解析器校验）");
        check(okStep, "混合图元样例已写出 build/selftest_formats.step");
        // 中文样例（GBK 编码）：供 ezdxf 等第三方解析器验证中文不乱码
        std::vector<Element> cnSample;
        for (const auto& e : buildSheetFrame(SheetSpec::make(SheetFormat::A3, true)))
            cnSample.push_back(e);
        const bool okCn = dumpSample(QStringLiteral("build/selftest_cn.dxf"), toDxf(cnSample, nullptr));
        check(okCn, "中文图框样例已写出 build/selftest_cn.dxf（GBK / ANSI_936，供第三方校验中文）");
    }

    qInfo().noquote() << QString("\n================ 结果: %1 通过 / %2 失败 ================")
                             .arg(g_pass).arg(g_fail);
    return g_fail == 0 ? 0 : 1;
}
