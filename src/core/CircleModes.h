#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 圆形绘制子模式：把菜单上的每一种圆画法映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
// 设计参照 LineModes.h 的架构：Mode 枚举 → compute() 主入口 → 原子构造函数。
namespace SketchCAD {
namespace circle {

// 12 种圆形绘制方式，对应附图菜单项 1-12
enum class Mode {
    CenterPoint,     // 1.  圆心 + 圆周点（半径 = 两点距离）
    TwoPoint,        // 2.  两点画圆（两点为直径端点）
    TwoPointRadius,  // 3.  两点 + 半径（两点定义弦，半径决定圆弧曲率）
    ThreePoint,      // 4.  三点定圆（外接圆）
    CenterRadius,    // 5.  圆心 + 半径数值（参数输入条）
    Tan2Ent1Pt,      // 6.  相切两图元 + 过一点
    Tan1Ent2Pt,      // 7.  相切一图元 + 过两点
    Tan2EntRadius,   // 8.  相切两图元 + 半径
    Tan3Ent,         // 9.  相切三图元（阿波罗尼斯问题）
    Inscribed,       // 10. 三直线内切圆（角平分线交点）
    Concentric,      // 11. 同心圆（取已有圆/弧圆心，新半径）
    ByArc            // 12. 由弧转圆（弧→同圆心同半径的完整圆）
};

struct Result {
    bool valid = false;
    Vec3 center = Vec3(0, 0, 0);
    double radius = 0;
    QString msg;
    // 多解场景的备选中心（用于预览时提示其他解）
    std::vector<Vec3> altCenters;
};

// 主入口：根据模式、已收集点、参考图元 ID、当前光标、图元列表与参数，
// 计算当前应绘制的圆（预览或最终）。
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param = 0.0);

Mode modeFromString(const QString& s);
QString stringFromMode(Mode m);

// ---------- 需求描述：各模式需要拾取的参考图元数和点数 ----------
struct ReqCount {
    int refs;         // 需要拾取的参考图元数量
    int pts;          // 需要拾取的点数量（不含光标预览点）
    bool usesParam;   // 是否使用参数输入条的半径值
};
ReqCount requirements(Mode m);

// 各模式允许拾取的参考图元类型
std::vector<ElementType> allowedTypes(Mode m);

// ---------- 原子几何构造（可被单独测试） ----------

// 1. 圆心 + 圆周点
Result centerPoint(const Vec3& c, const Vec3& p);
// 2. 两点画圆（直径）
Result twoPoint(const Vec3& a, const Vec3& b);
// 3. 两点 + 半径（两点定义弦，有上下两解，取靠近 cur 的）
Result twoPointRadius(const Vec3& a, const Vec3& b, double radius, const Vec3& cur);
// 4. 三点定圆
Result threePoint(const Vec3& a, const Vec3& b, const Vec3& c);
// 5. 圆心 + 半径数值
Result centerRadius(const Vec3& c, double radius);

// 6. 相切两图元 + 过一点（数值求解：圆过 P 且与 e1、e2 相切）
Result tan2Ent1Pt(const Element& e1, const Element& e2, const Vec3& pt, const Vec3& cur);
// 7. 相切一图元 + 过两点（圆过 P1、P2 且与 e 相切）
Result tan1Ent2Pt(const Element& e, const Vec3& p1, const Vec3& p2, const Vec3& cur);
// 8. 相切两图元 + 半径（偏移求交法，取靠近 cur 的解）
Result tan2EntRadius(const Element& e1, const Element& e2, double radius, const Vec3& cur);
// 9. 相切三图元（三直线用角平分线；含圆用数值法）
Result tan3Ent(const Element& e1, const Element& e2, const Element& e3, const Vec3& cur);
// 10. 三直线内切圆
Result inscribed(const Element& l1, const Element& l2, const Element& l3);
// 11. 同心圆
Result concentric(const Element& ref, double radius, const Vec3& cur);  // cur reserved for future nearest-solution selection
// 12. 由弧转圆
Result byArc(const Element& arc);

} // namespace circle
} // namespace SketchCAD
