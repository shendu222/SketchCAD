#include "SectionDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>

namespace SketchCAD {

static const char* materialName(SectionMaterial m) {
    switch (m) {
        case SectionMaterial::General:  return "通用金属（45°细实线）";
        case SectionMaterial::CastIron: return "铸铁（网状剖面线）";
        case SectionMaterial::Steel:    return "钢";
        case SectionMaterial::Wood:     return "木材（年轮）";
        case SectionMaterial::Glass:    return "玻璃";
        case SectionMaterial::Liquid:   return "液体";
        case SectionMaterial::Concrete: return "混凝土";
        case SectionMaterial::NonMetal: return "非金属（交叉网格）";
    }
    return "通用金属";
}

static const char* exemptName(SectionExempt e) {
    switch (e) {
        case SectionExempt::None:                  return "无（正常画剖面线）";
        case SectionExempt::AuxiliaryPart:         return "相邻辅助零件（不画，4.2）";
        case SectionExempt::SolidBarLongitudinal:  return "实心轴/杆/球/键纵向剖切（不画）";
        case SectionExempt::RibSpoke:              return "肋/轮辐/薄壁过对称面（不画）";
        case SectionExempt::StandardPart:          return "标准件（螺栓/螺母/轴承等，不画）";
        case SectionExempt::SmallAreaSolid:        return "狭小区域（涂黑代替）";
    }
    return "无（正常画剖面线）";
}

SectionDialog::SectionDialog(QWidget* parent)
    : SectionDialog(Result(), parent) {}

SectionDialog::SectionDialog(const Result& init, QWidget* parent)
    : QDialog(parent), res_(init) {
    setWindowTitle(QStringLiteral("剖面填充（GB/T 4457.5-2013）"));
    setModal(true);

    auto* form = new QFormLayout();

    cbMaterial_ = new QComboBox(this);
    for (int i = 0; i <= (int)SectionMaterial::NonMetal; ++i)
        cbMaterial_->addItem(QString::fromUtf8(materialName((SectionMaterial)i)));
    cbMaterial_->setCurrentIndex((int)init.style.material);

    cbExempt_ = new QComboBox(this);
    for (int i = 0; i <= (int)SectionExempt::SmallAreaSolid; ++i)
        cbExempt_->addItem(QString::fromUtf8(exemptName((SectionExempt)i)));
    cbExempt_->setCurrentIndex((int)init.style.exempt);

    sbAngle_ = new QDoubleSpinBox(this);
    sbAngle_->setRange(-90.0, 180.0);
    sbAngle_->setDecimals(1);
    sbAngle_->setSuffix(QStringLiteral(" °"));
    sbAngle_->setValue(init.style.angleDeg);

    sbSpacing_ = new QDoubleSpinBox(this);
    sbSpacing_->setRange(0.2, 50.0);
    sbSpacing_->setDecimals(2);
    sbSpacing_->setSingleStep(0.5);
    sbSpacing_->setSuffix(QStringLiteral(" mm"));
    sbSpacing_->setValue(init.style.spacingMm);

    cbAuto_ = new QCheckBox(QStringLiteral("自动区分相邻区域（方向相反 / 间隔不等，4.7）"), this);
    cbAuto_->setChecked(init.autoDistinguish);

    form->addRow(QStringLiteral("材料类别"), cbMaterial_);
    form->addRow(QStringLiteral("豁免类别"), cbExempt_);
    form->addRow(QStringLiteral("剖面线角度"), sbAngle_);
    form->addRow(QStringLiteral("剖面线间距"), sbSpacing_);

    auto* tip = new QLabel(QStringLiteral(
        "提示：GB/T 4457.5 规定同一零件剖面线间隔相等、方向相同，一般与主要轮廓成 45°；\n"
        "装配图中相邻零件剖面线方向应相反或间隔不等。"), this);
    tip->setWordWrap(true);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &SectionDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &SectionDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(cbAuto_);
    root->addWidget(tip);
    root->addWidget(box);
}

void SectionDialog::accept() {
    res_.style.material   = (SectionMaterial)cbMaterial_->currentIndex();
    res_.style.exempt     = (SectionExempt)cbExempt_->currentIndex();
    res_.style.angleDeg   = sbAngle_->value();
    res_.style.spacingMm  = sbSpacing_->value();
    res_.autoDistinguish  = cbAuto_->isChecked();
    QDialog::accept();
}

void SectionDialog::setMaterial(SectionMaterial m)  { cbMaterial_->setCurrentIndex((int)m); }
void SectionDialog::setExempt(SectionExempt e)      { cbExempt_->setCurrentIndex((int)e); }
void SectionDialog::setAngle(double deg)            { sbAngle_->setValue(deg); }
void SectionDialog::setSpacing(double mm)           { sbSpacing_->setValue(mm); }
void SectionDialog::setAutoDistinguish(bool on)     { cbAuto_->setChecked(on); }

SectionMaterial SectionDialog::material() const     { return (SectionMaterial)cbMaterial_->currentIndex(); }
SectionExempt   SectionDialog::exempt() const       { return (SectionExempt)cbExempt_->currentIndex(); }
double          SectionDialog::angle() const        { return sbAngle_->value(); }
double          SectionDialog::spacing() const      { return sbSpacing_->value(); }
bool            SectionDialog::autoDistinguish() const { return cbAuto_->isChecked(); }

} // namespace SketchCAD
