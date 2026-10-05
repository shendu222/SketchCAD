#pragma once
#include <QObject>
#include <QString>

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

// 脚本宿主对外暴露的统一交互面（QObject）。
//
// 这是「无需修改主程序源码即可扩展程序」的关键隔离层：脚本引擎（JS/Python/VBA）
// 通过此对象读写程序内部状态、增删图元、执行命令、写日志，而无需直接依赖任何
// UI 或内部头文件。三种语言以各自惯用方式绑定到同一 ScriptApi：
//   - JavaScript：作为全局对象 `app` 注入 QJSEngine
//   - Python    ：经 `sketch` 模块函数转发（见 PythonEngine）
//   - VBA       ：作为命名对象 `App` 的 IDispatch（见 VbaEngine）
//
// 所有方法均为 Q_INVOKABLE，便于 Qt 元对象系统统一反射调用（VBA/Python 桥接复用）。
class ScriptApi : public QObject {
    Q_OBJECT
public:
    explicit ScriptApi(SketchModel* model, CommandInterpreter* interp, QObject* parent = nullptr);

    // 多文档切换重绑：换绑当前活动文档的模型（解释器共享，不随文档换）
    void setModel(SketchModel* model) { model_ = model; }

    // ---- 图元创建（返回新图元 id；失败返回 -1）----
    Q_INVOKABLE int     addLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE int     addCircle(double cx, double cy, double r);
    Q_INVOKABLE int     addArc(double cx, double cy, double r, double startDeg, double endDeg);
    Q_INVOKABLE int     addRect(double x, double y, double w, double h);
    Q_INVOKABLE int     addPoint(double x, double y);
    Q_INVOKABLE int     addText(double x, double y, const QString& text, double heightMm);

    // ---- 图元查询 / 删除 ----
    Q_INVOKABLE int     elementCount() const;
    Q_INVOKABLE QString elementInfo(int id) const;
    Q_INVOKABLE bool    removeElement(int id);

    // ---- 命令 / 日志 ----
    Q_INVOKABLE bool    run(const QString& command);   // 委托命令行解释器执行一条命令
    Q_INVOKABLE void    log(const QString& msg);        // 写日志（经 model_->log，进 LogPanel）

    // ---- 元信息 ----
    Q_INVOKABLE QString version() const { return QStringLiteral("1.0"); }

signals:
    // 脚本输出（print/log）。宿主连接此信号即可把输出汇入日志面板/控制台。
    void scriptOutput(const QString& msg);

private:
    SketchModel*        model_  = nullptr;
    CommandInterpreter* interp_ = nullptr;
};

} // namespace SketchCAD
