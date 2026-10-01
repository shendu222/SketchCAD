#pragma once
#include <QString>
#include <functional>
#include <memory>

namespace SketchCAD {

class ScriptApi;

// 脚本执行结果（统一返回结构，供所有语言引擎复用）
struct ScriptResult {
    bool    ok      = false;   // 是否成功执行（无未捕获异常）
    QString output;            // 脚本产生的输出（print/log 累积）
    QString error;             // 异常/错误信息（若有）
    QString language;          // 引擎语言标识（"javascript"/"python"/"vba"）
};

// 脚本引擎抽象基类（策略模式 Strategy）。
// 每种语言对应一个具体策略（JavaScriptEngine / PythonEngine / VbaEngine），
// ScriptHost 作为上下文持有策略表，运行时按 language 选择引擎，对调用方完全透明。
// 新增语言只需派生一个 ScriptEngine 并在 ScriptHost::registerEngines 注册。
class ScriptEngine {
public:
    virtual ~ScriptEngine() = default;

    virtual QString language()    const = 0;   // 语言标识（小写）
    virtual QString displayName() const = 0;   // 人类可读名称
    virtual bool available() const { return true; }  // 引擎是否可用（外部运行时缺失则 false）

    // 执行脚本源码。
    //   code    —— 脚本源码
    //   api     —— 程序内部对象交互面（增删图元/执行命令/写日志）
    //   logSink —— 输出/日志回调（脚本 print/log 实时转发给宿主）
    virtual ScriptResult execute(const QString& code, ScriptApi* api,
                                 std::function<void(const QString&)> logSink) = 0;
};

} // namespace SketchCAD
