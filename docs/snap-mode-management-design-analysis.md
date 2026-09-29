# 捕捉模式管理（OSNAP 持久化 + 临时覆盖 + 设置面板）设计分析

> 对应需求：在既有对象捕捉（OSNAP）与约束求解模块之上，新增「捕捉模式管理」能力——
> 1) **临时捕捉覆盖**：Shift+右键弹出菜单临时指定单次捕捉类型，覆盖当前全局设置，放置一点后自动恢复；
> 2) **捕捉开关面板**：14 种捕捉模式各自独立启用/禁用；
> 3) 解决「捕捉冲突时只能全局切换、缺乏灵活性、效率低下」的痛点，实现按场景精细控制。
> 本文记录设计权衡、不变式与实现落点，供后续维护复用。

---

## 1. 问题背景与动机

既有实现（`Snap.h/.cpp` + `GraphicsView` 的 `snapCfg_`）已支持 14 种捕捉类型与优先级竞争，
但在「**行为如何被用户控制**」这一层存在三处短板：

1. **配置只活在命令期**：`snapCfg_` 是绘制命令进入时由 `snapDefaultFor(t)` 载入的临时快照，
   离开命令即被 `snapCfgBackup_` 覆盖还原。用户没有一处「长期有效、随图存档」的捕捉偏好。
2. **命令内置默认压过用户意图**：`snapDefaultFor(t)` 按工具硬编码开关组合（如圆命令强制开圆心/切线），
   用户即便想关掉某类捕捉也无从下手；碰到「该开的不想开、该关的被强制开」的冲突只能忍受或反复切换。
3. **冲突时粒度粗**：现有 E/T/M/C/X… 快捷键虽能命令期内切换，但仍是「全局即时改写」，
   无法表达「这次我只想吸一下象限点，吸完就回原样」这类一次性诉求。

AutoCAD 的做法是把这些拆成三层：**OSMODE（持久位掩码）+ 改写命令（`_end`/`_non）+
Shift+右键临时覆盖（object snap override）**。本方案即对齐这一成熟范式。

---

## 2. 四层配置模型（核心设计）

为兼顾「用户长期偏好」「命令上下文」「一次性覆盖」「撤销安全」，引入清晰的四层：

| 层 | 载体 | 生命周期 | 是否随图存档 | 谁写 |
|----|------|----------|--------------|------|
| ① 全局持久配置 | `SketchModel::snapConfig_` | 跨命令、跨会话 | ✅（`root["snapConfig"]`） | 捕捉设置面板（`setSnapConfig`） |
| ② 活动配置 | `GraphicsView::snapCfg_` | 绘制命令期 | — | 命令进入时 = ①；期间被快捷键/覆盖改写 |
| ③ 命令基线 / 恢复点 | `GraphicsView::snapCfgBackup_` | 绘制命令期 | — | 命令进入时 = ① |
| ④ 一次性临时覆盖 | `snapOverrideActive_` + `snapRestore_` | 仅下一次落点 | — | Shift+右键菜单 `applySnapOverride` |

要点：

- **绘制命令进入时**，`snapCfgBackup_` 与 `snapCfg_` 同时被赋值为 `model_->snapConfig()`（层①）。
  因此「用户面板设定」成为绘制期捕捉的真正基线，取代原先的 `snapDefaultFor(t)` 内置默认。
  这意味着用户关掉的捕捉，任何命令都不会再偷偷打开——正是需求要的「按场景精细控制」。
- `snapDefaultFor(t)` 保留为函数但退出活动路径；其语义被「全局配置优先」取代。保留它是为了
  避免破坏既有接口契约，且可作为将来「命令默认 + 用户配置」合并策略的落点（见 §6 备选）。
- **临时覆盖激活时**，`snapRestore_` 先记下当时的 `snapCfg_`（覆盖前活动配置），再把 `snapCfg_`
  置为「仅单一类型开启」的临时配置。`snapOverrideActive_=true`。
- **放置一点后**（`handlePress` 检测到 `pendingPts_` 增长）或**右键结束绘制**时，调用
  `clearSnapOverride()`：把 `snapCfg_` 还原为 `snapRestore_`，`snapOverrideActive_=false`。
  这就实现「操作结束后自动恢复原有配置」。

### 为什么用 `snapRestore_` 而非直接回退 `snapCfgBackup_`？

需求原文是「恢复**原有**配置」，即覆盖时刻之前正在用的那一份（可能已被 E/T/M/C/X 快捷键改过），
而不是命令进入时的干净基线。因此必须记录覆盖前的瞬时活动配置，而不能简单回退到层③。
这是实现「语义正确」的关键细节。

---

## 3. 数据源单一化：`snapFieldDefs()`

14 种捕捉的「快捷键 / 显示名 / 对应 `SnapConfig` 成员指针 / JSON 键」原先散落在三处：

- `GraphicsView` 构造里的 `defs[]` 快捷键表（E/T/M/C/X…）；
- 可能发生（未来）的覆盖菜单；
- 捕捉设置面板。

任何一处改了字段漏改另一处都会漂移。本方案把这张表收敛为 **唯一真源**
`snap::snapFieldDefs()`（定义于 `Snap.cpp`），返回：

```cpp
struct SnapFieldDef {
    int key;                   // Qt::Key
    QString label;             // 显示名（含快捷键提示，如 "象限(Q)"）
    const char* jsonKey;       // JSON 序列化键（稳定英文，与字段名一致）
    bool SnapConfig::* field;  // 对应 SnapConfig 开关成员指针
};
```

三处消费方：

1. **快捷键注册**：`GraphicsView` 构造遍历 `snapFieldDefs()` 建 `QShortcut`，触发 `toggleSnap(d.field, d.label)`。
2. **Shift+右键覆盖菜单**：遍历 `snapFieldDefs()` 建 `QAction`，触发 `applySnapOverride(d.field, d.label)`。
3. **捕捉设置面板**：遍历 `snapFieldDefs()` 建 `QCheckBox`，`collect()` 时按 `d.field` 读回。

`SnapConfig::toJson()/fromJson()` 也遍历同一张表，保证「内存字段 ↔ JSON 键 ↔ UI 标签 ↔ 快捷键」
四者永不失同步。新增一种捕捉类型，只需在 `SnapConfig` 加一个 `bool` 字段、**同时**在
`snapFieldDefs()` 加一行即可，其余全部自动生效。

---

## 4. 持久化范式（对齐 UNITS / LIMITS）

`SnapConfig` 沿用 `UnitsMeta`/`LimitsMeta` 的成熟范式：

- `QJsonObject toJson() const`（成员函数，遍历 `snapFieldDefs` 写 14 个布尔键）；
- `static SnapConfig fromJson(const QJsonObject&)`（缺键回落默认，兼容旧档）；
- `operator== / operator!=`（供 `SketchModel::setSnapConfig` 做幂等判定，相同则不发信号）。

`SketchModel` 新增：

```cpp
snap::SnapConfig snapConfig_;                       // 成员
const snap::SnapConfig& snapConfig() const;         // 读
void setSnapConfig(const snap::SnapConfig& c);      // 写（幂等）
// 信号：snapConfigChanged()
```

设计决策（与 `setUnits`/`setLimits` 完全一致）：

- **不入撤销栈**：捕捉配置是「应用级偏好」，与单位/界限同级，撤销几何不应连带回退捕捉开关。
- **随图存档**：`MainWindow::writeDrawing` 写 `root["snapConfig"]`，`readDrawing` 读回；
  旧文件无此键时 `fromJson({})` 回落默认（端点/圆心/交点/切线开，其余关）。
- **幂等**：`setSnapConfig` 与现值全等则 `return`，避免无谓的 `snapConfigChanged` 信号风暴。

---

## 5. 交互落点

### 5.1 Shift+右键临时覆盖
- 触发：`GraphicsView::mousePressEvent` 的右键分支，最先判断
  `(e->modifiers() & Qt::ShiftModifier) && drawToolActive_`，成立则 `showSnapOverrideMenu(mapToGlobal(...))`
  并 `accept()` 返回——**不进入平移、不结束命令**。
- 菜单：标题「临时捕捉覆盖（仅本次生效）」，下列 14 种类型（带快捷键提示），末项「清除覆盖（恢复原有配置）」。
- 选某类型 → `applySnapOverride`：仅该类型 `on`，其余 `off`，`snapRestore_` 记覆盖前配置，即时 `refreshSnapAtCursor()`。
- 自动恢复：下一次成功落点（`pendingPts_` 增长）或右键结束绘制时 `clearSnapOverride()`。
- 命令退出（`onActiveToolChanged` 的 `!newDraw && prevDraw` 分支）也强制 `snapOverrideActive_=false`，
  杜绝覆盖态跨命令残留。

### 5.2 捕捉设置面板（OSNAP）
- 入口：文件菜单「捕捉设置...」→ `MainWindow::openSnapSettings()` → `SnapSettingsDialog(model_, this).exec()`。
- UI：14 个 `QCheckBox`（来自 `snapFieldDefs()` 回填当前全局配置），便捷按钮「全部开启 / 全部关闭 / 恢复默认」。
- 确认：`accept()` → `model_->setSnapConfig(collect())` 落地为全局持久配置。
- 与命令期切换/覆盖的边界：**面板改的是层①（基线）**，E/T/M/C/X 与 Shift+右键只改层②④，命令结束即弃，
  不写回层①。三者各司其职、互不污染。

### 5.3 降级与健壮性
- 不在绘制命令期（非 `drawToolActive_`）Shift+右键 → 不弹菜单，走原有右键语义（平移/结束）。
- `fromJson` 对缺失键回落默认，旧图/损坏图不会让捕捉配置崩溃。
- 覆盖激活期间若用户再按 E/T 等快捷键，`toggleSnap` 改的是被覆盖的 `snapCfg_`；一旦落点清除覆盖，
  这些临时改动随 `snapRestore_` 一并被抛掉——符合「覆盖是最高优先、一次性」的语义。

---

## 6. 备选方案与取舍

| 方案 | 描述 | 取舍 |
|------|------|------|
| A. 命令默认 OR 用户配置 | 进入命令时 `base = global; def = snapDefaultFor(t); base.* = base.* || def.*` | 保证命令必要捕捉必开，但「用户关掉某捕捉」会被命令强制打开，与「精细控制」诉求相悖，**弃用** |
| B. 纯全局（采用） | 活动配置 = `model_->snapConfig()`，命令默认退出活动路径 | 完全把控制权交给用户面板，最贴合需求；代价是丢失命令「智能默认」，但被临时覆盖机制补偿 |
| C. 覆盖跨多次落点 | 覆盖不随单次落点清除，直到显式取消 | 实现更复杂，且与「单次捕捉」语义不符，**弃用** |

方案 B 配合「Shift+右键一次性覆盖」恰好覆盖 AutoCAD 的全部使用场景：
长期偏好 → 面板；当前命令微调 → E/T/M/C/X；偶发一次性 → Shift+右键。

---

## 7. 修改清单（落点索引）

- `src/core/Snap.h`：`SnapConfig` 增 `toJson/fromJson/operator==/operator!=`；新增 `SnapFieldDef` 与
  `snapFieldDefs()` 声明；`fromJson` 改为 `static`。
- `src/core/Snap.cpp`：实现 `snapFieldDefs()`（14 项，jsonKey 与字段名一致）、
  `SnapConfig::toJson()`、`SnapConfig::fromJson()`（静态，缺键回落）。
- `src/core/SketchModel.h`：增 `#include "Snap.h"`、`snapConfig_` 成员、`snapConfig()`/`setSnapConfig()`、
  信号 `snapConfigChanged()`。
- `src/core/SketchModel.cpp`：实现 `setSnapConfig`（幂等）。
- `src/ui/GraphicsView.h`：增 `snapOverrideActive_`、`snapRestore_` 成员；`applySnapOverride`/`clearSnapOverride`/`showSnapOverrideMenu` 声明。
- `src/ui/GraphicsView.cpp`：
  - `mousePressEvent` 右键分支首判 Shift → 弹覆盖菜单；
  - `onActiveToolChanged` 活动配置基线改为 `model_->snapConfig()`，并清覆盖态；
  - 新增 `applySnapOverride`/`clearSnapOverride`/`showSnapOverrideMenu`；
  - `handlePress` 落点后清覆盖；`handleRightButton` 结束绘制时清覆盖。
- `src/ui/SnapSettingsDialog.h/.cpp`：新增 14 项独立开关面板 + 便捷按钮。
- `src/ui/MainWindow.cpp`：增 `#include`、菜单项、槽 `openSnapSettings`、`writeDrawing`/`readDrawing` 接 JSON 键。
- `src/ui/MainWindow.h`：声明 `openSnapSettings()`。
- `CMakeLists.txt`：UI_SOURCES 注册 `SnapSettingsDialog.cpp`。

---

## 8. 验证要点

1. **编译**：`-j 1` 增量构建 `SketchModel`/`GraphicsView`/`MainWindow`/`SnapSettingsDialog` 通过，无新增告警。
2. **面板持久化**：打开面板关掉「端点」→ 确定 → 进入画线命令应不再吸附端点；保存→重开文件，设置保留。
3. **临时覆盖**：画线中 Shift+右键选「象限(Q)」→ 光标只吸象限点；落一点后自动恢复此前配置（日志可见「已清除临时捕捉覆盖」）。
4. **边界**：非绘制态 Shift+右键不弹菜单；覆盖激活后切命令不残留；`fromJson` 对缺键旧档回落默认。
5. **单一数据源**：新增/删除某捕捉字段后，快捷键、覆盖菜单、面板、JSON 键应同步变化（可由 `snapFieldDefs()` 一处修改验证）。
