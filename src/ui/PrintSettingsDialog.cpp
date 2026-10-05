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
#include <QFile>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <cmath>

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

    // 样式表预设行：黑白出图一键切换（GB 预设 / 灰度抖动 / 导入 CTB）
    presetCombo_ = new QComboBox();
    presetCombo_->addItem(QStringLiteral("无（沿用图元颜色与线宽组）"));   // idx 0
    presetCombo_->addItem(QStringLiteral("GB 黑白出图（按线型分粗/细，纯黑不抖动）")); // idx 1
    presetCombo_->addItem(QStringLiteral("GB 灰度抖动（粗/细分线宽 + 中间调网点）"));  // idx 2
    presetCombo_->addItem(QStringLiteral("自定义 / 已导入 CTB"));         // idx 3
    importCtbBtn_ = new QPushButton(QStringLiteral("导入 CTB..."));
    monoBox_ = new QCheckBox(QStringLiteral("黑白出图(打印机/PDF)"));
    sl->addWidget(new QLabel(QStringLiteral("样式表预设")), 0, 0);
    sl->addWidget(presetCombo_, 0, 1, 1, 2);
    sl->addWidget(importCtbBtn_, 0, 3);
    sl->addWidget(monoBox_, 1, 0, 1, 2);

    styleTargetType_ = new QComboBox();
    styleTargetType_->addItem(QStringLiteral("按图层"));
    styleTargetType_->addItem(QStringLiteral("按颜色"));
    styleTargetType_->addItem(QStringLiteral("按线型"));
    sl->addWidget(new QLabel(QStringLiteral("匹配维度")), 2, 0);
    sl->addWidget(styleTargetType_, 2, 1);
    styleTargetName_ = new QLineEdit();
    styleTargetName_->setPlaceholderText(QStringLiteral("图层名(如 0)")); // 按颜色/线型时显示目标值
    sl->addWidget(new QLabel(QStringLiteral("目标")), 3, 0);
    sl->addWidget(styleTargetName_, 3, 1);
    styleColorPick_ = new QPushButton(QStringLiteral("选择颜色"));
    sl->addWidget(styleColorPick_, 3, 2);
    styleOverrideColor_ = new QCheckBox(QStringLiteral("覆盖颜色"));
    styleOverrideWidth_ = new QCheckBox(QStringLiteral("覆盖线宽"));
    styleWidth_ = new QDoubleSpinBox(); styleWidth_->setRange(0.05, 10); styleWidth_->setDecimals(2);
    styleWidth_->setSuffix(" mm");
    styleOverrideGray_ = new QCheckBox(QStringLiteral("灰度(黑白)"));
    styleGray_ = new QDoubleSpinBox(); styleGray_->setRange(0.0, 1.0); styleGray_->setDecimals(2);
    styleGray_->setSingleStep(0.05);
    styleDither_ = new QCheckBox(QStringLiteral("抖动"));
    sl->addWidget(styleOverrideColor_, 4, 0);
    sl->addWidget(styleOverrideWidth_, 4, 1);
    sl->addWidget(styleWidth_, 4, 2);
    sl->addWidget(styleOverrideGray_, 5, 0);
    sl->addWidget(styleGray_, 5, 1);
    sl->addWidget(styleDither_, 5, 2);
    styleAddBtn_ = new QPushButton(QStringLiteral("添加/更新"));
    styleDelBtn_ = new QPushButton(QStringLiteral("删除"));
    sl->addWidget(styleAddBtn_, 6, 1);
    sl->addWidget(styleDelBtn_, 6, 2);
    styleList_ = new QListWidget();
    sl->addWidget(styleList_, 7, 0, 1, 4);

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
    connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintSettingsDialog::onPresetChanged);
    connect(importCtbBtn_, &QPushButton::clicked, this, &PrintSettingsDialog::onImportCtb);
    connect(styleTargetType_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int t) {
        // t=0 图层（可键入）；t=1 颜色（弹选色）；t=2 线型（弹选列表）
        styleTargetName_->setEnabled(t == 0);
        if (t == 1) {
            QColor c = QColorDialog::getColor(Qt::black, this, QStringLiteral("选择要匹配的图元颜色"));
            if (c.isValid()) {
                styleTargetName_->setText(c.name(QColor::HexArgb));
                styleTargetName_->setEnabled(false);
            }
        } else if (t == 2) {
            bool ok = false;
            const QStringList names = {
                QStringLiteral("实线"), QStringLiteral("虚线"), QStringLiteral("点线"),
                QStringLiteral("点划线"), QStringLiteral("双点划线"), QStringLiteral("波浪线"),
                QStringLiteral("双折线"), QStringLiteral("粗虚线"), QStringLiteral("粗点画线") };
            const QString picked = QInputDialog::getItem(this, QStringLiteral("选择线型"),
                                                         QStringLiteral("线型"), names, 0, false, &ok);
            if (ok) {
                // 显示中文，内部按索引映射回 LineStyle 枚举（与 Element.h 顺序一致）
                const int idx = names.indexOf(picked);
                static const LineStyle kMap[9] = {
                    LineStyle::Solid, LineStyle::Dashed, LineStyle::Dotted,
                    LineStyle::DashDot, LineStyle::DashDotDot, LineStyle::Wave,
                    LineStyle::DoubleBreak, LineStyle::ThickDashed, LineStyle::ThickDashDot };
                if (idx >= 0) styleTargetName_->setText(plotStyleName(kMap[idx]));
                styleTargetName_->setEnabled(false);
            }
        } else {
            styleTargetName_->clear();
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
    monoBox_->setChecked(s_.monochrome);
    // 依据当前样式表内容反推预设下拉（能识别 GB 预设就高亮它，否则显示自定义）
    presetCombo_->blockSignals(true);
    if (s_.plotStyle.empty()) {
        presetCombo_->setCurrentIndex(0);
    } else if (s_.plotStyle.globalGrayOn()) {
        presetCombo_->setCurrentIndex(2);   // 灰度抖动预设
    } else if (!s_.plotStyle.styleStyles().isEmpty()) {
        presetCombo_->setCurrentIndex(1);   // GB 黑白（按线型）
    } else {
        presetCombo_->setCurrentIndex(3);   // 自定义
    }
    presetCombo_->blockSignals(false);
}

void PrintSettingsDialog::onPresetChanged(int idx) {
    switch (idx) {
    case 0:   // 无：清空样式表
        s_.plotStyle.clearAll();
        break;
    case 1:   // GB 黑白：按线型分粗/细，纯黑不抖动
        s_.plotStyle = PlotStyleTable::gbPreset(currentLineWidthGroup());
        break;
    case 2:   // GB 灰度抖动：粗/细分线宽 + 全局灰度抖动
        s_.plotStyle = PlotStyleTable::gbPresetGrayDither();
        break;
    case 3:   // 自定义：保持现状，由用户手工编辑
        break;
    default:
        break;
    }
    syncStyleList();
}

void PrintSettingsDialog::onImportCtb() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入简化 CTB 打印样式表"), QString(),
        QStringLiteral("CTB 样式表 (*.ctb *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法打开文件: %1").arg(path));
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    PlotStyleTable t;
    QString err;
    if (!PlotStyleTable::fromCtb(text, &t, &err)) {
        QMessageBox::warning(this, QStringLiteral("CTB 解析警告"),
                             QStringLiteral("存在无法解析的行（已跳过）：\n%1").arg(err));
    }
    s_.plotStyle = t;
    presetCombo_->blockSignals(true);
    presetCombo_->setCurrentIndex(3);   // 导入后视为自定义表
    presetCombo_->blockSignals(false);
    syncStyleList();
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
    // 统一回填所有可编辑字段（含灰度/抖动）
    auto fill = [this](const PlotStyleEntry& e) {
        styleOverrideColor_->setChecked(e.overrideColor);
        styleOverrideWidth_->setChecked(e.overrideWidth);
        styleWidth_->setValue(e.widthMm);
        styleOverrideGray_->setChecked(e.overrideGray);
        styleGray_->setValue(e.gray);
        styleDither_->setChecked(e.dither);
    };
    // key 形如 "L:图层名" / "C:颜色名" / "S:线型名"
    if (key.startsWith("L:")) {
        const QString layer = key.mid(2);
        auto fit = s_.plotStyle.layerStyles().find(layer);
        if (fit != s_.plotStyle.layerStyles().end()) {
            styleTargetType_->setCurrentIndex(0);
            styleTargetName_->setText(layer);
            fill(fit.value());
        }
    } else if (key.startsWith("C:")) {
        const QString colStr = key.mid(2);            // "#rrggbbaa"
        const int ci = (int)QColor(colStr).rgba();
        auto fit = s_.plotStyle.colorStyles().find(ci);
        if (fit != s_.plotStyle.colorStyles().end()) {
            styleTargetType_->setCurrentIndex(1);
            styleTargetName_->setText(colStr);
            fill(fit.value());
        }
    } else if (key.startsWith("S:")) {
        const int si = key.mid(2).toInt();
        auto fit = s_.plotStyle.styleStyles().find(si);
        if (fit != s_.plotStyle.styleStyles().end()) {
            styleTargetType_->setCurrentIndex(2);
            styleTargetName_->setText(plotStyleName((LineStyle)si));
            fill(fit.value());
        }
    }
}

void PrintSettingsDialog::onAddStyle() {
    PlotStyleEntry e;
    e.overrideColor = styleOverrideColor_->isChecked();
    e.overrideWidth = styleOverrideWidth_->isChecked();
    e.widthMm = styleWidth_->value();
    e.overrideGray = styleOverrideGray_->isChecked();
    e.gray = styleGray_->value();
    e.dither = styleDither_->isChecked();
    QColor chosen = styleColorPick_->property("chosen").value<QColor>();
    if (!chosen.isValid()) chosen = Qt::black;
    e.color = chosen;

    const int dim = styleTargetType_->currentIndex();
    if (dim == 0) {
        const QString layer = styleTargetName_->text().trimmed();
        if (layer.isEmpty()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请填写图层名")); return; }
        s_.plotStyle.setLayerStyle(layer, e);
    } else if (dim == 1) {
        const QString colStr = styleTargetName_->text().trimmed();
        QColor col(colStr);
        if (!col.isValid()) { QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要匹配的颜色")); return; }
        s_.plotStyle.setColorStyle(col, e);
    } else {
        LineStyle ls;
        if (!plotStyleFromName(styleTargetName_->text().trimmed(), &ls)) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择线型"));
            return;
        }
        s_.plotStyle.setStyleStyle(ls, e);
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
    else if (key.startsWith("S:")) s_.plotStyle.clearStyle((LineStyle)key.mid(2).toInt());
    syncStyleList();
}

void PrintSettingsDialog::syncStyleList() {
    styleList_->clear();
    // 条目描述统一附「灰度/抖动」后缀，便于黑白出图前目检
    auto desc = [](const PlotStyleEntry& e) {
        QString tail;
        if (e.overrideGray)
            tail = QStringLiteral(" 灰度%1%2")
                .arg(e.gray, 0, 'f', 2)
                .arg(e.dither ? QStringLiteral("(抖动)") : QString());
        else if (e.overrideWidth)
            tail = QStringLiteral(" 线宽%1mm").arg(e.widthMm, 0, 'g', 2);
        if (e.overrideGray && e.overrideWidth)
            tail += QStringLiteral(" 线宽%1mm").arg(e.widthMm, 0, 'g', 2);
        return tail;
    };
    for (auto it = s_.plotStyle.layerStyles().begin(); it != s_.plotStyle.layerStyles().end(); ++it) {
        const PlotStyleEntry& e = it.value();
        const QString d = QStringLiteral("图层[%1] → %2%3")
            .arg(it.key())
            .arg(e.overrideColor ? e.color.name() : QStringLiteral("沿用色"))
            .arg(desc(e));
        auto* item = new QListWidgetItem(d);
        item->setData(Qt::UserRole, "L:" + it.key());
        styleList_->addItem(item);
    }
    for (auto it = s_.plotStyle.colorStyles().begin(); it != s_.plotStyle.colorStyles().end(); ++it) {
        const PlotStyleEntry& e = it.value();
        const QString d = QStringLiteral("颜色[#%1] → %2%3")
            .arg(QString::number(it.key() & 0x00FFFFFF, 16).rightJustified(6, QLatin1Char('0')))
            .arg(e.overrideColor ? e.color.name() : QStringLiteral("沿用色"))
            .arg(desc(e));
        auto* item = new QListWidgetItem(d);
        item->setData(Qt::UserRole, "C:#" + QString::number(it.key(), 16));
        styleList_->addItem(item);
    }
    for (auto it = s_.plotStyle.styleStyles().begin(); it != s_.plotStyle.styleStyles().end(); ++it) {
        const PlotStyleEntry& e = it.value();
        const QString d = QStringLiteral("线型[%1] → %2%3")
            .arg(plotStyleName((LineStyle)it.key()))
            .arg(e.overrideColor ? e.color.name() : QStringLiteral("灰度黑"))
            .arg(desc(e));
        auto* item = new QListWidgetItem(d);
        item->setData(Qt::UserRole, "S:" + QString::number(it.key()));
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
    // 视口旋转角（消费 PlotViewport::rotationDeg；0 = 不旋转）
    const double rot = QInputDialog::getDouble(this, QStringLiteral("视口旋转"),
                                               QStringLiteral("视口内容旋转角（度，逆时针；0 = 不旋转）:"),
                                               s_.viewports[row].rotationDeg, -360.0, 360.0, 2, &ok);
    if (ok) s_.viewports[row].rotationDeg = rot;
    // per-viewport 冻结：勾选的图层仅在该视口出图中隐藏（模型空间与其他视口正常）
    if (model_ && model_->layerCount() > 0) {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("视口冻结图层 - %1").arg(s_.viewports[row].name));
        auto* lay = new QVBoxLayout(&dlg);
        lay->addWidget(new QLabel(QStringLiteral("勾选 = 该视口内不输出（其他视口与模型空间不受影响）："), &dlg));
        auto* area = new QScrollArea(&dlg);
        auto* box = new QWidget(&dlg);
        auto* grid = new QVBoxLayout(box);
        grid->setContentsMargins(2, 2, 2, 2);
        QList<QCheckBox*> boxes;
        for (int i = 0; i < model_->layerCount(); ++i) {
            const QString ln = model_->layerAt(i).name;
            auto* cb = new QCheckBox(ln, box);
            cb->setChecked(s_.viewports[row].frozenLayers.contains(ln));
            grid->addWidget(cb);
            boxes.append(cb);
        }
        box->setLayout(grid);
        area->setWidget(box);
        area->setWidgetResizable(true);
        area->setMinimumSize(260, 200);
        lay->addWidget(area);
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(btns);
        if (dlg.exec() == QDialog::Accepted) {
            s_.viewports[row].frozenLayers.clear();
            for (const QCheckBox* cb : boxes)
                if (cb->isChecked()) s_.viewports[row].frozenLayers.append(cb->text());
        }
    }
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
        QString tail;
        if (std::fabs(vp.rotationDeg) > 1e-9) tail += QStringLiteral(" 旋转%1°").arg(vp.rotationDeg, 0, 'g', 3);
        if (!vp.frozenLayers.isEmpty()) tail += QStringLiteral(" 冻结%1层").arg(vp.frozenLayers.size());
        vpList_->addItem(QStringLiteral("%1: %2 比例%3%4").arg(vp.name).arg(rect).arg(vp.scale).arg(tail));
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
    s_.monochrome = monoBox_->isChecked();
    accept();
}

} // namespace SketchCAD
