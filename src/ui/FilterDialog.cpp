#include "FilterDialog.h"
#include "core/SketchModel.h"
#include "core/QSelect.h"
#include "core/Element.h"
#include <QTableWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace SketchCAD {

FilterDialog::FilterDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("过滤器 (FILTER)"));
    resize(680, 480);

    // ---- 已保存过滤器列表 ----
    savedList_ = new QListWidget(this);
    connect(savedList_, &QListWidget::currentRowChanged, this, [this](int) { onSavedSelected(); });

    auto* bDelete = new QPushButton(QStringLiteral("删除"), this);
    connect(bDelete, &QPushButton::clicked, this, &FilterDialog::onDelete);

    auto* savedBox = new QVBoxLayout;
    savedBox->addWidget(new QLabel(QStringLiteral("已保存过滤器："), this));
    savedBox->addWidget(savedList_, 1);
    savedBox->addWidget(bDelete);

    // ---- 条件表：特性 / 运算符 / 值 ----
    condTable_ = new QTableWidget(0, 3, this);
    condTable_->setHorizontalHeaderLabels({ QStringLiteral("特性"), QStringLiteral("运算符"), QStringLiteral("值") });
    condTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    condTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    condTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    condTable_->verticalHeader()->setVisible(false);

    auto* bAdd = new QPushButton(QStringLiteral("添加条件"), this);
    auto* bRemove = new QPushButton(QStringLiteral("删除条件"), this);
    connect(bAdd, &QPushButton::clicked, this, &FilterDialog::addRow);
    connect(bRemove, &QPushButton::clicked, this, &FilterDialog::removeRow);
    auto* condBtns = new QHBoxLayout;
    condBtns->addWidget(bAdd);
    condBtns->addWidget(bRemove);
    condBtns->addStretch(1);

    // 条件行的值列随特性联动：用 cellChanged 不行（值列是可编辑框），改为在
    // addRow 时给每行建立「特性列切换 → 重填本行值列候选」的连接（见 createRow 逻辑内联）。
    cbMatch_ = new QComboBox(this);
    cbMatch_->addItem(QStringLiteral("全部满足（AND）"), true);
    cbMatch_->addItem(QStringLiteral("任一满足（OR）"), false);

    cbAction_ = new QComboBox(this);
    cbAction_->addItem(QStringLiteral("替换当前选择集"), int(QSelectAction::Replace));
    cbAction_->addItem(QStringLiteral("追加到当前选择集"), int(QSelectAction::Append));
    cbAction_->addItem(QStringLiteral("从当前选择集排除"), int(QSelectAction::Exclude));

    edName_ = new QLineEdit(this);
    edName_->setPlaceholderText(QStringLiteral("输入名称后可保存复用"));

    auto* bSave = new QPushButton(QStringLiteral("保存过滤器"), this);
    connect(bSave, &QPushButton::clicked, this, &FilterDialog::onSave);

    auto* bApply = new QPushButton(QStringLiteral("应用"), this);
    auto* bArm   = new QPushButton(QStringLiteral("设为预选器（下一次拾取生效）"), this);
    auto* bClose = new QPushButton(QStringLiteral("关闭"), this);
    connect(bApply, &QPushButton::clicked, this, &FilterDialog::apply);
    connect(bArm,   &QPushButton::clicked, this, &FilterDialog::armFilter);
    connect(bClose, &QPushButton::clicked, this, &QDialog::reject);

    // ---- 布局 ----
    auto* left = new QVBoxLayout;
    left->addLayout(savedBox);

    auto* right = new QVBoxLayout;
    auto* matchForm = new QFormLayout;
    matchForm->addRow(QStringLiteral("组合方式："), cbMatch_);
    right->addLayout(matchForm);
    right->addWidget(condTable_, 1);
    right->addLayout(condBtns);
    auto* saveRow = new QHBoxLayout;
    saveRow->addWidget(edName_, 1);
    saveRow->addWidget(bSave);
    right->addLayout(saveRow);
    auto* actRow = new QFormLayout;
    actRow->addRow(QStringLiteral("应用方式："), cbAction_);
    right->addLayout(actRow);
    auto* applyRow = new QHBoxLayout;
    applyRow->addStretch(1);
    applyRow->addWidget(bApply);
    applyRow->addWidget(bArm);
    applyRow->addWidget(bClose);
    right->addLayout(applyRow);

    auto* h = new QHBoxLayout;
    h->addLayout(left, 1);
    h->addLayout(right, 3);

    auto* v = new QVBoxLayout(this);
    v->addLayout(h, 1);

    addRow();   // 默认给一行空条件
    refreshSaved();
}

// ---- 条件行构造：特性下拉 + 运算符下拉 + 值可编辑下拉（候选随特性联动）----
namespace {
QComboBox* makePropCombo() {
    auto* cb = new QComboBox;
    for (QSelectProp p : { QSelectProp::None, QSelectProp::Type, QSelectProp::Layer,
                           QSelectProp::Color, QSelectProp::LineStyle, QSelectProp::LineWidth,
                           QSelectProp::Text, QSelectProp::Name, QSelectProp::Group,
                           QSelectProp::Style, QSelectProp::Visible })
        cb->addItem(qselectPropName(p), int(p));
    return cb;
}
QComboBox* makeOpCombo() {
    auto* cb = new QComboBox;
    for (QSelectOp op : { QSelectOp::Eq, QSelectOp::Ne, QSelectOp::Contains, QSelectOp::StartsWith })
        cb->addItem(qselectOpName(op), int(op));
    return cb;
}
QComboBox* makeValueCombo(SketchModel* model, QSelectProp p) {
    auto* cb = new QComboBox;
    cb->setEditable(true);
    if (p != QSelectProp::None)
        cb->addItems(uniquePropValues(model, p));
    return cb;
}
} // namespace

void FilterDialog::addRow() {
    const int r = condTable_->rowCount();
    condTable_->insertRow(r);
    auto* cbProp = makePropCombo();
    auto* cbOp = makeOpCombo();
    auto* cbVal = makeValueCombo(model_, QSelectProp::None);
    connect(cbProp, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, cbProp, cbVal](int) {
                // 特性切换 → 重填本行值列候选
                const QSelectProp p = QSelectProp(cbProp->currentData().toInt());
                const QString cur = cbVal->currentText();
                cbVal->clear();
                if (p != QSelectProp::None)
                    cbVal->addItems(uniquePropValues(model_, p));
                cbVal->setEditable(true);
                if (!cur.isEmpty()) cbVal->setCurrentText(cur);
            });
    condTable_->setCellWidget(r, 0, cbProp);
    condTable_->setCellWidget(r, 1, cbOp);
    condTable_->setCellWidget(r, 2, cbVal);
}

void FilterDialog::removeRow() {
    const int r = condTable_->currentRow();
    if (r >= 0 && condTable_->rowCount() > 0) condTable_->removeRow(r);
}

FilterDef FilterDialog::currentFilter() const {
    FilterDef f;
    f.name     = edName_->text().trimmed();
    f.matchAll = cbMatch_->currentData().toBool();
    for (int r = 0; r < condTable_->rowCount(); ++r) {
        auto* cbProp = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 0));
        auto* cbOp   = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 1));
        auto* cbVal  = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 2));
        if (!cbProp || !cbOp || !cbVal) continue;
        QSelectCriteria c;
        c.prop  = QSelectProp(cbProp->currentData().toInt());
        c.op    = QSelectOp(cbOp->currentData().toInt());
        c.value = cbVal->currentText().trimmed();
        if (c.prop != QSelectProp::None)          // "不限"行视为占位行，忽略
            f.conds.push_back(c);
    }
    return f;
}

void FilterDialog::loadFilter(const FilterDef& f) {
    // 清空条件表重建
    condTable_->setRowCount(0);
    for (const auto& c : f.conds) {
        addRow();
        const int r = condTable_->rowCount() - 1;
        auto* cbProp = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 0));
        auto* cbOp   = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 1));
        auto* cbVal  = qobject_cast<QComboBox*>(condTable_->cellWidget(r, 2));
        if (!cbProp || !cbOp || !cbVal) continue;
        const int pi = cbProp->findData(int(c.prop));
        if (pi >= 0) cbProp->setCurrentIndex(pi);
        const int oi = cbOp->findData(int(c.op));
        if (oi >= 0) cbOp->setCurrentIndex(oi);
        cbVal->setCurrentText(c.value);
    }
    const int mi = cbMatch_->findData(f.matchAll);
    if (mi >= 0) cbMatch_->setCurrentIndex(mi);
    edName_->setText(f.name);
}

void FilterDialog::refreshSaved() {
    savedList_->clear();
    for (const auto& f : model_->savedFilters())
        savedList_->addItem(QStringLiteral("%1（%2 条件）").arg(f.name).arg(f.conds.size()));
}

void FilterDialog::onSavedSelected() {
    QListWidgetItem* it = savedList_->currentItem();
    if (!it) return;
    const QString nm = it->text().section(QStringLiteral("（"), 0, 0);
    for (const auto& f : model_->savedFilters())
        if (f.name == nm) { loadFilter(f); break; }
}

void FilterDialog::onSave() {
    FilterDef f = currentFilter();
    if (f.conds.empty()) {
        QMessageBox::information(this, QStringLiteral("保存过滤器"), QStringLiteral("请先添加至少一条有效条件（特性不为「不限」）。"));
        return;
    }
    if (f.name.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("保存过滤器"), QStringLiteral("请输入过滤器名称。"));
        return;
    }
    model_->saveFilter(f);
    refreshSaved();
    // 保存后选中刚保存的项
    for (int i = 0; i < savedList_->count(); ++i)
        if (savedList_->item(i)->text().startsWith(f.name + QStringLiteral("（")))
            savedList_->setCurrentRow(i);
}

void FilterDialog::onDelete() {
    QListWidgetItem* it = savedList_->currentItem();
    if (!it) return;
    const QString nm = it->text().section(QStringLiteral("（"), 0, 0);
    model_->deleteFilter(nm);
    refreshSaved();
}

void FilterDialog::apply() {
    FilterDef f = currentFilter();
    if (f.conds.empty()) {
        QMessageBox::information(this, QStringLiteral("应用过滤器"), QStringLiteral("请先添加至少一条有效条件（特性不为「不限」）。"));
        return;
    }
    const std::vector<int> hits = qselect(model_, f, nullptr);   // 过滤器作用于整个图形
    const QSelectAction act = QSelectAction(cbAction_->currentData().toInt());
    model_->applySelectionDelta(hits, act);
    const QString actName = (act == QSelectAction::Replace) ? QStringLiteral("替换")
                          : (act == QSelectAction::Append)  ? QStringLiteral("追加")
                                                            : QStringLiteral("排除");
    model_->log(QStringLiteral("过滤器「%1」：命中 %2 个对象（%3 选择集）")
                    .arg(f.name.isEmpty() ? QStringLiteral("(未命名)") : f.name)
                    .arg(hits.size()).arg(actName));
}

void FilterDialog::armFilter() {
    FilterDef f = currentFilter();
    if (f.conds.empty()) {
        QMessageBox::information(this, QStringLiteral("设为预选器"), QStringLiteral("请先添加至少一条有效条件（特性不为「不限」）。"));
        return;
    }
    model_->setActivePickFilter(f);   // 瞬态预选器：下一次拾取/框选/栏选生效后自动清除
    accept();
}

} // namespace SketchCAD
