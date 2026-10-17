#include "SectionProps.h"
#include "DensityTable.h"
#include <cmath>

namespace SketchCAD {
namespace {

// 鞋带公式叉积项 cross_i = x_i*y_{i+1} - x_{i+1}*y_i（闭合）
inline double crossAt(const std::vector<Vec3>& p, int i) {
    const int n = (int)p.size();
    const int j = (i + 1) % n;
    return p[i].x * p[j].y - p[j].x * p[i].y;
}

// 线段相交判定（方向法，含共线重叠）。相邻共享端点不算（自交检测时排除）。
bool segIntersect(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    auto cross2 = [](double ox, double oy, double ax, double ay, double bx, double by) {
        return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
    };
    const double d1 = cross2(a.x, a.y, b.x, b.y, c.x, c.y);
    const double d2 = cross2(a.x, a.y, b.x, b.y, d.x, d.y);
    const double d3 = cross2(c.x, c.y, d.x, d.y, a.x, a.y);
    const double d4 = cross2(c.x, c.y, d.x, d.y, b.x, b.y);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
        return true;
    // 共线且投影重叠
    auto onSeg = [](double px, double py, double qx, double qy, double rx, double ry) {
        return std::fmin(px, qx) - 1e-9 <= rx && rx <= std::fmax(px, qx) + 1e-9 &&
               std::fmin(py, qy) - 1e-9 <= ry && ry <= std::fmax(py, qy) + 1e-9;
    };
    if (std::fabs(d1) < 1e-9 && onSeg(a.x, a.y, b.x, b.y, c.x, c.y)) return true;
    if (std::fabs(d2) < 1e-9 && onSeg(a.x, a.y, b.x, b.y, d.x, d.y)) return true;
    if (std::fabs(d3) < 1e-9 && onSeg(c.x, c.y, d.x, d.y, a.x, a.y)) return true;
    if (std::fabs(d4) < 1e-9 && onSeg(c.x, c.y, d.x, d.y, b.x, b.y)) return true;
    return false;
}

} // namespace

bool SectionProps::isSelfIntersecting(const std::vector<Vec3>& polygon) {
    const int n = (int)polygon.size();
    if (n < 4) return false;                 // 三角形不可能自交
    for (int i = 0; i < n; ++i) {
        const Vec3 a1 = polygon[i], a2 = polygon[(i + 1) % n];
        for (int j = i + 1; j < n; ++j) {
            // 排除相邻边（共享端点）
            if (j == i || (j + 1) % n == i || (i + 1) % n == j) continue;
            const Vec3 b1 = polygon[j], b2 = polygon[(j + 1) % n];
            if (segIntersect(a1, a2, b1, b2)) return true;
        }
    }
    return false;
}

bool SectionProps::compute(const std::vector<Vec3>& polygon, SectionProperties& out, QString* reason) {
    out = SectionProperties{};
    const int n = (int)polygon.size();
    if (n < 3) { if (reason) *reason = QStringLiteral("截面多边形顶点不足 3"); return false; }
    if (isSelfIntersecting(polygon)) {
        if (reason) *reason = QStringLiteral("截面多边形自交（无效截面）");
        return false;
    }

    // 鞋带公式：面积与形心
    double a2 = 0.0;            // 2×有向面积
    double cx6 = 0.0, cy6 = 0.0;  // 6A·Cx, 6A·Cy 的累加
    for (int i = 0; i < n; ++i) {
        const Vec3& p = polygon[i];
        const Vec3& q = polygon[(i + 1) % n];
        const double cr = p.x * q.y - q.x * p.y;
        a2  += cr;
        cx6 += (p.x + q.x) * cr;
        cy6 += (p.y + q.y) * cr;
    }
    const double A = a2 * 0.5;
    if (std::fabs(A) < 1e-12) { if (reason) *reason = QStringLiteral("截面面积退化（≈0）"); return false; }
    const double sgn = (A < 0) ? -1.0 : 1.0;   // 顶点方向：Green 定理矩公式随有向面积变号
    const double area = std::fabs(A);
    out.area = area;
    out.centroid = Vec3(cx6 / (6.0 * A), cy6 / (6.0 * A), 0.0);

    // 二次矩/惯性积（关于原点，给定标准公式）；再平移至形心轴（平行轴定理）
    //   Ix_o = (1/12) Σ (y_i² + y_i y_j + y_j²)·cross_i
    //   Iy_o = (1/12) Σ (x_i² + x_i x_j + x_j²)·cross_i
    //   Ixy_o= (1/24) Σ (x_i y_j + 2 x_i y_i + 2 x_j y_j + x_j y_i)·cross_i
    double ixO = 0.0, iyO = 0.0, ixyO = 0.0;
    for (int i = 0; i < n; ++i) {
        const Vec3& p = polygon[i];
        const Vec3& q = polygon[(i + 1) % n];
        const double cr = p.x * q.y - q.x * p.y;
        ixO  += (p.y * p.y + p.y * q.y + q.y * q.y) * cr;
        iyO  += (p.x * p.x + p.x * q.x + q.x * q.x) * cr;
        ixyO += (p.x * q.y + 2.0 * p.x * p.y + 2.0 * q.x * q.y + q.x * p.y) * cr;
    }
    // 随顶点方向校正（CW 时公式给负值）：Ix/Iy 取正，Ixy 保留有符号
    const double Ix_o = sgn * ixO / 12.0, Iy_o = sgn * iyO / 12.0, Ixy_o = sgn * ixyO / 24.0;
    const double Cx = out.centroid.x, Cy = out.centroid.y;
    // 平行轴定理平移至形心轴
    out.inertiaIx  = Ix_o - area * Cy * Cy;
    out.inertiaIy  = Iy_o - area * Cx * Cx;
    out.inertiaIxy = Ixy_o - area * Cx * Cy;
    out.valid = true;
    return true;
}

bool SectionProps::massGrams(double areaMm2, const QString& material, double& gramsOut, double thicknessMm) {
    const double d = DensityTable::density(material);
    if (d <= 0.0) return false;                       // 未知材料
    gramsOut = areaMm2 * thicknessMm * d * 1e-3;      // mm²·mm·(g/cm³)·1e-3 = g
    return true;
}

} // namespace SketchCAD
