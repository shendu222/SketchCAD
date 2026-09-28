# 边界 / 区域（BOUNDARY · BPOLY · REGION）功能设计分析

> 本文记录「新增 BOUNDARY（边界）、BPOLY（边界多边形）、REGION（区域）」功能的**深度思考过程**：
> 从需求动机出发，逐层推演关键设计决策、权衡取舍、与既有剖面填充体系的解耦与集成方式，
> 最后复盘编译验证中暴露的问题。目标读者是后续维护者——读完应能理解「为什么这样设计」而非仅「改了哪里」。

---

## 1. 需求拆解与动机

用户原始诉求可拆为三层：

1. **自定义填充边界**：支持用户通过交互方式自定义并指定填充边界，**解除剖面填充对内置边界追踪算法的单一依赖**。
2. **区域选择与管理**：提供区域选择与管理机制，使**复杂腔体填充无需人工逐段描边**即可自动识别与填充。
3. **复用与可编辑**：交互指定的边界数据应可被剖面填充模块**直接复用**，并支持后续编辑；同时输出本深度思考文档。

### 1.1 既有体系的瓶颈

既有剖面填充（`Section` 模块）的两条入口：

- **点选拾取**（`makeSectionChangeForPick` → `buildSectionLoopSet` → `pickRegionAt`）：把全图图元投入「边界交叉细分 + 平面细分」自动成环流水线，找出包含鼠标点的内层闭环再填充。
- **选图元填充**（`makeSectionChanges` → `buildLoopsInternal`）：对选中的闭合图元（圆/椭圆/矩形/多边形/闭合折线/样条/Hatch）自动成环后填充。

这两条路径**都强依赖自动边界追踪**。当用户面对「外框 + 多个内孔」的复杂腔体（如带加强筋的法兰腔、带孔的支座）时：

- 自动成环会把内孔当独立闭合图元，或在内孔与外框之间推导出非预期的区域；
- 用户被迫先逐段描出轮廓线，再依赖算法拼环——一旦算法对交点/容差敏感，结果不稳定；
- 嵌套腔体（腔中腔）几乎无法靠自动追踪稳定命中。

所以**真正的痛点是「边界的权威来源」**：自动追踪是「猜测」用户意图，而用户手绘精确边界是「声明」意图。本功能要让「声明式边界」成为一等公民。

---

## 2. 设计目标与约束

| 目标 | 说明 |
|------|------|
| 解耦 | 剖面填充必须能消费「用户声明的边界」，而非只能从图元自动追踪 |
| 复用 | 边界数据直接喂给 `hatchRegionFromElement`，不二次转换、不丢内孔 |
| 可编辑 | 边界在填充前后都可被选中、平移、缩放、改名、删除 |
| 区域化 | 多个边界可归为一个 REGION，一次填充整个腔体 |
| 兼容 | 不破坏旧存档（ElementType 枚举「只增不改」）；不改动既有自动追踪正确性 |
| 最小侵入 | 尽量复用 Hatch 的几何与样式载体，避免新数据结构扩散到 20+ 个 switch 站点 |

---

## 3. 核心设计决策（含推演）

### 3.1 决策一：Boundary 复用 Hatch 的全部几何/样式字段，不新增存储

**推演**：剖面填充消费的到底是什么？

追踪 `Section::hatchRegionFromElement`（Section.cpp:1009），它**只读**以下字段：

```
e.pts          // 外环
e.holes        // 内孔列表
e.rotation     // 剖面线角度
e.radius       // 间距（mm）
e.sectionExempt
e.sectionMaterial
```

换言之，`Hatch` 图元本身就是「一个已定义的填充区域描述」。如果我们新建一个 `Boundary` 图元，但**让它携带完全相同的字段语义**，那么 `hatchRegionFromElement` 对 `Boundary` 与对 `Hatch` 的处理**完全一致**——函数无需任何改动。

**决策**：`Boundary` 复用 `Hatch` 的字段约定——
- `pts` = 外环多边形
- `holes` = 内孔（嵌套环）
- `rotation` = 剖面线角度（默认 45°）
- `radius` = 剖面线间距（mm，默认 3）
- `sectionExempt` / `sectionMaterial` = 豁免 / 材料

**收益**：
- `hatchRegionFromElement` **零改动**即可消费 Boundary；
- Boundary 在屏幕上以「虚线轮廓」渲染，区别于 Hatch 的「实心剖面线」，语义清晰；
- 序列化（JSON/DXF/DWG）走既有 `holes/sectionExempt/sectionMaterial` 通道，无需新字段。

**代价**：Boundary 与 Hatch 在数据结构上「长得一样」，需要通过 `type` 区分渲染与行为。这是有意的——用 `type` 做轻量区分，比引入平行类层次更省成本。

### 3.2 决策二：ElementType 枚举「只增不改」，Boundary = 14

**约束**：旧存档把图元类型存为整数序号。任何已有枚举值的位置都**不能**挪动，否则旧文件反序列化错位。

既有末尾为 `Xline = 13`，故新增值追加为：

```cpp
enum class ElementType {
    // ... 既有 0..13 ...
    Xline = 13,
    Boundary,   // = 14
};
```

`elementTypeName` 返回「边界」。这是整个系统新增类型时的铁律，单独成一条以避免回归。

### 3.3 决策三：REGION 不新增图元类型，用 `name` 分组

**推演**：REGION 的本质是「一组边界的命名集合」。如果为 REGION 再建一个图元类型 `Region = 15`，会产生：
- 新的 switch 站点（渲染/命中/序列化/导出/撤销…）；
- REGION 与 Boundary 的包含关系需要额外存储（谁属于哪个 region）；
- 自动追踪流水线要识别 REGION 并「把其成员聚合」——侵入面大。

**决策**：REGION = **共享同一 `name` 字段的若干 `Boundary` 图元**。

- `name` 字段在 `Element` 中已存在（用于块/Xref 等），零新增存储；
- 「未命名边界」统一显示为「未命名边界（N 个）」；
- 区域管理对话框只做 `name` 的读写与成员枚举，**完全不碰几何**。

**收益**：REGION 概念被「压缩」进已有的 `name` 维度，避免了第二种图元类型带来的全面侵入。这是本设计中「最小侵入」原则的最强体现。

### 3.4 决策四：让 Boundary 在 `buildSectionLoopSet` 中「独立登记」而非投入自动细分

**推演**：点选拾取路径（`buildSectionLoopSet`）把全图图元投入 `buildLoopsSubdivided` 自动细分。若把 Boundary 也丢进去：
- 用户手绘的精确外环会被自动追踪「重新拆环」或「与外框/内孔图元交叉剪开」，破坏用户意图；
- Boundary 的内孔是 `holes` 字段，**不是独立图元**，自动成环阶段根本没有内孔对象可参与，内孔会被静默丢弃。

**决策**：在 `buildSectionLoopSet` 中，先把 `Boundary` 与 `Hatch` 一起**排除出自动细分图**（`if (e.type != ElementType::Hatch && e.type != ElementType::Boundary) rest.push_back(e);`），再单独把每个 Boundary 的 `e.pts` 作为外环、`e.holes` 作为嵌套内孔**直接登记**进 `set.loops / loopSrcIds / loopArea`，且 `loopHatchId` 置 0（表示「可填充、尚未填充」）。

**收益**：
- 点选 Boundary 外环即可命中填充，内孔自动被掏空（`pickRegionAt` 基于 `loops` 做点包含判定，内外环关系天然成立）；
- 填充后该 Boundary 的 id 原地变为 Hatch，`loopHatchId` 被 Hatch 分支接管为「已填充」，重入拾取直接命中 AlreadyFilled，行为闭环；
- 自动追踪流水线对其它图元的行为**零变化**。

### 3.5 决策五：`makeSectionChangesEx` 拆分「Boundary 直接复用分支」

`makeSectionChangesEx`（选图元填充的「带回报」版）是落盘前的关键分叉点。原逻辑对 `others` 统一走 `buildLoopsInternal` 自动成环。

**决策**：进入函数先遍历 `srcs`，把 `Boundary` 挑出单独处理：

```cpp
for (const Element* b : boundaries) {
    SectionRegion r = hatchRegionFromElement(*b);   // 直接复用外环/内孔/角度/间距/材料/豁免
    Element h = makeHatchForRegion(r, b->id, b);    // 复用边界 id 原地替换
    Change ch; ch.id = b->id; ch.replacement = h;
    ch.note = "剖面填充（自定义边界）";
    res.changes.push_back(ch);
    ++res.filledRegions;
}
// 其余图元：走原有自动边界追踪
```

**要点**：
- Boundary 经 `hatchRegionFromElement` 直接复用其 `holes`——这正是自动追踪路径**做不到**的（自动路径在成环阶段就把内孔丢掉了）；
- 复用 `b->id` 原地替换 → 单次撤销、选中态不跳，REGION 按 id 分组仍有效；
- 其余图元完全走原逻辑，**保证交互边界「跳过自动追踪」这一核心诉求落地**。

### 3.6 决策六：交互工具 `BoundaryTool` 两段式拾取

复用既有 `PolylineTool` 的两段式交互框架（`ITool` + `ToolManager` + `ToolContext`）：
- `phase_ = 0`：拾取外环（连续点 + 双击/回车闭合）；
- `phase_ = 1`：拾取内孔（可多圈，Esc 结束当前孔，全部完成落盘）；
- 闭合半径 `kCloseRadius = 3.0` 与 PolylineTool 一致。

落盘为 `ElementType::Boundary`，使用 `ElementFactory::createBoundary(pts, holes, model)`，其默认样式与 `Hatch` 同源（rotation=45、radius=3、Solid、closed=true）。

**工具注册**：`ToolManager` 注册两个 id（`boundary` 与 `bpoly`）指向同一 `BoundaryTool` 实例；`ToolRegistry` 登记中文元数据「边界 / 边界多边形」，并接入工具栏下拉与修改菜单。

### 3.7 决策七：全 `switch(e.type)` 站点的扩展策略

系统约有 23 处 `switch(e.type)` 处理各图元类型。本功能采用**逐站判定、只改必要的**策略，而非无脑复制 Hatch 分支：

**必须扩展（与 Boundary 几何/语义相关）：**

| 站点 | 改动 |
|------|------|
| `Section::sectionSelfClosedOutline` | Boundary 直接沿用 `e.pts` 为外环 |
| `Section::buildSectionLoopSet` | 独立登记外环 + 内孔（决策四） |
| `Section::makeSectionChangesEx` | 拆分直接复用分支（决策五） |
| `GraphicsView::buildPath` / `paintOne` / `paintBoundary` | 虚线轮廓渲染（区别于 Hatch 实心剖面线） |
| `PlotRenderer::paintOne` / `paintBoundary` | 打印时用逻辑 mm 虚线轮廓，避免「非 cosmetic 双重缩放」失真 |
| `Geometry::distanceToElement` | 复用 Hatch 的 `distPolyline(pts, true)` + holes 逻辑 |
| `Inquiry::discretizeRing` / `areaOfElement` / `massPropOfElement` | 面积/周长/形心按「外环 + 内孔」算法 |
| `Snap::snapHandles` | 外环顶点可吸附 |
| `Block::blockTransform` | 缩放 `radius`、反射 `rotation` 角度 |
| `Modify::makeMirror` | 内孔反射 + `rotation` 角度反射 |
| `Element::sizeDesc` | 描述角度/间距/内孔数 |
| `CadFormats` DWG / STEP 导出 | 以闭合折线导出外环、内孔作独立折线（Hatch 此前未导出，本次一并补齐防数据丢失） |
| `icons.h` | `boundary` / `bpoly` 图标：虚线方框 + 内小虚线框 |

**明确保持 `default` 正确（无需改）：**

| 站点 | 原因 |
|------|------|
| `GraphicsView::worldLengthOf` | 走 `e.pts` 或空，Boundary 默认可用 |
| `Geometry::elementHandles` / `setElementHandle` / `flatten` / `elementCentroid` | 走 `e.pts` 或空 |
| `Section::decomposeToEdges` | Boundary 是闭合区域，**不应**被边界段分解；保持 default 排除正确 |
| `SketchModel` 缩放 `radius` | default 不改 |
| `Trim::polylineOf` | Boundary 不可被修剪，default `return false` 正确 |

**原则**：每处扩展都问「Boundary 在此处的语义是什么？」——若与 Hatch 一致则用 Hatch 分支逻辑；若 Boundary 是「区域」而非「可分解图元」，则保持排除。拒绝「为了对称而复制」。

---

## 4. 集成点清单

功能与既有体系的所有接线点：

1. **图元层**：`Element.h` 枚举 + `elementTypeName`；`ElementFactory::createBoundary`；`Element::sizeDesc` 序列化描述。
2. **剖面填充层**：`Section::sectionSelfClosedOutline`、`buildSectionLoopSet`、`makeSectionChangesEx` 三处加入 Boundary 分支。
3. **渲染层**：`GraphicsView`（屏幕）+ `PlotRenderer`（打印）各加 `paintBoundary`。
4. **几何/查询层**：`Geometry::distanceToElement`、`Inquiry` 三函数、`Snap`、`Block::blockTransform`、`Modify::makeMirror`。
5. **持久化层**：`CadFormats` DWG / STEP 导出闭合折线。
6. **交互层**：`BoundaryTool` + `ToolManager`/`ToolRegistry` 注册（`boundary`/`bpoly`）。
7. **UI 层**：
   - 工具栏：BOUNDARY 下拉按钮（图标 `boundary`）；
   - 修改菜单：`用选中边界填充(&B)...` → `sectionFillSelected()`；`边界区域管理(&R)...` → `openRegionManager()`；
   - `RegionManagerDialog`：REGION 的列出/新建/填充/全选/重命名/删除。
8. **构建层**：`CMakeLists.txt` 的 `UI_SOURCES` 加入 `RegionManagerDialog.cpp`（BoundaryTool.cpp 已在历史完成）。

---

## 5. 区域管理模型（REGION）

`RegionManagerDialog` 是对「共享 `name` 的 Boundary 集合」的可视化管理器：

- **列出**：遍历图面所有 Boundary，按 `name` 分组（无 `name` 归入「未命名边界」），显示每组边界数。
- **新建区域**：把**当前选中的若干 Boundary** 标记为同一 `name`（批量 `updateElement` 写 `name`）。
- **填充区域**：对该区域全部 Boundary 调用 `makeSectionChangesEx(members, SectionStyle{}, true)`——每个 Boundary 经决策五的专用分支复用外环/内孔/样式，**跳过自动追踪**，原地替换为 Hatch。
- **全选区域**：`setSelection(memberIds)` 选中整组 Boundary，便于整体平移/缩放/删除。
- **重命名 / 删除**：批量改写或删除该 `name` 下的所有 Boundary。

**用户视角的工作流**（复杂腔体一次填充）：
1. 用「边界(BOUNDARY)」工具：画一圈外框（phase 0）→ 画几圈内孔（phase 1）→ 落盘为多个 Boundary。
2. 打开「边界区域管理」，选中这些 Boundary → 「新建区域」命名（或保持未命名）。
3. 点「填充区域」→ 整个腔体（含所有内孔）一次填充完成，无需逐段描边、无需依赖自动追踪。

---

## 6. 编译验证与修复复盘

### 6.1 构建环境

- 工具链：Qt 6.10.1 mingw_64 + MinGW 13.1.0 + CMake + Ninja。
- 关键约束（来自项目经验）：
  - **必须用绝对路径** `cmake --build /d/01-CAD/02-avticationCAD/sketch-cad-cpp/build`（相对路径会静默失败且 exit 0，极易误判）。
  - **PATH 用 POSIX 风格 `:` 分隔**追加 Qt/MinGW/Ninja（Windows `;` 在运行时会导致 exe 缺 DLL、exit 127）。
  - **`-j 1`** 单线程编，避免重型目标 OOM（`cc1plus out of memory`）。

### 6.2 暴露的编译错误与根因

**错误**：`MainWindow.cpp:3106: error: 'section' has not been declared` —— `section::makeSectionChangesEx(...)` 未声明。

**根因**：`makeSectionChangesEx` 声明于 `namespace SketchCAD`（Section.h 仅 `namespace SketchCAD { ... }`，**无嵌套 `section` 命名空间**）。既有调用（如 `makeSectionChangeForPick`）在 `MainWindow` 内**无前缀**直接使用，因为 `MainWindow` 自身也在 `SketchCAD` 命名空间内。我在新增代码中误加了 `section::` 前缀，触发未声明错误。该错误同样存在于 `RegionManagerDialog.cpp`。

**修复**：两处 `section::makeSectionChangesEx` → `makeSectionChangesEx`（无前缀）；注释中同样笔误一并修正。

**教训**：
- 调用库函数前，先用 `grep` 确认其**真实命名空间**，不要凭「剖面填充相关，大概是 `section` 命名空间」的猜测写前缀。
- 同一符号在文件内已有正确调用范式（无前缀），新增代码应**对齐既有范式**而非引入新假设。
- 编译日志必须落盘到**项目目录内可读路径**（沙箱 `/tmp` 不可读），否则无法定位错误。

### 6.3 验证结果

- `cmake --build .../build --target SketchCAD -j 1` → **EXIT=0**，产出 `build/SketchCAD.exe`（约 11 MB）。
- 已同步至 `dist/SketchCAD.exe`（覆盖旧构建）。
- 注：本次会话删除了全部 `tests/` 自测（含 `SketchSelfTest`），故质量门禁改为「编译通过 + 人工真机验证」。建议在真机启动 `dist/SketchCAD.exe`，实测：画 BOUNDARY 外框+内孔 → 区域管理填充 → 验证内孔掏空与剖面线正确。

---

## 7. 关键经验沉淀

1. **复用字段语义 > 新建数据结构**：Boundary 复用 Hatch 的 `pts/holes/rotation/radius/sectionExempt/sectionMaterial`，使 `hatchRegionFromElement` 零改动生效——这是「解耦」能低成本落地的核心。
2. **概念压缩**：REGION 不新增图元，用 `name` 维度表达「分组」，把「第二种类型」需改的 20+ 站点压缩为「一个字段的读写」。
3. **区分「区域」与「可分解图元」**：Boundary 是区域，应被直接登记进填充环路；它**不是**可被边界段分解/修剪的图元，故 `decomposeToEdges`/`Trim::polylineOf` 保持排除正确。
4. **命名空间对齐既有范式**：库函数调用前缀必须与文件内已有调用一致，新增代码前先 grep 确认。
5. **最小侵入原则**：23 处 switch 站点，只改与 Boundary 语义相关的，其余明确保持 default 正确并写清原因，拒绝「为对称而复制」。

---

## 8. 文件清单

| 文件 | 改动性质 |
|------|----------|
| `src/core/Element.h` | 枚举 `Boundary = 14` + `elementTypeName` |
| `src/core/ElementFactory.h/.cpp` | `createBoundary()` |
| `src/ui/tools/BoundaryTool.h/.cpp` | 两段式交互工具 |
| `src/core/Section.cpp` | `sectionSelfClosedOutline` / `buildSectionLoopSet` / `makeSectionChangesEx` 三处 Boundary 分支 |
| `src/ui/GraphicsView.cpp/.h` | `buildPath` + `paintOne` + `paintBoundary` 虚线轮廓渲染 |
| `src/ui/PlotRenderer.cpp` | `paintOne` + `paintBoundary` 打印虚线轮廓 |
| `src/core/Geometry.cpp` | `distanceToElement` Boundary 分支 |
| `src/core/Inquiry.cpp` | `discretizeRing` / `areaOfElement` / `massPropOfElement` 扩展 |
| `src/core/Snap.cpp` / `Block.cpp` / `Modify.cpp` | 吸附 / 块变换 / 镜像 扩展 |
| `src/core/Element.cpp` | `sizeDesc` 分支 |
| `src/core/CadFormats.cpp` | DWG / STEP 导出闭合折线 |
| `src/ui/icons.h` | `boundary` / `bpoly` 图标 |
| `src/ui/ToolManager.cpp` / `src/core/ToolRegistry.cpp` | 工具注册与元数据 |
| `src/ui/MainWindow.cpp/.h` | 工具栏 + 修改菜单 + `sectionFillSelected` / `openRegionManager` |
| `src/ui/RegionManagerDialog.h/.cpp` | REGION 管理器（新建） |
| `CMakeLists.txt` | `UI_SOURCES` 加入 `RegionManagerDialog.cpp` |
| `docs/boundary-region-design-analysis.md` | 本文（新建） |
