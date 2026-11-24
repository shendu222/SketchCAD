# SketchCAD 重构与界面布局优化：技术分析与决策依据记录

> 文档日期：2026-11-23  
> 适用基线：本轮重构后的源码  
> 文档性质：可公开的技术分析、模式选择依据与变更记录。本文记录工程决策及其理由，不包含任何内部推理轨迹。  
> 目标：在不重写超大文件的前提下，按 DRY 原则抽取重复逻辑，合理应用 GoF 23 种设计模式中有真实痛点的部分，并提升布局一致性与跨屏幕适配。

---

## 1. 目标与边界

### 1.1 目标

1. 找出重复出现且语义稳定的代码形态，抽取为可复用模块。
2. 让模式选择服务于具体问题，不为凑齐模式而引入无意义抽象。
3. 统一界面构造参数，使控件对齐、间距、信息层级在不同屏幕下保持一致。
4. 保持现有命令语义、交互习惯、快捷键与存档格式不变。

### 1.2 明确不做

- 不整体拆分 `MainWindow.cpp` 的 `buildMenu()` / `buildToolbar()`：它们与语言重建、插件菜单、面板动作深度耦合，整体重写风险高于收益。
- 不改写 `ElementType`、存档字段、命令注册语义。
- 不把 GPL 转换器链入主程序。
- 不为了覆盖全部 23 种模式而制造抽象层。

---

## 2. 现状热点分析

### 2.1 QAction 构造重复

`MainWindow.cpp` 中曾存在三份等价动作构造样板：

- `buildMenu()` 的 `mk`：菜单文本 → 快捷键 → `ApplicationShortcut` → `connect`。
- `buildBlockMenu()` 的 `add`：同样流程，额外设置图标；源码注释已承认“复用 mk 不便”。
- `buildToolbar()` 的 `addEx`：新建 `QAction` → 提示文本 → 快捷键 → 上下文 → `connect` → 登记 `toolActions_`。

问题：

- 新增动作时容易漏设快捷键上下文或提示文本。
- 三处逻辑漂移会导致菜单项与工具条项行为不一致。
- 快捷键冲突自检依赖统一的 action 构造口径。

### 2.2 画笔配方重复

`GraphicsView.cpp` 中：

- 尺寸标注、多重引线、形位公差框格三处重复构造“细实线”画笔。
- 对象追踪、极轴、正交引导线三处重复构造“虚线 cosmetic”画笔。

问题：

- 线宽系数、cosmetic、style 的调用顺序散落，视觉一致性依赖人工记忆。
- 后续要调整细线规则时必须改多处，违反 DRY。

### 2.3 标量参数解析重复

`Commands.cpp` 中：

- `arrayrect` 连续 `toInt/toDouble` 四次并统一校验。
- `list` / `area` 等查询命令重复 `toInt(&ok)` 循环与“不存在”错误分支。

对照：点坐标解析已有 `CommandContext::parsePoint/tryParsePoint` 统一入口，但标量参数没有同类入口。

### 2.4 布局参数分散

界面尺寸曾散落在：

- `applyToolBarLook()`：图标 20×20、边距 `6/2`、间距 2、按钮最小宽 64。
- `buildParamBar()`：边距 `8/4`、间距 8、角度框 80、数值框 90、线型框最小宽 120。
- `buildDocks()`：左 240 / 右 300 的固定初始比例。

问题：

- 1280 宽屏幕下工具条容易触发 `>>` 溢出。
- 参数条固定宽度会把主窗口最小宽推大。
- 没有统一的“紧凑/舒适”档位，适配只能靠零散特判。

---

## 3. 本轮重构内容

### 3.1 新增可复用模块

| 模块 | 文件 | 解决的问题 | 设计要点 |
|---|---|---|---|
| 动作工厂 | `src/ui/ActionFactory.h` | 三份 QAction 构造样板 | `ActionSpec` 链式配置 + `addMenuAction` / `addToolBarAction` / `configureAction` |
| 界面密度 | `src/ui/UiMetrics.h/.cpp` | 布局参数分散、窄屏溢出 | 以 1440px 为阈值，统一输出工具条/参数条/停靠面板尺寸 |
| 画笔配方 | `src/ui/PenRecipes.h/.cpp` | 细实线与引导线重复构造 | `thinOutline()`、`guide()` 两个纯函数 |
| 命令参数解析 | `src/core/CommandArgParser.h` | 标量参数校验重复 | `parseRectArray()`、`parseIds()` 纯函数，错误文案仍由调用点给出 |

### 3.2 改造的调用点

| 文件 | 改动 |
|---|---|
| `src/ui/MainWindow.cpp` | 三处动作构造改用 `ActionFactory`；工具条/参数条/停靠面板改用 `UiMetrics`；新增 `resizeEvent()` 与 `applyResponsiveLayout()`。 |
| `src/ui/MainWindow.h` | 声明响应式重排方法与密度状态。 |
| `src/ui/GraphicsView.cpp` | 6 处派生画笔改用 `PenRecipes`。 |
| `src/core/Commands.cpp` | `arrayrect`、`list`、`area` 改用 `CommandArgParser`。 |
| `CMakeLists.txt` | 将 `UiMetrics.cpp`、`PenRecipes.cpp` 加入 `UI_SOURCES`。 |

### 3.3 保持的行为

- 命令名、参数顺序、错误文案不变。
- 菜单顺序、工具条分组、面板停靠位置不变。
- 撤销/重做、自动保存、文件版本选择不变。
- 画布中央区仍是视觉重心，`viewStack_` 拉伸因子仍为 1。

---

## 4. 设计模式清单与理由

> 结论：本轮“采用或强化”13 种；“保留现状、不新增抽象”4 种；“明确不采用”6 种。  
> 判断标准只有两个：是否消除真实重复；是否让变化点隔离得更清楚。

| # | 模式 | 结论 | 场景 / 解决的问题 |
|---|---|---|---|
| 1 | Abstract Factory（抽象工厂） | 不采用 | 没有“成族替换”的对象需求；引入只会增加一层空接口。 |
| 2 | Builder（建造者） | 已采用 | `ActionSpec` 以链式调用组装 QAction 的图标/快捷键/提示/可选状态，避免多参数构造函数与调用点顺序错误。`SheetFrameOptions` 也是既有同类实践。 |
| 3 | Factory Method（工厂方法） | 已采用/强化 | `ActionFactory::addMenuAction()` / `addToolBarAction()` 集中创建并配置动作；`ElementFactory` 已是图元创建单一入口。 |
| 4 | Prototype（原型） | 保留现状，不新增 | 剪贴板与图元复制已有按值复制语义；当前无克隆体系漂移，无需再抽象。 |
| 5 | Singleton（单例） | 已存在，保留 | `CommandRegistry`、`AliasTable`、`CuiManager`、`StandardsDB`、`ThemeManager` 等需要全应用唯一状态源。 |
| 6 | Adapter（适配器） | 已存在，保留 | `DwgBridge` 把进程外转换器适配为内部格式通道；`ModelMediator` 也承担 UI 与模型间接口适配的一部分。 |
| 7 | Bridge（桥接） | 不采用 | 当前没有稳定抽象与多变实现分离的需求；格式层已用 `CadFormats` 门面收敛。 |
| 8 | Composite（组合） | 已存在，保留 | 菜单树、工具条下拉、QGraphicsScene 图元树天然是组合结构；统一 action 工厂让组合节点构造更一致。 |
| 9 | Decorator（装饰器） | 不采用 | 工具条外观变化通过 QSS 与集中度量处理，不需要运行时层层包装对象。 |
| 10 | Facade（门面） | 已存在，保留 | `CadFormats` 统一 JSON/DXF/DWG/STEP；`ConstraintSolverFacade` 统一求解器路由。新增模块保持小接口，不滥用门面。 |
| 11 | Flyweight（享元） | 不采用 | 图标与线型资源由 Qt 共享隐式管理；没有海量细粒度对象需要共享状态。 |
| 12 | Proxy（代理） | 不采用 | 无远程/懒加载/权限代理需求；Xref 是引用语义但不是 GoF Proxy 场景。 |
| 13 | Chain of Responsibility（责任链） | 已存在，保留 | `CommandInterpreter` 依次尝试精确命令、前缀族、外部命令、相近建议；新增帮助提示未改变该链。 |
| 14 | Command（命令） | 已存在，保留 | `ICommand` 及 `Commands.cpp` 注册类把请求封装为对象，使菜单、命令行、宏、脚本共用同一处理入口。 |
| 15 | Interpreter（解释器） | 已存在（有限形态） | `CommandLine` + `CommandInterpreter` 解析内部命令 DSL；未扩展为通用语法解释器，避免误用。 |
| 16 | Iterator（迭代器） | 不采用 | STL/Qt 容器迭代器已足够；没有自定义聚合遍历算法。 |
| 17 | Mediator（中介者） | 已存在，保留 | `ModelMediator` 收敛模型↔视图↔面板↔状态栏联动，避免面板互相直连。 |
| 18 | Memento（备忘录） | 已存在，保留 | 撤销/重做快照保存编辑前状态，公开修改命令通过 `beginEdit` 形成单步撤销。 |
| 19 | Observer（观察者） | 已存在，保留 | Qt signals/slots 承载模型变化、主题变化、文档切换广播。 |
| 20 | State（状态） | 已存在/强化 | 当前活动工具、正交/捕捉/追踪开关、本轮新增的布局密度档位，都是明确状态；`applyResponsiveLayout()` 只在状态切换时动作，避免重复重排。 |
| 21 | Strategy（策略） | 已存在/强化 | `ICommand`、工具策略与 `UiMetrics` 的密度参数可视为可替换策略：同一界面流程在不同密度下采用不同尺寸集合。 |
| 22 | Template Method（模板方法） | 已存在，保留 | `CommandCatalog::entries()` 固定“构建表项 → 翻译 → 缓存”的骨架，具体条目由 `add()` 注入；动作工厂也固定“创建→配置→连接”的流程。 |
| 23 | Visitor（访问者） | 不采用 | 图元类型稳定且操作分散在命令/渲染/格式层；引入 Visitor 会迫使所有 Element 增加 Accept，收益不足。 |

---

## 5. 界面布局优化说明

### 5.1 信息层级

- 中央画布保持最大拉伸空间，工具条与参数条只占固定高度。
- 左侧面板（BOM / 图元 / 图层）和右侧面板（属性 / 约束 / 执行信息 / 合规）保持原有分区语义，不改变用户肌肉记忆。
- 命令行仍停靠底部，状态栏仍为常驻信息带。
- 窄屏时优先压缩左右侧栏与参数控件，不压缩中央画布。

### 5.2 控件对齐

- 工具条按钮最小宽度从单一常量 64 改为 `UiMetrics::toolButtonMinWidth()`：
  - 舒适档：64 px，保持原观感。
  - 紧凑档：56 px，减少溢出概率。
- `ActionFactory::configureAction()` 统一设置图标、提示、快捷键上下文，避免工具条与菜单同类项口径不一致。

### 5.3 间距协调

| 区域 | 舒适档 | 紧凑档 |
|---|---:|---:|
| 工具条图标 | 20×20 | 18×18 |
| 工具条左右边距 | 6 | 3 |
| 工具条上下边距 | 2 | 1 |
| 工具条控件间距 | 2 | 1 |
| 参数条左右边距 | 8 | 4 |
| 参数条上下边距 | 4 | 2 |
| 参数条控件间距 | 8 | 5 |

### 5.4 不同屏幕适配

- 阈值：`availableWidth < 1440` 进入紧凑档。
- 参数输入条：
  - 角度框 80 → 72。
  - 长度/间隔框 90 → 78。
  - 线型下拉最小宽 120 → 104。
- 停靠面板初始比例：
  - 左栏 240 → 200。
  - 右栏 300 → 260。
- 新增 `MainWindow::resizeEvent()` 与 `applyResponsiveLayout()`：
  - 只在“紧凑/舒适”档位变化时重排。
  - 同档位内不覆盖用户手动拖拽，不产生 resize 递归。
- `showEvent()` 在首次显示并校正窗口几何后应用一次响应式布局。

### 5.5 视觉重心

- 参数提示标签继续保留 `stretch=1`，大屏充分展开、窄屏压缩。
- 中央区边距与间距保持 0，`viewStack_` 拉伸因子保持 1。
- 紧凑档收缩的是周边控件，而不是绘图区。

---

## 6. 验证记录

### 6.1 已完成

- IDE 诊断：对全部改动文件读取诊断，未发现 error / warning。
- 语法编译校验通过：
  - `src/ui/UiMetrics.cpp`
  - `src/ui/PenRecipes.cpp`
  - `src/ui/MainWindow.cpp`
  - `src/ui/GraphicsView.cpp`
  - `src/core/Commands.cpp`
- CMake 配置已重新生成，新增两处 `.cpp` 被纳入 `UI_SOURCES`。

### 6.2 未完成项与原因

- 整包增量构建在 `sketchcad_dwgbridge_autogen` 的 AutoMoc `moc_predefs.h` 生成阶段失败。
- 失败发生在与本轮源码改动无关的 DWG 桥目标，且单独执行同一条编译器宏展开命令可成功返回。
- 曾尝试清理该 autogen 缓存后重试，但该操作需要额外审批且未获执行；因此本轮未做最终链接验证。
- 建议在有完整工具链授权的环境中执行 `build.sh` 或 `cmake --build build` 完成最终回归。

---

## 7. 后续建议

1. 将 `CommandDialogs` 作为第二阶段抽取对象：先收敛同过滤器文件对话框，再处理符号插入中的批量输入。
2. 为 `ActionFactory` 增加快捷键冲突自检的单测，复用现有 `ShortcutAudit`。
3. 把 `UiMetrics` 的阈值与尺寸写入选项页，允许用户选择“紧凑/舒适/跟随系统”。
4. 在 `PenRecipes` 上补离屏渲染比对，确保主题切换下细实线颜色与宽度一致。
5. 若引入 Ribbon，可在不改 `ActionFactory` 调用点的前提下把同一批动作复用到 Ribbon 面板。
