#pragma once
#include <QDialog>
#include "core/QSelect.h"   // QSelectCriteria / QSelectProp / QSelectOp
class QComboBox;
class QCheckBox;

namespace SketchCAD {
class SketchModel;

// 快速选择（QSELECT）：按对象类型 + 特性（图层/颜色/线型/文字内容/编组/样式…）
// 在整个图形或当前选择集范围内筛选，结果以 替换/追加/排除 三种方式落地到选择集。
class QSelectDialog : public QDialog {
    Q_OBJECT
public:
    explicit QSelectDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void refreshValues();   // 特性/类型切换 → 重填候选值下拉
    void apply();           // 执行筛选并落地选择集

private:
    QSelectCriteria currentCriteria() const;

    SketchModel* model_;
    QComboBox* cbScope_;    // 应用范围：整个图形 / 当前选择集
    QComboBox* cbType_;     // 对象类型（"不限" + 全部类型）
    QComboBox* cbProp_;     // 特性
    QComboBox* cbOp_;       // 运算符
    QComboBox* cbValue_;    // 值（可编辑，预填图形内现存值）
    QComboBox* cbAction_;   // 应用方式：替换 / 追加 / 排除
    QCheckBox* ckIncludeInNew_; // "包含在新的选择集中"（AutoCAD 语义：结果并入新选择集）
};

} // namespace SketchCAD
