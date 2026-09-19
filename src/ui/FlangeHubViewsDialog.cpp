#include "FlangeHubViewsDialog.h"
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

FlangeHubViewsDialog::FlangeHubViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("绘制法兰盘轮毂视图"));
    resize(380, 680);
    buildUi();
}

void FlangeHubViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        QStringLiteral("阶梯回转体（法兰盘 + 轮毂 + 中心孔）参数化视图。\n"
                       "主视图全剖 A—A，左视图为圆形投影。单位：毫米（mm）。\n"
                       "默认：法兰 Ø80 / 轮毂 Ø60 / 沉孔 Ø41 / 通孔 Ø33 / 总长 136，\n"
                       "3-Ø11.5 均布 + 2-Ø10 定位孔。"), this));

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
    auto makeInt = [&](int val, int minV, int maxV) {
        auto* sb = new QSpinBox(this);
        sb->setRange(minV, maxV);
        sb->setValue(val);
        sb->setMinimumWidth(140);
        return sb;
    };

    // ---- 径向尺寸 ----
    auto* sbFlangeD = makeDouble(p_.flangeD, 20.0, 2000.0, " mm");
    auto* sbHubD    = makeDouble(p_.hubD,    5.0, 2000.0, " mm");
    auto* sbCboreD  = makeDouble(p_.cboreD,  2.0, 1000.0, " mm");
    auto* sbBoreD   = makeDouble(p_.boreD,   1.0, 1000.0, " mm");

    // ---- 轴向尺寸 ----
    auto* sbCboreDepth = makeDouble(p_.cboreDepth, 1.0, 500.0, " mm");
    auto* sbFlangeT    = makeDouble(p_.flangeT,   1.0, 500.0, " mm");
    auto* sbTotalL     = makeDouble(p_.totalL,   10.0, 3000.0, " mm");

    // ---- 过渡特征 ----
    auto* sbChamfer = makeDouble(p_.chamfer, 0.0, 50.0, " mm");
    auto* sbFilletR = makeDouble(p_.filletR, 0.0, 50.0, " mm");

    // ---- 螺栓孔（剖切面穿过其中一个）----
    auto* sbBoltCount = makeInt(p_.boltCount, 0, 36);
    auto* sbBoltD     = makeDouble(p_.boltD,    1.0, 200.0, " mm");
    auto* sbBoltBCD   = makeDouble(p_.boltBCD,  1.0, 2000.0, " mm");
    auto* sbBoltAng   = makeDouble(p_.boltAngle0Deg, -180.0, 180.0, " °");

    // ---- 定位孔（仅左视图表达）----
    auto* sbLocCount = makeInt(p_.locCount, 0, 36);
    auto* sbLocD     = makeDouble(p_.locD,    1.0, 200.0, " mm");
    auto* sbLocBCD   = makeDouble(p_.locBCD,  1.0, 2000.0, " mm");
    auto* sbLocAng   = makeDouble(p_.locAngle0Deg, -180.0, 180.0, " °");

    // ---- 标注 ----
    auto* edRoughBore = new QLineEdit(p_.roughBore, this);
    edRoughBore->setPlaceholderText(QStringLiteral("空 = 不标"));
    auto* edRoughFace = new QLineEdit(p_.roughFace, this);
    edRoughFace->setPlaceholderText(QStringLiteral("空 = 不标"));
    auto* edBoreTol   = new QLineEdit(p_.boreTol, this);
    edBoreTol->setPlaceholderText(QStringLiteral("空 = 不注公差"));

    form->addRow(QStringLiteral("法兰外径 Ø"),    sbFlangeD);
    form->addRow(QStringLiteral("轮毂外径 Ø"),    sbHubD);
    form->addRow(QStringLiteral("左端沉孔径 Ø"),  sbCboreD);
    form->addRow(QStringLiteral("中心通孔 Ø"),    sbBoreD);
    form->addRow(QStringLiteral("沉孔深"),        sbCboreDepth);
    form->addRow(QStringLiteral("法兰厚度"),      sbFlangeT);
    form->addRow(QStringLiteral("零件总长"),      sbTotalL);
    form->addRow(QStringLiteral("轮毂左端倒角 C"), sbChamfer);
    form->addRow(QStringLiteral("根部圆角 R"),    sbFilletR);
    form->addRow(QStringLiteral("螺栓孔数量"),    sbBoltCount);
    form->addRow(QStringLiteral("螺栓孔径 Ø"),    sbBoltD);
    form->addRow(QStringLiteral("螺栓分布圆 Ø"),  sbBoltBCD);
    form->addRow(QStringLiteral("首孔起始角"),    sbBoltAng);
    form->addRow(QStringLiteral("定位孔数量"),    sbLocCount);
    form->addRow(QStringLiteral("定位孔径 Ø"),    sbLocD);
    form->addRow(QStringLiteral("定位分布圆 Ø"),  sbLocBCD);
    form->addRow(QStringLiteral("定位首孔角"),    sbLocAng);
    form->addRow(QStringLiteral("中心孔面粗糙度"), edRoughBore);
    form->addRow(QStringLiteral("法兰外圆粗糙度"), edRoughFace);
    form->addRow(QStringLiteral("中心孔公差"),    edBoreTol);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this,
            [this, sbFlangeD, sbHubD, sbCboreD, sbBoreD, sbCboreDepth, sbFlangeT,
             sbTotalL, sbChamfer, sbFilletR, sbBoltCount, sbBoltD, sbBoltBCD, sbBoltAng,
             sbLocCount, sbLocD, sbLocBCD, sbLocAng, edRoughBore, edRoughFace,
             edBoreTol]() {
        FlangeHubParams p;
        p.flangeD      = sbFlangeD->value();
        p.hubD         = sbHubD->value();
        p.cboreD       = sbCboreD->value();
        p.boreD        = sbBoreD->value();
        p.cboreDepth   = sbCboreDepth->value();
        p.flangeT      = sbFlangeT->value();
        p.totalL       = sbTotalL->value();
        p.chamfer      = sbChamfer->value();
        p.filletR      = sbFilletR->value();
        p.boltCount    = sbBoltCount->value();
        p.boltD        = sbBoltD->value();
        p.boltBCD      = sbBoltBCD->value();
        p.boltAngle0Deg= sbBoltAng->value();
        p.locCount     = sbLocCount->value();
        p.locD         = sbLocD->value();
        p.locBCD       = sbLocBCD->value();
        p.locAngle0Deg = sbLocAng->value();
        p.roughBore    = edRoughBore->text().trimmed();
        p.roughFace    = edRoughFace->text().trimmed();
        p.boreTol      = edBoreTol->text().trimmed();

        const auto r = buildFlangeHubViews(p, model_);
        model_->beginEdit(QStringLiteral("绘制法兰盘轮毂视图"));
        model_->addElementsQuiet(r.elems);
        model_->log(QStringLiteral("已生成法兰盘轮毂视图：%1 个图元，比例 %2（主视图全剖 A—A + 左视图）")
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
