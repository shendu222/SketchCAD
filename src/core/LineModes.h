#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 直线绘制子模式：把「菜单上的每一种直线画法」映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
namespace SketchCAD {
namespace line {

enum class Mode {
    TwoPoint,              // 2 Points
    Horizontal,            // Horizontal
    Vertical,              // Vertical
    Angle,                 // Angle（固定角度射线）
    Parallel,              // Parallel（平行于参考线）
    ParallelThroughPoint,  // Parallel through point
    Bisector,              // Bisector（角平分线）
    TangentPC,             // Tangent (P,C)  点→圆切线
    TangentCC,             // Tangent (C,C)  两圆公切线
    TangentOrthogonal,     // Tangent Orthogonal（切线且垂直于参考线）
    Orthogonal,            // Orthogonal（垂直于参考线）
    RelativeAngle,         // Relative angle（相对参考线成角）
    Snake,                 // Snake（正交折线，自动交替方向）
    SnakeX,                // Snake (X)（以水平方向开始）
    SnakeY,                // Snake (Y)（以竖直方向开始）
    AngleFromLine,         // Angle From Line（Relative angle 的别名）
    OrthogonalFromLine,    // Orthogonal From Line（在参考线上某点作垂线）
    FromPointToLine        // From Point To Line（点到直线的垂线）
};

struct Result {
    bool valid = false;
    Vec3 a = Vec3(0, 0, 0);
    Vec3 b = Vec3(0, 0, 0);
    QString msg;          // 失败原因或提示
    // 备选切线（用于两圆公切线预览：显示所有候选切线，高亮最近的那条）
    std::vector<std::pair<Vec3, Vec3>> altLines;
};

// 子模式的交互元数据（D‑数据驱动）：drawing 采集需要多少个参考图元/定位点、
// 是否支持右键提前结束。供 UI 提示阶段机与采集流程共用，避免在各族 if 里手写计数。
struct ReqCount {
    int refs = 0;                 // 需要拾取的参考图元数
    int pts = 0;                  // 需要收集的定位点数（光标驱动的取 0）
    bool rightClickFinish = false;// 收集途中右键即结束（如 Snake）
};
ReqCount requirements(Mode m);
std::vector<ElementType> allowedTypes(Mode m);

// 主入口：根据模式、已收集点、参考图元 ID、当前光标、图元列表与参数，
// 计算当前应绘制的线段端点（预览或最终）。
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param = 0.0);

// 字符串工具：将 activeTool 的 id 转成 Mode；无法识别返回 TwoPoint。
Mode modeFromString(const QString& s);
QString stringFromMode(Mode m);

// ---------- 原子几何构造（可被单独测试） ----------
Result twoPoint(const Vec3& a, const Vec3& b);
Result horizontal(const Vec3& a, const Vec3& b);
Result vertical(const Vec3& a, const Vec3& b);
Result angle(const Vec3& a, const Vec3& b, double angleDeg);

// 方向约束构造：参考线 refLine 必须是直线（Line）或矩形边（Rectangle）。
Result parallelThroughPoint(const Element& refLine, const Vec3& p, const Vec3& cur);
// 过点平行（保持参考线长度与方向，沿垂直方向对齐到该点）——对应菜单「Parallel through point」。
Result parallelAligned(const Element& refLine, const Vec3& p);
Result orthogonalThroughPoint(const Element& refLine, const Vec3& p, const Vec3& cur);

// 角度 + 长度 → 第二点（供「参数输入条」的角度直线：选起点后按输入角度/长度直接绘制）。
Vec3 pointAtAngleLength(const Vec3& a, double angleDeg, double length);

// 平行复制（对应菜单「Parallel」）：参考线 refLine，sidePoint 决定偏移方向
// （投影到参考线垂直轴的正负），沿垂直方向按 spacing 间隔生成 count 条、
// 长度与方向同参考线的平行线。算法独立，便于无界面自测。
struct ParallelCopies {
    bool valid = false;
    std::vector<std::pair<Vec3, Vec3>> lines;   // 每条平行线的 (起点, 终点)
    QString msg;
};
ParallelCopies parallelCopies(const Element& refLine, const Vec3& sidePoint,
                              double spacing, int count);
Result relativeAngle(const Element& refLine, const Vec3& p, const Vec3& cur, double angleDeg);
Result orthogonalFromLine(const Element& refLine, const Vec3& pOnLine, const Vec3& cur);
Result fromPointToLine(const Element& refLine, const Vec3& p);

// 角平分线：l1、l2 为两条直线，p 用于决定长度/方向。
Result bisector(const Element& l1, const Element& l2, const Vec3& p, const Vec3& cur);

// 切线
Result tangentPC(const Element& circle, const Vec3& p, const Vec3& cur);
Result tangentCC(const Element& c1, const Element& c2, const Vec3& cur);
Result tangentOrthogonal(const Element& circle, const Element& refLine, const Vec3& cur);

// Snake：根据已有点序列与光标，追加下一段正交折线。
// xFirst 控制第一段方向：true=水平优先，false=竖直优先，
// 后续段自动交替。返回新追加的线段。
Result snakeSegment(const std::vector<Vec3>& pts, const Vec3& cur, bool xFirst);

} // namespace line
} // namespace SketchCAD
