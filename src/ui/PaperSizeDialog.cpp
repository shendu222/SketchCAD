#include "PaperSizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>

namespace SketchCAD {

PaperSizeDialog::PaperSizeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("按幅面生成图框与标题栏 — 选择幅面"));
    resize(340, 300);

    auto* root = new QVBoxLayout(this);

    // ---- 幅面 A0~A5 ----
    auto* gbFmt = new QGroupBox(QStringLiteral("图纸幅面 (GB/T 14689 / ISO 128)"), this);
    auto* fmtLay = new QVBoxLayout(gbFmt);
    auto* grpFmt = new QButtonGroup(this);
    static const QPair<SheetFormat, QString> kFmts[] = {
        {SheetFormat::A0, QStringLiteral("A0  (1189 × 841)")},
        {SheetFormat::A1, QStringLiteral("A1  (841 × 594)")},
        {SheetFormat::A2, QStringLiteral("A2  (594 × 420)")},
        {SheetFormat::A3, QStringLiteral("A3  (420 × 297)")},
        {SheetFormat::A4, QStringLiteral("A4  (297 × 210)")},
        {SheetFormat::A5, QStringLiteral("A5  (210 × 148)")},
    };
    for (const auto& f : kFmts) {
        auto* rb = new QRadioButton(f.second, gbFmt);
        grpFmt->addButton(rb, (int)f.first);
        fmtLay->addWidget(rb);
        if (f.first == fmt_) rb->setChecked(true);
    }
    connect(grpFmt, QOverload<int, bool>::of(&QButtonGroup::idToggled),
            this, [this](int id, bool on) { if (on) fmt_ = (SheetFormat)id; });
    root->addWidget(gbFmt);

    // ---- 摆放方向 ----
    auto* gbOri = new QGroupBox(QStringLiteral("摆放方向"), this);
    auto* oriLay = new QHBoxLayout(gbOri);
    auto* grpOri = new QButtonGroup(this);
    auto* rbLand = new QRadioButton(QStringLiteral("横向（长边水平）"), gbOri);
    auto* rbPort = new QRadioButton(QStringLiteral("纵向（长边垂直）"), gbOri);
    grpOri->addButton(rbLand, 1);
    grpOri->addButton(rbPort, 0);
    rbLand->setChecked(true);   // 工程图默认横向
    oriLay->addWidget(rbLand);
    oriLay->addWidget(rbPort);
    connect(rbLand, &QRadioButton::toggled, this, [this](bool on) { if (on) landscape_ = true; });
    connect(rbPort, &QRadioButton::toggled, this, [this](bool on) { if (on) landscape_ = false; });
    root->addWidget(gbOri);

    root->addWidget(new QLabel(QStringLiteral(
        "确认后将以所选幅面为最小尺寸自动套框：图框会包住全部已有图元，"
        "标题栏固定在右下角并自动避让，不覆盖任何现有图元。"), this));

    // ---- 确认/取消 ----
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                    Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

} // namespace SketchCAD
