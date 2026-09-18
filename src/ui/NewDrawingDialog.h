#pragma once
#include <QDialog>
#include "core/SheetSpec.h"

namespace SketchCAD {

// 「文件 - 新建」图纸幅面选择对话框：选择 A0~A5 幅面与横/纵摆放方向，
// 返回一份 SheetSpec 供 MainWindow 生成对应图框与标题栏。
class NewDrawingDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewDrawingDialog(QWidget* parent = nullptr);

    // 用户确认后的图纸规格（取消则返回默认 A3 横向，但调用方以 exec()==Accepted 为准）
    SheetSpec selectedSpec() const {
        SheetSpec s = SheetSpec::make(fmt_, landscape_);
        s.projection = projection_;
        return s;
    }

    // 用户确认后的绘图比例（图形:实际），默认 1:1
    double selectedScaleRatio() const { return scaleRatio_; }

    // 用户确认后的投影法（GB/T 14665-2012 第7章），默认第一角
    ProjectionMethod selectedProjection() const { return projection_; }

private:
    SheetFormat fmt_ = SheetFormat::A3;
    bool         landscape_ = true;
    double       scaleRatio_ = 1.0;   // 绘图比例（1:1 = 1.0）
    ProjectionMethod projection_ = ProjectionMethod::FirstAngle;
};

} // namespace SketchCAD
