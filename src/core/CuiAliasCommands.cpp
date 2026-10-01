#include "Commands.h"
#include "AliasTable.h"
#include "CuiManager.h"
#include "SketchModel.h"      // ctx.model->log 需要完整 SketchModel 定义
#include <QFileInfo>

namespace SketchCAD {

// ---------------------------------------------------------------------------
// 命令别名（acad.pgp 兼容）
// ---------------------------------------------------------------------------
void registerAliasCommands(CommandRegistry& reg) {
    // alias <别名> <完整命令>：运行时新增/覆盖一条别名
    reg.registerExact("alias", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 3) {
                ctx.model->log(QStringLiteral("用法：alias <别名> <完整命令>"), "warn");
                return false;
            }
            QString a = ctx.toks[1].toLower();
            QString c = ctx.toks[2].toLower();
            AliasTable::instance().setAlias(a, c);
            ctx.model->log(QStringLiteral("已设置别名：%1 → %2").arg(a, c));
            return true;
        }));

    // aliasreload：从当前 .pgp 路径重新加载
    reg.registerExact("aliasreload", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            QString path = AliasTable::instance().filePath();
            if (path.isEmpty()) {
                ctx.model->log(QStringLiteral("未载入过别名文件，改为注入默认别名"), "warn");
                AliasTable::instance().clear();
                auto d = AliasTable::defaultAliases();
                for (auto it = d.begin(); it != d.end(); ++it)
                    AliasTable::instance().setAlias(it.key(), it.value());
                ctx.model->log(QStringLiteral("已载入 %1 条默认别名").arg(d.size()));
                return true;
            }
            QString err;
            if (!AliasTable::instance().load(path, &err)) {
                ctx.model->log(err, "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已重新加载别名文件：%1（%2 条）")
                               .arg(path).arg(AliasTable::instance().map().size()));
            return true;
        }));

    // aliasload <文件>：载入指定 .pgp
    reg.registerExact("aliasload", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：aliasload <文件>"), "warn");
                return false;
            }
            QString path = ctx.toks.mid(1).join(QChar(' '));
            QString err;
            if (!AliasTable::instance().load(path, &err)) {
                ctx.model->log(err, "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已加载别名文件：%1（%2 条）")
                               .arg(path).arg(AliasTable::instance().map().size()));
            return true;
        }));

    // aliassave [文件]：写回当前别名表
    reg.registerExact("aliassave", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            QString path = ctx.toks.size() > 1 ? ctx.toks.mid(1).join(QChar(' ')) : QString();
            QString err;
            if (!AliasTable::instance().save(path, &err)) {
                ctx.model->log(err, "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已保存别名文件：%1").arg(AliasTable::instance().filePath()));
            return true;
        }));

    // aliaslist：列出全部别名
    reg.registerExact("aliaslist", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            const auto& m = AliasTable::instance().map();
            if (m.isEmpty()) { ctx.model->log(QStringLiteral("当前没有别名")); return true; }
            ctx.model->log(QStringLiteral("命令别名（%1 条）：").arg(m.size()));
            for (auto it = m.begin(); it != m.end(); ++it)
                ctx.model->log(QStringLiteral("  %1 = %2").arg(it.key(), it.value()));
            return true;
        }));
}

// ---------------------------------------------------------------------------
// 界面定制（CUIx）：工具条 / 工作空间
// ---------------------------------------------------------------------------
void registerCuiCommands(CommandRegistry& reg) {
    // cuiload <文件>：载入 .cuix（含工具条与工作空间）
    reg.registerExact("cuiload", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：cuiload <文件>"), "warn");
                return false;
            }
            QString path = ctx.toks.mid(1).join(QChar(' '));
            QString err;
            if (!CuiManager::instance().load(path, &err)) {
                ctx.model->log(err, "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已加载界面配置：%1（工具条 %2 / 工作空间 %3）")
                               .arg(path)
                               .arg(CuiManager::instance().toolbarIds().size())
                               .arg(CuiManager::instance().workspaceIds().size()));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // cuisave [文件]：保存当前界面配置
    reg.registerExact("cuisave", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            QString path = ctx.toks.size() > 1 ? ctx.toks.mid(1).join(QChar(' ')) : QString();
            QString err;
            if (!CuiManager::instance().save(path, &err)) {
                ctx.model->log(err, "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已保存界面配置：%1").arg(CuiManager::instance().filePath()));
            return true;
        }));

    // wsnew <id> [标题]：以「全部已知工具条 + 全部内置菜单」为可见集新建工作空间
    reg.registerExact("wsnew", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：wsnew <id> [标题]"), "warn");
                return false;
            }
            QString id = ctx.toks[1].toLower();
            QString title = ctx.toks.size() > 2 ? ctx.toks.mid(2).join(QChar(' ')) : id;
            QStringList tbs = CuiManager::instance().builtinToolbars();
            for (const QString& t : CuiManager::instance().toolbarIds())
                if (!tbs.contains(t)) tbs << t;
            CuiManager::instance().addWorkspace(id, title, tbs,
                                               CuiManager::instance().builtinMenus());
            ctx.model->log(QStringLiteral("已新建工作空间：%1（%2 个工具条可见）").arg(title).arg(tbs.size()));
            return true;
        }));

    // wsload <id>：切换工作空间
    reg.registerExact("wsload", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：wsload <id>"), "warn");
                return false;
            }
            QString id = ctx.toks[1].toLower();
            if (!CuiManager::instance().hasWorkspace(id)) {
                ctx.model->log(QStringLiteral("工作空间不存在：%1").arg(id), "warn");
                return false;
            }
            CuiManager::instance().setCurrentWorkspace(id);
            ctx.model->log(QStringLiteral("已切换到工作空间：%1")
                               .arg(CuiManager::instance().workspace(id)->title));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // wslist：列出工作空间
    reg.registerExact("wslist", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            QStringList ids = CuiManager::instance().workspaceIds();
            if (ids.isEmpty()) { ctx.model->log(QStringLiteral("当前没有工作空间")); return true; }
            QString cur = CuiManager::instance().currentWorkspace();
            ctx.model->log(QStringLiteral("工作空间（%1 个，当前：%2）：")
                               .arg(ids.size()).arg(cur.isEmpty() ? QStringLiteral("（默认/全显）") : cur));
            QStringList titles = CuiManager::instance().workspaceTitles();
            for (int i = 0; i < ids.size(); ++i)
                ctx.model->log(QStringLiteral("  %1 = %2%3")
                                   .arg(ids[i], titles[i],
                                        ids[i] == cur ? QStringLiteral("  [当前]") : QString()));
            return true;
        }));

    // wsdel <id>：删除工作空间
    reg.registerExact("wsdel", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：wsdel <id>"), "warn");
                return false;
            }
            QString id = ctx.toks[1].toLower();
            if (!CuiManager::instance().removeWorkspace(id)) {
                ctx.model->log(QStringLiteral("工作空间不存在：%1").arg(id), "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已删除工作空间：%1").arg(id));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // tbnew <id> [标题]：新建空自定义工具条
    reg.registerExact("tbnew", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：tbnew <id> [标题]"), "warn");
                return false;
            }
            QString id = ctx.toks[1].toLower();
            QString title = ctx.toks.size() > 2 ? ctx.toks.mid(2).join(QChar(' ')) : id;
            if (CuiManager::instance().hasToolbar(id)) {
                ctx.model->log(QStringLiteral("工具条已存在：%1").arg(id), "warn");
                return false;
            }
            CuiManager::instance().addToolbar(id, title);
            ctx.model->log(QStringLiteral("已新建工具条：%1").arg(title));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // tbadd <工具条> <命令> [文字] [图标]：向工具条追加按钮
    reg.registerExact("tbadd", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 3) {
                ctx.model->log(QStringLiteral("用法：tbadd <工具条> <命令> [文字] [图标]"), "warn");
                return false;
            }
            QString tb = ctx.toks[1].toLower();
            CuiButton b;
            b.command = ctx.toks[2].toLower();
            b.label = ctx.toks.size() > 3 ? ctx.toks[3] : QString();
            b.icon = ctx.toks.size() > 4 ? ctx.toks[4] : QString();
            if (!CuiManager::instance().hasToolbar(tb)) CuiManager::instance().addToolbar(tb, tb);
            CuiManager::instance().addButton(tb, b);
            ctx.model->log(QStringLiteral("已向工具条 %1 添加按钮：%2").arg(tb, b.command));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // tbdel <工具条>：删除整个工具条
    reg.registerExact("tbdel", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 2) {
                ctx.model->log(QStringLiteral("用法：tbdel <工具条>"), "warn");
                return false;
            }
            QString tb = ctx.toks[1].toLower();
            if (!CuiManager::instance().removeToolbar(tb)) {
                ctx.model->log(QStringLiteral("工具条不存在：%1").arg(tb), "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已删除工具条：%1").arg(tb));
            CuiManager::instance().requestRefresh();
            return true;
        }));

    // tbrm <工具条> <序号>：删除工具条中第 N 个按钮（从 0 起）
    reg.registerExact("tbrm", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) {
            if (ctx.toks.size() < 3) {
                ctx.model->log(QStringLiteral("用法：tbrm <工具条> <序号>"), "warn");
                return false;
            }
            QString tb = ctx.toks[1].toLower();
            bool ok = false;
            int idx = ctx.toks[2].toInt(&ok);
            if (!ok || !CuiManager::instance().removeButton(tb, idx)) {
                ctx.model->log(QStringLiteral("删除按钮失败（工具条：%1，序号：%2）").arg(tb, ctx.toks[2]), "warn");
                return false;
            }
            ctx.model->log(QStringLiteral("已从工具条 %1 删除第 %2 个按钮").arg(tb).arg(idx));
            CuiManager::instance().requestRefresh();
            return true;
        }));
}

} // namespace SketchCAD
