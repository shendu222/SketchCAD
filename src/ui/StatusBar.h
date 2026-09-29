#pragma once
#include <QStatusBar>
#include "core/Units.h"
class QLabel;

namespace SketchCAD {
// 底部状态栏：命令名 / X·Y·Z 坐标 / 坐标系 / 图元数 / 比例
// 坐标按缓存 UnitsMeta 格式化（换算到当前单位 + 精度 + 短后缀），
// 单位变更时 MainWindow 连接 unitsChanged → setUnitsMeta 刷新缓存。
class StatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);
    void setCommand(const QString& s);
    void setCoord(double x, double y, double z);
    void setPlaneInfo(const QString& s);
    void setCount(int n);
    void setScale(double s);
    void setDrawScale(const QString& label);   // 绘图比例标签，如 "1:1" / "1:2"
    void setUnitsMeta(const UnitsMeta& u);     // 缓存单位口径（坐标显示实时跟随）
private:
    QLabel *lCmd, *lCoord, *lPlane, *lCount, *lScale;
    UnitsMeta units_;                          // 坐标显示口径（默认 mm/2 位）
};
} // namespace SketchCAD
