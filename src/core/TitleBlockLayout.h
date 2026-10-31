#pragma once
#include <QRectF>
#include "Vec.h"

namespace SketchCAD {

class Element;
class SketchModel;

// ============================================================================
// 标题栏单元格文字自适应（生成期 / 编辑期 / 自测 三处共用的单一入口）
// ----------------------------------------------------------------------------
// 背景（缺陷修复 2026-10）：标题栏文字原先按「锚点 = 块左下角、块自锚点向右上
// 延伸」的假设放置（水平 left+inset 起、垂直贴顶/贴底），但 plain Text 的真实
// 渲染锚点语义（GraphicsView::paintText 与 PlotRenderer 同源，离屏渲染实测）
// 是 9 点对齐：默认 align=1 把锚点当作**水平中心** → 全部标题栏文字向左越出
// 单元格竖边线约半个文字宽（用户可见：图纸名称/单位名称/图集号等骑线）。
// 本模块因此收敛为「居中放置」这一种规范：
//
//   ① 度量：TextMetrics::lineWidthMm（屏幕/打印/取景三处同源的单一数据源）
//   ② 收缩：超宽按比例降字高，下限 kFitMinHeightMm（绝不截断/省略——
//            elideToWidthMm 的省略号会破坏「完整显示」要求，故不使用）
//   ③ 定位：锚点 = 单元格中心（水平+垂直均居中）。创建端必须同步给文本图元
//            设 textAlign=4（9 点对齐之「中心」），保证屏幕/打印两端渲染
//            锚点语义一致；此后无论字段填什么值，文字始终保持在格内居中，
//            编辑期 refit（batchSign / fitTitleBlockText）只需收缩字高。
// ----------------------------------------------------------------------------
// 两区分离（缺陷修复 2026-10-31：填写值覆盖标签）：
//   标签（灰）与字段（黑）本是两个独立图元，但旧生成把两者锚点都放在同一
//   单元格中心 → 未填时只见标签，填写后黑色值文字与灰色标签同点重叠（用户
//   可见：填标题栏后「图纸名称/图号/材料」等标签被值盖住）。结构性修复：
//
//     标签 fitCellLabelText  → 单元格顶部「标签带」中心（高 kTitleBlockLabelStripMm）
//     字段 fitCellFieldValue → 去掉标签带后的「值区」中心
//
//   两区由同一 strip 推导、严格不重叠；水平方向仍全宽居中（锚点 X = 格中心）。
//   写回路径（TitleBlockForm → fitTitleBlockText）与批量签名（batchSign）对
//   tb:field:* 图元一律落值区，与生成端同一契约；旧档标签/普通文本保持整格居中。
// ============================================================================

// 单元格内文字自适应放置结果（世界 mm）
struct CellTextFit {
    double x      = 0.0;    // Text 锚点 X（textAlign=4：块水平中心）
    double y      = 0.0;    // Text 锚点 Y（textAlign=4：块垂直中心）
    double height = 3.5;    // 适配后字高(mm)
    bool   shrunk = false;  // 是否发生收缩（字高被降低）
};

// 在单元格 [left,right] × [bottom,top]（Y 向上，单位 mm）内放置文字。
// 返回锚点 = 单元格中心（配合 textAlign=4 渲染即格内居中）。
//   stickTop：历史参数（旧「贴顶/贴底」语义），现不影响定位，保留仅为源兼容；
//   inset   ：收缩可用宽度的左右预留（不参与定位）。
// 空文本不收缩（占位字段初始为空，由用户后续填写）。
CellTextFit fitCellText(const QString& text, double wantHeight,
                        double left, double right, double bottom, double top,
                        bool stickTop, double inset = 2.0);

// 标签带高度：单元格顶部预留给灰色标签的窄带（标签字高 3.0mm + 上下余量）。
// 对高度不足 8mm 的退化单元格自适应减半（strip ≤ 半格），两区永不产生负高度。
inline constexpr double kTitleBlockLabelStripMm = 4.0;

// 标签带放置（灰色标签）：锚点 = 单元格顶部标签带中心（配合 textAlign=4）。
// 与 fitCellFieldValue 成对使用 → 同单元格内标签与填写值垂直分离、零重叠。
CellTextFit fitCellLabelText(const QString& text, double wantHeight,
                             double left, double right, double bottom, double top,
                             double inset = 2.0);

// 值区放置（tb:field:* 填写值）：锚点 = 去掉顶部标签带后值区的中心（配合 textAlign=4）。
// 超宽收缩在值区内进行（值区宽度 = 全宽，收缩行为与旧整格一致）。
CellTextFit fitCellFieldValue(const QString& text, double wantHeight,
                              double left, double right, double bottom, double top,
                              double inset = 2.0);

// 由「标题栏」图层网格线重建包含世界点 (x,y) 的单元格矩形。
// 竖线须横跨该点 Y、横线须横跨该点 X 方为候选边界（图号|比例等部分高度
// 竖线不会误伤底带单元格）；任一侧无界 → 返回 invalid QRectF（调用方应
// 容忍：网格不完整时放弃适配，绝不猜）。
// 供编辑期（「填写标题栏」写回后按真实单元格收缩）与自测共用。
QRectF titleBlockCellAt(const SketchModel* model, double x, double y);

// 按所在单元格适配标题栏文本图元（两区放置 + 超宽收缩）。
// tb:field:* 字段图元 → 锚点复位到「值区」中心（去顶部标签带）并按需降字高，
// 与灰色标签两区分离永不重叠（填写值覆盖标签缺陷的写回端收敛点）；
// 其余文本（旧档标签/普通文本）保持整格居中。不改 textAlign——
// 新生成图元由 sheet_frame 创建端设为 4（中心），旧档图元如仍为默认 1，
// 渲染为「中心/贴底」差异极小且 refit 后仍在格内。
// cell 无效 → 不改动返回 false。返回字高是否被收缩。
bool fitTitleBlockText(Element& e, const QRectF& cell);

} // namespace SketchCAD
