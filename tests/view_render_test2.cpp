// 渲染级根因验证：对比「右键拖拽前后」两帧真实渲染出的像素。
// 背景：既有 ViewPanTest / ViewRenderTest 只断言 view.transform()（矩阵），
// 而「矩阵正确 ≠ 画面跟随」——QGraphicsView 的 alignment/scrollbar 机制可能在渲染时
// 用补偿偏移抵消矩阵平移量，导致「日志在变、矩阵在变、画面纹丝不动」。
// 本测试：offscreen 下渲染两帧 QImage，求红色标记物的像素质心，验证拖拽 (dx,dy) 后
// 画面内容是否精确位移 (dx,dy)（像素级），从渲染侧证明/证伪根因。
#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsRectItem>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/Element.h"
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
    void moveTo(const QPoint& pos) {
        QMouseEvent e(QEvent::MouseMove, pos, Qt::NoButton, Qt::RightButton, Qt::NoModifier);
        mouseMoveEvent(&e);
    }
    void releaseRight(const QPoint& pos) {
        QMouseEvent e(QEvent::MouseButtonRelease, pos, Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        mouseReleaseEvent(&e);
    }
};

} // namespace SketchCAD

using namespace SketchCAD;

// 渲染当前视口到 QImage，返回红色像素的质心（像素坐标）；无红色像素返回 (-1,-1)
static QPointF renderRedCentroid(QGraphicsView* v, const QString& dumpPath = QString()) {
    const QSize sz = v->viewport()->size();
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        v->viewport()->render(&p);
    }
    if (!dumpPath.isEmpty()) img.save(dumpPath);
    double sx = 0, sy = 0;
    long n = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            if (qRed(c) > 150 && qGreen(c) < 100 && qBlue(c) < 100) { sx += x; sy += y; ++n; }
        }
    }
    return n ? QPointF(sx / n, sy / n) : QPointF(-1, -1);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchModel model;
    TestView view(&model);
    view.resize(800, 600);
    view.show();
    app.processEvents();

    // 一个普通图元（确保场景非空；同时把有效 sceneRect 从「空」落到 itemsBoundingRect）
    Element el;
    el.type = ElementType::Line;
    el.pts = { Vec3(-30, 0, 0), Vec3(30, 0, 0) };
    el.visible = true;
    model.addElement(el);   // 触发 rebuild

    // 红色标记：世界坐标 (-10,-10,20,20)，作为像素位移的基准物
    auto* marker = view.scene()->addRect(QRectF(-10, -10, 20, 20), QPen(Qt::NoPen), QBrush(QColor(220, 40, 40)));
    marker->setZValue(100);

    // 先把视图摆到「原点在视口中心、zoom=4」的自然状态
    view.fitView();
    app.processEvents();

    // 场景矩形信息（用于判断 alignment 补偿是否参与）
    const QRectF sr = view.scene()->sceneRect();
    const QRectF br = view.scene()->itemsBoundingRect();
    printf("[INFO] sceneRect=(%.1f,%.1f,%.1f,%.1f)  itemsBoundingRect=(%.1f,%.1f,%.1f,%.1f)  viewport=%dx%d  zoom=4.0\n",
           sr.x(), sr.y(), sr.width(), sr.height(),
           br.x(), br.y(), br.width(), br.height(),
           view.viewport()->width(), view.viewport()->height());

    const QPointF c0 = renderRedCentroid(&view, QStringLiteral("C:/Users/shend/AppData/Local/Temp/pan_f0.png"));
    printf("[F0] 红色标记质心 = (%.1f, %.1f)  transform.m31/m32=(%.2f,%.2f)\n",
           c0.x(), c0.y(), view.transform().m31(), view.transform().m32());

    // 右键拖拽 (+40, +25) —— 与 ViewPanTest 完全相同的输入
    const QPoint start(400, 300);
    const QPoint end(440, 325);
    view.pressRight(start);
    view.moveTo(end);
    view.releaseRight(end);
    app.processEvents();

    const QPointF c1 = renderRedCentroid(&view, QStringLiteral("C:/Users/shend/AppData/Local/Temp/pan_f1.png"));
    const double dx = c1.x() - c0.x(), dy = c1.y() - c0.y();
    printf("[F1] 红色标记质心 = (%.1f, %.1f)  transform.m31/m32=(%.2f,%.2f)\n",
           c1.x(), c1.y(), view.transform().m31(), view.transform().m32());
    printf("[DELTA] 质心位移 = (%.2f, %.2f)   期望 = (40.00, 25.00)\n", dx, dy);

    // 矩阵增量（既有测试已证明恒等于 (40,25)——本测试要与「渲染」对比）
    const double mx = c1.x() != -1 && c0.x() != -1 ? dx : -1;
    const double my = c1.y() != -1 && c0.y() != -1 ? dy : -1;

    const bool moved = std::fabs(mx - 40.0) < 6.0 && std::fabs(my - 25.0) < 6.0;   // 画面跟手
    const bool stuck = std::fabs(mx) < 2.0 && std::fabs(my) < 2.0;                 // 画面锁死
    printf("[CHECK] 画面跟随平移=%d  画面锁死=%d\n", moved, stuck);
    printf("RESULT: %s\n", moved ? "PASS" : (stuck ? "STUCK(BUG 复现)" : "INDETERMINATE"));
    return moved ? 0 : (stuck ? 2 : 3);
}
