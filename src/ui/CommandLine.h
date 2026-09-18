#pragma once
#include <QLineEdit>
#include "core/Vec.h"

namespace SketchCAD {
class SketchModel;
class CommandInterpreter;

// 命令行输入区：回车执行、↑↓ 历史浏览、Tab 自动补全
// 分段式命令流程：当绘制工具激活时，输入坐标 (x,y) 可直接注入为拾取点
class CommandLine : public QLineEdit {
    Q_OBJECT
public:
    CommandLine(SketchModel* m, CommandInterpreter* ci, QWidget* parent = nullptr);
signals:
    // 键盘输入坐标时发出，由 GraphicsView::injectPoint 接收
    void pointEntered(const Vec3& p);
protected:
    void keyPressEvent(QKeyEvent* e) override;
private slots:
    void onReturn();
private:
    SketchModel* model_;
    CommandInterpreter* interp_;
    int histIdx_ = 0;
};
} // namespace SketchCAD
