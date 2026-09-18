#include "ElementPanel.h"
#include "dnd.h"
#include <QMimeData>

namespace SketchCAD {

ElementPanel::ElementPanel(QWidget* p) : QListWidget(p) {
    setDragEnabled(true);
    setAcceptDrops(false);
    setViewMode(QListView::ListMode);
    struct P { QString zh, en; };
    P list[] = {
        {"直线", "line"}, {"圆", "circle"}, {"三点圆", "circle3p"}, {"圆弧", "arc"},
        {"矩形", "rect"}, {"多边形", "poly"}, {"样条曲线", "spline"},
        {"椭圆", "ellipse_1pt"}, {"折线", "polyline"},
        {"约束", "constraint"}, {"标注", "dim"}
    };
    for (const auto& x : list) {
        auto* it = new QListWidgetItem(x.zh);
        it->setData(Qt::UserRole, x.en);
        addItem(it);
    }
    connect(this, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        emit toolSelected(it->data(Qt::UserRole).toString());
    });
}

QMimeData* ElementPanel::mimeData(const QList<QListWidgetItem*>& items) const {
    auto* m = new QMimeData();
    if (!items.isEmpty())
        m->setData(ELEMENT_MIME, items[0]->data(Qt::UserRole).toString().toUtf8());
    return m;
}

} // namespace SketchCAD
