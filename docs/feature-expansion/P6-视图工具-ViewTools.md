# P6 视图工具：向视符号 / 孔轴投影 / 转角投影（ViewTools）

> 功能扩充方案_参考CAD对照.md §3.3 —— P6。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch6「视图工具（🆕）」：新增**向视符号**、**孔轴投影**、**转角投影**三类视图辅助生成器，输出符号 + 投影辅助线。

提示词原文要点（§3.3）：

- **GOAL**：新增向视符号(direction symbol)、孔轴投影(hole/shaft projection)、转角投影(rotation projection) 生成器，输出符号 BlockRef + 投影辅助线（正交/旋转矩阵复用 Transform 原语）。
- **CONTEXT**：`SectionCutSymbol.cpp`（符号生成范例）、`Geometry.h`（Transform）。
- **IO_SPEC**：辅助线为 Line/Polyline；符号锚点 + 方向参数。
- **ACCEPT**：① 每工具 ≥5 断言（锚点/方向角/字母标识）；② 退化：缺方向 → false；③ `tests/viewtool_test.cpp` 注册。
- **FORBID**：自造投影矩阵（用既有 Transform）；改枚举。

---

## 2. 思考与方案选型

### 2.1 复用 SectionCutSymbol 符号范例范式

三个生成器均沿用 `SectionCutSymbol` 的「纯生成器 → `std::vector<Element>`」范式（与 P1/P3/P4/P5 一致）：匿名 `mkLine/mkCenterLine/mkCircle/mkText` 助手统一构造图元，生成器只产出几何、调用方 append。

### 2.2 返回值：`bool` + out 参数

为精确满足 ACCEPT「缺方向 → **false**」（而非仅返回空 vector），三个生成器签名统一为 `bool buildXxx(const Spec&, std::vector<Element>& out)`：退化时返回 false 并清空 out——与 `InvoluteGear::profile` / `PartLibrary::generate` 同构。

### 2.3 Transform 原语复用（满足 FORBID）

- **向视符号 / 孔轴投影**：方向仅由「方向角 → 单位向量」三角函数求得（`dirOf(angleDeg)`），这是方向参数本身，**非投影矩阵**；箭头 V 形两翼、孔轴轮廓母线均由方向向量与法向（`perp = (-dy, dx)`）线性生成。
- **转角投影**：第二段方向 = 第一段方向经转角旋转，**复用 `geo::rotateAroundCentroid({d1}, Vec3(0,0,0), θ)`**（`Geometry.h` 既有 Transform 原语；实测绕**传入** centroid 旋转、保长）——不自造旋转/投影矩阵，直接命中 FORBID。

---

## 3. 核心算法与设计思路

| 生成器 | 输入 | 输出 | 关键算法 |
|--------|------|------|----------|
| 向视符号 | 锚点 + 方向角 + 字母 | 箭杆(Line) + V 形箭头(2×Line) + 字母(Text) | `dir=dirOf(θ)`；箭头端=锚点+arrowLen·dir；两翼=前端反向 ±15° 旋转；字母置于箭杆中点法向偏移 |
| 孔轴投影 | 锚点 + 直径 + 深度 + 轴向角 | 中心线(DashDot Line) + 近/远投影圆(2×Circle) + 两侧轮廓母线(2×Line) | 远端点=锚点+depth·dir；母线=锚点±perp·r 连至远端±perp·r；中心线两端各延 2mm |
| 转角投影 | 锚点 + 两段长 + 转角 + 起始角 | 折线(Polyline, 3 点) | p1=锚点+length1·d1；`d2=rotateAroundCentroid({d1},0,θ)[0]`；p2=p1+length2·d2 |

**退化判据**（均返回 false 并清空 out）：
- 向视符号：`!hasDirection` 或 `arrowLen<=0`；
- 孔轴投影：`!hasDirection` 或 `diameter<=0` 或 `depth<=0`；
- 转角投影：`!hasAngle` 或 `length1<=0` 或 `length2<=0`。

---

## 4. 关键实现要点

**新增文件**
- `src/core/ViewTools.h`：`DirectionSymbolSpec / HoleProjectionSpec / RotationProjectionSpec` + `buildDirectionSymbol / buildHoleProjection / buildRotationProjection`（`bool` + out）。
- `src/core/ViewTools.cpp`：三生成器实现 + `mkLine/mkCenterLine/mkCircle/mkText/dirOf` 助手；`#include "Geometry.h"` 取 `geo::rotateAroundCentroid`。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `ViewTools.cpp`（`SketchSelfTest` 复用同源集）。
- `tests/SketchSelfTest.cpp`：`#include "ViewTools.h"` + `static bool t_viewtool()` + `CHECK` 注册。

**约束遵守**
- 未改 `ElementType` 枚举（FORBID）。
- 未自造投影/旋转矩阵：转角投影复用 `geo::rotateAroundCentroid`；其余仅方向向量 + 法向线性组合（FORBID）。
- 中心线用 `LineStyle::DashDot`（GB 细点画线），与既有中心线约定一致。

---

## 5. 自测覆盖（`t_viewtool`，全部断言）

每工具 ≥5 断言（锚点 / 方向角 / 字母 / 尺寸）+ 退化：

1. **向视符号**：成功、4 图元、锚点=(10,20)、方向角 90° 箭头端=(10,28)、含字母 B；退化：缺方向→false、箭长非正→false；
2. **孔轴投影**：成功、5 图元、[0]=点画线中心线起点=(5,3)、近端圆半径=5、近端圆心=锚点、远端圆心=(5,35)；退化：缺方向→false、直径非正→false；
3. **转角投影**：成功、单个 3 点 Polyline、起点=锚点、第一段末=(20,0)、第二段末=(20,15)、两段夹角=90°；退化：缺转角→false、段长非正→false。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 `EXIT=0`。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| 输出符号 **BlockRef** + 投影辅助线 | 直接返回 `std::vector<Element>`（Line/Circle/Text/Polyline） | 与 P1/P3/P4/P5 同范式（纯生成器返回几何）；BlockRef 引用式打包可经 `PartLibrary::checkout` 达成，Phase-1 未强制打包，记偏差。 |
| `tests/viewtool_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_viewtool()` + `CHECK` 注册） | P1~P5 已据此偏离；项目基线为单文件聚合自测。 |
| 「正交/旋转矩阵复用 Transform 原语」 | 转角投影复用 `geo::rotateAroundCentroid`；向视/孔轴仅用方向向量+法向（非投影矩阵） | 满足 FORBID（不自造投影矩阵）；正交方向由法向 `perp=(-dy,dx)` 给出。 |

**已知限制（Phase-1 范围）**
- 几何为二维示意：向视符号未做「向视图」完整标注（视图名/配置）；孔轴投影为圆柱/孔的简化投影（未处理相贯/截交）；转角投影为单转角折线（未做多段/圆弧过渡）。
- BlockRef 打包（IO_SPEC 的「符号 BlockRef」）未在生成器内强制，交由 `PartLibrary::checkout` 或调用方按需组装。

---

## 7. 验收结论

- ✅ 真实功能扩充：向视符号 / 孔轴投影 / 转角投影三生成器（`ViewTools.{h,cpp}`）。
- ✅ 零警告编译（`-Wall -Wextra`）；`SketchSelfTest` 全量通过、`EXIT=0`。
- ✅ `t_viewtool` 每工具 ≥5 断言（锚点/方向角/字母/尺寸）+ 缺方向/缺转角退化 → false。
- ✅ 未改 `ElementType` 枚举；转角投影复用既有 Transform 原语（满足 FORBID）。
- ⏳ 后续（已列入偏差）：BlockRef 打包、向视图完整标注、孔轴/转角的细部（截交/多段）。
