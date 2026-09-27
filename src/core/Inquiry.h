#pragma once
// ============================================================================
// Inquiry —— 查询测量核心层（DIST / AREA / MASSPROP / ID 的纯几何实现）
// ----------------------------------------------------------------------------
// 职责：为命令行测量命令提供与渲染同源、解析精确的查询结果；
//       不依赖 UI、不修改模型（只读）。命令层（Commands.cpp）负责参数解析、
//       日志格式与选区分派，本层只回答"是多少"。
//
// 精度约定：
//   * 圆/椭圆 —— 解析精确（πr²、Ramanujan 周长近似 < 5e-5 相对误差）；
//   * 多边形类（矩形/多边形/无弧段闭合折线/剖面区域）—— shoelace 精确；
//   * 含弧段闭合折线 —— 面积/周长用弧段精确公式（弓形修正），惯性矩用
//     16 段/弧的离散环（与渲染采样一致，误差 ~0.1%，文档注明近似）；
//   * 闭合样条 —— 与渲染同源的 Catmull-Rom 采样离散。
// ============================================================================
#include <QString>
#include <QRectF>
#include <vector>

#include "Vec.h"
#include "Element.h"

namespace SketchCAD {
class SketchModel;

namespace inquiry {

// ---- 两点测量（DIST 命令）--------------------------------------------------
struct DistResult {
    double distance = 0;    ///< 两点间距 (mm)
    double dx = 0, dy = 0;  ///< 增量 ΔX / ΔY (mm)
    double angleDeg = 0;    ///< XY 平面内方向角，(-180, 180]
};
DistResult distTwoPoints(const Vec3& a, const Vec3& b);

// ---- 封闭区域面积与周长（AREA 命令）----------------------------------------
struct AreaResult {
    bool ok = false;
    double area = 0;        ///< 面积 (mm²，恒为正；带孔区域已扣除内孔)
    double perimeter = 0;   ///< 周长 (mm；带孔区域含内孔周长)
    Vec3 centroid;          ///< 形心（ok 时有效；带孔区域为净形心）
    QString error;          ///< !ok 时的原因（中文，可直接入日志）
};
/// 点列模式：任意点列按闭合多边形计算（shoelace，<3 点返回 error）
AreaResult areaOfPolygon(const std::vector<Vec3>& ring);
/// 图元分派：仅封闭图元（圆/椭圆/矩形/多边形/闭合折线/闭合样条/剖面区域）；
/// 开放图元（直线/圆弧/开放折线等）返回 ok=false + error。
AreaResult areaOfElement(const Element& e);

/// ---- 直线链围合区域（首尾相接的若干 Line → 多边形面积）----
/// 把一组 Line 图元按端点容差 tol 串成有序顶点环（不重复首点）。
/// 全部线段被用且末端回到起点（闭合）返回 true；缺段/分叉/不闭合返回 false。
bool buildRingFromLines(const std::vector<Element>& lines,
                        std::vector<Vec3>& ring, double tol = 1e-6);
/// 便捷入口：segs 能围成封闭环 → 返回该多边形的面积/周长/形心；
/// 否则 ok=false + 中文 error（可直接入日志/结果区）。
AreaResult areaOfClosedLineChain(const std::vector<Element>& segs);

// ---- 质量特性（MASSPROP 命令）----------------------------------------------
struct MassPropResult {
    bool ok = false;
    double area = 0;        ///< 面积 (mm²)
    double perimeter = 0;   ///< 周长 (mm，含内孔)
    Vec3 centroid;          ///< 形心 (mm)
    QRectF bbox;            ///< 包围盒 (世界坐标)
    double ix = 0, iy = 0;  ///< 绕世界 X/Y 轴惯性矩 (mm⁴)
    double ixy = 0;         ///< 对世界 XY 轴惯性积 (mm⁴)
    double ixBar = 0;       ///< 绕形心 X 轴惯性矩 (mm⁴)
    double iyBar = 0;       ///< 绕形心 Y 轴惯性矩 (mm⁴)
    double ixyBar = 0;      ///< 对形心轴惯性积 (mm⁴)
    QString error;          ///< !ok 时的原因
};
/// 图元分派：仅封闭图元（同 areaOfElement 的类型集）。
MassPropResult massPropOfElement(const Element& e);

// ---- 点坐标查询（ID 命令）--------------------------------------------------
/// 点查询结果：坐标本身 + 模型内最近图元（增强交互：ID 时顺带报最近对象）。
struct IdResult {
    Vec3 point;                 ///< 查询点（原样返回，供格式化输出）
    const Element* nearest = nullptr;   ///< 最近图元（es 为空时 nullptr）
    double nearestDist = 0;     ///< 到最近图元的距离 (mm)
};
IdResult idPoint(const Vec3& p, const std::vector<Element>& elements);

// ---- 供 list 命令与测试复用的底层环几何 -------------------------------------
/// 多边形环有向面积（CCW 正；环不要求首尾重复）
double polygonArea(const std::vector<Vec3>& ring);
/// 多边形环周长（自动闭合收尾段）
double polygonPerimeter(const std::vector<Vec3>& ring);
/// 多边形环形心（shoelace 精确；退化时返回顶点均值）
Vec3 polygonCentroid(const std::vector<Vec3>& ring);
/// 多边形环绕原点惯性矩/惯性积（环先归一为 CCW，结果为正）
void polygonInertia(const std::vector<Vec3>& ring,
                    double& ix, double& iy, double& ixy);
/// 封闭图元 → 渲染同源离散环（圆/椭圆 64 段；弧段 16 段/弧；样条 Catmull-Rom）。
/// 不支持的类型返回空环。Hatch 只返回外环（内孔由调用方处理）。
std::vector<Vec3> discretizeRing(const Element& e);

} // namespace inquiry
} // namespace SketchCAD
