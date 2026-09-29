# TABLE 表格图元（明细表）设计分析

> 2026-09-29 完成实施。本文记录表格图元（ElementType::Table = 16）的需求动机、数据结构、渲染与打印逻辑、与现有图元体系的集成方式、打印/导出接口适配及兼容性影响范围。

---

## 1. 需求动机

此前明细表（如装配明细栏、参数表）只能用「标题栏字段 + 逐条单行文字」拼凑：无法整体选中/移动，内容不对齐，且不参与打印与导出流程。本次将表格实现为**可独立存在的图元（实体对象）**，使其：

- 像普通图元一样被**选中、拖动、复制、删除、撤销/重做**；
- **双击**重新打开表格编辑器；
- **完整参与打印**（PlotRenderer 走 CTB 样式表）与**导出**（DXF/DWG/JSON）。

## 2. 数据结构

### 2.1 枚举与字段（只增不改）

- `ElementType::Table = 16`，追加在 MText 之后——序列化按整数索引，旧档不受影响。
- 字段复用优先（与 Text/MText 同一哲学）：

| 字段 | 复用/新增 | 含义 |
|---|---|---|
| `pts[0]` | 复用 | 表格**左上角锚点**（世界坐标；行向下增长，行 r 顶边 y = pts[0].y − r×rowHeight） |
| `radius` | 复用 | 单元格文字字高 mm（默认 3.5） |
| `textAlign` | 复用 | 单元格文字**水平**对齐（0=左 1=中 2=右；默认居中，明细表惯例） |
| `color/layer/lineStyle/lineWidth` | 复用 | 网格线通用样式 |
| `tableRows / tableCols` | 新增 | 行列数（钳到 [1,200]，反序列化时防损坏数据撑爆渲染） |
| `rowHeight` | 新增 | 统一行高 mm（默认 8） |
| `colWidths` | 新增 | 各列宽 mm；缺项/非正按 30mm 兜底（`tableColWidth(c)`） |
| `tableCells` | 新增 | 单元格文本，**行主序**，size == rows×cols；支持 `%<字段>%` 占位（渲染/打印时经 `resolveFields` 求值） |

- 辅助内联函数：`tableColWidth(c)` / `tableWidthMm()` / `tableCell(r,c)`——渲染、包围盒、导出三处共用同一几何定义（单一真源）。

### 2.2 序列化（向后兼容）

- `elementToJson`：仅 `type == Table` 时写 `tableRows/tableCols/rowHeight/colWidths/tableCells` 五个键。
- `elementFromJson`：缺键回落默认（3×4、行高 8、列宽 30、空单元格）；行列数 `std::clamp` 到 [1,200]。
- 旧版本程序读到含 Table 的档案会得到「未知类型 16」——与所有追加型图元（Xline/Boundary/MText）同一限制；旧**档案文件**（不含 Table）在新版本中零改动兼容。

## 3. 渲染逻辑（画布与打印同源）

### 3.1 画布（GraphicsView::paintTable）

- `translate(锚点) + scale(1/z, −1/z)` 抵消视图 Y 翻转 → 局部坐标 **y 向下为正**，行边界 = r×rowHeight，与「行向下增长」的模型约定一致。
- **网格线**：横线 R+1 条、竖线 C+1 条；列边界由 colWidths 累加；画笔走图元通用样式（`penFor` 同款：cosmetic、线型 `toPenStyle(lineStyle)`、高亮加粗）。
- **单元格文字**：字高 radius、水平对齐 textAlign、垂直一律行内居中、左右各 1mm 内边距；`resolveFields` 求值字段占位（与 Text/MText 同一通路）。

### 3.2 打印（PlotRenderer::paintTable）

- 同一几何，两处差异：线宽走打印样式表 `ResolvedPlotStyle` + `lineWidthScale`（纸面 mm，非 cosmetic）；文字像素按 `scaleDev` 换算。字段占位打印时求值为静态快照。
- `paintOne` 路由新增 `case Table`；STEP 导出计入 `skippedText`（曲线集不含表格，既有语义扩展）。

### 3.3 包围盒与命中

- `rawBounds`：锚点 + (tableWidthMm, rows×rowHeight) 向下展开——选中/框选/平移剔除均正确。
- `shape()`：与 Text/MText 同一矩形命中分支——点表格内部任意位置即可选中。
- `Geometry::distanceToElement`：矩形内 0、矩形外取到矩形最短距离（服务「最近实体」检索）。
- `buildPath`：显式空分支（表格无路径几何，消除 -Wswitch 警告）。

## 4. 与现有图元体系的集成

| 子系统 | 集成方式 | 是否需改动 |
|---|---|---|
| 选中/拖动/删除/撤销 | `shape()` 矩形命中 + SketchModel 通用机制 | 无（自动获益） |
| 移动/复制/旋转/缩放 | `Modify::applyGeom` 泛化平移 pts | 无（锚点随变换走；表格本体保持轴对齐，见 §7） |
| 属性面板/图元列表 | 走 `elementTypeName`/`sizeDesc`（"表格 R×C（W × H mm）/ 已填 N 格"） | 无 |
| 图层 | 网格线随 `layer` 归层，LayerPanel 显隐生效 | 无 |
| 双击编辑 | `mouseDoubleClickEvent` 增加 Table 分支 → 复用 `editTextRequested` 信号，MainWindow 按类型分派（MText 对话框 vs 表格编辑器） | 已改 |
| 打印预览/PDF/打印机 | PlotRenderer paintTable | 已改 |
| JSON 存档 | elementToJson/FromJson + 顶层 writeDrawing 无需改（图元数组自动携带） | 无 |
| DXF/DWG 导出 | 炸开为 LINE 网格 + TEXT 单元格（见 §5） | 已改 |
| 工具/命令行 | `table` 工具（ToolRegistry + TableTool），菜单【绘图 → 文字与表格 → 表格】 | 已改 |

## 5. 打印/导出接口适配

| 通道 | 处理 |
|---|---|
| DXF R12 | 网格炸开为 LINE（R+1 横 + C+1 竖），单元格求值后写 TEXT（40 字高、72/73=1/2 居中、11/21 对齐点）；图层/颜色随 `ent()` 通用属性 |
| DWG R12 | 同几何：`dwgAddLineR` 网格 + `dwgAddTextR` 单元格（R12 TEXT 无对齐组码，文字置于单元格左侧 +1mm，近似观感） |
| STEP | 跳过，计入 `skippedText`（曲线集合不含表格语义） |
| 回读（DXF/DWG 导入） | 导出的表格读回后成为**独立 LINE + TEXT 图元**——与标注炸开导出同一降级策略；表格的可编辑原型仅存于 JSON 档案 |
| JSON | 完整往返（含单元格文本与字段占位原文） |

## 6. UI 与交互

- **创建**：【绘图 → 文字与表格 → 表格】或 `table` 工具 → 画布点击放置左上角 → `TableDialog`。
- **TableDialog**：QTableWidget 逐格编辑 + 行数/列数（变化时重建网格并保留已填内容）+ 行高/字高/对齐（左中右）+ 列宽（逗号分隔 mm）；支持 `%<DATE>%` 等字段占位。
- **编辑**：双击表格 → 同一对话框预填 → 确认后经 `beginEdit + updateElementQuiet` 就地更新表格数据字段（不动 id/图层/颜色，一次 Ctrl+Z 整体撤销）。
- 图标：`table` 矢量 glyph（外框 + 网格 + 首行强调线）。

## 7. 已知限制

- **旋转/镜像**：锚点会正确变换，但表格本体保持轴对齐（不随角度倾斜）——明细表场景（水平放置）足够；如需斜排表格须实现旋转网格渲染，暂缓。
- **列宽拖拽**：列宽经对话框数值输入，未做画布拖拽夹点。
- **合并单元格/逐格对齐**：当前统一行高 + 全表统一水平对齐；合并单元格需扩展单元格描述结构，暂缓。
- **DXF/DWG 回读**：表格降级为线+文字（见 §5）。
- 命令行 `table` 命令未加（菜单/工具入口已覆盖，命令行走 `setActiveTool("table")` 亦可用）。

## 8. 影响范围与兼容性结论

- **旧档案**：零影响（枚举只增、字段缺键回落、无既有键语义变化）。
- **旧功能**：文字/样式/字段、剖面、块、图层、打印均不受影响；`editTextRequested` 信号复用，MainWindow 内按类型分派，对外接口未变。
- **新增文件**：`TableDialog.h/.cpp`、`TableTool.h/.cpp`；**改动文件**：Element.h/.cpp、ElementFactory.h/.cpp、GraphicsView.h/.cpp、PlotRenderer.cpp、Geometry.cpp、ToolRegistry.cpp、ToolManager.cpp、icons.h、MainWindow.cpp、CMakeLists.txt、CadFormats.cpp。
