#include "MTextDialog.h"
#include "core/Element.h"
#include "core/SketchModel.h"
#include "core/TextStyle.h"
#include <QPlainTextEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

namespace SketchCAD {

MTextDialog::MTextDialog(SketchModel* model, QWidget* parent, const Element* initial)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("多行文字 (MTEXT)"));
    resize(460, 420);

    edText_ = new QPlainTextEdit(this);
    cbStyle_ = new QComboBox(this);
    sbHeight_ = new QDoubleSpinBox(this);
    sbWidthFactor_ = new QDoubleSpinBox(this);
    cbAlign_ = new QComboBox(this);
    sbLineSpacing_ = new QDoubleSpinBox(this);
    sbWrap_ = new QDoubleSpinBox(this);
    cbField_ = new QComboBox(this);

    sbHeight_->setRange(0.1, 1000.0); sbHeight_->setSuffix(QStringLiteral(" mm"));
    sbHeight_->setValue(3.5); sbHeight_->setDecimals(2);
    sbWidthFactor_->setRange(0.1, 10.0); sbWidthFactor_->setSingleStep(0.1);
    sbWidthFactor_->setValue(1.0); sbWidthFactor_->setDecimals(2);
    sbLineSpacing_->setRange(0.5, 5.0); sbLineSpacing_->setSingleStep(0.1);
    sbLineSpacing_->setValue(1.3); sbLineSpacing_->setDecimals(2);
    sbWrap_->setRange(0.0, 10000.0); sbWrap_->setSingleStep(1.0);
    sbWrap_->setValue(0.0); sbWrap_->setDecimals(1);
    sbWrap_->setSpecialValueText(QStringLiteral("不自动换行"));

    // 对齐：9 点（索引 0..8 与 Element::textAlign 对应）
    for (int i = 0; i <= 8; ++i) cbAlign_->addItem(QString::fromUtf8(mtextAlignName(i)));
    cbAlign_->setCurrentIndex(1);

    // 样式下拉：首项「默认（不关联）」→ 空 styleName
    cbStyle_->addItem(QStringLiteral("默认（不关联样式）"), QString());
    if (model_) {
        for (int i = 0; i < model_->textStyleCount(); ++i) {
            const TextStyle& s = model_->textStyleAt(i);
            cbStyle_->addItem(s.name, s.name);
        }
    }

    // 字段占位插入
    cbField_->addItem(QStringLiteral("插入字段…"), QString());
    cbField_->addItem(QStringLiteral("日期 %<DATE>%"), QStringLiteral("%<DATE>%"));
    cbField_->addItem(QStringLiteral("时间 %<TIME>%"), QStringLiteral("%<TIME>%"));
    cbField_->addItem(QStringLiteral("文件名 %<FILENAME>%"), QStringLiteral("%<FILENAME>%"));
    cbField_->addItem(QStringLiteral("文件路径 %<FILEPATH>%"), QStringLiteral("%<FILEPATH>%"));
    cbField_->addItem(QStringLiteral("图元总数 %<COUNT>%"), QStringLiteral("%<COUNT>%"));

    // 预填已有图元
    if (initial) {
        edText_->setPlainText(initial->text);
        sbHeight_->setValue(initial->radius > 0.1 ? initial->radius : 3.5);
        sbWidthFactor_->setValue(initial->widthFactor > 0 ? initial->widthFactor : 1.0);
        cbAlign_->setCurrentIndex(initial->textAlign >= 0 && initial->textAlign <= 8 ? initial->textAlign : 1);
        sbLineSpacing_->setValue(initial->lineSpacing > 0 ? initial->lineSpacing : 1.3);
        sbWrap_->setValue(initial->mtextWidth > 0 ? initial->mtextWidth : 0.0);
        const int si = cbStyle_->findData(initial->styleName);
        if (si >= 0) cbStyle_->setCurrentIndex(si);
    }

    // ---- 布局 ----
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("内容（支持 \\n 换行、%<字段>%）："), (QWidget*)nullptr);
    form->addRow(edText_);
    form->addRow(QStringLiteral("文字样式："), cbStyle_);
    form->addRow(QStringLiteral("字高："), sbHeight_);
    form->addRow(QStringLiteral("宽度因子："), sbWidthFactor_);
    form->addRow(QStringLiteral("对齐方式："), cbAlign_);
    form->addRow(QStringLiteral("行距因子："), sbLineSpacing_);
    form->addRow(QStringLiteral("自动换行宽度："), sbWrap_);

    auto* fieldRow = new QHBoxLayout;
    fieldRow->addWidget(new QLabel(QStringLiteral("字段：")));
    fieldRow->addWidget(cbField_);
    QPushButton* bIns = new QPushButton(QStringLiteral("插入到光标处"));
    fieldRow->addWidget(bIns);
    connect(bIns, &QPushButton::clicked, this, &MTextDialog::insertField);
    connect(cbField_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MTextDialog::onStyleChanged);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    v->addLayout(fieldRow);
    v->addWidget(btns);
}

void MTextDialog::insertField() {
    const QString code = cbField_->currentData().toString();
    if (code.isEmpty()) return;
    edText_->insertPlainText(code);
    cbField_->setCurrentIndex(0);
}

void MTextDialog::onStyleChanged(int idx) {
    (void)idx;
    // 选择字段项时自动插入（下拉选择即插入，免去多一次点击）
    const QString code = cbField_->currentData().toString();
    if (!code.isEmpty()) insertField();
}

Element MTextDialog::buildElement(const Vec3& anchor) const {
    Element e;
    e.type = ElementType::MText;
    e.name = QStringLiteral("多行文字");
    e.id = 0;
    e.pts = {anchor};
    e.text = edText_->toPlainText();
    e.radius = sbHeight_->value();                 // 字高(mm) 复用 radius
    e.color = model_ ? model_->defaultColor() : QColor(26, 29, 40);
    e.lineWidth = 1.0; e.lineWidthAuto = true; e.lineStyle = LineStyle::Solid;
    e.textAlign = cbAlign_->currentIndex();
    e.widthFactor = sbWidthFactor_->value();
    e.styleName = cbStyle_->currentData().toString();
    e.lineSpacing = sbLineSpacing_->value();
    e.mtextWidth = sbWrap_->value();
    return e;
}

} // namespace SketchCAD
