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

// 对称「修剪 / 延伸」两条开放直线段：
//   计算两条线段所在无限直线的交点 P，对每条线只移动其「离用户点选端最远的那个端点」到 P，
//   保留端（离 pick 最近的端点）保持不变。
//   * P 落在某条线段之内  → 该线被「修剪」，多余部分（交点之外的远端）被截掉；
//   * P 落在某条线段之外  → 该线被「延伸」，远端延长至交点，两条线恰好交于一点。
//   两种情形由几何自动判定，无需用户区分修剪/延伸。
// 入参：
//   a, pa  第 1 条直线及其上用户点选位置（pa 决定该线要保留的端点）
//   b, pb  第 2 条直线及其上用户点选位置
// 出参：
//   oa, aChanged  第 1 条的新几何；aChanged=false 表示该线无需改动（如保留端已贴交点）
//   ob, bChanged  第 2 条的新几何
//   msg           失败原因（平行 / 共线 / 非直线 / 几何不完整）
// 仅支持 ElementType::Line；平行或共线返回 false 且不做任何修改；浮点判定带容差。
bool extendTrimPair(const Element& a, const Vec3& pa,
                    const Element& b, const Vec3& pb,
                    Element& oa, bool& aChanged,
                    Element& ob, bool& bChanged,
                    QString& msg);

} // namespace trim
} // namespace SketchCAD
