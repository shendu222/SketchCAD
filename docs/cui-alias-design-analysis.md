# 界面定制（CUIx）与命令别名（acad.pgp）设计分析与深思记录

> 第五阶段二次开发生态改造：界面定制能力（CUIx 兼容的工具条与工作空间加载/保存/编辑）
> 与命令别名机制（acad.pgp 兼容的单/多字母别名、可手编文件、重载即时生效）。
> 对应新增模块：`src/core/AliasTable.{h,cpp}`、`src/core/CuiManager.{h,cpp}`、
> `src/core/CuiAliasCommands.cpp`；接入点：`CommandInterpreter::execute()` 别名展开、
> `CommandRegistry::registerBuiltins()` 命令注册、`MainWindow` UI 集成。

## 1. 需求与边界

| 需求（用户指令） | 落点 |
|------|------|
| CUIx 文件的加载、保存与编辑 | `CuiManager::load/save`（XML(.cuix) 读写）+ 命令 `cuiload`/`cuisave` |
| 自定义工具条：创建、编辑 | `CuiManager` 工具条 CRUD + 命令 `tbnew`/`tbadd`/`tbdel`/`tbrm` |
| 多工作空间（Workspace）的创建、切换、管理 | `CuiManager` 工作空间 CRUD + 命令 `wsnew`/`wsload`/`wslist`/`wsdel` |
| acad.pgp 风格命令别名 | `AliasTable`（单例）单字母/多字母映射 |
| 输入时解析别名并执行完整命令 | `CommandInterpreter::execute()` 分词后、查找前就地展开 |
| 用户可手编别名文件，修改后重载生效 | `.pgp` 文本格式 + `aliasreload`/`aliasload`/`aliassave`/`aliaslist` |
| 兼容现有架构、接口清晰可扩展 | 纯 C++ 核心层 + 单接入点，UI 仅消费数据层可见性判定 |

**两个清晰的能力边界**（取自 AutoCAD 既有范式，避免自造轮子）：

1. **别名只作用于「命令名 token」**：别名机制的语义是对用户输入的首 token 做
   字符串→字符串映射；它不解析参数、不介入几何。这与 AutoCAD `acad.pgp` 完全一致，
   保证行为可预测、可调试。
2. **CUIx 是「界面配置」而非「行为逻辑」**：`CuiManager` 仅描述「有哪些工具条、
   每个按钮触发什么命令/工具、当前工作空间显示哪些 UI」，不内嵌任何命令执行逻辑。
   命令执行仍走既有的 `CommandInterpreter`/`ToolRegistry` 通路。由此数据与行为解耦，
   配置可被任意来源（手写文件、命令、未来 UI 对话框）读写。

## 2. 命令别名机制：解析点设计

### 2.1 为什么是 execute() 分词之后、findExact 之前

勘察确认的命令调用拓扑（延续前序阶段结论）：

```
CommandLine::onReturn ──┐
ScriptRunner(SCR 逐行) ──┼──> CommandInterpreter::execute(line)
ScriptApi::run(JS/Py) ──┤        │
SDK 外部命令注册 ────────┘        ├─ 分词 toks[0].toLower()  →  cmd
                                  ├─ 【别名展开】resolve(cmd) 替换 toks[0]/cmd
                                  ├─ CommandRegistry::findExact(cmd)
                                  ├─ findPrefixMatch(cmd)   （line_* 家族）
                                  └─ 外部命令表 → fn(args)
```

`execute()` 是所有命令通路的唯一汇合点，且 `cmd` 在第 96 行已完成 `toLower()`。
别名展开插入在「第 97 行 `cmd = toks[0].toLower()`」与「第 112 行
`CommandRegistry::instance()` 查找」之间：

```cpp
QString cmd = toks[0].toLower();
{
    QString resolved = AliasTable::instance().resolve(cmd);
    int guard = 0;
    while (resolved != cmd && guard < 8) {     // 别名链 + 深度保护
        toks[0] = resolved;
        cmd = resolved;
        resolved = AliasTable::instance().resolve(cmd);
        ++guard;
    }
}
auto& reg = CommandRegistry::instance();
ICommand* command = reg.findExact(cmd);
```

**取舍理由**：
- **一处接入，覆盖全部入口**——命令行、SCR 脚本、JS/Python 脚本宿主、插件外部命令，
  只要在 execute() 内分词，别名就生效；无需在每个 UI/脚本入口埋点。
- **参数零侵入**——`toks[0]` 被替换为完整命令名，后续 `toks[1..]` 原样保留，
  命令执行时看到的参数与用户手写完整命令完全一致。
- **大小写无关**——`resolve` 内部 `toLower()`，用户输 `L`/`l`/`LINE` 行为一致。

### 2.2 别名链与防环（8 跳深度保护）

`resolve` 的语义是「alias → command」；但命令本身也可能是另一个别名（例如误配
`a→b`、`b→c`）。因此展开用 `while` 循环做链式替换，并以 `guard < 8` 硬上限兜底：
正常深度远小于 8，一旦误配成环，循环在第 9 跳前必定停止，绝不死锁。

反例（应安全退化）：`.pgp` 中写 `x,x` 或 `x,y; y,x` → 至多 8 跳后停在某个
`resolve` 返回自身（即 `resolved == cmd`），退出循环，最终把该 token 当作命令名
交给查找（查找失败则走既有「未知命令」提示，不会崩）。

### 2.3 acad.pgp 兼容格式

逐行解析：`;` / `#` 开头为注释，空行跳过，逗号分隔 `别名,完整命令`，两侧 trim：

```
; SketchCAD 命令别名文件（acad.pgp 兼容）
; 每行格式：别名,完整命令
L,LINE
REC,RECT
CUSTOM,mycommand
```

`save` 写回带注释头的 UTF-8 文本。`load` 成功后记录 `filePath_`，使 `aliasreload`
无需记忆路径即可「原地重载」——改完文件回命令行敲 `aliasreload` 即生效，无需重启。
默认别名集合（`defaultAliases()`，23 条）对齐用户需求示例：
`l→line, c→circle, a→arc, rec→rect, pl/pli→polyline, e→delete, m→move,
co/cp→duplicate, ro→rotate, tr→trim, ex→extend, mi→mirror, o→offset, f→fillet,
sc→scale, z→zoom, la→layer, b→block, x→explode, dt→mtext, q→qselect`。
（注：真实命令名以 `Commands.cpp` 注册为准——删除命令名为 `delete`、复制为
`duplicate`，故未引入不存在的 `erase`/`purge`/`copy` 别名，避免「别名指向空命令」。）

## 3. 界面定制（CUIx）：数据/UI 分离

### 3.1 核心原则

`CuiManager` 是**纯 C++ 数据模型 + XML IO，零 QtWidgets 依赖**，可无头单测。
`MainWindow` 只消费其可见性判定（`toolbarVisible`/`menuVisible`）来 `show()`/`hide()`
对应的 `QToolBar`/`QMenu`。数据层不持有、不操作任何 Qt 控件——这是与既有架构
「互不污染」的关键。

```
CuiManager（核心层，可单测）           MainWindow（UI 层）
┌─────────────────────────┐           ┌──────────────────────────┐
│ toolbars_  (CuiToolbar)  │           │ initCui(): 注册内置 UI    │
│ workspaces_(CuiWorkspace)│  refresh  │  → 载入默认 .pgp/.cuix     │
│ builtinToolbars_/Menus_ │ ─callback→│  → applyCui(): 重建+可见性 │
│ currentWs_              │           │ findChildren<QToolBar>()  │
│ refresh_ (std::function) │           │ menuBar()->actions()      │
└─────────────────────────┘           └──────────────────────────┘
        ↑ 命令改动（cuiload/wsload/...）调用 requestRefresh()
```

### 3.2 数据模型

```cpp
struct CuiButton   { QString command; QString label; QString icon; };
struct CuiToolbar  { QString id; QString title; QList<CuiButton> buttons; };
struct CuiWorkspace{ QString id; QString title; QStringList toolbars; QStringList menus; };
```

- `command` 既可是**工具 id**（如 `"line"`，走 `model_->setActiveTool`）也可是
  **命令名**（如 `"save"`，走 `interp_->execute`）——`MainWindow::runToken` 二选一：
  先 `ToolRegistry::find(id)`，命中则激活工具，否则当作命令执行。
- `label`/`icon` 为空时由 UI 取默认值（工具 `displayName` / `icons::make(id)` 兜底），
  配置可只写 `command`，保持 .cuix 简洁。

### 3.3 XML(.cuix) 格式取舍

选用 **XML 而非二进制/JSON** 的理由：
- **人类可读可手编**：与「用户可自编别名文件」的需求同调，CUIx 也应允许文本编辑器
  直接改；XML 比二进制友好，比 JSON 的 `<Button command=...>` 属性写法更贴近
  AutoCAD 的 CUIx 直觉（降低用户迁移认知成本）。
- **流式读写、无第三方依赖**：`QXmlStreamReader`/`QXmlStreamWriter` 随 Qt 自带，
  不引入新依赖；`setAutoFormatting` 保证落盘文件美观、diff 友好。
- **版本属性**：根元素 `<SketchCADCUI version="1.0">`，为未来增量字段预留软扩展点。

```xml
<SketchCADCUI version="1.0">
  <Toolbars>
    <Toolbar id="myBar" title="我的工具条">
      <Button command="line" label="直线" icon="draw-line"/>
      <Button command="circle"/>
    </Toolbar>
  </Toolbars>
  <Workspaces current="ws1">
    <Workspace id="ws1" title="草图">
      <Toolbars><Ref id="myBar"/></Toolbars>
      <Menus><Ref id="menuDraw"/><Ref id="menuEdit"/></Menus>
    </Workspace>
  </Workspaces>
</SketchCADCUI>
```

### 3.4 工具条 CRUD

`CuiManager` 持有 `QList<CuiToolbar>`，提供 `addToolbar`/`removeToolbar`/
`addButton`/`removeButton`。`removeToolbar` 会同步清理所有工作空间对该工具条的
`Ref`（避免悬空引用）；`removeButton(tb, idx)` 按序号删除（序号从 0 起，与
`tbrm` 命令一致）。自定义工具条在 `MainWindow::rebuildCustomToolbars()` 中按
`customToolbars_` 列表**整体替换**，避免重复累积——这是相对「每次命令追加一个
QToolBar」更稳妥的 UI 策略。

### 3.5 工作空间可见性规则

这是 CUIx 的核心语义，规则刻意保持简单、可预测：

| 状态 | 工具条/菜单可见性 |
|------|------|
| `currentWs_` 为空（默认启动态） | **全部可见**（`toolbarVisible`/`menuVisible` 直接返回 true） |
| 已 `setCurrentWorkspace(id)` | 仅工作空间 `toolbars`/`menus` 列表内的元素可见；列表外隐藏 |

实现：

```cpp
bool CuiManager::toolbarVisible(const QString& id) const {
    if (currentWs_.isEmpty()) return true;            // 默认全显
    const CuiWorkspace* w = workspace(currentWs_);
    return w ? w->toolbars.contains(id) : true;
}
```

`wsnew` 以「全部已知工具条 + 全部内置菜单」为可见集快照新建，故新工作空间默认
不丢失任何 UI；`saveCurrentAsWorkspaceDialog`（UI 菜单入口）则快照**当前实际可见**
的 UI 为工作空间，契合「把当前布局存成预设」的直觉。

### 3.6 命令驱动 UI 实时刷新（解耦核心与 UI）

核心层不得依赖 Qt 信号，否则破坏「无头单测」前提。改用回调注入：

```cpp
// CuiManager 暴露
using RefreshFn = std::function<void()>;
void setRefreshCallback(RefreshFn fn);
void requestRefresh() const { if (refresh_) refresh_(); }

// MainWindow::initCui() 注册
CuiManager::instance().setRefreshCallback([this]{ applyCui(); });
```

命令（`cuiload`/`wsload`/`wsdel`/`tbnew`/`tbadd`/`tbdel`/`tbrm`）在改完数据后调用
`requestRefresh()`，由 UI 注册的 `applyCui()` 重建工具条并重算可见性。命令逻辑
零 Qt 代码，单测时 `refresh_` 为空回调即无害。

## 4. 命令注册：防 magic statics 再入（收引用）

单例 `CommandRegistry::instance()` 的静态初始化器内会调 `registerBuiltins()`，
而 `registerBuiltins()` 又调用一批 `registerXxxCommands`。若这些函数体内部再调
`CommandRegistry::instance()`，会触发 magic statics 递归初始化死锁（历史高价值坑，
第四阶段已用 `registerMacroCommands(*this)` 解决）。

本阶段沿用同一纪律：

```cpp
// Commands.cpp registerBuiltins() 末尾
registerMacroCommands(*this);   // 收引用（*this 已在 instance() 初始化器中）
registerAliasCommands(*this);
registerCuiCommands(*this);
```

`registerAliasCommands`/`registerCuiCommands` 签名均为
`void(CommandRegistry& reg)`，体内只使用传入引用、**绝不**再调 `instance()`。
`Commands.h` 在 `HelpCommand` 之后、`} // namespace SketchCAD` 之前补前向声明。

## 5. UI 集成

`MainWindow` 改动保持**既有行为不变**（既有工具条/菜单构建逻辑未改）：

- `buildMenu()` 为 12 个顶级菜单加 `setObjectName`（`menuFile`/`menuDraw`/…），
  供工作空间按 objectName 精确 show/hide。
- 构造函数 `buildMenu()` 之后调 `initCui()`：注册内置 UI 元素 id、载入默认
  `alias.pgp`/`sketchcad.cuix`、设 refresh 回调、调 `applyCui()`。
- 新增「界面(&U)」菜单（`cui`，objectName `menuCui`）：打开别名文件、aliasreload、
  aliaslist、工作空间子菜单（动态重建：默认项 + 各 ws 勾选项 + 新建/列表）、
  cuiload/cuisave、新建工具条对话框。
- 8 个方法：`initCui`/`runToken`/`applyCui`/`rebuildCustomToolbars`/
  `applyWorkspaceVisibility`/`openAliasFile`/`newToolbarDialog`/
  `saveCurrentAsWorkspaceDialog`。

## 6. 自测验证（基线 51 → 53/0）

`tests/SketchSelfTest.cpp` 新增两个无头用例（纯核心层，不依赖 GUI）：

- **`t_alias`**：默认别名集合规模与关键映射；单字母（`L`/`c`/`A`）与多字母
  （`REC`/`pl`/`PLI`）解析；大小写无关；非别名原样返回；运行时 `setAlias`/
  `removeAlias`；`.pgp` 注释/空行跳过与逗号解析；`save→load` 往返一致性。
- **`t_cui`**：工具条 `addToolbar`/`addButton`/`removeButton`/`removeToolbar`；
  无工作空间默认全显、`setCurrentWorkspace` 后按列表可见性判定；`.cuix`
  `save→clear→load` 往返（工具条数、按钮命令、工作空间、当前 ws、菜单列表一致）。

构建三目标（`SketchCAD`/`SketchSelfTest`/`SketchCADAutomation`）零警告回归，
经 `build.sh test` 放行通路运行 `SketchSelfTest.exe`，确认 **53/0**。

## 7. 关键方案取舍与理由（汇总）

| 决策点 | 选择 | 理由（与替代方案对比） |
|------|------|------|
| 别名解析位置 | `execute()` 分词后、findExact 前 | 单接入点覆盖全部命令入口；参数零侵入；替代「每个 UI 入口各解析一次」会漏 SCR/脚本/插件 |
| 别名链 | `while` + 8 跳 guard | 兼容 `a→b→c` 链式；硬上限防误配成环死锁；替代「只展开一层」会漏多级别名 |
| 别名文件格式 | acad.pgp 文本（逗号分隔） | 与 AutoCAD 习惯一致、可手编、`aliasreload` 即时生效；替代 JSON 反而偏离用户预期 |
| CUIx 数据层 | 纯 C++ 单例 + XML，零 QtWidgets | 可无头单测；数据与行为解耦；替代「UI 内直接读写 QToolBar」会把配置逻辑绑死到 GUI |
| CUIx 文件格式 | XML(.cuix) | 可读手编、Qt 自带流式 IO、版本属性预留；替代二进制不可审计、替代 JSON 不贴合 CUIx 直觉 |
| 工作空间默认可见性 | 无 ws → 全显 | 默认态不丢 UI，迁移安全；替代「默认全隐藏」会让首次加载空配置的用户看到空白界面 |
| 核心→UI 刷新 | `std::function` 回调注入 | 核心层不依赖 Qt 信号，保住无头单测；替代 Qt 信号耦合会污染核心层 |
| 命令注册 | 收 `*this` 引用 | 防 magic statics 递归初始化死锁；历史已验证的纪律 |

## 8. 遗留与演进

1. **自定义工具条持久化到用户目录**：当前默认 `sketchcad.cuix` 由 `initCui` 载入，
   后续可对接 `QStandardPaths::writableLocation(AppConfig)` 实现「用户级覆盖系统级」。
2. **按钮图标体系**：`icons::make(id)` 全矢量绘制无 qrc；CUIx 允许 `icon` 字段指向
   图标 id，未来可扩展为「图标集」概念，支持用户换肤。
3. **工作空间快捷切换键**：当前经「界面」菜单切换；可加 `Ctrl+W` 循环或状态栏下拉。
4. **别名与工具条的交互**：`tbadd` 的 `command` 已是「工具 id 或命令名」双形态，
   未来可让别名也作为按钮 command（经 `runToken` 再走一次 `resolve`），进一步统一。
5. **CUIx 导入 AutoCAD 真实 .cui/.cuix**：当前为自有 schema；如需互操作，可在
   `load` 增加「检测 AutoCAD 命名空间并映射」的适配层（结构同构，工作量可控）。

## 9. 实现期踩坑与修正（避雷）

本阶段在「编译 + 自测」双验证环节暴露并修正了若干坑，记录于此供后续维护者避坑：

1. **`save()` 不可声明为 `const`**：`AliasTable::save` / `CuiManager::save` 在写盘后会
   记录 `filePath_`（供 `aliasreload`/`cuisave` 复用路径）。初版误加 `const` 导致
   `filePath_ = target` 触发 `-fpermissive`（丢弃 const 限定）编译错误。修正：
   去掉 `const`——保存本就是「带副作用（记录落盘路径）」的操作，非 const 才是诚实签名。

2. **const 正确性的连带修复**：`CuiManager::hasToolbar` 声明 `const` 却调用非 const
   的 `findToolbar`，触发同样错误。修正：为 `findToolbar` 提供 `const` 重载
   （`const CuiToolbar* findToolbar(...) const` + 非 const 版），契合标准 const-correct
   范式，避免把数据查询函数误标为非 const。

3. **Qt6 `QXmlStreamReader` 拒读带 `encoding` 的 XML 声明**：`CuiManager::save` 初版用
   `writeStartDocument("UTF-8")` 写出 `<?xml version="1.0" encoding="UTF-8"?>`，
   回读时 `QXmlStreamReader` 报 **"Unsupported XML version"** 而 `load` 失败（自测
   `t_cui` 的 XML 往返用例因此变红）。修正：改为 `writeStartDocument()` 省略 `encoding`
   属性——`QXmlStreamWriter` 本就按 UTF-8 写盘，省略后声明变为 `<?xml version="1.0"?>`，
   解析器普遍接受，中文等高字节内容不受影响。这是 Qt6 对 XML 声明更严格的表现，
   配 CUIx 文件时务必留意。

4. **自测 TU 必须显式 include 新头文件**：`t_alias`/`t_cui` 最初未 `#include`
   `AliasTable.h`/`CuiManager.h`，导致「`SketchCAD::AliasTable` 不是成员」式编译错误。
   教训：新增核心单例后，测试 TU 要同步补 include（不能仅靠 `using namespace SketchCAD`）。

5. **GCC `std::vector<bool>` 的 `-Warray-bounds` 误报**：既有 `t_serializeRoundTrip`
   用例用 `e.arcFlags = {false,true,false,false}`（initializer_list 赋值给
   `std::vector<bool>`），在 `-O3` 下触发 GCC 关于 `memmove` 越界的误报。修正：
   改用 `push_back` 逐个写入，避开 `std::vector<bool>` 位打包内部的 `std::copy` 路径。
   该警告与本次功能无关，但会破坏「零警告」基线，故顺带修正。
