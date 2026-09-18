#include "PropertyPanel.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Geometry.h"
#include "core/LineStyleConfig.h"
#include "ui/icons.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QGroupBox>
#include <QSignalBlocker>

namespace SketchCAD {

// 属性面板里的预设色板（与工具条同一套配色，避免两处色号不一致）
static const QVector<QColor>& kPresets() {
    static const QVector<QColor> v = {
        QColor(0x1A, 0x1D, 0x28), QColor(0xD1, 0x4B, 0x3F), QColor(0x2B, 0x59, 0xA8),
        QColor(0x2E, 0x8B, 0x57), QColor(0xE6, 0x91, 0x38), QColor(0x7E, 0x57, 0xC2),
        QColor(0x00, 0x96, 0xA0), QColor(0x80, 0x86, 0x90),
    };
    return v;
}

PropertyPanel::PropertyPanel(SketchModel* m, QWidget* p)
    : QWidget(p), model_(m) {
    form_ = new QFormLayout(this);
    lId = new QLabel("-"); lType = new QLabel("-");
    eName = new QLineEdit(); eLayer = new QLineEdit("0");
    eText = new QLineEdit();
    sDimValue = new QDoubleSpinBox();
    sDimValue->setRange(0.0, 1e7);
    sDimValue->setDecimals(3);
    sDimValue->setSuffix(QStringLiteral(" mm"));

    // ---- 公差字段 + 单标注标准覆盖 ----
    sTolUp = new QDoubleSpinBox();
    sTolLo = new QDoubleSpinBox();
    for (auto* s : {sTolUp, sTolLo}) {
        s->setRange(-1e4, 1e4);
        s->setDecimals(3);
        s->setSuffix(QStringLiteral(" mm"));
    }
    sTolUp->setPrefix(QStringLiteral("+"));
    sTolLo->setPrefix(QStringLiteral("-"));
    eFit = new QLineEdit();
    eFit->setPlaceholderText(QStringLiteral("如 H7 / g6"));
    cDimStd = new QComboBox();
    cDimStd->addItem(QStringLiteral("GB/T 4458（国标）"), (int)DimensionStandard::GBT);
    cDimStd->addItem(QStringLiteral("ISO 129（国际）"),  (int)DimensionStandard::ISO);

    sX = new QDoubleSpinBox(); sY  = new QDoubleSpinBox();
    sX2 = new QDoubleSpinBox(); sY2 = new QDoubleSpinBox();
    sLen = new QDoubleSpinBox(); sAng = new QDoubleSpinBox();
    sR = new QDoubleSpinBox();
    for (auto* s : {sX, sY, sX2, sY2, sR}) {
        s->setRange(-1e6, 1e6); s->setDecimals(3); s->setSuffix(" mm");
    }
    // 长度不允许为负：负长度在几何上没有意义，方向由夹角表达。
    sLen->setRange(0.0, 1e7); sLen->setDecimals(3); sLen->setSuffix(" mm");
    // 夹角用 [0,360) 而不是 (-180,180]，是因为工程图上报角度习惯从 X 轴正向逆时针计数；
    // wrapping 打开后，在 359.9 上再按一下上箭头会回到 0，符合圆周语义。
    sAng->setRange(0.0, 360.0); sAng->setDecimals(3); sAng->setSuffix(QStringLiteral(" °"));
    sAng->setWrapping(true);

    // ---- 多边形专属属性 ----
    sPolySides = new QSpinBox();
    sPolySides->setRange(3, 100);
    sPolySides->setSuffix(QStringLiteral(" 边"));

    sPolySideLen = new QDoubleSpinBox();
    sPolyInRadius = new QDoubleSpinBox();
    sPolyCircRadius = new QDoubleSpinBox();
    for (auto* s : {sPolySideLen, sPolyInRadius, sPolyCircRadius}) {
        s->setRange(0.0, 1e7); s->setDecimals(3); s->setSuffix(" mm");
    }

    // ---- 线宽 ----
    cWidth = new QComboBox();
    cWidth->setIconSize(QSize(24, 16));
    for (double px : {1.0, 2.0, 3.0, 5.0})
        cWidth->addItem(icons::widthBar(px), QString::fromUtf8(lineWidthName(px)), px);
    // 恢复默认线宽：清除手动覆盖，线宽改由线型派生（见 LineStyleConfig）
    bWidthAuto = new QPushButton(QStringLiteral("恢复默认"));
    bWidthAuto->setToolTip(QStringLiteral("清除手动线宽，恢复为当前线型对应的默认档位"));

    // ---- 线型 ----
    cLineStyle = new QComboBox();
    cLineStyle->setIconSize(QSize(48, 16));
    for (int i = 0; i < kLineStyleCount; ++i) {
        LineStyle ls = (LineStyle)i;
        cLineStyle->addItem(icons::lineStyleIcon(ls),
                           QString::fromUtf8(lineStyleName(ls)), i);
    }

    // ---- 颜色：一行预设色块 + 自定义按钮 ----
    bColor = new QPushButton(QStringLiteral("自定义..."));
    bColor->setAutoFillBackground(true);

    presetRow = new QWidget();
    auto* prl = new QHBoxLayout(presetRow);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(2);
    for (const QColor& c : kPresets()) {
        auto* b = new QToolButton(presetRow);
        b->setIcon(icons::swatch(c));
        b->setIconSize(QSize(18, 18));
        b->setAutoRaise(true);
        b->setToolTip(c.name().toUpper());
        connect(b, &QToolButton::clicked, this, [this, c] { setColor(c); apply(); });
        prl->addWidget(b);
    }
    prl->addStretch();

    labX = new QLabel(QStringLiteral("起点 X"));
    labY = new QLabel(QStringLiteral("起点 Y"));

    form_->addRow(QStringLiteral("编号"), lId);
    form_->addRow(QStringLiteral("类型"), lType);
    form_->addRow(QStringLiteral("名称"), eName);
    form_->addRow(QStringLiteral("图层"), eLayer);
    form_->addRow(labX, sX);
    form_->addRow(labY, sY);
    form_->addRow(QStringLiteral("终点 X"), sX2);
    form_->addRow(QStringLiteral("终点 Y"), sY2);
    form_->addRow(QStringLiteral("长度"), sLen);
    form_->addRow(QStringLiteral("与X轴夹角"), sAng);
    form_->addRow(QStringLiteral("半径"), sR);
    form_->addRow(QStringLiteral("边数"), sPolySides);
    form_->addRow(QStringLiteral("边长"), sPolySideLen);
    form_->addRow(QStringLiteral("内接圆半径"), sPolyInRadius);
    form_->addRow(QStringLiteral("外接圆半径"), sPolyCircRadius);
    form_->addRow(QStringLiteral("标注文本"), eText);
    form_->addRow(QStringLiteral("尺寸值"), sDimValue);
    // 公差表达规则（需用户手动填值；具体注写形式随"单标注标准"切换：GB/T 默认无、ISO 默认极限偏差）
    form_->addRow(QStringLiteral("上偏差 +"), sTolUp);
    form_->addRow(QStringLiteral("下偏差 -"), sTolLo);
    form_->addRow(QStringLiteral("配合代号"), eFit);
    form_->addRow(QStringLiteral("标注标准"), cDimStd);
    // 线宽：下拉框 + “恢复默认”按钮（清除手动覆盖，回到线型派生档位）
    auto* wRow = new QWidget();
    auto* wLay = new QHBoxLayout(wRow);
    wLay->setContentsMargins(0, 0, 0, 0); wLay->setSpacing(6);
    wLay->addWidget(cWidth); wLay->addWidget(bWidthAuto);
    form_->addRow(QStringLiteral("线宽"), wRow);
    form_->addRow(QStringLiteral("线型"), cLineStyle);
    form_->addRow(QStringLiteral("常用色"), presetRow);
    form_->addRow(QStringLiteral("颜色"), bColor);

    connect(eName, &QLineEdit::editingFinished, this, &PropertyPanel::apply);
    connect(eLayer, &QLineEdit::editingFinished, this, &PropertyPanel::apply);
    connect(eText, &QLineEdit::editingFinished, this, &PropertyPanel::apply);
    // 数值输入框使用 editingFinished（回车或失焦时触发），而非 valueChanged（每输入一位数字就触发），
    // 避免用户输入"120"时先以"1"再以"12"修改图元。
    connect(sDimValue, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyDimValue);
    connect(sTolUp, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyTolerance);
    connect(sTolLo, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyTolerance);
    connect(eFit, &QLineEdit::editingFinished, this, &PropertyPanel::applyTolerance);
    connect(cDimStd, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::applyTolerance);
    connect(sR, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::apply);
    for (auto* s : {sX, sY, sX2, sY2})
        connect(s, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyCartesian);
    for (auto* s : {sLen, sAng})
        connect(s, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyPolar);
    // 多边形属性：边数、边长、内接圆半径、外接圆半径（回车或失焦时触发）
    connect(sPolySides, &QSpinBox::editingFinished, this, &PropertyPanel::applyPoly);
    for (auto* s : {sPolySideLen, sPolyInRadius, sPolyCircRadius})
        connect(s, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyPoly);
    connect(cWidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::applyWidth);
    connect(cLineStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::applyStyle);
    connect(bWidthAuto, &QPushButton::clicked, this, &PropertyPanel::restoreDefaultWidth);
    connect(bColor, &QPushButton::clicked, this, [this] {
        QColor c = QColorDialog::getColor(curColor_, this, QStringLiteral("选择图元颜色"));
        if (c.isValid()) { setColor(c); apply(); }
    });

    clearFields();
}

void PropertyPanel::onDefaultStyleChanged(const QColor&, double) {
    if (curId_ < 0) clearFields();   // 未选中图元时，回显新的默认值
}

void PropertyPanel::onDefaultLineStyleChanged(int) {
    if (curId_ < 0) clearFields();
}

void PropertyPanel::setColor(const QColor& c) {
    curColor_ = c;
    // 底色深时用白字，否则按钮上的"自定义..."会看不清
    const QString fg = (c.lightness() < 128) ? QStringLiteral("#FFFFFF") : QStringLiteral("#1A1D28");
    bColor->setStyleSheet(QStringLiteral("background-color:%1; color:%2").arg(c.name(), fg));
}

// QFormLayout 的整行显隐：连标签一起收掉，否则会留下一排孤儿标签
void PropertyPanel::showRow(QWidget* field, bool on) {
    if (!field) return;
    field->setVisible(on);
    if (QWidget* lb = form_->labelForField(field)) lb->setVisible(on);
}

void PropertyPanel::clearFields() {
    updating_ = true;
    lId->setText("-"); lType->setText("-");
    eName->clear(); eLayer->setText("0"); eText->clear();
    sDimValue->setValue(0);
    sTolUp->setValue(0); sTolLo->setValue(0); eFit->clear();
    cDimStd->setCurrentIndex(0);
    for (auto* s : {sX, sY, sX2, sY2, sLen, sAng, sR}) s->setValue(0);
    sPolySides->setValue(3);
    for (auto* s : {sPolySideLen, sPolyInRadius, sPolyCircRadius}) s->setValue(0);
    labX->setText(QStringLiteral("起点 X"));
    labY->setText(QStringLiteral("起点 Y"));
    for (QWidget* w : {(QWidget*)sX2, (QWidget*)sY2, (QWidget*)sLen,
                       (QWidget*)sAng, (QWidget*)sR, (QWidget*)eText, (QWidget*)sDimValue,
                       (QWidget*)sTolUp, (QWidget*)sTolLo, (QWidget*)eFit, (QWidget*)cDimStd,
                       (QWidget*)sPolySides, (QWidget*)sPolySideLen,
                       (QWidget*)sPolyInRadius, (QWidget*)sPolyCircRadius})
        showRow(w, false);
    cWidth->setCurrentIndex(0);
    cLineStyle->setCurrentIndex((int)model_->defaultLineStyle());
    bWidthAuto->setEnabled(false);   // 无选中图元时“恢复默认”无意义
    // 无选中时展示模型的"新图元默认样式"，面板不会是一片空白，
    // 也让用户能直观看到「接下来画出来会是什么样」。
    setColor(model_->defaultColor());
    int di = cWidth->findData(model_->defaultWidth());
    if (di >= 0) cWidth->setCurrentIndex(di);
    setEnabledFields(false);
    updating_ = false;
}

// 无选中图元时，几何相关的输入框禁用，避免用户在空面板上做无效输入
void PropertyPanel::setEnabledFields(bool on) {
    for (QWidget* w : {(QWidget*)eName, (QWidget*)eLayer, (QWidget*)eText,
                       (QWidget*)sX, (QWidget*)sY, (QWidget*)sX2, (QWidget*)sY2,
                       (QWidget*)sLen, (QWidget*)sAng, (QWidget*)sR, (QWidget*)sDimValue,
                       (QWidget*)sTolUp, (QWidget*)sTolLo, (QWidget*)eFit, (QWidget*)cDimStd,
                       (QWidget*)sPolySides, (QWidget*)sPolySideLen,
                       (QWidget*)sPolyInRadius, (QWidget*)sPolyCircRadius})
        w->setEnabled(on);
}

void PropertyPanel::load(int id) {
    const Element* e = model_->element(id);
    if (!e) { clearFields(); curId_ = -1; return; }
    updating_ = true;
    curId_ = id;
    setColor(e->color);
    lId->setText("#" + QString::number(e->id));
    lType->setText(e->typeName());
    eName->setText(e->name);
    eLayer->setText(e->layer);

    const bool isLine = (e->type == ElementType::Line);
    const bool hasR   = (e->type == ElementType::Circle || e->type == ElementType::Arc);
    const bool isDim  = (e->type == ElementType::Dimension);
    const bool isText = (e->type == ElementType::Text);
    const bool isPoly = (e->type == ElementType::Polygon && e->pts.size() >= 3);

    if (!e->pts.empty()) { sX->setValue(e->pts[0].x); sY->setValue(e->pts[0].y); }
    labX->setText(isLine ? QStringLiteral("起点 X") : QStringLiteral("基点 X"));
    labY->setText(isLine ? QStringLiteral("起点 Y") : QStringLiteral("基点 Y"));

    // 直线：终点 + 长度 + 夹角。长度与夹角是「派生属性」，
    // 既能被读出来，也能被改回去（改完反算终点），这才叫真正的可编辑。
    if (isLine && e->pts.size() >= 2) {
        sX2->setValue(e->pts[1].x); sY2->setValue(e->pts[1].y);
        sLen->setValue(geo::segLength(e->pts[0], e->pts[1]));
        sAng->setValue(geo::segAngleDeg(e->pts[0], e->pts[1]));
    }
    showRow(sX2, isLine); showRow(sY2, isLine);
    showRow(sLen, isLine); showRow(sAng, isLine);

    showRow(sR, hasR); sR->setValue(e->radius);
    showRow(eText, isDim || isText);
    eText->setText(e->text);
    // 文本图元：把"标注文本"行标签临时改为"文本"，更贴切
    if (QLabel* lb = qobject_cast<QLabel*>(form_->labelForField(eText)))
        lb->setText(isText ? QStringLiteral("文本") : QStringLiteral("标注文本"));
    showRow(sDimValue, isDim);
    showRow(sTolUp, isDim);
    showRow(sTolLo, isDim);
    showRow(eFit, isDim);
    showRow(cDimStd, isDim);
    if (isDim) {
        const double v = geo::measureDimension(*e);
        QSignalBlocker b(sDimValue);
        sDimValue->setValue(v);
        // 角度标注用度作后缀，其余用毫米
        sDimValue->setSuffix(e->dtype == DimensionType::Angle
                                 ? QStringLiteral(" °") : QStringLiteral(" mm"));
        // 没有关联源图元(refA)的标注不可驱动几何，只读显示
        sDimValue->setEnabled(e->refA >= 0);

        // 公差字段与单标注标准覆盖
        QSignalBlocker b1(sTolUp), b2(sTolLo), b3(eFit), b4(cDimStd);
        sTolUp->setValue(e->tolUpper);
        sTolLo->setValue(e->tolLower);
        eFit->setText(e->fitCode);
        cDimStd->setCurrentIndex((e->standard == DimensionStandard::ISO) ? 1 : 0);
    }

    // 多边形：边数、边长、内接圆半径、外接圆半径
    // 从顶点序列计算质心、外接圆半径（质心到首顶点距离）、边长（首条边）、
    // 内接圆半径（质心到首条边中点的距离）。
    if (isPoly) {
        const int n = static_cast<int>(e->pts.size());
        sPolySides->setValue(n);

        // 质心
        Vec3 centroid(0, 0, 0);
        for (const auto& p : e->pts) { centroid.x += p.x; centroid.y += p.y; }
        centroid.x /= n; centroid.y /= n;

        // 外接圆半径 = 质心到首顶点的距离
        const double circR = std::hypot(e->pts[0].x - centroid.x, e->pts[0].y - centroid.y);

        // 边长 = 首条边长度
        const double sideLen = e->pts[0].distTo(e->pts[1]);

        // 内接圆半径 = 质心到首条边中点的距离
        const Vec3 edgeMid((e->pts[0].x + e->pts[1].x) * 0.5,
                           (e->pts[0].y + e->pts[1].y) * 0.5);
        const double inR = std::hypot(edgeMid.x - centroid.x, edgeMid.y - centroid.y);

        sPolySideLen->setValue(sideLen);
        sPolyInRadius->setValue(inR);
        sPolyCircRadius->setValue(circR);
    }
    showRow(sPolySides, isPoly);
    showRow(sPolySideLen, isPoly);
    showRow(sPolyInRadius, isPoly);
    showRow(sPolyCircRadius, isPoly);

    // 线宽下拉显示"实际生效线宽"（自动模式即线型默认档位），便于用户一眼看出当前粗细
    int wi = cWidth->findData(e->effectiveLineWidth());
    cWidth->setCurrentIndex(wi >= 0 ? wi : 0);
    cLineStyle->setCurrentIndex((int)e->lineStyle);
    // 已处于默认档位时禁用“恢复默认”，避免无意义操作
    bWidthAuto->setEnabled(!e->lineWidthAuto);
    setEnabledFields(true);
    updating_ = false;
}

void PropertyPanel::refreshDerived() {
    const Element* e = model_->element(curId_);
    if (!e || e->type != ElementType::Line || e->pts.size() < 2) return;
    QSignalBlocker b1(sLen), b2(sAng);
    sLen->setValue(geo::segLength(e->pts[0], e->pts[1]));
    sAng->setValue(geo::segAngleDeg(e->pts[0], e->pts[1]));
}

void PropertyPanel::apply() {
    if (updating_) return;
    if (curId_ < 0) {
        // 没有选中图元时，面板上的线宽/颜色/线型改动作用到"新图元默认样式"，
        // 这样属性面板既能改已有图元，也能当作绘制前的样式设置器。
        model_->setDefaultColor(curColor_);
        model_->setDefaultWidth(cWidth->currentData().toDouble());
        model_->setDefaultLineStyle((LineStyle)cLineStyle->currentIndex());
        return;
    }
    // 名称/图层/颜色/半径/文本。线宽与线型由 applyWidth/applyStyle 专门处理，
    // 以分别满足「手动覆盖(auto=false)」与「自动跟随线型(auto=true)」两种语义。
    model_->updateElementLinked(curId_, [this](Element& e) {
        e.name = eName->text();
        e.layer = eLayer->text();
        e.color = curColor_;
        if (e.type == ElementType::Circle || e.type == ElementType::Arc) e.radius = sR->value();
        if (e.type == ElementType::Dimension || e.type == ElementType::Text) e.text = eText->text();
    });
}

// 改线宽下拉：固定为手动覆盖（lineWidthAuto=false），后续改线型不再自动改回线宽
void PropertyPanel::applyWidth() {
    if (updating_ || curId_ < 0) return;
    const double px = cWidth->currentData().toDouble();
    model_->updateElementLinked(curId_, [px](Element& e) {
        e.lineWidth = px;
        e.lineWidthAuto = false;
    });
}

// 改线型下拉：若仍为自动模式，线宽跟随新线型的默认档位派生；手动覆盖则保持不变
void PropertyPanel::applyStyle() {
    if (updating_ || curId_ < 0) return;
    const LineStyle ls = (LineStyle)cLineStyle->currentIndex();
    model_->updateElementLinked(curId_, [ls](Element& e) {
        e.lineStyle = ls;
        if (e.lineWidthAuto) e.lineWidth = LineStyleConfig::instance().defaultWidth(ls);
    });
    load(curId_);   // 刷新宽度下拉框与“恢复默认”按钮状态
}

// “恢复默认”按钮：清除手动覆盖，线宽改由线型派生
void PropertyPanel::restoreDefaultWidth() {
    if (curId_ < 0) return;
    model_->updateElementLinked(curId_, [](Element& e) { e.lineWidthAuto = true; });
    load(curId_);
}

// 用户直接改了起点/终点坐标：写回坐标，再把长度与夹角刷新成新值
// 使用联动更新：端点移动时自动带动端点重合的其他图元一起移动
void PropertyPanel::applyCartesian() {
    if (updating_ || curId_ < 0) return;
    const Element* cur = model_->element(curId_);
    if (!cur) return;
    const bool isLine = (cur->type == ElementType::Line);
    model_->updateElementLinked(curId_, [this, isLine](Element& e) {
        if (!e.pts.empty()) { e.pts[0].x = sX->value(); e.pts[0].y = sY->value(); }
        if (isLine && e.pts.size() >= 2) { e.pts[1].x = sX2->value(); e.pts[1].y = sY2->value(); }
    });
    refreshDerived();
}

// 用户改了长度或夹角：以质心（中点）为固定基点，两端点对称伸缩/旋转
// 需求：质心基点原则 —— 直线以中点为基点，修改长度时中点位置不变，两端点对称伸缩
void PropertyPanel::applyPolar() {
    if (updating_ || curId_ < 0) return;
    const Element* cur = model_->element(curId_);
    if (!cur || cur->type != ElementType::Line || cur->pts.size() < 2) return;

    // 当前中点（质心），修改过程中保持不变
    const Vec3 mid((cur->pts[0].x + cur->pts[1].x) * 0.5,
                   (cur->pts[0].y + cur->pts[1].y) * 0.5);
    const double newLen = sLen->value();
    const double newAng = sAng->value();

    // 以中点为基点：两端点沿新方向对称分布
    const double halfLen = newLen * 0.5;
    const double rad = newAng * M_PI / 180.0;
    const double dx = std::cos(rad) * halfLen;
    const double dy = std::sin(rad) * halfLen;
    const Vec3 newStart(mid.x - dx, mid.y - dy);
    const Vec3 newEnd(mid.x + dx, mid.y + dy);

    model_->updateElementLinked(curId_, [&](Element& e) {
        e.pts[0] = newStart;
        e.pts[1] = newEnd;
    });
    QSignalBlocker b1(sX2), b2(sY2), b3(sX), b4(sY);
    sX->setValue(newStart.x);  sY->setValue(newStart.y);
    sX2->setValue(newEnd.x);   sY2->setValue(newEnd.y);
}

// 标注尺寸值被编辑 → 驱动源图元几何并联动传播到相连图元
void PropertyPanel::applyDimValue() {
    if (updating_ || curId_ < 0) return;
    const Element* e = model_->element(curId_);
    if (!e || e->type != ElementType::Dimension || e->refA < 0) return;
    const double v = sDimValue->value();
    if (!model_->driveDimension(curId_, v))
        model_->log(QStringLiteral("该标注类型暂不支持驱动几何"), "warn");
}

// 公差字段 / 单标注标准被编辑 → 写回并刷新标注文字（label 由 dim::formatTolerance 重新生成）
void PropertyPanel::applyTolerance() {
    if (updating_ || curId_ < 0) return;
    const Element* e = model_->element(curId_);
    if (!e || e->type != ElementType::Dimension) return;
    const DimensionStandard std = (DimensionStandard)cDimStd->currentData().toInt();
    model_->updateElementLinked(curId_, [this, std](Element& el) {
        el.tolUpper = sTolUp->value();
        el.tolLower = sTolLo->value();
        el.fitCode  = eFit->text().trimmed();
        el.standard = std;   // 单标注标准覆盖（全局切换时会整体重戳）
    });
}

// 多边形属性被编辑 → 以质心为固定基点，重新生成正多边形顶点
// 需求：质心基点原则 —— 多边形以几何质心为基点，修改边数/边长/半径时质心位置不变
void PropertyPanel::applyPoly() {
    if (updating_ || curId_ < 0) return;
    const Element* cur = model_->element(curId_);
    if (!cur || cur->type != ElementType::Polygon || cur->pts.size() < 3) return;

    const int n = sPolySides->value();
    if (n < 3) return;

    // 计算当前质心（修改过程中保持不变的基点）
    Vec3 centroid(0, 0, 0);
    for (const auto& p : cur->pts) { centroid.x += p.x; centroid.y += p.y; }
    centroid.x /= cur->pts.size(); centroid.y /= cur->pts.size();

    // 保留起始角度（质心到首顶点的方向），修改后多边形朝向不变
    const double startAng = std::atan2(cur->pts[0].y - centroid.y,
                                       cur->pts[0].x - centroid.x);

    // 根据 sender 判断哪个属性被修改，反算外接圆半径 R
    double R = 0;
    QObject* sender = this->sender();
    if (sender == sPolyCircRadius) {
        // 外接圆半径直接修改
        R = sPolyCircRadius->value();
    } else if (sender == sPolySideLen) {
        // 边长 → R = s / (2 * sin(π/n))
        const double s = sPolySideLen->value();
        R = s / (2.0 * std::sin(M_PI / n));
    } else if (sender == sPolyInRadius) {
        // 内接圆半径 → R = r / cos(π/n)
        const double r = sPolyInRadius->value();
        R = r / std::cos(M_PI / n);
    } else {
        // 边数改变 或 未知来源：保持原外接圆半径
        R = std::hypot(cur->pts[0].x - centroid.x, cur->pts[0].y - centroid.y);
    }

    if (R < 1e-6) return;

    // 以质心为基点，重新生成正多边形顶点
    const double step = 2.0 * M_PI / n;
    std::vector<Vec3> newPts;
    newPts.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double ang = startAng + i * step;
        newPts.emplace_back(centroid.x + R * std::cos(ang),
                            centroid.y + R * std::sin(ang), 0);
    }

    model_->updateElementLinked(curId_, [&newPts](Element& e) {
        e.pts = std::move(newPts);
    });

    // 刷新派生属性显示（边长、内接圆半径、外接圆半径联动更新）
    refreshPolyDerived();
}

// 刷新多边形派生属性显示（不触发写回）
void PropertyPanel::refreshPolyDerived() {
    const Element* e = model_->element(curId_);
    if (!e || e->type != ElementType::Polygon || e->pts.size() < 3) return;

    const int n = static_cast<int>(e->pts.size());

    // 质心
    Vec3 centroid(0, 0, 0);
    for (const auto& p : e->pts) { centroid.x += p.x; centroid.y += p.y; }
    centroid.x /= n; centroid.y /= n;

    // 外接圆半径
    const double circR = std::hypot(e->pts[0].x - centroid.x, e->pts[0].y - centroid.y);
    // 边长
    const double sideLen = e->pts[0].distTo(e->pts[1]);
    // 内接圆半径
    const Vec3 edgeMid((e->pts[0].x + e->pts[1].x) * 0.5,
                       (e->pts[0].y + e->pts[1].y) * 0.5);
    const double inR = std::hypot(edgeMid.x - centroid.x, edgeMid.y - centroid.y);

    QSignalBlocker b1(sPolySides), b2(sPolySideLen), b3(sPolyInRadius), b4(sPolyCircRadius);
    sPolySides->setValue(n);
    sPolySideLen->setValue(sideLen);
    sPolyInRadius->setValue(inR);
    sPolyCircRadius->setValue(circR);
}

void PropertyPanel::onSelectionChanged(int id) { load(id); }
void PropertyPanel::onModelChanged() {
    if (curId_ >= 0 && model_->element(curId_)) load(curId_);
    else if (curId_ >= 0) { clearFields(); curId_ = -1; }
}

} // namespace SketchCAD
