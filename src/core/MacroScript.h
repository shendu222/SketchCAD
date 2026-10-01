#pragma once
// ============================================================
// MacroScript —— 动作宏录制 / SCR 脚本批处理（第四阶段二次开发生态）
//
// 设计要点（详见 docs/macro-script-design-analysis.md）：
//   1. 捕获点唯一：CommandInterpreter::execute() 成功路径统一调用
//      MacroRecorder::captureLine()——命令行输入、脚本回放之外的
//      一切命令通路（含插件注册的外部命令）都会被自动捕获。
//   2. 录制/回放互斥：回放激活期间禁止录制捕获与嵌套回放，
//      录制中禁止回放，避免递归套娃与重复记录。
//   3. SCR 格式 = 每行一条与命令行完全一致的输入：
//      '#' / ';' / '//' 开头为注释，空行跳过；
//      因此 .scr 文件既是可重放脚本，也是可编辑的命令日志。
//   4. 中断策略：STOP（默认，遇错即停）与 CONTINUE（遇错继续，
//      最后汇总失败行号）；逐行日志 + 结束汇总双反馈。
// ============================================================
#include <QString>
#include <QStringList>
#include <QList>

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

// ------------------------------------------------------------
// 宏录制器（进程级单例）
// ------------------------------------------------------------
class MacroRecorder {
public:
    static MacroRecorder& instance();

    /// 开始录制。autoSavePath 非空时 stop() 会自动保存到该路径。
    /// 返回 false = 已在录制中或脚本回放进行中。
    bool start(const QString& autoSavePath = QString());

    /// 停止录制。有 autoSavePath 则自动保存；返回保存是否成功
    /// （无自动保存路径时返回 true）。录制缓冲保留，可 macroshow/再次保存。
    bool stop(QString* err = nullptr);

    bool isRecording() const { return recording_; }
    const QString& autoSavePath() const { return autoSavePath_; }
    const QStringList& lines() const { return lines_; }
    void clear();

    /// 保存录制内容为 .scr 纯文本（UTF-8，每行一条命令）。
    bool save(const QString& path, QString* err = nullptr) const;
    /// 从 .scr 载入命令序列到录制缓冲（供编辑后续录/回放）。
    bool load(const QString& path, QString* err = nullptr);

    // ---- 回放互斥标志（ScriptRunner 维护） ----
    static bool playbackActive() { return s_playbackActive; }
    static void setPlaybackActive(bool on) { s_playbackActive = on; }

    /// 宏控制命令名判断：这些命令永不进入录制缓冲。
    static bool isMacroControlCommand(const QString& cmdLower);

    /// 宏捕获点：由 CommandInterpreter::execute() 在命令成功后调用。
    /// 内部自行判断（录制中 && 非回放 && 非宏控制命令），可无条件调用。
    static void captureLine(const QString& rawLine);

private:
    MacroRecorder() = default;
    bool recording_ = false;
    QString autoSavePath_;
    QStringList lines_;
    static bool s_playbackActive;
};

// ------------------------------------------------------------
// SCR 脚本执行器
// ------------------------------------------------------------
struct ScriptRunResult {
    int totalLines = 0;        ///< 已处理的有效命令行数（STOP 中断后不再计入后续行）
    int executed = 0;          ///< 成功执行数
    int failed = 0;            ///< 失败数
    QList<int> failedLineNo;   ///< 失败行在文件中的行号（1 起）
    bool stoppedEarly = false; ///< STOP 模式提前中断

    bool ok() const { return failed == 0; }
};

class ScriptRunner {
public:
    /// 出错中断策略
    enum class StopMode { StopOnError, ContinueOnError };

    /// 执行脚本文本：逐行 trim 后交给 interp->execute()，
    /// '#'、';'、'//' 开头为注释，空行跳过。回放期间置全局互斥标志。
    static ScriptRunResult runText(SketchModel* m, CommandInterpreter* interp,
                                const QString& text, StopMode mode,
                                const QString& logTag = QStringLiteral("scr"));

    /// 读取脚本文件后执行；读失败时 err 非空并返回空结果。
    static ScriptRunResult runFile(SketchModel* m, CommandInterpreter* interp,
                                const QString& path, StopMode mode,
                                QString* err = nullptr,
                                const QString& logTag = QStringLiteral("scr"));
};

// ------------------------------------------------------------
// 命令注册（CommandRegistry::registerBuiltins() 末尾调用）：
//   macrorec  [文件]            开始录制（可选自动保存路径）
//   macrostop                   停止录制（有自动保存路径则落盘）
//   macrosave <文件>            保存当前录制内容为 .scr
//   macroshow                   列出已录制的命令序列
//   macroplay <文件>            回放宏文件（STOP 模式）
//   scr <文件> [continue]       运行 SCR 脚本（continue=遇错继续）
// 文件路径含空格时用双引号包裹。
// 注意：传入注册表引用而非内部调 instance()——registerBuiltins()
// 本身运行在 instance() 的静态局部初始化器内，再入会死锁（magic statics）。
// ------------------------------------------------------------
class CommandRegistry;
void registerMacroCommands(CommandRegistry& reg);

} // namespace SketchCAD
