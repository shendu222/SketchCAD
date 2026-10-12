#pragma once
#include <QRectF>
#include "Vec.h"

namespace SketchCAD {

class Element;
class SketchModel;

// ============================================================================
// 标题栏单元格文字自适应（生成期 / 编辑期 / 自测 三处共用的单一入口）
// ----------------------------------------------------------------------------
// 背景（缺陷修复）：标题栏标签原先按「锚点 = 单元格顶边 - 2mm」放置，而
// plain Text 的锚点语义是「块左下角、块自锚点向右上延伸」（屏幕 GraphicsView
// 与打印 PlotRenderer 同一套 align=1 代码，语义一致）——标签因此向上越界
// 约 1mm；同时用户填写/预填的长值（图纸名称等）按固定字高渲染会横向冲出
// 单元格右边界。本模块把「放进单元格」收敛为唯一实现：
//
//   ① 度量：TextMetrics::lineWidthMm（屏幕/打印/取景三处同源的单一数据源）
//   ② 收缩：超宽按比例降字高，下限 kFitMinHeightMm（绝不截断/省略——
//            elideToWidthMm 的省略号会破坏「完整显示」要求，故不使用）
//   ③ 定位：水平左对齐（left + inset 起）；垂直贴顶（标签）或贴底（字段值），
//            各留 kCellBreathMm 呼吸边
// ============================================================================

// 单元格内文字自适应放置结果（世界 mm）
struct CellTextFit {
    double x      = 0.0;    // Text 锚点 X（plain Text：块左边缘）
    double y      = 0.0;    // Text 锚点 Y（plain Text：块底边缘）
    double height = 3.5;    // 适配后字高(mm)
    bool   shrunk = false;  // 是否发生收缩（字高被降低）
};

// 在单元格 [left,right] × [bottom,top]（Y 向上，单位 mm）内放置文字。
//   stickTop=true  → 标签（贴单元格顶边下方 kCellBreathMm）
//   stickTop=false → 字段值（贴单元格底边上方 kCellBreathMm）
// 空文本不收缩（占位字段初始为空，由用户后续填写）。
CellTextFit fitCellText(const QString& text, double wantHeight,
                        double left, double right, double bottom, double top,
                        bool stickTop, double inset = 2.0);

// 由「标题栏」图层网格线重建包含世界点 (x,y) 的单元格矩形。
// 竖线须横跨该点 Y、横线须横跨该点 X 方为候选边界（图号|比例等部分高度
// 竖线不会误伤底带单元格）；任一侧无界 → 返回 invalid QRectF（调用方应
// 容忍：网格不完整时放弃适配，绝不猜）。
// 供编辑期（「填写标题栏」写回后按真实单元格收缩）与自测共用。
QRectF titleBlockCellAt(const SketchModel* model, double x, double y);

// 按所在单元格适配标题栏文本图元（标签贴顶 / 字段值贴底）。
// 判别：name 以 "tb:label:" 开头 → 标签（贴顶）；其余（"tb:field:" 等）→
// 字段值（贴底）。同时把锚点复位到规范位置（修复旧档错位）并按需降字高。
// cell 无效 → 不改动返回 false。返回字高是否被收缩。
bool fitTitleBlockText(Element& e, const QRectF& cell);

} // namespace SketchCAD
