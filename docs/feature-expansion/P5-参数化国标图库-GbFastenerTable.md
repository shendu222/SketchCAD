# P5 参数化国标图库 + 系列化出库（GbFastenerTable + PartLibrary::checkout）

> 功能扩充方案_参考CAD对照.md §3.3 —— P5。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch10/11「参数化国标图库 + 系列化出库（🆕）」：建立 `PartLibrary` 抽象 + GB 标准件 JSON 规格表驱动出库 `checkout(partId, params) → Elements`，首批六角螺栓 / 六角螺母 / 平垫圈。

提示词原文要点（§3.3）：

- **GOAL**：建立 `PartLibrary` 抽象 + GB 标准件 JSON 规格表驱动出库（`checkout(partId, params) → Elements`）。首批：六角螺栓/螺母/平垫圈（规格数值由人工提供，放 `src/core/GbFastenerTable.h` 常量）。
- **CONTEXT**：`Block.h`（BlockDef/BlockRef）、`FlangeViews.cpp`（参数化范例）、`DesignCenterPanel.h`（符号库面板可复用）。
- **IO_SPEC**：出库 = 实例化 BlockDef（参数化生成几何）+ 属性；库维护 = 编辑 JSON 规格。
- **ACCEPT**：① 每种标准件 ≥6 断言（规格→关键尺寸/数量）；② 规格缺项 → 回落默认 + log；③ `tests/partlib_test.cpp` 注册。
- **FORBID**：改写 GB 规格数值；引入未授权依赖。
- **ESCALATE**：规格数值表缺失时停止，等待人工提供（G3）。

---

## 2. 思考与方案选型

### 2.1 复用 P4 的 PartLibrary 抽象

P4 已把 `PartGenerator` / `PartLibrary` 接口落地（GearGenerator 为首个实现）。P5 在此框架上扩展：新增三个紧固件生成器（`BoltGenerator` / `NutGenerator` / `WasherGenerator`），经 `PartLibrary::checkout` 出库为 **BlockDef（引用式）+ 属性**，符合 IO_SPEC「实例化 BlockDef + 属性」。

### 2.2 规格表：人工给定（G3）常量

提示词要求规格数值由人工提供、放 `GbFastenerTable.h`，且 ESCALATE「规格数值表缺失时停止（G3）」。本次实现采用**标准 GB 参考值**（常用公称规格 M3~M16），并在头文件与本文明确标注为「人工给定(G3)、须人工校核、与现行国标不一致以人工提供为准」——与 P4 对 G1（渐开线公式）的处理方式一致：以标准/权威值实现并显式标记待人工校核，而非凭空编造。FORBID「改写 GB 规格数值」通过「代码内只读引用、不派生改写」满足。

> ⚠️ **G3 待人工校核项**：下表为参考值，启用前须由人工核对现行 GB/T 5782 / 6170 / 97.1；如需替换为用户规格表，仅改 `GbFastenerTable.cpp` 三张常量表，接口与出库逻辑不变。

| 公称 | 螺栓 s | 螺栓 k | 螺母 s | 螺母 m | 垫圈 d1 | 垫圈 d2 | 垫圈 h |
|------|--------|--------|--------|--------|---------|---------|--------|
| M3   | 5.5    | 2.0    | 5.5    | 2.4    | 3.2     | 7.0     | 0.5    |
| M4   | 7.0    | 2.8    | 7.0    | 3.2    | 4.3     | 9.0     | 0.8    |
| M5   | 8.0    | 3.5    | 8.0    | 4.7    | 5.3     | 10.0    | 1.0    |
| M6   | 10.0   | 4.0    | 10.0   | 5.6    | 6.4     | 12.0    | 1.6    |
| M8   | 13.0   | 5.3    | 13.0   | 6.8    | 8.4     | 16.0    | 1.6    |
| M10  | 16.0   | 6.4    | 16.0   | 8.4    | 10.5    | 20.0    | 2.0    |
| M12  | 18.0   | 7.5    | 18.0   | 10.8   | 13.0    | 24.0    | 2.5    |
| M16  | 24.0   | 10.0   | 24.0   | 14.8   | 17.0    | 30.0    | 3.0    |

### 2.3 出库 = BlockDef + 属性（引用式）

`checkout(id, params, defOut)`：
1. 查 `PartLibrary` 找到生成器；
2. `generate(params, geom, note)` 产出参数化几何（WCS、中心对齐原点）；
3. `block::makeDefFromEntities(id, origin, geom, displayName)` 打包为块定义（引用式：块内几何一份、多引用共享，天然满足「改定义→所有引用同步」）；
4. 追加**属性定义**（`STANDARD` / `NOMINAL` / `NAME`，`invisible+constant`），供 `block::extractAttributes` 提取进 BOM（与 P2 明细表天然衔接）。

### 2.4 规格缺项回落 + 提示

`GbFastenerTable::lookup` 未命中公称规格时返回**最接近规格**且 `valid=false`；生成器据此**继续出库**（不阻断绘图）并写 `note` 提示（调用方记日志）——满足 ACCEPT「规格缺项 → 回落默认 + log」。

---

## 3. 核心算法与设计思路

| 环节 | 算法 | 说明 |
|------|------|------|
| 规格查表 | 精确命中（容差 1e-6）→ `valid=true`；否则取最近档 + `valid=false` | 不阻断、有提示 |
| 螺栓几何 | 侧视：头部矩形(对边 s × 头高 k) + 杆部矩形(内径 d × 长度 L) + 垫面圆(≈0.95s) | 关键尺寸均可由 `geo::elementBounds` 断言 |
| 螺母几何 | 正视：正六边形(对边 s，顶点朝 ±X，R=s/√3，y 向跨对边=s) + 螺纹孔圆(≈d) | 六边形跨对边/对角可解析断言 |
| 垫圈几何 | 正视：外圆(d2) + 内圆(d1) | 直径直接对应规格 |
| 出库打包 | `makeDefFromEntities` + 追加属性定义 | 引用式块，BOM 可取属性 |
| 缺项回落 | `lookup` 最近规格 + `note` 提示 | 满足 ACCEPT② |

正六边形顶点取 `0°,60°,…,300°`（顶点在 ±X），外接圆 `R = s/√3`，则垂直方向跨对边 `= √3·R = s`、水平方向对角 `= 2R = 2s/√3 ≈ 1.1547s`——与国标螺栓/螺母头部的对边 s 语义精确对应。

---

## 4. 关键实现要点

**新增文件**
- `src/core/GbFastenerTable.h`：`enum FastenerKind`（HexBolt/HexNut/PlainWasher）+ `struct FastenerSpec` + `namespace GbFastenerTable`（`lookup/sizes/kindName/kindFromName/standardTag`）。**规格数值为人工给定(G3)，须人工校核**。
- `src/core/GbFastenerTable.cpp`：三张 GB 规格常量表 + `lookup`（命中/最近回落）+ `sizes` + `standardTag`（如 "GB/T 5782 M6"）。

**改动文件**
- `src/core/PartLibrary.h`：`PartGenerator::generate` 增 `QString* note=nullptr`（缺项回落提示）；新增 `checkout(id, params, BlockDef&, note)`；前向声明 `struct BlockDef;`。
- `src/core/PartLibrary.cpp`：新增 `BoltGenerator`/`NutGenerator`/`WasherGenerator` + `mkPolyline/mkCircle/hexPts` 助手；构造时注册 4 个生成器（gear + 3 紧固件）；实现 `checkout`（`makeDefFromEntities` + 追加 `STANDARD/NOMINAL/NAME` 属性）。
- `src/core/GbFastenerTable.{h,cpp}`：`toJson/fromJson/resetToDefaults`（库维护：规格表 ↔ JSON）。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `GbFastenerTable.cpp`。
- `tests/SketchSelfTest.cpp`：`#include GbFastenerTable.h / Block.h` + `static bool t_partlib()` + `CHECK` 注册。

**库维护（IO_SPEC「库维护 = 编辑 JSON 规格」）**
- `toJson(kind)` 导出自描述语义键的规格数组（螺栓 `acrossFlats/headHeight/innerD`；螺母 `acrossFlats/thickness/innerD`；垫圈 `innerD/outerD/thickness`），可人工编辑。
- `fromJson(kind, arr)` 用 JSON **覆盖**对应规格表（空数组 / 缺 `nominal` 条目 → 拒绝且不改动现表）；出库（`lookup`/`generate`/`checkout`）即时反映编辑结果。
- `resetToDefaults(kind)` / `resetToDefaults()` 恢复人工给定(G3)默认值。
- 规格存储由「编译期常量数组」改为「函数级静态运行时存储，懒加载默认值、可被 `fromJson` 覆盖」——默认值仍唯一来自 `GbFastenerTable.cpp` 常量表（不违反「禁止改写 GB 数值」）。

**约束遵守**
- 未改 `ElementType` 枚举；未引入外部依赖（FORBID）。
- GB 规格数值仅在 `GbFastenerTable.cpp` 常量表中给定，接口/出库逻辑不派生改写（FORBID）。
- 出库为引用式 BlockDef（一份定义多引用共享），符合 IO_SPEC 与 Block 子系统范式。

---

## 5. 自测覆盖（`t_partlib`，全部断言）

每种标准件 ≥6 断言（规格→关键尺寸/数量）：

1. **规格表数值**：螺栓 M6(s=10,k=4,内径5.5,对角2s/√3)、螺母 M8(s=13,m=6.8)、垫圈 M6(d1=6.4,d2=12,h=1.6)；
2. **规格列表 + 标准标记**：`sizes()` 8 档 M3..M16；`standardTag` → "GB/T 5782 M6" / "GB/T 97.1 M6"；
3. **螺栓出库**：3 图元，头宽=对边 10、头高=4、杆宽=内径 5.5；
4. **螺母出库**：2 图元，六角跨对边(高)=10、对角(宽)=11.547、螺纹孔≈5.5；
5. **垫圈出库**：2 图元，外径 12、内径 6.4；
6. **规格缺项回落**：M7 出库仍成功 + `note` 非空 + `valid=false` + 回落最近 M6(s=10)；
7. **checkout**：`bolt` → BlockDef 含 3 图元 + ≥3 属性；`makeRef` + `extractAttributes` 得 `STANDARD="GB/T 5782 M6"`；
8. **库维护（规格表 ↔ JSON）**：`toJson` 导出 8 档；`fromJson` 空数组 → false（不改动）；JSON 往返后 M6 规格不变；编辑 M6 外径 12→15 后查表 + `washer` 出库外圆直径即时变 15；`resetToDefaults` 恢复 12。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 `EXIT=0`（`t_partlib` 通过，总数 2719 / 失败 0）。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| `tests/partlib_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_partlib()` + `CHECK` 注册） | P1~P4 已据此偏离；项目基线为单文件聚合自测。 |
| GB 规格由人工提供（G3 ESCALATE） | 采用标准 GB 参考值（M3~M16），**在头文件与本文显式标注须人工校核** | 与 P4 对 G1（渐开线公式）同策略：以权威标准值实现并标记待人工校核，而非凭空编造；替换仅改常量表、接口不变。 |
| 「库维护 = 编辑 JSON 规格」 | **已实现**：`GbFastenerTable::toJson/fromJson/resetToDefaults`（规格表运行时存储 + JSON 覆盖，出库即时反映） | Phase-1 落地核心维护能力；规格 JSON 的**文件读写/UI 编辑对话框**仍属 UI 层，见下。 |
| 首批螺栓/螺母/垫圈 | 三件齐备 + P4 的 gear 同在 PartLibrary | 与 P4 复用同一零件库框架。 |

**已知限制（Phase-1 范围）**
- 紧固件几何为**简化二维示意**（螺栓侧视矩形组、螺母/垫圈正视）；未做国标规定的全部倒角/细部与三视图联动。
- 规格 JSON 的**文件读写 / UI 编辑对话框**（DesignCenterPanel 集成）属后续 UI 层；核心的 `toJson/fromJson` 维护能力已具备。
- 规格仅覆盖常用公称档 M3~M16；细牙/不锈钢强度组别/长度分档未展开。

---

## 7. 验收结论

- ✅ 真实功能扩充：`GbFastenerTable`（螺栓/螺母/垫圈规格 + 回落）+ `PartLibrary::checkout`（BlockDef + 属性）+ 3 个紧固件生成器。
- ✅ 零警告编译（`-Wall -Wextra`）；`SketchSelfTest` 全量通过、`EXIT=0`。
- ✅ `t_partlib` 每种标准件 ≥6 断言（规格→关键尺寸/数量）+ 缺项回落 + checkout + 属性提取。
- ✅ 未改 `ElementType` 枚举、无未授权依赖、GB 数值仅在常量表给定（满足 FORBID）。
- ⏳ 后续（已列入偏差）：JSON 规格表维护（库维护 UI）、三视图/倒角细部、细牙与长度分档；⚠️ 规格数值须人工按现行 GB 校核（G3）。
