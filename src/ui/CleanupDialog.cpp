#include "CleanupDialog.h"
#include "core/SketchModel.h"
#include "core/Cleanup.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>

namespace SketchCAD {

CleanupDialog::CleanupDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("图形清理 (PURGE / OVERKILL)"));
    setMinimumWidth(520);

    // ---- 清理选项 ----
    auto* optBox = new QGroupBox(QStringLiteral("清理选项"), this);
    auto* form = new QFormLayout(optBox);

    cbGarbage_   = new QCheckBox(QStringLiteral("清除垃圾图元（零长线/零径圆弧/NaN 坐标/空文字/空多边形）"), this);
    cbGarbage_->setChecked(true);
    cbInvisible_ = new QCheckBox(QStringLiteral("同时清除隐藏图元 (visible=false)【可能移除有意隐藏的内容】"), this);
    cbInvisible_->setChecked(false);
    cbBlocks_    = new QCheckBox(QStringLiteral("清除无引用的块定义（冗余数据）"), this);
    cbBlocks_->setChecked(true);
    cbLayers_    = new QCheckBox(QStringLiteral("清除空图层（保留层 \"0\" 与当前层）【谨慎】"), this);
    cbLayers_->setChecked(false);
    cbOverkill_  = new QCheckBox(QStringLiteral("重复图元清理 (OVERKILL)：删除完全重合 / 合并共线重叠线段"), this);
    cbOverkill_->setChecked(true);

    tolSpin_ = new QDoubleSpinBox(this);
    tolSpin_->setDecimals(4);
    tolSpin_->setRange(0.0001, 10.0);
    tolSpin_->setSingleStep(0.001);
    tolSpin_->setValue(0.001);
    tolSpin_->setToolTip(QStringLiteral("几何容差(mm)：两点距离/半径差/角度差小于该值视为重合"));

    form->addRow(cbGarbage_);
    form->addRow(cbInvisible_);
    form->addRow(cbBlocks_);
    form->addRow(cbLayers_);
    form->addRow(cbOverkill_);
    form->addRow(QStringLiteral("几何容差 (mm):"), tolSpin_);

    // ---- 结果区 ----
    resultLabel_ = new QLabel(QStringLiteral("点击「预览统计」查看清理前后对比（不修改图面）。"), this);
    resultLabel_->setWordWrap(true);
    detailList_ = new QListWidget(this);
    detailList_->setMinimumHeight(160);

    btnPreview_ = new QPushButton(QStringLiteral("预览统计"), this);
    btnRun_     = new QPushButton(QStringLiteral("执行清理"), this);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnPreview_);
    btnRow->addWidget(btnRun_);
    btnRow->addStretch(1);
    btnRow->addWidget(closeBtn);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(optBox);
    lay->addWidget(resultLabel_);
    lay->addWidget(detailList_, 1);
    lay->addLayout(btnRow);

    connect(btnPreview_, &QPushButton::clicked, this, &CleanupDialog::onPreview);
    connect(btnRun_,     &QPushButton::clicked, this, &CleanupDialog::onRun);
    connect(closeBtn,    &QPushButton::clicked, this, &QDialog::accept);
}

cleanup::CleanupOptions CleanupDialog::options() const {
    cleanup::CleanupOptions opt;
    opt.purgeGarbage      = cbGarbage_->isChecked();
    opt.purgeInvisible    = cbInvisible_->isChecked();
    opt.purgeUnusedBlocks = cbBlocks_->isChecked();
    opt.purgeUnusedLayers = cbLayers_->isChecked();
    opt.overkill          = cbOverkill_->isChecked();
    opt.tol               = tolSpin_->value();
    return opt;
}

void CleanupDialog::onPreview() {
    const cleanup::CleanupStats st = cleanup::runCleanup(model_, options(), /*dryRun=*/true);
    showResult(st, /*dryRun=*/true);
}

void CleanupDialog::onRun() {
    const cleanup::CleanupStats st = cleanup::runCleanup(model_, options(), /*dryRun=*/false);
    showResult(st, /*dryRun=*/false);
}

void CleanupDialog::showResult(const cleanup::CleanupStats& st, bool dryRun) {
    const QString head = dryRun ? QStringLiteral("【预览】") : QStringLiteral("【完成】");
    resultLabel_->setText(head + st.summary());
    detailList_->clear();
    for (const auto& d : st.detail) detailList_->addItem(d);
    if (st.detail.empty())
        detailList_->addItem(QStringLiteral("（无可清理项）"));
}

} // namespace SketchCAD
