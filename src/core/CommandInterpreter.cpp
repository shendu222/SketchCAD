#include "CommandInterpreter.h"
#include "MacroScript.h"
#include "AliasTable.h"
#include "SketchModel.h"
#include <QRegularExpression>

namespace SketchCAD {

// 直线子模式命令名列表（用于 Tab 补全）
const QStringList& CommandInterpreter::lineSubModeNames() {
    static const QStringList names = {
        "line_2pt", "line_h", "line_v", "line_angle",
        "line_parallel", "line_parallel_pt", "line_bisector",
        "line_tangent_pc", "line_tangent_cc", "line_tangent_ortho",
        "line_orthogonal", "line_rel_angle", "line_snake", "line_snake_x", "line_snake_y",
        "line_angle_from_line", "line_orthogonal_from_line", "line_from_point_to_line"
    };
    return names;
}

// 样条家族命令名列表（绘制 + 编辑，用于 Tab 补全）
const QStringList& CommandInterpreter::splineCommandNames() {
    static const QStringList names = {
        "spline_parabola", "spline_hyperbola", "spline_parabola_4p",
        "spline_parabola_fd", "spline_through_points", "freehand_line",
        "spline_from_polyline", "spline_append_pt", "spline_insert_pts",
        "spline_remove_pts", "spline_remove_between", "spline_explode"
    };
    return names;
}

// 椭圆家族命令名列表（用于 Tab 补全）
const QStringList& CommandInterpreter::ellipseCommandNames() {
    static const QStringList names = {
        "ellipse_1pt", "ellipse_axis", "ellipse_foci",
        "ellipse_4p", "ellipse_c3p", "ellipse_inscribed"
    };
    return names;
}

// 折线家族命令名列表（用于 Tab 补全）
const QStringList& CommandInterpreter::polylineCommandNames() {
    static const QStringList names = {
        "polyline",
        "polyline_add_node", "polyline_append_node", "polyline_delete_node",
        "polyline_delete_between", "polyline_trim_segments",
        "polyline_equidistant", "polyline_from_segments",
        "polyline_arcs_to_chords", "polyline_change_segment_type"
    };
    return names;
}

// 选择家族命令名列表（用于 Tab 补全）
const QStringList& CommandInterpreter::selectCommandNames() {
    static const QStringList names = {
        "select_all", "select_none", "invert",
        "select_entity", "deselect_entity",
        "select_contour", "toggle_contour", "select_intersected", "deselect_intersected",
        "select_layer", "deselect_layer", "toggle_layer"
    };
    return names;
}

QStringList CommandInterpreter::commandNames() {
    QStringList names = CommandRegistry::instance().exactNames();
    names << lineSubModeNames();
    names << splineCommandNames();
    names << ellipseCommandNames();
    names << polylineCommandNames();
    names << selectCommandNames();
    return names;
}

QStringList CommandInterpreter::completions(const QString& prefix) const {
    QStringList res;
    for (const QString& c : commandNames())
        if (c.startsWith(prefix, Qt::CaseInsensitive)) res << c;
    // 外部命令（插件注册）同样参与 Tab 补全。
    for (const QString& c : externalCommandNames())
        if (c.startsWith(prefix, Qt::CaseInsensitive)) res << c;
    // 别名键同样参与 Tab 补全（展开在 execute 中完成）。
    for (const QString& a : AliasTable::instance().aliases())
        if (a.startsWith(prefix, Qt::CaseInsensitive)) res << a;
    return res;
}

void CommandInterpreter::addHistory(const QString& line) {
    QString s = line.trimmed();
    if (s.isEmpty()) return;
    if (!history_.isEmpty() && history_.last() == s) return;
    history_ << s;
    if (history_.size() > 100) history_.removeFirst();
}

bool CommandInterpreter::execute(const QString& line) {
    QString s = line.trimmed();
    if (s.isEmpty()) return true;

    QStringList toks = s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QString cmd = toks[0].toLower();

    // 别名展开（acad.pgp 兼容）：若首 token 是别名则就地替换为完整命令名。
    // 支持别名→别名链（深度保护，最多 8 跳，避免误配成环时死循环）。
    {
        QString resolved = AliasTable::instance().resolve(cmd);
        int guard = 0;
        while (resolved != cmd && guard < 8) {
            toks[0] = resolved;
            cmd = resolved;
            resolved = AliasTable::instance().resolve(cmd);
            ++guard;
        }
    }

    // 命令模式分派：先精确查找，再前缀查找（line_* 家族）
    auto& reg = CommandRegistry::instance();
    ICommand* command = reg.findExact(cmd);
    if (!command)
        command = reg.findPrefixMatch(cmd);

    // 统一出口：结果先收敛到 ok，供宏捕获点判断（成功路径才录制）
    bool ok = false;
    if (!command) {
        // 内置命令查找失败 → 回退外部命令表（插件经 SDK 注册）。
        auto it = externalCommands_.find(cmd);
        if (it != externalCommands_.end()) {
            QString args = (toks.size() > 1) ? toks.mid(1).join(QChar(' ')) : QString();
            ok = it->second(args);
        } else {
            model_->log("未知命令: " + cmd + "（输入 help 查看）", "warn");
        }
    } else {
        CommandContext ctx{model_, toks, cmd, this};
        ok = command->execute(ctx);
    }

    // 宏录制捕获点：唯一出口——命令行、脚本回放以外的所有命令通路
    // （含插件外部命令）成功执行后都会被 MacroRecorder 判断并捕获。
    if (ok) {
        MacroRecorder::captureLine(s);
        lastCommand_ = cmd;   // 记录解析后的命令名，供空输入重复执行
    }
    return ok;
}

bool CommandInterpreter::registerExternalCommand(const QString& name,
                                                 std::function<bool(const QString&)> fn) {
    if (name.isEmpty() || !fn) return false;
    QString nm = name.toLower();
    if (nm.contains(QChar(' '))) return false;          // 命令名不得含空格
    // 内置命令优先：重名拒绝。
    auto& reg = CommandRegistry::instance();
    if (reg.findExact(nm) || reg.findPrefixMatch(nm)) return false;
    if (externalCommands_.count(nm)) return false;     // 外部去重
    externalCommands_[nm] = std::move(fn);
    return true;
}

QStringList CommandInterpreter::externalCommandNames() const {
    QStringList names;
    for (const auto& kv : externalCommands_) names << kv.first;
    return names;
}

} // namespace SketchCAD
