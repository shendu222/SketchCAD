#pragma once
#include <QDialog>
#include "core/SheetSpec.h"

namespace SketchCAD {

// 「按幅面生成图框与标题栏」对话框：选择 A0~A5 幅面与横/纵摆放方向，
// 返回一份 SheetSpec 供 MainWindow 在当前图纸上自动套框（含内容避让其图元）。
class PaperSizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit PaperSizeDialog(QWidget* parent = nullptr);

    // 用户确认后的图纸规格（取消则调用方以 exec()==Accepted 判断，不会用到该值）
    SheetSpec selectedSpec() const { return SheetSpec::make(fmt_, landscape_); }

private:
    SheetFormat fmt_ = SheetFormat::A3;
    bool         landscape_ = true;
};

} // namespace SketchCAD
