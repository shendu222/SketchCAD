#pragma once
// =============================================================================
// I18n —— 界面语言单一入口（国际化基建）
//
// 职责：
//   1. resolveLanguage()：把用户设置（general/language）解析为可执行语言代码。
//      "" / 未知值 = 跟随系统（中文系统 → zh-CN 源语言；其他 → en）。
//   2. installTranslator()：装载/卸载 QTranslator（资源内 ：/i18n/sketchcad_en.qm，
//      回落 exe 同目录磁盘文件）。zh-CN 是源语言，不装翻译器（串直出）。
//
// 设计约束：
//   - 中文（zh-CN）为源语言：所有 tr() 源串即中文串；缺翻译自然回落中文源串。
//   - 切换即刻生效 = 卸旧装新翻译器 + 调用方重建可见界面（MainWindow 重建菜单栏）。
//   - 命令名/别名/JSON 键一律 ASCII，不经 tr()，翻译永不影响命令语义。
// =============================================================================

#include <QString>

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

} // namespace I18n

} // namespace SketchCAD
