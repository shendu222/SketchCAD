#pragma once
#include <vector>
#include <QString>
#include "Element.h"

// ============================================================================
// 中心线家族生成器（参考 CAD 机械 Ch3.2 中心线 / 圆心标记 / 十字中心线 等）
// ----------------------------------------------------------------------------
// 设计要点（对照《功能扩充方案_参考CAD对照.md》P1 提示词）：
//   * 纯函数生成器：输入「被标注图元集合 refs」与参数 p，输出 Line 序列，
//     调用方负责 append 进模型——与 SurfaceRoughness::buildRoughnessSymbol 同源范式。
//   * 不复用/新增 ElementType；中心线在 GB/T 4457.4 中即「细点画线」，
//     故所有产物统一 LineStyle::DashDot（点画线），不新增线型枚举值（满足 FORBID：不改枚举）。
//   * 几何一律走 geo:: / math:: 既有原语（circleFrom3Points / intersectElements /
//     lineLineIntersect / elementCentroid / elementBounds），禁止自造几何算法。
//   * 退化（refs 不足 / 半径 0 / 平行无交点 / 重合点）一律返回空 vector + 可选原因字符串。
// ============================================================================

namespace SketchCAD {

// 10 种中心线类型
enum class CenterLineKind {
    kCenter,        // 中心线：沿轴线/直线延伸的单轴中心线
    kCross,         // 十字中心线：过圆心（或交点）的一对垂直中心线
    kCircleMark,    // 圆心标记：圆/圆弧中心的小十字标记（支持多圆批量）
    kCorner,        // 角点中心线：过两直线交点的角平分线
    kPlate,         // 对称板中心线：矩形（板）的双对称轴线
    kFullCircle,    // 整圆十字中心线：过整圆的完整十字中心线
    kAngle,         // 角度中心线：沿角两臂延伸的中心线
    kHole,          // 孔中心线：过孔中心的单轴中心线
    kThroughHole,   // 通孔中心线：过孔中心、向两侧长延伸的单轴中心线（表示贯通）
    kTwoLinesCross  // 两线交点十字线：过两已知直线交点的十字中心线
};

inline const char* centerLineKindName(CenterLineKind k) {
    switch (k) {
        case CenterLineKind::kCenter:       return "中心线";
        case CenterLineKind::kCross:        return "十字中心线";
        case CenterLineKind::kCircleMark:   return "圆心标记";
        case CenterLineKind::kCorner:       return "角点中心线";
        case CenterLineKind::kPlate:        return "对称板中心线";
        case CenterLineKind::kFullCircle:   return "整圆十字中心线";
        case CenterLineKind::kAngle:        return "角度中心线";
        case CenterLineKind::kHole:         return "孔中心线";
        case CenterLineKind::kThroughHole:  return "通孔中心线";
        case CenterLineKind::kTwoLinesCross:return "两线交点十字线";
    }
    return "未知";
}

// 中心线生成参数
struct CLParams {
    double  extend = 5.0;   // 超出特征端点的延伸长度(mm)
    double  gap    = 0.0;   // 特征边缘到中心线起点的间隙(mm)；0 表示线穿过特征
    QString layer  = "0";   // 当前图层（产物归属图层）
};

// 生成中心线图元集合。
//   kind 决定语义与 refs 解释方式（圆/直线/点/矩形/两线）；
//   失败或退化返回空 vector；reason（若非空）写入失败原因，便于上层 log/提示。
std::vector<Element> makeCenterLine(CenterLineKind kind,
                                    const std::vector<Element>& refs,
                                    const CLParams& p,
                                    QString* reason = nullptr);

} // namespace SketchCAD
