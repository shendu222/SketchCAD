#include "ScriptHost.h"
#include "ScriptApi.h"
#include "JavaScriptEngine.h"
#ifdef SCRIPTHOST_ENABLE_PYTHON
#include "PythonEngine.h"
#endif
#ifdef SCRIPTHOST_ENABLE_VBA
#include "VbaEngine.h"
#endif

#include <QFile>
#include <QTextStream>
#include <QStringConverter>

namespace SketchCAD {

ScriptHost::ScriptHost(SketchModel* model, CommandInterpreter* interp, QObject* parent)
    : QObject(parent) {
    api_ = std::make_unique<ScriptApi>(model, interp, this);
    registerEngines();
}

void ScriptHost::registerEngines() {
    engines_.push_back(std::make_unique<JavaScriptEngine>());
#ifdef SCRIPTHOST_ENABLE_PYTHON
    engines_.push_back(std::make_unique<PythonEngine>());
#endif
#ifdef SCRIPTHOST_ENABLE_VBA
    engines_.push_back(std::make_unique<VbaEngine>());
#endif
}

ScriptEngine* ScriptHost::engineFor(const QString& language) const {
    for (auto& e : engines_)
        if (e->language().compare(language, Qt::CaseInsensitive) == 0) return e.get();
    return nullptr;
}

QStringList ScriptHost::availableLanguages() const {
    QStringList r;
    for (auto& e : engines_)
        if (e->available()) r.append(e->language());
    return r;
}

bool ScriptHost::isLanguageAvailable(const QString& language) const {
    ScriptEngine* e = engineFor(language);
    return e && e->available();
}

ScriptResult ScriptHost::runCode(const QString& language, const QString& code) {
    ScriptEngine* e = engineFor(language);
    if (!e) {
        ScriptResult r;
        r.language = language;
        r.error = QStringLiteral("不支持的语言：%1").arg(language);
        emit scriptError(r.error);
        return r;
    }
    if (!e->available()) {
        ScriptResult r;
        r.language = language;
        r.error = QStringLiteral("%1 引擎不可用（运行时缺失或未启用）").arg(e->displayName());
        emit scriptError(r.error);
        return r;
    }
    ScriptResult res = e->execute(code, api_.get(), [this](const QString& s) {
        emit scriptOutput(s);
    });
    if (!res.ok) emit scriptError(res.error);
    return res;
}

ScriptResult ScriptHost::runFile(const QString& language, const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ScriptResult r;
        r.language = language;
        r.error = QStringLiteral("无法打开脚本文件：%1").arg(filePath);
        emit scriptError(r.error);
        return r;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    QString code = ts.readAll();
    f.close();
    return runCode(language, code);
}

void ScriptHost::setModel(SketchModel* model) {
    if (api_) api_->setModel(model);   // 引擎经 api() 反向访问模型，换绑 api 即全局生效
}

} // namespace SketchCAD
