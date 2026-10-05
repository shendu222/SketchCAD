#pragma once
#include <QDialog>
#include <QVector>
#include "core/CommandCatalog.h"

class QLineEdit;
class QListWidget;

namespace SketchCAD {

class AppSettings;         // 须在命名空间内前向声明（否则签名退化为全局 ::AppSettings）
class CommandInterpreter;

// ============================================================
// CommandPaletteDialog —— 命令面板（Ctrl+Shift+P）
// ============================================================
// 薄 UI 壳：过滤/排序/最近使用等纯逻辑全部在 core/CommandCatalog.{h,cpp}
// 的 CommandPaletteModel（无头可测）；本对话框只负责装配与交互。
//   - 输入即过滤：中文名 / 英文命令名 / 别名 三通道模糊匹配；
//   - 每条显示：标题（中文名 + 命令名）、分类、简介、快捷键、别名；
//   - Enter 执行（经 CommandInterpreter，与命令行同一 handler），Esc 关闭；
//   - ↑↓ 在候选间移动，Tab 亦可；
//   - 最近使用（≤10 条）经 AppSettings 持久化（commandLine/recentPalette）。
// ============================================================
class CommandPaletteDialog : public QDialog {
    Q_OBJECT
public:
    // interp：命令执行入口（与命令行共用）；settings：最近使用持久化（可空=不持久化）。
    CommandPaletteDialog(CommandInterpreter* interp, AppSettings* settings,
                         QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    /// 装配面板条目：CommandCatalog 元数据 + commandNames() 回落条目 + 别名入口。
    static QVector<PaletteEntry> buildEntries();
    void rebuildList();          // 按当前输入刷新候选
    void executeCurrent();       // 执行选中条目
    void loadRecent();           // 读持久化最近使用
    void saveRecent();           // 写持久化最近使用

    CommandInterpreter* interp_ = nullptr;
    AppSettings* settings_ = nullptr;
    CommandPaletteModel model_;
    QLineEdit* edit_ = nullptr;
    QListWidget* list_ = nullptr;
    QVector<PaletteEntry> shown_;   // 当前展示的候选（与 list_ 行一一对应）
};

} // namespace SketchCAD
