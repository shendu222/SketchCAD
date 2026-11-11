# SketchCAD 自然语言助手（LLM 嵌入）设计方案

> 状态：设计已讨论确定；**P0–P2 已实现并通过验收**（2026-11-08），P3–P5 待续
> 讨论结论：云端+本地可切换 / 混合工具（少量查询工具 + 单一动作工具）/ 破坏性命令需审批 / 首版仅「LLM 生成命令 + 对话 UI」
> 目标：让用户用自然语言操作 SketchCAD（绘图 / 修改 / 查询 / 标注 / 约束 / 图层 / 块 / 视图 …），
> 复用现有命令体系，不重新发明命令。

---

## 1. 目标与范围

**目标**
- 用户在对话面板输入自然语言（如"画一个圆心(0,0)、半径50的圆"），软件执行对应操作并反馈。
- 覆盖现有命令能力，而非另起一套动作系统。

**复用底座（已存在，关键）**
- `CommandInterpreter::execute(const QString& line)` —— 所有菜单/CLI/面板动作的最终执行入口。
  `MainWindow` 持有 `interp_` 并大量调用 `interp_->execute(...)`，自然语言助手复用同一实例、同一路径。
- `CommandCatalog::entries()` —— 127+ 命令的元数据（`name / zhName / category / summary / usage / params / example / related`）。
  可直接转为 LLM 的"工具定义"与 system prompt 语料。
- `CommandInterpreter::commandNames()` —— 全部命令名（含别名家族），用于工具/补全。
- `ScriptHost::runCode(lang, code)` —— 已有 JS/Python/VBA 脚本引擎，未来可作"生成脚本"备选路线。
- 撤销系统（Memento/undo 栈）—— NL 产生的改动与菜单操作同一条撤销链，用户可 Ctrl+Z 回退。

**非目标（首版）**
- 不重写命令解析；不替代现有 CLI / 脚本；不追求"理解图纸语义做设计决策"（那是更远的事）。

---

## 2. 整体架构

```
┌──────────────────────────────────────────────────────────┐
│  AiAssistantPanel (UI, 停靠面板)                           │
│   对话流 / 流式显示 / 命令卡片(执行·取消) / 停止            │
└───────────────┬──────────────────────────────────────────┘
                │ 用户消息 / 审批结果
                ▼
┌──────────────────────────────────────────────────────────┐
│  NlAssistant (编排层, 主线程持有)                          │
│   维护对话历史 · 构造 system prompt · 工具调用循环          │
│   LLM ⇄ 工具 ⇄ 执行 ⇄ 反馈                                 │
└───────┬───────────────────────────┬──────────────────────┘
        │ 网络(worker线程)            │ 工具执行(主线程)
        ▼                            ▼
┌────────────────────┐   ┌──────────────────────────────────┐
│  LlmProvider(抽象) │   │  Tools (tooldefs)                 │
│  OpenAiProvider    │   │  runSketchCommand(line)           │
│  MockProvider      │   │  getSelection / listElements /     │
│  (可扩展 Local)    │   │  getLayers / getCommandHelp(name)  │
└────────────────────┘   └──────────────┬───────────────────┘
                                         │ 命令串
                                         ▼
                              interp_->execute(line)  ← 复用现有命令体系
                                         │
                                         ▼
                              SketchModel / 视图 / 撤销栈
```

- **网络与 JSON 解析**在 worker 线程，避免阻塞 UI。
- **命令真正执行**（`interp_->execute`）必须在主线程（改模型/视图/UI）。
- **失败闭环**：`execute` 返回失败或"Did you mean?" → 把错误信息回灌 LLM 让其重试。

---

## 3. 关键设计决策（已讨论确定）

### 3.1 后端形态：云端 vs 本地

统一走 **OpenAI 兼容协议**（`/v1/chat/completions` + SSE 流式）。一个 Provider 实现即可覆盖：

| 形态 | 接入方式 | 优点 | 缺点 |
|------|----------|------|------|
| 云端 | base_url + api_key + model（DeepSeek / 通义千问 / 智谱 GLM / OpenAI） | 质量好、首版最快 | 需联网、密钥 |
| 本地 | Ollama / llama.cpp server（同一协议，base_url=本地端口，无需 key） | 隐私/离线 | 小模型 function-calling 较弱 |

**结论（已确认）**：Provider 无关 + 配置化，**云端与本地都支持、可切换**。不绑定具体厂商，用
`baseUrl + apiKey + model` 三元组配置；`cloud` / `local` 共用 `OpenAiProvider`（仅 base_url 不同），
另设 `mock` 离线规则后端用于无 key 联调与无头自测。默认 `cloud`（base_url = `https://api.openai.com/v1`），
配置页可随时切换，切换时热替换 Provider 而不重建对话历史。

### 3.2 自然语言 → 动作的表示（三种路线）

- **A. 单一 `runSketchCommand(line)` 工具**：LLM 直接产出命令串（如 `circle 0,0 50`），由 `interp_->execute` 执行。
  最简单、100% 复用现有语法；依赖 LLM 写对语法（用 `CommandCatalog` 的 usage/example 喂给它）。
- **B. 每命令一个 function-call 工具（结构化参数）**：最严谨、参数可校验；但 127+ 工具过多、上下文占用大、选型困难。
- **C. 脚本生成（ScriptHost::runCode）**：灵活但难校验、安全风险高。

**推荐（混合）**：少量"查询工具" + 一个"动作工具"：
- 查询工具：`getSelection` / `listElements` / `getLayers` / `getViewExtents` / `getCommandHelp(name)`（按需取用法，避免把整图塞进 prompt）。
- 动作工具：`runSketchCommand(line)` —— LLM 写命令串执行。
- 需要某命令语法时，LLM 调 `getCommandHelp(name)` 取用法后再写，从而写对。
工具少、稳健、完整复用。
**结论（已确认）**：采用混合路线。实际落地 5 个工具（`tooldefs.cpp`）：
`runSketchCommand(line)` / `getSelection` / `listElements` / `getLayers` / `getCommandHelp(name)`。
`getViewExtents` 因视图范围目前无稳定的模型层单一数据源而暂缓（P3 再评估）。

### 3.3 安全性与可控性

- 所有 NL 改动经 `interp_->execute` → 与菜单同一条撤销栈，可 Ctrl+Z 回退。
- **破坏性命令**（`delete` / `trim` / `purge` / `overkill` 等）默认**先审批**：面板展示拟执行命令 + `执行`/`取消`；或"预览模式"显示将发生的改动。
- **失败反馈闭环**：`execute` 失败 → 回灌 LLM 带错误信息重试（最多 N 次）。

**结论（已确认）**：破坏性命令默认审批。实际把 `delete / erase / trim / extend / purge / overkill /
break / explode / undo` 列入清单，`NlAssistant::processToolCalls` 挂起并 emit `approvalRequested(line)`，
面板弹出「执行 / 取消」审批条；非破坏性命令直接执行并回显 `⮕ 命令 ✓/✗`。
工具执行结果以 `Tool` 消息回灌历史并进入下一轮（最多 `kMaxSteps=8` 步），构成失败重试闭环。

### 3.4 上下文注入

- 轻量：当前选择集、图层列表、视图范围、最近命令。
- **不**把整图所有图元塞进 prompt（大图爆上下文）；用按需查询工具代替。
- 坐标语义走现有 UCS/点解析（`CommandContext::tryParsePoint`、别名、`@` 极坐标语法），与手工输入一致。

---

## 4. 模块与文件（建议）

| 新增/修改 | 说明 |
|-----------|------|
| `src/ai/LlmProvider.h` | 抽象接口 `chat(messages, tools, onDelta, onToolCall, onDone)`，异步 |
| `src/ai/OpenAiProvider.cpp` | QtNetwork + SSE 流式解析（OpenAI 兼容） |
| `src/ai/MockProvider.cpp` | 离线/开发用（规则回显），便于无 key 联调 |
| `src/ai/NlAssistant.{h,cpp}` | 编排 + 工具调用循环 + 上下文 |
| `src/ai/AppContextProvider.{h,cpp}` | 文档状态采集（选择/图层/视图） |
| `src/ai/tooldefs.{h,cpp}` | 工具 schema + 派发（runSketchCommand / 查询工具） |
| `src/ui/AiAssistantPanel.{h,cpp}` | 停靠对话面板（流式、命令卡片、执行/取消、停止） |
| `src/core/AppSettings` | 新增键 `ai/enabled, ai/baseUrl, ai/apiKey, ai/model, ai/autoExecute, ai/maxTokens …`（遵守现有键注册表规范） |
| `MainWindow.{h,cpp}` | 持有 `NlAssistant`；菜单"AI 助手(&A)" + 停靠开关；工具执行主线程派发 |
| `OptionsDialog` | AI 配置页（后端/base_url/api_key/model/是否自动执行） |

---

## 5. 实施阶段与进度

- ✅ **P0 后端抽象**：`LlmProvider` + `OpenAiProvider` + `MockProvider` + AppSettings 键 + 选项页。无 UI 也可单测。
- ✅ **P1 编排核心**：`NlAssistant` + 工具派发到 `interp_->execute` + 无头自测（给消息，断言执行了某命令）。
- ✅ **P2 对话面板**：`AiAssistantPanel` 流式显示 + 破坏性命令审批条 + MainWindow 接入（菜单 / 停靠 / 选项页 / 多文档重绑 / 后端热替换）。
- 🔶 **P3 上下文与可控性**：已落地上下文注入（选择集/图层/最近命令）、破坏性命令审批、失败结果回灌；
  **未完**：`getViewExtents`、撤销链路的显式断言用例。
- 🔶 **P4 打磨**：已落地中文文案、停止/取消、命令回显留痕、日志面板回显；
  **未完**：会话历史持久化、最近命令条、针对 CAD 语法的提示词调优（需真实模型联调）。
- ⬜ **P5（可选）**：本地模型（Ollama/llama.cpp）实测与提示词适配；function-calling 与自由命令行的对比评估。

---

## 6. 验收

- **无头自测 `t_nlAssistant`**：MockProvider 下，输入"画一个圆心(0,0)半径50的圆" → 断言 `circle 0,0 50` 被 `interp_->execute` 调用且模型出现对应图元。
- **端到端**：配置真实 key，面板输入自然语言 → 程序内出现对应图形；破坏性操作需审批。
- **质量门**：双目标零警告构建 + 自测 `EXIT=0`。

---

## 7. 讨论结论（2026-11-08，四项均按推荐方案定稿）

| # | 问题 | 结论 |
|---|------|------|
| 1 | 后端 | **云端 + 本地可切换**（不绑定厂商），另加 `mock` 离线后端；默认云端 |
| 2 | NL→动作表示 | **混合**：少量查询工具 + 单一 `runSketchCommand(line)` 动作工具 |
| 3 | 安全模型 | **破坏性操作默认审批**，非破坏性自动执行，全部改动可 Ctrl+Z |
| 4 | 首版范围 | **仅「LLM 生成命令 + 对话 UI」**，不含本地模型实测 |

> 第 5 项（中文优先模型/提示词）归入 P4：当前 system prompt 与工具描述已全中文，
> 具体模型选型与语法提示词调优留待真实模型联调后迭代。

---

## 8. 实现记录（P0–P2）

**新增文件**
| 文件 | 职责 |
|------|------|
| `src/ai/LlmProvider.h` + `.cpp` | 抽象后端 + `LlmToolCall/LlmMessage/LlmTool/LlmOptions`；薄 `.cpp` 专门接管 QObject 的 moc/vtable |
| `src/ai/OpenAiProvider.{h,cpp}` | OpenAI 兼容 `/chat/completions` + SSE 流式；按 index 聚合碎片化 `tool_calls` |
| `src/ai/MockProvider.{h,cpp}` | 离线规则后端（识别「圆心(x,y)半径r」→ `circle x,y r`），零网络 |
| `src/ai/tooldefs.{h,cpp}` | 5 个工具的 JSON Schema |
| `src/ai/AppContextProvider.{h,cpp}` | `contextJson()` 产出 elementCount/selection/layers/recentCommands |
| `src/ai/NlAssistant.{h,cpp}` | 对话历史 + system prompt + 工具循环 + 破坏性审批挂起 + `kMaxSteps=8` |
| `src/ui/AiAssistantPanel.{h,cpp}` | 停靠面板：流式显示、命令回显、审批条、停止 |

**修改文件**
`CMakeLists.txt`（`CORE_SOURCES` 加 6 个 ai 源、`UI_SOURCES` 加面板、两目标链 `Qt6::Network`）、
`src/core/AppSettings.{h,cpp}`（7 个 `ai/*` 键 + 类型化访问器）、
`src/ui/OptionsDialog.{h,cpp}`（新增「AI 助手」分类页 + load/collect/defaults 逐键接线）、
`src/ui/MainWindow.{h,cpp}`（成员/前向声明、`buildDocks` 停靠、`视图>面板` 开关、
`工具>AI 助手(&A)...`（Ctrl+Shift+A）、`hookActiveDoc` 模型重绑、`settingChanged` 热更新后端）、
`tests/SketchSelfTest.cpp`（`t_nlAssistant` 7 断言）。

**接线要点**
- 面板只做展示/采集；编排与执行全在 `NlAssistant`，命令唯一出口 `interp_->execute(line)`。
- `initAiAssistant()` 在 `loadAppSettingsOnStartup()` 之后调用（依赖 `appSettings_` 就绪），
  面板则在 `buildDocks()` 期先建好，保证菜单 `toggleViewAction()` 可用。
- 后端类型 `cloud/local ↔ mock` 变化时热替换 Provider（`setProvider`），不重建助手与历史；
  切换前先 `stop()` 终止在途请求，旧对象 `deleteLater()` 防回调悬垂。
- 选项「取消回滚」复用 `settingChanged` 同一通道，AI 配置随之回退。
- 面板对「不发流式增量、只给最终文本」的后端做了兜底显示（`gotDelta_` 标志）。

**验收结果（2026-11-08）**
- 双目标 `-Wall -Wextra` 零警告构建通过（`SketchCAD.exe` / `SketchSelfTest.exe` 均重新链接）。
- `SketchSelfTest`：**通过 3165 / 失败 0，EXIT=0**；其中 `t_nlAssistant` **7 / 0**
  （MockProvider → `circle 0,0 50` 经 `interp_->execute` 落模 → 模型恰有 1 个 Circle；
  非绘图指令不产生图元）。
- 端到端（配置真实 key → 面板输入 → 画布出现图形）仍需人工联调，属 P4/P5。

---

## 9. 修正记录：回归「LLM 真正嵌入核心链路」（2026-11-09）

**背景**：曾误引入 `NlCommandMapper` 的归一化/正则/关键词确定性映射，并在
`NlAssistant::sendUserMessage()` 中据此**绕过模型直接执行**，导致
`LlmProvider::chat()` 不再是核心处理环节；自测甚至把「无 provider 也能执行」
当作正确行为固化。该实现与本文 §2、§3.2 的架构相悖，已整体纠正。

**纠正后的链路（对外接口一字未改）**

```
用户输入 → sendUserMessage（原文入历史）→ runTurn()
  → LlmProvider::chat(history, tools, options)   ← 模型做语义理解 / 参数抽取 / 命令生成
  → 模型产出 runSketchCommand(line) 或结构化文本命令
  → interp_->execute(line) → Tool 结果回灌 → 模型生成最终回复
```

- `sendUserMessage()` 删除全部本地规则分支；`provider_ == nullptr` 显式报错且**零执行**。
- `buildSystemPrompt()` 动态注入 `CommandCatalog::usage` 的真实命令语法表
  + 参数抽取 few-shot 示例，让模型依据事实写对 `circle 0,0 20` 这类命令串。
- `tooldefs.cpp` 的 `runSketchCommand` 描述改用真实命令示例（原 `delete last` 并不存在）。
- `NlCommandMapper` 保留 `NlParseResult` 与 `parse(const QString&)` 签名，
  但重定义为**模型输出解码层**：仅解码模型以 JSON 文本返回的命令对象
  （兼容不支持 function-calling 的本地模型 / 网关），不含任何归一化、关键词或正则抽取。
- `MainWindow` / `AiAssistantPanel` / `LlmProvider` / 破坏性审批 / 上下文注入 /
  撤销链 / 后端热替换全部未改；默认 `ai/provider=cloud` → `OpenAiProvider` 真实模型。

**验收**：新增 `t_llmIntegration`（23 断言），用脚本化模型后端证明「用户原文原样入模、
命令串由模型 tool call 决定、工具结果回灌、最终文本由模型生成、无后端零执行」；
`t_nlAssistant`（MockProvider）保持回归。双目标零警告构建 + 自测 `EXIT=0`，
全量 `通过 3189 / 失败 0`。
