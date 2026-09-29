#pragma once
#include <QDialog>
#include <QString>
#include "core/TextStyle.h"   // TextStyle（currentFromForm 返回类型）
class QListWidget;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;

namespace SketchCAD {
class SketchModel;

// 文字样式管理器（STYLE）：创建/修改/删除样式，统一治理字体、字高、宽度因子、倾斜、粗体/斜体，
// 并可把某样式批量应用到选中或全部文字对象（经 Element::styleName 关联，渲染时解析生效）。
class StyleManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit StyleManagerDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void onNew();
    void onDelete();
    void onSave();
    void onApplySelected();
    void onApplyAll();

private:
    void refreshList();
    void loadCurrent();
    TextStyle currentFromForm() const;

    SketchModel* model_;
    QListWidget* list_;
    QLineEdit*   edName_;
    QComboBox*   cbFont_;
    QDoubleSpinBox* sbHeight_;
    QDoubleSpinBox* sbWidth_;
    QDoubleSpinBox* sbOblique_;
    QCheckBox*  ckBold_;
    QCheckBox*  ckItalic_;
    QString     curName_;   // 当前选中样式的原始名（用于 rename 判定）
};

} // namespace SketchCAD
