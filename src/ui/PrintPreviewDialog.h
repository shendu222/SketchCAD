#pragma once
#include <QDialog>
#include "core/SketchModel.h"
#include "ui/PrintEngine.h"

class QPrinter;
class QPrintPreviewWidget;

namespace SketchCAD {

// 打印预览对话框：基于 QPrintPreviewWidget 的可视化预览，
// 并附带「打印到打印机」「导出 PDF(QPdfWriter)」入口。
class PrintPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrintPreviewDialog(SketchModel* model, const PrintSettings& settings,
                               QWidget* parent = nullptr);
    ~PrintPreviewDialog();

private slots:
    void onPaintRequested(QPrinter* printer);
    void onExportPdf();

private:
    SketchModel* model_;
    PrintSettings s_;
    QPrinter* printer_;
    QPrintPreviewWidget* preview_;
};

} // namespace SketchCAD
