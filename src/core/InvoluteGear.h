#pragma once
// P4（Ch13 机械零件设计）：渐开线直齿圆柱齿轮齿廓采样生成器 + 零件库抽象
// ------------------------------------------------------------
// 设计要点：
//   * InvoluteGear::profile 仅做几何采样（返回 Polyline 点集），不负责标注/图层；
//     齿数/模数/压力角为参数，符合提示词 IO_SPEC。
//   * 渐开线方程采用「人工给定(G1)」标准公式 involute(rb, t)，禁止自造其它表达式
//     （见提示词 FORBID）。本文件即该公式的唯一权威实现点。
//   * PartLibrary 把 FlangeViews/AircraftViews 的「参数 → Elements」模式收敛为统一接口，
//     InvoluteGear 作为首个具体零件生成器接入（P5 会在此框架下扩展 GB 标准件出库）。
#include <vector>
#include "Vec.h"

namespace SketchCAD {

// 渐开线齿轮齿廓采样生成器
struct InvoluteGear {
    // 采样齿轮外轮廓点集（世界坐标，z=0）。
    //   teeth < 2 或 module <= 0 → 返回 false（退化，outPoints 清空）。
    //   pressureAngleDeg 默认 20（标准压力角）；addendumFactor/dedendumFactor 默认 1.0/1.25。
    //   fullGear=true 输出整齿轮闭合外轮廓；false 仅输出单个轮齿齿廓（开放）。
    // 几何关系（标准渐开线齿轮）：
    //   分度圆 r = m*z/2；基圆 rb = r*cos(α)；齿顶圆 ra = r + ha*m；齿根圆 rf = r - hf*m。
    static bool profile(int teeth, double module, double pressureAngleDeg,
                        std::vector<Vec3>& outPoints,
                        bool fullGear = true,
                        double addendumFactor = 1.0,
                        double dedendumFactor = 1.25);

    // ---- 几何辅助（供调用方与自测断言）----
    static double pitchRadius(int teeth, double module)            { return module * teeth / 2.0; }
    static double baseRadius(int teeth, double module, double pressureAngleDeg);
    static double addendumRadius(int teeth, double module, double addendumFactor = 1.0);
    static double dedendumRadius(int teeth, double module, double dedendumFactor = 1.25);

    // 人工给定(G1)的渐开线参数方程：基圆半径 rb、展开角 t（弧度）→ 点 (x, y)。
    //   x = rb*(cos t + t*sin t)
    //   y = rb*(sin t - t*cos t)
    // 该点所在半径 = rb*sqrt(1 + t^2) = rb/cos(φ)（φ 为该半径处压力角），inv(φ)=tanφ-φ=t。
    static void involute(double rb, double t, double& x, double& y);
};

} // namespace SketchCAD
