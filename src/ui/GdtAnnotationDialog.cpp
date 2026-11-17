#include "core/I18n.h"
#include "ui/GdtAnnotationDialog.h"
#include "core/Gdt.h"              // 纯逻辑 helper + createFcf + FcfSymbol
#include "core/SurfaceRoughness.h" // buildRoughnessSymbol：粗糙度符号生成
#include "core/ElementFactory.h"   // ElementFactory::createFcf：形位公差框格
#include "core/SketchModel.h"      // 放置落盘（beginEdit / addElement / addElementsQuiet）

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <vector>

namespace SketchCAD {

GdtAnnotationDialog::GdtAnnotationDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(SketchCAD::I18n::t(QString::fromUtf8("GD&T 标注面板 — 粗糙度 / 几何公差")));
    resize(520, 560);

    auto* tabs = new QTabWidget(this);

    // ============================ 粗糙度 ============================
    auto* roughPage = new QWidget(tabs);
    auto* rLay = new QVBoxLayout(roughPage);
    auto* rTop = new QHBoxLayout();
    cbRoughType_ = new QComboBox(roughPage);
    cbRoughType_->addItem(SketchCAD::I18n::t(QString::fromUtf8("Ra（轮廓算术平均偏差）")));
    cbRoughType_->addItem(SketchCAD::I18n::t(QString::fromUtf8("Rz（轮廓最大高度）")));
    leRough_ = new QLineEdit(roughPage);
    leRough_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("输入数值，如 1.6 → 自动匹配标准值")));
    rTop->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("类型：")), roughPage));
    rTop->addWidget(cbRoughType_, 1);
    rTop->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("值：")), roughPage));
    rTop->addWidget(leRough_, 1);
    rLay->addLayout(rTop);

    rLay->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("标准系列（点击即赋值 / 输入即实时高亮最近值）：")), roughPage));
    listRough_ = new QListWidget(roughPage);
    rLay->addWidget(listRough_, 1);

    auto* rBtn = new QHBoxLayout();
    auto* btnRoughPlace = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("放置到图纸")), roughPage);
    btnRoughPlace->setEnabled(model_ != nullptr);
    rBtn->addStretch(1);
    rBtn->addWidget(btnRoughPlace);
    rLay->addLayout(rBtn);

    // ============================ 几何公差 ============================
    auto* geoPage = new QWidget(tabs);
    auto* gLay = new QVBoxLayout(geoPage);
    auto* gForm = new QFormLayout();
    cbGeoFeature_ = new QComboBox(geoPage);
    cbGeoFeature_->addItems(geoTolFeatures());
    leGeoValue_ = new QLineEdit(geoPage);
    leGeoValue_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("公差值，如 0.02")));
    leGeoDatum_ = new QLineEdit(geoPage);
    leGeoDatum_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("基准（可空，多基准空格分隔，如 A B）")));
    gForm->addRow(SketchCAD::I18n::t(QString::fromUtf8("特征：")), cbGeoFeature_);
    gForm->addRow(SketchCAD::I18n::t(QString::fromUtf8("公差值：")), leGeoValue_);
    gForm->addRow(SketchCAD::I18n::t(QString::fromUtf8("基准：")), leGeoDatum_);
    gLay->addLayout(gForm);

    lblGeoSym_ = new QLabel(geoPage);
    gLay->addWidget(lblGeoSym_);

    gLay->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("标准选项（随特征即时刷新）：")), geoPage));
    listGeo_ = new QListWidget(geoPage);
    gLay->addWidget(listGeo_, 1);

    gLay->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("修饰符（勾选即纳入框格）：")), geoPage));
    listMod_ = new QListWidget(geoPage);
    listMod_->setMaximumHeight(110);
    for (const QString& m : geoTolModifiers()) {
        auto* it = new QListWidgetItem(m, listMod_);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
    }
    gLay->addWidget(listMod_);

    auto* gBtn = new QHBoxLayout();
    auto* btnGeoPlace = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("放置到图纸")), geoPage);
    btnGeoPlace->setEnabled(model_ != nullptr);
    gBtn->addStretch(1);
    gBtn->addWidget(btnGeoPlace);
    gLay->addLayout(gBtn);

    tabs->addTab(roughPage, SketchCAD::I18n::t(QString::fromUtf8("粗糙度")));
    tabs->addTab(geoPage, SketchCAD::I18n::t(QString::fromUtf8("几何公差")));

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    box->button(QDialogButtonBox::Close)->setText(SketchCAD::I18n::t(QString::fromUtf8("关闭")));
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(box);

    // ---- 信号 ----
    connect(cbRoughType_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GdtAnnotationDialog::onRoughTypeChanged);
    connect(leRough_, &QLineEdit::textChanged, this, &GdtAnnotationDialog::onRoughTextChanged);
    connect(listRough_, &QListWidget::itemClicked, this, &GdtAnnotationDialog::onRoughListClicked);
    connect(btnRoughPlace, &QPushButton::clicked, this, &GdtAnnotationDialog::onRoughPlace);

    connect(cbGeoFeature_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GdtAnnotationDialog::onGeoFeatureChanged);
    connect(leGeoValue_, &QLineEdit::textChanged, this, &GdtAnnotationDialog::onGeoValueChanged);
    connect(leGeoDatum_, &QLineEdit::textChanged, this, &GdtAnnotationDialog::onGeoDatumChanged);
    connect(listMod_, &QListWidget::itemChanged, this, &GdtAnnotationDialog::onGeoModifierChanged);
    connect(btnGeoPlace, &QPushButton::clicked, this, &GdtAnnotationDialog::onGeoPlace);

    // ---- 初值 ----
    refreshRoughList();
    refreshGeoOptions();
}

// ----------------------------- 粗糙度 -----------------------------
void GdtAnnotationDialog::onRoughTypeChanged(int) { refreshRoughList(); }

void GdtAnnotationDialog::refreshRoughList() {
    const bool rz = (cbRoughType_->currentIndex() == 1);
    const QStringList series = roughnessSeries(rz);
    listRough_->blockSignals(true);
    listRough_->clear();
    for (const QString& v : series) listRough_->addItem(v);
    listRough_->blockSignals(false);
    onRoughTextChanged(leRough_->text());   // 重新高亮最近值
}

void GdtAnnotationDialog::onRoughTextChanged(const QString& text) {
    const bool rz = (cbRoughType_->currentIndex() == 1);
    QString t = text.trimmed();
    // 去除 Ra / Rz 前缀与空白，仅取数值
    if (t.startsWith(QStringLiteral("Ra"), Qt::CaseInsensitive)) t = t.mid(2);
    else if (t.startsWith(QStringLiteral("Rz"), Qt::CaseInsensitive)) t = t.mid(2);
    bool ok = false; const double v = t.trimmed().toDouble(&ok);
    if (!ok) { listRough_->setCurrentRow(-1); return; }
    const QString nearest = nearestRoughnessValue(v, rz);
    if (nearest.isEmpty()) { listRough_->setCurrentRow(-1); return; }
    for (int i = 0; i < listRough_->count(); ++i) {
        if (listRough_->item(i)->text() == nearest) {
            listRough_->setCurrentRow(i);
            listRough_->scrollToItem(listRough_->item(i));
            break;
        }
    }
}

void GdtAnnotationDialog::onRoughListClicked(QListWidgetItem* item) {
    if (item) leRough_->setText(item->text());   // 点选即赋值
}

void GdtAnnotationDialog::onRoughPlace() {
    if (!model_) return;
    const bool rz = (cbRoughType_->currentIndex() == 1);
    QString t = leRough_->text().trimmed();
    if (t.startsWith(QStringLiteral("Ra"), Qt::CaseInsensitive)) t = t.mid(2);
    else if (t.startsWith(QStringLiteral("Rz"), Qt::CaseInsensitive)) t = t.mid(2);
    bool ok = false; const double v = t.trimmed().toDouble(&ok);
    if (!ok) { QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("GD&T 标注")),
                                   SketchCAD::I18n::t(QString::fromUtf8("请输入有效数值（如 1.6）"))); return; }
    const QString nearest = nearestRoughnessValue(v, rz);
    if (nearest.isEmpty()) { QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("GD&T 标注")),
                                                 SketchCAD::I18n::t(QString::fromUtf8("无可用标准值"))); return; }
    RoughnessSpec spec;
    spec.anchor = Vec3(60.0, 60.0, 0.0);
    spec.value = (rz ? QStringLiteral("Rz ") : QStringLiteral("Ra ")) + nearest;
    spec.removeMaterial = true;
    spec.textHeight = 3.5;
    model_->beginEdit(SketchCAD::I18n::t(QString::fromUtf8("粗糙度标注")));
    model_->addElementsQuiet(buildRoughnessSymbol(spec));
    model_->log(SketchCAD::I18n::t(QString::fromUtf8("已放置粗糙度符号 %1")).arg(spec.value), QStringLiteral("info"));
}

// ----------------------------- 几何公差 -----------------------------
void GdtAnnotationDialog::onGeoFeatureChanged(int) { refreshGeoOptions(); }

void GdtAnnotationDialog::refreshGeoOptions() {
    const QString feature = cbGeoFeature_->currentText();
    const QString category = geoTolFeatureCategory(feature);
    const int sym = geoTolSymbolForFeature(feature);
    const QString symName = (sym >= 0 && fcfSymbolValid(sym))
                                ? QString::fromUtf8(fcfSymbolName((FcfSymbol)sym))
                                : QStringLiteral("（当前符号集未收录）");
    lblGeoSym_->setText(SketchCAD::I18n::t(QString::fromUtf8("框格符号：%1")).arg(symName));

    listGeo_->clear();
    listGeo_->addItem(SketchCAD::I18n::t(QString::fromUtf8("类别：")) + (category.isEmpty() ? QStringLiteral("（未知）") : category));
    listGeo_->addItem(SketchCAD::I18n::t(QString::fromUtf8("特征：")) + feature);
    listGeo_->addItem(SketchCAD::I18n::t(QString::fromUtf8("可用修饰符：")) + geoTolModifiers().join(QStringLiteral(" / ")));
    listGeo_->addItem(SketchCAD::I18n::t(QString::fromUtf8("预览：")));
    updateGeoPreview();
}

void GdtAnnotationDialog::updateGeoPreview() {
    if (listGeo_->count() < 4) return;
    const QString feature = cbGeoFeature_->currentText();
    const QString value = leGeoValue_->text().trimmed();
    const QStringList datums = leGeoDatum_->text().split(QRegularExpression(QStringLiteral("[ ,]+")),
                                                         Qt::SkipEmptyParts);
    QString mods;
    for (int i = 0; i < listMod_->count(); ++i)
        if (listMod_->item(i)->checkState() == Qt::Checked)
            mods += (mods.isEmpty() ? QString() : QStringLiteral(" ")) + listMod_->item(i)->text();
    listGeo_->item(3)->setText(SketchCAD::I18n::t(QString::fromUtf8("预览：")) +
                                composeGeoTolText(feature, value, datums, mods));
}

void GdtAnnotationDialog::onGeoValueChanged(const QString&) { updateGeoPreview(); }
void GdtAnnotationDialog::onGeoDatumChanged(const QString&) { updateGeoPreview(); }
void GdtAnnotationDialog::onGeoModifierChanged(QListWidgetItem*) { updateGeoPreview(); }

void GdtAnnotationDialog::onGeoPlace() {
    if (!model_) return;
    const QString feature = cbGeoFeature_->currentText();
    const int sym = geoTolSymbolForFeature(feature);
    if (sym < 0 || !fcfSymbolValid(sym)) {
        QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("GD&T 标注")),
            SketchCAD::I18n::t(QString::fromUtf8("当前符号集未收录「%1」，已禁用放置；可用命令 tol 以文本创建框格。")).arg(feature));
        return;
    }
    const QString value = leGeoValue_->text().trimmed();
    const QStringList datums = leGeoDatum_->text().split(QRegularExpression(QStringLiteral("[ ,]+")),
                                                         Qt::SkipEmptyParts);
    std::vector<QString> datumVec(datums.begin(), datums.end());
    QString mods;
    for (int i = 0; i < listMod_->count(); ++i)
        if (listMod_->item(i)->checkState() == Qt::Checked)
            mods += (mods.isEmpty() ? QString() : QStringLiteral(" ")) + listMod_->item(i)->text();
    const Element e = ElementFactory::createFcf(Vec3(60.0, 60.0, 0.0), sym, value, datumVec, 7.0, mods);
    model_->beginEdit(SketchCAD::I18n::t(QString::fromUtf8("几何公差标注")));
    const int id = model_->addElement(e);
    if (id >= 0)
        model_->log(SketchCAD::I18n::t(QString::fromUtf8("已放置形位公差框格 #%1：%2")).arg(id)
                       .arg(composeGeoTolText(feature, value, datums, mods)), QStringLiteral("info"));
    else
        model_->log(SketchCAD::I18n::t(QString::fromUtf8("几何公差创建失败")), QStringLiteral("error"));
}

} // namespace SketchCAD
