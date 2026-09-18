// 按幅面自动套框 + 标题栏 单元测试：纯核心层，无需 GUI。
// 验证 buildAutoFrameTitleBlock：
//   * 纯函数（无模型入参），只追加新图元，绝不读取/改动模型；
//   * 图纸边框尺寸 = max(所选幅面, 内容最小外接矩形 + 边距)：
//       小幅面且内容很小 -> 边框至少达到所选幅面（A0~A5）；
//       内容很大 -> 边框自动扩张以包住全部图元；
//   * 内容被边框完整包含（含统一安全边距）；
//   * 标题栏固定在图框右下角，并通过自动避让保证不与任何已有图元相交；
//   * 必要字段 图名/图号/比例/绘制日期 齐备（tb:field: 命名）。
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
#include "core/SheetSpec.h"
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

// 取出层为"图框"的矩形（buildAutoFrameTitleBlock 仅画一个图框矩形）
static bool findFrameRect(const std::vector<Element>& elems, QRectF& out) {
    for (const auto& e : elems) {
        if (e.type == ElementType::Rectangle && e.layer == QStringLiteral("图框") && e.pts.size() >= 2) {
            out = QRectF(e.pts[0].x, e.pts[0].y, e.pts[1].x - e.pts[0].x, e.pts[1].y - e.pts[0].y).normalized();
            return true;
        }
    }
    return false;
}

// 取出层为"标题栏"的外框矩形（appendTitleBlockGrid 仅画一个标题栏矩形）
static bool findTitleBlockRect(const std::vector<Element>& elems, QRectF& out) {
    for (const auto& e : elems) {
        if (e.type == ElementType::Rectangle && e.layer == QStringLiteral("标题栏") && e.pts.size() >= 2) {
            const QRectF r(e.pts[0].x, e.pts[0].y, e.pts[1].x - e.pts[0].x, e.pts[1].y - e.pts[0].y);
            if (r.width() > out.width()) out = r.normalized();  // 取最宽的（即外框）
        }
    }
    return out.isValid();
}

static bool hasField(const std::vector<Element>& elems, const QString& key) {
    for (const auto& e : elems)
        if (e.type == ElementType::Text && e.name == QStringLiteral("tb:field:") + key) return true;
    return false;
}

int main() {
    InsertedTitleBlock data;
    data.drawingName   = QStringLiteral("法兰盘");
    data.drawingNumber = QStringLiteral("FL-001");
    data.scale         = QStringLiteral("1:2");
    data.designDate    = QStringLiteral("2026-09-13");

    // ==================== 1. 小幅面 + 小内容：边框≥所选幅面，且包含内容 ====================
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A4, true); // 297×210
        auto elems = buildAutoFrameTitleBlock(QRectF(0, 0, 10, 10), spec, data);
        check(!elems.empty(), "A4+小内容：生成了图元");

        QRectF frame;
        check(findFrameRect(elems, frame), "A4+小内容：存在图框矩形");
        check(frame.width()  >= spec.width  - 1e-6, "A4+小内容：边框宽度≥所选幅面宽度(297)",
              QString::number(frame.width()));
        check(frame.height() >= spec.height - 1e-6, "A4+小内容：边框高度≥所选幅面高度(210)",
              QString::number(frame.height()));

        const QRectF content(0, 0, 10, 10);
        // 标准 QRectF 约定：left()=最小X，right()=最大X，top()=最小Y（图框下边界），bottom()=最大Y（图框上边界）
        check(frame.left()   <= content.left()   - 1e-6, "A4+小内容：边框左边界在内容左侧（含边距）");
        check(frame.right()  >= content.right()  + 1e-6, "A4+小内容：边框右边界在内容右侧（含边距）");
        check(frame.top()    <= content.top()    - 1e-6, "A4+小内容：边框下边界在内容下方（含边距）");
        check(frame.bottom() >= content.bottom() + 1e-6, "A4+小内容：边框上边界在内容上方（含边距）");

        QRectF tb;
        check(findTitleBlockRect(elems, tb), "A4+小内容：存在标题栏矩形");
        check(!tb.intersects(content), "A4+小内容：标题栏不覆盖内容");
        check(tb.right()  <= frame.right() + 1e-6, "A4+小内容：标题栏在图框内（右侧不越界）");
        check(qAbs(tb.top() - frame.top()) < 1e-6, "A4+小内容：标题栏贴图框下边（右下角）");

        check(hasField(elems, QStringLiteral("drawingName"))  &&
              hasField(elems, QStringLiteral("drawingNumber")) &&
              hasField(elems, QStringLiteral("scale"))        &&
              hasField(elems, QStringLiteral("designDate")),
              "A4+小内容：必要字段 图名/图号/比例/绘制日期 齐备");
    }

    // ==================== 2. 大内容：边框自动扩张以包住全部图元 ====================
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A4, true); // 297×210
        const QRectF content(0, 0, 1000, 800);   // 远超 A4
        auto elems = buildAutoFrameTitleBlock(content, spec, data);

        QRectF frame;
        check(findFrameRect(elems, frame), "大内容：存在图框矩形");
        check(frame.width()  > spec.width  + 1.0, "大内容：边框宽度被扩张（>297）", QString::number(frame.width()));
        check(frame.height() > spec.height + 1.0, "大内容：边框高度被扩张（>210）", QString::number(frame.height()));
        check(frame.left()   <= content.left()   - 1e-6 && frame.right()  >= content.right()  + 1e-6 &&
              frame.top()    <= content.top()    - 1e-6 && frame.bottom() >= content.bottom() + 1e-6,
              "大内容：边框完整包住内容（含边距）");

        QRectF tb;
        check(findTitleBlockRect(elems, tb), "大内容：存在标题栏矩形");
        check(!tb.intersects(content), "大内容：标题栏自动避让，不覆盖内容（右下角外扩）");
    }

    // ==================== 3. 内容恰在右下角：触发避让，标题栏不得与之相交 ====================
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A3, true); // 420×297
        // 内容贴着右下角，正好与"默认右下角标题栏"重叠
        const QRectF content(300, 0, 100, 56);
        auto elems = buildAutoFrameTitleBlock(content, spec, data);

        QRectF frame, tb;
        check(findFrameRect(elems, frame), "右下角内容：存在图框矩形");
        check(findTitleBlockRect(elems, tb), "右下角内容：存在标题栏矩形");
        check(!tb.intersects(content), "右下角内容：标题栏自动避让，不与内容相交",
              QString("tb=[%1,%2 %3x%4] content=[%5,%6 %7x%8]")
                  .arg(tb.left()).arg(tb.bottom()).arg(tb.width()).arg(tb.height())
                  .arg(content.left()).arg(content.bottom()).arg(content.width()).arg(content.height()));
        check(frame.left() <= content.left() - 1e-6 && frame.right() >= content.right() + 1e-6,
              "右下角内容：边框仍完整包住内容");
    }

    // ==================== 4. 纯函数：不读取/改动任何外部状态（无模型入参，天然满足）================
    {
        const QRectF cb(20, 30, 200, 120);
        auto a = buildAutoFrameTitleBlock(cb, SheetSpec::make(SheetFormat::A3, true), data);
        auto b = buildAutoFrameTitleBlock(cb, SheetSpec::make(SheetFormat::A3, true), data);
        check(a.size() == b.size(), "纯函数：相同输入两次调用输出规模一致（确定性）",
              QString("%1 vs %2").arg(a.size()).arg(b.size()));
        // 输入包围盒未被改动
        check(cb.left() == 20 && cb.right() == 220 && cb.top() == 30 && cb.bottom() == 150,
              "纯函数：输入 contentBBox 未被修改");
        bool allLayered = true;
        for (const auto& e : a)
            if (e.layer != QStringLiteral("图框") && e.layer != QStringLiteral("标题栏")) allLayered = false;
        check(allLayered, "生成的图元均归入 图框/标题栏 层（便于按层控制显示）");
    }

    // ==================== 5. 纵向幅面：尺寸按长边垂直 ====================
    {
        const SheetSpec spec = SheetSpec::make(SheetFormat::A4, false); // 210×297（纵）
        auto elems = buildAutoFrameTitleBlock(QRectF(0, 0, 10, 10), spec, data);
        QRectF frame;
        check(findFrameRect(elems, frame), "A4纵：存在图框矩形");
        check(std::abs(frame.width() - spec.width) < 1e-6 && std::abs(frame.height() - spec.height) < 1e-6,
              "A4纵：边框尺寸与纵向规格一致（宽210 高297）");
    }

    printf("\n按幅面自动套框测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
