#include "FloorPlanViewsDialog.h"
#include "core/SketchModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>

namespace SketchCAD {

FloorPlanViewsDialog::FloorPlanViewsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("绘制户型平面图"));
    resize(400, 720);
    buildUi();
}

void FloorPlanViewsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        QStringLiteral("参数化户型平面图（建筑装修专业，默认 C户型 101M² 三房两厅两卫）。\n"
                       "墙体（剪力墙涂黑 + 双线内墙）、门（开启弧）、窗、家具图块、\n"
                       "房间标注、四周建筑尺寸链与右侧图签全部可由参数控制。\n"
                       "单位：毫米（mm），默认比例 1:50，A3 横放。"), this));

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto makeDouble = [&](double val, double minV, double maxV, const QString& suffix,
                          double step = 1.0) {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(minV, maxV);
        sb->setDecimals(1);
        sb->setSingleStep(step);
        sb->setSuffix(suffix);
        sb->setValue(val);
        sb->setMinimumWidth(140);
        return sb;
    };

    // ---- 户型总尺寸 ----
    auto* sbOuterW = makeDouble(p_.outerW, 1000.0, 100000.0, " mm", 100.0);
    auto* sbOuterH = makeDouble(p_.outerH, 1000.0, 100000.0, " mm", 100.0);

    // ---- 墙体与门窗 ----
    auto* sbShearT  = makeDouble(p_.shearWallT, 50.0, 600.0, " mm", 10.0);
    auto* sbPartT   = makeDouble(p_.partWallT, 50.0, 400.0, " mm", 10.0);
    auto* sbDoorW   = makeDouble(p_.doorW, 600.0, 2000.0, " mm", 50.0);
    auto* sbBathW   = makeDouble(p_.bathDoorW, 600.0, 2000.0, " mm", 50.0);
    auto* sbSlideW  = makeDouble(p_.slideDoorW, 900.0, 4000.0, " mm", 100.0);

    // ---- 文字 ----
    auto* sbLabelH  = makeDouble(p_.labelTextH, 1.5, 10.0, " mm", 0.5);
    auto* sbDimH    = makeDouble(p_.dimTextH, 1.2, 8.0, " mm", 0.5);
    auto* sbTitleH  = makeDouble(p_.titleTextH, 1.5, 10.0, " mm", 0.5);

    // ---- 图纸 ----
    auto* sbSheetW  = makeDouble(p_.sheetW, 210.0, 1189.0, " mm");
    auto* sbSheetH  = makeDouble(p_.sheetH, 148.0, 841.0, " mm");
    auto* sbMargin  = makeDouble(p_.margin, 5.0, 40.0, " mm");
    auto* sbTbW     = makeDouble(p_.titleBlockW, 60.0, 300.0, " mm");
    auto* sbDimBand = makeDouble(p_.dimBand, 10.0, 60.0, " mm");

    // ---- 比例与开关 ----
    auto* ckAuto    = new QCheckBox(QStringLiteral("自动选取标准比例"), this);
    ckAuto->setChecked(p_.autoScale);
    auto* sbManual  = makeDouble(p_.manualScale, 1.0, 2000.0, "", 10.0);
    auto* ckFurn    = new QCheckBox(QStringLiteral("绘制家具图块"), this);
    ckFurn->setChecked(p_.showFurniture);
    auto* ckDims    = new QCheckBox(QStringLiteral("绘制四周尺寸链"), this);
    ckDims->setChecked(p_.showDims);
    auto* ckLabels  = new QCheckBox(QStringLiteral("绘制房间名标注"), this);
    ckLabels->setChecked(p_.showLabels);

    // ---- 图签文本 ----
    auto* edCompany = new QLineEdit(p_.company, this);
    auto* edClient  = new QLineEdit(p_.client, this);
    auto* edProject = new QLineEdit(p_.projectName, this);
    auto* edDwgName = new QLineEdit(p_.drawingName, this);
    auto* edScale   = new QLineEdit(p_.drawScale, this);

    form->addRow(QStringLiteral("总开间（宽）"), sbOuterW);
    form->addRow(QStringLiteral("总进深（高）"), sbOuterH);
    form->addRow(QStringLiteral("剪力墙厚"),    sbShearT);
    form->addRow(QStringLiteral("内隔墙厚"),    sbPartT);
    form->addRow(QStringLiteral("房门宽"),      sbDoorW);
    form->addRow(QStringLiteral("卫生间门宽"),  sbBathW);
    form->addRow(QStringLiteral("推拉门宽"),    sbSlideW);
    form->addRow(QStringLiteral("房间名字高"),  sbLabelH);
    form->addRow(QStringLiteral("尺寸数字字高"), sbDimH);
    form->addRow(QStringLiteral("图签字高"),    sbTitleH);
    form->addRow(QStringLiteral("图纸宽"),      sbSheetW);
    form->addRow(QStringLiteral("图纸高"),      sbSheetH);
    form->addRow(QStringLiteral("图框边距"),    sbMargin);
    form->addRow(QStringLiteral("图签宽"),      sbTbW);
    form->addRow(QStringLiteral("尺寸链带宽"),  sbDimBand);
    form->addRow(QString(), ckAuto);
    form->addRow(QStringLiteral("手动比例 1:"), sbManual);
    form->addRow(QString(), ckFurn);
    form->addRow(QString(), ckDims);
    form->addRow(QString(), ckLabels);
    form->addRow(QStringLiteral("设计单位"),    edCompany);
    form->addRow(QStringLiteral("建设单位"),    edClient);
    form->addRow(QStringLiteral("工程名称"),    edProject);
    form->addRow(QStringLiteral("图纸名称"),    edDwgName);
    form->addRow(QStringLiteral("比例标注"),    edScale);

    root->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this,
            [this, sbOuterW, sbOuterH, sbShearT, sbPartT, sbDoorW, sbBathW, sbSlideW,
             sbLabelH, sbDimH, sbTitleH, sbSheetW, sbSheetH, sbMargin, sbTbW, sbDimBand,
             ckAuto, sbManual, ckFurn, ckDims, ckLabels,
             edCompany, edClient, edProject, edDwgName, edScale]() {
        FloorPlanParams p;
        p.outerW = sbOuterW->value();  p.outerH = sbOuterH->value();
        p.shearWallT = sbShearT->value(); p.partWallT = sbPartT->value();
        p.doorW = sbDoorW->value();    p.bathDoorW = sbBathW->value();
        p.slideDoorW = sbSlideW->value();
        p.labelTextH = sbLabelH->value(); p.dimTextH = sbDimH->value();
        p.titleTextH = sbTitleH->value();
        p.sheetW = sbSheetW->value();  p.sheetH = sbSheetH->value();
        p.margin = sbMargin->value();  p.titleBlockW = sbTbW->value();
        p.dimBand = sbDimBand->value();
        p.autoScale = ckAuto->isChecked(); p.manualScale = sbManual->value();
        p.showFurniture = ckFurn->isChecked(); p.showDims = ckDims->isChecked();
        p.showLabels = ckLabels->isChecked();
        p.company = edCompany->text().trimmed();
        p.client = edClient->text().trimmed();
        p.projectName = edProject->text().trimmed();
        p.drawingName = edDwgName->text().trimmed();
        p.drawScale = edScale->text().trimmed();

        const auto r = buildFloorPlanViews(p, model_);
        model_->beginEdit(QStringLiteral("绘制户型平面图"));
        model_->addElementsQuiet(r.elems);
        model_->log(QStringLiteral("已生成户型平面图：%1 个图元，比例 %2"
                                   "（墙体/门窗/家具/标注/图签）")
                        .arg(r.elems.size()).arg(r.scaleLabel));
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
