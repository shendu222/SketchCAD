#include "AppSettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QFile>
#include <QDir>
#include <QFileInfo>

namespace SketchCAD {

using SettingKeys::kLanguage;
using SettingKeys::kRecentFileCount;
using SettingKeys::kUndoDepth;
using SettingKeys::kUndoDelta;
using SettingKeys::kFieldAutoUpdate;
using SettingKeys::kShowGrid;
using SettingKeys::kGridSnap;
using SettingKeys::kGridSnapStep;
using SettingKeys::kOrtho;
using SettingKeys::kDynMode;
using SettingKeys::kShowLineWeight;
using SettingKeys::kGpuAcceleration;
using SettingKeys::kOffscreenFallback;
using SettingKeys::kConstraintSolver;
using SettingKeys::kHighDpiRounding;
using SettingKeys::kDefaultSaveDir;
using SettingKeys::kDefaultTemplate;
using SettingKeys::kLastDir;
using SettingKeys::kRecentList;
using SettingKeys::kAutosaveInterval;
using SettingKeys::kBackupCount;
using SettingKeys::kLowDiskMb;
using SettingKeys::kPrinterDevice;
using SettingKeys::kPrintDefaultsJson;
using SettingKeys::kRecentEntries;
using SettingKeys::kRecentCommands;
using SettingKeys::kRecentPalette;
using SettingKeys::kRightClickIdleRepeat;
using SettingKeys::kTelemetryEnabled;
using SettingKeys::kFirstRunNotice;
using SettingKeys::kUserName;
using SettingKeys::kUserCompany;
using SettingKeys::kWindowGeometry;
using SettingKeys::kWindowState;

namespace {

// ---- 类型收窄辅助：QSettings（尤其注册表后端）把数值/布尔以字符串存取，
//      校验必须「按值」而非「按 QVariant 类型」判定，否则注册表往返会误判非法。
//      Qt 6.10 起 QVariant::type() 弃用，统一用 typeId() + QMetaType 判别。 ----

namespace {
bool isType(const QVariant& v, QMetaType::Type t) { return v.typeId() == t; }
}

QVariant boolVal(const QVariant& v) {
    if (isType(v, QMetaType::Bool)) return v;
    if (isType(v, QMetaType::Int) || isType(v, QMetaType::LongLong))
        return (v.toLongLong() == 0 || v.toLongLong() == 1) ? QVariant(v.toLongLong() != 0)
                                                            : QVariant();
    const QString s = v.toString();
    if (s == QLatin1String("true"))  return QVariant(true);
    if (s == QLatin1String("false")) return QVariant(false);
    return QVariant();   // 其它一律视为非法 → 回落默认
}

QVariant intRangeVal(const QVariant& v, int lo, int hi) {
    bool ok = false;
    int n = 0;
    switch (v.typeId()) {
    case QMetaType::Int: case QMetaType::LongLong: case QMetaType::UInt:
        n = v.toInt(); ok = true; break;
    case QMetaType::Double:
        n = v.toInt(&ok); break;                 // 整数键收到浮点：可无损取整才接受
    case QMetaType::QString:
        n = v.toString().toInt(&ok); break;
    default:
        return QVariant();
    }
    if (!ok || n < lo || n > hi) return QVariant();
    return QVariant(n);
}

QVariant doubleRangeVal(const QVariant& v, double lo, double hi) {
    bool ok = false;
    double d = 0;
    switch (v.typeId()) {
    case QMetaType::Double: case QMetaType::Int: case QMetaType::LongLong: case QMetaType::UInt:
        d = v.toDouble(); ok = true; break;
    case QMetaType::QString:
        d = v.toString().toDouble(&ok); break;
    default:
        return QVariant();
    }
    if (!ok || d < lo || d > hi) return QVariant();
    return QVariant(d);
}

QVariant stringVal(const QVariant& v) {
    if (isType(v, QMetaType::QString)) {
        const QString s = v.toString();
        // 防御性截断：异常长的脏数据不进注册表
        return s.size() > 4096 ? QVariant() : QVariant(s);
    }
    return QVariant();
}

QVariant stringListVal(const QVariant& v) {
    if (isType(v, QMetaType::QStringList)) return v.toStringList();
    if (isType(v, QMetaType::QString)) {         // 单串宽容为单元素列表
        const QString s = v.toString();
        return s.isEmpty() ? QVariant(QStringList()) : QVariant(QStringList{ s });
    }
    return QVariant();
}

QVariant stringWhitelistVal(const QVariant& v, std::initializer_list<QLatin1String> allowed) {
    if (!isType(v, QMetaType::QString)) return QVariant();
    const QString s = v.toString();
    for (const QLatin1String& a : allowed)
        if (s == QLatin1String(a)) return QVariant(s);
    return QVariant();
}

// print/defaultSettings：空串=未设置；非空必须能解析为 JSON 对象
QVariant printJsonVal(const QVariant& v) {
    if (!isType(v, QMetaType::QString)) return QVariant();
    const QString s = v.toString();
    if (s.isEmpty()) return QVariant(s);
    const QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
    if (doc.isNull() || !doc.isObject()) return QVariant();
    return QVariant(s);
}

// files/recentEntries：空串=未设置；非空必须能解析为 JSON 数组（RecentFiles 持久化文本）
QVariant recentEntriesVal(const QVariant& v) {
    if (!isType(v, QMetaType::QString)) return QVariant();
    const QString s = v.toString();
    if (s.isEmpty()) return QVariant(s);
    const QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
    if (doc.isNull() || !doc.isArray()) return QVariant();   // 损坏数据一律回落默认（空串）
    return QVariant(s);
}

// window/geometry|state：base64 文本（QByteArray 经注册表往返类型不稳，显式编码）
QVariant base64Val(const QVariant& v) {
    if (!isType(v, QMetaType::QString)) return QVariant();
    const QString s = v.toString();
    if (s.isEmpty()) return QVariant(s);
    const QByteArray raw = QByteArray::fromBase64(s.toLatin1());
    if (raw.isEmpty() && !s.isEmpty()) return QVariant();   // 解不开的串视为损坏
    return QVariant(s);
}

} // namespace

// ---- 键规格表（唯一数据源）----

const QVector<AppSettings::Spec>& AppSettings::specTable() {
    static const QVector<Spec> table = {
        // ---- general ----
        // 界面语言："" = 跟随系统（QLocale 中文系统→源语言 zh-CN，否则 en）；
        // zh-CN 为源语言不装翻译器，en 装 sketchcad_en.qm。缺键默认跟随系统，
        // 对既有中文用户零感知（系统即中文）。
        { kLanguage, QStringLiteral(""),
          [](const QVariant& v) { return stringWhitelistVal(v, { QLatin1String(""), QLatin1String("zh-CN"), QLatin1String("en") }); } },
        { kRecentFileCount, 10, [](const QVariant& v) { return intRangeVal(v, 0, 20); } },
        // 撤销栈深度（步）：上下限与 SketchModel 的 kUndoDepthMin/Max/Default 保持一致。
        // 下限为 1（0 步=关掉撤销，会让 Ctrl+Z 变成"什么都没发生"，比留 1 步更难理解）。
        { kUndoDepth, 100, [](const QVariant& v) { return intRangeVal(v, 1, 1000); } },
        // 撤销差量化开关：默认开；关则回落全量 Memento 模式（性能/行为对照用）。
        { kUndoDelta, true, boolVal },
        // ---- display ----
        { kShowGrid, true,  boolVal },
        { kGridSnap, false, boolVal },
        { kGridSnapStep, 1.0, [](const QVariant& v) { return doubleRangeVal(v, 1e-3, 1e6); } },
        { kOrtho, false, boolVal },
        { kDynMode, true, boolVal },
        { kShowLineWeight, false, boolVal },
        { kGpuAcceleration, true, boolVal },
        { kOffscreenFallback, false, boolVal },
        // ---- solver（约束求解特性开关，默认开）----
        { kConstraintSolver, true, boolVal },
        { kHighDpiRounding, QStringLiteral("Round"),
          [](const QVariant& v) {
              return stringWhitelistVal(v, { QLatin1String("Round"), QLatin1String("Floor"),
                                             QLatin1String("Ceil"), QLatin1String("RoundPreferFloor"),
                                             QLatin1String("PassThrough") });
          } },
        // ---- files ----
        { kDefaultSaveDir,  QString(), stringVal },
        { kDefaultTemplate, QString(), stringVal },
        { kLastDir,         QString(), stringVal },
        { kRecentList, QStringList(), stringListVal },
        { kRecentEntries, QString(), recentEntriesVal },
        // ---- commandLine ----
        // 命令行最近命令（重启后预填 ↑↓ 历史）；上限在写入方裁剪（≤50 条），
        // 校验器只保证类型与元素非空。
        { kRecentCommands, QStringList(), stringListVal },
        // 命令面板最近使用（命令名列表，≤10 条由写入方裁剪）。
        { kRecentPalette, QStringList(), stringListVal },
        // ---- interaction ----
        // 空闲态（select 且无进行中手势）右键行为：true = AutoCAD 约定（重复上一命令），
        // false = 既有上下文菜单。默认取 AutoCAD 值（用户任务约定）。
        { kRightClickIdleRepeat, true, boolVal },
        // ---- fields（FIELD 域自动刷新；默认开）----
        { kFieldAutoUpdate, true, boolVal },
        // ---- privacy ----
        // 遥测默认关闭（用户任务硬边界：默认不得有任何网络外发）；
        // 诊断包导出不依赖此键——只在用户主动点击时生成本地 ZIP。
        { kTelemetryEnabled, false, boolVal },
        { kFirstRunNotice,   false, boolVal },
        // ---- autosave（沿用既有键名，旧版注册表数据无缝继承）----
        { kAutosaveInterval, 10,
          [](const QVariant& v) {
              const QVariant n = intRangeVal(v, 0, 30);
              if (!n.isValid()) return QVariant();
              const int m = n.toInt();
              // 合法集合：0(关)/5/10/15/30；其它回落默认
              return (m == 0 || m == 5 || m == 10 || m == 15 || m == 30) ? QVariant(m) : QVariant();
          } },
        { kBackupCount, 1, [](const QVariant& v) { return intRangeVal(v, 0, 9); } },
        { kLowDiskMb, 100, [](const QVariant& v) { return intRangeVal(v, 1, 1000000); } },
        // ---- print ----
        { kPrinterDevice, QString(), stringVal },
        { kPrintDefaultsJson, QString(), printJsonVal },
        // ---- user ----
        { kUserName, QString(), stringVal },
        { kUserCompany, QString(), stringVal },
        // ---- window（本机状态；不参与导出）----
        { kWindowGeometry, QString(), base64Val },
        { kWindowState, QString(), base64Val },
    };
    return table;
}

QVariant AppSettings::defaultValue(const QString& key) {
    for (const Spec& sp : specTable())
        if (key == QLatin1String(sp.key)) return sp.def;
    return QVariant();
}

bool AppSettings::isKnownKey(const QString& key) {
    return defaultValue(key).isValid();
}

QStringList AppSettings::allKeys() {
    QStringList out;
    for (const Spec& sp : specTable()) out << QLatin1String(sp.key);
    return out;
}

QVariant AppSettings::validated(const QString& key, const QVariant& v) {
    for (const Spec& sp : specTable())
        if (key == QLatin1String(sp.key)) return sp.validate(v);
    return QVariant();   // 未知键一律非法
}

// ---- 生命周期 ----

AppSettings::AppSettings(QSettings* injected, QObject* parent)
    : QObject(parent) {
    if (injected) {
        s_ = injected;
        owns_ = false;
    } else {
        // NativeFormat + 空 applicationName ⇒ Windows 上落在 HKCU\Software\SketchCAD
        s_ = new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                           QStringLiteral("SketchCAD"), QString(), this);
        owns_ = true;
    }
}

AppSettings::~AppSettings() {
    // owns_ 为 true 时 s_ 以 this 为 parent，由 Qt 自动析构；此处不重复 delete。
    if (owns_) s_ = nullptr;
}

void AppSettings::setSettings(QSettings* s, bool own) {
    if (s_ == s) return;
    if (owns_ && s_) s_->deleteLater();
    s_ = s;
    owns_ = own;
    writeFailed_ = false;
    lastError_.clear();
    if (!s_) {
        s_ = new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                           QStringLiteral("SketchCAD"), QString(), this);
        owns_ = true;
    }
}

// ---- 通用键值层 ----

QVariant AppSettings::value(const QString& key) const {
    const QVariant def = defaultValue(key);
    if (!def.isValid()) return def;                 // 未知键：无效 QVariant
    if (!s_) return def;
    const QVariant raw = s_->value(key, def);
    const QVariant v = validated(key, raw);
    return v.isValid() ? v : def;                   // 非法/损坏 → 回落默认
}

void AppSettings::setValue(const QString& key, const QVariant& v) {
    const QVariant valid = validated(key, v);
    if (!valid.isValid()) return;                   // 非法值静默忽略
    if (value(key) == valid) return;                // 幂等（同值不写不发信号）
    if (s_) {
        s_->setValue(key, valid);
        s_->sync();                                 // 立即落盘：失败当场可检（写失败必须提示）
        recordStatus();
    }
    emit settingChanged(key, valid);
    emitLegacySignals(key, valid);
}

bool AppSettings::contains(const QString& key) const {
    return s_ && s_->contains(key);
}

void AppSettings::recordStatus() {
    if (!s_) return;
    const QSettings::Status st = s_->status();
    writeFailed_ = (st != QSettings::NoError);
    if (writeFailed_) {
        switch (st) {
        case QSettings::AccessError:
            lastError_ = QStringLiteral("无法写入配置（权限不足或路径不可用）"); break;
        case QSettings::FormatError:
            lastError_ = QStringLiteral("配置文件格式错误，本次改动未能保存"); break;
        default:
            lastError_ = QStringLiteral("配置写入发生未知错误"); break;
        }
    } else {
        lastError_.clear();
    }
}

bool AppSettings::syncAndCheck() {
    if (s_) {
        s_->sync();
        recordStatus();
    }
    return !writeFailed_;
}

// ---- 导出 / 导入 ----

QByteArray AppSettings::exportToJsonData() const {
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("SketchCAD-Settings"));
    root.insert(QStringLiteral("version"), 1);
    QJsonObject vals;
    for (const Spec& sp : specTable()) {
        const QString k = QLatin1String(sp.key);
        // 窗口几何/布局属本机状态，不随导出下发
        if (k.startsWith(QLatin1String("window/"))) continue;
        vals.insert(k, QJsonValue::fromVariant(value(k)));
    }
    root.insert(QStringLiteral("values"), vals);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool AppSettings::exportToJson(const QString& path, QString* err) const {
    const QByteArray data = exportToJsonData();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("无法写入导出文件：%1").arg(path);
        return false;
    }
    if (f.write(data) != data.size()) {
        if (err) *err = QStringLiteral("导出文件写入不完整：%1").arg(path);
        return false;
    }
    if (err) *err = QString();
    return true;
}

bool AppSettings::importFromJsonData(const QByteArray& data, QString* err) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (doc.isNull() || !doc.isObject()) {
        if (err) *err = QStringLiteral("配置文件不是合法 JSON（偏移 %1）").arg(pe.offset);
        return false;   // 当前值保持不动，不崩溃
    }
    const QJsonObject root = doc.object();
    const QJsonObject vals = root.value(QStringLiteral("values")).toObject();
    if (vals.isEmpty() && !root.contains(QStringLiteral("values"))) {
        if (err) *err = QStringLiteral("配置文件缺少 values 字段");
        return false;
    }
    for (auto it = vals.begin(); it != vals.end(); ++it) {
        if (!isKnownKey(it.key())) continue;        // 未知键忽略（前向兼容）
        const QVariant valid = validated(it.key(), it.value().toVariant());
        if (!valid.isValid()) continue;             // 值非法：跳过该键，不中断
        setValue(it.key(), valid);                  // 逐键生效（信号 → 消费方即时应用）
    }
    if (err) *err = QString();
    return true;
}

bool AppSettings::importFromJson(const QString& path, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取配置文件：%1").arg(path);
        return false;
    }
    return importFromJsonData(f.readAll(), err);
}

// ---- 重置 ----

void AppSettings::resetToDefaults() {
    if (s_) {
        for (const Spec& sp : specTable())
            s_->remove(QLatin1String(sp.key));
        s_->sync();
        recordStatus();
    }
    // 逐键广播默认值（消费方即时回默认态），再发整体重置信号
    for (const Spec& sp : specTable()) {
        const QString k = QLatin1String(sp.key);
        emit settingChanged(k, sp.def);
        emitLegacySignals(k, sp.def);
    }
    emit settingsReset();
}

// ---- 既有类型化访问器（走通用层，保持向后兼容）----

int AppSettings::autosaveIntervalMinutes() const {
    return value(QLatin1String(kAutosaveInterval)).toInt();
}

void AppSettings::setAutosaveIntervalMinutes(int minutes) {
    if (minutes != 0 && minutes != 5 && minutes != 10 && minutes != 15 && minutes != 30)
        return;                                  // 非法值静默忽略
    setValue(QLatin1String(kAutosaveInterval), minutes);
}

int AppSettings::lowDiskThresholdMb() const {
    return value(QLatin1String(kLowDiskMb)).toInt();
}

void AppSettings::setLowDiskThresholdMb(int mb) {
    if (mb <= 0) return;                         // 非法值静默忽略
    setValue(QLatin1String(kLowDiskMb), mb);
}

// ---- 类型化便捷访问器 ----

bool AppSettings::showGrid() const { return value(QLatin1String(kShowGrid)).toBool(); }
void AppSettings::setShowGrid(bool b) { setValue(QLatin1String(kShowGrid), b); }

bool AppSettings::gridSnap() const { return value(QLatin1String(kGridSnap)).toBool(); }
void AppSettings::setGridSnap(bool b) { setValue(QLatin1String(kGridSnap), b); }

double AppSettings::gridSnapStep() const { return value(QLatin1String(kGridSnapStep)).toDouble(); }
void AppSettings::setGridSnapStep(double mm) { setValue(QLatin1String(kGridSnapStep), mm); }

bool AppSettings::orthoMode() const { return value(QLatin1String(kOrtho)).toBool(); }
void AppSettings::setOrthoMode(bool b) { setValue(QLatin1String(kOrtho), b); }

bool AppSettings::dynMode() const { return value(QLatin1String(kDynMode)).toBool(); }
void AppSettings::setDynMode(bool b) { setValue(QLatin1String(kDynMode), b); }

bool AppSettings::showLineWeight() const { return value(QLatin1String(kShowLineWeight)).toBool(); }
void AppSettings::setShowLineWeight(bool b) { setValue(QLatin1String(kShowLineWeight), b); }

int AppSettings::recentFileCount() const { return value(QLatin1String(kRecentFileCount)).toInt(); }
void AppSettings::setRecentFileCount(int n) { setValue(QLatin1String(kRecentFileCount), n); }

int AppSettings::undoDepth() const { return value(QLatin1String(kUndoDepth)).toInt(); }
void AppSettings::setUndoDepth(int n) { setValue(QLatin1String(kUndoDepth), n); }
bool AppSettings::undoDelta() const { return value(QLatin1String(kUndoDelta)).toBool(); }
void AppSettings::setUndoDelta(bool b) { setValue(QLatin1String(kUndoDelta), b); }

int AppSettings::backupCount() const { return value(QLatin1String(kBackupCount)).toInt(); }
void AppSettings::setBackupCount(int n) { setValue(QLatin1String(kBackupCount), n); }

QString AppSettings::language() const { return value(QLatin1String(kLanguage)).toString(); }
void AppSettings::setLanguage(const QString& l) { setValue(QLatin1String(kLanguage), l); }

QString AppSettings::defaultSaveDir() const { return value(QLatin1String(kDefaultSaveDir)).toString(); }
void AppSettings::setDefaultSaveDir(const QString& d) { setValue(QLatin1String(kDefaultSaveDir), d); }

QString AppSettings::defaultTemplate() const { return value(QLatin1String(kDefaultTemplate)).toString(); }
void AppSettings::setDefaultTemplate(const QString& p) { setValue(QLatin1String(kDefaultTemplate), p); }

QString AppSettings::lastDir() const { return value(QLatin1String(kLastDir)).toString(); }
void AppSettings::setLastDir(const QString& d) { setValue(QLatin1String(kLastDir), d); }

QStringList AppSettings::recentFiles() const { return value(QLatin1String(kRecentList)).toStringList(); }
void AppSettings::setRecentFiles(const QStringList& l) { setValue(QLatin1String(kRecentList), l); }

bool AppSettings::gpuAcceleration() const { return value(QLatin1String(kGpuAcceleration)).toBool(); }
void AppSettings::setGpuAcceleration(bool b) { setValue(QLatin1String(kGpuAcceleration), b); }

bool AppSettings::offscreenFallback() const { return value(QLatin1String(kOffscreenFallback)).toBool(); }
void AppSettings::setOffscreenFallback(bool b) { setValue(QLatin1String(kOffscreenFallback), b); }

bool AppSettings::constraintSolverEnabled() const { return value(QLatin1String(kConstraintSolver)).toBool(); }
void AppSettings::setConstraintSolverEnabled(bool b) { setValue(QLatin1String(kConstraintSolver), b); }

bool AppSettings::fieldAutoUpdateOnOpen() const { return value(QLatin1String(kFieldAutoUpdate)).toBool(); }
void AppSettings::setFieldAutoUpdateOnOpen(bool b) { setValue(QLatin1String(kFieldAutoUpdate), b); }

bool AppSettings::telemetryEnabled() const { return value(QLatin1String(kTelemetryEnabled)).toBool(); }
void AppSettings::setTelemetryEnabled(bool b) { setValue(QLatin1String(kTelemetryEnabled), b); }
bool AppSettings::firstRunNoticeShown() const { return value(QLatin1String(kFirstRunNotice)).toBool(); }
void AppSettings::setFirstRunNoticeShown(bool b) { setValue(QLatin1String(kFirstRunNotice), b); }

QString AppSettings::highDpiRoundingPolicy() const { return value(QLatin1String(kHighDpiRounding)).toString(); }
void AppSettings::setHighDpiRoundingPolicy(const QString& p) { setValue(QLatin1String(kHighDpiRounding), p); }

QString AppSettings::printerName() const { return value(QLatin1String(kPrinterDevice)).toString(); }
void AppSettings::setPrinterName(const QString& name) { setValue(QLatin1String(kPrinterDevice), name); }

QString AppSettings::printDefaultsJson() const { return value(QLatin1String(kPrintDefaultsJson)).toString(); }
void AppSettings::setPrintDefaultsJson(const QString& json) { setValue(QLatin1String(kPrintDefaultsJson), json); }

QString AppSettings::userName() const { return value(QLatin1String(kUserName)).toString(); }
void AppSettings::setUserName(const QString& n) { setValue(QLatin1String(kUserName), n); }

QString AppSettings::userCompany() const { return value(QLatin1String(kUserCompany)).toString(); }
void AppSettings::setUserCompany(const QString& c) { setValue(QLatin1String(kUserCompany), c); }

QByteArray AppSettings::windowGeometry() const {
    return QByteArray::fromBase64(value(QLatin1String(kWindowGeometry)).toString().toLatin1());
}
void AppSettings::setWindowGeometry(const QByteArray& g) {
    setValue(QLatin1String(kWindowGeometry), QString::fromLatin1(g.toBase64()));
}

QByteArray AppSettings::windowState() const {
    return QByteArray::fromBase64(value(QLatin1String(kWindowState)).toString().toLatin1());
}
void AppSettings::setWindowState(const QByteArray& st) {
    setValue(QLatin1String(kWindowState), QString::fromLatin1(st.toBase64()));
}

// ---- 内部 ----

void AppSettings::emitLegacySignals(const QString& key, const QVariant& v) {
    if (key == QLatin1String(kAutosaveInterval)) emit autosaveIntervalChanged(v.toInt());
    else if (key == QLatin1String(kLowDiskMb)) emit lowDiskThresholdChanged(v.toInt());
}

} // namespace SketchCAD
