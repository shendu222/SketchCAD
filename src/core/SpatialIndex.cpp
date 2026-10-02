#include "SpatialIndex.h"

namespace SketchCAD {

// 计算覆盖包围盒 b 的全部网格单元。对退化（零面积）包围盒取中心点所在单元；
// 对极端细长图元（如裁剪前的无限线）做单元数上限保护，避免登记爆炸。
std::vector<SpatialIndex::Cell> SpatialIndex::cellsFor(const QRectF& b) const {
    std::vector<Cell> out;
    const QRectF n = b.normalized();
    const long x0 = (long)std::floor(n.left()   / cellSize_);
    const long y0 = (long)std::floor(n.top()    / cellSize_);
    const long x1 = (long)std::floor(n.right()  / cellSize_);
    const long y1 = (long)std::floor(n.bottom() / cellSize_);

    // 退化包围盒（空或零面积）：用中心点单元
    if (n.isEmpty() || (x1 - x0 <= 0 && y1 - y0 <= 0)) {
        const long gx = (long)std::floor(n.center().x() / cellSize_);
        const long gy = (long)std::floor(n.center().y() / cellSize_);
        out.push_back({ (std::int32_t)gx, (std::int32_t)gy });
        return out;
    }
    // 单元数上限保护（极长图元不会真的登记几百万单元）
    constexpr long kMaxCells = 4096;
    if ((x1 - x0 + 1) * (y1 - y0 + 1) > kMaxCells) {
        const long gx = (long)std::floor(n.center().x() / cellSize_);
        const long gy = (long)std::floor(n.center().y() / cellSize_);
        out.push_back({ (std::int32_t)gx, (std::int32_t)gy });
        return out;
    }
    for (long gx = x0; gx <= x1; ++gx)
        for (long gy = y0; gy <= y1; ++gy)
            out.push_back({ (std::int32_t)gx, (std::int32_t)gy });
    return out;
}

void SpatialIndex::add(int id, const QRectF& b) {
    const auto cls = cellsFor(b);
    for (const auto& c : cls) cells_[c].push_back(id);
    idCells_[id] = cls;
}

void SpatialIndex::remove(int id) {
    auto it = idCells_.find(id);
    if (it == idCells_.end()) return;
    for (const auto& c : it->second) {
        auto cit = cells_.find(c);
        if (cit == cells_.end()) continue;
        auto& v = cit->second;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        if (v.empty()) cells_.erase(cit);
    }
    idCells_.erase(it);
}

void SpatialIndex::update(int id, const QRectF& b) {
    remove(id);
    add(id, b);
}

std::vector<int> SpatialIndex::queryRect(const QRectF& rect) const {
    std::vector<int> out;
    if (!enabled_) return out;
    const auto cls = cellsFor(rect);
    std::unordered_set<int> seen;
    seen.reserve(cls.size() * 4);
    for (const auto& c : cls) {
        auto it = cells_.find(c);
        if (it == cells_.end()) continue;
        for (int id : it->second) {
            if (seen.insert(id).second) out.push_back(id);
        }
    }
    return out;
}

} // namespace SketchCAD
