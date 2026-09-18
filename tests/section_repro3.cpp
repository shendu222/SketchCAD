// 剖面填充「三处新异常」复现诊断（2026-09-18 用户截图）
// ----------------------------------------------------------------------------
// 共同模式：分割线/曲线【超出封闭边界】（悬出段），左侧填充正确、右侧异常。
//   S1(图1)：矩形 + 左凸曲线(上下端超出矩形)。填左 OK；填右 → 多画直线且越界。
//   S2(图2)：矩形 + 两条超出上下边的竖直分割线(三区)。填左 OK；填右 → 跨中间+右两区，
//            且中间分割线被删除。
//   S3(图3)：平行四边形 + 斜分割线(两端超出)。填左 OK；填右 → 分割线未生效(跨线填充)。
//
// 每个场景都走真实应用序列：pick 左 → makeSectionChangeForPick → applyChanges 落盘 →
// pick 右 → 落盘 → 检查模型状态（分割线存活 / Hatch 边界不越界 / 无多余删除）。
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

static void dumpPick(const char* tag, const RegionPickResult& p) {
    printf("  [%s] reason=%s found=%d totalLoops=%d\n",
           tag, reasonStr(p.reason), (int)p.found, p.totalLoops);
    printf("      boundaryVerts=%d area=%.4f holes=%d\n",
           (int)p.boundary.size(), p.area, (int)p.holes.size());
    printf("      boundarySrcIds=[%s] exclusive=[%s]\n",
           idsToStr(p.boundarySrcIds).toUtf8().constData(),
           idsToStr(p.exclusiveSrcIds).toUtf8().constData());
    printf("      boundary pts:");
    for (const auto& v : p.boundary) printf(" (%.2f,%.2f)", v.x, v.y);
    printf("\n");
}

static void printChanges(const SectionResult& res) {
    printf("  changes=%d:\n", (int)res.changes.size());
    for (const auto& c : res.changes) {
        const char* rtype = c.replacement.type == ElementType::Hatch ? "Hatch"
                          : c.replacement.type == ElementType::Line ? "Line"
                          : c.replacement.type == ElementType::Rectangle ? "Rectangle"
                          : c.replacement.type == ElementType::Polyline ? "Polyline"
                          : c.replacement.type == ElementType::Spline ? "Spline" : "?";
        printf("    Change id=%d remove=%d replType=%s replId=%d\n",
               c.id, (int)c.remove, rtype, c.replacement.id);
    }
}

// 模型摘要：各类型数量 + 全部 Line/Spline 端点
static void dumpModel(const SketchModel& m, const char* tag) {
    int nRect = 0, nLine = 0, nHatch = 0, nSpline = 0, nOther = 0;
    printf("  [%s] 模型:\n", tag);
    for (const auto& e : m.elements()) {
        if (e.type == ElementType::Rectangle) ++nRect;
        else if (e.type == ElementType::Line) { ++nLine;
            printf("    Line id=%d (%.2f,%.2f)->(%.2f,%.2f)\n", e.id,
                   e.pts[0].x, e.pts[0].y, e.pts[1].x, e.pts[1].y); }
        else if (e.type == ElementType::Hatch) { ++nHatch;
            double a2 = 0; const int n = (int)e.pts.size();
            for (int i = 0; i < n; ++i) { const Vec3& a = e.pts[i]; const Vec3& b = e.pts[(i+1)%n];
                a2 += a.x*b.y - b.x*a.y; }
            printf("    Hatch id=%d verts=%d area=%.2f\n", e.id, n, std::fabs(a2)*0.5); }
        else if (e.type == ElementType::Spline) ++nSpline;
        else ++nOther;
    }
    printf("    汇总 rect=%d line=%d hatch=%d spline=%d other=%d\n",
           nRect, nLine, nHatch, nSpline, nOther);
}

// ---- 场景驱动：填左 → 落盘 → 填右 → 落盘 → 断言 ---------------------------
struct SceneResult {
    RegionPickResult pickL, pickR;
    SectionResult resL, resR;
    SketchModel model;
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 三处新异常复现（悬出段分割线/曲线）===\n");

    // ================= S1：矩形 + 左凸曲线(上下端超出) =================
    {
        printf("\n========== S1: 矩形(0,0)-(100,60) + 样条(65,75)->左凸->(65,-15) ==========\n");
        SceneResult S;
        int rid = S.model.addElement(makeRect(0, 0, 100, 60, 1));
        Element sp; sp.type = ElementType::Spline;
        sp.pts = { Vec3(65, 75, 0), Vec3(45, 45, 0), Vec3(45, 20, 0), Vec3(65, -15, 0) };
        sp.id = 2;
        int sid = S.model.addElement(sp);
        printf("  rect id=%d spline id=%d\n", rid, sid);

        std::vector<Element> srcs = S.model.elements();
        S.pickL = pickSectionRegionAt(srcs, Vec3(25, 30, 0));
        dumpPick("pickL(25,30)", S.pickL);
        check(S.pickL.ok(), "S1 左侧 pick → Ok");
        check(std::fabs(S.pickL.area - 2900.0) < 350.0, "S1 左侧 area≈2900(曲线左凸)",
              QString::number(S.pickL.area));
        check(S.pickL.boundary.size() >= 8,
              "S1 左区边界顶点数≥8(必须沿曲线采样点行走，而非直线弦封口)",
              QString::number(S.pickL.boundary.size()));

        S.resL = makeSectionChangeForPick(S.pickL, srcs, SectionStyle());
        printChanges(S.resL);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resL.changes, QString());
        dumpModel(S.model, "填左后");

        std::vector<Element> srcs2 = S.model.elements();
        S.pickR = pickSectionRegionAt(srcs2, Vec3(85, 30, 0));
        dumpPick("pickR(85,30)", S.pickR);
        check(S.pickR.ok(), "S1 右侧 pick → Ok");
        check(std::fabs(S.pickR.area - 3100.0) < 350.0, "S1 右侧 area≈3100",
              QString::number(S.pickR.area));
        if (S.pickR.ok()) {
            // 关键断言1：右区边界全部在矩形内（不越界）
            QString why;
            bool within = true;
            for (const auto& v : S.pickR.boundary) {
                if (v.x < -1e-6 || v.x > 100 + 1e-6 || v.y < -1e-6 || v.y > 60 + 1e-6) {
                    within = false; why = QString("边界顶点(%1,%2)越界").arg(v.x).arg(v.y); break;
                }
            }
            check(within, "S1 右区边界全部落在矩形内(不越界)", why);
            // 关键断言2：左区探针不得落在右区多边形内（区域不被直线弦错误吞并）
            bool swallow = geo::pointInPolygon(Vec3(25, 30, 0), S.pickR.boundary);
            check(!swallow, "S1 左区探针(25,30)不在右区内(无直线弦封口吞并)");
            // 关键断言3：右区边界顶点数≥8（必须沿曲线行走）
            check(S.pickR.boundary.size() >= 8,
                  "S1 右区边界顶点数≥8(沿曲线采样点)",
                  QString::number(S.pickR.boundary.size()));
        }
        S.resR = makeSectionChangeForPick(S.pickR, srcs2, SectionStyle());
        printChanges(S.resR);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resR.changes, QString());
        dumpModel(S.model, "填右后");
        {
            // 关键断言3：分割线(样条 id)与矩形均存活，Hatch=2
            int nH = 0; bool splineAlive = false, rectAlive = false;
            for (const auto& e : S.model.elements()) {
                if (e.type == ElementType::Hatch) ++nH;
                if (e.id == 2 && e.type == ElementType::Spline) splineAlive = true;
                if (e.id == 1 && e.type == ElementType::Rectangle) rectAlive = true;
            }
            check(splineAlive, "S1 填右后样条分割线存活");
            check(rectAlive, "S1 填右后矩形存活");
            check(nH == 2, "S1 最终 Hatch 数==2", QString::number(nH));
        }
    }

    // ================= S2：矩形 + 两条超出竖直分割线（三区） =================
    {
        printf("\n========== S2: 矩形(0,0)-(150,60) + 竖线x=50/x=100(均超出上下边) ==========\n");
        SceneResult S;
        S.model.addElement(makeRect(0, 0, 150, 60, 1));
        S.model.addElement(makeLine(Vec3(50, -15, 0), Vec3(50, 75, 0), 2));
        S.model.addElement(makeLine(Vec3(100, -15, 0), Vec3(100, 75, 0), 3));

        std::vector<Element> srcs = S.model.elements();
        S.pickL = pickSectionRegionAt(srcs, Vec3(25, 30, 0));
        dumpPick("pickL(25,30)", S.pickL);
        check(S.pickL.ok(), "S2 左区 pick → Ok");
        S.resL = makeSectionChangeForPick(S.pickL, srcs, SectionStyle());
        printChanges(S.resL);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resL.changes, QString());

        std::vector<Element> srcs2 = S.model.elements();
        S.pickR = pickSectionRegionAt(srcs2, Vec3(125, 30, 0));
        dumpPick("pickR(125,30)", S.pickR);
        check(S.pickR.ok(), "S2 右区 pick → Ok");
        check(std::fabs(S.pickR.area - 3000.0) < 50.0, "S2 右区 area≈3000(仅右三分一，不跨区)",
              QString::number(S.pickR.area));
        if (S.pickR.ok()) {
            bool cross = false;
            for (const auto& v : S.pickR.boundary)
                if (v.x < 100.0 - 1e-6) { cross = true; break; }
            check(!cross, "S2 右区边界全部 x≥100(不跨过中间分割线)");
        }
        S.resR = makeSectionChangeForPick(S.pickR, srcs2, SectionStyle());
        printChanges(S.resR);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resR.changes, QString());
        dumpModel(S.model, "填右后");
        {
            bool div1Alive = false, div2Alive = false, rectAlive = false;
            int nH = 0;
            for (const auto& e : S.model.elements()) {
                if (e.type == ElementType::Hatch) ++nH;
                if (e.id == 2 && e.type == ElementType::Line) div1Alive = true;
                if (e.id == 3 && e.type == ElementType::Line) div2Alive = true;
                if (e.id == 1 && e.type == ElementType::Rectangle) rectAlive = true;
            }
            check(div1Alive, "S2 中间分割线(x=50)存活(未被切断)");
            check(div2Alive, "S2 右分割线(x=100)存活");
            check(rectAlive, "S2 矩形存活");
            check(nH == 2, "S2 最终 Hatch 数==2", QString::number(nH));
        }
    }

    // ================= S3：平行四边形 + 斜分割线(两端超出) =================
    {
        printf("\n========== S3: 平行四边形 + 斜线(75,-10)->(60,60) ==========\n");
        SceneResult S;
        S.model.addElement(makeClosedPolyline(
            { Vec3(0, 0, 0), Vec3(100, 0, 0), Vec3(120, 50, 0), Vec3(20, 50, 0) }, 1));
        S.model.addElement(makeLine(Vec3(75, -10, 0), Vec3(60, 60, 0), 2));

        std::vector<Element> srcs = S.model.elements();
        S.pickL = pickSectionRegionAt(srcs, Vec3(30, 25, 0));
        dumpPick("pickL(30,25)", S.pickL);
        check(S.pickL.ok(), "S3 左侧 pick → Ok");
        S.resL = makeSectionChangeForPick(S.pickL, srcs, SectionStyle());
        printChanges(S.resL);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resL.changes, QString());

        std::vector<Element> srcs2 = S.model.elements();
        S.pickR = pickSectionRegionAt(srcs2, Vec3(90, 25, 0));
        dumpPick("pickR(90,25)", S.pickR);
        check(S.pickR.ok(), "S3 右侧 pick → Ok");
        // 右区梯形 (72.86,0)-(100,0)-(120,50)-(62.14,50)：面积 = (27.14+57.86)/2*50 = 2125
        check(std::fabs(S.pickR.area - 2125.0) < 50.0, "S3 右区 area≈2125(仅分割线右侧)",
              QString::number(S.pickR.area));
        if (S.pickR.ok()) {
            // 分割线 x(y) = 75 - (y+10)*15/70 → 右区边界点必须在分割线右侧
            bool cross = false;
            for (const auto& v : S.pickR.boundary)
                if (v.x < 75.0 - (v.y + 10.0) * 15.0 / 70.0 - 1e-6) { cross = true; break; }
            check(!cross, "S3 右区边界全部在分割线右侧(分割线生效)");
        }
        S.resR = makeSectionChangeForPick(S.pickR, srcs2, SectionStyle());
        printChanges(S.resR);
        S.model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&S.model, S.resR.changes, QString());
        dumpModel(S.model, "填右后");
        {
            bool divAlive = false, paraAlive = false; int nH = 0;
            for (const auto& e : S.model.elements()) {
                if (e.type == ElementType::Hatch) ++nH;
                if (e.id == 2 && e.type == ElementType::Line) divAlive = true;
                if (e.id == 1 && e.type == ElementType::Polyline) paraAlive = true;
            }
            check(divAlive, "S3 分割线存活");
            check(paraAlive, "S3 平行四边形存活");
            check(nH == 2, "S3 最终 Hatch 数==2", QString::number(nH));
        }
    }

    printf("\nPASS=%d FAIL=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
