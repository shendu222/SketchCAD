# 倒角（Chamfer）/ 倒圆（Fillet）核心层设计分析

> 模块位置：`src/core/ChamferFillet.h` / `src/core/ChamferFillet.cpp`
> 命名空间：`SketchCAD::corner`
> 设计目标：对两条开放直线段在它们所在直线的交点（角点）处做倒角或倒圆；
> 纯几何、不依赖模型与界面，**可独立调用、易于扩展**。

---

## 1. 为什么独立成模块

- 既有 `modify::makeFillet`（半径>0 倒圆、半径=0 退化为「交点截断」）已被 `SketchSelfTest` 断言
  （2 删 + 1 弧），**不可改动**。新增的「距离-距离倒角」「距离-角度倒角」属于不同的参数化语义，
  放在新模块避免回归。
- 新模块只依赖 `Element` 与 `Vec`（几何原语），无 `SketchModel` / Qt Widgets 依赖，
  可在命令行、菜单、交互工具、甚至未来的「轮廓/多段线角点批处理」中直接复用。

---

## 2. 返回值：`CornerResult`

```cpp
struct CornerResult {
    Element lineA;    // 裁短后的第 1 条（保留端 + 裁断点），type = Line
    Element lineB;    // 裁短后的第 2 条
    Element corner;   // 倒角直线(Line) 或 倒圆圆弧(Arc)，继承源 a 的样式/图层/线宽
    bool    ok = false;
    QString msg;      // 失败原因（平行 / 共线 / 夹角过小 / 参数非法 / 距离越界 / 角度越界 / 非直线）
};
```

字段语义：

| 字段 | 含义 |
|------|------|
| `lineA` / `lineB` | 原两直线被裁短后的结果；**保留端（离角点远的一端，或点选端）端点不变**，仅靠近角点的一端被移到「裁断点」 |
| `corner` | 倒角时为一条 `Line`（连接两裁断点）；倒圆时为一条 `Arc`（与两线相切）。其 `pts[0]`=圆心、`radius`+`startAngle`/`endAngle`（度） |
| `ok` | `true` 表示几何有效；`false` 时 `lineA/lineB/corner` 无意义，调用方应据 `msg` 提示并放弃落盘 |
| `msg` | 人类可读的中文失败原因，直接 `log()` 即可 |

---

## 3. 函数接口与参数定义

所有函数签名一致，仅参数不同：

```cpp
// 倒角（距离-距离）：沿 a 回退 d1、沿 b 回退 d2，补直线连接两个裁断点
CornerResult chamferPair(const Element& a, const Element& b,
                        double d1, double d2,
                        bool byPick = false,
                        const Vec3& pa = Vec3(), const Vec3& pb = Vec3());

// 倒角（距离-角度）：沿 a 回退 d，倒角直线与 a 夹角 angleDeg（朝 b 一侧），
// 自动求 b 上的回退距离 d2 = d · tan(angleDeg)（角点处三角形几何）
CornerResult chamferPairDistAngle(const Element& a, const Element& b,
                                  double d, double angleDeg,
                                  bool byPick = false,
                                  const Vec3& pa = Vec3(), const Vec3& pb = Vec3());

// 倒圆（半径）：补与两线相切的圆弧，圆心在夹角平分线上、距角点 r/sin(θ/2)
CornerResult filletPair(const Element& a, const Element& b,
                        double r,
                        bool byPick = false,
                        const Vec3& pa = Vec3(), const Vec3& pb = Vec3());
```

### 参数表

| 参数 | 类型 | 说明 |
|------|------|------|
| `a`, `b` | `const Element&` | 两条**开放直线段**（`ElementType::Line`，`pts` 恰含 2 点）。单位与工程一致（mm） |
| `d1`, `d2` | `double` | 沿 a / b 从角点回退的距离（≥0，且不超过该线可用长度） |
| `d` | `double` | 距离-角度模式下，沿 a 回退的距离 |
| `angleDeg` | `double` | 距离-角度模式下，倒角直线与 a 的夹角（度）；须 `0 < angleDeg < 两线夹角` |
| `r` | `double` | 倒圆半径（>0） |
| `byPick` | `bool` | `false`（默认）= 自动保留「远离角点」的一端；`true` = 保留离 `pa`/`pb` 最近的一端（与修剪/延伸交互一致） |
| `pa`, `pb` | `Vec3` | `byPick=true` 时的用户点选位置，分别决定 a / b 的保留端 |

> 约定：`byPick=false` 适合「选中两条线直接处理」；`byPick=true` 适合交互式两击拾取。
> 无论哪种模式，**只移动靠近角点的一端（裁断点），保留端端点恒不改变**。

---

## 4. 调用示例

### 4.1 纯几何（独立调用，无需模型）

```cpp
#include "core/ChamferFillet.h"
using namespace SketchCAD;

Element a = /* 一条 Line：(0,0)->(100,0) */;
Element b = /* 一条 Line：(0,0)->(0,100) */;

// 距离-距离倒角：各退 10 / 20
auto r1 = corner::chamferPair(a, b, 10.0, 20.0);
//   r1.lineA = (100,0)->(10,0)
//   r1.lineB = (0,100)->(0,20)
//   r1.corner = (10,0)->(0,20)，Line

// 距离-角度倒角：沿 a 退 10，倒角线相对 a 成 45°
auto r2 = corner::chamferPairDistAngle(a, b, 10.0, 45.0);
//   r2.corner = (10,0)->(0,10)，Line

// 半径倒圆：r = 15
auto r3 = corner::filletPair(a, b, 15.0);
//   r3.corner = Arc，圆心 ≈ (15,15)，radius = 15，
//   r3.lineA/lineB 裁断于 (15,0)/(0,15)
```

### 4.2 落盘到模型（命令行 / 菜单共用路径）

```cpp
#include "core/Modify.h"   // modify::makeCornerChanges / applyChanges

const Element* ea = model->element(id1);
const Element* eb = model->element(id2);
auto res = corner::chamferPair(*ea, *eb, d1, d2);   // 或 chamferPairDistAngle / filletPair
if (!res.ok) { model->log("倒角失败：" + res.msg); return; }

auto changes = modify::makeCornerChanges(id1, id2, res); // 5 个 Change：2 删 + 2 裁短 + 1 倒角/圆角
model->beginEdit(QStringLiteral("修改-倒角"));
modify::applyChanges(model, changes, QString());         // 统一落盘 + 撤销快照
```

### 4.3 命令行（已在 `Commands.cpp` 注册）

```
chamfer <id1> <id2> <d1> <d2> [angleDeg]
```

- 仅给 `d1 d2` → 距离-距离倒角
- 额外给 `angleDeg` → 距离-角度倒角（沿第 1 条线回退 `d1`，与第 1 条线成 `angleDeg`）

### 4.4 菜单（一步式，无对话框）

【修改 > 倒角...】对当前选中两条直线做默认距离（5mm）的距离-距离倒角，
保持与「反向 / 分解 / 删除」一致的非对话框一步式契约；参数化场景走 `chamfer` 命令行。
【修改 > 圆角...】沿用既有 `M_Fillet` 对话框（半径），未改动。

---

## 5. 边界与容差

| 场景 | 处理 |
|------|------|
| 非直线图元 | `ok=false`，`msg` 含「直线」 |
| 两直线平行（无交点） | `ok=false`，`msg` 含「平行」 |
| 近似平行（夹角 < `kThetaMin=1e-3` rad） | `ok=false`，含「无法倒角」 |
| 近似共线（夹角 > `kThetaMax=π-1e-3`） | `ok=false`，含「共线」 |
| 参数为负（距离 / 半径 < 0） | `ok=false`，含「非负 / 正数」 |
| 回退距离超出该线可用长度 | `ok=false`，`msg` 含「d1 / d2」定位 |
| 距离-角度的 `angleDeg` ≥ 两线夹角 | `ok=false`，含「夹角」 |

浮点判定：`math::lineLineIntersect` 分母阈值 `1e-12`；夹角阈值 `1e-3` rad；
端点下限 `1e-9`。

---

## 6. 易扩展性（扩展点）

1. **多角点 / 轮廓批处理**：`corner` 命名空间下可新增
   `chamferContour(const std::vector<Element>&)` / `filletContour(...)`，
   对多段线、矩形、多边形每个内角依次调用 `chamferPair`/`filletPair`，
   再批量生成 `Change` —— 核心几何零改动。
2. **新增参数化形态**：如「距离-比例」「对称倒角」，只需在 `corner` 内新增一个公共函数 +
   对应的 `makeCornerChanges` 适配，不影响既有接口。
3. **交互工具接入**：`byPick` 已为两击拾取预留；交互式 `handleChamfer`/`handleFillet`
   可在 `GraphicsView` 中复用 `corner` API（当前菜单走一步式，命令行走全参数）。
4. **样式继承**：`corner` 在产出几何时已复制源 `a` 的图层/颜色/线宽/线型/文本，
   落盘层（`makeCornerChanges`）仅改 `id`，新增图元天然继承源样式。

---

## 7. 测试覆盖

- `tests/chamfer_fillet_test.cpp`（`ChamferFilletTest`，37 断言）：
  距离-距离 / 距离-角度(45°等腰、30°→退距 `d·tan30≈5.7735`) / 半径倒圆(圆心(15,15)、裁断(15,0)(0,15)) /
  平行 / 负半径与距离越界 / 角度越界 / 点选 vs 自动保留端 / 非直线 /
  **模型集成**（`makeCornerChanges`+`applyChanges` 落地真实 `SketchModel`）。
- 回归：`SketchSelfTest`（1201 断言）、`ModifyMenuTest`、`DrawingMenuTest`、`TrimExtendTest` 全部 `exit 0`。
