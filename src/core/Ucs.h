#pragma once
#include <cmath>
#include <QString>
#include "Vec.h"

// ============================================================
// 用户坐标系（UCS）
// ------------------------------------------------------------
// 设计要点：
//   · UCS 是一个 **2D 刚性变换**（原点平移 + 绕世界 Z 轴旋转），不改动任何
//     图元存储的世界坐标语义。Element.pts 始终是「世界坐标」；UCS 只存在于
//     「用户输入 → 世界坐标」与「世界坐标 → 显示」这两道转换边界上。
//   · 旋转矩阵保持正交、可逆，z 分量直通（本模块只处理 XY 平面内的工作平面旋转）。
//   · 默认 UCS = 世界坐标系（origin=0, angleRad=0），即单位变换；
//     所有既有命令/自测在无 UCS 操作时行为完全不变。
//   · toWorld / toUcs 是唯一双向换算入口；Snap / GraphicsView / 命令行点解析
//     全部经此转换，杜绝「同一处换算散落多处导致方向不一致」。
// ============================================================
namespace SketchCAD {

struct Ucs {
    QString name;          ///< 命名 UCS 的标识（"WORLD" 为内置世界系）
    Vec3    origin;        ///< UCS 原点（世界坐标）
    double  angleRad = 0;  ///< 绕世界 Z 轴逆时针旋转角（弧度）

    Ucs() = default;
    Ucs(const QString& n, const Vec3& o, double a) : name(n), origin(o), angleRad(a) {}

    // 是否为世界坐标系（单位变换）
    bool isWorld() const {
        return origin.x == 0.0 && origin.y == 0.0 && origin.z == 0.0 && angleRad == 0.0;
    }

    // UCS X 轴在世界系下的方向（单位向量，z=0）
    Vec3 xAxis() const { return Vec3(std::cos(angleRad), std::sin(angleRad), 0); }
    // UCS Y 轴在世界系下的方向（单位向量，z=0，与 X 正交）
    Vec3 yAxis() const { return Vec3(-std::sin(angleRad), std::cos(angleRad), 0); }

    // UCS 局部坐标 → 世界坐标：先旋转后平移
    Vec3 toWorld(const Vec3& p) const {
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        return Vec3(origin.x + p.x * c - p.y * s,
                    origin.y + p.x * s + p.y * c,
                    origin.z + p.z);
    }

    // 世界坐标 → UCS 局部坐标：toWorld 的逆变换
    Vec3 toUcs(const Vec3& w) const {
        const double c = std::cos(angleRad), s = std::sin(angleRad);
        const double dx = w.x - origin.x, dy = w.y - origin.y;
        return Vec3(dx * c + dy * s,
                    -dx * s + dy * c,
                    w.z - origin.z);
    }

    // 人类可读摘要（状态栏 / 日志用）：如 "MYUCS @(10,20) 45°"
    QString describe() const {
        const int deg = (int)std::lround(angleRad * 180.0 / M_PI);
        return QString("%1 @(%2,%3) %4°")
            .arg(name)
            .arg(origin.x, 0, 'f', 1).arg(origin.y, 0, 'f', 1)
            .arg(deg);
    }
};

} // namespace SketchCAD
