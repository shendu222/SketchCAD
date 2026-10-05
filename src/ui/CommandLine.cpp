#include "CommandLine.h"
#include "core/CommandInterpreter.h"
#include "core/CommandPrompt.h"
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

    // 分段式命令提示：监听模型的 commandPromptChanged / commandOptionsChanged 信号，
    // 实时把「提示 + 选项 [C/闭合 U/放弃]」组合到占位文本，作为动态输入提示。
    // 连接登记进 modelConns_（多文档切换时由 setModel 统一迁移）。
    modelConns_ << connect(model_, &SketchModel::commandPromptChanged, this, [this](const QString& prompt) {
        lastPrompt_ = prompt;
        updatePlaceholder();
    });
    modelConns_ << connect(model_, &SketchModel::commandOptionsChanged, this, [this](const QStringList& opts) {
        lastOptions_ = opts;
        updatePlaceholder();
    });
}

// 多文档切换重绑：断开旧模型连接 → 换绑 → 重新连接 → 重置历史游标与占位提示
void CommandLine::setModel(SketchModel* m) {
    for (const auto& c : modelConns_) disconnect(c);
    modelConns_.clear();
    model_ = m;
    if (model_) {
        modelConns_ << connect(model_, &SketchModel::commandPromptChanged, this, [this](const QString& prompt) {
            lastPrompt_ = prompt;
            updatePlaceholder();
        });
        modelConns_ << connect(model_, &SketchModel::commandOptionsChanged, this, [this](const QStringList& opts) {
            lastOptions_ = opts;
            updatePlaceholder();
        });
    }
    histIdx_ = interp_ ? interp_->history().size() : 0;
    updatePlaceholder();
}

void CommandLine::updatePlaceholder() {
    const QString def = QStringLiteral("输入命令 (如 line 0,0 10,0) — 输入 help 查看, Tab 补全, ↑↓ 历史");
    QString ph = lastPrompt_.isEmpty() ? def : lastPrompt_;
    const QString opt = CommandPrompt::formatOptionPrompt(lastOptions_);
    if (!opt.isEmpty()) ph += QStringLiteral("  ") + opt;
    setPlaceholderText(ph);
}

void CommandLine::repeatLastCommand() {
    const QString lc = interp_->lastCommand();
    if (lc.isEmpty()) return;
    model_->log(QStringLiteral("重复命令: ") + lc, "cmd");
    interp_->addHistory(lc);
    histIdx_ = interp_->history().size();
    if (!interp_->execute(lc)) model_->log(QStringLiteral("命令执行失败"), "error");
    clear();
}

void CommandLine::onReturn() {
    const QString cmd = this->text().trimmed();

    // 空输入：重复上一成功执行的命令（对应 AutoCAD 空格/回车重复）。
    if (cmd.isEmpty()) {
        if (!interp_->lastCommand().isEmpty())
            repeatLastCommand();
        else
            clear();
        return;
    }

    // 分段式命令流程：若绘制工具已激活，先尝试把输入解析为选项关键字 / 坐标点
    const QString activeTool = model_->activeTool();
    const bool drawActive = (activeTool != "select" && !activeTool.isEmpty());

    if (drawActive) {
        // 1) 选项关键字优先（如 C/闭合、U/放弃），拦截后不再当作坐标或命令
        const QString key = CommandPrompt::matchOption(cmd, model_->commandOptions());
        if (!key.isEmpty()) {
            model_->log(">> " + cmd + QStringLiteral("  [选项 %1]").arg(key), "cmd");
            emit optionEntered(key);
            clear();
            return;
        }
        // 2) 坐标点（UCS 感知）：绝对 x,y / 相对 @dx,dy / 极坐标 d<a / 相对极坐标 @d<a，
        //    解析出的局部坐标经 model->toWorld 转世界系（唯一换算入口）。
        Vec3 pt;
        if (model_->parsePointToWorld(cmd, model_->lastInputPointWorld(), pt)) {
            model_->log(">> " + cmd + QStringLiteral("  [坐标输入]"), "cmd");
            emit pointEntered(pt);
            clear();
            return;
        }
    }

    // 非坐标/非选项输入 → 作为完整命令执行（支持「命令+参数」整行，如 circle 5,5 10）
    model_->log(">> " + cmd, "cmd");
    interp_->addHistory(cmd);
    histIdx_ = interp_->history().size();
    bool ok = interp_->execute(cmd);
    if (!ok) model_->log(QStringLiteral("命令执行失败"), "error");
    clear();
}

void CommandLine::keyPressEvent(QKeyEvent* e) {
    // 空格在空输入时等价于回车：重复上一命令（避免插入空格字符）
    if (e->key() == Qt::Key_Space && text().trimmed().isEmpty()) {
        if (!interp_->lastCommand().isEmpty())
            repeatLastCommand();
        clear();
        e->accept();
        return;
    }

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
