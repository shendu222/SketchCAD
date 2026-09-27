#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {
namespace corner {

// ============================================================
// 倒角（Chamfer）/ 倒圆（Fillet）—— 核心几何层
// ============================================================
// 设计目标：对两条开放直线段（ElementType::Line）在它们所在直线的交点（角点）处
// 做倒角或倒圆；纯几何、不依赖模型与界面，可被命令行 / 菜单 / 交互工具直接调用，
// 也易于扩展为折线角点、矩形角等 contours 处理。
//
// 输入输出（单位与工程一致，mm）：
//   in  : a, b                 两条开放直线段
//         pa, pb               用户点选位置（byPick=true 时决定「保留端」）
//         d1, d2 / d, angle / r 倒角距离、倒角角度、倒圆半径
//   out : CornerResult { lineA, lineB, corner, ok, msg }
//           lineA/lineB —— 裁短后的两条线（保留端 + 裁断点），type=Line
//           corner     —— 倒角直线(Line) 或 倒圆圆弧(Arc)，继承 a 的样式
//           ok=false    —— 不产出有效几何，msg 给出可读原因
//
// 保留端规则：
//   byPick=false（自动）  —— 保留「远离角点」的一端，适合「选中两条线直接处理」；
//   byPick=true（点选）   —— 保留离 pa/pb 最近的一端，与修剪/延伸交互一致。
// 只移动「离角点近的一端」（裁断点），保留端恒不变。

struct CornerResult {
    Element lineA;    // 裁短后的第 1 条（保留端 + 裁断点）
    Element lineB;    // 裁短后的第 2 条
    Element corner;   // 倒角直线 或 倒圆圆弧（继承 a 的样式/图层/线宽）
    bool    ok = false;
    QString msg;      // 失败原因（平行 / 共线 / 夹角过小 / 参数非法 / 距离越界）
};

// 倒角（距离-距离）：沿 a 从角点回退 d1、沿 b 回退 d2，补直线连接两个裁断点。
CornerResult chamferPair(const Element& a, const Element& b,
                        double d1, double d2,
                        bool byPick = false,
                        const Vec3& pa = Vec3(), const Vec3& pb = Vec3());

// 倒角（距离-角度）：沿 a 回退 d，倒角直线与 a 夹角 angleDeg（朝 b 一侧），
// 自动求 b 上的回退距离 d2。要求 0 < angleDeg < 两线夹角。
CornerResult chamferPairDistAngle(const Element& a, const Element& b,
                                  double d, double angleDeg,
                                  bool byPick = false,
                                  const Vec3& pa = Vec3(), const Vec3& pb = Vec3());

// 倒圆：半径 r，补与两线相切的圆弧（圆心在夹角平分线上，距角点 r/sin(θ/2)）。
CornerResult filletPair(const Element& a, const Element& b,
                        double r,
                        bool byPick = false,
                        const Vec3& pa = Vec3(), const Vec3& pb = Vec3());

} // namespace corner
} // namespace SketchCAD
