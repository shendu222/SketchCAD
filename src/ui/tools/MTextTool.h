#pragma once
#include "ui/tools/ITool.h"

namespace SketchCAD {

// 多行文字（MTEXT）交互工具：点击画布放置锚点 → 弹出多行文字编辑对话框 →
// 确认后按锚点与对话框参数创建 MText 图元。两步即完成，无需拖拽。
class MTextTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

} // namespace SketchCAD
