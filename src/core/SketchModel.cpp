#include "SketchModel.h"
#include "Geometry.h"
#include "ConstraintSolver.h"
#include "DimStyle.h"
#include "Modify.h"
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include <QJsonArray>

namespace SketchCAD {

SketchModel::SketchModel(QObject* p) : QObject(p) {
    layers_.push_back(Layer(QStringLiteral("0")));   // 默认层始终存在
}

std::vector<Element>::iterator SketchModel::find(int id) {
    return std::find_if(elems_.begin(), elems_.end(),
                        [id](const Element& e) { return e.id == id; });
}

void SketchModel::pushMemento(const QString& label) {
    // 快照同时记录绘图比例：撤销"修改比例"时必须让几何与比例一起回退，
    // 否则回退后再改比例会以错误的基准计算缩放因子。
    // 也记录图层表：撤销"隐藏图层/改图层样式"等操作时，图层定义与图元需一起回退。
    Memento m;
    m.label = label; m.elems = elems_; m.nextId = nextId_;
    m.scale = scaleRatio_; m.layers = layers_;
    undoStack_.push_back(std::move(m));
    if (undoStack_.size() > 200) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

int SketchModel::addElement(const Element& e) {
    pushMemento(QStringLiteral("添加") + e.name);
    Element ne = e;
    ne.id = nextId_++;
    elems_.push_back(ne);
    emit modelChanged();
    return ne.id;
}

bool SketchModel::removeElement(int id) {
    auto it = find(id);
    if (it == elems_.end()) return false;
    pushMemento(QStringLiteral("删除") + it->name);
    elems_.erase(it);
    auto sit = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (sit != selectedIds_.end()) selectedIds_.erase(sit);
    if (selectedId_ == id) {
        selectedId_ = selectedIds_.empty() ? -1 : selectedIds_.back();
        emit selectionChanged(selectedId_);
    }
    emit modelChanged();
    return true;
}

bool SketchModel::updateElement(int id, std::function<void(Element&)> fn) {
    auto it = find(id);
    if (it == elems_.end()) return false;
    pushMemento(QStringLiteral("编辑") + it->name);
    fn(*it);
    emit modelChanged();
    emit elementUpdated(id);
    return true;
}

void SketchModel::beginEdit(const QString& label) {
    pushMemento(label.isEmpty() ? QStringLiteral("编辑") : label);
}

bool SketchModel::updateElementQuiet(int id, std::function<void(Element&)> fn) {
    auto it = find(id);
    if (it == elems_.end()) return false;
    fn(*it);
    emit modelChanged();
    emit elementUpdated(id);
    return true;
}

// 静默添加：不压撤销栈；调用前需 beginEdit() 一次。
std::vector<int> SketchModel::addElementsQuiet(const std::vector<Element>& parts) {
    std::vector<int> ids;
    ids.reserve(parts.size());
    for (const auto& p : parts) {
        Element ne = p;
        ne.id = nextId_++;
        elems_.push_back(ne);
        ids.push_back(ne.id);
    }
    if (!parts.empty()) emit modelChanged();
    return ids;
}

// 静默删除：不压撤销栈；调用前需 beginEdit() 一次。
bool SketchModel::removeElementQuiet(int id) {
    auto it = find(id);
    if (it == elems_.end()) return false;
    elems_.erase(it);
    auto sit = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (sit != selectedIds_.end()) selectedIds_.erase(sit);
    if (selectedId_ == id) {
        selectedId_ = selectedIds_.empty() ? -1 : selectedIds_.back();
        emit selectionChanged(selectedId_);
    }
    emit modelChanged();
    return true;
}

// ---------------- 端点捕捉与联动 ----------------

SketchModel::HandleRef SketchModel::handleAt(const Vec3& p, double tol) const {
    HandleRef best;
    double bestD = tol;
    // 倒序遍历：后画的（视觉上在上层的）图元优先抓到
    for (int i = (int)elems_.size() - 1; i >= 0; --i) {
        const Element& e = elems_[i];
        if (!e.visible) continue;
        if (e.type == ElementType::Constraint) continue;
        auto hs = geo::elementHandles(e);
        for (int k = 0; k < (int)hs.size(); ++k) {
            double d = std::hypot(hs[k].x - p.x, hs[k].y - p.y);
            // 严格小于才替换，加上「选中图元享受一次容差放宽」，
            // 让用户在两个端点重合时优先拖自己刚选中的那个。
            double bias = (e.id == selectedId_) ? d * 0.5 : d;
            if (bias <= bestD) { bestD = bias; best.elemId = e.id; best.index = k; }
        }
    }
    return best;
}

std::vector<SketchModel::HandleRef> SketchModel::collectLinked(const Vec3& p, double tol) const {
    std::vector<HandleRef> out;
    for (const auto& e : elems_) {
        if (!e.visible) continue;
        if (e.type == ElementType::Constraint) continue;
        auto hs = geo::elementHandles(e);
        for (int k = 0; k < (int)hs.size(); ++k) {
            // 圆心(圆/弧的 0 号手柄)不参与「端点联动」：它是位置手柄不是连接点，
            // 否则一条直线端点恰好压在圆心上就会把整个圆拖走，明显不是用户想要的。
            if ((e.type == ElementType::Circle || e.type == ElementType::Arc) && k == 0) continue;
            if (std::hypot(hs[k].x - p.x, hs[k].y - p.y) <= tol) out.push_back({e.id, k});
        }
    }
    return out;
}

int SketchModel::moveHandles(const std::vector<HandleRef>& refs, const Vec3& to) {
    int n = 0;
    for (const auto& r : refs) {
        auto it = find(r.elemId);
        if (it == elems_.end()) continue;
        if (geo::setElementHandle(*it, r.index, to)) { ++n; emit elementUpdated(r.elemId); }
    }
    if (n) emit modelChanged();
    return n;
}

std::vector<int> SketchModel::replaceElement(int id, const std::vector<Element>& parts,
                                             const QString& label) {
    std::vector<int> ids;
    auto it = find(id);
    if (it == elems_.end()) return ids;
    pushMemento(label.isEmpty() ? QStringLiteral("剪切") : label);
    size_t pos = (size_t)(it - elems_.begin());
    elems_.erase(it);
    // 插回原位置，保持图层顺序（z 序）不跳动
    for (size_t k = 0; k < parts.size(); ++k) {
        Element ne = parts[k];
        ne.id = nextId_++;
        elems_.insert(elems_.begin() + (long)(pos + k), ne);
        ids.push_back(ne.id);
    }
    if (selectedId_ == id) {
        auto sit = std::find(selectedIds_.begin(), selectedIds_.end(), id);
        if (sit != selectedIds_.end()) selectedIds_.erase(sit);
        for (int nid : ids) selectedIds_.push_back(nid);
        selectedId_ = ids.empty() ? -1 : ids.front();
        emit selectionChanged(selectedId_);
    }
    emit modelChanged();
    return ids;
}

// ============================ 联动更新（需求 2/3）============================

bool SketchModel::updateElementLinked(int id, std::function<void(Element&)> fn) {
    auto it = find(id);
    if (it == elems_.end()) return false;

    // 修改前快照所有控制点位置——这是「拓扑连接关系」的具体表达：
    // 两个图元的端点重合即视为相连，修改后要把同一位置上的其他端点也带过去。
    std::vector<Vec3> oldHandles = geo::elementHandles(*it);

    // 修改前快照整个图元列表，用于约束检测（需求：关联图元联动原则）
    auto beforeElems = elems_;

    pushMemento(QStringLiteral("编辑(联动)") + it->name);
    fn(*it);

    // 修改后取新控制点位置，逐个比较：移动了的控制点需要传播到相连图元
    std::vector<Vec3> newHandles = geo::elementHandles(*it);
    const double tol = 1e-6;
    for (size_t i = 0; i < oldHandles.size() && i < newHandles.size(); ++i) {
        if (oldHandles[i].distTo(newHandles[i]) < 1e-9) continue;  // 该控制点未移动

        // 在旧位置附近收集所有重合的控制点（= 与之相连的图元端点）
        auto linked = collectLinked(oldHandles[i], tol);
        for (const auto& ref : linked) {
            if (ref.elemId == id) continue;  // 跳过自身
            auto lit = find(ref.elemId);
            if (lit == elems_.end()) continue;
            if (geo::setElementHandle(*lit, ref.index, newHandles[i]))
                emit elementUpdated(ref.elemId);
        }
    }

    // 约束恢复：检测修改前满足的相切/垂直/平行约束，恢复被破坏的约束（需求：关联图元联动）
    auto modifiedIds = ConstraintSolver::solveAfterChange(beforeElems, elems_, id);
    for (int mid : modifiedIds) emit elementUpdated(mid);

    emit modelChanged();
    emit elementUpdated(id);
    return true;
}

bool SketchModel::driveDimension(int dimId, double value) {
    const Element* dimEl = element(dimId);
    if (!dimEl || dimEl->type != ElementType::Dimension || dimEl->refA < 0) return false;
    const Element* srcEl = element(dimEl->refA);
    if (!srcEl) return false;

    const int srcId = dimEl->refA;
    const DimensionType dt = dimEl->dtype;
    Vec3 newPos;
    double newRadius = 0;
    bool isLineDrive = false, isCircleDrive = false;

    // ---- 根据标注类型计算目标几何 ----
    switch (dt) {
        case DimensionType::Aligned:
        case DimensionType::Distance: {
            // 对齐/线性标注：保持方向，改长度。起点不动，终点沿原方向走到新长度。
            if (srcEl->type != ElementType::Line || srcEl->pts.size() < 2) return false;
            const Vec3 dir = srcEl->pts[1] - srcEl->pts[0];
            const double oldLen = std::hypot(dir.x, dir.y);
            if (oldLen < 1e-9) return false;
            const double ux = dir.x / oldLen, uy = dir.y / oldLen;
            newPos = Vec3(srcEl->pts[0].x + ux * value,
                           srcEl->pts[0].y + uy * value, 0);
            isLineDrive = true;
            break;
        }
        case DimensionType::Horizontal: {
            // 水平标注：只改 Δx，Y 保持不变
            if (srcEl->type != ElementType::Line || srcEl->pts.size() < 2) return false;
            const double sign = (srcEl->pts[1].x >= srcEl->pts[0].x) ? 1.0 : -1.0;
            newPos = Vec3(srcEl->pts[0].x + sign * value, srcEl->pts[1].y, 0);
            isLineDrive = true;
            break;
        }
        case DimensionType::Vertical: {
            // 竖直标注：只改 Δy，X 保持不变
            if (srcEl->type != ElementType::Line || srcEl->pts.size() < 2) return false;
            const double sign = (srcEl->pts[1].y >= srcEl->pts[0].y) ? 1.0 : -1.0;
            newPos = Vec3(srcEl->pts[1].x, srcEl->pts[0].y + sign * value, 0);
            isLineDrive = true;
            break;
        }
        case DimensionType::Radius:
            if (srcEl->type != ElementType::Circle && srcEl->type != ElementType::Arc) return false;
            newRadius = std::max(1e-6, value);
            isCircleDrive = true;
            break;
        case DimensionType::Diameter:
            if (srcEl->type != ElementType::Circle && srcEl->type != ElementType::Arc) return false;
            newRadius = std::max(1e-6, value / 2.0);
            isCircleDrive = true;
            break;
        default:
            return false;  // 角度标注暂不支持驱动
    }

    // ---- 统一执行：一次快照、一次 modelChanged ----
    pushMemento(QStringLiteral("驱动尺寸"));

    // 直线类标注：修改终点 + 联动传播
    if (isLineDrive) {
        // 记录旧控制点位置用于联动
        std::vector<Vec3> oldHandles = geo::elementHandles(*find(srcId));
        // 修改源图元终点
        updateElementQuiet(srcId, [newPos](Element& e) { e.pts[1] = newPos; });
        // 传播到相连图元
        std::vector<Vec3> newHandles = geo::elementHandles(*find(srcId));
        const double tol = 1e-6;
        for (size_t i = 0; i < oldHandles.size() && i < newHandles.size(); ++i) {
            if (oldHandles[i].distTo(newHandles[i]) < 1e-9) continue;
            auto linked = collectLinked(oldHandles[i], tol);
            for (const auto& ref : linked) {
                if (ref.elemId == srcId) continue;
                auto lit = find(ref.elemId);
                if (lit == elems_.end()) continue;
                if (geo::setElementHandle(*lit, ref.index, newHandles[i]))
                    emit elementUpdated(ref.elemId);
            }
        }
    }
    // 圆/弧类标注：修改半径
    if (isCircleDrive) {
        updateElementQuiet(srcId, [newRadius](Element& e) { e.radius = newRadius; });
    }

    // 同步标注自身的测量点与字段，使标注文字跟随更新
    updateElementQuiet(dimId, [this, srcId, dt, newRadius, isLineDrive](Element& d) {
        d.text.clear();  // 清除手动覆盖，让显示跟随实测值
        const Element* s = element(srcId);
        if (!s) return;
        if (isLineDrive) {
            if (s->pts.size() >= 2 && d.pts.size() >= 2) {
                d.pts[0] = s->pts[0];
                d.pts[1] = s->pts[1];
            }
        } else {
            d.radius = newRadius;
            if (d.pts.size() >= 2 && !d.pts.empty())
                d.pts[1] = Vec3(d.pts[0].x + newRadius, d.pts[0].y, 0);
        }
    });

    emit modelChanged();
    emit elementUpdated(dimId);
    return true;
}

const Element* SketchModel::element(int id) const {
    for (const auto& e : elems_) if (e.id == id) return &e;
    return nullptr;
}
const Element* SketchModel::selected() const {
    return selectedId_ >= 0 ? element(selectedId_) : nullptr;
}

void SketchModel::setSelected(int id) {
    if (id < 0) { clearSelection(); return; }
    if (selectedId_ == id) return;
    selectedIds_ = {id};
    selectedId_ = id;
    emit selectionChanged(id);
}
void SketchModel::setSelection(const std::vector<int>& ids) {
    selectedIds_ = ids;
    selectedId_ = ids.empty() ? -1 : ids.back();
    emit selectionChanged(selectedId_);
}
void SketchModel::addToSelection(int id) {
    if (id < 0) return;
    if (std::find(selectedIds_.begin(), selectedIds_.end(), id) != selectedIds_.end()) return;
    selectedIds_.push_back(id);
    selectedId_ = id;
    emit selectionChanged(id);
}
void SketchModel::addToSelection(const std::vector<int>& ids) {
    bool changed = false;
    for (int id : ids) {
        if (id < 0) continue;
        if (std::find(selectedIds_.begin(), selectedIds_.end(), id) == selectedIds_.end()) {
            selectedIds_.push_back(id);
            changed = true;
        }
    }
    if (changed) {
        selectedId_ = selectedIds_.empty() ? -1 : selectedIds_.back();
        emit selectionChanged(selectedId_);
    }
}
void SketchModel::toggleSelection(int id) {
    auto it = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (it != selectedIds_.end()) {
        selectedIds_.erase(it);
        selectedId_ = selectedIds_.empty() ? -1 : selectedIds_.back();
        emit selectionChanged(selectedId_);
    } else {
        addToSelection(id);
    }
}
void SketchModel::clearSelection() {
    if (selectedIds_.empty() && selectedId_ < 0) return;
    selectedIds_.clear();
    selectedId_ = -1;
    emit selectionChanged(-1);
}
bool SketchModel::isSelected(int id) const {
    return std::find(selectedIds_.begin(), selectedIds_.end(), id) != selectedIds_.end();
}

// ==================== 高级选择（选择菜单 / 命令行）====================
// 设计说明：所有操作最终都收敛到 setSelection / addToSelection / clearSelection，
// 与点击选择、框选共用同一套 selectedId_ / selectedIds_ 状态，避免新增第二套真相。

void SketchModel::selectAll() {
    std::vector<int> ids;
    for (const auto& e : elems_) if (e.visible) ids.push_back(e.id);
    setSelection(ids);
    log(QStringLiteral("已全选 %1 个图元").arg(ids.size()));
}

void SketchModel::selectWindow(const QRectF& rect, bool additive) {
    // 含边界判定：QRectF::intersects 要求非零公共面积，而直线/折线等零高/零宽图元的
    // 几何包围盒面积为零，必须用容差同时膨胀查询框与图元包围盒，否则框选永远选不中线。
    const double eps = 1e-6;
    const QRectF r = rect.adjusted(-eps, -eps, eps, eps);
    std::vector<int> ids;
    for (const auto& e : elems_) {
        if (!e.visible) continue;
        if (r.intersects(geo::elementBounds(e).adjusted(-eps, -eps, eps, eps))) ids.push_back(e.id);
    }
    if (additive) addToSelection(ids); else setSelection(ids);
    log(QStringLiteral("框选完成：选中 %1 个图元%2")
            .arg(ids.size()).arg(additive ? QStringLiteral("（已并入选区）") : QString()));
}

void SketchModel::deselectWindow(const QRectF& rect) {
    const double eps = 1e-6;
    const QRectF r = rect.adjusted(-eps, -eps, eps, eps);        // 与 selectWindow 一致：含边界
    std::vector<int> keep;
    for (int id : selectedIds_) {
        const Element* e = element(id);
        if (!e) { keep.push_back(id); continue; }                 // 已不存在，保留以防误删
        if (!r.intersects(geo::elementBounds(*e).adjusted(-eps, -eps, eps, eps))) keep.push_back(id);
    }
    if ((int)keep.size() != (int)selectedIds_.size()) setSelection(keep);
    log(QStringLiteral("框取消完成：剩余选中 %1 个图元").arg(keep.size()));
}

void SketchModel::selectEntity(int id) {
    setSelected(id);
}

void SketchModel::deselectEntity(int id) {
    auto it = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (it == selectedIds_.end()) return;
    selectedIds_.erase(it);
    selectedId_ = selectedIds_.empty() ? -1 : selectedIds_.back();
    emit selectionChanged(selectedId_);
    log(QStringLiteral("已取消选择图元 #%1").arg(id));
}

std::vector<int> SketchModel::collectContourIds(int seedId) const {
    // 沿「拓扑连通性」(共享端点) 做连通分量收集：从种子图元出发，
    // 取其每个端点，用 collectLinked 找出同端点上的其它图元，递归扩展。
    // 圆/弧的圆心(index 0)不是连接点，collectLinked 已将其排除。
    const Element* seed = element(seedId);
    if (!seed || !seed->visible) return {};
    const double tol = 1e-6;
    std::vector<int> result;
    std::vector<int> stack = {seedId};
    while (!stack.empty()) {
        int cur = stack.back(); stack.pop_back();
        const Element* e = element(cur);
        if (!e) continue;
        if (std::find(result.begin(), result.end(), cur) != result.end()) continue;
        result.push_back(cur);
        // 收集当前图元的「端点」(圆/弧排除圆心)，向同端点图元扩散
        auto hs = geo::elementHandles(*e);
        for (int k = 0; k < (int)hs.size(); ++k) {
            if ((e->type == ElementType::Circle || e->type == ElementType::Arc) && k == 0) continue;
            auto linked = collectLinked(hs[k], tol);
            for (const auto& ref : linked)
                if (std::find(result.begin(), result.end(), ref.elemId) == result.end())
                    stack.push_back(ref.elemId);
        }
    }
    return result;
}

void SketchModel::selectContour(int seedId) {
    const Element* seed = element(seedId);
    if (!seed || !seed->visible) { log(QStringLiteral("选择轮廓：无效的种子图元"), "warn"); return; }
    const std::vector<int> result = collectContourIds(seedId);
    setSelection(result);
    log(QStringLiteral("已选择轮廓：%1 个相连图元").arg(result.size()));
}

void SketchModel::toggleContour(int seedId) {
    // (取消)选择轮廓：轮廓内图元已全部选中 → 整条移除；否则整条选中。
    // 与 LibreCAD 的 (De-)Select Contour 语义一致，一个菜单项同时承担选/取消。
    const Element* seed = element(seedId);
    if (!seed || !seed->visible) { log(QStringLiteral("选择轮廓：无效的种子图元"), "warn"); return; }
    const std::vector<int> ids = collectContourIds(seedId);
    const bool allSelected = !ids.empty() &&
        std::all_of(ids.begin(), ids.end(), [this](int id) { return isSelected(id); });
    if (allSelected) {
        std::vector<int> keep;
        for (int id : selectedIds_)
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) keep.push_back(id);
        setSelection(keep);
        log(QStringLiteral("已取消轮廓：%1 个相连图元").arg(ids.size()));
    } else {
        setSelection(ids);
        log(QStringLiteral("已选择轮廓：%1 个相连图元").arg(ids.size()));
    }
}

void SketchModel::selectIntersected(int seedId) {
    const Element* seed = element(seedId);
    if (!seed || !seed->visible) { log(QStringLiteral("选择相交：无效的种子图元"), "warn"); return; }
    std::vector<int> ids;
    for (const auto& e : elems_) {
        if (!e.visible) continue;
        if (e.id == seedId) continue;
        if (e.type == ElementType::Constraint) continue;
        if (!geo::intersectElements(*seed, e).empty()) ids.push_back(e.id);
    }
    if (!ids.empty()) addToSelection(ids);   // 默认并入当前选区
    log(QStringLiteral("已选择 %1 个与种子图元相交的图元").arg(ids.size()));
}

void SketchModel::deselectIntersected(int seedId) {
    const Element* seed = element(seedId);
    if (!seed) return;
    std::vector<int> keep;
    for (int id : selectedIds_) {
        if (id == seedId) { keep.push_back(id); continue; }
        const Element* e = element(id);
        if (!e || e->type == ElementType::Constraint) { keep.push_back(id); continue; }
        if (geo::intersectElements(*seed, *e).empty()) keep.push_back(id);
    }
    setSelection(keep);
    log(QStringLiteral("已取消 %1 个与种子图元相交的图元").arg((int)selectedIds_.size() - (int)keep.size()));
}

void SketchModel::selectLayer(const QString& layer) {
    std::vector<int> ids;
    for (const auto& e : elems_)
        if (e.visible && e.layer == layer) ids.push_back(e.id);
    setSelection(ids);
    log(QStringLiteral("已选择图层 %1 上的 %2 个图元").arg(layer).arg(ids.size()));
}

void SketchModel::selectLayerOf(int seedId) {
    const Element* seed = element(seedId);
    if (!seed) { log(QStringLiteral("选择图层：无效的种子图元"), "warn"); return; }
    selectLayer(seed->layer);
}

void SketchModel::toggleLayerOf(int seedId) {
    // (取消)选择图层：种子所在图层的全部可见图元已全选中 → 整层移除；否则整层选中。
    // 与 LibreCAD 的 (De-)Select Layer 语义一致。
    const Element* seed = element(seedId);
    if (!seed) { log(QStringLiteral("选择图层：无效的种子图元"), "warn"); return; }
    std::vector<int> ids;
    for (const auto& e : elems_)
        if (e.visible && e.layer == seed->layer) ids.push_back(e.id);
    const bool allSelected = !ids.empty() &&
        std::all_of(ids.begin(), ids.end(), [this](int id) { return isSelected(id); });
    if (allSelected) deselectLayer(seed->layer);
    else             selectLayer(seed->layer);
}

void SketchModel::deselectLayer(const QString& layer) {
    std::vector<int> keep;
    for (int id : selectedIds_) {
        const Element* e = element(id);
        if (!e || e->layer != layer) keep.push_back(id);
    }
    const int removed = (int)selectedIds_.size() - (int)keep.size();
    setSelection(keep);
    log(QStringLiteral("已取消图层 %1 上的 %2 个图元").arg(layer).arg(removed));
}

void SketchModel::invertSelection() {
    std::vector<int> ids;
    for (const auto& e : elems_) if (e.visible && !isSelected(e.id)) ids.push_back(e.id);
    setSelection(ids);
    log(QStringLiteral("已反选：%1 个图元").arg(ids.size()));
}

// ---- 累加选择：按相同线型 / 相同颜色，保留已有选择 ----
void SketchModel::addSelectionByStyle(LineStyle ls) {
    std::vector<int> add;
    for (const auto& e : elems_)
        if (e.visible && e.lineStyle == ls) add.push_back(e.id);
    if (add.empty()) { log(QStringLiteral("没有线型为「%1」的图元可累加选择").arg(lineStyleName(ls)), "warn"); return; }
    addToSelection(add);   // 追加且去重：已选中的保留、不清除，满足"累加选择"
    log(QStringLiteral("已按线型「%1」累加选择 %2 个图元（含原有选区）").arg(lineStyleName(ls)).arg(selection().size()));
}

void SketchModel::addSelectionByColor(const QColor& c) {
    const QRgb key = c.rgb();   // 忽略 alpha，仅比较 RGB
    std::vector<int> add;
    for (const auto& e : elems_)
        if (e.visible && (e.color.rgb() == key)) add.push_back(e.id);
    if (add.empty()) { log(QStringLiteral("没有该颜色的图元可累加选择"), "warn"); return; }
    addToSelection(add);   // 追加且去重：保留原有选择
    log(QStringLiteral("已按颜色累加选择，当前共 %1 个图元被选中").arg(selection().size()));
}

// ============================================================
// 多选整体平移：把当前选择集当作一个刚体整体平移 delta
// ============================================================
// 设计要点：
//   * 整组只压一次撤销快照（beginEdit），无论选中几个图元，一次 Ctrl+Z 全退回；
//   * 复用 modify::makeMoveCopy：对选区里每个图元施加同一 delta（替换原 id，
//     保留 id 不变 → 属性面板/BOM 选中态不跳），相对位置天然守恒；
//   * 拖拽平移（GraphicsView 的 Live Quiet 路径）与本条「输入偏移坐标」路径共用
//     makeMoveCopy 语义，行为不会分叉。
void SketchModel::translateSelection(const Vec3& delta, const QString& undoLabel) {
    if (selectedIds_.empty()) return;
    std::vector<Element> src;
    src.reserve(selectedIds_.size());
    for (int id : selectedIds_)
        if (const Element* e = element(id)) src.push_back(*e);
    if (src.empty()) return;
    auto changes = modify::makeMoveCopy(src, delta, /*copy=*/false);
    beginEdit(undoLabel);
    modify::applyChanges(this, changes, QString());
}

int SketchModel::smartDimension() {
    if (selectedIds_.size() != 1) return -1;
    const Element* s = element(selectedIds_.front());
    if (!s) return -1;

    Element el;
    el.type = ElementType::Dimension;
    el.standard = dimensionStandard_;     // 跟随当前标注标准

    if (s->type == ElementType::Line && s->pts.size() >= 2) {
        // 对齐标注：尺寸线平行于直线（量真实长度），尺寸界线自然垂直于直线
        const Vec3 A = s->pts[0], B = s->pts[1];
        Vec3 dir = B - A;
        const double L = std::hypot(dir.x, dir.y);
        if (L < 1e-9) return -1;
        dir = Vec3(dir.x / L, dir.y / L, 0);
        const Vec3 n(-dir.y, dir.x, 0);     // 逆时针 90° = 直线的垂直方向
        const double off = 12.0;            // 与 DimStyle::Style.defOffset 一致
        const Vec3 P = (A + B) * 0.5 + n * off;
        el.name = QStringLiteral("对齐标注");
        applyDefaultStyle(el);
        el.dtype = DimensionType::Aligned;
        el.refA = s->id;          // 关联源图元，使标注数值可驱动几何
        el.pts = {A, B, P};
    } else if ((s->type == ElementType::Circle || s->type == ElementType::Arc) && s->radius > 1e-9) {
        // 直径标注：通过圆心的尺寸线两端各一个箭头，数字前带 ⌀
        const Vec3 C = s->pts[0];
        const double R = s->radius;
        el.name = QStringLiteral("直径标注");
        applyDefaultStyle(el);
        el.dtype = DimensionType::Diameter;
        el.refA = s->id;          // 关联源图元，使标注数值可驱动几何
        el.radius = R;
        el.pts = {C, C + Vec3(R, 0, 0)};
    } else {
        return -1;
    }

    // 标注数字用实物尺寸：走模型唯一的样式入口（已带绘图比例；pts 是图纸坐标）
    const dim::Geom g = dim::build(el, dimStyle());
    if (!g.valid) return -1;
    const int id = addElement(el);
    setSelected(id);
    log(QString("已智能创建%1 #%2 = %3（%4）")
            .arg(el.name).arg(id).arg(g.label)
            .arg(dimensionStandardName(dimensionStandard_)));
    return id;
}
void SketchModel::setActiveTool(const QString& t) {
    if (activeTool_ == t) return;
    activeTool_ = t;
    emit activeToolChanged(t);      // 供工具条同步高亮
    emit statusCommandChanged(t);   // 供状态栏显示当前命令
}

void SketchModel::setCommandPrompt(const QString& p) {
    if (commandPrompt_ != p) {
        commandPrompt_ = p;
        emit commandPromptChanged(p);
    }
}
void SketchModel::setPlane(Plane p) {
    if (plane_ != p) { plane_ = p; emit planeChanged(p); emit viewChanged(); }
}
void SketchModel::setShowGrid(bool b) {
    if (showGrid_ != b) { showGrid_ = b; emit gridToggled(b); emit viewChanged(); }
}
void SketchModel::setViewScale(double s) {
    if (viewScale_ != s) { viewScale_ = s; emit viewChanged(); }
}
void SketchModel::setDefaultColor(const QColor& c) {
    if (!c.isValid() || defaultColor_ == c) return;
    defaultColor_ = c;
    emit defaultStyleChanged(defaultColor_, defaultWidth_);
}
void SketchModel::setDefaultWidth(double px) {
    if (px <= 0) return;
    // 写入"当前默认线型"对应的默认线宽 —— 全局可维护：改一处生效于所有该线型图元。
    LineStyleConfig::instance().setDefaultWidth(defaultLineStyle_, px);
    defaultWidth_ = px;                                  // 缓存，供信号回显
    emit defaultStyleChanged(defaultColor_, defaultWidth_);
}
void SketchModel::setDefaultLineStyle(LineStyle ls) {
    if (defaultLineStyle_ == ls) return;
    defaultLineStyle_ = ls;
    emit defaultLineStyleChanged((int)ls);
}

void SketchModel::setDimensionStandard(DimensionStandard s) {
    if (dimensionStandard_ == s) return;
    dimensionStandard_ = s;
    // 全局切换：把已有标注全部改为当前标准（实时重绘）
    for (auto& e : elems_) {
        if (e.type == ElementType::Dimension) e.standard = s;
    }
    emit dimensionStandardChanged(s);
    emit modelChanged();
}
bool SketchModel::styleSelected(const QColor* c, const double* px) {
    return styleSelectedEx(c, px, nullptr);
}
bool SketchModel::styleSelectedEx(const QColor* c, const double* px, const LineStyle* ls) {
    if (selectedId_ < 0 || !element(selectedId_)) return false;
    return updateElement(selectedId_, [&](Element& e) {
        if (c && c->isValid()) e.color = *c;
        if (px && *px > 0) { e.lineWidth = *px; e.lineWidthAuto = false; }   // 手动覆盖线宽
        if (ls) {
            e.lineStyle = *ls;
            // 仍为自动模式时，线型改变 → 线宽跟随新线型的默认档位
            if (e.lineWidthAuto) e.lineWidth = LineStyleConfig::instance().defaultWidth(*ls);
        }
    });
}
// ============================================================
// 图层管理（Layer）
// 核心不变量：凡是对"图层"做批量属性/可见性操作，都只遍历 e.layer == 该层名 的图元，
// 其他图层图元的内容与显示状态保持不动，满足"仅影响该图层、互不干扰"。
// ============================================================
static const Layer& kDefaultLayer() {
    static const Layer d(QStringLiteral("0"));
    return d;
}

const Layer& SketchModel::layerAt(int i) const {
    if (i < 0 || i >= (int)layers_.size()) return kDefaultLayer();
    return layers_[i];
}

const Layer* SketchModel::findLayer(const QString& name) const {
    for (const auto& l : layers_) if (l.name == name) return &l;
    return nullptr;
}
Layer* SketchModel::findLayer(const QString& name) {
    for (auto& l : layers_) if (l.name == name) return &l;
    return nullptr;
}

bool SketchModel::addLayer(const QString& name, const QColor& color,
                           double lineWidth, LineStyle ls) {
    const QString n = name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("新建图层失败：名称不能为空"), "warn"); return false; }
    if (findLayer(n)) { log(QStringLiteral("新建图层失败：图层 %1 已存在").arg(n), "warn"); return false; }
    beginEdit(QStringLiteral("新建图层 ") + n);
    Layer l(n);
    if (color.isValid()) l.color = color;
    l.lineWidth = lineWidth > 0 ? lineWidth : 1.0;
    l.lineStyle = ls;
    layers_.push_back(l);
    emit layersChanged();
    log(QStringLiteral("已新建图层：%1").arg(n));
    return true;
}

bool SketchModel::ensureLayer(const QString& name) {
    const QString n = name.trimmed();
    if (n.isEmpty()) return false;
    if (findLayer(n)) return true;
    return addLayer(n);
}

bool SketchModel::renameLayer(const QString& oldName, const QString& newName) {
    const QString on = oldName.trimmed(), nn = newName.trimmed();
    if (on == QStringLiteral("0")) { log(QStringLiteral("默认层 0 不可改名"), "warn"); return false; }
    if (on.isEmpty() || nn.isEmpty()) return false;
    Layer* ol = findLayer(on);
    if (!ol) return false;
    if (findLayer(nn) && nn != on) { log(QStringLiteral("改名失败：图层 %1 已存在").arg(nn), "warn"); return false; }
    // 先压快照（记录旧名），再改图层名与图元归属：撤销时图层名与图元一起回退
    beginEdit(QStringLiteral("重命名图层 ") + on);
    ol->name = nn;
    // 同步图元归属：只改 e.layer == on 的图元
    for (auto& e : elems_) if (e.layer == on) e.layer = nn;
    emit layersChanged(); emit modelChanged();
    log(QStringLiteral("图层 %1 已改名为 %2").arg(on).arg(nn));
    return true;
}

bool SketchModel::removeLayer(const QString& name) {
    const QString n = name.trimmed();
    if (n == QStringLiteral("0")) { log(QStringLiteral("默认层 0 不可删除"), "warn"); return false; }
    auto it = std::find_if(layers_.begin(), layers_.end(),
                           [&](const Layer& l) { return l.name == n; });
    if (it == layers_.end()) return false;
    beginEdit(QStringLiteral("删除图层 ") + n);
    for (auto& e : elems_) if (e.layer == n) e.layer = QStringLiteral("0");   // 回落默认层
    layers_.erase(it);
    emit layersChanged(); emit modelChanged();
    log(QStringLiteral("已删除图层：%1（其图元回落到层 0）").arg(n));
    return true;
}

bool SketchModel::setLayerStyle(const QString& name, const QColor* color,
                                const double* lineWidth, const LineStyle* ls) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    // 没有任何字段要改，直接返回
    if (!color && !lineWidth && !ls) return true;
    // 单层操作也要可撤销：先压快照（记录旧图层定义与本层图元样式），再改
    beginEdit(QStringLiteral("修改图层样式 ") + name);
    if (color)     l->color = *color;
    if (lineWidth) l->lineWidth = *lineWidth > 0 ? *lineWidth : l->lineWidth;
    if (ls)        l->lineStyle = *ls;
    // 同步：把本层图元的「对应属性」更新为图层新值。
    // 只遍历 e.layer == name 的图元，其他图层图元纹丝不动（互不干扰）。
    // 用指针判空，保证只有被改动的对应属性被推送，未指定的属性（含图元的单独覆盖）保留不动。
    for (auto& e : elems_) {
        if (e.layer != name) continue;
        if (color)      e.color = *color;
        if (lineWidth)  { e.lineWidth = *lineWidth; e.lineWidthAuto = false; }
        if (ls)         e.lineStyle = *ls;
    }
    emit layersChanged(); emit modelChanged();   // modelChanged 触发重绘
    log(QStringLiteral("已修改图层 %1 的样式并同步到本层图元").arg(name));
    return true;
}

bool SketchModel::setLayerVisible(const QString& name, bool visible) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    if (l->visible == visible) return true;
    // 先压快照（记录旧 visible），再改图层定义与图元：撤销时图层定义与图元一起回退
    beginEdit((visible ? QStringLiteral("显示图层 ") : QStringLiteral("隐藏图层 ")) + name);
    l->visible = visible;
    // 批量翻转本层图元的 visible：只遍历 e.layer == name，其他层不动
    for (auto& e : elems_) if (e.layer == name) e.visible = visible;
    emit layersChanged(); emit modelChanged();   // modelChanged 触发重绘（GraphicsView 读 e.visible）
    log(QStringLiteral("%1图层 %2").arg(visible ? "已显示" : "已隐藏").arg(name));
    return true;
}

bool SketchModel::setLayerLocked(const QString& name, bool locked) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    beginEdit((locked ? QStringLiteral("锁定图层 ") : QStringLiteral("解锁图层 ")) + name);
    l->locked = locked;
    emit layersChanged();
    log(QStringLiteral("%1图层 %2").arg(locked ? "已锁定" : "已解锁").arg(name));
    return true;
}

// 把图层样式套用到该层全部图元（批量改属性，仅影响本层），返回受影响图元数
int SketchModel::applyLayerStyleToElements(const QString& name) {
    const Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return 0; }
    int n = 0;
    for (const auto& e : elems_) if (e.layer == name) ++n;
    if (n == 0) { log(QStringLiteral("图层 %1 暂无图元").arg(name), "info"); return 0; }
    beginEdit(QStringLiteral("应用图层样式 ") + name);
    for (auto& e : elems_) if (e.layer == name) l->applyTo(e);
    emit layersChanged(); emit modelChanged();
    log(QStringLiteral("已将图层 %1 的样式套用到 %2 个图元").arg(name).arg(n));
    return n;
}

// 把当前选择集移入指定图层；adoptStyle=true 时图元同步采用该层样式
int SketchModel::assignSelectionToLayer(const QString& name, bool adoptStyle) {
    const Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return 0; }
    std::vector<int> ids;
    for (int id : selectedIds_) if (element(id)) ids.push_back(id);
    if (ids.empty()) { log(QStringLiteral("没有选中任何图元可移入图层 %1").arg(name), "warn"); return 0; }
    beginEdit(QStringLiteral("移入图层 ") + name);
    for (int id : ids) {
        auto it = find(id);
        if (it == elems_.end()) continue;
        it->layer = name;
        if (adoptStyle) l->applyTo(*it);
    }
    emit layersChanged(); emit modelChanged(); emit selectionChanged(selectedId_);
    log(QStringLiteral("已将 %1 个选中图元移入图层 %2").arg(ids.size()).arg(name));
    return (int)ids.size();
}

bool SketchModel::setElementLayer(int id, const QString& name, bool adoptStyle) {
    return setElementLayers({id}, name, adoptStyle);
}

bool SketchModel::setElementLayers(const std::vector<int>& ids, const QString& name, bool adoptStyle) {
    const Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    std::vector<int> valid;
    for (int id : ids) if (element(id)) valid.push_back(id);
    if (valid.empty()) return false;
    beginEdit(QStringLiteral("移入图层 ") + name);
    for (int id : valid) {
        auto it = find(id);
        if (it == elems_.end()) continue;
        it->layer = name;
        if (adoptStyle) l->applyTo(*it);
    }
    emit layersChanged(); emit modelChanged();
    return true;
}

QJsonArray SketchModel::layersToJson() const {
    QJsonArray arr;
    for (const auto& l : layers_) {
        QJsonObject o;
        o["name"]      = l.name;
        o["color"]     = l.color.name();
        o["lineWidth"] = l.lineWidth;
        o["lineStyle"] = (int)l.lineStyle;
        o["visible"]   = l.visible;
        o["locked"]    = l.locked;
        arr.append(o);
    }
    return arr;
}

void SketchModel::setLayersFromJson(const QJsonArray& arr) {
    layers_.clear();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        Layer l(o["name"].toString());
        const QString cs = o["color"].toString();
        if (!cs.isEmpty()) l.color = QColor(cs);
        l.lineWidth = o.contains("lineWidth") ? o["lineWidth"].toDouble(1.0) : 1.0;
        l.lineStyle  = o.contains("lineStyle") ? (LineStyle)o["lineStyle"].toInt(0) : LineStyle::Solid;
        l.visible    = o.contains("visible")   ? o["visible"].toBool(true)        : true;
        l.locked     = o.contains("locked")    ? o["locked"].toBool(false)        : false;
        layers_.push_back(l);
    }
    // 保证默认层 "0" 始终存在（旧文件可能未保存该字段）
    if (!findLayer(QStringLiteral("0"))) layers_.insert(layers_.begin(), Layer(QStringLiteral("0")));
    emit layersChanged();
}

void SketchModel::setChainLine(bool b) {
    if (chainLine_ == b) return;
    chainLine_ = b;
    emit chainLineToggled(b);
}

// ---- 直线绘制参数输入条 ----
void SketchModel::setLineParamAngle(double v) {
    if (qFuzzyCompare(lp_.angleDeg, v)) return;
    lp_.angleDeg = v; emit lineParamsChanged();
}
void SketchModel::setLineParamLength(double v) {
    if (qFuzzyCompare(lp_.length, v)) return;
    lp_.length = v; emit lineParamsChanged();
}
void SketchModel::setLineParamSpacing(double v) {
    if (qFuzzyCompare(lp_.spacing, v)) return;
    lp_.spacing = v; emit lineParamsChanged();
}
void SketchModel::setLineParamCount(int v) {
    if (lp_.count == v) return;
    lp_.count = v; emit lineParamsChanged();
}

void SketchModel::log(const QString& m, const QString& t) { emit logAppended(m, t); }
void SketchModel::setStatusCommand(const QString& c) { emit statusCommandChanged(c); }

void SketchModel::undo() {
    if (undoStack_.empty()) { log(QStringLiteral("无可撤销操作"), "warn"); return; }
    Memento rb; rb.elems = elems_; rb.nextId = nextId_; rb.scale = scaleRatio_; rb.layers = layers_;
    redoStack_.push_back(std::move(rb));
    auto m = undoStack_.back(); undoStack_.pop_back();
    elems_ = std::move(m.elems); nextId_ = m.nextId; layers_ = std::move(m.layers);
    const bool scaleDiff = std::fabs(m.scale - scaleRatio_) > 1e-12;
    scaleRatio_ = m.scale;
    selectedIds_.clear(); selectedId_ = -1;
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged();
    if (scaleDiff) emit scaleChanged(scaleRatio_);
    log(QStringLiteral("已撤销: ") + m.label);
}
void SketchModel::redo() {
    if (redoStack_.empty()) { log(QStringLiteral("无可重做操作"), "warn"); return; }
    Memento rb; rb.elems = elems_; rb.nextId = nextId_; rb.scale = scaleRatio_; rb.layers = layers_;
    undoStack_.push_back(std::move(rb));
    auto m = redoStack_.back(); redoStack_.pop_back();
    elems_ = std::move(m.elems); nextId_ = m.nextId; layers_ = std::move(m.layers);
    const bool scaleDiff = std::fabs(m.scale - scaleRatio_) > 1e-12;
    scaleRatio_ = m.scale;
    selectedIds_.clear(); selectedId_ = -1;
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged();
    if (scaleDiff) emit scaleChanged(scaleRatio_);
    log(QStringLiteral("已重做: ") + m.label);
}

void SketchModel::copySelected() {
    auto s = selected();
    if (s) { clipboard_ = *s; hasClip_ = true; log(QStringLiteral("已复制: ") + s->name); }
    else log(QStringLiteral("无可复制的选中图元"), "warn");
}
void SketchModel::paste() {
    if (!hasClip_) { log(QStringLiteral("剪贴板为空"), "warn"); return; }
    Element e = clipboard_; e.id = 0; e.name += QStringLiteral("_副本");
    int id = addElement(e); setSelected(id);
    log(QStringLiteral("已粘贴: ") + e.name);
}
void SketchModel::deleteSelected() {
    if (selectedId_ < 0) { log(QStringLiteral("未选中要删除的图元"), "warn"); return; }
    removeElement(selectedId_);
    log(QStringLiteral("已删除选中图元"));
}
void SketchModel::reorderElement(int id, int beforeId) {
    auto it = find(id);
    if (it == elems_.end()) return;
    Element e = *it;
    elems_.erase(it);
    if (beforeId < 0) {
        elems_.push_back(e);
    } else {
        auto bit = find(beforeId);
        if (bit == elems_.end()) elems_.push_back(e);
        else elems_.insert(bit, e);
    }
    emit modelChanged();
}
void SketchModel::clear() {
    scaleRatio_ = 1.0;          // 清空即回到默认 1:1（新建图纸契约）
    if (elems_.empty()) return;
    pushMemento(QStringLiteral("清空"));
    elems_.clear(); selectedIds_.clear(); selectedId_ = -1;
    layers_.clear(); layers_.push_back(Layer(QStringLiteral("0")));   // 新建图纸只留默认层
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged();
}

void SketchModel::emitModelChanged() { emit modelChanged(); }

// 比例格式化：1.0 -> "1:1", 0.5 -> "1:2", 2.0 -> "2:1"（GB/T 14690 比例记法）
static QString formatScaleLabel(double r) {
    if (std::fabs(r - 1.0) < 1e-9) return QStringLiteral("1:1");
    if (r < 1.0) {
        double inv = 1.0 / r;
        long invI = std::lround(inv);
        if (std::fabs(inv - invI) < 1e-6) return QString("1:%1").arg(invI);
        return QString("1:%1").arg(inv, 0, 'f', 3);
    }
    long rI = std::lround(r);
    if (std::fabs(r - rI) < 1e-6) return QString("%1:1").arg(rI);
    return QString("%1:1").arg(r, 0, 'f', 3);
}

QString SketchModel::scaleLabel() const { return formatScaleLabel(scaleRatio_); }

// 标注样式唯一构造入口：带上当前绘图比例，保证标注数字是实物尺寸（GB/T 14690）
dim::Style SketchModel::dimStyle() const {
    dim::Style st;
    st.drawingScale = scaleRatio_;
    return st;
}

// 修改绘图比例：把已有图元（不含图框/标题栏）按内容包围盒中心等比缩放。
// factor = 新比例 / 旧比例：1:1 -> 1:2 时 factor=0.5（图缩小一半），
// 1:1 -> 2:1 时 factor=2.0（图放大一倍）。图框/标题栏代表固定纸张，不随比例缩放。
void SketchModel::setScaleRatio(double r, bool allowUndo) {
    if (r <= 0) return;
    if (std::fabs(r - scaleRatio_) < 1e-9) return;
    const double factor = r / scaleRatio_;

    QRectF box;
    bool hasContent = false;
    for (const auto& e : elems_) {
        if (e.layer == QStringLiteral("图框") || e.layer == QStringLiteral("标题栏")) continue;
        const QRectF b = geo::elementBounds(e);
        if (!hasContent) { box = b; hasContent = true; }
        else box = box.united(b);
    }
    const Vec3 c(hasContent ? box.center().x() : 0.0,
                hasContent ? box.center().y() : 0.0, 0.0);

    if (allowUndo) beginEdit(QStringLiteral("修改比例 %1").arg(scaleLabel()));
    for (auto& e : elems_) {
        if (e.layer == QStringLiteral("图框") || e.layer == QStringLiteral("标题栏")) continue;
        for (auto& p : e.pts) {
            p.x = c.x + (p.x - c.x) * factor;
            p.y = c.y + (p.y - c.y) * factor;
        }
        // 长度量字段随比例缩放。它们与 pts 同处「图纸坐标系」，必须一起缩放，
        // 否则会出现「两份数据漂移」：pts 缩了、缓存的 radius 没缩，
        // 半径/直径/弧长标注就会按旧半径算出错误数字。
        // 注意 lineWidth / tolUpper / tolLower 是「图纸固定量 / 实物尺寸公差」，
        // 不属于这里的长度量，不参与缩放。
        switch (e.type) {
            case ElementType::Circle:
            case ElementType::Arc:
            case ElementType::Text:       // 文字高度
            case ElementType::Dimension:  // 半径/直径/弧长标注缓存的半径
                e.radius *= factor;
                break;
            case ElementType::Ellipse:
                e.radius *= factor;
                e.radiusY *= factor;
                break;
            default:
                break;
        }
    }
    scaleRatio_ = r;
    emit modelChanged();
    emit scaleChanged(r);
}

// 只改比例值、不触碰几何。读档专用（见头文件说明）。
void SketchModel::setScaleRatioValue(double r) {
    if (r <= 0.0) return;
    if (std::fabs(r - scaleRatio_) < 1e-12) return;
    scaleRatio_ = r;
    emit scaleChanged(r);
}

} // namespace SketchCAD
