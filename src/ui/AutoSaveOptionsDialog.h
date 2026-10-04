#pragma once
#include <QDialog>

class QComboBox;
class QSpinBox;

namespace SketchCAD {

// 自动保存设置对话框（文件 → 自动保存设置...）：
//   - 保存间隔：关 / 5 / 10 / 15 / 30 分钟（默认 10）
//   - 按操作次数触发：关闭 / N 次（可选，默认关）
//   - 磁盘剩余告警阈值（MB，默认 100）
// 确认后经 AppSettings 落地并重建 AutoSave 定时器。
class AutoSaveOptionsDialog : public QDialog {
    Q_OBJECT
public:
    // currentIntervalMinutes：0=关；currentOpThreshold：<=0=关；currentLowDiskMb
    AutoSaveOptionsDialog(int currentIntervalMinutes, int currentOpThreshold,
                          int currentLowDiskMb, QWidget* parent = nullptr);

    int intervalMinutes() const;
    int opThreshold() const;
    int lowDiskMb() const;

private:
    QComboBox* cbInterval_ = nullptr;
    QComboBox* cbOpCount_  = nullptr;
    QSpinBox*  sbLowDisk_  = nullptr;
};

} // namespace SketchCAD
