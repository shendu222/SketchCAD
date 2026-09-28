// 图元修改引擎实现 —— 详见 Modify.h 设计说明。
//
// 设计要点：
//   * 所有 make* 工厂函数都只产出 Change 列表，零模型依赖；
//   * applyChanges() 是唯一会触碰 SketchModel 的入口，
//     调用前由命令 / 菜单按需 pushMemento()，Undo 一致性由调用方维护；
//   * 半轴几何（圆 radius / 椭圆 radius+radiusY）按 transform 的局部缩放因子变换；
//   * 多选场景：每个 make* 都遍历整组 src，独立产出 Change；
//     id < 0 的 replacement 表示新增，applyChanges() 会自动 addElement 并填实 ID。

#include "Modify.h"
#include "SketchModel.h"
#include "Geometry.h"
#include "ChamferFillet.h"

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace modify {

// ---- 私有辅助：把 Vec3 绕 center 旋转 angleDeg 度（CCW 为正） ----
static inline Vec3 rotatePt(const Vec3& p, const Vec3& c, double deg) {
    const double a = deg * M_PI / 180.0;
    const double cs = std::cos(a), sn = std::sin(a);
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Vec3(c.x + dx * cs - dy * sn,
                c.y + dx * sn + dy * cs, 0);
}

// ---- 私有辅助：把 Vec3 相对 center 缩放 factor 倍 ----
static inline Vec3 scalePt(const Vec3& p, const Vec3& c, double k) {
    return Vec3(c.x + (p.x - c.x) * k, c.y + (p.y - c.y) * k, 0);
}

// ---- 私有辅助：把 Vec3 沿直线 (p1, p2) 镜像 ----
static inline Vec3 mirrorPtAcrossLine(const Vec3& p, const Vec3& p1, const Vec3& p2) {
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double L2 = dx * dx + dy * dy;
    if (L2 < 1e-18) return p;
    // 投影参数 t = ((p-p1)·(p2-p1)) / |p2-p1|²
    const double t = ((p.x - p1.x) * dx + (p.y - p1.y) * dy) / L2;
    const Vec3 foot(p1.x + t * dx, p1.y + t * dy, 0);
    return Vec3(2 * foot.x - p.x, 2 * foot.y - p.y, 0);
}

// ---- 私有辅助：保留图层/颜色/线宽/线型，把几何 pts 应用变换 ----
// transform: 接受一个 Vec3 返回 Vec3；同时可改 radius/radiusY/角度（角度/半径字段）。
static inline Element applyGeom(Element e,
                                const std::function<Vec3(const Vec3&)>& T,
                                double scaleFactor = 1.0) {
    for (auto& p : e.pts) p = T(p);
    // 圆 / 圆弧半径：缩放因子同步
    if (e.type == ElementType::Circle || e.type == ElementType::Arc) {
        e.radius *= scaleFactor;
    }
    // 椭圆长短轴同步
    if (e.type == ElementType::Ellipse) {
        e.radius  *= scaleFactor;
        e.radiusY *= scaleFactor;
    }
    return e;
}

// ============================================================
// 1. 删除
// ============================================================
std::vector<Change> makeDelete(const std::vector<int>& ids) {
    std::vector<Change> out;
    out.reserve(ids.size());
    for (int id : ids) {
        Change c; c.id = id; c.remove = true; c.note = QStringLiteral("删除");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 2. 复制 (Duplicate) —— 原图元不变，新增一份偏置后的克隆
// ============================================================
std::vector<Change> makeDuplicate(const std::vector<Element>& src,
                                  double dx, double dy) {
    std::vector<Change> out;
    out.reserve(src.size());
    const Vec3 delta(dx, dy);
    for (const auto& s : src) {
        Element e = s;
        e.id = -1;  // 标记新增
        e.name = s.name;
        e.pts.clear();
        for (const auto& p : s.pts) e.pts.push_back(p + delta);
        e.radius *= 1.0;  // 偏置不影响半径
        Change c; c.id = -1; c.replacement = e; c.remove = false;
        c.note = QStringLiteral("复制");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 3. 移动 / 复制粘贴 Move-Copy
// ============================================================
std::vector<Change> makeMoveCopy(const std::vector<Element>& src,
                                 const Vec3& delta, bool copy) {
    std::vector<Change> out;
    out.reserve(src.size());
    auto T = [&](const Vec3& p) { return p + delta; };
    for (const auto& s : src) {
        if (copy) {
            // 复制模式：保留 ID 不变模式无法保留（id 唯一），统一走新增
            Element e = applyGeom(s, T);
            e.id = -1;
            Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("复制移动");
            out.push_back(c);
        } else {
            // 移动模式：替换原 id，保留 ID 不变（属性面板/BOM 不跳）
            Element e = applyGeom(s, T);
            e.id = s.id;
            Change c; c.id = s.id; c.replacement = e;
            c.note = QStringLiteral("移动");
            out.push_back(c);
        }
    }
    return out;
}

// ============================================================
// 4. 旋转
// ============================================================
std::vector<Change> makeRotate(const std::vector<Element>& src,
                               const Vec3& center, double angleDeg) {
    std::vector<Change> out;
    out.reserve(src.size());
    auto T = [&](const Vec3& p) { return rotatePt(p, center, angleDeg); };
    for (const auto& s : src) {
        Element e = applyGeom(s, T);
        e.id = s.id;
        // 圆弧 / 标注弧：起止角同步旋转（统一旋转后几何位置正确，标注弧是相对几何的表达）
        if (e.type == ElementType::Arc) {
            // 圆心已经在上面变换了；起止角是不变量（圆弧整体旋转 → 角度不变）。
            // 但若 Arc 由"圆心+起止角"描述，中心不变时角度也不变；中心变了仍以新中心为基准，
            // 起止角就是标准圆心表示的极角，不随中心变化。故不需调整。
        }
        if (e.type == ElementType::Ellipse) {
            e.rotation += angleDeg;  // 椭圆长轴方向跟着转
        }
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("旋转 %1°").arg(angleDeg, 0, 'f', 2);
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 5. 缩放
// ============================================================
std::vector<Change> makeScale(const std::vector<Element>& src,
                              const Vec3& center, double factor) {
    if (factor <= 0) return {};
    std::vector<Change> out;
    out.reserve(src.size());
    auto T = [&](const Vec3& p) { return scalePt(p, center, factor); };
    for (const auto& s : src) {
        Element e = applyGeom(s, T, factor);
        e.id = s.id;
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("缩放 %1 倍").arg(factor, 0, 'f', 3);
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 6. 镜像（严格对称：点对称 / 轴对称）
// ------------------------------------------------------------
// 数学基础（世界坐标，X 右 / Y 上）：
//   * 点对称（关于点 C）：逐点 P' = 2C − P。等价「绕 C 旋转 180°」，
//     故方向角类字段统一 +180°（圆弧起止角、椭圆长轴角、剖面线方向角）。
//   * 轴对称（关于直线 p1→p2）：逐点沿该直线镜像（见 mirrorPtAcrossLine）。
//     方向角类字段按反射公式 θ' = 2β − θ 修正，其中 β = atan2(p2−p1) 为镜像轴世界角。
// 反射是等距变换（保距），故半径 / 半轴 / 字高一律不变；折线 / 样条的弧段由
// 三个相邻节点定心，反射控制点即反射整条弧（无需单独处理角度）。
// 行为：保留源图元，新增一份镜像副本（replacement.id = -1，由模型分配新 ID），
//       图层 / 颜色 / 线宽 / 线型 / 文本等属性全部继承源图元。
// ============================================================
std::vector<Change> makeMirror(const std::vector<Element>& src,
                               bool isPointMirror,
                               const Vec3& base1,
                               const Vec3& base2) {
    std::vector<Change> out;
    out.reserve(src.size());

    // 线对称轴角 β（世界 CCW 角），用于角度反射 θ' = 2β − θ
    const double dx = base2.x - base1.x, dy = base2.y - base1.y;
    const double L2 = dx * dx + dy * dy;
    const double beta = (L2 < 1e-18) ? 0.0
                                      : std::atan2(dy, dx) * 180.0 / M_PI;

    for (const auto& s : src) {
        Element e = s;
        e.id = -1;                       // 新增镜像副本，源图元保留
        // ---- 逐点反射 ----
        if (isPointMirror) {
            for (auto& p : e.pts)
                p = Vec3(2 * base1.x - p.x, 2 * base1.y - p.y, 0);
        } else {
            for (auto& p : e.pts)
                p = mirrorPtAcrossLine(p, base1, base2);
        }
        // ---- 剖面区域 / 自定义边界：内孔点列同步反射 ----
        if (e.type == ElementType::Hatch || e.type == ElementType::Boundary) {
            for (auto& hole : e.holes) {
                for (auto& p : hole) {
                    p = isPointMirror
                          ? Vec3(2 * base1.x - p.x, 2 * base1.y - p.y, 0)
                          : mirrorPtAcrossLine(p, base1, base2);
                }
            }
        }
        // ---- 角度类字段的反射修正 ----
        if (e.type == ElementType::Arc) {
            if (isPointMirror) { e.startAngle += 180.0; e.endAngle += 180.0; }
            else                { e.startAngle = 2.0 * beta - e.startAngle;
                                 e.endAngle   = 2.0 * beta - e.endAngle; }
        }
        if (e.type == ElementType::Ellipse) {
            e.rotation = isPointMirror ? (e.rotation + 180.0)
                                       : (2.0 * beta - e.rotation);
        }
        if (e.type == ElementType::Hatch || e.type == ElementType::Boundary) {
            e.rotation = isPointMirror ? (e.rotation + 180.0)
                                       : (2.0 * beta - e.rotation);
        }
        // 半径 / 半轴 / 字高：反射保距，不变。
        // 圆 / 圆弧 / 椭圆 / 文本 / 标注 / 折线 / 样条 / 矩形 / 多边形 / 约束：
        //   几何已由 pts（及 holes）逐点反射覆盖，无需额外处理。

        Change c; c.id = -1; c.replacement = e;
        c.note = isPointMirror ? QStringLiteral("点镜像")
                               : QStringLiteral("线镜像");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 7. 反向 (Revert Direction) —— 直线交换端点；圆弧交换起止角
// ============================================================
std::vector<Change> makeRevertDirection(const std::vector<Element>& src) {
    std::vector<Change> out;
    out.reserve(src.size());
    for (const auto& s : src) {
        Element e = s;
        e.id = s.id;
        if (e.type == ElementType::Line && e.pts.size() >= 2) {
            std::swap(e.pts[0], e.pts[1]);
        } else if (e.type == ElementType::Arc) {
            // 把扫掠方向反转：start/end 互换（旧的 start = 新的 end）
            std::swap(e.startAngle, e.endAngle);
        } else if (e.type == ElementType::Polyline) {
            // 折线：节点顺序反转，arcFlags 同步反转（弧段的"上一节点"和"下一节点"互换）
            std::reverse(e.pts.begin(), e.pts.end());
            std::reverse(e.arcFlags.begin(), e.arcFlags.end());
        }
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("反向");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 8. 延长 / 缩短 (Lengthen) —— 仅作用于直线
//   side=0 → 改起点 pts[0]；side=1 → 改终点 pts[1]
//   delta>0 延长；delta<0 缩短
// ============================================================
std::vector<Change> makeLengthen(const std::vector<Element>& src, int side, double delta) {
    std::vector<Change> out;
    out.reserve(src.size());
    for (const auto& s : src) {
        Element e = s;
        e.id = s.id;
        if (e.type != ElementType::Line || e.pts.size() < 2) {
            // 不支持的图元：原样透传（保证菜单激活可应用时不抛错）
            Change c; c.id = s.id; c.replacement = e; c.note = QStringLiteral("（跳过）");
            out.push_back(c);
            continue;
        }
        // 当前端点的位置 = base，沿 (base - other) 方向延伸
        Vec3 base = s.pts[side];
        Vec3 other = s.pts[1 - side];
        Vec3 dir = base - other;
        const double L = dir.len2D();
        if (L < 1e-12) continue;
        dir = Vec3(dir.x / L, dir.y / L, 0);
        Vec3 nb = Vec3(base.x + dir.x * delta, base.y + dir.y * delta, 0);
        // 若缩短过多导致端点越过 other，反向限制在对方 1e-6 外侧
        if (delta < 0) {
            // 重新计算：若 nb 与 other 的距离 < 1e-6，把 nb 推回到 other + dir*1e-6
            const double L2 = (nb - other).len2D();
            if (L2 < 1e-6) {
                nb = Vec3(other.x + dir.x * 1e-6, other.y + dir.y * 1e-6, 0);
            }
        }
        e.pts[side] = nb;
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("延长 %1 mm").arg(delta, 0, 'f', 2);
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 9. 偏移 (Offset) —— 仅作用于直线 / 圆 / 圆弧
//   直线：朝 (a→b) 的左侧偏移 dist mm（负值朝右侧）
//   圆 / 圆弧：半径 += dist（dist=±r 退化为 0；夹缝负到 -r 视为非法）
// ============================================================
std::vector<Change> makeOffset(const std::vector<Element>& src, double dist) {
    std::vector<Change> out;
    out.reserve(src.size());
    for (const auto& s : src) {
        Element e = s;
        e.id = s.id;
        if (e.type == ElementType::Line && s.pts.size() >= 2) {
            const Vec3 a = s.pts[0], b = s.pts[1];
            const Vec3 dir = b - a;
            const double L = dir.len2D();
            if (L < 1e-12) continue;
            // 单位左法线 = perp 单位向量
            const Vec3 nrm(-dir.y / L, dir.x / L, 0);
            e.pts[0] = Vec3(a.x + nrm.x * dist, a.y + nrm.y * dist, 0);
            e.pts[1] = Vec3(b.x + nrm.x * dist, b.y + nrm.y * dist, 0);
        } else if (e.type == ElementType::Circle) {
            // 圆整体偏移：圆心不变（无方向信息），仅改半径
            const double nr = s.radius + dist;
            if (nr <= 0) continue;
            e.radius = nr;
        } else if (e.type == ElementType::Arc) {
            const double nr = s.radius + dist;
            if (nr <= 0) continue;
            e.radius = nr;
        } else {
            // 不支持的图元：跳过
            continue;
        }
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("偏移 %1 mm").arg(dist, 0, 'f', 2);
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 10. 打断 / 等分 (Break / Divide)
//   n == 0 → 单点打断（保留 id + 新增 id2）
//   n >= 2 → n 等分，保留 id + 新增 n-1 条
// ============================================================
std::vector<Change> makeBreakDivide(const std::vector<Element>& src, int id,
                                    const Vec3& cut, int n) {
    std::vector<Change> out;
    if (n < 0) return out;
    for (const auto& s : src) {
        if (s.id != id) continue;
        if (s.type != ElementType::Line || s.pts.size() < 2) continue;
        const Vec3 a = s.pts[0], b = s.pts[1];
        const Vec3 ab = b - a;
        const double L = ab.len2D();
        if (L < 1e-12) continue;
        const Vec3 u(ab.x / L, ab.y / L, 0);
        // 投影 cut 到线段上（限制在 [0, L]）
        double t = (cut.x - a.x) * u.x + (cut.y - a.y) * u.y;
        if (n == 0) {
            // 单点打断
            if (t <= 1e-9 || t >= L - 1e-9) continue;  // 端点附近无意义
            Vec3 p(a.x + u.x * t, a.y + u.y * t, 0);
            // 保留前半段
            Element e1 = s; e1.id = s.id; e1.pts = {a, p};
            Change c1; c1.id = s.id; c1.replacement = e1;
            c1.note = QStringLiteral("打断前半");
            out.push_back(c1);
            // 新增后半段
            Element e2 = s; e2.id = -1; e2.pts = {p, b};
            Change c2; c2.id = -1; c2.replacement = e2;
            c2.note = QStringLiteral("打断后半");
            out.push_back(c2);
        } else {
            // 等分为 n 段
            if (n < 2) continue;
            std::vector<Vec3> pts;
            pts.reserve(n + 1);
            for (int k = 0; k <= n; ++k) {
                double tt = (L * k) / n;
                pts.push_back(Vec3(a.x + u.x * tt, a.y + u.y * tt, 0));
            }
            // 保留第一段
            Element e1 = s; e1.id = s.id; e1.pts = {pts[0], pts[1]};
            Change c1; c1.id = s.id; c1.replacement = e1;
            c1.note = QStringLiteral("等分首段");
            out.push_back(c1);
            // 新增剩余 n-1 段
            for (int k = 1; k < n; ++k) {
                Element e = s; e.id = -1; e.pts = {pts[k], pts[k + 1]};
                Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("等分");
                out.push_back(c);
            }
        }
        break;  // 只处理 id 那一条
    }
    return out;
}

// ============================================================
// 11. 圆角 / 倒角 (Fillet / Bevel)
//   id1 / id2: 两条直线
//   radius: >0 = 圆角；=0 = 倒角（直接交点处切掉）
//   算法：找到两直线交点；以交点为基，沿两线分别退后 d = radius * tan(θ/2)；
//         radius=0 时 d=0 退化为两条交点截断线段
//   返回 5 个 Change：删 id1 + 删 id2 + 新增 1 条连接段 + 新增 2 条裁短线段
// ============================================================
std::vector<Change> makeFillet(int id1, int id2, double radius,
                               const std::vector<Element>& pool) {
    std::vector<Change> out;
    if (radius < 0) return out;
    auto it1 = std::find_if(pool.begin(), pool.end(),
                            [&](const Element& e){ return e.id == id1; });
    auto it2 = std::find_if(pool.begin(), pool.end(),
                            [&](const Element& e){ return e.id == id2; });
    if (it1 == pool.end() || it2 == pool.end()) return out;
    if (it1->type != ElementType::Line || it2->type != ElementType::Line) return out;
    if (it1->pts.size() < 2 || it2->pts.size() < 2) return out;

    const Vec3 a1 = it1->pts[0], b1 = it1->pts[1];
    const Vec3 a2 = it2->pts[0], b2 = it2->pts[1];
    Vec3 ip;
    if (!math::lineLineIntersect(a1, b1, a2, b2, ip)) {
        // 平行：无法圆角
        return out;
    }

    // 计算两条单位方向
    Vec3 u1 = (b1 - a1); const double L1 = u1.len2D();
    Vec3 u2 = (b2 - a2); const double L2 = u2.len2D();
    if (L1 < 1e-9 || L2 < 1e-9) return out;
    u1 = Vec3(u1.x / L1, u1.y / L1, 0);
    u2 = Vec3(u2.x / L2, u2.y / L2, 0);

    // 交角 θ：取锐角
    double dot = u1.x * u2.x + u1.y * u2.y;
    dot = std::min(1.0, std::max(-1.0, dot));
    const double theta = std::acos(std::fabs(dot));
    if (theta < 1e-3) return out;  // 几乎平行

    // 退后距离 d = radius * tan(θ/2)；radius=0 → d=0
    const double d = radius * std::tan(theta / 2.0);

    // 直线 1：从 a1 → b1，沿前进方向；trim 点 = ip - u1 * d（如果 u1 朝向 ip）
    // 判断 u1 是否朝向 ip：若 (ip - a1) 与 u1 点积 > 0，则 a1 端朝向 ip；否则 b1 端朝向 ip
    const double t1 = (ip.x - a1.x) * u1.x + (ip.y - a1.y) * u1.y;
    Vec3 line1End;
    if (t1 >= 0) {
        // a1 → ip，trimEnd = ip - u1 * d
        line1End = Vec3(ip.x - u1.x * d, ip.y - u1.y * d, 0);
        Element e = *it1; e.id = -1;
        e.pts = {a1, line1End};
        Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("裁短线1");
        out.push_back(c);
        Change del; del.id = id1; del.remove = true; del.note = QStringLiteral("原线1删除");
        // 注意：原线1 整段要删除；我们用"删除 id1"标记，并新增裁短后的线
        // 由于 applyChanges 先 remove 再 add，整条线删除是符合预期的
        out.push_back(del);
    } else {
        // b1 → ip，trimEnd = ip + u1 * d
        line1End = Vec3(ip.x + u1.x * d, ip.y + u1.y * d, 0);
        Element e = *it1; e.id = -1;
        e.pts = {b1, line1End};
        Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("裁短线1");
        out.push_back(c);
        Change del; del.id = id1; del.remove = true;
        out.push_back(del);
    }

    const double t2 = (ip.x - a2.x) * u2.x + (ip.y - a2.y) * u2.y;
    Vec3 line2End;
    if (t2 >= 0) {
        line2End = Vec3(ip.x - u2.x * d, ip.y - u2.y * d, 0);
        Element e = *it2; e.id = -1;
        e.pts = {a2, line2End};
        Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("裁短线2");
        out.push_back(c);
        Change del; del.id = id2; del.remove = true;
        out.push_back(del);
    } else {
        line2End = Vec3(ip.x + u2.x * d, ip.y + u2.y * d, 0);
        Element e = *it2; e.id = -1;
        e.pts = {b2, line2End};
        Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("裁短线2");
        out.push_back(c);
        Change del; del.id = id2; del.remove = true;
        out.push_back(del);
    }

    // 圆角：圆弧（id=-1），圆心 = 沿 u1 与 u2 角平分线方向距 ip = radius / sin(θ/2)
    // 倒角：直线段（id=-1），两端 = line1End, line2End
    if (radius > 1e-9) {
        // 圆弧：角平分线方向 = (u1 指向 ip 的方向 + u2 指向 ip 的方向) 单位化
        Vec3 toI1(-u1.x, -u1.y, 0);  // 由线段指向 ip 的反方向
        Vec3 toI2(-u2.x, -u2.y, 0);
        if (t1 < 0) toI1 = u1; else toI1 = Vec3(-u1.x, -u1.y, 0);
        if (t2 < 0) toI2 = u2; else toI2 = Vec3(-u2.x, -u2.y, 0);
        Vec3 bis = Vec3(toI1.x + toI2.x, toI1.y + toI2.y, 0);
        double Lb = bis.len2D();
        if (Lb < 1e-9) return out;
        bis = Vec3(bis.x / Lb, bis.y / Lb, 0);
        // 圆心 = ip - bis * (radius / sin(θ/2))
        const double dC = radius / std::sin(theta / 2.0);
        Vec3 ctr(ip.x - bis.x * dC, ip.y - bis.y * dC, 0);
        // 起止角：圆心 → line1End，扫掠到 line2End（CCW 为正，但题目未要求严格方向）
        const double a0 = std::atan2(line1End.y - ctr.y, line1End.x - ctr.x) * 180.0 / M_PI;
        const double a1v = std::atan2(line2End.y - ctr.y, line2End.x - ctr.x) * 180.0 / M_PI;
        Element arc = *it1; arc.id = -1;
        arc.type = ElementType::Arc;
        arc.pts = {ctr};
        arc.radius = radius;
        arc.startAngle = a0;
        arc.endAngle = a1v;
        arc.name = QStringLiteral("圆角");
        Change c; c.id = -1; c.replacement = arc; c.note = QStringLiteral("圆角弧");
        out.push_back(c);
    } else {
        // 倒角：连接 line1End → line2End 的直线
        Element e = *it1; e.id = -1;
        e.type = ElementType::Line;
        e.pts = {line1End, line2End};
        e.name = QStringLiteral("倒角");
        Change c; c.id = -1; c.replacement = e; c.note = QStringLiteral("倒角连线");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 11b. 倒角 / 倒圆（基于 corner 核心层）
//      把 corner::CornerResult 转成 Change：删原两线 + 增裁短线×2 + 增倒角/圆角段。
//      corner 层已复制源样式，这里只改 id 落盘；res.ok==false 返回空集合。
// ============================================================
std::vector<Change> makeCornerChanges(int id1, int id2, const corner::CornerResult& res) {
    std::vector<Change> out;
    if (!res.ok) return out;

    // 原两直线删除
    Change del1; del1.id = id1; del1.remove = true; del1.note = QStringLiteral("原线1删除");
    out.push_back(del1);
    Change del2; del2.id = id2; del2.remove = true; del2.note = QStringLiteral("原线2删除");
    out.push_back(del2);

    // 裁短后的两线（保留端不变、近端裁到裁断点），以新增方式落盘
    Element ea = res.lineA; ea.id = -1;
    Change c1; c1.id = -1; c1.replacement = ea; c1.note = QStringLiteral("裁短线1");
    out.push_back(c1);
    Element eb = res.lineB; eb.id = -1;
    Change c2; c2.id = -1; c2.replacement = eb; c2.note = QStringLiteral("裁短线2");
    out.push_back(c2);

    // 倒角直线 / 倒圆圆弧（继承 a 样式）
    Element cc = res.corner; cc.id = -1;
    Change c3; c3.id = -1; c3.replacement = cc; c3.note = QStringLiteral("倒角/圆角段");
    out.push_back(c3);
    return out;
}

// ============================================================
// 12. 分解 (Explode) —— 矩形/多边形/折线/样条 → 多段独立直线/圆弧
// ============================================================
std::vector<Change> makeExplode(const std::vector<Element>& src) {
    std::vector<Change> out;
    for (const auto& s : src) {
        if (s.type == ElementType::Rectangle && s.pts.size() == 2) {
            // 矩形：pts = (左下, 右上)
            Vec3 a = s.pts[0], b = s.pts[1];
            Vec3 c(b.x, a.y), d(a.x, b.y);
            // 四条边（ccw）
            Vec3 corners[4] = {a, c, b, d};
            for (int k = 0; k < 4; ++k) {
                Element e = s; e.id = -1;
                e.type = ElementType::Line;
                e.pts = {corners[k], corners[(k + 1) % 4]};
                Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("分解边");
                out.push_back(c);
            }
            Change del; del.id = s.id; del.remove = true; del.note = QStringLiteral("矩形删除");
            out.push_back(del);
        } else if (s.type == ElementType::Polygon && s.pts.size() >= 3) {
            // 多边形：pts = 顶点序列（首尾点隐式闭合）
            int n = (int)s.pts.size();
            for (int k = 0; k < n; ++k) {
                Element e = s; e.id = -1;
                e.type = ElementType::Line;
                e.pts = {s.pts[k], s.pts[(k + 1) % n]};
                Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("分解边");
                out.push_back(c);
            }
            Change del; del.id = s.id; del.remove = true;
            out.push_back(del);
        } else if (s.type == ElementType::Polyline && s.pts.size() >= 2) {
            // 折线：段数 = pts.size()-1（开放）或 pts.size()（闭合）
            const bool closed = s.closed;
            int segs = closed ? (int)s.pts.size() : (int)s.pts.size() - 1;
            for (int k = 0; k < segs; ++k) {
                int next = (k + 1) % (int)s.pts.size();
                const bool isArc = (k < (int)s.arcFlags.size() && s.arcFlags[k]);
                Element e = s; e.id = -1;
                e.type = isArc ? ElementType::Arc : ElementType::Line;
                if (!isArc) {
                    e.pts = {s.pts[k], s.pts[next]};
                } else {
                    // 圆弧：用 arcThrough 求圆心/半径/起止角
                    auto arcInfo = geo::arcThrough(s.pts, k, closed);
                    if (!arcInfo.valid) {
                        // 退化段：当直线处理
                        e.type = ElementType::Line;
                        e.pts = {s.pts[k], s.pts[next]};
                    } else {
                        e.pts = {arcInfo.center};
                        e.radius = arcInfo.radius;
                        e.startAngle = arcInfo.startDeg;
                        e.endAngle = arcInfo.startDeg + arcInfo.sweepDeg;
                    }
                }
                Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("分解段");
                out.push_back(c);
            }
            Change del; del.id = s.id; del.remove = true;
            out.push_back(del);
        } else if (s.type == ElementType::Spline && s.pts.size() >= 2) {
            // 样条：按 splineSeg 段离散
            std::vector<Vec3> samples = geo::sampleSpline(s.pts, 16);
            if (samples.size() < 2) continue;
            for (size_t k = 0; k + 1 < samples.size(); ++k) {
                Element e = s; e.id = -1;
                e.type = ElementType::Line;
                e.pts = {samples[k], samples[k + 1]};
                Change c; c.id = -1; c.replacement = e;
                c.note = QStringLiteral("样条离散");
                out.push_back(c);
            }
            Change del; del.id = s.id; del.remove = true;
            out.push_back(del);
        } else {
            // 直线/圆/弧/椭圆/文本/标注：原样透传（Explode 对它们无意义）
            Change c; c.id = s.id; c.replacement = s; c.note = QStringLiteral("（跳过）");
            out.push_back(c);
        }
    }
    return out;
}

// ============================================================
// 13. 对齐 (Align) —— 把 src 的"对齐锚点"全部对齐到 ref 的同一锚点
//   kind: 0=左,1=水平中,2=右,3=顶,4=垂直中,5=底
// ============================================================
std::vector<Change> makeAlign(const std::vector<Element>& src,
                              const Element& ref, int kind) {
    std::vector<Change> out;
    if (src.empty()) return out;
    QRectF refB = geo::elementBounds(ref);
    double target = 0;
    switch (kind) {
        case 0: target = refB.left();   break;
        case 1: target = refB.center().x(); break;
        case 2: target = refB.right();  break;
        case 3: target = refB.top();    break;
        case 4: target = refB.center().y(); break;
        case 5: target = refB.bottom(); break;
        default: return out;
    }
    for (const auto& s : src) {
        QRectF b = geo::elementBounds(s);
        double cur = 0;
        switch (kind) {
            case 0: cur = b.left();   break;
            case 1: cur = b.center().x(); break;
            case 2: cur = b.right();  break;
            case 3: cur = b.top();    break;
            case 4: cur = b.center().y(); break;
            case 5: cur = b.bottom(); break;
        }
        const double delta = target - cur;
        Vec3 dvec;
        if (kind <= 2) dvec = Vec3(delta, 0, 0);
        else            dvec = Vec3(0, delta, 0);
        Element e = s; e.id = s.id;
        for (auto& p : e.pts) p = p + dvec;
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("对齐 (%1)").arg(kind);
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 14. 拉伸 (Stretch) —— 把 src 中包含 basePt 端点的图元的那个端点移到 toPoint
// ============================================================
std::vector<Change> makeStretch(const std::vector<Element>& src,
                                const Vec3& basePt, const Vec3& toPoint,
                                double tol) {
    std::vector<Change> out;
    for (const auto& s : src) {
        int hitIdx = -1;
        for (size_t i = 0; i < s.pts.size(); ++i) {
            if (s.pts[i].distTo(basePt) <= tol) { hitIdx = (int)i; break; }
        }
        if (hitIdx < 0) continue;
        Element e = s; e.id = s.id;
        e.pts[hitIdx] = toPoint;
        Change c; c.id = s.id; c.replacement = e;
        c.note = QStringLiteral("拉伸端点");
        out.push_back(c);
    }
    return out;
}

// ============================================================
// 15. 矩形阵列 (Rectangular Array) —— 平移复制，原图元不动
// ============================================================
std::vector<Change> makeRectangularArray(const std::vector<Element>& src,
                                         int rows, int cols,
                                         double rowSpacing, double colSpacing) {
    std::vector<Change> out;
    if (rows < 1 || cols < 1) return out;
    // 预留容量：每个源图元最多产生 (rows·cols − 1) 个副本
    out.reserve(src.size() * (rows * cols - 1));
    for (const auto& s : src) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (r == 0 && c == 0) continue;   // 原图元位置，跳过（保留原件）
                const Vec3 off(c * colSpacing, r * rowSpacing, 0.0);
                Element e = s;
                e.id = -1;                         // 标记新增副本
                e.name = s.name;                   // 继承名称（属性一并继承）
                for (auto& p : e.pts) p = p + off;
                // 圆/弧/椭圆半径与角度不随平移变化，保持原值
                Change ch; ch.id = -1; ch.replacement = e;
                ch.note = QStringLiteral("矩形阵列 (%1,%2)").arg(r).arg(c);
                out.push_back(ch);
            }
        }
    }
    return out;
}

// ============================================================
// 16. 环形阵列 (Polar Array) —— 绕 center 旋转复制，原图元不动
// ============================================================
std::vector<Change> makePolarArray(const std::vector<Element>& src,
                                   const Vec3& center, int count,
                                   double angleRangeDeg) {
    std::vector<Change> out;
    if (count < 2) return out;
    out.reserve(src.size() * (count - 1));
    for (const auto& s : src) {
        for (int k = 1; k < count; ++k) {
            const double ang = k * angleRangeDeg / count;   // k=0 即原图元，跳过
            Element e = s;
            e.id = -1;
            e.name = s.name;
            // 旋转所有控制点（含圆/弧/椭圆的圆心 pts[0]）
            for (auto& p : e.pts) p = rotatePt(p, center, ang);
            // 半径 / 半轴不随旋转变化
            // 圆弧：整体绕 center 刚性旋转 → 相对自身圆心，起止角同步 +ang
            if (e.type == ElementType::Arc) {
                e.startAngle += ang;
                e.endAngle   += ang;
            }
            // 椭圆：长轴方向随整体旋转 +ang
            if (e.type == ElementType::Ellipse) {
                e.rotation += ang;
            }
            Change ch; ch.id = -1; ch.replacement = e;
            ch.note = QStringLiteral("环形阵列 %1°").arg(ang, 0, 'f', 2);
            out.push_back(ch);
        }
    }
    return out;
}

// ============================================================
// applyChanges：把 Change 写入 SketchModel 的统一入口
//   * 调用方需在调用前自己 beginEdit() 一次（push 一份快照）
//   * 此处所有修改走 Quiet 系列 API（addElementsQuiet / removeElementQuiet /
//     updateElementQuiet），不再重复压栈，整组操作可一次 Ctrl+Z 撤回。
//   * id < 0 的 replacement 走 addElementsQuiet（新 ID 由模型分配）
//   * 复用 id 的部分走 updateElementQuiet，保留图元 ID（属性面板/BOM 不跳）
// ============================================================
std::vector<int> applyChanges(SketchModel* model, const std::vector<Change>& changes,
                              const QString& /*undoLabel*/) {
    if (!model) return {};
    std::vector<int> newIds;

    // 1) 先按 remove 把要删的（静默）
    for (const auto& c : changes) {
        if (c.remove && c.id >= 0) {
            model->removeElementQuiet(c.id);
        }
    }

    // 2) 复用 id 走 updateElementQuiet；id<0 收集到 toAdd 后批量 addElementsQuiet
    std::vector<Element> toAdd;
    for (const auto& c : changes) {
        if (c.remove) continue;
        Element e = c.replacement;
        // 一致性保障：调用方在 c.id 中声明了目标图元（>=0）时，一律以 c.id 为准覆盖
        // replacement.id。否则一旦工厂忘了给 replacement 赋 ID（历史上剖面填充的 Hatch
        // 就漏了这一步，replacement.id 停留在默认 0），updateElementQuiet(0,...) 会因
        // "找不到该图元"静默返回 false —— 界面表现就是"点了确定没有任何反应"。
        if (c.id >= 0 && e.id >= 0) e.id = c.id;
        if (e.id < 0) {
            e.id = 0;  // 清掉 -1，让 addElementsQuiet 重新分配
            toAdd.push_back(e);
        } else {
            // 复用原 id：走 updateElementQuiet（不重复压撤销）
            model->updateElementQuiet(e.id, [&](Element& dst) {
                dst = e;
                dst.id = e.id;
            });
        }
    }
    // 一次性添加所有新增（不压撤销）
    if (!toAdd.empty()) {
        newIds = model->addElementsQuiet(toAdd);
    }
    return newIds;
}

} // namespace modify
} // namespace SketchCAD