#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <functional>

namespace SketchCAD {

// 工具调用（由模型在一次回复中发出，流式多段碎片在服务端聚合为一条）
struct LlmToolCall {
    QString id;                 // 本次 tool_call 的唯一 id（Tool 结果消息回带）
    QString name;               // 函数名（如 runSketchCommand）
    QJsonObject arguments;      // 已解析的参数对象
};

enum class LlmRole { System, User, Assistant, Tool };

struct LlmMessage {
    LlmRole role = LlmRole::User;
    QString content;                 // 文本内容（各角色通用）
    QString toolCallId;              // Tool 角色：对应的 tool_call id
    QString toolName;                // Tool 角色：函数名
    QVector<LlmToolCall> toolCalls;  // Assistant 角色：本轮发出的工具调用
};

struct LlmTool {
    QString name;
    QString description;
    QString parametersJson;          // JSON Schema 字符串（"type":"object", properties...）
};

struct LlmOptions {
    QString baseUrl;                 // /v1 前缀之前的 base（如 https://api.openai.com/v1）
    QString apiKey;
    QString model;
    int maxTokens = 1024;
    double temperature = 0.2;
};

// 抽象 LLM 后端。chat() 异步：
//   onDelta    : 流式文本增量（可能多次）
//   onToolCalls: 聚合后的工具调用（一次性）
//   onDone     : 结束（error 非空表示失败）
// 调用方（NlAssistant）在 onDone 内继续工具调用循环。云端 / 本地 / 离线 Mock 共用此接口。
class LlmProvider : public QObject {
    Q_OBJECT
public:
    explicit LlmProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~LlmProvider() override {}

    virtual void chat(const QVector<LlmMessage>& messages,
                      const QVector<LlmTool>& tools,
                      const LlmOptions& opts,
                      std::function<void(const QString&)> onDelta,
                      std::function<void(const QVector<LlmToolCall>&)> onToolCalls,
                      std::function<void(const QString&)> onDone) = 0;

    // 是否需要密钥（本地 / Mock 返回 false，便于无 key 联调）
    virtual bool needsKey() const { return true; }
    // 人类可读后端名（UI 展示 / 自测断言）
    virtual QString backendName() const = 0;
};

} // namespace SketchCAD
