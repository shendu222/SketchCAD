#include "Section.h"
#include <cmath>
#include <functional>
#include <algorithm>
#include <map>
#include <utility>
#include <QRectF>
#include <map>

namespace SketchCAD {

// ============================================================================
// 剖面填充的正确性基石：先"真实判定闭合"，再"填充"。
//
// 历史缺陷根因：上游 sectionOutline 把「点列 ≥ 3」等同于「闭合区域」，
// 下游 geo::hatchLines 又无条件把点列首尾相连当成闭合多边形，二者契约不一致：
//   * 开放样条 / 开放折线被误当闭合区域 → 隐式首尾连线 → 填充溢出真实轮廓；
//   * 圆弧没有分支、多图元首尾相接围成的环完全没有识别 → 区域遗漏。
//
// 本文件的重构把"哪些边界真的闭合"变成一等公民：
//   1) sectionOutline 只承认"自身几何闭合"的图元（含首尾自重合的样条/整圆弧）；
//   2) buildClosedLoopsFromElements 把直线/圆弧/开放折线/开放样条拆成"段"，
//      按端点容差首尾相接串成环，只有真正闭合的链才算区域；
//   3) 非闭合一律安全跳过，且通过 SectionClosureStats / SectionResult 回报原因。
//
// 采样精度：圆/椭圆/圆弧/样条一律按"弦高误差容差"自适应分段（见下方 refineRecursive），
// 保证折线化误差有界，从根本上杜绝"折线进入真实圆弧轮廓之外"的溢出。
// ============================================================================

// ---- 可复用几何原语 ---------------------------------------------------------
// 点列包围盒（可外扩 pad）。统一替代本文件各处重复的 1e100/-1e100 手写包围盒，
// 避免同一逻辑散落多份、宽相位预筛的容差外扩写法不一致。（pad 用于零宽/零高退化
// 包围盒——QRectF::intersects 对面积 0 的矩形恒为 false，必须外扩后才可用于宽相位。）
static QRectF bboxOfPoints(const std::vector<Vec3>& pts, double pad = 0.0) {
    if (pts.empty()) return QRectF();
    double x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
    for (const auto& p : pts) {
        x0 = std::min(x0, p.x); y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
    }
    return QRectF(x0 - pad, y0 - pad, (x1 - x0) + 2.0 * pad, (y1 - y0) + 2.0 * pad);
}

// 收集两条边界折线（pts + closed）之间的「非共线内部交点」，按容差去重。
// 共线（重合边，如相邻矩形共边）不计入——重合边是"相邻接缝"而非"穿越分区"。
// 该原语供 transverse 分区判定复用，避免交点收集逻辑在多处各写一份。
static std::vector<Vec3> collectTransverseHits(const std::vector<Vec3>& a, bool aClosed,
                                               const std::vector<Vec3>& b, bool bClosed,
                                               double tol) {
    std::vector<Vec3> hits;
    hits.reserve(4);
    const int na = (int)a.size(), sa = aClosed ? na : na - 1;
    const int nb = (int)b.size(), sb = bClosed ? nb : nb - 1;
    for (int i = 0; i < sa; ++i) {
        const Vec3 pa = a[i], pb = a[(i + 1) % na];
        for (int j = 0; j < sb; ++j) {
            const Vec3 qa = b[j], qb = b[(j + 1) % nb];
            Vec3 P;
            if (!geo::segSegIntersect(pa, pb, qa, qb, P)) continue;
            const Vec3 d1 = pb - pa, d2 = qb - qa;
            if (std::fabs(d1.x * d2.y - d1.y * d2.x) < 1e-9) continue;  // 近共线=重合边界
            bool dup = false;
            for (const auto& h : hits) if (h.distTo(P) <= tol) { dup = true; break; }
            if (!dup) hits.push_back(P);
        }
    }
    return hits;
}

namespace {

constexpr double kPi = 3.14159265358979323846;

// 弦高误差容差(mm)：折线到真实曲线的最大偏差不超过该值。
// 0.05mm 远小于 GB 细实线线宽与常见作图精度，保证"不溢出真实边界"。
constexpr double kChordTol = 0.05;

// ---- 弦高误差自适应离散 ----------------------------------------------------
// 递归细分参数区间 [t0,t1]，采样 1/4、1/2、3/4 三点检查其到弦 p0→p1 的最大偏离；
// 三处都 ≤ tol 即认为该段已足够平直，否则对半细分。depth 上限防止病态输入爆栈。
void refineRecursive(const std::function<Vec3(double)>& f,
                     double t0, double t1, const Vec3& p0, const Vec3& p1,
                     double tol, int depth, int maxDepth, std::vector<Vec3>& out) {
    if (depth >= maxDepth) { out.push_back(p1); return; }
    const double d = t1 - t0;
    const Vec3 pm  = f(t0 + d * 0.5);
    const Vec3 pq1 = f(t0 + d * 0.25);
    const Vec3 pq3 = f(t0 + d * 0.75);
    const double dev = std::max(math::pointLineDist(pq1, p0, p1),
                                std::max(math::pointLineDist(pm, p0, p1),
                                         math::pointLineDist(pq3, p0, p1)));
    if (dev <= tol) { out.push_back(p1); return; }
    refineRecursive(f, t0, t0 + d * 0.5, p0, pm, tol, depth + 1, maxDepth, out);
    refineRecursive(f, t0 + d * 0.5, t1, pm, p1, tol, depth + 1, maxDepth, out);
}

// 对参数曲线 f:[0,1]->R³ 做自适应离散。seedSegs 先做等分"种子"再逐段细分——
// 对闭合曲线（首尾同点）必须 seedSegs ≥ 2，否则顶层弦退化会让递归永不到达精度。
std::vector<Vec3> sampleParametricAdaptive(const std::function<Vec3(double)>& f,
                                           double tol, int seedSegs, int maxDepth) {
    std::vector<Vec3> out;
    if (seedSegs < 1) seedSegs = 1;
    std::vector<Vec3> seeds(seedSegs + 1);
    for (int i = 0; i <= seedSegs; ++i) seeds[i] = f((double)i / seedSegs);
    out.push_back(seeds[0]);
    for (int i = 0; i < seedSegs; ++i)
        refineRecursive(f, (double)i / seedSegs, (double)(i + 1) / seedSegs,
                        seeds[i], seeds[i + 1], tol, 0, maxDepth, out);
    return out;
}

std::vector<Vec3> sampleCircleAdaptive(const Vec3& c, double r, double chordTol) {
    auto f = [&](double t) {
        const double a = 2.0 * kPi * t;
        return Vec3(c.x + r * std::cos(a), c.y + r * std::sin(a), c.z);
    };
    return sampleParametricAdaptive(f, chordTol, 8, 12);
}

// 世界角约定：点 = 圆心 + r·(cos a, sin a)，逆时针为正（Qt 角 = −世界角，见 GraphicsView）。
std::vector<Vec3> sampleArcAdaptive(const Vec3& c, double r, double a0Deg, double a1Deg,
                                    double chordTol) {
    const double a0 = a0Deg * kPi / 180.0, a1 = a1Deg * kPi / 180.0;
    auto f = [&](double t) {
        const double a = a0 + (a1 - a0) * t;
        return Vec3(c.x + r * std::cos(a), c.y + r * std::sin(a), c.z);
    };
    int seeds = (int)std::ceil(std::fabs(a1Deg - a0Deg) / 45.0);
    if (seeds < 1) seeds = 1;
    return sampleParametricAdaptive(f, chordTol, seeds, 12);
}

std::vector<Vec3> sampleEllipseAdaptive(const Vec3& c, double a, double b, double rotDeg,
                                        double chordTol) {
    const double rad = rotDeg * kPi / 180.0;
    const double cr = std::cos(rad), sr = std::sin(rad);
    auto f = [&](double t) {
        const double ang = 2.0 * kPi * t;
        const double lx = a * std::cos(ang), ly = b * std::sin(ang);
        return Vec3(c.x + lx * cr - ly * sr, c.y + lx * sr + ly * cr, c.z);
    };
    return sampleParametricAdaptive(f, chordTol, 8, 12);
}

// 均匀 Catmull-Rom（张力 0.5）在参数 t∈[0,1] 处的取值，与 geo::catmullRomToBezier 同一条曲线。
Vec3 catmullRomEval(const std::vector<Vec3>& pts, double t) {
    const int n = (int)pts.size();
    if (n == 0) return Vec3();
    if (n == 1) return pts[0];
    const int segs = n - 1;
    double s = t * segs;
    int i = (int)std::floor(s);
    if (i < 0) i = 0;
    if (i > segs - 1) i = segs - 1;
    const double u = std::min(1.0, std::max(0.0, s - i));
    const Vec3& p0 = pts[std::max(i - 1, 0)];
    const Vec3& p1 = pts[i];
    const Vec3& p2 = pts[i + 1];
    const Vec3& p3 = pts[std::min(i + 2, n - 1)];
    const Vec3 c1 = p1 + (p2 - p0) * (1.0 / 6.0);
    const Vec3 c2 = p2 - (p3 - p1) * (1.0 / 6.0);
    const double mu = 1.0 - u;
    const double w0 = mu * mu * mu, w1 = 3 * mu * mu * u, w2 = 3 * mu * u * u, w3 = u * u * u;
    return Vec3(w0 * p1.x + w1 * c1.x + w2 * c2.x + w3 * p2.x,
                w0 * p1.y + w1 * c1.y + w2 * c2.y + w3 * p2.y,
                w0 * p1.z + w1 * c1.z + w2 * c2.z + w3 * p2.z);
}

std::vector<Vec3> sampleSplineAdaptive(const std::vector<Vec3>& pts, double chordTol) {
    if (pts.size() < 2) return pts;
    auto f = [&](double t) { return catmullRomEval(pts, t); };
    return sampleParametricAdaptive(f, chordTol, (int)pts.size() - 1, 12);
}

// 闭合折线（可含弧段）的自适应采样：按段输出，不含与首点重复的收尾点。
std::vector<Vec3> sampleClosedPolylineAdaptive(const Element& e, double chordTol) {
    std::vector<Vec3> out;
    const int n = (int)e.pts.size();
    for (int i = 0; i < n; ++i) {
        const bool isArc = (i < (int)e.arcFlags.size()) && e.arcFlags[i];
        if (isArc) {
            const geo::ArcSeg ar = geo::arcThrough(e.pts, i, true);
            if (ar.valid) {
                auto poly = sampleArcAdaptive(ar.center, ar.radius,
                                              ar.startDeg, ar.startDeg + ar.sweepDeg, chordTol);
                // 丢弃末点（≈ 下一段起点），避免重复点
                for (size_t k = 0; k + 1 < poly.size(); ++k) out.push_back(poly[k]);
                continue;
            }
        }
        out.push_back(e.pts[i]);
    }
    return out;
}

// ---- 退化 / 有效性判定 -----------------------------------------------------
// 有向面积（鞋带公式）的两倍。点不足 3 个返回 0。
double polygonArea2(const std::vector<Vec3>& poly) {
    const size_t n = poly.size();
    if (n < 3) return 0.0;
    double a2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % n];
        a2 += a.x * b.y - b.x * a.y;
    }
    return a2;
}

bool loopIsValid(const std::vector<Vec3>& poly) {
    if (poly.size() < 3) return false;
    const double area = std::fabs(polygonArea2(poly)) * 0.5;
    if (area < 1e-6) return false;              // 零面积 → 退化，跳过
    return true;
}

// ---- 段（Edge）与图元拆解 --------------------------------------------------
// 一段可参与首尾拼接的边界段：poly 为从 a 到 b 的采样点列（含两端）。
struct Edge {
    Vec3 a, b;
    std::vector<Vec3> poly;
    int srcId = 0;
};

// 把图元拆成开放段。返回 false 表示该图元不能作为边界段（标注/文本/约束/已闭合图元…）。
bool decomposeToEdges(const Element& e, double chordTol, std::vector<Edge>& out) {
    switch (e.type) {
        case ElementType::Line: {
            if (e.pts.size() < 2) return false;
            out.push_back(Edge{ e.pts[0], e.pts[1], { e.pts[0], e.pts[1] }, e.id });
            return true;
        }
        case ElementType::Arc: {
            if (e.pts.empty() || e.radius <= 1e-9) return false;
            const double sweep = e.endAngle - e.startAngle;
            if (std::fabs(sweep) < 1e-9) return false;      // 零扫角 → 退化
            auto poly = sampleArcAdaptive(e.pts[0], e.radius, e.startAngle, e.endAngle, chordTol);
            if (poly.size() < 2) return false;
            Edge ed; ed.a = poly.front(); ed.b = poly.back();
            ed.poly = std::move(poly); ed.srcId = e.id;
            out.push_back(std::move(ed));
            return true;
        }
        case ElementType::Polyline: {
            if (e.pts.size() < 2) return false;
            const int nSegs = (int)e.pts.size() - 1;        // 开放折线
            for (int i = 0; i < nSegs; ++i) {
                const bool isArc = (i < (int)e.arcFlags.size()) && e.arcFlags[i];
                if (isArc) {
                    const geo::ArcSeg ar = geo::arcThrough(e.pts, i, false);
                    if (ar.valid) {
                        auto poly = sampleArcAdaptive(ar.center, ar.radius,
                                                      ar.startDeg, ar.startDeg + ar.sweepDeg, chordTol);
                        if (poly.size() >= 2) {
                            Edge ed; ed.a = poly.front(); ed.b = poly.back();
                            ed.poly = std::move(poly); ed.srcId = e.id;
                            out.push_back(std::move(ed));
                            continue;
                        }
                    }
                }
                out.push_back(Edge{ e.pts[i], e.pts[i + 1],
                                    { e.pts[i], e.pts[i + 1] }, e.id });
            }
            return !out.empty();
        }
        case ElementType::Polygon: {
            if (e.closed || e.pts.size() < 2) return false;  // 闭合多边形走自闭合路径
            for (size_t i = 0; i + 1 < e.pts.size(); ++i)
                out.push_back(Edge{ e.pts[i], e.pts[i + 1], { e.pts[i], e.pts[i + 1] }, e.id });
            return !out.empty();
        }
        case ElementType::Spline: {
            if (e.pts.size() < 2) return false;
            auto poly = sampleSplineAdaptive(e.pts, chordTol);
            if (poly.size() < 2) return false;
            Edge ed; ed.a = poly.front(); ed.b = poly.back();
            ed.poly = std::move(poly); ed.srcId = e.id;
            out.push_back(std::move(ed));
            return true;
        }
        default:
            return false;
    }
}

// 把一段（poly：a→b 的采样）接到链尾。reverse=true 表示沿 poly 反向拼接（b→a）。
void appendEdge(std::vector<Vec3>& chain, const std::vector<Vec3>& poly, bool reverse) {
    const size_t m = poly.size();
    if (m < 2) return;
    if (!reverse) {
        for (size_t k = 1; k < m; ++k) chain.push_back(poly[k]);      // p1..pm
    } else {
        for (size_t k = m - 1; k > 0; --k) chain.push_back(poly[k - 1]);  // p(m-1)..p0
    }
}

// 把一段接到链头。reverse=true 表示沿 poly 反向拼接。
void prependEdge(std::vector<Vec3>& chain, const std::vector<Vec3>& poly, bool reverse) {
    const size_t m = poly.size();
    if (m < 2) return;
    std::vector<Vec3> pre;
    pre.reserve(m + chain.size());
    if (!reverse) {
        for (size_t k = 0; k + 1 < m; ++k) pre.push_back(poly[k]);    // p0..p(m-2)
    } else {
        for (size_t k = m - 1; k > 0; --k) pre.push_back(poly[k]);    // p(m-1)..p1
    }
    pre.insert(pre.end(), chain.begin(), chain.end());
    chain.swap(pre);
}

// 详情：一个已建成的闭环 + 构成它的源图元 id（按拼接顺序，去重）。
struct LoopBuildResult {
    std::vector<std::vector<Vec3>> loops;
    std::vector<std::vector<int>>  loopSrcIds;
    SectionClosureStats stats;
};

// 自身闭合图元 → 边界点列（真实判定闭合，不以点数冒充）。out 未重复首尾。
bool sectionSelfClosedOutline(const Element& e, std::vector<Vec3>& out,
                              double chordTol, double closureTol) {
    out.clear();
    switch (e.type) {
        case ElementType::Circle: {
            if (e.pts.empty() || e.radius <= 1e-9) return false;
            out = sampleCircleAdaptive(e.pts[0], e.radius, chordTol);
            return out.size() >= 3;
        }
        case ElementType::Ellipse: {
            if (e.pts.empty() || e.radius <= 1e-9 || e.radiusY <= 1e-9) return false;
            out = sampleEllipseAdaptive(e.pts[0], e.radius, e.radiusY, e.rotation, chordTol);
            return out.size() >= 3;
        }
        case ElementType::Rectangle: {
            if (e.pts.size() < 2) return false;
            const Vec3 a = e.pts[0], b = e.pts[1];
            if (std::fabs(a.x - b.x) < 1e-9 || std::fabs(a.y - b.y) < 1e-9) return false;  // 退化矩形
            out = { Vec3(a.x, a.y), Vec3(b.x, a.y), Vec3(b.x, b.y), Vec3(a.x, b.y) };
            return true;
        }
        case ElementType::Polygon: {
            if (e.pts.size() < 3 || !e.closed) return false;
            out = e.pts;
            return out.size() >= 3;
        }
        case ElementType::Polyline: {
            if (e.pts.size() < 3 || !e.closed) return false;
            out = sampleClosedPolylineAdaptive(e, chordTol);
            return out.size() >= 3;
        }
        case ElementType::Spline: {
            if (e.pts.size() < 2) return false;
            out = sampleSplineAdaptive(e.pts, chordTol);
            if (out.size() >= 2 && out.front().distTo(out.back()) <= closureTol) {
                out.pop_back();          // 去掉与首点重合的末点，保持"未重复"约定
                return out.size() >= 3;
            }
            return false;                // 开放样条：不构成剖面区域（修复 D1 渗漏）
        }
        case ElementType::Arc: {
            if (e.pts.empty() || e.radius <= 1e-9) return false;
            const double sweep = e.endAngle - e.startAngle;
            if (std::fabs(std::fabs(sweep) - 360.0) > 1e-6) return false;   // 仅整圆算闭合
            out = sampleCircleAdaptive(e.pts[0], e.radius, chordTol);
            return out.size() >= 3;
        }
        case ElementType::Hatch: {
            if (e.pts.size() < 3) return false;
            out = e.pts;                  // 已是剖面区域，沿用其外边界
            return true;
        }
        default:
            return false;
    }
}

// ===================== 边界交叉细分（平面细分）=====================
// 把"参与成环的边界"统一表示为采样折线原始边；自闭合图元额外标记 closed。
// 细分规则（见 buildLoopsInternal 交叉检测）：开放边被任意边界穿过即细分；
// 自闭合边仅在被【开放图元边界】穿过时才细分（避免把相邻/重叠的闭合区域误拆成并集面）。
struct RawEdge {
    std::vector<Vec3> poly;   // 采样点列（闭合边不含重复首点）
    int srcId = 0;
    bool closed = false;
};

// 把图元集合收集为原始边界：自闭合图元 → closed 原始边；其余拆成开放原始边。
void collectRawEdges(const std::vector<Element>& elems, double chordTol, double closureTol,
                    std::vector<RawEdge>& raws, SectionClosureStats& stats) {
    for (const auto& e : elems) {
        std::vector<Vec3> outline;
        if (sectionSelfClosedOutline(e, outline, chordTol, closureTol)) {
            RawEdge re;
            re.poly = std::move(outline);
            re.srcId = e.id;
            re.closed = true;
            raws.push_back(std::move(re));
        } else {
            std::vector<Edge> ed;
            if (decomposeToEdges(e, chordTol, ed)) {
                for (auto& x : ed) {
                    RawEdge re;
                    re.poly = std::move(x.poly);
                    re.srcId = x.srcId;
                    re.closed = false;
                    raws.push_back(std::move(re));
                }
            } else {
                ++stats.openElements;     // 非边界图元（标注/文本/约束/…）
            }
        }
    }
}

// 原始边的几何包围盒（含闭合段 v.back()→v.front()）。
QRectF rawEdgeBBox(const RawEdge& e) {
    return bboxOfPoints(e.poly);
}

// 交点是否落在某原始边的任一采样顶点上（含闭合段终点＝首点）。
// 用途：端点已重合（T 接 / 共端点，距离 ≤ tol）的情况不切分（避免零长子段）。
bool nearAnyVertex(const RawEdge& e, const Vec3& p, double tol) {
    for (const auto& v : e.poly) if (v.distTo(p) <= tol) return true;
    return false;
}

// 把一条原始边界按交点（所在段索引 + 交点坐标）切分为若干子折线段。
// 保留原采样点列，交点插入相邻顶点之间；重复交点（≤1e-6）去重。
// 闭合边在内部按 v.back()→v.front() 补一段闭合段处理；切分后首尾重合点由贪心链式成环闭合。
std::vector<std::vector<Vec3>> splitRawEdgeSubPolys(
        const RawEdge& e, const std::vector<std::pair<int, Vec3>>& splits) {
    const std::vector<Vec3>& v = e.poly;
    const int n = (int)v.size();
    if (n < 2) return {};
    const int nSeg = e.closed ? n : n - 1;        // 段数（含闭合段）
    std::map<int, std::vector<Vec3>> bySeg;
    for (const auto& s : splits) {
        if (s.first < 0 || s.first >= nSeg) continue;
        bySeg[s.first].push_back(s.second);
    }
    std::vector<std::vector<Vec3>> subs;
    std::vector<Vec3> cur;
    cur.push_back(v[0]);
    for (int k = 0; k < nSeg; ++k) {
        const Vec3& a = v[k];
        const Vec3& b = (k + 1 < n) ? v[k + 1] : v[0];   // 闭合段：v[n-1]→v[0]
        auto it = bySeg.find(k);
        if (it != bySeg.end()) {
            std::vector<Vec3>& ps = it->second;
            std::sort(ps.begin(), ps.end(), [&](const Vec3& p, const Vec3& q) {
                return p.distTo(a) < q.distTo(a);
            });
            for (const Vec3& p : ps) {
                if (cur.empty() || cur.back().distTo(p) > 1e-6) {
                    cur.push_back(p);
                    if (cur.size() >= 2) subs.push_back(cur);
                    cur.clear();
                    cur.push_back(p);
                }
            }
        }
        if (cur.empty()) cur.push_back(b);
        else if (cur.back().distTo(b) > 1e-6) cur.push_back(b);
    }
    if (cur.size() >= 2) subs.push_back(cur);
    return subs;
}

// 核心：图元集合 → 闭环 + 源 id + 统计。
// 在原有"端点容差贪心链式成环"之前加入"边界交叉细分"阶段：
//   1) 统一收集原始边界；
//   2) 对所有边界采样线段两两求交（包围盒预筛），把交点插入相交双方的点列并切分为子边；
//   3) 未被开放图元穿越的自闭合图元仍直接成整体环（保持现状，避免回归）；
//   4) 被切分的自闭合边 + 全部开放边 → 细分后的子边参与贪心链式成环，
//      分隔线两侧各自成环，loopSrcIds 沿子边 srcId 汇总去重。
LoopBuildResult buildLoopsInternal(const std::vector<Element>& elems, double tol) {
    LoopBuildResult res;

    // 1) 收集原始边界（自闭合 → closed；其余 → 开放）
    std::vector<RawEdge> raws;
    collectRawEdges(elems, kChordTol, tol, raws, res.stats);

    // 2) 边界交叉检测：两两求交（包围盒预筛），记录交点（所在段索引 + 坐标）。
    std::vector<std::vector<std::pair<int, Vec3>>> splits(raws.size());
    for (size_t i = 0; i + 1 < raws.size(); ++i) {
        const QRectF bi = rawEdgeBBox(raws[i]);
        const bool iOpen = !raws[i].closed;
        const int ni = (int)raws[i].poly.size();
        const int segi = raws[i].closed ? ni : ni - 1;
        for (size_t j = i + 1; j < raws.size(); ++j) {
            if (!bi.intersects(rawEdgeBBox(raws[j]))) continue;   // 包围盒预筛
            const bool jOpen = !raws[j].closed;
            const int nj = (int)raws[j].poly.size();
            const int segj = raws[j].closed ? nj : nj - 1;
            // 细分触发：开放边参与任意细分；自闭合边仅被开放边穿过时细分。
            const bool iCanSplit = iOpen || jOpen;
            const bool jCanSplit = jOpen || iOpen;
            for (int si = 0; si < segi; ++si) {
                const Vec3& p1 = raws[i].poly[si];
                const Vec3& p2 = (si + 1 < ni) ? raws[i].poly[si + 1] : raws[i].poly[0];
                for (int sj = 0; sj < segj; ++sj) {
                    const Vec3& q1 = raws[j].poly[sj];
                    const Vec3& q2 = (sj + 1 < nj) ? raws[j].poly[sj + 1] : raws[j].poly[0];
                    Vec3 out;
                    if (!geo::segSegIntersect(p1, p2, q1, q2, out)) continue;
                    // 端点已重合（T 接 / 共端点）不切分（避免零长子段）
                    if (iCanSplit && !nearAnyVertex(raws[i], out, tol))
                        splits[i].push_back({ si, out });
                    if (jCanSplit && !nearAnyVertex(raws[j], out, tol))
                        splits[j].push_back({ sj, out });
                }
            }
        }
    }

    // 3) 未穿越的自闭合图元：直接成整体环（保持现状，避免回归）
    std::vector<char> rawUsed(raws.size(), 0);
    for (size_t i = 0; i < raws.size(); ++i) {
        if (raws[i].closed && splits[i].empty()) {
            if (loopIsValid(raws[i].poly)) {
                res.loops.push_back(raws[i].poly);
                res.loopSrcIds.push_back({ raws[i].srcId });
                ++res.stats.selfClosedRegions;
            } else {
                ++res.stats.degenerate;
            }
            rawUsed[i] = 1;
        }
    }

    // 4) 其余（开放边 + 被切分的自闭合边）→ 细分后化为子边，参与链式成环
    std::vector<Edge> edges;
    for (size_t i = 0; i < raws.size(); ++i) {
        if (raws[i].closed && splits[i].empty()) continue;   // 已整体成环
        auto subs = splitRawEdgeSubPolys(raws[i], splits[i]);
        for (auto& sp : subs) {
            if (sp.size() < 2) continue;
            Edge ed;
            ed.a = sp.front();
            ed.b = sp.back();
            ed.poly = std::move(sp);
            ed.srcId = raws[i].srcId;
            edges.push_back(std::move(ed));
        }
    }

    // 5) 贪心链式成环：任取未用段作种子，向两端扩展到不能再接，再判闭合。
    std::vector<char> used(edges.size(), 0);
    for (size_t s = 0; s < edges.size(); ++s) {
        if (used[s]) continue;
        std::vector<Vec3> chain = edges[s].poly;
        std::vector<int> ids = { edges[s].srcId };
        used[s] = 1;

        bool grew = true;
        while (grew) {
            grew = false;
            // 尾部优先
            for (size_t k = 0; k < edges.size(); ++k) {
                if (used[k]) continue;
                if (edges[k].a.distTo(chain.back()) <= tol) {
                    appendEdge(chain, edges[k].poly, false);
                    ids.push_back(edges[k].srcId); used[k] = 1; grew = true; break;
                }
                if (edges[k].b.distTo(chain.back()) <= tol) {
                    appendEdge(chain, edges[k].poly, true);
                    ids.push_back(edges[k].srcId); used[k] = 1; grew = true; break;
                }
            }
            if (grew) continue;
            // 头部
            for (size_t k = 0; k < edges.size(); ++k) {
                if (used[k]) continue;
                if (edges[k].b.distTo(chain.front()) <= tol) {
                    prependEdge(chain, edges[k].poly, false);
                    ids.insert(ids.begin(), edges[k].srcId); used[k] = 1; grew = true; break;
                }
                if (edges[k].a.distTo(chain.front()) <= tol) {
                    prependEdge(chain, edges[k].poly, true);
                    ids.insert(ids.begin(), edges[k].srcId); used[k] = 1; grew = true; break;
                }
            }
        }

        // 闭合判定：链首尾端点距离 ≤ tol（真实闭合），且区域非退化。
        if (chain.size() >= 3 && chain.front().distTo(chain.back()) <= tol) {
            chain.pop_back();             // 去掉与首点重合的收尾点
            if (loopIsValid(chain)) {
                // 源 id 去重（同一图元可能贡献多段）
                std::vector<int> uniq;
                for (int id : ids)
                    if (std::find(uniq.begin(), uniq.end(), id) == uniq.end()) uniq.push_back(id);
                res.loops.push_back(std::move(chain));
                res.loopSrcIds.push_back(std::move(uniq));
                ++res.stats.chainedRegions;
            } else {
                ++res.stats.degenerate;
            }
        } else {
            ++res.stats.openChains;       // 未闭合 → 安全跳过（不下游填充）
        }
    }
    return res;
}

// ============================================================================
// 平面细分成环（点选填充专用）
//
// 与 buildLoopsInternal 仅按"端点容差贪心拼接"不同，本实现在成环**之前**把所有
// 边界段(含自闭合图元的采样轮廓)在"两两交点"处剪开，再从细分后的子段重建全部面
// (face)。这样，一条穿过多边形内部的边界线会自然成为区域分界线，其两侧各自成环——
// 这正是"点选圆外任意区域 / 竖线分隔出的单侧"两类诉求的数学基础。
//
// 不变量：
//   * 端点相接但不交叉的相邻图元(如两矩形共边)不产生 transverse 交点 → 保持旧行为；
//   * 仅当交点严格落在双方线段内部(距两端点 > tol)且非近共线(非重合)时才剪开；
//   * 自相交(8 字形)净面积≈0 → 该面退化为负向/极小面积 → 由面积阈值与"外立面剔除"双重丢弃。
// ============================================================================

namespace {

// 一条参与成环的"原始边界折线"(开放点列，closed=true 表示首尾当作一段相接)。
struct RawPoly {
    std::vector<Vec3> pts;
    int               srcId = 0;
    bool              closed = false;
};

// 把图元集合收集为原始边界折线：自闭合图元→一条封闭折线(轮廓采样)；其余→按段拆开。
std::vector<RawPoly> gatherBoundaryPolysForSubdivide(const std::vector<Element>& elems) {
    std::vector<RawPoly> out;
    for (const auto& e : elems) {
        std::vector<Vec3> outline;
        if (sectionSelfClosedOutline(e, outline, kChordTol, kDefaultClosureTol)) {
            out.push_back(RawPoly{ std::move(outline), e.id, true });
            continue;
        }
        std::vector<Edge> ed;
        if (decomposeToEdges(e, kChordTol, ed))
            for (auto& x : ed)
                out.push_back(RawPoly{ std::move(x.poly), x.srcId, false });
    }
    return out;
}

// 在折线各段中插入落在段内部的分裂点(按投影位置排序，去重、剔除端点附近点)。
std::vector<Vec3> rebuildWithSplits(const std::vector<Vec3>& orig, const std::vector<Vec3>& sp,
                                    bool closed, double tol) {
    std::vector<Vec3> out;
    const int m = (int)orig.size();
    const int segCount = closed ? m : m - 1;
    for (int k = 0; k < segCount; ++k) {
        const Vec3 a = orig[k], b = orig[(k + 1) % m];
        if (out.empty() || out.back().distTo(a) > tol) out.push_back(a);
        const Vec3 dir = b - a;
        const double LL = dir.x * dir.x + dir.y * dir.y;
        const double L = std::sqrt(LL);
        if (L <= tol) continue;
        std::vector<std::pair<double, Vec3>> on;
        for (const auto& s : sp) {
            if (geo::distPointSegment(s, a, b) > tol) continue;
            if (s.distTo(a) <= tol || s.distTo(b) <= tol) continue;  // 仅取段内部交点
            const double tproj = ((s.x - a.x) * dir.x + (s.y - a.y) * dir.y) / LL;
            on.push_back({ tproj, s });
        }
        std::sort(on.begin(), on.end(),
                  [](const std::pair<double, Vec3>& a, const std::pair<double, Vec3>& b) {
                      return a.first < b.first;
                  });
        for (auto& pr : on)
            if (out.empty() || out.back().distTo(pr.second) > tol) out.push_back(pr.second);
    }
    if (!closed && !out.empty() && out.back().distTo(orig.back()) > tol) out.push_back(orig.back());
    return out;
}

// 对所有原始折线路两两求 transverse 交点，并在交点处剪开双方(顶点去重)。
void splitPolysAtIntersections(std::vector<RawPoly>& polys, double tol) {
    const int N = (int)polys.size();
    std::vector<std::vector<Vec3>> splits(N);
    for (int i = 0; i < N; ++i) {
        const QRectF bi = bboxOfPoints(polys[i].pts, tol);
        for (int j = i + 1; j < N; ++j) {
            if (!bi.intersects(bboxOfPoints(polys[j].pts, tol))) continue;
            const int ni = (int)polys[i].pts.size();
            const int si = polys[i].closed ? ni : ni - 1;
            const int nj = (int)polys[j].pts.size();
            const int sj = polys[j].closed ? nj : nj - 1;
            for (int a = 0; a < si; ++a) {
                const Vec3 pa = polys[i].pts[a], pb = polys[i].pts[(a + 1) % ni];
                for (int b = 0; b < sj; ++b) {
                    const Vec3 qa = polys[j].pts[b], qb = polys[j].pts[(b + 1) % nj];
                    Vec3 P;
                    if (!geo::segSegIntersect(pa, pb, qa, qb, P)) continue;
                    // 仅近垂线(transverse)交点才剪开；近共线=重合边界(如共边矩形)跳过
                    const Vec3 d1 = pb - pa, d2 = qb - qa;
                    if (std::fabs(d1.x * d2.y - d1.y * d2.x) < 1e-9) continue;
                    // 端点判定与 transverseHit 保持一致：仅当交点处于双方端点(普通顶点相接)
                    // 才跳过切分；一方端点在交点、另一方在内部穿过(分隔线端点落在边界上)
                    // 必须切分，否则竖线端点落在矩形边上时无法把区域切开只填一侧。
                    const bool nearI = (P.distTo(pa) <= tol || P.distTo(pb) <= tol);
                    const bool nearJ = (P.distTo(qa) <= tol || P.distTo(qb) <= tol);
                    if (nearI && nearJ) continue;
                    splits[i].push_back(P);
                    splits[j].push_back(P);
                }
            }
        }
    }
    for (int i = 0; i < N; ++i) {
        if (splits[i].empty()) continue;
        std::vector<Vec3> uniq;
        for (const auto& p : splits[i]) {
            bool dup = false;
            for (const auto& q : uniq) if (p.distTo(q) <= tol) { dup = true; break; }
            if (!dup) uniq.push_back(p);
        }
        polys[i].pts = rebuildWithSplits(polys[i].pts, uniq, polys[i].closed, tol);
    }
}

} // anonymous namespace

// 平面细分 → 全部面(face)边界。subdivide=true 时先剪开交点再建图；false 时等价于
// 旧端点拼接语义(保持 buildClosedLoopsFromElements / makeSectionChangesEx 既有契约)。
//
// 关键设计：仅当某图元的边界被"其他边界线"在内部(transverse)穿过时才把它投入细分图；
// 未被穿过的自闭合图元(独立圆/矩形、共边相邻矩形)仍走"直接成环"——既保留旧路径行为
// (相邻矩形不会因重合边被错误合并)，又只把真正需要细分的交叉场景交给图遍历解决。
LoopBuildResult buildLoopsSubdivided(const std::vector<Element>& elems, double tol, bool subdivide) {
    LoopBuildResult res;
    std::vector<RawPoly> polys = gatherBoundaryPolysForSubdivide(elems);

    // 统一闭合边界为逆时针(CCW)定向。半边 next 规则对任意定向的输入都能正确提取
    // 全部面（有向边被各面互斥划分，方向只影响单个面的绕行方向，不影响面集合），
    // 但统一 CCW 可让"受限子区域必为正向环绕、无界外立面为反向环绕"这一性质稳定
    // 成立，便于下游做方向相关的断言与调试；开放折线(直线/开放路径)无定向概念，
    // 保持不变。定向规范化放在求交/剪开之前，不影响交点判定(线段相交与方向无关)。
    // （历史注：异常 A2——CW 输入的菱形被当整体——的真正根因是旧"最小正向转角"
    // 启发式只走得出 CW 面；半边规则修复后此处归一化为防御性保留。）
    for (auto& poly : polys) {
        if (!poly.closed || poly.pts.size() < 3) continue;
        if (polygonArea2(poly.pts) < 0.0) std::reverse(poly.pts.begin(), poly.pts.end());
    }

    // 一对边界折线是否发生 transverse(非近共线)内部交叉——即"被穿过"而构成真实分区。
    // 一个闭环只有被另一条边界在 ≥2 个不同交点处穿过才构成分区(T 接 / 贯穿)；仅 1 个交点
    // （两图元只共一角点 / 单点相切）不构成分区，仍安全保持旧行为。分隔线端点【恰好落在
    // 多边形顶点】时也计入该 ≥2 计数（交点收集与共线排除见 collectTransverseHits 原语）。
    // 注意：垂直/水平直线的包围盒退化为零宽/零高矩形，QRectF::intersects 对零面积矩形
    // 返回 false，会漏掉"竖线穿过矩形"这类交叉 → 宽相位 bbox 须按 tol 外扩后再相交。
    auto transverseHit = [&](const RawPoly& a, const RawPoly& b) -> bool {
        if (!bboxOfPoints(a.pts, tol).intersects(bboxOfPoints(b.pts, tol))) return false;
        return (int)collectTransverseHits(a.pts, a.closed, b.pts, b.closed, tol).size() >= 2;
    };

    // 两条「闭合」边界是否共享一段共线重叠边(seam)。相邻矩形共用一条边时，两条边界在该边
    // 两端各产生一个「端点—端点」接触点；transverseHit 会把这两个纯角点接触算成 2 个非共线
    // 交点，从而把两个本应各自独立的相邻区域误判为"被穿过"并投入细分图 → 面遍历把二者的
    // 并集当成一整块(症状：点右侧区域却命中 (左∪右) 的并集轮廓，且左区域不再变成 Hatch)。
    // 与旧注释"共边相邻矩形仍走直接成环"的既定语义一致：共边相邻 → 不作 transverse 分区判定。
    auto sharesCollinearEdge = [&](const RawPoly& a, const RawPoly& b) -> bool {
        if (!a.closed || !b.closed) return false;
        const int na = (int)a.pts.size(), nb = (int)b.pts.size();
        for (int i = 0; i < na; ++i) {
            const Vec3 pa = a.pts[i], pb = a.pts[(i + 1) % na];
            const Vec3 d1 = pb - pa;
            const double L1 = std::sqrt(d1.x * d1.x + d1.y * d1.y);
            if (L1 <= 1e-12) continue;
            for (int j = 0; j < nb; ++j) {
                const Vec3 qa = b.pts[j], qb = b.pts[(j + 1) % nb];
                const Vec3 d2 = qb - qa;
                const double L2 = std::sqrt(d2.x * d2.x + d2.y * d2.y);
                if (L2 <= 1e-12) continue;
                if (std::fabs(d1.x * d2.y - d1.y * d2.x) > 1e-9 * L1 * L2) continue; // 不平行
                const Vec3 dq = qa - pa;
                if (std::fabs(d1.x * dq.y - d1.y * dq.x) > 1e-6 * L1) continue;      // 不共线
                const double L1sq = d1.x * d1.x + d1.y * d1.y;
                auto proj = [&](const Vec3& p) {
                    return ((p.x - pa.x) * d1.x + (p.y - pa.y) * d1.y) / L1sq;
                };
                double lo = std::min(proj(qa), proj(qb));
                double hi = std::max(proj(qa), proj(qb));
                lo = std::max(lo, 0.0);
                hi = std::min(hi, 1.0);
                if ((hi - lo) * L1 > tol) return true;   // 重叠长度 > 容差 → 视为共边 seam
            }
        }
        return false;
    };

    std::vector<char> crossed(polys.size(), 0);
    if (subdivide) {
        for (size_t i = 0; i < polys.size(); ++i)
            for (size_t j = i + 1; j < polys.size(); ++j) {
                if (sharesCollinearEdge(polys[i], polys[j])) continue;   // 共边相邻 → 非分区
                if (transverseHit(polys[i], polys[j])) { crossed[i] = 1; crossed[j] = 1; }
            }
    }

    // (1) 未被穿过的自闭合图元 → 直接成环(与旧路径一致，避免重合边误合并)。
    for (size_t i = 0; i < polys.size(); ++i) {
        if (polys[i].closed && !crossed[i]) {
            res.loops.push_back(polys[i].pts);
            res.loopSrcIds.push_back({ polys[i].srcId });
        }
    }

    // (2) 细分图：开放图元 + 被穿过的自闭合图元。
    std::vector<RawPoly> sub;
    for (size_t i = 0; i < polys.size(); ++i)
        if (!polys[i].closed || crossed[i]) sub.push_back(polys[i]);
    if (subdivide) splitPolysAtIntersections(sub, tol);
    if (sub.empty()) { res.stats.selfClosedRegions = (int)res.loops.size(); return res; }

    // 顶点合并(容差) → 不同图元在交点/共端点处共享同一顶点。
    // 顶点合并：在容差 tol 内取最近已存在的顶点(任意对齐均可合并，保证"缺口 ≤ 容差
    // 即视为闭合"的旧语义——网格取整会因缺口跨网格边界而漏合并)。区域点选图规模很小，
    // 线性扫描足够。
    std::vector<Vec3> vpts;
    auto getV = [&](const Vec3& p) -> int {
        for (int i = 0; i < (int)vpts.size(); ++i)
            if (vpts[i].distTo(p) <= tol) return i;
        const int idx = (int)vpts.size();
        vpts.push_back(p);
        return idx;
    };

    struct DirEdge { int from, to, srcId; bool used = false; };
    std::vector<DirEdge> edges;
    for (const auto& poly : sub) {
        const int m = (int)poly.pts.size();
        const int segCount = poly.closed ? m : m - 1;
        for (int k = 0; k < segCount; ++k) {
            const Vec3 a = poly.pts[k], b = poly.pts[(k + 1) % m];
            if (a.distTo(b) <= 1e-9) continue;
            const int ia = getV(a), ib = getV(b);
            if (ia == ib) continue;
            edges.push_back({ ia, ib, poly.srcId, false });
            edges.push_back({ ib, ia, poly.srcId, false });
        }
    }

    std::vector<std::vector<int>> adj(vpts.size());
    for (int e = 0; e < (int)edges.size(); ++e) adj[edges[e].from].push_back(e);

    // 修剪悬挂尾：开放图元的端点若只连一条边(图顶点度=1)，其延伸段并非真实区域边界，
    // 会裹挟出伪面(如竖线超出矩形的上下端、斜穿底边超出四边形的两端)。迭代删去所有
    // 度<2 的顶点及其邻边，直到无悬挂端。真实角点(度≥2，由多条边界相接)不受影响。
    auto removeEdge = [&](int ei) {
        if (edges[ei].used) return;
        edges[ei].used = true;
        const int v = edges[ei].from, w = edges[ei].to;
        adj[v].erase(std::remove(adj[v].begin(), adj[v].end(), ei), adj[v].end());
        for (int k = 0; k < (int)edges.size(); ++k) {           // 同时删去反向边
            if (!edges[k].used && edges[k].from == w && edges[k].to == v) {
                edges[k].used = true;
                adj[w].erase(std::remove(adj[w].begin(), adj[w].end(), k), adj[w].end());
                break;
            }
        }
    };
    bool trimChanged = true;
    while (trimChanged) {
        trimChanged = false;
        for (int v = 0; v < (int)vpts.size(); ++v) {
            if ((int)adj[v].size() >= 2) continue;             // 真实角点：保留
            std::vector<int> es = adj[v];
            for (int ei : es) { removeEdge(ei); trimChanged = true; }
        }
    }

    // 面遍历（半边 next 规则，见 docs/boundary_tracing_algorithm.md §14）：
    //   next(h) = h.twin 在 h.target 出边角序(升序)中的【顺时针紧邻】，即 (pos-1+k)%k。
    // 该规则在每个顶点上恒有后继（悬挂修剪保证度≥2），把全部有向边划分成若干互不相交
    // 的闭合环——每个面恰好被绕一次，不重不漏。
    //
    // 为什么不能用"相对来向取最小正向转角"的启发式（旧实现）：
    //   * 度=2 的普通顶点（曲线采样点、含冗余共线顶点的折线）处，若下一段相对来向
    //     右弯(d≤0)或共线直行(d≈0)，启发式无候选可选 → 只能 U 形折返(d=180°) →
    //     面被针尖往返(spike)撕碎、区域丢失，剩余边界被直线弦错误封口（表现为
    //     "填充多画一条直线/越界"，样条等采样曲线边界必现）；
    //   * 半边规则对这些情形一律给出唯一正确后继：直行顶点直行、右弯顶点跟随右弯、
    //     T/X 交叉处按角序贴左墙转入分隔线（与 §17 手推表逐项一致）。
    // 注意：next 只依赖【完整图】的角序（悬挂修剪后的全部未用边），与遍历进度
    // (used 标记)无关——有向边被各面互斥 partition，走面时取到的后继恒未用。
    for (int v = 0; v < (int)vpts.size(); ++v) {
        std::sort(adj[v].begin(), adj[v].end(), [&](int a, int b) {
            const Vec3& O = vpts[v];
            const Vec3& A = vpts[edges[a].to];
            const Vec3& B = vpts[edges[b].to];
            const double aa = std::atan2(A.y - O.y, A.x - O.x);
            const double bb = std::atan2(B.y - O.y, B.x - O.x);
            if (aa != bb) return aa < bb;
            return a < b;                       // 同角稳定序（重复/重叠边）
        });
    }

    // 面遍历：从每条未用有向边出发，按半边 next 规则连续前进，回到起点即闭环完成。
    for (int e0 = 0; e0 < (int)edges.size(); ++e0) {
        if (edges[e0].used) continue;
        std::vector<int> faceEdges;
        int cur = e0;
        while (true) {
            if (edges[cur].used) break;
            edges[cur].used = true;
            faceEdges.push_back(cur);
            const int w = edges[cur].to;
            const int twinE = cur ^ 1;          // 边成对添加(见上方 push_back ×2)：反边 = 异或 1
            const std::vector<int>& lst = adj[w];
            int pos = -1;
            for (int i = 0; i < (int)lst.size(); ++i)
                if (lst[i] == twinE) { pos = i; break; }
            if (pos < 0) break;                 // 反边不在角序中(数值异常) → 丢弃该面
            const int nxt = lst[(pos - 1 + (int)lst.size()) % (int)lst.size()];
            cur = nxt;
            if (cur == e0) break;               // 回到起点 → 闭环完成
        }
        if (faceEdges.empty()) continue;

        std::vector<Vec3> poly;
        poly.reserve(faceEdges.size());
        for (int ei : faceEdges) poly.push_back(vpts[edges[ei].from]);
        // 去重：剔除连续重复顶点(含首尾相接处"首点被重复一次"的表示)，避免顶点数虚高与
        // 零长边——否则后续 Hatch 生成 / 点在多边形内判定会出现不稳定边界问题。
        std::vector<Vec3> clean;
        clean.reserve(poly.size());
        for (const auto& v : poly) {
            if (!clean.empty() && clean.back().distTo(v) <= tol) continue;
            clean.push_back(v);
        }
        if (clean.size() >= 3 && clean.back().distTo(clean.front()) <= tol) clean.pop_back();
        if (clean.size() < 3) continue;             // 去重后不足三角形 → 退化面丢弃
        poly = std::move(clean);
        const double a2 = polygonArea2(poly);
        if (std::fabs(a2) * 0.5 < 1e-6) continue;   // 退化面丢弃

        std::vector<int> ids;
        for (int ei : faceEdges) {
            const int sid = edges[ei].srcId;
            if (std::find(ids.begin(), ids.end(), sid) == ids.end()) ids.push_back(sid);
        }
        res.loops.push_back(std::move(poly));
        res.loopSrcIds.push_back(std::move(ids));
    }

    // 剔除"无界外立面"：半边 next 规则对任意闭合图元会同时给出其"内面"(CCW，有界)与
    // "外面"(CW，无界)两个有向环——二者顶点重合、面积相等。无界外立面恒满足：
    //   (1) 带符号面积为负(CW)；
    //   (2) |面积| 达到全局最大（它包围全部有界区域，面积必不小于任一有界环）。
    // 故删除"负面积且 |面积|==全局最大"的环即可，且天然处理单一闭合图元的平局情形
    // （内/外两环面积相等，恰好删掉 CW 外环）。该准则不会误删任何有界区域
    // （有界环面积恒 ≤ 外立面），也保留嵌套内孔（CW 内孔面积必小于其包容环，达不到全局最大）。
    // 原"远区探针"判定仅能剔除自交/畸形外立面，对"干净简单多边形"外立面失效（远探针落在
    // 环外），故以面积准则为主、探针作防御性兜底。
    auto loopSignedArea = [](const std::vector<Vec3>& p) -> double {
        return polygonArea2(p) * 0.5;   // 带符号面积：>0 CCW(有界)，<0 CW(无界/内孔)
    };
    if (res.loops.size() > 1) {
        double maxAbs = 0.0;
        for (size_t i = 0; i < res.loops.size(); ++i)
            maxAbs = std::max(maxAbs, std::fabs(loopSignedArea(res.loops[i])));
        const QRectF bb = bboxOfPoints(vpts);
        const double diag = std::max(bb.width(), bb.height()) + 1.0;
        const Vec3 probe(bb.left() - 10.0 * diag, bb.top() - 10.0 * diag, 0.0);
        std::vector<std::vector<Vec3>> kept;
        std::vector<std::vector<int>>  keptIds;
        for (size_t i = 0; i < res.loops.size(); ++i) {
            const double a = loopSignedArea(res.loops[i]);
            bool drop = false;
            if (a < -1e-9 && std::fabs(a) >= maxAbs - 1e-9) drop = true;   // CW 且达全局最大 → 无界外立面
            if (geo::pointInPolygon(probe, res.loops[i])) drop = true;      // 含远区探针 → 外立面(防御兜底)
            if (drop) continue;
            kept.push_back(std::move(res.loops[i]));
            keptIds.push_back(std::move(res.loopSrcIds[i]));
        }
        res.loops = std::move(kept);
        res.loopSrcIds = std::move(keptIds);
    }
    res.stats.selfClosedRegions = (int)res.loops.size();
    return res;
}

} // namespace

// ============================ 对外接口实现 ============================

bool sectionOutline(const Element& e, std::vector<Vec3>& out) {
    return sectionSelfClosedOutline(e, out, kChordTol, kDefaultClosureTol);
}

std::vector<std::vector<Vec3>> buildClosedLoopsFromElements(
        const std::vector<Element>& elems, double tol, SectionClosureStats* stats) {
    LoopBuildResult r = buildLoopsInternal(elems, tol);
    if (stats) *stats = r.stats;
    return std::move(r.loops);
}

SectionRegion hatchRegionFromElement(const Element& e) {
    SectionRegion r;
    r.boundary = e.pts;
    r.holes = e.holes;
    // 角度复用 rotation；间距复用 radius（<=0 回落默认 3mm）
    r.style.angleDeg   = e.rotation;
    r.style.spacingMm  = (e.radius > 1e-9) ? e.radius : 3.0;
    r.style.thinWidthPx = (e.lineWidth > 1e-9) ? e.lineWidth : 0.6;
    r.style.exempt   = (SectionExempt)e.sectionExempt;
    r.style.material = (SectionMaterial)e.sectionMaterial;
    return r;
}

Element makeHatchElement(const SectionRegion& r, const QString& name) {
    Element e;
    e.type = ElementType::Hatch;
    e.name = name;
    e.pts = r.boundary;             // 外边界多边形
    e.holes = r.holes;              // 内孔（布尔减）
    e.rotation = r.style.angleDeg;  // 剖面线方向角
    e.radius = r.style.spacingMm;   // 剖面线间距(mm)
    e.sectionExempt = (int)r.style.exempt;
    e.sectionMaterial = (int)r.style.material;
    // 外轮廓（剖切边界）：默认粗实线（线宽 2.0，对应粗线档；单位随线宽组别体系，
    // 由渲染层按 cosmetic pen 落到屏幕）。剖面线本身是细实线，由渲染器以固定细线宽
    // 单独绘制，不随此处线宽变化——这样填充后外框不会变细。
    e.lineWidth = 2.0;
    e.lineWidthAuto = false;
    e.lineStyle = LineStyle::Solid;
    e.closed = true;
    e.id = 0;
    return e;
}

// 由区域几何构建"承载填充"的 Hatch 图元：设置 id，并继承源图元的颜色 / 图层 / 外轮廓
// 线宽（styleSrc 可为空 → 用默认样式）。makeSectionChangesEx 与 makeSectionChangeForPick
// 共用本函数，避免"装配 Hatch + 继承源样式"这段契约在两处各写一份而漂移。
static Element makeHatchForRegion(const SectionRegion& r, int id, const Element* styleSrc) {
    Element h = makeHatchElement(r, QStringLiteral("剖面区域"));
    h.id = id;
    if (styleSrc) {
        h.color         = styleSrc->color;
        h.layer         = styleSrc->layer;
        h.lineWidth     = styleSrc->lineWidth;
        h.lineWidthAuto = styleSrc->lineWidthAuto;
    }
    return h;
}

// 由材料决定剖面线族（GB/T 4457.5-2013 + 行业材料符号惯例）。
// 材料 → PAT 图案名（对应 hatch/ 目录下 .pat 文件的内部名或文件名，小写键）。
// 返回非空时，generateHatchLines 将改用该 PAT 定义生成剖面线（忽略内置 anglesDeg/spacingMm）。
// 当前仅 Wood 映射到 wood.pat（用户需求样例）；可在此按需扩展更多材料。
static QString patNameForMaterial(SectionMaterial m) {
    switch (m) {
    case SectionMaterial::Wood: return QStringLiteral("wood");  // 对应 hatch/wood.pat（首个图案）
    default: return QString();
    }
}

HatchPattern hatchPatternFor(const SectionStyle& style) {
    HatchPattern pat;
    pat.spacingMm = style.spacingMm;          // 间距统一尊重用户设定
    switch (style.material) {
    case SectionMaterial::General:           // 通用金属：与主要轮廓成 45° 的单族细实线
    case SectionMaterial::Steel:
        pat.anglesDeg = { style.angleDeg };
        break;
    case SectionMaterial::CastIron:          // 铸铁：45° + 135° 网状剖面线
    case SectionMaterial::NonMetal:          // 非金属：交叉网格线（4.7 相邻同符号非金属疏密区分）
    case SectionMaterial::Concrete:          // 混凝土：交叉线 + 碎石点阵
        pat.anglesDeg = { style.angleDeg, style.angleDeg + 90.0 };
        pat.dots = (style.material == SectionMaterial::Concrete);
        break;
    case SectionMaterial::Wood:              // 木材：顺纹水平线族（近似木纹）
        pat.anglesDeg = { 0.0 };
        break;
    case SectionMaterial::Glass:            // 玻璃：45° 长短画线（GB 表1，断续线）
        // Table 1：玻璃及供观察用的其他透明材料——45° 方向、由长/短画构成的断续线。
        pat.anglesDeg = { style.angleDeg };
        pat.dashes = true;
        pat.dashLongShort = true;           // 长 + 短交替
        break;
    case SectionMaterial::Liquid:           // 液体：液面以下水平短画线（GB 表1 注3，断续线）
        // Note 3：在图形中一般按可见轮廓绘制，液面以下画水平细实线（此处以断续水平短线表示）。
        pat.anglesDeg = { 0.0 };            // 水平
        pat.dashes = true;
        pat.dashLongShort = false;          // 均匀短画线
        break;
    case SectionMaterial::Aluminum:         // 铝（非铁金属）：与钢反向 45° 单族
        // GB 4.7：装配图中相邻金属零件剖面线方向应相反（或同向而间隔不等）。
        // 钢取 +45°，铝取其反向 −45°（即 135°），既符合标准又可在拾取填充时直观区分。
        pat.anglesDeg = { -style.angleDeg };
        break;
    case SectionMaterial::Plywood:          // 胶合板：交叉层（水平 + 垂直细线，示意多层胶合）
        pat.anglesDeg = { 0.0, 90.0 };
        break;
    case SectionMaterial::Brick:            // 砖墙：砌体图案（水平砌缝 + 错缝竖线）
        pat.brick = true;
        pat.brickStagger = true;
        pat.brickCourseMm = 30.0;
        pat.brickWidthMm  = 60.0;
        pat.brickIrregular = false;
        break;
    case SectionMaterial::Stone:            // 石材墙面：错缝块石（不规则砌缝，近似毛石）
        pat.brick = true;
        pat.brickStagger = true;
        pat.brickCourseMm = 40.0;
        pat.brickWidthMm  = 70.0;
        pat.brickIrregular = true;
        break;
    case SectionMaterial::Insulation:       // 保温层：点阵符号（无斜线族）
        pat.anglesDeg.clear();
        pat.dots = true;
        break;
    // —— 以下 6 项：GB/T 4457.5-2013 表1「剖面符号」其余材料（2026-09-22 扩展）——
    case SectionMaterial::CoilWinding:      // 线圈绕组元件：45° 细实线 + 点阵（表1）
        pat.anglesDeg = { style.angleDeg };
        pat.dots = true;
        break;
    case SectionMaterial::LaminatedSteel:   // 叠钢片：薄 45° 细实线（表1；与钢同族，靠更细线宽区分）
        pat.anglesDeg = { style.angleDeg };
        break;
    case SectionMaterial::Sand:             // 型砂/填砂/粉末冶金/砂轮/陶瓷刀片/硬质合金刀片：自由分布点阵
        pat.anglesDeg.clear();
        pat.dots = true;
        break;
    case SectionMaterial::Soil:             // 基础周围的泥土：稀疏点阵（表1）
        pat.anglesDeg.clear();
        pat.dots = true;
        pat.dotStepScale = 1.8;             // 比常规更稀疏
        break;
    case SectionMaterial::ReinforcedConcrete: // 钢筋混凝土：45° 线 + 点阵（钢筋示意用点，表1）
        pat.anglesDeg = { style.angleDeg };
        pat.dots = true;
        break;
        case SectionMaterial::Mesh:             // 筛网/过滤网等：交叉网格（表1）
        pat.anglesDeg = { 0.0, 90.0 };
        break;
    }
    // PAT 覆盖：若材料映射到 hatch/ 目录下的 PAT 图案，标记 patName 以改用 PAT 生成。
    const QString pn = patNameForMaterial(style.material);
    if (!pn.isEmpty()) pat.patName = pn;
    return pat;
}

// 生成砌体图案（砖墙/石材）：水平砌缝（每皮一道）+ 竖缝（按块宽错缝/伪随机）。
// 所有线段均超出区域包围盒，由 GraphicsView::paintHatch 的 setClipPath 裁剪到区域，
// 故此处只需按砌层高度 / 块宽铺满包围盒即可，无需 even-odd 抠内孔（painter 统一裁剪）。
static std::vector<geo::Seg> generateBrickLines(const SectionRegion& region, const HatchPattern& pat) {
    std::vector<geo::Seg> out;
    out.reserve(256);
    if (region.boundary.size() < 3) return out;
    const QRectF bb = bboxOfPoints(region.boundary);
    const double xmin = bb.left(), ymin = bb.top(), xmax = bb.right(), ymax = bb.bottom();
    const double H = std::max(4.0, pat.brickCourseMm);   // 砌层高（mm）
    const double W = std::max(4.0, pat.brickWidthMm);     // 单块宽（mm）

    // 水平砌缝：每皮一道（含顶/底边界外扩，避免首末皮被截断）。
    for (double y = ymin - H; y <= ymax + H + 1e-6; y += H)
        out.push_back(geo::Seg{ Vec3(xmin - W, y, 0), Vec3(xmax + W, y, 0) });

    // 竖缝：逐皮偏移错缝；石材模式用确定性哈希扰动竖缝位置，近似毛石/块石墙面。
    int course = 0;
    for (double y0 = ymin - H, y1 = ymin; y1 <= ymax + H + 1e-6; y0 += H, y1 += H, ++course) {
        const double off = (pat.brickStagger && (course % 2 == 1)) ? W * 0.5 : 0.0;
        for (double x = xmin - W + off; x <= xmax + W + 1e-6; x += W) {
            double xj = x;
            if (pat.brickIrregular) {
                // 确定性伪随机：基于（皮号, 竖缝序号）的哈希，扰动 ±35%，不引入全局随机。
                const long long col = (long long)((x - off) / W);
                const long long key = (long long)course * 1000003 + col;
                const double hsh = (double)(key & 1023) / 1023.0;   // [0,1)
                xj += (hsh - 0.5) * W * 0.7;
            }
            out.push_back(geo::Seg{ Vec3(xj, y0, 0), Vec3(xj, y1, 0) });
        }
    }
    return out;
}

// 把连续剖面线切成「实段 + 间隔」的断续线（虚线），实现 GB/T 4457.5-2013 表1 的
// 玻璃（45° 长短画线）与液体（液面下水平短画线）符号。每条输入长线沿其方向依次排布
// 实段：dashLongShort=true 时长/短交替（长段=dashLenMm、短段=dashShortMm），false 时
// 全部取 dashShortMm（均匀短线）；实段之间留 dashGapMm 间隔。输出仍为普通线段集合，
// 下游 paintHatch 按普通线段绘制即呈现断续效果（预览/出图天然一致）。
static std::vector<geo::Seg> dashedSegments(const std::vector<geo::Seg>& in, const HatchPattern& pat) {
    std::vector<geo::Seg> out;
    out.reserve(in.size() * 6);
    for (const auto& s : in) {
        const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9) continue;
        const double ux = dx / len, uy = dy / len;
        double pos = 0.0;
        bool longPhase = true;             // 长/短交替状态（仅 dashLongShort 时生效）
        while (pos < len - 1e-9) {
            const double segLen = pat.dashLongShort
                                  ? (longPhase ? pat.dashLenMm : pat.dashShortMm)
                                  : pat.dashShortMm;   // 液面：全部取短段
            const double end = std::min(len, pos + segLen);
            out.push_back(geo::Seg{ Vec3(s.a.x + ux * pos, s.a.y + uy * pos, 0),
                                   Vec3(s.a.x + ux * end, s.a.y + uy * end, 0) });
            pos = end + pat.dashGapMm;
            longPhase = !longPhase;
        }
    }
    return out;
}

std::vector<geo::Seg> generateHatchLines(const SectionRegion& region) {
    const HatchPattern pat = hatchPatternFor(region.style);
    // PAT 覆盖：材料映射到 hatch/ 目录下的 .pat 图案时，改用 PAT 定义生成（自带角度/间距/笔段）。
    if (!pat.patName.isEmpty()) {
        const PatPattern* pp = HatchPatternLibrary::instance().find(pat.patName);
        if (pp) return generateHatchLinesFromPat(*pp, region);
        // 未找到 PAT（文件缺失）：清空 patName 回退到内置逻辑，保证不退化成空白
    }
    std::vector<geo::Seg> out;
    out.reserve(256);
    if (pat.brick) {                       // 砌体图案（砖墙/石材）：直接铺砌缝，不走斜线族
        auto fam = generateBrickLines(region, pat);
        out.insert(out.end(), fam.begin(), fam.end());
        return out;
    }
    for (double ang : pat.anglesDeg) {
        // 扫描包围盒外扩一个间距，保证首末线不被截断（even-odd 配对把多余部分裁掉）。
        // trustedClosed=true：region.boundary 已由上游 sectionOutline /
        // buildClosedLoopsFromElements 权威校验闭合（首尾距 ≤ 容差 且 面积 > 1e-6），
        // 无需下游再次启发式否决——避免误拒"单一主导收尾缝"的合法闭合多边形。
        auto fam = geo::hatchLines(region.boundary, region.holes,
                                   ang, pat.spacingMm, pat.spacingMm,
                                   /*trustedClosed=*/true);
        out.insert(out.end(), fam.begin(), fam.end());
    }
    // 断续线（虚线）后处理：玻璃/液体等表1 符号（非 brick 路径才生效）。
    if (pat.dashes && !out.empty())
        out = dashedSegments(out, pat);
    return out;
}

// 由 PAT 图案（AutoCAD .pat）生成剖面线。
// 与 brick 分支同理：生成的线段超出区域包围盒，由 GraphicsView::paintHatch 的
// setClipPath 裁剪到区域（含内孔 even-odd），故此处只需把图案铺满包围盒，不手动抠内孔。
//
// 几何约定严格对齐 geo::hatchLines：toLocal = 旋转 -angle 使剖面线水平；toWorld = 旋转 +angle。
// 在「图案空间」（未旋转）中，第 k 条平行线经过 (x0 + k·dx, y0 + k·dy)（k 为整数，铺满包围盒）；
//   * 相邻平行线垂直间距 = |dy|，沿线的错位 = dx（营造错缝/木纹感）；
//   * 笔段序列正负交替（正=落笔实段，负=抬笔间隔），从实段起；长度 0 已在前端解析时丢弃；
//   * 该行笔段相位从 xbase = x0 + k·dx 处为 0，沿图案空间 Xp 重复。
std::vector<geo::Seg> generateHatchLinesFromPat(const PatPattern& pat, const SectionRegion& region) {
    std::vector<geo::Seg> out;
    if (region.boundary.size() < 3) return out;
    out.reserve(512);
    const double BIG = 1e30;
    const double pad = 2.0;   // 包围盒外扩（mm），保证首末线/笔段不被截断

    for (const PatLine& pl : pat.lines) {
        const double a = pl.angleDeg * kPi / 180.0;
        const double c = std::cos(a), s = std::sin(a);
        auto toLocal = [&](const Vec3& p) -> Vec3 {
            return Vec3(p.x * c + p.y * s, -p.x * s + p.y * c, 0.0);
        };
        auto toWorld = [&](double xp, double yp) -> Vec3 {
            return Vec3(xp * c - yp * s, xp * s + yp * c, 0.0);
        };

        // 区域包围盒变换到图案空间（含内孔）。
        double lxmin =  BIG, lxmax = -BIG, lymin =  BIG, lymax = -BIG;
        auto upd = [&](double x, double y) {
            if (x < lxmin) lxmin = x;
            if (x > lxmax) lxmax = x;
            if (y < lymin) lymin = y;
            if (y > lymax) lymax = y;
        };
        for (const auto& p : region.boundary) { const Vec3 q = toLocal(p); upd(q.x, q.y); }
        for (const auto& h : region.holes)
            for (const auto& p : h) { const Vec3 q = toLocal(p); upd(q.x, q.y); }
        const double xStart = lxmin - pad, xEnd = lxmax + pad;
        const double yLo = lymin - pad, yHi = lymax + pad;

        // 笔段周期与空笔段（实线）判定。
        const std::vector<double>& ds = pl.dashes;
        double period = 0.0;
        for (double d : ds) period += std::fabs(d);
        const bool solid = (period <= 1e-9);

        // k 范围：使 yp = y0 + k·dy ∈ [yLo, yHi]。dy≈0 时退化为单条线（k=0）。
        int k0, k1;
        if (std::fabs(pl.dy) < 1e-9) { k0 = 0; k1 = 0; }
        else {
            k0 = (int)std::ceil((yLo - pl.y0) / pl.dy - 1e-9);
            k1 = (int)std::floor((yHi - pl.y0) / pl.dy + 1e-9);
        }

        for (int k = k0; k <= k1; ++k) {
            const double yp = pl.y0 + k * pl.dy;
            const double xbase = pl.x0 + k * pl.dx;   // 该行相位 0 点（图案空间 Xp）

            if (solid) {                              // 无笔段 → 整条实线
                out.push_back(geo::Seg{ toWorld(xStart, yp), toWorld(xEnd, yp) });
                continue;
            }

            // 沿 Xp 按相位循环排布笔段。初始相位 = (xStart - xbase) mod period。
            double phase = std::fmod(xStart - xbase, period);
            if (phase < 0) phase += period;
            // 定位初始相位落在第几个笔段及段内已消费长度。
            int idx = 0; double cons = 0.0;
            double cum = 0.0;
            for (int i = 0; i < (int)ds.size(); ++i) {
                const double L = std::fabs(ds[i]);
                if (phase < cum + L - 1e-12) { idx = i; cons = phase - cum; break; }
                cum += L;
                if (i == (int)ds.size() - 1) { idx = 0; cons = 0.0; }
            }
            double x = xStart;
            while (x < xEnd - 1e-9) {
                const double L = std::fabs(ds[idx]);
                const double remain = L - cons;       // 当前笔段剩余长度
                const double segEnd = std::min(xEnd, x + remain);
                if (ds[idx] > 0.0 && segEnd > x + 1e-9)   // 落笔 → 实段
                    out.push_back(geo::Seg{ toWorld(x, yp), toWorld(segEnd, yp) });
                x = segEnd;
                cons = 0.0;                           // 该笔段已用尽
                ++idx; if (idx >= (int)ds.size()) idx = 0;   // 循环到下一个笔段
            }
        }
    }
    return out;
}

std::vector<Vec3> generateHatchDots(const SectionRegion& region) {
    const HatchPattern pat = hatchPatternFor(region.style);
    if (!pat.dots) return {};
    if (region.boundary.size() < 3) return {};

    // 包围盒 + 点阵步长（用线间距的 0.7 倍，点阵疏于斜线；dotStepScale 可整体放大步长，
    // 用于「基础周围的泥土」等需更稀疏点阵的材料）。
    const QRectF bb = bboxOfPoints(region.boundary);
    const double xmin = bb.left(), ymin = bb.top(), xmax = bb.right(), ymax = bb.bottom();
    const double step = std::max(1.5, region.style.spacingMm * 0.7 * pat.dotStepScale);
    std::vector<Vec3> dots;
    for (double y = ymin + step * 0.5; y < ymax; y += step) {
        for (double x = xmin + step * 0.5; x < xmax; x += step) {
            const Vec3 p(x, y, 0);
            if (!geo::pointInPolygon(p, region.boundary)) continue;
            bool inHole = false;
            for (const auto& h : region.holes)
                if (geo::pointInPolygon(p, h)) { inHole = true; break; }
            if (inHole) continue;
            dots.push_back(p);
        }
    }
    return dots;
}

bool resolveAdjacency(std::vector<SectionRegion>& areas, double baseAngle) {
    const int n = (int)areas.size();
    if (n <= 1) return false;

    // 区域包围盒（外边界 ∪ 各内孔）
    auto bbox = [](const SectionRegion& a) -> QRectF {
        QRectF r = bboxOfPoints(a.boundary);
        for (const auto& h : a.holes) r = r.united(bboxOfPoints(h));
        return r;
    };

    std::vector<QRectF> bx(n);
    for (int i = 0; i < n; ++i) bx[i] = bbox(areas[i]);

    // 邻接关系：两区域包围盒相交（"相邻"的稳健代理，避免浮点边界判定抖动）
    std::vector<std::vector<int>> adj(n);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (bx[i].intersects(bx[j])) { adj[i].push_back(j); adj[j].push_back(i); }

    // 二色（角度 45°/135°）图着色，等价于 GB 4.7「相邻零件方向相反」
    std::vector<int> color(n, -1);
    bool conflict = false;
    std::function<void(int, int)> dfs = [&](int u, int col) {
        color[u] = col;
        for (int v : adj[u]) {
            if (color[v] == -1) dfs(v, 1 - col);
            else if (color[v] == col) conflict = true;
        }
    };
    for (int i = 0; i < n; ++i) if (color[i] == -1) dfs(i, 0);

    bool changed = false;
    for (int i = 0; i < n; ++i) {
        const double ang = baseAngle + color[i] * 90.0;
        if (std::fabs(areas[i].style.angleDeg - ang) > 1e-6) {
            areas[i].style.angleDeg = ang;
            changed = true;
        }
    }
    // 奇环无法二色：对一方改间距（GB 4.7「方向一致而间隔不等」）
    if (conflict) {
        for (int i = 0; i < n; ++i) {
            if (color[i] != 0) continue;
            const double sp = areas[i].style.spacingMm * 1.6;
            if (std::fabs(areas[i].style.spacingMm - sp) > 1e-6) {
                areas[i].style.spacingMm = sp;
                changed = true;
            }
        }
    }
    return changed;
}

SectionResult makeSectionChangesEx(const std::vector<Element>& srcs,
                                   const SectionStyle& style,
                                   bool autoDistinguish,
                                   double closureTol) {
    SectionResult res;

    // 1) 真实判定闭合：只有真正闭合的边界才进入填充。
    LoopBuildResult built = buildLoopsInternal(srcs, closureTol);
    res.skippedOpenChains   = built.stats.openChains;
    res.skippedOpenElements = built.stats.openElements;
    res.skippedDegenerate   = built.stats.degenerate;
    if (built.loops.empty()) return res;      // 无任何闭合轮廓 → 空动作（UI 据此精确提示）

    // 2) 装配区域（默认样式），并记录每个区域对应的源图元 id。
    std::vector<SectionRegion> regions;
    regions.reserve(built.loops.size());
    for (const auto& loop : built.loops) {
        SectionRegion r;
        r.boundary = loop;
        r.style = style;
        regions.push_back(std::move(r));
    }

    // 3) 相邻区域自动区分剖面线方向（GB/T 4457.5-2013 4.7）
    if (autoDistinguish && regions.size() > 1)
        resolveAdjacency(regions, style.angleDeg);

    // 4) 生成 Hatch 替换动作：复用源 ID；链式成环时合并其余源图元（remove）。
    auto findSrc = [&](int id) -> const Element* {
        for (const auto& s : srcs) if (s.id == id) return &s;
        return nullptr;
    };
    for (size_t i = 0; i < regions.size(); ++i) {
        const std::vector<int>& ids = built.loopSrcIds[i];
        if (ids.empty()) continue;
        const int target = ids[0];

        // 装配 Hatch 并继承源图元样式（复用 target 的 id → 单次撤销、选中态不跳）。
        Element h = makeHatchForRegion(regions[i], target, findSrc(target));
        modify::Change ch;
        ch.id = target;
        ch.replacement = h;
        ch.note = QStringLiteral("剖面填充");
        res.changes.push_back(ch);

        // 链式成环：其余参与图元合并删除，避免叠加出重复轮廓。
        for (size_t k = 1; k < ids.size(); ++k) {
            modify::Change del;
            del.id = ids[k];
            del.remove = true;
            del.note = QStringLiteral("剖面填充（并入相邻轮廓）");
            res.changes.push_back(del);
        }
        ++res.filledRegions;
        if (ids.size() > 1) ++res.chainedRegions;
    }
    return res;
}

std::vector<modify::Change> makeSectionChanges(const std::vector<Element>& srcs,
                                              const SectionStyle& style,
                                              bool autoDistinguish) {
    return makeSectionChangesEx(srcs, style, autoDistinguish, kDefaultClosureTol).changes;
}

// ===================== 点选拾取（"剖面填充"的交互核心）=====================

SectionLoopSet buildSectionLoopSet(const std::vector<Element>& elems, double tol) {
    SectionLoopSet set;
    // 点选路径启用"平面细分"：边界线交叉点处剪开 → 分隔线两侧各自成环，
    // 从而支持"点选圆外任意区域 / 竖线分隔单侧"等诉求(见 buildLoopsSubdivided)。

    // 异常 A1 修复：已填充的 Hatch 不再投入细分图。先前 Hatch 会被当成普通边界参与
    // 交点剪开与面遍历，重新解析时把 Hatch 边界与原始边界编织出一个"面积大于原图形"的
    // 非法大环，导致"已填充区域重新进入时被重新划分"。现改为：Hatch 单独作为一条
    // 单源闭环注册(loopHatchId 标出其元素 id)，既保留原始填充边界不变，又让重入拾取
    // 直接命中 loopHatchId 返回 AlreadyFilled。
    std::vector<Element> rest;
    rest.reserve(elems.size());
    for (const auto& e : elems)
        if (e.type != ElementType::Hatch) rest.push_back(e);

    LoopBuildResult built = buildLoopsSubdivided(rest, tol, /*subdivide=*/true);
    set.stats = built.stats;
    set.loops = std::move(built.loops);
    set.loopSrcIds = std::move(built.loopSrcIds);

    const size_t n = set.loops.size();
    set.loopArea.assign(n, 0.0);
    set.loopHatchId.assign(n, 0);
    for (size_t i = 0; i < n; ++i) {
        set.loopArea[i] = std::fabs(polygonArea2(set.loops[i])) * 0.5;
    }

    // 已填充 Hatch → 独立单源闭环 + loopHatchId 标记（不再依赖"单源自闭合"偶然判定）。
    for (const auto& h : elems) {
        if (h.type != ElementType::Hatch) continue;
        if (h.pts.size() < 3) continue;
        set.loops.push_back(h.pts);                 // 外边界多边形(见 makeHatchElement)
        set.loopSrcIds.push_back({ h.id });
        set.loopArea.push_back(std::fabs(polygonArea2(h.pts)) * 0.5);
        set.loopHatchId.push_back(h.id);
    }
    return set;
}

namespace {

// inner 是否严格位于 outer 之内（用于识别嵌套闭环 → 内孔）。
// 先用包围盒快速排除，再逐点判定（点列均已按弦高容差自适应采样，规模有限）。
bool loopInsideLoop(const std::vector<Vec3>& inner, const std::vector<Vec3>& outer) {
    if (inner.size() < 3 || outer.size() < 3) return false;
    const QRectF ib = bboxOfPoints(inner);
    const QRectF ob = bboxOfPoints(outer);
    const double eps = 1e-6;
    if (ib.left() < ob.left() - eps || ib.top() < ob.top() - eps ||
        ib.right() > ob.right() + eps || ib.bottom() > ob.bottom() + eps) return false;
    for (const auto& q : inner)
        if (!geo::pointInPolygon(q, outer)) return false;
    return true;
}

} // namespace

RegionPickResult pickRegionAt(const SectionLoopSet& set, const Vec3& p) {
    RegionPickResult r;
    r.totalLoops = (int)set.loops.size();

    // 1) 包含该点的所有闭环中：优先命中"已填充 Hatch 环"(异常 A1 修复——重入即返回
    //    AlreadyFilled，保留原始填充边界不变)；否则取面积最小者 = 最内层（相邻区域不会互相吞并）。
    int best = -1;
    int bestHatch = -1;
    for (size_t i = 0; i < set.loops.size(); ++i) {
        if (!geo::pointInPolygon(p, set.loops[i])) continue;
        if (i < set.loopHatchId.size() && set.loopHatchId[i] != 0) {
            if (bestHatch < 0 || set.loopArea[i] < set.loopArea[(size_t)bestHatch] - 1e-9)
                bestHatch = (int)i;
            continue;
        }
        if (best < 0 || set.loopArea[i] < set.loopArea[(size_t)best] - 1e-9) best = (int)i;
    }
    if (bestHatch >= 0) {                  // 重入已填充区域 → 不重复填充
        r.loopIndex = bestHatch;
        r.boundary = set.loops[(size_t)bestHatch];
        r.boundarySrcIds = set.loopSrcIds[(size_t)bestHatch];
        r.reason = RegionPickReason::AlreadyFilled;
        return r;
    }
    if (best < 0) { r.reason = RegionPickReason::NoLoopAtPoint; return r; }

    r.loopIndex = best;
    r.boundary = set.loops[(size_t)best];
    r.boundarySrcIds = set.loopSrcIds[(size_t)best];

    // 2) 已填充区域：不重复填充（仍在结果中带回边界，便于 UI 高亮提示）。
    if (best < (int)set.loopHatchId.size() && set.loopHatchId[(size_t)best] != 0) {
        r.reason = RegionPickReason::AlreadyFilled;
        return r;
    }

    // 3) 严格位于命中环内部的更小闭环 → 内孔（even-odd 掏空）。
    for (size_t j = 0; j < set.loops.size(); ++j) {
        if ((int)j == best) continue;
        if (set.loopArea[j] >= set.loopArea[(size_t)best] - 1e-9) continue;   // 不可能被包含
        if (!loopInsideLoop(set.loops[j], set.loops[(size_t)best])) continue;
        r.holes.push_back(set.loops[j]);
        for (int id : set.loopSrcIds[j])
            if (std::find(r.holeSrcIds.begin(), r.holeSrcIds.end(), id) == r.holeSrcIds.end())
                r.holeSrcIds.push_back(id);
    }

    // 4) 净面积（外环 − 内孔）；退化则安全跳过。
    double a = std::fabs(polygonArea2(r.boundary)) * 0.5;
    for (const auto& h : r.holes) a -= std::fabs(polygonArea2(h)) * 0.5;
    r.area = a;
    if (a < 1e-6) { r.reason = RegionPickReason::Degenerate; return r; }

    // 5) "独占"边界源图元：仅出现在本环源列表中（不与任何其它闭环共享），且本环
    //    不是内孔（未被更大的闭环包含——否则本环边界正是外层区域的孔边界）。
    //    只有独占图元可被填充消费；共享边（公共边 / 作为整体外框的同一图元）必须
    //    保留，否则填充本区域会把相邻区域一并裁剪掉。
    {
        auto loopUseCount = [&](int id) {
            int c = 0;
            for (const auto& v : set.loopSrcIds)
                if (std::find(v.begin(), v.end(), id) != v.end()) ++c;
            return c;
        };
        bool bestNested = false;
        for (size_t j = 0; j < set.loops.size(); ++j) {
            if ((int)j == best) continue;
            if (set.loopArea[j] <= set.loopArea[(size_t)best] + 1e-9) continue;
            if (loopInsideLoop(set.loops[(size_t)best], set.loops[j])) { bestNested = true; break; }
        }
        if (!bestNested)
            for (int id : r.boundarySrcIds)
                if (loopUseCount(id) == 1) r.exclusiveSrcIds.push_back(id);
    }

    r.reason = RegionPickReason::Ok;
    r.found  = true;
    return r;
}

RegionPickResult pickSectionRegionAt(const std::vector<Element>& elems, const Vec3& p,
                                     double tol) {
    return pickRegionAt(buildSectionLoopSet(elems, tol), p);
}

SectionResult makeSectionChangeForPick(const RegionPickResult& pick,
                                       const std::vector<Element>& pool,
                                       const SectionStyle& style) {
    SectionResult res;
    if (!pick.ok() || pick.boundarySrcIds.empty()) return res;

    SectionRegion region;
    region.boundary = pick.boundary;
    region.holes    = pick.holes;
    region.style    = style;

    // 自动加密：区域过窄时按设定间距可能一条剖面线都画不出（表现为"填充不完整/
    // 整块空白"）。逐级减半重试，并把实际生效的间距写入 Hatch（渲染端据此绘制）。
    for (int k = 0; k < 3 && generateHatchLines(region).empty(); ++k)
        region.style.spacingMm = std::max(0.8, region.style.spacingMm * 0.5);

    // 只消费"独占"的边界源图元（pick.exclusiveSrcIds）。与相邻区域共享的边界图元
    // （作为整体外框的同一图元 / 公共分隔线 / 公共边）必须原样保留，否则填充本区域
    // 会连带把相邻区域裁剪掉。
    auto isExclusive = [&](int id) {
        return std::find(pick.exclusiveSrcIds.begin(), pick.exclusiveSrcIds.end(), id)
               != pick.exclusiveSrcIds.end();
    };

    // 选一个独占图元承载 Hatch（复用其 id → 单次撤销、选中态不跳）；
    // 若无独占图元（区域内边界全部与相邻区域共享）→ 纯新增 Hatch，不改动任何原图元。
    int target = -1;
    for (int id : pick.boundarySrcIds) if (isExclusive(id)) { target = id; break; }

    // 颜色 / 图层 / 线宽继承：优先承载图元，否则借用第一个边界图元（保持风格一致）。
    const int styleId = (target > 0) ? target
                       : (pick.boundarySrcIds.empty() ? -1 : pick.boundarySrcIds.front());
    const Element* src = nullptr;
    for (const auto& e : pool) if (e.id == styleId) { src = &e; break; }

    // 装配 Hatch：有独占图元 → 复用其 id 原地 update（单次撤销、选中态不跳）；
    // 无独占图元 → 纯新增（id=-1，不删任何东西）。
    const int hatchId = (target > 0) ? target : -1;
    Element h = makeHatchForRegion(region, hatchId, src);
    modify::Change ch;
    ch.id = hatchId;
    ch.replacement = h;
    ch.note = QStringLiteral("剖面填充");
    res.changes.push_back(ch);

    // 其余"独占"边界图元：几何已写入 Hatch，合并删除避免重复轮廓；共享边界图元一律保留。
    // 内孔图元仍按原逻辑并入删除（其几何写入 Hatch.holes）。
    auto pushRemove = [&](int id) {
        if (id <= 0 || id == target) return;
        for (const auto& c : res.changes)
            if (c.remove && c.id == id) return;
        modify::Change d;
        d.id = id;
        d.remove = true;
        d.note = QStringLiteral("剖面填充（并入剖面区域）");
        res.changes.push_back(d);
    };
    for (int id : pick.boundarySrcIds) { if (!isExclusive(id)) continue; pushRemove(id); }
    for (int id : pick.holeSrcIds) pushRemove(id);

    res.filledRegions = 1;
    if (pick.boundarySrcIds.size() > 1 || !pick.holeSrcIds.empty()) res.chainedRegions = 1;
    return res;
}

HatchDraw hatchDrawMode(const SectionRegion& region) {
    switch (region.style.exempt) {
        case SectionExempt::AuxiliaryPart:          // 4.2 相邻辅助零件
        case SectionExempt::SolidBarLongitudinal:   // 实心轴/杆/球/键/销 纵向剖切
        case SectionExempt::RibSpoke:               // 肋/轮辐/薄壁 过对称面
        case SectionExempt::StandardPart:           // 标准件
            return HatchDraw::NoFill;
        case SectionExempt::SmallAreaSolid:         // 狭小区域涂黑
            return HatchDraw::SolidFill;
        case SectionExempt::None:
        default:
            return HatchDraw::Hatch;
    }
}

} // namespace SketchCAD
