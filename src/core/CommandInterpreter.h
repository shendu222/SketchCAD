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
    /// 公开点解析：供 CommandLine 在交互模式下判断输入是否为坐标
    static bool tryParsePoint(const QString& s, Vec3& out) {
        return CommandContext::tryParsePoint(s, out);
    }

private:
    SketchModel* model_;
    QStringList history_;
    QString lastCommand_;   // 上一次成功执行的命令名（别名展开后），供重复执行
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
