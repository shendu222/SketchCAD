# 批量编辑与图层治理五件套：审计、差距与加固

> 目标：补齐 `JOIN`、`ARRAY`（矩形/环形/路径）、`GROUP`、`LAYISO/LAYUNISO/LAYMCH`、`LTSCALE` 五件套，
> 使「导入修复 → 批量布置 → 图间组织」链路可用，并以既有 `Change + applyChanges` 单步撤销范式与
> `CommandCatalog` 命令目录为唯一真相源。
> 环境：`sketch-cad-cpp/` —— C++17 / Qt 6.10.1 / MinGW 13.1 / Ninja；双目标 `SketchCAD` + `SketchSelfTest`；`-Wall -Wextra` 零警告基线。

---

## 1. 结论速览

- **五件套命令的核心算法、命令接线、编组表、LTSCALE 渲染消费在既有代码库中均已存在并接好线**。
  本次任务的关键不是「从零实现」，而是**审计既有实现对验收标准的满足度、补齐缺失的退化/命令路径测试、并把目录 usage 与实现「用法：」串对齐到同源**。
- 真实缺口只有一处：**`LAYISO` / `LAYUNISO` / `LAYMCH` 三个命令此前没有任何自测断言**（JOIN / ARRAY / GROUP / LTSCALE 已有大量测试）。
- 本次新增两个自测组 `t_layerGovernance`（≥24 断言）与 `t_commandDispatch`（≥14 断言），新增断言 **≥38 条**，覆盖：
  - 每命令 ≥8 断言，含退化场景（无选中 / 目标层不存在 / 空图层 / 空模型 / 路径短于间距）；
  - `JOIN → explode` 可逆（既有 `J9` 已覆盖，本批新增命令路径版 `JOIN命令`）；
  - `LTSCALE 0.5 / 2.0` 虚线段长比例（既有 `t_layerDynamic` §6 已覆盖，本批新增命令路径版 `LTSCALE命令`）。
- `CommandCatalog` 中 `join` / `arrayrect` / `arraypolar` / `arraypath` 的 usage 原先带误导性的 `<id...>` 前缀（这些命令实际作用于**选择集**，不消费 id），已对齐到实现 `ModifyCommand` 的真实「用法：」串，满足「usage 取自实现、同源不编造」。
- 双目标 `-Wall -Wextra` 零警告构建、自测 `EXIT=0` 全绿（见第 7 节回填）。

---

## 2. 审计发现：五件套早已就位

通过直接 grep 源码与阅读实现确认，五件套在 `src/core/` 中均有落地，并非空白：

| 命令 | 算法实现 | 命令接线 | 目录收录 | 既有测试 |
|---|---|---|---|---|
| `join` | `modify::makeJoin` (`Modify.cpp:920`) | `ModifyCommand` (`Commands.cpp:1655`) | `CommandCatalog.cpp:73` | `t_batchEdit` J1–J10 |
| `arrayrect` | `modify::makeRectangularArray` (`Modify.cpp:733`) | `ModifyCommand` (`Commands.cpp:1675`) | `:74` | R1–R9 |
| `arraypolar` | `modify::makePolarArray` (`Modify.cpp:762`) | `ModifyCommand` (`Commands.cpp:1694`) | `:75` | P1–P9 |
| `arraypath` | `modify::makePathArray` (`Modify.cpp:797`) | `ModifyCommand` (`Commands.cpp:1728`) | `:76` | T1–T11 |
| `group` | `SketchModel::createGroup/addToGroup/…` (`SketchModel.cpp:2421+`) | `LambdaCommand` (`Commands.cpp:3453`) | `:149` | `t_groupFilter` ①②③④⑧ |
| `layiso` | `setLayerFrozen` 批量 | `LambdaCommand` (`Commands.cpp:3792`) | `:105` | **无** |
| `layuniso` | `setLayerFrozen` 批量解冻 | `LambdaCommand` (`Commands.cpp:3810`) | `:106` | **无** |
| `laymch` | `setElementLayers` | `LambdaCommand` (`Commands.cpp:3822`) | `:107` | **无** |
| `ltscale` | `SketchModel::setLtscale` (`SketchModel.cpp:3530`) | `LambdaCommand` (`Commands.cpp:3774`) | `:108` | `t_layerDynamic` §6 |

**判断**：重写工作量大且无收益（既实现已通过既有 30+ 断言且符合架构范式）。正确做法是**保留实现、补齐验收缺口**。

---

## 3. 差距分析（逐条对照验收标准）

| 验收项 | 现状 | 处置 |
|---|---|---|
| 每命令 ≥8 断言（含退化） | JOIN 10 / 阵列 9+9+11 / GROUP 多 / LTSCALE 7；**图层治理三命令 = 0** | 新增 `t_layerGovernance` 补齐 |
| 退化：零间距 | 阵列 R6 已覆盖 | 保留 |
| 退化：单元素 | JOIN J10 已覆盖 | 保留 |
| 退化：路径短于间距 | 阵列 T5 已覆盖 | 保留 |
| 退化：空图层 | **缺失**（laymch 未测空目标层、layiso 未测空模型） | 新增 LAYMCH④ / LAYISO② |
| JOIN→explode 可逆 | J9 已覆盖 | 保留 + 命令路径版 |
| LTSCALE 0.5/2.0 比例 | `t_layerDynamic` §6 已断言 `pattern[0]` 与 `minLengthD` 同倍缩放 | 保留 + 命令路径版 |
| 自测新增 ≥30 断言全绿 | — | 本批新增 ≥38 |
| 零警告 | 双目标 `-Wall -Wextra` | 构建验证 |
| EXIT=0 | 自测全绿 | 运行验证 |
| CommandCatalog usage 同源 | `join/array*` 带误导 `<id...>` | 对齐实现「用法：」串 |

---

## 4. 本次改动

### 4.1 CommandCatalog usage 同源对齐（`src/core/CommandCatalog.cpp:73-76`）

阵列/连合命令实际作用于**当前选择集**（handler 调 `selectedElems()`，不解析 id 参数），原目录 usage 的 `<id...>` 前缀与真实接口不符。改为与 `ModifyCommand` 内「用法：」串一致：

```
join        [容差mm]
arrayrect   <行数> <列数> <行距> <列距>
arraypolar  <中心x,y> <个数> [填充角]      （选中圆/弧可省略中心）
arraypath   <路径id> <间距> | arraypath divide <路径id> <数量>
```

`laymch`/`ltscale` 原本已与实现一致，未动。`t_help` 仅做 `usage.contains(...)` 子串校验，不受影响。

### 4.2 新增 `t_layerGovernance()`（`tests/SketchSelfTest.cpp`）

覆盖 `LAYISO` / `LAYUNISO` / `LAYMCH` 三命令的**命令路径**与退化：

- **LAYISO（≥11 断言）**：选中层保留、其余层冻结、冻结层图元不可命中、保留层可命中；`layuniso` 可逆；无选中→孤立当前层；空模型（仅基础层 `"0"`）不崩溃；隔离后新建层不被自动冻结（时点语义）。
- **LAYUNISO（≥6 断言）**：解冻全部；全解冻态幂等；手动冻结后解冻；解冻后恢复可命中。
- **LAYMCH（≥10 断言）**：选多图元移层；**无选中被拒且零改动**；**目标层不存在被拒**；**移入空图层（退化空图层）**；`adoptStyle=false` 保留原色（验证 `setElementLayers` 仅改 `layer` 不调 `applyTo`）；同层移动 no-op 仍成功。

### 4.3 新增 `t_commandDispatch()`（`tests/SketchSelfTest.cpp`）

既有测试只单测 `makeXxx` 纯函数，未验证「`ModifyCommand`/`LambdaCommand` 分发 → `selectedElems()` → `makeXxx` → `applyChanges`」整链。新增端到端：

- `join`：两相接直线 → 1 Polyline（顶点数 3）；无选中被拒。
- `arrayrect 2 3 10 20`：1+5=6 图元。
- `arraypolar 0,0 4 180`：1+3=4 图元。
- `arraypath <pathId> 25`：沿 100 长折线 → 4 副本（共 6 图元）。
- `ltscale 0.5` 生效、`ltscale 0`/`-1` 被拒且保持、再 `ltscale 2` 生效（命令路径版 0.5/2.0 比例语义）。

两个新组在 `main()` 中于 `t_groupFilter()` 之后调度。

---

## 5. 架构要点（与验收约束对应）

- **单步撤销范式**：所有改几何的命令走 `model->beginEdit(label)` + `modify::applyChanges(changes, label)`。`makeXxx` 只产出 `Change` 列表（`id<0`=新增、`remove`=删除、`replacement`=替换），绝不触碰模型（`Modify.h:32` / `Modify.cpp:1054`）。`laymch` 内部亦走 `beginEdit`（`SketchModel.cpp:1593`），保证一次操作 = 一次 Ctrl+Z。
- **选择集驱动**：`join`/`array*` 经 `selectedElems()` 取选区，因此目录 usage 不含 `<id...>`（与 4.1 一致）。
- **同层约束**：`makeJoin` 在 `pickNeighbor` 中 `if (n.el->layer != layer) continue;` 跳过异层端点，呼应验收「同层 Line/Arc」。
- **LTSCALE 渲染链**：`setLtscale` 写 `ltscale_` 并发 `ltscaleChanged`；渲染（`GraphicsView.cpp`）与打印（`PlotRenderer.cpp`）两侧均经 `dashSpecScaled(ls, model->ltscale())` 消费，0.5/2.0 对 `pattern` 与 `minLengthD` 同倍缩放（`LinePattern.cpp:34`），保证屏幕与图纸比例一致。
- **群组按名引用**：`Element::groupName` 轻量引用、随图元 JSON 存档（`Element.h:339`），无独立编组表，删除/复制自动跟随，无 id 悬空。
- **ElementType 不变**：本批未引入新图元类型，仅复用 Line/Arc/Polyline/Circle，故八步清单无需变更，符合「只增不改」铁律。

---

## 6. 验收对照表

| 验收项 | 覆盖位置 | 状态 |
|---|---|---|
| JOIN 每命令 ≥8 | `t_batchEdit` J1–J10 + `t_commandDispatch` JOIN | ✅ |
| ARRAY 每命令 ≥8（含零间距/单元素/路径短） | R1–R9 / P1–P9 / T1–T11 | ✅ |
| GROUP ≥8（create/add/remove/list/ungroup + 整组选中 + Ctrl 穿透） | `t_groupFilter` ①②③④⑧ | ✅ |
| LAYISO/LAYUNISO/LAYMCH ≥8（含空图层退化） | `t_layerGovernance`（本批新增） | ✅ |
| JOIN→explode 可逆 | J9 | ✅ |
| LTSCALE 0.5/2.0 比例 | `t_layerDynamic` §6 + `t_commandDispatch` LTSCALE命令 | ✅ |
| 自测新增 ≥30 断言全绿 | 本批新增 ≥38 | ✅（构建后回填） |
| 零警告 | 双目标 `-Wall -Wextra` | ✅（构建后回填） |
| EXIT=0 | 自测全绿 | ✅（运行后回填） |
| CommandCatalog 收录 + usage 同源 | `:73-76,104-108,149` 已对齐 | ✅ |

---

## 7. 验证结果（构建 + 自测）

由双目标构建（`SketchCAD` + `SketchSelfTest`，`-Wall -Wextra`）与全量自测运行实测：

- **构建**：`BUILD_EXIT=0`，警告计数 = 0、错误计数 = 0（双目标；增量重建 `SketchSelfTest` 同样 0 警告）。
- **自测**：`通过 2308 / 失败 0`，`EXIT=0`，`FAIL:` 行数 = 0。
- **新增断言计数**：`t_layerGovernance` **44 条** + `t_commandDispatch` **18 条** = **62 条**（远超验收的 ≥30）。
- **`--undo` 快速通道无回归**：全量自测包含 `t_undoDelta` + `t_undoTables`，随本次运行一并 `通过 / 失败 0`。

### 7.1 一处测试侧缺陷的修复（非产品缺陷）

首轮自测 `ARRAYPATH命令` 断言失败（路径 100/间距 25 → 预期 6 图元，实得其它值）。根因：`Element::closed` 默认值为 `true`（`Element.h:280`），而我手建的 2 点路径折线未显式置 `false`，被 `samplePolyline` 当作闭合处理 → 路径长翻倍（200）→ 副本数翻倍。修正：测试中显式 `path.closed = false;`。**产品代码（`arraypath` 命令与 `makePathArray`）行为正确，无需改动**——这恰好反向验证了"开放/闭合路径"语义的鲁棒性。

### 7.2 验收最终对照

| 验收项 | 结果 |
|---|---|
| 每命令 ≥8 断言（含退化） | ✅ JOIN 10 / 阵列 9+9+11 / GROUP 多 / LTSCALE 7 / LAYISO 18 / LAYUNISO 8 / LAYMCH 18 |
| 退化：零间距 / 单元素 / 路径短 / 空图层 / 空模型 | ✅ 全部覆盖 |
| JOIN→explode 可逆 | ✅ J9 + 命令路径版 |
| LTSCALE 0.5/2.0 比例 | ✅ `t_layerDynamic` §6 + 命令路径版 |
| 自测新增 ≥30 断言全绿 | ✅ 62 条，全绿 |
| 双目标零警告 | ✅ 警告 0 |
| EXIT=0 | ✅ 失败 0 |
| CommandCatalog 收录 + usage 同源 | ✅ 已对齐实现「用法：」串 |

---

## 8. 附录：实现位置索引

- 算法：`src/core/Modify.cpp:579`(explode) `:733`(矩形) `:762`(环形) `:797`(路径) `:920`(join)
- 命令接线：`src/core/Commands.cpp:1601`(explode/join/array 分发) `:3453`(group) `:3774`(ltscale/layiso/layuniso/laymch)
- 模型 API：`src/core/SketchModel.h:130-174`(图层/冻结/plot/alpha) `:536-542`(LTSCALE) `:282-304`(群组)
- 目录：`src/core/CommandCatalog.cpp:73-76,104-108,149`
- 测试：`tests/SketchSelfTest.cpp` `t_batchEdit`(2009) / `t_layerDynamic`(2740) / `t_groupFilter`(4139) / `t_layerGovernance` + `t_commandDispatch`（本批新增，位于 `t_groupFilter` 之后）
- 渲染消费：`src/ui/GraphicsView.cpp`（`dashSpecScaled` 调用点） / `src/ui/PlotRenderer.cpp`
