#include "DiagnosticsDialog.h"

#include "core/DiagBundle.h"
#include "core/Diagnostics.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace SketchCAD {

DiagnosticsDialog::DiagnosticsDialog(const QVariantMap& drawingMeta, QWidget* parent)
    : QDialog(parent), meta_(drawingMeta) {
    setWindowTitle(tr("诊断包"));
    setModal(true);
    resize(520, 420);

    auto* lay = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("诊断包帮助开发者复现问题。包内包含：\n"
           "  · 最近日志（SketchCAD.log 及其滚动备份）\n"
           "  · 崩溃转储（如存在）\n"
           "  · 版本 / 操作系统 / 显卡等环境信息\n"
           "  · 当前图纸元信息（仅图元/图层/块计数，不含几何与文字内容）"),
        this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    lay->addWidget(new QLabel(tr("将打包的文件："), this));
    fileList_ = new QListWidget(this);
    for (const QString& p : DiagBundle::collectSourceFiles())
        fileList_->addItem(QFileInfo(p).fileName());
    fileList_->addItem(QStringLiteral("system-info.txt"));
    fileList_->addItem(QStringLiteral("drawing-meta.json"));
    lay->addWidget(fileList_, 1);

    auto* note = new QLabel(
        tr("诊断包只生成为本机 ZIP 文件（用户目录），本程序不会自动上传任何数据。"), this);
    note->setStyleSheet(QStringLiteral("color: gray;"));
    note->setWordWrap(true);
    lay->addWidget(note);

    auto* btnLay = new QHBoxLayout();
    auto* btnExport = new QPushButton(tr("导出诊断包..."), this);
    btnExport->setDefault(true);
    auto* btnLogs = new QPushButton(tr("打开日志目录"), this);
    auto* btnClose = new QPushButton(tr("关闭"), this);
    btnLay->addWidget(btnExport);
    btnLay->addWidget(btnLogs);
    btnLay->addStretch(1);
    btnLay->addWidget(btnClose);
    lay->addLayout(btnLay);

    connect(btnExport, &QPushButton::clicked, this, &DiagnosticsDialog::doExport);
    connect(btnLogs,   &QPushButton::clicked, this, &DiagnosticsDialog::openLogDir);
    connect(btnClose,  &QPushButton::clicked, this, &QDialog::accept);
}

void DiagnosticsDialog::doExport() {
    QString err;
    const QString zipPath = DiagBundle::exportBundle(meta_, QString(), &err);
    if (zipPath.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"),
                             tr("诊断包导出失败：\n%1").arg(err));
        return;
    }
    QMessageBox::information(this, tr("导出完成"),
        tr("诊断包已导出到：\n%1\n\n请把该文件发送给技术支持以协助定位问题。").arg(zipPath));
}

void DiagnosticsDialog::openLogDir() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Diagnostics::logDir()));
}

} // namespace SketchCAD
