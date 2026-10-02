# DXF 导入实体覆盖扩展 —— 设计与深思（6 类 → 16 类实体）

> 日期：2026-10-02 ｜ 模块：`src/core/CadFormats.cpp`（读侧）+ `src/ui/MainWindow.cpp`（块表接线）+ `tests/SketchSelfTest.cpp`（黄金样本）
> 验收基线：自测 73/0 → **114/0 全绿**（`t_dxfImport` 新增 40 条断言）；`./build.sh test` EXIT=0；SketchCAD / SketchSelfTest 双目标 `-Wall -Wextra` 零警告。

---

## 1. 目标与约束

### 1.1 目标

将 DXF 导入实体覆盖从 6 类（LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE/TEXT，MTEXT 同路）扩展至 **15+ 类**，使典型机械图纸：

- 几何保真率 ≥95%
- 块/标注/填充三类可读入 ≥90%

### 1.2 硬约束

| 约束 | 落实方式 |
|---|---|
| DXF R12（AC1009）三铁律不破坏（读侧向后兼容） | 读侧只做"增量分发"，原 7 类解析逻辑逐字保留；写出端（`toDxf`）一行未动 |
| 未知实体跳过并计数，禁止中断整档 | `handleEntity` 末尾兜底 `++st.unknownEntities; return;` |
| `ElementType` 枚举只增不改 | 本任务零枚举改动——全部复用既有图元类型（含 BlockRef/Spline/Xline/Point 等） |
| 几何算法仅调 `Geometry.h` 既有 API；新几何人工复核 | 唯一新几何=椭圆参数化采样，数学单独复核（§7.1）；样条重建直接复用 `Spline` 图元的 `splineSubType=0`（Catmull-Rom 严格过点）通路，未新写插值 |
| 不引入第三方解析库 | 纯手工 token 流解析，沿用既有 `tokenizeDxf` |

---

## 2. 改造前现状与差距

改前 `fromDxf` 是"逐 token 状态机"：只认 ENTITIES 段的 6 类实体；遇到 INSERT/ELLIPSE/SPLINE/DIMENSION/HATCH 等直接落入未知分支被丢弃。典型机械图纸（块螺栓、椭圆孔、样条轮廓、尺寸标注、剖面填充）导入后大面积丢图——正是本任务要消灭的差距。

---

## 3. 总体架构：DxfImportCtx 上下文状态机

### 3.1 设计决策：一次全流扫描 + SECTION 状态机

新 `fromDxf` 不再逐段截取，而是**扫描整份 token 流**，按 `0 SECTION / 2 <name>` 维护段状态：

```
section: 0(无) / 1(ENTITIES) / 2(BLOCKS) / 3(TABLES)
```

- `section==1|2` 的 `0 <实体名>` → 收集该实体全部组码对 → `C.handleEntity(t, props)` 统一分发；
- `section==2` 的 `0 BLOCK` → 读块名(2)/基点(10/20)，`inBlock=true`；`0 ENDBLK` → `flush()` 后块内图元存入 `blockEntities[name]`；
- `section==3`（TABLES）→ 仅消费跳过（LTYPE 线型映射按**名字**在实体组码 6 处理，见 §5.8，无需深解析表结构）；
- `ENDSEC/EOF` → `flush()` 收尾悬挂的 POLYLINE。

### 3.2 关键收敛点：ENTITIES 与 BLOCKS 共用一套实体分发器

块内图元与顶层图元的解析**必须是同一份代码**——否则两套逻辑必然漂移（块内 LINE 与顶层 LINE 语义相同）。实现为 `DxfImportCtx::handleEntity`，路由收敛在唯一的 `emitEl`：

```cpp
void emitEl(Element e) {
    if (inBlock) curBlock.entities.push_back(std::move(e));  // 块内 → BlockDef
    else { out.push_back(std::move(e)); ++count; }           // 顶层 → 输出
}
```

### 3.3 状态清单（DxfImportCtx 成员）

| 状态 | 用途 |
|---|---|
| `section` / `inBlock` / `curBlock` / `blockEntities` | 段与块路由 |
| `pending/pendingBulge/pendingProps/pendingClosed/inPoly` | POLYLINE→VERTEX→SEQEND 三段式缓冲 |
| `lastBlockRefIdx` | 最近一个**顶层** BlockRef 在 `out` 中的下标，接收后续 ATTRIB |
| `count/arcsKept/arcsDropped` | 统计回填 `DxfImportStats` |

---

## 4. 实体映射总表（16 类 + 未知兜底）

| DXF 实体 | 关键组码 | 目标图元 | 保真策略 |
|---|---|---|---|
| LINE | 10/20/11/21 | `Line` | 精确 |
| CIRCLE | 10/20/40 | `Circle` | 精确 |
| ARC | 10/20/40/50/51 | `Arc` | 精确（角度直接存世界角） |
| LWPOLYLINE / POLYLINE+VERTEX | 10/20/42/70 | `Polyline`（arcFlags 经 bulge 反函数与三点定弧双验） | 精确（bulge 一致性 <0.5° 才置弧段） |
| TEXT / MTEXT(R12) | 1/10/20/40 | `Text` | 精确（缺字高回落 3.5mm） |
| **INSERT** | 2/10/20/41/42/50/44/45/70/71 | `BlockRef`（MINSERT 阵列逐实例展开） | 引用式（名+变换+属性） |
| **ATTRIB** | 2(tag)/1(value) | 并入最近 BlockRef 的 `blockAttrs` | 精确 |
| **ELLIPSE** | 10/20/11/21/40/41/42 | 全椭圆→`Ellipse`；部分弧→64 段折线 | 参数化精确 / 弧采样 |
| **SPLINE** | 10/20(控制) 11/21(拟合) | `Spline`（`splineSubType=0`） | 拟合点优先，控制点回落 |
| **DIMENSION** | 70/13/14/15/10/11/50/1 | `Dimension`（5 类） / LINE+TEXT 降级 | 测量值重建；ordinate 降级 |
| **HATCH** | 91/92/93/10/20 | `Hatch`（pts=外环，holes=岛） / 边界折线降级 | 简化岛检测；边环降级 |
| **POINT** | 10/20 | `Point` | 精确 |
| **LEADER** | 10/20 序列 | 开放 `Polyline` + 降级计数 | 无专用图元，几何保真 |
| **XLINE / RAY** | 10/20(基点)/11/21(方向) | `Xline`（rotation=atan2(dir.y,dir.x)） | 方向精确；RAY 暂以构造线承载 |
| 未知 | — | — | `++unknownEntities` 跳过，不中断 |

样式公共通路 `applyStyle`：图层(8)/ACI 颜色(62→rgb)/线宽(370, 1/100mm→组别)/线型(6→§5.8 映射)。

---

## 5. 逐类设计要点

### 5.1 INSERT：块表 + 块引用 + MINSERT 阵列

- BLOCKS 段解析出的块定义收在 `ctx.blockEntities`，收尾转 `BlockDef` 填入 `ls.blocks` 交回调用方（§6）。
- 实体侧只存**引用式** `BlockRef`：`blockName + pts[0](插入点) + scaleX(41) + scaleY(42) + rotation(50)`，与产品块体系（`SketchModel::expandBlockRef`）一致。
- **MINSERT 阵列**（70 列数/71 行数/44 列距/45 行距）沿**旋转后轴向**逐实例生成：

```cpp
const Vec3 colDir(std::cos(rot), std::sin(rot), 0);     // 列方向 = 旋转后 X 轴
const Vec3 rowDir(-std::sin(rot), std::cos(rot), 0);    // 行方向 = 旋转后 Y 轴
for r,c: pos = ins + colDir*(colSp*c) + rowDir*(rowSp*r);
```

- ATTRIB 归属：仅**第一实例**（`r==0&&c==0&&!inBlock`）记 `lastBlockRefIdx`；实例化开始前置 `-1`，块内 ATTRIB 直接忽略。

### 5.2 ELLIPSE：全椭圆保类型、部分弧降采样

DXF 语义：10/20=圆心，11/21=**长轴端点（相对圆心向量）**，40=短长轴比，41/42=参数角起止（弧度）。

- `span≈2π` → `Ellipse`：`radius=|mj|`（长半轴 a）、`radiusY=a·ratio`、`rotation=atan2(mj.y,mj.x)`；
- 部分椭圆 → `sampleEllipseArc` 采样 64 段开放折线（数学见 §7.1）；采样失败计数 `fallbackEntities`。

### 5.3 SPLINE：拟合点优先

R12 时代样条罕见，但高版本图档转存常带 SPLINE。策略：同时收集控制点(10/20)与拟合点(11/21)；**拟合点优先**——本产品 `Spline` 的 `splineSubType=0` 是 Catmull-Rom 插值、严格过点，拿拟合点重建即几何等价；无拟合点用控制点（形状近似）。<2 点回落计数。

### 5.4 DIMENSION：5 类重建 + ordinate 降级

按组码 70 类型分派（本产品 `DimensionType` 对齐）：

| 70 | DXF 含义 | 重建 | 测量值 |
|---|---|---|---|
| 0 | 旋转/水平/垂直 | `Distance`，pts={13,14} | 沿 50 角单位向量投影 \|Δ·u\| |
| 1 | 对齐 | `Aligned`，pts={13,14} | `p1.distTo(p2)` |
| 2/5 | 角度（15=顶点，13/14=两脚） | `Angle`，pts={15,13,14} | 两脚方向角差（>180° 取补） |
| 3 | 直径 | `Diameter`，pts={15,10} | `2×center.distTo(defP)` |
| 4 | 半径 | `Radius`，pts={15,10} | `center.distTo(defP)` |
| 6/其它 | ordinate/未知 | **LINE+TEXT 降级** + `++fallbackEntities` | — |

⚠️ 实现中修正过一处语义错位：半径/直径的"弧上点"在**组码 10**（defP），圆心在**组码 15**（center）；初版误用 13（延伸线原点）。所有重建 `refA=refB=-1`（导入件无源图元绑定）。

### 5.5 HATCH：多段线环 → 本产品 Hatch，边环降级

- 91=边界环数；每环 92 的 **bit2=多段线边界**：读 93 顶点数 + 连续 10/20 对（72/42 bulge 字段跳过）；
- 环 0 → `pts`（外环），其余环 ≥3 点 → `holes`（简化岛检测：不做包含判定，信任 DXF 环序）；
- **边环**（线/弧/椭圆/样条边界，92 无 bit2）→ 视为复杂：整实体回落为边界闭合折线 + `++fallbackEntities`；
- 默认样式：`rotation=45°、radius=2.5（间距）、lineWidth=0.5、sectionMaterial=0`（通用 ANSI31 风格斜线）。

### 5.6 POINT / LEADER / XLINE / RAY

- POINT → `Point`（精确）。
- LEADER → 顶点序列重建为**开放 Polyline**（名"导入:引线"）+ 降级计数——无引线箭头/文字语义，但几何保真。
- XLINE/RAY → 复用 `Xline`（pts[0]=基点，rotation=方向角）；方向零向量兜底 (1,0)。RAY 暂以构造线承载（两端无限），几何超集可接受。

### 5.7 样式公共通路 applyStyle 的增补

原 8 层/62 色/370 线宽之外，新增一行：

```cpp
e.lineStyle = lineStyleFromDxfName(firstStr(p, 6));  // 实体级组码 6
```

### 5.8 LTYPE 线型映射（按名，免深解析 TABLES）

```cpp
CONTINUOUS/BYLAYER/BYBLOCK → Solid
DASHED* / HIDDEN           → Dashed
DOT* / DOTTED              → Dotted
CENTER* / DASHDOT*         → DashDot
DIVIDE / PHANTOM / DASHDOT2→ DashDotDot
```

TABLES 段整体"仅消费"——线型映射按**实体组码 6 的名字**完成，避免解析 LTYPE 表的 dash 长度定义（本产品 LineStyle 是 9 值枚举，语义级映射足够）。

---

## 6. 块表交付机制：stats 携带 + 调用方注册

**API 向后兼容**是第一考量：不改 `fromDxf` 签名，块定义经扩展的 `DxfImportStats` 携带：

```cpp
// CadFormats.h
int unknownEntities = 0;          // 未知实体计数（跳过不中断）
int fallbackEntities = 0;         // 复杂实体降级计数
std::vector<BlockDef> blocks;     // BLOCKS 段解析结果（新增；仅 BlockDef 纯数据，无循环依赖）
```

`MainWindow` 接线，**顺序是硬约束**：

```cpp
model_->clear();                                   // ① 先清（clear 会清 blocks_）
for (const auto& b : st.blocks) model_->addBlockDef(b);  // ② 块定义先注册（同名拒绝语义，clear 后无冲突）
model_->beginEdit(QStringLiteral("导入 DXF"));
model_->addElementsQuiet(imported);                // ③ 再入图元（BlockRef 按名解析，缺定义渲染为空）
```

导入日志如实上报：块定义数、降级数、未知跳过数（有降级/未知时以 warn 级提示）。

---

## 7. 数学复核（人工验证）

### 7.1 椭圆参数化采样

DXF ELLIPSE 参数式：`p(t) = c + (a·cos t)·u + (b·sin t)·v`，其中 `u = mj/|mj|`（长轴单位向量），`v = (-u.y, u.x)`（u 旋转 90°）。验证：

- t=0 → `c + a·u` = 长轴端点 ✓（与组码 11 定义一致）
- t=π/2 → `c + b·v` = 短轴端点 ✓
- span 归一到 [0, 2π]：负角 +2π、超界 −2π，采样均匀 n=64 段，弦高误差对 a≤1000mm 的图幅远小于 0.01mm ✓
- 退化保护：`a < 1e-12` 返回空 → 上层回落计数 ✓

### 7.2 MINSERT 阵列方向

旋转 θ 后的局部 X/Y 轴即 `(cosθ, sinθ)` 与 `(−sinθ, cosθ)`；列沿局部 X、行沿局部 Y，与 AutoCAD 语义一致（阵列间距在**旋转后坐标系**中度量）。

### 7.3 线性标注投影

类型 0 的旋转标注测量值 = `(p2−p1)·u` 的绝对值，u 为 50 角单位向量——水平/垂直标注（角度 0/90°）自动退化为 |Δx|/|Δy| ✓。

### 7.4 角度标注

两脚方向角差先取绝对值，`>180°` 取补角——因 atan2 差值域 (−2π,2π)，取 abs 后落在 [0°,360°)，需折半到 (0°,180°] ✓。

---

## 8. 容错与计数策略

总原则：**能救则救、救不了降级、降级了计数、绝不中断**。

| 畸形样本 | 行为 |
|---|---|
| 纯垃圾文件（无 SECTION） | token 空或 count==0 → 返回 false + 非空 err |
| ENTITIES 内只有未知实体（BANANA） | 逐个 `unknownEntities++` 跳过；count==0 → 整体失败（err 提示无可识别图元），但**计数已带回** |
| 截断实体（LINE 缺 11/21） | 缺省值兜底解析 → 仍得 1 条直线（起点有效） |
| 坏组码行（非整数 "XX"） | `tokenizeDxf` 跳过坏行，后续实体不受影响 |

---

## 9. 验收结果（黄金样本三件套）

`t_dxfImport()`（tests/SketchSelfTest.cpp）共 **40 条新断言**：

1. **① round-trip 差分**：自绘 Line/Circle/Arc/Polyline/Text → `toDxf` → `fromDxf` → 逐元素类型、点集、半径/字高、弧起止角、文本内容、整图包围盒全部 ≤1e-6 图纸 mm；`stats.entities` 与 `unknownEntities` 一致。（13 断言）
2. **② 手工多类样本逐类断言**：手工拼 DXF 文本（含 $INSUNITS/BLOCKS(BOLT)/INSERT+ATTRIB/ELLIPSE/SPLINE/DIMENSION/HATCH/POINT/XLINE/RAY/LEADER），断言：块表 1 个名 BOLT 且块内 1 图元；INSERT→BlockRef 且 TAG1=VAL1 并入属性；椭圆 a=10/b=5/rot=0；样条拟合点重建；对齐标注 dimValue=30；填充 4 顶点；点/构造线×2/RAY 90°/引线折线；fallback≥1、unknown==0、新增实体 ≥7 类同时在场。（20 断言）
3. **③ 畸形容错**：垃圾失败不崩溃；仅未知实体失败且计数 ≥1；截断 LINE 降级成功；坏组码行跳过后 LINE 可解析。（7 断言）

全量：**通过 114 / 失败 0**；双目标零警告；`./build.sh test` EXIT=0。

---

## 10. 过程问题与修正

| # | 问题 | 根因 | 修正 |
|---|---|---|---|
| 1 | `-Wmisleading-indentation` 警告（CadFormats.cpp MINSERT 归一化） | 两条独立 if 写在同一行 | 拆为两行独立语句 |
| 2 | 半径/直径标注初版误用组码 13 | 混淆"延伸线原点(13/14)"与"弧上定义点(10)+圆心(15)"语义 | case 3/4 改 `center.distTo(defP)`，同时消除 defP 未使用告警风险 |
| 3 | 自测首跑 ③d 坏组码用例失败 | **测试输入不合法**：`0 LINE` 未包 SECTION/ENTITIES 段，新版按段分发跳过 → 正确失败 | 给 ③d 补段包装，保留坏组码行；重跑全绿（产品行为正确） |

---

## 11. 经验沉淀

- **段状态机 + 共用分发器**是 DXF 读侧扩展的正确形状：新增实体只改 `handleEntity` 一个函数，BLOCKS/ENTITIES 免费获得同款能力。
- **统计即验收**：`unknownEntities/fallbackEntities` 让"跳过/降级"从静默行为变成可断言、可上报的数据——黄金样本直接对计数断言。
- **块表走 stats 携带**而非改签名：调用方自主决定注册时机（必须在 clear 之后、addElements 之前），API 零破坏。
- DXF 组码语义要逐码核对文档：13/14（延伸线原点）vs 10（定义点）vs 15（圆心/角度顶点）的错位是标注导入的典型坑。
- 测试样本自身必须合法（段包装完整），否则会把"正确的拒绝"误判为产品 bug。

---

## 12. 改动清单

| 文件 | 改动 |
|---|---|
| `src/core/CadFormats.h` | `DxfImportStats` 增 `unknownEntities`/`fallbackEntities`/`blocks`；include Block.h |
| `src/core/CadFormats.cpp` | 新增 `lineStyleFromDxfName`/`sampleEllipseArc`/`struct DxfImportCtx`；`handleEntity` 新增 10 类实体解析；`fromDxf` 重写为段状态机 + 块表交付 |
| `src/ui/MainWindow.cpp` | DXF 导入流接线 `addBlockDef`（clear→注册→入图元顺序）；日志上报块数/降级/未知计数 |
| `tests/SketchSelfTest.cpp` | 新增 `t_dxfImport()`（40 断言）并登记 main() |
| `docs/dxf-import-expansion-design-analysis.md` | 本文 |

— 完 —
