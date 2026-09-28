#pragma once
#include "ui/tools/ITool.h"
#include "core/Element.h"
#include <vector>

namespace SketchCAD {

// ============================================================
// BoundaryTool —— 边界（BOUNDARY / BPOLY）绘制工具
// ============================================================
// 与 PolylineTool 的区别：边界**一定是闭合环**，且支持「外环 + 若干内孔」两段式拾取，
// 最终产出 ElementType::Boundary 图元——一个可被剖面填充模块直接消费的填充边界。
//
// 交互：
//   阶段 0（外环）：依次左键点击外环节点；点击「首点附近」自动闭合外环并进入阶段 1；
//                    右键直接闭合（无内孔）。< 3 点右键视为取消。
//   阶段 1（内孔）：依次左键点击一个内孔的节点；点击「该内孔首点附近」完成当前内孔，
//                    可继续拾取下一个内孔；右键结束并生成边界图元。
// 这样复杂腔体只需用户「画一圈外框、再画几圈内孔」，无需逐段描边、也不依赖自动边界追踪。
class BoundaryTool : public ITool {
public:
    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    int phase_ = 0;                                 // 0=外环, 1=内孔
    std::vector<Vec3> outer_;                       // 外环节点
    std::vector<Vec3> holePts_;                     // 当前正在拾取的内孔节点
    std::vector<std::vector<Vec3>> holes_;          // 已完成的内孔
    bool drawing_ = false;

    static constexpr double kCloseRadius = 3.0;     // 距首点 < 3mm 视为闭合

    void previewOuter(ToolContext& ctx, const Vec3& cur);
    void previewHole(ToolContext& ctx, const Vec3& cur);
    void finishOuter(ToolContext& ctx);
    void finishHole();
    void commit(ToolContext& ctx);
    void reset(ToolContext& ctx);
};

} // namespace SketchCAD
