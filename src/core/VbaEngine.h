#pragma once
#include "ScriptEngine.h"

// 注意：VbaEngine 仅在开启 SCRIPTHOST_ENABLE_VBA 且为 Windows 平台时编译（默认 OFF）。
// 依赖 Windows Script Host（VBScript 引擎，系统自带）与 COM（ole32/oleaut32）。
#if defined(SCRIPTHOST_ENABLE_VBA) && defined(Q_OS_WIN)
// 实现体需要的 Windows/COM 头文件在 .cpp 中引入，避免污染公共头。
#endif

namespace SketchCAD {

// VBA 引擎（策略）：基于 Windows Script Host 宿主 VBScript。
// 把 ScriptApi 作为命名对象 `App`（IDispatch）暴露给脚本；支持 App.AddLine(...) 等调用。
// 仅在开启 SCRIPTHOST_ENABLE_VBA 且 Q_OS_WIN 时参与构建（默认 OFF）。
class VbaEngine : public ScriptEngine {
public:
    QString language()    const override { return QStringLiteral("vba"); }
    QString displayName() const override { return QStringLiteral("VBA / VBScript"); }
    bool available() const override;       // 仅当 Windows Script Host 可用时为真
    ScriptResult execute(const QString& code, ScriptApi* api,
                         std::function<void(const QString&)> logSink) override;
};

} // namespace SketchCAD
