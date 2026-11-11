#pragma once
#include "ui/tools/ITool.h"
#include "core/weld/WeldSymbol.h"   // WeldSpec / buildWeldSymbol（ghost 生成）

namespace SketchCAD {

// ============================================================
// 焊缝符号（WELD）交互放置工具
// ============================================================
// 需求：焊缝图形添加至图纸后**实时跟随鼠标光标**移动；移动过程中**启用对象捕捉**
// （osnap）辅助精确定位；**单击左键**把图形固定在当前（经捕捉后的）光标位置。
//
// 交互流：
//   * 进入本工具前，WeldSymbolDialog 已把用户选定的 WeldSpec 暂存到
//     model->pendingWeldSpec（由 onActivate 读取），随后 setActiveTool("weld")。
//   * onActivate：setDrawing(true) 让 snap 标记随移动显示（drawForeground 门控 drawing_）。
//   * onMove：对光标世界坐标做 snapWorld（对象捕捉 + 正交/栅格），把 WeldSpec.anchor
//     更新为吸附点，重建焊缝图元作 ghost（虚线）跟随光标。
//   * onPress：再次 snapWorld 取最终吸附点，beginEdit + addElementsQuiet 落盘，
//     然后回到选择工具。
//   * onRightButton：取消放置（clearPreview + 清暂存 + 回到选择工具）。
//
// 复用既有封装：ctx.snapWorld（osnap）、ctx.showPreview/clearPreview（ghost）、
// model->beginEdit + addElementsQuiet（带撤销落盘）。不新增 ElementType。
class WeldTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;
    void onActivate(ToolContext& ctx) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    // 把 spec 的 anchor 设为 p 后生成全部图元，作为 ghost 预览：
    // 第一个图元（基准线）作主元素、其余作 guides（均为虚线）。
    void showGhost(ToolContext& ctx, const Vec3& p);
};

} // namespace SketchCAD
