#pragma once
#include <vector>
#include <QRectF>
#include "Vec.h"
#include "Element.h"

// 纯几何计算层：不依赖任何 UI，可被图形视图、命令行、DXF 导出与无界面自测共用。
// 之所以单独抽出来，是因为「样条平滑」「三点定圆」「命中检测」三件事各自都需要
// 同一批基础运算，散落在 UI 里既没法回归测试，也一定会出现多份不一致的实现。
namespace SketchCAD {
namespace geo {

// ---------- 基础 ----------
double distPointSegment(const Vec3& p, const Vec3& a, const Vec3& b);

// 端点/坐标重合判定（容差 tol，单位 mm）。
// JOIN 端点连合、阵列退化检测、剪切容差等所有「两个点算不算同一个点」的判定
// 统一走这里，避免各模块自写 fabs(dx)<eps 时 eps 取值漂移。
inline bool almostEqual(const Vec3& a, const Vec3& b, double tol) {
    return a.distTo(b) <= tol;
}

// 角度规整
double wrapDeg180(double d);   // -> (-180, 180]
double wrapDeg360(double d);   // -> [0, 360)
// a 是否落在以 start 起、扫掠 sweep 度（可正可负）的圆弧角范围内
bool   angleInSweep(double a, double start, double sweep);

// ---------- 样条 ----------
// Catmull-Rom 插值 -> 三次贝塞尔控制点序列。
// 返回 [P0, c1,c2,P1, c1,c2,P2, ...]，长度 = 1 + 3*(n-1)。
// 选 Catmull-Rom 的理由：它是「插值型」样条，曲线严格通过每个控制点，
// 符合用户在画布上点哪儿曲线就过哪儿的直觉；B 样条是「逼近型」，不过点。
std::vector<Vec3> catmullRomToBezier(const std::vector<Vec3>& pts);

// 按每段 perSeg 段折线采样样条（命中检测 / DXF 导出用）
std::vector<Vec3> sampleSpline(const std::vector<Vec3>& pts, int perSeg = 16);

// ---------- 椭圆 ----------
// 按 N 段采样椭圆（命中检测 / 离散化 / 渲染兜底用）。
// center=圆心，a=长半轴，b=短半轴，rotationDeg=长轴与 +X 轴夹角（世界坐标，逆时针为正）。
std::vector<Vec3> sampleEllipse(const Vec3& center, double a, double b,
                                double rotationDeg, int n = 160);

// ---------- 折线（Polyline：含直线段与圆弧段混合）----------
// 弧段几何：arcFlags[i]=true 表示 pts[i]→pts[i+1] 是圆弧段，
// 圆心由 (pts[i-1], pts[i], pts[i+1]) 三个相邻节点的外接圆决定（边界段退化为以端点为圆心）。
struct ArcSeg { Vec3 center; double radius; double startDeg; double sweepDeg; bool valid; };
ArcSeg arcThrough(const std::vector<Vec3>& pts, int segIdx, bool closed);

// 按 N 段采样整条折线（直线段直接 lineTo，弧段沿圆周采样 perSeg 段）。
// 返回折线轨迹的密集点列（含起止点，不重复相邻段端点）。
std::vector<Vec3> samplePolyline(const std::vector<Vec3>& pts,
                                 const std::vector<bool>& arcFlags,
                                 bool closed, int perSeg = 16);

// 折线「拟合曲线」（PEDIT Fit）：平滑曲线**严格通过**所有顶点（Catmull-Rom 插值）。
// 支持闭合（首尾节点间也用曲线相连）。用于 polyline.splineSubType == 1 的渲染/命中/离散。
std::vector<Vec3> samplePolylineFit(const std::vector<Vec3>& pts, bool closed, int perSeg = 16);

// 折线条样曲线（PEDIT Spline）：顶点作为三次 B 样条的控制点（逼近型，
// 端点靠近首尾顶点、中间顶点一般不被曲线穿过，与 AutoCAD PEDIT 样条语义一致）。
// 支持闭合。用于 polyline.splineSubType == 2 的渲染/命中/离散。
std::vector<Vec3> samplePolylineSpline(const std::vector<Vec3>& pts, bool closed, int perSeg = 16);

// ---------- 圆 ----------
// 三点定圆（外接圆）。三点共线时返回 false。
bool circleFrom3Points(const Vec3& a, const Vec3& b, const Vec3& c,
                       Vec3& center, double& radius);

// ---------- 直线极坐标属性 ----------
double segLength(const Vec3& a, const Vec3& b);
// 有向线段与 X 轴正方向的夹角，返回 [0, 360)。零长度段返回 0。
double segAngleDeg(const Vec3& a, const Vec3& b);
// 由起点 + 长度 + 夹角反算终点（属性面板「改长度/改夹角」时用）
Vec3   endpointFrom(const Vec3& start, double length, double angleDeg);

// 标注的实测值：线性/对齐返回长度(mm)，角度返回度数，半径/直径返回相应长度。
// 放在几何层而不是渲染层，是为了让 BOM 列表、属性面板、DXF 导出读到同一个数。
double measureDimension(const Element& e);

// 直线标注的类型归一（修复：直线应标真实长度而非坐标轴投影）。
// 当标注入口以「点选直线」为源时，除非用户显式要求水平/竖直，否则一律退化为对齐标注
// （尺寸线平行于直线本身、量真实长度）；其它图元原样透传。UI 与自测共用，保证决策唯一。
inline DimensionType lineDimType(DimensionType requested) {
    return (requested == DimensionType::Horizontal || requested == DimensionType::Vertical)
               ? requested : DimensionType::Aligned;
}

// ---------- 命中检测 ----------
// 点到图元真实几何的最短距离（世界单位）。用于替代「包围盒命中」。
double distanceToElement(const Element& e, const Vec3& p);

// 点到一组图元的最短距离（取各图元最小值）。块引用命中即对"展开后的子图元集合"
// 调用本函数：点块内任一图形都能命中整个块。
double distanceToElements(const std::vector<Element>& es, const Vec3& p);

// ---------- 控制点（端点拖拽 / 捕捉）----------
// 图元的可拖拽控制点。索引语义按类型固定，setElementHandle 与之严格对应：
//   直线   [起点, 终点]
//   矩形   [左下, 右下, 右上, 左上]（由对角两点导出，拖任一角改写对应分量）
//   圆     [圆心, 半径手柄(圆心右侧象限点)]
//   圆弧   [圆心, 起点, 终点]
//   多边形/样条/标注  所有 pts
std::vector<Vec3> elementHandles(const Element& e);
// 把第 idx 号控制点移到 p（越界则忽略）。返回是否发生了修改。
bool setElementHandle(Element& e, int idx, const Vec3& p);

// ---------- 离散化与求交 ----------
struct Seg { Vec3 a, b; };

// 把图元离散成线段集合。圆与圆弧不离散（返回空），它们走解析求交。
std::vector<Seg> flatten(const Element& e, int splineSeg = 24);

// 线段 × 线段：仅当交点同时落在两条线段内才算命中
bool segSegIntersect(const Vec3& p1, const Vec3& p2,
                     const Vec3& q1, const Vec3& q2, Vec3& out);
// 线段 × 圆：返回落在线段内的交点个数 0/1/2
int  segCircleIntersect(const Vec3& p1, const Vec3& p2,
                        const Vec3& c, double r, Vec3 out[2]);
// 圆 × 圆：0/1/2（同心或不相交返回 0）
int  circleCircleIntersect(const Vec3& c1, double r1,
                           const Vec3& c2, double r2, Vec3 out[2]);

// 两个图元的全部交点（已按圆弧角度范围、线段范围过滤，并去重）
std::vector<Vec3> intersectElements(const Element& a, const Element& b);

// 图元在世界坐标系下的几何包围盒（圆/椭圆按完整范围采样）。
// 命中检测与框选命中判断共用：比「控制点包围盒」更准（能覆盖圆弧外凸、椭圆旋转）。
QRectF elementBounds(const Element& e);

// ---------- 折线参数化（剪切用）----------
// 折线总长；closed 时含收尾段
double polylineLength(const std::vector<Vec3>& v, bool closed);
// 弧感知精确周长：arcFlags[i]=true 的段用 arcThrough 外接圆算真实弧长 |sweep|·r，
// 直线段用弦长。与渲染采样同源，替代旧「弦长×1.05」粗估。
double polylineArcLength(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed);
// 弧感知有向面积（CCW 为正）：直线段贡献 shoelace 项 ½(xₐy_b−x_b yₐ)；
// 弧段贡献弓形修正 ½[r²Δ + cx(b.y−a.y) + cy(a.x−b.x)]（Δ 为有向扫掠弧度）。
// 退化弧（arcThrough 失效）按直线段处理。
double polylineSignedArea(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed);
// 点 p 投影到折线上的累积弧长参数
double polylineParam(const std::vector<Vec3>& v, bool closed, const Vec3& p);
// 按累积弧长参数取点
Vec3   polylinePointAt(const std::vector<Vec3>& v, bool closed, double t);
// 用 cuts（累积弧长）切分折线，丢弃包含 tClick 的那一段，返回剩余的若干条折线。
// closed=true 时参数空间是环，结果必定是 0 或 1 条开放折线。
std::vector<std::vector<Vec3>> splitPolyline(const std::vector<Vec3>& v, bool closed,
                                             std::vector<double> cuts, double tClick);

// ---------- 剖面线填充（GB/T 4457.5-2013 剖面区域表示）----------
// 点在多边形内（射线法 / even-odd 奇偶规则）。poly 为封闭点列（不要求首尾重复）。
bool pointInPolygon(const Vec3& p, const std::vector<Vec3>& poly);

// 把任意（可带内孔、可非凸）闭合多边形区域用等距平行线填充。
// region：外边界点列（世界坐标，自动闭合）；holes：内孔点列（每个自动闭合，布尔减）。
// angleDeg：剖面线方向角（度，与 +X 轴正向夹角，逆时针为正）；spacing：线间距(mm)；
// margin：扫描包围盒外扩量(mm)，保证边缘线不被截断。
// 算法：把坐标系旋转到「剖面线水平」，做水平扫描线求交（even-odd 配对，
// 内孔同样参与配对 → 自动掏空），再把交点逆旋转回世界坐标。返回裁剪到区域内的线段。
//
// trustedClosed：
//   * false（默认）——调用方直接传入"裸点列"，无法保证其闭合。此时会启用一道启发式
//     防御（见 Geometry.cpp regionLooksClosed）：若闭合缝远大于内部最长边，判定为
//     "疑似开放曲线被误闭合"并**拒绝填充**（避免渗漏/溢出）。所有历史调用方维持此行为。
//   * true——调用方已在上游权威校验闭合性（如 Section.cpp 的 sectionOutline /
//     buildClosedLoopsFromElements：首尾距 ≤ 容差 且 面积 > 1e-6）。此时**跳过**该启发式，
//     避免它误拒"单一主导收尾缝"的合法闭合多边形（细长针状/月牙状）。闭合性的权威
//     判定应唯一地留在上游，下游不得二次启发式否决。
std::vector<Seg> hatchLines(const std::vector<Vec3>& region,
                            const std::vector<std::vector<Vec3>>& holes,
                            double angleDeg, double spacing, double margin = 0.0,
                            bool trustedClosed = false);

// ---------- 质心计算（参数修改基点）----------
// 各类图元的几何质心：
//   直线     → 两端点中点
//   圆/圆弧  → 圆心
//   矩形     → 对角线交点
//   多边形   → 顶点算术平均（重心近似）
//   样条     → 控制点算术平均
Vec3 elementCentroid(const Element& e);

// 以质心为基点缩放折线类图元：所有顶点相对质心按 factor 缩放。
// 返回新的顶点数组，原数组不变。
std::vector<Vec3> scaleAroundCentroid(const std::vector<Vec3>& pts, const Vec3& centroid, double factor);

// 以质心为基点旋转折线类图元：所有顶点绕质心旋转 angleDeg 度。
std::vector<Vec3> rotateAroundCentroid(const std::vector<Vec3>& pts, const Vec3& centroid, double angleDeg);

// ---------- 约束关系检测 ----------
// 判断两图元是否满足给定的约束类型（容差 tol，单位 mm/度）
bool checkConstraint(const Element& a, const Element& b, ConstraintType ct, double tol = 0.5);

// 判断直线与圆是否相切（距离 ≈ 半径）
bool isTangentLineCircle(const Element& line, const Element& circle, double tol = 0.5);
// 判断两直线是否垂直（夹角 ≈ 90°）
bool isPerpendicularLines(const Element& la, const Element& lb, double tol = 1.0);
// 判断两直线是否平行（夹角 ≈ 0° 或 180°）
bool isParallelLines(const Element& la, const Element& lb, double tol = 1.0);
// 判断两圆是否相切（圆心距 ≈ 半径和/差）
bool isTangentCircles(const Element& ca, const Element& cb, double tol = 0.5);

// ---------- 约束维持（参数修改后自动调整关联图元）----------
// 当图元 a 发生参数变化后，检查与 b 之间原先存在的约束关系，
// 调整 b 的几何以恢复约束。返回 b 是否被修改。
bool restoreConstraint(const Element& a, Element& b, ConstraintType ct, double tol = 0.5);

} // namespace geo
} // namespace SketchCAD
