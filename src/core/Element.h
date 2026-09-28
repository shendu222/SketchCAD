#pragma once
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include "Vec.h"
#include "DimensionStandard.h"

namespace SketchCAD {

enum class ElementType { Line, Circle, Arc, Rectangle, Polygon, Spline, Constraint, Dimension, Ellipse, Polyline, Text, Hatch, BlockRef, Xline, Boundary };
// 注：ElementType 枚举「只增不改」，新类型必须追加到末尾（Text=10，Hatch=11，BlockRef=12，Xline=13，Boundary=14）。
// 已存盘 .json 文件里存的是整数序号，改顺序会让老文件读出错误类型。
// Xline（构造线）：向两端无限延伸的辅助线。复用 pts[0]=通过点 + rotation=方向角(度)，无需新增字段。
// Boundary（边界/BOUNDARY/BPOLY）：用户交互定义的填充边界闭合环（可含内孔），作为剖面填充的直接输入，
//   解除剖面填充对内置「边界自动追踪算法」的单一依赖。几何直接复用 pts(外环) + holes(内孔)，
//   样式复用 rotation(剖面线角) + radius(间距) + sectionMaterial/sectionExempt —— 与 Hatch 同字段、零新增字段。
//   REGION（区域）不另设图元类型，而是「边界的命名分组」：多个 Boundary 共享同一 name 即构成一个区域，
//   由区域管理对话框统一选择/填充/编辑（见 RegionManagerDialog）。

// 线型（GB/T 4457.4-2002 技术制图 图线 → GB/T 14665-2012 4.1 九种线型）
// 枚举值顺序只增不改：已存盘 .sketch 文件里存的是整数，改顺序会让老文件读出错误线型。
// 粗/细由「线宽组别」(LineWidthGroup) 决定，不再单列枚举值：
//   Solid/Dashed/Dotted/DashDot/DashDotDot 为细档；
//   ThickDashed/ThickDashDot 明确为粗档；Wave/DoubleBreak 为断裂边界（细）。
enum class LineStyle {
    Solid = 0,       // 实线
    Dashed = 1,      // 虚线
    Dotted = 2,      // 点线
    DashDot = 3,     // 点划线
    DashDotDot = 4,  // 双点划线
    Wave = 5,        // 波浪线（断裂边界）
    DoubleBreak = 6, // 双折线（断裂边界）
    ThickDashed = 7, // 粗虚线
    ThickDashDot = 8 // 粗点画线
};
constexpr int kLineStyleCount = 9;   // 线型总数（新增线型务必同步此值）

// 线型 → Qt 画笔风格。
// 波浪线/双折线 Qt 无内建画笔风格：此处给近似风格（正式绘制由渲染层用自定义路径完成），
// 以保证图标预览与命中几何始终可用。
inline Qt::PenStyle toPenStyle(LineStyle ls) {
    switch (ls) {
        case LineStyle::Solid:       return Qt::SolidLine;
        case LineStyle::Dashed:      return Qt::DashLine;
        case LineStyle::Dotted:      return Qt::DotLine;
        case LineStyle::DashDot:     return Qt::DashDotLine;
        case LineStyle::DashDotDot:  return Qt::DashDotDotLine;
        case LineStyle::Wave:        return Qt::DashLine;        // 近似（自定义路径绘制）
        case LineStyle::DoubleBreak: return Qt::DashDotLine;     // 近似（自定义路径绘制）
        case LineStyle::ThickDashed: return Qt::DashLine;
        case LineStyle::ThickDashDot:return Qt::DashDotLine;
    }
    return Qt::SolidLine;
}

inline const char* lineStyleName(LineStyle ls) {
    switch (ls) {
        case LineStyle::Solid:       return "实线";
        case LineStyle::Dashed:      return "虚线";
        case LineStyle::Dotted:      return "点线";
        case LineStyle::DashDot:     return "点划线";
        case LineStyle::DashDotDot:  return "双点划线";
        case LineStyle::Wave:        return "波浪线";
        case LineStyle::DoubleBreak: return "双折线";
        case LineStyle::ThickDashed: return "粗虚线";
        case LineStyle::ThickDashDot:return "粗点画线";
    }
    return "实线";
}

// GB/T 14665-2012 4.3「重合图线优先顺序」：数值越大越优先（应绘制在上层）。
//   可见轮廓(粗实线) > 不可见轮廓(细虚线) > 轴线/对称中心线(点画线) > 假想轮廓(双点画线) > 断裂线
inline int linePriority(LineStyle ls) {
    switch (ls) {
        case LineStyle::Solid:                          return 6;  // 可见轮廓
        case LineStyle::Dashed:
        case LineStyle::ThickDashed:                    return 5;  // 不可见轮廓
        case LineStyle::DashDot:
        case LineStyle::ThickDashDot:
        case LineStyle::Dotted:                         return 3;  // 轴线/中心线
        case LineStyle::DashDotDot:                     return 2;  // 假想轮廓
        case LineStyle::Wave:
        case LineStyle::DoubleBreak:                    return 1;  // 断裂边界
    }
    return 0;
}

// 线型默认线宽的查询桥。真实映射由 LineStyleConfig 单例持有（单一数据源），
// 此处仅前向声明一个自由函数，供 Element::effectiveLineWidth() 调用，
// 从而避免 Element.h ↔ LineStyleConfig.h 的头文件循环依赖。
// （实现见 LineStyleConfig.cpp）
double defaultWidthForStyle(LineStyle ls);

enum class ConstraintType {
    Coincident, Parallel, Perpendicular, Equal, Tangent, Horizontal, Vertical, Fixed
};

// 尺寸标注类型（GB/T 4458.4-2003 / ISO 129-1 尺寸注法）。
// 枚举值顺序只增不改：已存盘的 .sketch 文件里存的是整数，改顺序会让老文件读出错误类型。
enum class DimensionType {
    Distance = 0,   // 线性（自动判水平/竖直，取投影较长的方向）
    Angle,          // 角度
    Radius,         // 半径 R
    Diameter,       // 直径 ⌀
    Aligned,        // 对齐（尺寸线平行于两测量点连线，量真实距离）
    Horizontal,     // 线性-水平（只量 Δx）
    Vertical,       // 线性-竖直（只量 Δy）
    Baseline,       // 基线标注（与 Aligned/Horizontal/Vertical 共用测量点，按基线层叠）
    Continue,       // 连续标注（链式，从上一尺寸末端继续）
    Arc,            // 弧长标注（⌒ 前缀）
    Leader,         // 指引线/引线标注（文字 + 引线）
    Ordinate        // 坐标标注（从基准出发，标注 X 或 Y 坐标）
};

inline const char* dimensionTypeName(DimensionType t) {
    switch (t) {
        case DimensionType::Distance:   return "线性标注";
        case DimensionType::Angle:      return "角度标注";
        case DimensionType::Radius:     return "半径标注";
        case DimensionType::Diameter:   return "直径标注";
        case DimensionType::Aligned:    return "对齐标注";
        case DimensionType::Horizontal: return "水平标注";
        case DimensionType::Vertical:   return "竖直标注";
        case DimensionType::Baseline:   return "基线标注";
        case DimensionType::Continue:   return "连续标注";
        case DimensionType::Arc:        return "弧长标注";
        case DimensionType::Leader:     return "引线标注";
        case DimensionType::Ordinate:   return "坐标标注";
    }
    return "标注";
}

inline const char* elementTypeName(ElementType t) {
    switch (t) {
        case ElementType::Line:      return "直线";
        case ElementType::Circle:    return "圆";
        case ElementType::Arc:       return "圆弧";
        case ElementType::Rectangle: return "矩形";
        case ElementType::Polygon:   return "多边形";
        case ElementType::Spline:    return "样条曲线";
        case ElementType::Constraint:return "约束";
        case ElementType::Dimension: return "标注";
        case ElementType::Ellipse:   return "椭圆";
        case ElementType::Polyline:  return "折线";
    case ElementType::Text:      return "文本";
    case ElementType::Hatch:     return "剖面区域";
    case ElementType::BlockRef:  return "块引用";
    case ElementType::Xline:     return "构造线";
    case ElementType::Boundary:  return "边界";
    }
    return "未知";
}

inline const char* constraintTypeName(ConstraintType t) {
    switch (t) {
        case ConstraintType::Coincident:    return "重合";
        case ConstraintType::Parallel:      return "平行";
        case ConstraintType::Perpendicular: return "垂直";
        case ConstraintType::Equal:         return "相等";
        case ConstraintType::Tangent:       return "相切";
        case ConstraintType::Horizontal:    return "水平";
        case ConstraintType::Vertical:      return "竖直";
        case ConstraintType::Fixed:         return "固定";
    }
    return "固定";
}

// 线宽档位（屏幕像素，与视图缩放无关 —— 用 cosmetic pen 绘制）
inline const char* lineWidthName(double px) {
    if (px <= 1.0) return "细线 1px";
    if (px <= 2.0) return "中粗线 2px";
    if (px <= 3.0) return "粗线 3px";
    return "特粗线 5px";
}

// ---- 块引用（ElementType::BlockRef）实例上的数据 ----
// 之所以把这两个小 POD 定义在 Element.h（而非 Block.h）：BlockDef 需要 Element，
// 而 Element 需要它们；反向包含会造成头文件循环。它们是纯数据、无外部依赖。
//
// 属性值（ATTRIB）：tag 与块定义 AttributeDef::tag 对应，value 为本实例的取值。
struct BlockAttrValue {
    QString tag;
    QString value;
};
// 动态参数取值（按参数名索引）：可见性用 state，其余用 value。
struct BlockDynValue {
    QString name;             // 参数名
    double  value = 0.0;      // 拉伸距离(mm) / 旋转角(度)；翻转用 0/1
    QString state;            // 可见性状态名（仅可见性参数使用）
};

// 草图图元（统一数据模型）。坐标为世界坐标，遵循标准笛卡尔右手系：X 向右为正、Y 向上为正。
// （视图层通过 Y 轴翻转把它映射到 Qt 的屏幕坐标，模型层不感知屏幕方向。）
struct Element {
    int id = 0;
    ElementType type = ElementType::Line;
    QString name;
    QString layer = "0";
    QColor color = QColor(26, 29, 40);   // 默认深灰 1A1D28
    double lineWidth = 1.0;              // 手动线宽(px)：仅当 lineWidthAuto==false 时生效
    LineStyle lineStyle = LineStyle::Solid;  // 线型：实线/虚线/点划线/双点划线
    bool lineWidthAuto = true;           // true=线宽随线型自动派生(见 LineStyleConfig)；
                                         // false=使用上方 lineWidth 手动覆盖值
    // 实际生效线宽：自动模式取"线型默认线宽"，手动模式取 lineWidth。
    // 创建与渲染统一走此函数，保证"线型改变→线宽自动跟随"且保留手动覆盖。
    double effectiveLineWidth() const {
        return lineWidthAuto ? defaultWidthForStyle(lineStyle) : lineWidth;
    }
    std::vector<Vec3> pts;               // 控制点(世界坐标)
    double radius = 0;                   // 圆/圆弧半径；角度标注时为尺寸弧半径；椭圆长半轴
                                        // 对 Text 类型：复用本字段表示文字高度(mm)，见 sheet_frame/graphicsview
    double startAngle = 0, endAngle = 0; // 圆弧起止角(度)
    double radiusY = 0;                  // 椭圆短半轴（仅 ElementType::Ellipse 有效）
    double rotation = 0;               // 椭圆长轴与 X 轴正向夹角(度)，0 表示长轴水平
    // 折线是否闭合。矩形/多边形天然闭合，样条天然开放；
    // 剪切会把闭合轮廓切成开放折线，因此闭合与否必须是数据而不是类型的固有属性。
    bool closed = true;
    // 样条子类型（仅对 ElementType::Spline 有效）：
    //   0 = Catmull-Rom 插值样条（默认，过所有控制点）
    //   1 = 抛物线（二次贝塞尔，pts 存 3 个控制点：起点、切线交点、终点）
    //   2 = 双曲线（pts 存 3 个定义点：焦点1、焦点2、曲线上一点，渲染时采样）
    //   3 = 抛物线(4点)：pts 存 4 个定义点，渲染时拟合二次贝塞尔
    //   4 = 抛物线(焦点准线)：pts[0]=焦点，pts[1]/pts[2]=准线上两点
    //   5 = 样条过点：Catmull-Rom 插值，过所有控制点
    //   6 = 自由手绘线：采集点序列，Catmull-Rom 插值
    // 整数值顺序只增不改，旧 .sketch 文件兼容。
    int splineSubType = 0;
    ConstraintType ctype = ConstraintType::Fixed;
    DimensionType dtype = DimensionType::Distance;
    DimensionStandard standard = DimensionStandard::GBT; // 该标注遵循的制图标准（默认国标）
    QString text;                        // 标注文本（用户手改或自动生成的完整文本）
    double tolUpper = 0.0;               // 上偏差（+）
    double tolLower = 0.0;               // 下偏差（-）
    double dimTextHeight = 0.0;          // 尺寸标注字高覆盖(mm)：>0 时覆盖 DimStyle 默认字高(3.5)。
                                         //   用于法兰三维图等需缩小标注字高、避免遮挡图形的场景。
    QString fitCode;                     // 配合代号，如 H7/g6
    QString symbolLibrary;               // 符号库引用
    int refA = -1, refB = -1;            // 约束/标注引用的图元 id
    bool visible = true;
    // 折线段类型标记（仅对 ElementType::Polyline 有效）：
    //   size() == 段数 == pts.size() - 1（开放）或 pts.size()（闭合）。
    //   false = 直线段，true = 圆弧段（圆弧由 3 个相邻节点 + 该段两端的相邻节点共同定心/半径）。
    // 旧 .sketch 文件没这字段时回退为空 → 全部视为直线段。
    std::vector<bool> arcFlags;

    // ---- 剖面区域（ElementType::Hatch）专用字段（GB/T 4457.5-2013）----
    //   pts            = 外边界（闭合多边形，世界坐标）
    //   holes          = 内孔点列（每个自动闭合，布尔减），用于带孔剖面（如法兰螺栓孔）
    //   rotation       = 剖面线方向角(度)（复用；默认由工厂置 45）
    //   radius         = 剖面线间距(mm)（复用；GB 建议 2~4mm，默认 3）
    //   lineWidth      = 细实线线宽(px)（复用；lineWidthAuto=false 时生效）
    //   sectionExempt  = (int)SectionExempt 豁免类别（见 Section.h）
    //   sectionMaterial= (int)SectionMaterial 材料类别（决定剖面符号）
    // 这些字段对其它图元无副作用（默认空/0）。旧存档无这些键时回落默认，向后兼容。
    std::vector<std::vector<Vec3>> holes;
    int sectionExempt = 0;      // 0 == SectionExempt::None
    int sectionMaterial = 0;    // 0 == SectionMaterial::General

    // ---- 块引用（ElementType::BlockRef）专用字段 ----
    // 采用「引用式」：只存块名 + 插入变换 + 属性值 + 动态参数值，**不复制**块内几何。
    //   pts[0]   = 插入点（世界坐标；与 BlockDef::basePoint 对齐）
    //   rotation = 插入旋转角(度)（复用；与椭圆/剖面线方向角同字段）
    // 渲染 / 命中 / 导出时按 BlockDef 实时展开（SketchModel::expandBlockRef）。
    // 因此「块内图元被重定义后，所有引用同步更新」天然成立：全球共享同一份定义。
    QString blockName;                        // 引用的块定义名（唯一键）
    double  scaleX = 1.0;                     // X 方向插入缩放
    double  scaleY = 1.0;                     // Y 方向插入缩放
    std::vector<BlockAttrValue> blockAttrs;   // 属性值（tag→value）
    std::vector<BlockDynValue>  dynValues;    // 动态参数取值

    // 便捷查询：块引用的属性值（不存在返回 fallback）
    QString blockAttr(const QString& tag, const QString& fallback = QString()) const {
        for (const auto& a : blockAttrs) if (a.tag == tag) return a.value;
        return fallback;
    }
    // 便捷查询：动态参数取值（不存在返回 fallback）
    double blockDyn(const QString& name, double fallback = 0.0) const {
        for (const auto& d : dynValues) if (d.name == name) return d.value;
        return fallback;
    }

    Element() = default;
    QString typeName() const { return elementTypeName(type); }
    QString sizeDesc() const;            // 主尺寸描述

    // 折线类图元是否按闭合处理。样条一律开放，所以 closed 字段的默认 true
    // 对它无副作用；矩形/多边形被剪切后会被置成 false。
    bool polyClosed() const {
        return (type == ElementType::Rectangle || type == ElementType::Polygon) && closed;
    }
};

// ============================================================================
// 图元序列化（单一实现，核心层）
// ----------------------------------------------------------------------------
// 顶层图纸（MainWindow::writeDrawing）与块定义内图元（Block.cpp）共用这一对函数，
// 避免"同一份字段表写两遍、改一处漏一处"的漂移。所有字段对未知类型无副作用，
// 旧存档缺键时一律回落默认值（向后兼容）。
// ============================================================================
QJsonObject elementToJson(const Element& e);
Element     elementFromJson(const QJsonObject& o);

} // namespace SketchCAD
