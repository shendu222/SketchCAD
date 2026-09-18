# 折线全功能 — 实现说明

> 项目：航空 CAD 草图绘制模块（sketch-cad-cpp，C++17 / Qt 6.10.1 / CMake+Ninja）
> 目标：参照 FreeCAD 草图「Polyline」菜单 10 项，完整实现折线绘制与编辑（直线段 + 弧段混合）。

## 1. 功能范围（菜单 10 项）

| 菜单项 | 工具 ID | 行为 |
|---|---|---|
| Polyline | `polyline` | 拾点序列生成多段线；点击首点 < 3mm 自动闭合；右键结束 |
| Add node | `polyline_add_node` | 选中折线 + 段上点 → 段中点插入节点 |
| Append node | `polyline_append_node` | 选中折线 + 端点近处 → 首/末端追加 |
| Delete node | `polyline_delete_node` | 选中折线 + 节点 → 删节点（< 2 节点自动删整条） |
| Delete between two nodes | `polyline_delete_between` | 选中折线 + 两节点 → 删中间部分 |
| Trim segments | `polyline_trim_segments` | 选中折线 + 段 → 删该段（删段终点节点） |
| Create Equidistant Polylines | `polyline_equidistant` | 选中折线 → 沿节点法向偏移生成新折线 |
| Polyline from Existing Segments | `polyline_from_segments` | 选若干 line/arc/polyline → 合并为一条折线 |
| Polyline Arcs to Chords | `polyline_arcs_to_chords` | 选中折线 → 所有弧段转直线 |
| Polyline Change Segment Type | `polyline_change_segment_type` | 选中折线 + 两段 → 切换直线↔弧 |

## 2. 数据模型

- `ElementType::Polyline`（追加在枚举末尾，`.sketch` 归档向后兼容）。
- `Element` 新增字段：
  ```cpp
  std::vector<bool> arcFlags;   // 每段类型（false=直线，true=弧），size == 段数
  ```
  - 弧段几何不存半径/圆心，渲染/命中时按**邻接 3 节点**外接圆动态求——这样弧段可随邻接节点编辑自适应。
- `ElementFactory::createPolyline(pts, arcFlags, closed, model)`。
- JSON 序列化新增 `arcFlags` 数组，向后兼容（旧档无字段时回退为空=全直线）。
- `Element::sizeDesc()` 输出「节点 N / 段 M / 弧段 K / 总长 L mm」。

## 3. 几何层（core/Geometry）

- `struct ArcSeg { Vec3 center; double radius; double startDeg; double sweepDeg; bool valid; };`
- `ArcSeg arcThrough(pts, segIdx, closed)`：取段 i (pts[i]→pts[i+1])，用三邻接节点 (pts[i-1], pts[i], pts[i+1]) 外接圆；通过邻接点方向校准 sweep（保证邻接点落在弧的那侧）；退化（n==2 或三点共线）时以端点为圆心、半径=弦长。
- `samplePolyline(pts, arcFlags, closed, perSeg=16)`：直线段直接 lineTo（1 点/段），弧段按圆周均匀采样 perSeg 点；闭合时首末重合。
- `distanceToElement / elementHandles / setElementHandle / flatten` 加 Polyline case：控制点 = 全部节点；拖动改写 pts 即可（弧段几何自动重算）；flatten 把弧段离散为 perSeg 段折线供求交。

## 4. 渲染（GraphicsView::buildPath）

- 直线段：`path.lineTo(qpt(b))`。
- 弧段：`QRectF(ar.center, ar.radius)` + `arcMoveTo/arcTo`（Qt 角约定与 Y 翻转协调：`-startDeg / -sweepDeg`）。

## 5. 工具层（ui/tools）

- `PolylineTool`（绘制 ITool）：
  - onPress：snap 后入栈 pts_；pts.size() >= 3 且距首点 < 3mm → 自动闭合结束；否则 onMove 实时预览从最后节点到当前鼠标的引导段。
  - onRightButton：开放折线结束（pts < 2 视为取消）。
  - finish：调用 `ElementFactory::createPolyline`，setSelected。
- `PolylineEditTool`（单类多 id，9 个编辑）：
  - 入口 prompt 按 id_ 给出不同提示。
  - 拾取：`pickPolyline`（限定 ElementType::Polyline）、`pickSegmentLike`（line/arc/polyline，from_segments 用）。
  - 命中：`nearestNode`（最近节点）、`nearestSeg`（最近段，对弧段先经 arcThrough 求圆心再投影）。
  - 状态字段：`firstId_/firstIdx_/firstSeg_`（两步操作 delete_between / change_segment_type）、`fromIds_`（from_segments 累积）。

## 6. CLI（core/Commands）

- `PolylineModeCommand` 路由：
  - 精确 `polyline` → 创建（任意多点 x,y...，首末重合自动闭合）。
  - 前缀 `polyline_` → 9 个编辑子命令（add/append/delete/delete_between/trim/equidistant/from_segments/arcs_to_chords/change_segment_type）。
- 实现与 ITool 端**等价**（独立几何代码，不依赖 UI 状态），保证 CLI 与 GUI 行为一致。
- `from_segments` 例外：跳过「需先选折线」通用检查（其参数是源段 id 列表而非目标折线 id）。
- 帮助文本新增 4 行（绘制 + 9 个编辑的用法示例）。

## 7. 菜单/工具栏/图标

- 顶级菜单「折线(&Y)」10 项（避开与「多边形(&P)」冲突）：1 绘制 + 5 节点/段编辑 + 4 高级编辑，分 3 组用分隔符。
- 草图绘制菜单：「折线」section + 子菜单（10 项）。
- 工具栏下拉按钮：主按钮 `polyline`（绘制），下拉 9 个编辑；快捷键 P。
- 调色板入口「折线」（ElementPanel）。
- 图标（icons.h）：折线字形（3 段折线 + 4 节点圆点）。

## 8. 测试

`tests/selftest.cpp` §26「折线：10 种功能（绘制 + 9 编辑）与几何层」60+ 用例全绿：
- 6.1–6.3 创建（开放/闭合/参数错误）
- 6.4–6.5 `arcThrough` 求外接圆（pts (0,0)(10,0)(5,5) → (5,2.5) r≈5.59）+ `samplePolyline` 弧段采样点落在圆上
- 6.6 add/append/delete_node
- 6.7 delete_between / trim_segments（多次 trim 验证裁剪语义与 2 节点边界）
- 6.8 equidistant 生成新折线
- 6.9 from_segments 合并 line+line
- 6.10 arcs_to_chords + change_segment_type（单段 + 范围 0..1 切换）
- 6.12 错误路径（id 不存在、参数不足、未知子命令）

## 9. 关键文件清单

| 文件 | 变更 |
|---|---|
| `src/core/Element.h` / `Element.cpp` / `ElementFactory.*` | `ElementType::Polyline` + `arcFlags` + 工厂 + sizeDesc |
| `src/core/Geometry.h` / `.cpp` | `ArcSeg` / `arcThrough` / `samplePolyline` + 4 个 switch case |
| `src/ui/GraphicsView.cpp` | buildPath Polyline 渲染（lineTo + arcTo） |
| `src/ui/tools/PolylineTool.{h,cpp}` | 折线绘制（新增） |
| `src/ui/tools/PolylineEditTool.{h,cpp}` | 9 个编辑（新增） |
| `src/core/ToolRegistry.*` | `ToolCategory::Polyline/PolylineEdit` + 10 个工具注册 |
| `src/ui/tools/ToolManager.cpp` | `registerBuiltins` 增加 10 个 polyline_* |
| `src/ui/MainWindow.*` | 顶级「折线(&Y)」菜单 + 草图绘制 section + 工具栏下拉 + polylineSubModes() |
| `src/ui/ElementPanel.cpp` | 「折线」入口 |
| `src/ui/icons.h` | 折线字形 |
| `src/core/Commands.{h,cpp}` | `PolylineModeCommand` 路由 + help 文本 |
| `src/core/CommandInterpreter.{h,cpp}` | `polylineCommandNames()` |
| `CMakeLists.txt` | 加入 PolylineTool.cpp / PolylineEditTool.cpp |
| `tests/selftest.cpp` | §26 折线测试 |
