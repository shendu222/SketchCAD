#include "MainWindow.h"
#include <QSet>
#include "core/SketchModel.h"
#include "core/CommandInterpreter.h"
#include "core/Element.h"
#include "core/Geometry.h"
#include "core/DimStyle.h"
#include "core/DimensionStandard.h"
#include "core/Vec.h"
#include "core/ToolRegistry.h"
#include "ui/GraphicsView.h"
#include "ui/ModelMediator.h"
#include "ui/RulerWidget.h"
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
#include "ui/SectionDialog.h"
#include "core/Section.h"
#include "core/SheetSpec.h"
#include "core/Modify.h"
#include "core/CadFormats.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>
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
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include <QPointF>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {

// 工具条按钮统一最小宽度（px）：图标 20 + 文字 2 汉字 + 内边距，
// 保证 1~2 个汉字的短按钮与长按钮对齐时不参差不齐（见 MainWindow::uniformButtonWidth）。
static const int kToolButtonMinWidth = 64;

MainWindow::MainWindow(QWidget* p) : QMainWindow(p) {
    model_ = new SketchModel(this);
    interp_ = new CommandInterpreter(model_);
    view_ = new GraphicsView(model_, this);

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

    // 标尺必须在 connectSignals() 之前创建，否则信号连接时 hRuler_/vRuler_ 为 nullptr
    hRuler_ = new RulerWidget(view_, RulerWidget::Orientation::Horizontal);
    vRuler_ = new RulerWidget(view_, RulerWidget::Orientation::Vertical);

    connectSignals();
    buildParamBar();

    // 中央区 = [参数输入条] 在上 + [标尺 + 图形视图] 在下
    auto* center = new QWidget(this);
    auto* cvl = new QVBoxLayout(center);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);
    cvl->addWidget(paramBar_);

    // 标尺 + 图形视图：用 QGridLayout 拼成十字布局
    //   [角落] [水平标尺]
    //   [垂直标尺] [GraphicsView]
    auto* corner = new QWidget();
    corner->setFixedSize(24, 24);
    corner->setAutoFillBackground(true);

    auto* rulerGrid = new QGridLayout();
    rulerGrid->setContentsMargins(0, 0, 0, 0);
    rulerGrid->setSpacing(0);
    rulerGrid->addWidget(corner, 0, 0);
    rulerGrid->addWidget(hRuler_, 0, 1);
    rulerGrid->addWidget(vRuler_, 1, 0);
    rulerGrid->addWidget(view_, 1, 1);
    rulerGrid->setColumnStretch(1, 1);
    rulerGrid->setRowStretch(1, 1);

    cvl->addLayout(rulerGrid, 1);
    setCentralWidget(center);

    statusBar_->setCount(0);
    statusBar_->setPlaneInfo(planeName(model_->plane()));
    statusBar_->setDrawScale(model_->scaleLabel());
    model_->log(QStringLiteral("航空CAD 草图模块已启动 (C++17 / Qt6)"));

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

    QMenu* f = new QMenu(QStringLiteral("文件(&F)"), this);
    mk(f, QStringLiteral("新建"),        QKeySequence::New,  [this] {
            NewDrawingDialog dlg(this);
            if (dlg.exec() != QDialog::Accepted) return;
            // 参数确定后交给可测接缝执行（对话框本身在离屏测试里无法驱动）
            applyNewDrawing(dlg.selectedSpec(), dlg.selectedScaleRatio());
        })
        ->setIcon(icons::make("new"));
    mk(f, QStringLiteral("填写标题栏..."), QKeySequence(), [this] {
            TitleBlockForm dlg(model_, this);
            dlg.exec();
        });
    mk(f, QStringLiteral("插入标准标题栏..."), QKeySequence(), [this] {
            insertStandardTitleBlock();
        });
    mk(f, QStringLiteral("按幅面生成图框与标题栏..."), QKeySequence(), [this] {
            generateFramedTitleBlock();
        });
    // 绘图比例属于「图纸属性」而非视图属性，故放在文件菜单（GB/T 14690 技术制图 比例）
    mk(f, QStringLiteral("绘图比例..."), QKeySequence("Ctrl+Shift+S"), [this] {
            changeDrawingScale();
        });
    mk(f, QStringLiteral("打开..."),      QKeySequence::Open, [this] { openFile(); })
        ->setIcon(icons::make("open"));
    mk(f, QStringLiteral("保存..."),      QKeySequence::Save, [this] { saveFile(); })
        ->setIcon(icons::make("save"));
    f->addSeparator();
    mk(f, QStringLiteral("导出 DXF..."),  QKeySequence("Ctrl+E"), [this] { exportDxf(); })
        ->setIcon(icons::make("export"));
    f->addSeparator();
    mk(f, QStringLiteral("退出"),        QKeySequence::Quit, [this] { close(); });
    bar->addMenu(f);

    // 顶级菜单整合后，【修改】【选择】不再各占一个顶级入口，而是作为【编辑】的子菜单
    // （下方构建完成后用 e->addMenu(...) 挂进来）。显示顺序 = 添加顺序：
    //   撤销 / 重做 | 复制 / 粘贴 / 删除 | 修改 | 选择
    QMenu* e = new QMenu(QStringLiteral("编辑(&E)"), this);
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
    bar->addMenu(e);

    // ---- 新增顶级菜单：修改（Modify）—— 与参考附图（LibreCAD Modify 菜单）逐项对应 ----
    // 23 个图元修改 / 几何编辑 / 工具动作，覆盖：
    //   变换类（Move/Copy, Duplicate, Align, Rotate, Scale, Mirror, Move-Rotate,
    //           Rotate Two, Revert Direction）
    //   修剪类（Trim, Trim Two, Lengthen, Line Join, Break/Divide, Line Gap）
    //   几何构造（Offset, Bevel, Fillet, Divide, Stretch）
    //   工具与分解（Properties, Attributes, Explode Text into Letters, Explode, Delete）
    // 已存在的工具型菜单项（trim/extend）复用 toolActions_ 指针，其余走 mk() 即时构造。
    QMenu* mod = new QMenu(QStringLiteral("修改(&M)"), this);

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
    auto* arrayMenu = mod->addMenu(icons::make("duplicate"), QStringLiteral("阵列(&R)"));
    mk(arrayMenu, QStringLiteral("矩形阵列..."), QKeySequence(), [this] { arrayRectangular(); })
        ->setIcon(icons::make("duplicate"));
    mk(arrayMenu, QStringLiteral("环形阵列..."), QKeySequence(), [this] { arrayPolar(); })
        ->setIcon(icons::make("rotate"));

    // ---- 剖面填充（GB/T 4457.5-2013 技术制图 剖面区域的表示法）----
    // 把选中的闭合图元（圆/椭圆/矩形/多边形/闭合折线）转成剖面区域并套用标准剖面线规则。
    mk(mod, QStringLiteral("剖面填充(&H)..."), QKeySequence("Ctrl+Shift+H"), [this] { sectionFill(); })
        ->setIcon(icons::make("hatch"));

    auto* alignMenu = mod->addMenu(icons::make("align"), QStringLiteral("对齐(&A)"));
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
        // 等价于 fillet id1 id2 0
        int id1 = model_->selection().size() >= 1 ? model_->selection()[0] : -1;
        int id2 = model_->selection().size() >= 2 ? model_->selection()[1] : -1;
        if (id1 < 0 || id2 < 0) { model_->log(QStringLiteral("倒角需要 2 条选中直线"), "warn"); return; }
        std::vector<Element> pool = model_->elements();
        auto changes = modify::makeFillet(id1, id2, 0.0, pool);
        if (changes.empty()) { model_->log(QStringLiteral("倒角失败（两线平行或不相交）"), "warn"); return; }
        model_->beginEdit(QStringLiteral("修改-倒角"));
        auto added = modify::applyChanges(model_, changes, QString());
        QStringList ids;
        for (int id : added) ids << ("#" + QString::number(id));
        model_->log(QStringLiteral("已对 #%1 与 #%2 做倒角，新增 %3：%4")
                        .arg(id1).arg(id2).arg(added.size()).arg(ids.join(", ")));
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
        QMenu* blk = mod->addMenu(icons::make("block"), QStringLiteral("块(&B)"));
        buildBlockMenu(blk);
    }

    e->addSeparator();
    // 挂到【编辑】菜单下（不再作为顶级菜单），保持「编辑类功能一个入口」
    e->addMenu(mod);

    // ---- 新增顶级菜单：选择（与附图 LibreCAD Select 菜单逐项对应）----
    // 结构：全部不选 / 全选 / 选择实体 / (取消)选择轮廓 / 窗口选择 / 窗口取消 /
    //       选择相交 / 取消相交 / (取消)选择图层 / 反选
    // 注意：工具型菜单项直接复用 toolActions_ 中已注册的 QAction（与工具条下拉
    // 共享高亮/互斥状态），即时命令（全选等）由 mk() 单独构造。
    QMenu* sel = new QMenu(QStringLiteral("选择(&N)"), this);
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
    addSel("toggle_layer");               // (取消)选择图层
    sel->addSeparator();
    mk(sel, QStringLiteral("反选"),     QKeySequence("Ctrl+Shift+I"), [this] { model_->invertSelection(); })
        ->setIcon(icons::make("invert_selection"));
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
    QMenu* s = new QMenu(QStringLiteral("绘图(&D)"), this);
    buildSketchMenu(s);
    bar->addMenu(s);

    QMenu* v = new QMenu(QStringLiteral("视图(&V)"), this);
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
    connect(model_, &SketchModel::gridToggled, aGrid, &QAction::setChecked);

    // 绘图比例入口已迁至【文件】菜单（属图纸属性），此处不再重复提供，避免同一功能两处出现

    QMenu* plane = new QMenu(QStringLiteral("视图方向"), this);
    mk(plane, QStringLiteral("XY 平面（俯视）"), QKeySequence("Ctrl+1"), [this] { model_->setPlane(Plane::XY); })
        ->setIcon(icons::make("plane_xy"));
    mk(plane, QStringLiteral("YZ 平面（左视）"), QKeySequence("Ctrl+2"), [this] { model_->setPlane(Plane::YZ); })
        ->setIcon(icons::make("plane_yz"));
    mk(plane, QStringLiteral("XZ 平面（主视）"), QKeySequence("Ctrl+3"), [this] { model_->setPlane(Plane::XZ); })
        ->setIcon(icons::make("plane_xz"));
    v->addMenu(plane);
    v->addSeparator();

    // 面板显隐切换：关闭后可在此重新勾选（QDockWidget::toggleViewAction 自带勾选态同步）
    QMenu* panels = new QMenu(QStringLiteral("面板"), this);
    panels->addAction(bomDock_->toggleViewAction());
    panels->addAction(elemDock_->toggleViewAction());
    panels->addAction(layerDock_->toggleViewAction());
    panels->addAction(propDock_->toggleViewAction());
    panels->addAction(logDock_->toggleViewAction());
    panels->addAction(cmdDock_->toggleViewAction());
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
    bar->addMenu(v);

    QMenu* h = new QMenu(QStringLiteral("帮助(&H)"), this);
    mk(h, QStringLiteral("命令列表"), QKeySequence("F1"), [this] { interp_->execute("help"); });
    mk(h, QStringLiteral("关于"), QKeySequence(), [this] {
        model_->log(QStringLiteral("航空CAD 草图模块 v1.0 — C++17 / Qt6 实现")); });
    bar->addMenu(h);
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
    m->addSeparator();
    add(m, QStringLiteral("块管理器..."), QKeySequence(),
        [this] { openBlockManagerDialog(); }, "block");
    add(m, QStringLiteral("分解块..."), QKeySequence(),
        [this] { explodeBlockFromMenu(); }, "explode");
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
    sub(QStringLiteral("圆(&C)"),     QStringLiteral("circle"),   &MainWindow::buildCircleMenu);
    sub(QStringLiteral("圆弧(&A)"),   QStringLiteral("arc"),      &MainWindow::buildArcMenu);
    sub(QStringLiteral("多边形(&P)"), QStringLiteral("poly"),     &MainWindow::buildPolyMenu);
    sub(QStringLiteral("样条(&S)"),   QStringLiteral("spline"),   &MainWindow::buildSplineMenu);
    sub(QStringLiteral("椭圆(&E)"),   QStringLiteral("ellipse"),  &MainWindow::buildEllipseMenu);
    sub(QStringLiteral("折线(&Y)"),   QStringLiteral("polyline"), &MainWindow::buildPolylineMenu);

    // 尺寸标注子菜单：GB/T 4458.4-2003 的各种注法集中在这里
    m->addSection(QStringLiteral("尺寸标注  GB/T 4458.4 / ISO 129"));
    dimMenu_ = m->addMenu(icons::make("dim"), QStringLiteral("尺寸标注"));
    for (QAction* a : dimActs_) if (a) dimMenu_->addAction(a);

    m->addSection(QStringLiteral("绘制选项"));
    m->addAction(actChain_);

    // 线宽子菜单（与工具条下拉框指向同一个 applyWidth）
    widthMenu_ = m->addMenu(icons::make("width"), QStringLiteral("线宽"));
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
        QMenu* blk = m->addMenu(icons::make("block"), QStringLiteral("块(&B)"));
        buildBlockMenu(blk);
    }

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
         "polyline_arcs_to_chords", "polyline_change_segment_type"}
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
            this, [this](double v) { model_->setLineParamLength(v); });
    hl->addWidget(sbLength_);

    lblSpacing_ = new QLabel(QStringLiteral("间隔"));
    hl->addWidget(lblSpacing_);
    sbSpacing_ = new QDoubleSpinBox(this);
    sbSpacing_->setRange(0.1, 1e6);
    sbSpacing_->setValue(model_->lineParams().spacing);
    sbSpacing_->setSuffix(QStringLiteral(" mm"));
    sbSpacing_->setFixedWidth(90);
    connect(sbSpacing_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { model_->setLineParamSpacing(v); });
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
        &lineSubModes(), &circleSubModes(), &arcSubModes(),
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

    // 左栏：上 BOM 树 / 中 图元面板 / 下 图层
    addDockWidget(Qt::LeftDockWidgetArea, bomDock_);
    splitDockWidget(bomDock_, elemDock_, Qt::Vertical);
    splitDockWidget(elemDock_, layerDock_, Qt::Vertical);
    // 右栏：上 属性面板 / 下 执行信息
    addDockWidget(Qt::RightDockWidgetArea, propDock_);
    splitDockWidget(propDock_, logDock_, Qt::Vertical);
    // 底部：命令行
    addDockWidget(Qt::BottomDockWidgetArea, cmdDock_);

    // 全部面板可拖拽移动、浮动、关闭（关闭后可从「视图 > 面板」重新勾选显示）
    for (auto* d : {bomDock_, elemDock_, propDock_, logDock_, cmdDock_, layerDock_})
        d->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable |
                       QDockWidget::DockWidgetFloatable);

    // 初始分栏比例：左 240 / 右 300；左栏上下各分
    resizeDocks({bomDock_, propDock_}, {240, 300}, Qt::Horizontal);
    resizeDocks({bomDock_, elemDock_, layerDock_}, {300, 200, 160}, Qt::Vertical);
    resizeDocks({propDock_, logDock_}, {380, 300}, Qt::Vertical);
}

void MainWindow::connectSignals() {
    // 模型↔视图↔面板↔状态栏↔标尺↔命令行 的全部信号接线收口到 ModelMediator（中介者，
    // 单一触发时机）：谁订阅谁、触发几次只在此定义，杜绝"面板自连接 vs 主窗口连接"的
    // 双份/遗漏。仅需触碰主窗口私有成员的反应以回调注入，纯组件联动由中介者直接接线。
    auto* mediator = new ModelMediator(this, {
        model_, view_, logPanel_, elemPanel_, bom_, propPanel_,
        statusBar_, hRuler_, vRuler_, cmdLine_
    });
    mediator->setHost({
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
    mediator->wire();

    // ---- 剖面填充：区域拾取模式接线（视图 → 主窗口）----
    connect(view_, &GraphicsView::regionPickRequested, this, &MainWindow::onRegionPickRequested);
    connect(view_, &GraphicsView::regionHoverMoved, this, &MainWindow::onRegionHover);
    connect(view_, &GraphicsView::regionPickCancelled, this, &MainWindow::onRegionPickCancelled);
    connect(view_, &GraphicsView::regionPickParamsRequested, this, &MainWindow::onRegionPickParams);
    // ---- 中心点拾取模式接线（阵列对话框「拾取中心点」）----
    // 注：该模式此前只置位、无人消费，左键取点从未回抛（功能实际不可用），在此补齐。
    connect(view_, &GraphicsView::canvasPointPicked, this,
            [this](double x, double y) { onCanvasPointPicked(Vec3(x, y, 0)); });
    connect(view_, &GraphicsView::pointPickCancelled, this, &MainWindow::onPointPickCancelled);

    // Ctrl+回车：把焦点交给命令行，便于纯键盘连续作业
    auto* focusCmd = new QAction(this);
    focusCmd->setShortcut(QKeySequence("Ctrl+Return"));
    focusCmd->setShortcutContext(Qt::ApplicationShortcut);
    connect(focusCmd, &QAction::triggered, this, [this] {
        cmdDock_->show();
        cmdLine_->setFocus(Qt::ShortcutFocusReason);
    });
    addAction(focusCmd);
}

// ---- 文件格式过滤器（另存为 / 打开）----
// 与 core/CadFormats.h 的 CadFormat 一一对应；扩展名是唯一的分派依据。
namespace {
QStringList saveNameFilters() {
    return {
        QStringLiteral("草图文件 (*.json)"),
        QStringLiteral("AutoCAD DXF 图形交换 (*.dxf)"),
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
    dlg.selectFile(QStringLiteral("sketch.json"));
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

    // DWG 是专有二进制格式：本项目未链接 ODA 转换库，无法写出原生 DWG。
    // 与其伪造一个 AutoCAD 打不开的二进制头，不如如实说明并提供 DXF 回落。
    if (fmt == cadfmt::CadFormat::Dwg) {
        QMessageBox mb(this);
        mb.setIcon(QMessageBox::Information);
        mb.setWindowTitle(QStringLiteral("DWG 格式说明"));
        mb.setText(QStringLiteral("DWG 为专有二进制格式，本程序未链接 ODA 转换库，无法写出原生二进制 DWG。"));
        mb.setInformativeText(QStringLiteral(
            "推荐保存为 DXF —— AutoCAD 可直接打开，再用「另存为」即可得到原生 DWG。\n"
            "若仍要写 .dwg 文件，将写入 DXF 兼容内容（部分看图软件可识别，AutoCAD 不认该文件）。"));
        QPushButton* bDxf = mb.addButton(QStringLiteral("保存为 DXF（推荐）"), QMessageBox::AcceptRole);
        QPushButton* bRaw = mb.addButton(QStringLiteral("仍写 .dwg（DXF 兼容内容）"), QMessageBox::ActionRole);
        mb.addButton(QMessageBox::Cancel);
        mb.exec();
        if (mb.clickedButton() == bDxf) {
            const QFileInfo fi(fn);
            fn = fi.absoluteDir().filePath(fi.completeBaseName() + QStringLiteral(".dxf"));
            fmt = cadfmt::CadFormat::Dxf;
        } else if (mb.clickedButton() != bRaw) {
            return;                                          // 取消
        }
    }

    writeDrawing(fn, nullptr);
}

// 新建图纸的实作（与对话框解耦，便于离屏测试直接驱动）。
// 契约：比例由调用方给出（对话框默认 1:1）；clear() 会把比例复位为 1:1，
// 所以必须紧随其后 setScaleRatio 才能生效；整张图框只占一次撤销。
void MainWindow::applyNewDrawing(const SheetSpec& spec, double scaleRatio) {
    if (scaleRatio <= 0.0) scaleRatio = 1.0;
    setCurrentSheetFormat(spec.format);                // T6：字高等「随幅面」的量据此派生
    model_->clear();
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

    // 草图 JSON 的完整内容（也是 STEP/DXF 之外的兜底格式）
    QJsonObject root;
    root["app"] = QStringLiteral("SketchCAD");
    root["formatVersion"] = 1;
    root["scaleRatio"] = model_->scaleRatio();         // 比例随图存档，打开时恢复
    QJsonArray arr;
    for (const auto& e2 : model_->elements()) arr.append(elementToJson(e2));
    root["elements"] = arr;
    root["layers"] = model_->layersToJson();   // 图层表随图存档（旧文件无此字段则读回时回退单层"0"）
    root["blocks"] = model_->blocksToJson();   // 块表随图存档（旧文件无此字段则读回时无块定义）

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
    case cadfmt::CadFormat::Dxf:
        out = cadfmt::toDxf(exp, &e, ds);
        break;
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
        out = QJsonDocument(root).toJson(QJsonDocument::Indented);
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
    if (err) *err = e;
    model_->log(QStringLiteral("已保存：%1（%2，%3 字节）%4")
                    .arg(path).arg(cadfmt::formatName(fmt)).arg(out.size()).arg(e));
    return true;
}

// 打开：草图 JSON 完整往返；DXF 读回图元几何（标注/样条已按几何炸开）
void MainWindow::openFile() {
    const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("打开图纸"), QString(),
        QStringLiteral("草图与图形 (*.json *.dxf);;草图文件 (*.json);;AutoCAD DXF (*.dxf);;所有文件 (*)"));
    if (fn.isEmpty()) return;
    readDrawing(fn, nullptr);
}

// 读盘的实作（与文件对话框解耦，便于离屏测试直接驱动）。
bool MainWindow::readDrawing(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取 ") + path;
        model_->log(QStringLiteral("打开失败：") + path, "error");
        return false;
    }
    const QByteArray raw = f.readAll();
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
        if (!cadfmt::fromDxf(raw, imported, &e, &st)) {
            if (err) *err = e;
            model_->log(QStringLiteral("DXF 解析失败：") + e, "error");
            return false;
        }
        model_->clear();
        model_->beginEdit(QStringLiteral("导入 DXF"));
        model_->addElementsQuiet(imported);
        QString note = QStringLiteral("已打开 DXF：%1（导入 %2 个图元；标注/样条已按几何炸开）")
                           .arg(path).arg(imported.size());
        // 圆弧段表示法不同导致的降级必须如实告知，不能静默改形状（宁可少画，绝不画错）
        if (st.arcsDropped > 0) {
            note += QStringLiteral("；其中 %1 段圆弧因表示法差异按直线导入（另有 %2 段已还原）")
                        .arg(st.arcsDropped).arg(st.arcsKept);
        }
        model_->log(note, st.arcsDropped > 0 ? "warn" : "info");
        // 打开后把整幅图适配居中。不能沿用 fitView()（固定 zoom=4 且以**原点**为中心）：
        // 外部 DXF 的图形常画在远离原点处（如 x≈200~240），按原点居中会落在视口外，
        // 表现为"打开了却一片空白"。
        QRectF bb;
        bool haveBox = false;
        for (const auto& el : imported) {
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

    const QJsonDocument d = QJsonDocument::fromJson(raw);
    if (!d.isObject()) {
        if (err) *err = QStringLiteral("不是有效的草图文件");
        model_->log(QStringLiteral("打开失败：不是有效的草图文件"), "error");
        return false;
    }
    model_->clear();
    const QJsonObject root = d.object();
    model_->beginEdit(QStringLiteral("打开草图"));
    std::vector<Element> loaded;
    for (const auto& v : root["elements"].toArray()) loaded.push_back(elementFromJson(v.toObject()));
    model_->addElementsQuiet(loaded);
    // 读回图层表（若有）。必须在 addElementsQuiet 之后、setScaleRatioValue 之前：
    // 图层表不依赖比例；若文件无 layers 字段则回退为单层"0"，不影响旧图兼容。
    model_->setLayersFromJson(root["layers"].toArray());
    // 读回块表（若有）。块引用图元（type=BlockRef）在渲染/命中时按此表展开，
    // 因此必须与图元一并读回，否则引用会退化为"未定义块"而不绘制。旧文件无此字段 → 无块定义。
    model_->setBlocksFromJson(root["blocks"].toArray());
    // 读档的比例只作元数据恢复：存档里的几何已经是按该比例画好的成品，不能再缩放一次
    if (root.contains("scaleRatio"))
        model_->setScaleRatioValue(root["scaleRatio"].toDouble(1.0));
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

void MainWindow::exportDxf() {
    const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("导出 DXF"),
                                                    QStringLiteral("sketch.dxf"),
                                                    QStringLiteral("DXF (*.dxf)"));
    if (fn.isEmpty()) return;
    QString err;
    const QByteArray body = cadfmt::toDxf(model_->elements(), &err, model_->scaleRatio());
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

} // namespace SketchCAD
