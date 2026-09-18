#include "EllipseModes.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {
namespace ellipse {

namespace {

inline double sq(double x) { return x * x; }

// 将点 p 变换到椭圆局部坐标系（中心 c，长轴旋转 rotDeg）
inline Vec3 toLocal(const Vec3& p, const Vec3& c, double rotDeg) {
    const double a = math::deg2rad(rotDeg);
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Vec3(dx * std::cos(a) + dy * std::sin(a),
                -dx * std::sin(a) + dy * std::cos(a), 0);
}

inline Vec3 fromLocal(const Vec3& q, const Vec3& c, double rotDeg) {
    const double a = math::deg2rad(rotDeg);
    return Vec3(c.x + q.x * std::cos(a) - q.y * std::sin(a),
                c.y + q.x * std::sin(a) + q.y * std::cos(a), 0);
}

// 最小二乘解线性方程 A x = b（A 行数 >= 列数）
bool solveLeastSquares2x2(const double A[4][2], const double b[4], int n,
                          double out[2]) {
    double AtA[2][2] = {{0, 0}, {0, 0}};
    double Atb[2] = {0, 0};
    for (int i = 0; i < n; ++i) {
        AtA[0][0] += A[i][0] * A[i][0];
        AtA[0][1] += A[i][0] * A[i][1];
        AtA[1][1] += A[i][1] * A[i][1];
        Atb[0] += A[i][0] * b[i];
        Atb[1] += A[i][1] * b[i];
    }
    AtA[1][0] = AtA[0][1];
    const double det = AtA[0][0] * AtA[1][1] - AtA[0][1] * AtA[1][0];
    if (std::fabs(det) < 1e-12) return false;
    out[0] = (Atb[0] * AtA[1][1] - Atb[1] * AtA[0][1]) / det;
    out[1] = (Atb[1] * AtA[0][0] - Atb[0] * AtA[1][0]) / det;
    return true;
}

// 统一表示：保证 radius >= radiusY（长半轴在前），旋转角相应 +90°。
// 这两组参数描述的是同一个椭圆，统一后 sizeDesc / 控制点 / 属性面板的语义才稳定，
// 否则拟合结果的 a、b 可能因 FFT/正负号习惯互换，让用户看到「长轴 5 / 短轴 10」这种别扭读法。
inline void normalizeAxes(Result& r) {
    if (r.radiusY > r.radius) {
        std::swap(r.radius, r.radiusY);
        r.rotation += 90.0;
        r.rotation = std::fmod(r.rotation, 360.0);
        if (r.rotation < 0.0) r.rotation += 360.0;
    }
}

// 数值拟合：中心已知，三个点求旋转椭圆 (a, b, rot)
Result fitCenter3Points(const Vec3& c, const std::vector<Vec3>& pts) {
    Result r;
    if (pts.size() < 3) { r.msg = QStringLiteral("需要中心加三个点"); return r; }

    // 把三个点转为中心相对的点，计算极角和距离
    struct Polar { double ang, rad; };
    std::vector<Polar> pol;
    for (const auto& p : pts) {
        const Vec3 d = p - c;
        pol.push_back({std::atan2(d.y, d.x), d.len2D()});
    }

    // 在 rot 上搜索，最小化 sum_i (r_i^2 * (cos^2(phi_i-rot)/a^2 + sin^2(phi_i-rot)/b^2) - 1)^2
    // 对固定 rot，关于 u=1/a^2, v=1/b^2 是线性最小二乘
    double bestErr = 1e18;
    double bestRot = 0, bestU = 0, bestV = 0;

    for (int k = 0; k < 360; ++k) {
        const double rot = k * M_PI / 180.0;
        double A[3][2], bb[3];
        for (int i = 0; i < 3; ++i) {
            const double phi = pol[i].ang - rot;
            const double rr = pol[i].rad;
            A[i][0] = rr * rr * sq(std::cos(phi));
            A[i][1] = rr * rr * sq(std::sin(phi));
            bb[i] = 1.0;
        }
        double uv[2];
        if (!solveLeastSquares2x2(A, bb, 3, uv)) continue;
        if (uv[0] <= 1e-12 || uv[1] <= 1e-12) continue;  // 不是椭圆
        double err = 0;
        for (int i = 0; i < 3; ++i) {
            const double phi = pol[i].ang - rot;
            const double rr = pol[i].rad;
            const double f = rr * rr * (uv[0] * sq(std::cos(phi)) + uv[1] * sq(std::sin(phi))) - 1.0;
            err += f * f;
        }
        if (err < bestErr) { bestErr = err; bestRot = rot; bestU = uv[0]; bestV = uv[1]; }
    }

    // 精细搜索
    double step = 1.0 * M_PI / 180.0;
    for (int iter = 0; iter < 6; ++iter) {
        double base = bestRot;
        for (double d = -step; d <= step + 1e-12; d += step / 10.0) {
            const double rot = base + d;
            double A[3][2], bb[3];
            for (int i = 0; i < 3; ++i) {
                const double phi = pol[i].ang - rot;
                const double rr = pol[i].rad;
                A[i][0] = rr * rr * sq(std::cos(phi));
                A[i][1] = rr * rr * sq(std::sin(phi));
                bb[i] = 1.0;
            }
            double uv[2];
            if (!solveLeastSquares2x2(A, bb, 3, uv)) continue;
            if (uv[0] <= 1e-12 || uv[1] <= 1e-12) continue;
            double err = 0;
            for (int i = 0; i < 3; ++i) {
                const double phi = pol[i].ang - rot;
                const double rr = pol[i].rad;
                const double f = rr * rr * (uv[0] * sq(std::cos(phi)) + uv[1] * sq(std::sin(phi))) - 1.0;
                err += f * f;
            }
            if (err < bestErr) { bestErr = err; bestRot = rot; bestU = uv[0]; bestV = uv[1]; }
        }
        step *= 0.2;
    }

    if (bestErr >= 1e18) { r.msg = QStringLiteral("无法拟合旋转椭圆"); return r; }
    r.valid = true;
    r.center = c;
    r.radius = 1.0 / std::sqrt(bestU);
    r.radiusY = 1.0 / std::sqrt(bestV);
    r.rotation = math::rad2deg(bestRot);
    normalizeAxes(r);
    return r;
}

} // anonymous namespace

// ============================ 1. 中心 + 椭圆上一点（1 Point） ============================

Result onePoint(const Vec3& c, const Vec3& p) {
    Result r;
    const Vec3 d = p - c;
    const double a = d.len2D();
    if (a < 1e-6) { r.msg = QStringLiteral("长半轴为 0"); return r; }
    r.valid = true;
    r.center = c;
    r.radius = a;
    r.radiusY = a * 0.5;            // 默认短半轴为长半轴的一半
    r.rotation = d.angleDeg();
    normalizeAxes(r);
    return r;
}

// ============================ 2. 中心 + 长轴端点 + 椭圆上一点（Axis） ============================

Result axis(const Vec3& c, const Vec3& axisEnd, const Vec3& passPt) {
    Result r;
    const Vec3 maj = axisEnd - c;
    const double a = maj.len2D();
    if (a < 1e-6) { r.msg = QStringLiteral("长半轴为 0"); return r; }
    const double rot = maj.angleDeg();
    const Vec3 q = toLocal(passPt, c, rot);   // 局部坐标
    // 标准椭圆方程：u^2/a^2 + v^2/b^2 = 1
    const double u = q.x, v = q.y;
    const double ratio = std::fabs(u / a);
    if (std::fabs(v) < 1e-6) { r.msg = QStringLiteral("第三点位于长轴上，无法确定短半轴"); return r; }
    if (ratio >= 1.0 - 1e-9) {
        // 投影超出长轴范围：退化为用点到长轴的距离作为短半轴
        r.valid = true;
        r.center = c; r.radius = a; r.rotation = rot;
        r.radiusY = std::fabs(v);
        normalizeAxes(r);
        return r;
    }
    const double b = std::fabs(v) / std::sqrt(1.0 - ratio * ratio);
    r.valid = true;
    r.center = c; r.radius = a; r.radiusY = std::max(b, 1e-6); r.rotation = rot;
    normalizeAxes(r);
    return r;
}

// ============================ 3. 两焦点 + 椭圆上一点（Foci Point） ============================

Result fociPoint(const Vec3& f1, const Vec3& f2, const Vec3& p) {
    Result r;
    const double d1 = p.distTo(f1), d2 = p.distTo(f2);
    const double twoA = d1 + d2;
    const double focal = f1.distTo(f2);
    if (twoA < 1e-6) { r.msg = QStringLiteral("长轴长度为 0"); return r; }
    if (twoA <= focal + 1e-6) {
        r.msg = QStringLiteral("两焦点距离之和不大于焦距，无法形成椭圆");
        return r;
    }
    const double a = twoA * 0.5;
    const double c_dist = focal * 0.5;
    const double b = std::sqrt(std::max(0.0, a * a - c_dist * c_dist));
    r.valid = true;
    r.center = (f1 + f2) * 0.5;
    r.radius = a;
    r.radiusY = b;
    r.rotation = (f2 - f1).angleDeg();
    normalizeAxes(r);
    return r;
}

// ============================ 4. 四个点拟合轴对齐椭圆（4 Point） ============================

Result fourPoint(const std::vector<Vec3>& pts) {
    Result r;
    const int n = (int)pts.size();
    if (n < 4) { r.msg = QStringLiteral("需要四个点"); return r; }

    // 注意：不能像早期实现那样把「四点质心」直接当作椭圆中心——
    // 那只在点绕两轴对称分布时才成立。四点一共给出 4 个方程，未知量却是
    // (cx, cy, u, v) 四个，必须中心与轴长一起解，否则像取 45°/135°/225°/315°
    // 这种斜对称点集会让系数矩阵奇异（四行方程完全一样），最小二乘退化后
    // 吐出一个根本不通过这四点的假椭圆。这里改为：外层在中心平面上搜索，
    // 内层给定中心后对 u=1/a², v=1/b² 做线性最小二乘，用残差驱动搜索。
    double x0 = pts[0].x, x1 = pts[0].x, y0 = pts[0].y, y1 = pts[0].y;
    for (const auto& p : pts) {
        x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
    }
    const double span = std::max(x1 - x0, y1 - y0);
    if (span < 1e-6) { r.msg = QStringLiteral("点过于集中，无法确定椭圆"); return r; }

    // 给定中心时的线性拟合：返回残差平方和；不可解（奇异/非椭圆）返回 -1
    auto fitAt = [&](double cx, double cy, double& u, double& v) -> double {
        double A00 = 0, A01 = 0, A11 = 0, b0 = 0, b1 = 0;
        for (const auto& p : pts) {
            const double dx = p.x - cx, dy = p.y - cy;
            const double px = dx * dx,  py = dy * dy;
            A00 += px * px; A01 += px * py; A11 += py * py;
            b0   += px;     b1   += py;
        }
        // 相对判据：行列式相对矩阵规模可忽略时视为奇异（绝对阈值 1e-12 会漏判大规模坐标）
        const double det = A00 * A11 - A01 * A01;
        const double scale = A00 * A11 + A01 * A01;
        if (std::fabs(det) < 1e-12 * (scale + 1e-30)) return -1.0;
        u = (b0 * A11 - b1 * A01) / det;
        v = (b1 * A00 - b0 * A01) / det;
        if (u <= 1e-12 || v <= 1e-12) return -1.0;      // 退化为双曲线/抛物线
        double err = 0;
        for (const auto& p : pts) {
            const double dx = p.x - cx, dy = p.y - cy;
            const double f = u * dx * dx + v * dy * dy - 1.0;
            err += f * f;
        }
        return err;
    };

    // 以包围盒中心为起点做模式搜索（pattern search）：每次在 ±step 网格里找更优点，
    // 找到就搬过去，找不到就把步长折半，直到步长足够小。
    double bx = (x0 + x1) * 0.5, by = (y0 + y1) * 0.5;
    double bu = 0, bv = 0;
    double err = fitAt(bx, by, bu, bv);
    // 起点退化时（概率极低）在其邻域找一个可解点作为起点
    if (err < 0.0) {
        bool seeded = false;
        for (double k = 0.05; !seeded && k <= 1.0; k += 0.05) {
            for (const double sx : {1.0, -1.0}) {
                for (const double sy : {1.0, -1.0}) {
                    const double cx = bx + sx * k * span, cy = by + sy * k * span;
                    double u = 0, v = 0;
                    const double e = fitAt(cx, cy, u, v);
                    if (e >= 0.0) { bx = cx; by = cy; bu = u; bv = v; err = e; seeded = true; break; }
                }
                if (seeded) break;
            }
        }
        if (!seeded) { r.msg = QStringLiteral("无法拟合椭圆：方程恒退化"); return r; }
    }

    double step = span;
    int guard = 0;                       // 迭代上限，防止病态输入下搜索不收敛而空转
    while (step > span * 1e-10 && guard < 200) {
        ++guard;
        bool improved = false;
        const double sub = step / 3.0;
        for (int i = -3; i <= 3 && !improved; ++i) {
            for (int j = -3; j <= 3 && !improved; ++j) {
                if (i == 0 && j == 0) continue;
                const double cx = bx + i * sub, cy = by + j * sub;
                double u = 0, v = 0;
                const double e = fitAt(cx, cy, u, v);
                if (e >= 0.0 && e < err - 1e-30) {
                    err = e; bx = cx; by = cy; bu = u; bv = v; improved = true;
                }
            }
        }
        if (!improved) step *= 0.5;
    }

    // 四点四自由度，正常情况下应能精确过点；残差过大说明这组点不存在轴对齐椭圆解
    double fbu = 0, fbv = 0;
    if (fitAt(bx, by, fbu, fbv) < 0.0) {
        // 收敛点本身仍然奇异 —— 典型的欠定布局：四点凑成一个轴对齐矩形时，
        // 四个方程退化成一模一样的一行，过它们有无穷多个轴对齐椭圆。
        // 此时必须明确报错，而不是随便挑一个，否则用户会拿到一条「看着像、实为错」的椭圆。
        r.msg = QStringLiteral("四点构成退化布局（如对称矩形），无法唯一确定椭圆");
        return r;
    }
    if (err > 1e-12) {
        r.msg = QStringLiteral("无法拟合椭圆：四点不共轴对齐椭圆");
        return r;
    }
    r.valid = true;
    r.center = Vec3(bx, by, 0);
    r.radius = 1.0 / std::sqrt(bu);
    r.radiusY = 1.0 / std::sqrt(bv);
    r.rotation = 0.0;
    normalizeAxes(r);
    return r;
}

// ============================ 5. 中心 + 三个点拟合旋转椭圆（Center and 3 Points） ============================

Result center3Points(const Vec3& c, const std::vector<Vec3>& pts) {
    return fitCenter3Points(c, pts);
}

// ============================ 6. 内切于矩形/多边形（Inscribed） ============================

Result inscribed(const Element& e) {
    Result r;
    if (e.type != ElementType::Rectangle && e.type != ElementType::Polygon) {
        r.msg = QStringLiteral("请选择矩形或多边形作为外框");
        return r;
    }
    std::vector<Vec3> corners;
    if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
        const double x0 = e.pts[0].x, y0 = e.pts[0].y;
        const double x1 = e.pts[1].x, y1 = e.pts[1].y;
        corners = {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)};
    } else if (e.type == ElementType::Polygon) {
        corners = e.pts;
    }
    if (corners.size() < 3) { r.msg = QStringLiteral("外框顶点数不足"); return r; }
    double x0 = corners[0].x, x1 = corners[0].x;
    double y0 = corners[0].y, y1 = corners[0].y;
    for (const auto& p : corners) {
        x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
    }
    const double w = x1 - x0, h = y1 - y0;
    if (w < 1e-6 || h < 1e-6) { r.msg = QStringLiteral("外框面积过小"); return r; }
    r.valid = true;
    r.center = Vec3((x0 + x1) * 0.5, (y0 + y1) * 0.5, 0);
    r.radius = w * 0.5;
    r.radiusY = h * 0.5;
    r.rotation = 0.0;
    normalizeAxes(r);
    return r;
}

// ============================ 主入口 ============================

Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double /*param*/) {
    Result r;

    auto getRef = [&](int idx) -> const Element* {
        if (idx < 0 || idx >= (int)refs.size()) return nullptr;
        for (const auto& e : elems) if (e.id == refs[idx]) return &e;
        return nullptr;
    };

    switch (m) {
    case Mode::OnePoint: {
        if (pts.size() < 1) return r;
        Vec3 p = pts.size() >= 2 ? pts[1] : cur;
        return onePoint(pts[0], p);
    }
    case Mode::Axis: {
        if (pts.size() < 2) return r;
        return axis(pts[0], pts[1], cur);
    }
    case Mode::FociPoint: {
        if (pts.size() < 2) return r;
        return fociPoint(pts[0], pts[1], cur);
    }
    case Mode::FourPoint: {
        auto pp = pts;
        if (pp.size() < 4) pp.push_back(cur);
        return fourPoint(pp);
    }
    case Mode::Center3Points: {
        if (pts.size() < 1) return r;
        auto pp = pts;
        pp.erase(pp.begin()); // 第一个是中心
        while (pp.size() < 3) pp.push_back(cur);
        return center3Points(pts[0], pp);
    }
    case Mode::Inscribed: {
        const Element* e = getRef(0);
        if (!e) { r.msg = QStringLiteral("请选择矩形或多边形"); return r; }
        return inscribed(*e);
    }
    }
    return r;
}

// ============================ 工具映射 ============================

Mode modeFromString(const QString& s) {
    if (s == "ellipse_1pt")          return Mode::OnePoint;
    if (s == "ellipse_axis")         return Mode::Axis;
    if (s == "ellipse_foci")          return Mode::FociPoint;
    if (s == "ellipse_4p")            return Mode::FourPoint;
    if (s == "ellipse_c3p")           return Mode::Center3Points;
    if (s == "ellipse_inscribed")     return Mode::Inscribed;
    return Mode::OnePoint;
}

QString stringFromMode(Mode m) {
    switch (m) {
    case Mode::OnePoint:      return "ellipse_1pt";
    case Mode::Axis:          return "ellipse_axis";
    case Mode::FociPoint:     return "ellipse_foci";
    case Mode::FourPoint:     return "ellipse_4p";
    case Mode::Center3Points: return "ellipse_c3p";
    case Mode::Inscribed:     return "ellipse_inscribed";
    }
    return "ellipse_1pt";
}

ReqCount requirements(Mode m) {
    switch (m) {
    case Mode::OnePoint:      return {0, 1, false};
    case Mode::Axis:          return {0, 2, false};
    case Mode::FociPoint:     return {0, 2, false};
    case Mode::FourPoint:     return {0, 4, false};
    case Mode::Center3Points: return {0, 4, false}; // 1 中心 + 3 点
    case Mode::Inscribed:     return {1, 0, false};
    }
    return {0, 0, false};
}

std::vector<ElementType> allowedTypes(Mode m) {
    switch (m) {
    case Mode::Inscribed:
        return {ElementType::Rectangle, ElementType::Polygon};
    default:
        return {};
    }
}

} // namespace ellipse
} // namespace SketchCAD
