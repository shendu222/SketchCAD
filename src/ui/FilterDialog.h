#pragma once
#include <QDialog>
#include "core/QSelect.h"   // QSelectCriteria / QSelectProp / QSelectOp
class QComboBox;
class QTableWidget;
class QListWidget;
class QLineEdit;

namespace SketchCAD {
class SketchModel;

// 过滤器（FILTER）：多条件组合筛选（全部满足 AND / 任一满足 OR），
// 支持按名称保存到图纸（savedFilters）并随时复用；结果以 替换/追加/排除 落地选择集。
class FilterDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void refreshSaved();     // 重填已保存过滤器列表
    void onSavedSelected();  // 列表选中 → 预填条件区
    void addRow();           // 追加一条空条件
    void removeRow();        // 删除当前条件行
    void onSave();           // 保存当前条件为命名过滤器（重名覆盖）
    void onDelete();         // 删除选中的已保存过滤器
    void apply();            // 执行筛选并落地选择集

private:
    FilterDef currentFilter() const;   // 从条件表收集过滤器定义
    void loadFilter(const FilterDef& f); // 把过滤器定义写入条件表

    SketchModel* model_;
    QListWidget*  savedList_;   // 已保存过滤器
    QTableWidget* condTable_;   // 条件行：特性 / 运算符 / 值
    QComboBox*    cbMatch_;     // 组合方式：全部满足(AND) / 任一满足(OR)
    QComboBox*    cbAction_;    // 应用方式：替换 / 追加 / 排除
    QLineEdit*    edName_;      // 过滤器名（保存用）
};

} // namespace SketchCAD
