# 尺寸与符号标注：GB/T 4458 ↔ ISO 129 标准切换

## 1. 需求与设计目标
- 程序支持对尺寸/符号标注在 **GB/T 4458 系列（中国机械制图国标）** 与 **ISO 129 系列（国际技术产品文档标准）** 之间切换。
- 用户可在界面选择标注标准；系统根据所选标准**自动采用对应的箭头形式、文字方向、公差表达规则**。
- 未明确选择时**默认采用国内 GB/T 4458**。
- 两种标准下的标注结果均符合各自规范，可正确渲染附件工程图样中的内容。

## 2. 两种标准的关键差异（实现依据）

| 维度 | GB/T 4458（默认） | ISO 129-1:2018 |
|------|------------------|----------------|
| 尺寸线终端（箭头） | 实心闭合三角箭头 | 闭合（可空心）；亦允许开口/斜线/圆点等形式 |
| 线性尺寸文字方向 | 方法 1：数字按可读方向，近竖直方向夹取为水平（±90°），避免倒置 | 文字与尺寸线对齐（ISO 可读方向），不强制水平 |
| 角度数字 | 一律水平 | 同左（可对齐） |
| 公差注写 | 默认不注公差；常用极限偏差堆叠 | 默认极限偏差堆叠（上/下偏差）；亦支持极限尺寸、配合代号 |
| 直径/半径符号 | ⌀ / R | ∅ / R（含球面 S∅、SR） |
| 弧长符号 | — | ⌒（弧长大写前缀） |

> 实现不排斥其他箭头/公差形式，均通过枚举与配置可扩展。

## 3. 架构与数据流
- **核心层（与渲染解耦）**：`src/core/DimStyle.*` 是「几何生成器」，把一个 `Dimension` 图元展开为尺寸界线、尺寸线、箭头、数字位置/朝向、公差文本，全部以「图纸毫米」为单位（符合制图规范，可被自测与 DXF 导出复用）。
- **标准配置**：`src/core/DimensionStandard.h` 定义枚举与 `DimensionStandardConfig`：
  - `gbtDefault()`：实心闭合箭头 + 对齐可读（方法 1）+ 无公差。
  - `isoDefault()`：空心闭合箭头 + ISO 对齐 + 极限偏差。
  - `forStandard(s)`：按图元/全局当前标准取默认配置。
- **渲染层**：`GraphicsView::paintDimension` 以 `DimensionStandardConfig::forStandard(e.standard)` 取样式，按 `ArrowForm` 分支绘制实心/空心/开口/斜线/圆点箭头，坐标标注绘制原点；文字旋转经视图 Y 轴翻转后再取负角。

## 4. 支持的标注类型
线性（自动判水平/竖直）、对齐、水平、竖直、**基线**、**连续**、角度、半径 R、直径 ⌀、**弧长 ⌒**、**引线/指引线**、**坐标标注**。后 5 种为本次新增几何生成器（`buildBaseline/buildContinue/buildArcLength/buildLeader/buildOrdinate`）。

### 4.1 新增 5 种标注的入口（UI / 命令行）
| 工具 ID | 菜单项 | 交互方式 | 命令行 |
|---------|--------|----------|--------|
| `dim_baseline` | 基线标注 | 选直线 → 移动预览 → 点击确认 | `dim baseline x1,y1 x2,y2` |
| `dim_continue` | 连续标注 | 同上（尺寸线首尾相接） | `dim continue x1,y1 x2,y2` |
| `dim_arc` | 弧长标注 | 选圆/圆弧 → 点击方位（⌒ 前缀） | `dim arc <圆id> [起角 终角]` |
| `dim_leader` | 引线标注 | 连续点击折线点，末点为文字落点 | `dim leader 起点 折点 [文本点]` |
| `dim_ordinate` | 坐标标注 | 两点：被测点 + 基准点（X/Y 坐标） | `dim ordinate 被测点 基准点 [0/90]` |

- 三类入口（菜单 / 工具条 / 命令行）共用同一套 `ToolRegistry` 工具定义与 `DimensionToolHandler`，菜单项通过 `dimActs_` 自动挂到「草图绘制 > 尺寸标注」子菜单。
- 右键在「圆/圆弧待确认」状态下仍只切换直径↔半径，不会误伤弧长标注（已在 `handleRightPress` 加守卫）。

## 5. 公差表达规则与编辑入口
`formatTolerance(e, nominal, st)` 按 `ToleranceStyle` 分支生成公差文本，取值优先级：
1. **显式传入的 `Style::stdConf`**（如属性面板/命令行指定）——用于「单标注覆盖」；
2. 否则回退 `DimensionStandardConfig::forStandard(e.standard)` 的默认规则（GB/T → 不注公差；ISO → 极限偏差堆叠）。

| ToleranceStyle | 输出示例 | 说明 |
|----------------|----------|------|
| `None` | （空） | GB/T 默认：不注公差 |
| `Symmetric` | `±0.05` | 对称公差 |
| `DeviationStacked` | `(+0.10/-0.05)` | ISO 默认：上下偏差堆叠 |
| `LimitSize` | `上极限 … 下极限 …` | 极限尺寸 |
| `FitCode` | ` H7/g6` | 配合代号（来自 `Element::fitCode`） |

**属性面板编辑**：选中任一标注后，`PropertyPanel` 追加 4 个字段——上偏差 / 下偏差 / 配合代号 / 该标注所用标准（跟随全局 / GB/T / ISO）。修改后立即调用 `applyTolerance()` 写回 `Element` 并重绘，无需重新创建标注。

## 6. 标准切换的交互路径
- **全局默认标准**：`SketchModel` 维护 `dimensionStandard_`（默认 `GBT`）。
- **UI 入口**：主窗口工具条新增「标注标准」下拉框（GB/T 4458（国标）/ ISO 129（国际））。切换即调用 `model->setDimensionStandard(...)`。
- **新建标注**：所有创建入口（`handleDimension`/`finalizeDimension`/拖拽预览/`DimCommand`/`smartDimension`）在构造时写 `e.standard = model->dimensionStandard()`，因此**新建标注自动跟随当前标准**。
- **切换重绘**：`setDimensionStandard` 会把**已有标注全部重戳为新标准**并广播 `modelChanged`，画面实时更新——这是「用户在界面选择标准 → 系统整体改变标注样式」的直接落地。
- **图元级持久化**：每个 `Dimension` 自带 `standard` 字段，存盘（JSON）后读取回放，旧档缺字段回退 GB/T。

## 7. 验证
- 全量构建通过（含 `SketchCAD.exe`），无新增编译告警。
- `SketchSelfTest` **415 通过 / 0 失败**，其中本次新增：
  - §15b：标准默认配置、GB/T 无公差 vs ISO 极限偏差注写、近竖直尺寸文字方向（GB/T 夹取 90° / ISO 对齐非 90°）、5 种新增类型 build 有效。
  - §15c：模型切换标准后已有标注被重戳为 ISO；ISO 公差含上/下偏差；配合代号 `H7/g6` 写入 `tolLabel`。
  - §15d：**5 种新增标注的工具注册（`tryDimensionType`/`isDimensionTool`）+ 命令行创建闭环**（dtype 正确、实测值正确、弧长带 ⌒ 前缀且实测 = 四分之一圆周、新建标注继承模型当前标准）。
- 其余 UI 测试全绿：`ViewPanTest` / `ViewRenderTest` / `ViewRenderTest2` / `SelectMenuTest` / `SelectWindowTest` 全部 PASS。
- `geo::measureDimension` 补齐基线/连续/弧长/引线/坐标分支。

## 8. 已知边界
- 角度/弧长/引线/坐标的「数值驱动几何」（`driveDimension`）目前仅线性/水平/竖直/半径/直径支持，其余类型返回 false（安全跳过，不影响标注与渲染）。
- 属性面板的公差编辑写的是 `Element::tolUpper/tolLower/fitCode`，实际注写成什么样仍由该标注的 `ToleranceStyle` 决定——例如 GB/T 默认「不注公差」，此时填了上/下偏差也不会显示；把该标注切到 ISO（或全局切到 ISO）即可看到极限偏差注写。
- 基线/连续标注目前按「单段」生成几何，尚未实现「自动串接上一组尺寸」的连续拾取；如需成组基线，可连续拾取同一直线上的线段分别标注。
