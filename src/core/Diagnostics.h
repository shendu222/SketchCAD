#pragma once
#include <QString>

// 诊断子系统：Qt 日志落盘（滚动）+ Windows 崩溃转储（mini-dump）。
//
// 设计要点：
// 1. 全部落盘位置在用户目录 %LOCALAPPDATA%/SketchCAD/{logs,crashes}，
//    绝不写入程序安装目录（满足「用户数据只写用户目录」的改动边界）。
// 2. Qt 侧只把 warning / critical / fatal 落盘，滚动保留 3 个文件，单个 ≤ 2MB，
//    每次写后 flush，保证后续崩溃不丢诊断信息。
// 3. 崩溃转储（mini-dump + 摘要）仅 Windows 启用，且用 SKETCHCAD_ENABLE_CRASHHANDLER
//    宏隔离：SketchSelfTest 不定义该宏、不链接 dbghelp，从而保持无头自测目标纯净。
namespace SketchCAD {

class Diagnostics {
public:
    // 初始化诊断子系统：确定日志/崩溃目录并安装 Qt 消息处理器。
    // 必须在 QApplication 构造之后、主窗口构造之前调用一次。
    static void init(const QString& appName = QStringLiteral("SketchCAD"));

    // 卸载消息处理器（调试用）。
    static void shutdown();

    // 日志目录：%LOCALAPPDATA%/SketchCAD/logs
    static QString logDir();
    // 崩溃转储目录：%LOCALAPPDATA%/SketchCAD/crashes
    static QString crashDir();

    // 主动写一条诊断日志（落盘到滚动日志集）。level 通常 info/warning/error 等。
    static void writeLog(const QString& msg, const QString& level = "info");

    // 安装 Windows 未处理异常过滤器（写 mini-dump + 摘要）。非 Windows 为空操作。
    static void installCrashHandler();

    // 最近一次崩溃摘要（崩溃恢复对话框可参考展示），无则空串。
    static QString lastCrashSummary();

    // 确保日志/崩溃目录已创建（幂等）。公开以便崩溃过滤器在极早期调用。
    static void ensureDirs();
};

} // namespace SketchCAD
