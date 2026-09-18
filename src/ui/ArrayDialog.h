#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include "core/Vec.h"

namespace SketchCAD {

// 阵列复制对话框：在一个对话框内用两个标签页分别配置两种阵列模式。
//   * 矩形阵列：行数 / 列数 / 行间距(mm) / 列间距(mm)
//   * 环形阵列：阵列个数 / 角度范围(°) / 中心点(X,Y)
// 中心点默认由调用方传入（一般为当前选区包围盒中心）。环形阵列的中心点支持两种指定方式：
//   1) 手动输入坐标（X, Y）——在输入框中键入；
//   2) 视图拾取——点击「拾取中心点」后在绘图区点击，由 MainWindow 把世界坐标回填。
// 两种模式共用同一对 X/Y 输入框作为「单一事实来源」（effectiveCenter()），
// 因此可自由切换且始终保持一致；accept() 统一从输入框读中心点。
// 通过 result() 在 accept() 后读取用户确认的参数；accept() 内做范围校验，
// 非法时弹窗提示且不关闭对话框。
class ArrayDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { Rectangular, Polar };

    struct Result {
        Mode  mode = Rectangular;
        // 矩形阵列
        int    rows = 3, cols = 3;
        double rowSpacing = 20.0, colSpacing = 20.0;
        // 环形阵列
        int    count = 8;
        double angleRangeDeg = 360.0;
        Vec3   center;             // 环形阵列中心点（世界坐标 mm）
    };

    explicit ArrayDialog(Mode initial, const Vec3& defaultCenter,
                         QWidget* parent = nullptr);

    Result result() const { return res_; }

    // 中心点「单一事实来源」：始终返回 X/Y 输入框当前值。
    // 手动输入与视图拾取两种模式都收敛到它，故切换模式不会丢失/错乱中心点。
    Vec3 effectiveCenter() const;
    // 视图拾取回调：写入拾取到的世界坐标，并切到「视图拾取」显示态（输入框只读回显该值）。
    void setCenter(const Vec3& p);
    bool isCenterPickMode() const;   // 当前是否为「视图拾取」模式
    void setCenterMode(bool pick);   // 程序化切换中心点指定方式（true=视图拾取，false=手动输入）
    void setManualCenter(double x, double y);   // 程序化设定手动输入坐标（切到手动模式）

signals:
    // 用户点击「拾取中心点」时发射，由 MainWindow 进入视图拾取态。
    void requestCanvasPick();

public slots:
    void accept() override;

private:
    Result res_;

    // 矩形页控件
    QSpinBox*   sbRows_   = nullptr;
    QSpinBox*   sbCols_   = nullptr;
    QDoubleSpinBox* sbRowSp_ = nullptr;
    QDoubleSpinBox* sbColSp_ = nullptr;

    // 环形页控件
    QSpinBox*   sbCount_  = nullptr;
    QDoubleSpinBox* sbAngle_ = nullptr;
    QDoubleSpinBox* sbCenterX_ = nullptr;
    QDoubleSpinBox* sbCenterY_ = nullptr;

    // 中心点双模式控件
    QRadioButton*   rbManual_  = nullptr;   // 手动输入坐标 (X, Y)
    QRadioButton*   rbPick_    = nullptr;   // 在视图中拾取中心点
    QPushButton*    pickBtn_   = nullptr;   // 拾取中心点（仅拾取模式下可用）
    QButtonGroup*   modeGroup_ = nullptr;

    QTabWidget* tabs_ = nullptr;
};

} // namespace SketchCAD
