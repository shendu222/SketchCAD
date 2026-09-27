// 测量查询入口回归（offscreen 构造 MainWindow + InquiryDialog）
//
// 守住三条不变量：
//   1) 主菜单新增顶级菜单【测量查询(&I)】，含动作「打开测量查询面板...」且带 query 图标；
//   2) 工具栏第二行存在【测量查询】按钮，图标 + 提示文字与现有一致（非纯图标、tooltip 非空）；
//   3) 菜单项与工具栏按钮共用同一 InquiryDialog（代码层均经 MainWindow::openInquiry）；
//      且 InquiryDialog 自身功能正确：距离 / 面积(选区) / 质量特性 / 点坐标 四项查询可运行。
//
// 纯 UI 层：以 exit code 为权威判定（stdout 可能丢失 → 落盘）。
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QToolButton>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <cstdio>

#include "ui/MainWindow.h"
#include "ui/InquiryDialog.h"
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Vec.h"

using namespace SketchCAD;

static int g_fail = 0;
static FILE* g_log = nullptr;
static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap); fflush(g_log);
}
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { logf("  [PASS] %s\n", what); }
    else   { logf("  [FAIL] %s (%s)\n", what, detail.toUtf8().constData()); ++g_fail; }
}

static QMenu* findTopMenu(QMenuBar* bar, const QString& frag) {
    for (QAction* a : bar->actions())
        if (QMenu* m = a->menu())
            if (m->title().remove('&').contains(frag)) return m;
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    (void)argc; (void)argv;
    g_log = fopen("inquiry_dialog_test_result.txt", "w");

    // ============== 1. 主菜单入口 ==============
    {
        MainWindow win;
        win.show();
        QMenu* q = findTopMenu(win.menuBar(), QStringLiteral("测量查询"));
        check(q != nullptr, "菜单栏存在【测量查询】顶级菜单");
        if (q) {
            QAction* open = nullptr;
            for (QAction* a : q->actions()) {
                if (a->text().contains(QStringLiteral("打开测量查询面板"))) { open = a; break; }
            }
            check(open != nullptr, "【测量查询】菜单含「打开测量查询面板...」动作");
            if (open) {
                check(!open->icon().isNull(), "「打开测量查询面板」动作带图标（query）");
            }
        }
    }

    // ============== 2. 工具栏入口 ==============
    {
        MainWindow win;
        win.show();
        QToolButton* btn = nullptr;
        for (QToolBar* tb : win.findChildren<QToolBar*>()) {
            if (tb->parent() != &win) continue;
            for (QToolButton* b : tb->findChildren<QToolButton*>()) {
                const QString t = b->text().isEmpty() && b->defaultAction()
                                      ? b->defaultAction()->text() : b->text();
                if (t == QStringLiteral("测量查询")) { btn = b; break; }
            }
            if (btn) break;
        }
        check(btn != nullptr, "工具栏存在【测量查询】按钮");
        if (btn) {
            check(!btn->icon().isNull(), "【测量查询】工具按钮带图标（query）");
            check(!btn->toolTip().isEmpty(), "【测量查询】工具按钮提示文字非空");
            check(btn->toolButtonStyle() != Qt::ToolButtonIconOnly,
                  "【测量查询】工具按钮为图标+文字风格");
        }
    }

    // ============== 3. 对话框功能闭环 ==============
    {
        SketchModel m;
        const int rectId = m.addElement(ElementFactory::createRect(Vec3(0, 0, 0), Vec3(10, 20, 0), &m));
        check(rectId >= 0, "测试矩形已加入模型", QString::number(rectId));

        InquiryDialog dlg(&m, nullptr);
        QTabWidget* tabs = dlg.findChild<QTabWidget*>();
        QPlainTextEdit* res = dlg.findChild<QPlainTextEdit*>();
        check(tabs != nullptr && res != nullptr, "InquiryDialog 含标签页与结果区");

        // 3.1 距离（默认输入 0,0 -> 10,0 = 10mm）
        {
            QPushButton* b = nullptr;
            for (QPushButton* p : dlg.findChildren<QPushButton*>())
                if (p->text().contains(QStringLiteral("计算距离"))) { b = p; break; }
            check(b != nullptr, "距离页存在「计算距离」按钮");
            bool distOk = false;
            if (b) {
                b->click();
                const QString out = res->toPlainText();
                distOk = out.contains(QStringLiteral("距离 = 10"));
                check(distOk, "距离查询：两点 (0,0)-(10,0) -> 距离=10mm", out);
            }
        }

        // 3.2 面积（用当前选区）
        {
            m.selectEntity(rectId);
            tabs->setCurrentIndex(1);
            QPushButton* b = nullptr;
            for (QPushButton* p : dlg.findChildren<QPushButton*>())
                if (p->text().contains(QStringLiteral("计算选中/指定图元"))) { b = p; break; }
            check(b != nullptr, "面积页存在「计算选中/指定图元」按钮");
            if (b) {
                b->click();
                const QString out = res->toPlainText();
                // 矩形 10 x 20 -> 面积 200 mm²
                check(out.contains(QStringLiteral("面积 = 200")) || out.contains(QStringLiteral("面积 =200")),
                      "面积查询（选区矩形 10x20）-> 面积=200mm²", out);
            }
        }

        // 3.3 质量特性（用当前选区）
        {
            tabs->setCurrentIndex(2);
            QPushButton* b = nullptr;
            for (QPushButton* p : dlg.findChildren<QPushButton*>())
                if (p->text().contains(QStringLiteral("计算质量特性"))) { b = p; break; }
            check(b != nullptr, "质量特性页存在「计算质量特性」按钮");
            if (b) {
                b->click();
                const QString out = res->toPlainText();
                check(out.contains(QStringLiteral("面积")) && out.contains(QStringLiteral("形心")),
                      "质量特性查询：输出面积/形心等字段", out);
            }
        }

        // 3.4 点坐标（查询 (5,5)，应落到矩形内、最近图元即该矩形）
        {
            QLineEdit* ix = dlg.findChild<QLineEdit*>("idX");
            QLineEdit* iy = dlg.findChild<QLineEdit*>("idY");
            if (ix) ix->setText("5");
            if (iy) iy->setText("5");
            tabs->setCurrentIndex(3);
            QPushButton* b = nullptr;
            for (QPushButton* p : dlg.findChildren<QPushButton*>())
                if (p->text().contains(QStringLiteral("查询坐标"))) { b = p; break; }
            check(b != nullptr, "点坐标页存在「查询坐标 / 最近图元」按钮");
            if (b) {
                b->click();
                const QString out = res->toPlainText();
                check(out.contains(QStringLiteral("坐标")) && out.contains(QStringLiteral("最近图元")),
                      "点坐标查询：输出坐标与最近图元", out);
            }
        }
        // 3.5 直线链模式：4 条首尾相接直线围成 10×10 正方形 → 面积 100mm²
        {
            SketchModel m2;
            const int l1 = m2.addElement(ElementFactory::createLine(Vec3(0, 0, 0),  Vec3(10, 0, 0),  &m2));
            const int l2 = m2.addElement(ElementFactory::createLine(Vec3(10, 0, 0), Vec3(10, 10, 0), &m2));
            const int l3 = m2.addElement(ElementFactory::createLine(Vec3(10, 10, 0), Vec3(0, 10, 0), &m2));
            const int l4 = m2.addElement(ElementFactory::createLine(Vec3(0, 10, 0), Vec3(0, 0, 0),  &m2));
            check(l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0, "直线链测试：4 条边已加入模型");
            m2.setSelection({l1, l2, l3, l4});   // selectEntity 是替换式选中，须用 setSelection 批量选中

            InquiryDialog dlg2(&m2, nullptr);
            dlg2.findChild<QTabWidget*>()->setCurrentIndex(1);
            QPushButton* b = nullptr;
            for (QPushButton* p : dlg2.findChildren<QPushButton*>())
                if (p->text().contains(QStringLiteral("计算选中/指定图元"))) { b = p; break; }
            check(b != nullptr, "直线链测试：面积页按钮存在");
            if (b) {
                b->click();
                QPlainTextEdit* res2 = dlg2.findChild<QPlainTextEdit*>();
                const QString out = res2->toPlainText();
                check(out.contains(QStringLiteral("直线链模式")) && out.contains(QStringLiteral("面积 = 100")),
                      "对话框直线链模式：4 条边围成 10x10 正方形 → 面积=100mm²", out);
            }
        }
    }

    logf("\nRESULT: %s\n", g_fail == 0 ? "PASS" : "FAIL");
    const int rc = (g_fail == 0) ? 0 : 1;
    if (g_log) fclose(g_log);
    return rc;
}
