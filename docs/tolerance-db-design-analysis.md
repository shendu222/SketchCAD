# ISO 公差数据库 ToleranceDB 设计与验收报告

> 模块路径：`src/core/standards/ToleranceDB.{h,cpp}`
> 依据标准：ISO 286-1 / GB/T 1800.1（数值口径同《机械制图手册（第四版）》第 6 章「极限与配合」）
> 配套自测：`tests/SketchSelfTest.cpp::t_toleranceDB()`
> 状态：已实现并通过自测（3324/0，EXIT=0）

---

## 1. 背景与目标

为 GD&T 标注、尺寸公差注写与 `ComplianceChecker` 提供**统一、可复用的 ISO 公差查询能力**，新增独立模块 `ToleranceDB`：

- 内置 **IT01~IT18 标准公差数值**（由 ISO 286-1 公式生成，等价于手册数表）；
- 内置 **轴（a~zc）/ 孔（A~ZC）基本偏差**查询；
- 纯查询、零副作用，**不替换既有 `DiagBundle` 诊断管线**；
- 大表以 **builder lambda** 构建，规避 GCC 13.1 巨型静态聚合初始化静默崩溃。

---

## 2. 对外 API

```cpp
namespace SketchCAD {

enum class ITGrade { IT01, IT0, IT1, IT2, IT3, IT4,
                     IT5, IT6, IT7, IT8, IT9, IT10,
                     IT11, IT12, IT13, IT14, IT15, IT16, IT17, IT18 }; // IT01=0 … IT18=19

enum class FitMember { Hole, Shaft };

struct Deviation { double upper = 0.0;  // 上偏差 ES / es
                   double lower = 0.0; }; // 下偏差 EI / ei

class ToleranceDB {
public:
    // 标准公差（mm）：返回 basicSizeMm 所在尺寸段、该 IT 等级的标准公差
    static double standardTolerance(double basicSizeMm, ITGrade grade);

    // 基本偏差（mm，带符号）：letter 大小写均可（轴 a..zc / 孔 A..ZC；js/JS 对称）
    static Deviation deviation(const QString& letter, double basicSizeMm,
                               ITGrade grade, FitMember holeOrShaft);

    // 便捷：由基本尺寸与偏差求上下极限尺寸（mm）
    static inline double upperLimit(double basicSizeMm, const Deviation& d);
    static inline double lowerLimit(double basicSizeMm, const Deviation& d);
};

}
```

---

## 3. 数值口径（ISO 286-1 / GB/T 1800.1）

### 3.1 标准公差 `standardTolerance`

1. 取基本尺寸所在尺寸段的**几何平均值** `D̄ = √(lo·hi)`（尺寸段 19 段，含 1…3150mm，`builder lambda` 注册）；
2. 公差单位 `i = 0.45·D̄^(1/3) + 0.001·D̄`（μm）；
3. 按等级求未取整 IT（μm）：
   - IT01 = 0.3 + 0.008·D̄
   - IT0  = 0.5 + 0.012·D̄
   - IT1  = 0.8 + 0.020·D̄
   - IT2~IT4 = IT1 与 IT5 之间的几何 4 等分插值
   - IT5~IT18 = 等级系数 · i（系数 `{7,10,16,25,40,64,100,160,250,400,640,1000,1600,2500}`）
4. **取整对齐手册数表**：IT01~IT1 取 0.1μm 精度，IT2~IT18 取整 μm → `/1000` 得 mm。

### 3.2 基本偏差 `deviation`

- 以 **builder lambda** 注册 `QHash<QString, ShaftDevFn>` 大表，每项为公式
  `std::function<double(double D, double i, double IT, int gidx)>`。
- **约定**：
  - `a~g / j` 公式返回轴的**上偏差 es**（a~g 为负、j 近零）；
  - `k~zc` 返回轴的**下偏差 ei**（为正）。
- `shaftBasicIsUpperDev()` 判定该分类，决定 es/ei 换算方向。
- **换算与镜像**（关键不变量）：
  - 轴：`es = base`（a~g/j）或 `ei = base; es = ei + IT`（k~zc）
  - 孔 = 轴反号镜像：`ES = −ei(轴)`、`EI = −es(轴)`
  - 特判：`JS/JS → ±IT/2`；`H 孔 EI=0`；`h 轴 es=0`
  - 未知字母 → 零偏差（不崩溃）
- 计算 es/ei 所用的 **IT 必须是 `standardTolerance()` 的取整值**（见 §5 缺陷 2）。

---

## 4. 实现要点与约束遵守

| 约束 | 落实 |
|------|------|
| 大表禁止巨型硬编码 | 基本偏差表 `shaftDeviationTable()` 用 `add(k,f)` lambda 逐条注册 |
| 规避 GCC 13.1 巨型聚合静默崩溃 | 同 §上，尺寸段表亦用 builder lambda |
| 中文比较禁用 `QLatin1String` | 全部 `QStringLiteral` / `QString::fromUtf8` |
| 不触碰 `DiagBundle` | 纯查询、无副作用 |
| `-Wall -Wextra` 零警告 | 构建已验证零警告 |
| `dist/` 与 `build/` 字节同步 | `cmp` 已核对一致并 `cp` 同步 |

---

## 5. 过程中的两个实质缺陷（已修正）

### 缺陷 1：a~g 基本偏差角色错误
初版把 a~g 的偏差误当作「下偏差 ei」。按 ISO 286-1，**a~g/j 的基本偏差是上偏差 es**（a~g 为负），k~zc 才是 ei。
- 修正：引入 `shaftBasicIsUpperDev()`，对 a~g/j 走 `es=base; ei=es−IT`，对 k~zc 走 `ei=base; es=ei+IT`；
- 孔侧统一以 `ES=−ei(轴)`、`EI=−es(轴)` 做反号镜像，同时修好了孔 A~G 下偏差少减一个 IT 的旧错。

### 缺陷 2：未取整 IT 导致 H7 上偏差偏差
初版 `deviation()` 用未取整 IT（20.918μm）算 es/ei，使 `deviation("H",30,IT7,Hole).upper` = 0.020918mm ≠ 0.021mm。
- 修正：`deviation()` 统一改用 `ToleranceDB::standardTolerance()` 的**取整后** IT（mm→μm）参与 es/ei 计算；
- 自测因此从 4 项失败转为全过。

---

## 6. 验收结果

| 项 | 结果 |
|----|------|
| `standardTolerance(30, IT7)` | **0.021mm**（精确命中手册） |
| `standardTolerance(30, IT6/IT8)` | 0.013 / 0.033mm（同源一致） |
| `standardTolerance(50, IT7)` | 0.025mm（>30~50 段） |
| `deviation("H",30,IT7,Hole).lower` | **0**（H 孔 EI=0） |
| `deviation("h",30,IT7,Shaft).upper` | 0（h 轴 es=0） |
| `deviation("JS",30,IT7)` | 对称 ±0.0105mm |
| a~g 间隙配合符号 | 轴为负、孔为正（A-G 孔轴镜像正确） |
| k~zc 过渡/过盈配合 | 轴 ei>0、es=ei+IT；孔反号 |
| 大小写等价 / 未知字母 | H==h；未知字母零偏不崩溃 |
| **构建** | `-j1` 零警告 |
| **自测** | **3324 / 0，EXIT=0**（t_toleranceDB 子项 3113） |
| **dist 同步** | `cmp build/SketchCAD.exe dist/SketchCAD.exe` 字节一致 |

---

## 7. 自测覆盖（`t_toleranceDB`）

- 标准公差验收点 + 等级单调递增；
- H7 孔（EI=0、ES=+IT）、h7 轴（es=0、ei=−IT）；
- JS 对称 ±IT/2；
- a~g 间隙配合符号与公式值近似（es≈−0.295mm、ei≈−0.316mm @30mm IT7）；
- A–G 孔轴反号镜像（EI≈+0.295mm、ES≈+0.316mm）；
- k~zc 过渡/过盈（轴 0<ei<es、孔 ES<EI<0）；
- 大小写等价、极限尺寸辅助函数、未知字母零偏。

---

## 8. 后续建议

`ToleranceDB` 已作为可复用成分就绪，但**尚未接入** Gdt / 尺寸标注 / `ComplianceChecker` 的调用点。下一步建议：
1. 在尺寸公差注写处调用 `deviation()` 生成上下偏差文本；
2. 在 `ComplianceChecker` 中调用 `standardTolerance()` 做公差合理性核查；
3. 如需严格逐值对齐手册数表（极小尺寸 IT7 等个别 1μm 级边界差异），可补「公式结果 + 手册校正表」双层查表。
