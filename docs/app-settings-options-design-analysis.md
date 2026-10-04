# 应用级配置持久化 + 统一「选项」对话框 — 设计分析

> 需求：把"每次启动回到默认"变成"记住用户习惯"——应用级配置持久化 + 统一「选项」对话框。
> 涉及：`src/core/AppSettings.{h,cpp}`（扩展）、`src/ui/OptionsDialog.{h,cpp}`（新增）、
> `src/ui/MainWindow.{h,cpp}`、`src/main.cpp`。

## 1. 需求与验收对照

| 需求 | 实现 | 状态 |
|---|---|---|
| 窗口几何与面板布局自动恢复 | `window/geometry`、`window/state`（base64）+ `restoreGeometry/restoreState` | ✅ |
| 网格显示/步长、正交、DYN、线宽显示 | `display/*` 六键 → SketchModel 单一状态源 | ✅ |
| 自动保存间隔、备份数 | `autosave/intervalMinutes`（沿用旧键）、`autosave/backupCount` | ✅ |
| 最近文件数（+最近文件列表） | `general/recentFileCount` + `files/recentList`（文件菜单新增「最近打开」子菜单） | ✅ |
| 默认模板/存盘目录 | `files/defaultTemplate`、`files/defaultSaveDir`（+ `files/lastDir` 自动记忆） | ✅ |
| 打印默认设备与布局 | `print/device`（直接打印预选）+ `print/defaultSettings`（PrintSettings JSON 快照，退出时自动记住） | ✅ |
| 语言 | `general/language`（zh-CN/en；en 为预留，界面尚无 i18n 框架） | ✅（存储层） |
| 是否启用 GPU/离屏回退 | `display/gpuAcceleration`、`display/offscreenFallback`（渲染后端消费点预留） | ✅（存储层） |
| 选项对话框：分类树 + 确认/取消/应用 + 取消回滚 + 重置二次确认 | `OptionsDialog`（常规/显示/文件位置/自动保存/打印/用户 六页） | ✅ |
| 导出/导入设置 | `exportToJson/importFromJson`（`.json`；window/ 组不下发） | ✅ |
| 首选项读取不阻塞启动 | 全部为注册表/ini 单键懒读（毫秒级），无文件扫描 | ✅ |
| 写入失败必须提示 | `syncAndCheck/lastWriteFailed/lastError`；对话框应用/取消时弹窗；退出时弹窗+日志 | ✅ |
| 自测 t_appsettings ≥10 | 13 组 CHECK（默认回填/重载保持/脏数据回落/信号/往返/损坏/未知键/重置/文件/写失败/损坏文件/打印JSON/导出边界） | ✅ |
| 双目标零警告、build.sh test EXIT=0 | 见 §7 | ✅ |

## 2. 架构决策

### 2.1 扩展而非重写 AppSettings

`AppSettings` 在「崩溃不丢稿」特性中已存在（自动保存间隔 + 磁盘阈值两键，且 `t_autosave` 已覆盖）。
本次演进为**通用键值层**，原接口保持二进制兼容：

```
AppSettings
├── 通用层：value(key) / setValue(key,v) / contains / resetToDefaults
├── 规格表：specTable() —— 24 个键 × {默认值, 校验器} 的唯一数据源
├── 导入导出：exportToJson(Data) / importFromJson(Data)
├── 写失败：syncAndCheck / lastWriteFailed / lastError
└── 类型化便捷访问器（全部委托通用层，零独立状态）
```

**不变式：任何键的默认值/校验规则只存在于 specTable() 一处**。读取回填、写入校验、
导出、导入、重置、选项对话框的"重置为默认值"全部消费同一张表——不存在
"一处更新一处漏"（与 writeDrawing 版本号教训同源的根治手段）。

### 2.2 校验按「值」不按「类型」

QSettings 注册表后端把布尔/整数/浮点全部以字符串存取。若校验按 `QVariant::type()` 判定，
注册表往返会把 `"true"` 误判为非法。因此校验器一律按值收窄：

- 布尔：`Bool` 类型直通；`Int/LongLong` 仅接受 0/1；字符串仅接受 `"true"/"false"`；
- 整数：`Int/LongLong/UInt` 直通；`String` 走 `toInt(&ok)`；每键有独立范围；
- 字符串：仅 `String`（长度 >4096 视为脏数据拒绝）；
- 列表：`StringList` 直通，单 `String` 宽容为单元素列表；
- base64（geometry/state）：必须能 `fromBase64` 解开。

任何校验失败的读取一律**回落默认**——损坏配置文件绝不崩溃、绝不扩散（验收标准 1）。

### 2.3 「即时预览 + 取消回滚」的单一信号通道

```
OptionsDialog 控件 ──collectWidgets──▶ AppSettings::setValue ──settingChanged(key,v)──▶ MainWindow::applySettingLive
      ▲                                                                                    │
      └── loadWidgets（打开时快照回滚）                                        写入真实归宿（模型/自动保存/最近文件）
```

- **[应用]/[确定]**：控件 → `setValue`（逐键，幂等，立即 sync）→ 信号 → 主窗口即时生效 = 预览；
- **[取消]**：打开对话框时的全键快照逐键写回 → 走**同一条信号通道** → 模型/自动保存/菜单
  同步回到原状态。已点过「应用」的会话内改动一并回滚（需求 2）；
- **[重置为默认值]**：仅把控件回填为 specTable 默认值（草稿态），须再点确定/应用才生效；
  点击时 `QMessageBox::question` 二次确认（默认按钮 No）。

`applySettingLive` 是所有显示开关的唯一分派点：把值写入真实归宿（`model_->setShowGrid`
等 SketchModel 单一状态源 setter），不另存第二份状态。启动回放复用同一函数。

### 2.4 写失败的提示链路

`setValue` 内部**立即 `sync()`**（而非等析构/事件循环）→ 失败当场可检：

- 对话框 [应用]/[取消]：`syncAndCheck()` 失败 → `QMessageBox::warning`；
- 程序退出：`saveAppSettingsOnClose()` 保存几何/布局/显示开关/最近文件/打印设置后
  `syncAndCheck()` 失败 → 弹窗 + `Diagnostics::writeLog`；
- QSettings 语义澄清（自测 #10 发现）：写失败后**内存缓存仍持有新值**（会话内可用），
  提示用户的是"重启后可能丢失"；真正"损坏 → 回落默认"属于读路径（自测 #10b 单独覆盖）。

### 2.5 导出/导入（企业批量下发）

- 格式：JSON `{"format":"SketchCAD-Settings","version":1,"values":{…}}`；
- `window/` 组（几何/布局）属**本机状态**，不参与导出；
- 导入：JSON 非法 → `false` 且当前值原样保持；**仅接受已知键**、逐键走完整校验——
  新版配置文件在旧版程序上只落可识别的键（前向兼容），未知键/非法值静默跳过不中断。

### 2.6 高 DPI 早期读取（main.cpp）

Qt6 中高 DPI 缩放**恒启用**（`AA_EnableHighDpiScaling` 已弃用），启动前唯一可配置的是
**缩放舍入策略**。`main()` 在 `QApplication` 构造之前用独立 `QSettings` 读
`display/highDpiRoundingPolicy`（注册表单键读取，毫秒级，不阻塞启动），映射到
`QGuiApplication::setHighDpiScaleFactorRoundingPolicy`；非法值回落 `Round`（与
specTable 校验一致）。该键可在「选项」对话框之外由企业部署直接写注册表。

### 2.7 与「随图存档」的严格边界（改动边界）

| 归宿 | 内容 |
|---|---|
| **AppSettings（注册表）** | 应用习惯：窗口几何/布局、显示开关默认值、自动保存参数、最近文件、默认目录、打印设备偏好、语言、GPU/离屏开关、用户信息 |
| **SketchModel（随图 JSON/DXF）** | 图形内容：图元、图层、块表、文字样式、标注样式、布局表、捕捉模式（OSNAP）、追踪配置、单位/界限、绘图比例 |

显示开关（栅格/正交/吸附/DYN/线宽）的**运行时状态源仍是 SketchModel**；
AppSettings 只在启动时提供初值、退出时抄回终值——模型状态永不写入注册表，
注册表状态永不进入图档。捕捉设置（OSNAP）与追踪配置属"随图属性"，故不入选项对话框。

## 3. 键规格表（24 键）

| 分组 | 键 | 默认 | 校验 |
|---|---|---|---|
| general | `general/language` | zh-CN | 白名单 {zh-CN, en} |
| general | `general/recentFileCount` | 10 | 0..20 |
| display | `display/showGrid` | true | 布尔 |
| display | `display/gridSnap` | false | 布尔 |
| display | `display/gridSnapStep` | 1.0 | double 1e-3..1e6 |
| display | `display/ortho` | false | 布尔 |
| display | `display/dynMode` | true | 布尔 |
| display | `display/showLineWeight` | false | 布尔 |
| display | `display/gpuAcceleration` | true | 布尔 |
| display | `display/offscreenFallback` | false | 布尔 |
| display | `display/highDpiRoundingPolicy` | Round | 白名单 {Round, Floor, Ceil, RoundPreferFloor, PassThrough} |
| files | `files/defaultSaveDir` | （空） | 字符串 |
| files | `files/defaultTemplate` | （空） | 字符串 |
| files | `files/lastDir` | （空） | 字符串（自动记忆，非对话框编辑） |
| files | `files/recentList` | （空列表） | StringList |
| autosave | `autosave/intervalMinutes` | 10 | 白名单 {0,5,10,15,30}（沿用旧键，旧数据无缝继承） |
| autosave | `autosave/backupCount` | 1 | 0..9 |
| system | `system/lowDiskThresholdMb` | 100 | 1..1e6（沿用旧键） |
| print | `print/device` | （空） | 字符串 |
| print | `print/defaultSettings` | （空） | 非空必须可解析为 JSON 对象 |
| user | `user/name` | （空） | 字符串 |
| user | `user/company` | （空） | 字符串 |
| window | `window/geometry` | （空） | base64 可解（不下发导出） |
| window | `window/state` | （空） | base64 可解（不下发导出） |

## 4. MainWindow 集成点

- **启动**（构造末尾 `loadAppSettingsOnStartup`）：`restoreGeometry/restoreState`（恢复成功
  则 `firstShow_=false`，跳过首显居中校正——否则会覆盖用户上次的位置习惯）；
  显示开关经 `applySettingLive` 回放；最近文件列表载入；`print/defaultSettings` 回放为
  `currentPrintSettings_` 起始值；连接 `settingChanged → applySettingLive`。
- **关闭**（`closeEvent → saveAppSettingsOnClose`）：`saveGeometry/saveState`、显示开关从
  模型抄回、最近文件、打印设置快照；`syncAndCheck` 失败弹窗+日志。
- **入口**：新增顶级菜单「工具(&T) → 选项...」（`menuTools`）；文件菜单新增「最近打开(&R)」
  动态子菜单（去重置顶、按 `recentFileCount` 截断、含"清除历史"）。
- **文件对话框目录习惯**：起始目录 = `files/defaultSaveDir` > `files/lastDir` > 系统默认；
  打开/保存成功后自动更新 `lastDir`。
- **直接打印**：`print/device` 存在且可用时预选设备；打印成功后记住实际设备名。
- **测试隔离**：`MainWindow::s_recentPersistEnabled`（默认 true）——自测中读档不会写注册表。

## 5. 「选项」对话框页面

| 页 | 控件 |
|---|---|
| 常规 | 语言下拉（zh-CN/en）、最近文件数 Spin(0..20) |
| 显示 | 显示栅格、栅格吸附、吸附/栅格步长(0.001..1e6 mm)、正交、DYN、显示线宽、GPU 加速（预留）、离屏回退（预留） |
| 文件位置 | 默认存盘目录（编辑框+浏览）、默认模板文件（编辑框+浏览） |
| 自动保存 | 间隔下拉（关/5/10/15/30）、备份保留份数(0..9)、磁盘告警阈值(1..1e6 MB) |
| 打印 | 默认打印机下拉（〈系统默认〉+ 可用设备名） |
| 用户 | 姓名、公司/单位 |

组合框 data 字段存实际值（如间隔分钟数、打印机名），text 存文案——不存在"第 N 项"魔法下标。

## 6. 自测清单（t_appsettings，13 组）

1. 全新存储全部已知键 == 默认值 + 既有访问器 10/100；
2. 改值 → 重新构造 AppSettings → 全部保持（持久化语义）；
3. 七类脏数据（非法白名单/越界/脏布尔串/非白名单语言与策略）→ 全部回落默认；
4. `settingChanged` 携带 key/value、同值与写默认值幂等不重复发；
5. 导出/导入内存往返（改乱当前值后导入恢复）；
6. 损坏 JSON → 导入失败、错误信息非空、当前值原样保持、不崩溃；
7. 含未知键的合法 JSON → 已知键生效、未知键不落入存储（前向兼容）；
8. 重置为默认值 → 存储清空 + `settingsReset` + 逐键广播默认；
9. 文件级导出/导入往返（两个独立实例）；
10. 写失败（父路径为普通文件）→ `lastWriteFailed/lastError` 置位 + 会话内内存值保持不崩溃；
10b. 损坏 ini 文件 → 全部键读回落默认且不崩溃（验收标准 1 原文场景）；
11. `print/defaultSettings` 值校验：非 JSON 回落空串、合法 JSON 保存；
12. 导出不含 `window/` 组 + 几何/布局 base64 往返一致。

## 7. 验收数据

- 构建：`cmake --build ... --target SketchCAD SketchSelfTest -j 1` → EXIT=0，`grep -ci warning` = **0**；
- 自测：`SketchSelfTest` 通过 **779/0**，EXIT=0（含 t_appsettings 13 组、t_autosave 既有用例全绿）；
- 全量回归：`./build.sh test` EXIT=0（见会话记录）。
- 手工验证路径（真机）：改面板布局 → 关 → 开，布局保持；选项改栅格/正交 → 即时生效 →
  取消 → 回滚；重置 → 二次确认；导出 JSON → 清空注册表 → 导入恢复。

## 8. 边界与遗留

- **语言**：`general/language` 键与下拉已就位，但界面文案为中文硬编码（无 QTranslator 框架），
  选 English 仅记录偏好——i18n 是独立特性，落地后此键即被消费。
- **GPU/离屏回退**：键 + 开关已就位，渲染后端尚未提供消费点（当前为纯 QPainter 栅格渲染），
  与语言同属"存储层先行"。
- **默认模板**：键与文件选择已就位；`applyNewDrawing` 消费（新建时优先加载模板文件）留作
  下一增量——避免在本次改动中触碰新建流程的既有测试路径。
- **备份份数（backupCount）**：键与设置页就位；`.bak` 轮转机制待接入 `writeDrawing`。
- 注册表路径 `HKCU\Software\SketchCAD` 与「崩溃不丢稿」时期的既有数据无缝兼容
  （autosave/intervalMinutes、system/lowDiskThresholdMb 两键沿用旧名）。
