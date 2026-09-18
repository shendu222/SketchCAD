#include "LogPanel.h"
#include <QDateTime>
#include <QTextCharFormat>
#include <QTextCursor>

namespace SketchCAD {

LogPanel::LogPanel(QWidget* p) : QPlainTextEdit(p) {
    setReadOnly(true);
    setMaximumBlockCount(2000);
    setStyleSheet("QPlainTextEdit{background:#1e2230;color:#d6dbe5;font-family:Consolas,monospace;font-size:12px;}");
    appendPlainText(QStringLiteral("[系统] 航空CAD 草图模块 — 执行信息面板已就绪"));
}

void LogPanel::appendLog(const QString& msg, const QString& type) {
    QColor c = Qt::lightGray;
    if (type == "warn")  c = QColor(220, 170, 60);
    else if (type == "error") c = QColor(230, 90, 90);
    else if (type == "cmd")   c = QColor(120, 180, 230);

    QString t = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = QString("[%1] %2").arg(t, msg);

    QTextCharFormat f; f.setForeground(c);
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(line + "\n", f);
    moveCursor(QTextCursor::End);
}

} // namespace SketchCAD
