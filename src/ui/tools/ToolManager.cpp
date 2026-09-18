// ============================================================
// ToolManager 实现 —— 工具注册 + 事件分派
// ============================================================
#include "ui/tools/ToolManager.h"
#include "ui/tools/Tools.h"
#include "ui/tools/FreehandLineTool.h"
#include "ui/tools/SplineEditTool.h"
#include "ui/tools/PolylineTool.h"
#include "ui/tools/PolylineEditTool.h"
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/ToolRegistry.h"

namespace SketchCAD {

ToolManager::ToolManager(ToolContext ctx) : ctx_(std::move(ctx)) {
    registerBuiltins();
}

void ToolManager::registerTool(const QString& id, std::unique_ptr<ITool> tool) {
    tools_[id] = std::move(tool);
}

void ToolManager::setActiveTool(const QString& id) {
    if (activeId_ == id) return;

    // 退出旧工具
    if (active_) active_->onDeactivate(ctx_);

    activeId_ = id;
    auto it = tools_.find(id);
    active_ = (it != tools_.end()) ? it->second.get() : nullptr;

    // 进入新工具
    if (active_) active_->onActivate(ctx_);
}

QString ToolManager::dispatchPrompt() const {
    if (active_) return active_->prompt(ctx_);
    // 未注册工具的默认提示
    const QString t = ctx_.activeTool();
    return t + QStringLiteral(" — 请操作:");
}

bool ToolManager::dispatchPress(const Vec3& w, QMouseEvent* e) {
    if (active_) return active_->onPress(ctx_, w, e);
    return false;   // 未注册工具走默认逻辑
}

bool ToolManager::dispatchMove(const Vec3& w, QMouseEvent* e) {
    if (active_) return active_->onMove(ctx_, w, e);
    return false;
}

bool ToolManager::dispatchRelease(const Vec3& w, QMouseEvent* e) {
    if (active_) return active_->onRelease(ctx_, w, e);
    return false;
}

bool ToolManager::dispatchRightButton() {
    if (active_) return active_->onRightButton(ctx_);
    return false;
}

// ============================================================
// 注册所有内置工具
// ============================================================
// 工具 ID → ITool 实例的映射。新增工具只需在此添加一行。
// 这是 Strategy Pattern 的 Factory 部分：所有工具的创建集中于此，
// GraphicsView 无需知道具体工具类的存在。
void ToolManager::registerBuiltins() {
    // ---- 选择 ----
    registerTool("select", std::make_unique<SelectTool>());
    // 选择菜单对应的高级选择工具（对应 FreeCAD Sketcher 选择菜单）
    registerTool("select_window",        std::make_unique<SelectWindowTool>());
    registerTool("deselect_window",      std::make_unique<DeselectWindowTool>());
    registerTool("select_entity",        std::make_unique<PickSelectTool>(PickSelectTool::Action::SelectEntity));
    registerTool("deselect_entity",      std::make_unique<PickSelectTool>(PickSelectTool::Action::DeselectEntity));
    registerTool("select_contour",       std::make_unique<PickSelectTool>(PickSelectTool::Action::SelectContour));
    registerTool("toggle_contour",       std::make_unique<PickSelectTool>(PickSelectTool::Action::ToggleContour));
    registerTool("select_intersected",   std::make_unique<PickSelectTool>(PickSelectTool::Action::SelectIntersected));
    registerTool("deselect_intersected", std::make_unique<PickSelectTool>(PickSelectTool::Action::DeselectIntersected));
    registerTool("select_layer",         std::make_unique<PickSelectTool>(PickSelectTool::Action::SelectLayer));
    registerTool("toggle_layer",         std::make_unique<PickSelectTool>(PickSelectTool::Action::ToggleLayer));

    // ---- 基本绘制（PointCollectTool：收集 N 个点后终结）----
    registerTool("line",      std::make_unique<PointCollectTool>("line",      QStringLiteral("直线"), 2));
    registerTool("line_2pt",  std::make_unique<PointCollectTool>("line_2pt",  QStringLiteral("直线"), 2));
    registerTool("line_h",    std::make_unique<PointCollectTool>("line_h",    QStringLiteral("水平线"), 2));
    registerTool("line_v",    std::make_unique<PointCollectTool>("line_v",    QStringLiteral("竖直线"), 2));
    registerTool("line_angle",std::make_unique<PointCollectTool>("line_angle",QStringLiteral("角度直线"), 2));
    registerTool("circle",    std::make_unique<PointCollectTool>("circle",    QStringLiteral("圆"), 2));
    registerTool("circle3p",  std::make_unique<PointCollectTool>("circle3p",  QStringLiteral("三点圆"), 3));
    registerTool("rect",      std::make_unique<PointCollectTool>("rect",      QStringLiteral("矩形"), 2));
    registerTool("arc",       std::make_unique<PointCollectTool>("arc",       QStringLiteral("圆弧"), 3));
    registerTool("poly",      std::make_unique<PointCollectTool>("poly",      QStringLiteral("多边形"), 0, false, true));
    registerTool("spline",    std::make_unique<PointCollectTool>("spline",    QStringLiteral("样条"), 0, true, false));
    registerTool("freehand_line", std::make_unique<FreehandLineTool>());

    // ---- 直线子模式 ----
    const QStringList& lineIds = ToolRegistry::instance().lineSubToolIds();
    for (const QString& id : lineIds)
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Line));

    // ---- 圆形子模式 ----
    const QStringList& circleIds = ToolRegistry::instance().circleSubToolIds();
    for (const QString& id : circleIds)
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Circle));

    // ---- 圆弧子模式 ----
    const QStringList& arcIds = ToolRegistry::instance().arcSubToolIds();
    for (const QString& id : arcIds)
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Arc));

    // ---- 多边形子模式 ----
    const QStringList& polyIds = ToolRegistry::instance().polySubToolIds();
    for (const QString& id : polyIds)
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Poly));

    // ---- 样条子模式 ----
    const QStringList& splineIds = ToolRegistry::instance().splineSubToolIds();
    for (const QString& id : splineIds) {
        if (id == "freehand_line") continue;  // 自有 ITool（FreehandLineTool），勿被 SubToolHandler 覆盖
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Spline));
    }

    // ---- 椭圆子模式 ----
    const QStringList& ellipseIds = ToolRegistry::instance().ellipseSubToolIds();
    for (const QString& id : ellipseIds)
        registerTool(id, std::make_unique<SubToolHandler>(SubToolHandler::Family::Ellipse));

    // ---- 样条编辑工具 ----
    registerTool("spline_from_polyline",   std::make_unique<SplineEditTool>("spline_from_polyline"));
    registerTool("spline_append_pt",       std::make_unique<SplineEditTool>("spline_append_pt"));
    registerTool("spline_insert_pts",      std::make_unique<SplineEditTool>("spline_insert_pts"));
    registerTool("spline_remove_pts",      std::make_unique<SplineEditTool>("spline_remove_pts"));
    registerTool("spline_remove_between",  std::make_unique<SplineEditTool>("spline_remove_between"));
    registerTool("spline_explode",         std::make_unique<SplineEditTool>("spline_explode"));

    // ---- 折线绘制与编辑（10 个）----
    registerTool("polyline",                       std::make_unique<PolylineTool>());
    registerTool("polyline_add_node",              std::make_unique<PolylineEditTool>("polyline_add_node"));
    registerTool("polyline_append_node",           std::make_unique<PolylineEditTool>("polyline_append_node"));
    registerTool("polyline_delete_node",           std::make_unique<PolylineEditTool>("polyline_delete_node"));
    registerTool("polyline_delete_between",        std::make_unique<PolylineEditTool>("polyline_delete_between"));
    registerTool("polyline_trim_segments",         std::make_unique<PolylineEditTool>("polyline_trim_segments"));
    registerTool("polyline_equidistant",           std::make_unique<PolylineEditTool>("polyline_equidistant"));
    registerTool("polyline_from_segments",         std::make_unique<PolylineEditTool>("polyline_from_segments"));
    registerTool("polyline_arcs_to_chords",        std::make_unique<PolylineEditTool>("polyline_arcs_to_chords"));
    registerTool("polyline_change_segment_type",   std::make_unique<PolylineEditTool>("polyline_change_segment_type"));

    // ---- 标注工具 ----
    // 遍历 ToolRegistry 中所有 Dimension 类别的工具（单一真源，避免硬编码 id 双处维护），
    // 为每种标注类型注册一个 DimensionToolHandler。
    const QStringList& dimIds = ToolRegistry::instance().dimensionToolIds();
    for (const QString& id : dimIds) {
        DimensionType dt = DimensionType::Distance;
        ToolRegistry::instance().tryDimensionType(id, dt);
        if (id == "dim_smart")
            registerTool(id, std::make_unique<SmartDimTool>());
        else
            registerTool(id, std::make_unique<DimensionToolHandler>(dt));
    }

    // ---- 编辑工具 ----
    registerTool("delete",     std::make_unique<DeleteTool>());
    registerTool("trim",       std::make_unique<TrimTool>());
    registerTool("extend",     std::make_unique<ExtendTool>());
    registerTool("constraint", std::make_unique<ConstraintTool>());
}

} // namespace SketchCAD
