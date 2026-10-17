#include "InvoluteGear.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {

namespace {
constexpr double K_PI = 3.14159265358979323846;

// 渐开线函数 inv(φ) = tanφ - φ（展开角 t），φ 为对应半径处压力角
double involuteFn(double phi) { return std::tan(phi) - phi; }

// 单个轮齿一侧齿廓（自基圆到齿顶）采样点，置于「齿居中、对称轴=+X」的局部坐标系。
// side = +1 取上半齿廓（角度为正），side = -1 取镜像（角度为负）。
// 返回点集按半径升序（根→顶）；rootR 为齿根圆半径，tipR 为齿顶圆半径，
// offset 为基圆处齿廓相对对称轴的半角（保证分度圆处半齿厚角 = π/(2z)）。
std::vector<Vec3> flank(int teeth, double rb, double rf, double ra, double offset, int side) {
    std::vector<Vec3> pts;
    pts.reserve(48);
    const int N = 24;                       // 每侧采样段数
    const double rStart = std::max(rb, rf); // 渐开线仅在 rho>=rb 存在；rb>rf 时从基圆起采样
    for (int i = 0; i <= N; ++i) {
        const double rho = rStart + (ra - rStart) * (double)i / N;
        double x, y;
        if (rho < rb - 1e-9) {
            // 基圆以下：沿基圆处径向直线（非渐开线区，按径向退化处理）
            const double ang = offset;
            x = rho * std::cos(ang);
            y = rho * std::sin(ang);
        } else {
            const double phi = std::acos(std::min(1.0, rb / rho));   // 该半径处压力角
            const double t = std::tan(phi);              // 渐开线展开参数：半径 = rb*sqrt(1+t^2) = rho
            double rx, ry;
            InvoluteGear::involute(rb, t, rx, ry);       // 标准渐开线（G1 公式）
            // 刚性旋转 offset，使分度圆处半齿厚角 = π/(2z)
            x = rx * std::cos(offset) - ry * std::sin(offset);
            y = rx * std::sin(offset) + ry * std::cos(offset);
        }
        if (side < 0) y = -y;                            // 下半齿廓取镜像
        pts.push_back(Vec3(x, y, 0.0));
    }
    (void)teeth;
    return pts;
}
} // namespace

void InvoluteGear::involute(double rb, double t, double& x, double& y) {
    // 人工给定(G1)标准公式 —— 唯一权威实现点，禁止自造等价/近似表达式
    x = rb * (std::cos(t) + t * std::sin(t));
    y = rb * (std::sin(t) - t * std::cos(t));
}

double InvoluteGear::baseRadius(int teeth, double module, double pressureAngleDeg) {
    const double alpha = pressureAngleDeg * K_PI / 180.0;
    return pitchRadius(teeth, module) * std::cos(alpha);
}

double InvoluteGear::addendumRadius(int teeth, double module, double addendumFactor) {
    return pitchRadius(teeth, module) + addendumFactor * module;
}

double InvoluteGear::dedendumRadius(int teeth, double module, double dedendumFactor) {
    return pitchRadius(teeth, module) - dedendumFactor * module;
}

bool InvoluteGear::profile(int teeth, double module, double pressureAngleDeg,
                           std::vector<Vec3>& outPoints, bool fullGear,
                           double addendumFactor, double dedendumFactor) {
    outPoints.clear();
    if (teeth < 2 || module <= 0.0) return false;       // 退化：齿数非法 / 模数非正
    const double alpha = pressureAngleDeg * K_PI / 180.0;
    const double rb = baseRadius(teeth, module, pressureAngleDeg);
    double ra = addendumRadius(teeth, module, addendumFactor);
    double rf = dedendumRadius(teeth, module, dedendumFactor);
    if (rf < 0.1 * ra) rf = 0.1 * ra;                  // 防止小齿数根圆为负（非物理降级）
    if (ra <= rb) return false;                         // 齿顶不入基圆（几何非法）

    // 分度圆处半齿厚角 = π/(2z)；offset 使渐开线经旋转后于分度圆处达到该角
    // 分度圆处展开角 t_p = inv(α)，原始渐开线在该半径的极角 ψ_p ≈ 0（α 较小时），
    // 故 offset ≈ π/(2z) - ψ_p；此处按通用式计算（ψ_p 由 involute 反解）。
    const double tP = involuteFn(alpha);
    double xp, yp;
    involute(rb, tP, xp, yp);                           // 分度圆处渐开线原始点
    const double psiP = std::atan2(yp, xp);             // 原始极角（≤0）
    const double offset = K_PI / (2.0 * teeth) - psiP;  // 旋转量：分度圆半齿厚角 = π/(2z)

    const std::vector<Vec3> upper = flank(teeth, rb, rf, ra, offset, +1);
    const std::vector<Vec3> lower = flank(teeth, rb, rf, ra, offset, -1);
    const double step = 2.0 * K_PI / teeth;

    if (!fullGear) {
        // 单齿廓（开放）：下半根 → 下半顶 → 齿顶弧 → 上半顶 → 上半根
        for (const auto& p : lower) outPoints.push_back(p);                 // 根→顶
        outPoints.push_back(Vec3(ra * std::cos(-offset), ra * std::sin(-offset), 0.0));
        outPoints.push_back(Vec3(ra * std::cos(offset),  ra * std::sin(offset),  0.0));
        for (auto it = upper.rbegin(); it != upper.rend(); ++it) outPoints.push_back(*it); // 顶→根
        return true;
    }

    // 整齿轮闭合外轮廓：逐齿 CCW 拼接
    const int z = teeth;
    for (int k = 0; k < z; ++k) {
        const double ca = k * step;                     // 本齿中心角
        const double c = std::cos(ca), s = std::sin(ca);
        // 上半齿廓：根(rf) → 顶(ra)（局部角度 +offset 递减）
        for (const auto& p : upper) {
            outPoints.push_back(Vec3(p.x * c - p.y * s, p.x * s + p.y * c, 0.0));
        }
        // 齿顶弧：上半顶 → 下半顶（半径 ra，跨越 [+offset, -offset]）
        {
            const int M = 6;
            for (int i = 1; i < M; ++i) {
                const double a = offset - 2.0 * offset * (double)i / M; // +offset → -offset
                outPoints.push_back(Vec3(ra * std::cos(a + ca), ra * std::sin(a + ca), 0.0));
            }
        }
        // 下半齿廓：顶(ra) → 根(rf)（镜像，局部角度负，半径升序即顶→根）
        for (const auto& p : lower) {
            outPoints.push_back(Vec3(p.x * c - p.y * s, p.x * s + p.y * c, 0.0));
        }
        // 齿根弧：本齿下半根 → 下一齿上半根（半径 rf，CCW 填补齿槽）
        {
            const double a0 = -offset + ca;             // 本齿下半根角
            const double a1 = offset + (k + 1) * step;  // 下一齿上半根角
            const int M = 6;
            for (int i = 1; i < M; ++i) {
                const double a = a0 + (a1 - a0) * (double)i / M;
                outPoints.push_back(Vec3(rf * std::cos(a), rf * std::sin(a), 0.0));
            }
        }
    }
    return true;
}

} // namespace SketchCAD
