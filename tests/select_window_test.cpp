// 画布框选（窗口选择 / 窗口取消）端到端测试：用合成鼠标事件驱动完整链路
//   viewport -> viewportEvent -> mousePress/Move/Release -> ToolManager -> SketchModel
// 覆盖选择交互的四个结算语义：框选入选 / Ctrl 加选 / 窗口取消 / 空白点击清空。
//
// 为什么要在 UI 层单独立一个测试：
//   核心层 SketchModel::selectWindow 早有单测（§27），但「框选」这条路径一半在
//   GraphicsView 的鼠标事件里。此前交互层自己维护了一份 intersects 判定，与核心层
//   是两份逻辑——菜单/命令行走核心层、画布拖拽走交互层，长期必然分叉。
//   现在交互层统一调用 selectWindow，本测试即从 UI 层锚定这条契约。
//
// 用例刻意选用「水平线（包围盒高=0）+ 垂直线（包围盒宽=0）」：
//   QRectF::intersects 要求非零公共面积，零面积包围盒是框选判定的经典边界；
//   核心层靠 1e-6 双侧膨胀覆盖它，若哪天判定改回裸 intersects，这里会立刻红。
// 运行平台：QT_QPA_PLATFORM=offscreen（无真实显示亦可）。
#include <QApplication>
#include <QMouseEvent>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include <cstdio>
#include <cmath>

namespace SketchCAD {

// 借派生类把合成的鼠标事件送进视口（与真机一致：事件先到 viewport）
class TestView : public GraphicsView {
public:
    explicit TestView(SketchModel* m, QWidget* p = nullptr) : GraphicsView(m, p) {}

    // 在世界坐标 (x1,y1) → (x2,y2) 之间拖一个框；mod 用于传 Ctrl（加选）
    void dragBox(QPoint a, QPoint b, Qt::KeyboardModifiers mod = Qt::NoModifier) {
        QWidget* vp = viewport();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(a),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(b),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(b),
                            Qt::LeftButton, Qt::NoButton, mod);
        QApplication::sendEvent(vp, &release);
#pragma GCC diagnostic pop
    }
};

} // namespace SketchCAD

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what, detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchCAD::SketchModel model;
    SketchCAD::TestView view(&model);
    view.resize(800, 600);
    view.show();

    auto mkLine = [](double x1, double y1, double x2, double y2) {
        SketchCAD::Element e;
        e.type = SketchCAD::ElementType::Line;
        e.pts = {SketchCAD::Vec3(x1, y1, 0), SketchCAD::Vec3(x2, y2, 0)};
        return e;
    };
    const int H = model.addElement(mkLine(0, 0, 10, 0));        // 水平线：包围盒高 = 0
    const int V = model.addElement(mkLine(20, 0, 20, 10));      // 垂直线：包围盒宽 = 0
    const int F = model.addElement(mkLine(100, 100, 110, 100)); // 远处水平线（不该被框到）

    view.fitView();   // 建立世界→视口的映射

    // 世界坐标 → 视口像素（后续合成事件都用像素坐标）
    auto vp = [&](double x, double y) { return view.mapFromScene(QPointF(x, y)); };

    // ---- 1. 窗口选择：框住水平线 H（零高度包围盒）----
    model.clearSelection();
    model.setActiveTool(QStringLiteral("select_window"));
    view.dragBox(vp(-2, -2), vp(12, 3));
    check(model.isSelected(H), "窗口选择：框住水平线 -> 水平线被选中（零高包围盒可命中）");
    check(!model.isSelected(V) && !model.isSelected(F),
          "窗口选择：窗口外的垂直线/远处图元不被选中");

    // ---- 2. Ctrl 加选：再框住垂直线 V（零宽度包围盒）----
    view.dragBox(vp(15, -2), vp(25, 12), Qt::ControlModifier);
    check(model.isSelected(H) && model.isSelected(V),
          "Ctrl + 窗口选择：垂直线并入选择集（零宽包围盒可命中）",
          QString::number(model.selection().size()));

    // ---- 3. 窗口取消：框住 V，仅移除 V、保留 H ----
    model.setActiveTool(QStringLiteral("deselect_window"));
    view.dragBox(vp(15, -2), vp(25, 12));
    check(!model.isSelected(V) && model.isSelected(H),
          "窗口取消：移除框内垂直线，保留框外的水平线",
          QString::number(model.selection().size()));

    // ---- 4. 空白处小拖动（未拖出有效矩形）视为点击 -> 清空选择 ----
    model.setActiveTool(QStringLiteral("select"));
    view.dragBox(vp(300, 300), vp(300, 300));
    check(!model.hasSelection(), "空白处点击（未拖出矩形）-> 清空选择");

    // ============================================================
    // 5. 多选整体平移（输入偏移坐标路径）：translateSelection 把选区当刚体平移
    // ============================================================
    // 复选 H/V/F 三个图元，一次性平移 (15,7)；验证三件事：
    //   a) 每个选中图元都精确平移相同 delta；
    //   b) 图元之间的相对向量不变（整体刚性移动）；
    //   c) 未选中图元不动（此处全部选中，故这条由 d 间接保证）。
    model.clearSelection();
    model.setSelection({H, V, F});
    auto startOf = [&](int id) -> Vec3 { return model.element(id)->pts.front(); };
    Vec3 bH = startOf(H), bV = startOf(V), bF = startOf(F);
    model.translateSelection(Vec3(15.0, 7.0), QStringLiteral("测试-平移"));
    auto movedBy = [&](const Vec3& before, int id) -> Vec3 {
        return model.element(id)->pts.front() - before;
    };
    Vec3 dH = movedBy(bH, H), dV = movedBy(bV, V), dF = movedBy(bF, F);
    check(std::fabs(dH.x - 15.0) < 1e-9 && std::fabs(dH.y - 7.0) < 1e-9,
          "偏移平移：水平线精确平移 (15,7)");
    check(std::fabs(dV.x - 15.0) < 1e-9 && std::fabs(dV.y - 7.0) < 1e-9,
          "偏移平移：垂直线精确平移 (15,7)");
    check(std::fabs(dF.x - 15.0) < 1e-9 && std::fabs(dF.y - 7.0) < 1e-9,
          "偏移平移：远处水平线也平移 (15,7)");
    // 相对位置不变：H↔V 原向量与现向量相等
    Vec3 relHV_before = bV - bH, relHV_after = model.element(V)->pts.front() - model.element(H)->pts.front();
    check(std::fabs(relHV_after.x - relHV_before.x) < 1e-9 &&
          std::fabs(relHV_after.y - relHV_before.y) < 1e-9,
          "偏移平移：图元间相对位置保持不变（H↔V 向量不变）");

    // ---- 6. 多选整体平移（鼠标拖拽路径）：拖一个选中图元，整组跟移 ----
    model.setActiveTool(QStringLiteral("select"));
    model.setSelection({H, V});
    const Vec3 preH = model.element(H)->pts.front();
    const Vec3 preV = model.element(V)->pts.front();
    const double lenH_before = (model.element(H)->pts[1] - model.element(H)->pts[0]).len();
    // 在水平线当前中点按下并拖到 (mid+20, mid+10) —— 触发整组平移，非单图元
    const Vec3 hmid = (model.element(H)->pts[0] + model.element(H)->pts[1]) * 0.5;
    view.dragBox(vp(hmid.x, hmid.y), vp(hmid.x + 20, hmid.y + 10));
    const Vec3 postH = model.element(H)->pts.front();
    const Vec3 postV = model.element(V)->pts.front();
    const Vec3 dHg = postH - preH, dVg = postV - preV;
    check(std::fabs(dHg.x - dVg.x) < 1e-6 && std::fabs(dHg.y - dVg.y) < 1e-6,
          "拖拽平移：整组图元移动量一致（相对位置不变）",
          QString("dH=(%1,%2) dV=(%3,%4)")
              .arg(dHg.x, 0, 'f', 2).arg(dHg.y, 0, 'f', 2)
              .arg(dVg.x, 0, 'f', 2).arg(dVg.y, 0, 'f', 2));
    const double lenH_after = (model.element(H)->pts[1] - model.element(H)->pts[0]).len();
    check(std::fabs(lenH_after - lenH_before) < 1e-6,
          "拖拽平移：单个图元几何未变形（长度不变）");

    printf("================ 结果: %s ================\n",
           g_fail == 0 ? "全部通过" : QString("%1 项失败").arg(g_fail).toUtf8().constData());
    return g_fail == 0 ? 0 : 1;
}
