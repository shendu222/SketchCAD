# 查询测量功能设计分析（Inquiry：DIST / AREA / MASSPROP / ID + LIST 增强）

> 本轮改造范围：改进现有查询测量功能的**准确性**与交互体验，优化 `list` 的对象信息展示；新增 `DIST`、`AREA`、`MASSPROP`、`ID` 四个测量命令。所有新增命令与现有命令行风格一致（经 `CommandRegistry` 精确匹配分派、`SketchModel::log` 输出、中文提示、异常输入有明确回显）。

---

## 1. 背景与差距

改造前查询能力仅有 `list` 单行输出（`#id 类型 sizeDesc`），存在四类差距：

| 差距 | 说明 |
| --- | --- |
| 无测量命令 | 两点距离 / 面积周长 / 质量特性 / 点坐标均不可查询（AutoCAD 对应 DIST / AREA / MASSPROP / ID） |
| list 信息单薄 | 单行文本，无图层、无封闭图元的面积/周长/形心，无计数头部，不支持指定 id |
| 弧段几何不准 | 折线弧段长度用「弦长×1.05」粗估；`sizeDesc`、面积、周长均失真 |
| **arcThrough 根本性缺陷** | 弧段方向翻转后**弧终点偏离线段终点 b**，靠渲染层 `moveTo` 兜底掩盖（详见 §2） |

## 2. 核心发现：arcThrough 方向校准缺陷（准确性问题的根）

### 2.1 现象

`geo::arcThrough(pts, i, closed)` 用三相邻节点（前驱 c、当前 a、后继 b）确定折线弧段。旧实现的 sweep 校准逻辑：先把 CCW 张角归一到 (−180, 180]，再按「c 是否在弧内」条件翻转方向。**翻转后弧终点不再等于 b**，导致：

- 渲染断线（GraphicsView 靠 `moveTo` 兜底掩盖，视觉上弧段与后续段脱开）；
- 面积/周长失真（弧段终点错位，shoelace 闭合边错误）；
- DXF 导出的 bulge 与三相邻点定圆不一致（`tan/atan` 在 (−360°, 360°) 全域互逆，方向错则 bulge 错）。

旧 DXF 导入测试因此只能断言「镜像弧被 dropped」——即外部 bulge 弧与三相邻点定圆方向相反时放弃还原。

### 2.2 修复（Geometry.cpp）

两条候选弧（CCW 张角 `dCcw`、CW 张角 `dCcw−360°`）中，**选择不经过前驱节点 c 的那条**：

```cpp
// CCW 张角与 c 的相对角位置都归一化到 [0, 360)
double dCcw = std::fmod(endDeg - r.startDeg, 360.0);
if (dCcw < 0) dCcw += 360.0;
double dC = std::fmod(cDeg - r.startDeg, 360.0);
if (dC < 0) dC += 360.0;
constexpr double kAngEps = 1e-9;
r.sweepDeg = (dC > kAngEps && dC < dCcw - kAngEps) ? (dCcw - 360.0) : dCcw;
```

不变式：**弧终点恒为 b**。圆弧两端分别映射即正确（无需交换起止角），端点保持语义在所有消费方统一：

| 消费方 | 修复后状态 |
| --- | --- |
| GraphicsView 渲染 | 移除 `if (i+1 < nSegs) path.moveTo(...)` 兜底；弧终点恒为 b，`lineTo/closeSubpath` 自然衔接 |
| Geometry::distanceToElement 命中 | 采样点链连续，命中与视觉一致 |
| CadFormats DXF 导出 | `bulge = tan(sweep/4)`：tan/atan 全域互逆，方向修复后 bulge 与定圆一致 |
| CadFormats DXF 导入 | 外部 bulge 弧与三相邻点定圆**同弧时被忠实还原**（`makeArcFlags`），测试断言已从「dropped」升级为「如实还原」 |
| Modify::offset / Section 剖面 / Inquiry 测量 | 复用同一 arcThrough，端点语义自动统一 |

闭合折线填充（Section 细分管线的半边面提取）此前因弧终点漂移存在泄漏隐患，修复后一并恢复正确。

### 2.3 弧段精确测量公式（Geometry 新增 API）

```cpp
double polylineArcLength(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed);
double polylineSignedArea(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed);
```

- 弧段长度 = `|sweep| · r`（sweep 取自 arcThrough 外接圆，解析精确）；
- 弧段面积贡献 = `½[r²·Δ + cx·(b.y − a.y) + cy·(a.x − b.x)]`（Δ 为有向扫掠弧度，弓形修正精确）；
- 直线段照常累加。`Element::sizeDesc` 的 Polyline 分支改用 `polylineArcLength` 取代「弦长×1.05」；Spline 六种子类型统一用 `geo::flatten` 离散求总长；Polygon 补总长。

## 3. 分层设计

延续工程既有分层，**几何事实与渲染同源**：

```
Inquiry.{h,cpp}（新增，纯只读几何，不依赖 UI、不改模型）
    ↑          命令层 Commands.cpp（参数解析 / 分派 / 日志格式 / 选区分派）
    ↑              ↑
  Geometry.cpp（arcThrough / polylineArcLength / polylineSignedArea）
                   SketchModel::log → logAppended 信号 → 命令行面板
```

- `Inquiry` 核心层只回答「是多少」；「怎么问、怎么显示」留在命令层；
- 命令注册：`registerExact("dist"/"area"/"massprop"/"id", ...)`，与 `dim dist` 子命令无冲突（分派只看 `toks[0]`）；
- `HelpCommand` 新增「查询」两行，说明五种命令与点格式。

## 4. Inquiry 核心层 API

```cpp
namespace SketchCAD::inquiry {
  DistResult     distTwoPoints(a, b);          // distance/dx/dy/angleDeg（(-180,180]）
  AreaResult     areaOfPolygon(ring);          // 点列模式（shoelace）
  AreaResult     areaOfElement(e);             // 图元分派
  // —— 直线链围合面积（第六轮-C，附图场景：若干首尾相接直线围成多边形）——
  bool           buildRingFromLines(lines, ring, tol=1e-6); // 贪心串环：N 条首尾相接直线 → 闭合顶点环
  AreaResult     areaOfClosedLineChain(segs);  // 直线链 → 串环 → shoelace 面积/周长/形心
  MassPropResult massPropOfElement(e);         // 质量特性
  IdResult       idPoint(p, elements);         // 点坐标 + 最近图元
  // 底层（list/测试复用）：polygonArea / polygonPerimeter / polygonCentroid
  //                      polygonInertia / discretizeRing
}
```

封闭类型集（AREA/MASSPROP 支持）：**圆、椭圆、矩形、多边形、闭合折线（含弧段）、闭合样条、剖面区域（Hatch，含孔）**。

### 4.1 精度策略

| 图元 | 面积/周长 | 惯性矩 |
| --- | --- | --- |
| 圆 | 解析精确（πr²、2πr） | 解析（πr⁴/4） |
| 椭圆 | πab 精确；Ramanujan 周长（< 5e-5 相对误差） | 64 段离散环（任意旋转角统一正确） |
| 矩形/多边形/Hatch | shoelace 精确 | 环绕原点 + 平行轴定理移轴 |
| 含弧段闭合折线 | 弧段精确公式（§2.3） | 16 段/弧离散环（与渲染采样一致，误差 ~0.1%） |
| 闭合样条 | Catmull-Rom 采样离散（与渲染同源） | 同左 |
| Hatch 带孔 | 外环 CCW + 内孔 CW 负贡献，净面积/净形心；周长含内孔 | `polygonInertiaSigned` 叠加后移轴 |

### 4.2 关键实现细节

- **方向归一**：`polygonInertia` 与 Hatch 处理先把环归一 CCW（CW 孔给法自动兼容，测试覆盖 CW/CCW 两种孔方向结果一致）；
- **Hatch 复合形心**：一阶矩加权 `Σ(Aᵢ·cᵢ)/ΣAᵢ`（孔为负贡献）；
- **包围盒**：`QRectF(minX, minY, w, h)` 世界坐标（屏幕语义：`top()`=min-y 边、`bottom()`=max-y 边）；
- **idPoint 最近图元**：复用 `geo::distanceToElement`（圆只命中圆周、矩形命中轮廓——与选择/悬停命中语义一致）。

## 5. 命令层（语法 / 输出 / 边界处理）

### 5.1 LIST（增强）

```
list              全部图元     list 3 5      指定 id（不存在的 id 明确报错）
```

输出格式（两行/图元）：

```
---- 对象列表（共 2 个）----
#1 矩形  [图层 0]  10×20
    面积 200.00 mm²  周长 60.00 mm  形心 (5.00, 10.00)
#2 直线  [图层 0]  L=10.00
```

- 首行：id + 类型 + 图层 + 主尺寸描述；次行仅封闭图元（与 AREA 同源计算）；
- 空模型提示「模型为空：无图元可列出」。

### 5.2 DIST

```
dist x1,y1 x2,y2
```

```
距离 = 14.1421 mm    ΔX = 10.00    ΔY = 10.00    XY平面角 = 45.00°
```

边界：无参数 → 用法提示；点解析失败 → 「点解析失败: xxx」。

### 5.3 AREA（四模式）

```
area <封闭图元id>                    对象模式
area x1,y1 x2,y2 [x3,y3 ...]        点列模式（任一参数含 "," 即识别）
area                                选区模式（无参用当前选区，逐个统计）
area <直线id...>                     直线链模式（≥3 条首尾相接直线围成多边形）  [第六轮-C 新增]
```

```
#1 矩形：面积 = 200.0000 mm²    周长 = 60.0000 mm    形心 = (5.00, 10.00)
合计 2 个封闭区域：总面积 = 284.0000 mm²
```

边界处理（均有中文原因回显、返回失败）：
- 非封闭图元 → `不是封闭区域（支持：圆/椭圆/矩形/多边形/闭合折线/剖面区域）`；
- 开放折线 → `折线未闭合，无法计算面积（可先执行 PEDIT Close）`；
- 点列 < 3 / 共线退化 / id 不存在 / 空选区 → 各自明确提示。

#### 5.3.1 直线链模式（第四模式）与输入数据形式

针对「由若干条首尾相接的直线所围成区域（多边形）」这一典型场景（如附图），此前 `area` 仅认封闭图元（矩形/多边形/闭合折线等），离散的 `Line` 链会被「非封闭图元」拒绝。**本轮新增直线链模式：当目标全部为直线且端点一一首尾相接时，自动串成闭合顶点环再用 shoelace 求面积**，命令输出带「直线链模式（N 条边围成多边形）」前缀。

**三种输入数据形式**

| 形式 | 如何得到封闭区域面积 | 备注 |
| --- | --- | --- |
| ① 顶点序列 | `area x1,y1 x2,y2 ...`（点列模式）/ 对话框「面积」页点列框 | 直接给定多边形顶点，shoelace 解析精确 |
| ② 一组首尾相接的直线 | 选中（选区模式）或 `area <直线id...>`（对象模式，全部为 Line 时进入直线链分支） | **本轮新增**：无需预先连成多边形/闭合折线，散直线即可 |
| ③ 图像文件 | 工程**不支持**直接读图求面积 | 须先矢量化：在画布描线，或导入 DXF（线/多段线）→ 再走 ① 或 ② |

**串环算法（`buildRingFromLines`）**

给定 N 条直线（每条 `pts[0]→pts[1]`），贪心拼接成闭合环：

1. 预校验：`N ≥ 3` 且每条均为 Line、端点各 ≥ 2；
2. 以首条直线起点为 `start`、终点为当前衔接点 `cur`（首顶点入环）；
3. 在剩余未用直线中找「某端点与 `cur` 距离 < `tol`（默认 1e-6）」者：
   - 命中则把**共享顶点 `cur`** 入环，将 `cur` 前进到该直线另一端点，标记已用；
   - 同时兼容两端中任意一个靠近 `cur` 的情况（自动换向，无需顶点顺序）；
4. 重复至用完全部直线，末端点须能回到 `start`（闭合判定）；任一缝隙/分叉即返回失败，错误信息提示「未能首尾相接围成封闭环（需 ≥3 条直线且端点一一相接、无缺口/分叉）」。

> ⚠️ 实现要点：入环时 push 的是**共享顶点 `cur`** 而非远端点——初版误 push 远端点导致环丢角点（如五边形串成 `[C,E,A,B,C]`、面积假值 1262.5），修正后环顶点数 = N、面积正确。

**面积公式（shoelace，有向面积取绝对值，解析精确）**

```
A = ½ | Σ (xᵢ·yᵢ₊₁ − xᵢ₊₁·yᵢ) |      （i 从 0 到 N−1，第 N 点回到第 0 点）
```

`areaOfClosedLineChain` = `buildRingFromLines(ring)` 成功 → `areaOfPolygon(ring)`，一并返回周长（逐边欧氏和）与形心（shoelace 一阶矩）。命令层 `AreaCommand` 与对话框 `InquiryDialog::doArea` 均以「目标是否全部为 Line」作为直线链优先分支的开关，非全直线时退回逐图元统计（见 §4 `areaOfClosedLineChain` 与 §7 GUI 入口）。

### 5.4 MASSPROP

```
massprop <id> [id ...]              无参用选区
```

```
#1 矩形  面积 = 200.00 mm²  周长 = 60.00 mm
  包围盒: (0.00, 0.00) - (10.00, 20.00)
  形心: (5.00, 10.00)
  惯性矩(形心) Ix = 6666.67    Iy = 1666.67    Ixy = 0.00 mm⁴
```

惯性矩为**形心轴**值（世界轴值经 Inquiry 层按平行轴定理提供，命令输出取形心轴，与 AutoCAD 惯例一致）。

### 5.5 ID

```
id x,y
```

```
坐标: X = 35.0000    Y = 2.0000    Z = 0.0000
最近图元: #2 直线（距离 2.0000 mm）
```

最近图元为增强交互（模型为空时只输出坐标）。

## 6. 测试与验证

### 6.1 新增 InquiryTest（tests/inquiry_test.cpp，52 断言，exit 0 权威判定）

| 节 | 覆盖 |
| --- | --- |
| 1. distTwoPoints | 水平/3-4-5 斜线/第三象限角/重合点 |
| 2. polygon 底层 | 面积/周长/形心/CW 负面积/绕原点惯性矩解析对照（wh³/12 + 平行轴） |
| 3. 弧感知折线 | `polylineArcLength/SignedArea` 弧段精确值（5π+10√2+20、50+25π）；**arcThrough 端点保持不变式**（sweep=+90°、采样终点逼近 b） |
| 4. areaOfElement | 圆/矩形/三角形/含弧折线/带孔 Hatch（CW 与 CCW 孔方向归一一致）/开放折线与直线拒绝 |
| 5. massPropOfElement | 矩形 wh³/12 解析一致、平行轴、包围盒、圆 πr⁴/4、带孔 IxBar=812 |
| 6. idPoint | 最近图元判定、点在图元上距离 0 |
| 7. 命令层 | 五命令的参数解析/输出格式/用法提示/异常输入（无参、坏点、坏 id、空选区） |

### 6.2 既有测试同步

- `dxf_import_test.cpp`：外部 bulge 弧断言由「与定圆镜像时 dropped」升级为「同弧时忠实还原（扫掠/半径一致）」；
- 全量回归（2026-09-27）：45 个目标（主程序 + 44 测试/预览 exe）`-j 2` 全量重建成功（BUILD_EXIT=0），**43 个测试 exe 全部 exit 0**（含 SketchSelfTest 主套件、DxfImportTest 新断言、Section 全家族、PolylineEditOpsTest、ModifyMenuTest/DrawingMenuTest/SelectMenuTest UI 契约）——arcThrough 端点保持修复未破坏任何消费方。

### 6.3 测试定位备注（本轮两处 FAIL 均为断言问题，实现正确）

1. **矩形包围盒**：断言误用 `QRectF::bottom()`（Qt 屏幕语义 = y()+height() = max-y）。世界坐标下 min-y 边是 `top()`，已改；
2. **id 最近图元**：测试模型中直线与矩形下边共线，查询点到二者距离**并列 1.0**，`idPoint` 严格 `<` 按向量顺序取先者（矩形）。直线已挪至 (30,0)-(40,0)、查询点 (35,2) 消除并列——这也验证了「最近图元」输出真实可达。

### 6.4 本轮新增测试（直线链 + GUI 入口）

- **InquiryTest 第 8 节（tests/inquiry_test.cpp，新增 8.1–8.3）**：五边形 A(0,0) B(40,0) C(55,25) D(30,50) E(5,30) 手算 shoelace = 1825 mm²。
  - 8.1 `buildRingFromLines` 乱序/反向串环、5 顶点、面积 1825、周长逐边（CD = hypot(25,25)）；
  - 8.2 缺口链拒绝（D→E 缺段）；
  - 8.3 命令层 `m.setSelection(ids)` 批量选中 5 直线 → `area` 输出「直线链模式」「面积 = 1825」「5 条边」；缺口选区报错含「封闭环」。
- **InquiryDialogTest（tests/inquiry_dialog_test.cpp，17 断言）**：锁三条不变量——① 菜单栏【测量查询】存在 + 「打开测量查询面板...」动作带图标；② 工具栏【测量查询】按钮存在（图标 + 文字 + tooltip 非空）；③ 对话框四查询功能闭环（10×20 矩形 → 距离 10 mm / 面积 200 mm² / 质量特性 / 点坐标最近图元）+ 直线链（4 直线 10×10 正方形 → 面积 100 mm²，用 `m.setSelection({l1,l2,l3,l4})`）。结果落盘 `inquiry_dialog_test_result.txt`。

## 7. 测量查询 GUI 入口（菜单 / 工具栏 / InquiryDialog 对话框）

### 7.1 设计与定位

- 与命令行风格解耦：GUI 入口与 `dist`/`area`/`massprop`/`id` 命令层**共用 `inquiry::` 只读核心层**（几何事实单一来源，UI 不重复计算）；
- 入口唯一：`MainWindow::openInquiry()`，主菜单与工具栏按钮共用，避免双份状态。

### 7.2 图标（icons::query）

- `src/ui/icons.h` 程序化绘制（`icons::make("query")`）：放大镜（椭圆 + 手柄）+ 镜内水平尺寸双向箭头，风格与现有 `block`/`dim` 等 glyph 一致，固定 2× 光栅化。

### 7.3 对话框（InquiryDialog）

- 模态 `QDialog`，`QTabWidget` 四页：**距离 / 面积 / 质量特性 / 点坐标**，结果区 `QPlainTextEdit` 只读；
- 复用 `inquiry::` 只读接口：
  - `doDist()` → `distTwoPoints`（dX1/dY1/dX2/dY2）；
  - `doArea(bool)` → 选区/ID 列表用 `areaOfElement`，点列框用 `areaOfPolygon`，**全部为 Line 时走 `areaOfClosedLineChain` 直线链分支**；
  - 质量特性页 → `massPropOfElement`；
  - 点坐标页 → `idPoint`（iX/iY）；
- 输入框 `objectName` 设 `distX1/distY1/distX2/distY2/idX/idY` 供 UI 测试定位；helper `parsePoint` / `collectTargets`（选区或 ID 解析）。

### 7.4 菜单与工具栏入口

- **主菜单**：新增顶级【测量查询(&I)】，位于【绘图(&D)】与【视图(&V)】之间，动作「打开测量查询面板...」带 `query` 图标（`icons::make("query")`）；
- **工具栏**：第二行【视图】组末尾新增「测量查询」按钮（图标 + 文字一致，tooltip 非空），共用 `openInquiry()`；
- 菜单测试按名称查找、工具栏 `ToolbarLayoutTest` 仅断言第二行宽度上限（~1358 px），新增菜单/按钮均不破坏既有契约。

### 7.5 CMake 注册与测试契约（InquiryDialogTest，17 断言）

- `CMakeLists.txt`：`UI_SOURCES` 追加 `src/ui/InquiryDialog.cpp`；新增 `InquiryDialogTest` 目标，链接 `MainWindow.cpp` + `CORE_SOURCES` + `UI_SOURCES`，`target_link_libraries(... Qt6::Widgets)`；
- 契约三锁：① 菜单【测量查询】存在 + 动作带图标；② 工具栏【测量查询】按钮存在（图标 + 文字 + tooltip 非空）；③ 四查询闭环 + 直线链（4 直线 10×10 正方形 → 面积 100 mm²）。

### 7.6 关键教训

- `ElementFactory::create*` **不**入模型 → 测试须 `m.addElement(ElementFactory::createRect(...))` 拿真实 id，否则 `model->elements()` 空、选区/ID 查询无目标；
- 选区 API：`SketchModel::selectEntity(id)` 是**替换式**（`selectedIds_ = {id}`），批量选中须 `setSelection(ids)`，否则循环 `selectEntity` 后选区只剩最后一条 → 走不进直线链分支。

## 8. 已知限制与后续方向

- 椭圆惯性矩、含弧折线惯性矩为离散近似（与渲染采样同源，误差 ~0.1%），如需解析可后续推导旋转椭圆积分；
- 闭合样条面积/周长依赖 Catmull-Rom 采样密度（与渲染一致，改采样密度需同步）；
- `area`/`massprop` 选区模式暂不支持框选交互（沿用模型选区）；
- MASSPROP 尚未输出回转半径与主惯性矩/主轴方向（AutoCAD 支持），可作后续增强；
- 命令暂无交互式点取（`dist` 等需键盘输入坐标），后续可接入画布点取模式（与镜像 pointPickMode 同机制）。
