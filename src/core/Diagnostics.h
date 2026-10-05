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

    // ---- 会话标记（异常退出检测）----
    // 机制：启动时写标记文件 → 正常退出删除 → 残留 = 上次未正常退出
    //（崩溃 / 被任务管理器杀死 / 断电）。标记内容只是时间戳，无敏感信息。
    // 路径注入版本供无头自测在临时目录验证；begin/endSession 用真实路径。
    static QString sessionMarkerPath();              // %LOCALAPPDATA%/SketchCAD/session.marker
    static bool writeSessionMarker(const QString& path);
    static bool isAbnormalExit(const QString& path); // 标记残留 = 上次异常退出
    static bool clearSessionMarker(const QString& path);

    // 便捷包装（真实路径）：启动时调用 beginSession（返回是否检测到上次异常
    // 退出并缓存）；aboutToQuit 调 endSession。lastSessionAbnormal 供 UI 稍后
    // 查询（showAbnormalExitNotice），acknowledgeAbnormalSession 消费后不再提示。
    static bool beginSession();
    static void endSession();
    static bool lastSessionAbnormal();
    static void acknowledgeAbnormalSession();
};

} // namespace SketchCAD
