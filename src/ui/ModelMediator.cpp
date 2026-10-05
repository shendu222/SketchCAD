#include "ui/ModelMediator.h"
#include "core/SketchModel.h"
#include "core/Vec.h"          // planeName(Plane)
#include "ui/GraphicsView.h"
#include "ui/LogPanel.h"
#include "ui/ElementPanel.h"
#include "ui/BomTree.h"
#include "ui/PropertyPanel.h"
#include "ui/StatusBar.h"
#include "ui/RulerWidget.h"
#include "ui/CommandLine.h"

namespace SketchCAD {

ModelMediator::ModelMediator(QObject* parent, const Components& comp)
    : QObject(parent), c_(comp) {}

void ModelMediator::setHost(const Host& host) { host_ = host; }

void ModelMediator::rewire(const Components& comp) {
    for (const auto& c : conns_) disconnect(c);   // 断开与旧模型/视图的全部连接
    conns_.clear();
    c_ = comp;
    wire();                                       // 按新组件集重新接线（连接清单同步重建）
}

void ModelMediator::wire() {
    // ---- 命令 / 操作结果 -> 日志面板 ----
    conns_ << connect(c_.model, &SketchModel::logAppended, c_.logPanel, &LogPanel::appendLog);

    // ---- 模型变化 -> 状态栏计数 + 各面板刷新（集中订阅：唯一触发时机）----
    // 此前面板在构造函数各自 connect 一次，主窗口又要记着"别重复连"，易失步；
    // 现统一由这里订阅一次，BomTree / PropertyPanel 不再自连接。
    conns_ << connect(c_.model, &SketchModel::modelChanged, c_.bom, &BomTree::onModelChanged);
    conns_ << connect(c_.model, &SketchModel::selectionChanged, c_.bom, &BomTree::onSelectionChanged);
    conns_ << connect(c_.model, &SketchModel::modelChanged, c_.propPanel, &PropertyPanel::onModelChanged);
    conns_ << connect(c_.model, &SketchModel::selectionChanged, c_.propPanel, &PropertyPanel::onSelectionChanged);
    conns_ << connect(c_.model, &SketchModel::defaultStyleChanged,
            c_.propPanel, &PropertyPanel::onDefaultStyleChanged);
    conns_ << connect(c_.model, &SketchModel::defaultLineStyleChanged,
            c_.propPanel, &PropertyPanel::onDefaultLineStyleChanged);

    // 选中数 / 图元总数 -> 状态栏计数
    conns_ << connect(c_.model, &SketchModel::modelChanged, this, [this] {
        if (c_.statusBar) c_.statusBar->setCount(c_.model->elementCount());
    });

    // ---- 当前命令 / 光标坐标 / 平面 / 比例 -> 状态栏 ----
    conns_ << connect(c_.model, &SketchModel::statusCommandChanged, c_.statusBar, &StatusBar::setCommand);
    conns_ << connect(c_.view, &GraphicsView::cursorMoved, c_.statusBar, &StatusBar::setCoord);
    conns_ << connect(c_.model, &SketchModel::planeChanged, this, [this](Plane p) {
        if (c_.statusBar) c_.statusBar->setPlaneInfo(QString::fromLatin1(planeName(p)));
    });
    conns_ << connect(c_.model, &SketchModel::viewChanged, this, [this] {
        if (!c_.statusBar) return;
        c_.statusBar->setDrawScale(c_.model->scaleLabel());
        c_.statusBar->setPlaneInfo(QString::fromLatin1(planeName(c_.model->plane())));
    });
    // ---- UCS / 坐标读数模式 -> 状态栏 UCS 指示器 ----
    // 世界系显示 WCS；命名/临时 UCS 显示「名称 角度°」；读数切到 UCS 时附加提示。
    auto updateUcsIndicator = [this] {
        if (!c_.statusBar) return;
        const Ucs& u = c_.model->currentUcs();
        const QString suffix = c_.model->coordDisplayUcs()
                                   ? QStringLiteral("（读数 UCS）") : QString();
        if (u.isWorld())
            c_.statusBar->setUcsInfo(QStringLiteral("WCS") + suffix);
        else
            c_.statusBar->setUcsInfo(QString("%1 %2°%3")
                                         .arg(u.name)
                                         .arg(math::rad2deg(u.angleRad), 0, 'f', 1)
                                         .arg(suffix));
    };
    conns_ << connect(c_.model, &SketchModel::ucsChanged, this, updateUcsIndicator);
    updateUcsIndicator();   // 启动时回显一次初始状态
    conns_ << connect(c_.model, &SketchModel::scaleChanged, this, [this](double) {
        if (c_.statusBar) c_.statusBar->setDrawScale(c_.model->scaleLabel());
        if (host_.onScaleChanged) host_.onScaleChanged();   // 标题栏比例栏随动（主窗口私有）
    });

    // ---- 标尺联动：视图缩放/平移时刷新 ----
    conns_ << connect(c_.view, &GraphicsView::viewTransformChanged, c_.hRuler, &RulerWidget::sync);
    conns_ << connect(c_.view, &GraphicsView::viewTransformChanged, c_.vRuler, &RulerWidget::sync);

    // ---- 图元面板双击 -> 激活工具；工具切换 -> 工具条高亮/参数条同步 ----
    conns_ << connect(c_.elemPanel, &ElementPanel::toolSelected, c_.model, &SketchModel::setActiveTool);
    if (host_.onActiveToolChanged)
        conns_ << connect(c_.model, &SketchModel::activeToolChanged, this,
                [this](const QString& t) { host_.onActiveToolChanged(t); });

    // ---- 默认样式变化 -> 回显到工具条与菜单（主窗口私有）----
    if (host_.onDefaultStyleChanged)
        conns_ << connect(c_.model, &SketchModel::defaultStyleChanged, this,
                [this](const QColor& c, double w) { host_.onDefaultStyleChanged(c, w); });
    if (host_.onDefaultLineStyleChanged)
        conns_ << connect(c_.model, &SketchModel::defaultLineStyleChanged, this,
                [this](int s) { host_.onDefaultLineStyleChanged(s); });

    // ---- 连续直线模式（命令行 chain on/off 也会走到这里）----
    if (host_.onChainToggled)
        conns_ << connect(c_.model, &SketchModel::chainLineToggled, this,
                [this](bool on) { host_.onChainToggled(on); });

    // ---- 命令行键盘输入坐标 -> 注入为拾取点（分段式命令流程）----
    conns_ << connect(c_.cmdLine, &CommandLine::pointEntered, c_.view, &GraphicsView::injectPoint);
    // ---- 绘制命令期间输入选项关键字（如 C/闭合）-> 应用到当前绘制流程 ----
    conns_ << connect(c_.cmdLine, &CommandLine::optionEntered, c_.view, &GraphicsView::applyCommandOption);
}

} // namespace SketchCAD