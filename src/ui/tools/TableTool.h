#pragma once
#include "ui/tools/ITool.h"

namespace SketchCAD {

// 表格（TABLE）交互工具：点击画布放置左上角锚点 → 弹出表格编辑对话框 →
// 确认后按锚点与对话框参数创建 Table 图元。与 MTextTool 同一「两步放置」模式。
class TableTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
};

} // namespace SketchCAD
