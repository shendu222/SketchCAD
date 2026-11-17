#include "core/I18n.h"
#include "ui/StandardsBrowserDialog.h"
#include "core/standards/StandardsDB.h" // 只读浏览数据源（与命令/LLM 工具共用单例）

#include <QListWidget>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace SketchCAD {

// 把 JSON 标量/对象/数组统一为可读文本：对象/数组 → Compact JSON，标量 → 字符串化。
static QString valueToText(const QJsonValue& v) {
    if (v.isObject() || v.isArray()) {
        QJsonDocument doc(v.isObject() ? QJsonDocument(v.toObject()) : QJsonDocument(v.toArray()));
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }
    if (v.isNull() || v.isUndefined()) return QString();
    return v.toVariant().toString();
}

StandardsBrowserDialog::StandardsBrowserDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(SketchCAD::I18n::t(tr("标准知识库")));
    resize(780, 540);

    const StandardsDB& db = StandardsDB::instance();

    auto* root = new QVBoxLayout(this);
    const int n = db.allTables().size();
    srcLabel_ = new QLabel(SketchCAD::I18n::t(tr("机械制图国标数据（内嵌《机械制图手册（第四版）》）：共 %1 类表，只读浏览。")).arg(n));
    root->addWidget(srcLabel_);

    auto* split = new QHBoxLayout();
    list_ = new QListWidget();
    list_->setFixedWidth(190);
    for (const QString& t : db.allTables()) list_->addItem(t);
    split->addWidget(list_);

    auto* right = new QVBoxLayout();
    filter_ = new QLineEdit();
    filter_->setPlaceholderText(SketchCAD::I18n::t(tr("过滤当前表的键名 / 值")));
    right->addWidget(filter_);

    table_ = new QTableWidget(0, 2);
    table_->setHorizontalHeaderLabels({ SketchCAD::I18n::t(tr("键")), SketchCAD::I18n::t(tr("值")) });
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->verticalHeader()->setVisible(false);
    right->addWidget(table_, 1);
    split->addLayout(right, 1);
    root->addLayout(split);

    connect(list_, &QListWidget::currentTextChanged, this, &StandardsBrowserDialog::onTableSelected);
    connect(filter_, &QLineEdit::textChanged, this, &StandardsBrowserDialog::onFilterChanged);

    if (list_->count() > 0) list_->setCurrentRow(0);
}

void StandardsBrowserDialog::onTableSelected() {
    populateTable(list_->currentItem() ? list_->currentItem()->text() : QString());
}

void StandardsBrowserDialog::populateTable(const QString& table) {
    const StandardsDB& db = StandardsDB::instance();
    const QJsonObject obj = db.tableObject(table);
    table_->setRowCount(0);
    int r = 0;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        table_->insertRow(r);
        table_->setItem(r, 0, new QTableWidgetItem(it.key()));
        table_->setItem(r, 1, new QTableWidgetItem(valueToText(it.value())));
        ++r;
    }
    table_->resizeColumnsToContents();
    // 切换表时清空过滤，避免误隐藏新表内容
    filter_->blockSignals(true);
    filter_->clear();
    filter_->blockSignals(false);
}

void StandardsBrowserDialog::onFilterChanged(const QString& text) {
    const QString t = text.trimmed().toLower();
    for (int i = 0; i < table_->rowCount(); ++i) {
        bool show = t.isEmpty();
        if (!show) {
            const QString k = table_->item(i, 0) ? table_->item(i, 0)->text().toLower() : QString();
            const QString v = table_->item(i, 1) ? table_->item(i, 1)->text().toLower() : QString();
            show = k.contains(t) || v.contains(t);
        }
        table_->setRowHidden(i, !show);
    }
}

} // namespace SketchCAD
