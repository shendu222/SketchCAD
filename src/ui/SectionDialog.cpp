#include "SectionDialog.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPixmap>
#include <QPainter>
#include <QFrame>
#include <algorithm>

namespace SketchCAD {

static const char* materialName(SectionMaterial m) {
    switch (m) {
        case SectionMaterial::General:  return "通用金属（45°细实线）";
        case SectionMaterial::CastIron: return "铸铁（网状剖面线）";
        case SectionMaterial::Steel:    return "钢";
        case SectionMaterial::Aluminum: return "铝（反向 45°）";
        case SectionMaterial::Wood:     return "木材（顺纹水平线）";
        case SectionMaterial::Glass:    return "玻璃（45°长短画线）";
        case SectionMaterial::Liquid:   return "液体（水平短画线）";
        case SectionMaterial::Concrete: return "混凝土（交叉线+点）";
        case SectionMaterial::NonMetal: return "非金属（交叉网格）";
        case SectionMaterial::Brick:    return "砖墙（错缝砌体）";
        case SectionMaterial::Stone:    return "石材墙面（块石）";
        case SectionMaterial::Insulation: return "保温层（点阵）";
        case SectionMaterial::Plywood:  return "胶合板（交叉层）";
        case SectionMaterial::CoilWinding: return "线圈绕组元件（45°线+点）";
        case SectionMaterial::LaminatedSteel: return "叠钢片（薄45°线）";
        case SectionMaterial::Sand:     return "型砂/粉末冶金等（点阵）";
        case SectionMaterial::Soil:     return "基础周围泥土（疏点）";
        case SectionMaterial::ReinforcedConcrete: return "钢筋混凝土（45°线+点）";
        case SectionMaterial::Mesh:     return "筛网/过滤网（网格）";
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
    for (int i = 0; i < kSectionMaterialCount; ++i)
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

    // 实时预览：把当前材料/角度/间距的图案画进缩略图，让用户在确定前就看到效果。
    previewLabel_ = new QLabel(this);
    previewLabel_->setFixedSize(180, 120);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setAlignment(Qt::AlignCenter);

    form->addRow(QStringLiteral("材料类别"), cbMaterial_);
    form->addRow(QStringLiteral("图案预览"), previewLabel_);
    form->addRow(QStringLiteral("豁免类别"), cbExempt_);
    form->addRow(QStringLiteral("剖面线角度"), sbAngle_);
    form->addRow(QStringLiteral("剖面线间距"), sbSpacing_);

    // 任一参数变化 → 重绘预览（所见即所得）
    connect(cbMaterial_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updatePreview(); });
    connect(sbAngle_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { updatePreview(); });
    connect(sbSpacing_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { updatePreview(); });

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

    updatePreview();   // 初始预览：与传入的初始材料/角度/间距一致
}

// 把当前参数对应的剖面图案绘制到缩略图。
// 复用引擎的 hatchPatternFor / generateHatchLines / generateHatchDots，保证"预览 == 实际填充"。
void SectionDialog::updatePreview() {
    if (!previewLabel_) return;

    SectionStyle st;
    st.material  = (SectionMaterial)cbMaterial_->currentIndex();
    st.angleDeg  = sbAngle_->value();
    st.spacingMm = sbSpacing_->value();

    const int Wpx = previewLabel_->width(), Hpx = previewLabel_->height();
    QPixmap pm(Wpx, Hpx);
    pm.fill(Qt::white);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);

    // 预览区域：世界坐标取一个方形，使其约 3 个基准间距见方，保证图案密度适中。
    const double span = std::max(6.0, st.spacingMm * 6.0);
    SectionRegion r;
    r.boundary = { Vec3(0, 0, 0), Vec3(span, 0, 0),
                   Vec3(span, span, 0), Vec3(0, span, 0) };
    r.style = st;

    // 世界→像素：留 6px 边距，Y 翻转（与画布一致，Y 向上）
    const double m = 6.0;
    const double s = (std::min(Wpx, Hpx) - 2 * m) / span;
    auto toPx = [&](const Vec3& v) {
        return QPointF(m + v.x * s, Hpx - (m + v.y * s));
    };

    // 外框（示意剖切边界）
    p.setPen(QPen(QColor(40, 40, 40), 1.2));
    p.drawRect(QRectF(m, m, span * s, span * s));

    // 剖面线（细实线，深色）
    const HatchDraw mode = hatchDrawMode(r);
    if (mode == HatchDraw::SolidFill) {
        p.fillRect(QRectF(m, m, span * s, span * s), QColor(20, 20, 20));
    } else if (mode == HatchDraw::Hatch) {
        p.setPen(QPen(QColor(60, 60, 60), 1.0));
        for (const auto& seg : generateHatchLines(r)) {
            // painter 裁剪：超长线被矩形裁剪（与实际渲染一致）
            p.drawLine(toPx(seg.a), toPx(seg.b));
        }
        if (hatchPatternFor(st).dots) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(60, 60, 60));
            for (const auto& d : generateHatchDots(r))
                p.drawEllipse(toPx(d), 1.1, 1.1);
            p.setBrush(Qt::NoBrush);
        }
    }
    p.end();

    previewLabel_->setPixmap(pm);
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
