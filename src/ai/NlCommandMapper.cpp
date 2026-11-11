#include "ai/NlCommandMapper.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace SketchCAD {

// 解码「模型输出」中的结构化命令对象。主链路是 LlmProvider 的 tool_calls，
// 这里只兼容「模型以 JSON 文本给出命令」的后端形态。
// 刻意不做任何文本归一化 / 关键词 / 正则抽取：一旦那样做，就又变成用本地
// 规则替代模型推理了。解析范围严格限定在模型回复这一侧。
NlParseResult NlCommandMapper::parse(const QString& modelOutput) {
    NlParseResult r;
    const QString s = modelOutput.trimmed();
    if (s.isEmpty()) return r;

    // 模型常把 JSON 包在 ```json ... ``` 里，或在 JSON 前后附加一句说明。
    // 仅定位 JSON 对象的边界，不改写其中任何内容。
    const int begin = s.indexOf(QLatin1Char('{'));
    const int end   = s.lastIndexOf(QLatin1Char('}'));
    if (begin < 0 || end <= begin) return r;

    const QJsonDocument doc = QJsonDocument::fromJson(s.mid(begin, end - begin + 1).toUtf8());
    if (!doc.isObject()) return r;

    const QJsonObject o = doc.object();
    const QString line = o.value(QStringLiteral("command")).toString().trimmed();
    if (line.isEmpty()) return r;

    r.ok = true;
    r.commandLine = line;
    r.intent = line.section(QLatin1Char(' '), 0, 0).toLower();
    r.confidence = o.value(QStringLiteral("confidence")).toDouble(1.0);
    r.explanation = o.value(QStringLiteral("explanation")).toString();
    return r;
}

} // namespace SketchCAD
