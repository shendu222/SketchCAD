#include "PrintPreviewDialog.h"
#include "PrintEngine.h"
#include <QPrinter>
#include <QPrintPreviewWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

namespace SketchCAD {

PrintPreviewDialog::PrintPreviewDialog(SketchModel* model, const PrintSettings& settings,
                                       QWidget* parent)
    : QDialog(parent), model_(model), s_(settings) {
    setWindowTitle(QStringLiteral("打印预览"));
    resize(900, 700);

    printer_ = new QPrinter(QPrinter::HighResolution);
    printer_->setResolution(150);   // 预览用适中分辨率，兼顾清晰度与响应速度

    preview_ = new QPrintPreviewWidget(printer_, this);
    preview_->setZoomFactor(1.0);

    auto* toolbar = new QToolBar(this);
    auto* zoomIn = toolbar->addAction(QStringLiteral("放大"));
    auto* zoomOut = toolbar->addAction(QStringLiteral("缩小"));
    auto* fit = toolbar->addAction(QStringLiteral("适应窗口"));
    toolbar->addSeparator();
    auto* printBtn = toolbar->addAction(QStringLiteral("打印..."));
    auto* pdfBtn = toolbar->addAction(QStringLiteral("导出 PDF..."));
    toolbar->addSeparator();
    auto* closeBtn = toolbar->addAction(QStringLiteral("关闭"));

    connect(zoomIn, &QAction::triggered, preview_, &QPrintPreviewWidget::zoomIn);
    connect(zoomOut, &QAction::triggered, preview_, &QPrintPreviewWidget::zoomOut);
    connect(fit, &QAction::triggered, preview_, &QPrintPreviewWidget::fitToWidth);
    connect(printBtn, &QAction::triggered, preview_, &QPrintPreviewWidget::print);
    connect(pdfBtn, &QAction::triggered, this, &PrintPreviewDialog::onExportPdf);
    connect(closeBtn, &QAction::triggered, this, &QDialog::accept);

    connect(preview_, &QPrintPreviewWidget::paintRequested, this, &PrintPreviewDialog::onPaintRequested);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(toolbar);
    lay->addWidget(preview_);
}

PrintPreviewDialog::~PrintPreviewDialog() {
    delete printer_;   // preview_ 不拥有 printer_
}

void PrintPreviewDialog::onPaintRequested(QPrinter* printer) {
    // 预览部件会把其内部 printer 传回；统一用对话框持有的 settings 出图
    PrintEngine::print(model_, s_, printer);
}

void PrintPreviewDialog::onExportPdf() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PDF"),
                                                     QStringLiteral("SketchCAD.pdf"),
                                                     QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    if (PrintEngine::exportPdf(model_, s_, path))
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                QStringLiteral("已生成 PDF：%1").arg(path));
    else
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("生成 PDF 时出错。"));
}

} // namespace SketchCAD
