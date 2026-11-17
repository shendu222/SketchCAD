#include "core/I18n.h"
#include "DbDrawingsDialog.h"
#include "core/DrawingDb.h"

#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>

namespace SketchCAD {

namespace {
    // 构造一个不可编辑的单元格
    QTableWidgetItem* mkItem(const QString& text) {
        auto* it = new QTableWidgetItem(text);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        return it;
    }
}

DbDrawingsDialog::DbDrawingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(SketchCAD::I18n::t(QString::fromUtf8("数据库绘图记录")));
    resize(760, 480);

    auto* root = new QVBoxLayout(this);

    tabs_ = new QTabWidget(this);
    root->addWidget(tabs_);

    buildDrawingsTable();
    buildTemplatesTable();

    // ---- 底部操作行 ----
    auto* btnRow = new QHBoxLayout();
    auto* btnSaveCur = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("保存当前图纸到数据库...")), this);
    auto* btnSaveTpl = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("另存当前图纸为模板...")), this);
    auto* btnRefresh = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("刷新")), this);
    auto* btnClose   = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("关闭")), this);
    btnRow->addWidget(btnSaveCur);
    btnRow->addWidget(btnSaveTpl);
    btnRow->addStretch(1);
    btnRow->addWidget(btnRefresh);
    btnRow->addWidget(btnClose);
    root->addLayout(btnRow);

    connect(btnSaveCur, &QPushButton::clicked, this, &DbDrawingsDialog::onSaveCurrent);
    connect(btnSaveTpl, &QPushButton::clicked, this, &DbDrawingsDialog::onSaveTemplate);
    connect(btnRefresh, &QPushButton::clicked, this, &DbDrawingsDialog::refresh);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void DbDrawingsDialog::buildDrawingsTable() {
    drawingsTable_ = new QTableWidget(0, 5, this);
    drawingsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    drawingsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    drawingsTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), SketchCAD::I18n::t(QString::fromUtf8("名称")),
        SketchCAD::I18n::t(QString::fromUtf8("创建时间")), SketchCAD::I18n::t(QString::fromUtf8("更新时间")),
        SketchCAD::I18n::t(QString::fromUtf8("操作")) });
    drawingsTable_->horizontalHeader()->setStretchLastSection(true);
    tabs_->addTab(drawingsTable_, SketchCAD::I18n::t(QString::fromUtf8("图档")));
    rebuildDrawings();
}

void DbDrawingsDialog::buildTemplatesTable() {
    templatesTable_ = new QTableWidget(0, 5, this);
    templatesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    templatesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    templatesTable_->setHorizontalHeaderLabels({
        QStringLiteral("ID"), SketchCAD::I18n::t(QString::fromUtf8("模板名称")),
        SketchCAD::I18n::t(QString::fromUtf8("参考尺寸 (mm)")), SketchCAD::I18n::t(QString::fromUtf8("创建时间")),
        SketchCAD::I18n::t(QString::fromUtf8("操作")) });
    templatesTable_->horizontalHeader()->setStretchLastSection(true);
    tabs_->addTab(templatesTable_, SketchCAD::I18n::t(QString::fromUtf8("模板")));
    rebuildTemplates();
}

void DbDrawingsDialog::refresh() {
    rebuildDrawings();
    rebuildTemplates();
}

void DbDrawingsDialog::rebuildDrawings() {
    drawingsTable_->setRowCount(0);
    auto* db = DrawingDb::instance();
    if (!db->isConnected()) {
        drawingsTable_->setRowCount(1);
        drawingsTable_->setItem(0, 1, mkItem(QStringLiteral("（数据库未连接，仅使用文件存储）")));
        return;
    }
    QList<DrawingDb::DrawingRecord> recs = db->listDrawings();
    drawingsTable_->setRowCount(recs.size());
    for (int r = 0; r < recs.size(); ++r) {
        const auto& rec = recs[r];
        auto* idItem = mkItem(QString::number(rec.id));
        idItem->setData(Qt::UserRole, rec.id);
        drawingsTable_->setItem(r, 0, idItem);
        drawingsTable_->setItem(r, 1, mkItem(rec.name));
        drawingsTable_->setItem(r, 2, mkItem(rec.createdAt));
        drawingsTable_->setItem(r, 3, mkItem(rec.updatedAt));

        int id = rec.id;
        auto* op = new QWidget(drawingsTable_);
        auto* opLay = new QHBoxLayout(op);
        opLay->setContentsMargins(4, 2, 4, 2);
        opLay->setSpacing(4);
        auto* btnOpen = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("打开")));
        auto* btnDel  = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("删除")));
        opLay->addWidget(btnOpen);
        opLay->addWidget(btnDel);
        drawingsTable_->setCellWidget(r, 4, op);

        connect(btnOpen, &QPushButton::clicked, this,
                [this, id]() { emit openDrawingRequested(id); });
        connect(btnDel, &QPushButton::clicked, this, [this, id]() {
            if (QMessageBox::question(this, SketchCAD::I18n::t(QString::fromUtf8("删除确认")),
                    SketchCAD::I18n::t(QString::fromUtf8("确定从数据库中删除该图档？此操作不可撤销。")),
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;
            if (DrawingDb::instance()->deleteDrawing(id))
                rebuildDrawings();
            else
                QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("删除失败")),
                    SketchCAD::I18n::t(QString::fromUtf8("删除图档失败：")) + DrawingDb::instance()->lastError());
        });
    }
    drawingsTable_->resizeColumnsToContents();
    if (drawingsTable_->columnCount() > 0)
        drawingsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void DbDrawingsDialog::rebuildTemplates() {
    templatesTable_->setRowCount(0);
    auto* db = DrawingDb::instance();
    if (!db->isConnected()) {
        templatesTable_->setRowCount(1);
        templatesTable_->setItem(0, 1, mkItem(QStringLiteral("（数据库未连接，仅使用文件存储）")));
        return;
    }
    QList<DrawingDb::TemplateRecord> recs = db->listTemplates();
    templatesTable_->setRowCount(recs.size());
    for (int r = 0; r < recs.size(); ++r) {
        const auto& rec = recs[r];
        auto* idItem = mkItem(QString::number(rec.id));
        idItem->setData(Qt::UserRole, rec.id);
        templatesTable_->setItem(r, 0, idItem);
        templatesTable_->setItem(r, 1, mkItem(rec.name));
        templatesTable_->setItem(r, 2,
            mkItem(QStringLiteral("%1 × %2").arg(rec.refWidth, 0, 'f', 1).arg(rec.refHeight, 0, 'f', 1)));
        templatesTable_->setItem(r, 3, mkItem(rec.createdAt));

        int id = rec.id;
        auto* op = new QWidget(templatesTable_);
        auto* opLay = new QHBoxLayout(op);
        opLay->setContentsMargins(4, 2, 4, 2);
        opLay->setSpacing(4);
        auto* btnApply = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("应用")));
        auto* btnDel   = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("删除")));
        opLay->addWidget(btnApply);
        opLay->addWidget(btnDel);
        templatesTable_->setCellWidget(r, 4, op);

        connect(btnApply, &QPushButton::clicked, this,
                [this, id]() { emit applyTemplateRequested(id); });
        connect(btnDel, &QPushButton::clicked, this, [this, id]() {
            if (QMessageBox::question(this, SketchCAD::I18n::t(QString::fromUtf8("删除确认")),
                    SketchCAD::I18n::t(QString::fromUtf8("确定从数据库中删除该模板？此操作不可撤销。")),
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;
            if (DrawingDb::instance()->deleteTemplate(id))
                rebuildTemplates();
            else
                QMessageBox::warning(this, SketchCAD::I18n::t(QString::fromUtf8("删除失败")),
                    SketchCAD::I18n::t(QString::fromUtf8("删除模板失败：")) + DrawingDb::instance()->lastError());
        });
    }
    templatesTable_->resizeColumnsToContents();
    if (templatesTable_->columnCount() > 0)
        templatesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void DbDrawingsDialog::onSaveCurrent() {
    bool ok = false;
    QString name = QInputDialog::getText(this, SketchCAD::I18n::t(QString::fromUtf8("保存到数据库")),
        SketchCAD::I18n::t(QString::fromUtf8("输入图纸名称：")), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty())
        emit saveCurrentRequested(name.trimmed());
}

void DbDrawingsDialog::onSaveTemplate() {
    bool ok = false;
    QString name = QInputDialog::getText(this, SketchCAD::I18n::t(QString::fromUtf8("另存为模板")),
        SketchCAD::I18n::t(QString::fromUtf8("输入模板名称（将记录当前图纸尺寸作为参考尺寸）：")),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty())
        emit saveTemplateRequested(name.trimmed());
}

} // namespace SketchCAD
