#include "ai/NlAssistant.h"

#include "core/CommandInterpreter.h"
#include "core/CommandCatalog.h"
#include "core/Element.h"
#include "core/SketchModel.h"
#include "ai/tooldefs.h"
#include "ai/NlCommandMapper.h"   // 仅用于解码「模型输出」的结构化命令（兼容非 function-calling 后端）
#include "core/standards/StandardsDB.h" // 标准知识库：query_standards 工具数据源（与命令/面板共用单例）

#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace SketchCAD {

const QStringList& NlAssistant::destructiveCommands() {
    static const QStringList s = {
        QStringLiteral("delete"),  QStringLiteral("erase"),
        QStringLiteral("trim"),    QStringLiteral("extend"),
        QStringLiteral("purge"),   QStringLiteral("overkill"),
        QStringLiteral("break"),   QStringLiteral("explode"),
        QStringLiteral("undo")
    };
    return s;
}

NlAssistant::NlAssistant(CommandInterpreter* interp, LlmProvider* provider,
                         AppContextProvider* ctx, SketchModel* model, QObject* parent)
    : QObject(parent), interp_(interp), provider_(provider), ctx_(ctx), model_(model) {
    LlmMessage sys;
    sys.role = LlmRole::System;
    sys.content = buildSystemPrompt();
    history_.append(sys);
}

void NlAssistant::sendUserMessage(const QString& text) {
    if (busy_) return;
    busy_ = true;
    aborted_ = false;
    step_ = 0;
    LlmMessage u;
    u.role = LlmRole::User;
    u.content = text;
    history_.append(u);

    // 用户原文原样进入模型：语义理解、参数抽取、命令串生成与结果说明
    // 全部由 LlmProvider 背后的模型完成。此处不得存在任何本地规则映射，
    // 也不得有「绕过模型直接执行」的快捷路径。
    runTurn();
}

void NlAssistant::runTurn() {
    if (aborted_) { finalize({}, QStringLiteral("已取消")); return; }
    // LLM 是核心处理环节，缺后端必须显式报错，绝不回落到本地规则。
    if (provider_ == nullptr) {
        finalize({}, QStringLiteral("未配置 LLM 后端（工具 → 选项 → AI 助手）"));
        return;
    }
    if (interp_ == nullptr) { finalize({}, QStringLiteral("命令解释器未正确初始化")); return; }
    if (step_ >= kMaxSteps) { finalize({}, QStringLiteral("达到最大对话步数，已停止")); return; }
    ++step_;
    accContent_.clear();
    accToolCalls_.clear();
    // 每轮刷新 system 上下文（选择集 / 图层等随操作变化）
    if (!history_.isEmpty() && history_.first().role == LlmRole::System)
        history_.first().content = buildSystemPrompt();

    const QVector<LlmTool> tools = NlTools::schemas();
    provider_->chat(history_, tools, options_,
        [this](const QString& d) { accContent_ += d; emit tokenDelta(d); },
        [this](const QVector<LlmToolCall>& c) { accToolCalls_ = c; },
        [this](const QString& err) { handleDone(err); });
}

void NlAssistant::handleDone(const QString& err) {
    if (!err.isEmpty()) { finalize({}, err); return; }

    QVector<LlmToolCall> calls = accToolCalls_;
    // 兼容路径：部分本地模型/网关不产生 function-calling，改以结构化 JSON
    // 文本给出命令对象。解码的对象是【模型输出】，等价于一次工具调用；
    // 命令内容仍完全由模型决定，本地只做协议解码。
    if (calls.isEmpty()) {
        const NlParseResult r = NlCommandMapper::parse(accContent_);
        if (r.ok && !r.commandLine.isEmpty()) {
            LlmToolCall tc;
            tc.id = QStringLiteral("model_text_%1").arg(step_);
            tc.name = QStringLiteral("runSketchCommand");
            tc.arguments = QJsonObject{{ QStringLiteral("line"), r.commandLine }};
            calls.append(tc);
        }
    }

    LlmMessage assistant;
    assistant.role = LlmRole::Assistant;
    assistant.content = accContent_;
    assistant.toolCalls = calls;   // 与后续 Tool 消息配对，保证对话协议合法
    history_.append(assistant);

    if (calls.isEmpty()) {
        // 无工具调用 = 模型生成的最终文本回复（不是本地拼装的模板文案）
        finalize(accContent_, QString());
    } else {
        processToolCalls(calls);
    }
}

void NlAssistant::processToolCalls(const QVector<LlmToolCall>& calls) {
    for (const LlmToolCall& call : calls) {
        if (call.name == QStringLiteral("runSketchCommand")) {
            const QString line = call.arguments.value(QStringLiteral("line")).toString().trimmed();
            if (isDestructive(line)) {
                pendingCall_ = call;
                pendingCommand_ = line;
                emit approvalRequested(line);
                return;   // 挂起，等待用户审批
            }
            const bool ok = interp_ ? interp_->execute(line) : false;
            emit commandExecuted(line, ok);
            LlmMessage toolMsg;
            toolMsg.role = LlmRole::Tool;
            toolMsg.content = ok ? QStringLiteral("命令已执行成功。")
                                 : QStringLiteral("命令执行失败（语法错误或参数非法）。");
            toolMsg.toolCallId = call.id;
            toolMsg.toolName = call.name;
            history_.append(toolMsg);
        } else {
            const QString result = runQueryTool(call.name, call.arguments);
            LlmMessage toolMsg;
            toolMsg.role = LlmRole::Tool;
            toolMsg.content = result;
            toolMsg.toolCallId = call.id;
            toolMsg.toolName = call.name;
            history_.append(toolMsg);
        }
    }
    runTurn();
}

void NlAssistant::approvePending() {
    if (pendingCommand_.isEmpty()) return;
    const QString line = pendingCommand_;
    const LlmToolCall call = pendingCall_;
    pendingCommand_.clear();
    pendingCall_ = LlmToolCall();
    const bool ok = interp_ ? interp_->execute(line) : false;
    emit commandExecuted(line, ok);
    LlmMessage toolMsg;
    toolMsg.role = LlmRole::Tool;
    toolMsg.content = ok ? QStringLiteral("命令已执行成功。") : QStringLiteral("命令执行失败。");
    toolMsg.toolCallId = call.id;
    toolMsg.toolName = call.name;
    history_.append(toolMsg);
    runTurn();
}

void NlAssistant::cancelPending() {
    if (pendingCommand_.isEmpty()) return;
    const LlmToolCall call = pendingCall_;
    pendingCommand_.clear();
    pendingCall_ = LlmToolCall();
    LlmMessage toolMsg;
    toolMsg.role = LlmRole::Tool;
    toolMsg.content = QStringLiteral("用户取消了该操作。");
    toolMsg.toolCallId = call.id;
    toolMsg.toolName = call.name;
    history_.append(toolMsg);
    runTurn();
}

void NlAssistant::stop() {
    aborted_ = true;
    if (busy_) finalize({}, QStringLiteral("已停止"));
}

void NlAssistant::finalize(const QString& assistantText, const QString& error) {
    busy_ = false;
    if (!assistantText.isEmpty()) emit assistantMessage(assistantText);
    emit finished(error.isEmpty(), error);
}

QString NlAssistant::buildSystemPrompt() const {
    QString p = QStringLiteral(
        "你是 SketchCAD 草图设计软件的内置自然语言助手。"
        "用户用自然语言描述绘图或操作意图；你必须先理解语义、抽取参数，"
        "再调用 runSketchCommand 工具给出完整命令串（参数 line）。"
        "命令语法与界面命令行完全一致，不要输出代码块，不要复述命令，直接调用工具。"
        "\n参数抽取示例：「给我绘制一个圆心在0，0、半径为20的圆」→ line=\"circle 0,0 20\"；"
        "「从 0,0 画一条到 100,0 的直线」→ line=\"line 0,0 100,0\"。"
        "\n执行结果会以 tool 消息回传给你，请据此用中文简要说明结果（结果文本也由你生成）。"
        "\n不确定语法时先用 getCommandHelp(name) 查询；需要图纸状态时用 "
        "getSelection / getLayers / listElements。");

    // 把真实命令语法喂给模型（唯一数据源 CommandCatalog::usage），
    // 让「自然语言 → 参数 → 命令串」这一步由模型依据事实完成，而非凭记忆猜参数顺序。
    // 只注入常用核心命令以控制上下文体积；其余命令模型可用 getCommandHelp 按需查询。
    static const char* const kCore[] = {
        "circle", "circle3p", "arc", "line", "line_h", "line_v", "line_angle",
        "rect", "poly", "point", "text", "delete", "undo", "redo", "clear",
        "select_all", "select_none", "zoom", "move", "rotate"
    };
    QStringList syntax;
    for (const char* nm : kCore) {
        const CommandInfo* info = CommandCatalog::find(QString::fromLatin1(nm));
        if (info && !info->usage.isEmpty())
            syntax.append(QStringLiteral("  %1").arg(info->usage));
    }
    if (!syntax.isEmpty())
        p += QStringLiteral("\n可用命令语法（usage，必须严格遵守）：\n") + syntax.join(QLatin1Char('\n'));

    if (ctx_) p += QStringLiteral("\n当前图纸上下文：") + ctx_->contextJson();
    return p;
}

bool NlAssistant::isDestructive(const QString& line) const {
    const QString cmd = line.section(QLatin1Char(' '), 0, 0).trimmed().toLower();
    return destructiveCommands().contains(cmd);
}

QString NlAssistant::runQueryTool(const QString& name, const QJsonObject& args) {
    if (name == QStringLiteral("getSelection"))
        return ctx_ ? ctx_->selectionSummary() : QStringLiteral("（无选择集信息）");

    if (name == QStringLiteral("listElements")) {
        if (!model_) return QStringLiteral("（无图纸）");
        QStringList parts;
        parts.append(QStringLiteral("共 %1 个图元。").arg(model_->elementCount()));
        QMap<QString, int> cnt;
        for (const Element& e : model_->elements())
            cnt[QString::fromUtf8(elementTypeName(e.type))] += 1;
        QStringList detail;
        for (auto it = cnt.begin(); it != cnt.end(); ++it)
            detail.append(QStringLiteral("%1×%2").arg(it.key()).arg(it.value()));
        if (!detail.isEmpty()) parts.append(detail.join(QStringLiteral("，")));
        return parts.join(QStringLiteral(" "));
    }

    if (name == QStringLiteral("getLayers"))
        return ctx_ ? ctx_->layersSummary() : QStringLiteral("（无图层信息）");

    if (name == QStringLiteral("getCommandHelp")) {
        const QString nm = args.value(QStringLiteral("name")).toString().trimmed().toLower();
        const CommandInfo* info = CommandCatalog::find(nm);
        if (!info) return QStringLiteral("未知命令：") + nm;
        QString out = info->name + QStringLiteral("（") + info->zhName + QStringLiteral("）\n") + info->summary;
        if (!info->usage.isEmpty()) out += QStringLiteral("\n用法：") + info->usage;
        if (!info->example.isEmpty()) out += QStringLiteral("\n示例：") + info->example;
        return out;
    }

    if (name == QStringLiteral("query_standards")) {
        const QString table = args.value(QStringLiteral("table")).toString().trimmed().toLower();
        const QString key = args.value(QStringLiteral("key")).toString().trimmed();
        if (table.isEmpty())
            return QStringLiteral("缺少参数 table（标准表名）。");
        const StandardsDB& db = StandardsDB::instance();
        const QJsonObject obj = db.tableObject(table);
        if (obj.isEmpty())
            return QStringLiteral("未知标准表：") + table +
                   QStringLiteral("（可用：") + db.allTables().join(QChar(' ')) + QStringLiteral("）");
        if (key.isEmpty()) {
            QJsonObject out;
            out[table] = obj;
            return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Indented));
        }
        const QJsonValue v = db.query(table, key);
        if (v.isUndefined() || v.isNull())
            return QStringLiteral("未找到键：") + key + QStringLiteral("（") + table +
                   QStringLiteral(" 可用键：") + QStringList(obj.keys()).join(QChar(' ')) + QStringLiteral("）");
        return table + QChar('.') + key + QStringLiteral(" = ") + db.queryString(table, key);
    }

    return QStringLiteral("未知工具：") + name;
}

} // namespace SketchCAD
