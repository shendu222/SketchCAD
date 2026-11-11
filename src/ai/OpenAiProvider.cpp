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

    // 错误诊断所需上下文：本次是否带了密钥、目标服务地址。
    const bool hadKey = !opts.apiKey.trimmed().isEmpty();
    const QString baseUrl = opts.baseUrl;

    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, agg, onToolCalls, onDone, hadKey, baseUrl]() {
        // ⚠️ Qt 对 HTTP 4xx/5xx 不一定设置 reply->error()，只看 error() 会把
        //    401/402/404 误判成「成功但无内容」，用户只看到空回复。必须同时看状态码。
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray pending = reply->property("ssebuf").toByteArray();
        QByteArray body = reply->readAll();
        if (body.isEmpty()) body = pending;
        else if (!pending.isEmpty()) body = pending + body;

        // 非 SSE 的错误响应体通常是 {"error":{"message":"..."}}，把原文带给用户。
        QString detail;
        if (!body.isEmpty()) {
            const QJsonDocument d = QJsonDocument::fromJson(body);
            if (d.isObject()) {
                const QJsonValue e = d.object().value(QStringLiteral("error"));
                if (e.isObject()) detail = e.toObject().value(QStringLiteral("message")).toString();
                else if (e.isString()) detail = e.toString();
            }
            if (detail.isEmpty()) detail = QString::fromUtf8(body.left(300)).trimmed();
        }

        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            QString err;
            if (status == 401) {
                err = hadKey
                    ? QStringLiteral("HTTP 401 认证失败：API 密钥无效，或服务地址与密钥不匹配。\n"
                                    "请检查「服务地址」是否为该密钥所属厂商的地址，密钥是否复制完整、账户是否有效。")
                    : QStringLiteral("HTTP 401 认证失败：当前未填写 API 密钥（该字段为空）。\n"
                                    "请在「工具 → 选项 → AI 助手」填入密钥，并确认后端类型为云端/本地。");
                err += QStringLiteral("\n提示：若用同一密钥独立请求（curl）返回 402 或 200，"
                                     "说明本实例可能未加载最新密钥，请完全退出程序后重开。");
            } else if (status == 402) {
                err = QStringLiteral("HTTP 402：API 密钥有效、认证已通过，但账户余额不足。\n"
                                    "请到模型厂商控制台充值后再试（DeepSeek：https://platform.deepseek.com/top_up）；"
                                    "充值后无需改动密钥。");
            } else if (status == 404) {
                err = QStringLiteral("HTTP 404：服务地址或模型名有误。请检查「服务地址」路径与「模型」id 是否为该服务实际提供的值。");
            } else if (status == 429) {
                err = QStringLiteral("HTTP 429：请求速率达到上限，请稍后重试。");
            } else if (status >= 500) {
                err = QStringLiteral("HTTP %1：模型服务暂时不可用，请稍后重试。").arg(status);
            } else if (status >= 400) {
                err = QStringLiteral("HTTP %1：请求被拒绝。").arg(status);
            } else {
                err = QStringLiteral("网络请求失败：") + reply->errorString();
            }
            if (!detail.isEmpty()) err += QStringLiteral("\n服务端原文：") + detail;
            if (status >= 400 && reply->error() != QNetworkReply::NoError)
                err += QStringLiteral("\n底层信息：") + reply->errorString();
            if (!baseUrl.isEmpty()) err += QStringLiteral("\n当前服务地址：") + baseUrl;

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
