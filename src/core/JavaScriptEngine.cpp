#include "JavaScriptEngine.h"
#include "ScriptApi.h"

namespace SketchCAD {

ScriptResult JavaScriptEngine::execute(const QString& code, ScriptApi* api,
                                       std::function<void(const QString&)> logSink) {
    ScriptResult res;
    res.language = language();

    if (!api) {
        res.error = QStringLiteral("内部交互面未初始化");
        return res;
    }

    QJSEngine engine;
    // 把统一交互面作为全局对象 `app` 注入脚本作用域
    engine.globalObject().setProperty("app", engine.newQObject(api));

    // 全局 print(...) 转发到 app.log（后者进入日志/输出流），支持任意数量参数
    engine.evaluate(
        "function print() {"
        "  var a = [];"
        "  for (var i = 0; i < arguments.length; ++i) a.push(String(arguments[i]));"
        "  app.log(a.join(' '));"
        "}");

    // 捕获脚本 print/log 输出：累积到 res.output 并实时转发 logSink
    auto conn = QObject::connect(api, &ScriptApi::scriptOutput, &engine,
        [&](const QString& s) {
            res.output += s + "\n";
            if (logSink) logSink(s);
        });

    QJSValue r = engine.evaluate(code);
    if (r.isError()) {
        res.ok = false;
        QString atLine = r.property("lineNumber").isNumber()
            ? QString::number(r.property("lineNumber").toInt()) : QString();
        res.error = (atLine.isEmpty() ? QString()
                                      : QStringLiteral("行 %1: ").arg(atLine)) + r.toString();
    } else {
        res.ok = true;
        // 顶层表达式返回值也作为输出（便于脚本求值调试）
        if (!r.isUndefined())
            res.output += r.toString() + "\n";
    }

    QObject::disconnect(conn);
    return res;
}

} // namespace SketchCAD
