#pragma once
#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace SketchCAD {

class AppSettings;   // 仅以指针引用；AppSettings.h 在 cpp 中包含

// 最近文件（MRU）：富条目 = 路径 + 打开时间 + 固定标记（pin）。
//
// 职责与边界：
//   * 纯 core 组件，仅依赖 AppSettings 的通用键值层（键 files/recentEntries，
//     JSON 数组文本），可注入临时 QSettings 离线直测（与 t_appsettings 同模式）；
//   * 硬上限 kMaxEntries=10：add 裁剪时优先挤掉最旧的**未固定**项，全固定时
//     才挤最旧项（硬上限优先于 pin——列表不允许无限增长）；
//   * pin 只影响裁剪，不影响手动删除（remove 可删任何条目）；
//   * 每次变更立即持久化（AppSettings::setValue 自带 sync），崩溃也不丢 MRU；
//   * 旧版兼容：首次读取时若无新键而有旧键 files/recentList（纯路径表），
//     一次性迁移到新键（打开时间=迁移时刻，pin=false），旧键保留不动（可降级）。
class RecentFiles : public QObject {
    Q_OBJECT
public:
    // 硬上限：无论「选项-最近文件数」如何设置，存储条目不超过 10 条
    static constexpr int kMaxEntries = 10;

    // 一条最近文件记录
    struct RecentEntry {
        QString   path;
        QDateTime openedAt;
        bool      pinned = false;
    };

    // settings 可为 nullptr（仅内存态，不持久化——测试便捷构造）
    explicit RecentFiles(AppSettings* settings, QObject* parent = nullptr);

    // ---- 只读访问 ----
    const QVector<RecentEntry>& entries() const { return entries_; }
    int  count() const { return entries_.size(); }
    bool contains(const QString& path) const;
    bool isPinned(const QString& path) const;
    // 第 index 条的文件是否已不存在（index 越界返回 false）
    bool isMissing(int index) const;

    // ---- 变更（全部：内存更新 + 持久化 + 发 changed）----
    void add(const QString& path);                  // 去重置顶 + 刷新打开时间（pin 保留）+ 裁剪
    void remove(const QString& path);               // 删除单条（不存在则无操作）
    void removeAt(int index);
    void setPinned(const QString& path, bool on);
    void clear();                                   // 全部清除（含 pin）

    // 自检后重载（外部直接改了底层存储时调用，如「选项-取消回滚/重置/导入」）。
    // 底层文本与最近一次自写内容一致时为无操作（避免 自己写→信号→重读→再写 的环）。
    void reloadFromSettings();

    // 测试/MainWindow 注入开关：false = 只改内存不落盘（离屏测试不写注册表）
    void setPersistEnabled(bool on) { persistEnabled_ = on; }

signals:
    // 任何增删/清空/固定状态变化后发出（MainWindow 据此重建「最近打开」菜单）
    void changed();

private:
    void load();    // 新键优先；无新键且有旧键 → 一次性迁移
    void save();
    void trim();    // 硬上限裁剪（pin 优先保护）

    AppSettings* s_ = nullptr;
    bool  persistEnabled_ = true;
    QString lastJson_;              // 最近一次自写的存储文本（自写回环检测）
    QVector<RecentEntry> entries_;
};

} // namespace SketchCAD
