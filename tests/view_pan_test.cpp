// 视图平移（右键拖拽）集成测试：用合成鼠标事件直接驱动 GraphicsView 的受保护事件处理器，
// 验证「右键拖拽 → 视图跟手平移」「右键单击（不移动）→ 不平移、走原结束/取消语义」。
// 运行平台：QT_QPA_PLATFORM=offscreen（无真实显示亦可）。
#include <QApplication>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include <cstdio>
#include <cmath>

namespace SketchCAD {

// 借由派生类访问受保护的 mousePress/Move/Release，等效于真实右键交互。
class TestView : public GraphicsView {
public:
    explicit TestView(SketchModel* m, QWidget* p = nullptr) : GraphicsView(m, p) {}

    // dx,dy：相对按下点的位移（逻辑像素）；doMove=true 模拟「拖拽」，false 模拟「单击」。
    void simulateRight(int dx, int dy, bool doMove) {
        const QPointF start(100, 100);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        QMouseEvent press(QEvent::MouseButtonPress, start, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        mousePressEvent(&press);
        if (doMove) {
            QMouseEvent move(QEvent::MouseMove, QPointF(100 + dx, 100 + dy),
                             Qt::NoButton, Qt::RightButton, Qt::NoModifier);
            mouseMoveEvent(&move);
        }
        const QPointF end = doMove ? QPointF(100 + dx, 100 + dy) : start;
        QMouseEvent release(QEvent::MouseButtonRelease, end, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        mouseReleaseEvent(&release);
#pragma GCC diagnostic pop
    }

    // 用 QApplication::sendEvent 把「真实」右键事件派发进来，走与真机完全一致的链路：
    //   viewport() -> GraphicsView::viewportEvent()（打点处）-> QAbstractScrollArea 转发 -> mousePressEvent 等
    // 关键：必须发给 viewport() 而不是 this。QGraphicsView 的鼠标事件由视口控件接收，
    // 直接发给 view 本身既不会经过 viewportEvent() 打点，也不会被转发到鼠标处理器。
    void simulateRightSend(int dx, int dy) {
        const QPointF start(200, 200);
        QWidget* vp = viewport();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        QMouseEvent press(QEvent::MouseButtonPress, start, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(200 + dx, 200 + dy),
                         Qt::NoButton, Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &move);
        // 释放时按键已抬起：buttons 应为 NoButton，与真实平台派发保持一致
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(200 + dx, 200 + dy),
                            Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &release);
#pragma GCC diagnostic pop
    }
};

} // namespace SketchCAD

// 读取本进程写入的平移排查日志（%TEMP%/sketchcad_pan.log）。
// 日志在进程内以 Truncate 方式打开，故文件内容即本次运行的全部打点。
static QString readPanLog(QString* pathOut = nullptr) {
    const QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                            + QStringLiteral("/sketchcad_pan.log");
    if (pathOut) *pathOut = logPath;
    QFile lf(logPath);
    if (!lf.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QString s = QTextStream(&lf).readAll();
    lf.close();
    return s;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchCAD::SketchModel model;
    SketchCAD::TestView view(&model);
    view.resize(800, 600);
    view.show();

    const double dx0 = view.transform().dx();
    const double dy0 = view.transform().dy();

    // 1) 右键拖拽 (+40, +25)：视图平移分量应增加约 (40, 25)
    view.simulateRight(40, 25, true);
    const double dx1 = view.transform().dx();
    const double dy1 = view.transform().dy();
    const double adx = dx1 - dx0, ady = dy1 - dy0;
    printf("[PAN]   dx0=%.2f dy0=%.2f  dx1=%.2f dy1=%.2f  ddx=%.2f ddy=%.2f\n",
           dx0, dy0, dx1, dy1, adx, ady);

    // 2) 右键单击（不移动）：不应平移
    const double dx2 = view.transform().dx();
    const double dy2 = view.transform().dy();
    view.simulateRight(0, 0, false);
    const double dx3 = view.transform().dx();
    const double dy3 = view.transform().dy();
    printf("[CLICK] ddx=%.2f ddy=%.2f\n", dx3 - dx2, dy3 - dy2);

    const bool panOk = std::fabs(adx - 40.0) < 1.0 && std::fabs(ady - 25.0) < 1.0;
    const bool clickOk = std::fabs(dx3 - dx2) < 1e-6 && std::fabs(dy3 - dy2) < 1e-6;
    printf("[CHECK] panOk=%d clickOk=%d\n", panOk, clickOk);

    // 3) 端到端：用 QApplication::sendEvent 走真实派发链路（viewport -> viewportEvent -> handler），
    //    既验证平移生效，也验证 viewportEvent 打点确实抓得到原始右键事件。
    const QString logBefore = readPanLog();
    const double dx4 = view.transform().dx();
    const double dy4 = view.transform().dy();
    view.simulateRightSend(60, 40);
    const double dx5 = view.transform().dx();
    const double dy5 = view.transform().dy();
    const double sdx = dx5 - dx4, sdy = dy5 - dy4;

    QString logPath;
    const QString logAfter = readPanLog(&logPath);
    // 只在本次派发新增的日志片段里查找，避免被前面直接调 handler 的打点干扰
    const QString delta = logAfter.mid(logBefore.size());
    const bool hasRaw  = delta.contains(QStringLiteral("[raw] Press button=2"));
    const bool hasRawMove = delta.contains(QStringLiteral("[raw] Move"));
    const bool hasMove = delta.contains(QStringLiteral("[move] 平移中"));
    const bool sendOk  = std::fabs(sdx - 60.0) < 1.0 && std::fabs(sdy - 40.0) < 1.0;

    printf("[SEND]  ddx=%.2f ddy=%.2f  (期望 60.00 / 40.00)\n", sdx, sdy);
    printf("[LOG]   path=%s\n", logPath.toUtf8().constData());
    printf("[LOG]   viewportEvent 抓到原始右键按下=%d  原始移动=%d  平移中日志=%d\n",
           hasRaw, hasRawMove, hasMove);
    printf("[CHECK] sendOk=%d\n", sendOk);

    const bool ok = panOk && clickOk && sendOk && hasRaw && hasRawMove && hasMove;
    printf(ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return ok ? 0 : 1;
}
