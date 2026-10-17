# P9 扩展工具：机械图拆分 / 自动排图 / 批量签名（BatchTools）

> 功能扩充方案_参考CAD对照.md §3.3 —— P9（末项）。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch15「扩展工具：机械图拆分/自动排图/批量签名（🆕）」：面向机械图批量后处理的三件套工具。

提示词原文要点（§3.3）：

- **GOAL**：机械图拆分（按图层/图框切分存档）、自动排图（图框矩形装箱输出布局）、批量签名（图框签名栏填字）。
- **CONTEXT（必注入）**：`src/core/CadFormats.cpp`（writeDrawing 分片）、`src/core/SheetSpec.h`（图框矩形）。
- **IO_SPEC**：拆分 = 按选定元素子集调 writeDrawing；排图 = 矩形 shelf 装箱算法（**自写简单算法，G1 非几何无风险**）；签名 = 定位标题栏签名单元格填 Text。
- **ACCEPT**：① 拆分 ≥5 断言（切分后子档可独立打开）；② 排图 ≥5 断言（不重叠/利用率）；③ `tests/splitplot_test.cpp` 注册。
- **FORBID**：改存档格式；改枚举。

---

## 2. 思考与方案选型

### 2.1 纯函数工具层（沿用 P1~P8 范式）

三个工具均为**纯函数**（不读取/不改动模型；产出数据由调用方落盘或 apply）：

- 拆分产出子档 JSON 字节 + 路径（调用方仅需 `beginEdit` 语义无关——写文件本就无模型变更）；
- 排图产出摆放结果（UI 层据此平移图框/视口）；
- 签名产出 `SignUpdate` 副本（调用方 `beginEdit` + `updateElementQuiet` 应用）。

### 2.2 拆分：克隆源根、仅替换 elements（FORBID「不改存档格式」的核心应对）

提示词 IO_SPEC 说「按选定元素子集调 writeDrawing」，但 `writeDrawing` 实为 `MainWindow` 的 UI 层方法（按扩展名路由格式），且工程铁律是「**序列化单一入口 `toDrawingBytes()`**」。直接在工具里绕过分派自写 JSON 会造出第二序列化组装点（历史踩坑：BatchPlotEngine 手工拼装漏写 sheet/properties）。

**选型：子档 = 源图 JSON 根深拷贝 + 仅替换 `"elements"` 为该组图元的 `elementToJson` 数组**，其余顶层键（layers/blocks/sheet/bom/params/properties/layouts…）原样保留：

- 存档格式**零改动**（键集与 `toDrawingBytes()` 完全一致，子档可独立打开）；
- 不丢数据：图层/块定义/属性表随子档共存（BOM/FIELD 的 id 引用在子档中可能悬空，但读档链路对缺引用有 ### 回落，不会崩——记入已知限制）；
- 写盘走 `QSaveFile::commit()` 原子提交（Windows/MinGW 禁 `QTemporaryFile+rename`，工程约束）。

### 2.3 排图：shelf 装箱（IO_SPEC 明示可自写）

矩形 shelf（货架）装箱：按**高度降序**（同高宽降、再按 id 升——保证确定性）依次上架；当前架剩余宽度不足则新起一架（y 累加上一架最大高度 + gap）。构造上**保证不重叠**（同架左→右递进 + 架间高度隔离），无需事后校验。IO_SPEC 明示「自写简单算法，G1 非几何无风险」——装箱是布局问题而非几何求算，无 ESCALATE 触发。

不实现旋转（Phase-1）：`PackOptions::allowRotate` 字段保留但恒不生效，`PackPlacement::rotated` 恒 false——避免「旋转后标签朝向」等机械图语义问题（记入已知限制）。

### 2.4 批量签名：复用标题栏单元格单一入口

提示词 IO_SPEC「定位标题栏签名单元格填 Text」。工程已有成熟机制：

- 标题栏字段元素命名约定 `tb:field:<key>`（`sheet_frame.cpp` 生成，`TitleBlockForm` 消费）；
- `TitleBlockLayout::titleBlockCellAt(model, x, y)` 由网格线重建单元格（**无界 → invalid，绝不猜**）；
- `TitleBlockLayout::fitCellText` 度量→收缩→定位单一入口（标签贴顶/字段贴底）。

**选型**：签名 = 找 `tb:field:` 元素 → 匹配 key/label → 填字 → 经 `titleBlockCellAt` + `fitCellText`（stickTop=false 贴底）重定位/收缩。网格无效则**保留原锚点**（继承「绝不猜」原则）。key/label 双通道：label 经 `titleBlockFields()` 单一数据源映射，不另建对照表。

---

## 3. 核心算法与设计思路

### 3.1 模块 API（`src/core/BatchTools.{h,cpp}`）

| 工具 | 入口 | 产出 |
|------|------|------|
| 拆分-分组 | `groupByLayer(elems)` / `groupByFrame(elems, frames, outsideName)` | `QVector<SplitGroup>{name, elemIds}` |
| 拆分-写档 | `writeSplitDrawings(sourceRoot, groups, elems, outDir, baseName, ext)` | `SplitResult{outPaths, errors, written}` |
| 排图 | `packShelf(rects, opt)` | `PackResult{ok, placements, usedW/H, areaSum, utilization, error}` |
| 签名 | `batchSign(m, fields)` / `titleBlockFieldElems(elems)` | `SignResult{updates, unmatched}` |

**分组语义**
- 按图层：组名 = 图层名（保首见序；空 layer → `<无图层>`）；
- 按图框：元素包围盒**中心**归入首个包含它的帧（`frames` 保序即优先级，交叠区确定性地归前帧）；不在任何帧内 → `框外` 组（**不丢弃**）；`frames` 为空 → 全部归框外（等价不拆，调用方应检查）；
- 图元守恒：分组不丢不重（自测断言）。

**shelf 装箱伪码**
```
校验：纸张>0、gap≥0、rects 非空、每框 (w,h) 正有限；Σarea
排序：h 降 → w 降 → id 升
curY=0, shelfH=0, remainW=sheetW
for p in order:
    p 超纸张 → error, break
    p.w > remainW → curY += shelfH+gap; shelfH=0; remainW=sheetW   // 新架
    curY+p.h > sheetH → error「放不下」, break
    place(x = sheetW-remainW, y = curY); remainW -= p.w+gap; shelfH=max(shelfH,p.h)
ok = 全部放入; utilization = Σarea/(usedW·usedH)（≤1 钳位）
```

**签名语义**
- `fields`：key 或 label → 文字；label 经 `titleBlockFields()` 映射；
- 空值**跳过**（不擦除已有签名）；未知 key/label → `unmatched` 回填（防静默漏签）；
- 单元格 invalid → 保留原锚点/字高（`refit=false`）；
- 产出副本同 id，调用方 Quiet API 应用（一次签名一次 `beginEdit`）。

---

## 4. 关键实现要点

**新增文件**
- `src/core/BatchTools.h`：四个入口 + `SplitGroup/SplitResult/PackRect/PackOptions/PackPlacement/PackResult/SignUpdate/SignResult` 数据结构。
- `src/core/BatchTools.cpp`：
  - `sanitizeName`（组名/基名文件系统净化：路径非法字符/空白/控制符 → `_`，空 → `_`）；
  - `writeSplitDrawings`：`QHash<id, const Element*>` 反查 → `elementToJson` → 克隆根替换 `elements` → `QSaveFile` 原子写；幽灵 id / 空组 / 目录不可建 / 写失败 → `errors` 记因且该组不写；
  - `packShelf`：排序 + 货架循环（§3.1 伪码）；`utilization` 钳位 ≤1；
  - `batchSign`：key/label 双通道 + `titleBlockCellAt`/`fitCellText` 适配 + `unmatched` 回填。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `BatchTools.cpp`。
- `tests/SketchSelfTest.cpp`：`#include "BatchTools.h"/"TitleBlockLayout.h"/"SheetSpec.h"` + `static bool t_splitplot()` + `CHECK` 注册。

**约束遵守**
- **FORBID「不改存档格式」**：子档 = 源根深拷贝 + 仅替换 `elements`（键集与 `toDrawingBytes()` 一致；自测逐键比对源根其余顶层键）。
- **FORBID「不改枚举」**：未触任何枚举。
- 复用既有单一入口：`elementToJson/elementFromJson`（序列化）、`fitCellText/titleBlockCellAt`（单元格适配）、`titleBlockFields()`（key↔label）、`geo::elementBounds`（包围盒）——无第二套实现。
- 原子写用 `QSaveFile::commit()`（工程约束：Windows/MinGW 禁手写临时文件 + rename）。

---

## 5. 自测覆盖（`t_splitplot`，全部断言）

1. **按图层分组**：2 组、成员与保序正确、图元守恒（3 条）；
2. **按图框分组**：3 组（帧-1 含交叠区元素=首个包含者优先、帧-2、框外不丢弃）；无帧输入 → 全部归框外（3 条）；
3. **子档写出 + 独立打开**：4 组（零件A/零件B/图框/标题栏）全部写出无错；逐档验证 JSON 可解析 + `elementFromJson` 往返 + id 集合与分组一致（**可独立打开**）；除 `elements` 外顶层键与源逐键相等（**格式零改动**）；幽灵 id → 跳过该组 + 明确错误、其余组照常（3 条）；
4. **自动排图**：5 框全部放入；成对不重叠；不越界；利用率 ∈ (0,1]；确定性（同输入两次逐位一致）；利用率 = Σ面积/占用包围盒；退化：超纸张/空表/非法尺寸 → false + 原因；贴边单框利用率 = 1（8 条）；
5. **批量签名**：key 通道 3 字段命中（设计/审核/批准）且内容/elemId 正确；label 通道（「图纸名称」→ drawingName）；updates 经 Quiet API 落模；空值跳过不擦除；未知栏 → unmatched；空签名表 → 空操作；`titleBlockFieldElems` 提取（7 条）。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 `EXIT=0`；全量 `SketchCAD` 链接确认。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| `tests/splitplot_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_splitplot()` + `CHECK` 注册） | P1~P8 已据此偏离；项目基线为单文件聚合自测。 |
| 「按选定元素子集调 **writeDrawing**」 | 子档 = 源根深拷贝 + 仅替换 `elements`，经 `elementToJson` 组装、`QSaveFile` 原子写 | `writeDrawing` 是 MainWindow 的 UI 层方法（按扩展名路由）；无头工具层直接组装 JSON 会造第二序列化点（历史踩坑）。本实现与 `toDrawingBytes()` 同构，UI 层仍可经既有 writeDrawing 落盘任意格式。 |
| 「按**图层/图框**切分」 | 两种分组均实现；图框分组由**调用方提供帧矩形**（`groupByFrame(elems, frames)`），未做帧自动检测 | 帧自动检测需平面几何重建（图框层线段→矩形），歧义场景多（嵌套帧/网格线误配），违背「绝不猜」原则；UI 层持图框生成上下文，由调用方传矩形最稳。记偏差。 |
| 「自动排图……输出布局」 | 产出摆放坐标（左下角 + 尺寸 + 利用率）；不含旋转 | 旋转涉及标签朝向/装订边等机械图语义，Phase-1 不做（`allowRotate` 字段保留恒 false）。 |
| 「批量签名（图框签名栏填字）」 | 通用 `tb:field:` 填字（key/label 双通道），签名栏=其中 design/review/approve 等字段 | 未硬编码「签名栏」子集——由调用方 fields 表决定填哪些栏，更通用且免维护第二份栏目清单。 |

**已知限制（Phase-1 范围）**
- 子档保留源图全部其余顶层键：BOM/FIELD 按 id 的引用在子档中可能悬空（读档有 ### 回落，不崩）；如需「纯元素子档」可由调用方自行裁剪根对象后再传入。
- 图框分组无自动检测（见偏差表）；交叠区确定性归**前**帧（frames 顺序即优先级）。
- 排图无旋转；利用率按占用包围盒口径（非纸张口径）；不处理图框内已含边距的扣除（由调用方给净尺寸）。
- 签名为纯文本填写：不改单元格格式/字体；适配失败（网格无效）时保留原锚点。

---

## 7. 验收结论

- ✅ 真实功能扩充：`BatchTools.{h,cpp}`（分组 + 原子写子档 + shelf 装箱 + 批量签名），全部纯函数、无头可测。
- ✅ 拆分 ≥5 断言（实际 9 条）：按图层/按图框分组、图元守恒、**子档可独立打开**（JSON 解析 + elementFromJson 往返 + id 集合一致）、格式零改动（顶层键逐键比对）、坏组容错。
- ✅ 排图 ≥5 断言（实际 8 条）：不重叠（成对判定）、不越界、利用率 ∈ (0,1] 且口径正确、确定性、退化三分支（超纸张/空表/非法尺寸）。
- ✅ 批量签名：key/label 双通道、单元格适配复用单一入口、空值不擦除、unmatched 防漏签、Quiet API 可落模。
- ✅ FORBID：存档格式零改动（子档与 `toDrawingBytes()` 同构）；枚举零改动；原子写遵循工程约束。
- ⏳ 后续（已列入偏差）：帧矩形自动检测、排图旋转、纯元素子档模式、签名栏专用预设。
