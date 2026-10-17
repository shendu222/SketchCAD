#include "SheetFrameDialog.h"
#include "core/Element.h"
#include "core/Vec.h"
#include "core/SheetFrameTemplate.h"
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>   // 比例计算子对话框
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QScrollArea>
#include <QInputDialog>
#include <QListWidget>
#include <QLineEdit>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// 把生成图元渲染为预览位图（与落盘同源，仅用于展示）。Y 轴翻转（世界 Y 向上 → 屏幕 Y 向下）。
static QPixmap renderPreview(const std::vector<Element>& els, int w, int h) {
    QPixmap pm(w, h);
    pm.fill(Qt::white);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    bool any = false;
    for (const auto& el : els) {
        for (const auto& v : el.pts) {
            if (v.x < minX) { minX = v.x; }
            if (v.x > maxX) { maxX = v.x; }
            if (v.y < minY) { minY = v.y; }
            if (v.y > maxY) { maxY = v.y; }
            any = true;
        }
    }
    if (!any) return pm;
    const double m = 10.0;
    const double spanX = std::max(1e-6, maxX - minX);
    const double spanY = std::max(1e-6, maxY - minY);
    const double s = std::min((w - 2 * m) / spanX, (h - 2 * m) / spanY);
    auto X = [&](double x) { return m + (x - minX) * s; };
    auto Y = [&](double y) { return h - (m + (y - minY) * s); };

    for (const auto& el : els) {
        const QColor col = el.color.isValid() ? el.color : QColor(26, 29, 40);
        if (el.type == ElementType::Line && el.pts.size() >= 2) {
            QPen pen(col); pen.setWidthF(qMax(1.0, el.lineWidth * 0.6));
            p.setPen(pen);
            p.drawLine(X(el.pts[0].x), Y(el.pts[0].y), X(el.pts[1].x), Y(el.pts[1].y));
        } else if (el.type == ElementType::Rectangle && el.pts.size() >= 2) {
            QPen pen(col); pen.setWidthF(qMax(1.0, el.lineWidth * 0.6));
            p.setPen(pen);
            const double x0 = X(el.pts[0].x), y0 = Y(el.pts[0].y);
            const double x1 = X(el.pts[1].x), y1 = Y(el.pts[1].y);
            p.drawRect(QRectF(std::min(x0, x1), std::min(y0, y1),
                              std::fabs(x1 - x0), std::fabs(y1 - y0)));
        } else if (el.type == ElementType::Text && !el.text.isEmpty()) {
            QFont f = p.font();
            f.setPixelSize(qMax(6, (int)(el.radius * s)));
            p.setFont(f);
            p.setPen(col);
            p.drawText(X(el.pts[0].x), Y(el.pts[0].y), el.text);
        }
    }
    return pm;
}

// 绘图比例前值/后值候选（GB/T 14690 数值系列；下拉可编辑，允许任意正值）
static const double kScaleValues[] = {
    1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 10.0, 20.0, 50.0, 100.0,
};

static QString scaleText(double v) {
    return (v == std::floor(v)) ? QString::number((long long)v)
                                : QString::number(v, 'g', 6);
}

// 各栏「预览内容」按钮的栏类别（showBarPreview 消费）
enum BarPreviewKind { BarTitle = 0, BarBom, BarAdd, BarCode, BarParam };

SheetFrameDialog::SheetFrameDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("新建图框 — GB/T 14689"));
    resize(1040, 800);
    buildUi();

    // ---- 模板（需求 13）：加载后默认应用 sample（内置模板 = 附图示例状态）----
    templates_ = loadFrameTemplates();
    int sel = 0;
    for (int i = 0; i < templates_.size(); ++i)
        if (templates_[i].name == QStringLiteral("sample")) { sel = i; break; }
    cbTemplate_->blockSignals(true);
    for (const auto& t : templates_) cbTemplate_->addItem(t.name);
    cbTemplate_->setCurrentIndex(sel);
    cbTemplate_->blockSignals(false);
    if (!templates_.isEmpty())
        applyTemplate(templates_[sel]);

    validateZones();
    updatePreview();
}

void SheetFrameDialog::buildUi() {
    auto* root = new QHBoxLayout(this);

    // ============================ 左列 ============================
    auto* leftCol = new QVBoxLayout();

    // ---- 模板（需求 13）----
    auto* gbTpl = new QGroupBox(QStringLiteral("模板"), this);
    auto* tplLay = new QHBoxLayout(gbTpl);
    tplLay->addWidget(new QLabel(QStringLiteral("模板名称:"), gbTpl));
    cbTemplate_ = new QComboBox(gbTpl);
    cbTemplate_->setMinimumWidth(150);
    tplLay->addWidget(cbTemplate_, 1);
    btnTplSave_ = new QPushButton(QStringLiteral("存为模板"), gbTpl);
    btnTplManage_ = new QPushButton(QStringLiteral("管理模板"), gbTpl);
    tplLay->addWidget(btnTplSave_);
    tplLay->addWidget(btnTplManage_);
    connect(cbTemplate_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i) {
                if (updating_ || i < 0 || i >= templates_.size()) return;
                applyTemplate(templates_[i]);
            });
    connect(btnTplSave_, &QPushButton::clicked, this, &SheetFrameDialog::saveCurrentAsTemplate);
    connect(btnTplManage_, &QPushButton::clicked, this, &SheetFrameDialog::manageTemplates);
    leftCol->addWidget(gbTpl);

    // ---- 图幅（A0~A4 单选 / 加长图幅+下拉 / 自定义+长L宽W，附图布局）----
    auto* gbFmt = new QGroupBox(QStringLiteral("图幅"), this);
    auto* fmtLay = new QGridLayout(gbFmt);
    grpFmt_ = new QButtonGroup(this);
    // 基本幅面单选行（附图：A0~A4 五项一行；A5 仍属核心枚举，仅对话框不提供）
    auto* fmtRow = new QHBoxLayout();
    static const QPair<SheetFormat, QString> kFmts[] = {
        {SheetFormat::A0, QStringLiteral("A0")}, {SheetFormat::A1, QStringLiteral("A1")},
        {SheetFormat::A2, QStringLiteral("A2")}, {SheetFormat::A3, QStringLiteral("A3")},
        {SheetFormat::A4, QStringLiteral("A4")},
    };
    for (const auto& f : kFmts) {
        auto* rb = new QRadioButton(f.second, gbFmt);
        grpFmt_->addButton(rb, (int)f.first);
        fmtRow->addWidget(rb);
    }
    fmtLay->addLayout(fmtRow, 0, 0, 1, 2);
    // 加长图幅（单选 + 下拉；下拉仅在选中时可用）
    rbExt_ = new QRadioButton(QStringLiteral("加长图幅"), gbFmt);
    grpFmt_->addButton(rbExt_, 100);
    cbExt_ = new QComboBox(gbFmt);
    cbExt_->setEnabled(false);
    for (const auto& p : extendedSheetPresets())
        cbExt_->addItem(QStringLiteral("%1 (%2×%3)").arg(p.label)
                            .arg(p.longSide, 0, 'f', 0).arg(p.shortSide, 0, 'f', 0),
                        QVariant::fromValue((int)p.base * 100 + p.factor));
    fmtLay->addWidget(rbExt_, 1, 0);
    fmtLay->addWidget(cbExt_, 1, 1);
    // 自定义（单选 + 长L/宽W）
    rbCustom_ = new QRadioButton(QStringLiteral("自定义"), gbFmt);
    grpFmt_->addButton(rbCustom_, 101);
    fmtLay->addWidget(rbCustom_, 2, 0, 1, 2);
    auto* custRow = new QHBoxLayout();
    sbCW_ = new QDoubleSpinBox(gbFmt); sbCW_->setRange(10, 100000); sbCW_->setValue(594);
    sbCW_->setSuffix(QStringLiteral(" mm")); sbCW_->setEnabled(false);
    sbCH_ = new QDoubleSpinBox(gbFmt); sbCH_->setRange(10, 100000); sbCH_->setValue(420);
    sbCH_->setSuffix(QStringLiteral(" mm")); sbCH_->setEnabled(false);
    custRow->addWidget(new QLabel(QStringLiteral("长L:"), gbFmt)); custRow->addWidget(sbCW_, 1);
    custRow->addWidget(new QLabel(QStringLiteral("宽W:"), gbFmt)); custRow->addWidget(sbCH_, 1);
    fmtLay->addLayout(custRow, 3, 0, 1, 2);

    connect(grpFmt_, QOverload<int, bool>::of(&QButtonGroup::idToggled), this,
            [this](int id, bool on) {
                if (updating_ || !on) return;
                if (id == 100)      opts_.sizeMode = SheetFrameOptions::SizeMode::Extended;
                else if (id == 101) opts_.sizeMode = SheetFrameOptions::SizeMode::Custom;
                else { opts_.sizeMode = SheetFrameOptions::SizeMode::Basic;
                       opts_.basicFormat = (SheetFormat)id; }
                updatePreview();
            });
    connect(cbExt_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (updating_ || i < 0) return;
        const int key = cbExt_->itemData(i).toInt();
        opts_.extBase = (SheetFormat)(key / 100);
        opts_.extFactor = key % 100;
        if (opts_.sizeMode == SheetFrameOptions::SizeMode::Extended) updatePreview();
    });
    connect(sbCW_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { if (updating_) return; opts_.customW = v;
                               if (opts_.sizeMode==SheetFrameOptions::SizeMode::Custom) updatePreview(); });
    connect(sbCH_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { if (updating_) return; opts_.customH = v;
                               if (opts_.sizeMode==SheetFrameOptions::SizeMode::Custom) updatePreview(); });
    leftCol->addWidget(gbFmt);

    // ---- 布置方式 + 绘图比例（附图：同一组框，无标题）----
    auto* gbLayout = new QGroupBox(QString(), this);
    auto* layGrid = new QGridLayout(gbLayout);
    layGrid->addWidget(new QLabel(QStringLiteral("布置方式:"), gbLayout), 0, 0);
    auto* oriRow = new QHBoxLayout();
    rbLand_ = new QRadioButton(QStringLiteral("横置"), gbLayout);
    rbPort_ = new QRadioButton(QStringLiteral("竖置"), gbLayout);
    rbLand_->setChecked(true);
    oriRow->addWidget(rbLand_); oriRow->addSpacing(40); oriRow->addWidget(rbPort_);
    layGrid->addLayout(oriRow, 0, 1, 1, 2);
    connect(rbLand_, &QRadioButton::toggled, this,
            [this](bool on) { if (!updating_ && on) { opts_.landscape = true; updatePreview(); } });
    connect(rbPort_, &QRadioButton::toggled, this,
            [this](bool on) { if (!updating_ && on) { opts_.landscape = false; updatePreview(); } });

    layGrid->addWidget(new QLabel(QStringLiteral("绘图比例:"), gbLayout), 1, 0);
    auto* scaleRow = new QHBoxLayout();
    cbScaleNum_ = new QComboBox(gbLayout);   // 前值（附图：第一个下拉）
    cbScaleDen_ = new QComboBox(gbLayout);   // 后值（附图：第二个下拉）
    for (double v : kScaleValues) {
        cbScaleNum_->addItem(scaleText(v), v);
        cbScaleDen_->addItem(scaleText(v), v);
    }
    cbScaleNum_->setEditable(true); cbScaleDen_->setEditable(true);
    cbScaleNum_->setCurrentText(QStringLiteral("1")); cbScaleDen_->setCurrentText(QStringLiteral("1"));
    scaleRow->addWidget(cbScaleNum_, 1);
    scaleRow->addWidget(new QLabel(QStringLiteral(":"), gbLayout));
    scaleRow->addWidget(cbScaleDen_, 1);
    layGrid->addLayout(scaleRow, 1, 1, 1, 2);
    auto onScaleChanged = [this]() {
        if (updating_) return;
        bool nOk = false, dOk = false;
        const double n = cbScaleNum_->currentText().toDouble(&nOk);
        const double d = cbScaleDen_->currentText().toDouble(&dOk);
        if (nOk && dOk && n > 0.0 && d > 0.0) { scaleNum_ = n; scaleDen_ = d; }
        updatePreview();
    };
    connect(cbScaleNum_, &QComboBox::currentTextChanged, this, onScaleChanged);
    connect(cbScaleDen_, &QComboBox::currentTextChanged, this, onScaleChanged);

    // 计算(C)<：由图上长度/实际长度反推比例并回填两个下拉
    btnScaleCalc_ = new QPushButton(QStringLiteral("计算(C)<"), gbLayout);
    layGrid->addWidget(btnScaleCalc_, 2, 1, 1, 2);
    connect(btnScaleCalc_, &QPushButton::clicked, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("比例计算"));
        auto* fl = new QFormLayout(&dlg);
        auto* spImg  = new QDoubleSpinBox(&dlg);   // 图上长度
        auto* spReal = new QDoubleSpinBox(&dlg);   // 实际长度
        spImg->setRange(0.001, 1e7);  spImg->setValue(100.0);  spImg->setSuffix(QStringLiteral(" mm"));
        spReal->setRange(0.001, 1e7); spReal->setValue(100.0); spReal->setSuffix(QStringLiteral(" mm"));
        fl->addRow(QStringLiteral("图上长度:"), spImg);
        fl->addRow(QStringLiteral("实际长度:"), spReal);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &dlg);
        box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
        box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        fl->addRow(box);
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        // 比例 = 图上:实际（a:b，a/b 即 ratio）；整数对用最大公约数化简
        double a = spImg->value(), b = spReal->value();
        if (a <= 0.0 || b <= 0.0) return;
        if (a == std::floor(a) && b == std::floor(b)) {
            long long ia = (long long)a, ib = (long long)b, x = ia, y = ib;
            while (y) { const long long t = x % y; x = y; y = t; }
            if (x > 0) { ia /= x; ib /= x; a = (double)ia; b = (double)ib; }
        }
        scaleNum_ = a; scaleDen_ = b;
        cbScaleNum_->setCurrentText(scaleText(a));
        cbScaleDen_->setCurrentText(scaleText(b));
        updatePreview();
    });
    leftCol->addWidget(gbLayout);

    // ---- 分区（GB/T 14689：偶数 + 25~75mm 实时校验）----
    auto* gbZone = new QGroupBox(QStringLiteral("分区"), this);
    gbZone->setCheckable(true); gbZone->setChecked(false);
    auto* zoneLay = new QGridLayout(gbZone);
    sbZoneX_ = new QSpinBox(gbZone); sbZoneX_->setRange(2, 40); sbZoneX_->setSingleStep(2);
    sbZoneX_->setValue(opts_.zoneCountX);
    sbZoneY_ = new QSpinBox(gbZone); sbZoneY_->setRange(2, 40); sbZoneY_->setSingleStep(2);
    sbZoneY_->setValue(opts_.zoneCountY);
    zoneLay->addWidget(new QLabel(QStringLiteral("长度分区数:"), gbZone), 0, 0);
    zoneLay->addWidget(sbZoneX_, 0, 1);
    zoneLay->addWidget(new QLabel(QStringLiteral("宽度分区数:"), gbZone), 1, 0);
    zoneLay->addWidget(sbZoneY_, 1, 1);
    lblZone_ = new QLabel(QStringLiteral("分区默认关闭"), gbZone);
    zoneLay->addWidget(lblZone_, 2, 0, 1, 2);
    connect(gbZone, &QGroupBox::toggled, this,
            [this](bool on) { if (updating_) return; opts_.zonesEnabled = on;
                              validateZones(); updatePreview(); });
    connect(sbZoneX_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { if (updating_) return; opts_.zoneCountX = v; validateZones(); });
    connect(sbZoneY_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { if (updating_) return; opts_.zoneCountY = v; validateZones(); });
    leftCol->addWidget(gbZone);

    // ---- 底部选项组（附图：自动填写日期/对中符号/装订线/指定图框位置/装订宽度 + 确定取消）----
    auto* gbAux = new QGroupBox(QString(), this);
    auto* auxGrid = new QGridLayout(gbAux);
    chkAutoDate_ = new QCheckBox(QStringLiteral("自动填写日期"), gbAux);
    chkCenter_   = new QCheckBox(QStringLiteral("对中符号"), gbAux);
    chkBinding_  = new QCheckBox(QStringLiteral("装订线"), gbAux);
    chkBinding_->setChecked(true);
    chkFramePos_ = new QCheckBox(QStringLiteral("指定图框位置"), gbAux);
    auxGrid->addWidget(chkAutoDate_, 0, 0);
    auxGrid->addWidget(chkCenter_,   0, 1);
    auxGrid->addWidget(chkBinding_,  1, 0);
    auxGrid->addWidget(chkFramePos_, 1, 1);
    auto* bindRow = new QHBoxLayout();
    bindRow->addWidget(new QLabel(QStringLiteral("装订宽度"), gbAux));
    sbBindingW_ = new QSpinBox(gbAux); sbBindingW_->setRange(5, 100);
    sbBindingW_->setValue((int)opts_.bindingWidth);
    bindRow->addWidget(sbBindingW_, 1);
    auxGrid->addLayout(bindRow, 2, 0, 1, 2);
    connect(chkAutoDate_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) opts_.autoFillDate = on; });
    connect(chkCenter_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) { opts_.centerMarks = on; updatePreview(); } });
    connect(chkBinding_, &QCheckBox::toggled, this, [this](bool on) {
        if (updating_) return;
        opts_.bindingMargin = on;
        sbBindingW_->setEnabled(on);
        updatePreview();
    });
    connect(chkFramePos_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) opts_.specifyFramePos = on; });
    connect(sbBindingW_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { if (updating_) return; opts_.bindingWidth = v; updatePreview(); });

    // 确定 / 取消（附图：确定居左、取消居右）
    auto* btnRow = new QHBoxLayout();
    auto* btnOk = new QPushButton(QStringLiteral("确定"), gbAux);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), gbAux);
    btnRow->addWidget(btnOk);
    btnRow->addStretch(1);
    btnRow->addWidget(btnCancel);
    auxGrid->addLayout(btnRow, 3, 0, 1, 2);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    leftCol->addWidget(gbAux);
    leftCol->addStretch(1);

    auto* leftScroll = new QScrollArea(this);
    leftScroll->setWidgetResizable(true);
    auto* leftWrap = new QWidget(leftScroll);
    leftWrap->setLayout(leftCol);
    leftScroll->setWidget(leftWrap);
    root->addWidget(leftScroll, 5);

    // ============================ 右列 ============================
    auto* rightCol = new QVBoxLayout();

    // ---- 实时预览 ----
    rightCol->addWidget(new QLabel(QStringLiteral("实时预览"), this));
    previewLabel_ = new QLabel(this);
    previewLabel_->setMinimumSize(380, 300);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setAlignment(Qt::AlignCenter);
    rightCol->addWidget(previewLabel_, 1);

    // ---- 各栏（附图顺序：标题栏→明细栏→附加栏→代号栏→参数栏；开关+下拉+预览）----
    auto* barGrid = new QGridLayout();
    int barRow = 0;
    auto addBarRow = [&](QCheckBox* chk, const QString& text, bool checked,
                         QComboBox*& combo, QPushButton*& btn) {
        chk->setText(text);
        chk->setChecked(checked);
        barGrid->addWidget(chk, barRow, 0);
        barGrid->addWidget(combo, barRow, 1);
        barGrid->addWidget(btn, barRow, 2);
        ++barRow;
    };
    chkTitle_ = new QCheckBox(this);
    cbTitleStyle_ = new QComboBox(this);
    cbTitleStyle_->addItem(QStringLiteral("标题栏1"));
    cbTitleStyle_->setMinimumWidth(140);
    btnPrevTitle_ = new QPushButton(QStringLiteral("预览"), this);
    addBarRow(chkTitle_, QStringLiteral("标题栏"), true, cbTitleStyle_, btnPrevTitle_);

    chkBom_ = new QCheckBox(this);
    cbBomStyle_ = new QComboBox(this);
    cbBomStyle_->addItem(QStringLiteral("mx"));
    btnPrevBom_ = new QPushButton(QStringLiteral("预览"), this);
    addBarRow(chkBom_, QStringLiteral("明细栏"), false, cbBomStyle_, btnPrevBom_);

    chkAddBar_ = new QCheckBox(this);
    cbAddBar_ = new QComboBox(this);
    for (const auto& p : additionalBarPresets()) cbAddBar_->addItem(p.label);
    btnPrevAddBar_ = new QPushButton(QStringLiteral("预览"), this);
    addBarRow(chkAddBar_, QStringLiteral("附加栏"), false, cbAddBar_, btnPrevAddBar_);
    // 是否显示左分区（附图：附加栏行下缩进子复选；控制分区左带字母 A..n）
    chkZoneLeft_ = new QCheckBox(QStringLiteral("是否显示左分区"), this);
    barGrid->addWidget(chkZoneLeft_, barRow, 1);
    ++barRow;

    chkCodeBar_ = new QCheckBox(this);
    cbCodeBar_ = new QComboBox(this);
    for (const auto& p : codeBarPresets()) cbCodeBar_->addItem(p.label);
    btnPrevCodeBar_ = new QPushButton(QStringLiteral("预览"), this);
    addBarRow(chkCodeBar_, QStringLiteral("代号栏"), false, cbCodeBar_, btnPrevCodeBar_);

    chkParamBar_ = new QCheckBox(this);
    cbParamBar_ = new QComboBox(this);
    for (const auto& p : paramBarPresets()) cbParamBar_->addItem(p.label);
    btnPrevParamBar_ = new QPushButton(QStringLiteral("预览"), this);
    addBarRow(chkParamBar_, QStringLiteral("参数栏"), false, cbParamBar_, btnPrevParamBar_);

    rightCol->addLayout(barGrid);

    // 开关联动：关闭时禁止选取该类型（需求 10/11/12），预览按钮同步禁用
    auto bindBarToggle = [this](QCheckBox* chk, QComboBox* combo, QPushButton* btn) {
        connect(chk, &QCheckBox::toggled, this, [this, chk, combo, btn](bool on) {
            combo->setEnabled(on);
            btn->setEnabled(on);
            if (updating_) return;
            if (chk == chkTitle_)
                opts_.titleBlock = on ? TitleBlockStyle::Standard : TitleBlockStyle::None;
            else if (chk == chkBom_)   opts_.bomEnabled = on;
            else if (chk == chkAddBar_) opts_.additionalBarEnabled = on;
            else if (chk == chkCodeBar_) opts_.codeBarEnabled = on;
            else if (chk == chkParamBar_) opts_.paramBarEnabled = on;
            updatePreview();
        });
    };
    bindBarToggle(chkTitle_, cbTitleStyle_, btnPrevTitle_);
    bindBarToggle(chkBom_, cbBomStyle_, btnPrevBom_);
    bindBarToggle(chkAddBar_, cbAddBar_, btnPrevAddBar_);
    bindBarToggle(chkCodeBar_, cbCodeBar_, btnPrevCodeBar_);
    bindBarToggle(chkParamBar_, cbParamBar_, btnPrevParamBar_);
    connect(chkZoneLeft_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) { opts_.zoneShowLeft = on; updatePreview(); } });
    // 预设下拉 → opts_（key 与注册表同源）
    connect(cbAddBar_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (updating_ || i < 0 || i >= additionalBarPresets().size()) return;
        opts_.additionalBarKey = additionalBarPresets()[i].key;
        if (opts_.additionalBarEnabled) updatePreview();
    });
    connect(cbCodeBar_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (updating_ || i < 0 || i >= codeBarPresets().size()) return;
        opts_.codeBarKey = codeBarPresets()[i].key;
        if (opts_.codeBarEnabled) updatePreview();
    });
    connect(cbParamBar_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (updating_ || i < 0 || i >= paramBarPresets().size()) return;
        opts_.paramBarKey = paramBarPresets()[i].key;
        if (opts_.paramBarEnabled) updatePreview();
    });
    // 各栏「预览内容」：仅该栏图元的独立预览窗
    connect(btnPrevTitle_,  &QPushButton::clicked, this, [this] { showBarPreview(BarTitle); });
    connect(btnPrevBom_,    &QPushButton::clicked, this, [this] { showBarPreview(BarBom); });
    connect(btnPrevAddBar_, &QPushButton::clicked, this, [this] { showBarPreview(BarAdd); });
    connect(btnPrevCodeBar_,&QPushButton::clicked, this, [this] { showBarPreview(BarCode); });
    connect(btnPrevParamBar_,&QPushButton::clicked, this, [this] { showBarPreview(BarParam); });

    // ---- 比例调整设置（附图右下组）----
    auto* gbAdj = new QGroupBox(QStringLiteral("比例调整设置"), this);
    auto* adjLay = new QVBoxLayout(gbAdj);
    chkDimAdj_ = new QCheckBox(QStringLiteral("标注随图框比例调整"), gbAdj);
    chkDimAdj_->setChecked(true);
    adjLay->addWidget(chkDimAdj_);
    chkTextAdj_ = new QCheckBox(QStringLiteral("文字随图框比例调整"), gbAdj);
    chkTextAdj_->setChecked(true);
    adjLay->addWidget(chkTextAdj_);
    auto* thRow = new QHBoxLayout();
    thRow->addSpacing(20);
    chkTextInitH_ = new QCheckBox(QStringLiteral("指定初始高度:"), gbAdj);
    sbTextInitH_ = new QDoubleSpinBox(gbAdj);
    sbTextInitH_->setRange(0.5, 20.0); sbTextInitH_->setSingleStep(0.5);
    sbTextInitH_->setValue(opts_.textInitHeight);
    sbTextInitH_->setSuffix(QStringLiteral(" mm")); sbTextInitH_->setEnabled(false);
    thRow->addWidget(chkTextInitH_); thRow->addWidget(sbTextInitH_, 1);
    adjLay->addLayout(thRow);
    chkLwAdj_ = new QCheckBox(QStringLiteral("线宽随图框比例调整"), gbAdj);
    chkLwAdj_->setChecked(true);
    adjLay->addWidget(chkLwAdj_);
    auto* lwRow = new QHBoxLayout();
    lwRow->addSpacing(20);
    chkLwInit_ = new QCheckBox(QStringLiteral("指定初始比例:"), gbAdj);
    sbLwInit_ = new QDoubleSpinBox(gbAdj);
    sbLwInit_->setRange(0.05, 5.0); sbLwInit_->setSingleStep(0.05);
    sbLwInit_->setValue(opts_.lineWidthInitRatio);
    sbLwInit_->setEnabled(false);
    lwRow->addWidget(chkLwInit_); lwRow->addWidget(sbLwInit_, 1);
    adjLay->addLayout(lwRow);
    connect(chkDimAdj_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) opts_.dimScaleAdjust = on; });
    connect(chkTextAdj_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) opts_.textScaleAdjust = on; });
    connect(chkTextInitH_, &QCheckBox::toggled, this, [this](bool on) {
        sbTextInitH_->setEnabled(on);
        if (!updating_) { opts_.textInitHeightEnabled = on; updatePreview(); }
    });
    connect(sbTextInitH_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { if (!updating_) { opts_.textInitHeight = v; updatePreview(); } });
    connect(chkLwAdj_, &QCheckBox::toggled, this,
            [this](bool on) { if (!updating_) opts_.lineWidthScaleAdjust = on; });
    connect(chkLwInit_, &QCheckBox::toggled, this, [this](bool on) {
        sbLwInit_->setEnabled(on);
        if (!updating_) { opts_.lineWidthInitEnabled = on; updatePreview(); }
    });
    connect(sbLwInit_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { if (!updating_) { opts_.lineWidthInitRatio = v; updatePreview(); } });
    rightCol->addWidget(gbAdj);
    rightCol->addStretch(1);

    root->addLayout(rightCol, 6);
}

// ---- 模板应用：opts_ + 比例 → 全部控件整批回填（updating_ 抑制联动）----
void SheetFrameDialog::loadOptsToUi() {
    // 图幅单选（A5 不在对话框候选中，回落 A3 展示）
    if ((int)opts_.basicFormat > (int)SheetFormat::A4)
        opts_.basicFormat = SheetFormat::A3;
    const int fmtId = (opts_.sizeMode == SheetFrameOptions::SizeMode::Custom)  ? 101
                    : (opts_.sizeMode == SheetFrameOptions::SizeMode::Extended) ? 100
                    : (int)opts_.basicFormat;
    if (QAbstractButton* b = grpFmt_->button(fmtId)) b->setChecked(true);
    cbExt_->setEnabled(opts_.sizeMode == SheetFrameOptions::SizeMode::Extended);
    sbCW_->setEnabled(opts_.sizeMode == SheetFrameOptions::SizeMode::Custom);
    sbCH_->setEnabled(opts_.sizeMode == SheetFrameOptions::SizeMode::Custom);
    const int extKey = (int)opts_.extBase * 100 + opts_.extFactor;
    for (int i = 0; i < cbExt_->count(); ++i)
        if (cbExt_->itemData(i).toInt() == extKey) { cbExt_->setCurrentIndex(i); break; }
    sbCW_->setValue(opts_.customW);
    sbCH_->setValue(opts_.customH);

    // 布置方式 + 绘图比例
    (opts_.landscape ? rbLand_ : rbPort_)->setChecked(true);
    cbScaleNum_->setCurrentText(scaleText(scaleNum_));
    cbScaleDen_->setCurrentText(scaleText(scaleDen_));

    // 分区
    QGroupBox* gbZone = qobject_cast<QGroupBox*>(sbZoneX_->parentWidget());
    if (gbZone) gbZone->setChecked(opts_.zonesEnabled);
    sbZoneX_->setValue(opts_.zoneCountX);
    sbZoneY_->setValue(opts_.zoneCountY);

    // 底部选项
    chkAutoDate_->setChecked(opts_.autoFillDate);
    chkCenter_->setChecked(opts_.centerMarks);
    chkBinding_->setChecked(opts_.bindingMargin);
    chkFramePos_->setChecked(opts_.specifyFramePos);
    sbBindingW_->setValue((int)opts_.bindingWidth);
    sbBindingW_->setEnabled(opts_.bindingMargin);

    // 各栏
    chkTitle_->setChecked(opts_.titleBlock != TitleBlockStyle::None);
    chkBom_->setChecked(opts_.bomEnabled);
    const auto& addPresets = additionalBarPresets();
    for (int i = 0; i < addPresets.size(); ++i)
        if (addPresets[i].key == opts_.additionalBarKey) { cbAddBar_->setCurrentIndex(i); break; }
    const auto& codePresets = codeBarPresets();
    for (int i = 0; i < codePresets.size(); ++i)
        if (codePresets[i].key == opts_.codeBarKey) { cbCodeBar_->setCurrentIndex(i); break; }
    const auto& paramPresets = paramBarPresets();
    for (int i = 0; i < paramPresets.size(); ++i)
        if (paramPresets[i].key == opts_.paramBarKey) { cbParamBar_->setCurrentIndex(i); break; }
    chkAddBar_->setChecked(opts_.additionalBarEnabled);
    chkCodeBar_->setChecked(opts_.codeBarEnabled);
    chkParamBar_->setChecked(opts_.paramBarEnabled);
    chkZoneLeft_->setChecked(opts_.zoneShowLeft);
    cbTitleStyle_->setEnabled(chkTitle_->isChecked());
    cbBomStyle_->setEnabled(chkBom_->isChecked());
    cbAddBar_->setEnabled(chkAddBar_->isChecked());
    cbCodeBar_->setEnabled(chkCodeBar_->isChecked());
    cbParamBar_->setEnabled(chkParamBar_->isChecked());
    btnPrevTitle_->setEnabled(chkTitle_->isChecked());
    btnPrevBom_->setEnabled(chkBom_->isChecked());
    btnPrevAddBar_->setEnabled(chkAddBar_->isChecked());
    btnPrevCodeBar_->setEnabled(chkCodeBar_->isChecked());
    btnPrevParamBar_->setEnabled(chkParamBar_->isChecked());

    // 比例调整设置
    chkDimAdj_->setChecked(opts_.dimScaleAdjust);
    chkTextAdj_->setChecked(opts_.textScaleAdjust);
    chkTextInitH_->setChecked(opts_.textInitHeightEnabled);
    sbTextInitH_->setValue(opts_.textInitHeight);
    sbTextInitH_->setEnabled(opts_.textInitHeightEnabled);
    chkLwAdj_->setChecked(opts_.lineWidthScaleAdjust);
    chkLwInit_->setChecked(opts_.lineWidthInitEnabled);
    sbLwInit_->setValue(opts_.lineWidthInitRatio);
    sbLwInit_->setEnabled(opts_.lineWidthInitEnabled);
}

void SheetFrameDialog::applyTemplate(const SheetFrameTemplate& t) {
    updating_ = true;
    opts_ = t.opts;
    scaleNum_ = t.scaleNum > 0.0 ? t.scaleNum : 1.0;
    scaleDen_ = t.scaleDen > 0.0 ? t.scaleDen : 1.0;
    loadOptsToUi();
    updating_ = false;
    validateZones();
    updatePreview();
}

// ---- 存为模板（需求 13）：当前全部设置保存为命名模板 ----
void SheetFrameDialog::saveCurrentAsTemplate() {
    QString name = cbTemplate_->currentText().trimmed();
    bool ok = false;
    name = QInputDialog::getText(this, QStringLiteral("存为模板"),
                                 QStringLiteral("模板名称:"), QLineEdit::Normal, name, &ok)
               .trimmed();
    if (!ok || name.isEmpty()) return;
    SheetFrameTemplate t;
    t.name = name;
    t.opts = opts_;
    t.scaleNum = scaleNum_;
    t.scaleDen = scaleDen_;
    bool replaced = false;
    int foundIdx = -1;
    for (int i = 0; i < (int)templates_.size(); ++i)
        if (templates_[i].name == name) { templates_[i] = t; replaced = true; foundIdx = i; break; }
    if (!replaced) { templates_.push_back(t); foundIdx = (int)templates_.size() - 1; }
    saveFrameTemplates(templates_);
    // 刷新下拉并选中新模板
    cbTemplate_->blockSignals(true);
    cbTemplate_->clear();
    for (const auto& e : templates_) cbTemplate_->addItem(e.name);
    cbTemplate_->setCurrentIndex(foundIdx);
    cbTemplate_->blockSignals(false);
}

// ---- 管理模板（需求 13）：列表 + 删除/重命名 ----
void SheetFrameDialog::manageTemplates() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("管理模板"));
    dlg.resize(320, 380);
    auto* lay = new QVBoxLayout(&dlg);
    auto* list = new QListWidget(&dlg);
    // UserRole 存原始名（身份）：重命名只改显示文本，回写时按身份匹配模板数据
    for (const auto& t : templates_) {
        auto* it = new QListWidgetItem(t.name, list);
        it->setData(Qt::UserRole, t.name);
    }
    lay->addWidget(list);
    auto* btnRow = new QHBoxLayout();
    auto* btnDel = new QPushButton(QStringLiteral("删除"), &dlg);
    auto* btnRen = new QPushButton(QStringLiteral("重命名"), &dlg);
    auto* btnClose = new QPushButton(QStringLiteral("关闭"), &dlg);
    btnRow->addWidget(btnDel); btnRow->addWidget(btnRen); btnRow->addStretch(1);
    btnRow->addWidget(btnClose);
    lay->addLayout(btnRow);
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(btnDel, &QPushButton::clicked, this, [list]() {
        const int row = list->currentRow();
        if (row >= 0) delete list->takeItem(row);
    });
    connect(btnRen, &QPushButton::clicked, this, [list]() {
        const int row = list->currentRow();
        if (row < 0) return;
        bool ok = false;
        const QString name = QInputDialog::getText(list, QStringLiteral("重命名模板"),
                                                   QStringLiteral("新名称:"), QLineEdit::Normal,
                                                   list->item(row)->text(), &ok).trimmed();
        if (ok && !name.isEmpty()) list->item(row)->setText(name);
    });
    const QString appliedName = cbTemplate_->currentText();
    dlg.exec();

    // 管理结果回写：按列表终态重建 templates_（按 UserRole 原始名匹配模板数据，
    // 重命名同步新名），落盘 + 刷新下拉
    QVector<SheetFrameTemplate> next;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem* it = list->item(i);
        const QString origName = it->data(Qt::UserRole).toString();
        for (const auto& t : templates_)
            if (t.name == origName) {
                SheetFrameTemplate t2 = t;
                t2.name = it->text();               // 重命名同步
                next.append(t2);
                break;
            }
    }
    templates_ = next;
    saveFrameTemplates(templates_);
    cbTemplate_->blockSignals(true);
    cbTemplate_->clear();
    for (const auto& t : templates_) cbTemplate_->addItem(t.name);
    int keep = -1;
    for (int i = 0; i < templates_.size(); ++i)
        if (templates_[i].name == appliedName) { keep = i; break; }
    cbTemplate_->setCurrentIndex(keep >= 0 ? keep : (templates_.isEmpty() ? -1 : 0));
    cbTemplate_->blockSignals(false);
    // 原应用模板被删 → 回落剩余第一个；重命名 → 名称已同步，无需重应用
    if (keep < 0 && !templates_.isEmpty())
        applyTemplate(templates_.first());
}

// ---- 各栏「预览内容」：隔离选项只保留目标栏，独立预览窗展示 ----
void SheetFrameDialog::showBarPreview(int kind) {
    // 框架上下文（同幅面/布置/装订），仅保留目标栏
    SheetFrameOptions o;
    o.sizeMode = opts_.sizeMode;
    o.basicFormat = opts_.basicFormat;
    o.landscape = opts_.landscape;
    o.extBase = opts_.extBase;
    o.extFactor = opts_.extFactor;
    o.customW = opts_.customW;
    o.customH = opts_.customH;
    o.bindingMargin = opts_.bindingMargin;
    o.bindingWidth = opts_.bindingWidth;
    switch (kind) {
    case BarTitle: o.titleBlock = TitleBlockStyle::Standard; break;
    case BarBom:   o.bomEnabled = true; o.bomRows = opts_.bomRows; break;
    case BarAdd:   o.additionalBarEnabled = true; o.additionalBarKey = opts_.additionalBarKey; break;
    case BarCode:  o.codeBarEnabled = true; o.codeBarKey = opts_.codeBarKey; break;
    case BarParam: o.paramBarEnabled = true; o.paramBarKey = opts_.paramBarKey; break;
    }
    const auto els = buildSheetFrameWithOptions(o);

    static const char* kNames[] = {"标题栏", "明细栏", "附加栏", "代号栏", "参数栏"};
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("预览 — %1").arg(QString::fromUtf8(kNames[kind])));
    auto* lay = new QVBoxLayout(&dlg);
    auto* lbl = new QLabel(&dlg);
    lbl->setPixmap(renderPreview(els, 520, 400));
    lay->addWidget(lbl);
    auto* btnClose = new QPushButton(QStringLiteral("关闭"), &dlg);
    lay->addWidget(btnClose);
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void SheetFrameDialog::validateZones() {
    if (!opts_.zonesEnabled) { lblZone_->setText(QStringLiteral("分区默认关闭")); return; }
    const double W = opts_.widthMm(), H = opts_.heightMm();
    const SheetFormat ff = opts_.frameMarginFormat();
    const bool ten = (ff==SheetFormat::A0||ff==SheetFormat::A1||ff==SheetFormat::A2);
    const double c = ten ? 10.0 : 5.0, e = ten ? 20.0 : 10.0;
    const double leftM = opts_.bindingMargin ? std::clamp(opts_.bindingWidth, 5.0, 100.0) : e;
    const double otherM = opts_.bindingMargin ? c : e;
    const double innerW = W - leftM - otherM, innerH = H - 2 * otherM;
    const double lenX = innerW / std::max(1, opts_.zoneCountX);
    const double lenY = innerH / std::max(1, opts_.zoneCountY);
    const bool even = (opts_.zoneCountX % 2 == 0) && (opts_.zoneCountY % 2 == 0);
    const bool inRange = (lenX >= 25.0 && lenX <= 75.0 && lenY >= 25.0 && lenY <= 75.0);
    QString msg = QStringLiteral("长度每区 %1mm，宽度每区 %2mm；")
                      .arg(lenX, 0, 'f', 0).arg(lenY, 0, 'f', 0);
    if (!even) msg += QStringLiteral("✗ 分区数须为偶数；");
    if (!inRange) msg += QStringLiteral("✗ 每分区须 25~75mm；");
    if (even && inRange) msg += QStringLiteral("✓ 符合 GB/T 14689");
    lblZone_->setText(msg);
}

void SheetFrameDialog::syncOptionsFromUi() {
    validateZones();
    updatePreview();
}

void SheetFrameDialog::updatePreview() {
    const auto els = buildSheetFrameWithOptions(opts_);
    previewLabel_->setPixmap(renderPreview(els, previewLabel_->width() > 0 ? previewLabel_->width() : 380,
                                           previewLabel_->height() > 0 ? previewLabel_->height() : 300));
}

} // namespace SketchCAD
