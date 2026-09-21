// 图层（Layer）管理功能单元测试：在核心层 SketchModel 上验证图元与图层的交互规则
//   * 默认层 "0" 始终存在；新建/重名/删除图层；
//   * 【要求1】把选中图元分配（assign）到指定图层时"保留图元原有属性"，不套用图层样式；
//   * 【要求2·解耦】修改图层属性（setLayerStyle）时，仅更新图层定义；本层已有图元属性保持不变，
//            不反向推送；只有此后再新建的图元才快照该层新属性；其他图层图元不受影响；
//   * 【要求3】单个图元可单独编辑（styleSelectedEx），只作用于该图元本身，
//            不反向改变图层定义，也不影响其他图元；
//   * 对某一图层执行显示控制（setLayerVisible）只翻转本层图元的 visible；
//   * applyLayerStyleToElements 是"强制重新同步"：把图层当前全部三项样式写回本层图元；
//   * 删除/重命名图层时其图元归属正确回落/迁移；
//   * 图层操作可一次 Ctrl+Z 撤回（图元与图层定义一起回退）；
//   * 图层表可随图存盘/读回（layersToJson / setLayersFromJson），旧文件无该字段时回退单层"0"。
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
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
    const QColor def(26, 29, 40);     // 图元默认色（Element 默认值）

    // ---- 1. 默认层 "0" 始终存在 ----
    check(model.layerCount() == 1, "初始只有默认层 0");
    check(model.hasLayer(QStringLiteral("0")), "默认层 0 存在");
    check(!model.hasLayer(QStringLiteral("尺寸线")), "新建前不存在尺寸线层");

    // ---- 2. 新建图层（带颜色/线宽/线型）----
    const QColor red(220, 40, 40);
    check(model.addLayer(QStringLiteral("尺寸线"), red, 2.0, LineStyle::Dashed),
          "新建图层 尺寸线(红/2px/虚线)");
    check(model.layerCount() == 2, "图层数变为 2");
    const Layer* L1 = model.findLayer(QStringLiteral("尺寸线"));
    check(L1 && L1->color == red && L1->lineWidth == 2.0 && L1->lineStyle == LineStyle::Dashed,
          "图层 尺寸线 的属性正确写入", L1 ? L1->color.name() : QString());
    check(!model.addLayer(QStringLiteral("尺寸线")), "重名图层拒绝创建");

    check(model.addLayer(QStringLiteral("中心线"), QColor(0, 120, 200), 3.0, LineStyle::DashDotDot),
          "新建图层 中心线(蓝/3px/双点划线)");
    const Layer* L2 = model.findLayer(QStringLiteral("中心线"));

    // ---- 3. 【要求1】选中图元分配（assign）到图层，保留图元原有属性（不套用图层样式）----
    const int a = model.addElement(mkLine(0, 0, 10, 0));     // 留在层 0
    const int b = model.addElement(mkLine(0, 5, 10, 5));     // 移入 尺寸线
    const int c = model.addElement(mkLine(0, 10, 10, 10));   // 移入 中心线
    check(model.element(a)->layer == QStringLiteral("0"), "新图元默认归属层 0");

    model.setSelection({b});
    int n = model.assignSelectionToLayer(QStringLiteral("尺寸线"));   // adoptStyle 默认 false
    check(n == 1 && model.element(b)->layer == QStringLiteral("尺寸线"),
          "assign：单选中图元 b 移入 尺寸线");
    check(model.element(b)->color == def && model.element(b)->lineWidth == 1.0 &&
          model.element(b)->lineStyle == LineStyle::Solid && model.element(b)->lineWidthAuto,
          "【要求1】assign：b 保留原有属性(默认色/1px/实线/自动)，未套用图层样式");

    model.setSelection({c});
    model.assignSelectionToLayer(QStringLiteral("中心线"));
    check(model.element(c)->layer == QStringLiteral("中心线"), "assign：c 移入 中心线");
    check(model.element(c)->color == def && model.element(c)->lineWidth == 1.0,
          "【要求1】assign：c 保留原有属性");
    check(model.element(a)->layer == QStringLiteral("0"), "assign：未选中的 a 仍在层 0（互不干扰）");

    // ---- 4. 【要求2·解耦】修改图层属性 -> 仅更新图层定义，本层已有图元属性保持不变 ----
    //   （不再反向推送；只有此后再新建的图元才快照该层新属性）
    // 4a. 只改颜色：图层定义颜色=蓝；本层已有图元 b 颜色保持原样，其他层不受影响
    const QColor blue(0, 0, 255);
    check(model.setLayerStyle(QStringLiteral("尺寸线"), &blue, nullptr, nullptr),
          "setLayerStyle 尺寸线：仅改颜色为蓝");
    check(model.findLayer(QStringLiteral("尺寸线"))->color == blue, "图层定义 尺寸线 颜色=蓝");
    check(model.element(b)->color == def, "【解耦】本层已有图元 b 颜色保持不变(仍为默认色)");
    check(model.element(b)->lineWidth == 1.0 && model.element(b)->lineStyle == LineStyle::Solid
          && model.element(b)->lineWidthAuto,
          "【解耦】b 的线宽/线型/自动派生均未被图层属性覆盖");
    check(model.element(a)->color == def && model.element(c)->color == def,
          "【解耦】其他层(a/c)颜色不变（互不干扰）");

    // 4b. 只改线宽：图层定义线宽=2.5；已有图元 b 线宽保持 1.0（lineWidthAuto 不变）
    const double w25 = 2.5;
    check(model.setLayerStyle(QStringLiteral("尺寸线"), nullptr, &w25, nullptr),
          "setLayerStyle 尺寸线：仅改线宽为 2.5px");
    check(model.findLayer(QStringLiteral("尺寸线"))->lineWidth == 2.5, "图层定义 尺寸线 线宽=2.5");
    check(model.element(b)->lineWidth == 1.0 && model.element(b)->lineWidthAuto,
          "【解耦】本层已有图元 b 线宽保持 1.0/自动派生，未被层线宽覆盖");
    check(model.element(a)->lineWidth == 1.0 && model.element(c)->lineWidth == 1.0,
          "【解耦】其他层(a/c)线宽不变");

    // 4c. 只改线型：图层定义线型=点划线；已有图元 b 线型保持实线
    const LineStyle dashDot = LineStyle::DashDot;
    check(model.setLayerStyle(QStringLiteral("尺寸线"), nullptr, nullptr, &dashDot),
          "setLayerStyle 尺寸线：仅改线型为点划线");
    check(model.findLayer(QStringLiteral("尺寸线"))->lineStyle == LineStyle::DashDot,
          "图层定义 尺寸线 线型=点划线");
    check(model.element(b)->lineStyle == LineStyle::Solid, "【解耦】本层已有图元 b 线型保持实线");
    check(model.element(a)->lineStyle == LineStyle::Solid && model.element(c)->lineStyle == LineStyle::Solid,
          "【解耦】其他层(a/c)线型不变");

    // 4d. 改层属性后新建图元：经 ElementFactory::create → applyActiveLayer 快照图层"当前"属性
    model.setCurrentLayer(QStringLiteral("尺寸线"));
    const int nb = model.addElement(ElementFactory::create(ElementType::Line,
                                QStringLiteral("直线"), &model));
    check(model.element(nb)->layer == QStringLiteral("尺寸线"), "改层属性后新建图元 nb 归属当前活动层 尺寸线");
    check(model.element(nb)->color == blue && model.element(nb)->lineWidth == 2.5 &&
          model.element(nb)->lineStyle == LineStyle::DashDot && !model.element(nb)->lineWidthAuto,
          "【解耦】新建 nb 应用了图层新属性(蓝/2.5/点划线)，为固定快照副本");
    check(model.element(b)->color == def && model.element(b)->lineStyle == LineStyle::Solid,
          "【解耦】新建 nb 不影响已有图元 b（仍为旧属性）");

    // ---- 5. 【要求3】单个图元单独编辑：只影响该图元，不反向改变图层，不影响其他图元 ----
    model.setSelection({b});
    const QColor green(0, 200, 0);
    check(model.styleSelectedEx(&green, nullptr, nullptr), "单独编辑 b 颜色=绿");
    check(model.element(b)->color == green, "【要求3】b 颜色=绿");
    check(model.findLayer(QStringLiteral("尺寸线"))->color == blue,
          "【要求3】单独编辑 b 不反向改变图层 尺寸线 的颜色(仍为蓝)");
    check(model.element(a)->color == def && model.element(c)->color == def,
          "【要求3】单独编辑 b 不影响其他图元(a/c)");

    // 给 b 一个单独线型覆盖(点线)，再改图层颜色（只改颜色），验证"对应属性"不覆盖单独覆盖
    const LineStyle dotted = LineStyle::Dotted;
    model.setSelection({b});
    check(model.styleSelectedEx(nullptr, nullptr, &dotted), "单独编辑 b 线型=点线");
    check(model.element(b)->lineStyle == LineStyle::Dotted, "b 线型覆盖为点线");
    const QColor purple(150, 0, 200);
    check(model.setLayerStyle(QStringLiteral("尺寸线"), &purple, nullptr, nullptr),
          "改图层 尺寸线 颜色=紫(只改颜色)");
    check(model.element(b)->color == green, "【解耦】图层改色后已有图元 b 颜色保持绿(不被层色覆盖)");
    check(model.element(b)->lineStyle == LineStyle::Dotted,
          "【要求2】只改颜色未覆盖 b 的单独线型覆盖(仍为点线)");
    check(model.findLayer(QStringLiteral("尺寸线"))->color == purple &&
          model.findLayer(QStringLiteral("尺寸线"))->lineStyle == LineStyle::DashDot,
          "图层定义：颜色=紫、线型仍为点划线(未因只改颜色而变)");

    // ---- 6. applyLayerStyleToElements：强制把图层当前全部三项样式写回本层图元（重新同步）----
    int applied = model.applyLayerStyleToElements(QStringLiteral("尺寸线"));
    check(applied == 2, "apply：影响 2 个本层图元(b 与改层后新建的 nb)");
    check(model.element(b)->color == purple && model.element(b)->lineWidth == 2.5 &&
          model.element(b)->lineStyle == LineStyle::DashDot && !model.element(b)->lineWidthAuto,
          "apply：本层图元 b 被强制同步为图层当前全部样式(覆盖其单独线型覆盖)");
    check(model.element(a)->color == def && model.element(c)->color == def,
          "apply：层 0 的 a 与层 中心线 的 c 不变（互不干扰）");

    // ---- 7. 显示控制仅翻转本层（setLayerVisible）----
    check(model.element(a)->visible && model.element(b)->visible && model.element(c)->visible,
          "初始三层图元均可见");
    model.setLayerVisible(QStringLiteral("尺寸线"), false);
    check(!model.element(b)->visible, "隐藏 尺寸线：本层图元 b 不可见");
    check(model.findLayer(QStringLiteral("尺寸线"))->visible == false, "隐藏 尺寸线：图层定义 visible=false");
    check(model.element(a)->visible, "隐藏 尺寸线：层 0 的 a 仍可见（互不干扰）");
    check(model.element(c)->visible, "隐藏 尺寸线：层 中心线 的 c 仍可见（互不干扰）");
    model.setLayerVisible(QStringLiteral("尺寸线"), true);
    check(model.element(b)->visible, "重新显示 尺寸线：b 恢复可见");

    // ---- 8. 删除图层：其图元回落默认层，默认层不可删 ----
    const int d = model.addElement(mkLine(50, 50, 60, 60));    // 临时移入 中心线 再删层
    model.setSelection({d});
    model.assignSelectionToLayer(QStringLiteral("中心线"));
    check(model.element(d)->layer == QStringLiteral("中心线"), "准备：d 在 中心线");
    check(!model.removeLayer(QStringLiteral("0")), "默认层 0 不可删除");
    check(model.removeLayer(QStringLiteral("中心线")), "删除图层 中心线");
    check(model.element(c)->layer == QStringLiteral("0") && model.element(d)->layer == QStringLiteral("0"),
          "删除 中心线：其图元 c/d 回落到层 0（保留各自属性）");
    check(!model.hasLayer(QStringLiteral("中心线")), "删除后 中心线 不再存在");

    // ---- 9. 重命名图层：图元归属同步迁移 ----
    check(model.renameLayer(QStringLiteral("尺寸线"), QStringLiteral("标注")), "重命名 尺寸线 -> 标注");
    check(model.element(b)->layer == QStringLiteral("标注"), "重命名：图元 b 归属同步改为 标注");
    check(!model.hasLayer(QStringLiteral("尺寸线")) && model.hasLayer(QStringLiteral("标注")),
          "重命名：旧名消失、新名出现");
    check(!model.renameLayer(QStringLiteral("0"), QStringLiteral("x")), "默认层 0 不可改名");

    // ---- 10. 序列化往返（layersToJson / setLayersFromJson）----
    QJsonArray arr = model.layersToJson();
    SketchModel m2;
    m2.setLayersFromJson(arr);
    check(m2.layerCount() == model.layerCount(), "读回：图层数量一致",
          QString("%1 vs %2").arg(m2.layerCount()).arg(model.layerCount()));
    const Layer* src = model.findLayer(QStringLiteral("标注"));
    const Layer* got = m2.findLayer(QStringLiteral("标注"));
    check(got && got->color == src->color && got->lineWidth == src->lineWidth &&
          got->lineStyle == src->lineStyle,
          "读回：标注层 属性完好");

    // 旧文件兼容：无 layers 字段（空数组）应回退为单层 "0"
    SketchModel m3;
    m3.setLayersFromJson(QJsonArray());
    check(m3.layerCount() == 1 && m3.hasLayer(QStringLiteral("0")), "空图层表读回 -> 仅默认层 0");

    // ---- 11. 撤销：图层显示控制可一次 Ctrl+Z 回退（图元与定义一起）----
    SketchModel model4;
    const int e0 = model4.addElement(mkLine(0, 0, 10, 0));
    model4.addLayer(QStringLiteral("隐藏层"), QColor(255, 0, 0), 1.0, LineStyle::Solid);
    model4.setSelection({e0});
    model4.assignSelectionToLayer(QStringLiteral("隐藏层"));   // 保留属性
    model4.setLayerVisible(QStringLiteral("隐藏层"), false);
    check(!model4.element(e0)->visible, "撤销前：e0 因图层隐藏而不可见");
    model4.undo();   // 撤销“隐藏图层”
    check(model4.element(e0)->visible, "撤销隐藏图层：e0 恢复可见");
    check(model4.findLayer(QStringLiteral("隐藏层"))->visible == true, "撤销隐藏图层：图层定义也恢复");
    model4.undo();   // 撤销“移入图层”
    check(model4.element(e0)->layer == QStringLiteral("0"), "撤销移入图层：e0 回落层 0");

    printf("\n图层测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
