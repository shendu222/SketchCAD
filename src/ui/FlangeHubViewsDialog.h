#pragma once
#include <QDialog>
#include "core/FlangeHubViews.h"

namespace SketchCAD {

class SketchModel;

// 法兰盘轮毂视图参数对话框：阶梯回转体（法兰盘 + 轮毂圆筒 + 中心通孔/沉孔）
// 的关键尺寸全部可调整，单位毫米（mm）。生成内容：主视图全剖 A—A（含剖面线、
// 剖切符号、粗糙度、引线标注）、左视图圆形视图（同心圆 + 两圈分布圆 + 孔 +
// 十字中心线 + 竖直剖切符号 A—A）、尺寸标注与技术要求。
class FlangeHubViewsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlangeHubViewsDialog(SketchModel* model, QWidget* parent = nullptr);

private:
    void buildUi();

    SketchModel* model_;
    FlangeHubParams p_;
};

} // namespace SketchCAD
