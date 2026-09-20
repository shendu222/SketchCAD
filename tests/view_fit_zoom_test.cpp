// 「适配窗口」与「窗口缩放」端到端测试：从 UI 层锚定两个视图命令的契约。
//
// 1) 适配窗口（fitView）：点击后所有可见图元（绘制的图形）应**精确缩放并充满整个绘图工作区域**。
//    坐标轴(drawAxes)与程序提示文字(drawForeground 的 HUD)是覆盖层、不在 model_->elements() 里，
//    天然不参与适配，且缩放到屏幕常量、保持原有显示不变（本测试以「内容包围盒填满视口」间接锚定）。
//
// 2) 窗口缩放（setZoomBoxMode + 框选）：进入框选模式 -> 左键拖拽框选目标区域 -> 释放后
//    该区域**精确缩放并充满整个绘图工作区域**，并自动退出框选模式；
//    未拖出有效矩形（单击）回退为「适配窗口」；退出模式后框选状态被清除。
//
// 取景的几何来源是 ElementItem::geometryBounds（**不含** cosmetic 描边余量），
// 故断言可用紧公差：若哪天误用带余量的 boundingRect()，缩放会偏小、内容不满视口，本测试立刻红。
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

    // 在视口像素 a -> b 之间拖一个左键框（用于窗口缩放框选）
    void dragBox(QPoint a, QPoint b) {
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
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
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
    app.processEvents();

    // 一条对角线：世界几何包围盒精确为 x[0,100] y[0,50]（宽高均非零，触发真正的内容适配）
    SketchCAD::Element el;
    el.type = SketchCAD::ElementType::Line;
    el.pts = { SketchCAD::Vec3(0, 0, 0), SketchCAD::Vec3(100, 50, 0) };
    el.visible = true;
    model.addElement(el);   // 触发 rebuild

    const double vw = view.viewport()->width();
    const double vh = view.viewport()->height();
    printf("[INFO] viewport = %g x %g, content = 100 x 50\n", vw, vh);

    // ============================================================
    // 1) 适配窗口：内容精确缩放并充满整个绘图工作区域
    // ============================================================
    view.fitView();
    app.processEvents();

    const double zFit = std::min(vw / 100.0, vh / 50.0);   // 等比：取受限（较小）边
    check(std::fabs(view.zoom() - zFit) < 1e-6,
          "适配窗口：缩放 = min(视口宽/内容宽, 视口高/内容高)（等比精确）",
          QString("zoom=%1 exp=%2").arg(view.zoom(), 0, 'f', 4).arg(zFit, 0, 'f', 4));

    const QPointF p00 = view.mapFromScene(QPointF(0, 0));      // 内容左下角
    const QPointF p11 = view.mapFromScene(QPointF(100, 50));   // 内容右上角
    const double cw = std::fabs(p11.x() - p00.x());            // 内容屏幕宽
    const double ch = std::fabs(p00.y() - p11.y());            // 内容屏幕高
    printf("[INFO] content screen rect: x[%.1f,%.1f] y[%.1f,%.1f]\n",
           std::min(p00.x(), p11.x()), std::max(p00.x(), p11.x()),
           std::min(p00.y(), p11.y()), std::max(p00.y(), p11.y()));

    // 受限维度应恰好充满视口（允许 1.5px 像素舍入）
    const bool fillW = std::fabs(cw - vw) < 1.5;
    const bool fillH = std::fabs(ch - vh) < 1.5;
    check(fillW || fillH, "适配窗口：内容在受限维度上充满视口（充满整个绘图工作区域）",
          QString("cw=%1 ch=%2 vs vw=%3 vh=%4")
              .arg(cw, 0, 'f', 1).arg(ch, 0, 'f', 1).arg(vw).arg(vh));
    // 未受限维度应居中
    const double cx = (p00.x() + p11.x()) / 2.0, cy = (p00.y() + p11.y()) / 2.0;
    check(std::fabs(cx - vw / 2.0) < 1.5 && std::fabs(cy - vh / 2.0) < 1.5,
          "适配窗口：内容在视口中居中",
          QString("center=(%1,%2) viewCenter=(%3,%4)")
              .arg(cx, 0, 'f', 1).arg(cy, 0, 'f', 1).arg(vw / 2, 0, 'f', 1).arg(vh / 2, 0, 'f', 1));

    // ============================================================
    // 2) 窗口缩放：框选目标区域，释放后该区域充满整个绘图工作区域
    // ============================================================
    view.fitView();                       // 先建立世界 -> 视口映射
    app.processEvents();
    check(!view.zoomBoxMode(), "初始未处于窗口缩放框选模式");

    // 目标子区域：世界 (20,10)-(60,40)
    const QPoint sa = view.mapFromScene(QPointF(20, 10));
    const QPoint sb = view.mapFromScene(QPointF(60, 40));
    // 框选实际代表的「世界矩形」由像素角点反查得到（与内部结算同源），据此推算期望缩放
    const QRectF rBox = QRectF(view.mapToScene(sa), view.mapToScene(sb)).normalized();
    const double zBox = std::min(vw / rBox.width(), vh / rBox.height());

    view.setZoomBoxMode(true);
    check(view.zoomBoxMode(), "setZoomBoxMode(true) 进入框选模式");

    view.dragBox(sa, sb);
    app.processEvents();

    check(std::fabs(view.zoom() - zBox) < 1e-6,
          "窗口缩放：框选区域精确缩放并充满绘图工作区域（等比）",
          QString("zoom=%1 exp=%2").arg(view.zoom(), 0, 'f', 4).arg(zBox, 0, 'f', 4));
    const QPointF cc = view.mapFromScene(rBox.center());   // 框选区域中心
    check(std::fabs(cc.x() - vw / 2.0) < 1.5 && std::fabs(cc.y() - vh / 2.0) < 1.5,
          "窗口缩放：框选区域中心落到视口中心",
          QString("center=(%1,%2)").arg(cc.x(), 0, 'f', 1).arg(cc.y(), 0, 'f', 1));
    check(!view.zoomBoxMode(), "窗口缩放：框选完成后自动退出框选模式");

    // ============================================================
    // 3) 单击（未拖出有效矩形）回退为「适配窗口」
    // ============================================================
    view.setZoomBoxMode(true);
    const QPointF c = view.mapFromScene(QPointF(40, 25));
    view.dragBox(c.toPoint(), c.toPoint());   // 原地点击，未拖出矩形
    app.processEvents();
    check(std::fabs(view.zoom() - zFit) < 1e-6 && !view.zoomBoxMode(),
          "窗口缩放：单击（未拖出矩形）回退为适配窗口并退出模式",
          QString("zoom=%1 exp=%2").arg(view.zoom(), 0, 'f', 4).arg(zFit, 0, 'f', 4));

    // ============================================================
    // 4) 退出模式清除框选状态（Esc/右键取消走同一路径）
    // ============================================================
    view.setZoomBoxMode(true);
    check(view.zoomBoxMode(), "再次进入框选模式");
    view.setZoomBoxMode(false);      // 等价于 Esc / 右键取消
    check(!view.zoomBoxMode(), "退出框选模式后 zoomBoxMode() 复位");

    printf("================ 结果: %s ================\n",
           g_fail == 0 ? "全部通过" : QString("%1 项失败").arg(g_fail).toUtf8().constData());
    return g_fail == 0 ? 0 : 1;
}
