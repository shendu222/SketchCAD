#pragma once

#include "sdk_version.h"

// ============================================================================
// SketchCAD 原生 SDK —— 插件接口契约（对标 ObjectARX AcRx 运行时 / AcEd 命令）
// ----------------------------------------------------------------------------
// IPlugin 是第三方插件必须实现的唯一抽象基类。宿主通过 PluginManager
// 经 C 链接工厂 sketchcad_create_plugin() 取得 IPlugin*，并管理其生命周期：
//
//   加载  → 调 initialize(IApplication*)      （插件在此注册命令、建菜单钩子等）
//   运行  → 插件经 IApplication 反向访问宿主能力
//   卸载  → 调 shutdown() 后 delete 实例、卸载 DLL
//
// 接口全部为纯虚函数，且仅使用 POD / const char* / 抽象指针，保证跨
// DLL 的 vtable ABI 稳定（同一编译器工具链下二进制兼容）。
// ============================================================================

namespace SketchCAD {
namespace sdk {

class IApplication;   // 前向声明：initialize() 入参，完整定义见 i_application.h

// 插件元信息。由插件以「静态字符串 / 静态结构体」提供，
// 生命周期等同于插件 DLL；宿主仅读取、不释放、不修改。
struct PluginInfo {
    const char* name        = "";   // 插件显示名（UTF-8）
    const char* version     = "";   // 插件版本（UTF-8），如 "1.0.0"
    const char* description  = "";   // 一句话说明（UTF-8）
    const char* category     = "";   // 行业分类（UTF-8）："机械" / "建筑" / "电气" / "通用" ...
    int         sdkAbiVersion = 0;  // 该插件编译时依据的 SKETCHCAD_SDK_ABI_VERSION
};

// 插件抽象接口。第三方插件继承并实现以下三个方法。
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // 加载时由宿主调用一次。返回 true 表示初始化成功（插件进入可用态）；
    // 返回 false 视为加载失败，宿主将调用 shutdown() 并卸载该 DLL。
    // 在此方法内插件应：
    //   - 经 app->registerCommand(...) 注册自身命令
    //   - 经 app->model() 预创建所需图元 / 读初始状态
    //   - 缓存 app 指针以备后续回调（注意：app 指针在整个插件生命周期内稳定有效）
    virtual bool initialize(IApplication* app) = 0;

    // 卸载时由宿主调用一次。插件应在此释放自身资源、反注册（命令由宿主统一回收）。
    virtual void shutdown() = 0;

    // 返回插件元信息（指向插件自身持有的静态 PluginInfo）。
    virtual const PluginInfo* info() const = 0;

    // 释放插件实例自身。宿主在 shutdown() 之后调用，代替直接 delete，
    // 使内存释放回落到插件自身模块（与 create 同堆），规避跨 DLL 的
    // delete 不兼容问题。默认实现即为 delete this，插件通常无需重写。
    virtual void dispose() { delete this; }
};

} // namespace sdk
} // namespace SketchCAD
