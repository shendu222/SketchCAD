#pragma once
#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include "core/SketchModel.h"
#include "ui/PrintEngine.h"

namespace SketchCAD {

// 打印设置对话框：页面设置 + 简化 CTB 样式映射编辑 + 视口/布局(MVP) + 预览/导出入口
class PrintSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrintSettingsDialog(SketchModel* model, const PrintSettings& initial,
                                QWidget* parent = nullptr);

    PrintSettings settings() const { return s_; }

private slots:
    void onPaperChanged(int idx);
    void onAddStyle();
    void onDeleteStyle();
    void onStyleSelected();
    void onAddViewport();
    void onDeleteViewport();
    void onEditViewport();
    void onPreview();
    void onExportPdf();
    void onAccept();

private:
    void syncFromSettings();
    void syncStyleList();
    void syncViewportList();
    void refreshPaperSizeCombo();

    SketchModel* model_;
    PrintSettings s_;

    // 页面设置
    QComboBox* paperCombo_;
    QRadioButton* portraitBtn_;
    QRadioButton* landscapeBtn_;
    QDoubleSpinBox* marginL_;
    QDoubleSpinBox* marginT_;
    QDoubleSpinBox* marginR_;
    QDoubleSpinBox* marginB_;
    QDoubleSpinBox* scaleSpin_;
    QCheckBox* fitBox_;
    QCheckBox* centerHBox_;
    QCheckBox* centerVBox_;
    QCheckBox* borderBox_;
    QDoubleSpinBox* lineWidthScale_;   // 打印线宽比例（默认 0.5，细一档）

    // 打印样式 CTB
    QComboBox* styleTargetType_;
    QLineEdit* styleTargetName_;
    QPushButton* styleColorPick_;
    QCheckBox* styleOverrideColor_;
    QCheckBox* styleOverrideWidth_;
    QDoubleSpinBox* styleWidth_;
    QPushButton* styleAddBtn_;
    QPushButton* styleDelBtn_;
    QListWidget* styleList_;

    // 视口/布局
    QListWidget* vpList_;
    QPushButton* vpAddBtn_;
    QPushButton* vpEditBtn_;
    QPushButton* vpDelBtn_;
};

} // namespace SketchCAD
