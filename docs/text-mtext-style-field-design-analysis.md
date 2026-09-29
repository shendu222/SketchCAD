# MTEXT / STYLE / FIELD 文字功能设计分析

> 2026-09-29 完成实施。本文记录三项文字功能（多行文字 MTEXT、文字样式 STYLE、字段 FIELD）的需求动机、结构设计、兼容性策略与关键决策，供后续维护与扩展参考。

---

## 1. 需求动机

此前程序仅有单行 Text 图元，存在三个痛点：

| 痛点 | 表现 | 对应功能 |
|---|---|---|
| 多行注释排版弱 | 技术要求等多行说明只能逐条创建单行 Text，行距/对齐/自动换行均不可控 | **MTEXT** |
| 字体字高无法统一治理 | 修改字体/字高须逐个图元手改，无样式表概念 | **STYLE** |
| 内容无法自动更新 | 日期、文件名、图元数量等变动内容须手动改写 | **FIELD** |

设计总原则：**与现有程序结构和数据格式保持兼容**——旧档案字节级可读、旧图元行为不变、新增字段全部带回落默认值。

---

## 2. MTEXT 多行文字

### 2.1 数据模型

- **枚举追加**：`ElementType::MText = 15`（追加在 `Boundary` 之后，**只增不改**——序列化按整数索引，重排将破坏旧档）。
- **字段复用**：MText 复用 Text 的核心字段——`text`（内容，支持 `\n` 换行 + `%<字段>%` 占位）、`radius`（字高 mm）、`rotation`（角度）、`pts[0]`（锚点），**不另立存储**。
- **新增字段**（`struct Element`，均带默认值，旧档反序列化自动回落）：

  | 字段 | 默认 | 含义 |
  |---|---|---|
  | `widthFactor` | 1.0 | 宽度因子（字宽伸缩） |
  | `textAlign` | 1（中上） | 9 点对齐：index = row×3+col（row: 0上/1中/2下，col: 0左/1中/2右） |
  | `styleName` | 空 | 关联文字样式名（空 = 不关联，用默认） |
  | `lineSpacing` | 1.3 | 行距因子（×字高） |
  | `mtextWidth` | 0.0 | 自动换行宽度 mm（0 = 不换行） |

### 2.2 渲染（画布与打印同源）

- `GraphicsView::paintText` 与 `PlotRenderer::paintText` 统一泛化：经 `resolveTextStyle` 合并样式字体参数（family/bold/italic/stretch/倾斜），经 `resolveFields` 解析占位，经 `wrapText` 折行，按 9 点对齐逐行 `drawText`。
- 行间距始终 = 字高 × `lineSpacing`；导出（DXF/DWG）用同一 1.3 倍展开规则，与画布视觉一致。

### 2.3 交互

- 工具 `mtext`（ToolRegistry 注册 + ToolManager 挂 `MTextTool`）：点击画布指定插入点 → 弹 `MTextDialog` 编辑内容/对齐/字高/宽度因子/样式/行距/换行宽度，并可一键插入字段占位。
- **双击编辑**：`GraphicsView::mouseDoubleClickEvent` 双击 Text/MText 发 `editTextRequested(id)` → `MainWindow::editElementText` 弹同一对话框，确认后经 `updateElementQuiet` **就地更新**文字相关字段（不动 id/图层/颜色/线型；单行 Text 编辑后仍保持 Text 类型）。
- 菜单入口：【绘图 → 文字 → 多行文字】（互斥工具动作，经 `drawGroups_` 与命令行/工具 id 同源）；【绘图 → 文字样式管理器...】打开 STYLE 对话框。工具条第一行横向空间已满（1366px 上限回归约束），MTEXT 只进菜单不占工具条。

---

## 3. STYLE 文字样式管理器

### 3.1 结构设计（照搬 BlockDef 范式）

- `TextStyle` 结构：`name / fontFamily / textHeight / widthFactor / obliquingAngle / bold / italic`。
- `SketchModel` 持有 `vector<TextStyle> textStyles_` 表，构造时预置唯一 **Standard**（family 空 = 系统默认）。
- 完整 API：`textStyleCount / textStyleAt / findTextStyle / hasTextStyle / addTextStyle / renameTextStyle / updateTextStyle / removeTextStyle / applyStyleToElements / textStylesToJson / setTextStylesFromJson`；增删改发 `textStylesChanged()` 信号。
- **撤销支持**：`Memento` 扩展 `textStyles` 成员，样式表变更与图元变更共用同一套快照/恢复机制，一次 Ctrl+Z 同时回滚样式与文字。

### 3.2 引用式关联（关键决策）

图元经 `Element::styleName` **按名字引用**样式（同 Xref/块引用的既有范式），而非拷贝属性：

- `resolveTextStyle(e, model)` 渲染/导出时解析生效值：
  - 字高：样式 `textHeight > 0.1` 优先（AutoCAD 语义），否则用图元自身 `radius`；
  - 宽度因子：样式 × 图元 连乘；
  - family/bold/italic/倾斜：样式有值用样式，否则默认。
- **改样式 → 所有引用它的文字下次重绘自动更新**，实现"统一设置、批量管理"。
- `applyStyleToElements(styleName, ids)` 仅对 Text/MText 生效，走 `beginEdit` 一次撤销。

### 3.3 删除与改名保护

- `removeTextStyle`：保留至少 1 条样式（最后一条拒绝删除）；删除时把引用该样式的图元 `styleName` 置空（回退默认），不产生悬空引用。
- `renameTextStyle`：同步更新所有引用图元的 `styleName`，视图/存档无悬空名。

### 3.4 持久化

- 顶层图纸 JSON 增加 `textStyles` 数组（`writeDrawing`/`readDrawing`，置于图层/块表之后）。旧文件无此字段 → `setTextStylesFromJson` 兜底回落单一 Standard，**旧档零改动兼容**。

### 3.5 管理器对话框

`StyleManagerDialog`：左侧样式列表 + 右侧表单（名称/字体族（QFontDatabase 全族）/字高/宽度因子/倾角/粗体/斜体），按钮：新建/删除/保存/应用到选中/应用到全部。保存时改名走 `renameTextStyle`、参数走 `updateTextStyle`。

---

## 4. FIELD 字段

### 4.1 占位语法

`%<CODE>%`（与 AutoCAD FIELD 语义对齐但极简化）：

| 代码 | 求值 |
|---|---|
| `%<DATE>%` | 当前日期 `yyyy-MM-dd` |
| `%<TIME>%` | 当前时间 `HH:mm:ss` |
| `%<FILENAME>%` | 文件名（未保存 → "未命名"） |
| `%<FILEPATH>%` | 完整路径 |
| `%<COUNT>%` | 图元总数 |

### 4.2 求值时机（关键决策）

字段在**渲染/导出时求值**（`resolveFields(raw, model)`），而非存储时写死：

- 画布重绘、打印出图时自动取最新值 → 打开旧图即显示当日日期；
- 导出 DXF/DWG 时解析为**求值后的静态快照**（下游 CAD 无字段引擎也能正确显示）；
- JSON 存档保留原始占位（可再次求值）；
- 纯文本无占位走快速路径原样返回，**旧文字零开销零行为变化**。

### 4.3 交互

MTextDialog 内"字段"下拉选择即插入到文本光标处（`%<DATE>%` 等原码存入 `text`）。

---

## 5. 导出/导入兼容矩阵

| 通道 | 处理 |
|---|---|
| DXF R12 导出 | Text/MText 统一按行拆为多个 TEXT（组码 40 字高、41 宽度因子、50 旋转、72/73 中心对齐）；字段先求值 |
| DWG R12 导出 | 同上（`dwgAddTextR` 逐行展开，含旋转） |
| DXF/DWG 导入 | `TEXT`/`MTEXT` 实体 → 单行 Text（R12 无原生多行语义，降级可接受；本程序导出的多行在回读后成为多条 Text，与炸开导出行为一致） |
| STEP 导出 | Text/MText 跳过并计入 `skippedText`（曲线集不含文字注记，既有语义扩展） |
| JSON 档案 | MText 专属字段随图元写出；旧档缺失字段回落默认；`textStyles` 数组缺失回落 Standard |

---

## 6. 新增图元类型同步清单执行情况

① Element.h 枚举末尾 + `elementTypeName` + `mtextAlignName` ✔
② Element.cpp `sizeDesc` / `elementToJson` / `elementFromJson`（回落默认）✔
③ struct 加 5 字段 ✔
④ ElementFactory::createMText ✔
⑤ GraphicsView rawBounds / shape / paintOne / paintText ✔
⑥ PlotRenderer paintText ✔
⑦ Geometry distanceToElement ✔
⑧ CMake 注册（TextStyle.cpp / MTextDialog.cpp / StyleManagerDialog.cpp / MTextTool.cpp）✔
⑨ 工具（ToolRegistry + ToolManager + MTextTool）/ 命令 / 菜单 / 对话框 / 图标（mtext/style/field 矢量 glyph）接线 ✔

---

## 7. 已知限制与后续方向

- MTEXT 段落级属性（段前距、项目符号）、行内字体混排未做——当前以"行距 + 自动换行 + 9 点对齐"覆盖技术要求等主流场景。
- FIELD 为运行时求值，无"锁定快照"模式；打印存档建议走 PDF/DXF 导出（已求值）。
- 样式表不随 DWG/DXF 往返（R12 无 STYLE 表读写），仅 JSON 档案持久化。
- 双击编辑对单行 Text 保留原类型；如需把单行转多行，可用 MTEXT 工具新建后删除原对象。
