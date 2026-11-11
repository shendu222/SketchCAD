#include "ui/WeldSymbolDialog.h"
#include "core/weld/WeldSymbol.h"       // buildWeldSymbol / WeldSpec / weldCommandString / weldTypeName
#include "core/SketchModel.h"           // 放置落盘（beginEdit / addElementsQuiet / log）
#include "core/CommandInterpreter.h"    // CommandInterpreter::execute（确认时调用 WELD 命令）

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QPalette>
#include <cmath>
#include <algorithm>
#include <vector>

namespace SketchCAD {

// ===================== 实时预览控件（直接绘制既有图元组合） =====================
// 说明：WeldPreview 不需要信号槽，仅重写 paintEvent，故不加 Q_OBJECT 以避免 moc 负担。
class WeldPreview : public QWidget {
public:
    explicit WeldPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(340, 220);
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
    }
    void setSpec(const WeldSpec& s) { spec_ = s; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), palette().color(QPalette::Base));

        const std::vector<Element> els = buildWeldSymbol(spec_);
        if (els.empty()) return;

        // ---- 计算包围盒（含圆与文本占位）----
        double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
        auto acc = [&](double x, double y) {
            minx = std::min(minx, x); maxx = std::max(maxx, x);
            miny = std::min(miny, y); maxy = std::max(maxy, y);
        };
        for (const auto& e : els) {
            for (const auto& v : e.pts) acc(v.x, v.y);
            if (e.type == ElementType::Circle && !e.pts.empty()) {
                acc(e.pts[0].x - e.radius, e.pts[0].y - e.radius);
                acc(e.pts[0].x + e.radius, e.pts[0].y + e.radius);
            }
        }
        const double pad = 10.0;
        const double w = std::max(1e-3, maxx - minx);
        const double h = std::max(1e-3, maxy - miny);
        const double scale = std::min((width() - 2 * pad) / w, (height() - 2 * pad) / h);
        auto tx = [&](double x) { return pad + (x - minx) * scale; };
        auto ty = [&](double y) { return height() - (pad + (y - miny) * scale); }; // 翻转 Y（屏幕向下）

        for (const auto& e : els) {
            QPen pen(Qt::black);
            pen.setWidthF(e.lineWidthAuto ? 1.5 : std::max(1.0, e.lineWidth * scale / 4.0));
            p.setPen(pen);
            switch (e.type) {
                case ElementType::Line:
                    if (e.pts.size() >= 2)
                        p.drawLine(QLineF(tx(e.pts[0].x), ty(e.pts[0].y),
                                         tx(e.pts[1].x), ty(e.pts[1].y)));
                    break;
                case ElementType::Polyline: {
                    QPolygonF poly;
                    for (const auto& v : e.pts) poly.append(QPointF(tx(v.x), ty(v.y)));
                    if (e.closed && poly.size() > 1) poly.append(poly.first());
                    p.drawPolyline(poly);
                    break;
                }
                case ElementType::Circle:
                    if (!e.pts.empty())
                        p.drawEllipse(QPointF(tx(e.pts[0].x), ty(e.pts[0].y)),
                                      e.radius * scale, e.radius * scale);
                    break;
                case ElementType::Text:
                    if (!e.pts.empty()) {
                        QPen tpen(Qt::black); tpen.setWidthF(1.0); p.setPen(tpen);
                        QFont f = p.font();
                        f.setPixelSize(std::max(9.0, e.radius * scale));
                        p.setFont(f);
                        p.drawText(QPointF(tx(e.pts[0].x), ty(e.pts[0].y)), e.text);
                    }
                    break;
                default:
                    break;
            }
        }
    }

private:
    WeldSpec spec_;
};

// ================================ 对话框主体 ================================
WeldSymbolDialog::WeldSymbolDialog(SketchModel* model, CommandInterpreter* interp, QWidget* parent)
    : QDialog(parent), model_(model), interp_(interp) {
    setWindowTitle(QStringLiteral("焊缝符号（GB/T 324）"));
    resize(620, 360);

    auto* root = new QHBoxLayout(this);

    // -------------------- 左侧：参数表单 --------------------
    auto* left = new QVBoxLayout();
    auto* form = new QFormLayout();

    cbType_ = new QComboBox(this);
    cbType_->addItems(weldTypeNames());                 // 顺序对应 WeldType 枚举
    cbAux_  = new QComboBox(this);
    cbAux_->addItems({ QStringLiteral("无"), QStringLiteral("平面"),
                       QStringLiteral("凸面"), QStringLiteral("凹面") });
    cbSupp_ = new QComboBox(this);
    cbSupp_->addItems({ QStringLiteral("无"), QStringLiteral("周围焊"),
                        QStringLiteral("现场焊"), QStringLiteral("熔透焊") });
    cbSide_ = new QComboBox(this);
    cbSide_->addItems({ QStringLiteral("箭头侧（基准线下方）"),
                        QStringLiteral("非箭头侧（基准线上方）"),
                        QStringLiteral("双面焊（基准线上）") });

    form->addRow(QStringLiteral("基本符号："), cbType_);
    form->addRow(QStringLiteral("辅助符号："), cbAux_);
    form->addRow(QStringLiteral("补充符号："), cbSupp_);
    form->addRow(QStringLiteral("箭头侧："),   cbSide_);
    left->addLayout(form);

    auto* dimBox = new QGroupBox(QStringLiteral("尺寸（mm）"), this);
    auto* dimForm = new QFormLayout(dimBox);
    leK_ = new QLineEdit(this); leK_->setPlaceholderText(QStringLiteral("焊脚 k"));
    leS_ = new QLineEdit(this); leS_->setPlaceholderText(QStringLiteral("坡口深 s"));
    leL_ = new QLineEdit(this); leL_->setPlaceholderText(QStringLiteral("长度 l"));
    leE_ = new QLineEdit(this); leE_->setPlaceholderText(QStringLiteral("间距 e"));
    leN_ = new QLineEdit(this); leN_->setPlaceholderText(QStringLiteral("段数 n"));
    dimForm->addRow(QStringLiteral("k："), leK_);
    dimForm->addRow(QStringLiteral("s："), leS_);
    dimForm->addRow(QStringLiteral("l："), leL_);
    dimForm->addRow(QStringLiteral("e："), leE_);
    dimForm->addRow(QStringLiteral("n："), leN_);
    left->addWidget(dimBox);
    left->addStretch(1);
    root->addLayout(left, 1);

    // -------------------- 右侧：实时预览 --------------------
    auto* right = new QVBoxLayout();
    right->addWidget(new QLabel(QStringLiteral("实时预览（线宽 = 2d/3，符合 GB/T 12212）："), this));
    preview_ = new WeldPreview(this);
    right->addWidget(preview_, 1);
    root->addLayout(right, 1);

    // -------------------- 底部按钮 --------------------
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("放置到图纸"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &WeldSymbolDialog::onAccepted);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout();
    // 用嵌套布局把左右与按钮组合（root 已是横向；外层纵向）
    auto* wrap = new QWidget(this);
    wrap->setLayout(root);
    outer->addWidget(wrap);
    outer->addWidget(box);
    setLayout(outer);

    // ---- 信号：任意改动即时刷新预览 ----
    connect(cbType_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WeldSymbolDialog::onTypeChanged);
    connect(cbAux_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WeldSymbolDialog::onAuxChanged);
    connect(cbSupp_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WeldSymbolDialog::onSuppChanged);
    connect(cbSide_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WeldSymbolDialog::onSideChanged);
    connect(leK_, &QLineEdit::textChanged, this, &WeldSymbolDialog::onParamChanged);
    connect(leS_, &QLineEdit::textChanged, this, &WeldSymbolDialog::onParamChanged);
    connect(leL_, &QLineEdit::textChanged, this, &WeldSymbolDialog::onParamChanged);
    connect(leE_, &QLineEdit::textChanged, this, &WeldSymbolDialog::onParamChanged);
    connect(leN_, &QLineEdit::textChanged, this, &WeldSymbolDialog::onParamChanged);

    // 初值：角焊缝 + 焊脚 5，立即出预览
    cbType_->setCurrentIndex(static_cast<int>(WeldType::Fillet));
    leK_->setText(QStringLiteral("5"));
    rebuildPreview();
}

WeldSpec WeldSymbolDialog::currentSpec() const {
    WeldSpec s;
    s.type = static_cast<WeldType>(cbType_->currentIndex());
    s.aux  = static_cast<WeldAux>(cbAux_->currentIndex());
    s.supp = static_cast<WeldSupp>(cbSupp_->currentIndex());
    s.side = static_cast<WeldSide>(cbSide_->currentIndex());
    s.dims.k = leK_->text().toDouble();
    s.dims.s = leS_->text().toDouble();
    s.dims.l = leL_->text().toDouble();
    s.dims.e = leE_->text().toDouble();
    s.dims.n = leN_->text().toInt();
    s.d = 5.0;                       // 基本符号尺寸（mm）
    s.anchor = Vec3(0, 0, 0);        // 对话框预览/放置的局部原点由命令落盘时确定
    s.rotationDeg = 0.0;
    s.layer = QStringLiteral("标注");
    return s;
}

void WeldSymbolDialog::rebuildPreview() {
    if (preview_) preview_->setSpec(currentSpec());
}

void WeldSymbolDialog::onTypeChanged(int)  { rebuildPreview(); }
void WeldSymbolDialog::onAuxChanged(int)   { rebuildPreview(); }
void WeldSymbolDialog::onSuppChanged(int)  { rebuildPreview(); }
void WeldSymbolDialog::onSideChanged(int)  { rebuildPreview(); }
void WeldSymbolDialog::onParamChanged(const QString&) { rebuildPreview(); }

void WeldSymbolDialog::onAccepted() {
    if (!model_) { accept(); return; }
    const WeldSpec spec = currentSpec();

    // 交互放置：把 WeldSpec 暂存到模型，激活 weld 工具进入「ghost 实时跟随 + 对象捕捉 +
    // 单击落盘」流程。落盘逻辑集中在 WeldTool（beginEdit + addElementsQuiet，带撤销），
    // 不再经 CommandInterpreter 立即落盘（WELD 命令仍保留为脚本/参数化入口）。
    model_->setPendingWeldSpec(spec);
    model_->setActiveTool(QStringLiteral("weld"));
    accept();
}

} // namespace SketchCAD
