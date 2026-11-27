#include "core/I18n.h"
#include "ai/NlAssistant.h"

#include "core/CommandInterpreter.h"
#include "core/CommandCatalog.h"
#include "core/Commands.h"      // 注册表为已注册命令的权威来源（命令目录可能滞后）
#include "core/Element.h"
#include "core/SketchModel.h"
#include "ai/tooldefs.h"
#include "ai/NlCommandMapper.h"   // 仅用于解码「模型输出」的结构化命令（兼容非 function-calling 后端）
#include "core/standards/StandardsDB.h" // 标准知识库：queryStandard / system prompt 摘要的唯一数据源
#include "core/standards/ToleranceDB.h" // ISO 286-1 公式：数值型规则（Φ30 IT7）实时计算，不复制常数表
#include "core/standards/ComplianceChecker.h" // checkCompliance 工具：与 STDCHECK 命令同源的纯读检查

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMap>
#include <QStringList>
#include <QRegularExpression>

namespace SketchCAD {

namespace {

// IT 等级字符串 → 枚举：IT01 单独处理，IT0=1，IT1=2 … IT18=19（即序号 n+1）。
// 兼容非 function-calling 后端：模型直接输出一行裸命令行（如 "stdpart M10X100"）
// 时，按「命令名必须是已注册命令」的严格条件解码为一次工具调用。
// 这不是本地意图推理，只是把模型给出的命令文本接到既有执行链上。
QString decodeBareCommandLine(const QString& modelOutput) {
    QString s = modelOutput.trimmed();
    if (s.isEmpty() || s.contains(QLatin1Char('{'))) return QString();
    // 去掉模型偶尔附带的代码围栏 / 角色前缀
    if (s.startsWith(QStringLiteral("```"))) {
        s.remove(0, 3);
        const int fence = s.lastIndexOf(QStringLiteral("```"));
        if (fence >= 0) s.truncate(fence);
        s = s.trimmed();
    }
    const QStringList prefixes = {
        QStringLiteral("助手："), QStringLiteral("助手:"),
        QStringLiteral("命令："), QStringLiteral("命令:"),
        QStringLiteral("执行："), QStringLiteral("执行:")
    };
    for (const QString& p : prefixes)
        if (s.startsWith(p)) { s = s.mid(p.size()).trimmed(); break; }
    if (s.isEmpty() || s.contains(QLatin1Char('\n')) || s.contains(QStringLiteral("。"))) return QString();

    const QString cmd = s.section(QLatin1Char(' '), 0, 0).trimmed().toLower();
    // 命令注册表优先：即使 config/command_catalog.json 尚未随新二进制更新，
    // 已编译进去的命令（如 stdpart）也必须能被识别执行。
    const bool registered = CommandRegistry::instance().findExact(cmd) != nullptr
                         || CommandInterpreter::commandNames().contains(cmd);
    if (!registered) return QString();
    return s;
}

bool parseITGrade(const QString& s, ITGrade& out) {
    const QString t = s.trimmed();
    if (t == QStringLiteral("01")) { out = ITGrade::IT01; return true; }
    bool ok = false;
    const int n = t.toInt(&ok);
    if (!ok || n < 0 || n > 18) return false;
    out = static_cast<ITGrade>(n + 1);
    return true;
}

// 高频规则 → (标准表, 键列表)。builder lambda 逐条注册，规避 GCC 13.1
// 巨型聚合初始化静默崩溃；中文关键字一律用 UTF-8 字面量 + fromUtf8 比较。
struct RuleEntry {
    const char* kwUtf8;
    const char* table;
    const char* keysCsv;
};

QVector<RuleEntry> buildRuleEntries() {
    QVector<RuleEntry> v;
    auto add = [&](const char* kw, const char* t, const char* keys) {
        v.append(RuleEntry{kw, t, keys});
    };
    add("粗细比",   "line_types",  "thick_thin_ratio,width_series");
    add("线宽比",   "line_types",  "thick_thin_ratio,width_series");
    add("线宽粗细", "line_types",  "thick_thin_ratio,width_series");
    add("粗线",     "line_types",  "thick_thin_ratio");
    add("线宽系数", "line_types",  "width_series_ratio");
    add("线宽系列", "line_types",  "width_series");
    add("最小间隙", "line_types",  "min_gap_mm");
    add("线间距",   "line_types",  "min_gap_mm");
    add("间隙",     "line_types",  "min_gap_mm");
    add("线型",     "line_types",  "types");
    add("最小字高", "fonts",       "hanzi_min_height,hanzi_font");
    add("字高",     "fonts",       "hanzi_min_height,sizes");
    add("字号",     "fonts",       "sizes,hanzi_min_height");
    add("字宽",     "fonts",       "hanzi_width_ratio");
    add("长仿宋",   "fonts",       "hanzi_font,hanzi_width_ratio,hanzi_min_height");
    add("字体",     "fonts",       "hanzi_font,sizes");
    add("留边",     "sheet_sizes", "margin_c,margin_e");
    add("边距",     "sheet_sizes", "margin_c,margin_e");
    add("装订",     "sheet_sizes", "binding_edge_a");
    add("幅面",     "sheet_sizes", "A0,A1,A2,A3,A4");
    add("A0",       "sheet_sizes", "A0");
    add("A1",       "sheet_sizes", "A1");
    add("A2",       "sheet_sizes", "A2");
    add("A3",       "sheet_sizes", "A3");
    add("A4",       "sheet_sizes", "A4");
    add("加长幅面", "sheet_sizes", "long_sizes");
    add("比例",     "scales",      "1:1,2:1,1:2,1:5");
    add("公差等级", "tolerances",  "IT_grades");
    add("等级系列", "tolerances",  "IT_grades");
    add("尺寸段",   "tolerances",  "size_segments");
    add("配合",     "tolerances",  "H_lower,h_upper,js_rule");
    add("基本偏差", "tolerances",  "shaft_deviations,hole_deviations,H_lower,h_upper");
    add("粗糙度",   "roughness",   "Ra_series,symbol_angle");
    add("Ra",       "roughness",   "Ra_series");
    add("螺纹",     "thread",      "types,example_M,_pitch_standard");
    add("螺距",     "thread",      "_pitch_standard,_coarse_pitch_note");
    add("焊缝",     "weld",        "_standard");
    add("几何公差", "geo_tol",     "_standard");
    add("图层",     "cad_layers",  "_standard");
    // 兜底：模型若把「线宽合规吗」误送进 queryStandard，也返回线宽规则而非空答案。
    add("线宽",     "line_types",  "thick_thin_ratio,width_series,min_gap_mm");
    return v;
}

// 合规检查器 7 条规则的展示元数据（顺序与 ComplianceChecker::run 一致）。
struct RuleMeta {
    const char* id;
    const char* zhUtf8;
};

QVector<RuleMeta> buildRuleMetas() {
    QVector<RuleMeta> v;
    auto add = [&](const char* id, const char* zh) { v.append(RuleMeta{id, zh}); };
    add("R1_line_width_ratio", "线宽粗细比");
    add("R2_min_line_gap",     "两线最小间隙");
    add("R3_text_height",      "字号下限");
    add("R4_roughness_notation", "粗糙度符号");
    add("R5_weld_symbol",      "焊缝符号");
    add("R6_sheet_margin",     "幅面留边");
    add("R7_layer_naming",     "图层命名");
    return v;
}

} // namespace

const QStringList& NlAssistant::destructiveCommands() {
    static const QStringList s = {
        QStringLiteral("delete"),  QStringLiteral("erase"),
        QStringLiteral("trim"),    QStringLiteral("extend"),
        QStringLiteral("purge"),   QStringLiteral("overkill"),
        QStringLiteral("break"),   QStringLiteral("explode"),
        QStringLiteral("undo")
    };
    return s;
}

NlAssistant::NlAssistant(CommandInterpreter* interp, LlmProvider* provider,
                         AppContextProvider* ctx, SketchModel* model, QObject* parent)
    : QObject(parent), interp_(interp), provider_(provider), ctx_(ctx), model_(model) {
    LlmMessage sys;
    sys.role = LlmRole::System;
    sys.content = buildSystemPrompt();
    history_.append(sys);
}

void NlAssistant::sendUserMessage(const QString& text) {
    if (busy_) return;
    busy_ = true;
    aborted_ = false;
    step_ = 0;
    lastBareCommand_.clear();
    LlmMessage u;
    u.role = LlmRole::User;
    u.content = text;
    history_.append(u);

    // 用户在 AI 面板里直接输入一条已注册命令（如 "stdpart M10X100"）时，
    // 按命令行同源语义立即执行，不再交给模型复述。破坏性命令仍走模型审批流程。
    const QString direct = decodeBareCommandLine(text);
    if (!direct.isEmpty() && !isDestructive(direct)) {
        lastBareCommand_ = direct;
        LlmToolCall tc;
        tc.id = QStringLiteral("user_command_%1").arg(step_);
        tc.name = QStringLiteral("runSketchCommand");
        tc.arguments = QJsonObject{{ QStringLiteral("line"), direct }};
        LlmMessage a;
        a.role = LlmRole::Assistant;
        a.content = direct;
        a.toolCalls = { tc };
        history_.append(a);
        const bool ok = interp_ ? interp_->execute(direct) : false;
        emit commandExecuted(direct, ok);
        LlmMessage toolMsg;
        toolMsg.role = LlmRole::Tool;
        toolMsg.toolCallId = tc.id;
        toolMsg.toolName = tc.name;
        toolMsg.content = ok ? QStringLiteral("命令已执行成功。") : QStringLiteral("命令执行失败。");
        history_.append(toolMsg);
        finalize(ok ? QStringLiteral("已执行命令：%1").arg(direct)
                    : QStringLiteral("命令执行失败：%1").arg(direct), QString());
        return;
    }

    // 用户原文原样进入模型：语义理解、参数抽取、命令串生成与结果说明
    // 全部由 LlmProvider 背后的模型完成。
    runTurn();
}

void NlAssistant::runTurn() {
    if (aborted_) { finalize({}, QStringLiteral("已取消")); return; }
    // LLM 是核心处理环节，缺后端必须显式报错，绝不回落到本地规则。
    if (provider_ == nullptr) {
        finalize({}, QStringLiteral("未配置 LLM 后端（工具 → 选项 → AI 助手）"));
        return;
    }
    if (interp_ == nullptr) { finalize({}, QStringLiteral("命令解释器未正确初始化")); return; }
    if (step_ >= kMaxSteps) { finalize({}, QStringLiteral("达到最大对话步数，已停止")); return; }
    ++step_;
    accContent_.clear();
    accToolCalls_.clear();
    // 每轮刷新 system 上下文（选择集 / 图层等随操作变化）
    if (!history_.isEmpty() && history_.first().role == LlmRole::System)
        history_.first().content = buildSystemPrompt();

    const QVector<LlmTool> tools = NlTools::schemas();
    provider_->chat(history_, tools, options_,
        [this](const QString& d) { accContent_ += d; emit tokenDelta(d); },
        [this](const QVector<LlmToolCall>& c) { accToolCalls_ = c; },
        [this](const QString& err) { handleDone(err); });
}

void NlAssistant::handleDone(const QString& err) {
    if (!err.isEmpty()) { finalize({}, err); return; }

    QVector<LlmToolCall> calls = accToolCalls_;
    // 兼容路径：部分本地模型/网关不产生 function-calling，改以结构化 JSON
    // 文本给出命令对象。解码的对象是【模型输出】，等价于一次工具调用；
    // 命令内容仍完全由模型决定，本地只做协议解码。
    if (calls.isEmpty()) {
        const NlParseResult r = NlCommandMapper::parse(accContent_);
        if (r.ok && !r.commandLine.isEmpty()) {
            LlmToolCall tc;
            tc.id = QStringLiteral("model_text_%1").arg(step_);
            tc.name = QStringLiteral("runSketchCommand");
            tc.arguments = QJsonObject{{ QStringLiteral("line"), r.commandLine }};
            calls.append(tc);
        } else {
            // 裸命令行回退：模型没有 function-calling、也没有输出 JSON，
            // 而是直接给出 "stdpart M10X100" 一行命令时仍应真正执行。
            const QString bare = decodeBareCommandLine(accContent_);
            if (!bare.isEmpty() && bare != lastBareCommand_) {
                lastBareCommand_ = bare;
                LlmToolCall tc;
                tc.id = QStringLiteral("model_bare_%1").arg(step_);
                tc.name = QStringLiteral("runSketchCommand");
                tc.arguments = QJsonObject{{ QStringLiteral("line"), bare }};
                calls.append(tc);
            }
        }
    }

    LlmMessage assistant;
    assistant.role = LlmRole::Assistant;
    assistant.content = accContent_;
    assistant.toolCalls = calls;   // 与后续 Tool 消息配对，保证对话协议合法
    history_.append(assistant);

    if (calls.isEmpty()) {
        // 无工具调用 = 模型生成的最终文本回复（不是本地拼装的模板文案）
        finalize(accContent_, QString());
    } else {
        processToolCalls(calls);
    }
}

void NlAssistant::processToolCalls(const QVector<LlmToolCall>& calls) {
    for (const LlmToolCall& call : calls) {
        if (call.name == QStringLiteral("runSketchCommand")) {
            const QString line = call.arguments.value(QStringLiteral("line")).toString().trimmed();
            if (isDestructive(line)) {
                pendingCall_ = call;
                pendingCommand_ = line;
                emit approvalRequested(line);
                return;   // 挂起，等待用户审批
            }
            const bool ok = interp_ ? interp_->execute(line) : false;
            emit commandExecuted(line, ok);
            LlmMessage toolMsg;
            toolMsg.role = LlmRole::Tool;
            toolMsg.content = ok ? QStringLiteral("命令已执行成功。")
                                 : QStringLiteral("命令执行失败（语法错误或参数非法）。");
            toolMsg.toolCallId = call.id;
            toolMsg.toolName = call.name;
            history_.append(toolMsg);
        } else {
            const QString result = runQueryTool(call.name, call.arguments);
            LlmMessage toolMsg;
            toolMsg.role = LlmRole::Tool;
            toolMsg.content = result;
            toolMsg.toolCallId = call.id;
            toolMsg.toolName = call.name;
            history_.append(toolMsg);
        }
    }
    runTurn();
}

void NlAssistant::approvePending() {
    if (pendingCommand_.isEmpty()) return;
    const QString line = pendingCommand_;
    const LlmToolCall call = pendingCall_;
    pendingCommand_.clear();
    pendingCall_ = LlmToolCall();
    const bool ok = interp_ ? interp_->execute(line) : false;
    emit commandExecuted(line, ok);
    LlmMessage toolMsg;
    toolMsg.role = LlmRole::Tool;
    toolMsg.content = ok ? QStringLiteral("命令已执行成功。") : QStringLiteral("命令执行失败。");
    toolMsg.toolCallId = call.id;
    toolMsg.toolName = call.name;
    history_.append(toolMsg);
    runTurn();
}

void NlAssistant::cancelPending() {
    if (pendingCommand_.isEmpty()) return;
    const LlmToolCall call = pendingCall_;
    pendingCommand_.clear();
    pendingCall_ = LlmToolCall();
    LlmMessage toolMsg;
    toolMsg.role = LlmRole::Tool;
    toolMsg.content = QStringLiteral("用户取消了该操作。");
    toolMsg.toolCallId = call.id;
    toolMsg.toolName = call.name;
    history_.append(toolMsg);
    runTurn();
}

void NlAssistant::stop() {
    aborted_ = true;
    if (busy_) finalize({}, QStringLiteral("已停止"));
}

void NlAssistant::finalize(const QString& assistantText, const QString& error) {
    busy_ = false;
    if (!assistantText.isEmpty()) emit assistantMessage(assistantText);
    emit finished(error.isEmpty(), error);
}

QString NlAssistant::buildSystemPrompt() const {
    QString p = QStringLiteral(
        "你是 SketchCAD 草图设计软件的内置自然语言助手。"
        "用户用自然语言描述绘图或操作意图；你必须先理解语义、抽取参数，"
        "再调用 runSketchCommand 工具给出完整命令串（参数 line）。"
        "命令语法与界面命令行完全一致，不要输出代码块，不要复述命令，直接调用工具。"
        "\n【命令目录唯一性】下方“命令目录”由程序从 CommandCatalog 注入，"
        "是命令名称、用途、参数与使用示例的唯一事实来源；不要在目录之外猜测或发明命令。"
        "自然语言抽取出的每个命令串都必须与目录中某条命令的 name / usage / example 对应。"
        "\n【语义消歧】当两种图元语义同时出现时，以目录条目的用途描述最贴切者为准；"
        "椭圆语义（椭圆/长半轴/短半轴/长轴/短轴）不得映射为圆形条目，反之亦然。"
        "遇到“绘制一个<零件名称>”“生成<标准件>”“插入<零件名称>”且含机械标准件品名"
        "（螺栓、T形槽用螺栓、六角头螺栓、内六角螺栓、螺母、垫圈、销、键、轴承、铆钉、挡圈等）"
        "与规格（如 M10X100、M6x30、10x50、M8）时，必须调用 `stdpart` 命令，"
        "对应菜单【绘图】-【标准件】，直接生成标准件；命令串必须写成"
        "`stdpart <完整品名> <规格>`，例如 `stdpart T形槽用螺栓 M10X100`，"
        "不得省略用户给出的品名（否则可能匹配到其它同类标准件）。"
        "禁止把品名拆成“T形槽”+“螺栓”两段，也禁止改用 line/circle/rect 等通用绘图命令分步拼画。"
        "\n执行结果会以 tool 消息回传给你，请据此用中文简要说明结果（结果文本也由你生成）。"
        "\n不确定语法时先用 getCommandHelp(name) 查询；需要图纸状态时用 "
        "getSelection / getLayers / listElements。"
        "\n【国标问答】涉及机械制图国标的数值或条文时，必须调用 queryStandard(rule) 精确查询，"
        "不得凭记忆回答；问「这张图是否合规 / 线宽合规吗」时必须调用 checkCompliance()，"
        "由检查器对当前图纸给出结论。这两个工具都是只读查询，不会执行任何绘图命令；"
        "只有真正要改图时才用 runSketchCommand。");

    // 全量注入命令目录：名称/用途/用法/参数/示例均取自 CommandCatalog，
    // 与 help 命令读取的是同一份数据，避免另维护一份“核心命令表”造成漂移。
    QVector<QPair<QString, QStringList>> groups;
    for (const CommandInfo& ci : CommandCatalog::entries()) {
        QStringList* bucket = nullptr;
        for (auto& g : groups)
            if (g.first == ci.category) { bucket = &g.second; break; }
        if (!bucket) { groups.append({ ci.category, QStringList() }); bucket = &groups.last().second; }
        const QString row = QStringLiteral("%1（%2）| 用途:%3 | 用法:%4 | 参数:%5 | 示例:%6")
                                .arg(ci.name, ci.zhName, ci.summary, ci.usage, ci.params, ci.example);
        *bucket << row;
    }
    if (!groups.isEmpty()) {
        p += QStringLiteral("\n命令目录（CommandCatalog 全量，必须严格遵守）：\n");
        for (const auto& g : groups)
            p += QStringLiteral("[%1]\n%2\n").arg(g.first, g.second.join(QLatin1Char('\n')));
    }

    // 构造期（以及每轮刷新）注入国标高频规则摘要：只放「高频规则 + 标准号 +
    // 用 queryStandard 精确查询」的指引，绝不把 12 张全量 JSON 塞进上下文。
    p += QStringLiteral("\n") + StandardsDB::instance().summary();

    if (ctx_) p += QStringLiteral("\n当前图纸上下文：") + ctx_->contextJson();
    return p;
}

bool NlAssistant::isDestructive(const QString& line) const {
    const QString cmd = line.section(QLatin1Char(' '), 0, 0).trimmed().toLower();
    return destructiveCommands().contains(cmd);
}

QString NlAssistant::runQueryTool(const QString& name, const QJsonObject& args) {
    if (name == QStringLiteral("getSelection"))
        return ctx_ ? ctx_->selectionSummary() : QStringLiteral("（无选择集信息）");

    if (name == QStringLiteral("listElements")) {
        if (!model_) return QStringLiteral("（无图纸）");
        QStringList parts;
        parts.append(SketchCAD::I18n::t(QString::fromUtf8("共 %1 个图元。")).arg(model_->elementCount()));
        QMap<QString, int> cnt;
        for (const Element& e : model_->elements())
            cnt[SketchCAD::I18n::t(QString::fromUtf8(elementTypeName(e.type)))] += 1;
        QStringList detail;
        for (auto it = cnt.begin(); it != cnt.end(); ++it)
            detail.append(QStringLiteral("%1×%2").arg(it.key()).arg(it.value()));
        if (!detail.isEmpty()) parts.append(detail.join(QStringLiteral("，")));
        return parts.join(QStringLiteral(" "));
    }

    if (name == QStringLiteral("getLayers"))
        return ctx_ ? ctx_->layersSummary() : QStringLiteral("（无图层信息）");

    if (name == QStringLiteral("getCommandHelp")) {
        const QString nm = args.value(QStringLiteral("name")).toString().trimmed().toLower();
        const CommandInfo* info = CommandCatalog::find(nm);
        if (!info) return QStringLiteral("未知命令：") + nm;
        QString out = info->name + QStringLiteral("（") + info->zhName + QStringLiteral("）\n") + info->summary;
        if (!info->usage.isEmpty()) out += QStringLiteral("\n用法：") + info->usage;
        if (!info->example.isEmpty()) out += QStringLiteral("\n示例：") + info->example;
        return out;
    }

    if (name == QStringLiteral("query_standards")) {
        const QString table = args.value(QStringLiteral("table")).toString().trimmed().toLower();
        const QString key = args.value(QStringLiteral("key")).toString().trimmed();
        if (table.isEmpty())
            return QStringLiteral("缺少参数 table（标准表名）。");
        const StandardsDB& db = StandardsDB::instance();
        const QJsonObject obj = db.tableObject(table);
        if (obj.isEmpty())
            return QStringLiteral("未知标准表：") + table +
                   QStringLiteral("（可用：") + db.allTables().join(QChar(' ')) + QStringLiteral("）");
        if (key.isEmpty()) {
            QJsonObject out;
            out[table] = obj;
            return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Indented));
        }
        const QJsonValue v = db.query(table, key);
        if (v.isUndefined() || v.isNull())
            return QStringLiteral("未找到键：") + key + QStringLiteral("（") + table +
                   QStringLiteral(" 可用键：") + QStringList(obj.keys()).join(QChar(' ')) + QStringLiteral("）");
        return table + QChar('.') + key + QStringLiteral(" = ") + db.queryString(table, key);
    }

    // ---- 新增两个国标只读工具：与 runSketchCommand 的动作出口严格分离 ----
    if (name == QStringLiteral("queryStandard"))
        return queryStandardRule(args.value(QStringLiteral("rule")).toString());

    if (name == QStringLiteral("checkCompliance"))
        return complianceReport(args.value(QStringLiteral("rule")).toString());

    return QStringLiteral("未知工具：") + name;
}

QString NlAssistant::queryStandardRule(const QString& rule) const {
    const StandardsDB& db = StandardsDB::instance();
    const QString r = rule.trimmed();
    if (r.isEmpty())
        return QStringLiteral("缺少参数 rule：请给出要精确查询的国标规则，"
                              "例如「M20 粗牙螺距」「Φ30 IT7 标准公差」「线宽粗细比」。");

    // ① 普通螺纹粗牙螺距：数据源 = StandardsDB.query(thread, coarse_pitch_M)。
    //    M20 默认即粗牙 P=2.5mm；标注中显式写 ×P 时为细牙，本分支只答粗牙系列。
    {
        const QRegularExpression re(
            QStringLiteral("(?:^|[^A-Za-z0-9])M([0-9]+(?:\\.[0-9]+)?)(?=$|[^0-9A-Za-z.])"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(r);
        if (m.hasMatch()) {
            const QString code = QStringLiteral("M") + m.captured(1);
            const QJsonObject tab = db.query(QStringLiteral("thread"),
                                             QStringLiteral("coarse_pitch_M")).toObject();
            const QString stdNo = db.query(QStringLiteral("thread"),
                                           QStringLiteral("_pitch_standard")).toString();
            const QString note  = db.query(QStringLiteral("thread"),
                                           QStringLiteral("_coarse_pitch_note")).toString();
            if (!tab.contains(code))
                return QStringLiteral("%1 不在内嵌粗牙螺距表内（%2）。请用 query_standards "
                                      "查看 thread 表，或在 rule 中给出已收录的公称直径。")
                           .arg(code, stdNo);
            const double pitch = tab.value(code).toDouble();
            return QStringLiteral("%1 普通螺纹粗牙螺距 P = %2 mm（%3）。%4"
                                  "标注示例：%5（细牙须显式写 ×P，如 M20×2 表示螺距 2mm）。"
                                  "数据源：StandardsDB.query(thread, coarse_pitch_M)['%6']。")
                       .arg(code, QString::number(pitch, 'g', 6), stdNo, note,
                            db.query(QStringLiteral("thread"), QStringLiteral("example_M")).toString(),
                            code);
        }
    }

    // ② 标准公差 IT：数值由 ToleranceDB 按 GB/T 1800.1 / ISO 286-1 公式实时计算，
    //    口径说明与标准号仍从 StandardsDB.query(tolerances, ...) 读取，不复制常数表。
    {
        const QRegularExpression reGrade(QStringLiteral("IT\\s*([0-9]{1,2})"),
                                        QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch gm = reGrade.match(r);
        if (gm.hasMatch()) {
            ITGrade grade = ITGrade::IT7;
            if (!parseITGrade(gm.captured(1), grade))
                return QStringLiteral("无法解析 IT 等级：%1（有效范围 IT01 / IT0 / IT1~IT18）。")
                           .arg(gm.captured(1));
            QString rest = r;
            rest.remove(gm.capturedStart(), gm.capturedLength());
            const QRegularExpression reNum(QStringLiteral("[0-9]+(?:\\.[0-9]+)?"));
            const QRegularExpressionMatch sm = reNum.match(rest);
            if (!sm.hasMatch())
                return QStringLiteral("标准公差需要基本尺寸，请在 rule 中给出，例如「Φ30 IT7 标准公差」。");
            const double size = sm.captured(0).toDouble();
            const double it = ToleranceDB::standardTolerance(size, grade);
            const QString engine = db.query(QStringLiteral("tolerances"),
                                            QStringLiteral("standard_tolerance_mm")).toString();
            const QString stdNo = db.query(QStringLiteral("tolerances"),
                                           QStringLiteral("_standard")).toString();
            return QStringLiteral("Φ%1 IT%2 标准公差 IT = %3 mm（%4 μm）。依据 %5；"
                                  "计算口径：%6。"
                                  "数据源：ToleranceDB::standardTolerance(%7, IT%8) + "
                                  "StandardsDB.query(tolerances, standard_tolerance_mm)。")
                       .arg(QString::number(size, 'g', 8), gm.captured(1),
                            QString::number(it, 'f', 6), QString::number(it * 1000.0, 'g', 6),
                            stdNo, engine, QString::number(size, 'g', 8), gm.captured(1));
        }
    }

    // ③ 配合偏差（如「Φ30 H7 孔上下偏差」）：同样复用 ToleranceDB 公式，
    //    标准号/偏差代号表从 StandardsDB.query 读取。
    const bool asksDeviation = r.contains(QStringLiteral("偏差")) ||
                               r.contains(QStringLiteral("配合")) ||
                               r.contains(QStringLiteral("极限尺寸"));
    if (asksDeviation) {
        const QRegularExpression reDev(
            QStringLiteral("(?:^|[^A-Za-z])([A-Za-z]{1,2})\\s*([0-9]{1,2})(?=$|[^0-9A-Za-z])"));
        const QRegularExpressionMatch dm = reDev.match(r);
        const QString letters = dm.hasMatch() ? dm.captured(1) : QString();
        if (dm.hasMatch() && letters.compare(QStringLiteral("IT"), Qt::CaseInsensitive) != 0 &&
            letters.compare(QStringLiteral("GB"), Qt::CaseInsensitive) != 0) {
            ITGrade grade = ITGrade::IT7;
            if (parseITGrade(dm.captured(2), grade)) {
                QString rest = r;
                rest.remove(dm.capturedStart(), dm.capturedLength());
                const QRegularExpression reNum(QStringLiteral("[0-9]+(?:\\.[0-9]+)?"));
                const QRegularExpressionMatch sm = reNum.match(rest);
                if (sm.hasMatch()) {
                    const double size = sm.captured(0).toDouble();
                    const bool hole = r.contains(QStringLiteral("孔")) ||
                                      (letters.at(0).isUpper() && !r.contains(QStringLiteral("轴")));
                    const Deviation d = ToleranceDB::deviation(
                        letters, size, grade, hole ? FitMember::Hole : FitMember::Shaft);
                    const QString stdNo = db.query(QStringLiteral("tolerances"),
                                                   QStringLiteral("_standard")).toString();
                    return QStringLiteral("Φ%1 %2%3（%4）基本偏差：上偏差 = %5 mm，下偏差 = %6 mm；"
                                          "极限尺寸 %7 ~ %8 mm（%9）。"
                                          "数据源：ToleranceDB::deviation + StandardsDB.query(tolerances, _standard)。")
                               .arg(QString::number(size, 'g', 8), letters, dm.captured(2),
                                    hole ? QStringLiteral("孔") : QStringLiteral("轴"),
                                    QString::number(d.upper, 'f', 6), QString::number(d.lower, 'f', 6),
                                    QString::number(ToleranceDB::lowerLimit(size, d), 'f', 6),
                                    QString::number(ToleranceDB::upperLimit(size, d), 'f', 6),
                                    stdNo);
                }
            }
        }
    }

    // ④ 其余高频规则：一律以 StandardsDB.query(table, key) 取数后原样返回，
    //    附上标准号，避免模型把摘要里的定性描述当成精确值。
    const QVector<RuleEntry> rules = buildRuleEntries();
    for (const RuleEntry& e : rules) {
        const QString kw = QString::fromUtf8(e.kwUtf8);
        if (!r.contains(kw, Qt::CaseInsensitive)) continue;
        const QString table = QString::fromLatin1(e.table);
        const QStringList keys = QString::fromLatin1(e.keysCsv).split(QLatin1Char(','));
        QStringList lines;
        for (const QString& key : keys) {
            const QJsonValue v = db.query(table, key);
            if (v.isUndefined() || v.isNull()) {
                lines.append(SketchCAD::I18n::t(QString::fromUtf8("  StandardsDB.query(%1, %2) = （该键未收录）")).arg(table, key));
            } else {
                lines.append(QStringLiteral("  StandardsDB.query(%1, %2) = %3")
                                 .arg(table, key, db.queryString(table, key)));
            }
        }
        const QString stdNo = db.query(table, QStringLiteral("_standard")).toString();
        return QStringLiteral("【%1】来自标准表 %2（%3）：\n%4\n"
                              "如需其它精确数值，请用 queryStandard 精确查询其它规则。")
                   .arg(kw, table, stdNo.isEmpty() ? QStringLiteral("未标注") : stdNo,
                        lines.join(QLatin1Char('\n')));
    }

    return QStringLiteral("未识别的规则「%1」。queryStandard 支持：M<公称直径> 粗牙螺距、"
                          "Φ<基本尺寸> IT<等级> 标准公差、<代号><等级> 配合偏差，"
                          "以及线宽/间隙/字高/幅面/比例/粗糙度/螺纹/焊缝/图层等高频规则；"
                          "也可用 query_standards(table, key) 直接查 12 类标准表。").arg(r);
}

QString NlAssistant::complianceReport(const QString& focus) const {
    if (!model_)
        return QStringLiteral("当前没有打开的图纸，无法运行合规检查。");

    // 与命令行 stdcheck / 合规面板完全同源：纯读遍历，零副作用、不执行命令。
    const QVector<ComplianceIssue> all = ComplianceChecker::run(model_);
    const QString f = focus.trimmed();
    const QVector<RuleMeta> metas = buildRuleMetas();

    QStringList lines;
    lines.append(SketchCAD::I18n::t(QString::fromUtf8("合规检查结果（ComplianceChecker::run，7 类 GB/T 规则；纯读，不修改图纸、不执行任何命令）：")));
    lines.append(SketchCAD::I18n::t(QString::fromUtf8("共发现 %1 个不合规项。")).arg(all.size()));

    int r1Count = 0;
    for (const ComplianceIssue& iss : all)
        if (iss.ruleId == QStringLiteral("R1_line_width_ratio")) ++r1Count;

    for (const RuleMeta& rm : metas) {
        const QString id = QString::fromLatin1(rm.id);
        const QString zh = QString::fromUtf8(rm.zhUtf8);
        const bool focusHit = f.isEmpty() || id.contains(f, Qt::CaseInsensitive) ||
                              zh.contains(f) || f.contains(id, Qt::CaseInsensitive) ||
                              f.contains(zh);
        if (!focusHit) continue;

        QVector<ComplianceIssue> mine;
        for (const ComplianceIssue& iss : all)
            if (iss.ruleId == id) mine.append(iss);
        lines.append(SketchCAD::I18n::t(QString::fromUtf8("[%1] %2：%3（%4 项）"))
                         .arg(id, zh, mine.isEmpty() ? SketchCAD::I18n::t(QString::fromUtf8("合规")) : SketchCAD::I18n::t(QString::fromUtf8("不合规")))
                         .arg(mine.size()));
        for (const ComplianceIssue& iss : mine)
            lines.append(SketchCAD::I18n::t(QString::fromUtf8("  - #%1 %2：%3（建议：%4）"))
                             .arg(QString::number(iss.elementId))
                             .arg(iss.severity, iss.message, iss.fixHint));
    }

    // 明确结论：无论模型是否传 focus，线宽这一高频问题都给出可直接引用的判定。
    lines.append(SketchCAD::I18n::t(QString::fromUtf8("结论：当前图纸线宽%1（R1_line_width_ratio，%2 项）。"))
                     .arg(r1Count == 0 ? SketchCAD::I18n::t(QString::fromUtf8("合规")) : SketchCAD::I18n::t(QString::fromUtf8("不合规")))
                     .arg(r1Count));
    if (f.isEmpty())
        lines.append(SketchCAD::I18n::t(QString::fromUtf8("总体：%1")).arg(all.isEmpty()
                         ? SketchCAD::I18n::t(QString::fromUtf8("全部 7 条规则均合规。"))
                         : SketchCAD::I18n::t(QString::fromUtf8("存在 %1 个不合规项，按上述建议修正。")).arg(all.size())));
    return lines.join(QLatin1Char('\n'));
}

} // namespace SketchCAD
