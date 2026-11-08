#pragma once
#include <QWidget>
#include <QString>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QFrame;

namespace SketchCAD {

// AI 自然语言助手对话面板（停靠控件）。
// 仅负责「展示与采集」：流式文本、命令执行回显、破坏性命令审批条；
// 真正的编排由 MainWindow 持有的 NlAssistant 完成（信号 / 槽对接）。
class AiAssistantPanel : public QWidget {
    Q_OBJECT
public:
    explicit AiAssistantPanel(QWidget* parent = nullptr);

    // 让面板可见并聚焦输入（菜单 / 面板开关调用）
    void focusInput();

signals:
    void userMessage(const QString& text);   // 用户点「发送」
    void stopRequested();                    // 用户点「停止」
    void approveRequested();                 // 用户批准待执行的破坏性命令
    void cancelRequested();                  // 用户取消待执行的破坏性命令

public slots:
    void appendAssistantDelta(const QString& text);   // 流式增量
    void setAssistantMessage(const QString& text);    // 最终助手文本
    void showApproval(const QString& commandLine);    // 展示待审批命令 + 执行/取消
    void appendCommandResult(const QString& line, bool ok);
    void onFinished(bool ok, const QString& error);   // 一轮对话结束

private slots:
    void onSend();
    void onApprove();
    void onCancel();

private:
    void appendLine(const QString& who, const QString& text);
    void setBusy(bool busy);

    QTextEdit*   chatView_;
    QLineEdit*   input_;
    QPushButton* sendBtn_;
    QPushButton* stopBtn_;
    QLabel*      statusLabel_;
    QFrame*      approvalBar_;   // 含待执行命令 + 执行/取消
    QLabel*      approvalLabel_;
    QPushButton* approveBtn_;
    QPushButton* cancelBtn_;
    bool         busy_ = false;
    bool         awaitingApproval_ = false;
    bool         gotDelta_ = false;   // 本轮是否已收到流式增量（用于兜底显示最终文本）
};

} // namespace SketchCAD
