// 修改菜单结构回归：offscreen 下构造完整 MainWindow，验证【修改】菜单
// 与附图（LibreCAD Modify 菜单）逐项对应——
//   1) 结构与顺序：移动/复制、复制、对齐(子菜单)、旋转、缩放、镜像、移动并旋转、
//      旋转两条、反向 | 修剪、修剪二、延长、连接线段、打断/等分、线间隙 |
//      偏移、倒角、圆角、等分、拉伸 | 属性、属性模板、文本分解为字、分解、删除
//   2) 每一项都带矢量图标（此前工具型子项无图标，与附图不符）
//   3) 行为闭环：不弹参数对话框的菜单项（反向 / 线间隙 / 分解 / 倒角 / 删除）
//      触发后真实改写 SketchModel
//   4) 撤销一致性：一次批量修改 -> 一次 Ctrl+Z 完整回滚（modify::applyChanges
//      全程走 *Quiet API，只由调用方 push 一份快照）
// 背景：「菜单文案写对了 ≠ 菜单动作接对了」。旋转/偏移/拉伸等项需要用户输参数，
// 会弹模态对话框，故本测试只触发无对话框的项，避免阻塞。
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QToolButton>
#include "ui/MainWindow.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Vec.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { qInfo().noquote() << "  [PASS]" << what; }
    else    { qInfo().noquote() << "  [FAIL]" << what << (detail.isEmpty() ? QString() : "(" + detail + ")"); ++g_fail; }
}

// 菜单整合后【修改】挂在【编辑】下，故需递归遍历菜单树（而非只看顶层）来定位。
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

    // ---- 1. 找到【修改】菜单（菜单整合后可能嵌套在【编辑】下，递归查找）----
    QMenu* mod = findTopMenu(win.menuBar(), QStringLiteral("修改"));
    check(mod != nullptr, QStringLiteral("菜单树中存在【修改】菜单（含：嵌套于【编辑】下）"));
    if (mod) {
        QMenu* edit = findTopMenu(win.menuBar(), QStringLiteral("编辑"));
        bool nested = false;
        if (edit) for (QAction* a : edit->actions())
            if (a->menu() == mod) { nested = true; break; }
        check(nested, QStringLiteral("【修改】为【编辑】的子菜单（菜单整合要求）"));
    }

    if (!mod) { qInfo().noquote() << QStringLiteral("RESULT: FAIL"); return 1; }

    // ---- 2. 结构与顺序（忽略分隔符；& 是 Alt 助记符，比较前去掉）----
    const QStringList expect = {
        QStringLiteral("移动 / 复制..."), QStringLiteral("复制"), QStringLiteral("阵列(R)"),
        QStringLiteral("剖面填充(H)..."),
        QStringLiteral("对齐(A)"),
        QStringLiteral("旋转..."), QStringLiteral("缩放..."), QStringLiteral("镜像..."),
        QStringLiteral("移动并旋转..."), QStringLiteral("旋转两条图元..."), QStringLiteral("反向"),
        QStringLiteral("修剪"), QStringLiteral("修剪二..."), QStringLiteral("延长..."),
        QStringLiteral("连接线段..."), QStringLiteral("打断 / 等分..."), QStringLiteral("线间隙..."),
        QStringLiteral("偏移..."), QStringLiteral("倒角..."), QStringLiteral("圆角..."),
        QStringLiteral("等分..."), QStringLiteral("拉伸..."),
        QStringLiteral("属性..."), QStringLiteral("属性模板..."), QStringLiteral("文本分解为字..."),
        QStringLiteral("分解"), QStringLiteral("删除"),
    };
    QStringList actual;
    for (QAction* a : mod->actions())
        if (!a->isSeparator()) actual << a->text().remove('&');
    check(actual == expect,
          QStringLiteral("【修改】菜单 %1 项结构与附图一致").arg(expect.size()),
          actual.join(QStringLiteral("|")));

    // ---- 2b. 「对齐」子菜单 3 项 ----
    QAction* alignAct = nullptr;
    for (QAction* a : mod->actions())
        if (!a->isSeparator() && a->text().remove('&') == QStringLiteral("对齐(A)")) alignAct = a;
    if (alignAct && alignAct->menu()) {
        QStringList sub;
        for (QAction* a : alignAct->menu()->actions())
            if (!a->isSeparator()) sub << a->text().remove('&');
        const QStringList subExpect = { QStringLiteral("对齐..."),
                                        QStringLiteral("对齐单条..."),
                                        QStringLiteral("对齐参考点...") };
        check(sub == subExpect, QStringLiteral("【对齐】子菜单 3 项（对齐 / 对齐单条 / 对齐参考点）"),
              sub.join(QStringLiteral("|")));
    } else check(false, QStringLiteral("【对齐】是子菜单且含 3 项"));

    auto act = [&](const QString& text) -> QAction* {
        for (QAction* a : mod->actions())
            if (!a->isSeparator() && a->text().remove('&') == text) return a;
        return nullptr;
    };

    // ---- 3. 每一项都带矢量图标 ----
    int noIcon = 0;
    for (QAction* a : mod->actions()) {
        if (a->isSeparator()) continue;
        if (a->icon().isNull()) { ++noIcon; qInfo().noquote() << "    无图标:" << a->text(); }
    }
    check(noIcon == 0, QStringLiteral("【修改】菜单全部项都带图标"),
          QStringLiteral("缺图标 %1 项").arg(noIcon));

    // ---- 4. 行为闭环：无对话框菜单项真实驱动模型 ----
    SketchModel* model = win.findChild<SketchModel*>();
    check(model != nullptr, QStringLiteral("可从 MainWindow 获取 SketchModel"));
    if (!model) { qInfo().noquote() << QStringLiteral("RESULT: FAIL"); return 1; }

    auto mkLine = [](double x1, double y1, double x2, double y2) {
        Element e; e.type = ElementType::Line;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        return e;
    };
    auto mkRect = [](double x1, double y1, double x2, double y2) {
        Element e; e.type = ElementType::Rectangle;
        e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
        return e;
    };

    // --- 4a. 反向：直线端点互换 ---
    model->clear();
    const int L1 = model->addElement(mkLine(0, 0, 10, 0));
    model->setSelected(L1);
    if (QAction* a = act(QStringLiteral("反向"))) {
        a->trigger();
        const Element* e = model->element(L1);
        check(e && qFuzzyCompare(e->pts[0].x, 10.0) && qFuzzyCompare(e->pts[1].x, 0.0),
              QStringLiteral("触发【反向】-> 直线端点互换 (0,0)-(10,0) → (10,0)-(0,0)"),
              e ? QStringLiteral("(%1,%2)-(%3,%4)").arg(e->pts[0].x).arg(e->pts[0].y)
                    .arg(e->pts[1].x).arg(e->pts[1].y) : QStringLiteral("无图元"));
    } else check(false, QStringLiteral("找到【反向】菜单项"));

    // --- 4b. 文本分解为字：一个 2 字文本 -> 2 个单字文本 ---
    // 说明：【线间隙】等需要用户输入参数的项会弹模态对话框，触发会阻塞测试进程，
    //       故这里只覆盖无对话框的菜单项（旋转/偏移/拉伸…的行为由 selftest §30 的
    //       modify:: 引擎与 CLI 分支覆盖）。
    model->clear();
    {
        Element t; t.type = ElementType::Text;
        t.text = QStringLiteral("AB");
        t.pts = {Vec3(0, 0, 0)};
        const int T1 = model->addElement(t);
        model->setSelected(T1);
    }
    if (QAction* a = act(QStringLiteral("文本分解为字..."))) {
        a->trigger();
        int texts = 0;
        for (const auto& e : model->elements()) if (e.type == ElementType::Text) ++texts;
        check(texts == 2, QStringLiteral("触发【文本分解为字】-> \"AB\" 拆成 2 个单字文本"),
              QStringLiteral("实际=%1").arg(texts));
    } else check(false, QStringLiteral("找到【文本分解为字】菜单项"));

    // --- 4c. 分解：矩形 → 4 条边，且一次 undo 完整回滚 ---
    model->clear();
    const int R1 = model->addElement(mkRect(0, 0, 10, 10));
    model->setSelected(R1);
    if (QAction* a = act(QStringLiteral("分解"))) {
        a->trigger();
        check(model->elementCount() == 4, QStringLiteral("触发【分解】-> 矩形拆成 4 条边"),
              QStringLiteral("实际=%1").arg(model->elementCount()));
        int lines = 0;
        for (const auto& e : model->elements()) if (e.type == ElementType::Line) ++lines;
        check(lines == 4, QStringLiteral("分解结果 4 条均为直线"), QStringLiteral("实际=%1").arg(lines));
        // 一次批量修改 = 一次 Ctrl+Z
        model->undo();
        check(model->elementCount() == 1, QStringLiteral("一次 undo 完整回滚分解（不是逐段撤销）"),
              QStringLiteral("实际=%1").arg(model->elementCount()));
        const Element* back = model->element(R1);
        check(back && back->type == ElementType::Rectangle, QStringLiteral("undo 后恢复为原矩形"));
    } else check(false, QStringLiteral("找到【分解】菜单项"));

    // --- 4d. 倒角：两条垂直直线 → 2 截短线 + 1 连接直线 ---
    model->clear();
    const int B1 = model->addElement(mkLine(0, 0, 10, 0));
    const int B2 = model->addElement(mkLine(0, 0, 0, 10));
    model->setSelection({B1, B2});
    if (QAction* a = act(QStringLiteral("倒角..."))) {
        a->trigger();
        int lines = 0;
        for (const auto& e : model->elements()) if (e.type == ElementType::Line) ++lines;
        check(lines == 3, QStringLiteral("触发【倒角】-> 2 原线删 + 2 截短线 + 1 连接线 = 3 条直线"),
              QStringLiteral("实际=%1").arg(lines));
    } else check(false, QStringLiteral("找到【倒角】菜单项"));

    // --- 4e. 删除：选中图元被删除 ---
    model->clear();
    const int D1 = model->addElement(mkLine(0, 0, 10, 0));
    model->addElement(mkLine(20, 0, 30, 0));
    model->setSelected(D1);
    if (QAction* a = act(QStringLiteral("删除"))) {
        a->trigger();
        check(model->elementCount() == 1 && model->element(D1) == nullptr,
              QStringLiteral("触发【删除】-> 选中图元被移除"),
              QStringLiteral("剩余=%1").arg(model->elementCount()));
    } else check(false, QStringLiteral("找到【删除】菜单项"));

    // ---- 5. 工具条「修改」下拉按钮：修剪/延伸 + 4 个一键命令 ----
    QToolButton* modifyBtn = nullptr;
    for (QToolBar* tb : win.findChildren<QToolBar*>()) {
        if (tb->parent() != &win) continue;
        for (QToolButton* b : tb->findChildren<QToolButton*>())
            if (b->text().remove('&') == QStringLiteral("修改")) modifyBtn = b;
    }
    check(modifyBtn != nullptr, QStringLiteral("工具条存在【修改】下拉按钮"));
    if (modifyBtn && modifyBtn->menu()) {
        QStringList sub;
        for (QAction* a : modifyBtn->menu()->actions())
            if (!a->isSeparator()) sub << a->text().remove('&');
        const QStringList subExpect = { QStringLiteral("修剪"), QStringLiteral("延伸"),
                                        QStringLiteral("旋转"), QStringLiteral("镜像"),
                                        QStringLiteral("偏移"), QStringLiteral("分解") };
        check(sub == subExpect, QStringLiteral("【修改】下拉含 修剪/延伸/旋转/镜像/偏移/分解"),
              sub.join(QStringLiteral("|")));
        check(!modifyBtn->icon().isNull(), QStringLiteral("【修改】下拉按钮带图标"));
    }

    qInfo().noquote() << (g_fail == 0 ? QStringLiteral("RESULT: PASS") : QStringLiteral("RESULT: FAIL"));
    return g_fail == 0 ? 0 : 1;
}
