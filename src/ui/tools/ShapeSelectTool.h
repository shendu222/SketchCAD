#pragma once
#include "ITool.h"
#include "core/ShapeSelect.h"

namespace SketchCAD {

// ============================================================
// ShapeSelectTool —— 栏选(Fence) / 圈围(WP) / 圈交(CP) 三合一选择工具
// ------------------------------------------------------------
// 交互与连续直线绘制同风格：左键采集顶点（带智能捕捉/追踪/正交约束），
// Enter / 双击结算，Esc / 右键取消；结算时把命中图元**追加**到当前选区
// （addToSelection，不破坏已有选择）。
// 一个类承载三种模式（构造注入 shape::Mode），注册三个工具 id：
//   fence / wp / cp（与 AutoCAD 命令名对齐）。
// ============================================================
class ShapeSelectTool : public ITool {
public:
    explicit ShapeSelectTool(shape::Mode m) : mode_(m) {}

    QString prompt(const ToolContext& ctx) const override;
    void onActivate(ToolContext& ctx) override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;

private:
    shape::Mode mode_;
};

} // namespace SketchCAD
