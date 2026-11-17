#include "core/I18n.h"
#include "BearingViewsDialog.h"
#include "core/SketchModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace SketchCAD {

BearingViewsDialog::BearingViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(SketchCAD::I18n::t(QString::fromUtf8("绘制轴承座三面图")));
    resize(380, 600);
    buildUi();
}

void BearingViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("轴承座三面图（底板 + 支承板 + 圆筒；左视图全剖）。单位：毫米（mm）。\n默认：Ø130 / Ø96 / Ø30，圆筒长 100，底板 240×160×20，总高 180。")), this));

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

    auto* sbBossD   = makeDouble(p_.bossD,        20.0, 5000.0, " mm");
    auto* sbRingD   = makeDouble(p_.ringD,        10.0, 4000.0, " mm");
    auto* sbBoreD   = makeDouble(p_.boreD,         4.0, 2000.0, " mm");
    auto* sbBossLen = makeDouble(p_.bossLen,      10.0, 2000.0, " mm");
    auto* sbBaseL   = makeDouble(p_.baseL,        40.0, 5000.0, " mm");
    auto* sbBaseW   = makeDouble(p_.baseW,        20.0, 2000.0, " mm");
    auto* sbBaseThk = makeDouble(p_.baseThk,       4.0,  500.0, " mm");
    auto* sbOverall = makeDouble(p_.overallH,     20.0, 2000.0, " mm");
    auto* sbWebBot  = makeDouble(p_.webBotW,      16.0, 4000.0, " mm");
    auto* sbSlotWd  = makeDouble(p_.footSlotWd,    4.0,  500.0, " mm");
    auto* sbSlotDp  = makeDouble(p_.footSlotDepth, 6.0,  500.0, " mm");

    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("圆筒外径 Ø")),   sbBossD);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("轴承孔 Ø")),     sbRingD);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("台阶孔 Ø")),     sbBoreD);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("圆筒轴向长")),   sbBossLen);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板长度")),     sbBaseL);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板宽度")),     sbBaseW);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板厚度")),     sbBaseThk);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("总高")),         sbOverall);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("支承板底宽")),   sbWebBot);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("地脚槽宽度")),   sbSlotWd);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("地脚槽深度")),   sbSlotDp);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(SketchCAD::I18n::t(QString::fromUtf8("确定")));
    box->button(QDialogButtonBox::Cancel)->setText(SketchCAD::I18n::t(QString::fromUtf8("取消")));
    connect(box, &QDialogButtonBox::accepted, this, [this, sbBossD, sbRingD, sbBoreD,
            sbBossLen, sbBaseL, sbBaseW, sbBaseThk, sbOverall, sbWebBot, sbSlotWd, sbSlotDp]() {
        BearingParams p;
        p.bossD        = sbBossD->value();
        p.ringD        = sbRingD->value();
        p.boreD        = sbBoreD->value();
        p.bossLen      = sbBossLen->value();
        p.baseL        = sbBaseL->value();
        p.baseW        = sbBaseW->value();
        p.baseThk      = sbBaseThk->value();
        p.overallH     = sbOverall->value();
        p.webBotW      = sbWebBot->value();
        p.footSlotWd    = sbSlotWd->value();
        p.footSlotDepth = sbSlotDp->value();

        const auto r = buildBearingViews(p, model_);
        model_->beginEdit(SketchCAD::I18n::t(QString::fromUtf8("绘制轴承座三面图")));
        model_->addElementsQuiet(r.elems);
        model_->log(SketchCAD::I18n::t(QString::fromUtf8("已生成轴承座三面图：%1 个图元，比例 %2（左视图全剖）"))
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
