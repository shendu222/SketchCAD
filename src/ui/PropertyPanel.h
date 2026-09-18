#pragma once
#include <QWidget>
#include <QColor>

class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QComboBox;
class QFormLayout;
class QWidget;

namespace SketchCAD {
class SketchModel;

// 右侧上方属性面板：动态显示选中图元属性，支持编辑与实时更新
class PropertyPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertyPanel(SketchModel* m, QWidget* parent = nullptr);
    void load(int id);
public slots:
    void onSelectionChanged(int id);
    void onModelChanged();
    // 默认样式变化：当前未选中图元时同步清空为新的默认值。原为构造函数的自接 lambda，
    // 后因信号接线收口到 ModelMediator 而抽成 public slot，由中介者统一订阅。
    void onDefaultStyleChanged(const QColor& color, double width);
    void onDefaultLineStyleChanged(int style);
private:
    SketchModel* model_;
    int curId_ = -1;
    bool updating_ = false;
    QFormLayout* form_ = nullptr;
    QLabel *lId, *lType;
    QLineEdit *eName, *eLayer, *eText;
    // 几何：起点(x,y)、终点(x,y)、长度、夹角、半径
    QDoubleSpinBox *sX, *sY, *sX2, *sY2, *sLen, *sAng, *sR;
    QLabel *labX, *labY;               // 「起点 X/Y」在非直线图元上要改叫「基点 X/Y」
    QDoubleSpinBox *sDimValue;          // 标注尺寸值（可编辑，驱动源图元几何）
    // 公差字段与单标注标准覆盖（满足"用户可在界面设置公差表达规则"）
    QDoubleSpinBox *sTolUp, *sTolLo;    // 上偏差(+)/下偏差(-)
    QLineEdit *eFit;                    // 配合代号（如 H7 / g6）
    QComboBox *cDimStd;                // 单标注标准覆盖（GB/T 4458 ↔ ISO 129）
    // 多边形专属：边数、边长、内接圆半径、外接圆半径
    QSpinBox *sPolySides;
    QDoubleSpinBox *sPolySideLen, *sPolyInRadius, *sPolyCircRadius;
    QPushButton *bColor;
    QComboBox *cWidth;                 // 线宽档位
    QPushButton *bWidthAuto;           // 「恢复默认」：清除手动覆盖，线宽随线型派生
    QComboBox *cLineStyle;             // 线型：实线/虚线/点划线/双点划线
    QWidget *presetRow;                // 预设色一排小方块
    QColor curColor_;
    void apply();                      // 名称/图层/颜色/半径/文本（不含线宽/线型，见下方专方法）
    void applyWidth();                 // 改线宽下拉 -> 手动覆盖(auto=false)
    void applyStyle();                 // 改线型下拉 -> 自动模式时线宽跟随派生
    void restoreDefaultWidth();        // “恢复默认”按钮 -> 清除手动覆盖(auto=true)
    void applyCartesian();             // 改了起点或终点 -> 反算长度与夹角（联动传播）
    void applyPolar();                 // 改了长度或夹角 -> 反算终点（联动传播）
    void applyDimValue();              // 标注数值改变 -> 驱动源图元几何并联动
    void applyTolerance();              // 改公差/单标注标准 -> 写回并刷新标注文字
    void applyPoly();                  // 多边形属性改变 -> 以质心为基点重新生成顶点
    void refreshDerived();             // 由当前起终点刷新长度/夹角显示（不触发写回）
    void refreshPolyDerived();         // 刷新多边形派生属性显示（不触发写回）
    void clearFields();
    void setColor(const QColor& c);    // 统一走这里，保证按钮底色与 curColor_ 不脱节
    void setEnabledFields(bool on);
    void showRow(QWidget* field, bool on);
};
} // namespace SketchCAD
