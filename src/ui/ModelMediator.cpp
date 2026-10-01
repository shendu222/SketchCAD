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

void ModelMediator::wire() {
    // ---- 命令 / 操作结果 -> 日志面板 ----
    connect(c_.model, &SketchModel::logAppended, c_.logPanel, &LogPanel::appendLog);

    // ---- 模型变化 -> 状态栏计数 + 各面板刷新（集中订阅：唯一触发时机）----
    // 此前面板在构造函数各自 connect 一次，主窗口又要记着"别重复连"，易失步；
    // 现统一由这里订阅一次，BomTree / PropertyPanel 不再自连接。
    connect(c_.model, &SketchModel::modelChanged, c_.bom, &BomTree::onModelChanged);
    connect(c_.model, &SketchModel::selectionChanged, c_.bom, &BomTree::onSelectionChanged);
    connect(c_.model, &SketchModel::modelChanged, c_.propPanel, &PropertyPanel::onModelChanged);
    connect(c_.model, &SketchModel::selectionChanged, c_.propPanel, &PropertyPanel::onSelectionChanged);
    connect(c_.model, &SketchModel::defaultStyleChanged,
            c_.propPanel, &PropertyPanel::onDefaultStyleChanged);
    connect(c_.model, &SketchModel::defaultLineStyleChanged,
            c_.propPanel, &PropertyPanel::onDefaultLineStyleChanged);

    // 选中数 / 图元总数 -> 状态栏计数
    connect(c_.model, &SketchModel::modelChanged, this, [this] {
        if (c_.statusBar) c_.statusBar->setCount(c_.model->elementCount());
    });

    // ---- 当前命令 / 光标坐标 / 平面 / 比例 -> 状态栏 ----
    connect(c_.model, &SketchModel::statusCommandChanged, c_.statusBar, &StatusBar::setCommand);
    connect(c_.view, &GraphicsView::cursorMoved, c_.statusBar, &StatusBar::setCoord);
    connect(c_.model, &SketchModel::planeChanged, this, [this](Plane p) {
        if (c_.statusBar) c_.statusBar->setPlaneInfo(QString::fromLatin1(planeName(p)));
    });
    connect(c_.model, &SketchModel::viewChanged, this, [this] {
        if (!c_.statusBar) return;
        c_.statusBar->setDrawScale(c_.model->scaleLabel());
        c_.statusBar->setPlaneInfo(QString::fromLatin1(planeName(c_.model->plane())));
    });
    connect(c_.model, &SketchModel::scaleChanged, this, [this](double) {
        if (c_.statusBar) c_.statusBar->setDrawScale(c_.model->scaleLabel());
        if (host_.onScaleChanged) host_.onScaleChanged();   // 标题栏比例栏随动（主窗口私有）
    });

    // ---- 标尺联动：视图缩放/平移时刷新 ----
    connect(c_.view, &GraphicsView::viewTransformChanged, c_.hRuler, &RulerWidget::sync);
    connect(c_.view, &GraphicsView::viewTransformChanged, c_.vRuler, &RulerWidget::sync);

    // ---- 图元面板双击 -> 激活工具；工具切换 -> 工具条高亮/参数条同步 ----
    connect(c_.elemPanel, &ElementPanel::toolSelected, c_.model, &SketchModel::setActiveTool);
    if (host_.onActiveToolChanged)
        connect(c_.model, &SketchModel::activeToolChanged, this,
                [this](const QString& t) { host_.onActiveToolChanged(t); });

    // ---- 默认样式变化 -> 回显到工具条与菜单（主窗口私有）----
    if (host_.onDefaultStyleChanged)
        connect(c_.model, &SketchModel::defaultStyleChanged, this,
                [this](const QColor& c, double w) { host_.onDefaultStyleChanged(c, w); });
    if (host_.onDefaultLineStyleChanged)
        connect(c_.model, &SketchModel::defaultLineStyleChanged, this,
                [this](int s) { host_.onDefaultLineStyleChanged(s); });

    // ---- 连续直线模式（命令行 chain on/off 也会走到这里）----
    if (host_.onChainToggled)
        connect(c_.model, &SketchModel::chainLineToggled, this,
                [this](bool on) { host_.onChainToggled(on); });

    // ---- 命令行键盘输入坐标 -> 注入为拾取点（分段式命令流程）----
    connect(c_.cmdLine, &CommandLine::pointEntered, c_.view, &GraphicsView::injectPoint);
    // ---- 绘制命令期间输入选项关键字（如 C/闭合）-> 应用到当前绘制流程 ----
    connect(c_.cmdLine, &CommandLine::optionEntered, c_.view, &GraphicsView::applyCommandOption);
}

} // namespace SketchCAD