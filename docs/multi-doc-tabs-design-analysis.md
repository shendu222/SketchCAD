# 多文档标签体系（多图对照 / 独立会话 / 跨文档粘贴）设计分析

> 需求：支持同时打开多张图纸并快速切换，满足"对照两张图"的真实作业方式。
> 状态：已实现。自测 t_multidoc 10 组 33 条，全量 **1093/0**，双目标零警告，BUILD_EXIT=0。

---

## 0. 改动边界（硬约束）

- `SketchModel` 的 JSON 存储格式**零改动**（`toDrawingJson` / `fromDrawingJson` 原样）；
- **不得引入第二个模型真相源**：每个 `DocSession` 各持一份 SketchModel（parent 到会话，随会话销毁），禁止全局单例模型；
- 既有单文档流程（open/save/saveAs/new JSON）行为不变：`readDrawing`/`writeDrawing`/`applyNewDrawing` 原样复用，只是调用点从"唯一模型"换到"活动文档的模型"。

---

## 1. 现状勘察结论

1. **构造即绑定**：`MainWindow` 构造函数 `new SketchModel` 一次，`model_`/`view_` 被当作"唯一模型/唯一视图"，散布在 4858 行代码的数十个 connect 与消费者中——任何"把 model_ 改成可换"的方案若要求改写全部引用点，风险不可控。
2. **ModelMediator 已收口信号网**：中介者集中登记了 model↔statusBar/panels/view 的约 20 条 connect，但 wire 一次即终局，无换绑能力；且 `MainWindow::connectSignals` 里仍有部分直连 model_/view_ 的信号（状态栏开关组、选项显示、约束栏、自动保存等）散落各处。
3. **会话态天然内嵌 SketchModel**：undo/redo 双栈、选择集、UCS、活动工具（commandOptions_）全部是模型成员 ⇒ "每文档一份模型"即自动获得"撤销栈/选择集/坐标系/活动工具随文档切换"，无需额外搬运逻辑。
4. **剪贴板已是纯 core 可测组件**：`Clipboard::Payload{base, elements, blocks}` + `toJson/fromJson` 版本化 JSON，跨文档粘贴天然可复用，同一 payload 写入多文档内容必然一致。
5. **自测不构造 MainWindow**（无头环境）：多文档核心逻辑必须抽成**不依赖 ui 的纯会话层**才能离线直测。

## 2. 核心设计决策

### 2.1 当前指针交换（Current-Pointer Swap）——零重写的多文档化

`MainWindow::model_`/`view_` 从"唯一模型/唯一视图"降格为**"活动文档的借用指针"**：

```
切换文档 = 4 步指针赋值 + 一次 hookActiveDoc()（换绑全部连接与消费者）
```

全部既有代码继续读 `model_`/`view_`，**4858 行零改动**；单一真相源不变——只是真相源从"构造时 new 的那一个"变成"活动 DocSession 持有的那一个"。

### 2.2 纯会话核心 DocTab（离线直测）

新建 `src/ui/DocTab.{h,cpp}`，**仅依赖 core**，同时进 SketchCAD 与 SketchSelfTest 两个目标：

- **`DocSession : QObject`**：构造即 `model_ = new SketchModel(this)`，并连接 `modelChanged → setDirty(true)`（脏标记自动跟踪）；成员 `filePath / cmdHistory / lastCommand / untitledIndex`；`title()` 规则：未存盘→"绘图N"、已存盘→`completeBaseName`，脏追加 " *"；`markClean/markDirty` 槽幂等（状态不变不发信号）。
- **`DocManager : QObject`**：`addDoc/addNewDoc`（untitledSeq_ 自增、首张自动活动）、`cycle(step)`（`((active_+step)%n+n)%n` 双向循环、纯读不改状态）、`reorder(from,to)`（QList::move + 活动身份重映射）、`removeDoc(i)`（活动/非活动两路收缩）、`isDirty/markClean/markDirty(i)`；信号 `docAdded/docRemoved/activeChanged/dirtyChanged(i)/orderChanged`。

纯数据结构语义（cycle/reorder/remove 收缩）不涉及任何 Qt 控件 ⇒ 自测可在无头环境全量覆盖。

### 2.3 连接清单对称收口（hookActiveDoc 一进一出）

散落的直连全部收口为两张清单：

- `modelConns_`：unitsChanged、ortho/gridSnap/track 开关组（状态栏 + 工具栏按钮回显）、commandOptionsChanged、selectionChanged→属性坞门控、constraintsChanged、modelChanged→constraintBar、modelChanged→autoSave noteOperation（含 recovering_ 门控）等；
- `viewConns_`：sceneStatsChanged、requestShowProperties、regionPick×4、canvasPointPicked、pointPickCancelled、editTextRequested。

`hookActiveDoc()` 流程：**先 disconnect 两张清单全部再 clear → mediator_->rewire(...) → 重连全部登记进清单 → 消费者 setModel 换绑（interp/cmdLine/bom/propPanel/layerPanel/constraintBar/scriptHost/pluginManager/autoSave）→ 状态回显（工具高亮/样式控件/状态栏/UCS）**。

"断多少、重接多少"由同一函数保证严格对称，杜绝旧模型串扰——这是本特性最大的正确性风险点，靠结构性收口而非人工核对解决。

配套消费者接口：`CommandInterpreter::setModel/setHistory/setLastCommand`、`ScriptApi::setModel`、`ScriptHost::setModel`（引擎经 api() 反向访问模型，换 api 即全局生效）、`PluginManager::setModel`（插件不重新 initialize，下次命令自然作用于新文档）、`SdkModel/SdkApplication::setModel`、`BomTree/PropertyPanel/ConstraintBar::setModel`、`LayerPanel/CommandLine::setModel`（两者自有模型自连接一并迁入 setModel 的断开-换绑-重连）。

命令行历史与 lastCommand 是**会话态**：切换时 copy-out（`cur->cmdHistory = interp_->history()`）→ 换绑 → copy-in（`interp_->setHistory(new->cmdHistory)`）与共享 CommandInterpreter 互换。

### 2.4 页面化视图（每文档一页）

- `DocPage{page, view, hRuler, vRuler}`：标尺与视图**成对创建成对切换**，装入 QStackedWidget；`QTabBar`（`setMovable(true)` + `setTabsClosable(true)`）在上。
- `tabMoved → docs_->reorder(from,to)`（拖拽重排）；`tabCloseRequested → confirmCloseDoc && closeDocAt`；`currentChanged` 经 **lambda 包装**连 `switchToDoc(idx)`（见踩坑 ④）。
- `Ctrl+Tab` QShortcut（ApplicationShortcut）→ count>1 时 `switchToDoc(docs_->cycle(1))`。

### 2.5 脏标记自动跟踪与关闭保护

- 脏标记：DocSession 构造连接自身 `modelChanged → setDirty(true)`，编辑即置脏、幂等；读档/新建/保存后外部 `markClean` 复位；`dirtyChanged(i) → syncTabTitle` 刷新标签 `*`。
- `confirmCloseDoc(idx)`：不脏直接放行；脏则先 switchToDoc（保存作用于活动文档）→ 「保存 / 不保存 / 取消」三选（默认保存），Save 走 `saveFile()` 后按 `!isDirty()` 复核（用户可能取消对话框）。
- `closeEvent` 退出保护（新增行为）：收集全部脏文档 → 「全部保存 / 全部不保存 / 取消」（默认全部保存），Cancel 则 `e->ignore()`；SaveAll 对有 filePath 的静默写盘、无 filePath 的逐个弹 saveFile 对话框。
- **销毁顺序铁律**：`closeDocAt` 先 `delete pg.page`（GraphicsView 引用模型）再 `removeDoc`（会话析构携带模型）——视图不得悬空于模型之后析构；全关后回落新空白会话，窗口永不处于"无文档"状态。

### 2.6 跨文档剪贴板与打开/新建/保存接线

- 跨文档粘贴直接复用 `Clipboard::Payload → toJson → fromJson → pasteInto(model, payload, to, ...)`：复制时序列化的是**当时活动文档**的选中集，粘贴写入的是**粘贴时活动文档**的模型，同一 payload 到多个文档内容必然一致（MD⑦ 自测锚点）。
- `openDocInteractive(path)`：addNewDoc + makeDocPage + addTab + switchToDoc → `readDrawing` 失败则 `closeDocAt` 回滚返回 -1（不留空壳标签）；成功则记 filePath + markClean。openFile 对话框流程不变，仅末端换入口。
- 新建（NewDrawingDialog）同样开新标签，既有图纸不受影响。
- `saveFile()` 成功分支：回写 `active()->filePath` + `markClean(activeIndex)` + syncTabTitle——"另存为"后标签自动改名为真实文件名。

## 3. 改动清单

| 文件 | 改动 |
|---|---|
| `src/ui/DocTab.{h,cpp}` | **新建**：DocSession（独立模型+脏标记+标题）+ DocManager（增删/循环/重排/活动管理） |
| `src/ui/MainWindow.h/.cpp` | docs_/docTabs_/viewStack_/mediator_/docPages_ 成员；buildDocArea/makeDocPage/hookActiveDoc/switchToDoc/confirmCloseDoc/closeDocAt/openDocInteractive；构造重排；connectSignals 重构（mediator_ 成员化）；openFile/new/saveFile/closeEvent 接线 |
| `src/ui/ModelMediator.{h,cpp}` | 成员化 + `rewire(comp)`（断全部→换件→重 wire），wire 的 20 条 connect 登记 conns_ |
| `src/core/CommandInterpreter.h` | setModel / setHistory / setLastCommand |
| `src/core/ScriptApi.h` / `ScriptHost.{h,cpp}` | setModel（纯指针换绑） |
| `src/core/SdkContext.h` / `PluginManager.h` | setModel 贯通应用桥/模型桥/插件 |
| `src/ui/BomTree.h` / `PropertyPanel.h` / `ConstraintBar.h` | setModel（PropertyPanel 清旧文档选中 id） |
| `src/ui/LayerPanel.{h,cpp}` / `CommandLine.{h,cpp}` | setModel（自有模型自连接迁入） |
| `CMakeLists.txt` | DocTab.cpp 入 UI_SOURCES 与 SketchSelfTest 源清单 |
| `tests/SketchSelfTest.cpp` | t_multidoc 10 组 33 条 |

## 4. 自测设计（t_multidoc，10 组 33 条）

1. **独立会话**：开 3 张图 → count/首张自动活动/三模型互异（无单例）/各自画圆互不影响/撤销栈独立积累；
2. **独立 undo**：doc0 undo 只消失自己的圆，doc1 的保持；doc1 undo 同理；
3. **脏标记**：初始不脏 → 编辑自动置脏 → markClean 复位且标题无 `✳` → 再编辑标题带 `✳` → 按会话隔离（d2 干净不受牵连）→ 单独 markClean 只影响该会话；
4. **cycle**：正向循环/末尾回绕/负步长/不改文档状态；
5. **reorder**：`QList::move` 插入语义断言（move(2,0) → [d2,d0,d1]）/活动身份跟随重排/再次重排仍跟随；
6. **跨文档剪贴板**：Payload→JSON→往返、同一 payload 粘贴 doc1/doc2、dumpModel 全等、落点按基点平移、源文档不受影响；
7. **命令行历史互换**：copy-out/copy-in/新文档独立积累/切回恢复；
8. **关闭文档**：关闭活动 → 收缩正确 + 夹取邻居 + 幸存模型完好；全关 → `count==0 && activeIndex==-1`；
9. **标题命名**：绘图1/绘图2 序号、已存盘显示文件名、脏追加 `✳`；
10. **removeDoc 收缩全路**：活动位夹取、非活动且位于活动之前时活动下标左移（首版真 bug，见踩坑 ②）。

## 5. 验收对照（实测）

| 验收标准 | 结果 |
|---|---|
| ① t_multidoc ≥ 8 条（独立 undo / 脏标记切换 / 跨文档粘贴内容一致） | ✅ 33 条（含专组覆盖）；全量 **1093/0**（1052→1093），SELFTEST_EXIT=0 |
| ② 既有单文档流程行为不变 | ✅ readDrawing/writeDrawing/applyNewDrawing 原样；JSON 格式零改动；全量自测无回归 |
| ③ 双目标零警告、build.sh test EXIT=0 | ✅ BUILD_EXIT=0、编译器警告 0（-j 1 增量等效路径，沙箱高并行会 SIGTERM） |

改动边界遵守：JSON 存储格式未动；无全局单例模型——每个 DocSession 一份模型。

## 6. 踩坑记录

1. **`QList::move` 是插入语义**（remove 后 insert，非交换）：`[d0,d1,d2]` `move(2,0)` → `[d2,d0,d1]`。reorder 的活动下标重映射与测试断言都必须按"移除后插入"推导，首版按交换语义写断言导致 MD⑥ 失败。
2. **removeDoc 活动下标左移遗漏（真 bug）**：关闭**非活动**文档且其下标小于活动下标时，`active_` 必须左移，否则切换落空到错误文档；首版遗漏被 MD⑨（全关后 activeIndex 应为 -1）暴露，修复补 `else if (i < active_) --active_;`。
3. **头文件内联 setModel 访问不完整类型**：`CommandLine.h` 内联实现里访问 `interp_->history()`，而 CommandInterpreter 仅前置声明 → `invalid use of incomplete type`；规则：头文件内联函数体不得解引用仅前置声明的类，实现移 cpp。连带发现 CommandLine/LayerPanel 构造里的模型自连接也要一并迁入 setModel，否则换绑后旧连接残留。
4. **`QTabBar::currentChanged(int)` 不能直连带默认参数的双参槽**（`switchToDoc(int, bool forceHook=false)`）→ lambda 包装 `[this](int idx){ switchToDoc(idx); }`。
5. **`->model` 误写**：访问器是 `model()`，三处（构造 firstDoc / makeDocPage / switchToDoc）漏写括号，编译期即报（成员函数指针无法隐转），sed 批量修正。
6. **LineStyle 枚举进同步函数**：`syncLineStyleWidgets(int(model_->defaultLineStyle()))` 需显式 int 转换。
7. **unused-variable 警告**：测试里取了局部会话指针但断言未直引 → -Wunused-variable 破坏零警告基线；改为断言直引对象（`d1->isDirty() && !d2->isDirty()`、`CHECK(dm.doc(1)==d1)`）。

## 7. 已知边界

- **自动保存只盯活动文档**：`autoSave_->setCurrentDrawing(会话filePath)` 随切换走，非活动文档的编辑不触发快照 opThreshold 计数（切回后继续累计，风险窗口=切换前一刻的未快照编辑）；
- **命令行历史经解释器互换**：切换瞬间历史归零再灌入，历史中带模型引用的语义（如脚本局部变量）不跨文档保留；lastCommand 随历史同通道 copy-out/copy-in；
- **closeEvent 退出保护为新增行为**：退出时脏文档必弹汇总对话框（原单文档行为由各文档保存流程覆盖，语义向前兼容）；
- **标签重排的视觉与数据同步**：QTabBar 的 movable 拖拽先改 UI 再发 tabMoved，DocManager::reorder 后经 orderChanged 无反向刷新需求（UI 已先行），但外部若直接调 reorder 需自行刷新标签顺序；
- **关闭脏文档的 Save 路径**：saveFile 若被用户在对话框取消，`confirmCloseDoc` 按 `!isDirty()` 复核返回 false，标签保留——不会出现"点了保存却丢稿"。
