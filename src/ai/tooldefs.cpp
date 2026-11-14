#include "ai/tooldefs.h"

namespace SketchCAD {
namespace NlTools {

QVector<LlmTool> schemas() {
    QVector<LlmTool> t;
    t.append({ QStringLiteral("runSketchCommand"),
        QStringLiteral("执行一条 SketchCAD 命令（自然语言助手的主要动作入口）。参数 line 为完整命令串，"
                       "由你根据用户输入理解语义、抽取参数后生成，语法与界面命令行完全一致，"
                       "例如 \"circle 0,0 50\"、\"line 0,0 100,0\"、\"delete 7\"。"
                       "自然语言参数抽取示例：「给我绘制一个圆心在0，0、半径为20的圆」→ \"circle 0,0 20\"。"
                       "命令名与参数顺序必须严格遵守 system prompt 中的 usage；不确定时先用 getCommandHelp 查询。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{\"line\":{\"type\":\"string\","
                       "\"description\":\"完整命令串\"}},\"required\":[\"line\"]}") });

    t.append({ QStringLiteral("getSelection"),
        QStringLiteral("返回当前选择集的摘要（图元类型与数量）。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{}}") });

    t.append({ QStringLiteral("listElements"),
        QStringLiteral("列出当前图形中所有图元的类型与数量（大型图纸慎用，可优先用 getSelection）。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{}}") });

    t.append({ QStringLiteral("getLayers"),
        QStringLiteral("返回当前图形中的图层列表。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{}}") });

    t.append({ QStringLiteral("getCommandHelp"),
        QStringLiteral("查询某条命令的用法与示例（name 为命令名，如 circle）。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\","
                       "\"description\":\"命令名（小写）\"}},\"required\":[\"name\"]}") });

    t.append({ QStringLiteral("query_standards"),
        QStringLiteral("查询内嵌的机械制图国标知识库（数据源自《机械制图手册（第四版）》）。"
                       "table 为标准表名（12 类之一：sheet_sizes / scales / fonts / line_types / roughness / "
                       "tolerances / geo_tol / thread / gear / bearing / weld / cad_layers）；"
                       "给定 key 时返回该键的值，省略 key 时返回该表全部键值。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{"
                       "\"table\":{\"type\":\"string\",\"description\":\"标准表名（12 类之一）\"},"
                       "\"key\":{\"type\":\"string\",\"description\":\"键名；省略则返回该表全部键值\"}},"
                       "\"required\":[\"table\"]}") });

    // 面向国标问答的精确查询工具：rule 为自然语言规则短语，内部一律先经
    // StandardsDB::query 取数；数值型规则再复用 ToleranceDB 的 ISO 286-1 公式，
    // 绝不复制常数表、绝不凭模型记忆给数。
    t.append({ QStringLiteral("queryStandard"),
        QStringLiteral("按规则精确查询机械制图国标数值/条文，返回带标准号与数据来源的确定答案。"
                       "任何涉及具体数字的标准问题都必须调用本工具，禁止凭记忆作答。"
                       "rule 为自然语言短语，例如「M20 粗牙螺距」「Φ30 IT7 标准公差」"
                       "「H7 孔上下偏差」「线宽粗细比」「最小间隙」「汉字最小字高」"
                       "「A3 幅面」「Ra 优先系列」。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{"
                       "\"rule\":{\"type\":\"string\",\"description\":\"要精确查询的国标规则/数值短语\"}},"
                       "\"required\":[\"rule\"]}") });

    // 面向「当前图纸是否合规」的只读检查工具：直接调 ComplianceChecker::run(model)，
    // 与命令行 stdcheck / 合规面板同源。纯读、不写模型、不执行任何命令。
    t.append({ QStringLiteral("checkCompliance"),
        QStringLiteral("对当前打开的图纸运行工程制图合规检查（ComplianceChecker::run，7 类 GB/T 规则），"
                       "返回逐规则结论与不合规明细。问「这张图线宽合规吗」「图纸是否符合国标」时必须调用。"
                       "可选 rule 聚焦某条规则（如「线宽」或 R1_line_width_ratio）；省略返回全部 7 条。"
                       "纯读、零副作用、不会执行任何绘图命令。"),
        QStringLiteral("{\"type\":\"object\",\"properties\":{"
                       "\"rule\":{\"type\":\"string\",\"description\":\"可选：聚焦的规则名或编号；省略则检查全部规则\"}}}") });
    return t;
}

} // namespace NlTools
} // namespace SketchCAD
