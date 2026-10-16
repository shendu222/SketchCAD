#pragma once
#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

// 全局应用设置：应用级「用户习惯」的唯一持久化出口。
//
// 边界（与随图存档的 SketchModel 状态严格分离）：
//   * AppSettings 只放「应用习惯」——窗口几何/面板布局、显示开关默认值、
//     自动保存参数、最近文件、默认目录、打印设备偏好、语言、用户信息；
//   * 图形内容（图元数据、图层、块表、捕捉模式等随图属性）一律不进注册表。
//
// 落盘位置（Windows）：注册表 HKEY_CURRENT_USER\Software\SketchCAD
// （QSettings NativeFormat + 空 applicationName ⇒ 仅组织名一层）。
// 测试可注入自定义 QSettings（如临时 ini）以隔离系统注册表。
//
// 通用键值层：
//   * 所有合法键 + 默认值 + 校验器收敛于 specTable()（唯一数据源），
//     导出/导入/重置/读取回填全部走同一张表，不存在"一处更新一处漏"；
//   * value() 读到缺键/非法值一律回落默认（损坏配置不崩溃、不扩散）；
//   * setValue() 校验 + 幂等 + 发 settingChanged（消费方即时生效，
//     「选项」对话框的取消回滚也复用同一信号通道）；
//   * 导出/导入 JSON（企业批量下发同一套默认配置）；window/ 组属本机
//     状态（几何/布局），不随导出下发。
namespace SketchCAD {

// 键名常量：与 specTable() 保持一致（编译期常量，供消费方书写）
namespace SettingKeys {
inline constexpr const char* kLanguage          = "general/language";
inline constexpr const char* kRecentFileCount   = "general/recentFileCount";
// 撤销栈深度上限（步）。属"应用习惯"（用户愿为历史付出多少内存），与图纸内容无关：
// **它本身不入撤销栈**——否则"改栈深"会成为一步可撤销操作，语义自我指涉。
inline constexpr const char* kUndoDepth         = "general/undoDepth";
// 撤销差量化开关（默认 true = 开，差量/变更集存储；false = 回落全量 Memento 模式对照）。
// 亦属"应用习惯"，不入撤销栈。
inline constexpr const char* kUndoDelta         = "general/undoDelta";
inline constexpr const char* kShowGrid          = "display/showGrid";
inline constexpr const char* kGridSnap          = "display/gridSnap";
inline constexpr const char* kGridSnapStep      = "display/gridSnapStep";
inline constexpr const char* kOrtho             = "display/ortho";
inline constexpr const char* kDynMode           = "display/dynMode";
inline constexpr const char* kShowLineWeight    = "display/showLineWeight";
inline constexpr const char* kGpuAcceleration   = "display/gpuAcceleration";
inline constexpr const char* kOffscreenFallback = "display/offscreenFallback";
inline constexpr const char* kHighDpiRounding   = "display/highDpiRoundingPolicy";
// 约束求解器特性开关（默认开）：关 → 回落到内置修复模型（LegacyConstraintSolver）；
// 开 → 使用当前活动求解实现（Phase 2 可切换为 Skt 适配器）。属"应用习惯"。
inline constexpr const char* kConstraintSolver  = "solver/constraintEnabled";
// 打开文档时自动刷新 FIELD 域（默认开）：对全模型含域 MText/Table 预热求值一次
// （求值只读不改几何、异常回落 ###）。属"应用习惯"。
inline constexpr const char* kFieldAutoUpdate   = "fields/autoUpdateOnOpen";
// 尺寸驱动主开关（需求 D-③）：默认开。开→改标注值联动实际几何；关→仅覆盖标注显示文本。属"应用习惯"。
inline constexpr const char* kDimensionDrive    = "dimension/driveOn";
inline constexpr const char* kDefaultSaveDir    = "files/defaultSaveDir";
inline constexpr const char* kDefaultTemplate   = "files/defaultTemplate";
inline constexpr const char* kLastDir           = "files/lastDir";
inline constexpr const char* kRecentList        = "files/recentList";
// 最近文件富条目（RecentFiles 持久化文本：JSON 数组 [{p,t,pin},...]，含打开时间与固定标记）。
// 取代 kRecentList（旧键保留只读：仅作为首次升级时的一次性迁移来源）。
inline constexpr const char* kRecentEntries     = "files/recentEntries";
// 命令行最近命令历史（QStringList，重启后预填 ↑↓ 历史，AutoCAD 习惯）。
inline constexpr const char* kRecentCommands    = "commandLine/recentCommands";
// 命令面板最近使用（QStringList，命令名 ≤10 条，重启后面板最近使用置顶）。
inline constexpr const char* kRecentPalette     = "commandLine/recentPalette";
// 空闲态右键行为：true = AutoCAD 约定（重复上一命令）；false = 既有上下文菜单。
inline constexpr const char* kRightClickIdleRepeat = "interaction/rightClickIdleRepeat";
// 隐私：匿名遥测总开关（默认 false = 关闭；首启动有一次性明示文案）。
// 诊断包导出与遥测无关——诊断包只在用户主动点击时生成本地文件，零网络外发。
inline constexpr const char* kTelemetryEnabled  = "privacy/telemetryEnabled";
// 隐私明示文案是否已展示过（首启动一次性）。
inline constexpr const char* kFirstRunNotice    = "privacy/firstRunNoticeShown";
inline constexpr const char* kAutosaveInterval  = "autosave/intervalMinutes";
inline constexpr const char* kBackupCount       = "autosave/backupCount";
inline constexpr const char* kLowDiskMb         = "system/lowDiskThresholdMb";
inline constexpr const char* kPrinterDevice     = "print/device";
inline constexpr const char* kPrintDefaultsJson = "print/defaultSettings";
inline constexpr const char* kUserName          = "user/name";
inline constexpr const char* kUserCompany       = "user/company";
inline constexpr const char* kWindowGeometry    = "window/geometry";
inline constexpr const char* kWindowState       = "window/state";
} // namespace SettingKeys

class AppSettings : public QObject {
    Q_OBJECT
public:
    // 键规格：键名 + 默认值 + 校验器（返回无效 QVariant 表示非法 → 回落默认）。
    struct Spec {
        const char* key;
            QVariant def;
        std::function<QVariant(const QVariant&)> validate;
    };

    // 全部已知键规格（唯一数据源；顺序即「选项」对话框分类的展开依据）
    static const QVector<Spec>& specTable();
    static QVariant defaultValue(const QString& key);          // 未知键返回无效 QVariant
    static bool isKnownKey(const QString& key);
    static QStringList allKeys();
    // 校验：合法返回规范化后的值；非法返回无效 QVariant（调用方回落默认）
    static QVariant validated(const QString& key, const QVariant& v);

    // 默认构造：使用系统原生存储（HKCU\Software\SketchCAD）。
    // 若传入 injected，则复用该 QSettings 实例（不接管所有权），便于无头测试。
    explicit AppSettings(QSettings* injected = nullptr, QObject* parent = nullptr);

    ~AppSettings() override;

    // 注入（或换用）底层 QSettings；own=false 表示不接管所有权。
    void setSettings(QSettings* s, bool own = false);

    // ---- 通用键值层 ----
    // 读：缺键/非法/存储损坏 → 一律回落默认值（绝不抛异常、绝不崩溃）。
    QVariant value(const QString& key) const;
    // 写：校验（非法静默忽略）+ 幂等（同值不重复写、不发信号）+ 立即 sync
    //（写失败可当场被 syncAndCheck/lastWriteFailed 捕获）+ 发 settingChanged。
    void setValue(const QString& key, const QVariant& v);
    // 底层是否显式存过该键（与 value() 的"回落默认"互补）
    bool contains(const QString& key) const;

    // ---- 导出/导入（企业批量下发同一套默认配置；JSON 文本）----
    // 窗口几何/布局（window/ 组）属本机状态，不参与导出。
    QByteArray exportToJsonData() const;
    bool exportToJson(const QString& path, QString* err = nullptr) const;
    // 导入：JSON 结构非法 → false（当前值保持不动）；仅接受已知键，未知键忽略
    //（前向兼容：新版配置文件在旧版程序上导入只落可识别的键，不报错不崩溃）。
    bool importFromJsonData(const QByteArray& data, QString* err = nullptr);
    bool importFromJson(const QString& path, QString* err = nullptr);

    // ---- 写入状态（"写入失败必须提示"的接缝）----
    // 立即 sync 并刷新失败状态；返回 true = 无错误。
    bool syncAndCheck();
    bool    lastWriteFailed() const { return writeFailed_; }
    QString lastError() const { return lastError_; }

    // ---- 重置 ----
    // 移除全部已知键（回落默认）+ 广播 settingsReset 与逐键 settingChanged。
    void resetToDefaults();

    // ---- 既有类型化访问器（保持向后兼容；走通用层）----
    // 0 表示关闭自动保存；合法取值 0/5/10/15/30，默认 10。
    int  autosaveIntervalMinutes() const;
    void setAutosaveIntervalMinutes(int minutes);
    // 便捷判断：间隔 > 0 即视为开启
    bool autosaveEnabled() const { return autosaveIntervalMinutes() > 0; }
    // 磁盘剩余告警阈值（MB），默认 100MB。
    int  lowDiskThresholdMb() const;
    void setLowDiskThresholdMb(int mb);

    // ---- 类型化便捷访问器（消费方高频键；全部走通用层，不含独立状态）----
    bool   showGrid() const;           void setShowGrid(bool b);
    bool   gridSnap() const;           void setGridSnap(bool b);
    double gridSnapStep() const;       void setGridSnapStep(double mm);   // 吸附/栅格步长 mm >0
    bool   orthoMode() const;          void setOrthoMode(bool b);
    bool   dynMode() const;            void setDynMode(bool b);
    bool   showLineWeight() const;     void setShowLineWeight(bool b);
    bool   dimensionDriveOn() const;    void setDimensionDrive(bool b);      // 尺寸驱动主开关（默认开）
    int    recentFileCount() const;    void setRecentFileCount(int n);    // 0..20，默认 10
    int    undoDepth() const;          void setUndoDepth(int n);           // 1..1000，默认 100
    bool   undoDelta() const;          void setUndoDelta(bool b);          // 默认 true（差量化开）
    int    backupCount() const;        void setBackupCount(int n);        // 0..9，默认 1
    QString language() const;          void setLanguage(const QString& l);// zh-CN / en
    QString defaultSaveDir() const;    void setDefaultSaveDir(const QString& d);
    QString defaultTemplate() const;   void setDefaultTemplate(const QString& p);
    QString lastDir() const;           void setLastDir(const QString& d);
    QStringList recentFiles() const;   void setRecentFiles(const QStringList& l);
    bool   gpuAcceleration() const;    void setGpuAcceleration(bool b);   // 预留：渲染路径消费
    bool   offscreenFallback() const;  void setOffscreenFallback(bool b); // 预留：渲染路径消费
    // 约束求解特性开关（默认开；关 → 回落修复模型）
    bool   constraintSolverEnabled() const; void setConstraintSolverEnabled(bool b);
    // FIELD 域打开文档自动刷新（默认开）
    bool   fieldAutoUpdateOnOpen() const;   void setFieldAutoUpdateOnOpen(bool b);
    // 隐私：遥测开关（默认 false）与首启动明示标记
    bool   telemetryEnabled() const;   void setTelemetryEnabled(bool b);
    bool   firstRunNoticeShown() const; void setFirstRunNoticeShown(bool b);
    QString highDpiRoundingPolicy() const; void setHighDpiRoundingPolicy(const QString& p);
    QString printerName() const;       void setPrinterName(const QString& name);
    // 打印页面设置快照（PrintSettings::toJson 的 JSON 文本；空 = 未设置）
    QString printDefaultsJson() const; void setPrintDefaultsJson(const QString& json);
    QString userName() const;          void setUserName(const QString& n);
    QString userCompany() const;       void setUserCompany(const QString& c);
    // 窗口几何/面板布局：QByteArray 以 base64 字符串落注册表（类型稳定往返）
    QByteArray windowGeometry() const; void setWindowGeometry(const QByteArray& g);
    QByteArray windowState() const;    void setWindowState(const QByteArray& st);

signals:
    // 兼容既有订阅（AutoSaveOptionsDialog 流程）
    void autosaveIntervalChanged(int minutes);
    void lowDiskThresholdChanged(int mb);
    // 通用变更：key + 规范化后的新值（消费方按 key 分派；取消回滚复用同一通道）
    void settingChanged(const QString& key, const QVariant& value);
    // 重置为默认值（逐键 settingChanged 已同时广播，此信号供整体刷新）
    void settingsReset();

private:
    void recordStatus();          // 同步后刷新 writeFailed_/lastError_
    void emitLegacySignals(const QString& key, const QVariant& v);

    QSettings* s_ = nullptr;
    bool owns_ = true;
    bool writeFailed_ = false;
    QString lastError_;
};

} // namespace SketchCAD
