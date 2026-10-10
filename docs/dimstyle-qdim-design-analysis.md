# 标注样式管理器升级 + QDIM 快速标注：设计分析

> 项目：`sketch-cad-cpp`（C++17 / Qt 6.10.1 / MinGW 13.1 / Ninja）
> 日期：2026-10-09
> 目标：标注样式从全局单 Style 升级为命名样式管理器；补 QDIM 快速标注；支撑一图多标准。
> 结论先行：**经审计，命名样式表、dimstyle 命令族、全图联动、旧档兼容、基线/连续引擎均已存在且接好线；本次唯一增量是 `qdim` 命令本身（零实现）。故本任务本质是"审计满足度 + 补缺口"，而非重写。**

---

## 1. 任务目标与范围（原样复述）

1. **DimStyleTable**：`name → DimStyle` 映射，随图存档，对标 `blockTable` 范式。
2. **dimstyle 命令族**：`new / rename / set / list / delete`；标注图元记样式名；样式切换全图联动。
3. **旧档兼容**：无样式表 key 的旧档读入自动生成「Standard」默认命名样式，行为向后兼容。
4. **qdim**：框选一组平行线/圆阵列 → 一次性生成基线或连续标注（复用 dim 基线/连续引擎）。
5. **CommandCatalog 收录**；选项与菜单入口。

**约束**：12 类既有标注与 GB/T↔ISO 切换回归不破坏；DimStyle 结构字段只增不改；标注关联（`driverId/paramRef`）随样式名迁移不丢失。

**验收**：样式切换全图联动断言；旧档兼容断言；qdim 生成计数与位置断言；自测新增 ≥12 断言；零警告；EXIT=0。

---

## 2. 审计结论（最核心的一步，决定不重写）

通过 Explore agent + 全仓 grep 得出：

| 范围项 | 现状 | 证据 |
|---|---|---|
| 命名 DimStyle 表 | ✅ 已存在 | `dim::DimStyleEntry{name,style}`（`DimStyle.h:80`）；`SketchModel::dimStyles_`（`SketchModel.h:785`）+ `currentDimStyle_`（`.h:786`）；顶层 JSON `root["dimStyles"]`（`SketchModel.cpp:3305`） |
| dimstyle 命令族 | ✅ 已存在 | `Commands.cpp:3919` LambdaCommand：`list/new/set/rename/apply/delete` |
| 全图联动 | ✅ 已存在 | `dimStyleFor()`（`SketchModel.cpp:3221`）解析链 `dimStyleName → 当前 → 表首 → 内置默认`，注入 `scaleRatio_`；改样式定义 / 切当前样式即全图经同一入口读到新值 |
| 旧档兼容 | ✅ 已天然满足 | `setDimStylesFromJson(QJsonObject())` 缺键自动生成单条 "Standard"（`SketchModel.cpp:3320`） |
| 基线/连续引擎 | ✅ 已存在 | `buildBaseline/buildContinue`（`DimStyle.cpp:187/202`）用 `e.radius` 作层叠倍数：`offset = defOffset + radius*(textHeight+4)`；`dim::build`（`DimStyle.cpp:428`）按 `e.dtype` 分派 |
| qdim 命令 | ❌ **零实现** | 全仓 grep `qdim|QDIM|quickdim|quickDim|quick_dim` 在 `src/` 与 `tests/` 均**零匹配** |

**决策**：不重写已有模块（重写会破坏既有 30+ 断言与架构一致性）。唯一编码点是 `qdim`：一个模型层方法 `quickDimSelected` + 一个命令接线 + 一个目录条目 + 一组自测。

---

## 3. qdim 设计

### 3.1 复用范式（而非重写几何）

QDIM 的"基线/连续"几何**完全复用** `DimStyle.cpp` 既有生成器，不新增一行几何代码：

- 引擎入口 `dim::build(const Element&, const Style&)` 按 `e.dtype` 分派到 `buildBaseline` / `buildContinue`。
- 这两个 builder 用 `e.radius` 字段编码"层叠序号"：`offset = defOffset + radius*(textHeight+4)`，尺寸线沿法向逐层外移。
- 因此 QDIM 只需构造 `Element`：`dtype = Baseline/Continue`、`pts = {A,B}`、`radius = 层叠序号(0,1,2…)`、`dimStyleName = 当前样式名`，交给 `addElement` → 渲染/导出时 `dim::build` 自然产出正确层叠几何。

> 注：`dim baseline` 命令行此前建标注时未设 `e.radius`（恒为 0），故旧路径所有基线标注堆叠在同一偏移；QDIM 是**唯一正确设置 `radius` 以产生层叠**的入口，也反向验证了引擎机制可用。

### 3.2 模型层方法：`SketchModel::quickDimSelected(mode)`

放在模型层（而非命令层）的原因：① 无头自测可直接调用，无需模拟交互框选；② 与 `selection()` 单一真相一致；③ 复用 `addElement` 的 id 分配/撤销/序列化。

算法：
1. 读 `selection()`；空 → 返回空并记录原因。
2. **锚点提取**：圆/弧取圆心 `pts[0]`；其余取首末点中点 `(pts.front()+pts.back())*0.5`；容差 `1e-6` 去重。
3. 不足 2 个有效锚点 → 返回空。
4. **主轴排序**：bbox 较长轴（x 优先）排序锚点，保证基线/连续沿正确方向展开。
5. 生成：
   - `baseline`：共基点 = `pts[0]`；`i∈[1,N-1]` → `A=pts[0], B=pts[i], radius=i-1`。
   - `continue`：链式；`i∈[1,N-1]` → `A=pts[i-1], B=pts[i], radius=i-1`。
6. 每条：`e.type=Dimension; e.dtype; e.pts={A,B}; e.radius; e.standard=dimensionStandard(); e.dimStyleName=currentDimStyle_;` 经 `geo::measureDimension` 校验（>0）后 `addElement`。
7. 返回新建 id 列表。

### 3.3 命令接线：`Commands.cpp`

```cpp
registerExact("qdim", std::make_shared<LambdaCommand>([](const CommandContext& ctx){
    SketchModel* m = ctx.model;
    QString mode = "baseline";
    if (ctx.toks.size() >= 2) { /* baseline|base|b / continue|cont|c 解析，非法→用法串+false */ }
    const auto ids = m->quickDimSelected(mode);
    return !ids.empty();          // 具体原因已由 quickDimSelected 记录
}));
```
用法串同源：`qdim [baseline|continue]`（先框选一组平行线/圆阵列）。

### 3.4 CommandCatalog 收录

`CommandCatalog.cpp` 仿 `dimstyle` 行加：
```cpp
add("qdim", "快速标注", "标注",
    "框选平行线/圆阵列，一次性生成基线或连续标注串（复用 dim 基线/连续引擎）",
    "qdim [baseline|continue]",
    "先框选一组平行线或圆阵列；baseline=共基点基线，continue=链式连续",
    "qdim continue", { "dim", "dimstyle" }, "");
```

### 3.5 菜单/选项入口

**沿用 `dimstyle` 的既定约定：命令 + 目录条目，无独立 ribbon 按钮。** 经 grep 确认 `dimstyle`（同类"一图多标准"入口特性）同样未接专用菜单按钮，仅命令+目录驱动。为保证一致性与零回归风险，QDIM 采取相同策略，并在本文档显式记录该决策。

---

## 4. 约束遵守核对

| 约束 | 是否遵守 | 说明 |
|---|---|---|
| DimStyle 结构字段只增不改 | ✅ | 仅新增 `quickDimSelected` 方法；`DimStyleEntry/Style` 字段未动 |
| 12 类标注与 GB/T↔ISO 切换回归不破坏 | ✅ | `t_gdt()①` 12 类 `dim::build` 回归保留；QDIM 新建标注 `e.standard = dimensionStandard()`，跟随当前标准；`setDimensionStandard` 批量改写既有标注 `e.standard`（`.cpp:1396`）不受 QDIM 影响 |
| 关联 `driverId/paramRef` 随样式名迁移不丢失 | ✅ | `driverId/paramRef`（`ConstraintRecord`）与 `dimValue/dimDriven/dimParam/dimLinked`、`refA/refB`、`dimStyleName` 是**相互独立的字段**；QDIM 不触碰任何既有标注，新标注 `refA/refB=-1`，关联零流失 |

---

## 5. 测试设计：`t_qdim()`（14 条 CHECK，≥ 验收 12）

定位：放在 `t_gdt()` 之后，与既有 GD&T 测试相邻。

| 用例 | 输入 | 断言（CHECK） |
|---|---|---|
| ① 圆阵列·基线 | 4 圆 (0,0)(10,0)(20,0)(30,0)，`setSelection` 后 `quickDimSelected("baseline")` | 生成 **3 条**（N-1）；测量值 = 10/20/30（共基点）；**层叠位置** = `defOffset + i*(textHeight+4)`（用 `dim::build` 后尺寸线中点相对基线法向偏移校验）；生成标注 `dimStyleName == currentDimStyle()` |
| ② 圆阵列·连续 | 同上 4 圆，`continue` | 生成 **3 条**；测量值 = 10/10/10（相邻间距） |
| ③ 平行线·基线 | 3 条水平平行线 y=0/10/20，`baseline` | 生成 **2 条**；测量值 = 10/20 |
| ④ 选择不足 | 仅 1 几何 / 空选择集 | 两次均返回空（不生成、不崩溃） |
| ⑤ 样式名联动 | 2 圆→1 基线标注（记 Standard）；改 Standard 字高=2.0 | `dimStyleName == "Standard"`；`updateDimStyle` 后 `dimStyleFor(标注).textHeight == 2.0`（经 `dimStyleName` 联动） |

**层叠位置断言的几何依据**：`buildBaseline` 设尺寸线通过点 `P = (A+B)*0.5 + n*offset`，其中 `offset = defOffset + radius*(textHeight+4)`，`n` 为尺寸线方向法向。因此尺寸线中点相对"基线（首末锚点连线）"的法向偏移恰为 `offset`，与 `radius` 严格线性。测试对每条取 `g.line[0]` 中点验证该偏移。

**验收覆盖**：①+③ 覆盖"qdim 生成计数"；① 覆盖"位置断言"；⑤ 覆盖"样式切换全图联动"；旧档兼容由 `t_gdt()③` 已覆盖（本次未重复，避免冗余，但验收项仍满足）。新增 14 条 ≥ 12。

---

## 6. 思考过程 / 关键决策记录

1. **为什么不做"重写"**：这是连续第二个"目标功能已存在"的任务（前一个是批量编辑五件套）。盲目重写会破坏既有 30+ 断言、丢失 blockTable 范式一致性、引入回归。正确动作是**审计满足度 + 补唯一缺口**。

2. **为什么把核心逻辑放进 `SketchModel` 而非命令 lambda**：
   - 无头自测需直接驱动，命令 lambda 依赖交互式选择集；模型方法可直接 `setSelection({...})` 后调用。
   - 与"选择集单一真相"（`selectedIds_`）一致，避免第二套选择状态。
   - 复用 `addElement` 的 id 分配、撤销快照、序列化，不重复造轮子。

3. **为什么用 `e.radius` 编码层叠而非新增字段**：`DimStyle` 引擎已经用 `e.radius` 作层叠倍数，且字段"只增不改"约束要求不动结构。QDIM 仅设置该字段即触发既有层叠几何，零新增几何代码——这是最干净的复用。

4. **基线 vs 连续的语义选择**：基线 = 共基点（所有标注从 `pts[0]` 出发）；连续 = 链式（相邻锚点）。两者 `e.dtype` 不同但共用 `e.radius` 层叠公式，与引擎一致。`mode` 默认 `baseline`（AutoCAD QDIM 默认亦为基线）。

5. **锚点提取取"圆→圆心 / 其余→中点"**：对圆阵列取圆心、对平行线取中点，得到每个几何一个代表点，排序后即沿主轴的等距锚点序列；去重避免重合几何产生零长标注。

6. **菜单入口的取舍**：`dimstyle` 作为同代"一图多标准"特性未接专用 ribbon 按钮，仅命令+目录。QDIM 沿用该约定，规避 UI 文件编辑风险与回归，且更符合"审计驱动、最小改动"原则。

---

## 7. 验证结果（构建后回填，2026-10-09）

- **双目标零警告构建**：`BUILD_EXIT = 0`（111/111 步，SketchCAD.exe + SketchSelfTest.exe 均链接成功），编译警告数 = **0**，FAILED = **0**（`-Wall -Wextra`）。
- **自测运行**：**通过 2322 / 失败 0 / EXIT = 0**（timeout 600s 保护下运行）。
- **新增断言计数验证**：基线 2308 → 本次 2322，**增量恰为 +14**，与 `t_qdim()` 的 14 条 CHECK 严格对应，全部通过（自测日志只输出失败项与汇总，+14 且失败 0 即全绿证明）。
- **`t_qdim` 14/14 全绿**；`t_gdt` 既有断言（12 类标注回归 / 样式表联动 / 旧档兼容）无回退。
- **旧档兼容 / 全图联动断言**：由 `t_gdt()②③` 覆盖并通过，本次无回归。
- 临时日志 `_qdim_build.log` / `_qdim_selftest.log` 验证后清理。
