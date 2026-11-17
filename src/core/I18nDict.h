#pragma once
// =============================================================================
// I18nDict —— 生成式「中文 UI 文案 → 英文」词典接口
//
// ⚠️ 本头文件只放声明；实现 I18nDict.cpp 由 tools/i18n/build_i18n.py 生成，
//    请勿手工编辑生成文件。词典内容来源：
//      * tools/i18n/en_phrases.txt —— 逐条人工校准的高可见度整句
//      * tools/i18n/en_glossary.txt —— CAD/UI 术语表（最长匹配清洗）
//      * 源码中「用户可见文本出口」的中文字符串字面量（自动抽取）
// =============================================================================

#include <QHash>
#include <QString>

namespace SketchCAD {
namespace I18n {
namespace Runtime {

// 精确映射：中文源串 → 英文译文。
const QHash<QString, QString>& exactMap();

// 术语映射：中文词/短语 → 英文词/短语（用于动态文本的兜底清洗）。
const QHash<QString, QString>& termMap();

// termMap 中最长的中文键长度（字符数，最长匹配扫描上界）。
int maxTermLength();

} // namespace Runtime
} // namespace I18n
} // namespace SketchCAD
