#pragma once
#include "ScriptEngine.h"
#include <QJSEngine>

namespace SketchCAD {

// JavaScript 引擎（策略）：基于 Qt 内置 QJSEngine，零外部依赖，默认启用。
// 把 ScriptApi 作为全局对象 `app` 注入；另提供全局 `print(...)` 转发到日志回调。
class JavaScriptEngine : public ScriptEngine {
public:
    QString language()    const override { return QStringLiteral("javascript"); }
    QString displayName() const override { return QStringLiteral("JavaScript"); }
    bool available() const override { return true; }
    ScriptResult execute(const QString& code, ScriptApi* api,
                         std::function<void(const QString&)> logSink) override;
};

} // namespace SketchCAD
