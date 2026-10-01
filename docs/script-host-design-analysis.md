# 脚本宿主（Script Host）设计分析

> 功能：构建脚本宿主，支持用 **Python / VBA / JavaScript** 编写脚本来自定义和扩展程序行为。
> 用户无需修改主程序源码，即可通过脚本新增功能。宿主提供**脚本加载、执行、与程序内部对象交互、异常处理、日志**五要素。

---

## 1. 任务目标

在保持现有功能与行为完全不变的前提下，为 SketchCAD 增加一套二次开发能力：

1. **加载**：从字符串 / 文件加载脚本（`.js` / `.py` / `.vbs` 按扩展名识别语言）。
2. **执行**：由宿主统一调度，屏蔽语言差异。
3. **内部对象交互**：脚本能读写图元、执行命令、查询模型，但**只通过统一交互面**，不触碰任何 UI 或内部头文件。
4. **异常处理**：脚本运行时抛错被宿主捕获，转化为结构化结果，不导致主程序崩溃。
5. **日志**：脚本的 `print` / `log` 输出实时汇入主程序日志面板。

设计约束：默认构建必须**零外部依赖、可编译、自测通过**；Python / VBA 作为可选能力，开启后才引入对应运行时。

---

## 2. 约束与现状评估

- 技术栈：C++17 / Qt 6.10.1(mingw_64) / CMake + Ninja；命名空间 `SketchCAD`。
- 现有可复用的内部对象面：
  - `SketchModel`：`addElement(const Element&)->int`、`element(int)->const Element*`、`elements()`、`removeElement(int)->bool`、`log(msg,type)`、`CommandInterpreter::execute(line)->bool`。
  - `ElementFactory`：已封装 `createLine/createCircle/createArc/createRect/createText/...` 等工厂方法（避免重复手写 `Element`）。
  - `CommandInterpreter`：命令行解释器（命令模式 + 注册表），`execute` 可作为脚本 `run` 命令入口。
- 关键限制：
  - `QJSEngine` 位于 **`Qt6::Qml`** 模块（非 `Qt6::Core`），默认构建需显式链接 `Qt6::Qml`。
  - Python 需 CPython 开发库；VBA 需 Windows Script Host（COM / `ole32` `oleaut32`）。两者默认环境不一定具备，**不能强制默认编译**，否则破坏现有构建。

结论：采用**策略模式 + CMake 选项门控**，保证 JS 默认开箱即用，Py/VBA 按需开启。

---

## 3. 方案权衡

| 方案 | 说明 | 取舍 |
|------|------|------|
| A. 仅支持一种语言（JS） | 实现简单、零依赖 | 不满足"Python/VBA/JS"三选一需求 |
| B. 为每种语言写独立子系统 | 各写各的，无统一抽象 | 重复代码、难维护、违背 DRY |
| **C. 策略模式 + 统一交互面（采用）** | `ScriptHost` 为上下文，持有 `ScriptApi` 与各 `ScriptEngine` 策略；语言可插拔 | 新增语言只加一个 `ScriptEngine`；JS 默认、Py/VBA 门控；代码清晰、DRY |

关键设计决策：
- **统一交互面 `ScriptApi`（QObject）** 作为三语言的公共桥：JS 注入为全局对象 `app`；Python 经 `sketch` 模块函数转发；VBA 经 `App` 的 IDispatch 转发。**三语言复用同一份 `ScriptApi`**，彻底隔离主程序内部实现。
- **`ScriptResult{ok, output, error, language}`** 统一返回结构，所有引擎共用。
- **引擎 `available()`** 把"运行时是否就绪"纳入运行时判断，配合 CMake 门控，使宿主对缺失运行时优雅降级（返回明确错误而非崩溃）。

---

## 4. 架构设计（策略模式）

```
                ┌─────────────────────────────────────┐
   调用方 ──────▶│           ScriptHost (Context)        │
                │  · runCode(lang, code)               │
                │  · runFile(lang, path)               │
                │  · availableLanguages()              │
                │  持有：ScriptApi* + vector<Engine*>   │
                └───────────────┬─────────────────────┘
                                │ engineFor(lang)
                                ▼
                 ┌──────────────────────────────────┐
                 │        ScriptEngine (抽象)         │
                 │  + language() / displayName()      │
                 │  + available()                     │
                 │  + execute(code, api, logSink)      │
                 └────────────┬───────────┬───────────┘
                              │           │
                ┌─────────────┴──┐  ┌─────┴─────────────┐
                │ JavaScriptEngine│  │  PythonEngine*    │  (*门控默认OFF)
                │ (QJSEngine)    │  │  (CPython 嵌入)    │
                │ 默认启用        │  └──────────────────┘
                └────────────────┘  ┌──────────────────┐
                                   │  VbaEngine*       │  (*门控默认OFF)
                                   │ (WSH VBScript)    │
                                   └──────────────────┘

       所有引擎共享  ──▶  ScriptApi (QObject)
                          addLine/addCircle/addArc/addRect/addPoint/addText
                          elementCount/elementInfo/removeElement
                          run(cmd) / log(msg)  + signal scriptOutput
```

- **Context（ScriptHost）**：构造时创建 `ScriptApi`（绑定 `model_` + `interp_`），并 `registerEngines()` 注册策略。运行时按 `language` 选择引擎，对调用方透明。
- **Strategy（ScriptEngine）**：每个语言一个子类，实现 `execute`。
- **桥接（ScriptApi）**：策略与主体之间的唯一耦合点，向下调用 `SketchModel` / `CommandInterpreter` / `ElementFactory`。

---

## 5. 五要素落地

| 要素 | 落地方式 |
|------|----------|
| **加载** | `ScriptHost::runFile` 读文件（UTF-8）→ `runCode`；`runCode` 直接吃源码字符串。菜单"运行脚本文件..."按扩展名推断语言。 |
| **执行** | 各引擎 `execute()` 在独立引擎实例中求值；JS 用 `QJSEngine::evaluate`，Python 用 `PyRun_String`，VBA 用 `IActiveScript::ParseScriptText + SetScriptState(RUNNING)`。 |
| **内部交互** | 脚本只访问 `ScriptApi`（`app.addLine(...)` 等）。所有写操作经 `ElementFactory` + `SketchModel::addElement`，与 GUI 绘制路径完全一致。 |
| **异常** | JS：`QJSValue::isError()` 取行号+信息；Python：`PyErr_Fetch` 格式化；VBA：`IActiveScriptSite::OnScriptError` 取描述。统一写入 `ScriptResult.error`，`ok=false`，**绝不向外抛异常**。 |
| **日志** | `ScriptApi::log` 既写 `model_->log`（进 LogPanel）又 `emit scriptOutput`；各引擎在 `execute` 内把该信号连到 `logSink`，实时把 `print/log` 累积进 `res.output` 并转发给宿主信号。 |

`ScriptApi` 暴露的方法（Q_INVOKABLE，便于反射桥接）：
`addLine/addCircle/addArc/addRect/addPoint/addText`、`elementCount/elementInfo/removeElement`、`run/log/version`。

---

## 6. 实现中的坑与修复

1. **`QJSEngine` 误以为在 `Qt6::Core`**
   - 现象：若只链 `Qt6::Core` 会找不到 `<QJSEngine>`。
   - 修复：`find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Qml)`，两目标均链接 `Qt6::Qml`。

2. **`ScriptHost.h` 仅前向声明 `ScriptApi` → `unique_ptr` 析构 incomplete type**
   - 现象：测试里 `ScriptHost host(...)` 析构时报 `invalid application of 'sizeof' to incomplete type`。
   - 修复：`ScriptHost.h` 直接 `#include "ScriptApi.h"`，使 `ScriptApi` 在持有点即完整类型。

3. **测试缺 `CommandInterpreter` 完整类型**
   - 修复：`SketchSelfTest.cpp` 补 `#include "CommandInterpreter.h"`。

4. **菜单变量名 `s` 与已有"绘图"菜单冲突**
   - 修复：脚本菜单变量改名 `scr`。

5. **VBA IDispatch 参数逆序**
   - COM 约定 `DISPPARAMS.rgvarg` 中**最右为第一个实参**，取参时按 `rgvarg[cArgs-1-idx]` 还原，已在 `VbaAppDispatch::Invoke` 中处理。

---

## 7. 验证结果

- **双目标构建**：`SketchCAD` 与 `SketchSelfTest` 均 `-Wall -Wextra` **零错误、零新增警告**，链接 `Qt6::Qml` 成功。
- **自测**：`tests/SketchSelfTest.cpp` 新增 `t_scriptHost()`，全量 **48/0**（原 47/0，+1）：
  - 默认 JS 引擎可用；
  - 加载/执行/内部交互：`app.addLine(...)` 确实使 `elementCount` +2；
  - `print('hello',123)` 输出被捕获到 `res.output`；
  - `throw new Error('boom')` 被捕获（`ok=false` 且 `error` 非空）；
  - `app.run('help')` 委托命令行解释器成功；
  - 不支持的语言（`cobol`）返回 `ok=false` 而非崩溃。

---

## 8. 范围与扩展（Python / VBA 门控）

为不破坏默认构建，Python / VBA 引擎**默认不编译**，通过 CMake 选项开启：

```cmake
option(SCRIPTHOST_ENABLE_PYTHON "嵌入 CPython，支持 Python 脚本" OFF)
option(SCRIPTHOST_ENABLE_VBA    "Windows Script Host，支持 VBA/VBScript" OFF)
```

- **PythonEngine**：嵌入 CPython（`find_package(Python3 COMPONENTS Development)`），把 `ScriptApi` 包装为 `sketch` 模块函数，并重定向 `sys.stdout/stderr` 到 `sketch.log`。开启后落盘编译即可用。
- **VbaEngine**：基于 Windows Script Host 宿主 VBScript，把 `ScriptApi` 作为命名对象 `App` 的 `IDispatch` 暴露（`SCRIPTITEM_GLOBALMEMBERS`），支持 `App.AddLine(...)` 等调用。仅 `Q_OS_WIN` 且开启时编译。

两者与 JS 共享同一 `ScriptApi` 与 `ScriptResult`，宿主调度逻辑完全一致——**新增一门语言 = 派生一个 `ScriptEngine` + 在 `registerEngines` 注册**，符合开闭原则。

> 说明：JS 路径已在默认构建中经自测验证；Python/VBA 为完整实现、按开关启用，启用后需在具备对应运行时（CPython 开发库 / Windows 主机）的环境中编译与回归验证。

---

## 9. 主要改动清单

**新增文件（`src/core/`）**
- `ScriptEngine.h` —— `ScriptResult` 结构 + `ScriptEngine` 抽象基类（策略接口）。
- `ScriptApi.h` / `ScriptApi.cpp` —— 统一交互面（QObject），脚本增删图元/执行命令/写日志的**唯一桥**。
- `JavaScriptEngine.h` / `JavaScriptEngine.cpp` —— `QJSEngine` 引擎（默认启用，零依赖）。
- `ScriptHost.h` / `ScriptHost.cpp` —— 编排器（Context），按语言选引擎、统一调度与信号。
- `PythonEngine.h` / `PythonEngine.cpp` —— CPython 引擎（门控 `SCRIPTHOST_ENABLE_PYTHON`）。
- `VbaEngine.h` / `VbaEngine.cpp` —— WSH VBScript 引擎（门控 `SCRIPTHOST_ENABLE_VBA && Q_OS_WIN`）。

**修改文件**
- `CMakeLists.txt` —— 链接 `Qt6::Qml`；CORE_SOURCES 加入 `ScriptApi/JavaScriptEngine/ScriptHost`；新增 `SCRIPTHOST_ENABLE_PYTHON/VBA` 选项与门控源码装配。
- `src/ui/MainWindow.h` —— 前向声明 `ScriptHost`；新增成员 `scriptHost_` 与槽 `onRunJavaScript()/onRunScriptFile()`。
- `src/ui/MainWindow.cpp` —— 构造时创建 `ScriptHost` 并连接 `scriptOutput/scriptError` 到日志面板；新增"脚本(&S)"菜单（运行 JS / 运行文件 / 列出引擎）。
- `tests/SketchSelfTest.cpp` —— 新增 `t_scriptHost()` 并注册，验证五要素与异常/日志行为。

**行为不变性**：所有既有图元创建/命令/序列化路径未改动；脚本仅通过既有 `ElementFactory` + `SketchModel` API 操作数据，渲染与存档链路零影响。
