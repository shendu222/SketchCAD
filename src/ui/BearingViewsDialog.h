#pragma once
#include <QDialog>
#include "core/BearingViews.h"

namespace SketchCAD {

class SketchModel;

// 轴承座三面图参数对话框：关键尺寸全部可调整，单位毫米（mm）。
class BearingViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit BearingViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_;
    BearingParams p_;
};

} // namespace SketchCAD
