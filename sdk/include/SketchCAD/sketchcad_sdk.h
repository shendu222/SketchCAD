#pragma once

#include "sdk_version.h"

// ============================================================================
// SketchCAD 原生 SDK —— 导出宏
// ----------------------------------------------------------------------------
// 跨 DLL 的符号可见性宏。
//   - 宿主程序（主程序 / 自测）编译「SDK 桥接实现」(SdkContext) 时定义
//     SKETCHCAD_SDK_BUILD，使实现类的导出符号对插件可见（若插件需要回调查头）。
//   - 普通引用头文件（仅使用接口、不导出）时宏为空，避免误导出。
//   - 插件编译自身实现时定义 SKETCHCAD_PLUGIN_BUILD，使其
//     sketchcad_create_plugin / sketchcad_sdk_abi_version 两个 C 链接符号导出。
//
// 设计要点：SDK 公共接口本身是「纯 C++ 抽象类 + POD + const char*」，
// 不依赖任何导出宏即可被插件以头文件形式使用；导出宏只服务于
// 「宿主向插件暴露的 C 链接工厂 / 校验符号」与「插件向宿主暴露的入口符号」。
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#  if defined(SKETCHCAD_SDK_BUILD)
#    define SKETCHCAD_SDK_API __declspec(dllexport)
#  elif defined(SKETCHCAD_SDK_USE_DLL)
#    define SKETCHCAD_SDK_API __declspec(dllimport)
#  else
#    define SKETCHCAD_SDK_API
#  endif
// 插件导出：插件是符号提供方，始终 dllexport。
#  if defined(SKETCHCAD_PLUGIN_BUILD)
#    define SKETCHCAD_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define SKETCHCAD_PLUGIN_EXPORT
#  endif
#else
// Linux / macOS：用可见性属性（默认隐藏，仅标注符号导出）。
#  define SKETCHCAD_SDK_API __attribute__((visibility("default")))
#  define SKETCHCAD_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

// 字符串化辅助（供 SKETCHCAD_PLUGIN_ENTRY 宏把类名转字符串）。
#define SKETCHCAD_SDK_STRINGIFY_(x) #x
#define SKETCHCAD_SDK_STRINGIFY(x) SKETCHCAD_SDK_STRINGIFY_(x)
