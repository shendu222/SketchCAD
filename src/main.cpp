#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QWidget>
#include "ui/MainWindow.h"

namespace {

// 启动画面最短停留时长（ms）：主窗口初始化通常很快，补足 2~3 秒区间的观感。
constexpr int kMinSplashMs = 2500;
// 主窗口就绪后仍须保持可见的时长（ms）：初始化耗时意外偏长时，确保画面看得清。
constexpr int kMinVisibleAfterReadyMs = 800;

// 候选封面图路径：exe 同目录（build/ 或 dist/ 打包布局）→ 工程根（源码运行时 build 的上级）
// → 当前工作目录；每种目录下按 jpeg/jpg/png/bmp 依次尝试（BMP 为 Qt 内建格式，
// 即使 Qt 的图像格式插件未随程序部署也能解码，用作最后兜底）。
QStringList locateCoverCandidates() {
    const QStringList names = { QStringLiteral("CAD封面.jpeg"), QStringLiteral("CAD封面.jpg"),
                                QStringLiteral("CAD封面.png"),  QStringLiteral("CAD封面.bmp") };
    const QString exeDir = QCoreApplication::applicationDirPath();
    QStringList out;
    for (const QString& n : names) {
        out << exeDir + QStringLiteral("/") + n
            << exeDir + QStringLiteral("/../") + n
            << QDir::current().filePath(n);
    }
    return out;
}

// 封面图：pix 为按 devicePixelRatio 采样后的位图，logical 是它真正的显示尺寸（逻辑像素）。
// ⚠️ 关键：QPixmap::size()/width() 返回的是【物理像素】而非逻辑像素，
//    高 DPI 屏（dpr>1）下若把 size() 当窗口尺寸用，窗口会被放大 dpr 倍进而超出屏幕
//    （实测 dpr=1.25 时窗口 1612 宽 > 可用区 1536，居中后左上角为 (-39,3)，画面跑到屏幕外）。
//    因此窗口尺寸与居中计算一律使用 logical。
struct CoverImage {
    QPixmap pix;
    QSize   logical = QSize(0, 0);
};

// 加载封面并适配屏幕：
//   * 只缩不放——图片小于屏幕可用区时按原始尺寸居中展示（保持原始比例，不变形）；
//   * 超出可用区（宽高各留 8% 边距，不遮挡任务栏）才等比缩小（KeepAspectRatio + 平滑采样）；
//   * 高 DPI 屏以物理像素采样后设置 devicePixelRatio，避免逻辑缩放把图放大发虚。
CoverImage loadCoverPixmap(const QString& path) {
    QPixmap pix(path);
    if (pix.isNull()) return {};
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return { pix, pix.size() };
    const qreal dpr = screen->devicePixelRatio();
    const QSize avail = screen->availableGeometry().size() * 0.84;
    QSize logical = pix.size();                  // 文件像素即逻辑像素（加载时 dpr=1）
    if (logical.width() > avail.width() || logical.height() > avail.height())
        logical.scale(avail, Qt::KeepAspectRatio);
    if (logical == pix.size() && dpr <= 1.0)
        return { pix, logical };                 // 无需任何变换：原始像素直出
    // logical 已是比例正确的目标逻辑尺寸，按其物理像素尺寸做高质量重采样
    QPixmap scaled = pix.scaled(QSize(int(logical.width() * dpr), int(logical.height() * dpr)),
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return { scaled, logical };
}

// 启动画面诊断日志（exe 同级 splash_debug.log）：Windows 上画面不可见时用于取证，
// 记录封面加载、屏幕可用区、窗口几何与停留时长。不影响正常启动，可随时移除。
void logSplash(const QString& msg) {
    static QFile f(QCoreApplication::applicationDirPath() + QStringLiteral("/splash_debug.log"));
    if (!f.isOpen() && !f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << msg << '\n';
    ts.flush();
}

// 逐个尝试候选路径，返回第一个能被 Qt 真正解码的封面。
// ⚠️ 注意："文件存在" 不等于 "能解码"——独立部署目录（dist/）若缺少
//    imageformats/qjpeg.dll，JPEG 文件虽在却会加载失败，因此必须逐个真加载一次。
CoverImage loadCoverImage(QString* usedPath = nullptr) {
    for (const QString& p : locateCoverCandidates()) {
        if (!QFileInfo::exists(p)) continue;
        const CoverImage c = loadCoverPixmap(p);
        if (!c.pix.isNull() && c.logical.isValid()) {
            if (usedPath) *usedPath = p;
            return c;
        }
        logSplash(QStringLiteral("封面存在但解码失败: ") + p);
    }
    // 全部失败：记录 Qt 当前可用格式，一眼判断是否图像插件未部署（如缺 qjpeg.dll）
    const QList<QByteArray> fmts = QImageReader::supportedImageFormats();
    QStringList names;
    for (const QByteArray& b : fmts) names << QString::fromLatin1(b);
    logSplash(QStringLiteral("全部候选封面均不可用；Qt 当前支持格式: ")
              + (names.isEmpty() ? QStringLiteral("<无>") : names.join(QStringLiteral(","))));
    return {};
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // ---- 启动画面 ----
    // 不用 QSplashScreen：其窗口初始几何由平台决定（不同桌面/缩放策略下会落到屏幕左上角），
    // 这里改用完全可控的无边框置顶窗口，位置与尺寸由 setGeometry 明确指定。
    QWidget* splash = nullptr;
    QElapsedTimer splashTimer;
    QString coverPath;
    const CoverImage cover = loadCoverImage(&coverPath);   // 找不到或解码失败均返回空
    if (cover.pix.isNull() || !cover.logical.isValid()) {
        // 封面缺失 / 解码失败：splash 保持 nullptr，直接走正常启动流程。
        logSplash(QStringLiteral("启动画面已跳过（封面不可用），程序正常启动"));
    } else {
        // 无边框/无标题栏 + 置顶；不接受焦点且不吞输入，避免与主窗口抢焦点导致闪烁。
        splash = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                                     Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        splash->setFixedSize(cover.logical);             // 窗口 = 图片逻辑尺寸（原始比例不变形）
        auto* lab = new QLabel(splash);
        lab->setPixmap(cover.pix);
        lab->setGeometry(0, 0, cover.logical.width(), cover.logical.height());
        if (QScreen* scr = QGuiApplication::primaryScreen()) {
            const QRect avail = scr->availableGeometry();
            const QPoint tl(avail.center().x() - cover.logical.width() / 2,
                            avail.center().y() - cover.logical.height() / 2);
            splash->move(tl);                            // 显式居中，不依赖平台默认位置
        }
        splash->show();
        splash->raise();
        // 强制首帧同步绘制：随后主窗口初始化会长时间占用主线程，
        // 仅排队绘制的话画面在进入事件循环前根本画不出来（表现为"一闪而过"）。
        splash->repaint();
        app.processEvents();
            splashTimer.start();                          // 可见时长从"画面已绘制"起算
    }
    // 封面缺失/加载失败：splash 保持 nullptr，直接走正常启动流程。

    SketchCAD::MainWindow w;                              // 资源与界面初始化（此时画面已稳定显示）
    if (splash) {
        // 补足最短停留：取「剩余最短时长」与「就绪后仍需可见时长」的较大值，
        // 既保证 2~3 秒观感，也避免初始化极慢时刚画出来就消失。
        const qint64 remain = qMax<qint64>(kMinSplashMs - splashTimer.elapsed(),
                                           kMinVisibleAfterReadyMs);
        QEventLoop wait;                                  // 事件循环等待：不冻结进程，画面持续可刷新
        QTimer::singleShot(int(remain), &wait, &QEventLoop::quit);
        wait.exec();
    }
    w.show();                                             // 停留结束才显示主窗口，避免提前遮挡画面

    if (splash) {
        splash->close();
        delete splash;                                    // 主窗口已显示后关闭 → 平滑切换、无闪烁
    }
    return app.exec();
}
