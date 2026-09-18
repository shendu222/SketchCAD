#include "ArrayDialog.h"

#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLabel>

namespace SketchCAD {

ArrayDialog::ArrayDialog(Mode initial, const Vec3& defaultCenter, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("阵列复制"));
    res_.center = defaultCenter;

    auto* vbox = new QVBoxLayout(this);
    tabs_ = new QTabWidget(this);

    // ---------- 矩形阵列页 ----------
    auto* rectPage = new QWidget(this);
    auto* rectForm = new QFormLayout(rectPage);
    sbRows_ = new QSpinBox(this);
    sbRows_->setRange(1, 1000); sbRows_->setValue(3);
    sbCols_ = new QSpinBox(this);
    sbCols_->setRange(1, 1000); sbCols_->setValue(3);
    sbRowSp_ = new QDoubleSpinBox(this);
    sbRowSp_->setRange(0.01, 1e6); sbRowSp_->setDecimals(2);
    sbRowSp_->setValue(20.0); sbRowSp_->setSuffix(QStringLiteral(" mm"));
    sbColSp_ = new QDoubleSpinBox(this);
    sbColSp_->setRange(0.01, 1e6); sbColSp_->setDecimals(2);
    sbColSp_->setValue(20.0); sbColSp_->setSuffix(QStringLiteral(" mm"));
    rectForm->addRow(QStringLiteral("行数"), sbRows_);
    rectForm->addRow(QStringLiteral("列数"), sbCols_);
    rectForm->addRow(QStringLiteral("行间距"), sbRowSp_);
    rectForm->addRow(QStringLiteral("列间距"), sbColSp_);
    rectForm->addRow(new QLabel(QStringLiteral("说明：以原图元为 (0,0)，向右/向上阵列；原图元保留。")));
    tabs_->addTab(rectPage, QStringLiteral("矩形阵列"));

    // ---------- 环形阵列页 ----------
    auto* polarPage = new QWidget(this);
    auto* polarForm = new QFormLayout(polarPage);
    sbCount_ = new QSpinBox(this);
    sbCount_->setRange(2, 1000); sbCount_->setValue(8);
    sbAngle_ = new QDoubleSpinBox(this);
    sbAngle_->setRange(0.01, 360.0); sbAngle_->setDecimals(2);
    sbAngle_->setValue(360.0); sbAngle_->setSuffix(QStringLiteral(" °"));
    sbCenterX_ = new QDoubleSpinBox(this);
    sbCenterX_->setRange(-1e6, 1e6); sbCenterX_->setDecimals(2);
    sbCenterX_->setValue(defaultCenter.x); sbCenterX_->setSuffix(QStringLiteral(" mm"));
    sbCenterY_ = new QDoubleSpinBox(this);
    sbCenterY_->setRange(-1e6, 1e6); sbCenterY_->setDecimals(2);
    sbCenterY_->setValue(defaultCenter.y); sbCenterY_->setSuffix(QStringLiteral(" mm"));
    polarForm->addRow(QStringLiteral("阵列个数"), sbCount_);
    polarForm->addRow(QStringLiteral("角度范围"), sbAngle_);

    // ---- 中心点指定方式：手动输入 / 视图拾取（两种模式可自由切换、保持一致）----
    auto* modeBox = new QGroupBox(QStringLiteral("中心点指定方式"));
    auto* modeLayout = new QVBoxLayout(modeBox);
    modeGroup_ = new QButtonGroup(this);
    rbManual_ = new QRadioButton(QStringLiteral("手动输入坐标 (X, Y)"));
    rbPick_   = new QRadioButton(QStringLiteral("在视图中拾取中心点"));
    modeGroup_->addButton(rbManual_);
    modeGroup_->addButton(rbPick_);
    rbManual_->setChecked(true);                 // 默认手动输入
    pickBtn_ = new QPushButton(QStringLiteral("拾取中心点"));
    pickBtn_->setEnabled(false);                 // 默认手动模式：禁用拾取按钮
    modeLayout->addWidget(rbManual_);
    modeLayout->addWidget(rbPick_);
    modeLayout->addWidget(pickBtn_);

    // 模式切换：手动 → 输入框可编辑、拾取按钮禁用；拾取 → 输入框只读、拾取按钮可用。
    // 中心点值始终留存于 X/Y 输入框（单一事实来源），故切换模式不丢值、不串扰。
    auto applyMode = [this]() {
        const bool pick = rbPick_->isChecked();
        sbCenterX_->setEnabled(!pick);
        sbCenterY_->setEnabled(!pick);
        pickBtn_->setEnabled(pick);
    };
    connect(rbManual_, &QRadioButton::toggled, this, applyMode);
    connect(rbPick_,   &QRadioButton::toggled, this, applyMode);
    connect(pickBtn_,   &QPushButton::clicked, this, [this] { emit requestCanvasPick(); });

    polarForm->addRow(modeBox);
    polarForm->addRow(QStringLiteral("中心点 X"), sbCenterX_);
    polarForm->addRow(QStringLiteral("中心点 Y"), sbCenterY_);
    polarForm->addRow(new QLabel(QStringLiteral("说明：绕中心点均匀旋转复制；原图元保留。")));
    tabs_->addTab(polarPage, QStringLiteral("环形阵列"));

    tabs_->setCurrentIndex(initial == Polar ? 1 : 0);
    vbox->addWidget(tabs_);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &ArrayDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox->addWidget(btns);
}

void ArrayDialog::accept() {
    const bool polar = (tabs_->currentIndex() == 1);
    if (polar) {
        const int cnt = sbCount_->value();
        const double ang = sbAngle_->value();
        if (cnt < 2) {
            QMessageBox::warning(this, QStringLiteral("参数错误"),
                                 QStringLiteral("环形阵列个数至少为 2。"));
            return;
        }
        if (ang <= 0.0 || ang > 360.0) {
            QMessageBox::warning(this, QStringLiteral("参数错误"),
                                 QStringLiteral("角度范围需为 0°~360° 之间的正值。"));
            return;
        }
        res_.mode          = Polar;
        res_.count         = cnt;
        res_.angleRangeDeg = ang;
        res_.center        = Vec3(sbCenterX_->value(), sbCenterY_->value(), 0.0);
    } else {
        const int r = sbRows_->value();
        const int c = sbCols_->value();
        if (r < 1 || c < 1) {
            QMessageBox::warning(this, QStringLiteral("参数错误"),
                                 QStringLiteral("行数 / 列数至少为 1。"));
            return;
        }
        res_.mode        = Rectangular;
        res_.rows        = r;
        res_.cols        = c;
        res_.rowSpacing  = sbRowSp_->value();
        res_.colSpacing  = sbColSp_->value();
    }
    QDialog::accept();
}

Vec3 ArrayDialog::effectiveCenter() const {
    // 单一事实来源：两种模式都收敛到 X/Y 输入框，切换模式不丢值。
    return Vec3(sbCenterX_->value(), sbCenterY_->value(), 0.0);
}

void ArrayDialog::setCenter(const Vec3& p) {
    // 视图拾取回填：写入世界坐标，并切到「视图拾取」显示态（输入框只读回显该值）。
    sbCenterX_->setValue(p.x);
    sbCenterY_->setValue(p.y);
    rbPick_->setChecked(true);
}

bool ArrayDialog::isCenterPickMode() const {
    return rbPick_->isChecked();
}

void ArrayDialog::setCenterMode(bool pick) {
    if (pick) rbPick_->setChecked(true);
    else      rbManual_->setChecked(true);
}

void ArrayDialog::setManualCenter(double x, double y) {
    rbManual_->setChecked(true);
    sbCenterX_->setValue(x);
    sbCenterY_->setValue(y);
}

} // namespace SketchCAD
