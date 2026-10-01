// ============================================================================
// ABI 协商探针插件（仅用于宿主自测的负例，绝不投放给第三方）
// ----------------------------------------------------------------------------
// 故意导出 sketchcad_sdk_abi_version() = 99（「来自未来」的 ABI 代际），
// 验证宿主 PluginManager 的运行时协商策略：
//   插件 ABI > 宿主 ABI → 必须拒绝加载，且错误消息中报告双方版本。
// 本 DLL 故意不导出 sketchcad_create_plugin —— 即使将来宿主策略调整，
// 拒绝也必须发生在解析工厂符号之前。
// ============================================================================
#include "SketchCAD/sketchcad_sdk.h"

extern "C" {

SKETCHCAD_PLUGIN_EXPORT int sketchcad_sdk_abi_version() {
    return 99;   // 远超宿主 SKETCHCAD_SDK_ABI_VERSION（1），应触发「ABI 过新」拒绝
}

// 注意：刻意不提供 sketchcad_create_plugin。
// 协商策略要求版本检查先于工厂解析，本探针正是该顺序的回归验证。

} // extern "C"
