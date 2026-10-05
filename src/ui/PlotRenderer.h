#pragma once
#include <QPainter>
#include "core/Element.h"
#include "core/SketchModel.h"
#include "core/PlotStyle.h"
#include "core/Layout.h"

namespace SketchCAD {

// ============================================================================
// 视图无关的「单图元」渲染器
// ----------------------------------------------------------------------------
// 复用 GraphicsView 的几何路径(ElementItem::buildPath) 与国标线型/剖面/标注算法，
// 但用传入的 scaleDev(设备 px / 世界 mm) 取代屏幕缩放，并应用简化 CTB 样式映射。
// 与屏幕交互渲染完全解耦，供打印引擎 / PDF / 预览图共用，保证「所见即所印」。
// ============================================================================
class PlotRenderer {
public:
    // 渲染整个模型（不含坐标轴 / 提示等覆盖层）
    //   lineWidthScale: 打印时对图元线宽(mm)的整体缩放（<1 变细，默认 0.5），用于消除过粗失真
    //   frozenLayers:   per-viewport 冻结图层名单（nullptr = 不冻结，历史行为）；
    //                   名单内图层的打印零输出（仅影响本次调用，模型空间与其他视口不受影响）
    static void renderModel(QPainter* p, SketchModel* model, double scaleDev,
                           const QRectF& clipWorld, const PlotStyleTable& style,
                           double lineWidthScale = 0.5,
                           const QStringList* frozenLayers = nullptr);

    // 渲染单个图元（对外可见，便于测试 / 扩展）
    static void renderElement(QPainter* p, const Element& e, SketchModel* model,
                             double scaleDev, const QRectF& clipWorld,
                             const PlotStyleTable& style, double lineWidthScale = 0.5);
};

} // namespace SketchCAD
