#pragma once
#include <vector>
#include "Element.h"

namespace SketchCAD {

// GB/T 14665-2012 4.2「图线结构」：虚线/点画线/双点画线的线素长度以线宽 d 为单位。
// 图案以 d 为单位（Qt QPen::setDashPattern 亦以笔宽为单位，可直接使用）。
struct DashSpec {
    bool valid = false;              // 是否可用 dash pattern 表达（实线/波浪线/双折线为 false）
    std::vector<double> pattern;     // {画, 空, 画, 空, …}，单位 = d
    double minLengthD = 0.0;         // 4.2 最小长度（d 的倍数）；图线短于此应回落细实线
};

// 线型 → 线素规格（4.2 公式）。Solid/Wave/DoubleBreak 返回 valid=false。
DashSpec dashSpecFor(LineStyle ls);

// 全局线型比例（LTSCALE）下的线素规格：把 dashSpecFor 的图案整体缩放 k 倍
// （k<=0 视作 1.0）。渲染与打印两侧共用此单一入口，保证虚线段长比例恒等于 LTSCALE。
// pattern 与 minLengthD 同步缩放，使相位对齐（dashPhaseOffset）与短线段回落阈值一并随比例变化。
DashSpec dashSpecScaled(LineStyle ls, double ltscale);

// 4.2：给定图上长度 lengthMm 与线宽 d，该线型是否应回落为细实线。
bool shouldFallbackToSolid(LineStyle ls, double lengthMm, double d);

// 4.4：相位对齐——返回 dash 偏移（d 单位，∈[0,周期)），使线段两端都落在「画」上
// （交点/转角尽量交于画）。无法满足时返回 0。
double dashPhaseOffset(double lengthMm, double d, const DashSpec& spec);

// 相位是否落在「画」上（供测试与渲染复用）。
bool dashOnAt(double phaseD, const DashSpec& spec);

} // namespace SketchCAD
