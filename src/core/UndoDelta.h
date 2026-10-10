#pragma once
// ============================================================================
// UndoDelta —— 撤销栈差量化（变更集）核心数据结构与算法
// ----------------------------------------------------------------------------
// 设计目标：把「每步全量 Memento 快照」替换为「元素级变更集 + 每 50 步一个全量
// 检查点」。辅助状态（单位/界限/捕捉/追踪/布局/过滤器/样式表等，统称 aux）占比很小，
// 仍整段按值快照（见 AuxState），只把图元 vector 走差量，从而把单步内存从 O(N) 降到
// 接近 O(变更图元数)。
//
// 序列化复用 Element 既有编解码 elementToJson / elementFromJson（见 Element.h:469-470），
// 不在本文件重新发明元素序列化。
//
// 本文件刻意只依赖 Element 与各 aux 类型的头文件，不反向依赖 SketchModel，
// 以便 SketchModel 以「值拷贝」方式构造/消费 AuxState（见 SketchModel.cpp）。
// ============================================================================
#include <vector>
#include <map>
#include <QString>
#include <QByteArray>
#include "Element.h"
#include "Layer.h"
#include "Block.h"
#include "TextStyle.h"
#include "DimStyle.h"   // dim::DimStyleEntry
#include "Layout.h"     // PlotLayoutDef
#include "QSelect.h"    // FilterDef
#include "Units.h"      // UnitsMeta / LimitsMeta
#include "Snap.h"       // snap::SnapConfig / snap::TrackConfig

namespace SketchCAD {

// ---- 单图元变更 ----
// Op 决定反向/正向应用时的动作：
//   Added   : 正向=按 index 插入 afterJson；反向=按 id 删除
//   Removed : 正向=按 id 删除；          反向=按 index 插入 beforeJson
//   Modified: 正向=按 id 替换为 afterJson；反向=按 id 替换为 beforeJson
struct ElementDelta {
    enum Op { Added, Removed, Modified };
    Op          op = Added;
    int         id = 0;       // 图元 id（删除/修改按 id 定位，与位置无关，天然兼容 id 复用）
    int         index = 0;    // Added=在「结果态」中的序位；Removed=在「源态」中的序位
    QByteArray  beforeJson;   // 修改前完整状态（Removed 时即「被删图元的完整状态」）
    QByteArray  afterJson;    // 修改后完整状态（Added 时即「新增图元的完整状态」）

    ElementDelta() = default;
    ElementDelta(Op o, int i, int ix, const QByteArray& b, const QByteArray& a)
        : op(o), id(i), index(ix), beforeJson(b), afterJson(a) {}
};

// ---- 辅助状态整段快照（随图属性，按值拷贝，占比小）----
// 字段与 SketchModel::Memento 的非图元字段一一对应（只增不改）。
struct AuxState {
    int  nextId = 1;
    double scale = 1.0;
    std::vector<Layer>            layers;
    std::vector<BlockDef>        blocks;
    std::vector<TextStyle>       textStyles;
    std::map<QString, double>    params;
    std::vector<dim::DimStyleEntry> dimStyles;
    QString                      currentDimStyle;
    std::vector<PlotLayoutDef>   layouts;
    QString                      currentLayout;
    UnitsMeta                    units;
    LimitsMeta                   limits;
    snap::SnapConfig             snapConfig;
    snap::TrackConfig            trackConfig;
    std::vector<FilterDef>       savedFilters;
};

// ---- 一步变更集 ----
// auxBefore / auxAfter 分别是该步「源态/结果态」的辅助状态整段快照。
struct DeltaRecord {
    QString             label;
    AuxState            auxBefore;
    AuxState            auxAfter;
    std::vector<ElementDelta> elems;

    // 是否为空变更（用于快照去重：aux 不变且元素零改动 → 视为同态）
    bool empty() const { return elems.empty(); }
};

// 计算 before → after 的元素级差量（复用 elementToJson 做逐图元字节比较）。
std::vector<ElementDelta> diffElements(const std::vector<Element>& before,
                                       const std::vector<Element>& after);

// 把差量应用到 elems（forward=true 走「结果态」方向，false 走「源态」方向）。
// 内部顺序：先按 id 删除/替换、再按 index 插入，确保 z 序与位置与结果态一致。
void applyElementDeltas(std::vector<Element>& elems,
                        const std::vector<ElementDelta>& ds,
                        bool forward);

} // namespace SketchCAD
