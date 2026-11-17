#include "ElementPanel.h"
#include "core/I18n.h"
#include "dnd.h"
#include <QMimeData>

namespace SketchCAD {

// 图元类型条目唯一数据源：中文显示名 + 英文工具 id（语言切换时按 zh 重译）。
namespace {
struct PanelItem { const char* zh; const char* en; };
const PanelItem kPanelItems[] = {
    {"直线", "line"}, {"圆", "circle"}, {"三点圆", "circle3p"}, {"圆弧", "arc"},
    {"矩形", "rect"}, {"多边形", "poly"}, {"样条曲线", "spline"},
    {"椭圆", "ellipse_1pt"}, {"折线", "polyline"},
    {"约束", "constraint"}, {"标注", "dim"}
};
constexpr int kPanelItemCount = int(sizeof(kPanelItems) / sizeof(kPanelItems[0]));
}

ElementPanel::ElementPanel(QWidget* p) : QListWidget(p) {
    setDragEnabled(true);
    setAcceptDrops(false);
    setViewMode(QListView::ListMode);
    for (int i = 0; i < kPanelItemCount; ++i) {
        auto* it = new QListWidgetItem(SketchCAD::I18n::t(QString::fromUtf8(kPanelItems[i].zh)));
        it->setData(Qt::UserRole, QString::fromUtf8(kPanelItems[i].en));
        addItem(it);
    }
    connect(this, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        emit toolSelected(it->data(Qt::UserRole).toString());
    });
}

// 语言切换：按 zh 源串重译条目，英文工具 id 不变。
void ElementPanel::retranslate() {
    for (int i = 0; i < kPanelItemCount && i < count(); ++i)
        item(i)->setText(SketchCAD::I18n::t(QString::fromUtf8(kPanelItems[i].zh)));
}

QMimeData* ElementPanel::mimeData(const QList<QListWidgetItem*>& items) const {
    auto* m = new QMimeData();
    if (!items.isEmpty())
        m->setData(ELEMENT_MIME, items[0]->data(Qt::UserRole).toString().toUtf8());
    return m;
}

} // namespace SketchCAD
