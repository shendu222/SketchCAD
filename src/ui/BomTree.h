#pragma once
#include <QTreeWidget>

namespace SketchCAD {
class SketchModel;

// 左侧上方 BOM 树：按类型分组展示图元，支持展开/折叠、右键菜单、拖拽排序
class BomTree : public QTreeWidget {
    Q_OBJECT
public:
    explicit BomTree(SketchModel* m, QWidget* parent = nullptr);
    // 多文档切换重绑：换绑活动文档并立即按新模型重建（面板不自连接，信号由 ModelMediator 统一订阅）
    void setModel(SketchModel* m) { model_ = m; onModelChanged(); }
    void refresh();
public slots:
    void onModelChanged();
    void onSelectionChanged(int id);
protected:
    void contextMenuEvent(QContextMenuEvent* e) override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override;
    void dropEvent(QDropEvent* e) override;
    Qt::DropActions supportedDropActions() const override;
private:
    SketchModel* model_;
    QTreeWidgetItem* findItem(int id);
};
} // namespace SketchCAD
