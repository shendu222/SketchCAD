#pragma once
#include <QString>
#include <QStringList>
#include <QFont>
#include "Element.h"

namespace SketchCAD {

class SketchModel;   // 前向声明，避免与 SketchModel.h 形成包含环

// ============================================================
// 文字样式（STYLE）—— 统一治理字体/字高/宽度因子等属性
// ============================================================
// 与图层/块表同级，由 SketchModel 持有 vector<TextStyle> 作为唯一数据源。
// 图元经 Element::styleName 引用样式名；渲染/导出时按本结构解析出最终字体参数，
// 因此「修改样式 → 所有引用它的文字同步更新」天然成立，无需逐图元改属性。
struct TextStyle {
    QString name;             // 样式名（唯一键，空名非法）
    QString fontFamily;       // 字体族（空 = 使用系统/场景默认字体）
    double  textHeight = 0.0; // 字高(mm)：0 = 用图元自身 radius 字段
    double  widthFactor = 1.0; // 宽度因子（与图元 widthFactor 相乘生效）
    double  obliquingAngle = 0.0; // 文字倾斜角度(度，正值右斜)
    bool    bold = false;
    bool    italic = false;
};

// ---- 字段（FIELD）占位解析 ----
// 支持 %<CODE>% 形式占位，渲染/导出时替换为当前值；纯文本无占位则原样返回（向后兼容）。
//   DATE      当前日期 (yyyy-MM-dd)
//   TIME      当前时间 (HH:mm:ss)
//   FILENAME  当前图纸文件名（无则「未命名」）
//   FILEPATH  当前图纸完整路径
//   COUNT     图元总数
QString resolveFields(const QString& raw, const SketchModel* model);

// ---- 样式解析：把图元的 styleName 与图元自身字段合并成最终生效字体参数 ----
struct ResolvedTextStyle {
    QString fontFamily;       // 空 = 走默认字体
    double  height = 3.5;     // 字高(mm)，绝不为 0（回落国标 3.5）
    double  widthFactor = 1.0;// 已含 样式×图元 两级乘积
    double  obliquingAngle = 0.0;
    bool    bold = false;
    bool    italic = false;
};
ResolvedTextStyle resolveTextStyle(const Element& e, const SketchModel* model);

// ---- 按最大宽度(mm) 折行（用于 MText 自动换行）----
// 先按显式 '\n' 分段，每段再按字符贪心折行（兼容中文无空格场景）；maxWidthMm<=0 不折行。
QStringList wrapText(const QString& text, double maxWidthMm, const QFont& font);

} // namespace SketchCAD
