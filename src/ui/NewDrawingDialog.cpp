#include "NewDrawingDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>

namespace SketchCAD {

// 解析比例文本：支持 "a:b"（如 "1:2"、"2:1"）与纯小数（如 "0.5"）。
// 返回 ratio = 图形尺寸 / 实际尺寸（1:2 -> 0.5，2:1 -> 2.0）。解析失败 ok=false。
static double parseScaleRatio(const QString& s, bool& ok) {
    ok = false;
    QString t = s.trimmed();
    if (t.isEmpty()) return 1.0;
    int idx = t.indexOf(':');
    bool dOk = false;
    if (idx >= 0) {
        double a = t.left(idx).toDouble(&dOk);
        if (!dOk) return 1.0;
        double b = t.mid(idx + 1).toDouble(&dOk);
        if (!dOk || b == 0.0) return 1.0;
        ok = true;
        return a / b;
    }
    double v = t.toDouble(&dOk);
    if (dOk && v > 0.0) { ok = true; return v; }
    return 1.0;
}

NewDrawingDialog::NewDrawingDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("新建图纸 — 选择幅面"));
    resize(360, 320);

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

    // ---- 投影法（GB/T 14665-2012 第7章 图形符号）----
    auto* gbProj = new QGroupBox(QStringLiteral("投影法 (GB/T 14665-2012 第7章)"), this);
    auto* projLay = new QHBoxLayout(gbProj);
    auto* grpProj = new QButtonGroup(this);
    auto* rbFirst = new QRadioButton(QStringLiteral("第一角画法（国标）"), gbProj);
    auto* rbThird = new QRadioButton(QStringLiteral("第三角画法（ISO）"), gbProj);
    grpProj->addButton(rbFirst, 0);
    grpProj->addButton(rbThird, 1);
    rbFirst->setChecked(true);   // 国标默认第一角
    projLay->addWidget(rbFirst);
    projLay->addWidget(rbThird);
    connect(rbFirst, &QRadioButton::toggled, this, [this](bool on) { if (on) projection_ = ProjectionMethod::FirstAngle; });
    connect(rbThird, &QRadioButton::toggled, this, [this](bool on) { if (on) projection_ = ProjectionMethod::ThirdAngle; });
    root->addWidget(gbProj);

    root->addWidget(new QLabel(QStringLiteral("新建后自动生成对应图框与标准标题栏，可随后在「填写标题栏」中录入。"), this));

    // ---- 绘图比例（GB/T 14690）----
    auto* gbScale = new QGroupBox(QStringLiteral("绘图比例 (GB/T 14690)"), this);
    auto* scaleLay = new QHBoxLayout(gbScale);
    auto* cbScale = new QComboBox(gbScale);
    static const QPair<QString, double> kScales[] = {
        {QStringLiteral("1:1"),   1.0},
        {QStringLiteral("1:2"),   0.5},
        {QStringLiteral("1:5"),   0.2},
        {QStringLiteral("1:10"),  0.1},
        {QStringLiteral("1:20"),  0.05},
        {QStringLiteral("1:50"),  0.02},
        {QStringLiteral("1:100"), 0.01},
        // 以下为 GB/T 14690 允许系列（必要时采用），原列表缺失，已于 T16 补齐
        {QStringLiteral("1:2.5"), 0.4},
        {QStringLiteral("1:3"),   1.0 / 3.0},
        {QStringLiteral("1:4"),   0.25},
        {QStringLiteral("1:6"),   1.0 / 6.0},
        {QStringLiteral("2.5:1"), 2.5},
        {QStringLiteral("4:1"),   4.0},
        {QStringLiteral("2:1"),   2.0},
        {QStringLiteral("5:1"),   5.0},
        {QStringLiteral("10:1"),  10.0},
        {QStringLiteral("自定义..."), -1.0},
    };
    for (const auto& s : kScales) cbScale->addItem(s.first, s.second);
    cbScale->setCurrentText(QStringLiteral("1:1"));   // 默认 1:1
    scaleLay->addWidget(cbScale);
    connect(cbScale, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, cbScale](int i) {
                double v = cbScale->itemData(i).toDouble();
                if (v < 0.0) {   // 自定义
                    bool ok = false;
                    QString txt = QInputDialog::getText(this, QStringLiteral("自定义比例"),
                        QStringLiteral("输入比例（图形:实际），如 1:2 或 2:1，也可填小数 0.5"),
                        QLineEdit::Normal, QStringLiteral("1:1"));
                double r = parseScaleRatio(txt, ok);
                if (ok && r > 0.0) {
                    scaleRatio_ = r;
                    // GB/T 14690：非标准比例允许使用，但应提示用户（T16）
                    if (!isStandardDrawingScale(r)) {
                        QMessageBox::information(this, QStringLiteral("比例提示"),
                            QStringLiteral("“%1” 不在 GB/T 14690 标准比例系列内，仍可绘制，但建议优先采用标准比例。").arg(txt));
                    }
                } else {
                    cbScale->setCurrentText(QStringLiteral("1:1"));
                }
                } else {
                    scaleRatio_ = v;
                }
            });
    root->addWidget(gbScale);

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
