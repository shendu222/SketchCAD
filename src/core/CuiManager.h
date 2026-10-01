#pragma once
// ============================================================
// CuiManager —— 界面定制（二次开发生态，CUIx 兼容）
//
// 设计要点（详见 docs/cui-alias-design-analysis.md）：
//   1. 纯 C++ 数据模型 + XML(.cuix) 读写，无 QtWidgets 依赖，可无头单测。
//   2. 数据三类：
//       - Toolbar：自定义工具条（id/title + 按钮列表）；按钮 = 命令/工具 token + 可选文字/图标。
//       - Workspace：工作空间（id/title + 可见工具条 id 列表 + 可见菜单 id 列表）。
//       - 内置 UI 元素注册表：MainWindow 启动时登记内置工具条/菜单 id，
//         供工作空间「默认全显」与「保存当前视图」使用。
//   3. 可见性规则：currentWs 为空（默认）→ 全部可见；否则以工作空间的列表为准。
//      MainWindow 据此 show/hide 对应 QToolBar / QMenu，数据层不触碰任何 Qt 控件。
//   4. 单例：进程级唯一界面配置。
// ============================================================
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

namespace SketchCAD {

/// 工具条按钮：command 为工具 id（如 "line"）或命令名（如 "save"）；
/// label/icon 为空时由 MainWindow 取默认值（工具 displayName / 默认图标）。
struct CuiButton {
    QString command;
    QString label;
    QString icon;
};

/// 自定义工具条定义。
struct CuiToolbar {
    QString id;       // objectName（如 "myBar"）
    QString title;    // 显示标题
    QList<CuiButton> buttons;
};

/// 工作空间定义：记录当前应可见的工具条与菜单（id 均为 objectName）。
struct CuiWorkspace {
    QString id;
    QString title;
    QStringList toolbars;   // 可见工具条 id
    QStringList menus;      // 可见菜单 id（= 顶级菜单 objectName）
};

class CuiManager {
public:
    static CuiManager& instance();

    bool load(const QString& path, QString* err = nullptr);
    bool save(const QString& path = QString(), QString* err = nullptr);
    void clear();

    // ---- 内置 UI 元素注册（由 MainWindow 在启动时调用）----
    void registerBuiltinToolbar(const QString& id);
    void registerBuiltinMenu(const QString& id);
    const QStringList& builtinToolbars() const { return builtinToolbars_; }
    const QStringList& builtinMenus() const { return builtinMenus_; }

    // ---- 工具条 CRUD ----
    bool hasToolbar(const QString& id) const;
    const CuiToolbar* toolbar(const QString& id) const;
    void addToolbar(const QString& id, const QString& title);
    bool removeToolbar(const QString& id);
    void addButton(const QString& toolbarId, const CuiButton& b);
    bool removeButton(const QString& toolbarId, int index);
    QStringList toolbarIds() const;
    const QList<CuiToolbar>& toolbars() const { return toolbars_; }

    // ---- 工作空间 CRUD ----
    bool hasWorkspace(const QString& id) const;
    const CuiWorkspace* workspace(const QString& id) const;
    void addWorkspace(const QString& id, const QString& title,
                      const QStringList& toolbars, const QStringList& menus);
    bool removeWorkspace(const QString& id);
    QStringList workspaceIds() const;
    QStringList workspaceTitles() const;

    // ---- 当前工作空间与可见性 ----
    void setCurrentWorkspace(const QString& id);
    QString currentWorkspace() const { return currentWs_; }
    /// 工具条是否应可见：无当前工作空间→全部可见；否则以工作空间列表为准。
    bool toolbarVisible(const QString& id) const;
    bool menuVisible(const QString& id) const;

    QString filePath() const { return filePath_; }
    void setFilePath(const QString& p) { filePath_ = p; }

    // ---- UI 刷新回调（由 MainWindow 注册）：命令改动界面配置后请求实时刷新 ----
    using RefreshFn = std::function<void()>;
    void setRefreshCallback(RefreshFn fn) { refresh_ = std::move(fn); }
    void requestRefresh() const { if (refresh_) refresh_(); }

private:
    CuiManager() = default;
    CuiToolbar* findToolbar(const QString& id);
    const CuiToolbar* findToolbar(const QString& id) const;

    QList<CuiToolbar> toolbars_;
    QList<CuiWorkspace> workspaces_;
    QStringList builtinToolbars_;
    QStringList builtinMenus_;
    QString currentWs_;
    QString filePath_;
    RefreshFn refresh_;
};

} // namespace SketchCAD
