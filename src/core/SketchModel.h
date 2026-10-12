#pragma once
#include <QObject>
#include <vector>
#include <functional>
#include <QString>
#include <QJsonArray>
#include "Element.h"
#include "Vec.h"
#include "LineStyleConfig.h"
#include "DimStyle.h"
#include "Layer.h"
#include "Block.h"
#include "UndoDelta.h"   // 撤销栈差量化：ElementDelta / DeltaRecord / AuxState
#include "Xref.h"
#include "TextStyle.h"
#include "QSelect.h"
#include "Units.h"
#include "Snap.h"
#include "Layout.h"   // PlotLayoutDef / PrintSettings（布局表）
#include "SheetSpec.h"   // SheetSpec / SheetFormat：图纸幅面规格（打印页面尺寸与方向的数据源）
#include "ConstraintSolver.h"   // 显式几何约束表（ConstraintRecord）
#include "Ucs.h"      // 用户坐标系（输入/显示变换，不改图元世界坐标语义）

namespace SketchCAD {

struct SectionStyle;   // 前向声明（定义在 Section.h；避免 SketchModel.h ↔ Section.h 循环包含）

// 中央数据模型（QObject，信号驱动 UI 联动）
class SketchModel : public QObject {
    Q_OBJECT
public:
    explicit SketchModel(QObject* parent = nullptr);

    // ---- 图元操作 ----
    int  addElement(const Element& e);
    bool removeElement(int id);
    bool updateElement(int id, std::function<void(Element&)> fn);
    const Element* element(int id) const;
    const Element* selected() const;
    const std::vector<Element>& elements() const { return elems_; }
    int  elementCount() const { return (int)elems_.size(); }
    void clear();
    void emitModelChanged();   // 批量静默写入后主动触发刷新（Exchange 导入用）

    // ---- 静默编辑（拖拽过程中用）----
    // 拖拽每移动一个像素都压一次撤销快照的话，撤销栈会瞬间被几百帧噪声填满，
    // 用户按一次 Ctrl+Z 只能退回一个像素。所以拆成两半：
    // 按下时 beginEdit() 压一次快照，移动过程全部走 *Quiet（不压栈、只发信号）。
    void beginEdit(const QString& label);
    bool updateElementQuiet(int id, std::function<void(Element&)> fn);
    // 静默添加（不压撤销栈），用于批量修改（modify::applyChanges 等）
    // 调用前需先 beginEdit() 一次，使整组操作可一次 Ctrl+Z 撤回。
    std::vector<int> addElementsQuiet(const std::vector<Element>& parts);
    // 静默删除（不压撤销栈）
    bool removeElementQuiet(int id);

    // ---- 端点捕捉与联动拖拽（需求 3）----
    // 一个「控制点引用」= 哪个图元的第几号控制点。
    struct HandleRef {
        int  elemId = -1;
        int  index  = -1;
        bool valid() const { return elemId >= 0 && index >= 0; }
    };
    // 找 p 附近最近的控制点（选中图元优先，其次按 z 序靠后者优先）
    HandleRef handleAt(const Vec3& p, double tol) const;
    // 收集所有落在 p 容差内的控制点——这就是「拓扑连接关系」的具体表达：
    // 两个图元的端点重合即视为相连。拖拽开始时锁定这张表，之后整段拖拽都用它，
    // 避免拖到一半因为点已经移开而丢失联动对象。
    std::vector<HandleRef> collectLinked(const Vec3& p, double tol) const;
    // 把这一组控制点全部移动到 to（静默）。返回实际改动的图元数。
    int  moveHandles(const std::vector<HandleRef>& refs, const Vec3& to);

    // ---- 批量替换（剪切用）：删 1 加 N，只压一次快照 ----
    std::vector<int> replaceElement(int id, const std::vector<Element>& parts,
                                    const QString& label);

    // ---- 联动更新（需求 2/3）----
    // 更新图元并传播到端点相连的其他图元：先记录旧控制点位置，
    // 修改后再把位置发生变化的控制点连带其上的其他图元一起移到新位置。
    // 内部压一次撤销快照，保证整组联动可一次 Ctrl+Z 撤回。
    bool updateElementLinked(int id, std::function<void(Element&)> fn);
    // 标注驱动：把标注数值写入被引用图元(refA)的几何，并联动传播到相连图元。
    // 返回是否成功驱动（图元类型不匹配时返回 false）。
    bool driveDimension(int dimId, double value);

    // ---- 选中 ----
    void setSelected(int id);
    int  selectedId() const { return selectedId_; }
    // 多选支持（框选 / Ctrl 加选减选）
    void setSelection(const std::vector<int>& ids);          // 替换整个选择集
    void addToSelection(int id);                            // 追加单个
    void addToSelection(const std::vector<int>& ids);        // 批量追加
    void toggleSelection(int id);                           // Ctrl 点击：有则去掉、无则加上
    void clearSelection();
    bool hasSelection() const { return !selectedIds_.empty(); }
    bool isSelected(int id) const;
    const std::vector<int>& selection() const { return selectedIds_; }
    // 选区智能标注：选中单条直线→对齐标注（沿直线垂直方向引出）；
    // 选中单个圆/圆弧→直径标注（⌀）。其它情况返回 -1，交由调用方走常规拾取流程。
    int  smartDimension();

    // ---- 高级选择（选择菜单 / 命令行驱动，对应 FreeCAD Sketcher 选择菜单）----
    // 这些操作在「主选择 + 选择集」双模型上统一维护 selectedId_ / selectedIds_，
    // 与点击选择、框选共用同一套状态，保证属性面板、约束、标注等操作源一致。
    void selectAll();                                       // 全选所有可见图元
    void selectWindow(const QRectF& rect, bool additive = false); // 框选：与 rect 相交者入选
    void deselectWindow(const QRectF& rect);                // 框取消：移除与 rect 相交者
    void selectEntity(int id);                              // 选中单个图元（= setSelected）
    void deselectEntity(int id);                            // 从选择集移除单个图元
    void selectContour(int seedId);                         // 沿拓扑连通性收集整条相连轮廓
    void toggleContour(int seedId);                         // (取消)选择轮廓：轮廓全选中则移除，否则选中
    void selectIntersected(int seedId);                     // 选中与 seed 几何相交的全部图元
    void deselectIntersected(int seedId);                   // 移除与 seed 相交者
    void selectLayer(const QString& layer);                 // 选中指定图层全部图元
    void selectLayerOf(int seedId);                         // 选中与 seed 同图层的全部图元
    void deselectLayer(const QString& layer);               // 移除指定图层图元
    void toggleLayerOf(int seedId);                         // (取消)选择图层：同层全选中则移除，否则选中
    void invertSelection();                                 // 反选（可见图元的补集）

    // ---- 累加选择（按图元属性，保留已有选择）----
    // 把「线型相同」或「颜色相同（RGB 精确匹配，忽略 alpha）」的可见图元加入选择集；
    // 通过 addToSelection 追加（已选中的不去重重复添加、也不清除），满足
    // "按相同线型/相同颜色累加选择，保留已选图元，不取消原有选择" 的需求。
    void addSelectionByStyle(LineStyle ls);
    void addSelectionByColor(const QColor& c);

    // ---- 多选整体平移（一次操作移动所有选中图元，保持相对位置）----
    // 把当前选择集作为一个整体平移 delta（mm）。走 beginEdit + modify::applyChanges(
    // makeMoveCopy)，可一次 Ctrl+Z 撤回；空选择集直接返回。
    // 「修改 > 移动/复制」菜单与 CLI `move` 均经此单一入口，保证拖拽与输入偏移坐标同语义。
    void translateSelection(const Vec3& delta,
                            const QString& undoLabel = QStringLiteral("平移图元"));

    // ---- 图层管理（Layer）----
    // 图层是"属性分组容器"：图元经 Element::layer 归属到某层；对图层做批量改属性或
    // 显示控制时**只遍历本层图元**，其他图层的内容与显示状态保持不变、互不干扰。
    int  layerCount() const { return (int)layers_.size(); }
    const Layer& layerAt(int i) const;                  // 越界回落默认层 "0"
    const Layer* findLayer(const QString& name) const;
    Layer*       findLayer(const QString& name);
    bool hasLayer(const QString& name) const { return findLayer(name) != nullptr; }
    // 新建图层：名字为空/已存在/为保留名返回 false。color 无效时用默认色。
    bool addLayer(const QString& name,
                  const QColor& color = QColor(),
                  double lineWidth = 1.0,
                  LineStyle ls = LineStyle::Solid);
    bool ensureLayer(const QString& name);              // 不存在则建，存在则无操作
    bool renameLayer(const QString& oldName, const QString& newName); // 同步重命名图元归属
    bool removeLayer(const QString& name);              // 保留层"0"不可删；其图元回落"0"
    // 修改图层样式：color/lineWidth/ls 为 nullptr 表示"不改动该项"。
    // 【解耦】仅更新该图层的"定义"（颜色/线宽/线型三项），不再反向推送到已有图元；
    //   本层已有图元的属性保持不变，只有此后新建的图元（经 ElementFactory::create →
    //   applyActiveLayer 快照）才会采用该层的新属性。
    //   （若需把图层当前样式强制套用到本层图元，调用 applyLayerStyleToElements 显式同步。）
    bool setLayerStyle(const QString& name, const QColor* color, const double* lineWidth, const LineStyle* ls);
    bool setLayerVisible(const QString& name, bool visible);   // 批量翻转本层图元 visible
    bool setLayerLocked(const QString& name, bool locked);
    // 把图层样式套用到该层全部图元（批量改属性，仅影响本层），返回受影响图元数。
    // 与 setLayerStyle 区别：这里强制把图层当前「全部三项」样式写回本层图元（用于重新同步）。
    int  applyLayerStyleToElements(const QString& name);
    // 把当前选择集移入指定图层；默认 adoptStyle=false，即"保留图元原有属性，不应用图层样式"。
    int  assignSelectionToLayer(const QString& name, bool adoptStyle = false);
    bool setElementLayer(int id, const QString& name, bool adoptStyle = false);
    bool setElementLayers(const std::vector<int>& ids, const QString& name, bool adoptStyle = false);
    // 序列化：layers_ <-> JSON 数组（写档/读档用；旧文件无此字段则回退单层"0"）
    QJsonArray layersToJson() const;
    void setLayersFromJson(const QJsonArray& arr);

    // ---- 图层动态语义（冻结/打印/透明度；仅追加接口，不动既有接口）----
    // 冻结（frozen）：独立于 visible 的硬隔离——冻结层图元不参与命中/重算/打印（AutoCAD FROZEN 语义）。
    // 打印开关（plot）：false = 该层不打印（可显示但不出图）。
    // 透明度（alpha）：0~1，叠加到本层图元颜色 alpha 通道。
    bool setLayerFrozen(const QString& name, bool frozen);
    bool setLayerPlot(const QString& name, bool plot);
    bool setLayerAlpha(const QString& name, double alpha);
    // ByLayer 解析：byLayer=true 时返回「图层属性覆盖」后的图元副本（颜色/线型/线宽取自图层，线宽按固定值；
    //   图层 alpha 叠加到颜色 alpha）；byLayer=false 或图层缺失时原样返回（快照语义）。
    //   渲染（GraphicsView::penFor）与打印（PlotRenderer::paintOne）两侧共用此唯一入口。
    Element resolveElement(const Element& e) const;
    // 命中可拾取性：visible 且(所属图层未冻结) → 可拾取（冻结层零命中）。供 hitTest 与选择集统一判定。
    bool elementPickable(const Element& e) const;
    // 可打印性：所属图层未冻结且开启打印 → 可打印（冻结/关闭打印层打印零输出）。
    bool elementPrintable(const Element& e) const;

    // ---- 块（Block）管理 ----
    // 采用「引用式」：块表存唯一一份 BlockDef；元素 BlockRef 只存块名 + 插入变换 +
    // 属性值 + 动态参数值。因此"块内图元被修改后同步更新所有引用"= 改一份定义即可，
    // 无需任何反向推送（所有引用在渲染/命中/导出时按定义实时展开）。
    int blockCount() const { return (int)blocks_.size(); }
    const std::vector<BlockDef>& blockTable() const { return blocks_; }
    const BlockDef* findBlockDef(const QString& name) const;
    BlockDef*       findBlockDef(const QString& name);
    bool hasBlock(const QString& name) const { return findBlockDef(name) != nullptr; }
    // 新增块定义：空名 / 重名 / 与保留名冲突 → false。
    bool addBlockDef(const BlockDef& def);
    // 静默新增块定义：不压撤销快照（调用前需 beginEdit() 一次），重名 → false。
    // 剪贴板粘贴等「一次 beginEdit 覆盖块表+图元」的组合操作专用；
    // 独立添加块请走 addBlockDef()（自带快照，可单独撤销）。
    bool addBlockDefQuiet(const BlockDef& def);
    // 用当前选择集创建块定义：以 basePt 为基点收进定义。
    //   deleteSource=true  → 删除原图元并在 basePt 放置一个块引用（AutoCAD BLOCK 语义），返回引用 id；
    //   deleteSource=false → 保留原图元、不放置引用，返回 -1。
    int  createBlockFromSelection(const QString& name, const Vec3& basePt,
                                  bool deleteSource, const QString& description = QString());
    // 用当前选择集重定义已有块：替换其块内图元，所有引用立即同步为新几何。
    bool redefineBlockFromSelection(const QString& name);
    // 直接替换块定义（名字保持不变；不存在则视为新增）。引用同步更新。
    bool redefineBlock(const BlockDef& def);
    bool removeBlock(const QString& name);          // 删除块定义（引用将退化为"未定义"不绘制）
    bool renameBlock(const QString& oldName, const QString& newName);  // 同步更新所有引用的块名
    // 插入块引用：返回新图元 id（块不存在 → -1）。
    int  insertBlock(const QString& name, const Vec3& pos,
                     double rotationDeg = 0.0, double sx = 1.0, double sy = 1.0);
    // 展开块引用为世界坐标子图元（渲染 / 命中 / 导出共用）。非块引用或块未定义 → 空。
    std::vector<Element> expandBlockRef(const Element& ref) const;
    // 分解：把块引用 refId 替换为其展开所得的真实图元（可一次 Ctrl+Z 回滚）。返回新图元 id 列表。
    std::vector<int> explodeBlockRef(int refId);
    // 属性编辑：设置实例的某属性值（tag 不存在则追加）。常量属性拒绝修改。
    bool setBlockAttribute(int refId, const QString& tag, const QString& value);
    // 动态参数：设置数值（拉伸距离 / 旋转角 / 翻转 0|1）
    bool setBlockDynValue(int refId, const QString& name, double value);
    // 动态参数：设置可见性状态名（须是该参数已声明的状态之一）
    bool setBlockDynState(int refId, const QString& name, const QString& state);
    // 属性提取：返回某引用的全部属性（含不可见属性；定义中存在而实例缺失的取默认值）。
    std::vector<BlockAttrValue> extractBlockAttributes(int refId) const;
    // 序列化：块表 <-> JSON 数组（写档/读档；旧文件无此字段则不加载任何块）
    QJsonArray blocksToJson() const;
    void setBlocksFromJson(const QJsonArray& arr);

    // ---- BEDIT（块在位编辑）----
    // 引用式模型下"改块内几何 → 所有引用同步刷新"天然成立：redefineBlockEntities 只改一份
    // BlockDef 的 entities，所有 BlockRef 在渲染/命中/导出时经 expandBlockRef 实时读取，无需反向推送。
    // 把块定义的块内图元取出为可编辑副本（attdefs/dynParams/basePoint 等定义不动，仅取 entities）。
    std::vector<Element> getBlockEntities(const QString& name) const;
    // 写回块内几何：仅替换 entities，保留 attdefs / dynParams / basePoint / description /
    // Xref 字段；自动入撤销栈；引用实时刷新。Esc/取消时调用方不调用本函数即"不落盘"。
    bool redefineBlockEntities(const QString& name, const std::vector<Element>& ents);

    // ---- 填充边界联动（关联填充 / hatchedit）----
    // 关联填充：把 hatchId 的填充边界来源登记为 srcIds（保留边界图元本体不动），
    // 之后边界图元几何变更时经 refreshHatchBoundaries 自动重算该填充的 pts/holes。
    // 仅接受可成环的边界（复用 Boundary 引擎判定）；不成环 → 拒绝并返回 false。
    // 关联默认开（Element::hatchAssociative=true）；detach（置 false）即回到静态填充。
    bool  associateHatchBoundary(int hatchId, const std::vector<int>& srcIds);
    // 关联填充创建：由边界来源图元 srcIds 直接生成一个新的关联填充(Hatch)并入模型，
    // **保留边界图元本体不动** —— 这是"改图后填充随动"的前提（既有"点选/先选后填"
    // 填充会消费掉独占边界源，故关联填充需走本独立入口）。边界不成环 → 返回 -1。
    // style 决定材质/角度/间距；来源 id 记入 hatchSrcIds 且 hatchAssociative=true。
    int   createAssociativeHatch(const std::vector<int>& srcIds, const SectionStyle& style);
    // 断开关联：清空来源并置 hatchAssociative=false（回到静态填充，边界再变不再随动）。
    bool  detachHatchBoundary(int hatchId);
    // 变更联动核心：把「本次被改/被删的图元 id 集合」映射到受影响的关联填充并重算
    // 边界（复用 Boundary 引擎 buildClosedLoopsFromElements）；边界消失/不再成环则把
    // 该填充标记失效（hatchInvalid=true）并计入返回值。仅重算受影响 Hatch（性能约束）。
    // applyChanges / moveHandles / removeElement(s) 等几何变更出口统一调用本函数。
    // 不压撤销栈——由调用方的 beginEdit/单步快照统一覆盖（改边界+填充重算 = 一个编辑步）。
    int   refreshHatchBoundaries(const std::vector<int>& changedIds);
    // hatchedit 支撑：就地改填充的「图案覆盖 / 比例 / 关联」字段（保留边界与既有 4 字段）。
    // 单步撤销：beginEdit 由调用方做一次，本函数只做 updateElementQuiet 写入。
    bool  setHatchPatternOverride(int hatchId, const QString& patName, double scale,
                                  int triStateAssoc /* -1=不动 0=关 1=开 */);

    // ---- 文字样式（STYLE）管理 ----
    // 与图层/块表同级，由模型持有唯一数据源；图元经 Element::styleName 引用样式名，
    // 渲染/导出时按样式表解析字体/字高/宽度因子，实现「改样式 → 引用文字同步更新」。
    int  textStyleCount() const { return (int)textStyles_.size(); }
    const TextStyle& textStyleAt(int i) const;                 // 越界回落默认 "Standard"
    const TextStyle* findTextStyle(const QString& name) const; // 空名返回 nullptr
    bool hasTextStyle(const QString& name) const { return findTextStyle(name) != nullptr; }
    bool addTextStyle(const TextStyle& s);                     // 空名/重名 → false
    bool renameTextStyle(const QString& oldName, const QString& newName); // 同步更新图元引用
    bool updateTextStyle(const TextStyle& s);                 // 按 name 原地更新字段（含重命名）
    bool removeTextStyle(const QString& name);                 // 删除并解除图元引用
    // 把指定样式应用到给定图元（仅对 Text/MText 生效），返回受影响图元数；
    // styleName 为空串表示「清除样式关联、回退默认」。走一次 beginEdit 可整组 Ctrl+Z。
    int  applyStyleToElements(const QString& styleName, const std::vector<int>& ids);
    // 序列化：文字样式表 <-> JSON 数组（旧文件无此字段则回落单一默认 "Standard"）
    QJsonArray textStylesToJson() const;
    void setTextStylesFromJson(const QJsonArray& arr);

    // ---- 编组（GROUP）管理 ----
    // 编组 = 「groupName 相同的图元集合」，无独立编组表（REGION/STYLE 同款引用式范式）：
    // 创建/解散只改写图元的 groupName 字段，走 beginEdit 一次 Ctrl+Z 整组撤销；
    // 图元删除/复制后编组关系自动跟随（无 id 悬空问题）。
    QStringList groupNames() const;                        // 现有编组名（去重升序）
    int  elementCountInGroup(const QString& name) const;   // 编组成员数（含隐藏图元）
    std::vector<int> elementsInGroup(const QString& name) const;
    // 用给定 id 集创建编组（空名/重名/空集 → false）
    bool createGroup(const QString& name, const std::vector<int>& ids);
    // 把选中图元加入编组（图元已属其他编组则改属本组）
    int  addToGroup(const QString& name, const std::vector<int>& ids);
    // 从其所属编组移除（groupName 置空）；返回受影响图元数
    int  removeFromGroup(const std::vector<int>& ids);
    // 解散编组：全部成员 groupName 置空（编组随之消失，图元保留）
    int  ungroup(const QString& name);
    // 编组重命名：同步更新全部成员的 groupName
    bool renameGroup(const QString& oldName, const QString& newName);
    // 整体选中编组全部成员（一次 selectionChanged），返回成员数
    int  selectGroup(const QString& name);

    // ---- 编组整体选中语义（点击拾取扩展）----
    // 普通点击命中编组成员 → 扩展为整组（仅可见成员，保持图元遍历序）；
    // additive（Ctrl/Shift 修饰）= 穿透编组，只返回 {id} 本身（单选语义不变）。
    // 非编组图元无论修饰键一律 {id}。纯查询不改选择集，UI 结算走 setSelection。
    std::vector<int> expandPickToGroup(int id, bool additive) const;

    // ---- 快速选择 / 过滤器（QSELECT / FILTER）与选择集的统一落地口 ----
    // 替换/追加/排除一次完成，全程只发一次 selectionChanged（大图 O(n) 次刷新禁绝）。
    void applySelectionDelta(const std::vector<int>& hits, QSelectAction action);

    // ---- 命名过滤器（FILTER 保存/复用）----
    // 仅持久化（顶层图纸 JSON "savedFilters"），**不入撤销栈**——过滤器是检索配置
    // 而非图纸内容，与打印设置同级的"应用级配置"语义。
    const std::vector<FilterDef>& savedFilters() const { return savedFilters_; }
    bool saveFilter(const FilterDef& f);                   // 按名保存（重名覆盖；空名/无条件 → false）
    bool deleteFilter(const QString& name);
    QJsonArray savedFiltersToJson() const;
    void setSavedFiltersFromJson(const QJsonArray& arr);

    // ---- FILTER 预选器（作用于「随后一次」选择操作，用后即清）----
    // 与 savedFilters（持久化命名过滤器）不同：预选器是**瞬态会话配置**（不入撤销栈、
    // 不随图存档）。设置后，下一次点击/框选/栏选的候选集经 matchFilter 过滤，随即自动
    // 清除——「预选」语义：约束一次选择，而不是持续改写选择行为。
    const FilterDef& activePickFilter() const { return activePickFilter_; }
    bool hasActivePickFilter() const { return !activePickFilter_.conds.empty(); }
    // 空条件 FilterDef 等价清除（幂等）
    void setActivePickFilter(const FilterDef& f);
    void clearActivePickFilter();
    // 对候选集套用预选器：无活动预选器 → 原样返回（不清除）；有 → 仅保留可见且
    // matchFilter 命中的候选（保持原顺序），并消费（清除）预选器。UI 三处结算口共用。
    std::vector<int> applyPickFilterOnce(const std::vector<int>& candidates);

    // ---- 单位（UNITS）与图纸界限（LIMITS）----
    // 单位不变式：内部几何一律 mm；单位设置只定义「显示与输入的换算口径」。
    // 变更发 unitsChanged/limitsChanged 信号（状态栏坐标、参数输入条实时跟随），
    // 不入撤销栈（与打印设置同级的"应用级配置"），随图 JSON 持久化（缺键回落 mm/A3）。
    const UnitsMeta& units() const { return units_; }
    void setUnits(const UnitsMeta& u);                 // 与现值相同则不发信号
    const LimitsMeta& limits() const { return limits_; }
    void setLimits(const LimitsMeta& l);

    // ---- 图纸幅面规格（打印/预览/PDF 页面尺寸与方向的唯一数据源）----
    // 新建图纸时由 MainWindow::applyNewDrawing 写入；随图存档顶层 JSON "sheet"
    // （{"format":0..5, "landscape":bool}；缺键 = 旧档 → 读档后按「图框」图层
    // 纸张矩形推断自愈，见 inferSheetSpecFromFrame）。
    // 语义边界：与单位/界限同级——**不入撤销栈**（新建/读档时设定，运行期无修改入口）；
    // 修复的缺陷即「新建 A4 横向 → 打印预览/导出 PDF 却出 A4 纵向」：页面设置
    // 必须从图纸幅面派生（applySheetSpecToPage），而不是各站点各自默认 A4 纵向。
    bool hasSheetSpec() const { return hasSheetSpec_; }
    const SheetSpec& sheetSpec() const { return sheetSpec_; }   // 仅 hasSheetSpec() 为真时有效
    void setSheetSpec(const SheetSpec& s);                      // format 越界 → 拒绝（保持原值）
    void clearSheetSpec() { hasSheetSpec_ = false; }            // clear() 内部调用
    // 从「图框」图层面积最大的矩形（纸张外边界）推断幅面与方向；
    // 无矩形 / 非标尺寸 → nullopt（调用方放弃推断，绝不猜）
    std::optional<SheetSpec> inferSheetSpecFromFrame() const;
    QJsonObject sheetSpecToJson() const;                        // 无规格 → 空对象（不写 "sheet" 键）
    static std::optional<SheetSpec> sheetSpecFromJson(const QJsonObject& o);   // 缺键/非法 → nullopt    // 格式化便捷口（状态栏 / 属性面板 / 查询结果统一走此，杜绝两处口径漂移）
    QString formatLength(double mm) const { return formatLengthMm(mm, units_); }
    // 注意：同名自由函数被类内成员遮蔽，须 SketchCAD:: 限定才能命中双参版本
    QString formatLengthWithUnit(double mm) const { return SketchCAD::formatLengthWithUnit(mm, units_); }
    QString formatAngle(double deg) const { return formatAngleDeg(deg, units_); }
    // 新图元越界检查（LIMITS checkOn 开启时告警；坐标为模型 mm）
    void checkElementLimits(const Element& e);

    // ---- 捕捉全局配置（OSNAP 持久化）----
    // 全局持久配置：用户在各处设定的「默认开启哪些捕捉」随图存档（顶层 JSON "snapConfig"），
    // 不入撤销栈（属应用级配置，与单位/界限同级）。绘制命令进入时取此值作为活动配置，
    // 离开时复原；其上的「临时覆盖」与「命令默认」只作用于命令期内的活动快照（见 GraphicsView）。
    const snap::SnapConfig& snapConfig() const { return snapConfig_; }
    void setSnapConfig(const snap::SnapConfig& c);     // 与现值相同则不发信号

    // ---- 追踪全局配置（极轴/对象追踪，随图存档顶层 JSON "trackConfig"，不入撤销栈）----
    const snap::TrackConfig& trackConfig() const { return trackConfig_; }
    void setTrackConfig(const snap::TrackConfig& c);   // 与现值相同则不发信号
    // ---- 临时追踪点（TK，瞬态，不随图存档）----
    const std::vector<Vec3>& trackPoints() const { return trackPoints_; }
    void addTrackPoint(const Vec3& p);                 // 放置一个临时追踪点
    void clearTrackPoints();                           // 清空全部临时追踪点

    // ---- 显式几何约束表 ----
    const std::vector<ConstraintRecord>& constraints() const { return constraints_; }
    void addConstraintRecord(const ConstraintRecord& r);  // 追加并发信号
    void removeConstraint(int index);                     // 按下标删除（删除后 emit constraintsChanged）
    void clearConstraints();                           // 清空全部显式约束
    void setConstraints(const std::vector<ConstraintRecord>& v);  // 整体替换（读档用）
    QJsonArray  constraintsToJson() const;             // 随图存档
    void setConstraintsFromJson(const QJsonArray& a);  // 读档（旧文件无此键 → 空表）

    // ---- 参数表（尺寸驱动 / 参数化建模，Request M 新增）----
    // 命名设计变量（如 "L"=120, "ANG"=30）。与约束/标注的关联经
    // ConstraintSolver::linkDimension / driveParameter 建立。setParam 触发 paramsChanged。
    const std::map<QString,double>& params() const { return params_; }
    bool   hasParam(const QString& name) const;
    double param(const QString& name) const;             // 不存在返回 0.0
    void   setParam(const QString& name, double val);    // 改值并压一次撤销快照
    QJsonObject paramsToJson() const;                   // 随图存档顶层 JSON "params"
    void   setParamsFromJson(const QJsonObject& o);      // 读档（旧文件无此键 → 空表）

    // ---- 外部参照（Xref）管理 ----
    // 附加：读入外部图形文件，登记为一条 Xref 块记录，并在 insertPt 放置一个参照实例。
    //   pathType 决定 xrefPath 的**存盘形态**（完整路径 / 相对宿主图目录 / 仅文件名），
    //   解析时按同一天基准还原。插入点 / 缩放 / 旋转均有默认值（原点、1.0、0°）。
    //   返回新参照实例的图元 id；失败返回 -1 并写 warn 日志，
    //   覆盖：文件路径为空、文件不存在、格式不支持、该参照已附加、块名被普通块占用。
    int  attachXref(const QString& filePath, XrefPathType pathType = XrefPathType::FullPath,
                    const Vec3& insertPt = Vec3(0, 0, 0),
                    double rotationDeg = 0.0, double sx = 1.0, double sy = 1.0);
    // 分离：按块名移除 Xref 定义与它的**全部**参照实例（一次 Ctrl+Z 可撤）。
    //   因本次分离而失去全部引用者的嵌套参照会被级联清除（AutoCAD 语义）。
    //   块名不存在 / 不是外部参照 / 是嵌套参照 → false 并写 warn 日志。
    bool detachXref(const QString& blockName);
    // 已附加的外部参照名列表；includeNested=false 时只列本图直接附加的那些
    QStringList xrefNames(bool includeNested = false) const;
    int  xrefCount(bool includeNested = false) const;
    // 同一源文件是否已附加（按解析后的绝对路径比对，不受路径类型影响）
    bool hasXrefSource(const QString& filePath) const;
    // 把记录的 xrefPath 按路径类型还原为绝对路径（诊断 / 日志用）；非 Xref 返回空串
    QString xrefResolvedPath(const QString& blockName) const;

    // ---- 当前图纸文件路径（外部参照相对路径的解析基准）----
    // 相对路径必须相对「宿主图所在目录」才有意义，故读写档时由 UI 层登记；
    // 未存盘宿主图为空串，此时相对路径退化为纯文件名。
    QString drawingPath() const { return drawingPath_; }
    void    setDrawingPath(const QString& p) { drawingPath_ = p; }
    QString drawingDir() const;              // drawingPath 所在目录；空 → 返回空串

    // ---- 当前活动图层（绘制目标层）----
    // 新创建的图元自动归属此层并继承其属性；默认 "0"。
    // 切换活动层只改变"新图元去哪一层 + 继承哪层属性"，不改动任何已有图元，
    // 也不影响正在绘制的图元（正在绘制的预览仍按当前活动层渲染，符合"新图元归属当前层"）。
    QString currentLayer() const { return currentLayer_; }
    void    setCurrentLayer(const QString& name);

    // ---- 交互/视图状态 ----
    void setActiveTool(const QString& t);
    QString activeTool() const { return activeTool_; }
    void setPlane(Plane p);
    Plane plane() const { return plane_; }

    // ---- 用户坐标系（UCS）----
    // UCS 是「输入/显示变换」层，**绝不改动**图元存储的世界坐标语义（Element.pts 恒为世界坐标）。
    // 所有用户输入（命令行点 / 参数条 / 动态输入）与显示（栅格 / 坐标轴 / 极轴 / 正交 / 读数）
    // 都经 toWorld / toUcs 双向换算，保证唯一入口、方向一致。
    // 默认 UCS = 世界系（origin=0, angleRad=0），故既有命令 / 自测在未被显式切换 UCS 时行为完全不变。
    const Ucs& currentUcs() const { return currentUcs_; }
    void setCurrentUcs(const Ucs& u);                  // 变化发 ucsChanged() + viewChanged()
    // 双向换算唯一入口（世界坐标 ↔ UCS 局部坐标）
    Vec3 toWorld(const Vec3& p) const { return currentUcs_.toWorld(p); }
    Vec3 toUcs(const Vec3& w) const   { return currentUcs_.toUcs(w); }
    // 命令行/参数条点的 UCS 感知解析：支持 x,y[z] / @dx,dy / dist<angle / @dist<angle，
    // base 为相对坐标的基准点（世界坐标）。解析出的局部坐标经 toWorld 转世界系。
    bool parsePointToWorld(const QString& s, const Vec3& base, Vec3& outWorld) const;
    // 坐标读数显示模式（WCS 显示世界坐标 / UCS 显示局部坐标）；切换发 ucsChanged()。
    bool coordDisplayUcs() const { return coordDisplayUcs_; }
    void setCoordDisplayUcs(bool on);
    // 给定世界坐标，按当前显示模式返回应读出的坐标（WCS=原值，UCS=toUcs 结果）。
    Vec3 displayCoord(const Vec3& world) const {
        return coordDisplayUcs_ ? toUcs(world) : world;
    }
    // ---- 命名 UCS 表（内存态，不随图存档；避免 JSON 兼容回归）----
    int  ucsCount() const { return (int)ucsTable_.size(); }
    QStringList ucsNames() const;
    bool ucsHas(const QString& name) const;
    Ucs  ucsGet(const QString& name) const;            // 不存在返回世界系
    bool ucsSave(const QString& name);                // 以当前 UCS 存为命名（覆盖同名）
    bool ucsDelete(const QString& name);              // 删除命名（世界系不可删）
    bool ucsSetCurrent(const QString& name);           // 切换到命名 UCS（不存在返回 false）
    // 上一次键盘点输入的基准点（世界坐标），供相对坐标 @ 解析复用（非信号、不入存档）。
    Vec3 lastInputPointWorld() const { return lastInputPointWorld_; }
    void setLastInputPointWorld(const Vec3& p) { lastInputPointWorld_ = p; }
    void setShowGrid(bool b);
    bool showGrid() const { return showGrid_; }
    void setViewScale(double s);
    double viewScale() const { return viewScale_; }

    // ---- 输入约束：正交（ORTHO）+ 栅格吸附（SNAP）----
    // 单一状态源：快捷键 F8/F9、状态栏按钮、命令 ortho/snap 三条入口**都**经由这里的
    // setOrthoMode / setGridSnap 写入，任何一处改动都会发信号广播给其余入口，
    // 从根上消除「多处各存一份状态导致不一致」的问题。二者均不入撤销栈（属交互模式，非图数据）。
    bool orthoMode() const { return orthoMode_; }
    void setOrthoMode(bool on);                 // 幂等；变化发 orthoModeChanged()
    bool gridSnap() const { return gridSnap_; }
    void setGridSnap(bool on);                  // 幂等；变化发 gridSnapChanged()
    double gridSnapStep() const { return gridSnapStep_; }
    void setGridSnapStep(double mm);            // 吸附步长（mm，>0）；变化发 gridSnapChanged()

    // ---- 绘图比例（GB/T 14690 技术制图 比例）----
    // 默认 1:1。修改比例会把已有图元（不含图框/标题栏）按内容包围盒中心等比缩放，
    // 使「绘图根据比例进行缩放」。新建图纸时由新建对话框设定初始比例。
    double scaleRatio() const { return scaleRatio_; }
    QString scaleLabel() const;                 // 如 "1:1" / "1:2" / "2:1"
    // 标注样式的**唯一构造入口**：已带上当前绘图比例。
    // 凡是要读标注数字的地方（渲染、导出、日志）都应从这里取样式再调 dim::build，
    // 否则会退回默认 1:1，把 1:2 图纸上的 100mm 显示成 50。
    dim::Style dimStyle() const;
    // ---- 命名标注样式表（一图多标准 GB/T 4458.4 + ISO 129-1 并存）----
    // 图元经 Element::dimStyleName **按名引用**表中条目（引用式，对标 blockTable 范式）：
    // 改某条样式 → 引用它的全部标注实时联动；无 id 悬空。
    int  dimStyleCount() const { return (int)dimStyles_.size(); }
    const dim::DimStyleEntry* dimStyleAt(int i) const;             // 越界返回 nullptr
    const dim::DimStyleEntry* findDimStyle(const QString& name) const;
    bool hasDimStyle(const QString& name) const { return findDimStyle(name) != nullptr; }
    // 新建样式：空名 / 重名 → false（自带撤销快照，可单独撤销）
    bool addDimStyle(const dim::DimStyleEntry& e);
    // 原地更新样式定义（按 name；改名同步更新引用它的图元 dimStyleName）
    bool updateDimStyle(const dim::DimStyleEntry& e);
    // 重命名样式：同步更新所有引用它的图元
    bool renameDimStyle(const QString& oldName, const QString& newName);
    // 删除样式：引用它的图元 dimStyleName 置空（回落当前样式）
    bool removeDimStyle(const QString& name);
    QString currentDimStyle() const { return currentDimStyle_; }
    bool setCurrentDimStyle(const QString& name);
    // **解析某图元实际应使用的样式**：dimStyleName → 表中条目；为空/未命中 → 当前样式；
    // 两者都无则回落一张默认 dim::Style。已带上当前绘图比例（见 dimStyle()）。
    // 渲染/导出/日志一律走此入口（与 dimStyle() 单一比例入口同源）。
    dim::Style dimStyleFor(const Element& e) const;
    // 序列化：样式表 + 当前样式名（随图存档；旧档无此字段 → 自动生成默认 "Standard"）
    QJsonObject dimStylesToJson() const;
    void       setDimStylesFromJson(const QJsonObject& o);
    // 快速标注（QDIM）：对当前选择集（平行线 / 圆阵列等）一次性生成基线或连续标注串，
    // 复用 dim 基线/连续引擎（DimensionType::Baseline / Continue + e.radius 层叠）。
    // 锚点提取：圆/弧取圆心、其余取首末点中点；去重后按主轴排序；不足 2 点返回空。
    // 生成标注记当前样式名（dimStyleName），故随样式切换全图联动。
    std::vector<int> quickDimSelected(const QString& mode = QStringLiteral("baseline"));
    void   setScaleRatio(double r, bool allowUndo = true);
    // 只改比例值、不缩放图元。用于读档：存档里的几何已经是按该比例画好的成品，
    // 若再走 setScaleRatio 会被二次缩放。allowUndo=false 时纯赋值（不压撤销）。
    void   setScaleRatioValue(double r);

    // ---- 布局表（多布局：布局名 + 页面设置 + 视口集；随图存档，不入撤销栈）----
    // 每个 PlotLayoutDef 持有一份完整 PrintSettings（页面参数/样式映射/视口集）。
    // currentLayout_ 为空 → 模型空间打印（旧档无布局 key 时的回落行为）。
    int  layoutCount() const { return (int)layouts_.size(); }
    const PlotLayoutDef* layoutAt(int i) const;                // 越界返回 nullptr
    const PlotLayoutDef* findLayout(const QString& name) const;
    bool hasLayout(const QString& name) const { return findLayout(name) != nullptr; }
    // 新建布局：空名 / 重名 → false。发出 layoutsChanged()。
    bool addLayout(const QString& name, const PrintSettings& settings);
    // 删除布局：若被删的是当前布局 → currentLayout_ 清空（回落模型空间）。
    bool removeLayout(const QString& name);
    bool renameLayout(const QString& oldName, const QString& newName);
    // 原地替换布局的页面设置/视口集（不改名）；未命中 → false。
    bool updateLayout(const QString& name, const PrintSettings& settings);
    QString currentLayout() const { return currentLayout_; }   // 空 = 模型空间
    // 切换当前布局：空名 = 切回模型空间；名字不存在 → false。
    bool setCurrentLayout(const QString& name);
    // 当前布局定义；无布局/当前为模型空间 → nullptr（打印链按模型空间处理）。
    const PlotLayoutDef* currentLayoutDef() const;
    // 序列化：顶层 "layouts" 数组 + "currentLayout" 字符串。旧档缺键 → 空表回落模型空间。
    QJsonArray layoutsToJson() const;
    void       setLayoutsFromJson(const QJsonObject& root);

    // ---- 集中式整图序列化入口（DRY，供写文件与自动保存快照共用）----
    // 顶层 JSON 结构与 MainWindow::writeDrawing 的 Json 分支完全一致，不改动任何既有格式。
    // toDrawingJson() 返回根对象；toDrawingBytes() 返回 Indented UTF-8 字节（Json 格式）。
    // 注意：DXF/DWG/STEP 等非 Json 格式仍由 MainWindow 走 cadfmt 展开，此处只覆盖 Json 存档。
    QJsonObject toDrawingJson() const;
    QByteArray  toDrawingBytes() const;

    // ---- 全局线型比例（LTSCALE，随图存档，不入撤销栈）----
    // 控制所有虚线/点画线/双点画线的线素长度（dashSpecFor 图案整体缩放 LTSCALE 倍）。
    // 渲染（penFor）与打印（makePen）两侧共用 model_->ltscale()，保证屏幕与图纸比例一致。
    double ltscale() const { return ltscale_; }
    void   setLtscale(double v);   // 与现值相同/非法(≤0)则静默忽略；变化发 ltscaleChanged()

    // ---- 自定义图纸特性（FIELD 域 PROP:name 的数据源；随图存档，不入撤销栈）----
    // 名称→值的字符串表，随顶层 JSON "properties" 键存档（旧档缺键 → 空表，向后兼容）。
    // 典型键：项目名称/图号/材料/设计者——供 MText/Table 以 %<PROP:项目名称>% 引用。
    const std::map<QString,QString>& drawingProps() const { return drawingProps_; }
    QString drawingProp(const QString& name) const;   // 缺名 → 空串
    void setDrawingProp(const QString& name, const QString& value);  // 空值 = 移除；幂等
    QJsonObject propsToJson() const;
    void setPropsFromJson(const QJsonObject& o);      // 读档（旧文件无此键 → 空表）

    // ---- 域失效重算（FIELD：applyChanges 钩子 + fieldupdate 命令 + 打开文档刷新共用）----
    // 扫描 MText/Table 中的域宿主（含 %<...>%、{field:...} 或 =公式单元格）并预热求值：
    //   * changedIds 非空 → 只统计引用了这些图元的宿主（AREA:id / PERIM:id 精确匹配）；
    //   * changedIds 为空 → 全模型扫描（fieldupdate 无参 / 打开文档自动刷新）。
    // 求值走 resolveFields / evalTableCellFormula（渲染期同源），异常回落 "###"；
    // 只读不改几何、不压撤销（与 refreshHatchBoundaries 同 Quiet 语义）。返回宿主数。
    int refreshFields(const std::vector<int>& changedIds = {});

    // ---- 绘制前样式（新建图元的默认颜色 / 线宽）----
    // 放在模型里而不是某个面板里，是为了让工具条、菜单、属性面板、命令行
    // 四条输入通路读到同一个值，不会出现「工具条选了红色但命令行画出来是黑的」。
    void   setDefaultColor(const QColor& c);
    QColor defaultColor() const { return defaultColor_; }
    void   setDefaultWidth(double px);
    // 当前默认线型对应的默认线宽（来自 LineStyleConfig 单一数据源）
    double defaultWidth() const { return LineStyleConfig::instance().defaultWidth(defaultLineStyle_); }
    void   setDefaultLineStyle(LineStyle ls);
    LineStyle defaultLineStyle() const { return defaultLineStyle_; }
    // 把当前默认样式套用到一个新图元上：线宽按线型自动派生（lineWidthAuto=true）
    void   applyDefaultStyle(Element& e) const {
        e.color = defaultColor_;
        e.lineStyle = defaultLineStyle_;
        e.lineWidthAuto = true;                       // 线宽随线型派生
        e.lineWidth = LineStyleConfig::instance().defaultWidth(e.lineStyle);
    }
    // 把"当前活动图层"套用到新图元：归属该层 + 继承其颜色/线型/线宽（一次性快照拷贝）。
    // 默认层 "0" 不覆盖——保留 applyDefaultStyle 的既有行为（工具栏默认色/线型，线宽自动）；
    // 仅当活动层为非默认层时才套用图层属性，满足"切换图层后新建图元继承该层属性"。
    // 继承是快照：图元从此拥有自己的属性副本，后续图层样式变化（setLayerStyle）或单个图元
    // 单独编辑都不会反向影响彼此，天然满足"单个图元可单独修改、互不影响"。
    void   applyActiveLayer(Element& e) const;
    // 把样式写入当前选中图元；无选中返回 false
    bool   styleSelected(const QColor* c, const double* px);
    bool   styleSelectedEx(const QColor* c, const double* px, const LineStyle* ls);

    // ---- 当前标注标准（GB/T 4458 ↔ ISO 129）----
    // 新建标注默认采用此标准；切换时重戳所有已有标注并触发重绘。
    void   setDimensionStandard(DimensionStandard s);
    DimensionStandard dimensionStandard() const { return dimensionStandard_; }

    // ---- 连续直线模式 ----
    void setChainLine(bool b);
    bool chainLine() const { return chainLine_; }

    // ---- 直线绘制参数输入条（角度/长度、平行线间隔/条数）----
    // 工具条「参数输入条」写入，绘制命令（角度直线 / 平行线）读取，二者经模型同源。
    struct LineDrawParams {
        double angleDeg = 30.0;   // 角度直线与 X 轴夹角（度）
        double length   = 100.0;  // 角度直线长度（mm）
        double spacing  = 20.0;   // 平行线间隔（mm）
        int    count    = 1;      // 平行线条数（默认 1）
    };
    LineDrawParams lineParams() const { return lp_; }
    void setLineParamAngle(double v);
    void setLineParamLength(double v);
    void setLineParamSpacing(double v);
    void setLineParamCount(int v);

    // ---- 日志/状态 ----
    void log(const QString& msg, const QString& type = "info");
    void setStatusCommand(const QString& cmd);

    // ---- 分段式命令提示 ----
    // 当前绘制命令的分步提示文本（如"直线 — 请输入起点:"），
    // 由 GraphicsView 在每次状态变化时更新，CommandLine 监听信号实时显示。
    QString commandPrompt() const { return commandPrompt_; }
    void setCommandPrompt(const QString& p);

    // ---- 分段命令选项提示 ----
    // 当前绘制命令的可选项关键字列表（如 ["C/闭合", "U/放弃"]），
    // 由 GraphicsView 在分段状态变化时设置；CommandLine / StatusBar 监听信号显示 [C/闭合]。
    const QStringList& commandOptions() const { return commandOptions_; }
    void setCommandOptions(const QStringList& opts);

    // ---- 线宽显示开关 ----
    // 控制是否按图元实际线宽渲染（对应 AutoCAD LWDISPLAY）。默认关：一律细线。
    bool showLineWeight() const { return showLineWeight_; }
    void setShowLineWeight(bool on);          // 幂等；变化发 showLineWeightChanged()

    // ---- 空闲态右键行为（AutoCAD 交互对齐）----
    // true（默认，AutoCAD 约定）= select 空闲态右键重复上一命令；
    // false = 既有行为（弹出上下文菜单）。应用习惯，随 AppSettings
    // (interaction/rightClickIdleRepeat) 持久化；不入档、不入撤销栈。
    bool rightClickIdleRepeat() const { return rightClickIdleRepeat_; }
    void setRightClickIdleRepeat(bool on) { rightClickIdleRepeat_ = on; }   // 幂等直赋

    // ---- 动态输入 DYN ----
    // 绘制命令期间在光标旁显示「距离/角度」输入框（对应 AutoCAD DYN）。默认开。
    bool dynMode() const { return dynMode_; }
    void setDynMode(bool on);                 // 幂等；变化发 dynModeChanged()

    // ---- 撤销/重做/复制粘贴 ----
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

    // ---- 撤销栈深度上限（步）----
    // 默认 100；超出时丢弃最旧的一步（undo/redo 双栈同限，避免 redo 侧无界增长）。
    // 它记录的是「应用习惯」（用户愿意为历史付出多少内存），属 AppSettings 管辖、随注册表走，
    // **不入撤销栈**——否则"改栈深"本身会成为一个可撤销步，语义自我指涉。
    int  undoDepthLimit() const { return undoDepthLimit_; }
    void setUndoDepthLimit(int steps);        // 非正/越界 → 回落默认 100
    // 只读诊断/自测：栈内当前快照数（不是深度上限）
    int  undoStackSize() const { return (int)undoStack_.size(); }
    int  redoStackSize() const { return (int)redoStack_.size(); }
    // 诊断：差量模式下，单个差量步所存「元素变更 JSON 字节数」的上限（不含周期全量检查点）。
    // 用于验收「单步撤销内存 ≤1MB」：典型编辑只动个别图元，远小于 1MB。
    qint64 maxDeltaEntryBytes() const;

    // ---- 撤销差量化特性开关 ----
    // 默认开（差量存储）；可在 AppSettings 对照回落全量模式。运行时切换会清空现有撤销历史
    // （两种模式的栈表示不互换，清空是唯一安全做法，见设计文档）。
    bool undoDeltaEnabled() const { return deltaUndoEnabled_; }
    void setUndoDeltaEnabled(bool on);

    // ---- 撤销快照抑制（RAII）----
    // 反序列化/批量载入专用：打开图纸时调用方已 beginEdit("打开草图") 建立唯一编辑步，
    // 随后 setUnits/setLimits/setSnapConfig/setTrackConfig 等不得再各自入栈，
    // 否则一次「打开」会被拆成好几步 Ctrl+Z（几何回了、配置没回的新变体）。
    // 嵌套安全：构造时保存并置起，析构时恢复原值（不无条件置 false）。
    class UndoGuard {
    public:
        explicit UndoGuard(SketchModel* m) : m_(m), old_(m ? m->undoSuppressed_ : false) {
            if (m_) m_->undoSuppressed_ = true;
        }
        ~UndoGuard() { if (m_) m_->undoSuppressed_ = old_; }
        UndoGuard(const UndoGuard&) = delete;
        UndoGuard& operator=(const UndoGuard&) = delete;
    private:
        SketchModel* m_;
        bool old_;
    };

    void copySelected();
    void paste();
    void deleteSelected();
    void reorderElement(int id, int beforeId); // 将 id 移到 beforeId 之前；beforeId<0 表示移到末尾

signals:
    void modelChanged();
    void selectionChanged(int id);
    void elementUpdated(int id);
    void logAppended(const QString& msg, const QString& type);
    void statusCommandChanged(const QString& cmd);
    void activeToolChanged(const QString& tool);
    void viewChanged();
    void planeChanged(Plane p);
    void ucsChanged();                                 // 当前 UCS / 坐标显示模式变化（状态栏/视图联动）
    void gridToggled(bool show);
    void orthoModeChanged(bool on);             // 正交模式变化（状态栏/快捷键/命令三入口同步）
    void gridSnapChanged();                     // 栅格吸附开关或步长变化
    void cursorMoved(double x, double y, double z);
    void defaultStyleChanged(const QColor& color, double width);
    void defaultLineStyleChanged(int style);
    void chainLineToggled(bool on);
    void lineParamsChanged();
    void dimensionStandardChanged(DimensionStandard s);
    void commandPromptChanged(const QString& prompt);
    void commandOptionsChanged(const QStringList& opts);   // 分段命令可选项变化（状态栏/命令行提示）
    void showLineWeightChanged(bool on);                    // 线宽显示开关变化（状态栏/渲染联动）
    void dynModeChanged(bool on);                            // 动态输入 DYN 开关变化
    void scaleChanged(double ratio);
    void ltscaleChanged(double ratio);                 // 全局线型比例变化（状态栏/渲染联动）
    void layersChanged();                    // 图层表增删/改名/样式/可见性变化
    void currentLayerChanged(const QString& name);   // 活动图层切换（用于 HUD 实时显示 / 面板高亮）
    void blocksChanged();                    // 块表增删/改名/重定义变化（引用需重绘）
    void textStylesChanged();                // 文字样式表变化（管理器需刷新）
    void dimStylesChanged();                 // 标注样式表/当前样式变化（引用它的标注需重绘）
    void layoutsChanged();                   // 布局表/当前布局变化（布局管理器/打印链需刷新）
    void unitsChanged();                     // 单位设置变化（状态栏/参数输入条实时跟随）
    void limitsChanged();                    // 图纸界限变化
    void snapConfigChanged();                // 捕捉全局配置变化（设置面板/菜单改动后即时生效）
    void trackConfigChanged();               // 追踪全局配置变化（设置面板/菜单改动后即时生效）
    void trackPointsChanged();               // 临时追踪点集合变化（TK 放置/清空）
    void constraintsChanged();               // 显式几何约束表变化（添加/求解/清空）
    void paramsChanged();                    // 参数表变化（添加/修改/删除设计变量）

private:
    // 撤销快照。**字段只增不改、只追加到尾部**：既有字段的含义与顺序是旧档/旧行为契约，
    // 追加的随图属性（布局/单位/界限/捕捉/追踪/过滤器）全部放在尾部，不影响任何既有读取。
    struct Memento {
        QString label; std::vector<Element> elems; int nextId; double scale = 1.0;
        std::vector<Layer> layers; std::vector<BlockDef> blocks; std::vector<TextStyle> textStyles;
        std::map<QString,double> params; std::vector<dim::DimStyleEntry> dimStyles; QString currentDimStyle;
        // ---- 随图属性（「万能回退」补齐；只追加，不改上方任何字段）----
        std::vector<PlotLayoutDef> layouts;   // 布局表（含各布局完整页面设置）
        QString currentLayout;                // 当前布局名（空 = 模型空间）
        UnitsMeta  units;                     // 单位设置（显示/输入口径）
        LimitsMeta limits;                    // 图纸界限
        snap::SnapConfig  snapConfig;         // 捕捉全局配置
        snap::TrackConfig trackConfig;        // 追踪全局配置
        std::vector<FilterDef> savedFilters;  // 命名过滤器
        // 注：Xref 附加表**没有单列字段**——外部参照本身就是块表记录（BlockDef::isXref），
        // 已由上方 blocks 完整覆盖；再存一份会制造第二个真相源。见设计文档。
    };

    // ---- 撤销栈差量化：单步存储（出向差量语义）----
    // 每个条目表示「某一步操作之前的状态」，并存两样东西：
    //   * 状态表示   ：isCheckpoint=true 时 snap 存整张 Memento（全量检查点，每 50 步一个）；
    //   * 出向差量   ：delta 描述「该状态 → 下一步之后状态」的元素变更集（E_i→E_{i+1}），
    //                  即**刚执行完的那步操作**本身。条目创建（push，改前）时差量未知，
    //                  延迟到下一次 push / undo / redo 时封印（见 sealPendingStep）。
    // 撤销 = 弹出栈顶条目并**逆向应用**其出向差量（就地改 elems_，O(变更图元数)）；
    // 重做 = 从 redo 栈取回条目**正向应用**其出向差量并原样压回（检查点属性自动保留）。
    // 二者不再需要整图 reconstruct + 全量 diff——那是旧实现（入向差量）单步撤销
    // 5 万图元 39ms 的根源；新实现实测 <1ms。reconstructStep 保留为检查点回放工具。
    struct UndoStep {
        bool       isCheckpoint = false;
        Memento    snap;          // 有效 iff isCheckpoint（全量检查点锚）
        DeltaRecord delta;        // 出向差量（封印后有效；空 elems+aux 全等 = 无操作步）
    };
    // 全量检查点间隔（步）。内存/回放代价权衡：回放最多回退 kCheckpointInterval-1 个差量。
    static constexpr int kUndoCheckpointInterval = 50;
    std::vector<Element> elems_;
    int nextId_ = 1;
    int selectedId_ = -1;               // 主选择（属性面板编辑对象 / 约束等单对象操作）
    std::vector<int> selectedIds_;      // 当前选择集（框选 / 加选后可能含多个）
    QString activeTool_ = "select";
    Plane plane_ = Plane::XY;
    // ---- 用户坐标系（UCS）：纯输入/显示变换状态，不入存档、不入撤销栈 ----
    Ucs currentUcs_ = Ucs("WORLD", Vec3(0, 0, 0), 0.0);   // 默认世界系
    QMap<QString, Ucs> ucsTable_;                          // 命名 UCS 表（内存态，不序列化）
    bool coordDisplayUcs_ = false;                        // 坐标读数显示模式（false=WCS, true=UCS）
    Vec3 lastInputPointWorld_ = Vec3(0, 0, 0);            // 相对坐标 @ 解析的基准点（世界坐标）
    bool showGrid_ = true;
    // ---- 输入约束状态（单一状态源）----
    bool   orthoMode_    = false;   // 正交：方向锁定到水平/垂直
    bool   gridSnap_     = false;   // 栅格吸附：坐标量化到步长整数倍
    double gridSnapStep_ = 1.0;     // 栅格吸附步长（mm）
    double viewScale_ = 4.0;   // 初始 1mm = 4px
    QColor defaultColor_ = QColor(26, 29, 40);
    double defaultWidth_ = 1.0;
    LineStyle defaultLineStyle_ = LineStyle::Solid;
    DimensionStandard dimensionStandard_ = DimensionStandard::GBT;   // 当前标注标准（默认国标）
    bool chainLine_ = true;    // 直线默认连续绘制，右键结束
    LineDrawParams lp_;        // 直线绘制参数输入条（角度/长度、平行间隔/条数）
    double scaleRatio_ = 1.0;  // 绘图比例（图形:实际），1.0 = 1:1
    double ltscale_ = 1.0;     // 全局线型比例（LTSCALE），1.0 = 标准线素长度
    QString commandPrompt_;    // 当前分段命令提示文本
    QStringList commandOptions_;   // 当前分段命令可选项关键字（如 ["C/闭合","U/放弃"]）
    bool rightClickIdleRepeat_ = true;   // 空闲态右键=重复上一命令（AutoCAD 约定；会话态不入档）
    bool showLineWeight_ = false;  // 线宽显示开关（状态栏可点击，默认关）
    bool dynMode_ = true;          // 动态输入 DYN（光标跟随的距离/角度输入框，默认开）
    QString drawingPath_;      // 当前图纸文件路径（Xref 相对路径的解析基准；未存盘为空）
    std::vector<Layer> layers_;   // 图层表（默认含一层 "0"）
    QString currentLayer_ = QStringLiteral("0");   // 活动图层（绘制目标层），默认 "0"
    std::vector<BlockDef> blocks_;   // 块表（唯一一份定义，所有 BlockRef 共享）
    std::vector<TextStyle> textStyles_;   // 文字样式表（唯一数据源，图元经 styleName 引用）
    std::vector<dim::DimStyleEntry> dimStyles_;  // 标注样式表（一图多标准；图元经 dimStyleName 引用）
    QString currentDimStyle_ = QStringLiteral("Standard");  // 当前标注样式（dimStyleName 为空者用它）
    std::vector<FilterDef> savedFilters_; // 命名过滤器（FILTER 保存/复用；随图存档 + 入撤销栈）
    FilterDef activePickFilter_;          // FILTER 预选器（瞬态，作用于随后一次选择，用后即清）
    std::map<QString,QString> drawingProps_;   // 自定义图纸特性表（PROP 域数据源；随图存档）
    std::vector<PlotLayoutDef> layouts_;  // 布局表（多布局；随图存档 + 入撤销栈；切页签不单独入栈）
    QString currentLayout_;               // 当前布局名（空 = 模型空间）
    UnitsMeta  units_;                    // 单位设置（显示/输入口径；几何恒为 mm）
    LimitsMeta limits_;                   // 图纸界限（默认 A3，检查默认关）
    SheetSpec  sheetSpec_;                // 图纸幅面规格（hasSheetSpec_ 为真时有效；打印页面尺寸/方向数据源）
    bool       hasSheetSpec_ = false;     // 是否已设定幅面（空白图/外部导入未推断出 → false）
    snap::SnapConfig snapConfig_;         // 捕捉全局配置（随图存档 + 入撤销栈）
    snap::TrackConfig trackConfig_;       // 追踪全局配置（随图存档 + 入撤销栈）
    std::vector<Vec3> trackPoints_;       // 临时追踪点（TK，瞬态，不随图存档）

    // ---- 显式几何约束表 ----
    // 与「自动检测的隐式约束」互补：这里存用户显式声明的约束（重合/平行/…/尺寸），
    // 由 ConstraintSolver::solveAll 求解、detectConflicts 查冲突。随图存档顶层 JSON "constraints"。
    std::vector<ConstraintRecord> constraints_;
    std::map<QString,double> params_;   // 命名设计变量表（参数化建模，Request M 新增）
    Element clipboard_;
    bool hasClip_ = false;
    std::vector<UndoStep> undoStack_, redoStack_;
    bool undoSuppressed_ = false;   // UndoGuard 置起：为 true 时 pushMemento 直接返回
    int  undoDepthLimit_ = 100;     // 栈深上限（步）；setUndoDepthLimit 校验后写入
    bool deltaUndoEnabled_ = true;  // 撤销差量化开关（默认开）
    // ---- 出向差量 + 惰性封印（O(变更数) 撤销/重做的关键）----
    // push 时只捕获「改前」基态（elems 深拷贝一次，与旧实现每次 push 的
    // currentMemento() 同价）；栈顶条目的出向差量延迟到下一次 push / undo 时封印。
    // 封印前 pendingBase_ 即栈顶条目所代表状态的唯一凭据。
    Memento pendingBase_;           // 有效 iff unsealedStep_：栈顶条目的「改前」基态
    bool unsealedStep_ = false;     // 栈顶条目尚无出向差量
    void pushMemento(const QString& label);
    // 精确相等判定（快照去重用）：只有**逐字段全部相同**才返回 true。
    // 安全方向约定：无法证明相同即判为不同（照常入栈）——宁可少去重，绝不吞掉一个撤销步。
    static bool sameState(const Memento& a, const Memento& b);

    // ---- 撤销差量化内部原语（仅 delta 模式使用）----
    // 由当前模型字段构造一份完整 Memento（用于差量计算与正向回放基底）。
    Memento currentMemento() const;
    // 由当前模型字段构造 AuxState（aux 整段快照）。
    AuxState captureAux() const;
    // 把 AuxState 写回当前模型字段（差量回放/还原时复用）。
    void applyAux(const AuxState& a);
    // 把 Memento 的 aux 字段写入 a（Memento → AuxState）。
    static void mementoAuxTo(const Memento& m, AuxState& a);
    // 把 AuxState 写入 Memento 的 aux 字段（AuxState → Memento）。
    static void auxToMemento(const AuxState& a, Memento& m);
    // 从 undo/redo 栈的 [0, idx] 回放，重建第 idx 步的整图状态 Memento
    // （最近检查点 + 顺序回放出向差量链；检查点回放工具，undo/redo 热路径不依赖它）。
    Memento reconstructStep(int idx) const;
    // 封印栈顶条目的出向差量：diff(pendingBase_, 当前) 即上一步操作的变更集。
    // 返回 -1=无未封印条目；0=封印后为空（上一步是无操作，与旧 sameState 去重同义）；
    // 1=封印了真实变更集。
    int sealPendingStep();
    // 把 AuxState 就地写回模型字段 + 广播信号（信号集合与 restoreFromMemento 逐项一致，
    // 供差量 undo/redo 的就地还原复用；elems_ 由 applyElementDeltas 单独处理）。
    void restoreAuxInPlace(const AuxState& a);
    // 计算 before → after 的 DeltaRecord（元素差量 + aux 双快照）。
    static DeltaRecord makeDelta(const Memento& before, const Memento& after);
    // 把 DeltaRecord 正向应用到 base，产出「结果态」Memento。
    static Memento applyDeltaForward(const Memento& base, const DeltaRecord& d);
    // 用给定 Memento 还原模型（字段赋值 + 信号广播），undo/redo 共用，保证与全量模式逐字节一致。
    // 按值接收以便调用方 std::move，避免一次整图 vector 拷贝。
    void restoreFromMemento(Memento m);
    // 撤销/重做栈按深度上限收敛（双栈同限，且保留队首检查点，确保重建有锚点）。
    void trimUndoStackToLimit();
    void trimRedoStackToLimit();
    // 以下两个原写在 .cpp 匿名命名空间内的自由函数，因需访问私有 Memento/UndoStep，
    // 收敛为静态成员（等价能力，且不破坏封装）。
    static void auxToMementoStatic(const AuxState& a, Memento& m);
    static void trimStackToLimit(std::vector<UndoStep>& stk, int limit);

    std::vector<Element>::iterator find(int id);
    // 从种子图元沿「共享端点」连通性收集整条轮廓（selectContour/toggleContour 共用）
    std::vector<int> collectContourIds(int seedId) const;
    // 图元的世界包围盒：块引用取"展开后子图元"的并集，其余走 geo::elementBounds
    QRectF boundsOf(const Element& e) const;
};

} // namespace SketchCAD
