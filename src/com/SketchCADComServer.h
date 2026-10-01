#pragma once
// ============================================================================
// COM 自动化服务器 —— 对外契约（接口文档见 docs/COM_API.md）
// ----------------------------------------------------------------------------
// 组件标识（regsvr32 注册后外部系统按 ProgID 后期绑定调用）：
//   ProgID : SketchCAD.Automation        （版本无关）
//   VerInd : SketchCAD.Automation.1      （版本 1）
//   CLSID  : {B7E4D3A2-5C1F-4E8B-9A6D-2F3C8E1A7B90}
//   线程模型：Apartment（STA；批量任务在引擎层再加进程级互斥串行化）
//
// 暴露方法（IDispatch 手写分派，dispid 见 cpp 内分派表）：
//   Start()                    初始化引擎（宿主无 Qt 时补建 QApplication），返回版本串
//   OpenDrawing(path)          加载图纸（.json/.dxf/.dwg）
//   PlotToPdf(path[,layout])   输出 PDF；layout 为 PrintSettings JSON 字符串（可省）
//   ExportAs(path)             按扩展名导出 dxf/dwg/step/json
//   BatchPlot(tasks[,log])     批量出图；tasks 为任务 JSON，log 为日志文件路径（可省）
//   GetTaskStatus()            返回最近一次批量结果 JSON
//   RegisterCallback(sink)     注册事件宿主（IDispatch*，VBScript Class 即满足）
//   Quit()                     释放模型与事件宿主
//   Version / LastError        属性（get）
//
// 事件（回调宿主按名解析，缺失即跳过；每个事件携带**单个** JSON 字符串参数）：
//   OnTaskStart(taskJson)      — {"index":0,"source":..,"output":..}
//   OnTaskDone(taskJson)       — {"index":..,"source":..,"output":..,"ok":..,"error":..,"bytes":..,"elapsedMs":..}
//   OnBatchFinished(batchJson) — {"allOk":..,"total":..,"succeeded":..,"failed":..}
//
// DLL 标准导出：DllGetClassObject / DllCanUnloadNow / DllRegisterServer /
//               DllUnregisterServer
// ============================================================================
#ifndef NOMINMAX
#define NOMINMAX   // 防 windows.h 的 min/max 宏污染 Qt/std
#endif
#include <windows.h>

namespace SketchCAD {
namespace comauto {

// 组件标识（固定值，写入注册表；版本升级只加新 ProgID，不改 CLSID 语义）
// {B7E4D3A2-5C1F-4E8B-9A6D-2F3C8E1A7B90}
static const GUID kCLSID_SketchCadAutomation =
{ 0xB7E4D3A2, 0x5C1F, 0x4E8B, { 0x9A, 0x6D, 0x2F, 0x3C, 0x8E, 0x1A, 0x7B, 0x90 } };

static const wchar_t kProgID[]     = L"SketchCAD.Automation";
static const wchar_t kProgIDVer[]  = L"SketchCAD.Automation.1";
static const wchar_t kDescription[] = L"SketchCAD Automation & Batch Plot Server";

} // namespace comauto
} // namespace SketchCAD

// ---- DLL 标准导出（C 链接，regsvr32 / COM 运行时入口）----
extern "C" {

// COM 激活入口：返回 IClassFactory（或直接返回类对象接口）
__declspec(dllexport) HRESULT __stdcall DllGetClassObject(
    REFCLSID rclsid, REFIID riid, void** ppv);

// 引擎空闲判定：无活动对象且无锁时允许卸载
__declspec(dllexport) HRESULT __stdcall DllCanUnloadNow(void);

// regsvr32 注册：写 ProgID / CLSID / InprocServer32(ThreadingModel=Apartment)
__declspec(dllexport) HRESULT __stdcall DllRegisterServer(void);

// regsvr32 /u 注销：删除注册表项
__declspec(dllexport) HRESULT __stdcall DllUnregisterServer(void);

} // extern "C"
