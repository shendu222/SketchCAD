#pragma once
#include "ai/LlmProvider.h"

namespace SketchCAD {

// 离线规则后端：无需网络 / 密钥，便于无 key 联调与无头自测。
// 识别少量中文意图 → 产出 runSketchCommand 工具调用；已存在工具结果时给出收尾文本。
class MockProvider : public LlmProvider {
    Q_OBJECT
public:
    explicit MockProvider(QObject* parent = nullptr) : LlmProvider(parent) {}

    void chat(const QVector<LlmMessage>& messages,
              const QVector<LlmTool>& tools,
              const LlmOptions& opts,
              std::function<void(const QString&)> onDelta,
              std::function<void(const QVector<LlmToolCall>&)> onToolCalls,
              std::function<void(const QString&)> onDone) override;

    bool needsKey() const override { return false; }
    QString backendName() const override { return QStringLiteral("Mock(offline)"); }
};

} // namespace SketchCAD
