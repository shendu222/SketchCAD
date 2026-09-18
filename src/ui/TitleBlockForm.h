#pragma once
#include <QDialog>
#include <QVector>

namespace SketchCAD {
class SketchModel;

// 「填写标题栏」表单：扫描当前图纸中 name 以 "tb:field:" 开头的文本图元，
// 按固定栏目顺序列出输入框，确认后将各栏目文字写回对应图元，随图纸一起保存。
class TitleBlockForm : public QDialog {
    Q_OBJECT
public:
    explicit TitleBlockForm(SketchModel* model, QWidget* parent = nullptr);
};

} // namespace SketchCAD
