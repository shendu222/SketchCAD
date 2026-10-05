#pragma once
#include <QStatusBar>
#include <QDateTime>
#include "core/Units.h"
class QLabel;
class QToolButton;

namespace SketchCAD {
// 底部状态栏：命令名 / 选项提示 / X·Y·Z 坐标 / 坐标系 / 图元数 / 比例
//                + 正交·极轴·捕捉·追踪·线宽 五个可点击开关（对应 AutoCAD 状态栏按钮）
//                + 上次自动保存时间标签（点击可手动触发一次自动保存）
// 坐标按缓存 UnitsMeta 格式化（换算到当前单位 + 精度 + 短后缀），
// 单位变更时 MainWindow 连接 unitsChanged → setUnitsMeta 刷新缓存。
//
// 开关组采用「信号解耦」：按钮只发 toggleRequested(key, on)，由 MainWindow
// 回写模型单一状态源（setOrthoMode/setGridSnap/setTrackConfig/setShowLineWeight）；
// 模型信号再经 setToggleState 回写按钮勾选态（三入口同步，与 F8/F9/F10/F11 一致）。
class StatusBar : public QStatusBar {
    Q_OBJECT
public:
    // 可点击开关的键（顺序即排布顺序）
    enum class Toggle { Ortho = 0, Polar, Snap, Track, Lwt };
    explicit StatusBar(QWidget* parent = nullptr);
    void setCommand(const QString& s);
    void setOptions(const QStringList& opts);  // 选项提示区：[C/闭合 U/放弃]，空列表则清空
    void setCoord(double x, double y, double z);
    void setPlaneInfo(const QString& s);
    void setUcsInfo(const QString& s);         // UCS 指示器：如 "WCS" / "⌖ MYUCS 45°"
    void setCount(int n);
    void setScale(double s);
    void setDrawScale(const QString& label);   // 绘图比例标签，如 "1:1" / "1:2"
    void setLoadStats(int elements, int items, double openMs);   // P0-04：数据面板「场景项 M / 打开 Xms」
    void setUnitsMeta(const UnitsMeta& u);     // 缓存单位口径（坐标显示实时跟随）
    void setToggleState(Toggle key, bool on);  // 回写按钮勾选态（不回发 toggleRequested）
    // 上次自动保存时间；无效值 → 显示「自动保存:—」。点击标签手动触发一次。
    void setLastAutosave(const QDateTime& t);
    // 自动保存是否已关闭（间隔=0）→ 标签显示「自动保存:关」
    void setAutosaveEnabled(bool on);
signals:
    void toggleRequested(Toggle key, bool on); // 用户点击开关 → MainWindow 回写模型
    void manualSaveRequested();                // 用户点击「上次自动保存」标签
private:
    QLabel *lCmd, *lOpt, *lCoord, *lPlane, *lUcs, *lCount, *lScale, *lStats;
    QToolButton* btnSave_ = nullptr;            // 上次自动保存时间（点击手动触发）
    QToolButton* btns_[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    UnitsMeta units_;                          // 坐标显示口径（默认 mm/2 位）
    bool autosaveOn_ = true;                   // 间隔=0 时为 false
};
} // namespace SketchCAD
