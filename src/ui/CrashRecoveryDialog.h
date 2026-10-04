#pragma once
#include <QDialog>
#include <QVector>
#include "core/AutoSave.h"

class QTableWidget;
class QLabel;
class QPushButton;

namespace SketchCAD {

// 图纸恢复对话框：启动时发现残留 .sv$ 快照 → 列出（时间/原路径/大小），
// 提供「恢复所选 / 丢弃所选 / 全部丢弃」。丢弃走二次确认。
//
// 设计：对话框本身不碰模型，只发信号；真正的读档/删文件由 MainWindow 调
// AutoSave::restoreSnapshot / discardSnapshot 完成（保持 AutoSave 纯函数可无头测试）。
class CrashRecoveryDialog : public QDialog {
    Q_OBJECT
public:
    explicit CrashRecoveryDialog(const QVector<AutoSave::Snapshot>& snaps,
                                 QWidget* parent = nullptr);

    // 选中的快照（若有多选则返回首个）；无选中返回空
    AutoSave::Snapshot selectedSnapshot() const;

signals:
    // 用户点「恢复」：交由 MainWindow 执行读档
    void recoverRequested(const SketchCAD::AutoSave::Snapshot& s);
    // 用户点「丢弃」：交由 MainWindow 执行删除
    void discardRequested(const SketchCAD::AutoSave::Snapshot& s);
    // 用户点「全部丢弃」
    void discardAllRequested();

private slots:
    void onRecover();
    void onDiscard();
    void onDiscardAll();

private:
    AutoSave::Snapshot snapshotAtRow(int row) const;

    QVector<AutoSave::Snapshot> snaps_;
    QTableWidget*  table_ = nullptr;
    QLabel*        lbHint_ = nullptr;
    QPushButton*   btnRecover_ = nullptr;
    QPushButton*   btnDiscard_ = nullptr;
};

} // namespace SketchCAD
