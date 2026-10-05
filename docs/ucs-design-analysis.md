# UCS（用户坐标系）体系设计分析

> 需求：从"三投影面"（XY/YZ/XZ）升级为可用的 UCS 体系，支撑斜视图/局部斜坐标的实际制图需求。
> 状态：已实现，全链路静态命名 UCS（动态 UCS 为后续可选子项）。

---

## 1. 现状勘察结论（为什么必须做 UCS）

对既有代码的彻底勘察得出三个关键结论：

1. **`Plane` 是"空头机制"**：`enum class Plane { XY, YZ, XZ }`（Vec.h）与
   `SketchModel::plane_` 只存、只显示（状态栏 / HUD），**完全不参与任何坐标换算**。
   全代码库隐式假设坐标系恒为世界 XY。
2. **无集中坐标换算入口**：点解析 `CommandContext::tryParsePoint` 仅支持绝对
   `x,y[,z]`，不支持 `@` 相对与 `<` 极坐标；DYN 动态输入 `commitDynInput`
   直接对世界 X 轴算 `len*(cos,sin)`。
3. **Snap / GraphicsView / ConstraintSolver 硬编码世界轴**：
   - `Snap.cpp` 的 OTRACK/TK 对齐（硬编码 `(1,0,0)`/`(0,1,0)`）、正交分解、
     极轴角基准（世界 X=0°）；
   - `GraphicsView.cpp` 的网格、坐标轴图标、极轴射线、正交引导线、HUD、坐标读数；
   - `ConstraintSolver` 的角度约束以世界 X 为 0° 基准。

## 2. 核心设计决策

### 2.1 UCS = 2D 刚性变换（origin + 绕 Z 旋转角）

```
Ucs { QString name; Vec3 origin; double angleRad; }
```

- 派生 `xAxis()/yAxis()`（正交、单位、z=0），`toWorld()/toUcs()` 互逆；
- z 分量直通（平面制图工作面恒为 XY 平面内的旋转，不改 Z 语义）；
- **默认 UCS = 世界系**（origin=0, angleRad=0）⇒ 既有 963 条自测零回归；
- 旋转矩阵正交可逆 ⇒ `toUcs(toWorld(p)) == p` 恒成立（自测覆盖）。

### 2.2 插入层 = 输入/显示层，绝不碰存储层

**铁律：Element.pts 始终是世界坐标。** UCS 只存在于两道转换边界上：

```
用户输入(命令行/DYN/鼠标) ──toWorld──▶ 世界坐标 ──▶ Element.pts（存储，不变）
Element.pts（世界） ──toUcs──▶ 显示(网格/极轴/读数/HUD)
```

由此保证：
- 存档格式零改动（旧档/新档完全兼容）；
- 既有渲染、导出、约束求解全部无感。

### 2.3 世界↔用户换算收口到唯一入口

`SketchModel::toWorld()/toUcs()` 是**唯一**换算入口（内联转发 `currentUcs_`），
所有输入与显示路径均消费它，杜绝"同一处换算散落多处导致方向不一致"。

### 2.4 命名 UCS 表不序列化（刻意决策）

`ucsTable_` 存内存（QMap），**不入** toDrawingJson / Memento：
- 避免引入 JSON 兼容回归（顶层键集是旧档契约）；
- UCS 属"交互/视图态"（与会话态不入档的项目惯例一致）；
- 自测 t_ucs 明确断言 JSON 顶层无 `ucs`/`ucsTable` 键，防止将来误加。

### 2.5 `plane` 命令原样保留

既有 `plane xy|yz|xz` 三态行为不变（验收标准③）；UCS 与 Plane 是并行概念，
状态栏同时呈现（`平面:XY` 与 `⌖ WCS/UCS名 角度°`）。

## 3. 各模块改动清单

### 3.1 新增 `src/core/Ucs.h`
- `struct Ucs`：name/origin/angleRad + xAxis()/yAxis()/toWorld()/toUcs()/isWorld()/describe()。

### 3.2 `SketchModel`（core）
- 状态：`currentUcs_`（默认世界）、`ucsTable_`（内存）、`coordDisplayUcs_`（读数模式）、
  `lastInputPointWorld_`（相对坐标基准点）；
- API：`setCurrentUcs()/currentUcs()`、`toWorld()/toUcs()`、
  `parsePointToWorld(s, base, out)`、`displayCoord()`、
  `ucsSave/ucsDelete/ucsSetCurrent/ucsHas/ucsGet/ucsNames/ucsCount`；
- 信号：`ucsChanged()`（状态栏/视图联动；setCurrentUcs 另发 viewChanged 驱动重绘）。

### 3.3 点解析（UCS 感知，Task #530）
`SketchModel::parsePointToWorld` 支持四种语法（均经 toWorld 收口）：

| 语法 | 语义 | 基准 |
|---|---|---|
| `x,y[,z]` | 绝对（UCS 局部） | UCS 原点 |
| `@dx,dy` | 相对笛卡尔 | base（世界）转局部后偏移 |
| `dist<angle` | 绝对极坐标 | UCS 原点，UCS X=0° |
| `@dist<angle` | 相对极坐标 | base，UCS X=0° |

接线点：
- `CommandLine::onReturn`：键盘坐标输入改走 `parsePointToWorld`（基准 =
  `lastInputPointWorld()`；UCS=世界时与旧 `tryParsePoint` 行为逐位一致）；
- `GraphicsView::injectPoint`：提交后回写 `lastInputPointWorld_`（链式 `@` 基准）；
- `commitDynInput`（DYN 参数条）：偏移改沿 `ucs.xAxis()/yAxis()` 分解，
  `updateDynBox` 预填角减去 UCS 旋转角（显示口径 = 提交口径）。

### 3.4 `ucs` 命令（core/Commands）
- 五种定义：`ucs world` / `ucs view` / `ucs 3p p1 p2 [p3]` / `ucs object <id>` /
  `ucs z <x,y> <角°>`，另有便捷 `ucs origin <x,y> [角°]`；
- CRUD：`ucs save|set|restore|delete|list`（set 与 restore 同义；WORLD 不可删）；
- 读数切换：`ucs coords wcs|ucs`；
- 便捷：`ucs <已存在名>` 直接切换；
- 登记于 `registerExact("ucs", ...)`，紧邻 plane。

### 3.5 Snap 按 UCS 轴对齐（零回归设计）
三个函数追加**带默认值**的 UCS 参数（缺省=世界轴 ⇒ 既有调用零改动）：
- `polarAngles(cfg, baseDir, ucsOffsetDeg=0)`：极轴角基准整体偏移；
- `track(..., ucsAngleRad=0)`：OTRACK/TK 的水平/垂直对齐线改为在 UCS 局部系
  内做轴对齐、落点转回世界系（ucsAngleRad=0 时两次变换恒等，逐位等价旧实现）；
- `applyInputConstraints(..., ucsAngleRad=0)`：正交锁轴与栅格量化均转 UCS 局部系执行。

### 3.6 GraphicsView 按 UCS 渲染
- **网格**：UCS≠世界时转 UCS 局部系生成网格线（可见区四角转局部取包围盒保证覆盖），
  经 painter translate(UCS 原点)+rotate(UCS 角) 映射回世界；UCS=世界走原快路；
- **坐标轴图标**：save/translate/rotate 后绘制，落在 UCS 原点、按 UCS 角旋转
  （AutoCAD UCS 图标语义）；UCS=世界时变换恒等；
- **极轴射线**：与追踪吸附共用同一 `polarAngles` 入口（传 UCS 偏移），所见即所得；
- **正交引导线**：沿 `ucs.xAxis()/yAxis()` 绘制；
- **HUD**：显示当前 UCS 名称/原点/旋转角（世界系显示 WCS）；
- **坐标读数**：`cursorMoved` 改发 `model_->displayCoord(w)`（WCS/UCS 随模式）；
- **视图矩阵不变式保持**：`applyTransform` 的无旋转映射
  （`screen_x=wx*zoom+panX; screen_y=-wy*zoom+panY`）**绝不旋转**——
  UCS 旋转全部用几何层 UCS 基实现，避免破坏大量依赖该不变式的既有自测；
- **DYN**：提交/预填角均以 UCS X 为 0° 基准。

### 3.7 状态栏 UCS 指示器
- `StatusBar::setUcsInfo()` 新标签 `⌖ WCS` / `⌖ 名称 角度°`（⌖ 为坐标系定位符号，
  图标语义用文本符号实现，避免引入资源文件）；tooltip 提示 ucs 命令用法；
- `ModelMediator` 订阅 `ucsChanged` + 启动回显一次；
- 读数切到 UCS 时附加「（读数 UCS）」提示。

### 3.8 ConstraintSolver 不动（刻意决策）
`t_horizontal/t_vertical/t_angle` 等约束自测是世界 X 基准的回归用例；约束层
若随 UCS 旋转将全面破坏"约束=几何恒等关系"的语义。约束继续以世界系为唯一
基准（与"UCS 只是输入/显示变换"的定位一致）。

## 4. 自测覆盖（t_ucs，共 24 条 CHECK）

1. **默认=世界系**：isWorld / toWorld / toUcs 恒等（零回归基线）；
2. **Ucs 数学**：xAxis/yAxis 单位正交、双向往返 ≤1e-9、45° 正向算例 ≤1e-12；
3. **验收断言**：45° UCS 下 `@100<0`（基准原点）落点 = (100cos45, 100sin45)，
   **误差 ≤1e-6**；另覆盖绝对笛卡尔/绝对极坐标/相对笛卡尔/世界系零回归 4 例；
4. **ucs 命令**：3p/z/object/view/world 五种定义逐项断言 origin+angle，
   save/restore/delete/set/list/非法子命令拒绝（共 13 条）；
5. **极轴**：polarAngles 偏移后含 30°/120° 不含 0°；track 在 UCS 30° 命中极轴、
   世界系同光标不命中；
6. **正交/栅格**：UCS 90° 正交锁到世界 Y 轴；UCS 45° 栅格沿 UCS 轴量化；
   UCS 0° 与旧实现逐位一致；
7. **读数切换**：WCS 原值 / UCS 局部值且二者不同；
8. **网格方向**：UCS 30° 时主方向=(cos30,sin30)，世界系回到 (1,0)；
9. **不入档**：toDrawingJson 顶层无 ucs/ucsTable 键。

## 5. 已知边界与后续可选项

- **动态 UCS**（绘制时对临时工作平面自动吸附）列为可选子项，未实现；
  静态命名 UCS 全链路已正确，动态 UCS 可在 snapWorld 入口前插入临时 Ucs 而复用全部机制。
- **约束求解**不随 UCS 旋转（见 3.8）；如未来需要"UCS 相对约束"应作为独立约束类型设计。
- **UCS 表不持久化**是刻意取舍；若需随图保存应走"顶层键只追加"路径并同步黄金样本。
- `CommandContext::parsePoint`（脚本/命令行整行命令的绝对点）保持世界系语义——
  键盘交互入口已 UCS 化，脚本参数保持确定性解析，避免既有脚本回归。
