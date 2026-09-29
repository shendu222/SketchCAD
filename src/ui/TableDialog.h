#pragma once
#include <QDialog>
#include <QString>
#include "core/Vec.h"   // Vec3（buildElement 锚点参数）
class QTableWidget;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QLineEdit;

namespace SketchCAD {
class SketchModel;
class Element;

// 表格（TABLE / 明细表）编辑对话框：
//   - QTableWidget 逐格编辑单元格文本（支持 %<字段>% 占位，渲染/打印时求值）
//   - 行数/列数/行高/字高/水平对齐/各列宽度（逗号分隔 mm）
// initial 非空表示编辑已有表格（预填其全部数据）；为空表示新建（默认 3×4、列宽 30mm）。
// 确认后由调用方用 buildElement(topLeft) 取图元（新建含模型默认样式）。
class TableDialog : public QDialog {
    Q_OBJECT
public:
    explicit TableDialog(SketchModel* model, QWidget* parent = nullptr,
                         const Element* initial = nullptr);

    // 用对话框当前设置 + 左上角锚点构造 Table 图元（套用模型默认样式）。
    Element buildElement(const Vec3& topLeft) const;

private slots:
    void rebuildGrid();     // 行列数变化 → 重建网格（保留已填内容）

private:
    void loadFrom(const Element& e);   // 编辑模式：预填已有表格

    SketchModel*    model_;
    QTableWidget*   grid_;          // 单元格文本编辑区
    QSpinBox*       sbRows_;
    QSpinBox*       sbCols_;
    QDoubleSpinBox* sbRowHeight_;
    QDoubleSpinBox* sbTextHeight_;
    QComboBox*      cbAlign_;       // 单元格文字水平对齐：左/中/右
    QLineEdit*      edColWidths_;   // 逗号分隔列宽 mm
};

} // namespace SketchCAD
