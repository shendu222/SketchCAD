#pragma once
#include <QObject>
#include <functional>

class QWidget;
class QEvent;

namespace SketchCAD {
class SketchModel;

// Del 键统一兜底（应用级事件过滤器）。
//
// 背景：菜单里一旦用 QAction(QKeySequence::Delete, Qt::ApplicationShortcut) 注册
// 删除快捷键，真实键盘事件会在 QWidgetWindow / QShortcutMap 层被直接消费，
// KeyPress 根本到不了焦点控件（GraphicsView / CommandLine 的按键处理全是死代码）。
// 而 QLineEdit 等文本控件又会 accept ShortcutOverride 把菜单快捷键屏蔽，于是
// 「画布能删、属性面板里不能删」这类随焦点漂移的诡异现象就出现了。
//
// 本过滤器装在 qApp 上，在 QShortcutMap 之前统一接管 Key_Delete：
//   · 画布族 / 文本编辑器 / 模态对话框 / 非主窗口 → 放行，保留各自默认语义；
//   · 其它控件 → 删除当前活动文档的选中图元并吞掉事件，避免 shortcut 抢键或重复删除。
// 只处理 KeyPress + Key_Delete，不碰 ShortcutOverride，其它按键与其它快捷键完全不受影响。
class DeleteKeyFilter : public QObject {
public:
    // 多文档下 model_ 是「活动文档」借用指针，切标签会重绑，故用回调动态取。
    using ModelProvider = std::function<SketchModel*()>;

    DeleteKeyFilter(QWidget* host, ModelProvider provider, QObject* parent = nullptr);

    // 主窗口销毁时解除对活动文档的引用，避免 destroyed 之后仍有按键派发时
    // provider_ 回调访问已析构的 MainWindow。
    void detach();

    // 纯策略（公开供无头自测逐控件验证）：给定焦点控件，是否应由本过滤器接管 Delete。
    static bool shouldHandleDelete(QWidget* focus, QWidget* host, bool modalActive);

    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QWidget* host_;
    ModelProvider provider_;
};

} // namespace SketchCAD
