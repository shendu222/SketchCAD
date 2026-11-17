#pragma once
// =============================================================================
// I18n —— 界面语言单一入口（国际化基建）
//
// 职责：
//   1. resolveLanguage()：把用户设置（general/language）解析为可执行语言代码。
//      "" / 未知值 = 跟随系统（中文系统 → zh-CN 源语言；其他 → en）。
//   2. installTranslator()：装载/卸载 QTranslator（资源内 ：/i18n/sketchcad_en.qm，
//      回落 exe 同目录磁盘文件）。zh-CN 是源语言，不装翻译器（串直出）。
//   3. t()：运行时文本翻译层。源码里大量用户可见文案是 QStringLiteral("中文")
//      硬编码（不走 tr()），Qt 翻译器对此无能为力。t() 用「生成式词典」把这类
//      串在英文模式下换成英文；中文模式恒等返回，保证源语言零变化。
//
// 设计约束：
//   - 中文（zh-CN）为源语言：所有 tr() 源串即中文串；缺翻译自然回落中文源串。
//   - 切换即刻生效 = 卸旧装新翻译器 + 调用方重建可见界面（MainWindow 重建菜单栏）。
//   - 命令名/别名/JSON 键一律 ASCII，不经 tr()，翻译永不影响命令语义。
//   - t() 只在「用户可见文本出口」调用：菜单、标签、按钮、弹窗、状态、空态、
//     命令行提示/反馈、动态模板。字体族名、文件名、命令名、设置键、比较键等
//     内部字符串不进词典，t() 查不到时原样返回，因此不会改变任何行为。
//   - 词典由 tools/i18n/build_i18n.py 生成（I18nDict.cpp），逐条 add() 构建，
//     规避 MinGW GCC 13.1 巨型聚合初始化静默崩溃。
// =============================================================================

#include <QString>
#include <QStringList>

namespace SketchCAD {

namespace I18n {

// 把设置原始值解析为语言代码："zh-CN" / "en"。
//   * "zh-CN"、"en" 原样返回；
//   * 空 / 未知值 → 跟随系统：QLocale::system().name() 前缀 zh → "zh-CN"，否则 "en"。
QString resolveLanguage(const QString& setting);

// 当前已装载的解析后语言（resolveLanguage 的最近一次结果；初始 "zh-CN"）。
QString activeLanguage();

// 卸旧装新翻译器。返回 true = 有翻译器在生效（en）；false = 源语言直出（zh-CN）。
// 装载顺序：资源 ：/i18n/sketchcad_en.qm → exe 同目录 sketchcad_en.qm（测试/独立部署）。
bool installTranslator(const QString& resolvedLang);

// ---- 运行时文本翻译层（硬编码中文 → 英文）--------------------------------
// zh-CN：恒等返回入参（逐字不变，保证中文界面显示/布局/流程零变化）。
// en   ：① 精确词典命中 → 返回英文译文；
//        ② 未命中 → 术语级最长匹配清洗（覆盖动态拼接 / %1 模板 / 运行时
//           插入的中文术语），把残留的中文 UI 词换成英文；
//        ③ 仍无命中 → 原样返回（用户自定义内容、文件名、字体名等数据不属 UI 文案）。
QString t(const QString& zhText);

// 便捷重载：给 const char* 元数据表（CommandCatalog::add 等）用的出口。
QString tUtf8(const char* zhUtf8);

// 仅供自测：返回生成式词典的全部中文源串（用于英文模式「无中文残留」全量断言）。
QStringList dictionaryKeys();

// 仅供自测 / 工具：把一段任意文本按术语表做最长匹配清洗（不查精确词典）。
QString termScrub(const QString& text);

// 仅供自测：在不依赖 .qm 是否装载成功的情况下切换运行时语言。
// 生产代码只用 installTranslator()；此钩子让无头测试能稳定复现英文模式。
void setRuntimeLanguageForTest(const QString& resolvedLang);

} // namespace I18n

} // namespace SketchCAD
