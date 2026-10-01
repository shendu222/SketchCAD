#include "CuiManager.h"
#include <QFile>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace SketchCAD {

CuiManager& CuiManager::instance() {
    static CuiManager s;
    return s;
}

void CuiManager::clear() {
    toolbars_.clear();
    workspaces_.clear();
    currentWs_.clear();
    filePath_.clear();
    // 内置注册表保留（它们描述应用固有 UI，不随文件清空而丢失）
}

void CuiManager::registerBuiltinToolbar(const QString& id) {
    if (!builtinToolbars_.contains(id)) builtinToolbars_ << id;
}
void CuiManager::registerBuiltinMenu(const QString& id) {
    if (!builtinMenus_.contains(id)) builtinMenus_ << id;
}

CuiToolbar* CuiManager::findToolbar(const QString& id) {
    for (auto& t : toolbars_) if (t.id == id) return &t;
    return nullptr;
}
const CuiToolbar* CuiManager::findToolbar(const QString& id) const {
    for (const auto& t : toolbars_) if (t.id == id) return &t;
    return nullptr;
}
const CuiToolbar* CuiManager::toolbar(const QString& id) const {
    for (const auto& t : toolbars_) if (t.id == id) return &t;
    return nullptr;
}

bool CuiManager::hasToolbar(const QString& id) const { return findToolbar(id) != nullptr; }
QStringList CuiManager::toolbarIds() const {
    QStringList r;
    for (const auto& t : toolbars_) r << t.id;
    return r;
}

void CuiManager::addToolbar(const QString& id, const QString& title) {
    if (hasToolbar(id)) return;
    CuiToolbar t;
    t.id = id;
    t.title = title;
    toolbars_.append(t);
}
bool CuiManager::removeToolbar(const QString& id) {
    for (int i = 0; i < toolbars_.size(); ++i) {
        if (toolbars_[i].id == id) {
            toolbars_.removeAt(i);
            // 同步从所有工作空间中移除该工具条引用
            for (auto& w : workspaces_) w.toolbars.removeAll(id);
            return true;
        }
    }
    return false;
}
void CuiManager::addButton(const QString& toolbarId, const CuiButton& b) {
    CuiToolbar* t = findToolbar(toolbarId);
    if (!t) { addToolbar(toolbarId, toolbarId); t = findToolbar(toolbarId); }
    t->buttons.append(b);
}
bool CuiManager::removeButton(const QString& toolbarId, int index) {
    CuiToolbar* t = findToolbar(toolbarId);
    if (!t) return false;
    if (index < 0 || index >= t->buttons.size()) return false;
    t->buttons.removeAt(index);
    return true;
}

bool CuiManager::hasWorkspace(const QString& id) const {
    for (const auto& w : workspaces_) if (w.id == id) return true;
    return false;
}
const CuiWorkspace* CuiManager::workspace(const QString& id) const {
    for (const auto& w : workspaces_) if (w.id == id) return &w;
    return nullptr;
}
QStringList CuiManager::workspaceIds() const {
    QStringList r;
    for (const auto& w : workspaces_) r << w.id;
    return r;
}
QStringList CuiManager::workspaceTitles() const {
    QStringList r;
    for (const auto& w : workspaces_) r << w.title;
    return r;
}
void CuiManager::addWorkspace(const QString& id, const QString& title,
                              const QStringList& toolbars, const QStringList& menus) {
    CuiWorkspace w;
    w.id = id; w.title = title;
    w.toolbars = toolbars; w.menus = menus;
    workspaces_.append(w);
}
bool CuiManager::removeWorkspace(const QString& id) {
    for (int i = 0; i < workspaces_.size(); ++i) {
        if (workspaces_[i].id == id) {
            workspaces_.removeAt(i);
            if (currentWs_ == id) currentWs_.clear();
            return true;
        }
    }
    return false;
}

void CuiManager::setCurrentWorkspace(const QString& id) {
    currentWs_ = hasWorkspace(id) ? id : QString();
}

bool CuiManager::toolbarVisible(const QString& id) const {
    if (currentWs_.isEmpty()) return true;              // 默认：全部可见
    const CuiWorkspace* w = workspace(currentWs_);
    return w ? w->toolbars.contains(id) : true;
}
bool CuiManager::menuVisible(const QString& id) const {
    if (currentWs_.isEmpty()) return true;
    const CuiWorkspace* w = workspace(currentWs_);
    return w ? w->menus.contains(id) : true;
}

// ---------------------------------------------------------------------------
// XML (.cuix) 读写
// ---------------------------------------------------------------------------
bool CuiManager::load(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开 CUIx 文件: ") + path;
        return false;
    }
    clear();

    QXmlStreamReader xml(&f);
    QString curToolbar;          // 当前正在填充的工具条 id
    QString curWorkspace;        // 当前正在填充的工作空间 id
    QString curSection;          // "toolbars" / "menus"（工作空间内）

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            QXmlStreamAttributes a = xml.attributes();
            if (name == "Toolbar") {
                QString id = a.value("id").toString();
                QString title = a.value("title").toString();
                if (!id.isEmpty()) { addToolbar(id, title.isEmpty() ? id : title); curToolbar = id; }
            } else if (name == "Button" && !curToolbar.isEmpty()) {
                CuiButton b;
                b.command = a.value("command").toString();
                b.label = a.value("label").toString();
                b.icon = a.value("icon").toString();
                if (!b.command.isEmpty()) addButton(curToolbar, b);
            } else if (name == "Workspace") {
                QString id = a.value("id").toString();
                QString title = a.value("title").toString();
                if (!id.isEmpty()) { addWorkspace(id, title.isEmpty() ? id : title, {}, {}); curWorkspace = id; }
            } else if (name == "Toolbars") {
                curSection = "toolbars";
            } else if (name == "Menus") {
                curSection = "menus";
            } else if (name == "Ref" && !curWorkspace.isEmpty()) {
                QString rid = a.value("id").toString();
                if (!rid.isEmpty()) {
                    CuiWorkspace* w = nullptr;
                    for (auto& x : workspaces_) if (x.id == curWorkspace) w = &x;
                    if (w) { if (curSection == "menus") w->menus << rid; else w->toolbars << rid; }
                }
            } else if (name == "Workspaces") {
                QString cur = a.value("current").toString();
                if (!cur.isEmpty()) currentWs_ = cur;
            }
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name == "Toolbar") curToolbar.clear();
            else if (name == "Workspace") curWorkspace.clear();
            else if (name == "Toolbars" || name == "Menus") curSection.clear();
        }
    }
    if (xml.hasError()) {
        if (err) *err = QStringLiteral("CUIx XML 解析错误: ") + xml.errorString();
        return false;
    }
    filePath_ = path;
    return true;
}

bool CuiManager::save(const QString& path, QString* err) {
    QString target = path.isEmpty() ? filePath_ : path;
    if (target.isEmpty()) {
        if (err) *err = QStringLiteral("未指定 CUIx 文件路径");
        return false;
    }
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法写入 CUIx 文件: ") + target;
        return false;
    }
    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    // 注意：Qt6 的 QXmlStreamReader 对带 encoding 属性的 XML 声明较严格，
    // 会报 "Unsupported XML version"。省略 encoding 属性（写入器默认即 UTF-8），
    // 既兼容解析又保留中文等高字节内容。
    xml.writeStartDocument();
    xml.writeStartElement("SketchCADCUI");
    xml.writeAttribute("version", "1.0");

    // ---- 工具条 ----
    xml.writeStartElement("Toolbars");
    for (const auto& t : toolbars_) {
        xml.writeStartElement("Toolbar");
        xml.writeAttribute("id", t.id);
        xml.writeAttribute("title", t.title);
        for (const auto& b : t.buttons) {
            xml.writeStartElement("Button");
            xml.writeAttribute("command", b.command);
            if (!b.label.isEmpty()) xml.writeAttribute("label", b.label);
            if (!b.icon.isEmpty())  xml.writeAttribute("icon", b.icon);
            xml.writeEndElement();
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();

    // ---- 工作空间 ----
    xml.writeStartElement("Workspaces");
    if (!currentWs_.isEmpty()) xml.writeAttribute("current", currentWs_);
    for (const auto& w : workspaces_) {
        xml.writeStartElement("Workspace");
        xml.writeAttribute("id", w.id);
        xml.writeAttribute("title", w.title);
        xml.writeStartElement("Toolbars");
        for (const QString& id : w.toolbars) {
            xml.writeStartElement("Ref"); xml.writeAttribute("id", id); xml.writeEndElement();
        }
        xml.writeEndElement();
        xml.writeStartElement("Menus");
        for (const QString& id : w.menus) {
            xml.writeStartElement("Ref"); xml.writeAttribute("id", id); xml.writeEndElement();
        }
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();
    filePath_ = target;
    return true;
}

} // namespace SketchCAD
