#pragma once
#include <QDialog>
#include "core/Cleanup.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace SketchCAD {

class SketchModel;

// 图形清理（PURGE + OVERKILL）对话框。
// 提供「垃圾图元 / 隐藏图元 / 冗余块 / 空图层 / 重复图元」五类清理选项与几何容差，
// 支持「预览统计」（只算不改）与「执行清理」，并以执行前后图元数量对比呈现结果。
class CleanupDialog : public QDialog {
    Q_OBJECT
public:
    explicit CleanupDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void onPreview();   // 只统计、不落盘
    void onRun();       // 实际执行

private:
    cleanup::CleanupOptions options() const;
    void showResult(const cleanup::CleanupStats& st, bool dryRun);

    SketchModel* model_ = nullptr;

    QCheckBox*      cbGarbage_  = nullptr;
    QCheckBox*      cbInvisible_= nullptr;
    QCheckBox*      cbBlocks_   = nullptr;
    QCheckBox*      cbLayers_   = nullptr;
    QCheckBox*      cbOverkill_ = nullptr;
    QDoubleSpinBox* tolSpin_    = nullptr;

    QLabel*       resultLabel_ = nullptr;
    QListWidget*  detailList_  = nullptr;
    QPushButton*  btnPreview_  = nullptr;
    QPushButton*  btnRun_      = nullptr;
};

} // namespace SketchCAD
