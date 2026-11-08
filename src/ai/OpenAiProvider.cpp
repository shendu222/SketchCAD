#include "ai/OpenAiProvider.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QUrl>

namespace SketchCAD {

OpenAiProvider::OpenAiProvider(QObject* parent) : LlmProvider(parent) {
    nam_ = new QNetworkAccessManager(this);
}

QJsonArray OpenAiProvider::messagesToJson(const QVector<LlmMessage>& messages) {
    QJsonArray arr;
    for (const LlmMessage& m : messages) {
        QJsonObject o;
        switch (m.role) {
            case LlmRole::System:    o["role"] = "system";    break;
            case LlmRole::User:      o["role"] = "user";      break;
            case LlmRole::Assistant: o["role"] = "assistant"; break;
            case LlmRole::Tool:      o["role"] = "tool";       break;
        }
        if (m.role == LlmRole::Tool) {
            o["tool_call_id"] = m.toolCallId;
            o["name"] = m.toolName;
        }
        if (!m.toolCalls.isEmpty()) {
            QJsonArray tcs;
            for (const LlmToolCall& tc : m.toolCalls) {
                QJsonObject t;
                t["id"] = tc.id;
                t["type"] = "function";
                QJsonObject fn;
                fn["name"] = tc.name;
                fn["arguments"] = QString::fromUtf8(QJsonDocument(tc.arguments).toJson(QJsonDocument::Compact));
                t["function"] = fn;
                tcs.append(t);
            }
            o["tool_calls"] = tcs;
        }
        // 工具结果消息 content 必须存在；assistant 含 tool_calls 时 content 可空
        o["content"] = m.content.isNull() ? QJsonValue(QString("")) : QJsonValue(m.content);
        arr.append(o);
    }
    return arr;
}

QJsonArray OpenAiProvider::toolsToJson(const QVector<LlmTool>& tools) {
    QJsonArray arr;
    for (const LlmTool& t : tools) {
        QJsonObject o;
        o["type"] = "function";
        QJsonObject fn;
        fn["name"] = t.name;
        fn["description"] = t.description;
        QJsonObject params;
        QJsonDocument pd = QJsonDocument::fromJson(t.parametersJson.toUtf8());
        params = pd.isNull() ? QJsonObject{{"type", "object"}, {"properties", QJsonObject()}} : pd.object();
        fn["parameters"] = params;
        o["function"] = fn;
        arr.append(o);
    }
    return arr;
}

void OpenAiProvider::chat(const QVector<LlmMessage>& messages,
                          const QVector<LlmTool>& tools,
                          const LlmOptions& opts,
                          std::function<void(const QString&)> onDelta,
                          std::function<void(const QVector<LlmToolCall>&)> onToolCalls,
                          std::function<void(const QString&)> onDone) {
    QJsonObject req;
    req["model"] = opts.model;
    req["messages"] = messagesToJson(messages);
    if (!tools.isEmpty()) {
        req["tools"] = toolsToJson(tools);
        req["tool_choice"] = "auto";
    }
    req["stream"] = true;
    req["temperature"] = opts.temperature;
    req["max_tokens"] = opts.maxTokens;

    const QUrl url(opts.baseUrl + "/chat/completions");
    QNetworkRequest rq(url);
    rq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!opts.apiKey.isEmpty())
        rq.setRawHeader("Authorization", ("Bearer " + opts.apiKey).toUtf8());

    QNetworkReply* reply = nam_->post(rq, QJsonDocument(req).toJson(QJsonDocument::Compact));

    // 流式聚合状态（跨 readyRead 保留缓冲与 tool_call 碎片）
    struct Agg {
        QString content;
        QMap<int, QJsonObject> calls;   // index -> {id, name, args}
    };
    auto* agg = new Agg;

    QObject::connect(reply, &QNetworkReply::readyRead, [reply, agg, onDelta]() {
        QByteArray buf = reply->readAll();
        QByteArray prev = reply->property("ssebuf").toByteArray();
        prev.append(buf);
        int nl;
        while ((nl = prev.indexOf('\n')) >= 0) {
            QByteArray line = prev.left(nl).trimmed();
            prev.remove(0, nl + 1);
            if (line.isEmpty() || !line.startsWith("data:")) continue;
            QByteArray data = line.mid(5).trimmed();
            if (data == "[DONE]") continue;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) continue;
            QJsonObject o = doc.object();
            QJsonArray choices = o.value("choices").toArray();
            if (choices.isEmpty()) continue;
            QJsonObject delta = choices.at(0).toObject().value("delta").toObject();
            if (delta.contains("content")) {
                const QString c = delta.value("content").toString();
                if (!c.isEmpty()) { agg->content.append(c); onDelta(c); }
            }
            if (delta.contains("tool_calls")) {
                QJsonArray tcs = delta.value("tool_calls").toArray();
                for (const QJsonValue& v : tcs) {
                    QJsonObject tc = v.toObject();
                    const int idx = tc.value("index").toInt();
                    QJsonObject& slot = agg->calls[idx];
                    if (tc.contains("id")) slot["id"] = tc.value("id");
                    if (tc.contains("function")) {
                        QJsonObject fn = tc.value("function").toObject();
                        if (fn.contains("name")) slot["name"] = fn.value("name");
                        if (fn.contains("arguments"))
                            slot["args"] = slot.value("args").toString() + fn.value("arguments").toString();
                    }
                }
            }
        }
        reply->setProperty("ssebuf", prev);
    });

    QObject::connect(reply, &QNetworkReply::finished, [reply, agg, onToolCalls, onDone]() {
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            delete agg;
            onToolCalls({});
            onDone(err);
            return;
        }
        QVector<LlmToolCall> calls;
        for (auto it = agg->calls.begin(); it != agg->calls.end(); ++it) {
            LlmToolCall tc;
            tc.id = it.value().value("id").toString();
            tc.name = it.value().value("name").toString();
            const QString argsStr = it.value().value("args").toString();
            QJsonDocument ad = QJsonDocument::fromJson(argsStr.toUtf8());
            tc.arguments = ad.isNull() ? QJsonObject() : ad.object();
            calls.append(tc);
        }
        reply->deleteLater();
        delete agg;
        onToolCalls(calls);
        onDone("");
    });
}

} // namespace SketchCAD
