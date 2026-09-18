#pragma once
#include "Vec.h"
#include "Element.h"
#include <vector>
#include <QString>

// 样条曲线绘制子模式：把「菜单上的每一种样条画法」映射为纯几何计算，
// 与 UI 解耦，可被命令行、无界面自测和视图层共用。
// 架构完全参照 PolyModes.h / CircleModes.h / ArcModes.h。
namespace SketchCAD {
namespace spline {

enum class Mode {
    Free,                 // 自由样条（控制点序列，右键结束，Catmull-Rom 插值）
    Parabola,             // 抛物线（3 点：起点 + 切线控制点 + 终点 → 二次贝塞尔）
    Hyperbola,            // 双曲线（2 焦点 + 曲线上一点 → 采样渲染）
    Parabola4P,           // 抛物线（4 点：拟合二次贝塞尔抛物线）
    ParabolaFocusDirectrix, // 抛物线（焦点 + 准线两点）
    ThroughPoints         // 样条过点（逐点插值，Catmull-Rom）
};

// 样条曲线子类型编码（与 Element::splineSubType 对应）。
// 0/1/2 为历史已有值，顺序只增不改，以保证旧 .sketch 文件兼容。
enum class SubType {
    CatmullRom = 0,
    Parabola = 1,
    Hyperbola = 2,
    Parabola4P = 3,
    ParabolaFocusDirectrix = 4,
    ThroughPoints = 5,
    Freehand = 6
};

struct Result {
    bool valid = false;
    std::vector<Vec3> pts;          // 定义点（抛物线=3控制点，双曲线=2焦点+1点，4点抛物线=4点）
    int subType = 0;                // 对应 SubType 整数值
    QString msg;                    // 失败原因或提示
};

// 主入口：根据模式、已收集点、当前光标，计算当前应绘制的曲线定义点。
Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const Vec3& cur);

// 字符串工具
Mode modeFromString(const QString& s);
QString stringFromMode(Mode m);

// 各模式的需求描述
struct ReqCount {
    int pts;               // 需要拾取的点数量（不含光标预览点）
    bool rightClickFinish; // 是否需要右键结束（自由样条）
};
ReqCount requirements(Mode m);

// ---------- 原子几何构造（可被单独测试） ----------

// 抛物线：3 个控制点 P0(起点)、P1(切线交点)、P2(终点)
// 二次贝塞尔曲线 B(t) = (1-t)²P0 + 2(1-t)t·P1 + t²P2 本质上是抛物线段
Result parabola(const Vec3& p0, const Vec3& p1, const Vec3& p2);

// 双曲线：2 个焦点 F1、F2 + 曲线上一点 P
// |PF1 - PF2| = 2a（定差），c = |F1F2|/2，要求 c > a（否则退化为直线）
Result hyperbola(const Vec3& f1, const Vec3& f2, const Vec3& p);

// 4 点抛物线：p0/p3 为两端点，p1/p2 为曲线上两个插值点，
// 用弦长参数化最小二乘拟合一条二次贝塞尔抛物线。
Result parabola4P(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3);

// 焦点准线抛物线：focus 为焦点，d1/d2 为准线上两点。
Result parabolaFocusDirectrix(const Vec3& focus, const Vec3& d1, const Vec3& d2);

// 过点样条：pts 为必须经过的点序列，结果 subType=ThroughPoints。
Result throughPoints(const std::vector<Vec3>& pts);

// ---------- 样条编辑（纯几何，UI / CLI / 自测共用） ----------

// 折线类图元 → 样条定义点（Spline from polyline）。
//   Line：两端点；Polygon：全部顶点；Rectangle：按对角点展开为 4 个角点。
// 其它类型或顶点数不足时返回空序列。
std::vector<Vec3> polylineControlPoints(const Element& src);

// 样条炸开为线段端点对（Explode spline to lines）。
//   - 插值类样条（CatmullRom=0 / ThroughPoints=5 / Freehand=6）：
//     控制点即曲线节点，按相邻节点弦生成 n-1 条线段（与 FreeCAD 同语义）。
//   - 圆锥曲线类（Parabola=1 / Hyperbola=2 / Parabola4P=3 / ParabolaFocusDirectrix=4）：
//     定义点不都在曲线上，按参数采样离散为 samples 段再逐段成线，
//     保证炸开后形状仍是原曲线（若直接连定义点会完全丢失形状）。
// 点数不足 2 或采样退化为单点时返回空。
std::vector<std::pair<Vec3, Vec3>> explodeToLines(const std::vector<Vec3>& pts,
                                                   int subType, int samples = 24);

// ---------- 采样（渲染 / 命中检测用） ----------

// 双曲线采样：把 3 个定义点（F1, F2, P）离散为折线点序列
// samples 为每条分支的采样数
std::vector<Vec3> sampleHyperbola(const Vec3& f1, const Vec3& f2, const Vec3& p,
                                   int samples = 64);

// 抛物线采样：把 3 个控制点离散为折线点序列
std::vector<Vec3> sampleParabola(const Vec3& p0, const Vec3& p1, const Vec3& p2,
                                  int samples = 48);

// 4 点抛物线采样：pts 必须包含 4 个定义点（前 3 个为二次贝塞尔控制点，第 4 个仅用于拟合）
std::vector<Vec3> sampleParabola4P(const std::vector<Vec3>& pts, int samples = 48);

// 焦点准线抛物线采样
std::vector<Vec3> sampleParabolaFD(const Vec3& focus, const Vec3& d1, const Vec3& d2,
                                    int samples = 64);

// 过点样条采样：等价于 Catmull-Rom 采样
std::vector<Vec3> sampleThroughPoints(const std::vector<Vec3>& pts, int samples = 48);

} // namespace spline
} // namespace SketchCAD
