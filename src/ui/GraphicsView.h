#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QShortcut>
#include <QKeySequence>
#include <QAction>
#include <QList>
#include <QSet>
#include <memory>
#include <vector>
#include "core/Element.h"
#include "core/SketchModel.h"
#include "core/Vec.h"
#include "core/GripOps.h"
#include "core/Snap.h"
#include "core/LineModes.h"
#include "core/CircleModes.h"
#include "core/ArcModes.h"
#include "core/PolyModes.h"
#include "core/SplineModes.h"
#include "core/EllipseModes.h"
#include "core/ElementFactory.h"
#include "core/ToolRegistry.h"
#include "core/SpatialIndex.h"
#include <unordered_map>

// 全局类前向声明（⚠ 必须在 namespace 之外：放在 SketchCAD 内会声明出
// SketchCAD::QLineEdit 遮蔽 ::QLineEdit，导致 DYN 成员指针指向错误类型）。
class QLineEdit;

namespace SketchCAD {

// 前置声明：ToolContext 需要访问 GraphicsView 私有成员
struct ToolContext;
class ToolManager;

// 单个图元在场景中的可视化项。
// 同一个类同时承担「正式图元」与「橡皮筋预览」两种角色：预览时 preview=true 走虚线样式，
// 并可携带若干条辅助图元（guides，例如圆弧的定位圆与半径线）。
// 这样预览与成品共用同一套绘制代码，不会出现「预览长这样、画完变那样」的偏差。
class ElementItem : public QGraphicsItem {
public:
    Element el;
    bool highlight = false;
    bool preview = false;
    bool warn = false;        // 过约束/冲突高亮：被诊断为冲突的图元描红加粗
    bool showLineWeight = false;  // 线宽显示开关（对应 LWDISPLAY）：false 时一律细线
    // 当前绘图比例（图形:实物）。标注的数字必须写**实物尺寸**，而 el.pts 是图纸坐标，
    // 故绘制标注时要用它把比例除回去；由 GraphicsView::rebuild() 从模型注入。
    double dimScale = 1.0;
    // 块引用展开所需的块表（模型持有）。仅当 el.type == BlockRef 时被使用；
    // 为 nullptr 时块引用不绘制（渲染入口新图元/预览场景不会遇到块引用）。
    const SketchModel* model = nullptr;
    std::vector<Element> guides;

    explicit ElementItem(const Element& e) : el(e) { setData(0, e.id); }

    void setElement(const Element& e) { prepareGeometryChange(); el = e; update(); }
    void setGuides(std::vector<Element> g) { prepareGeometryChange(); guides = std::move(g); update(); }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w) override;

    // 把一个图元转成绘制路径（样条在这里被展开成三次贝塞尔）。
    // xlineClip：构造线(Xline)专用的裁剪矩形（世界坐标）——无限直线与其求交后只画出可见段，
    // 空矩形时回落到场景范围级大矩形；其他图元类型忽略此参数。
    static QPainterPath buildPath(const Element& e, const QRectF& xlineClip = QRectF());
    // 构造线：无限直线（pts[0]=通过点，rotation=方向角度）与矩形求交，返回交线段端点（不相交返回 false）
    static bool xlineClipToRect(const Element& e, const QRectF& r, Vec3& a, Vec3& b);
    // 当前视图可见区域（世界坐标）——构造线「按视图边界裁剪显示」用
    QRectF visibleWorldRect() const;
    // 图元的「几何」包围盒（世界坐标，**不含** cosmetic 描边余量），
    // 供「适配窗口 / 窗口缩放」精确取景——用 boundingRect() 会带上描边余量，导致取景略微不满。
    static QRectF geometryBounds(const Element& e);

private:
    double viewZoom() const;                 // 当前视图缩放，用于把像素余量换算成场景单位
    QRectF rawBounds(const Element& e) const;
    void   paintOne(QPainter* p, const Element& e) const;
    void   paintDimension(QPainter* p, const Element& e) const;  // GB/T 4458.4-2003
    void   paintMLeader(QPainter* p, const Element& e) const;    // 多重引线（箭头+基线+多行文字）
    void   paintFcf(QPainter* p, const Element& e) const;        // 形位公差框格（GB/T 1182 符号）
    void   paintText(QPainter* p, const Element& e) const;       // 文本图元（标题栏栏目等）
    void   paintTable(QPainter* p, const Element& e) const;      // 表格图元（明细表：网格线 + 单元格文字）
    void   paintHatch(QPainter* p, const Element& e) const;      // 剖面区域（GB/T 4457.5-2013）
    void   paintBoundary(QPainter* p, const Element& e) const;   // 交互式自定义边界（虚线轮廓）
    // 块引用：把引用按块表展开为世界坐标子图元（无模型/未定义块 → 空）
    std::vector<Element> expandedBlock() const;
    void   paintBlockRef(QPainter* p, const Element& e) const;
    QPainterPath blockPath(const Element& e) const;              // 展开后各子图元路径的并
    // 由图元推导画笔（颜色/线宽/线型；预览与高亮的覆盖一并处理）。
    // 抽取出来是为了让"块引用内的子图元"与"独立图元"走**同一套**笔参数，观感完全一致。
    QPen   penFor(const Element& e) const;
};

// 图形区：工作平面、网格、坐标系、平移缩放、绘制交互、选中
class GraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphicsView(SketchModel* m, QWidget* parent = nullptr);
    ~GraphicsView();
    // 适配窗口：按所有可见图元（绘制的图形）的世界包围盒精确缩放并充满整个绘图工作区域。
    // 坐标轴(drawAxes)与程序提示文字(HUD)是覆盖层、不计入 model_->elements()，天然被排除，
    // 缩放/平移时保持屏幕常量显示不变。
    void fitView();
    void fitToRect(const QRectF& world);   // 将给定世界坐标矩形以 0.92 边距适配居中到视口（新建图纸后整幅可见）
    // 当前视口中心的世界坐标（设计中心双击块时作为插入点）
    Vec3 viewCenterWorld() const;
    void zoomBy(double f);
    // ---- 窗口缩放（框选放大）：进入后左键拖拽框选目标区域，释放后该区域充满整个绘图工作区域 ----
    void setZoomBoxMode(bool on);
    bool zoomBoxMode() const { return zoomBoxMode_; }
    // 特性开关：空间索引拾取（关闭→全量扫描，双跑对照/异常回落用）
    void setUseIndex(bool on) { useIndex_ = on; }
    bool useIndex() const { return useIndex_; }
    // 基于真实几何距离的命中检测（替代包围盒命中）：
    // 在容差范围内返回距离点击点最近的图元 id，未命中为 -1。
    int hitTest(const QPoint& viewPos) const;
    // 选择循环：返回容差内全部命中图元 id（按距离升序，最上层在前），
    // 供 Ctrl+点击在位置重叠的多个对象间切换选取目标。
    std::vector<int> hitTestAll(const QPoint& viewPos) const;
    void cancelDrawing();                    // 供菜单「取消当前绘制」调用
    void injectPoint(const Vec3& p);         // 供命令行键盘输入坐标注入（分段式命令流程）
    void applyCommandOption(const QString& key);  // 绘制命令期命令行输入选项关键字（如 C/闭合、U/放弃）
    void showContextMenu(const QPoint& globalPos); // 选择/空闲态右键：按上下文弹出菜单

    // ---- 约束可视化对外接口（供主窗口接线，需求③④）----
    void refreshConstraintDiagnostics();      // 约束表变化后重算冲突集合、刷新高亮与告警
    void zoomToElement(int id);               // 视图缩放适配到指定图元（约束栏「定位」用）

    // ---- 中心点拾取模式（环形阵列等需要用户在视图中点击取点）----
    void setPointPickMode(bool on);           // 进入/退出「拾取一个世界坐标点」模式（光标变十字）
    bool pointPickMode() const { return pointPickMode_; }
    // 世界坐标下的捕捉/命中容差（= 8px / 缩放）：JOIN 连合等需要「真实拾取手感」
    // 的命令从 GUI 侧取此值；命令行/无头没有缩放概念，用 modify::kJoinDefaultTol。
    double worldTol() const { return PICK_TOL_PX / std::max(1e-6, zoom_); }

    // ---- 区域拾取模式（【修改>剖面填充】：点选封闭区域）----
    // 进入后：左键点击 → 发出 regionPickRequested(世界坐标)，不进入绘制/选择逻辑；
    //         鼠标移动 → 发出 regionHoverMoved，供上层实时预览"将要填充的区域"；
    //         Esc / 右键单击 → 退出并发出 regionPickCancelled；
    //         Enter → 发出 regionPickParamsRequested（供上层打开参数对话框）。
    void setRegionPickMode(bool on);
    bool regionPickMode() const { return regionPickMode_; }
    void setRegionHighlight(const std::vector<std::vector<Vec3>>& polys);  // 预览高亮（世界坐标）

    // ---- 标尺联动：暴露变换状态 ----
    double zoom() const { return zoom_; }
    double panX() const { return panX_; }
    double panY() const { return panY_; }

    // ---- Strategy Pattern：工具管理器 ----
    ToolManager* toolManager() const { return toolMgr_.get(); }
    void handleRightButtonViaTool();  // 供 ToolManager 回调

signals:
    void cursorMoved(double x, double y, double z);
    void viewTransformChanged();             // 缩放/平移变化时通知标尺刷新
    void canvasPointPicked(double x, double y);     // 中心点拾取模式：左键取点（世界坐标）
    void pointPickCancelled();                      // 中心点拾取模式：Esc/右键取消
    // ---- 区域拾取（剖面填充）----
    void regionPickRequested(double x, double y);   // 点选模式：左键点选（世界坐标）
    void regionHoverMoved(double x, double y);      // 点选模式：光标移动（实时预览识别结果）
    void regionPickCancelled();                     // 点选模式被 Esc / 右键取消
    void regionPickParamsRequested();               // 点选模式按 Enter：请求打开参数对话框
    void editTextRequested(int id);                 // 双击文字图元：请求打开多行文字编辑框
    void requestShowProperties();                   // 右键「特性」：请求主窗口弹出属性面板
public slots:
    void onModelChanged();
    void onSelectionChanged(int id);
    void onViewChanged();
protected:
    void drawBackground(QPainter* p, const QRectF& rect) override;
    void drawForeground(QPainter* p, const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;   // 视口大小变化时通知标尺刷新
    bool viewportEvent(QEvent* e) override;   // 调试：最底层抓取鼠标/右键/滚轮事件（QGraphicsView 鼠标事件走 viewportEvent 而非 event）
    void contextMenuEvent(QContextMenuEvent* e) override;  // 显式忽略右键菜单手势，确保右键只用于平移/结束绘制
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

    // ---- 绘制命令期快捷键重映射（E/T/M/C/X → 捕捉开关）----
    // 进入绘制命令时：保存并临时禁用与之冲突的全局快捷键，启用本组切换键；
    // 退出绘制命令时：禁用本组切换键并还原全局快捷键，使快捷键状态与绘制前完全一致。
    QList<QShortcut*> snapShortcuts_;          // E/T/M/C/X 五个切换快捷键（仅绘制中启用）
    struct SavedShortcut { QObject* obj; QKeySequence seq; };
    QList<SavedShortcut> savedShortcuts_;      // 进入绘制命令前被临时禁用的冲突快捷键快照
private:
    static constexpr double PICK_TOL_PX = 8.0;   // 拾取容差（屏幕像素）

    SketchModel* model_;
    QGraphicsScene* scene_;
    std::unique_ptr<ToolManager> toolMgr_;   // 策略模式工具管理器
    double zoom_ = 4.0;
    bool showLineWeight_ = false;   // 线宽显示开关（从模型同步，决定 penFor 是否按实际线宽）

    // ---- 动态输入 DYN（光标跟随的距离/角度输入框，viewport 覆盖层）----
    QWidget*   dynBox_ = nullptr;
    QLineEdit* dynLen_ = nullptr;   // 距离输入（mm）
    QLineEdit* dynAng_ = nullptr;   // 角度输入（度，世界角，逆时针为正）
    void ensureDynBox();                    // 惰性创建覆盖层
    void updateDynBox(const QPoint& vp);    // 按最近采集点+光标刷新位置与预填值
    void hideDynBox();                      // 隐藏（提交/取消/会话结束时）
    void commitDynInput();                  // Enter：上一点 + 极坐标(距离,角度) 注入为拾取点
    double panX_ = 0, panY_ = 0;
    bool panning_ = false, drawing_ = false, spaceDown_ = false;
    bool panRight_ = false;   // 本次平移由右键发起
    bool pointPickMode_ = false;   // 中心点拾取模式：下次左键点击直接返回世界坐标（不进入绘制/选择）
    // 区域拾取模式（剖面填充）：左键点击 → regionPickRequested；不进入绘制/选择逻辑。
    bool regionPickMode_ = false;
    std::vector<std::vector<Vec3>> regionHighlight_;   // 区域拾取预览高亮（世界坐标多边形）
    // 窗口缩放框选模式：进入后左键拖拽框选目标区域，释放后该区域精确缩放并充满整个绘图工作区域。
    bool zoomBoxMode_ = false;             // 是否处于「窗口缩放」框选模式
    bool zoomBoxActive_ = false;           // 当前是否正在拖拽框选矩形
    QPointF zoomBoxStart_;                 // 框选起点（世界坐标）
    QRectF zoomBoxRect_;                   // 当前框选矩形（世界坐标，未经归一化）
    bool rightMoved_ = false; // 右键按下后是否发生了拖动（用于区分「右键单击」与「右键拖拽平移」）
    QPoint lastPan_;
    std::vector<Vec3> pendingPts_;
    std::vector<int>  pickedIds_;   // 修剪/延伸/约束 的两步拾取缓存
    ElementItem* previewItem_ = nullptr;

    // 圆弧扫掠角跟踪：逐帧累加「解缠」后的角增量，
    // 使预览弧能连续跟随光标绕行，而不是在 ±180° 处突然翻面。
    bool   arcTracking_ = false;
    double arcLastAng_ = 0.0;
    double arcSweep_ = 0.0;
    bool   chainHinted_ = false;    // 连续直线提示只打一次，避免刷屏

    // ---- 夹点热态（单击夹点 → 5 种热操作：拉伸/移动/旋转/缩放/镜像）----
    // 单击夹点进入热态（默认拉伸），移动光标即动态预览（不落盘）；
    // Ctrl 循环切换模式（重置零参考光标）；按住拖拽释放 / 热态中再次单击 /
    // Enter = 确认落盘（beginEdit + applyChanges 一次撤销）；Esc = 取消。
    //Stretch 的重合联动引用在进入热态瞬间锁定（与原端点拖拽同语义）。
    bool gripHot_ = false;          // 热态中
    bool gripArmed_ = false;        // 首次单击（未拖拽）完成，等待确认单击
    bool gripDragMoved_ = false;    // 本次按住发生过拖拽位移
    grip::Mode gripMode_ = grip::Mode::Stretch;
    grip::Grips gripData_;          // 热态几何上下文（基准点/作用集/联动引用/零参考）
    QGraphicsPathItem* gripPreviewItem_ = nullptr;   // 变换预览（虚线，不进模型）
    bool hoverHandle_ = false;      // 当前光标下是否有可抓的控制点
    Vec3 hoverPos_;
    int  hoverLinked_ = 0;          // 该控制点上重合了几个图元

    // ---- 图元平移拖拽 ----
    // 点击已选中图元 → dragMovePending_ 置位，等首次移动才压撤销快照（避免单击空压）
    // 首次移动 → beginEdit + draggingEntities_=true，后续增量平移全部走 Quiet
    bool dragMovePending_ = false;   // 按下选中图元，等待移动确认
    bool draggingEntities_ = false; // 正在拖拽平移选中图元
    Vec3 dragMoveLast_;              // 上次拖拽位置（增量平移基准）

    // ---- 剪切 ----
    std::vector<int> trimBounds_;   // 显式指定的剪切边界（空 = 快速剪切，全场景为界）

    // ---- 框选（橡皮筋多选）----
    bool   marqueeActive_ = false;  // 是否正在拖拽框选矩形
    QPointF marqueeStart_;          // 框选起点（场景坐标）
    QRectF marquee_;                // 当前框选矩形（场景坐标，未经归一化）
    bool   deselectMarquee_ = false; // 框选结算语义：false=加选, true=取消（由选择工具 onActivate 设置）

    // ---- 直线绘制智能捕捉 ----
    snap::SnapResult snapNow_;       // 当前光标处命中的捕捉点（用于预览标记与拾取替换）
    QSet<int> conflictIds_;          // 过约束/冲突诊断中被标记的高亮图元集合（需求④）
    snap::SnapConfig snapCfg_;       // 当前绘制命令生效的捕捉开关（按 E/T/M/C 切换）
    snap::SnapConfig snapCfgBackup_; // 进入绘制命令前的全局基线快照，退出/清除覆盖后恢复至此
    bool snapOverrideActive_ = false;// 是否已激活「临时捕捉覆盖」（Shift+右键）
    snap::SnapConfig snapRestore_;   // 覆盖前的活动配置，清除覆盖时回退至此（"恢复原有配置"）
    // ---- 追踪（极轴 POLAR / 对象捕捉追踪 OTRACK / 临时追踪点 TK）----
    snap::TrackResult trackNow_;       // 当前追踪命中（用于落点替换与引导线绘制）
    std::vector<Vec3> otrackAnchors_;  // 已获取的对象追踪锚点（命令期瞬态，OSNAP 命中时累积）
    // ---- 输入约束（正交 ORTHO / 栅格吸附 SNAP）：单一状态源在 SketchModel，此处仅缓存本帧结果 ----
    snap::InputResult inputNow_;       // 本帧施加了哪些输入约束（供引导线与 HUD 反馈）
    bool drawToolActive_ = false;    // 当前是否处于「绘制类命令」（E/T/M/C 切换仅此状态下生效）
    bool keyboardInputActive_ = false; // 命令行键盘输入坐标时为 true，使 snapWorld 跳过捕捉
    Vec3 lastMouseWorld_;            // 最近一次鼠标世界坐标，供快捷键切换后即时刷新标记
    Vec3 snapWorld(const Vec3& w);   // 综合捕捉：返回应作为拾取点的世界坐标，并写入 snapNow_
    // ---- 捕捉开关（快捷键 E/T/M/C/X 重映射）----
    void onActiveToolChanged();          // 进入/退出绘制命令时备份与恢复 snapCfg_ 及全局快捷键
    bool isDrawTool(const QString& t) const;     // 判断是否为绘制类命令（委托 ToolRegistry）
    snap::SnapConfig snapDefaultFor(const QString& t) const;  // 各命令的默认捕捉开关
    void refreshSnapAtCursor();           // 切换后按当前光标重算捕捉标记并刷新预览
    void logSnapToggle(const QString& name, bool on);  // 记录捕捉开关切换日志
    void toggleSnap(bool snap::SnapConfig::* field, const QString& name);          // 取反某个捕捉开关
    // ---- 临时捕捉覆盖（Shift+右键菜单，单次有效）----
    void applySnapOverride(bool snap::SnapConfig::* field, const QString& name);    // 临时仅启用某一捕捉类型
    void clearSnapOverride();             // 清除覆盖，恢复覆盖前的活动配置
    void showSnapOverrideMenu(const QPoint& globalPos);  // 弹出覆盖类型选择菜单
    // ---- 追踪（极轴 / 对象捕捉追踪 / 临时追踪点 TK）----
    void addOtrackAnchor(const Vec3& p);  // 去重并入对象追踪锚点集（命令期累积，封顶防膨胀）
    void clearOtrackAnchors();            // 清空对象追踪锚点集
    void addTrackPointAtCursor();         // 在当前光标处放置临时追踪点（TK）
    void clearTrackPoints();              // 清空全部临时追踪点（TK）
    void enableSnapShortcuts(bool on);    // 统一启用/禁用本组 E/T/M/C/X 切换键
    void saveConflictingShortcuts();      // 进入绘制命令：快照并临时禁用冲突的全局快捷键（要求 1）
    void restoreConflictingShortcuts();   // 退出绘制命令：还原被临时禁用的全局快捷键（要求 3）

    void applyTransform();
    void rebuild();
    // ---- 约束可视化（需求②：约束图标+类型标注；需求④：过约束/冲突高亮）----
    void drawConstraintMarkers(QPainter* p);   // 遍历模型约束表，在图形视图上画图标并标注类型
    void drawConflictOverlay(QPainter* p);      // 顶部红色告警条（过约束/冲突时）
    // ---- 适配窗口 / 窗口缩放 的私有辅助 ----
    QRectF contentBounds() const;          // 所有可见图元的世界包围盒（不含坐标轴/提示文字覆盖层）
    void fitWorldRect(const QRectF& r, double margin);  // 将世界矩形以 margin 系数适配居中到视口
    void applyLineWeightToItems();    // 线宽显示开关变化时，刷新全部图元的线宽呈现并触发重绘

    // ---- 增量场景同步 + 均匀网格空间索引（渲染/拾取 O(N)→增量/对数级）----
    // id→场景项映射；rebuild 仍保留为全量兜底路径（结构性/批量变更回落）。
    std::unordered_map<int, ElementItem*> items_;     // 图元 id → 场景项（增量同步真相源）
    std::unordered_map<int, QRectF>       itemBounds_; // id → 上次登记索引的包围盒（差异扫描用）
    SpatialIndex spatial_;                             // 均匀网格空间索引（世界坐标，2^k 单元）
    QSet<int> prevSelected_;                           // 上次选区基线（增量高亮差量用）
    bool incSyncPending_ = false;                     // 本编辑批次已由增量信号处理（避免 modelChanged 再全量重建）
    bool useIndex_ = true;                            // 特性开关：索引拾取（关闭→全量扫描，双跑对照用）
    void syncIncremental();                            // modelChanged 无增量信号时：差异扫描做增/删/改
    void syncItemAddOrUpdate(const Element& e);        // 创建或更新单个场景项 + 索引
    void syncItemRemove(int id);                       // 销毁单个场景项 + 索引
    void onElementUpdated(int id);                     // elementUpdated(id) 信号：O(1) 增量更新项+索引
    void setItemHighlight(int id, bool on);            // 仅改高亮标记，不重建场景
    QRectF elementBoundsForIndex(const Element& e) const;  // 索引用包围盒（BlockRef 展开，对齐 model::boundsOf）
    static double pickDist(const Element& e, const Vec3& w, const SketchModel* m);  // 精确拾取距离（含 BlockRef 展开）
    void updatePreview(const Vec3& cur);
    void showPreview(const Element& tmp, std::vector<Element> guides = {});
    void clearPreview();
    void resetPending();
    void handlePress(const Vec3& w, QMouseEvent* e);
    void handleRightButton();
    void updateCommandPrompt();               // 根据当前工具和状态生成分段命令提示
    void finalizeLine();
    // ---- 直线子模式（菜单：2 Points / Horizontal / Vertical / ...） ----
    double lineAngleParam_ = 30.0;      // Angle / RelativeAngle 的默认角度
    bool isLineSubTool(const QString& t) const;   // 委托 ToolRegistry
    void handleLineSubTool(const Vec3& w, QMouseEvent* e);
    void addLineSegment(const Vec3& a, const Vec3& b);
    void finalizeSnake();
    // 栏选/圈围/圈交结算：按 activeTool 定模式，命中图元追加进当前选区
    void finalizeShapePick();
    int pickRefElement(const QPoint& vp, const std::vector<ElementType>& allowed) const;

    // ---- 构造线子模式（Xline：水平/竖直/角度/两点/角平分线/平行 共6种）----
    bool isXlineSubTool(const QString& t) const;   // 委托 ToolRegistry
    void handleXlineSubTool(const Vec3& w, QMouseEvent* e);
    void previewXline(const Vec3& cur);            // 构造线动态预览（无限线按角度/参考线方向）
    // 提交一条构造线并保持连续绘制（清空本轮采集但不退出命令，右键/Esc 退出）
    void commitXlineAndContinue(const Vec3& through, double angleDeg, const QString& desc);

    // ---- 子模式采集机械（DRY）：收参考图元 / 收几何拾取点（含分段式命令反馈日志） ----
    // 各族 handle*SubTool 的"收参考→收点"只在下面两处定义一次；各族算法性分叉
    //（compute 签名、Result 类型、自动完成谓词、参数 schema 及 Line 的 Snake/Angle/Parallel
    //  特例）仍留在各族方法内——避免为发散流程做过早抽象。
    bool collectRef(const QPoint& vp, const std::vector<ElementType>& allowed); // 成功返回 true，已 push+日志
    void collectSketchPoint(const Vec3& w, int need);                           // 记录第 need 个拾取点（含日志）

    void finalizeCircle();
    void finalizeCircle3P();
    void finalizeArc();
    void finalizeRect();
    void finalizePoly(bool spline);
    void createDefaultAt(const QString& type, const Vec3& w);
    void trackArcAngle(double curAng);

    // ---- 圆形子模式（菜单：圆心+点 / 两点 / 三点 / 相切... 共12种） ----
    bool isCircleSubTool(const QString& t) const;  // 委托 ToolRegistry
    void handleCircleSubTool(const Vec3& w, QMouseEvent* e);
    void finalizeCircleEx(const circle::Result& r, const QString& method);

    // ---- 圆弧子模式（菜单：三点弧 / 起终点+半径 / 相切 ... 共11种） ----
    bool isArcSubTool(const QString& t) const;     // 委托 ToolRegistry
    void handleArcSubTool(const Vec3& w, QMouseEvent* e);
    void finalizeArcEx(const arc::Result& r, const QString& method);

    // ---- 多边形子模式（菜单：正多边形 / 倒角矩形 / 圆角矩形 / 星形 共7种） ----
    bool isPolySubTool(const QString& t) const;    // 委托 ToolRegistry
    void handlePolySubTool(const Vec3& w, QMouseEvent* e);
    void finalizePolyEx(const polygon::Result& r, const QString& method);

    // ---- 样条子模式（菜单：抛物线 / 双曲线） ----
    bool isSplineSubTool(const QString& t) const;  // 委托 ToolRegistry
    void handleSplineSubTool(const Vec3& w, QMouseEvent* e);
    void finalizeSplineEx(const spline::Result& r, const QString& method);

    // ---- 椭圆子模式（菜单：1点 / 轴 / 焦点 / 4点 / 中心+3点 / 内切 共6种） ----
    bool isEllipseSubTool(const QString& t) const;  // 委托 ToolRegistry
    void handleEllipseSubTool(const Vec3& w, QMouseEvent* e);
    void finalizeEllipseEx(const ellipse::Result& r, const QString& method);

    // 子模式收尾模板（DRY）：加入模型 + 选中 + 重置待定状态，返回新图元 id。
    // 各 finalize*Ex 共用此稳定尾部，仅日志文本各不相同（在调用点拼接）。
    int commitElement(Element el);

    // ---- 编辑类工具 ----
    void handleDimension(const Vec3& w, DimensionType dt);
    void finalizeDimension(const Vec3& w);                        // 第2步：点击确认标注位置
    void previewDimension(const Vec3& cur);                       // 标注动态预览
    int  findNearestEdge(const Element& e, const Vec3& p) const;  // 多边形：找离点最近的边
    void edgeEndpoints(const Element& e, int idx, Vec3& a, Vec3& b) const;  // 取边端点
    // 对齐标注的尺寸线偏置位置：由边端点 A-B 与光标 cur 算出（直线/多边形边共用，DRY）
    bool alignedDimOffset(const Vec3& A, const Vec3& B, const Vec3& cur, Vec3& pos) const;
    void handleConstraint(int hitId);
    void handleTrimExtend(int hitId, const Vec3& w, bool extend);
    void handleTrimAny(int hitId, const Vec3& w, bool shift);   // 全图元剪切
    static DimensionType dimToolType(const QString& tool, bool& ok);  // 委托 ToolRegistry

    // ---- 标注两步流程 ----
    // 选【标注】命令 → 点击选图元(dimTargetId_) → 移动鼠标动态预览 → 点击确认位置
    // 圆图元：右键可在直径/半径间切换 (dimPreviewType_)
    // 多边形：dimEdgeIdx_ 记录选中的边索引
    bool dimPending_ = false;
    int  dimTargetId_ = -1;
    int  dimEdgeIdx_ = -1;
    DimensionType dimPreviewType_ = DimensionType::Distance;

    // ---- 控制点 ----
    void   refreshHover(const Vec3& w);
    // 夹点热态状态机（替代原「按下即拖拽端点」）
    bool   beginGripHot(const Vec3& w);       // 单击夹点进入热态；返回是否命中
    void   updateGripPreview(const Vec3& w);  // 按当前模式计算变更集并画虚线预览
    void   clearGripPreview();
    void   commitGrip();                      // 确认落盘（一次撤销）
    void   cancelGrip();                      // 取消（未落盘，直接清态）
    void   drawHandles(QPainter* p) const;

    // 选择循环状态：同锚点连续 Ctrl+点击 → 依次切换；其它点击/集合变化 → 重建
    void cycleSelection(const QPoint& vp, const std::vector<int>& ids);
    void breakCycle();
    bool cycleActive_ = false;
    QPoint cycleAnchor_;
    std::vector<int> cycleList_;
    int  cycleIdx_ = 0;
    // 图元在世界坐标系下的几何包围盒（用于框选命中判断）
    QRectF elementSceneBounds(const Element& e) const;

    // 几何计算统一使用 Vec3::angleDeg() 和 math::lineLineIntersect()（DRY）
    void drawAxes(QPainter* p) const;
    // Y 轴翻转后所有文字都会镜像，这里就地抵消缩放与翻转，按像素单位落字
    void drawPixelText(QPainter* p, const QPointF& scenePos, const QString& s,
                       const QColor& c, int px = 12, bool bold = false) const;

    // ---- Strategy Pattern：ToolContext 通过 friend 访问私有成员 ----
    friend struct ToolContext;
    friend class ToolManager;
};

} // namespace SketchCAD
