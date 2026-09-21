#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QShortcut>
#include <QKeySequence>
#include <QAction>
#include <QList>
#include <memory>
#include <vector>
#include "core/Element.h"
#include "core/SketchModel.h"
#include "core/Vec.h"
#include "core/Snap.h"
#include "core/LineModes.h"
#include "core/CircleModes.h"
#include "core/ArcModes.h"
#include "core/PolyModes.h"
#include "core/SplineModes.h"
#include "core/EllipseModes.h"
#include "core/ElementFactory.h"
#include "core/ToolRegistry.h"

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
    // 当前绘图比例（图形:实物）。标注的数字必须写**实物尺寸**，而 el.pts 是图纸坐标，
    // 故绘制标注时要用它把比例除回去；由 GraphicsView::rebuild() 从模型注入。
    double dimScale = 1.0;
    std::vector<Element> guides;

    explicit ElementItem(const Element& e) : el(e) { setData(0, e.id); }

    void setElement(const Element& e) { prepareGeometryChange(); el = e; update(); }
    void setGuides(std::vector<Element> g) { prepareGeometryChange(); guides = std::move(g); update(); }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w) override;

    // 把一个图元转成绘制路径（样条在这里被展开成三次贝塞尔）
    static QPainterPath buildPath(const Element& e);
    // 图元的「几何」包围盒（世界坐标，**不含** cosmetic 描边余量），
    // 供「适配窗口 / 窗口缩放」精确取景——用 boundingRect() 会带上描边余量，导致取景略微不满。
    static QRectF geometryBounds(const Element& e);

private:
    double viewZoom() const;                 // 当前视图缩放，用于把像素余量换算成场景单位
    QRectF rawBounds(const Element& e) const;
    void   paintOne(QPainter* p, const Element& e) const;
    void   paintDimension(QPainter* p, const Element& e) const;  // GB/T 4458.4-2003
    void   paintText(QPainter* p, const Element& e) const;       // 文本图元（标题栏栏目等）
    void   paintHatch(QPainter* p, const Element& e) const;      // 剖面区域（GB/T 4457.5-2013）
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
    void zoomBy(double f);
    // ---- 窗口缩放（框选放大）：进入后左键拖拽框选目标区域，释放后该区域充满整个绘图工作区域 ----
    void setZoomBoxMode(bool on);
    bool zoomBoxMode() const { return zoomBoxMode_; }
    void cancelDrawing();                    // 供菜单「取消当前绘制」调用
    void injectPoint(const Vec3& p);         // 供命令行键盘输入坐标注入（分段式命令流程）

    // ---- 中心点拾取模式（环形阵列等需要用户在视图中点击取点）----
    void setPointPickMode(bool on);           // 进入/退出「拾取一个世界坐标点」模式（光标变十字）
    bool pointPickMode() const { return pointPickMode_; }

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

    // ---- 端点捕捉与联动拖拽 ----
    // dragRefs_ 在按下的那一刻锁定：那时哪些控制点重合在一起，整段拖拽就一直带着谁走。
    // 若改成每帧现算，第一帧移动后它们就不再重合，联动会立刻断掉。
    bool draggingHandle_ = false;
    std::vector<SketchModel::HandleRef> dragRefs_;
    Vec3 dragOrigin_;
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
    snap::SnapConfig snapCfg_;       // 当前绘制命令生效的捕捉开关（按 E/T/M/C 切换）
    snap::SnapConfig snapCfgBackup_; // 进入绘制命令前的快照，退出时自动恢复
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
    void enableSnapShortcuts(bool on);    // 统一启用/禁用本组 E/T/M/C/X 切换键
    void saveConflictingShortcuts();      // 进入绘制命令：快照并临时禁用冲突的全局快捷键（要求 1）
    void restoreConflictingShortcuts();   // 退出绘制命令：还原被临时禁用的全局快捷键（要求 3）

    void applyTransform();
    void rebuild();
    // ---- 适配窗口 / 窗口缩放 的私有辅助 ----
    QRectF contentBounds() const;          // 所有可见图元的世界包围盒（不含坐标轴/提示文字覆盖层）
    void fitWorldRect(const QRectF& r, double margin);  // 将世界矩形以 margin 系数适配居中到视口
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
    int pickRefElement(const QPoint& vp, const std::vector<ElementType>& allowed) const;

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
    double worldTol() const { return PICK_TOL_PX / std::max(1e-6, zoom_); }
    void   refreshHover(const Vec3& w);
    bool   beginHandleDrag(const Vec3& w);
    void   drawHandles(QPainter* p) const;

    // 基于真实几何距离的命中检测（替代包围盒命中）：
    // 在容差范围内返回距离点击点最近的图元 id，未命中为 -1。
    int  hitTest(const QPoint& viewPos) const;
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
