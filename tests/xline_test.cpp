// 构造线（Xline）功能单元测试：纯核心层验证「工厂 / 枚举序号 / 序列化往返 /
// 命令行六种模式 / 点到无限直线距离 / 交点（线×线、线×圆、构造线×构造线）/
// 捕捉手柄 / flatten 排除（剪切与面积长度统计不参与）/ ToolRegistry 注册 /
// line::modeFromString 映射 / DXF·DWG 导出降级 / 撤销重做」全链路。
//
// 可验证推论（对齐 AutoCAD XLINE 语义）：
//   1) 无限直线 = 通过点 + 方向角，无端点；捕捉只有通过点是真实几何点；
//   2) 交点捕捉是构造线的核心用途 —— 与任意图元（含另一条构造线）都能求交；
//   3) R12 DXF/DWG 无 XLINE 原生实体，导出降级为长线段（±10 m）。
//
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
#include <QCoreApplication>
#include <QByteArray>
#include <QRectF>
#include <cstdio>
#include <cmath>

#include "core/SketchModel.h"
#include "core/CommandInterpreter.h"
#include "core/Element.h"
#include "core/ElementFactory.h"
#include "core/Geometry.h"
#include "core/Snap.h"
#include "core/ToolRegistry.h"
#include "core/LineModes.h"
#include "core/CadFormats.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    SketchModel m;
    CommandInterpreter ci(&m);

    printf("=== 1. 工厂与枚举 ===\n");
    {
        Element x = ElementFactory::createXline(Vec3(10, 20, 0), 45.0, &m);
        check(x.type == ElementType::Xline, "工厂：类型 == Xline");
        check((int)ElementType::Xline == 13, "枚举序号 Xline == 13（只增不改）");
        check(x.pts.size() == 1 && near(x.pts[0].x, 10) && near(x.pts[0].y, 20),
              "工厂：pts[0] == 通过点");
        check(near(x.rotation, 45.0), "工厂：rotation == 方向角 45°");
        check(x.name == QStringLiteral("构造线"), "工厂：默认名「构造线」");
        check(QString::fromUtf8(elementTypeName(x.type)) == QStringLiteral("构造线"),
              "elementTypeName：构造线");
        check(x.sizeDesc().contains(QStringLiteral("45")), "sizeDesc：含角度文案");

        // 序列化往返（泛型字段：pts + rotation）
        const Element y = elementFromJson(elementToJson(x));
        check(y.type == ElementType::Xline, "序列化：类型往返");
        check(y.pts.size() == 1 && near(y.pts[0].x, 10) && near(y.pts[0].y, 20),
              "序列化：通过点往返");
        check(near(y.rotation, 45.0), "序列化：方向角往返");
    }

    printf("=== 2. ToolRegistry 注册 ===\n");
    {
        const ToolRegistry& reg = ToolRegistry::instance();
        check(reg.find("xline") != nullptr, "xline 已注册");
        check(reg.isDrawTool("xline"), "xline 属绘制类");
        check(reg.xlineSubToolIds().size() == 6, "6 个构造线子模式",
              QString("实际=%1").arg(reg.xlineSubToolIds().size()));
        const QStringList expect = {
            "xline_h", "xline_v", "xline_angle",
            "xline_2pt", "xline_bisector", "xline_parallel"};
        for (const QString& id : expect)
            check(reg.isXlineSubTool(id), QString("%1 属 XlineSub").arg(id).toUtf8().constData());
        check(line::modeFromString("xline") == line::Mode::TwoPoint, "modeFromString xline→TwoPoint");
        check(line::modeFromString("xline_h") == line::Mode::Horizontal, "modeFromString xline_h→Horizontal");
        check(line::modeFromString("xline_v") == line::Mode::Vertical, "modeFromString xline_v→Vertical");
        check(line::modeFromString("xline_angle") == line::Mode::Angle, "modeFromString xline_angle→Angle");
        check(line::modeFromString("xline_2pt") == line::Mode::TwoPoint, "modeFromString xline_2pt→TwoPoint");
        check(line::modeFromString("xline_bisector") == line::Mode::Bisector, "modeFromString xline_bisector→Bisector");
        check(line::modeFromString("xline_parallel") == line::Mode::Parallel, "modeFromString xline_parallel→Parallel");
    }

    printf("=== 3. 命令行六种模式 ===\n");
    {
        int n = m.elementCount();
        check(ci.execute("xline 10,20 45"), "xline 单点+角度");
        const Element* last = m.element(n + 1);
        check(last && last->type == ElementType::Xline, "xline 创建构造线");
        check(last && near(last->pts[0].x, 10) && near(last->pts[0].y, 20), "xline 通过点 (10,20)");
        check(last && near(last->rotation, 45.0), "xline 角度 45°");

        check(ci.execute("xline 0,0 100,50"), "xline 两点方式");
        last = m.element(n + 2);
        check(last && near(last->pts[0].x, 0) && near(last->pts[0].y, 0), "两点：通过点 = 第一拾取点");
        check(last && near(last->rotation, std::atan2(50.0, 100.0) * 180.0 / M_PI, 1e-3),
              "两点：方向角 = atan2(50,100)");

        check(ci.execute("xline_h 5,7"), "xline_h 水平过点");
        last = m.element(n + 3);
        check(last && near(last->pts[0].x, 5) && near(last->pts[0].y, 7) && near(last->rotation, 0.0),
              "水平：通过 (5,7) 角度 0°");

        check(ci.execute("xline_v 5,7"), "xline_v 竖直过点");
        last = m.element(n + 4);
        check(last && near(last->rotation, 90.0), "竖直：角度 90°");

        check(ci.execute("xline_angle 3,4 60"), "xline_angle 指定角度");
        last = m.element(n + 5);
        check(last && near(last->rotation, 60.0), "角度构造线：60°");

        check(ci.execute("xline_angle 3,4"), "xline_angle 缺省角度");
        last = m.element(n + 6);
        check(last && near(last->rotation, 30.0), "角度构造线：缺省 30°");

        // 角平分线：两条参考直线（0° 与 90°）→ 平分方向 45°
        check(ci.execute("line 0,0 100,0"), "参考直线 1（水平）");
        check(ci.execute("line 0,0 0,100"), "参考直线 2（竖直）");
        const int l1 = n + 7, l2 = n + 8;
        check(ci.execute(QString("xline_bisector %1 %2 10,10").arg(l1).arg(l2)), "xline_bisector");
        last = m.element(n + 9);
        check(last && last->type == ElementType::Xline, "角平分：生成构造线");
        if (last && last->type == ElementType::Xline) {
            const double r = std::fmod(std::fabs(last->rotation), 180.0);
            check(near(r, 45.0, 1e-3) || near(r, 135.0, 1e-3), "角平分：方向为 45°/135° 族",
                  QString("实际=%1").arg(last->rotation));
        }

        // 平行构造线：参考水平直线 → 方向 0°（模 180）
        check(ci.execute(QString("xline_parallel %1 6,6").arg(l1)), "xline_parallel");
        last = m.element(n + 10);
        check(last && last->type == ElementType::Xline, "平行：生成构造线");
        if (last && last->type == ElementType::Xline) {
            const double r = std::fmod(std::fabs(last->rotation), 180.0);
            check(near(r, 0.0, 1e-3), "平行：方向与参考线一致（模 180）",
                  QString("实际=%1").arg(last->rotation));
        }

        check(!ci.execute("xline"), "缺参数的 xline 被拒绝");
        check(!ci.execute("xline_parallel 9999 6,6"), "参考图元不存在被拒绝");
    }

    printf("=== 4. 几何：点线距离 / 交点 / 捕捉 / flatten 排除 ===\n");
    {
        Element xl = ElementFactory::createXline(Vec3(0, 0, 0), 0.0, &m);   // 水平过原点
        check(near(geo::distanceToElement(xl, Vec3(50, 7, 0)), 7.0), "点线距离（正侧）");
        check(near(geo::distanceToElement(xl, Vec3(-30, -2, 0)), 2.0), "点线距离（负侧，无限延伸）");

        // 线×线
        Element ln; ln.type = ElementType::Line; ln.pts = {Vec3(5, -5, 0), Vec3(5, 5, 0)};
        auto ips = geo::intersectElements(xl, ln);
        check(ips.size() == 1 && near(ips[0].x, 5) && near(ips[0].y, 0), "交点：构造线×直线");

        // 线×圆：圆心 (0,3) r=5 → (±4, 0)
        Element cir = ElementFactory::createCircle(Vec3(0, 3, 0), 5.0, &m);
        ips = geo::intersectElements(xl, cir);
        check(ips.size() == 2, "交点：构造线×圆得 2 解", QString("实际=%1").arg((int)ips.size()));
        bool got1 = false, got2 = false;
        for (const auto& p : ips) {
            if (near(p.x, 4) && near(p.y, 0)) got1 = true;
            if (near(p.x, -4) && near(p.y, 0)) got2 = true;
        }
        check(got1 && got2, "交点：构造线×圆 = (±4, 0)");

        // 构造线×构造线：水平 y=0 × 竖直过 (7,7) → (7, 0)
        Element xv = ElementFactory::createXline(Vec3(7, 7, 0), 90.0, &m);
        ips = geo::intersectElements(xl, xv);
        check(ips.size() == 1 && near(ips[0].x, 7) && near(ips[0].y, 0), "交点：构造线×构造线");

        // 捕捉：仅通过点是真实几何点
        const auto hs = snap::snapHandles(xl);
        check(hs.size() == 1 && near(hs[0].x, 0) && near(hs[0].y, 0), "snapHandles：仅通过点");

        // flatten 排除：剪切 / 面积长度统计不消费构造线
        check(geo::flatten(xl).empty(), "flatten：无离散段（不参与统计/剪切）");
        const QRectF b = geo::elementBounds(xl);
        check(b.width() < 1e3 && b.height() < 1e3, "elementBounds：不含无限延伸");
    }

    printf("=== 5. 导出降级（DXF / DWG）===\n");
    {
        Element xl = ElementFactory::createXline(Vec3(0, 0, 0), 45.0, &m);
        const std::vector<Element> els{xl};
        QString err;
        const QByteArray dxf = cadfmt::toDxf(els, &err);
        check(err.isEmpty(), "DXF 导出无错误", err);
        check(dxf.contains("LINE"), "DXF：构造线降级为 LINE 实体");
        check(dxf.contains("AC1009"), "DXF：R12 版本头");

        const QByteArray dwg = cadfmt::toDwg(els, &err);
        check(err.isEmpty(), "DWG 导出无错误", err);
        std::vector<Element> back;
        check(cadfmt::fromDwg(dwg, back, &err), "DWG 读回成功", err);
        check(back.size() == 1, "DWG 读回 1 个图元");
        check(!back.empty() && back[0].type == ElementType::Line,
              "DWG：构造线读回为降级 LINE（预期行为）");
        if (!back.empty() && back[0].type == ElementType::Line) {
            const double len = back[0].pts[0].distTo(back[0].pts[1]);
            check(len > 1.5e4, "DWG：降级线段足够长（>±10m 折半）",
                  QString("长度=%1").arg(len, 0, 'f', 0));
        }
    }

    printf("=== 6. 撤销 / 重做 ===\n");
    {
        const int n = m.elementCount();
        check(ci.execute("xline 1,1 30"), "xline 入模型");
        check(m.elementCount() == n + 1, "图元数 +1");
        m.undo();
        check(m.elementCount() == n, "undo 移除构造线");
        m.redo();
        check(m.elementCount() == n + 1, "redo 恢复构造线");
        const Element* e = m.element(n + 1);
        check(e && e->type == ElementType::Xline && near(e->rotation, 30.0),
              "redo 后字段完整");
    }

    printf("\n%s\n", g_fail == 0 ? "XlineTest: ALL PASS" : "XlineTest: HAS FAILURES");
    return g_fail == 0 ? 0 : 1;
}
