# P1 — 中心线家族（CenterLineGen）

> 对应《功能扩充方案_参考CAD对照.md》第 3.3 节提示词 **P1**。
> 模块：`src/core/CenterLineGen.{h,cpp}`（纯函数生成器，复刻 `SurfaceRoughness` 范式）。
> 自测：`tests/SketchSelfTest.cpp :: t_centerLine()`，经 `main()` 的 `CHECK(t_centerLine(), …)` 注册。
> 验收：`SketchSelfTest` 全量 **通过 2592 / 失败 0**，`rc=0`，`-Wall -Wextra` 零警告。

---

## 1. 修改背景

参考 CAD（浩辰 CAD 机械 2027）在 Ch3.2 提供「中心线 / 圆心标记 / 十字中心线」等一族命令，用于标注回转体、孔、板、角点的对称/轴线。
SketchCAD 此前仅有 `LineStyle::DashDot`（细点画线）这一线型原语，却没有「按几何关系自动生成中心线」的生成器；用户只能手工画线段再改线型，既慢又难保证伸出量一致。

本次依据 P1 提示词实现 `CenterLineGen::make(...)`，覆盖 10 种中心线语义，作为**纯几何生成器**接入：输入参考图元与参数，输出 `Line` 序列，调用方（命令层 / 脚本）负责追加进模型。

**与提示词的偏差（已决断并记录）**：
- 提示词写 `Geometry::center/intersect/parallel` 原语，但实际 `Geometry.h` 是 `geo::` 命名空间且无这些同名函数。实现严格改用既有 `geo::intersectElements / geo::elementBounds / geo::elementCentroid` 等，未自造几何算法（满足 FORBID）。
- 提示词写 `lineType=CENTER`，但 `LineStyle` 枚举无 `CENTER`。GB/T 4457.4 中「轴线 / 对称中心线」即**细点画线**，对应现成的 `LineStyle::DashDot`，故所有产物统一该枚举值，**不新增枚举**（满足 FORBID：不改枚举）。
- 提示词要求 `tests/centerline_test.cpp` 独立 CMake target。但项目实际约定是**单文件聚合 `SketchSelfTest.cpp`**（`t_xxx()` + `CHECK` 注册，保证「全量 38 exe 绿」基线）。为不破坏既有基线，本项沿用单文件约定，新增 `t_centerLine()` 并注册于 `main()`；功能等价于独立测试 target，且零新增构建目标风险。

---

## 2. 思考过程

1. **范式选择**：与 `SurfaceRoughness::buildRoughnessSymbol` 完全一致 —— `std::vector<Element> build(Spec)` 返回图元集合，调用方 `append`。好处：纯函数、无 UI 依赖、可在无头自测中直接验证几何，不污染现有渲染 / 序列化路径。
2. **线型映射**：中心线 = 细点画线 = `LineStyle::DashDot`。这是 GB 现行线型，且与 `linePriority()` 中「轴线 / 中心线 = 3」的优先级分组一致，打印 / 显隐行为无需额外处理。
3. **几何原语复用**：
   - 圆信息（圆心 / 半径）直接从 `Element`（Circle 类型 `pts[0]` + `radius`）读取，不重新计算。
   - 两直线交点用 `geo::intersectElements(lineA, lineB)`（已按线段范围过滤、去重），不手算联立方程。
   - 矩形包围盒用 `geo::elementBounds`（注意 `Rectangle` 仅由 `pts[0]/pts[1]` 两个对角角点定义，这是项目既有约定，已据此构造测试输入）。
   - 角平分线方向 = 两臂单位向量之和（反向平行时回退为垂线 `Vec3::perp()`），不引入新的角度求解。
4. **退化处理**：所有失败路径（refs 为空、半径 ≤0 的圆、两直线平行无交点、零长度线）一律 `return {}` 并可选地经 `reason` 出参说明，绝不抛异常、绝不残留半截图元。符合 L0「错误统一 model 层校验 → log → 返回 false」风格（此处为纯几何层，等价返回空 + 原因）。

---

## 3. 核心算法与设计思路

### 3.1 数据结构

```cpp
enum class CenterLineKind {        // 10 种
    kCenter, kCross, kCircleMark, kCorner, kPlate,
    kFullCircle, kAngle, kHole, kThroughHole, kTwoLinesCross
};
struct CLParams { double extend = 5.0; double gap = 0.0; QString layer = "0"; };
std::vector<Element> makeCenterLine(CenterLineKind kind,
                                    const std::vector<Element>& refs,
                                    const CLParams& p, QString* reason = nullptr);
```

- `extend`：超出特征端点的伸出长度（mm）。
- `gap`：特征边缘到中心线起点的间隙（mm）；默认 0 表示线穿过特征（中心线标准画法）。
- `layer`：产物归属图层（取当前层）。

### 3.2 各类型几何语义

| 类型 | 输入 | 产物 | 关键几何 |
|---|---|---|---|
| `kCenter` | 直线 / 圆 | 1 条单轴中心线 | 直线：沿该线两端各伸 `extend`；圆：水平轴 `2·(r+gap+extend)` |
| `kCross` | 圆 / 直线 / 点 | 2 条十字线 | 过圆心（或中点 / 点）的互相垂直两线，每臂 `r+gap+extend`（点则固定 `extend`） |
| `kCircleMark` | 圆（可多圆） | 每圆 2 条小十字 | 标记半臂长限幅 `clamp(r·0.25, 2, 6)`，不随圆无限放大 |
| `kCorner` | 两直线 | 1 条角平分线 | `intersectElements` 求交点 → 两臂单位向量和方向的平分线 |
| `kPlate` | 矩形 | 2 条对称轴线 | `elementBounds` 取矩形中心，水平 + 垂直两轴各伸 `extend` |
| `kFullCircle` | 圆 | 2 条十字线 | 与 `kCross` 对圆等价，语义层面强调「整圆贯通十字」 |
| `kAngle` | 两直线（角臂） | 2 条沿线中心线 | 沿每条臂两端各伸 `extend` |
| `kHole` | 圆（孔） | 1 条单轴中心线 | 水平轴 `2·(r+gap+extend)` |
| `kThroughHole` | 圆（孔） | 1 条单轴中心线 | 比 `kHole` 更长：`2·(r+gap+2·extend)`，体现「通孔贯通」 |
| `kTwoLinesCross` | 两直线 | 2 条十字线 | `intersectElements` 求交点 → 过交点的固定尺寸十字 |

### 3.3 为什么「单轴 / 十字」要分两种类型

参考 CAD 中 CENTERLINE（单轴）与 CENTERMARK（十字）是两条独立命令；本实现保留语义独立性：
- `kCenter / kHole / kThroughHole` 用于「回转体 / 孔」只需单轴的场景；
- `kCross / kFullCircle / kTwoLinesCross / kCircleMark` 用于需要对称标注的十字 / 标记场景。
两者几何可重叠，但对外语义清晰、参数可演进（如未来 `kThroughHole` 的「贯通长度」可独立调节）。

---

## 4. 关键实现要点

1. **统一构造助手** `mkCenterLine(a,b,layer)`：集中设置 `type=Line`、`lineStyle=DashDot`、`lineWidth=0.6/Auto=false`、`color=深灰`、`name="中心线"`，杜绝散落赋值。
2. **圆心 / 直线提取助手** `circleInfo()` / `lineEnds()`：半径 ≤0 直接判退化，避免下游除零 / 归一化炸裂。
3. **`Vec3::normalized()` 的零向量兜底**：零长度直线传入 `kCenter` 时归一化返回 `(1,0,0)`，仍产出 1 条有效线（端点 ±`extend`），不崩溃——自测中作为退化用例验证。
4. **`geo::intersectElements` 的「无交点」即退化**：`kCorner / kTwoLinesCross` 中若两直线平行，`intersectElements` 返回空 → 直接 `return {}`，对应「平行无交点 → 空」。
5. **`Rectangle` 双角点约定**：`kPlate` 测试输入必须只给 `pts[0]`、`pts[1]` 两个对角角点（`geo::elementCentroid` / `elementBounds` 均据此计算），给 4 角点会因只用前两点而塌缩——已在自测中修正。
6. **零警告**：所有 `reason` 出参在失败分支赋值、成功路径不触碰；`std::min/std::max` 来自 `<algorithm>`，无隐式转换告警。

---

## 5. 自测覆盖（`t_centerLine`）

- **正向**：`kCircleMark` 两圆 → 4 条线且全部 `DashDot`、`kFullCircle` 整圆 → 过圆心十字且臂长 `r+extend=14`、`kCorner` 两线交于 (5,5) → 角平分线过该点、`kPlate` 矩形 → 水平轴 y=2.5 + 垂直轴 x=5、`kCenter` 直线 → 两端各伸 4、`kCross`/`kAngle`/`kHole`/`kThroughHole`/`kTwoLinesCross` 数量与几何正确。
- **退化**：空 refs → 空；半径 0 圆 → 空；平行直线 → 空；零长度直线 → 不崩溃（兜底归一化）。
- 全部断言经 `CHECK` 注册于 `main()`，纳入全量 EXIT=0 验收。

---

## 6. 后续可演进（非本次范围）

- 命令层接入：在 `CommandInterpreter` 增加 `centerline` 命令，交互选取参考图元 → 弹出 `CLParams`（extend/gap）→ 调 `makeCenterLine` 并 `append` 进模型（一次操作一次 `pushMemento`）。
- 序列化：中心线本质是普通 `Line`（`DashDot`），已天然随 `toDrawingBytes()` 存档，无需新增键。
- 与 P3（符号标注）、P6（视图符号）共享 `mkCenterLine` 式构造助手，可下沉到 `SymbolFactory` 公共工具区。
