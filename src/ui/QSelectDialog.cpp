#include "QSelectDialog.h"
#include "core/SketchModel.h"
#include "core/QSelect.h"
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

namespace SketchCAD {

QSelectDialog::QSelectDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("快速选择 (QSELECT)"));
    resize(460, 280);

    cbScope_ = new QComboBox(this);
    cbScope_->addItem(QStringLiteral("整个图形"), 0);
    cbScope_->addItem(QStringLiteral("当前选择集内"), 1);

    // 对象类型下拉：不限 + 全部图元类型（与 elementTypeName 同源，枚举只增不改）
    cbType_ = new QComboBox(this);
    cbType_->addItem(QStringLiteral("不限"), QString());
    for (int t = 0; t <= int(ElementType::Table); ++t) {
        const ElementType et = (ElementType)t;
        cbType_->addItem(QString::fromUtf8(elementTypeName(et)),
                         QString::fromUtf8(elementTypeName(et)));
    }

    cbProp_ = new QComboBox(this);
    for (QSelectProp p : { QSelectProp::None, QSelectProp::Layer, QSelectProp::Color,
                           QSelectProp::LineStyle, QSelectProp::LineWidth, QSelectProp::Text,
                           QSelectProp::Name, QSelectProp::Group, QSelectProp::Style,
                           QSelectProp::Visible })
        cbProp_->addItem(qselectPropName(p), int(p));

    cbOp_ = new QComboBox(this);
    for (QSelectOp op : { QSelectOp::Eq, QSelectOp::Ne, QSelectOp::Contains, QSelectOp::StartsWith })
        cbOp_->addItem(qselectOpName(op), int(op));

    cbValue_ = new QComboBox(this);
    cbValue_->setEditable(true);

    cbAction_ = new QComboBox(this);
    cbAction_->addItem(QStringLiteral("替换当前选择集"), int(QSelectAction::Replace));
    cbAction_->addItem(QStringLiteral("追加到当前选择集"), int(QSelectAction::Append));
    cbAction_->addItem(QStringLiteral("从当前选择集排除"), int(QSelectAction::Exclude));

    connect(cbProp_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QSelectDialog::refreshValues);
    connect(cbType_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QSelectDialog::refreshValues);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("应用范围："), cbScope_);
    form->addRow(QStringLiteral("对象类型："), cbType_);
    form->addRow(QStringLiteral("特性："),     cbProp_);
    form->addRow(QStringLiteral("运算符："),   cbOp_);
    form->addRow(QStringLiteral("值："),       cbValue_);
    form->addRow(QStringLiteral("应用方式："), cbAction_);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QSelectDialog::apply);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* tip = new QLabel(
        QStringLiteral("筛选范围可选整个图形或当前选择集；结果以 替换/追加/排除 三种方式\n"
                       "落地到选择集，与点击选择/框选共用同一选择集状态。"), this);
    tip->setWordWrap(true);

    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    v->addWidget(tip);
    v->addWidget(btns);

    refreshValues();
}

QSelectCriteria QSelectDialog::currentCriteria() const {
    QSelectCriteria c;
    c.prop  = QSelectProp(cbProp_->currentData().toInt());
    c.op    = QSelectOp(cbOp_->currentData().toInt());
    c.value = cbValue_->currentText().trimmed();
    return c;
}

void QSelectDialog::refreshValues() {
    const QSelectProp p = QSelectProp(cbProp_->currentData().toInt());
    const QString cur = cbValue_->currentText();
    cbValue_->clear();
    if (p != QSelectProp::None)
        cbValue_->addItems(uniquePropValues(model_, p));
    cbValue_->setEditable(true);
    if (!cur.isEmpty()) cbValue_->setCurrentText(cur);
}

void QSelectDialog::apply() {
    // 组装过滤器：类型条件（可选）+ 特性条件（可选），AND 组合；
    // 两者均"不限" = 命中全部可见图元（等价全选），仍按所选方式落地。
    FilterDef f;
    f.name = QStringLiteral("QSELECT");
    f.matchAll = true;
    const QString tp = cbType_->currentData().toString();
    if (!tp.isEmpty()) {
        QSelectCriteria tc;
        tc.prop = QSelectProp::Type; tc.op = QSelectOp::Eq; tc.value = tp;
        f.conds.push_back(tc);
    }
    QSelectCriteria pc = currentCriteria();
    if (pc.prop != QSelectProp::None) f.conds.push_back(pc);

    const bool inSelection = cbScope_->currentData().toInt() == 1;
    // 大图性能关键路径：qselect 为单遍 O(n) 扫描；范围内筛选经 QSet 预建索引
    const std::vector<int> hits = qselect(model_, f,
                                          inSelection ? &model_->selection() : nullptr);

    const QSelectAction act = QSelectAction(cbAction_->currentData().toInt());
    model_->applySelectionDelta(hits, act);   // 单次落地，一次 selectionChanged

    const QString actName = (act == QSelectAction::Replace) ? QStringLiteral("替换")
                          : (act == QSelectAction::Append)  ? QStringLiteral("追加")
                                                            : QStringLiteral("排除");
    model_->log(QStringLiteral("快速选择：命中 %1 个对象（%2 选择集）").arg(hits.size()).arg(actName));
    accept();
}

} // namespace SketchCAD
