#include "CommandInterpreter.h"
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

    // 命令模式分派：先精确查找，再前缀查找（line_* 家族）
    auto& reg = CommandRegistry::instance();
    ICommand* command = reg.findExact(cmd);
    if (!command)
        command = reg.findPrefixMatch(cmd);

    if (!command) {
        model_->log("未知命令: " + cmd + "（输入 help 查看）", "warn");
        return false;
    }

    CommandContext ctx{model_, toks, cmd};
    return command->execute(ctx);
}

} // namespace SketchCAD
