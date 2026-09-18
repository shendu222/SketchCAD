#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchCAD::MainWindow w;
    w.show();
    return app.exec();
}
