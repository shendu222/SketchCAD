#include "SnapSettingsDialog.h"
#include "core/SketchModel.h"
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

namespace SketchCAD {

SnapSettingsDialog::SnapSettingsDialog(SketchModel* model, QWidget* parent)
    : QDialog(parent), model_(model) {
    setWindowTitle(QStringLiteral("捕捉设置 (OSNAP)"));

    // ---- 14 种捕捉模式的独立开关（数据源 = snap::snapFieldDefs，与快捷键/覆盖菜单一致）----
    const snap::SnapConfig cur = model ? model->snapConfig() : snap::SnapConfig();
    auto* form = new QFormLayout;
    for (const auto& d : snap::snapFieldDefs()) {
        auto* ck = new QCheckBox(d.label, this);
        ck->setChecked(cur.*(d.field));           // 当前全局配置回填
        checks_.push_back(ck);
        form->addRow(ck);
    }
    auto* box = new QGroupBox(QStringLiteral("捕捉模式（独立启用/禁用）"), this);
    box->setLayout(form);

    // ---- 便捷操作：全开 / 全关 / 恢复默认 ----
    auto* allOn  = new QPushButton(QStringLiteral("全部开启"), this);
    auto* allOff = new QPushButton(QStringLiteral("全部关闭"), this);
    auto* def    = new QPushButton(QStringLiteral("恢复默认"), this);
    auto* bar    = new QHBoxLayout;   bar->addWidget(allOn); bar->addWidget(allOff); bar->addWidget(def);

    connect(allOn,  &QPushButton::clicked, this, [this]() { for (auto* c : checks_) c->setChecked(true); });
    connect(allOff, &QPushButton::clicked, this, [this]() { for (auto* c : checks_) c->setChecked(false); });
    connect(def,    &QPushButton::clicked, this, [this]() {
        const snap::SnapConfig d;                 // 默认：端点/圆心/交点/切线开，其余关
        size_t i = 0;
        for (const auto& f : snap::snapFieldDefs()) checks_[i++]->setChecked(d.*(f.field));
    });

    // ---- 确认 / 取消 ----
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &SnapSettingsDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addWidget(box);
    main->addLayout(bar);
    main->addWidget(btns);
}

snap::SnapConfig SnapSettingsDialog::collect() const {
    snap::SnapConfig c;
    size_t i = 0;
    for (const auto& d : snap::snapFieldDefs()) {
        c.*(d.field) = checks_[i]->isChecked();
        ++i;
    }
    return c;
}

void SnapSettingsDialog::accept() {
    if (model_) model_->setSnapConfig(collect());
    QDialog::accept();
}

} // namespace SketchCAD
