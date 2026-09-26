#pragma once
#include "ui/tools/ITool.h"

namespace SketchCAD {

// ============================================================
// 具体工具类声明（Strategy Pattern 的 ConcreteStrategy）
// ============================================================
// 每个工具类封装一种（或一族）工具的全部交互逻辑：
//   - prompt()    → 生成命令行提示（取代 updateCommandPrompt 中的 if-else 分支）
//   - onPress()   → 处理左键按下（取代 handlePress 中的 if-else 分支）
//   - onRightButton() → 处理右键（取代 handleRightButton 中的 if-else 分支）

// ---- 选择 ----
class SelectTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    void onActivate(ToolContext& ctx) override;   // 进入选择工具：复位为「框选=加选」模式
};

// ---- 框选（窗口选择 / 窗口取消）----
// 两类工具的区别仅在于松手结算时是否从选区移除：由 GraphicsView 的 marqueeMode_ 决定。
class SelectWindowTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    void onActivate(ToolContext& ctx) override;  // 框选 = 加选
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};
class DeselectWindowTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    void onActivate(ToolContext& ctx) override;  // 框选 = 取消（deselect 模式）
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ---- 点选式高级选择（实体 / 轮廓 / 相交 / 图层）----
// 统一封装「点中图元后执行一种选择操作」，避免为每个动作单独建类。
class PickSelectTool : public ITool {
public:
    enum class Action {
        SelectEntity,        // 选择实体（= 点击选中）
        DeselectEntity,      // 取消选择实体
        SelectContour,       // 选择整条相连轮廓
        ToggleContour,       // (取消)选择轮廓：全选中则移除，否则选中
        SelectIntersected,   // 选择与种子相交的全部图元
        DeselectIntersected, // 取消与种子相交者
        SelectLayer,         // 选择同图层全部图元
        ToggleLayer          // (取消)选择图层：全选中则移除，否则选中
    };
    explicit PickSelectTool(Action a) : action_(a) {}
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
private:
    Action action_;
};

// ---- 删除（即时动作工具）----
class DeleteTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ---- 智能标注 ----
class SmartDimTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ---- 约束 ----
class ConstraintTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ---- 修剪 ----
class TrimTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ---- 延伸 ----
class ExtendTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

// ============================================================
// PointCollectTool —— 基本绘制工具（收集 N 个点后终结）
// ============================================================
// line / circle / circle3p / rect / arc / poly / spline 共用此模式：
//   1. 收集点直到满足所需数量
//   2. 调用对应的 finalize 方法生成图元
//   3. 部分工具在倒数第二个点后进入动态预览（如 arc 的扫掠角跟踪）
class PointCollectTool : public ITool {
public:
    PointCollectTool(QString id, QString displayName, int requiredPts,
                     bool isSpline = false, bool isPoly = false);

    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;

private:
    QString id_;
    QString name_;
    int required_;      // 所需点数（0 = 动态，如 poly/spline 由右键终结）
    bool isSpline_;
    bool isPoly_;

    void finalize(ToolContext& ctx);
};

// ============================================================
// SubToolHandler —— 子模式工具的通用委托
// ============================================================
// 直线/圆形/圆弧/多边形子模式共用同一个交互模式：
//   选参考图元 → 输入点 → 动态预览 → 点击确认
// 各子模式的 prompt 由对应的 Mode/ReqCount 元数据驱动，
// onPress / onRightButton 委托 GraphicsView 已有的 handle*SubTool 方法。
class SubToolHandler : public ITool {
public:
    enum class Family { Line, Xline, Circle, Arc, Poly, Spline, Ellipse };

    explicit SubToolHandler(Family family);

    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;

private:
    Family family_;
};

// ============================================================
// DimensionToolHandler —— 标注工具（两步流程）
// ============================================================
// 流程：选【标注】命令 → 点击选图元 → 移动鼠标动态预览 → 点击确认位置
// 圆图元：右键可在直径/半径间切换
class DimensionToolHandler : public ITool {
public:
    explicit DimensionToolHandler(DimensionType dt) : dt_(dt) {}

    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;

private:
    DimensionType dt_;
};

} // namespace SketchCAD
