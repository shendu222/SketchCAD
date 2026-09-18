#pragma once
#include <QDialog>
#include "core/HousingViews.h"

namespace SketchCAD {

class SketchModel;

// 箱体视图参数对话框：关键尺寸全部可调整，单位毫米（mm）。
// 生成内容：主视图（全剖，含剖切符号 A-A 与粗糙度）、俯视图 B、A—A 环形断面、C 向视图、技术要求。
class HousingViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit HousingViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_;
    HousingParams p_;
};

} // namespace SketchCAD
