#include "core/I18n.h"
#include "PublishDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QFileInfo>
#include <QRegularExpression>

namespace SketchCAD {

PublishDialog::PublishDialog(SketchModel* model, const PrintSettings& modelSpaceSettings,
                             QWidget* parent)
    : QDialog(parent), model_(model), modelSpaceSettings_(modelSpaceSettings) {
    setWindowTitle(SketchCAD::I18n::t(QString::fromUtf8("发布 (PUBLISH)")));
    resize(520, 460);

    auto* lay = new QVBoxLayout(this);

    // ---- 图纸集列表（勾选要发布的页）----
    lay->addWidget(new QLabel(SketchCAD::I18n::t(QString::fromUtf8("选择要发布的图纸（按列表顺序出图）：")), this));
    pageList_ = new QListWidget(this);
    pageList_->setObjectName("publishPageList");
    // 模型空间条目：视口清空 = 模型空间打印语义
    {
        auto* it = new QListWidgetItem(SketchCAD::I18n::t(QString::fromUtf8("模型空间")), pageList_);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
        it->setData(Qt::UserRole, QStringLiteral("__model__"));
    }
    // 布局条目：名 + 视口数
    for (int i = 0; i < model_->layoutCount(); ++i) {
        const PlotLayoutDef* def = model_->layoutAt(i);
        if (!def) continue;
        auto* it = new QListWidgetItem(
            SketchCAD::I18n::t(QString::fromUtf8("%1（%2 个视口）")).arg(def->name).arg(def->settings.viewports.size()), pageList_);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
        it->setData(Qt::UserRole, def->name);
    }
    lay->addWidget(pageList_);

    // ---- 输出目标 ----
    auto* targetGrp = new QGroupBox(SketchCAD::I18n::t(QString::fromUtf8("输出目标")), this);
    auto* tLay = new QVBoxLayout(targetGrp);
    singlePdfBtn_ = new QRadioButton(SketchCAD::I18n::t(QString::fromUtf8("单个多页 PDF")), targetGrp);
    multiPdfBtn_  = new QRadioButton(SketchCAD::I18n::t(QString::fromUtf8("每布局一个 PDF")), targetGrp);
    printerBtn_   = new QRadioButton(SketchCAD::I18n::t(QString::fromUtf8("物理打印机")), targetGrp);
    singlePdfBtn_->setChecked(true);
    tLay->addWidget(singlePdfBtn_);
    tLay->addWidget(multiPdfBtn_);
    tLay->addWidget(printerBtn_);
    auto* pathRow = new QHBoxLayout;
    pathEdit_ = new QLineEdit(targetGrp);
    pathEdit_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("PDF 路径（多 PDF 模式 = 文件基名）")));
    browseBtn_ = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("浏览...")), targetGrp);
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseBtn_);
    tLay->addLayout(pathRow);
    lay->addWidget(targetGrp);

    // ---- 页面设置覆盖（可选）----
    auto* ovGrp = new QGroupBox(SketchCAD::I18n::t(QString::fromUtf8("页面设置覆盖（可选）")), this);
    auto* ovLay = new QHBoxLayout(ovGrp);
    overrideBox_ = new QCheckBox(SketchCAD::I18n::t(QString::fromUtf8("统一覆盖纸张/方向")), ovGrp);
    overridePaper_ = new QComboBox(ovGrp);
    const char* papers[] = { "A4", "A3", "A2", "A1", "A0", "B5", "B4", "Letter", "Legal" };
    const QPageSize::PageSizeId ids[] = {
        QPageSize::A4, QPageSize::A3, QPageSize::A2, QPageSize::A1, QPageSize::A0,
        QPageSize::B5, QPageSize::B4, QPageSize::Letter, QPageSize::Legal
    };
    for (int i = 0; i < 9; ++i) {
        overridePaper_->addItem(QString::fromUtf8(papers[i]), (int)ids[i]);
    }
    overridePortrait_  = new QRadioButton(SketchCAD::I18n::t(QString::fromUtf8("纵向")), ovGrp);
    overrideLandscape_ = new QRadioButton(SketchCAD::I18n::t(QString::fromUtf8("横向")), ovGrp);
    overridePortrait_->setChecked(true);
    ovLay->addWidget(overrideBox_);
    ovLay->addWidget(overridePaper_);
    ovLay->addWidget(overridePortrait_);
    ovLay->addWidget(overrideLandscape_);
    ovLay->addStretch(1);
    lay->addWidget(ovGrp);

    // ---- 运行 ----
    auto* runRow = new QHBoxLayout;
    runRow->addStretch(1);
    auto* runBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("发布")), this);
    runBtn->setDefault(true);
    auto* closeBtn = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("关闭")), this);
    runRow->addWidget(runBtn);
    runRow->addWidget(closeBtn);
    lay->addLayout(runRow);

    connect(browseBtn_, &QPushButton::clicked, this, [this] {
        if (multiPdfBtn_->isChecked()) {
            const QString base = QFileDialog::getSaveFileName(
                this, SketchCAD::I18n::t(QString::fromUtf8("选择输出基名")), QStringLiteral("SketchCAD.pdf"),
                SketchCAD::I18n::t(QString::fromUtf8("PDF 文件 (*.pdf)")));
            if (!base.isEmpty()) pathEdit_->setText(base);
        } else {
            const QString p = QFileDialog::getSaveFileName(
                this, SketchCAD::I18n::t(QString::fromUtf8("选择输出 PDF")), QStringLiteral("SketchCAD.pdf"),
                SketchCAD::I18n::t(QString::fromUtf8("PDF 文件 (*.pdf)")));
            if (!p.isEmpty()) pathEdit_->setText(p);
        }
    });
    connect(runBtn, &QPushButton::clicked, this, &PublishDialog::onRun);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QList<PrintEngine::PublishPage> PublishDialog::buildPages() const {
    QList<PrintEngine::PublishPage> pages;
    for (int i = 0; i < pageList_->count(); ++i) {
        const QListWidgetItem* it = pageList_->item(i);
        if (!it || it->checkState() != Qt::Checked) continue;
        const QString key = it->data(Qt::UserRole).toString();
        PrintEngine::PublishPage pg;
        if (key == QLatin1String("__model__")) {
            pg.name = QStringLiteral("模型空间");
            pg.settings = modelSpaceSettings_;
            pg.settings.viewports.clear();       // 视口空 = 模型空间打印
        } else {
            const PlotLayoutDef* def = model_->findLayout(key);
            if (!def) continue;
            pg.name = def->name;
            pg.settings = def->settings;
        }
        if (overrideBox_->isChecked()) {
            const QPageSize paper((QPageSize::PageSizeId)overridePaper_->currentData().toInt());
            pg.settings.pageSize = paper;
            pg.settings.orientation = overrideLandscape_->isChecked()
                                          ? QPageLayout::Landscape : QPageLayout::Portrait;
        }
        pages.append(pg);
    }
    return pages;
}

void PublishDialog::onRun() {
    const QList<PrintEngine::PublishPage> pages = buildPages();
    if (pages.isEmpty()) {
        QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("发布")),
                             SketchCAD::I18n::t(QString::fromUtf8("未勾选任何图纸。")));
        return;
    }

    // ---- 单个多页 PDF ----
    if (singlePdfBtn_->isChecked()) {
        QString path = pathEdit_->text().trimmed();
        if (path.isEmpty()) {
            QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("发布")),
                                 SketchCAD::I18n::t(QString::fromUtf8("请先指定输出 PDF 路径。")));
            return;
        }
        const int done = PrintEngine::exportPagesPdf(model_, pages, path);
        if (done > 0)
            model_->log(SketchCAD::I18n::t(QString::fromUtf8("发布完成：%1 页 → %2")).arg(done).arg(path), QStringLiteral("info"));
        else
            model_->log(SketchCAD::I18n::t(QString::fromUtf8("发布失败：未生成任何页")), QStringLiteral("warn"));
        QMessageBox::information(this, SketchCAD::I18n::t(QString::fromUtf8("发布")),
                                 done > 0 ? SketchCAD::I18n::t(QString::fromUtf8("已发布 %1 页 → %2")).arg(done).arg(path)
                                          : SketchCAD::I18n::t(QString::fromUtf8("发布失败：未生成任何页。")));
        return;
    }

    // ---- 每布局一个 PDF ----
    if (multiPdfBtn_->isChecked()) {
        QString base = pathEdit_->text().trimmed();
        if (base.isEmpty()) {
            QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("发布")),
                                 SketchCAD::I18n::t(QString::fromUtf8("请先指定输出文件基名。")));
            return;
        }
        // 去掉可能带上的扩展名，统一 <基名>_<页名>.pdf
        if (base.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
            base.chop(4);
        static const QRegularExpression unsafeRgx(QStringLiteral(R"([\\/:*?"<>|])"));
        int okCnt = 0;
        for (const PrintEngine::PublishPage& pg : pages) {
            QString safeName = pg.name;
            safeName.replace(unsafeRgx, QStringLiteral("_"));
            const QString path = QStringLiteral("%1_%2.pdf").arg(base, safeName);
            if (PrintEngine::exportPdf(model_, pg.settings, path)) ++okCnt;
        }
        model_->log(QStringLiteral("发布完成：%1/%2 个 PDF（每布局一档）").arg(okCnt).arg(pages.size()),
                    QStringLiteral("info"));
        QMessageBox::information(this, QStringLiteral("发布"),
                                 QStringLiteral("已生成 %1/%2 个 PDF。").arg(okCnt).arg(pages.size()));
        return;
    }

    // ---- 物理打印机 ----
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const int done = PrintEngine::printPages(model_, pages, &printer);
    model_->log(QStringLiteral("已发送至打印机：%1 页").arg(done), QStringLiteral("info"));
    QMessageBox::information(this, QStringLiteral("发布"),
                             QStringLiteral("已发送 %1 页至打印机。").arg(done));
}

} // namespace SketchCAD
