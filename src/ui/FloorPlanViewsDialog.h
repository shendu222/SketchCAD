#pragma once
#include <QDialog>
#include "core/FloorPlanViews.h"

class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;

namespace SketchCAD {

class SketchModel;

// 户型平面图参数对话框：总尺寸 / 墙厚 / 门窗 / 文字 / 图纸 / 比例 / 图签文本
class FloorPlanViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FloorPlanViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_ = nullptr;
    FloorPlanParams p_;
};

} // namespace SketchCAD
