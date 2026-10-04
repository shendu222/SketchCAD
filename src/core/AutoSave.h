#pragma once
#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class QTimer;

namespace SketchCAD {

// 自动保存核心：定时 + 操作计数双触发，原子落盘，启动后可恢复。
//
// 落盘位置：%LOCALAPPDATA%/SketchCAD/autosave/
// 命名规则：对一张图纸 <原路径> 生成快照 <basename>.<hash8>.sv$
//   —— 原路径不直接做文件名（可能含盘符/非法字符），取「文件名 + 原路径哈希」避免同名冲突。
//   —— .sv$ 是约定的「自动保存快照」后缀；恢复对话框按此前缀扫描。
//
// 原子写：先写 <name>.tmp → flush → QFile::rename 覆盖正式 .sv$。
//   —— Qt 的 QFile::rename 在目标已存在时会失败，故先 remove 目标再 rename；
//      为避免 remove 后崩溃导致快照丢失，先写 .bak 备份再 remove+rename。
// 磁盘告警：写前检查剩余空间 < 阈值（默认 100MB）→ 发 lowDisk 信号（不阻断写入）。
class AutoSave : public QObject {
    Q_OBJECT
public:
    // 一条可恢复快照的描述
    struct Snapshot {
        QString   file;        // 快照文件名（含 .sv$）
        QString   fullPath;    // 快照完整路径
        QString   originPath;  // 原图纸路径（从快照头部元信息解析）
        QDateTime time;        // 快照时间
        qint64    size = 0;    // 字节数
    };

    explicit AutoSave(QObject* parent = nullptr);
    ~AutoSave() override;

    // ---- 纯函数区（静态，无状态，便于无头测试）----

    // 快照目录：%LOCALAPPDATA%/SketchCAD/autosave（不存在则创建）
    static QString snapshotDir();

    // 由原图纸路径推导快照文件名：<basename>.<hash8>.sv$
    static QString snapshotNameFor(const QString& originPath);

    // 原子写一份快照（自动补 .bak）。成功返回 true；失败写 *err（明确中文原因）。
    static bool writeSnapshot(const QString& fullPath, const QByteArray& bytes,
                              QString* err = nullptr);

    // 扫描目录内全部 .sv$ 快照，按时间倒序返回（最新在前）。目录不存在 → 空表。
    static QVector<Snapshot> scanSnapshots();

    // 读取一份快照的原始字节（不存在/读失败 → 空 + *err）
    static QByteArray readSnapshot(const QString& fullPath, QString* err = nullptr);

    // 把快照恢复写回目标路径（原子）。成功返回 true。目标与快照同路径时直接返回 false。
    static bool restoreSnapshot(const QString& snapPath, const QString& targetPath,
                                QString* err = nullptr);

    // 丢弃一份快照（连同 .bak / .tmp）。成功返回 true（文件不存在也算成功）。
    static bool discardSnapshot(const QString& snapPath);

    // 正常退出时清理所有 .tmp 残留（半写文件无恢复价值）。返回清理条数。
    static int cleanupTempFiles();

    // 目标卷剩余字节数；取不到（如路径不存在）返回 -1。
    static qint64 freeBytesFor(const QString& path);

    // ---- 有状态区（QObject：定时器 + 操作计数）----

    // 快照字节提供者：由 MainWindow 接 model_->toDrawingBytes()。
    // 返回空 QByteArray 表示本次不写（例如空图）。
    void setProvider(std::function<QByteArray()> fn) { provider_ = std::move(fn); }

    // 当前图纸路径（写文件/另存为后更新）。空 = 尚未命名。
    void setCurrentDrawing(const QString& p) { currentDrawing_ = p; }
    QString currentDrawing() const { return currentDrawing_; }

    // 间隔（分钟）；0 = 关闭定时触发。立即重建定时器。
    void setIntervalMinutes(int m);
    int  intervalMinutes() const { return intervalMinutes_; }

    // 累计操作数阈值；<=0 = 关闭计数触发。
    void setOperationThreshold(int n) { opThreshold_ = n; }
    int  operationThreshold() const { return opThreshold_; }

    // 磁盘告警阈值（MB），默认 100。
    void setLowDiskThresholdMb(int mb) { lowDiskMb_ = mb; }
    int  lowDiskThresholdMb() const { return lowDiskMb_; }

    void start();
    void stop();
    bool isActive() const;

    // 记一次「用户操作」（由 modelChanged 驱动）。达阈值立即触发一次保存。
    void noteOperation();

    // 手动/定时触发一次保存。成功发 saved()，失败发 saveFailed()。
    bool saveNow();

    // 上次成功保存时间（供状态栏显示）
    QDateTime lastSaved() const { return lastSaved_; }

signals:
    void saved(const QString& snapPath);
    void saveFailed(const QString& reason);
    void lowDisk(qint64 freeBytes, int thresholdMb);

private:
    QTimer* timer_ = nullptr;
    std::function<QByteArray()> provider_;
    QString currentDrawing_;
    int  intervalMinutes_ = 10;
    int  opThreshold_ = 0;      // 默认不按操作数触发（0=关），可后续开放
    int  lowDiskMb_ = 100;
    int  opCount_ = 0;
    QDateTime lastSaved_;
};

} // namespace SketchCAD
