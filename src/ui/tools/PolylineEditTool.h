#pragma once
#include "ui/tools/ITool.h"
#include "core/Element.h"
#include <vector>

namespace SketchCAD {

// ============================================================
// PolylineEditTool —— 折线编辑操作（单类多 id）
// ============================================================
// 一个工具实例覆盖 9 个编辑命令：
//   polyline_add_node           在段上插入节点
//   polyline_append_node        在端点（首或末）追加节点
//   polyline_delete_node        删除节点
//   polyline_delete_between     删除两节点之间的部分
//   polyline_trim_segments      修剪（删除整段）
//   polyline_equidistant        创建等距折线
//   polyline_from_segments      从若干已有 line/arc 合并为折线
//   polyline_arcs_to_chords     全部弧段转直线（清空 arcFlags）
//   polyline_change_segment_type 切换段类型（直线↔弧）
class PolylineEditTool : public ITool {
public:
    explicit PolylineEditTool(QString id);

    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    bool onRightButton(ToolContext& ctx) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    QString id_;

    // 两步操作（delete_between / change_segment_type）状态
    int firstId_  = -1;        // 已选中的折线 id
    int firstIdx_ = -1;        // 已选中的节点或段索引
    int firstSeg_ = -1;        // 段索引（仅 change_segment_type 用）

    // polyline_from_segments：累积已选段（Element id 列表）
    std::vector<int> fromIds_;

    // 通用：拾取 polyline 图元
    static const Element* pickPolyline(ToolContext& ctx, const QPoint& vp);
    // 通用：拾取任意可作为「from segments」来源的图元（Line/Arc/Polyline）
    static const Element* pickSegmentLike(ToolContext& ctx, const QPoint& vp);
    // 找 polyline 上离 p 最近的节点索引
    static int nearestNode(const Element& e, const Vec3& p);
    // 找 polyline 上离 p 最近的段索引（点到段投影距离最小）
    static int nearestSeg(const Element& e, const Vec3& p);

    // 各模式实现
    void addNode(ToolContext& ctx, const Element& e, const Vec3& p);
    void appendNode(ToolContext& ctx, const Element& e, const Vec3& p);
    void deleteNode(ToolContext& ctx, const Element& e, const Vec3& p);
    void deleteBetween(ToolContext& ctx, const Element& e, const Vec3& p);
    void trimSegments(ToolContext& ctx, const Element& e, const Vec3& p);
    void equidistant(ToolContext& ctx, const Element& e, const Vec3& p);
    void fromSegments(ToolContext& ctx, int pickedId);
    void arcsToChords(ToolContext& ctx, const Element& e);
    void changeSegmentType(ToolContext& ctx, const Element& e, const Vec3& p);
};

} // namespace SketchCAD
