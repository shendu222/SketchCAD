#pragma once
#include <vector>
#include <cstddef>
#include <QByteArray>
#include "Element.h"   // SketchCAD::Element

// =====================================================================
// 代理图形（Proxy Graphics）提取子系统 —— 命名空间 proxy
//
// 背景：AutoCAD 保存含「自定义 / 代理实体」（如 ObjectARX 应用、Mechanical
// 标准件、三维实体等）的图形时，会把该实体的「显示几何快照」以「内嵌二进制
// DXF」的形式一并写入文件（前提是 $PROXYGRAPHICS=1）。当标准转换器（LibreDWG）
// 无法把这些自定义实体展开为普通 LINE/CIRCLE/ARC 时，转换出的 DXF 实体段为空；
// 此时我们可直接从原始 DWG 中抠出这段内嵌二进制 DXF，解码为图元，从而恢复可见
// 几何——这正是「代理图形提取」修复项要做的事。
//
// 内嵌二进制 DXF 的字节格式（与文件级二进制 DXF 完全一致，参见
// CadFormats.cpp::binaryDxfToAscii / asciiDxfToBinary）：
//   * 组码 2 字节小端（little-endian）；
//   * 字符串（组码 0-9 / 100 / 102 / 105 / 300-309 / ...）为「空终止」；
//   * 数值（I16 / I32 / I64 / Dbl）按组码定长小端；
//   * Chunk（310-319）为 1 字节长度 + 数据。
// 与文件级二进制 DXF 的唯二区别：① 没有 22 字节哨兵头；② 在 DWG 流中由
// 「4 字节小端长度前缀」包裹（长度 = 紧随其后的二进制 DXF 字节数）。
//
// 解码出来的 (组码, 值) token 流，若自身未带 SECTION/ENTITIES 则包一层，
// 然后复用 cadfmt::fromDxf 的实体分发——与现有 DXF 导入架构、Element 模型
// 完全保持一致，不另起炉灶。
//
// 能力边界（诚实）：仅对「标准内嵌二进制 DXF」代理有效。AutoCAD 的部分私有
// 编译代理（如 Mechanical STDPART2D 标准件使用的私有显示列表）并非二进制 DXF，
// 而是不可公开解码的私有格式，本子系统对其诚实返回 0 且不崩溃。
// =====================================================================
namespace proxy {

// 解码单块「标准内嵌二进制 DXF」代理图形数据。
// blob：纯二进制 DXF 主体（不含 4 字节长度前缀、不含文件级哨兵头）。
// 成功且解出至少 1 个顶层图元 → 返回 true，图元追加到 out；失败返回 false（out 不变）。
bool decodeProxyGraphicsBlob(const QByteArray& blob, std::vector<SketchCAD::Element>& out);

// 整档模糊扫描：在 dwg 中查找「4 字节小端长度前缀 + 二进制 DXF」候选块，
// 逐块调用 decodeProxyGraphicsBlob，接纳解码出有效图元的块。
// 返回提取到的图元总数（累加所有被接纳的块）。对 AutoCAD 私有编译代理诚实
// 返回 0 且不崩溃。
std::size_t extractProxyGraphics(const QByteArray& dwg, std::vector<SketchCAD::Element>& out);

} // namespace proxy
