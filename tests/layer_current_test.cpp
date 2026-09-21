// 活动图层（current layer）逻辑单元测试：覆盖需求三条
//   1) 系统默认使用 0 层；
//   2) 切换图层后，新图元归属当前层并继承其属性（颜色/线型/线宽）；
//   3) 已有图元属性保持不变、正在修改中的图元不受影响、单个图元可单独编辑；
//   4) 修改图层属性(setLayerStyle)与已有图元解耦：仅更新图层定义，已有图元不变，
//      此后新建图元才快照该层新属性。
// 纯核心层，无需 GUI。复用 ElementFactory::create 作为"用户新建图元"的唯一入口。
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Layer.h"
#include <cstdio>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static Element mkLine(double x1, double y1, double x2, double y2) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}

int main() {
    SketchModel model;
    const QColor red(220, 40, 40);

    // ---- 1. 默认活动层为 "0" ----
    check(model.currentLayer() == QStringLiteral("0"), "初始活动图层为默认层 0");
    check(model.hasLayer(QStringLiteral("0")), "默认层 0 存在");

    // 默认层上新建图元：归属 0 层、保留既有 applyDefaultStyle 行为（线宽自动跟随线型）
    {
        Element e = ElementFactory::create(ElementType::Line, QStringLiteral("直线"), &model);
        check(e.layer == QStringLiteral("0"), "默认层新建图元归属层 0");
        check(e.lineWidthAuto, "默认层新建图元线宽仍自动跟随线型（未被图层覆盖）");
    }

    // ---- 2. 切换图层后，新图元归属当前层并继承其属性 ----
    model.addLayer(QStringLiteral("尺寸线"), red, 2.0, LineStyle::Dashed);
    model.setCurrentLayer(QStringLiteral("尺寸线"));
    check(model.currentLayer() == QStringLiteral("尺寸线"), "活动图层已切换到 尺寸线");

    int before = model.addElement(mkLine(0, 0, 10, 0));   // 切换前（应在层 0）
    int after  = model.addElement(ElementFactory::create(ElementType::Line,
                                QStringLiteral("直线"), &model));  // 切换后（经工厂，应继承尺寸线）

    check(model.element(after)->layer == QStringLiteral("尺寸线"),
          "切换后新建图元归属当前层 尺寸线");
    check(model.element(after)->color == red,
          "切换后新建图元继承图层颜色(红)", model.element(after)->color.name());
    check(model.element(after)->lineStyle == LineStyle::Dashed,
          "切换后新建图元继承图层线型(虚线)");
    check(model.element(after)->lineWidth == 2.0 && !model.element(after)->lineWidthAuto,
          "切换后新建图元继承图层线宽(2.0, 固定)");

    // ---- 3. 已有图元属性保持不变 ----
    check(model.element(before)->layer == QStringLiteral("0"),
          "切换图层前已存在的图元仍留在层 0（属性不变）");
    check(model.element(before)->color != red,
          "切换图层前已存在的图元未被新图层样式覆盖");

    // ---- 3(续). 同一来源两次新建：先继承旧层、后继承新层，互不串味 ----
    {
        Element a = ElementFactory::create(ElementType::Line, QStringLiteral("L"), &model); // 当前=尺寸线
        model.setCurrentLayer(QStringLiteral("0"));
        Element b = ElementFactory::create(ElementType::Line, QStringLiteral("L"), &model); // 当前=0
        check(a.layer == QStringLiteral("尺寸线") && a.lineStyle == LineStyle::Dashed,
              "切换前新建图元继承尺寸线");
        check(b.layer == QStringLiteral("0") && b.lineWidthAuto,
              "切回 0 层后新建图元恢复默认行为");
    }

    // ---- 单个图元可单独编辑，且隔离于"切换图层 / 其它图层属性变化" ----
    {
        model.setCurrentLayer(QStringLiteral("尺寸线"));
        Element e = ElementFactory::create(ElementType::Line, QStringLiteral("直线"), &model);
        const int id = model.addElement(e);
        const QColor green(0, 200, 0);
        model.setSelected(id);
        model.styleSelectedEx(&green, nullptr, nullptr);   // 单独编辑颜色
        check(model.element(id)->color == green, "单个图元可单独编辑（颜色=绿）");

        // 改图层线宽（只改线宽）：【解耦】已有图元(已单独编辑)属性不变，不被图层线宽覆盖
        const double w3 = 3.0;
        model.setLayerStyle(QStringLiteral("尺寸线"), nullptr, &w3, nullptr);
        check(model.element(id)->color == green, "单独编辑后不受图层属性变化影响：颜色仍=绿");
        check(model.element(id)->lineWidth == 2.0 && !model.element(id)->lineWidthAuto,
              "【解耦】图层改线宽不波及已有图元：id 线宽仍为创建时快照的 2.0(固定)");

        // 改层属性后新建图元：继承图层"当前"新属性（线宽=3.0）
        Element e2 = ElementFactory::create(ElementType::Line, QStringLiteral("直线"), &model);
        const int id2 = model.addElement(e2);
        check(model.element(id2)->lineWidth == w3 && !model.element(id2)->lineWidthAuto,
              "【解耦】改层线宽后新建图元继承新线宽 3.0(快照)");
        check(model.element(id2)->color == red && model.element(id2)->lineStyle == LineStyle::Dashed,
              "改层线宽后新建图元仍继承该层颜色/线型");

        // 切换活动图层不影响已单独编辑的图元
        model.setCurrentLayer(QStringLiteral("0"));
        check(model.element(id)->color == green && model.element(id)->lineWidth == 2.0,
              "切换活动图层后已编辑图元属性不变");
    }

    // ---- 删除当前活动层 → 回落默认层 ----
    model.setCurrentLayer(QStringLiteral("尺寸线"));
    model.removeLayer(QStringLiteral("尺寸线"));
    check(model.currentLayer() == QStringLiteral("0"), "删除活动图层后活动层回落 0");
    check(!model.hasLayer(QStringLiteral("尺寸线")), "尺寸线 已被删除");

    // ---- 重命名活动层 → 活动层名同步更新 ----
    model.addLayer(QStringLiteral("中心线"), QColor(0, 120, 200), 3.0, LineStyle::DashDotDot);
    model.setCurrentLayer(QStringLiteral("中心线"));
    model.renameLayer(QStringLiteral("中心线"), QStringLiteral("基准线"));
    check(model.currentLayer() == QStringLiteral("基准线"), "重命名活动层后活动层名同步更新");
    check(model.hasLayer(QStringLiteral("基准线")), "基准线 已存在");

    // ---- 切到不存在的层 → 不生效 ----
    const QString prev = model.currentLayer();
    model.setCurrentLayer(QStringLiteral("不存在的层"));
    check(model.currentLayer() == prev, "切换到不存在的图层被拒绝（保持原活动层）");

    printf("\n活动图层逻辑测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
