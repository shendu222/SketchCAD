#pragma once
#include <QString>

namespace SketchCAD {

// LLM 响应结构化解码结果。
// 注意：本结构只承载「模型输出」的解码结果，语义理解与命令串生成完全由
// LlmProvider 背后的模型完成；此处不参与任何自然语言理解。
struct NlParseResult {
    bool    ok           = false;   // 模型输出中是否含可执行的命令对象
    QString commandLine   = {};      // 模型给出的命令串，如 "circle 0,0 20"
    QString intent       = {};      // 意图标识（由命令名派生，仅用于展示）
    double  confidence   = 0.0;     // 模型自报置信度 0..1（缺省 1.0）
    QString explanation   = {};     // 模型自报的面向用户说明
};

// LLM 响应解码器（不再是自然语言映射器）。
//
// 主链路：NlAssistant → LlmProvider::chat() → 模型产出 runSketchCommand
// 工具调用 → interp_->execute()。命令串一律来自模型。
//
// 本类只处理一条兼容路径：部分本地模型/网关不支持 function calling，
// 会以纯文本 JSON 形式返回命令对象（{"command":"circle 0,0 20", ...}）。
// parse() 把这段【模型输出】解码为 NlParseResult，等价于一次工具调用。
// 输入是模型回复，不是用户自然语言；不做归一化、关键词匹配或参数抽取。
class NlCommandMapper {
public:
    static NlParseResult parse(const QString& modelOutput);

    // 历史阈值常量：保留以兼容既有调用方的编译依赖。
    // 主链路已不再据此绕过 LLM，任何输入都必须经过模型推理。
    static constexpr double kDirectExecMin = 0.6;
};

} // namespace SketchCAD
