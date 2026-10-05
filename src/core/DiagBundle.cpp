#include "DiagBundle.h"

#include "Diagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSysInfo>
#include <QtGlobal>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace SketchCAD {

namespace DiagBundle {

// ---------------------------------------------------------------------------
// CRC32（IEEE 反射多项式 0xEDB88320）：表在首次调用时循环生成，
// 不写字面量巨型表（见 CommandCatalog 的 47+ 条聚合初始化教训）。
// ---------------------------------------------------------------------------
quint32 crc32(const QByteArray& data) {
    static const quint32* table = [] {
        static quint32 t[256];
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();
    quint32 crc = 0xFFFFFFFFu;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data.constData());
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// ZipBuilder（Stored）
// ---------------------------------------------------------------------------
namespace {
    constexpr quint32 kLocalHeaderSig  = 0x04034b50u;   // PK\x03\x04
    constexpr quint32 kCentralDirSig   = 0x02014b50u;   // PK\x01\x02
    constexpr quint32 kEocdSig         = 0x06054b50u;   // PK\x05\x06
    constexpr quint16 kZipVersion      = 20;            // 2.0：仅 Stored 所需最低版本

    void put16(QByteArray& out, quint16 v) {
        out.append(char(v & 0xFFu)); out.append(char((v >> 8) & 0xFFu));
    }
    void put32(QByteArray& out, quint32 v) {
        out.append(char(v & 0xFFu));        out.append(char((v >> 8) & 0xFFu));
        out.append(char((v >> 16) & 0xFFu)); out.append(char((v >> 24) & 0xFFu));
    }

    // ZIP 的 DOS 时间戳（本地时间，2 秒粒度）
    void dosStamp(quint16* dosTime, quint16* dosDate) {
        const QDateTime now = QDateTime::currentDateTime();
        const QTime t = now.time();
        const QDate d = now.date();
        *dosTime = quint16((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
        *dosDate = quint16(((qMax(1980, d.year()) - 1980) << 9) | (d.month() << 5) | d.day());
    }

    // ZIP 内名字：统一正斜杠 + UTF-8 字节
    QByteArray zipNameBytes(const QString& name) {
        QString n = name;
        n.replace(QChar('\\'), QChar('/'));
        while (n.startsWith(QChar('/'))) n.remove(0, 1);
        return n.toUtf8();
    }
} // namespace

void ZipBuilder::addFile(const QString& name, const QByteArray& bytes) {
    Entry e;
    e.name = name;
    e.bytes = bytes;
    e.crc = crc32(bytes);
    entries_.append(e);
}

bool ZipBuilder::isEmpty() const { return entries_.isEmpty(); }

QByteArray ZipBuilder::finish() const {
    const quint16 dosTime = [] { quint16 t = 0, d = 0; dosStamp(&t, &d); return t; }();
    const quint16 dosDate = [] { quint16 t = 0, d = 0; dosStamp(&t, &d); return d; }();

    QByteArray out;
    QVector<quint32> offsets;
    offsets.reserve(entries_.size());

    // ---- local file header + data ----
    for (const Entry& e : entries_) {
        const QByteArray name = zipNameBytes(e.name);
        offsets.append(quint32(out.size()));
        put32(out, kLocalHeaderSig);
        put16(out, kZipVersion);        // version needed
        put16(out, 0x0800u);            // flags: bit11 = UTF-8 名字
        put16(out, 0);                  // method = 0（Stored）
        put16(out, dosTime);
        put16(out, dosDate);
        put32(out, e.crc);
        put32(out, quint32(e.bytes.size()));   // compressed size
        put32(out, quint32(e.bytes.size()));   // uncompressed size
        put16(out, quint16(name.size()));
        put16(out, 0);                  // extra len
        out.append(name);
        out.append(e.bytes);
    }

    // ---- central directory ----
    const quint32 cdOffset = quint32(out.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const Entry& e = entries_[i];
        const QByteArray name = zipNameBytes(e.name);
        put32(out, kCentralDirSig);
        put16(out, kZipVersion);        // version made by
        put16(out, kZipVersion);        // version needed
        put16(out, 0x0800u);            // flags: UTF-8
        put16(out, 0);                  // method
        put16(out, dosTime);
        put16(out, dosDate);
        put32(out, e.crc);
        put32(out, quint32(e.bytes.size()));
        put32(out, quint32(e.bytes.size()));
        put16(out, quint16(name.size()));
        put16(out, 0); put16(out, 0);   // extra / comment len
        put16(out, 0); put16(out, 0);   // disk start / internal attrs
        put32(out, 0);                  // external attrs
        put32(out, offsets[i]);
        out.append(name);
    }
    const quint32 cdSize = quint32(out.size()) - cdOffset;

    // ---- EOCD ----
    put32(out, kEocdSig);
    put16(out, 0); put16(out, 0);       // 磁盘号 / 中央目录起始盘
    put16(out, quint16(entries_.size()));
    put16(out, quint16(entries_.size()));
    put32(out, cdSize);
    put32(out, cdOffset);
    put16(out, 0);                      // comment len
    return out;
}

// ---------------------------------------------------------------------------
// 系统信息
// ---------------------------------------------------------------------------
namespace {
    QStringList gpuNames() {
        QStringList out;
#ifdef _WIN32
        DISPLAY_DEVICEW dd;
        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
        for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
            if (dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;
            const QString name = QString::fromWCharArray(dd.DeviceString);
            if (!name.isEmpty() && !out.contains(name)) out << name;
        }
#endif
        if (out.isEmpty()) out << QStringLiteral("未知");
        return out;
    }
}

QString systemInfoText() {
    QString text;
    text += QStringLiteral("SketchCAD 诊断包 - 系统信息\n");
    text += QStringLiteral("========================================\n");
    text += QStringLiteral("生成时间: %1\n")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
#ifdef SKETCHCAD_VERSION
    text += QStringLiteral("程序版本: %1\n").arg(QStringLiteral(SKETCHCAD_VERSION));
#else
    text += QStringLiteral("程序版本: 未知（自测构建）\n");
#endif
    text += QStringLiteral("Qt 版本: %1\n").arg(QString::fromLatin1(qVersion()));
    text += QStringLiteral("操作系统: %1\n").arg(QSysInfo::prettyProductName());
    text += QStringLiteral("CPU 架构: %1\n").arg(QSysInfo::currentCpuArchitecture());
    text += QStringLiteral("构建 ABI: %1\n").arg(QSysInfo::buildAbi());
    text += QStringLiteral("平台插件: %1\n").arg(QGuiApplication::platformName());
    text += QStringLiteral("显卡: %1\n").arg(gpuNames().join(QStringLiteral("; ")));
    return text;
}

// ---------------------------------------------------------------------------
// 源文件收集
// ---------------------------------------------------------------------------
QStringList collectSourceFiles() {
    QStringList out;
    // 滚动日志三份（存在才收）
    const QString logBase = Diagnostics::logDir() + QStringLiteral("/SketchCAD.log");
    for (const QString& suffix : { QString(), QStringLiteral(".1"), QStringLiteral(".2") }) {
        const QString p = logBase + suffix;
        if (QFileInfo::exists(p)) out << p;
    }
    // 崩溃摘要 last.txt + 按时间戳命名的 crash-*.{dmp,txt}（名字降序 = 最新在前）
    QDir crash(Diagnostics::crashDir());
    if (QFileInfo::exists(crash.filePath(QStringLiteral("last.txt"))))
        out << crash.filePath(QStringLiteral("last.txt"));
    const QStringList crashes =
        crash.entryList(QStringList{ QStringLiteral("crash-*.dmp"), QStringLiteral("crash-*.txt") },
                        QDir::Files, QDir::Name | QDir::Reversed);
    int taken = 0;
    for (const QString& f : crashes) {
        if (taken >= 3) break;          // 最近 3 组，控制包体积
        out << crash.filePath(f);
        ++taken;
    }
    return out;
}

// ---------------------------------------------------------------------------
// 一键打包
// ---------------------------------------------------------------------------
QString exportBundle(const QVariantMap& drawingMeta,
                     const QString& outputDir, QString* err) {
    auto fail = [err](const QString& why) {
        if (err) *err = why;
        Diagnostics::writeLog(QStringLiteral("诊断包导出失败：%1").arg(why), "error");
        return QString();
    };

    ZipBuilder zip;
    zip.addFile(QStringLiteral("system-info.txt"), systemInfoText().toUtf8());

    // 图纸元信息：只写调用方给的计数类字段（JSON 对象）；绝不在此触碰 SketchModel
    QJsonObject metaObj;
    for (auto it = drawingMeta.constBegin(); it != drawingMeta.constEnd(); ++it)
        metaObj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    metaObj.insert(QStringLiteral("generatedAt"),
                   QDateTime::currentDateTime().toString(Qt::ISODate));
    zip.addFile(QStringLiteral("drawing-meta.json"),
                QJsonDocument(metaObj).toJson(QJsonDocument::Indented));

    const QStringList sources = collectSourceFiles();
    for (const QString& p : sources) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) continue;   // 打不开的跳过（可能正被占用）
        zip.addFile(QFileInfo(p).fileName(), f.readAll());
    }

    QDir dir(outputDir.isEmpty() ? QDir::homePath() : outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return fail(QStringLiteral("无法创建输出目录：%1").arg(dir.absolutePath()));

    const QString zipPath = dir.filePath(QStringLiteral("SketchCAD-Diag-%1.zip")
                                             .arg(QDateTime::currentDateTime()
                                                      .toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    QFile out(zipPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("无法写入 %1").arg(zipPath));
    const QByteArray bytes = zip.finish();
    if (out.write(bytes) != bytes.size())
        return fail(QStringLiteral("写入不完整：%1").arg(zipPath));
    out.close();

    Diagnostics::writeLog(QStringLiteral("已导出诊断包：%1（包含 %2 个文件）")
                              .arg(zipPath).arg(sources.size() + 2), "info");
    if (err) err->clear();
    return zipPath;
}

} // namespace DiagBundle

} // namespace SketchCAD
