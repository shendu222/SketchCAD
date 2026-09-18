#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"   // ProjectionMethod

namespace SketchCAD {

// GB/T 14665-2012 第 7 章 / 表5：投影法识别图形符号。
// 给定圆心、半径与投影法，返回一组识别符号图元（layer="图框"），
// 调用方追加到图框即可。符号为「圆台投影示意」：
//   * 第一角画法：圆台尖端朝下（前视图在下、俯视图在上）
//   * 第三角画法：圆台尖端朝上（前视图在上、俯视图在下）
// 同时写入方法标注文本，便于人工与自动核对。
std::vector<Element> buildProjectionSymbol(const Vec3& center, double r,
                                           ProjectionMethod method);

} // namespace SketchCAD
