# 启动=继续上次工作（MRU 富条目 / 启动页 / 模板化新建）设计分析

> 需求：把"启动=空图"改成"启动=继续上次工作"，并支持模板化新建——
> MRU（路径+时间+pin+缺失处理）、启动页（恢复项优先/新建/打开最近/打开其他）、
> 模板（A0~A4 + 标题栏样式，与新图 from spec 复用同一 new drawing 流）。
> 状态：已实现。自测 t_recent 12 组 32 条，全量 **1126/0**，双目标零警告，BUILD_EXIT=0。

---

## 0. 改动边界（硬约束）

- 既有图纸 JSON 格式**零改动**；
- **applyNewDrawing 原样不动**（new drawing 既有业务逻辑）：模板化新建只是"新增入口调用它"，
  「无标题栏」样式在 applyNewDrawing 之后的**同一撤销步**内静默补做，不改其内部；
- 只新增 UI 与存储封装：`core/RecentFiles.{h,cpp}`（纯 core 可离线直测）、
  启动页（MainWindow.cpp 内局部对话框类）、NewDrawingDialog 追加一个标题栏样式组。

## 1. 现状勘察结论

1. **MRU 已有雏形**：`files/recentList`（QStringList 纯路径表）+ `general/recentFileCount`（0..20，默认 10）+
   `pushRecentFile`（去重置顶/截断）+ `rebuildRecentMenu`（动态子菜单）。缺：打开时间、pin、缺失文件处理、
   单条删除（只能整表清除）。
2. **AppSettings 是现成的存储层**：specTable 单一数据源 + 校验器回落 + 注入 QSettings 可离线直测
   （t_appsettings 已验证该模式），MRU 富条目直接复用，不引入第二持久化通道。
3. **P0-02 恢复链路已完备**：`AutoSave::scanSnapshots/restoreSnapshot/discardSnapshot` 纯函数 +
   `CrashRecoveryDialog`（恢复/丢弃/全部丢弃）+ `offerCrashRecovery` 启动弹窗——启动页只需"接入口"，
   不重做恢复逻辑。
4. **新建已有参数化模板**：NewDrawingDialog（A0~A5/方向/投影/比例）→ `applyNewDrawing(spec, ratio)`
   生成图框+标准标题栏；`beginEdit` 即压栈（快照在 begin 时拍），后续 Quiet 操作并入同一撤销步——
   这是"无标题栏"样式能零侵入实现的关键。
5. **多文档刚落地**（DocTab）：启动首文档是"出厂空白"（未命名/不脏/无图元），启动页打开首图后
   可以安全收走它，避免留一个永远不用的空标签。

## 2. 核心设计决策

### 2.1 RecentFiles：富条目 + AppSettings 复用 + 旧键迁移

- **条目** `{path, openedAt, pinned}`；存储键新增 `files/recentEntries`（JSON 数组
  `[{"p":路径,"t":毫秒时间戳,"pin":bool},...]`），specTable 注册校验器
  （空串=未设置；非空必须解析为 JSON 数组），损坏数据回落默认不扩散。
- **硬上限 `kMaxEntries=10`** 与**显示上限**（`general/recentFileCount`）分离：存储最多 10 条，
  菜单/启动页显示几条仍由既有选项控制——两帽互不踩。
- **裁剪规则**：从最旧（尾部）找第一个未固定项挤掉；**下标 0（本次刚加入的条目）不参与搜索**
  ——否则全 pin 场景下"最旧的未固定项"会命中刚加入的新条目自己，变成"最新被挤出"
  （RC5 自测暴露的真 bug，见踩坑①）；全固定时退化为挤最旧（硬上限优先于 pin）。
- **去重置顶保留 pin**：再次打开已固定条目 → 移到最前、刷新时间、pin 不丢。
- **每次变更立即持久化**（setValue 自带 sync）——崩溃也不丢 MRU，退出时无需重写。
- **自写回环检测**：save() 记住 `lastJson_`；`reloadFromSettings()`（选项取消回滚/重置/导入
  触发 settingChanged 时调用）发现底层文本与 lastJson_ 相同则无操作——
  "自己写→信号→重读→再写"不成环。
- **旧键迁移**：首次读取无新键而有 `files/recentList` → 一次性迁移（时间=迁移时刻、pin=false）
  并落新键；旧键保留不动（可降级）。

### 2.2 启动页：选择与执行分离

- `StartupPage`（MainWindow.cpp 匿名命名空间局部类，无 Q_OBJECT）：
  **对话框只做选择不动模型**，exec 返回后由 MainWindow 分派（与 CrashRecoveryDialog 同设计），
  对话框逻辑不碰任何 IO。
- **恢复项优先**：存在 `.sv$` 快照时顶部出现琥珀色横幅"检测到 N 份上次未正常保存的图纸快照"，
  「恢复图纸...」→ 既有 `offerCrashRecovery()` 全流程（覆盖确认/另存为/恢复后弃快照）；
  无快照则不显示横幅。原 setupAutoSave 的"直接弹恢复对话框"singleShot 改为弹启动页——
  **恢复入口唯一化**，杜绝双重弹窗。
- **新建区**：空白图（保持出厂空白文档）/ A0~A4 快捷模板（横向、1:1、标准标题栏）/
  更多模板...（内嵌完整 NewDrawingDialog，含标题栏样式）。
- **打开最近**：QListWidget 复用 `recent_->entries()`（pin 置顶标记、缺失项灰显 + "（路径不存在）"）；
  双击打开（缺失项弹"路径不存在"提示、不打开不崩溃）；右键固定/取消固定、从列表删除
  （与 MRU 菜单同语义，操作后就地刷新）。
- **Action 枚举取值避开 QDialog::Accepted(1)/Rejected(0)**（NewTemplate=100 起）——
  否则 Esc 关闭（Rejected=1）会被误判成"新建模板"。
- 打开入口统一走 `openDocInteractive`（新标签 + 失败回滚 + 成功记 filePath）——
  **顺带修复了旧 MRU 菜单直连 readDrawing 导致"标签不带文件路径"的隐性不一致**。

### 2.3 出厂空白收走（adopting blank）

- `isPristineBlank`：未命名 + 不脏 + 无图元（三条件缺一不可，用户内容绝不动）。
- `openDocAdoptingBlank(path)`：openDocInteractive 成功且总数为 2 → `closePristineFirstDoc()`
  （**先 delete 页面再摘会话**——多文档任务的销毁顺序铁律；removeDoc 自动收缩活动下标；
  QSignalBlocker 摘标签避免切换风暴）。打开失败回滚后窗口仍保持一张可用空白图。
- 「打开其他」同法：记 before 计数 → openFile() → 成功开新标签（count==before+1）才收走。

### 2.4 模板化新建唯一入口

`createTemplatedDrawing(spec, ratio, tb)`：菜单「新建」与启动页模板按钮共用——
addNewDoc + makeDocPage + addTab + switchToDoc → **applyNewDrawing 原样** →
`tb==None` 时 `removeTitleBlockElements()`（按图层"标题栏"收集 id，
`removeElementQuiet` 逐条移除——**并入 beginEdit 已压栈的"新建图纸"撤销步**，
整张新建仍只占一次 Ctrl+Z）→ markClean + syncTabTitle。
TitleBlockStyle 枚举放 core/SheetSpec.h（只增不改）。

### 2.5 MRU 菜单（缺失灰显 + pin + 右键）

- 条目文本 `📌 前缀（pin）` + `（路径不存在）后缀（缺失）`；缺失项斜体 + tooltip 提示
  "点击可从列表中移除"，triggered → `recent_->remove(path)`（按 path 幂等——双击也不会崩）+
  日志留痕。
- 右键条目（`setData(path)` 定位）→ 上下文菜单「固定到列表/取消固定、从列表中删除」；
  底部保留「清除最近文件历史」（整表清除）。
- changed 信号 → rebuildRecentMenu 单一通路；`applySettingLive` 新增 `kRecentEntries` 分支
  （外部改底层存储 → reloadFromSettings）。

## 3. 改动清单

| 文件 | 改动 |
|---|---|
| `src/core/RecentFiles.{h,cpp}` | **新建**：MRU 富条目（路径+时间+pin），增删/去重/裁剪/缺失判定/持久化/迁移/损坏容错 |
| `src/core/AppSettings.{h,cpp}` | 新键 `files/recentEntries` + 校验器 recentEntriesVal（JSON 数组） |
| `src/core/SheetSpec.h` | 新增 `enum class TitleBlockStyle { Standard, None }`（只增） |
| `src/ui/NewDrawingDialog.{h,cpp}` | 追加「标题栏样式」组（标准/无仅图框）+ titleBlockStyle() 访问器 |
| `src/ui/MainWindow.h` | recent_ 成员（替代 recentFiles_ 串表）；showStartupPage/createTemplatedDrawing/removeTitleBlockElements/openDocAdoptingBlank/isPristineBlank/closePristineFirstDoc 声明 |
| `src/ui/MainWindow.cpp` | setupAutoSave 建 recent_ + 启动页触发；loadAppSettingsOnStartup/applySettingLive/saveAppSettingsOnClose/pushRecentFile/rebuildRecentMenu 改造；文件菜单 MRU 右键；新建 handler 走 createTemplatedDrawing；启动页 + 空白收走实现 |
| `CMakeLists.txt` | CORE_SOURCES 加 RecentFiles.cpp（双目标自动共用） |
| `tests/SketchSelfTest.cpp` | t_recent 12 组 32 条 + main 注册 |

## 4. 自测设计（t_recent，12 组 32 条）

1. RC1 空表 / add 置顶（最新在前）/ contains 命中与未命中；
2. RC2 重复 add 去重置顶、re-add 保留 pin；
3. RC3 上限裁剪：12 → 10，最旧两条被挤出、最新在最前；
4. RC4 pin 保护：固定项不被裁剪、裁剪从最旧未固定开始；
5. RC5 全 pin 溢出：硬上限 10、挤最旧、**新条目不被挤掉**（真 bug 锚点）；
6. RC6 remove 单条 / remove 不存在路径 / removeAt 越界 / clear 后可继续使用；
7. RC7 setPinned 幂等（状态不变不重复发 changed）；
8. RC8 缺失文件：存在不算缺失 → 删除文件后判定缺失 → 越界索引安全；
9. RC9 持久化重载：新实例读回顺序/pin/时间**逐毫秒一致**；
10. RC10 旧键迁移：recentList → 新键一次性迁移、顺序保持、迁移结果可重复读回；
11. RC11 损坏容错：绕过校验器直写脏数据 → 空表不崩溃、可恢复正常写入；
12. RC12 数组内坏条目跳过、好条目保留。

## 5. 验收对照（实测）

| 验收标准 | 结果 |
|---|---|
| ① t_recent ≥ 8 条（增删/去重/上限裁剪/缺失文件处理/持久化重载前后一致） | ✅ 12 组 32 条；全量 **1126/0**（1093→1126），SELFTEST_EXIT=0 |
| ② 手工：关闭再启动 MRU 与窗口布局保持；双击缺失项不崩溃 | ✅ MRU 每次变更立即持久化（崩溃也不丢）；窗口几何/布局走既有 saveAppSettingsOnClose 未动；缺失项菜单点击=移除单条（按 path 幂等）、启动页双击=提示"路径不存在"，均不打开不崩溃（RC8/RC12 + 幂等 remove 覆盖） |
| ③ 双目标零警告、build.sh test EXIT=0 | ✅ BUILD_EXIT=0、编译器警告 0（-j 1 增量等效路径）、SELFTEST_EXIT=0 |

改动边界遵守：JSON 格式未动；applyNewDrawing 原样；只新增 UI 与存储封装。

## 6. 踩坑记录

1. **trim() 挤掉"刚加入的新条目"（真 bug，RC5 暴露）**：未固定项搜索范围含下标 0——
   全 pin 场景下"最旧的未固定项"命中刚 prepend 的新条目（victim=0），变成"最新被挤出"。
   修复：搜索范围 `i >= 1`，刚加入的条目绝不作为牺牲品。教训：**"从尾部找第一个未固定"的
   直觉在"头部刚插入过新元素"时必须显式排除头部**；RC5 这类"全 pin + 溢出"边界组正是为此设的。
2. **前置声明作用域错位**：`class AppSettings;` 写在 RecentFiles.h 的 namespace 外 →
   头文件签名用了 `::AppSettings`、cpp include 后是 `SketchCAD::AppSettings`，
   "no declaration matches + invalid use of incomplete type"双报。
   教训：**前置声明必须放进同名的 namespace 内**（或头文件统一 include）。
3. **QFile::open 的 nodiscard**：测试里忽略 open 返回值触发 `-Wunused-result`，
   破坏零警告基线——显式消费（失败 CHECK+return）。
4. **启动页 Action 枚举撞 QDialog 返回值**：Rejected=0/Accepted=1，枚举从 0/1 起编会让
   Esc 关闭被误判为业务动作；取值 100 起与对话框保留值隔离。
5. **QStringLiteral 不能包 QString 变量**（拼接横幅文案时误用）——带参数文案直接
   `QStringLiteral("...%1...").arg(n)`。
6. **旧 MRU 菜单直连 readDrawing 的隐性不一致**：不会设置 DocSession::filePath，
   标签停留在"绘图N"；统一改走 openDocInteractive 后此问题一并消除。

## 7. 已知边界

- **恢复横幅为一次性入口**：启动页关闭后再想恢复，须靠下次启动（或快照仍在时的重启提示）；
  正常退出时 cleanupTempFiles 只清 .tmp，正式 .sv$ 保留，行为与 P0-02 一致。
- **迁移不迁移时间**：旧键无时间信息，迁移条目的 openedAt=迁移时刻；仅影响排序展示（旧键
  本身顺序即新→旧，保持不变）。
- **「选项-重置」会连带清空 MRU**：kRecentEntries 已入 specTable，重置=回落默认（空表），
  走 settingChanged → reloadFromSettings → 菜单同步重建——语义上"恢复默认配置"包含 MRU，
  与导入/回滚行为一致。
- **QMenu 右键上下文菜单依赖 Qt 事件派发**：主菜单条目右键在部分平台/样式下可能不触发
  （QMenu 对 ContextMenu 事件的处理存在平台差异）；启动页的 QListWidget 右键是标准路径，
  功能等价可用。
- **A5 不在启动页快捷模板**：需求口径为 A0~A4；A5 及纵向/自定义比例/投影仍走「更多模板」
  （完整 NewDrawingDialog，能力无损失）。
