#include "StdPartTemplatePanel.h"
#include "core/StdPartTemplate.h"
#include "core/StdPartCatalog.h"
#include "core/StdPartInstance.h"
#include "core/Element.h"
#include "core/Geometry.h"
#include "core/SketchModel.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPainter>
#include <QPixmap>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>

namespace SketchCAD {

// 解析 "x,y[,z]" 形式的基点与插入坐标文本
static Vec3 parseVecText(const QString& s, bool* ok = nullptr) {
    const QStringList parts = s.trimmed().split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                                Qt::SkipEmptyParts);
    if (parts.size() < 2) { if (ok) *ok = false; return Vec3(0, 0, 0); }
    bool a = false, b = false, c = true;
    const double x = parts[0].toDouble(&a);
    const double y = parts[1].toDouble(&b);
    const double z = (parts.size() >= 3) ? parts[2].toDouble(&c) : 0.0;
    const bool good = a && b && c;
    if (ok) *ok = good;
    return good ? Vec3(x, y, z) : Vec3(0, 0, 0);
}

// 与 SheetFrameDialog::renderPreview 同源的轻量渲染（Line/Circle/Arc/Polyline/Text）
static QPixmap renderTemplatePreview(const std::vector<Element>& els, int w, int h) {
    QPixmap pm(w, h);
    pm.fill(Qt::white);
    if (els.empty()) return pm;
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (const auto& e : els) {
        if (e.type == ElementType::Circle || e.type == ElementType::Arc) {
            if (e.pts.empty()) continue;
            const Vec3& c = e.pts[0];
            minX = std::min(minX, c.x - e.radius); maxX = std::max(maxX, c.x + e.radius);
            minY = std::min(minY, c.y - e.radius); maxY = std::max(maxY, c.y + e.radius);
        }
        for (const auto& v : e.pts) {
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
        }
    }
    if (minX > maxX) return pm;
    const double margin = 12.0;
    const double spanX = std::max(1e-6, maxX - minX);
    const double spanY = std::max(1e-6, maxY - minY);
    const double s = std::min((w - 2 * margin) / spanX, (h - 2 * margin) / spanY);
    auto X = [&](double x) { return margin + (x - minX) * s; };
    auto Y = [&](double y) { return h - (margin + (y - minY) * s); };

    for (const auto& e : els) {
        const QColor col = e.color.isValid() ? e.color : QColor(26, 29, 40);
        QPen pen(col);
        pen.setWidthF(1.0);
        p.setPen(pen);
        switch (e.type) {
            case ElementType::Line:
                if (e.pts.size() >= 2)
                    p.drawLine(QPointF(X(e.pts[0].x), Y(e.pts[0].y)),
                               QPointF(X(e.pts[1].x), Y(e.pts[1].y)));
                break;
            case ElementType::Rectangle:
                if (e.pts.size() >= 2)
                    p.drawRect(QRectF(QPointF(X(e.pts[0].x), Y(e.pts[0].y)),
                                      QPointF(X(e.pts[1].x), Y(e.pts[1].y))));
                break;
            case ElementType::Circle:
                if (!e.pts.empty())
                    p.drawEllipse(QPointF(X(e.pts[0].x), Y(e.pts[0].y)),
                                  e.radius * s, e.radius * s);
                break;
            case ElementType::Arc:
                if (!e.pts.empty())
                    p.drawArc(QRectF(QPointF(X(e.pts[0].x - e.radius), Y(e.pts[0].y + e.radius)),
                                     QPointF(X(e.pts[0].x + e.radius), Y(e.pts[0].y - e.radius))),
                              (int)(-e.endAngle * 16), (int)((e.endAngle - e.startAngle) * 16));
                break;
            case ElementType::Polyline:
            case ElementType::Polygon: {
                if (e.pts.size() < 2) break;
                QPolygonF poly;
                for (const auto& v : e.pts) poly << QPointF(X(v.x), Y(v.y));
                if (e.closed) poly << poly.first();
                p.drawPolyline(poly);
                break;
            }
            case ElementType::Text:
            case ElementType::MText:
                if (!e.text.isEmpty() && !e.pts.empty()) {
                    QFont f = p.font();
                    f.setPixelSize(qBound(8, (int)(e.radius * s), 24));
                    p.setFont(f);
                    p.drawText(QPointF(X(e.pts[0].x), Y(e.pts[0].y)), e.text);
                }
                break;
            default:
                break;
        }
    }
    return pm;
}

StdPartTemplatePanel::StdPartTemplatePanel(SketchModel* model, QWidget* parent)
    : QWidget(parent), model_(model) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto* split = new QSplitter(Qt::Horizontal, this);
    root->addWidget(split, 1);

    // 左：分类/模板树
    auto* left = new QWidget(split);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    tree_ = new QTreeWidget(left);
    tree_->setHeaderLabels({ QStringLiteral("标准件模板库") });
    tree_->setAlternatingRowColors(true);
    ll->addWidget(tree_, 1);
    info_ = new QLabel(QStringLiteral("未加载模板库"), left);
    info_->setWordWrap(true);
    ll->addWidget(info_);
    split->addWidget(left);

    // 右：参数 + 预览
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    auto* formTitle = new QLabel(QStringLiteral("可变参数（修改后实时预览）"), right);
    formTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    rl->addWidget(formTitle);

    auto* specRow = new QHBoxLayout();
    specRow->addWidget(new QLabel(QStringLiteral("规格文字"), right));
    specEdit_ = new QLineEdit(right);
    specRow->addWidget(specEdit_, 1);
    rl->addLayout(specRow);

    // 插入定位：位置来源 / 坐标 / 基点 / 旋转 / 图层
    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(QStringLiteral("插入位置"), right));
    insertModeCombo_ = new QComboBox(right);
    insertModeCombo_->addItem(QStringLiteral("视图中心"), QStringLiteral("viewCenter"));
    insertModeCombo_->addItem(QStringLiteral("画布拾取"), QStringLiteral("pick"));
    insertModeCombo_->addItem(QStringLiteral("坐标输入"), QStringLiteral("coordinate"));
    modeRow->addWidget(insertModeCombo_, 1);
    pointEdit_ = new QLineEdit(right);
    pointEdit_->setPlaceholderText(QStringLiteral("x,y / @dx,dy / dist<angle"));
    modeRow->addWidget(pointEdit_, 2);
    rl->addLayout(modeRow);

    pickHint_ = new QLabel(QStringLiteral("点击「插入当前图纸」后，在画布上单击指定插入位置"), right);
    pickHint_->setStyleSheet(QStringLiteral("color:#7a5c00;"));
    pickHint_->setVisible(false);
    rl->addWidget(pickHint_);

    auto* placeRow = new QHBoxLayout();
    placeRow->addWidget(new QLabel(QStringLiteral("基点"), right));
    baseCombo_ = new QComboBox(right);
    baseCombo_->addItem(QStringLiteral("包围盒中心"), QStringLiteral("bboxCenter"));
    baseCombo_->addItem(QStringLiteral("左下角"), QStringLiteral("lowerLeft"));
    baseCombo_->addItem(QStringLiteral("首个圆心"), QStringLiteral("circleCenter"));
    baseCombo_->addItem(QStringLiteral("自定义"), QStringLiteral("custom"));
    placeRow->addWidget(baseCombo_);
    customBaseEdit_ = new QLineEdit(right);
    customBaseEdit_->setPlaceholderText(QStringLiteral("模板内基点 x,y"));
    placeRow->addWidget(customBaseEdit_, 1);
    rl->addLayout(placeRow);

    auto* alignRow = new QHBoxLayout();
    alignRow->addWidget(new QLabel(QStringLiteral("旋转"), right));
    angleSpin_ = new QDoubleSpinBox(right);
    angleSpin_->setRange(-360.0, 360.0);
    angleSpin_->setDecimals(2);
    angleSpin_->setSuffix(QStringLiteral(" °"));
    alignRow->addWidget(angleSpin_);
    alignRow->addWidget(new QLabel(QStringLiteral("图层"), right));
    layerEdit_ = new QLineEdit(QStringLiteral("0"), right);
    alignRow->addWidget(layerEdit_, 1);
    rl->addLayout(alignRow);

    paramHost_ = new QWidget(right);
    paramForm_ = new QFormLayout(paramHost_);
    paramForm_->setContentsMargins(0, 0, 0, 0);
    rl->addWidget(paramHost_);

    validation_ = new QLabel(QStringLiteral("请选择左侧模板"), right);
    validation_->setWordWrap(true);
    rl->addWidget(validation_);

    previewLabel_ = new QLabel(right);
    previewLabel_->setMinimumSize(320, 260);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet(QStringLiteral("border:1px solid #ccc;background:white;"));
    rl->addWidget(previewLabel_, 1);

    auto* btns = new QHBoxLayout();
    insertBtn_ = new QPushButton(QStringLiteral("插入当前图纸"), right);
    saveBtn_ = new QPushButton(QStringLiteral("保存为图纸文件"), right);
    resetBtn_ = new QPushButton(QStringLiteral("重置原规格"), right);
    insertBtn_->setEnabled(false);
    saveBtn_->setEnabled(false);
    resetBtn_->setEnabled(false);
    btns->addWidget(insertBtn_);
    btns->addWidget(saveBtn_);
    btns->addWidget(resetBtn_);
    rl->addLayout(btns);

    auto* jsonBtns = new QHBoxLayout();
    saveJsonBtn_ = new QPushButton(QStringLiteral("保存为参数 JSON"), right);
    openJsonBtn_ = new QPushButton(QStringLiteral("打开参数 JSON"), right);
    jsonBtns->addWidget(saveJsonBtn_);
    jsonBtns->addWidget(openJsonBtn_);
    rl->addLayout(jsonBtns);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);

    connect(tree_, &QTreeWidget::itemClicked,
            this, &StdPartTemplatePanel::onCategoryOrTemplatePicked);
    connect(specEdit_, &QLineEdit::textChanged, this, &StdPartTemplatePanel::onParamChanged);
    connect(insertBtn_, &QPushButton::clicked, this, &StdPartTemplatePanel::onInsert);
    connect(saveBtn_, &QPushButton::clicked, this, &StdPartTemplatePanel::onSave);
    connect(resetBtn_, &QPushButton::clicked, this, &StdPartTemplatePanel::onReset);

    auto syncMode = [this]() {
        const QString mode = insertModeCombo_->currentData().toString();
        pointEdit_->setVisible(mode == QStringLiteral("coordinate"));
        pickHint_->setVisible(mode == QStringLiteral("pick"));
        customBaseEdit_->setVisible(baseCombo_->currentData().toString() == QStringLiteral("custom"));
    };
    connect(insertModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, syncMode);
    connect(baseCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, syncMode);
    connect(insertModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StdPartTemplatePanel::onParamChanged);
    connect(baseCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StdPartTemplatePanel::onParamChanged);
    connect(pointEdit_, &QLineEdit::textChanged, this, &StdPartTemplatePanel::onParamChanged);
    connect(customBaseEdit_, &QLineEdit::textChanged, this, &StdPartTemplatePanel::onParamChanged);
    connect(angleSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StdPartTemplatePanel::onParamChanged);
    connect(layerEdit_, &QLineEdit::textChanged, this, &StdPartTemplatePanel::onParamChanged);
    syncMode();

    connect(saveJsonBtn_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存标准件参数 JSON"), proposeSavePath(QStringLiteral("stdpart.json")),
            QStringLiteral("标准件参数 JSON (*.stdpart.json *.json);;所有文件 (*)"));
        if (!path.isEmpty()) emit saveJsonRequested(path);
    });
    connect(openJsonBtn_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开标准件参数 JSON"), QString(),
            QStringLiteral("标准件参数 JSON (*.stdpart.json *.json);;所有文件 (*)"));
        if (!path.isEmpty()) emit openJsonRequested(path);
    });

    // 默认库路径由 StdPartTemplateStore 单一提供（发布/开发候选顺序同源）；
    // 可用 loadStore 覆盖。
    QString err;
    const QString chosen = StdPartTemplateStore::defaultStoreDir();
    if (!chosen.isEmpty()) {
        if (!loadStore(chosen, &err))
            info_->setText(QStringLiteral("模板库加载失败: %1").arg(err));
    } else {
        info_->setText(QStringLiteral("模板库未找到（运行 tools/stdpart 构建器生成 data/stdpart/templates）"));
    }
}

bool StdPartTemplatePanel::loadStore(const QString& storeDir, QString* err) {
    if (!StdPartTemplateStore::instance().load(storeDir, err)) return false;
    storeDir_ = storeDir;
    rebuildTree();
    const auto& ms = StdPartTemplateStore::instance().metas();
    int ok = 0;
    for (const auto& m : ms) if (m.ok) ++ok;
    info_->setText(QStringLiteral("模板: %1 个（可用 %2，失败 %3） · 库: %4")
                       .arg((int)ms.size()).arg(ok).arg((int)ms.size() - ok).arg(storeDir));
    return true;
}

void StdPartTemplatePanel::rebuildTree() {
    tree_->clear();
    const auto cats = StdPartTemplateStore::instance().categories();
    for (const QString& cat : cats) {
        auto* ci = new QTreeWidgetItem(tree_, { cat });
        ci->setData(0, Qt::UserRole, QStringLiteral("CAT:") + cat);
        ci->setFlags(ci->flags() & ~Qt::ItemIsDragEnabled);
        const auto metas = StdPartTemplateStore::instance().byCategory(cat);
        for (const StdPartTemplateMeta* m : metas) {
            auto* ti = new QTreeWidgetItem(ci, { m->specText.isEmpty() ? m->id : m->specText });
            ti->setData(0, Qt::UserRole, m->id);
            ti->setToolTip(0, QStringLiteral("%1\n标准: %2\n图元: %3\n%4")
                              .arg(m->sourceFile, m->standard)
                              .arg(m->elementCount)
                              .arg(m->ok ? QStringLiteral("解析成功") : m->failReason));
            if (!m->ok) ti->setForeground(0, QColor(180, 60, 60));
        }
    }
    tree_->expandAll();
}

void StdPartTemplatePanel::refresh() {
    if (!storeDir_.isEmpty()) {
        QString err;
        StdPartTemplateStore::instance().load(storeDir_, &err);
    }
    rebuildTree();
}

void StdPartTemplatePanel::onCategoryOrTemplatePicked(QTreeWidgetItem* item, int) {
    if (!item) return;
    const QString data = item->data(0, Qt::UserRole).toString();
    if (data.startsWith(QStringLiteral("CAT:"))) return;
    loadTemplate(data);
}

void StdPartTemplatePanel::loadTemplate(const QString& id) {
    const StdPartTemplateMeta* meta = nullptr;
    std::vector<Element> src;
    QString err;
    if (!StdPartTemplateStore::instance().loadElements(id, src, &meta, &err)) {
        validation_->setText(err);
        return;
    }
    currentId_ = id;

    // 重建参数表单：NOMINAL_D / LENGTH_L / PITCH_P / ACROSS_FLATS_S / THICKNESS_T
    while (paramForm_->rowCount() > 0) paramForm_->removeRow(0);
    paramWidgets_.clear();
    paramKeys_.clear();
    const QStringList keys = {
        QStringLiteral("NOMINAL_D"), QStringLiteral("LENGTH_L"),
        QStringLiteral("PITCH_P"), QStringLiteral("ACROSS_FLATS_S"),
        QStringLiteral("THICKNESS_T")
    };
    const QStringList labels = {
        QStringLiteral("公称直径 d (mm)"), QStringLiteral("长度 L (mm)"),
        QStringLiteral("螺距 P (mm)"), QStringLiteral("对边 s (mm)"),
        QStringLiteral("厚度 k/m/h (mm)")
    };
    const double defaults[] = { meta->baselineNominal, meta->baselineLength,
                                meta->baselinePitch, meta->baselineAcrossFlats,
                                meta->baselineThickness };
    for (int i = 0; i < keys.size(); ++i) {
        if (defaults[i] <= 0.0 && i > 1) continue;  // 无该参数则隐藏
        auto* sb = new QDoubleSpinBox(paramHost_);
        sb->setDecimals(2);
        sb->setRange(0.01, 1000.0);
        sb->setSingleStep(0.5);
        sb->setValue(defaults[i] > 0 ? defaults[i] : 1.0);
        sb->setSuffix(QStringLiteral(" mm"));
        sb->setObjectName(keys[i]);
        paramForm_->addRow(labels[i], sb);
        paramWidgets_ << sb;
        paramKeys_ << keys[i];
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &StdPartTemplatePanel::onParamChanged);
    }
    specEdit_->setText(meta->specText);
    validation_->setText(QStringLiteral("已载入: %1 · %2 · 原图 %3 图元")
                             .arg(meta->sourceFile, meta->standard).arg(meta->elementCount));
    insertBtn_->setEnabled(true);
    saveBtn_->setEnabled(true);
    resetBtn_->setEnabled(true);
    onParamChanged();
}

QVariantMap StdPartTemplatePanel::collectParams(QString* err) const {
    QVariantMap p;
    bool bad = false;
    for (int i = 0; i < paramWidgets_.size(); ++i) {
        auto* sb = qobject_cast<QDoubleSpinBox*>(paramWidgets_[i]);
        if (!sb) continue;
        const double v = sb->value();
        if (v <= 0.0) bad = true;
        p[paramKeys_[i]] = v;
    }
    const QString spec = specEdit_->text().trimmed();
    if (spec.isEmpty()) bad = true;
    p[QStringLiteral("SPEC_TEXT")] = spec;
    if (bad && err) *err = QStringLiteral("参数必须 > 0 且规格文字不能为空");
    return p;
}

void StdPartTemplatePanel::onParamChanged() {
    if (currentId_.isEmpty()) return;
    QString err;
    const QVariantMap p = collectParams(&err);
    if (!err.isEmpty()) {
        validation_->setText(err);
        return;
    }
    std::vector<Element> src;
    const StdPartTemplateMeta* meta = StdPartTemplateStore::instance().find(currentId_);
    if (!meta || !StdPartTemplateStore::instance().loadElements(currentId_, src)) return;
    // 用界面上的 SPEC_TEXT 覆盖自动拼装值
    QVariantMap effective = p;
    rendered_ = StdPartTemplateStore::render(src, *meta, effective);
    // 预览跟随插入定位（旋转/基点）；rendered_ 本身仍是不含定位的原始结果，
    // 避免「插入」或「保存图纸」对同一次渲染重复施加变换。
    const auto placed = StdPartInstance::placeElements(rendered_, placement());
    previewLabel_->setPixmap(renderTemplatePreview(placed, previewLabel_->width(),
                                                   previewLabel_->height()));
    validation_->setText(QStringLiteral("预览已更新 · %1 图元").arg(rendered_.size()));
}

void StdPartTemplatePanel::updatePreview() { onParamChanged(); }

void StdPartTemplatePanel::onInsert() { emit insertRequested(); }

QVariantMap StdPartTemplatePanel::parameters(QString* err) const {
    return collectParams(err);
}

QString StdPartTemplatePanel::currentTemplateId() const {
    return currentId_;
}

StdPartPlacement StdPartTemplatePanel::placement() const {
    StdPartPlacement p;
    p.mode = insertModeCombo_->currentData().toString();
    p.pointText = pointEdit_->text().trimmed();
    if (p.mode == QStringLiteral("coordinate") && model_ && !p.pointText.isEmpty()) {
        model_->parsePointToWorld(p.pointText, model_->lastInputPointWorld(), p.point);
    } else if (!p.pointText.isEmpty()) {
        bool ok = false;
        p.point = parseVecText(p.pointText, &ok);
        if (!ok) p.point = Vec3(0, 0, 0);
    }
    p.base = baseCombo_->currentData().toString();
    bool baseOk = false;
    p.customBase = parseVecText(customBaseEdit_->text(), &baseOk);
    if (!baseOk) p.customBase = Vec3(0, 0, 0);
    p.angleDeg = angleSpin_->value();
    p.scale = 1.0;
    p.layer = layerEdit_->text().trimmed();
    if (p.layer.isEmpty()) p.layer = QStringLiteral("0");
    return p;
}

bool StdPartTemplatePanel::applyInstance(const StdPartInstance& inst, QString* err) {
    auto& store = StdPartTemplateStore::instance();
    if (!store.loaded()) {
        QString le;
        if (!store.load(StdPartTemplateStore::defaultStoreDir(), &le)) {
            if (err) *err = le;
            return false;
        }
    }
    const StdPartTemplateMeta* meta = store.find(inst.templateId);
    if (!meta) {
        if (err) *err = QStringLiteral("模板库中找不到 templateId=%1").arg(inst.templateId);
        return false;
    }
    loadTemplate(inst.templateId);
    const QString spec = inst.params.value(QStringLiteral("SPEC_TEXT"), meta->specText).toString();
    specEdit_->setText(spec);
    for (const QString& key : paramKeys_) {
        if (inst.params.contains(key)) {
            const double v = inst.params.value(key).toDouble();
            for (QWidget* w : paramWidgets_) {
                if (auto* sb = qobject_cast<QDoubleSpinBox*>(w)) {
                    if (sb->objectName() == key) sb->setValue(v);
                }
            }
        }
    }
    auto setCombo = [](QComboBox* cb, const QString& data) {
        const int i = cb->findData(data);
        if (i >= 0) cb->setCurrentIndex(i);
    };
    setCombo(insertModeCombo_, inst.placement.mode);
    setCombo(baseCombo_, inst.placement.base);
    const QString pt = inst.placement.pointText.isEmpty()
        ? QStringLiteral("%1,%2").arg(inst.placement.point.x, 0, 'f', 2)
                                 .arg(inst.placement.point.y, 0, 'f', 2)
        : inst.placement.pointText;
    pointEdit_->setText(pt);
    customBaseEdit_->setText(QStringLiteral("%1,%2")
                                 .arg(inst.placement.customBase.x, 0, 'f', 2)
                                 .arg(inst.placement.customBase.y, 0, 'f', 2));
    angleSpin_->setValue(inst.placement.angleDeg);
    layerEdit_->setText(inst.placement.layer.isEmpty() ? QStringLiteral("0") : inst.placement.layer);
    onParamChanged();
    return true;
}

void StdPartTemplatePanel::setDialogMode(bool on) {
    for (QPushButton* b : { insertBtn_, saveBtn_, resetBtn_, saveJsonBtn_, openJsonBtn_ }) {
        if (b) b->setVisible(!on);
    }
}

void StdPartTemplatePanel::resetToDefaults() {
    if (!currentId_.isEmpty()) onReset();
}

QString StdPartTemplatePanel::proposeSavePath(const QString& ext) const {
    const StdPartTemplateMeta* m = StdPartTemplateStore::instance().find(currentId_);
    const QString name = m ? m->specText : currentId_;
    return QDir::home().filePath(QStringLiteral("%1.%2").arg(name, ext));
}

void StdPartTemplatePanel::onSave() {
    if (rendered_.empty()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存生成的图纸"), proposeSavePath(),
        QStringLiteral("DXF 图纸 (*.dxf);;SketchCAD 存档 (*.sketch);;DWG 图纸 (*.dwg)"));
    if (path.isEmpty()) return;
    emit saveRequested(path);
}

void StdPartTemplatePanel::onReset() { loadTemplate(currentId_); }

const std::vector<Element>& StdPartTemplatePanel::rendered() const { return rendered_; }

} // namespace SketchCAD
