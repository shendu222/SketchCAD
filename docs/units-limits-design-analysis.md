# UNITS / LIMITS — 单位与图纸界限设计分析

> Request F 实现记录。范围：单位（UNITS）与图纸界限（LIMITS）设置面板、
> 设置的实时应用（绘图输入 / 坐标显示 / 参数条）、图纸文件内单位元数据持久化、
> DXF 导入导出的跨单位自动换算。

---

## 1. 问题定义与需求拆解

| 需求点 | 对应实现 |
|---|---|
| units/limits 设置面板 | `UnitsLimitsDialog`（长度/角度单位 + 精度 + 界限四值 + 检查开关） |
| 实时应用到绘图 | 参数输入条后缀与显示值按当前单位换算，编辑回写 mm |
| 实时应用到坐标显示 | `StatusBar::setCoord` 经 `setUnitsMeta` 缓存口径格式化 |
| 实时应用到捕捉计算 | 捕捉/栅格几何仍在 mm 域，无换算介入（见 §3.4） |
| 图纸文件持久化 | 顶层 JSON `units` / `limits` 键（缺键回落默认） |
| 导入导出自动换算 | DXF HEADER `$INSUNITS` + `scaleElementsToUnit` |
| 精度不丢失 | 显示层四舍五入，存储层 mm 全精度（见 §2.1） |

---

## 2. 核心决策与深思过程

### 2.1 单位不变式：内部几何一律 mm（本设计的第一原则）

三个候选架构：

| 方案 | 做法 | 否决/采纳理由 |
|---|---|---|
| A. 几何随单位重算 | 切单位时缩放所有图元 | ❌ 破坏性：撤销栈、块表、Xref 全部要跟随重算；且"换显示单位"语义上不该动几何 |
| B. 双域存储 | 每个长度字段存 (value, unit) | ❌ 字段爆炸：Element 十余个长度量全部要改结构，序列化体积翻倍，旧档全部失效 |
| C. **存储恒 mm，单位只是显示/输入口径** | 唯一换算点在 UI 层（显示）与导入/导出边界（交换） | ✅ 与 AutoCAD `MEASUREMENT`/`$INSUNITS` 语义一致；旧档零迁移；换算只有两处，可审计 |

选 C。推论：

- **显示精度 ≠ 存储精度**。`formatLengthMm` 只是显示层四舍五入（0..6 位），存储值永不损失。用户把精度从 2 调到 4，图形不动、显示多两位——这正是"精度不丢失"的根源。
- 换算必须集中。所有 UI 出口（状态栏、参数条、面板预览）统一走 `SketchModel::formatLength/formatLengthWithUnit/formatAngle` 三个便捷口，杜绝各处手写换算口径漂移。

### 2.2 角度为什么只有 DMS 是"显示格式"，没有输入格式

角度单位四种：十进制度 / 度分秒(DMS) / 弧度 / 百分度。

- 显示全部支持（`formatAngleDeg`）；
- **输入一律十进制度**（paramBar 角度框、旋转命令等）。

理由：DMS 输入需要 `12d34'56"` 式的多段解析器，而本程序的角输入几乎都是
"直接键入十进制度"的工作流（绘图角度/旋转角），为低频场景引入二义性解析器
（`12.5` 是 12.5° 还是 12°05'？）得不偿失。DMS 只出现在查询/测量结果展示
（`12°34′56.8″`，进位边界 59.9995″ 已处理）。

### 2.3 LIMITS：告警而非拦截（AutoCAD 语义）

- `checkOn` 开启后，`SketchModel::addElement` 对新图元调 `checkElementLimits`，
  越界仅 `qWarning` 记日志，**不拒绝入模**。
- 判定口径：不用整框判定（图幅外的大圆心在界内会漏报），也不用全点判定
  （任一点出界即报警会误伤刻意的出血线）。折中：
  **中心在界外 且 整框与界限盒完全不相交** 才告警——只抓"整段明显画到界外"的失误。
- 界限坐标恒为模型 mm，与单位设置**无关**（它们是纸面定义，不是显示口径）。
- 非法范围（x2<=x1 等）在 `setLimits`/`fromJson` 双入口回落默认 A3。

### 2.4 捕捉计算为什么不需要动

捕捉/栅格/正交的吸附在**屏幕像素→模型 mm** 的几何域内完成，输出的落点
本来就是 mm。单位只影响"落点显示成什么字符串"。若在捕捉层再做单位换算
反而会引入双重换算 bug（换算两遍 = 换算错一遍）。因此本需求的"捕捉计算"
侧的正确动作是：**确认其保持 mm 域、不加换算**，并让状态栏显示与落点一致。

### 2.5 配置不入撤销栈（应用级配置语义）

与打印设置、命名过滤器（savedFilters）同级：单位/界限是"看图的方式"而非
"图的内容"。`setUnits/setLimits` 只发专用信号（`unitsChanged/limitsChanged`），
不发 `modelChanged`——否则 Ctrl+Z 会出现"数值没变、显示变了"的不可理解中间态。
随图 JSON 持久化但缺键回落默认（旧档零改动兼容）。

### 2.6 跨单位交换：$INSUNITS 是语义锚点

- **导出**：DXF HEADER 写 `$INSUNITS`（AutoCAD 标准码 0/1/2/4/5/6）。
  内部几何恒 mm，码值声明的是"文件里数值的口径"。
- **导入**：`fromDxf` 解析 HEADER 得码 → `DxfImportStats::insunits` 回传 →
  `lengthUnitFromInsunits` 还原单位 → `scaleElementsToUnit(elems, from)` 整体换算到 mm。
  `0=无单位`/缺头按 mm 解释（不缩放）+ 日志提示。
- **换算锚点 = 绝对原点**（x' = x×f）。这与"修改比例"的内容中心锚定不同：
  单位换算改的是度量口径不是图形位置，以原点为基准才与外部 CAD 的行为一致。
- **覆盖字段**：pts/holes/radius/radiusY/dimTextHeight/mtextWidth/rowHeight/colWidths。
  **不动**：lineWidth/lineWidthAuto（纸面量，随纸张不随度量）、角度（无量纲）、
  公差上下偏差（按标注语义保守不缩，见 §4 已知取舍）。
- **DWG（自研原生 R12）**：二进制容器无标准单位头，按 mm 直写直读（本程序
  导出的 DWG 口径恒 mm）；**STEP**：为只写格式，曲线集交换无单位语义，沿用 mm。

### 2.7 信号驱动的实时性（而非轮询/强制全刷）

```
UnitsLimitsDialog.accept()
  └─ model_->setUnits(u)            [u==现值则幂等返回，不发信号]
       └─ emit unitsChanged()
            ├─ MainWindow: statusBar_->setUnitsMeta(units)   → 下次 cursorMoved 起新口径
            └─ MainWindow: applyUnitsToParamBar()            → 后缀+显示值换算(blockSignals 防回环)
```

幂等短路保证读档（`setUnits(默认值)`）不产生虚假刷新；`blockSignals` 保证
参数条刷新期间 `valueChanged` 不回环把"已换算的显示值"再写回模型。

---

## 3. 数据结构

### 3.1 UnitsMeta / LimitsMeta（Units.h）

```cpp
enum class LengthUnit { Mm=0, Cm, M, Inch, Foot };   // 只增不改，JSON 按中文名持久化
enum class AngleUnit  { Deg=0, DMS, Rad, Grad };

struct UnitsMeta  { LengthUnit length; AngleUnit angle; int lengthPrec, anglePrec; toJson/fromJson/==/!= };
struct LimitsMeta { bool checkOn; double x1,y1,x2,y2; contains/isValid/toJson/fromJson };
```

- JSON 键用中文名（`"毫米"`…）：与线型名/对齐名的既有持久化契约一致，
  天然防枚举重排破坏旧档。
- `fromJson` 全字段容缺 + `std::clamp(0,6)` 防越界精度。

### 3.2 输入解析（parseLengthToMm）

当前单位为默认口径；**显式后缀覆盖**：`mm/cm/in/ft/m`（不区分大小写、允许
`"100 mm"` 中间空格）与 `"`（英寸）/`'`（英尺）。后缀表**从长到短排列**
（mm/cm/in/ft 先于 m），否则 `"100mm"` 会被 `m` 误吞尾部。

### 3.3 SketchModel 侧 API

```cpp
const UnitsMeta& units() const;  void setUnits(const UnitsMeta&);   // 幂等 + unitsChanged
const LimitsMeta& limits() const; void setLimits(const LimitsMeta&); // 非法回落 + limitsChanged
QString formatLength(double mm) const;          // 纯数值（当前单位+精度）
QString formatLengthWithUnit(double mm) const;  // 数值+短后缀（状态栏）
QString formatAngle(double deg) const;
void checkElementLimits(const Element&);        // addElement 挂接点
```

### 3.4 UI 层

- `UnitsLimitsDialog`：单位/精度/界限分区 + 实时格式化预览（1234.5mm / 45° 示例）。
- `StatusBar`：新增 `setUnitsMeta` 缓存口径，`setCoord` 用 `formatLengthMm`。
- `MainWindow::applyUnitsToParamBar`：参数条长度/间隔框换后缀+换算显示值
  （编辑时 lambda 反向 ×factor 回 mm）。

---

## 4. 兼容性与已知取舍

| 项 | 结论 |
|---|---|
| 旧 JSON 档 | 无 `units`/`limits` 键 → 回落 mm/度/2 位 + A3/不检查，零迁移 |
| 旧 DXF 档 | 无 `$INSUNITS` 或码=0 → 按 mm 解释不缩放 |
| 外部 DXF（英寸图） | 头码=1 → 导入时整体 ×25.4 到 mm，日志明示来源单位 |
| toDxf 签名 | 新增第 4 参 `int insunits=4` 默认值，既有调用零破坏 |
| DxfImportStats | 新增 `insunits` 字段（默认 4），既有消费方零破坏 |
| ElementType | 无新增图元类型，枚举与九步同步清单不受影响 |
| 公差字段 | tolUpper/tolLower 保守不随导入换算缩放（标注文字语义，见 Units.cpp 注释） |
| 角度输入 | 一律十进制度，DMS 仅显示（§2.2） |

## 5. 文件清单

| 文件 | 变更 |
|---|---|
| `src/core/Units.h/.cpp` | 新建：单位枚举/换算/格式化/解析/导入换算 |
| `src/core/SketchModel.h/.cpp` | units_/limits_ + set/get + format 便捷口 + checkElementLimits + addElement 挂接 |
| `src/ui/UnitsLimitsDialog.h/.cpp` | 新建：设置面板（含实时预览） |
| `src/ui/StatusBar.h/.cpp` | setUnitsMeta 缓存 + setCoord 单位化 |
| `src/ui/MainWindow.h/.cpp` | 菜单入口（文件→单位与图纸界限）、unitsChanged 联动、applyUnitsToParamBar、JSON 读写、DXF 导入换算/导出 $INSUNITS |
| `src/core/CadFormats.h/.cpp` | toDxf $INSUNITS 头、fromDxf HEADER 解析、DxfImportStats.insunits |
| `CMakeLists.txt` | 注册 Units.cpp / UnitsLimitsDialog.cpp |
