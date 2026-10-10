// ============================================================================
// UndoDelta 实现 —— 元素级差量计算与回放
// 序列化复用 Element::elementToJson / elementFromJson（Element.cpp）。
// ============================================================================
#include "UndoDelta.h"

#include <algorithm>
#include <unordered_map>
#include <QJsonDocument>

namespace SketchCAD {

namespace {
// 元素 → 紧凑 JSON 字节（用于逐图元字节级相等判定，复用既有编解码，零新增格式）
QByteArray elemBytes(const Element& e) {
    return QJsonDocument(elementToJson(e)).toJson(QJsonDocument::Compact);
}
} // namespace

std::vector<ElementDelta> diffElements(const std::vector<Element>& before,
                                       const std::vector<Element>& after) {
    std::vector<ElementDelta> out;
    // 快路径：图元 id 单调递增（addElement 用 nextId_++ 且尾部追加，删除不打乱顺序），
    // 两向量各自按 id 有序时用双指针归并 —— O(N) 且零堆分配。
    // 撤销热路径每步都要 diff 两份 5 万图元向量，std::map 建索引（10 万次树插入）
    // 实测会吃掉 20ms+，是「单步撤销 ≤16ms」预算的最大杀手，故必须有此快路径。
    auto sortedById = [](const std::vector<Element>& v) {
        for (size_t k = 1; k < v.size(); ++k) if (!(v[k-1].id < v[k].id)) return false;
        return true;
    };
    if (sortedById(before) && sortedById(after)) {
        size_t i = 0, j = 0;
        while (i < before.size() && j < after.size()) {
            const Element& b = before[i];
            const Element& a = after[j];
            if (b.id < a.id) {                       // 只存在于 before → 删除
                out.emplace_back(ElementDelta::Removed, b.id, (int)i, elemBytes(b), QByteArray());
                ++i;
            } else if (a.id < b.id) {                // 只存在于 after → 新增
                out.emplace_back(ElementDelta::Added, a.id, (int)j, QByteArray(), elemBytes(a));
                ++j;
            } else {                                 // 同 id → 修改检测（成员比较，JSON 只存真变更）
                if (!(b == a))
                    out.emplace_back(ElementDelta::Modified, a.id, (int)j, elemBytes(b), elemBytes(a));
                ++i; ++j;
            }
        }
        for (; i < before.size(); ++i)
            out.emplace_back(ElementDelta::Removed, before[i].id, (int)i, elemBytes(before[i]), QByteArray());
        for (; j < after.size(); ++j)
            out.emplace_back(ElementDelta::Added, after[j].id, (int)j, QByteArray(), elemBytes(after[j]));
        return out;
    }

    // 慢路径（乱序，如差量回放产生过中间插入）：哈希索引，仍避免红黑树开销
    std::unordered_map<int, int> bIdx, aIdx;
    bIdx.reserve(before.size() * 2);
    aIdx.reserve(after.size() * 2);
    for (int i = 0; i < (int)before.size(); ++i) bIdx.emplace(before[i].id, i);
    for (int j = 0; j < (int)after.size();  ++j) aIdx.emplace(after[j].id,  j);

    // 1) 删除：在 before 但不在 after
    for (int i = 0; i < (int)before.size(); ++i) {
        const Element& e = before[i];
        if (aIdx.find(e.id) == aIdx.end()) {
            out.emplace_back(ElementDelta::Removed, e.id, i,
                             elemBytes(e), QByteArray());
        }
    }
    // 2) 新增 / 3) 修改：遍历 after
    for (int j = 0; j < (int)after.size(); ++j) {
        const Element& e = after[j];
        auto it = bIdx.find(e.id);
        if (it == bIdx.end()) {
            out.emplace_back(ElementDelta::Added, e.id, j,
                             QByteArray(), elemBytes(e));
        } else if (it->second != j) {
            // 同 id 但位置不同（乱序/reorder/同 id 删除后异位复用）：必须拆成
            // Removed+Added 才能保序——若坍缩成 Modified（按 id 原位替换），
            // 该图元会留在旧位置，回放后顺序与结果态不一致。
            out.emplace_back(ElementDelta::Removed, e.id, it->second,
                             elemBytes(before[it->second]), QByteArray());
            out.emplace_back(ElementDelta::Added, e.id, j,
                             QByteArray(), elemBytes(e));
        } else if (!(before[it->second] == e)) {
            // 同位异容 → Modified（成员比较，仅对真正变化的图元做 JSON 存 payload）
            out.emplace_back(ElementDelta::Modified, e.id, j,
                             elemBytes(before[it->second]), elemBytes(e));
        }
    }
    return out;
}

void applyElementDeltas(std::vector<Element>& elems,
                        const std::vector<ElementDelta>& ds,
                        bool forward) {
    // id 定位快路径：elems 按 id 有序时二分（replay 热路径每步最多 49 个差量，
    // 每个 O(N) 线性扫会在 5 万图元场景吃掉 10ms+；删除/原位替换不破坏有序性）。
    bool sorted = true;
    for (size_t k = 1; k < elems.size(); ++k) if (!(elems[k-1].id < elems[k].id)) { sorted = false; break; }
    auto findByIdIn = [sorted](std::vector<Element>& v, int id) -> std::vector<Element>::iterator {
        if (sorted) {
            auto it = std::lower_bound(v.begin(), v.end(), id,
                                       [](const Element& e, int i) { return e.id < i; });
            if (it != v.end() && it->id == id) return it;
            return v.end();
        }
        return std::find_if(v.begin(), v.end(), [id](const Element& e) { return e.id == id; });
    };
    // 批量删除：逐条 erase 是 O(N²)（每条 erase 触发整段尾部移动，5 万条 Added 的
    // 逆向回放实测 64s）；超过阈值改走「id 集合标记 + 原地单趟压缩」O(N)，
    // 相对顺序不变（有序性保持，后续二分依旧有效）。
    auto eraseByIds = [findByIdIn](std::vector<Element>& v,
                                   const std::vector<const ElementDelta*>& list) {
        constexpr size_t kBulkEraseThreshold = 8;
        if (list.size() <= kBulkEraseThreshold) {
            for (const auto* d : list) {
                auto it = findByIdIn(v, d->id);
                if (it != v.end()) v.erase(it);
            }
            return;
        }
        std::unordered_map<int, char> kill;
        kill.reserve(list.size() * 2);
        for (const auto* d : list) kill.emplace(d->id, 1);
        size_t w = 0;
        for (size_t r = 0; r < v.size(); ++r) {
            if (kill.find(v[r].id) == kill.end()) v[w++] = std::move(v[r]);
        }
        v.resize(w);
    };

    if (forward) {
        // 先按 id 删除（Removed）
        {
            std::vector<const ElementDelta*> dels;
            for (const auto& d : ds) if (d.op == ElementDelta::Removed) dels.push_back(&d);
            eraseByIds(elems, dels);
        }
        // 再按 id 替换为 afterJson（Modified）
        for (const auto& d : ds) {
            if (d.op != ElementDelta::Modified) continue;
            auto it = findByIdIn(elems, d.id);
            if (it != elems.end()) *it = elementFromJson(QJsonDocument::fromJson(d.afterJson).object());
        }
        // 最后按 index（升序）插入 afterJson。
        // Added 的 index 是「结果态」中的最终位：当前向量 = 结果态去掉全部 Added，
        // 故升序逐个插在各自最终位即可精确还原（先插低位的不会被打乱）。
        // ⚠️ 旧写法降序插入 + qBound 钳位，多元素重叠区间时会错位。
        std::vector<const ElementDelta*> adds;
        for (const auto& d : ds) if (d.op == ElementDelta::Added) adds.push_back(&d);
        std::sort(adds.begin(), adds.end(),
                  [](const ElementDelta* x, const ElementDelta* y) { return x->index < y->index; });
        for (const auto* d : adds) {
            int pos = qBound(0, d->index, (int)elems.size());
            elems.insert(elems.begin() + pos,
                         elementFromJson(QJsonDocument::fromJson(d->afterJson).object()));
        }
    } else {
        // 反向：先按 id 删除（Added，即仅存在于结果态者）
        {
            std::vector<const ElementDelta*> dels;
            for (const auto& d : ds) if (d.op == ElementDelta::Added) dels.push_back(&d);
            eraseByIds(elems, dels);
        }
        // 按 id 替换为 beforeJson（Modified）
        for (const auto& d : ds) {
            if (d.op != ElementDelta::Modified) continue;
            auto it = findByIdIn(elems, d.id);
            if (it != elems.end()) *it = elementFromJson(QJsonDocument::fromJson(d.beforeJson).object());
        }
        // 按 index（升序）插入 beforeJson（Removed，index 为「源态」最终位，同上可精确还原）
        std::vector<const ElementDelta*> rems;
        for (const auto& d : ds) if (d.op == ElementDelta::Removed) rems.push_back(&d);
        std::sort(rems.begin(), rems.end(),
                  [](const ElementDelta* x, const ElementDelta* y) { return x->index < y->index; });
        for (const auto* d : rems) {
            int pos = qBound(0, d->index, (int)elems.size());
            elems.insert(elems.begin() + pos,
                         elementFromJson(QJsonDocument::fromJson(d->beforeJson).object()));
        }
    }
}

} // namespace SketchCAD
