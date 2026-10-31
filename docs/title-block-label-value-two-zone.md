# 标题栏「填写值覆盖标签」缺陷修复（2026-10-31）

## 根因

标题栏每个栏目由**两个独立文本图元**组成：

- 标签（灰，如「图纸名称 / 图号 / 材料」）—— 创建于 `sheet_frame.cpp` 的 `labelInCell`；
- 字段（黑，初始为空，由用户填写）—— 创建于 `fieldInCell`，命名 `tb:field:<key>`。

旧实现中 `labelInCell` 与 `fieldInCell` 都调用同一入口 `fitCellText`，而 `fitCellText` 的锚点
语义是「**单元格中心**」。于是标签与字段的锚点落在**同一单元格中心**。

- 未填写时：字段文本为空 → 只见灰色标签，画面正常；
- 填写后：黑色值文字在 `TitleBlockForm` 写回（→`fitTitleBlockText`）时被复位到同一中心，
  与灰色标签同点重叠，用户所见即「值把标签盖住了」。

`batchSign`（批量签名）也走 `fitCellText`，写入 `tb:field:*` 时同样会被拉回整格中心，维持重叠。

## 修复要点：标签带 + 值区 两区分离

**单一事实源**：在 `TitleBlockLayout` 新增两个分区入口，与原有 `fitCellText`（整格居中）共用
底层 `fitInZone`（收缩 + 矩形内居中）：

| 入口 | 锚点位置 | 用途 |
|------|----------|------|
| `fitCellText` | 整格中心 | 代号栏、签名格等无标签同位的单元格；旧档标签/普通文本 |
| `fitCellLabelText` | 单元格顶部「标签带」中心 | 灰色标签（高 `kTitleBlockLabelStripMm = 4mm`） |
| `fitCellFieldValue` | 去掉标签带后的「值区」中心 | `tb:field:*` 填写值 |

两区由同一 `labelStripOf(bottom,top)` 推导（`strip ≤ 半格`，退化小格自适应减半），严格不重叠；
水平方向仍全宽居中（锚点 X = 格中心）。

**改动文件**

1. `TitleBlockLayout.h / .cpp`
   - 新增 `kTitleBlockLabelStripMm`、`fitCellLabelText`、`fitCellFieldValue`；
   - `fitCellText` 重构为委托 `fitInZone`；
   - `fitTitleBlockText` 对 `tb:field:*` 图元改走 `fitCellFieldValue`（值区），其余保持整格居中
     —— 这是「写回端」字段落值区的**唯一收敛点**。
2. `sheet_frame.cpp`
   - `appendStandardTitleBlock` 与 `appendTitleBlockGrid`（插入/自动套框标题栏共享）的
     `labelInCell → fitCellLabelText`、`fieldInCell → fitCellFieldValue`；
   - 代号栏（`appendCodeBar`）标签列与值列本就左右分列，互不重叠，保持 `fitCellText` 不变。
3. `BatchTools.cpp`（`batchSign`）
   - 收敛为调用 `fitTitleBlockText` 单一入口（字段自动落值区），
     顺带消除原调用中 `cell.bottom()/cell.top()` 传参颠倒的隐患。
4. `tests/SketchSelfTest.cpp`（`t_titleBlockAux`）
   - TB① 垂直断言改为按角色：标签=标签带中心、字段=值区中心；
   - **新增回归断言**：同单元格内灰色标签块与填写值块 `intersects == false`（直接捕获本缺陷）；
     TB② 预填长图名场景同样加该断言。

## 验证

- 编译：`cmake --build -j 1`，双目标零警告。
- 自测：**通过 3157 / 失败 0**（较修复前 +3，即 TB①×2 规格 + TB② 的「标签/值块零重叠」断言）。
- `cmp build↔dist` 字节一致。
- 几何可证明性：标签块∈顶部标签带、值块∈值区，两区由同一 strip 分割、互不相交 →
  填写标题栏后「图纸名称/图号/材料/设计/审核/批准/日期」等标签始终完整可见，值居其下方。

## 兼容性说明

- 暗/亮主题、工具栏文字等无关改动，本修复仅移动标题栏文本锚点 Y，屏幕与打印（`PlotRenderer`）
  同源锚点语义，两者一致。
- 旧档（字段锚点为整格中心）经写回 `fitTitleBlockText` 会重定位到值区；标签若仍为旧中心，
  与值区在 12mm 带内仍有少量错位余量（新建图纸则完全两区分离）。
