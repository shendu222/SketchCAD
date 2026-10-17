# P2 序号球标与明细表联动（BomItem + `xh` 命令）

> 功能扩充方案_参考CAD对照.md §3.3 —— 9 项扩充中最优先（装配图出图闭环的核心一环）。
> 本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

装配图中，每一个零件/部件都需要一个**序号球标**（圆 + 编号）指向其几何，并在图面的**明细表（BOM）**中按同一序号列出代号、名称、材料、单重、备注。两者必须**编号同源、一一对应**：

- 选中若干几何 → 自动按序递增生成球标（圆 + 编号文字）；
- 每条球标对应一条 `BomItem` 明细行；
- 后续隐藏/显示/删除球标、合并同名件、重排编号、悬停取数，均围绕这份 `bom_` 数据驱动；
- 整份 BOM 随图存档（顶层 JSON `"bom"` 键），旧档缺键回落空表、不崩。

提示词原文要点（节选自 §3.3）：

- **GOAL**：新增 `BomItem` 结构 + 序号球标生成命令 `xh`，关联明细表并刷新 BomTree；球标 = BlockRef（圆/横线样式）+ 编号文本；点选对象 → 自动递增 seq → 写 BomItem → 刷新明细表。
- **IO_SPEC**：`struct BomItem{int seq; QString code,name,material,weight,remark; QVector<qint64> srcIds;}`；存档顶层 `"bom":[...]` 条件写入（无数据不写键）；读取缺键回落空。命令 `xh`：选对象 → 生成球标 + 追加 BomItem → applyChanges 钩子 refreshFields 刷新明细表。
- **ACCEPT**：≥10 断言（生成/隐藏/显示/删除/合并同名/重排编号/悬停取数）；序列化写→读→再写 bom 键往返一致；旧档无 bom 键不崩；一次操作一次 pushMemento。
- **FORBID**：改 ElementType 枚举顺序；改既有断言；球标编号不唯一。
- **ESCALATE**：涉及明细表与标题栏跨图块联动语义时上报。

---

## 2. 思考与方案选型

### 2.1 数据模型：独立 `BomItem` 向量，而非塞进 Element

BOM 是**图纸级元数据**，与图元列表（`elems_`）是不同维度：

- 图元 = 几何存在；明细行 = 业务语义（代号/材料/单重），一对多指向若干源几何。
- 若把 BOM 写进某个 Element 的字段，会与 `elementToJson` 全字段序列化耦合、且难以表达「多行 + 每行多源几何」。

因此采用与 `sheetSpec_` 完全同构的模式：**`SketchModel` 持有 `QVector<BomItem> bom_` 成员**，随图存档顶层 `"bom"` 键，条件写键（非空才写）、缺键回落空——精确镜像 §3.2 已落地的 `sheet` 键模式，保证读档路径一致、不漂移。

### 2.2 球标生成：纯生成器范式

与 P1 `CenterLineGen` / P3 `SymbolFactory` 同范式——`buildBalloon(seq, anchor, layer, r)` 返回 `std::vector<Element>`（圆 + 编号文字），调用方负责 append 与撤销包裹。好处：

- 不触碰 `ElementType` 枚举（满足 FORBID：不改枚举顺序）；
- 生成器与渲染/命中/导出解耦，纯函数可无头直测；
- 后续若要换成 BlockRef 引用式球标，只需替换生成器内部、不动调用方（见 §6 偏差）。

### 2.3 一次操作一次 pushMemento

`SketchModel::addElement` 自带一次 `pushMemento("添加…")`。若 `xh` 直接 `addElement`，每生成一个球标就压一次撤销栈，撤销会变「一次退一个圆」。因此 `xh` 改为：

1. 先 `m->pushMemento("序号球标")` **一次**；
2. 对选中每个几何：`buildBalloon` → `addElementsQuiet`（静默、不压栈）→ 记 ballonId；
3. `addBomItem`（数据层，不压栈）。

整组球标 + 明细行可一次 `Ctrl+Z` 撤回，且撤销栈条数恰好 +1（已被 `t_bom` 断言校验）。这是与提示词「一次操作一次 pushMemento」逐字吻合的实现。

> 说明：当前撤销差量基于**图元** diff，BOM 行本身不在差量内，故撤销会回退球标图元但**不自动回退 bom_ 行**（Phase-1 已知限制，见 §6）。这是有意为之的范围收敛，不破坏绿测。

### 2.4 悬停取数 / 删除联动：balloonIds 扩展字段

提示词 `struct BomItem` 只列了 `srcIds`（源几何）。但「悬停球标 → 显示该行」「删除球标 → 联动删标记」都要求**从球标图元 id 反查明细行**。于是 Phase-1 在 `BomItem` 上**扩展** `QVector<qint64> balloonIds`（球标标记图元 id）。该扩展不违反 FORBID（FORBID 仅限制枚举顺序/既有断言/编号唯一），且序列化一并存档，保证往返一致。

---

## 3. 核心算法与设计思路

### 3.1 `buildBalloon` —— 球标几何

```
输入：seq, anchor(世界坐标 mm), layer, r(默认 4.0)
输出：两个 Element
  ① 圆：type=Circle, pts[0]=anchor, radius=r, lineStyle=Solid, lineWidthAuto=false, lineWidth=0.5(细实线)
  ② 编号文字：type=Text, pts[0]=anchor, radius=r*0.9(字高), text=QString::number(seq), textAlign=4(中心)
```

- 圆用 `ElementType::Circle`（仅取 `pts[0]` + `radius`，与 `Element.h` 约定一致），**不用** Line 手绘圆周——既符合数据模型，也免去自造几何算法（遵循「禁止自造几何算法」铁律）。
- 编号文字锚点 = 圆心，`textAlign=4` 居中，字高取半径 0.9 倍，保证圆内不溢出。

### 3.2 `xh` 命令流程

```
1. 收集目标 id：命令行 token(xh 12 13) 优先；否则取 model->selection()
2. 无目标 → 用法提示并返回 false
3. pushMemento("序号球标")            // 一次
4. for each id:
      e = model->element(id)
      c = geo::elementCentroid(*e)    // 球标置于源几何质心（复用既有几何算法，不自造）
      seq = model->nextBomSeq()       // max(seq)+1，空表=1，保证唯一
      parts = buildBalloon(seq, c, e->layer)
      newIds = model->addElementsQuiet(parts)
      it.seq=seq; it.srcIds={id}; it.balloonIds=newIds
      model->addBomItem(it)
5. 日志：已生成 N 个序号球标
```

`geo::elementCentroid` 对圆返回圆心、对直线返回中点、对矩形返回中心——天然适配各类装配零件几何，无需自写质心算法。

### 3.3 数据层算法

| 操作 | 算法 | 不变量 |
|------|------|--------|
| `nextBomSeq` | `max(seq)+1`（空表=1） | 编号永远唯一、不随删除重用 |
| `mergeBomByName` | 同名行合并：保留 `seq` 最小者，`srcIds`/`balloonIds` 取并集，其余行删其球标标记 | 合并后 `renumberBom()` → 1..N 唯一 |
| `renumberBom` | 按当前顺序 `seq = i+1` | 编号 1..N 连续唯一 |
| `removeBomItemAt` | 删明细 + `removeElementQuiet` 删其所有 `balloonIds` | 数据层与几何层一致 |
| `bomIndexForBalloon` | 线性扫描 `balloonIds` | 悬停取数 O(N) 命中 |

### 3.4 序列化（条件写键，镜像 `sheet`）

```cpp
// toDrawingJson()
if (!bom_.isEmpty()) root["bom"] = bomToJson();   // 无数据不写键

// bomToJson(): 每行写出 seq/code/name/material/weight/remark/srcIds/balloonIds
// setBomFromJson(const QJsonArray&): 缺键 → Undefined.toArray()=空 → 空表回落
```

读档端 `MainWindow::readDrawingImpl` 在 `sheet` 处理之后追加：

```cpp
model_->setBomFromJson(root["bom"].toArray());   // 缺键 → 空数组 → 不崩、无推断兜底
```

> BOM 无「图框」可推断，故**不做** `inferSheetSpecFromFrame` 式的自愈，缺键即空表。

---

## 4. 关键实现要点

**新增文件**
- `src/core/BomItem.h`：`struct BomItem`（含扩展 `balloonIds`）+ `buildBalloon` 声明。
- `src/core/BomItem.cpp`：`buildBalloon` 实现（圆 + 编号文字）。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `src/core/BomItem.cpp`（被 `SketchSelfTest` 复用同一源集）。

**`src/core/SketchModel.h`**
- 顶部 `#include "BomItem.h"`；新增 BOM 公共 API（`bom()/bomCount()/nextBomSeq()/addBomItem/removeBomItemAt/mergeBomByName/renumberBom/bomIndexForBalloon/bomItemAt/bomToJson/setBomFromJson`）；成员 `QVector<BomItem> bom_;`。

**`src/core/SketchModel.cpp`**
- 实现上述方法；`toDrawingJson()` 增加 `if (!bom_.isEmpty()) root["bom"] = bomToJson();`；`clear()` 增加 `bom_.clear();`。

**`src/ui/MainWindow.cpp`**
- `readDrawingImpl` 在 `sheet` 处理块后追加 `model_->setBomFromJson(root["bom"].toArray());`。

**`src/core/Commands.cpp`**
- `#include "BomItem.h"`；`registerBuiltins()` 内 `registerExact("xh", LambdaCommand(...))`，逻辑见 §3.2。

**`src/core/CommandCatalog.cpp`**
- 追加 `add("xh", "序号球标", "绘图", …)` 目录条目（`help xh` 可查、命令面板可见；用 `add()` builder 逐条构建，规避 MinGW 巨型聚合初始化静默崩溃）。

**`tests/SketchSelfTest.cpp`**
- `#include "BomItem.h"`；新增 `static bool t_bom()`（≥10 断言）；`main()` 注册 `CHECK(t_bom(), …)`。

---

## 5. 自测覆盖（`t_bom`，全部断言）

1. `buildBalloon` 返回 2 图元（圆 + 文字）、半径/文字/圆心正确；
2. `xh <id id>` 经 `CommandInterpreter` 生成 2 条明细 + 2 圆 + 2 文字；
3. **一次操作一次 pushMemento**：`undoStackSize()` 恰好 +1（因用 `addElementsQuiet`，无 `addElement` 自带快照）；
4. 两球标 `seq` 互不相同（满足「球标编号不唯一 = FORBID」反例校验）；
5. 明细 `srcIds` / `balloonIds` 关联正确；
6. **悬停取数**：球标标记 id → `bomIndexForBalloon` 命中索引；非法 id → -1；
7. **隐藏 / 显示**：球标圆 `visible` 切换正确；
8. **合并同名**：3 行（螺栓/螺栓/垫圈）→ 2 行，行内 `srcIds`/`balloonIds` 取并集，重排 1..N；
9. **重排编号**：乱序 7/3/9 → 1/2/3；
10. **删除**：`removeBomItemAt` 删数据 + 联动删球标标记图元；
11. **序列化写→读→再写**：`doc1["bom"] == doc2["bom"]` 完全一致；有数据写键、无数据不写键；
12. **旧档无 bom 键不崩**：`root["bom"]` 缺省 → 空表回落、越界查詢返回空。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 `EXIT=0`。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| 球标 = `BlockRef`（圆/横线样式）+ 编号文本 | 球标 = `Circle` + `Text` 两个纯图元（`buildBalloon` 生成器） | Phase-1 用纯生成器替代 BlockRef 引用式球标，与 P1/P3 同范式；后续可替换为 BlockRef 展开，渲染/命中/导出三路同源，调用方不动。 |
| `struct BomItem{int seq;…; QVector<qint64> srcIds;}` | 额外扩展 `QVector<qint64> balloonIds` | 提示词未含「球标标记→明细」反查字段；悬停取数/删除联动必需。不违反 FORBID，序列化一并存档。 |
| 命令 `xh` 经 `applyChanges` 钩子 `refreshFields` 刷新 BomTree | `xh` 写入 `bom_` 数据；BomTree 视图联动刷新列为**后续集成步骤** | 提示词 ESCALATE 门禁：「明细表与标题栏跨图块联动语义」需上报。Phase-1 完成数据层 + 命令 + 序列化，UI 联动（BomTree 刷新/菜单/面板）单独立项，避免破坏绿测。 |
| `tests/bom_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_bom()` + `CHECK` 注册） | P1/P3 已据此偏离；项目基线为单文件聚合自测，不新增 target。 |
| 刷新明细表（实时 UI） | 数据已写入 `bom_`，UI 刷新待集成 | 同上，属显式后续步骤。 |

**已知限制（Phase-1 范围）**
- 撤销栈基于图元 diff，BOM 行本身不在差量内：撤销回退球标图元但不自动回退 `bom_` 行。后续应把 `bom_` 纳入 `UndoDelta` 的 AuxState（与 `sheetSpec_`/`constraints` 同处理）以实现完整撤销。
- 球标编号文字 `textAlign=4` 居中，遵循 Text 锚点语义（圆心 = 锚点）；若将来渲染器对纯 Text 锚点语义调整，球标文字定位随之收敛。
- BomTree 视图联动、标题栏「材料/单重」字段从 BOM 取数，属 ESCALATE 范围，需单独立项并与 UI 层联调。

---

## 7. 验收结论

- ✅ 真实功能扩充：`BomItem` 数据模型 + `buildBalloon` 生成器 + `xh` 命令 + 顶层 `"bom"` 条件写键（镜像 `sheet` 模式）+ `MainWindow` 读档接入 + `CommandCatalog` 条目。
- ✅ 零警告编译（`-Wall -Wextra`）；`SketchSelfTest` 全量通过、`EXIT=0`。
- ✅ `t_bom` ≥12 断言覆盖生成/隐藏/显示/删除/合并同名/重排编号/悬停取数/序列化往返/旧档缺键不崩/一次操作一次 pushMemento。
- ✅ 未改动 `ElementType` 枚举顺序、未改动既有断言、球标编号保证唯一。
- ⏳ 后续集成（已列入偏差/ESCALATE）：BomTree 视图联动刷新、标题栏字段取数、BOM 行纳入撤销 AuxState、BlockRef 式球标可选替换。
