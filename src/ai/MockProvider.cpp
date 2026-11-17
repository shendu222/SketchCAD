#include "ai/MockProvider.h"
#include "core/I18n.h"

#include <QRegularExpression>
#include <QJsonObject>

namespace SketchCAD {

static bool hasToolResult(const QVector<LlmMessage>& m) {
    for (const auto& x : m)
        if (x.role == LlmRole::Tool) return true;
    return false;
}

void MockProvider::chat(const QVector<LlmMessage>& messages,
                        const QVector<LlmTool>& /*tools*/,
                        const LlmOptions& /*opts*/,
                        std::function<void(const QString&)> onDelta,
                        std::function<void(const QVector<LlmToolCall>&)> onToolCalls,
                        std::function<void(const QString&)> onDone) {
    // 第二回合：已产生工具结果 → 收尾文本（不再触发工具）
    if (hasToolResult(messages)) {
        onDelta(SketchCAD::I18n::t(QStringLiteral("已为你执行命令，可在画布查看结果。")));
        onToolCalls({});
        onDone("");
        return;
    }

    // 取最后一条用户消息
    QString text;
    for (int i = (int)messages.size() - 1; i >= 0; --i) {
        if (messages.at(i).role == LlmRole::User) { text = messages.at(i).content; break; }
    }

    // 意图：圆心(x,y) 半径 r 的圆
    QRegularExpression re("圆心\\s*[\\(（]?\\s*([+-]?\\d+(?:\\.\\d+)?)\\s*,\\s*([+-]?\\d+(?:\\.\\d+)?)\\s*[\\)）]?[^\\d]*?半径\\s*[rR=]?\\s*([+-]?\\d+(?:\\.\\d+)?)");
    const QRegularExpressionMatch m = re.match(text);
    if (m.hasMatch()) {
        const QString x = m.captured(1);
        const QString y = m.captured(2);
        const QString r = m.captured(3);
        const QString line = QStringLiteral("circle %1,%2 %3").arg(x, y, r);
        onDelta(SketchCAD::I18n::t(QStringLiteral("正在执行命令：")) + line);
        LlmToolCall tc;
        tc.id = QStringLiteral("call_mock_1");
        tc.name = QStringLiteral("runSketchCommand");
        tc.arguments = QJsonObject{{ QStringLiteral("line"), line }};
        onToolCalls({ tc });
        onDone("");
        return;
    }

    // 未识别意图：纯文本回复（不触发工具）
    onDelta(SketchCAD::I18n::t(QStringLiteral("（离线模拟）暂未理解该指令。可尝试：“画一个圆心(0,0)半径50的圆”。")));
    onToolCalls({});
    onDone("");
}

} // namespace SketchCAD
