# SketchCAD 对标主流二维 CAD：四维差距分析、改动方案与 AI 编程提示词

> 评估对象：自研二维 CAD **SketchCAD**（`sketch-cad-cpp/`，C++17 / Qt 6.10.1 mingw_64 / MinGW 13.1 / CMake+Ninja）
> 对标基准：**AutoCAD 2025**（行业金标准），兼参 **ZWCAD 2025 / CAXA 电子图板 2023**（国产、GB 友好）、**QCAD / LibreCAD**（开源、Qt 同栈，最接近的技术对照）
> 评估基线：**2026-10-15**（本轮自测 **2469/0 EXIT=0**，双目标零警告）
> 评估方法：命令/工具/图元/标注注册表全量提取 + 关键模块源码核查（`GraphicsView.cpp` 拾取与重建路径、`CadFormats.cpp` 格式链、`Layer.cpp`、约束体系、`UndoDelta.cpp` 等）+ 自测运行验证 + 既有差距报告（`SketchCAD全面检测与主流CAD对比分析报告.md` 2026-10-01 基线）差异比对
> 前序基线已闭项：本报告**不再**重复列为差距，仅列当前仍存在的真实差距

---

## 0. 评估结论速览

| 维度 | 相对主流定位 | 综合评分（对标 AutoCAD=100） |
|---|---|---|
| 功能 | 核心绘图/编辑/标注/图层/块/打印/约束工具链已**基本对齐主流可日用**，国标化（GB/T 系列）甚至**超主流**；差距在参数化求解、DWG 原生闭环、个别高级工具 | 72 |
| 易用性 | 命令行 + 命令面板 + 属性面板 + 停靠面板体系成形，手感接近主流；差距在引导/教程/模板库/图纸集等"软工程化" | 65 |
| 外观 | 功能完整但视觉偏"工程默认态"：无 Ribbon、无完整主题引擎、默认 Qt 风格；高 DPI 已支持 | 55 |
| 性能 | 大图**加载**已达标（10 万图元 182.8ms）；拾取已接空间索引；但**交互编辑仍 O(N) 全场景重建**，撤销已差量化 | 68 |

**一句话结论**：SketchCAD 已跨过"可用"门槛，进入"准主流"区间；剩余差距从「功能有无」转向「体系深度 / 交互性能 / 视觉与引导」，且多数属**架构级**（需重构而非补命令），应作为下一阶段工程主线。

---

## 一、四维系统对比评测

### 1.1 功能维度（核心工具链对照）

| 能力 | SketchCAD 现状 | AutoCAD 2025 | ZWCAD/CAXA | 结论 |
|---|---|---|---|---|
| 二维绘图（直线/圆/弧/多段线/样条/椭圆/矩形/多边形） | 完善（含多模式族 + 徒手） | 完善 | 完善 | 对齐 |
| 编辑（移动/旋转/缩放/镜像/偏移/拉伸/打断/修剪/延伸/倒角/圆角） | 完善 | 完善 | 完善 | 对齐 |
| 夹点编辑 | 5 热操作 + Ctrl 循环 + 双段确认 | 完善（多功能夹点、动态块夹点） | 完善 | 基本对齐 |
| 阵列（矩形/环形/路径） + JOIN + 剪贴板族 | 完善（命令行 + UI） | 完善 | 完善 | 对齐 |
| 图层（ByLayer/冻结/打印/透明度/LTSCALE + LAYISO 族） | 完善（动态语义 + 治理命令） | 完善 | 完善 | 对齐 |
| 尺寸标注（12 类 + 命名样式 + QDIM + GB↔ISO 切换） | 完善 | 完善 | 完善 | 对齐 |
| 形位公差 / 多重引线（MLEADER/FCF） | 完善（GB/T 1182 子集） | 完善 | 完善（CAXA 更强） | 对齐 |
| 字段 FIELD + 表格公式 | 完善（AREA/PERIM/PROP 域 + sum/count 列） | 完善 | 部分 | **超主流** |
| 图块（属性块/动态块）+ BEDIT 在位编辑 + 设计中心 MVP | 完善 | 完善 | 完善 | 对齐 |
| 填充 / 国标剖面（GB/T 4457.5 十九种） | 完善（关联更新 + HATCHEDIT） | 完善 | CAXA 强 | **超主流** |
| 外部参照 Xref（附加/分离/嵌套） | 基本可用（无路径管理器/裁剪） | 完善 | 完善 | 弱项 |
| 约束（15 种 + DOF 诊断） | **部分**（事后检测-修复模型，无方程组求解） | 完善（几何+尺寸联合求解） | 中（部分求解） | **差距** |
| 参数化（改尺寸全图联动 / 系列化设计） | **缺失**（同约束，无求解内核） | 完善（含参数化族） | 部分 | **差距** |
| 文件格式 DXF（R12/R2000，15+ 实体） | 完善（导入导出原生实体） | 完善 | 完善 | 对齐 |
| 文件格式 DWG | **部分**（自研 R12 二进制 + R2000 导出桥[炸开]；R2000 原生导入缺） | 完善（R12~R2024） | 完善 | **差距** |
| 打印（页面/预览/PDF/批量/COM/CTB 样式/布局多视口/幅面驱动） | 完善 | 完善 | 完善 | 对齐 |
| 二次开发（SDK/JS/Python/COM/CUIx/宏/SCR） | 完善（五件齐） | 完善（.NET/LISP 更强） | 完善 | 对齐（缺 LISP 兼容） |
| 国标化（GB/T 14665/4457.5/131/17452/4458.4/14689 + 6 套模板） | **超主流**（开箱即国标） | 通用工具+用户自配 | CAXA 强 | **核心资产** |

### 1.2 易用性维度

| 能力 | SketchCAD | 主流 | 结论 |
|---|---|---|---|
| 命令输入（命令行 + 补全 + DYN 动态输入 + 空格回车重复） | 完善 | 完善 | 对齐 |
| 命令发现（CommandPalette + help 总表 + 冲突自检 + 别名） | 完善 | 完善（AutoCAD 有更深的命令搜索） | 对齐 |
| 属性面板（Ctrl+1 选中即编辑） | 基本可用（字段较少） | 完善（几何/文字/标注全属性） | 弱 |
| 停靠面板体系（图层/约束/设计中心/BOM/图元/日志） | 完善（QDockWidget） | 完善 | 对齐 |
| Ribbon 功能区 | **缺失**（菜单 + 工具条） | 完善（Ribbon 工作空间） | **差距** |
| 首次启动向导 / 新手引导 / 交互式教程 | **缺失** | 完善（Getting Started / 上下文帮助） | **差距** |
| 模板库 / 符号库 / 图框库浏览器（带预览） | 弱（设计中心 MVP，无素材库） | 完善（Tool Palette / Content Explorer） | **差距** |
| 图纸集管理器（Sheet Set） | **缺失** | 完善 | **差距** |
| 高 DPI / 字体跨区域一致 | 完善 | 完善 | 对齐 |

### 1.3 外观维度

| 能力 | SketchCAD | 主流 | 结论 |
|---|---|---|---|
| 主题引擎（浅色/深色/自定义色板） | **缺失**（散点 QSS，局部深色调） | 完善（Light/Dark/图纸对比主题） | **差距** |
| Ribbon / 工具带视觉密度与分组 | 菜单+工具条（密度偏低） | 高信息密度 Ribbon | **差距** |
| 图标体系一致性 | 基本可用（icons.h 矢量图标，但未系统分级） | 完善（统一图标语言） | 弱 |
| 画布视觉（栅格/光标/坐标系/视口边框） | 基本可用 | 完善（自定义十字光标/视口标签） | 弱 |
| 打印/PDF 输出保真 | 完善 | 完善 | 对齐 |

### 1.4 性能维度

| 项 | SketchCAD | 主流 | 结论 |
|---|---|---|---|
| 大图加载（10 万图元） | 182.8ms（裁剪+LOD+异步分块 P0-04） | 亚秒级 | 对齐 |
| 拾取/命中测试 | O(候选)（空间索引 `spatial_.queryPoint`） | O(log) 索引 | 对齐 |
| 视口裁剪（离屏不建场景项） | 完善 | 完善 | 对齐 |
| **交互编辑重建** | **O(N) 全场景 `scene_->clear()`（每次模型变更含选中）** | 增量/脏矩形 + 显示列表 | **差距** |
| 撤销 | O(差量)（UndoDelta 元素级 diff 回放） | O(差量) | 对齐 |
| 渲染帧率（万级平移缩放） | 基本可用；5 万+ 单次大重建可感 | ≥30FPS 流畅 | 弱 |
| 内存占用（大图） | 有防护（bad_alloc 回落索引/停止建项） | 优化 | 基本可用 |

---

## 二、具体差距清单（含代码/架构改动建议）

> 严重程度：**高**=阻塞关键场景；**中**=显著降低效率/覆盖面；**低**=体验差距。
> 每项含「对标差异 → 对用户的实质影响 → 代码/架构改动建议（具体文件与模块）」。

### G-A 约束求解器与参数化（高，架构级）
- **对标差异**：AutoCAD/中望为「几何 + 尺寸联合方程组」求解，含自由度(DOF)分析、欠/过约束诊断；SketchCAD 仍是「改后检测-修复-传播」的事后模型，无迭代求解器、无 DOF。
- **实质影响**：「改一个尺寸全图联动」「系列化/变型设计」「机构运动仿真」不可用——这是与主流"参数化 CAD"差距最大的单点。
- **代码/架构改动**：
  1. 抽象层 `src/core/ConstraintSolver.h` 定义 `IConstraintSolver`（addConstraint / solve / getDOF / diagnose），`ConstraintSolverFacade` 路由（现有事后模型作回落）；
  2. 集成 **solvespace（BSD-2，白名单内）** 作几何内核后端：新增 `src/core/solvers/solvespace_bridge.{h,cpp}`，把 `ConstraintRecord`（含新增的 Dimensional/driverId/paramRef）映射为求解器约束；
  3. `SketchModel` 解算钩子：尺寸标注值变更 → `solve()` → 回写受驱几何（替换现 `solveAfterChange` 事后修复）；
  4. 约束栏显示 DOF 计数与欠/过约束诊断文案（接 `Diagnostics`）；
  5. 验收：20 个基准草图（欠/过/奇异构型）求解正确 + 自测 ≥15 断言。
  - ⚠️ 前置：solvespace 许可评审通过；数值容差统一 `geo::almostEqual`，禁止求解器魔数散布。

### G-B DWG R2000+ 原生闭环（高，合规前置）
- **对标差异**：AutoCAD/中望支持 R12~R2024 原生读写（块/标注/填充**不炸开**）；SketchCAD 仅自研 R12 二进制 + R2000 **导出**走外部桥 `DwgBridge`（仍炸开），**R2000 原生导入缺**。
- **实质影响**：设计院交付默认 DWG（近 25 年版本）打不开或打开后丢块/标注/填充——进入企业现场的最后一道硬门槛。
- **代码/架构改动**：
  1. 前置合规评审（ODA 商业授权 vs 自研 R12+DXF 双轨 vs 外部桥）；
  2. 若走桥接：`DwgBridge` 补齐 **R2000 导入**（解析 AC1015 段，原生实体不炸开）；`CadFormats.cpp` 增加版本探测路由（`fromDwg` 按版本分支，与自研 R12 并存）；
  3. 黄金样本 round-trip 差分（`tests/golden/` 已就位，复用 t_dwg 框架）；
  4. 验收：R2000 样本读写 round-trip + AutoCAD 交叉打开验证。
  - ⚠️ 严禁逆向工程产物入库；GPL 库（如 LibreDWG）禁止引入闭源工程；评审未过则**不启动**，维持 R12+DXF 双轨。

### G-C 场景增量更新（高，交互性能架构级）
- **对标差异**：主流为增量/脏矩形 + 视图驻留显示列表；SketchCAD `GraphicsView::rebuild()` 仍 `scene_->clear()` 全量重建，且每次模型变更（**含仅选中变化**）触发。
- **实质影响**：5 万+ 图元拖拽/批量选中/属性刷仍 O(N) 卡顿；加载快但编辑慢，用户体验割裂。
- **代码/架构改动**：
  1. `SketchModel` 发出**细粒度变更信号** `changed(const QSet<int>& ids, ChangeType)`（add/update/remove），替代"任何变更都 rebuild"；
  2. `GraphicsView` 新增 `applyDelta()`：按变更集增/改/删 `items_` 映射项（已有 `items_` + `ElementItem`），选中变化仅改 `item->highlight` **不触发 rebuild**；
  3. `rebuild()` 保留为兜底（层表/单位/界限等结构性变更、索引失效、bad_alloc 时调用）；
  4. 性能基准脚本（1万/5万/10万 拖拽+选中计时）；
  5. 验收：单图元改属性不重建全场景；10 万图元拖拽 ≥30FPS；特性开关双跑对照（索引/全量命中集一致）。

### G-D UI 现代化与主题引擎（中，外观维度）
- **对标差异**：主流有 Ribbon + 完整 Light/Dark 主题 + 统一图标语言；SketchCAD 为菜单+工具条 + 散点 QSS（仅日志面板深色）。
- **实质影响**：视觉专业度与主流落差明显，深色模式缺失影响长时间作业舒适度，降低"国产替代"第一印象。
- **代码/架构改动**：
  1. 新增 `ThemeManager`（`src/ui/`）：design tokens + QSS 主题表（浅/深/对比），挂 `AppSettings` 记录主题；
  2. `MainWindow` 布局框架支持 **Ribbon / Classic 双模式切换**（参考现有 `CuiManager` 工具栏 XML 化，Ribbon 为其超集布局）；
  3. 收口现有散点 `setStyleSheet` 到单一 `:/themes/*.qss` 资源（icons.h 矢量图标已具备，按大小/语义分级）；
  4. 验收：浅/深主题切换无残留样式；高 DPI 下 Ribbon 不溢出；零警告。

### G-E 引导、教程与 Help 深化（中，易用性维度）
- **对标差异**：主流有首次启动向导、交互式新手引导、上下文图文帮助、模板库浏览器；SketchCAD 仅有 `help <cmd>` 文本 + 命令面板。
- **实质影响**：新用户上手曲线陡，企业培训成本高，阻碍推广。
- **代码/架构改动**：
  1. `StartupWizard` 首次启动（选模板/幅面/单位 + 国标默认）；
  2. 交互式引导（高亮命令 + 分步动画）接 `CommandPalette`；
  3. `help` 增强为带示例/图示上下文帮助（现有 `CommandCatalog` 已有 usage/example 数据源，渲染为富文本/弹层）；
  4. 模板库浏览器（接现有 6 套参数化模板 + `DesignDock`）。

### G-F 生态兼容：AutoLISP / .NET（中，远期）
- **对标差异**：AutoCAD 生态核心是 AutoLISP 与 .NET API；SketchCAD 有 JS/Python/C ABI/COM/CUIx 但**无 LISP 兼容**。
- **实质影响**：海量历史 LISP 脚本/二次开发无法迁移，企业存量自动化资产失效。
- **代码/架构改动**：新增 LISP 解释器沙箱（或 AutoLISP 兼容层）接 `ScriptApi`；.NET 后端绑定现有 SDK C ABI。成本高、风险大，列为远期（P3）。

### G-G 设计中心 / 符号库 / 素材库深度（中低）
- **对标差异**：主流有 Tool Palette / Content Explorer（带缩略图预览 + 拖拽）；SketchCAD `DesignDock` 仅列块表。
- **代码/架构改动**：`DesignDock` 扩展为素材浏览器（国标图符/螺栓/轴承 GB 件 + 图框库），缩略图缓存 + 拖拽插入；与 `buildSheetFrame`/`buildInsertedTitleBlock` 模板链路打通。

### G-H 图纸集 / 外部参照管理（中低）
- **对标差异**：主流有 Sheet Set Manager + Xref 路径管理器 + xref clip；SketchCAD 仅 Xref 附加/分离（嵌套不动点 guard）。
- **代码/架构改动**：新增 `SheetSetManager`（跨多文件图纸集，JSON 工程描述）；Xref 路径管理器 UI + `xrefClip` 裁剪（接 `Clipping` 边界引擎）。

### G-I 编辑细节补强（低）
- **MEASURE 定距等分**：`Commands.cpp` 补 `measure`（复用 `divide` 等分内核改间距语义）；
- **AUDIT 强校验**：`Diagnostics` 增加档案一致性校验 + **错误修复回执**（当前 AUDIT 弱）；
- **视口级图层冻结（vpfreeze）**：`PlotViewport` 增加 per-viewport 冻结集（现有 `frozenLayers` 已支持，需 UI 与打印链消费）。

---

## 三、改动建议清单（按优先级）

| 优先级 | 项 | 类型 | 完成标志 |
|---|---|---|---|
| **P0（本周期，阻塞企业落地）** | G-B DWG R2000+ 原生闭环 | 合规前置+桥接 | 设计院 DWG 可开可编辑（块/标注/填充不炸开） |
| **P0（本周期，交互性能）** | G-C 场景增量更新 | 架构重构 | 10 万图元拖拽/选中 ≥30FPS，单图元改属性不重建全场景 |
| **P1（下一周期，参数化核心）** | G-A 约束求解器/参数化 | 内核集成 | 改尺寸全图联动 + DOF 诊断，20 基准草图全过 |
| **P1（视觉与引导）** | G-D 主题引擎 + Ribbon | UI 重构 | 浅/深主题 + Ribbon/Classic 双模式 |
| **P1（易用性）** | G-E 引导/教程/Help | 新增模块 | 首次向导 + 上下文帮助 + 模板库 |
| **P2（生态/素材）** | G-G 符号库 + G-H 图纸集/Xref 管理 | 新增模块 | 素材浏览器 + 图纸集管理 |
| **P2（细节补强）** | G-I MEASURE/AUDIT/vpfreeze | 命令补全 | 三命令可用 + 自测 |
| **P3（远期）** | G-F AutoLISP/.NET 兼容 | 生态 | 历史脚本可迁移（前置评审） |

**两条合并底线（全程）**：① `ElementType` 枚举只增不改、存档字段语义不可重排；② 每项改动以「自测全绿 + `-Wall -Wextra` 零警告 + `SketchSelfTest.exe` EXIT=0」为合并门槛（当前基线 2469/0）。

---

## 四、可直接使用的 AI 编程提示词（含导出 Markdown 指令）

> 使用约定：以下每条提示词**自成一体**，可直接粘贴给 AI 编程工具（WorkBuddy / Cursor / Claude Code 等）。
> 每条末的「📄 交付物导出」段为**强制指令**：要求 AI 把完整思考过程、评测结论与改动方案**导出为 Markdown 文档**（落盘到 `docs/` 并以 `present_files` 呈现）。
> 建议流程：先跑 **M0 元提示词**生成单点设计文档，再按 P0→P1 顺序派发 G-* 提示词。

### M0 · 通用元提示词（任何一项差距改造前必跑）

```text
你是一名资深 C++/Qt CAD 软件架构师，正在维护自研二维 CAD「SketchCAD」
（C++17 / Qt 6.10.1 / MinGW 13.1 / CMake+Ninja，双目标 SketchCAD + SketchSelfTest）。

任务目标：<在此填写差距编号与名称，例如 G-C 场景增量更新>。

请严格按下列四步产出，并把全过程落盘为 Markdown 文档：

① 思考过程（Thinking）：先复述当前代码实况（用 grep/Read 核实 src/ 下相关文件与函数，
      给出关键文件:行号证据），再推导改造方案，列出至少 2 个候选方案并比较取舍，
      说明为何选最终方案、放弃了什么、引入什么新风险。
② 评测结论（Assessment）：用一句话给出「改造前 vs 改造后」的能力对照，
      并标注对标的 AutoCAD 2025 行为。
③ 改动方案（Plan）：精确到「文件 → 函数/类 → 改什么 → 不改什么」，
      含新增 API 签名、信号/槽、序列化字段（牢记 ElementType 只增不改、存档字段不重排）、
      自测断言清单（pytest 风格的 CHECK 列表）。
④ 验证（Verify）：构建命令（cmake --build build --target SketchCAD SketchSelfTest -j 1，
      PATH 含 Qt/mingw/mingw_64/bin）+ 自测命令（PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"
      ./build/SketchSelfTest.exe），要求双目标 -Wall -Wextra 零警告、EXIT=0。

📄 交付物导出（强制）：将【思考过程 + 评测结论 + 改动方案 + 验证】四节完整写入
    Markdown 文件 `docs/<gap-id>-design-analysis.md`（kebab 命名，例如 g-c-incremental-rebuild-design-analysis.md），
    并通过 present_files 呈现该文件。文档开头用 blockquote 标注「任务/状态/日期/关联文档」。
    禁止只给口头答复——必须产出可追溯的 Markdown 设计文档。
```

### P0-1 · G-B DWG R2000+ 原生闭环

```text
依据 M0 元提示词的四步框架执行，但任务固定为「G-B DWG R2000+ 原生闭环」。

重点约束：
- 先完成合规评审小节（ODA 商业授权 / 自研 R12+DXF 双轨 / 外部桥 DwgBridge 三选一），
  在文档中明确写出选型与理由；若评审结论为「不引入第三方」，则方案收敛为
  「自研 AC1015 段解析 + 原生实体不炸开」，禁止任何逆向工程产物。
- 改动落点：src/core/CadFormats.cpp 的 fromDwg 版本路由、src/com 或 src/core 的 DwgBridge、
  tests/golden 的 R2000 黄金样本（复用 t_dwg / t_formatsGolden 框架）。
- 验收包含：R2000 样本读写 round-trip 差分 ≤1e-6（图纸 mm）、AutoCAD 交叉打开人工核。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-b-dwg-r2000-design-analysis.md` 并 present_files 呈现。
```

### P0-2 · G-C 场景增量更新

```text
依据 M0 元提示词的四步框架执行，任务固定为「G-C 场景增量更新（消除 O(N) 全场景重建）」。

重点约束：
- 现状证据：GraphicsView::rebuild()（src/ui/GraphicsView.cpp:1066）仍 scene_->clear() 全量重建；
  hitTestAll（:2298）已接 spatial_ 索引；items_ 映射已存在。请勿破坏索引/裁剪既有收益。
- 方案核心：SketchModel 增加细粒度 changed(ids, ChangeType) 信号；GraphicsView::applyDelta()
  按集增删改 ElementItem；选中变化仅改 item->highlight，绝不调 rebuild()；rebuild() 仅作结构性兜底。
- 回归护栏：保留"特性开关双跑"——开/关增量路径的拾取/渲染结果集必须一致（随机 1e4 次差分）。
- 自测：新增 t_incrementalRender（≥10 断言：单图元改属性不重建、批量 add 增量建项、remove 删项、undo 回放终态全等）。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-c-incremental-rebuild-design-analysis.md` 并 present_files 呈现。
```

### P1-1 · G-A 约束求解器与参数化

```text
依据 M0 元提示词的四步框架执行，任务固定为「G-A 约束求解器与参数化（集成 solvespace）」。

重点约束：
- 前置：先写许可评审小节（solvespace 为 BSD-2，确认在白名单内）；否决自研求解器。
- 架构：IConstraintSolver 抽象（src/core/ConstraintSolver.h）+ solvespace_bridge
  （src/core/solvers/）+ ConstraintSolverFacade 路由（现有事后修复模型作回落）。
- 把 ConstraintRecord（含 Dimensional/driverId/paramRef）映射为求解器约束；
  SketchModel 尺寸值变更钩子改走 solve() 回写（替换 solveAfterChange 事后修复）。
- 约束栏（ConstraintBar）显示 DOF 计数与欠/过约束诊断。
- 自测：20 个基准草图（欠/过/奇异）+ t_constraintSolver（≥15 断言）；既有约束回归全绿。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-a-constraint-solver-design-analysis.md` 并 present_files 呈现。
```

### P1-2 · G-D 主题引擎与 Ribbon

```text
依据 M0 元提示词的四步框架执行，任务固定为「G-D UI 现代化：主题引擎 + Ribbon/Classic 双模式」。

重点约束：
- 新增 ThemeManager（src/ui/）：design tokens + QSS 主题表（浅/深/对比），挂 AppSettings 记录主题；
  收口现有散点 setStyleSheet 到单一 :/themes/*.qss 资源（icons.h 矢量图标按大小/语义分级复用）。
- MainWindow 布局框架支持 Ribbon / Classic 切换（Ribbon 为现有 CuiManager 工具栏 XML 的超集布局）。
- 不得破坏现有菜单/工具条/停靠面板功能；高 DPI 下 Ribbon 不溢出；字体跨区域一致（&F 简写风格）。
- 自测：t_theme（≥6 断言：主题切换无残留样式、QSS 加载失败回落默认、Ribbon/Classic 切换后功能可达）。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-d-theme-ribbon-design-analysis.md` 并 present_files 呈现。
```

### P1-3 · G-E 引导与 Help 深化

```text
依据 M0 元提示词的四步框架执行，任务固定为「G-E 首次启动向导 + 交互式引导 + 上下文 Help + 模板库」。

重点约束：
- 复用既有资产：CommandCatalog（usage/example 数据源）、CommandPalette、DesignDock、6 套参数化模板。
- 新增 StartupWizard（选模板/幅面/单位，国标默认）、交互式引导（高亮命令+分步动画）、
  help 富文本弹层（接 CommandCatalog 数据）、模板库浏览器。
- 不引入新依赖；所有新增 UI 走 Qt Widgets + 现有 QSS 体系。
- 自测：t_onboarding（≥8 断言：向导默认值、引导步骤推进、help 弹层内容非空、模板插入成功）。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-e-onboarding-help-design-analysis.md` 并 present_files 呈现。
```

### P2 · G-G/G-H 符号库与图纸集（合并提示词）

```text
依据 M0 元提示词的四步框架执行，任务为「G-G 符号/素材库浏览器」与「G-H 图纸集管理器 + Xref 路径管理」二选一或合并。

重点约束：
- G-G：扩展 DesignDock 为素材浏览器（国标图符/螺栓/轴承 GB 件 + 图框库），缩略图缓存 + 拖拽插入，
  复用 buildSheetFrame / buildInsertedTitleBlock 模板链路。
- G-H：SheetSetManager（JSON 工程描述跨多文件图纸集）；Xref 路径管理器 UI + xrefClip 裁剪（接 Clipping 边界引擎）。
- 存档字段只增不改；跨文件引用路径用相对路径（Xref 既有相对路径解析基准）。
- 自测：t_symbollib（≥6 断言）/ t_sheetset（≥8 断言），含空库/损坏路径/跨文件引用回落。

📄 交付物导出（强制）：将完整思考/结论/方案/验证写入
    `docs/g-gh-library-sheetset-design-analysis.md` 并 present_files 呈现。
```

---

## 五、结语

SketchCAD 在**功能覆盖**上已逼近主流二维 CAD 的「可日用」水位，并在**国标化**上形成差异化优势；当前真正的短板已从「功能有无」迁移到三类**架构级课题**——参数化求解内核（G-A）、DWG 原生闭环（G-B）、交互场景增量更新（G-C），以及两类**体验级课题**——视觉现代化（G-D）与新手引导/生态兼容（G-E/G-F）。

建议实施顺序：**P0（G-B + G-C）→ P1（G-A + G-D + G-E）→ P2（G-G/G-H）→ P3（G-F）**，每步以 M0 元提示词驱动、以单点 Markdown 设计文档可追溯、以 2469+ 自测零失败为合并门槛。

---

*报告生成：2026-10-15 · 全部结论以当日工作副本（自测 2469/0）代码实况为准，关键行号可在 src/ 复核 · 前序基线：`SketchCAD全面检测与主流CAD对比分析报告.md`（2026-10-01）*
