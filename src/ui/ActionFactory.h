#pragma once

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QToolBar>
#include <utility>

namespace SketchCAD {
namespace ui {

// 动作外观与快捷键的统一描述。链式调用属于轻量 Builder：
// 只组装配置，不持有业务状态，避免为同一类 QAction 写多份样板代码。
class ActionSpec {
public:
    ActionSpec& withIcon(const QIcon& icon) { icon_ = icon; return *this; }
    ActionSpec& withShortcut(const QKeySequence& key) { shortcut_ = key; return *this; }
    ActionSpec& withToolTip(const QString& tip) { toolTip_ = tip; return *this; }
    ActionSpec& withStatusTip(const QString& tip) { statusTip_ = tip; return *this; }
    ActionSpec& withCheckable(bool checkable) { checkable_ = checkable; return *this; }

    QIcon icon() const { return icon_; }
    QKeySequence shortcut() const { return shortcut_; }
    QString toolTip() const { return toolTip_; }
    QString statusTip() const { return statusTip_.isEmpty() ? toolTip_ : statusTip_; }
    bool checkable() const { return checkable_; }

private:
    QIcon icon_;
    QKeySequence shortcut_;
    QString toolTip_;
    QString statusTip_;
    bool checkable_ = false;
};

// 把统一配置应用到已创建的 QAction。保留 QAction* 返回/外部创建能力，
// 使工具条中需要 QActionGroup、对象名、bucket 登记的动作也能复用同一套配置逻辑。
inline QAction* configureAction(QAction* action, QObject* owner, const ActionSpec& spec) {
    if (!action) return nullptr;
    if (!spec.icon().isNull()) action->setIcon(spec.icon());
    if (!spec.shortcut().isEmpty()) action->setShortcut(spec.shortcut());
    action->setShortcutContext(Qt::ApplicationShortcut);
    action->setCheckable(spec.checkable());
    if (!spec.toolTip().isEmpty()) action->setToolTip(spec.toolTip());
    if (!spec.statusTip().isEmpty()) action->setStatusTip(spec.statusTip());
    Q_UNUSED(owner);
    return action;
}

// 菜单动作工厂：创建 + 配置 + 连接一次完成。
// Handler 支持 []{} 与 [](bool){} 两种 QAction::triggered 槽签名。
template <class Handler>
QAction* addMenuAction(QMenu* menu, QObject* owner, const QString& text,
                       Handler&& handler, const ActionSpec& spec = {}) {
    if (!menu) return nullptr;
    QAction* action = menu->addAction(text);
    configureAction(action, owner, spec);
    QObject::connect(action, &QAction::triggered, owner, std::forward<Handler>(handler));
    return action;
}

// 工具条动作工厂：与菜单工厂共享 ActionSpec，保持图标、提示和快捷键口径一致。
template <class Handler>
QAction* addToolBarAction(QToolBar* bar, QObject* owner, const QIcon& icon,
                          const QString& text, Handler&& handler,
                          const ActionSpec& spec = {}) {
    if (!bar) return nullptr;
    QAction* action = new QAction(icon, text, owner);
    ActionSpec effective = spec;
    if (effective.icon().isNull() && !icon.isNull()) effective.withIcon(icon);
    configureAction(action, owner, effective);
    bar->addAction(action);
    QObject::connect(action, &QAction::triggered, owner, std::forward<Handler>(handler));
    return action;
}

} // namespace ui
} // namespace SketchCAD
