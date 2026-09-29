#include "ui/tools/TableTool.h"
#include "ui/TableDialog.h"
#include "ui/GraphicsView.h"   // GraphicsView 完整类型（作为对话框 parent 的 QWidget*）
#include "core/ElementFactory.h"
#include "core/SketchModel.h"  // addElement/setSelected/setActiveTool/log

namespace SketchCAD {

QString TableTool::prompt(const ToolContext& ctx) const {
    (void)ctx;
    return QStringLiteral("表格(TABLE)：点击画布指定表格左上角，将弹出表格编辑器");
}

bool TableTool::onPress(ToolContext& ctx, const Vec3& w, QMouseEvent* e) {
    (void)e;
    // 锚点 = 当前光标世界坐标（表格左上角；行向下增长）
    const Vec3 topLeft = ctx.snapWorld(w);

    // 弹出表格编辑对话框（模态）。单元格文本、行列、列宽、字高、对齐均在其中完成。
    TableDialog dlg(ctx.model, ctx.view);
    if (dlg.exec() != QDialog::Accepted) {
        // 取消：回到选择工具，避免卡在绘制态
        ctx.model->setActiveTool(QStringLiteral("select"));
        return true;
    }

    // 用对话框当前设置 + 锚点构造 Table 并加入模型
    Element el = dlg.buildElement(topLeft);
    const int id = ctx.model->addElement(el);   // 触发 rebuild()，预览项在那里被销毁
    ctx.model->setSelected(id);
    ctx.model->log(QStringLiteral("已创建表格 #%1（%2×%3）")
                      .arg(id).arg(el.tableRows).arg(el.tableCols));
    // 创建完成后回到选择工具（TABLE 为单次放置型）
    ctx.model->setActiveTool(QStringLiteral("select"));
    return true;
}

} // namespace SketchCAD
