#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"   // ProjectionMethod

namespace SketchCAD {

// 投影法识别符号图元的 name 标签前缀（单一事实源）：
// buildProjectionSymbol 生成的每个图元都带此前缀，供打印/导出层
// 识别「辅助说明元素」并做零输出过滤（与 "tb:field:" 域标签同风格）。
inline constexpr const char* kProjectionSymbolTagPrefix = "投影符号:";

// GB/T 14665-2012 第 7 章 / 表5：投影法识别图形符号。
// 给定圆心、半径与投影法，返回一组识别符号图元（layer="图框"），
// 调用方追加到图框即可。符号为「圆台投影示意」：
//   * 第一角画法：圆台尖端朝下（前视图在下、俯视图在上）
//   * 第三角画法：圆台尖端朝上（前视图在上、俯视图在下）
// 同时写入方法标注文本，便于人工与自动核对。
std::vector<Element> buildProjectionSymbol(const Vec3& center, double r,
                                           ProjectionMethod method);

// 判断图元是否为投影法识别符号（辅助说明元素）。
// 打印/预览/PDF/物理打印/COM 批量出图均经此谓词过滤，屏幕显示不受影响。
bool isProjectionSymbolElement(const Element& e);

} // namespace SketchCAD
