#include "AutoSave.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTimer>
#include <QDateTime>

namespace SketchCAD {

namespace {
    // 快照文件魔数与头格式。头部每行 "KEY=VALUE"，空行后即为 JSON 负载。
    // 用 base64 编码 origin，避免路径里的特殊字符破坏行结构。
    const char* kMagic = "SCAUTOSAVE1";
    const char* kHeaderEnd = "\n\n";
}

// ---------- 纯函数区 ----------

QString AutoSave::snapshotDir() {
    QString local = QDir::fromNativeSeparators(
        QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")));
    if (local.isEmpty()) local = QDir::homePath() + "/AppData/Local";
    if (local.isEmpty()) local = QDir::homePath();
    const QString dir = local + "/SketchCAD/autosave";
    QDir().mkpath(dir);
    return dir;
}

QString AutoSave::snapshotNameFor(const QString& originPath) {
    // 取原路径的文件名部分做可读前缀，非法字符替换为 '_'；再拼原路径哈希 8 位防同名冲突。
    QString base = QFileInfo(originPath).fileName();
    if (base.isEmpty()) base = QStringLiteral("untitled");
    QString clean;
    for (QChar c : base) {
        if (c.isLetterOrNumber() || c == u'.' || c == u'_' || c == u'-')
            clean.append(c);
        else
            clean.append(u'_');
    }
    const QByteArray h = QCryptographicHash::hash(originPath.toUtf8(),
                                                 QCryptographicHash::Md5).toHex().left(8);
    return QStringLiteral("%1.%2.sv$").arg(clean, QString::fromLatin1(h));
}

bool AutoSave::writeSnapshot(const QString& fullPath, const QByteArray& bytes, QString* err) {
    QFileInfo fi(fullPath);
    QDir().mkpath(fi.absolutePath());
    const QString tmp  = fullPath + QStringLiteral(".tmp");
    const QString bak  = fullPath + QStringLiteral(".bak");

    // 1) 先写 .tmp（完整写入并 flush）
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (err) *err = QStringLiteral("无法写入临时文件：%1").arg(f.errorString());
            return false;
        }
        const qint64 n = f.write(bytes);
        if (n != (qint64)bytes.size()) {
            f.close();
            QFile::remove(tmp);
            if (err) *err = QStringLiteral("写入临时文件不完整（%1/%2 字节）").arg(n).arg(bytes.size());
            return false;
        }
        f.flush();
        f.close();
    }

    // 2) 旧版本留一份 .bak
    if (QFile::exists(fullPath)) {
        QFile::remove(bak);
        QFile::rename(fullPath, bak);
    }

    // 3) 原子替换：QFile::rename 目标已存在会失败，故此时目标已不存在（或本就不存在）
    if (!QFile::rename(tmp, fullPath)) {
        // 兜底：再试一次（极端文件系统竞态）
        QFile::remove(fullPath);
        if (!QFile::rename(tmp, fullPath)) {
            if (err) *err = QStringLiteral("无法提交快照文件：%1").arg(fullPath);
            return false;
        }
    }
    return true;
}

QVector<AutoSave::Snapshot> AutoSave::scanSnapshots() {
    QVector<Snapshot> out;
    const QString dir = snapshotDir();
    QDir d(dir);
    if (!d.exists()) return out;
    const QStringList files = d.entryList(QStringList() << QStringLiteral("*.sv$"),
                                          QDir::Files, QDir::Time);
    for (const QString& fn : files) {
        const QString fp = dir + QLatin1Char('/') + fn;
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly)) continue;
        // 解析头（读前若干字节即可；负载可能很大，只读头部）
        Snapshot s;
        s.file = fn;
        s.fullPath = fp;
        s.size = f.size();
        s.time = QFileInfo(fp).lastModified();
        // 逐行读头直到空行
        QTextStream ts(&f);
        bool first = true;
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            if (first) {
                first = false;
                if (line != QLatin1String(kMagic)) break;   // 非本格式
                continue;
            }
            if (line.isEmpty()) break;
            const int eq = line.indexOf(u'=');
            if (eq < 0) continue;
            const QString k = line.left(eq);
            const QString v = line.mid(eq + 1);
            if (k == QLatin1String("T"))
                s.time = QDateTime::fromString(v, Qt::ISODate);
            else if (k == QLatin1String("O"))
                s.originPath = QString::fromUtf8(QByteArray::fromBase64(v.toLatin1()));
        }
        f.close();
        out.append(s);
    }
    return out;
}

QByteArray AutoSave::readSnapshot(const QString& fullPath, QString* err) {
    QFile f(fullPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取快照：%1").arg(f.errorString());
        return {};
    }
    QByteArray all = f.readAll();
    f.close();
    const QByteArray magic = QByteArray(kMagic);
    if (!all.startsWith(magic)) {
        // 无头（测试或异常产物）→ 整体视为负载
        return all;
    }
    const int hdrEnd = all.indexOf(QByteArray(kHeaderEnd));
    if (hdrEnd < 0) {
        if (err) *err = QStringLiteral("快照头部损坏");
        return {};
    }
    return all.mid(hdrEnd + 2);   // +2 跳过 "\n\n"
}

bool AutoSave::restoreSnapshot(const QString& snapPath, const QString& targetPath,
                               QString* err) {
    if (QFileInfo(snapPath) == QFileInfo(targetPath)) {
        if (err) *err = QStringLiteral("源与目标相同");
        return false;
    }
    const QByteArray payload = readSnapshot(snapPath, err);
    if (payload.isEmpty()) {
        if (err && err->isEmpty()) *err = QStringLiteral("快照内容为空");
        return false;
    }
    // 恢复也走原子写：先写 target.tmp 再替换，避免恢复中途崩溃损坏原文件
    const QString tmp = targetPath + QStringLiteral(".recover.tmp");
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (err) *err = QStringLiteral("无法写入恢复文件：%1").arg(f.errorString());
            return false;
        }
        f.write(payload);
        f.flush();
        f.close();
    }
    if (QFile::exists(targetPath)) QFile::remove(targetPath);
    if (!QFile::rename(tmp, targetPath)) {
        if (err) *err = QStringLiteral("无法提交恢复文件：%1").arg(targetPath);
        return false;
    }
    return true;
}

bool AutoSave::discardSnapshot(const QString& snapPath) {
    bool ok = true;
    ok = QFile::remove(snapPath) || !QFile::exists(snapPath);
    QFile::remove(snapPath + QStringLiteral(".bak"));
    QFile::remove(snapPath + QStringLiteral(".tmp"));
    QFile::remove(snapPath + QStringLiteral(".recover.tmp"));
    return ok;
}

int AutoSave::cleanupTempFiles() {
    QDir d(snapshotDir());
    if (!d.exists()) return 0;
    int n = 0;
    const QStringList tmps = d.entryList(QStringList()
                                         << QStringLiteral("*.tmp")
                                         << QStringLiteral("*.recover.tmp"),
                                         QDir::Files);
    for (const QString& f : tmps) {
        if (QFile::remove(d.filePath(f))) ++n;
    }
    return n;
}

qint64 AutoSave::freeBytesFor(const QString& path) {
    QStorageInfo si(path);
    if (!si.isValid() || !si.isReady()) return -1;
    return si.bytesFree();
}

// ---------- 有状态区 ----------

AutoSave::AutoSave(QObject* parent) : QObject(parent) {
    timer_ = new QTimer(this);
    timer_->setSingleShot(false);
    connect(timer_, &QTimer::timeout, this, [this]{ saveNow(); });
}

AutoSave::~AutoSave() = default;

void AutoSave::setIntervalMinutes(int m) {
    intervalMinutes_ = (m < 0) ? 0 : m;
    if (timer_) {
        if (intervalMinutes_ > 0 && timer_->isActive())
            timer_->start(intervalMinutes_ * 60 * 1000);
        else
            timer_->stop();
    }
}

void AutoSave::start() {
    if (intervalMinutes_ > 0 && timer_) timer_->start(intervalMinutes_ * 60 * 1000);
}

void AutoSave::stop() {
    if (timer_) timer_->stop();
}

bool AutoSave::isActive() const {
    return timer_ && timer_->isActive();
}

void AutoSave::noteOperation() {
    ++opCount_;
    if (opThreshold_ > 0 && opCount_ >= opThreshold_) {
        opCount_ = 0;
        saveNow();
    }
}

bool AutoSave::saveNow() {
    if (!provider_) return false;
    const QByteArray payload = provider_();
    if (payload.isEmpty()) return false;   // 空图不写

    // 磁盘余量告警（不阻断）
    const QString dir = snapshotDir();
    const qint64 free = freeBytesFor(dir);
    if (free >= 0 && lowDiskMb_ > 0 && free < (qint64)lowDiskMb_ * 1024 * 1024)
        emit lowDisk(free, lowDiskMb_);

    const QString origin = currentDrawing_.isEmpty()
                               ? QStringLiteral("(未保存)")
                               : currentDrawing_;
    const QString full = dir + QLatin1Char('/') + snapshotNameFor(origin);

    // 组装带元信息头的完整快照
    QByteArray blob(kMagic);
    blob += "\nT=" + QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8() + "\n";
    blob += "O=" + origin.toUtf8().toBase64() + "\n";
    blob += "S=" + QByteArray::number(payload.size()) + "\n";
    blob += kHeaderEnd;
    blob += payload;

    QString err;
    if (!writeSnapshot(full, blob, &err)) {
        emit saveFailed(err);
        return false;
    }
    opCount_ = 0;
    lastSaved_ = QDateTime::currentDateTime();
    emit saved(full);
    return true;
}

} // namespace SketchCAD
