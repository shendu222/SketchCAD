#pragma once
#include "ui/tools/ITool.h"
#include "core/Element.h"
#include <vector>

namespace SketchCAD {

// ============================================================
// SplineEditTool —— 样条曲线编辑操作
// ============================================================
// 一个工具处理图中全部样条编辑命令：
//   spline_from_polyline   折线/多边形 → 样条
//   spline_append_pt       在端点追加控制点
//   spline_insert_pts      在段上插入控制点
//   spline_remove_pts      删除控制点
//   spline_remove_between  删除两控制点之间的部分
//   spline_explode         样条 → 折线
class SplineEditTool : public ITool {
public:
    explicit SplineEditTool(QString id);

    QString prompt(const ToolContext& ctx) const override;
    bool onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) override;
    void onDeactivate(ToolContext& ctx) override;

private:
    QString id_;
    // 用于两步操作（remove_between）的暂存
    int firstId_ = -1;
    int firstIdx_ = -1;

    // 通用：点击命中 spline
    static const Element* pickSpline(ToolContext& ctx, const QPoint& vp);
    // 通用：点击命中可转换为样条的折线类图元
    static const Element* pickPolylineLike(ToolContext& ctx, const QPoint& vp);
    // 找 spline 上距离 p 最近的控制点索引
    static int nearestControlPoint(const Element& e, const Vec3& p);

    void fromPolyline(ToolContext& ctx, const Element& src);
    void appendPoint(ToolContext& ctx, const Element& e, const Vec3& p);
    void insertPoint(ToolContext& ctx, const Element& e, const Vec3& p);
    void removePoint(ToolContext& ctx, const Element& e, const Vec3& p);
    void removeBetween(ToolContext& ctx, const Element& e, const Vec3& p);
    void explode(ToolContext& ctx, const Element& e);
};

} // namespace SketchCAD
