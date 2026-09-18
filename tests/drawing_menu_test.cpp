// 绘图菜单结构回归：offscreen 下构造完整 MainWindow，验证【绘图】菜单去重后的结构——
//   1) 存在 7 个图元绘制方式子菜单（直线 / 圆 / 圆弧 / 多边形 / 样条 / 椭圆 / 折线）
//      与「尺寸标注」子菜单，且每个子菜单都非空（即所有子功能入口仍在）。
//   2) 不再有与子菜单功能重复的扁平动作项：直线 / 圆 / 圆弧 / 多边形 / 椭圆 / 折线 /
//      样条 / 样条编辑 / 矩形 / 标注 这些「既作为扁平项又作为子菜单首项」的重复入口
//      已从绘图菜单顶层移除（仅保留含子菜单的那一项）。
//   3) 「修改」类命令（修剪 / 延伸 / 旋转 / 镜像 / 偏移 / 分解）不再平铺在绘图菜单——
//      它们已归到【编辑 > 修改】子菜单，绘图菜单去重后不丢失任何子功能入口。
//   4) 无同名重复项：绘图菜单顶层可点击项（去分隔符后）文本互不相同。
//   8) 顺带守住【文件】菜单的多格式存取入口与「绘图比例」归属（图纸属性只在【文件】，
//      【视图】不再重复提供），避免同一功能两处出现。
// 背景：此前绘图菜单把每个图元的主工具既平铺在「基本图元」分组，又作为「绘制方式」
// 子菜单的首项，造成同一功能在两个层级重复出现；本次按「保留含子菜单的入口、移除
// 无子菜单重复项」原则合并去重。
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <cmath>
#include "ui/MainWindow.h"
#include "ui/NewDrawingDialog.h"
#include "core/SketchModel.h"
#include "core/ToolRegistry.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { qInfo().noquote() << "  [PASS]" << what; }
    else    { qInfo().noquote() << "  [FAIL]" << what << (detail.isEmpty() ? QString() : "(" + detail + ")"); ++g_fail; }
}

// 在菜单栏顶层按标题片段定位菜单（绘图 / 编辑 均为顶层，无需递归）
static QMenu* findTopMenu(QMenuBar* bar, const QString& frag) {
    for (QAction* a : bar->actions())
        if (QMenu* m = a->menu())
            if (m->title().remove('&').contains(frag)) return m;
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    MainWindow win;
    win.show();   // offscreen 下 show() 只是把窗口标记为可见，不真开屏

    // ---- 1. 找到【绘图】菜单 ----
    QMenu* draw = findTopMenu(win.menuBar(), QStringLiteral("绘图"));
    check(draw != nullptr, QStringLiteral("菜单栏存在【绘图】菜单"));
    if (!draw) { qInfo().noquote() << QStringLiteral("RESULT: FAIL"); return 1; }

    // 顶层可点击项（去分隔符）的文本集合
    QStringList topTexts;
    QList<QAction*> topLeaf;
    QList<QMenu*>   topSubs;
    for (QAction* a : draw->actions()) {
        if (a->isSeparator()) continue;
        const QString t = a->text().remove('&');
        if (t.isEmpty()) continue;
        topTexts << t;
        if (a->menu()) topSubs << a->menu();
        else           topLeaf  << a;
    }

    // ---- 2. 7 个图元子菜单 + 尺寸标注子菜单均存在且非空 ----
    QStringList wantSubs = {
        QStringLiteral("直线"), QStringLiteral("圆"), QStringLiteral("圆弧"),
        QStringLiteral("多边形"), QStringLiteral("样条"), QStringLiteral("椭圆"),
        QStringLiteral("折线"), QStringLiteral("尺寸标注")
    };
    for (const QString& name : wantSubs) {
        QMenu* sm = nullptr;
        for (QMenu* m : topSubs)
            if (m->title().remove('&').contains(name)) { sm = m; break; }
        bool ok = (sm != nullptr) && !sm->actions().isEmpty();
        check(ok, QStringLiteral("【绘图】含子菜单「%1」且非空（子功能入口在）").arg(name),
              sm ? QStringLiteral("空") : QStringLiteral("缺"));
    }

    // ---- 3. 与子菜单重复的扁平项已移除（仅保留含子菜单的那一项）----
    // 这些名字若以「无子菜单的扁平动作」形式出现在绘图顶层，即为重复残留。
    QStringList dupLeaf = {
        QStringLiteral("直线"), QStringLiteral("圆"), QStringLiteral("圆弧"),
        QStringLiteral("多边形"), QStringLiteral("椭圆"), QStringLiteral("折线"),
        QStringLiteral("样条"), QStringLiteral("样条编辑"), QStringLiteral("矩形"),
        QStringLiteral("标注")
    };
    for (const QString& name : dupLeaf) {
        bool foundLeaf = false;
        for (QAction* a : topLeaf)
            if (a->text().remove('&') == name) { foundLeaf = true; break; }
        check(!foundLeaf, QStringLiteral("绘图顶层已无扁平重复项「%1」（仅保留其子菜单）").arg(name));
    }

    // ---- 4. 「修改」类命令不在绘图菜单平铺（已归【编辑>修改】）----
    QStringList modNames = {
        QStringLiteral("修剪"), QStringLiteral("延伸"), QStringLiteral("旋转"),
        QStringLiteral("镜像"), QStringLiteral("偏移"), QStringLiteral("分解")
    };
    for (const QString& name : modNames) {
        bool inDraw = false;
        for (QAction* a : topLeaf)
            if (a->text().remove('&').contains(name)) { inDraw = true; break; }
        check(!inDraw, QStringLiteral("绘图菜单顶层不再平铺「%1」（子功能在【编辑>修改】）").arg(name));
    }

    // ---- 5. 无同名重复项：顶层可点击项文本互不相同 ----
    QStringList seen;
    bool dup = false;
    for (const QString& t : topTexts) {
        if (seen.contains(t)) { dup = true; qInfo().noquote() << "    重复标签:" << t; }
        seen << t;
    }
    check(!dup, QStringLiteral("绘图菜单顶层可点击项无同名重复"),
          QStringLiteral("共 %1 项").arg(topTexts.size()));

    // ---- 6. 不丢失子功能入口：多边形子菜单仍可触达 矩形 / 多边形；尺寸标注子菜单含 标注 ----
    auto subByName = [&](const QString& name) -> QMenu* {
        for (QMenu* m : topSubs)
            if (m->title().remove('&').contains(name)) return m;
        return nullptr;
    };
    if (QMenu* poly = subByName(QStringLiteral("多边形"))) {
        bool hasRect = false, hasPoly = false;
        for (QAction* a : poly->actions()) {
            if (a->isSeparator()) continue;
            const QString t = a->text().remove('&');
            if (t.contains(QStringLiteral("矩形"))) hasRect = true;
            if (t.contains(QStringLiteral("多边形"))) hasPoly = true;
        }
        check(hasRect, QStringLiteral("【多边形】子菜单仍可触达「矩形」"));
        check(hasPoly, QStringLiteral("【多边形】子菜单仍可触达「多边形」"));
    }
    if (QMenu* dim = subByName(QStringLiteral("尺寸标注"))) {
        bool hasDim = false;
        for (QAction* a : dim->actions()) {
            if (a->isSeparator()) continue;
            const QString t = a->text().remove('&');
            if (t.contains(QStringLiteral("标注")) || t.contains(QStringLiteral("尺寸"))) { hasDim = true; break; }
        }
        check(hasDim, QStringLiteral("【尺寸标注】子菜单含尺寸标注入口（功能未丢失）"));
    }

    // ---- 7. 修改命令仍可经【编辑>修改】触达（证明去重未丢功能）----
    QMenu* edit = findTopMenu(win.menuBar(), QStringLiteral("编辑"));
    if (edit) {
        QMenu* mod = nullptr;
        for (QAction* a : edit->actions())
            if (QMenu* m = a->menu())
                if (m->title().remove('&').contains(QStringLiteral("修改"))) { mod = m; break; }
        if (mod) {
            QStringList got;
            for (QAction* a : mod->actions())
                if (!a->isSeparator() && !a->text().remove('&').isEmpty()) got << a->text().remove('&');
            auto has = [&](const QString& frag) {
                for (const QString& s : got) if (s.contains(frag)) return true;
                return false;
            };
            bool modReachable = has(QStringLiteral("修剪")) &&
                                has(QStringLiteral("旋转")) &&
                                has(QStringLiteral("分解"));
            check(modReachable, QStringLiteral("【编辑>修改】仍可触达 修剪/旋转/分解 等命令"),
                  got.join(QStringLiteral("|")));
        } else check(false, QStringLiteral("【编辑>修改】子菜单存在"));
    } else check(false, QStringLiteral("【编辑】菜单存在"));

    // ---- 8. 【文件】菜单：多格式存取入口 + 绘图比例归属（同一功能不两处出现）----
    // 另存为支持的格式（草图 JSON / DXF / STEP / DWG）由保存对话框过滤器提供，
    // 但入口必须是「保存」，且绘图比例属于图纸属性，只应出现在【文件】菜单。
    QMenu* file = findTopMenu(win.menuBar(), QStringLiteral("文件"));
    check(file != nullptr, QStringLiteral("菜单栏存在【文件】菜单"));
    if (file) {
        QStringList fEntries;
        for (QAction* a : file->actions()) {
            if (a->isSeparator()) continue;
            const QString t = a->text().remove('&');
            if (!t.isEmpty()) fEntries << t;
        }
        const auto fHas = [&](const QString& frag) {
            for (const QString& s : fEntries) if (s.contains(frag)) return true;
            return false;
        };
        check(fHas(QStringLiteral("新建")),   QStringLiteral("【文件】含 新建"));
        check(fHas(QStringLiteral("打开")),   QStringLiteral("【文件】含 打开"));
        check(fHas(QStringLiteral("保存")),   QStringLiteral("【文件】含 保存（多格式另存为入口）"));
        check(fHas(QStringLiteral("DXF")),    QStringLiteral("【文件】含 DXF 导出入口"));
        check(fHas(QStringLiteral("绘图比例")), QStringLiteral("【文件】含 绘图比例（图纸属性）"),
              fEntries.join(QStringLiteral("|")));

        QStringList fSeen; bool fDup = false;
        for (const QString& t : fEntries)
            if (fSeen.contains(t)) fDup = true; else fSeen << t;
        check(!fDup, QStringLiteral("【文件】菜单无同名重复项"));
    }
    QMenu* view = findTopMenu(win.menuBar(), QStringLiteral("视图"));
    if (view) {
        bool scaleInView = false;
        for (QAction* a : view->actions())
            if (!a->isSeparator() && a->text().remove('&').contains(QStringLiteral("绘图比例")))
                scaleInView = true;
        check(!scaleInView, QStringLiteral("【视图】菜单不再重复提供 绘图比例（已归【文件】）"));
    }

    // ---- 9. 新建图纸：默认绘图比例必须是 1:1（需求：新建选好幅面后默认 1:1）----
    // 只校验对话框的默认值，不 exec()（模态会阻塞离屏测试）。
    {
        NewDrawingDialog dlg;
        check(std::fabs(dlg.selectedScaleRatio() - 1.0) < 1e-9,
              QStringLiteral("新建图纸对话框默认绘图比例 = 1:1"),
              QStringLiteral("实际=%1").arg(dlg.selectedScaleRatio()));
        // 默认幅面也要有值，否则"选好图纸"这一步无从谈起
        const SheetSpec spec = dlg.selectedSpec();
        check(spec.width > 0.0 && spec.height > 0.0,
              QStringLiteral("新建图纸对话框默认幅面有效（带默认选项）"),
              QStringLiteral("%1x%2").arg(spec.width).arg(spec.height));
    }

    // ---- 10. 单一真源一致性：每个 UI 子模式 id 都在 ToolRegistry 注册有工具 ----
    // 下拉/菜单子模式由 MainWindow::ToolSubMode 定义（zh 长名 + id），但真正执行是
    // 靠 model_->setActiveTool(id) 在 ToolRegistry 找到对应处理器。若某 id 只在
    // ToolSubMode 里写了却没注册，命令会「点了没反应」。这一节把 id 收口到
    // ToolRegistry::instance() 比对，确保 UI 显示名单与工具实现名单一致。
    {
        const QStringList ids = MainWindow::allSubModeToolIds();
        check(!ids.isEmpty(), QStringLiteral("枚举到子模式工具 id（%1 个，去重后）").arg(ids.size()));
        const auto& reg = ToolRegistry::instance();
        QStringList missing;
        for (const QString& id : ids)
            if (reg.find(id) == nullptr) missing << id;
        check(missing.isEmpty(),
              QStringLiteral("所有 UI 子模式 id 均在 ToolRegistry 注册"),
              missing.isEmpty() ? QString() :
              QStringLiteral("未注册: %1").arg(missing.join(QStringLiteral(", "))));
    }

    qInfo().noquote() << (g_fail == 0 ? QStringLiteral("RESULT: PASS") : QStringLiteral("RESULT: FAIL"));
    return g_fail == 0 ? 0 : 1;
}
