# SketchCAD 与主流二维 CAD 差距分析报告（工程化应用）

> **对象**：本仓库 `sketch-cad-cpp`（产品名 SketchCAD，C++17 / Qt 6.10.1 Widgets / MinGW-w64 GCC 13.1 / CMake+Ninja）  
> **版本基线**：`CMakeLists.txt` 中 `project(SketchCAD VERSION 1.1.0)`；证据取自工作副本源码（含未提交改动）  
> **对标产品**：AutoCAD 2025（二维工作集）、中望 CAD 2025、CAXA 电子图板 2023；开源对照 LibreCAD / QCAD  
> **方法**：以源码与自测为事实源，不以过期文档为准。仓库内 `docs/SketchCAD-主流CAD差异分析报告.md`、`docs/二维CAD差距分析与AI编程实施方案.md`、`docs/commercialization-audit-gaps-and-prompts.md` 的多项结论**已过期**（例如“无打印/无脚本/仅 5 种捕捉/仅 R12”），本报告取代其作为 2026-10-10 决策基线。  
> **定位声明**：SketchCAD 是**面向航空/机械工程的二维制图系统**，不是三维 CAD。三维能力不计入“应补齐”清单，仅在覆盖度表中标注“产品边界外”。

---

## 0. 一句话结论

SketchCAD 已经从“演示级画板”进入**可用的轻量二维工程 CAD**：日常绘图、国标出图、块/参照、布局打印、命令行习惯、崩溃恢复、二次开发入口都已具备骨架。与 AutoCAD/中望/CAXA 的差距，不再是“没有图层/没有打印”，而是：

1. **互操作保真**：DWG 真内核与 R2004–R2018 写出、代理对象/动态块/注释性/SHX 等语义损失；  
2. **产能级深度**：关联阵列、图纸集、图层状态、注释性比例、工业级约束核；  
3. **正式工程化交付**：许可证与激活、代码签名证书落地、更新通道、跨平台、PLM 对接、现场级 DWG 验收。

**可以**：中小团队用 JSON 原生档 + DXF/R2000 DWG 做国标零件图、模板三视图、内部出图。  
**还不能**：作为设计院主 CAD 无条件承接十年存量 DWG、替代 AutoCAD 企业许可、支撑十万级装配图流畅编辑与外部 LISP 生态。

---

## 1. 当前能力快照（代码证据）

| 指标 | 现状 | 证据 |
|---|---|---|
| 图元类型 | 20 类（Line…Fcf） | `Element.h` `ElementType`，只增不改 |
| 命令目录 | `CommandCatalog` 约 100 条精确名 + 前缀族（line_/ellipse_/polyline_ 等） | `CommandCatalog.cpp` |
| 别名 | acad.pgp 风格默认表（L/C/PL/H/MA/PU/W…） | `AliasTable.cpp` |
| 约束 | 声明 21 种；隐式相切/垂直/平行/同心 + 显式表 + LM 数值求解 + 尺寸驱动 | `ConstraintSolver.h`、`kConstraintTypeCount=21` |
| 捕捉 | 14 种 OSNAP + 正交/极轴/对象追踪/TK/动态输入 | `Snap.h` |
| 格式 | JSON 全量；DXF R12+R2000（ASCII/二进制）；DWG 自研 R12；R2000+ 经进程外 LibreDWG 桥 | `CadFormats.h`、`DwgBridge.h` |
| 打印 | 页面设置、CTB 简化、多布局/视口、PUBLISH、PDF | `PrintEngine`/`PlotRenderer`/`PublishDialog` |
| 撤销 | 差量变更集 + 每 50 步检查点，可回落全量 | `UndoDelta.h` |
| 工程化骨架 | 自动保存 `.sv$`、崩溃 mini-dump、诊断包、多文档标签、选项/QSettings、i18n、安装包流水线 | `AutoSave`/`Diagnostics`/`DocTab`/`AppSettings`/`build.sh package` |
| 二次开发 | C ABI SDK、JS 脚本、可选 Python/VBA、COM 批量出图、宏/SCR、CUIx | `sdk/`、`ScriptHost`、`SketchCADAutomation` |
| 自测 | 无头 `SketchSelfTest` 分层网（约束/捕捉/DXF/DWG/打印/fuzz/撤销差量等） | `tests/SketchSelfTest.cpp` 末尾注册表；近期设计文档记载约 2300+ CHECK |

国标内建（差异化优势，补差距时不得破坏）：GB/T 14665 线型/线宽组/十二标准层、GB/T 4457.5 剖面、GB/T 131 粗糙度、GB/T 17452 剖切符号、GB/T 4458.4 十二类标注 + GB/ISO 切换、GB/T 14689 图幅标题栏；六套参数化模板（飞机三面图、法兰、轴承座、箱体、法兰轮毂、户型）。

---

## 2. 优缺点

### 2.1 优点

1. **开箱即国标**：主流 CAD 是通用引擎 + 用户自配图层/线型/图框；本产品把机械/航空制图标准做成默认行为，出图合规成本低。  
2. **垂直模板**：三视图投影关系（长对正、高平齐、宽相等）一键生成，这是 AutoCAD 本身不提供的行业产能。  
3. **架构干净**：`SketchModel` 单一真相源；命令行 / 菜单 / 工具 / 脚本收敛到同一 Modify 范式（`Change + applyChanges`）；图元枚举与存档“只增不改”，适合用 AI 持续改代码而不毁档。  
4. **测试密度高**：无头自测 + 黄金样本 + 文件 fuzz + 基准，这是多数商业 CAD 内部才有、开源 CAD 少见的改动护栏。  
5. **交互对标有意识**：OSNAP 十四种、POLAR/OTRACK、空格重复命令、pgp 别名、UCS、夹点热操作、QSELECT/FILTER/编组，不是“自创一套快捷键”。  
6. **稳定性工具链超轻量级产品平均水平**：自动保存、崩溃转储、诊断包、遥测默认关闭、损坏文件校验层 `ElementValidate`。  
7. **二次开发面已铺开**：SDK ABI 协商、COM、脚本、宏，虽生态为零，但接口不是零。

### 2.2 缺点

1. **DWG 不是“原生全版本内核”**：R12 自研子集；R2000+ 读走 `dwg2dxf` 再进自有 DXF 解析，写最高 R2000。保真、速度、代理对象、动态块、注释性、ACIS/自定义实体均弱于 ODA/中望自有内核。转换器未安装则通道关闭。  
2. **渲染仍是 Qt 场景图**：空间索引与增量同步已做，但仍是“一图元一 Item”模型，难与 AutoCAD 显示列表/GPU 批处理在 10–50 万图元上对打。  
3. **约束核是自研 LM + 修复模型**，不是工业草图求解器。门面 `ConstraintSolverFacade` 已预留 SKT 注入，**Phase A 内核尚未接入**。大规模过约束、装配级尺寸联动可靠性未知。  
4. **Windows + 写死 Qt 路径**：换机、CI、Linux/macOS 不是一等公民；COM 仅 Windows。  
5. **商业闭环未完成**：无许可证校验/试用期/节点锁；Authenticode 流水线已写但依赖未配置的证书环境；Inno Setup 商业许可仍待决策。  
6. **生态空**：无 AutoLISP 兼容、无 .NET API、无第三方插件市场、帮助体系弱于商业产品。  
7. **部分能力是“占位或简化”**：块 `annotative` 字段不参与展开；路径阵列不沿切线旋转；SHX/堆叠分数仅检测提示；FIELD 码集远小于 AutoCAD。

---

## 3. 功能差异总表

图例：**相当** ≈ 可完成同类任务；**部分** = 主干有、语义/深度不够；**落后** = 阻塞企业场景；**领先** = 本产品明显强于通用 CAD 默认配置；**—** = 产品边界外。

| 维度 | SketchCAD | AutoCAD 2025 | 中望 CAD | CAXA 电子图板 | 判定 |
|---|---|---|---|---|---|
| 基础二维绘图 | 直线族/圆/弧/椭圆/样条/多段线/构造线/点/徒手线 | 全 | 全 | 全 | 相当 |
| 编辑命令 | move/rotate/scale/mirror/offset/trim/extend/fillet/chamfer/join/array×3/stretch/align/lengthen/pedit/matchprop/purge/overkill | 全 + 关联阵列 + OVERKILL 深度 | 近全 | 机械向强 | 部分：缺关联阵列、路径切线对齐、部分清理深度 |
| 捕捉与追踪 | 14 种 + POLAR/OTRACK/TK/ORTHO | 全 + 临时覆盖菜单更完整 | 近全 | 够用 | 相当～部分 |
| UCS | 命名 UCS、3p/对象/Z 轴、极轴随 UCS | 命名 + 动态 UCS | 有 | 弱 | 部分：无动态 UCS |
| 图层 | ByLayer、冻结/打印/透明度、LAYISO 族、LTSCALE、GB 标准层 | 状态管理器/过滤器/视口覆盖更全 | 近全 | 国标层强 | 部分：缺 LAS/过滤器树/LAYWALK |
| 块 / Xref | 引用式块、属性、动态参数、在位编辑、Xref 附加/分离、设计中心 | 动态块完整、XBIND、覆盖/附着、路径管理器 | 近全 | 图库强 | 部分 |
| 标注 / 文字 | 12 类 + DimStyle 表 + QDIM + MText/STYLE + FIELD 子集 + Table 公式 | 注释性、SHX、完整 FIELD、QLEADER 族 | 近全 | 国标标注强 | 部分：领先在 GB/ISO 切换与 GD&T 框格；落后在 SHX/注释性 |
| GD&T / 粗糙度 / 剖切 | MLeader、Fcf、GB 粗糙度、剖切符号 | 有（需配置） | 部分 | 强 | **领先**（开箱） |
| 填充 | 关联 hatch、PAT、hatchedit、Boundary/Region 分组 | 渐变/实体/孤岛更全 | 近全 | 剖面强 | 部分：无渐变填充 |
| 布局 / 打印 | 多布局、视口比例/冻结、CTB 简化、PUBLISH | 图纸集、STB、戳记、电子传递 | 近全 | 出图向强 | 部分 |
| DXF | R12+R2000 读写，原生块/标注/填充可选 | 全版本 | 全版本 | 常用版本 | 部分：缺 R2004–2018 实体集 |
| DWG | R12 自研；R13–R2018 **读**经 LibreDWG→DXF；**写**最高 R2000 | 原生全版本 | 原生全版本 | 常用版本 | **落后**（企业门槛） |
| 参数化约束 | 21 类 + 尺寸驱动 + 模板 | 参数化草图完整 | 部分 | 参数化图库 | 部分 |
| 脚本 / API | JS/SDK/COM/宏；Python/VBA 可选编译 | LISP/.NET/ARX/COM | LISP/.NET/ZRX | 二次开发包 | 部分：接口有、生态无 |
| 三维 | 无 | 有 | 有 | 无/弱 | — 边界外 |
| 协同 / 云 | 无 | Autodesk 云/图纸集 | 部分 | 部分 | **落后** |
| 安装与许可 | Inno 流水线；无激活 | 完整商业许可 | 完整 | 完整 | **落后** |

---

## 4. 按场景看差距（比功能清单更重要）

| 用户场景 | 本程序能否完成 | 卡点 |
|---|---|---|
| 从零画国标零件图并 PDF/纸质出图 | **能** | 注释性比例、SHX 旧图文字 |
| 用法兰/箱体模板出三视图 | **能** | 模板覆盖行业面仍窄 |
| 打开客户 2013/2018 DWG 继续改 | **勉强** | 依赖转换器；块/标注/填充/动态块/代理对象丢语义 |
| 把本程序图交给客户用 AutoCAD 打开 | **R2000 DXF/DWG 可用** | 默认炸开路径会打散；须开 native 导出并交叉目检 |
| 十万图元总图平移缩放 | **风险** | 索引已做，Item 模型与内存预算仍紧 |
| 改一个尺寸驱动整张草图 | **小图能** | 工业求解器未接入；装配级未验证 |
| 企业 LISP 批量改图 | **不能** | 无 LISP |
| PLM 驱动批量出 PDF | **能（COM）** | 需 Windows 注册；无完整图纸集/戳记流程 |
| 发给外部用户安装即用 | **半能** | SmartScreen 未签名、无许可、GPL 转换器分发义务 |

---

## 5. 正式工程化应用差距（与“功能对标”正交）

功能齐不等于能进现场。工程化清单如下。

### 5.1 已具备（不要再当缺口汇报）

- 版本号单一来源（CMake → version.h / exe 属性 / 安装包名）  
- `build.sh package` → Inno Setup `SketchCAD-<ver>-Setup.exe` + sha256  
- Authenticode **脚本已接入**（`PKG_CERT` / `PKG_CERT_PWD`，无证书则告警跳过）  
- 自动保存、崩溃 dump、诊断包、选项持久化、最近文件、多文档  
- 损坏输入 fuzz、打开失败不污染模型  
- 第三方声明文档（LibreDWG GPLv3 进程外边界）

### 5.2 仍阻塞“正式工程化”

| ID | 差距 | 影响 | 等级 |
|---|---|---|---|
| E1 | **无许可证/激活/试用期**（源码无 license 引擎） | 无法控装机数、无法售卖、无法区分内部版与客户版 | 高 |
| E2 | **代码签名证书未落地** | Windows SmartScreen“未知发布者”，企业软件白名单过不了 | 高 |
| E3 | **DWG 通道依赖可选 GPL 转换器** | 安装包若未带 tools/dwg，现场打不开主流 DWG；带上则须附 GPL 源码获取途径 | 高 |
| E4 | **写出 DWG 仅 R2000** | 客户强制 R2013/R2018 交付时失败 | 高 |
| E5 | **无自动更新/补丁通道** | 现场版本分裂，缺陷无法热修 | 中 |
| E6 | **工具链路径写死** `C:/Qt/6.10.1` | 可重复构建、多开发机、CI 脆弱 | 中 |
| E7 | **仅 Windows 交付** | 部分航空/高校 Linux 工作站无法部署 | 中 |
| E8 | **无电子图纸签署/受控发放** | 设计院 ISO 流程（图号、签字、升版）接不上 | 中 |
| E9 | **PLM/PDM 仅 COM 出图** | 缺检入检出、属性映射、BOM 回写 | 中 |
| E10 | **性能 SLO 未产品化** | 无内存预算 UI、无打开进度取消、大图降级策略不完整 | 中 |
| E11 | **帮助/培训/认证缺失** | 用户手册有，但无命令级 F1 上下文、无认证课 | 低 |
| E12 | **Inno Setup ≥6.5 商业许可决策未关闭** | 合规台账风险（法律上非强制，审计上需结论） | 低 |
| E13 | **Python/VBA 默认关闭** | 对外宣传“脚本宿主”与默认安装包能力不一致 | 低 |
| E14 | **约束 SKT 未接入** | 宣传“参数化”与工业求解器能力不一致，存在交付预期风险 | 中 |

---

## 6. 弥补差异需要做什么（工作分解）

建议按 **P0 交付通道 → P1 保真与产能 → P2 体验与生态** 推进。每项给出工作量量级（人周，单熟练 C++/Qt）。

### P0 — 能发给客户且打得开对方的图

| 包 | 工作 | 人周 | 依赖 |
|---|---|---|---|
| P0-A | 安装包默认附带 dwg2dxf/dwgwrite + GPLv3 文本与源码链接；选项里明示“DWG 转换组件” | 1 | 法务确认分发 |
| P0-B | 采购代码签名证书，CI 注入 `PKG_CERT`，双签 Setup.exe 与 SketchCAD.exe | 0.5 + 采购周期 | 证书 |
| P0-C | 最小许可：机器指纹试用 30 天 + 离线许可文件（不需先做账号云） | 2–3 | 产品策略 |
| P0-D | 与 AutoCAD 交叉打开矩阵：20 张典型机械图（块/标注/填充/中文/xref）读写目检表，作为放行门禁 | 2（含样本收集） | 一台正版 AutoCAD |
| P0-E | DWG **写出 R2013/R2018**：合规路径优先 ODA Drawings SDK；拒绝则继续 LibreDWG 能力边界并在 UI 明确“仅 R2000” | 4–8（ODA）或 1（产品文案） | 授权 |

### P1 — 设计院日常改图不踩坑

| 包 | 工作 | 人周 |
|---|---|---|
| P1-A | DXF/DWG 导入覆盖：MLEADER、TABLE 原生、VIEWPORT、IMAGE、WIPEOUT、代理对象占位框 | 4 |
| P1-B | SHX 形字体只读渲染（常见 gbenor/gbcbig） | 3 |
| P1-C | 注释性比例真正生效（标注/文字/块随视口比例） | 3 |
| P1-D | 关联阵列 + 路径切线对齐 | 2 |
| P1-E | 图层状态（LAS）+ 简单过滤器 | 2 |
| P1-F | 接入 SKT/工业求解器（Facade 已在） | 3–6 |
| P1-G | 渲染：脏矩形 + 填充/样条显示缓存；10 万图元打开有进度/可取消/OOM 降级 | 3 |
| P1-H | 静默安装、企业 MST、更新通道（Sparkle 类或自建版本清单） | 2 |

### P2 — 生态与体验

| 包 | 工作 | 人周 |
|---|---|---|
| P2-A | AutoLISP 子集（命令调用 + 选择集 + 简单 entget）或明确“不兼容 LISP、提供 JS 迁移指南” | 6 或 0.5（文档） |
| P2-B | 图纸集/电子传递、打印戳记完善 | 2 |
| P2-C | 动态 UCS、夹点菜单与 Shift 临时捕捉覆盖补齐 | 2 |
| P2-D | 构建路径环境变量化、可选 Linux 无头测试 | 2 |
| P2-E | 行业模板扩展（紧固件、焊接符号、管路） | 按模板 1–2 |

---

## 7. 用 AI 编程弥补差异：提示词

使用约定（与本仓库已有任务提示词同构，可直接贴进新对话）：

- 验收基线：`build.sh test` EXIT=0；双目标 `-Wall -Wextra` 零警告。  
- 铁律：`ElementType` / 存档字段只增不改；撤销走 `Change + applyChanges`；不引入 GPL 链入主程序（LibreDWG 仅进程外）。  
- 改完必须在 `SketchSelfTest` 增断言，并更新对应 `docs/*-design-analysis.md`。

### 提示词 1 — DWG 转换器随包与失败可诊断（P0-A）

```text
【目标】让正式安装包默认具备 DWG R13–R2018 读取与 R2000 写出能力，转换器缺失时给出可执行的安装指引而不是静默失败。
【范围】1) build.sh package：把 thirdparty 构建出的 dwg2dxf/dwgwrite 拷入 STAGE/tools/dwg/；
        2) 安装脚本同步该目录；第三方声明页写入 GPLv3 与源码获取 URL；
        3) dwgbridge::available()==false 时，打开/另存 DWG 对话框展示版本串原文 + 「未安装 DWG 转换组件」+ 帮助链接；
        4) 选项页增加「DWG 转换组件路径」覆盖 SKETCHCAD_DWGTOOLS。
【约束】主程序零链接 libdwg；不把 GPL 源码编译进 SketchCAD 目标；自研 AC1009 路径不动。
【验收】无转换器环境：打开 AC1015 文件失败报文含 AC1015 且不崩溃；有转换器：t_dwg 全绿；
        安装包目录树含 tools/dwg 与 COPYING；build.sh test 与 package EXIT=0；零警告。
```

### 提示词 2 — 离线许可与试用（E1 / P0-C）

```text
【目标】为 SketchCAD 增加最小可用的商业许可：30 天试用 + 离线许可文件，过期后只读打开、禁止保存/打印/导出。
【范围】新建 src/core/Licensing.{h,cpp}：机器指纹（主板 UUID+磁盘，允许缺失字段）、许可文件 JSON
        （签发者、过期、功能位、Ed25519 签名）；MainWindow 启动校验；过期 UI 横幅；命令行 --license 导入。
        公钥编译进程序，私钥不进仓库。自测注入固定时钟与测试密钥。
【约束】不联网；失败默认保守（无许可=试用起算，时钟回拨检测记录 lastSeen）；不加密图纸文件本身；
        核心几何代码零耦合，只在保存/打印/导出/COM 出图四处设闸。
【验收】无许可首次启动写入试用起点；过期后保存返回明确错误；合法许可文件可保存；篡改签名拒绝；
        自测 ≥12 断言；零警告；EXIT=0。
```

### 提示词 3 — 代码签名落地检查清单（E2 / P0-B）

```text
【目标】在已有 Authenticode 流水线（build.sh package + PKG_CERT/PKG_CERT_PWD）上做“可发布”验收，
        而不是再改签名算法。
【范围】1) 文档化证书申请（OV/EV）、时间戳 URL 故障切换；2) 无证书/错误口令/成功签名三种 CI 日志黄金样例；
        3) 增加自测或脚本断言：未签名包 checksums 仍生成但日志含「产物未签名」；
        4) 不要把证书提交进 git。
【约束】不削弱现有“签名失败不得假装成功”裁决矩阵（见 docs/authenticode-signing-design-analysis.md）。
【验收】有证书环境 signtool verify /pa 对 Setup.exe 与安装后 SketchCAD.exe 均通过；无证书环境 EXIT=0 且告警可见。
```

### 提示词 4 — DWG/DXF 交叉保真门禁（P0-D）

```text
【目标】建立 20 张黄金图纸的 AutoCAD 交叉打开门禁，把“能读”升级为“可编辑语义保留率可度量”。
【范围】tests/golden/ 增加机械零件/装配/带中文/带块属性/带填充/带标注/带 xref 相对路径样本；
        自测输出保真报告：图元计数、块表名、标注数、填充环数、中文往返；
        对无法自动比的项生成 PDF 叠图脚本说明供人工。
【约束】不把客户涉密 DWG 提交进仓库；样本自绘或使用已授权测试图；R12 回归不得破坏。
【验收】报告字段稳定可 diff；至少 8 张全自动 round-trip 几何误差 ≤1e-6 mm；其余列入人工清单；
        t_formatsGolden 扩展断言 ≥15；EXIT=0。
```

### 提示词 5 — 注释性比例（P1-C）

```text
【目标】让标注、文字、属性块按布局视口比例缩放，对标 AutoCAD annotative 的最小可用子集。
【范围】消费 BlockDef.annotative（现为占位）；DimStyle/TextStyle 增加 annotative 布尔（只增字段）；
        PlotRenderer/GraphicsView 在图纸空间按 viewport scale 计算字高与箭头；
        模型空间保持 1:1 图纸坐标现语义。旧档缺键 = false。
【约束】ElementType 不动；模型空间打印回归（PlotStyleSamples）像素差分为 0 或有基线更新说明；
        非注释性图元零行为变化。
【验收】同一标注在 1:1 与 1:2 视口字高比为 2；旧档打开字高不变；自测 ≥10 断言；零警告。
```

### 提示词 6 — 关联阵列与路径切线（P1-D）

```text
【目标】阵列从“一次性复制”升级为可编辑关联阵列；路径阵列项目可沿切线旋转（对齐路径）。
【范围】新增阵列定义表（随图存档，对标 blockTable：源 id 列表、阵列类型、参数）；
        改源几何或参数后重生成实例；explode 炸开为独立图元并删除定义；
        arraypath 增加 align=tangent|none（默认 none 保持旧行为）。
【约束】现有 arrayrect/polar/path 无定义的旧结果保持普通副本；撤销单步；ElementType 只增不改
        （可用 BlockRef 或末尾新类型，若新类型必须走八步清单）。
【验收】改源圆半径 → 全部实例半径同步；tangent 模式下切向角误差 ≤0.1°；旧命令路径测试全绿；
        新增 ≥16 断言。
```

### 提示词 7 — SKT 求解器接入（P1-F / E14）

```text
【目标】在 ConstraintSolverFacade 已有接缝上注入 avcad SketchEngine（SKT），实现改标注值全图联动，
        失败自动回落 LegacyConstraintSolver。
【范围】1) 以静态库编译 SKT 核（MinGW 兼容），禁止 UI 层 include 其头；
        2) SktConstraintSolver : IConstraintSolver，21 种 ConstraintType 1:1 映射（见 IConstraintSolver.h 注释表）；
        3) Facade::setActiveSolver 在 MainWindow 启动时按 settings solver/constraintEnabled 注入；
        4) 对照基线：同一组草图 Legacy vs SKT 几何差 ≤ 容差或 SKT 失败回落。
【约束】存档 ConstraintRecord 不动；禁止求解器魔数，容差走 geo::almostEqual；GPL/许可按企业内部码处理。
【验收】t_sktIntegration 扩展：欠/过约束诊断、尺寸驱动半径/长度、开关关闭≡Legacy；
        既有 21 类约束测试全绿；零警告；EXIT=0。
```

### 提示词 8 — 大图打开进度与 OOM 降级（P1-G / E10）

```text
【目标】5 万图元保持交互；10 万图元允许变慢但禁止 bad_alloc 崩溃；打开过程可取消。
【范围】CadFormats 导入预留 vector；GraphicsView 分块创建 Item（已有异步分块则补齐取消令牌）；
        捕获 std::bad_alloc 转为用户对话框；超过内存预算时关闭填充图案/降低 LOD；
        SketchBenchmark 增加峰值 RSS 列与 OOM 用例（不得 abort）。
【约束】空间索引语义不变（不漏选）；小图（<5000）路径零回归（无进度对话框闪烁）。
【验收】基准 1万/5万指标不差于现文档；10 万或内存失败返回错误码而不是 terminate；
        自测模拟 new 失败钩子 ≥4 断言。
```

### 提示词 9 — 图层状态管理器 LAS（P1-E）

```text
【目标】保存/恢复图层可见、冻结、锁定、颜色、线型快照，支持装配图快速切换显示方案。
【范围】SketchModel 增 LayerState 表（名 → 每层属性拷贝），随 JSON 存档；
        命令 layerstate save/restore/delete/list；面板入口；恢复走单步撤销。
【约束】Layer 结构只增不改；与 LAYISO 隔离栈互不破坏（restore 后 layuniso 语义文档化）；
        旧档无 key 视为空表。
【验收】改层后 restore 逐字段回到快照；与 layiso 组合用例；≥10 断言；EXIT=0。
```

### 提示词 10 — SHX 只读渲染（P1-B）

```text
【目标】打开使用 gbenor.shx / gbcbig.shx 的外来 DXF 时文字可见、可打印，不要求可编辑形定义。
【范围】fonts/ 放置官方允许分发的 SHX 或用户目录配置；解析 SHX 形并栅格/路径化；
        TEXT/MTEXT 若字体名为 SHX 则走形字体，否则 TTF；缺字体时用固定宽回落并日志一次。
【约束】不破解商业 SHX；不把字体二进制许可不清的文件提交仓库（用 README 说明用户自备路径）；
        TextMetrics 三同源（屏/打/取景）对 SHX 路径同样走单一 layout。
【验收】样张中文 SHX 与 AutoCAD 打印叠图可辨；缺字体不崩；自测用夹具形字体 ≥8 断言。
```

### 提示词 11 — 构建可移植（E6 / P2-D）

```text
【目标】去掉 CMakeLists.txt 对 C:/Qt/6.10.1 与 mingw1310 的硬编码，改为 CMAKE_PREFIX_PATH / 环境变量。
【范围】toolchain 文件或文档化 env：QTDIR、MINGW；build.sh 探测；CI 示例；README「已知限制」删除写死路径条。
【约束】本机默认路径仍可作为 fallback，不打断现有开发者；不改 C++ 逻辑。
【验收】在未改源码仅设环境变量的另一前缀下配置成功；现有 build.sh test 仍 EXIT=0。
```

### 提示词 12 — 明确二次开发边界（P2-A 文档优先）

```text
【目标】避免销售/README 暗示“兼容 AutoLISP”。写清：支持 JS/SDK/COM/宏；不支持 LISP/.NET；给 10 条常见 LISP 任务的 JS 对照。
【范围】docs/SDK_GUIDE.md 与 README 二次开发章节；CommandCatalog help 增加 script 示例。
【约束】不实现 LISP 解释器（本提示词只做预期管理）。若后续要做 LISP，另开任务。
【验收】文档中不再出现“兼容 AutoLISP”或等价表述；示例脚本可在 ScriptHost 跑通 3 个（选中、改层、导出 DXF）。
```

---

## 8. 建议的 90 天路线

| 阶段 | 周 | 产出 | 完成定义 |
|---|---|---|---|
| 放行 | 1–3 | 签名证书 + 许可 MVP + 转换器随包 + 20 图交叉表 | 外部电脑 SmartScreen 可过或仅轻度提示；主流 DWG 能打开并列出块/标注 |
| 保真 | 4–8 | 注释性、SHX、导入实体补齐、关联阵列 | 设计院样张“能改尺寸、能出 PDF”内部验收通过 |
| 求解与性能 | 9–12 | SKT 接入或明确不上并改文案；10 万图元不崩 | 基准入 CI；产品说明书与真实能力一致 |

**不要做的事**：在 DWG 保真与许可未闭环前，投入 LISP、云协同、三维；那会稀释工程化放行。

---

## 9. 对过期文档的处理建议

以下文档结论勿再引用为现状，可在文首加过期横幅并指向本报告：

- `docs/SketchCAD-主流CAD差异分析报告.md`（仍写无块/无打印/DXF 仅 R12）  
- `docs/二维CAD差距分析与AI编程实施方案.md`（打印/脚本/捕捉等基线已变）  
- `docs/commercialization-audit-gaps-and-prompts.md`（多文档/QSettings/自动保存等已落地）  
- `docs/cad-vs-autocad-gap-analysis-2026-10.md`（D2–D7 多项已实现，仅部分仍有效）

---

*报告结束。可视化摘要见 Cursor Canvas「SketchCAD 差距看板」。*
