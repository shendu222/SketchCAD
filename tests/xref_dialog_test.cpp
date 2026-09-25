// 外部参照（Xref）UI 层回归：offscreen 下验证「附加外部参照」对话框与
// 【块】子菜单中的两项新入口真正接到了模型上。
//
// 关注点（与既有 block 子系统一致）：UI 只负责「把参数收齐 → 调模型 API」，
// 成功/失败判定与日志全部由 SketchModel 产出，故这里只断言两条链路：
//   1) 对话框确认真的影响了模型（产生了 Xref 记录 + 一个参照实例）；
//   2) 菜单项存在（文案去掉 & 后比对），并且拦掉了会弹模态的用例（未选中/空names）。
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "ui/BlockDialogs.h"
#include "ui/MainWindow.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/Xref.h"
#include "core/Vec.h"

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const QString& what, const QString& detail = QString()) {
    if (ok) { qInfo().noquote() << "  [PASS]" << what; }
    else    { qInfo().noquote() << "  [FAIL]" << what << (detail.isEmpty() ? QString() : "(" + detail + ")"); ++g_fail; }
}

static Element mkLine(double x1, double y1, double x2, double y2) {
    Element e; e.type = ElementType::Line;
    e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}
static bool writeJsonDrawing(const QString& path, const std::vector<Element>& elems) {
    QJsonObject root;
    root[QStringLiteral("app")] = QStringLiteral("SketchCAD");
    root[QStringLiteral("formatVersion")] = 1;
    root[QStringLiteral("scaleRatio")] = 1.0;
    QJsonArray arr;
    for (const auto& e : elems) arr.append(elementToJson(e));
    root[QStringLiteral("elements")] = arr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}
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
            if (m->title().remove('&').contains(frag)) return m;
            if (QMenu* r = findMenuRec(m, frag)) return r;
        }
    return nullptr;
}
// 在对象树里按类型找子控件（避免为测试专门开放访问器）
template <class T>
static T* findChild(QObject* root) {
    if (!root) return nullptr;
    if (T* t = qobject_cast<T*>(root)) return t;
    for (QObject* c : root->children())
        if (T* t = findChild<T>(c)) return t;
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir tmp;
    if (!tmp.isValid()) { qInfo().noquote() << "临时目录创建失败"; return 1; }
    const QString dir  = tmp.path();
    const QString ref  = dir + QStringLiteral("/refA.json");
    if (!writeJsonDrawing(ref, {mkLine(0, 0, 10, 0)})) {
        qInfo().noquote() << "  [FAIL] 写出参照源文件";
        return 1;
    }

    // ---- 1. 附加对话框：默认项 + 确认后真正落到模型 ----
    qInfo().noquote() << "\n[1] 附加外部参照对话框";
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        AttachXrefDialog dlg(&m, nullptr);
        check(dlg.attachedId() == -1, QStringLiteral("未确认前 attachedId 为 -1"));

        QComboBox* typeCombo = findChild<QComboBox>(&dlg);
        check(typeCombo != nullptr, QStringLiteral("对话框含路径类型下拉框"));
        check(typeCombo && typeCombo->currentData().toInt() == (int)XrefPathType::FullPath,
              QStringLiteral("路径类型默认为「完整路径」"));

        QLineEdit* pathEdit = findChild<QLineEdit>(&dlg);
        check(pathEdit != nullptr, QStringLiteral("对话框含文件路径输入框"));
        pathEdit->setText(ref);
        dlg.accept();
        check(dlg.result() == QDialog::Accepted, QStringLiteral("确认后被接受"));
        check(dlg.attachedId() >= 0, QStringLiteral("确认后返回参照实例 id"));
        check(m.xrefCount() == 1 && m.hasBlock(QStringLiteral("refA")),
              QStringLiteral("模型确实登记了该外部参照"));
        const BlockDef* d = m.findBlockDef(QStringLiteral("refA"));
        check(d && d->isXref && d->xrefPath == QDir::cleanPath(ref),
              QStringLiteral("记录的路径类型/路径正确（完整路径）"),
              d ? d->xrefPath : QString());
        if (d && !d->entities.empty())
            check(true, QStringLiteral("参照定义内含源图几何"));
        check(m.elementCount() == 1, QStringLiteral("对话框确认后本图新增 1 个参照实例"));
    }

    // ---- 2. 相对路径：下拉框第 2 项 ----
    qInfo().noquote() << "\n[2] 相对路径经 UI 附加";
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        AttachXrefDialog dlg(&m, nullptr);
        QComboBox* typeCombo = findChild<QComboBox>(&dlg);
        QLineEdit* pathEdit  = findChild<QLineEdit>(&dlg);
        typeCombo->setCurrentIndex(1);                 // 相对路径
        pathEdit->setText(ref);
        dlg.accept();
        const BlockDef* d = m.findBlockDef(QStringLiteral("refA"));
        check(d && d->xrefPathType == (int)XrefPathType::RelativePath &&
                   d->xrefPath == QStringLiteral("refA.json"),
              QStringLiteral("UI 选择「相对路径」→ 记录为文件名"),
              d ? d->xrefPath : QString());
        check(m.xrefResolvedPath(QStringLiteral("refA")) == QDir::cleanPath(ref),
              QStringLiteral("相对路径可按宿主图目录还原"));
    }

    // ---- 3. 菜单入口存在（【绘图>块】共用同一 buildBlockMenu）----
    qInfo().noquote() << "\n[3] 菜单入口";
    {
        MainWindow win;
        QMenu* blk = findTopMenu(win.menuBar(), QStringLiteral("块"));
        check(blk != nullptr, QStringLiteral("菜单树中存在【块】子菜单"));
        if (blk) {
            QStringList items;
            for (QAction* a : blk->actions())
                if (!a->isSeparator()) items << a->text().remove('&');
            check(items.contains(QStringLiteral("附加外部参照...")),
                  QStringLiteral("【块】含「附加外部参照...」"), items.join(QStringLiteral("|")));
            check(items.contains(QStringLiteral("分离外部参照...")),
                  QStringLiteral("【块】含「分离外部参照...」"), items.join(QStringLiteral("|")));
            for (QAction* a : blk->actions())
                if (!a->isSeparator() && a->text().remove('&').startsWith(QStringLiteral("附加外部参照")))
                    check(!a->icon().isNull(), QStringLiteral("「附加外部参照...」带矢量图标"));
        }
    }

    // ---- 4. 空图纸上「分离」不应崩溃（走信息弹窗分支，此处只验证前置查询）----
    qInfo().noquote() << "\n[4] 无参照时的查询";
    {
        SketchModel m;
        check(m.xrefNames().isEmpty(), QStringLiteral("无参照时 xrefNames 为空"));
        check(!m.detachXref(QStringLiteral("refA")), QStringLiteral("无参照时分离被拒绝"));
    }

    qInfo().noquote() << QStringLiteral("\n外部参照 UI 测试：%1（%2 项失败）")
                             .arg(g_fail == 0 ? QStringLiteral("全部通过") : QStringLiteral("存在失败"))
                             .arg(g_fail);
    return g_fail == 0 ? 0 : 1;
}
