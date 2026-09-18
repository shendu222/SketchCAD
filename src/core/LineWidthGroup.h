#pragma once
#include <QString>

namespace SketchCAD {

// GB/T 14665-2012 4.1 / 表1「图线组别」。
// 每种组别给出「粗线 / 细线」两级线宽（mm），粗:细恒为 2:1：
//   G1: 2.0 / 1.0
//   G2: 1.4 / 0.7
//   G3: 1.0 / 0.5   ← 默认（A2/A3/A4 常用）
//   G4: 0.7 / 0.35
//   G5: 0.5 / 0.25
enum class LineWidthGroup { G1 = 0, G2 = 1, G3 = 2, G4 = 3, G5 = 4 };

int         lineWidthGroupCount();                        // 5
double      groupThickMm(LineWidthGroup g);               // 粗线线宽(mm)
double      groupThinMm(LineWidthGroup g);                // 细线线宽(mm) = 粗/2
QString     lineWidthGroupName(LineWidthGroup g);         // 如 "第3组 (粗1.0/细0.5 mm)"

// 进程内「当前线组」：新建图纸时按幅面选定；切换后全图线宽随之同步。
LineWidthGroup currentLineWidthGroup();
void           setCurrentLineWidthGroup(LineWidthGroup g);

// 图纸 mm → 屏幕像素（cosmetic pen 用）。屏幕不做真实打印比例换算（无出图通道），
// 采用固定系数，使默认组 G3 的粗线≈3px，与旧版观感一致。
double mmToScreenPx(double mm);

} // namespace SketchCAD
