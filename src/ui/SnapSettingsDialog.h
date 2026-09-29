#pragma once
#include <QDialog>
#include <vector>
#include "core/Snap.h"

class QCheckBox;       // Qt 全局类，前向声明须置于命名空间外
class QPushButton;     // 同上

namespace SketchCAD {

class SketchModel;     // 本项目类，前向声明须置于同名命名空间内

// 捕捉设置面板（OSNAP）：提供 14 种捕捉模式的独立启用/禁用控制，
// 设定结果经 SketchModel::setSnapConfig 落地为「全局持久配置」并随图存档。
// 与绘制命令期内的 E/T/M/C… 临时切换、Shift+右键临时覆盖互为补充：
//   面板改的是「基线默认值」，命令期切换/覆盖只作用于当前命令、不写回此处。
// 14 个复选项由 snap::snapFieldDefs() 统一驱动，与快捷键、覆盖菜单共用同一数据源。
class SnapSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SnapSettingsDialog(SketchModel* model, QWidget* parent = nullptr);

private slots:
    void accept();                  // 落地到模型（setSnapConfig）

private:
    snap::SnapConfig collect() const;   // 由复选项收集当前配置

    SketchModel*                model_;
    std::vector<QCheckBox*>     checks_;   // 与 snap::snapFieldDefs() 一一对应
};

} // namespace SketchCAD
