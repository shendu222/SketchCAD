#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Block.h"   // DxfImportStats 携带块表（仅 BlockDef 纯数据，无循环依赖）

// 图纸文件格式互操作层（纯函数，不依赖任何 UI / 模型对象）。
//
// 设计原则：
//   1) 只接收 std::vector<Element>，返回文件字节，便于在核心层自测直接覆盖；
//   2) 与 SketchModel 解耦，MainWindow 只负责"选文件 → 调这里 → 写盘"；
//   3) 每种格式一个独立函数，扩展名分派由 formatFromSuffix 唯一决定。
//
// 支持的格式：
//   Json — 本程序原生草图格式（完整往返，含样式/标注/文字）；UTF-8
//   Dxf  — AutoCAD DXF **R12 (AC1009)**。默认把图元炸开为 LINE/CIRCLE/ARC/POLYLINE/
//          TEXT/SOLID 基元（最大兼容）；开启 DxfExportOptions 后块/标注/填充按原生
//          实体导出（INSERT/DIMENSION/HATCH），在 AutoCAD/ZWCAD 中可编辑。
//   Step — ISO 10303-21（AP214 二维曲线集：LINE/CIRCLE/TRIMMED_CURVE/POLYLINE/ELLIPSE）；ASCII
//   Dwg  — 原生 DWG R12 (AC1009) 二进制格式。本项目不引入 ODA/LibreDWG 等第三方
//          库，手写实现最广泛兼容的 R12 子集：实体炸开为 LINE/CIRCLE/ARC/TEXT/
//          SOLID/POLYLINE+VERTEX（与 DXF 导出同一套基元展开），封装进原生 R12
//          二进制容器（文件头 + 图层表 + 实体记录，每记录附 CRC16），可被
//          AutoCAD / LibreCAD / QCAD / BricsCAD 等读取。详见 CadFormats.cpp。
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

// DXF 原生导出开关集。**默认值 = 旧版"炸开/离散"行为**：不传 opts（nullptr）或
// 保持默认 false 时，toDxf 的输出与升级前逐字节一致（旧调用方零影响）。
// 传 opts 且打开开关后，块/标注/填充以原生实体写进 DXF，AutoCAD/ZWCAD 打开后：
//   * BlockRef → INSERT+ATTRIB（块不展开、属性块可编辑，块定义在 BLOCKS 段）
//   * Dimension → DIMENSION（9 类原生映射；Arc/Leader/Ordinate 3 类仍炸开），每个原生
//     标注配一个 *Dn 匿名块承载显示几何，回读时由组码直接重建
//   * Hatch → HATCH（多段线边界路径 + 图案线；R12 规范无路径组码，此为业界事实
//     标准写法——ZWCAD/浩辰/GstarCAD 导出 R12 亦如此，AutoCAD R14+ 读取容错）
//   * Point 补写 POINT（R12 原生实体，此前被静默丢弃）
// Ellipse/Spline 在 R12 无原生实体（ELLIPSE/SPLINE 是 R13+），保留折线/采样离散，
// 精度可经 splineSegments/ellipseSegments 配置（默认与旧版一致：16/96）。
// 注意：LTYPE 表升级为 9 种线型、新增 STYLE/DIMSTYLE 表与实体线型组码 6 为**无条件
// 增强**（对旧调用方也是保真提升，文件结构不变）。
struct DxfExportOptions {
    bool nativeBlocks = false;   // ① 块原生：BLOCKS 段 + INSERT/ATTRIB/ATTDEF
    bool nativeDims   = false;   // ② 标注原生：DIMENSION + *D 匿名块（9 类）
    bool nativeHatch  = false;   // ③ 填充原生：HATCH 边界路径
    int  splineSegments  = 16;   // ④ 样条离散段数/段（POLYLINE 拟合精度）
    int  ellipseSegments = 96;   //    椭圆采样段数（闭合折线）
};

// ---- 序列化（失败返回空并把原因写入 *err）----
// drawingScale = 绘图比例因子（图形:实物）。图元的 pts 是「图纸坐标」，而标注数字
// 必须等于**实物尺寸**（GB/T 14690），故导出时要把比例因子传进来，否则 1:2 图纸上
// 100mm 的尺寸会被导出成 50。默认 1.0（1:1 图纸无需传）。
// insunits = DXF $INSUNITS 标准码（AutoCAD 约定，见 Units.h 的 insunitsCode）：
// 写进 HEADER 供导入方识别度量口径，实现跨单位交换（本程序内部几何恒为 mm）。
// blocks = 块定义表（nativeBlocks 开启时必需；nullptr = 旧行为，块引用静默跳过）。
// opts = 原生导出开关（nullptr = 旧版炸开行为，见 DxfExportOptions 注释）。
QByteArray toDxf (const std::vector<Element>& elems, QString* err = nullptr,
                  double drawingScale = 1.0, int insunits = 4,
                  const std::vector<BlockDef>* blocks = nullptr,
                  const DxfExportOptions* opts = nullptr);
QByteArray toDwg (const std::vector<Element>& elems, QString* err = nullptr,
                  double drawingScale = 1.0);
// 说明：toDwg 输出**原生 R12 (AC1009) 二进制**格式（手写实现，零第三方依赖）：
//   把图元炸开为 LINE / CIRCLE / ARC / TEXT / SOLID / POLYLINE+VERTEX 基本体，
//   封装进原生 R12 二进制容器（含图层表、实体记录、每记录 CRC16），
//   可被 AutoCAD / LibreCAD / QCAD / BricsCAD 作为 .dwg 直接打开。

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
    int entities    = 0;   // 成功导入的图元数（顶层 ENTITIES，不含块内图元）
    int arcsKept    = 0;   // 由 bulge 成功还原的弧段数
    int arcsDropped = 0;   // bulge 无法用「三相邻点定圆」表达、已退回直线段的段数
    int insunits    = 4;   // HEADER $INSUNITS 单位码（默认 4=mm；0=无单位/缺头也按 mm）
    int unknownEntities = 0;   // 无法识别/不支持的实体类型计数（跳过，不中断整档）
    int fallbackEntities = 0;  // 复杂实体降级为基元（LINE+TEXT/边界线）的计数
    std::vector<BlockDef> blocks;   // 从 BLOCKS 段解析出的块定义（供调用方 addBlockDef）
};                         //   调用方按 Units.h::lengthUnitFromInsunits 换算到 mm
bool fromDxf(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
             DxfImportStats* stats = nullptr);

// DWG 回读：支持 R12 (AC1009) 原生二进制。成功返回 true（out 追加解析到的图元）。
// 解析图层表得到图层名，实体记录的图层索引映射回图层名；颜色由 ACI 还原为 RGB。
// 文本按 UTF-8 解码（R12 无真彩色组码，颜色经 62→ACI→RGB 近似）。
bool fromDwg(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
             DxfImportStats* stats = nullptr);

} // namespace cadfmt
} // namespace SketchCAD
