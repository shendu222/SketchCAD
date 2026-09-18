// 选择菜单结构回归：offscreen 下构造完整 MainWindow，验证【选择】菜单
// 与附图（LibreCAD Select 菜单）逐项对应——
//   1) 结构与顺序：全部不选 / 全选 / 选择实体 / (取消)选择轮廓 /
//      窗口选择 / 窗口取消 / 选择相交 / 取消相交 / (取消)选择图层 / 反选
//   2) 快捷键：Ctrl+K / Ctrl+A / Ctrl+Shift+I
//   3) 行为闭环：即时命令（全部不选 / 全选 / 反选）真实驱动 SketchModel 选择集
// 背景：「菜单文案写对了 ≠ 菜单动作接对了」——此前下拉菜单曾出现空 id 动作
// 触发 setActiveTool("") 的静默失效，本测试从 UI 层兜住这类回归。
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "ui/MainWindow.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Vec.h"
#include <cstdio>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { qInfo().noquote() << "  [PASS]" << what; }
    else    { qInfo().noquote() << "  [FAIL]" << what << (detail.isEmpty() ? QString() : "(" + detail + ")"); ++g_fail; }
}

// 菜单整合后【选择】挂在【编辑】下，故需递归遍历菜单树（而非只看顶层）来定位。
static QMenu* findMenuRec(QMenu* root, const QString& frag) {
    if (!root) return nullptr;
    for (QAction* a : root->actions())
        if (QMenu* m = a->menu()) {
            if (m->title().remove('&').contains(frag)) return m;
            if (QMenu* r = findMenuRec(m, frag)) return r;
        }
    return nullptr;
}
static QMenu* findTopMenu(QMenuBar* bar, const QString& frag) {
    for (QAction* a : bar->actions())
        if (QMenu* m = a->menu()) {
            if (m->title().remove('&').contains(frag)) return m;   // 顶层即命中
            if (QMenu* r = findMenuRec(m, frag)) return r;          // 嵌套子菜单
        }
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    MainWindow win;
    win.show();   // offscreen 下 show() 只是把窗口标记为可见，不真开屏

    // ---- 1. 找到【选择】菜单（菜单整合后可能嵌套在【编辑】下，递归查找）----
    QMenu* sel = findTopMenu(win.menuBar(), QStringLiteral("选择"));
    check(sel != nullptr, QStringLiteral("菜单树中存在【选择】菜单（含：嵌套于【编辑】下）"));
    if (sel) {
        QMenu* edit = findTopMenu(win.menuBar(), QStringLiteral("编辑"));
        bool nested = false;
        if (edit) for (QAction* a : edit->actions())
            if (a->menu() == sel) { nested = true; break; }
        check(nested, QStringLiteral("【选择】为【编辑】的子菜单（菜单整合要求）"));
    }

    if (sel) {
        // ---- 2. 结构与顺序（忽略分隔符；工具型项复用 toolActions_ 的中文名）----
        const QStringList expect = {
            QStringLiteral("全部不选"), QStringLiteral("全选"),
            QStringLiteral("选择实体"), QStringLiteral("(取消)选择轮廓"),
            QStringLiteral("窗口选择"), QStringLiteral("窗口取消"),
            QStringLiteral("选择相交"), QStringLiteral("取消相交"),
            QStringLiteral("(取消)选择图层"),
            QStringLiteral("反选"),
            QStringLiteral("按相同线型累加选择"),
            QStringLiteral("按相同颜色累加选择"),
        };
        QStringList actual;
        for (QAction* a : sel->actions())
            if (!a->isSeparator()) actual << a->text().remove('&');
        check(actual == expect, "【选择】菜单 12 项结构与附图一致（含按属性累加选择）",
              actual.join(QStringLiteral("|")));

        // ---- 3. 快捷键 ----
        auto act = [&](const QString& text) -> QAction* {
            for (QAction* a : sel->actions())
                if (!a->isSeparator() && a->text().remove('&') == text) return a;
            return nullptr;
        };
        check(act(QStringLiteral("全部不选")) &&
              act(QStringLiteral("全部不选"))->shortcut() == QKeySequence(QStringLiteral("Ctrl+K")),
              "全部不选 绑定 Ctrl+K");
        check(act(QStringLiteral("全选")) &&
              act(QStringLiteral("全选"))->shortcut() == QKeySequence(QStringLiteral("Ctrl+A")),
              "全选 绑定 Ctrl+A");
        check(act(QStringLiteral("反选")) &&
              act(QStringLiteral("反选"))->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+I")),
              "反选 绑定 Ctrl+Shift+I");

        // ---- 3b. 选项展示：附图每一项都带图标（此前工具型子项无图标，与图不符）----
        for (QAction* a : sel->actions()) {
            if (a->isSeparator()) continue;
            check(!a->icon().isNull(), QString("菜单项【%1】带图标").arg(a->text().remove('&')));
        }

        // ---- 3c. 交互响应：工具型菜单项触发后真实切换 activeTool ----
        // 菜单与工具条下拉共用同一个 QAction，这里验证「点了菜单 -> 工具真的切过去」
        struct { const char* text; const char* tool; } toolCases[] = {
            { "选择实体",            "select_entity" },
            { "(取消)选择轮廓",      "toggle_contour" },
            { "窗口选择",            "select_window" },
            { "窗口取消",            "deselect_window" },
            { "选择相交",            "select_intersected" },
            { "取消相交",            "deselect_intersected" },
            { "(取消)选择图层",      "toggle_layer" },
        };
        for (const auto& c : toolCases) {
            QAction* a = act(QString::fromUtf8(c.text));
            if (!a) { check(false, QString("找到工具型菜单项【%1】").arg(QString::fromUtf8(c.text))); continue; }
            a->trigger();
            check(win.findChild<SketchModel*>()->activeTool() == QString::fromUtf8(c.tool),
                  QString("触发【%1】-> activeTool=%2").arg(QString::fromUtf8(c.text), QString::fromUtf8(c.tool)),
                  win.findChild<SketchModel*>()->activeTool());
        }

        // ---- 4. 行为闭环：即时命令真实驱动模型选择集 ----
        SketchModel* model = win.findChild<SketchModel*>();
        check(model != nullptr, "可从 MainWindow 获取 SketchModel（父子关系）");
        if (model) {
            auto mkLine = [](double x1, double y1, double x2, double y2) {
                Element e; e.type = ElementType::Line;
                e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
                return e;
            };
            const int A = model->addElement(mkLine(0, 0, 10, 0));
            model->addElement(mkLine(10, 0, 10, 10));
            model->addElement(mkLine(100, 100, 110, 100));

            model->clearSelection();
            if (QAction* a = act(QStringLiteral("全选"))) {
                a->trigger();
                check((int)model->selection().size() == 3, "触发【全选】菜单项 -> 模型选中 3 个图元",
                      QString::number(model->selection().size()));
            } else check(false, "触发【全选】菜单项 -> 找不到动作");

            model->setSelected(A);
            if (QAction* a = act(QStringLiteral("反选"))) {
                a->trigger();
                check((int)model->selection().size() == 2 && !model->isSelected(A),
                      "触发【反选】菜单项 -> 选中 A 外的 2 个图元",
                      QString::number(model->selection().size()));
            } else check(false, "触发【反选】菜单项 -> 找不到动作");

            if (QAction* a = act(QStringLiteral("全部不选"))) {
                a->trigger();
                check(!model->hasSelection(), "触发【全部不选】菜单项 -> 选择集清空");
            } else check(false, "触发【全部不选】菜单项 -> 找不到动作");
        }
    }

    qInfo().noquote() << QString("================ 结果: %1 ================")
                             .arg(g_fail == 0 ? QStringLiteral("全部通过") : QString("%1 项失败").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
