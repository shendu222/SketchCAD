// ============================================================================
// InquiryDialog —— 测量查询图形界面实现
// 几何计算复用 inquiry:: 核心层；只读、不改模型。
// ============================================================================
#include "ui/InquiryDialog.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <algorithm>

#include "ui/icons.h"
#include "core/Vec.h"
#include "core/Element.h"
#include "core/Inquiry.h"

namespace SketchCAD {

namespace {
// 解析 "x,y"
bool parsePoint(const QString& s, Vec3& out) {
    QString t = s.trimmed();
    int c = t.indexOf(',');
    if (c < 0) return false;
    bool ok1 = false, ok2 = false;
    double x = t.left(c).toDouble(&ok1);
    double y = t.mid(c + 1).toDouble(&ok2);
    if (!ok1 || !ok2) return false;
    out = Vec3(x, y, 0);
    return true;
}

// 从"ID 列表（空格分隔）"或当前选区收集目标图元；空文本 -> 选区
std::vector<Element> collectTargets(SketchModel* m, const QString& idsText) {
    std::vector<Element> out;
    const QString t = idsText.trimmed();
    if (t.isEmpty()) {
        for (int id : m->selection())
            if (const Element* e = m->element(id)) out.push_back(*e);
    } else {
        for (const QString& s : t.split(QRegularExpression("\\s+"))) {
            bool ok = false;
            int id = s.toInt(&ok);
            if (!ok) continue;
            if (const Element* e = m->element(id)) out.push_back(*e);
        }
    }
    return out;
}
} // namespace

InquiryDialog::InquiryDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model)
{
    setWindowTitle(QStringLiteral("测量查询"));
    setWindowIcon(icons::make("query"));
    setMinimumWidth(460);

    auto* root = new QVBoxLayout(this);

    tabs_ = new QTabWidget(this);
    root->addWidget(tabs_);

    // ---------------- 距离页 ----------------
    {
        auto* w = new QWidget(this);
        auto* fl = new QFormLayout(w);
        dX1_ = new QLineEdit("0", w); dY1_ = new QLineEdit("0", w);
        dX2_ = new QLineEdit("10", w); dY2_ = new QLineEdit("0", w);
        dX1_->setObjectName("distX1"); dY1_->setObjectName("distY1");
        dX2_->setObjectName("distX2"); dY2_->setObjectName("distY2");
        fl->addRow(QStringLiteral("点 1  X , Y"), row2(dX1_, dY1_));
        fl->addRow(QStringLiteral("点 2  X , Y"), row2(dX2_, dY2_));
        auto* btn = new QPushButton(icons::make("query"), QStringLiteral("计算距离"), w);
        btn->setToolTip(QStringLiteral("两点距离 + ΔX / ΔY / XY 平面方向角"));
        connect(btn, &QPushButton::clicked, this, &InquiryDialog::doDist);
        fl->addRow(btn);
        tabs_->addTab(w, icons::make("query"), QStringLiteral("距离"));
    }

    // ---------------- 面积页 ----------------
    {
        auto* w = new QWidget(this);
        auto* fl = new QFormLayout(w);
        idList_ = new QLineEdit(w);
        idList_->setPlaceholderText(QStringLiteral("留空 = 当前选区；或填 ID 如 3 5 7"));
        pointList_ = new QLineEdit(w);
        pointList_->setPlaceholderText(QStringLiteral("点列模式：x1,y1 x2,y2 x3,y3 ..."));
        fl->addRow(QStringLiteral("图元 ID"), idList_);
        fl->addRow(QStringLiteral("点列"), pointList_);

        auto* hb = new QHBoxLayout;
        auto* bSel = new QPushButton(QStringLiteral("计算选中/指定图元"), w);
        bSel->setToolTip(QStringLiteral("对选区或 ID 列表中的封闭图元逐个统计面积/周长/形心"));
        connect(bSel, &QPushButton::clicked, this, [this] { doArea(false); });
        auto* bPts = new QPushButton(QStringLiteral("计算点列多边形"), w);
        bPts->setToolTip(QStringLiteral("把点列当作闭合多边形 shoelace 计算"));
        connect(bPts, &QPushButton::clicked, this, [this] {
            clearResult();
            std::vector<Vec3> ring;
            for (const QString& s : pointList_->text().trimmed().split(QRegularExpression("\\s+"))) {
                Vec3 p;
                if (parsePoint(s, p)) ring.push_back(p);
            }
            if (ring.size() < 3) { appendResult(QStringLiteral("面积：点列需至少 3 个点")); return; }
            const auto r = inquiry::areaOfPolygon(ring);
            if (!r.ok) { appendResult("面积：" + r.error); return; }
            appendResult(QString("点列模式（%1 个顶点）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)")
                             .arg(ring.size()).arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                             .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
        });
        hb->addWidget(bSel); hb->addWidget(bPts);
        fl->addRow(hb);
        tabs_->addTab(w, icons::make("query"), QStringLiteral("面积"));
    }

    // ---------------- 质量特性页 ----------------
    {
        auto* w = new QWidget(this);
        auto* fl = new QFormLayout(w);
        auto* id2 = new QLineEdit(w);
        id2->setPlaceholderText(QStringLiteral("留空 = 当前选区；或填 ID 如 3 5 7"));
        fl->addRow(QStringLiteral("图元 ID"), id2);
        auto* btn = new QPushButton(QStringLiteral("计算质量特性"), w);
        btn->setToolTip(QStringLiteral("面积 / 周长 / 包围盒 / 形心 / 形心轴惯性矩"));
        connect(btn, &QPushButton::clicked, this, [this, id2] {
            clearResult();
            auto targets = collectTargets(model_, id2->text());
            if (targets.empty()) { appendResult(QStringLiteral("质量特性：无目标（先选中图元或填入 ID）")); return; }
            for (const auto& e : targets) {
                const auto r = inquiry::massPropOfElement(e);
                if (!r.ok) { appendResult(QString("#%1 %2 —— %3").arg(e.id).arg(e.typeName(), r.error)); continue; }
                appendResult(QString("---- #%1 %2 质量特性 ----").arg(e.id).arg(e.typeName()));
                appendResult(QString("  面积        %1 mm²").arg(r.area, 0, 'f', 4));
                appendResult(QString("  周长        %1 mm").arg(r.perimeter, 0, 'f', 4));
                appendResult(QString("  包围盒      (%1, %2) ~ (%3, %4)    %5 × %6")
                                 .arg(r.bbox.left(), 0, 'f', 2).arg(r.bbox.bottom(), 0, 'f', 2)
                                 .arg(r.bbox.right(), 0, 'f', 2).arg(r.bbox.top(), 0, 'f', 2)
                                 .arg(r.bbox.width(), 0, 'f', 2).arg(r.bbox.height(), 0, 'f', 2));
                appendResult(QString("  形心        (%1, %2)").arg(r.centroid.x, 0, 'f', 3).arg(r.centroid.y, 0, 'f', 3));
                appendResult(QString("  惯性矩(形心) Ix = %1    Iy = %2    Ixy = %3 mm⁴")
                                 .arg(r.ixBar, 0, 'f', 2).arg(r.iyBar, 0, 'f', 2).arg(r.ixyBar, 0, 'f', 2));
            }
        });
        fl->addRow(btn);
        tabs_->addTab(w, icons::make("query"), QStringLiteral("质量特性"));
    }

    // ---------------- 点坐标页 ----------------
    {
        auto* w = new QWidget(this);
        auto* fl = new QFormLayout(w);
        iX_ = new QLineEdit("0", w); iY_ = new QLineEdit("0", w);
        iX_->setObjectName("idX"); iY_->setObjectName("idY");
        fl->addRow(QStringLiteral("查询点  X , Y"), row2(iX_, iY_));
        auto* btn = new QPushButton(icons::make("query"), QStringLiteral("查询坐标 / 最近图元"), w);
        btn->setToolTip(QStringLiteral("点坐标 + 模型内最近图元及距离"));
        connect(btn, &QPushButton::clicked, this, &InquiryDialog::doId);
        fl->addRow(btn);
        tabs_->addTab(w, icons::make("query"), QStringLiteral("点坐标"));
    }

    // ---------------- 结果区 ----------------
    result_ = new QPlainTextEdit(this);
    result_->setReadOnly(true);
    result_->setLineWrapMode(QPlainTextEdit::NoWrap);
    root->addWidget(result_);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

// 两列输入框的水平布局（X / Y）
QHBoxLayout* InquiryDialog::row2(QLineEdit* a, QLineEdit* b) {
    auto* h = new QHBoxLayout;
    h->addWidget(a); h->addWidget(b);
    return h;
}

void InquiryDialog::appendResult(const QString& line) {
    result_->appendPlainText(line);
}

void InquiryDialog::clearResult() {
    result_->clear();
}

void InquiryDialog::doDist() {
    clearResult();
    Vec3 a, b;
    if (!parsePoint(dX1_->text() + "," + dY1_->text(), a) ||
        !parsePoint(dX2_->text() + "," + dY2_->text(), b)) {
        appendResult(QStringLiteral("距离：点解析失败（应为数字）"));
        return;
    }
    const auto r = inquiry::distTwoPoints(a, b);
    appendResult(QString("距离 = %1 mm    ΔX = %2    ΔY = %3    XY平面角 = %4°")
                     .arg(r.distance, 0, 'f', 4).arg(r.dx, 0, 'f', 2)
                     .arg(r.dy, 0, 'f', 2).arg(r.angleDeg, 0, 'f', 2));
}

void InquiryDialog::doArea(bool /*massprop*/) {
    clearResult();
    auto targets = collectTargets(model_, idList_->text());
    if (targets.empty()) {
        appendResult(QStringLiteral("面积：无目标（先选中图元、填入 ID，或使用点列模式）"));
        return;
    }
    // 直线链优先：目标全为直线 → 尝试按"首尾相接"围成封闭多边形求面积
    const bool allLines = targets.size() >= 3 && std::all_of(
        targets.begin(), targets.end(),
        [](const Element& e) { return e.type == ElementType::Line; });
    if (allLines) {
        const auto r = inquiry::areaOfClosedLineChain(targets);
        if (r.ok) {
            appendResult(QString("直线链模式（%1 条边围成多边形）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)")
                             .arg(targets.size()).arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                             .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
            return;
        }
        appendResult("直线链：" + r.error + "；退回逐图元统计：");
    }
    double total = 0; int counted = 0;
    for (const auto& e : targets) {
        const auto r = inquiry::areaOfElement(e);
        if (!r.ok) { appendResult(QString("#%1 %2 —— %3").arg(e.id).arg(e.typeName(), r.error)); continue; }
        ++counted; total += r.area;
        appendResult(QString("#%1 %2：面积 = %3 mm²    周长 = %4 mm    形心 = (%5, %6)")
                         .arg(e.id).arg(e.typeName())
                         .arg(r.area, 0, 'f', 4).arg(r.perimeter, 0, 'f', 4)
                         .arg(r.centroid.x, 0, 'f', 2).arg(r.centroid.y, 0, 'f', 2));
    }
    if (counted >= 2)
        appendResult(QString("合计 %1 个封闭区域：总面积 = %2 mm²").arg(counted).arg(total, 0, 'f', 4));
}

void InquiryDialog::doId() {
    clearResult();
    Vec3 p;
    if (!parsePoint(iX_->text() + "," + iY_->text(), p)) {
        appendResult(QStringLiteral("点坐标：点解析失败（应为数字）"));
        return;
    }
    const auto r = inquiry::idPoint(p, model_->elements());
    appendResult(QString("坐标: X = %1    Y = %2    Z = %3")
                     .arg(p.x, 0, 'f', 4).arg(p.y, 0, 'f', 4).arg(p.z, 0, 'f', 4));
    if (r.nearest)
        appendResult(QString("最近图元: #%1 %2（距离 %3 mm）")
                         .arg(r.nearest->id).arg(r.nearest->typeName())
                         .arg(r.nearestDist, 0, 'f', 4));
    else
        appendResult(QStringLiteral("（模型中暂无可查询图元）"));
}

} // namespace SketchCAD
