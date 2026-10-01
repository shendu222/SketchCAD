# 用户体验增强（UX Enhancement）设计分析

> 模块：交互层（CommandLine / StatusBar / GraphicsView / MainWindow）
> 版本：基于第五阶段（CUIx + 命令别名）完成后的代码基线
> 关联文档：`cui-alias-design-analysis.md`、`macro-script-design-analysis.md`、`polar-otrack-tk-design-analysis.md`

---

## 1. 需求拆解

原始需求（用户原话）拆为九项可验证能力：

| # | 需求 | 类型 | 落点 |
|---|------|------|------|
| R1 | 「命令+参数」输入模式 | 确认既有 + 补强 | CommandLine / CommandInterpreter |
| R2 | 自动补全 | 补强 | CommandInterpreter::completions + QCompleter |
| R3 | 动态提示 | 新增 | CommandLine placeholder + SketchModel::commandPrompt |
| R4 | 带选项关键字的输入提示（如 [C/闭合]） | 新增 | CommandPrompt 协议层 + StatusBar 选项区 |
| R5 | 空格/回车重复上一未确认命令 | 新增 | CommandInterpreter::lastCommand |
| R6 | 选中对象即弹出属性面板 | 新增 | MainWindow selectionChanged 门控接线 |
| R7 | 上下文相关右键菜单 | 新增 | GraphicsView::showContextMenu |
| R8 | 状态栏开关组（正交/极轴/捕捉/追踪/线宽）+ 状态栏与标尺完善 | 新增+确认 | StatusBar 开关组 + ModelMediator 既有联动 |
| R9 | 动态输入 DYN（光标跟随的距离/角度输入框） | 新增 | GraphicsView DYN 覆盖层 + SketchModel::dynMode |

## 2. 现状摸底结论（动手前的关键判断）

架构摸底发现**三条联动链已由 ModelMediator 接好**，属于 R8 的「已具备部分」，不重复建设：

- `cursorMoved → StatusBar::setCoord`（坐标读数）
- `viewTransformChanged → hRuler/vRuler::sync`（标尺联动）
- `selectionChanged → PropertyPanel::onSelectionChanged`（选中→属性内容刷新）

**真正缺的是「命令会话态」**：模型层没有「当前命令的可选项」「上一命令」「线宽显示」「DYN 开关」这些状态，UI 层自然无处消费。因此增量工作收敛为三层：

```
┌─ UI 消费层 ── CommandLine（提示/拦截/重复） StatusBar（开关组/选项区）
│               GraphicsView（右键菜单/DYN/线宽渲染） MainWindow（接线/弹面板）
├─ 协议层 ───── CommandPrompt.h（纯函数：选项格式化/匹配，headless 可测）
└─ 会话态层 ─── SketchModel（commandOptions/showLineWeight/dynMode）
                CommandInterpreter（lastCommand）
```

## 3. 设计决策与取舍

### 3.1 会话态全部收敛到 SketchModel（单一状态源纪律）

沿用 orthoMode_/gridSnap_ 的成熟范式（F8/F9 快捷键、状态栏按钮、命令三入口同步的那套）：

- `commandOptions_` + `setCommandOptions()` + `commandOptionsChanged` —— 由 GraphicsView 在分段状态变化时写入，CommandLine/StatusBar 消费
- `showLineWeight_`（默认关）+ `setShowLineWeight()` + `showLineWeightChanged`
- `dynMode_`（默认开）+ `setDynMode()` + `dynModeChanged`，配 `dyn [on|off]` 命令（镜像 ortho 命令注册）

**取舍**：把 DYN 开关也放进模型而非 GraphicsView 成员，是为了「命令/快捷键/UI」三入口同构；虽然需求只要求命令入口，但范式一致性比省两行代码更值钱。

### 3.2 选项关键字协议："KEY/标签" 双格式

```
选项字符串： "C/闭合"  "U/放弃"  "L/长度"
             └┬┘ └┬┘
           英文短键  中文标签
```

- **英文短键**：命令行输入口径（大小写不敏感），与 AutoCAD 习惯对齐
- **中文标签**：中文直输同样命中（`matchOption("闭合")` → `"C"`），且展示可读

`CommandPrompt::formatOptionPrompt()` 生成 `[C/闭合 U/放弃]`，`matchOption()` 返回命中的英文键。**纯函数、零 Qt Widgets 依赖，headless 进自测**——这是本次工程化最重要的一层：选项协议一旦有了独立纯函数层，命令行拦截、状态栏展示、单测三处共用同一实现，杜绝硬编码漂移。

**命令行分派优先级**（绘制工具激活时）：**选项关键字 > 坐标点 > 完整命令**。选项必须最先判——否则 `[C/闭合]` 里的输入可能被 tryParsePoint 误吃或当成未知命令。

### 3.3 R5 重复上一命令：记录「别名展开后」的命令名

`CommandInterpreter::execute()` 唯一出口处（`ok` 为真时）记 `lastCommand_ = cmd`。三个细节：

1. **记录展开后的名字**而非原始输入：用户敲 `l 0,0 10,0` 后，空回车重复的是 `line`（与 AutoCAD 语义一致）
2. **失败不更新**：`nosuchcmd` 执行失败后 lastCommand 保持原值
3. **空格与回车同权**：keyPressEvent 拦截「空输入+空格」（避免插入空格字符），onReturn 处理「空输入+回车」

### 3.4 R6 选中弹属性面板：activeTool 门控（重要取舍）

朴素实现 `selectionChanged → propDock_->show()+raise()` 有一个隐蔽副作用：**连续直线绘制时 `commitElement()` 逐段 `setSelected(id)`**，每画一段都会抢屏弹面板，绘制体验被破坏。

**决策**：门控为 `activeTool == "select"` 时才弹出。绘制完成的元素用户随时点选即可看到面板；绘制中不打扰。右键「特性」不经此门控（显式动作永远生效）。

### 3.5 R7 右键菜单：语义保持优先

现状：右键承担「结束连续绘制 / 取消命令」职责（`handleRightButton()`），且 `setContextMenuPolicy(NoContextMenu)` 是为保护该语义刻意设置的。

**决策**：不启用 Qt 原生 contextMenuEvent，而是在 `handleRightButton()` 尾部扩充分派：

```
右键单击（wasClick）
  ├─ 绘制会话中（dimPending_/工具终结/有 pendingPts_） → 结束/取消（原语义，不动）
  ├─ activeTool != "select"                            → 退出到选择（原语义）
  └─ select 态                                          → showContextMenu(QCursor::pos())
```

上下文两类：**有选择集**（特性/复制/删除/反选/全部不选）、**空白**（全选/粘贴/缩放范围/栅格开关）。GraphicsView 通过 `requestShowProperties` 信号请求主窗口弹属性面板——视图层不持有 dock 指针，保持既有分层。

### 3.6 R8 状态栏开关组：信号解耦防回环

```
用户点击按钮 ──toggled──▶ StatusBar::toggleRequested(key,on) ──▶ MainWindow 回写模型 setter
模型状态变化 ──model信号──▶ MainWindow ──▶ StatusBar::setToggleState（QSignalBlocker 阻塞 toggled）
```

StatusBar 保持「无模型指针」的纯组件风格（与既有 setter 风格一致），按钮组只是多了两个出口：`toggleRequested` 信号 + `setToggleState` 回写方法。**QSignalBlocker 是防回环关键**——程序性 setChecked 不再触发 toggled，否则按钮回写模型、模型信号又回写按钮，形成信号风暴。

五个开关映射：正交→`setOrthoMode`、捕捉→`setGridSnap`（栅格吸附，即 F9 语义）、极轴→`trackConfig().polar`、追踪→`trackConfig().objectTrack`、线宽→`setShowLineWeight`。其中极轴/追踪**就地改 TrackConfig 单字段**再整体 set，避免重建整个配置。

### 3.7 R9 DYN：viewport 覆盖层 + 焦点暂停预填

- **载体**：viewport 子 QWidget（不进 QGraphicsScene，避免参与场景索引/缩放变换），含「距离/角度」两个 QLineEdit
- **出现时机**：`drawToolActive_ && dynMode && !pendingPts_.empty()`（已有第一个点才有极坐标参考系）
- **预填**：每帧按「上一点→光标」的极坐标刷新；**焦点在输入框内时暂停预填**，避免覆盖用户正在键入的值——这是覆盖层方案唯一的输入竞争点，用 hasFocus 判断化解
- **提交**：Enter → 上一点 + 极坐标(距离, 角度°) 算出世界点 → **复用 `injectPoint()` 点击管线**（追加采集点/更新预览/点数满足自动收尾全走既有逻辑），不复制任何分派代码
- **隐藏时机**：`resetPending()` 统一挂 `hideDynBox()`（绘制会话结束的唯一收口），dynMode 关闭时同步隐藏
- 角度口径：世界角、逆时针为正（与极轴追踪一致；Qt 渲染期的 Y 翻转不参与计算）

### 3.8 线宽显示：penFor 单点收敛

`ElementItem::penFor()` 是全部图元线宽的唯一出处（块引用子图元也复用它）。实现：ElementItem 加 `showLineWeight` 标志位，`penFor` 中 `if (!showLineWeight) wpx = 1.0;`（一律细线，即 LWDISPLAY 关的观感；warn/highlight 的加粗增量保留，选中仍可见）。

同步链：模型信号 → GraphicsView `showLineWeight_` → ① rebuild 时注入新建 item ② 开关瞬间遍历 `scene_->items()` 刷新存量 item。预览项同样注入，避免预览与成品线宽口径不一致。

## 4. 数据与兼容性

- **零序列化影响**：commandOptions/showLineWeight/dynMode 全部是**会话态**，不入 Memento、不入图层/块/样式 JSON——关掉软件重开就是默认值，旧档完全兼容
- **零 ElementType 影响**：本次未新增图元类型
- **命令表只增不改**：`dyn` 为新增注册，ortho/snap/polar 等既有命令语义未动
- **lastCommand 是解释器实例状态**：不进宏录制缓冲（MacroRecorder 捕获的是命令行原文，不受影响）

## 5. 过程陷阱实录

1. **前向声明进命名空间（第 4 次同型坑）**：`class QLineEdit;` 放进了 `namespace SketchCAD` 内，声明出 `SketchCAD::QLineEdit` 遮蔽全局 `::QLineEdit`，连锁炸出 CommandLine override 失效、MainWindow 内 new QLineEdit 不完整类型等一串错误。修复：移到 namespace 外。此坑在 COM 阶段（::SketchModel）和 SDK 阶段（QLibrary）已各踩过——已在 GraphicsView.h 修复处留注释警告。
2. **默认别名悬空（历史遗留发现）**：默认别名 `q → qselect`，但 `qselect` 并非注册命令，执行 `q` 会报「未知命令: qselect」。本次未顺手修（属别名表与命令表的一致性问题，值得单开任务），仅记录。
3. **Edit 误删函数体**：插入 `applyCommandOption` 时 old_string 匹配了整个 `commitElement` 导致函数体被吞，靠 grep 复查发现后立即恢复——**编辑后 grep 复查关键函数完整性应成为肌肉记忆**。

## 6. 验证

- **三目标构建**：`cmake --build build --target SketchCAD SketchSelfTest SketchCADAutomation -j 1`，全量重编译 `-Wall -Wextra` **零警告**
- **基线遗留警告顺手清零**：全量重编译暴露 4 个非本次触碰文件的基线警告（ConstraintSolver 未用函数 / Tools 未用参数 / PolylineEditTool 未用变量 / LayerPanel 弃用 signal×2），已机械修复——`applyOne` 保留后备实现加 `[[maybe_unused]]`、LayerPanel 迁移 `stateChanged→checkStateChanged`（Qt 6.7+ API）
- **自测**：`./build.sh test` → **通过 54 / 失败 0**（基线 53 → 54）
- `t_cmdprompt` 覆盖：选项格式化/键标签提取/matchOption（英文键大小写、中文标签、未命中）/模型三会话态（信号幂等）/lastCommand（含别名展开、失败不更新）/`dyn` 命令/completions 含别名键

## 7. 已知限制与演进方向

| 项 | 现状 | 演进 |
|----|------|------|
| 选项 C/闭合 | 仅 polyline 收口已接（≥3 点回起点闭合） | 各绘制命令按需挂接：GraphicsView 只需在分段状态变化时 setCommandOptions + 在 applyCommandOption 加分支 |
| DYN 键入路径 | 输入框需点击聚焦后键入 | 画布键入数字直路由 DYN（需 GraphicsView keyPressEvent 数字捕获 + 焦点移交） |
| 别名 q 悬空 | `q→qselect` 但命令未注册 | 注册 qselect 命令或改别名指向 |
| 右键菜单动作集 | 选择/空白两组基础动作 | 接修改类多步工具（移动/旋转/镜像），需 ToolManager 上下文启动支持 |
| 空格重复 | 仅空输入时生效（非空时插空格） | 若需 AutoCAD 完全同构（非空空格=确认），需改输入法语义，权衡误触风险 |

## 8. 小结

本次增强的本质是给既有成熟 CAD 补一层「会话态基础设施」：模型层四个新状态（commandOptions/showLineWeight/dynMode + 解释器 lastCommand）、一个纯函数协议层（CommandPrompt）、四处 UI 消费。所有新能力都走既有范式（单一状态源、中介者接线、信号解耦、唯一出口），不引入新的状态同步机制；绘制期右键语义、序列化格式、命令表兼容性三条红线全部保住。
