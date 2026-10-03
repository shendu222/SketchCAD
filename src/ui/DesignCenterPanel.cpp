#include "DesignCenterPanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "core/SketchModel.h"
#include "core/Block.h"
#include "core/CadFormats.h"
#include <algorithm>

namespace SketchCAD {

// 树节点 userData 角色：区分「当前图块表」条目与「外部文件块表」条目
static constexpr int kRoleKind = Qt::UserRole + 1;   // 0=当前图块表, 1=外部文件块表
static constexpr int kRoleName = Qt::UserRole + 2;   // 块名

DesignCenterPanel::DesignCenterPanel(SketchModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("<b>设计中心</b>（符号库）"));
    loadBtn_ = new QPushButton(QStringLiteral("加载外部块库..."));
    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(loadBtn_);

    tree_ = new QTreeWidget;
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({ QStringLiteral("名称"), QStringLiteral("图元数") });
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(true);

    info_ = new QLabel(QStringLiteral(
        "双击块 → 在视图中心插入。「当前图块表」随本图块表实时刷新；"
        "「外部文件块表」来自 SketchCAD 存档(.json) / DXF(.dxf)。"));
    info_->setWordWrap(true);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addLayout(top);
    lay->addWidget(tree_, 1);
    lay->addWidget(info_);

    connect(loadBtn_, &QPushButton::clicked, this, &DesignCenterPanel::onLoadExternal);
    connect(tree_, &QTreeWidget::itemActivated,   this, &DesignCenterPanel::onItemActivated);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &DesignCenterPanel::onItemDoubleClicked);

    // 本图块表变化（BEDIT 重定义 / 插入外部块 / 删除等）→ 树实时刷新
    if (model_) {
        connect(model_, &SketchModel::blocksChanged, this, &DesignCenterPanel::refresh);
    }
    refresh();
}

void DesignCenterPanel::refresh() {
    if (!tree_) return;
    // 保留展开状态与外部集合（重建会清空 tree_，故先摘下外部集合指针再重填）
    tree_->clear();

    // ---- 当前图块表 ----
    auto* cur = new QTreeWidgetItem(tree_, { QStringLiteral("当前图块表"), QString() });
    cur->setData(0, kRoleKind, 0);
    cur->setFirstColumnSpanned(true);
    if (model_) {
        for (const auto& d : model_->blockTable()) {
            auto* it = new QTreeWidgetItem(cur, { d.name, QString::number((int)d.entities.size()) });
            it->setData(0, kRoleKind, 0);
            it->setData(0, kRoleName, d.name);
            if (d.isXref) it->setToolTip(0, QStringLiteral("外部参照（Xref）"));
        }
    }
    cur->setExpanded(true);

    // ---- 外部文件块表（每个已加载文件一个分组）----
    for (int gi = 0; gi < (int)externals_.size(); ++gi) {
        const auto& es = externals_[gi];
        auto* grp = new QTreeWidgetItem(tree_, { es.label, QString() });
        grp->setData(0, kRoleKind, -1);   // 分组节点本身不可双击插入
        grp->setFirstColumnSpanned(true);
        for (const auto& d : es.defs) {
            auto* it = new QTreeWidgetItem(grp, { d.name, QString::number((int)d.entities.size()) });
            it->setData(0, kRoleKind, 1);
            it->setData(0, kRoleName, d.name);
        }
        grp->setExpanded(true);
    }
    // 重建 externalIndex_（块名 → 外部集合索引；同名以先加载者为准）
    externalIndex_.clear();
    for (int gi = 0; gi < (int)externals_.size(); ++gi)
        for (const auto& d : externals_[gi].defs)
            if (!externalIndex_.contains(d.name)) externalIndex_.insert(d.name, gi);
}

void DesignCenterPanel::onLoadExternal() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("加载外部块库"), QString(),
        QStringLiteral("块库文件 (*.json *.dxf);;SketchCAD 存档 (*.json);;DXF 图纸 (*.dxf);;所有文件 (*)"));
    if (path.isEmpty()) return;
    addExternalFile(path);
}

void DesignCenterPanel::addExternalFile(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    std::vector<BlockDef> defs;
    if (suffix == "json")      defs = cadfmt::blockTableFromJson(path);
    else if (suffix == "dxf")  defs = cadfmt::blockTableFromDxf(path);
    else {
        QMessageBox::warning(this, QStringLiteral("加载外部块库"),
                             QStringLiteral("仅支持 .json（SketchCAD 存档）与 .dxf 图纸的块表。"));
        return;
    }
    if (defs.empty()) {
        QMessageBox::information(this, QStringLiteral("加载外部块库"),
                                 QStringLiteral("未能从「%1」读取到任何块定义。").arg(QFileInfo(path).fileName()));
        return;
    }
    // 剔除匿名块（* 前缀，导入器已过滤，此处再兜一层）
    defs.erase(std::remove_if(defs.begin(), defs.end(),
                              [](const BlockDef& d){ return d.name.startsWith(QLatin1Char('*')); }),
               defs.end());
    const QString label = QStringLiteral("外部：%1（%2 块）")
                              .arg(QFileInfo(path).fileName()).arg((int)defs.size());
    externals_.push_back(ExternalSet{ label, std::move(defs) });
    refresh();
}

void DesignCenterPanel::onItemActivated(QTreeWidgetItem* item, int) {
    // 回车 = 视作双击插入
    onItemDoubleClicked(item, 0);
}

void DesignCenterPanel::onItemDoubleClicked(QTreeWidgetItem* item, int) {
    if (!item || !model_) return;
    const int kind = item->data(0, kRoleKind).toInt();
    if (kind != 0 && kind != 1) return;   // 分组节点
    const QString name = item->data(0, kRoleName).toString();
    if (name.isEmpty()) return;

    // 外部块：先并入本图块表（重名则跳过），再请求插入
    if (kind == 1) {
        if (!model_->hasBlock(name)) {
            auto it = externalIndex_.find(name);
            if (it != externalIndex_.end()) {
                for (const auto& d : externals_[it.value()].defs) {
                    if (d.name != name) continue;
                    model_->addBlockDef(d);   // 并入本图块表（可撤销）
                    break;
                }
            }
        }
    }
    emit insertBlockRequested(name);
}

} // namespace SketchCAD
