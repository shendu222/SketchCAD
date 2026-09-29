#pragma once
#include <QDialog>
class QListWidget;

namespace SketchCAD {
class SketchModel;

// 编组管理器（GROUP）：创建/命名编组、向编组添加或移除选中对象、
// 整体选中编组、重命名、解散。编组 = groupName 相同的图元集合（引用式）。
class GroupManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit GroupManagerDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onCreate();     // 用当前选中对象创建新编组（弹名称输入框）
    void onAdd();        // 把当前选中对象加入列表中选中的编组
    void onRemove();     // 把当前选中对象从其所属编组移除
    void onRename();
    void onUngroup();
    void onSelect();     // 整体选中列表中选中的编组

private:
    QString currentGroup() const;   // 列表当前编组名；无选中返回空串

    SketchModel* model_;
    QListWidget* list_;
};

} // namespace SketchCAD
