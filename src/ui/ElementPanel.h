#pragma once
#include <QListWidget>

namespace SketchCAD {
// 左侧下方图元面板：列出可用图元类型/符号库，支持拖拽至图形区创建，双击进入绘制
class ElementPanel : public QListWidget {
    Q_OBJECT
public:
    explicit ElementPanel(QWidget* parent = nullptr);
    // 语言切换：重译图元类型条目（直线/圆/圆弧/矩形/多边形/样条/椭圆/折线/约束/标注）
    void retranslate();
signals:
    void toolSelected(const QString& tool); // 传出英文工具 id
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;
};
} // namespace SketchCAD
