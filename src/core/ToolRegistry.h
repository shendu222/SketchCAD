#pragma once
#include <QString>
#include <QHash>
#include <QStringList>
#include "core/Element.h"   // DimensionType

namespace SketchCAD {

// ---- 工具类别 ----
enum class ToolCategory {
    Select,       // 选择
    DrawBasic,    // 基本绘制（line, circle, circle3p, rect, arc, poly, spline）
    LineSub,      // 直线子模式（line_h, line_v, line_angle, line_parallel, ...）
    CircleSub,    // 圆形子模式（circle_2p, circle_cr, circle_t2e1p, ...）
    ArcSub,       // 圆弧子模式（arc_3p, arc_tpr, arc_tan, ...）
    PolySub,      // 多边形子模式（poly_regular, poly_circumscribed, poly_edge, ...）
    SplineSub,    // 样条子模式（spline_parabola, spline_hyperbola）
    EllipseSub,   // 椭圆子模式（ellipse_1pt, ellipse_axis, ellipse_foci, ...）
    Polyline,     // 折线绘制（polyline）
    PolylineEdit, // 折线编辑（add/append/delete/delete_between/trim/equidistant/from_segments/arcs_to_chords/change_segment）
    Dimension,    // 标注（dim, dim_aligned, dim_angle, ...）
    Edit          // 编辑（delete, trim, extend, constraint）
};

// ---- 参数输入条模式 ----
// 原先 MainWindow::updateParamBarForTool() 中硬编码了 4 组 if-else 来判断
// 当前工具应启用哪些参数控件。提取为枚举后，工具定义与 UI 控件逻辑解耦：
// 新增工具只需在 ToolSpec 中指定 paramMode，无需改 updateParamBarForTool()。
enum class ParamBarMode {
    None,         // 不需要参数（大部分工具）
    AngleLength,  // 角度 + 长度（line_angle）
    SpacingCount, // 间隔 + 条数（line_parallel）
    Radius,       // 半径（circle_cr, circle_2pr, circle_t2er, circle_concentric）
    Sides         // 边数（poly_regular, poly_circumscribed, poly_edge, poly_star）
};

// ---- 工具元数据 ----
// 所有工具信息的唯一定义点（Single Source of Truth）。
// 原先工具 ID 字符串散落在 GraphicsView::isDrawTool / isLineSubTool / isCircleSubTool /
// dimToolType / snapDefaultFor 以及 MainWindow::updateParamBarForTool 等多处 if-else 链中，
// 新增工具需要改 5~6 个地方。现在只需在 ToolRegistry 构造函数中添加一行 ToolSpec。
struct ToolSpec {
    QString id;                    // 工具标识（如 "line", "circle_cr"）
    QString displayName;           // 中文名称（如 "直线", "圆心+半径"）
    ToolCategory category;         // 类别
    ParamBarMode paramMode = ParamBarMode::None;  // 参数输入条模式
    DimensionType dimType = DimensionType::Distance;  // 标注类型（仅 Dimension 类别有效）
    QString paramHint;             // 参数输入条提示文本（默认空）

    // 显式默认构造，避免聚合初始化时 -Wmissing-field-initializers 警告
    ToolSpec() = default;
    ToolSpec(QString id_, QString name, ToolCategory cat,
             ParamBarMode pm = ParamBarMode::None,
             DimensionType dt = DimensionType::Distance,
             QString hint = QString())
        : id(std::move(id_)), displayName(std::move(name)), category(cat),
          paramMode(pm), dimType(dt), paramHint(std::move(hint)) {}
};

// ---- 工具注册表（Singleton + Factory）----
// 设计模式：
//   - Singleton：全局唯一实例，所有模块通过 ToolRegistry::instance() 访问
//   - Factory：按工具 ID 查找 ToolSpec，封装了工具的创建/查找逻辑
// DRY 收益：
//   - 消除 GraphicsView 中 isDrawTool / isLineSubTool / isCircleSubTool / dimToolType
//     四个分类函数的重复工具 ID 字符串匹配
//   - 消除 MainWindow::updateParamBarForTool 中硬编码的工具 ID 判断
//   - 新增工具只需在 registerBuiltins() 中添加一行，无需修改其他文件
class ToolRegistry {
public:
    static const ToolRegistry& instance();

    // ---- 查找 ----
    const ToolSpec* find(const QString& id) const;

    // ---- 分类查询（替代 GraphicsView 中的多处 if-else 链）----
    bool isDrawTool(const QString& id) const;        // 绘制类（基本 + 直线子 + 圆形子）
    bool isLineSubTool(const QString& id) const;     // 直线子模式
    bool isCircleSubTool(const QString& id) const;   // 圆形子模式
    bool isArcSubTool(const QString& id) const;     // 圆弧子模式
    bool isPolySubTool(const QString& id) const;    // 多边形子模式
    bool isSplineSubTool(const QString& id) const;  // 样条子模式
    bool isEllipseSubTool(const QString& id) const; // 椭圆子模式
    bool isPolylineTool(const QString& id) const;   // 折线绘制
    bool isPolylineEditTool(const QString& id) const; // 折线编辑
    bool isDimensionTool(const QString& id) const;   // 标注工具
    bool isEditTool(const QString& id) const;        // 编辑工具
    bool usesRadiusLabel(const QString& id) const;   // 需要半径标签（参数输入条）

    // 标注工具 ID → DimensionType 映射（替代 GraphicsView::dimToolType）
    bool tryDimensionType(const QString& id, DimensionType& out) const;

    // 参数输入条模式
    ParamBarMode paramMode(const QString& id) const;

    // 参数输入条提示文本
    QString paramHint(const QString& id) const;

    // 获取各类工具 ID 列表
    const QStringList& lineSubToolIds() const { return lineSubIds_; }
    const QStringList& circleSubToolIds() const { return circleSubIds_; }
    const QStringList& arcSubToolIds() const { return arcSubIds_; }
    const QStringList& polySubToolIds() const { return polySubIds_; }
    const QStringList& splineSubToolIds() const { return splineSubIds_; }
    const QStringList& ellipseSubToolIds() const { return ellipseSubIds_; }
    const QStringList& dimensionToolIds() const { return dimensionIds_; }

private:
    ToolRegistry();
    void registerTool(const ToolSpec& spec);

    QHash<QString, ToolSpec> tools_;
    QStringList lineSubIds_;       // 直线子模式 ID 列表
    QStringList circleSubIds_;     // 圆形子模式 ID 列表
    QStringList arcSubIds_;        // 圆弧子模式 ID 列表
    QStringList polySubIds_;       // 多边形子模式 ID 列表
    QStringList splineSubIds_;     // 样条子模式 ID 列表
    QStringList ellipseSubIds_;    // 椭圆子模式 ID 列表
    QStringList dimensionIds_;     // 标注工具 ID 列表
};

} // namespace SketchCAD
