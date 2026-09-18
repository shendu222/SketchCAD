#pragma once
#include <QDialog>
#include "core/FlangeViews.h"

namespace SketchCAD {

class SketchModel;

// 法兰盘三面图参数对话框：关键尺寸全部可调整，单位毫米（mm）。
class FlangeViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlangeViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_;
    FlangeParams p_;
};

} // namespace SketchCAD
