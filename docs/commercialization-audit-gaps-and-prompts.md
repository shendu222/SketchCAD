# SketchCAD 商用化复检报告：差距清单 + AI 编程提示词

> 复检对象：`sketch-cad-cpp/`（C++17 / Qt 6.10.1 Widgets / MinGW 64 / CMake+Ninja）
> 复检基线：截至「布局体系（任务 H）」与「启动菜单弹出修复」完成后的代码状态
> 文档用途：**以差距清单与可执行提示词为主体**，按优先级逐条实施，不做教程讲解
> 取代旧文档：`SketchCAD-主流CAD差异分析报告.md`、`二维CAD差距分析与AI编程实施方案.md`、`AI提示词驱动的CAD能力持续改进方案.md`（三者基于早期版本的结论已过期）

---

## 0. 复检证据（实测）

| 证据项 | 实测数据 / 代码事实 |
| --- | --- |
| 构建 | 双目标 `-Wall -Wextra` **零警告**（SketchCAD / SketchSelfTest） |
| 自测 | `build.sh test` **EXIT=0，通过 630 / 失败 0**（20+ 模块分组，含几何/修改/存档兼容/黄金样本/图层/块/GD&T/编组/布局） |
| 命令规模 | `Commands.cpp` 注册 111 条（`registerExact/Prefix`），另有 23 条 acad.pgp 别名 |
| 实体类型 | `ElementType` 20 类（含 Ellipse/Spline/Polyline/MText/Table/Hatch/Boundary/Xline/Point/MLeader/Fcf/BlockRef） |
| 文件格式 | 仅 **R12(AC1009)**：`toDxf/fromDxf/toDwg/fromDwg`；`fromDwg` 头部校验 `AC1009`；导入覆盖实体 ∩ 导出有限集 |
| 打印/布局 | 多布局 + 视口平铺 + 视口独立比例（新增），PNG/PDF 输出，STA/CTB 简化实现 |
| 性能实测（Benchmark FULL 模式，离屏软件光栅） | **1 万图元**：打开（场景重建）**47.9ms**，索引拾取 avg **0.005ms** / p99 0.011ms，全量 0.278ms（**54.0×**）<br>**5 万图元**：打开 **563.4ms**，索引拾取 avg **0.035ms** / max 8.552ms，全量 4.075ms（**115.8×**）<br>**10 万图元**：**`terminate called after throwing an instance of 'std::bad_alloc'`，进程异常终止（EXIT=3）——未能产出任何指标** |
| 性能结论 | 规模线性尚可（1万→5万 打开 47.9→563.4ms），但 **10 万图元直接内存分配失败崩溃**，且无后台线程/进度/取消/LOD/内存预算/失败降级机制（`QThread`/`QtConcurrent`/`QProgressDialog` 计数=0，无 `bad_alloc` 捕获） |
| 容错现状 | 全局异常处理器（`qInstallMessageHandler` / `SetUnhandledExceptionFilter`）计数 **0**；`try/catch` 仅见于插件与 COM 边界 |
| 持久化 | `QSettings` 使用数 **0**（无应用级首选项），自动保存/图纸恢复（`.bak`/`.sv$`/恢复对话框）**无** |
| 多文档 | `QTabWidget`/`addTab` 计数 **0**（单文档），最近打开文件列表（MRU）**无** |
| 交付资产 | `build.sh dist` 产出目录 + windeployqt（无 Installer / 卸载 / 版本资源 / 授权协议 / 更新通道） |
| 已知未完成 | 视口内图层冻结（per-viewport freeze）、视口夹点调整、`PlotViewport::rotationDeg` 未消费、核心层文字宽度仍为估算值 |

---

## 1. 复检结论：十大领域逐项验收

| 领域 | 现状 | 结论 |
| --- | --- | --- |
| 绘图命令 | line/circle/arc/rect/poly/spline/ellipse/polyline/xline/point + 多种参数化模式；缺 `HATCH/PEDIT/TEXT/MTEXT/INSERT(顶层)/DONUT` 等入口 | 基本完整，**命令命名与覆盖度落后** |
| 编辑命令 | move/rotate/scale/mirror/offset/trim/fillet/chamfer/explode/join/array*/stretch/align/lengthen/break_at/divide + 夹点编辑 + 形状选择 | **良好**，缺 `MATCHPROP/PURGE/OVERKILL/MEASURE/WBLOCK` |
| 图层与属性 | Layer 表 + ByLayer + 冻结/打印/透明度 + LTSCALE + layiso/layuniso/laymch + 属性面板 + QSELECT/FILTER | **良好（领先同类轻量级）**，缺图层状态/图层 filter tree |
| 块与外部引用 | 引用式块表 + 在位编辑 + 嵌套传播 + 属性/动态参数 + Xref 附加/卸载 + 设计中心 | **良好**，缺 `REFEDIT` 所见即所得、`WBLOCK`、`XBIND`、Xref 管理器面板 |
| 标注与文字 | 12 类标注 + GB/ISO 双标准 + DimStyle 表 + MLEADER/FCF(GD&T) + MText/Table/TextStyle | **强**，缺真实字体度量、`TOLERANCE`、引出线 Families |
| 捕捉与坐标系 | 14 种 OSNAP + POLAR/OTRACK/TK + 正交/栅格/隐式约束 + 在线 "三投影面" | 捕捉**良好**；**坐标系弱**（无命名 UCS / 动态 UCS） |
| 文件读写 | JSON 原生 + DXF R12 读/写 + DWG R12 读/写（自研二进制） | **最大短板**：仅 R12 |
| 打印与导出 | 页面设置 + PDF/打印/预览 + 多布局 + CTB 简化 | **基本可用**，缺 STB 完整、PUBLISH 图纸集、打印戳记 |
| 撤销重做 | Memento 快照覆盖几何/层/块/文字样式/标注样式/参数 | **基本可用**，表级配置与布局不入栈（见 P1-03） |
| 性能/容错 | 空间索引 54× 提速、增量场景同步 | **中等**：大图未验证、异步/容错缺位（见 P0-02/P0-04/P1-04） |

---

## 2. 与主流 CAD 对照（缺失/落后标红）

| 维度 | AutoCAD | ZWCAD | LibreCAD | SketchCAD 现状 | 判定 |
| --- | --- | --- | --- | --- | --- |
| DWG 原生读写 | R2018+ 全版本 | 全版本 | 无（QCAD Pro 需授权） | **仅 R12** | 🔴 落后 |
| DXF | 全版本 | 全版本 | R12/LT2 | **仅 R12** | 🔴 落后 |
| 命令命名/别名 | `L/C/PL/REC/H/MI/AR/MA/PU/OVERKILL/W/ED/PR/LT` | 同 AutoCAD | 同 AutoCAD | 自有命名 + 23 别名，`H/MA/PU/W/ED/PR/LT/OVERKILL` 缺 | 🔴 落后 |
| 多文档 | 支持（标签） | 支持 | 支持 | **单文档** | 🔴 落后 |
| 自动保存/崩溃恢复 | 支持 `.sv$` + Drawing Recovery | 支持 | 支持 autosave | **无** | 🔴 落后 |
| 选项持久化 | 支持（注册表/配置） | 支持 | 支持 | **无 QSettings** | 🔴 落后 |
| 布局多视口/比例 | 完整（含 per-viewport freeze/Paperspace 编辑） | 完整 | 部分 | 多布局+平铺+独立比例（本期新增），**缺视口冻结/夹点** | 🟡 部分 |
| GD&T/GB 标准 | 支持（含 Tolerance/FCF 部分） | 部分 | 弱 | **强**（12 类标注 + GB/ISO + FCF/MLeader + 国标图框） | 🟢 领先 |
| OSNAP/追踪 | 完善 | 完善 | 基础 | 完善（14 种 + POLAR/OTRACK/TK） | 🟢 相当 |
| 参数化/约束 | 完整 | 部分 | 弱 | 隐式约束 + 显式约束表 + 尺寸驱动 | 🟡 部分 |
| 块/Xref | 完整 | 完整 | 完整 | 引用式块 + 在位编辑 + Xref + 设计中心 | 🟡 相当 |
| 二次开发 | .NET/LISP/ARX/COM | LISP/.NET/COM | Script | **SDK + JS/Python/VBA + COM + CUIx + 别名** | 🟢 领先（生态量级除外） |
| 字体/排版 | SHX/TTF/字段/堆叠 | 完整 | 基础 | **估算度量**，无 SHX/字段 | 🔴 落后 |
| 性能（十万图元） | 流畅 | 流畅 | 一般 | **未验证/存疑** | 🔴 落后 |
| 安装/授权/许可 | 完整商业级 | 完整 | 开源免安装 | **仅目录分发** | 🔴 落后 |

---

## 3. 差距清单（按优先级）

### P0 阻塞商用

**P0-01｜文件格式仅 R12（DXF/DWG）**
- 表现：`toDxf/toDwg` 输出 R12(AC1009)；`fromDwg` 头部强校验 `AC1009`；`fromDxf` 面向 R12 组码。主流客户与 OUT 供应商图纸为 R2000/R2007/R2013/R2018 DWG → 打不开、读不全、写不出。
- 影响场景：接收客户图纸、向客户交付 DWG、与 ZWCAD/AutoCAD 协同——全部不可闭环，直接卡住任何付费交付。
- 涉及模块：`src/core/CadFormats.{h,cpp}`、`src/ui/MainWindow.cpp`（打开/保存过滤器 3009/3011/3177）、`tests/SketchSelfTest.cpp`（黄金样本）。
- 建议优先级：**最高**（商用前置）。

**P0-02｜零崩溃防护 + 无自动保存/图纸恢复**
- 表现：全局异常处理缺失（`SetUnhandledExceptionFilter`/`qInstallMessageHandler` 计数 0）；无定时自动保存、无 `.bak`、无异常退出后的恢复提示与回滚；`try/catch` 仅覆盖插件/COM。
- 影响场景：长时间绘图后崩溃 = 全部未保成果丢失；商业用户零容忍，直接导致流失。
- 涉及模块：`src/main.cpp`、新增 `src/core/AppSettings.{h,cpp}`、`src/core/AutoSave.{h,cpp}`、`src/ui/MainWindow.cpp`。
- 建议优先级：**最高**。

**P0-03｜无商业交付壳（安装器/版本/授权）**
- 表现：`build.sh dist` 仅产出目录 + Qt 运行库拷贝；无安装包/卸载入口/版本资源与文件属性/应用图标/许可协议/更新通道。
- 影响场景：发给客户即需手工拷 ZIP，无版本号无法定位问题，无授权无法商业化、无法控试用期。
- 涉及模块：`CMakeLists.txt`、`build.sh`、新增 `packaging/`（含 NSIS 或 Inno Setup 脚本、`resources/app.rc`、`LICENSE`）。
- 建议优先级：**最高**（交付前置）。

**P0-04｜十万图元加载直接崩溃（std::bad_alloc）**
- 表现（FULL 基准实测）：1 万图元打开 47.9ms、拾取 0.005ms（54×）；5 万图元打开 563.4ms、拾取 0.035ms（115.8×）；**10 万图元阶段 `std::bad_alloc` 抛出，进程 terminate（EXIT=3），无任何优雅降级与提示**。
- 影响场景：建筑/装配/地形类图纸常态在十万图元量级 → **打开即崩溃**，等同于程序对该类客户不可用；Windows 下还会被判定为"无响应"。
- 涉及模块：`src/ui/GraphicsView.cpp`（每图元一个场景项的增量同步）、`src/core/SpatialIndex.{h,cpp}`、`src/ui/MainWindow.cpp` 打开/导入路径、`src/core/CadFormats.cpp`（入库容器预留）、`bench/SketchBenchmark.cpp`（无 OOM 保护/无内存列）。
- 建议优先级：**最高**（已实测确认的崩溃，非推测风险）。

### P1 重要缺失

**P1-01｜应用级首选项不持久化（无 QSettings）+ 无「选项」对话框**

**P1-02｜高频命令缺失与命名不一致（H/PEDIT/MA/PU/OVERKILL/W/ED/PR/LT/ME/PUBLISH 等）**

**P1-03｜撤销/重做未覆盖表级状态（布局/单位/界限/捕捉/追踪/过滤器/Xref）**

**P1-04｜异常输入与损坏文件容错不足（无 fuzz/鲁棒性回归）**

**P1-05｜文字/表格排版不使用真实字体度量（估算宽度），屏幕与打印不一致**

**P1-06｜坐标系能力弱：无命名/动态 UCS，仅三投影面**

**P1-07｜打印发布链路不完整：视口内图层冻结、PUBLISH 图纸集、打印戳记、`rotationDeg` 未消费**

### P2 体验优化

**P2-01｜单文档：无多图标签页/窗口切换**

**P2-02｜无最近打开文件（MRU）与启动新图/模板流程**

**P2-03｜交互习惯差异：右键菜单、空格重复命令、Esc 取消、Tab 动态输入、OSNAP 快捷菜单、命令历史上下键**

**P2-04｜无多语言框架（QTranslator 计数 0）与高 DPI 策略未显式声明**

**P2-05｜帮助与容错输入：命令名补全、未知命令拼写纠错、命令索引面板**

**P2-06｜诊断与崩溃上报（可选）：版本+环境+日志一键收集**

---

## 4. AI 编程提示词正文

> 通用前置约定（每条提示词默认继承）：
> - 项目根目录 `sketch-cad-cpp/`；构建：`export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.10.1/mingw_64/bin:/c/Qt/Tools/Ninja:$PATH" && ./build.sh`（`/ dist` / `test`）
> - 硬约束：双目标 `SketchCAD` + `SketchSelfTest` 必须 **`-Wall -Wextra` 零警告**；`build.sh test` **EXIT=0**；新功能必须同时补自测断言并给出增/失败计数
> - 数据模型约束：`ElementType`/`LineStyle`/`ConstraintType` **只增不改**（旧档存整数序号）；JSON 缺键必须向后兼容回落
> - 输出：代码 + 自测分组 `t_xxx()` + 中文注释 + 一句话改动边界说明 + `docs/xxx-design-analysis.md`

---

### P0-01｜DWG/DXF 支持 R2000/R2007/R2013/R2018

```
【目标】
把文件格式从「仅 R12 (AC1009)」升级为「R12 + R2000(AC1015) 读写」，并让打不开的版本有清晰降级路径，
使 SketchCAD 能打开客户主流 DWG/DXF 并导出对方可接受的版本。

【涉及路径】
- src/core/CadFormats.h / CadFormats.cpp（toDxf / fromDxf / toDwg / fromDwg / DxfImportStats / DxfExportOptions）
- src/ui/MainWindow.cpp 打开/另存过滤器（约 3009/3011/3177 行）
- tests/SketchSelfTest.cpp（新增 t_formatsVersions）
- tests/golden/（新增 R2000 样本）

【功能与交互要求】
1. 优先采用第三方成熟内核（建议 libredwg 或 ODA Drawings SDK），以 CMake 可选特性接入；
   若不可用时**先实现 R2000 DXF（ASCII + Binary）读写**作为可达中间方案，不得只写导出不写导入。
2. 版本协商：读档先探测头部版本串（AC1009/AC1015/AC1021/AC1032…），受支持则进对应解析路径；
   不支持时不得崩溃，返回明确错误「不支持的文件版本：DWG 2013，请另存为 2000 或更早」并写入日志。
3. 导出提供版本下拉：R12 / 2000 / 2007（可先到 2000），默认沿用 R12（旧行为不变，opts==nullptr 必须逐字节等价）。
4. 打开/另存对话框的过滤器与默认扩展名随支持版本同步更新；命令行导出 DXF 增 `-v version` 选项。
5. 保真要求：图层/线型/颜色/块定义/标注/填充/文字样式必须随版本映射，不做“一律炸开”的退化。

【验收标准】
1. `tests/golden/` 下 R12/R2000 DXF+DWG 样本 round-trip：实体数量、图层、块定义一致性 ≥ 99%，自测断言新增 ≥ 10 条。
2. 读取不支持版本（如任意 R2018 文件）返回 false 且不崩溃，日志含版本提示；连读 20 个损坏/不支持文件进程不退出。
3. 旧档路径不变：`opts==nullptr` 的 toDxf 输出与改造前逐字节一致（用既有黄金样本比对）。
4. 双目标零警告，`build.sh test` EXIT=0，自测失败数 0。

【改动边界】
只改 CadFormats 层与其调用方（MainWindow 过滤器/导出入口），**不得改动 Element 结构体字段顺序**，
**不得引入 GPL 许可依赖**（若用第三方内核需确认许可并写入 docs/README 的第三方声明）。
```

---

### P0-02｜自动保存 + 崩溃防护 + 图纸恢复

```
【目标】
建立「崩溃不丢稿」的最小商业级保障：周期自动保存、备份文件、异常退出后可恢复、全局异常转储。

【涉及路径】
- 新增 src/core/AppSettings.{h,cpp}（QSettings 封装：HKCU 下 Software/SketchCAD）
- 新增 src/core/AutoSave.{h,cpp}（定时器 + 文件策略 + 清理）
- 新增 src/ui/CrashRecoveryDialog.{h,cpp}（启动扫描 + 恢复/丢弃）
- 新增 src/core/Diagnostics.{h,cpp}（日志落盘 %LOCALAPPDATA%/SketchCAD/logs）
- src/main.cpp（顶层异常安装 + 退出钩子）、src/ui/MainWindow.cpp（接线）

【功能与交互要求】
1. 全局异常：Windows 用 SetUnhandledExceptionFilter 写 mini-dump + 摘要日志；Qt 侧安装 qInstallMessageHandler，
   将 warning/critical/fatal 落盘（滚动 3 个文件，单个 ≤ 2MB）。
2. 自动保存：默认 10 分钟（可在「选项」改 5/10/15/30/关）+ 累计 N 次操作触发；先写临时文件再原子替换（临时名 `.tmp` → rename），
   主图对齐 `$drawing$.sv$`，并保留上一个版本 `.bak`。
3. 保存/任何写文件失败（只读、磁盘满、被占用）必须弹明确对话框 + 日志，禁止静默失败或写坏文件；磁盘剩 <100MB 时提前告警。
4. 下次启动若发现 `.sv$`：弹出「图纸恢复」对话框，列出文件（原路径/时间/大小），提供「恢复 / 丢弃 / 全部丢弃」；
   选择丢弃前必须二次确认，正常退出流程必须清理临时文件。
5. 状态栏显示「上次自动保存 HH:MM」并可手动触发。

【验收标准】
1. 自测新增 t_autosave 分组 ≥ 12 条：临时文件生成/原子替换/失败回滚/清理/启动扫描识别/恢复内容一致。
2. 无头测试模拟写失败（只读目录）→ 返回明确错误，进程不崩溃、主图文件未被截断。
3. 手动验证：启动后杀进程（任务管理器结束任务）→ 重启出现恢复对话框 → 恢复后内容与自动保存点一致。
4. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不修改既有 `SketchModel` 序列化格式；临时/配置文件只写在用户目录，**不得在程序安装目录写用户数据**。
```

---

### P0-03｜安装包 / 版本资源 / 授权壳

```
【目标】
产出可分发的商业安装包，带版本号、文件属性、图标、许可协议与卸载入口。

【涉及路径】
- CMakeLists.txt（版本、应用图标资源、app.rc）
- 新增 resources/app.rc、resources/logo.ico、LICENSE.txt、packaging/installer.iss（Inno Setup）或 packaging/installer.nsi
- build.sh（新增 `package` 模式：Release 构建 → windeployqt → iscc/nsis → 产出 SketchCAD-<版本>-Setup.exe + sha256）
- README.md（安装/升级/许可章节）

【功能与交互要求】
1. 三方版本统一：CMake `project(SketchCAD VERSION x.y.z)` → 版本宏注入 exe 文件属性（公司/产品/版权/版本），并显示在「关于」对话框。
2. 安装器：许可协议页（必须接受）、目标目录页（默认 `C:\Program Files\SketchCAD`）、开始菜单与桌面快捷方式、卸载程序（含注册表卸载项）、
   静默安装参数 `/VERYSILENT /DIR=` 便于企业部署。
3. 文件关联建议可选：`.json/.dxf` 关联到本程序（可在安装器勾选）；若做，卸载必须还原。
4. 升级：安装时检测旧版本并允许就地升级，保留用户目录配置。
5. 产物：`<name>-<version>-Setup.exe` 与 `checksums.sha256`，两者放 `dist/` 并打印路径。

【验收标准】
1. 全新虚拟机（无 Qt、无编译器）运行安装包 → 安装成功 → 打开程序、绘制/保存/打开图纸一条路径通过 → 控制面板卸载成功且无残留目录。
2. 「关于」显示的版本号与 exe 属性版本、安装包文件名三者一致。
3. `./build.sh package` 一条命令产出上述产物并打印校验和。
4. 既有 `./build.sh dist` 行为不变。

【改动边界】
只新增打包脚本与资源，**不得修改核心/UI 业务逻辑**；不得将 Qt 运行时以外的新第三方 DLL 隐式带入。
```

---

### P0-04｜十万图元崩溃修复：内存预算、异步加载、LOD 与基准体系

```
【目标】
修复"10 万图元打开即 std::bad_alloc 崩溃"这一实测确认的阻塞项，并让大图纸加载可预测：
后台加载 + 进度/取消 + 视图裁剪 + 重绘节流 + 内存预算与优雅降级 + 可复现基准。
（基线实测：1 万=打开 47.9ms/拾取 0.005ms；5 万=563.4ms/0.035ms；10 万=崩溃 EXIT=3）

【涉及路径】
- src/ui/GraphicsView.cpp（增量 items_/itemBounds_/spatial_ 同步、重绘节流）
- src/core/SpatialIndex.{h,cpp}
- src/ui/MainWindow.cpp（打开/导入流程）
- bench/SketchBenchmark.cpp（分档超时/基线/自动 SKETCH_BENCH_FAST）

【功能与交互要求】
0. **先修崩溃（必做前置）**：定位 10 万图元的内存放大点——逐图元场景项（`ElementItem`）开销、入库容器反复扩容、
   `SpatialIndex` 网格桶、离屏绘制缓冲；给出内存预算表（每图元约多少字节，目标 ≤ 1KB/图元），
   并在所有大批量分配处捕获 `std::bad_alloc`：改为「弹窗 + 日志 + 保持当前图纸可用」，**绝不允许 terminate**。
1. 打开大图：读取与场景项构建放到 `QtConcurrent`/`QThreadPool`，主线程显示模态 `QProgressDialog`（阶段：解析→入库→场景项），支持取消且取消后状态一致。
2. 只构建可见区场景项（视口裁剪 + 空闲时间补全），移动/缩放到新区域时按 LOD 补建；离屏/极小缩放比 (<0.05) 时启用简化绘制（关曲线细分/文字取景为方框）。
3. 平移/缩放期间重绘节流到 ~60fps，释放前的连续事件只保留最后一次；不得出现「节流到 0 帧」的白屏。
4. Benchmark 增加：每档软超时（默认 300s）、内存占用列、`--scale=all|10k|50k|100k` 参数、CI 友好的单行 CSV 输出。
5. 数据面板：加载完成后在状态栏显示「图元 N / 场景项 M / 打开 Xms」。

【验收标准】
1. **10 万图元不再崩溃**：FULL 基准 100k 档位必须产出完整指标行（不得出现 `bad_alloc`/EXIT≠0）；
   在最坏情况（内存不足）必须弹「内存不足，已停止加载」并保留程序可用，绝不 terminate。
2. 10 万图元：`打开(可交互) ≤ 8s`、`平移/缩放 ≥ 30fps`、峰值内存 ≤ 1.5GB（真实 GPU 环境）；进度条可取消且状态一致。
3. 视野外图元不产生场景项：断言 `场景项数 < 图元总数` 在初始（未全览）状态下成立。
4. Benchmark 三档全部产出完整行（含 50k/100k），新增内存峰值列与 CSV 输出，可被脚本解析；OOM 时记录为 `内存不足` 而非崩溃。
5. 自测新增 ≥ 10 条（进度/取消/只建可见/节流后重绘一次/`bad_alloc` 捕获后仍可用），双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得改变既有 hitTest/渲染结果的确定性（同一输入 → 同一输出）；不得引入新的 GUI 线程模型（仍在主线程做 QGraphicsScene 操作）；
内存优化不得牺牲 1 万/5 万档的既有指标（47.9ms / 563.4ms 不得劣化超过 10%）。
```

---

### P1-01｜应用首选项持久化与「选项」对话框

```
【目标】
把"每次启动回到默认"变成"记住用户习惯"：应用级配置持久化 + 统一「选项」对话框。

【涉及路径】
- 新增 src/core/AppSettings.{h,cpp}（QSettings + 默认值 + 变更信号）
- 新增 src/ui/OptionsDialog.{h,cpp}（分类树：常规/显示/文件位置/自动保存/打印/用户）
- src/ui/MainWindow.cpp（启动载入 / 关闭保存 / 工具→选项 入口）、src/main.cpp（高 DPI 读取早期）

【功能与交互要求】
1. 键值：窗口几何与面板布局自动恢复；网格显示/步长、正交、DYN、线宽显示、自动保存间隔、备份数、
   最近文件数、默认模板/存盘目录、打印默认设备与布局、语言、是否启用 GPU/离屏回退。
2. 「选项」对话框：左侧分类树 + 右侧表单，[确定/取消/应用]，取消必须回滚本次会话内的即时预览改动；
   [重置为默认值] 需二次确认。
3. 导出/导入设置（.ini 或 JSON）便于企业批量下发同一套默认配置。
4. 首选项读取不得阻塞启动；写入失败必须提示。

【验收标准】
1. 自测新增 t_appsettings ≥ 10 条：默认回填、改值后重载保持、导出/导入往返、损坏配置文件 → 回落默认且不崩溃。
2. 手工验证：改面板布局→关→开，布局保持；取消/重置行为符合 2/3 条要求。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
图形内容（图元数据）仍不允许写注册表；AppSettings 只放"应用习惯"，与"随图存档"的 SketchModel 状态严格分离。
```

---

### P1-02｜补齐高频命令与 AutoCAD 命名/别名

```
【目标】
让 AutoCAD/ZWCAD 熟练用户"肌肉记忆直接可用"：补齐高频命令、统一命名、扩充别名表。

【涉及路径】
- src/core/Commands.cpp（registerBuiltins 注册区）
- src/core/AliasTable.cpp（默认别名表）
- src/ui/MainWindow.cpp（菜单栏/工具条的同名入口）

【功能与交互要求】
1. 补齐顶层命令：`hatch`(H) 边界/图案填充交互、`bhatch`、`pedit`(PE 多段线编辑：闭合/合并/倒圆/样条化/宽度)、
   `matchprop`(MA 特性匹配，含设置排除项)、`purge`(PU 清理未用层/块/样式/类别)、`overkill`(去重/合并共线图元)、
   `wblock`(W 写块到文件)、`insert`(顶层 -I，含缩放/旋转/分解选项)、`text`/`mtext`(DT/MT)、`dtext`、`ddedit`(ED)、
   `properties`(PR)、`linetype`(LT 线型加载/当前/比例)、`measure`(ME)、`tol`、`explode` 保留、`regen`(RE 重生成)。
2. 别名：在默认别名表内补齐 `L/PL/REC/C/A/E/EX/MI/RO/SC/MA/PU/OVERKILL/W/I/ED/PR/LT/H/PE/DT/MT/ME/RE/UN/UNI` 等，
   支持用户 `alias.pgp` 覆盖，`aliasreload` 即时生效。
3. 每个新命令都要：中文用法提示、重复按 Enter 沿用上次选项（AutoCAD 惯例）、Esc 取消干净、命错时给出最接近命令建议。
4. 所有新命令必须同时经 `CommandInterpreter` 与 GUI 可用（菜单/工具条调用同一 handler），不得仅命令行实现。

【验收标准】
1. 自测新增 t_cmdParity ≥ 20 条：每条命令「执行成功 + 参数缺失给用法 + 非法参数被拒」三态。
2. 别名表 ≥ 60 条，`aliasreload` 后新别名可用，`filter/group/layout/mview` 等既有命令不受影响。
3. 旧行为不变：`trim/offset/fillet/chamfer/join/array*` 的既有自测全部仍通过。
4. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
只允许新增注册与别名；**不得修改既有命令名、不得调整 ElementType 枚举**；撤销语义一律走 beginEdit 单步。
```

---

### P1-03｜撤销/重做覆盖表级状态

```
【目标】
让 Ctrl+Z 成为用户心智模型中的"万能回退"，消除"几何回了、配置没回"的不一致。

【涉及路径】
- src/core/SketchModel.h（struct Memento、pushMemento、各 setXxx 入口）
- src/core/SketchModel.cpp（beginEdit/undo/redo）

【功能与交互要求】
1. Memento 扩列：追加 `layouts_` + `currentLayout_`、`units_`、`limits_`、`snapConfig_`、`trackConfig_`、`savedFilters_`、Xref 附加表。
   ⚠️ 必须与布局/存档 key 无关地下盘保持一致：随图存档字段照旧，只是快照多了这几项。
2. 区分两类状态：
   - 图纸内容（几何/图层/块/样式/布局）→ 入撤销栈，Ctrl+Z 可回退，redo 对称；
   - 应用习惯（AppSettings，见 P1-01）→ **不入栈**，明确文档化并在命令日志说明。
3. 快照去重：连续两次完全相同状态不重复入栈；单次批量操作仍必须单步撤销（既有 beginEdit 契约不变）。
4. 栈深：默认 100 步，可在「选项」调整；超限丢弃最旧。

【验收标准】
1. 自测新增 t_undoTables ≥ 12 条：每类表"改一次 → undo → 值还原 → redo → 值恢复"，且几何管理操作不受影响。
2. 混合用例：「改布局 + 移动图元 + 改单位」三步 → 三次 Ctrl+Z 全部还原 → 三次 Ctrl+Y 全部重做，自测断言通过。
3. 既有 630 条断言全部仍通过；双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得改变 Memento 既有字段含义与顺序（只追加到尾部）；旧档读取路径一字不改；不得把 AppSettings 混入撤销栈。
```

---

### P1-04｜异常输入与损坏文件鲁棒性

```
【目标】
任何输入都不能让程序崩溃、卡死或写坏用户文件——建立 fuzz + 黄金损坏样本回归。

【涉及路径】
- tests/SketchSelfTest.cpp（新增分组 t_robustness + t_fileFuzz）
- tests/golden/（新增损坏样本目录 corrupt/）
- src/core/CadFormats.cpp、src/core/SketchModel.cpp 读档路径、src/core/Element.cpp elementFromJson

【功能与交互要求】
1. 鲁棒性向量（每个都要有样本/生成器）：
   - 截断到 1%/10%/50% 的 JSON / DXF / DWG；
   - 随机字节翻转（每样本 ≤ 20 处）；
   - 非法几何：NaN/Inf 坐标、1e18 极值坐标、零半径圆、零长度直线、重复首尾点闭合；
   - 非法结构：块递归自引用、Xref 环路、超深嵌套（>64 层）、图元引用不存在的层/块/样式；
   - 非法编码：非法 UTF-8 文字内容、超长字符串（>1MB）、控制字符；
   - 缺失/非法类型键：ElementType 整数越界、负数 id、重复 id。
2. 处理策略优先级：**不崩溃 > 报错明确 > 部分降级加载 > 完全拒绝**；任何降级必须在日志写清丢弃/跳过了什么。
3. 永远保证：打开失败不得污染当前图纸状态；保存前必须先在内存构造完整数据再落盘（禁止边写边构造）。
4. 生成器写成一个可复跑的确定性脚本（固定种子），便于 CI。

【验收标准】
1. t_robustness ≥ 25 条断言，覆盖上述全部向量；每个向量必须有：返回明确错误、进程不退出、当前图纸状态未被污染三项。
2. 连跑 200 个随机损坏样本（固定种子）零崩溃、零死锁、单样本超时 ≤ 5s。
3. 每种降级路径都有一条日志断言（含被跳过实体数）。
4. 既有 630 条仍通过；双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得修改正常文件的解析结果（正常路径必须逐字节等价）；新增代码集中在 helper/守卫层，不动核心几何算法。
```

---

### P1-05｜文字/表格真实字体度量与排版保真

```
【目标】
解决"屏幕上看着对、打印出来位移/换行错"的问题：屏幕、打印、导出三处文字布局一致。

【涉及路径】
- src/ui/PlotRenderer.cpp、src/ui/GraphicsView.cpp（paintText/paintTable）
- src/core/Element.cpp（Text/MText/Table 字段）、src/core/Gdt.cpp（当前估算式 0.62）
- src/core/PrintEngine.cpp（modelBounds 里 textBoxOf 估算）
- 新增 src/core/TextMetrics.{h,cpp}（字体度量 + 缓存）

【功能与交互要求】
1. 引入真实度量：`QFontMetricsF`（按文字样式字体/宽度因子/倾斜）+ LRU 缓存（key=样式+字符），离屏/回调有 QFontDatabase 兜底。
2. 覆盖：单行 Text 的宽度/基线/9 点对齐（现仅支持左端 + 垂直居中）、MText 自动换行（按 `mtextWidth` 真实断行，
   含 CJK 断点规则）、行距因子、Table 列宽自适应（真实度量）与单元格溢出省略、标注文字宽度
   （现为估算，导致尺寸界线长度与文字重叠判断偏差）。
3. 堆叠分数（分数公差的上下堆叠）本轮不支持时给出明确提示，不得静默退化。
3. 三处同源：屏幕渲染、打印渲染、`modelBounds` 取景必须调用同一度量 API（不得三套实现）。
4. 字体缺失/离屏无字体时降级为估算并写日志（保留既有"豆腐块"环境下的可用性），估算误差需在文档中注明上限。
5. 批处理安全：度量 API 必须可在无 QGuiApplication 的无头环境返回确定值（必要时提供 `setFallbackCharWidth`）。

【验收标准】
1. 自测新增 t_textMetrics ≥ 12 条：度量 vs 估算偏差、9 点对齐坐标、换行结果、列宽自适应、三同源一致性（屏幕/打印/取景三者 boundingRect 相等）。
2. 回归：GD&T 既有 12 类标注断言、Table/MText 序列化断言全部通过。
3. 打印样张比对：同一图纸导出 PDF 与屏幕如 nick? .

Rewrite:
3. 打印样张比对：同一图纸导出 PDF，其标注文字外接框与屏幕 modelBounds 差异 ≤ 0.5mm（以 BNF 断言或导出几何校验）。
4. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得修改既有 JSON 字段名与默认回落值（新增字段仅在缺键时补齐）；不得改变既有图元 pts 语义。
```

---

### P1-06｜命名/动态 UCS 与坐标系能力

```
【目标】
从"三投影面"升级为可用的 UCS 体系，支撑斜视图/局部斜座标的实际制图需求。

【涉及路径】
- src/core/SketchModel.h（plane_ 状态）、src/core/Commands.cpp（plane/ucs 命令）
- src/ui/GraphicsView.cpp（网格/正交/极轴/坐标显示/OSNAP 均基于世界 XY 的假设）
- src/core/Snap.{h,cpp}、src/core/ConstraintSolver.cpp

【功能与交互要求】
1. 新增命名 UCS：三点/对象/Z 轴/视图/世界五种定义方式 + `ucs list/set/save/restore/delete` 命令 + 状态栏 UCS 指示器 + 图标显示。
2. 世界↔用户坐标双向换算收口到**唯一入口**（建议 `SketchModel::toUcs/toWorld`），所有输入（命令行点、参数条、动态输入）与显示都经它。
3. 正交/极轴/栅格/OSNAP 必须按当前 UCS 轴对齐（当前只按世界 XY）；坐标读数支持 WCS/UCS 切换显示。
4. 动态 UCS（绘制时对临时工作平面自动吸附）列为可选子项，先保证静态命名 UCS 全链路正确。

【验收标准】
1. 自测新增 t_ucs ≥ 12 条：换算往返、命令 CRUD、极轴对齐角度、坐标读数随切换变化、网格方向随 UCS 旋转。
2. 断言：在 45° UCS 下用命令行输入 `@100<0` 落点，世界坐标符合预期（误差 ≤ 1e-6）。
3. 既有 plane XY/YZ/XZ 三态命令行为不变，既有 630 条通过。
4. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得修改图元存储的世界坐标语义（UCS 只是输入/显示变换）；不得改动 Element 字段；既有三平面命令行为保持兼容。
```

---

### P1-07｜打印发布链路补齐

```
【目标】
把布局体系补齐到"能替代 AutoCAD 出图"的完整度：视口冻结、批量发布、打印戳记、视口旋转。

【涉及路径】
- src/core/Layout.h/.cpp（PlotViewport 追加 per-viewport 冻结集合，只增不改）
- src/ui/PrintEngine.h/.cpp（renderPageBody / exportPdfLayouts）
- src/ui/PlotRenderer.cpp（打印可见性裁决）
- src/ui/MainWindow.cpp（新增「发布」入口 + 图纸集列表）

【功能与交互要求】
1. per-viewport freeze：`PlotViewport` 追加 `QStringList frozenLayers`（序列化缺键 → 空），
   渲染某视口时该视口内这些图层不输出（模型空间与其他视口正常）；`mview` / `layout vpfreeze` 命令 + 视口属性面板编辑。
2. PUBLISH：新增「发布」对话框（勾选布局 + 目标：单个多页 PDF / 每布局一个 PDF / 物理打印机 + 页码/顺序/页面设置覆盖），
   复用既有 `PrintEngine::exportPdfLayouts` 与 COM `BatchPlotEngine`，GUI 与 COM 两条路径结果必须一致。
3. 打印戳记：可选叠加"图名/日期/时间/布局名/比例/文件名"，字体与位置可配（默认关闭，旧行为不变）。
4. 消费 `PlotViewport::rotationDeg`：视口内容旋转出图（当前字段存在但未消费）。

【验收标准】
1. 自测新增 t_plotPublish ≥ 10 条：per-viewport 冻结生效且不影响模型空间与其他视口、发布多布局返回正确页数、
   戳记开关默认关闭（旧样张逐字节等价）、rotationDeg=90 出图几何正确。
2. 旧档兼容：不含 `frozenLayers` 的存档读回为空冻结集，渲染结果与改造前一致。
3. COM BatchPlot 与 GUI 发布同一图纸集 → 页数一致、PDF 均非空。
4. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
`PlotViewport` 只追加字段（不得调整既有 4 字段）；既有 `exportPdf` 单页行为不得变化。
```

---

### P2-01｜多文档标签页

```
【目标】
支持同时打开多张图纸并快速切换，满足"对照两张图"的真实作业方式。

【涉及路径】
- src/ui/MainWindow.cpp（中央部件的容器层）、新增 src/ui/DocTab.{h,cpp}（Tab + Document 会话）

【功能与交互要求】
1. 每个文档拥有独立 SketchModel + GraphicsView + Undo 栈 + 命令行历史 + 脏标记；标签显示文件名 + `*` 未保存。
2. 切换文档时：活动工具、选择集、坐标系、撤销栈随文档切换；关闭前脏标记弹「保存 / 不保存 / 取消」。
3. 剪贴板跨文档粘贴（既有 Clipboard Payload 已 JSON 序列化，直接复用）、`Ctrl+Tab` 循环切换、拖拽标签重排。
4. 单文档模式入口（如并存需求）不得破坏现有 open/save JSON 流程。

【验收标准】
1. 自测新增 t_multidoc ≥ 8 条：开 3 张图 → 各自独立 undo 不串编辑相互影响、脏标记切换、跨文档复制粘贴内容一致。
2. 既有单文档流程（打开/保存/另存/新建）行为不变，630 条仍通过。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得改变 SketchModel 的 JSON 存储格式；不得让多文档引入第二个模型真相源（每个 Doc 一份模型，禁止全局单例模型）。
```

---

### P2-02｜最近文件（MRU）与启动流程

```
【目标】
把"启动=空图"改成"启动=继续上次工作"，并支持模板化新建。

【涉及路径】
- 新增 core/RecentFiles.{h,cpp}（复用 AppSettings 存储）
- ui/MainWindow.cpp（文件菜单 MRU 区 + 启动新图/模板入门页）、ui/NewDrawingDialog.cpp

【功能与交互要求】
1. MRU：最多 10 项，存路径+打开时间，缺失文件灰显并提示"路径不存在"（点击可删除单条），支持固定（pin）与右键清除。
2. 启动页：新建（空白 / 标准模板 A0-A4 + 标题栏样式）/ 打开最近 / 打开其他；上次未正常保存时优先显示恢复项（配合 P0-02）。
3. 模板：既有参数化模板与新图from spec 复用同一 new drawing 流。

【验收标准】
1. 自测新增 t_recent ≥ 8 条：增删/去重/上限裁剪/缺失文件处理/持久化重载前后一致。
2. 手工：关闭再启动，MRU 与上次窗口布局保持；双击缺失项不崩溃。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
只新增 UI 与存储封装；不得修改既有图纸 JSON 格式与 new drawing 已有业务逻辑。
```

---

### P2-03｜交互习惯对齐（AutoCAD/ZWCAD 肌肉记忆）

```
【目标】
消除"会用 AutoCAD 但在这里点不惯"的摩擦，按清单逐项对齐。

【涉及路径】
- src/ui/GraphicsView.cpp（右键/中键/滚轮/Shift+右键/双击）
- src/ui/CommandPrompt.*、src/core/CommandInterpreter.cpp（历史、重复命令、补全）
- src/ui/MainWindow.cpp（快捷键）

【功能与交互要求】
1. 必须支持：右键=确认/重复上一命令；中键拖拽平移、滚轮缩放（已有则保持）；Shift+右键=OSNAP 快捷菜单（14 种可临时勾选与取消）；
   Ctrl+右键=捕捉/追踪快捷项；Esc 层层取消（命令→选项→选择）不误杀已提交结果；空格/Enter 重复上一命令；
   ↑/↓ 翻命令历史（含持久化的最近命令，重启可用）；Tab 在动态输入框之间切换字段。
2. 命令输入容错：未知命令给出最相近 3 个候选（`Did you mean:` 风格中文提示）；前缀匹配自动补全 + Tab 补齐。
3. 双击行为：文字/标注/属性值 → 就地编辑；块 → 進入在位编辑（#P2 可选项）；否则打开属性面板。

【验收标准】
1. 自测新增 t_interaction ≥ 12 条（可借助 QTest 键盘/鼠标事件 + 无头 GraphicsView 实例）：每类手势/按键触发正确行为。
2. 与既有流程不冲突：既有自定义右键行为（如有）需在选项里可切换，默认遵循上述约定。
3. 630 条既有断言仍全通过；双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得修改命令语义本身；快捷键冲突时以"新增可配置、默认取 AutoCAD 值"处理。
```

---

### P2-04｜多语言框架与高 DPI

```
【目标】
具备国际化的工程能力与清晰的显示策略，为出海与高分屏用户提供基础。

【涉及路径】
- 新增 resources/translations/*.ts（Qt Linguist）
- src/main.cpp（早期 SetProcessDpiAwarenessContext / AA_EnableHighDpiScaling + QTranslator 装入）
- src/ui/*.cpp 的字符串出口检查（QStringLiteral 全量覆盖）

【功能与交互要求】
1. 多语言：所有可见字符串（菜单/对话框/日志/命令提示）走 `tr()`；中文为源语言，产出英语 .qm；运行时按系统语言 + 「选项→语言」覆盖，切换即刻生效。
2. 高 DPI：声明 Per-Monitor V2 awareness；验证 100/125/150/200% 缩放下画布线宽、图标、网格、打印预览与导出 PDF 均无错位/裁切/锯齿异常。
3. 单位/小数/日期 一律走 QLocale 或既有 UnitsMeta 口径，不得硬编码中文/毫米习惯到字符串里。

【验收标准】
1. 自测新增 t_i18n ≥ 6 条：翻译上下文存在、缺翻译回落中文、语言切换后关键菜单文本变化。
2. 125%/150% DPI 下 Exe 运行无错位、无"图像拉伸"；打印导出 DPI 元数据正确。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得改动任何图元数据/JSON 键名；不得因翻译导致命令名变化（命令名仍为 ASCII）。
```

---

### P2-05｜帮助与命令发现

```
【目标】
让用户不用翻文档也能找到命令、纠正错误输入。

【涉及路径】
- src/core/Commands.cpp（既有 help 命令 + CommandRegistry 元数据）
- 新增 src/ui/CommandPalette.{h,cpp}（Ctrl+Shift+P 命令面板）

【功能与交互要求】
1. 命令面板：模糊搜索中文名/英文名/别名，显示分类、当前快捷键、简介，Enter 执行；支持记录最近使用。
2. `help <cmd>` 输出用法、参数、样例、相关命令；未知命令输出相近候选（复用 P2-03 的相似度函数）。
3. 快捷键冲突自检：启动时扫描重复绑定，若有则以 warn 日志输出。

【验收标准】
1. 自测新增 t_help ≥ 6 条：命令面板过滤结果、`help` 输出含 Usage、未知命令给候选、重复快捷键被检测。
2. ibrary 命令总数与注册总数一致（断言 == 注册表大小 + 别名单列）。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
不得修改任何既有命令的行为与名字；帮助文本更新不得影响命令解析。
```

---

### P2-06｜诊断与崩溃一键上报（可选）

```
【目标】
把"客户说崩了"变成"客户发一个包即可复现"。

【涉及路径】
- 新增 src/core/Diagnostics.{h,cpp}、src/ui/DiagnosticsDialog.{h,cpp}
- src/main.cpp（异常钩子接入）

【功能与交互要求】
1. 「导出诊断包」：一键打包最近日志 + mini-dump + 版本/Qt/OS/显卡信息 + 当前图纸元信息（不含几何内容），输出到用户目录 ZIP。
2. 崩溃后下次启动提示"上次异常退出，是否发送诊断包/查看日志"。
3. 遥测（可选，默认关闭）：匿名会话计数/崩溃计数，必须在「选项→隐私」可关闭并在首启动明示。

【验收标准】
1. 自测新增 t_diag ≥ 6 条：ZIP 生成、含预期文件、信息字段完整、隐私开关默认关闭时零外发请求（网络层 mock 断言）。
2. 手动：杀进程后重启，出现提示并能导出诊断包。
3. 双目标零警告，`build.sh test` EXIT=0。

【改动边界】
默认不得有任何网络外发；不得输出用户图元几何/文本内容到日志。
```

---

## 5. 执行顺序建议

| 批次 | 条目 | 目的 |
| --- | --- | --- |
| 第 1 批 | P0-02 → P0-03 → P0-01 | 先立"不丢稿 + 可交付 + 开得开客户图"的商业底线 |
| 第 2 批 | P0-04 → P1-01 → P1-04 | 十万图元不崩（必做）、习惯记住、坏文件不崩 |
| 第 3 批 | P1-02 → P1-03 → P1-07 | 命令覆盖、撤销完整性、出图闭环 |
| 第 4 批 | P1-05 → P1-06 → P2-* | 排版保真、UCS、多文档与体验细节 |

> 每批完成后固定执行：双目标零警告 → `build.sh test` EXIT=0 → 断言计数递增记录 → 导出 `docs/<feature>-design-analysis.md` → 更新 `.workbuddy/memory`。
