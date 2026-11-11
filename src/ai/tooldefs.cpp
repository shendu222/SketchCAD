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
    return t;
}

} // namespace NlTools
} // namespace SketchCAD
