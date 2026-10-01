#pragma once
#include "ScriptEngine.h"

// 注意：PythonEngine 仅在开启 SCRIPTHOST_ENABLE_PYTHON 构建开关时编译（默认 OFF）。
// 开启后需系统提供 CPython 开发库（find_package(Python3 COMPONENTS Development)）。
#ifdef SCRIPTHOST_ENABLE_PYTHON
#include <Python.h>
#endif

namespace SketchCAD {

// Python 引擎（策略）：嵌入 CPython（需 SCRIPTHOST_ENABLE_PYTHON 构建开关，默认 OFF）。
// 把 ScriptApi 包装为 `sketch` 模块的函数，并重定向 stdout/stderr 到日志回调。
// 因此 Python 脚本里既能 `import sketch; sketch.add_line(...)`，也能用内置 print(...)。
class PythonEngine : public ScriptEngine {
public:
    QString language()    const override { return QStringLiteral("python"); }
    QString displayName() const override { return QStringLiteral("Python"); }
    bool available() const override;       // 仅当 CPython 可初始化时为真
    ScriptResult execute(const QString& code, ScriptApi* api,
                         std::function<void(const QString&)> logSink) override;
};

} // namespace SketchCAD
