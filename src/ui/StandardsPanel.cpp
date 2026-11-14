#include "ui/StandardsPanel.h"

#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QAbstractItemView>

#include "core/SketchModel.h"
#include "core/CommandInterpreter.h"   // 「运行检查」按钮经命令解释器派发 stdcheck
#include "ui/GraphicsView.h"

namespace SketchCAD {

StandardsPanel::StandardsPanel(SketchModel* model, QWidget* parent)
    : QWidget(parent), model_(model) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(6, 6, 6, 6);

    summaryLabel_ = new QLabel(QStringLiteral("尚未运行合规检查（执行 STDCHECK 后更新）。"));
    summaryLabel_->setWordWrap(true);
    vl->addWidget(summaryLabel_);

    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setAlternatingRowColors(true);
    connect(list_, &QListWidget::itemDoubleClicked, this, &StandardsPanel::onItemDoubleClicked);
    vl->addWidget(list_);

    runBtn_ = new QPushButton(QStringLiteral("运行合规检查 (STDCHECK)"));
    connect(runBtn_, &QPushButton::clicked, this, &StandardsPanel::onRunCheck);
    vl->addWidget(runBtn_);

    refresh();
}

void StandardsPanel::setModel(SketchModel* model) {
    model_ = model;
    refresh();
}

void StandardsPanel::setView(GraphicsView* view) {
    view_ = view;
}

void StandardsPanel::setIssues(const QVector<ComplianceIssue>& issues) {
    issues_ = issues;
    refresh();
}

void StandardsPanel::refresh() {
    list_->clear();
    if (issues_.isEmpty()) {
        summaryLabel_->setText(QStringLiteral("未发现不合规项（执行 STDCHECK 后更新）。"));
        return;
    }
    summaryLabel_->setText(QStringLiteral("发现 %1 个不合规项（双击可跳转到图元）：").arg(issues_.size()));
    for (int i = 0; i < issues_.size(); ++i) {
        const ComplianceIssue& iss = issues_[i];
        const QString label = QStringLiteral("[%1] #%2 %3：%4")
                                  .arg(iss.severity)
                                  .arg(iss.elementId)
                                  .arg(iss.ruleId)
                                  .arg(iss.message);
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, i);   // 记录原始 issue 下标，双击时取回
        item->setToolTip(iss.fixHint);
        list_->addItem(item);
    }
}

void StandardsPanel::onRunCheck() {
    if (!model_) return;
    // 复用命令路径：经 CommandInterpreter 派发 stdcheck，
    // 命令内部会 logAppended 上报并 notifyComplianceReport 刷新本面板。
    CommandInterpreter interp(model_);
    interp.execute(QStringLiteral("stdcheck"));
}

void StandardsPanel::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item || !model_) return;
    const int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= issues_.size()) return;
    const ComplianceIssue& iss = issues_[idx];
    if (iss.elementId > 0) {
        model_->setSelected(iss.elementId);
        if (view_) view_->zoomToElement(iss.elementId);
    }
}

} // namespace SketchCAD
