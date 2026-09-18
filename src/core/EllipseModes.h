#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 椭圆绘制子模式：把菜单上的每一种椭圆画法映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
// 设计参照 CircleModes.h 的架构：Mode 枚举 → compute() 主入口 → 原子构造函数。
namespace SketchCAD {
namespace ellipse {

// 6 种椭圆绘制方式，对应附图菜单项
enum class Mode {
    OnePoint,       // 1. Ellipse (1 Point)：中心 + 椭圆上一点（该点作为长轴端点，短轴取长轴一半）
    Axis,           // 2. Ellipse (Axis)：中心 + 长轴端点 + 椭圆上一点
    FociPoint,      // 3. Ellipse Foci Point：两个焦点 + 椭圆上一点
    FourPoint,      // 4. Ellipse 4 Point：四个椭圆上的点，拟合轴对齐椭圆
    Center3Points,  // 5. Ellipse Center and 3 Points：中心 + 椭圆上三个点
    Inscribed       // 6. Ellipse Inscribed：内切于矩形/多边形
};

struct Result {
    bool   valid = false;
    Vec3   center = Vec3(0, 0, 0);
    double radius = 0;       // 长半轴 a
    double radiusY = 0;      // 短半轴 b
    double rotation = 0;     // 长轴与 X 轴正向夹角（度）
    QString msg;
};

// 主入口：根据模式、已收集点、参考图元 ID、当前光标、图元列表与参数，
// 计算当前应绘制的椭圆（预览或最终）。
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param = 0.0);

Mode    modeFromString(const QString& s);
QString stringFromMode(Mode m);

// ---------- 需求描述：各模式需要拾取的参考图元数和点数 ----------
struct ReqCount {
    int refs;         // 需要拾取的参考图元数量
    int pts;          // 需要拾取的几何点数量（不含光标预览点）
    bool usesParam;   // 是否使用参数输入条的值
};
ReqCount requirements(Mode m);

// 各模式允许拾取的参考图元类型
std::vector<ElementType> allowedTypes(Mode m);

// ---------- 原子几何构造（可被单独测试） ----------

// 1. 中心 + 椭圆上一点（作为长轴端点，短轴取长轴的一半）
Result onePoint(const Vec3& c, const Vec3& p);

// 2. 中心 + 长轴端点 + 椭圆上一点
Result axis(const Vec3& c, const Vec3& axisEnd, const Vec3& passPt);

// 3. 两焦点 + 椭圆上一点
Result fociPoint(const Vec3& f1, const Vec3& f2, const Vec3& p);

// 4. 四个椭圆上的点，拟合轴对齐椭圆
Result fourPoint(const std::vector<Vec3>& pts);

// 5. 中心 + 三个椭圆上的点，拟合旋转椭圆
Result center3Points(const Vec3& c, const std::vector<Vec3>& pts);

// 6. 内切于矩形/多边形
Result inscribed(const Element& e);

} // namespace ellipse
} // namespace SketchCAD
