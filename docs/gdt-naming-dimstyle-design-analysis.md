# SketchCAD GD&T 与一图多标准（命名标注样式 + 多重引线 + 形位公差框格）：设计与深思

> 配套任务：支撑机械 GD&T 图纸与「一图多标准」——命名标注样式表多实例化、多重引线（MLeader）、
> 形位公差框格（Fcf，GB/T 1182 符号集子集）。
> 范围：① DimStyle 表多实例化（name→Style 映射随图存档，对标 blockTable 范式）+ `dimstyle` 命令
> （新建/重命名/置当前/列表/应用/删除）+ 标注图元记样式名；② MLEADER 图元；③ 形位公差框格图元。
> 约束：`ElementType` 枚举末尾追加（MLEADER=18 / FCF=19），序列化按整数索引不重排；单全局 DimStyle
> 旧档读入自动转为默认命名样式；新增图元走「新增图元类型同步清单」八步。
> 验收：12 类既有标注回归不破坏；样式切换全图联动断言；MLEADER/FCF 序列化 round-trip；
> 自测新增 ≥12 断言（实际 **+32**，496 → 528）；零警告；`build.sh test` EXIT=0。

---

## 1. 核心洞察：又一次"引用式"范式，与块/文字样式同构

本工程已有两处成熟的**引用式**范式：块（`BlockRef` 存块名，渲染时实时展开 `BlockDef`）、
文字样式（`Text` 存 `styleName`，渲染时按 STYLE 表解析字体/字高）。标注样式此前却是
**单全局**——`SketchModel::dimStyle()` 每次现场造一张 `dim::Style`，改不了、也存不下。

本任务的核心命题因此非常清楚：

> **把标注样式也变成一张「name → Style」表，标注经 `dimStyleName` 按名引用。**
> "改样式 → 全图联动"于是成为引用式模型的**免费性质**——与块 redefining、文字样式改表
> 完全同构，无需任何反向推送或遍历刷新。

配套的第二个命题是**渲染入口的单一化**：此前渲染侧 `paintDimension` 绕开模型、直接按
`e.standard` 现场派生样式。要让样式表真正生效，样式解析必须收敛到模型层唯一入口
`dimStyleFor(e)`，由渲染/打印/导出共用。

---

## 2. DimStyle 表（一图多标准）

### 2.1 数据结构与解析

```cpp
struct DimStyleEntry { QString name; dim::Style style; };   // dim 命名空间
std::vector<dim::DimStyleEntry> dimStyles_;                  // 表
QString currentDimStyle_ = "Standard";                       // 当前样式
```

解析入口（渲染/导出/日志唯一共用）：

```
dimStyleFor(e)：e.dimStyleName → 表中条目
                ∅/未命中     → 当前样式
                仍无        → 表首条 → 内置默认
                （统一注入 drawingScale = 当前图纸比例）
```

`dimStyle()` 保留为**兼容入口**，内部即 `dimStyleFor(Element())`（当前样式 + 比例），
既有调用点（`SketchModel::syncDimensionValues` 等）零改动。

### 2.2 命令 `dimstyle`

```
dimstyle                          列表（* 标当前）
dimstyle new <名> [gbt|iso]       新建（可指定标准预设）
dimstyle set <名>                 置当前
dimstyle rename <旧> <新>         重命名（同步更新引用它的图元）
dimstyle apply <名> [图元id...]    把选中/指定标注改引用该样式
dimstyle delete <名>              删除（引用者回落当前样式）
```

表操作全部走既有范式：增删改名自带 `pushMemento`（可单独撤销），`apply` 用
`beginEdit` + `updateElementQuiet` 批量写入（一次 Ctrl+Z）。新增 `dimStylesChanged` 信号，
表变更时发 `modelChanged()` 令引用它的标注重绘。

### 2.3 随图存档 + 旧档兼容

顶层 JSON 新增 `dimStyles` 键（`{styles:[…], current:"…"}`），交互存读档（`MainWindow`）
与 COM 批量出图（`BatchPlotEngine`）两处读写对齐。`drawingScale` **不入表**——它来自
图纸比例，每次由 `dimStyleFor` 注入，避免样式与比例两处真相。

**旧档兼容（约束条款）**：`setDimStylesFromJson` 在 `styles` 缺失或为空数组时**自动生成
一条 "Standard"**（= 原 `dim::Style()` 默认值，国标）。于是"单全局 DimStyle"的旧图读入后
等价于"一张名为 Standard 的样式"，**渲染结果逐像素等价、零行为变化**。旧档中所有标注的
`dimStyleName` 缺键 → 空串 → 走当前样式 → 即 Standard。已由自测③锁定。

### 2.4 渲染侧：显式引用才覆盖（保住既有标注零回归）

这是本任务最需要克制的一处。`paintDimension` 原先按 `e.standard` + 当前图幅字高**现场派生**
样式；若直接改成"一律走 `dimStyleFor`"，则 `standard=ISO` 的既有标注会被默认的
"Standard"（国标）覆盖——**既有标注行为改变，回归被破坏**。

因此采用**双路径**：

```cpp
// 既有基线（不变）
st.stdConf   = forStandard(e.standard);
st.textHeight= standardTextHeightMm(当前图幅);
// 命名样式覆盖：**仅当元素显式引用了表中某式时**
if (model && !e.dimStyleName.isEmpty())
    if (const auto* de = model->findDimStyle(e.dimStyleName)) st = de->style;
if (e.dimTextHeight > 1e-6) st.textHeight = e.dimTextHeight;  // 单图元覆盖优先级最高
```

即：**空 `dimStyleName` 的标注保持历史行为不变**；只有显式 `dimstyle apply` 过的标注才受
样式表驱动。这既满足"一图多标准"（按需引用），又保证 12 类既有标注零回归（自测①锁定）。

---

## 3. MLEADER（多重引线）

新建 `src/core/Gdt.{h,cpp}` 作为 GD&T 几何生成器（与 `Section.cpp` 同范式：国标量是
「图纸毫米」，放核心层以便自测无头断言、导出复用）。

几何：`pts = [附着点, 折点, 文字基线起点]`（两点时退化为直线引线），
`buildMLeader` 产出：引线折线段 + 实心箭头（尖端/长/半宽）+ 水平基线（`mleaderLanding>0` 时）
+ 按 `\n` 分行的多行文字锚点。字段 `mleaderStyleName` 复用 DimStyle 表（引线标注与尺寸标注
同属标注体系，共用一张表符合 AutoCAD 语义）。

## 4. 形位公差框格（Fcf，GB/T 1182 子集）

`FcfSymbol` 枚举按 GB/T 1182 收录 **11 类**（顺序只增不改，整数入库）：
直线度 / 平面度 / 圆度 / 圆柱度 / 平行度 / 垂直度 / 倾斜度 / 位置度 / 对称度 / 圆跳动 / 全跳动。

框格按内容自左向右布局为若干格：`[符号] [修饰符] [公差值] [基准]…`，外环取面积最大者、
其余被包含者为内孔的同类策略不适用（框格是单行），故各格宽度按内容估算、锚点 `pts[0]`
为框格左下角。`appendFcfSymbolStrokes` 把每个符号画成单位方框内的基础图元组合
（圆度=圆、位置度=圆+十字、圆柱度=圆夹两平行线…），圆统一离散成线段，
使绘制/包围盒/拾取都只需处理 `Seg` 一种几何。

> **核心层不引 `QFontMetrics`**：离屏无字体环境会退化成方框，导致布局在测试与实机不一致。
> 故格宽用「字符数 × 字高 × 0.62」的确定性估算——保证布局**可测且可复现**。

---

## 5. 「新增图元类型同步清单」八步执行情况

| 步 | 内容 | 落地 |
|----|------|------|
| ① | `Element.h` 枚举末尾 + `elementTypeName` | `MLeader=18` / `Fcf=19` 追加末尾；两个名称 |
| ② | `Element.cpp` `sizeDesc` / `elementToJson` / `elementFromJson` | 各 1 处；`sizeDesc` 回退「未知」；缺键回落默认（旧档兼容）|
| ③ | struct 字段 | `mleaderStyleName/Landing/ArrowLen`、`fcfSymbol/Modifier/Tolerance/Datums`、`dimStyleName` |
| ④ | `ElementFactory` | `createMLeader` / `createFcf` |
| ⑤ | `GraphicsView` | `rawBounds`/`shape` 走 `ElementPath`；`paintOne` 分派 + `paintMLeader` / `paintFcf` |
| ⑥ | `PlotRenderer` | `paintGdt`（线框+实心箭头+文字），并在分派中优先于通用路径 |
| ⑦ | `Geometry` | `distanceToElement` + `elementBounds` 各自新增分支 |
| ⑧ | CMake | `src/core/Gdt.cpp` 加入 `CORE_SOURCES`（两目标共用）|

配套还改了 `ConstraintSolver::naturalDof`（MLeader=2×点数、Fcf=2）与
`ElementPath::buildPath`（两类型路径）——`-Wswitch` 对新增枚举值会报警，两处补齐后
双目标零警告。

---

## 6. 自测（`t_gdt`，+32 断言，496 → 528）

| 组 | 锁定的不变式 |
|----|------------|
| ① 12 类既有标注回归 | 逐类构造满足该 builder 最小输入的标注，`dim::build(...).valid` 全为真；`dimensionTypeName` 全为专名（未退化兜底）。**这是"不破坏既有标注"的直接证据** |
| ② DimStyle 表联动 | 默认含 "Standard"；新建 ISO 式；两个标注引用它 → 解析到 ISO 字高/箭头；**改样式定义 → 两处标注实时联动**（引用式全图联动）；重命名同步引用；空引用回落当前样式 |
| ③ 旧档兼容 | `setDimStylesFromJson({})` → 自动生成唯一 "Standard"；解析出的样式等价于原单全局（国标 + 字高 3.5）|
| ④ MLEADER/FCF 往返 | MLEADER：类型/附着点/多行文字/基线/箭头往返 + 几何（引线+两行文字+基线）；FCF：类型/符号/公差/修饰符/基准组/锚点往返 + 几何（5 格 + 框线 + 符号笔画）；11 类符号均有名称、越界被拒 |

> ② 中"改样式 → #1 与 #2 同时联动"两条断言是**分别**锁定的，确保联动不是偶然只对一个生效。

---

## 7. 踩坑与经验

1. **CMake 注释是 `#` 不是 `//`**：`src/core/Gdt.cpp   // GD&T：…` 里的 `&` 与全角括号
   被 CMake 当成源文件列表（`Cannot find source file: GD&T：多重引线`）。**CMakeLists 内
   一律用 `#`**。
2. **新增枚举值会触发 `-Wswitch`**：`ConstraintSolver::naturalDof` 与 `ElementPath::buildPath`
   两处 `switch (ElementType)` 立刻报警——这正是八步清单的意义所在（枚举一改就暴露所有
   需要补的分支）。补齐后零警告。
3. **`std::vector<QString>` 不是 QString**：FCF 基准组是 vector，`sizeDesc` 里误用
   `.isEmpty()/.join()` 编译失败；改为手工拼接。
4. **回归优先于新功能**：样式表若"一律生效"会改变 `standard=ISO` 既有标注的行为。故采用
   "显式引用才覆盖"的双路径，把"零回归"作为硬约束保住，同时不牺牲一图多标准能力。
5. **核心层布局不用字体度量**：`QFontMetricsF` 在离屏/无字体环境退化，会让"格宽估算"在测试
   与实机不一致；用确定性字符宽度估算换取布局可测、跨环境一致。

---

## 8. 变更清单

**新增文件**
- `src/core/Gdt.{h,cpp}` — `FcfSymbol` 符号集 + `buildMLeader` / `buildFcf` 几何生成器
- `docs/gdt-naming-dimstyle-design-analysis.md` — 本文

**修改文件**
- `src/core/Element.h` — 枚举末尾追加 MLeader/Fcf；两个 `elementTypeName`；新增 9 个字段
- `src/core/Element.cpp` — `sizeDesc` / `elementToJson` / `elementFromJson`（缺键回落默认）
- `src/core/Gdt.h|.cpp`（新）— 见上
- `src/core/DimStyle.h|.cpp` — `DimStyleEntry` + 其序列化
- `src/core/SketchModel.h|.cpp` — 样式表/当前样式/`dimStyleFor`/增删改名/序列化；Memento 纳入
  `dimStyles`+`currentDimStyle`；undo/redo 恢复；构造预置 "Standard"
- `src/core/Commands.cpp` — `dimstyle` 命令
- `src/core/Geometry.cpp` — `distanceToElement` / `elementBounds` 新增分支
- `src/core/ConstraintSolver.cpp` — `naturalDof` 补两类型
- `src/core/ElementFactory.h|.cpp` — `createMLeader` / `createFcf`
- `src/ui/ElementPath.cpp` — `buildPath` 新增两类型
- `src/ui/GraphicsView.h|.cpp` — `paintMLeader` / `paintFcf` + `paintOne` 分派；
  `paintDimension` 支持命名样式覆盖
- `src/ui/PlotRenderer.cpp` — `paintGdt` + 分派
- `src/ui/MainWindow.cpp` — `dimStyles` 存读档
- `src/com/BatchPlotEngine.cpp` — `dimStyles` 存读档
- `CMakeLists.txt` — `Gdt.cpp` 入 `CORE_SOURCES`
- `tests/SketchSelfTest.cpp` — `t_gdt()` + 注册

**验收结果**
- 自测 **528 / 0**（基线 496，+32）
- `SketchCAD` / `SketchSelfTest` 双目标 **零警告零错误**
- `build.sh test` **EXIT = 0**
