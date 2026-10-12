#pragma once
#include <QString>
#include <QStringList>
#include <map>
#include <functional>
#include "Vec.h"
#include "Element.h"
#include "ElementFactory.h"
#include "Commands.h"

namespace SketchCAD {

class SketchModel;
class AppSettings;

/// 文本命令行解释器。
/// 重构后采用命令模式 + 注册表分派：execute() 不再包含 if-else 链，
/// 而是通过 CommandRegistry 查找对应的 ICommand 并委托执行。
/// 新增命令只需创建命令类并在 CommandRegistry::registerBuiltins() 注册。
class CommandInterpreter {
public:
    explicit CommandInterpreter(SketchModel* m) : model_(m) {}

    bool execute(const QString& line);                     ///< 返回是否成功执行
    QStringList completions(const QString& prefix) const;  ///< Tab 自动补全候选
    const QStringList& history() const { return history_; }
    /// 上一次成功执行的（别名解析后的）命令名，供空输入按 Enter/Space 重复。
    const QString& lastCommand() const { return lastCommand_; }

    /// 未知命令容错（AutoCAD "Did you mean?" 风格）：在 names 中找与 cmd
    /// 编辑距离最小、距离不超过阈值（len/2，下限 2）的至多 maxCount 个候选，
    /// 距离升序、同距离保持注册顺序。公有静态：供 execute 提示与无头自测直测。
    static QStringList nearestCommands(const QString& cmd, const QStringList& names,
                                       int maxCount = 3);

    // ---- 历史持久化（AppSettings 注入，可空=不持久化）----
    // 注入后：① 立即把已持久化的最近命令预填进当前会话历史（仅当会话历史为空，
    // 不覆盖 DocTab copy-in 的会话态）；② 此后每次 addHistory 同步写回
    // AppSettings（commandLine/recentCommands，尾部 ≤50 条）。重启后 ↑↓ 可用。
    void setPersistence(AppSettings* s);
    static QStringList persistedCommands(const AppSettings& s);   // 读持久化（供自测/装配）
    static constexpr int kPersistMax = 50;                        // 持久化条数上限

    // ---- 多文档切换重绑（DocTab）----
    // 解释器是共享单件；切换活动文档时由 MainWindow 换绑模型，并做历史的
    // copy-out / copy-in（历史与 lastCommand 属文档会话态，见 ui/DocTab.h）。
    void setModel(SketchModel* m) { model_ = m; }
    void setHistory(const QStringList& h) { history_ = h; }
    void setLastCommand(const QString& c) { lastCommand_ = c; }

    // ---- 外部命令（插件经 SDK 注册）----
    // 注册一条由插件提供的命令。name 为小写命令名（不含空格）：
    //   - 与内置命令重名 → 拒绝（返回 false，内置优先）
    //   - 与已注册外部命令重名 → 拒绝（返回 false）
    // 注册后命令行输入该 name 即调用 fn（参数为命令名之后的剩余文本）。
    bool registerExternalCommand(const QString& name, std::function<bool(const QString&)> fn);
    // 当前已注册的外部命令名列表（供 Tab 补全）。
    QStringList externalCommandNames() const;
    void addHistory(const QString& line);
    static QStringList commandNames();
    /// 公开点解析：纯解析（不换算，无 UCS 语义）。UCS 感知入口在
    /// CommandContext::tryParsePoint（成员，经 model->toWorld）与
    /// SketchModel::parsePointToWorld（CommandLine 分段式，含 @/极坐标语法）。
    static bool tryParsePoint(const QString& s, Vec3& out) {
        return CommandContext::tryParsePointRaw(s, out);
    }

private:
    SketchModel* model_;
    QStringList history_;
    QString lastCommand_;   // 上一次成功执行的命令名（别名展开后），供重复执行
    AppSettings* persist_ = nullptr;   // 历史持久化后端（可空=不持久化）
    // 外部命令表（插件注册）：命令名(小写) → 处理函数。execute() 在内置命令
    // 查找失败后回退到此表；同名冲突时内置优先、外部去重。
    std::map<QString, std::function<bool(const QString&)>> externalCommands_;

    /// 直线子模式的命令名（用于 commandNames / completions）
    static const QStringList& lineSubModeNames();
    /// 样条家族命令名（绘制 + 编辑，用于 commandNames / completions）
    static const QStringList& splineCommandNames();
    /// 椭圆家族命令名（用于 commandNames / completions）
    static const QStringList& ellipseCommandNames();
    /// 折线家族命令名（用于 commandNames / completions）
    static const QStringList& polylineCommandNames();
    /// 选择家族命令名（用于 commandNames / completions）
    static const QStringList& selectCommandNames();
};

} // namespace SketchCAD
