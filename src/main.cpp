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
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QWidget>
#include "core/Diagnostics.h"   // 崩溃不丢稿：日志滚动 + 崩溃转储
#include "core/I18n.h"         // 国际化：语言解析 + QTranslator 装载
#include "core/DwgBridge.h"     // 启动诊断：DWG 转换组件发现状态
#include "ui/MainWindow.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
    // ---- Per-Monitor V2 DPI 感知声明（必须在任何窗口创建之前）----
    // Qt6 高 DPI 缩放恒启用（AA_EnableHighDpiScaling 已弃用，设置反而触发弃用警告），
    // 这里补声明 Windows Per-Monitor V2：多显示器不同缩放率下窗口跨屏拖动即时重排，
    // 125%/150%/200% 混合缩放不再出现"图像拉伸/模糊"。
    // 动态加载 user32（Win10 1703+），失败回落 shcore 的 SetProcessDpiAwareness(2)，
    // 两者都失败则保持系统默认（进程未声明感知 = 由 DWM 位图拉伸）。
    // 不依赖 <winuser.h> 的常量声明：PER_MONITOR_AWARE_V2 的句柄值就是 (void*)-4。
#ifdef Q_OS_WIN
    {
        using SetDpiCtxFn  = BOOL(WINAPI*)(void*);   // DPI_AWARENESS_CONTEXT 即句柄指针
        using SetDpiAwareFn = HRESULT(WINAPI*)(int);
        const HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
        if (user32) {
            const auto setCtx = reinterpret_cast<SetDpiCtxFn>(
                reinterpret_cast<void*>(::GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (DPI_AWARENESS_CONTEXT)-4
            if (setCtx && setCtx(reinterpret_cast<void*>(-4))) {
                // Per-Monitor V2 生效
            } else if (const HMODULE shcore = ::LoadLibraryW(L"shcore.dll")) {
                const auto setDpi = reinterpret_cast<SetDpiAwareFn>(
                    reinterpret_cast<void*>(::GetProcAddress(shcore, "SetProcessDpiAwareness")));
                if (setDpi) setDpi(2 /*PROCESS_PER_MONITOR_DPI_AWARE*/);
            }
        }
    }
#endif

    // ---- 高 DPI 策略早期读取（必须在 QApplication 构造之前）----
    // Qt6 中高 DPI 缩放恒启用（AA_EnableHighDpiScaling 已弃用），启动前唯一可配置的
    // 是缩放舍入策略。这里用独立 QSettings 读 display/highDpiRoundingPolicy（与
    // AppSettings 同一注册表路径 HKCU\Software\SketchCAD）：注册表单键读取，不阻塞
    // 启动；非法值一律回落 Round（与 AppSettings 校验表一致）。真正的用户偏好读写
    // 在「选项」对话框（display/highDpiRoundingPolicy）。
    QString preferredLang;   // 界面语言原始值：""(缺省)=跟随系统 / zh-CN / en
    {
        const QSettings pre(QSettings::NativeFormat, QSettings::UserScope,
                            QStringLiteral("SketchCAD"), QString());
        const QString pol = pre.value(QLatin1String("display/highDpiRoundingPolicy"),
                                      QStringLiteral("Round")).toString();
        using P = Qt::HighDpiScaleFactorRoundingPolicy;
        P p = P::Round;
        if (pol == QLatin1String("PassThrough"))          p = P::PassThrough;
        else if (pol == QLatin1String("Floor"))           p = P::Floor;
        else if (pol == QLatin1String("Ceil"))            p = P::Ceil;
        else if (pol == QLatin1String("RoundPreferFloor"))p = P::RoundPreferFloor;
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(p);
        // 语言同样走注册表单键早期读取（general/language，AppSettings 同路径）：
        // 空/缺键 = 跟随系统（中文系统直接用源语言，零感知）
        preferredLang = pre.value(QLatin1String("general/language"), QString()).toString();
    }

    QApplication app(argc, argv);

    // ---- 界面语言装载（必须在 QApplication 之后：installTranslator 需要应用实例）----
    // 主窗口/对话框构造中的 tr() 由此取到译文；zh-CN 为源语言不装翻译器。
    SketchCAD::I18n::installTranslator(SketchCAD::I18n::resolveLanguage(preferredLang));

    // ---- 崩溃不丢稿：诊断子系统（Qt 日志滚动落盘 + Windows 崩溃 mini-dump）----
    // 必须在 QApplication 构造之后、主窗口构造之前安装：消息处理器依赖事件循环上下文，
    // 崩溃过滤器则要抢在任何窗口初始化失败之前。
    SketchCAD::Diagnostics::init(QStringLiteral("SketchCAD"));
    SketchCAD::Diagnostics::writeLog(QStringLiteral("SketchCAD 启动"), "info");
    // 启动路径诊断：记录 exe 实际目录与 DWG 转换组件发现状态，便于用户在
    // 「打开 DWG 失败」时从 %LOCALAPPDATA%/SketchCAD/logs 直接定位根因
    // （例如 dist/ 打包时漏拷 tools/dwg → available()==false）。
    {
        const QString exeDir = QCoreApplication::applicationDirPath();
        const QString td = SketchCAD::dwgbridge::resolvedToolsDir();
        const bool dwgOk = SketchCAD::dwgbridge::available();
        SketchCAD::Diagnostics::writeLog(
            QStringLiteral("启动路径诊断：exeDir=%1，DWG转换组件=%2，toolsDir=%3")
                .arg(exeDir)
                .arg(dwgOk ? QStringLiteral("已就绪") : QStringLiteral("缺失"))
                .arg(td.isEmpty() ? QStringLiteral("(未找到)") : td),
            "info");
    }
    // 会话标记：启动即写 → 正常退出删除 → 残留 = 上次异常退出（主窗口启动后弹提示）。
    // 检测结果缓存在 Diagnostics 内，MainWindow::showAbnormalExitNotice 稍后消费。
    SketchCAD::Diagnostics::beginSession();

    // 退出钩子：正常退出时收尾（MainWindow::closeEvent 已停自动保存并清理 .tmp）
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []{
        SketchCAD::Diagnostics::endSession();   // 清会话标记 → 下次启动不误报异常退出
        SketchCAD::Diagnostics::writeLog(QStringLiteral("SketchCAD 正常退出"), "info");
    });

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

    // ---- 启动对话框流（修复"卡在 splash"假死）----
    // 此刻主窗口已 show、splash 已销毁，才允许弹出启动页/隐私明示/异常退出提示。
    // ❌ 旧实现的缺陷：这三个模态对话框在 MainWindow 构造期以 singleShot(0) 注册，
    //    zero-timer 在上方 wait.exec()（splash 最短停留的嵌套事件循环）里立即抢跑，
    //    模态框被置顶（WindowStaysOnTopHint）的 splash 完全遮挡——用户看到的是
    //    "永远停在启动画面"，实际是在等一个看不见的模态框（异常退出提示必弹）。
    // ✅ 现改为 app.exec() 首拍（splash 已不存在）经 runStartupDialogFlow 显式触发。
    QTimer::singleShot(0, &w, &SketchCAD::MainWindow::runStartupDialogFlow);
    return app.exec();
}
