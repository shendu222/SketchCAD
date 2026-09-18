#include "FlangeViewsDialog.h"
#include "core/SketchModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace SketchCAD {

FlangeViewsDialog::FlangeViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("绘制法兰盘三面图"));
    resize(360, 520);
    buildUi();
}

void FlangeViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        QStringLiteral("所有尺寸单位：毫米（mm）。参数收敛后生成带颈平焊法兰三面图。"), this));

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto makeDouble = [&](double val, double minV, double maxV, const QString& suffix) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(minV, maxV);
        sb->setDecimals(1);
        sb->setSingleStep(1.0);
        sb->setSuffix(suffix);
        sb->setValue(val);
        sb->setMinimumWidth(140);
        return sb;
    };

    auto* sbOuterD     = makeDouble(p_.outerD,     20.0, 5000.0, " mm");
    auto* sbHubD       = makeDouble(p_.hubD,        5.0, 2000.0, " mm");
    auto* sbThickness  = makeDouble(p_.thickness,  1.0,  500.0, " mm");
    auto* sbBoreD      = makeDouble(p_.boreD,      1.0, 1500.0, " mm");
    auto* sbBoltCircle = makeDouble(p_.boltCircle, 1.0, 3000.0, " mm");
    auto* sbBoltHoleD  = makeDouble(p_.boltHoleD,  1.0,  200.0, " mm");
    auto* sbHubHeight  = makeDouble(p_.hubHeight,  1.0, 1000.0, " mm");

    auto* sbBoltCount = new QSpinBox(this);
    sbBoltCount->setRange(1, 64);
    sbBoltCount->setValue(p_.boltCount);
    sbBoltCount->setMinimumWidth(140);

    auto* sbBoltAngle = makeDouble(p_.boltAngle0Deg, -180.0, 180.0, " °");

    form->addRow(QStringLiteral("法兰外径 D"),      sbOuterD);
    form->addRow(QStringLiteral("颈部外径 N"),      sbHubD);
    form->addRow(QStringLiteral("法兰厚度 C"),      sbThickness);
    form->addRow(QStringLiteral("中心孔径"),        sbBoreD);
    form->addRow(QStringLiteral("螺栓孔分布圆 K"),  sbBoltCircle);
    form->addRow(QStringLiteral("螺栓孔径 L"),      sbBoltHoleD);
    form->addRow(QStringLiteral("螺栓孔数量 n"),    sbBoltCount);
    form->addRow(QStringLiteral("颈部总高"),          sbHubHeight);
    form->addRow(QStringLiteral("第一孔起始角"),    sbBoltAngle);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, [this, sbOuterD, sbHubD,
            sbThickness, sbBoreD, sbBoltCircle, sbBoltHoleD,
            sbBoltCount, sbHubHeight, sbBoltAngle]() {
        FlangeParams p;
        p.outerD       = sbOuterD->value();
        p.hubD         = sbHubD->value();
        p.thickness    = sbThickness->value();
        p.boreD        = sbBoreD->value();
        p.boltCircle   = sbBoltCircle->value();
        p.boltHoleD    = sbBoltHoleD->value();
        p.boltCount    = sbBoltCount->value();
        p.hubHeight    = sbHubHeight->value();
        p.boltAngle0Deg= sbBoltAngle->value();

        const auto r = buildFlangeViews(p, model_);
        model_->beginEdit(QStringLiteral("绘制法兰盘三面图"));
        model_->addElementsQuiet(r.elems);
        model_->log(QStringLiteral("已生成法兰盘三面图：%1 个图元，比例 %2")
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
