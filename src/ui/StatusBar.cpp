#include "StatusBar.h"
#include <QLabel>

namespace SketchCAD {

StatusBar::StatusBar(QWidget* p) : QStatusBar(p) {
    lCmd   = new QLabel(QStringLiteral("就绪"));   lCmd->setMinimumWidth(120);
    lCoord = new QLabel(QStringLiteral("X:0.00  Y:0.00  Z:0.00")); lCoord->setMinimumWidth(170);
    lPlane = new QLabel(QStringLiteral("平面:XY")); lPlane->setMinimumWidth(70);
    lCount = new QLabel(QStringLiteral("图元:0"));  lCount->setMinimumWidth(60);
    lScale = new QLabel(QStringLiteral("比例:4.00")); lScale->setMinimumWidth(70);
    addWidget(lCmd); addWidget(lCoord); addWidget(lPlane);
    addWidget(lCount); addWidget(lScale);
    setStyleSheet("QStatusBar::item{border:0;}");
}

void StatusBar::setCommand(const QString& s) { lCmd->setText(s); }
void StatusBar::setCoord(double x, double y, double z) {
    lCoord->setText(QString("X:%1  Y:%2  Z:%3").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(z, 0, 'f', 2));
}
void StatusBar::setPlaneInfo(const QString& s) { lPlane->setText("平面:" + s); }
void StatusBar::setCount(int n) { lCount->setText("图元:" + QString::number(n)); }
void StatusBar::setScale(double s) { lScale->setText("比例:" + QString::number(s, 'f', 2)); }
void StatusBar::setDrawScale(const QString& label) { lScale->setText("比例:" + label); }

} // namespace SketchCAD
