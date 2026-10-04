# 崩溃不丢稿 —— 自动保存 · 备份 · 异常恢复 · 全局转储

> 对应需求：建立「崩溃不丢稿」的最小商业级保障。
> 涉及路径：`src/core/AppSettings.{h,cpp}`、`src/core/AutoSave.{h,cpp}`、
> `src/ui/CrashRecoveryDialog.{h,cpp}`、`src/core/Diagnostics.{h,cpp}`、
> `src/main.cpp`、`src/ui/MainWindow.cpp`。
> 改动边界：**不修改既有 SketchModel 序列化格式**；临时/配置文件只写在用户目录，
> 不得在程序安装目录写用户数据。

---

## 0. 一句话结论

把「写盘」这件事从 UI 里抽出一个**纯函数级的序列化入口**，让「保存」和「自动保存」
共用同一条路径；快照一律**先写 `.tmp` 再原子替换并留 `.bak`**；启动时扫描 `*.sv$`
并弹恢复对话框；Qt 警告级以上日志滚动落盘 + Windows 未处理异常写 mini-dump。
最终：**杀进程 → 重启 → 出现恢复对话框 → 恢复内容与崩溃前完全一致**。

---

## 1. 为什么先动序列化入口（DRY 的地基）

### 1.1 现状问题

`MainWindow::writeDrawing` 里手工拼装了 17 个顶层 JSON 键：

```cpp
QJsonObject root;
root["app"] = ...;  root["formatVersion"] = 1;
root["scaleRatio"] = ...;  root["ltscale"] = ...;
root["elements"] = ...;  root["layers"] = ...;  root["blocks"] = ...;
/* …共 17 键… */
case cadfmt::CadFormat::Json:
    out = QJsonDocument(root).toJson(QJsonDocument::Indented);
```

自动保存需要**同一份字节**。若在 `AutoSave` 里重抄一遍，日后新增顶层字段必然漏改一处
—— 这是典型的「两处真相」缺陷，且故障形态很隐蔽：手动保存的新字段能读回来，
自动保存恢复的却丢字段。

### 1.2 决策：抽出 `toDrawingJson()` / `toDrawingBytes()`

在 `SketchModel` 上新增两个方法（`SketchModel.h` 布局表段之后）：

```cpp
QJsonObject toDrawingJson() const;   // 顶层根对象（17 键，与写盘一致）
QByteArray  toDrawingBytes() const;  // QJsonDocument(root).toJson(Indented)
```

`MainWindow::writeDrawing` 的 Json 分支随即塌缩为一行：

```cpp
out = model_->toDrawingBytes();   // 集中式整图序列化入口（与 AutoSave 快照同源）
```

**不变式**：字段名、字段顺序、类型、`Indented` 缩进全部逐字保持不变。
`SketchModel` 只增不改，`ElementType` 枚举与元素字段顺序一律未动 ⇒ **旧档全部可读**。

> 经验教训（与既有 DXF 教训同源）：本项目里凡是「同一份语义在两处手工拼装」的代码，
> 后来都出过「一处更新一处漏」的 bug。**把序列化收敛成单一入口是这类 bug 的根治手段**，
> 而不是在两处各加一个注释提醒「记得同步」。

---

## 2. AutoSave：拆成「纯函数区 + 有状态区」

### 2.1 分层理由

自动保存若整体写成一个大 QObject，逻辑就只能在有 `QApplication` 的环境下验证。
本项目有一条硬约束：**`SketchSelfTest` 不链接 UI**。因此把 `AutoSave` 切成两半：

| 层 | 成员 | 依赖 | 可测性 |
|---|---|---|---|
| **纯函数区**（`static`） | `snapshotDir` / `snapshotNameFor` / `writeSnapshot` / `scanSnapshots` / `readSnapshot` / `restoreSnapshot` / `discardSnapshot` / `cleanupTempFiles` / `freeBytesFor` | 仅 QtCore | 全部可无头直测 |
| **有状态区**（QObject） | `setIntervalMinutes` / `noteOperation` / `saveNow` / `start` / `stop` | QTimer | 只测触发计数，不测 GUI |

对话框（`CrashRecoveryDialog`）也遵守同一纪律：**只发信号，不碰模型、不碰文件**。
真正的读档/删文件由 `MainWindow` 调纯函数完成。这让「对话框长什么样」和
「恢复怎么落盘」两件事各自可独立验证。

### 2.2 命名：可读前缀 + 原路径哈希

Windows 文件名不能含 `\ / : * ? " < > |`，且盘符路径直接当文件名会爆长度。
而不同目录下同名图纸（`A/part.json` 与 `B/part.json`）**必须映射到不同快照**，
否则会互相覆盖。故：

```
快照名 = <清洗后的原文件名>.<原路径 MD5 前 8 位>.sv$
例：C:/work/part.json  →  part.json.1a2b3c4d.sv$
```

- 清洗：非字母数字/`.`/`_`/`-` 一律替换为 `_`；
- 哈希保证不同路径不冲突；
- 前缀保证用户在资源管理器里**一眼认出**是哪张图；
- `.sv$` 是恢复扫描的唯一判据。

### 2.3 原子写：`.tmp` → `.bak` → 提交

```
1) 写 <name>.tmp（WriteOnly|Truncate，校验写入字节数，flush）
2) 若 <name>.sv$ 存在：remove .bak → rename .sv$ → .bak
3) rename .tmp → .sv$（目标此时已不存在，rename 必成功）
```

**为什么不能直接 `QFile::rename` 覆盖**：Qt 明确规定「目标已存在时 rename 失败」，
必须先 `remove` 目标。但「先 remove 目标、再 rename」中间崩溃 = 快照凭空消失，
正是我们要防的场景。所以先 `.bak` 留底：即便第 3 步崩溃，`.bak` 里还有上一个完整版本。

`writeSnapshot` 对每一步失败都写**明确中文原因**（`*err`），并清理半写的 `.tmp`。
第 1 步还会比对 `f.write()` 的实际返回字节数与预期长度，**不等即判失败并删除 `.tmp`**
——磁盘满导致的静默截断是自动保存最典型的故障，必须显式拦下。

### 2.4 头部元信息

快照 = 明文头部 + `\n\n` + 整图 JSON 负载：

```
SCAUTOSAVE1
T=2026-10-04T12:00:00
O=<原路径的 base64>
S=<负载字节数>

{ ... 整图 JSON ... }
```

- `O` 用 **base64** 而非原样：Windows 路径理论上可含换行，原样写会破坏行结构；
- `T` 存快照时间（ISO 格式），而非文件 mtime —— mtime 在跨卷拷贝后不可靠；
- 头本身是**人类可读**的：出问题时直接记事本打开就能判断快照是否完整。

`readSnapshot` 按 `\n\n` 切分返回纯负载；**若无魔数则整体返回**（兼容测试直写的裸数据，
也容忍早期版本的产物）。

### 2.5 双触发

| 触发源 | 实现 | 默认 |
|---|---|---|
| 时间 | `QTimer`（`intervalMinutes_ * 60 * 1000`） | 10 分钟 |
| 操作数 | `noteOperation()` 累加，达 `opThreshold_` 即存并清零 | 关闭（0） |

`saveNow()` 的短路条件：`provider_` 未设置、或负载为空（空图）→ 直接返回 `false`，
**不产生噪声快照、不报警**。

---

## 3. AppSettings：配置可注入

`QSettings(NativeFormat, UserScope, "SketchCAD", "")` ⇒ Windows 上落在
`HKCU\Software\SketchCAD`（空 applicationName ⇒ 只有组织名一层）。

间隔取值域固定为 `{0, 5, 10, 15, 30}`（0=关），**非法值静默回落默认 10**——
注册表被外部改坏时不应导致程序行为不可预期。写入同理忽略非法值，且**同值幂等不写盘**。

构造函数接受注入的 `QSettings*`（`owns=false`），使 `t_autosave` 能用临时 `.ini`
验证读写而不污染真实注册表。

---

## 4. Diagnostics：日志 + 转储

### 4.1 Qt 日志

`qInstallMessageHandler` 拦截 Qt 消息，**仅 warning / critical / fatal 落盘**
（debug/info 量大且无追查价值，只转发给原处理器）。文件 `logs/SketchCAD.log`，
单文件上限 2MB，滚动保留 3 个（`.log` / `.log.1` / `.log.2`），**每次写后 flush**。

> 为什么必须 flush：写日志的目的就是崩溃前留下线索。不 flush 的缓冲在崩溃时
> 会随进程一起消失，等于白记。

### 4.2 崩溃转储（仅主程序）

用 `SetUnhandledExceptionFilter` + `MiniDumpWriteDump` 写 `crashes/crash-<时间戳>.dmp`，
同时写一份 `.txt` 摘要（异常码/时间/OS）与 `last.txt`。

**用 `SKETCHCAD_ENABLE_CRASHHANDLER` 宏隔离**：该宏与 `dbghelp` 链接**只在
`SketchCAD` 目标上启用**，`SketchSelfTest` 不定义、不链接 ⇒ 无头自测目标保持纯净，
不会被 `dbghelp` 拖入 GUI/调试器依赖。

> 实现坑：MinGW/GCC 不认 MSVC 的 `#pragma comment(lib, "dbghelp.lib")`，
> 会报 `-Wunknown-pragmas` 警告并**破坏「双目标零警告」基线**。链接一律交给
> CMake 的 `target_link_libraries(... dbghelp)`。

### 4.3 落盘位置

一律 `%LOCALAPPDATA%/SketchCAD/{logs,crashes,autosave}`。
`LOCALAPPDATA` 取不到时回落到 `~/AppData/Local` → `~`。
**绝不写程序安装目录**（安装目录通常只读，且违反改动边界）。

---

## 5. MainWindow 接线

### 5.1 时序（构造函数末尾）

```
setupAutoSave()
  ├─ 建 AppSettings / AutoSave
  ├─ setProvider([this]{ return model_->toDrawingBytes(); })   ← 与写盘同源
  ├─ 应用间隔 / 磁盘阈值 / 当前图纸路径
  ├─ 接 saved / saveFailed / lowDisk / manualSaveRequested
  ├─ connect(model_, modelChanged) → noteOperation()            ← 操作计数
  ├─ autoSave_->start()
  └─ QTimer::singleShot(0, …) → offerCrashRecovery()           ← 延到事件循环后
```

`offerCrashRecovery` 走 `singleShot(0)` 而非直接调用：必须等主窗口 `show()` 完成，
否则模态对话框会在 splash 阶段弹出。

### 5.2 恢复流程的三条安全分支

`onAutoSaveRecovered` 绝不静默覆盖：

1. **原路径为空 / "(未保存)"** → 弹「另存为」，用户取消则**保留快照**；
2. **原路径已存在文件** → 三选一（覆盖 / 另存为 / 取消），**默认焦点在「取消」**；
3. 正常 → `restoreSnapshot` 原子写回。

恢复成功后立刻 `discardSnapshot`，避免下次启动重复提示。

`recovering_` 标志位在读档期间抑制 `noteOperation()`：读档会触发大量
`modelChanged`（重建图元/图层/标注），若不抑制，**刚恢复就立刻被自动保存覆盖**——
这既是浪费，也可能与用户随后的一次真实修改竞态。

### 5.3 保存失败的显式化

`saved` / `saveFailed` / `lowDisk` 三个信号都同时做三件事：
**状态栏更新 + 模型日志 + `Diagnostics::writeLog`**，其中 `saveFailed`
额外弹 `QMessageBox`（明确写出「图纸仍在内存中，请尽快手动保存」），
杜绝「静默失败」。

### 5.4 正常退出

`closeEvent` 里停定时器 + `cleanupTempFiles()`（清 `.tmp` 半写残留）。
**正式 `.sv$` 保留** —— 正常退出后它仍可作为下次崩溃的恢复点。

---

## 6. 状态栏

`StatusBar` 新增一个**扁平 `QToolButton`**（`btnSave_`）：

- 文案：`上次自动保存 HH:MM` / `自动保存:—`（尚未保存）/ `自动保存:关`（间隔=0）；
- 点击 → `manualSaveRequested()` → `MainWindow` 触发一次 `saveNow()`。

> 实现坑：一开始用了 `QLabel` + 信号，但 **`QLabel 没有 `clicked` 信号**，
> 要做可点击就得另加事件过滤器。`QToolButton` 配 `setAutoRaise(true)` 外观
> 与状态栏文本一致，且天然带 `clicked` —— 直接换按钮是更小的改动面。

---

## 7. 自测（t_autosave，22 条 CHECK）

覆盖验收标准 ①②③④：

| # | 断言要点 |
|---|---|
| 1–2 | 目录建于用户目录；命名稳定/互异/后缀/可读前缀/非法字符清洗 |
| 3–4 | 原子写落盘、`.tmp` 不残留；`readSnapshot` 剥头还原负载 |
| 5 | 覆盖写产生 `.bak` 且新内容生效 |
| 6 | **写失败 → false + 明确中文原因，不产生半截正式文件** |
| 7 | **写失败后原快照内容未被截断** |
| 8 | `scanSnapshots` 发现快照；头部 `O=` 解析回原路径；大小有效 |
| 9 | 恢复内容与负载一致；`.recover.tmp` 不残留 |
| 10–11 | 恢复拒绝「源=目标」；目标不可写 → 明确错误 |
| 12–13 | 丢弃清干净三类文件且幂等；`cleanupTempFiles` 返回条数 |
| 14 | `freeBytesFor` 有效 |
| 15–16 | `saveNow` 走 provider 发 `saved`；空负载不写不报警 |
| 17 | `saveFailed` 初始无触发 |
| 18 | 操作计数：未达阈值不触发、达阈值触发一次 |
| 19 | 定时器 0 关 / >0 激活 / stop |
| 20 | AppSettings 默认值、合法往返、非法回落、信号与幂等 |
| 21 | **`toDrawingJson` 17 键与写盘一致 + 字节可往返** |
| 22 | Diagnostics 目录/写日志落盘 |

> 写失败用例的一个现实约束：Windows 上「置只读目录」在**以管理员身份运行时无效**，
> 且 `QFile::setPermissions` 对目录的语义并不等同 POSIX。所以第 6/7 条采用
> **双保险**：先试只读目录，若仍能写（特权环境）则**改用「目标是已存在的目录」**
> —— 后者在任何环境下都必然失败，可稳定复现「明确报错 + 不崩溃 + 主图不截断」。

---

## 8. 手动验证清单（验收标准 ③）

1. 启动 SketchCAD，画几个图元；
2. 状态栏点「上次自动保存」→ 显示当前时间（手动触发一次）；
3. **任务管理器直接结束进程**（模拟崩溃/断电，绕过 `closeEvent`）；
4. 重新启动 → 弹出「图纸恢复」对话框，列出快照（时间/原路径/大小）；
5. 点「恢复所选」→ 内容与崩溃前一致；
6. 另一次崩溃后点「丢弃所选」/「全部丢弃」→ 二次确认后文件消失；
7. 正常退出后再启动 → **不再**提示（`.tmp` 已清理、无新快照）。

---

## 9. 不变式清单

| 不变式 | 保障方式 |
|---|---|
| 旧档全部可读 | `SketchModel` 只增不改；`toDrawingJson` 字段逐字照搬 |
| 手动保存与自动保存内容一致 | 同一 `toDrawingBytes()` 入口 |
| 快照写入不产生半截文件 | 校验写入字节数 + `.tmp` 中转 + `.bak` 留底 |
| 恢复不覆盖用户现有文件 | 三分支确认，默认焦点在「取消」 |
| 刚恢复不被立刻覆盖 | `recovering_` 抑制 |
| 用户数据不进安装目录 | 全部落 `%LOCALAPPDATA%/SketchCAD/*` |
| 无头自测目标纯净 | `SKETCHCAD_ENABLE_CRASHHANDLER` 宏 + dbghelp 仅链主目标 |
| 双目标零警告 | MinGW 下不用 `#pragma comment`；链接交给 CMake |

---

## 10. 遗留与后续

- **未实现**：快照数量上限 / 过期清理（当前只滚动单个文件的历史 `.bak`，跨图纸的旧快照
  需人工或「全部丢弃」清理）。商用化前建议加「保留最近 N 份 + 按天过期」。
- **未实现**：崩溃时把恢复提示做成非模态常驻横幅（当前是启动模态对话框，
  对超大图纸的启动速度有一帧影响）。
- **待办**：`AutoSave::opThreshold` 目前默认 0（关闭），命令行
  `autosaveinterval` / `autosavethreshold` 别名尚未接入 `AliasTable`。
