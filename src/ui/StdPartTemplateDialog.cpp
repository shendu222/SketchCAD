#include "StdPartTemplateDialog.h"
#include "StdPartTemplatePanel.h"
#include "core/StdPartTemplate.h"
#include "core/SketchModel.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QDir>

namespace SketchCAD {

StdPartTemplateDialog::StdPartTemplateDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("标准件模板库"));
    setObjectName(QStringLiteral("dlgStdPartTemplates"));
    setWindowModality(Qt::NonModal);   // 非模态：允许「画布拾取」落点时操作图纸
    resize(1120, 760);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // 实例工作集区：多个参数实例可在对话框内新建/编辑/删除/导入/导出
    auto* workBox = new QGroupBox(QStringLiteral("实例工作集"), this);
    auto* workRow = new QHBoxLayout(workBox);
    workRow->addWidget(new QLabel(QStringLiteral("当前实例"), workBox));
    instanceCombo_ = new QComboBox(workBox);
    workRow->addWidget(instanceCombo_, 2);
    newBtn_    = new QPushButton(QStringLiteral("新建"), workBox);
    editBtn_   = new QPushButton(QStringLiteral("编辑"), workBox);
    deleteBtn_ = new QPushButton(QStringLiteral("删除"), workBox);
    importBtn_ = new QPushButton(QStringLiteral("导入"), workBox);
    exportBtn_ = new QPushButton(QStringLiteral("导出"), workBox);
    for (QPushButton* b : { newBtn_, editBtn_, deleteBtn_, importBtn_, exportBtn_ })
        workRow->addWidget(b);
    root->addWidget(workBox);

    root->addWidget(new QLabel(
        QStringLiteral("工作区：左侧为模板分类导航，右侧为缩略图预览与参数配置（修改后即时刷新）"), this));

    // 模板分类导航 / 缩略图预览 / 参数配置：复用原面板，隐藏其自带按钮由对话框统一接管
    panel_ = new StdPartTemplatePanel(model_, this);
    panel_->setDialogMode(true);
    root->addWidget(panel_, 1);

    auto* actionBox = new QGroupBox(QStringLiteral("操作"), this);
    auto* actionRow = new QHBoxLayout(actionBox);
    applyBtn_ = new QPushButton(QStringLiteral("应用"), actionBox);
    insertBtn_ = new QPushButton(QStringLiteral("插入当前图纸"), actionBox);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), actionBox);
    status_ = new QLabel(QStringLiteral("请选择模板并新建参数实例"), actionBox);
    status_->setStyleSheet(QStringLiteral("color:#4a6b8a;"));
    actionRow->addWidget(applyBtn_);
    actionRow->addWidget(insertBtn_);
    actionRow->addWidget(closeBtn);
    actionRow->addWidget(status_, 1);
    root->addWidget(actionBox);

    connect(newBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onNew);
    connect(editBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onEdit);
    connect(deleteBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onDelete);
    connect(importBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onImport);
    connect(exportBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onExport);
    connect(applyBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onApply);
    connect(insertBtn_, &QPushButton::clicked, this, &StdPartTemplateDialog::onPanelInsertRequested);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(instanceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StdPartTemplateDialog::onInstanceSelected);
    connect(panel_, &StdPartTemplatePanel::insertRequested,
            this, &StdPartTemplateDialog::onPanelInsertRequested);

    rebuildCombo();
}

StdPartInstance StdPartTemplateDialog::makeInstanceFromPanel(QString* err) const {
    StdPartInstance inst;
    if (!panel_) {
        if (err) *err = QStringLiteral("标准件面板未初始化");
        return inst;
    }
    QString pe;
    const QVariantMap params = panel_->parameters(&pe);
    if (!pe.isEmpty()) {
        if (err) *err = pe;
        return inst;
    }
    auto& store = StdPartTemplateStore::instance();
    if (!store.loaded()) store.load(StdPartTemplateStore::defaultStoreDir());
    const StdPartTemplateMeta* meta = store.find(panel_->currentTemplateId());
    if (!meta) {
        if (err) *err = QStringLiteral("请先选择一个标准件模板");
        return inst;
    }
    StdPartPlacement pl = panel_->placement();
    if (pl.mode == QStringLiteral("coordinate") && model_ && !pl.pointText.isEmpty()) {
        model_->parsePointToWorld(pl.pointText, model_->lastInputPointWorld(), pl.point);
    }
    inst.typeId     = StdPartInstance::typeIdFor(*meta);
    inst.category   = meta->category;
    inst.standard   = meta->standard;
    inst.templateId = meta->id;
    inst.params     = params;
    inst.placement  = pl;
    return inst;
}

StdPartInstance StdPartTemplateDialog::currentInstance(QString* err) const {
    const int i = currentIndex();
    if (i >= 0 && i < instances_.size()) return instances_[i];
    return makeInstanceFromPanel(err);
}

QString StdPartTemplateDialog::instanceLabel(const StdPartInstance& inst) const {
    const QString spec = inst.params.value(QStringLiteral("SPEC_TEXT")).toString();
    const QString head = inst.typeId.isEmpty() ? inst.templateId.left(8) : inst.typeId;
    return spec.isEmpty() ? head : QStringLiteral("%1 · %2").arg(head, spec);
}

int StdPartTemplateDialog::currentIndex() const {
    if (!instanceCombo_ || instanceCombo_->currentIndex() < 0) return -1;
    const int i = instanceCombo_->itemData(instanceCombo_->currentIndex()).toInt();
    return (i >= 0 && i < instances_.size()) ? i : -1;
}

void StdPartTemplateDialog::rebuildCombo(int selectIndex) {
    const QSignalBlocker blocker(instanceCombo_);
    instanceCombo_->clear();
    if (instances_.isEmpty()) {
        instanceCombo_->addItem(QStringLiteral("（未建立实例，点击「新建」）"), -1);
    } else {
        for (int i = 0; i < instances_.size(); ++i)
            instanceCombo_->addItem(instanceLabel(instances_[i]), i);
    }
    const int target = instances_.isEmpty() ? 0
        : qBound(0, selectIndex, instances_.size() - 1);
    instanceCombo_->setCurrentIndex(target);
}

void StdPartTemplateDialog::setStatus(const QString& text) {
    if (status_) status_->setText(text);
}

void StdPartTemplateDialog::onNew() {
    panel_->resetToDefaults();
    QString err;
    const StdPartInstance inst = makeInstanceFromPanel(&err);
    if (!err.isEmpty()) { QMessageBox::warning(this, QStringLiteral("新建失败"), err); return; }
    instances_.append(inst);
    rebuildCombo(instances_.size() - 1);
    setStatus(QStringLiteral("已新建实例：%1").arg(instanceLabel(inst)));
    emit instanceApplied(inst);
}

void StdPartTemplateDialog::onEdit() {
    const int i = currentIndex();
    if (i < 0) { setStatus(QStringLiteral("请先在实例工作集中选择一个实例")); return; }
    QString err;
    if (!panel_->applyInstance(instances_[i], &err)) {
        QMessageBox::warning(this, QStringLiteral("编辑失败"), err);
        return;
    }
    setStatus(QStringLiteral("正在编辑：%1（修改后点「应用」写回实例）")
                  .arg(instanceLabel(instances_[i])));
}

void StdPartTemplateDialog::onDelete() {
    const int i = currentIndex();
    if (i < 0) { setStatus(QStringLiteral("没有可删除的实例")); return; }
    const QString label = instanceLabel(instances_[i]);
    instances_.removeAt(i);
    rebuildCombo(qMin(i, instances_.size() - 1));
    const int next = currentIndex();
    if (next >= 0) panel_->applyInstance(instances_[next]);
    setStatus(QStringLiteral("已删除实例：%1").arg(label));
}

void StdPartTemplateDialog::onImport() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入标准件参数 JSON"), QString(),
        QStringLiteral("标准件参数 JSON (*.stdpart.json *.json);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QString err;
    StdPartInstance inst;
    if (!StdPartInstance::readFile(path, inst, &err)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), err);
        return;
    }
    if (!panel_->applyInstance(inst, &err)) {
        QMessageBox::warning(this, QStringLiteral("模板载入失败"), err);
        return;
    }
    instances_.append(inst);
    rebuildCombo(instances_.size() - 1);
    setStatus(QStringLiteral("已导入：%1").arg(path));
    emit instanceApplied(inst);
}

void StdPartTemplateDialog::onExport() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出标准件参数 JSON"),
        QDir::home().filePath(QStringLiteral("stdpart.stdpart.json")),
        QStringLiteral("标准件参数 JSON (*.stdpart.json *.json);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QString outPath = path;
    if (!outPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".stdpart.json");
    QString err;
    const StdPartInstance inst = makeInstanceFromPanel(&err);
    if (!err.isEmpty()) { QMessageBox::warning(this, QStringLiteral("导出失败"), err); return; }
    if (!StdPartInstance::writeFile(outPath, inst, &err)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), err);
        return;
    }
    setStatus(QStringLiteral("已导出：%1").arg(outPath));
}

void StdPartTemplateDialog::onApply() {
    QString err;
    StdPartInstance inst = makeInstanceFromPanel(&err);
    if (!err.isEmpty()) { QMessageBox::warning(this, QStringLiteral("应用失败"), err); return; }
    const int i = currentIndex();
    if (i >= 0) {
        inst.createdAt = instances_[i].createdAt;
        instances_[i] = inst;
        rebuildCombo(i);
    } else {
        instances_.append(inst);
        rebuildCombo(instances_.size() - 1);
    }
    setStatus(QStringLiteral("已应用到实例：%1").arg(instanceLabel(inst)));
    emit instanceApplied(inst);
}

void StdPartTemplateDialog::onInstanceSelected(int) {
    const int i = currentIndex();
    if (i < 0) return;
    QString err;
    if (!panel_->applyInstance(instances_[i], &err)) {
        setStatus(err);
        return;
    }
    setStatus(QStringLiteral("已载入实例：%1").arg(instanceLabel(instances_[i])));
}

void StdPartTemplateDialog::onPanelInsertRequested() {
    if (currentIndex() < 0) onApply();   // 未新建实例时先按当前表单建立实例
    emit insertRequested();
}

} // namespace SketchCAD
