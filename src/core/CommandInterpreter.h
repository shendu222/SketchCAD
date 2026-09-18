#pragma once
#include <QString>
#include <QStringList>
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
    void addHistory(const QString& line);
    static QStringList commandNames();
    /// 公开点解析：供 CommandLine 在交互模式下判断输入是否为坐标
    static bool tryParsePoint(const QString& s, Vec3& out) {
        return CommandContext::tryParsePoint(s, out);
    }

private:
    SketchModel* model_;
    QStringList history_;

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
