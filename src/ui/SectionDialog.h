#pragma once
#include <QDialog>
#include "core/Section.h"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;

namespace SketchCAD {

// 剖面填充参数对话框（GB/T 4457.5-2013）。
// 负责收集：材料类别、豁免类别、剖面线角度、剖面线间距、是否自动区分相邻区域。
// 不直接触碰模型；参数交由 MainWindow::sectionFill 落地。
class SectionDialog : public QDialog {
    Q_OBJECT
public:
    struct Result {
        SectionStyle style;             // 角度/间距/材料/豁免
        bool autoDistinguish = true;    // 自动按 GB 4.7 区分相邻区域剖面线方向
    };

    explicit SectionDialog(QWidget* parent = nullptr);
    explicit SectionDialog(const Result& init, QWidget* parent = nullptr);

    Result result() const { return res_; }

    // ---- 程序化访问（供回归测试无界面驱动）----
    void setMaterial(SectionMaterial m);
    void setExempt(SectionExempt e);
    void setAngle(double deg);
    void setSpacing(double mm);
    void setAutoDistinguish(bool on);

    SectionMaterial material() const;
    SectionExempt   exempt() const;
    double          angle() const;
    double          spacing() const;
    bool            autoDistinguish() const;

protected:
    void accept() override;

private:
    QComboBox*      cbMaterial_ = nullptr;
    QComboBox*      cbExempt_   = nullptr;
    QDoubleSpinBox* sbAngle_    = nullptr;
    QDoubleSpinBox* sbSpacing_  = nullptr;
    QCheckBox*      cbAuto_     = nullptr;
    Result          res_;
};

} // namespace SketchCAD
