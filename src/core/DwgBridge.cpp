#include "DwgBridge.h"
#include "CadFormats.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QDebug>

namespace SketchCAD {
namespace dwgbridge {

// ---------------------------------------------------------------------------
// 转换器目录解析（查找顺序见头注释；无单例——魔静态只放 POD 串，无再入风险）
// ---------------------------------------------------------------------------
static QString g_testToolsDir;   // 自测注入（setToolsDirForTest）

static QString toolsDir() {
    if (!g_testToolsDir.isEmpty()) {
        qWarning() << "[DwgBridge] toolsDir: 使用自测注入目录 =" << g_testToolsDir;
        return g_testToolsDir;
    }
    const QString envDir = qEnvironmentVariable("SKETCHCAD_DWGTOOLS");
    if (!envDir.isEmpty()) {
        qWarning() << "[DwgBridge] toolsDir: 使用环境变量 SKETCHCAD_DWGTOOLS =" << envDir;
        return envDir;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    qWarning() << "[DwgBridge] toolsDir: 起始 appDir =" << appDir;
    if (!appDir.isEmpty()) {
        // 安装包布局：<exe目录>/tools/dwg/dwg2dxf.exe …（优先，存在即采用）
        const QString local = appDir + QStringLiteral("/tools/dwg");
        if (QFile::exists(local + QStringLiteral("/dwg2dxf.exe"))
            || QFile::exists(local + QStringLiteral("/dwg2dxf"))) {
            qWarning() << "[DwgBridge] toolsDir: 命中 exe 同级 tools/dwg =" << local;
            return local;
        }
        // 开发布局回退：从 exe 目录逐层向上，查找 <某级>/thirdparty/libredwg-*/build-mingw
        // 或 <某级>/libredwg-*/build-mingw。本机布局转换器在
        //   02-avticationCAD/thirdparty/libredwg-0.14/build-mingw
        // （CMAKE_SOURCE_DIR=sketch-cad-cpp，故在 build 目录两级之上级），必须用向上搜索，
        // 不能只查 build/.. 一级，否则永远找不到而静默报"未安装"。
        // 转换器为独立 GPL 工程，不进入本工程；此处仅做发现，避免未部署 tools/dwg
        // 时无法打开合法 DWG。
        QDir cur(appDir);
        for (int depth = 0; depth < 6; ++depth) {
            // 模式 A：<级>/thirdparty/libredwg-*/build-mingw
            QDir pdir(cur.absolutePath() + QStringLiteral("/thirdparty"));
            if (pdir.exists()) {
                pdir.setNameFilters(QStringList() << QStringLiteral("libredwg-*"));
                pdir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString& d : pdir.entryList()) {
                    const QString cand = pdir.absoluteFilePath(d) + QStringLiteral("/build-mingw");
                    if (QFile::exists(cand + QStringLiteral("/dwg2dxf.exe"))) {
                        qWarning() << "[DwgBridge] toolsDir: 命中向上搜索 thirdparty/" << d << "/build-mingw =" << cand;
                        return cand;
                    }
                }
            }
            // 模式 B：<级>/libredwg-*/build-mingw（平铺在工程根的情况）
            QDir sib(cur.absolutePath());
            sib.setNameFilters(QStringList() << QStringLiteral("libredwg-*"));
            sib.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& d : sib.entryList()) {
                const QString cand = sib.absoluteFilePath(d) + QStringLiteral("/build-mingw");
                if (QFile::exists(cand + QStringLiteral("/dwg2dxf.exe"))) {
                    qWarning() << "[DwgBridge] toolsDir: 命中向上搜索" << d << "/build-mingw =" << cand;
                    return cand;
                }
            }
            if (!cur.cdUp()) break;  // 已达文件系统根，停止
        }
    }
    qWarning() << "[DwgBridge] toolsDir: 未找到任何转换器目录（available() 将返回 false）";
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
    if (QFile::exists(p)) {
        qWarning() << "[DwgBridge] converterPath:" << exeName << "->" << p;
        return p;
    }
    qWarning() << "[DwgBridge] converterPath: 缺失" << exeName << "（期望路径" << p << "）";
    return QString();
}

bool available() {
    const bool a = !converterPath("dwg2dxf").isEmpty();
    const bool b = !converterPath("dwgwrite").isEmpty();
    qWarning() << "[DwgBridge] available(): dwg2dxf=" << a << "dwgwrite=" << b
               << "=>" << (a && b);
    return a && b;
}

QString resolvedToolsDir() {
    return toolsDir();
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
    qWarning() << "[DwgBridge] runConverter: 启动" << exe
               << "参数" << args
               << "工作目录" << workDir << "超时(ms)" << timeoutMs;
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    // 将转换器自身目录加入 PATH，确保随附 DLL 在进程外调用时可解析（静态链接时无害）
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString exeDir = QFileInfo(exe).absolutePath();
        const QString sysPath = env.value(QStringLiteral("PATH"));
        env.insert(QStringLiteral("PATH"), exeDir + QDir::listSeparator() + sysPath);
        proc.setProcessEnvironment(env);
    }
    proc.start(exe, args);
    if (!proc.waitForStarted(5000)) {
        // QProcess::error 枚举：0=FailedToStart 1=Crashed 2=Timedout 4=WriteError 5=Unknown
        r.err = QStringLiteral("DWG 转换器无法启动: %1（QProcess::error=%2）")
                    .arg(exe).arg(int(proc.error()));
        qWarning() << "[DwgBridge] runConverter: 启动失败" << r.err
                   << "errorString=" << proc.errorString();
        return r;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        if (proc.state() == QProcess::Running) proc.kill();
        proc.waitForFinished(2000);
        r.err = QStringLiteral("DWG 转换器超时（>%1s）或无响应").arg(timeoutMs / 1000);
        qWarning() << "[DwgBridge] runConverter: 超时" << r.err;
        return r;
    }
    if (proc.exitStatus() == QProcess::CrashExit) {
        r.err = QStringLiteral("DWG 转换器异常退出（文件可能损坏或含不支持结构）");
        qWarning() << "[DwgBridge] runConverter: 崩溃" << r.err;
        return r;
    }
    if (proc.exitCode() != 0) {
        const QByteArray msg = proc.readAllStandardError();
        r.err = QStringLiteral("DWG 转换器返回错误（exit=%1）%2")
                    .arg(proc.exitCode())
                    .arg(msg.isEmpty() ? QString()
                                       : QStringLiteral(": %1").arg(
                                             QString::fromLocal8Bit(msg).left(200).trimmed()));
        qWarning() << "[DwgBridge] runConverter: 非零退出 code=" << proc.exitCode()
                   << "stderr=" << QString::fromLocal8Bit(msg).left(200).trimmed();
        return r;
    }
    qWarning() << "[DwgBridge] runConverter: 成功" << exe;
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
// 读桥：DWG → DXF（dwg2dxf，输出 r2004 DXF 供 cadfmt::fromDxf 回读；
//       LibreDWG 的 r2004 输出 $ACADVER=AC1018，落在 fromDxf 白名单内）
// ---------------------------------------------------------------------------
bool dwgToDxf(const QByteArray& dwgBytes, QByteArray& dxfOut, QString* err,
              int timeoutMs) {
    if (err) err->clear();
    qWarning() << "[DwgBridge] dwgToDxf: 入口，输入字节数 =" << dwgBytes.size();
    if (!available()) {
        if (err) *err = QStringLiteral("未安装 DWG 转换组件（tools/dwg），无法读取 %1")
                            .arg(versionHuman(versionFromBytes(dwgBytes)));
        qWarning() << "[DwgBridge] dwgToDxf: available()==false，未安装转换组件（tools/dwg 缺失）";
        return false;
    }
    const QString ver = versionFromBytes(dwgBytes);
    const QString verCtx = ver.isEmpty() ? QStringLiteral("版本未知")
                                         : QStringLiteral("%1 / %2").arg(ver, versionHuman(ver));
    qWarning() << "[DwgBridge] dwgToDxf: 探测版本 =" << (ver.isEmpty() ? QStringLiteral("(无)") : ver)
               << "human=" << versionHuman(ver);
    if (ver.isEmpty()) {
        if (err) *err = QStringLiteral("不是有效的 DWG 文件（头部无版本串）");
        qWarning() << "[DwgBridge] dwgToDxf: 非法文件（头部无 AC1xNN 版本串）";
        return false;
    }
    if (!canReadVersion(ver)) {
        if (err) *err = QStringLiteral("不支持的 DWG 版本：%1（%2）").arg(ver, versionHuman(ver));
        qWarning() << "[DwgBridge] dwgToDxf: 版本不在可读取集合（canReadVersion=false）";
        return false;
    }
    // dwg2dxf <in.dwg> --as r2004 -y -o <out.dxf>
    // ⚠️ 关键①：必须显式 --as。dwg2dxf 默认【保留输入版本】，若不加该参数，
    // AC1021(DWG 2004) 会被转成「AC1021 DXF」，而 cadfmt::fromDxf 的版本协商
    // 只接受 ≤ AC1018，于是回读时直接报「不支持的文件版本」——这是 DWG 2004
    // 经桥转换后仍打不开的第一层根因。
    // ⚠️ 关键②：必须选 r2004 而非 r2000。LibreDWG 在把动态块(DynamicBlock)
    // 降级到 R2000 DXF 时会【丢弃块内几何】（实测：WAGO 端子块/M5螺孔等块
    // 在 r2000 输出中只剩 BLOCK 记录头、零图元；r2004 输出保留 49 LINE+8
    // CIRCLE/块）。r2004 降级时块几何完整，且 LibreDWG 的 r2004 输出
    // $ACADVER 实际写作 AC1018（本就在 fromDxf 白名单内，无需放宽版本契约）。
    const bool ok = convert(converterPath("dwg2dxf"),
                   { QStringLiteral("@IN@"), QStringLiteral("-y"),
                     QStringLiteral("--as"), QStringLiteral("r2004"),
                     QStringLiteral("-o"), QStringLiteral("@OUT@") },
                   dwgBytes, QStringLiteral("in.dwg"), QStringLiteral("out.dxf"),
                   dxfOut, err, verCtx, timeoutMs);
    if (!ok)
        qWarning() << "[DwgBridge] dwgToDxf: 转换失败" << (err ? *err : QString());
    else
        qWarning() << "[DwgBridge] dwgToDxf: 成功，输出 DXF 字节数 =" << dxfOut.size();
    return ok;
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
