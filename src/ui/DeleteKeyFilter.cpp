#include "ui/DeleteKeyFilter.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

#include "core/SketchModel.h"

namespace SketchCAD {
namespace {

// 文本编辑控件：Del 必须保留 QLineEdit/QTextEdit 的字符删除默认行为，
// 不能在用户输入命令、改参数、写多行文字时把整张图的选中图元删掉。
bool isTextEditor(QObject* o) {
    return qobject_cast<QLineEdit*>(o) || qobject_cast<QTextEdit*>(o) ||
           qobject_cast<QPlainTextEdit*>(o) || qobject_cast<QAbstractSpinBox*>(o) ||
           qobject_cast<QComboBox*>(o);
}

// 画布族（QGraphicsView 本体或其 viewport 等后代）：它们已有
// GraphicsView::handleDeleteKey() 且带 idleForDelete() 保护，交给自身处理，
// 否则绘制/框选/拾取过程中按 Del 也会被这里无条件误删。
bool isCanvasBranch(QWidget* w) {
    for (QObject* o = w; o; o = o->parent())
        if (qobject_cast<QGraphicsView*>(o)) return true;
    return false;
}

} // namespace

DeleteKeyFilter::DeleteKeyFilter(QWidget* host, ModelProvider provider, QObject* parent)
    : QObject(parent), host_(host), provider_(std::move(provider)) {}

void DeleteKeyFilter::detach() { provider_ = nullptr; }

bool DeleteKeyFilter::shouldHandleDelete(QWidget* focus, QWidget* host, bool modalActive) {
    if (!focus || modalActive) return false;                       // 模态对话框内不删画布图元
    if (host && focus != host && !host->isAncestorOf(focus)) return false; // 只管本主窗口（含浮动 dock）
    if (isTextEditor(focus)) return false;
    if (isCanvasBranch(focus)) return false;
    return true;
}

bool DeleteKeyFilter::eventFilter(QObject*, QEvent* ev) {
    if (!ev || ev->type() != QEvent::KeyPress) return false;
    auto* ke = static_cast<QKeyEvent*>(ev);
    if (ke->key() != Qt::Key_Delete) return false;

    // 真实平台派发时本过滤器首先看到的是 QWidgetWindow 的 KeyPress，
    // 不能用 obj 判断焦点；必须用 QApplication::focusWidget()。
    QWidget* focus = QApplication::focusWidget();
    if (!shouldHandleDelete(focus, host_, QApplication::activeModalWidget() != nullptr))
        return false;

    SketchModel* model = provider_ ? provider_() : nullptr;
    if (!model) return false;
    model->deleteSelected();
    ke->accept();
    // 吞掉 KeyPress：QShortcutMap 没机会再抢键，菜单 action 也不会二次删除。
    return true;
}

} // namespace SketchCAD
