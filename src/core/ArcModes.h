#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 圆弧绘制子模式：把菜单上的每一种圆弧画法映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
// 设计参照 CircleModes.h 的架构：Mode 枚举 → compute() 主入口 → 原子构造函数。
//
// 附图 11 种圆弧类型：
//   1. Center, Point, Angles      圆心 + 点 + 角度扫掠
//   2. Center, Point, Chord       圆心 + 点 + 弦长定终点
//   3. Center, Point, Arc Angle   圆心 + 点 + 弧角（参数条输入扫掠角）
//   4. 3 Points                   三点定弧（自动判定凹凸侧）
//   5. 2 Points, Angle            起终点 + 夹角（参数条）
//   6. 2 Points, Radius           起终点 + 半径（两解，光标选近解）
//   7. 2 Points, Length           起终点 + 弦长（等价于半径，但用户输入弦长）
//   8. 2 Points, Height           起终点 + 弦高（凸度/sagitta）
//   9. Arc Tangential             相切延续弧（接已有图元端点）
//  10. Ellipse Arc (Axis)         椭圆弧（轴定义）
// 11. Ellipse Arc (1 Point)       椭圆弧（一点定义）
namespace SketchCAD {
namespace arc {

// 11 种圆弧绘制方式（含 2 种椭圆弧）
enum class Mode {
    CenterPointAngles,    // 1.  圆心 + 点 + 角度扫掠（现有 "arc" 行为，保持兼容）
    CenterPointChord,     // 2.  圆心 + 点 + 弦长
    CenterPointArcAngle,  // 3.  圆心 + 点 + 弧角（参数输入条）
    ThreePoint,           // 4.  三点圆弧（起点 / 弧上点 / 终点）
    TwoPointAngle,        // 5.  起点 + 终点 + 夹角
    TwoPointRadius,       // 6.  起点 + 终点 + 半径（两解，光标选近解）
    TwoPointLength,       // 7.  起点 + 终点 + 弦长
    TwoPointBulge,        // 8.  起点 + 终点 + 弦高点（凸度）
    TangentContinue,      // 9.  相切延续弧（接续已有图元端点并与之相切）
    EllipseArcAxis,       // 10. 椭圆弧（轴定义：中心 + 轴端点 + 角度点）
    EllipseArc1Pt         // 11. 椭圆弧（一点定义：轴 + 过点）
};

// 计算结果：与 circle::Result 同构，额外带起止角
struct Result {
    bool   valid = false;
    Vec3   center = Vec3(0, 0, 0);
    double radius = 0;
    double startAngle = 0;   // 度，数学正方向（逆时针为正）
    double endAngle   = 0;   // 度，endAngle - startAngle 的符号即扫掠方向，不做归一化
    QString msg;
    std::vector<Vec3> altCenters;   // 多解场景的备选圆心（预览虚线提示）

    // 椭圆弧扩展字段（Mode::EllipseArc* 时有效）
    double radiusY = 0;       // 椭圆短轴（radius 为长轴）
};

// 主入口：签名与 circle::compute 完全一致，方便复用 GraphicsView 的三段式状态机
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param = 0.0);

Mode    modeFromString(const QString& s);
QString stringFromMode(Mode m);

// ---------- 需求描述：各模式需要拾取的参考图元数和点数 ----------
struct ReqCount { int refs; int pts; bool usesParam; };
ReqCount requirements(Mode m);

// 各模式允许拾取的参考图元类型
std::vector<ElementType> allowedTypes(Mode m);

// ---------- 原子几何构造（可被 selftest.cpp 单独测试） ----------

// 1. 圆心 + 圆周点 + 光标位置（角度扫掠，与现有 finalizeArc 行为一致）
Result centerPointAngles(const Vec3& c, const Vec3& s, const Vec3& e);

// 2. 圆心 + 圆周点 + 弦长（沿圆周到距离=chord处取终点）
Result centerPointChord(const Vec3& c, const Vec3& p, double chord, const Vec3& cur);

// 3. 圆心 + 半径 + 扫掠角（参数条输入角度）
Result centerPointArcAngle(const Vec3& c, double radius, const Vec3& cur, double sweepDeg);

// 4. 三点圆弧（起点 / 弧上点 / 终点，自动判定凹凸侧）
Result threePoint(const Vec3& s, const Vec3& mid, const Vec3& e);

// 5. 起终点 + 夹角（参数条输入圆心角）
Result twoPointAngle(const Vec3& s, const Vec3& e, double angleDeg, const Vec3& cur);

// 6. 起终点 + 半径（两解，光标选近解）
Result twoPointRadius(const Vec3& s, const Vec3& e, double radius, const Vec3& cur);

// 7. 起终点 + 弦长（内部转为半径计算）
Result twoPointLength(const Vec3& s, const Vec3& e, double length, const Vec3& cur);

// 8. 起终点 + 弦高点（凸度 / sagitta）
Result twoPointBulge(const Vec3& s, const Vec3& e, const Vec3& bulgePt);

// 9. 相切延续弧（接续已有图元在其端点处相切）
Result tangentContinue(const Element& ref, const Vec3& from, const Vec3& to);

// 10. 椭圆弧（轴定义）：中心 + 长轴端点 + 角度点
Result ellipseArcAxis(const Vec3& center, const Vec3& axisEnd, const Vec3& anglePt);

// 11. 椭圆弧（一点定义）：中心 + 长轴端点 + 过点
Result ellipseArc1Pt(const Vec3& center, const Vec3& axisEnd, const Vec3& passPt);

} // namespace arc
} // namespace SketchCAD
