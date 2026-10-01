#pragma once
// ============================================================================
// 视图无关的图元几何路径 / 包围盒（ElementItem 静态逻辑的无头下沉版）
// ----------------------------------------------------------------------------
// 背景：ElementItem::buildPath / xlineClipToRect / rawBounds 原本定义在
// GraphicsView.cpp 内。其中 buildPath 与 geometryBounds 虽为 static、注释明确
// 「视图无关」，但物理上位于 UI 巨型编译单元，导致打印引擎链（PrintEngine/
// PlotRenderer，被 COM 自动化 DLL 无头复用）被迫拖入整套图形视图依赖。
//
// 本单元把这三段纯几何逻辑下沉为自由函数（算法一字未改），此后：
//   * ElementItem::buildPath / xlineClipToRect / rawBounds / geometryBounds
//     变为一行委托（对 GraphicsView 内部与既有调用方零破坏）；
//   * PrintEngine / PlotRenderer / COM 自动化引擎直接引用本单元，
//     不再依赖 GraphicsView.cpp。
//
// model 参数（可空）语义与原 ElementItem::model 成员一致：
//   * 非空 → 文字样式宽度因子参与文本包围盒估算；块引用按块表展开取并集；
//   * 空   → 宽度因子取 1.0；块引用退化为插入点（无块表可查）。
// ============================================================================
#include <QPainterPath>
#include <QRectF>
#include "core/Element.h"

class SketchModel;

namespace SketchCAD {
namespace ElementPath {

// 把图元转成世界坐标绘制路径（样条展开为三次贝塞尔等，见 cpp 内逐类型处理）。
// xlineClip：构造线(Xline)专用裁剪矩形；空矩形回落到场景级大矩形。
QPainterPath buildPath(const Element& e, const QRectF& xlineClip = QRectF());

// 构造线：无限直线与矩形求交，返回交线段端点（不相交返回 false）
bool xlineClipToRect(const Element& e, const QRectF& r, Vec3& a, Vec3& b);

// 图元的「几何」包围盒（世界坐标，不含 cosmetic 描边余量）——即原
// ElementItem::rawBounds。供取景/打印排版/命中估算共用。
QRectF rawBounds(const Element& e, const SketchModel* model = nullptr);

} // namespace ElementPath
} // namespace SketchCAD
