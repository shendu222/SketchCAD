#pragma once
#include <QDialog>
#include "core/SheetSpec.h"
#include "core/SheetFrameTemplate.h"   // 完整类型：成员 QVector<SheetFrameTemplate>（moc 元类型需完整）

class QButtonGroup;
class QRadioButton;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace SketchCAD {

// 【图纸】>【新建图框】对话框：严格按附图布局（界面完全相同）。
// 左列：模板 / 图幅(A0~A4+加长+自定义) / 布置方式+绘图比例(双下拉:前值:后值) /
//       分区 / 底部选项组(自动填写日期·对中符号·装订线·指定图框位置·装订宽度+确定取消)。
// 右列：实时预览 / 标题栏→明细栏→附加栏(含是否显示左分区)→代号栏→参数栏
//       （各栏=开关+系统预设下拉+预览按钮，关闭时禁止选取） / 比例调整设置组。
// 需求 13：图框布置编辑模板（存为模板/管理模板，选择模板快速切换全部设置）。
// 右侧实时预览由「同一套生成图元」绘制，与最终落盘完全一致（单一数据源）。
class SheetFrameDialog : public QDialog {
    Q_OBJECT
public:
    explicit SheetFrameDialog(QWidget* parent = nullptr);

    // 确认后由 MainWindow 取用
    SheetFrameOptions result() const { return opts_; }
    double selectedScaleRatio() const { return scaleDen_ > 0.0 ? scaleNum_ / scaleDen_ : 1.0; }

private:
    void buildUi();
    void syncOptionsFromUi();       // 所有控件变更 → 刷新校验 + 预览
    void updatePreview();
    void validateZones();           // 分区偶数 + 25~75mm 实时校验
    void loadOptsToUi();            // opts_ + 比例 → 全部控件（应用模板时整批回填）
    void applyTemplate(const SheetFrameTemplate& t);
    void saveCurrentAsTemplate();
    void manageTemplates();
    void showBarPreview(int kind);  // 各栏「预览内容」：仅该栏图元的独立预览窗

    SheetFrameOptions opts_;
    double scaleNum_ = 1.0;         // 绘图比例前值（附图：第一个下拉）
    double scaleDen_ = 1.0;         // 绘图比例后值（附图：第二个下拉）
    bool   updating_ = false;       // 模板回填期间抑制控件联动
    QVector<SheetFrameTemplate> templates_;   // 已加载模板（与下拉框一一对应）

    // ---- 左列：模板 ----
    QComboBox*  cbTemplate_  = nullptr;
    QPushButton* btnTplSave_ = nullptr;
    QPushButton* btnTplManage_ = nullptr;
    // ---- 左列：图幅（A0~A4 / 加长图幅+下拉 / 自定义+长L宽W）----
    QButtonGroup*  grpFmt_   = nullptr;
    QRadioButton*  rbExt_    = nullptr;
    QComboBox*     cbExt_    = nullptr;
    QRadioButton*  rbCustom_ = nullptr;
    QDoubleSpinBox* sbCW_    = nullptr;   // 长 L
    QDoubleSpinBox* sbCH_    = nullptr;   // 宽 W
    // ---- 左列：布置方式 + 绘图比例 ----
    QRadioButton* rbLand_ = nullptr;
    QRadioButton* rbPort_ = nullptr;
    QComboBox*  cbScaleNum_ = nullptr;
    QComboBox*  cbScaleDen_ = nullptr;
    QPushButton* btnScaleCalc_ = nullptr;
    // ---- 左列：分区 ----
    QSpinBox* sbZoneX_ = nullptr;
    QSpinBox* sbZoneY_ = nullptr;
    QLabel*   lblZone_ = nullptr;
    // ---- 左列：底部选项组 ----
    QCheckBox* chkAutoDate_  = nullptr;
    QCheckBox* chkCenter_    = nullptr;
    QCheckBox* chkBinding_   = nullptr;
    QCheckBox* chkFramePos_  = nullptr;
    QSpinBox*  sbBindingW_   = nullptr;
    // ---- 右列：五栏（开关 + 下拉 + 预览）----
    QCheckBox* chkTitle_    = nullptr;  QComboBox* cbTitleStyle_ = nullptr;
    QPushButton* btnPrevTitle_ = nullptr;
    QCheckBox* chkBom_      = nullptr;  QComboBox* cbBomStyle_   = nullptr;
    QPushButton* btnPrevBom_ = nullptr;
    QCheckBox* chkAddBar_   = nullptr;  QComboBox* cbAddBar_     = nullptr;
    QPushButton* btnPrevAddBar_ = nullptr;
    QCheckBox* chkZoneLeft_ = nullptr;  // 是否显示左分区（附加栏行下子选项）
    QCheckBox* chkCodeBar_  = nullptr;  QComboBox* cbCodeBar_    = nullptr;
    QPushButton* btnPrevCodeBar_ = nullptr;
    QCheckBox* chkParamBar_ = nullptr;  QComboBox* cbParamBar_   = nullptr;
    QPushButton* btnPrevParamBar_ = nullptr;
    // ---- 右列：比例调整设置 ----
    QCheckBox* chkDimAdj_     = nullptr;
    QCheckBox* chkTextAdj_    = nullptr;
    QCheckBox* chkTextInitH_  = nullptr;  QDoubleSpinBox* sbTextInitH_ = nullptr;
    QCheckBox* chkLwAdj_      = nullptr;
    QCheckBox* chkLwInit_     = nullptr;  QDoubleSpinBox* sbLwInit_    = nullptr;
    // ---- 右列：实时预览 ----
    QLabel* previewLabel_ = nullptr;
};

} // namespace SketchCAD
