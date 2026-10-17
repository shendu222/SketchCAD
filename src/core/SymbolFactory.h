#pragma once
#include <vector>
#include <QString>
#include <QVariantMap>
#include "Element.h"

// ============================================================================
// 符号标注扩展生成器（参考 CAD 机械 Ch8：基准 / 焊接 / 锥斜度 / 中心孔 / 标高 / 旋转）
// ----------------------------------------------------------------------------
// 设计要点（对照《功能扩充方案_参考CAD对照.md》P3 提示词）：
//   * 统一入口 SymbolFactory::mkSymbol(name, anchor, params) -> vector<Element>，
//     与 SectionCutSymbol / SurfaceRoughness 同源「纯函数生成器」范式；调用方负责 append 进模型。
//   * 产物为 Line / Text / Circle 组合，复用既有 ElementType，不新增枚举（满足 FORBID：不改枚举）。
//   * 各符号的尺寸均为 GB 简化示意定值（由本模块集中提供，AI 不擅自改写国标数值语义）。
//   * 未知 name → 返回空 vector（退化）；其余按 params 取值，缺省走合理默认值。
// ============================================================================

namespace SketchCAD {

namespace SymbolFactory {

// name 分支：
//   "datum"      基准符号        params["letter"] 基准字母(默认 "A")
//   "weld"       焊接符号        params["type"]: fillet(角焊,默认) / v(V形) / square(方框)
//   "taper"      锥度/斜度符号    params["ratio"] 文本(默认 "1:50")
//   "centerhole" 中心孔符号      params["r"] 孔径(mm,默认 2.5)；params["label"] 旁注(可空)
//   "elevation"  标高符号        params["value"] 标高值(默认 "±0.000")
//   "rotation"   旋转符号        params["angle"] 角度文本(默认 "360°")
//   params["rot"] 整体旋转角(度, 绕 anchor，可选)
std::vector<Element> mkSymbol(const QString& name, const Vec3& anchor, const QVariantMap& params);

} // namespace SymbolFactory
} // namespace SketchCAD
