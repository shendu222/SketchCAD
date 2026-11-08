#pragma once
#include "ai/LlmProvider.h"
#include "ai/AppContextProvider.h"
#include <QObject>
#include <QVector>
#include <QString>

namespace SketchCAD {

class CommandInterpreter;
class SketchModel;

// 自然语言助手编排层：维护对话历史、构造 system prompt、驱动
// 「LLM ⇄ 工具 ⇄ 执行 ⇄ 反馈」循环。所有真正的建模动作都经
// interp_->execute(line) 落到现有命令体系（与菜单 / 命令行完全同源）。
class NlAssistant : public QObject {
    Q_OBJECT
public:
    explicit NlAssistant(CommandInterpreter* interp,
                         LlmProvider* provider,
                         AppContextProvider* ctx,
                         SketchModel* model,
                         QObject* parent = nullptr);

    void setOptions(const LlmOptions& o) { options_ = o; }
    const LlmOptions& options() const { return options_; }
    void setProvider(LlmProvider* p) { provider_ = p; }   // 选项变更时热替换后端（不重建助手/历史）

    void sendUserMessage(const QString& text);
    void approvePending();     // 用户批准被拦截的破坏性命令
    void cancelPending();      // 用户取消
    bool isBusy() const { return busy_; }
    void setModel(SketchModel* m) { model_ = m; }   // 多文档切换时重绑活动模型
    void stop();               // 停止当前对话（在下一轮循环入口生效）

signals:
    void tokenDelta(const QString& text);        // 流式文本增量
    void assistantMessage(const QString& text);  // 最终助手文本
    void approvalRequested(const QString& commandLine);
    void finished(bool ok, const QString& error);
    void commandExecuted(const QString& line, bool ok);

private:
    void runTurn();
    void handleDone(const QString& err);
    void processToolCalls(const QVector<LlmToolCall>& calls);
    void finalize(const QString& assistantText, const QString& error);
    QString buildSystemPrompt() const;
    bool isDestructive(const QString& line) const;
    QString runQueryTool(const QString& name, const QJsonObject& args);

    CommandInterpreter* interp_;
    LlmProvider* provider_;
    AppContextProvider* ctx_;
    SketchModel* model_;
    LlmOptions options_;
    QVector<LlmMessage> history_;
    QString pendingCommand_;
    LlmToolCall pendingCall_;
    bool busy_ = false;
    bool aborted_ = false;
    int step_ = 0;
    QString accContent_;
    QVector<LlmToolCall> accToolCalls_;
    static constexpr int kMaxSteps = 8;
    static const QStringList& destructiveCommands();
};

} // namespace SketchCAD
