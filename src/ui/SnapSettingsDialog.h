#pragma once
#include <QDialog>
#include <vector>
#include "core/Snap.h"

class QCheckBox;        // Qt 全局类，前向声明须置于命名空间外
class QPushButton;      // 同上
class QDoubleSpinBox;   // 同上

namespace SketchCAD {

class SketchModel;     // 本项目类，前向声明须置于同名命名空间内

// 捕捉设置面板（OSNAP + 追踪）：提供 14 种捕捉模式的独立启用/禁用控制，
// 以及追踪配置（极轴 POLAR / 对象捕捉追踪 OTRACK / 增量角 / 相对上一段）。
// 设定结果经 SketchModel::setSnapConfig / setTrackConfig 落地为「全局持久配置」并随图存档。
// 与绘制命令期内的 E/T/M/C… 临时切换、Shift+右键临时覆盖互为补充：
//   面板改的是「基线默认值」，命令期切换/覆盖只作用于当前命令、不写回此处。
// 14 个复选项由 snap::snapFieldDefs() 统一驱动，与快捷键、覆盖菜单共用同一数据源。
class SnapSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SnapSettingsDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void accept();                  // 落地到模型（setSnapConfig + setTrackConfig）

private:
    snap::SnapConfig  collect() const;       // 由复选项收集当前捕捉配置
    snap::TrackConfig collectTrack() const;  // 由追踪控件收集当前追踪配置

    SketchModel*                model_;
    std::vector<QCheckBox*>     checks_;     // 与 snap::snapFieldDefs() 一一对应
    // ---- 追踪控件 ----
    QCheckBox*       trackPolar_    = nullptr;   // 极轴追踪开关
    QCheckBox*       trackOtrack_   = nullptr;   // 对象捕捉追踪开关
    QCheckBox*       trackRelative_ = nullptr;   // 极轴角相对上一段方向
    QDoubleSpinBox*  trackInc_      = nullptr;   // 极轴增量角（度）
    QPushButton*     tkClearBtn_    = nullptr;   // 清除全部临时追踪点
};

} // namespace SketchCAD
