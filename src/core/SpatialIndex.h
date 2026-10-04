#pragma once
// ============================================================================
// 均匀网格空间索引（世界坐标，单元尺寸 = 2^k）
// ----------------------------------------------------------------------------
// 设计目标（对应「渲染与拾取复杂度从 O(N) 全量降为增量/对数级」）：
//   * hitTest / hitTestAll / 框选 通过本索引把候选集从全部图元缩小到「查询矩形
//     覆盖的少数网格单元」，使拾取从 O(N) 降为 O(候选数)（场景均匀时≈O(1)）。
//   * 索引只按图元几何包围盒登记；命中判定由调用方用精确几何做二次过滤，
//     因此本索引保证「查询结果 ⊇ 真实命中集」，绝不会漏选（最多多候选）。
//   * 本类为纯 core、视图无关，无 GUI 依赖，可在无界面自测中直接验证。
//
// 单元尺寸：默认为 64（2^6）；GraphicsView 在 rebuild 时按场景跨度自适应取 2^k，
// 使每轴约 sqrt(N) 个单元，单元内平均约 1 个图元。
// ============================================================================
#include <QRectF>
#include <QPointF>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// ============================================================================
// 轻量小向量 SmallVec<T, N>（P0-04 内存预算优化）
// ----------------------------------------------------------------------------
// 绝大多数图元只占 1~2 个网格单元、绝大多数单元桶只有 1~2 个 id：
// 用 N 个元素内联存储，超出才退化为堆分配。相比 std::vector，
// 每图元/每单元省一次堆分配（malloc 头 + 元素块 ≈ 32~48B），
// 10 万图元规模合计节省 ≈ 8~10 MB（见 docs/p0-04 文档预算表）。
// 仅用于 SpatialIndex 内部（容量小、无异常安全诉求的 PoD 场景）。
// ============================================================================
template <class T, int N>
struct SmallVec {
    T inline_[N];
    std::vector<T> overflow;   // 内联槽满后才分配
    int n = 0;                 // 内联槽中的元素数

    // 不变式：溢出激活（overflow 非空）后 n 恒为 0 —— size() 只有一处计数源，
    // 后续 push 一律进 overflow，避免「内联段被双重计入」的重复计数 bug。
    void push_back(const T& v) {
        if (overflow.empty() && n < N) { inline_[n++] = v; return; }
        if (overflow.empty()) { overflow.assign(inline_, inline_ + n); n = 0; }
        overflow.push_back(v);
    }
    int size() const { return n + (int)overflow.size(); }
    bool empty() const { return size() == 0; }
    void clear() { n = 0; overflow.clear(); overflow.shrink_to_fit(); }

    // 跨段只读迭代器（内联槽 + 溢出区连续遍历），支持 range-for
    class const_iterator {
        const SmallVec* v; int i;
    public:
        const_iterator(const SmallVec* vv, int ii) : v(vv), i(ii) {}
        const T& operator*() const { return i < v->n ? v->inline_[i] : v->overflow[i - v->n]; }
        const_iterator& operator++() { ++i; return *this; }
        bool operator!=(const const_iterator& o) const { return v != o.v || i != o.i; }
    };
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end()   const { return const_iterator(this, size()); }

    // 只读遍历（内联槽 + 溢出区）
    template <class F> void each(F f) const {
        for (int i = 0; i < n; ++i) f(inline_[i]);
        for (const auto& v : overflow) f(v);
    }
    // 线性查找（元素为 PoD、数量极小，O(n) 足够）
    bool contains(const T& v) const {
        for (int i = 0; i < n; ++i) if (inline_[i] == v) return true;
        for (const auto& x : overflow) if (x == v) return true;
        return false;
    }
    // 删除一个值（remove 语义）。内联段用尾元素填补；溢出段直接 erase
    //（段间不回搬 —— 元素量级 ≤N，正确性优先于紧凑性）。
    void eraseValue(const T& v) {
        for (int i = 0; i < n; ++i) {
            if (inline_[i] == v) {   // 内联段命中时溢出区必为空（见 push_back 不变式）
                inline_[i] = inline_[n - 1]; --n;
                return;
            }
        }
        for (auto it = overflow.begin(); it != overflow.end(); ++it) {
            if (*it == v) { overflow.erase(it); return; }
        }
    }
};

class SpatialIndex {
public:
    SpatialIndex() = default;
    explicit SpatialIndex(double cellSize) : cellSize_(cellSize > 0.0 ? cellSize : 64.0) {}

    void setCellSize(double s) { if (s > 0.0) cellSize_ = s; }
    double cellSize() const { return cellSize_; }

    // 特性开关：关闭后 query* 返回空（调用方回落全量扫描），用于双跑对照验证。
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    void clear() { cells_.clear(); idCells_.clear(); }
    bool empty() const { return cells_.empty(); }

    // 登记 / 移除 / 更新（更新 = 先按旧单元移除再按新包围盒登记，幂等）
    void add(int id, const QRectF& b);
    void remove(int id);
    void update(int id, const QRectF& b);

    // 返回 bbox 与查询矩形相交的候选 id（已去重）。启用开关时为空向量。
    std::vector<int> queryRect(const QRectF& rect) const;
    std::vector<int> queryPoint(const QPointF& w, double tol) const {
        return queryRect(QRectF(w.x() - tol, w.y() - tol, 2.0 * tol, 2.0 * tol));
    }

private:
    struct Cell { std::int32_t gx, gy; };
    struct CellHash {
        size_t operator()(const Cell& c) const noexcept {
            // 大质数异或混合，避免 gx/gy 量级/符号导致的桶聚集
            return (size_t)((std::int64_t)c.gx * 73856093 ^ (std::int64_t)c.gy * 19349663);
        }
    };
    struct CellEq { bool operator()(const Cell& a, const Cell& b) const noexcept {
        return a.gx == b.gx && a.gy == b.gy; } };

    double cellSize_ = 64.0;
    bool   enabled_  = true;
    // P0-04：桶内 id 与 id→单元反查均用 SmallVec 内联（≤2 个内联、零堆分配），
    // 10 万图元规模下比 vector 版本省约 8~10MB 与 20 万次 malloc。
    std::unordered_map<Cell, SmallVec<int, 2>, CellHash, CellEq> cells_;
    std::unordered_map<int, SmallVec<Cell, 2>, std::hash<int>> idCells_;   // 每个 id 登记过的单元（移除/更新用）

    SmallVec<Cell, 8> cellsFor(const QRectF& b) const;
};

} // namespace SketchCAD
