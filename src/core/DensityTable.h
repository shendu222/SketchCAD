#pragma once
// P7（Ch5 辅助工具）：材料密度表
// ------------------------------------------------------------
// ⚠️ 密度数值为「人工提供，禁改」（提示词 FORBID：改国标密度数值）。
//    以下为常用工程材料参考值 (g/cm³)，与现行标准/企业规范不一致时**以人工提供为准**。
//    仅用于 SectionProps 重量估算：mass(g) = area(mm²) × thickness(mm) × density(g/cm³) × 1e-3。
#include <QString>
#include <QStringList>

namespace SketchCAD {
namespace DensityTable {

// 按材料名查密度 (g/cm³)；未命中回落 0.0（调用方据此判「未知材料」）。
double density(const QString& material);
bool  has(const QString& material);
// 已知材料名列表（规范化中文键）。
QStringList materials();

} // namespace DensityTable
} // namespace SketchCAD
