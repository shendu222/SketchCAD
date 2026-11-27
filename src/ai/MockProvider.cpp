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

    // 意图：标准件。必须先于任何通用几何判定，避免“T形槽用螺栓 M10X100”
    // 被拆成“T形槽 + 螺栓”的分步绘图流程。对应【绘图】-【标准件】。
    {
        QRegularExpression specRe(QStringLiteral("[Mm]\\s*(\\d+(?:\\.\\d+)?)\\s*[xX×]\\s*(\\d+(?:\\.\\d+)?)"));
        const QRegularExpressionMatch sm = specRe.match(text);
        const bool stdName = text.contains(QStringLiteral("螺栓")) || text.contains(QStringLiteral("螺钉"))
            || text.contains(QStringLiteral("螺母")) || text.contains(QStringLiteral("垫圈"))
            || text.contains(QStringLiteral("垫片")) || text.contains(QStringLiteral("销"))
            || text.contains(QStringLiteral("键")) || text.contains(QStringLiteral("轴承"))
            || text.contains(QStringLiteral("铆钉")) || text.contains(QStringLiteral("挡圈"));
        if (sm.hasMatch() && stdName) {
            const QString spec = QStringLiteral("M%1X%2").arg(sm.captured(1), sm.captured(2));
            QString name = text.left(sm.capturedStart(0));
            name.remove(QRegularExpression(QStringLiteral("^(绘制|画|生成|插入|添加|创建|调用)(一个|个|一幅|一张)?(的)?")));
            name.remove(QRegularExpression(QStringLiteral("[\\s，,。：:]")));
            if (name.isEmpty()) name = QStringLiteral("标准件");
            const QString line = QStringLiteral("stdpart %1 %2").arg(name, spec);
            onDelta(SketchCAD::I18n::t(QStringLiteral("正在执行命令：")) + line);
            LlmToolCall tc;
            tc.id = QStringLiteral("call_mock_stdpart_1");
            tc.name = QStringLiteral("runSketchCommand");
            tc.arguments = QJsonObject{{ QStringLiteral("line"), line }};
            onToolCalls({ tc });
            onDone("");
            return;
        }
    }

    // 意图：椭圆。必须先于圆判定，避免“椭圆 + 圆心/半径”描述被圆形规则抢走。
    const bool wantsEllipse = text.contains(QStringLiteral("椭圆"))
                              || text.contains(QStringLiteral("ellipse"), Qt::CaseInsensitive);
    if (wantsEllipse) {
        QString x = QStringLiteral("0"), y = QStringLiteral("0");
        double a = 50.0, b = 25.0, rot = 0.0;

        QRegularExpression pointRe(QStringLiteral("([+-]?\\d+(?:\\.\\d+)?)\\s*[,，]\\s*([+-]?\\d+(?:\\.\\d+)?)"));
        const QRegularExpressionMatch pm = pointRe.match(text);
        if (pm.hasMatch()) { x = pm.captured(1); y = pm.captured(2); }

        auto axis = [&text](const QString& label, double fallback) {
            QRegularExpression re(label + QStringLiteral("(?:半)?轴?\\s*[=:：]?\\s*([+-]?\\d+(?:\\.\\d+)?)"));
            const QRegularExpressionMatch m = re.match(text);
            return m.hasMatch() ? m.captured(1).toDouble() : fallback;
        };
        a = axis(QStringLiteral("长"), a);
        b = axis(QStringLiteral("短"), b);
        if (b <= 0.0) b = a * 0.5;

        QRegularExpression rotRe(QStringLiteral("旋转\\s*([+-]?\\d+(?:\\.\\d+)?)"));
        const QRegularExpressionMatch rm = rotRe.match(text);
        if (rm.hasMatch()) rot = rm.captured(1).toDouble();

        const QString line = QStringLiteral("ellipse %1,%2 %3 %4 %5")
                                 .arg(x, y)
                                 .arg(a, 0, 'f', 2).arg(b, 0, 'f', 2).arg(rot, 0, 'f', 0);
        onDelta(SketchCAD::I18n::t(QStringLiteral("正在执行命令：")) + line);
        LlmToolCall tc;
        tc.id = QStringLiteral("call_mock_ellipse_1");
        tc.name = QStringLiteral("runSketchCommand");
        tc.arguments = QJsonObject{{ QStringLiteral("line"), line }};
        onToolCalls({ tc });
        onDone("");
        return;
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
