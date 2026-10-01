#pragma once
#include <QObject>
#include <QStringList>
#include <memory>
#include <vector>
#include "ScriptEngine.h"
#include "ScriptApi.h"   // ScriptApi 在此被 unique_ptr 持有，需完整类型（析构需要）

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

// 脚本宿主编排器（策略模式上下文 Context）。
//
// 持有统一的内部对象交互面 ScriptApi 与各语言引擎策略表；运行时按语言标识选择引擎，
// 对调用方完全透明。默认注册 JavaScript 引擎；Python/VBA 在开启对应 CMake 开关时注册。
// 调用方只需 `host->runCode("javascript", src)` 或 `host->runFile(...)`，无需感知引擎差异。
class ScriptHost : public QObject {
    Q_OBJECT
public:
    explicit ScriptHost(SketchModel* model, CommandInterpreter* interp, QObject* parent = nullptr);

    ScriptResult runCode(const QString& language, const QString& code);
    ScriptResult runFile(const QString& language, const QString& filePath);
    QStringList availableLanguages() const;
    bool isLanguageAvailable(const QString& language) const;

    ScriptApi* api() const { return api_.get(); }

signals:
    void scriptOutput(const QString& msg);   // 脚本输出行（实时）
    void scriptError(const QString& msg);    // 脚本错误（实时）

private:
    void registerEngines();
    ScriptEngine* engineFor(const QString& language) const;

    std::unique_ptr<ScriptApi>                api_;
    std::vector<std::unique_ptr<ScriptEngine>> engines_;
};

} // namespace SketchCAD
