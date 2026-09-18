#include "LayerPanel.h"
#include "core/SketchModel.h"
#include "core/Layer.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QColorDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QFrame>
#include <QSignalBlocker>

namespace SketchCAD {

namespace {
    // 线宽档位（GB/T 14665-2012 表1 的线宽系列，单位 mm）：索引 -> 线宽(mm)
    double lwValue(int idx) {
        switch (idx) {
            case 1: return 1.0;
            case 2: return 1.4;
            case 3: return 2.0;
            default: return 0.5;
        }
    }
    int lwIndex(double mm) {
        if (mm >= 1.7) return 3;
        if (mm >= 1.2) return 2;
        if (mm >= 0.75) return 1;
        return 0;
    }
    // 给 QWidget 设纯色背景（当作色块/颜色按钮；QLabel 与 QPushButton 通用）
    void setColorSwatch(QWidget* lab, const QColor& c) {
        lab->setStyleSheet(QStringLiteral("background:%1;border:1px solid #888;border-radius:2px;")
                               .arg(c.name()));
    }
}

LayerPanel::LayerPanel(SketchModel* model, QWidget* parent)
    : QWidget(parent), model_(model) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);

    list_ = new QListWidget(this);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setContextMenuPolicy(Qt::NoContextMenu);
    v->addWidget(list_, 1);

    // ---- 样式编辑组 ----
    auto* gb = new QGroupBox(QStringLiteral("图层样式"), this);
    auto* gl = new QVBoxLayout(gb);
    gl->setContentsMargins(8, 8, 8, 8);
    gl->setSpacing(5);

    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(new QLabel(QStringLiteral("颜色：")));
    colorBtn_ = new QPushButton(this);
    colorBtn_->setFixedSize(46, 22);
    setColorSwatch(colorBtn_, QColor(26, 29, 40));
    colorRow->addWidget(colorBtn_);
    colorRow->addStretch(1);
    gl->addLayout(colorRow);

    auto* lsRow = new QHBoxLayout();
    lsRow->addWidget(new QLabel(QStringLiteral("线型：")));
    lineStyleCombo_ = new QComboBox(this);
    for (int i = 0; i < kLineStyleCount; ++i)
        lineStyleCombo_->addItem(QString::fromUtf8(lineStyleName((LineStyle)i)));
    lsRow->addWidget(lineStyleCombo_, 1);
    gl->addLayout(lsRow);

    auto* lwRow = new QHBoxLayout();
    lwRow->addWidget(new QLabel(QStringLiteral("线宽：")));
    lineWidthCombo_ = new QComboBox(this);
    lineWidthCombo_->addItems({QStringLiteral("细线 0.5mm"), QStringLiteral("中粗线 1.0mm"),
                               QStringLiteral("粗线 1.4mm"), QStringLiteral("特粗线 2.0mm")});
    lwRow->addWidget(lineWidthCombo_, 1);
    gl->addLayout(lwRow);

    applyStyleBtn_ = new QPushButton(QStringLiteral("批量套用到本层图元"), this);
    gl->addWidget(applyStyleBtn_);
    v->addWidget(gb);

    // ---- 操作按钮 ----
    auto* op = new QHBoxLayout();
    newBtn_   = new QPushButton(QStringLiteral("新建"), this);
    delBtn_   = new QPushButton(QStringLiteral("删除"), this);
    renameBtn_ = new QPushButton(QStringLiteral("重命名"), this);
    op->addWidget(newBtn_); op->addWidget(delBtn_); op->addWidget(renameBtn_);
    v->addLayout(op);

    assignBtn_ = new QPushButton(QStringLiteral("将选中图元移入此层"), this);
    v->addWidget(assignBtn_);

    hintLabel_ = new QLabel(QStringLiteral("提示：把选中图元移入图层时保留其原有属性；修改图层样式/显示会同步到本层全部图元，不影响其他层。"), this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setStyleSheet(QStringLiteral("color:#777;font-size:11px;"));
    v->addWidget(hintLabel_);

    // ---- 信号接线 ----
    connect(list_, &QListWidget::currentRowChanged, this, &LayerPanel::onCurrentRowChanged);
    connect(colorBtn_, &QPushButton::clicked, this, &LayerPanel::onColorClicked);
    connect(lineStyleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LayerPanel::onLineStyleChanged);
    connect(lineWidthCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LayerPanel::onLineWidthChanged);
    connect(applyStyleBtn_, &QPushButton::clicked, this, &LayerPanel::onApplyStyle);
    connect(newBtn_, &QPushButton::clicked, this, &LayerPanel::onNewLayer);
    connect(delBtn_, &QPushButton::clicked, this, &LayerPanel::onDeleteLayer);
    connect(renameBtn_, &QPushButton::clicked, this, &LayerPanel::onRenameLayer);
    connect(assignBtn_, &QPushButton::clicked, this, &LayerPanel::onAssignSelection);

    // 模型变化即刷新面板：图层表增删/改名/样式/可见性、图元层归属或可见性变化都反映到列表
    connect(model_, &SketchModel::layersChanged, this, &LayerPanel::refresh);
    connect(model_, &SketchModel::modelChanged,  this, &LayerPanel::refresh);
    connect(model_, &SketchModel::selectionChanged, this, [this](int) { assignBtn_->setEnabled(model_->hasSelection()); });

    refresh();
}

QString LayerPanel::selectedLayer() const {
    QListWidgetItem* it = list_->currentItem();
    if (!it) return QString();
    return it->data(Qt::UserRole).toString();
}

void LayerPanel::refresh() {
    currentName_ = selectedLayer();
    list_->clear();
    const int n = model_->layerCount();
    for (int i = 0; i < n; ++i) {
        const Layer& l = model_->layerAt(i);
        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, l.name);
        item->setSizeHint(QSize(0, 26));
        list_->setItemWidget(item, makeRowWidget(l));
    }
    // 恢复选择
    if (!currentName_.isEmpty()) {
        for (int i = 0; i < list_->count(); ++i) {
            if (list_->item(i)->data(Qt::UserRole).toString() == currentName_) {
                list_->setCurrentRow(i); break;
            }
        }
    }
    updateControls();
    assignBtn_->setEnabled(model_->hasSelection());
}

QWidget* LayerPanel::makeRowWidget(const Layer& l) {
    auto* w = new QWidget(list_);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(4, 1, 4, 1);
    h->setSpacing(6);

    auto* vis = new QCheckBox(this);
    vis->setChecked(l.visible);
    vis->setToolTip(QStringLiteral("显示/隐藏整层"));
    connect(vis, &QCheckBox::stateChanged, this,
            [this, name = l.name](int s) { onVisibleToggled(name, s); });
    h->addWidget(vis);

    auto* lock = new QCheckBox(this);
    lock->setText(QStringLiteral("🔒"));
    lock->setChecked(l.locked);
    lock->setToolTip(QStringLiteral("锁定图层（禁止编辑本层图元）"));
    connect(lock, &QCheckBox::stateChanged, this,
            [this, name = l.name](int s) { onLockToggled(name, s); });
    h->addWidget(lock);

    auto* sw = new QLabel(this);
    sw->setFixedSize(14, 14);
    setColorSwatch(sw, l.color.isValid() ? l.color : QColor(26, 29, 40));
    h->addWidget(sw);

    auto* name = new QLabel(l.name, this);
    name->setStyleSheet(l.visible ? QStringLiteral("") : QStringLiteral("color:#999;"));
    h->addWidget(name, 1);

    return w;
}

void LayerPanel::updateControls() {
    const QString name = selectedLayer();
    const bool has = !name.isEmpty();
    colorBtn_->setEnabled(has);
    lineStyleCombo_->setEnabled(has);
    lineWidthCombo_->setEnabled(has);
    applyStyleBtn_->setEnabled(has);
    delBtn_->setEnabled(has && name != QStringLiteral("0"));
    renameBtn_->setEnabled(has && name != QStringLiteral("0"));
    if (!has) return;
    const Layer* l = model_->findLayer(name);
    if (!l) return;
    setColorSwatch(colorBtn_, l->color.isValid() ? l->color : QColor(26, 29, 40));
    // 阻塞信号：下面设置下拉索引是"回填显示"，不应触发 currentIndexChanged ->
    // onLineStyleChanged/onLineWidthChanged -> setLayerStyle 形成回写死循环（且会凭空压撤销栈）。
    {
        QSignalBlocker b1(lineStyleCombo_), b2(lineWidthCombo_);
        lineStyleCombo_->setCurrentIndex((int)l->lineStyle);
        lineWidthCombo_->setCurrentIndex(lwIndex(l->lineWidth));
    }
}

void LayerPanel::onCurrentRowChanged(int /*row*/) { updateControls(); }

void LayerPanel::onVisibleToggled(const QString& layer, int state) {
    model_->setLayerVisible(layer, state != Qt::Unchecked);
}

void LayerPanel::onLockToggled(const QString& layer, int state) {
    model_->setLayerLocked(layer, state != Qt::Unchecked);
}

void LayerPanel::onColorClicked() {
    const QString name = selectedLayer();
    if (name.isEmpty()) return;
    const Layer* l = model_->findLayer(name);
    if (!l) return;
    const QColor c = QColorDialog::getColor(l->color.isValid() ? l->color : QColor(26, 29, 40), this,
                                            QStringLiteral("选择图层颜色"));
    if (c.isValid()) model_->setLayerStyle(name, &c, nullptr, nullptr);   // 只改颜色，仅同步本层颜色
}

void LayerPanel::onLineStyleChanged(int idx) {
    const QString name = selectedLayer();
    if (name.isEmpty()) return;
    const LineStyle ls = (LineStyle)idx;
    model_->setLayerStyle(name, nullptr, nullptr, &ls);                   // 只改线型，仅同步本层线型
}

void LayerPanel::onLineWidthChanged(int idx) {
    const QString name = selectedLayer();
    if (name.isEmpty()) return;
    const double w = lwValue(idx);
    model_->setLayerStyle(name, nullptr, &w, nullptr);                   // 只改线宽，仅同步本层线宽
}

void LayerPanel::onApplyStyle() {
    const QString name = selectedLayer();
    if (name.isEmpty()) return;
    model_->applyLayerStyleToElements(name);
}

void LayerPanel::onNewLayer() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建图层"),
                              QStringLiteral("图层名称："), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) model_->addLayer(name.trimmed());
}

void LayerPanel::onDeleteLayer() {
    const QString name = selectedLayer();
    if (name.isEmpty() || name == QStringLiteral("0")) return;
    model_->removeLayer(name);
}

void LayerPanel::onRenameLayer() {
    const QString name = selectedLayer();
    if (name.isEmpty() || name == QStringLiteral("0")) return;
    bool ok = false;
    const QString nn = QInputDialog::getText(this, QStringLiteral("重命名图层"),
                            QStringLiteral("新名称："), QLineEdit::Normal, name, &ok);
    if (ok && !nn.trimmed().isEmpty() && nn.trimmed() != name)
        model_->renameLayer(name, nn.trimmed());
}

void LayerPanel::onAssignSelection() {
    const QString name = selectedLayer();
    if (name.isEmpty()) return;
    model_->assignSelectionToLayer(name, false);   // 保留图元原有属性，不套用图层样式
}

} // namespace SketchCAD
