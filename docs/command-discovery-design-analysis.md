# 命令发现与容错帮助（Command Discovery）— 深思过程与设计分析

> 阶段五交付 · 2026-10-05
> 目标：让用户不用翻文档也能找到命令、纠正错误输入。
> 验收基线：**全量自测 1197/0（EXIT=0）**，双目标零警告；基线由 1167 → 1197（t_help 新增 30 条 CHECK）。

---

## 一、需求回顾

三项功能、三条硬边界：

| # | 功能 | 要点 |
|---|------|------|
| 1 | 命令面板 | Ctrl+Shift+P；模糊搜索中文名/英文名/别名；显示分类/快捷键/简介；Enter 执行；记录最近使用 |
| 2 | help 增强 | `help <cmd>` 输出用法/参数/样例/相关命令；未知命令给相近候选（复用 P2-03 的 nearestCommands） |
| 3 | 快捷键冲突自检 | 启动时扫描重复绑定，warn 日志输出 |

边界：不得修改任何既有命令的行为与名字；帮助文本不得影响命令解析；深思过程导出本文档。

## 二、现状勘察

- **CommandRegistry 无元数据**：`QMap<QString, shared_ptr<ICommand>>` 仅 name→命令对象；无分类/中文名/简介字段。`help <cmd>` 详情与面板都需要一个**独立的元数据目录**。
- **HelpCommand 旧实现**（Commands.cpp）：纯手写 38 行分类列表——与注册表**无任何联动**，命令补齐阶段新增的 17 个顶层命令全靠手记，迟早漂移。这正是「同一份语义在两处手工拼装」的又一实例。
- **可直接复用的既有资产**：
  - `CommandInterpreter::nearestCommands(cmd, names, maxCount=3)`（P2-03，编辑距离容错，公有静态）；
  - `CommandInterpreter::commandNames()`（注册表精确名 + line/spline/ellipse/polyline/select 五个家族子模式表）；
  - `AliasTable::instance()`（别名→完整命令；**构造即空**，GUI 启动由 aliasreload 注入——无头测试须手动灌 defaultAliases()，t_qselect/t_cmdParity 的已知陷阱）；
  - `SketchModel::logAppended(msg, type)` 信号——t_interaction IA3 已用它捕获日志做断言，help 输出断言直接同款。
- **快捷键来源盘点**：buildMenu 的 `mk` lambda（`setShortcutContext(ApplicationShortcut)`）、正交/捕捉 QShortcut（F8/F9）、Ctrl+Return/Ctrl+Tab/actChain_（K）等散点 setShortcut。冲突扫描须覆盖 **菜单 QAction（递归）+ findChildren\<QShortcut\*>** 两路。
- **最近使用持久化**：CommandInterpreter 已有 `commandLine/recentCommands`（QStringList + stringListVal 校验器）的完整模式，面板最近使用照抄该模式新增 `commandLine/recentPalette`。

## 三、方案设计

### 3.1 分层：纯逻辑进 core，薄壳进 ui

沿用项目惯例（AliasTable / DocSession / CommandLine 键盘语义均「纯逻辑可无头直测 + UI 薄壳」）：

```
src/core/CommandCatalog.{h,cpp}   ← 纯数据 + 纯逻辑（无 QtWidgets）
  ├─ CommandInfo / entries() / find() / categoryOf()   命令元数据目录
  ├─ PaletteEntry / CommandPaletteModel                 面板过滤/排序/最近使用
  └─ ShortcutAudit::duplicates()                        冲突审计纯函数

src/ui/CommandPalette.{h,cpp}     ← 对话框薄壳（QLineEdit + QListWidget）
src/core/Commands.cpp HelpCommand ← 重写：总表自动枚举 + <cmd> 详情
```

面板 UI 装配（buildEntries）放对话框静态方法：目录元数据 ∪ commandNames() 回落条目 ∪ 别名入口（alias 条目 name=解析后目标命令，回车直接执行）。

### 3.2 帮助文本与命令解析「同源而不同路」

- 目录表 `usage` 字段**逐条取自各命令实现的「用法:」错误提示串或 Commands.h 家族注释**——帮助文本与真实解析行为同源，不凭空编造；未收录命令不编造，走回落分支（名称/推断分类 + 「暂无详细帮助」提示）。
- HelpCommand 重写后：
  - `help`：按 `categoryOf` 分组自动枚举 `commandNames()` 全集 → **永不漏登记**；尾部一行「命令总数：N（别名 M 条，单列如下）」+ 别名单列 + 面板提示；
  - `help <cmd>`：先经 AliasTable 解析别名 → 目录命中输出 名称/分类/简介/用法（Usage:）/参数/样例/快捷键/相关命令；注册表命中但未收录走回落；彻底未知 → nearestCommands 候选（返回 false，与 execute 的未知命令行为一致）。
- 「帮助文本不影响命令解析」由结构保证：HelpCommand 仍按注册名 `help` 分派，帮助输出只是 `m->log`，不触碰任何解析路径。

### 3.3 面板模糊计分（确定性，可测）

三通道（name/title/alias，基分 100/90/80）× 三档匹配：完全相等 ×2 > 前缀 ×1.2 > 包含 ×1.0 > 子序列（基分 − 间隔×5，间隔 >8 判不匹配）。排序：得分降序、同分保持装配顺序（stable_sort 语义）。空查询 = 最近使用置顶 + 其余原序。`pushRecent` 去重置顶、≤10 条，经 AppSettings `commandLine/recentPalette` 持久化（与 recentCommands 同款 stringListVal）。

### 3.4 快捷键冲突自检

启动末尾（构造函数最后）`auditShortcutConflicts()`：菜单栏 QAction 递归收集 + `findChildren<QShortcut*>()` → `ShortcutAudit::duplicates()`（纯函数，快捷键小写规格化后查重）→ 每组冲突一条 `warn` 日志（含快捷键 + 双方动作名）。查重逻辑纯函数化，无头直测；MainWindow 只做采集与日志。

### 3.5 执行通路

面板 Enter → `interp_->execute(name)`——与命令行、菜单项**同一 handler**（阶段三确立的命令补齐原则）；成功才 `accept()` 关窗并写最近使用，失败保持面板打开可重试。

## 四、实现细节与踩坑

1. **⚠️ MinGW GCC 13.1 巨型聚合初始化静默崩溃（本次最大坑，最反直觉）**
   目录表初版写为 `static const QVector<CommandInfo> table = { 127 条嵌套花括号条目 }`。
   症状：`cc1plus` **EXIT=1 零诊断**（无 error/warning 任何输出），ninja 只回显 FAILED；`-fsyntax-only` 通过、O0 与 O3 同样死亡——与记忆中「PropertyPanel.cpp 无诊断失败=资源耗尽」判据不同，本次**前台手工复现同样失败**，排除沙箱资源因素。
   二分定位（python 花括号计数切条目逐数编译）：46 条通过、47 条起失败 → **非单条内容问题，是单条巨型初始化表达式的规模阈值**（非平凡类型嵌套花括号初始化，约 47+ 条触发 cc1plus 静默死亡）。
   根治：改为 **`add()` lambda 逐条 builder**（每条一个独立表达式 + `QString::fromUtf8`），编译即过、双档优化零警告。此坑已具备普适性：**大静态表一律 builder 化，不写巨型聚合初始化**。
2. **⚠️ 前向声明必须在命名空间内**（记忆已知陷阱再踩）：CommandPalette.h 最初把 `class AppSettings;` 写在 namespace 外 → 头文件签名是全局 `::AppSettings`，cpp 内 `SketchCAD::AppSettings` → "no declaration matches" + incomplete type。移入 namespace 即修。
3. `auto& groupOf = [lambda]` 不可编译（lambda 是纯右值，非 const 左值引用绑不上）→ 用 `auto`。
4. Commands.cpp 补 include：HelpCommand 用 `CommandInterpreter::commandNames()` 需完整定义（MacroScript.h 只有前向声明）→ 加 `#include "CommandInterpreter.h"`。
5. **后台构建 cwd 免疫**：`cmake --build` 一律绝对构建路径；另发现本会话 shell 的 `cd` 前缀会被环境剥离（多次复发），一律改绝对路径调用。
6. t_help 动态消息必须具名（`const QByteArray msg = ...toUtf8(); CHECK(cond, msg.constData())`）——`CHECK(cond, expr.toUtf8().constData())` 内联临时即悬垂 UB（记忆已知陷阱）。
7. AppSettings 消费遵循既有口径：`value(QString)` / `setValue(QString, QVariant)` 直调（与 CommandInterpreter 持久化同款），不新造 helper。

## 五、改动清单

| 文件 | 改动 |
|------|------|
| `src/core/CommandCatalog.h/.cpp` | **新增**：CommandInfo 目录（127 条 add() builder）+ categoryOf 前缀推断 + CommandPaletteModel（计分/过滤/最近使用）+ ShortcutAudit::duplicates |
| `src/core/Commands.cpp` | HelpCommand 重写（总表自动枚举 + `<cmd>` 详情/回落/候选）；include CommandCatalog/CommandInterpreter/AliasTable |
| `src/core/AppSettings.h/.cpp` | 新键 `commandLine/recentPalette`（stringListVal，默认空表） |
| `src/ui/CommandPalette.h/.cpp` | **新增**：命令面板对话框（过滤/↑↓/Enter 执行/Esc；最近使用读写） |
| `src/ui/MainWindow.h/.cpp` | `openCommandPalette()` + Ctrl+Shift+P QShortcut；`auditShortcutConflicts()`（构造末尾）；include CommandPalette/CommandCatalog/functional |
| `CMakeLists.txt` | CORE_SOURCES + CommandCatalog.cpp；UI_SOURCES + CommandPalette.cpp |
| `tests/SketchSelfTest.cpp` | 新增 t_help（30 条 CHECK）+ include CommandCatalog.h + main 注册 |

## 六、验收结果（t_help 30 条 CHECK）

- **P1 面板过滤**：中文名『直』命中 line、『圆』命中 circle、不相关输入空候选（3）
- **P2 别名通道**：别名 c→circle 得分为正；前缀匹配得分 > 子序列；阈值外判不匹配（3）
- **P3 最近使用**：空查询置顶序（move→circle→原序）；pushRecent 去重置顶（2）
- **H1 help 详情**：`help line` 成功、输出含「用法（Usage:）」、样例与实现同源（3）
- **H2 别名/回落**：`help h` 解析到 hatch；未收录的 poly_regular 经 poly_ 前缀命中走「暂无详细帮助」回落（4）
- **H3 未知容错**：`help circl` 失败 + 「您是不是想输入」含 circle（2）
- **H4 总数一致**：help 总表含别名单列段；**解析出的命令总数 == `commandNames().size()`（注册表+家族全集）**；别名计数 == AliasTable 条数；分组行逐名覆盖全集（5）
- **H5 冲突自检**：Ctrl+X 大小写规格化检出 1 组冲突且含双方动作名；无冲突返回空（4）
- **H6 目录一致性**：circle 收录、大小写不敏感、line 用法与实现同源、未收录家族按前缀推断分类（4）

回归：**通过 1197 / 失败 0，EXIT=0**；`grep -c warning` 双目标构建日志 = **0**；SketchCAD/SketchSelfTest 双目标 `-j1` 增量构建 EXIT=0。命令行为与名字零改动（仅 HelpCommand 的**输出内容**升级，注册名不变、返回值语义不变）。

## 七、遗留与后续

- **目录覆盖为「代表集 + 同源回落」**：127 条详细元数据覆盖全部高频命令；约 40 个家族子模式/低频命令走回落（显示名称+推断分类）。后续可在各命令落地时顺手 `add()` 一条（模式已固化，单条成本极低）。
- **冲突自检为启动一次性扫描**：语言重建（rebuildMenusForLanguage）会重建菜单但绑定不变，无需重扫；若未来支持用户自定义快捷键，需在应用配置变更时重扫一次。
- **菜单动作 ↔ 命令名的反查**：目录表的 shortcut 字段是静态展示数据（F1/F5/F8 等）；菜单 lambda 内嵌命令串无法反向解析，故面板快捷键展示以目录为准。若后续要强一致，可让 mk 额外登记 (命令名, 快捷键) 到目录。
- **英文界面**：help 输出与面板文案暂为中文源语言（与 i18n 阶段范围一致，.ts 已含 MainWindow/OptionsDialog 59 条）；命令面板对话框文案的 tr() 已就位，译文可随后补入 sketchcad_en.ts。
- build.sh test 默认并行在本会话沙箱会被 SIGTERM（历史已知）；本阶段以「双目标 -j1 构建 EXIT=0 + 自测 exe EXIT=0」等价验证。
