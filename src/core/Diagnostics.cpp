#include "Diagnostics.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QMutex>
#include <QSaveFile>
#include <QtGlobal>

#ifdef SKETCHCAD_ENABLE_CRASHHANDLER
#  if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <dbghelp.h>
#    // 注：dbghelp 由 CMake 的 target_link_libraries(... dbghelp) 链接，
#    // 不使用 MSVC 专有的 #pragma comment(lib, ...)（MinGW/GCC 会报 -Wunknown-pragmas 警告）。
#  endif
#endif

namespace SketchCAD {

namespace {
    QString g_logDir;
    QString g_crashDir;
    QMutex g_logMutex;          // 日志写串行，避免多线程交错
    QtMessageHandler g_oldHandler = nullptr;
    QString g_lastCrash;        // 最近一次崩溃摘要

    const qint64 kMaxLogBytes = 2 * 1024 * 1024;   // 单个日志文件 ≤ 2MB

    // 取 %LOCALAPPDATA%/SketchCAD 根目录（无则回落到用户主目录）
    QString localAppRoot() {
        QString local = QDir::fromNativeSeparators(
            QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")));
        if (local.isEmpty()) local = QDir::homePath() + "/AppData/Local";
        if (local.isEmpty()) local = QDir::homePath();
        return local + "/SketchCAD";
    }

    void rotateIfNeeded(const QString& base) {
        QFile f(base);
        if (!f.exists()) return;
        if (f.size() <= kMaxLogBytes) return;
        // 滚动：.2 丢弃，.1→.2，当前→.1
        QFile::remove(base + ".2");
        if (QFile::exists(base + ".1")) QFile::rename(base + ".1", base + ".2");
        QFile::rename(base, base + ".1");
    }
}

void Diagnostics::ensureDirs() {
    if (!g_logDir.isEmpty()) return;
    const QString root = localAppRoot();
    QDir d(root);
    d.mkpath("logs");
    d.mkpath("crashes");
    g_logDir = root + "/logs";
    g_crashDir = root + "/crashes";
}

QString Diagnostics::logDir() {
    ensureDirs();
    return g_logDir;
}

QString Diagnostics::crashDir() {
    ensureDirs();
    return g_crashDir;
}

void Diagnostics::writeLog(const QString& msg, const QString& level) {
    QMutexLocker lock(&g_logMutex);
    ensureDirs();
    const QString base = g_logDir + "/SketchCAD.log";
    rotateIfNeeded(base);
    QFile out(base);
    if (!out.open(QIODevice::Append | QIODevice::Text)) return;
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ")
                        + level + ": " + msg + "\n";
    out.write(line.toUtf8());
    out.flush();   // 关键：每次写后 flush，后续崩溃不丢诊断
}

#ifdef SKETCHCAD_ENABLE_CRASHHANDLER
#  if defined(_WIN32)
static QString crashStamp() {
    return QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
}

static void writeCrashSummary(const QString& stamp, EXCEPTION_POINTERS* ep) {
    QString dir = Diagnostics::crashDir();
    const QString txt = dir + "/crash-" + stamp + ".txt";
    QFile f(txt);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QString body = "SketchCAD 未处理异常崩溃\n";
    body += "时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "\n";
    if (ep && ep->ExceptionRecord) {
        body += QString("异常码: 0x%1\n").arg(
            (quint64)ep->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'));
    }
    body += "系统: " + QString::fromLocal8Bit(qgetenv("OS")) + "\n";
    f.write(body.toUtf8());
    f.close();

    // 记录「最近一次崩溃」供恢复对话框展示
    g_lastCrash = body;

    // 额外写一份固定名 last.txt，便于快速定位
    QFile last(dir + "/last.txt");
    if (last.open(QIODevice::WriteOnly | QIODevice::Text)) {
        last.write(body.toUtf8());
        last.close();
    }
}

static LONG WINAPI sketchcadUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    // 目录可能尚未初始化（极早期崩溃），先确保存在
    Diagnostics::ensureDirs();
    const QString stamp = crashStamp();
    const QString dir = Diagnostics::crashDir();
    const QString dmp = dir + "/crash-" + stamp + ".dmp";

    HANDLE hFile = CreateFileW((LPCWSTR)dmp.utf16(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }
    writeCrashSummary(stamp, ep);
    Diagnostics::writeLog(QString("未处理异常崩溃，已生成转储: %1.dmp").arg(stamp), "fatal");
    // 交给系统默认处理（弹出 WER），返回 EXCEPTION_EXECUTE_HANDLER 终止进程
    return EXCEPTION_EXECUTE_HANDLER;
}
#  endif
#endif

void Diagnostics::installCrashHandler() {
#ifdef SKETCHCAD_ENABLE_CRASHHANDLER
#  if defined(_WIN32)
    SetUnhandledExceptionFilter(sketchcadUnhandledExceptionFilter);
#  endif
#endif
}

QString Diagnostics::lastCrashSummary() {
    return g_lastCrash;
}

static void sketchcadMessageHandler(QtMsgType type, const QMessageLogContext& ctx,
                                    const QString& msg) {
    bool toDisk = false;
    QString level;
    switch (type) {
    case QtDebugMsg:  level = "debug";   break;
    case QtInfoMsg:   level = "info";    break;
    case QtWarningMsg:level = "warning"; toDisk = true; break;
    case QtCriticalMsg:level = "critical";toDisk = true; break;
    case QtFatalMsg:  level = "fatal";   toDisk = true; break;
    default:          level = "?";       break;
    }
    if (toDisk) Diagnostics::writeLog(msg, level);
    if (g_oldHandler) g_oldHandler(type, ctx, msg);
}

void Diagnostics::init(const QString& /*appName*/) {
    ensureDirs();
    if (!g_oldHandler) g_oldHandler = qInstallMessageHandler(sketchcadMessageHandler);
    installCrashHandler();
}

void Diagnostics::shutdown() {
    if (g_oldHandler) {
        qInstallMessageHandler(g_oldHandler);
        g_oldHandler = nullptr;
    }
}

} // namespace SketchCAD
