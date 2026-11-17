#include "core/I18n.h"
#include "HousingViewsDialog.h"
#include "core/SketchModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>

namespace SketchCAD {

HousingViewsDialog::HousingViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(SketchCAD::I18n::t(QString::fromUtf8("绘制箱体视图")));
    resize(380, 620);
    buildUi();
}

void HousingViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        SketchCAD::I18n::t(QString::fromUtf8("箱体视图（主视图全剖 + 俯视图 B + A—A 环形断面 + C 向视图；\n含剖切符号、表面粗糙度与技术要求）。单位：毫米（mm）。\n默认：筒体 Ø82 / Ø64H8，底板 46×70×7，总高 70，凸台 Ø34/Ø26。")), this));

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

    auto* sbOuter   = makeDouble(p_.outerD,    10.0, 2000.0, " mm");
    auto* sbBore    = makeDouble(p_.boreD,      2.0, 2000.0, " mm");
    auto* sbBaseW   = makeDouble(p_.baseW,     10.0, 2000.0, " mm");
    auto* sbBaseL   = makeDouble(p_.baseL,     10.0, 2000.0, " mm");
    auto* sbBaseThk = makeDouble(p_.baseThk,    2.0,  500.0, " mm");
    auto* sbOverall = makeDouble(p_.overallH,  20.0, 2000.0, " mm");
    auto* sbBossD   = makeDouble(p_.bossD,      6.0,  500.0, " mm");
    auto* sbBossBore= makeDouble(p_.bossBoreD,  2.0,  500.0, " mm");
    auto* sbBossH   = makeDouble(p_.bossAxisH, 10.0, 2000.0, " mm");
    auto* sbWallHoleD = makeDouble(p_.wallHoleD, 1.0,  100.0, " mm");
    auto* sbWallHoleN = new QSpinBox(this);
    sbWallHoleN->setRange(0, 36);
    sbWallHoleN->setValue(p_.wallHoleCount);
    auto* edBoreTol = new QLineEdit(p_.boreTol, this);
    edBoreTol->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("空 = 不注公差")));

    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("筒体外径 Ø")),   sbOuter);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("主孔 Ø")),       sbBore);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("主孔公差")),     edBoreTol);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板宽")),       sbBaseW);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板深")),       sbBaseL);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("底板厚")),       sbBaseThk);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("总高")),         sbOverall);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("凸台外径 Ø")),   sbBossD);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("凸台孔 Ø")),     sbBossBore);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("凸台轴距底面")), sbBossH);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("壁孔数量")),     sbWallHoleN);
    form->addRow(SketchCAD::I18n::t(QString::fromUtf8("壁孔径 Ø")),     sbWallHoleD);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(SketchCAD::I18n::t(QString::fromUtf8("确定")));
    box->button(QDialogButtonBox::Cancel)->setText(SketchCAD::I18n::t(QString::fromUtf8("取消")));
    connect(box, &QDialogButtonBox::accepted, this, [this, sbOuter, sbBore, edBoreTol,
            sbBaseW, sbBaseL, sbBaseThk, sbOverall, sbBossD, sbBossBore, sbBossH,
            sbWallHoleN, sbWallHoleD]() {
        HousingParams p;
        p.outerD      = sbOuter->value();
        p.boreD       = sbBore->value();
        p.boreTol     = edBoreTol->text().trimmed();
        p.baseW       = sbBaseW->value();
        p.baseL       = sbBaseL->value();
        p.baseThk     = sbBaseThk->value();
        p.overallH    = sbOverall->value();
        p.bossD       = sbBossD->value();
        p.bossBoreD   = sbBossBore->value();
        p.bossAxisH   = sbBossH->value();
        p.wallHoleCount = sbWallHoleN->value();
        p.wallHoleD   = sbWallHoleD->value();

        const auto r = buildHousingViews(p, model_);
        model_->beginEdit(SketchCAD::I18n::t(QString::fromUtf8("绘制箱体视图")));
        model_->addElementsQuiet(r.elems);
        model_->log(SketchCAD::I18n::t(QString::fromUtf8("已生成箱体视图：%1 个图元，比例 %2（主视图全剖 + A—A 断面）"))
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
