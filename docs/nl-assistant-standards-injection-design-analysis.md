# AI 国标知识注入：设计与深思过程

> 模块：`src/ai/NlAssistant.{h,cpp}` / `src/ai/tooldefs.cpp` / `src/core/standards/StandardsDB.*`
> 日期：2026-11-13
> 验收：双目标 `-Wall -Wextra` 零警告；全量自测 **3406 / 0，EXIT=0**；`dist/SketchCAD.exe` 已同步。

---

## 1. 需求与硬约束

原始需求：

1. 构造时读取 `StandardsDB` 摘要，拼入 system prompt（**仅高频规则 + "用 queryStandard 精确查询"**）。
2. 向 `LlmProvider` 工具集注册 `queryStandard(rule)`（内部调 `StandardsDB.query`）与 `checkCompliance()`（调 `ComplianceChecker.run`）。
3. AI 对话经 `AiAssistantPanel → NlAssistant → CommandInterpreter::execute` 落地命令。
4. 验收三问：`M20 的螺距是多少`、`Φ30 IT7 公差多少`、`这张图线宽合规吗` 能经工具返回准确值；自测含 AI 工具调用断言；`EXIT=0`。
5. **不改动命令执行单一出口**。

第 5 条是本轮的宪法性约束。它的准确含义不是「禁止新增工具」，而是：

- `interp_->execute(line)` 在整个 `NlAssistant` 中**只能出现在 `runSketchCommand` 分支**（外加审批后的 `approvePending()`）；
- 新工具必须是**纯读**：不改模型、不写日志、不触发命令、不刷新面板；
- 不得为了「让三问返回准确值」而在本地塞一条绕过 LLM 的快捷路径。自然语言 → 工具调用的语义转换仍完全由模型负责，本地只做确定性的**数值/规则计算**。

---

## 2. 现状勘察：链路与数据契约

### 2.1 现有链路（未改动）

```
AiAssistantPanel::onSend()
  └─ emit userMessage(text)
       └─ MainWindow lambda → NlAssistant::sendUserMessage(text)
            └─ runTurn() → LlmProvider::chat(history_, NlTools::schemas(), ...)
                 └─ onToolCalls → processToolCalls()
                      ├─ runSketchCommand ──→ CommandInterpreter::execute(line)   ← 唯一动作出口
                      └─ 其它工具 ─────────→ runQueryTool()（纯读）               ← 本次扩展点
                 └─ Tool 结果回灌 → runTurn() → 最终文本（由模型生成）
```

`AiAssistantPanel` 只负责采集与展示，**不执行命令**；`MainWindow` 只做信号转接。因此把两个新工具放进 `runQueryTool()` 的 `else` 分支，天然满足「单一出口不变」。自测中验证编排层即可等价覆盖该链路（`AiAssistantPanel.cpp` 属 `UI_SOURCES`，不在 `SketchSelfTest` target 中，强行加入只会引入无谓的 QWidget 依赖）。

### 2.2 原有工具集（6 个）

`runSketchCommand` / `getSelection` / `listElements` / `getLayers` / `getCommandHelp` / `query_standards`。

### 2.3 关键矛盾：三问的答案根本不在同一处

这是本轮最需要想清楚的地方。验收三问看似都是「查国标」，实际数据真相分属三个完全不同的层：

| 问句 | 期望值 | 真相来源 | 原 12 张 JSON 表能不能给？ |
|---|---|---|---|
| `M20 的螺距是多少` | `2.5 mm` | GB/T 193 普通螺纹**粗牙直径-螺距系列** | ❌ `thread.json` 只有 `M="普通螺纹"`、`example_M="M20×2-6H"`（这是**细牙**标注示例），无任何螺距表 |
| `Φ30 IT7 公差多少` | `0.021 mm` | ISO 286-1 / GB/T 1800.1 公式，已实现于 `ToleranceDB::standardTolerance` | ❌ `tolerances.json` 只有 `IT_grades` / `size_segments` 等描述性文本 |
| `这张图线宽合规吗` | 取决于**当前模型** | `ComplianceChecker::run(model)` 的 R1 判定 | ❌ 静态国标表无法表达「当前图纸」 |

若机械地把 12 张 JSON 全塞进 system prompt，三个问题一个也答不准，还会把上下文撑爆。若把 `0.021`、`2.5` 硬编码进 C++，则制造了第二套数值真相，与已有的 `ToleranceDB` 公式实现分叉——这正是工程里最该避免的做法。

**结论：system prompt 只放「高频规则 + 查询指引」，精确值必须经工具在运行时求解，且每条答案都要标注数据来源。**

---

## 3. 方案取舍

### 决策一：摘要由 `query()` 派生，而不是新增第 13 张表

`StandardsDB::summary()` 只读 12 类表中已存在的**标量键**（`thick_thin_ratio` / `min_gap_mm` / `hanzi_min_height` / `_standard` / …），拼成约 600 字符的 9 行文本。好处：

- 不新增资源表 → `allTables().size() == 12` 的既有契约与资源字节探针零改动；
- 摘要与表数据同源，改 JSON 即改 prompt，不存在两份描述漂移；
- 体积受控（自测断言 `< 1800` 字符），不会挤占模型上下文。

摘要末行是硬性指令：

> 凡涉及具体数值（如 M20 粗牙螺距、Φ30 IT7 标准公差、某幅面留边值），一律用 `queryStandard` 精确查询，禁止凭记忆作答；判断「当前图纸是否符合规范」必须调用 `checkCompliance()`，不得凭空推断。

`buildSystemPrompt()` 既在构造时调用，也在每轮 `runTurn()` 刷新，保证多轮对话中指引不丢失。

### 决策二：`queryStandard` 先查 `StandardsDB.query`，数值再走公式

工具实现 `NlAssistant::queryStandardRule(const QString& rule) const`，四条分支：

1. **普通螺纹粗牙螺距**：正则抽 `M<公称直径>` → `db.query("thread", "coarse_pitch_M").toObject()` 取 `M20` → `2.5`。
   - 为此在 `thread.json` **只增键**（不增表）补了 `coarse_pitch_M`（M1~M64，GB/T 193-2003 粗牙系列）、`_pitch_standard`、`_coarse_pitch_note`。这是唯一新增的静态数值，因为它是离散的规格系列，没有可推导公式。
   - 返回文本明确区分粗牙与细牙：`M20×2` 表示细牙 P=2mm，未标 `×P` 默认取粗牙 2.5mm，避免把 `example_M` 的 `×2` 误当粗牙答案。
2. **标准公差**：正则抽 `IT<等级>` + 基本尺寸 → `ToleranceDB::standardTolerance(30, IT7)` 实时算 `0.021mm`（并给 `21 μm`）。标准号与计算口径仍从 `db.query("tolerances", ...)` 读取。
   - `tolerances.json` 只增 `standard_tolerance_mm` / `_engine_note` 两条**口径描述**，不复制任何 IT 数值。
   - `parseITGrade()` 处理 `IT01` 特例；`IT0~IT18` 与枚举的映射是 `序号 + 1`（`IT01=0, IT0=1, IT1=2 … IT18=19`）。
3. **配合偏差**（增强项）：`Φ30 H7 孔上下偏差` → `ToleranceDB::deviation(...)` + 极限尺寸。
4. **其余高频规则**：`buildRuleEntries()` 小表（builder lambda 逐条注册，规避 GCC 13.1 巨型聚合初始化静默崩溃）把中文关键字映射到 `(table, keysCsv)`，逐键 `db.query()` 取数并附标准号。

**所有分支都调用 `StandardsDB::query`**，满足需求 2 的字面约束；数值型分支额外复用既有公式，不制造第二份真相。未识别规则明确返回「未识别的规则 + 支持范围」，**绝不编造数值**。

> 中文关键字一律用 `const char*` UTF-8 字面量 + `QString::fromUtf8`，沿用本工程既有铁律（禁用 `QLatin1String(中文)`）。

### 决策三：`checkCompliance()` 与 `STDCHECK` 同源，但零副作用

`NlAssistant::complianceReport(focus)` 调 `ComplianceChecker::run(model_)`，把 `QVector<ComplianceIssue>` 按 7 条规则聚合成可读报告：

```
合规检查结果（ComplianceChecker::run，7 类 GB/T 规则；纯读…）：
共发现 1 个不合规项。
[R1_line_width_ratio] 线宽粗细比：不合规（1 项）
  - #2 警告：粗线(1.00mm)与细线(0.70mm)的线宽比为 1.43:1 …（建议：…）
结论：当前图纸线宽不合规（R1_line_width_ratio，1 项）。
```

- 可选 `rule` 参数聚焦（"线宽" / `R1_line_width_ratio`），省略则列全部 7 条；
- **不调用** `notifyComplianceReport()`、不写 `m->log()`：那条通道属于 `STDCHECK` 命令与 `StandardsPanel` 的 UI 刷新，AI 问答不应顺手改界面状态；
- 结论行显式输出「线宽合规 / 线宽不合规」，让模型无需自行解读 issue 列表即可引用。

### 决策四：不碰 `CommandInterpreter`、不碰面板

`runSketchCommand` 分支、`approvePending()`、`isDestructive()`、`CommandInterpreter::execute` 一行未改。`AiAssistantPanel` / `MainWindow` 也一行未改——链路本来就通，本轮只是**给模型多两件只读工具**。自测用 `scripted provider` 断言：两个新工具的 `commandExecuted` 为空、图元数不变；而 `runSketchCommand` 仍真实落模。

---

## 4. 自测设计

扩展 `ScriptedLlmProvider`：新增 `firstToolName` / `firstToolArgs` / `toolSchemaNames`，使第一回合可发出任意名字的工具调用，同时保留原 `firstToolLine` 用例零回归。新增 `runAiTool()` 辅助函数与 `t_aiStandards()`（50 条 CHECK），覆盖：

- `StandardsDB::summary()` 非空、含 `queryStandard` / `checkCompliance` / `2:1` / `0.7` / `3.5` / 标准号，体积 `<1800`，且不含整表 JSON；
- 新增数据键 `thread.coarse_pitch_M["M20"] == 2.5`，`allTables()` 仍为 12；
- 工具 schema 共 8 个（原 6 + 2），`runSketchCommand` 仍在；
- **三问逐条经工具返回**：`2.5`（含 GB/T 193 + 数据来源）、`0.021` / `21μm`（含 GB/T 1800 + `ToleranceDB` 来源）、`R1_line_width_ratio` + `线宽不合规` + 实测 `1.43`；
- 合规对照组（1.0 / 0.5 = 2:1）返回「线宽合规」，且无 focus 时列出全部 7 条规则；
- 通用规则查询、未知规则兜底；
- **单一出口回归**：两个新工具 `executed.isEmpty()`、`elementCount()` 不变；`runSketchCommand` 仍执行并落模；
- system prompt 注入摘要、工具结果回灌触发第二轮推理。

---

## 5. 踩坑与修复（本轮真实调试记录）

### 5.1 假阳性断言掩盖了真实缺陷

首轮自测 `3403 / 失败 2`，唯一实质失败是「明细含实测线宽比 `1.43`」。而相邻的「不合规」断言**却通过了**——因为报告首行固定文案「共发现 N 个不合规项」本身含「不合规」三字，即便 N=0 也会命中。教训：中文子串断言必须选**带主语的完整短语**。已改为断言 `线宽不合规`，并补一条「直接调用检查器也应报 R1」的断言来区分「工具坏了」还是「测试数据没触发」。

### 5.2 `printf` 风格占位符在 `QString::arg` 中静默失效 ⭐

打印工具返回原文后看到：

```
- #2 警告：粗线(%.2fmm)与细线(%.2fmm)的线宽比为 %.2f:1，不符合 …
```

根因：`ComplianceChecker.cpp` 里 R1/R2/R3 的 `iss.message` 用了 `%.2f` / `%.3f` / `%.1f` 这类 **printf 占位符**，而 Qt 的 `QString::arg()` 只认 `%1/%2/...` 序号占位符。结果是数值**一个都没替换**，日志、`StandardsPanel`、AI 工具结果全部显示字面量，实测比值丢失。这是上一轮合规检查器遗留的显示缺陷，旧自测只断言 `ruleId` 存在，因此一直没暴露。

修复（三处）：

```diff
- "粗线(%.2fmm)与细线(%.2fmm)的线宽比为 %.2f:1，…"
+ "粗线(%1mm)与细线(%2mm)的线宽比为 %3:1，…"
- "图元 #%1 与 #%2 的最小间隙为 %.3fmm，…"
+ "图元 #%1 与 #%2 的最小间隙为 %3mm，…"
- "图元 #%1 字号 %.2fmm 低于 … 下限 %.1fmm（…）。"
+ "图元 #%1 字号 %2mm 低于 … 下限 %3mm（…）。"
```

`.arg(..., 0, 'f', N)` 的格式控制保持不变。这条修复让 STDCHECK 命令与合规面板也一并受益。

### 5.3 其它防御

- `QVector<RuleEntry>` / `QVector<RuleMeta>` 一律 builder lambda + 显式 `RuleEntry{...}` 聚合，规避 GCC 13.1 巨型静态表静默崩溃；
- `iss.elementId` 先用 `QString::number()` 转换再进 `.arg()`，避免 variadic `arg` 对 `long` 的重载歧义；
- 螺纹正则用 `(?=$|[^0-9A-Za-z.])` 收尾，防止把 `M20×2` 的细牙标注误判为粗牙查询；`IT7` 分支先移除匹配串再找尺寸数字，避免把等级号 `7` 当基本尺寸。

---

## 6. 验收结果

| 项 | 结果 |
|---|---|
| 构建 | `cmake --build build --target SketchCAD SketchSelfTest -j 1`，27 + 9 个 ninja 步骤全过，`-Wall -Wextra` **零 warning / 零 error** |
| 自测 | `QT_QPA_PLATFORM=offscreen ./build/SketchSelfTest.exe` → **通过 3406 / 失败 0，EXIT=0** |
| 新增用例 | `t_aiStandards()` **通过 51 / 失败 0**（含「直接调用检查器也应报 R1」断言） |
| 回归 | `t_nlAssistant` 7/0、`t_llmIntegration` 23/0、`t_compliance`、`t_toleranceDB` 等全部保持全绿 |
| dist | `cp build/SketchCAD.exe dist/SketchCAD.exe`，`cmp` 字节一致；dist mtime `15:43:51` > 源码最晚 `15:40:04` |

---

## 7. 已知限制与后续

- 粗牙螺距表目前收录 M1~M64 常用规格；梯形/管螺纹、细牙全系列未收录，工具会明确返回「不在内嵌粗牙螺距表内」，并提示改用 `query_standards`。
- `queryStandard` 的规则路由是**确定性关键字匹配**（不是第二次 LLM 调用），因此依赖模型把自然语言规整成含关键 token 的 `rule` 短语；system prompt 与工具描述里已给出完整示例。未知规则一律兜底，不编数。
- `checkCompliance()` 不刷新 `StandardsPanel`：需要面板落地的场景仍走 `STDCHECK` / `checkcompliance` 命令，保持命令与 UI 的既有单一通道。
- 多文档切换时 `NlAssistant::setModel()` 会把 `complianceReport` 自动指向活动模型，无需额外接线。

---

## 8. 变更文件清单

| 文件 | 变更 |
|---|---|
| `src/core/standards/data/thread.json` | 只增 `_pitch_standard` / `_coarse_pitch_note` / `coarse_pitch_M`（M1~M64 粗牙螺距表） |
| `src/core/standards/data/tolerances.json` | 只增 `standard_tolerance_mm` / `_engine_note` 口径描述（不复制数值） |
| `src/core/standards/StandardsDB.h` | 声明 `QString summary() const` |
| `src/core/standards/StandardsDB.cpp` | 实现 `summary()`：9 行高频规则摘要 + 精确查询指引，全部经 `query()` 派生 |
| `src/ai/tooldefs.cpp` | 新增 `queryStandard(rule)` 与 `checkCompliance()` 两个工具 schema |
| `src/ai/NlAssistant.h` | 新增两个 const private 方法声明 |
| `src/ai/NlAssistant.cpp` | system prompt 注入摘要与工具指引；`runQueryTool` 两个只读分支；`parseITGrade` / `buildRuleEntries` / `buildRuleMetas` helper；`queryStandardRule` / `complianceReport` 实现 |
| `src/core/standards/ComplianceChecker.cpp` | 修复 R1/R2/R3 的 printf 风格占位符（真实显示缺陷） |
| `tests/SketchSelfTest.cpp` | `ScriptedLlmProvider` 支持通用工具脚本；新增 `runAiTool()` + `t_aiStandards()` 并注册 |
| `docs/nl-assistant-standards-injection-design-analysis.md` | 本文档 |
| `dist/SketchCAD.exe` | 同步构建产物 |
