# 极轴追踪（POLAR）/ 对象捕捉追踪（OTRACK）/ 临时追踪点（TK）设计分析

> 关联文档：`snap-mode-management-design-analysis.md`（捕捉模式管理）、`inquiry-measure-design-analysis.md`（查询测量）
> 状态：已实现，`build/SketchCAD.exe` 12.6MB（2026-09-29）

---

## 1. 需求与问题定义

### 1.1 现状边界梳理（实现前）

Request H 落地后的捕捉体系已覆盖「点落在哪」的问题：

| 层 | 机制 | 局限 |
|---|---|---|
| OSNAP 全局配置 | `SnapConfig` 14 开关，随图存档 | 只解决「吸附到已有几何特征点」 |
| 命令期活动配置 | `snapCfg_` + E/T/M/C… 快捷键 | 切换粒度是「类型」而非「位置」 |
| 临时覆盖 | Shift+右键单次覆盖 | 单次有效，无法做方向对齐 |

**缺失的能力**：实际制图中大量点位并不在已有几何的特征点上，而是「与某点水平/垂直对齐」「沿上一段方向成 45°」「相对某锚点偏移」。此时用户只能辅助线+量取，效率低且易错。这正是 AutoCAD 三件套要解决的问题：

- **POLAR（极轴追踪）**：沿极轴角射线对齐，约束**方向**；
- **OTRACK（对象捕捉追踪）**：沿 OSNAP 锚点的水平/垂直对齐线及其交点对齐，约束**坐标关系**；
- **TK（临时追踪点）**：用户显式指定的瞬态锚点，扩展 OTRACK 的锚点来源。

### 1.2 本设计目标

1. 三种追踪统一走**一条纯几何路径**（`snap::track()`），与既有 OSNAP 互补而非叠加混乱；
2. 明确**触发条件、追踪角度、对齐行为、优先级**四要素，行为可预期；
3. 配置持久化（`trackConfig` 随图存档），临时锚点瞬态（不入档）；
4. 视觉反馈完整：射线/对齐线/追踪点/落点四类引导元素；
5. 命令行（`polar`/`otrack`/`tk`/`tkclear`）、菜单（追踪）、面板（追踪设置）三入口可达。

---

## 2. 核心设计决策

### 2.1 决策一：追踪是「落点候选」而非「约束修正」

**权衡**：约束求解器（ConstraintSolver）理论上可把落点表达为约束后修正，但追踪的本质是**交互期临时对齐**——点落位后与追踪状态完全解耦，不产生任何持久约束。若走约束路线：
- 优点：参数化后拖拽仍保持对齐关系；
- 缺点：每次落位都注入约束会污染约束表（用户未必想要永久对齐），且交互期实时求解开销大。

**决策**：追踪作为 `snapWorld()` 中的**落点候选提供者**，与 OSNAP 竞争后择一替换自由点；不与约束系统交互。AutoCAD 同语义（ORTHO/POLAR 只影响点的输入，不是几何约束）。

### 2.2 决策二：统一 track() 纯几何函数

三种追踪共享同一签名（`Snap.h`）：

```cpp
TrackResult track(const Vec3& w, const TrackConfig& cfg,
                  const Vec3* lastSegDir, const Vec3* polarBase,
                  const std::vector<Vec3>& trackPoints,
                  const std::vector<Vec3>& osnapAnchors, double tol);
```

- **输入全部是已算好的锚点**：OSNAP 锚点由 GraphicsView 在悬停时获取（`otrackAnchors_`），TK 点由模型持有（`trackPoints_`），track() 自身不查元素表——保持纯函数、零 UI 依赖，与 `snap::snap()` 同范式；
- **输出** `TrackResult{point, hit, mode, dir, axis, anchorA, anchorB}`：mode 区分三种来源供 HUD/着色，dir/axis 供引导线复用；
- **竞争规则**：所有候选（极轴射线投影点、各锚点水平/垂直对齐点、锚点对正交交点）统一按「到光标距离」取最优，容差 `tol`（= `worldTol()`，8px 换算）内才视为命中。

### 2.3 决策三：落点优先级 OSNAP > 追踪 > 自由

`snapWorld()` 的合并逻辑：

```
snapNow_ = snap::snap(...)          // 先算 OSNAP
trackNow_ = track(...)              // 再算追踪（无论 OSNAP 是否命中都算，供引导线绘制）
落点 = OSNAP命中 ? snapNow_.point
     : track命中  ? trackNow_.point
     :              w
```

**为什么 OSNAP 优先**：OSNAP 是显式几何特征（端点/圆心…），语义最强；追踪是「方向/对齐暗示」。若追踪压过 OSNAP，会出现「想吸端点却被极轴拽走」的失控感。AutoCAD 行为一致（aperture 内 OSNAP 永远赢）。

**为什么 OSNAP 命中时仍算追踪**：① 引导线需要持续显示供用户预判下一落点；② OSNAP 命中点正是 OTRACK 锚点的获取来源。

### 2.4 决策四：极轴角度体系 = 增量角 × N + 正交兜底 + 可选相对基准

```cpp
std::vector<double> polarAngles(const TrackConfig&, const Vec3* baseDir);
```

- **基准**：`polarRelative=true` 且存在上一段方向（`pendingPts_` ≥ 2 点）时，以该方向世界角为 0° 基准；首段/关闭相对时用绝对 0°；
- **射线集合**：`base + k·polarIncrement`（k=0..⌈360/inc⌉-1）∪ {base+0°, 90°, 180°, 270°}（正交兜底，去重后入集）；
- **正交兜底的理由**：用户把增量角设为 22.5° 时仍必须保留水平/垂直对齐能力——水平垂直是制图最高频方向，不应因增量设置而消失；
- **吸附行为**：光标到射线垂距 ≤ tol 且投影长度 > 0（仅前方）时，落点 = 基点 + 方向 × 投影长度——即**保持当前光标距离**，只约束方向。这是 AutoCAD 极轴的标准语义（配合极轴距离输入可精确画「45° 长 100」）。

### 2.5 决策五：OTRACK 锚点获取 = OSNAP 命中即获取，命令期累积

**获取机制**：绘制命令期内，`snapWorld()` 每次算出 OSNAP 命中点即调用 `addOtrackAnchor()`（去重 + 封顶 8 个，FIFO 淘汰）。

**权衡过的备选方案**：
- *方案 A（AutoCAD 原版）*：悬停停留约 300ms 才获取。更贴近原版但引入定时器状态机，交互不可预期（快速划过拿不到锚点），实现复杂度高；
- *方案 B（Shift+点击显式获取）*：用户可控但多一步操作，违背「追踪提效」初衷；
- *方案 C（采用：命中即获取）*：零延迟零操作，代价是锚点集合可能比用户实际需要的多——由封顶 8 个 + 「离得越近距离排序自然胜出」兜底。

**生命周期**：
| 事件 | 动作 |
|---|---|
| 绘制命令中 OSNAP 命中 | 去重并入锚点集 |
| 对象追踪关闭 / 退出绘制命令 | 清空锚点集 |
| 右键结束绘制 | 清空锚点集 |
| 落一点 / 命令内续段 | **保留**（跨段对齐到此前锚点是追踪的核心用法） |

### 2.6 决策六：TK 复用 OTRACK 通道

TK 点与 OSNAP 锚点在 `track()` 内合并为同一对齐锚点表（各带 mode 标签），水平/垂直对齐线与正交交点逻辑完全共享——**一份代码两种来源**。区别仅在于：
- TK 点显式放置（Shift+右键菜单「临时追踪点」/命令行 `tk x,y`），立即持久（直到清除）；
- TK 点不入档、`SketchModel::clear()` 时丢弃（瞬态会话状态）；
- mode 标签为 `TempTK`，HUD/落点标记用青色与 OTRACK 蓝色区分。

### 2.7 决策七：配置分层与持久化

沿用 Request H 确立的四层范式，追踪只有两层：

| 层 | 载体 | 持久性 |
|---|---|---|
| 全局配置 | `SketchModel::trackConfig_`（`TrackConfig{polar, objectTrack, polarIncrement, polarRelative}`） | 随图存档顶层 JSON `"trackConfig"`，缺键回落默认 |
| 瞬态锚点 | `SketchModel::trackPoints_`（TK）+ `GraphicsView::otrackAnchors_`（OTRACK） | 会话内，不入档不入撤销栈 |

- `TrackConfig::operator==` 带 1e-9 角度容差，`setTrackConfig` 幂等（同值不发信号），与 `setSnapConfig` 同范式；
- 读取旧档（无 `trackConfig` 键）：`fromJson(空对象)` 返回默认值（极轴开/对象追踪开/增量 90°/相对上一段），**向后兼容零成本**。

---

## 3. 模块拆分与职责

```
┌────────────────────────────────────────────────────────────┐
│ Snap.h/.cpp (core)        纯几何层                          │
│   TrackConfig   配置 + toJson/fromJson/==                  │
│   polarAngles() 极轴角集生成                                │
│   track()       三合一追踪竞争求解（无 UI 依赖）             │
├────────────────────────────────────────────────────────────┤
│ SketchModel.h/.cpp (core) 状态持有层                        │
│   trackConfig_ + setTrackConfig (幂等+信号)                 │
│   trackPoints_ + addTrackPoint/clearTrackPoints (TK 瞬态)   │
│   clear() 时丢弃 TK 点                                      │
├────────────────────────────────────────────────────────────┤
│ GraphicsView.h/.cpp (ui)   交互驱动层                        │
│   snapWorld(): OSNAP→锚点获取→track→落点优先级               │
│   addOtrackAnchor/clearOtrackAnchors: 锚点生命周期           │
│   addTrackPointAtCursor/clearTrackPoints: TK 入口            │
│   drawForeground: TK 叉/对齐线/极轴射线/落点十字 四类引导     │
│   HUD: 追踪状态行（开关·增量·相对·TK 数量）                  │
├────────────────────────────────────────────────────────────┤
│ Commands.cpp               命令行层                          │
│   polar [on|off|<角度>] / otrack [on|off] / tk <x,y> / tkclear │
├────────────────────────────────────────────────────────────┤
│ MainWindow.cpp             菜单 + 存档                       │
│   顶级「追踪」菜单（F10/F11 快捷切换 + 清除 + 设置入口）      │
│   writeDrawing/readDrawing ↔ root["trackConfig"]            │
├────────────────────────────────────────────────────────────┤
│ SnapSettingsDialog.h/.cpp  设置面板                          │
│   追踪 GroupBox：POLAR/OTRACK 复选 + 增量角 SpinBox           │
│   + 相对上一段复选 + 清除 TK 按钮；accept 双落 setSnapConfig   │
└────────────────────────────────────────────────────────────┘
```

**职责边界**：track() 不知道锚点从哪来；GraphicsView 不知道 track() 内部几何；SketchModel 只存状态不参与计算。三层互不越界，追踪增改不触碰捕捉主流程（`snap()` 零改动）。

---

## 4. 交互逻辑规格

### 4.1 触发条件汇总

| 功能 | 触发前提 | 生效范围 |
|---|---|---|
| POLAR | `trackConfig.polar` 开 且 处于绘制命令（`drawing_`）且已有 ≥1 落点（极轴基点） | 该命令期内 |
| OTRACK | `trackConfig.objectTrack` 开 且 绘制命令期内 且 `otrackAnchors_` 非空（需 OSNAP 曾命中） | 该命令期内 |
| TK | `trackPoints_` 非空（放置不要求绘制中，但对齐只在绘制命令期生效） | 清除前始终有效 |

### 4.2 对齐行为

- **POLAR**：落点 = 基点 + 射线方向 × |光标投影|（保距约束方向）。容差带 = 光标到射线垂距 ≤ 8px；
- **OTRACK/TK 单锚点**：光标 |Δy|≤8px → 吸到 (光标x, 锚点y)；|Δx|≤8px → 吸到 (锚点x, 光标y)；
- **OTRACK/TK 双锚点交点**：(A.水平线)×(B.垂直线) → (B.x, A.y) 及镜像；自然被距离排序淘汰，只有光标真正靠近交叉区时胜出；
- **多候选竞争**：全部候选按到光标距离取最小者，与来源无关（极轴与对齐线公平竞争）。

### 4.3 与既有捕捉/约束的协同规则

1. **OSNAP 优先**：aperture 内有几何特征点时追踪让位（§2.3）；
2. **OSNAP 喂养 OTRACK**：OSNAP 命中点即获取为 OTRACK 锚点——捕捉是追踪的锚点来源，关闭全部 OSNAP 类型则 OTRACK 无锚点可用（依赖关系而非冲突）；
3. **追踪不产生约束**：落位后即为普通点，约束求解器（自动约束生成等）完全无感知；
4. **临时覆盖（Shift+右键单类型）与追踪共存**：覆盖只改 `snapCfg_`（OSNAP 层），追踪层不受影响；同一菜单同时提供 TK 入口，一次弹出即可完成「单次捕捉 + 放追踪点」两类操作；
5. **键盘输入坐标**：`keyboardInputActive_` 时 `snapWorld` 直接返回原值，追踪同被跳过（精确坐标优先）。

### 4.4 视觉语言

| 元素 | 形态 | 颜色 |
|---|---|---|
| TK 临时追踪点 | 小叉 × | 青 (0,170,170) |
| OTRACK 对齐线 | 全幅虚线（水平+垂直过锚点） | 蓝 (0,150,220) |
| POLAR 射线 | 从基点出发的全幅虚线 | 橙 (220,140,0) |
| 追踪落点 | 十字标记（随 mode 着色） | 同上三色 |

引导线长度取 1e5（场景边界），cosmetic 笔宽随缩放恒定，与既有捕捉标记同一绘制管线（`drawForeground`）。

---

## 5. 实现要点与陷阱规避

1. **`drawing_` vs `drawToolActive_`**：追踪计算与引导线绘制统一门控在 `drawing_`（进入实际取点交互才置位），HUD 状态行沿用 `drawToolActive_`（与捕捉开关两行一致）；
2. **上一段方向的取法**：`(pendingPts_.back() - pendingPts_[size-2]).normalized()`，仅当 ≥2 落点；首段回落绝对角——避免用 `lastMouseWorld_`（光标实时移动会把「相对基准」变成追光标）；
3. **锚点封顶 8 个 FIFO**：长折线命令期 OSNAP 命中会持续累积，不封顶则 `track()` 的 O(n) 锚点 × O(n) 交点循环退化为 O(n²)；
4. **角度归一**：`polarAngles` 内部 fmod 归一到 [0,360) 再去重（±360 邻近等价），防止相对基准为负角时射线重复；
5. **投影仅前方**：`proj <= 0` 直接跳过，基点背后的「反向射线」永不吸附（AutoCAD 同语义）；
6. **`TrackResult` 默认值**：`trackNow_` 每帧重置为 `{}`，工具切换/结束绘制时再显式清一次，杜绝跨命令残留十字标记；
7. **幂等信号**：`setTrackConfig` 与 `setSnapConfig` 同范式（同值早退），菜单勾选态经 `trackConfigChanged` 双向同步，命令行/面板/菜单三入口改配置不会互相打架。

---

## 6. 验证

- 构建：`cmake --build ... --target SketchCAD -j 1` → **EXIT=0**（62/62 编译单元 + 链接一次通过）；
- 产物：`build/SketchCAD.exe` 12,634,325 字节（2026-09-29 22:31）；
- 警告：仅既有 4 条 `QCheckBox::stateChanged` 弃用警告，本次零新增；
- 本轮改动文件：`Snap.h/.cpp`、`SketchModel.h/.cpp`、`GraphicsView.h/.cpp`、`MainWindow.cpp`、`Commands.cpp`、`SnapSettingsDialog.h/.cpp`。

---

## 7. 后续可扩展方向

- **极轴距离输入**：落点吸附极轴后，命令行直接输入长度即得「角度+距离」精确点（当前可经参数输入条实现）；
- **额外极轴角列表**：在增量角之外支持用户自定义附加角集合（AutoCAD「附加角」）；
- **锚点显式管理**：Shift+右键菜单列出已获取锚点并支持单个移除；
- **OTRACK 悬停获取延时**：若实测「命中即获取」噪音大，可加 300ms 悬停门控（§2.5 方案 A）作为配置项。
