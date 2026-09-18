#pragma once
#include <cmath>
#include <QString>
#include <vector>

namespace SketchCAD {

// 3D 点（草图工作平面上 z 通常为 0）
// 扩展为自包含的向量类型：把原先散落在 DimStyle / LineModes / CircleModes
// 三处重复实现的 normalize / dot / perp / angleDeg 统一收敛为成员方法，
// 消除 DRY 违规的同时保证三处行为完全一致。
struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double x_, double y_, double z_ = 0) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    bool   operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }

    double len() const { return std::sqrt(x * x + y * y + z * z); }
    // 2D 长度（忽略 z），用 hypot 避免溢出
    double len2D() const { return std::hypot(x, y); }
    double distTo(const Vec3& o) const { return (*this - o).len(); }

    // 2D 点积（DimStyle / LineModes 里的 dot2 统一到这里）
    double dot2D(const Vec3& o) const { return x * o.x + y * o.y; }

    // 逆时针旋转 90°（DimStyle / LineModes 里的 perp 统一到这里）
    Vec3 perp() const { return Vec3(-y, x, 0); }

    // 归一化为 2D 单位向量。零向量返回 (1,0,0)。
    // DimStyle 和 LineModes 各自有一份实现，行为略有差异（一个用 len()，一个用 hypot），
    // 统一后使用 len2D() 保证 2D 场景下的一致性。
    Vec3 normalized() const {
        const double L = len2D();
        return L < 1e-12 ? Vec3(1, 0, 0) : Vec3(x / L, y / L, 0);
    }

    // 与 X 轴正方向的夹角（度），范围 (-180, 180]。
    // DimStyle 里的 degOf 和各处 atan2 调用统一到这里。
    double angleDeg() const { return std::atan2(y, x) * 180.0 / M_PI; }

    // 与 X 轴正方向的夹角（弧度），范围 (-π, π]。
    // PolyModes.cpp 等模块中大量 std::atan2(dy, dx) 调用统一到这里。
    double angleRad() const { return std::atan2(y, x); }

    QString str() const { return QString("%1, %2, %3").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(z, 0, 'f', 2); }
};

// 工作平面：决定哪两个轴映射到屏幕 (u,v)
enum class Plane { XY, YZ, XZ };
inline const char* planeName(Plane p) {
    return p == Plane::XY ? "XY" : (p == Plane::YZ ? "YZ" : "XZ");
}

// ===================== 数学工具函数（DRY 收敛点） =====================
// 以下函数原先在 DimStyle.cpp / LineModes.cpp / CircleModes.cpp 中各有
// 一份 static 实现，现在统一为 inline 自由函数，供所有模块共用。

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace math {

// 角度 ↔ 弧度（LineModes.cpp 里的 deg2rad / rad2deg 统一到这里）
inline double deg2rad(double d) { return d * M_PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / M_PI; }

// 由角度构造 2D 单位向量（DimStyle.cpp 里的 unitOf 统一到这里）
inline Vec3 unitOf(double deg) {
    const double a = deg2rad(deg);
    return Vec3(std::cos(a), std::sin(a), 0);
}

// 点 p 到直线 ab 的投影点（LineModes.cpp 里的 projPointLine 统一到这里）
inline Vec3 projPointLine(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 ab = b - a;
    const double L2 = ab.x * ab.x + ab.y * ab.y;
    if (L2 < 1e-18) return a;
    const double t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / L2;
    return a + ab * t;
}

// 点到直线的距离（LineModes.cpp 里的 pointLineDist 统一到这里）
inline double pointLineDist(const Vec3& p, const Vec3& a, const Vec3& b) {
    return p.distTo(projPointLine(p, a, b));
}

// 点到直线的有符号距离（CircleModes.cpp 里的 signedDistLine 统一到这里）
// 正值在左侧，负值在右侧；方向从 a→b
inline double signedDistLine(const Vec3& p, const Vec3& a, const Vec3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-12) return 0;
    return (dx * (a.y - p.y) - dy * (a.x - p.x)) / len;
}

// 两条无限直线的交点（CircleModes.cpp 里的 lineLineIntersect 统一到这里）
// 平行时返回 false
inline bool lineLineIntersect(const Vec3& a1, const Vec3& a2,
                              const Vec3& b1, const Vec3& b2, Vec3& out) {
    const double d1x = a2.x - a1.x, d1y = a2.y - a1.y;
    const double d2x = b2.x - b1.x, d2y = b2.y - b1.y;
    const double denom = d1x * d2y - d1y * d2x;
    if (std::fabs(denom) < 1e-12) return false;
    const double t = ((b1.x - a1.x) * d2y - (b1.y - a1.y) * d2x) / denom;
    out = Vec3(a1.x + t * d1x, a1.y + t * d1y, 0);
    return true;
}

// 从候选点列表中选离 cur 最近的（CircleModes.cpp 里的 pickNearest 统一到这里）
inline Vec3 pickNearest(const std::vector<Vec3>& candidates, const Vec3& cur) {
    if (candidates.empty()) return Vec3();
    Vec3 best = candidates[0];
    double bestD = best.distTo(cur);
    for (size_t i = 1; i < candidates.size(); ++i) {
        const double d = candidates[i].distTo(cur);
        if (d < bestD) { bestD = d; best = candidates[i]; }
    }
    return best;
}

} // namespace math

} // namespace SketchCAD
