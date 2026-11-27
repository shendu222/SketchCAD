# SketchCAD v1.3.6 发布说明（Release Notes）

- 发布版本：**v1.3.6**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：AI 面板命令执行与视图回显闭环
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- 裸命令识别不再只依赖 `config/command_catalog.json`；命令注册表为权威来源，
  即使配置尚未更新，已编译进去的 `stdpart` 也能被识别执行。
- 用户在 AI 面板直接输入已注册命令（如 `stdpart M10X100`）时立即按命令行语义执行，
  不再交给模型复述“已成功生成”。
- `stdpart` 命令执行成功后自动 `fitView()`，避免标准件模板坐标落在当前视口之外导致“看不到图”。
- 复现截图两轮交互验证：
  1. 模型纯文本回复 `stdpart T形槽用螺栓 M10X100` → 实际执行
  2. 用户直接输入 `stdpart M10X100` → 实际执行
  共执行 2 次，生成 360 个图元（每次 180）。

## 二、质量校验

- `SketchCAD` 构建通过，`-Wall -Wextra` 零警告。
- 无头验证程序退出码 0：`EXECUTED_COUNT=2 ... ELEMENTS=360`。

---

# SketchCAD v1.3.5 发布说明（Release Notes）

- 发布版本：**v1.3.5**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：AI 返回裸命令行时界面不执行
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- 部分模型不产生 function-calling，也不输出 JSON，而是直接回复一行命令文本
  （如 `stdpart M10X100`）。旧 `NlAssistant` 仅把这种回复展示为助手消息，不执行。
- 新增严格裸命令行解码：仅当整行文本的唯一命令名是已注册命令时才转为
  `runSketchCommand` 工具调用，真正落到命令解释器。
- 增加同轮重复裸命令去重，避免模型连续输出同一命令导致重复落模。
- 系统提示强调 `stdpart <完整品名> <规格>`，不得省略用户给出的品名。
- 验证“绘制一个T形槽用螺栓M10X100”且模型直接回复 `stdpart M10X100`：
  命令被实际执行，生成 180 个图元、1 条 `M10x100` 规格文字，且只执行一次。

## 二、质量校验

- `SketchCAD` 重建通过，`-Wall -Wextra` 零警告。
- 临时无头验证程序退出码 0：`EXECUTED=stdpart M10X100 OK=1 ELEMENTS=180 SPEC_TEXTS=1`。

---

# SketchCAD v1.3.4 发布说明（Release Notes）

- 发布版本：**v1.3.4**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：AI 标准件绘图意图被误拆为通用分步绘图
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- 新增内置 `stdpart` 命令（目录条目：绘图 / 标准件），与菜单【绘图】-【标准件】同源，
  直接按原图模板生成标准件。
- AI 系统提示增加标准件专属消歧：含“螺栓 / T形槽用螺栓 / 螺母 / 垫圈 / 销 / 键 / 轴承 ...”
  与规格（`M10X100` 等）时，必须调用 `stdpart`，禁止拆成“T形槽 + 螺栓”分步绘图。
- 离线 `MockProvider` 同步前置识别标准件意图，输出：
  `runSketchCommand(line="stdpart T形槽用螺栓 M10X100")`。
- 验证输入“绘制一个T形槽用螺栓M10X100”：
  - 工具调用行：`stdpart T形槽用螺栓 M10X100`
  - 执行结果：成功，生成 180 个图元，规格文字 `M10x100` 已写入。

## 二、质量校验

- `SketchCAD` 增量重建通过，`-Wall -Wextra` 零警告。
- `config/command_catalog.json` JSON 校验通过，共 223 条命令，`stdpart` 已注册。
- 无头验证程序（临时目录）退出码 0。

---

# SketchCAD v1.3.3 发布说明（Release Notes）

- 发布版本：**v1.3.3**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：长度参数修改时头部倒角弧漂移
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- `StdPartTemplateStore::render()` 的分区缩放此前按「每个点的 x」判断所属区段。
  M10X100 模板的头部中弧圆心 x=162.38 大于头下边界 headEnd=153.51，
  因此改 L 时圆心被当作光杆点缩放/平移，半径却保持不变，导致浅倒角弧整体左移、
  与顶/底边脱节，表现为深凹弧（L=90 亦复现）。
- 新实现按「整件」判定头部：圆弧只要其左端点（center-r）落在 headEnd 左侧即整体保留；
  头部实线轮廓按 bbox 整体保留；中心线等非实线不参与头部判定。
- 新增回归断言：即使弧心 x>headEnd，只要弧属于头部，改 L 后圆心不变。

## 二、质量校验

- `SketchCAD` / `SketchSelfTest` 重建通过，`-Wall -Wextra` 零警告。
- 全量自测：`通过 3688 / 失败 0`。
- M10X100 改为 M10X90：头部中弧/侧弧圆心保持不变，仅光杆段缩短，端视图位置不变。

---

# SketchCAD v1.3.2 发布说明（Release Notes）

- 发布版本：**v1.3.2**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：六角头螺栓主视头部倒角弧与标准图纸对齐
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- `PartLibrary::BoltGenerator` 中头部 30° 倒角的轴向起点误写为 `xd = Δ·√3`
  （`Δ=(e−s)/2`），比标准 DWG 的 `xd = Δ/√3` 放大 3 倍。
- 由此导致中弧半径 `ρm`、侧弧半径 `ρs` 和圆心全部错误：M10 的中弧由标准 ≈15.27
  变为 ≈5.3，主视头部出现深凹半圆而非标准图的浅倒角三段弧。
- 修正为 `xd = Δ/√3` 后：
  - 中弧半径/圆心 ≈15.28，起角 ≈162.4°
  - 侧弧半径/圆心 ≈4.09
  - 顶面倒角起点 x≈0.71（旧值 2.14）
  与 GB 标准原图模板一致。
- 新增主视头部圆弧回归断言（中弧、侧弧、头部轮廓）。

## 二、质量校验

- `SketchCAD` / `SketchSelfTest` 重建通过，`-Wall -Wextra` 零警告。
- 全量自测：`通过 3687 / 失败 0`。

---

# SketchCAD v1.3.1 发布说明（Release Notes）

- 发布版本：**v1.3.1**
- 发布日期：2026-11-26
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**缺陷修复版**：修正标准件模板修改长度后头部/螺纹/端视图变形
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本修复

- `StdPartTemplateStore::render()` 旧实现把模板内所有点的 x 坐标统一乘以 `L/L0`，
  导致 M10x100 改为 70 时头部、螺纹段、端视图一并压缩，图形失真。
- 新实现按模板几何自动识别「头下 / 螺纹起点 / 杆端 / 端视图」四个边界：
  头部 x 不变、光杆段伸缩、螺纹段整体平移且长度不变、端视图位置固定；
  d 驱动的 y 向缩放与半径缩放保持原语义。
- 新增变长螺栓回归断言（头部、光杆、螺纹长度、总长、端视图位置）。

## 二、质量校验

- `SketchCAD` / `SketchSelfTest` 重建通过，`-Wall -Wextra` 零警告。
- 全量自测：`通过 3681 / 失败 0`。
- 真实 M10x100 模板按 L=70 重算：头下 153.51 → 杆端 223.51（总长 70），螺纹长度保持 26，端视图中心 277.23 不变。

---

# SketchCAD v1.3.0 发布说明（Release Notes）

- 发布版本：**v1.3.0**
- 发布日期：2026-11-25
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**界面版**：标准件操作由停靠面板迁移至【绘图 > 标准件】对话框
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本新增

1. 【绘图 > 标准件...】菜单项：点击弹出非模态【标准件模板库】对话框；原「视图 > 面板 > 标准件模板库」停靠项移除。
2. 对话框分为四个区：实例工作集区（新建/编辑/删除/导入/导出）、模板分类导航区、缩略图预览区、参数配置区与操作按钮区（应用/插入/关闭）。
3. 实例工作集支持多个 `.stdpart.json` 参数实例在会话内维护；导入导出即写读标准件参数实例 JSON，应用把表单修改写回当前实例，插入交给主窗口解析落点并落模。
4. 非模态对话框保持打开状态，画布拾取落点操作可用；关闭后再次打开保留实例工作集与当前选择，实现状态回显。

## 二、质量校验

- `SketchCAD` / `SketchSelfTest` 重建通过，`-Wall -Wextra` 零警告。
- 全量自测：`通过 3676 / 失败 0`。
- 离屏启动主程序稳定驻留；菜单与对话框接线经编译期检查。

---

# SketchCAD v1.2.0 发布说明（Release Notes）

- 发布版本：**v1.2.0**
- 发布日期：2026-11-25
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**功能版**：标准件图库支持参数实例 JSON 落盘/重开渲染与指定位置插入
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本新增

1. **标准件参数实例 JSON（`.stdpart.json`）**：保存标准件类型标识 `typeId`、模板引用
   `templateId`、参数键值 `params`（与 `{{KEY}}` 占位符一一对应）、版本字段
   `formatVersion/schemaVersion=1` 与定位信息 `placement`；重新打开后按模板重放替换与变换，
   直接渲染插入图纸，不依赖几何副本。
2. **参数输入界面扩展**：标准件模板库面板增加插入位置（视图中心 / 画布拾取 / 坐标输入）、
   基点（包围盒中心 / 左下角 / 首个圆心 / 自定义）、旋转角、目标图层，以及
   「保存为参数 JSON」「打开参数 JSON」按钮；实时预览同步插入定位。
3. **定位与对齐**：画布拾取复用 `GraphicsView::pointPickMode`；坐标输入复用
   `SketchModel::parsePointToWorld`（`x,y` / `@dx,dy` / `dist<angle`）；定点后按基点旋转、
   缩放、平移并覆盖图层，整组一次撤销。
4. **文件菜单**：新增「打开标准件参数 JSON...」入口。

## 二、质量校验

- `SketchSelfTest` 全量自测：`通过 3676 / 失败 0`（新增 `t_stdPartInstance` 15 条断言）。
- `SketchCAD` 主程序 `-Wall -Wextra` 零警告构建通过；离屏启动 5 秒稳定驻留无崩溃。
- 版本一致性：CMake `project(VERSION 1.2.0)` → `version.h` / `app.rc` / 安装包名同源。

---

# SketchCAD v1.1.1 发布说明（Release Notes）

- 发布版本：**v1.1.1**
- 发布日期：2026-11-08
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**启动故障修复版（Hotfix）**，功能与 v1.1.0 保持一致
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本解决的问题

**症状**：程序启动后主界面尚未显示即退出，多次运行稳定复现。

**根本原因**（实证定位）：`MainWindow::buildDocks()` 中新增的 AI 助手停靠窗口 `aiDock_`
被加入了统一的 `setFeatures` 遍历列表与 `setVisible(false)` 调用，但其构造语句被放在函数末尾 ——
遍历执行时 `aiDock_` 仍为 `nullptr`，对空指针调用 `QDockWidget::setFeatures()` 触发
`0xc0000005`（访问违例）。崩溃发生在 `MainWindow` 构造期，早于 `main()` 中的 `w.show()`，
因此用户只能看到启动画面一闪而过，主界面永远不出现。

gdb 调用栈（修复前）：

```
#0 QDockWidget::setFeatures(...)  Qt6Widgets.dll
#1 SketchCAD::MainWindow::buildDocks()
#2 SketchCAD::MainWindow::MainWindow(QWidget*)
#3 main()
```

**修复范围**：仅 `src/ui/MainWindow.cpp` 的 `buildDocks()` —— 把 AI 助手面板与停靠窗口的
创建、`addDockWidget()` 前移到遍历之前，删除函数末尾的重复创建块。不改变任何既有功能、
布局参数、菜单项或模型行为。

**修复后预期行为**：启动画面正常展示 → 停留结束 → 主窗口正常显示并保持运行；
AI 助手停靠窗口初始隐藏，可从「视图 > 面板」或「工具 > AI 助手」唤起。

## 二、发行产物（两种安装程序）

| 文件 | 格式 | 说明 |
| --- | --- | --- |
| `SketchCAD-1.1.1-Setup.exe` | Inno Setup 6 引导式安装程序 | 含许可协议页、可选桌面图标与文件关联、开始菜单与卸载入口 |
| `SketchCAD-1.1.1-Setup.msi` | Windows Installer (WiX) 程序包 | 适用于 `msiexec` 静默部署 / 组策略分发，支持版本升级 |

两者打包的是**同一批**经校验的程序文件（主程序、Qt 运行时、图像与 SQL 插件、
`tools/dwg` 转换器、GPLv3 `COPYING`、`LICENSE.txt`、README、启动封面与剖面图案）。
校验值见 `dist/checksums.sha256`。

安装示例：

```
SketchCAD-1.1.1-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
msiexec /i SketchCAD-1.1.1-Setup.msi /qn
```

## 三、质量校验

- 双目标（SketchCAD / SketchSelfTest）`-Wall -Wextra` 零警告构建通过。
- 全量自测：`通过 3165 / 失败 0`（EXIT=0）。
- 主程序实跑：主窗口标题 `SketchCAD`、窗口句柄有效、`Responding = True`，进程稳定驻留。
- `build/SketchCAD.exe` 与 `dist/SketchCAD.exe` 字节一致。
- 版本一致性：CMake `project(VERSION 1.1.1)` → `version.h` / `app.rc` / 安装包名 / 安装器属性同源。

---

## 附录：历史版本 v1.1.0 发布说明

> 以下为 v1.1.0 原始发布说明，保留作为版本历史。

- 发布版本：**v1.1.0**
- 发布日期：2026-10-31
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 概述

SketchCAD 是一款基于 **C++17 / Qt 6** 的二维 CAD 应用，覆盖草图绘制、几何辅助、DXF/DWG
导入导出、参数化图库、标题栏与批量出图、明暗主题等完整制图工作流。本版本（v1.1.0）在既有功能
基础上修复了若干影响使用体验的缺陷，并增强了文字排版与界面可读性。

## 本版本重点修复

- **标题栏填写不再覆盖标签**
  标题栏每个栏目由「灰色标签 + 黑色字段」两个独立图元组成；旧实现两者的锚点都落在单元格中心，
  填写后字段值会压住标签。现改为**「标签带 + 值区」两区分离**布局：标签固定在单元格顶部
  标签带、填写值居中于下方值区，两者永不重叠。写回路径（`fitTitleBlockText`）与批量签名
  （`batchSign`）统一收敛到值区单一入口；自测新增「同单元格标签块与值块零重叠」回归断言。

- **明暗主题工具栏文字可读性**
  亮色主题工具栏文字显式设为纯黑（`#000000`），暗色主题保持原生白字不变；主题切换时重新生成
  工具栏外观，确保明暗两态均正确显示。

- **图标主题化**
  工具栏/菜单图标改用引擎化绘制（`ThemeIconEngine`），墨色在绘制时按主题实时解析——
  暗色下为浅墨、亮色下为深藏青，消除「暗底深字不可见」问题。

## 主要能力（摘要）

- 草图约束求解器、多种图元与尺寸标注、GD&T 标注。
- DXF（R12–R2018）导入导出；DWG 经 LibreDWG 桥接导入、自研 R12 导出。
- 参数化国标图库（如 GB/T 5782 六角头螺栓标注图）。
- 标题栏表格化、批量三件套（拆分 / 排图 / 签名）、填充联动、FIELD 域与 Table 公式列。
- 崩溃不丢稿（AutoSave + mini-dump）、鲁棒性 / Fuzz 防护、商业安装包（Inno Setup）。
- 国际化简档、高 DPI 支持、绘图区域亮 / 暗主题。

## 安装与构建

- **安装包**：运行 `SketchCAD-1.1.0-Setup.exe`（安装包内版本与程序版本同源，均为 1.1.0）。
- **源码构建**：克隆仓库后按 `README` 使用 `CMake + Ninja + MinGW` 构建。

## 质量校验

- 全量自测通过（3157 / 0）。
- 构建产物经 `cmp build↔dist` 字节一致性校验。
- 标签 `v1.1.0` 同时推送 GitHub 与 Gitee，两平台发布内容一致。
