#pragma once
#include <QLineEdit>
#include <QList>
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
    // 多文档切换重绑：换绑活动文档；历史由解释器层互换（DocTab 会话态），此处重置浏览游标
    void setModel(SketchModel* m);
public slots:
    // 重复上一成功执行的命令（空输入回车/空格、空闲态右键三入口共用；保持历史游标同步）
    void repeatLastCommand();
signals:
    // 键盘输入坐标时发出，由 GraphicsView::injectPoint 接收
    void pointEntered(const Vec3& p);
    // 绘制命令期间输入选项关键字（如 C/闭合）时发出，由 GraphicsView::applyCommandOption 接收
    void optionEntered(const QString& key);
protected:
    void keyPressEvent(QKeyEvent* e) override;
private slots:
    void onReturn();
private:
    void updatePlaceholder();   // 组合 commandPrompt + 选项提示 [C/闭合] 到占位文本
    SketchModel* model_;
    CommandInterpreter* interp_;
    QList<QMetaObject::Connection> modelConns_;   // 自连接的模型信号（setModel 迁移用）
    int histIdx_ = 0;
    QString lastPrompt_;        // 最近一次模型下发的分段命令提示
    QStringList lastOptions_;   // 最近一次模型下发的可选项关键字列表
};
} // namespace SketchCAD
