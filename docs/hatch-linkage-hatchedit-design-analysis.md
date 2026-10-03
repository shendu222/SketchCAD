# SketchCAD 填充边界随几何变更自动更新 + HATCHEDIT：设计与深思

> 配套任务：让剖面填充（Hatch）在其边界被改动后自动跟随重算，并提供 `hatchedit` 命令行，
> 消除"改图后填充错位出图"事故。
> 范围：① Hatch 记录边界来源图元 id 集合（关联填充）；② 变更联动：边界几何变更时（applyChanges
> 等出口挂钩）复用 Boundary 引擎重算边界内填充，边界消失/不成环则标记失效并提示；
> ③ `hatchedit` 命令行：选填充 → 改图案/比例/角度/关联开关。
> 约束：「就地改字段、不重建 Hatch」既有范式与 19 种材料语义不动；联动更新走**单步撤销**
> （改边界 + 填充重算 = 一个编辑步）；关联默认开、可断开（detach 回到静态填充）；
> 性能：边界重算限仅受影响 Hatch。
> 验收：拖动边界 → 填充随动断言；删边界 → 失效提示断言；hatchedit 改比例单步撤销还原；
> 自测新增 ≥10 断言（实际 **+28**，468 → 496）；零警告；`build.sh test` EXIT=0。

---

## 1. 问题本质：填充边界被"消费"了，联动无从挂载

本工程既有的填充流程（`makeSectionChanges` / `makeSectionChangeForPick`）在生成 Hatch 时，
会把**构成边界的源图元消费掉**——第一个独占源图元被原地替换为 Hatch，其余独占源合并删除。
这意味着填充完成后，**边界已不复存在**，自然谈不上"边界变更后填充跟随"。

主流 CAD（AutoCAD）的**关联填充（associative hatch）** 正是为此设计：填充保留其边界对象
（多段线/边界），二者独立共存；边界改动时填充按边界实时重算。

因此本任务的核心不是"加一个刷新按钮"，而是**引入一种保留边界的填充形态**（关联填充），
并为其建立"边界 → 填充"的单向联动。

---

## 2. 数据模型：五个"只增不改"字段

在 `Element` 的剖面区域专用字段区**追加**五个字段（不触碰既有 `rotation`/`radius`/
`sectionMaterial`/`sectionExempt` 四字段的语义）：

| 字段 | 类型 | 含义 |
|------|------|------|
| `hatchSrcIds` | `std::vector<int>` | 边界来源图元 id 集合（空 = 静态/非关联） |
| `hatchPattern` | `QString` | PAT 图案名覆盖（空 = 按 `sectionMaterial` 取默认） |
| `hatchScale` | `double = 1.0` | 图案比例倍数（作用于间距） |
| `hatchAssociative` | `bool = true` | 关联开关（默认开） |
| `hatchInvalid` | `bool = false` | 边界失效标记 |

序列化遵循既有"**旧档缺键 → 回落默认**"范式（`elementToJson`/`elementFromJson`），
默认组合 `无来源 / 无覆盖 / scale=1 / 关联开 / 未失效` 语义等价于**静态填充**，
故旧档逐字节兼容、渲染结果不变。

### 2.1 为什么不重映射 `DynParam` 式的索引

块动态参数 `affected` 存的是块内图元**索引**，增删图元会越界；本填充的来源用 **id 集合**
而非索引，从根本上避免了"增删后索引错位"这一类 bug——id 不会因其它图元增删而失效。

---

## 3. 联动核心：`refreshHatchBoundaries(changedIds)`

### 3.1 算法（复用 Boundary 引擎）

```
对每个 Hatch（且 hatchAssociative && !hatchSrcIds.empty()）：     ← 性能约束
    若 hatchSrcIds ∩ changedIds == ∅ → 跳过（仅重算受影响 Hatch）
    取当前存活的来源图元 → rebuildHatchLoops：
        buildClosedLoopsFromElements(来源)          ← 复用 Boundary 引擎串闭合环
        + 补回 Boundary 图元自带的 holes 环           ← 引擎只登记其外环
        外环 = 面积最大者；内孔 = 被外环包含的其余环（射线法 pointInRing 判定）
    成功 → 就地改写该 Hatch 的 pts / holes，hatchInvalid=false       ← 不重建 Hatch
    失败 → hatchInvalid=true，保留最后几何，log 警告提示
```

关键取舍：

- **复用 `buildClosedLoopsFromElements`（而非 `buildSectionLoopSet`）**：前者只做"串环"不
  做剖面细分，语义正好是"把边界拼成闭合区域"；后者带 `subdivide` 会把区域再拆环，
  对"重算边界"反而不对。
- **就地改 `pts`/`holes`，不重建 Hatch**：`sectionMaterial`(19 值)/`exempt`/颜色/图层/线宽/
  图案覆盖等字段全部原样保留，与"就地改 4 字段"范式一脉相承。
- **外环/内孔分类**：来源可能同时给出多个环（如 Boundary 带孔、或多线围成带孔区域），
  取面积最大者为外环、被包含者为内孔，保证 even-odd 掏空正确。

### 3.2 挂钩点（几何变更出口）

| 出口 | 处理 |
|------|------|
| `modify::applyChanges` | 收集 `touchedIds`（被改/被删的既有 id），落盘后调 `refreshHatchBoundaries(touchedIds)` |
| `SketchModel::moveHandles` | 直接拖动路径（绕过 applyChanges），移动后按 `moved` ids 刷新 |
| `SketchModel::removeElement` | 删除后按被删 id 刷新（→ 标记失效） |

**单步撤销**：`refreshHatchBoundaries` 自身**不压撤销栈**。`applyChanges` 的既有范式是
"调用方先 `beginEdit()` 压一次快照 → 一批 `*Quiet` 静默写入"。填充重算发生在该快照**之后**，
故它与边界改动同属那一个快照 → 一次 Ctrl+Z 边界与填充**一起回退**（自测②已断言）。
`removeElement` 自身先 `pushMemento` 再删，联动失效标记发生在其后，同样单步回退。

### 3.3 性能：只重算受影响 Hatch

三重收敛：
1. 外层只遍历"关联且有来源"的 Hatch；
2. 内层先用 `hatchSrcIds ∩ changedIds` 快速判定是否受影响，不受影响直接跳过（**不做几何重算**）；
3. 即使受影响，也只重解析该 Hatch 的来源（`buildClosedLoopsFromElements` 作用于一个小集合）。

---

## 4. 关联填充的创建：保留边界

既有填充会消费边界，故关联填充需一个**保留边界图元本体**的独立入口：

```cpp
int createAssociativeHatch(const std::vector<int>& srcIds, const SectionStyle& style);
```

- 先按当前边界试解析（`rebuildHatchLoops`），**不成环则拒绝**（不产生"一出生就失效"的填充）；
- 组装 Hatch（`pts`=外环、`holes`=内孔、style 四字段），并记 `hatchSrcIds` + `hatchAssociative=true`；
- `addElement` 入模型（自带一次撤销快照）。

另有 `associateHatchBoundary(hatchId, srcIds)`：把**已存在**的静态填充就地升级为关联填充
（同样先试解析，不成环则拒绝）。两者都只 `pushMemento` 一次。

---

## 5. `hatchedit` 命令行

```
hatchedit                          查询选中填充的 图案/比例/角度/关联(+失效态)
hatchedit scale   <倍数>           改图案比例（>0；作用于间距）
hatchedit pattern <名|none|->      改 PAT 图案覆盖（none/- = 清除覆盖，按材料默认）
hatchedit angle   <度>             改剖面线方向角（就地改 rotation）
hatchedit assoc   on|off           关联开关（off = 断开，回到静态填充）
```

实现遵循既有「就地改字段、不重建 Hatch」范式：**一次 `beginEdit` + 逐个 `updateElementQuiet`**
（`scale`/`pattern`/`assoc` 走 `setHatchPatternOverride`，`angle` 直接改 `rotation`），
故一批选中填充的参数修改 = **一次 Ctrl+Z**（自测⑤断言 `hatchedit scale 2.0` 后单步撤销回到 1.0）。

> `hatchedit` 与既有 GUI「剖面填充参数」对话框（`MainWindow::sectionFillParams`）共用同一
> 就地改写范式；前者补上了**图案名/比例/关联**这三项 GUI 尚未覆盖的维度，并提供命令行入口。

### 5.1 关联开关的可逆语义

`assoc off` 时**保留** `hatchSrcIds`，只把 `hatchAssociative` 置 false——
`refreshHatchBoundaries` 首行即 `if (!hatchAssociative) continue`，故 off 期间边界再变也不随动
（= 静态填充）；再 `assoc on` 即恢复随动，**无需重新登记边界**。这让"断开/恢复"成为真正的
可逆切换，而非破坏性操作。

---

## 6. 渲染联动：让 `hatchScale` / `hatchPattern` 真正生效

字段不能只存不生效，否则 `hatchedit` 是"假功能"。经 `SectionStyle` 中转：

- `SectionStyle` 追加 `patNameOverride`（非空 = 用户显式覆盖，优先于材料默认）；
- `hatchRegionFromElement` 把 `Element::hatchPattern → style.patNameOverride`、
  `Element::hatchScale → 间距倍数`（`spacingMm = base * hatchScale`）；
- `hatchPatternFor` 末尾：`patNameOverride` 非空则优先采用，否则回落 `patNameForMaterial(material)`。

既有"材料 → 图案"映射（19 值语义）完全未动，只是多了一条**优先级更高的用户覆盖**通道。

---

## 7. 自测（`t_hatchLinkage`，+28 断言，468 → 496）

`tests/SketchSelfTest.cpp::t_hatchLinkage()`（`void`，逐条 `CHECK`）。边界统一用
**闭合矩形多段线**（拖顶点/改形后仍闭合，故能验证"随动"）与**4 线矩形**（验证"拖断即失效"）：

| 组 | 锁定的不变式 |
|----|------------|
| ① 关联创建 | 记录来源 / 关联默认开 / 面积=80 |
| ② 改边界随动（applyChanges） | 面积 80→160；**单步撤销后边界与填充一起回退到 80** |
| ②b 拖顶点随动（moveHandles） | 顶点拖到 (20,0) → 面积 80→120（另一条几何出口） |
| ③ 删边界失效 | `hatchInvalid=true`；**保留最后几何**（不丢形状） |
| ③b 多线拖断失效 | 4 线矩形单边平移 → 不成环 → 失效 |
| ④ assoc off 静态 | 边界改了但填充面积不变 |
| ⑤ hatchedit 撤销 | `scale 2.0` 生效；**单步撤销回 1.0** |
| ⑥ 序列化往返 | 五字段往返一致；旧档缺键回落默认（关联开/无来源/scale=1/未失效） |
| ⑦ 渲染参数 | `hatchScale=2 → 间距 3→6`；`hatchPattern → patNameOverride` 覆盖材料默认 |

> ② 的"单步撤销一起回退"与 ⑤ 的"单步撤销还原"分别锁定"改边界+重算"和"改参数"两类编辑的
> **单步撤销**语义（约束条款）。

---

## 8. 踩坑与经验

1. **测试用闭合多段线而非多线矩形做"随动"**：单独平移 4 线矩形的一条边会让它与上下边**脱开**、
   不再闭合——此时正确的行为是"失效"而非"随动"。用闭合多段线（改形后仍闭合）才能验证随动；
   另配 ③b 专门验证"拖断即失效"。这最初是我第一版测试的误判（2 条断言红），修正后全绿。
2. **联动重算不压撤销栈**：`refreshHatchBoundaries` 必须只做静默写入，撤销完全依赖调用方
   `beginEdit` 的那一个快照——否则会多出一步撤销（"改边界要按两次 Ctrl+Z"）。
3. **`createAssociativeHatch` 不可 `pushMemento` + `addElement` 连用**：`addElement` 自带快照，
   两者并用会双步撤销。此处只调 `addElement`。
4. **选 `buildClosedLoopsFromElements` 而非 `buildSectionLoopSet`**：后者带 `subdivide` 会把区域
   再拆环（为"剖切分区"设计），对"重算边界"是过度处理甚至会挑错外环。
5. **改多线矩形的测试期望要先判闭合性**：AutoCAD 关联填充在边界被拖断时同样会提示失效，
   本实现以 `hatchInvalid` + 日志提示对齐该语义。

---

## 9. 变更清单

**修改文件**
- `src/core/Element.h` — Hatch 追加 5 个关联/图案字段
- `src/core/Element.cpp` — 序列化（只增键 + 缺键回落默认）
- `src/core/Section.h` — `SectionStyle` 追加 `patNameOverride`
- `src/core/Section.cpp` — `hatchRegionFromElement` 映射 scale/pattern；`hatchPatternFor` 优先覆盖
- `src/core/SketchModel.h/.cpp` — `createAssociativeHatch`/`associateHatchBoundary`/
  `detachHatchBoundary`/`refreshHatchBoundaries`/`setHatchPatternOverride`；
  `rebuildHatchLoops`/`pointInRing`/`ringArea` 辅助；`removeElement` 挂钩
- `src/core/Modify.cpp` — `applyChanges` 收集 `touchedIds` 并调用联动
- `src/core/Commands.cpp` — 注册 `hatchedit` 命令
- `tests/SketchSelfTest.cpp` — `t_hatchLinkage()` + 注册

**验收结果**
- 自测 **496 / 0**（基线 468，+28）
- `SketchCAD` / `SketchSelfTest` 双目标 **零警告零错误**
- `build.sh test` **EXIT = 0**
