#include "core/RecentFiles.h"
#include "core/AppSettings.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace SketchCAD {

namespace {
// JSON 字段名（紧凑：路径/时间毫秒/固定）
constexpr const char* kFieldPath = "p";
constexpr const char* kFieldTime = "t";
constexpr const char* kFieldPin  = "pin";

QJsonObject entryToJson(const RecentFiles::RecentEntry& e) {
    QJsonObject o;
    o.insert(QLatin1String(kFieldPath), e.path);
    o.insert(QLatin1String(kFieldTime),
             double(e.openedAt.isValid() ? e.openedAt.toMSecsSinceEpoch() : 0));
    o.insert(QLatin1String(kFieldPin), e.pinned);
    return o;
}

// 单条解析：path 缺失/为空 → 无效条目（跳过，不中断整表）
bool entryFromJson(const QJsonObject& o, RecentFiles::RecentEntry& out) {
    out = RecentFiles::RecentEntry{};
    out.path = o.value(QLatin1String(kFieldPath)).toString();
    if (out.path.isEmpty()) return false;
    const double t = o.value(QLatin1String(kFieldTime)).toDouble(0.0);
    out.openedAt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(t));
    if (!out.openedAt.isValid()) out.openedAt = QDateTime::currentDateTime();
    out.pinned = o.value(QLatin1String(kFieldPin)).toBool();
    return true;
}
} // namespace

RecentFiles::RecentFiles(AppSettings* settings, QObject* parent)
    : QObject(parent), s_(settings) {
    load();
}

void RecentFiles::load() {
    entries_.clear();
    if (!s_) return;

    const QString raw = s_->value(QLatin1String(SettingKeys::kRecentEntries)).toString();
    if (!raw.isEmpty()) {
        // 新键存在即以它为准（即使损坏也回落空表——校验器已拦截大多数脏数据，
        // 绕过校验器直接写盘的极端情形在这里兜底，绝不崩溃）
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const auto& v : arr) {
                if (!v.isObject()) continue;
                RecentEntry e;
                if (entryFromJson(v.toObject(), e)) entries_.push_back(e);
            }
        }
        lastJson_ = raw;
        return;
    }

    // 旧版迁移：files/recentList（QStringList 路径表，顺序即新→旧）→ 新键。
    // 打开时间取迁移时刻（旧数据无时间信息）；一次性写回新键，旧键保留（可降级）。
    const QStringList legacy =
        s_->value(QLatin1String(SettingKeys::kRecentList)).toStringList();
    for (const QString& p : legacy) {
        if (p.isEmpty()) continue;
        RecentEntry e;
        e.path = p;
        e.openedAt = QDateTime::currentDateTime();
        e.pinned = false;
        entries_.push_back(e);
    }
    if (!entries_.isEmpty()) save();
}

void RecentFiles::save() {
    if (!s_ || !persistEnabled_) return;
    QJsonArray arr;
    for (const auto& e : entries_) arr.append(entryToJson(e));
    lastJson_ = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    // setValue 自带校验 + 幂等 + sync；settingChanged 由 MainWindow 分派回
    // reloadFromSettings（文本与 lastJson_ 相同 → 无操作，不成环）。
    s_->setValue(QLatin1String(SettingKeys::kRecentEntries), lastJson_);
}

void RecentFiles::reloadFromSettings() {
    if (!s_) return;
    const QString raw = s_->value(QLatin1String(SettingKeys::kRecentEntries)).toString();
    if (raw == lastJson_) return;      // 自己刚写的（或语义未变）→ 忽略
    load();
    emit changed();
}

bool RecentFiles::contains(const QString& path) const {
    for (const auto& e : entries_)
        if (e.path == path) return true;
    return false;
}

bool RecentFiles::isPinned(const QString& path) const {
    for (const auto& e : entries_)
        if (e.path == path) return e.pinned;
    return false;
}

bool RecentFiles::isMissing(int index) const {
    if (index < 0 || index >= entries_.size()) return false;
    return !QFile::exists(entries_[index].path);
}

void RecentFiles::add(const QString& path) {
    if (path.isEmpty()) return;
    // 去重置顶：重复打开同一文件 → 移到最前并刷新时间；固定标记保留
    //（用户显式固定的条目不因「再次打开」丢掉固定）
    bool wasPinned = false;
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->path == path) { wasPinned = it->pinned; entries_.erase(it); break; }
    }
    RecentEntry e;
    e.path = path;
    e.openedAt = QDateTime::currentDateTime();
    e.pinned = wasPinned;
    entries_.prepend(e);
    trim();
    save();
    emit changed();
}

void RecentFiles::trim() {
    while (entries_.size() > kMaxEntries) {
        // 从最旧（尾部）起找第一个未固定项挤掉；**下标 0（本次刚加入的条目）不参与**
        // ——否则全 pin 场景下"最旧的未固定项"会命中刚加入的新条目自己，
        //    变成"最新被挤出"；全固定时退化为挤最旧（硬上限优先）。
        int victim = -1;
        for (int i = entries_.size() - 1; i >= 1; --i) {
            if (!entries_[i].pinned) { victim = i; break; }
        }
        if (victim < 0) victim = entries_.size() - 1;
        entries_.remove(victim);
    }
}

void RecentFiles::remove(const QString& path) {
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_[i].path == path) { removeAt(i); return; }
    }
}

void RecentFiles::removeAt(int index) {
    if (index < 0 || index >= entries_.size()) return;
    entries_.remove(index);
    save();
    emit changed();
}

void RecentFiles::setPinned(const QString& path, bool on) {
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_[i].path == path) {
            if (entries_[i].pinned == on) return;   // 幂等：状态不变不写盘不发信号
            entries_[i].pinned = on;
            save();
            emit changed();
            return;
        }
    }
}

void RecentFiles::clear() {
    if (entries_.isEmpty()) return;
    entries_.clear();
    save();
    emit changed();
}

} // namespace SketchCAD
