#include "StyleManagerDialog.h"
#include "core/SketchModel.h"
#include "core/TextStyle.h"
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>

namespace SketchCAD {

StyleManagerDialog::StyleManagerDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("文字样式管理器 (STYLE)"));
    resize(640, 420);

    list_ = new QListWidget(this);
    edName_ = new QLineEdit(this);
    cbFont_ = new QComboBox(this);
    sbHeight_ = new QDoubleSpinBox(this);
    sbWidth_ = new QDoubleSpinBox(this);
    sbOblique_ = new QDoubleSpinBox(this);
    ckBold_ = new QCheckBox(QStringLiteral("粗体"));
    ckItalic_ = new QCheckBox(QStringLiteral("斜体"));

    // 系统字体族
    cbFont_->addItem(QStringLiteral("（默认/系统）"), QString());
    const QStringList fam = QFontDatabase::families();
    for (const QString& f : fam) cbFont_->addItem(f, f);

    sbHeight_->setRange(0.0, 1000.0); sbHeight_->setSuffix(QStringLiteral(" mm"));
    sbHeight_->setValue(0.0); sbHeight_->setDecimals(2); sbHeight_->setSpecialValueText(QStringLiteral("用图元字高"));
    sbWidth_->setRange(0.1, 10.0); sbWidth_->setSingleStep(0.1); sbWidth_->setValue(1.0); sbWidth_->setDecimals(2);
    sbOblique_->setRange(-85.0, 85.0); sbOblique_->setSuffix(QStringLiteral("°"));
    sbOblique_->setValue(0.0); sbOblique_->setDecimals(1);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("样式名："), edName_);
    form->addRow(QStringLiteral("字体："), cbFont_);
    form->addRow(QStringLiteral("字高："), sbHeight_);
    form->addRow(QStringLiteral("宽度因子："), sbWidth_);
    form->addRow(QStringLiteral("倾斜角："), sbOblique_);
    form->addRow(QStringLiteral("字形："), [this]() -> QWidget* {
        auto* w = new QWidget(this);
        auto* h = new QHBoxLayout(w); h->setContentsMargins(0,0,0,0);
        h->addWidget(ckBold_); h->addWidget(ckItalic_);
        return w;
    }());

    auto* bNew = new QPushButton(QStringLiteral("新建"));
    auto* bDel = new QPushButton(QStringLiteral("删除"));
    auto* bSave = new QPushButton(QStringLiteral("保存修改"));
    auto* bApplySel = new QPushButton(QStringLiteral("应用到选中文字"));
    auto* bApplyAll = new QPushButton(QStringLiteral("应用到全部文字"));
    connect(bNew, &QPushButton::clicked, this, &StyleManagerDialog::onNew);
    connect(bDel, &QPushButton::clicked, this, &StyleManagerDialog::onDelete);
    connect(bSave, &QPushButton::clicked, this, &StyleManagerDialog::onSave);
    connect(bApplySel, &QPushButton::clicked, this, &StyleManagerDialog::onApplySelected);
    connect(bApplyAll, &QPushButton::clicked, this, &StyleManagerDialog::onApplyAll);

    auto* left = new QVBoxLayout;
    left->addWidget(list_);
    auto* lbtns = new QHBoxLayout; lbtns->addWidget(bNew); lbtns->addWidget(bDel);
    left->addLayout(lbtns);

    auto* right = new QVBoxLayout;
    right->addLayout(form);
    auto* rbtns = new QHBoxLayout;
    rbtns->addWidget(bSave); rbtns->addWidget(bApplySel); rbtns->addWidget(bApplyAll);
    right->addLayout(rbtns);
    right->addStretch(1);

    auto* h = new QHBoxLayout;
    h->addLayout(left, 1); h->addLayout(right, 2);

    auto* v = new QVBoxLayout(this);
    v->addLayout(h);
    v->addWidget(new QLabel(QStringLiteral("提示：修改字体/字高/宽度因子后，所有引用该样式的文字会同步更新。")));

    connect(list_, &QListWidget::currentItemChanged, this, [this]() { onSelectionChanged(); });
    refreshList();
}

void StyleManagerDialog::refreshList() {
    list_->clear();
    for (int i = 0; i < model_->textStyleCount(); ++i)
        list_->addItem(model_->textStyleAt(i).name);
    if (list_->count() > 0) { list_->setCurrentRow(0); loadCurrent(); }
}

void StyleManagerDialog::onSelectionChanged() { loadCurrent(); }

void StyleManagerDialog::loadCurrent() {
    const QListWidgetItem* it = list_->currentItem();
    if (!it) { curName_.clear(); return; }
    const QString nm = it->text();
    const TextStyle* s = model_->findTextStyle(nm);
    if (!s) return;
    curName_ = nm;
    edName_->setText(s->name);
    const int fi = cbFont_->findData(s->fontFamily);
    cbFont_->setCurrentIndex(fi >= 0 ? fi : 0);
    sbHeight_->setValue(s->textHeight);
    sbWidth_->setValue(s->widthFactor);
    sbOblique_->setValue(s->obliquingAngle);
    ckBold_->setChecked(s->bold);
    ckItalic_->setChecked(s->italic);
}

TextStyle StyleManagerDialog::currentFromForm() const {
    TextStyle s;
    s.name = edName_->text().trimmed();
    s.fontFamily = cbFont_->currentData().toString();
    s.textHeight = sbHeight_->value();
    s.widthFactor = sbWidth_->value();
    s.obliquingAngle = sbOblique_->value();
    s.bold = ckBold_->isChecked();
    s.italic = ckItalic_->isChecked();
    return s;
}

void StyleManagerDialog::onNew() {
    bool ok = false;
    const QString nm = QInputDialog::getText(this, QStringLiteral("新建文字样式"),
                                            QStringLiteral("样式名："), QLineEdit::Normal,
                                            QStringLiteral("样式") + QString::number(model_->textStyleCount() + 1), &ok);
    if (!ok || nm.trimmed().isEmpty()) return;
    TextStyle s = currentFromForm();
    s.name = nm.trimmed();
    if (!model_->addTextStyle(s)) {
        QMessageBox::warning(this, QStringLiteral("新建失败"), QStringLiteral("样式名为空或已存在。"));
        return;
    }
    refreshList();
    // 选中新建项
    const QList<QListWidgetItem*> hits = list_->findItems(nm.trimmed(), Qt::MatchExactly);
    if (!hits.isEmpty()) list_->setCurrentRow(list_->row(hits.first()));
}

void StyleManagerDialog::onDelete() {
    if (curName_.isEmpty()) return;
    if (model_->textStyleCount() <= 1) {
        QMessageBox::warning(this, QStringLiteral("无法删除"), QStringLiteral("至少需保留一个文字样式。"));
        return;
    }
    model_->removeTextStyle(curName_);
    curName_.clear();
    refreshList();
}

void StyleManagerDialog::onSave() {
    TextStyle s = currentFromForm();
    if (s.name.isEmpty()) { QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("样式名不能为空。")); return; }
    // 改名：先 rename 再 update（update 按 name 查找，改名后需两步）
    if (!curName_.isEmpty() && curName_ != s.name) {
        if (!model_->renameTextStyle(curName_, s.name)) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("样式名与现有样式冲突。"));
            return;
        }
    }
    if (!model_->updateTextStyle(s)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("样式不存在。"));
        return;
    }
    curName_ = s.name;
    refreshList();
}

void StyleManagerDialog::onApplySelected() {
    if (curName_.isEmpty()) return;
    const std::vector<int> ids = model_->selection();
    const int n = model_->applyStyleToElements(curName_, ids);
    QMessageBox::information(this, QStringLiteral("应用结果"),
        ids.empty() ? QStringLiteral("当前没有选中的文字对象，请先在绘图区选中文字。")
                    : QStringLiteral("已将样式「%1」应用到 %2 个选中文字对象。").arg(curName_).arg(n));
}

void StyleManagerDialog::onApplyAll() {
    if (curName_.isEmpty()) return;
    std::vector<int> ids;
    for (const auto& e : model_->elements())
        if (e.type == ElementType::Text || e.type == ElementType::MText) ids.push_back(e.id);
    const int n = model_->applyStyleToElements(curName_, ids);
    QMessageBox::information(this, QStringLiteral("应用结果"),
        QStringLiteral("已将样式「%1」应用到全部 %2 个文字对象。").arg(curName_).arg(n));
}

} // namespace SketchCAD
