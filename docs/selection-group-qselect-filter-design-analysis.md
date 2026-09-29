# GROUP 编组 / QSELECT 快速选择 / FILTER 过滤器 设计分析

> 2026-09-29 完成实施。本文记录编组、快速选择、过滤器三项选择功能的设计与实施，重点说明大图纸场景下的性能决策与选择集一致性保证。

---

## 1. 需求与总体设计

| 能力 | 需求 | 实现载体 |
|---|---|---|
| GROUP 编组 | 创建/命名编组、加入/移除对象、整体选中 | `Element::groupName` 引用式归属 + GroupManagerDialog |
| QSELECT 快速选择 | 按类型/属性筛选，结果追加/替换/排除选择集 | `QSelect` 核心单遍 O(n) 扫描 + QSelectDialog |
| FILTER 过滤器 | 多条件组合（AND/OR）、保存复用 | `FilterDef`（QSELECT 条件的超集）+ FilterDialog + 随图持久化 |

三者的**统一收敛点**：`SketchModel::applySelectionDelta(hits, action)`——替换/追加/排除一次落地、全程只发一次 `selectionChanged`，与点击选择、框选、图层选择共用同一选择集状态（`selectedIds_`/`selectedId_`），属性面板、约束、标注等下游零改动。

## 2. GROUP 编组

### 2.1 引用式归属（关键决策）

编组 = **`groupName` 相同的图元集合**，不设独立编组表（与 REGION 命名分组、STYLE 引用式同一范式）：

- 创建/解散编组只改写各图元的 `groupName` 字段；
- `beginEdit + 静默写` 包裹 → Memento 快照整个图元向量，**编组变更天然可整组撤销**；
- 图元删除/复制后编组关系自动跟随，**无 id 悬空问题**（独立 id 表方案必须做成员清理与复制继承，引用式零成本解决）；
- 允许"重名创建" = 向既有编组添加成员，与「添加到编组」语义统一。

### 2.2 API（SketchModel）

`groupNames / elementCountInGroup / elementsInGroup / createGroup / addToGroup / removeFromGroup / ungroup / renameGroup / selectGroup`（整体选中 = `setSelection(members)`，一次信号）。

### 2.3 持久化

`groupName` 随图元 JSON 走（**空串不写盘**，旧档案缺键回落"未编组"）——编组关系随图完整往返，块定义内图元同样支持。

### 2.4 UI

GroupManagerDialog：编组列表（含成员数）+ 用选中对象创建 / 添加选中 / 移除选中 / 重命名 / 解散 / 整体选中（双击编组名 = 整体选中并关闭回画布）。

## 3. QSELECT 快速选择

### 3.1 条件模型

- **范围**：整个图形 / 当前选择集内；
- **对象类型**：不限 + 全部 ElementType（含 Table，随枚举自动扩展）；
- **特性**：图层 / 颜色(#rrggbb) / 线型 / 线宽(实际生效值) / 文字内容 / 名称 / 编组 / 文字样式 / 可见性；
- **运算符**：等于 / 不等于 / 包含 / 开头为（不区分大小写）；
- **候选值**：`uniquePropValues` 从图形内现存值一次性枚举去重，下拉直选不手输。

### 3.2 与选择集的交互

替换 / 追加 / 排除 三种方式（`QSelectAction`），经 `applySelectionDelta` 落地：
- 追加 = 现有 `addToSelection(vector)`（去重、一次信号）；
- 排除 = 选择集减去命中集（新实现，一次信号）；
- 类型 + 特性双条件按 AND 组合进同一 `FilterDef` 管线——**QSELECT 是 FILTER 的单条特例**，无重复实现。

## 4. FILTER 过滤器

- **多条件组合**：条件行（特性/运算符/值）任意增删，组合方式 全部满足(AND) / 任一满足(OR)，作用于整个图形；
- **保存/复用**：按名称保存到 `SketchModel::savedFilters_`（重名覆盖），列表点选即回填条件区；
- **持久化**：顶层图纸 JSON `savedFilters` 数组（`FilterDef::toJson/fromJson`，条件以中文名为持久化键，只增不改）；**不入撤销栈**——过滤器是检索配置而非图纸内容，与打印设置同级语义（旧档缺键回落空列表）。

## 5. 大图纸性能（本需求重点）

| 决策 | 说明 |
|---|---|
| **单遍 O(n) 扫描** | `qselect` 对 `elements()` 向量顺序单遍；无逐 id `element(id)` 反查（id→元素在 id 非连续时退化为 O(n) 线性，会变 O(n²)） |
| **范围筛选预建索引** | "当前选择集内"模式先把范围 id 装入 `QSet`（k 次哈希），扫描期 O(1) 判定——避免 O(n·k) 嵌套线性查找 |
| **单次信号** | 结果经 `applySelectionDelta` 一次落地；若逐 id 调 `addToSelection(int)`，大图会触发 O(n) 次 `selectionChanged` → 视图/面板 O(n) 次全量刷新，整体 O(n²) 且 UI 冻结 |
| **短路求值** | AND 模式任一条件不满足立即淘汰；OR 任一满足即命中 |
| **属性求值单一真源** | `evalElementProp` 同时服务筛选与候选值枚举，无重复遍历 |
| **最小分配** | 每图元至多 1~2 次短字符串构造（属性比较需要）；值枚举用 QSet 去重 |

万级图元下筛选为一次线性扫描 + 一次批量落地，复杂度 O(n)，与全选/反选同量级。

## 6. 与现有体系的一致性

- 选择结果与点击/框选/图层选择**同一状态源**，属性面板、删除、移动、智能标注等所有选中操作直接可用；
- 命中集只含**可见图元**（与框选/图层选择语义一致，隐藏图元不入选）；
- 编组信息随图元走，块引用（BlockRef）内部子图元的 groupName 在展开副本上存在但不影响顶层选择语义；
- 菜单入口：【编辑 → 选择工具 → 编组管理 / 快速选择 / 过滤器】。

## 7. 影响范围与兼容性

- **旧档案**：零影响。`groupName` 缺键回落空串、`savedFilters` 缺键回落空列表；无既有键语义变化。
- **改动文件**：Element.h/.cpp（groupName + 序列化）、SketchModel.h/.cpp（编组 API/applySelectionDelta/savedFilters）、QSelect.h/.cpp（新建）、MainWindow.h/.cpp（菜单/读写档）、GroupManagerDialog、QSelectDialog、FilterDialog（新建）、icons.h（3 个 glyph）、CMakeLists.txt。
- **持久化契约**：`qselectPropName/qselectOpName` 的中文名同时是 JSON 键，只增不改（改键会破坏旧过滤器存档）。

## 8. 已知限制

- 编组无嵌套（AutoCAD 同样默认无嵌套）；需要层次编组时可扩展 groupName 为路径式命名。
- FILTER 组合方式为全局 AND/OR，不支持逐条件括号分组（覆盖绝大多数工程筛选场景；如需可扩展条件树）。
- 候选值枚举对超大图纸（10 万+）首次打开下拉会有一次 O(n) 收集，之后无开销；如需要可加缓存。
