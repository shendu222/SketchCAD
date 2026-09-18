#pragma once
#include <vector>
#include "Vec.h"
#include "Element.h"

// 直线绘制时的「智能捕捉」几何层：把「鼠标附近有哪些可吸附的几何特征点」这件事
// 完全算清楚，UI 只负责把吸附点画出来、把拾取点替换成吸附点。
//
// 五种捕捉类型（对应需求）：
//   Endpoint     直线/弧/矩形/多边形/样条的端点或顶点
//   Center       圆/弧的圆心
//   Intersection 两图元的交点（直线×直线、直线×圆、圆×圆……）
//   Tangent      由已知另一端点向圆(弧)作切线的切点
// 所有计算纯几何、无 UI 依赖，可被视图、命令行与无界面自测共用。
namespace SketchCAD {
namespace snap {

enum class SnapType { None, Endpoint, Center, Intersection, Tangent, Midpoint };

struct SnapResult {
    Vec3 point = Vec3(0, 0, 0);
    bool snapped = false;
    SnapType type = SnapType::None;
    int elemA = -1;   // 主图元（切点/圆心/端点所属图元，或交点的图元A）
    int elemB = -1;   // 次图元（交点图元B）
};

// 捕捉开关配置：绘制命令在「命令执行期间」据此决定启用哪些捕捉类型。
// 各绘制命令可持有独立的一份（由 GraphicsView 在进出命令时备份/恢复），
// 从而支持「E/T/M/C 一键切换、多模式共存、退出命令自动复原」。
struct SnapConfig {
    bool endpoint     = true;   // 端点 / 顶点
    bool center       = true;   // 圆心
    bool intersection = true;   // 两图元交点
    bool tangent      = true;   // 切点（需已知另一端点）
    bool midpoint     = false;  // 直线段中点（默认关闭，按 M 开启）
};

// 图元上「真实几何意义」的捕捉点（不含圆的半径手柄这类辅助点）。
// 圆：仅圆心；弧：圆心 + 起/终点；直线：两端点；矩形：四角；多边形/样条：全部顶点。
std::vector<Vec3> snapHandles(const Element& e);

// 由外部点 P 向圆 (C, r) 作切线，返回切点个数 0/1/2。
// P 在圆内（d<r）无实切线，返回 0；P 在圆上返回 1（切点即 P 自身投影）。
// 切点满足：|T-C|=r 且 PT ⟂ CT。
int tangentPointsFromPointToCircle(const Vec3& P, const Vec3& C, double r, Vec3 out[2]);

// 综合捕捉：在 tol 范围内寻找离 w 最近的吸附点。
// otherEnd 非空时，额外计算「由 otherEnd 向各圆/弧作切线的切点」参与竞争（用于画直线第二点切圆）。
// cfg 决定哪些捕捉类型参与竞争（按 E/T/M/C 切换的开关）。
// 返回最近命中；若 tol 内无任何吸附点，snapped=false 且 point==w（即自由点）。
SnapResult snap(const std::vector<Element>& elems, const Vec3& w,
               const Vec3* otherEnd, double tol, const SnapConfig& cfg);

// 兼容旧调用：等价于所有捕捉类型全开（中点仍默认关闭，与历史行为一致）
inline SnapResult snap(const std::vector<Element>& elems, const Vec3& w,
                      const Vec3* otherEnd, double tol) {
    return snap(elems, w, otherEnd, tol, SnapConfig{});
}

} // namespace snap
} // namespace SketchCAD
