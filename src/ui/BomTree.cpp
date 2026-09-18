#include "BomTree.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QDropEvent>
#include <QMimeData>
#include <QMap>

namespace SketchCAD {

BomTree::BomTree(SketchModel* m, QWidget* p) : QTreeWidget(p), model_(m) {
    setHeaderLabel(QStringLiteral("模型浏览器"));
    setColumnCount(1);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    refresh();
}

void BomTree::refresh() {
    clear();
    auto* root = new QTreeWidgetItem(this, QStringList{QStringLiteral("草图模型")});
    root->setExpanded(true);
    QMap<QString, QTreeWidgetItem*> groups;
    for (const auto& e : model_->elements()) {
        const QString g = e.typeName();
        if (!groups.contains(g)) {
            auto* gi = new QTreeWidgetItem(root, QStringList{g});
            gi->setExpanded(true);
            groups[g] = gi;
        }
        auto* it = new QTreeWidgetItem(groups[g],
            QStringList{QString("#%1  %2").arg(e.id).arg(e.name)});
        it->setData(0, Qt::UserRole, e.id);
    }
}

QTreeWidgetItem* BomTree::findItem(int id) {
    QList<QTreeWidgetItem*> all = findItems("*", Qt::MatchWildcard);
    for (auto* it : all)
        if (it->data(0, Qt::UserRole).toInt() == id) return it;
    return nullptr;
}

void BomTree::onModelChanged() { refresh(); }
void BomTree::onSelectionChanged(int id) {
    QTreeWidgetItem* it = findItem(id);
    if (it) { setCurrentItem(it); it->setSelected(true); }
}

void BomTree::contextMenuEvent(QContextMenuEvent* e) {
    QTreeWidgetItem* it = itemAt(e->pos());
    if (!it) return;
    int id = it->data(0, Qt::UserRole).toInt();
    QMenu menu(this);
    if (id >= 0) {
        menu.addAction(QStringLiteral("选中"), [=] { model_->setSelected(id); });
        menu.addAction(QStringLiteral("添加到选区"), [=] { model_->addToSelection(id); });
        menu.addAction(QStringLiteral("从选区移除"), [=] { model_->deselectEntity(id); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("选择轮廓"), [=] { model_->selectContour(id); });
        menu.addAction(QStringLiteral("选择相交"), [=] { model_->selectIntersected(id); });
        menu.addAction(QStringLiteral("选择同图层"), [=] { model_->selectLayerOf(id); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("复制并粘贴"), [=] {
            model_->setSelected(id); model_->copySelected(); model_->paste();
        });
        menu.addAction(QStringLiteral("删除"), [=] { model_->removeElement(id); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("移到末尾"), [=] { model_->reorderElement(id, -1); });
        menu.addAction(QStringLiteral("移到开头"), [=] {
            int first = model_->elements().empty() ? -1 : model_->elements().front().id;
            model_->reorderElement(id, first);
        });
    }
    menu.exec(e->globalPos());
}

QStringList BomTree::mimeTypes() const { return {"application/x-bom-id"}; }
Qt::DropActions BomTree::supportedDropActions() const { return Qt::MoveAction; }

QMimeData* BomTree::mimeData(const QList<QTreeWidgetItem*>& items) const {
    auto* m = new QMimeData();
    if (!items.isEmpty())
        m->setData("application/x-bom-id",
                   QString::number(items[0]->data(0, Qt::UserRole).toInt()).toUtf8());
    return m;
}

void BomTree::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasFormat("application/x-bom-id")) {
        int src = e->mimeData()->data("application/x-bom-id").toInt();
        QTreeWidgetItem* tgt = itemAt(e->position().toPoint());
        int before = -1;
        if (tgt) { int tid = tgt->data(0, Qt::UserRole).toInt(); if (tid >= 0) before = tid; }
        model_->reorderElement(src, before);
        e->acceptProposedAction();
    } else {
        e->ignore();
    }
}

} // namespace SketchCAD
