#pragma once
#include <QPageSize>
#include <QPageLayout>
#include <QRectF>
#include <QMarginsF>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "core/PlotStyle.h"

namespace SketchCAD {

// ============================================================================
// 布局 / 视口（MVP，最小可行版本）
// ----------------------------------------------------------------------------
// 真实的图纸空间(Paper Space) + 浮动视口是庞大体系；此处仅实现其最小骨架：
//   * PlotViewport 描述「模型空间的哪一块矩形区域(modelRect) 以何种比例映射到图纸」。
//   * PrintSettings（见 PrintEngine.h）充当「布局」：一张页面的页面参数 + 视口集合
//     + 样式映射。视口为空 → 模型空间打印（整体图形直接出图，无需切换布局）。
//   * 多个视口时，MVP 将每个视口独立映射到整块可打印区（等价于「每布局一个浮动视口」，
//     可叠加对比）；更复杂的视口平铺/相邻排布留待后续迭代。
// ============================================================================

// 视口：模型空间中的一块矩形区域，及其到图纸的映射比例
struct PlotViewport {
    QString name;          // 视口名称（便于识别）
    QRectF  modelRect;     // 模型空间世界坐标(mm, y-up)；空矩形 → 整体图形范围
    double  scale = 1.0;   // 视口比例：视口内 1 个模型 mm 在图纸上占 scale mm；<=0 沿用布局整体比例
    double  rotationDeg = 0.0;

    QJsonObject toJson() const;
    static PlotViewport fromJson(const QJsonObject& o);
};

// 当前设计把「布局」直接由 PrintSettings 表达（见 PrintEngine.h），此处仅提供
// 一个构造整体图形单视口的便捷工厂，供 PrintSettings::defaults() 使用。
struct PlotLayout {
    static PlotViewport fullModelViewport() {
        PlotViewport vp;
        vp.name = QStringLiteral("模型空间");
        vp.modelRect = QRectF();   // 空 → 使用整体范围
        vp.scale = 1.0;
        return vp;
    }
};

} // namespace SketchCAD
