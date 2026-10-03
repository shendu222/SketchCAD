# 任务 G 深思过程：关联部件成组操作（GROUP）与大型装配选择管理（FILTER 预选器）

> 日期：2026-10-03
> 范围：GROUP 命令行五子命令 + 编组整体选中语义（Ctrl 穿透）；FILTER 预选器（作用于随后一次选择操作）。
> 验收：编组移动一次撤销整组还原 ✅；穿透单选与整组切换断言 ✅；FILTER×fence 组合用例 ✅；
> 自测新增 ≥8 断言（实际 +44，总数 528→572）✅；零警告 ✅；`build.sh test` EXIT=0 ✅。

---

## 一、现状盘点：任务下达时「已有 vs 需补」

探查结论：**编组的模型层地基在早前任务中已经完整落地**，任务 G 的真实工作量在
「命令行 + 选择语义 + 预选器」三层接线，而非重造编组系统。

| 能力 | 现状 | 处置 |
|---|---|---|
| `Element.groupName`（引用式编组，随图元 JSON 存档） | ✅ 已有 | 不动（唯一真相源） |
| 模型层 `createGroup/addToGroup/removeFromGroup/ungroup/renameGroup/selectGroup` | ✅ 已有 | 增补 Xref 守卫 |
| `GroupManagerDialog`（GUI 编组管理器） | ✅ 已有 | 不动 |
| `group` 命令行 | ❌ 无 | **本次新增**（五子命令） |
| 点击拾取的编组整体选中 / Ctrl 穿透 | ❌ 无 | **本次新增**（挂接 SelectTool） |
| FILTER 命名过滤器保存/复用（`FilterDialog` + `savedFilters`） | ✅ 已有 | 不动 |
| FILTER **预选器**（约束随后一次选择） | ❌ 无 | **本次新增** |
| QSelect 引擎（`qselect/qselectByDims/matchFilter`） | ✅ 已有 | **API 一行未改**（约束达成） |
| `ShapeSelect`（fence/wp/cp 纯几何判定） | ✅ 已有 | 直接复用 |

## 二、设计决策与取舍

### 决策 1：编组真相源维持「groupName 引用式」，不建独立编组表

任务原文写「命名编组表（随图存档）」。存在两条路线：

- A. 新建 `groups_` 名表（名 → id 集合），随图存档；
- B. 维持既有引用式：图元 `groupName` 字段即编组关系，序列化随图元走。

选 **B**，理由：
1. **无 id 悬空问题**：图元删除/复制后编组关系自动跟随；独立 id 表需要级联清理。
2. **「随图存档」天然成立**：groupName 已随图元 JSON 落盘（空串不写盘），旧档缺键回落未编组——
   与 DimStyle 表、文字样式表同款「引用式范式」，本项目已三次验证。
3. **撤销语义天然正确**：编组变更 = 改写 groupName 字段 → Memento 整图元向量快照 →
   一次 Ctrl+Z 整组回退，与其它编辑完全一致。

「命名编组表」的需求实质是「**可查询、可列举、可整组操作**」——`groupNames()` 去重升序 +
`elementsInGroup()` 已提供等价能力，无需物理表。

### 决策 2：编组整体选中语义挂在「普通点击」分支，Ctrl/Shift 分支零改动

`SelectTool::onPress` 三分支（Tools.cpp）：

```
Ctrl/Shift 点击 → toggleSelection(hit)   【不动：加选/减选语义不变，天然即「穿透」】
普通点击已选中 → 平移预备态              【不动：整组已在选择集，拖动即整组平移】
普通点击未选中 → setSelected(hit)        【改：expandPickToGroup + applyPickFilterOnce】
```

关键洞察：**「Ctrl 单选可穿透编组」不需要新机制**——既有 Ctrl+点击 = toggle 单选切换，
对编组成员天然只选中单个成员。把扩展逻辑放进普通点击分支，约束「选择循环/加选语义不变」
零成本达成：Ctrl+点击重叠区的选择循环（cycleSelection）路径一行未动。

扩展语义收敛为纯函数 `SketchModel::expandPickToGroup(id, additive)`（core 层，headless 可测）：

- `additive=true`（Ctrl/Shift 修饰）→ `{id}`（穿透）；
- `groupName` 为空 → `{id}`；
- 否则 → 该组**全部可见成员**（保持图元遍历序；隐藏成员不参与选择，与框选口径一致）。

**只查不改**：扩展返回 id 集合，选择集落盘仍走 `setSelection`，不给模型层引入隐藏副作用。

### 决策 3：FILTER 预选器 = 瞬态会话配置，「用后即清」

任务原文「作用于随后一次选择操作」。与既有 `savedFilters`（持久化命名过滤器）对比：

| | savedFilters（已有） | activePickFilter（新增） |
|---|---|---|
| 生命周期 | 随图存档（顶层 JSON） | 瞬态，用后即清 |
| 作用方式 | 立即对全图执行一次筛选 | 约束**下一次**拾取/框选/栏选 |
| 入撤销栈 | 否 | 否（同为应用级配置） |

消费口收敛为单一方法 `applyPickFilterOnce(candidates)`：

- 无预选器 → 原样返回（**不清除**，无日志）——保证无预选器时代码路径与旧行为逐字节一致；
- 有 → 仅保留可见且 `matchFilter` 命中的候选（保持原顺序）→ 清除 → 日志播报命中数。

**复用 QSelect 引擎**：过滤判定直接调 `matchFilter`（纯函数），预选器的四维条件构造与
`qselectByDims` 同款（Type/Layer/Color/LineStyle → Eq 条件）。约束「QSelect 引擎 API 不改」达成。

三处结算口接线（全部「无预选器时路径不变」）：

1. **点击**（SelectTool）：`applyPickFilterOnce(expandPickToGroup(hit, false))`；
2. **框选**（mouseReleaseEvent 结算）：候选循环（elementBoundsForIndex 判定，与核心层同源）
   后过滤；索引关闭但有预选器时也走同一候选循环，保证开/关索引结果一致；
3. **栏选/圈围/圈交**（finalizeShapePick）：`shape::pick` 命中集后过滤；形状退化（未构成
   有效范围）不算一次选择操作，**不消费**预选器。

### 决策 4：Xref 边界禁止放在模型层守卫，而非 UI 提示

约束「编组跨 Xref 边界禁止（本图内限定）」。Xref 块引用的成员资格属于外部图纸，本图编组
若圈占 Xref 引用，外部图纸更新后编组语义即悬空。守卫放 `createGroup/addToGroup` 入口：

- 纯 Xref 选择 → 直接拒绝（false / 0）；
- 混合选择 → **剔除** Xref 引用、其余正常编组 + warn 日志（不中断用户操作流）；
- 普通块引用是本图对象，**允许**编组。

判定 helper：`e.type == BlockRef && findBlockDef(e.blockName)->isXref`。放在模型层（而非
命令行层）保证 GUI（GroupManagerDialog）、命令行、脚本三条入口同规则，单一真相源。

## 三、踩坑与经验

### 坑 1：测试 fence 几何「看着穿过、实则不相交」

FILTER×fence 用例首版：直线 (0,0)-(10,0)，圆心 (5,3) r=1，栏线 (-1,0)→(11,6.6)。直觉上
「栏线从直线起点附近出发往右上方，应该穿过两者」。实际：

- 栏线与 y=0 仅在起点 (-1,0) 相交——**在线段范围 [0,10] 之外**，直线未命中；
- 圆心到栏线距离 0.266 < r=1，圆命中。

断言 `hits.size()==2` 失败。修正：栏线 (-1,3)→(11,-1)，在 x=8 处穿过直线段、且过圆心 (5,1)。
**经验**：fence 用例的几何必须逐点验证相交性，「起点落在直线上」不等于「穿过线段」。

### 坑 2：unused variable 在测试代码里同样零容忍

FILTER⑥ 中圆的 id 变量仅用于构造、未参与断言 → `-Wunused-variable`。双目标零警告的
纪律下，测试代码与产品代码同一标准：改为不接收返回值（`addCircle(m, ...)` 裸调用）。

### 经验 1：「作用于一次」语义的实现关键在「无预选器时零开销、零扰动」

`applyPickFilterOnce` 的三态设计（无→原样返回不清除；有→过滤+清除+日志）使三处结算口
都只需一行接线，且不改变任何既有行为——这是「选择循环/加选语义不变」约束的低风险达成方式。

### 经验 2：穿透语义复用既有修饰键行为，优于新增机制

Ctrl+点击在本工程已是「toggle 单选」（含重叠区选择循环），对编组成员天然穿透。
新增机制（如专门的「穿透模式」）反而与既有语义冲突。设计前先盘点既有交互矩阵，常常能省掉一个特性。

## 四、八步执行对照（新增图元类型清单的变体：本次无新 ElementType）

| 步骤 | 内容 | 状态 |
|---|---|---|
| 1 | 模型层：`expandPickToGroup` / 预选器四方法 / Xref 守卫 | ✅ SketchModel.h/.cpp |
| 2 | 序列化：groupName 已随图元 JSON（空串不写盘、缺键回落），无新键 | ✅ 既有 + 往返断言 |
| 3 | 命令行：`group create/add/remove/list/ungroup` + `filter 四维/查看/none` | ✅ Commands.cpp |
| 4 | UI 接线：SelectTool 点击扩展；框选/栏选结算套预选器 | ✅ Tools.cpp / GraphicsView.cpp |
| 5 | GUI 补充：FilterDialog「设为预选器（下一次拾取生效）」按钮 | ✅ FilterDialog |
| 6 | 渲染/导出：无新图元类型，无需同步 | ✅ 不适用 |
| 7 | 自测：`t_groupFilter()` 44 条 CHECK（8 组用例） | ✅ 572/0 |
| 8 | 文档 + 记忆 | ✅ 本文 + .workbuddy/memory |

## 五、验收对照表

| 验收项 | 要求 | 实测 |
|---|---|---|
| 编组移动一次撤销整组还原 | selectGroup → translateSelection → undo 整组回退 | ✅ GROUP②（直线/圆同时还原） |
| 穿透单选与整组切换断言 | expandPickToGroup 两态 | ✅ GROUP①（整组 2 项 / Ctrl 穿透 1 项） |
| FILTER×fence 组合用例 | shape::pick 命中 2 → 预选器过滤留 1 | ✅ FILTER⑥ |
| 自测新增 ≥8 断言 | — | ✅ 新增 44 条，总数 528→572，0 失败 |
| 零警告 | -Wall -Wextra 双目标 | ✅ 0 warning / 0 error |
| 全量回归 | build.sh test EXIT=0 | ✅ EXIT=0 |
| 约束：不改图元 id 与层归属 | 编组只写 groupName 字段 | ✅ GROUP② 层归属断言 |
| 约束：跨 Xref 边界禁止 | 模型层守卫 | ✅ GROUP④（纯 Xref 拒绝 / 混合剔除） |
| 约束：QSelect 引擎 API 不改 | matchFilter 复用，零签名变更 | ✅ |
| 约束：选择循环/加选语义不变 | Ctrl/Shift 分支与 cycleSelection 零改动 | ✅ |

## 六、用例清单（t_groupFilter，44 条 CHECK）

- **GROUP①** 编组创建 / groupNames / 成员数 / 普通点击整组扩展 / Ctrl 穿透 / 非编组不扩展（8）
- **GROUP②** selectGroup 整组入选 / 平移生效 / 一次 undo 直线还原 / 圆还原 / 层归属不变（5）
- **GROUP③** 命令行 create/add/list/remove/ungroup / groupName 清空 / 编组消失 / 未知子命令被拒（9）
- **GROUP④** Xref 块表创建 / 引用插入 / 纯 Xref 拒绝 / 混合剔除后入组（4 + 前置 2）
- **FILTER⑤** 预选器设置 / 三候选过滤留一直线 / 用后即清 / 清除后放行（4 + 前置 1）
- **FILTER⑥** fence 命中 2 / 过滤留 1 / 结算选择集（3）
- **FILTER⑦** type 设置 / 查看 / none 清除 / 组合条件 / 非法参数被拒（7）
- **GROUP⑧** groupName 往返一致 / 旧档缺键回落未编组（2）
