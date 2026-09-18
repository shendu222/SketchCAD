#include "CommandLine.h"
#include "core/CommandInterpreter.h"
#include "core/SketchModel.h"
#include <QKeyEvent>
#include <QCompleter>

namespace SketchCAD {

CommandLine::CommandLine(SketchModel* m, CommandInterpreter* ci, QWidget* p)
    : QLineEdit(p), model_(m), interp_(ci) {
    setPlaceholderText(QStringLiteral("输入命令 (如 line 0,0 10,0) — 输入 help 查看, Tab 补全, ↑↓ 历史"));
    auto* comp = new QCompleter(interp_->commandNames(), this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(comp);
    connect(this, &QLineEdit::returnPressed, this, &CommandLine::onReturn);

    // 分段式命令提示：监听模型的 commandPromptChanged 信号，实时更新占位提示
    connect(model_, &SketchModel::commandPromptChanged, this, [this](const QString& prompt) {
        if (prompt.isEmpty()) {
            setPlaceholderText(QStringLiteral("输入命令 (如 line 0,0 10,0) — 输入 help 查看, Tab 补全, ↑↓ 历史"));
        } else {
            setPlaceholderText(prompt);
        }
    });
}

void CommandLine::onReturn() {
    const QString cmd = this->text().trimmed();
    if (cmd.isEmpty()) return;

    // 分段式命令流程：若绘制工具已激活，先尝试将输入解析为坐标点
    const QString activeTool = model_->activeTool();
    const bool drawActive = (activeTool != "select" && !activeTool.isEmpty());

    if (drawActive) {
        Vec3 pt;
        if (CommandInterpreter::tryParsePoint(cmd, pt)) {
            // 输入是坐标 → 注入为拾取点（等效于鼠标点击）
            model_->log(">> " + cmd + QStringLiteral("  [坐标输入]"), "cmd");
            emit pointEntered(pt);
            clear();
            return;
        }
    }

    // 非坐标输入 → 作为完整命令执行
    model_->log(">> " + cmd, "cmd");
    interp_->addHistory(cmd);
    histIdx_ = interp_->history().size();
    bool ok = interp_->execute(cmd);
    if (!ok) model_->log(QStringLiteral("命令执行失败"), "error");
    clear();
}

void CommandLine::keyPressEvent(QKeyEvent* e) {
    const QStringList& h = interp_->history();
    if (e->key() == Qt::Key_Up) {
        if (!h.isEmpty()) { if (histIdx_ > 0) histIdx_--; setText(h[histIdx_]); }
        e->accept(); return;
    }
    if (e->key() == Qt::Key_Down) {
        if (!h.isEmpty()) {
            if (histIdx_ < h.size() - 1) { histIdx_++; setText(h[histIdx_]); }
            else { histIdx_ = h.size(); clear(); }
        }
        e->accept(); return;
    }
    if (e->key() == Qt::Key_Tab) {
        QString prefix = text();
        QStringList c = interp_->completions(prefix);
        if (c.size() == 1) { setText(c[0] + " "); }
        else if (c.size() > 1) {
            QString common = c[0];
            for (int i = 1; i < c.size(); ++i)
                while (!c[i].startsWith(common) && !common.isEmpty()) common.chop(1);
            if (common.length() > prefix.length()) setText(common);
            model_->log("候选: " + c.join(" "));
        }
        e->accept(); return;
    }
    QLineEdit::keyPressEvent(e);
}

} // namespace SketchCAD
