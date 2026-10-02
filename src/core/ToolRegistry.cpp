#include "core/ToolRegistry.h"

namespace SketchCAD {

// ============================================================
// ToolRegistry 实现
// ============================================================

const ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry reg;
    return reg;
}

ToolRegistry::ToolRegistry() {
    // ==================== 基本绘制工具 ====================
    registerTool({"line",      QStringLiteral("直线"),     ToolCategory::DrawBasic});
    registerTool({"circle",    QStringLiteral("圆"),       ToolCategory::DrawBasic});
    registerTool({"circle3p",  QStringLiteral("三点圆"),   ToolCategory::DrawBasic});
    registerTool({"rect",      QStringLiteral("矩形"),     ToolCategory::DrawBasic});
    registerTool({"arc",       QStringLiteral("圆弧"),     ToolCategory::DrawBasic});
    registerTool({"poly",      QStringLiteral("多边形"),   ToolCategory::DrawBasic});
    registerTool({"spline",    QStringLiteral("样条曲线"), ToolCategory::DrawBasic});
    registerTool({"mtext",     QStringLiteral("多行文字"),   ToolCategory::DrawBasic, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("多行文字(MTEXT)：点击插入点后编辑多行内容、对齐、字高、宽度因子、样式与自动换行")});
    registerTool({"table",     QStringLiteral("表格"),       ToolCategory::DrawBasic, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("表格(TABLE)：点击放置表格左上角后编辑行列、列宽、行高、字高与单元格内容（明细表）")});

    // ==================== 选择 / 编辑 ====================
    registerTool({"select",    QStringLiteral("选择"),     ToolCategory::Select});
    // 选择菜单的高级选择工具（对应 FreeCAD Sketcher 选择菜单逐项）
    registerTool({"select_window",        QStringLiteral("窗口选择"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("窗口选择：拖拽矩形框选内部图元（Ctrl 加选）")});
    registerTool({"deselect_window",      QStringLiteral("窗口取消"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("窗口取消：拖拽矩形取消框内图元的选中")});
    registerTool({"select_entity",        QStringLiteral("选择实体"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("选择实体：点击图元将其选中")});
    registerTool({"deselect_entity",      QStringLiteral("取消选择实体"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("取消选择实体：点击图元取消其选中")});
    registerTool({"select_contour",       QStringLiteral("选择轮廓"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("选择轮廓：点击一条边，选中整条拓扑相连的轮廓")});
    registerTool({"toggle_contour",       QStringLiteral("(取消)选择轮廓"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("(取消)选择轮廓：点击一条边，轮廓已全选中则整条取消，否则整条选中")});
    registerTool({"select_intersected",   QStringLiteral("选择相交"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("选择相交：点击图元，选中所有与之相交的图元")});
    registerTool({"deselect_intersected", QStringLiteral("取消相交"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("取消相交：点击图元，取消与之相交者的选中")});
    registerTool({"select_layer",         QStringLiteral("选择图层"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("选择图层：点击图元，选中同图层全部图元")});
    registerTool({"toggle_layer",         QStringLiteral("(取消)选择图层"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("(取消)选择图层：点击图元，同层已全选中则整层取消，否则整层选中")});
    // 快速选择（QSELECT）：与 QSelectDialog 共用过滤引擎；命令行等价入口为 qselect（别名 q）。
    registerTool({"qselect",              QStringLiteral("快速选择"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("快速选择(QSELECT)：按对象类型/图层/颜色/线型筛选并选中匹配图元")});
    // 不规则范围选择（AutoCAD FENCE / WPolygon / CPolygon 对齐）：点选顶点围出范围
    registerTool({"fence",                QStringLiteral("栏选"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("栏选(FENCE)：画一条多段栏线，与栏线相交的图元全部加选")});
    registerTool({"wp",                   QStringLiteral("圈围"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("圈围(WP)：画一个多边形，完全位于多边形内部的图元加选")});
    registerTool({"cp",                   QStringLiteral("圈交"),
                  ToolCategory::Select, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("圈交(CP)：画一个多边形，与多边形相交或在其内部的图元加选")});
    registerTool({"delete",    QStringLiteral("删除"),     ToolCategory::Edit});
    registerTool({"trim",      QStringLiteral("剪切"),     ToolCategory::Edit});
    registerTool({"extend",    QStringLiteral("延伸"),     ToolCategory::Edit});
    registerTool({"constraint",QStringLiteral("约束"),     ToolCategory::Edit});

    // ==================== 直线子模式（17 种）====================
    // 提示文本与 MainWindow::lineSubModes() 的 tip 字段保持一致，
    // 但这里用于参数输入条下方提示，lineSubModes 用于工具条 tooltip。
    registerTool({"line_h",                 QStringLiteral("水平线"),
                  ToolCategory::LineSub});
    registerTool({"line_v",                 QStringLiteral("竖直线"),
                  ToolCategory::LineSub});
    registerTool({"line_angle",             QStringLiteral("角度直线"),
                  ToolCategory::LineSub, ParamBarMode::AngleLength,
                  DimensionType::Distance,
                  QStringLiteral("角度直线：点击起点即按「角度 + 长度」绘制")});
    registerTool({"line_parallel",          QStringLiteral("平行线"),
                  ToolCategory::LineSub, ParamBarMode::SpacingCount,
                  DimensionType::Distance,
                  QStringLiteral("平行线：先选参考线，再点击决定偏移方向（间隔/条数见左）")});
    registerTool({"line_parallel_pt",       QStringLiteral("过点平行线"),
                  ToolCategory::LineSub, ParamBarMode::None,
                  DimensionType::Distance,
                  QStringLiteral("过点平行线：先选参考线，再点一点（生成等长且对齐的平行线）")});
    registerTool({"line_bisector",          QStringLiteral("角平分线"),
                  ToolCategory::LineSub});
    registerTool({"line_tangent_pc",        QStringLiteral("点-圆切线"),
                  ToolCategory::LineSub});
    registerTool({"line_tangent_cc",        QStringLiteral("圆-圆公切线"),
                  ToolCategory::LineSub, ParamBarMode::None,
                  DimensionType::Distance,
                  QStringLiteral("两圆公切线：依次选两个圆，鼠标移动实时切换切线，点击确认")});
    registerTool({"line_tangent_ortho",     QStringLiteral("垂直参考线切线"),
                  ToolCategory::LineSub});
    registerTool({"line_orthogonal",        QStringLiteral("垂直线"),
                  ToolCategory::LineSub});
    registerTool({"line_rel_angle",         QStringLiteral("相对角度线"),
                  ToolCategory::LineSub});
    registerTool({"line_snake",             QStringLiteral("正交折线"),
                  ToolCategory::LineSub});
    registerTool({"line_snake_x",           QStringLiteral("正交折线(水平起)"),
                  ToolCategory::LineSub});
    registerTool({"line_snake_y",           QStringLiteral("正交折线(竖直起)"),
                  ToolCategory::LineSub});
    registerTool({"line_angle_from_line",   QStringLiteral("相对参考线角度"),
                  ToolCategory::LineSub});
    registerTool({"line_orthogonal_from_line", QStringLiteral("参考线垂线"),
                  ToolCategory::LineSub});
    registerTool({"line_from_point_to_line",   QStringLiteral("点到直线垂线"),
                  ToolCategory::LineSub});

    // ==================== 构造线（Xline，6 种子模式）====================
    // 向两端无限延伸的辅助线。复用直线几何计算（line::Mode），结果线段转无限直线。
    registerTool({"xline",        QStringLiteral("构造线"),
                  ToolCategory::DrawBasic, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("构造线(Xline)：选择绘制方式(H/V/A/两点/角平分线/平行)，向两端无限延伸")});
    registerTool({"xline_h",       QStringLiteral("水平构造线"),
                  ToolCategory::XlineSub});
    registerTool({"xline_v",       QStringLiteral("竖直构造线"),
                  ToolCategory::XlineSub});
    registerTool({"xline_angle",   QStringLiteral("角度构造线"),
                  ToolCategory::XlineSub, ParamBarMode::AngleLength,
                  DimensionType::Distance,
                  QStringLiteral("角度构造线：点击基点即按「角度 + 长度」方向无限延伸")});
    registerTool({"xline_2pt",     QStringLiteral("两点构造线"),
                  ToolCategory::XlineSub});
    registerTool({"xline_bisector", QStringLiteral("角平分线构造线"),
                  ToolCategory::XlineSub});
    registerTool({"xline_parallel", QStringLiteral("平行构造线"),
                  ToolCategory::XlineSub, ParamBarMode::None,
                  DimensionType::Distance,
                  QStringLiteral("平行构造线：先选参考直线，再点击决定平行线位置")});

    // ==================== 圆形子模式（10 种）====================
    registerTool({"circle_2p",          QStringLiteral("两点圆"),
                  ToolCategory::CircleSub});
    registerTool({"circle_2pr",         QStringLiteral("两点+半径"),
                  ToolCategory::CircleSub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("两点+半径：点击两点定义弦，左侧输入半径，光标选近解")});
    registerTool({"circle_cr",          QStringLiteral("圆心+半径"),
                  ToolCategory::CircleSub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("圆心+半径：在左侧输入半径(mm)，点击圆心即绘制")});
    registerTool({"circle_t2e1p",       QStringLiteral("相切两图元+点"),
                  ToolCategory::CircleSub});
    registerTool({"circle_t1e2p",       QStringLiteral("相切一图元+两点"),
                  ToolCategory::CircleSub});
    registerTool({"circle_t2er",        QStringLiteral("相切两图元+半径"),
                  ToolCategory::CircleSub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("相切两图元+半径：拾取两图元，左侧输入半径，点击选解")});
    registerTool({"circle_t3e",         QStringLiteral("相切三图元"),
                  ToolCategory::CircleSub});
    registerTool({"circle_inscribed",   QStringLiteral("内切圆"),
                  ToolCategory::CircleSub});
    registerTool({"circle_concentric",  QStringLiteral("同心圆"),
                  ToolCategory::CircleSub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("同心圆：拾取圆/弧，左侧输入半径或点击设半径")});
    registerTool({"circle_byarc",       QStringLiteral("由圆弧创建"),
                  ToolCategory::CircleSub});

    // ==================== 圆弧子模式（10 种，主模式 "arc" 已在 DrawBasic）====================
    registerTool(ToolSpec("arc_3p",            QStringLiteral("三点圆弧"),
                  ToolCategory::ArcSub));
    registerTool(ToolSpec("arc_cpc",           QStringLiteral("圆心+点+弦长"),
                  ToolCategory::ArcSub, ParamBarMode::Radius, DimensionType::Distance,
                  QStringLiteral("圆心+点+弦长：点击圆心和圆周点，左侧输入弦长")));
    registerTool(ToolSpec("arc_cpaa",          QStringLiteral("圆心+点+弧角"),
                  ToolCategory::ArcSub, ParamBarMode::Radius, DimensionType::Angle,
                  QStringLiteral("圆心+半径+弧角：左侧输入半径和扫掠角度")));
    registerTool(ToolSpec("arc_tpa",           QStringLiteral("起终点+夹角"),
                  ToolCategory::ArcSub, ParamBarMode::AngleLength, DimensionType::Angle,
                  QStringLiteral("起终点+夹角：点击两端点，左侧输入圆心角")));
    registerTool(ToolSpec("arc_tpr",           QStringLiteral("起终点+半径"),
                  ToolCategory::ArcSub, ParamBarMode::Radius, DimensionType::Distance,
                  QStringLiteral("起终点+半径：点击两端点，左侧输入半径，光标选近解")));
    registerTool(ToolSpec("arc_tpl",           QStringLiteral("起终点+弦长"),
                  ToolCategory::ArcSub, ParamBarMode::Radius, DimensionType::Distance,
                  QStringLiteral("起终点+弦长（弧长）：点击两端点，左侧输入弧长")));
    registerTool(ToolSpec("arc_tph",           QStringLiteral("起终点+弦高"),
                  ToolCategory::ArcSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("起终点+弦高(凸度)：点击两端点和凸度控制点")));
    registerTool(ToolSpec("arc_tan",           QStringLiteral("相切延续弧"),
                  ToolCategory::ArcSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("相切延续弧：选择参考图元，再点起点和终点")));
    registerTool(ToolSpec("arc_ea_axis",      QStringLiteral("椭圆弧(轴)"),
                  ToolCategory::ArcSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆弧(轴)：中心 + 长轴端点 + 角度点")));
    registerTool(ToolSpec("arc_ea_1pt",        QStringLiteral("椭圆弧(1点)"),
                  ToolCategory::ArcSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆弧(1点)：中心 + 长轴端点 + 过点")));

    // ==================== 多边形子模式（7 种，主模式 "poly" 已在 DrawBasic）====================
    registerTool({"poly_regular",       QStringLiteral("正多边形(内接)"),
                  ToolCategory::PolySub, ParamBarMode::Sides,
                  DimensionType::Distance,
                  QStringLiteral("正多边形(内接)：左侧输入边数，点击圆心与圆周顶点")});
    registerTool({"poly_circumscribed", QStringLiteral("正多边形(外切)"),
                  ToolCategory::PolySub, ParamBarMode::Sides,
                  DimensionType::Distance,
                  QStringLiteral("正多边形(外切)：左侧输入边数，点击圆心与边中点")});
    registerTool({"poly_edge",          QStringLiteral("正多边形(边长)"),
                  ToolCategory::PolySub, ParamBarMode::Sides,
                  DimensionType::Distance,
                  QStringLiteral("正多边形(边长)：左侧输入边数，点击边的起点与终点")});
    registerTool({"rect_chamfer",       QStringLiteral("倒角矩形"),
                  ToolCategory::PolySub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("倒角矩形：左侧输入倒角距离(mm)，点击对角两点")});
    registerTool({"rect_fillet",        QStringLiteral("圆角矩形"),
                  ToolCategory::PolySub, ParamBarMode::Radius,
                  DimensionType::Distance,
                  QStringLiteral("圆角矩形：左侧输入圆角半径(mm)，点击对角两点")});
    registerTool({"poly_star",          QStringLiteral("星形"),
                  ToolCategory::PolySub, ParamBarMode::Sides,
                  DimensionType::Distance,
                  QStringLiteral("星形：左侧输入角数，点击圆心与外顶点")});

    // ==================== 样条子模式（2 种，主模式 "spline" 已在 DrawBasic）====================
    registerTool({"spline_parabola",  QStringLiteral("抛物线"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("抛物线：依次点击起点、切线控制点、终点（二次贝塞尔曲线）")});
    registerTool({"spline_hyperbola", QStringLiteral("双曲线"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("双曲线：依次点击两个焦点和曲线上一点")});
    registerTool({"spline_parabola_4p",    QStringLiteral("抛物线(4点)"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("抛物线(4点)：依次点击两个端点和曲线上两个插值点，自动拟合二次贝塞尔抛物线")});
    registerTool({"spline_parabola_fd",    QStringLiteral("抛物线(焦点准线)"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("抛物线(焦点准线)：依次点击焦点、准线上第1点、准线上第2点")});
    registerTool({"spline_through_points", QStringLiteral("样条过点"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("样条过点（拟合样条）：逐点点击必经点，右键结束（Catmull-Rom 插值）")});
    // 自由手绘：自有 ITool（FreehandLineTool），仍归入 SplineSub 仅用于菜单归类；
    // ToolManager 的 SplineSub 自动注册循环会跳过它，避免被 SubToolHandler 覆盖。
    registerTool({"freehand_line",         QStringLiteral("自由手绘"),
                  ToolCategory::SplineSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("自由手绘：按住左键移动绘制，松开结束")});

    // ==================== 样条编辑工具（自有 SplineEditTool，归入 Edit 避免被 SubToolHandler 覆盖） ====================
    registerTool({"spline_from_polyline",  QStringLiteral("由多段线创建样条"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("由多段线创建样条：选择折线/多边形/矩形转换为样条")});
    registerTool({"spline_append_pt",      QStringLiteral("追加样条点"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("追加样条点：选择样条并点击要追加的点")});
    registerTool({"spline_insert_pts",     QStringLiteral("插入样条点"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("插入样条点：选择样条并点击段上位置")});
    registerTool({"spline_remove_pts",     QStringLiteral("删除样条点"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("删除样条点：选择样条并点击要删除的控制点")});
    registerTool({"spline_remove_between", QStringLiteral("删除两点之间"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("删除两点之间：两次点击两个控制点，删除中间段")});
    registerTool({"spline_explode",        QStringLiteral("分解样条为线段"),
                  ToolCategory::Edit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("分解样条为线段：将样条炸开为多条独立线段（Explode spline to lines）")});

    // ==================== 椭圆子模式（6 种，对应 FreeCAD Ellipse 菜单）====================
    registerTool({"ellipse_1pt",       QStringLiteral("椭圆(1点)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(1点)：点击中心，再点椭圆上一点（作为长轴端点，短轴取长轴一半）")});
    registerTool({"ellipse_axis",       QStringLiteral("椭圆(轴)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(轴)：点击中心、长轴端点、椭圆上一点")});
    registerTool({"ellipse_foci",       QStringLiteral("椭圆(焦点)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(焦点)：依次点击两个焦点，再点椭圆上一点")});
    registerTool({"ellipse_4p",         QStringLiteral("椭圆(4点)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(4点)：依次点击椭圆上四个点，拟合轴对齐椭圆")});
    registerTool({"ellipse_c3p",        QStringLiteral("椭圆(中心+3点)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(中心+3点)：先点中心，再点椭圆上三个点，拟合旋转椭圆")});
    registerTool({"ellipse_inscribed",  QStringLiteral("椭圆(内切)"),
                  ToolCategory::EllipseSub, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("椭圆(内切)：先选中一个矩形或多边形，生成其内切椭圆")});

    // ==================== 折线（绘制 + 9 个编辑）====================
    registerTool({"polyline",                      QStringLiteral("折线"),
                  ToolCategory::Polyline, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("折线（Polyline）：依次点击节点生成多段线，右键结束，点击首点附近可闭合")});

    // ==================== 边界（BOUNDARY / BPOLY）====================
    // 用户交互定义填充边界（外环 + 可选内孔），作为剖面填充的直接输入，
    // 解除剖面填充对内置「边界自动追踪算法」的单一依赖。BPOLY 为同义命令别名。
    registerTool({"boundary", QStringLiteral("边界"),
                  ToolCategory::Polyline, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("边界(BOUNDARY)：依次点击节点定义封闭填充边界（可含内孔），生成边界图元供剖面填充直接复用，无需逐段描边")});
    registerTool({"bpoly",    QStringLiteral("边界多边形"),
                  ToolCategory::Polyline, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("边界多边形(BPOLY)：与边界同义，依次点击节点定义封闭填充多边形（可含内孔）")});
    registerTool({"polyline_add_node",             QStringLiteral("折线·添加节点"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("在折线段上插入节点")});
    registerTool({"polyline_append_node",          QStringLiteral("折线·追加节点"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("在折线首/末端追加节点")});
    registerTool({"polyline_delete_node",          QStringLiteral("折线·删除节点"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("删除折线上的节点")});
    registerTool({"polyline_delete_between",       QStringLiteral("折线·删除两点之间"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("删除折线上两节点之间的部分")});
    registerTool({"polyline_trim_segments",        QStringLiteral("折线·修剪段"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("修剪折线段（删除整段）")});
    registerTool({"polyline_equidistant",          QStringLiteral("折线·等距"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("创建等距折线（距离 = 鼠标到折线质心）")});
    registerTool({"polyline_from_segments",        QStringLiteral("折线·从已有段"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("从若干已有线段/圆弧/折线合并为一条折线")});
    registerTool({"polyline_arcs_to_chords",       QStringLiteral("折线·弧转弦"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("将折线中所有弧段改为直线段")});
    registerTool({"polyline_change_segment_type",  QStringLiteral("折线·切换段类型"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("切换折线段的类型（直线↔弧）")});
    registerTool({"polyline_fit",                  QStringLiteral("折线·拟合"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("拟合曲线：平滑通过所有顶点（PEDIT Fit）")});
    registerTool({"polyline_spline",               QStringLiteral("折线·样条化"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("样条化：顶点作为三次 B 样条控制点（PEDIT Spline）")});
    registerTool({"polyline_width",                QStringLiteral("折线·设宽"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("设置折线统一线宽（PEDIT Width）")});
    registerTool({"polyline_close",                QStringLiteral("折线·闭合/打开"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("切换折线闭合状态（PEDIT Close）")});
    registerTool({"polyline_join",                 QStringLiteral("折线·合并"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("把相邻直线/圆弧/折线合并到折线（PEDIT Join）")});
    registerTool({"polyline_reverse",              QStringLiteral("折线·反转"),
                  ToolCategory::PolylineEdit, ParamBarMode::None, DimensionType::Distance,
                  QStringLiteral("反转折线顶点顺序")});

    // ==================== 标注工具（8 种）====================
    registerTool({"dim",          QStringLiteral("标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Distance});
    registerTool({"dim_linear",  QStringLiteral("线性标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Distance});
    registerTool({"dim_aligned",  QStringLiteral("对齐标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Aligned});
    registerTool({"dim_h",        QStringLiteral("水平标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Horizontal});
    registerTool({"dim_v",        QStringLiteral("竖直标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Vertical});
    registerTool({"dim_angle",    QStringLiteral("角度标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Angle});
    registerTool({"dim_radius",   QStringLiteral("半径标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Radius});
    registerTool({"dim_diameter", QStringLiteral("直径标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Diameter});
    registerTool({"dim_smart",   QStringLiteral("智能标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Distance});
    // 新增的 5 种标注类型（与 DimStyle::build 的 buildBaseline/buildContinue/buildArcLength/
    // buildLeader/buildOrdinate 一一对应；CLI 见 Commands.cpp 的 DimCommand::dmap）。
    registerTool({"dim_baseline", QStringLiteral("基线标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Baseline});
    registerTool({"dim_continue", QStringLiteral("连续标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Continue});
    registerTool({"dim_arc",      QStringLiteral("弧长标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Arc});
    registerTool({"dim_leader",   QStringLiteral("引线标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Leader});
    registerTool({"dim_ordinate", QStringLiteral("坐标标注"),
                  ToolCategory::Dimension, ParamBarMode::None, DimensionType::Ordinate});

    // ==================== 构建分类 ID 列表 ====================
    for (auto it = tools_.constBegin(); it != tools_.constEnd(); ++it) {
        if (it.value().category == ToolCategory::LineSub)
            lineSubIds_ << it.key();
        else if (it.value().category == ToolCategory::XlineSub)
            xlineSubIds_ << it.key();
        else if (it.value().category == ToolCategory::CircleSub)
            circleSubIds_ << it.key();
        else if (it.value().category == ToolCategory::ArcSub)
            arcSubIds_ << it.key();
        else if (it.value().category == ToolCategory::PolySub)
            polySubIds_ << it.key();
        else if (it.value().category == ToolCategory::SplineSub)
            splineSubIds_ << it.key();
        else if (it.value().category == ToolCategory::EllipseSub)
            ellipseSubIds_ << it.key();
        else if (it.value().category == ToolCategory::Dimension)
            dimensionIds_ << it.key();
    }
}

void ToolRegistry::registerTool(const ToolSpec& spec) {
    tools_.insert(spec.id, spec);
}

const ToolSpec* ToolRegistry::find(const QString& id) const {
    auto it = tools_.constFind(id);
    return it != tools_.constEnd() ? &it.value() : nullptr;
}

// ---- 分类查询 ----

bool ToolRegistry::isDrawTool(const QString& id) const {
    const ToolSpec* s = find(id);
    if (!s) return false;
    return s->category == ToolCategory::DrawBasic ||
           s->category == ToolCategory::LineSub ||
           s->category == ToolCategory::XlineSub ||
           s->category == ToolCategory::CircleSub ||
           s->category == ToolCategory::ArcSub ||
           s->category == ToolCategory::PolySub ||
           s->category == ToolCategory::SplineSub ||
           s->category == ToolCategory::EllipseSub ||
           s->category == ToolCategory::Polyline;
}

bool ToolRegistry::isPolylineTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::Polyline;
}

bool ToolRegistry::isPolylineEditTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::PolylineEdit;
}

bool ToolRegistry::isLineSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::LineSub;
}

bool ToolRegistry::isXlineSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::XlineSub;
}

bool ToolRegistry::isCircleSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::CircleSub;
}

bool ToolRegistry::isArcSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::ArcSub;
}

bool ToolRegistry::isPolySubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::PolySub;
}

bool ToolRegistry::isSplineSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::SplineSub;
}

bool ToolRegistry::isEllipseSubTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::EllipseSub;
}

bool ToolRegistry::isDimensionTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::Dimension;
}

bool ToolRegistry::isEditTool(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->category == ToolCategory::Edit;
}

bool ToolRegistry::usesRadiusLabel(const QString& id) const {
    const ToolSpec* s = find(id);
    return s && s->paramMode == ParamBarMode::Radius;
}

bool ToolRegistry::tryDimensionType(const QString& id, DimensionType& out) const {
    const ToolSpec* s = find(id);
    if (!s || s->category != ToolCategory::Dimension) return false;
    out = s->dimType;
    return true;
}

ParamBarMode ToolRegistry::paramMode(const QString& id) const {
    const ToolSpec* s = find(id);
    return s ? s->paramMode : ParamBarMode::None;
}

QString ToolRegistry::paramHint(const QString& id) const {
    const ToolSpec* s = find(id);
    return s ? s->paramHint : QString();
}

} // namespace SketchCAD
