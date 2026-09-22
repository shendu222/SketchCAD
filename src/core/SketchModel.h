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

namespace SketchCAD {

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
    void setShowGrid(bool b);
    bool showGrid() const { return showGrid_; }
    void setViewScale(double s);
    double viewScale() const { return viewScale_; }

    // ---- 绘图比例（GB/T 14690 技术制图 比例）----
    // 默认 1:1。修改比例会把已有图元（不含图框/标题栏）按内容包围盒中心等比缩放，
    // 使「绘图根据比例进行缩放」。新建图纸时由新建对话框设定初始比例。
    double scaleRatio() const { return scaleRatio_; }
    QString scaleLabel() const;                 // 如 "1:1" / "1:2" / "2:1"
    // 标注样式的**唯一构造入口**：已带上当前绘图比例。
    // 凡是要读标注数字的地方（渲染、导出、日志）都应从这里取样式再调 dim::build，
    // 否则会退回默认 1:1，把 1:2 图纸上的 100mm 显示成 50。
    dim::Style dimStyle() const;
    void   setScaleRatio(double r, bool allowUndo = true);
    // 只改比例值、不缩放图元。用于读档：存档里的几何已经是按该比例画好的成品，
    // 若再走 setScaleRatio 会被二次缩放。allowUndo=false 时纯赋值（不压撤销）。
    void   setScaleRatioValue(double r);

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

    // ---- 撤销/重做/复制粘贴 ----
    void undo();
    void redo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
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
    void gridToggled(bool show);
    void cursorMoved(double x, double y, double z);
    void defaultStyleChanged(const QColor& color, double width);
    void defaultLineStyleChanged(int style);
    void chainLineToggled(bool on);
    void lineParamsChanged();
    void dimensionStandardChanged(DimensionStandard s);
    void commandPromptChanged(const QString& prompt);
    void scaleChanged(double ratio);
    void layersChanged();                    // 图层表增删/改名/样式/可见性变化
    void currentLayerChanged(const QString& name);   // 活动图层切换（用于 HUD 实时显示 / 面板高亮）
    void blocksChanged();                    // 块表增删/改名/重定义变化（引用需重绘）

private:
    struct Memento { QString label; std::vector<Element> elems; int nextId; double scale = 1.0; std::vector<Layer> layers; std::vector<BlockDef> blocks; };
    std::vector<Element> elems_;
    int nextId_ = 1;
    int selectedId_ = -1;               // 主选择（属性面板编辑对象 / 约束等单对象操作）
    std::vector<int> selectedIds_;      // 当前选择集（框选 / 加选后可能含多个）
    QString activeTool_ = "select";
    Plane plane_ = Plane::XY;
    bool showGrid_ = true;
    double viewScale_ = 4.0;   // 初始 1mm = 4px
    QColor defaultColor_ = QColor(26, 29, 40);
    double defaultWidth_ = 1.0;
    LineStyle defaultLineStyle_ = LineStyle::Solid;
    DimensionStandard dimensionStandard_ = DimensionStandard::GBT;   // 当前标注标准（默认国标）
    bool chainLine_ = true;    // 直线默认连续绘制，右键结束
    LineDrawParams lp_;        // 直线绘制参数输入条（角度/长度、平行间隔/条数）
    double scaleRatio_ = 1.0;  // 绘图比例（图形:实际），1.0 = 1:1
    QString commandPrompt_;    // 当前分段命令提示文本
    std::vector<Layer> layers_;   // 图层表（默认含一层 "0"）
    QString currentLayer_ = QStringLiteral("0");   // 活动图层（绘制目标层），默认 "0"
    std::vector<BlockDef> blocks_;   // 块表（唯一一份定义，所有 BlockRef 共享）
    Element clipboard_;
    bool hasClip_ = false;
    std::vector<Memento> undoStack_, redoStack_;
    void pushMemento(const QString& label);
    std::vector<Element>::iterator find(int id);
    // 从种子图元沿「共享端点」连通性收集整条轮廓（selectContour/toggleContour 共用）
    std::vector<int> collectContourIds(int seedId) const;
    // 图元的世界包围盒：块引用取"展开后子图元"的并集，其余走 geo::elementBounds
    QRectF boundsOf(const Element& e) const;
};

} // namespace SketchCAD
