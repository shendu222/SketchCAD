#include "core/I18n.h"
#include "ui/AiAssistantPanel.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>

namespace SketchCAD {

AiAssistantPanel::AiAssistantPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    statusLabel_ = new QLabel(SketchCAD::I18n::t(QString::fromUtf8("AI 助手（自然语言操作）")), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: gray;"));
    root->addWidget(statusLabel_);

    chatView_ = new QTextEdit(this);
    chatView_->setReadOnly(true);
    chatView_->setMinimumHeight(180);
    root->addWidget(chatView_, 1);

    // 输入行：文本框 + 发送 / 停止
    auto* inRow = new QHBoxLayout();
    input_ = new QLineEdit(this);
    input_->setPlaceholderText(SketchCAD::I18n::t(QString::fromUtf8("用自然语言下指令，例如：画一个圆心(0,0)半径50的圆")));
    sendBtn_ = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("发送")), this);
    stopBtn_ = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("停止")), this);
    stopBtn_->setEnabled(false);
    inRow->addWidget(input_, 1);
    inRow->addWidget(sendBtn_);
    inRow->addWidget(stopBtn_);
    root->addLayout(inRow);

    // 破坏性命令审批条（默认隐藏）
    approvalBar_ = new QFrame(this);
    approvalBar_->setFrameShape(QFrame::StyledPanel);
    approvalBar_->setStyleSheet(QStringLiteral("background: #fff4e0;"));
    auto* ap = new QHBoxLayout(approvalBar_);
    ap->setContentsMargins(6, 4, 6, 4);
    approvalLabel_ = new QLabel(this);
    approvalLabel_->setWordWrap(true);
    approveBtn_ = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("执行")), this);
    cancelBtn_  = new QPushButton(SketchCAD::I18n::t(QString::fromUtf8("取消")), this);
    ap->addWidget(approvalLabel_, 1);
    ap->addWidget(approveBtn_);
    ap->addWidget(cancelBtn_);
    approvalBar_->setVisible(false);
    root->addWidget(approvalBar_);

    connect(sendBtn_, &QPushButton::clicked, this, &AiAssistantPanel::onSend);
    connect(stopBtn_, &QPushButton::clicked, this, &AiAssistantPanel::stopRequested);
    connect(input_, &QLineEdit::returnPressed, this, &AiAssistantPanel::onSend);
    connect(approveBtn_, &QPushButton::clicked, this, &AiAssistantPanel::onApprove);
    connect(cancelBtn_,  &QPushButton::clicked, this, &AiAssistantPanel::onCancel);

    setBusy(false);
}

void AiAssistantPanel::focusInput() {
    show();
    raise();
    activateWindow();
    input_->setFocus();
    input_->selectAll();
}

void AiAssistantPanel::appendAssistantDelta(const QString& text) {
    if (text.isEmpty()) return;
    gotDelta_ = true;
    chatView_->moveCursor(QTextCursor::End);
    chatView_->insertPlainText(text);
}

void AiAssistantPanel::setAssistantMessage(const QString& text) {
    chatView_->moveCursor(QTextCursor::End);
    // 兜底：某些后端不产生流式增量、只在结束时给整段文本，此时补插一次，
    // 避免助手回复「什么都不显示」；已收到增量则不重复插入。
    if (!gotDelta_ && !text.isEmpty()) chatView_->insertPlainText(text);
    chatView_->insertPlainText(QStringLiteral("\n"));
}

void AiAssistantPanel::showApproval(const QString& commandLine) {
    approvalLabel_->setText(SketchCAD::I18n::t(QString::fromUtf8("⚠ 该操作会修改图纸，请确认：")) + commandLine);
    approvalBar_->setVisible(true);
    awaitingApproval_ = true;
    setBusy(true);
}

void AiAssistantPanel::appendCommandResult(const QString& line, bool ok) {
    chatView_->moveCursor(QTextCursor::End);
    const QString okMark = QStringLiteral("  ✓");
    const QString failMark = SketchCAD::I18n::t(QString::fromUtf8("  ✗ 失败"));
    chatView_->insertPlainText(QStringLiteral("\n⮕ ") + line + (ok ? okMark : failMark) + QStringLiteral("\n"));
}

void AiAssistantPanel::onFinished(bool ok, const QString& error) {
    Q_UNUSED(ok);
    if (!error.isEmpty()) {
        chatView_->moveCursor(QTextCursor::End);
        chatView_->insertPlainText(QStringLiteral("\n") + SketchCAD::I18n::t(QString::fromUtf8("（错误：")) + error + SketchCAD::I18n::t(QString::fromUtf8("）")) + QStringLiteral("\n"));
    }
    chatView_->moveCursor(QTextCursor::End);
    chatView_->insertPlainText(QStringLiteral("\n") + SketchCAD::I18n::t(QString::fromUtf8("— — — — —")) + QStringLiteral("\n"));
    approvalBar_->setVisible(false);
    awaitingApproval_ = false;
    gotDelta_ = false;
    setBusy(false);
}

void AiAssistantPanel::onSend() {
    const QString t = input_->text().trimmed();
    if (t.isEmpty() || busy_) return;
    if (awaitingApproval_) return;   // 审批未完成前不接受新指令
    appendLine(SketchCAD::I18n::t(QString::fromUtf8("你")), t);
    chatView_->moveCursor(QTextCursor::End);
    chatView_->insertPlainText(SketchCAD::I18n::t(QString::fromUtf8("助手：")));
    input_->clear();
    gotDelta_ = false;
    emit userMessage(t);
}

void AiAssistantPanel::onApprove() {
    if (!awaitingApproval_) return;
    approvalBar_->setVisible(false);
    awaitingApproval_ = false;
    emit approveRequested();
}

void AiAssistantPanel::onCancel() {
    if (!awaitingApproval_) return;
    approvalBar_->setVisible(false);
    awaitingApproval_ = false;
    emit cancelRequested();
}

void AiAssistantPanel::appendLine(const QString& who, const QString& text) {
    chatView_->moveCursor(QTextCursor::End);
    chatView_->insertPlainText(QStringLiteral("\n") + SketchCAD::I18n::t(who) + SketchCAD::I18n::t(QString::fromUtf8("：")) + text + QStringLiteral("\n"));
}

void AiAssistantPanel::setBusy(bool busy) {
    busy_ = busy;
    sendBtn_->setEnabled(!busy);
    stopBtn_->setEnabled(busy);
    if (busy) statusLabel_->setText(SketchCAD::I18n::t(QString::fromUtf8("AI 正在处理…")));
    else      statusLabel_->setText(SketchCAD::I18n::t(QString::fromUtf8("AI 助手（自然语言操作）")));
}

} // namespace SketchCAD
