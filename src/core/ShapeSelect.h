#pragma once
#include <QString>
#include <vector>
#include "Geometry.h"   // Seg
#include "Vec.h"

namespace SketchCAD {

class Element;

// ============================================================
// ShapeSelect —— 栏选(Fence) / 圈围(WP) / 圈交(CP) 纯几何判定层
// ------------------------------------------------------------
// 三种不规则范围选择的统一真相源：画布交互（ShapeSelectTool）、
// 单元测试共用同一份判定代码，杜绝两条路径漂移。
//
//   * Fence    栏选：多段折线，图元任一段与栏线相交即命中（穿过即选中）；
//   * Window   圈围：多边形，图元完全位于多边形内才命中（全包含）；
//   * Crossing 圈交：多边形，图元任一部分与多边形相交/在内即命中。
//
// 所有函数不触碰 SketchModel（headless 可测）；BlockRef 的展开判定
// 由调用方（GraphicsView）负责——core 层只认「单图元几何」。
// ============================================================
namespace shape {

enum class Mode { Fence = 0, Window = 1, Crossing = 2 };

QString modeName(Mode m);   // 栏选 / 圈围 / 圈交

// ---- 图元采样 ----
// 采样点集：WP 的「全部点在多边形内」与点图元（Text 等）的近邻判定用。
// 直线/矩形/多边形 → 顶点；圆 → 24 等分点；弧 → 沿弧 24 点；
// 椭圆/样条 → 既有采样函数；其余类型回落到 pts 全体。
std::vector<Vec3> samplePoints(const Element& e);

// 线段化：Fence/CP 的相交判定用。优先复用 geo::flatten（求交同源），
// 圆/弧（flatten 返回空）按 24 段圆周逼近补齐。
std::vector<geo::Seg> segments(const Element& e);

// ---- 判定 ----
// 线段 ab 是否与多边形任一边相交
bool segCrossesPolygon(const Vec3& a, const Vec3& b, const std::vector<Vec3>& poly);
// 点 p 是否落在多边形内（含边界：内部 or 到任一边距离 ≤ tol）
bool pointInPolygonTol(const Vec3& p, const std::vector<Vec3>& poly, double tol);

// 单图元命中判定。tol：点图元近邻 / 边缘贴合的世界容差（mm）。
// 形状退化（Fence < 2 点、WP/CP < 3 点）一律不命中。
bool hit(const Element& e, const std::vector<Vec3>& shapePts, Mode m, double tol);

// 批量：返回命中图元 id（保持 elems 顺序；跳过不可见图元）。
std::vector<int> pick(const std::vector<Element>& elems,
                      const std::vector<Vec3>& shapePts, Mode m, double tol);

} // namespace shape
} // namespace SketchCAD
