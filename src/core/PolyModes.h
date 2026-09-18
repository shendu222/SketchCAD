#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 多边形绘制子模式：把「菜单上的每一种多边形画法」映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
// 架构完全参照 CircleModes.h / LineModes.h。
namespace SketchCAD {
namespace polygon {

enum class Mode {
    Free,                  // 自由多边形（顶点序列，右键闭合）
    RegularInscribed,      // 正多边形(内接)：圆心 + 顶点
    RegularCircumscribed,  // 正多边形(外切)：圆心 + 边中点
    RegularEdge,           // 正多边形(边长)：边起点 + 边终点
    Rectangle,             // 矩形：对角两点
    RectChamfer,           // 倒角矩形：对角两点 + 倒角距离
    RectFillet,            // 圆角矩形：对角两点 + 圆角半径
    Star                   // 星形：圆心 + 外顶点
};

struct Result {
    bool valid = false;
    std::vector<Vec3> pts;                      // 多边形顶点序列
    ElementType type = ElementType::Polygon;    // 结果图元类型（矩形用 Rectangle，其余用 Polygon）
    QString msg;                                // 失败原因或提示
    std::vector<std::vector<Vec3>> altPolys;    // 备选解（如正多边形边长方向有两种）
};

// 主入口：根据模式、已收集点、参考图元 ID、当前光标、图元列表与参数，
// 计算当前应绘制的多边形顶点序列（预览或最终）。
// param: 倒角距离 / 圆角半径（RectChamfer / RectFillet 模式使用）
// sides: 正多边形边数 / 星形角数（Regular*/Star 模式使用）
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param = 0.0,
               int sides = 6);

// 字符串工具：将 activeTool 的 id 转成 Mode；无法识别返回 Free。
Mode modeFromString(const QString& s);
QString stringFromMode(Mode m);

// 各模式的需求描述（参照 CircleModes::ReqCount）
struct ReqCount {
    int refs;              // 需要拾取的参考图元数量
    int pts;               // 需要拾取的点数量（不含光标预览点）
    bool usesParam;        // 是否使用参数输入条（边数或倒角/圆角尺寸）
    bool rightClickFinish; // 是否需要右键结束（自由多边形）
};
ReqCount requirements(Mode m);
std::vector<ElementType> allowedTypes(Mode m);

// ---------- 原子几何构造（可被单独测试） ----------

// 正多边形(内接)：圆心 center，外接圆上一点 vertex，边数 sides
Result regularInscribed(const Vec3& center, const Vec3& vertex, int sides);

// 正多边形(外切)：圆心 center，边中点 edgeMid，边数 sides
Result regularCircumscribed(const Vec3& center, const Vec3& edgeMid, int sides);

// 正多边形(边长)：边起点 p1，边终点 p2，边数 sides，cur 用于选择上下两侧
Result regularEdge(const Vec3& p1, const Vec3& p2, int sides, const Vec3& cur);

// 矩形：对角两点（结果类型为 Rectangle，pts 仅存对角两点）
Result rectangle(const Vec3& p1, const Vec3& p2);

// 倒角矩形：对角两点 + 倒角距离（结果类型为 Polygon，8 个顶点）
Result rectChamfer(const Vec3& p1, const Vec3& p2, double chamfer);

// 圆角矩形：对角两点 + 圆角半径（结果类型为 Polygon，每角用圆弧近似）
Result rectFillet(const Vec3& p1, const Vec3& p2, double filletRadius);

// 星形：圆心 center，外顶点 outerVertex，角数 points，内截比 innerRatio
Result star(const Vec3& center, const Vec3& outerVertex, int points, double innerRatio = 0.5);

} // namespace polygon
} // namespace SketchCAD
