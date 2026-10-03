#include "LinePattern.h"
#include <cmath>

namespace SketchCAD {

DashSpec dashSpecFor(LineStyle ls) {
    DashSpec s;
    switch (ls) {
        // 4.2：虚线 画12d 空3d；最小长度 27d
        case LineStyle::Dashed:
        case LineStyle::ThickDashed:
            s = { true, { 12.0, 3.0 }, 27.0 };
            break;
        // 4.2：点画线 画12d 空3d 点1d 空3d；最小长度 54.5d
        case LineStyle::DashDot:
        case LineStyle::ThickDashDot:
            s = { true, { 12.0, 3.0, 1.0, 3.0 }, 54.5 };
            break;
        // 4.2：双点画线 画12d 空3d 点1d 空3d 点1d 空3d；最小长度 58d
        case LineStyle::DashDotDot:
            s = { true, { 12.0, 3.0, 1.0, 3.0, 1.0, 3.0 }, 58.0 };
            break;
        // 点线：1d 画 / 3d 空（细点线，最小长度 15d）
        case LineStyle::Dotted:
            s = { true, { 1.0, 3.0 }, 15.0 };
            break;
        default:
            s.valid = false;   // Solid / Wave / DoubleBreak（后两者由渲染层自定义路径绘制）
            break;
    }
    return s;
}

DashSpec dashSpecScaled(LineStyle ls, double ltscale) {
    DashSpec s = dashSpecFor(ls);
    if (!s.valid) return s;
    const double k = (ltscale > 0.0) ? ltscale : 1.0;
    if (k == 1.0) return s;                       // 比例 1.0：原样返回，零拷贝
    for (double& v : s.pattern) v *= k;
    s.minLengthD *= k;
    return s;
}

bool shouldFallbackToSolid(LineStyle ls, double lengthMm, double d) {
    const DashSpec s = dashSpecFor(ls);
    if (!s.valid) return false;
    if (d <= 0.0) return false;
    return lengthMm < s.minLengthD * d;
}

bool dashOnAt(double phaseD, const DashSpec& spec) {
    if (!spec.valid || spec.pattern.empty()) return true;
    double cycle = 0.0;
    for (double v : spec.pattern) cycle += v;
    if (cycle <= 0.0) return true;
    double p = std::fmod(phaseD, cycle);
    if (p < 0.0) p += cycle;
    bool on = true;                 // 图案首段为「画」
    double acc = 0.0;
    for (double v : spec.pattern) {
        acc += v;
        if (p < acc) return on;
        on = !on;
    }
    return false;
}

double dashPhaseOffset(double lengthMm, double d, const DashSpec& spec) {
    if (!spec.valid || spec.pattern.empty() || d <= 0.0) return 0.0;
    double cycle = 0.0;
    for (double v : spec.pattern) cycle += v;
    if (cycle <= 0.0) return 0.0;
    const double Ld = lengthMm / d;    // 线段长度（d 单位）
    // 在 [0,周期) 内搜索使两端都落在「画」上的偏移，步长 0.05d
    const double step = 0.05;
    for (double o = 0.0; o < cycle; o += step) {
        const double phEnd = std::fmod(Ld + o, cycle);
        if (dashOnAt(o, spec) && dashOnAt(phEnd, spec)) return o;
    }
    return 0.0;
}

} // namespace SketchCAD
