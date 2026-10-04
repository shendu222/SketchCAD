#include "AutoSaveOptionsDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QLabel>

namespace SketchCAD {

AutoSaveOptionsDialog::AutoSaveOptionsDialog(int currentIntervalMinutes, int currentOpThreshold,
                                             int currentLowDiskMb, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("自动保存设置"));

    auto* lay = new QVBoxLayout(this);
    auto* tip = new QLabel(
        QStringLiteral("自动保存会在异常退出（崩溃 / 断电）后于下次启动提示恢复，\n"
                       "不会覆盖你的原图纸文件。"), this);
    tip->setWordWrap(true);
    lay->addWidget(tip);

    auto* form = new QFormLayout;

    // 间隔：关(0) / 5 / 10 / 15 / 30
    cbInterval_ = new QComboBox(this);
    cbInterval_->addItem(QStringLiteral("关闭"), 0);
    cbInterval_->addItem(QStringLiteral("5 分钟"), 5);
    cbInterval_->addItem(QStringLiteral("10 分钟"), 10);
    cbInterval_->addItem(QStringLiteral("15 分钟"), 15);
    cbInterval_->addItem(QStringLiteral("30 分钟"), 30);
    {
        int idx = cbInterval_->findData(currentIntervalMinutes);
        cbInterval_->setCurrentIndex(idx >= 0 ? idx : 2);   // 缺省 10 分钟
    }
    form->addRow(QStringLiteral("保存间隔："), cbInterval_);

    // 操作次数触发：关(0) / 20 / 50 / 100
    cbOpCount_ = new QComboBox(this);
    cbOpCount_->addItem(QStringLiteral("关闭"), 0);
    cbOpCount_->addItem(QStringLiteral("每 20 次操作"), 20);
    cbOpCount_->addItem(QStringLiteral("每 50 次操作"), 50);
    cbOpCount_->addItem(QStringLiteral("每 100 次操作"), 100);
    {
        int idx = cbOpCount_->findData(currentOpThreshold);
        cbOpCount_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    form->addRow(QStringLiteral("按操作次数："), cbOpCount_);

    sbLowDisk_ = new QSpinBox(this);
    sbLowDisk_->setRange(10, 100000);
    sbLowDisk_->setValue(currentLowDiskMb > 0 ? currentLowDiskMb : 100);
    sbLowDisk_->setSuffix(QStringLiteral(" MB"));
    form->addRow(QStringLiteral("磁盘剩余告警："), sbLowDisk_);

    lay->addLayout(form);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

int AutoSaveOptionsDialog::intervalMinutes() const {
    return cbInterval_ ? cbInterval_->currentData().toInt() : 10;
}

int AutoSaveOptionsDialog::opThreshold() const {
    return cbOpCount_ ? cbOpCount_->currentData().toInt() : 0;
}

int AutoSaveOptionsDialog::lowDiskMb() const {
    return sbLowDisk_ ? sbLowDisk_->value() : 100;
}

} // namespace SketchCAD
