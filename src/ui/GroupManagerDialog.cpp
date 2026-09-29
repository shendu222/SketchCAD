#include "GroupManagerDialog.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>

namespace SketchCAD {

GroupManagerDialog::GroupManagerDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("编组管理器 (GROUP)"));
    resize(440, 420);

    list_ = new QListWidget(this);

    auto* bCreate = new QPushButton(QStringLiteral("用选中对象创建编组..."), this);
    auto* bAdd    = new QPushButton(QStringLiteral("添加选中对象"), this);
    auto* bRemove = new QPushButton(QStringLiteral("移除选中对象"), this);
    auto* bRename = new QPushButton(QStringLiteral("重命名..."), this);
    auto* bUngroup= new QPushButton(QStringLiteral("解散编组"), this);
    auto* bSelect = new QPushButton(QStringLiteral("整体选中编组"), this);

    connect(bCreate, &QPushButton::clicked, this, &GroupManagerDialog::onCreate);
    connect(bAdd,    &QPushButton::clicked, this, &GroupManagerDialog::onAdd);
    connect(bRemove, &QPushButton::clicked, this, &GroupManagerDialog::onRemove);
    connect(bRename, &QPushButton::clicked, this, &GroupManagerDialog::onRename);
    connect(bUngroup,&QPushButton::clicked, this, &GroupManagerDialog::onUngroup);
    connect(bSelect, &QPushButton::clicked, this, &GroupManagerDialog::onSelect);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { onSelect(); });

    auto* lbtns = new QVBoxLayout;
    lbtns->addWidget(bCreate);
    lbtns->addWidget(bAdd);
    lbtns->addWidget(bRemove);
    lbtns->addWidget(bRename);
    lbtns->addWidget(bUngroup);
    lbtns->addWidget(bSelect);
    lbtns->addStretch(1);

    auto* h = new QHBoxLayout;
    h->addWidget(list_, 1);
    h->addLayout(lbtns);

    auto* tip = new QLabel(
        QStringLiteral("编组 = 名称相同的对象集合。选中若干对象后可创建编组或加入既有编组；\n"
                       "双击编组名 = 整体选中。编组关系随图存档，可整组撤销。"), this);
    tip->setWordWrap(true);

    auto* v = new QVBoxLayout(this);
    v->addWidget(tip);
    v->addLayout(h, 1);

    refresh();
}

void GroupManagerDialog::refresh() {
    list_->clear();
    for (const QString& g : model_->groupNames())
        list_->addItem(QStringLiteral("%1（%2 个对象）").arg(g).arg(model_->elementCountInGroup(g)));
}

QString GroupManagerDialog::currentGroup() const {
    QListWidgetItem* it = list_->currentItem();
    if (!it) return QString();
    // 显示文本带"（N 个对象）"后缀，取全角括号前主体
    const QString t = it->text();
    const int p = t.indexOf(QStringLiteral("（"));
    return p > 0 ? t.left(p) : t;
}

void GroupManagerDialog::onCreate() {
    const std::vector<int> sel = model_->selection();
    if (sel.empty()) {
        QMessageBox::information(this, QStringLiteral("创建编组"), QStringLiteral("请先在绘图区选中要编组的对象。"));
        return;
    }
    bool ok = false;
    const QString nm = QInputDialog::getText(this, QStringLiteral("创建编组"),
                                             QStringLiteral("编组名："), QLineEdit::Normal,
                                             QStringLiteral("编组%1").arg(model_->groupNames().size() + 1), &ok);
    if (!ok) return;
    if (model_->createGroup(nm, sel)) refresh();
}

void GroupManagerDialog::onAdd() {
    const QString g = currentGroup();
    const std::vector<int> sel = model_->selection();
    if (g.isEmpty()) { QMessageBox::information(this, QStringLiteral("添加到编组"), QStringLiteral("请先在列表中选择目标编组。")); return; }
    if (sel.empty()) { QMessageBox::information(this, QStringLiteral("添加到编组"), QStringLiteral("请先在绘图区选中要加入的对象。")); return; }
    model_->addToGroup(g, sel);
    refresh();
}

void GroupManagerDialog::onRemove() {
    const std::vector<int> sel = model_->selection();
    if (sel.empty()) { QMessageBox::information(this, QStringLiteral("移出编组"), QStringLiteral("请先在绘图区选中要移出的对象。")); return; }
    model_->removeFromGroup(sel);
    refresh();
}

void GroupManagerDialog::onRename() {
    const QString g = currentGroup();
    if (g.isEmpty()) return;
    bool ok = false;
    const QString nm = QInputDialog::getText(this, QStringLiteral("重命名编组"),
                                             QStringLiteral("新编组名："), QLineEdit::Normal, g, &ok);
    if (!ok) return;
    model_->renameGroup(g, nm);
    refresh();
}

void GroupManagerDialog::onUngroup() {
    const QString g = currentGroup();
    if (g.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("解散编组"),
                              QStringLiteral("解散编组「%1」？对象本身保留，仅解除编组关系。").arg(g))
            != QMessageBox::Yes) return;
    model_->ungroup(g);
    refresh();
}

void GroupManagerDialog::onSelect() {
    const QString g = currentGroup();
    if (g.isEmpty()) return;
    const int n = model_->selectGroup(g);
    if (n > 0) accept();   // 选中后关闭对话框，回画布直接操作
}

} // namespace SketchCAD
