#include "ui/tools/MTextTool.h"
#include "ui/MTextDialog.h"
#include "ui/GraphicsView.h"   // GraphicsView 完整类型（作为对话框 parent 的 QWidget*）
#include "core/ElementFactory.h"
#include "core/SketchModel.h"  // addElement/setSelected/setActiveTool/log

namespace SketchCAD {

QString MTextTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("多行文字(MTEXT)：点击画布指定插入点，将弹出编辑框");
}

bool MTextTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    // 锚点 = 当前光标世界坐标
    const Vec3 anchor = ctx.snapWorld(w);

    // 弹出多行文字编辑对话框（模态）。样式表、字段占位等均在其中完成。
    MTextDialog dlg(ctx.model, ctx.view);
    if (dlg.exec() != QDialog::Accepted) {
        // 取消：回到选择工具，避免卡在绘制态
        ctx.model->setActiveTool(QStringLiteral("select"));
        return true;
    }

    // 用对话框当前设置 + 锚点构造 MText 并加入模型
    Element el = dlg.buildElement(anchor);
    const int id = ctx.model->addElement(el);   // 触发 rebuild()，预览项在那里被销毁
    ctx.model->setSelected(id);
    ctx.model->log(QStringLiteral("已创建多行文字 #%1（%2 行，样式 %3）")
                      .arg(id)
                      .arg(el.text.split('\n').size())
                      .arg(el.styleName.isEmpty() ? QStringLiteral("默认") : el.styleName));
    // 创建完成后回到选择工具（MTEXT 为单次放置型）
    ctx.model->setActiveTool(QStringLiteral("select"));
    return true;
}

} // namespace SketchCAD
