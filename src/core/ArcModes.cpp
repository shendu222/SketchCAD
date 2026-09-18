#include "ArcModes.h"
#include "Geometry.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {
namespace arc {

// ============================ 内部工具函数 ============================

namespace {

// 计算扫掠方向：从 startAng 到 endAng，ccw=true 取逆时针（正方向）
// 返回值范围 (-360, 360)，零角度时给默认全圆
double sweepTowards(double s, double e, bool ccw) {
    double d = geo::wrapDeg360(e - s);
    if (!ccw) d -= 360.0;
    if (std::fabs(d) < 1e-9) d = ccw ? 360.0 : -360.0;
    return d;
}

// 判断点 mid 是否落在从 s 到 e 的逆时针弧上
bool midOnArcCCW(double mid, double s, double e) {
    double sweep = sweepTowards(s, e, true);
    double rel = geo::wrapDeg360(mid - s);
    if (sweep > 0) return rel >= -1e-6 && rel <= sweep + 1e-6;
    // 全圆
    return true;
}

} // anonymous namespace

// ==================== 1. 圆心 + 点 + 角度扫掠 ====================
// 与现有 finalizeArc 行为完全一致：三点定圆心/半径，
// 第三点决定终止角（通过 arcTracking_ 累计扫掠量）。

Result centerPointAngles(const Vec3& c, const Vec3& s, const Vec3& e) {
    Result r;
    const double rad = c.distTo(s);
    if (rad < 1e-6) { r.msg = QStringLiteral("圆弧：半径为 0"); return r; }
    r.valid = true;
    r.center = c;
    r.radius = rad;
    r.startAngle = (s - c).angleDeg();
    // 终止角：直接用光标点的角度（调用方负责解缠）
    r.endAngle = (e - c).angleDeg();
    return r;
}

// ==================== 2. 圆心 + 点 + 弦长 ====================
// 沿圆周到距离=chord处取终点

Result centerPointChord(const Vec3& c, const Vec3& p, double chord, const Vec3& cur) {
    (void)cur;
    Result r;
    const double rad = c.distTo(p);
    if (rad < 1e-6) { r.msg = QStringLiteral("圆弧：半径为 0"); return r; }
    if (chord < 1e-9) { r.msg = QStringLiteral("弦长无效"); return r; }
    if (chord > 2.0 * M_PI * rad) {
        r.msg = QStringLiteral("弦长 %1 超过圆周长 %2").arg(chord, 0, 'f', 2).arg(2.0 * M_PI * rad, 0, 'f', 2);
        return r;
    }

    // 弧长 → 圆心角（rad）
    double centralRad = chord / rad;
    // 限制在合理范围
    if (centralRad > 2.0 * M_PI - 1e-6) centralRad = 2.0 * M_PI - 1e-6;

    double startAng = (p - c).angleDeg();
    double sweepDeg = math::rad2deg(centralRad);

    r.valid = true;
    r.center = c;
    r.radius = rad;
    r.startAngle = startAng;
    r.endAngle = startAng + sweepDeg;   // 默认逆时针
    return r;
}

// ==================== 3. 圆心 + 半径 + 扫掠角（参数条）====================

Result centerPointArcAngle(const Vec3& c, double radius, const Vec3& cur, double sweepDeg) {
    Result r;
    if (radius < 1e-6) { r.msg = QStringLiteral("半径无效，请在参数输入条输入半径"); return r; }

    double startAng = (cur - c).angleDeg();

    r.valid = true;
    r.center = c;
    r.radius = radius;
    r.startAngle = startAng;
    r.endAngle = startAng + (std::fabs(sweepDeg) < 1e-6 ? 90.0 : sweepDeg);
    return r;
}

// ==================== 4. 三点圆弧 ====================
// 过三点作弧：先求外接圆，再判定中间点在哪侧以确定扫掠方向。

Result threePoint(const Vec3& s, const Vec3& mid, const Vec3& e) {
    Result r;
    Vec3 cntr; double rad = 0;
    if (!geo::circleFrom3Points(s, mid, e, cntr, rad)) {
        r.msg = QStringLiteral("三点圆弧：三点共线或重合，无法定弧");
        return r;
    }

    const double as = (s   - cntr).angleDeg();
    const double am = (mid - cntr).angleDeg();
    const double ae = (e   - cntr).angleDeg();

    // 判断中间点落在 s→e 逆时针弧还是顺时针弧上
    bool ccw = midOnArcCCW(am, as, ae);
    // 如果中间点恰好在两端角的平分线附近，用距离辅助判断
    if (!ccw && !midOnArcCCW(am, ae, as)) {
        // 中间点不在任一侧的短弧上——说明是优弧情况
        // 取劣弧方向（更短的弧）
        double cwSweep = std::fabs(sweepTowards(as, ae, false));
        double ccwSweep = std::fabs(sweepTowards(as, ae, true));
        ccw = (ccwSweep <= cwSweep);
    }

    r.valid = true;
    r.center = cntr;
    r.radius = rad;
    r.startAngle = as;
    r.endAngle = as + sweepTowards(as, ae, ccw);
    return r;
}

// ==================== 5. 起终点 + 夹角 ====================
// 已知弦和圆心角，求半径和弦高方向的圆心位置。

Result twoPointAngle(const Vec3& s, const Vec3& e, double angleDeg, const Vec3& cur) {
    Result r;
    const double d = s.distTo(e);
    if (d < 1e-9) { r.msg = QStringLiteral("起点与终点重合"); return r; }
    if (angleDeg < 1e-6 || angleDeg > 360.0 - 1e-6) {
        // 特殊处理：接近 0 或 360 时退化
        if (angleDeg < 1e-6) { r.msg = QStringLiteral("夹角过小"); return r; }
        // 接近 360° → 近似半圆
        angleDeg = 359.9;
    }

    // 弦长 d，圆心角 θ → 半径 R = d / (2*sin(θ/2))
    const double halfAng = math::deg2rad(angleDeg * 0.5);
    if (std::fabs(std::sin(halfAng)) < 1e-9) {
        r.msg = QStringLiteral("夹角导致除零（180° 的倍数附近）");
        return r;
    }
    const double R = d * 0.5 / std::sin(halfAng);

    // 弦中点到圆心的距离（矢高方向）
    const double h = R * std::cos(halfAng);   // 注意：这里 h 是中点到圆心的距离
    // 实际矢高 sagitta = R - R*cos(θ/2) = R*(1-cos(θ/2))

    // 弦的方向和中垂线
    const Vec3 dir((e.x - s.x) / d, (e.y - s.y) / d, 0);
    const Vec3 perp(-dir.y, dir.x, 0);   // 左法线（逆时针 90°）
    const Vec3 mid((s.x + e.x) * 0.5, (s.y + e.y) * 0.5, 0);

    // 两个候选圆心（弦两侧）
    const Vec3 c1(mid.x + perp.x * h, mid.y + perp.y * h, 0);
    const Vec3 c2(mid.x - perp.x * h, mid.y - perp.y * h, 0);
    const Vec3 cntr = (cur.distTo(c1) <= cur.distTo(c2)) ? c1 : c2;

    r.valid = true;
    r.center = cntr;
    r.radius = R;
    r.startAngle = (s - cntr).angleDeg();
    // 扫掠方向：取光标所在侧为正向
    const bool ccw = (cur.distTo(c1) <= cur.distTo(c2));
    r.endAngle = r.startAngle + sweepTowards(r.startAngle, (e - cntr).angleDeg(), ccw);

    // 备选解
    r.altCenters.push_back((cntr.distTo(c1) < 1e-6) ? c2 : c1);
    return r;
}

// ==================== 6. 起终点 + 半径 ====================
// 两解：圆心在弦中垂线上，距中点 sqrt(R² - (d/2)²)

Result twoPointRadius(const Vec3& s, const Vec3& e, double radius, const Vec3& cur) {
    Result r;
    const double d = s.distTo(e);
    if (d < 1e-9) { r.msg = QStringLiteral("起点与终点重合"); return r; }
    if (radius < d * 0.5 - 1e-9) {
        r.msg = QStringLiteral("半径过小：需 >= 弦长的一半 (%1 mm)").arg(d * 0.5, 0, 'f', 2);
        return r;
    }

    const Vec3 mid((s.x + e.x) * 0.5, (s.y + e.y) * 0.5, 0);
    const double halfChord = d * 0.5;
    const double sagitta = std::sqrt(std::max(0.0, radius * radius - halfChord * halfChord));

    // 弦方向单位向量及左法线
    const double dx = e.x - s.x, dy = e.y - s.y;
    const double len = std::hypot(dx, dy);
    const Vec3 n(-dy / len, dx / len, 0);

    const Vec3 c1(mid.x + n.x * sagitta, mid.y + n.y * sagitta, 0);
    const Vec3 c2(mid.x - n.x * sagitta, mid.y - n.y * sagitta, 0);
    const Vec3 cntr = (cur.distTo(c1) <= cur.distTo(c2)) ? c1 : c2;

    r.valid = true;
    r.center = cntr;
    r.radius = radius;
    r.startAngle = (s - cntr).angleDeg();
    const bool ccw = ((e - cntr).angleDeg() - r.startAngle) > 0;
    r.endAngle = r.startAngle + sweepTowards(r.startAngle, (e - cntr).angleDeg(), ccw);

    r.altCenters.push_back((cntr.distTo(c1) < 1e-6) ? c2 : c1);
    return r;
}

// ==================== 7. 起终点 + 弦长 ====================
// 内部转为等价半径后复用 twoPointRadius

Result twoPointLength(const Vec3& s, const Vec3& e, double length, const Vec3& cur) {
    Result r;
    const double d = s.distTo(e);
    if (d < 1e-9) { r.msg = QStringLiteral("起点与终点重合"); return r; }
    if (length < d - 1e-9) {
        r.msg = QStringLiteral("弦长 %1 小于两点距离 %2").arg(length, 0, 'f', 2).arg(d, 0, 'f', 2);
        return r;
    }
    if (length < d * 0.5) {
        r.msg = QStringLiteral("弦长不足：需 >= 两点距离的一半 (%1)").arg(d * 0.5, 0, 'f', 2);
        return r;
    }

    // 弦长 L，弦半长 d/2 → 矢高 h = sqrt((L/2)^2 - (d/2)^2)
    // 但这不够——需要反推半径。已知弦长 L 和弦 d：
    // 若 L 为弧长：R = L / θ, d/2 = R*sin(θ/2) → 需数值解 θ
    // 若 L 为弦长（即直线距离）：L == d，无意义
    //
    // CAD 惯例："2 Points, Length" 通常指弧长（arc length）。
    // 简化实现：将 length 视为弧长，用迭代法求半径。
    //
    // 更实用的简化：把"Length"理解为弦长的约束条件下的弧长，
    // 即用户想画一条指定弧长的弧。用二分法搜索圆心角。

    // 二分搜索圆心角 θ ∈ (0°, 360°)，使得 arc_length = R*θ_rad = length
    // 其中 R = d/(2*sin(θ/2))
    // 即: d/(2*sin(θ/2)) * θ_rad = length
    // f(θ) = d*θ_rad/(2*sin(θ/2)) - length = 0

    double lo = 1e-3, hi = math::deg2rad(359.9);
    for (int iter = 0; iter < 60; ++iter) {
        double mid = (lo + hi) * 0.5;
        double sinHalf = std::sin(mid * 0.5);
        if (std::fabs(sinHalf) < 1e-12) { hi = mid; continue; }
        double f = d * mid / (2.0 * sinHalf) - length;
        if (std::fabs(f) < 1e-6) break;
        if (f > 0) hi = mid; else lo = mid;
    }

    double theta = (lo + hi) * 0.5;
    double sinHalf = std::sin(theta * 0.5);
    if (std::fabs(sinHalf) < 1e-12) {
        r.msg = QStringLiteral("无法计算对应弧长的圆弧"); return r;
    }
    double R = d / (2.0 * sinHalf);

    return twoPointRadius(s, e, R, cur);
}

// ==================== 8. 起终点 + 弦高点（凸度/sagitta）====================
// 弦高点即弧上离弦最远的点（凸度控制点）

Result twoPointBulge(const Vec3& s, const Vec3& e, const Vec3& bulgePt) {
    Result r;
    const double d = s.distTo(e);
    if (d < 1e-9) { r.msg = QStringLiteral("起点与终点重合"); return r; }

    const Vec3 mid((s.x + e.x) * 0.5, (s.y + e.y) * 0.5, 0);
    const double bulgeH = math::signedDistLine(bulgePt, s, e);   // 到弦的有符号距离

    if (std::fabs(bulgeH) < 1e-6) {
        r.msg = QStringLiteral("弦高点在弦上，无法构成圆弧"); return r;
    }

    // 几何关系：R² = (d/2)² + (R - |bulgeH|)²
    // 解得: R = ((d/2)² + bulgeH²) / (2*|bulgeH|)
    const double halfD = d * 0.5;
    const double absH = std::fabs(bulgeH);
    const double R = (halfD * halfD + bulgeH * bulgeH) / (2.0 * absH);

    // 弦方向及法线
    const double dx = e.x - s.x, dy = e.y - s.y;
    const double len = std::hypot(dx, dy);
    const Vec3 n(-dy / len, dx / len, 0);   // 左法线

    // 圆心在弦高点相对于弦的同一侧（凸向）
    const int sign = (bulgeH > 0) ? 1 : -1;
    const Vec3 cntr(mid.x + n.x * (sign * (R - absH)),
                     mid.y + n.y * (sign * (R - absH)), 0);

    r.valid = true;
    r.center = cntr;
    r.radius = R;
    r.startAngle = (s - cntr).angleDeg();
    const bool ccw = (bulgeH > 0);   // 凸向决定扫掠方向
    r.endAngle = r.startAngle + sweepTowards(r.startAngle, (e - cntr).angleDeg(), ccw);
    return r;
}

// ==================== 9. 相切延续弧 ====================
// 从参考图元的端点出发，保持切向，到目标点结束。

Result tangentContinue(const Element& ref, const Vec3& from, const Vec3& to) {
    Result r;

    // 确定起始点和起始切向
    Vec3 startPos;
    Vec3 tangentDir;   // 单位切向

    if (ref.type == ElementType::Line) {
        if (ref.pts.size() < 2) { r.msg = QStringLiteral("参考直线数据不完整"); return r; }
        Vec3 a = ref.pts[0], b = ref.pts[1];
        // from 应靠近 a 或 b 之一
        if (from.distTo(a) < from.distTo(b)) {
            startPos = a;
            tangentDir = (b - a).normalized();
        } else {
            startPos = b;
            tangentDir = (a - b).normalized();
        }
    } else if (ref.type == ElementType::Arc || ref.type == ElementType::Circle) {
        if (ref.pts.empty()) { r.msg = QStringLiteral("参考圆/弧数据不完整"); return r; }
        Vec3 c = ref.pts[0];
        // 判断 from 靠近起点还是终点
        double angFrom = (from - c).angleDeg();
        double angStart = ref.startAngle;
        double angEnd   = ref.endAngle;

        // 归一化到同一起点比较
        double fromRel = geo::wrapDeg360(angFrom - angStart);
        double endRel  = geo::wrapDeg360(angEnd - angStart);
        // 如果 from 更靠近起点
        bool nearStart = (std::fabs(fromRel) < std::fabs(endRel) ||
                          std::fabs(fromRel - endRel) > std::fabs(endRel) * 0.5);

        if (nearStart) {
            // 在起点处：切向垂直于半径，方向由扫掠方向决定
            double cs = std::cos(math::deg2rad(angStart));
            double sn = std::sin(math::deg2rad(angStart));
            startPos = Vec3(c.x + ref.radius * cs, c.y + ref.radius * sn, 0);
            double sweepDir = (angEnd > angStart) ? 1.0 : -1.0;
            tangentDir = Vec3(-sn * sweepDir, cs * sweepDir, 0);
        } else {
            // 在终点处
            double ce = std::cos(math::deg2rad(angEnd));
            double se = std::sin(math::deg2rad(angEnd));
            startPos = Vec3(c.x + ref.radius * ce, c.y + ref.radius * se, 0);
            double sweepDir = (angEnd > angStart) ? 1.0 : -1.0;
            tangentDir = Vec3(-se * sweepDir, ce * sweepDir, 0);
        }
    } else {
        r.msg = QStringLiteral("相切延续弧仅支持直线、圆弧或圆作为参考图元");
        return r;
    }

    // 圆心：从 startPos 沿切向垂线走，使得圆弧经过 to
    // 圆心 = startPos + R * (tangentDir 旋转 ±90°)
    // 且 |to - center| = R
    //
    // 设 normal = tangentDir.rotate90CCW()
    // center = startPos + R * normal
    // |to - startPos - R * normal|^2 = R^2
    // 展开: |to-startPos|^2 - 2*R*(to-startPos)·normal + R^2 = R^2
    //      R = |to-startPos|^2 / (2 * (to-startPos)·normal)
    Vec3 normal(-tangentDir.y, tangentDir.x, 0);   // 左法线（tangent 逆时针 90°）
    Vec3 toVec(to.x - startPos.x, to.y - startPos.y, 0);
    double dot = toVec.dot2D(normal);

    if (std::fabs(dot) < 1e-9) {
        r.msg = QStringLiteral("目标点与起点的连线与切向平行，无法构弧"); return r;
    }

    double R = toVec.len2D() * toVec.len2D() / (2.0 * dot);
    if (R < 1e-6) { r.msg = QStringLiteral("计算半径无效"); return r; }

    Vec3 cntr(startPos.x + normal.x * R, startPos.y + normal.y * R, 0);

    r.valid = true;
    r.center = cntr;
    r.radius = R;
    r.startAngle = (startPos - cntr).angleDeg();
    r.endAngle = (to - cntr).angleDeg();   // 不归一化，让调用方决定扫掠
    return r;
}

// ==================== 10. 椭圆弧（轴定义）====================
// 中心 + 长轴端点 + 角度点 → 定义椭圆弧
// 当前系统仅支持圆形图元存储，椭圆弧近似为圆弧（取平均半径或长轴）

Result ellipseArcAxis(const Vec3& center, const Vec3& axisEnd, const Vec3& anglePt) {
    Result r;

    double a = center.distTo(axisEnd);   // 长半轴
    if (a < 1e-6) { r.msg = QStringLiteral("椭圆轴长度为 0"); return r; }

    // 短轴：中心到角度点的投影距离
    Vec3 axisDir((axisEnd.x - center.x) / a, (axisEnd.y - center.y) / a, 0);
    Vec3 toPt(anglePt.x - center.x, anglePt.y - center.y, 0);
    double proj = toPt.dot2D(axisDir);
    Vec3 perpPt(toPt.x - axisDir.x * proj, toPt.y - axisDir.y * proj, 0);
    double b = perpPt.len2D();           // 短半轴

    if (b < 1e-6) b = a;                // 退化为圆弧

    // 用等效圆弧表示（取长轴作为半径，这是 CAD 常见简化）
    // 精确椭圆弧需要扩展 Element 数据结构，此处做合理近似
    r.valid = true;
    r.center = center;
    r.radius = a;                       // 主半径 = 长轴
    r.radiusY = b;                      // 次半径 = 短轴（供未来渲染使用）
    r.startAngle = axisDir.angleDeg();
    r.endAngle = toPt.angleDeg();

    r.msg = QStringLiteral("椭圆弧已创建（当前版本以等效圆弧显示，长短轴比 %1:%2）")
             .arg(a, 0, 'f', 1).arg(b, 0, 'f', 1);
    return r;
}

// ==================== 11. 椭圆弧（一点定义）====================
// 中心 + 长轴端点 + 过点 → 椭圆弧经过该点

Result ellipseArc1Pt(const Vec3& center, const Vec3& axisEnd, const Vec3& passPt) {
    Result r;

    double a = center.distTo(axisEnd);
    if (a < 1e-6) { r.msg = QStringLiteral("椭圆轴长度为 0"); return r; }

    Vec3 axisDir((axisEnd.x - center.x) / a, (axisEnd.y - center.y) / a, 0);
    Vec3 toPt(passPt.x - center.x, passPt.y - center.y, 0);

    // 长轴投影
    double proj = toPt.dot2D(axisDir);
    // 短轴分量
    Vec3 perpVec(toPt.x - axisDir.x * proj, toPt.y - axisDir.y * proj, 0);
    double b = perpVec.len2D();

    if (b < 1e-6) b = a;

    r.valid = true;
    r.center = center;
    r.radius = a;
    r.radiusY = b;
    r.startAngle = axisDir.angleDeg();
    r.endAngle = toPt.angleDeg();

    r.msg = QStringLiteral("椭圆弧(1点)已创建（等效圆弧显示，长短轴比 %1:%2）")
             .arg(a, 0, 'f', 1).arg(b, 0, 'f', 1);
    return r;
}

// ============================ 主入口 ============================

Result compute(Mode m,
               const std::vector<Vec3>& pts,
               const std::vector<int>& refs,
               const Vec3& cur,
               const std::vector<Element>& elems,
               double param) {

    // 获取参考图元指针（与 CircleModes.cpp 同构）
    auto getRef = [&](int idx) -> const Element* {
        if (idx < 0 || idx >= (int)refs.size()) return nullptr;
        for (const auto& e : elems) if (e.id == refs[idx]) return &e;
        return nullptr;
    };

    Result bad;

    switch (m) {
    case Mode::CenterPointAngles: {
        // 兼容现有 "arc" 行为：圆心 + 起点 + 光标(预览)/第三点(确认)
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("圆弧 — 请输入圆心"); return bad; }
        if (pp.size() < 2) return bad;
        return centerPointAngles(pp[0], pp[1], pp.size() >= 3 ? pp[2] : cur);
    }

    case Mode::CenterPointChord: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("圆心+弦 — 请输入圆心与圆周点"); return bad; }
        double chord = (param > 1e-6) ? param : 20.0;   // 默认 20mm 弦长
        return centerPointChord(pp[0], pp[1], chord, cur);
    }

    case Mode::CenterPointArcAngle: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 1) { bad.msg = QStringLiteral("圆心+弧角 — 请输入圆心"); return bad; }
        double rad = (param > 1e-6) ? param : 10.0;
        double sweep = 90.0;   // 默认扫掠角可在参数条修改时覆盖
        // 尝试从 lineParams 获取角度值（由调用方传入 param）
        return centerPointArcAngle(pp[0], rad, cur, sweep);
    }

    case Mode::ThreePoint: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) {
            if (pp.size() == 2) {
                // 不足3点时：用外接圆预览（退化但有用）
                Vec3 c; double cr;
                if (geo::circleFrom3Points(pp[0], pp[1], cur, c, cr)) {
                    Result tr; tr.valid = true; tr.center = c; tr.radius = cr;
                    tr.startAngle = (pp[0] - c).angleDeg();
                    tr.endAngle = (cur - c).angleDeg();
                    return tr;
                }
            }
            bad.msg = QStringLiteral("三点圆弧 — 请继续拾取点"); return bad;
        }
        return threePoint(pp[0], pp[1], pp[2]);
    }

    case Mode::TwoPointAngle: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("起终点+夹角 — 请输入起点"); return bad; }
        double angle = (param > 1e-6) ? param : 90.0;
        return twoPointAngle(pp[0], pp[1], angle, cur);
    }

    case Mode::TwoPointRadius: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("起终点+半径 — 请输入起点"); return bad; }
        double rad = (param > 1e-6) ? param : pp[0].distTo(pp[1]) * 0.8;
        return twoPointRadius(pp[0], pp[1], rad, cur);
    }

    case Mode::TwoPointLength: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("起终点+弦长 — 请输入起点"); return bad; }
        double len = (param > 1e-6) ? param : pp[0].distTo(pp[1]) * 1.2;
        return twoPointLength(pp[0], pp[1], len, cur);
    }

    case Mode::TwoPointBulge: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) { bad.msg = QStringLiteral("起终点+弦高 — 请输入起点、终点和凸度点"); return bad; }
        return twoPointBulge(pp[0], pp[1], pp[2]);
    }

    case Mode::TangentContinue: {
        const Element* ref = getRef(0);
        if (!ref) { bad.msg = QStringLiteral("相切延续弧 — 请选择一个参考图元"); return bad; }
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 2) { bad.msg = QStringLiteral("相切延续弧 — 请输入起点和终点"); return bad; }
        return tangentContinue(*ref, pp[0], pp[1]);
    }

    case Mode::EllipseArcAxis: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) { bad.msg = QStringLiteral("椭圆弧(轴) — 请输入中心、轴端点和角度点"); return bad; }
        return ellipseArcAxis(pp[0], pp[1], pp[2]);
    }

    case Mode::EllipseArc1Pt: {
        std::vector<Vec3> pp = pts;
        pp.push_back(cur);
        if (pp.size() < 3) { bad.msg = QStringLiteral("椭圆弧(1点) — 请输入中心、轴端点和过点"); return bad; }
        return ellipseArc1Pt(pp[0], pp[1], pp[2]);
    }
    }

    bad.msg = QStringLiteral("未知的圆弧模式");
    return bad;
}

// ============================ 工具 ID 映射 ============================

Mode modeFromString(const QString& s) {
    if (s == "arc_3p")          return Mode::ThreePoint;
    if (s == "arc_cpc")         return Mode::CenterPointChord;
    if (s == "arc_cpaa")        return Mode::CenterPointArcAngle;
    if (s == "arc_tpa")         return Mode::TwoPointAngle;
    if (s == "arc_tpr")         return Mode::TwoPointRadius;
    if (s == "arc_tpl")         return Mode::TwoPointLength;
    if (s == "arc_tph")         return Mode::TwoPointBulge;
    if (s == "arc_tan")         return Mode::TangentContinue;
    if (s == "arc_ea_axis")     return Mode::EllipseArcAxis;
    if (s == "arc_ea_1pt")      return Mode::EllipseArc1Pt;
    // 缺省 == "arc"（兼容原有行为）
    return Mode::CenterPointAngles;
}

QString stringFromMode(Mode m) {
    switch (m) {
    case Mode::CenterPointAngles: return "arc";
    case Mode::CenterPointChord:  return "arc_cpc";
    case Mode::CenterPointArcAngle:return "arc_cpaa";
    case Mode::ThreePoint:        return "arc_3p";
    case Mode::TwoPointAngle:     return "arc_tpa";
    case Mode::TwoPointRadius:    return "arc_tpr";
    case Mode::TwoPointLength:    return "arc_tpl";
    case Mode::TwoPointBulge:     return "arc_tph";
    case Mode::TangentContinue:   return "arc_tan";
    case Mode::EllipseArcAxis:   return "arc_ea_axis";
    case Mode::EllipseArc1Pt:    return "arc_ea_1pt";
    }
    return "arc";
}

// ============================ 需求描述 ============================

ReqCount requirements(Mode m) {
    switch (m) {
    case Mode::CenterPointAngles:  return {0, 3, false};    // 圆心 + 起点 + 终点
    case Mode::CenterPointChord:   return {0, 2, true};     // 圆心 + 点；参数=弦长
    case Mode::CenterPointArcAngle:return {0, 1, true};     // 圆心；参数=半径+弧角
    case Mode::ThreePoint:         return {0, 3, false};
    case Mode::TwoPointAngle:      return {0, 2, true};     // 参数=夹角
    case Mode::TwoPointRadius:     return {0, 2, true};     // 参数=半径
    case Mode::TwoPointLength:     return {0, 2, true};     // 参数=弦长
    case Mode::TwoPointBulge:      return {0, 3, false};
    case Mode::TangentContinue:    return {1, 2, false};    // 1个参考图元 + 2个点
    case Mode::EllipseArcAxis:    return {0, 3, false};
    case Mode::EllipseArc1Pt:     return {0, 3, false};
    }
    return {0, 3, false};
}

std::vector<ElementType> allowedTypes(Mode m) {
    switch (m) {
    case Mode::TangentContinue:
        return {ElementType::Line, ElementType::Circle, ElementType::Arc};
    default:
        return {};   // 其余模式不需要参考图元
    }
}

} // namespace arc
} // namespace SketchCAD
