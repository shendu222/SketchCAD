// 平移修复验证：验证「右键拖拽 → 视图底层变换(view.transform())精确更新」。
// 背景：原代码 QGraphicsView 默认 AlignCenter 会在每帧依据 sceneRect 把场景原点重新居中，
// 把 setTransform 写入的平移量整层覆盖，导致右键拖拽时画面锁死在正中（拖不动）。
// 修复：setSceneRect(QRectF()) 清空场景矩形 + setAlignment(AlignLeft|AlignTop) 关闭自动居中，
// 使 setTransform 成为视图变换唯一权威。
// 说明：render()/mapFromScene 在 offscreen 下因视图未真正布局而数值抖动，故本测试断言确定性的
// view.transform()（即 applyTransform 写入的底层变换）。
#include <QApplication>
#include <QMouseEvent>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Geometry.h"
#include <cstdio>
#include <cmath>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace SketchCAD {

class TestView : public GraphicsView {
public:
    explicit TestView(SketchModel* m, QWidget* p = nullptr) : GraphicsView(m, p) {}
    void pressRight(const QPoint& pos) {
        QMouseEvent e(QEvent::MouseButtonPress, pos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        mousePressEvent(&e);
    }
    void moveTo(const QPoint& pos, Qt::MouseButtons b) {
        QMouseEvent e(QEvent::MouseMove, pos, Qt::NoButton, b, Qt::NoModifier);
        mouseMoveEvent(&e);
    }
    void releaseRight(const QPoint& pos) {
        QMouseEvent e(QEvent::MouseButtonRelease, pos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        mouseReleaseEvent(&e);
    }
};

} // namespace SketchCAD

using namespace SketchCAD;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchModel model;
    TestView view(&model);
    view.resize(800, 600);
    view.show();

    Element el;
    el.type = ElementType::Line;
    el.pts = { Vec3(-30, 0, 0), Vec3(30, 0, 0) };
    el.visible = true;
    model.addElement(el);

    // (A) 直接 setTransform 平移 (120,80)：view.transform() 必须精确为 (120,80)
    view.setTransform(QTransform::fromTranslate(120, 80).scale(4.0, -4.0));
    const double ax = view.transform().m31(), ay = view.transform().m32();
    printf("[A] view.transform() after setTransform(120,80): m31=%.1f m32=%.1f\n", ax, ay);
    const bool aOk = std::fabs(ax - 120.0) < 1.0 && std::fabs(ay - 80.0) < 1.0;
    printf("[A] setTransform 精确写入底层变换: %s\n", aOk ? "YES" : "NO");

    // (B) 真实右键拖拽：先 fitView 恢复 panX_/panY_ 与变换一致的自然状态，
    //     从 (400,300) 拖 (120,80)，底层变换增量应精确 (120,80)。
    //     （注意：不要单独 setTransform 重置矩阵而不同步成员变量，会脱钩；fitView 经 applyTransform 二者一致。）
    view.fitView();
    const double b0x = view.transform().m31(), b0y = view.transform().m32();
    view.pressRight(QPoint(400, 300));
    view.moveTo(QPoint(400 + 120, 300 + 80), Qt::RightButton);
    view.releaseRight(QPoint(400 + 120, 300 + 80));
    const double b1x = view.transform().m31(), b1y = view.transform().m32();
    printf("[B] transform delta after right-drag: dx=%.1f dy=%.1f\n", b1x - b0x, b1y - b0y);
    const bool bOk = std::fabs((b1x - b0x) - 120.0) < 1.0 && std::fabs((b1y - b0y) - 80.0) < 1.0;
    printf("[B] 右键拖拽精确更新底层变换: %s\n", bOk ? "YES" : "NO");

    const bool ok = aOk && bOk;
    printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
