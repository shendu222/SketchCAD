#include "ai/LlmProvider.h"

// LlmProvider 是含 Q_OBJECT 的抽象基类（仅有头文件定义）。为使其 moc 生成的
// vtable / staticMetaObject / qt_metacast / qt_metacall 有落脚的翻译单元，
// 在此显式包含由 AUTOMOC 生成的 moc_LlmProvider.cpp（标准头文件 QObject 模式）。
#include "moc_LlmProvider.cpp"
