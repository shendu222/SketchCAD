#include "CommandCatalogEditor.h"
#include "core/CommandCatalog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QJsonArray>
#include <algorithm>

namespace SketchCAD {
namespace {

enum class FieldKind { Text, Int, Enum, Bool, StringList };

struct Field {
    QString label;
    FieldKind kind = FieldKind::Text;
    QVariant value;
    QStringList options;
    bool required = false;
    bool readOnly = false;
};

bool promptFields(QWidget* parent, const QString& title, QVector<Field>& fields, QString* error) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto* form = new QFormLayout(&dlg);
    QVector<QWidget*> editors;

    for (Field& f : fields) {
        QWidget* editor = nullptr;
        if (f.kind == FieldKind::Bool) {
            auto* cb = new QCheckBox(&dlg);
            cb->setChecked(f.value.toBool());
            cb->setEnabled(!f.readOnly);
            editor = cb;
        } else if (f.kind == FieldKind::Enum) {
            auto* cb = new QComboBox(&dlg);
            cb->addItems(f.options);
            cb->setCurrentText(f.value.toString());
            cb->setEnabled(!f.readOnly);
            editor = cb;
        } else {
            auto* le = new QLineEdit(f.value.toString(), &dlg);
            le->setReadOnly(f.readOnly);
            if (f.kind == FieldKind::Int) le->setValidator(new QIntValidator(1, 1000000, le));
            editor = le;
        }
        form->addRow(f.label + (f.required ? QStringLiteral(" *") : QString()), editor);
        editors.append(editor);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;

    for (int i = 0; i < fields.size(); ++i) {
        Field& f = fields[i];
        QWidget* w = editors.value(i, nullptr);
        if (auto* cb = qobject_cast<QCheckBox*>(w)) f.value = cb->isChecked();
        else if (auto* cb = qobject_cast<QComboBox*>(w)) f.value = cb->currentText();
        else if (auto* le = qobject_cast<QLineEdit*>(w)) f.value = le->text();
    }

    for (const Field& f : fields) {
        if (!f.required || f.kind == FieldKind::Bool) continue;
        if (f.value.toString().trimmed().isEmpty()) {
            if (error) *error = QStringLiteral("必填字段不能为空：") + f.label;
            return false;
        }
    }
    return true;
}

QStringList csvList(const QString& s) {
    QStringList out;
    for (const QString& part : s.split(QChar(','), Qt::SkipEmptyParts)) {
        const QString v = part.trimmed();
        if (!v.isEmpty()) out << v;
    }
    return out;
}

QString listCsv(const QJsonValue& v) {
    QStringList out;
    for (const QJsonValue& item : v.toArray()) out << item.toString();
    return out.join(QStringLiteral(", "));
}

QJsonValue makeValue(const CatalogParameterMeta& meta, const QString& text) {
    if (meta.type == QStringLiteral("stringList")) {
        QJsonArray arr;
        for (const QString& s : csvList(text)) arr.append(s);
        return arr;
    }
    return QJsonValue(text);
}

bool valueEmpty(const CatalogParameterMeta& meta, const QJsonValue& value) {
    return meta.type == QStringLiteral("string")
               ? value.toString().trimmed().isEmpty()
               : value.toArray().isEmpty();
}

int schemaIndex(const CatalogDocument& doc, const QString& name) {
    for (int i = 0; i < doc.parameterSchema.size(); ++i)
        if (doc.parameterSchema.at(i).name == name) return i;
    return -1;
}

void setCell(QTableWidget* table, int row, int col, const QString& text) {
    table->setItem(row, col, new QTableWidgetItem(text));
}

QString commandDetail(const CatalogCommandDef& cmd) {
    QStringList lines;
    lines << QStringLiteral("命令：") + cmd.key;
    for (const CatalogParameterValue& p : cmd.parameters) {
        const QString v = p.value.isArray() ? listCsv(p.value) : p.value.toString();
        lines << QStringLiteral("%1 (%2) = %3").arg(p.name).arg(p.order).arg(v);
    }
    return lines.join(QChar('\n'));
}

} // namespace

CommandCatalogEditor::CommandCatalogEditor(QWidget* parent)
    : QDialog(parent), path_(CommandCatalog::configPath()) {
    setWindowTitle(QStringLiteral("命令目录维护"));
    resize(1180, 760);
    buildUi();
    loadDocument();
}

void CommandCatalogEditor::buildUi() {
    auto* root = new QVBoxLayout(this);

    auto* top = new QHBoxLayout();
    pathLabel_ = new QLabel(this);
    reloadButton_ = new QPushButton(QStringLiteral("重新加载"), this);
    saveButton_ = new QPushButton(QStringLiteral("保存并生效"), this);
    top->addWidget(pathLabel_, 1);
    top->addWidget(reloadButton_);
    top->addWidget(saveButton_);
    root->addLayout(top);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* cmdBox = new QGroupBox(QStringLiteral("命令清单"), splitter);
    auto* cmdLayout = new QVBoxLayout(cmdBox);
    commandTable_ = new QTableWidget(0, 4, cmdBox);
    commandTable_->setHorizontalHeaderLabels({ QStringLiteral("命令名"), QStringLiteral("中文名"),
                                               QStringLiteral("分类"), QStringLiteral("用途") });
    commandTable_->verticalHeader()->setVisible(false);
    commandTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    commandTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cmdLayout->addWidget(commandTable_, 1);
    auto* cmdButtons = new QHBoxLayout();
    QPushButton* cmdAdd = new QPushButton(QStringLiteral("增加命令"), cmdBox);
    QPushButton* cmdDel = new QPushButton(QStringLiteral("删除命令"), cmdBox);
    QPushButton* cmdMod = new QPushButton(QStringLiteral("修改命令"), cmdBox);
    QPushButton* cmdView = new QPushButton(QStringLiteral("查看命令"), cmdBox);
    cmdButtons->addWidget(cmdAdd); cmdButtons->addWidget(cmdDel);
    cmdButtons->addWidget(cmdMod); cmdButtons->addWidget(cmdView);
    cmdLayout->addLayout(cmdButtons);
    splitter->addWidget(cmdBox);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);

    auto* schemaBox = new QGroupBox(QStringLiteral("参数定义（add 形参元数据）"), right);
    auto* schemaLayout = new QVBoxLayout(schemaBox);
    schemaTable_ = new QTableWidget(0, 6, schemaBox);
    schemaTable_->setHorizontalHeaderLabels({ QStringLiteral("顺序"), QStringLiteral("名称"),
                                              QStringLiteral("类型"), QStringLiteral("默认值"),
                                              QStringLiteral("必填"), QStringLiteral("说明") });
    schemaTable_->verticalHeader()->setVisible(false);
    schemaTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    schemaTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    schemaLayout->addWidget(schemaTable_, 1);
    auto* schemaButtons = new QHBoxLayout();
    QPushButton* schAdd = new QPushButton(QStringLiteral("增加参数"), schemaBox);
    QPushButton* schDel = new QPushButton(QStringLiteral("删除参数"), schemaBox);
    QPushButton* schMod = new QPushButton(QStringLiteral("修改参数"), schemaBox);
    QPushButton* schView = new QPushButton(QStringLiteral("查看参数"), schemaBox);
    schemaButtons->addWidget(schAdd); schemaButtons->addWidget(schDel);
    schemaButtons->addWidget(schMod); schemaButtons->addWidget(schView);
    schemaLayout->addLayout(schemaButtons);
    rightLayout->addWidget(schemaBox, 1);

    auto* valueBox = new QGroupBox(QStringLiteral("当前命令的参数取值"), right);
    auto* valueLayout = new QVBoxLayout(valueBox);
    parameterTable_ = new QTableWidget(0, 3, valueBox);
    parameterTable_->setHorizontalHeaderLabels({ QStringLiteral("顺序"), QStringLiteral("名称"),
                                                 QStringLiteral("取值") });
    parameterTable_->verticalHeader()->setVisible(false);
    parameterTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    parameterTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    valueLayout->addWidget(parameterTable_, 1);
    auto* valueButtons = new QHBoxLayout();
    QPushButton* valAdd = new QPushButton(QStringLiteral("增加参数值"), valueBox);
    QPushButton* valDel = new QPushButton(QStringLiteral("删除参数值"), valueBox);
    QPushButton* valMod = new QPushButton(QStringLiteral("修改参数值"), valueBox);
    QPushButton* valView = new QPushButton(QStringLiteral("查看参数值"), valueBox);
    valueButtons->addWidget(valAdd); valueButtons->addWidget(valDel);
    valueButtons->addWidget(valMod); valueButtons->addWidget(valView);
    valueLayout->addLayout(valueButtons);
    rightLayout->addWidget(valueBox, 1);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    root->addWidget(splitter, 1);

    connect(reloadButton_, &QPushButton::clicked, this, &CommandCatalogEditor::loadDocument);
    connect(saveButton_, &QPushButton::clicked, this, &CommandCatalogEditor::saveDocument);
    connect(commandTable_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this] { refreshParameterTable(); updateButtons(); });
    connect(schemaTable_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CommandCatalogEditor::updateButtons);
    connect(parameterTable_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CommandCatalogEditor::updateButtons);

    connect(cmdAdd, &QPushButton::clicked, this, &CommandCatalogEditor::addCommand);
    connect(cmdDel, &QPushButton::clicked, this, &CommandCatalogEditor::deleteCommand);
    connect(cmdMod, &QPushButton::clicked, this, &CommandCatalogEditor::modifyCommand);
    connect(cmdView, &QPushButton::clicked, this, &CommandCatalogEditor::viewCommand);
    connect(schAdd, &QPushButton::clicked, this, &CommandCatalogEditor::addSchemaParameter);
    connect(schDel, &QPushButton::clicked, this, &CommandCatalogEditor::deleteSchemaParameter);
    connect(schMod, &QPushButton::clicked, this, &CommandCatalogEditor::modifySchemaParameter);
    connect(schView, &QPushButton::clicked, this, &CommandCatalogEditor::viewSchemaParameter);
    connect(valAdd, &QPushButton::clicked, this, &CommandCatalogEditor::addCommandParameter);
    connect(valDel, &QPushButton::clicked, this, &CommandCatalogEditor::deleteCommandParameter);
    connect(valMod, &QPushButton::clicked, this, &CommandCatalogEditor::modifyCommandParameter);
    connect(valView, &QPushButton::clicked, this, &CommandCatalogEditor::viewCommandParameter);
}

void CommandCatalogEditor::loadDocument() {
    CommandCatalogConfig cfg;
    if (!cfg.load(path_, &lastError_)) {
        showError(lastError_);
        doc_ = CatalogDocument();
        refreshAll();
        return;
    }
    doc_ = cfg.document();
    lastError_.clear();
    refreshAll();
}

void CommandCatalogEditor::saveDocument() {
    const QString validation = CommandCatalogConfig::validate(doc_);
    if (!validation.isEmpty()) { showError(validation); return; }

    CommandCatalogConfig cfg;
    cfg.document() = doc_;
    if (!cfg.save(path_, &lastError_)) { showError(lastError_); return; }
    if (!CommandCatalog::reload(&lastError_)) { showError(lastError_); return; }
    refreshAll();
    showInfo(QStringLiteral("已保存到：\n%1\n并已重新加载，help 与 AI 提示词立即生效。").arg(path_));
}

void CommandCatalogEditor::refreshAll() {
    pathLabel_->setText(QStringLiteral("配置文件：") + path_);
    refreshCommandTable();
    refreshSchemaTable();
    refreshParameterTable();
    updateButtons();
}

void CommandCatalogEditor::refreshCommandTable() {
    commandTable_->setRowCount(doc_.commands.size());
    for (int i = 0; i < doc_.commands.size(); ++i) {
        const CatalogCommandDef& c = doc_.commands.at(i);
        const CatalogParameterValue* zh = CommandCatalogConfig::findParameter(c, QStringLiteral("zhName"));
        const CatalogParameterValue* cat = CommandCatalogConfig::findParameter(c, QStringLiteral("category"));
        const CatalogParameterValue* sum = CommandCatalogConfig::findParameter(c, QStringLiteral("summary"));
        setCell(commandTable_, i, 0, c.key);
        setCell(commandTable_, i, 1, zh ? zh->value.toString() : QString());
        setCell(commandTable_, i, 2, cat ? cat->value.toString() : QString());
        setCell(commandTable_, i, 3, sum ? sum->value.toString() : QString());
    }
}

void CommandCatalogEditor::refreshSchemaTable() {
    schemaTable_->setRowCount(doc_.parameterSchema.size());
    for (int i = 0; i < doc_.parameterSchema.size(); ++i) {
        const CatalogParameterMeta& m = doc_.parameterSchema.at(i);
        setCell(schemaTable_, i, 0, QString::number(m.order));
        setCell(schemaTable_, i, 1, m.name);
        setCell(schemaTable_, i, 2, m.type);
        setCell(schemaTable_, i, 3, m.defaultValue.isArray() ? listCsv(m.defaultValue) : m.defaultValue.toString());
        setCell(schemaTable_, i, 4, m.required ? QStringLiteral("是") : QStringLiteral("否"));
        setCell(schemaTable_, i, 5, m.description);
    }
}

void CommandCatalogEditor::refreshParameterTable() {
    parameterTable_->setRowCount(0);
    const int row = selectedCommandRow();
    if (row < 0 || row >= doc_.commands.size()) return;
    const CatalogCommandDef& cmd = doc_.commands.at(row);
    parameterTable_->setRowCount(cmd.parameters.size());
    for (int i = 0; i < cmd.parameters.size(); ++i) {
        const CatalogParameterValue& p = cmd.parameters.at(i);
        setCell(parameterTable_, i, 0, QString::number(p.order));
        setCell(parameterTable_, i, 1, p.name);
        setCell(parameterTable_, i, 2, p.value.isArray() ? listCsv(p.value) : p.value.toString());
    }
}

void CommandCatalogEditor::updateButtons() {
    saveButton_->setEnabled(!doc_.commands.isEmpty() && CommandCatalogConfig::validate(doc_).isEmpty());
}

int CommandCatalogEditor::selectedCommandRow() const {
    return commandTable_->currentRow();
}
int CommandCatalogEditor::selectedSchemaRow() const {
    return schemaTable_->currentRow();
}
int CommandCatalogEditor::selectedParameterRow() const {
    return parameterTable_->currentRow();
}

void CommandCatalogEditor::showError(const QString& message) {
    QMessageBox::critical(this, QStringLiteral("命令目录配置错误"), message);
}
void CommandCatalogEditor::showInfo(const QString& message) {
    QMessageBox::information(this, QStringLiteral("命令目录维护"), message);
}

// ---- 命令级 CRUD ----

void CommandCatalogEditor::addCommand() {
    QVector<Field> fields;
    fields.append({ QStringLiteral("命令名 key"), FieldKind::Text, QString(), {}, true, false });
    for (const CatalogParameterMeta& m : doc_.parameterSchema) {
        const bool isName = (m.name == QStringLiteral("name"));
        fields.append({ m.name + QStringLiteral("（") + m.description + QStringLiteral("）"),
                        m.type == QStringLiteral("stringList") ? FieldKind::StringList : FieldKind::Text,
                        isName ? QString() : (m.defaultValue.isArray() ? listCsv(m.defaultValue) : m.defaultValue.toString()),
                        {}, m.required, isName });
    }
    QString err;
    if (!promptFields(this, QStringLiteral("增加命令"), fields, &err)) { showError(err); return; }

    CatalogCommandDef cmd;
    cmd.key = fields.value(0).value.toString().trimmed();
    if (CommandCatalogConfig::findCommand(doc_, cmd.key)) { showError(QStringLiteral("命令名已存在：") + cmd.key); return; }
    cmd.key = cmd.key.toLower();
    for (int i = 0; i < doc_.parameterSchema.size(); ++i) {
        const CatalogParameterMeta& m = doc_.parameterSchema.at(i);
        CatalogParameterValue pv;
        pv.order = m.order;
        pv.name = m.name;
        pv.value = (m.name == QStringLiteral("name")) ? QJsonValue(cmd.key)
                                                      : makeValue(m, fields.value(i + 1).value.toString());
        if (m.required && valueEmpty(m, pv.value)) {
            showError(QStringLiteral("必填参数为空：") + m.name);
            return;
        }
        cmd.parameters.append(pv);
    }
    doc_.commands.append(cmd);
    refreshAll();
}

void CommandCatalogEditor::deleteCommand() {
    const int row = selectedCommandRow();
    if (row < 0) return;
    const QString key = doc_.commands.at(row).key;
    if (QMessageBox::question(this, QStringLiteral("删除命令"),
                              QStringLiteral("确认删除命令 %1？").arg(key)) != QMessageBox::Yes) return;
    doc_.commands.removeAt(row);
    refreshAll();
}

void CommandCatalogEditor::modifyCommand() {
    const int row = selectedCommandRow();
    if (row < 0) return;
    CatalogCommandDef& cmd = doc_.commands[row];
    const QString oldKey = cmd.key;

    QVector<Field> fields;
    fields.append({ QStringLiteral("命令名 key"), FieldKind::Text, oldKey, {}, true, false });
    for (const CatalogParameterMeta& m : doc_.parameterSchema) {
        const CatalogParameterValue* pv = CommandCatalogConfig::findParameter(cmd, m.name);
        const bool isName = (m.name == QStringLiteral("name"));
        fields.append({ m.name + QStringLiteral("（") + m.description + QStringLiteral("）"),
                        m.type == QStringLiteral("stringList") ? FieldKind::StringList : FieldKind::Text,
                        isName ? QVariant(oldKey)
                               : QVariant(pv ? (pv->value.isArray() ? listCsv(pv->value) : pv->value.toString()) : QString()),
                        {}, m.required, isName });
    }
    QString err;
    if (!promptFields(this, QStringLiteral("修改命令"), fields, &err)) { showError(err); return; }
    const QString newKey = fields.value(0).value.toString().trimmed().toLower();
    if (newKey != oldKey && CommandCatalogConfig::findCommand(doc_, newKey)) {
        showError(QStringLiteral("命令名已存在：") + newKey); return;
    }
    CatalogCommandDef updated;
    updated.key = newKey;
    for (int i = 0; i < doc_.parameterSchema.size(); ++i) {
        const CatalogParameterMeta& m = doc_.parameterSchema.at(i);
        CatalogParameterValue pv;
        pv.order = m.order;
        pv.name = m.name;
        pv.value = (m.name == QStringLiteral("name")) ? QJsonValue(newKey)
                                                      : makeValue(m, fields.value(i + 1).value.toString());
        if (m.required && valueEmpty(m, pv.value)) { showError(QStringLiteral("必填参数为空：") + m.name); return; }
        updated.parameters.append(pv);
    }
    doc_.commands[row] = updated;
    refreshAll();
}

void CommandCatalogEditor::viewCommand() {
    const int row = selectedCommandRow();
    if (row < 0) return;
    showInfo(commandDetail(doc_.commands.at(row)));
}

// ---- 参数定义（schema）CRUD ----

void CommandCatalogEditor::addSchemaParameter() {
    QVector<Field> fields = {
        { QStringLiteral("顺序 order"), FieldKind::Int, doc_.parameterSchema.size() + 1, {}, true, false },
        { QStringLiteral("名称 name"), FieldKind::Text, QString(), {}, true, false },
        { QStringLiteral("类型 type"), FieldKind::Enum, QStringLiteral("string"),
          { QStringLiteral("string"), QStringLiteral("stringList") }, true, false },
        { QStringLiteral("默认值 default"), FieldKind::Text, QString(), {}, false, false },
        { QStringLiteral("是否必填 required"), FieldKind::Bool, false, {}, false, false },
        { QStringLiteral("说明 description"), FieldKind::Text, QString(), {}, true, false },
    };
    QString err;
    if (!promptFields(this, QStringLiteral("增加参数定义"), fields, &err)) { showError(err); return; }
    CatalogParameterMeta m;
    m.order = fields.value(0).value.toInt();
    m.name = fields.value(1).value.toString().trimmed();
    m.type = fields.value(2).value.toString();
    m.defaultValue = makeValue(m, fields.value(3).value.toString());
    m.required = fields.value(4).value.toBool();
    m.description = fields.value(5).value.toString().trimmed();
    if (schemaIndex(doc_, m.name) >= 0) { showError(QStringLiteral("参数名已存在：") + m.name); return; }
    doc_.parameterSchema.append(m);
    std::stable_sort(doc_.parameterSchema.begin(), doc_.parameterSchema.end(),
                     [](const CatalogParameterMeta& a, const CatalogParameterMeta& b) { return a.order < b.order; });
    for (CatalogCommandDef& c : doc_.commands)
        c.parameters.append({ m.order, m.name, m.defaultValue });
    refreshAll();
}

void CommandCatalogEditor::deleteSchemaParameter() {
    const int row = selectedSchemaRow();
    if (row < 0 || doc_.parameterSchema.size() <= 1) { showError(QStringLiteral("至少要保留一个参数定义")); return; }
    const QString name = doc_.parameterSchema.at(row).name;
    if (QMessageBox::question(this, QStringLiteral("删除参数定义"),
                              QStringLiteral("将同步删除所有命令中的参数值：%1，确认？").arg(name)) != QMessageBox::Yes) return;
    doc_.parameterSchema.removeAt(row);
    for (CatalogCommandDef& c : doc_.commands) {
        for (int i = c.parameters.size() - 1; i >= 0; --i)
            if (c.parameters.at(i).name == name) c.parameters.removeAt(i);
    }
    refreshAll();
}

void CommandCatalogEditor::modifySchemaParameter() {
    const int row = selectedSchemaRow();
    if (row < 0) return;
    const CatalogParameterMeta old = doc_.parameterSchema.at(row);
    QVector<Field> fields = {
        { QStringLiteral("顺序 order"), FieldKind::Int, old.order, {}, true, false },
        { QStringLiteral("名称 name"), FieldKind::Text, old.name, {}, true, false },
        { QStringLiteral("类型 type"), FieldKind::Enum, old.type,
          { QStringLiteral("string"), QStringLiteral("stringList") }, true, false },
        { QStringLiteral("默认值 default"), FieldKind::Text,
          old.defaultValue.isArray() ? listCsv(old.defaultValue) : old.defaultValue.toString(), {}, false, false },
        { QStringLiteral("是否必填 required"), FieldKind::Bool, old.required, {}, false, false },
        { QStringLiteral("说明 description"), FieldKind::Text, old.description, {}, true, false },
    };
    QString err;
    if (!promptFields(this, QStringLiteral("修改参数定义"), fields, &err)) { showError(err); return; }
    CatalogParameterMeta updated;
    updated.order = fields.value(0).value.toInt();
    updated.name = fields.value(1).value.toString().trimmed();
    updated.type = fields.value(2).value.toString();
    updated.defaultValue = makeValue(updated, fields.value(3).value.toString());
    updated.required = fields.value(4).value.toBool();
    updated.description = fields.value(5).value.toString().trimmed();
    const int dup = schemaIndex(doc_, updated.name);
    if (dup >= 0 && dup != row) { showError(QStringLiteral("参数名已存在：") + updated.name); return; }

    doc_.parameterSchema[row] = updated;
    for (CatalogCommandDef& c : doc_.commands) {
        for (CatalogParameterValue& pv : c.parameters) {
            if (pv.name != old.name) continue;
            pv.name = updated.name;
            pv.order = updated.order;
            if (old.type != updated.type)
                pv.value = makeValue(updated, pv.value.isArray() ? listCsv(pv.value) : pv.value.toString());
        }
    }
    std::stable_sort(doc_.parameterSchema.begin(), doc_.parameterSchema.end(),
                     [](const CatalogParameterMeta& a, const CatalogParameterMeta& b) { return a.order < b.order; });
    refreshAll();
}

void CommandCatalogEditor::viewSchemaParameter() {
    const int row = selectedSchemaRow();
    if (row < 0) return;
    const CatalogParameterMeta& m = doc_.parameterSchema.at(row);
    showInfo(QStringLiteral("顺序：%1\n名称：%2\n类型：%3\n默认值：%4\n必填：%5\n说明：%6")
                 .arg(m.order).arg(m.name, m.type,
                                   m.defaultValue.isArray() ? listCsv(m.defaultValue) : m.defaultValue.toString(),
                                   m.required ? QStringLiteral("是") : QStringLiteral("否"), m.description));
}

// ---- 命令参数取值 CRUD ----

void CommandCatalogEditor::addCommandParameter() {
    const int cmdRow = selectedCommandRow();
    if (cmdRow < 0) return;
    CatalogCommandDef& cmd = doc_.commands[cmdRow];
    QStringList available;
    for (const CatalogParameterMeta& m : doc_.parameterSchema)
        if (!CommandCatalogConfig::findParameter(cmd, m.name)) available << m.name;
    if (available.isEmpty()) { showError(QStringLiteral("该命令已包含全部参数定义")); return; }

    QVector<Field> fields = {
        { QStringLiteral("参数名"), FieldKind::Enum, available.first(), available, true, false },
        { QStringLiteral("取值"), FieldKind::Text, QString(), {}, false, false },
    };
    QString err;
    if (!promptFields(this, QStringLiteral("增加参数值"), fields, &err)) { showError(err); return; }
    const QString name = fields.value(0).value.toString();
    const int si = schemaIndex(doc_, name);
    if (si < 0) { showError(QStringLiteral("未知参数：") + name); return; }
    const CatalogParameterMeta& meta = doc_.parameterSchema.at(si);
    CatalogParameterValue pv{meta.order, name, makeValue(meta, fields.value(1).value.toString())};
    if (meta.required && valueEmpty(meta, pv.value)) { showError(QStringLiteral("必填参数为空：") + name); return; }
    cmd.parameters.append(pv);
    refreshAll();
}

void CommandCatalogEditor::deleteCommandParameter() {
    const int cmdRow = selectedCommandRow(), parRow = selectedParameterRow();
    if (cmdRow < 0 || parRow < 0) return;
    doc_.commands[cmdRow].parameters.removeAt(parRow);
    refreshAll();
}

void CommandCatalogEditor::modifyCommandParameter() {
    const int cmdRow = selectedCommandRow(), parRow = selectedParameterRow();
    if (cmdRow < 0 || parRow < 0) return;
    CatalogParameterValue& pv = doc_.commands[cmdRow].parameters[parRow];
    const int si = schemaIndex(doc_, pv.name);
    if (si < 0) { showError(QStringLiteral("未知参数：") + pv.name); return; }
    const CatalogParameterMeta& meta = doc_.parameterSchema.at(si);

    QVector<Field> fields = {
        { QStringLiteral("顺序"), FieldKind::Int, pv.order, {}, true, true },
        { QStringLiteral("参数名"), FieldKind::Enum, pv.name,
          [this] { QStringList n; for (const auto& m : doc_.parameterSchema) n << m.name; return n; }(), true, false },
        { QStringLiteral("取值"), FieldKind::Text,
          pv.value.isArray() ? listCsv(pv.value) : pv.value.toString(), {}, false, false },
    };
    QString err;
    if (!promptFields(this, QStringLiteral("修改参数值"), fields, &err)) { showError(err); return; }
    const QString name = fields.value(1).value.toString();
    const int nsi = schemaIndex(doc_, name);
    if (nsi < 0) { showError(QStringLiteral("未知参数：") + name); return; }
    const CatalogParameterMeta& nmeta = doc_.parameterSchema.at(nsi);
    QJsonValue newValue = makeValue(nmeta, fields.value(2).value.toString());
    if (nmeta.required && valueEmpty(nmeta, newValue)) { showError(QStringLiteral("必填参数为空：") + name); return; }
    pv.name = name;
    pv.order = nmeta.order;
    pv.value = newValue;
    refreshAll();
}

void CommandCatalogEditor::viewCommandParameter() {
    const int cmdRow = selectedCommandRow(), parRow = selectedParameterRow();
    if (cmdRow < 0 || parRow < 0) return;
    const CatalogParameterValue& pv = doc_.commands.at(cmdRow).parameters.at(parRow);
    showInfo(QStringLiteral("顺序：%1\n名称：%2\n取值：%3")
                 .arg(pv.order).arg(pv.name,
                                    pv.value.isArray() ? listCsv(pv.value) : pv.value.toString()));
}

} // namespace SketchCAD
