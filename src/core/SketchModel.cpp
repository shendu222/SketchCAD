#include "SketchModel.h"
#include "Geometry.h"
#include "ConstraintSolver.h"
#include "ConstraintSolverFacade.h"   // 约束求解路由门面（特性开关+回落）
#include "DimStyle.h"
#include "Modify.h"
#include "Section.h"    // buildClosedLoopsFromElements：填充边界重算（关联填充随动）
#include "Xref.h"
#include "TextStyle.h"  // FIELD 域：refreshFields 预热求值（resolveFields/displayTableCell）
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace SketchCAD {

SketchModel::SketchModel(QObject* p) : QObject(p) {
    layers_.push_back(Layer(QStringLiteral("0")));   // 默认层始终存在
    // 文字样式表默认含一条 "Standard"（与 AutoCAD 一致）：空字体族 = 走系统默认，
    // 字高 0 = 用图元自身 radius，宽度因子 1，无倾斜，非粗/斜体。
    textStyles_.push_back(TextStyle{QStringLiteral("Standard"), QString(), 0.0, 1.0, 0.0, false, false});
    // 标注样式表默认含一条 "Standard"（GB/T 4458.4 国标默认）：保证 dimStyleFor 永不为空，
    // 旧档读入（无 dimStyles 键）也回落成这一张，语义等价于原"单全局 DimStyle"。
    {
        dim::DimStyleEntry d;
        d.name  = QStringLiteral("Standard");
        d.style = dim::Style();
        dimStyles_.push_back(std::move(d));
    }
    currentDimStyle_ = QStringLiteral("Standard");
}

std::vector<Element>::iterator SketchModel::find(int id) {
    return std::find_if(elems_.begin(), elems_.end(),
                        [id](const Element& e) { return e.id == id; });
}

// ============================================================================
// 撤销快照去重：精确相等的逐字段比较
// ----------------------------------------------------------------------------
// 安全方向是单向的：**无法证明相同，就判为不同**。
//   判为不同 → 照常入栈，最坏只是少去重一次（多一步无意义的 Ctrl+Z，无害）；
//   判为相同 → 跳过入栈，一旦判错就永久吞掉用户的一步撤销（致命）。
// 因此这里宁可写全，也不做任何"看起来差不多"的近似（不比 JSON 摘要的模糊等价、
// 不比容器 size 了事——改一个图层的名字、挪一个图元，size 都不变）。
//
// 双精度用 == 精确比较是安全的：待比较的两份快照都是同一份数据的副本链，
// 未被改动过的字段必然逐位相同；而"改过但值恰好相等"的情形由各 setter 的幂等守卫挡住。
// ============================================================================

// 深比较的元素数上限：超过则直接判为"不同"（照常入栈）。
// 大图纸下逐图元比较是 O(N)，而 pushMemento 本身还要复制一份；去重只是优化，
// 不能反过来成为编辑卡顿的来源。拒绝去重是安全方向（少优化，不丢步）。
static constexpr size_t kDedupDeepElemsLimit = 20000;

static bool sameVec3s(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}
static bool sameStrings(const std::vector<QString>& a, const std::vector<QString>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

// 逐字段比较图元。Element 字段只增不改：新增字段时必须同步这里，
// 否则"只改了新字段"的两次快照会被误判为相同（吞掉一步撤销）。
static bool sameElement(const Element& a, const Element& b) {
    if (a.id != b.id || a.type != b.type || a.name != b.name || a.layer != b.layer) return false;
    if (a.color != b.color) return false;
    if (a.lineWidth != b.lineWidth || a.lineStyle != b.lineStyle || a.lineWidthAuto != b.lineWidthAuto) return false;
    if (!sameVec3s(a.pts, b.pts)) return false;
    if (a.radius != b.radius || a.startAngle != b.startAngle || a.endAngle != b.endAngle) return false;
    if (a.radiusY != b.radiusY || a.rotation != b.rotation || a.closed != b.closed) return false;
    if (a.splineSubType != b.splineSubType || a.ctype != b.ctype
        || a.dtype != b.dtype || a.standard != b.standard) return false;
    if (a.text != b.text) return false;
    if (a.tolUpper != b.tolUpper || a.tolLower != b.tolLower || a.dimTextHeight != b.dimTextHeight) return false;
    if (a.fitCode != b.fitCode || a.symbolLibrary != b.symbolLibrary) return false;
    if (a.refA != b.refA || a.refB != b.refB || a.visible != b.visible || a.byLayer != b.byLayer) return false;
    if (a.dimValue != b.dimValue || a.dimDriven != b.dimDriven
        || a.dimParam != b.dimParam || a.dimLinked != b.dimLinked) return false;
    if (a.widthFactor != b.widthFactor || a.textAlign != b.textAlign || a.styleName != b.styleName) return false;
    if (a.lineSpacing != b.lineSpacing || a.mtextWidth != b.mtextWidth || a.groupName != b.groupName) return false;
    if (a.tableRows != b.tableRows || a.tableCols != b.tableCols || a.rowHeight != b.rowHeight) return false;
    if (a.colWidths != b.colWidths) return false;                 // std::vector<double>
    if (!sameStrings(a.tableCells, b.tableCells)) return false;
    if (a.dimStyleName != b.dimStyleName) return false;
    if (a.mleaderStyleName != b.mleaderStyleName
        || a.mleaderLanding != b.mleaderLanding || a.mleaderArrowLen != b.mleaderArrowLen) return false;
    if (a.fcfSymbol != b.fcfSymbol || a.fcfModifier != b.fcfModifier || a.fcfTolerance != b.fcfTolerance) return false;
    if (!sameStrings(a.fcfDatums, b.fcfDatums)) return false;
    if (a.arcFlags != b.arcFlags) return false;                   // std::vector<bool>
    if (a.holes.size() != b.holes.size()) return false;
    for (size_t i = 0; i < a.holes.size(); ++i)
        if (!sameVec3s(a.holes[i], b.holes[i])) return false;
    if (a.sectionExempt != b.sectionExempt || a.sectionMaterial != b.sectionMaterial) return false;
    if (a.hatchSrcIds != b.hatchSrcIds) return false;             // std::vector<int>
    if (a.hatchPattern != b.hatchPattern || a.hatchScale != b.hatchScale) return false;
    if (a.hatchAssociative != b.hatchAssociative || a.hatchInvalid != b.hatchInvalid) return false;
    if (a.blockName != b.blockName || a.scaleX != b.scaleX || a.scaleY != b.scaleY) return false;
    if (a.blockAttrs.size() != b.blockAttrs.size()) return false;
    for (size_t i = 0; i < a.blockAttrs.size(); ++i) {
        if (a.blockAttrs[i].tag != b.blockAttrs[i].tag) return false;
        if (a.blockAttrs[i].value != b.blockAttrs[i].value) return false;
    }
    if (a.dynValues.size() != b.dynValues.size()) return false;
    for (size_t i = 0; i < a.dynValues.size(); ++i) {
        if (a.dynValues[i].name != b.dynValues[i].name) return false;
        if (a.dynValues[i].value != b.dynValues[i].value) return false;
        if (a.dynValues[i].state != b.dynValues[i].state) return false;
    }
    return true;
}

static bool sameLayer(const Layer& a, const Layer& b) {
    return a.name == b.name && a.color == b.color && a.lineWidth == b.lineWidth
        && a.lineStyle == b.lineStyle && a.visible == b.visible && a.locked == b.locked
        && a.frozen == b.frozen && a.plot == b.plot && a.alpha == b.alpha
        && a.code == b.code && a.standardName == b.standardName;
}

static bool sameTextStyle(const TextStyle& a, const TextStyle& b) {
    return a.name == b.name && a.fontFamily == b.fontFamily && a.textHeight == b.textHeight
        && a.widthFactor == b.widthFactor && a.obliquingAngle == b.obliquingAngle
        && a.bold == b.bold && a.italic == b.italic;
}

static bool sameBlockDef(const BlockDef& a, const BlockDef& b) {
    if (a.name != b.name || a.description != b.description || !(a.basePoint == b.basePoint)) return false;
    if (a.annotative != b.annotative) return false;
    // Xref 附加属性：外部参照就是块表记录（BlockDef::isXref），随块表一并回退
    if (a.isXref != b.isXref || a.xrefPath != b.xrefPath || a.xrefPathType != b.xrefPathType) return false;
    if (a.xrefNested != b.xrefNested || a.xrefStatus != b.xrefStatus) return false;
    if (a.entities.size() != b.entities.size()) return false;
    for (size_t i = 0; i < a.entities.size(); ++i)
        if (!sameElement(a.entities[i], b.entities[i])) return false;
    if (a.attdefs.size() != b.attdefs.size()) return false;
    for (size_t i = 0; i < a.attdefs.size(); ++i) {
        const AttributeDef& x = a.attdefs[i];
        const AttributeDef& y = b.attdefs[i];
        if (x.tag != y.tag || x.prompt != y.prompt || x.defaultValue != y.defaultValue) return false;
        if (!(x.pos == y.pos) || x.height != y.height) return false;
        if (x.constant != y.constant || x.invisible != y.invisible) return false;
    }
    if (a.dynParams.size() != b.dynParams.size()) return false;
    for (size_t i = 0; i < a.dynParams.size(); ++i) {
        const DynParam& x = a.dynParams[i];
        const DynParam& y = b.dynParams[i];
        if (x.name != y.name || x.type != y.type || x.affected != y.affected) return false;
        if (!(x.base == y.base) || !(x.direction == y.direction)) return false;
        if (x.defaultValue != y.defaultValue || x.minValue != y.minValue || x.maxValue != y.maxValue) return false;
        if (x.states.size() != y.states.size()) return false;
        for (size_t j = 0; j < x.states.size(); ++j) {
            if (x.states[j].name != y.states[j].name) return false;
            if (x.states[j].hiddenEntities != y.states[j].hiddenEntities) return false;
        }
    }
    return true;
}

static bool sameFilter(const FilterDef& a, const FilterDef& b) {
    if (a.name != b.name || a.matchAll != b.matchAll || a.conds.size() != b.conds.size()) return false;
    for (size_t i = 0; i < a.conds.size(); ++i) {
        if (a.conds[i].prop != b.conds[i].prop) return false;
        if (a.conds[i].op != b.conds[i].op) return false;
        if (a.conds[i].value != b.conds[i].value) return false;
    }
    return true;
}

// 两张"字段很多但条数很少"的表（标注样式 / 布局）：手写逐字段比较不划算且易漏，
// 改走它们**自己的** toJson —— 序列化是这批结构的完整定义（漏字段 = 存盘丢数据，
// 早就是必须为零的缺陷），因此 JSON 全等 ⟺ 结构全等。条数个位数，开销可忽略。
static QByteArray jsonCompact(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
static bool sameDimStyles(const std::vector<dim::DimStyleEntry>& a, const std::vector<dim::DimStyleEntry>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (jsonCompact(dim::dimStyleEntryToJson(a[i])) != jsonCompact(dim::dimStyleEntryToJson(b[i]))) return false;
    return true;
}
static bool sameLayouts(const std::vector<PlotLayoutDef>& a, const std::vector<PlotLayoutDef>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (jsonCompact(a[i].toJson()) != jsonCompact(b[i].toJson())) return false;
    return true;
}

bool SketchModel::sameState(const Memento& a, const Memento& b) {
    // 先比廉价且最易分化的：绝大多数入栈在这一层就分出胜负，不碰大数组。
    if (a.nextId != b.nextId || a.scale != b.scale) return false;
    if (a.currentDimStyle != b.currentDimStyle || a.currentLayout != b.currentLayout) return false;
    if (!(a.units == b.units) || !(a.limits == b.limits)) return false;
    if (!(a.snapConfig == b.snapConfig) || !(a.trackConfig == b.trackConfig)) return false;
    if (a.params != b.params) return false;                       // std::map 自带 ==
    if (a.elems.size() != b.elems.size() || a.layers.size() != b.layers.size()) return false;
    if (a.blocks.size() != b.blocks.size() || a.textStyles.size() != b.textStyles.size()) return false;
    if (a.dimStyles.size() != b.dimStyles.size() || a.layouts.size() != b.layouts.size()) return false;
    if (a.savedFilters.size() != b.savedFilters.size()) return false;
    // 大图纸：放弃深比较，判为"不同" → 照常入栈（安全方向）
    if (a.elems.size() > kDedupDeepElemsLimit) return false;
    for (size_t i = 0; i < a.elems.size(); ++i)
        if (!sameElement(a.elems[i], b.elems[i])) return false;
    for (size_t i = 0; i < a.layers.size(); ++i)
        if (!sameLayer(a.layers[i], b.layers[i])) return false;
    for (size_t i = 0; i < a.textStyles.size(); ++i)
        if (!sameTextStyle(a.textStyles[i], b.textStyles[i])) return false;
    for (size_t i = 0; i < a.blocks.size(); ++i)
        if (!sameBlockDef(a.blocks[i], b.blocks[i])) return false;
    if (!sameDimStyles(a.dimStyles, b.dimStyles)) return false;
    if (!sameLayouts(a.layouts, b.layouts)) return false;
    for (size_t i = 0; i < a.savedFilters.size(); ++i)
        if (!sameFilter(a.savedFilters[i], b.savedFilters[i])) return false;
    return true;
}

// AuxState 精确相等（封印去重用）：与 sameState 的 aux 部分逐字段同源。
// 字段只增不改：Memento/aux 增字段时两处必须同步，否则"只改了新字段"的
// 无操作步会被误判为有变更（多一步无意义撤销，无害但不干净）。
static bool sameAuxState(const AuxState& a, const AuxState& b) {
    if (a.nextId != b.nextId || a.scale != b.scale) return false;
    if (a.currentDimStyle != b.currentDimStyle || a.currentLayout != b.currentLayout) return false;
    if (!(a.units == b.units) || !(a.limits == b.limits)) return false;
    if (!(a.snapConfig == b.snapConfig) || !(a.trackConfig == b.trackConfig)) return false;
    if (a.params != b.params) return false;                       // std::map 自带 ==
    if (a.layers.size() != b.layers.size() || a.blocks.size() != b.blocks.size()) return false;
    if (a.textStyles.size() != b.textStyles.size() || a.dimStyles.size() != b.dimStyles.size()) return false;
    if (a.layouts.size() != b.layouts.size() || a.savedFilters.size() != b.savedFilters.size()) return false;
    for (size_t i = 0; i < a.layers.size(); ++i)
        if (!sameLayer(a.layers[i], b.layers[i])) return false;
    for (size_t i = 0; i < a.textStyles.size(); ++i)
        if (!sameTextStyle(a.textStyles[i], b.textStyles[i])) return false;
    for (size_t i = 0; i < a.blocks.size(); ++i)
        if (!sameBlockDef(a.blocks[i], b.blocks[i])) return false;
    if (!sameDimStyles(a.dimStyles, b.dimStyles)) return false;
    if (!sameLayouts(a.layouts, b.layouts)) return false;
    for (size_t i = 0; i < a.savedFilters.size(); ++i)
        if (!sameFilter(a.savedFilters[i], b.savedFilters[i])) return false;
    return true;
}

void SketchModel::pushMemento(const QString& label) {
    // 批量载入/反序列化：调用方已 beginEdit() 建立唯一编辑步，此处禁止再入栈，
    // 否则一次「打开图纸」会被拆成若干步 Ctrl+Z（几何回了、配置还留在半路）。
    if (undoSuppressed_) return;

    // ---- 全量模式：保留既有行为，整张 Memento 入栈（用于与差量模式对照）----
    if (!deltaUndoEnabled_) {
        Memento m;
        m.label = label; m.elems = elems_; m.nextId = nextId_;
        m.scale = scaleRatio_; m.layers = layers_; m.blocks = blocks_; m.textStyles = textStyles_; m.params = params_;
        m.dimStyles = dimStyles_; m.currentDimStyle = currentDimStyle_;
        m.layouts = layouts_; m.currentLayout = currentLayout_;
        m.units = units_; m.limits = limits_;
        m.snapConfig = snapConfig_; m.trackConfig = trackConfig_;
        m.savedFilters = savedFilters_;
        if (!undoStack_.empty() && sameState(undoStack_.back().snap, m)) return;
        UndoStep s; s.isCheckpoint = true; s.snap = std::move(m); s.delta.label = label;
        undoStack_.push_back(std::move(s));
        trimUndoStackToLimit();
        redoStack_.clear();
        return;
    }

    // ---- 差量模式：出向差量 + 惰性封印 ----
    // 1) 先封印上一个未封印条目：它的出向差量 delta(E_k→E_{k+1}) 正是上一步操作的
    //    变更集（pendingBase_ = E_k，当前 = E_{k+1}）。封印结果为空 ⟺ 上一步是无操作
    //    （与旧 sameState 去重同义）→ 原样早退：不建新条目、不 trim、不清 redo，
    //    栈顶条目保持未封印、基态不变，下一步真实操作的变更集仍封进同一条目。
    // 2) 再建新条目：只捕获 label + auxBefore + 「改前」基态（elems 深拷贝一次，
    //    与旧实现每次 push 的 currentMemento() 同价）；每 50 步仍落一个全量检查点。
    //    出向差量留给下一次 push / undo 封印——push 从此不再 reconstruct + 全量 diff。
    const int sealed = sealPendingStep();
    if (sealed == 0) return;   // 无操作步：与旧 sameState 去重逐字节对应（不清 redo）
    UndoStep s;
    s.isCheckpoint = ((int)undoStack_.size() % kUndoCheckpointInterval == 0);
    if (s.isCheckpoint) {
        s.snap = currentMemento();
        s.snap.label = label;
    }
    s.delta.label = label;
    s.delta.auxBefore = captureAux();
    undoStack_.push_back(std::move(s));
    pendingBase_ = currentMemento();
    unsealedStep_ = true;
    trimUndoStackToLimit();
    redoStack_.clear();
}

// 撤销栈深度的合法区间与默认值。AppSettings 的校验器与之保持一致（唯一真源在此）。
// 下限取 1：0 步等于关掉撤销，会让 Ctrl+Z 变成"什么都没发生"，比留 1 步更难理解。
static constexpr int kUndoDepthMin     = 1;
static constexpr int kUndoDepthMax     = 1000;
static constexpr int kUndoDepthDefault = 100;

void SketchModel::setUndoDeltaEnabled(bool on) {
    if (on == deltaUndoEnabled_) return;
    deltaUndoEnabled_ = on;
    // 两种模式栈表示不互换，切换唯一安全做法：清空撤销历史（与 setUndoDepthLimit 收敛同理）。
    undoStack_.clear();
    redoStack_.clear();
    unsealedStep_ = false;      // 未封印条目随栈一并作废
    pendingBase_ = Memento{};
}

void SketchModel::setUndoDepthLimit(int steps) {
    int v = (steps >= kUndoDepthMin && steps <= kUndoDepthMax) ? steps : kUndoDepthDefault;
    if (v == undoDepthLimit_) return;
    undoDepthLimit_ = v;
    trimUndoStackToLimit();
    trimRedoStackToLimit();
}

// ===================== 撤销差量化内部原语 =====================
SketchModel::Memento SketchModel::currentMemento() const {
    Memento m;
    m.elems = elems_; m.nextId = nextId_; m.scale = scaleRatio_;
    m.layers = layers_; m.blocks = blocks_; m.textStyles = textStyles_; m.params = params_;
    m.dimStyles = dimStyles_; m.currentDimStyle = currentDimStyle_;
    m.layouts = layouts_; m.currentLayout = currentLayout_;
    m.units = units_; m.limits = limits_; m.snapConfig = snapConfig_; m.trackConfig = trackConfig_;
    m.savedFilters = savedFilters_;
    return m;
}

AuxState SketchModel::captureAux() const {
    AuxState a;
    a.nextId = nextId_; a.scale = scaleRatio_;
    a.layers = layers_; a.blocks = blocks_; a.textStyles = textStyles_;
    a.params = params_; a.dimStyles = dimStyles_; a.currentDimStyle = currentDimStyle_;
    a.layouts = layouts_; a.currentLayout = currentLayout_;
    a.units = units_; a.limits = limits_; a.snapConfig = snapConfig_; a.trackConfig = trackConfig_;
    a.savedFilters = savedFilters_;
    return a;
}

void SketchModel::applyAux(const AuxState& a) {
    nextId_ = a.nextId; scaleRatio_ = a.scale;
    layers_ = a.layers; blocks_ = a.blocks; textStyles_ = a.textStyles;
    params_ = a.params; dimStyles_ = a.dimStyles; currentDimStyle_ = a.currentDimStyle;
    layouts_ = a.layouts; currentLayout_ = a.currentLayout;
    units_ = a.units; limits_ = a.limits; snapConfig_ = a.snapConfig; trackConfig_ = a.trackConfig;
    savedFilters_ = a.savedFilters;
}

void SketchModel::mementoAuxTo(const Memento& m, AuxState& a) {
    a.nextId = m.nextId; a.scale = m.scale;
    a.layers = m.layers; a.blocks = m.blocks; a.textStyles = m.textStyles;
    a.params = m.params; a.dimStyles = m.dimStyles; a.currentDimStyle = m.currentDimStyle;
    a.layouts = m.layouts; a.currentLayout = m.currentLayout;
    a.units = m.units; a.limits = m.limits; a.snapConfig = m.snapConfig; a.trackConfig = m.trackConfig;
    a.savedFilters = m.savedFilters;
}

void SketchModel::auxToMemento(const AuxState& a, Memento& m) {
    m.nextId = a.nextId; m.scale = a.scale;
    m.layers = a.layers; m.blocks = a.blocks; m.textStyles = a.textStyles;
    m.params = a.params; m.dimStyles = a.dimStyles; m.currentDimStyle = a.currentDimStyle;
    m.layouts = a.layouts; m.currentLayout = a.currentLayout;
    m.units = a.units; m.limits = a.limits; m.snapConfig = a.snapConfig; m.trackConfig = a.trackConfig;
    m.savedFilters = a.savedFilters;
}

SketchModel::Memento SketchModel::reconstructStep(int idx) const {
    if (idx < 0) idx = 0;
    if (idx >= (int)undoStack_.size()) idx = (int)undoStack_.size() - 1;
    // 最近检查点 ≤ idx（entry 0 恒为检查点，故必然命中）
    int cp = -1;
    for (int i = idx; i >= 0; --i) {
        if (undoStack_[i].isCheckpoint) { cp = i; break; }
    }
    Memento m;
    if (cp >= 0) m = undoStack_[cp].snap; else m = Memento{};
    // 出向差量链：entry i 的 delta 描述 E_i→E_{i+1}，故从 cp 连放到 idx-1 即得 E_idx。
    // （入向差量旧语义是 cp+1..idx；改为出向后整体前移一位，且链中途的检查点条目
    //  其出向差量同样有效，无需特判。）
    for (int i = cp; i < idx; ++i) {
        const UndoStep& s = undoStack_[i];
        applyElementDeltas(m.elems, s.delta.elems, /*forward=*/true);
        auxToMemento(s.delta.auxAfter, m);
    }
    m.label = undoStack_[idx].isCheckpoint ? undoStack_[idx].snap.label : undoStack_[idx].delta.label;
    return m;
}

// 封印栈顶条目的出向差量。diff(pendingBase_, 当前) 在数学上恰好等于「上一步操作」
// 的变更集——撤销/重做热路径从此只需把这一个变更集就地正/逆向应用（O(变更图元数)），
// 不再做整图 reconstruct + 全量 diff（旧实现 5 万图元单步撤销 39ms 的根源）。
int SketchModel::sealPendingStep() {
    if (!unsealedStep_) return -1;
    DeltaRecord d;
    d.elems = diffElements(pendingBase_.elems, elems_);
    mementoAuxTo(pendingBase_, d.auxBefore);
    d.auxAfter = captureAux();
    const bool changed = !d.elems.empty() || !sameAuxState(d.auxBefore, d.auxAfter);
    unsealedStep_ = false;
    pendingBase_ = Memento{};   // 及时释放基态深拷贝（大图 42MB 级）
    if (changed) {
        UndoStep& top = undoStack_.back();
        top.delta.elems = std::move(d.elems);
        top.delta.auxBefore = std::move(d.auxBefore);
        top.delta.auxAfter = std::move(d.auxAfter);
        // label 已在条目创建时写入 delta.label / snap.label（即该差量所描述操作的名字）
    }
    return changed ? 1 : 0;
}

// 就地写回 aux + 广播。信号集合与 restoreFromMemento 逐项一致（先算差异标志再写值，
// 条件信号只在真变了时发），保证差量 undo/redo 与全量模式对外可观察行为一致。
// elems_ 不在此处理：撤销/重做的元素部分由 applyElementDeltas 就地正/逆向应用。
void SketchModel::restoreAuxInPlace(const AuxState& a) {
    const bool unitsDiff  = !(a.units == units_);
    const bool limitsDiff = !(a.limits == limits_);
    const bool snapDiff   = !(a.snapConfig == snapConfig_);
    const bool trackDiff  = !(a.trackConfig == trackConfig_);
    const bool layoutDiff = (a.currentLayout != currentLayout_) || !sameLayouts(a.layouts, layouts_);
    const bool scaleDiff  = std::fabs(a.scale - scaleRatio_) > 1e-12;

    nextId_ = a.nextId; scaleRatio_ = a.scale;
    layers_ = a.layers; blocks_ = a.blocks;
    textStyles_ = a.textStyles; params_ = a.params;
    dimStyles_ = a.dimStyles; currentDimStyle_ = a.currentDimStyle;
    units_ = a.units; limits_ = a.limits; snapConfig_ = a.snapConfig; trackConfig_ = a.trackConfig;
    layouts_ = a.layouts; currentLayout_ = a.currentLayout;
    savedFilters_ = a.savedFilters;
    selectedIds_.clear(); selectedId_ = -1;
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged(); emit blocksChanged(); emit textStylesChanged(); emit paramsChanged();
    if (layoutDiff) emit layoutsChanged();
    if (unitsDiff)  emit unitsChanged();
    if (limitsDiff) emit limitsChanged();
    if (snapDiff)   emit snapConfigChanged();
    if (trackDiff)  emit trackConfigChanged();
    if (scaleDiff)  emit scaleChanged(scaleRatio_);
}

DeltaRecord SketchModel::makeDelta(const Memento& before, const Memento& after) {
    DeltaRecord d;
    d.elems = diffElements(before.elems, after.elems);
    mementoAuxTo(before, d.auxBefore);
    mementoAuxTo(after,  d.auxAfter);
    return d;
}

SketchModel::Memento SketchModel::applyDeltaForward(const Memento& base, const DeltaRecord& d) {
    Memento m = base;
    applyElementDeltas(m.elems, d.elems, /*forward=*/true);
    auxToMemento(d.auxAfter, m);
    m.label = d.label;
    return m;
}

void SketchModel::restoreFromMemento(Memento m) {
    // 与既有全量撤销/重做的字段赋值 + 信号广播逐字节一致：先算差异标志，再整体替换。
    const bool unitsDiff  = !(m.units == units_);
    const bool limitsDiff = !(m.limits == limits_);
    const bool snapDiff   = !(m.snapConfig == snapConfig_);
    const bool trackDiff  = !(m.trackConfig == trackConfig_);
    const bool layoutDiff = (m.currentLayout != currentLayout_) || !sameLayouts(m.layouts, layouts_);
    const bool scaleDiff  = std::fabs(m.scale - scaleRatio_) > 1e-12;

    elems_ = std::move(m.elems); nextId_ = m.nextId;
    layers_ = std::move(m.layers); blocks_ = std::move(m.blocks);
    textStyles_ = std::move(m.textStyles); params_ = std::move(m.params);
    dimStyles_ = std::move(m.dimStyles); currentDimStyle_ = m.currentDimStyle;
    units_ = m.units; limits_ = m.limits; snapConfig_ = m.snapConfig; trackConfig_ = m.trackConfig;
    layouts_ = std::move(m.layouts); currentLayout_ = m.currentLayout;
    savedFilters_ = std::move(m.savedFilters);
    scaleRatio_ = m.scale;
    selectedIds_.clear(); selectedId_ = -1;
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged(); emit blocksChanged(); emit textStylesChanged(); emit paramsChanged();
    if (layoutDiff) emit layoutsChanged();
    if (unitsDiff)  emit unitsChanged();
    if (limitsDiff) emit limitsChanged();
    if (snapDiff)   emit snapConfigChanged();
    if (trackDiff)  emit trackConfigChanged();
    if (scaleDiff)  emit scaleChanged(scaleRatio_);
}

// 撤销栈按深度上限收敛：双栈同限，且保留队首检查点（其余差量均以它为回放锚点），
// 故 erasing 时把「队首检查点 + 其后的连续差量」整块前移，把下一个检查点提升为新的队首锚点，
// 保证任意可达步都能从某一检查点回放重建（不会因裁剪丢失锚点）。
// 静态成员版 aux→Memento（原匿名命名空间自由函数，因访问私有 Memento 改为静态成员）
void SketchModel::auxToMementoStatic(const AuxState& a, Memento& m) {
    m.nextId = a.nextId; m.scale = a.scale;
    m.layers = a.layers; m.blocks = a.blocks; m.textStyles = a.textStyles;
    m.params = a.params; m.dimStyles = a.dimStyles; m.currentDimStyle = a.currentDimStyle;
    m.layouts = a.layouts; m.currentLayout = a.currentLayout;
    m.units = a.units; m.limits = a.limits; m.snapConfig = a.snapConfig; m.trackConfig = a.trackConfig;
    m.savedFilters = a.savedFilters;
}

void SketchModel::trimStackToLimit(std::vector<UndoStep>& stk, int limit) {
    while ((int)stk.size() > limit) {
        // front 之后第一个检查点 i（可能不存在：i 触底）
        size_t i = 1;
        while (i < stk.size() && !stk[i].isCheckpoint) ++i;
        // 吸收区间终点：有检查点则吸收到它之前的所有差量（检查点自身取代 front 成为新锚）；
        // 无检查点则只把第一条差量合并进 front。
        // ⚠️ 旧写法 erase(begin+1, begin+i) 在 i==1 时是空区间——size 永不缩小 → 死循环。
        size_t last = (i < stk.size()) ? i : 1;
        // 重建原索引 last 的状态，作为新的队首全量检查点
        Memento m = stk.front().snap;
        for (int k = 1; k <= (int)last; ++k) {
            const UndoStep& s = stk[k];
            if (s.isCheckpoint) m = s.snap;
            else {
                applyElementDeltas(m.elems, s.delta.elems, true);
                auxToMementoStatic(s.delta.auxAfter, m);
            }
        }
        stk.erase(stk.begin() + 1, stk.begin() + last + 1);   // 闭区间 [1, last]
        stk.front().snap = m;
        stk.front().isCheckpoint = true;
    }
}

void SketchModel::trimUndoStackToLimit() { trimStackToLimit(undoStack_, undoDepthLimit_); }
void SketchModel::trimRedoStackToLimit() { trimStackToLimit(redoStack_, undoDepthLimit_); }

qint64 SketchModel::maxDeltaEntryBytes() const {
    qint64 mx = 0;
    auto scan = [&](const std::vector<UndoStep>& stk) {
        for (const auto& s : stk) {
            if (s.isCheckpoint) continue;   // 周期全量检查点单独计，不计入单步差量预算
            qint64 b = 0;
            for (const auto& d : s.delta.elems) b += (qint64)d.beforeJson.size() + (qint64)d.afterJson.size();
            if (b > mx) mx = b;
        }
    };
    scan(undoStack_); scan(redoStack_);
    return mx;
}

int SketchModel::addElement(const Element& e) {
    pushMemento(QStringLiteral("添加") + e.name);
    Element ne = e;
    ne.id = nextId_++;
    checkElementLimits(ne);   // LIMITS checkOn 开启时对新增图元做越界告警（不拦截，仅提示）
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
    // 边界联动：被删图元若是某关联填充的边界来源 → 该填充标记失效（与删除同属一个撤销步）
    refreshHatchBoundaries({ id });
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
// P0-04：一次性 reserve —— 10 万图元走逐个 push_back 会触发 log2(N) 次 vector 倍增，
// 每次 realloc 都同时存在旧块+新块（瞬时峰值 ×1.5），是大图纸打开内存峰值的主因之一。
// 一次到位后循环内零 realloc，分配失败点收敛到 reserve 一处（失败时原向量完好，
// 调用方 catch 后模型仍处于可用状态）。
std::vector<int> SketchModel::addElementsQuiet(const std::vector<Element>& parts) {
    std::vector<int> ids;
    ids.reserve(parts.size());
    elems_.reserve(elems_.size() + parts.size());
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
    std::vector<int> moved;
    for (const auto& r : refs) {
        auto it = find(r.elemId);
        if (it == elems_.end()) continue;
        if (geo::setElementHandle(*it, r.index, to)) { ++n; emit elementUpdated(r.elemId); moved.push_back(r.elemId); }
    }
    if (n) emit modelChanged();
    // 边界联动：直接拖动（非 applyChanges 路径）也要让关联填充随动
    if (n) refreshHatchBoundaries(moved);
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

    // 约束恢复：检测修改前满足的相切/垂直/平行/同心约束，恢复被破坏的约束（需求：关联图元联动）
    auto modifiedIds = ConstraintSolverFacade::instance().solveAfterChange(beforeElems, elems_, id);
    for (int mid : modifiedIds) emit elementUpdated(mid);

    // 显式约束闭环：图元被编辑后重解一次用户声明的约束（尺寸锁定/同心/对称/…），
    // 使"拖一下端点、已声明的关系自动保持"。仅在存在显式约束时才跑，避免常态开销。
    // 注意走 updateElementQuiet（不会再触发本函数），因此不存在递归求解。
    if (!constraints_.empty()) {
        const auto rep = ConstraintSolverFacade::instance().solveAll(*this);
        if (!rep.failures.empty())
            log(QStringLiteral("几何约束：%1 条未满足，可用 constraint list / check 定位")
                    .arg(rep.failures.size()), QStringLiteral("warn"));
    }

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
        if (r.intersects(boundsOf(e).adjusted(-eps, -eps, eps, eps))) ids.push_back(e.id);
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
        if (!r.intersects(boundsOf(*e).adjusted(-eps, -eps, eps, eps))) keep.push_back(id);
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
        applyActiveLayer(el);
        el.dtype = DimensionType::Aligned;
        el.refA = s->id;          // 关联源图元，使标注数值可驱动几何
        el.pts = {A, B, P};
    } else if ((s->type == ElementType::Circle || s->type == ElementType::Arc) && s->radius > 1e-9) {
        // 直径标注：通过圆心的尺寸线两端各一个箭头，数字前带 ⌀
        const Vec3 C = s->pts[0];
        const double R = s->radius;
        el.name = QStringLiteral("直径标注");
        applyDefaultStyle(el);
        applyActiveLayer(el);
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

void SketchModel::setCurrentLayer(const QString& name) {
    const QString n = name.trimmed();
    if (!hasLayer(n)) {
        log(QStringLiteral("切换活动图层失败：图层 %1 不存在").arg(n), "warn");
        return;
    }
    if (currentLayer_ == n) return;          // 相同则短路，避免无意义重绘与信号
    currentLayer_ = n;
    emit currentLayerChanged(n);
}

void SketchModel::applyActiveLayer(Element& e) const {
    // 把"当前活动图层"套用到新图元：归属该层 + 继承其颜色/线型/线宽（一次性快照）。
    // 默认层 "0" 不覆盖——保留 applyDefaultStyle 的既有行为（工具栏默认色/线型，线宽自动）。
    // 仅当活动层为非默认层时才套用图层属性，满足"切换图层后新建图元继承该层属性"。
    const QString& cl = currentLayer_;
    const Layer* L = (cl == QStringLiteral("0")) ? nullptr : findLayer(cl);
    if (!L) { e.layer = QStringLiteral("0"); return; }
    e.layer = cl;
    if (L->color.isValid()) e.color = L->color;
    e.lineStyle = L->lineStyle;
    e.lineWidth = L->lineWidth;
    e.lineWidthAuto = false;   // 继承的线宽作为固定值，不被线型自动派生覆盖
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
void SketchModel::setCommandOptions(const QStringList& opts) {
    if (commandOptions_ != opts) {
        commandOptions_ = opts;
        emit commandOptionsChanged(opts);
    }
}
void SketchModel::setShowLineWeight(bool on) {
    if (showLineWeight_ != on) {
        showLineWeight_ = on;
        emit showLineWeightChanged(on);
    }
}
void SketchModel::setDynMode(bool on) {
    if (dynMode_ != on) {
        dynMode_ = on;
        emit dynModeChanged(on);
    }
}
void SketchModel::setPlane(Plane p) {
    if (plane_ != p) { plane_ = p; emit planeChanged(p); emit viewChanged(); }
}

// ============================================================
// 用户坐标系（UCS）：纯输入/显示变换层
// ============================================================

void SketchModel::setCurrentUcs(const Ucs& u) {
    // 归一化命名：世界系强制为内置 "WORLD"
    Ucs next = u;
    if (next.isWorld()) next.name = "WORLD";
    currentUcs_ = next;
    emit ucsChanged();
    emit viewChanged();   // 网格 / 坐标轴 / 极轴等显示层需重绘
}

void SketchModel::setCoordDisplayUcs(bool on) {
    if (coordDisplayUcs_ != on) {
        coordDisplayUcs_ = on;
        emit ucsChanged();
    }
}

QStringList SketchModel::ucsNames() const {
    QStringList names = ucsTable_.keys();
    // 世界系始终作为隐含项存在，但表里不存；此处仅返回显式命名项。
    return names;
}

bool SketchModel::ucsHas(const QString& name) const {
    return ucsTable_.contains(name);
}

Ucs SketchModel::ucsGet(const QString& name) const {
    auto it = ucsTable_.find(name);
    return (it != ucsTable_.end()) ? *it : Ucs("WORLD", Vec3(0, 0, 0), 0.0);
}

bool SketchModel::ucsSave(const QString& name) {
    if (name.isEmpty() || name.compare("WORLD", Qt::CaseInsensitive) == 0) return false;
    ucsTable_[name] = currentUcs_;   // 以当前 UCS 存为命名（覆盖同名）
    return true;
}

bool SketchModel::ucsDelete(const QString& name) {
    if (name.compare("WORLD", Qt::CaseInsensitive) == 0) return false;   // 世界系不可删
    return ucsTable_.remove(name) > 0;
}

bool SketchModel::ucsSetCurrent(const QString& name) {
    auto it = ucsTable_.find(name);
    if (it == ucsTable_.end()) return false;
    setCurrentUcs(*it);
    return true;
}

bool SketchModel::parsePointToWorld(const QString& s, const Vec3& base, Vec3& outWorld) const {
    // 支持：
    //   x,y [z]            绝对（UCS 局部坐标）→ 世界
    //   @dx,dy             相对（UCS 局部坐标系下相对 base 的偏移）
    //   dist<angle         绝对极坐标（从 UCS 原点起，UCS X 正向为 0°）
    //   @dist<angle        相对极坐标（从 base 起，UCS X 正向为 0°）
    // 解析得到的局部坐标经 toWorld 转世界系，保证唯一换算入口。
    QString t = s.trimmed();
    if (t.isEmpty()) return false;
    bool rel = false;
    if (t.startsWith('@')) { rel = true; t = t.mid(1); }

    // 极坐标形式：含 '<'
    const int lt = t.indexOf('<');
    if (lt >= 0) {
        bool okD = false, okA = false;
        const double dist = t.left(lt).toDouble(&okD);
        const double ang  = t.mid(lt + 1).toDouble(&okA);
        if (!okD || !okA) return false;
        const double a = math::deg2rad(ang);
        const Vec3 localDir(std::cos(a), std::sin(a), 0.0);
        if (rel) {
            const Vec3 outUcs = toUcs(base) + localDir * dist;
            outWorld = toWorld(outUcs);
        } else {
            outWorld = toWorld(localDir * dist);   // 从 UCS 原点起
        }
        return true;
    }

    // 笛卡尔形式：x,y[,z]
    const QStringList parts = t.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;
    bool ok1 = false, ok2 = false, ok3 = true;
    const double x = parts[0].toDouble(&ok1);
    const double y = parts[1].toDouble(&ok2);
    double z = 0.0;
    if (parts.size() >= 3) z = parts[2].toDouble(&ok3);
    if (!ok1 || !ok2 || !ok3) return false;
    if (rel) {
        const Vec3 outUcs = toUcs(base) + Vec3(x, y, z);
        outWorld = toWorld(outUcs);
    } else {
        outWorld = toWorld(Vec3(x, y, z));
    }
    return true;
}
void SketchModel::setShowGrid(bool b) {
    if (showGrid_ != b) { showGrid_ = b; emit gridToggled(b); emit viewChanged(); }
}

// ---- 输入约束单一状态源：正交 / 栅格吸附 ----
// 三入口（F8/F9 快捷键、状态栏按钮、命令 ortho/snap）全部汇聚于此，
// 由信号广播同步，保证任何时刻只有一个真相。
void SketchModel::setOrthoMode(bool on) {
    if (orthoMode_ == on) return;               // 幂等：同值不重复发信号
    orthoMode_ = on;
    emit orthoModeChanged(on);
}
void SketchModel::setGridSnap(bool on) {
    if (gridSnap_ == on) return;
    gridSnap_ = on;
    emit gridSnapChanged();
}
void SketchModel::setGridSnapStep(double mm) {
    if (mm <= 0) return;                        // 步长必须为正，非法值静默忽略
    if (std::fabs(gridSnapStep_ - mm) < 1e-9) return;
    gridSnapStep_ = mm;
    emit gridSnapChanged();
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
    if (currentLayer_ == on) {                     // 活动层被改名 → 同步更新活动层名
        currentLayer_ = nn;
        emit currentLayerChanged(nn);
    }
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
    if (currentLayer_ == n) {                       // 删除的恰是当前活动层 → 回落默认层
        currentLayer_ = QStringLiteral("0");
        emit currentLayerChanged(currentLayer_);
    }
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
    // 单层操作也要可撤销：先压快照（记录旧图层定义），再改
    beginEdit(QStringLiteral("修改图层样式 ") + name);
    if (color)     l->color = *color;
    if (lineWidth) l->lineWidth = *lineWidth > 0 ? *lineWidth : l->lineWidth;
    if (ls)        l->lineStyle = *ls;
    // 【解耦】不再把图层新属性反向推送到已有图元：本层已有图元的颜色/线宽/线型保持不变，
    //   只有此后经 ElementFactory::create → applyActiveLayer 新建的图元才会快照本层最新属性。
    //   若用户希望把图层当前样式套用到本层图元，可显式调用 applyLayerStyleToElements。
    emit layersChanged(); emit modelChanged();   // modelChanged 触发重绘（图层定义已变）
    log(QStringLiteral("已修改图层 %1 的样式（已有图元属性保持不变）").arg(name));
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
        // 图层动态语义新字段（只增不改；旧档缺键 → 读端回落 frozen=false/plot=true/alpha=1.0）
        o["frozen"]    = l.frozen;
        o["plot"]      = l.plot;
        o["alpha"]     = l.alpha;
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
        l.lineWidth = json::getDouble(o, "lineWidth", 1.0);
        l.lineStyle  = (LineStyle)json::getInt(o, "lineStyle", (int)LineStyle::Solid);
        l.visible    = json::getBool(o, "visible", true);
        l.locked     = json::getBool(o, "locked", false);
        // 图层动态语义新字段：缺键回落默认值（向后兼容，不破坏旧档语义）
        l.frozen    = json::getBool(o, "frozen", false);
        l.plot      = json::getBool(o, "plot", true);
        l.alpha     = json::getDouble(o, "alpha", 1.0);
        layers_.push_back(l);
    }
    // 保证默认层 "0" 始终存在（旧文件可能未保存该字段）
    if (!findLayer(QStringLiteral("0"))) layers_.insert(layers_.begin(), Layer(QStringLiteral("0")));
    // 读档后若当前活动层不在新图层表中（旧文件无 layers 字段时常见），回落默认层
    if (!findLayer(currentLayer_)) currentLayer_ = QStringLiteral("0");
    emit layersChanged();
}

// ============================================================================
// 图层动态语义：冻结 / 打印开关 / 透明度 / ByLayer 解析 / 命中·打印可判定
// ============================================================================
bool SketchModel::setLayerFrozen(const QString& name, bool frozen) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    if (l->frozen == frozen) return true;
    beginEdit((frozen ? QStringLiteral("冻结图层 ") : QStringLiteral("解冻图层 ")) + name);
    l->frozen = frozen;
    // 冻结改变命中/重算/打印语义 → 触发重绘（GraphicsView 经 elementPickable 排除冻结层）
    emit layersChanged(); emit modelChanged();
    log(QStringLiteral("%1图层 %2").arg(frozen ? "已冻结" : "已解冻").arg(name));
    return true;
}

bool SketchModel::setLayerPlot(const QString& name, bool plot) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    if (l->plot == plot) return true;
    beginEdit((plot ? QStringLiteral("开启图层打印 ") : QStringLiteral("关闭图层打印 ")) + name);
    l->plot = plot;
    emit layersChanged(); emit modelChanged();   // 打印开关影响出图，触发重绘预览
    log(QStringLiteral("图层 %1 打印已%2").arg(name).arg(plot ? "开启" : "关闭"));
    return true;
}

bool SketchModel::setLayerAlpha(const QString& name, double alpha) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log(QStringLiteral("图层 %1 不存在").arg(name), "warn"); return false; }
    const double a = std::clamp(alpha, 0.0, 1.0);
    if (std::fabs(l->alpha - a) < 1e-6) return true;
    beginEdit(QStringLiteral("设置图层透明度 ") + name);
    l->alpha = a;
    emit layersChanged(); emit modelChanged();   // 透明度影响渲染，触发重绘
    log(QStringLiteral("图层 %1 透明度 = %2").arg(name).arg(a, 0, 'f', 2));
    return true;
}

Element SketchModel::resolveElement(const Element& e) const {
    if (!e.byLayer) return e;                     // 覆盖（ByObject）：快照原样使用
    const Layer* L = findLayer(e.layer);
    if (!L) return e;                             // 图层缺失：保持原值，避免凭空改变外观
    Element r = e;
    if (L->color.isValid()) r.color = L->color;   // 图层颜色优先（无效色不覆盖）
    r.lineStyle      = L->lineStyle;
    r.lineWidth      = L->lineWidth;
    r.lineWidthAuto  = false;                     // 随层线宽作为固定值（渲染/打印期不随线型派生）
    if (L->alpha < 1.0 - 1e-6) {                 // 透明度叠加到颜色 alpha 通道（不污染快照）
        QColor c = r.color;
        c.setAlphaF(L->alpha);
        r.color = c;
    }
    return r;
}

bool SketchModel::elementPickable(const Element& e) const {
    if (!e.visible) return false;
    const Layer* L = findLayer(e.layer);
    if (L && L->frozen) return false;             // 冻结层零命中
    return true;
}

bool SketchModel::elementPrintable(const Element& e) const {
    const Layer* L = findLayer(e.layer);
    if (L && (L->frozen || !L->plot)) return false;   // 冻结/关闭打印 → 打印零输出
    return true;
}

// ============================================================================
// 块（Block）管理
// ----------------------------------------------------------------------------
// 引用式模型：块表只有一份定义；BlockRef 只存"块名 + 插入变换 + 属性值 + 动态参数值"。
// 因此重定义块（改一份定义）就是"同步更新所有引用"—— 展开在渲染/命中/导出时实时进行。
// ============================================================================
namespace {
int countBlockRefs(const std::vector<Element>& elems, const QString& name) {
    int n = 0;
    for (const auto& e : elems)
        if (e.type == ElementType::BlockRef && e.blockName == name) ++n;
    return n;
}
} // namespace

// 图元世界包围盒：块引用取"展开后子图元"并集（geometryBounds 是静态函数，拿不到块表）
QRectF SketchModel::boundsOf(const Element& e) const {
    if (e.type != ElementType::BlockRef) return geo::elementBounds(e);
    QRectF b;
    bool any = false;
    for (const auto& s : expandBlockRef(e)) {
        const QRectF sb = geo::elementBounds(s);
        if (sb.isNull()) continue;
        if (!any) { b = sb; any = true; } else { b = b.united(sb); }
    }
    if (!any && !e.pts.empty()) b = QRectF(e.pts[0].x, e.pts[0].y, 0, 0);
    return b;
}

const BlockDef* SketchModel::findBlockDef(const QString& name) const {
    for (const auto& d : blocks_) if (d.name == name) return &d;
    return nullptr;
}
BlockDef* SketchModel::findBlockDef(const QString& name) {
    for (auto& d : blocks_) if (d.name == name) return &d;
    return nullptr;
}

bool SketchModel::addBlockDef(const BlockDef& def) {
    const QString n = def.name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("块名不能为空"), "warn"); return false; }
    if (hasBlock(n)) { log(QStringLiteral("块 %1 已存在").arg(n), "warn"); return false; }
    pushMemento(QStringLiteral("新建块 ") + n);
    BlockDef d = def; d.name = n;
    blocks_.push_back(std::move(d));
    emit blocksChanged();
    log(QStringLiteral("已创建块定义 %1（块内 %2 个图元 / 属性 %3 / 动态参数 %4）")
            .arg(n).arg(def.entities.size()).arg(def.attdefs.size()).arg(def.dynParams.size()));
    return true;
}

bool SketchModel::addBlockDefQuiet(const BlockDef& def) {
    const QString n = def.name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("块名不能为空"), "warn"); return false; }
    if (hasBlock(n)) { log(QStringLiteral("块 %1 已存在").arg(n), "warn"); return false; }
    BlockDef d = def; d.name = n;
    blocks_.push_back(std::move(d));
    emit blocksChanged();     // 引用重绘；不压栈（撤销由外层 beginEdit 统一负责）
    return true;
}

bool SketchModel::redefineBlock(const BlockDef& def) {
    const QString n = def.name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("块名不能为空"), "warn"); return false; }
    pushMemento(QStringLiteral("重定义块 ") + n);
    BlockDef d = def; d.name = n;
    if (BlockDef* ex = findBlockDef(n)) *ex = std::move(d);
    else                                blocks_.push_back(std::move(d));
    emit blocksChanged();
    emit modelChanged();     // 引用几何随之变化，必须重绘
    log(QStringLiteral("已重定义块 %1，%2 个引用同步更新")
            .arg(n).arg(countBlockRefs(elems_, n)));
    return true;
}

bool SketchModel::removeBlock(const QString& name) {
    auto it = std::find_if(blocks_.begin(), blocks_.end(),
                           [&](const BlockDef& d) { return d.name == name; });
    if (it == blocks_.end()) { log(QStringLiteral("块 %1 不存在").arg(name), "warn"); return false; }
    const int refs = countBlockRefs(elems_, name);
    pushMemento(QStringLiteral("删除块 ") + name);
    blocks_.erase(it);
    emit blocksChanged();
    emit modelChanged();
    log(refs > 0
            ? QStringLiteral("已删除块 %1；%2 个引用因块定义缺失而暂不绘制（可撤销恢复）").arg(name).arg(refs)
            : QStringLiteral("已删除块 %1").arg(name));
    return true;
}

bool SketchModel::renameBlock(const QString& oldName, const QString& newName) {
    BlockDef* d = findBlockDef(oldName);
    if (!d) { log(QStringLiteral("块 %1 不存在").arg(oldName), "warn"); return false; }
    const QString nn = newName.trimmed();
    if (nn.isEmpty()) { log(QStringLiteral("新块名不能为空"), "warn"); return false; }
    if (nn == oldName) return true;
    if (hasBlock(nn)) { log(QStringLiteral("块 %1 已存在").arg(nn), "warn"); return false; }
    pushMemento(QStringLiteral("重命名块 ") + oldName);
    d->name = nn;
    int cnt = 0;
    for (auto& e : elems_)
        if (e.type == ElementType::BlockRef && e.blockName == oldName) { e.blockName = nn; ++cnt; }
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("块 %1 已改名为 %2（%3 个引用同步）").arg(oldName).arg(nn).arg(cnt));
    return true;
}

int SketchModel::createBlockFromSelection(const QString& name, const Vec3& basePt,
                                          bool deleteSource, const QString& description) {
    const QString n = name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("块名不能为空"), "warn"); return -1; }
    if (hasBlock(n)) {
        log(QStringLiteral("块 %1 已存在（如需更新块内几何请用「重定义块」）").arg(n), "warn");
        return -1;
    }
    if (selectedIds_.empty()) { log(QStringLiteral("请先选择要收进块的图元"), "warn"); return -1; }

    std::vector<Element> srcs;
    srcs.reserve(selectedIds_.size());
    for (int id : selectedIds_) if (const Element* e = element(id)) srcs.push_back(*e);
    if (srcs.empty()) { log(QStringLiteral("选中图元为空"), "warn"); return -1; }

    pushMemento(QStringLiteral("创建块 ") + n);
    BlockDef def = block::makeDefFromEntities(n, basePt, srcs, description);
    blocks_.push_back(def);

    int refId = -1;
    if (deleteSource) {
        // 先拷一份 ID：removeElementQuiet 会就地改写 selectedIds_，
        // 直接边遍历边删会失效迭代器（且漏删）。
        const std::vector<int> ids = selectedIds_;
        for (int id : ids) removeElementQuiet(id);
        Element ref = block::makeRef(def, basePt, 0.0, 1.0, 1.0);
        auto newIds = addElementsQuiet({ref});
        if (!newIds.empty()) { refId = newIds.front(); setSelected(refId); }
    }
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("已创建块 %1：块内 %2 个图元%3")
            .arg(n).arg(srcs.size())
            .arg(deleteSource ? QStringLiteral("，原图元已替换为块引用")
                              : QStringLiteral("（原图元保留）")));
    return refId;
}

bool SketchModel::redefineBlockFromSelection(const QString& name) {
    BlockDef* d = findBlockDef(name);
    if (!d) { log(QStringLiteral("块 %1 不存在，无法重定义").arg(name), "warn"); return false; }
    if (selectedIds_.empty()) { log(QStringLiteral("请先选择作为新块内几何的图元"), "warn"); return false; }
    std::vector<Element> srcs;
    srcs.reserve(selectedIds_.size());
    for (int id : selectedIds_) if (const Element* e = element(id)) srcs.push_back(*e);
    if (srcs.empty()) { log(QStringLiteral("选中图元为空"), "warn"); return false; }

    pushMemento(QStringLiteral("重定义块 ") + name);
    d->entities = std::move(srcs);
    for (auto& e : d->entities) e.id = 0;
    // 属性定义 / 动态参数定义保持不变；其中"影响图元索引"若越界在展开时被安全忽略
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("已重定义块 %1：%2 个引用同步更新")
            .arg(name).arg(countBlockRefs(elems_, name)));
    return true;
}

int SketchModel::insertBlock(const QString& name, const Vec3& pos,
                             double rotationDeg, double sx, double sy) {
    const BlockDef* d = findBlockDef(name);
    if (!d) { log(QStringLiteral("块 %1 不存在").arg(name), "warn"); return -1; }
    pushMemento(QStringLiteral("插入块 ") + name);
    Element ref = block::makeRef(*d, pos, rotationDeg, sx, sy);
    auto ids = addElementsQuiet({ref});
    if (ids.empty()) return -1;
    setSelected(ids.front());
    emit modelChanged();
    log(QStringLiteral("已插入块 %1 @(%2,%3) 旋转 %4° 缩放 %5×%6")
            .arg(name).arg(pos.x, 0, 'f', 2).arg(pos.y, 0, 'f', 2)
            .arg(rotationDeg, 0, 'f', 1).arg(sx, 0, 'f', 2).arg(sy, 0, 'f', 2));
    return ids.front();
}

std::vector<Element> SketchModel::expandBlockRef(const Element& ref) const {
    return block::expandRef(ref, blocks_);
}

std::vector<int> SketchModel::explodeBlockRef(int refId) {
    auto it = find(refId);
    if (it == elems_.end() || it->type != ElementType::BlockRef) {
        log(QStringLiteral("分解失败：图元 #%1 不是块引用").arg(refId), "warn");
        return {};
    }
    const QString bn = it->blockName;
    auto subs = expandBlockRef(*it);
    if (subs.empty()) {
        log(QStringLiteral("分解失败：块 %1 未定义或为空").arg(bn), "warn");
        return {};
    }
    pushMemento(QStringLiteral("分解块 ") + bn);
    removeElementQuiet(refId);
    auto ids = addElementsQuiet(subs);
    setSelection(ids);
    emit modelChanged();
    log(QStringLiteral("已分解块 %1 为 %2 个独立图元").arg(bn).arg(ids.size()));
    return ids;
}

bool SketchModel::setBlockAttribute(int refId, const QString& tag, const QString& value) {
    auto it = find(refId);
    if (it == elems_.end() || it->type != ElementType::BlockRef) {
        log(QStringLiteral("属性编辑失败：图元 #%1 不是块引用").arg(refId), "warn");
        return false;
    }
    // 常量属性拒绝修改（与 AutoCAD ATTDEF 常量语义一致）
    if (const BlockDef* d = findBlockDef(it->blockName)) {
        for (const auto& ad : d->attdefs)
            if (ad.tag == tag && ad.constant) {
                log(QStringLiteral("属性 %1 为常量属性，不可修改").arg(tag), "warn");
                return false;
            }
    }
    pushMemento(QStringLiteral("修改块属性 ") + tag);
    it = find(refId);   // pushMemento 不改变 elems_，但重新定位更稳妥
    bool found = false;
    for (auto& a : it->blockAttrs) if (a.tag == tag) { a.value = value; found = true; break; }
    if (!found) it->blockAttrs.push_back(BlockAttrValue{tag, value});
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("块 #%1 属性 %2 = %3").arg(refId).arg(tag).arg(value));
    return true;
}

bool SketchModel::setBlockDynValue(int refId, const QString& name, double value) {
    auto it = find(refId);
    if (it == elems_.end() || it->type != ElementType::BlockRef) {
        log(QStringLiteral("动态参数设置失败：图元 #%1 不是块引用").arg(refId), "warn");
        return false;
    }
    // 值域钳制（若该参数声明了范围）
    if (const BlockDef* d = findBlockDef(it->blockName)) {
        for (const auto& p : d->dynParams) {
            if (p.name != name) continue;
            if (p.maxValue > p.minValue)
                value = std::min(std::max(value, p.minValue), p.maxValue);
            break;
        }
    }
    pushMemento(QStringLiteral("设置动态参数 ") + name);
    it = find(refId);
    bool found = false;
    for (auto& dv : it->dynValues) if (dv.name == name) { dv.value = value; found = true; break; }
    if (!found) it->dynValues.push_back(BlockDynValue{name, value, QString()});
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("块 #%1 动态参数 %2 = %3").arg(refId).arg(name).arg(value, 0, 'f', 3));
    return true;
}

bool SketchModel::setBlockDynState(int refId, const QString& name, const QString& state) {
    auto it = find(refId);
    if (it == elems_.end() || it->type != ElementType::BlockRef) {
        log(QStringLiteral("可见性状态设置失败：图元 #%1 不是块引用").arg(refId), "warn");
        return false;
    }
    // 状态必须已在该可见性参数中声明
    bool declared = false;
    if (const BlockDef* d = findBlockDef(it->blockName)) {
        for (const auto& p : d->dynParams) {
            if (p.name != name || p.type != DynParamType::Visibility) continue;
            for (const auto& s : p.states) if (s.name == state) { declared = true; break; }
            break;
        }
    }
    if (!declared) {
        log(QStringLiteral("可见性参数 %1 未声明状态「%2」").arg(name, state), "warn");
        return false;
    }
    pushMemento(QStringLiteral("切换可见性状态 ") + state);
    it = find(refId);
    bool found = false;
    for (auto& dv : it->dynValues) if (dv.name == name) { dv.state = state; found = true; break; }
    if (!found) it->dynValues.push_back(BlockDynValue{name, 0.0, state});
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("块 #%1 可见性 %2 = %3").arg(refId).arg(name).arg(state));
    return true;
}

std::vector<BlockAttrValue> SketchModel::extractBlockAttributes(int refId) const {
    const Element* r = element(refId);
    if (!r || r->type != ElementType::BlockRef) return {};
    return block::extractAttributes(*r, blocks_);
}

QJsonArray SketchModel::blocksToJson() const {
    QJsonArray arr;
    for (const auto& d : blocks_) arr.append(block::blockDefToJson(d));
    return arr;
}

void SketchModel::setBlocksFromJson(const QJsonArray& arr) {
    blocks_.clear();
    for (const auto& v : arr) {
        BlockDef d = block::blockDefFromJson(v.toObject());
        if (!d.name.trimmed().isEmpty()) blocks_.push_back(std::move(d));
    }
    emit blocksChanged();
}

// ---- BEDIT（块在位编辑）----
// 引用式模型：改一份 BlockDef 的 entities 即"所有引用同步刷新"，无需反向推送。
std::vector<Element> SketchModel::getBlockEntities(const QString& name) const {
    if (const BlockDef* d = findBlockDef(name)) return d->entities;
    return {};
}

bool SketchModel::redefineBlockEntities(const QString& name, const std::vector<Element>& ents) {
    const QString n = name.trimmed();
    BlockDef* d = findBlockDef(n);
    if (!d) { log(QStringLiteral("块 %1 不存在，无法在位编辑").arg(n), "warn"); return false; }
    pushMemento(QStringLiteral("在位编辑块 ") + n);
    d->entities = ents;
    // 块内图元 id 归零：展开时重算，避免与顶层图元 id 冲突（attdefs/dynParams/basePoint
    // 等定义字段保持不变 —— 这正是"保留动态块参数/属性定义"的语义保证）。
    for (auto& e : d->entities) e.id = 0;
    emit blocksChanged();
    emit modelChanged();   // 引用几何随之变化，必须重绘
    log(QStringLiteral("已在位编辑块 %1：%2 个引用同步更新，块内 %3 个图元")
            .arg(n).arg(countBlockRefs(elems_, n)).arg(d->entities.size()));
    return true;
}

// ============================================================================
// 填充边界联动（关联填充 / hatchedit）
// ----------------------------------------------------------------------------
// 关联填充 = Hatch 记录其边界来源图元 id（Element::hatchSrcIds）。边界图元几何变更后，
// 经 refreshHatchBoundaries 用 Boundary 引擎（buildClosedLoopsFromElements）把来源重解析为
// 闭合环并就地改写该 Hatch 的 pts/holes（**不重建 Hatch**，保留材质/图案/颜色等全部字段）。
// 边界消失或不再成环 → 该填充标记失效（hatchInvalid=true）并提示，不再随动。
// ============================================================================
namespace {

// 射线法：点 p 是否在多边形 ring 内（even-odd）。用于把重解析出的多个环
// 分类为「外边界」与「内孔」：外环取面积最大者，其余被它包含者为内孔。
bool pointInRing(const std::vector<Vec3>& ring, const Vec3& p) {
    if (ring.size() < 3) return false;
    bool in = false;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Vec3& a = ring[i];
        const Vec3& b = ring[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x = a.x + (p.y - a.y) / (b.y - a.y) * (b.x - a.x);
            if (p.x < x) in = !in;
        }
    }
    return in;
}

double ringArea(const std::vector<Vec3>& ring) {
    if (ring.size() < 3) return 0.0;
    double s = 0.0;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        s += (ring[j].x * ring[i].y - ring[i].x * ring[j].y);
    return std::fabs(s) * 0.5;
}

// 由「边界来源图元」重解析出 (外边界, 内孔)。复用 Boundary 引擎把来源串成闭合环；
// Boundary 图元自带的内孔环也一并纳入（buildClosedLoopsFromElements 只取其外环）。
// 返回 false = 来源缺失 / 无法成闭合环（→ 填充失效）。
bool rebuildHatchLoops(const std::vector<const Element*>& srcs,
                       std::vector<Vec3>& outer, std::vector<std::vector<Vec3>>& holes) {
    if (srcs.empty()) return false;
    std::vector<Element> pool;
    pool.reserve(srcs.size());
    for (const auto* e : srcs) pool.push_back(*e);

    std::vector<std::vector<Vec3>> loops =
        buildClosedLoopsFromElements(pool, kDefaultClosureTol);
    // Boundary 的内孔（引擎只登记其外环，此处把 hole 环补进来）
    for (const auto& e : pool) {
        if (e.type != ElementType::Boundary) continue;
        for (const auto& hole : e.holes)
            if (hole.size() >= 3) loops.push_back(hole);
    }
    if (loops.empty()) return false;

    // 外环 = 面积最大者；内孔 = 被外环包含的其余环
    size_t bi = 0;
    for (size_t i = 1; i < loops.size(); ++i)
        if (ringArea(loops[i]) > ringArea(loops[bi])) bi = i;
    outer = loops[bi];
    if (outer.size() < 3 || ringArea(outer) <= 0.0) return false;
    holes.clear();
    for (size_t i = 0; i < loops.size(); ++i) {
        if (i == bi) continue;
        if (loops[i].size() < 3) continue;
        if (pointInRing(outer, loops[i].front())) holes.push_back(loops[i]);
    }
    return true;
}

} // namespace

bool SketchModel::associateHatchBoundary(int hatchId, const std::vector<int>& srcIds) {
    auto it = find(hatchId);
    if (it == elems_.end() || it->type != ElementType::Hatch) {
        log(QStringLiteral("关联填充失败：图元 #%1 不是剖面区域").arg(hatchId), "warn");
        return false;
    }
    if (srcIds.empty()) {
        log(QStringLiteral("关联填充失败：未指定边界图元"), "warn");
        return false;
    }
    // 先按当前边界试解析一次：不成环则拒绝（避免登记一个永远失效的关联）
    std::vector<const Element*> srcs;
    for (int sid : srcIds) {
        const Element* se = element(sid);
        if (!se) { log(QStringLiteral("关联填充失败：边界图元 #%1 不存在").arg(sid), "warn"); return false; }
        srcs.push_back(se);
    }
    std::vector<Vec3> outer; std::vector<std::vector<Vec3>> holes;
    if (!rebuildHatchLoops(srcs, outer, holes)) {
        log(QStringLiteral("关联填充失败：所选边界当前不构成闭合区域"), "warn");
        return false;
    }
    pushMemento(QStringLiteral("关联填充边界 ") + it->name);
    it = find(hatchId);
    it->hatchSrcIds     = srcIds;
    it->hatchAssociative = true;
    it->hatchInvalid    = false;
    it->pts  = outer;      // 立即按当前边界对齐一次
    it->holes = holes;
    emit modelChanged();
    emit elementUpdated(hatchId);
    log(QStringLiteral("剖面区域 #%1 已关联 %2 个边界图元（随边界自动更新）")
            .arg(hatchId).arg((int)srcIds.size()));
    return true;
}

int SketchModel::createAssociativeHatch(const std::vector<int>& srcIds, const SectionStyle& style) {
    if (srcIds.empty()) {
        log(QStringLiteral("关联填充失败：未指定边界图元"), "warn");
        return -1;
    }
    std::vector<const Element*> srcs;
    for (int sid : srcIds) {
        const Element* se = element(sid);
        if (!se) { log(QStringLiteral("关联填充失败：边界图元 #%1 不存在").arg(sid), "warn"); return -1; }
        srcs.push_back(se);
    }
    // 先按当前边界试解析：不成环则拒绝（不产生"一出生就失效"的填充）
    std::vector<Vec3> outer; std::vector<std::vector<Vec3>> holes;
    if (!rebuildHatchLoops(srcs, outer, holes)) {
        log(QStringLiteral("关联填充失败：所选边界当前不构成闭合区域"), "warn");
        return -1;
    }
    // 组装 Hatch（就地字段语义：pts=外环，holes=内孔，rotation/radius/material/exempt 沿用 style）
    SectionRegion region;
    region.boundary = outer;
    region.holes    = holes;
    region.style    = style;
    Element h = makeHatchElement(region, QStringLiteral("剖面区域(关联)"));
    h.hatchSrcIds      = srcIds;
    h.hatchAssociative = true;
    h.hatchInvalid     = false;
    // addElement 自带一次撤销快照（"新建图元"），此处不再额外 pushMemento，避免双步撤销。
    const int id = addElement(h);
    if (id > 0) {
        log(QStringLiteral("已创建关联填充 #%1（来源 %2 个边界图元，随边界自动更新）")
                .arg(id).arg((int)srcIds.size()));
    }
    return id;
}

bool SketchModel::detachHatchBoundary(int hatchId) {
    auto it = find(hatchId);
    if (it == elems_.end() || it->type != ElementType::Hatch) {
        log(QStringLiteral("断开关联失败：图元 #%1 不是剖面区域").arg(hatchId), "warn");
        return false;
    }
    if (!it->hatchAssociative && it->hatchSrcIds.empty()) return true;   // 已断开
    pushMemento(QStringLiteral("断开填充关联 ") + it->name);
    it = find(hatchId);
    it->hatchAssociative = false;
    // 保留 hatchSrcIds：关联开关是"可逆切换"——off 时 refreshHatchBoundaries 跳过本填充
    // （= 静态填充，边界再变也不随动），再 on 即可恢复随动，无需重新登记边界。
    emit modelChanged();
    emit elementUpdated(hatchId);
    log(QStringLiteral("剖面区域 #%1 已断开边界关联（回到静态填充）").arg(hatchId));
    return true;
}

int SketchModel::refreshHatchBoundaries(const std::vector<int>& changedIds) {
    if (changedIds.empty()) return 0;
    int affected = 0, invalidated = 0;
    // 只遍历"关联且有来源"的 Hatch —— 性能约束：边界重算限仅受影响 Hatch。
    for (auto& h : elems_) {
        if (h.type != ElementType::Hatch) continue;
        if (!h.hatchAssociative || h.hatchSrcIds.empty()) continue;
        // 受影响判定：该填充的来源集合与本次变更集有交集
        bool hit = false;
        for (int sid : h.hatchSrcIds) {
            if (std::find(changedIds.begin(), changedIds.end(), sid) != changedIds.end()) { hit = true; break; }
        }
        if (!hit) continue;
        ++affected;

        // 取当前来源（缺失者跳过 → 视为边界消失）
        std::vector<const Element*> srcs;
        for (int sid : h.hatchSrcIds) if (const Element* se = element(sid)) srcs.push_back(se);

        std::vector<Vec3> outer; std::vector<std::vector<Vec3>> holes;
        if (rebuildHatchLoops(srcs, outer, holes)) {
            // 就地改写边界（保留材质/图案/颜色/线宽等全部字段，不重建 Hatch）
            h.pts = outer;
            h.holes = holes;
            if (h.hatchInvalid) {          // 从失效恢复
                h.hatchInvalid = false;
                log(QStringLiteral("剖面区域 #%1 边界已恢复，随动更新").arg(h.id));
            }
            emit elementUpdated(h.id);
        } else {
            // 边界消失/不再成环 → 标记失效（保留最后几何，交由提示与 hatchedit 处理）
            if (!h.hatchInvalid) {
                h.hatchInvalid = true;
                log(QStringLiteral("剖面区域 #%1 的边界已消失或不成闭合区域，填充已标记失效（可用 HATCHEDIT 解除关联或重填）")
                        .arg(h.id), "warn");
            }
            ++invalidated;
            emit elementUpdated(h.id);
        }
    }
    if (affected) emit modelChanged();
    (void)invalidated;
    return affected;
}

// ============================================================================
// FIELD 域失效重算（applyChanges 钩子 + fieldupdate 命令 + 打开文档刷新共用）
// ----------------------------------------------------------------------------
// 求值是渲染期实时的（resolveFields/displayTableCell 每次渲染都算），因此"重算"
// 的实质是三件事：① 显式标记受影响域宿主（供日志/信号扩展）；② 预热一次求值，
// 保证异常路径在此被吞（崩溃安全约束的守卫点）；③ 为未来缓存型域值预留统一入口。
// 命中规则：宿主自身被改（Table 公式随行内容变化）或其域引用了本次变更图元。
// 只读不改几何、不压撤销 —— 与 refreshHatchBoundaries 同 Quiet 语义。
// ============================================================================
namespace {
// 从一段文本中提取 AREA:n / PERIM:n 域引用的图元 id（两种占位语法均认）。
// 供"宿主是否引用了本次变更图元"的精确匹配判定。
void collectFieldRefIds(const QString& text, std::vector<int>& out) {
    auto scanOne = [&](const QString& body) {
        const int colon = body.indexOf(QLatin1Char(':'));
        if (colon <= 0) return;
        const QString head = body.left(colon).toUpper();
        if (head != QLatin1String("AREA") && head != QLatin1String("PERIM")) return;
        bool ok = false;
        const int id = body.mid(colon + 1).trimmed().toInt(&ok);
        if (ok) out.push_back(id);
    };
    int i = 0;
    while ((i = text.indexOf(QLatin1Char('%'), i)) >= 0) {
        const int b = text.indexOf(QLatin1Char('<'), i);
        const int e = text.indexOf(QLatin1String(">%"), i);
        if (b < 0 || e < 0) break;
        scanOne(text.mid(b + 1, e - b - 1));
        i = e + 2;
    }
    i = 0;
    while ((i = text.indexOf(QStringLiteral("{field:"), i)) >= 0) {
        const int e = text.indexOf(QLatin1Char('}'), i);
        if (e < 0) break;
        scanOne(text.mid(i + 7, e - i - 7));
        i = e + 1;
    }
}
// refs 与 changedIds 是否有交集
bool refsHitChanged(const std::vector<int>& refs, const std::vector<int>& changedIds) {
    for (int r : refs)
        if (std::find(changedIds.begin(), changedIds.end(), r) != changedIds.end()) return true;
    return false;
}
} // namespace

int SketchModel::refreshFields(const std::vector<int>& changedIds) {
    int hosts = 0;
    try {
        for (const auto& e : elems_) {
            if (e.type == ElementType::MText) {
                if (!e.text.contains(QStringLiteral("%<")) &&
                    !e.text.contains(QStringLiteral("{field:")))
                    continue;
                if (!changedIds.empty()) {
                    std::vector<int> refs;
                    collectFieldRefIds(e.text, refs);
                    const bool selfChanged =
                        std::find(changedIds.begin(), changedIds.end(), e.id) != changedIds.end();
                    if (!selfChanged && !refsHitChanged(refs, changedIds)) continue;
                }
                resolveFields(e.text, this);   // 预热：异常在 resolveFields 内部收敛为 ###
                ++hosts;
            } else if (e.type == ElementType::Table) {
                bool hasField = false;
                for (const QString& c : e.tableCells) {
                    if (c.startsWith(QLatin1Char('=')) ||
                        c.contains(QStringLiteral("%<")) ||
                        c.contains(QStringLiteral("{field:"))) { hasField = true; break; }
                }
                if (!hasField) continue;
                if (!changedIds.empty()) {
                    std::vector<int> refs;
                    for (const QString& c : e.tableCells) collectFieldRefIds(c, refs);
                    const bool selfChanged =
                        std::find(changedIds.begin(), changedIds.end(), e.id) != changedIds.end();
                    if (!selfChanged && !refsHitChanged(refs, changedIds)) continue;
                }
                for (int r = 1; r <= e.tableRows; ++r)          // 预热全表（异常收敛 ###）
                    for (int c = 0; c < e.tableCols; ++c)
                        displayTableCell(e, r, c, this);
                ++hosts;
            }
        }
    } catch (...) {
        // 崩溃安全兜底：预热阶段任何异常都不向调用方（applyChanges/命令/打开文档）扩散
    }
    return hosts;
}

bool SketchModel::setHatchPatternOverride(int hatchId, const QString& patName, double scale,
                                          int triStateAssoc) {
    auto it = find(hatchId);
    if (it == elems_.end() || it->type != ElementType::Hatch) {
        log(QStringLiteral("HATCHEDIT 失败：图元 #%1 不是剖面区域").arg(hatchId), "warn");
        return false;
    }
    const double sc = (scale > 1e-6) ? scale : ((it->hatchScale > 1e-6) ? it->hatchScale : 1.0);
    // 就地改写（保留边界/内孔/材质/颜色等；单步撤销由调用方 beginEdit 覆盖）。
    // patName 直接赋值：空串 = 清除覆盖（渲染层按 sectionMaterial 取默认图案）。
    updateElementQuiet(hatchId, [&](Element& dst) {
        dst.hatchPattern = patName;
        dst.hatchScale   = sc;
        if (triStateAssoc == 0)      { dst.hatchAssociative = false; }  // 断开（保留来源，可再开）
        else if (triStateAssoc == 1) { dst.hatchAssociative = true; }
    });
    return true;
}

// ============================================================
// 文字样式（STYLE）
// ============================================================
const TextStyle& SketchModel::textStyleAt(int i) const {
    static const TextStyle kDefault{QStringLiteral("Standard"), QString(), 0.0, 1.0, 0.0, false, false};
    if (i < 0 || i >= (int)textStyles_.size()) return kDefault;
    return textStyles_[i];
}

const TextStyle* SketchModel::findTextStyle(const QString& name) const {
    if (name.isEmpty()) return nullptr;
    for (const auto& s : textStyles_)
        if (s.name == name) return &s;
    return nullptr;
}

bool SketchModel::addTextStyle(const TextStyle& s) {
    if (s.name.trimmed().isEmpty()) { log(QStringLiteral("文字样式：名称为空，已忽略"), "warn"); return false; }
    if (hasTextStyle(s.name)) { log(QStringLiteral("文字样式已存在：%1").arg(s.name), "warn"); return false; }
    pushMemento(QStringLiteral("新建文字样式 ") + s.name);
    textStyles_.push_back(s);
    emit textStylesChanged(); emit modelChanged();
    log(QStringLiteral("已新建文字样式：%1").arg(s.name));
    return true;
}

bool SketchModel::renameTextStyle(const QString& oldName, const QString& newName) {
    if (oldName == newName) return true;
    if (newName.trimmed().isEmpty()) { log(QStringLiteral("文字样式：新名称为空，已忽略"), "warn"); return false; }
    if (hasTextStyle(newName)) { log(QStringLiteral("文字样式已存在：%1").arg(newName), "warn"); return false; }
    auto it = std::find_if(textStyles_.begin(), textStyles_.end(),
                           [&](const TextStyle& s) { return s.name == oldName; });
    if (it == textStyles_.end()) { log(QStringLiteral("文字样式不存在：%1").arg(oldName), "warn"); return false; }
    pushMemento(QStringLiteral("重命名文字样式 ") + oldName);
    // 同步更新所有引用该样式的图元（Text/MText 的 styleName 字段）
    for (auto& e : elems_) if (e.styleName == oldName) e.styleName = newName;
    it->name = newName;
    emit textStylesChanged(); emit modelChanged();
    log(QStringLiteral("已重命名文字样式：%1 → %2").arg(oldName).arg(newName));
    return true;
}

bool SketchModel::updateTextStyle(const TextStyle& s) {
    if (s.name.trimmed().isEmpty()) { log(QStringLiteral("文字样式：名称为空，已忽略"), "warn"); return false; }
    auto it = std::find_if(textStyles_.begin(), textStyles_.end(),
                          [&](const TextStyle& x) { return x.name == s.name; });
    // 改名：目标名与现有某条不同样式冲突则拒绝
    if (it == textStyles_.end()) { log(QStringLiteral("文字样式不存在：%1").arg(s.name), "warn"); return false; }
    pushMemento(QStringLiteral("修改文字样式 ") + s.name);
    *it = s;
    emit textStylesChanged(); emit modelChanged();
    log(QStringLiteral("已更新文字样式：%1").arg(s.name));
    return true;
}

bool SketchModel::removeTextStyle(const QString& name) {
    auto it = std::find_if(textStyles_.begin(), textStyles_.end(),
                           [&](const TextStyle& s) { return s.name == name; });
    if (it == textStyles_.end()) { log(QStringLiteral("文字样式不存在：%1").arg(name), "warn"); return false; }
    if (textStyles_.size() <= 1) { log(QStringLiteral("至少需保留一个文字样式，已忽略删除"), "warn"); return false; }
    pushMemento(QStringLiteral("删除文字样式 ") + name);
    // 解除所有图元对该样式的引用（回退默认样式）
    for (auto& e : elems_) if (e.styleName == name) e.styleName.clear();
    textStyles_.erase(it);
    emit textStylesChanged(); emit modelChanged();
    log(QStringLiteral("已删除文字样式：%1（引用该样式的文字回退默认）").arg(name));
    return true;
}

int SketchModel::applyStyleToElements(const QString& styleName, const std::vector<int>& ids) {
    if (!styleName.isEmpty() && !hasTextStyle(styleName)) {
        log(QStringLiteral("文字样式不存在：%1").arg(styleName), "warn"); return 0;
    }
    beginEdit(styleName.isEmpty() ? QStringLiteral("清除文字样式关联")
                                  : QStringLiteral("应用文字样式 ") + styleName);
    int n = 0;
    for (int id : ids) {
        Element* e = const_cast<Element*>(element(id));
        if (!e) continue;
        if (e->type != ElementType::Text && e->type != ElementType::MText) continue;
        e->styleName = styleName;
        ++n;
    }
    emit textStylesChanged(); emit modelChanged();
    log(QStringLiteral("已将文字样式「%1」应用到 %2 个文字对象").arg(styleName.isEmpty() ? QStringLiteral("(默认)") : styleName).arg(n));
    return n;
}

QJsonArray SketchModel::textStylesToJson() const {
    QJsonArray arr;
    for (const auto& s : textStyles_) {
        QJsonObject o;
        o["name"]           = s.name;
        o["fontFamily"]     = s.fontFamily;
        o["textHeight"]     = s.textHeight;
        o["widthFactor"]    = s.widthFactor;
        o["obliquingAngle"] = s.obliquingAngle;
        o["bold"]           = s.bold;
        o["italic"]         = s.italic;
        arr.append(o);
    }
    return arr;
}

void SketchModel::setTextStylesFromJson(const QJsonArray& arr) {
    textStyles_.clear();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        const QString nm = o["name"].toString().trimmed();
        if (nm.isEmpty()) continue;
        TextStyle s;
        s.name           = nm;
        s.fontFamily     = o["fontFamily"].toString();
        s.textHeight     = json::getDouble(o, "textHeight", 0.0);
        s.widthFactor    = json::getDouble(o, "widthFactor", 1.0);
        s.obliquingAngle = json::getDouble(o, "obliquingAngle", 0.0);
        s.bold           = json::getBool(o, "bold", false);
        s.italic         = json::getBool(o, "italic", false);
        textStyles_.push_back(s);
    }
    // 兜底：任何情况都至少保留一条 "Standard"，保证渲染/解析非 null
    if (textStyles_.empty())
        textStyles_.push_back(TextStyle{QStringLiteral("Standard"), QString(), 0.0, 1.0, 0.0, false, false});
    emit textStylesChanged();
}

// ============================================================================
// 编组（GROUP）：groupName 相同的图元集合（引用式，无独立编组表）
// ----------------------------------------------------------------------------
// 全部变更走 beginEdit + updateElementQuiet：Memento 快照的是整个图元向量，
// groupName 改动随之入栈 —— 一次 Ctrl+Z 整组撤销，与其它编辑完全一致。
// ============================================================================

QStringList SketchModel::groupNames() const {
    QSet<QString> seen;
    for (const auto& e : elems_)
        if (!e.groupName.isEmpty()) seen.insert(e.groupName);
    QStringList out;
    for (const QString& s : seen) out << s;
    out.sort(Qt::CaseInsensitive);
    return out;
}

int SketchModel::elementCountInGroup(const QString& name) const {
    if (name.isEmpty()) return 0;
    int n = 0;
    for (const auto& e : elems_) if (e.groupName == name) ++n;
    return n;
}

std::vector<int> SketchModel::elementsInGroup(const QString& name) const {
    std::vector<int> ids;
    if (name.isEmpty()) return ids;
    for (const auto& e : elems_) if (e.groupName == name) ids.push_back(e.id);
    return ids;
}

// 编组跨 Xref 边界禁止：Xref 块引用的成员资格属于外部图纸，本图编组不得圈占
// （普通块引用是本图对象，可编组）。返回 true 表示该图元是 Xref 块引用。
namespace {
bool isXrefBlockRef(const SketchModel* m, const Element& e) {
    if (e.type != ElementType::BlockRef) return false;
    const BlockDef* d = m->findBlockDef(e.blockName);
    return d && d->isXref;
}
} // namespace

bool SketchModel::createGroup(const QString& name, const std::vector<int>& ids) {
    const QString nm = name.trimmed();
    if (nm.isEmpty() || ids.empty()) {
        log(QStringLiteral("创建编组失败：编组名或对象为空"), "warn");
        return false;
    }
    // Xref 边界守卫：剔除 Xref 块引用（不中断其余成员的编组）
    std::vector<int> ok;
    ok.reserve(ids.size());
    int xrefN = 0;
    for (int id : ids) {
        const Element* e = element(id);
        if (!e) continue;
        if (isXrefBlockRef(this, *e)) { ++xrefN; continue; }
        ok.push_back(id);
    }
    if (ok.empty()) {
        log(QStringLiteral("创建编组失败：所选对象均为外部参照（Xref），禁止跨参照边界编组"), "warn");
        return false;
    }
    if (xrefN > 0)
        log(QStringLiteral("已跳过 %1 个外部参照（Xref 不参与本图编组）").arg(xrefN), "warn");
    // 允许重名 = 向既有编组添加成员（与「添加到编组」语义统一），无需拒绝
    beginEdit(QStringLiteral("创建编组 %1").arg(nm));
    QSet<int> want;
    for (int id : ok) want.insert(id);
    for (auto& e : elems_) {
        if (!want.contains(e.id)) continue;
        e.groupName = nm;
        want.remove(e.id);
    }
    log(QStringLiteral("已创建编组「%1」（%2 个对象）").arg(nm).arg(ok.size()));
    return true;
}

int SketchModel::addToGroup(const QString& name, const std::vector<int>& ids) {
    const QString nm = name.trimmed();
    if (nm.isEmpty() || ids.empty()) return 0;
    // Xref 边界守卫：与 createGroup 同规则，静默跳过 Xref 块引用
    std::vector<int> ok;
    ok.reserve(ids.size());
    for (int id : ids) {
        const Element* e = element(id);
        if (e && !isXrefBlockRef(this, *e)) ok.push_back(id);
    }
    if (ok.empty()) {
        log(QStringLiteral("添加到编组失败：所选对象均为外部参照（Xref），禁止跨参照边界编组"), "warn");
        return 0;
    }
    beginEdit(QStringLiteral("添加到编组 %1").arg(nm));
    QSet<int> want;
    for (int id : ok) want.insert(id);
    int n = 0;
    for (auto& e : elems_) {
        if (!want.contains(e.id) || e.groupName == nm) continue;
        e.groupName = nm;
        ++n;
    }
    if (n > 0) log(QStringLiteral("已将 %1 个对象加入编组「%2」").arg(n).arg(nm));
    return n;
}

int SketchModel::removeFromGroup(const std::vector<int>& ids) {
    if (ids.empty()) return 0;
    beginEdit(QStringLiteral("从编组移除"));
    QSet<int> want;
    for (int id : ids) want.insert(id);
    int n = 0;
    for (auto& e : elems_) {
        if (!want.contains(e.id) || e.groupName.isEmpty()) continue;
        e.groupName.clear();
        ++n;
    }
    if (n > 0) log(QStringLiteral("已将 %1 个对象移出编组").arg(n));
    return n;
}

int SketchModel::ungroup(const QString& name) {
    if (name.isEmpty()) return 0;
    beginEdit(QStringLiteral("解散编组 %1").arg(name));
    int n = 0;
    for (auto& e : elems_) {
        if (e.groupName != name) continue;
        e.groupName.clear();
        ++n;
    }
    log(n > 0 ? QStringLiteral("已解散编组「%1」（%2 个对象保留）").arg(name).arg(n)
              : QStringLiteral("编组「%1」不存在").arg(name), n > 0 ? "info" : "warn");
    return n;
}

bool SketchModel::renameGroup(const QString& oldName, const QString& newName) {
    const QString nm = newName.trimmed();
    if (oldName.isEmpty() || nm.isEmpty() || nm == oldName) return false;
    beginEdit(QStringLiteral("重命名编组 %1 → %2").arg(oldName, nm));
    int n = 0;
    for (auto& e : elems_) {
        if (e.groupName != oldName) continue;
        e.groupName = nm;
        ++n;
    }
    log(QStringLiteral("已重命名编组：%1 → %2（%3 个对象）").arg(oldName, nm).arg(n));
    return true;
}

int SketchModel::selectGroup(const QString& name) {
    const std::vector<int> ids = elementsInGroup(name);
    setSelection(ids);
    return int(ids.size());
}

std::vector<int> SketchModel::expandPickToGroup(int id, bool additive) const {
    const Element* e = element(id);
    if (!e) return {};
    // Ctrl/Shift 修饰 = 穿透编组（单选语义不变）；非编组成员无需扩展
    if (additive || e->groupName.isEmpty()) return { id };
    // 普通点击命中编组成员 → 整组选中（仅可见成员；隐藏成员不参与选择，与框选一致）
    std::vector<int> out;
    out.reserve(8);
    for (const auto& el : elems_)
        if (el.groupName == e->groupName && el.visible) out.push_back(el.id);
    if (out.empty()) out.push_back(id);   // 兜底：全员隐藏时退化为单选
    return out;
}

// ============================================================================
// 快速选择 / 过滤器：与选择集的统一落地口
// ============================================================================

void SketchModel::applySelectionDelta(const std::vector<int>& hits, QSelectAction action) {
    switch (action) {
        case QSelectAction::Replace:
            setSelection(hits);                       // 内部一次 selectionChanged
            break;
        case QSelectAction::Append:
            addToSelection(hits);                     // 批量追加已去重，一次信号
            break;
        case QSelectAction::Exclude: {
            QSet<int> hit;
            for (int id : hits) hit.insert(id);
            std::vector<int> kept;
            kept.reserve(selectedIds_.size());
            for (int id : selectedIds_)
                if (!hit.contains(id)) kept.push_back(id);
            setSelection(kept);
            break;
        }
    }
}

// ============================================================================
// 命名过滤器（FILTER 保存/复用）：随图持久化 + **入撤销栈**。
// 过滤器存的是"这张图的检索口径"，与图层/块表同级属图纸内容，删除/覆盖都应可回退。
// 入口先压改前状态再改；重名覆盖同样算一步。
// ============================================================================

bool SketchModel::saveFilter(const FilterDef& f) {
    const QString nm = f.name.trimmed();
    if (nm.isEmpty() || f.conds.empty()) return false;
    pushMemento(QStringLiteral("保存过滤器 ") + nm);
    for (auto& sf : savedFilters_) {
        if (sf.name == nm) { sf = f; sf.name = nm; return true; }   // 重名覆盖
    }
    savedFilters_.push_back(f);
    savedFilters_.back().name = nm;
    return true;
}

bool SketchModel::deleteFilter(const QString& name) {
    for (auto it = savedFilters_.begin(); it != savedFilters_.end(); ++it) {
        if (it->name == name) {
            pushMemento(QStringLiteral("删除过滤器 ") + name);
            savedFilters_.erase(it);
            return true;
        }
    }
    return false;
}

QJsonArray SketchModel::savedFiltersToJson() const {
    QJsonArray arr;
    for (const auto& f : savedFilters_) arr.append(f.toJson());
    return arr;
}

void SketchModel::setSavedFiltersFromJson(const QJsonArray& arr) {
    savedFilters_.clear();
    for (const auto& v : arr) {
        FilterDef f = FilterDef::fromJson(v.toObject());
        if (!f.name.trimmed().isEmpty() && f.isValid())
            savedFilters_.push_back(std::move(f));
    }
}

void SketchModel::setActivePickFilter(const FilterDef& f) {
    activePickFilter_ = f;
    activePickFilter_.name.clear();   // 预选器是瞬态配置，不带命名语义
    if (activePickFilter_.conds.empty()) {
        // 空条件 = 清除（幂等），不打扰日志
    } else {
        log(QStringLiteral("FILTER 预选器已设置（%1 条件）：作用于下一次拾取/框选/栏选，随后自动清除")
                .arg(activePickFilter_.conds.size()));
    }
}

void SketchModel::clearActivePickFilter() {
    activePickFilter_ = FilterDef{};
}

std::vector<int> SketchModel::applyPickFilterOnce(const std::vector<int>& candidates) {
    if (activePickFilter_.conds.empty()) return candidates;   // 无预选器：原样返回（不清除）
    std::vector<int> kept;
    kept.reserve(candidates.size());
    for (int id : candidates) {
        const Element* e = element(id);
        if (e && e->visible && matchFilter(*e, activePickFilter_))
            kept.push_back(id);
    }
    activePickFilter_ = FilterDef{};   // 用后即清：「作用于随后一次」
    log(QStringLiteral("FILTER 预选器生效：候选 %1 个 → 命中 %2 个（预选器已自动清除）")
            .arg(candidates.size()).arg(kept.size()),
        kept.empty() ? QStringLiteral("warn") : QStringLiteral("info"));
    return kept;
}

// ============================================================================
// 单位（UNITS）与图纸界限（LIMITS）
// ----------------------------------------------------------------------------
// 随图属性（与几何同源）：**入撤销栈** —— 单位/界限/捕捉/追踪都随图存档，
// 用户按 Ctrl+Z 时期望"整体回到上一步"；若这几项不跟随回退，就会出现
// 「线回来了、单位没回来」的半截状态，正是本次要消除的不一致。
// 每个入口先 pushMemento（改**前**状态）再改值，保证一步一次、redo 对称。
// 幂等守卫保持不变：值没变则不入栈、不发信号。
// 信号仅在设置真变化时发出（unitsChanged/limitsChanged/…），下游（状态栏坐标、
// 参数输入条后缀、单位面板）各自连接刷新，避免无效重绘。
//
// 边界：这里入栈的是**随图属性**；「应用习惯」（窗口几何、最近文件、自动保存间隔等
// AppSettings 键）一律不入栈——它们不属于图纸内容，见 P1-01 与本文档第 2 节。
// ============================================================================

void SketchModel::setUnits(const UnitsMeta& u) {
    if (u == units_) return;                       // 相同不发信号、也不入栈（幂等）
    pushMemento(QStringLiteral("修改单位"));
    units_ = u;
    emit unitsChanged();
}

void SketchModel::setLimits(const LimitsMeta& l) {
    const LimitsMeta v = l.isValid() ? l : LimitsMeta();   // 非法范围回落默认 A3
    if (v == limits_) return;
    pushMemento(QStringLiteral("修改图纸界限"));
    limits_ = v;
    emit limitsChanged();
}

// ---- 图纸幅面规格（打印页面尺寸/方向数据源；随图存档顶层 "sheet"）----
// 与单位/界限同级：新建/读档时设定，**不入撤销栈**（运行期无修改入口，故无需快照语义）。
void SketchModel::setSheetSpec(const SheetSpec& s) {
    const int fi = (int)s.format;
    if (fi < 0 || fi > (int)SheetFormat::A5) return;      // 非法幅面 → 拒绝（保持原值）
    sheetSpec_ = s;
    hasSheetSpec_ = true;
}

std::optional<SheetSpec> SketchModel::inferSheetSpecFromFrame() const {
    const QString frameLayer = QStringLiteral("图框");
    double bestArea = -1.0, bw = 0.0, bh = 0.0;
    for (const auto& e : elems_) {
        if (e.layer != frameLayer || e.type != ElementType::Rectangle) continue;
        if (e.pts.size() < 2) continue;
        const double w = std::fabs(e.pts[1].x - e.pts[0].x);
        const double h = std::fabs(e.pts[1].y - e.pts[0].y);
        const double area = w * h;
        if (area > bestArea) { bestArea = area; bw = w; bh = h; }
    }
    if (bestArea <= 0.0) return std::nullopt;             // 无图框矩形 → 放弃推断
    const auto fmt = sheetFormatFromSize(bw, bh);
    if (!fmt) return std::nullopt;                        // 非标尺寸 → 放弃推断（不猜）
    return SheetSpec::make(*fmt, bw > bh);                // 方向由矩形长短边决定
}

QJsonObject SketchModel::sheetSpecToJson() const {
    QJsonObject o;
    if (!hasSheetSpec_) return o;                         // 无规格 → 空对象（调用方不写键）
    o["format"]    = (int)sheetSpec_.format;
    o["landscape"] = sheetSpec_.landscape;
    return o;
}

std::optional<SheetSpec> SketchModel::sheetSpecFromJson(const QJsonObject& o) {
    if (o.isEmpty() || !o.contains("format")) return std::nullopt;
    const int fi = o.value("format").toInt(-1);
    if (fi < 0 || fi > (int)SheetFormat::A5) return std::nullopt;   // 越界 → 视为缺键
    SheetSpec s = SheetSpec::make((SheetFormat)fi, o.value("landscape").toBool(true));
    return s;
}

void SketchModel::setSnapConfig(const snap::SnapConfig& c) {
    if (c == snapConfig_) return;                 // 相同不发信号、也不入栈（幂等）
    pushMemento(QStringLiteral("修改捕捉设置"));
    snapConfig_ = c;
    emit snapConfigChanged();
}

void SketchModel::setTrackConfig(const snap::TrackConfig& c) {
    if (c == trackConfig_) return;               // 相同不发信号、也不入栈（幂等）
    pushMemento(QStringLiteral("修改追踪设置"));
    trackConfig_ = c;
    emit trackConfigChanged();
}

void SketchModel::addTrackPoint(const Vec3& p) {
    trackPoints_.push_back(p);
    emit trackPointsChanged();
}

void SketchModel::clearTrackPoints() {
    if (trackPoints_.empty()) return;
    trackPoints_.clear();
    emit trackPointsChanged();
}

// ---- 显式几何约束表 ----
void SketchModel::addConstraintRecord(const ConstraintRecord& r) {
    constraints_.push_back(r);
    emit constraintsChanged();
}
void SketchModel::removeConstraint(int index) {
    if (index < 0 || index >= (int)constraints_.size()) return;
    constraints_.erase(constraints_.begin() + index);
    emit constraintsChanged();
}
void SketchModel::clearConstraints() {
    if (constraints_.empty()) return;
    constraints_.clear();
    emit constraintsChanged();
}
void SketchModel::setConstraints(const std::vector<ConstraintRecord>& v) {
    constraints_ = v;
    emit constraintsChanged();
}
QJsonArray SketchModel::constraintsToJson() const {
    QJsonArray a;
    for (const auto& r : constraints_) {
        QJsonObject o;
        o["type"]   = (int)r.type;      // 枚举按整数存档（顺序只增不改）
        o["idA"]    = r.idA;
        o["idB"]    = r.idB;
        o["axisId"] = r.axisId;
        o["value"]  = r.value;
        o["idxA"]   = r.idxA;
        o["idxB"]   = r.idxB;
        // ---- 尺寸驱动 / 参数化建模关联（Request M 新增；旧档缺键回落默认，向后兼容）----
        o["driverId"] = r.driverId;
        o["paramRef"]  = r.paramRef;
        a.append(o);
    }
    return a;
}
void SketchModel::setConstraintsFromJson(const QJsonArray& a) {
    std::vector<ConstraintRecord> v;
    for (const auto& jv : a) {
        const QJsonObject o = jv.toObject();
        if (!o.contains("type")) continue;             // 缺关键字段的条目直接跳过
        ConstraintRecord r;
        r.type   = (ConstraintType)o["type"].toInt((int)ConstraintType::Fixed);
        r.idA    = o["idA"].toInt(-1);
        r.idB    = o["idB"].toInt(-1);
        r.axisId = o["axisId"].toInt(-1);
        r.value  = o["value"].toDouble(0.0);
        r.idxA   = o["idxA"].toInt(-1);   // 旧档缺键回落 -1（默认 0 号控制点）
        r.idxB   = o["idxB"].toInt(-1);
        r.driverId = json::getInt(o, "driverId", -1);  // 旧档缺键回落 -1
        r.paramRef  = json::getString(o, "paramRef", QString());
        v.push_back(r);
    }
    setConstraints(v);
}

// ---- 参数表（尺寸驱动 / 参数化建模，Request M 新增）----
bool SketchModel::hasParam(const QString& name) const {
    return params_.find(name) != params_.end();
}
double SketchModel::param(const QString& name) const {
    auto it = params_.find(name);
    return it != params_.end() ? it->second : 0.0;
}
void SketchModel::setParam(const QString& name, double val) {
    // 与几何编辑同语义：压一次快照，使整组参数修改可一次 Ctrl+Z 撤回
    if (params_.find(name) == params_.end() || params_[name] != val) {
        beginEdit(QStringLiteral("参数 %1").arg(name));
        params_[name] = val;
        emit paramsChanged();
    }
}
QJsonObject SketchModel::paramsToJson() const {
    QJsonObject o;
    for (const auto& kv : params_) o[kv.first] = kv.second;
    return o;
}
void SketchModel::setParamsFromJson(const QJsonObject& o) {
    params_.clear();
    for (auto it = o.begin(); it != o.end(); ++it)
        params_[it.key()] = it.value().toDouble(0.0);
}

// ---- 自定义图纸特性（PROP 域数据源）----
QString SketchModel::drawingProp(const QString& name) const {
    auto it = drawingProps_.find(name);
    return it != drawingProps_.end() ? it->second : QString();
}
void SketchModel::setDrawingProp(const QString& name, const QString& value) {
    if (name.isEmpty()) return;
    if (value.isEmpty()) {          // 空值 = 移除（与"空特性无意义"语义一致）
        if (drawingProps_.erase(name) > 0) emit modelChanged();
        return;
    }
    auto it = drawingProps_.find(name);
    if (it != drawingProps_.end() && it->second == value) return;   // 幂等
    drawingProps_[name] = value;
    emit modelChanged();
}
QJsonObject SketchModel::propsToJson() const {
    QJsonObject o;
    for (const auto& kv : drawingProps_) o[kv.first] = kv.second;
    return o;
}
void SketchModel::setPropsFromJson(const QJsonObject& o) {
    drawingProps_.clear();
    for (auto it = o.begin(); it != o.end(); ++it)
        drawingProps_[it.key()] = it.value().toString();
}

void SketchModel::checkElementLimits(const Element& e) {
    // 告警而非拦截：越界图元仍正常入模（与 AutoCAD LIMITS 语义一致——
    // 越界只是提示，用户可能刻意画到图框外）；检查只看「锚点/特征点」，
    // 用 geo::elementBounds 的整框判定会出现"图幅外大圆心在界内"漏报，
    // 折中：界外但与界限盒相交也算过（取 bounds 中心 + 矩形相交双条件从宽）。
    if (!limits_.checkOn || !limits_.isValid()) return;
    const QRectF b = geo::elementBounds(e);
    const bool anchorOut =
        (b.width() <= 0 && b.height() <= 0)        // 退化（无几何）→ 查首个锚点
        ? !limits_.contains(e.pts.empty() ? 0.0 : e.pts.front().x,
                            e.pts.empty() ? 0.0 : e.pts.front().y)
        : !limits_.contains(b.center().x(), b.center().y()) &&
          (b.right() < limits_.x1 || b.left() > limits_.x2 ||
           b.bottom() < limits_.y1 || b.top() > limits_.y2);
    if (anchorOut)
        qWarning() << QStringLiteral("[LIMITS] 图元 %1(id=%2) 超出图纸界限 (%3,%4)-(%5,%6)")
                          .arg(e.name, QString::number(e.id),
                               QString::number(limits_.x1), QString::number(limits_.y1),
                               QString::number(limits_.x2), QString::number(limits_.y2));
}

// ============================================================================
// 外部参照（Xref）
// ----------------------------------------------------------------------------
// 「附加」的产物是一条 Xref 块记录 + 一个参照实例。几何在附加时按源文件读入定义，
// 之后渲染 / 命中 / 导出全部复用既有的引用式展开路径（BlockRef → expandRef），
// 不必为 Xref 另写一套图形管线。
//
// 两条不可违背的约束：
//   1) 定义内的 entities 不得在附加后被局部增删改 —— DynParam::affected / VisibilityState
//      按**索引**引用它们，删一条就会整体错位；因此嵌套实例的清理一律以「整份 BlockDef」
//      为单位，绝不到定义内部去摘图元。
//   2) 依赖（源图自带、本图尚缺的块定义）只引入被引用的那些；未被引用的定义不污染宿主图。
// ============================================================================

QString SketchModel::drawingDir() const {
    if (drawingPath_.isEmpty()) return QString();
    return QFileInfo(drawingPath_).absolutePath();
}

int SketchModel::attachXref(const QString& filePath, XrefPathType pathType,
                            const Vec3& insertPt, double rotationDeg, double sx, double sy) {
    const QString raw = filePath.trimmed();
    if (raw.isEmpty()) { log(QStringLiteral("附加外部参照失败：文件路径为空"), "warn"); return -1; }
    const QFileInfo fi(raw);
    if (!fi.exists() || !fi.isFile()) {
        log(QStringLiteral("附加外部参照失败：文件不存在 %1").arg(raw), "warn");
        return -1;
    }
    const QString abs = fi.absoluteFilePath();

    // 「已被附加」按符号名判定（与 AutoCAD 一致：块名 = 文件名主体）；
    // 同名普通块则拒绝，避免把普通块当成参照来分离。
    const QString bname = xref::blockNameForPath(abs);
    if (hasBlock(bname)) {
        const BlockDef* ex = findBlockDef(bname);
        log(ex && ex->isXref
                ? QStringLiteral("外部参照 %1 已附加（如需更换请先「分离」）").arg(bname)
                : QStringLiteral("附加外部参照失败：块名 %1 已被普通块占用").arg(bname),
            "warn");
        return -1;
    }

    XrefSource src;
    QString err;
    if (!xref::readSource(abs, src, &err)) {
        log(QStringLiteral("附加外部参照失败：%1").arg(err), "warn");
        return -1;
    }

    const QString stored = xref::storedPath(abs, pathType, drawingDir());
    pushMemento(QStringLiteral("附加外部参照 ") + bname);

    // 依赖引入：源图自带、本图尚缺的定义一律标为「嵌套参照」（不可直接分离、可级联清理）。
    // 依赖之间也可能互相引用（A→B→C），用队列迭代到不再新增为止；已经在表里的跳过，
    // 因此依赖成环也不会死循环。
    int nested = 0;
    std::vector<QString> queue;
    for (const auto& n : xref::missingDependencies(src.elements, blocks_)) queue.push_back(n);
    for (int guard = 0; guard < 1024 && !queue.empty(); ++guard) {
        const QString nm = queue.back();
        queue.pop_back();
        if (hasBlock(nm)) continue;
        const BlockDef* dep = nullptr;
        for (const auto& b : src.blocks) if (b.name == nm) { dep = &b; break; }
        // 源图也没有定义 → 未定义块：展开为空（与本地未定义块同语义，不报错）
        if (!dep) continue;
        BlockDef nb = *dep;
        for (auto& e : nb.entities) e.id = 0;
        // 随参照带入的依赖一律视为「外部内容」：不可在本图编辑，也不可直接分离，
        // 宿主被分离且再无引用者时随之一并消失（与 AutoCAD 嵌套 Xref 的语义一致）。
        nb.isXref     = true;
        nb.xrefNested = true;
        // 依赖本身若在源图里就是一条 Xref 记录，其路径是相对**源图**的，在本图中不可解，
        // 一律清空，避免留下一条指向错误基准的路径（几何已随定义带入，不影响绘制）。
        if (!nb.xrefPath.isEmpty() && !QFileInfo(nb.xrefPath).isAbsolute()) nb.xrefPath.clear();
        std::vector<QString> ndeps = xref::missingDependencies(nb.entities, blocks_);
        blocks_.push_back(std::move(nb));
        ++nested;
        for (const auto& m : ndeps) queue.push_back(m);
    }

    BlockDef def;
    def.name        = bname;
    def.description = QStringLiteral("外部参照 ← %1").arg(stored);
    def.basePoint   = Vec3(0, 0, 0);      // 定义保持源文件世界坐标 → 插入到原点即原位
    def.entities    = std::move(src.elements);
    for (auto& e : def.entities) e.id = 0;
    def.isXref       = true;
    def.xrefPath     = stored;
    def.xrefPathType = (int)pathType;
    def.xrefNested   = false;
    def.xrefStatus   = (int)XrefStatus::Loaded;
    blocks_.push_back(def);

    const Element ref = block::makeRef(blocks_.back(), insertPt, rotationDeg, sx, sy);
    auto ids = addElementsQuiet({ref});
    if (ids.empty()) {
        log(QStringLiteral("附加外部参照失败：无法放置参照实例"), "warn");
        return -1;
    }
    setSelected(ids.front());
    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("已附加外部参照 %1 ← %2（%3，%4 个图元%5），插入点 (%6,%7) 旋转 %8° 缩放 %9×%10")
            .arg(bname).arg(stored).arg(src.formatName).arg(def.entities.size())
            .arg(nested > 0 ? QStringLiteral("，引入 %1 个嵌套参照").arg(nested) : QString())
            .arg(insertPt.x, 0, 'f', 2).arg(insertPt.y, 0, 'f', 2)
            .arg(rotationDeg, 0, 'f', 1).arg(sx, 0, 'f', 2).arg(sy, 0, 'f', 2));
    return ids.front();
}

bool SketchModel::detachXref(const QString& blockName) {
    const QString n = blockName.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("分离外部参照失败：块名不能为空"), "warn"); return false; }
    const BlockDef* d = findBlockDef(n);
    if (!d) { log(QStringLiteral("分离外部参照失败：块 %1 不存在").arg(n), "warn"); return false; }
    if (!d->isXref) {
        log(QStringLiteral("分离失败：%1 不是外部参照（普通块请用「块管理器」删除）").arg(n), "warn");
        return false;
    }
    if (d->xrefNested) {
        log(QStringLiteral("分离失败：%1 是嵌套参照，只能在直接附加它的宿主图中分离").arg(n), "warn");
        return false;
    }

    // 统计：本图中的参照实例 + 其它块定义内部对本参照的嵌套引用（分离后将成为未定义块）
    std::vector<int> victims;
    for (const auto& e : elems_)
        if (e.type == ElementType::BlockRef && e.blockName == n) victims.push_back(e.id);
    int inDefs = 0;
    for (const auto& b : blocks_)
        for (const auto& e : b.entities)
            if (e.type == ElementType::BlockRef && e.blockName == n) ++inDefs;

    pushMemento(QStringLiteral("分离外部参照 ") + n);
    for (int id : victims) removeElementQuiet(id);
    auto it = std::find_if(blocks_.begin(), blocks_.end(),
                           [&](const BlockDef& b) { return b.name == n; });
    if (it != blocks_.end()) blocks_.erase(it);

    // 级联清理：嵌套参照随宿主存在，宿主被分离后若再无引用者，一并清除。
    // 「是否还有引用者」在当前表上现算（顶图 + 其余定义），故依赖成链也能收敛
    // （每轮清掉一层，下一轮里层自然变为无人引用）；最多 64 轮作保险。
    auto referencedAnywhere = [&](const QString& nm) {
        for (const auto& e : elems_)
            if (e.type == ElementType::BlockRef && e.blockName == nm) return true;
        for (const auto& b : blocks_)
            for (const auto& e : b.entities)
                if (e.type == ElementType::BlockRef && e.blockName == nm) return true;
        return false;
    };
    int cascaded = 0;
    for (int guard = 0; guard < 64; ++guard) {
        std::vector<QString> dead;
        for (const auto& b : blocks_) {
            if (!b.isXref || !b.xrefNested) continue;
            if (referencedAnywhere(b.name)) continue;
            dead.push_back(b.name);
        }
        if (dead.empty()) break;
        for (const auto& nm : dead) {
            auto jt = std::find_if(blocks_.begin(), blocks_.end(),
                                   [&](const BlockDef& b) { return b.name == nm; });
            if (jt != blocks_.end()) { blocks_.erase(jt); ++cascaded; }
        }
    }

    emit blocksChanged();
    emit modelChanged();
    log(QStringLiteral("已分离外部参照 %1：删除 %2 个参照实例%3%4")
            .arg(n).arg(victims.size())
            .arg(inDefs > 0
                     ? QStringLiteral("（另有 %1 处嵌套引用因定义缺失不再绘制）").arg(inDefs)
                     : QString())
            .arg(cascaded > 0 ? QStringLiteral("，级联清理 %1 个嵌套参照").arg(cascaded) : QString()));
    return true;
}

QStringList SketchModel::xrefNames(bool includeNested) const {
    QStringList out;
    for (const auto& d : blocks_) {
        if (!d.isXref) continue;
        if (!includeNested && d.xrefNested) continue;
        out << d.name;
    }
    return out;
}

int SketchModel::xrefCount(bool includeNested) const {
    int n = 0;
    for (const auto& d : blocks_) {
        if (!d.isXref) continue;
        if (!includeNested && d.xrefNested) continue;
        ++n;
    }
    return n;
}

bool SketchModel::hasXrefSource(const QString& filePath) const {
    if (filePath.trimmed().isEmpty()) return false;
    const QString target = QFileInfo(filePath).absoluteFilePath();
    for (const auto& d : blocks_) {
        if (!d.isXref || d.xrefPath.isEmpty()) continue;
        const QString rp = QFileInfo(xref::resolvedPath(d.xrefPath,
                                                        (XrefPathType)d.xrefPathType,
                                                        drawingDir())).absoluteFilePath();
        if (rp == target) return true;
    }
    return false;
}

QString SketchModel::xrefResolvedPath(const QString& blockName) const {
    const BlockDef* d = findBlockDef(blockName);
    if (!d || !d->isXref) return QString();
    return xref::resolvedPath(d->xrefPath, (XrefPathType)d->xrefPathType, drawingDir());
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

// 说明：命名过滤器表随快照回退，但它**没有专用变更信号**（既有 saveFilter/deleteFilter
// 也从不发信号，过滤器管理界面在打开时读取），故此处不额外广播，与既有行为一致。
void SketchModel::undo() {
    if (undoStack_.empty()) { log(QStringLiteral("无可撤销操作"), "warn"); return; }
    if (!deltaUndoEnabled_) {
        // 全量模式（既有行为，逐字节保留）：当前态存入 redo，回退到栈顶整张 Memento
        Memento m = std::move(undoStack_.back().snap);
        undoStack_.pop_back();
        UndoStep rs; rs.isCheckpoint = true; rs.snap = currentMemento();
        redoStack_.push_back(std::move(rs));
        trimRedoStackToLimit();
        restoreFromMemento(std::move(m));
        log(QStringLiteral("已撤销: ") + m.label);
        return;
    }
    // 差量模式：栈顶条目的出向差量 delta(E_{n-1}→E_n)（未封印则此刻封印）就是本次
    // 撤销的变更集——就地逆向应用即可回到 E_{n-1}，O(变更图元数)。
    // 空出向差量 = 无操作步（与旧 sameState 去重同源）：丢弃不消耗撤销步，继续向下找
    // 最近的真实变更条目；全部为空则与空栈同样处理。
    sealPendingStep();
    while (!undoStack_.empty()) {
        UndoStep rs = std::move(undoStack_.back());
        undoStack_.pop_back();
        if (rs.delta.elems.empty() && sameAuxState(rs.delta.auxBefore, rs.delta.auxAfter))
            continue;   // 无操作步：直接丢弃
        const QString undoneLabel = rs.isCheckpoint ? rs.snap.label : rs.delta.label;
        applyElementDeltas(elems_, rs.delta.elems, /*forward=*/false);
        restoreAuxInPlace(rs.delta.auxBefore);
        unsealedStep_ = false;
        pendingBase_ = Memento{};
        redoStack_.push_back(std::move(rs));
        trimRedoStackToLimit();
        log(QStringLiteral("已撤销: ") + undoneLabel);
        return;
    }
    unsealedStep_ = false;
    pendingBase_ = Memento{};
    log(QStringLiteral("无可撤销操作"), "warn");
}

void SketchModel::redo() {
    if (redoStack_.empty()) { log(QStringLiteral("无可重做操作"), "warn"); return; }
    if (!deltaUndoEnabled_) {
        Memento m = std::move(redoStack_.back().snap);
        redoStack_.pop_back();
        UndoStep us; us.isCheckpoint = true; us.snap = currentMemento();
        undoStack_.push_back(std::move(us));
        trimUndoStackToLimit();
        restoreFromMemento(std::move(m));
        log(QStringLiteral("已重做: ") + m.label);
        return;
    }
    // 差量模式：redo 栈顶 = 刚被撤销的条目，其出向差量 delta(E_{n-1}→E_n) 描述被撤销
    // 的操作本身——正向就地应用即回到 E_n，条目原样压回撤销栈：检查点属性/快照/出向
    // 差量在新栈语义下全部继续有效，无需重算 %50 检查点、无需 makeDelta 重建。
    // （压回后栈顶条目已封印、unsealedStep_=false：恰好是「push 前夜」的合法形态，
    //   下一次 push 的 sealPendingStep 会因无未封印条目而直接跳到建新条目。）
    UndoStep rs = std::move(redoStack_.back());
    redoStack_.pop_back();
    const QString redoneLabel = rs.delta.label;
    applyElementDeltas(elems_, rs.delta.elems, /*forward=*/true);
    restoreAuxInPlace(rs.delta.auxAfter);
    undoStack_.push_back(std::move(rs));
    trimUndoStackToLimit();
    unsealedStep_ = false;
    pendingBase_ = Memento{};
    log(QStringLiteral("已重做: ") + redoneLabel);
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
    hasSheetSpec_ = false;      // 幅面规格随 clear 一并丢弃（新图由 applyNewDrawing 重设；
                                // 读档路径在 clear 之后按 "sheet" 键/图框推断重设）
    trackPoints_.clear();       // 临时追踪点属瞬态会话状态，新建/清空图纸时一并丢弃
    if (elems_.empty()) return;
    pushMemento(QStringLiteral("清空"));
    elems_.clear(); selectedIds_.clear(); selectedId_ = -1;
    layers_.clear(); layers_.push_back(Layer(QStringLiteral("0")));   // 新建图纸只留默认层
    blocks_.clear();                                                  // 新建图纸不保留块定义
    currentLayer_ = QStringLiteral("0");                              // 新建图纸活动层回落默认层
    emit modelChanged(); emit selectionChanged(-1); emit layersChanged(); emit blocksChanged();
    emit currentLayerChanged(currentLayer_);
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
    dim::Style st = dimStyleFor(Element());
    st.drawingScale = scaleRatio_;
    return st;
}

// ============================================================================
// 命名标注样式表（一图多标准）
// ----------------------------------------------------------------------------
// 图元经 dimStyleName 按名引用表中条目（引用式，对标 blockTable / textStyleName）。
// 关键：dimStyle() 不再返回一张"硬编码默认样式"，而是经 dimStyleFor 解析当前样式——
// 故改样式表 / 切当前样式后，全图标注经同一入口自动读到新值（即"样式切换全图联动"）。
// ============================================================================
const dim::DimStyleEntry* SketchModel::dimStyleAt(int i) const {
    if (i < 0 || i >= (int)dimStyles_.size()) return nullptr;
    return &dimStyles_[(size_t)i];
}

const dim::DimStyleEntry* SketchModel::findDimStyle(const QString& name) const {
    for (const auto& e : dimStyles_) if (e.name == name) return &e;
    return nullptr;
}

dim::Style SketchModel::dimStyleFor(const Element& e) const {
    // 解析顺序：图元 dimStyleName → 当前样式 → 表首条 → 内置默认
    const dim::DimStyleEntry* hit = nullptr;
    if (!e.dimStyleName.isEmpty()) hit = findDimStyle(e.dimStyleName);
    if (!hit && !currentDimStyle_.isEmpty()) hit = findDimStyle(currentDimStyle_);
    if (!hit && !dimStyles_.empty()) hit = &dimStyles_.front();
    dim::Style st = hit ? hit->style : dim::Style();
    st.drawingScale = scaleRatio_;      // 比例不入样式表，每次按当前图纸注入
    return st;
}

bool SketchModel::addDimStyle(const dim::DimStyleEntry& e) {
    const QString n = e.name.trimmed();
    if (n.isEmpty()) { log(QStringLiteral("标注样式名不能为空"), "warn"); return false; }
    if (hasDimStyle(n)) { log(QStringLiteral("标注样式 %1 已存在").arg(n), "warn"); return false; }
    pushMemento(QStringLiteral("新建标注样式 ") + n);
    dim::DimStyleEntry d = e; d.name = n;
    dimStyles_.push_back(std::move(d));
    emit dimStylesChanged();
    emit modelChanged();          // 引用它的标注需重绘
    log(QStringLiteral("已创建标注样式 %1").arg(n));
    return true;
}

bool SketchModel::updateDimStyle(const dim::DimStyleEntry& e) {
    const QString n = e.name.trimmed();
    auto it = std::find_if(dimStyles_.begin(), dimStyles_.end(),
                           [&](const dim::DimStyleEntry& d) { return d.name == n; });
    if (it == dimStyles_.end()) { log(QStringLiteral("标注样式 %1 不存在").arg(n), "warn"); return false; }
    pushMemento(QStringLiteral("修改标注样式 ") + n);
    it->style = e.style;
    emit dimStylesChanged();
    emit modelChanged();
    log(QStringLiteral("已更新标注样式 %1（引用它的标注已联动）").arg(n));
    return true;
}

bool SketchModel::renameDimStyle(const QString& oldName, const QString& newName) {
    auto it = std::find_if(dimStyles_.begin(), dimStyles_.end(),
                           [&](const dim::DimStyleEntry& d) { return d.name == oldName; });
    if (it == dimStyles_.end()) { log(QStringLiteral("标注样式 %1 不存在").arg(oldName), "warn"); return false; }
    const QString nn = newName.trimmed();
    if (nn.isEmpty()) { log(QStringLiteral("新标注样式名不能为空"), "warn"); return false; }
    if (nn == oldName) return true;
    if (hasDimStyle(nn)) { log(QStringLiteral("标注样式 %1 已存在").arg(nn), "warn"); return false; }
    pushMemento(QStringLiteral("重命名标注样式 ") + oldName);
    it->name = nn;
    if (currentDimStyle_ == oldName) currentDimStyle_ = nn;
    int cnt = 0;
    for (auto& el : elems_)
        if (el.dimStyleName == oldName) { el.dimStyleName = nn; ++cnt; }
    emit dimStylesChanged();
    emit modelChanged();
    log(QStringLiteral("标注样式 %1 已改名为 %2（%3 个标注同步）").arg(oldName).arg(nn).arg(cnt));
    return true;
}

bool SketchModel::removeDimStyle(const QString& name) {
    auto it = std::find_if(dimStyles_.begin(), dimStyles_.end(),
                           [&](const dim::DimStyleEntry& d) { return d.name == name; });
    if (it == dimStyles_.end()) { log(QStringLiteral("标注样式 %1 不存在").arg(name), "warn"); return false; }
    if (dimStyles_.size() == 1) { log(QStringLiteral("标注样式表至少保留一条"), "warn"); return false; }
    pushMemento(QStringLiteral("删除标注样式 ") + name);
    dimStyles_.erase(it);
    if (currentDimStyle_ == name) currentDimStyle_ = dimStyles_.front().name;
    int cnt = 0;
    for (auto& el : elems_)
        if (el.dimStyleName == name) { el.dimStyleName.clear(); ++cnt; }   // 回落当前样式
    emit dimStylesChanged();
    emit modelChanged();
    log(QStringLiteral("已删除标注样式 %1；%2 个标注回落当前样式").arg(name).arg(cnt));
    return true;
}

bool SketchModel::setCurrentDimStyle(const QString& name) {
    if (!hasDimStyle(name)) { log(QStringLiteral("标注样式 %1 不存在").arg(name), "warn"); return false; }
    if (currentDimStyle_ == name) return true;
    currentDimStyle_ = name;
    emit dimStylesChanged();
    emit modelChanged();     // 空 dimStyleName 的标注解析到新当前样式 → 全图联动
    log(QStringLiteral("当前标注样式 = %1").arg(name));
    return true;
}

QJsonObject SketchModel::dimStylesToJson() const {
    QJsonObject o;
    QJsonArray arr;
    for (const auto& d : dimStyles_) arr.append(dim::dimStyleEntryToJson(d));
    o["styles"] = arr;
    o["current"] = currentDimStyle_;
    return o;
}

void SketchModel::setDimStylesFromJson(const QJsonObject& o) {
    dimStyles_.clear();
    for (const auto& v : o["styles"].toArray()) {
        dim::DimStyleEntry d = dim::dimStyleEntryFromJson(v.toObject());
        if (!d.name.trimmed().isEmpty()) dimStyles_.push_back(std::move(d));
    }
    // 旧档兼容：本字段整体缺失或为空数组 ⇒ 自动生成默认 "Standard" 一条，
    // 于是「单全局 DimStyle」的旧图等价于"一张名为 Standard 的样式"，语义零变化。
    if (dimStyles_.empty()) {
        dim::DimStyleEntry d;
        d.name = QStringLiteral("Standard");
        d.style = dim::Style();
        dimStyles_.push_back(std::move(d));
    }
    const QString cur = o["current"].toString();
    currentDimStyle_ = hasDimStyle(cur) ? cur : dimStyles_.front().name;
    emit dimStylesChanged();
}

// ============================================================================
// 快速标注（QDIM）：框选平行线 / 圆阵列 → 一次性生成基线或连续标注串
// ============================================================================
// 设计要点（复用 dim 基线/连续引擎，不重写几何）：
//   - 锚点：圆/弧取圆心 pts[0]；其余取首末点中点。去重容差 1e-6。
//   - 按主轴（bbox 较长轴）排序锚点，保证基线/连续沿正确方向展开。
//   - 基线：共用首锚点为基点，依次连到其后各点；连续：链式连相邻点。
//   - 每条标注 dtype = Baseline/Continue，e.radius = 层叠序号(0,1,2…)，
//     引擎 buildBaseline/buildContinue 据 e.radius 把尺寸线逐层外移 defOffset+i*(textH+4)。
//   - 记当前样式名（dimStyleName）+ 当前标准，故切样式/切标准时随全图联动。
std::vector<int> SketchModel::quickDimSelected(const QString& mode) {
    std::vector<int> created;
    const auto& sel = selection();
    if (sel.empty()) { log(QStringLiteral("qdim: 请先框选一组平行线或圆阵列"), QStringLiteral("warn")); return created; }

    // 1) 从选择集提取锚点（圆/弧→圆心；其余→首末点中点），去重
    struct AP { Vec3 p; int srcId; };
    std::vector<AP> pts;
    for (int id : sel) {
        const Element* e = element(id);
        if (!e || !e->visible) continue;
        Vec3 p;
        if (e->type == ElementType::Circle || e->type == ElementType::Arc) {
            if (e->pts.empty()) continue;
            p = e->pts[0];
        } else if (e->pts.size() >= 2) {
            p = (e->pts.front() + e->pts.back()) * 0.5;
        } else {
            continue;
        }
        bool dup = false;
        for (const auto& q : pts) if (q.p.distTo(p) < 1e-6) { dup = true; break; }
        if (!dup) pts.push_back({p, id});
    }
    if (pts.size() < 2) { log(QStringLiteral("qdim: 选择集中可标注的有效几何不足 2 个"), QStringLiteral("warn")); return created; }

    // 2) 按主轴（bbox 较长轴）排序
    double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
    for (const auto& q : pts) {
        minx = std::min(minx, q.p.x); maxx = std::max(maxx, q.p.x);
        miny = std::min(miny, q.p.y); maxy = std::max(maxy, q.p.y);
    }
    const bool byX = (maxx - minx) >= (maxy - miny);
    std::sort(pts.begin(), pts.end(), [byX](const AP& a, const AP& b) {
        return byX ? a.p.x < b.p.x : a.p.y < b.p.y;
    });

    const QString m = mode.toLower();
    const bool cont = (m == QStringLiteral("continue") || m == QStringLiteral("cont") || m == QStringLiteral("c"));
    const DimensionType dt = cont ? DimensionType::Continue : DimensionType::Baseline;
    const QString curStyle = currentDimStyle_;
    const DimensionStandard std = dimensionStandard();

    // 3) 生成标注串（基线=共基点；连续=链式）
    for (size_t i = 1; i < pts.size(); ++i) {
        const Vec3 a = cont ? pts[i - 1].p : pts[0].p;
        const Vec3 b = pts[i].p;
        if (a.distTo(b) < 1e-9) continue;        // 跳过重合锚点
        Element e;
        e.type = ElementType::Dimension;
        e.dtype = dt;
        e.pts = { a, b };
        e.radius = (double)(i - 1);              // 层叠序号：0,1,2…
        e.standard = std;
        e.dimStyleName = curStyle;               // 记当前样式 → 随样式切换全图联动
        if (geo::measureDimension(e) < 1e-9) continue;
        const int id = addElement(e);
        if (id >= 0) created.push_back(id);
    }
    log(QStringLiteral("qdim: 已生成 %1 个%2标注（%3 个锚点）")
            .arg(created.size()).arg(cont ? QStringLiteral("连续") : QStringLiteral("基线"))
            .arg(pts.size()), QStringLiteral("info"));
    return created;
}

// ============================================================================
// 布局表（多布局；不入撤销栈，随图存档）
// ============================================================================

const PlotLayoutDef* SketchModel::layoutAt(int i) const {
    return (i >= 0 && i < (int)layouts_.size()) ? &layouts_[i] : nullptr;
}

const PlotLayoutDef* SketchModel::findLayout(const QString& name) const {
    for (const auto& l : layouts_)
        if (l.name == name) return &l;
    return nullptr;
}

// 布局是随图内容（与图层/块表同级），增删改一律入撤销栈：先压改前状态，再改。
// 唯一例外是 setCurrentLayout（切页签）：它只改"当前看哪一张"，属视图焦点而非图纸内容，
// 若为它单独压栈，Ctrl+Z 会退化成"来回翻页签"。它仍**参与快照**（currentLayout 字段），
// 因此撤销任何一步时当前布局都会随快照一起回到当时，不会出现"布局表回来了、
// 当前页签还停在新布局"的错位。
bool SketchModel::addLayout(const QString& name, const PrintSettings& settings) {
    if (name.trimmed().isEmpty() || findLayout(name)) return false;
    pushMemento(QStringLiteral("新建布局 ") + name);
    PlotLayoutDef def;
    def.name = name;
    def.settings = settings;
    layouts_.push_back(std::move(def));
    emit layoutsChanged();
    return true;
}

bool SketchModel::removeLayout(const QString& name) {
    for (auto it = layouts_.begin(); it != layouts_.end(); ++it) {
        if (it->name == name) {
            pushMemento(QStringLiteral("删除布局 ") + name);
            layouts_.erase(it);
            if (currentLayout_ == name) currentLayout_.clear();   // 回落模型空间
            emit layoutsChanged();
            return true;
        }
    }
    return false;
}

bool SketchModel::renameLayout(const QString& oldName, const QString& newName) {
    if (newName.trimmed().isEmpty()) return false;
    PlotLayoutDef* def = nullptr;
    for (auto& l : layouts_)
        if (l.name == oldName) { def = &l; break; }
    if (!def) return false;
    if (newName != oldName && findLayout(newName)) return false;  // 重名拒绝
    pushMemento(QStringLiteral("重命名布局 ") + oldName);
    def->name = newName;
    if (currentLayout_ == oldName) currentLayout_ = newName;
    emit layoutsChanged();
    return true;
}

bool SketchModel::updateLayout(const QString& name, const PrintSettings& settings) {
    for (auto& l : layouts_) {
        if (l.name == name) {
            pushMemento(QStringLiteral("修改布局 ") + name);
            l.settings = settings;
            emit layoutsChanged();
            return true;
        }
    }
    return false;
}

bool SketchModel::setCurrentLayout(const QString& name) {
    if (name.isEmpty()) {
        if (currentLayout_.isEmpty()) return true;   // 幂等
        currentLayout_.clear();
        emit layoutsChanged();
        return true;
    }
    if (!findLayout(name)) return false;
    if (currentLayout_ == name) return true;         // 幂等
    currentLayout_ = name;                           // 不单独入栈：见上方说明
    emit layoutsChanged();
    return true;
}

const PlotLayoutDef* SketchModel::currentLayoutDef() const {
    if (currentLayout_.isEmpty()) return nullptr;
    return findLayout(currentLayout_);
}

QJsonArray SketchModel::layoutsToJson() const {
    QJsonArray arr;
    for (const auto& l : layouts_) arr.append(l.toJson());
    return arr;
}

void SketchModel::setLayoutsFromJson(const QJsonObject& root) {
    layouts_.clear();
    currentLayout_.clear();
    // 旧档兼容：顶层无 "layouts" 键（或缺省空数组）⇒ 空表 = 模型空间打印，行为零变化。
    for (const auto& v : root.value("layouts").toArray()) {
        PlotLayoutDef def = PlotLayoutDef::fromJson(v.toObject());
        if (!def.name.trimmed().isEmpty()) layouts_.push_back(std::move(def));
    }
    const QString cur = root.value("currentLayout").toString();
    if (!cur.isEmpty() && findLayout(cur)) currentLayout_ = cur;
    emit layoutsChanged();
}

// 集中式整图序列化入口：顶层 JSON 与 MainWindow::writeDrawing 的 Json 分支完全一致，
// 不改动任何既有字段/格式。供「写文件」与「AutoSave 快照」共用，避免两处各写一份。
QJsonObject SketchModel::toDrawingJson() const {
    QJsonObject root;
    root["app"] = QStringLiteral("SketchCAD");
    root["formatVersion"] = 1;
    root["scaleRatio"] = scaleRatio();
    root["ltscale"] = ltscale();
    QJsonArray arr;
    for (const auto& e2 : elements()) arr.append(elementToJson(e2));
    root["elements"] = arr;
    root["layers"] = layersToJson();
    root["blocks"] = blocksToJson();
    root["textStyles"] = textStylesToJson();
    root["dimStyles"] = dimStylesToJson();
    root["savedFilters"] = savedFiltersToJson();
    root["layouts"] = layoutsToJson();
    root["currentLayout"] = currentLayout();
    root["units"]  = units().toJson();
    root["limits"] = limits().toJson();
    root["snapConfig"] = snapConfig().toJson();
    root["trackConfig"] = trackConfig().toJson();
    root["constraints"] = constraintsToJson();
    root["params"] = paramsToJson();
    root["properties"] = propsToJson();   // 自定义图纸特性（PROP 域数据源；旧档缺键 → 空表）
    // 图纸幅面规格（打印/预览/PDF 页面尺寸与方向的数据源；旧档缺键 → 读档按图框推断）
    if (hasSheetSpec_) root["sheet"] = sheetSpecToJson();
    return root;
}

QByteArray SketchModel::toDrawingBytes() const {
    return QJsonDocument(toDrawingJson()).toJson(QJsonDocument::Indented);
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

void SketchModel::setLtscale(double v) {
    if (v <= 0.0) return;                          // 非法值静默忽略
    if (std::fabs(v - ltscale_) < 1e-9) return;   // 幂等：同值不重复发信号
    ltscale_ = v;
    emit ltscaleChanged(v);
    emit modelChanged();                           // 线型比例变化影响所有虚线渲染 → 触发重绘
}

} // namespace SketchCAD
