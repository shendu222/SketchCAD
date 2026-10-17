#pragma once
// P5（Ch10/11 参数化国标图库）：GB 紧固件规格表（六角螺栓 / 六角螺母 / 平垫圈）
// ------------------------------------------------------------
// ⚠️ 规格数值为「人工给定(G3)」：以下为常用公称规格 M3~M16 的标准 GB 参考值，
//    禁止在代码中改写；与现行国标不一致时**以人工提供为准**（ESCALATE G3 门禁）。
//    来源（须由人工校核后启用）：GB/T 5782 六角头螺栓、GB/T 6170 1型六角螺母、GB/T 97.1 平垫圈-A级。
// 字段语义（统一结构，按 kind 取用）：
//   HexBolt      : nominal=d(螺纹公称), acrossFlats=s(对边), thickness=k(头高), outerD=对角(2s/√3), innerD≈d
//   HexNut       : nominal=d,              acrossFlats=s,          thickness=m(厚),   outerD=对角,         innerD≈d(螺纹孔径)
//   PlainWasher  : nominal=d,              acrossFlats=0(不用),     thickness=h(厚),   innerD=d1(内径),    outerD=d2(外径)
#include <QVector>
#include <QJsonArray>

namespace SketchCAD {

enum class FastenerKind { HexBolt = 0, HexNut = 1, PlainWasher = 2 };

struct FastenerSpec {
    double nominal     = 0.0;   // 公称直径 d (mm)
    double acrossFlats = 0.0;   // 对边宽度 s (mm)（垫圈不用）
    double thickness   = 0.0;   // 螺栓=头高 k / 螺母=厚 m / 垫圈=厚 h (mm)
    double innerD      = 0.0;   // 内径：垫圈 d1；螺栓/螺母简化为螺纹孔径
    double outerD      = 0.0;   // 外径：垫圈 d2；螺栓/螺母头部对角(外接圆)
    bool   valid       = false; // 是否命中规格表（false = 回落最近规格）
};

namespace GbFastenerTable {

// 按类型 + 公称规格查表。命中 → spec.valid=true；未命中 → 返回最接近规格且 valid=false
// （调用方据此回落默认 + 记日志）。
FastenerSpec lookup(FastenerKind kind, double nominal);

// 该类型支持的规格列表（公称直径，升序）。
QVector<double> sizes(FastenerKind kind);

QString    kindName(FastenerKind kind);
FastenerKind kindFromName(const QString& name);

// 标准标记，如 "GB/T 5782 M6"。
QString    standardTag(FastenerKind kind, double nominal);

// ---- 库维护：规格表 ↔ JSON（IO_SPEC「库维护 = 编辑 JSON 规格」）----
// toJson 导出自描述语义键（螺栓 acrossFlats/headHeight/innerD；螺母 acrossFlats/thickness/innerD；
// 垫圈 innerD/outerD/thickness），可人工编辑后再 fromJson 覆盖规格表，出库即时反映。
QJsonArray toJson(FastenerKind kind);                     // 导出当前规格表（可编辑）
bool       fromJson(FastenerKind kind, const QJsonArray& arr);  // 用 JSON 覆盖规格表（空/缺 nominal → false，不改动）
void       resetToDefaults(FastenerKind kind);            // 恢复人工给定(G3)默认值
void       resetToDefaults();                            // 全部恢复

} // namespace GbFastenerTable
} // namespace SketchCAD
