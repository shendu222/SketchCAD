#pragma once
#include <QListWidget>

namespace SketchCAD {
// 左侧下方图元面板：列出可用图元类型/符号库，支持拖拽至图形区创建，双击进入绘制
class ElementPanel : public QListWidget {
    Q_OBJECT
public:
    explicit ElementPanel(QWidget* parent = nullptr);
signals:
    void toolSelected(const QString& tool); // 传出英文工具 id
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
};
} // namespace SketchCAD
