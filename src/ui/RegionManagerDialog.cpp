#include "RegionManagerDialog.h"
#include "core/SketchModel.h"
#include "core/Section.h"
#include "core/Modify.h"
#include "core/Element.h"

#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSet>

namespace SketchCAD {

RegionManagerDialog::RegionManagerDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("边界区域管理 (REGION)"));
    setMinimumWidth(420);

    auto* top = new QLabel(QStringLiteral(
        "区域 = 共享同一名称的若干「边界」图元。\n"
        "画好外框与内孔得到若干边界后，归为同一区域即可一次填充（无需逐段描边）。"));
    top->setWordWrap(true);

    list_ = new QListWidget(this);
    info_ = new QLabel(QStringLiteral("图面暂无边界图元。请先用「边界(BOUNDARY)」工具绘制。"));
    info_->setWordWrap(true);

    btnNew_    = new QPushButton(QStringLiteral("新建区域..."), this);
    btnFill_   = new QPushButton(QStringLiteral("填充区域"), this);
    btnSelect_ = new QPushButton(QStringLiteral("全选区域"), this);
    btnRename_ = new QPushButton(QStringLiteral("重命名"), this);
    btnDelete_ = new QPushButton(QStringLiteral("删除区域"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnNew_); btnRow->addWidget(btnFill_);
    btnRow->addWidget(btnSelect_); btnRow->addWidget(btnRename_); btnRow->addWidget(btnDelete_);

    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    closeRow->addWidget(closeBtn);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(top);
    lay->addWidget(list_, 1);
    lay->addWidget(info_);
    lay->addLayout(btnRow);
    lay->addLayout(closeRow);

    connect(btnNew_,    &QPushButton::clicked, this, &RegionManagerDialog::onNewRegion);
    connect(btnFill_,   &QPushButton::clicked, this, &RegionManagerDialog::onFillRegion);
    connect(btnSelect_, &QPushButton::clicked, this, &RegionManagerDialog::onSelectRegion);
    connect(btnRename_, &QPushButton::clicked, this, &RegionManagerDialog::onRenameRegion);
    connect(btnDelete_, &QPushButton::clicked, this, &RegionManagerDialog::onDeleteRegion);
    connect(closeBtn,   &QPushButton::clicked, this, &QDialog::accept);
    connect(list_, &QListWidget::itemSelectionChanged, this, [this] {
        const bool has = !list_->selectedItems().isEmpty();
        btnFill_->setEnabled(has); btnSelect_->setEnabled(has);
        btnRename_->setEnabled(has); btnDelete_->setEnabled(has);
    });

    refresh();
}

std::vector<int> RegionManagerDialog::memberIds(const QString& name) const {
    std::vector<int> ids;
    for (const auto& e : model_->elements())
        if (e.type == ElementType::Boundary && e.name == name)
            ids.push_back(e.id);
    return ids;
}

QString RegionManagerDialog::currentRegionName() const {
    QListWidgetItem* it = list_->currentItem();
    if (!it) return QString();
    return it->data(Qt::UserRole).toString();
}

void RegionManagerDialog::refresh() {
    list_->clear();
    QSet<QString> names;
    int total = 0;
    for (const auto& e : model_->elements())
        if (e.type == ElementType::Boundary) { names.insert(e.name); ++total; }

    if (total == 0) {
        info_->setText(QStringLiteral("图面暂无边界图元。请先用「边界(BOUNDARY)」工具绘制外框与内孔。"));
        btnFill_->setEnabled(false); btnSelect_->setEnabled(false);
        btnRename_->setEnabled(false); btnDelete_->setEnabled(false);
        return;
    }

    QStringList sorted = names.values();
    sorted.sort();
    for (const QString& nm : sorted) {
        const int n = (int)memberIds(nm).size();
        auto* it = new QListWidgetItem(
            nm.isEmpty() ? QStringLiteral("未命名边界（%1 个）").arg(n)
                         : QStringLiteral("%1（%2 个边界）").arg(nm).arg(n));
        it->setData(Qt::UserRole, nm);
        list_->addItem(it);
    }
    info_->setText(QStringLiteral("共 %1 个边界，分 %2 个区域。选中区域后可填充 / 全选 / 重命名 / 删除。")
                       .arg(total).arg(names.size()));
    if (!list_->selectedItems().isEmpty()) list_->setCurrentItem(list_->item(0));
}

void RegionManagerDialog::onNewRegion() {
    // 取当前选中的 Boundary（无选中则提示）
    std::vector<Element> sel;
    for (int id : model_->selection())
        if (const Element* e = model_->element(id))
            if (e->type == ElementType::Boundary) sel.push_back(*e);
    if (sel.empty()) {
        QMessageBox::information(this, QStringLiteral("新建区域"),
            QStringLiteral("请先在图面选中若干「边界」图元（用边界工具绘制的），再归为同一区域。"));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建区域"),
        QStringLiteral("为选中的 %1 个边界指定区域名称：").arg(sel.size()),
        QLineEdit::Normal, QStringLiteral("区域1"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString nm = name.trimmed();
    model_->beginEdit(QStringLiteral("新建边界区域"));
    for (auto& e : sel) model_->updateElement(e.id, [&](Element& x) { x.name = nm; });
    model_->log(QStringLiteral("已把 %1 个边界归入区域「%2」").arg(sel.size()).arg(nm));
    refresh();
}

void RegionManagerDialog::onFillRegion() {
    const QString nm = currentRegionName();
    if (nm.isNull()) return;
    std::vector<Element> members;
    for (int id : memberIds(nm))
        if (const Element* e = model_->element(id)) members.push_back(*e);
    if (members.empty()) return;

    // 直接调用剖面填充：Boundary 经 makeSectionChangesEx 的专用分支复用其外环/内孔/样式，
    // 跳过自动边界追踪。每个 Boundary 复用自身 id 替换为 Hatch（单次撤销、选中态不跳）。
    const auto res = makeSectionChangesEx(members, SectionStyle{}, true);
    if (res.changes.empty()) {
        model_->log(QStringLiteral("区域「%1」无可填充的边界").arg(nm), "warn");
        return;
    }
    model_->beginEdit(QStringLiteral("剖面填充（区域）"));
    const std::vector<int> newIds = modify::applyChanges(model_, res.changes, QString());
    int picked = -1;
    for (const auto& c : res.changes) if (!c.remove) { picked = (c.id > 0 ? c.id : -1); break; }
    if (picked > 0) model_->setSelection({ picked });
    else if (!newIds.empty()) model_->setSelection({ newIds.front() });
    model_->log(QStringLiteral("区域「%1」已填充 %2 个边界").arg(nm).arg(res.filledRegions));
}

void RegionManagerDialog::onSelectRegion() {
    const QString nm = currentRegionName();
    if (nm.isNull()) return;
    const auto ids = memberIds(nm);
    if (ids.empty()) return;
    model_->setSelection(ids);
    model_->log(QStringLiteral("已选中区域「%1」的 %2 个边界").arg(nm).arg((int)ids.size()));
}

void RegionManagerDialog::onRenameRegion() {
    const QString nm = currentRegionName();
    if (nm.isNull()) return;
    const auto ids = memberIds(nm);
    if (ids.empty()) return;
    bool ok = false;
    const QString newName = QInputDialog::getText(this, QStringLiteral("重命名区域"),
        QStringLiteral("将区域重命名为："), QLineEdit::Normal, nm, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    const QString nn = newName.trimmed();
    model_->beginEdit(QStringLiteral("重命名边界区域"));
    for (int id : ids) model_->updateElement(id, [&](Element& x) { x.name = nn; });
    model_->log(QStringLiteral("区域「%1」已重命名为「%2」").arg(nm).arg(nn));
    refresh();
}

void RegionManagerDialog::onDeleteRegion() {
    const QString nm = currentRegionName();
    if (nm.isNull()) return;
    const auto ids = memberIds(nm);
    if (ids.empty()) return;
    if (QMessageBox::question(this, QStringLiteral("删除区域"),
            QStringLiteral("确认删除区域「%1」的 %2 个边界图元？").arg(nm).arg((int)ids.size()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    std::vector<Element> src;
    for (int id : ids) if (const Element* e = model_->element(id)) src.push_back(*e);
    auto ch = modify::makeDelete(ids);
    model_->beginEdit(QStringLiteral("删除边界区域"));
    modify::applyChanges(model_, ch, QString());
    model_->log(QStringLiteral("已删除区域「%1」的 %2 个边界").arg(nm).arg((int)ids.size()));
    refresh();
}

} // namespace SketchCAD
