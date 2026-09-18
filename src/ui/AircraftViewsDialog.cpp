#include "AircraftViewsDialog.h"
#include "core/SketchModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>

namespace SketchCAD {

AircraftViewsDialog::AircraftViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("绘制飞机三面图"));
    resize(360, 460);
    buildUi();
}

void AircraftViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        QStringLiteral("所有尺寸单位：米（m）。修改后点击“确定”即按新尺寸生成三面图。"), this));

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto makeSpin = [&](double val, double minV, double maxV, const QString& suffix) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(minV, maxV);
        sb->setDecimals(2);
        sb->setSingleStep(0.1);
        sb->setSuffix(suffix);
        sb->setValue(val);
        sb->setMinimumWidth(140);
        return sb;
    };

    auto* sbLength         = makeSpin(p_.length,         1.0, 200.0, " m");
    auto* sbWingspan       = makeSpin(p_.wingspan,       1.0, 200.0, " m");
    auto* sbFuselageHeight = makeSpin(p_.fuselageHeight, 0.5,  50.0, " m");
    auto* sbTailHeight     = makeSpin(p_.tailHeight,     0.5,  50.0, " m");
    auto* sbEngineX        = makeSpin(p_.engineX,        0.0, 150.0, " m");
    auto* sbEngineY        = makeSpin(p_.engineY,        0.0, 100.0, " m");
    auto* sbNoseGearX      = makeSpin(p_.noseGearX,      0.0, 150.0, " m");
    auto* sbMainGearX      = makeSpin(p_.mainGearX,      0.0, 150.0, " m");
    auto* sbMainGearTrack  = makeSpin(p_.mainGearTrack,  0.0,  50.0, " m");
    auto* sbOuterWidth     = makeSpin(p_.outerWidth,     0.0,  50.0, " m");

    form->addRow(QStringLiteral("机身总长"), sbLength);
    form->addRow(QStringLiteral("翼展"),     sbWingspan);
    form->addRow(QStringLiteral("机身高度"), sbFuselageHeight);
    form->addRow(QStringLiteral("垂尾高度"), sbTailHeight);
    form->addRow(QStringLiteral("发动机距机头"), sbEngineX);
    form->addRow(QStringLiteral("发动机半翼展"), sbEngineY);
    form->addRow(QStringLiteral("前起落架距机头"), sbNoseGearX);
    form->addRow(QStringLiteral("主起落架距机头"), sbMainGearX);
    form->addRow(QStringLiteral("主起落架轮距"), sbMainGearTrack);
    form->addRow(QStringLiteral("底部外廓宽度"), sbOuterWidth);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, [this, sbLength, sbWingspan,
            sbFuselageHeight, sbTailHeight, sbEngineX, sbEngineY,
            sbNoseGearX, sbMainGearX, sbMainGearTrack, sbOuterWidth]() {
        aircraft::AircraftViewsParams p;
        p.length         = sbLength->value();
        p.wingspan       = sbWingspan->value();
        p.fuselageHeight = sbFuselageHeight->value();
        p.tailHeight     = sbTailHeight->value();
        p.engineX        = sbEngineX->value();
        p.engineY        = sbEngineY->value();
        p.noseGearX      = sbNoseGearX->value();
        p.mainGearX      = sbMainGearX->value();
        p.mainGearTrack  = sbMainGearTrack->value();
        p.outerWidth     = sbOuterWidth->value();

        const auto elems = aircraft::buildThreeViews(p, model_);
        model_->beginEdit(QStringLiteral("绘制飞机三面图"));
        model_->addElementsQuiet(elems);
        model_->log(QStringLiteral("已生成飞机三面图：%1 个图元")
                        .arg(elems.size()));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
