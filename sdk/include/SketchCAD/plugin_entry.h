#pragma once

#include "sketchcad_sdk.h"
#include "i_plugin.h"
#include "sdk_version.h"

// ============================================================================
// SketchCAD 原生 SDK —— 插件入口契约（第三方必须遵循）
// ----------------------------------------------------------------------------
// 一个可被宿主 PluginManager 加载的合法插件，必须向外界导出两个 C 链接符号：
//
//   1) int  sketchcad_sdk_abi_version(void)
//        返回编译该插件时所依据的 SKETCHCAD_SDK_ABI_VERSION。
//        宿主在加载时解析此符号并与自身 ABI 比对，不一致即拒绝加载。
//
//   2) IPlugin* sketchcad_create_plugin(void)
//        返回一个「新创建」的插件实例（堆分配，由宿主负责 delete）。
//        返回的实例必须能在后续被宿主调用 initialize() / shutdown()。
//
// 插件作者只需：
//   (a) 定义一个继承自 SketchCAD::sdk::IPlugin 的子类，实现
//       info() / initialize() / shutdown()；
//   (b) 在 .cpp 中放置一行 SKETCHCAD_PLUGIN_ENTRY(MyPluginClass)，
//       即可一键生成上述两个导出符号。
//
// 编译插件时须定义 SKETCHCAD_PLUGIN_BUILD（见下方导出宏链路），
// 并通过 -I 包含本 SDK 的 include 目录（sdk/include）。
// 插件无需链接宿主任何库 —— 仅依赖本头文件契约（纯接口 + POD），
// 因此 ABI 稳定，可独立于宿主二进制更新。
// ============================================================================

// 一键生成插件入口。用法（插件 .cpp 中）：
//   class MyPlugin : public SketchCAD::sdk::IPlugin { ... };
//   SKETCHCAD_PLUGIN_ENTRY(MyPlugin)
// 注意：MyPlugin 必须是可默认构造的（宿主以 new MyPlugin() 创建实例）。
#define SKETCHCAD_PLUGIN_ENTRY(Cls)                                               \
extern "C" {                                                                     \
SKETCHCAD_PLUGIN_EXPORT int sketchcad_sdk_abi_version() {                         \
    return SKETCHCAD_SDK_ABI_VERSION;                                            \
}                                                                                \
SKETCHCAD_PLUGIN_EXPORT ::SketchCAD::sdk::IPlugin* sketchcad_create_plugin() {   \
    return new Cls();                                                            \
}                                                                                \
} /* extern "C" */

// 便捷：在插件源文件顶部声明「本文件是插件实现」，启用导出宏链路。
// （等价于在编译命令行加 -DSKETCHCAD_PLUGIN_BUILD；此处提供源码级开关。）
#ifndef SKETCHCAD_PLUGIN_BUILD
#  define SKETCHCAD_PLUGIN_BUILD
#endif
