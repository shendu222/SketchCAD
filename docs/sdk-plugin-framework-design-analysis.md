# SketchCAD 原生 SDK 与插件框架 — 设计分析

> 版本：1.0　日期：2026-10-01
> 关联文档：`docs/SDK_GUIDE.md`（第三方开发者指南）、`docs/script-host-design-analysis.md`（脚本宿主，同为二次开发基础设施）

---

## 1. 背景与目标

脚本宿主（JS/Python/VBA）解决了"用户级轻量定制"；但对**行业软件厂商**（机械/建筑/电气），脚本在性能、工程化（版本管理/单测/混淆）、能力面上均不足。工业 CAD 的标准做法是提供**原生 SDK**——对标 AutoCAD ObjectARX：第三方用 C++ 编写动态库插件，注入宿主进程，获得与内置功能同等的能力。

**需求五要素**（用户原话对齐）：

1. 稳定的公共 API；
2. 插件接口规范；
3. 版本与依赖说明；
4. 插件注册、加载及生命周期管理机制；
5. 明确第三方开发须遵循的接口契约。

**非目标**（本版不做，规划 ABI 2）：GUI 钩子（菜单/面板注入）、图元自定义类型、撤销栈深度接入、事件订阅总线。

## 2. 总体架构

```
┌────────────────────────── SketchCAD.exe（宿主）──────────────────────────┐
│  MainWindow  ──「插件(&X)」菜单──►  PluginManager（注册/加载/生命周期）      │
│       │                                    │ initialize(&app_)            │
│  SketchModel ◄── SdkModel (IModel) ◄──── SdkApplication (IApplication)    │
│  CommandInterpreter ◄─ registerExternalCommand ─┘   （桥接层，宿主内部）     │
└──────────────────────────────┬───────────────────────────────────────────┘
                               │  虚函数调用（IApplication* 反向访问）
┌──────────────────────────────▼───────────────────────────────────────────┐
│  SamplePlugin.dll（第三方，仅依赖 sdk/include 纯头）                        │
│   IPlugin 实现 + SKETCHCAD_PLUGIN_ENTRY 导出                               │
│   sketchcad_create_plugin() / sketchcad_sdk_abi_version()  ← C 链接契约   │
└───────────────────────────────────────────────────────────────────────────┘
```

**分层对标 ObjectARX**：

| ObjectARX | 本 SDK | 职责 |
|---|---|---|
| `AcRx`（运行时/注册） | `IApplication` + `PluginManager` | 能力入口、命令注册、加载卸载 |
| `AcDb`（数据库） | `IModel` | 图元增删查 |
| `AcEd`（命令） | `SdkCommandFn` + `CommandInterpreter` 外部命令表 | 命令行命令注入 |
| `AcArxApp`（入口） | `sketchcad_create_plugin` 工厂 + 入口宏 | DLL 入口契约 |

## 3. 关键设计决策

### 3.1 ABI 稳定策略（稳定公共 API 的核心）

跨 DLL 边界的 C++ ABI 脆弱点：STL 类型布局、异常传播、堆所有权、vtable 布局。对策逐条落地：

1. **纯 C++ 抽象接口**：`IPlugin/IModel/IApplication` 全部为纯虚类 + 虚析构；新增能力 = 新增接口类（不破坏），改既有虚函数签名/顺序 = ABI 破坏（触发 ABI 版本递增）。
2. **无 STL / 无 Qt 跨边界**：公共头只含 `int/double/bool/const char*` 与 POD（`SdkPoint/SdkElementInfo/PluginInfo`）。字符串一律 UTF-8 `const char*`，由实现方持有生命周期（读接口返回指针保效至"下一次同对象读调用"，见 SDK_GUIDE §5）。
3. **堆所有权归位**：插件实例由插件 `new`（`create_plugin`）、由插件 `delete`（`IPlugin::dispose()` 默认 `delete this`）。宿主只调 `dispose()`，规避跨模块 `operator delete` 不匹配。
4. **异常屏障**：PluginManager 对 `create/initialize/shutdown/dispose` 全部 try/catch，插件异常不会击穿宿主。
5. **运行时 ABI 协商**：加载时解析插件导出的 `sketchcad_sdk_abi_version()` 与宿主 `SKETCHCAD_SDK_ABI_VERSION` 比对，不一致拒绝——宁可拒绝加载，不可带病运行。

### 3.2 插件入口契约（接口规范）

最小契约 = 1 个基类 + 2 个 C 导出符号，用宏一键生成：

```cpp
SKETCHCAD_PLUGIN_ENTRY(Cls)
// 生成：
//   extern "C" SKETCHCAD_PLUGIN_EXPORT int sketchcad_sdk_abi_version();
//   extern "C" SKETCHCAD_PLUGIN_EXPORT IPlugin* sketchcad_create_plugin();
```

`PluginInfo` 要求插件提供**静态存储**元信息（名称/版本/描述/行业分类/ABI 版本），宿主只读不释放——分类（机械/建筑/电气/通用）为后续插件市场/分组预留。

### 3.3 加载与生命周期管理（PluginManager）

```
loadPlugin(path):
  规范化路径 → 去重（已加载拒绝）
  → QLibrary::load（失败→pluginError）
  → resolve("sketchcad_sdk_abi_version") → 校验 ABI（不一致→拒绝）
  → resolve("sketchcad_create_plugin") → 创建 IPlugin*
  → initialize(&app_)（false/异常→shutdown+dispose+unload→pluginError）
  → 登记 LoadedPlugin{path, instance, info, lib} → pluginLoaded 信号

unloadPlugin/unloadAll:
  shutdown() → dispose() → QLibrary::unload → 移出注册表 → pluginUnloaded
  （unloadAll 逆序卸载；析构函数自动调用）
```

- **QLibrary 常驻**：加载成功的 `QLibrary*` 挂在 `LoadedPlugin` 上，插件存活期不卸载；
- **信号上报**：`pluginLoaded / pluginUnloaded / pluginError` 供 UI（日志面板、菜单刷新）解耦联动；
- **去重**：规范化路径（`canonicalFilePath`）入 `QSet`，重复加载拒绝；
- `loadDirectory(dir)` 只认平台后缀（Windows `.dll` / 其他 `.so|.dylib`），启动时扫 `exe 同目录 /plugins`。

### 3.4 命令注入（复用既有命令体系）

`CommandInterpreter` 新增**外部命令表**（`std::map<QString, std::function<bool(const QString&)>>`）：

- `execute()` 在内置注册表（CommandRegistry 精确+前缀查找）落空后**回退**外部命令表——内置优先，插件不能遮蔽内置命令；
- `registerExternalCommand` 拒绝：与内置重名、外部重名、含空格的名字；
- 参数传递：`toks.mid(1).join(' ')`（命令名后剩余文本），SdkContext 包装层转 UTF-8 `const char*` 后调 `SdkCommandFn`；
- `completions()` 同步包含外部命令名 → Tab 补全、命令行体验一致；
- 插件回调访问宿主的方式：命令函数拿到的是 `const char* args`，宿主能力经 `initialize` 时缓存的 `IApplication*` 反向访问（SamplePlugin 的 `g_app` 模式）。

### 3.5 桥接层（SdkContext）

- `SdkModel : IModel`：增删查全部委托 `SketchModel` + `ElementFactory`（与 ScriptApi 同一数据通路，保证脚本/插件/命令行三种入口行为一致）。`elementLayer` 返回值用 `mutable QByteArray layerBuf_` 暂存 UTF-8；
- `SdkApplication : IApplication`：值成员持 `SdkModel`，`registerCommand` 把 C 函数指针包成 `std::function` 注入解释器；版本协商直接读 `sdk_version.h` 宏。
- 一个不易察觉的坑：`SdkModel::elementTypeName(int id)` 成员与全局 `SketchCAD::elementTypeName(ElementType)` 同名遮蔽，实现内必须 `SketchCAD::` 限定。

## 4. 文件清单

| 文件 | 角色 |
|---|---|
| `sdk/include/SketchCAD/sdk_version.h` | ABI/语义版本常量（契约根基） |
| `sdk/include/SketchCAD/sketchcad_sdk.h` | 导出宏（SKETCHCAD_SDK_API / PLUGIN_EXPORT） |
| `sdk/include/SketchCAD/types.h` | SdkPoint / SdkElementInfo（POD） |
| `sdk/include/SketchCAD/i_plugin.h` | IPlugin + PluginInfo（插件接口规范） |
| `sdk/include/SketchCAD/i_model.h` | IModel（数据库接口） |
| `sdk/include/SketchCAD/i_application.h` | IApplication + SdkCommandFn（运行时接口） |
| `sdk/include/SketchCAD/plugin_entry.h` | 入口契约 + SKETCHCAD_PLUGIN_ENTRY 宏 |
| `src/core/SdkContext.{h,cpp}` | 宿主桥接（IApplication/IModel 实现） |
| `src/core/PluginManager.{h,cpp}` | 注册/加载/生命周期管理 |
| `src/core/CommandInterpreter.{h,cpp}` | 扩展：外部命令表 |
| `sdk/samples/SamplePlugin/SamplePlugin.cpp` | 机械行业示例（drawGear/mechHello） |
| `src/ui/MainWindow.{h,cpp}` | 「插件(&X)」菜单、启动自动加载、信号接日志 |
| `tests/SketchSelfTest.cpp` | t_pluginManager（桥接/命令/负例/DLL 全链路） |
| `docs/SDK_GUIDE.md` | 第三方开发者指南（含契约与依赖说明） |

## 5. 构建与发布形态

- `SketchCADSdk` 能力以**头文件库**形态发布（`sdk/include/` 整目录拷贝即可分发）——纯头即契约，无需 import 库；
- `SamplePlugin` 为 SHARED 目标，`PREFIX ""` 输出 `build/plugins/SamplePlugin.dll`（MinGW 默认 `lib` 前缀会破坏"按名查找"约定，已显式关闭）；
- 宿主启动自动加载 `exe 同目录/plugins/`，第三方只需投放 DLL 文件；
- `SKETCHCAD_SDK_BUILD/USE_DLL` 宏为宿主侧导出预留，当前桥接全经虚表，无需导出符号。

## 6. 验证

- 双目标 + 示例插件构建 `BUILD_EXIT=0`，`-Wall -Wextra` 零警告；
- 自测 **49/0**（48 存量 + `t_pluginManager`）：
  - 桥接直测：IModel 六种创建/查询/删除、UTF-8 类型名、版本协商宏；
  - 外部命令：注册/重名拒绝/内置冲突拒绝/命令行触发（args 剥离）/Tab 补全；
  - 负例：坏路径加载拒绝且 `pluginError` 上报；
  - **真实 DLL 全链路**：加载 `plugins/SamplePlugin.dll` → ABI 校验 → initialize → 命令行执行 `drawGear`（默认参数产 16 图元，断言精确计数）→ 重复加载拒绝 → 卸载（图元保留、注册表清空）。
- 行为不变性：脚本宿主/命令行既有用例全数保持通过。

## 7. 已知限制与权衡

1. **同工具链约束**：插件须与宿主同编译器/C++ 运行时（vtable + 堆兼容）。已在 SDK_GUIDE 显式声明，与 ObjectARX 锁 VC 版本同策略。`dispose()` 已消除最尖锐的跨堆 delete 风险，但 vtable 布局仍依赖同编译器族。
2. **能力面收窄**：IModel 只开放 6 种图元创建 + 只读查询 + 删除；未开放图层/块/标注/修改操作。原则：先窄后宽——收窄的 ABI 才是能守住十年不变的 ABI。
3. **无 GUI 钩子**：插件暂不能注入菜单/面板，交互入口统一为命令行命令。
4. **线程模型**：插件命令在主线程命令行解释器内同步执行，未定义多线程访问 IModel 的语义。
5. **卸载后指针悬空**：`shutdown()` 后 `IApplication*` 即失效，契约已写明但无运行时防护（成本/收益不划算）。

## 8. 演进路线（ABI 兼容原则下的扩面）

| 阶段 | 内容 | ABI 影响 |
|---|---|---|
| 1.1 | `IModel` 追加图层查询/活动层设置；`IApplication` 追加 `registerMenuHook`（若加，须**追加到接口末尾**并同步升 minor） | 末尾追加纯虚 = 破坏 ABI；正确做法是**新接口类** `IModel11 : IModel` + ` queryInterface` 式向下转型，或直接递增 ABI 2 |
| 1.2 | 事件订阅（modelChanged/selectionChanged） | 新接口 `IEventSink` |
| 2.0 | 图元自定义类型 / GUI 面板注入 | ABI 2 |

> 结论：本次交付把"五要素"全部落地，且以**纯头契约 + C 链接工厂 + ABI 运行时协商**三件套，把稳定性建立在机制而非纪律上——第三方插件的编译、加载、运行、卸载全链路均有自测背书。

## 9. 增补：ABI 运行时协商与向后兼容（v1.1）

### 9.1 需求映射审计

| 需求 | 落点 | 状态 |
|---|---|---|
| ①插件接口规范（注册/初始化/生命周期/卸载） | `IPlugin`（initialize/shutdown/info/dispose）+ `SKETCHCAD_PLUGIN_ENTRY` 一键入口 + `PluginManager` 加载→初始化→运行→逆序卸载全链 | 既有交付 ✅ |
| ②稳定 ABI 约定（布局/调用约定/内存所有权） | 纯头契约（零 Qt/STL 跨边界）、POD + `const char*` UTF-8、C 链接工厂、`dispose()` 堆所有权归位、异常屏障 | 既有交付 ✅ |
| ③版本兼容机制（版本号/向后兼容/运行时能力协商） | 原为「严格相等」校验 → **本轮升级为协商区间**（见 9.2） | 本轮补齐 ✅ |
| ④接口演进不破坏既有插件 | `sdk_version.h` 追加式演进承诺 + 9.3 演进纪律 | 本轮文档化 ✅ |

### 9.2 协商策略设计（为什么用区间而不是相等）

原实现「插件 ABI ≠ 宿主 ABI 即拒绝」在 v1 时代与协商等价，但它把**向后兼容
承诺写死成了不兼容**：宿主升到 ABI 2 的那一刻，所有 v1 插件立即全部失效——
这恰恰是需求要求避免的。本轮把判定改为区间协商：

```
插件声明 pluginAbi（编译所依据的 SDK_ABI_VERSION）
宿主持有区间 [SDK_ABI_MIN_SUPPORTED=1, SDK_ABI_VERSION=1]

pluginAbi ∈ 区间        → 接受；pluginAbi < 宿主时按「兼容模式」加载并记录协商结果
pluginAbi >  宿主 ABI   → 拒绝："ABI 版本过新：插件=99，宿主=1（请升级宿主程序）"
pluginAbi <  区间下限    → 拒绝："ABI 版本过旧或非法：插件=0（宿主支持区间 1..1）"
```

**区间为什么可行**——两条硬承诺（写进 `sdk_version.h` 头注释，与代码同生命周期）：
1. 接口 vtable 只允许「末尾追加槽位」，且追加必须通过**新接口类**完成
   （旧接口类永不修改）——旧插件按旧 vtable 布局调用槽位 k，宿主 vtable 前 k 个
   槽位恒等，调用天然兼容；
2. POD 结构体字段只增不改不删，新增字段追加到尾部。

协商发生在解析工厂符号**之前**（先验版本、后调 create），因此被拒插件
的任何代码都不会在宿主进程内执行。

### 9.3 未来演进的操作手册（避免破坏既有插件的清单）

| 变更类型 | 允许？ | 正确做法 |
|---|---|---|
| 接口类**末尾追加**纯虚函数 | ❌（对插件实现的接口 = 槽位缺失 = 崩溃） | 新接口类 `IFoo2 : IFoo`，宿主以 `dynamic_cast`/查询口暴露；或整体升 ABI 2 |
| 修改虚函数签名/顺序/删除 | ❌ 永不 | 同上 |
| POD 结构体尾部追加字段 | ✅（约定新字段带默认值，旧插件 memset/值初始化不受影响） | 同步递增语义版本 minor |
| POD 字段改类型/顺序/删除 | ❌ 永不 | 新结构体 + 新接口方法 |
| 新增接口类 / 新导出函数 | ✅ 天然兼容（旧插件不引用新符号） | 递增语义版本 minor |
| 字符串编码（UTF-8）/坐标约定（mm/度） | ❌ 永不改——这是数据语义契约 | — |
| 确需废弃旧接口 | 保持 `MIN_SUPPORTED` 不动若干版本 → 废弃期（加载仍接受、日志警告）→ 上调 `MIN_SUPPORTED` | 三段式退役 |

宿主侧仅需维护两个常量：`SDK_ABI_VERSION`（当前能力上限）与
`SDK_ABI_MIN_SUPPORTED`（承诺兼容下限）。区间宽度即「旧插件的兼容窗口」，
由宿主按废弃节奏主动控制，而不是让插件作者被动跟着宿主升级。

### 9.4 运行时协商的验证方式

* **正向**：`SamplePlugin.dll`（ABI=1 = 宿主 ABI）全链路加载→命令执行→卸载（既有用例）。
* **负向（本轮新增）**：`AbiProbePlugin.dll` —— 探针插件导出 `sketchcad_sdk_abi_version()=99`
  且**刻意不导出** `sketchcad_create_plugin`，验证两点：①过新插件被拒且错误消息
  报告双方版本（断言含 "99" 与 "过新"）；②拒绝发生在工厂解析之前（若宿主顺序错误
  会因缺符号走另一错误分支，断言消息含 "过新" 即可甄别）。探针输出到独立目录
  `build/abiprobe/`，避免被宿主启动扫描误加载。
* **结果**：自测 50/0（探针用例嵌入 `t_pluginManager` 第 5 段），三目标构建零警告。
