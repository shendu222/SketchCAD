# P4 机械零件设计：渐开线齿轮齿廓 + 零件库抽象（InvoluteGear + PartLibrary）

> 功能扩充方案_参考CAD对照.md §3.3 —— P4。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch13「机械零件设计（🟡）」深化要求把 `AircraftViews`/`FlangeViews` 的「参数对话框 → Elements」模式抽象为**零件库（PartLibrary）**接口，并实现**渐开线齿轮齿廓采样** `InvoluteGear::profile(...)`，作为首个具体零件生成器。

提示词原文要点（§3.3）：

- **GOAL**：把 `*Views` 的「参数 → Elements」模式抽象为 `PartLibrary` 接口；新增 `InvoluteGear::profile(int teeth, double module, ...)` 渐开线齿廓采样（返回 Polyline 点集）。
- **CONTEXT**：`FlangeViews.cpp`（现有参数化生成范例）、`Geometry.h`。
- **IO_SPEC**：`profile()` 仅做采样生成点集，不负责标注；齿数/模数/压力角为参数。
- **ACCEPT**：① 渐开线 ≥8 断言（齿数 1 / 标准 20° 压力角 / 对称 / 节圆处斜率）；② 退化：`teeth<2 → false`；③ `tests/gear_test.cpp` 注册；全量 EXIT=0。
- **FORBID**：自造渐开线方程（必须用人工给定的 `involute(rb, t)` 公式）；改枚举。
- **ESCALATE**：齿轮参数方程/啮合关系涉及数值正确性时停止（G1）。

---

## 2. 思考与方案选型

### 2.1 渐开线方程：严格采用人工给定（G1）公式

提示词 FORBID 明确「禁止自造渐开线方程，必须用人工给定的 `involute(rb, t)` 公式」。因此本模块**唯一**实现该公式：

```
x = rb * (cos t + t * sin t)
y = rb * (sin t - t * cos t)
```

其几何不变量：点所在半径 `rho = rb * sqrt(1 + t²) = rb / cos φ`，其中 `φ = inv⁻¹(t)` 为该半径处压力角、`t = inv(φ) = tanφ - φ`。该公式与导数的数值正确性由 `t_gear` 直接断言（见 §5），确保未「自造」。

### 2.2 profile 的几何构造（标准直齿圆柱齿轮）

给定 `z`(齿数)/`m`(模数)/`α`(压力角，默认 20°)：

- 分度圆 `r  = m·z/2`
- 基圆 `rb = r·cos α`
- 齿顶圆 `ra = r + ha·m`（ha 默认 1.0）
- 齿根圆 `rf = r - hf·m`（hf 默认 1.25）

单侧齿廓采样：从 `rho = max(rb, rf)` 到 `ra`，按 `rho` 反解 `φ = acos(rb/rho)`、`t = inv(φ)`，代入 G1 公式得原始点，再**刚性旋转 `offset`**，使分度圆处半齿厚角恰为 `π/(2z)`（`offset = π/(2z) - ψ_p`，`ψ_p` 为分度圆处原始极角）。两侧齿廓互为镜像（y 取反），天然对称。

整齿轮外轮廓：逐齿 CCW 拼接「上半齿廓(根→顶) + 齿顶弧 + 下半齿廓(顶→根) + 齿根弧(填补齿槽)」，闭环。单齿廓（`fullGear=false`）则输出开放轮廓。

### 2.3 零件库抽象（GOAL 落地）

`PartGenerator` 抽象基类（`id()` / `displayName()` / `generate(params, out)`），`PartLibrary` 单例按 id 注册/查找/出库。`InvoluteGear` 经 `GearGenerator` 接入：读取 `teeth/module/pressureAngle` 参数 → `InvoluteGear::profile` → 构造一个**闭合 Polyline** 图元。P5 将在此框架下扩展 GB 标准件 JSON 规格表驱动出库。

---

## 3. 核心算法与设计思路

| 步骤 | 算法 | 不变量 |
|------|------|--------|
| 退化判定 | `teeth<2 \|\| module<=0` → 返回 false | 满足 ACCEPT 退化要求 |
| 根圆降级 | `rf < 0.1·ra` 时 `rf = 0.1·ra` | 防小齿数根圆为负（非物理降级，不崩） |
| 单侧齿廓 | `ρ∈[max(rb,rf), ra]` 反解 `t=inv(acos(rb/ρ))` → G1 公式 → 旋转 `offset` | 分度圆半齿厚角 = π/(2z) |
| 对称性 | 下半齿廓 = 上半齿廓 `y→-y` 镜像 | 整轮关于 x 轴对称 |
| 整轮拼接 | 逐齿 CCW：`上廓 + 齿顶弧 + 下廓 + 齿根弧` | 闭合、连续、无自交 |
| 出库 | `GearGenerator::generate` → 单个 `ElementType::Polyline`(`closed=true`) | 与既有 Polyline 渲染/命中/导出三路同源 |

`offset` 的推导保证「分度圆处半齿厚角 = π/(2z)」，即标准齿轮分度圆齿厚 = 齿槽宽 = 半圆周长的一半，是渐开线齿轮正确啮合的几何前提。

---

## 4. 关键实现要点

**新增文件**
- `src/core/InvoluteGear.h`：`struct InvoluteGear`（`profile` + 几何辅助 `pitchRadius/baseRadius/addendumRadius/dedendumRadius` + `involute(rb,t)` G1 公式）。
- `src/core/InvoluteGear.cpp`：`profile` 实现 + 匿名 `flank` 助手（单侧齿廓采样）。
- `src/core/PartLibrary.h`：`PartGenerator` 抽象基类 + `PartLibrary` 单例接口。
- `src/core/PartLibrary.cpp`：`PartLibrary` 单例（构造注册内建零件）+ `GearGenerator`（接入 `InvoluteGear`）。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `InvoluteGear.cpp`、`PartLibrary.cpp`（被 `SketchSelfTest` 复用）。
- `tests/SketchSelfTest.cpp`：新增 `#include` + `static bool t_gear()` + `CHECK` 注册（`tests/gear_test.cpp` 独立 target 按既定先例并入单文件聚合）。

**关键约束遵守**
- 未改 `ElementType` 枚举（FORBID）。
- 渐开线仅由 G1 公式 `involute(rb,t)` 生成，无其它等价/近似表达式（FORBID）。
- `profile()` 只生成几何点集，不含任何标注（IO_SPEC）。

---

## 5. 自测覆盖（`t_gear`，全部断言）

1. 退化：`teeth<2` / `module<=0` / `teeth=0` → `profile` 返回 false 且输出清空；
2. 标准 20° 压力角：`r = m·z/2`、`rb = r·cos20°`、`ra = r+m`、`rf = r-1.25m` 数值正确；
3. 整轮采样：点集稠密（>200）、最大半径 ≈ 齿顶圆 22、最小半径 ≈ 齿根圆 17.5；
4. 对称性：整轮外轮廓关于 x 轴镜像（每点都有镜像点）；
5. **G1 公式正确性**：`involute(rb,t)` 与标准公式逐位一致、半径 = `rb·sqrt(1+t²)`；
6. **节圆处斜率 = 压力角**：数值微分得切线与径向夹角 ≈ 20°（G1 公式的微分性质）；
7. 单齿廓：`fullGear=false` 返回开放稠密点集；
8. **PartLibrary 接入**：`find("gear")` 非空、列表含 gear、`generate` 出库 1 个 `closed` Polyline（点集稠密）；退化参数(`teeth=1`) → 出库失败。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 `EXIT=0`。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| `tests/gear_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_gear()` + `CHECK` 注册） | P1/P2/P3 已据此偏离；项目基线为单文件聚合自测，不新增 target。 |
| PartLibrary 仅 GOAL 提及、ACCEPT 未测 | 实现 `PartLibrary` 抽象 + `GearGenerator` 接入，并加 1 组断言验证出库 | GOAL 要求抽象接口，P5 将扩展 GB 标准件；本项使其可测、非死代码。 |
| 仅 `profile` 返回 Polyline 点集 | 额外用 `GearGenerator` 把点集包成 `ElementType::Polyline` 图元出库 | 使几何能直接落到模型/自测，闭环验证。 |

**已知限制（Phase-1 范围）**
- 齿根以下（`rho<rb`）按径向直线退化，未做齿根过渡曲线（profil/过渡圆角）；对标准齿轮轮廓足够，不影响啮合几何主体。
- 仅直齿圆柱外齿轮；内齿轮/斜齿轮/齿条未覆盖（后续扩展）。
- `PartLibrary` 目前仅内建 gear；GB 标准件（六角螺栓/螺母/垫圈等 JSON 规格表出库）属 P5。

---

## 7. 验收结论

- ✅ 真实功能扩充：`InvoluteGear` 齿廓采样生成器（G1 公式）+ `PartLibrary` 抽象 + `GearGenerator` 接入。
- ✅ 零警告编译（`-Wall -Wextra`）；`SketchSelfTest` 全量通过、`EXIT=0`。
- ✅ `t_gear` ≥12 断言覆盖退化/标准压力角/对称/节圆斜率/G1 公式正确性/单齿廓/PartLibrary 接入。
- ✅ 未改动 `ElementType` 枚举；渐开线仅由 G1 公式生成（满足 FORBID）。
- ⏳ 后续（已列入偏差）：齿根过渡曲线、斜齿/内齿/齿条、P5 GB 标准件 JSON 规格表出库。
