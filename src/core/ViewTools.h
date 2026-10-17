#pragma once
// P6（Ch6 视图工具）：向视符号 / 孔轴投影 / 转角投影 生成器
// ------------------------------------------------------------
// 对标 SectionCutSymbol 的「符号生成范例」范式：纯生成器返回图元集合，调用方 append。
// FORBID「自造投影矩阵（用既有 Transform）」：转角投影的方向旋转复用 geo::rotateAroundCentroid
// （Geometry.h 既有 Transform 原语），其余仅用三角函数求方向向量，不构造投影矩阵。
// Phase-1 直接返回 std::vector<Element>（与 P1/P3/P4 一致）；IO_SPEC 的「符号 BlockRef」打包
// 可经 PartLibrary::checkout 达成，此处记偏差（见 docs/feature-expansion/P6...md）。
#include <vector>
#include <QString>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

// 向视符号（向视图，GB/T 4458.1）：箭头指向观察方向 + 字母标识。
struct DirectionSymbolSpec {
    Vec3  anchor;                 // 箭头尾端锚点（世界坐标）
    double angleDeg = 90.0;       // 观察方向角（度；0=+X，逆时针为正）
    double arrowLen = 8.0;        // 箭杆长(mm)
    QString letter = QStringLiteral("A");  // 视图字母标识（单字符）
    double textHeight = 5.0;      // 字母字高(mm)
    QString layer = QStringLiteral("标注");
    bool  hasDirection = true;    // false → 缺方向，生成失败
};
// 生成向视符号（箭杆 + V 形箭头 + 字母）；缺方向/箭长非正 → 返回 false（out 清空）。
bool buildDirectionSymbol(const DirectionSymbolSpec& s, std::vector<Element>& out);

// 孔轴投影（孔/轴沿给定方向的投影）：中心线(点画线) + 两端投影圆 + 两侧轮廓线。
struct HoleProjectionSpec {
    Vec3  anchor;                 // 孔/轴中心（近端）
    double diameter = 10.0;       // 孔/轴直径(mm)
    double depth = 30.0;          // 沿轴投影长度(mm)
    double angleDeg = 90.0;       // 轴向角（度；0=+X）
    QString layer = QStringLiteral("标注");
    bool  hasDirection = true;    // false → 缺方向，生成失败
};
// 生成孔轴投影；缺方向/直径或深度非正 → false。
bool buildHoleProjection(const HoleProjectionSpec& s, std::vector<Element>& out);

// 转角投影（轴线经一次转角后的投影路径）：折线 p0→p1→p2，第二段方向由转角旋转而得。
struct RotationProjectionSpec {
    Vec3  anchor;                 // 起点
    double length1 = 20.0;        // 第一段长(mm)
    double turnAngleDeg = 45.0;   // 转角（度，逆时针为正）
    double length2 = 15.0;        // 第二段长(mm)
    double startAngleDeg = 0.0;   // 起始方向角（度）
    QString layer = QStringLiteral("标注");
    bool  hasAngle = true;        // false → 缺转角，生成失败
};
// 生成转角投影折线（Polyline，3 点）；缺转角/段长非正 → false。
bool buildRotationProjection(const RotationProjectionSpec& s, std::vector<Element>& out);

} // namespace SketchCAD
