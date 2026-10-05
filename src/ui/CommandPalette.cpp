#include "CommandPalette.h"
#include "core/CommandInterpreter.h"
#include "core/AliasTable.h"
#include "core/AppSettings.h"
#include "core/SketchModel.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QShowEvent>

namespace SketchCAD {

// ---- 条目装配：目录元数据 + 注册表回落条目 + 别名入口 ----
QVector<PaletteEntry> CommandPaletteDialog::buildEntries() {
    QVector<PaletteEntry> entries;
    const QStringList names = CommandInterpreter::commandNames();
    // 目录收录命令 → 用元数据；未收录 → 回落条目（title = 命令名，分类推断）。
    QHash<QString, const CommandInfo*> byName;
    for (const CommandInfo& ci : CommandCatalog::entries())
        byName.insert(ci.name, &ci);
    for (const QString& n : names) {
        PaletteEntry e;
        e.name = n;
        if (const CommandInfo* ci = byName.value(n, nullptr)) {
            e.title = ci->zhName.isEmpty() ? ci->name : ci->zhName;
            e.category = ci->category;
            e.summary = ci->summary;
            e.shortcut = ci->shortcut;
        } else {
            e.title = n;
            e.category = CommandCatalog::categoryOf(n);
        }
        entries.append(e);
    }
    // 别名入口：alias 非空，回车执行解析后的目标命令。
    const auto aliasMap = AliasTable::instance().map();
    for (auto it = aliasMap.constBegin(); it != aliasMap.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        PaletteEntry e;
        e.name = it.value();          // 执行目标命令
        e.title = it.value();
        e.category = CommandCatalog::categoryOf(it.value());
        e.alias = it.key();
        entries.append(e);
    }
    return entries;
}

CommandPaletteDialog::CommandPaletteDialog(CommandInterpreter* interp,
                                           AppSettings* settings, QWidget* parent)
    : QDialog(parent), interp_(interp), settings_(settings) {
    setWindowTitle(tr("命令面板"));
    setModal(true);
    resize(520, 420);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    edit_ = new QLineEdit(this);
    edit_->setPlaceholderText(tr("输入命令名 / 中文名 / 别名过滤（↑↓ 选择，Enter 执行，Esc 关闭）"));
    edit_->installEventFilter(this);
    lay->addWidget(edit_);

    list_ = new QListWidget(this);
    list_->setUniformItemSizes(false);
    lay->addWidget(list_, 1);

    auto* hint = new QLabel(
        tr("Enter 执行 · Ctrl+Shift+P 再次呼出 · 最近使用自动置顶"), this);
    hint->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    lay->addWidget(hint);

    connect(edit_, &QLineEdit::textChanged, this, [this] { rebuildList(); });
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem*) { executeCurrent(); });
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { executeCurrent(); });

    model_.setEntries(buildEntries());
    loadRecent();
}

void CommandPaletteDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    // 每次呼出重读最近使用（其他入口执行的命令也会写入该键）。
    loadRecent();
    edit_->clear();
    edit_->setFocus();
}

bool CommandPaletteDialog::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == edit_ && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
            const int dir = (ke->key() == Qt::Key_Up) ? -1 : 1;
            const int n = list_->count();
            if (n > 0) {
                int row = list_->currentRow() + dir;
                if (row < 0) row = n - 1;
                if (row >= n) row = 0;
                list_->setCurrentRow(row);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            executeCurrent();
            return true;
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void CommandPaletteDialog::rebuildList() {
    shown_ = model_.filter(edit_->text());
    list_->clear();
    for (const PaletteEntry& e : shown_) {
        QString text = QStringLiteral("%1  [%2]").arg(e.title, e.name);
        if (!e.alias.isEmpty()) text += QStringLiteral("  （别名 %1）").arg(e.alias);
        if (!e.shortcut.isEmpty()) text += QStringLiteral("  {%1}").arg(e.shortcut);
        QString sub = e.category;
        if (!e.summary.isEmpty()) sub += QStringLiteral(" — %1").arg(e.summary);
        auto* item = new QListWidgetItem(text, list_);
        item->setData(Qt::UserRole, e.name);
        item->setToolTip(sub);
    }
    if (list_->count() > 0) list_->setCurrentRow(0);
}

void CommandPaletteDialog::executeCurrent() {
    if (shown_.isEmpty()) return;
    const int row = list_->currentRow();
    if (row < 0 || row >= shown_.size()) return;
    const PaletteEntry e = shown_[row];
    if (!interp_) return;
    // 与命令行同一 handler 执行；成功才记入最近使用（失败保持面板打开可重试）。
    if (interp_->execute(e.name)) {
        model_.pushRecent(e.name);
        saveRecent();
        accept();
    }
}

void CommandPaletteDialog::loadRecent() {
    if (!settings_) return;
    QStringList recent;
    for (const QString& s : settings_->value(QLatin1String(SettingKeys::kRecentPalette)).toStringList())
        if (!s.trimmed().isEmpty()) recent << s.trimmed().toLower();
    model_.setRecent(recent);
}

void CommandPaletteDialog::saveRecent() {
    if (!settings_) return;
    settings_->setValue(QLatin1String(SettingKeys::kRecentPalette),
                        QVariant(model_.recent()));
}

} // namespace SketchCAD
