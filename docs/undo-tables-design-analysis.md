# 撤销表（Undo Tables）设计分析：让 Ctrl+Z 成为「万能回退」

> 目标：消除「几何回了、配置没回」的不一致，让 Ctrl+Z 在用户心智里等价于"整体回到上一步"。
> 涉及：`src/core/SketchModel.h`（Memento / pushMemento / 各 setXxx）、`src/core/SketchModel.cpp`（beginEdit / undo / redo），
> 外溢接线：`MainWindow`（开档抑制 + 设置消费）、`AppSettings`+`OptionsDialog`（栈深）、`BatchPlotEngine`（批量载入抑制）。
> 日期：2026-10-04

---

## 1. 问题定义

改造前 `Memento` 只快照 10 项：`label / elems / nextId / scale / layers / blocks / textStyles / params / dimStyles / currentDimStyle`。
而模型里还有一批**随图存档**的状态被排除在快照之外：

| 状态 | 是否随图存档 | 改造前是否入撤销栈 |
|---|---|---|
| 布局表 `layouts_` + 当前布局 `currentLayout_` | ✅ 顶层 `layouts`/`currentLayout` | ❌ |
| 单位 `units_` | ✅ 顶层 `units` | ❌（注释明确写了"不入撤销栈"）|
| 图纸界限 `limits_` | ✅ 顶层 `limits` | ❌ |
| 捕捉配置 `snapConfig_` | ✅ 顶层 `snapConfig` | ❌ |
| 追踪配置 `trackConfig_` | ✅ 顶层 `trackConfig` | ❌ |
| 命名过滤器 `savedFilters_` | ✅ 顶层 `savedFilters` | ❌ |
| 外部参照（Xref） | ✅ 随块表 `blocks` | 部分（见 §3.1）|

后果是一个可复现的割裂：用户改单位 → 画线 → Ctrl+Z，线消失了但单位没回来；
改布局 → 移动图元 → Ctrl+Z，图元复位了却仍停在新布局。这正是本次要消除的"半截状态"。

---

## 2. 两类状态的分界（本次的核心判据）

判据不是"这个字段重不重要"，而是：**它属于图纸内容，还是属于使用习惯？**

- **图纸内容**（随图存档、换台机器打开同一张图应当一致）→ 入撤销栈，undo/redo 对称；
- **应用习惯**（AppSettings，见 P1-01）→ **不入栈**，且明确文档化。

据此，单位/界限/捕捉/追踪/布局/过滤器全部归入第一类：它们写在图纸 JSON 里，
是"这张图"的一部分，而非"这个人"的习惯。反过来，栈深上限本身、窗口几何、最近文件、
自动保存间隔等留在 AppSettings，**永不入撤销栈**。

> ⚠️ 一个容易自指的地方：**栈深上限存在 AppSettings 里，但它自己不入栈**。
> 否则"修改栈深"会成为一个可撤销步，撤销它又需要栈……语义自我指涉。这一点在
> `AppSettings.h` 的键注释与 `SketchModel.h` 的访问器注释里都写明了。

---

## 3. Memento 扩列：只追加，不改既有

约束：既有字段的**含义与顺序一字不改**（它们是旧档/旧行为的契约），新字段一律追加到尾部。

```cpp
struct Memento {
    QString label; std::vector<Element> elems; int nextId; double scale = 1.0;
    std::vector<Layer> layers; std::vector<BlockDef> blocks; std::vector<TextStyle> textStyles;
    std::map<QString,double> params; std::vector<dim::DimStyleEntry> dimStyles; QString currentDimStyle;
    // ---- 随图属性（「万能回退」补齐；只追加，不改上方任何字段）----
    std::vector<PlotLayoutDef> layouts;   // 布局表（含各布局完整页面设置）
    QString currentLayout;                // 当前布局名（空 = 模型空间）
    UnitsMeta  units;                     // 单位设置（显示/输入口径）
    LimitsMeta limits;                    // 图纸界限
    snap::SnapConfig  snapConfig;         // 捕捉全局配置
    snap::TrackConfig trackConfig;        // 追踪全局配置
    std::vector<FilterDef> savedFilters;  // 命名过滤器
};
```

### 3.1 Xref 附加表：刻意**不**单列字段

需求列出"Xref 附加表"，但代码事实是：**外部参照本身就是块表记录**（`BlockDef::isXref`
+ `xrefPath/xrefPathType/xrefNested/xrefStatus`，见 `Block.h:89-93` 与 `Xref.h` 的设计说明）。
AutoCAD 语义一致：Xref 是 Block 表的一类记录。

因此 Xref 的完整状态**已被 `blocks` 快照覆盖**。若再追加一个 `xrefs` 字段，就会制造
第二个真相源——两处更新一处漏只是时间问题（本项目在 DXF R2000、writeDrawing root 上
已经为这个模式付过学费）。**结论：不新增字段，改为在自测中直接断言 attach/detach 的
撤销行为经块表生效，并在本文档记录该决策。**

---

## 4. 快照去重：精确性是硬要求，且安全方向必须是单向的

需求："连续两次完全相同状态不重复入栈；单次批量操作仍必须单步撤销"。

### 4.1 为什么不能用"看起来差不多"

去重的失败模式是**不对称**的：

- 判为"不同" → 照常入栈，最坏只是少去重一次（多一步空转的 Ctrl+Z，**无害**）；
- 判为"相同" → 跳过入栈，一旦判错就**永久吞掉用户的一步撤销**（**致命**）。

所以比较器必须是**精确**的，且遵循"无法证明相同即判为不同"。据此否掉了两个看似省事的方案：

- ❌ **只比容器 `size()`**：改图层名、挪一个图元，size 都不变 → 会吞步；
- ❌ **对图元走 JSON 摘要**：`elementToJson` 确实完整，但对 10 万图元是 O(N) 序列化，
  而 `pushMemento` 每次还要复制一份，叠加后编辑会明显卡顿。

### 4.2 采用的方案：分层比较

1. **廉价层**（绝大多数入栈在此分出胜负）：`nextId / scale / currentDimStyle / currentLayout / units / limits / snapConfig / trackConfig / params` + 各容器 `size()`；
2. **大图保护**：`elems.size() > 20000` 时直接判"不同" → 照常入栈。放弃这次优化换取性能，方向安全；
3. **深比较层**：逐图元 `sameElement`（Element 全部 66 个字段逐项比较）、逐条 `sameLayer / sameTextStyle / sameBlockDef / sameFilter`；
4. **标注样式 / 布局**：字段极多但条数是个位数，改走它们**自己的** `toJson()` 比较——
   序列化是这批结构的完整定义（漏字段 = 存盘丢数据，本就是必须为零的缺陷），故 JSON 全等 ⟺ 结构全等。

双精度用 `==` 精确比较是安全的：待比较的两份快照是同一份数据的副本链，未改动字段必然逐位相同；
而"改过但值恰好相等"的情形已被各 setter 的幂等守卫（`if (u == units_) return;`）挡住。

> 维护约定：`Element` 字段"只增不改"，**新增字段时必须同步 `sameElement`**，
> 否则"只改了新字段"的两次快照会被误判为相同。这条写进了函数上方的注释。

### 4.3 与既有 beginEdit 契约的关系

"单次批量操作仍是一步"由调用方 `beginEdit()` + 逐个 `*Quiet` 写入保证，**与去重无关**；
去重只处理"压栈时状态与栈顶逐字段相同"这一种情况，两者互不干扰。

---

## 5. 栈深：默认 100，可配，超限丢弃最旧

- 默认 **100 步**（原先硬编码 200）；合法区间 1..1000，非法/越界回落默认 100；
- 下限取 1 而非 0：0 步等于关掉撤销，会让 Ctrl+Z 变成"什么都没发生"，比留 1 步更难理解；
- **undo / redo 双栈同限**：redo 栈在连续撤销时会增长，不同限会无界；
- 调小上限后**立即收敛**（`setUndoDepthLimit` 里当场丢弃最旧），不留超限残骸；
- 配置项落在 `general/undoDepth`（AppSettings 规格表，唯一数据源），选项对话框「常规」页新增 SpinBox，
  经 `MainWindow::applySettingLive` 灌入 `model_->setUndoDepthLimit()`。

---

## 6. 开档路径：必须抑制，否则"打开"被拆成多步

这是本次**最容易踩的坑**。改造前 `setUnits/setLimits/setSnapConfig/setTrackConfig` 不入栈，
开档调用它们毫无副作用；改造后它们各自入栈，而开档代码本身已经建立了唯一编辑步：

```cpp
model_->clear();
model_->beginEdit(QStringLiteral("打开草图"));   // ← 唯一的编辑步
model_->addElementsQuiet(loaded);
...
model_->setUnits(...);      // ← 改造后各自压栈 → 一次「打开」被拆成 4 步 Ctrl+Z
```

解法是 RAII 抑制作用域 `SketchModel::UndoGuard`：构造置起 `undoSuppressed_`、析构**恢复原值**（嵌套安全），
`pushMemento` 开头直接返回。抑制范围**严格限于那四行**，其余调用（含 `setScaleRatioValue` / `setLtscale`）
保持原样，因此**开档的撤销步数与旧版完全一致**，不触碰"既有行为不变"的红线。

`BatchPlotEngine`（COM 批量出图，同样 `beginEdit("自动化打开")` 后灌表）同步加抑制。

---

## 7. 各入口入栈一览

| 入口 | 是否入栈 | 说明 |
|---|---|---|
| `setUnits` / `setLimits` | ✅ | 幂等守卫在前：值没变则不入栈、不发信号 |
| `setSnapConfig` / `setTrackConfig` | ✅ | 同上 |
| `addLayout` / `removeLayout` / `renameLayout` / `updateLayout` | ✅ | 布局是随图内容，与图层/块表同级 |
| `setCurrentLayout` | ❌（仅参与快照）| 见下 |
| `saveFilter` / `deleteFilter` | ✅ | 检索口径属图纸内容 |
| Xref `attachXref` / `detachXref` | ✅（经块表）| 原本就压栈，未改动 |
| AppSettings 各键 | ❌ | 应用习惯，见 §2 |

**`setCurrentLayout` 刻意不单独入栈**：它只改"当前看哪一张"，属视图焦点而非图纸内容；
若为它压栈，Ctrl+Z 会退化成"来回翻页签"。但它**仍参与快照**（`currentLayout` 字段），
因此撤销任何一步时当前布局都随快照回到当时——不会出现"布局表回来了、页签还停在新布局"的错位。

---

## 8. undo / redo 的对称与信号

- 恢复顺序：先算差异（下面 `std::move` 会掏空 `m`，之后再比就晚了），再压回滚快照，最后落值；
- 新字段与几何**同源抄录**，保证 redo 与 undo 严格互逆；
- 只在**真变化**时发信号（`layoutsChanged / unitsChanged / limitsChanged / snapConfigChanged / trackConfigChanged`），
  与既有 `scaleChanged` 的处理一致，避免撤销引发无谓的面板重绘；
- 过滤器表随快照回退，但它没有专用变更信号（既有 `saveFilter/deleteFilter` 也从不发信号，
  过滤器管理界面在打开时读取），故不额外广播，与既有行为一致。

---

## 9. 自测 `t_undoTables`（44 条 CHECK）

每类表「改一次 → undo → 值还原 → redo → 值恢复」：

| 分组 | CHECK 数 | 要点 |
|---|---|---|
| 单位 / 界限 / 捕捉 / 追踪 | 3×4 = 12 | 改后生效、undo 还原、redo 恢复 |
| 布局表（含当前布局） | 7 | 新增 → 切页签 → undo 后布局归零且回落模型空间 → redo 恢复 |
| 命名过滤器 | 4 | 保存/undo 清空/redo 恢复 |
| 快照去重 | 2 | 连续两次 `beginEdit` 且期间无改动 → 栈大小不变 |
| 栈深上限 | 7 | 设为 5 → 加 12 个点 → 栈只留 5 步 → 只能撤到第 7 个点；非法 0 / 越界均回落 100 |
| 混合用例 | 9 | 改布局 + 移动图元 + 改单位 → 三次 Ctrl+Z 全还原 → 三次 Ctrl+Y 全恢复 |
| 几何回归 | 3 | 删除/撤销/重做不受本次改动影响 |

栈深用例的算术（可复核）：`addElement` 是"先压快照再添加"，12 次后栈顶 5 张为
`S7..S11`（分别含 7..11 个图元），撤 5 次后停在第 7 个点，与断言一致。

---

## 10. 验证结果

| 项目 | 结果 |
|---|---|
| 编译（SketchCAD / SketchSelfTest / SketchBenchmark，`-Wall -Wextra`） | **零警告**，`BUILD_EXIT=0` |
| `SketchSelfTest.exe` | **通过 890 / 失败 0**（改造前基线 845，新增 44 条 + 1 条注册）|
| `SketchBenchmark.exe --selftest`（P0-04 视图级） | `P004 SELFTEST: ALL PASS (fail=0)` |
| 既有断言 | 全部仍通过（filter/group/layout/mview/存档兼容/黄金样本无回归）|

> 构建插曲：`./build.sh test` 走默认高并行度时，`PropertyPanel.cpp` 被编译进程杀死
> （无编译器诊断，属资源耗尽，非代码错误）。改用 `cmake --build build --target ... -j 1`
> 增量构建 1m28s 通过。这条与既有经验（"长构建用 `-j 1` 防 OOM"）一致，已补记入项目记忆。

---

## 11. 改动边界核对

- ✅ Memento 既有字段含义与顺序未改，新字段只追加到尾部；
- ✅ 旧档读取路径（`setLayoutsFromJson` / `setSavedFiltersFromJson` / `UnitsMeta::fromJson` 等）一字未改；
- ✅ 随图存档字段照旧，快照只是多了几项；
- ✅ 未把任何 AppSettings 键混入撤销栈（栈深上限只是"被读取的配置值"，不是被快照的状态）；
- ✅ 未改 `ElementType` 枚举、未改任何命令名；
- ✅ 开档/批量载入的撤销步数与旧版一致（抑制范围严格受限）。

## 12. 遗留与后续

- 去重在 >20000 图元时主动放弃（性能换安全），属有意取舍；若后续需要覆盖大图，
  可考虑给 `elems_` 引入"改动序号"计数器，避免在热路径上做 O(N) 比较。
- `setCurrentLayout` 不产生独立撤销步是产品取舍：若用户反馈"切了布局后 Ctrl+Z 期望先退回上一个布局"，
  可改为入栈（一行 `pushMemento`），但会让翻页签污染历史。
- 选项对话框的栈深改动**立即生效**（走 `applySettingLive`），不做二次确认；与既有开关类设置行为一致。
