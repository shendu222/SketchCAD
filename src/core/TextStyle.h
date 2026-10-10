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
// 求值结果：ok=true 时 text 为求值文本；ok=false 时 text 恒为 "###"（占位回落）。
struct FieldEvalResult {
    QString text;
    bool    ok = false;
};

// 支持 %<CODE>% 与 {field:code} 两种形式占位（语义等价，单一入口统一解析），
// 渲染/导出时替换为当前值；纯文本无占位则原样返回（向后兼容）。
//   DATE        当前日期 (yyyy-MM-dd)
//   TIME        当前时间 (HH:mm:ss)
//   FILENAME    当前图纸文件名（无则「未命名」）
//   FILEPATH    当前图纸完整路径
//   COUNT       图元总数（全模型计数）
//   AREA:id     引用图元的封闭面积 mm²（复用 Inquiry 层 areaOfElement；非封闭/断链 → ###）
//   PERIM:id    引用图元的封闭周长 mm（同上）
//   PROP:name   自定义图纸特性（SketchModel 特性表；缺名 → ###）
// 任一域求值异常/断链/非法 → 该占位回落占位文本 "###"（崩溃安全，不扩散、不抛出）。
QString resolveFields(const QString& raw, const SketchModel* model);

// ---- 单域求值（fieldupdate 命令/自测/域下拉提示复用）----
// code 为占位内部码（如 "AREA:5"、"PERIM:5"、"PROP:图纸名称"、"COUNT"）。
// 返回求值文本与 ok 标志：!ok 时 text 恒为 "###"（与 resolveFields 回落一致）。
FieldEvalResult evalFieldCode(const QString& code, const SketchModel* model);

// ---- Table 公式单元格求值（明细栏数量列自动维护）----
// 单元格文本以 '=' 开头视为公式（仅支持列内行号引用，1 基行号，含端点）：
//   =sum(a,b)     第 a..b 行【本列】数值求和（非数字单元格按 0 计）
//   =count(a,b)   第 a..b 行【本列】非空单元格计数
// 公式引用到的单元格若本身也是公式 → ok=false（防递归，简单安全）。
// 非公式单元格原样返回（ok=true, text=原文，不做域替换——域替换仍由 resolveFields
// 在渲染期统一处理）。行号越界/参数非法 → ok=false。
// 典型用法：明细栏「数量」列末行填 =count(2,9) 或重量列末行填 =sum(2,9)，
// 上游行改动后渲染/导出/fieldupdate 自动重算。
FieldEvalResult evalTableCellFormula(const Element& tbl, int row, int col,
                                     const SketchModel* model);

// ---- Table 单元格显示统一管线（渲染/打印/导出三路同源）----
// 公式求值（=sum/=count）→ 域替换（%<...>% / {field:...}）→ 失败回落 "###"。
// GraphicsView / PlotRenderer / CadFormats 的单元格显示一律走本函数，
// 不得各自手工拼装（同一语义多处手工拼装⇒一处更新一处漏）。
QString displayTableCell(const Element& tbl, int row, int col, const SketchModel* model);

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
