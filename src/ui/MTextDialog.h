#pragma once
#include <QDialog>
#include <QString>
#include "core/Vec.h"   // Vec3（buildElement 锚点参数）
class QPlainTextEdit;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;

namespace SketchCAD {
class SketchModel;
class Element;

// 多行文字（MTEXT）编辑对话框：编辑内容、字高、宽度因子、9 点对齐、行距、自动换行宽度、
// 关联文字样式，并支持插入 FIELD 字段占位（%<CODE>%）。确认后由调用方用 buildElement() 取图元。
class MTextDialog : public QDialog {
    Q_OBJECT
public:
    // initial：非空表示编辑已有图元（预填其属性）；为空表示新建（取默认）。
    explicit MTextDialog(SketchModel* model, QWidget* parent = nullptr,
                        const Element* initial = nullptr);

    // 用对话框当前设置 + 给定锚点构造一个 MText 图元（供工具/双击编辑调用）。
    Element buildElement(const Vec3& anchor) const;

private slots:
    void insertField();
    void onStyleChanged(int idx);

private:
    SketchModel* model_;
    QPlainTextEdit* edText_;
    QComboBox*     cbStyle_;
    QDoubleSpinBox* sbHeight_;
    QDoubleSpinBox* sbWidthFactor_;
    QComboBox*     cbAlign_;
    QDoubleSpinBox* sbLineSpacing_;
    QDoubleSpinBox* sbWrap_;
    QComboBox*     cbField_;
};

} // namespace SketchCAD
