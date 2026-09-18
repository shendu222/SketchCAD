#pragma once
#include "ui/tools/ITool.h"
#include <vector>

namespace SketchCAD {

// ============================================================
// FreehandLineTool —— 鼠标拖拽生成自由曲线
// ============================================================
// 交互：按住左键移动即连续采样，松开左键后按 Catmull-Rom 插值生成样条曲线。
class FreehandLineTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRelease(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    bool drawing_ = false;
    std::vector<Vec3> pts_;

    void preview(ToolContext& ctx);
    void finish(ToolContext& ctx);
};

} // namespace SketchCAD
