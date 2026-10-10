#include "ConstraintBar.h"
#include "core/ConstraintSolver.h"
#include "core/ConstraintSolverFacade.h"   // 约束求解路由门面（特性开关+回落）
#include "core/Element.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <algorithm>

namespace SketchCAD {

ConstraintBar::ConstraintBar(SketchModel* m, QWidget* parent)
    : QWidget(parent), model_(m) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);

    headerLabel_ = new QLabel(QStringLiteral("约束栏（选中对象的约束）"));
    headerLabel_->setStyleSheet(QStringLiteral("font-weight:bold;"));
    bannerLabel_ = new QLabel();
    bannerLabel_->setWordWrap(true);
    bannerLabel_->setStyleSheet(QStringLiteral("color:#d63031; padding:2px 4px;"));
    bannerLabel_->hide();

    list_ = new QListWidget();
    list_->setAlternatingRowColors(true);

    locateBtn_ = new QPushButton(QStringLiteral("定位"));
    deleteBtn_ = new QPushButton(QStringLiteral("删除"));
    auto* h = new QHBoxLayout();
    h->addWidget(locateBtn_);
    h->addWidget(deleteBtn_);
    h->addStretch(1);

    v->addWidget(headerLabel_);
    v->addWidget(bannerLabel_);   // 正常隐藏，冲突时出现
    v->addWidget(list_, 1);
    v->addLayout(h);

    connect(list_, &QListWidget::itemDoubleClicked, this, &ConstraintBar::onLocate);
    connect(locateBtn_, &QPushButton::clicked, this, &ConstraintBar::onLocate);
    connect(deleteBtn_, &QPushButton::clicked, this, &ConstraintBar::onDelete);
}

void ConstraintBar::refresh() {
    list_->clear();
    bannerLabel_->hide();

    const auto& recs = model_->constraints();

    // 收集「选中集」涉及的约束下标（选中单图元 → 仅该图元；多选 → 并集；无选中 → 全部）
    QVector<int> selIds;
    const int single = model_->selectedId();
    if (single >= 0) selIds.append(single);
    for (int id : model_->selection())
        if (id != single) selIds.append(id);

    std::vector<int> idxs;
    if (!selIds.isEmpty()) {
        for (int id : selIds)
            for (int i : ConstraintSolver::constraintsInvolving(*model_, id))
                if (std::find(idxs.begin(), idxs.end(), i) == idxs.end())
                    idxs.push_back(i);
        headerLabel_->setText(QStringLiteral("约束栏（选中 #%1 的约束，共 %2 条）")
                                 .arg(selIds.first()).arg(idxs.size()));
    } else {
        for (int i = 0; i < (int)recs.size(); ++i) idxs.push_back(i);
        headerLabel_->setText(QStringLiteral("约束栏（全部约束，共 %1 条）").arg(recs.size()));
    }

    for (int i : idxs) {
        if (i < 0 || i >= (int)recs.size()) continue;
        const auto& r = recs[i];
        const QString type = QString::fromUtf8(constraintTypeName(r.type));
        QString parts = QStringLiteral("#%1").arg(r.idA);
        if (r.idB >= 0) parts += QStringLiteral(" ↔ #%1").arg(r.idB);
        if (r.axisId >= 0) parts += QStringLiteral(" ⟂轴#%1").arg(r.axisId);
        if (r.value > 0 &&
            (r.type == ConstraintType::Dimensional || r.type == ConstraintType::Distance ||
             r.type == ConstraintType::Angle)) {
            const QString unit = (r.type == ConstraintType::Angle) ? QStringLiteral("°") : QString();
            parts += QStringLiteral(" = %1%2").arg(r.value, 0, 'f', 2).arg(unit);
        }
        const QString stat = r.satisfied ? QStringLiteral("满足") : QStringLiteral("未满足");
        auto* item = new QListWidgetItem(QStringLiteral("%1  %2  [%3]").arg(type, parts, stat));
        item->setData(Qt::UserRole, i);            // 约束表下标（删除用）
        item->setData(Qt::UserRole + 1, r.idA);   // 主相关图元 id（定位用）
        if (!r.satisfied) item->setForeground(QColor(200, 90, 30));
        list_->addItem(item);
    }

    // 过约束 / 冲突横幅：与图形视图高亮共用 diagnose 结论
    const auto diag = ConstraintSolverFacade::instance().diagnose(*model_);
    if (!diag.overconstrainedIds.empty() ||
        diag.state == ConstraintDiagnosis::OverConstrained ||
        diag.state == ConstraintDiagnosis::Conflicting) {
        QStringList s; for (int id : diag.overconstrainedIds) s << QString::number(id);
        bannerLabel_->setText(QStringLiteral("⚠ %1（图元 #%2）")
                                 .arg(diag.stateText()).arg(s.join(" #")));
        bannerLabel_->show();
    }
}

void ConstraintBar::onLocate() {
    QListWidgetItem* it = list_->currentItem();
    if (!it) return;
    const int id = it->data(Qt::UserRole + 1).toInt();
    if (id >= 0) emit locateRequested(id);
}

void ConstraintBar::onDelete() {
    QListWidgetItem* it = list_->currentItem();
    if (!it) return;
    const int idx = it->data(Qt::UserRole).toInt();
    emit deleteRequested(idx);
}

} // namespace SketchCAD
