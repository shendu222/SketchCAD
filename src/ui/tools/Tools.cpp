// ============================================================
// 具体工具实现（Strategy Pattern 的 ConcreteStrategy）
// ============================================================
// 每个工具类的 prompt() / onPress() / onRightButton() 取代了
// GraphicsView 中对应的 if-else 分支。逻辑保持完全一致，
// 仅从过程式分派改为多态分派。
#include "ui/tools/Tools.h"
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/ToolRegistry.h"
#include "core/LineModes.h"
#include "core/CircleModes.h"
#include "core/ArcModes.h"
#include "core/PolyModes.h"
#include "core/SplineModes.h"
#include "core/EllipseModes.h"

namespace SketchCAD {

// ============================================================
// SelectTool
// ============================================================
QString SelectTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("选择模式 — 点击图元选择, 或输入命令名激活工具");
}

bool SelectTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    const QPoint vp = e->position().toPoint();
    // 优先抓端点：捕捉到就进入联动拖拽
    if (ctx.beginHandleDrag(w)) return true;
    const int hit = ctx.hitTest(vp);
    if (hit >= 0) {
        // Ctrl / Shift 点击：加选或减选（多选的两种标准修饰键，语义一致 = 切换）
        if (e->modifiers().testFlag(Qt::ControlModifier) ||
            e->modifiers().testFlag(Qt::ShiftModifier)) {
            ctx.model->toggleSelection(hit);
        } else if (ctx.model->isSelected(hit)) {
            // 点击已选中图元 → 进入平移预备态
            ctx.setDragMovePending(true, w);
        } else {
            ctx.model->setSelected(hit);
        }
        return true;
    }
    // 空白处 → 框选
    ctx.setMarquee(QPointF(w.x, w.y));
    return true;
}

void SelectTool::onActivate(ToolContext& ctx) {
    // 进入标准选择工具：确保框选结算为「加选」语义，而非「取消」。
    ctx.setMarqueeMode(false);
}

// ============================================================
// SelectWindowTool / DeselectWindowTool
// ============================================================
// 两者都只负责「按下即起框」；最终加选还是取消，由 GraphicsView 的
// marqueeMode_ 在松手结算时决定（onActivate 设置该模式）。
QString SelectWindowTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("窗口选择 — 拖拽出矩形框选内部图元 (Ctrl 加选)");
}
void SelectWindowTool::onActivate(ToolContext& ctx) {
    ctx.setMarqueeMode(false);   // 框选 = 加选
}
bool SelectWindowTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    ctx.setMarquee(QPointF(w.x, w.y));
    return true;
}

QString DeselectWindowTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("窗口取消 — 拖拽出矩形取消框内图元的选中");
}
void DeselectWindowTool::onActivate(ToolContext& ctx) {
    ctx.setMarqueeMode(true);    // 框选 = 取消
}
bool DeselectWindowTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    ctx.setMarquee(QPointF(w.x, w.y));
    return true;
}

// ============================================================
// PickSelectTool —— 点选式高级选择
// ============================================================
QString PickSelectTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    switch (action_) {
    case Action::SelectEntity:        return QStringLiteral("选择实体 — 点击图元将其选中");
    case Action::DeselectEntity:      return QStringLiteral("取消选择实体 — 点击图元取消其选中");
    case Action::SelectContour:       return QStringLiteral("选择轮廓 — 点击一条边，选中整条相连轮廓");
    case Action::ToggleContour:       return QStringLiteral("(取消)选择轮廓 — 点击一条边：轮廓已全选中则整条取消，否则整条选中");
    case Action::SelectIntersected:   return QStringLiteral("选择相交 — 点击图元，选中所有与之相交的图元");
    case Action::DeselectIntersected: return QStringLiteral("取消相交 — 点击图元，取消与之相交者的选中");
    case Action::SelectLayer:         return QStringLiteral("选择图层 — 点击图元，选中同图层全部图元");
    case Action::ToggleLayer:         return QStringLiteral("(取消)选择图层 — 点击图元：同层已全选中则整层取消，否则整层选中");
    }
    return QStringLiteral("选择 — 请点击图元");
}

bool PickSelectTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    const int hit = ctx.hitTest(e->position().toPoint());
    if (hit < 0) {
        ctx.model->log(QStringLiteral("未命中任何图元"), "warn");
        return true;
    }
    switch (action_) {
    case Action::SelectEntity:        ctx.model->selectEntity(hit); break;
    case Action::DeselectEntity:      ctx.model->deselectEntity(hit); break;
    case Action::SelectContour:       ctx.model->selectContour(hit); break;
    case Action::ToggleContour:       ctx.model->toggleContour(hit); break;
    case Action::SelectIntersected:   ctx.model->selectIntersected(hit); break;
    case Action::DeselectIntersected: ctx.model->deselectIntersected(hit); break;
    case Action::SelectLayer:         ctx.model->selectLayerOf(hit); break;
    case Action::ToggleLayer:         ctx.model->toggleLayerOf(hit); break;
    }
    return true;
}


// ============================================================
// DeleteTool
// ============================================================
QString DeleteTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("删除 — 请选择要删除的图元:");
}

bool DeleteTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)w;
    const int id = ctx.hitTest(e->position().toPoint());
    if (id >= 0) ctx.model->removeElement(id);
    return true;
}

// ============================================================
// SmartDimTool
// ============================================================
QString SmartDimTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("智能标注 — 请先选中一条直线或一个圆");
}

bool SmartDimTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)w; (void)e;
    if (ctx.model->smartDimension() >= 0) {
        ctx.resetPending();
        return true;
    }
    ctx.model->log(QStringLiteral("智能标注：请先选中一条直线或一个圆"), "warn");
    return true;
}

// ============================================================
// ConstraintTool
// ============================================================
QString ConstraintTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("约束 — 请选择图元:");
}

bool ConstraintTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)w;
    ctx.handleConstraint(ctx.hitTest(e->position().toPoint()));
    return true;
}

// ============================================================
// TrimTool
// ============================================================
QString TrimTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("剪切 — 请选择要处理的图元:");
}

bool TrimTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    ctx.handleTrimAny(ctx.hitTest(e->position().toPoint()), w,
                      e->modifiers().testFlag(Qt::ShiftModifier));
    return true;
}

// ============================================================
// ExtendTool
// ============================================================
QString ExtendTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("延伸 — 请选择要处理的图元:");
}

bool ExtendTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    ctx.handleTrimExtend(ctx.hitTest(e->position().toPoint()), w, true);
    return true;
}

// ============================================================
// PointCollectTool —— 基本绘制工具
// ============================================================
PointCollectTool::PointCollectTool(QString id, QString displayName, int requiredPts,
                                   bool isSpline, bool isPoly)
    : id_(std::move(id)), name_(std::move(displayName)),
      required_(requiredPts), isSpline_(isSpline), isPoly_(isPoly) {}

QString PointCollectTool::prompt(const ToolContext& ctx) const {
    const int nPts = ctx.pendingPtCount();

    if (id_ == "line" || id_ == "line_2pt") {
        if (ctx.isChainHinted() && nPts == 1)
            return QStringLiteral("连续直线 — 请输入下一点 (右键结束):");
        return (nPts == 0) ? QStringLiteral("直线 — 请输入起点 (鼠标点击或输入 x,y):")
                           : QStringLiteral("直线 — 请输入终点 (鼠标点击或输入 x,y):");
    }
    if (id_ == "line_h")
        return (nPts == 0) ? QStringLiteral("水平线 — 请输入起点:")
                           : QStringLiteral("水平线 — 请输入终点:");
    if (id_ == "line_v")
        return (nPts == 0) ? QStringLiteral("竖直线 — 请输入起点:")
                           : QStringLiteral("竖直线 — 请输入终点:");
    if (id_ == "line_angle")
        return QStringLiteral("角度直线 — 请输入起点 (参数: 角度=%1° 长度=%2mm):")
                   .arg(ctx.model->lineParams().angleDeg, 0, 'f', 1)
                   .arg(ctx.model->lineParams().length, 0, 'f', 1);
    if (id_ == "circle")
        return (nPts == 0) ? QStringLiteral("圆 — 请输入圆心:")
                           : QStringLiteral("圆 — 请输入半径点:");
    if (id_ == "circle3p") {
        const QStringList labels = {QStringLiteral("第一点"), QStringLiteral("第二点"), QStringLiteral("第三点")};
        if (nPts < 3) return QStringLiteral("三点圆 — 请输入%1:").arg(labels[nPts]);
    }
    if (id_ == "rect")
        return (nPts == 0) ? QStringLiteral("矩形 — 请输入第一角点:")
                           : QStringLiteral("矩形 — 请输入对角点:");
    if (id_ == "arc") {
        const QStringList labels = {QStringLiteral("起点"), QStringLiteral("第二点"), QStringLiteral("终点")};
        if (nPts < 3) return QStringLiteral("圆弧 — 请输入%1:").arg(labels[nPts]);
    }
    if (id_ == "poly")
        return QStringLiteral("多边形 — 请输入顶点%1 (右键结束):").arg(nPts + 1);
    if (id_ == "spline")
        return QStringLiteral("样条 — 请输入控制点%1 (右键结束):").arg(nPts + 1);
    return name_ + QStringLiteral(" — 请操作:");
}

bool PointCollectTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    ctx.setDrawing(true);
    ctx.addPendingPt(ctx.snapWorld(w));
    ctx.model->log(QString("已拾取点 %1: (%2, %3)")
                       .arg(ctx.pendingPtCount())
                       .arg(w.x, 0, 'f', 2)
                       .arg(w.y, 0, 'f', 2));

    const int n = ctx.pendingPtCount();
    if (required_ > 0 && n >= required_) {
        finalize(ctx);
    } else if (id_ == "arc" && n == 2) {
        // 半径与起始角已定，从这一刻开始累计扫掠角
        ctx.beginArcSweep();
        ctx.updatePreview(w);
    } else {
        ctx.updatePreview(w);
    }
    return true;
}

bool PointCollectTool::onRightButton(ToolContext& ctx) {
    if ((id_ == "poly" || id_ == "spline") && ctx.pendingPtCount() >= 2) {
        ctx.finalizePoly(isSpline_);
        return true;
    }
    return false;
}

void PointCollectTool::finalize(ToolContext& ctx) {
    if (id_ == "line" || id_ == "line_2pt")       ctx.finalizeLine();
    else if (id_ == "circle")                      ctx.finalizeCircle();
    else if (id_ == "circle3p")                    ctx.finalizeCircle3P();
    else if (id_ == "rect")                        ctx.finalizeRect();
    else if (id_ == "arc")                         ctx.finalizeArc();
}

// ============================================================
// SubToolHandler —— 直线/圆形/圆弧/多边形子模式
// ============================================================
SubToolHandler::SubToolHandler(Family family) : family_(family) {}

namespace {
// 子模式提示阶段机（模板方法）：各族子模式共用「选参考→收点→确认」三阶段提示，
// modeName 一律取 ToolSpec.displayName（单一真源），消除 Circle/Arc/Poly/Ellipse
// 中近似重复的 find(t)+分支 逻辑。
QString phasedPrompt(const QString& toolId, int nRefs, int nPts,
                     int reqRefs, int reqPts) {
    const ToolSpec* spec = ToolRegistry::instance().find(toolId);
    const QString modeName = spec ? spec->displayName : toolId;
    if (reqRefs > 0 && nRefs < reqRefs)
        return QString(QStringLiteral("%1 — 请选择第%2个参考图元 (共需%3个):"))
                   .arg(modeName).arg(nRefs + 1).arg(reqRefs);
    if (nPts < reqPts)
        return QString(QStringLiteral("%1 — 请输入第%2个点 (共需%3个):"))
                   .arg(modeName).arg(nPts + 1).arg(reqPts);
    return QStringLiteral("%1 — 移动鼠标选择, 点击确认:").arg(modeName);
}

// 直线族提示：按「选参考 → 收点/方向 → 确认」的阶段性文案表驱动。
// 文案与重构前的 updateCommandPrompt 逐字一致，避免手写 if 链重复。
QString linePrompt(const QString& toolId, int nRefs, int nPts) {
    const line::Mode m = line::modeFromString(toolId);
    switch (m) {
    case line::Mode::Parallel:
        return (nRefs == 0) ? QStringLiteral("平行线 — 请选择参考直线/矩形:")
                            : QStringLiteral("平行线 — 请点击确定偏移方向:");
    case line::Mode::ParallelThroughPoint:
        return (nRefs == 0) ? QStringLiteral("过点平行 — 请选择参考直线/矩形:")
             : (nPts == 0) ? QStringLiteral("过点平行 — 请输入经过的点:")
                           : QString();
    case line::Mode::Orthogonal:
        return (nRefs == 0) ? QStringLiteral("垂线 — 请选择参考直线/矩形:")
             : (nPts == 0) ? QStringLiteral("垂线 — 请输入经过的点:")
                           : QString();
    case line::Mode::OrthogonalFromLine:
        return (nRefs == 0) ? QStringLiteral("线上垂线 — 请选择参考直线/矩形:")
             : (nPts == 0) ? QStringLiteral("线上垂线 — 请在参考线上拾取一点:")
                           : QString();
    case line::Mode::FromPointToLine:
        return (nRefs == 0) ? QStringLiteral("点到线垂线 — 请选择参考直线/矩形:")
             : (nPts == 0) ? QStringLiteral("点到线垂线 — 请输入外部点:")
                           : QString();
    case line::Mode::RelativeAngle:
    case line::Mode::AngleFromLine:
        return (nRefs == 0) ? QStringLiteral("相对角度线 — 请选择参考直线/矩形:")
             : (nPts == 0) ? QStringLiteral("相对角度线 — 请输入起点:")
                           : QStringLiteral("相对角度线 — 请输入终点:");
    case line::Mode::TangentPC:
        return (nRefs == 0) ? QStringLiteral("点切线 — 请选择圆或圆弧:")
             : (nPts == 0) ? QStringLiteral("点切线 — 请输入外部点:")
                           : QString();
    case line::Mode::TangentCC:
        return (nRefs == 0) ? QStringLiteral("公切线 — 请选择第一个圆:")
             : (nRefs == 1) ? QStringLiteral("公切线 — 请选择第二个圆:")
                            : QStringLiteral("公切线 — 移动鼠标选择切线, 点击确认:");
    case line::Mode::TangentOrthogonal:
        return (nRefs == 0) ? QStringLiteral("正交切线 — 请选择圆或圆弧:")
             : (nRefs == 1) ? QStringLiteral("正交切线 — 请选择参考直线/矩形:")
                            : QStringLiteral("正交切线 — 移动鼠标选择, 点击确认:");
    case line::Mode::Bisector:
        return (nRefs == 0) ? QStringLiteral("角平分线 — 请选择第一条直线:")
             : (nRefs == 1) ? QStringLiteral("角平分线 — 请选择第二条直线:")
             : (nPts == 0) ? QStringLiteral("角平分线 — 请输入长度参考点:")
                           : QString();
    case line::Mode::Snake:
    case line::Mode::SnakeX:
    case line::Mode::SnakeY:
        return (nPts == 0) ? QStringLiteral("正交折线 — 请输入起点:")
                           : QStringLiteral("正交折线 — 请输入下一点 (右键结束):");
    default:
        return toolId + QStringLiteral(" — 请操作:");
    }
}

// ============================================================
// 统一提示阶段机：所有子模式（Line/Circle/Arc/Poly/Spline/Ellipse）
// 共用单一入口。每种族的阶段提示由此数据驱动分派：
//   - Circle/Arc/Poly/Ellipse → 通用 phasedPrompt（选参考→收点→确认）
//   - Line  → 专有的分阶段文案表 linePrompt（按 refs/pts 逐阶段）
//   - Spline→ 专有分支（右键结束 / 焦点式抛物线）
// 顶层坐标全部收敛于此，SubToolHandler::prompt 只做一次透传。

// ---- Spline 阶段提示数据表：按「阶段模式」驱动，替代硬编码 if-else ----
enum class SplineStage { RightClick, FocusDirectrix, ForwardPoint };
struct SplineHintSpec {
    SplineStage stage;
    int total;      // 所需点总数（仅在 ForwardPoint 用）
};

// spline 各模式的阶段提示规格（文案在 splineStagePrompt 中逐字给出）。
SplineHintSpec splineHint(const spline::Mode sm) {
    switch (sm) {
        case spline::Mode::Free:
        case spline::Mode::ThroughPoints:
            return {SplineStage::RightClick, 0};
        case spline::Mode::ParabolaFocusDirectrix:
            return {SplineStage::FocusDirectrix, 3};
        case spline::Mode::Parabola:
        case spline::Mode::Hyperbola:
            return {SplineStage::ForwardPoint, 3};
        case spline::Mode::Parabola4P:
            return {SplineStage::ForwardPoint, 4};
    }
    return {SplineStage::ForwardPoint, 3};
}

QString splineStagePrompt(const QString& modeName, const SplineHintSpec& spec, int nPts) {
    switch (spec.stage) {
        case SplineStage::RightClick:
            return (nPts == 0) ? QStringLiteral("%1 — 请输入控制点 (右键结束):").arg(modeName)
                               : QStringLiteral("%1 — 请输入下一个控制点 (右键结束):").arg(modeName);
        case SplineStage::FocusDirectrix:
            if (nPts == 0)
                return QStringLiteral("%1 — 请输入焦点:").arg(modeName);
            if (nPts < spec.total)
                return QStringLiteral("%1 — 请输入准线上第%2个点:").arg(modeName).arg(nPts + 1);
            return QStringLiteral("%1 — 移动鼠标选择, 点击确认:").arg(modeName);
        case SplineStage::ForwardPoint:
            if (nPts >= spec.total)
                return QStringLiteral("%1 — 移动鼠标选择, 点击确认:").arg(modeName);
            return QString(QStringLiteral("%1 — 请输入第%2个点 (共需%3个):"))
                       .arg(modeName).arg(nPts + 1).arg(spec.total);
    }
    return modeName + QStringLiteral(" — 请操作:");
}

QString subModePrompt(const QString& t, SubToolHandler::Family family,
                      int nRefs, int nPts) {
    switch (family) {
    case SubToolHandler::Family::Line:
        return linePrompt(t, nRefs, nPts);
    case SubToolHandler::Family::Circle: {
        const circle::ReqCount rc = circle::requirements(circle::modeFromString(t));
        return phasedPrompt(t, nRefs, nPts, rc.refs, rc.pts);
    }
    case SubToolHandler::Family::Arc: {
        const arc::ReqCount rc = arc::requirements(arc::modeFromString(t));
        return phasedPrompt(t, nRefs, nPts, rc.refs, rc.pts);
    }
    case SubToolHandler::Family::Poly: {
        const polygon::ReqCount rc = polygon::requirements(polygon::modeFromString(t));
        return phasedPrompt(t, nRefs, nPts, rc.refs, rc.pts);
    }
    case SubToolHandler::Family::Spline: {
        const spline::Mode sm = spline::modeFromString(t);
        const ToolSpec* spec = ToolRegistry::instance().find(t);
        const QString modeName = spec ? spec->displayName : QStringLiteral("样条");
        return splineStagePrompt(modeName, splineHint(sm), nPts);
    }
    case SubToolHandler::Family::Ellipse: {
        const ellipse::ReqCount rc = ellipse::requirements(ellipse::modeFromString(t));
        return phasedPrompt(t, nRefs, nPts, rc.refs, rc.pts);
    }
    }
    return t + QStringLiteral(" — 请操作:");
}
} // namespace

QString SubToolHandler::prompt(const ToolContext& ctx) const {
    const QString t = ctx.activeTool();
    const int nPts = ctx.pendingPtCount();
    const int nRefs = ctx.pickedIdCount();
    return subModePrompt(t, family_, nRefs, nPts);
}

bool SubToolHandler::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    switch (family_) {
    case Family::Line:   ctx.handleLineSubTool(w, e);   break;
    case Family::Circle: ctx.handleCircleSubTool(w, e); break;
    case Family::Arc:    ctx.handleArcSubTool(w, e);    break;
    case Family::Poly:   ctx.handlePolySubTool(w, e);   break;
    case Family::Spline: ctx.handleSplineSubTool(w, e); break;
    case Family::Ellipse: ctx.handleEllipseSubTool(w, e); break;
    }
    return true;
}

bool SubToolHandler::onRightButton(ToolContext& ctx) {
    const QString t = ctx.activeTool();
    if (family_ == Family::Line) {
        const line::Mode m = line::modeFromString(t);
        // 右键结束由元数据驱动（Snake 系列），不再按 mode 枚举
        if (line::requirements(m).rightClickFinish && ctx.pendingPtCount() >= 1) {
            ctx.finalizeSnake();
            return true;
        }
    }
    return false;
}

// ============================================================
// DimensionToolHandler —— 标注工具（两步流程）
// ============================================================
QString DimensionToolHandler::prompt(const ToolContext& ctx) const {
    if (!ctx.isDimPending())
        return QStringLiteral("标注 — 请选择要标注的图元:");
    return QStringLiteral("标注 — 点击确认标注位置 (圆右键切换直径/半径):");
}

bool DimensionToolHandler::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    ctx.handleDimension(w, dt_);
    return true;
}

bool DimensionToolHandler::onRightButton(ToolContext& ctx) {
    // 标注两步流程：右键切换圆的直径/半径，或取消标注
    if (ctx.isDimPending()) {
        // 委托给 GraphicsView::handleRightButton() 中的标注逻辑
        // 由于 ToolContext 没有直接暴露切换逻辑，返回 false 让 GraphicsView 处理
        return false;
    }
    return false;
}

} // namespace SketchCAD
