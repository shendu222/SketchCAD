#pragma once
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include "core/SketchModel.h"

namespace SketchCAD {

// 约束栏（Constraint Bar）：展示当前选中图元（或全部）参与的全部几何约束及其状态，
// 支持「定位」（缩放至相关图元）与「删除」（移除该约束）。过约束/冲突时显示红色告警横幅。
// 数据来源为 SketchModel::constraints()（显式约束表），与图形视图的约束图标（drawForeground）
// 共用同一份数据源，保证面板与画布状态一致。
class ConstraintBar : public QWidget {
    Q_OBJECT
public:
    explicit ConstraintBar(SketchModel* m, QWidget* parent = nullptr);

public slots:
    void refresh();   // 选中变化 / 约束表变化 / 几何变化 时刷新列表与告警

signals:
    void locateRequested(int elemId);        // 请求视图缩放至相关图元（elemId）
    void deleteRequested(int constraintIndex); // 请求删除约束表中下标为 constraintIndex 的约束

private:
    SketchModel* model_;
    QLabel*     headerLabel_;
    QLabel*     bannerLabel_;
    QListWidget* list_;
    QPushButton* locateBtn_;
    QPushButton* deleteBtn_;

    void onLocate();
    void onDelete();
};

} // namespace SketchCAD
