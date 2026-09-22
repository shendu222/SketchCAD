#include "BlockDialogs.h"

#include <QListWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>

#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Block.h"
#include "ui/GraphicsView.h"   // ElementItem

namespace SketchCAD {

// 把一组图元加入预览场景；遇到块引用则就地展开（递归），使嵌套块也能正确预览。
static void addPreviewEntities(QGraphicsScene* scene, SketchModel* model,
                              const std::vector<Element>& ents, double dimScale) {
    for (const auto& e : ents) {
        if (e.type == ElementType::BlockRef) {
            // expandBlockRef 已把嵌套块递归展开为世界坐标子图元
            addPreviewEntities(scene, model, model->expandBlockRef(e), dimScale);
            continue;
        }
        auto* it = new ElementItem(e);
        it->model = model;                // 理论上已是展开结果，但保留以策万全
        it->dimScale = dimScale;
        scene->addItem(it);
    }
}

// ============================================================================
// InsertBlockDialog
// ============================================================================
InsertBlockDialog::InsertBlockDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model)
{
    setWindowTitle(QStringLiteral("插入块"));
    setMinimumSize(520, 420);

    auto* left = new QVBoxLayout;
    list_ = new QListWidget;
    list_->setMinimumWidth(150);
    left->addWidget(new QLabel(QStringLiteral("块名：")));
    left->addWidget(list_);

    scene_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(scene_, this);
    view_->setRenderHint(QPainter::Antialiasing, true);
    view_->setBackgroundBrush(QBrush(Qt::white));
    view_->setFrameShape(QFrame::StyledPanel);
    view_->setMinimumSize(220, 220);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* right = new QVBoxLayout;
    right->addWidget(new QLabel(QStringLiteral("预览：")));
    right->addWidget(view_);

    auto* top = new QHBoxLayout;
    top->addLayout(left);
    top->addLayout(right, 1);

    auto* form = new QFormLayout;
    sbX_   = new QDoubleSpinBox; sbX_->setRange(-1e7, 1e7); sbX_->setDecimals(2); sbX_->setValue(0);
    sbY_   = new QDoubleSpinBox; sbY_->setRange(-1e7, 1e7); sbY_->setDecimals(2); sbY_->setValue(0);
    sbRot_ = new QDoubleSpinBox; sbRot_->setRange(-360, 360); sbRot_->setDecimals(1); sbRot_->setValue(0); sbRot_->setSuffix(QStringLiteral("°"));
    sbSx_  = new QDoubleSpinBox; sbSx_->setRange(0.001, 1e4); sbSx_->setDecimals(3); sbSx_->setValue(1);
    sbSy_  = new QDoubleSpinBox; sbSy_->setRange(0.001, 1e4); sbSy_->setDecimals(3); sbSy_->setValue(1);
    form->addRow(QStringLiteral("插入点 X"), sbX_);
    form->addRow(QStringLiteral("插入点 Y"), sbY_);
    form->addRow(QStringLiteral("旋转角"),   sbRot_);
    form->addRow(QStringLiteral("X 缩放"),   sbSx_);
    form->addRow(QStringLiteral("Y 缩放"),   sbSy_);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &InsertBlockDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(top);
    main->addLayout(form);
    main->addWidget(box);

    connect(list_, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { updatePreview(); });

    refreshList();
}

void InsertBlockDialog::refreshList() {
    list_->clear();
    for (const auto& def : model_->blockTable())
        list_->addItem(def.name);
    if (list_->count() > 0) list_->setCurrentRow(0);
    updatePreview();
}

void InsertBlockDialog::updatePreview() {
    scene_->clear();
    const QString name = list_->currentItem() ? list_->currentItem()->text() : QString();
    const BlockDef* def = model_->findBlockDef(name);
    if (!def || def->entities.empty()) {
        fitPreview();
        return;
    }
    addPreviewEntities(scene_, model_, def->entities, model_->scaleRatio());
    fitPreview();
}

void InsertBlockDialog::fitPreview() {
    QRectF r = scene_->itemsBoundingRect();
    if (r.isEmpty()) {
        view_->setSceneRect(QRectF(0, 0, 10, 10));
        view_->fitInView(QRectF(0, 0, 10, 10), Qt::KeepAspectRatio);
        return;
    }
    // 四周留白（约 8% 边距），避免图形贴边
    const double mx = std::max(1.0, r.width()  * 0.08);
    const double my = std::max(1.0, r.height() * 0.08);
    r = r.adjusted(-mx, -my, mx, my);
    view_->fitInView(r, Qt::KeepAspectRatio);
}

void InsertBlockDialog::accept() {
    const QString name = list_->currentItem() ? list_->currentItem()->text() : QString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("插入块"),
                            QStringLiteral("当前没有可用的块，请先创建块。"));
        return;
    }
    const Vec3 pos(sbX_->value(), sbY_->value(), 0.0);
    const int id = model_->insertBlock(name, pos, sbRot_->value(), sbSx_->value(), sbSy_->value());
    if (id < 0) {
        QMessageBox::warning(this, QStringLiteral("插入块"),
                            QStringLiteral("插入失败：块「%1」不存在或已被删除。").arg(name));
        return;
    }
    insertedId_ = id;
    model_->setSelection({id});
    QDialog::accept();
}

void InsertBlockDialog::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    // 对话框缩放时让预览重新适配视口（保持比例、不贴边）
    if (scene_ && scene_->itemsBoundingRect().isValid() && !scene_->itemsBoundingRect().isEmpty())
        fitPreview();
}

// ============================================================================
// CreateBlockDialog
// ============================================================================
CreateBlockDialog::CreateBlockDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model)
{
    setWindowTitle(QStringLiteral("创建块"));
    setMinimumWidth(320);

    auto* nameLay = new QHBoxLayout;
    nameEdit_ = new QLineEdit;
    nameEdit_->setPlaceholderText(QStringLiteral("块名（必填，须唯一）"));
    nameLay->addWidget(new QLabel(QStringLiteral("块名：")));
    nameLay->addWidget(nameEdit_);

    auto* form = new QFormLayout;
    sbX_ = new QDoubleSpinBox; sbX_->setRange(-1e7, 1e7); sbX_->setDecimals(2); sbX_->setValue(0);
    sbY_ = new QDoubleSpinBox; sbY_->setRange(-1e7, 1e7); sbY_->setDecimals(2); sbY_->setValue(0);
    form->addRow(QStringLiteral("基点 X"), sbX_);
    form->addRow(QStringLiteral("基点 Y"), sbY_);

    deleteRadio_ = new QRadioButton(QStringLiteral("删除原图元并放置块引用（BLOCK 语义）"));
    keepRadio_   = new QRadioButton(QStringLiteral("仅建立块定义，保留原图元"));
    deleteRadio_->setChecked(true);   // 默认：选择集直接变为一个块引用（可撤销）
    auto* modeBox = new QVBoxLayout;
    modeBox->addWidget(deleteRadio_);
    modeBox->addWidget(keepRadio_);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &CreateBlockDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(nameLay);
    main->addLayout(form);
    main->addWidget(new QLabel(QStringLiteral("创建后行为：")));
    main->addLayout(modeBox);
    main->addWidget(box);
}

QString CreateBlockDialog::blockName() const {
    return nameEdit_->text().trimmed();
}
Vec3 CreateBlockDialog::basePoint() const {
    return Vec3(sbX_->value(), sbY_->value(), 0.0);
}
bool CreateBlockDialog::deleteSource() const {
    return deleteRadio_->isChecked();
}

void CreateBlockDialog::accept() {
    const QString name = blockName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("创建块"),
                            QStringLiteral("块名不能为空。"));
        return;
    }
    if (model_->hasBlock(name)) {
        QMessageBox::warning(this, QStringLiteral("创建块"),
                            QStringLiteral("块「%1」已存在，请换名或用「块管理器」重定义。").arg(name));
        return;
    }
    if (model_->selection().empty()) {
        QMessageBox::warning(this, QStringLiteral("创建块"),
                            QStringLiteral("请先选中要收进块的图元。"));
        return;
    }
    // createBlockFromSelection：deleteSource 成功返回引用 id（>=0）；仅建定义成功返回 -1
    // （二者均属成功）。其余失败（空名/重名/空选）已在前置校验与 model 内部拦截。
    model_->createBlockFromSelection(name, basePoint(), deleteSource());
    QDialog::accept();
}

// ============================================================================
// BlockManagerDialog
// ============================================================================
BlockManagerDialog::BlockManagerDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model)
{
    setWindowTitle(QStringLiteral("块管理器"));
    setMinimumSize(420, 360);

    list_ = new QListWidget;
    infoLabel_ = new QLabel(QStringLiteral("（未选择块）"));
    infoLabel_->setWordWrap(true);

    renameBtn_   = new QPushButton(QStringLiteral("重命名..."));
    deleteBtn_   = new QPushButton(QStringLiteral("删除"));
    redefineBtn_ = new QPushButton(QStringLiteral("用当前选择重定义"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(renameBtn_);
    btnRow->addWidget(deleteBtn_);
    btnRow->addWidget(redefineBtn_);
    btnRow->addWidget(refreshBtn);

    auto* left = new QVBoxLayout;
    left->addWidget(new QLabel(QStringLiteral("已有块：")));
    left->addWidget(list_);

    auto* right = new QVBoxLayout;
    right->addWidget(infoLabel_);
    right->addLayout(btnRow);
    right->addStretch(1);

    auto* top = new QHBoxLayout;
    top->addLayout(left, 1);
    top->addLayout(right, 1);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addLayout(top);
    main->addWidget(box);

    connect(list_, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { onSelectionChanged(); });
    connect(renameBtn_,   &QPushButton::clicked, this, &BlockManagerDialog::onRename);
    connect(deleteBtn_,   &QPushButton::clicked, this, &BlockManagerDialog::onDelete);
    connect(redefineBtn_, &QPushButton::clicked, this, &BlockManagerDialog::onRedefine);
    connect(refreshBtn,   &QPushButton::clicked, this, &BlockManagerDialog::onRefresh);

    rebuildList();
}

QString BlockManagerDialog::currentName() const {
    return list_->currentItem() ? list_->currentItem()->text() : QString();
}

void BlockManagerDialog::rebuildList() {
    const QString cur = currentName();
    list_->clear();
    for (const auto& def : model_->blockTable())
        list_->addItem(def.name);
    if (!cur.isEmpty()) {
        for (int i = 0; i < list_->count(); ++i)
            if (list_->item(i)->text() == cur) { list_->setCurrentRow(i); break; }
    }
    if (list_->count() > 0 && !list_->currentItem()) list_->setCurrentRow(0);
    onSelectionChanged();
}

void BlockManagerDialog::onRefresh() {
    rebuildList();
}

void BlockManagerDialog::onSelectionChanged() {
    const QString name = currentName();
    const BlockDef* def = model_->findBlockDef(name);
    const bool has = def != nullptr;
    renameBtn_->setEnabled(has);
    deleteBtn_->setEnabled(has);
    redefineBtn_->setEnabled(has);
    if (!has) {
        infoLabel_->setText(list_->count() == 0
                            ? QStringLiteral("（当前没有块。请先用「创建块」建立块定义。）")
                            : QStringLiteral("（未选择块）"));
        return;
    }
    infoLabel_->setText(QStringLiteral("块「%1」\n图元数：%2\n属性定义：%3\n动态参数：%4")
                        .arg(def->name)
                        .arg(def->entities.size())
                        .arg(def->attdefs.size())
                        .arg(def->dynParams.size()));
}

void BlockManagerDialog::onRename() {
    const QString old = currentName();
    if (old.isEmpty()) return;
    bool ok = false;
    const QString nn = QInputDialog::getText(this, QStringLiteral("重命名块"),
                                            QStringLiteral("块「%1」的新名称：").arg(old),
                                            QLineEdit::Normal, old, &ok);
    if (!ok) return;
    const QString n = nn.trimmed();
    if (n.isEmpty()) { QMessageBox::warning(this, QStringLiteral("重命名块"), QStringLiteral("名称不能为空。")); return; }
    if (!model_->renameBlock(old, n)) {
        QMessageBox::warning(this, QStringLiteral("重命名块"),
                            QStringLiteral("重命名失败：新名称已存在或非法。"));
        return;
    }
    rebuildList();
}

void BlockManagerDialog::onDelete() {
    const QString name = currentName();
    if (name.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("删除块"),
                              QStringLiteral("确定删除块「%1」？所有引用将退化为未定义（不绘制）。")
                                  .arg(name),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!model_->removeBlock(name)) {
        QMessageBox::warning(this, QStringLiteral("删除块"), QStringLiteral("删除失败。"));
        return;
    }
    rebuildList();
}

void BlockManagerDialog::onRedefine() {
    const QString name = currentName();
    if (name.isEmpty()) return;
    if (model_->selection().empty()) {
        QMessageBox::warning(this, QStringLiteral("重定义块"),
                            QStringLiteral("请先选中要收进块「%1」的新图元。").arg(name));
        return;
    }
    if (!model_->redefineBlockFromSelection(name)) {
        QMessageBox::warning(this, QStringLiteral("重定义块"),
                            QStringLiteral("重定义失败，请检查选择集。"));
        return;
    }
    rebuildList();
}

} // namespace SketchCAD
