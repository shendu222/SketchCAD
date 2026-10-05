# AutoCAD 交互对齐——设计分析（interaction parity）

> 任务：消除"会用 AutoCAD 但在这里点不惯"的摩擦，按清单逐项对齐。
> 基线：全量自测 1126/0 → **1152/0**，双目标零警告，`SketchCAD`/`SketchSelfTest` 构建 EXIT=0。

---

## 一、改动边界

- **不修改命令语义本身**：所有命令行为、撤销、序列化、DXF/DWG/JSON 格式零改动。
- 快捷键冲突处理遵循"**新增可配置、默认取 AutoCAD 值**"：新增 AppSettings 键
  `interaction/rightClickIdleRepeat`（默认 true = AutoCAD 约定），既有右键菜单行为可一键切回。
- 既有流程保持：绘制中右键=结束/取消、右键拖拽平移、Shift+右键临时捕捉覆盖等原样保留。

## 二、现状勘察（哪些其实已经有了）

勘察结论：清单中过半项目在既有代码里已实现，本次只补缺口——

| 清单项 | 现状 | 位置 |
|---|---|---|
| 中键拖拽平移 / 滚轮缩放 | ✅ 已有（保持） | GraphicsView mousePressEvent / wheelEvent |
| 右键=确认（绘制中） | ✅ 已有：右键按下进平移预备态，未移动的单击走 `handleRightButton` → 结束/取消绘制 | mouseReleaseEvent:wasClick → handleRightButton |
| Esc 层层取消 | ✅ 已有：夹点热态 Ctrl/Enter/Esc → regionPickMode → zoomBoxMode → pointPickMode 分层链 | keyPressEvent |
| Shift+右键 = OSNAP 快捷菜单 | ✅ 已有：14 种捕捉类型临时覆盖（单次有效，放置一点自动恢复） | showSnapOverrideMenu / snapFieldDefs() |
| 空格/Enter 重复上一命令 | ✅ 已有（命令行空输入） | CommandLine::onReturn / keyPressEvent |
| ↑/↓ 翻命令历史 | ⚠️ 已有但仅内存（DocTab 会话态），**重启丢失** → 本次持久化 | CommandInterpreter::history_ |
| Tab 前缀补全 | ⚠️ 已有算法，本次补自测锚定 | CommandLine keyPressEvent + QCompleter |
| 未知命令建议 | ⚠️ 已有但只给 **1 个**候选 → 本次扩为 3 个 | CommandInterpreter::execute |
| 双击行为 | ⚠️ 只有 Text/MText/Table → 本次扩标注/块/其他 | mouseDoubleClickEvent |
| Ctrl+右键捕捉/追踪快捷项 | ❌ 缺失 → 新增 | mousePressEvent |
| 空闲态右键=重复上一命令 | ❌ 缺失（现为上下文菜单）→ 新增可配置 | handleRightButton 空闲分支 |
| Tab 在动态输入框间切换字段 | ❌ 缺失 → 新增 | DYN dynLen_/dynAng_ |

## 三、核心决策

### 1. 空闲态右键行为：配置化，默认 AutoCAD 约定
- AppSettings 新键 `interaction/rightClickIdleRepeat`（bool，默认 true）。
- **值经 `MainWindow::applySettingLive` 写入 `SketchModel::rightClickIdleRepeat_` 会话态**（不入档、
  不入撤销栈，与 commandOptions_/showLineWeight_ 同惯例）；`hookActiveDoc` 换绑时对新活动模型重放，
  保证多文档/新建文档后设置一致。
- GraphicsView 空闲态读 `model_->rightClickIdleRepeat()`：true → 发 `repeatLastCommandRequested()`
  信号；false → 既有 `showContextMenu`。**主窗口侧连接到 `CommandLine::repeatLastCommand()`**——
  与空输入回车/空格重复共用同一实现（提取为公共槽），保证历史游标 `histIdx_` 同步、避免三处复制。
- 无上一命令时回显"没有可重复的命令"。

### 2. 未知命令 3 候选（Did you mean 中文）
- `nearestCommandName`（返回 1 个）升级为公有静态 **`CommandInterpreter::nearestCommands`
  （≤3 个，编辑距离升序、同距离保持注册顺序，距离阈值 len/2 下限 2 不变）**。
  公有静态便于无头自测直测与未来复用（状态栏提示等）。
- 日志文案：`未知命令: xxx（您是不是想输入: a / b / c？输入 help 查看全部）`。

### 3. 命令历史持久化（重启可用）
- AppSettings 新键 `commandLine/recentCommands`（QStringList，stringListVal 校验器，默认空）。
- `CommandInterpreter::setPersistence(AppSettings*)` 注入式接入：
  - 注入时**仅当会话历史为空**才预填持久化内容（不覆盖 DocTab copy-in 的会话态——多文档
    各自历史独立的既有语义不破坏）；
  - 此后每次 `addHistory` 同步写回尾部 ≤50 条（`kPersistMax`；内存会话历史仍 100 条）。
- 装配点在 `setupAutoSave`（appSettings_ 创建之后；离线自测经 `s_recentPersistEnabled` 隔离，
  与 RecentFiles 同一开关语义）。
- 解释器是共享单件 + 历史属文档会话态（copy-out/copy-in），持久化层只保证"**最新命令全局可达、
  重启后首个文档可翻**"，不承诺逐文档历史恢复。

### 4. Ctrl+右键 = 捕捉/追踪快捷菜单
- `showTrackingQuickMenu`：勾选项直写 model_ 单一状态源——正交(F8)、栅格捕捉、极轴追踪、
  对象捕捉追踪（后两项经 `setTrackConfig`）；底部分隔线后附"捕捉设置 (Shift+右键 = 临时覆盖)"
  复用既有覆盖菜单。
- 与快捷键/状态栏/命令 ortho/snap 三入口同源（SketchModel 广播信号同步其余入口）。
- 任何时刻可用（全局输入约束开关不受"是否绘制中"限制）；不触发平移。

### 5. 双击行为矩阵（AutoCAD 约定）
| 双击对象 | 行为 | 通路 |
|---|---|---|
| Text / MText / Table | 就地编辑内容（既有） | `editTextRequested(id)` |
| Dimension / MLeader / Fcf | 就地编辑覆盖文本（留空恢复测量值） | `editTextRequested(id)` → editElementText 新分支（QInputDialog 多行输入 + beginEdit） |
| BlockRef | 进入在位编辑（BEDIT，改定义→全部引用同步） | 新信号 `blockEditRequested(id)` → `openBlockEditorDialog(const QString&)` 重载 |
| 其它图元 | 打开属性面板 | `requestShowProperties()`（既有信号复用） |

- 块编辑器入口重构：原 `openBlockEditorDialog()` 的"选块解析"尾部落到新重载
  `openBlockEditorDialog(const QString&)`（校验 hasBlock → BlockEditorDialog），菜单/双击两入口同源。
- poly/spline/fence/wp/cp 绘制中的"双击=结束采集"既有语义**保持在最前**，优先于新矩阵。

### 6. DYN 动态输入框 Tab 字段切换
- `ensureDynBox` 对 dynLen_/dynAng_ `installEventFilter(this)`；`eventFilter` 处理
  Key_Tab/Key_Backtab 在"距离/角度"两输入框间循环并 `selectAll`，吃掉事件阻止焦点逃逸
  （QLineEdit 默认 Tab 会把焦点交还主窗口）。仅 dynBox 可见时生效，不影响全局 Tab。

### 7. 自测基础设施：main 换 QApplication
- t_interaction 要无头构造 `CommandLine`（QLineEdit 系），必须 QApplication；自测目标已链
  Qt6::Widgets，`src/ui/CommandLine.cpp` 仿 DocTab 先例编入 SketchSelfTest。
- 连锁回归与修复：
  - `TextMetrics::isGuiAvailable` / Clipboard 门禁都以 `qobject_cast<QGuiApplication*>` 判"无头"，
    换 QApplication 后打破 t_textMetrics 全组估算锚定断言与剪贴板 C18。
  - **修复**：TextMetrics 增加测试专用 `setHeadlessOverride(bool)`（inline static 开关，主程序从不
    调用、行为零变化）；t_textMetrics 开头开启、结尾还原——无头降级路径的回归能力 100% 保留。
  - Clipboard C18 改环境自适应：无 GUI 断言安全 false；有 GUI 仅断言调用不崩溃。

## 四、改动清单

| 文件 | 改动 |
|---|---|
| `src/core/AppSettings.h/.cpp` | 新键 `kRecentCommands`、`kRightClickIdleRepeat` + spec 条目 + using 声明 |
| `src/core/CommandInterpreter.h/.cpp` | `nearestCommands`（3 候选）、`setPersistence`/`persistedCommands`/`kPersistMax`、addHistory 写回、include AppSettings |
| `src/core/SketchModel.h` | 会话态 `rightClickIdleRepeat_`（默认 true）+ getter/setter（幂等直赋，不入档） |
| `src/core/TextMetrics.h/.cpp` | `setHeadlessOverride` 测试开关（inline static，主程序零变化） |
| `src/ui/GraphicsView.h/.cpp` | 信号 `repeatLastCommandRequested`/`blockEditRequested`；Ctrl+右键 `showTrackingQuickMenu`；空闲态右键配置分支；双击矩阵扩展；DYN Tab `eventFilter` |
| `src/ui/CommandLine.h/.cpp` | 公共槽 `repeatLastCommand()`（空回车/空格/右键三入口共用） |
| `src/ui/MainWindow.h/.cpp` | `openBlockEditorDialog(const QString&)` 重载；editElementText 标注文本分支；两新信号连接；applySettingLive/hookActiveDoc/loadAppSettingsOnStartup 接 `kRightClickIdleRepeat`；setupAutoSave 装配 `setPersistence` |
| `CMakeLists.txt` | SketchSelfTest 增编 `src/ui/CommandLine.cpp` |
| `tests/SketchSelfTest.cpp` | main → QApplication；新增 `t_interaction`（25 条 CHECK）；t_textMetrics 开关包裹；C18 环境自适应 |

## 五、自测设计（t_interaction 25 条，≥12 达标）

- **A 未知命令容错**：IA1 最相近者排首且 ≤3；IA2 阈值外返回空；IA3 execute 日志含
  "您是不是想输入"与候选 circle（logAppended 信号捕获）。
- **B 历史持久化**：B1 入史即写回 + 模拟重启预填顺序一致；B2 封顶 50 且保留最新尾部；
  B3 会话态优先（持久化预填不覆盖 setHistory 的 copy-in 历史）。
- **C CommandLine 键盘语义（QApplication::sendEvent 直注）**：
  C1a 唯一候选补齐 `"linet"`→`"linetype "`（含尾空格）；C1b 多候选公共前缀 `"line_p"`→
  `"line_parallel"`；C1c 歧义前缀（命中 line 系 + 别名 lin→linetype）不退化；C2 空格空输入
  重复上一命令且入史（help 执行后 history=={"help"}）；C3 ↑ 逐条上翻 / ↓ 到末尾清空；
  C4 绘制中坐标输入 → `pointEntered(10,20)` 注入；C5 空历史键入安全。
- **D 配置与校验**：D1 rightClickIdleRepeat 默认 true / 写 false 生效 / 非法写被忽略
  （保持上次合法值）；D2 SketchModel 会话态默认与 setter；D3 recentCommands 列表往返。

## 六、验收对照

| 验收标准 | 结果 |
|---|---|
| t_interaction ≥12 条，每类手势/按键触发正确行为 | ✅ 25 条（键盘事件经 sendEvent 直注、无头 CommandLine 实例） |
| 与既有流程不冲突；既有自定义右键行为可切换、默认 AutoCAD 约定 | ✅ `interaction/rightClickIdleRepeat` 默认 true，false 恢复上下文菜单 |
| 既有断言全通过 | ✅ 1126 → **1152/0**（净增 26：t_interaction 25 + C18 合并后差值 1） |
| 双目标零警告、build.sh test EXIT=0 | ✅ BUILD_EXIT=0、WARN=0、SELFTEST_EXIT=0 |
| 不改命令语义 / 新键默认取 AutoCAD 值 | ✅ 全部为新增通路；既有绘制中右键/Shift+右键/Esc 分层零改动 |

## 七、踩坑与已知边界

### 踩坑
1. **main QCoreApplication → QApplication 的隐性连锁**：`TextMetrics`/`Clipboard` 以
   `qobject_cast<QGuiApplication*>` 作"无头"判据——QApplication 正是其子类，导致两组测试
   假设崩塌。根修是给 TextMetrics 加显式覆盖开关（而非改判据语义，避免影响主程序行为）。
2. **AppSettings using 声明遗漏**：specTable 用非限定键名，文件采用逐键 `using SettingKeys::kXxx;`
   模式，新键忘记补 using → "was not declared in this scope"（编译期即拦，未入库）。
3. **Tab 补齐的"歧义前缀"**：别名 `lin`（→linetype）参与候选后，"lin" 的公共前缀==输入本身，
   补齐不推进——这是算法正确行为（用户可能想输别名也可能想输 line 系），测试按三档语义
   （唯一/多候选推进/歧义不退化）重新锚定，勿再断言"必须到 line"。
4. **QSettings 非法写回落语义**：`setValue` 校验失败=静默忽略（不写存储），读回的是**上次合法
   值**而非默认值——D1 断言曾写错方向。
5. **C++17 inline static 成员**：头文件内联 setter 需直写的状态必须在头文件内
   `inline static`，放 cpp 文件静态会得到 "not declared in this scope"。

### 已知边界
- 右键重复的游标同步：经 `CommandLine::repeatLastCommand` 公共槽已同步 `histIdx_`；
  若未来新增第四条重复入口，务必复用该槽。
- 持久化历史为全局单份（重启后预填到首个文档）；逐文档历史仍为会话态，不持久化。
- 双击标注就地编辑用多行输入框（QInputDialog），与 MText 编辑器视觉不同；留空=恢复测量值。
- Ctrl+右键菜单当前只含四个全局开关 + 捕捉覆盖入口，未含"捕捉设置面板"对话框（既有面板
  入口在状态栏/菜单，避免菜单内二次 exec 嵌套）。
- DYN Tab 切换在 dynBox 隐藏时不生效（eventFilter 有 isVisible 守卫）。
