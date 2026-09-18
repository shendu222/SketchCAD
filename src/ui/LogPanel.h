#pragma once
#include <QPlainTextEdit>

namespace SketchCAD {
// 执行信息面板：以日志形式输出操作记录/命令结果/错误
class LogPanel : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);
public slots:
    void appendLog(const QString& msg, const QString& type = "info");
};
} // namespace SketchCAD
