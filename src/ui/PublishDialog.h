#pragma once
#include <QDialog>
#include <QListWidget>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include "core/SketchModel.h"
#include "ui/PrintEngine.h"

class QPrinter;

namespace SketchCAD {

// ============================================================================
// 发布对话框（PUBLISH）：勾选布局 → 批量出图
// ----------------------------------------------------------------------------
// 页列表 = 勾选条目（模型空间页 = currentPrintSettings 视口清空；布局页 = 布局 settings）。
// 三种目标：
//   1) 单个多页 PDF —— PrintEngine::exportPagesPdf（与 exportPdfLayouts 同一渲染裁决）
//   2) 每布局一个 PDF —— 逐页 exportPdf，文件名 <基名>_<页名>.pdf
//   3) 物理打印机 —— printPages（每页独立页面设置，页间 newPage）
// 页面设置覆盖：可选统一覆盖纸张/方向（仅替换 pageSize/orientation，其余沿用各页设置）。
// 与 COM BatchPlotEngine 的发布路径同源（空 settings + 有布局表 → exportPdfLayouts），
// 同一图纸集页数一致。
// ============================================================================
class PublishDialog : public QDialog {
    Q_OBJECT
public:
    explicit PublishDialog(SketchModel* model, const PrintSettings& modelSpaceSettings,
                           QWidget* parent = nullptr);

private slots:
    void onRun();

private:
    // 由勾选状态构建页列表（模型空间条目在前，布局条目按表序）
    QList<PrintEngine::PublishPage> buildPages() const;

    SketchModel* model_;
    PrintSettings modelSpaceSettings_;   // 模型空间页使用的页面设置（视口清空后使用）

    QListWidget* pageList_;              // 图纸集列表（模型空间 + 各布局，带勾选框）
    QRadioButton* singlePdfBtn_;         // 目标：单个多页 PDF
    QRadioButton* multiPdfBtn_;          // 目标：每布局一个 PDF
    QRadioButton* printerBtn_;           // 目标：物理打印机
    QLineEdit* pathEdit_;                // PDF 路径（单 PDF = 完整路径；多 PDF = 基名）
    QPushButton* browseBtn_;
    QCheckBox* overrideBox_;             // 页面设置覆盖
    QComboBox* overridePaper_;           // 覆盖纸张
    QRadioButton* overridePortrait_;     // 覆盖方向：纵向
    QRadioButton* overrideLandscape_;    // 覆盖方向：横向
};

} // namespace SketchCAD
