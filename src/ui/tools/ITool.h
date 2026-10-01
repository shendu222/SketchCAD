#pragma once
#include <QString>
#include <QMouseEvent>
#include <vector>
#include "core/Vec.h"
#include "core/Element.h"

namespace SketchCAD {

class SketchModel;
class GraphicsView;

// ============================================================
// ToolContext —— 工具运行时上下文（Strategy Pattern）
// ============================================================
// 设计目的：
//   GraphicsView 原先是一个 2300+ 行的「上帝类」，handlePress / handleRightButton /
//   updateCommandPrompt 三个方法各自拖着一条 80~140 行的 if-else 链。
//   Strategy Pattern 把每种工具的行为封装为独立的 ITool 实例，
//   ToolContext 则是工具与视图之间唯一的通信桥梁——
//   工具通过 context 读写视图状态、调用视图方法，无需直接持有 GraphicsView 指针。
//
// DRY 收益：
//   - 消除 updateCommandPrompt() 中 140 行 if-else（每个工具自带 prompt()）
//   - 消除 handlePress() 中 80 行 if-else（ToolManager 按工具类型分派）
//   - 新增工具只需创建一个 ITool 子类并注册，无需修改 GraphicsView
struct ToolContext {
    SketchModel* model = nullptr;
    GraphicsView* view = nullptr;

    // ---- 状态查询 ----
    int  pendingPtCount() const;
    int  pickedIdCount() const;
    bool isChainHinted() const;
    bool isDimPending() const;
    int  dimTargetId() const;
    int  dimEdgeIdx() const;
    DimensionType dimPreviewType() const;
    QString activeTool() const;

    // ---- 状态操作 ----
    void addPendingPt(const Vec3& p);
    void setDrawing(bool d);
    void setArcTracking(bool on, double lastAng = 0.0);
    void beginArcSweep();  // 弧扫掠角跟踪：从已拾取的前两点计算起始角
    void clearPreview();
    void resetPending();
    void updatePreview(const Vec3& cur);

    // ---- 坐标与拾取 ----
    Vec3 snapWorld(const Vec3& w);
    int  hitTest(const QPoint& vp);
    // 容差内全部命中（按距离升序）：选择循环用
    std::vector<int> hitTestAll(const QPoint& vp);

    // ---- 预览 ----
    void showPreview(const Element& tmp, std::vector<Element> guides = {});

    // ---- 绘制终结（基本图元）----
    void finalizeLine();
    void finalizeCircle();
    void finalizeCircle3P();
    void finalizeArc();
    void finalizeRect();
    void finalizePoly(bool spline);
    void finalizeSnake();
    void finalizeShapePick();   // 栏选/圈围/圈交结算（fence/wp/cp 工具 Enter/双击调用）

    // ---- 子模式分派（委托 GraphicsView 已有方法）----
    void handleLineSubTool(const Vec3& w, QMouseEvent* e);
    void handleXlineSubTool(const Vec3& w, QMouseEvent* e);
    void handleCircleSubTool(const Vec3& w, QMouseEvent* e);
    void handleArcSubTool(const Vec3& w, QMouseEvent* e);
    void handlePolySubTool(const Vec3& w, QMouseEvent* e);
    void handleSplineSubTool(const Vec3& w, QMouseEvent* e);
    void handleEllipseSubTool(const Vec3& w, QMouseEvent* e);
    void handleDimension(const Vec3& w, DimensionType dt);
    void handleConstraint(int hitId);
    void handleTrimAny(int hitId, const Vec3& w, bool shift);
    void handleTrimExtend(int hitId, const Vec3& w, bool extend);

    // ---- 选择/拖拽 ----
    bool beginGripHot(const Vec3& w);   // 单击夹点进入热态（拉伸/移动/旋转/缩放/镜像）
    void cycleSelection(const QPoint& vp, const std::vector<int>& ids);  // Ctrl+点击循环
    void breakCycle();                  // 退出选择循环（普通选择路径调用）
    void setDragMovePending(bool on, const Vec3& w);
    void setMarquee(const QPointF& start);
    void setMarqueeMode(bool deselect);   // 框选结算语义：false=加选, true=取消

};

// ============================================================
// ITool —— 工具策略接口
// ============================================================
// 每种绘图/编辑工具实现此接口。ToolManager 持有当前活动工具的实例，
// 将鼠标事件与命令提示请求转发给它，取代 GraphicsView 中的 if-else 分派链。
class ITool {
public:
    virtual ~ITool() = default;

    /// 返回当前状态下的命令行提示文本
    virtual QString prompt(const ToolContext& ctx) const = 0;

    /// 左键按下：返回 true 表示已处理，false 表示走默认行为
    virtual bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) = 0;

    /// 鼠标移动（左键按下拖拽时）：返回 true 表示已处理
    virtual bool onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) { (void)ctx; (void)w; (void)e; return false; }

    /// 左键释放：返回 true 表示已处理
    virtual bool onRelease(ToolContext& ctx, const Vec3& w, QMouseEvent* e) { (void)ctx; (void)w; (void)e; return false; }

    /// 右键按下：返回 true 表示已处理
    virtual bool onRightButton(ToolContext& ctx) { (void)ctx; return false; }

    /// 工具被激活时调用（进入该工具）
    virtual void onActivate(ToolContext& ctx) { (void)ctx; }

    /// 工具被切换走时调用（离开该工具）
    virtual void onDeactivate(ToolContext& ctx) { (void)ctx; }
};

} // namespace SketchCAD
