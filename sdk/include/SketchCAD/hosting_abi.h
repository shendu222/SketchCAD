#pragma once

// =============================================================================
// SketchCAD Hosting ABI —— sketchCAD 作为「被宿主加载的动态库」的 C 接口
// -----------------------------------------------------------------------------
// 与 sdk/include/SketchCAD/i_plugin.h 的区别（务必分清方向）：
//   * i_plugin.h / IPlugin  : 第三方扩展 → 被 SketchCAD.exe 加载（向内扩展）
//   * 本文件 hosting ABI    : SketchCAD 本体 → 被 openMBD Studio 加载（向外嵌入）
// 二者符号前缀不同（sketchcad_sdk_* vs ombd_sketchcad_*），可在同一进程共存。
//
// 设计铁律：
//   1) 纯 C ABI：无 Qt / C++ 类型跨界，全部为 POD + void* 句柄 + int 状态码；
//   2) 所有权归属明确：create 创建的句柄必须由 destroy 释放；export_json
//      返回的缓冲必须由 free_bytes 释放（同一 CRT 堆）；
//   3) 不创建 QApplication：宿主必须已有 QCoreApplication 实例，否则 create 失败；
//   4) 环境路径由宿主显式注入（resource/config/temp/tools），插件不依赖 CWD；
//   5) 版本协商：abi_version + qt_version 双重校验，不匹配即拒绝加载。
// =============================================================================

#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(SKETCHCAD_HOSTING_ABI_BUILD)
#    define SKETCHCAD_HOSTING_ABI_EXPORT __declspec(dllexport)
#  else
#    define SKETCHCAD_HOSTING_ABI_EXPORT __declspec(dllimport)
#  endif
#else
#  define SKETCHCAD_HOSTING_ABI_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ABI 修订号：仅在破坏性变更时递增；宿主必须要求精确相等。
// v2：新增无状态交换 ABI（三视图 / 反哺计划 / Sketch→原生 JSON / DXF），
//     宿主不再静态链接 sketchCAD core/gui/dwgbridge，资源只由插件注册一次。
#define OMBD_SKETCHCAD_HOSTING_ABI_VERSION 2

// 状态码（负值统一为错误，正值保留给异步/扩展）。
#define OMBD_SK_OK                 0
#define OMBD_SK_ERR_INVALID_ARG   -1
#define OMBD_SK_ERR_STATE         -2
#define OMBD_SK_ERR_NOT_FOUND     -3
#define OMBD_SK_ERR_VERSION       -4
#define OMBD_SK_ERR_INTERNAL      -5

typedef void (*OmbdSkLogCallback)(int level, const char* message, void* userData);
typedef void (*OmbdSkWritebackCallback)(const uint8_t* json, int32_t len, void* userData);

// 宿主环境：所有字符串为 UTF-8，允许为 NULL（表示未提供）。
// 宿主应以 structSize = sizeof(OmbdSkHostEnv) 调用；插件据此做向前兼容检查。
typedef struct OmbdSkHostEnv {
    uint32_t structSize;
    const char* resourceDir;   // 资源根目录（翻译 / 标准库等外部资源）
    const char* configDir;     // 配置写入目录（QSettings 文件）
    const char* tempDir;       // 临时文件目录（Autosave / DXF 中转）
    const char* toolsDir;      // 外部工具目录（DWG 转换器）
    OmbdSkLogCallback log;
    void* userData;
} OmbdSkHostEnv;

SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_abi_version(void);
// 返回插件编译时的 QT_VERSION（如 0x060A01）；宿主用自己的 QT_VERSION 精确比对。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_qt_version(void);
// 线程局部错误串；返回指针在本线程下一次调用任一 API 前有效。
SKETCHCAD_HOSTING_ABI_EXPORT const char* ombd_sketchcad_last_error(void);

SKETCHCAD_HOSTING_ABI_EXPORT void* ombd_sketchcad_create(const OmbdSkHostEnv* env);
SKETCHCAD_HOSTING_ABI_EXPORT void  ombd_sketchcad_destroy(void* session);

// editor 为 QWidget*（不跨界暴露类型）；宿主需在 QApplication 线程调用。
SKETCHCAD_HOSTING_ABI_EXPORT void* ombd_sketchcad_create_editor(void* session, void* parentWidget);
SKETCHCAD_HOSTING_ABI_EXPORT void  ombd_sketchcad_destroy_editor(void* session, void* editorWidget);

SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_load_json(void* session,
                                                              const uint8_t* bytes,
                                                              int32_t len);
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_export_json(void* session,
                                                                uint8_t** outBytes,
                                                                int32_t* outLen);
SKETCHCAD_HOSTING_ABI_EXPORT void ombd_sketchcad_free_bytes(uint8_t* bytes);

SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_set_writeback(
    void* session, OmbdSkWritebackCallback cb, void* userData);
// 宿主侧本地编辑后主动触发一次回写通知（载入 JSON 不触发，防回环）。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_notify_local_change(void* session);

// 宿主载入 JSON 后请求编辑器重绘并适应视图（等价 GraphicsView::onModelChanged + fitView）。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_refresh_editor(void* session);

// v2 追加（非破坏性）：嵌入式绘图命令入口。宿主侧「绘图」菜单与命令行把 UTF-8
// 命令整行（如 "line 0,0 10,0"）交给插件的 CommandInterpreter 执行；图元变更
// 经 modelChanged → writeback 自动回流宿主，与 sketchCAD 原生命令行同源。
// 返回 OMBD_SK_OK 表示命令执行成功；执行失败返回 OMBD_SK_ERR_INTERNAL 并写 last_error。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exec_command(void* session,
                                                                 const uint8_t* bytes,
                                                                 int32_t len);
// 激活交互绘图工具（与 sketchCAD 原生菜单同源：model->setActiveTool(id)，
// 例如 "line"/"circle"/"rect"/"arc"/"select"）；id 为 UTF-8 的 GTool id。
// 工具激活后，用户在画布点选由 GraphicsView 完成；命令行坐标由宿主经 exec_command 注入。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_set_tool(void* session,
                                                             const uint8_t* bytes,
                                                             int32_t len);
// 查询当前活动工具 id（UTF-8，输出缓冲由 free_bytes 释放）。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_active_tool(void* session,
                                                                uint8_t** outBytes,
                                                                int32_t* outLen);
// 返回内置命令名列表（UTF-8，'\n' 分隔），供宿主命令行自动补全；无会话也可调用。
// 输出缓冲必须由 ombd_sketchcad_free_bytes 释放（同一 CRT 堆）。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_command_names(uint8_t** outBytes,
                                                                  int32_t* outLen);

// 轻量自检：把可读报告写入 out（cap 字节），返回 OMBD_SK_OK 表示健康。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_self_check(char* out, int32_t cap);

// ---- v2 交换 ABI：无状态，可在 create() 之前调用 --------------------------
// 这些函数把 sketchCAD 专属类型完全留在 DLL 内；宿主只交换 UTF-8 JSON 字节。
// 输出缓冲必须由 ombd_sketchcad_free_bytes 释放（同一 CRT 堆）。
//
// 1) 宿主把 OMBD 三视图几何序列化为中性 JSON → 插件生成 sketchCAD 原生 JSON。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exchange_build_three_views(
    const uint8_t* neutralJson, int32_t len, uint8_t** outBytes, int32_t* outLen);
// 2) 原生图纸 JSON → 反哺计划 JSON（几何原语 + 溯源 + 非几何元数据）。
//    sourceDrawingId 可为 NULL；maxDeviationMm <=0 时插件按 0.01mm 处理。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exchange_drawing_plan(
    const uint8_t* nativeJson, int32_t len, const char* sourceDrawingId,
    double maxDeviationMm, uint8_t** outBytes, int32_t* outLen);
// 3) 宿主 Sketch 原语 JSON → sketchCAD 原生 JSON。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exchange_native_from_sketch(
    const uint8_t* sketchJson, int32_t len, uint8_t** outBytes, int32_t* outLen);
// 4) DXF 兼容通道（有损，明确不承诺 PMI/约束/布局无损）。
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exchange_to_dxf(
    const uint8_t* nativeJson, int32_t len, uint8_t** outBytes, int32_t* outLen);
SKETCHCAD_HOSTING_ABI_EXPORT int32_t ombd_sketchcad_exchange_from_dxf(
    const uint8_t* dxf, int32_t len, uint8_t** outBytes, int32_t* outLen);

#ifdef __cplusplus
} // extern "C"
#endif
