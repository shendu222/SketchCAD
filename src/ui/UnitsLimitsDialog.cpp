#include "UnitsLimitsDialog.h"
#include "core/SketchModel.h"
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

namespace SketchCAD {

UnitsLimitsDialog::UnitsLimitsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("单位与图纸界限 (UNITS / LIMITS)"));
    const UnitsMeta  u = model ? model->units()  : UnitsMeta();
    const LimitsMeta l = model ? model->limits() : LimitsMeta();

    // ---- 单位区 ----
    cbLength_ = new QComboBox(this);
    for (int i = 0; i <= int(LengthUnit::Foot); ++i)
        cbLength_->addItem(lengthUnitName(LengthUnit(i)), i);
    cbLength_->setCurrentIndex(int(u.length));

    cbAngle_ = new QComboBox(this);
    for (int i = 0; i <= int(AngleUnit::Grad); ++i)
        cbAngle_->addItem(angleUnitName(AngleUnit(i)), i);
    cbAngle_->setCurrentIndex(int(u.angle));

    sbLengthPrec_ = new QSpinBox(this);
    sbLengthPrec_->setRange(0, 6); sbLengthPrec_->setValue(u.lengthPrec);
    sbLengthPrec_->setSuffix(QStringLiteral(" 位"));

    sbAnglePrec_ = new QSpinBox(this);
    sbAnglePrec_->setRange(0, 6); sbAnglePrec_->setValue(u.anglePrec);
    sbAnglePrec_->setSuffix(QStringLiteral(" 位"));

    auto* unitForm = new QFormLayout;
    unitForm->addRow(QStringLiteral("长度单位（显示/输入）："), cbLength_);
    unitForm->addRow(QStringLiteral("角度单位（显示/输入）："), cbAngle_);
    unitForm->addRow(QStringLiteral("长度精度："), sbLengthPrec_);
    unitForm->addRow(QStringLiteral("角度精度："), sbAnglePrec_);

    auto* unitBox = new QGroupBox(QStringLiteral("单位"), this);
    unitBox->setLayout(unitForm);

    // ---- 界限区（坐标恒为模型 mm，与单位设置无关）----
    ckCheckOn_ = new QCheckBox(QStringLiteral("开启越界检查（新增图元出界时告警，不拦截）"), this);
    ckCheckOn_->setChecked(l.checkOn);

    auto mk = [this](double v) {
        QDoubleSpinBox* sb = new QDoubleSpinBox(this);
        sb->setRange(-1.0e6, 1.0e6); sb->setDecimals(2);
        sb->setSuffix(QStringLiteral(" mm")); sb->setValue(v);
        return sb;
    };
    sbX1_ = mk(l.x1); sbY1_ = mk(l.y1); sbX2_ = mk(l.x2); sbY2_ = mk(l.y2);

    auto* limForm = new QFormLayout;
    limForm->addRow(QStringLiteral("左下 X："), sbX1_);
    limForm->addRow(QStringLiteral("左下 Y："), sbY1_);
    limForm->addRow(QStringLiteral("右上 X："), sbX2_);
    limForm->addRow(QStringLiteral("右上 Y："), sbY2_);
    limForm->addRow(QString(), ckCheckOn_);

    auto* limBox = new QGroupBox(QStringLiteral("图纸界限（LIMITS，模型 mm）"), this);
    limBox->setLayout(limForm);

    // ---- 预览（示例：长度 1234.5mm / 角度 45° 按当前口径的显示效果）----
    lbPreview_ = new QLabel(this);
    lbPreview_->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &UnitsLimitsDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addWidget(unitBox);
    main->addWidget(limBox);
    main->addWidget(lbPreview_);
    main->addWidget(btns);

    const auto dirty = QOverload<int>::of(&QComboBox::currentIndexChanged);
    connect(cbLength_,      dirty, this, &UnitsLimitsDialog::updatePreview);
    connect(cbAngle_,       dirty, this, &UnitsLimitsDialog::updatePreview);
    connect(sbLengthPrec_,  QOverload<int>::of(&QSpinBox::valueChanged), this, &UnitsLimitsDialog::updatePreview);
    connect(sbAnglePrec_,   QOverload<int>::of(&QSpinBox::valueChanged), this, &UnitsLimitsDialog::updatePreview);
    updatePreview();
}

UnitsMeta UnitsLimitsDialog::collectUnits() const {
    UnitsMeta u;
    u.length     = LengthUnit(cbLength_->currentData().toInt());
    u.angle      = AngleUnit(cbAngle_->currentData().toInt());
    u.lengthPrec = sbLengthPrec_->value();
    u.anglePrec  = sbAnglePrec_->value();
    return u;
}

LimitsMeta UnitsLimitsDialog::collectLimits() const {
    LimitsMeta l;
    l.checkOn = ckCheckOn_->isChecked();
    l.x1 = sbX1_->value(); l.y1 = sbY1_->value();
    l.x2 = sbX2_->value(); l.y2 = sbY2_->value();
    return l;
}

void UnitsLimitsDialog::updatePreview() {
    const UnitsMeta u = collectUnits();
    lbPreview_->setText(QStringLiteral("预览：长度 %1 ／ 角度 %2")
                        .arg(formatLengthWithUnit(1234.5, u),
                             formatAngleDeg(45.0, u)));
}

void UnitsLimitsDialog::accept() {
    if (model_) {
        model_->setUnits(collectUnits());
        model_->setLimits(collectLimits());   // 非法范围内部回落默认 A3
    }
    QDialog::accept();
}

} // namespace SketchCAD
