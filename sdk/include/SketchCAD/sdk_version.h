#pragma once

// ============================================================================
// SketchCAD 原生 SDK —— 版本与 ABI 定义
// ----------------------------------------------------------------------------
// 本文件是 SDK 的「契约根基」，被宿主程序与第三方插件共同包含。
// 设计为纯头文件、零外部依赖（不引入任何 Qt / STL 跨边界类型），
// 以保证同一份头文件在宿主与任意第三方编译器中可独立编译通过。
//
// 对标 AutoCAD ObjectARX 的版本策略：
//   - ABI 版本（SKETCHCAD_SDK_ABI_VERSION）：仅当「破坏二进制兼容」时递增。
//     触发条件：任何公共接口的虚函数签名 / 顺序改变、POD 布局改变、
//     导出符号语义改变。新增纯虚函数（末尾追加）也会破坏既有插件；
//     新增接口（新抽象类）不算 ABI 破坏。
//   - 语义版本（VERSION_MAJOR/MINOR/PATCH）：对外发布号，可独立递增，
//     反映功能新增 / 文档更新 / 行为改进等。
//
// 运行时协商（PluginManager 加载期执行）：
//   插件导出 sketchcad_sdk_abi_version() 声明「本插件编译所依据的 ABI」；
//   宿主按区间 [SKETCHCAD_SDK_ABI_MIN_SUPPORTED, SKETCHCAD_SDK_ABI_VERSION] 判定：
//     插件 ABI ∈ 区间 → 接受（低于宿主时按兼容模式加载，宿主日志记录协商结果）；
//     插件 ABI > 宿主 → 拒绝（插件需要更新的宿主，宿主无法凭空提供新槽位）；
//     插件 ABI < 区间下限 → 拒绝（宿主已不再保证该代际的接口仍存在）。
//   区间可行的前提是两条硬承诺（见 docs/sdk-plugin-framework-design-analysis.md §9）：
//     (a) 接口 vtable 只允许「末尾追加槽位」（且追加须升新接口类，禁止改既有类）；
//     (b) POD 结构体字段只增不改不删（新增字段追加到尾部并给默认值）。
// ============================================================================

// ---- ABI 版本（跨 DLL 二进制兼容的硬门槛）----
#define SKETCHCAD_SDK_ABI_VERSION 1
// 宿主承诺仍兼容的最低插件 ABI 代际（协商区间下限，含）。
// 升级宿主 ABI 到 2 时，若 v1 接口全部按承诺保留，则应把本值保持为 1，
// 使旧插件在「兼容模式」下继续加载；仅当决定废弃 v1 接口时才上调本值。
#define SKETCHCAD_SDK_ABI_MIN_SUPPORTED 1

// ---- 语义版本 ----
#define SKETCHCAD_SDK_VERSION_MAJOR 1
#define SKETCHCAD_SDK_VERSION_MINOR 0
#define SKETCHCAD_SDK_VERSION_PATCH 0

// 版本字符串（UTF-8），形如 "1.0.0"
#define SKETCHCAD_SDK_VERSION_STRING "1.0.0"

// 把 (major,minor) 压成一个整数，便于 IApplication::sdkVersion() 返回与比对。
// 例如 1.0 -> 100，1.2 -> 102。
#define SKETCHCAD_SDK_VERSION_INT \
    (SKETCHCAD_SDK_VERSION_MAJOR * 100 + SKETCHCAD_SDK_VERSION_MINOR)
