#include "ai/tooldefs.h"

namespace SketchCAD {
namespace NlTools {

QVector<LlmTool> schemas() {
    QVector<LlmTool> t;
    t.append({ QStringLiteral("runSketchCommand"),
        QStringLiteral("执行一条 SketchCAD 命令（自然语言助手的主要动作入口）。参数 line 为完整命令串，"
                       "语法与界面命令行完全一致，例如 \"circle 0,0 50\"、\"line 0,0 100,0\"、\"delete last\"。"
                       "不确定语法时先用 getCommandHelp 查询。"),
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
    return t;
}

} // namespace NlTools
} // namespace SketchCAD
