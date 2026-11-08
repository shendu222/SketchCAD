#pragma once
#include "ai/LlmProvider.h"
#include <QVector>

namespace SketchCAD {
namespace NlTools {

// 向 LLM 暴露的工具 schema 集合（混合策略：少量查询工具 + 一个动作工具）。
// 动作入口统一为 runSketchCommand(line)，其余为按需取数的查询工具。
QVector<LlmTool> schemas();

} // namespace NlTools
} // namespace SketchCAD
