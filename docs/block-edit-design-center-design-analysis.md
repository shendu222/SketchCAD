# SketchCAD 块迭代不破坏引用（BEDIT 在位编辑 + 设计中心符号库）：设计与深思

> 配套任务：让块定义可"在位编辑"而不破坏任何引用，并提供符号库（设计中心）浏览插入。
> 范围：① 模型层新增 `getBlockEntities` / `redefineBlockEntities`（在位编辑会话的读写两端）；
> ② 新增 `BlockEditorDialog`（BEDIT 在位编辑块内几何，Esc 丢弃不落盘）；
> ③ 新增 `DesignCenterPanel`（侧栏树：当前图块表 + 外部文件块表，双击插入）；
> ④ `cadfmt` 外部块表读取（JSON / DXF BLOCKS 段）。
> 约束：块表结构与 Xref 语义不动；嵌套块（引用含引用）编辑正确传播；编辑中 Esc 丢弃不落盘；
> 与 dynset 动态块参数并存。
> 验收：BEDIT 改内部几何 → N 处引用同步刷新断言；嵌套块两层级用例；
> 自测新增 ≥8 断言（实际 **+33**，435 → 468）；双目标零警告；`build.sh test` EXIT=0。

---

## 1. 核心洞察：引用式模型让"同步刷新"成为免费性质

本工程的块系统是**引用式**（与图层/线型的"快照式"相反），这一点在 `Block.h` 开头就已写死：

```
BlockDef   块定义 —— 块名 + 基点 + 块内图元 + 属性定义 + 动态参数
Element    块引用 —— 只存：块名 + 插入点/旋转/缩放 + 属性值 + 动态参数值
```

所有引用在渲染 / 命中 / 导出时都经 `SketchModel::expandBlockRef(ref)` → `block::expandRef`
**实时读取** `BlockDef`。因此：

> **"改块内几何 → 所有引用同步刷新"不需要任何反向推送逻辑。**
> 改一份定义（`redefineBlockEntities`）就是"同步更新 N 处引用"，天然成立。

这与快照式（图层/文字样式 `resolveElement` 需逐图元改属性）截然不同。本设计的全部优雅性都来自
"识别出这条既有性质，并为其补上**编辑入口**"，而不是发明一套新机制。

### 1.1 嵌套块为何"免费"就对了

`block::evalDef` 对嵌套块做**递归复合变换**（`X2D::insert(...).then(xf)`），并用 `visiting`
集合做环检测。于是：

- 块 A 的 `entities` 里含一个指向块 B 的 `BlockRef`；
- 改 B 的 `entities` → A 的 `entities`（那个 BlockRef）**无需改动**；
- 但 A 的每个引用在展开时递归读 B → 自动看到新几何。

**传播链是"读取时"发生的，不是"写入时"推送的。** 这正是自测②要锁定的两层级用例。

---

## 2. 模型层：在位编辑会话的读写两端（#437）

在位编辑需要两个 API，构成一个"取出 → 编辑 → 写回"的会话：

```cpp
// SketchModel.h（块管理区）
std::vector<Element> getBlockEntities(const QString& name) const;
bool redefineBlockEntities(const QString& name, const std::vector<Element>& ents);
```

**`getBlockEntities`** 返回 `BlockDef.entities` 的**值拷贝**。这一条是"不落盘"的第一道保证：
调用方拿到的是副本，对它的任何修改都不触及模型。

**`redefineBlockEntities`** 只替换 `entities`，其余定义字段**一律保留**：

```cpp
pushMemento("在位编辑块 " + n);   // 自动入撤销栈
d->entities = ents;
for (auto& e : d->entities) e.id = 0;   // 块内图元 id 归零
emit blocksChanged();                  // 引用重绘
emit modelChanged();                   // 引用几何变化，必须重绘
```

| 字段 | 在位编辑时的处置 | 理由 |
|------|------------------|------|
| `entities` | **替换** | 这是在位编辑唯一要改的东西 |
| `attdefs` | 保留 | 属性块定义（TAG/prompt/constant…）不能因编辑几何而丢 |
| `dynParams` | 保留 | **与 dynset 动态块参数并存的硬约束** |
| `basePoint` | 保留 | 改基点会移动所有已插入引用的对齐基准，属另一操作 |
| `description` | 保留 | 元信息 |
| `isXref / xrefPath / …` | 保留 | **Xref 语义不动**（约束） |

> ⚠️ **陷阱**：`DynParam::affected` / `VisibilityState::hiddenEntities` 存的是**块内图元索引**。
> 在位编辑若增删图元会让这些索引错位。既有 `redefineBlockFromSelection` 已采用同样策略
> （注释写明"越界在展开时被安全忽略"）—— `applyDynParams` 对越界索引是**忽略**而非崩溃。
> 本设计沿用该策略：不重映射索引（复杂且易错），越界静默忽略，符合"不崩优先"的总原则。

### 2.1 Esc 丢弃：靠"不调用写回"而非额外机制

「编辑中 Esc 丢弃不落盘」这条约束，实现上是**免费的**——因为 `redefineBlockEntities` 是唯一的
落盘入口，Esc/Cancel 只要**不调用它**，定义与主模型都不变。自测③正是把这条契约钉死：

```cpp
std::vector<Element> edited = before;
edited[0].pts[1] = Vec3(99,0,0);
// 故意不调用 m.redefineBlockEntities("ESC", edited)
auto after = m.getBlockEntities("ESC");
CHECK(eq(after[0].pts[1], 10, 0), "Esc 未写回：原几何不变(不落盘)");
```

不需要 `BlockEditSession` RAII、不需要脏标记回滚——**API 形状本身就是保证**。

---

## 3. UI 层：`BlockEditorDialog`（在位编辑）

### 3.1 关键实现细节：`ElementItem` 的 pos-vs-el 契约

复用主视图的 `ElementItem` 渲染路径（与 `InsertBlockDialog` 预览同一套）能省掉大量渲染代码，
但必须先摸清一个契约（读 `GraphicsView.cpp::rebuild` 确认）：

> `ElementItem` 以 `pos() == (0,0)` 加入场景，`boundingRect()` / `paint()` **直接用 `el` 的世界坐标**绘制。

于是开启 `ItemIsMovable` 后：用户拖动 → Qt 只改 `pos()`，`el` **不变**；视觉上因为图形项变换而移动。
因此保存时必须把偏移**补偿回 `el`**：

```cpp
Element e = it->el;
translateElement(e, it->pos().x(), it->pos().y());   // 拖动偏移叠加回几何
e.id = 0;
```

`translateElement` 平移 `pts`（线端点/圆心/文本锚点/多段线顶点）与 `holes`（剖面填充的岛），
**不动** `radius` / 起止角（平移不改变半径与角度）。这是覆盖全图元类型的最小充分变换。

### 3.2 嵌套块在编辑器内的表现

编辑块 A（其 `entities` 含指向 B 的 BlockRef）时，该 BlockRef 仍以 `BlockRef` 形态留在场景中，
只是因 `it->model = model_` 而被 `ElementItem::paintBlockRef` **展开渲染**。保存时原样写回该
BlockRef —— **引用不被拍平**，块 A 与块 B 的引用关系得以保持，这正是"嵌套块编辑正确传播"的前提。

> 若把嵌套块"就地展开成子图元再写回"，就会**摧毁引用关系**（A 会内嵌 B 的一份几何副本），
> 此后再改 B 就不再传播。这是本设计刻意规避的错误。

### 3.3 目标块解析（MainWindow 侧）

`openBlockEditorDialog()` 的块名解析优先级：
1. 当前选中的 `BlockRef` → 其 `blockName`；
2. 块表仅一个块 → 直接用它；
3. 弹 `QInputDialog::getItem` 让用户从块表里挑。

---

## 4. 设计中心：`DesignCenterPanel`（符号库浏览 / 插入）

侧栏 `QTreeWidget` 分两组：

| 组 | 数据源 | 刷新时机 |
|----|--------|----------|
| 当前图块表 | `model_->blockTable()` | 连 `SketchModel::blocksChanged` → 实时 |
| 外部文件块表 | `cadfmt::blockTableFromJson/Dxf(path)` | 用户点「加载外部块库...」时 |

双击条目 → `insertBlockRequested(name)` 信号 → `MainWindow::insertBlockAtViewCenter(name)`：
外部块先经面板 `addBlockDef()` **并入本图块表**（可撤销），再以 `view_->viewCenterWorld()`
为插入点调 `model_->insertBlock()`。

### 4.1 外部块表读取（#439）

```cpp
std::vector<BlockDef> blockTableFromJson(const QString& path);  // 顶层 "blocks" 数组 或 裸数组
std::vector<BlockDef> blockTableFromDxf(const QString& path);   // 走 fromDxf 取 stats.blocks
```

- **JSON**：兼容两种形态——① SketchCAD 原生存档（顶层对象含 `blocks`）；② 裸块表数组。
- **DXF**：复用既有 `fromDxf` 的 BLOCKS 段解析，取 `DxfImportStats.blocks`。
  ⚠️ `fromDxf` 在 `C.count == 0`（ENTITIES 段无可识别图元）时返回 `false`，
  故**只含块表的 DXF 需在 ENTITIES 段至少有一个实体**，否则取不到块表——自测⑤据此构造样本。
- 两者都过滤 `*` 前缀匿名块（与导入器口径一致）。

### 4.2 视图中心作为插入点

新增 `GraphicsView::viewCenterWorld()`（`mapToScene(viewport()->rect().center())`）。
设计中心面板**不持有 GraphicsView**，只发信号，由 MainWindow 消费并求中心点——
保持面板与视图解耦，避免 `#include "ui/GraphicsView.h"` 造成的循环依赖。

---

## 5. 自测（`t_blockEdit`，+33 断言，435 → 468）

新增 `tests/SketchSelfTest.cpp::t_blockEdit()`（`void`，逐条 `CHECK` 计数，与 `t_layerDynamic`
同范式），在 `main()` 的"存档兼容/黄金样本"层后调用。五组用例：

| 组 | 锁定的不变式 | 关键断言 |
|----|-------------|---------|
| ① 顶层块 N 处引用同步 | 改端点 (10,0)→(20,0) 后，**3 处**引用展开后都含 `insertPos+(20,0)`，且旧端点 `insertPos+(10,0)` **已消失** | 同步刷新 + 非追加 |
| ② 嵌套块两层级 | 块 A 含引用 B；改 B 半径 2→5；A 的 2 处引用展开后都见半径 5 的圆 | 跨层传播 |
| ③ Esc 不落盘 | 改本地副本但不调写回 API → 定义 `entities` 数与几何**均不变** | 契约 |
| ④ 保留动态参数 | 写回后 `attdefs`（TAG）与 `dynParams`（ROT）**仍在** | 与 dynset 并存 |
| ⑤ 外部块表读取 | JSON 读回含 `EXT`；DXF BLOCKS 读回含 `DXB` | 双格式 |

> ① 中特意断言"旧端点已消失"，是为了防止实现退化成"在原几何上**追加**新图元"这种
> 表面通过、实则错误的写法。

---

## 6. 菜单 / 命令入口

- 【绘图 > 块 > 在位编辑块...】→ `openBlockEditorDialog()`。
- 【视图 > 面板 > 设计中心】→ `designDock_->toggleViewAction()`（初始隐藏，按需勾出）。
- `openDesignCenter()` 提供"显示并置顶"入口。

---

## 7. 踩坑与经验

1. **不要为 BEDIT 发明 `BlockEditSession` RAII**：写回 API 唯一，"不调用即丢弃"已是天然保证，
   多一层状态机反而是 bug 来源。
2. **复用 `ElementItem` 前必须确认 pos-vs-el 契约**：主视图 `rebuild()` 里 `pos()` 恒为 (0,0)，
   靠 `el` 绘制。任何"拖动后回写"都必须 `translateElement(el, pos.x, pos.y)` 补偿，
   否则拖一个圆形会**只改显示不改数据**（保存后弹回原位）。
3. **嵌套块在编辑器里不能拍平**：展开渲染可以，写回必须保留 BlockRef，否则摧毁引用关系。
4. **`fromDxf` 对空 ENTITIES 返回 false**：`blockTableFromDxf` 依赖它，故纯块表 DXF 需带一个实体。
5. **面板不持有 GraphicsView**：用信号把"插入"请求抛给 MainWindow，中心点由 MainWindow 求，
   避免 UI 层循环依赖。

---

## 8. 变更清单

**新增文件**
- `src/ui/BlockEditorDialog.{h,cpp}` — BEDIT 在位编辑器
- `src/ui/DesignCenterPanel.{h,cpp}` — 设计中心侧栏
- `docs/block-edit-design-center-design-analysis.md` — 本文

**修改文件**
- `src/core/SketchModel.h/.cpp` — `getBlockEntities` / `redefineBlockEntities`
- `src/core/CadFormats.h/.cpp` — `blockTableFromJson` / `blockTableFromDxf`
- `src/ui/GraphicsView.h/.cpp` — `viewCenterWorld()`
- `src/ui/MainWindow.h/.cpp` — 设计中心 dock + BEDIT 菜单 + 三个槽 + 信号接线
- `CMakeLists.txt` — 纳入两个新源文件
- `tests/SketchSelfTest.cpp` — `t_blockEdit()` + 注册

**验收结果**
- 自测 **468 / 0**（基线 435，+33）
- `SketchCAD` / `SketchSelfTest` 双目标 **零警告零错误**
- `build.sh test` **EXIT = 0**
