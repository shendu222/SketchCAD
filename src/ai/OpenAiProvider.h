#pragma once
#include "ai/LlmProvider.h"
#include <QNetworkAccessManager>

namespace SketchCAD {

// OpenAI 兼容后端（/v1/chat/completions + SSE 流式）。
// 同时覆盖云端（DeepSeek / 通义千问 / 智谱 GLM / OpenAI：base_url + key）
// 与本地（Ollama / llama.cpp：base_url = 本地端口、key 可空）。
class OpenAiProvider : public LlmProvider {
    Q_OBJECT
public:
    explicit OpenAiProvider(QObject* parent = nullptr);

    void chat(const QVector<LlmMessage>& messages,
              const QVector<LlmTool>& tools,
              const LlmOptions& opts,
              std::function<void(const QString&)> onDelta,
              std::function<void(const QVector<LlmToolCall>&)> onToolCalls,
              std::function<void(const QString&)> onDone) override;

    bool needsKey() const override { return true; }
    QString backendName() const override { return QStringLiteral("OpenAI-compatible"); }

private:
    static QJsonArray messagesToJson(const QVector<LlmMessage>& messages);
    static QJsonArray toolsToJson(const QVector<LlmTool>& tools);
    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace SketchCAD
