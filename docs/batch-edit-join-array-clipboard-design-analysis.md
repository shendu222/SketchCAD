# SketchCAD 批量编辑三件套（JOIN / 阵列 / 剪贴板）：设计与深思

> 配套任务：补齐高频批量编辑三件套——通用 JOIN（同层 Line/Arc 连合为 Polyline）、
> 命令行阵列（ARRAYRECT/ARRAYPOLAR/ARRAYPATH）与剪贴板族命令
> （copyclip/pasteclip/pasteblock/copybase），使「导入修复 → 批量布置 → 图间搬运」
> 链路完整可用。
> 范围：Modify 层三个纯函数工厂（makeJoin/makeRectangularArray/makePolarArray/makePathArray）、
> 新增 `src/core/Clipboard.{h,cpp}` 版本化剪贴板载荷、Commands 命令行接入、
> MainWindow 菜单/拾取编排接线（双入口）。
> 约束：全部命令走 Modify 层 Change+applyChanges **单步撤销**范式；不破坏既有选择交互；
> 阵列基点语义对齐 AutoCAD；剪贴板数据版本化，旧版内容忽略且不崩溃。
> 验收：每命令 ≥8 条断言（含零间距、单元素、路径短于间距等退化场景）；
> JOIN 后再 explode 结果可逆；自测合计新增 ≥25 条断言；菜单与命令行双入口；
> 编译零警告；`build.sh test` 退出码 0。

---

## 1. 设计总纲：三个新站点，一条既有流水线

本工程自镜像/剖面引擎以来已确立一条铁律：**一切修改类操作 = `std::vector<Change>` 纯函数产出，
落盘由调用方 `beginEdit(label)` + `applyChanges` 一次完成**。这条流水线带给批量编辑三件套
三个免费性质：

1. **单步撤销天然成立**——所有副本/删除/新增在同一个快照里，一次 `undo()` 全部回退；
2. **命令行与 GUI 共用纯函数**——`makeJoin(src, tol)` 不知道自己被谁调用，
   `Commands.cpp` 与 `MainWindow` 只是两个不同的「组装 Change 的方式」；
3. **测试不需要视图**——`SketchSelfTest` 直接构造 `Element` 数组断言 Change 流，
   无头环境全绿。

因此本设计的核心命题是：

> **三件套不发明新机制，只向 Change 流水线贡献四个新纯函数 + 一个新载荷类型（Payload）。
> 任何「在工厂函数里偷偷落盘」的写法都是对本范式的背叛。**

四层架构如下：

```
┌─ GUI 层（MainWindow）─────────────────────────────────────┐
│  菜单项/快捷键 → 拾取状态机（clipBasePending_/clipPastePending_）│
│  容差口径：worldTol()（= 8px/zoom，真实拾取手感）              │
├─ 命令行层（Commands.cpp）─────────────────────────────────┤
│  join/arrayrect/arraypolar/arraypath/copyclip/copybase/     │
│  pasteclip/pasteblock → 参数解析 → 组装调用                  │
│  容差口径：modify::kJoinDefaultTol（0.5mm 固定值）            │
├─ 核心层 A（Modify.{h,cpp}）── 既有 ─────────────────────────┤
│  makeJoin / makeRectangularArray / makePolarArray /         │
│  makePathArray → std::vector<Change>（纯函数，无副作用）      │
├─ 核心层 B（Clipboard.{h,cpp}）── 新增 ─────────────────────┤
│  Payload{base,elements,blocks} + 版本化 JSON + pasteInto    │
│  （makePasteChanges / makeBlockDefFromPayload 纯函数）       │
└─ 模型层（SketchModel）─────────────────────────────────────┘
   addElementsQuiet / updateElementQuiet / removeElementQuiet /
   addBlockDefQuiet —— 只被 applyChanges 与 pasteInto 调用
```

注意 **核心层 A 与 B 的分工**：Modify 层产出的是「对当前图的修改」，
Clipboard 层承载的是「跨图/跨实例搬运的数据」。JOIN 和阵列只服务当前图，
所以进 Modify；剪贴板必须同时被 copyclip（读图）与 pasteInto（写图）使用，
且要跨进程存活，所以独立成模块。

---

## 2. JOIN 连合：最深的一口井

### 2.1 为什么 Arc 必须离散——Polyline 弧段语义的硬边界

第一直觉是「JOIN 输出的 Polyline 里保留弧段信息」，但被 Polyline 的存储语义挡住了：

> 本工程 Polyline 的弧段是**三相邻点外接圆**语义（`arcFlags[i]` 表示
> 第 i 段由 pts[i-1]、pts[i]、pts[i+1] 三点确定的外接圆弧）。
> 一个独立的 Arc 图元携带（圆心/半径/起止角），它与相邻线段的连接点
> **不保证**落在其外接圆的三点语义可复原的位置上。

具体反例：一段 60° 的 Arc，端点 A、B 与下一段直线在 B 相接。若强行记 arcFlag，
Polyline 反解时需要「三相邻点」重建圆——但 A、B、下一个顶点三点确定的圆
与原 Arc 的圆心/半径只有在极特殊布局下才一致。**信息在写入瞬间就已经失真。**

所以决策是：**Arc 参与连合时按弧长离散为折线顶点**（`joinArcSampleCount`：
弧长每 ~2mm 一点，夹在 [8, 64]），全部并入全直线段 Polyline
（`closed=false`、`arcFlags` 清空、继承链首图元属性）。

这个决策的可信度由**可逆性验收**背书：JOIN 后立刻 explode，
应还原出等顶点数的线段集——自测 J7 断言了这条链路（连合 → explode → 顶点无重复、
分段数正确）。离散有损（弦高 ≈ r·(1−cos(Δθ/2))，2mm 步长下工程精度内不可见），
但**无损的假象比有损的真话更危险**，我们选择把损失做小并写进头文件注释。

### 2.2 贪心链式与 flip 的「首尾相反」语义

算法骨架：把每个候选（Line/Arc、同层）包装成 `JoinNode{el, a, b, used}`，
维护当前链的两个端点（tip），反复 `pickNeighbor` 找端点容差内的未用节点，
直到两端都无法扩展或全表用尽。

**最容易写错的是「翻转」语义。** pickNeighbor 返回的 `bestFlip` 含义是：
「邻居节点的 **b 端**落在当前 tip 上」——即邻居以 b 端朝向链内、a 端朝向链外。
于是：

- **链尾扩展**（在链的末端接新节点）：邻居的 b 端贴着链尾 →
  新顶点序列应取邻居的 **a→b** 方向？不——邻居 b 端与链尾重合，
  接缝点只写一次，后续顶点来自 a 端方向，**flip = bHit**；
- **链首扩展**（在链的头部前面插入新节点）：几何上镜像了上述情形，
  顶点要以反序前插，**flip = !bHit**。

⚠️ 两处 flip 判断**形式上相反**，实现时曾有过「统一成一个变量」的冲动——
这是错的。它们描述的是同一个物理事实（邻居哪端贴链）在**两种插入方向**
（尾追加 vs 头前插）下的不同投影。代码里已加注释警告：
**这两处不要"统一"，统一即引入反序 bug**。

### 2.3 容差双口径：worldTol() 与 kJoinDefaultTol

同一个 JOIN 命令，两个入口的容差来源不同：

| 入口 | 容差 | 理由 |
| --- | --- | --- |
| GUI 菜单「连合」 | `GraphicsView::worldTol()` = `PICK_TOL_PX/zoom` | 与 OSNAP 拾取手感一致：用户看到「两个端点几乎重合」的像素距离不随缩放变化 |
| 命令行/脚本/自测 | `modify::kJoinDefaultTol = 0.5`（mm） | 无视图缩放概念；0.5mm ≈ 1:1 视图下的捕捉容差量级 |

为此把 `worldTol()` 从 GraphicsView 的 private 区**上移到 public**
（原先只有控制点命中逻辑使用）。这是「GUI 能力下沉为公共查询」的小重构：
命令行与 GUI 的语义差异被**显式化成参数**，而不是各自硬编码。

端点重合判定统一走 `geo::almostEqual(p, q, tol)`——不新写容差比较，
杜绝「JOIN 用了自己的 epsilon」这种口径漂移。

### 2.4 产出语义的四个细节

1. **接缝点吸附去重**：链内相邻图元的端点在容差内但不严格相等，
   连合后直接**复用前一节点的坐标**作为接缝顶点，消除微小缝隙
   （否则后续 boundary/hatch 识别会因缝隙失败——JOIN 的核心价值就在此）；
2. **链内 ≥2 才产出**：单节点链不是「连合」，是原图元；
3. **孤立节点归还**：某条链最终只有 1 个成员（其余邻居跨层被滤掉）时，
   该图元**不产任何 Change**——既不删也不增，保持原 id 与属性；
   若 `produced=false` 则 `out.clear()`，保证「失败即零副作用」；
4. **跨层不连**：只认同一图层的图元。跨层连合会把不同线宽/颜色的图元
   压成一个继承链首属性的 Polyline，视觉信息静默丢失——宁可让用户先归层。

---

## 3. 阵列三命令：一个基点范式 + 两套间距语义

### 3.1 统一的「P0 跳过」范式（AutoCAD 基点对齐）

三个阵列命令共享同一条基点语义，与 AutoCAD 一致：

> **第 0 个阵列位置就是原图元所在位置，不生成副本。**
> 矩形阵列中即 `(r,c)==(0,0)`；环形阵列中即旋转角 0°；
> 路径阵列中即路径起点 P0。

因此**新增副本数**：矩形 = rows·cols − 1；环形 = count − 1；路径 = 有效位置数 − 1。
全部副本 `id=-1`（新增语义），**完整继承原图元属性**（图层/颜色/线宽/线型/文本），
只改几何。自测 R6/P9 分别断言了 layer/color/lineStyle 的继承。

这个范式有个反直觉推论：**1×1 矩形阵列、count=1 的环形阵列都产出空 Change 列表**
——不是错误，是「零个新副本」的合法结果（R3/P7 断言）。

### 3.2 矩形阵列：零间距是合法输入

`rows/cols < 1` → 空（参数无效）。但 **rowSpacing/colSpacing = 0 不是无效**：
它产出 rows·cols−1 个**完全重合**的副本（R4 断言 2×3 零间距 → 3 个重合副本）。
理由：与 AutoCAD 行为一致——间距是用户输入，重合是用户可见、可撤销的几何事实；
CAD 引擎不应替用户「纠正」输入，只应拒绝结构上无法执行的参数（行列为 0/负）。

### 3.3 环形阵列：角度同步不是「平移旋转」能白拿的

副本几何 = 原图元绕 center 旋转 `k·angleRangeDeg/count`（k=1..count−1）。
点集旋转是通用的（pts 逐点转），但两类图元有**隐藏的角度字段**：

- **Arc**：起止角必须同步 `+= ang`，否则圆弧形状会「断开」（弧心角区间错了）；
- **Ellipse**：长轴方向 `rotation += ang`，否则椭圆方向不变而点集转了，
  形状自相矛盾。

圆/文本无角度字段（圆各向同性、文本按需求不旋转），随 pts 走即可。
P4 断言：圆弧 8 份 360° 阵列后副本起止角精确 +45°。

### 3.4 路径阵列：Measure/Divide 双语义与「位置表是弧长参数」

`makePathArray(src, pathPts, spacing, count=0)` 的双语义：

| 输入 | 语义 | 行为 |
| --- | --- | --- |
| `spacing > 0` | **Measure**（定距） | 沿路径每 spacing 取一点，直到路径走完；末点若 ≈ 总长则去重 |
| `spacing <= 0 && count >= 2` | **Divide**（定数） | 全长均分 count 段，取 count−1 个内部等分点 |
| 其余 | 退化 | 返回空 |

调用方分工：命令行 `arraypath` 默认 Measure（用户给间距），GUI 路径阵列
对话框同样以间距为主参数；Divide 语义保留给脚本/未来「等分布置」入口。

**⚠️ 本命令贡献了本轮最典型的一个类型错位编译错误**：位置表最初声明为
`std::vector<Vec3> positions`，但实际存的是**弧长参数**（`double`）——
因为路径是「已离散的密集点列」，取点用二分插值 `pointAt(s)`，
真正需要的下游数据是「在哪个弧长处放副本」，不是「世界坐标点」。
改名 `params` 后类型系统立刻接管了正确性。教训写进 3.4 节标题：
**位置表的类型选择本身就是设计决策**——存参数而非坐标，
使得同一条路径在平移/缩放后参数化结果依然有效。

退化矩阵（T 组断言逐条覆盖）：

| 场景 | 期望 |
| --- | --- |
| 路径总长 < 1e-9（单点路径） | 空 |
| Measure：路径长 100、spacing=25 | 副本落点 25/50/75/100（末点=总长，含终点，共 4 个） |
| Measure：路径长 100、spacing=30 | 落点 30/60/90（100 处不足一档不生成），共 3 个 |
| Divide：count=5 | 4 个等分副本 |
| spacing=25 但路径长 10（短于间距） | 空（走不完一档） |
| spacing=0 且 count=1 / 负 spacing 且无 count | 空 |
| L 形折线 spacing=35 | 副本跨折角（弧长参数天然处理折角，落点 (40,30) 精确） |

### 3.5 双源阵列

`src` 是数组而非单元素：选区里 2 个图元阵列 → 2×(rows·cols−1) 个副本
（R5 断言 6 个）。每个源独立继承自身属性——这也是「继承链首/原图元属性」
统一写法的直接收益。

---

## 4. 剪贴板族：版本化载荷 + 双语义粘贴

### 4.1 为什么是 JSON 而不是 QDataStream

自定义 mime `application/x-sketchcad-clip` 上放的是**版本化 JSON 文本**：

```json
{
  "v": 1,
  "base":  {"x": 0, "y": 0},
  "elements": [ /* elementToJson ... */ ],
  "blocks":   [ /* blockDefToJson ... */ ]
}
```

三条理由：

1. **DRY**：与存档序列化共用 `elementToJson/blockDefToJson` 单一实现，
   字段增减不会出现「存档支持、剪贴板忘了同步」的漂移；
2. **可诊断**：文本可读，出问题时能直接看剪贴板内容定位；
3. **跨实例天然成立**：两个 SketchCAD 进程通过系统剪贴板搬文本，
   不需要任何进程间协议。

### 4.2 版本化策略：只认 kVersion，绝不越界解析

`fromJson` 读到 `v != 1`（含缺版本 key）→ 返回 false 并给出原因字符串，
调用方 log 提示后安全退出。**旧版/新版内容忽略但不崩溃**（C2/C3 断言）。
这是硬约束的直接兑现：剪贴板是全局共享资源，任何第三方程序、旧版本实例
都可能往里塞东西，解析器必须把「不认识」当作常态处理。

### 4.3 collectBlockDeps 下沉：命令行与 GUI 共用一份依赖收集

copyclip/copybase 组包时必须把选区里 BlockRef 引用的块定义（含**嵌套依赖**）
一并拷进 payload，否则跨实例粘贴会得到「引用悬空」的块。
该逻辑原本内联在 `Commands.cpp` 的 copyclip 分支（递归 lambda），
本次提取为 `clipboard::collectBlockDeps(m, elements)`：

- GUI copybase 与命令行 copyclip 调**同一个函数**，语义单一不漂移；
- 递归去重（嵌套 BlockRef 自调用），依赖闭包完整。

### 4.4 pasteInto：一条函数承载两种粘贴语义

```
pasteInto(m, p, to, asBlock, msg)
  ① 合并块表：payload.blocks 逐个入表，冲突名走 uniqueBlockName 自动改名
     （blk → blk_1 → blk_2 …），并同步改 BlockRef 引用名
  ② asBlock ? 转块+插入 : 平移+新增
     - 普通粘贴：translateTo(p, to) 对齐基点 → makePasteChanges 全 id=-1
     - pasteblock：makeBlockDefFromPayload（基点=p.base，块内几何保持世界坐标）
                   → makeBlockRefElement(def, insertPos)
```

`uniqueBlockName` 的冲突改名有一个**配套动作容易漏**：块定义改名后，
payload 里指向旧名的 BlockRef **必须同步改引用名**，否则图元指向不存在的块。
C8 断言：目标图已有 `blk` 时，粘贴后新块名 `blk_1` 且引用名同步。

### 4.5 makeRef id=0 静默失败——本轮最深刻的一个 bug

**现象**：pasteblock 命令日志显示「已粘贴为块成功」，块表 +1 正确，
但**画布上一个图元都没有**，且无任何报错。调试 fprintf 显示
`count=0 blocks=1 msg=成功`。

**根因**：`block::makeRef` 返回的 `Element.id` 默认为 **0**，而
`applyChanges` 的分派语义是：

- `change.id < 0` → **新增**（addElementsQuiet）；
- `change.id >= 0` → **更新**（updateElementQuiet，复用该 id 的既有图元）。

id=0 的块引用走了 update 分支 → 模型里找 id=0 的图元 → 找不到 →
**静默失败**（update 落空不报错是既有设计：更新类 Change 允许目标已被并行删除）。

**修复**：`makeBlockRefElement` 内显式 `e.id = -1`，并加 ⚠️ 注释说明
「id 必须显式置 -1（新增语义）」。C10/C16 两条断言分别锁定
makePasteChanges 全 id=-1 与 pasteblock 落盘两个面。

**泛化教训**：这个 bug 的危险在于**三层静默叠加**——
工厂函数的默认值（id=0）、分派器的语义分支（>=0 即更新）、
更新失败的容错设计（落空不报错），任何一层单看都合理，
叠加起来就是一个「成功日志 + 零落盘」的幽灵行为。
**凡是构造「将要交给 applyChanges 的 Element」，id 必须显式赋值，
默认值一律不可信任。**

### 4.6 QClipboard 无 GUI 会话的壳对象——段错误的防御门禁

**现象**：无头自测（QCoreApplication 环境）跑剪贴板断言时 SIGSEGV，
gdb 回溯定位在 `QClipboard::mimeData()`。

**根因**：QCoreApplication（纯控制台）下 `QGuiApplication::clipboard()`
返回一个**非空指针的壳对象**——判空完全防不住它，但调用其任何平台相关
方法（mimeData）即段错误，因为 Qt 的剪贴板平台层根本没初始化。

**修复**：入口门禁

```cpp
bool clipboardAvailable() {
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
}
```

put/get 两端开头各调一次；无 GUI 会话 get 返回 false + err
「当前无 GUI 会话，剪贴板不可用」。C17/C18 断言锁定无 GUI 环境
put/get 均安全返回 false。**判空指针防不住 Qt 的半初始化对象，
必须用「实例的真实类型」做能力探测。**

### 4.7 单步撤销与失败回滚

- 命令行 pasteclip/pasteblock：`beginEdit` → `pasteInto`（内部全 Quiet 落盘）
  → 一次快照一次 apply，Ctrl+Z 一步回退；
- **失败回滚**：`pasteInto` 返回 false（如 payload 为空）时，命令行与 GUI
  均调 `m->undo()` **弹出刚压入的空转快照**——否则撤销栈里会留一个
  「什么都没做」的幽灵步骤，用户按一次 Ctrl+Z 没反应，第二次才生效。
- GUI 拾取态（clipPastePending_）中途 Esc：`finishClipPick()` 复位全部状态，
  不留下半初始化的 pending 标志。

---

## 5. GUI 接线：拾取状态机与快捷键避让

### 5.1 四个状态成员的最小状态机

```cpp
bool clipBasePending_ = false;          // 正在拾取 copybase 基点
bool clipPastePending_ = false;         // 正在拾取粘贴落点
bool clipPasteAsBlock_ = false;         // 落点拾取完成后是否转为块引用
clipboard::Payload clipPastePayload_;   // 粘贴阶段暂存的载荷
```

复用既有的 `onCanvasPointPicked` 拾取总线，在 mirror 拦截之后追加剪贴板分支：
`onCanvasPointPicked` → `handleClipPick(p)`（copybase 组包 put / 粘贴落点
beginEdit+pasteInto）；`onPointPickCancelled` → `finishClipPick()`。
不新造拾取模式，不碰 GraphicsView 状态——**挂进既有总线，
而不是并行第二套**。

### 5.2 快捷键避让

Ctrl+C / Ctrl+V 已被模型内部剪贴板（同图复制粘贴）占用，剪贴板族四命令取：

| 命令 | 快捷键 |
| --- | --- |
| copyclip | Ctrl+Alt+C |
| copybase | Ctrl+Shift+C |
| pasteclip | Ctrl+Alt+V |
| pasteblock | Ctrl+Shift+V |

菜单注释明确两者关系：**模型内部剪贴板（内存直拷）与系统剪贴板族互补**——
前者快、后者可跨图/跨实例。菜单入口「编辑 → 剪贴板(&L)」子菜单收纳四项；
「连合(&J)」入修改菜单，路径阵列入「阵列」子菜单。

---

## 6. 踩坑记录汇总

| # | 现象 | 根因 | 修复 | 泛化教训 |
| --- | --- | --- | --- | --- |
| 1 | 无头自测 SIGSEGV 于 `QClipboard::mimeData()` | QCoreApplication 下 clipboard() 返回非空壳对象，判空防不住 | `clipboardAvailable()` 用 `qobject_cast<QGuiApplication*>` 门禁 | 判空防不住 Qt 半初始化对象，须探测实例真实类型 |
| 2 | pasteblock「成功」但零图元落盘 | `block::makeRef` 产出 Element.id 默认 0，applyChanges 只认 id<0 为新增 | makeBlockRefElement 显式 `e.id=-1` + ⚠️ 注释 | 交给 applyChanges 的 Element，id 必须显式赋值 |
| 3 | makePathArray 5 个编译错误 | 位置表声明 `vector<Vec3>` 实存弧长参数 `double` | 改名 `params`，类型接管正确性 | 位置表的类型选择本身是设计决策，存参数优于存坐标 |
| 4 | 崩溃点定位迟迟无果 | stdout 全缓冲，崩溃时缓冲丢失，日志只有 2 行 | 直接 gdb bt 锁定 mimeData | 无头崩溃排查第一动作是 gdb，别指望日志 |

---

## 7. 自测与验收数据

### 7.1 断言分布（`t_batchEdit`，共 59 条 CHECK）

| 分组 | 条数 | 关键断言 |
| --- | --- | --- |
| JOIN J1-J10 | 10 | 三段连合 3删+1增；落盘顶点无重复；孤立→空；跨层不连；反向乱序 flip 正确；Arc 离散 ≥12 顶点；**explode 可逆还原 3 段**；单元素→空 |
| 矩形阵列 R1-R9 | 9 | 2×3→5 副本网格坐标精确；落盘 6；1×1→空；零间距 3 重合副本；双源 6；继承 layer+color+Dashed；rows=0→空 |
| 环形阵列 P1-P9 | 9 | 4份180°→3 副本（45°/90° 精确）；圆 8份360° 圆心转 45° 半径不变；Arc 起止角同步 +45°；count=1/0→空；落盘 3；副本继承 layer |
| 路径阵列 T1-T11 | 11 | Measure 100/25→4 副本含终点；不整除 100/30→3；Divide 5 等分→4；路径短于间距→空；单点→空；spacing=0 count=1→空；L 形折线跨折角 (40,30) |
| 剪贴板 C1-C18 | 18（约） | JSON round-trip 保真；版本不符/缺版本拒绝；translateTo 对齐；makePasteChanges 全 id=-1；uniqueBlockName 冲突 blk_1；pasteInto 基点对齐 (100,50)+单步撤销；pasteblock 块表+1 且 BlockRef 落盘；空 payload 拒绝；**无 GUI 环境 get/put 安全 false** |

### 7.2 验收对照

| 验收项 | 要求 | 实际 |
| --- | --- | --- |
| 每命令断言数 | ≥8 | JOIN 10 / 矩形 9 / 环形 9 / 路径 11 / 剪贴板 18 ✅ |
| JOIN→explode 可逆 | 通过 | J7 ✅ |
| 自测新增 | ≥25 | **59 条（344 → 403，全绿 0 失败）** ✅ |
| 菜单+命令行双入口 | 具备 | 8 命令全注册 + 剪贴板子菜单/连合/路径阵列菜单项 ✅ |
| 编译零警告 | 通过 | SketchCAD / SketchSelfTest 均 -Wall -Wextra 0 warning ✅ |
| build.sh test | 退出码 0 | **EXITCODE=0，通过 403 / 失败 0** ✅ |
| 旧版剪贴板内容 | 忽略不崩溃 | C2/C3 版本门禁 ✅ |

---

## 8. 遗留与展望

1. **路径阵列「对齐项目」**：当前仅平移不旋转（AutoCAD 的沿路径切线对齐
   未实现）。MakePathArray 注释已预留——实现时需要位置表带切线角，
   这正是「存弧长参数」类型的第二个收益点；
2. **JOIN 闭合链**：当前只产开放 Polyline（closed=false）。首尾相接成环的
   链（如圆的 8 段近似）可升级为 closed Polyline，直接可用作 hatch 边界；
3. **剪贴板多格式**：可追加 DXF 片段格式作为第二 mime，实现与第三方 CAD
   的剪贴板互通；当前版本化 JSON 已为多格式共存预留（mime 名不变，v 递增）;
4. **GUI 阵列对话框与命令行参数收敛**：既有 ArrayDialog（环形/矩形双模式）
   与命令行参数解析各自组装，长期可统一到「参数结构体 → makeXxxArray」
   的一层薄封装。

---

*配套代码：`src/core/Modify.{h,cpp}`（§16-18）、`src/core/Clipboard.{h,cpp}`、
`src/core/Commands.cpp`（join/arrayrect/arraypolar/arraypath/copyclip/copybase/pasteclip/pasteblock）、
`src/ui/MainWindow.{h,cpp}`（剪贴板子菜单/拾取状态机）、`src/ui/GraphicsView.h`（worldTol 公开化）、
`tests/SketchSelfTest.cpp::t_batchEdit`。*
