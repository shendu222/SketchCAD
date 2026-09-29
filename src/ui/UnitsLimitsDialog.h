#pragma once
#include <QDialog>
#include "core/Units.h"
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

namespace SketchCAD {
class SketchModel;

// 单位 / 图纸界限设置面板（UNITS / LIMITS）：
//   - 长度单位（毫米/厘米/米/英寸/英尺）与角度单位（十进制度/度分秒/弧度/百分度）
//   - 数值显示精度（长度/角度各 0..6 位小数）
//   - 图纸界限（LIMITS）矩形范围 + 越界检查开关（坐标恒为模型 mm）
// 确认后经 SketchModel::setUnits/setLimits 落地（变更发 unitsChanged/limitsChanged
// 信号，状态栏坐标、参数输入条后缀实时跟随）。单位切换不改任何已有几何——
// 只换显示与输入口径（单位不变式：内部几何一律 mm）。
class UnitsLimitsDialog : public QDialog {
    Q_OBJECT
public:
    explicit UnitsLimitsDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void updatePreview();   // 单位/精度变化 → 刷新示例格式化预览
    void accept();          // 落地到模型（setUnits/setLimits）

private:
    UnitsMeta collectUnits() const;
    LimitsMeta collectLimits() const;

    SketchModel*    model_;
    QComboBox*      cbLength_;      // 长度单位
    QComboBox*      cbAngle_;       // 角度单位
    QSpinBox*       sbLengthPrec_;  // 长度显示小数位
    QSpinBox*       sbAnglePrec_;   // 角度显示小数位
    QCheckBox*      ckCheckOn_;     // LIMITS 越界检查开关
    QDoubleSpinBox* sbX1_, * sbY1_, * sbX2_, * sbY2_;   // 界限矩形（模型 mm）
    QLabel*         lbPreview_;     // 示例格式化预览
};

} // namespace SketchCAD
