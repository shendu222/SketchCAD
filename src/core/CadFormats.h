#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Block.h"    // DxfImportStats 携带块表（仅 BlockDef 纯数据，无循环依赖）
#include "TextStyle.h" // DxfImportStats 携带 STYLE 文字样式表（纯数据，无循环依赖）
#include "Layer.h"     // DxfImportStats 携带 LAYER 图层表（纯数据，无循环依赖）

// 图纸文件格式互操作层（纯函数，不依赖任何 UI / 模型对象）。
//
// 设计原则：
//   1) 只接收 std::vector<Element>，返回文件字节，便于在核心层自测直接覆盖；
//   2) 与 SketchModel 解耦，MainWindow 只负责"选文件 → 调这里 → 写盘"；
//   3) 每种格式一个独立函数，扩展名分派由 formatFromSuffix 唯一决定。
//
// 支持的格式：
//   Json — 本程序原生草图格式（完整往返，含样式/标注/文字）；UTF-8
//   Dxf  — AutoCAD DXF **R12 (AC1009)** 与 **R2000 (AC1015)** 读写（ASCII + 二进制容器）。
//          默认把图元炸开为 LINE/CIRCLE/ARC/POLYLINE/TEXT/SOLID 基元（最大兼容）；
//          开启 DxfExportOptions 后块/标注/填充按原生实体导出（INSERT/DIMENSION/HATCH），
//          在 AutoCAD/ZWCAD 中可编辑。R2000 在 R12 token 流之上注入 AcDb 子类标记(100)/
//          句柄(5)/owner(330)并改写 $ACADVER=AC1015、追加 OBJECTS 段（见 CadFormats.cpp）。
//   Step — ISO 10303-21（AP214 二维曲线集：LINE/CIRCLE/TRIMMED_CURVE/POLYLINE/ELLIPSE）；ASCII
//   Dwg  — 原生 DWG R12 (AC1009) 二进制格式。本项目不引入 ODA/LibreDWG 等第三方
//          库，手写实现最广泛兼容的 R12 子集：实体炸开为 LINE/CIRCLE/ARC/TEXT/
//          SOLID/POLYLINE+VERTEX（与 DXF 导出同一套基元展开），封装进原生 R12
//          二进制容器（文件头 + 图层表 + 实体记录，每记录附 CRC16），可被
//          AutoCAD / LibreCAD / QCAD / BricsCAD 等读取。详见 CadFormats.cpp。
//
//  第三方内核说明（详见 docs/third-party-dxf-license.md 与
//  docs/dwg-r2000-bridge-design-analysis.md）：
//   D1 起读取 R2000+ DWG 经**进程外** LibreDWG 转换器（GPLv3，独立 exe +
//   QProcess 桥接，主程序零 GPL 代码链入，见 src/core/DwgBridge.h）。
//   ODA Drawings SDK 为商业授权，未接入。
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

// DXF 目标版本（导出侧选择；导入侧据此协商是否支持）。
//   R12   — AC1009（默认，与升级前逐字节一致）
//   R2000 — AC1015（注入 AcDb 子类标记 / 句柄 / owner + 改写 $ACADVER + OBJECTS 段）
enum class DxfVersion { R12, R2000 };

// DXF 容器形态：ASCII 文本（默认）或二进制 DXF（带 22 字节哨兵头的紧凑二进制）。
enum class DxfContainer { Ascii, Binary };

// 版本枚举 → 简短标签（"R12" / "R2000"），用于下拉与日志。
QString dxfVersionLabel(DxfVersion v);
// AutoCAD 内部版本串（"AC1009"/"AC1015"/"AC1021"…）→ 人类可读名（"DWG R12"/"DWG 2000"…）。
QString cadVerToHuman(const QString& acadVer);
// 该 AutoCAD 版本串 SketchCAD 是否支持读取。
//   支持：AC1009(R12) / AC1012(R13) / AC1014(R14) / AC1015(R2000) / AC1018(R2000i)
//   不支持：AC1021(R2004) 及以后（R2007=AC1024 / R2010=AC1027 / R2013=AC1032 / R2018=AC1032…）
bool isSupportedDxfVersion(const QString& acadVer);
// 探测字节流头部版本串（ASCII 走文本解码，Binary 走二进制组流）。
//   无法识别 / 不支持时回落 R12（调用方应再用 isSupportedDxfVersion 判定是否拒绝）。
DxfVersion detectDxfVersion(const QByteArray& data, bool isBinary = false);

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
    DxfVersion   version   = DxfVersion::R12;     // ⑤ 目标 DXF 版本（默认 R12，逐字节兼容旧档）
    DxfContainer container = DxfContainer::Ascii; // ⑥ 容器形态（默认 ASCII 文本；Binary=二进制 DXF）
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
// 二进制 DXF 写出：内部生成（R12 或 R2000）文本 DXF 后序列化为带 22 字节哨兵头的二进制容器。
// opts==nullptr 或 version=R12/container=Ascii 时与 toDxf 等价（仅容器不同）。
QByteArray toDxfBinary(const std::vector<Element>& elems, QString* err = nullptr,
                       double drawingScale = 1.0, int insunits = 4,
                       const std::vector<BlockDef>* blocks = nullptr,
                       const DxfExportOptions* opts = nullptr);
QByteArray toDwg (const std::vector<Element>& elems, QString* err = nullptr,
                  double drawingScale = 1.0);
// 说明：toDwg 输出**原生 R12 (AC1009) 二进制**格式（手写实现，零第三方依赖）：
//   把图元炸开为 LINE / CIRCLE / ARC / TEXT / SOLID / POLYLINE+VERTEX 基本体，
//   封装进原生 R12 二进制容器（含图层表、实体记录、每记录 CRC16），
//   可被 AutoCAD / LibreCAD / QCAD / BricsCAD 作为 .dwg 直接打开。

// DWG R2000 (AC1015) 导出（D1：经外部转换器桥，见 DwgBridge.h）。
//   路径：内部先 toDxf(R2000) 生成 DXF → dwgwrite --as r2000 进程外转换。
//   失败返回空并把原因写入 *err（含转换器 stderr 摘要）。
//   ⚠️ LibreDWG 写能力上限即 R2000：R2004/R2007 的 DWG 写不支持（业界惯例是
//   交付 R2000 DWG 或 R2007 DXF），本函数**不虚标**更高版本。
//   转换组件未安装时也走此错误通道（报文含指引）。
QByteArray toDwgR2000 (const std::vector<Element>& elems, QString* err = nullptr,
                       double drawingScale = 1.0);

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
    std::vector<TextStyle> styles;  // 从 TABLES 段解析出的文字样式（供调用方 addTextStyle；
                                     //   已剔除字体文件名等无法跨程序解释的原始字段）
    std::vector<Layer> layers;      // 从 TABLES 段解析出的 LAYER 图层表（供调用方注册：
                                     //   addLayer/setLayerStyle，byLayer 图元的层色解析依赖它）
};                         //   调用方按 Units.h::lengthUnitFromInsunits 换算到 mm
bool fromDxf(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
             DxfImportStats* stats = nullptr);
// 二进制 DXF 回读：校验哨兵头 + 探测版本（不支持返回 false 且不崩溃）；版本兼容后
// 解码为 token 流复用 fromDxf 的实体分发（R2000 的 100/5/330 标记被自然忽略）。
bool fromDxfBinary(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
                   DxfImportStats* stats = nullptr);

// DWG 回读（D1 版本自动路由）：
//   * AC1009 (R12) —— 自研原生二进制解析（下述既有实现，语义不动）；
//   * AC1012~AC1032 (R13~R2018) —— 经外部转换器桥（DwgBridge::dwgToDxf）
//     转为 DXF 后复用 fromDxf 回读；转换组件未安装/版本超界时返回 false，
//     *err 含版本串原文（如"不支持的 DWG 版本：AC1021（DWG 2004）"）。
// 成功返回 true（out 追加解析到的图元）。
// R12 路径：解析图层表得到图层名，实体记录的图层索引映射回图层名；颜色由
// ACI 还原为 RGB。文本按 UTF-8 解码（R12 无真彩色组码，颜色经 62→ACI→RGB 近似）。
bool fromDwg(const QByteArray& data, std::vector<Element>& out, QString* err = nullptr,
             DxfImportStats* stats = nullptr);

// ---- 外部块表读取（设计中心侧栏：列出外部文件的块定义，双击插入）----
// 从 SketchCAD 原生存档 JSON（顶层含 "blocks" 数组）读取块定义列表；文件不存在/解析失败 → 空。
std::vector<BlockDef> blockTableFromJson(const QString& path);
// 从 DXF 文件的 BLOCKS 段读取块定义列表（仅取块表，不导入 ENTITIES）；文件不存在/解析失败 → 空。
std::vector<BlockDef> blockTableFromDxf(const QString& path);

} // namespace cadfmt
} // namespace SketchCAD
