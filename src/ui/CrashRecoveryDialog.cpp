#include "CrashRecoveryDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QDateTime>
#include <QLocale>

namespace SketchCAD {

CrashRecoveryDialog::CrashRecoveryDialog(const QVector<AutoSave::Snapshot>& snaps,
                                         QWidget* parent)
    : QDialog(parent), snaps_(snaps) {
    setWindowTitle(QStringLiteral("图纸恢复"));
    setMinimumSize(640, 380);

    auto* lay = new QVBoxLayout(this);

    lbHint_ = new QLabel(
        QStringLiteral("检测到上次异常退出留下的自动保存快照。\n"
                       "选择一份恢复到其原图纸位置，或丢弃（丢弃不可撤销）。"),
        this);
    lbHint_->setWordWrap(true);
    lay->addWidget(lbHint_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("保存时间"), QStringLiteral("原图纸路径"), QStringLiteral("大小")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setColumnWidth(0, 150);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    for (const auto& s : snaps_) {
        const int r = table_->rowCount();
        table_->insertRow(r);
        table_->setItem(r, 0, new QTableWidgetItem(
            s.time.isValid() ? s.time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—")));
        const QString origin = s.originPath.isEmpty() ? QStringLiteral("(未保存)") : s.originPath;
        table_->setItem(r, 1, new QTableWidgetItem(origin));
        table_->setItem(r, 2, new QTableWidgetItem(
            QLocale().toString(static_cast<double>(s.size) / 1024.0, 'f', 1)
            + QStringLiteral(" KB")));
    }
    if (table_->rowCount() > 0) table_->selectRow(0);
    lay->addWidget(table_, 1);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    btnRecover_ = new QPushButton(QStringLiteral("恢复所选"), this);
    btnDiscard_ = new QPushButton(QStringLiteral("丢弃所选"), this);
    QPushButton* btnAll = new QPushButton(QStringLiteral("全部丢弃"), this);
    btns->addWidget(btnRecover_);
    btns->addWidget(btnDiscard_);
    btns->addWidget(btnAll);
    lay->addLayout(btns);

    connect(btnRecover_, &QPushButton::clicked, this, &CrashRecoveryDialog::onRecover);
    connect(btnDiscard_, &QPushButton::clicked, this, &CrashRecoveryDialog::onDiscard);
    connect(btnAll,     &QPushButton::clicked, this, &CrashRecoveryDialog::onDiscardAll);
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]{
        const bool has = !selectedSnapshot().fullPath.isEmpty();
        btnRecover_->setEnabled(has);
        btnDiscard_->setEnabled(has);
    });
    btnRecover_->setEnabled(!snaps_.isEmpty());
    btnDiscard_->setEnabled(!snaps_.isEmpty());
}

AutoSave::Snapshot CrashRecoveryDialog::snapshotAtRow(int row) const {
    if (row < 0 || row >= snaps_.size()) return {};
    return snaps_.at(row);
}

AutoSave::Snapshot CrashRecoveryDialog::selectedSnapshot() const {
    if (!table_) return {};
    const int r = table_->currentRow();
    return snapshotAtRow(r);
}

void CrashRecoveryDialog::onRecover() {
    const auto s = selectedSnapshot();
    if (s.fullPath.isEmpty()) return;
    emit recoverRequested(s);
    accept();   // 恢复流程交由 MainWindow 继续，此处关闭
}

void CrashRecoveryDialog::onDiscard() {
    const auto s = selectedSnapshot();
    if (s.fullPath.isEmpty()) return;
    const auto a = QMessageBox::question(
        this, QStringLiteral("确认丢弃"),
        QStringLiteral("确定丢弃该自动保存快照？\n此操作不可撤销。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (a != QMessageBox::Yes) return;
    emit discardRequested(s);
}

void CrashRecoveryDialog::onDiscardAll() {
    if (snaps_.isEmpty()) return;
    const auto a = QMessageBox::question(
        this, QStringLiteral("确认全部丢弃"),
        QStringLiteral("确定丢弃全部 %1 份自动保存快照？\n此操作不可撤销。").arg(snaps_.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (a != QMessageBox::Yes) return;
    emit discardAllRequested();
    accept();
}

} // namespace SketchCAD
