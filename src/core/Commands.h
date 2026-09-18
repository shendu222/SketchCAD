#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <memory>
#include <functional>
#include <utility>
#include <vector>
#include "Vec.h"
#include "Element.h"
#include "ElementFactory.h"

namespace SketchCAD {

class SketchModel;

// ============================================================
// 命令模式基础设施：ICommand 接口 + CommandContext + CommandRegistry
// ============================================================

/// 所有命令共享的执行上下文。把原来 if-else 链中反复定义的
/// parsePts / parseRefs / makeLine lambda 收敛为成员方法（DRY）。
struct CommandContext {
    SketchModel* model;          ///< 数据模型（addElement / log / element …）
    const QStringList& toks;     ///< 已分词的命令行输入
    const QString& cmd;          ///< 命令名（toks[0].toLower()）

    // ---- 公共解析工具（消除重复 lambda） ----
    bool parsePoint(int idx, Vec3& out) const;             ///< 解析 toks[idx] 为 Vec3
    std::vector<Vec3> parsePts(int start, int n) const;    ///< 从 start 连续解析 n 个点
    std::vector<int>  parseRefs(int start, int n) const;   ///< 从 start 连续解析 n 个图元 ID
    void makeLine(const Vec3& a, const Vec3& b) const;     ///< 创建直线并记录日志
    static bool tryParsePoint(const QString& s, Vec3& out);///< 字符串 → Vec3（静态，供外部调用）
};

/// 命令接口（Command Pattern）
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual bool execute(const CommandContext& ctx) = 0;
};

/// Lambda 包装器：用于 undo / redo / clear 等一行命令，
/// 避免为每个一行命令单独建类。
class LambdaCommand : public ICommand {
    std::function<bool(const CommandContext&)> fn_;
public:
    explicit LambdaCommand(std::function<bool(const CommandContext&)> fn)
        : fn_(std::move(fn)) {}
    bool execute(const CommandContext& ctx) override { return fn_(ctx); }
};

/// 命令注册表（Factory + Singleton）。
/// 启动时自动注册所有内置命令；CommandInterpreter::execute()
/// 只需查找并分派，不再有 if-else 链。
/// 支持精确匹配（绝大多数命令）和前缀匹配（line_* 系列）。
class CommandRegistry {
public:
    static CommandRegistry& instance();

    void registerExact(const QString& name, std::shared_ptr<ICommand> cmd);
    void registerPrefix(const QString& prefix, std::shared_ptr<ICommand> cmd);

    ICommand* findExact(const QString& name) const;
    ICommand* findPrefixMatch(const QString& name) const;
    QStringList exactNames() const;

private:
    CommandRegistry();                 ///< 构造时注册全部内置命令
    void registerBuiltins();

    QMap<QString, std::shared_ptr<ICommand>> exact_;
    QVector<QPair<QString, std::shared_ptr<ICommand>>> prefixes_;
};

// ============================================================
// 具体命令声明
// ============================================================

// ---- 基础绘图 ----
class LineCommand    : public ICommand { public: bool execute(const CommandContext&) override; };
class CircleCommand  : public ICommand { public: bool execute(const CommandContext&) override; };
class Circle3PCommand: public ICommand { public: bool execute(const CommandContext&) override; };
class ArcCommand     : public ICommand { public: bool execute(const CommandContext&) override; };
class RectCommand    : public ICommand { public: bool execute(const CommandContext&) override; };

/// poly / spline 逻辑相同，仅类型和名称不同 —— 构造时注入。
class PolySplineCommand : public ICommand {
    ElementType type_;
    QString name_;
public:
    PolySplineCommand(ElementType t, const QString& name) : type_(t), name_(name) {}
    bool execute(const CommandContext&) override;
};

// ---- 直线子模式（line_* 前缀家族） ----
class LineModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 圆弧子模式（arc_* 前缀家族）----
class ArcModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 多边形子模式（poly_* / rect_chamfer / rect_fillet 前缀家族）----
class PolyModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// 样条子模式命令（spline_parabola / spline_hyperbola）
class SplineModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 椭圆子模式（ellipse_* 前缀家族）----
// 与 UI 共用 ellipse::compute；命令行没有真实光标，故把最后一个点当作光标位置。
class EllipseModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 折线族：精确 "polyline" + 前缀 "polyline_" 9 个编辑命令 ----
// 全部走同一个 PolylineModeCommand，按 ctx.toks[0] 分派到具体实现。
class PolylineModeCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 编辑 ----
class DeleteCommand : public ICommand { public: bool execute(const CommandContext&) override; };
class TrimCommand   : public ICommand { public: bool execute(const CommandContext&) override; };
class ListCommand   : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 修改菜单（Modify）-- 单一 ModifyCommand 按 ctx.cmd 分派所有 modify_* 子命令
//  覆盖：move, copy, duplicate, rotate, scale, mirror, revert, lengthen,
//        offset, break_at, divide, fillet, bevel, explode, align, stretch,
//        plus "delete <id>" 作为保留的精确命令（已有 DeleteCommand 接管）
// ------------------------------------------------------------
class ModifyCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 视图 ----
class PlaneCommand : public ICommand { public: bool execute(const CommandContext&) override; };
class GridCommand  : public ICommand { public: bool execute(const CommandContext&) override; };
class ZoomCommand  : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 样式 ----
class WidthCommand : public ICommand { public: bool execute(const CommandContext&) override; };
class ColorCommand : public ICommand { public: bool execute(const CommandContext&) override; };
class ChainCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 标注 / 约束 ----
class DimCommand       : public ICommand { public: bool execute(const CommandContext&) override; };
class DimSmartCommand  : public ICommand { public: bool execute(const CommandContext&) override; };
class ConstraintCommand: public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 选择（对应选择菜单 / FreeCAD Sketcher 选择命令）----
// 同一类按 ctx.cmd 分派：select_all / select_none / invert /
// select_entity <id> / deselect_entity <id> / select_contour <id> /
// select_intersected <id> / deselect_intersected <id> /
// select_layer <name> / deselect_layer <name>
class SelectCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 图层管理 ----
// 按 ctx.cmd 分派（layer 为顶层精确命令）：
//   layer add <名称> [颜色] [线宽] [线型]
//   layer delete <名称> / rename <旧名> <新名>
//   layer visible <名称> [on|off]   layer lock <名称> [on|off]
//   layer set <名称> <color|width|style> <值>
//   layer assign <名称>   （把选中图元移入该层）
//   layer apply  <名称>   （把图层样式套用到本层图元，批量改属性）
//   layer list
// 所有"改属性/显示控制"都只作用于本层图元，其他图层不受影响。
class LayerCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 飞机三面图（航空制图模板）----
class AircraftViewsCommand : public ICommand { public: bool execute(const CommandContext&) override; };

// ---- 帮助 ----
class HelpCommand : public ICommand { public: bool execute(const CommandContext&) override; };

} // namespace SketchCAD
