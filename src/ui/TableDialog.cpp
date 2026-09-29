#include "TableDialog.h"
#include "core/Element.h"
#include "core/ElementFactory.h"
#include "core/SketchModel.h"
#include <QTableWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>

namespace SketchCAD {

TableDialog::TableDialog(SketchModel* model, QWidget* parent, const Element* initial)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("表格编辑器 (TABLE)"));
    resize(560, 480);

    grid_ = new QTableWidget(this);
    grid_->horizontalHeader()->setDefaultSectionSize(90);
    grid_->verticalHeader()->setDefaultSectionSize(26);

    sbRows_ = new QSpinBox(this);
    sbCols_ = new QSpinBox(this);
    sbRows_->setRange(1, 200); sbRows_->setValue(3);
    sbCols_->setRange(1, 200); sbCols_->setValue(4);

    sbRowHeight_ = new QDoubleSpinBox(this);
    sbRowHeight_->setRange(1.0, 200.0); sbRowHeight_->setValue(8.0);
    sbRowHeight_->setSuffix(QStringLiteral(" mm")); sbRowHeight_->setDecimals(1);

    sbTextHeight_ = new QDoubleSpinBox(this);
    sbTextHeight_->setRange(0.5, 50.0); sbTextHeight_->setValue(3.5);
    sbTextHeight_->setSuffix(QStringLiteral(" mm")); sbTextHeight_->setDecimals(2);

    cbAlign_ = new QComboBox(this);
    cbAlign_->addItem(QStringLiteral("左对齐"), 0);
    cbAlign_->addItem(QStringLiteral("居中"),   1);
    cbAlign_->addItem(QStringLiteral("右对齐"), 2);
    cbAlign_->setCurrentIndex(1);

    edColWidths_ = new QLineEdit(this);
    edColWidths_->setToolTip(QStringLiteral("各列宽度(mm)，逗号分隔；数量少于列数时余下列用 30mm"));
    edColWidths_->setText(QStringLiteral("30, 30, 30, 30"));

    connect(sbRows_, QOverload<int>::of(&QSpinBox::valueChanged), this, &TableDialog::rebuildGrid);
    connect(sbCols_, QOverload<int>::of(&QSpinBox::valueChanged), this, &TableDialog::rebuildGrid);

    // ---- 参数区（左）+ 网格（右/下）----
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("行数："), sbRows_);
    form->addRow(QStringLiteral("列数："), sbCols_);
    form->addRow(QStringLiteral("行高："), sbRowHeight_);
    form->addRow(QStringLiteral("字高："), sbTextHeight_);
    form->addRow(QStringLiteral("对齐："), cbAlign_);
    form->addRow(QStringLiteral("列宽："), edColWidths_);

    auto* top = new QHBoxLayout;
    top->addLayout(form, 1);
    auto* tip = new QLabel(
        QStringLiteral("单元格支持 %<DATE>% / %<FILENAME>% 等字段占位，\n"
                       "渲染与打印时自动求值。\n\n"
                       "表格可整体选中/移动，\n双击表格重新打开本编辑器。"), this);
    tip->setWordWrap(true);
    top->addWidget(tip, 1);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->addLayout(top);
    v->addWidget(grid_, 1);
    v->addWidget(btns);

    rebuildGrid();
    if (initial) loadFrom(*initial);
}

void TableDialog::rebuildGrid() {
    const int R = sbRows_->value(), C = sbCols_->value();
    // 先搬旧内容，避免重建丢字
    const int oldR = grid_->rowCount(), oldC = grid_->columnCount();
    QVector<QVector<QString>> keep(std::max(oldR, 0));
    for (int r = 0; r < oldR; ++r) {
        keep[r].resize(std::max(oldC, 0));
        for (int c = 0; c < oldC; ++c)
            if (grid_->item(r, c)) keep[r][c] = grid_->item(r, c)->text();
    }
    grid_->clearContents();
    grid_->setRowCount(R);
    grid_->setColumnCount(C);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
            if (r < oldR && c < oldC && !keep[r][c].isEmpty())
                grid_->setItem(r, c, new QTableWidgetItem(keep[r][c]));
}

void TableDialog::loadFrom(const Element& e) {
    sbRows_->setValue(std::clamp(e.tableRows, 1, 200));
    sbCols_->setValue(std::clamp(e.tableCols, 1, 200));
    sbRowHeight_->setValue(e.rowHeight > 0.5 ? e.rowHeight : 8.0);
    sbTextHeight_->setValue(e.radius > 0.1 ? e.radius : 3.5);
    const int ai = cbAlign_->findData(std::clamp(e.textAlign, 0, 2));
    if (ai >= 0) cbAlign_->setCurrentIndex(ai);
    QStringList cw;
    for (int c = 0; c < e.tableCols; ++c)
        cw << QString::number(e.tableColWidth(c), 'f', 1);
    edColWidths_->setText(cw.join(QStringLiteral(", ")));
    rebuildGrid();
    for (int r = 0; r < e.tableRows && r < grid_->rowCount(); ++r)
        for (int c = 0; c < e.tableCols && c < grid_->columnCount(); ++c)
            grid_->setItem(r, c, new QTableWidgetItem(e.tableCell(r, c)));
}

Element TableDialog::buildElement(const Vec3& topLeft) const {
    // 经工厂创建：套用模型默认样式（颜色/线型/线宽/当前图层）
    Element e = ElementFactory::createTable(topLeft, sbRows_->value(), sbCols_->value(), model_);
    e.rowHeight = sbRowHeight_->value();
    e.radius    = sbTextHeight_->value();          // 单元格文字字高(mm)
    e.textAlign = cbAlign_->currentData().toInt(); // 0=左 1=中 2=右
    // 列宽：逗号分隔；缺项回落 30mm，多余截断
    e.colWidths.clear();
    const QStringList parts = edColWidths_->text().split(',', Qt::SkipEmptyParts);
    for (const QString& s : parts)
        e.colWidths.push_back(std::max(1.0, s.trimmed().toDouble()));
    while ((int)e.colWidths.size() < e.tableCols) e.colWidths.push_back(30.0);
    e.colWidths.resize(e.tableCols);
    // 单元格文本（行主序）
    e.tableCells.assign(size_t(e.tableRows) * size_t(e.tableCols), QString());
    for (int r = 0; r < e.tableRows && r < grid_->rowCount(); ++r)
        for (int c = 0; c < e.tableCols && c < grid_->columnCount(); ++c)
            if (grid_->item(r, c))
                e.tableCells[size_t(r) * size_t(e.tableCols) + size_t(c)] = grid_->item(r, c)->text();
    return e;
}

} // namespace SketchCAD
