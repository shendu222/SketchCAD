#pragma once
#include "ui/tools/ITool.h"
#include "core/Element.h"
#include <vector>

namespace SketchCAD {

// ============================================================
// PolylineTool —— 折线绘制工具（按点序列生成 ElementType::Polyline）
// ============================================================
// 交互：
//   - 左键依次点击节点，节点被收集到 internal buffer 并实时预览。
//   - 当新点击距首点 < 3mm（世界）时自动闭合并结束。
//   - 右键结束：点数 ≥ 2 时生成开放折线；< 2 视为取消。
class PolylineTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    std::vector<Vec3> pts_;        // 已拾取的节点（世界坐标）
    bool drawing_ = false;

    static constexpr double kCloseRadius = 3.0;   // 距首点 < 3mm 时自动闭合

    void preview(ToolContext& ctx, const Vec3& cur);
    void finish(ToolContext& ctx, bool closed);
};

} // namespace SketchCAD
