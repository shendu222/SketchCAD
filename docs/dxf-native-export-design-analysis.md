# DXF 原生导出（块/标注/填充）—— 设计与深思

> 日期：2026-10-02 ｜ 模块：`src/core/CadFormats.cpp`（写侧）+ `src/ui/MainWindow.cpp`（接线）+ `src/com/BatchPlotEngine.cpp`（COM 同步受益）+ `tests/SketchSelfTest.cpp`（黄金样本）
> 验收基线：自测 114/0 → **155/0 全绿**（`t_dxfExport` 新增 40 条断言）；`./build.sh test` EXIT=0；SketchCAD / SketchSelfTest 双目标 `-Wall -Wextra` 零警告。
> 姊妹篇：《DXF 导入实体覆盖扩展》（`dxf-import-expansion-design-analysis.md`）——读侧 6→16 类；本篇为写侧原生输出闭环。

---

## 1. 任务目标与硬约束

### 1.1 任务目标

DXF 导出从"炸开/离散"升级为**原生实体输出**，使导出文件在 AutoCAD/ZWCAD 打开后：

- **标注可编辑**（DIMENSION 实体而非 LINE+SOLID+TEXT 集合）
- **填充可编辑**（HATCH 实体且图案名/比例/角度可改）
- **块不展开**（BLOCKS 段 + INSERT 引用，属性块带 ATTRIB）

### 1.2 硬约束对照

| 约束 | 落实方式 |
|---|---|
| R12（AC1009）组码规范不破 | 新增实体严格按 R12 语法；唯一例外 HATCH 路径组码（§7）有意识取舍并文档注明 |
| 旧炸开行为仅在块无法表达时启用 | 原生模式兜底链：块定义缺失/嵌套超深/isXref → `block::expandRef` 递归展开；**opts==nullptr 时 100% 旧行为** |
| round-trip 自身文件几何差 ≤1e-6 | 自测逐点对比（§11），Spline/Ellipse 按同参数重采样逐点断言 |
| 不改变 JSON 原生存档 | 本次零 JSON 代码改动 |
| 导出默认行为向后兼容 | `DxfExportOptions` 全默认关闭（§2.1），`opts=nullptr` 时连 bounds 计算路径都与旧版字节级一致 |
| COM 批量出图通路同步受益 | `BatchPlotEngine.cpp` Dxf 分支与 GUI 侧同参接线 |

---

## 2. API 设计与向后兼容边界

### 2.1 DxfExportOptions

```cpp
struct DxfExportOptions {
    bool nativeBlocks = false;   // ① BLOCKS 段 + INSERT/ATTRIB/ATTDEF
    bool nativeDims   = false;   // ② DIMENSION 原生（12 类映射）
    bool nativeHatch  = false;   // ③ HATCH 原生（边界路径）
    int  splineSegments  = 16;   // ④ 样条离散段数（POLYLINE 拟合精度）
    int  ellipseSegments = 96;   //    椭圆采样段数（闭合折线）
};
```

toDxf 签名扩展（尾部追加，不破坏既有调用点）：

```cpp
QByteArray toDxf(const std::vector<Element>& elems, QString* err = nullptr,
                 double drawingScale = 1.0, int insunits = 4,
                 const std::vector<BlockDef>* blocks = nullptr,
                 const DxfExportOptions* opts = nullptr);
```

**兼容铁律：`opts == nullptr` ⇒ 旧行为**。不仅是"开关关闭"，连 `$EXTMIN/$EXTMAX` 的计算都保持旧逻辑（直接对 elems，不做展开副本）——预炸开调用方输出字节级一致。

### 2.2 增强边界划分（关键决策）

增强项分两级，边界必须清晰：

| 级别 | 内容 | 理由 |
|---|---|---|
| **无条件增强**（对旧调用方也生效） | ① LTYPE 表 1→9 条；② 新增 STYLE/DIMSTYLE 表；③ 实体组码 6（非 Solid 线型）；④ POINT 实体补写 | 纯保真提升：文件结构合法（表数量增加是 R12 允许的）、不改实体语义、旧调用方零感知风险 |
| **开关增强**（仅 opts 开启时） | INSERT/DIMENSION/HATCH 实体分支 | 改变实体构成（一个元素 → 一个实体 vs 一组离散线），必须由调用方显式选择 |

---

## 3. 总体架构：两遍组装

### 3.1 为什么需要两遍

DXF 文件结构为 HEADER → TABLES → BLOCKS → ENTITIES → EOF。而标注的原生输出依赖 **匿名块（*D…）**：DIMENSION 实体只存几何锚点与文字位置，箭头/尺寸线/文字本体在 `*D` 匿名块中，由 DIMBLK 机制引用。但 *D 块必须出现在 BLOCKS 段——**在实体写出之前**。

方案对比：

| 方案 | 问题 |
|---|---|
| 预扫描所有标注单独再算一遍几何 | 标注几何计算逻辑两份，必然漂移 |
| **两遍组装（选定）** | 第一遍写 ENTITIES（LEnt）顺带收集 `anonDims`（`vector<pair<QString, QStringList>>`，*D 块名→块体 token 流）；第二遍拼装 HEADER/TABLES/BLOCKS(用户块+*D 匿名块)/ENTITIES/EOF |

### 3.2 递归写出器 writeEntity

```cpp
std::function<void(const Element&, QStringList&, int depth)> writeEntity
```

- 旧有全部分支（Line→Text…）原样保留，仅将 token 目标从固定 QStringList 参数化为 `QStringList& T`（块体内复用同一 lambda 集）；
- `depth` 用于块内判定：depth>0 时标注一律炸开（DXF 规范：块内 DIMENSION 语义未定义，AutoCAD 自己也不写）；
- depth<8 防循环引用（嵌套块）；
- 新增分支：Dimension（原生判定）、Point、Hatch（原生判定）、BlockRef（INSERT/兜底/跳过）。

`arrowSolid(T, e, Arrow)` lambda 与旧炸开路径共用 `dimSt.arrowLen/arrowWidth`——箭头几何单点维护。

---

## 4. 块原生导出

### 4.1 写出链

```
BlockRef ─┬─ 定义存在且 depth<8 且 非 isXref 且 名不带'*' → INSERT{2 块名/66/10/20/30/41/42/43/50}
          │    └─ 有属性 → 66=1 + 逐条 ATTRIB{2 tag/1 value/10/20/30/40=3.5/70=0} + SEQEND
          ├─ 定义缺失/嵌套超深 → block::expandRef 递归展开（兜底，永不静默丢图）
          └─ 无表（blocks==nullptr）→ 静默跳过（旧 parity）
```

### 4.2 块定义写出

BLOCKS 段逐块：

```
BLOCK{2 名/70=0/10,20,30=base 点} + ATTDEF（有属性块）+ 实体（writeEntity depth=1）+ ENDBLK
```

- `isXref` 块与 `'*'` 前缀块（含本产品匿名块）跳过；
- ATTDEF 组码：`1=默认值 / 2=tag / 3=提示 / 70=invisible|constant<<1`；
- 属性值链路：BlockDef 的 `BlockAttrValue{tag,value}` → INSERT 后 ATTRIB 逐条落值。

### 4.3 设计要点

- **兜底优先于跳过**：定义缺失不是错误——expandRef 展开后输出离散几何，与旧行为一致，保证"永不比旧版差"；
- **depth 传递**：块内再引用块 → 递归 depth+1，>8 展开成几何（恶意环深度保护）。

---

## 5. 标注原生导出：12 类映射与 *D 匿名块

### 5.1 dxfDimKind 映射表（70 组码）

| DimensionType | DXF kind=70 | 关键几何组码 | 说明 |
|---|---|---|---|
| Horizontal | 0 | 13/23,14/24=pts[0],pts[1]；**50=0** | 水平投影 |
| Vertical | 0 | 同上；**50=90** | 垂直投影 |
| Distance | 0 | 同上；50=\|Δx\|≥\|Δy\| ? 0 : 90 | R12 无 70=1 的对齐语义时以投影角表达 |
| Aligned | 1 | 13/23,14/24=pts[0],pts[1] | 真实两点距离 |
| Baseline | 1 | 同 Aligned | 基线系列共享第一条延伸线 |
| Continue | 1 | 同 Aligned | 连续系列 |
| Angle | 5 | 15/25=pts[0] 顶点、13/23=pts[1]、14/24=pts[2] | 角度两腿 |
| Diameter | 3 | 15/25=pts[0] 圆心、10/20=pts[1] 弧上点 | 70=3 |
| Radius | 4 | 同 Diameter | 70=4 |
| Arc / Leader / Ordinate | **-1（保留炸开）** | — | R12 下不可原生表达或重建歧义大 |

- **10/20（DIMLINE 定义点）**：Distance/Aligned 类 = `g.line[0]` 中点；Angle = arcPoly 中点；Radius/Diameter = pts[1]；
- **11/21 = g.textPos**（文字位置，可编辑性关键）；
- **组码 1（替代文字）仅当 `e.text` 非空**；组码 3 = "STANDARD"；
- 标注箭头/尺寸线/文字本体 → *D 匿名块（§5.2）。

### 5.2 *D 匿名块坐标方案（深思核心）

**问题**：DIMENSION 实体引用 *D 块时，块内几何的坐标系必须与 INSERT 显示语义自洽。三种组合在两种显示模型下的还原分析：

| 方案 | 块内坐标 | BLOCK base(10/20) | AutoCAD 语义（相对定义点偏移） | 普通 INSERT at defP（平移 defP−base） |
|---|---|---|---|---|
| **A（选定）** | 世界 − defP | 0,0,0 | ✅ 还原世界 | ✅ 还原世界 |
| B | 世界（绝对） | defP | ✅ | ✅ |
| C | 世界（绝对） | 0,0,0 | ❌ 偏移 defP | ❌ 双重偏移 |

方案 B 也可行但依赖 base 语义精确传递，方案 A 的"块内相对 + base 归零"在两种显示模型下都鲁棒（消费端无论按 DIMENSION 专用语义还是普通 INSERT 语义解释都正确），且与 AutoCAD 导出文件实测结构一致——**选定 A**。

块体生成（`rel()` = 世界 − defP）：

| 源 | 块内实体 |
|---|---|
| g.ext | LINE |
| g.line | LINE |
| g.arcPoly | 代理 Element(type=Polyline) 走 polylineEx |
| g.arrows | arrowSolid（tip 平移） |
| label | textLines(rel(textPos)) |

### 5.3 块内标注策略

depth>0（块内）标注一律炸开：AutoCAD 自身也不在块内写 DIMENSION，硬写会产生不可编辑的嵌套标注，炸开反而保真。

---

## 6. 填充原生导出（HATCH）

### 6.1 组码序列

```
HATCH{2="ANSI31"/70=0/71=0/10,20,30=0/78=1/53=e.rotation/43,44=0/
      45=0/46=e.radius(>0.05 否则 2.5)/79=0/
      91=1+holes.size() ← 路径数
      ├─ 外环：92=3(external|polyline)/73=1/93=顶点数/每顶点{72=0/10/20}
      └─ 孔环：92=2(default|polyline)/73=1/93=顶点数/每顶点{72=0/10/20}
      75=0（关联样式）/76=1}
```

- 图案名固定 ANSI31（本产品 Hatch 图元仅存预定义图案参数）；
- **46 = scale**：`e.radius > 0.05 ? e.radius : 2.5`——旧档 radius 缺省时给视觉合理默认；
- **53 = 角度**。

### 6.2 R12 规范缺口（有意识的取舍）

R12（AC1009）规范中 **HATCH 没有边界路径组码（91/92/93/72/73 路径描述为 R13+ 引入）**。业界事实标准：

- ZWCAD、浩辰、GstarCAD 导出 R12 时同样写 91/92 路径组码；
- AutoCAD R14+ 读 R12 时对 HATCH 容错接受。

**决策**：按事实标准写路径组码，文档注明此为规范外写法。备选方案（降级写 POLYLINE 边界 + 独立 HATCH 壳）会导致 round-trip 丢填充语义，违背任务目标。round-trip 由导入器路径解析（§9）闭环保证。

---

## 7. LTYPE 表：9 线型 dash 定义

### 7.1 定义表

| 名称 | 描述 | dash 序列（49 组码，正=划 长，负=空 长，0=点） |
|---|---|---|
| CONTINUOUS | Solid line | （空） |
| DASHED | `__ __ __` | 12, -3 |
| DOTTED | `. . . .` | 0, -3 |
| CENTER | `__ . __ .` | 30, -4, 0, -4 |
| PHANTOM | `__ . . .` | 50, -5, 0, -5, 0, -5 |
| DASHEDWAVE | Wave (dash approx) | 3, -2 |
| DASHDOTDBRK | Double break (approx) | 30, -4, 0, -4 |
| DASHEDTHICK | Thick dashed | 12, -3 |
| DASHDOTTHICK | Thick dash-dot | 30, -4, 0, -4 |

`dxfWriteLtype` 统一写：`70=64 / 72=65 / 73=划数 / 40=总长 / 49×n`。

### 7.2 反向兼容（有损映射）

导入端 `lineStyleFromDxfName` 按 startsWith 前缀规则映射回本产品 9 种 LineStyle：

- DASHEDWAVE → Dashed（波形以短划近似，有损但可接受——R12 无法表达真实波形）
- DASHDOTDBRK / DASHDOTTHICK → DashDot
- 厚度语义（THICK 后缀）在 DXF 中由图层/颜色承载，本产品由 LineStyle 表达——**导入时厚度信息无法区分，接受有损**。

---

## 8. DIMSTYLE / STYLE 表（官方核对结果）

### 8.1 DIMSTYLE（最小实现）

**组码顺序经 WebSearch 对照 Paul Bourke DXF R12 与 Autodesk 官方文档逐一核对**（过程中纠正了前期笔记的两处颠倒，见 §10.3）：

| 组码 | 变量 | 值 | 语义 |
|---|---|---|---|
| 40 | DIMSCALE | 1 | 全局比例 |
| 41 | DIMASZ | 3.5 | 箭头尺寸 |
| 42 | DIMEXO | 0 | 延伸线起点偏移 |
| **43** | **DIMDLI** | **10** | **基线间距（注意：不是 DIMEXE！）** |
| **44** | **DIMEXE** | **2.5** | **延伸线超出量** |
| 140 | DIMTXT | 3.5 | 文字高度 |
| 141 | DIMCEN | 0 | 圆心标记 |
| 142 | DIMTSZ | 0 | 斜箭头（0=用箭头） |
| 144 | DIMLFAC | 1/drawingScale | 线性测量比例因子 |
| 147 | DIMGAP | 0.8 | 文字间隙 |
| **77** | **DIMTAD** | **1** | **文字位于尺寸线上方** |
| 78 | DIMZIN | 0 | 零抑制 |

### 8.2 STYLE（文字样式，LTYPE 表同级的必需表）

```
STANDARD：40=0（固定字高）/ 41=1（宽度因子）/ 50=0（倾角）/ 71=0 / 42=2.5 / 3="txt"（字体文件）
```

缺 STYLE 表时部分 CAD 打开会报缺字体样式——这是"打开不报错"验收的一部分。

---

## 9. 导入器四适配（读侧联动）

原生导出的文件要能被**自身导入器**正确读回（round-trip 验收），四项适配：

1. **ATTDEF 静默 return**：块定义中的 ATTDEF 不是图元，解析后不计入 unknown，也不产生图元；
2. **HATCH 路径解析容错**：读 92 后跳 73；顶点循环改为逐组码容错（72/42/91 逐个跳过、非 10 组码则 break）——旧样本无这些组码不受影响（读侧向后兼容铁律）；
3. **收尾 `'*'` 前缀匿名块过滤**：*D 匿名块被 §3 读侧解析后已在 DIMENSION 重建中消费，残余跳过；
4. **SOLID → 闭合 Polyline（新增通用能力）**：*D 块内箭头是 SOLID 实体，导入器原本无分支 → unknown。顶点语义：1→2→4→3（Z 字序 → 环绕序），输出闭合 Polyline，名"导入:实心面"。

> SOLID 顶点序坑：DXF SOLID 四点为 1(左下)→2(右下)→3(左上)→4(右上) 的 Z 字排布，直接 1→2→3→4 连线会自交；正确环绕序为 q1,q2,q4,q3。

---

## 10. 过程修正记录（深思留下的实证教训）

### 10.1 Radial 标注静默失效

**现象**：首轮自测半径/直径标注 round-trip 值 = -1（DIMENSION 缺失）。
**根因**：`DimStyle.cpp:371` `buildRadial` 要求 `e.radius > 1e-9`，测试构造漏设 radius → Geom invalid → 原生分支不触发 → 炸开分支也不产 DIMENSION。
**修复**：测试元素补 `d.radius = 10.0;`（Radius 与 Diameter 均补）。
**教训**：DimElement 的 radius 字段是径向标注的隐式必需项，构造侧极易漏。

### 10.2 SOLID unknown=4

**现象**：round-trip unknown=4。
**根因**：4 个标注箭头 SOLID 落入导入器未知分支（§9.4）。
**修复**：新增 SOLID→Polyline 分支，顺带获得通用 SOLID 导入能力（非仅服务标注）。

### 10.3 DIMSTYLE 43/44 组码颠倒

**现象**：前期笔记写 "43=DIMEXE、44=DIMDLI"。
**根因**：记忆混淆。
**修复**：WebSearch 对照 Paul Bourke DXF R12 参考与 Autodesk 官方文档核对——**43=DIMDLI、44=DIMEXE**。落码按官方顺序。
**教训**：组码语义必须官方核对，不信任中间笔记。

### 10.4 首轮断言 FAIL 的诊断方法升级

6 条 FAIL 但首轮 tail -3 误判为 2 条。修正方法：断言消息动态化（`QStringLiteral("...%1...").arg(x).toUtf8().constData()`），一次运行定位全部根因，避免"修一轮跑一轮"的慢循环。

---

## 11. 验收数据

### 11.1 自测：t_dxfExport 40 断言（114 → 155/0）

| 组 | 断言数 | 内容 |
|---|---|---|
| token 计数 | 12 | INSERT=1 / ATTRIB=1 / ATTDEF=1 / DIMENSION=4 / HATCH=1 / POINT=1 / BLOCK=5 / LTYPE=9 / DIMSTYLE=1 / SOLID>0 / 无错误 / 非空 |
| round-trip | 21 | fromDxf 成功、unknown=0、fallback=0、blocks==1（名 BRK）、entities==1、BlockRef 位置+TAG1=VAL1、nDim=4、四类标注值 30/90/10/20（±1e-6）、Hatch 4 顶点+1 孔 3 顶点、Point 位置、Spline/Ellipse 逐点 ≤1e-6（对比 sampleSpline(pts,16) 与 sampleEllipse(…,96)） |
| legacy + 精度 | 7 | 无 opts → INSERT/DIMENSION/HATCH==0 且 LINE>0（旧行为）；splineSegments=32 → VERTEX>8 |

测试元素：Line / BlockRef(BRK+TAG1=VAL1) / Aligned 30 / Angle 90(V(0,40), 脚(10,40),(0,50)) / Radius 10(r=10) / Diameter 20(r=10) / Hatch 4 顶点+1 孔 3 顶点 / Point(5,5) / Spline 3 点 / Ellipse(260,10,a=30,b=15,rot=20)。

计数方法：`countToken` 按 `"0\r\n<类型>"` 相邻行计数——规避 TEXT 子串误命中（如 MTEXT）。

### 11.2 全量验收

- 全量自测 **155/0** 全绿；
- `./build.sh test` **EXIT=0**；
- SketchCAD / SketchSelfTest 双目标 `-Wall -Wextra` **零警告**（grep 0 命中）；
- round-trip 逐点 ≤1e-6 实证（Spline/Ellipse 按同参数重采样比对）。

---

## 12. 改动清单

| 文件 | 改动 |
|---|---|
| `src/core/CadFormats.h` | 格式注释更新；新增 `DxfExportOptions`（含默认值=旧行为注释、HATCH 规范注记）；toDxf 尾部追加 blocks/opts 参数 |
| `src/core/CadFormats.cpp` | `#include <functional>`；助手 dxfLtypeName/dxfWriteLtype/dxfDimKind；toDxf 两遍组装重构；writeEntity 递归（含 Dimension/Point/Hatch/BlockRef 新分支、*D 块体生成）；TABLES 段 9 LTYPE+STYLE+DIMSTYLE；BLOCKS 段用户块+*D；fromDxf 四适配（ATTDEF/HATCH 容错/匿名块过滤/SOLID） |
| `src/ui/MainWindow.cpp` | saveFileAs Dxf case、exportDxf：传 model->elements() + blockTable + native opts（DWG/STEP 保持预炸开） |
| `src/com/BatchPlotEngine.cpp` | Dxf case 与 GUI 同参（COM 批量通路同步受益） |
| `tests/SketchSelfTest.cpp` | 新增 `t_dxfExport()` 40 断言 + main 登记 |

---

## 13. 后续可演进项

- **LTYPE 双向精确映射**：DASHEDWAVE 波形、THICK 厚度语义当前有损，可在导入端引入宽度覆盖补偿；
- **DIMSTYLE 命名化**：当前单一 STANDARD，可按 dimSt 参数生成命名样式；
- **HATCH 图案族**：ANSI31 固定 → 按本产品图案参数映射 PAT 库；
- **Arc/Leader/Ordinate 原生化**：R12 下 Arc 标注可尝试 70=2（直径前缀约束）或 leader 化重建；
- **AutoCAD 人工核验**：黄金样本导出后在 AutoCAD/ZWCAD 实机打开截图归档（本任务以 round-trip + 计数验收替代）。
