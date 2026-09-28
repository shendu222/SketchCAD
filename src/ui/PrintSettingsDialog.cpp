#include "PrintSettingsDialog.h"
#include "PrintPreviewDialog.h"
#include "core/PlotStyle.h"
#include "core/Layout.h"
#include <QComboBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPageSize>

namespace SketchCAD {

// 常见图纸尺寸（id 顺序与枚举一致，便于 combo 索引 ↔ QPageSize::PageSizeId）
static const QPageSize::PageSizeId kPaperIds[] = {
    QPageSize::A4, QPageSize::A3, QPageSize::A2, QPageSize::A1, QPageSize::A0,
    QPageSize::B5, QPageSize::B4, QPageSize::Letter, QPageSize::Legal
};
static const char* kPaperNames[] = {
    "A4 (210×297mm)", "A3 (297×420mm)", "A2 (420×594mm)",
    "A1 (594×841mm)", "A0 (841×1189mm)",
    "B5 (176×250mm)", "B4 (250×353mm)",
    "Letter (8.5×11in)", "Legal (8.5×14in)"
};

PrintSettingsDialog::PrintSettingsDialog(SketchModel* model, const PrintSettings& initial,
                                         QWidget* parent)
    : QDialog(parent), model_(model), s_(initial) {
    setWindowTitle(QStringLiteral("打印设置"));
    resize(720, 640);

    auto* root = new QVBoxLayout(this);

    // ---------------- 页面设置 ----------------
    auto* pageBox = new QGroupBox(QStringLiteral("页面设置"));
    auto* pg = new QGridLayout(pageBox);
    paperCombo_ = new QComboBox();
    for (const char* n : kPaperNames) paperCombo_->addItem(QString::fromUtf8(n));
    refreshPaperSizeCombo();
    pg->addWidget(new QLabel(QStringLiteral("纸张尺寸")), 0, 0);
    pg->addWidget(paperCombo_, 0, 1);

    portraitBtn_  = new QRadioButton(QStringLiteral("纵向"));
    landscapeBtn_ = new QRadioButton(QStringLiteral("横向"));
    auto* orientRow = new QHBoxLayout();
    orientRow->addWidget(portraitBtn_); orientRow->addWidget(landscapeBtn_);
    pg->addWidget(new QLabel(QStringLiteral("方向")), 1, 0);
    pg->addLayout(orientRow, 1, 1);

    marginL_ = new QDoubleSpinBox(); marginL_->setRange(0, 200); marginL_->setSuffix(" mm");
    marginT_ = new QDoubleSpinBox(); marginT_->setRange(0, 200); marginT_->setSuffix(" mm");
    marginR_ = new QDoubleSpinBox(); marginR_->setRange(0, 200); marginR_->setSuffix(" mm");
    marginB_ = new QDoubleSpinBox(); marginB_->setRange(0, 200); marginB_->setSuffix(" mm");
    pg->addWidget(new QLabel(QStringLiteral("页边距 左")), 2, 0); pg->addWidget(marginL_, 2, 1);
    pg->addWidget(new QLabel(QStringLiteral("上")), 2, 2); pg->addWidget(marginT_, 2, 3);
    pg->addWidget(new QLabel(QStringLiteral("右")), 3, 0); pg->addWidget(marginR_, 3, 1);
    pg->addWidget(new QLabel(QStringLiteral("下")), 3, 2); pg->addWidget(marginB_, 3, 3);

    scaleSpin_ = new QDoubleSpinBox(); scaleSpin_->setRange(0.01, 1000); scaleSpin_->setDecimals(3);
    scaleSpin_->setValue(1.0); scaleSpin_->setSuffix(" : 1");
    fitBox_ = new QCheckBox(QStringLiteral("自适应铺满可打印区(优先)"));
    pg->addWidget(new QLabel(QStringLiteral("打印比例")), 4, 0); pg->addWidget(scaleSpin_, 4, 1);
    pg->addWidget(fitBox_, 4, 2, 1, 2);

    centerHBox_ = new QCheckBox(QStringLiteral("水平居中"));
    centerVBox_ = new QCheckBox(QStringLiteral("垂直居中"));
    borderBox_  = new QCheckBox(QStringLiteral("绘制可打印区边框"));
    auto* cRow = new QHBoxLayout();
    cRow->addWidget(centerHBox_); cRow->addWidget(centerVBox_); cRow->addWidget(borderBox_);
    pg->addLayout(cRow, 5, 0, 1, 4);

    // 打印线宽比例：修复「打印线过粗失真」。默认 0.5（细一档），1.0 = 按图纸 mm 原值。
    lineWidthScale_ = new QDoubleSpinBox();
    lineWidthScale_->setRange(0.1, 2.0);
    lineWidthScale_->setDecimals(2);
    lineWidthScale_->setValue(0.5);
    lineWidthScale_->setSingleStep(0.05);
    pg->addWidget(new QLabel(QStringLiteral("打印线宽比例")), 6, 0);
    pg->addWidget(lineWidthScale_, 6, 1);
    pg->addWidget(new QLabel(QStringLiteral("(0.5=细一档, 1.0=原值, 越小越细)")), 6, 2, 1, 2);

    root->addWidget(pageBox);
    connect(paperCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintSettingsDialog::onPaperChanged);

    // ---------------- 打印样式 CTB ----------------
    auto* styleBox = new QGroupBox(QStringLiteral("打印样式 (简化 CTB)"));
    auto* sl = new QGridLayout(styleBox);
    styleTargetType_ = new QComboBox();
    styleTargetType_->addItem(QStringLiteral("按图层"));
    styleTargetType_->addItem(QStringLiteral("按颜色"));
    sl->addWidget(new QLabel(QStringLiteral("匹配维度")), 0, 0);
    sl->addWidget(styleTargetType_, 0, 1);
    styleTargetName_ = new QLineEdit();
    styleTargetName_->setPlaceholderText(QStringLiteral("图层名(如 0)")); // 按颜色时显示颜色值
    sl->addWidget(new QLabel(QStringLiteral("目标")), 1, 0);
    sl->addWidget(styleTargetName_, 1, 1);
    styleColorPick_ = new QPushButton(QStringLiteral("选择颜色"));
    sl->addWidget(styleColorPick_, 1, 2);
    styleOverrideColor_ = new QCheckBox(QStringLiteral("覆盖颜色"));
    styleOverrideWidth_ = new QCheckBox(QStringLiteral("覆盖线宽"));
    styleWidth_ = new QDoubleSpinBox(); styleWidth_->setRange(0.05, 10); styleWidth_->setDecimals(2);
    styleWidth_->setSuffix(" mm");
    sl->addWidget(styleOverrideColor_, 2, 0);
    sl->addWidget(styleOverrideWidth_, 2, 1);
    sl->addWidget(styleWidth_, 2, 2);
    styleAddBtn_ = new QPushButton(QStringLiteral("添加/更新"));
    styleDelBtn_ = new QPushButton(QStringLiteral("删除"));
    sl->addWidget(styleAddBtn_, 3, 1);
    sl->addWidget(styleDelBtn_, 3, 2);
    styleList_ = new QListWidget();
    sl->addWidget(styleList_, 4, 0, 1, 3);

    auto pickColor = [this]() {
        QColor c = QColorDialog::getColor(Qt::black, this, QStringLiteral("选择输出颜色"));
        if (c.isValid()) {
            styleColorPick_->setProperty("chosen", c);
            styleColorPick_->setText(c.name());
            styleColorPick_->setStyleSheet(
                QStringLiteral("background:%1;color:%2").arg(c.name()).arg(c.lightness() > 128 ? "black" : "white"));
        }
    };
    connect(styleColorPick_, &QPushButton::clicked, this, pickColor);
    connect(styleAddBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onAddStyle);
    connect(styleDelBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onDeleteStyle);
    connect(styleList_, &QListWidget::currentRowChanged, this, [this](int){ onStyleSelected(); });
    connect(styleTargetType_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int t) {
        styleTargetName_->setEnabled(t == 0);
        if (t == 1) {
            QColor c = QColorDialog::getColor(Qt::black, this, QStringLiteral("选择要匹配的图元颜色"));
            if (c.isValid()) {
                styleTargetName_->setText(c.name(QColor::HexArgb));
                styleTargetName_->setEnabled(false);
            }
        }
    });

    root->addWidget(styleBox);

    // ---------------- 视口 / 布局 ----------------
    auto* vpBox = new QGroupBox(QStringLiteral("视口 / 布局 (MVP)"));
    auto* vl = new QVBoxLayout(vpBox);
    vl->addWidget(new QLabel(QStringLiteral("空列表 = 模型空间打印（整体图形直接出图，无需切换布局）。"
                                           "下列每个视口将其区域映射到整块可打印区。")));
    vpList_ = new QListWidget();
    vl->addWidget(vpList_);
    vpAddBtn_ = new QPushButton(QStringLiteral("添加：整体图形范围"));
    vpEditBtn_ = new QPushButton(QStringLiteral("编辑比例..."));
    vpDelBtn_ = new QPushButton(QStringLiteral("删除"));
    auto* vpRow = new QHBoxLayout();
    vpRow->addWidget(vpAddBtn_); vpRow->addWidget(vpEditBtn_); vpRow->addWidget(vpDelBtn_);
    vl->addLayout(vpRow);
    connect(vpAddBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onAddViewport);
    connect(vpEditBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onEditViewport);
    connect(vpDelBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onDeleteViewport);
    root->addWidget(vpBox);

    // ---------------- 底部按钮 ----------------
    auto* btnRow = new QHBoxLayout();
    auto* previewBtn = new QPushButton(QStringLiteral("打印预览..."));
    auto* pdfBtn = new QPushButton(QStringLiteral("导出 PDF..."));
    auto* okBtn = new QPushButton(QStringLiteral("确定"));
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"));
    okBtn->setDefault(true);
    btnRow->addWidget(previewBtn); btnRow->addWidget(pdfBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn); btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);
    connect(previewBtn, &QPushButton::clicked, this, &PrintSettingsDialog::onPreview);
    connect(pdfBtn, &QPushButton::clicked, this, &PrintSettingsDialog::onExportPdf);
    connect(okBtn, &QPushButton::clicked, this, &PrintSettingsDialog::onAccept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    syncFromSettings();
    syncStyleList();
    syncViewportList();
}

void PrintSettingsDialog::refreshPaperSizeCombo() {
    const QPageSize::PageSizeId cur = s_.pageSize.id();
    int idx = 0;
    for (int i = 0; i < (int)(sizeof(kPaperIds)/sizeof(kPaperIds[0])); ++i)
        if (kPaperIds[i] == cur) { idx = i; break; }
    paperCombo_->setCurrentIndex(idx);
}

void PrintSettingsDialog::syncFromSettings() {
    refreshPaperSizeCombo();
    portraitBtn_->setChecked(s_.orientation == QPageLayout::Portrait);
    landscapeBtn_->setChecked(s_.orientation == QPageLayout::Landscape);
    marginL_->setValue(s_.marginsMm.left());
    marginT_->setValue(s_.marginsMm.top());
    marginR_->setValue(s_.marginsMm.right());
    marginB_->setValue(s_.marginsMm.bottom());
    scaleSpin_->setValue(s_.scale);
    fitBox_->setChecked(s_.fitToPage);
    centerHBox_->setChecked(s_.centerH);
    centerVBox_->setChecked(s_.centerV);
    borderBox_->setChecked(s_.drawBorder);
    lineWidthScale_->setValue(s_.lineWidthScale);
}

void PrintSettingsDialog::onPaperChanged(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(kPaperIds)/sizeof(kPaperIds[0]))) return;
    s_.pageSize = QPageSize(kPaperIds[idx]);
}

void PrintSettingsDialog::onStyleSelected() {
    const int row = styleList_->currentRow();
    if (row < 0) return;
    const QListWidgetItem* it = styleList_->item(row);
    if (!it) return;
    const QString key = it->data(Qt::UserRole).toString();
    // key 形如 "L:图层名" 或 "C:颜色名"
    if (key.startsWith("L:")) {
        const QString layer = key.mid(2);
        auto fit = s_.plotStyle.layerStyles().find(layer);
        if (fit != s_.plotStyle.layerStyles().end()) {
            styleTargetType_->setCurrentIndex(0);
            styleTargetName_->setText(layer);
            styleOverrideColor_->setChecked(fit->overrideColor);
            styleOverrideWidth_->setChecked(fit->overrideWidth);
            styleWidth_->setValue(fit->widthMm);
        }
    } else if (key.startsWith("C:")) {
        const QString colStr = key.mid(2);            // "#rrggbbaa"
        const int ci = (int)QColor(colStr).rgba();
        auto fit = s_.plotStyle.colorStyles().find(ci);
        if (fit != s_.plotStyle.colorStyles().end()) {
            styleTargetType_->setCurrentIndex(1);
            styleTargetName_->setText(colStr);
            styleOverrideColor_->setChecked(fit->overrideColor);
            styleOverrideWidth_->setChecked(fit->overrideWidth);
            styleWidth_->setValue(fit->widthMm);
        }
    }
}

void PrintSettingsDialog::onAddStyle() {
    PlotStyleEntry e;
    e.overrideColor = styleOverrideColor_->isChecked();
    e.overrideWidth = styleOverrideWidth_->isChecked();
    e.widthMm = styleWidth_->value();
    QColor chosen = styleColorPick_->property("chosen").value<QColor>();
    if (!chosen.isValid()) chosen = Qt::black;
    e.color = chosen;

    if (styleTargetType_->currentIndex() == 0) {
        const QString layer = styleTargetName_->text().trimmed();
        if (layer.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请填写图层名")); return; }
        s_.plotStyle.setLayerStyle(layer, e);
    } else {
        const QString colStr = styleTargetName_->text().trimmed();
        QColor col(colStr);
        if (!col.isValid()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要匹配的颜色")); return; }
        s_.plotStyle.setColorStyle(col, e);
    }
    syncStyleList();
}

void PrintSettingsDialog::onDeleteStyle() {
    const int row = styleList_->currentRow();
    if (row < 0) return;
    const QListWidgetItem* it = styleList_->item(row);
    if (!it) return;
    const QString key = it->data(Qt::UserRole).toString();
    if (key.startsWith("L:")) s_.plotStyle.clearLayer(key.mid(2));
    else if (key.startsWith("C:")) s_.plotStyle.clearColor(QColor(key.mid(2)));
    syncStyleList();
}

void PrintSettingsDialog::syncStyleList() {
    styleList_->clear();
    for (auto it = s_.plotStyle.layerStyles().begin(); it != s_.plotStyle.layerStyles().end(); ++it) {
        const PlotStyleEntry& e = it.value();
        const QString desc = QStringLiteral("图层[%1] → %2%3 %4")
            .arg(it.key())
            .arg(e.overrideColor ? e.color.name() : QStringLiteral("沿用"))
            .arg(e.overrideColor ? "" : "")
            .arg(e.overrideWidth ? QStringLiteral("线宽%1mm").arg(e.widthMm, 0, 'g', 2) : QStringLiteral("沿用线宽"));
        auto* item = new QListWidgetItem(desc);
        item->setData(Qt::UserRole, "L:" + it.key());
        styleList_->addItem(item);
    }
    for (auto it = s_.plotStyle.colorStyles().begin(); it != s_.plotStyle.colorStyles().end(); ++it) {
        const PlotStyleEntry& e = it.value();
        const QString desc = QStringLiteral("颜色[%1] → %2 %3")
            .arg(it.key(), 0, 16)
            .arg(e.overrideColor ? e.color.name() : QStringLiteral("沿用"))
            .arg(e.overrideWidth ? QStringLiteral("线宽%1mm").arg(e.widthMm, 0, 'g', 2) : QStringLiteral("沿用线宽"));
        auto* item = new QListWidgetItem(desc);
        item->setData(Qt::UserRole, "C:#" + QString::number(it.key(), 16));
        styleList_->addItem(item);
    }
}

void PrintSettingsDialog::onAddViewport() {
    PlotViewport vp;
    vp.name = QStringLiteral("视口%1").arg(s_.viewports.size() + 1);
    vp.modelRect = QRectF();   // 空 → 整体图形范围
    bool ok = false;
    const double sc = QInputDialog::getDouble(this, QStringLiteral("视口比例"),
                                             QStringLiteral("该视口的模型mm→图纸mm比例 (1=1:1):"),
                                             1.0, 0.01, 1000, 3, &ok);
    if (!ok) return;
    vp.scale = sc;
    s_.viewports.append(vp);
    syncViewportList();
}

void PrintSettingsDialog::onEditViewport() {
    const int row = vpList_->currentRow();
    if (row < 0 || row >= s_.viewports.size()) return;
    bool ok = false;
    const double sc = QInputDialog::getDouble(this, QStringLiteral("视口比例"),
                                             QStringLiteral("该视口的模型mm→图纸mm比例 (1=1:1):"),
                                             s_.viewports[row].scale > 0 ? s_.viewports[row].scale : 1.0,
                                             0.01, 1000, 3, &ok);
    if (!ok) return;
    s_.viewports[row].scale = sc;
    syncViewportList();
}

void PrintSettingsDialog::onDeleteViewport() {
    const int row = vpList_->currentRow();
    if (row < 0) return;
    s_.viewports.removeAt(row);
    syncViewportList();
}

void PrintSettingsDialog::syncViewportList() {
    vpList_->clear();
    if (s_.viewports.isEmpty()) {
        auto* item = new QListWidgetItem(QStringLiteral("（空）= 模型空间打印"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        vpList_->addItem(item);
        return;
    }
    for (int i = 0; i < s_.viewports.size(); ++i) {
        const PlotViewport& vp = s_.viewports[i];
        const QString rect = vp.modelRect.isNull()
            ? QStringLiteral("整体图形范围")
            : QStringLiteral("rect(%1,%2,%3×%4)").arg(vp.modelRect.x(),0,'g',2)
                .arg(vp.modelRect.y(),0,'g',2).arg(vp.modelRect.width(),0,'g',2).arg(vp.modelRect.height(),0,'g',2);
        vpList_->addItem(QStringLiteral("%1: %2 比例%3").arg(vp.name).arg(rect).arg(vp.scale));
    }
}

void PrintSettingsDialog::onPreview() {
    // 先回写页面设置到 s_
    s_.orientation = landscapeBtn_->isChecked() ? QPageLayout::Landscape : QPageLayout::Portrait;
    s_.marginsMm = QMarginsF(marginL_->value(), marginT_->value(), marginR_->value(), marginB_->value());
    s_.scale = scaleSpin_->value();
    s_.fitToPage = fitBox_->isChecked();
    s_.centerH = centerHBox_->isChecked();
    s_.centerV = centerVBox_->isChecked();
    s_.drawBorder = borderBox_->isChecked();
    s_.lineWidthScale = lineWidthScale_->value();
    PrintPreviewDialog dlg(model_, s_, this);
    dlg.exec();
}

void PrintSettingsDialog::onExportPdf() {
    s_.orientation = landscapeBtn_->isChecked() ? QPageLayout::Landscape : QPageLayout::Portrait;
    s_.marginsMm = QMarginsF(marginL_->value(), marginT_->value(), marginR_->value(), marginB_->value());
    s_.scale = scaleSpin_->value();
    s_.fitToPage = fitBox_->isChecked();
    s_.centerH = centerHBox_->isChecked();
    s_.centerV = centerVBox_->isChecked();
    s_.drawBorder = borderBox_->isChecked();
    s_.lineWidthScale = lineWidthScale_->value();

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PDF"),
                                                     QStringLiteral("SketchCAD.pdf"),
                                                     QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    if (PrintEngine::exportPdf(model_, s_, path))
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                QStringLiteral("已生成 PDF：%1").arg(path));
    else
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("生成 PDF 时出错。"));
}

void PrintSettingsDialog::onAccept() {
    s_.orientation = landscapeBtn_->isChecked() ? QPageLayout::Landscape : QPageLayout::Portrait;
    s_.marginsMm = QMarginsF(marginL_->value(), marginT_->value(), marginR_->value(), marginB_->value());
    s_.scale = scaleSpin_->value();
    s_.fitToPage = fitBox_->isChecked();
    s_.centerH = centerHBox_->isChecked();
    s_.centerV = centerVBox_->isChecked();
    s_.drawBorder = borderBox_->isChecked();
    s_.lineWidthScale = lineWidthScale_->value();
    accept();
}

} // namespace SketchCAD
