#pragma once
// ============================================================================
// COM 自动化接口 —— 错误码定义
// ----------------------------------------------------------------------------
// 采用 HRESULT 编码规范：Severity(31)=1 表示失败、Facility=ITF(4)，
// 自定义码从 0x0601 起线性分配，保证与系统错误码空间不冲突。
// 成功码仅两个：S_OK（全部成功）与 SCAD_S_PARTIAL（批量任务部分失败）。
//
// 所有 COM 方法失败时同时：
//   1) 返回失败 HRESULT；
//   2) 更新对象的 LastError 属性（「错误码:中文描述」格式）；
//   3) 引擎层写入日志文件（若调用方指定了 logPath）。
// ============================================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   // HRESULT / S_OK / E_FAIL / E_INVALIDARG
#include <QString>

namespace SketchCAD {
namespace comauto {

// ---- 成功码 ----
static constexpr HRESULT SCAD_S_OK          = S_OK;          // 0x00000000 全部成功
static constexpr HRESULT SCAD_S_PARTIAL     = 0x00040609;    // 批量执行：部分任务失败（结果 JSON 中有明细）

// ---- 失败码（FACILITY_ITF 自定义区）----
static constexpr HRESULT SCAD_E_FAIL            = E_FAIL;             // 0x80004005 未分类失败
static constexpr HRESULT SCAD_E_BAD_ARG         = E_INVALIDARG;       // 0x80070057 参数非法（空路径/类型不符）
static constexpr HRESULT SCAD_E_NOT_INITIALIZED = 0x80040601;         // 未初始化（未 Start 就调用业务方法）
static constexpr HRESULT SCAD_E_FILE_NOT_FOUND  = 0x80040602;         // 输入图纸文件不存在或不可读
static constexpr HRESULT SCAD_E_PARSE_FAILED    = 0x80040603;         // 图纸解析失败（损坏/格式不符）
static constexpr HRESULT SCAD_E_PLOT_FAILED     = 0x80040604;         // PDF 出图失败
static constexpr HRESULT SCAD_E_BAD_TASK        = 0x80040605;         // 批量任务 JSON 非法（缺 source/output 字段）
static constexpr HRESULT SCAD_E_WRITE_FAILED    = 0x80040606;         // 输出文件写入失败（目录不存在/被占用）
static constexpr HRESULT SCAD_E_BUSY            = 0x80040607;         // 引擎忙（另一批量任务正在执行）
static constexpr HRESULT SCAD_E_UNSUPPORTED_FMT = 0x80040608;         // 不支持的格式（如 STEP 读回）

// 错误码 → 短名（日志与错误报告用，如 "SCAD_E_FILE_NOT_FOUND"）
inline const char* comErrorName(HRESULT hr) {
    switch (hr) {
    case SCAD_S_OK:              return "SCAD_S_OK";
    case SCAD_S_PARTIAL:         return "SCAD_S_PARTIAL";
    case SCAD_E_FAIL:            return "SCAD_E_FAIL";
    case SCAD_E_BAD_ARG:         return "SCAD_E_BAD_ARG";
    case SCAD_E_NOT_INITIALIZED: return "SCAD_E_NOT_INITIALIZED";
    case SCAD_E_FILE_NOT_FOUND:  return "SCAD_E_FILE_NOT_FOUND";
    case SCAD_E_PARSE_FAILED:    return "SCAD_E_PARSE_FAILED";
    case SCAD_E_PLOT_FAILED:     return "SCAD_E_PLOT_FAILED";
    case SCAD_E_BAD_TASK:        return "SCAD_E_BAD_TASK";
    case SCAD_E_WRITE_FAILED:    return "SCAD_E_WRITE_FAILED";
    case SCAD_E_BUSY:            return "SCAD_E_BUSY";
    case SCAD_E_UNSUPPORTED_FMT: return "SCAD_E_UNSUPPORTED_FMT";
    default:                     return "SCAD_E_UNKNOWN";
    }
}

// 错误码 → 中文描述（LastError 属性 / 日志正文）
inline QString comErrorMessage(HRESULT hr) {
    switch (hr) {
    case SCAD_S_OK:              return QStringLiteral("成功");
    case SCAD_S_PARTIAL:         return QStringLiteral("批量任务部分失败（详见结果明细）");
    case SCAD_E_FAIL:            return QStringLiteral("未分类失败");
    case SCAD_E_BAD_ARG:         return QStringLiteral("参数非法");
    case SCAD_E_NOT_INITIALIZED: return QStringLiteral("自动化引擎未初始化，请先调用 Start()");
    case SCAD_E_FILE_NOT_FOUND:  return QStringLiteral("输入图纸文件不存在或不可读");
    case SCAD_E_PARSE_FAILED:    return QStringLiteral("图纸解析失败（文件损坏或格式不符）");
    case SCAD_E_PLOT_FAILED:     return QStringLiteral("PDF 出图失败");
    case SCAD_E_BAD_TASK:        return QStringLiteral("批量任务定义非法（缺少 source/output 字段）");
    case SCAD_E_WRITE_FAILED:    return QStringLiteral("输出文件写入失败（目录不存在或文件被占用）");
    case SCAD_E_BUSY:            return QStringLiteral("引擎忙：另一个批量任务正在执行");
    case SCAD_E_UNSUPPORTED_FMT: return QStringLiteral("不支持的文件格式（STEP 为只写格式，不支持读回）");
    default:                     return QStringLiteral("未知错误");
    }
}

// 组合「名称:描述」一行式（LastError 属性格式）
inline QString comErrorText(HRESULT hr) {
    return QStringLiteral("%1: %2 (0x%3)")
        .arg(QString::fromLatin1(comErrorName(hr)), comErrorMessage(hr))
        .arg(static_cast<uint>(hr), 8, 16, QLatin1Char('0'));
}

} // namespace comauto
} // namespace SketchCAD
