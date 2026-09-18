#pragma once
#include <QString>
#include <vector>
#include "Element.h"

// 图纸文件格式互操作层（纯函数，不依赖任何 UI / 模型对象）。
//
// 设计原则：
//   1) 只接收 std::vector<Element>，返回文件字节，便于在核心层自测直接覆盖；
//   2) 与 SketchModel 解耦，MainWindow 只负责"选文件 → 调这里 → 写盘"；
//   3) 每种格式一个独立函数，扩展名分派由 formatFromSuffix 唯一决定。
//
// 支持的格式：
//   Json — 本程序原生草图格式（完整往返，含样式/标注/文字）；UTF-8
//   Dxf  — AutoCAD DXF **R12 (AC1009)**（图元炸开为 LINE/CIRCLE/ARC/POLYLINE/TEXT/SOLID）
//   Step — ISO 10303-21（AP214 二维曲线集：LINE/CIRCLE/TRIMMED_CURVE/POLYLINE/ELLIPSE）；ASCII
//   Dwg  — 专有二进制格式。本程序未链接 ODA 转换库，无法写出原生二进制 DWG；
//          toDwg() 返回与 DXF 完全相同的内容（"DXF 兼容"写出），由调用方决定
//          是否真的落成 .dwg 文件名，并须向用户说明该限制。
//
// 编码约定（重要）：
//   DXF 的编码由文件内 $DWGCODEPAGE 声明，**不是** UTF-8（UTF-8 只有 R2007+ 才支持）。
//   因此 toDxf/toDwg 返回 QByteArray 而不是 QString：
//     - 内容全为 ASCII → ANSI_1252，字节即 ASCII（兼容性最好）
//     - 含中文等非 ASCII → GBK 字节 + 声明 ANSI_936（中文 AutoCAD / 看图软件的标准做法）
//   调用方**必须原样写盘**，不要再做一次 toUtf8()，否则中文会乱码。
namespace SketchCAD {
namespace cadfmt {

enum class CadFormat { Unknown, Json, Dxf, Step, Dwg };

// 扩展名（不带点，大小写不敏感）→ 格式。无法识别返回 CadFormat::Unknown。
CadFormat formatFromSuffix(const QString& suffix);
// 从完整路径取扩展名再判定（如 "a/b.STEP" → Step）
CadFormat formatFromPath(const QString& path);

// 该格式的默认扩展名（json / dxf / step / dwg）。
QString defaultSuffix(CadFormat f);
// 人类可读的格式名，用于日志（"草图 JSON" / "DXF" / "STEP" / "DWG"）。
QString formatName(CadFormat f);
// 该格式是否支持"打开/回读"。
bool canImport(CadFormat f);

// ---- 序列化（失败返回空并把原因写入 *err）----
// drawingScale = 绘图比例因子（图形:实物）。图元的 pts 是「图纸坐标」，而标注数字
// 必须等于**实物尺寸**（GB/T 14690），故导出时要把比例因子传进来，否则 1:2 图纸上
// 100mm 的尺寸会被导出成 50。默认 1.0（1:1 图纸无需传）。
QByteArray toDxf (const std::vector<Element>& elems, QString* err = nullptr,
                  double drawingScale = 1.0);
QByteArray toDwg (const std::vector<Element>& elems, QString* err = nullptr,
                  double drawingScale = 1.0);  // 内容 = DXF

// STEP 导出统计（供日志说明"哪些内容未随 STEP 导出"）
struct StepStats {
    int curves = 0;    // 写出的曲线实体数
    int skippedText = 0;
};
QString toStep(const std::vector<Element>& elems, StepStats* stats = nullptr,
               QString* err = nullptr, double drawingScale = 1.0);

// ---- 反序列化 ----
// DXF 回读：支持 LINE / CIRCLE / ARC / LWPOLYLINE / POLYLINE+VERTEX / TEXT。
// 成功返回 true（out 追加解析到的图元；文件里没有 ENTITIES 段也算失败）。
// 文本按字节自动解码：优先看文件声明的 $DWGCODEPAGE（ANSI_936=GBK），再试 UTF-8。
//
// 弧段（bulge，组码 42）的处理是本函数最微妙的一处，务必看清：
//   外部 CAD 用「两点 + 凸度 bulge」参数化弧段，而本模型用「三相邻点外接圆」
//   （见 geo::arcThrough）。两者不等价，故策略是：
//     - 若用三相邻点算出的圆弧与 bulge 声明的扫掠角一致 → 还原为弧段（本程序自己导出的
//       文件、以及任何采用同一三定点写法的文件，都走这条，往返无损）；
//     - 不一致 → **退回直线段**，宁可少画一段弧，也绝不画出方向/圆心错误的圆弧。
//   退化情况由 DxfImportStats::arcsDropped 如实报出，调用方可据此提示用户，而不是静默丢形状。
struct DxfImportStats {
    int entities    = 0;   // 成功导入的图元数
    int arcsKept    = 0;   // 由 bulge 成功还原的弧段数
    int arcsDropped = 0;   // bulge 无法用「三相邻点定圆」表达、已退回直线段的段数
};
bool fromDxf(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
             DxfImportStats* stats = nullptr);

} // namespace cadfmt
} // namespace SketchCAD
