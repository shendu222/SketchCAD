# 动作宏 / 命令脚本（MACRO + SCR）设计分析与深思记录

> 第四阶段二次开发生态改造：宏录制、SCR 脚本批处理、批量命令自动执行、
> 录制/编辑/保存/回放流程与中断策略。对应模块 `src/core/MacroScript.{h,cpp}`。

## 1. 需求与边界

| 需求 | 落点 |
|------|------|
| 连续操作录制为可保存、可重放的动作宏 | `MacroRecorder`（单例）+ `CommandInterpreter::execute()` 成功路径捕获点 |
| SCR 脚本编写、导入与顺序执行 | `ScriptRunner::runText/runFile` + `scr` 命令 |
| 批量改图：批量命令一次性自动执行 | SCR 逐行执行 + STOP/CONTINUE 中断策略 + 逐行日志 |
| 录制/编辑/保存/回放流程 | 命令集 `macrorec/macrostop/macrosave/macroshow/macroplay/scr` + 「脚本」菜单入口 |
| 参数解析、出错中断、日志反馈 | 统一 `execute(line)` 通路；行级失败定位；`SketchModel::log` 全程反馈 |

**明确的能力边界**（与 AutoCAD 动作宏一致的取舍）：
宏录制捕获的是**命令行可表达的命令序列**（绘图/修改/图层/块/查询等一切经
`CommandInterpreter::execute` 的操作，含插件注册的外部命令）。鼠标拾取、视图平移缩放
等 GUI 交互不在捕获范围——它们不产生命令行输入。录制结果天然是可读可编辑的文本
（.scr），因此「编辑宏」不需要专门的编辑器 UI：任何文本编辑器都可用。

## 2. 捕获点设计：为什么是 execute() 的统一出口

勘察确认的调用拓扑：

```
CommandLine::onReturn ──┐
ScriptRunner(SCR逐行) ──┼──> CommandInterpreter::execute(line)
ScriptApi::run(JS/Py) ──┤        │
SDK 外部命令注册 ────────┘        ├─ 未知命令 → log(warn) + false
                                  ├─ 外部命令表 → fn(args)
                                  └─ CommandRegistry → ICommand::execute(ctx)
```

`execute(line)` 返回 `bool`（是否成功）且是**所有命令通路的唯一汇合点**。
改造为统一出口后：

```cpp
bool ok = false;
if (!command) { /* 外部命令 / 未知命令 */ }
else          { CommandContext ctx{model_, toks, cmd, this}; ok = command->execute(ctx); }
if (ok) MacroRecorder::captureLine(s);   // 唯一捕获点
return ok;
```

好处：
- **一处接入，处处录制**：命令行输入、JS/Python 脚本里的 `app.run(...)`、
  插件命令全部被覆盖，无需在每个 UI 入口埋点；
- **失败不录**：只有成功执行的命令进入宏，回放天然规避「录制了错误操作」；
- **入参保真**：录制的是用户输入的原始命令行（trim 后），不是解析后的参数——
  回放时重新走完整的分词/解析路径，与手工输入完全同构。

## 3. 录制/回放互斥：递归套娃防护

三个互斥规则，全部收敛到 `MacroRecorder` 的两个静态标志：

| 场景 | 策略 | 实现位置 |
|------|------|----------|
| 回放中执行命令 | `captureLine` 直接忽略（`s_playbackActive`） | `captureLine` |
| 回放中再次回放 | `runText` 拒绝嵌套，返回 `stoppedEarly` | `runText` 开头 |
| 录制中回放 | `runText` 拒绝 + `start()` 返回 false | 双侧检查 |

`setPlaybackActive(true)` 用 RAII 守卫（栈对象析构复位），异常/提前 return 都不会
留下「永久回放态」。互斥期不是静默失败：`runText` 拒绝时 `log(error)` 明确告知原因。

另外 `isMacroControlCommand()` 把 6 个宏命令本身排除在录制外——否则 `macrorec`
（在录制开始后才走到捕获点）或回放期间的 `scr` 会污染录制缓冲、甚至产生指数套娃。

## 4. SCR 格式与中断策略

### 4.1 格式（与 AutoCAD .scr 习惯对齐，但更严格）

- 每行一条命令，内容与命令行输入**完全一致**（同一解析器、同一语义）；
- 注释：`#`、`;`、`//` 三种行首标记；空行跳过；
- 编码 UTF-8；CRLF/LF 均可（逐行 `trimmed()` 天然去除 `\r`）；
- 路径含空格用双引号包裹（`macrosave "D:\my dir\a.scr"`），解析端
  `parsePathArgs` 支持引号提取 + 尾部 `continue` 关键字。

**格式即录制格式的逆**：`macrorec` 录出的缓冲逐行写盘就是合法 .scr，
反之 `load()` 读入的也是同一结构——录制、编辑、回放共享一个文本表示，
不存在第二种序列化格式（DRY）。

### 4.2 中断策略：STOP / CONTINUE

| 模式 | 行为 | 适用场景 |
|------|------|----------|
| `StopOnError`（默认） | 首个失败行立即中断，报告行号与统计 | 改图命令有依赖顺序时（后续行依赖前序结果） |
| `ContinueOnError`（`scr 文件 continue`） | 全部执行，结束汇总失败行号列表 | 独立命令的批量处理（每行互不依赖） |

选择默认 STOP 的理由：批量改图最常见的形态是「对同一批图元的连续操作」，
前一行失败后继续执行往往产生更混乱的中间状态；而真正独立的批处理用户
显式加 `continue` 即可，成本一行、语义清晰。

`macroplay` 固定 STOP（回放宏本来就是一条依赖链）；`scr` 尊重参数。

### 4.3 返回值语义

`ScriptRunner` 返回 `ScriptRunResult{totalLines, executed, failed, failedLineNo,
stoppedEarly}`。`scr`/`macroplay` 命令将其折叠为 `execute()` 的 bool：
全部成功才返回 true——命令行层（含调用它的 JS/COM 层）因此能感知脚本整体成败，
行级细节则由日志承担。

## 5. 日志反馈设计

逐行双日志（均走 `SketchModel::log`，自动汇入 UI 日志面板）：

1. 执行前：`[<脚本名:行号> >> 命令内容]`（type=cmd）——回放过程可完整追溯；
2. 失败时：`[<脚本名:行号> 执行失败：内容]`（type=error）+ STOP 模式附加中断位置
   （type=warn）；结束汇总：`共 N 条，成功 X，失败 Y（失败行：i,j,k）`。

日志前缀用**脚本文件名**而非固定 "scr"：嵌套场景（未来 COM 批量任务跑脚本）
中多脚本并行写日志时仍可区分来源。

## 6. 与既有生态的协作

- **脚本宿主（JS/Python）**：`app.run("...")` 经 execute 成功后同样被录制——
  混合工作流（JS 生成 + 命令微调）可整体录为一个宏；
- **原生 SDK 插件**：外部命令走 `execute` 的外部命令表分支，同样在录制范围；
  注意 `ScriptRunner` 用 `ctx.interp`（而非新建解释器）执行各行，保证插件命令
  在脚本中可用；
- **COM 批量出图**：`BatchPlotEngine` 面向「文件级批量」；本模块面向「命令级批量」，
  两者互补不重叠。未来 COM 任务可加「先跑 .scr 再出图」的任务类型。

## 7. UI 流程（MainWindow「脚本」菜单）

- 「宏：开始录制 / 停止录制」：单一入口动态文本，触发即经 `interp_->execute()`
  走命令通路（与命令行输入零差异）；`aboutToShow` 时按 `isRecording()` 同步文本，
  命令行方式的录制状态变化也能反映；
- 「宏：保存录制...」：`QFileDialog::getSaveFileName` → `macrosave "<路径>"`；
- 「运行 SCR 脚本...」：`getOpenFileName` → `scr "<路径>"`（默认 STOP；
  需要 CONTINUE 的用户在命令行加 `continue` 参数）。

路径一律加双引号传入命令行——Windows 用户目录（如 `C:\Users\xx New\...`）
含空格是常态，`parsePathArgs` 的引号分支处理。

## 8. 自测与验证

`t_macroScript`（SketchSelfTest，全量无头）覆盖 8 组断言：
① 录制捕获（成功入、失败与宏命令不入）；② .scr 保存/加载往返；
③ `macroplay` 回放后新模型图元一致；④ 三种注释与空行跳过；
⑤ STOP 提前中断 + 失败行号 + 中断后行未执行；⑥ CONTINUE 全执行 + 汇总；
⑦ 三重互斥（录制中回放拒、重复录制拒、回放期捕获忽略）；
⑧ `scr` 命令经 execute 的带引号路径解析与失败返回值。

实测基线：**51/0 全过，`-Wall -Wextra` 零新增警告**（仅剩 GCC STL
`array-bounds` 存量误报）。

## 9. 实测教训（两个高价值 bug 的定位与修复）

### 9.1 magic statics 递归初始化死锁（严重，全局性）

首版实现 `registerMacroCommands()` 内部调用 `CommandRegistry::instance()`
获取注册表。而调用链是：`instance()` 的静态局部单例构造 → `registerBuiltins()`
→ 末尾 `registerMacroCommands()` → **再次 `instance()`**。C++11 起静态局部
变量初始化有并发/递归保护：在初始化完成前再入会**永久阻塞**。

症状极具迷惑性：自测进程在 t_scriptHost（首个触发 `instance()` 的用例）
处空闲挂起——CPU 时间 5 秒增量为 0，内存稳定，无输出、无崩溃。若不是给
CHECK 宏加逐用例进度标记 + 采样 CPU 时间，很容易误判为沙箱/环境问题。

**修复**：`registerMacroCommands(CommandRegistry& reg)` 改为接收引用参数，
调用点 `registerBuiltins()` 传入 `*this`，彻底消除再入。
**规则沉淀**：任何在单例构造期间被调用的注册函数，一律由构造方传引用，
函数体内不得再调 `instance()`。

### 9.2 断言语义与实现语义的偏差（轻微，测试侧）

`ScriptRunResult::totalLines` 实现语义是「已处理行数」（`++` 发生在
execute 之前、STOP 中断后不再累计），断言却按「脚本有效行总数」写。
修复：断言改为 `totalLines == 2`，并在头文件注释中明确语义。

### 9.3 诊断方法论

- 自测用例成功时无输出（CHECK 宏只打失败项）——「日志停在头部」不代表
  卡在第一个用例，必须先加进度标记再定位；
- 直接运行 exe 被沙箱 SIGTERM 拦截时，`./build.sh test`（shell 脚本内
  执行）是放行通路；最小 C 程序（minic）对照实验可快速区分「环境拦截」
  与「程序自身问题」。

## 10. 遗留与演进

- 宏录制目前为进程内缓冲，未随图保存（重启丢失）——`macrosave` 落盘即补救；
  未来可挂入 Memento/图纸 JSON `"macros"` 字段；
- 录制不含 GUI 交互（见 §1 边界）；若需要，可在 GraphicsView 层把拾取动作
  反向翻译为命令行文本再 `captureLine`（工程量大，暂不做）；
- `relative` 坐标（相对上一命令点）等 AutoCAD SCR 高级语法未引入；
  当前以 `line_rel_angle` 等显式命令家族覆盖同类需求。
