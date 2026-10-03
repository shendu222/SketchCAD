#include "BlockEditorDialog.h"

#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "core/SketchModel.h"
#include "core/Element.h"
#include "ui/GraphicsView.h"   // ElementItem

namespace SketchCAD {

// 把一个图元整体平移 (dx,dy)。pts 为通用控制点（直线端点/圆心/文本锚点/多段线顶点）；
// 圆的半径与弧角不随平移变化，故只动坐标不动 radius/角度。剖面填充的 holes 同步平移。
static void translateElement(Element& e, double dx, double dy) {
    for (auto& p : e.pts) { p.x += dx; p.y += dy; }
    for (auto& hole : e.holes)
        for (auto& p : hole) { p.x += dx; p.y += dy; }
}

BlockEditorDialog::BlockEditorDialog(SketchModel* model, const QString& blockName,
                                     QWidget* parent)
    : QDialog(parent), model_(model), blockName_(blockName)
{
    setWindowTitle(QStringLiteral("在位编辑块 — %1").arg(blockName_));
    setMinimumSize(680, 520);

    auto* top = new QHBoxLayout;
    auto* addLineBtn = new QPushButton(QStringLiteral("添加直线"));
    auto* addCircleBtn = new QPushButton(QStringLiteral("添加圆"));
    delBtn_        = new QPushButton(QStringLiteral("删除选中"));
    auto* fitBtn    = new QPushButton(QStringLiteral("适应窗口"));
    top->addWidget(addLineBtn);
    top->addWidget(addCircleBtn);
    top->addWidget(delBtn_);
    top->addWidget(fitBtn);
    top->addStretch(1);

    info_ = new QLabel(QStringLiteral(
        "在下方直接拖动图元即可修改块内几何；确定后所有引用实时同步。Esc / 取消丢弃不落盘。"));
    info_->setWordWrap(true);

    scene_ = new QGraphicsScene(this);
    view_  = new QGraphicsView(scene_, this);
    view_->setRenderHint(QPainter::Antialiasing, true);
    view_->setBackgroundBrush(QBrush(Qt::white));
    view_->setFrameShape(QFrame::StyledPanel);
    view_->setDragMode(QGraphicsView::NoDrag);   // 拖动由图元自身的 ItemIsMovable 承担

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    auto* lay = new QVBoxLayout(this);
    lay->addLayout(top);
    lay->addWidget(view_, 1);
    lay->addWidget(info_);
    lay->addWidget(box);

    connect(addLineBtn,   &QPushButton::clicked, this, &BlockEditorDialog::onAddLine);
    connect(addCircleBtn, &QPushButton::clicked, this, &BlockEditorDialog::onAddCircle);
    connect(delBtn_,       &QPushButton::clicked, this, &BlockEditorDialog::onDeleteSelected);
    connect(fitBtn,        &QPushButton::clicked, this, &BlockEditorDialog::onFit);
    connect(box, &QDialogButtonBox::accepted, this, [this] {
        // 确定：写回块内几何（仅替换 entities，保留 attdefs/dynParams/basePoint），
        // 所有引用经 expandBlockRef 实时同步刷新。
        model_->redefineBlockEntities(blockName_, collectEntities());
        QDialog::accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &BlockEditorDialog::reject);

    loadFromModel();
}

// 把一个块内图元加入场景：ElementItem 用 el 坐标直接绘制（pos 保持 0），
// 开启 ItemIsMovable 后用户拖动只改变 pos()，保存时把该偏移叠加回 el。
static ElementItem* addEditableItem(QGraphicsScene* scene, const SketchModel* model,
                                    const Element& e) {
    auto* it = new ElementItem(e);
    it->model    = model;                       // 嵌套块引用渲染需要块表
    it->dimScale = model ? model->scaleRatio() : 1.0;
    it->setFlag(QGraphicsItem::ItemIsMovable, true);
    it->setFlag(QGraphicsItem::ItemIsSelectable, true);
    scene->addItem(it);
    return it;
}

void BlockEditorDialog::loadFromModel() {
    if (!model_) return;
    for (const auto& e : model_->getBlockEntities(blockName_))
        addEditableItem(scene_, model_, e);
    onFit();
}

std::vector<Element> BlockEditorDialog::collectEntities() const {
    std::vector<Element> out;
    for (const auto* gi : scene_->items()) {
        const auto* it = dynamic_cast<const ElementItem*>(gi);
        if (!it) continue;
        Element e = it->el;
        // 拖动偏移叠加回几何：ElementItem 绘制用 el 坐标、拖动只改 pos，故此处补偿
        translateElement(e, it->pos().x(), it->pos().y());
        e.id = 0;                               // 块内图元 id 归零（与 redefineBlockEntities 一致）
        out.push_back(std::move(e));
    }
    return out;
}

void BlockEditorDialog::onAddLine() {
    // 在场景中心附近放一条默认水平线，随后可拖动
    const QPointF c = scene_->sceneRect().center();
    Element e; e.type = ElementType::Line;
    e.pts = { Vec3(c.x() - 20, c.y(), 0), Vec3(c.x() + 20, c.y(), 0) };
    auto* it = addEditableItem(scene_, model_, e);
    it->setSelected(true);
}

void BlockEditorDialog::onAddCircle() {
    const QPointF c = scene_->sceneRect().center();
    Element e; e.type = ElementType::Circle;
    e.pts = { Vec3(c.x(), c.y(), 0) };
    e.radius = 15.0;
    auto* it = addEditableItem(scene_, model_, e);
    it->setSelected(true);
}

void BlockEditorDialog::onDeleteSelected() {
    const auto sel = scene_->selectedItems();
    if (sel.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("删除"),
                                 QStringLiteral("请先在编辑区点选要删除的图元。"));
        return;
    }
    for (auto* gi : sel) {
        scene_->removeItem(gi);
        delete gi;
    }
}

void BlockEditorDialog::onFit() {
    QRectF r = scene_->itemsBoundingRect();
    if (r.isEmpty()) {
        view_->setSceneRect(QRectF(-50, -50, 100, 100));
        view_->fitInView(QRectF(-50, -50, 100, 100), Qt::KeepAspectRatio);
        return;
    }
    const double mx = std::max(1.0, r.width()  * 0.10);
    const double my = std::max(1.0, r.height() * 0.10);
    r = r.adjusted(-mx, -my, mx, my);
    view_->fitInView(r, Qt::KeepAspectRatio);
}

} // namespace SketchCAD
