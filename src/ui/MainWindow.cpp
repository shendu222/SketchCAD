#include "MainWindow.h"
#include <QSet>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include "core/SketchModel.h"
#include "ui/TableDialog.h"         // 表格编辑器（双击 Table / 表格创建共用）
#include "ui/GroupManagerDialog.h"  // 编组管理器（GROUP）
#include "ui/QSelectDialog.h"       // 快速选择（QSELECT）
#include "ui/CommandPalette.h"      // 命令面板（Ctrl+Shift+P）
#include "ui/DiagnosticsDialog.h"   // 诊断包导出对话框（异常退出提示/帮助菜单共用）
#include "core/CommandCatalog.h"    // 快捷键冲突自检（ShortcutAudit 纯函数）
#include "core/Telemetry.h"         // 遥测出口（默认关闭零外发）
#include <functional>               // auditShortcutConflicts 递归收集 std::function
#include "ui/FilterDialog.h"        // 过滤器（FILTER）
#include "ui/UnitsLimitsDialog.h"   // 单位与图纸界限（UNITS / LIMITS）
#include "ui/SnapSettingsDialog.h"   // 捕捉设置（OSNAP）
#include "core/CommandInterpreter.h"
#include "core/ScriptHost.h"          // 脚本宿主（二次开发：JS/Python/VBA）
#include "core/MacroScript.h"         // 动作宏录制 / SCR 脚本批处理
#include "core/PluginManager.h"       // 插件管理器（原生 SDK 二次开发）
#include "core/CuiManager.h"          // 界面定制（CUIx）：工具条 / 工作空间
#include "core/AliasTable.h"          // 命令别名（acad.pgp 兼容）
#include "core/Element.h"
#include "core/ElementValidate.h"   // 鲁棒性守卫层：读档前图元只读校验/丢弃/修复
#include "core/Geometry.h"
#include "core/DimStyle.h"
#include "core/DimensionStandard.h"
#include "core/Vec.h"
#include "core/ToolRegistry.h"
#include "ui/GraphicsView.h"
#include "ui/ModelMediator.h"
#include "ui/RulerWidget.h"
#include <QTabBar>
#include <QStackedWidget>
#include <QShortcut>
#include <QListWidget>     // 启动页「打开最近」列表
#include <QGridLayout>     // 启动页模板按钮网格
#include <QBrush>          // 启动页缺失项灰显前景
#include <QGroupBox>       // 启动页分组框
#include "ui/BomTree.h"
#include "ui/ElementPanel.h"
#include "ui/PropertyPanel.h"
#include "ui/LogPanel.h"
#include "ui/LayerPanel.h"
#include "ui/CommandLine.h"
#include "ui/StatusBar.h"
#include "ui/icons.h"
#include "ui/NewDrawingDialog.h"
#include "ui/PaperSizeDialog.h"
#include "ui/TitleBlockForm.h"
#include "ui/ModifyInputDialog.h"
#include "ui/AircraftViewsDialog.h"
#include "ui/FlangeViewsDialog.h"
#include "ui/BearingViewsDialog.h"
#include "ui/HousingViewsDialog.h"
#include "ui/FlangeHubViewsDialog.h"
#include "ui/FloorPlanViewsDialog.h"
#include "ui/ArrayDialog.h"
#include "ui/BlockDialogs.h"
#include "ui/BlockEditorDialog.h"
#include "ui/DesignCenterPanel.h"
#include "ui/SectionDialog.h"
#include "ui/RegionManagerDialog.h"
#include "ui/CleanupDialog.h"
#include "ui/InquiryDialog.h"
#include "ui/PrintSettingsDialog.h"
#include "ui/PublishDialog.h"
#include "ui/PrintPreviewDialog.h"
#include "ui/PrintEngine.h"
#include "ui/CrashRecoveryDialog.h"     // 启动恢复对话框（.sv$ 快照）
#include "ui/AutoSaveOptionsDialog.h"   // 自动保存设置
#include "core/AutoSave.h"              // 自动保存核心
#include "core/AppSettings.h"           // 全局设置（HKCU\Software\SketchCAD）
#include "core/ConstraintSolverFacade.h" // 约束求解路由门面（注入 AppSettings 供特性开关读取）
#include "core/I18n.h"                  // 界面语言单一入口（翻译器装载/语言解析）
#include "core/RecentFiles.h"           // 最近文件 MRU（路径+时间+pin）
#include "ui/OptionsDialog.h"           // 统一「选项」对话框（工具 → 选项...）
#include "core/Diagnostics.h"           // 日志滚动落盘（供保存失败等记日志）
#include "version.h"                    // 版本宏（由 CMake project VERSION 生成；关于对话框用）
#include "core/Section.h"
#include "core/SheetSpec.h"
#include "core/Modify.h"
#include "core/ChamferFillet.h"
#include "core/CadFormats.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QShortcut>     // 正交 F8 / 栅格吸附 F9 的全局快捷键
#include <QComboBox>
#include <QProgressDialog>   // P0-04：大图纸解析进度（可取消）
#include <QEventLoop>        // P0-04：模态等待事件循环
#include <QFutureWatcher>    // P0-04：线程池任务完成通知
#include <QDateTime>         // P0-04：打开耗时计时
#include <QtConcurrent/QtConcurrent>   // P0-04：QtConcurrent::run 后台解析
#include <atomic>            // P0-04：解析取消标志
#include <new>               // P0-04：std::bad_alloc
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QAction>
#include <QActionGroup>
#include <QSet>
#include <QKeySequence>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrinterInfo>      // 选项：默认打印设备（可用打印机名列表）
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include <QPointF>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QCoreApplication>   // applicationDirPath（插件目录定位）
#include <QDesktopServices>   // 命令别名文件：用系统默认编辑器打开；日志目录打开
#include <QUrl>               // QUrl::fromLocalFile（诊断包：打开日志目录）
#include <QStandardPaths>     // 配置目录定位（别名 / CUIx 默认路径）
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QCloseEvent>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {

// 工具条按钮统一最小宽度（px）：图标 20 + 文字 2 汉字 + 内边距，
// 保证 1~2 个汉字的短按钮与长按钮对齐时不参差不齐（见 MainWindow::uniformButtonWidth）。
static const int kToolButtonMinWidth = 64;

MainWindow::MainWindow(QWidget* p) : QMainWindow(p) {
    // 多文档会话表：首张图纸（会话 0）在构造期创建；model_/view_ 从此只是
    // 「活动文档」的借用指针，切换标签时由 switchToDoc 重绑（见 hookActiveDoc）。
    docs_ = new DocManager(this);
    DocSession* firstDoc = docs_->addNewDoc();
    model_ = firstDoc->model();
    interp_ = new CommandInterpreter(model_);

    // 脚本宿主：构建统一交互面与语言引擎策略表，并实时把脚本输出/错误汇入日志面板
    scriptHost_ = new ScriptHost(model_, interp_, this);
    connect(scriptHost_, &ScriptHost::scriptOutput, this,
            [this](const QString& s) { model_->log(s, QStringLiteral("script")); });
    connect(scriptHost_, &ScriptHost::scriptError, this,
            [this](const QString& s) { model_->log(s, QStringLiteral("error")); });

    // 插件管理器：构建原生 SDK 插件框架（注册/加载/生命周期），并实时把插件事件汇入日志面板
    pluginManager_ = new PluginManager(model_, interp_, this);
    connect(pluginManager_, &PluginManager::pluginLoaded, this,
            [this](const QString& name, const QString&) {
                model_->log(QStringLiteral("插件已加载：%1").arg(name), QStringLiteral("plugin"));
                refreshPluginMenu();
            });
    connect(pluginManager_, &PluginManager::pluginUnloaded, this,
            [this](const QString& name, const QString&) {
                model_->log(QStringLiteral("插件已卸载：%1").arg(name), QStringLiteral("plugin"));
                refreshPluginMenu();
            });
    connect(pluginManager_, &PluginManager::pluginError, this,
            [this](const QString& path, const QString& msg) {
                model_->log(QStringLiteral("插件加载失败 [%1]：%2").arg(path).arg(msg),
                            QStringLiteral("error"));
                refreshPluginMenu();
            });
    // 启动时自动扫描宿主同目录下的 plugins/ 子目录（如存在示例插件 SamplePlugin.dll）。
    pluginManager_->loadDirectory(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));

    bom_ = new BomTree(model_);
    elemPanel_ = new ElementPanel();
    propPanel_ = new PropertyPanel(model_);
    logPanel_ = new LogPanel();
    cmdLine_ = new CommandLine(model_, interp_);
    statusBar_ = new StatusBar();
    setStatusBar(statusBar_);

    // 先建面板与工具条，菜单需要引用它们的显隐开关（toggleViewAction）
    buildDocks();
    buildToolbar();
    buildMenu();
    initCui();   // 注册内置 UI 元素、载入默认别名/CUIx、应用界面配置（#360/#361）

    buildParamBar();   // 参数输入条先建（中央部件自上而下组装时需要）
    buildDocArea();    // 多文档区：标签条 + 页栈 + 首页（创建 view_ 与活动文档标尺）

    connectSignals();

    statusBar_->setCount(0);
    statusBar_->setPlaneInfo(planeName(model_->plane()));
    statusBar_->setDrawScale(model_->scaleLabel());
    model_->log(QStringLiteral("航空CAD 草图模块已启动 (C++17 / Qt6)"));

    // ---- 输入约束（正交 ORTHO / 栅格吸附 SNAP）：三入口之「状态栏按钮」与「快捷键」----
    // 单一状态源在 SketchModel：按钮点击、F8/F9 快捷键、命令行 ortho/snap 三者都只调
    // setOrthoMode / setGridSnap；再由 orthoModeChanged / gridSnapChanged 回写按钮勾选态，
    // 从机制上保证「三入口状态永远一致」，不存在各存一份导致的漂移。
    {
        auto makeToggle = [this](const QString& text, const QString& tip, const QKeySequence& key,
                                 auto getter, auto setter, QToolButton** btnSlot) {
            auto* btn = new QToolButton(statusBar_);
            btn->setText(text);
            btn->setCheckable(true);
            btn->setToolTip(tip + QStringLiteral("（快捷键 %1）").arg(key.toString()));
            btn->setChecked((model_->*getter)());
            connect(btn, &QToolButton::toggled, this, [this, setter, text](bool on) {
                (model_->*setter)(on);
                model_->log(QStringLiteral("%1：%2").arg(text).arg(on ? QStringLiteral("开") : QStringLiteral("关")));
            });
            // 模型信号 → 按钮勾选态的连接在 hookActiveDoc 登记（随活动文档换绑）
            // 全局快捷键（应用级，画布有焦点时同样生效）
            auto* sc = new QShortcut(key, this);
            sc->setContext(Qt::ApplicationShortcut);
            connect(sc, &QShortcut::activated, this, [this, getter, setter, text]() {
                const bool on = !(model_->*getter)();
                (model_->*setter)(on);
                model_->log(QStringLiteral("%1：%2").arg(text).arg(on ? QStringLiteral("开") : QStringLiteral("关")));
            });
            statusBar_->addWidget(btn);
            *btnSlot = btn;
        };
        makeToggle(QStringLiteral("正交"), QStringLiteral("正交模式：把落点方向锁定到水平/垂直"),
                   QKeySequence("F8"), &SketchModel::orthoMode, &SketchModel::setOrthoMode,
                   &btnOrtho_);
        makeToggle(QStringLiteral("捕捉"), QStringLiteral("栅格吸附：把坐标量化到步长的整数倍"),
                   QKeySequence("F9"), &SketchModel::gridSnap, &SketchModel::setGridSnap,
                   &btnSnap_);
    }

    // 命令面板（Ctrl+Shift+P）：模糊搜索中文名/英文命令名/别名 → Enter 执行。
    // 与 F1（help 命令列表）互为补充：面板面向「找命令」，help 面向「学命令」。
    {
        auto* pal = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this);
        pal->setContext(Qt::ApplicationShortcut);
        connect(pal, &QShortcut::activated, this, &MainWindow::openCommandPalette);
    }

    // 默认窗口尺寸偏好 1366×720；先给一个合理的初始几何（落在屏幕可用区域内并居中），
    // 真正的"贴合屏幕"校正放到 showEvent 里在真实屏幕上下文中执行，避免被窗口管理器覆盖。
    {
        const int kDefaultWidth = 1366;
        const int kDefaultHeight = 720;
        QRect avail(0, 0, kDefaultWidth, kDefaultHeight);
        if (QScreen* screen = QGuiApplication::primaryScreen())
            avail = screen->availableGeometry();
        const int w = qMin(kDefaultWidth, avail.width());
        const int h = qMin(kDefaultHeight, avail.height());
        setGeometry(avail.left() + (avail.width() - w) / 2,
                    avail.top() + (avail.height() - h) / 2,
                    w, h);
    }

    setupAutoSave();   // 崩溃不丢稿：配置 + 自动保存 + 恢复（须在 statusBar_ 就绪后）

    // 应用级配置持久化：注册表读入用户习惯（几何/布局/显示开关/最近文件）。
    // setupAutoSave 已建好 appSettings_；此函数只做读取与回放，不阻塞启动。
    loadAppSettingsOnStartup();

    // 快捷键冲突自检：全部快捷键注册完毕后扫描重复绑定，warn 日志提示。
    auditShortcutConflicts();

    // 遥测开关同步（默认关闭；「选项 → 隐私」可改，改动经 applySettingLive 即时生效）。
    // 自测（s_recentPersistEnabled=false）不读注册表，保持默认关闭。
    if (appSettings_ && s_recentPersistEnabled)
        Telemetry::setEnabled(appSettings_->telemetryEnabled());

    // 启动对话框流（启动页/隐私明示/异常退出提示）同样不在此注册 singleShot(0)，
    // 原因同上（构造期 zero-timer 会在 splash 停留的 wait.exec() 里抢跑）。
    // 统一由 main() 在 w.show() 且 splash 销毁后经 runStartupDialogFlow() 触发。
}

// ---- 崩溃不丢稿：自动保存 + 恢复 ----

void MainWindow::setupAutoSave() {
    appSettings_ = new SketchCAD::AppSettings(nullptr, this);
    // 约束求解门面注入应用设置：特性开关 solver/constraintEnabled 经此读取（默认开）。
    ConstraintSolverFacade::instance().setSettings(appSettings_);
    autoSave_ = new SketchCAD::AutoSave(this);

    // 最近文件 MRU：存储复用 AppSettings（键 files/recentEntries），任何增删立即持久化；
    // changed → 重建「最近打开」菜单（离屏测试经 s_recentPersistEnabled 关闭落盘）
    recent_ = new SketchCAD::RecentFiles(appSettings_, this);
    recent_->setPersistEnabled(s_recentPersistEnabled);
    connect(recent_, &SketchCAD::RecentFiles::changed,
            this, &MainWindow::rebuildRecentMenu);

    // 命令行历史持久化：注入 AppSettings（键 commandLine/recentCommands），
    // 启动时预填 ↑↓ 历史（重启可用，AutoCAD 习惯）；此后每次入史同步写回。
    interp_->setPersistence(s_recentPersistEnabled ? appSettings_ : nullptr);

    // 快照字节来源与「写盘」同一序列化入口（SketchModel::toDrawingBytes），
    // 保证恢复出来的内容与当时保存的完全一致。
    autoSave_->setProvider([this]{ return model_->toDrawingBytes(); });
    autoSave_->setIntervalMinutes(appSettings_->autosaveIntervalMinutes());
    autoSave_->setLowDiskThresholdMb(appSettings_->lowDiskThresholdMb());
    autoSave_->setCurrentDrawing(model_->drawingPath());

    // 状态栏：显示「上次自动保存 HH:MM」；点击手动触发一次
    statusBar_->setAutosaveEnabled(autoSave_->intervalMinutes() > 0);
    connect(statusBar_, &StatusBar::manualSaveRequested, this, [this]{
        if (!autoSave_ || autoSave_->intervalMinutes() <= 0) {
            QMessageBox::information(this, QStringLiteral("自动保存"),
                QStringLiteral("自动保存当前处于关闭状态。\n可在「文件 → 自动保存设置...」中开启。"));
            return;
        }
        autoSave_->saveNow();   // saved / saveFailed 信号已接，统一回写状态栏
    });

    // 每次保存成功：状态栏刷新时间 + 日志留痕
    connect(autoSave_, &SketchCAD::AutoSave::saved, this, [this](const QString& p){
        statusBar_->setLastAutosave(autoSave_->lastSaved());
        model_->log(QStringLiteral("已自动保存：%1").arg(QFileInfo(p).fileName()));
    });

    // 保存失败：明确对话框 + 日志（禁止静默失败）
    connect(autoSave_, &SketchCAD::AutoSave::saveFailed, this, [this](const QString& reason){
        model_->log(QStringLiteral("自动保存失败：%1").arg(reason), QStringLiteral("error"));
        SketchCAD::Diagnostics::writeLog(QStringLiteral("自动保存失败：%1").arg(reason), "error");
        QMessageBox::warning(this, QStringLiteral("自动保存失败"),
            QStringLiteral("自动保存失败：\n%1\n\n图纸仍在内存中，请尽快手动「文件 → 保存...」。").arg(reason));
    });

    // 磁盘余量提前告警（< 阈值，默认 100MB）
    connect(autoSave_, &SketchCAD::AutoSave::lowDisk, this, [this](qint64 free, int thr){
        const QString msg = QStringLiteral("磁盘剩余空间仅 %1 MB，低于阈值 %2 MB。\n"
                                          "自动保存可能失败，请清理磁盘或更换保存位置。")
                            .arg(free / 1024 / 1024).arg(thr);
        model_->log(msg, QStringLiteral("error"));
        SketchCAD::Diagnostics::writeLog(msg, "warning");
        QMessageBox::warning(this, QStringLiteral("磁盘空间不足"), msg);
    });

    // 模型变化 = 一次「用户操作」→ 累计到阈值触发一次保存
    // （连接在 hookActiveDoc 登记：随活动文档换绑，自动保存只盯活动文档）
    autoSave_->start();

    // 启动页不在此注册 singleShot(0)：构造期注册的 zero-timer 会在 main() 里
    // splash 停留等待的 wait.exec() 嵌套事件循环中抢跑，模态启动页被置顶 splash
    // 遮挡（表现为"卡在启动画面"假死）。改由 main() 在 splash 销毁后经
    // runStartupDialogFlow() 显式触发。
}

void MainWindow::offerCrashRecovery() {
    const QVector<SketchCAD::AutoSave::Snapshot> snaps = SketchCAD::AutoSave::scanSnapshots();
    if (snaps.isEmpty()) return;

    // 先清掉 .tmp 半写残留（无恢复价值），避免它们长期堆积占空间
    SketchCAD::AutoSave::cleanupTempFiles();

    auto* dlg = new CrashRecoveryDialog(snaps, this);
    connect(dlg, &CrashRecoveryDialog::recoverRequested,
            this, &MainWindow::onAutoSaveRecovered);
    connect(dlg, &CrashRecoveryDialog::discardRequested,
            this, &MainWindow::onAutoSaveDiscarded);
    connect(dlg, &CrashRecoveryDialog::discardAllRequested,
            this, &MainWindow::onAutoSaveDiscardAll);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::onAutoSaveRecovered(const SketchCAD::AutoSave::Snapshot& s) {
    const QString origin = s.originPath.isEmpty() ? QString() : s.originPath;

    // 原路径缺失或已被占用（同路径已有图纸）→ 不静默覆盖，先问用户
    QString target = origin;
    if (target.isEmpty() || target == QStringLiteral("(未保存)")) {
        target = QFileDialog::getSaveFileName(this, QStringLiteral("恢复为"),
            QString(), QStringLiteral("草图文件 (*.json)"));
        if (target.isEmpty()) return;   // 用户取消 → 快照保留
    } else if (QFile::exists(target)) {
        const auto a = QMessageBox::question(this, QStringLiteral("原文件已存在"),
            QStringLiteral("原图纸已存在：\n%1\n\n"
                           "「是」= 用快照覆盖它；「否」= 另存为新文件；「取消」= 不恢复。").arg(target),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
        if (a == QMessageBox::Cancel) return;
        if (a == QMessageBox::No) {
            target = QFileDialog::getSaveFileName(this, QStringLiteral("另存为"),
                target, QStringLiteral("草图文件 (*.json)"));
            if (target.isEmpty()) return;
        }
    }

    QString err;
    if (!SketchCAD::AutoSave::restoreSnapshot(s.fullPath, target, &err)) {
        model_->log(QStringLiteral("恢复失败：%1").arg(err), QStringLiteral("error"));
        SketchCAD::Diagnostics::writeLog(QStringLiteral("恢复失败：%1").arg(err), "error");
        QMessageBox::critical(this, QStringLiteral("恢复失败"), err);
        return;
    }

    // 读档：recovering_ 抑制期间的 modelChanged 不计操作，避免刚恢复就被自动保存覆盖
    recovering_ = true;
    readDrawing(target, &err);
    recovering_ = false;
    if (autoSave_) autoSave_->setCurrentDrawing(target);

    // 恢复成功后删掉这份快照（避免下次启动重复提示）
    SketchCAD::AutoSave::discardSnapshot(s.fullPath);

    model_->log(QStringLiteral("已从自动保存恢复：%1").arg(target));
    SketchCAD::Diagnostics::writeLog(QStringLiteral("已从自动保存恢复：%1").arg(target), "info");
    QMessageBox::information(this, QStringLiteral("恢复完成"),
        QStringLiteral("已恢复自动保存的内容：\n%1").arg(target));
}

void MainWindow::onAutoSaveDiscarded(const SketchCAD::AutoSave::Snapshot& s) {
    if (SketchCAD::AutoSave::discardSnapshot(s.fullPath))
        model_->log(QStringLiteral("已丢弃自动保存快照：%1").arg(s.file));
    else
        model_->log(QStringLiteral("丢弃快照失败：%1").arg(s.file), QStringLiteral("error"));
}

void MainWindow::onAutoSaveDiscardAll() {
    const QVector<SketchCAD::AutoSave::Snapshot> snaps = SketchCAD::AutoSave::scanSnapshots();
    int n = 0;
    for (const auto& s : snaps)
        if (SketchCAD::AutoSave::discardSnapshot(s.fullPath)) ++n;
    model_->log(QStringLiteral("已丢弃 %1 份自动保存快照").arg(n));
}

// ---- 崩溃可复现诊断包 ----

QVariantMap MainWindow::drawingDiagnosticsMeta() const {
    // ⚠️ 边界铁律：这里只允许「计数类」字段——图元几何/文本内容绝不进诊断包。
    QVariantMap meta;
    const QString path = model_ ? model_->drawingPath() : QString();
    meta.insert(QStringLiteral("drawingFile"),
                path.isEmpty() ? QStringLiteral("(未命名)") : QFileInfo(path).fileName());
    meta.insert(QStringLiteral("elementCount"), model_ ? model_->elementCount() : 0);
    meta.insert(QStringLiteral("layerCount"),   model_ ? model_->layerCount()   : 0);
    meta.insert(QStringLiteral("blockCount"),   model_ ? model_->blockCount()   : 0);
    return meta;
}

void MainWindow::exportDiagnosticBundle() {
    DiagnosticsDialog dlg(drawingDiagnosticsMeta(), this);
    dlg.exec();
}

void MainWindow::maybeShowPrivacyNotice() {
    // 首启动一次性明示（用户任务要求）。自测不写注册表 → 直接跳过。
    if (!appSettings_ || !s_recentPersistEnabled) return;
    if (appSettings_->firstRunNoticeShown()) return;
    QMessageBox::information(this, QStringLiteral("隐私说明"),
        QStringLiteral("SketchCAD 尊重您的隐私：\n\n"
                       "· 默认不上传任何数据，没有任何后台网络请求；\n"
                       "· 「导出诊断包」只在您主动点击时生成一个本机 ZIP 文件，\n"
                       "  是否发送完全由您决定；\n"
                       "· 匿名遥测默认关闭，可在「选项 → 隐私」中开启或关闭。"));
    appSettings_->setFirstRunNoticeShown(true);
}

void MainWindow::showAbnormalExitNotice() {
    if (!SketchCAD::Diagnostics::lastSessionAbnormal()) return;
    SketchCAD::Diagnostics::acknowledgeAbnormalSession();   // 只提示一次
    model_->log(QStringLiteral("检测到上次异常退出"), QStringLiteral("warning"));
    const auto ret = QMessageBox::question(this, QStringLiteral("上次异常退出"),
        QStringLiteral("检测到上次运行未正常退出（可能发生崩溃或被强制结束）。\n\n"
                       "是否导出诊断包？包内包含日志、崩溃转储与环境信息（不含图纸内容），\n"
                       "只生成本机 ZIP 文件，不会自动发送。\n\n"
                       "「Yes」= 导出诊断包；「No」= 查看日志目录；「Cancel」= 忽略。"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (ret == QMessageBox::Yes) {
        exportDiagnosticBundle();
    } else if (ret == QMessageBox::No) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(SketchCAD::Diagnostics::logDir()));
    }
}

void MainWindow::runStartupDialogFlow() {
    // 启动对话框流：显式顺序执行，消除 zero-timer FIFO 的隐式耦合。
    //   1) 启动页（恢复项优先：P0-02 快照 → 新建/打开最近/打开其他）
    //   2) 首启动隐私明示（一次性）
    //   3) 上次异常退出提示（session.marker 残留）
    // 调用时机（main.cpp 保证）：主窗口已 show()、splash 已销毁——
    // 否则模态框在置顶 splash 之下弹出即成"卡在启动画面"假死。
    showStartupPage();
    maybeShowPrivacyNotice();
    showAbnormalExitNotice();
}

void MainWindow::openAutoSaveOptions() {
    if (!appSettings_ || !autoSave_) return;
    AutoSaveOptionsDialog dlg(autoSave_->intervalMinutes(),
                              autoSave_->operationThreshold(),
                              autoSave_->lowDiskThresholdMb(), this);
    if (dlg.exec() != QDialog::Accepted) return;

    // 落地：配置持久化 + 重建定时器 + 状态栏同步
    appSettings_->setAutosaveIntervalMinutes(dlg.intervalMinutes());
    appSettings_->setLowDiskThresholdMb(dlg.lowDiskMb());
    autoSave_->setIntervalMinutes(dlg.intervalMinutes());
    autoSave_->setOperationThreshold(dlg.opThreshold());
    autoSave_->setLowDiskThresholdMb(dlg.lowDiskMb());
    if (autoSave_->intervalMinutes() > 0) autoSave_->start();
    else autoSave_->stop();
    statusBar_->setAutosaveEnabled(autoSave_->intervalMinutes() > 0);
    statusBar_->setLastAutosave(autoSave_->lastSaved());

    model_->log(QStringLiteral("自动保存间隔：%1")
        .arg(autoSave_->intervalMinutes() > 0
                 ? QStringLiteral("%1 分钟").arg(autoSave_->intervalMinutes())
                 : QStringLiteral("关闭")));
}

// ==================== 应用级配置持久化（AppSettings + 选项对话框） ====================

bool MainWindow::s_recentPersistEnabled = true;   // 自测置 false：离屏测试不写注册表

// 启动载入：把注册表里的「用户习惯」回放到界面。只读取，不写入，不阻塞启动。
void MainWindow::loadAppSettingsOnStartup() {
    if (!appSettings_) return;
    using namespace SettingKeys;

    // 窗口几何与面板布局：恢复成功则跳过首显的居中校正（否则会覆盖用户习惯）
    bool geometryRestored = false;
    const QByteArray geo = appSettings_->windowGeometry();
    if (!geo.isEmpty()) geometryRestored = restoreGeometry(geo);
    const QByteArray dockState = appSettings_->windowState();
    if (!dockState.isEmpty()) restoreState(dockState);
    if (geometryRestored) firstShow_ = false;

    // 显示开关 → 模型（单一状态源）；菜单勾选/状态栏按钮经既有 *Changed 信号自动同步。
    // 逐键走 applySettingLive，与「选项」对话框的预览/回滚共用同一条分派逻辑。
    applySettingLive(QLatin1String(kShowGrid),       appSettings_->value(QLatin1String(kShowGrid)));
    applySettingLive(QLatin1String(kGridSnap),       appSettings_->value(QLatin1String(kGridSnap)));
    applySettingLive(QLatin1String(kGridSnapStep),   appSettings_->value(QLatin1String(kGridSnapStep)));
    applySettingLive(QLatin1String(kOrtho),          appSettings_->value(QLatin1String(kOrtho)));
    applySettingLive(QLatin1String(kDynMode),        appSettings_->value(QLatin1String(kDynMode)));
    applySettingLive(QLatin1String(kShowLineWeight), appSettings_->value(QLatin1String(kShowLineWeight)));
    // 撤销栈深度是"应用习惯"：不入撤销栈，只在启动时把值灌进模型的栈深上限
    applySettingLive(QLatin1String(kUndoDepth), appSettings_->value(QLatin1String(kUndoDepth)));
    // 撤销差量化开关（默认开；关则回落全量 Memento 模式对照）
    applySettingLive(QLatin1String(kUndoDelta), appSettings_->value(QLatin1String(kUndoDelta)));
    // 空闲态右键行为（AutoCAD 约定默认开）：灌进活动模型会话态
    applySettingLive(QLatin1String(kRightClickIdleRepeat),
                     appSettings_->value(QLatin1String(kRightClickIdleRepeat)));

    // 自动保存参数已由 setupAutoSave 从 AppSettings 读入，无需重复。

    // 上次退出时的打印页面设置快照 → 本次「打印设置/预览/导出 PDF」的起始值
    //（AppSettings 校验层已保证该键是合法 JSON 或空串，损坏数据不会到达这里）
    const QString pj = appSettings_->printDefaultsJson();
    if (!pj.isEmpty()) {
        const QJsonDocument pdoc = QJsonDocument::fromJson(pj.toUtf8());
        if (pdoc.isObject()) currentPrintSettings_.fromJson(pdoc.object());
    }

    // 最近文件：RecentFiles 在构造时已从 AppSettings 自载（含旧键迁移），这里只需重建菜单
    rebuildRecentMenu();

    // 之后所有设置变更（对话框应用/取消回滚/导入）都走这一条统一消费点
    connect(appSettings_, &AppSettings::settingChanged,
            this, &MainWindow::applySettingLive);
}

// settingChanged 统一消费点：对话框「应用」即时预览、「取消」回滚、启动回放共用。
// 每个分支把值写入它的真实归宿（模型/自动保存/最近文件），不另存第二份状态。
void MainWindow::applySettingLive(const QString& key, const QVariant& v) {
    if (!model_) return;
    using namespace SettingKeys;
    if (key == QLatin1String(kShowGrid)) {
        model_->setShowGrid(v.toBool());
    } else if (key == QLatin1String(kGridSnap)) {
        model_->setGridSnap(v.toBool());
    } else if (key == QLatin1String(kGridSnapStep)) {
        model_->setGridSnapStep(v.toDouble());
    } else if (key == QLatin1String(kOrtho)) {
        model_->setOrthoMode(v.toBool());
    } else if (key == QLatin1String(kDynMode)) {
        model_->setDynMode(v.toBool());
    } else if (key == QLatin1String(kShowLineWeight)) {
        model_->setShowLineWeight(v.toBool());
    } else if (key == QLatin1String(kAutosaveInterval)) {
        if (!autoSave_) return;
        autoSave_->setIntervalMinutes(v.toInt());
        if (v.toInt() > 0) autoSave_->start(); else autoSave_->stop();
        statusBar_->setAutosaveEnabled(v.toInt() > 0);
    } else if (key == QLatin1String(kLowDiskMb)) {
        if (autoSave_) autoSave_->setLowDiskThresholdMb(v.toInt());
    } else if (key == QLatin1String(kRecentFileCount)) {
        // 上限条数只影响「菜单显示几条」；存储硬上限 10 由 RecentFiles::trim 保证
        rebuildRecentMenu();
    } else if (key == QLatin1String(kUndoDepth)) {
        // 栈深是应用习惯：只改模型的上限（并即时收敛超限的栈），本身不产生撤销步
        model_->setUndoDepthLimit(v.toInt());
    } else if (key == QLatin1String(kUndoDelta)) {
        // 撤销差量化开关：切换会清空现有撤销历史（两种模式栈表示不互换，清空唯一安全）
        model_->setUndoDeltaEnabled(v.toBool());
    } else if (key == QLatin1String(kRecentEntries)) {
        // 外部直接改了底层存储（选项-取消回滚 / 重置 / 导入）→ RecentFiles 重载
        if (recent_) recent_->reloadFromSettings();   // 重载后 changed → rebuildRecentMenu
    } else if (key == QLatin1String(kRightClickIdleRepeat)) {
        // 空闲态右键行为（AutoCAD 约定默认开）→ 写入活动模型会话态（GraphicsView 侧判定）
        if (model_) model_->setRightClickIdleRepeat(v.toBool());
    } else if (key == QLatin1String(kTelemetryEnabled)) {
        // 遥测总开关即时生效（默认 false；与 Telemetry 静态状态同步）
        Telemetry::setEnabled(v.toBool());
    } else if (key == QLatin1String(kLanguage)) {
        // 界面语言切换即刻生效：重装翻译器 + 重建菜单栏（buildMenu 内 tr() 重新求值）。
        // 对话框类（选项/新建图纸等）下次打开自然使用新语言，无需驻留重建。
        SketchCAD::I18n::installTranslator(SketchCAD::I18n::resolveLanguage(v.toString()));
        rebuildMenusForLanguage();
    }
    // 其余键（目录/打印设备/用户信息/语言/GPU 预留）由消费点在各自使用时机读取，
    // 无需即时动作。
}

// 关闭保存：窗口几何/面板布局/显示开关/打印设置 → AppSettings；写失败必须提示。
void MainWindow::saveAppSettingsOnClose() {
    if (!appSettings_) return;
    using namespace SettingKeys;

    appSettings_->setWindowGeometry(saveGeometry());
    appSettings_->setWindowState(saveState());
    // 显示开关从模型（单一状态源）抄回配置
    appSettings_->setShowGrid(model_->showGrid());
    appSettings_->setGridSnap(model_->gridSnap());
    appSettings_->setGridSnapStep(model_->gridSnapStep());
    appSettings_->setOrthoMode(model_->orthoMode());
    appSettings_->setDynMode(model_->dynMode());
    appSettings_->setShowLineWeight(model_->showLineWeight());
    // 最近文件由 RecentFiles 在每次变更时立即持久化（崩溃也不丢），退出时无需重写
    // 打印页面设置快照（下次启动作为「打印设置」对话框的起始值）
    appSettings_->setPrintDefaultsJson(
        QString::fromUtf8(QJsonDocument(currentPrintSettings_.toJson()).toJson(QJsonDocument::Compact)));

    if (!appSettings_->syncAndCheck()) {
        // 写入失败必须提示（禁止静默失败）：明确告知 + 日志留痕
        const QString msg = QStringLiteral("应用配置写入失败：%1\n窗口位置、面板布局等本次改动可能无法在下一次启动时恢复。")
                                .arg(appSettings_->lastError());
        SketchCAD::Diagnostics::writeLog(msg, "error");
        QMessageBox::warning(this, QStringLiteral("配置写入失败"), msg);
    }
}

void MainWindow::openOptionsDialog() {
    if (!appSettings_) return;
    OptionsDialog dlg(appSettings_, this);
    dlg.exec();
    // 应用/取消的写失败提示由对话框负责；此处仅补日志留痕
    if (appSettings_->lastWriteFailed())
        model_->log(QStringLiteral("配置写入失败：%1").arg(appSettings_->lastError()),
                    QStringLiteral("error"));
}

// ---- 最近文件 ----

void MainWindow::pushRecentFile(const QString& path) {
    if (path.isEmpty()) return;
    // MRU 语义（去重置顶/刷新时间/pin 保留/上限 10/立即持久化）收敛在 RecentFiles；
    // changed 信号 → rebuildRecentMenu（菜单重建只有一条通路）
    if (recent_) recent_->add(path);
    else rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    if (!recentMenu_) return;
    recentMenu_->clear();
    const int maxN = appSettings_ ? appSettings_->recentFileCount() : 10;
    if (!recent_ || recent_->count() == 0 || maxN <= 0) {
        QAction* a = recentMenu_->addAction(tr("（空）"));
        a->setEnabled(false);
        return;
    }
    int shown = 0;
    const QVector<SketchCAD::RecentFiles::RecentEntry> entries = recent_->entries();
    for (const auto& e : entries) {
        if (shown >= maxN) break;
        const QString p = e.path;
        const bool missing = !QFile::exists(p);
        const QString text = QStringLiteral("%1%2%3")
                .arg(e.pinned ? QStringLiteral("📌 ") : QString())
                .arg(QFileInfo(p).fileName())
                .arg(missing ? QStringLiteral("（路径不存在）") : QString());
        QAction* a = recentMenu_->addAction(text);
        a->setData(p);                 // 右键菜单按 path 定位条目
        a->setStatusTip(p);
        a->setToolTip(missing ? QStringLiteral("%1\n路径不存在，点击可从列表中移除。").arg(p) : p);
        if (missing) {
            QFont f = a->font();
            f.setItalic(true);
            a->setFont(f);             // 灰显提示：缺失项点击不再尝试打开，而是从列表移除
            connect(a, &QAction::triggered, this, [this, p] {
                if (recent_) recent_->remove(p);   // changed → 菜单重建
                model_->log(QStringLiteral("最近列表：已移除缺失项 %1").arg(p));
            });
        } else {
            connect(a, &QAction::triggered, this, [this, p] { openDocInteractive(p); });
        }
        ++shown;
    }
    recentMenu_->addSeparator();
    recentMenu_->addAction(tr("清除最近文件历史"), this, [this] {
        if (recent_) recent_->clear();   // changed → 菜单重建
        else rebuildRecentMenu();
    });
}

// 文件对话框起始目录：默认存盘目录 > 上次目录 > 系统默认（空串）
QString MainWindow::rememberDialogDir() const {
    if (!appSettings_) return QString();
    const QString d = appSettings_->defaultSaveDir();
    return d.isEmpty() ? appSettings_->lastDir() : d;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // 多文档退出保护：任一文档有未保存修改 → 一次性询问（全部保存 / 放弃 / 取消退出）。
    // 有路径的文档直接写盘（不打断），未命名的走「保存」对话框逐个取路径。
    QList<int> dirtyIdx;
    for (int i = 0; i < docs_->count(); ++i)
        if (docs_->isDirty(i)) dirtyIdx << i;
    if (!dirtyIdx.isEmpty()) {
        const auto ret = QMessageBox::question(this, QStringLiteral("退出"),
            QStringLiteral("有 %1 张图纸存在未保存的修改。是否全部保存？")
                .arg(dirtyIdx.size()),
            QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::SaveAll);
        if (ret == QMessageBox::Cancel) { e->ignore(); return; }
        if (ret == QMessageBox::SaveAll) {
            for (int i : dirtyIdx) {
                switchToDoc(i);
                DocSession* s = docs_->doc(i);
                if (!s) continue;
                if (s->filePath.isEmpty()) {
                    saveFile();                       // 未命名 → 对话框取路径（成功后内部清脏）
                } else if (writeDrawing(s->filePath, nullptr)) {
                    docs_->markClean(i);              // 已有路径 → 静默写盘后清脏
                    syncTabTitle(i);
                }
            }
        }
    }
    // 记住用户习惯：几何/布局/显示开关/最近文件/打印设置（写失败会弹窗提示）
    saveAppSettingsOnClose();
    // 正常退出：停定时器 + 清理 .tmp 半写残留（正式 .sv$ 保留，供下次恢复）
    if (autoSave_) autoSave_->stop();
    SketchCAD::AutoSave::cleanupTempFiles();
    e->accept();
}

// 把窗口几何校正到"所在屏幕可用区域"内：外框(含标题栏)完全落进可用区域并整体居中，
// 保证标题栏/菜单栏不被推到屏幕外。注意：setGeometry/move 定位的是客户区左上角，
// Windows 标题栏会使外框再向上偏移 titleH(=外框高-客户区高)px，故需做换算。
void MainWindow::ensureVisibleOnScreen() {
    QScreen* screen = window()->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect avail = screen->availableGeometry();   // 排除任务栏/系统栏
    if (avail.isEmpty()) return;

    const int titleH = qMax(0, frameGeometry().height() - height());  // 顶部标题栏/边框高

    // 目标客户区尺寸：宽不超屏、高不超过可用高(扣除标题栏)
    int w = qMin(width(), avail.width());
    int h = qMin(height(), avail.height() - titleH);
    h = qMax(h, minimumHeight());

    // 外框(含标题栏)在可用区域内居中并夹紧
    const int frameW = w;
    const int frameH = h + titleH;
    const int frameX = qMin(qMax(avail.left() + (avail.width()  - frameW) / 2, avail.left()),
                            avail.right() + 1 - frameW);
    const int frameY = qMin(qMax(avail.top()   + (avail.height() - frameH) / 2, avail.top()),
                            avail.bottom() + 1 - frameH);

    resize(w, h);
    move(frameX, frameY + titleH);   // 客户区顶 = 外框顶 + 标题栏高

    // 兜底：个别平台把标题栏计进尺寸导致仍越界时，把客户区顶压回可用区顶部
    if (frameGeometry().top() < avail.top())
        move(frameX, avail.top() + titleH);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    // 仅首次显示时校正一次，之后不再干扰用户的手动缩放/移动/最大化。
    // 延迟到事件循环稳定后再执行：确保 window()->screen() 已返回真实屏幕，
    // 且窗口管理器完成初始定位后几何不被 WM 覆盖。
    if (firstShow_) {
        firstShow_ = false;
        QTimer::singleShot(0, this, &MainWindow::ensureVisibleOnScreen);
    }
}

// 预设色板：工具条下拉与菜单子项共用同一份定义
const QVector<QPair<QString, QColor>>& MainWindow::presetColors() {
    static const QVector<QPair<QString, QColor>> kList = {
        {QStringLiteral("墨黑（默认）"), QColor(0x1A, 0x1D, 0x28)},
        {QStringLiteral("正红"),        QColor(0xD1, 0x4B, 0x3F)},
        {QStringLiteral("工程蓝"),      QColor(0x2B, 0x59, 0xA8)},
        {QStringLiteral("松绿"),        QColor(0x2E, 0x8B, 0x57)},
        {QStringLiteral("琥珀橙"),      QColor(0xE6, 0x91, 0x38)},
        {QStringLiteral("紫罗兰"),      QColor(0x7E, 0x57, 0xC2)},
        {QStringLiteral("青碧"),        QColor(0x00, 0x96, 0xA0)},
        {QStringLiteral("中性灰"),      QColor(0x80, 0x86, 0x90)},
    };
    return kList;
}

// "对齐方式"字符串 → modify::makeAlign 的 kind 整数：
//   left / centerx / right / topy / centery / bottom → 0/1/2/3/4/5
int MainWindow::kindIndexFromName(const QString& k) {
    const QString s = k.toLower();
    if (s == "left"   || s == "l")  return 0;
    if (s == "centerx"|| s == "cx") return 1;
    if (s == "right"  || s == "r")  return 2;
    if (s == "topy"   || s == "t")  return 3;
    if (s == "centery"|| s == "cy") return 4;
    if (s == "bottom" || s == "b")  return 5;
    return 0;
}

// 语言切换即刻生效：菜单栏整体重建。
// QMenu 由 new QMenu(title, this) 挂在 MainWindow 上、经 bar->addMenu 呈现；
// 先收集顶级菜单（action->menu()），clear 菜单栏后逐个 delete（此刻无菜单弹出中，
// 同步删除安全），再重跑 buildMenu —— 其中所有 tr() 按新翻译器重新求值。
// 成员菜单（recentMenu_ 等）由 buildMenu 重新赋值；rebuildRecentMenu/插件面板
// 等外部消费者都有空守卫，重建窗口期内安全。
void MainWindow::rebuildMenusForLanguage() {
    const QList<QAction*> tops = menuBar()->actions();
    QList<QMenu*> olds;
    for (QAction* a : tops)
        if (QMenu* m = a->menu()) olds << m;
    menuBar()->clear();
    for (QMenu* m : olds) delete m;
    buildMenu();
}

// ---- 命令面板（Ctrl+Shift+P）：薄壳，过滤/排序/最近使用逻辑在 CommandPaletteModel ----
void MainWindow::openCommandPalette() {
    CommandPaletteDialog dlg(interp_, s_recentPersistEnabled ? appSettings_ : nullptr, this);
    dlg.exec();
}

// ---- 快捷键冲突自检：菜单动作（递归收集）+ 全局 QShortcut，重复绑定 warn 提示 ----
void MainWindow::auditShortcutConflicts() {
    QVector<ShortcutAudit::Binding> bindings;

    // 1) 菜单栏全部 QAction（含子菜单，递归）
    std::function<void(QMenu*)> walk = [&](QMenu* menu) {
        for (QAction* a : menu->actions()) {
            if (QMenu* sub = a->menu()) { walk(sub); continue; }
            if (!a->shortcut().isEmpty())
                bindings.append({ a->text().remove(QChar('&')), a->shortcut().toString() });
        }
    };
    for (QAction* top : menuBar()->actions())
        if (QMenu* m = top->menu()) walk(m);

    // 2) 独立 QShortcut（F8/F9 正交捕捉、Ctrl+Return 聚焦命令行、Ctrl+Tab 文档切换、
    //    Ctrl+Shift+P 命令面板等——不挂在菜单上的全局键）
    for (QShortcut* sc : findChildren<QShortcut*>())
        if (!sc->key().isEmpty())
            bindings.append({ QStringLiteral("（全局快捷键 %1）").arg(sc->key().toString()),
                              sc->key().toString() });

    // 3) 纯函数查重（可无头直测）→ warn 日志
    const auto dups = ShortcutAudit::duplicates(bindings);
    for (const auto& d : dups)
        model_->log(QStringLiteral("快捷键冲突：%1 同时绑定于 %2（后者将不生效或抢占，请检查菜单定义）")
                        .arg(d.first, d.second),
                    QStringLiteral("warn"));
}

void MainWindow::buildMenu() {
    QMenuBar* bar = menuBar();

    // 统一带快捷键的动作构造（键盘通路与鼠标菜单一一对应）
    auto mk = [this](QMenu* menu, const QString& text, const QKeySequence& key, auto fn) {
        QAction* a = menu->addAction(text);
        if (!key.isEmpty()) a->setShortcut(key);
        a->setShortcutContext(Qt::ApplicationShortcut);
        connect(a, &QAction::triggered, this, fn);
        return a;
    };

    // 命令补齐：菜单/工具条与命令行共用同一 CommandInterpreter 处理器（菜单项直接走 interp_->execute，与命令行同一 handler）。
    // 文字类命令需参数，菜单点击时以对话框收集后交给解释器（同一 handler）。
    auto dlgText = [this](bool multi) {
        bool ok = false;
        QString pos = QInputDialog::getText(this, multi ? QStringLiteral("多行文字") : QStringLiteral("单行文字"),
                                            QStringLiteral("插入点 x,y："), QLineEdit::Normal, QStringLiteral("0,0"), &ok);
        if (!ok || pos.isEmpty()) return;
        bool ok2 = false;
        double h = QInputDialog::getDouble(this, QStringLiteral("字高"), QStringLiteral("字高(mm)："), 3.5, 0.1, 1000.0, 2, &ok2);
        if (!ok2) return;
        bool ok3 = false;
        QString content = QInputDialog::getText(this, QStringLiteral("内容"), QStringLiteral("文字内容："), QLineEdit::Normal, QString(), &ok3);
        if (!ok3) return;
        interp_->execute(QStringLiteral("%1 %2 %3 %4")
                             .arg(multi ? QStringLiteral("mtext") : QStringLiteral("text")).arg(pos).arg(h, 0, 'f', 2).arg(content));
    };
    auto dlgDdedit = [this]() {
        bool ok = false;
        QString content = QInputDialog::getText(this, QStringLiteral("编辑文字"), QStringLiteral("新内容："), QLineEdit::Normal, QString(), &ok);
        if (ok && !content.isEmpty()) interp_->execute(QStringLiteral("ddedit ") + content);
    };

    QMenu* f = new QMenu(tr("文件(&F)"), this);
    f->setObjectName("menuFile");
    mk(f, tr("新建"),        QKeySequence::New,  [this] {
            NewDrawingDialog dlg(this);
            if (dlg.exec() != QDialog::Accepted) return;
            // 模板化新建唯一入口：多文档新标签 + applyNewDrawing（既有流）+ 标题栏样式
            createTemplatedDrawing(dlg.selectedSpec(), dlg.selectedScaleRatio(),
                                   dlg.titleBlockStyle());
        })
        ->setIcon(icons::make("new"));
    mk(f, tr("填写标题栏..."), QKeySequence(), [this] {
            TitleBlockForm dlg(model_, this);
            dlg.exec();
        });
    mk(f, tr("插入标准标题栏..."), QKeySequence(), [this] {
            insertStandardTitleBlock();
        });
    mk(f, tr("按幅面生成图框与标题栏..."), QKeySequence(), [this] {
            generateFramedTitleBlock();
        });
    // 绘图比例属于「图纸属性」而非视图属性，故放在文件菜单（GB/T 14690 技术制图 比例）
    mk(f, tr("绘图比例..."), QKeySequence("Ctrl+Shift+S"), [this] {
            changeDrawingScale();
        });
    // 单位与图纸界限同为「图纸属性」：显示/输入口径 + LIMITS 范围，随图存档
    mk(f, tr("单位与图纸界限..."), QKeySequence(), [this] {
            openUnitsLimits();
        });
    // 捕捉设置（OSNAP）：14 种捕捉模式的独立启用/禁用，随图存档
    mk(f, tr("捕捉设置..."), QKeySequence(), [this] {
            openSnapSettings();
        });
    // 自动保存设置：间隔/操作次数/磁盘告警阈值（崩溃不丢稿的用户入口）
    mk(f, tr("自动保存设置..."), QKeySequence(), [this] {
            openAutoSaveOptions();
        });
    mk(f, tr("打开..."),      QKeySequence::Open, [this] { openFile(); })
        ->setIcon(icons::make("open"));
    mk(f, tr("保存..."),      QKeySequence::Save, [this] { saveFile(); })
        ->setIcon(icons::make("save"));
    // 最近打开：动态子菜单（条目数 = 选项里的「最近文件数」，列表随成功读档更新；
    // 缺失文件灰显提示、点击移除单条；右键条目 → 固定/取消固定、从列表删除）
    recentMenu_ = new QMenu(tr("最近打开(&R)"), f);
    recentMenu_->setObjectName("menuRecent");
    recentMenu_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(recentMenu_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QAction* a = recentMenu_->actionAt(pos);
        const QString p = a ? a->data().toString() : QString();
        if (!a || p.isEmpty()) return;                    // 分隔线/空态/清除历史 无右键菜单
        QMenu ctx(recentMenu_);
        QAction* pinAct = ctx.addAction(recent_->isPinned(p)
                                            ? QStringLiteral("取消固定")
                                            : QStringLiteral("固定到列表"));
        QAction* delAct = ctx.addAction(QStringLiteral("从列表中删除"));
        QAction* chosen = ctx.exec(recentMenu_->mapToGlobal(pos));
        if (chosen == pinAct) recent_->setPinned(p, !recent_->isPinned(p));
        else if (chosen == delAct) recent_->remove(p);
    });
    f->addMenu(recentMenu_);
    rebuildRecentMenu();
    f->addSeparator();
    mk(f, tr("导出 DXF..."),  QKeySequence("Ctrl+E"), [this] { exportDxf(); })
        ->setIcon(icons::make("export"));
    f->addSeparator();
    mk(f, tr("退出"),        QKeySequence::Quit, [this] { close(); });
    bar->addMenu(f);

    // ---- 顶级菜单：打印（页面设置 + 简化 CTB 样式 + 视口/布局 MVP + 预览/PDF/物理打印）----
    QMenu* pr = new QMenu(tr("打印(&P)"), this);
    pr->setObjectName("menuPrint");
    mk(pr, tr("打印设置..."), QKeySequence(), [this] { openPrintSettings(); });
    mk(pr, tr("打印预览..."), QKeySequence(), [this] { openPrintPreview(); });
    mk(pr, tr("导出 PDF..."), QKeySequence(), [this] { exportPdfDirect(); });
    mk(pr, tr("导出 PDF（全部布局）..."), QKeySequence(), [this] { exportPdfLayoutsDirect(); });
    pr->addSeparator();
    mk(pr, tr("发布... (PUBLISH)"), QKeySequence(), [this] { openPublish(); });
    pr->addSeparator();
    mk(pr, QStringLiteral("直接打印 (模型空间)"), QKeySequence(), [this] { printDirect(); });
    bar->addMenu(pr);

    // ---- 顶级菜单：追踪（极轴 POLAR / 对象捕捉追踪 OTRACK / 临时追踪点 TK）----
    // 快速开关 + 追踪设置入口；勾选态与 trackConfigChanged 双向同步（命令行/面板改动即时反映）。
    QMenu* k = new QMenu(tr("追踪(&K)"), this);
    k->setObjectName("menuTrack");
    QAction* aPolar = mk(k, QStringLiteral("极轴追踪 (POLAR)"), QKeySequence("F10"), [this] {
            snap::TrackConfig c = model_->trackConfig();
            c.polar = !c.polar;
            model_->setTrackConfig(c);
            model_->log(QStringLiteral("极轴追踪：%1").arg(c.polar ? QStringLiteral("开") : QStringLiteral("关")));
        });
    aPolar->setCheckable(true);
    aPolar->setChecked(model_->trackConfig().polar);
    aPolar_ = aPolar;   // 勾选态连接随活动文档换（hookActiveDoc 登记）
    QAction* aOtrack = mk(k, QStringLiteral("对象捕捉追踪 (OTRACK)"), QKeySequence("F11"), [this] {
            snap::TrackConfig c = model_->trackConfig();
            c.objectTrack = !c.objectTrack;
            model_->setTrackConfig(c);
            model_->log(QStringLiteral("对象捕捉追踪：%1").arg(c.objectTrack ? QStringLiteral("开") : QStringLiteral("关")));
        });
    aOtrack->setCheckable(true);
    aOtrack->setChecked(model_->trackConfig().objectTrack);
    aOtrack_ = aOtrack;
    k->addSeparator();
    mk(k, QStringLiteral("清除临时追踪点"), QKeySequence(), [this] {
            model_->clearTrackPoints();
            model_->log(QStringLiteral("已清除全部临时追踪点 (TK)"));
        });
    mk(k, QStringLiteral("追踪设置..."), QKeySequence(), [this] {
            openSnapSettings();   // 追踪配置并入捕捉设置面板（同一数据源，随图存档）
        });
    // 勾选态随配置变化实时刷新（连接在 hookActiveDoc 登记，随活动文档换绑）
    bar->addMenu(k);

    // 顶级菜单整合后，【修改】【选择】不再各占一个顶级入口，而是作为【编辑】的子菜单
    // （下方构建完成后用 e->addMenu(...) 挂进来）。显示顺序 = 添加顺序：
    //   撤销 / 重做 | 复制 / 粘贴 / 删除 | 修改 | 选择
    QMenu* e = new QMenu(tr("编辑(&E)"), this);
    e->setObjectName("menuEdit");
    mk(e, QStringLiteral("撤销"), QKeySequence::Undo,  [this] { model_->undo(); })
        ->setIcon(icons::make("undo"));
    mk(e, QStringLiteral("重做"), QKeySequence::Redo,  [this] { model_->redo(); })
        ->setIcon(icons::make("redo"));
    e->addSeparator();
    mk(e, QStringLiteral("复制"), QKeySequence::Copy,  [this] { model_->copySelected(); })
        ->setIcon(icons::make("copy"));
    mk(e, QStringLiteral("粘贴"), QKeySequence::Paste, [this] { model_->paste(); })
        ->setIcon(icons::make("paste"));
    mk(e, QStringLiteral("删除"), QKeySequence::Delete,[this] { model_->deleteSelected(); })
        ->setIcon(icons::make("delete"));
    e->addSeparator();
    // ---- 剪贴板族：copyclip / copybase / pasteclip / pasteblock ----
    // 与上方「复制/粘贴」（模型内部剪贴板）互补：走系统剪贴板上的版本化 JSON
    // （application/x-sketchcad-clip），支持跨 SketchCAD 实例搬运图元，
    // 粘贴时依赖的块定义一并合并进目标图（详见 core/Clipboard.h）。
    // 快捷键避开既有 Ctrl+C / Ctrl+V（模型内部剪贴板），用 Alt/Shift 组合。
    QMenu* clipMenu = e->addMenu(icons::make("copy"), tr("剪贴板(&L)"));
    mk(clipMenu, QStringLiteral("复制(&C)"), QKeySequence("Ctrl+Alt+C"), [this] { copyClipToClipboard(false); })
        ->setIcon(icons::make("copy"));
    mk(clipMenu, QStringLiteral("带基点复制(&B)..."), QKeySequence("Ctrl+Shift+C"), [this] { copyClipToClipboard(true); })
        ->setIcon(icons::make("copy"));
    mk(clipMenu, QStringLiteral("粘贴(&P)"), QKeySequence("Ctrl+Alt+V"), [this] { pasteFromClipboard(false); })
        ->setIcon(icons::make("paste"));
    mk(clipMenu, QStringLiteral("粘贴为块(&K)..."), QKeySequence("Ctrl+Shift+V"), [this] { pasteFromClipboard(true); })
        ->setIcon(icons::make("paste"));
    e->addSeparator();
    // ---- 选择工具：编组 / 快速选择 / 过滤器（QSELECT / FILTER）----
    // 三者共用 SketchModel 选择集状态，与点击选择/框选/图层选择完全一致。
    QMenu* selMenu = e->addMenu(icons::make("qselect"), tr("选择工具(&S)"));
    mk(selMenu, QStringLiteral("编组管理(&G)..."), QKeySequence(), [this] { openGroupManager(); })
        ->setIcon(icons::make("group"));
    mk(selMenu, QStringLiteral("快速选择(&Q)..."), QKeySequence(), [this] { openQSelect(); })
        ->setIcon(icons::make("qselect"));
    mk(selMenu, QStringLiteral("过滤器(&F)..."), QKeySequence(), [this] { openFilterDialog(); })
        ->setIcon(icons::make("filter"));
    e->addSeparator();
    bar->addMenu(e);

    // ---- 新增顶级菜单：修改（Modify）—— 与参考附图（LibreCAD Modify 菜单）逐项对应 ----
    // 23 个图元修改 / 几何编辑 / 工具动作，覆盖：
    //   变换类（Move/Copy, Duplicate, Align, Rotate, Scale, Mirror, Move-Rotate,
    //           Rotate Two, Revert Direction）
    //   修剪类（Trim, Trim Two, Lengthen, Line Join, Break/Divide, Line Gap）
    //   几何构造（Offset, Bevel, Fillet, Divide, Stretch）
    //   工具与分解（Properties, Attributes, Explode Text into Letters, Explode, Delete）
    // 已存在的工具型菜单项（trim/extend）复用 toolActions_ 指针，其余走 mk() 即时构造。
    QMenu* mod = new QMenu(tr("修改(&M)"), this);
    mod->setObjectName("menuModify");

    // ---- 变换类 ----
    mk(mod, QStringLiteral("移动 / 复制..."), QKeySequence("Ctrl+Shift+M"), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Move, QStringLiteral("移动 / 复制"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        if (model_->selection().empty()) { model_->log(QStringLiteral("无可移动图元（请先选中）"), "warn"); return; }
        const int n = model_->selection().size();
        model_->translateSelection(Vec3(r.dx, r.dy), QStringLiteral("修改-移动"));
        model_->log(QStringLiteral("已移动 %1 个图元（Δ=(%2,%3)）")
                        .arg(n).arg(r.dx, 0, 'f', 2).arg(r.dy, 0, 'f', 2));
    })->setIcon(icons::make("move_copy"));
    mk(mod, QStringLiteral("复制"), QKeySequence("Ctrl+Shift+D"), [this] {
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可复制图元"), "warn"); return; }
        auto changes = modify::makeDuplicate(src, 10.0, 0.0);
        model_->beginEdit(QStringLiteral("修改-复制"));
        auto added = modify::applyChanges(model_, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        model_->log(QStringLiteral("已复制 %1 个图元 → %2").arg(src.size()).arg(ids.join(", ")));
    })->setIcon(icons::make("duplicate"));

    // ---- 阵列复制（需求：基于选中图元的矩形 / 环形阵列）----
    auto* arrayMenu = mod->addMenu(icons::make("duplicate"), tr("阵列(&R)"));
    mk(arrayMenu, QStringLiteral("矩形阵列..."), QKeySequence(), [this] { arrayRectangular(); })
        ->setIcon(icons::make("duplicate"));
    mk(arrayMenu, QStringLiteral("环形阵列..."), QKeySequence(), [this] { arrayPolar(); })
        ->setIcon(icons::make("rotate"));
    mk(arrayMenu, QStringLiteral("路径阵列..."), QKeySequence(), [this] { arrayPath(); })
        ->setIcon(icons::make("duplicate"));

    // ---- 连合（JOIN）：同层 Line/Arc 按端点容差连合为 Polyline ----
    // 容差取视图真实 snap 容差（8px/zoom），与画布拾取手感一致；
    // 命令行入口 join [tol] 缺省 0.5mm。两者共用 modify::makeJoin。
    mk(mod, QStringLiteral("连合(&J)"), QKeySequence(), [this] { joinSelected(); })
        ->setIcon(icons::make("line_join"));

    // ---- 剖面填充（GB/T 4457.5-2013 技术制图 剖面区域的表示法）----
    // 把选中的闭合图元（圆/椭圆/矩形/多边形/闭合折线）转成剖面区域并套用标准剖面线规则。
    mk(mod, QStringLiteral("剖面填充(&H)..."), QKeySequence("Ctrl+Shift+H"), [this] { sectionFill(); })
        ->setIcon(icons::make("hatch"));

    // ---- 剖面填充参数（对已填充的区域重新选材料/换图案）----
    // 不需重新框选区域：直接对当前选中的 Hatch 图元就地重设材料类别 / 角度 / 间距，
    // 让用户随时把「通用金属」改成「砖墙」「石材」「木材」等工程图案。
    mk(mod, QStringLiteral("剖面填充参数(&P)..."), QKeySequence(), [this] { sectionFillParams(); })
        ->setIcon(icons::make("hatch"));

    // ---- 用选中边界填充（BOUNDARY/BPOLY 直接复用，跳过自动追踪）----
    // 与「点选区域填充」互补：先选中若干边界图元，一次性把它们生成剖面区域，
    // 复杂腔体无需人工逐段描边、也无需依赖剖面引擎的自动边界追踪。
    mk(mod, QStringLiteral("用选中边界填充(&B)..."), QKeySequence(), [this] { sectionFillSelected(); })
        ->setIcon(icons::make("boundary"));

    // ---- 边界区域管理（REGION：命名分组 + 批量填充/选择/重命名/删除）----
    mk(mod, QStringLiteral("边界区域管理(&R)..."), QKeySequence(), [this] { openRegionManager(); })
        ->setIcon(icons::make("boundary"));

    // ---- 图形清理（PURGE + OVERKILL：清垃圾/冗余、重合删除、共线重叠合并）----
    mk(mod, QStringLiteral("图形清理(&C)..."), QKeySequence(), [this] { openCleanup(); })
        ->setIcon(icons::make("cleanup"));

    auto* alignMenu = mod->addMenu(icons::make("align"), tr("对齐(&A)"));
    mk(alignMenu, QStringLiteral("对齐..."), QKeySequence(), [this] {
        std::vector<Element> src;
        const Element* refE = nullptr;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) {
            if (!refE) refE = e; else src.push_back(*e);
        }
        if (!refE || src.empty()) { model_->log(QStringLiteral("至少 2 个选中（1 参考 + 1+ 待对齐）"), "warn"); return; }
        ModifyInputDialog dlg(ModifyInputDialog::M_Align, QStringLiteral("对齐"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto changes = modify::makeAlign(src, *refE, kindIndexFromName(dlg.result().kind));
        model_->beginEdit(QStringLiteral("修改-对齐"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已对齐 %1 个图元（方式=%2）").arg(src.size()).arg(dlg.result().kind));
    })->setIcon(icons::make("align"));
    mk(alignMenu, QStringLiteral("对齐单条..."), QKeySequence(), [this] {
        model_->log(QStringLiteral("对齐单条：请用「对齐...」并指定参考元素"), "info");
    })->setIcon(icons::make("align_one"));
    mk(alignMenu, QStringLiteral("对齐参考点..."), QKeySequence(), [this] {
        model_->log(QStringLiteral("对齐参考点：请用「对齐...」并指定参考元素"), "info");
    })->setIcon(icons::make("align_ref"));

    mk(mod, QStringLiteral("旋转..."), QKeySequence("Ctrl+R"), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Rotate, QStringLiteral("旋转"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可旋转图元"), "warn"); return; }
        auto changes = modify::makeRotate(src, Vec3(r.cx, r.cy), r.angleDeg);
        model_->beginEdit(QStringLiteral("修改-旋转"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已旋转 %1 个图元 %2°").arg(src.size()).arg(r.angleDeg, 0, 'f', 2));
    })->setIcon(icons::make("rotate"));
    mk(mod, QStringLiteral("缩放..."), QKeySequence("Ctrl+K"), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Scale, QStringLiteral("缩放"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        if (r.factor <= 0) { model_->log(QStringLiteral("缩放因子必须 > 0"), "warn"); return; }
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可缩放图元"), "warn"); return; }
        auto changes = modify::makeScale(src, Vec3(r.cx, r.cy), r.factor);
        model_->beginEdit(QStringLiteral("修改-缩放"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已缩放 %1 个图元（因子=%2）").arg(src.size()).arg(r.factor, 0, 'f', 3));
    })->setIcon(icons::make("scale"));
    mk(mod, QStringLiteral("镜像..."), QKeySequence(), [this] { mirror(); })->setIcon(icons::make("mirror"));
    mk(mod, QStringLiteral("移动并旋转..."), QKeySequence(), [this] {
        // 复合：先移动，再旋转（链式应用两次 modify）
        ModifyInputDialog dlg(ModifyInputDialog::M_Rotate, QStringLiteral("移动并旋转（先平移到原点，再绕原点旋转）"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可变换图元"), "warn"); return; }
        // 1) 平移 → (cx, cy)
        auto cs = modify::makeMoveCopy(src, Vec3(r.cx, r.cy), false);
        // 2) 旋转 r.angleDeg 绕原点
        auto cr = modify::makeRotate(src, Vec3(r.cx, r.cy), r.angleDeg);
        // 合并：复用 cr 的几何 + 应用 cs 的平移
        std::vector<modify::Change> combined;
        for (auto& a : cs) combined.push_back(a);
        for (auto& a : cr) {
            // 在 cr 的基础上再加一次 (cx,cy) 平移
            for (auto& p : a.replacement.pts) p = p + Vec3(r.cx, r.cy);
            combined.push_back(a);
        }
        model_->beginEdit(QStringLiteral("修改-移动并旋转"));
        modify::applyChanges(model_, combined, QString());
        model_->log(QStringLiteral("已对 %1 个图元执行移动并旋转").arg(src.size()));
    })->setIcon(icons::make("move_rotate"));
    mk(mod, QStringLiteral("旋转两条图元..."), QKeySequence(), [this] {
        // 复用 rotate：取前 2 个选中
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) {
            src.push_back(*e); if ((int)src.size() >= 2) break;
        }
        if (src.size() < 2) { model_->log(QStringLiteral("需要至少 2 个选中图元"), "warn"); return; }
        ModifyInputDialog dlg(ModifyInputDialog::M_Rotate, QStringLiteral("旋转两条图元"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        auto changes = modify::makeRotate(src, Vec3(r.cx, r.cy), r.angleDeg);
        model_->beginEdit(QStringLiteral("修改-旋转两图元"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已旋转 2 个图元 %1°").arg(r.angleDeg, 0, 'f', 2));
    })->setIcon(icons::make("rotate_two"));
    mk(mod, QStringLiteral("反向"), QKeySequence("Ctrl+Shift+R"), [this] {
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可反向图元"), "warn"); return; }
        auto changes = modify::makeRevertDirection(src);
        model_->beginEdit(QStringLiteral("修改-反向"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已反向 %1 个图元").arg(src.size()));
    })->setIcon(icons::make("revert_direction"));
    mod->addSeparator();

    // ---- 修剪类 ----
    if (QAction* aTrim = toolActions_.value("trim"))  mod->addAction(aTrim);
    mk(mod, QStringLiteral("修剪二..."), QKeySequence(), [this] {
        // 复用 TrimCommand 的语义但强制两个图元
        if (model_->selection().size() < 2) {
            model_->log(QStringLiteral("修剪二需要至少 2 个选中图元（点击+边界）"), "warn");
            return;
        }
        // 直接调用 CLI
        interp_->execute("trim");
        model_->log(QStringLiteral("已启动修剪（Trim Two）"), "info");
    })->setIcon(icons::make("trim_two"));
    mk(mod, QStringLiteral("延长..."), QKeySequence(), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Lengthen, QStringLiteral("延长 / 缩短"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可延长图元"), "warn"); return; }
        auto changes = modify::makeLengthen(src, r.side, r.deltaMm);
        model_->beginEdit(QStringLiteral("修改-延长"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已延长 %1 个图元（side=%2, Δ=%3 mm）")
                        .arg(src.size()).arg(r.side).arg(r.deltaMm, 0, 'f', 2));
    })->setIcon(icons::make("lengthen"));
    mk(mod, QStringLiteral("连接线段..."), QKeySequence(), [this] {
        // 选两条直线，连接成一条（保留第一段几何不变，延伸/裁短第二段接到第一段端点）
        if (model_->selection().size() < 2) {
            model_->log(QStringLiteral("连接线段需要至少 2 个选中图元"), "warn");
            return;
        }
        // 直接用 BendLineJoin 算法：取两条线的端点，按距离最小配对，延伸一段
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.size() < 2 || src[0].pts.size() < 2 || src[1].pts.size() < 2) {
            model_->log(QStringLiteral("选中的图元不是直线"), "warn"); return;
        }
        // 简化：把第二条直线删除并把第一条延长到第二条最远端
        Vec3 a = src[0].pts[0], b = src[0].pts[1];
        Vec3 c = src[1].pts[0], d = src[1].pts[1];
        // 配对：把 a 接到最远的端点
        double da = (c.distTo(a) < d.distTo(a)) ? c.distTo(a) : d.distTo(a);
        double db = (c.distTo(b) < d.distTo(b)) ? c.distTo(b) : d.distTo(b);
        Vec3 farEnd;
        // 把 src[0] 的 a 端或 b 端延长到 src[1] 的远端
        if (da < db) farEnd = (c.distTo(a) < d.distTo(a)) ? c : d;
        else         farEnd = (c.distTo(b) < d.distTo(b)) ? c : d;
        // 用 lengthen 把 a 或 b 端延伸到 farEnd
        int side = (da < db) ? 0 : 1;
        auto changes = modify::makeLengthen({src[0]}, side, farEnd.distTo(da < db ? a : b));
        // 加删除 src[1]
        changes.push_back({src[1].id, Element(), true, QStringLiteral("删除第二条")});
        model_->beginEdit(QStringLiteral("修改-连接"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已连接两线段"));
    })->setIcon(icons::make("line_join"));
    mk(mod, QStringLiteral("打断 / 等分..."), QKeySequence(), [this] {
        if (model_->selection().size() != 1) {
            model_->log(QStringLiteral("打断 / 等分需要先选中 1 条直线"), "warn"); return;
        }
        const Element* e = model_->element(model_->selection()[0]);
        if (!e || e->type != ElementType::Line) {
            model_->log(QStringLiteral("请选中一条直线"), "warn"); return;
        }
        ModifyInputDialog dlg(ModifyInputDialog::M_Divide, QStringLiteral("打断 / 等分（直线）"), e->id, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        int n = dlg.result().n;
        std::vector<Element> src = { *e };
        auto changes = modify::makeBreakDivide(src, e->id, Vec3(0,0,0), n);
        if (changes.empty()) { model_->log(QStringLiteral("等分失败"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-等分"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已把图元 #%1 等分为 %2 段").arg(e->id).arg(n));
    })->setIcon(icons::make("break_divide"));
    mk(mod, QStringLiteral("线间隙..."), QKeySequence(), [this] {
        // 线间隙 (Line Gap)：在选中直线的中点处切开一个指定宽度的缺口，
        // 变成两段互不相连的直线（起止点各自后退 gap/2）。
        if (model_->selection().size() != 1) {
            model_->log(QStringLiteral("线间隙需要 1 条直线"), "warn"); return;
        }
        const Element* e = model_->element(model_->selection()[0]);
        if (!e || e->type != ElementType::Line || e->pts.size() < 2) {
            model_->log(QStringLiteral("请选中一条直线"), "warn"); return;
        }
        bool ok = false;
        const double gap = QInputDialog::getDouble(
            this, QStringLiteral("线间隙"),
            QStringLiteral("缺口宽度 (mm)："), 2.0, 0.01, 1.0e6, 2, &ok);
        if (!ok) return;

        const Vec3 a = e->pts[0], b = e->pts[1];
        const Vec3 ab = b - a;
        const double L = ab.len2D();
        if (L < 1e-9) { model_->log(QStringLiteral("直线长度为 0"), "warn"); return; }
        if (gap >= L) { model_->log(QStringLiteral("缺口宽度不能大于线长"), "warn"); return; }
        const Vec3 u(ab.x / L, ab.y / L, 0);
        const double t0 = (L - gap) / 2.0;      // 缺口起点参数
        const double t1 = t0 + gap;             // 缺口终点参数
        const Vec3 p0(a.x + u.x * t0, a.y + u.y * t0, 0);
        const Vec3 p1(a.x + u.x * t1, a.y + u.y * t1, 0);

        // 先在 p0 处打断（保留 (a,p0)，新增 (p0,b)），再把新增段的起点推到 p1，形成缺口
        auto changes = modify::makeBreakDivide({*e}, e->id, p0, 0);
        if (changes.empty()) { model_->log(QStringLiteral("线间隙打断失败"), "warn"); return; }
        for (auto& c : changes)
            if (!c.remove && c.id < 0 && c.replacement.pts.size() >= 2) c.replacement.pts[0] = p1;

        model_->beginEdit(QStringLiteral("修改-线间隙"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已在 #%1 中点处开出 %2 mm 缺口").arg(e->id).arg(gap, 0, 'f', 2));
    })->setIcon(icons::make("line_gap"));
    mod->addSeparator();

    // ---- 几何构造 ----
    mk(mod, QStringLiteral("偏移..."), QKeySequence("Ctrl+Shift+O"), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Offset, QStringLiteral("偏移"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        double d = dlg.result().distMm;
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可偏移图元"), "warn"); return; }
        auto changes = modify::makeOffset(src, d);
        if (changes.empty()) { model_->log(QStringLiteral("偏移失败（线 / 圆 / 圆弧才有意义）"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-偏移"));
        auto added = modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已偏移 %1 个图元（Δ=%2 mm）").arg(added.size()).arg(d, 0, 'f', 2));
    })->setIcon(icons::make("offset"));
    mk(mod, QStringLiteral("倒角..."), QKeySequence(), [this] {
        // 快速倒角：取选中两条直线的角点，沿两线各回退默认距离做距离-距离倒角。
        // 无参数对话框、直接落盘（与「反向/分解/删除」同为一步式菜单项）。
        // 需要设定距离/角度时，使用命令行 chamfer <id1> <id2> <d1> <d2> [angleDeg]。
        int id1 = model_->selection().size() >= 1 ? model_->selection()[0] : -1;
        int id2 = model_->selection().size() >= 2 ? model_->selection()[1] : -1;
        if (id1 < 0 || id2 < 0) { model_->log(QStringLiteral("倒角需要 2 条选中直线"), "warn"); return; }
        const Element* ea = model_->element(id1);
        const Element* eb = model_->element(id2);
        if (!ea || !eb) { model_->log(QStringLiteral("图元不存在"), "warn"); return; }
        const double defD = 5.0;   // 默认回退距离（mm），可由命令行 override
        corner::CornerResult res = corner::chamferPair(*ea, *eb, defD, defD);
        if (!res.ok) { model_->log(QStringLiteral("倒角失败：") + res.msg, "warn"); return; }
        auto changes = modify::makeCornerChanges(id1, id2, res);
        model_->beginEdit(QStringLiteral("修改-倒角"));
        auto added = modify::applyChanges(model_, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        model_->log(QStringLiteral("已对 #%1 与 #%2 做倒角（默认 d=%3 mm），新增 %4：%5")
                        .arg(id1).arg(id2).arg(defD, 0, 'f', 2)
                        .arg(added.size()).arg(ids.join(", ")));
    })->setIcon(icons::make("bevel"));
    mk(mod, QStringLiteral("圆角..."), QKeySequence(), [this] {
        int id1 = model_->selection().size() >= 1 ? model_->selection()[0] : -1;
        int id2 = model_->selection().size() >= 2 ? model_->selection()[1] : -1;
        if (id1 < 0 || id2 < 0) { model_->log(QStringLiteral("圆角需要 2 条选中直线"), "warn"); return; }
        ModifyInputDialog dlg(ModifyInputDialog::M_Fillet, QStringLiteral("圆角"), id1, id2, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        std::vector<Element> pool = model_->elements();
        auto changes = modify::makeFillet(r.id1, r.id2, r.radiusMm, pool);
        if (changes.empty()) { model_->log(QStringLiteral("圆角失败（两线平行或不相交）"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-圆角"));
        auto added = modify::applyChanges(model_, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        model_->log(QStringLiteral("已对 #%1 与 #%2 做圆角（r=%3 mm），新增 %4：%5")
                        .arg(r.id1).arg(r.id2).arg(r.radiusMm, 0, 'f', 2)
                        .arg(added.size()).arg(ids.join(", ")));
    })->setIcon(icons::make("fillet"));
    mk(mod, QStringLiteral("等分..."), QKeySequence(), [this] {
        if (model_->selection().size() != 1) {
            model_->log(QStringLiteral("等分需要 1 条直线"), "warn"); return;
        }
        const Element* e = model_->element(model_->selection()[0]);
        if (!e || e->type != ElementType::Line) {
            model_->log(QStringLiteral("请选中一条直线"), "warn"); return;
        }
        ModifyInputDialog dlg(ModifyInputDialog::M_Divide, QStringLiteral("等分"), e->id, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        int n = dlg.result().n;
        auto changes = modify::makeBreakDivide({*e}, e->id, Vec3(0,0,0), n);
        if (changes.empty()) { model_->log(QStringLiteral("等分失败"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-等分"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已把 #%1 等分为 %2 段").arg(e->id).arg(n));
    })->setIcon(icons::make("divide"));
    mk(mod, QStringLiteral("拉伸..."), QKeySequence(), [this] {
        ModifyInputDialog dlg(ModifyInputDialog::M_Stretch, QStringLiteral("拉伸端点"), -1, -1, this);
        if (dlg.exec() != QDialog::Accepted) return;
        auto r = dlg.result();
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可拉伸图元"), "warn"); return; }
        auto changes = modify::makeStretch(src, Vec3(r.baseX, r.baseY), Vec3(r.toX, r.toY));
        if (changes.empty()) { model_->log(QStringLiteral("未命中端点（basePt 必须落在图元端点 1e-3 mm 容差内）"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-拉伸"));
        modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已拉伸 %1 个端点").arg(changes.size()));
    })->setIcon(icons::make("stretch"));
    mod->addSeparator();

    // ---- 工具与分解 ----
    mk(mod, QStringLiteral("属性..."), QKeySequence(), [this] {
        // 切到属性面板：仅显示属性 dock
        if (propDock_) propDock_->show();
        model_->log(QStringLiteral("属性面板已显示（编辑当前选中）"), "info");
    })->setIcon(icons::make("properties"));
    mk(mod, QStringLiteral("属性模板..."), QKeySequence(), [this] {
        // 属性模板 (Attributes)：把工具条当前的「颜色 / 线宽 / 线型」三件套
        // 一次性套用到选中图元（未选中时只更新"新图元默认样式"的提示）。
        // 与 LibreCAD 的 Modify > Attributes 语义一致：改属性，不改几何。
        if (model_->selection().empty()) {
            model_->log(QStringLiteral("属性模板：请先选中图元，再套用当前颜色 / 线宽 / 线型"), "warn");
            return;
        }
        const QColor c = model_->defaultColor();
        const double px = model_->defaultWidth();
        const LineStyle ls = model_->defaultLineStyle();
        model_->beginEdit(QStringLiteral("修改-属性模板"));
        const bool ok = model_->styleSelectedEx(&c, &px, &ls);
        model_->log(ok ? QStringLiteral("已把当前样式（%1 / %2 / %3）套用到 %4 个选中图元")
                            .arg(c.name(), QString::number(px, 'f', 1),
                                 QString::fromUtf8(lineStyleName(ls)))
                            .arg(model_->selection().size())
                       : QStringLiteral("属性模板套用失败"), ok ? "info" : "warn");
    })->setIcon(icons::make("attributes"));
    mk(mod, QStringLiteral("文本分解为字..."), QKeySequence(), [this] {
        // 简化：对 ElementType::Text 图元按字符拆为多段独立 Text（无字体子集化，仅字符级拷贝）
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可分解文本"), "warn"); return; }
        std::vector<modify::Change> changes;
        for (const auto& s : src) {
            if (s.type != ElementType::Text) continue;
            const QString t = s.text;
            const double w = 1.0;  // 单字间距 (mm)
            for (int k = 0; k < (int)t.size(); ++k) {
                Element e = s; e.id = -1;
                e.text = QString(t[k]);
                // 在源文字的起点 (x + k*w, y) 处放置
                if (!e.pts.empty()) e.pts[0] = Vec3(s.pts[0].x + k * w, s.pts[0].y, 0);
                modify::Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("字分解");
                changes.push_back(c);
            }
            modify::Change del; del.id = s.id; del.remove = true;
            changes.push_back(del);
        }
        if (changes.empty()) { model_->log(QStringLiteral("未找到可分解的文本图元"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-文本分解"));
        auto added = modify::applyChanges(model_, changes, QString());
        model_->log(QStringLiteral("已把 %1 个文本图元分解为 %2 个字")
                        .arg(src.size()).arg(added.size()));
    })->setIcon(icons::make("explode_text"));
    mk(mod, QStringLiteral("分解"), QKeySequence(), [this] {
        std::vector<Element> src;
        for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
        if (src.empty()) { model_->log(QStringLiteral("无可分解图元"), "warn"); return; }
        auto changes = modify::makeExplode(src);
        if (changes.empty()) { model_->log(QStringLiteral("无可分解对象"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-分解"));
        auto added = modify::applyChanges(model_, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        model_->log(QStringLiteral("已分解 %1 个图元 → %2 段：%3")
                        .arg(src.size()).arg(added.size()).arg(ids.join(", ")));
    })->setIcon(icons::make("explode"));
    mk(mod, QStringLiteral("删除"), QKeySequence::Delete, [this] { model_->deleteSelected(); })
        ->setIcon(icons::make("delete"));

    // 块（Block）：把命令行块操作封装为可视化子菜单（与【绘图>块】共用 buildBlockMenu）
    {
        QMenu* blk = mod->addMenu(icons::make("block"), tr("块(&B)"));
        buildBlockMenu(blk);
    }

    // ---- 命令补齐：修改类命令（与命令行共用 interp_->execute）----
    mk(mod, QStringLiteral("多段线编辑(&P)..."), QKeySequence(), [this] { interp_->execute("pedit"); });
    mk(mod, QStringLiteral("特性匹配(&M)"),       QKeySequence(), [this] { interp_->execute("matchprop"); });
    mk(mod, QStringLiteral("删除重复对象(&O)"),   QKeySequence(), [this] { interp_->execute("overkill"); });
    mk(mod, QStringLiteral("写块(&W)..."),        QKeySequence(), [this] { interp_->execute("wblock"); });
    mk(mod, QStringLiteral("插入块(&I)..."),      QKeySequence(), [this] { interp_->execute("insert"); });
    mk(mod, QStringLiteral("清理(&U)"),           QKeySequence(), [this] { interp_->execute("purge"); });

    e->addSeparator();
    // 挂到【编辑】菜单下（不再作为顶级菜单），保持「编辑类功能一个入口」
    e->addMenu(mod);

    // ---- 新增顶级菜单：选择（与附图 LibreCAD Select 菜单逐项对应）----
    // 结构：全部不选 / 全选 / 选择实体 / (取消)选择轮廓 / 窗口选择 / 窗口取消 /
    //       选择相交 / 取消相交 / (取消)选择图层 / 反选
    // 注意：工具型菜单项直接复用 toolActions_ 中已注册的 QAction（与工具条下拉
    // 共享高亮/互斥状态），即时命令（全选等）由 mk() 单独构造。
    QMenu* sel = new QMenu(tr("选择(&N)"), this);
    sel->setObjectName("menuSelect");
    mk(sel, QStringLiteral("全部不选"), QKeySequence("Ctrl+K"), [this] { model_->clearSelection(); })
        ->setIcon(icons::make("deselect_all"));
    mk(sel, QStringLiteral("全选"),     QKeySequence("Ctrl+A"), [this] { model_->selectAll(); })
        ->setIcon(icons::make("select_all"));
    sel->addSeparator();
    auto addSel = [&](const QString& id) {
        if (QAction* a = toolActions_.value(id)) sel->addAction(a);
    };
    addSel("select_entity");              // 选择实体
    addSel("toggle_contour");             // (取消)选择轮廓
    sel->addSeparator();
    addSel("select_window");              // 窗口选择
    addSel("deselect_window");            // 窗口取消
    sel->addSeparator();
    addSel("select_intersected");         // 选择相交
    addSel("deselect_intersected");       // 取消相交
    sel->addSeparator();
    addSel("fence");                      // 栏选（不规则范围）
    addSel("wp");                         // 圈围
    addSel("cp");                         // 圈交
    sel->addSeparator();
    addSel("toggle_layer");               // (取消)选择图层
    sel->addSeparator();
    mk(sel, QStringLiteral("反选"),     QKeySequence("Ctrl+Shift+I"), [this] { model_->invertSelection(); })
        ->setIcon(icons::make("invert_selection"));
    sel->addSeparator();
    // 快速选择（QSELECT）：打开 QSelectDialog，按类型/图层/颜色/线型筛选并落地选择集
    mk(sel, QStringLiteral("快速选择(&Q)..."), QKeySequence(), [this] { openQSelect(); })
        ->setIcon(icons::make("qselect"));
    sel->addSeparator();
    // 按属性累加选择：以当前选中的"参考图元"的线型/颜色为匹配条件，把同类图元加入选区，
    // 保留已有选择（不清除、不取消）——满足"累加选择"需求。
    mk(sel, QStringLiteral("按相同线型累加选择"), QKeySequence(), [this] {
        if (model_->selection().empty()) {
            model_->log(QStringLiteral("请先选中一个参考图元（以其线型为匹配条件）"), "warn");
            return;
        }
        const Element* ref = model_->element(model_->selection().front());
        if (!ref) return;
        model_->addSelectionByStyle(ref->lineStyle);
    })->setIcon(icons::make("select_entity"));
    mk(sel, QStringLiteral("按相同颜色累加选择"), QKeySequence(), [this] {
        if (model_->selection().empty()) {
            model_->log(QStringLiteral("请先选中一个参考图元（以其颜色为匹配条件）"), "warn");
            return;
        }
        const Element* ref = model_->element(model_->selection().front());
        if (!ref) return;
        model_->addSelectionByColor(ref->color);
    })->setIcon(icons::make("color"));
    e->addSeparator();
    // 挂到【编辑】菜单下（不再作为顶级菜单），与【修改】并列
    e->addMenu(sel);

    // 工具条第二行的「编辑」组即时命令：按文本去重后补挂到【编辑】末尾，
    // 这样工具条上新增的编辑类按钮会自动出现在菜单里，又不会与已有的「删除」重复。
    {
        QStringList used;
        for (QAction* a : e->actions()) if (a) used << a->text();
        for (const auto& grp : editGroups_) {
            if (grp.first != QStringLiteral("编辑")) continue;   // 视图 / 工作平面组归【视图】
            QList<QAction*> uniq;
            for (QAction* a : grp.second) if (a && !used.contains(a->text())) uniq << a;
            if (uniq.isEmpty()) continue;
            e->addSeparator();
            for (QAction* a : uniq) e->addAction(a);
        }
    }

    // ---- 顶级菜单：绘图（原「草图绘制」+ 直线/圆/圆弧/多边形/样条/椭圆/折线 7 个顶级菜单
    //      全部收纳到本菜单，各类图元的绘制方式以子菜单形式挂在下面）----
    QMenu* s = new QMenu(tr("绘图(&D)"), this);
    s->setObjectName("menuDraw");
    buildSketchMenu(s);
    // ---- 命令补齐：绘图类命令（与命令行共用 interp_->execute）----
    mk(s, QStringLiteral("图案填充(&H)..."), QKeySequence(), [this] { interp_->execute("hatch"); });
    mk(s, QStringLiteral("边界填充(&B)..."), QKeySequence(), [this] { interp_->execute("bhatch"); });
    mk(s, QStringLiteral("单行文字(&T)..."), QKeySequence(), [this, dlgText] { dlgText(false); });
    mk(s, QStringLiteral("多行文字(&M)..."), QKeySequence(), [this, dlgText] { dlgText(true); });
    mk(s, QStringLiteral("编辑文字(&E)..."), QKeySequence(), [this, dlgDdedit] { dlgDdedit(); });
    mk(s, QStringLiteral("定距等分(&D)..."), QKeySequence(), [this] { interp_->execute("measure"); });
    mk(s, QStringLiteral("形位公差(&G)..."), QKeySequence(), [this] { interp_->execute("tol"); });
    bar->addMenu(s);

    // ---- 顶级菜单：测量查询（只读几何查询面板，dist/area/massprop/id 的图形化入口）----
    QMenu* q = new QMenu(tr("测量查询(&I)"), this);
    q->setObjectName("menuInquiry");
    mk(q, QStringLiteral("打开测量查询面板..."), QKeySequence(), [this] { openInquiry(); })
        ->setIcon(icons::make("query"));
    bar->addMenu(q);

    QMenu* v = new QMenu(tr("视图(&V)"), this);
    v->setObjectName("menuView");
    mk(v, QStringLiteral("放大"),     QKeySequence::ZoomIn,  [this] { view_->zoomBy(1.2); })
        ->setIcon(icons::make("zoomin"));
    mk(v, QStringLiteral("缩小"),     QKeySequence::ZoomOut, [this] { view_->zoomBy(1.0 / 1.2); })
        ->setIcon(icons::make("zoomout"));
    mk(v, QStringLiteral("适配窗口"), QKeySequence("Ctrl+0"), [this] { view_->fitView(); })
        ->setIcon(icons::make("fit"));
    mk(v, QStringLiteral("窗口缩放"), QKeySequence("Ctrl+9"), [this] { view_->setZoomBoxMode(true); })
        ->setIcon(icons::make("zoombox"));
    v->addSeparator();
    QAction* aGrid = mk(v, QStringLiteral("显示栅格"), QKeySequence("F7"),
                        [this] { model_->setShowGrid(!model_->showGrid()); });
    aGrid->setIcon(icons::make("grid"));
    aGrid->setCheckable(true);
    aGrid->setChecked(model_->showGrid());
    aGrid_ = aGrid;   // gridToggled 连接在 hookActiveDoc 登记（随活动文档换绑）

    // 绘图比例入口已迁至【文件】菜单（属图纸属性），此处不再重复提供，避免同一功能两处出现

    QMenu* plane = new QMenu(tr("视图方向"), this);
    mk(plane, QStringLiteral("XY 平面（俯视）"), QKeySequence("Ctrl+1"), [this] { model_->setPlane(Plane::XY); })
        ->setIcon(icons::make("plane_xy"));
    mk(plane, QStringLiteral("YZ 平面（左视）"), QKeySequence("Ctrl+2"), [this] { model_->setPlane(Plane::YZ); })
        ->setIcon(icons::make("plane_yz"));
    mk(plane, QStringLiteral("XZ 平面（主视）"), QKeySequence("Ctrl+3"), [this] { model_->setPlane(Plane::XZ); })
        ->setIcon(icons::make("plane_xz"));
    v->addMenu(plane);
    v->addSeparator();

    // 面板显隐切换：关闭后可在此重新勾选（QDockWidget::toggleViewAction 自带勾选态同步）
    QMenu* panels = new QMenu(tr("面板"), this);
    panels->addAction(bomDock_->toggleViewAction());
    panels->addAction(elemDock_->toggleViewAction());
    panels->addAction(layerDock_->toggleViewAction());
    panels->addAction(propDock_->toggleViewAction());
    panels->addAction(logDock_->toggleViewAction());
    panels->addAction(cmdDock_->toggleViewAction());
    panels->addAction(designDock_->toggleViewAction());
    panels->addSeparator();
    if (toolbar_)  panels->addAction(toolbar_->toggleViewAction());
    if (toolbar2_) panels->addAction(toolbar2_->toggleViewAction());
    panels->addSeparator();
    panels->addAction(QStringLiteral("全部显示"), this, [this] {
        for (auto* d : {bomDock_, elemDock_, propDock_, logDock_, cmdDock_, layerDock_}) d->show();
        if (toolbar_)  toolbar_->show();
        if (toolbar2_) toolbar2_->show();
        model_->log(QStringLiteral("已恢复全部面板"));
    });
    v->addMenu(panels);
    mk(v, QStringLiteral("重生成(&R)"), QKeySequence("F5"), [this] { interp_->execute("regen"); });
    bar->addMenu(v);

    // ---- 脚本（二次开发宿主）----
    QMenu* scr = new QMenu(tr("脚本(&S)"), this);
    scr->setObjectName("menuScript");
    mk(scr, QStringLiteral("运行 JavaScript..."), QKeySequence(), [this] { onRunJavaScript(); })
        ->setIcon(icons::make("script"));
    mk(scr, QStringLiteral("运行脚本文件..."), QKeySequence(), [this] { onRunScriptFile(); })
        ->setIcon(icons::make("script"));
    mk(scr, QStringLiteral("可用脚本引擎"), QKeySequence(), [this] {
        model_->log(QStringLiteral("可用脚本引擎：%1").arg(scriptHost_->availableLanguages().join(", ")));
    });

    // ---- 动作宏 / SCR 脚本批处理（录制 → 保存 → 回放）----
    scr->addSeparator();
    QAction* aRec = mk(scr, QStringLiteral("宏：开始录制"), QKeySequence(), [this] {
        QAction* act = qobject_cast<QAction*>(sender());
        const bool wasRecording = MacroRecorder::instance().isRecording();
        interp_->execute(wasRecording ? "macrostop" : "macrorec");
        if (act) act->setText(MacroRecorder::instance().isRecording()
                                  ? QStringLiteral("宏：停止录制")
                                  : QStringLiteral("宏：开始录制"));
    });
    aRec->setIcon(icons::make("script"));
    // 菜单打开时同步录制状态（命令行 macrorec/macrostop 也能改变状态）
    connect(scr, &QMenu::aboutToShow, this, [aRec] {
        aRec->setText(MacroRecorder::instance().isRecording()
                          ? QStringLiteral("宏：停止录制")
                          : QStringLiteral("宏：开始录制"));
    });
    mk(scr, QStringLiteral("宏：保存录制..."), QKeySequence(), [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存动作宏"), QString(),
            QStringLiteral("脚本文件 (*.scr);;所有文件 (*.*)"));
        if (path.isEmpty()) return;
        interp_->execute(QStringLiteral("macrosave \"%1\"").arg(path));
    })->setIcon(icons::make("script"));
    scr->addSeparator();
    mk(scr, QStringLiteral("运行 SCR 脚本..."), QKeySequence(), [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("运行 SCR 脚本（每行一条命令，# 为注释）"), QString(),
            QStringLiteral("脚本文件 (*.scr);;所有文件 (*.*)"));
        if (path.isEmpty()) return;
        interp_->execute(QStringLiteral("scr \"%1\"").arg(path));
    })->setIcon(icons::make("script"));
    bar->addMenu(scr);

    // ---- 界面定制（CUIx + 命令别名，二次开发生态）----
    QMenu* cui = new QMenu(tr("界面(&U)"), this);
    cui->setObjectName("menuCui");
    mk(cui, QStringLiteral("编辑命令别名文件..."), QKeySequence(), [this] { openAliasFile(); })
        ->setIcon(icons::make("script"));
    mk(cui, QStringLiteral("重新加载命令别名"), QKeySequence(), [this] {
        interp_->execute("aliasreload");
    })->setIcon(icons::make("script"));
    mk(cui, QStringLiteral("命令别名列表"), QKeySequence(), [this] {
        interp_->execute("aliaslist");
    })->setIcon(icons::make("script"));
    cui->addSeparator();
    // 工作空间子菜单：切换项在展开时动态重建（反映当前工作空间列表与勾选态）
    QMenu* wsMenu = new QMenu(tr("工作空间"), this);
    cui->addMenu(wsMenu);
    connect(wsMenu, &QMenu::aboutToShow, this, [this, wsMenu] {
        wsMenu->clear();
        QAction* def = wsMenu->addAction(QStringLiteral("（默认 / 全部显示）"));
        def->setCheckable(true);
        def->setChecked(CuiManager::instance().currentWorkspace().isEmpty());
        connect(def, &QAction::triggered, this, [this] {
            CuiManager::instance().setCurrentWorkspace("");
            applyCui();
        });
        for (const QString& id : CuiManager::instance().workspaceIds()) {
            const CuiWorkspace* w = CuiManager::instance().workspace(id);
            QAction* a = wsMenu->addAction(w ? w->title : id);
            a->setCheckable(true);
            a->setChecked(CuiManager::instance().currentWorkspace() == id);
            connect(a, &QAction::triggered, this, [this, id] {
                CuiManager::instance().setCurrentWorkspace(id);
                applyCui();
            });
        }
        wsMenu->addSeparator();
        {
            QAction* a = wsMenu->addAction(QStringLiteral("新建工作空间（保存当前视图）..."));
            connect(a, &QAction::triggered, this, [this] { saveCurrentAsWorkspaceDialog(); });
        }
        {
            QAction* a = wsMenu->addAction(QStringLiteral("工作空间列表"));
            connect(a, &QAction::triggered, this, [this] { interp_->execute("wslist"); });
        }
    });
    cui->addSeparator();
    mk(cui, QStringLiteral("加载界面配置 (CUIx)..."), QKeySequence(), [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("加载界面配置"),
            QString(), QStringLiteral("CUIx 文件 (*.cuix);;所有文件 (*.*)"));
        if (path.isEmpty()) return;
        interp_->execute(QStringLiteral("cuiload \"%1\"").arg(path));
    })->setIcon(icons::make("properties"));
    mk(cui, QStringLiteral("保存界面配置 (CUIx)"), QKeySequence(), [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存界面配置"),
            QString(), QStringLiteral("CUIx 文件 (*.cuix);;所有文件 (*.*)"));
        if (path.isEmpty()) return;
        interp_->execute(QStringLiteral("cuisave \"%1\"").arg(path));
    })->setIcon(icons::make("properties"));
    mk(cui, QStringLiteral("新建自定义工具条..."), QKeySequence(), [this] { newToolbarDialog(); })
        ->setIcon(icons::make("properties"));
    bar->addMenu(cui);

    // ---- 插件（原生 SDK 二次开发框架）----
    QMenu* plug = new QMenu(tr("插件(&X)"), this);
    plug->setObjectName("menuPlugin");
    mk(plug, QStringLiteral("加载插件文件..."), QKeySequence(), [this] { onLoadPlugin(); })
        ->setIcon(icons::make("properties"));
    mk(plug, QStringLiteral("重新加载插件目录"), QKeySequence(), [this] { onReloadPlugins(); })
        ->setIcon(icons::make("properties"));
    pluginLoadedSub_ = new QMenu(tr("已加载插件"), this);
    pluginLoadedSub_->setIcon(icons::make("properties"));
    plug->addMenu(pluginLoadedSub_);
    refreshPluginMenu();   // 填充已加载列表（如启动时已自动加载示例插件）
    bar->addMenu(plug);

    // ---- 工具（统一「选项」入口：应用级配置持久化 + 分类树表单）----
    QMenu* tools = new QMenu(tr("工具(&T)"), this);
    tools->setObjectName("menuTools");
    mk(tools, QStringLiteral("选项..."), QKeySequence(), [this] { openOptionsDialog(); });
    // ---- 命令补齐：格式/工具类命令（与命令行共用 interp_->execute）----
    mk(tools, QStringLiteral("线型(&L)..."), QKeySequence(), [this] { interp_->execute("linetype"); });
    mk(tools, QStringLiteral("特性(&P)..."), QKeySequence(), [this] { interp_->execute("properties"); });
    bar->addMenu(tools);

    QMenu* h = new QMenu(tr("帮助(&H)"), this);
    h->setObjectName("menuHelp");
    mk(h, QStringLiteral("命令列表"), QKeySequence("F1"), [this] { interp_->execute("help"); });
    // 崩溃可复现诊断包：一键导出（异常退出启动提示与此共用 DiagnosticsDialog）
    mk(h, QStringLiteral("导出诊断包..."), QKeySequence(), [this] { exportDiagnosticBundle(); });
    mk(h, QStringLiteral("打开日志目录"), QKeySequence(), [this] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(SketchCAD::Diagnostics::logDir()));
    });
    mk(h, QStringLiteral("关于"), QKeySequence(), [this] {
        // 版本号来自 version.h（CMake project VERSION 生成的宏），
        // 与 exe 文件属性（app.rc）、安装包名（installer.iss）同源于同一个版本号，
        // 三者天然一致，不会出现「关于显示 1.1.0、属性却是 1.0.0」的商用事故。
        model_->log(QStringLiteral("航空CAD 草图模块 v%1 — C++17 / Qt6 实现")
                        .arg(QString::fromLatin1(SKETCHCAD_VERSION)));
        QMessageBox::about(this, QStringLiteral("关于 SketchCAD"),
            QStringLiteral(
                "<h3>SketchCAD 草图设计系统</h3>"
                "<p><b>版本</b>：%1（文件属性 %2）</p>"
                "<p><b>版权所有</b>：%3</p>"
                "<p>基于 C++17 / Qt 6 构建，核心几何与约束求解为自研实现。</p>"
                "<p>本软件为专有商业软件，详见安装目录内 LICENSE.txt。</p>")
                .arg(QString::fromLatin1(SKETCHCAD_VERSION),
                     QString::fromLatin1(SKETCHPAD_VERSION),
                     QString::fromUtf8(SKETCHCAD_COPYRIGHT)));
    });
    bar->addMenu(h);
}

void MainWindow::openInquiry() {
    InquiryDialog dlg(model_, this);
    dlg.exec();
}

// MTEXT 多行文字：激活 mtext 工具，用户在画布点击插入点后弹出编辑框
// （MTextTool::onPress 负责 MTextDialog + addElement，与工具条按钮同一链路）。
void MainWindow::openMTextDialog() {
    model_->setActiveTool(QStringLiteral("mtext"));
}

// STYLE 文字样式管理器：创建/修改/删除样式并批量应用到已有文字对象。
// 样式表存于 SketchModel（随图 JSON 存档），图元经 styleName 引用——
// 修改样式后所有引用它的文字在下次重绘时自动更新。
void MainWindow::openStyleManager() {
    StyleManagerDialog dlg(model_, this);
    dlg.exec();
}

// 运行 JavaScript：弹窗输入源码，经脚本宿主执行；输出与错误实时进日志面板，并弹窗提示结果。
void MainWindow::onRunJavaScript() {
    bool ok = false;
    QString code = QInputDialog::getMultiLineText(this, QStringLiteral("运行 JavaScript"),
        QStringLiteral("输入 JavaScript 脚本。全局对象 app 提供：addLine/addCircle/addArc/addRect/addPoint/addText、"
                       "elementCount/elementInfo/removeElement、run(命令)/log(消息)；print(...) 输出到日志。"),
        QStringLiteral("// 示例：画一个十字标记\n"
                       "app.addLine(-50, 0, 50, 0);\n"
                       "app.addLine(0, -50, 0, 50);\n"
                       "print('当前图元数 =', app.elementCount());\n"),
        &ok);
    if (!ok || code.trimmed().isEmpty()) return;
    ScriptResult r = scriptHost_->runCode("javascript", code);
    if (!r.ok) {
        QMessageBox::warning(this, QStringLiteral("脚本错误"), r.error);
    } else {
        model_->log(QStringLiteral("JavaScript 执行完成，图元数=%1").arg(scriptHost_->api()->elementCount()));
        if (!r.output.trimmed().isEmpty())
            model_->log(QStringLiteral("脚本输出：%1").arg(r.output.trimmed()));
    }
}

// 运行脚本文件：按扩展名推断语言（.js→javascript/.py→python/.vbs→vba），经脚本宿主执行。
void MainWindow::onRunScriptFile() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择脚本文件"),
        QString(), QStringLiteral("脚本 (*.js *.py *.vbs);;JavaScript (*.js);;Python (*.py);;VBA/VBScript (*.vbs);;所有文件 (*.*)"));
    if (path.isEmpty()) return;
    QString ext = QFileInfo(path).suffix().toLower();
    QString lang;
    if (ext == "js")       lang = "javascript";
    else if (ext == "py")  lang = "python";
    else if (ext == "vbs") lang = "vba";
    else { model_->log(QStringLiteral("无法根据扩展名判断脚本语言：%1").arg(path)); return; }
    if (!scriptHost_->isLanguageAvailable(lang)) {
        QMessageBox::warning(this, QStringLiteral("引擎不可用"),
            QStringLiteral("%1 引擎不可用（可能未启用构建开关或缺少运行时）").arg(lang));
        return;
    }
    ScriptResult r = scriptHost_->runFile(lang, path);
    if (!r.ok) QMessageBox::warning(this, QStringLiteral("脚本错误"), r.error);
    else       model_->log(QStringLiteral("脚本执行完成：%1").arg(path));
}

// ===================== 插件框架（原生 SDK 二次开发） =====================

// 加载插件文件：文件对话框选 .dll/.so，交由 PluginManager 校验 ABI 并加载。
void MainWindow::onLoadPlugin() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("加载插件"),
        QString(), QStringLiteral("插件 (*.dll *.so *.dylib);;所有文件 (*.*)"));
    if (path.isEmpty()) return;
    if (pluginManager_->loadPlugin(path))
        model_->log(QStringLiteral("插件加载成功：%1").arg(path));
    else
        QMessageBox::warning(this, QStringLiteral("插件加载失败"),
            QStringLiteral("未能加载插件：%1\n详见日志面板（ABI 不兼容 / 符号缺失 / 初始化失败等）。").arg(path));
}

// 重新加载插件目录：先整体卸载，再扫描宿主同目录 plugins/ 重新加载。
void MainWindow::onReloadPlugins() {
    pluginManager_->unloadAll();
    const int n = pluginManager_->loadDirectory(
        QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));
    model_->log(QStringLiteral("已重新加载插件目录，当前加载 %1 个插件").arg(n));
    refreshPluginMenu();
}

// 刷新「已加载插件」子菜单：列出每个插件的名称/版本/分类，并附「卸载」操作。
void MainWindow::refreshPluginMenu() {
    if (!pluginLoadedSub_) return;
    pluginLoadedSub_->clear();
    const auto& list = pluginManager_->loadedPlugins();
    if (list.isEmpty()) {
        QAction* empty = pluginLoadedSub_->addAction(QStringLiteral("（无已加载插件）"));
        empty->setEnabled(false);
        return;
    }
    for (const auto& p : list) {
        const QString name = p.info ? QString::fromUtf8(p.info->name) : QStringLiteral("?");
        const QString ver  = p.info ? QString::fromUtf8(p.info->version) : QString();
        const QString cat  = p.info ? QString::fromUtf8(p.info->category) : QString();
        const QString label = QStringLiteral("%1  v%2  [%3]").arg(name, ver, cat);
        QAction* a = pluginLoadedSub_->addAction(label);
        a->setData(p.path);
        // 点击插件项 → 写日志详情；右键「卸载」单独提供
        QAction* unload = pluginLoadedSub_->addAction(
            QStringLiteral("  卸载 %1").arg(name));
        unload->setData(p.path);
        connect(unload, &QAction::triggered, this, [this, path = p.path] {
            if (pluginManager_->unloadPlugin(path))
                model_->log(QStringLiteral("已卸载插件：%1").arg(path));
        });
        Q_UNUSED(a);
    }
}

// 双击 Text/MText 图元 → 弹出多行文字编辑框；双击 Table → 弹出表格编辑器。
// 就地更新数据字段（不重建图元、不动 id/图层/颜色/线型），
// 单行 Text 编辑后仍保持 Text 类型（lineSpacing/mtextWidth 对 Text 无害且不序列化）。
void MainWindow::editElementText(int id) {
    const Element* e = model_->element(id);
    if (!e) return;

    // ---- 表格（Table）：表格编辑器，更新表格数据字段 ----
    if (e->type == ElementType::Table) {
        TableDialog dlg(model_, this, e);
        if (dlg.exec() != QDialog::Accepted) return;
        const Element upd = dlg.buildElement(e->pts.empty() ? Vec3() : e->pts[0]);
        model_->beginEdit(QStringLiteral("编辑表格"));
        model_->updateElementQuiet(id, [&upd](Element& x) {
            x.tableRows  = upd.tableRows;
            x.tableCols  = upd.tableCols;
            x.rowHeight  = upd.rowHeight;
            x.colWidths  = upd.colWidths;
            x.tableCells = upd.tableCells;
            x.radius     = upd.radius;     // 字高
            x.textAlign  = upd.textAlign;  // 单元格文字对齐
        });
        model_->log(QStringLiteral("已更新表格 #%1（%2×%3）")
                        .arg(id).arg(upd.tableRows).arg(upd.tableCols));
        return;
    }

    // ---- 标注/引线/形位公差：就地编辑覆盖文本（AutoCAD 双击标注约定）----
    // 标注文字 el.text 为空时显示测量值；写入覆盖文本后由标注渲染器优先显示。
    if (e->type == ElementType::Dimension || e->type == ElementType::MLeader
        || e->type == ElementType::Fcf) {
        bool ok = false;
        const QString txt = QInputDialog::getMultiLineText(
            this, QStringLiteral("编辑文字"),
            QStringLiteral("覆盖文本（留空恢复测量值/默认内容）："), e->text, &ok);
        if (!ok) return;
        model_->beginEdit(QStringLiteral("编辑标注文字"));
        model_->updateElementQuiet(id, [txt](Element& x) { x.text = txt; });
        model_->log(QStringLiteral("已更新标注文字 #%1").arg(id));
        return;
    }

    if (e->type != ElementType::Text && e->type != ElementType::MText) return;

    MTextDialog dlg(model_, this, e);
    if (dlg.exec() != QDialog::Accepted) return;

    const Element upd = dlg.buildElement(e->pts.empty() ? Vec3() : e->pts[0]);
    model_->beginEdit(QStringLiteral("编辑文字"));
    model_->updateElementQuiet(id, [&upd](Element& x) {
        x.text        = upd.text;         // 内容（支持 \n 与 %<字段>% 占位）
        x.radius      = upd.radius;       // 字高(mm)，复用 radius
        x.widthFactor = upd.widthFactor;
        x.textAlign   = upd.textAlign;
        x.styleName   = upd.styleName;
        x.lineSpacing = upd.lineSpacing;
        x.mtextWidth  = upd.mtextWidth;
    });
    model_->log(QStringLiteral("已更新文字 #%1（样式 %2）")
                    .arg(id)
                    .arg(upd.styleName.isEmpty() ? QStringLiteral("默认") : upd.styleName));
}

// ============================================================================
// 选择工具：编组（GROUP）/ 快速选择（QSELECT）/ 过滤器（FILTER）
// ============================================================================
void MainWindow::openGroupManager() {
    GroupManagerDialog dlg(model_, this);
    dlg.exec();
}

void MainWindow::openUnitsLimits() {
    UnitsLimitsDialog dlg(model_, this);
    dlg.exec();   // 确定后经 setUnits/setLimits 发信号，状态栏/参数条实时跟随
}

void MainWindow::openSnapSettings() {
    SnapSettingsDialog dlg(model_, this);
    dlg.exec();   // 确定后经 setSnapConfig 落地为全局持久配置（随图存档、不入撤销栈）
}

void MainWindow::openQSelect() {
    QSelectDialog dlg(model_, this);
    dlg.exec();
}

void MainWindow::openFilterDialog() {
    FilterDialog dlg(model_, this);
    dlg.exec();
}

// ============================================================================
// 打印 / 出图模块入口
// ============================================================================
void MainWindow::openPrintSettings() {
    PrintSettingsDialog dlg(model_, currentPrintSettings_, this);
    if (dlg.exec() == QDialog::Accepted)
        currentPrintSettings_ = dlg.settings();
}

// 发布（PUBLISH）：勾选布局批量出图（单多页 PDF / 每布局一档 / 物理打印机）。
// 模型空间页沿用当前打印设置（发布时视口清空 = 模型空间打印语义）。
void MainWindow::openPublish() {
    PublishDialog dlg(model_, currentPrintSettings_, this);
    dlg.exec();
}

void MainWindow::openPrintPreview() {
    PrintPreviewDialog dlg(model_, currentPrintSettings_, this);
    dlg.exec();
}

void MainWindow::exportPdfDirect() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PDF"),
                                                     QStringLiteral("SketchCAD.pdf"),
                                                     QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    if (PrintEngine::exportPdf(model_, currentPrintSettings_, path))
        model_->log(QStringLiteral("已导出 PDF：%1").arg(path), "info");
    else
        model_->log(QStringLiteral("导出 PDF 失败"), "warn");
}

void MainWindow::exportPdfLayoutsDirect() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PDF（全部布局）"),
                                                     QStringLiteral("SketchCAD.pdf"),
                                                     QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    const int pages = PrintEngine::exportPdfLayouts(model_, path);
    if (pages > 0)
        model_->log(QStringLiteral("已按布局导出 PDF（%1 页）：%2").arg(pages).arg(path), "info");
    else
        model_->log(QStringLiteral("导出 PDF 失败"), "warn");
}

void MainWindow::printDirect() {
    QPrinter printer(QPrinter::HighResolution);
    // 选项里配置的默认打印设备：存在才使用，否则回落系统默认
    if (appSettings_ && !appSettings_->printerName().isEmpty()) {
        if (QPrinterInfo::availablePrinterNames().contains(appSettings_->printerName()))
            printer.setPrinterName(appSettings_->printerName());
    }
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // 记住本次实际使用的设备（用户习惯，供下次直接打印与选项回显）
    if (appSettings_ && !printer.printerName().isEmpty())
        appSettings_->setPrinterName(printer.printerName());
    if (!PrintEngine::print(model_, currentPrintSettings_, &printer))
        model_->log(QStringLiteral("打印失败"), "warn");
    else
        model_->log(QStringLiteral("已发送至打印机"), "info");
}

// ============================================================================
// 块（Block）GUI 入口：把命令行 block 操作封装为可视化菜单/对话框
// ============================================================================
void MainWindow::buildBlockMenu(QMenu* m) {
    // 与 buildMenu 中的 mk 等价的局部构造器（buildBlockMenu 是独立成员，复用 mk 不便）
    auto add = [this](QMenu* menu, const QString& text, const QKeySequence& key,
                      auto fn, const QString& icon) {
        QAction* a = menu->addAction(text);
        if (!key.isEmpty()) a->setShortcut(key);
        a->setShortcutContext(Qt::ApplicationShortcut);
        a->setIcon(icons::make(icon));
        connect(a, &QAction::triggered, this, fn);
        return a;
    };
    add(m, QStringLiteral("创建块..."), QKeySequence("Ctrl+Shift+B"),
        [this] { openCreateBlockDialog(); }, "block");
    add(m, QStringLiteral("插入块..."), QKeySequence(),
        [this] { openInsertBlockDialog(); }, "block");
    add(m, QStringLiteral("在位编辑块..."), QKeySequence(),
        [this] { openBlockEditorDialog(); }, "block");
    m->addSeparator();
    add(m, QStringLiteral("块管理器..."), QKeySequence(),
        [this] { openBlockManagerDialog(); }, "block");
    add(m, QStringLiteral("分解块..."), QKeySequence(),
        [this] { explodeBlockFromMenu(); }, "explode");
    m->addSeparator();
    add(m, QStringLiteral("附加外部参照..."), QKeySequence(),
        [this] { openAttachXrefDialog(); }, "block");
    add(m, QStringLiteral("分离外部参照..."), QKeySequence(),
        [this] { detachXrefFromMenu(); }, "block");
}

void MainWindow::openInsertBlockDialog() {
    InsertBlockDialog dlg(model_, this);
    dlg.exec();
}

void MainWindow::openCreateBlockDialog() {
    if (model_->selection().empty()) {
        QMessageBox::information(this, QStringLiteral("创建块"),
            QStringLiteral("请先选中要收进块的图元，再执行「创建块」。"));
        return;
    }
    CreateBlockDialog dlg(model_, this);
    dlg.exec();
}

void MainWindow::openBlockManagerDialog() {
    BlockManagerDialog dlg(model_, this);
    dlg.exec();
}

void MainWindow::openBlockEditorDialog() {
    // BEDIT：目标块名解析优先级 ① 当前选中的块引用 → 其块名；② 当前选择的单个图元所引用的块；
    // ③ 弹窗让用户从块表里挑一个。均无则提示先插入/选中块。
    QString name;
    for (int id : model_->selection()) {
        const Element* e = model_->element(id);
        if (e && e->type == ElementType::BlockRef) { name = e->blockName; break; }
    }
    if (name.isEmpty() && model_->blockCount() == 1) name = model_->blockTable().front().name;
    if (name.isEmpty() || !model_->hasBlock(name)) {
        // 兜底：从块表里选一个
        if (model_->blockCount() == 0) {
            QMessageBox::information(this, QStringLiteral("在位编辑块"),
                QStringLiteral("当前图还没有块定义，请先「创建块」或「插入块」。"));
            return;
        }
        QStringList names;
        for (const auto& d : model_->blockTable()) names << d.name;
        bool ok = false;
        name = QInputDialog::getItem(this, QStringLiteral("在位编辑块"),
                                     QStringLiteral("选择要编辑的块："), names, 0, false, &ok);
        if (!ok || name.isEmpty()) return;
    }
    openBlockEditorDialog(name);
}

// 指定块名直接打开在位编辑器（双击块引用入口；菜单入口经上面解析后同样落到这里）。
void MainWindow::openBlockEditorDialog(const QString& blockName) {
    if (blockName.isEmpty() || !model_->hasBlock(blockName)) return;
    BlockEditorDialog dlg(model_, blockName, this);
    dlg.exec();   // 确定 → redefineBlockEntities（全部引用同步）；取消/Esc → 丢弃不落盘
}

void MainWindow::openDesignCenter() {
    designDock_->show();
    designDock_->raise();
}

void MainWindow::insertBlockAtViewCenter(const QString& name) {
    if (name.isEmpty() || !model_->hasBlock(name)) return;
    const Vec3 c = view_->viewCenterWorld();
    const int id = model_->insertBlock(name, c);
    if (id < 0) return;
    model_->setSelection({ id });
}

void MainWindow::explodeBlockFromMenu() {
    // 优先分解当前选中的块引用；多选则逐个分解
    std::vector<int> refs;
    for (int id : model_->selection()) {
        const Element* e = model_->element(id);
        if (e && e->type == ElementType::BlockRef) refs.push_back(id);
    }
    if (refs.empty()) {
        QMessageBox::information(this, QStringLiteral("分解块"),
            QStringLiteral("请先选中至少一个块引用（已插入的块），再执行「分解块」。"));
        return;
    }
    int total = 0;
    for (int id : refs) {
        auto added = model_->explodeBlockRef(id);
        total += (int)added.size();
    }
    model_->log(QStringLiteral("已分解 %1 个块引用 → %2 个图元")
                    .arg(refs.size()).arg(total));
}

void MainWindow::openAttachXrefDialog() {
    AttachXrefDialog dlg(model_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const int id = dlg.attachedId();
    if (id < 0) return;
    // 附加后把视口适配到新参照，避免它落在当前视野之外而被误判为"没进来"
    view_->fitView();
}

void MainWindow::detachXrefFromMenu() {
    const QStringList names = model_->xrefNames();
    if (names.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("分离外部参照"),
            QStringLiteral("当前图纸没有已附加的外部参照。"));
        return;
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, QStringLiteral("分离外部参照"),
        QStringLiteral("选择要分离的外部参照（嵌套参照只能在它的宿主图中分离）："),
        names, 0, false, &ok);
    if (!ok || pick.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("分离外部参照"),
            QStringLiteral("确定分离外部参照「%1」？它的全部参照实例将一并删除（可撤销）。")
                .arg(pick),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    model_->detachXref(pick);
}

// 【绘图】菜单 —— 不新建动作，直接挂载工具条已有的 QAction。
// 复用带来的直接好处：菜单项与工具条按钮共享 checked 状态、图标、快捷键，
// 用户从任一入口切换工具，另一处立刻同步高亮，不需要额外写同步代码。
// 整合后本菜单 = 全部绘图能力唯一入口：基本工具平铺 + 7 类图元绘制方式子菜单
// （直线 / 圆 / 圆弧 / 多边形 / 样条 / 椭圆 / 折线）+ 尺寸标注 + 绘制选项。

void MainWindow::buildSketchMenu(QMenu* m) {
    // ---- 基本绘图工具：与工具条第一行的分组一一对应，共享同一 QAction 指针 ----
    // 去重：部分基础工具（直线/矩形/多边形/圆/圆弧/椭圆/样条/样条编辑/折线/标注）
    // 在下方「绘制方式」「尺寸标注」子菜单中已作为首项出现，功能完全相同。按
    // 「保留含子菜单的入口、移除其余无子菜单的重复项」原则，此处跳过这些重复项，
    // 避免同一功能在两个层级重复出现，同时不丢失任何子功能入口（子菜单中仍可达）。
    // 修改类命令（修剪/延伸/旋转/镜像/偏移/分解）本就归属【编辑>修改】子菜单，
    // 故构建工具条时已不写入 drawGroups_，绘图菜单不再平铺它们。
    // 判定依据用 QAction::objectName()（createDropdownTool / addEx 已写入工具 id），
    // 而非 toolActions_[id] 反查：drop 主按钮 id 与下拉子菜单首项 id 常相同（如 "line"），
    // 子菜单构建会覆盖 toolActions_[id]，反查到的指针与 drawGroups_ 实际挂载的 mainAct
    // 不一致，按 objectName 则可稳定命中。
    static const char* kDupNames[] = {
        "line", "rect", "poly", "circle", "arc", "ellipse_1pt",
        "spline", "spline_from_polyline", "polyline", "dim"
    };
    QSet<QString> dupNames;
    for (const char* n : kDupNames) dupNames.insert(QString::fromUtf8(n));

    for (const auto& grp : drawGroups_) {
        QList<QAction*> kept;
        for (QAction* a : grp.second)
            if (!dupNames.contains(a->objectName())) kept << a;
        if (kept.isEmpty()) continue;               // 整组被去重掉（如「基本图元」全部迁入子菜单）
        m->addSection(grp.first);                   // 分组标题，与工具条分隔符一一对应
        for (QAction* a : kept) m->addAction(a);
    }

    // ---- 各类图元的完整绘制方式：收进二级子菜单 ----
    // 原先直线 / 圆 / 圆弧 / 多边形 / 样条 / 椭圆 / 折线 各自占一个顶级菜单，
    // 菜单栏被拆得又长又散。现在统一并入【绘图】：主菜单放主工具，绘制方式放子菜单，
    // 一个入口即可触达全部绘图能力，菜单项本身一个没少。
    m->addSection(QStringLiteral("绘制方式"));
    auto sub = [&](const QString& title, const QString& icon, void (MainWindow::*fn)(QMenu*)) {
        QMenu* sm = m->addMenu(icons::make(icon), title);
        (this->*fn)(sm);
        return sm;
    };
    sub(QStringLiteral("直线(&L)"),   QStringLiteral("line"),     &MainWindow::buildLineMenu);
    sub(QStringLiteral("构造线(&X)"), QStringLiteral("xline"),    &MainWindow::buildXlineMenu);
    sub(QStringLiteral("圆(&C)"),     QStringLiteral("circle"),   &MainWindow::buildCircleMenu);
    sub(QStringLiteral("圆弧(&A)"),   QStringLiteral("arc"),      &MainWindow::buildArcMenu);
    sub(QStringLiteral("多边形(&P)"), QStringLiteral("poly"),     &MainWindow::buildPolyMenu);
    sub(QStringLiteral("样条(&S)"),   QStringLiteral("spline"),   &MainWindow::buildSplineMenu);
    sub(QStringLiteral("椭圆(&E)"),   QStringLiteral("ellipse"),  &MainWindow::buildEllipseMenu);
    sub(QStringLiteral("折线(&Y)"),   QStringLiteral("polyline"), &MainWindow::buildPolylineMenu);

    // 尺寸标注子菜单：GB/T 4458.4-2003 的各种注法集中在这里
    m->addSection(QStringLiteral("尺寸标注  GB/T 4458.4 / ISO 129"));
    dimMenu_ = m->addMenu(icons::make("dim"), tr("尺寸标注"));
    for (QAction* a : dimActs_) if (a) dimMenu_->addAction(a);

    m->addSection(QStringLiteral("绘制选项"));
    m->addAction(actChain_);

    // 线宽子菜单（与工具条下拉框指向同一个 applyWidth）
    widthMenu_ = m->addMenu(icons::make("width"), tr("线宽"));
    auto* wg = new QActionGroup(this);
    wg->setExclusive(true);
    for (double px : {1.0, 2.0, 3.0, 5.0}) {
        QAction* a = widthMenu_->addAction(icons::widthBar(px),
                                           QString::fromUtf8(lineWidthName(px)));
        a->setCheckable(true);
        a->setActionGroup(wg);
        a->setData(px);
        connect(a, &QAction::triggered, this, [this, px] { applyWidth(px); });
        widthActs_ << a;
    }
    // 颜色子菜单：直接复用工具条按钮的那一个 QMenu 实例
    if (colorMenu_) m->addMenu(colorMenu_);

    m->addSeparator();

    // 航空制图与机械制图模板
    m->addSection(QStringLiteral("制图模板"));
    QAction* aircraftAct = m->addAction(QStringLiteral("飞机三面图..."));
    aircraftAct->setToolTip(QStringLiteral("按默认或自定义尺寸生成飞机正面图、侧面图、俯视图"));
    aircraftAct->setStatusTip(aircraftAct->toolTip());
    connect(aircraftAct, &QAction::triggered, this, [this] {
        AircraftViewsDialog dlg(model_, this);
        dlg.exec();
    });

    QAction* flangeAct = m->addAction(QStringLiteral("法兰盘三面图..."));
    flangeAct->setToolTip(QStringLiteral("按默认或自定义参数生成法兰盘主视图、俯视图、左视图"));
    flangeAct->setStatusTip(flangeAct->toolTip());
    connect(flangeAct, &QAction::triggered, this, [this] {
        FlangeViewsDialog dlg(model_, this);
        dlg.exec();
    });

    QAction* bearingAct = m->addAction(QStringLiteral("轴承座三面图..."));
    bearingAct->setToolTip(QStringLiteral("按默认或自定义参数生成轴承座主视图、俯视图、左视图（左视图全剖）"));
    bearingAct->setStatusTip(bearingAct->toolTip());
    connect(bearingAct, &QAction::triggered, this, [this] {
        BearingViewsDialog dlg(model_, this);
        dlg.exec();
    });

    QAction* housingAct = m->addAction(QStringLiteral("箱体视图..."));
    housingAct->setToolTip(QStringLiteral("按默认或自定义参数生成箱体主视图（全剖）、俯视图 B、A—A 断面、C 向视图（含剖切符号与粗糙度）"));
    housingAct->setStatusTip(housingAct->toolTip());
    connect(housingAct, &QAction::triggered, this, [this] {
        HousingViewsDialog dlg(model_, this);
        dlg.exec();
    });

    QAction* flangeHubAct = m->addAction(QStringLiteral("法兰盘轮毂视图..."));
    flangeHubAct->setToolTip(QStringLiteral("按默认或自定义参数生成法兰盘轮毂主视图（全剖 A—A）与左视图（含剖面线、剖切符号、粗糙度、尺寸标注）"));
    flangeHubAct->setStatusTip(flangeHubAct->toolTip());
    connect(flangeHubAct, &QAction::triggered, this, [this] {
        FlangeHubViewsDialog dlg(model_, this);
        dlg.exec();
    });

    QAction* floorPlanAct = m->addAction(QStringLiteral("户型平面图..."));
    floorPlanAct->setToolTip(QStringLiteral("按默认或自定义参数生成建筑户型平面图：墙体（剪力墙涂黑/双线内墙）、门（开启弧）、窗、家具图块、房间标注、四周建筑尺寸链与右侧图签"));
    floorPlanAct->setStatusTip(floorPlanAct->toolTip());
    connect(floorPlanAct, &QAction::triggered, this, [this] {
        FloorPlanViewsDialog dlg(model_, this);
        dlg.exec();
    });

    // 块（Block）：创建/插入/管理/分解的可视化入口（与【编辑>修改>块】共用 buildBlockMenu）
    {
        QMenu* blk = m->addMenu(icons::make("block"), tr("块(&B)"));
        buildBlockMenu(blk);
    }

    // 文字样式管理器（STYLE）：统一治理字体/字高/宽度因子并批量应用。
    // MTEXT 多行文字工具经上方「文字」分组（drawGroups_）进入，此处不重复挂载。
    QAction* styleAct = m->addAction(icons::make("style"), QStringLiteral("文字样式管理器..."));
    styleAct->setToolTip(QStringLiteral("文字样式管理器 (STYLE)：创建/修改/删除样式，统一字体、字高、宽度因子并批量应用到已有文字"));
    styleAct->setStatusTip(styleAct->toolTip());
    connect(styleAct, &QAction::triggered, this, [this] { openStyleManager(); });

    m->addSeparator();
    // 不给它绑 Esc：Esc 已经绑在「选择」工具上，两处都注册会触发 Qt 的
    // ambiguous shortcut（按键被吞掉、谁也不响应）。画布内的 Esc 由
    // GraphicsView::keyPressEvent 统一处理，这里只提供一个鼠标入口。
    QAction* cancel = m->addAction(QStringLiteral("取消当前绘制  (Esc)"));
    connect(cancel, &QAction::triggered, this, [this] { view_->cancelDrawing(); });
    // 注：原「编辑与视图」分组（editGroups_）已按语义拆分——
    // 编辑类即时命令归【编辑】菜单，视图 / 工作平面类本就在【视图】菜单中，此处不再重复挂载。
}

// 菜单构建辅助（DRY）：供各 build*Menu 复用。
//  - addSubMenu：生成一个带图标的二级子菜单，把 ids 中已注册的动作依次加入；
//  - addFlatGroups：把若干组动作以"分隔线分组"扁平排入菜单。
void MainWindow::addSubMenu(QMenu* parent, const QString& iconId,
                            const QString& title, const QStringList& ids) {
    QMenu* sm = parent->addMenu(icons::make(iconId), title);
    for (const QString& id : ids)
        if (QAction* a = toolActions_.value(id)) sm->addAction(a);
}

void MainWindow::addFlatGroups(QMenu* parent,
                               const std::initializer_list<QStringList>& groups) {
    bool first = true;
    for (const QStringList& g : groups) {
        if (!first) parent->addSeparator();
        first = false;
        for (const QString& id : g)
            if (QAction* a = toolActions_.value(id)) parent->addAction(a);
    }
}

// 【直线】菜单：把直线绘制方式按类别拆成中文二级子菜单。
// 全部复用工具条的 toolActions_ 指针，因此菜单项与工具条按钮共享高亮/快捷键，
// 与「草图绘制 > 直线绘制方式」是同一批动作的两个入口，不会不同步。
void MainWindow::buildLineMenu(QMenu* m) {
    addSubMenu(m, "line", QStringLiteral("基本"), {
        "line", "line_h", "line_v", "line_angle",
        "line_parallel", "line_parallel_pt", "line_bisector"});
    addSubMenu(m, "line", QStringLiteral("切线"), {
        "line_tangent_pc", "line_tangent_cc", "line_tangent_ortho"});
    addSubMenu(m, "line", QStringLiteral("垂直与角度"), {
        "line_orthogonal", "line_rel_angle", "line_angle_from_line",
        "line_orthogonal_from_line", "line_from_point_to_line"});
    addSubMenu(m, "line", QStringLiteral("正交折线"), {
        "line_snake", "line_snake_x", "line_snake_y"});
}

// 【构造线】菜单：XLINE 六种绘制方式（对应 AutoCAD Through/Hor/Ver/Angle/Bisect/Offset）。
// 全部复用工具条的 toolActions_ 指针，与「草图绘制 > 构造线」是同一批动作的两个入口。
void MainWindow::buildXlineMenu(QMenu* m) {
    addSubMenu(m, "xline", QStringLiteral("基本"), {
        "xline", "xline_h", "xline_v", "xline_angle",
        "xline_bisector", "xline_parallel"});
}

// 【圆】菜单：把圆形绘制方式按类别拆成中文二级子菜单。
// 全部复用工具条的 toolActions_ 指针，因此菜单项与工具条按钮共享高亮/快捷键，
// 与「草图绘制 > 圆」是同一批动作的两个入口，不会不同步。
void MainWindow::buildCircleMenu(QMenu* m) {
    addSubMenu(m, "circle", QStringLiteral("基本"), {
        "circle", "circle_2p", "circle_2pr", "circle3p", "circle_cr"});
    addSubMenu(m, "circle", QStringLiteral("相切"), {
        "circle_t2e1p", "circle_t1e2p", "circle_t2er", "circle_t3e"});
    addSubMenu(m, "circle", QStringLiteral("其他"), {
        "circle_inscribed", "circle_concentric", "circle_byarc"});
}

// 【圆弧】菜单：把圆弧绘制方式按类别拆成中文二级子菜单。
// 全部复用工具条的 toolActions_ 指针，与「草图绘制 > 圆弧」是同一批动作的两个入口。
void MainWindow::buildArcMenu(QMenu* m) {
    addSubMenu(m, "arc", QStringLiteral("基本"), {
        "arc", "arc_3p", "arc_cpc", "arc_cpaa",
        "arc_tpa", "arc_tpr", "arc_tpl", "arc_tph"});
    addSubMenu(m, "arc", QStringLiteral("相切"), {"arc_tan"});
    addSubMenu(m, "arc", QStringLiteral("椭圆弧"), {"arc_ea_axis", "arc_ea_1pt"});
}

// 【多边形】菜单：把多边形绘制方式按类别拆成中文二级子菜单。
// 全部复用工具条的 toolActions_ 指针，因此菜单项与工具条按钮共享高亮/快捷键，
// 与「草图绘制 > 多边形绘制方式」是同一批动作的两个入口，不会不同步。
void MainWindow::buildPolyMenu(QMenu* m) {
    // 自由多边形 / 矩形（主工具，不在子菜单中，直接作为顶级项）
    if (QAction* a = toolActions_.value("poly")) m->addAction(a);
    if (QAction* a = toolActions_.value("rect")) m->addAction(a);
    addSubMenu(m, "poly", QStringLiteral("正多边形"), {
        "poly_regular", "poly_circumscribed", "poly_edge"});
    addSubMenu(m, "poly", QStringLiteral("特殊矩形"), {
        "rect_chamfer", "rect_fillet"});
    addSubMenu(m, "poly", QStringLiteral("其他"), {"poly_star"});
}

// 【样条】菜单 —— 扁平布局，与参考图（FreeCAD Sketcher 的 Spline 菜单）逐项一致：
//   抛物线(4点)                          Parabola 4 points
//   ─────────────
//   抛物线(焦点准线)  双曲线              Parabola Focus Directrix / Hyperbola Ecci Point
//   ─────────────
//   样条曲线  样条过点  由多段线创建样条    Spline / Spline through points / Spline from polyline
//   ─────────────
//   追加样条点  插入样条点  删除样条点      Append / Insert / Remove spline points
//   ─────────────
//   删除两点之间  分解样条为线段           Remove between two points / Explode spline to lines
//   ─────────────
//   自由手绘线                            Freehand Line
// 全部复用工具条的 toolActions_ 指针，菜单项与工具条按钮共享高亮/快捷键。
void MainWindow::buildSplineMenu(QMenu* m) {
    addFlatGroups(m, {
        {"spline_parabola_4p"},
        {"spline_parabola_fd", "spline_hyperbola", "spline_parabola"},
        {"spline", "spline_through_points", "spline_from_polyline"},
        {"spline_append_pt", "spline_insert_pts", "spline_remove_pts"},
        {"spline_remove_between", "spline_explode"},
        {"freehand_line"},
    });
}

// 【椭圆】菜单 —— 扁平布局，与参考图（FreeCAD Sketcher 的 Ellipse 菜单）逐项一致：
//   椭圆(1点)                              Ellipse (1 Point)
//   ─────────────
//   椭圆(轴)                               Ellipse (Axis)
//   ─────────────
//   椭圆(焦点)                             Ellipse Foci Point
//   ─────────────
//   椭圆(4点)                              Ellipse 4 Point
//   ─────────────
//   椭圆(中心+3点)                         Ellipse Center and 3 Points
//   ─────────────
//   椭圆(内切)                             Ellipse Inscribed
// 全部复用工具条的 toolActions_ 指针，菜单项与工具条按钮共享高亮/快捷键。
void MainWindow::buildEllipseMenu(QMenu* m) {
    addFlatGroups(m, {
        {"ellipse_1pt"},
        {"ellipse_axis"},
        {"ellipse_foci"},
        {"ellipse_4p"},
        {"ellipse_c3p"},
        {"ellipse_inscribed"},
    });
}

// 折线顶级菜单（与 FreeCAD Sketcher Polyline 菜单的 10 项逐项一致）
void MainWindow::buildPolylineMenu(QMenu* m) {
    addFlatGroups(m, {
        {"polyline"},
        {"polyline_add_node", "polyline_append_node", "polyline_delete_node",
         "polyline_delete_between", "polyline_trim_segments"},
        {"polyline_equidistant", "polyline_from_segments",
         "polyline_arcs_to_chords", "polyline_change_segment_type"},
        {"polyline_fit", "polyline_spline", "polyline_width",
         "polyline_close", "polyline_join", "polyline_reverse"}
    });
}

// 参数输入条：位于工具条下方。角度直线用「角度 / 长度」，平行线用「间隔 / 条数」。
// 圆形半径模式将「长度」标签改为「半径」，值复用同一字段。
void MainWindow::buildParamBar() {
    paramBar_ = new QWidget(this);
    paramBar_->setObjectName("paramBar");
    auto* hl = new QHBoxLayout(paramBar_);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(8);

    hl->addWidget(new QLabel(QStringLiteral("参数输入条")));

    lblAngle_ = new QLabel(QStringLiteral("角度"));
    hl->addWidget(lblAngle_);
    sbAngle_ = new QDoubleSpinBox(this);
    sbAngle_->setRange(-360.0, 360.0);
    sbAngle_->setValue(model_->lineParams().angleDeg);
    sbAngle_->setSuffix(QStringLiteral("°"));
    sbAngle_->setFixedWidth(80);
    connect(sbAngle_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { model_->setLineParamAngle(v); });
    hl->addWidget(sbAngle_);

    lblLength_ = new QLabel(QStringLiteral("长度"));
    hl->addWidget(lblLength_);
    sbLength_ = new QDoubleSpinBox(this);
    sbLength_->setRange(0.0, 1e6);
    sbLength_->setValue(model_->lineParams().length);
    sbLength_->setSuffix(QStringLiteral(" mm"));
    sbLength_->setFixedWidth(90);
    connect(sbLength_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {   // 输入按当前单位口径，回写模型 mm
                model_->setLineParamLength(v * lengthFactorToMm(model_->units().length));
            });
    hl->addWidget(sbLength_);

    lblSpacing_ = new QLabel(QStringLiteral("间隔"));
    hl->addWidget(lblSpacing_);
    sbSpacing_ = new QDoubleSpinBox(this);
    sbSpacing_->setRange(0.1, 1e6);
    sbSpacing_->setValue(model_->lineParams().spacing);
    sbSpacing_->setSuffix(QStringLiteral(" mm"));
    sbSpacing_->setFixedWidth(90);
    connect(sbSpacing_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {   // 输入按当前单位口径，回写模型 mm
                model_->setLineParamSpacing(v * lengthFactorToMm(model_->units().length));
            });
    hl->addWidget(sbSpacing_);

    lblCount_ = new QLabel(QStringLiteral("条数"));
    hl->addWidget(lblCount_);
    sbCount_ = new QSpinBox(this);
    sbCount_->setRange(1, 999);
    sbCount_->setValue(model_->lineParams().count);
    connect(sbCount_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { model_->setLineParamCount(v); });
    hl->addWidget(sbCount_);

    hl->addSpacing(12);

    // 线型选择：绘图时可在此切换新图元的线型
    hl->addWidget(new QLabel(QStringLiteral("线型")));
    paramStyleBox_ = new QComboBox(this);
    paramStyleBox_->setIconSize(QSize(48, 16));
    paramStyleBox_->setMinimumContentsLength(8);
    // 限定下拉框最小宽，避免其(图标+文字)硬性最小宽度把主窗口最小宽推得过大而超屏
    paramStyleBox_->setMinimumWidth(120);
    for (int i = 0; i < kLineStyleCount; ++i) {
        LineStyle ls = (LineStyle)i;
        paramStyleBox_->addItem(icons::lineStyleIcon(ls),
                                QString::fromUtf8(lineStyleName(ls)), i);
    }
    connect(paramStyleBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i) { if (i >= 0) applyLineStyle(i); });
    hl->addWidget(paramStyleBox_);

    hl->addSpacing(12);
    paramHint_ = new QLabel(QStringLiteral("（选择「角度直线」或「平行线」时此处生效）"));
    paramHint_->setStyleSheet(QStringLiteral("color:#6c7484;"));
    // 提示文字占 paramBar 最小宽的大头(约 333px)：设弹性尺寸策略后大屏由 stretch 占满显示，
    // 窄屏时随 paramBar 一起压缩，从而让 paramBar/主窗口最小宽显著下降、不再把窗口顶出屏幕。
    paramHint_->setMinimumWidth(40);
    paramHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hl->addWidget(paramHint_, 1);

    updateParamBarForTool(model_->activeTool());
}

// 把单位设置的「显示/输入口径」应用到参数输入条：换后缀、把模型 mm 换算为当前
// 单位显示；编辑时由 buildParamBar 的 lambda 换算回 mm 落模。刷新期间 blockSignals，
// 避免 valueChanged 回环把刚换算的显示值又写回模型造成双重换算。
void MainWindow::applyUnitsToParamBar() {
    const LengthUnit u = model_->units().length;
    const double   f   = lengthFactorToMm(u);
    const QString suf  = QStringLiteral(" ") + lengthUnitShort(u);
    const auto sync = [f, &suf](QDoubleSpinBox* sb, double mm, double minMm) {
        if (!sb) return;
        sb->blockSignals(true);
        sb->setSuffix(suf);
        sb->setRange(minMm / f, 1.0e6 / f);
        sb->setValue(mm / f);
        sb->blockSignals(false);
    };
    const auto& lp = model_->lineParams();
    sync(sbLength_,  lp.length,  0.0);
    sync(sbSpacing_, lp.spacing, 0.1);
}

// 按当前命令启停参数输入条字段并给出操作提示：
// 角度直线 → 用角度/长度；平行线 → 用间隔/条数；
// 圆形半径模式 → 隐藏角度，将「长度」标签改为「半径」。
// 工具参数模式由 ToolRegistry 统一管理（DRY），不再硬编码工具 ID。
void MainWindow::updateParamBarForTool(const QString& t) {
    if (!paramBar_) return;
    const auto& reg = ToolRegistry::instance();
    const auto mode = reg.paramMode(t);
    const bool isAngle        = (mode == ParamBarMode::AngleLength);
    const bool isParallel     = (mode == ParamBarMode::SpacingCount);
    const bool isCircleRadius = (mode == ParamBarMode::Radius);
    const bool isPolySides    = (mode == ParamBarMode::Sides);
    if (sbAngle_)   sbAngle_->setEnabled(isAngle);
    if (sbLength_)  sbLength_->setEnabled(isAngle || isCircleRadius);
    if (sbSpacing_) sbSpacing_->setEnabled(isParallel);
    if (sbCount_)   sbCount_->setEnabled(isParallel || isPolySides);
    // 圆形半径模式：隐藏角度标签+输入框，将「长度」改为「半径」
    if (lblAngle_)  lblAngle_->setVisible(!isCircleRadius && !isPolySides);
    if (sbAngle_)   sbAngle_->setVisible(!isCircleRadius && !isPolySides);
    if (lblLength_) {
        lblLength_->setVisible(!isPolySides);
        lblLength_->setText(isCircleRadius
                            ? QStringLiteral("半径")
                            : QStringLiteral("长度"));
    }
    if (sbLength_)  sbLength_->setVisible(!isPolySides);
    // 多边形边数模式：隐藏间隔，将「条数」改为「边数」
    if (lblSpacing_) lblSpacing_->setVisible(!isPolySides);
    if (sbSpacing_)  sbSpacing_->setVisible(!isPolySides);
    if (lblCount_)   lblCount_->setText(isPolySides
                                        ? QStringLiteral("边数")
                                        : QStringLiteral("条数"));
    // 提示文本从 ToolRegistry 获取（单一数据源，消除 8 个硬编码 if-else 分支）
    if (paramHint_) {
        const QString hint = reg.paramHint(t);
        paramHint_->setText(hint.isEmpty()
                            ? QStringLiteral("（选择「角度直线」「平行线」「圆心+半径」等时此处生效）")
                            : hint);
    }
}

// ============================================================
// UI 工厂方法（Factory Pattern）
// ============================================================

const QVector<MainWindow::ToolSubMode>& MainWindow::lineSubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("两点直线"),       "line",                    QStringLiteral("两定点直线：点击起点与终点")},
        {QStringLiteral("水平线"),         "line_h",                  QStringLiteral("水平线：终点 Y 与起点相同，拖动时实时预览")},
        {QStringLiteral("竖直线"),         "line_v",                  QStringLiteral("竖直线：终点 X 与起点相同，拖动时实时预览")},
        {QStringLiteral("角度直线"),       "line_angle",              QStringLiteral("角度直线：在参数输入条设角度与长度，点击起点即绘制")},
        {QStringLiteral("平行线"),         "line_parallel",           QStringLiteral("平行线：选参考线后点击方向，按间隔生成多条")},
        {QStringLiteral("过点平行线"),     "line_parallel_pt",        QStringLiteral("过点平行线：选参考线后点一点，生成等长且与该点对齐的平行线")},
        {QStringLiteral("角平分线"),       "line_bisector",           QStringLiteral("两直线角平分线：选两参考直线后再拾取一点定长")},
        {QStringLiteral("点-圆切线"),      "line_tangent_pc",         QStringLiteral("点→圆切线：选圆后拾取外部点")},
        {QStringLiteral("圆-圆公切线"),    "line_tangent_cc",         QStringLiteral("两圆公切线：依次选两个圆，鼠标移动实时预览所有切线，点击确认最近切线")},
        {QStringLiteral("垂直参考线切线"), "line_tangent_ortho",      QStringLiteral("垂直于参考线的圆切线：选圆与参考直线")},
        {QStringLiteral("垂直线"),         "line_orthogonal",         QStringLiteral("垂直线：选参考线后拾取一点作垂线")},
        {QStringLiteral("相对角度线"),     "line_rel_angle",          QStringLiteral("相对参考线成角：选参考线后拾取一点")},
        {QStringLiteral("正交折线"),       "line_snake",              QStringLiteral("正交折线：逐段交替水平/竖直，右键结束")},
        {QStringLiteral("正交折线(水平起)"),"line_snake_x",            QStringLiteral("以水平方向开始的正交折线")},
        {QStringLiteral("正交折线(竖直起)"),"line_snake_y",            QStringLiteral("以竖直方向开始的正交折线")},
        {QStringLiteral("相对参考线角度"), "line_angle_from_line",    QStringLiteral("相对参考线成角：选参考线后拾取一点")},
        {QStringLiteral("参考线垂线"),     "line_orthogonal_from_line", QStringLiteral("从参考线某点作垂线")},
        {QStringLiteral("点到直线垂线"),   "line_from_point_to_line", QStringLiteral("点到直线垂线：选参考线后拾取外部点")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::xlineSubModes() {
    // 顺序与 AutoCAD XLINE 选项对应：Through(两点) / Hor / Ver / Angle / Bisect / Offset。
    // 主按钮 xline 固定排第一（默认方式 = 两点定线）。
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("两点构造线"),     "xline",           QStringLiteral("构造线 (XLINE 两点)：点两个点，过两点向两端无限延伸")},
        {QStringLiteral("水平构造线"),     "xline_h",         QStringLiteral("水平构造线 (H)：点击通过点，水平无限延伸")},
        {QStringLiteral("竖直构造线"),     "xline_v",         QStringLiteral("竖直构造线 (V)：点击通过点，竖直无限延伸")},
        {QStringLiteral("角度构造线"),     "xline_angle",     QStringLiteral("角度构造线 (A)：参数输入条设角度，点击基点即按该方向无限延伸")},
        {QStringLiteral("角平分构造线"),   "xline_bisector",  QStringLiteral("角平分构造线 (B)：选两条直线，沿其角平分方向无限延伸")},
        {QStringLiteral("平行构造线"),     "xline_parallel",  QStringLiteral("平行构造线 (O)：选参考直线后点通过点，平行于参考线无限延伸")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::circleSubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("圆心+点"),          "circle",              QStringLiteral("圆心 + 圆周点：半径 = 两点距离")},
        {QStringLiteral("两点画圆"),         "circle_2p",           QStringLiteral("两点为直径端点")},
        {QStringLiteral("两点+半径"),        "circle_2pr",          QStringLiteral("两点定义弦，参数输入条设半径，光标选近解")},
        {QStringLiteral("三点圆"),           "circle3p",            QStringLiteral("过任意三点作圆（外接圆）")},
        {QStringLiteral("圆心+半径"),        "circle_cr",           QStringLiteral("圆心 + 参数输入条半径值")},
        {QStringLiteral("相切两图元+点"),    "circle_t2e1p",        QStringLiteral("圆与两图元相切且过一点")},
        {QStringLiteral("相切+两点"),        "circle_t1e2p",        QStringLiteral("圆与一图元相切且过两点")},
        {QStringLiteral("相切两图元+半径"),  "circle_t2er",         QStringLiteral("圆与两图元相切，参数输入条设半径")},
        {QStringLiteral("相切三图元"),       "circle_t3e",          QStringLiteral("圆与三图元同时相切（阿波罗尼斯问题）")},
        {QStringLiteral("内切圆"),           "circle_inscribed",    QStringLiteral("三条直线的内切圆")},
        {QStringLiteral("同心圆"),           "circle_concentric",   QStringLiteral("取已有圆/弧圆心，参数输入条或点击设新半径")},
        {QStringLiteral("由弧转圆"),         "circle_byarc",        QStringLiteral("将圆弧转为同圆心同半径的完整圆")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::arcSubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("圆心+点+角度"),    "arc",              QStringLiteral("圆心 + 起点 + 终点（角度扫掠）")},
        {QStringLiteral("三点圆弧"),        "arc_3p",           QStringLiteral("起点 + 弧上点 + 终点，自动判定凹凸侧")},
        {QStringLiteral("圆心+点+弦长"),    "arc_cpc",          QStringLiteral("圆心 + 圆周点，参数输入条设弦长")},
        {QStringLiteral("圆心+点+弧角"),    "arc_cpaa",         QStringLiteral("圆心 + 半径，参数输入条设扫掠角")},
        {QStringLiteral("起终点+夹角"),     "arc_tpa",          QStringLiteral("两端点 + 参数输入条圆心角")},
        {QStringLiteral("起终点+半径"),     "arc_tpr",          QStringLiteral("两端点 + 参数半径，光标选近解")},
        {QStringLiteral("起终点+弦长"),     "arc_tpl",          QStringLiteral("两端点 + 参数弧长")},
        {QStringLiteral("起终点+弦高"),     "arc_tph",          QStringLiteral("两端点 + 凸度控制点（sagitta）")},
        {QStringLiteral("相切延续弧"),      "arc_tan",          QStringLiteral("选择参考图元，再点起点和终点，保持相切")},
        {QStringLiteral("椭圆弧(轴)"),      "arc_ea_axis",      QStringLiteral("中心 + 长轴端点 + 角度点")},
        {QStringLiteral("椭圆弧(1点)"),     "arc_ea_1pt",       QStringLiteral("中心 + 长轴端点 + 过点")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::polySubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("正多边形(内接)"),   "poly_regular",       QStringLiteral("正多边形(内接)：参数输入条设边数，点击圆心与圆周顶点")},
        {QStringLiteral("正多边形(外切)"),   "poly_circumscribed", QStringLiteral("正多边形(外切)：参数输入条设边数，点击圆心与边中点")},
        {QStringLiteral("正多边形(边长)"),   "poly_edge",          QStringLiteral("正多边形(边长)：参数输入条设边数，点击边的起点与终点")},
        {QStringLiteral("倒角矩形"),         "rect_chamfer",       QStringLiteral("倒角矩形：参数输入条设倒角距离，点击对角两点")},
        {QStringLiteral("圆角矩形"),         "rect_fillet",        QStringLiteral("圆角矩形：参数输入条设圆角半径，点击对角两点")},
        {QStringLiteral("星形"),             "poly_star",          QStringLiteral("星形：参数输入条设角数，点击圆心与外顶点")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::splineSubModes() {
    // 顺序与参考图（FreeCAD Spline 菜单）对齐；主按钮 spline 固定排第一。
    // tooltip 末尾附英文原名，便于与图中菜单项一一对照。
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("样条曲线"),   "spline",           QStringLiteral("样条曲线 (Spline)：过控制点的光滑曲线（Catmull-Rom 插值），右键结束")},
        {QStringLiteral("抛物线(4点)"),"spline_parabola_4p",QStringLiteral("抛物线 4 点 (Parabola 4 points)：依次点击两个端点和曲线上两个插值点，自动拟合二次贝塞尔抛物线")},
        {QStringLiteral("抛物线(焦点准线)"),"spline_parabola_fd", QStringLiteral("抛物线·焦点准线 (Parabola Focus Directrix)：依次点击焦点、准线上第1点、准线上第2点")},
        {QStringLiteral("双曲线"),     "spline_hyperbola", QStringLiteral("双曲线·离心点 (Hyperbola Ecci Point)：依次点击两个焦点和曲线上一点")},
        {QStringLiteral("抛物线(切线)"),"spline_parabola",  QStringLiteral("抛物线(切线式)：依次点击起点、切线控制点、终点（二次贝塞尔曲线）")},
        {QStringLiteral("样条过点"),   "spline_through_points", QStringLiteral("样条过点 (Spline through points)：逐点点击必经点，右键结束（Catmull-Rom 插值）")},
        {QStringLiteral("自由手绘线"), "freehand_line",    QStringLiteral("自由手绘线 (Freehand Line)：按住左键移动绘制，松开结束")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::ellipseSubModes() {
    // 顺序与参考图（FreeCAD Sketcher 的 Ellipse 菜单）逐项一致：
    //   Ellipse (1 Point) / Ellipse (Axis) / Ellipse Foci Point /
    //   Ellipse 4 Point / Ellipse Center and 3 Points / Ellipse Inscribed
    // tooltip 末尾附英文原名，便于与图中菜单项一一对照。
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("椭圆(1点)"),       "ellipse_1pt",      QStringLiteral("椭圆·1点 (Ellipse (1 Point))：点击中心，再点椭圆上一点（该点作为长轴端点，短轴取长轴一半）")},
        {QStringLiteral("椭圆(轴)"),        "ellipse_axis",     QStringLiteral("椭圆·轴 (Ellipse (Axis))：点击中心、长轴端点、椭圆上一点")},
        {QStringLiteral("椭圆(焦点)"),      "ellipse_foci",     QStringLiteral("椭圆·焦点 (Ellipse Foci Point)：依次点击两个焦点，再点椭圆上一点")},
        {QStringLiteral("椭圆(4点)"),       "ellipse_4p",       QStringLiteral("椭圆·4点 (Ellipse 4 Point)：依次点击椭圆上四个点，最小二乘拟合轴对齐椭圆")},
        {QStringLiteral("椭圆(中心+3点)"),  "ellipse_c3p",      QStringLiteral("椭圆·中心加三点 (Ellipse Center and 3 Points)：先点中心，再点椭圆上三个点，拟合旋转椭圆")},
        {QStringLiteral("椭圆(内切)"),      "ellipse_inscribed",QStringLiteral("椭圆·内切 (Ellipse Inscribed)：先选中一个矩形或多边形，生成其内切椭圆")},
    };
    return modes;
}

// 折线绘制 + 9 个编辑命令，顺序与参考图（FreeCAD Sketcher 的 Polyline 菜单）逐项一致。
const QVector<MainWindow::ToolSubMode>& MainWindow::polylineSubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("折线"),                "polyline",                   QStringLiteral("折线 (Polyline)：依次点击节点生成多段线，右键结束，点击首点附近可闭合")},
        {QStringLiteral("添加节点"),            "polyline_add_node",          QStringLiteral("折线·添加节点 (Add node)：选择折线并点击段上要插入的位置")},
        {QStringLiteral("追加节点"),            "polyline_append_node",       QStringLiteral("折线·追加节点 (Append node)：选择折线并点击要追加的位置（自动选最近的端点）")},
        {QStringLiteral("删除节点"),            "polyline_delete_node",       QStringLiteral("折线·删除节点 (Delete node)：选择折线并点击要删除的节点")},
        {QStringLiteral("删除两点之间"),        "polyline_delete_between",    QStringLiteral("折线·删除两点之间 (Delete between two nodes)：选择折线上的两个节点")},
        {QStringLiteral("修剪段"),              "polyline_trim_segments",     QStringLiteral("折线·修剪段 (Trim segments)：选择折线并点击要删除的段")},
        {QStringLiteral("创建等距折线"),        "polyline_equidistant",       QStringLiteral("折线·创建等距 (Create Equidistant Polylines)：选择源折线（距离 = 鼠标到质心）")},
        {QStringLiteral("从已有段生成"),        "polyline_from_segments",     QStringLiteral("折线·从已有段 (Polyline from Existing Segments)：依次选择线段/圆弧/折线合并为一条折线")},
        {QStringLiteral("弧段转弦"),            "polyline_arcs_to_chords",    QStringLiteral("折线·弧转弦 (Polyline Arcs to Chords)：将折线中所有弧段改为直线段")},
        {QStringLiteral("改变段类型"),          "polyline_change_segment_type", QStringLiteral("折线·改变段类型 (Polyline Change Segment Type)：选择两段切换其类型（直线↔弧）")},
    };
    return modes;
}

// 选择子模式，与附图 LibreCAD Select 菜单逐项对应（工具条「选择」下拉同源）：
//   窗口选择 / 窗口取消 / 全选 / 全部不选 /
//   选择实体 / 取消选择实体 / (取消)选择轮廓 / 选择相交 / 取消相交 / (取消)选择图层 / 反选
// 其中 全选/全部不选/反选 是一次性命令（instant=true，点击即执行、不作为工具停留）；
// 其余为点选/框选式工具模式，激活后等待用户在画布上操作。
const QVector<MainWindow::ToolSubMode>& MainWindow::selectSubModes() {
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("窗口选择"),        "select_window",        QStringLiteral("窗口选择 (Select Window)：拖拽矩形框选内部图元（Ctrl 加选）"), false, QStringLiteral("select_window")},
        {QStringLiteral("窗口取消"),        "deselect_window",      QStringLiteral("窗口取消 (Deselect Window)：拖拽矩形取消框内图元的选中"), false, QStringLiteral("deselect_window")},
        {QStringLiteral("全选"),            "select_all",           QStringLiteral("全选 (Select All)：选中所有可见图元"), true, QStringLiteral("select_all")},
        {QStringLiteral("全部不选"),        "deselect_all",         QStringLiteral("全部不选 (Deselect All)：清空当前选择"), true, QStringLiteral("deselect_all")},
        {QStringLiteral("选择实体"),        "select_entity",        QStringLiteral("选择实体 (Select Entity)：点击图元将其选中"), false, QStringLiteral("select_entity")},
        {QStringLiteral("取消选择实体"),    "deselect_entity",      QStringLiteral("取消选择实体 (Deselect Entity)：点击图元取消其选中"), false, QStringLiteral("deselect_entity")},
        {QStringLiteral("(取消)选择轮廓"),  "toggle_contour",       QStringLiteral("(取消)选择轮廓 ((De-)Select Contour)：点击一条边，轮廓已全选中则整条取消，否则整条选中"), false, QStringLiteral("toggle_contour")},
        {QStringLiteral("选择相交"),        "select_intersected",   QStringLiteral("选择相交 (Select Intersected Entities)：点击图元，选中所有与之相交的图元"), false, QStringLiteral("select_intersected")},
        {QStringLiteral("取消相交"),        "deselect_intersected", QStringLiteral("取消相交 (Deselect Intersected Entities)：点击图元，取消与之相交者的选中"), false, QStringLiteral("deselect_intersected")},
        {QStringLiteral("栏选"),            "fence",                QStringLiteral("栏选 (Select Fence)：画一条多段栏线，与栏线相交的图元全部加选"), false, QStringLiteral("fence")},
        {QStringLiteral("圈围"),            "wp",                   QStringLiteral("圈围 (Select WPolygon)：画一个多边形，完全位于内部的图元加选"), false, QStringLiteral("wp")},
        {QStringLiteral("圈交"),            "cp",                   QStringLiteral("圈交 (Select CPolygon)：画一个多边形，与多边形相交或在其内部的图元加选"), false, QStringLiteral("cp")},
        {QStringLiteral("(取消)选择图层"),  "toggle_layer",         QStringLiteral("(取消)选择图层 ((De-)Select Layer)：点击图元，同层已全选中则整层取消，否则整层选中"), false, QStringLiteral("toggle_layer")},
        {QStringLiteral("反选"),            "invert_selection",     QStringLiteral("反选 (Invert Selection)：选中所有当前未选中的可见图元"), true, QStringLiteral("invert_selection")},
    };
    return modes;
}

const QVector<MainWindow::ToolSubMode>& MainWindow::splineEditSubModes() {
    // 注意：主按钮（createDropdownTool 的 mainToolId）已对应 spline_from_polyline，
    // 故此处子项不再重复列出它。顺序与参考图 7~11 项一致。
    static const QVector<ToolSubMode> modes = {
        {QStringLiteral("追加样条点"),     "spline_append_pt",     QStringLiteral("追加样条点 (Append spline point)：选择样条并点击要追加的点")},
        {QStringLiteral("插入样条点"),     "spline_insert_pts",    QStringLiteral("插入样条点 (Insert spline points)：选择样条并点击段上位置")},
        {QStringLiteral("删除样条点"),     "spline_remove_pts",    QStringLiteral("删除样条点 (Remove spline points)：选择样条并点击要删除的控制点")},
        {QStringLiteral("删除两点之间"),   "spline_remove_between",QStringLiteral("删除两点之间 (Remove between two points)：两次点击两个控制点，删除中间段")},
        {QStringLiteral("分解样条为线段"), "spline_explode",       QStringLiteral("分解样条为线段 (Explode spline to lines)：将样条炸开为多条独立线段")},
    };
    return modes;
}

QStringList MainWindow::allSubModeToolIds() {
    // 单一真源：所有下拉/菜单子模式的 id 统一从这里汇总，供一致性测试校验
    // 每个 id 都在 ToolRegistry 有对应工具实现（instant 项直接调 model_，不纳入）。
    QSet<QString> ids;
    const QVector<const QVector<ToolSubMode>*> lists = {
        &lineSubModes(), &xlineSubModes(), &circleSubModes(), &arcSubModes(),
        &polySubModes(), &splineSubModes(), &splineEditSubModes(),
        &ellipseSubModes(), &polylineSubModes(), &selectSubModes(),
    };
    for (const QVector<ToolSubMode>* list : lists)
        for (const ToolSubMode& sm : *list)
            if (!sm.instant && !sm.id.isEmpty()) ids.insert(sm.id);
    return ids.values();
}

// 即时命令注册表（数据驱动分派）：instant 子模式的 id → 对 SketchModel 的一次性操作。
// 单一真源：新增即时命令只需在此加一条映射，createDropdownTool 的分派逻辑不用再动。
static const QHash<QString, void(*)(SketchModel*)>& instantCommandTable() {
    static const QHash<QString, void(*)(SketchModel*)> tbl = {
        {"select_all",       [](SketchModel* m){ m->selectAll(); }},
        {"deselect_all",     [](SketchModel* m){ m->clearSelection(); }},
        {"invert_selection", [](SketchModel* m){ m->invertSelection(); }},
    };
    return tbl;
}

void MainWindow::createDropdownTool(QToolBar* tb, QActionGroup* group, QList<QAction*>& bucket,
                                     const QString& icon, const QString& btnText,
                                     const QString& mainToolId, const QString& mainTip,
                                     const QString& shortcut, const QString& menuTitle,
                                     const QVector<ToolSubMode>& subModes) {
    // ---- 主按钮 ----
    auto* btn = new QToolButton(tb);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIcon(icons::make(icon));
    btn->setText(btnText);
    btn->setPopupMode(QToolButton::MenuButtonPopup);

    auto* mainAct = new QAction(icons::make(icon), btnText, this);
    mainAct->setCheckable(true);
    mainAct->setActionGroup(group);
    mainAct->setToolTip(shortcut.isEmpty() ? mainTip : QString("%1  [%2]").arg(mainTip, shortcut));
    mainAct->setStatusTip(mainAct->toolTip());
    if (!shortcut.isEmpty()) {
        mainAct->setShortcut(QKeySequence(shortcut));
        mainAct->setShortcutContext(Qt::ApplicationShortcut);
    }
    connect(mainAct, &QAction::triggered, this, [this, mainToolId] { model_->setActiveTool(mainToolId); });
    btn->setDefaultAction(mainAct);
    addAction(mainAct);
    toolActions_.insert(mainToolId, mainAct);
    mainAct->setObjectName(mainToolId);   // 供【绘图】菜单去重按 id 判定（不受子菜单覆盖 toolActions_[id] 影响）
    bucket << mainAct;

    // ---- 下拉子菜单 ----
    auto* menu = new QMenu(menuTitle, this);
    // 即时命令项（instant=true）点击后立即执行，不作为工具停留：
    // 不加入互斥组、不可高亮保持、不改变当前活动工具（否则会把 activeTool 置成无效 id）。
    // 分派走数据驱动注册表 instantCommandTable()：新增即时命令只需加一条 id→操作 映射，
    // 无需再改这里的 if 链。
    auto runInstant = [this](const QString& id) {
        const auto& tbl = instantCommandTable();
        auto it = tbl.constFind(id);
        if (it != tbl.constEnd()) it.value()(model_);
        else qWarning().noquote() << QStringLiteral("未知即时命令:") << id;
    };
    for (const auto& sm : subModes) {
        if (sm.instant) {
            auto* a = new QAction(sm.zh, this);
            if (!sm.icon.isEmpty()) a->setIcon(icons::make(sm.icon));
            a->setCheckable(false);
            a->setToolTip(sm.tip);
            a->setStatusTip(sm.tip);
            connect(a, &QAction::triggered, this, [runInstant, id = sm.id] { runInstant(id); });
            menu->addAction(a);
            addAction(a);
            continue;                          // 即时项不注册进 toolActions_（无工具状态可同步）
        }
        auto* a = new QAction(sm.zh, this);
        if (!sm.icon.isEmpty()) a->setIcon(icons::make(sm.icon));
        a->setCheckable(true);
        a->setActionGroup(group);
        a->setToolTip(sm.tip);
        a->setStatusTip(sm.tip);
        connect(a, &QAction::triggered, this, [this, id = sm.id] { model_->setActiveTool(id); });
        menu->addAction(a);
        addAction(a);
        toolActions_.insert(sm.id, a);
    }
    btn->setMenu(menu);
    tb->addWidget(btn);
}

// ---------------------------------------------------------------------------
// 工具条外观统一：两行共用同一套参数，保证「图标尺寸 / 按钮风格 / 内边距 / 间距 /
// 对齐方式」完全一致，视觉上像同一条工具条折成两行，而不是两条各自为政的工具条。
// ---------------------------------------------------------------------------
void MainWindow::applyToolBarLook(QToolBar* tb) {
    // 图标 + 文字并排：文字完整显示，不会被压成纯图标或被截断
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(20, 20));
    // 锁定在顶部工具条区：不可拖动/浮动，避免被拖到左右两侧竖排挤压，
    // 也避免浮动成独立窗口与画布、停靠面板重叠。
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setAllowedAreas(Qt::TopToolBarArea);
    tb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    if (QLayout* l = tb->layout()) {
        l->setContentsMargins(6, 2, 6, 2);
        l->setSpacing(2);
    }
    // 收紧按钮内边距：系统默认留白偏大（每个按钮 80~116px），24 个按钮会把两行
    // 都顶到 1360px 以上，窄屏（1280）仍会触发 ">>" 溢出按钮、尾部按钮被收起。
    // 这里只改 padding / spacing，不改配色与边框，图标和文字依旧完整显示。
    tb->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 4px; }"
        "QToolBar    { spacing: 2px; }"));
}

// 统一按钮最小宽度：短标签（1~2 个汉字，如「圆」「删除」「XY」）与长标签留白一致，
// 排在一起不会参差不齐；超过该宽度的按钮自然变宽，不会被截断。
void MainWindow::uniformButtonWidth(QToolBar* tb) {
    for (QToolButton* b : tb->findChildren<QToolButton*>()) {
        if (b->minimumWidth() < kToolButtonMinWidth)
            b->setMinimumWidth(kToolButtonMinWidth);
    }
}

void MainWindow::buildToolbar() {
    // ================= 第一行：草图绘制 / 标注 / 修改 =================
    QToolBar* tb = addToolBar(QStringLiteral("草图工具"));
    tb->setObjectName("mainToolBar");
    applyToolBarLook(tb);
    toolbar_ = tb;

    // 当前正在填充的工具条：第一行填完后切到第二行，
    // 下面所有 add/addEx/createDropdownTool/flush 都跟着它走，无需改调用点。
    QToolBar* curTb = tb;

    // 绘图模式类工具：进入对应绘图模式，互斥高亮
    auto* group = new QActionGroup(this);
    group->setExclusive(true);

    QList<QAction*> bucket;
    QString bucketTitle;
    // 收口当前分组：写入 drawGroups_ 并在工具条上补一个分隔符。
    // toMenu=false：仅上工具条、不进菜单（用于「修改」组——它已归到【编辑>修改】子菜单，
    // 绘图菜单若再平铺一次即成重复项，按去重规则移除）。
    auto flush = [&](bool sep = true, bool toMenu = true) {
        if (!bucket.isEmpty()) {
            if (toMenu) drawGroups_ << qMakePair(bucketTitle, bucket);
            bucket.clear();
        }
        if (sep) curTb->addSeparator();
    };
    auto beginGroup = [&](const QString& title) { bucketTitle = title; };

    // onBar=false：只注册动作与快捷键，不占用工具条横向空间（放进菜单子项里）
    auto addEx = [&](const QString& zh, const QString& en, const QString& tip,
                     const QString& key, const QString& icon, bool onBar, bool inBucket) {
        auto* a = new QAction(icons::make(icon), zh, this);
        a->setCheckable(true);
        a->setActionGroup(group);
        a->setToolTip(key.isEmpty() ? tip : QString("%1  [%2]").arg(tip, key));
        a->setStatusTip(a->toolTip());
        if (!key.isEmpty()) { a->setShortcut(QKeySequence(key)); a->setShortcutContext(Qt::ApplicationShortcut); }
        connect(a, &QAction::triggered, this, [this, en] { model_->setActiveTool(en); });
        if (onBar) curTb->addAction(a);
        else       addAction(a);          // 不上工具条也要挂到窗口上，否则快捷键不生效
        toolActions_.insert(en, a);
        a->setObjectName(en);             // 供【绘图】菜单去重按 id 判定
        if (inBucket) bucket << a;
        return a;
    };
    auto add = [&](const QString& zh, const QString& en, const QString& tip,
                   const QString& key, const QString& icon) {
        return addEx(zh, en, tip, key, icon, true, true);
    };

    beginGroup(QStringLiteral("选择"));
    // 主按钮 = 选择/移动工具（Esc），下拉菜单含窗口选择/取消、选择实体/轮廓/相交/图层等
    createDropdownTool(curTb, group, bucket, "select", QStringLiteral("选择"),
                       "select", QStringLiteral("选择 / 移动 (Select)：点击选择，拖拽平移，空白处框选"),
                       "Esc", QStringLiteral("选择菜单"), selectSubModes());
    if (QAction* sa = toolActions_.value("select")) sa->setChecked(true);  // 默认选择工具高亮
    flush();

    beginGroup(QStringLiteral("基本图元"));
    // 直线：主按钮 = 2 Points，下拉菜单包含所有绘制方式（工厂方法消除重复代码）
    createDropdownTool(curTb, group, bucket, "line", QStringLiteral("直线"),
                       "line", QStringLiteral("直线 (两点)：连续模式下右键结束"),
                       "L", QStringLiteral("直线绘制方式"), lineSubModes());
    // 构造线：主按钮 = 两点构造线，下拉菜单包含 H/V/A/B/O 六种绘制方式
    createDropdownTool(curTb, group, bucket, "xline", QStringLiteral("构造线"),
                       "xline", QStringLiteral("构造线 (Xline)：向两端无限延伸的辅助线，连续绘制，右键/Esc 退出"),
                       "", QStringLiteral("构造线绘制方式"), xlineSubModes());
    add(QStringLiteral("矩形"),   "rect", QStringLiteral("矩形 (Rectangle)"), "R", "rect");
    // 多边形：主按钮 = 自由多边形(右键闭合)，下拉菜单包含正多边形/倒角矩形/圆角矩形/星形等绘制方式
    createDropdownTool(curTb, group, bucket, "poly", QStringLiteral("多边形"),
                       "poly", QStringLiteral("多边形 (Polygon)：自由顶点，右键闭合"),
                       "P", QStringLiteral("多边形绘制方式"), polySubModes());
    flush();

    beginGroup(QStringLiteral("圆与圆弧"));
    // 圆：主按钮 = 圆心+点，下拉菜单包含所有12种绘制方式（工厂方法消除重复代码）
    createDropdownTool(curTb, group, bucket, "circle", QStringLiteral("圆"),
                       "circle", QStringLiteral("圆 (圆心+点)：点击圆心与圆周点"),
                       "C", QStringLiteral("圆形绘制方式"), circleSubModes());
    // 圆弧：主按钮 = 圆心+起点+终点，下拉菜单包含全部 11 种绘制方式
    createDropdownTool(curTb, group, bucket, "arc", QStringLiteral("圆弧"),
                       "arc", QStringLiteral("圆弧 (Arc)：圆心 + 起点 + 终点"),
                       "A", QStringLiteral("圆弧绘制方式"), arcSubModes());
    // 椭圆：主按钮 = 椭圆(1点)，下拉菜单包含参考图全部 6 种绘制方式
    createDropdownTool(curTb, group, bucket, "ellipse", QStringLiteral("椭圆"),
                       "ellipse_1pt", QStringLiteral("椭圆 (Ellipse (1 Point))：点击中心，再点椭圆上一点"),
                       "E", QStringLiteral("椭圆绘制方式"), ellipseSubModes());
    flush();

    // 折线：主按钮 = 绘制折线（依次点击节点），下拉菜单包含 9 个编辑命令
    createDropdownTool(curTb, group, bucket, "polyline", QStringLiteral("折线"),
                       "polyline", QStringLiteral("折线 (Polyline)：依次点击节点生成多段线，右键结束"),
                       "P", QStringLiteral("折线绘制与编辑"), polylineSubModes());
    flush();

    // 边界（BOUNDARY / BPOLY）：交互定义填充边界（外环 + 可选内孔），作为剖面填充的直接输入，
    // 解除剖面填充对内置「边界自动追踪算法」的单一依赖。BPOLY 为同义命令别名。
    createDropdownTool(curTb, group, bucket, "boundary", QStringLiteral("边界"),
                       "boundary", QStringLiteral("边界 (BOUNDARY)：依次点击节点定义封闭填充边界（可含内孔），生成边界图元供剖面填充直接复用，无需逐段描边"),
                       "", QStringLiteral("边界 (BOUNDARY/BPOLY)"), QVector<ToolSubMode>());
    flush();

    beginGroup(QStringLiteral("曲线"));
    // 样条：主按钮 = 自由样条(右键结束)，下拉菜单包含抛物线/双曲线/4点抛物线/焦点准线/过点/自由手绘等绘制方式
    createDropdownTool(curTb, group, bucket, "spline", QStringLiteral("样条"),
                       "spline", QStringLiteral("样条曲线 (Spline)：过控制点的光滑曲线，右键结束"),
                       "S", QStringLiteral("样条绘制方式"), splineSubModes());
    // 样条编辑：主按钮 = 由多段线创建，下拉菜单包含追加/插入/删除控制点/删除两段之间/分解
    createDropdownTool(curTb, group, bucket, "spline", QStringLiteral("样条编辑"),
                       "spline_from_polyline", QStringLiteral("样条编辑：由多段线创建 / 增删控制点 / 分解"),
                       "", QStringLiteral("样条编辑命令"), splineEditSubModes());
    flush();

    beginGroup(QStringLiteral("约束"));
    add(QStringLiteral("标注"), "dim",
        QStringLiteral("线性标注 (GB/T 4458.4)：两测量点 + 尺寸线位置"), "D", "dim");
    add(QStringLiteral("约束"), "constraint", QStringLiteral("几何约束 (Constraint)"), "N", "constraint");
    flush();

    // 文字与表格组：MTEXT 多行文字 + TABLE 明细表。不上工具条（第一行横向空间已满，
    // 见修改组注释），经【绘图>文字与表格】分组进入；点击画布后弹出对应编辑对话框。
    beginGroup(QStringLiteral("文字与表格"));
    addEx(QStringLiteral("多行文字"), "mtext",
          QStringLiteral("多行文字 (MTEXT)：点击插入点后编辑多行内容、对齐、样式、字段与自动换行"),
          "", "mtext", /*onBar=*/false, /*inBucket=*/true);
    addEx(QStringLiteral("表格"), "table",
          QStringLiteral("表格 (TABLE)：点击放置表格左上角后编辑行列、列宽、行高、字高与单元格内容（明细表）"),
          "", "table", /*onBar=*/false, /*inBucket=*/true);
    flush(/*sep=*/false, /*toMenu=*/true);

    // 其余标注类型不上工具条（横向空间有限），集中放进「草图绘制 > 尺寸标注」子菜单。
    // 它们与工具条上的「标注」同属一个互斥组，切换时高亮状态自动互斥。
    dimActs_.clear();
    dimActs_ << toolActions_.value("dim");
    dimActs_ << addEx(QStringLiteral("智能标注"), "dim_smart",
                      QStringLiteral("智能标注：先选中一条直线(→对齐标注)或一个圆(→直径标注)，一键生成，免逐点拾取"),
                      "Shift+S", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("对齐标注"), "dim_aligned",
                      QStringLiteral("对齐标注：尺寸线平行于两测量点连线，量真实距离"),
                      "Shift+D", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("水平标注"), "dim_h",
                      QStringLiteral("水平标注：只量 X 方向投影"), "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("竖直标注"), "dim_v",
                      QStringLiteral("竖直标注：只量 Y 方向投影"), "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("角度标注"), "dim_angle",
                      QStringLiteral("角度标注：角顶点 + 两条边上各一点，数字一律水平书写"),
                      "Shift+A", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("半径标注"), "dim_radius",
                      QStringLiteral("半径标注：点选圆/圆弧，点击方位即引出方向（R 前缀）"),
                      "Shift+R", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("直径标注"), "dim_diameter",
                      QStringLiteral("直径标注：点选圆/圆弧，尺寸线通过圆心（⌀ 前缀）"),
                      "Shift+I", "dim", false, false);
    // ---- GB/T 4458.4 / ISO 129 通用注法：基线 / 连续 / 弧长 / 引线 / 坐标 ----
    dimActs_ << addEx(QStringLiteral("基线标注"), "dim_baseline",
                      QStringLiteral("基线标注：自同一基准线起并行排列的一组尺寸"),
                      "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("连续标注"), "dim_continue",
                      QStringLiteral("连续标注：首尾相接的链式尺寸（首尾均为箭头）"),
                      "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("弧长标注"), "dim_arc",
                      QStringLiteral("弧长标注：点选圆弧，尺寸线为同心弧，数字带 ⌒ 前缀"),
                      "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("引线标注"), "dim_leader",
                      QStringLiteral("引线标注：连续点击折线点，末点为文字落点，用于文字/符号说明"),
                      "", "dim", false, false);
    dimActs_ << addEx(QStringLiteral("坐标标注"), "dim_ordinate",
                      QStringLiteral("坐标标注：折线引出 + X 或 Y 坐标值，用于孔系定位"),
                      "", "dim", false, false);

    // =================================================================
    // 修改组：工具条上只放一个「修改」下拉按钮，内含 修剪/延伸 + 旋转/镜像/
    // 偏移/分解 等一键命令。
    // 为什么不平铺按钮：第一行工具条在 1366px 宽屏下的自然宽度上限约 1358px
    // （见 tests/toolbar_layout_test.cpp 的回归断言）。把「旋转/镜像/偏移/分解」
    // 四个按钮平铺会把第一行从 ~1296px 撑到 1644px，窄屏出现 ">>" 溢出按钮。
    // 按 LibreCAD 的信息架构（Trim / Extend 本就属于 Modify 菜单），把它们与
    // 新的修改命令一起收进同一个下拉，功能不减、横向省下 ~260px。
    // =================================================================
    beginGroup(QStringLiteral("修改"));
    // 修剪 / 延伸：仍是互斥工具（快捷键 T / X），但不再单独占用工具条横向空间
    QAction* trimAct = addEx(QStringLiteral("修剪"), "trim",
        QStringLiteral("修剪 (Trim)：点击要剪掉的一段；Shift+点击可先指定边界图元"),
        "T", "trim", /*onBar=*/false, /*inBucket=*/true);
    QAction* extendAct = addEx(QStringLiteral("延伸"), "extend",
        QStringLiteral("延伸 (Extend)：目标直线 + 边界直线"),
        "X", "extend", /*onBar=*/false, /*inBucket=*/true);

    // 一键触发型修改命令：不是"切换工具"，而是点击后立即调用 modify 引擎，
    // 因此不加入互斥组（否则会把 activeTool 置成无效 id）。
    QList<QAction*> modifyActs;
    auto cmd = [&](const QString& zh, const QString& tip, const QString& icon, auto fn) {
        auto* a = new QAction(icons::make(icon), zh, this);
        a->setToolTip(tip);
        a->setStatusTip(a->toolTip());
        connect(a, &QAction::triggered, this, fn);
        addAction(a);            // 挂到窗口，便于下拉菜单与菜单栏复用同一指针
        modifyActs << a;
        bucket << a;
        return a;
    };
        cmd(QStringLiteral("旋转"), QStringLiteral("旋转 (Rotate)：选中图元 + 中心 + 角度"), "rotate",
            [this] {
                ModifyInputDialog dlg(ModifyInputDialog::M_Rotate, QStringLiteral("旋转"), -1, -1, this);
                if (dlg.exec() != QDialog::Accepted) return;
                auto r = dlg.result();
                std::vector<Element> src;
                for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
                if (src.empty()) { model_->log(QStringLiteral("无可旋转图元"), "warn"); return; }
                auto changes = modify::makeRotate(src, Vec3(r.cx, r.cy), r.angleDeg);
                model_->beginEdit(QStringLiteral("修改-旋转"));
                modify::applyChanges(model_, changes, QString());
                model_->log(QStringLiteral("已旋转 %1 个图元 %2°").arg(src.size()).arg(r.angleDeg, 0, 'f', 2));
            });
        cmd(QStringLiteral("镜像"), QStringLiteral("镜像 (Mirror)：选中图元 → 选基准(点/直线) → 生成镜像副本"), "mirror",
            [this] { mirror(); });
        cmd(QStringLiteral("偏移"), QStringLiteral("偏移 (Offset)：选中直线/圆/弧 + 偏移距离"), "offset",
            [this] {
                ModifyInputDialog dlg(ModifyInputDialog::M_Offset, QStringLiteral("偏移"), -1, -1, this);
                if (dlg.exec() != QDialog::Accepted) return;
                double d = dlg.result().distMm;
                std::vector<Element> src;
                for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
                if (src.empty()) { model_->log(QStringLiteral("无可偏移图元"), "warn"); return; }
                auto changes = modify::makeOffset(src, d);
                if (changes.empty()) { model_->log(QStringLiteral("偏移失败"), "warn"); return; }
                model_->beginEdit(QStringLiteral("修改-偏移"));
                auto added = modify::applyChanges(model_, changes, QString());
                model_->log(QStringLiteral("已偏移 %1 个图元（Δ=%2 mm）").arg(added.size()).arg(d, 0, 'f', 2));
            });
        cmd(QStringLiteral("分解"), QStringLiteral("分解 (Explode)：矩形/多边形/折线/样条 → 多段独立直线/圆弧"), "explode",
            [this] {
                std::vector<Element> src;
                for (int id : model_->selection()) if (const Element* e = model_->element(id)) src.push_back(*e);
                if (src.empty()) { model_->log(QStringLiteral("无可分解图元"), "warn"); return; }
                auto changes = modify::makeExplode(src);
                if (changes.empty()) { model_->log(QStringLiteral("无可分解对象"), "warn"); return; }
                model_->beginEdit(QStringLiteral("修改-分解"));
                auto added = modify::applyChanges(model_, changes, QString());
                QStringList ids;
                for (int id : added) ids << ("#" + QString::number(id));
                model_->log(QStringLiteral("已分解 %1 个图元 → %2 段：%3")
                                .arg(src.size()).arg(added.size()).arg(ids.join(", ")));
            });

    // ---- 工具条上的「修改」下拉按钮（修剪/延伸 + 上述一键命令）----
    auto* modifyBtn = new QToolButton(curTb);
    modifyBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    modifyBtn->setIcon(icons::make(QStringLiteral("modify")));
    modifyBtn->setText(QStringLiteral("修改"));
    modifyBtn->setPopupMode(QToolButton::InstantPopup);
    modifyBtn->setToolTip(QStringLiteral("修改 (Modify)：修剪 / 延伸 / 旋转 / 镜像 / 偏移 / 分解 …"));
    modifyBtn->setStatusTip(modifyBtn->toolTip());
    auto* modifyMenu = new QMenu(QStringLiteral("修改命令"), modifyBtn);
    modifyMenu->addAction(trimAct);
    modifyMenu->addAction(extendAct);
    modifyMenu->addSeparator();
    for (QAction* a : modifyActs) modifyMenu->addAction(a);
    modifyBtn->setMenu(modifyMenu);
    curTb->addWidget(modifyBtn);
    flush(/*sep=*/true, /*toMenu=*/false);   // 修改命令已归【编辑>修改】子菜单，绘图菜单不再平铺

    // =====================================================================
    // 第二行：编辑 / 样式 / 视图 / 工作平面
    // 原单行工具条按钮过多，窗口宽度不足时会出现"…"溢出按钮、文字被挤掉。
    // addToolBarBreak() 强制第二行另起一行，两行各自独立排版，
    // 单行按钮数减半 → 图标与文字都能完整显示。
    // =====================================================================
    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar* tb2 = addToolBar(QStringLiteral("样式与视图"));
    tb2->setObjectName("viewToolBar");
    applyToolBarLook(tb2);
    toolbar2_ = tb2;
    curTb = tb2;                 // 之后的所有 add/addWidget 都落到第二行

    // ---- 编辑 ----
    QList<QAction*> ebucket;
    QString etitle;
    auto eflush = [&](bool sep = true) {
        if (!ebucket.isEmpty()) { editGroups_ << qMakePair(etitle, ebucket); ebucket.clear(); }
        if (sep) curTb->addSeparator();
    };
    auto act = [&](const QString& zh, const QString& tip, const QString& key,
                   const QString& icon, auto fn) {
        auto* a = new QAction(icons::make(icon), zh, this);
        a->setToolTip(key.isEmpty() ? tip : QString("%1  [%2]").arg(tip, key));
        a->setStatusTip(a->toolTip());
        connect(a, &QAction::triggered, this, fn);
        curTb->addAction(a);
        ebucket << a;
        return a;
    };

    etitle = QStringLiteral("编辑");
    act(QStringLiteral("删除"), QStringLiteral("删除选中图元 (Delete)"), "Del", "delete",
        [this] { model_->deleteSelected(); });
    eflush();

    // ---- 样式区：线宽 / 颜色 / 连续直线 ----
    auto* wLabel = new QLabel(QStringLiteral(" 线宽 "), curTb);
    curTb->addWidget(wLabel);
    widthBox_ = new QComboBox(curTb);
    widthBox_->setIconSize(QSize(20, 14));
    widthBox_->setMinimumContentsLength(4);
    // 下拉框里只显示 "1px/2px/3px/5px"（条形图标已直观表达粗细，完整中文名放 tooltip），
    // 否则最长的"特粗线 5px"会把这个下拉框撑到 138px，第二行总宽随之溢出。
    for (double px : {1.0, 2.0, 3.0, 5.0})
        widthBox_->addItem(icons::widthBar(px), QStringLiteral("%1px").arg(px, 0, 'f', 0), px);
    widthBox_->setToolTip(QStringLiteral(
        "线宽（细线 1px / 中粗线 2px / 粗线 3px / 特粗线 5px）："
        "未选中图元时设定新图元线宽；已选中时同时修改选中图元"));
    connect(widthBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i >= 0) applyWidth(widthBox_->itemData(i).toDouble());
    });
    curTb->addWidget(widthBox_);

    // ---- 线型下拉 ----
    styleBox_ = new QComboBox(curTb);
    styleBox_->setIconSize(QSize(32, 14));
    styleBox_->setMinimumContentsLength(3);
    for (int i = 0; i < kLineStyleCount; ++i) {
        LineStyle ls = (LineStyle)i;
        styleBox_->addItem(icons::lineStyleIcon(ls),
                           QString::fromUtf8(lineStyleName(ls)), i);
    }
    styleBox_->setToolTip(QStringLiteral("线型：未选中图元时设定新图元线型；已选中时同时修改选中图元"));
    connect(styleBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i) { if (i >= 0) applyLineStyle(i); });
    curTb->addWidget(styleBox_);

    // ---- 标注标准切换：GB/T 4458 ↔ ISO 129 ----
    auto* stdLabel = new QLabel(QStringLiteral(" 标准 "), curTb);
    stdLabel->setToolTip(QStringLiteral("切换尺寸标注遵循的制图标准：箭头形式 / 文字方向 / 公差规则随之改变"));
    curTb->addWidget(stdLabel);
    stdBox_ = new QComboBox(curTb);
    stdBox_->setMinimumContentsLength(7);
    // 条目文字精简（"国标/国际"的说明放进 tooltip），避免第二行被这两个宽条目挤爆
    stdBox_->addItem(QStringLiteral("GB/T 4458"), (int)DimensionStandard::GBT);
    stdBox_->addItem(QStringLiteral("ISO 129"),   (int)DimensionStandard::ISO);
    stdBox_->setCurrentIndex((model_->dimensionStandard() == DimensionStandard::ISO) ? 1 : 0);
    stdBox_->setToolTip(QStringLiteral("标注标准（GB/T 4458 国标 / ISO 129 国际）："
                                       "切换后已有标注按所选标准的箭头/文字方向/公差规则自动重绘"));
    connect(stdBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i >= 0) model_->setDimensionStandard((DimensionStandard)stdBox_->itemData(i).toInt());
    });
    curTb->addWidget(stdBox_);

    colorBtn_ = new QToolButton(curTb);
    colorBtn_->setText(QStringLiteral("颜色"));
    colorBtn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    colorBtn_->setPopupMode(QToolButton::InstantPopup);
    colorBtn_->setToolTip(QStringLiteral("图元颜色：预设色板 + 自定义取色；已选中图元时同时改其颜色"));
    colorMenu_ = new QMenu(QStringLiteral("颜色"), this);
    colorMenu_->setIcon(icons::make("color"));
    for (const auto& pc : presetColors()) {
        QAction* a = colorMenu_->addAction(icons::swatch(pc.second), pc.first);
        a->setCheckable(true);
        a->setData(pc.second);
        const QColor c = pc.second;
        connect(a, &QAction::triggered, this, [this, c] { applyColor(c); });
        colorActs_ << a;
    }
    colorMenu_->addSeparator();
    QAction* custom = colorMenu_->addAction(icons::make("color"), QStringLiteral("自定义颜色..."));
    connect(custom, &QAction::triggered, this, &MainWindow::pickCustomColor);
    colorBtn_->setMenu(colorMenu_);
    colorBtn_->setIcon(icons::swatch(model_->defaultColor()));
    curTb->addWidget(colorBtn_);

    actChain_ = new QAction(icons::make("chain"), QStringLiteral("连续"), this);
    actChain_->setCheckable(true);
    actChain_->setChecked(model_->chainLine());
    actChain_->setToolTip(QStringLiteral("连续直线模式：上一段终点自动作为下一段起点，右键结束  [K]"));
    actChain_->setStatusTip(actChain_->toolTip());
    actChain_->setShortcut(QKeySequence("K"));
    actChain_->setShortcutContext(Qt::ApplicationShortcut);
    connect(actChain_, &QAction::toggled, this, [this](bool on) { model_->setChainLine(on); });
    curTb->addAction(actChain_);
    curTb->addSeparator();

    // 视图类为即时命令（不改变绘图模式，因此不入互斥组）。
    // 第二行空间充裕，这里与绘图工具一样显示「图标 + 文字」，不再压成纯图标。
    etitle = QStringLiteral("视图");
    act(QStringLiteral("网格"), QStringLiteral("栅格显示开/关 (Grid)"), "F7", "grid",
        [this] { model_->setShowGrid(!model_->showGrid()); });
    act(QStringLiteral("放大"), QStringLiteral("放大 (Zoom In)"), "Ctrl++", "zoomin",
        [this] { view_->zoomBy(1.2); });
    act(QStringLiteral("缩小"), QStringLiteral("缩小 (Zoom Out)"), "Ctrl+-", "zoomout",
        [this] { view_->zoomBy(1.0 / 1.2); });
    act(QStringLiteral("适配"), QStringLiteral("适配窗口 (Fit)"), "Ctrl+0", "fit",
        [this] { view_->fitView(); });
    act(QStringLiteral("窗口缩放"), QStringLiteral("窗口缩放：拖拽框选目标区域，释放后该区域充满整个绘图工作区域 (Zoom to Box)"), "Ctrl+9", "zoombox",
        [this] { view_->setZoomBoxMode(true); });
    // 测量查询：只读几何查询面板入口（与【测量查询】菜单共用 openInquiry()）
    act(QStringLiteral("测量查询"), QStringLiteral("测量查询 (Inquiry)：距离 / 面积 / 质量特性 / 点坐标"), "", "query",
        [this] { openInquiry(); });
    // 打印预览：页面设置 / 简化 CTB 样式 / 视口布局 / PDF 入口（与【打印】菜单共用 openPrintPreview()）
    act(QStringLiteral("打印"), QStringLiteral("打印预览 (Print)：页面设置 / CTB 样式 / 视口 / PDF"), "", "print",
        [this] { openPrintPreview(); });
    eflush();

    etitle = QStringLiteral("工作平面");
    act(QStringLiteral("XY"), QStringLiteral("切换到 XY 工作平面（俯视）"), "Ctrl+1", "plane_xy",
        [this] { model_->setPlane(Plane::XY); });
    act(QStringLiteral("YZ"), QStringLiteral("切换到 YZ 工作平面（左视）"), "Ctrl+2", "plane_yz",
        [this] { model_->setPlane(Plane::YZ); });
    act(QStringLiteral("XZ"), QStringLiteral("切换到 XZ 工作平面（主视）"), "Ctrl+3", "plane_xz",
        [this] { model_->setPlane(Plane::XZ); });
    eflush(false);

    // 两行都统一按钮最小宽度，保证短标签（圆/删除/XY…）对齐一致
    uniformButtonWidth(tb);
    uniformButtonWidth(tb2);

    syncStyleWidgets(model_->defaultColor(), model_->defaultWidth());
}

// ---- 样式应用：绘制前设默认值；若当前有选中图元，则一并改选中图元 ----
void MainWindow::applyWidth(double px) {
    model_->setDefaultWidth(px);
    const QString name = QString::fromUtf8(lineWidthName(px));
    if (model_->selectedId() >= 0 && model_->styleSelected(nullptr, &px))
        model_->log(QStringLiteral("选中图元线宽 -> %1").arg(name));
    else
        model_->log(QStringLiteral("新图元线宽 -> %1").arg(name));
}

void MainWindow::applyLineStyle(int idx) {
    if (idx < 0 || idx > 4) return;
    const LineStyle ls = (LineStyle)idx;
    model_->setDefaultLineStyle(ls);
    const QString name = QString::fromUtf8(lineStyleName(ls));
    if (model_->selectedId() >= 0 && model_->styleSelectedEx(nullptr, nullptr, &ls))
        model_->log(QStringLiteral("选中图元线型 -> %1").arg(name));
    else
        model_->log(QStringLiteral("新图元线型 -> %1").arg(name));
}

void MainWindow::applyColor(const QColor& c) {
    if (!c.isValid()) return;
    model_->setDefaultColor(c);
    if (model_->selectedId() >= 0 && model_->styleSelected(&c, nullptr))
        model_->log(QStringLiteral("选中图元颜色 -> %1").arg(c.name().toUpper()));
    else
        model_->log(QStringLiteral("新图元颜色 -> %1").arg(c.name().toUpper()));
}

void MainWindow::pickCustomColor() {
    QColor c = QColorDialog::getColor(model_->defaultColor(), this,
                                      QStringLiteral("自定义图元颜色"));
    if (c.isValid()) applyColor(c);
}

// 模型是样式的唯一真源，这里只做「模型 -> 控件」的单向回显；
// 全程屏蔽控件信号，避免回显再次触发 applyWidth/applyColor 形成回环。
void MainWindow::syncStyleWidgets(const QColor& c, double px) {
    if (widthBox_) {
        QSignalBlocker b(widthBox_);
        int idx = widthBox_->findData(px);
        if (idx >= 0) widthBox_->setCurrentIndex(idx);
    }
    for (QAction* a : widthActs_) {
        QSignalBlocker b(a);
        a->setChecked(qFuzzyCompare(a->data().toDouble() + 1.0, px + 1.0));
    }
    if (colorBtn_) colorBtn_->setIcon(icons::swatch(c));
    for (QAction* a : colorActs_) {
        QSignalBlocker b(a);
        a->setChecked(a->data().value<QColor>() == c);
    }
}

// 线型变更信号 → 同步工具条与参数输入条的下拉框回显
// 全程屏蔽控件信号，避免回显再次触发 applyLineStyle 形成回环。
void MainWindow::syncLineStyleWidgets(int styleIdx) {
    if (styleBox_) {
        QSignalBlocker b(styleBox_);
        if (styleBox_->currentIndex() != styleIdx)
            styleBox_->setCurrentIndex(styleIdx);
    }
    if (paramStyleBox_) {
        QSignalBlocker b(paramStyleBox_);
        if (paramStyleBox_->currentIndex() != styleIdx)
            paramStyleBox_->setCurrentIndex(styleIdx);
    }
}

void MainWindow::buildDocks() {
    bomDock_ = new QDockWidget(QStringLiteral("BOM 树"), this);
    bomDock_->setObjectName("dockBom");
    bomDock_->setWidget(bom_);

    elemDock_ = new QDockWidget(QStringLiteral("图元面板"), this);
    elemDock_->setObjectName("dockElement");
    elemDock_->setWidget(elemPanel_);

    propDock_ = new QDockWidget(QStringLiteral("属性面板"), this);
    propDock_->setObjectName("dockProperty");
    propDock_->setWidget(propPanel_);

    logDock_ = new QDockWidget(QStringLiteral("执行信息"), this);
    logDock_->setObjectName("dockLog");
    logDock_->setWidget(logPanel_);

    auto* cmdWidget = new QWidget(this);
    auto* cl = new QHBoxLayout(cmdWidget);
    cl->setContentsMargins(6, 2, 6, 2);
    cl->addWidget(new QLabel(QStringLiteral("命令>")));
    cl->addWidget(cmdLine_);
    cmdWidget->setLayout(cl);
    cmdDock_ = new QDockWidget(QStringLiteral("命令行"), this);
    cmdDock_->setObjectName("dockCommand");
    cmdDock_->setWidget(cmdWidget);

    // 图层管理面板（新建/删除/改名/显隐/批量改属性/把选中图元移入图层）
    layerPanel_ = new LayerPanel(model_, this);
    layerDock_ = new QDockWidget(QStringLiteral("图层"), this);
    layerDock_->setObjectName("dockLayer");
    layerDock_->setWidget(layerPanel_);

    // 约束栏面板（需求③④：展示选中图元的约束与状态；删除/定位约束）
    constraintBar_ = new ConstraintBar(model_, this);
    constraintDock_ = new QDockWidget(QStringLiteral("约束栏"), this);
    constraintDock_->setObjectName("dockConstraint");
    constraintDock_->setWidget(constraintBar_);

    // 设计中心（符号库浏览/插入）：当前图块表 + 外部文件块表；双击块在视图中心插入
    designPanel_ = new DesignCenterPanel(model_, this);
    designDock_ = new QDockWidget(QStringLiteral("设计中心"), this);
    designDock_->setObjectName("dockDesignCenter");
    designDock_->setWidget(designPanel_);

    // 左栏：上 BOM 树 / 中 图元面板 / 下 图层
    addDockWidget(Qt::LeftDockWidgetArea, bomDock_);
    splitDockWidget(bomDock_, elemDock_, Qt::Vertical);
    splitDockWidget(elemDock_, layerDock_, Qt::Vertical);
    // 右栏：上 属性面板 / 中 约束栏 / 下 执行信息
    addDockWidget(Qt::RightDockWidgetArea, propDock_);
    splitDockWidget(propDock_, constraintDock_, Qt::Vertical);
    splitDockWidget(constraintDock_, logDock_, Qt::Vertical);
    // 设计中心：右侧独立停靠（初始隐藏，可从「视图 > 面板」勾出）
    addDockWidget(Qt::RightDockWidgetArea, designDock_);
    // 底部：命令行
    addDockWidget(Qt::BottomDockWidgetArea, cmdDock_);

    // 全部面板可拖拽移动、浮动、关闭（关闭后可从「视图 > 面板」重新勾选显示）
    for (auto* d : {bomDock_, elemDock_, propDock_, logDock_, cmdDock_, layerDock_, constraintDock_, designDock_})
        d->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                       QDockWidget::DockWidgetFloatable);
    designDock_->setVisible(false);   // 初始隐藏，按需勾出（避免首次启动过于拥挤）

    // 初始分栏比例：左 240 / 右 300；左栏上下各分
    resizeDocks({bomDock_, propDock_}, {240, 300}, Qt::Horizontal);
    resizeDocks({bomDock_, elemDock_, layerDock_}, {300, 200, 160}, Qt::Vertical);
    resizeDocks({propDock_, constraintDock_, logDock_}, {320, 200, 280}, Qt::Vertical);

    // 设计中心：双击块 → 在视图中心插入引用（外部块已由面板并入本图块表）
    connect(designPanel_, &DesignCenterPanel::insertBlockRequested,
            this, &MainWindow::insertBlockAtViewCenter);
}

void MainWindow::connectSignals() {
    // 模型↔视图↔面板↔状态栏↔标尺↔命令行 的全部信号接线收口到 ModelMediator（中介者，
    // 单一触发时机）：谁订阅谁、触发几次只在此定义，杜绝"面板自连接 vs 主窗口连接"的
    // 双份/遗漏。仅需触碰主窗口私有成员的反应以回调注入，纯组件联动由中介者直接接线。
    // 多文档：中介者成为成员，切文档时 rewire() 换绑活动文档的模型/视图/标尺。
    mediator_ = new ModelMediator(this, {
        model_, view_, logPanel_, elemPanel_, bom_, propPanel_,
        statusBar_, hRuler_, vRuler_, cmdLine_
    });
    mediator_->setHost({
        /* onActiveToolChanged */ [this](const QString& t) {
            if (QAction* a = toolActions_.value(t, nullptr)) a->setChecked(true);
            else if (QAction* s = toolActions_.value("select", nullptr)) s->setChecked(true);
            updateParamBarForTool(t);
        },
        /* onDefaultStyleChanged */ [this](const QColor& c, double w) { syncStyleWidgets(c, w); },
        /* onDefaultLineStyleChanged */ [this](int s) {
            syncLineStyleWidgets(s);
            syncStyleWidgets(model_->defaultColor(), model_->defaultWidth());
        },
        /* onScaleChanged */ [this] { syncTitleBlockScale(); },
        /* onChainToggled */ [this](bool on) {
            if (actChain_) { QSignalBlocker b(actChain_); actChain_->setChecked(on); }
        },
    });

    // ---- 状态栏可点击开关组：按钮点击 → 回写模型 setter（一次性；信号侧回显随文档换绑）----
    connect(statusBar_, &StatusBar::toggleRequested, this,
            [this](StatusBar::Toggle key, bool on) {
        switch (key) {
        case StatusBar::Toggle::Ortho: model_->setOrthoMode(on); break;
        case StatusBar::Toggle::Snap:  model_->setGridSnap(on); break;
        case StatusBar::Toggle::Polar:
        case StatusBar::Toggle::Track: {
            auto cfg = model_->trackConfig();       // 就地改单字段，其余保持
            if (key == StatusBar::Toggle::Polar) cfg.polar = on; else cfg.objectTrack = on;
            model_->setTrackConfig(cfg);
            break;
        }
        case StatusBar::Toggle::Lwt:   model_->setShowLineWeight(on); break;
        }
    });

    // 约束栏交互 → 视图定位 / 模型删除约束（发送方是常驻面板，lambda 内的 model_ 随切换换绑）
    connect(constraintBar_, &ConstraintBar::locateRequested, this,
            [this](int id) { view_->zoomToElement(id); });
    connect(constraintBar_, &ConstraintBar::deleteRequested, this,
            [this](int idx) { model_->removeConstraint(idx); });

    // 脏标记 → 标签文本刷新（DocSession 置脏/复位 → 对应下标的标签重绘 "*"）
    connect(docs_, &DocManager::dirtyChanged, this, [this](int i) { syncTabTitle(i); });
    connect(docs_, &DocManager::docAdded, this, [this](int i) { syncTabTitle(i); });

    // Ctrl+回车：把焦点交给命令行，便于纯键盘连续作业
    auto* focusCmd = new QAction(this);
    focusCmd->setShortcut(QKeySequence("Ctrl+Return"));
    focusCmd->setShortcutContext(Qt::ApplicationShortcut);
    connect(focusCmd, &QAction::triggered, this, [this] {
        cmdDock_->show();
        cmdLine_->setFocus(Qt::ShortcutFocusReason);
    });
    addAction(focusCmd);

    // 活动文档接线（模型/视图信号 + 消费者重绑 + 状态回显）——切文档时再次进入
    hookActiveDoc();
}

// ============================================================================
// 多文档切换：活动文档接线 + 状态回显
// ----------------------------------------------------------------------------
// 单一职责：把「model_/view_/标尺」指向活动文档，断开旧文档的全部信号连接，
// 重绑所有持有模型指针的消费者，并把活动文档状态回显到常驻 UI（工具条/参数条/
// 状态栏/开关组）。连接清单 modelConns_/viewConns_ 保证"断多少、重接多少"对称。
// ============================================================================
void MainWindow::hookActiveDoc() {
    for (const auto& c : modelConns_) disconnect(c);
    for (const auto& c : viewConns_)  disconnect(c);
    modelConns_.clear();
    viewConns_.clear();

    // ---- 中介者重绑：model/view/标尺换活动文档的，共享面板不变 ----
    mediator_->rewire({ model_, view_, logPanel_, elemPanel_, bom_, propPanel_,
                        statusBar_, hRuler_, vRuler_, cmdLine_ });

    // 应用习惯会话态重放：新活动文档的模型把 AppSettings 里的空闲态右键行为
    // 带上（每文档模型独立一份会话态，切标签/新建文档后保持用户设置一致）。
    if (appSettings_)
        model_->setRightClickIdleRepeat(
            appSettings_->value(QLatin1String(SettingKeys::kRightClickIdleRepeat)).toBool());

    // ---- 单位（UNITS）：状态栏坐标口径与参数条后缀/显示值实时跟随 ----
    modelConns_ << connect(model_, &SketchModel::unitsChanged, this, [this] {
        statusBar_->setUnitsMeta(model_->units());
        applyUnitsToParamBar();
    });

    // ---- 状态栏开关组 / 快捷键三入口的信号回显 ----
    modelConns_ << connect(model_, &SketchModel::orthoModeChanged, this,
            [this](bool on) { statusBar_->setToggleState(StatusBar::Toggle::Ortho, on); });
    modelConns_ << connect(model_, &SketchModel::gridSnapChanged, this,
            [this] { statusBar_->setToggleState(StatusBar::Toggle::Snap, model_->gridSnap()); });
    modelConns_ << connect(model_, &SketchModel::trackConfigChanged, this, [this] {
        statusBar_->setToggleState(StatusBar::Toggle::Polar, model_->trackConfig().polar);
        statusBar_->setToggleState(StatusBar::Toggle::Track, model_->trackConfig().objectTrack);
    });
    modelConns_ << connect(model_, &SketchModel::showLineWeightChanged, this,
            [this](bool on) { statusBar_->setToggleState(StatusBar::Toggle::Lwt, on); });

    // ---- 选项提示区：分段命令的可选项关键字（如 [C/闭合 U/放弃]）----
    modelConns_ << connect(model_, &SketchModel::commandOptionsChanged, this,
            [this](const QStringList& opts) { statusBar_->setOptions(opts); });

    // ---- 选中对象即弹出属性面板（门控：仅选择工具态弹出，避免绘制中反复抢屏）----
    modelConns_ << connect(model_, &SketchModel::selectionChanged, this, [this](int id) {
        if (id < 0 || model_->activeTool() != "select") return;
        propDock_->show();
        propDock_->raise();
    });

    // ---- 约束可视化联动 ----
    modelConns_ << connect(model_, &SketchModel::constraintsChanged, constraintBar_, &ConstraintBar::refresh);
    modelConns_ << connect(model_, &SketchModel::constraintsChanged, view_, &GraphicsView::refreshConstraintDiagnostics);
    modelConns_ << connect(model_, &SketchModel::selectionChanged, constraintBar_, &ConstraintBar::refresh);
    modelConns_ << connect(model_, &SketchModel::modelChanged, constraintBar_, &ConstraintBar::refresh);

    // ---- 追踪/栅格菜单勾选态随活动文档同步 ----
    modelConns_ << connect(model_, &SketchModel::trackConfigChanged, this, [this] {
        if (aPolar_)  aPolar_->setChecked(model_->trackConfig().polar);
        if (aOtrack_) aOtrack_->setChecked(model_->trackConfig().objectTrack);
    });
    if (aGrid_)
        modelConns_ << connect(model_, &SketchModel::gridToggled, aGrid_, &QAction::setChecked);

    // ---- 状态栏「正交/捕捉」按钮勾选态（按钮常驻，连接随文档换）----
    if (btnOrtho_)
        modelConns_ << connect(model_, &SketchModel::orthoModeChanged, this,
                [this] { btnOrtho_->setChecked(model_->orthoMode()); });
    if (btnSnap_)
        modelConns_ << connect(model_, &SketchModel::gridSnapChanged, this,
                [this] { btnSnap_->setChecked(model_->gridSnap()); });

    // ---- 自动保存：模型变化 → 累计操作（只盯活动文档）----
    modelConns_ << connect(model_, &SketchModel::modelChanged, this, [this]{
        if (recovering_) return;      // 恢复读档过程中的联动刷新不算用户操作
        if (autoSave_) autoSave_->noteOperation();
    });

    // ---- 视图信号（每文档一个视图，随页栈切换）----
    // P0-04 数据面板：场景项数变化（打开/裁剪补建/批量同步）时刷新「场景项 M / 打开 Xms」
    viewConns_ << connect(view_, &GraphicsView::sceneStatsChanged, this, [this] {
        statusBar_->setLoadStats((int)model_->elements().size(), view_->sceneItemCount(), lastOpenMs_);
    });
    viewConns_ << connect(view_, &GraphicsView::requestShowProperties, this, [this] {
        propDock_->show();
        propDock_->raise();
    });
    viewConns_ << connect(view_, &GraphicsView::regionPickRequested, this, &MainWindow::onRegionPickRequested);
    viewConns_ << connect(view_, &GraphicsView::regionHoverMoved, this, &MainWindow::onRegionHover);
    viewConns_ << connect(view_, &GraphicsView::regionPickCancelled, this, &MainWindow::onRegionPickCancelled);
    viewConns_ << connect(view_, &GraphicsView::regionPickParamsRequested, this, &MainWindow::onRegionPickParams);
    viewConns_ << connect(view_, &GraphicsView::canvasPointPicked, this,
            [this](double x, double y) { onCanvasPointPicked(Vec3(x, y, 0)); });
    viewConns_ << connect(view_, &GraphicsView::pointPickCancelled, this, &MainWindow::onPointPickCancelled);
    viewConns_ << connect(view_, &GraphicsView::editTextRequested, this, &MainWindow::editElementText);
    // 空闲态右键 = 重复上一命令（AutoCAD 约定；开关 interaction/rightClickIdleRepeat，
    // 经 SketchModel::rightClickIdleRepeat 会话态传递，GraphicsView 侧判定后才发本信号）。
    // 走 CommandLine::repeatLastCommand（与空输入回车/空格重复同构，历史游标保持同步）；
    // 无上一命令时回显提示。
    viewConns_ << connect(view_, &GraphicsView::repeatLastCommandRequested, this, [this] {
        if (interp_->lastCommand().isEmpty())
            model_->log(QStringLiteral("没有可重复的命令"), "info");
        else
            cmdLine_->repeatLastCommand();
    });
    // 双击块引用 = 进入在位编辑（BEDIT，改定义 → 全部引用同步）
    viewConns_ << connect(view_, &GraphicsView::blockEditRequested, this, [this](int id) {
        const Element* e = model_->element(id);
        if (!e || e->type != ElementType::BlockRef || e->blockName.isEmpty()) return;
        openBlockEditorDialog(e->blockName);
    });

    // ---- 消费者重绑（持有模型指针的共享组件随活动文档换绑）----
    if (auto* cur = docs_->active()) {
        interp_->setHistory(cur->cmdHistory);       // 命令行历史/lastCommand 属会话态
        interp_->setLastCommand(cur->lastCommand);
    }
    interp_->setModel(model_);
    cmdLine_->setModel(model_);
    bom_->setModel(model_);
    propPanel_->setModel(model_);
    layerPanel_->setModel(model_);
    constraintBar_->setModel(model_);
    scriptHost_->setModel(model_);
    pluginManager_->setModel(model_);
    if (autoSave_) autoSave_->setCurrentDrawing(docs_->active() ? docs_->active()->filePath : QString());

    // ---- 活动文档状态回显到常驻 UI ----
    const QString tool = model_->activeTool();
    if (QAction* a = toolActions_.value(tool, nullptr)) a->setChecked(true);
    else if (QAction* s = toolActions_.value("select", nullptr)) s->setChecked(true);
    updateParamBarForTool(tool);
    syncStyleWidgets(model_->defaultColor(), model_->defaultWidth());
    syncLineStyleWidgets(int(model_->defaultLineStyle()));
    if (actChain_) { QSignalBlocker b(actChain_); actChain_->setChecked(model_->chainLine()); }
    statusBar_->setUnitsMeta(model_->units());
    applyUnitsToParamBar();
    statusBar_->setToggleState(StatusBar::Toggle::Ortho, model_->orthoMode());
    statusBar_->setToggleState(StatusBar::Toggle::Snap,  model_->gridSnap());
    statusBar_->setToggleState(StatusBar::Toggle::Polar, model_->trackConfig().polar);
    statusBar_->setToggleState(StatusBar::Toggle::Track, model_->trackConfig().objectTrack);
    statusBar_->setToggleState(StatusBar::Toggle::Lwt,   model_->showLineWeight());
    statusBar_->setOptions(model_->commandOptions());
    statusBar_->setCount(model_->elementCount());
    statusBar_->setDrawScale(model_->scaleLabel());
    statusBar_->setPlaneInfo(QString::fromLatin1(planeName(model_->plane())));
    // UCS 指示器由 mediator->rewire() 末尾回显；工具条高亮由 activeToolChanged 路径覆盖。
}

// ============================================================================
// 多文档区构建：标签条 + 页栈 + 首页（中央部件在此组装）
// ============================================================================
void MainWindow::buildDocArea() {
    docTabs_ = new QTabBar(this);
    docTabs_->setObjectName(QStringLiteral("docTabs"));
    docTabs_->setMovable(true);            // 拖拽标签重排
    docTabs_->setTabsClosable(true);       // 关闭按钮（脏文档走「保存/不保存/取消」）
    docTabs_->setExpanding(false);
    connect(docTabs_, &QTabBar::currentChanged, this,
            [this](int idx) { switchToDoc(idx); });
    connect(docTabs_, &QTabBar::tabCloseRequested, this, [this](int idx) {
        if (confirmCloseDoc(idx)) closeDocAt(idx);
    });
    connect(docTabs_, &QTabBar::tabMoved, this, [this](int from, int to) {
        docs_->reorder(from, to);          // 会话顺序重排（活动身份跟随会话，不随下标漂移）
        syncTabTitle(docs_->activeIndex());
    });
    // Ctrl+Tab 循环切换（应用级快捷键，命令行/面板聚焦时同样生效）
    auto* cyc = new QShortcut(QKeySequence("Ctrl+Tab"), this);
    cyc->setContext(Qt::ApplicationShortcut);
    connect(cyc, &QShortcut::activated, this,
            [this] { if (docs_->count() > 1) switchToDoc(docs_->cycle(1)); });

    viewStack_ = new QStackedWidget(this);

    // 首页（构造期唯一一次；后续每文档一页走 makeDocPage）
    const DocPage pg = makeDocPage(docs_->doc(0));
    view_ = pg.view;
    hRuler_ = pg.hRuler;
    vRuler_ = pg.vRuler;
    docTabs_->addTab(docTabText(0));

    // 中央区 = [参数输入条] 在上 + [文档标签条] + [页栈（标尺 + 视图）]
    auto* center = new QWidget(this);
    auto* cvl = new QVBoxLayout(center);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);
    cvl->addWidget(paramBar_);
    cvl->addWidget(docTabs_);
    cvl->addWidget(viewStack_, 1);
    setCentralWidget(center);
}

// 为会话创建一页：标尺十字布局 + 该文档专属的 GraphicsView（每文档独立视图）
MainWindow::DocPage MainWindow::makeDocPage(DocSession* d) {
    DocPage pg;
    pg.view   = new GraphicsView(d->model(), this);
    pg.hRuler = new RulerWidget(pg.view, RulerWidget::Orientation::Horizontal);
    pg.vRuler = new RulerWidget(pg.view, RulerWidget::Orientation::Vertical);

    auto* corner = new QWidget();
    corner->setFixedSize(24, 24);
    corner->setAutoFillBackground(true);

    pg.page = new QWidget;
    auto* grid = new QGridLayout(pg.page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->addWidget(corner, 0, 0);
    grid->addWidget(pg.hRuler, 0, 1);
    grid->addWidget(pg.vRuler, 1, 0);
    grid->addWidget(pg.view, 1, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(1, 1);

    docPages_.insert(d, pg);
    viewStack_->addWidget(pg.page);
    return pg;
}

QString MainWindow::docTabText(int idx) const {
    const DocSession* d = docs_->doc(idx);
    return d ? d->title() : QString();
}

void MainWindow::syncTabTitle(int idx) {
    if (docTabs_ && idx >= 0 && idx < docTabs_->count())
        docTabs_->setTabText(idx, docTabText(idx));
}

// 切换活动文档：互换命令行历史 → 换绑指针与连接 → 回显 → 同步标签/页栈
void MainWindow::switchToDoc(int idx, bool forceHook) {
    DocSession* d = docs_ ? docs_->doc(idx) : nullptr;
    if (!d || !docPages_.contains(d)) return;
    if (d != docs_->active() || forceHook) {
        if (auto* cur = docs_->active()) {           // copy-out：当前文档的会话态
            cur->cmdHistory  = interp_->history();
            cur->lastCommand = interp_->lastCommand();
        }
        { QSignalBlocker b(docs_); docs_->setActive(idx); }   // 本函数负责同步 UI
        const DocPage pg = docPages_.value(d);
        model_  = d->model();
        view_   = pg.view;
        hRuler_ = pg.hRuler;
        vRuler_ = pg.vRuler;
        hookActiveDoc();                              // copy-in + 重绑 + 回显
    }
    { QSignalBlocker b(docTabs_); docTabs_->setCurrentIndex(idx); }
    viewStack_->setCurrentWidget(docPages_.value(d).page);
}

// 关闭保护：脏文档弹「保存 / 不保存 / 取消」；返回 false = 用户取消，放弃关闭
bool MainWindow::confirmCloseDoc(int idx) {
    DocSession* d = docs_->doc(idx);
    if (!d || !d->isDirty()) return true;
    switchToDoc(idx);   // 保存流程作用于活动文档，先切过去
    const auto ret = QMessageBox::question(this, QStringLiteral("关闭图纸"),
        QStringLiteral("图纸「%1」有未保存的修改，是否保存？").arg(d->title()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (ret == QMessageBox::Save)   { saveFile(); return !d->isDirty(); }   // 保存失败/取消 → 继续保留
    if (ret == QMessageBox::Discard) return true;
    return false;
}

// 关闭文档：先销毁页面（视图引用模型，必须先于模型析构），再销毁会话
void MainWindow::closeDocAt(int idx) {
    DocSession* d = docs_->doc(idx);
    if (!d) return;
    if (DocPage pg = docPages_.value(d); pg.page) {
        docPages_.remove(d);
        delete pg.page;   // 视图/标尺随页面销毁
    }
    docs_->removeDoc(idx);   // 会话 + 模型销毁；活动下标夹取到邻居
    if (docTabs_) {
        QSignalBlocker b(docTabs_);
        docTabs_->removeTab(idx);
    }
    if (docs_->count() == 0) {
        // 全部关闭：回落到一张空白图，保证窗口始终可用（与「新建」等价的空会话）
        DocSession* s = docs_->addNewDoc();
        makeDocPage(s);
        if (docTabs_) docTabs_->addTab(docTabText(0));
        switchToDoc(0, /*forceHook=*/true);
        return;
    }
    switchToDoc(docs_->activeIndex(), /*forceHook=*/true);
}

// 新标签打开图纸：新建会话 → 读档；失败回滚关闭新标签（空文档不脏，直接关）
int MainWindow::openDocInteractive(const QString& path) {
    DocSession* s = docs_->addNewDoc();
    makeDocPage(s);
    if (docTabs_) docTabs_->addTab(docTabText(docs_->count() - 1));
    switchToDoc(docs_->count() - 1);
    QString err;
    if (!readDrawing(path, &err)) {
        closeDocAt(docs_->count() - 1);
        return -1;
    }
    s->filePath = path;
    s->markClean();          // 读档过程的模型改动是瞬态的，读后即净
    syncTabTitle(docs_->activeIndex());
    return docs_->activeIndex();
}

// ---- 启动页：启动=继续上次工作 ----

// 「出厂空白」判定：唯一/未命名/不脏/无图元——启动页打开首图后可安全收走的文档
bool MainWindow::isPristineBlank(const DocSession* s) const {
    return s && s->filePath.isEmpty() && !s->isDirty()
        && s->model() && s->model()->elements().empty();
}

// 收走 0 号位的出厂空白文档：先 delete 页面（视图先于会话/模型析构）再摘会话。
// 活动下标收缩由 DocManager::removeDoc 保证（关非活动且位于活动之前 → 左移）。
void MainWindow::closePristineFirstDoc() {
    if (docs_->count() == 0) return;
    if (!isPristineBlank(docs_->doc(0))) return;      // 只收出厂空白，用户内容绝不动
    const DocPage pg = docPages_.take(docs_->doc(0));
    delete pg.page;
    {
        QSignalBlocker blk(docTabs_);                 // 摘标签不触发切换（活动会话不变）
        docTabs_->removeTab(0);
    }
    docs_->removeDoc(0);
    if (docs_->count() > 0 && docs_->activeIndex() >= 0)
        switchToDoc(docs_->activeIndex(), /*forceHook=*/true);   // 保险：重绑 + 回显
}

// 启动页打开首图：成功后若出厂空白仍在则收走（避免留下永远不用的空标签）。
// 打开失败时 openDocInteractive 内部回滚 → 窗口仍保持一张可用空白图。
int MainWindow::openDocAdoptingBlank(const QString& path) {
    const int idx = openDocInteractive(path);
    if (idx >= 0 && docs_->count() == 2 && isPristineBlank(docs_->doc(0)))
        closePristineFirstDoc();
    return idx;
}

// 模板化新建唯一入口：菜单「新建」与启动页模板按钮共用同一 new drawing 流。
// applyNewDrawing 原样不动；TitleBlockStyle::None 在同一撤销步内静默移除标题栏层
//（beginEdit 的快照在 applyNewDrawing 开头已压栈，此后的 Quiet 操作并入该步，
//  整张"新建"仍只占一次 Ctrl+Z）。
void MainWindow::createTemplatedDrawing(const SheetSpec& spec, double ratio,
                                        TitleBlockStyle tb) {
    DocSession* s = docs_->addNewDoc();
    makeDocPage(s);
    docTabs_->addTab(docTabText(docs_->count() - 1));
    switchToDoc(docs_->count() - 1);
    applyNewDrawing(spec, ratio);                      // 既有业务逻辑原样
    if (tb == TitleBlockStyle::None) removeTitleBlockElements();
    s->markClean();                                    // 新建的图框/标准层不是用户改动
    syncTabTitle(docs_->activeIndex());
}

// 移除标题栏层全部图元（网格线 + tb:label/tb:field 文本同层）
void MainWindow::removeTitleBlockElements() {
    std::vector<int> ids;
    for (const auto& e : model_->elements())
        if (e.layer == QStringLiteral("标题栏")) ids.push_back(e.id);
    for (int id : ids) model_->removeElementQuiet(id);
    model_->log(QStringLiteral("已按「无标题栏」样式移除标题栏（%1 条图元）").arg(ids.size()));
}

// 启动页对话框：只做选择、不动模型；执行统一在 exec 返回后分派。
// 恢复横幅仅在存在 .sv$ 快照时出现（优先显示，配合 P0-02）；恢复走既有
// CrashRecoveryDialog → onAutoSaveRecovered 全流程（含覆盖确认/另存为）。
namespace {

class StartupPage : public QDialog {
public:
    // 取值避开 QDialog::Accepted(1)/Rejected(0)：exec() 的返回值须与本枚举互斥判别
    enum Action { None = 0, NewTemplate = 100, OpenOther = 101, OpenRecent = 102, Recover = 103 };

    explicit StartupPage(SketchCAD::RecentFiles* recent, int maxShown, QWidget* parent)
        : QDialog(parent), recent_(recent), maxShown_(maxShown) {
        setWindowTitle(QStringLiteral("航空CAD — 继续"));
        resize(420, 480);

        auto* root = new QVBoxLayout(this);
        auto* title = new QLabel(QStringLiteral("继续上次工作"), this);
        QFont tf = title->font();
        tf.setBold(true);
        tf.setPointSize(tf.pointSize() + 2);
        title->setFont(tf);
        root->addWidget(title);

        // ---- 恢复横幅（优先显示：上次未正常保存）----
        const QVector<SketchCAD::AutoSave::Snapshot> snaps = SketchCAD::AutoSave::scanSnapshots();
        if (!snaps.isEmpty()) {
            auto* banner = new QFrame(this);
            banner->setFrameShape(QFrame::StyledPanel);
            banner->setStyleSheet(
                QStringLiteral("QFrame { background: #FFF4E5; border: 1px solid #E6A23C; border-radius: 4px; }"));
            auto* bl = new QVBoxLayout(banner);
            bl->addWidget(new QLabel(
                QStringLiteral("检测到 %1 份上次未正常保存的图纸快照").arg(snaps.size()), banner));
            auto* recBtn = new QPushButton(QStringLiteral("恢复图纸..."), banner);
            bl->addWidget(recBtn);
            connect(recBtn, &QPushButton::clicked, this, [this] { done(Recover); });
            root->addWidget(banner);
        }

        // ---- 新建：空白 / 标准模板 A0~A4（横向 1:1）/ 更多模板 ----
        auto* gbNew = new QGroupBox(QStringLiteral("新建"), this);
        auto* nl = new QGridLayout(gbNew);
        auto* blankBtn = new QPushButton(QStringLiteral("空白图"), gbNew);
        nl->addWidget(blankBtn, 0, 0);
        connect(blankBtn, &QPushButton::clicked, this, [this] { done(None); });
        static const QPair<SheetFormat, QString> kFmts[] = {
            {SheetFormat::A0, QStringLiteral("A0")}, {SheetFormat::A1, QStringLiteral("A1")},
            {SheetFormat::A2, QStringLiteral("A2")}, {SheetFormat::A3, QStringLiteral("A3")},
            {SheetFormat::A4, QStringLiteral("A4")},
        };
        int col = 1;
        for (const auto& f : kFmts) {
            auto* b = new QPushButton(f.second, gbNew);
            const SheetFormat fmt = f.first;
            connect(b, &QPushButton::clicked, this, [this, fmt] {
                spec_ = SheetSpec::make(fmt, /*landscape=*/true);
                ratio_ = 1.0;
                tb_ = TitleBlockStyle::Standard;
                done(NewTemplate);
            });
            nl->addWidget(b, 0, col++);
        }
        auto* moreBtn = new QPushButton(QStringLiteral("更多模板..."), gbNew);
        connect(moreBtn, &QPushButton::clicked, this, [this] {
            NewDrawingDialog dlg(this);
            if (dlg.exec() != QDialog::Accepted) return;
            spec_ = dlg.selectedSpec();
            ratio_ = dlg.selectedScaleRatio();
            tb_ = dlg.titleBlockStyle();
            done(NewTemplate);
        });
        nl->addWidget(moreBtn, 1, 0, 1, col);
        root->addWidget(gbNew);

        // ---- 打开最近 ----
        auto* gbRecent = new QGroupBox(QStringLiteral("打开最近"), this);
        auto* rl = new QVBoxLayout(gbRecent);
        list_ = new QListWidget(gbRecent);
        list_->setObjectName("startupRecentList");
        populateRecent();
        rl->addWidget(list_);
        // 双击打开；缺失项提示且不打开（不崩溃）
        connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
            const QString p = it->data(Qt::UserRole).toString();
            if (p.isEmpty()) return;
            if (!QFile::exists(p)) {
                QMessageBox::information(this, QStringLiteral("路径不存在"),
                    QStringLiteral("该文件当前不可用：\n%1").arg(p));
                return;
            }
            recentPath_ = p;
            done(OpenRecent);
        });
        // 右键：固定/取消固定、从列表中删除（与「最近打开」菜单同语义）
        list_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            QListWidgetItem* it = list_->itemAt(pos);
            if (!it) return;
            const QString p = it->data(Qt::UserRole).toString();
            if (p.isEmpty()) return;
            QMenu ctx(list_);
            QAction* pinAct = ctx.addAction(recent_->isPinned(p)
                                                ? QStringLiteral("取消固定")
                                                : QStringLiteral("固定到列表"));
            QAction* delAct = ctx.addAction(QStringLiteral("从列表中删除"));
            QAction* chosen = ctx.exec(list_->mapToGlobal(pos));
            if (chosen == pinAct) recent_->setPinned(p, !recent_->isPinned(p));
            else if (chosen == delAct) recent_->remove(p);
            populateRecent();
        });
        root->addWidget(gbRecent, 1);

        // ---- 打开其他 / 关闭 ----
        auto* hl = new QHBoxLayout;
        auto* openBtn = new QPushButton(QStringLiteral("打开其他..."), this);
        connect(openBtn, &QPushButton::clicked, this, [this] { done(OpenOther); });
        hl->addWidget(openBtn);
        hl->addStretch(1);
        auto* closeBtn = new QPushButton(QStringLiteral("继续使用空白图"), this);
        connect(closeBtn, &QPushButton::clicked, this, [this] { done(None); });
        hl->addWidget(closeBtn);
        root->addLayout(hl);
    }

    Action    action() const { return action_; }
    SheetSpec spec() const { return spec_; }
    double    ratio() const { return ratio_; }
    TitleBlockStyle titleBlockStyle() const { return tb_; }
    QString   recentPath() const { return recentPath_; }

private:
    // 从 RecentFiles 重建最近列表（pin 置顶标记 / 缺失项灰显 + 提示）
    void populateRecent() {
        list_->clear();
        if (!recent_ || recent_->count() == 0 || maxShown_ <= 0) {
            auto* it = new QListWidgetItem(QStringLiteral("（空）"), list_);
            it->setFlags(Qt::NoItemFlags);
            return;
        }
        int shown = 0;
        for (const auto& e : recent_->entries()) {
            if (shown >= maxShown_) break;
            const bool missing = !QFile::exists(e.path);
            auto* it = new QListWidgetItem(
                QStringLiteral("%1%2%3")
                    .arg(e.pinned ? QStringLiteral("📌 ") : QString())
                    .arg(QFileInfo(e.path).fileName())
                    .arg(missing ? QStringLiteral("（路径不存在）") : QString()),
                list_);
            it->setData(Qt::UserRole, e.path);
            it->setToolTip(e.path);
            if (missing) it->setForeground(QBrush(QColor(0x9A, 0x9A, 0x9A)));
            ++shown;
        }
    }

    SketchCAD::RecentFiles* recent_ = nullptr;
    int maxShown_ = 10;
    Action action_ = None;
    SheetSpec spec_;
    double ratio_ = 1.0;
    TitleBlockStyle tb_ = TitleBlockStyle::Standard;
    QString recentPath_;
    QListWidget* list_ = nullptr;
};
} // namespace

void MainWindow::showStartupPage() {
    StartupPage dlg(recent_, appSettings_ ? appSettings_->recentFileCount() : 10, this);
    const StartupPage::Action act = static_cast<StartupPage::Action>(dlg.exec());

    switch (act) {
    case StartupPage::Recover:
        offerCrashRecovery();   // 既有恢复全流程（CrashRecoveryDialog + 覆盖确认）
        break;
    case StartupPage::NewTemplate:
        createTemplatedDrawing(dlg.spec(), dlg.ratio(), dlg.titleBlockStyle());
        break;
    case StartupPage::OpenRecent: {
        const QString p = dlg.recentPath();
        if (!p.isEmpty()) openDocAdoptingBlank(p);
        break;
    }
    case StartupPage::OpenOther: {
        // 先记原状：openFile() 成功开新标签后收走出厂空白；取消则什么都不动
        const int before = docs_->count();
        openFile();
        if (docs_->count() == before + 1) closePristineFirstDoc();
        break;
    }
    case StartupPage::None:
    default:
        break;   // 空白图：首张空白文档保持原样
    }
}


// ---- 文件格式过滤器（另存为 / 打开）----
// 与 core/CadFormats.h 的 CadFormat 一一对应；扩展名是唯一的分派依据。
namespace {
QStringList saveNameFilters() {
    return {
        QStringLiteral("草图文件 (*.json)"),
        QStringLiteral("AutoCAD DXF (R12/R2000) 图形交换 (*.dxf)"),
        QStringLiteral("STEP 三维交换 (*.step *.stp)"),
        QStringLiteral("AutoCAD DWG 图形 (*.dwg)"),
    };
}
// 从过滤器文本（如 "AutoCAD DXF 图形交换 (*.dxf)"）里取第一个通配符的扩展名
QString suffixOfFilter(const QString& filter) {
    const int lb = filter.indexOf(QStringLiteral("*."));
    if (lb < 0) return QString();
    QString sfx;
    for (int i = lb + 2; i < filter.size(); ++i) {
        const QChar c = filter.at(i);
        if (c.isLetterOrNumber()) sfx += c;
        else break;
    }
    return sfx.toLower();
}
} // namespace

// 保存 / 另存为：按扩展名分派到 草图 JSON / DXF / STEP / DWG
void MainWindow::saveFile() {
    QFileDialog dlg(this, QStringLiteral("保存图纸"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setNameFilters(saveNameFilters());
    dlg.selectNameFilter(saveNameFilters().first());
    dlg.setDefaultSuffix(QStringLiteral("json"));
    // 多文档：默认预填活动文档的文件名（已存过盘）；未命名图保持 sketch.json
    const QString curPath = docs_ && docs_->active() ? docs_->active()->filePath : QString();
    dlg.selectFile(curPath.isEmpty() ? QStringLiteral("sketch.json")
                                     : QFileInfo(curPath).fileName());
    // 起始目录习惯：默认存盘目录 > 上次目录 > 系统默认
    const QString startDir = rememberDialogDir();
    if (!startDir.isEmpty()) dlg.setDirectory(startDir);
    connect(&dlg, &QFileDialog::filterSelected, &dlg, [&dlg](const QString& f) {
        const QString sfx = suffixOfFilter(f);
        if (!sfx.isEmpty()) dlg.setDefaultSuffix(sfx);      // 切格式时自动带上对应扩展名
    });
    if (dlg.exec() != QDialog::Accepted) return;

    QString fn = dlg.selectedFiles().value(0);
    if (fn.isEmpty()) return;

    cadfmt::CadFormat fmt = cadfmt::formatFromPath(fn);
    if (fmt == cadfmt::CadFormat::Unknown) {                 // 用户没写扩展名：按所选过滤器补
        fmt = cadfmt::formatFromSuffix(suffixOfFilter(dlg.selectedNameFilter()));
        if (fmt == cadfmt::CadFormat::Unknown) fmt = cadfmt::CadFormat::Json;
        fn += QLatin1Char('.') + cadfmt::defaultSuffix(fmt);
    }
    if (appSettings_) appSettings_->setLastDir(QFileInfo(fn).absolutePath());

    // DWG：已实现原生 R12 (AC1009) 二进制写出（见 CadFormats::toDwg），直接落盘即可。
    // DXF：弹出版本下拉（R12 默认 / R2000），写入 dxfSaveOptions_ 供 writeDrawing 采用。
    if (fmt == cadfmt::CadFormat::Dxf)
        dxfSaveOptions_.version = chooseDxfVersion();
    if (writeDrawing(fn, nullptr)) {
        // 多文档：保存成功 → 会话记录路径并清除脏标记（标签 "*" 消失）
        if (docs_ && docs_->active()) {
            docs_->active()->filePath = fn;
            docs_->markClean(docs_->activeIndex());
            syncTabTitle(docs_->activeIndex());
        }
    }
}

// 新建图纸的实作（与对话框解耦，便于离屏测试直接驱动）。
// 契约：比例由调用方给出（对话框默认 1:1）；clear() 会把比例复位为 1:1，
// 所以必须紧随其后 setScaleRatio 才能生效；整张图框只占一次撤销。
void MainWindow::applyNewDrawing(const SheetSpec& spec, double scaleRatio) {
    if (scaleRatio <= 0.0) scaleRatio = 1.0;
    setCurrentSheetFormat(spec.format);                // T6：字高等「随幅面」的量据此派生
    model_->clear();
    model_->setDrawingPath(QString());                 // 新图尚未存盘 → 外部参照无相对路径基准
    model_->setScaleRatio(scaleRatio, false);          // 本次新建的绘图比例，不压撤销
    // GB/T 14665-2012 表6：预置标准图层模板（01~12），便于按标准标识分层出图（T9）
    for (const Layer& l : standardLayers())
        model_->addLayer(l.name, l.color, l.lineWidth, l.lineStyle);
    model_->beginEdit(QStringLiteral("新建图纸"));
    model_->addElementsQuiet(buildSheetFrame(spec));   // 整张图框只占一次撤销
    syncTitleBlockScale();                             // 标题栏「比例」栏预填当前比例
    model_->log(QStringLiteral("已新建 %1 图纸（比例 %2）：已生成图框与标准标题栏")
                    .arg(spec.formatName()).arg(model_->scaleLabel()));
    view_->fitToRect(QRectF(0.0, 0.0, spec.width, spec.height));
}

// 按扩展名把当前图纸写盘。格式判据唯一来自扩展名（cadfmt::formatFromPath）。
// 返回是否成功；失败原因写入 *err（同时记日志）。
// 注意：DXF/DWG 的返回值是"最终字节"（可能为 GBK），必须原样写盘，
// 不能再 toUtf8() 一次，否则中文组码值会乱码。
bool MainWindow::writeDrawing(const QString& path, QString* err) {
    QString e;
    cadfmt::CadFormat fmt = cadfmt::formatFromPath(path);
    if (fmt == cadfmt::CadFormat::Unknown) fmt = cadfmt::CadFormat::Json;

    // 导出用图元：DXF/DWG/STEP 都不认识 BlockRef，导出前把块引用展开为其真实子图元，
    // 保证"导出物即所见"（几何不丢；块引用实例的层级关系仅在本程序 JSON 中保留）。
    std::vector<Element> exp;
    exp.reserve(model_->elements().size());
    for (const auto& e2 : model_->elements()) {
        if (e2.type == ElementType::BlockRef) {
            const auto subs = model_->expandBlockRef(e2);
            exp.insert(exp.end(), subs.begin(), subs.end());
        } else {
            exp.push_back(e2);
        }
    }

    QByteArray out;
    // 把绘图比例传给导出层，保证标注数字导出的是实物尺寸（GB/T 14690）
    const double ds = model_->scaleRatio();
    switch (fmt) {
    case cadfmt::CadFormat::Dxf: {
        // 原生导出：块/标注/填充以原生实体写出（AutoCAD/ZWCAD 打开后标注与填充可编辑、
        // 块不展开）。DWG/STEP 仍走预炸开的 exp（R12 二进制容器与 STEP 无块/标注语义）。
        // P0-01：版本由 dxfSaveOptions_ 携带（默认 R12，逐字节兼容旧档；另存为 DXF 时弹框选）
        cadfmt::DxfExportOptions no = dxfSaveOptions_;
        no.nativeBlocks = no.nativeDims = no.nativeHatch = true;
        // $INSUNITS 携带当前单位口径（AutoCAD 标准码），跨单位交换的语义锚点
        out = cadfmt::toDxf(model_->elements(), &e, ds, insunitsCode(model_->units().length),
                            &model_->blockTable(), &no);
        break;
    }
    case cadfmt::CadFormat::Dwg:
        out = cadfmt::toDwg(exp, &e, ds);
        break;
    case cadfmt::CadFormat::Step: {
        cadfmt::StepStats st;
        out = cadfmt::toStep(exp, &st, &e, ds).toUtf8();
        if (st.skippedText > 0 && e.isEmpty())
            e = QStringLiteral("（%1 个文字/标注未随 STEP 导出）").arg(st.skippedText);
        break;
    }
    case cadfmt::CadFormat::Json:
    default:
        fmt = cadfmt::CadFormat::Json;
        out = model_->toDrawingBytes();   // 集中式整图序列化入口（与 AutoSave 快照同源）
        break;
    }
    if (out.isEmpty()) {
        if (err) *err = e;
        model_->log(QStringLiteral("保存失败：") + e, "error");
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("无法写入 ") + path;
        model_->log(QStringLiteral("保存失败（无法写入）：") + path, "error");
        return false;
    }
    f.write(out);
    f.close();
    // 登记图纸路径：外部参照的「相对路径」以此为解析基准
    model_->setDrawingPath(path);
    // 自动保存的「原路径」随之更新（换名/另存为后，下次恢复写回新位置）
    if (autoSave_) autoSave_->setCurrentDrawing(path);
    if (err) *err = e;
    model_->log(QStringLiteral("已保存：%1（%2，%3 字节）%4")
                    .arg(path).arg(cadfmt::formatName(fmt)).arg(out.size()).arg(e));
    return true;
}

// 打开：草图 JSON 完整往返；DXF 读回图元几何（标注/样条已按几何炸开）。
// 多文档：每次打开都落在一张新标签上（对照作业）；读档失败自动回滚新标签。
void MainWindow::openFile() {
    // 起始目录习惯：默认存盘目录 > 上次目录 > 系统默认
    const QString startDir = rememberDialogDir();
    const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("打开图纸"), startDir,
        QStringLiteral("草图与图形 (*.json *.dxf *.dwg);;草图文件 (*.json);;AutoCAD DXF (*.dxf);;AutoCAD (*.dwg);;所有文件 (*)"));
    if (fn.isEmpty()) return;
    if (appSettings_) appSettings_->setLastDir(QFileInfo(fn).absolutePath());
    openDocInteractive(fn);
}

// ==================== P0-04：大图纸打开（后台解析 + 内存优雅降级） ====================

bool MainWindow::s_asyncLoadEnabled = true;   // 大文件后台解析开关（自测可关）

namespace {
// 大于此字节数的文件走线程池解析 + 模态进度对话框（4MB ≈ 3~5 万图元文本）。
constexpr qint64 kAsyncLoadBytes = 4LL * 1024 * 1024;

// 执行一个可能耗时的解析步骤 fn（签名 void()）：
//   小文件（或 s_asyncLoadEnabled=false）：同步直接执行，GUI 冻结时间与旧版一致；
//   大文件：QtConcurrent 线程池执行 + 模态 QProgressDialog（GUI 保持响应，可点「取消」）。
// cancelFlag 在用户点取消时置位（供 worker 内部协作中断）；返回 true = 用户已取消。
template <class F>
bool runParseStep(QWidget* parent, F&& fn, const QString& label, std::atomic<bool>* cancelFlag) {
    if (!MainWindow::s_asyncLoadEnabled) { fn(); return false; }   // 同步路径（自测）
    QProgressDialog prog(label, QStringLiteral("取消"), 0, 0, parent);
    prog.setWindowTitle(QStringLiteral("打开图纸"));
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    prog.setValue(0);                        // 0..0 = busy 转圈（解析总量未知）
    QObject::connect(&prog, &QProgressDialog::canceled, parent,
                     [cancelFlag]() { if (cancelFlag) cancelFlag->store(true); });
    QFutureWatcher<void> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run(std::forward<F>(fn)));
    loop.exec();                             // 等待解析完成（期间 GUI 事件正常分发）
    return prog.wasCanceled();
}
} // namespace

// 读盘的实作（与文件对话框解耦，便于离屏测试直接驱动）。
// P0-04 外壳：任何大批量分配 std::bad_alloc → 「弹窗 + 日志 + 保持程序可用」，绝不 terminate。
// 外壳统一记录打开耗时（成功时刷新状态栏数据面板）。
bool MainWindow::readDrawing(const QString& path, QString* err) {
    bool modelTouched = false;               // 模型是否已被 clear（决定失败时如何恢复）
    const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    try {
        const bool ok = readDrawingImpl(path, err, &modelTouched);
        if (ok) {
            lastOpenMs_ = (double)(QDateTime::currentMSecsSinceEpoch() - t0);
            statusBar_->setLoadStats((int)model_->elements().size(), view_->sceneItemCount(), lastOpenMs_);
            pushRecentFile(path);   // 成功读档 → 最近文件去重置顶（上限内的持久化在函数内部）
            // FIELD 域自动刷新（AppSettings fields/autoUpdateOnOpen，默认开）：
            // 打开文档后对全模型含域宿主预热求值一次（只读不改几何，异常回落 ###）。
            if (appSettings_ && appSettings_->fieldAutoUpdateOnOpen()) {
                const int nField = model_->refreshFields();
                if (nField > 0)
                    model_->log(QStringLiteral("已自动刷新 %1 个域宿主（fieldupdate 可手动重算）").arg(nField),
                                QStringLiteral("info"));
            }
        }
        return ok;
    } catch (const std::bad_alloc&) {
        if (modelTouched) {
            model_->clear();   // 恢复到已知空态（clear 发 modelChanged → 视图自动清空场景项）
        }
        model_->log(QStringLiteral("内存不足，已停止加载：") + path, "error");
        QMessageBox::warning(this, QStringLiteral("打开图纸"),
            QStringLiteral("内存不足，已停止加载。\n\n文件过大或系统可用内存不足，当前图纸未被改动（或已恢复为空图），程序仍可正常使用。"));
        if (err) *err = QStringLiteral("内存不足，已停止加载");
        return false;
    }
}

// 读盘实作（原同步实现；P0-04 增加后台解析与取消支持）。
bool MainWindow::readDrawingImpl(const QString& path, QString* err, bool* modelTouched) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取 ") + path;
        model_->log(QStringLiteral("打开失败：") + path, "error");
        return false;
    }
    const QByteArray raw = f.readAll();
    const bool bigFile = raw.size() > kAsyncLoadBytes;   // 大文件 → 后台解析 + 进度对话框
    const cadfmt::CadFormat fmt = cadfmt::formatFromPath(path);

    if (fmt == cadfmt::CadFormat::Step) {
        if (err) *err = QStringLiteral("STEP 为只写格式，不支持读回");
        model_->log(QStringLiteral("STEP 为只写格式（曲线集交换），本程序不支持读回：请用 CAD 转换后再导入"), "warn");
        return false;
    }

    if (fmt == cadfmt::CadFormat::Dxf) {
        std::vector<Element> imported;
        QString e;
        cadfmt::DxfImportStats st;
        bool parseOk = false;
        std::atomic<bool> cancelFlag{false};
        const bool canceled = runParseStep(this, [&]() {
            parseOk = cadfmt::fromDxf(raw, imported, &e, &st);   // 后台线程：纯数据解析，不碰 model/scene
        }, bigFile ? QStringLiteral("正在解析 DXF（后台）…") : QStringLiteral("正在解析 DXF…"),
           bigFile ? &cancelFlag : nullptr);
        if (canceled) {
            model_->log(QStringLiteral("已取消打开：") + path, "info");
            if (err) *err = QStringLiteral("已取消");
            return false;                    // 解析阶段取消：模型未被改动，原图纸完好
        }
        if (!parseOk) {
            if (err) *err = e;
            model_->log(QStringLiteral("DXF 解析失败：") + e, "error");
            return false;
        }
        // 跨单位交换：DXF 头声明的 $INSUNITS ≠ mm 时，按元数据把几何整体换算到 mm
        // （原点锚定，见 Units.h；0=无单位/缺头按 mm 解释，不缩放）。线宽与角度不动。
        const LengthUnit srcUnit = lengthUnitFromInsunits(st.insunits);
        const bool rescaled = lengthFactorToMm(srcUnit) != 1.0;
        if (rescaled)
            scaleElementsToUnit(imported, srcUnit);
        *modelTouched = true;   // P0-04：此后模型可能被改动（内存失败需恢复空态）
        model_->clear();
        // 块定义须先于 BlockRef 图元注册：INSERT 引用按名解析，缺定义则渲染为空。
        for (const auto& b : st.blocks) model_->addBlockDef(b);
        // 鲁棒性守卫：解析得到的图元先做只读校验/丢弃/修复，避免损坏数据进入模型造成崩溃或画错。
        int robSkip = 0, robFix = 0; QString robRep;
        std::vector<Element> kept = SketchCAD::filterElements(imported, &robSkip, &robFix, &robRep);
        if (robSkip > 0 || robFix > 0)
            model_->log(QStringLiteral("打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                           .arg(robSkip).arg(robFix).arg(robRep), "warn");
        model_->beginEdit(QStringLiteral("导入 DXF"));
        model_->addElementsQuiet(kept);
        QString note = QStringLiteral("已打开 DXF：%1（导入 %2 个图元%3；标注/样条已按几何炸开）")
                           .arg(path).arg(kept.size())
                           .arg(st.blocks.empty() ? QString() : QStringLiteral("，块定义 %1 个").arg(st.blocks.size()));
        if (rescaled)
            note += QStringLiteral("；按文件头 $INSUNITS 自 %1 换算到 mm")
                        .arg(lengthUnitName(srcUnit));
        // 圆弧段表示法不同导致的降级必须如实告知，不能静默改形状（宁可少画，绝不画错）
        if (st.arcsDropped > 0) {
            note += QStringLiteral("；其中 %1 段圆弧因表示法差异按直线导入（另有 %2 段已还原）")
                        .arg(st.arcsDropped).arg(st.arcsKept);
        }
        // 复杂实体（HATCH/DIMENSION/LEADER 降级、未知类型跳过）也须如实计数告知
        if (st.fallbackEntities > 0)
            note += QStringLiteral("；%1 个复杂实体已按基元降级导入").arg(st.fallbackEntities);
        if (st.unknownEntities > 0)
            note += QStringLiteral("；%1 个未知实体类型已跳过").arg(st.unknownEntities);
        model_->log(note, (st.arcsDropped > 0 || st.fallbackEntities > 0 || st.unknownEntities > 0) ? "warn" : "info");
        // 打开后把整幅图适配居中。不能沿用 fitView()（固定 zoom=4 且以**原点**为中心）：
        // 外部 DXF 的图形常画在远离原点处（如 x≈200~240），按原点居中会落在视口外，
        // 表现为"打开了却一片空白"。
        QRectF bb;
        bool haveBox = false;
        for (const auto& el : kept) {
            const QRectF r = geo::elementBounds(el);
            bb = haveBox ? bb.united(r) : r;
            haveBox = true;
        }
        if (haveBox) {
            if (bb.width()  < 1e-6) bb.adjust(-10.0, 0.0, 10.0, 0.0);   // 退化：只有水平线之类
            if (bb.height() < 1e-6) bb.adjust(0.0, -10.0, 0.0, 10.0);
            view_->fitToRect(bb);
        } else {
            view_->fitView();
        }
        return true;
    }

    if (fmt == cadfmt::CadFormat::Dwg) {
        std::vector<Element> imported;
        QString e;
        cadfmt::DxfImportStats st;
        bool parseOk = false;
        std::atomic<bool> cancelFlag{false};
        const bool canceled = runParseStep(this, [&]() {
            parseOk = cadfmt::fromDwg(raw, imported, &e, &st);   // 后台线程：纯数据解析
        }, bigFile ? QStringLiteral("正在解析 DWG（后台）…") : QStringLiteral("正在解析 DWG…"),
           bigFile ? &cancelFlag : nullptr);
        if (canceled) {
            model_->log(QStringLiteral("已取消打开：") + path, "info");
            if (err) *err = QStringLiteral("已取消");
            return false;
        }
        if (!parseOk) {
            if (err) *err = e;
            model_->log(QStringLiteral("DWG 解析失败：") + e, "error");
            return false;
        }
        *modelTouched = true;
        model_->clear();
        // 鲁棒性守卫：解析得到的图元先做只读校验/丢弃/修复，避免损坏数据进入模型造成崩溃或画错。
        int robSkip = 0, robFix = 0; QString robRep;
        std::vector<Element> kept = SketchCAD::filterElements(imported, &robSkip, &robFix, &robRep);
        if (robSkip > 0 || robFix > 0)
            model_->log(QStringLiteral("打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                           .arg(robSkip).arg(robFix).arg(robRep), "warn");
        model_->beginEdit(QStringLiteral("导入 DWG"));
        model_->addElementsQuiet(kept);
        QString note = QStringLiteral("已打开 DWG（R12/AC1009）：%1（导入 %2 个图元）")
                           .arg(path).arg(kept.size());
        model_->log(note, "info");
        QRectF bb; bool haveBox = false;
        for (const auto& el : kept) {
            const QRectF r = geo::elementBounds(el);
            bb = haveBox ? bb.united(r) : r;
            haveBox = true;
        }
        if (haveBox) {
            if (bb.width()  < 1e-6) bb.adjust(-10.0, 0.0, 10.0, 0.0);
            if (bb.height() < 1e-6) bb.adjust(0.0, -10.0, 0.0, 10.0);
            view_->fitToRect(bb);
        } else {
            view_->fitView();
        }
        return true;
    }

    // P0-04：JSON 解析（fromJson + 逐图元 elementFromJson）对大文件走后台线程；
    // worker 只产出 loaded 与 parsedRoot（纯数据值语义），循环内响应取消。
    // 小文件同步执行（行为与旧版一致）。
    std::vector<Element> loaded;
    QJsonObject parsedRoot;
    bool jsonOk = false;
    {
        std::atomic<bool> cancelFlag{false};
        const bool canceled = runParseStep(this, [&]() {
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (!doc.isObject()) return;
            parsedRoot = doc.object();
            const QJsonArray arr = parsedRoot["elements"].toArray();
            loaded.reserve(arr.size());
            for (const auto& v : arr) {
                if (cancelFlag.load(std::memory_order_relaxed)) { loaded.clear(); return; }
                loaded.push_back(elementFromJson(v.toObject()));
            }
            jsonOk = true;
        }, bigFile ? QStringLiteral("正在解析图纸（后台）…") : QStringLiteral("正在解析图纸…"),
           bigFile ? &cancelFlag : nullptr);
        if (canceled) {
            model_->log(QStringLiteral("已取消打开：") + path, "info");
            if (err) *err = QStringLiteral("已取消");
            return false;   // 解析阶段取消：模型未被改动，原图纸完好
        }
    }
    if (!jsonOk) {
        if (err) *err = QStringLiteral("不是有效的草图文件");
        model_->log(QStringLiteral("打开失败：不是有效的草图文件"), "error");
        return false;
    }
    const QJsonObject root = parsedRoot;
    *modelTouched = true;   // P0-04：此后模型可能被改动（内存失败需恢复空态）
    model_->clear();
    // 鲁棒性守卫：解析得到的图元先做只读校验/丢弃/修复，避免损坏数据进入模型造成崩溃或画错。
    int robSkip = 0, robFix = 0; QString robRep;
    std::vector<Element> kept = SketchCAD::filterElements(loaded, &robSkip, &robFix, &robRep);
    if (robSkip > 0 || robFix > 0)
        model_->log(QStringLiteral("打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                       .arg(robSkip).arg(robFix).arg(robRep), "warn");
    model_->beginEdit(QStringLiteral("打开草图"));
    model_->addElementsQuiet(kept);
    // 读回图层表（若有）。必须在 addElementsQuiet 之后、setScaleRatioValue 之前：
    // 图层表不依赖比例；若文件无 layers 字段则回退为单层"0"，不影响旧图兼容。
    model_->setLayersFromJson(root["layers"].toArray());
    // 读回块表（若有）。块引用图元（type=BlockRef）在渲染/命中时按此表展开，
    // 因此必须与图元一并读回，否则引用会退化为"未定义块"而不绘制。旧文件无此字段 → 无块定义。
    model_->setBlocksFromJson(root["blocks"].toArray());
    // 读回文字样式表（若有）。图元经 styleName 引用样式，必须与图元一并读回；
    // 旧文件无此字段 → 回落为单一默认 "Standard"（渲染解析非 null）。
    model_->setTextStylesFromJson(root["textStyles"].toArray());
    // 读回标注样式表（一图多标准）。图元经 dimStyleName 引用样式，必须与图元一并读回；
    // 旧文件无此字段 → 自动生成单一默认 "Standard"，等价于原"单全局 DimStyle"（零行为变化）。
    model_->setDimStylesFromJson(root["dimStyles"].toObject());
    model_->setSavedFiltersFromJson(root["savedFilters"].toArray());   // 旧文件无此字段 → 空过滤器列表（容缺）
    model_->setLayoutsFromJson(root);   // 布局表（顶层 "layouts"/"currentLayout"；旧文件缺键 → 空表回落模型空间打印）
    // 鲁棒性守卫（续）：统计引用了不存在图层/块/样式或悬空几何(refA/refB/hatchSrcIds)的图元，
    // 如实告知用户（不丢弃——这些引用按各模块既有降级逻辑静默处理）。
    {
        std::vector<QString> lns, bns, sns;
        for (int i = 0; i < model_->layerCount(); ++i) lns.push_back(model_->layerAt(i).name);
        for (const auto& b : model_->blockTable()) bns.push_back(b.name);
        for (int i = 0; i < model_->textStyleCount(); ++i) sns.push_back(model_->textStyleAt(i).name);
        QString drep;
        const int dangling = SketchCAD::countDanglingRefs(kept, lns, bns, sns, &drep);
        if (dangling > 0)
            model_->log(QStringLiteral("打开校验：%1 个图元引用了不存在的图层/块/样式或悬空几何：%2")
                           .arg(dangling).arg(drep), "warn");
    }
    // 读回单位与图纸界限（若有）。二者是**随图属性**（随图存档、现按 Ctrl+Z 可回退）：
    // 不发 modelChanged，只发各自的专用信号驱动状态栏/参数条刷新；
    // 旧文件无此字段 → 回落 mm/十进制度/2 位 + A3。
    //
    // 抑制快照：这四项现在会各自入栈，但开档已经由上面的 beginEdit("打开草图")
    // 建立了唯一编辑步；若不抑制，一次「打开」会被拆成好几步 Ctrl+Z
    // （先退回"单位没设"、再退回"界限没设"…）。抑制范围严格限于这四行，
    // 其余调用（含 setScaleRatioValue / setLtscale）保持原样，开档的撤销步数与旧版一致。
    {
        SketchModel::UndoGuard guard(model_);
        model_->setUnits(UnitsMeta::fromJson(root["units"].toObject()));
        model_->setLimits(LimitsMeta::fromJson(root["limits"].toObject()));
        model_->setSnapConfig(snap::SnapConfig::fromJson(root["snapConfig"].toObject()));  // 捕捉配置（缺键回落默认）
        model_->setTrackConfig(snap::TrackConfig::fromJson(root["trackConfig"].toObject())); // 追踪配置（缺键回落默认）
    }
    model_->setConstraintsFromJson(root["constraints"].toArray());  // 显式几何约束（缺键 → 空表）
    model_->setParamsFromJson(root["params"].toObject());           // 命名设计变量表（缺键 → 空表，Request M 新增）
    model_->setPropsFromJson(root["properties"].toObject());        // 自定义图纸特性表（缺键 → 空表，PROP 域数据源）
    // 读档的比例只作元数据恢复：存档里的几何已经是按该比例画好的成品，不能再缩放一次
    if (root.contains("scaleRatio"))
        model_->setScaleRatioValue(root["scaleRatio"].toDouble(1.0));
    // 全局线型比例随图恢复（缺键 → 回落 1.0；setLtscale 内部守卫保证非正/无变化不触发重绘）
    if (root.contains("ltscale"))
        model_->setLtscale(root["ltscale"].toDouble(1.0));
    // 登记图纸路径：它是外部参照「相对路径」的解析基准（读回后 xref 才能被重新定位）
    model_->setDrawingPath(path);
    syncTitleBlockScale();
    model_->log(QStringLiteral("已打开：%1（比例 %2）").arg(path).arg(model_->scaleLabel()));
    view_->fitView();
    if (err) err->clear();
    return true;
}

// 修改绘图比例：按内容包围盒中心等比缩放已有图元（图框/标题栏代表纸张，不缩放）
void MainWindow::changeDrawingScale() {
    bool ok = false;
    const double v = QInputDialog::getDouble(this, QStringLiteral("绘图比例"),
        QStringLiteral("输入比例（图形:实际）。1:1 = 1.0，1:2 = 0.5，2:1 = 2.0。\n"
                       "修改比例后，已有图元将按此比例等比缩放（图框与标题栏不变）。"),
        model_->scaleRatio(), 1e-4, 1000.0, 4, &ok);
    if (ok && v > 0.0) model_->setScaleRatio(v, true);
}

// 把当前绘图比例写进标题栏的「比例」栏（tb:field:scale），保证图纸打印时比例正确
void MainWindow::syncTitleBlockScale() {
    const QString label = model_->scaleLabel();
    std::vector<int> need;
    for (const auto& e : model_->elements())
        if (e.name == QStringLiteral("tb:field:scale") && e.text != label) need.push_back(e.id);
    for (int id : need)
        model_->updateElementQuiet(id, [&label](Element& x) { x.text = label; });
}

// 插入标准标题栏：弹出图纸选择对话框 -> 读取所选文件元数据 -> 在当前图纸的图框之外
// 追加绘制标准化「图纸边框 + 标准标题栏」。全程只追加新图元，绝不修改/移动/删除已有图元。
void MainWindow::insertStandardTitleBlock() {
    // 1) 弹出图纸选择对话框，供用户选择目标图纸文件并确认
    const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("选择图纸文件"),
        QString(), QStringLiteral("草图文件 (*.json);;所有文件 (*)"));
    if (fn.isEmpty()) {   // 未选择图纸
        QMessageBox::warning(this, QStringLiteral("插入标准标题栏"),
            QStringLiteral("未选择图纸文件。"));
        return;
    }

    // 2) 读取并解析所选文件（仅取元数据，不替换当前图纸）
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("插入标准标题栏"),
            QStringLiteral("文件加载失败：无法打开文件。\n%1").arg(fn));
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    const QJsonDocument d = QJsonDocument::fromJson(raw);
    if (!d.isObject()) {
        QMessageBox::critical(this, QStringLiteral("插入标准标题栏"),
            QStringLiteral("文件加载失败：不是有效的草图文件。"));
        return;
    }
    const QJsonObject root = d.object();

    // 3) 提取元数据：图名/图号优先取文件内标题栏，否则用文件名；比例为当前图纸比例；日期取今天
    InsertedTitleBlock data;
    const QJsonObject tb = root["titleBlock"].toObject();
    data.drawingName = tb["drawingName"].toString();
    if (data.drawingName.isEmpty()) {
        QFileInfo fi(fn);
        data.drawingName = fi.completeBaseName();
    }
    data.drawingNumber = tb["drawingNumber"].toString();
    data.scale = model_->scaleLabel();
    data.designDate = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));

    // 4) 计算当前图纸内容包围盒，绘制边框 + 标题栏（仅追加新图元，不改动已有图元）
    QRectF cb; bool have = false;
    for (const auto& el : model_->elements()) {
        const QRectF r = geo::elementBounds(el);
        if (r.isValid()) { cb = have ? cb.united(r) : r; have = true; }
    }
    auto elems = buildInsertedTitleBlock(have ? cb : QRectF(), data);
    model_->beginEdit(QStringLiteral("插入标准标题栏"));
    model_->addElementsQuiet(elems);
    model_->log(QStringLiteral("已插入标准标题栏（图名：%1，比例：%2）")
        .arg(data.drawingName).arg(data.scale));

    // 适配视图以显示全部内容（含新边框/标题栏）；仅改变视图，不动图元
    QRectF all; bool ha = false;
    for (const auto& el : model_->elements()) {
        const QRectF r = geo::elementBounds(el);
        if (r.isValid()) { all = ha ? all.united(r) : r; ha = true; }
    }
    if (ha) view_->fitToRect(all);
}

void MainWindow::generateFramedTitleBlock() {
    // 1) 弹出幅面选择对话框（A0~A5 + 横/纵）
    PaperSizeDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;   // 取消：不改动任何图元

    const SheetSpec spec = dlg.selectedSpec();
    setCurrentSheetFormat(spec.format);            // T6：字高等随幅面派生

    // 2) 计算当前图纸全部图元的最小外接矩形（左下角原点，X 右 Y 上，单位 mm）
    QRectF cb; bool have = false;
    for (const auto& el : model_->elements()) {
        const QRectF r = geo::elementBounds(el);
        if (r.isValid()) { cb = have ? cb.united(r) : r; have = true; }
    }

    // 3) 组装标题栏数据：比例取当前图纸比例，日期取今天；图名/图号留空待「填写标题栏」录入
    InsertedTitleBlock data;
    data.drawingName   = QString();
    data.drawingNumber = QString();
    data.scale         = model_->scaleLabel();
    data.designDate    = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));

    // 4) 生成"图纸边框 + 标题栏"并仅追加：不修改/移动/删除任何已有图元
    auto elems = buildAutoFrameTitleBlock(have ? cb : QRectF(), spec, data);
    model_->beginEdit(QStringLiteral("按幅面生成图框与标题栏"));
    model_->addElementsQuiet(elems);
    model_->log(QStringLiteral("已按 %1 生成图框与标题栏（内容%2被包含，标题栏自动避让）")
        .arg(spec.formatName())
        .arg(have ? QStringLiteral("已") : QStringLiteral("未")));

    // 5) 适配视图以显示全部内容（含新边框/标题栏）；仅改变视图，不动图元
    QRectF all; bool ha = false;
    for (const auto& el : model_->elements()) {
        const QRectF r = geo::elementBounds(el);
        if (r.isValid()) { all = ha ? all.united(r) : r; ha = true; }
    }
    if (ha) view_->fitToRect(all);
}

// 收集当前选择集的图元副本（用于阵列等复制型变换）
static std::vector<Element> collectSelectedElements(SketchModel* model) {
    std::vector<Element> src;
    if (!model) return src;
    for (int id : model->selection())
        if (const Element* e = model->element(id)) src.push_back(*e);
    return src;
}

// 当前选择集的几何包围盒中心（环形阵列默认中心点）
static Vec3 selectionCenter(SketchModel* model) {
    QRectF box; bool have = false;
    for (int id : model->selection()) {
        const Element* e = model->element(id);
        if (!e) continue;
        const QRectF r = geo::elementBounds(*e);
        if (r.isValid()) { box = have ? box.united(r) : r; have = true; }
    }
    if (!have) return Vec3(0, 0, 0);
    const QPointF c = box.center();
    return Vec3(c.x(), c.y(), 0);
}

void MainWindow::arrayRectangular() {
    if (model_->selection().empty()) {
        QMessageBox::warning(this, QStringLiteral("阵列复制"),
                             QStringLiteral("请先选中要阵列的一个或多个图元。"));
        return;
    }
    ArrayDialog dlg(ArrayDialog::Rectangular, Vec3(0, 0, 0), this);
    if (dlg.exec() != QDialog::Accepted) return;
    const auto res = dlg.result();
    if (res.mode != ArrayDialog::Rectangular) return;

    auto src = collectSelectedElements(model_);
    auto changes = modify::makeRectangularArray(src, res.rows, res.cols,
                                                res.rowSpacing, res.colSpacing);
    model_->beginEdit(QStringLiteral("矩形阵列"));
    const auto newIds = modify::applyChanges(model_, changes, QString());
    // 选中全部新生成的副本，便于继续编辑
    model_->setSelection(newIds);
    model_->log(QStringLiteral("矩形阵列：%1 个源图元 → 新增 %2 个副本（%3×%4）")
                    .arg(src.size()).arg(newIds.size()).arg(res.rows).arg(res.cols));
}

void MainWindow::arrayPolar() {
    if (model_->selection().empty()) {
        QMessageBox::warning(this, QStringLiteral("阵列复制"),
                             QStringLiteral("请先选中要阵列的一个或多个图元。"));
        return;
    }
    const Vec3 defCenter = selectionCenter(model_);
    // 非模态：保持主窗口可交互，以便用户在「视图拾取」模式下点击绘图区取中心点。
    auto* dlg = new ArrayDialog(ArrayDialog::Polar, defCenter, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    activeArrayDialog_ = dlg;

    connect(dlg, &QDialog::finished, this, [this](int r) {
        ArrayDialog* d = activeArrayDialog_;
        activeArrayDialog_ = nullptr;
        view_->setPointPickMode(false);   // 关闭对话框即退出拾取态，复位光标
        if (r != QDialog::Accepted || !d) return;
        const auto res = d->result();
        if (res.mode != ArrayDialog::Polar) return;

        auto src = collectSelectedElements(model_);
        auto changes = modify::makePolarArray(src, res.center, res.count, res.angleRangeDeg);
        model_->beginEdit(QStringLiteral("环形阵列"));
        const auto newIds = modify::applyChanges(model_, changes, QString());
        // 选中全部新生成的副本，便于继续编辑
        model_->setSelection(newIds);
        model_->log(QStringLiteral("环形阵列：%1 个源图元 → 新增 %2 个副本（%3 个 / %4°）")
                        .arg(src.size()).arg(newIds.size())
                        .arg(res.count).arg(res.angleRangeDeg, 0, 'f', 2));
    });
    // 用户点击「拾取中心点」→ 进入视图拾取态（光标变十字，下次左键返回世界坐标）
    connect(dlg, &ArrayDialog::requestCanvasPick, this, [this]() {
        if (activeArrayDialog_) view_->setPointPickMode(true);
    });
    dlg->show();
}

void MainWindow::onCanvasPointPicked(const Vec3& p) {
    // 镜像（点 / 线）基准拾取优先处理：拦截后不进入阵列对话框回填逻辑。
    if (mirrorPending_) { handleMirrorPick(p); return; }
    // 剪贴板（copybase 基点 / 粘贴落点）拾取次优先处理。
    if (clipBasePending_ || clipPastePending_) { handleClipPick(p); return; }
    // 仅当处于拾取态且有活动对话框时回填；否则复位拾取态（避免左键被永久吞掉）。
    if (!view_->pointPickMode()) return;
    if (!activeArrayDialog_) {
        view_->setPointPickMode(false);
        return;
    }
    activeArrayDialog_->setCenter(p);   // 写入世界坐标并切到「视图拾取」显示态
    view_->setPointPickMode(false);      // 复位拾取态与光标
    activeArrayDialog_->raise();
    activeArrayDialog_->activateWindow();
}

// ============================ 镜像（点 / 线）============================
// 交互：菜单【修改>镜像】或命令「镜像」→ 弹出"点镜像 / 线镜像"选择 →
//   点镜像：进入视图取点模式，单击指定对称中心点；
//   线镜像：进入视图取点模式，依次单击两点定义镜像轴（先第一点，后第二点）；
//   确认后保留源图元、自动生成一份严格对称的镜像副本，并选中该副本。
// 复用 GraphicsView 的 pointPickMode（与环形阵列中心点拾取同一套基础设施）。

void MainWindow::mirror() {
    if (model_->selection().empty()) {
        model_->log(QStringLiteral("请先选中要镜像的图元"), "warn");
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("镜像"));
    box.setText(QStringLiteral("请选择镜像基准："));
    QPushButton* bPoint = box.addButton(QStringLiteral("点镜像（指定对称中心点）"), QMessageBox::ActionRole);
    QPushButton* bLine  = box.addButton(QStringLiteral("线镜像（指定一条直线）"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == bPoint) {
        mirrorPending_ = true; mirrorIsPoint_ = true;  mirrorP1Valid_ = false;
        model_->setActiveTool(QStringLiteral("select"));
        view_->setPointPickMode(true);
        model_->log(QStringLiteral("点镜像：请在画布上单击指定对称中心点"));
    } else if (box.clickedButton() == bLine) {
        mirrorPending_ = true; mirrorIsPoint_ = false; mirrorP1Valid_ = false;
        model_->setActiveTool(QStringLiteral("select"));
        view_->setPointPickMode(true);
        model_->log(QStringLiteral("线镜像：请依次单击两点定义镜像轴（先第一点，后第二点）"));
    }
}

void MainWindow::handleMirrorPick(const Vec3& p) {
    if (mirrorIsPoint_) {
        applyMirror(true, p, p);
        finishMirror();
        return;
    }
    // 线镜像：收集两点
    if (!mirrorP1Valid_) {
        mirrorP1_ = p; mirrorP1Valid_ = true;
        model_->log(QStringLiteral("线镜像：已拾取第一点，请拾取第二点定义镜像轴"));
        // pointPickMode 在每次左键拾取后仍保持开启（见 GraphicsView::mousePressEvent），
        // 无需重新 setPointPickMode(true)；保持十字光标等待第二点。
        return;
    }
    applyMirror(false, mirrorP1_, p);
    finishMirror();
}

void MainWindow::applyMirror(bool isPoint, const Vec3& b1, const Vec3& b2) {
    std::vector<Element> src = collectSelectedElements(model_);
    if (src.empty()) { model_->log(QStringLiteral("无可镜像图元"), "warn"); return; }
    auto changes = modify::makeMirror(src, isPoint, b1, b2);
    model_->beginEdit(QStringLiteral("修改-镜像"));
    const auto newIds = modify::applyChanges(model_, changes, QString());
    if (!newIds.empty()) model_->setSelection(newIds);  // 选中新生成的镜像副本，便于继续编辑
    model_->log(QStringLiteral("已生成 %1 个镜像图元（%2）")
                    .arg(newIds.size()).arg(isPoint ? "点镜像" : "线镜像"));
}

void MainWindow::finishMirror() {
    mirrorPending_ = false; mirrorIsPoint_ = false; mirrorP1Valid_ = false;
    if (view_->pointPickMode()) view_->setPointPickMode(false);
}

void MainWindow::onPointPickCancelled() {
    if (mirrorPending_) finishMirror();
    if (clipBasePending_ || clipPastePending_) finishClipPick();
}

// ============================ 连合（JOIN） ============================
// GUI 入口：把选中的同层 Line/Arc 按端点容差连合为 Polyline。
// 容差传 view_->worldTol()（= 8px/zoom，真实捕捉容差），与画布拾取手感一致；
// 命令行入口 join [tol] 缺省 kJoinDefaultTol=0.5mm，两者共用 modify::makeJoin。
void MainWindow::joinSelected() {
    std::vector<Element> src = collectSelectedElements(model_);
    if (src.empty()) { model_->log(QStringLiteral("请先选中要连合的 Line/Arc"), "warn"); return; }
    auto changes = modify::makeJoin(src, view_->worldTol());
    if (changes.empty()) {
        model_->log(QStringLiteral("没有可连合的链（只连合同层且端点在容差内相接的 Line/Arc）"), "warn");
        return;
    }
    model_->beginEdit(QStringLiteral("连合"));
    const auto newIds = modify::applyChanges(model_, changes, QString());
    if (!newIds.empty()) model_->setSelection(newIds);   // 选中连合结果，便于继续编辑
    model_->log(QStringLiteral("连合：%1 条图元 → %2 条 Polyline")
                    .arg(src.size()).arg(newIds.size()));
}

// ============================ 路径阵列 ============================
// GUI 入口：选区中第一条 Polyline/Spline 作路径，其余图元沿路径等距布点。
// 与命令行 arraypath 同一离散口径（32 段）与同一核心 modify::makePathArray。
void MainWindow::arrayPath() {
    std::vector<Element> src;
    const Element* path = nullptr;
    for (int id : model_->selection()) {
        const Element* e = model_->element(id);
        if (!e) continue;
        if (!path && (e->type == ElementType::Polyline || e->type == ElementType::Spline))
            path = e;                       // 第一条 Polyline/Spline 作路径
        else
            src.push_back(*e);              // 其余图元作阵列源
    }
    if (!path || src.empty()) {
        model_->log(QStringLiteral("路径阵列需要选区同时含 1 条 Polyline/Spline（路径）与待阵列图元"), "warn");
        return;
    }
    std::vector<Vec3> pathPts;
    if (path->type == ElementType::Spline)
        pathPts = geo::sampleSpline(path->pts, 32);
    else
        pathPts = geo::samplePolyline(path->pts, path->arcFlags, path->closed, 32);

    bool ok = false;
    const double spacing = QInputDialog::getDouble(
        this, QStringLiteral("路径阵列"),
        QStringLiteral("项目间距 (mm)："), 10.0, 0.001, 1.0e6, 2, &ok);
    if (!ok) return;

    auto changes = modify::makePathArray(src, pathPts, spacing);
    if (changes.empty()) {
        model_->log(QStringLiteral("路径阵列退化：路径太短或间距大于路径全长"), "warn");
        return;
    }
    model_->beginEdit(QStringLiteral("路径阵列"));
    const auto newIds = modify::applyChanges(model_, changes, QString());
    if (!newIds.empty()) model_->setSelection(newIds);
    model_->log(QStringLiteral("路径阵列：%1 个源图元 → 新增 %2 个副本（间距 %3 mm，路径 #%4）")
                    .arg(src.size()).arg(newIds.size()).arg(spacing, 0, 'f', 2).arg(path->id));
}

// ============================ 剪贴板族（GUI 入口） ============================
// 与命令行 ClipCommand 完全同语义：组包/落盘均走 clipboard 命名空间同一实现，
// 唯一差异是交互形态——copybase 基点与粘贴落点经画布拾取（pointPickMode），
// pasteclip 在命令行缺省落点 (0,0)，GUI 必须拾取一个落点（所见即所得）。

void MainWindow::copyClipToClipboard(bool withBase) {
    std::vector<Element> src = collectSelectedElements(model_);
    if (src.empty()) { model_->log(QStringLiteral("无可复制图元（请先选中）"), "warn"); return; }
    if (withBase) {
        // 带基点复制：进入拾取态，落点即基点（图元在 handleClipPick 里组包）
        clipBasePending_ = true;
        model_->setActiveTool(QStringLiteral("select"));
        view_->setPointPickMode(true);
        model_->log(QStringLiteral("带基点复制：请单击指定基点"));
        return;
    }
    // 复制：基点 = 选区包围盒左下角（对齐 AutoCAD 默认基点语义，与命令行一致）
    clipboard::Payload p;
    bool first = true;
    double minX = 0, minY = 0;
    for (const auto& e : src) {
        const QRectF b = geo::elementBounds(e);
        if (first) { minX = b.left(); minY = b.bottom(); first = false; }
        else { minX = std::min(minX, b.left()); minY = std::min(minY, b.bottom()); }
    }
    p.base = Vec3(minX, minY, 0);
    p.elements = std::move(src);
    p.blocks = clipboard::collectBlockDeps(*model_, p.elements);
    if (!clipboard::putToClipboard(p)) { model_->log(QStringLiteral("写入剪贴板失败"), "warn"); return; }
    model_->log(QStringLiteral("已复制 %1 个图元到剪贴板（基点 %2,%3）")
                    .arg(p.elements.size()).arg(p.base.x, 0, 'f', 2).arg(p.base.y, 0, 'f', 2));
}

void MainWindow::pasteFromClipboard(bool asBlock) {
    clipboard::Payload p;
    QString err;
    if (!clipboard::getFromClipboard(&p, &err)) {
        model_->log(err.isEmpty() ? QStringLiteral("剪贴板内容不可用") : err, "warn");
        return;
    }
    clipPastePayload_ = std::move(p);
    clipPastePending_ = true;
    clipPasteAsBlock_ = asBlock;
    model_->setActiveTool(QStringLiteral("select"));
    view_->setPointPickMode(true);
    model_->log(asBlock ? QStringLiteral("粘贴为块：请单击指定插入点")
                        : QStringLiteral("粘贴：请单击指定落点（剪贴板基点将对齐该点）"));
}

void MainWindow::handleClipPick(const Vec3& p) {
    if (clipBasePending_) {
        // 带基点复制：拾取点即基点
        clipBasePending_ = false;
        if (view_->pointPickMode()) view_->setPointPickMode(false);
        std::vector<Element> src = collectSelectedElements(model_);
        if (src.empty()) { model_->log(QStringLiteral("无可复制图元"), "warn"); return; }
        clipboard::Payload pay;
        pay.base = p;
        pay.elements = std::move(src);
        pay.blocks = clipboard::collectBlockDeps(*model_, pay.elements);
        if (!clipboard::putToClipboard(pay)) { model_->log(QStringLiteral("写入剪贴板失败"), "warn"); return; }
        model_->log(QStringLiteral("已复制 %1 个图元到剪贴板（基点 %2,%3）")
                        .arg(pay.elements.size()).arg(pay.base.x, 0, 'f', 2).arg(pay.base.y, 0, 'f', 2));
        return;
    }

    // 粘贴落点：把暂存载荷粘贴到拾取点
    clipboard::Payload pay = std::move(clipPastePayload_);
    const bool asBlock = clipPasteAsBlock_;
    finishClipPick();
    model_->beginEdit(asBlock ? QStringLiteral("剪贴板-粘贴为块") : QStringLiteral("剪贴板-粘贴"));
    QString msg;
    if (!clipboard::pasteInto(model_, pay, p, asBlock, &msg)) {
        model_->undo();   // 空转回滚：粘贴失败不留 beginEdit 快照
        model_->log(msg.isEmpty() ? QStringLiteral("粘贴失败") : msg, "warn");
        return;
    }
    model_->log(msg);
}

void MainWindow::finishClipPick() {
    clipBasePending_ = false;
    clipPastePending_ = false;
    clipPasteAsBlock_ = false;
    clipPastePayload_ = clipboard::Payload{};
    if (view_->pointPickMode()) view_->setPointPickMode(false);
}

// 材料类别 → 中文标签（供日志/提示使用；与 SectionDialog 的可选项一致）。
static const char* sectionMaterialLabel(SectionMaterial m) {
    switch (m) {
    case SectionMaterial::General:    return "通用金属（45°细实线）";
    case SectionMaterial::CastIron:   return "铸铁（网状）";
    case SectionMaterial::Steel:      return "钢";
    case SectionMaterial::Aluminum:   return "铝（反向 45°）";
    case SectionMaterial::Wood:       return "木材（顺纹）";
    case SectionMaterial::Glass:      return "玻璃（45°长短画线）";
    case SectionMaterial::Liquid:     return "液体（水平短画线）";
    case SectionMaterial::Concrete:   return "混凝土（交叉线+点）";
    case SectionMaterial::NonMetal:   return "非金属（交叉网格）";
    case SectionMaterial::Brick:      return "砖墙（错缝砌体）";
    case SectionMaterial::Stone:      return "石材墙面（块石）";
    case SectionMaterial::Insulation: return "保温层（点阵）";
    case SectionMaterial::Plywood:    return "胶合板（交叉层）";
    case SectionMaterial::CoilWinding: return "线圈绕组元件（45°线+点）";
    case SectionMaterial::LaminatedSteel: return "叠钢片（薄45°线）";
    case SectionMaterial::Sand:       return "型砂/粉末冶金等（点阵）";
    case SectionMaterial::Soil:       return "基础周围泥土（疏点）";
    case SectionMaterial::ReinforcedConcrete: return "钢筋混凝土（45°线+点）";
    case SectionMaterial::Mesh:       return "筛网/过滤网（网格）";
    }
    return "通用金属";
}

// ============================ 剖面填充（区域选择模式） ============================
//
// 交互：菜单【修改>剖面填充】→ 进入区域选择模式（十字光标）→ 在要填充的封闭区域内
// 单击 → 程序自动解析全图边界、找出"包含该点的最内层闭环"并只填充该区域；
// 可连续点击多个区域；Enter 调参数、Esc / 右键退出。
//
// 为什么不是"先选图元再填充"：手工逐个选中构成边界的图元既繁琐又易错（漏选一段
// 直线、把相邻区域的多段线一并选上 → 填充到隔壁）。点选模式下"点在哪就填哪"，
// 边界由几何闭合关系自动推导，从交互层面消除了选错的可能。

void MainWindow::sectionFill() {
    // 已是拾取模式 → 再次点击菜单视为退出（开关式，便于键盘操作）。
    if (view_->regionPickMode()) {
        exitRegionPick(QStringLiteral("已退出剖面填充"));
        return;
    }
    // 先切回选择工具：避免残留绘制命令（绘制命令与点选模式互斥，切换工具会互相打断）
    model_->setActiveTool(QStringLiteral("select"));
    view_->setRegionPickMode(true);
    pickLoopsValid_ = false;
    pickHlLoop_ = -2;
    view_->setRegionHighlight({});
    model_->log(QStringLiteral("剖面填充：请在要填充的封闭区域内部单击（可连续点击多个区域）；"
                               "Enter 调整参数（角度/间距/材料）、Esc 或右键退出。"));
}

void MainWindow::exitRegionPick(const QString& logMsg) {
    view_->setRegionPickMode(false);      // 内部会清除高亮并复位光标
    pickLoopsValid_ = false;
    pickHlLoop_ = -2;
    if (!logMsg.isEmpty()) model_->log(logMsg);
}

void MainWindow::refreshPickLoopSet() {
    if (pickLoopsValid_) return;
    pickLoops_ = buildSectionLoopSet(model_->elements());
    pickLoopsValid_ = true;
}

void MainWindow::onRegionHover(double x, double y) {
    refreshPickLoopSet();
    const RegionPickResult pick = pickRegionAt(pickLoops_, Vec3(x, y, 0));
    // 只在"命中的闭环发生变化"时刷新高亮，避免鼠标每移动 1px 都重绘整幅视图。
    if (pick.loopIndex == pickHlLoop_) return;
    pickHlLoop_ = pick.loopIndex;

    std::vector<std::vector<Vec3>> hl;
    if (pick.found) {
        hl.push_back(pick.boundary);
        for (const auto& h : pick.holes) hl.push_back(h);   // 内孔一并高亮（奇偶规则掏空）
    }
    view_->setRegionHighlight(hl);
}

void MainWindow::onRegionPickRequested(double x, double y) {
    const Vec3 p(x, y, 0);
    refreshPickLoopSet();
    const RegionPickResult pick = pickRegionAt(pickLoops_, p);

    if (!pick.found) {
        // 分类提示：让用户知道"为什么没填"。只有"全图根本没有可填充的闭合区域"才弹模态
        // 对话框（此时点选模式无从下手）；"点到空白处""已填充""退化"属正常操作噪声，
        // 只写日志，避免每次误点都打断操作。
        QString msg;
        bool modal = false;
        switch (pick.reason) {
        case RegionPickReason::AlreadyFilled:
            msg = QStringLiteral("该处已经是剖面区域，无需重复填充"
                                 "（如需修改请先撤销 Ctrl+Z 或删除该剖面区域）。");
            break;
        case RegionPickReason::Degenerate:
            msg = QStringLiteral("该处区域已退化（净面积≈0，例如被内孔完全占满），无法填充。");
            break;
        case RegionPickReason::NoLoopAtPoint:
        default:
            if (pick.totalLoops == 0) {
                modal = true;
                msg = QStringLiteral("当前图中没有找到任何封闭区域，无法填充。\n\n"
                                     "封闭边界可由圆 / 椭圆 / 矩形 / 多边形 / 闭合多段线直接构成，"
                                     "或由直线、圆弧、开放多段线、开放样条首尾相接围成"
                                     "（端点相距 ≤ 0.5mm 视为相连）。");
            } else {
                msg = QStringLiteral("点击处不在任何封闭区域内（全图已识别 %1 个封闭区域）。")
                          .arg(pick.totalLoops);
            }
            break;
        }
        model_->log(QStringLiteral("剖面填充：%1").arg(msg.section('\n', 0, 0)), "warn");
        if (modal) QMessageBox::information(this, QStringLiteral("剖面填充"), msg);
        return;                     // 保持拾取模式，用户可直接换位置再点
    }

    // 生成动作：仅"独占"的边界源图元被消费——第一个独占图元原地替换为 Hatch（复用 ID），
    // 其余独占图元合并删除（几何已写入 Hatch）；与相邻区域共享的边界图元一律保留，
    // 避免填充本区域时把相邻区域一并裁剪掉（"另一部分被一并裁掉"缺陷）。
    const auto result = makeSectionChangeForPick(pick, model_->elements(), sectionStyle_);
    if (!result.anySuccess()) {
        model_->log(QStringLiteral("剖面填充：该区域无法生成剖面线，已跳过"), "warn");
        return;
    }

    model_->beginEdit(QStringLiteral("剖面填充"));
    const std::vector<int> newIds = modify::applyChanges(model_, result.changes, QString());
    const int changedId = result.changes.empty() ? -1 : result.changes.front().id;
    if (changedId > 0)          model_->setSelection({ changedId });      // 复用原 ID：选中态不跳
    else if (!newIds.empty())   model_->setSelection({ newIds.front() }); // 纯新增：选中新 Hatch

    // 模型已变化 → 缓存失效（下次点击/悬停按新图重新解析），并清除旧高亮。
    pickLoopsValid_ = false;
    pickHlLoop_ = -2;
    view_->setRegionHighlight({});

    model_->log(QStringLiteral("剖面填充：已填充 1 个区域（边界 %1 段%2；角度 %3°，间距 %4mm）")
                    .arg(pick.boundary.size())
                    .arg(pick.holes.empty()
                             ? QString()
                             : QStringLiteral("，内孔 %1 个已掏空").arg(pick.holes.size()))
                    .arg(sectionStyle_.angleDeg, 0, 'f', 1)
                    .arg(sectionStyle_.spacingMm, 0, 'f', 2));
}

void MainWindow::onRegionPickCancelled() {
    if (view_->regionPickMode()) view_->setRegionPickMode(false);
    pickLoopsValid_ = false;
    pickHlLoop_ = -2;
    model_->log(QStringLiteral("剖面填充：已退出区域选择"));
}

void MainWindow::onRegionPickParams() {
    SectionDialog dlg(SectionDialog::Result{ sectionStyle_, true }, this);
    if (dlg.exec() != QDialog::Accepted) return;
    sectionStyle_ = dlg.result().style;
    model_->log(QStringLiteral("剖面填充参数：角度 %1°、间距 %2mm（继续在区域内单击即可填充）")
                    .arg(sectionStyle_.angleDeg, 0, 'f', 1)
                    .arg(sectionStyle_.spacingMm, 0, 'f', 2));
}

// 【修改>剖面填充参数】：对当前选中的 Hatch 图元（已填充的剖面区域）就地重设
// 材料类别 / 豁免 / 角度 / 间距，实现"换填充类型"。
//   * 无选中 Hatch 时，只把参数存为默认（下次点选填充即生效），并给出提示；
//   * 有选中 Hatch 时，逐一无损改写（保留边界/内孔/颜色/图层/线宽），一次 Ctrl+Z 回滚。
// 关键：不要重建图元（会丢 holes/边界），只改 style 字段并写回即可。
void MainWindow::sectionFillParams() {
    SectionDialog dlg(SectionDialog::Result{ sectionStyle_, true }, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const SectionStyle st = dlg.result().style;
    sectionStyle_ = st;   // 同时更新默认样式：后续点选填充沿用本次选择

    // 收集当前选中的 Hatch 图元
    std::vector<int> hatchIds;
    for (int id : model_->selection()) {
        if (const Element* e = model_->element(id))
            if (e->type == ElementType::Hatch) hatchIds.push_back(id);
    }

    if (hatchIds.empty()) {
        model_->log(QStringLiteral("剖面填充参数已设为：%1（角度 %2°、间距 %3mm）；"
                                   "下次点选填充即按此生效。若想改已填充区域，请先选中该剖面区域。")
                        .arg(QString::fromUtf8(sectionMaterialLabel(st.material)))
                        .arg(st.angleDeg, 0, 'f', 1)
                        .arg(st.spacingMm, 0, 'f', 2));
        return;
    }

    model_->beginEdit(QStringLiteral("剖面填充参数"));
    int n = 0;
    for (int id : hatchIds) {
        const Element* src = model_->element(id);
        if (!src || src->type != ElementType::Hatch) continue;
        Element e = *src;                          // 复制：保留 boundary/holes/颜色/图层/线宽
        e.sectionMaterial = (int)st.material;
        e.sectionExempt   = (int)st.exempt;
        e.rotation        = st.angleDeg;
        e.radius          = st.spacingMm;
        model_->updateElementQuiet(id, [&e](Element& dst) {
            dst.sectionMaterial = e.sectionMaterial;
            dst.sectionExempt   = e.sectionExempt;
            dst.rotation        = e.rotation;
            dst.radius          = e.radius;
        });
        ++n;
    }
    model_->log(QStringLiteral("已把 %1 个剖面区域改为：%2（角度 %3°、间距 %4mm）")
                    .arg(n)
                    .arg(QString::fromUtf8(sectionMaterialLabel(st.material)))
                    .arg(st.angleDeg, 0, 'f', 1)
                    .arg(st.spacingMm, 0, 'f', 2));
}

// 【修改>用选中边界填充】：与「点选区域填充」互补的「先选后填」路径。
// 选中若干图元（边界/闭合形状）后一次性生成剖面区域：
//   * 边界(BOUNDARY/BPOLY) 经 makeSectionChangesEx 的专用分支**直接复用**
//     其外环/内孔/样式，跳过剖面引擎的自动边界追踪——这正是解除「单一依赖自动追踪」的关键；
//   * 其余闭合图元（圆/矩形/多边形/闭合折线/样条）走原有自动追踪路径。
void MainWindow::sectionFillSelected() {
    std::vector<Element> src;
    for (int id : model_->selection())
        if (const Element* e = model_->element(id)) src.push_back(*e);
    if (src.empty()) {
        model_->log(QStringLiteral("请先选中要填充的图元（边界/闭合形状），再执行「用选中边界填充」"), "warn");
        return;
    }
    const auto res = makeSectionChangesEx(src, sectionStyle_, true);
    if (res.changes.empty()) {
        model_->log(QStringLiteral("选中图元中无可填充的闭合区域（边界需 ≥3 点外环，或选中圆/矩形/多边形/闭合折线）"), "warn");
        return;
    }
    model_->beginEdit(QStringLiteral("剖面填充（选中）"));
    const std::vector<int> newIds = modify::applyChanges(model_, res.changes, QString());
    int picked = -1;
    for (const auto& c : res.changes)
        if (!c.remove) { picked = (c.id > 0 ? c.id : -1); break; }
    if (picked > 0)          model_->setSelection({ picked });
    else if (!newIds.empty()) model_->setSelection({ newIds.front() });
    model_->log(QStringLiteral("已填充 %1 个区域（跨 %2 个选中的闭合图元）")
                    .arg(res.filledRegions).arg((int)src.size()));
}

// 【修改>边界区域管理】：打开 REGION 管理对话框。REGION = 共享同一 name 的若干 Boundary，
// 提供命名分组、批量填充、整体选择、重命名、删除，使复杂腔体一次填充即可、无需逐段描边。
void MainWindow::openRegionManager() {
    RegionManagerDialog dlg(model_, this);
    dlg.exec();
}

// 【修改>图形清理】：PURGE（垃圾/冗余数据）+ OVERKILL（重合删除 / 共线重叠合并）。
// 执行前后输出图元数量对比统计；图元增删一次撤销，块/图层清除各自独立撤销。
void MainWindow::openCleanup() {
    CleanupDialog dlg(model_, this);
    dlg.exec();
}

// P0-01：DXF 导出版本选择（R12 默认，R2000 可编辑性更好）。返回用户选择的版本。
cadfmt::DxfVersion MainWindow::chooseDxfVersion() {
    QStringList items;
    items << QStringLiteral("R12 (AC1009) —— 兼容性最佳，所有 CAD 均可打开")
          << QStringLiteral("R2000 (AC1015) —— 原生实体带 AcDb 标记/句柄，可编辑性更好");
    bool ok = false;
    QString sel = QInputDialog::getItem(this, QStringLiteral("选择 DXF 版本"),
                                        QStringLiteral("导出 DXF 时使用的 AutoCAD 版本："),
                                        items, 0, false, &ok);
    if (!ok || sel.startsWith(QStringLiteral("R12"))) return cadfmt::DxfVersion::R12;
    return cadfmt::DxfVersion::R2000;
}

void MainWindow::exportDxf() {
    const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出 DXF"),
                                                    QStringLiteral("sketch.dxf"),
                                                    QStringLiteral("DXF (*.dxf)"));
    if (fn.isEmpty()) return;
    QString err;
    // P0-01：导出版本由用户在下拉框选择（默认 R12，逐字节兼容旧档）
    cadfmt::DxfExportOptions no;
    no.nativeBlocks = no.nativeDims = no.nativeHatch = true;
    no.version = chooseDxfVersion();
    const QByteArray body = cadfmt::toDxf(model_->elements(), &err, model_->scaleRatio(),
                                          4, &model_->blockTable(), &no);
    if (body.isEmpty()) { model_->log(QStringLiteral("导出失败：") + err, "error"); return; }
    QFile f(fn);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(body);                       // 原样写盘：DXF 可能是 GBK 字节
        model_->log(QStringLiteral("已导出 DXF：") + fn);
    } else {
        model_->log(QStringLiteral("导出失败（无法写入）：") + fn, "error");
    }
}

QJsonObject MainWindow::elementToJson(const Element& e) {
    // 单一实现已下沉到核心层（SketchCAD::elementToJson，见 Element.cpp），
    // 顶层图纸与块定义内图元共用同一份字段表，避免两处漂移。
    return ::SketchCAD::elementToJson(e);
}

Element MainWindow::elementFromJson(const QJsonObject& o) {
    return ::SketchCAD::elementFromJson(o);
}

// ===========================================================================
// 界面定制（CUIx + 命令别名）：工具条 / 工作空间 / 别名文件
// 设计要点见 docs/cui-alias-design-analysis.md
// ===========================================================================
void MainWindow::initCui() {
    CuiManager& cm = CuiManager::instance();
    // 注册内置工具条与顶级菜单（供工作空间「默认全显」与「保存当前视图」）
    cm.registerBuiltinToolbar("mainToolBar");
    cm.registerBuiltinToolbar("viewToolBar");
    const QStringList menus = {
        "menuFile", "menuPrint", "menuTrack", "menuEdit", "menuModify",
        "menuSelect", "menuDraw", "menuInquiry", "menuView", "menuScript",
        "menuPlugin", "menuHelp", "menuCui"
    };
    for (const QString& m : menus) cm.registerBuiltinMenu(m);
    // 命令改动界面配置后请求实时刷新（避免 Qt 信号耦合进纯 C++ 核心层）
    cm.setRefreshCallback([this] { applyCui(); });

    // 命令别名：载入默认 .pgp（不存在则注入默认别名并落盘）
    QString ap = QCoreApplication::applicationDirPath() + "/alias.pgp";
    if (QFile::exists(ap)) {
        QString err; AliasTable::instance().load(ap, &err);
    } else {
        auto d = AliasTable::defaultAliases();
        for (auto it = d.begin(); it != d.end(); ++it)
            AliasTable::instance().setAlias(it.key(), it.value());
        AliasTable::instance().save(ap);
    }
    AliasTable::instance().setFilePath(ap);

    // 界面配置：存在则载入默认 .cuix（否则保持内置 UI 全显）
    QString cp = QCoreApplication::applicationDirPath() + "/sketchcad.cuix";
    if (QFile::exists(cp)) { QString err; CuiManager::instance().load(cp, &err); }

    applyCui();
}

void MainWindow::runToken(const QString& cmd) {
    // 工具 id（如 line/circle）→ 进入对应绘制模式；其余按命令执行。
    if (ToolRegistry::instance().find(cmd)) model_->setActiveTool(cmd);
    else interp_->execute(cmd);
}

void MainWindow::applyCui() {
    rebuildCustomToolbars();
    applyWorkspaceVisibility();
}

void MainWindow::rebuildCustomToolbars() {
    // 先移除旧自定义工具条（整体替换，避免重复累积）
    for (QToolBar* tb : customToolbars_) { removeToolBar(tb); tb->deleteLater(); }
    customToolbars_.clear();

    const CuiManager& cm = CuiManager::instance();
    for (const CuiToolbar& t : cm.toolbars()) {
        QToolBar* tb = addToolBar(t.title.isEmpty() ? t.id : t.title);
        tb->setObjectName(t.id);
        applyToolBarLook(tb);
        for (const CuiButton& b : t.buttons) {
            QString label = b.label;
            if (label.isEmpty()) {
                const ToolSpec* spec = ToolRegistry::instance().find(b.command);
                label = spec ? spec->displayName : b.command.toUpper();
            }
            // 图标：显式指定 > 工具默认（以命令 id 作图标） > 兜底属性图标
            QString icon = b.icon.isEmpty()
                ? (ToolRegistry::instance().find(b.command) ? b.command : QStringLiteral("properties"))
                : b.icon;
            QAction* a = new QAction(icons::make(icon), label, this);
            a->setToolTip(label);
            a->setStatusTip(label);
            connect(a, &QAction::triggered, this, [this, cmd = b.command] { runToken(cmd); });
            tb->addAction(a);
        }
        customToolbars_.append(tb);
    }
}

void MainWindow::applyWorkspaceVisibility() {
    const CuiManager& cm = CuiManager::instance();
    // 工具条可见性（内置 + 自定义，均按 objectName 判定）
    for (QToolBar* tb : findChildren<QToolBar*>())
        tb->setVisible(cm.toolbarVisible(tb->objectName()));
    // 菜单可见性（按顶级菜单 objectName 判定）。
    // ⚠️ 必须作用于菜单栏上的 QAction，而不是 QMenu 本体：
    // 对 QMenu 调 setVisible(true) 会把它当作弹出窗口立即弹出（多个菜单级联堆叠
    // 在窗口左上角）——启动时 initCui→applyCui 逐个「弹出」全部顶级菜单，正是
    // 启动界面菜单堆叠/重叠凌乱的根因。改控 QAction 可见性：只增删菜单栏入口，
    // 菜单本体保持隐藏、仅在用户点击时弹出（启动时默认隐藏、需要时才显示）。
    for (QAction* a : menuBar()->actions())
        if (QMenu* m = a->menu())
            a->setVisible(cm.menuVisible(m->objectName()));
}

void MainWindow::openAliasFile() {
    QString ap = AliasTable::instance().filePath();
    if (ap.isEmpty()) ap = QCoreApplication::applicationDirPath() + "/alias.pgp";
    if (!QFile::exists(ap)) {
        auto d = AliasTable::defaultAliases();
        for (auto it = d.begin(); it != d.end(); ++it)
            AliasTable::instance().setAlias(it.key(), it.value());
        AliasTable::instance().save(ap);
    }
    AliasTable::instance().setFilePath(ap);
    QDesktopServices::openUrl(QUrl::fromLocalFile(ap));
    model_->log(QStringLiteral("已用系统编辑器打开别名文件：%1（编辑后点「重新加载命令别名」生效）").arg(ap));
}

void MainWindow::newToolbarDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新建自定义工具条"));
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(QStringLiteral("工具条 id（如 mybar），用作 objectName"));
    auto* titleEdit = new QLineEdit(&dlg);
    titleEdit->setPlaceholderText(QStringLiteral("显示标题（留空则用 id）"));
    auto* body = new QPlainTextEdit(&dlg);
    body->setPlaceholderText(QStringLiteral("每行一个按钮：命令 [文字] [图标]\n示例：\nline 直线 line\ncircle 圆 circle\nzoom 缩放 zoom"));
    body->setMinimumSize(440, 240);
    auto* ok = new QPushButton(QStringLiteral("确定"), &dlg);
    auto* cancel = new QPushButton(QStringLiteral("取消"), &dlg);
    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("工具条 id："), nameEdit);
    form->addRow(QStringLiteral("标题："), titleEdit);
    form->addRow(QStringLiteral("按钮："), body);
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(); btnRow->addWidget(ok); btnRow->addWidget(cancel);
    auto* v = new QVBoxLayout(&dlg);
    v->addLayout(form); v->addLayout(btnRow);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    QString id = nameEdit->text().trimmed().toLower();
    if (id.isEmpty()) { model_->log(QStringLiteral("工具条 id 不能为空"), "warn"); return; }
    QString title = titleEdit->text().trimmed();
    CuiManager::instance().addToolbar(id, title.isEmpty() ? id : title);
    int n = 0;
    for (const QString& raw : body->toPlainText().split('\n', Qt::SkipEmptyParts)) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        CuiButton b;
        b.command = parts[0].toLower();
        b.label = parts.size() > 1 ? parts[1] : QString();
        b.icon = parts.size() > 2 ? parts[2] : QString();
        CuiManager::instance().addButton(id, b);
        ++n;
    }
    CuiManager::instance().requestRefresh();
    model_->log(QStringLiteral("已新建自定义工具条：%1（%2 个按钮）")
                    .arg(title.isEmpty() ? id : title).arg(n));
}

void MainWindow::saveCurrentAsWorkspaceDialog() {
    bool ok = false;
    QString id = QInputDialog::getText(this, QStringLiteral("新建工作空间"),
        QStringLiteral("工作空间 id："), QLineEdit::Normal, QString(), &ok);
    if (!ok || id.trimmed().isEmpty()) return;
    id = id.trimmed().toLower();

    // 以当前实际可见的 UI 作为该工作空间的可见集（精确快照）
    // 菜单口径与 applyWorkspaceVisibility 一致：读菜单栏 QAction 的可见性
    // （QMenu 本体平时始终隐藏，只有被点击才弹出，不能作为快照依据）
    QStringList tbs, menus;
    for (QToolBar* tb : findChildren<QToolBar*>())
        if (tb->isVisible()) tbs << tb->objectName();
    for (QAction* a : menuBar()->actions())
        if (QMenu* m = a->menu())
            if (a->isVisible()) menus << m->objectName();

    CuiManager::instance().addWorkspace(id, id, tbs, menus);
    CuiManager::instance().setCurrentWorkspace(id);
    QString cp = QCoreApplication::applicationDirPath() + "/sketchcad.cuix";
    QString err; CuiManager::instance().save(cp, &err);
    applyCui();
    model_->log(QStringLiteral("已保存并切换到工作空间：%1（%2 工具条 / %3 菜单）")
                    .arg(id).arg(tbs.size()).arg(menus.size()));
}

} // namespace SketchCAD
