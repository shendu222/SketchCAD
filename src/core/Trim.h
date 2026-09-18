#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Vec.h"

// 剪切（修剪）引擎：把「求交 → 参数化 → 切分 → 重建图元」这条链路收在核心层。
//
// 设计取舍：
//   * 语义对齐主流 CAD —— 只移除鼠标点中的那一「跨」，而不是把图元在每个交点处切碎；
//   * 边界既可以是场景里的其它图元（快速剪切），也可以是显式指定的若干条边界线，
//     两者的差别仅仅是传进来的 cutters 集合不同，算法完全一致；
//   * 闭合轮廓（矩形/多边形）被剪切后必然变成开放折线，因此 Element 上的 closed
//     是数据而不是类型属性；矩形失去矩形性后统一降级为多边形。
namespace SketchCAD {
namespace trim {

// 用 cutters 作为剪切边界，剪掉 target 上包含 clickPt 的那一跨。
// 返回 true 表示剪切成立：out 为剩余图元（0~2 个，已继承 target 的样式与图层）；
//   out 为空表示该图元被整体剪除，调用方应删除它。
// 返回 false 表示无法剪切，msg 给出可直接展示给用户的原因。
bool trimElement(const Element& target, const std::vector<Element>& cutters,
                 const Vec3& clickPt, std::vector<Element>& out, QString& msg);

// 目标图元与边界集合的全部交点（供交互层做「有没有边界」的前置提示）
std::vector<Vec3> gatherCuts(const Element& target, const std::vector<Element>& cutters);

} // namespace trim
} // namespace SketchCAD
