// ============================================================
// ShapeSelectTool 实现
// ============================================================
#include "ShapeSelectTool.h"
#include "core/SketchModel.h"

namespace SketchCAD {

QString ShapeSelectTool::prompt(const ToolContext& ctx) const {
    const int n = ctx.pendingPtCount();
    const QString name = shape::modeName(mode_);
    if (n == 0) {
        return mode_ == shape::Mode::Fence
            ? QStringLiteral("栏选(FENCE)：依次点击栏线顶点，Enter/双击结算并加选，Esc 取消")
            : QString("%1：依次点击多边形顶点，Enter/双击结算并加选，Esc 取消").arg(name);
    }
    return QString("%1：已拾取 %2 个点（左键继续，Enter/双击结算，Esc 取消）")
            .arg(name).arg(n);
}

void ShapeSelectTool::onActivate(ToolContext& ctx) {
    (void)ctx;   // 无需额外初始化：顶点缓存在 pendingPts_，进入即清空态
}

bool ShapeSelectTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    // 采集点走统一捕捉管线（对象捕捉/追踪/正交/栅格），与绘制命令同手感
    ctx.addPendingPt(ctx.snapWorld(w));
    return true;
}

} // namespace SketchCAD
