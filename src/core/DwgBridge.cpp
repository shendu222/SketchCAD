#include "DwgBridge.h"
#include "CadFormats.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

namespace SketchCAD {
namespace dwgbridge {

// ---------------------------------------------------------------------------
// 转换器目录解析（查找顺序见头注释；无单例——魔静态只放 POD 串，无再入风险）
// ---------------------------------------------------------------------------
static QString g_testToolsDir;   // 自测注入（setToolsDirForTest）

static QString toolsDir() {
    if (!g_testToolsDir.isEmpty()) return g_testToolsDir;
    const QString envDir = qEnvironmentVariable("SKETCHCAD_DWGTOOLS");
    if (!envDir.isEmpty()) return envDir;
    // 安装包布局：<exe目录>/tools/dwg/dwg2dxf.exe …
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) return appDir + QStringLiteral("/tools/dwg");
    return QString();
}

static QString converterPath(const char* exeName) {
    const QString dir = toolsDir();
    if (dir.isEmpty()) return QString();
    QString p = dir + QLatin1Char('/') + QLatin1String(exeName);
#ifdef Q_OS_WIN
    p += QStringLiteral(".exe");
#else
    Q_UNUSED(exeName)
#endif
    return QFile::exists(p) ? p : QString();
}

bool available() {
    return !converterPath("dwg2dxf").isEmpty() && !converterPath("dwgwrite").isEmpty();
}

// ---------------------------------------------------------------------------
// 版本探测（唯一入口；CadFormats 路由与错误报文共用）
// ---------------------------------------------------------------------------
QString versionFromBytes(const QByteArray& data) {
    if (data.size() < 6) return QString();
    const QByteArray head = data.left(6);
    // DWG 头 6 字节必为 "AC" + 数字版本串（AC1xNN 形态）；粗校验防误读
    if (!head.startsWith("AC1")) return QString();
    return QString::fromLatin1(head);
}

QString versionHuman(const QString& acadVer) {
    return cadfmt::cadVerToHuman(acadVer);
}

bool canReadVersion(const QString& acadVer) {
    // AC1009(R12) 走自研 R12 解析，不经本桥；AC1012 及以后经 LibreDWG。
    // LibreDWG 读支持 R13~R2018；AC1032 及以后的未知新串也不在此放行——
    // 放行与否以"转换器能否吃下"为准，这里只做路由白名单。
    static const char* kReadSet[] = {
        "AC1012", "AC1014", "AC1015", "AC1018",
        "AC1021", "AC1024", "AC1027", "AC1032",
    };
    for (const char* v : kReadSet)
        if (acadVer == QLatin1String(v)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// 进程外转换核心：write(in) → run(exe,args) → read(out)
// 所有失败路径都收敛到 fail()，报文统一带版本上下文。
// ---------------------------------------------------------------------------
namespace {

struct RunResult {
    bool ok = false;
    QString err;
};

// 同步执行转换器；捕获 stderr 供诊断；超时/崩溃/非零退出统一归一。
RunResult runConverter(const QString& exe, const QStringList& args,
                       const QString& workDir, int timeoutMs) {
    RunResult r;
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(exe, args);
    if (!proc.waitForStarted(5000)) {
        // QProcess::error 枚举：0=FailedToStart 1=Crashed 2=Timedout 4=WriteError 5=Unknown
        r.err = QStringLiteral("DWG 转换器无法启动: %1（QProcess::error=%2）")
                    .arg(exe).arg(int(proc.error()));
        return r;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        if (proc.state() == QProcess::Running) proc.kill();
        proc.waitForFinished(2000);
        r.err = QStringLiteral("DWG 转换器超时（>%1s）或无响应").arg(timeoutMs / 1000);
        return r;
    }
    if (proc.exitStatus() == QProcess::CrashExit) {
        r.err = QStringLiteral("DWG 转换器异常退出（文件可能损坏或含不支持结构）");
        return r;
    }
    if (proc.exitCode() != 0) {
        const QByteArray msg = proc.readAllStandardError();
        r.err = QStringLiteral("DWG 转换器返回错误（exit=%1）%2")
                    .arg(proc.exitCode())
                    .arg(msg.isEmpty() ? QString()
                                       : QStringLiteral(": %1").arg(
                                             QString::fromLocal8Bit(msg).left(200).trimmed()));
        return r;
    }
    r.ok = true;
    return r;
}

// 通用三段式：落盘输入 → 转换 → 读回输出。临时目录 RAII 自动清理。
bool convert(const QString& exe, const QStringList& args,
             const QByteArray& inBytes, const QString& inName,
             const QString& outName, QByteArray& outBytes, QString* err,
             const QString& verCtx, int timeoutMs) {
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (err) *err = QStringLiteral("无法创建 DWG 转换临时目录");
        return false;
    }
    const QString inPath = tmp.filePath(inName);
    {   QFile f(inPath);
        if (!f.open(QIODevice::WriteOnly) || f.write(inBytes) != inBytes.size()) {
            if (err) *err = QStringLiteral("DWG 转换输入落盘失败");
            return false;
        } }
    const QString outPath = tmp.filePath(outName);

    QStringList realArgs = args;
    realArgs.replaceInStrings(QStringLiteral("@IN@"), QDir::toNativeSeparators(inPath));
    realArgs.replaceInStrings(QStringLiteral("@OUT@"), QDir::toNativeSeparators(outPath));

    const RunResult r = runConverter(exe, realArgs, tmp.path(), timeoutMs);
    if (!r.ok) {
        if (err) *err = r.err + (verCtx.isEmpty() ? QString()
                                                  : QStringLiteral("（%1）").arg(verCtx));
        return false;
    }
    QFile f(outPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("DWG 转换完成但输出不可读") +
                        (verCtx.isEmpty() ? QString()
                                          : QStringLiteral("（%1）").arg(verCtx));
        return false;
    }
    outBytes = f.readAll();
    if (outBytes.isEmpty()) {
        if (err) *err = QStringLiteral("DWG 转换输出为空") +
                        (verCtx.isEmpty() ? QString()
                                          : QStringLiteral("（%1）").arg(verCtx));
        return false;
    }
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 读桥：DWG → DXF（dwg2dxf，输出 R2000 DXF 供 cadfmt::fromDxf 回读）
// ---------------------------------------------------------------------------
bool dwgToDxf(const QByteArray& dwgBytes, QByteArray& dxfOut, QString* err,
              int timeoutMs) {
    if (err) err->clear();
    if (!available()) {
        if (err) *err = QStringLiteral("未安装 DWG 转换组件（tools/dwg），无法读取 %1")
                            .arg(versionHuman(versionFromBytes(dwgBytes)));
        return false;
    }
    const QString ver = versionFromBytes(dwgBytes);
    const QString verCtx = ver.isEmpty() ? QStringLiteral("版本未知")
                                         : QStringLiteral("%1 / %2").arg(ver, versionHuman(ver));
    if (ver.isEmpty()) {
        if (err) *err = QStringLiteral("不是有效的 DWG 文件（头部无版本串）");
        return false;
    }
    if (!canReadVersion(ver)) {
        if (err) *err = QStringLiteral("不支持的 DWG 版本：%1（%2）").arg(ver, versionHuman(ver));
        return false;
    }
    // dwg2dxf <in.dwg> -o <out.dxf>（-y 覆盖已有输出；LibreDWG CLI 契约）
    return convert(converterPath("dwg2dxf"),
                   { QStringLiteral("@IN@"), QStringLiteral("-y"),
                     QStringLiteral("-o"), QStringLiteral("@OUT@") },
                   dwgBytes, QStringLiteral("in.dwg"), QStringLiteral("out.dxf"),
                   dxfOut, err, verCtx, timeoutMs);
}

// ---------------------------------------------------------------------------
// 写桥：DXF(R2000) → DWG R2000（dwgwrite --as r2000 -I DXF）
// ---------------------------------------------------------------------------
bool dxfToDwgR2000(const QByteArray& dxfBytes, QByteArray& dwgOut, QString* err,
                   int timeoutMs) {
    if (err) err->clear();
    if (!available()) {
        if (err) *err = QStringLiteral("未安装 DWG 转换组件（tools/dwg），无法另存为 DWG R2000");
        return false;
    }
    // dwgwrite --as r2000 -I DXF -o <out.dwg> <in.dxf>（-y 覆盖）
    return convert(converterPath("dwgwrite"),
                   { QStringLiteral("--as"), QStringLiteral("r2000"),
                     QStringLiteral("-I"), QStringLiteral("DXF"),
                     QStringLiteral("-y"),
                     QStringLiteral("-o"), QStringLiteral("@OUT@"),
                     QStringLiteral("@IN@") },
                   dxfBytes, QStringLiteral("in.dxf"), QStringLiteral("out.dwg"),
                   dwgOut, err, QStringLiteral("DWG R2000"), timeoutMs);
}

void setToolsDirForTest(const QString& dir) {
    g_testToolsDir = dir;
}

} // namespace dwgbridge
} // namespace SketchCAD
