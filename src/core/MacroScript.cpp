#include "MacroScript.h"
#include "Commands.h"
#include "CommandInterpreter.h"
#include "SketchModel.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

namespace SketchCAD {

bool MacroRecorder::s_playbackActive = false;

MacroRecorder& MacroRecorder::instance() {
    static MacroRecorder inst;
    return inst;
}

bool MacroRecorder::isMacroControlCommand(const QString& cmdLower) {
    static const QStringList kMacroCommands = {
        "macrorec", "macrostop", "macrosave", "macroshow", "macroplay", "scr"
    };
    return kMacroCommands.contains(cmdLower);
}

void MacroRecorder::captureLine(const QString& rawLine) {
    MacroRecorder& r = instance();
    if (!r.recording_ || s_playbackActive) return;
    const QString s = rawLine.trimmed();
    if (s.isEmpty()) return;
    // 提取命令名（首个空白前缀，小写），宏控制命令不入缓冲
    static const QRegularExpression ws("\\s+");
    const QString cmd = s.split(ws, Qt::SkipEmptyParts).value(0).toLower();
    if (isMacroControlCommand(cmd)) return;
    r.lines_ << s;
}

bool MacroRecorder::start(const QString& autoSavePath) {
    if (recording_ || s_playbackActive) return false;
    recording_ = true;
    autoSavePath_ = autoSavePath;
    lines_.clear();
    return true;
}

bool MacroRecorder::stop(QString* err) {
    if (!recording_) {
        if (err) *err = QStringLiteral("当前没有正在进行的宏录制");
        return false;
    }
    recording_ = false;
    if (autoSavePath_.isEmpty()) return true;
    const QString path = autoSavePath_;
    autoSavePath_.clear();
    return save(path, err);
}

void MacroRecorder::clear() {
    lines_.clear();
    autoSavePath_.clear();
}

bool MacroRecorder::save(const QString& path, QString* err) const {
    if (path.isEmpty()) {
        if (err) *err = QStringLiteral("保存路径为空");
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法写入文件：%1").arg(path);
        return false;
    }
    QTextStream ts(&f);
    for (const QString& l : lines_) ts << l << '\n';
    return true;
}

bool MacroRecorder::load(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法读取文件：%1").arg(path);
        return false;
    }
    QStringList out;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QChar('#')) || line.startsWith(QChar(';'))
            || line.startsWith(QStringLiteral("//")))
            continue;
        out << line;
    }
    lines_ = out;
    return true;
}

// ============================================================
// ScriptRunner
// ============================================================
ScriptRunResult ScriptRunner::runText(SketchModel* m, CommandInterpreter* interp,
                                   const QString& text, StopMode mode,
                                   const QString& logTag) {
    ScriptRunResult res;
    if (!interp) {
        m->log(QStringLiteral("[%1] 内部错误：命令解释器为空").arg(logTag), "error");
        res.stoppedEarly = true;
        return res;
    }
    if (MacroRecorder::playbackActive()) {
        m->log(QStringLiteral("[%1] 拒绝嵌套执行：已有脚本正在运行").arg(logTag), "error");
        res.stoppedEarly = true;
        return res;
    }
    if (MacroRecorder::instance().isRecording()) {
        m->log(QStringLiteral("[%1] 拒绝执行：宏录制进行中（先 macrostop）").arg(logTag), "error");
        res.stoppedEarly = true;
        return res;
    }

    MacroRecorder::setPlaybackActive(true);
    struct PlaybackGuard { ~PlaybackGuard() { MacroRecorder::setPlaybackActive(false); } } guard;

    const QStringList lines = text.split(QChar('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();   // trimmed 同时去除 CRLF 的 \r
        if (line.isEmpty()) continue;
        if (line.startsWith(QChar('#')) || line.startsWith(QChar(';'))
            || line.startsWith(QStringLiteral("//")))
            continue;

        ++res.totalLines;
        m->log(QString("[%1:%2] >> %3").arg(logTag).arg(i + 1).arg(line), "cmd");
        const bool ok = interp->execute(line);
        if (ok) {
            ++res.executed;
            continue;
        }
        ++res.failed;
        res.failedLineNo << (i + 1);
        m->log(QString("[%1:%2] 执行失败：%3").arg(logTag).arg(i + 1).arg(line), "error");
        if (mode == StopMode::StopOnError) {
            res.stoppedEarly = true;
            m->log(QString("[%1] 中断于第 %2 行（STOP 模式）：已执行 %3，失败 %4")
                       .arg(logTag).arg(i + 1).arg(res.executed).arg(res.failed), "warn");
            return res;
        }
    }

    m->log(QString("[%1] 脚本执行完成：共 %2 条，成功 %3，失败 %4%5")
               .arg(logTag).arg(res.totalLines).arg(res.executed).arg(res.failed)
               .arg(res.failedLineNo.isEmpty()
                        ? QString()
                        : QStringLiteral("（失败行：%1）").arg(
                              [&] {
                                  QStringList ns;
                                  for (int n : res.failedLineNo) ns << QString::number(n);
                                  return ns.join(QChar(','));
                              }())),
           res.failed > 0 ? "warn" : "info");
    return res;
}

ScriptRunResult ScriptRunner::runFile(SketchModel* m, CommandInterpreter* interp,
                                   const QString& path, StopMode mode,
                                   QString* err, const QString& logTag) {
    ScriptRunResult res;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法读取脚本文件：%1").arg(path);
        m->log(QStringLiteral("[%1] %2").arg(logTag).arg(err ? *err : QString()), "error");
        return res;
    }
    QTextStream ts(&f);
    const QString text = ts.readAll();
    return runText(m, interp, text, mode, logTag);
}

// ============================================================
// 宏/脚本命令（CommandRegistry 注册）
// ============================================================
namespace {

/// 参数解析：把 toks[1..] 重新拼成参数串。
/// 支持带空格路径（双引号包裹）；"continue" 关键字置于尾部 → CONTINUE 模式。
struct PathArgs {
    QString path;
    bool cont = false;
};

PathArgs parsePathArgs(const QStringList& toks) {
    PathArgs pa;
    if (toks.size() < 2) return pa;
    const QString rest = toks.mid(1).join(QChar(' ')).trimmed();
    if (rest.startsWith(QChar('"'))) {
        const int end = rest.indexOf(QChar('"'), 1);
        if (end > 0) {
            pa.path = rest.mid(1, end - 1);
            pa.cont = rest.mid(end + 1).trimmed()
                          .startsWith(QStringLiteral("continue"),
                                      Qt::CaseInsensitive);
            return pa;
        }
    }
    if (toks.size() > 2
        && toks.last().compare(QStringLiteral("continue"), Qt::CaseInsensitive) == 0) {
        pa.cont = true;
        pa.path = toks.mid(1, toks.size() - 2).join(QChar(' '));
        return pa;
    }
    pa.path = rest;
    return pa;
}

/// macroplay / scr 共用回放入口。fixedContinue：<0 跟随参数，>=0 固定模式。
bool runScriptCommand(const CommandContext& ctx, int fixedContinue, const QString& usage) {
    SketchModel* m = ctx.model;
    const PathArgs pa = parsePathArgs(ctx.toks);
    if (pa.path.isEmpty()) {
        m->log(usage, "error");
        return false;
    }
    CommandInterpreter* interp = ctx.interp;
    if (!interp) {
        m->log(QStringLiteral("内部错误：命令解释器不可用"), "error");
        return false;
    }
    const ScriptRunner::StopMode mode =
        (fixedContinue >= 0) ? static_cast<ScriptRunner::StopMode>(fixedContinue)
                             : (pa.cont ? ScriptRunner::StopMode::ContinueOnError
                                        : ScriptRunner::StopMode::StopOnError);
    QString err;
    const ScriptRunResult res = ScriptRunner::runFile(m, interp, pa.path, mode, &err,
                                                   QFileInfo(pa.path).fileName());
    return !err.isEmpty() ? false : res.ok();
}

} // namespace

// ---- 各命令实现 ----
namespace macrocmd {

bool cmdMacroRec(const CommandContext& ctx) {
    const PathArgs pa = parsePathArgs(ctx.toks);
    if (!MacroRecorder::instance().start(pa.path)) {
        ctx.model->log(QStringLiteral("macrorec：无法开始录制（已在录制中或脚本回放进行中）"), "error");
        return false;
    }
    ctx.model->log(
        pa.path.isEmpty()
            ? QStringLiteral("宏录制开始：此后执行的命令将逐条记录（macrostop 结束，macrosave 保存，macroshow 查看）")
            : QStringLiteral("宏录制开始（自动保存到 %1）：macrostop 结束并落盘").arg(pa.path),
        "info");
    return true;
}

bool cmdMacroStop(const CommandContext& ctx) {
    QString err;
    if (!MacroRecorder::instance().stop(&err)) {
        ctx.model->log(QStringLiteral("macrostop：%1").arg(err), "error");
        return false;
    }
    ctx.model->log(QStringLiteral("宏录制已停止：共 %1 条命令（macrosave 保存 / macroshow 查看）")
                       .arg(MacroRecorder::instance().lines().size()), "info");
    return true;
}

bool cmdMacroSave(const CommandContext& ctx) {
    const PathArgs pa = parsePathArgs(ctx.toks);
    if (pa.path.isEmpty()) {
        ctx.model->log(QStringLiteral("用法：macrosave <文件>（路径含空格用双引号包裹）"), "error");
        return false;
    }
    if (MacroRecorder::instance().lines().isEmpty()) {
        ctx.model->log(QStringLiteral("macrosave：没有可保存的录制内容"), "error");
        return false;
    }
    QString err;
    if (!MacroRecorder::instance().save(pa.path, &err)) {
        ctx.model->log(QStringLiteral("macrosave：%1").arg(err), "error");
        return false;
    }
    ctx.model->log(QStringLiteral("宏已保存：%1（共 %2 条命令）")
                       .arg(pa.path).arg(MacroRecorder::instance().lines().size()), "info");
    return true;
}

bool cmdMacroShow(const CommandContext& ctx) {
    const QStringList& ls = MacroRecorder::instance().lines();
    if (ls.isEmpty()) {
        ctx.model->log(QStringLiteral("macroshow：录制缓冲为空"), "warn");
        return true;
    }
    ctx.model->log(QStringLiteral("已录制 %1 条命令：").arg(ls.size()), "info");
    for (int i = 0; i < ls.size(); ++i)
        ctx.model->log(QString("  %1: %2").arg(i + 1).arg(ls[i]), "info");
    return true;
}

bool cmdMacroPlay(const CommandContext& ctx) {
    return runScriptCommand(ctx, static_cast<int>(ScriptRunner::StopMode::StopOnError),
                            QStringLiteral("用法：macroplay <文件>（回放宏，遇错即停）"));
}

bool cmdScr(const CommandContext& ctx) {
    return runScriptCommand(ctx, -1,
                            QStringLiteral("用法：scr <文件> [continue]（continue=遇错继续执行）"));
}

} // namespace macrocmd

void registerMacroCommands(CommandRegistry& reg) {
    reg.registerExact("macrorec", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdMacroRec(ctx); }));
    reg.registerExact("macrostop", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdMacroStop(ctx); }));
    reg.registerExact("macrosave", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdMacroSave(ctx); }));
    reg.registerExact("macroshow", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdMacroShow(ctx); }));
    reg.registerExact("macroplay", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdMacroPlay(ctx); }));
    reg.registerExact("scr", std::make_shared<LambdaCommand>(
        [](const CommandContext& ctx) { return macrocmd::cmdScr(ctx); }));
}

} // namespace SketchCAD
