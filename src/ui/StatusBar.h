#pragma once
#include <QStatusBar>
class QLabel;

namespace SketchCAD {
// 底部状态栏：命令名 / X·Y·Z 坐标 / 坐标系 / 图元数 / 比例
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
private:
    QLabel *lCmd, *lCoord, *lPlane, *lCount, *lScale;
};
} // namespace SketchCAD
