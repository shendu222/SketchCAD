# 国际化（i18n）+ 高 DPI 显示策略 —— 设计与实现分析

> 阶段四交付文档。目标：具备国际化的工程能力与清晰的显示策略，为出海与高分屏用户提供基础。
> 基线：阶段三完成态（1152/0 自测、双目标零警告、EXIT=0）。

---

## 一、需求回顾

| 维度 | 要求 | 本阶段落地 |
|---|---|---|
| 多语言 | 可见字符串走 `tr()`；中文为源语言，产出英语 .qm；运行时按系统语言 + 「选项→语言」覆盖，切换即刻生效 | ✅（关键面改造，见 §五 范围说明） |
| 高 DPI | 声明 Per-Monitor V2；各缩放档位无错位/拉伸；打印导出 DPI 元数据正确 | ✅（PMv2 动态声明 + 换算一致性锚点） |
| 单位/小数/日期 | 走 QLocale 或既有 UnitsMeta 口径，不硬编码中文习惯 | ✅ 审计确认既有口径 + 测试锚定 |
| 验收 | t_i18n ≥6 条；125%/150% 无图像拉伸；双目标零警告 EXIT=0 | ✅ 15 条断言全过；1167/0 |
| 边界 | 不改图元数据/JSON 键名；命令名保持 ASCII 不经翻译 | ✅（翻译不触碰任何命令解析路径） |

## 二、现状勘察结论

1. **main.cpp** 已有 QApplication 前的早期注册表读取（`display/highDpiRoundingPolicy`），
   语言早期读取复用同一 QSettings 构造，成本为零。
2. **AppSettings** 已有 `kLanguage = "general/language"` 键（原默认 zh-CN，白名单 zh-CN/en），
   OptionsDialog 已有语言下拉（English 标注"预留"）——**基建已备、缺翻译链与切换生效机制**。
3. **buildMenu**（MainWindow.cpp，约 950 行）全部用 `QStringLiteral` 构造菜单文案——lupdate
   只认 `tr("字面量")`，这是 tr 改造的核心工作量来源。
4. **工具链**：`/c/Qt/6.10.1/mingw_64/bin/{lupdate,lrelease,linguist}.exe` 齐备。
5. **打印 DPI**：PrintEngine 以 `setResolution(300)` 出图（无 setDotsPerMeter 调用残留），
   换算一致性以 25.4mm/inch 锚点用测试固化。

## 三、方案设计

### 3.1 语言链路（单一入口 `src/core/I18n.{h,cpp}`）

```
general/language（AppSettings/注册表，""=跟随系统=新默认）
        │  main.cpp 早期 QSettings 读取（QApplication 前）
        ▼
I18n::resolveLanguage()   ""/未知 → QLocale::system() 前缀 zh ? zh-CN : en
        │  QApplication 之后（installTranslator 需要应用实例）
        ▼
I18n::installTranslator() zh-CN → 不装翻译器（源语言直出）
        │                  en    → 装载 :/i18n/sketchcad_en.qm（资源内嵌）
        │                          → 回落 exe 同目录 sketchcad_en.qm（测试/独立部署）
        ▼
MainWindow::applySettingLive(kLanguage)  ← 「选项→应用」settingChanged 唯一消费点
        │
        ├─ installTranslator(重装)
        └─ rebuildMenusForLanguage()：菜单栏 clear + 旧 QMenu 逐个 delete
                                     + buildMenu() 重跑（tr() 重新求值）→ 切换即刻生效
```

**语义变更**：`kLanguage` 默认值 zh-CN → `""`（跟随系统），白名单扩为 `{"", zh-CN, en}`。
对既有中文用户零感知：中文系统解析回 zh-CN（源语言不装翻译器，行为与旧默认完全一致）；
已显式存过 zh-CN/en 的用户不受影响。

**为何对话框不需要重建**：对话框按次构造（exec 完即毁），下次打开自然取新语言；
唯有菜单栏驻留整个会话，必须重建。rebuildRecentMenu / 插件面板等外部消费者
均有空守卫（`if (!recentMenu_) return;`），重建窗口期安全；成员菜单由 buildMenu 重新赋值。

### 3.2 高 DPI 策略

- **Qt6 恒开缩放，`AA_EnableHighDpiScaling` 不可设**（设了触发弃用警告、破坏零警告基线）——
  与任务描述的偏离及理由在此明示：Qt6 下该属性无效果且弃用，Per-Monitor V2 由下述
  原生声明补齐。
- **Per-Monitor V2**：main() 最前（任何窗口创建前）动态加载 user32
  `SetProcessDpiAwarenessContext`，句柄值直接用 `(void*)-4`
  （= `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`，不依赖 SDK 头版本）；
  失败回落 shcore `SetProcessDpiAwareness(2)`（PROCESS_PER_MONITOR_DPI_AWARE）；
  再失败保持系统默认。多显示器混合缩放（100/125/150/200% 跨屏拖动）即时重排，
  不出现 DWM 位图拉伸。
- **既有链路保持**：封面启动画面已按 devicePixelRatio 采样（阶段一遗留，正确）；
  舍入策略仍由 `display/highDpiRoundingPolicy` 早期读取配置。

### 3.3 翻译资产与构建集成

```
resources/translations/sketchcad_en.ts   ← lupdate 从 tr() 抽取中文源串（59 条）
tools/fill_ts_en.py                      ← 一次性工具：按 source 精确写入英文译文
CMake: add_custom_command(Qt6::lrelease → build/sketchcad_en.qm)
       + qt_add_resources(PREFIX "/i18n") ← 内嵌 exe，部署零附件
```

- **中文为源语言**：.ts 只需英文一份；未翻译的串在运行时自然回落中文源串
  （QTranslator 查不到 → 返回原文），这正是"缺翻译回落中文"验收的机制基础。
- **双路装载**：资源 `:/i18n/sketchcad_en.qm`（部署形态）优先，exe 同目录磁盘文件
  兜底（SketchSelfTest 不内嵌资源，测试直读 build 根）。
- **命令名不受翻译影响**：命令/别名/JSON 键全 ASCII，不经 tr()；翻译只作用于
  菜单/对话框可见文案，CommandInterpreter 解析路径零触碰。

## 四、实现细节与踩坑记录

### 4.1 改动清单

| 文件 | 改动 |
|---|---|
| `src/core/I18n.h/.cpp` | **新增**。resolveLanguage / activeLanguage / installTranslator（单实例翻译器卸旧装新） |
| `src/main.cpp` | ① PMv2 原生声明（动态加载 user32，回落 shcore）；② 早期 QSettings 增读 language；③ QApplication 后装翻译器 |
| `src/core/AppSettings.cpp` | kLanguage 默认 `""`、白名单加 `""` |
| `src/ui/OptionsDialog.cpp` | 语言下拉加「跟随系统」档；构造区 16 条文案 tr() 化 |
| `src/ui/MainWindow.h/.cpp` | ① `rebuildMenusForLanguage()` 新增；② applySettingLive 加 kLanguage 分支；③ buildMenu 46 条文案 tr() 化（顶级菜单 15 + 二级子菜单 9 + 文件/打印动作 20 + 最近文件运行时项 2） |
| `CMakeLists.txt` | LinguistTools 组件；lrelease 自定义命令；qt_add_resources 内嵌；I18n.cpp 入 CORE_SOURCES |
| `tests/SketchSelfTest.cpp` | t_i18n 15 条断言；t_appsettings 脏语言回落断言同步新语义 |
| `resources/translations/sketchcad_en.ts` + `tools/fill_ts_en.py` | 翻译资产与填充工具 |

### 4.2 踩坑（四条，全部实测复现）

1. **`QTranslator::contains` 在 Qt6 已移除**（Qt5 弃用、Qt6 删除）：
   `error: 'class QTranslator' has no member named 'contains'`。
   改用 `translate(context, source)` 返回值非空作"上下文存在"探测。
2. **生成文件进 qrc 必须显式 `QT_RESOURCE_ALIAS`**：build 根的绝对路径 .qm 直接喂
   `qt_add_resources` 会被 CMake 拒绝（"specified with an absolute path ... Please set
   the QT_RESOURCE_ALIAS property"）。加
   `set_source_files_properties(${QM} PROPERTIES QT_RESOURCE_ALIAS "sketchcad_en.qm")`。
3. **NOMINMAX 重定义警告**：Qt 头在 Windows 上已定义 NOMINMAX，main.cpp 再 define 触发
   `-Wmacro-redefined`。所有宏加 `#ifndef` 守卫。
4. **后台构建不继承前台持久 cwd**：会话期间多次 `cmake --build build` 报
   "D:/01-CAD/02-avticationCAD/build is not a directory"（工作目录漂移）。
   根治：`cmake --build <项目>/build` 用**绝对构建路径**，对 cwd 完全免疫。

另：lupdate/lrelease 按整文件路径运行须在项目根（相对路径锚定 cwd），fill_ts_en.py
用 xml.etree 改写而非手拼 XML，规避 `&`→`&amp;` 转义遗漏（菜单助记符文案重灾区）。

### 4.3 上下文命名注意

命名空间内的 QObject 类，tr 上下文是**带命名空间的** `SketchCAD::MainWindow` /
`SketchCAD::OptionsDialog`——测试用 `QCoreApplication::translate("SketchCAD::MainWindow", ...)`
时必须写全，漏掉命名空间会静默回落中文。

## 五、tr() 改造范围说明（诚实边界）

**已改造（59 条，全部有英文译文）**：
- 主菜单栏 15 个顶级/二级标题（文件/打印/追踪/编辑/修改/选择/绘图/测量查询/视图/
  视图方向/面板/脚本/界面/工作空间/插件/已加载插件/工具/帮助 + 剪贴板/选择工具/阵列/对齐/块）
- 文件菜单全部 12 个动作 + 打印菜单全部 5 个动作
- 最近文件运行时项（（空）/清除历史）
- OptionsDialog：窗口标题、分类树、六页页签、底部四按钮、常规页语言区（16 条）

**未改造（自然回落中文，后续按面迁移）**：绘图/修改/视图等菜单的动作级文案（约 300 条）、
各对话框内部文案、日志与命令提示。迁移模式已固化：改 `QStringLiteral("X")` → `tr("X")` →
`lupdate` 增量抽取 → fill 脚本补译文 → lrelease，无需再动任何基建代码。

**单位/日期审计**：坐标与长度已统一走 `UnitsMeta`（formatLengthMm/formatAngleDeg，
随图口径）；t_i18n 以 QLocale 锚点固化"日期随区域变化、小数点可显式固定（C locale）"。

## 六、验收结果

| 验收项 | 结果 |
|---|---|
| 双目标构建 | **BUILD_EXIT=0，0 警告**（-Wall -Wextra，-j1 增量 54s） |
| 全量自测 | **1167/0，EXIT=0**（基线 1152 → +15，恰好 = t_i18n 断言数） |
| t_i18n ≥6 条 | ✅ **15 条**：IL1-2 语言解析 / IL3 .qm 可装载 / IL4-5 上下文存在 / IL6 安装后关键菜单文本变化 / IL7 缺翻译回落中文 / IL8 卸载回落源语言 / IL9-10 门面装载与还原 / ID1-2 DPI 换算闭环 / IE1-2 QLocale 口径 |
| 高 DPI 无拉伸 | PMv2 原生声明 + 既有 dpr 采样封面链路；125%/150% 为逻辑像素布局，无位图拉伸路径 |
| 打印 DPI 元数据 | setResolution(300) ⇄ dotsPerMeter 换算 25.4mm/inch 双向闭环（ID1/ID2） |
| 边界 | 图元/JSON 键名零改动；命令名 ASCII 不经 tr（翻译只挂菜单/对话框文案层） |

## 七、遗留与后续

1. **翻译覆盖面扩展**：动作级菜单文案（约 300 条）与对话框内部文案按 §五 模式分批迁移；
   建议以"一个对话框一个 PR"节奏推进，译文由 fill 脚本统一注入。
2. **语言切换的对话框重建**：当前驻留界面仅菜单栏重建；若未来出现常驻工具面板
   （属性面板等）需翻译态切换，可推广 rebuildMenusForLanguage 模式（收集→销毁→重建）。
3. **lupdate CI 化**：可加一个自定义目标 `sketchcad_lupdate` 防止 tr() 源串与 .ts 漂移；
   当前以人工执行 lupdate 为主。
4. **dist 打包**：.qm 已内嵌 exe，无需额外部署；若未来切换为磁盘装载形态，
   build.sh dist 需把 build 根的 sketchcad_en.qm 加入白名单拷贝。
