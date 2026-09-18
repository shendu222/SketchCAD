// 标题栏插入 + 累加选择 单元测试：纯核心层，无需 GUI。
//   * buildInsertedTitleBlock：依据内容包围盒生成"图纸边框 + 标准标题栏"，
//     含 图名/图号/比例/绘制日期 等必要字段（tb:field: 命名），且为纯函数不改动模型；
//   * addSelectionByStyle / addSelectionByColor：按相同线型/相同颜色把图元加入选择集，
//     保留已有选择（累加、不去重重复、不清除），且不修改/移动/删除任何图元。
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
#include "core/SheetSpec.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}

static Element mkLine(double x1, double y1, double x2, double y2) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}

int main() {
    // ==================== 1. 标题栏构建器 ====================
    // 1a. 含内容的包围盒：生成边框 + 标题栏，四必要字段齐备且文本正确
    InsertedTitleBlock data;
    data.drawingName  = QStringLiteral("法兰盘");
    data.drawingNumber = QStringLiteral("FL-001");
    data.scale        = QStringLiteral("1:2");
    data.designDate   = QStringLiteral("2026-09-13");
    auto elems = buildInsertedTitleBlock(QRectF(0, 0, 100, 80), data);
    check(!elems.empty(), "有内容包围盒：生成了图元");

    int nameF = 0, numF = 0, scaleF = 0, dateF = 0, rectC = 0, lineC = 0;
    for (const auto& e : elems) {
        if (e.type == ElementType::Rectangle) ++rectC;
        if (e.type == ElementType::Line)      ++lineC;
        if (e.type == ElementType::Text) {
            if      (e.name == QStringLiteral("tb:field:drawingName"))  { check(e.text == data.drawingName,  "标题栏 图名 文本正确"); ++nameF; }
            else if (e.name == QStringLiteral("tb:field:drawingNumber")){ check(e.text == data.drawingNumber,"标题栏 图号 文本正确"); ++numF; }
            else if (e.name == QStringLiteral("tb:field:scale"))        { check(e.text == data.scale,        "标题栏 比例 文本正确"); ++scaleF; }
            else if (e.name == QStringLiteral("tb:field:designDate"))   { check(e.text == data.designDate,   "标题栏 绘制日期 文本正确"); ++dateF; }
        }
    }
    check(nameF >= 1 && numF >= 1 && scaleF >= 1 && dateF >= 1,
          "【必要字段】图名/图号/比例/绘制日期 均已生成且文本正确");
    check(rectC >= 2, "至少含 图纸边框 + 标题栏外框 两个矩形", QString::number(rectC));
    check(lineC >= 1, "标题栏含分隔线（细实线网格）", QString::number(lineC));

    // 1b. 所有生成的图元都归属到"图框"/"标题栏"层，且为有效线型/颜色（结构正确）
    bool allLayered = true;
    for (const auto& e : elems)
        if (e.layer != QStringLiteral("图框") && e.layer != QStringLiteral("标题栏")) allLayered = false;
    check(allLayered, "生成图元均归入 图框/标题栏 层");

    // 1c. 空包围盒：不崩溃，仍能生成（采用默认区域）
    auto elems2 = buildInsertedTitleBlock(QRectF(), data);
    check(!elems2.empty(), "空内容包围盒：仍可生成标题栏（默认区域）");

    // 1d. 纯函数：不读取/改动任何外部状态（无模型入参，天然满足"不修改已有图元"）

    // ==================== 2. 累加选择（按线型 / 按颜色）================
    SketchModel m;
    Element a = mkLine(0, 0, 10, 0);  a.lineStyle = LineStyle::Dashed; a.color = QColor(255, 0, 0);
    Element b = mkLine(0, 5, 10, 5);  b.lineStyle = LineStyle::Dashed; b.color = QColor(255, 0, 0);
    Element c = mkLine(0, 10, 10, 10); c.lineStyle = LineStyle::Solid;  c.color = QColor(0, 0, 255);
    Element d = mkLine(20, 0, 25, 0); d.lineStyle = LineStyle::Solid;  d.color = QColor(0, 0, 255);
    const int ia = m.addElement(a), ib = m.addElement(b), ic = m.addElement(c), id = m.addElement(d);
    check(m.elements().size() == 4, "准备：4 个图元（2 虚线红 + 2 实线蓝）");

    // 2a. 选中 a（虚线红），按线型累加选虚线 -> 仅追加 b，保留 a（选区=2，不清除）
    m.setSelection({ia});
    m.addSelectionByStyle(LineStyle::Dashed);
    check(m.selection().size() == 2, "按线型累加：选区含 2 个虚线图元（a+b）", QString::number(m.selection().size()));
    check(m.isSelected(ia) && m.isSelected(ib), "按线型累加：原选 a 保留、新增 b");
    check(!m.isSelected(ic) && !m.isSelected(id), "按线型累加：实线图元 c/d 未被选中（互不干扰）");

    // 2b. 继续按颜色累加选蓝色 -> 追加 c、d，保留 a、b（选区=4，仍为累加）
    m.addSelectionByColor(QColor(0, 0, 255));
    check(m.selection().size() == 4, "按颜色累加：选区扩为 4（保留原 2，新增 c/d）", QString::number(m.selection().size()));
    check(m.isSelected(ia) && m.isSelected(ib) && m.isSelected(ic) && m.isSelected(id),
          "按颜色累加：a/b/c/d 全部在选区内");

    // 2c. 累加选择不得修改/移动/删除任何图元：图元数量与各自属性保持不变
    check(m.elements().size() == 4, "累加选择后：图元总数不变（未删除）");
    check(m.element(ia)->lineStyle == LineStyle::Dashed && m.element(ia)->color == QColor(255, 0, 0),
          "累加选择后：a 的线型/颜色未被改动");
    check(m.element(ic)->lineStyle == LineStyle::Solid && m.element(ic)->color == QColor(0, 0, 255),
          "累加选择后：c 的线型/颜色未被改动");
    check(m.element(id)->pts[0] == Vec3(20, 0, 0), "累加选择后：d 的几何位置未被移动");

    // 2d. 反向对照：清空后按线型选实线 -> 只选 c、d（验证按属性匹配正确）
    m.clearSelection();
    m.addSelectionByStyle(LineStyle::Solid);
    check(m.selection().size() == 2 && m.isSelected(ic) && m.isSelected(id),
          "清选后按线型选实线：仅 c/d 入选（按属性匹配正确）");

    printf("\n标题栏/累加选择测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
