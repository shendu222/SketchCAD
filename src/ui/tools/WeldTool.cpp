#include "core/I18n.h"
#include "ui/tools/WeldTool.h"
#include "core/SketchModel.h"   // beginEdit / addElementsQuiet / setActiveTool / pendingWeldSpec / log
#include <vector>

namespace SketchCAD {

QString WeldTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("焊缝符号(WELD)：移动光标实时预览，单击左键放置（右键取消）");
}

void WeldTool::onActivate(ToolContext& ctx) {
    // 让对象捕捉标记随移动显示：drawForeground 仅在 drawing_==true 时绘制 snap 标记。
    ctx.setDrawing(true);
}

void WeldTool::onDeactivate(ToolContext& ctx) {
    // 离开工具：清 ghost 与 drawing 态（正常放置/切换走 onPress/onRightButton 已清）。
    ctx.clearPreview();
    ctx.setDrawing(false);
}

void WeldTool::showGhost(ToolContext& ctx, const Vec3& p) {
    WeldSpec spec = ctx.model->pendingWeldSpec();
    spec.anchor = p;                                  // 以（经捕捉后的）光标点为膝点
    std::vector<Element> parts = buildWeldSymbol(spec);
    if (parts.empty()) { ctx.clearPreview(); return; }
    // 第一个图元（基准线）作主元素，其余作 guides；主元素用 preview 虚线笔、guides 用
    // th.guide 虚线，整体呈现为虚线 ghost 跟随光标。
    Element main = parts.front();
    std::vector<Element> guides(parts.begin() + 1, parts.end());
    ctx.showPreview(main, std::move(guides));
}

bool WeldTool::onMove(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    if (!ctx.model || !ctx.model->hasPendingWeldSpec()) return false;
    const Vec3 a = ctx.snapWorld(w);   // 对象捕捉 + 正交/栅格，返回最终落点
    showGhost(ctx, a);
    return true;
}

bool WeldTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    if (!ctx.model || !ctx.model->hasPendingWeldSpec()) return false;

    const Vec3 a = ctx.snapWorld(w);   // 单击取最终吸附点
    WeldSpec spec = ctx.model->pendingWeldSpec();
    spec.anchor = a;

    // 带撤销落盘（单一入口 beginEdit + addElementsQuiet）
    ctx.model->beginEdit(SketchCAD::I18n::t(QString::fromUtf8("放置焊缝符号")));
    ctx.model->addElementsQuiet(buildWeldSymbol(spec));
    ctx.model->log(SketchCAD::I18n::t(QString::fromUtf8("已放置焊缝符号：%1")).arg(weldTypeName(spec.type)),
                   QStringLiteral("info"));

    // 清理交互态：清 ghost、清暂存、回选择工具
    ctx.clearPreview();
    ctx.setDrawing(false);
    ctx.model->clearPendingWeldSpec();
    ctx.model->setActiveTool(QStringLiteral("select"));
    return true;
}

bool WeldTool::onRightButton(ToolContext& ctx) {
    // 右键取消放置：清 ghost 与暂存，回选择工具，不落盘。
    ctx.clearPreview();
    ctx.setDrawing(false);
    if (ctx.model) {
        ctx.model->clearPendingWeldSpec();
        ctx.model->setActiveTool(QStringLiteral("select"));
    }
    return true;
}

} // namespace SketchCAD
