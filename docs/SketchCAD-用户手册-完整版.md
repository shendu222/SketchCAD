# SketchCAD 用户手册（源码基线完整版）

> 适用版本：SketchCAD v1.1.1 源码基线  
> 核对日期：2026-11-23  
> 平台：Windows 10 / 11（x64）  
> 技术栈：C++17 / Qt 6 / CMake / Ninja  
> 手册定位：既是用户操作指南，也是后期修改与功能扩展的依据。  
> 事实口径：本手册以 `src/` 中的实际注册、菜单、工具与保存/打开实现为准。若旧版 `README.md`、`docs/用户手册.md` 或差距分析报告与本文冲突，以本文“源码现状”为准。

---

## 状态标记约定

| 标记 | 含义 |
|---|---|
| 【已有】 | 源码中存在明确实现、命令注册或界面入口，可直接使用。 |
| 【已有·受限】 | 有入口或部分实现，但能力、版本、精度或场景有限，详见“限制”。 |
| 【工具模式】 | 存在于 GUI 工具注册表；鼠标交互可用，但不一定有等价命令行命令。 |
| 【待扩展】 | 主流二维 CAD 通常具备，但本程序尚未实现或未形成完整能力；仅作为迭代依据。 |

本手册结构参考 AutoCAD、中望 CAD、浩辰 CAD 等主流二维 CAD 的常见功能组织方式，按“文件与文档 → 绘图 → 修改 → 选择 → 视图 → 图层与样式 → 标注与文字 → 约束 → 块与参照 → 布局与打印 → 查询与检查 → 自动化与扩展”展开。

---

## 目录

1. [产品概览](#1-产品概览)
2. [快速开始](#2-快速开始)
3. [术语与输入约定](#3-术语与输入约定)
4. [界面参考](#4-界面参考)
5. [基础交互](#5-基础交互)
6. [文件与文档管理](#6-文件与文档管理)
7. [绘图功能](#7-绘图功能)
8. [编辑与修改](#8-编辑与修改)
9. [选择与过滤](#9-选择与过滤)
10. [视图与坐标](#10-视图与坐标)
11. [图层、颜色、线型与线宽](#11-图层颜色线型与线宽)
12. [尺寸标注、文字与符号](#12-尺寸标注文字与符号)
13. [约束与几何辅助](#13-约束与几何辅助)
14. [块、属性、外部参照与数据库](#14-块属性外部参照与数据库)
15. [布局、打印与数据交换](#15-布局打印与数据交换)
16. [查询、标准与合规检查](#16-查询标准与合规检查)
17. [自动化与二次开发](#17-自动化与二次开发)
18. [设置、诊断与已知问题](#18-设置诊断与已知问题)
19. [命令、工具与快捷键参考](#19-命令工具与快捷键参考)
20. [待扩展功能与迭代路线](#20-待扩展功能与迭代路线)
21. [开发迭代参考](#21-开发迭代参考)
22. [附录](#22-附录)

---

## 1. 产品概览

### 1.1 产品定位 【已有】

SketchCAD 是一款基于 Qt Widgets 的轻量级二维工程 CAD 草图工具，面向机械、航空、建筑装修等制图场景。其特点如下：

- 经典 CAD 交互：菜单、工具栏、参数输入条、命令行、状态栏并存。
- 国标优先：线宽、线型、图框、尺寸注法、剖面符号、粗糙度、焊缝符号、标题栏等内置 GB/T 规则，并可切换到 ISO 标准。
- 命令生态：`CommandCatalog` 统一收录 222 条内置命令（150 个精确命令 + 72 个前缀族命令），另有 119 个 GUI 工具模式；`help`、AI 助手提示词、Tab 补全与命令面板共用该目录。
- 数据交换：原生 JSON 完整往返；DXF 支持 R12 至 R2018；DWG 写出最高 R2000；STEP 只写不读。
- 扩展入口：宏、SCR 脚本、JavaScript 宿主、命令别名、CUIx 界面配置、SDK 插件、COM 自动化、AI 助手调用链。

### 1.2 核心数据模型 【已有】

统一图元模型 `Element` 当前包含 20 类：

`Line`、`Circle`、`Arc`、`Rectangle`、`Polygon`、`Spline`、`Constraint`、`Dimension`、`Ellipse`、`Polyline`、`Text`、`Hatch`、`BlockRef`、`Xline`、`Boundary`、`MText`、`Table`、`Point`、`MLeader`、`Fcf`。

要点：

- 图元通过自增 `id` 标识；命令行多数编辑命令以 `#id` 或直接 `id` 指定对象。
- 选择集支持单选与多选，窗口选择、相交选择、栏选、圈围、圈交、按图层选择、快速选择等均可用。
- 撤销/重做基于编辑快照；公开修改命令通常包在单步 `beginEdit` 中。
- 旧手册中“12 类图元”的描述已过期，源码现状为 20 类。

### 1.3 能力边界 【已有·受限】

- 程序是二维 CAD，不提供三维建模；三维视图方向仅用于二维工作平面切换。
- DWG 写出最高 R2000；若客户要求 R2004/R2013/R2018，应导出 DXF 高版本。
- STEP 只写不读。
- 约束求解为自研轻量求解器，尚非工业级参数化内核。
- 渲染基于 Qt 场景图，超大规模图纸性能需实测，不宜默认按十万图元级商用 CAD 预期使用。

---

## 2. 快速开始

### 2.1 安装与启动 【已有】

提供两类 Windows 安装包：

| 安装包 | 用途 |
|---|---|
| `SketchCAD-<版本>-Setup.exe` | Inno Setup 安装，适合普通用户。 |
| `SketchCAD-<版本>-Setup.msi` | Windows Installer，适合静部署与组策略分发。 |

静默安装示例：

```cmd
SketchCAD-1.1.1-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
msiexec /i SketchCAD-1.1.1-Setup.msi /qn
```

开发环境可直接运行：

```bash
./build.sh
./build/SketchCAD.exe
```

或使用 CMake：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SketchCAD.exe
```

程序不解析操作系统启动参数；本文中的“命令行”均指主窗口底部的内部命令输入框。

### 2.2 第一次绘制一条直线 【已有】

**鼠标方式**

1. 启动程序，确认状态栏显示“就绪”。
2. 点击工具栏“草图工具”中的“直线”，或按 `L`，或选择“绘图 → 直线”。
3. 在画布中单击起点，再单击终点。
4. 按 `Esc` 或右键结束当前工具。

**命令行方式**

1. 点击底部命令行输入框。
2. 输入：`line 0,0 100,50`
3. 按 `Enter`。
4. 输入 `zoom` 或在画布滚轮缩放查看结果。

### 2.3 第一次保存与导出 【已有】

1. 选择“文件 → 保存”或按 `Ctrl+S`。
2. 选择保存类型：
   - `*.json`：原生格式，信息最完整。
   - `*.dxf`：交换格式，可选择 R12 至 R2018。
   - `*.dwg`：写出最高 R2000。
   - `*.step`：只写，文字不会导出。
3. 若选择 DXF，会在保存对话框中弹出版本选择。
4. 对外交付时建议优先选择 DXF R2004–R2018，而不是 DWG。

### 2.4 获取帮助 【已有】

- 按 `F1` 或输入 `help` 打开命令总表。
- 输入 `help line` 查看 `line` 的用法、参数与示例。
- 命令行支持 `↑` / `↓` 历史、`Tab` 补全、空回车重复上一命令。

---

## 3. 术语与输入约定

### 3.1 统一术语

| 术语 | 含义 |
|---|---|
| 图元 | 图中最小可编辑对象，如直线、圆、文字、标注、块引用。 |
| 选择集 | 当前被选中的一个或多个图元。 |
| 当前图层 | 新建图元默认归属的图层。 |
| ByLayer | 颜色/线型/线宽随图层属性，而非对象显式属性。 |
| 工具模式 | 通过菜单或工具栏激活的鼠标交互状态，如“两点圆”“正交折线”。 |
| 命令 | 在内部命令行、脚本或宏中输入的文本指令。 |
| 夹点 | 图元上可直接拖拽编辑的控制点。 |
| 块 | 可重复插入的图元定义；插入后形成块引用。 |
| 属性 | 绑定到块定义/引用的文本数据字段。 |
| 外部参照 | 以路径引用外部图纸内容，不复制为本地图元。 |
| 布局 | 图纸空间环境，可包含图框与视口。 |
| 视口 | 布局中观察模型空间的矩形窗口，可设置比例与冻结层。 |
| 极轴追踪 | 按设定增量角显示对齐引导线。 |
| 对象捕捉追踪 | 基于已有对象捕捉点生成水平/竖直/角度对齐线。 |
| 域 | 可自动更新的文本字段，如面积、自定义图纸特性。 |

### 3.2 点坐标格式 【已有】

命令行支持以下点格式：

| 格式 | 示例 | 说明 |
|---|---|---|
| 逗号分隔 | `50,30` | 绝对二维点。 |
| 空格分隔 | `50 30` | 绝对二维点。 |
| 三维写法 | `50,30,0` | Z 通常忽略，仅保留二维语义。 |
| 相对直角坐标 | `@20,10` | 相对上一点偏移。 |
| 极坐标 | `100<45` | 距离 100，角度 45°。 |
| 相对极坐标 | `@100<45` | 相对上一点按距离与角度定位。 |

### 3.3 命令语法 【已有】

```text
命令名 [参数...]
```

示例：

```text
line 0,0 50,0
circle 50,50 25
dim aligned 0,0 100,0
qselect type=直线 layer=0
```

解析顺序：

1. 去除首尾空白并按空白分词。
2. 展开命令别名。
3. 精确命令匹配。
4. 前缀命令族匹配，如 `line_*`、`arc_*`、`spline_*`。
5. 插件外部命令匹配。
6. 失败时给出最多 3 个相近命令建议。

### 3.4 命令与工具的关系 【已有·受限】

- 部分功能同时有命令行命令和 GUI 工具，例如 `line`、`circle`、`trim`。
- 部分功能主要是 GUI 工具模式，例如 `circle_cr`、`dim_h`、`fence`、`wp`、`cp`；它们存在于工具注册表，命令行不一定提供同名精确命令。
- 少数命令存在于注册表但未写入命令目录，例如 `sheetframe`、`weld`、`macroshow`、`tbnew` 等；可用 `help` 总表未必列出，操作以本文和源码为准。
- 默认别名 `ex` 指向 `extend`，但命令注册表未发现同名精确命令；命令行输入 `ex` 可能提示未知命令，延伸功能建议使用 GUI 工具入口。

---

## 4. 界面参考

### 4.1 主窗口布局 【已有】

```text
┌──────────────────────────────────────────────┐
│ 菜单栏                                       │
├──────────────────────────────────────────────┤
│ 工具条①：草图工具                             │
│ 工具条②：样式与视图                           │
├──────────┬───────────────────────────┬────────┤
│ 左侧面板 │ 绘图区 GraphicsView        │ 右侧面板│
├──────────┴───────────────────────────┴────────┤
│ 参数输入条                                   │
│ 状态栏 + 命令行                               │
└──────────────────────────────────────────────┘
```

### 4.2 菜单顺序 【已有】

顶级菜单实际顺序为：

`文件 → 编辑 → 视图 → 绘图 → 图纸 → 测量查询 → 脚本 → 界面 → 插件 → 扩展工具 → 工具 → 打印 → 追踪 → 帮助`

常用子菜单：

- 文件：新建、打开、保存、自动保存设置、最近文件、导出 DXF、退出。
- 编辑：撤销、重做、剪贴板、修改、选择。
- 修改：移动/复制、复制、阵列、连合、剖面填充、对齐、旋转、缩放、镜像、反向、修剪、延伸、偏移、倒角/圆角、打断/等分、拉伸、属性、分解、删除。
- 选择：全选、清空、反选、窗口选择、轮廓、相交、图层、快速选择、过滤器。
- 绘图：基本图元、圆弧、多边形、样条、椭圆、折线、标注、约束、文字、表格、模板。
- 图纸：标题栏、图框、幅面、比例、图层/样式入口。
- 追踪：正交、栅格、捕捉、极轴、对象捕捉追踪、动态输入、临时追踪点。
- 测量查询：距离、面积、面域特性、点坐标、列表、快速选择。
- 脚本：宏录制/停止/保存/回放、SCR、JavaScript 示例。
- 界面：面板显隐、主题、语言、命令面板、工作空间。
- 插件：插件加载与自动加载配置。
- 扩展工具：设计中心、标准知识库、合规检查等。
- 打印：打印设置、打印预览、发布、PDF、直接打印。
- 帮助：命令列表、关于。

### 4.3 工具栏 【已有】

**草图工具**：选择、直线、构造线、矩形、多边形、圆、圆弧、椭圆、折线、边界、样条、样条编辑、标注、约束、多行文字、表格、修改下拉。

**样式与视图**：删除、线宽、线型、标准 GB/T 4458 或 ISO 129、颜色、连续绘制、栅格、放大、缩小、适配、窗口缩放、测量查询、打印、XY/YZ/XZ 平面。

### 4.4 参数输入条 【已有】

字段包括：

- 角度
- 长度
- 间隔
- 条数
- 线型
- 提示文本

适用场景：在鼠标拾取时精确指定半径、角度、阵列间距、等分段数等参数，避免完全依赖屏幕坐标。

### 4.5 停靠面板 【已有】

| 面板 | 用途 |
|---|---|
| BOM 树 | 查看序号球标与明细表关联。 |
| 图元面板 | 列出当前图图元。 |
| 属性面板 | 编辑选中图元的图层、颜色、线型、线宽、几何参数。 |
| 执行信息 | 查看命令日志与错误提示。 |
| 命令行 | 输入文本命令。 |
| 图层面板 | 新建/删除/重命名图层，颜色、线型、线宽、显隐、锁定、置为当前。 |
| 约束栏 | 查看/管理几何与尺寸约束。 |
| 合规检查 | 显示 `stdcheck` 诊断结果。 |
| 设计中心 | 浏览块、标准与可复用内容。 |
| AI 助手 | 自然语言入口，可调用内置命令与合规检查。 |

### 4.6 状态栏 【已有】

状态栏显示：就绪状态、选项、X/Y/Z 坐标、平面、UCS、图元数、比例、场景项、自动保存状态、正交、极轴、捕捉、追踪、线宽、尺寸驱动、数据库状态等。

数据库状态通常为：

- `数据库: 已连接`
- `数据库: 未连接（使用文件）`

后者不影响绘图、编辑、保存等核心功能。

---

## 5. 基础交互

### 5.1 鼠标操作 【已有】

| 操作 | 行为 |
|---|---|
| 左键单击 | 选择图元、拾取点、确认工具当前步骤。 |
| 左键拖拽空白区 | 窗口选择或窗口取消，取决于工具状态。 |
| 右键单击 | 结束/取消当前工具；空闲状态下可重复上一命令或弹出上下文菜单。 |
| 右键拖拽 | 平移视图。 |
| 中键拖拽 | 平移视图。 |
| 空格 + 左键拖拽 | 平移视图。 |
| 滚轮 | 以光标位置为锚点缩放，范围约 0.05–500。 |
| Shift + 右键 | 绘制期间临时对象捕捉覆盖菜单。 |
| Ctrl + 右键 | 正交/极轴/追踪/捕捉快捷开关。 |
| 双击 | 编辑文字、表格、标注、引线、块在位编辑，或打开属性面板。 |

### 5.2 键盘操作 【已有】

| 按键 | 行为 |
|---|---|
| `Esc` | 取消当前绘制、选择、缩放框、夹点热态或点拾取。 |
| `Enter` | 确认输入、结束多段线/样条/栏选等连续采集。 |
| `Delete` / `Backspace` | 删除选中图元。 |
| 空格 | 命令行空输入时重复上一命令；画布中用于临时平移。 |
| `↑` / `↓` | 命令行历史。 |
| `Tab` | 命令补全。 |
| `F1` | 帮助/命令列表。 |
| `F5` | 重生成。 |
| `F7` | 栅格显示。 |
| `F8` | 正交。 |
| `F9` | 栅格吸附。 |
| `F10` | 极轴追踪。 |
| `F11` | 对象捕捉追踪。 |
| `F12` | 动态输入。 |

### 5.3 取消与结束规则 【已有】

- 绘制过程中：`Esc` 取消当前操作并输出“已取消当前操作”。
- 右键单击：通常等价于结束或取消当前操作。
- 连续线/样条/栏选：`Enter` 结束并生成结果，`Esc` 放弃。
- 夹点热态：`Esc` 退出夹点编辑。
- 命令行空输入：重复上一次成功命令。

### 5.4 捕捉与追踪 【已有】

- 对象捕捉包括端点、中点、圆心、节点、象限点、交点、垂足、切点等类型。
- 正交模式将输入约束为水平或竖直。
- 栅格吸附按设定步长对齐。
- 极轴追踪按增量角显示引导线。
- 对象捕捉追踪基于已捕捉点生成对齐线。
- `tk` 放置临时追踪点，`tkclear` 清除全部临时追踪点。
- Shift + 右键提供临时捕捉覆盖，但完整主流 CAD 级覆盖菜单仍属待扩展范围。

---

## 6. 文件与文档管理

### 6.1 功能总表 【已有】

| 功能 | 操作方式 | 适用场景 | 命令/快捷键 | 状态 |
|---|---|---|---|---|
| 新建 | 文件 → 新建 | 开始空白草图 | `Ctrl+N` | 【已有】；当前实现为空白新建，不弹图框向导。 |
| 打开 | 文件 → 打开 | 读取 JSON/DXF/DWG | `Ctrl+O` | 【已有】 |
| 保存 | 文件 → 保存 | 按扩展名保存 | `Ctrl+S` | 【已有】 |
| 另存为 | 文件菜单 | 更换格式或路径 | — | 【已有】 |
| 自动保存 | 文件 → 自动保存设置 | 防止意外退出丢失 | — | 【已有】 |
| 最近文件 | 文件菜单 | 快速回到上次图纸 | — | 【已有】 |
| 导出 DXF | 文件 → 导出 DXF | 与其他 CAD 交换 | `Ctrl+E`、`dxfout` | 【已有】 |
| 多文档标签 | 文档区标签 | 同时打开多张图 | — | 【已有】 |
| 打印设置 | 打印 → 打印设置 | 配置纸张/比例 | — | 【已有】 |
| 打印预览 | 打印 → 打印预览 | 出图前检查 | — | 【已有】 |
| 导出 PDF | 打印 → 导出 PDF | 交付 PDF | — | 【已有】 |
| 发布 | 打印 → 发布 | 批量/多页输出 | — | 【已有·受限】 |
| 直接打印 | 打印 → 打印 | 连接打印机输出 | — | 【已有】 |
| 退出 | 文件 → 退出 | 关闭程序 | `Ctrl+Q` | 【已有】 |

### 6.2 文件格式 【已有·受限】

| 格式 | 读 | 写 | 说明 |
|---|---|---|---|
| JSON | 支持 | 支持 | 原生格式，完整往返，UTF-8。 |
| DXF | 支持 | 支持 | R12、R2000、R2004、R2007、R2010、R2013、R2018；GUI 保存可选择版本。 |
| DWG | 支持 | 支持 | 原生 R12 二进制写出；R2000 经外部转换器桥接；R2004+ 不支持写出。 |
| STEP | 不支持 | 支持 | AP214 二维曲线集；文字不会导出。 |

### 6.3 自动保存与恢复 【已有】

- 自动保存目录位于 `%LOCALAPPDATA%\SketchCAD`。
- 异常退出后启动流程会检测会话标记并提示恢复。
- 自动保存不改变用户主动保存的文件路径。

### 6.4 多文档 【已有】

- 每个文档有独立标签，未保存文档以 `*` 标识。
- 切换标签保留各自图元、选择集与视图状态。
- 保存成功后标签脏标记清除。

---

## 7. 绘图功能

### 7.1 基础图元 【已有】

| 图元 | 主要操作 | 命令/工具 | 适用场景 |
|---|---|---|---|
| 直线 | 两点；水平/竖直/角度/平行/平分/切线/正交折线 | `line`、`line_h`、`line_v`、`line_angle`、GUI 直线族 | 轮廓、辅助线、机械边线。 |
| 构造线 | 过点+角度、两点、水平、竖直、平行、平分 | `xline`、`xline_*` | 作图基准、中心线。 |
| 圆 | 圆心半径、三点、两点、相切、内切、同心、由弧转圆 | `circle`、`circle3p`、GUI `circle_*` | 孔、轴、定位圆。 |
| 圆弧 | 圆心半径起止角、三点、相切、椭圆弧 | `arc`、GUI `arc_*` | 过渡圆角、凸轮轮廓。 |
| 矩形 | 两角点、倒角、圆角 | `rect`、`rect_chamfer`、`rect_fillet` | 箱体、板件、图框。 |
| 多边形 | 自由顶点、正多边形内接/外切/边长、星形 | `poly`、`poly_regular`、`poly_circumscribed`、`poly_edge`、`poly_star` | 螺母、法兰、装饰轮廓。 |
| 椭圆 | 圆心+长/短半轴+旋转角；另有 GUI 一点、轴、焦点、四点、三点、内切 | `ellipse`、GUI `ellipse_*` | 投影视图、斜孔、AI 助手“绘制一个椭圆”。 |
| 样条 | 过点、抛物线、双曲线、手绘、折线转样条 | `spline`、`spline_*` | 自由曲线、流体/航空轮廓。 |
| 折线 | 连续线段、弧段、节点编辑 | `polyline`、`polyline_*` | 复杂轮廓、路径。 |
| 点 | 绘制离散点 | `point` | 节点捕捉、等分标记、测量点。 |
| 填充 | 关联剖面填充、参数调整 | `hatch`、`bhatch`、`hatchedit` | 剖视图、材料区域。 |
| 边界 | 从闭合区域生成边界 | GUI `boundary`、`bpoly` | 由复杂图形提取轮廓。 |

### 7.2 直线族详细操作 【已有】

| 子模式 | 操作要点 | 适用场景 |
|---|---|---|
| `line` | 指定起点和终点，可连续输入。 | 通用轮廓。 |
| `line_h` | 指定一点与长度，生成水平线。 | 建筑墙线、中心线。 |
| `line_v` | 指定一点与长度，生成竖直线。 | 机械对称线。 |
| `line_angle` | 指定点、角度、长度。 | 斜线、放射线。 |
| `line_parallel` / `line_parallel_pt` | 参考已有线作平行线。 | 等距轮廓。 |
| `line_bisector` | 由两线生成角平分线。 | 对称结构。 |
| `line_tangent_pc` / `line_tangent_cc` | 点圆切线、圆圆公切线。 | 带传动、凸轮。 |
| `line_snake*` | 正交折返线，可指定水平或竖直起始。 | PCB 走线、管路。 |
| `line_angle_from_line` | 以参考线为基准按角度生成。 | 相对角度标注前作图。 |
| `line_from_point_to_line` | 点到直线作垂线。 | 垂直投影。 |

### 7.3 圆与圆弧族详细操作 【已有·工具模式】

GUI 工具提供：`circle_2p`（两点圆）、`circle_2pr`（两点+半径）、`circle_cr`（圆心+半径）、`circle_t2e1p`（相切两图元+点）、`circle_t1e2p`（相切一图元+两点）、`circle_t2er`（相切两图元+半径）、`circle_t3e`（相切三图元）、`circle_inscribed`（内切圆）、`circle_concentric`（同心圆）、`circle_byarc`（由弧转圆）。

圆弧 GUI 工具包括：三点弧、圆心两点弧、切线点弧、弧长弧、椭圆弧等。

适用场景：齿轮过渡、圆角连接、孔系布局、凸轮与叶片轮廓。

限制：多数 `circle_*`、`arc_*` 子模式通过工具注册表激活，命令行不一定有同名命令；自动化脚本应优先验证命令注册表是否支持。

### 7.4 文字、表格与符号 【已有】

| 功能 | 命令/工具 | 操作方式 | 适用场景 |
|---|---|---|---|
| 单行文字 | `text`、`dtext` | 指定插入点、字高、内容。 | 简短说明、图签字段。 |
| 多行文字 | `mtext`、GUI `table` 相关入口 | 指定插入点与字高后输入内容。 | 技术要求、说明段。 |
| 编辑文字 | `ddedit`、双击文字 | 选择文字后修改文本。 | 修改说明、标注文字。 |
| 表格 | GUI `table` | 打开表格编辑器。 | 明细表、参数表。 |
| 序号球标 | `xh` | 选中对象或给定 id，自动生成圆+编号并写入 BOM。 | 装配图、零件序号。 |
| 焊缝符号 | `weld` | 指定类型、锚点与参数，如 `k=6 side=both`。 | 焊接结构图。 |
| 形位公差 | `tol` | 指定位置与公差文本。 | GD&T 框格。 |

---

## 8. 编辑与修改

### 8.1 变换类 【已有】

| 功能 | 命令 | 典型用法 | 说明 |
|---|---|---|---|
| 移动 | `move` | `move 10 0` | 平移当前选择集。 |
| 复制移动 | `move_copy` | `move_copy 10 0` | 保留源对象。 |
| 复制 | `duplicate` / `copy_dup` | 命令行或编辑菜单 | 生成副本。 |
| 旋转 | `rotate` | `rotate 0,0 90` | 绕基点旋转，逆时针为正。 |
| 缩放 | `scale` | `scale 0,0 2` | 绕基点比例缩放。 |
| 镜像 | `mirror` | `mirror 0,0 100,0` | 沿轴镜像，可选择删除源。 |
| 对齐 | `align` | 按源点/目标点对 | 可结合两点实现旋转与缩放对齐。 |
| 拉伸 | `stretch` | 选中部分后输入参数 | 局部变形。 |
| 反向 | `revert` / `revert_direction` | 选择有向图元 | 反转线/弧/多段线方向。 |

### 8.2 修剪与构造 【已有】

| 功能 | 命令 | 说明 |
|---|---|---|
| 修剪 | `trim <id> <拾取点> [边界id...]` | 以边界剪去指定侧；无边界时全体互为边界。 |
| 延伸 | GUI 工具 `extend` | 命令行别名 `ex` 可能不可用，建议使用工具入口。 |
| 打断 | `break_at <id> <x,y>` | 在指定点分成两个图元。 |
| 定数等分 | `divide <id> <n>` | 等分为 n 段。 |
| 定距等分 | `measure <id> <间距>` | 按间距放置点。 |
| 拉长 | `lengthen <id> <参数>` | 按增量或百分比拉长。 |
| 偏移 | `offset <id> <距离> [方向点]` | 直线/圆/弧/折线等距复制。 |
| 圆角 | `fillet <id1> <id2> <半径>` | 两图元倒圆角。 |
| 倒角 | `chamfer <id1> <id2> <距离1> <距离2|角度>` | 距离-距离或距离-角度。 |

### 8.3 组合、阵列与清理 【已有】

| 功能 | 命令 | 说明 |
|---|---|---|
| 连合 | `join [容差mm]` | 同层首尾相接的 Line/Arc 合成 Polyline，默认容差 0.5 mm。 |
| 多段线编辑 | `pedit <id> [close/open/width/join/fit/spline]` | 闭合、打开、线宽、合并、拟合、样条化。 |
| 矩形阵列 | `arrayrect <行> <列> <行距> <列距>` | 作用于当前选择集。 |
| 环形阵列 | `arraypolar <中心> <个数> [填充角]` | 选中圆/弧时可省略中心。 |
| 路径阵列 | `arraypath <路径id> <间距>` 或 `arraypath divide <路径id> <数量>` | 沿 Polyline/Spline 排列。 |
| 分解 | `explode <id>` | 块引用或复合图元拆为基本图元。 |
| 样条打散 | `spline_explode <id>` | 样条转折线/线段。 |
| 特性匹配 | `matchprop <源id> <目标id...>` | 复制图层、颜色、线宽、线型。 |
| 清理 | `purge [all/layers/blocks/dimstyles]` | 删除未引用项，保留 0 层与 Standard 样式。 |
| 删除重复 | `overkill` | 同类型同几何视为重复，保留首个。 |
| 删除 | `delete [id...]` | 删除指定对象或当前选择集。 |
| 清空 | `clear` | 清空全部图元。 |
| 撤销 | `undo` / `Ctrl+Z` | 撤销一步编辑。 |
| 重做 | `redo` / `Ctrl+Y` | 恢复一步撤销。 |

### 8.4 剪贴板 【已有】

| 操作 | 快捷键 | 命令 | 说明 |
|---|---|---|---|
| 内部复制 | `Ctrl+C` | `copyclip` | 写入程序内部剪贴板。 |
| 带基点复制 | `Ctrl+Shift+C` | `copybase` | 指定基点。 |
| 内部粘贴 | `Ctrl+V` | `pasteclip` | 粘贴为独立图元。 |
| 粘贴为块 | `Ctrl+Shift+V` | `pasteblock` | 剪贴板内容收为块引用。 |
| 系统剪贴板 | `Ctrl+Alt+C` / `Ctrl+Alt+V` | — | 与操作系统交换。 |

---

## 9. 选择与过滤

### 9.1 基本选择 【已有】

- 点选：左键单击图元。
- 窗口选择：从左向右拖拽，通常要求完全包含。
- 相交选择/圈交：从右向左拖拽或 `cp`，与框相交即可选中。
- 栏选：`fence`，画折线穿过目标。
- 圈围：`wp`；圈交：`cp`。
- 全选：`Ctrl+A` / `select_all`。
- 清空选择：`Ctrl+Shift+K` / `select_none`。
- 反选：`Ctrl+Shift+I` / `invert`。

### 9.2 高级选择 【已有】

| 功能 | 命令 | 说明 |
|---|---|---|
| 选择/取消实体 | `select_entity` / `deselect_entity` | 按 id 增减选择集。 |
| 选择/取消轮廓 | `select_contour` / `toggle_contour` | 选择相连轮廓链。 |
| 选择/取消相交 | `select_intersected` / `deselect_intersected` | 按相交关系选择。 |
| 选择/取消图层 | `select_layer` / `deselect_layer` / `toggle_layer` | 按图层批量选择。 |
| 快速选择 | `qselect` | 按类型、图层、颜色、线型四维过滤。 |
| 选择过滤器 | `filter` | 创建或应用命名过滤器。 |
| 编组 | `group` | 创建、加入、移除、列表、解组。 |

示例：

```text
qselect type=直线 layer=0
filter type=圆 layer=中心线
```

### 9.3 夹点编辑 【已有】

- 选中图元后显示夹点。
- 悬停夹点进入热态，拖拽可修改几何。
- `Enter` 确认夹点操作。
- `Esc` 退出夹点热态。
- 多段线、样条、块引用等有专门夹点行为。

---

## 10. 视图与坐标

### 10.1 视图操作 【已有】

| 功能 | 操作/快捷键 | 命令 |
|---|---|---|
| 放大 | 工具栏/滚轮向上 | `zoom <倍率>` |
| 缩小 | 工具栏/滚轮向下 | `zoom <倍率>` |
| 适配窗口 | `Ctrl+0` | — |
| 窗口缩放 | `Ctrl+9` | — |
| 平移 | 中键/右键拖拽/空格+左键 | — |
| 重生成 | `F5` | `regen` |
| 栅格显示 | `F7` | `grid [on/off]` |
| XY 平面 | `Ctrl+1` | `plane xy` |
| YZ 平面 | `Ctrl+2` | `plane yz` |
| XZ 平面 | `Ctrl+3` | `plane xz` |

### 10.2 UCS 与工作平面 【已有·受限】

命令 `ucs` 支持：

```text
ucs world
ucs view
ucs 3p <p1> <p2> [p3]
ucs object <id>
ucs z <x,y> <角度>
ucs origin <x,y> [角度]
ucs save / set / restore / delete / list
ucs coords wcs|ucs
```

限制：这是二维环境的工作坐标与平面管理，不是完整三维动态 UCS 系统。

### 10.3 输入辅助状态 【已有】

| 状态 | 开关 | 命令 |
|---|---|---|
| 正交 | `F8` | `ortho [on/off]` |
| 栅格吸附 | `F9` | `snap [on/off/步长]` |
| 极轴追踪 | `F10` | `polar [on/off/增量角]` |
| 对象捕捉追踪 | `F11` | `otrack [on/off]` |
| 动态输入 | `F12` | `dyn [on/off]` |
| 临时追踪点 | — | `tk <x,y>` / `tkclear` |

---

## 11. 图层、颜色、线型与线宽

### 11.1 图层管理 【已有】

图层面板支持：

- 新建、删除、重命名图层。
- 设置图层颜色、线型、线宽。
- 置为当前图层。
- 显示/隐藏图层。
- 锁定/解锁图层。
- 将选中对象赋到目标图层。

命令：

```text
layer add <名称> [颜色] [线宽] [线型]
layer delete <名称>
layer rename <旧名> <新名>
layer visible <名称> on|off
layer lock <名称> on|off
layer set <名称>
layer assign <名称>
layer apply
layer list
```

相关命令：

- `layiso`：只显示选中对象所在图层。
- `layuniso`：取消图层隔离。
- `laymch <目标图层>`：图层匹配。
- `ltscale <正数>`：全局线型比例。

### 11.2 颜色与线宽 【已有】

- `color #RRGGBB` 或 `color red` 设置后续绘制颜色。
- `width 1|2|3|5` 设置后续绘制线宽档位。
- 工具栏也提供颜色与线宽下拉。

限制：工具条线型下拉列表存在索引边界不一致问题，部分后段线型选择可能不生效，详见第 18 章。

### 11.3 线型 【已有】

内置 9 种线型，包括连续、虚线、点画线、双点画线、波浪线、双折线等。

命令：

```text
linetype
linetype scale <正数>
linetype <线型名>
ltscale <正数>
```

`linetype load` 会提示内置全部线型，无需外部 `.lin` 文件。

### 11.4 标准切换 【已有】

工具栏支持在 GB/T 4458 与 ISO 129 间切换，影响标注、线宽、箭头与文字规则。  
`standards` 命令可查询内嵌国标表，如幅面、比例、字体、线型、粗糙度、公差、形位公差、螺纹、齿轮、轴承、焊缝、CAD 图层等。

---

## 12. 尺寸标注、文字与符号

### 12.1 尺寸标注 【已有】

支持的标注类型：

| 类型 | 命令/工具 | 说明 |
|---|---|---|
| 线性 | `dim linear` / `dim_linear` | 两点间线性尺寸。 |
| 对齐 | `dim aligned` / `dim_aligned` | 沿真实倾斜方向。 |
| 水平 | `dim_h` | 水平投影尺寸。 |
| 竖直 | `dim_v` | 竖直投影尺寸。 |
| 角度 | `dim_angle` | 两线夹角。 |
| 半径 | `dim_radius` | 圆/弧半径。 |
| 直径 | `dim_diameter` | 圆/弧直径。 |
| 弧长 | `dim_arc` | 圆弧长度。 |
| 引线 | `dim_leader` | 引线说明。 |
| 坐标 | `dim_ordinate` | 坐标尺寸。 |
| 基线 | `dim_baseline` / `qdim baseline` | 共基准串联标注。 |
| 连续 | `dim_continue` / `qdim continue` | 链式连续标注。 |

智能标注：`dims` 或 `dim_smart` 根据选中对象自动选择对齐/直径等类型。

### 12.2 标注样式 【已有】

```text
dimstyle list
dimstyle new <名> [gbt|iso]
dimstyle set <名>
dimstyle rename <旧> <新>
dimstyle apply <名> [id...]
dimstyle delete <名>
```

### 12.3 尺寸驱动 【已有·受限】

`dimedit` 支持修改标注值并联动几何：

```text
dimedit <标注id> <新值>
dimedit 50
```

支持的标注类型：对齐、距离、水平、竖直、半径、直径、角度。  
数值解析容忍 `R20`、`⌀50`、`45°` 等前后缀。

限制：

- 其他标注类型不支持尺寸驱动。
- 尺寸驱动总开关关闭时，`dimedit` 仅更新显示文本，不修改几何。
- 工业级欠约束/过约束诊断与稳定求解仍待扩展。

### 12.4 文字与域 【已有·受限】

- `text`、`dtext`：单行文字。
- `mtext`：多行文字。
- `ddedit`：编辑文字。
- `prop <名称> <值>`：设置自定义图纸特性。
- 在多行文字或表格中使用 `%<PROP:名称>%` 引用特性。
- `fieldupdate`：重算全模型域宿主。
- 打开文档时可按选项自动刷新域。

限制：FIELD 码集远小于主流 CAD，复杂字段与注释性比例尚未完整实现。

---

## 13. 约束与几何辅助

### 13.1 几何约束 【已有】

源码定义 21 类约束，包括：重合、固定、固定点、水平、竖直、平行、垂直、相切、共线、同心、对称、相等、长度、距离、角度、半径、直径等。

命令：

```text
constraint add <类型> <idA> <idB> [值|轴id]
constraint solve
constraint list
constraint check
constraint diagnose
constraint clear
```

### 13.2 自动几何生成 【已有】

| 命令 | 功能 | 示例 |
|---|---|---|
| `perp` | 过点作已知直线的垂线。 | `perp 1 30,20` |
| `tangent2` | 求两圆/弧的全部公切线，0–4 条。 | `tangent2 1 2` |
| `qconnect` | 从一点连到圆/弧最近象限点。 | `qconnect 3 10,10` |

### 13.3 约束求解限制 【已有·受限】

- 当前为自研轻量求解器。
- 可用于常规草图约束与尺寸驱动。
- 装配级、欠约束/过约束诊断、复杂尺寸链的可靠性不等同于工业求解器。
- 后期可接入 SKT/工业求解器，详见第 20 章。

---

## 14. 块、属性、外部参照与数据库

### 14.1 块 【已有】

```text
block create <块名> [基点x,y] [delete|keep]
block insert <块名> <x,y> [旋转] [sx] [sy]
block list
block info <块名>
block redefine
block explode <引用id>
block delete <块名>
block rename <旧名> <新名>
```

插入块：

```text
insert <块名> <x,y> [旋转度] [缩放X] [缩放Y]
```

写块：

```text
wblock <输出.dxf> [id...]
```

### 14.2 属性块 【已有】

| 命令 | 功能 |
|---|---|
| `attdef <块名> <标签> [提示] [x,y] [字高]` | 为块定义追加属性。 |
| `attedit <引用id> <标签> <值>` | 修改块引用属性值。 |
| `attlist [引用id]` | 列出属性。 |

### 14.3 动态块参数 【已有·受限】

```text
dynlist <块名>
dynset <引用id> <参数名> <值|状态名>
```

限制：提供参数读写入口，但动态块深度求值、复杂动作与可见性状态不等同于主流 CAD。

### 14.4 外部参照 【已有·受限】

```text
xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy]
xref detach <块名>
xref list
xref path <块名>
```

适用场景：引用底图、标准图框、共享部件。  
限制：路径管理、XBIND、重载/卸载语义与主流 CAD 相比仍较弱。

### 14.5 数据库与设计中心 【已有·受限】

“数据库”菜单提供：配置、绘图记录、保存图纸、另存模板。  
设计中心面板用于浏览块、标准与可复用内容。

限制：属于轻量本地数据管理，不等于 PLM/PDM 集成。

---

## 15. 布局、打印与数据交换

### 15.1 布局与视口 【已有】

```text
layout new <名>
layout set <名>
layout list
layout rename <旧> <新>
layout delete <名>
layout copy <源> <目标>
layout vpscale <视口id|当前> <比例>
layout vpfreeze <层名>
layout vpthaw <层名>
mview <图纸X> <图纸Y> <宽> <高> [比例]
```

适用场景：同一模型按不同比例出图、图框内多视图排版。

### 15.2 图框与标题栏 【已有】

- 菜单“图纸”提供填写标题栏、插入标准标题栏、按幅面生成图框与标题栏。
- `sheetframe <A0|A1|A2|A3|A4> [portrait|landscape]` 按幅面重新生成图框，仅替换图框/标题栏图层内容。
- 标准留边按 GB/T 14689 查询，默认第一角画法。

### 15.3 打印与发布 【已有·受限】

- 打印设置：纸张、幅面、比例、打印样式。
- 打印预览：出图前检查。
- 导出 PDF：生成 PDF。
- 发布：多页输出入口。
- 直接打印：连接物理打印机。

限制：无完整图纸集、STB 打印样式、戳记与电子传递闭环。

### 15.4 DXF/DWG/STEP 导出 【已有·受限】

- DXF GUI 导出版本：R12、R2000、R2004、R2007、R2010、R2013、R2018。
- DXF 命令：`dxfout <路径.dxf> [-v R12|2000]`，命令行参数仅暴露 R12/R2000；高版本建议使用 GUI 保存/导出。
- 原生实体导出可保留块 `INSERT`、标注 `DIMENSION`、填充 `HATCH`。
- DWG：原生 R12 或桥接 R2000，最高 R2000。
- STEP：只写，文字不导出。

---

## 16. 查询、标准与合规检查

### 16.1 查询测量 【已有】

| 命令 | 功能 | 示例 |
|---|---|---|
| `list [id...]` | 列出图元类型、图层、几何信息。 | `list 3` |
| `dist x1,y1 x2,y2` | 测量距离与角度。 | `dist 0,0 30,40` |
| `area <封闭id>` | 测量面积。 | `area 5` |
| `massprop <封闭id>` | 面积、质心、惯性矩。 | `massprop 5` |
| `id x,y` | 显示点坐标。 | `id 12.5,7` |
| `prop` | 列出/设置/删除图纸特性。 | `prop 项目名称 减速器箱体` |
| `fieldupdate` | 重算域宿主。 | `fieldupdate` |

### 16.2 标准知识库 【已有】

```text
standards
standards <表名>
standards <表名> <键名>
```

示例：

```text
standards sheet_sizes A3
standards scales
```

数据源为内嵌的《机械制图手册（第四版）》12 类国标表。

### 16.3 工程制图合规检查 【已有】

```text
stdcheck
checkcompliance
```

检查内容包括：

- 线宽粗细比。
- 两线最小间隙。
- 字号下限。
- 粗糙度符号。
- 焊缝符号完整性。
- 幅面留边。
- 图层命名。

检查为只读诊断，不修改图元；结果写入执行信息并刷新合规检查面板。

### 16.4 AI 助手 【已有·受限】

AI 助手面板提供自然语言入口，可通过 `runSketchCommand("checkcompliance")` 等调用内置命令。  
“绘制一个椭圆”会派发 `ellipse 0,0 50 25 0` 并生成 `Ellipse` 图元；含“圆/半径/直径”的请求仍派发 `circle`/`circle3p`。  
限制：属于命令派发与辅助入口，不等同于完整智能设计系统。

---

## 17. 自动化与二次开发

### 17.1 宏录制 【已有】

| 命令 | 功能 |
|---|---|
| `macrorec` | 开始录制命令。 |
| `macrostop` | 停止录制。 |
| `macroshow` | 查看录制缓冲。 |
| `macrosave "<路径>"` | 保存宏。 |
| `macroplay "<路径>"` | 回放宏，遇错即停。 |
| `scr "<路径>" [continue]` | 执行 SCR 脚本；`continue` 表示遇错继续。 |

### 17.2 JavaScript 脚本 【已有·受限】

“脚本 → JavaScript 示例 API”提供脚本宿主入口。  
可通过脚本调用图元创建、编辑与查询接口。

限制：生态与文档完整度远低于主流 CAD 的 LISP/.NET/ARX 体系。

### 17.3 命令别名 【已有】

兼容 `acad.pgp` 风格。

```text
alias <别名> <完整命令>
aliasload "<路径>"
aliasreload
aliassave ["<路径>"]
aliaslist
```

常用默认别名见附录 D。

### 17.4 CUIx 与工作空间 【已有·受限】

```text
cuiload "<路径>"
cuisave ["<路径>"]
wsnew <id> [标题]
wsload <id>
wslist
wsdel <id>
tbnew <id> [标题]
tbadd <工具条> <命令> [文字] [图标]
tbrm <工具条> <序号>
tbdel <工具条>
```

限制：支持工具条与工作空间配置，但不是主流 CAD 级深度可定制 CUI/Ribbon 系统。

### 17.5 插件、SDK 与 COM 【已有·受限】

- SDK 提供 C++ 插件接口与示例。
- 插件支持加载与自动加载配置。
- Windows 下提供 COM 自动化注册项 `HKCU\Software\SketchCAD`。
- 插件命令通过 `registerExternalCommand` 进入命令解释器。

限制：插件生态较小，COM 自动化接口覆盖面有限。

---

## 18. 设置、诊断与已知问题

### 18.1 常规设置 【已有】

- 界面语言：`zh-CN` / `en`。
- 主题：亮色/暗色。
- 高 DPI 支持。
- 单位与图纸界限。
- 自动保存路径与周期。
- DWG 转换组件路径。
- 域打开时自动更新。

路径：

| 路径 | 用途 |
|---|---|
| `%LOCALAPPDATA%\SketchCAD` | 自动保存、崩溃日志、运行日志。 |
| `HKCU\Software\SketchCAD` | COM 自动化注册。 |

### 18.2 诊断 【已有】

- 启动时执行 DPI、主题、语言、会话标记检查。
- 异常退出后提示恢复。
- 快捷键冲突审计会写入警告日志。
- 命令行错误会给出相近命令建议。

### 18.3 已知问题与不一致 【已有·受限】

| 问题 | 表现 | 建议 |
|---|---|---|
| 线型下拉索引不一致 | 工具条列出 9 种线型，但应用函数可能拒绝后 4 项。 | 需要修复 `applyLineStyle()` 的索引判断。 |
| `properties` 快捷键元数据冲突 | 命令目录标 `Ctrl+1`，UI 中 `Ctrl+1` 绑定 XY 平面。 | 以 UI 实际绑定为准，开发时统一元数据。 |
| 默认别名 `ex` | 指向 `extend`，但命令注册表无同名精确命令。 | 使用 GUI 延伸工具，或新增 `extend` 命令入口。 |
| 旧文档图元数 | 旧手册/README 写 12 类，源码为 20 类。 | 以本文为准。 |
| 旧文档新建流程 | 旧手册称新建弹图纸设置对话框，源码为空白新建。 | 以本文为准。 |
| STEP 读写描述冲突 | README 曾写读写均支持，源码提示只写不读。 | 以本文为准。 |
| DWG 写出上限 | 最高 R2000。 | 需要 R2013/R2018 时导出 DXF 高版本。 |
| 尺寸驱动部分支持 | 并非所有标注类型可驱动。 | 使用支持的 7 类标注。 |
| 大图性能 | 十万图元级尚无成熟 SLO。 | 拆分图纸或控制填充、样条密度。 |

---

## 19. 命令、工具与快捷键参考

### 19.1 快捷键速查 【已有】

| 快捷键 | 功能 |
|---|---|
| `F1` | 帮助/命令列表 |
| `F5` | 重生成 |
| `F7` | 栅格显示 |
| `F8` | 正交 |
| `F9` | 栅格吸附 |
| `F10` | 极轴追踪 |
| `F11` | 对象捕捉追踪 |
| `F12` | 动态输入 |
| `Ctrl+N` | 新建 |
| `Ctrl+O` | 打开 |
| `Ctrl+S` | 保存 |
| `Ctrl+Q` | 退出 |
| `Ctrl+E` | 导出 DXF |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Ctrl+C` / `Ctrl+V` | 内部复制 / 粘贴 |
| `Ctrl+Alt+C` / `Ctrl+Alt+V` | 系统剪贴板复制 / 粘贴 |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | 带基点复制 / 粘贴为块 |
| `Ctrl+Shift+P` | 命令面板 |
| `Ctrl+A` | 全选 |
| `Ctrl+Shift+K` | 全部不选 |
| `Ctrl+Shift+I` | 反选 |
| `Ctrl+Shift+M` | 移动/复制 |
| `Ctrl+Shift+D` | 复制 |
| `Ctrl+Shift+H` | 剖面填充 |
| `Ctrl+Shift+O` | 偏移 |
| `Ctrl+R` / `Ctrl+K` | 旋转 / 缩放 |
| `Ctrl+0` / `Ctrl+9` | 适配窗口 / 窗口缩放 |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | XY / YZ / XZ 平面 |
| `L` / `R` / `P` / `C` / `A` / `E` / `S` | 直线 / 矩形 / 多边形 / 圆 / 弧 / 椭圆 / 样条 |
| `D` / `N` | 标注 / 约束 |
| `T` / `X` | 修剪 / 延伸 |
| `K` | 连续绘制 |
| `Shift+S/D/A/R/I` | 智能/对齐/角度/半径/直径标注 |

> 注意：`Ctrl+K` 为缩放，`K` 为连续绘制；两者不同。旧手册中的 `Ctrl+K` 全部不选已改为 `Ctrl+Shift+K`。

### 19.2 核心命令用法速查 【已有】

| 命令 | 用法 | 说明 |
|---|---|---|
| `line` | `line x1,y1 x2,y2` | 两点直线。 |
| `circle` | `circle cx,cy r` | 圆心+半径圆。 |
| `circle3p` | `circle3p x1,y1 x2,y2 x3,y3` | 三点圆。 |
| `arc` | `arc cx,cy r a0 [a1]` | 圆心、半径、起止角。 |
| `ellipse` | `ellipse [cx,cy] [长半轴] [短半轴] [旋转角]` | 圆心+长短半轴椭圆；无参数时用默认值。 |
| `rect` | `rect x0,y0 x1,y1` | 两角点矩形。 |
| `poly` | `poly x1,y1 x2,y2 ...` | 顶点多边形。 |
| `spline` | `spline p1 p2 ...` | 样条曲线。 |
| `point` | `point <x,y> [x,y ...]` | 绘制点。 |
| `xline` | `xline x,y <角度>` 或 `xline x1,y1 x2,y2` | 构造线。 |
| `hatch` | `hatch [scale\|angle\|pattern ...]` | 关联填充。 |
| `move` | `move <dx> <dy> [copy]` | 移动选择集。 |
| `rotate` | `rotate <基点> <角度> [copy]` | 旋转。 |
| `scale` | `scale <基点> <比例> [copy]` | 缩放。 |
| `mirror` | `mirror x1,y1 x2,y2 [delete源]` | 镜像。 |
| `offset` | `offset <id> <距离> [方向点]` | 偏移。 |
| `trim` | `trim <id> <x,y> [边界id...]` | 修剪。 |
| `break_at` | `break_at <id> <x,y>` | 打断。 |
| `divide` | `divide <id> <n>` | 定数等分。 |
| `measure` | `measure <id> <间距>` | 定距等分。 |
| `fillet` | `fillet <id1> <id2> <半径>` | 圆角。 |
| `chamfer` | `chamfer <id1> <id2> <距离1> <距离2|角度>` | 倒角。 |
| `join` | `join [容差mm]` | 连合。 |
| `arrayrect` | `arrayrect <行> <列> <行距> <列距>` | 矩形阵列。 |
| `arraypolar` | `arraypolar <中心> <个数> [填充角]` | 环形阵列。 |
| `arraypath` | `arraypath <路径id> <间距>` | 路径阵列。 |
| `pedit` | `pedit <id> [close/open/width/join/fit/spline]` | 多段线编辑。 |
| `matchprop` | `matchprop <源id> <目标id...>` | 特性匹配。 |
| `purge` | `purge [all/layers/blocks/dimstyles]` | 清理未引用项。 |
| `overkill` | `overkill` | 删除重复对象。 |
| `list` | `list [id...]` | 列表查询。 |
| `dist` | `dist x1,y1 x2,y2` | 距离。 |
| `area` | `area <封闭图元id>` | 面积。 |
| `massprop` | `massprop <封闭图元id>` | 面域特性。 |
| `id` | `id x,y` | 点坐标。 |
| `dim` | `dim linear|aligned|h|v x1,y1 x2,y2 [px,py]` | 尺寸标注。 |
| `dims` | `dims [id...]` | 智能标注。 |
| `qdim` | `qdim [baseline|continue]` | 快速标注。 |
| `dimstyle` | `dimstyle list|new|set|rename|apply|delete` | 标注样式。 |
| `dimedit` | `dimedit <标注id> <新值>` | 尺寸驱动。 |
| `tol` | `tol <x,y> [公差文本]` | 形位公差。 |
| `constraint` | `constraint add/solve/list/check/diagnose/clear` | 草图约束。 |
| `perp` | `perp <直线Id> <x,y>` | 垂线。 |
| `tangent2` | `tangent2 <圆IdA> <圆IdB>` | 公切线。 |
| `qconnect` | `qconnect <目标圆Id> <x,y>` | 象限连接。 |
| `layer` | `layer add/delete/rename/visible/lock/set/assign/apply/list` | 图层管理。 |
| `layiso` | `layiso` | 图层隔离。 |
| `ltscale` | `ltscale <正数>` | 线型比例。 |
| `block` | `block create/insert/list/info/redefine/explode/delete/rename` | 块管理。 |
| `insert` | `insert <块名> <x,y> [旋转] [sx] [sy]` | 插入块。 |
| `xref` | `xref attach/detach/list/path` | 外部参照。 |
| `wblock` | `wblock <输出.dxf> [id...]` | 写块。 |
| `layout` | `layout new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw` | 布局。 |
| `mview` | `mview <x> <y> <宽> <高> [比例]` | 建视口。 |
| `ucs` | `ucs world/view/3p/object/z/origin/save/set/restore/delete/list/coords` | UCS。 |
| `plane` | `plane xy|yz|xz` | 工作平面。 |
| `text` | `text <x,y> <字高> <内容...>` | 单行文字。 |
| `mtext` | `mtext <x,y> <字高> <内容...>` | 多行文字。 |
| `ddedit` | `ddedit <id> <新内容>` | 编辑文字。 |
| `standards` | `standards [<表名> [<键名>]]` | 标准知识库。 |
| `stdcheck` | `stdcheck` | 合规检查。 |
| `dxfout` | `dxfout <路径.dxf> [-v R12|2000]` | 导出 DXF。 |
| `help` | `help [<命令名>]` | 帮助。 |

### 19.3 精确命令全集索引 【已有】

`config/command_catalog.json` 现统一收录 222 条内置命令（150 个精确命令 + 72 个前缀族命令），是 `help`、AI 提示词、Tab 补全与命令面板的唯一元数据来源；程序启动时自动读取，无需重新编译。可通过“工具 → 命令目录维护...”在线增删改命令与参数定义并写回 JSON。下表按分类列出常用条目，完整清单以 `help` 输出和 JSON 配置为准：

| 分类 | 命令 |
|---|---|
| 绘图 | `line`、`circle`、`circle3p`、`ellipse`、`arc`、`rect`、`poly`、`spline`、`point`、`xh`、`xline`、`hatch`、`bhatch`、`aircraft_views`、`line_h`、`line_v`、`line_angle`、`spline_parabola_4p`、`spline_through_points`、`spline_from_polyline`、`freehand_line`、`polyline` |
| 修改 | `move`、`move_copy`、`duplicate`、`copy_dup`、`rotate`、`scale`、`mirror`、`revert`、`revert_direction`、`lengthen`、`offset`、`break_at`、`divide`、`fillet`、`chamfer`、`explode`、`align`、`stretch`、`join`、`arrayrect`、`arraypolar`、`arraypath`、`trim`、`delete`、`undo`、`redo`、`clear`、`measure`、`matchprop`、`properties`、`purge`、`overkill`、`pedit` |
| 查询 | `list`、`dist`、`area`、`massprop`、`id`、`fieldupdate`、`prop`、`qselect`、`filter` |
| 标注 | `dim`、`dims`、`dim_smart`、`qdim`、`dimstyle`、`dimedit`、`tol` |
| 约束 | `constraint`、`perp`、`tangent2`、`qconnect` |
| 选择 | `select_all`、`select_none`、`invert`、`select_entity`、`deselect_entity`、`select_contour`、`toggle_contour`、`select_intersected`、`deselect_intersected`、`select_layer`、`deselect_layer`、`toggle_layer` |
| 图层/样式 | `layer`、`layiso`、`layuniso`、`laymch`、`ltscale`、`width`、`color`、`linetype`、`chain` |
| 视图 | `plane`、`ucs`、`grid`、`zoom`、`regen`、`ortho`、`snap`、`polar`、`otrack`、`dyn`、`tk`、`tkclear` |
| 块/参照 | `block`、`insert`、`attdef`、`attedit`、`attlist`、`dynset`、`dynlist`、`wblock`、`xref`、`group` |
| 剪贴板 | `copyclip`、`copybase`、`pasteclip`、`pasteblock` |
| 布局/图框 | `layout`、`mview`、`sheetframe` |
| 文字 | `text`、`dtext`、`mtext`、`ddedit` |
| 符号 | `weld`、`hatchedit` |
| 文件/导出 | `dxfout` |
| 脚本/宏 | `macrorec`、`macrostop`、`macrosave`、`macroshow`、`macroplay`、`scr` |
| 别名/CUI | `alias`、`aliasreload`、`aliasload`、`aliassave`、`aliaslist`、`cuiload`、`cuisave`、`wsnew`、`wsload`、`wslist`、`wsdel`、`tbnew`、`tbadd`、`tbrm`、`tbdel` |
| 标准/帮助 | `standards`、`stdcheck`、`checkcompliance`、`help` |

### 19.4 前缀命令族 【已有】

| 前缀 | 已见子命令 |
|---|---|
| `line_` | `line_h`、`line_v`、`line_2pt`、`line_2points`、`line_angle`、`line_angle_from_line`、`line_rel_angle`、`line_parallel`、`line_parallel_pt`、`line_bisector`、`line_tangent_pc`、`line_tangent_cc`、`line_tangent_ortho`、`line_orthogonal`、`line_orthogonal_from_line`、`line_from_point_to_line`、`line_gap`、`line_join`、`line_snake`、`line_snake_x`、`line_snake_y`、`line_types` |
| `xline_` | `xline_h`、`xline_v`、`xline_horizontal`、`xline_vertical`、`xline_a`、`xline_angle`、`xline_2pt`、`xline_2points`、`xline_b`、`xline_bisector`、`xline_o`、`xline_parallel` |
| `arc_` | `arc_3p`、`arc_cpaa`、`arc_cpc`、`arc_ea_1pt`、`arc_ea_axis`、`arc_length`、`arc_tan`、`arc_tpa`、`arc_tph`、`arc_tpl`、`arc_tpr` |
| `poly_` | `poly_regular`、`poly_circumscribed`、`poly_edge`、`poly_star` |
| `polyline_` | `polyline_add_node`、`polyline_append_node`、`polyline_delete_node`、`polyline_delete_between`、`polyline_trim_segments`、`polyline_equidistant`、`polyline_from_segments`、`polyline_arcs_to_chords`、`polyline_change_segment_type`、`polyline_fit`、`polyline_spline`、`polyline_width`、`polyline_close`、`polyline_join`、`polyline_reverse` |
| `spline_` | `spline_parabola`、`spline_parabola_4p`、`spline_parabola_fd`、`spline_hyperbola`、`spline_through_points`、`spline_from_polyline`、`spline_append_pt`、`spline_insert_pts`、`spline_remove_pts`、`spline_remove_between`、`spline_explode` |
| `ellipse_` | `ellipse_1pt`、`ellipse_axis`、`ellipse_foci`、`ellipse_4p`、`ellipse_c3p`、`ellipse_inscribed` |
| `rect_` | `rect_chamfer`、`rect_fillet` |

### 19.5 GUI 工具全集索引 【工具模式】

工具注册表共 119 项，按类别归纳如下：

- 基础绘图：`line`、`circle`、`circle3p`、`rect`、`arc`、`poly`、`spline`、`mtext`、`table`、`weld`。
- 选择：`select`、`select_window`、`deselect_window`、`select_entity`、`deselect_entity`、`select_contour`、`toggle_contour`、`select_intersected`、`deselect_intersected`、`select_layer`、`toggle_layer`、`qselect`、`fence`、`wp`、`cp`。
- 编辑：`delete`、`trim`、`extend`、`constraint`。
- 直线族：`line_h`、`line_v`、`line_angle`、`line_parallel`、`line_parallel_pt`、`line_bisector`、`line_tangent_pc`、`line_tangent_cc`、`line_tangent_ortho`、`line_orthogonal`、`line_rel_angle`、`line_snake`、`line_snake_x`、`line_snake_y`、`line_angle_from_line`、`line_orthogonal_from_line`、`line_from_point_to_line`。
- 构造线族：`xline`、`xline_h`、`xline_v`、`xline_angle`、`xline_2pt`、`xline_bisector`、`xline_parallel`。
- 圆族：`circle_2p`、`circle_2pr`、`circle_cr`、`circle_t2e1p`、`circle_t1e2p`、`circle_t2er`、`circle_t3e`、`circle_inscribed`、`circle_concentric`、`circle_byarc`。
- 多边形族：`poly_regular`、`poly_circumscribed`、`poly_edge`、`rect_chamfer`、`rect_fillet`、`poly_star`。
- 样条族：`spline_parabola`、`spline_hyperbola`、`spline_parabola_4p`、`spline_parabola_fd`、`spline_through_points`、`freehand_line`、`spline_from_polyline`、`spline_append_pt`、`spline_insert_pts`、`spline_remove_pts`、`spline_remove_between`、`spline_explode`。
- 椭圆族：`ellipse_1pt`、`ellipse_axis`、`ellipse_foci`、`ellipse_4p`、`ellipse_c3p`、`ellipse_inscribed`。
- 折线族：`polyline`、`boundary`、`bpoly`、`polyline_add_node`、`polyline_append_node`、`polyline_delete_node`、`polyline_delete_between`、`polyline_trim_segments`、`polyline_equidistant`、`polyline_from_segments`、`polyline_arcs_to_chords`、`polyline_change_segment_type`、`polyline_fit`、`polyline_spline`、`polyline_width`、`polyline_close`、`polyline_join`、`polyline_reverse`。
- 标注族：`dim`、`dim_linear`、`dim_aligned`、`dim_h`、`dim_v`、`dim_angle`、`dim_radius`、`dim_diameter`、`dim_smart`、`dim_baseline`、`dim_continue`、`dim_arc`、`dim_leader`、`dim_ordinate`。

---

## 20. 待扩展功能与迭代路线

> 本章全部为【待扩展】。这些功能参考 AutoCAD、中望 CAD、浩辰 CAD 等主流二维 CAD 的常见能力，作为后期修改与产品迭代依据，不代表当前版本已经可用。

### 20.1 P0：互操作与交付闭环

| ID | 功能 | 主流参照 | 当前状态 | 建议实现 | 适用场景 |
|---|---|---|---|---|---|
| P0-1 | DWG 写出升级到 R2013/R2018 | 原生全版本 DWG | 最高 R2000 | 评估 ODA SDK 或扩展合规桥；UI 明示真实能力 | 客户强制新版 DWG 交付 |
| P0-2 | DWG 转换器随包与可诊断 | 安装包内置转换器 | 依赖外部转换器，缺失时体验差 | 打包 `dwg2dxf`/`dwgwrite`，缺失时显示诊断与帮助链接 | 现场安装与初次打开 DWG |
| P0-3 | 黄金图纸交叉保真门禁 | 主流 CAD 互操作测试 | 无自动保真率报告 | 20 张黄金图纸交叉打开，统计图元/块/标注/填充/中文保真 | 企业落地验收 |
| P0-4 | 代码签名证书落地 | Authenticode | 流水线有脚本，证书未落地 | CI 注入证书，双签 Setup/exe | SmartScreen 与企业白名单 |
| P0-5 | 试用许可与离线激活 | 商业许可系统 | 无许可引擎 | 机器指纹 + 30 天试用 + 离线许可文件 | 商业售卖与装机控制 |

### 20.2 P1：产能与专业深度

| ID | 功能 | 主流参照 | 当前状态 | 建议实现 | 适用场景 |
|---|---|---|---|---|---|
| P1-1 | 注释性比例 | AutoCAD Annotative | 部分字段占位 | 标注/文字/属性块随视口比例缩放 | 一图多比例出图 |
| P1-2 | SHX 字体与中文兼容 | gbenor/gbcbig 等 | 部分只读渲染，字体未默认带 | 可选分发 SHX，完善缺失字体提示 | 打开外来 DWG |
| P1-3 | 关联阵列 | AutoCAD Associative Array | 一次性副本 | 新增阵列定义表，改源后联动 | 重复构件批量编辑 |
| P1-4 | 路径阵列切向对齐 | 路径项目沿切线旋转 | 默认不旋转 | `arraypath align=tangent|none` | 管路、沿轮廓排布 |
| P1-5 | 图层状态管理器 | LAYERSTATE/LAS | 无 | 保存/恢复层可见、冻结、锁定、颜色、线型快照 | 大装配显示方案切换 |
| P1-6 | 工业级约束求解器 | SKT/商用求解器 | 自研轻量求解 | 通过 Facade 接入 SKT，失败回落 Legacy | 复杂草图与装配联动 |
| P1-7 | 大图打开进度与取消 | 大图内存预算 | 十万图元风险 | 分块创建 Item、进度/取消、OOM 降级、脏矩形重绘 | 大总图 |
| P1-8 | 图层过滤器树与 LAYWALK | 图层状态/过滤器 | 无 | 图层过滤器与遍历工具 | 大图图层治理 |
| P1-9 | 渐变填充与孤岛检测 | HATCH 完整实现 | 关联图案填充已有，渐变无 | 扩展 Hatch 类型与孤岛算法 | 建筑/材料表达 |
| P1-10 | FIELD 完整码集 | 完整字段系统 | 子集 | 扩展字段类型、表达式与稳定性 | 自动图签与统计 |

### 20.3 P2：生态与体验

| ID | 功能 | 主流参照 | 当前状态 | 建议实现 | 适用场景 |
|---|---|---|---|---|---|
| P2-1 | Ribbon 界面 | AutoCAD Ribbon | 经典菜单/工具条 | 可切换 Ribbon 工作空间 | 重度用户效率 |
| P2-2 | 深度 CUI 定制 | 完整 CUIx | 工具条/工作空间基础 | 菜单、快捷键、Ribbon 全量可配置 | 企业标准化界面 |
| P2-3 | 命令级 F1 上下文帮助 | 上下文帮助 | 总表帮助为主 | 按当前命令定位参数/示例/相关命令 | 新用户学习 |
| P2-4 | AutoLISP 子集或迁移指南 | LISP 生态 | 无 | 实现子集或提供 JS 迁移文档 | 脚本资产迁移 |
| P2-5 | 图纸集与电子传递 | Sheet Set/ETRANSMIT | 无 | 图纸集管理、依赖打包 | 设计院交付 |
| P2-6 | 打印戳记与 STB | 完整打印样式 | 简化 CTB/发布 | 打印戳记、STB 样式 | 正式出图 |
| P2-7 | 动态块深度求值 | 完整动态块 | 参数读写入口 | 动作、约束、可见性状态完整求值 | 标准件复用 |
| P2-8 | XBIND 与路径管理 | 外部参照完整管理 | 基础附加/分离 | 绑定、重载、卸载、路径修复 | 外来图纸协作 |
| P2-9 | 动态 UCS 与夹点菜单 | 主流交互增强 | 基础 UCS/夹点 | 动态 UCS、夹点上下文菜单 | 高效编辑 |
| P2-10 | 跨平台构建 | Win/Linux/mac | 仅 Windows | 路径环境变量化、Linux 无头 CI | 高校/工作站部署 |
| P2-11 | 行业模板扩展 | 标准件库 | 六套模板 | 紧固件、焊接符号、管路模板 | 垂直行业 |
| P2-12 | 自动更新 | 商业 CAD 更新通道 | 无 | 静默安装、MST、更新通道 | 现场版本统一 |

### 20.4 明确不建议在早期投入的方向

在 P0 互操作与商业闭环未关闭前，不建议优先投入：

- 三维 CAD 功能。
- 云协同。
- 完整 LISP 生态复刻。
- 大规模 UI 重构。

原因：这些方向不能解决“能否打开客户图纸、能否交付新版格式、能否合法售卖”的核心阻塞。

---

## 21. 开发迭代参考

### 21.1 关键源码索引

| 目录/文件 | 职责 |
|---|---|
| `src/main.cpp` | 程序入口、DPI/主题/语言/启动画面/异常提示。 |
| `src/ui/MainWindow.*` | 菜单、工具栏、面板、文档、文件、打印、脚本入口。 |
| `src/ui/GraphicsView.*` | 鼠标交互、框选、夹点、缩放平移、双击行为。 |
| `src/ui/CommandLine.*` | 命令行解析交互、历史、补全、重复命令。 |
| `src/core/CommandCatalog.*` | 命令元数据：名称、中文名、用法、参数、示例、快捷键。 |
| `src/core/Commands.*` | 命令类与注册表。 |
| `src/core/CommandInterpreter.*` | 别名展开、精确/前缀匹配、外部命令、错误建议。 |
| `src/core/Element.*` | 图元类型、约束类型、线型、标注类型枚举。 |
| `src/core/SketchModel.*` | 图元模型、选择集、图层、块、撤销重做、约束、字段。 |
| `src/core/CadFormats.*` | JSON/DXF/DWG/STEP 读写。 |
| `src/core/ConstraintSolver.*` | 约束求解。 |
| `src/core/ToolRegistry.*` | GUI 工具模式注册。 |
| `src/core/MacroScript.*` | 宏与 SCR 脚本。 |
| `src/core/CuiAliasCommands.*` | 别名与 CUIx 命令。 |
| `src/core/AliasTable.*` | 默认别名与 `.pgp` 读写。 |
| `tests/SketchSelfTest.cpp` | 无头自测。 |
| `tests/golden/` | DXF/DWG 黄金样本与损坏样本。 |

### 21.2 扩展原则

- 不修改已有 `ElementType` 枚举值；新增类型只能追加。
- 存档字段只增不改，保持旧档兼容。
- 修改图元必须走 `beginEdit` / `applyChanges`，保证单步撤销。
- 新增命令需同时考虑：
  1. 命令类实现。
  2. `registerExact` 或 `registerPrefix` 注册。
  3. `CommandCatalog` 元数据与 `help`。
  4. GUI 菜单/工具栏入口。
  5. 自测断言。
  6. 用户手册同步。
- 第三方 GPL 组件（如 LibreDWG 转换器）仅进程外调用，不链入主程序。
- 格式版本号不得虚标；DWG 写不出的版本要明确提示改导 DXF。

### 21.3 验收基线

```bash
./build.sh test
```

建议关注：

- `SketchSelfTest` EXIT=0。
- `-Wall -Wextra` 零警告。
- 黄金 DXF/DWG 样本回归不退化。
- 新增命令 `help <命令>` 可查询。
- 新增 UI 入口与快捷键无冲突。
- 旧 JSON 文档可打开。

---

## 22. 附录

### 附录 A：默认命令别名

| 别名 | 命令 | 别名 | 命令 |
|---|---|---|---|
| `l` | `line` | `c` | `circle` |
| `a` | `arc` | `rec` | `rect` |
| `pl` / `pli` | `polyline` | `e` | `delete` |
| `m` | `move` | `co` / `cp` | `duplicate` |
| `ro` | `rotate` | `tr` | `trim` |
| `ex` | `extend`（注意命令入口可能缺失） | `mi` | `mirror` |
| `o` | `offset` | `f` | `fillet` |
| `sc` | `scale` | `z` | `zoom` |
| `la` / `lay` | `layer` | `b` | `block` |
| `x` | `explode` | `q` | `qselect` |
| `dt` / `tx` / `dtext` | `text` | `mt` / `t` | `mtext` |
| `ed` | `ddedit` | `h` / `hat` | `hatch` |
| `bh` / `bha` | `bhatch` | `pe` / `ped` | `pedit` |
| `ma` / `mat` | `matchprop` | `pu` / `pur` | `purge` |
| `ok` / `ove` | `overkill` | `w` / `wbl` | `wblock` |
| `i` / `ins` | `insert` | `pr` / `pro` / `ch` | `properties` |
| `lt` / `lin` | `linetype` | `lts` | `ltscale` |
| `me` / `mea` | `measure` | `re` / `reg` | `regen` |
| `tol` | `tol` | `fi` / `fli` | `filter` |
| `g` / `grp` | `group` | `laym` | `laymch` |
| `xa` / `xr` | `xref` | `xl` | `xline` |
| `hlp` | `help` | — | — |

### 附录 B：常见问题

| 问题 | 处理 |
|---|---|
| 启动后主界面未显示即退出 | 升级到 v1.1.1 或检查 Qt 运行库与停靠窗口初始化。 |
| 打开 DWG 失败 | 检查是否安装 DWG 转换组件；高版本 DWG 读取依赖外部桥。 |
| 保存 DWG 后客户打不开或版本过低 | DWG 写出最高 R2000；改用 DXF R2013/R2018。 |
| STEP 文件打不开 | STEP 只写不读，请用 JSON/DXF/DWG。 |
| 中文 DXF 乱码 | 确认保存时文件编码与 `$DWGCODEPAGE`，不要用 UTF-8 二次转码。 |
| 命令行输入别名无反应 | 用 `aliaslist` 检查别名；`ex` 可能未映射到可用命令。 |
| 尺寸改了但几何没动 | 检查尺寸驱动总开关与标注类型是否受支持。 |
| 工具条选线型后无变化 | 可能是线型索引边界问题，等待修复或用属性面板设置。 |
| 大图卡顿 | 关闭填充显示、减少样条密度、拆分图纸，或等待大图渲染优化。 |

### 附录 C：术语索引

- 图元、选择集、夹点、块、属性、外部参照、布局、视口、UCS、域、约束、标注样式、线型比例、图层隔离、极轴追踪、对象捕捉追踪、动态输入、宏、脚本、别名、CUIx。

### 附录 D：版本说明

本手册基于当前源码撰写，取代旧版手册中已过期的以下描述：

- 图元数量由 12 类更新为 20 类。
- 新建为空白新建，不弹出图纸设置向导。
- DXF 支持 R12 至 R2018，GUI 保存可选版本。
- DWG 写出最高 R2000，而非完全读写全版本。
- STEP 只写不读。
- 全部不选快捷键为 `Ctrl+Shift+K`。
- 程序含有进程外 LibreDWG 转换器用于高版本 DWG 读取与 R2000 写出桥接。

---

> 维护建议：每次新增命令、改快捷键、改文件格式能力或新增 GUI 工具后，同步更新第 19 章命令索引、第 4 章界面参考、第 20 章待扩展状态与第 21 章源码索引。
