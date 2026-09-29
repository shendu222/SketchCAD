#include "Cleanup.h"
#include "SketchModel.h"
#include "Geometry.h"
#include "Modify.h"
#include "Layer.h"
#include "Block.h"

#include <cmath>
#include <set>
#include <algorithm>

namespace SketchCAD {
namespace cleanup {

// ============================================================================
// 辅助：退化判定（垃圾图元）
// ============================================================================

// 任一坐标非有限（NaN/inf）→ 图元彻底不可渲染，属垃圾
static bool finitePt(const Vec3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

// 闭合多边形面积（shoelace）
static double polyArea(const std::vector<Vec3>& p) {
    double a = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        const Vec3& u = p[i];
        const Vec3& v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return std::fabs(a) * 0.5;
}

static bool isGarbage(const Element& e, double tol) {
    for (const auto& p : e.pts) if (!finitePt(p)) return true;
    if (!std::isfinite(e.radius) || !std::isfinite(e.radiusY)) return true;

    switch (e.type) {
        case ElementType::Line:
            if (e.pts.size() < 2) return true;
            return e.pts[0].distTo(e.pts[1]) <= tol;          // 零长直线
        case ElementType::Circle:
            return e.radius <= tol;                            // 零/负半径圆
        case ElementType::Arc: {
            if (e.radius <= tol) return true;
            // 起止角几乎相等（不含"整圈 360°"这种合法表达）
            const double d = e.endAngle - e.startAngle;
            const bool eq   = std::fabs(d) <= tol;
            const bool full = std::fabs(std::fabs(d) - 360.0) <= tol;
            return eq && !full;
        }
        case ElementType::Ellipse:
            return e.radius <= tol || e.radiusY <= tol;
        case ElementType::Rectangle:
        case ElementType::Polygon:
        case ElementType::Hatch:
        case ElementType::Boundary:
            if (e.pts.size() < 3) return true;
            return polyArea(e.pts) <= tol * tol;               // 近零面积
        case ElementType::Polyline:
        case ElementType::Spline:
            return e.pts.size() < 2;
        case ElementType::Text:
            return e.text.trimmed().isEmpty();                 // 空文字（无内容可渲染）
        default:
            return false;                                      // 其余类型保守保留
    }
}

// ============================================================================
// 辅助：等价判定（OVERKILL）
// ============================================================================

// 两直线端点重合（允许方向相反）
static bool lineEquiv(const Element& a, const Element& b, double tol) {
    const Vec3& a0 = a.pts[0]; const Vec3& a1 = a.pts[1];
    const Vec3& b0 = b.pts[0]; const Vec3& b1 = b.pts[1];
    const bool f = a0.distTo(b0) <= tol && a1.distTo(b1) <= tol;
    const bool r = a0.distTo(b1) <= tol && a1.distTo(b0) <= tol;
    return f || r;
}

// 共线重叠直线合并：两端点分别投影到 a 的方向轴，取整体最小/最大处为新端点。
// 仅当两段近似平行、互距在容差内、且投影区间重叠或相邻（间隙≤tol）时合并成功。
static bool tryMergeLines(const Element& a, const Element& b, double tol, Element& out) {
    if (a.pts.size() < 2 || b.pts.size() < 2) return false;
    const Vec3 a0 = a.pts[0], a1 = a.pts[1], b0 = b.pts[0], b1 = b.pts[1];
    const Vec3 d1 = a1 - a0; const double L1 = d1.len2D(); if (L1 < 1e-9) return false;
    const Vec3 u1 = d1 * (1.0 / L1);
    const Vec3 d2 = b1 - b0; const double L2 = d2.len2D(); if (L2 < 1e-9) return false;
    const Vec3 u2 = d2 * (1.0 / L2);

    // 近平行（含反向）：|cos| ≈ 1（容差约 ±2.5°）
    if (std::fabs(std::fabs(u1.dot2D(u2)) - 1.0) > 1e-3) return false;

    // 每个端点到对方无限直线的距离都在容差内 → 共线
    auto distPtLine = [](const Vec3& p, const Vec3& o, const Vec3& u) {
        const Vec3 w = p - o;
        const double t = w.dot2D(u);
        return p.distTo(o + u * t);
    };
    if (distPtLine(b0, a0, u1) > tol || distPtLine(b1, a0, u1) > tol) return false;
    if (distPtLine(a0, b0, u2) > tol || distPtLine(a1, b0, u2) > tol) return false;

    // 投影区间必须重叠或相邻（否则只是同一条无限直线上的两段分离线段，不可合并）
    auto proj = [](const Vec3& p, const Vec3& o, const Vec3& u) { return (p - o).dot2D(u); };
    const double ta0 = proj(a0, a0, u1), ta1 = proj(a1, a0, u1);
    const double tb0 = proj(b0, a0, u1), tb1 = proj(b1, a0, u1);
    const double minA = std::min(ta0, ta1), maxA = std::max(ta0, ta1);
    const double minB = std::min(tb0, tb1), maxB = std::max(tb0, tb1);
    if (std::max(minA, minB) > std::min(maxA, maxB) + tol) return false;

    const double lo = std::min(std::min(ta0, ta1), std::min(tb0, tb1));
    const double hi = std::max(std::max(ta0, ta1), std::max(tb0, tb1));
    out = a;                                    // 继承 a 的全部样式/图层/属性
    out.id = -1;
    out.pts = { a0 + u1 * lo, a0 + u1 * hi };   // 合并后的两端
    out.closed = false;
    return true;
}

static bool circleEquiv(const Element& a, const Element& b, double tol) {
    return a.pts[0].distTo(b.pts[0]) <= tol && std::fabs(a.radius - b.radius) <= tol;
}

static bool arcEquiv(const Element& a, const Element& b, double tol) {
    if (a.pts[0].distTo(b.pts[0]) > tol) return false;
    if (std::fabs(a.radius - b.radius) > tol) return false;
    const double s1 = geo::wrapDeg360(a.startAngle), e1 = geo::wrapDeg360(a.endAngle);
    const double s2 = geo::wrapDeg360(b.startAngle), e2 = geo::wrapDeg360(b.endAngle);
    const bool ff = std::fabs(s1 - s2) <= tol && std::fabs(e1 - e2) <= tol;
    const bool rr = std::fabs(s1 - e2) <= tol && std::fabs(e1 - s2) <= tol;
    return ff || rr;
}

static bool ellipseEquiv(const Element& a, const Element& b, double tol) {
    if (a.pts[0].distTo(b.pts[0]) > tol) return false;
    if (std::fabs(a.radius - b.radius) > tol) return false;
    if (std::fabs(a.radiusY - b.radiusY) > tol) return false;
    return std::fabs(geo::wrapDeg360(a.rotation) - geo::wrapDeg360(b.rotation)) <= tol;
}

// 顶点序列等价：闭合形状允许循环移位与反向；开放形状允许正向或反向
static bool polyEquiv(const std::vector<Vec3>& A, const std::vector<Vec3>& B,
                      double tol, bool closed) {
    if (A.size() != B.size()) return false;
    if (A.empty()) return true;
    if (closed) {
        for (size_t off = 0; off < A.size(); ++off) {
            bool ok = true;
            for (size_t i = 0; i < A.size(); ++i)
                if (A[i].distTo(B[(i + off) % A.size()]) > tol) { ok = false; break; }
            if (ok) return true;
            ok = true;
            for (size_t i = 0; i < A.size(); ++i)
                if (A[i].distTo(B[(A.size() - 1 - ((i + off) % A.size()))]) > tol) { ok = false; break; }
            if (ok) return true;
        }
        return false;
    }
    bool f = true, r = true;
    for (size_t i = 0; i < A.size(); ++i) {
        if (A[i].distTo(B[i]) > tol) f = false;
        if (A[i].distTo(B[A.size() - 1 - i]) > tol) r = false;
    }
    return f || r;
}

// 内孔集合等价（逐个匹配即可，数量一致且每个 A 孔都能在 B 中找到配对）
static bool holesEquiv(const std::vector<std::vector<Vec3>>& A,
                       const std::vector<std::vector<Vec3>>& B, double tol) {
    if (A.size() != B.size()) return false;
    std::vector<bool> used(B.size(), false);
    for (const auto& ha : A) {
        bool found = false;
        for (size_t j = 0; j < B.size(); ++j) {
            if (used[j]) continue;
            if (polyEquiv(ha, B[j], tol, true)) { used[j] = true; found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// 分类型等价判定。didMerge=true 表示通过"重叠合并"而非"完全重合"判为等价，
// mergedOut 携带合并后的新图元。
static bool typeEquiv(const Element& a, const Element& b, double tol,
                      Element& mergedOut, bool& didMerge) {
    didMerge = false;
    switch (a.type) {
        case ElementType::Line:
            if (lineEquiv(a, b, tol)) return true;
            if (tryMergeLines(a, b, tol, mergedOut)) { didMerge = true; return true; }
            return false;
        case ElementType::Circle:   return circleEquiv(a, b, tol);
        case ElementType::Arc:      return arcEquiv(a, b, tol);
        case ElementType::Ellipse:  return ellipseEquiv(a, b, tol);
        case ElementType::Rectangle:
        case ElementType::Polygon:
            return polyEquiv(a.pts, b.pts, tol, true);
        case ElementType::Hatch:
        case ElementType::Boundary:
            return polyEquiv(a.pts, b.pts, tol, true) && holesEquiv(a.holes, b.holes, tol);
        case ElementType::Polyline:
        case ElementType::Spline:
            return polyEquiv(a.pts, b.pts, tol, a.closed);
        case ElementType::Text:
            return a.pts[0].distTo(b.pts[0]) <= tol &&
                   a.text == b.text &&
                   std::fabs(a.radius - b.radius) <= tol;
        case ElementType::Xline:
            return a.pts[0].distTo(b.pts[0]) <= tol &&
                   std::fabs(geo::wrapDeg360(a.rotation) - geo::wrapDeg360(b.rotation)) <= tol;
        default:
            return false;   // BlockRef / Dimension / Constraint 不参与（有意重复 / id 引用）
    }
}

// 重复对保留谁：GB/T 14665 重合图线优先顺序高者优先 → 可见者优先 → id 小者优先
static int betterKeep(const Element& a, const Element& b) {
    const int pa = linePriority(a.lineStyle), pb = linePriority(b.lineStyle);
    if (pa != pb) return pa > pb ? a.id : b.id;
    if (a.visible != b.visible) return a.visible ? a.id : b.id;
    return std::min(a.id, b.id);
}

// ============================================================================
// 辅助：分桶（把 O(n²) 两两比较压缩到桶内）
// ============================================================================
static double qstep(double v, double s) { return std::floor(v / s + 0.5) * s; }

static QString bucketKey(const Element& e, double tol) {
    const double s = std::max(tol * 50.0, 1e-3);
    double minx = 1e100, miny = 1e100, maxx = -1e100, maxy = -1e100;
    auto upd = [&](const Vec3& p) {
        minx = std::min(minx, p.x); miny = std::min(miny, p.y);
        maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y);
    };
    for (const auto& p : e.pts) upd(p);
    QString k = QStringLiteral("%1|%2|%3|%4|%5")
                    .arg((int)e.type).arg(qstep(minx, s)).arg(qstep(miny, s))
                    .arg(qstep(maxx, s)).arg(qstep(maxy, s));
    if (e.type == ElementType::Circle || e.type == ElementType::Arc)
        k += QStringLiteral("|r%1").arg(qstep(e.radius, s));
    if (e.type == ElementType::Ellipse)
        k += QStringLiteral("|a%1|b%2").arg(qstep(e.radius, s)).arg(qstep(e.radiusY, s));
    return k;
}

static bool canOverkill(ElementType t) {
    return t != ElementType::BlockRef && t != ElementType::Dimension &&
           t != ElementType::Constraint;
}

// ============================================================================
// 主入口
// ============================================================================
QString CleanupStats::summary() const {
    QString s = QStringLiteral("图形清理：图元 %1 → %2（移除 %3：垃圾 %4 / 重复 %5 / 合并 %6")
                    .arg(totalBefore).arg(totalAfter)
                    .arg(totalBefore - totalAfter)
                    .arg(garbageRemoved).arg(dupDeleted).arg(merged);
    if (blocksPurged || layersPurged)
        s += QStringLiteral("；冗余块 %1 / 空图层 %2").arg(blocksPurged).arg(layersPurged);
    s += QStringLiteral("）");
    return s;
}

CleanupStats runCleanup(SketchModel* model, const CleanupOptions& opt, bool dryRun) {
    CleanupStats st;
    if (!model) return st;

    const auto& elems = model->elements();
    st.totalBefore = (int)elems.size();

    std::vector<modify::Change> changes;
    std::set<int> skipIds;                       // 已被判垃圾的 id，OVERKILL 不再比较
    std::vector<QString> blocksToPurge, layersToPurge;

    // ---- 1) 垃圾 / 隐藏图元 ----
    if (opt.purgeGarbage || opt.purgeInvisible) {
        for (const auto& e : elems) {
            bool drop = false;
            if (opt.purgeGarbage && isGarbage(e, opt.tol)) drop = true;
            if (!drop && opt.purgeInvisible && !e.visible) drop = true;
            if (!drop) continue;
            modify::Change c;
            c.id = e.id; c.remove = true;
            changes.push_back(c);
            ++st.garbageRemoved;
            ++st.removedByType[e.type];
            skipIds.insert(e.id);
            st.detail.push_back(QStringLiteral("清除垃圾%1 #%2")
                                    .arg(elementTypeName(e.type)).arg(e.id));
        }
    }

    // ---- 2) 无引用块定义（冗余数据）----
    if (opt.purgeUnusedBlocks) {
        std::set<QString> used;
        for (const auto& e : elems)
            if (e.type == ElementType::BlockRef) used.insert(e.blockName);
        for (const auto& def : model->blockTable())
            if (!used.count(def.name)) blocksToPurge.push_back(def.name);
    }

    // ---- 3) 空图层（冗余数据；保留层 "0" 与当前活动层永不清理）----
    if (opt.purgeUnusedLayers) {
        std::set<QString> usedL;
        for (const auto& e : elems) usedL.insert(e.layer);
        for (int i = 0; i < model->layerCount(); ++i) {
            const Layer& L = model->layerAt(i);
            if (L.name == QStringLiteral("0") || L.name == model->currentLayer()) continue;
            if (!usedL.count(L.name)) layersToPurge.push_back(L.name);
        }
    }

    // ---- 4) OVERKILL（桶内两两比较）----
    if (opt.overkill) {
        std::map<QString, std::vector<int>> buckets;
        for (int i = 0; i < (int)elems.size(); ++i) {
            if (skipIds.count(elems[i].id)) continue;
            if (!canOverkill(elems[i].type)) continue;
            buckets[bucketKey(elems[i], opt.tol)].push_back(i);
        }
        std::vector<bool> removed(elems.size(), false);
        for (auto& kv : buckets) {
            auto& ids = kv.second;
            for (size_t i = 0; i < ids.size(); ++i) {
                for (size_t j = i + 1; j < ids.size(); ++j) {
                    const int ia = ids[i], ib = ids[j];
                    if (removed[ia] || removed[ib]) continue;
                    const Element& A = elems[ia];
                    const Element& B = elems[ib];
                    if (A.type != B.type) continue;
                    Element merged;
                    bool didMerge = false;
                    if (!typeEquiv(A, B, opt.tol, merged, didMerge)) continue;

                    if (didMerge) {
                        modify::Change c1; c1.id = A.id; c1.remove = true; changes.push_back(c1);
                        modify::Change c2; c2.id = B.id; c2.remove = true; changes.push_back(c2);
                        merged.id = -1;
                        modify::Change ca; ca.replacement = merged; changes.push_back(ca);
                        removed[ia] = removed[ib] = true;
                        ++st.merged;
                        ++st.removedByType[A.type];
                        st.detail.push_back(QStringLiteral("合并共线/重叠%1 #%2 与 #%3")
                                                .arg(elementTypeName(A.type))
                                                .arg(A.id).arg(B.id));
                    } else {
                        const int keep = betterKeep(A, B);
                        const int drop = (keep == A.id) ? B.id : A.id;
                        modify::Change c; c.id = drop; c.remove = true; changes.push_back(c);
                        removed[ia] = removed[ib] = true;
                        ++st.dupDeleted;
                        ++st.removedByType[A.type];
                        st.detail.push_back(QStringLiteral("删除重复%1 #%2（与 #%3 重合，保留 #%3）")
                                                .arg(elementTypeName(A.type)).arg(drop).arg(keep));
                    }
                }
            }
        }
    }

    // ---- 统计 ----
    st.totalAfter = st.totalBefore - st.garbageRemoved - st.dupDeleted - st.merged;
    st.blocksPurged = (int)blocksToPurge.size();
    st.layersPurged = (int)layersToPurge.size();

    // ---- 落盘 ----
    if (!dryRun) {
        if (!changes.empty()) {
            model->beginEdit(QStringLiteral("图形清理"));
            modify::applyChanges(model, changes, QString());
        }
        for (const auto& b : blocksToPurge) model->removeBlock(b);
        for (const auto& l : layersToPurge) model->removeLayer(l);
        model->log(st.summary(), "info");
    }
    return st;
}

} // namespace cleanup
} // namespace SketchCAD
