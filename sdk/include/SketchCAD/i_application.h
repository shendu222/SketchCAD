#pragma once

#include "types.h"

// ============================================================================
// SketchCAD 原生 SDK —— 应用（运行时）接口（对标 ObjectARX AcRx / AcEd）
// ----------------------------------------------------------------------------
// IApplication 是插件加载时拿到的「宿主能力总入口」：
//   - model()        取得 IModel，读写图纸图元
//   - log()          写日志
//   - registerCommand() 注册一条可在命令行调用的命令
//   - appVersion() / sdkVersion() / sdkAbiVersion()  版本协商
//
// 该指针在插件整个生命周期内稳定有效，插件可在 initialize() 中缓存它，
// 用于命令回调里反向访问宿主。纯虚接口 + POD，锁定跨 DLL ABI。
// ============================================================================

namespace SketchCAD {
namespace sdk {

class IModel;

// 插件命令处理函数指针类型。
//   args：命令行中命令名之后的剩余参数（UTF-8，不含首 token）。
//         例如用户输入 "drawGear 20 30"，则 args = "20 30"。
//   return：命令是否执行成功（false 会被宿主记入错误日志）。
// 插件以 SKETCHCAD_PLUGIN_EXPORT 函数或 lambda 形式提供，经 registerCommand 注册。
using SdkCommandFn = bool (*)(const char* args);

class IApplication {
public:
    virtual ~IApplication() = default;

    // 取得模型（数据库）访问入口。非空；生命周期等同宿主进程。
    virtual IModel* model() = 0;

    // 写一行日志到宿主日志面板（msg 为 UTF-8）。
    virtual void log(const char* msg) = 0;

    // 注册一条命令。name 为命令行名称（小写 ASCII，建议不含空格）；
    // 注册后用户在命令行输入该 name 即触发 fn。同名冲突时返回 false
    // （内置命令优先，已注册的外部命令次之，重名外部命令被拒绝）。
    virtual bool registerCommand(const char* name, SdkCommandFn fn) = 0;

    // 宿主程序版本字符串（UTF-8），如 "SketchCAD 1.0"。
    virtual const char* appVersion() const = 0;
    // SDK 语义版本号（整数，major*100+minor），如 1.0 -> 100。
    virtual int sdkVersion() const = 0;
    // SDK ABI 版本号（等于 SKETCHCAD_SDK_ABI_VERSION）。插件据此判断兼容性。
    virtual int sdkAbiVersion() const = 0;
};

} // namespace sdk
} // namespace SketchCAD
