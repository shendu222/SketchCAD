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
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

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
    std::unordered_map<Cell, std::vector<int>, CellHash, CellEq> cells_;
    std::unordered_map<int, std::vector<Cell>> idCells_;   // 每个 id 登记过的单元（移除/更新用）

    std::vector<Cell> cellsFor(const QRectF& b) const;
};

} // namespace SketchCAD
