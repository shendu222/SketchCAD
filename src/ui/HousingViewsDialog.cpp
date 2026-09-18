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
    setWindowTitle(QStringLiteral("绘制箱体视图"));
    resize(380, 620);
    buildUi();
}

void HousingViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        QStringLiteral("箱体视图（主视图全剖 + 俯视图 B + A—A 环形断面 + C 向视图；\n"
                       "含剖切符号、表面粗糙度与技术要求）。单位：毫米（mm）。\n"
                       "默认：筒体 Ø82 / Ø64H8，底板 46×70×7，总高 70，凸台 Ø34/Ø26。"), this));

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
    edBoreTol->setPlaceholderText(QStringLiteral("空 = 不注公差"));

    form->addRow(QStringLiteral("筒体外径 Ø"),   sbOuter);
    form->addRow(QStringLiteral("主孔 Ø"),       sbBore);
    form->addRow(QStringLiteral("主孔公差"),     edBoreTol);
    form->addRow(QStringLiteral("底板宽"),       sbBaseW);
    form->addRow(QStringLiteral("底板深"),       sbBaseL);
    form->addRow(QStringLiteral("底板厚"),       sbBaseThk);
    form->addRow(QStringLiteral("总高"),         sbOverall);
    form->addRow(QStringLiteral("凸台外径 Ø"),   sbBossD);
    form->addRow(QStringLiteral("凸台孔 Ø"),     sbBossBore);
    form->addRow(QStringLiteral("凸台轴距底面"), sbBossH);
    form->addRow(QStringLiteral("壁孔数量"),     sbWallHoleN);
    form->addRow(QStringLiteral("壁孔径 Ø"),     sbWallHoleD);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
        model_->beginEdit(QStringLiteral("绘制箱体视图"));
        model_->addElementsQuiet(r.elems);
        model_->log(QStringLiteral("已生成箱体视图：%1 个图元，比例 %2（主视图全剖 + A—A 断面）")
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
