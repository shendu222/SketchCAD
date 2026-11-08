#include "ai/NlAssistant.h"

#include "core/CommandInterpreter.h"
#include "core/CommandCatalog.h"
#include "core/Element.h"
#include "core/SketchModel.h"
#include "ai/tooldefs.h"

#include <QJsonObject>
#include <QMap>

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
    runTurn();
}

void NlAssistant::runTurn() {
    if (aborted_) { finalize({}, QStringLiteral("已取消")); return; }
    if (provider_ == nullptr || interp_ == nullptr) { finalize({}, QStringLiteral("助手未正确初始化")); return; }
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
    LlmMessage assistant;
    assistant.role = LlmRole::Assistant;
    assistant.content = accContent_;
    assistant.toolCalls = accToolCalls_;
    history_.append(assistant);
    if (accToolCalls_.isEmpty()) {
        finalize(accContent_, QString());
    } else {
        processToolCalls(accToolCalls_);
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
        "你是 SketchCAD 草图设计软件的内置自然语言助手。请尽量复用现有命令体系："
        "用 runSketchCommand 工具执行命令（参数 line 为完整命令串，语法与界面命令行一致，"
        "例如 circle 0,0 50、line 0,0 100,0、delete last）。不确定语法时先用 getCommandHelp 查询。"
        "需要了解当前图形状态时用 getSelection / getLayers / listElements。不要输出代码块，直接调用工具完成用户意图。");
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

    return QStringLiteral("未知工具：") + name;
}

} // namespace SketchCAD
