#pragma once
#include <QDialog>
#include "core/AircraftViews.h"

namespace SketchCAD {

class SketchModel;

// 飞机三面图参数对话框：所有尺寸默认取自附图，用户可修改后重新生成。
class AircraftViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AircraftViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_;
    aircraft::AircraftViewsParams p_;
};

} // namespace SketchCAD
