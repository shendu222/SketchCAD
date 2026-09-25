# 附加 / 分离外部参照（Xref）设计与实现分析

> 参照标准：Autodesk 官方文档 *Attach and Detach Xrefs*
> （<https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-A987D2FF-45BD-474E-99C1-E6316A42F667>）
> 工程：SketchCAD（C++17 / Qt 6.10.1 mingw_64 / MinGW 13.1 / CMake + Ninja）

---

## 1. 需求界定

在既有代码基础上补充两项操作，且不改动其它既有功能：

| 编号 | 需求 | 本文对应实现 |
|---|---|---|
| R1 | 附加：指定参照图形的**完整文件路径** | `SketchModel::attachXref(filePath, …)` |
| R2 | 附加：指定**路径类型**（完整路径 / 相对路径） | `XrefPathType` + `xref::storedPath/resolvedPath` |
| R3 | 附加：插入点、缩放、旋转**沿用默认值** | 默认参数 `(0,0)` / `1.0` / `0°` |
| R4 | 附加：处理**文件不存在**与**已被附加** | 拒绝并写 warn 日志，返回 `-1` |
| R5 | 分离：按**参照块名**移除 Xref 定义 + **全部**参照实例 | `SketchModel::detachXref(name)` |
| R6 | 分离：处理**块名不存在**与**存在嵌套参照** | 显式拒绝；嵌套随宿主级联清理 |
| R7 | 实现方式、命名、错误处理风格与既有代码一致 | 沿用「模型判别 + `log(..., "warn")` + `false`/`-1`」三段式 |

---

## 2. 理论基础

### 2.1 Xref 在 AutoCAD 中的本质是一类块表记录

外部参照不是"另一份图形"。
被附加的图形在宿主图中登记为**一条块表记录（Block Table Record）**，符号名默认取文件名主体，
此后宿主图中的每次出现都是一个插入实例。由此得到两条推论，也是本实现的公理：

1. **几何不复制**：改一份定义，所有实例同步 → 本工程已有的「引用式」块模型天然满足，
   渲染 / 命中 / 导出全部走 `block::expandRef` 实时展开，**不必为 Xref 另写一套图形管线**；
2. **分离 = 删定义 + 删全部实例**：只删定义会留下"未定义块"的悬空实例（这正是既有
   `removeBlock()` 的语义），必须显式清除全部实例才能满足 R5。

### 2.2 为什么 Xref 字段可以安全地长在 `BlockDef` 上

既有 `BlockDef` 已保存在 `SketchModel::blocks_` 并随图存盘；把 Xref 属性做成 `BlockDef` 的附加字段，
可以零成本复用既有的"引用—展开—序列化—撤销"四条链路。为了让**旧存档格式一字不改**：

* 序列化仅在 `isXref == true` 时写出 `"xref"` 子对象；
* 反序列化在键缺失时全部回落默认（`isXref = false`）→ 与普通块完全等价。

这与 `Element::sectionExempt / sectionMaterial`（普通图元默认 0）的处理手法一致。

### 2.3 避免头文件循环依赖

`Xref.h` 需要 `BlockDef`（声明依赖块），而 `BlockDef` 需要路径类型的枚举——二者互相包含。
故在 `BlockDef` 中**只存整数**（`int xrefPathType`），注释标明对应的枚举，与既有的
`int sectionExempt` 同手法：

```cpp
// Block.h（数据控制在顶层命名空间，仅存整数）
bool    isXref       = false;   // 是否为外部参照记录
QString xrefPath;               // 源文件路径（按 xrefPathType 的形态存储）
int     xrefPathType = 0;       // 0 == XrefPathType::FullPath（见 Xref.h）
bool    xrefNested   = false;   // 嵌套参照（随宿主参照带入），不可在本图直接分离
int     xrefStatus   = 0;       // 0 == XrefStatus::Loaded（见 Xref.h）
```

命名空间约定沿用 Block 子系统：**纯数据**在 `SketchCAD` 顶层，**纯算法**在 `SketchCAD::xref`。

---

## 3. 算法原理

### 3.1 数据流

```
文件路径 + 路径类型
      │
      ├─ 存在性校验（QFileInfo::exists/isFile）        → 失败即 -1 + warn
      ├─ 符号名 = 文件名主体（xref::blockNameForPath）  → 同名已存在即 -1 + warn
      ├─ xref::readSource()  按扩展名分派
      │       JSON（本程序存档） / DXF / DWG；STEP 只写不读 → 拒绝
      ├─ 依赖引入（不动点迭代，见 3.3）                → 标 isXref + xrefNested
      ├─ 建立 Xref 块记录（几何 = 源图世界坐标，基点 (0,0)）
      └─ block::makeRef() 放置一个参照实例（默认插入参数）
```

### 3.2 路径换算（`storedPath` / `resolvedPath`）

记宿主图所在目录为 $D$（= `drawingPath()` 的所在目录），源文件绝对路径为 $P$：

| 类型 | storedPath（写盘形态） | resolvedPath（还原） |
|---|---|---|
| `FullPath` | $P$ | 原样返回 |
| `RelativePath` | $\mathrm{rel}(D \to P)$；$D$ 为空时退化为文件名 | `QFileInfo(QDir(D), stored).absoluteFilePath()` |
| `NoPath` | $\mathrm{base}(P)$ | 按 $D$ 拼接（$D$ 为空则返回原串） |

**为什么 $D$ 为空时退化为文件名而不是抛错**：未存盘的宿主图没有解析基准，
此时"相对路径"无法兑现；给出文件名（等价于搜索本图目录）比留下一条注定解不开的路径更诚实，
对话框也同步把该项文案改为「相对路径（本图未存盘 → 退化为文件名）」。

为此在模型层新增了宿主图路径登记（附加 members 一处、读写档各调用一处）：

```cpp
QString drawingPath() const { return drawingPath_; }
void    setDrawingPath(const QString& p) { drawingPath_ = p; }
QString drawingDir() const;   // drawingPath 所在目录；空 → 返回空串
```

`MainWindow::writeDrawing / readDrawing` 成功后登记，`applyNewDrawing` 时清空。

### 3.3 依赖引入的不动点迭代

源图顶层的 `BlockRef` 可能引用它自己的块定义。本工程读取健康的所有权取决于"定义是否存在"，
因此附加时必须把被引用的依赖一起带入宿主图。

令 $L$ 为当前尚缺定义的依赖名队列，$C$ 为宿主块表：

$$
\text{循环：} \quad n \leftarrow L.\text{pop};\quad
\begin{cases}
n \in C, & 跳过 \\[2pt]
n \notin \text{源图表}, & 跳过（未定义块：展开为空，同本地未定义块语义）\\[2pt]
否则 & C \leftarrow C \cup \{n\ \text{并标记 } nested\},\ \ L \leftarrow L \cup \text{deps}(n)
\end{cases}
$$

因为每条定义**至多入表一次**，即使依赖成环（A→B→A）该过程也必定收敛；外层的 `guard ≤ 1024`
只是保险，正常规模下 1~3 轮即稳定。

> **为什么不整体 `push_back(src.blocks)`**：源图里有些块定义从未被任何实例引用，
> 全量搬入会污染宿主图的块表，且让"分离时的清理范围"变得不可判定。

### 3.4 分离与级联清理的不动点迭代

设待分离 Xref 为 $H$。"谁是该随 $H$ 一起消失的嵌套参照"**不额外存储**，而是在当前表上现算——
这样即使依赖成链也能自然收敛（每轮清掉外层，下一轮里层才变为"无人引用"）：

$$
\text{循环：} \quad \text{dead} = \{\, b \mid b.\text{xrefNested} \wedge \neg\,\text{referencedAnywhere}(b)\,\}
$$

其中 `referencedAnywhere` 同时扫描**顶图图元**与**其余定义的 entities**。
逐轮删除直到 `dead` 为空（上限 64 轮）。

一条不可违背的实现约束必须写在前面：

> ⚠️ **定义内部的 `entities` 不得在附加后被局部增删改。**
> `DynParam::affected` 与 `VisibilityState::hiddenEntities` 是按**索引**引用它们的，
> 摘掉一条就会整体错位（从安全 Bug 变成"形状莫名改变"）。
> 因此所有清理都只以**整份 `BlockDef`** 为单位。

这条约束直接决定了 3.4 的形态：级联清理从不进到某个定义内部去删 `BlockRef`，
失去引用者的整份定义直接抹除；即便某处定义内部仍挂着对该 Xref 的引用，
也让它退化为"未定义块（不绘制）"，与既有 `removeBlock()` 的语义保持一致，并在日志中如实报告条数。

### 3.5 失败 / 边界判定矩阵

| 场景 | attachXref | detachXref |
|---|---|---|
| 路径为空 / 块名为空 | `-1` + warn | `false` + warn |
| 文件不存在或不是文件 | `-1` + warn | — |
| 参照已附加（同名 Xref） | `-1` + warn「请先分离」 | — |
| 同名已被普通块占用 | `-1` + warn | — |
| 格式不支持 / STEP 只写 | `-1` + warn（带转换器原错误串） | — |
| 块名不存在 | — | `false` + warn |
| 目标不是外部参照 | — | `false` + warn「请用块管理器删除」 |
| 目标是嵌套参照 | — | `false` + warn「只能在宿主图中分离」 |

全部拖 āi 不下 fats：失败一律**不留半成品**（不创建定义、不放置实例、不压撤销快照）。

### 3.6 撤销

`pushMemento` 的快照已含 `blocks`（`Memento::blocks`），因此附加 / 分离整体上只压一份快照，
一次 `Ctrl+Z` 即可让**定义 + 全部实例**共同回退。

---

## 4. 实施步骤

| 文件 | 改动性质 | 说明 |
|---|---|---|
| `src/core/Xref.h` | 新增 | 枚举（路径类型 / 状态）、`XrefSource`、`xref::*` 算法声明 |
| `src/core/Xref.cpp` | 新增 | 源文件读取、路径换算、依赖收集 |
| `src/core/Block.h` | 追加字段 | `BlockDef` 的 5 个 Xref 字段（默认全部回落普通块） |
| `src/core/Block.cpp` | 追加分支 | `blockDefToJson/FromJson` 的 `"xref"` 读写（条件写入，旧档格式不变） |
| `src/core/SketchModel.h/.cpp` | 追加 API | `attachXref / detachXref / xrefNames / xrefCount / hasXrefSource / xrefResolvedPath`、`drawingPath()` |
| `src/core/Commands.h/.cpp` | 追加命令 | `XrefCommand`（`xref attach/detach/list/path`）+ 注册 |
| `src/ui/BlockDialogs.h/.cpp` | 追加类 | `AttachXrefDialog` |
| `src/ui/MainWindow.h/.cpp` | 追加入口 | 菜单两项 + 两个 handler + 读写/新建时登记 `drawingPath` |
| `CMakeLists.txt` | 追加 | `src/core/Xref.cpp` 进 `CORE_SOURCES`；新增 `XrefTest` / `XrefDialogTest` |
| `tests/xref_test.cpp` | 新增 | 核心层回归 |
| `tests/xref_dialog_test.cpp` | 新增 | UI 层回归 |

### 4.1 附加（核心片段）

```cpp
const QString bname = xref::blockNameForPath(abs);        // 符号名 = 文件名主体
if (hasBlock(bname)) { log(...); return -1; }             // 已附加 / 被普通块占用
xref::readSource(abs, src, &err);                         // 读源图（JSON/DXF/DWG）
const QString stored = xref::storedPath(abs, pathType, drawingDir());
pushMemento(QStringLiteral("附加外部参照 ") + bname);
/* 依赖引入（不动点迭代，见 §3.3） */
blocks_.push_back(def);                                   // 一条 Xref 块记录
const Element ref = block::makeRef(blocks_.back(), insertPt, rotationDeg, sx, sy);
auto ids = addElementsQuiet({ref});                       // 一个参照实例（默认参数）
```

定义保留源图的**世界坐标**、基点取 $(0,0)$；于是插入到默认点 $(0,0)$、比例 $1$、旋转 $0°$
时展开结果与原图几何完全重合——这正是 R3 想要的"附加即可见、位置不漂"。

### 4.2 命令行

```
xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy]
xref detach <块名>
xref list                   列出本图直接附加的外部参照（路径 + 类型 + 状态 + 图元数）
xref path  <块名>           打印按路径类型还原后的绝对路径（诊断用）
```

### 4.3 菜单

【绘图 > 块(&B)】与【编辑 > 修改 > 块(&B)】共用 `buildBlockMenu`，新增两项（带矢量图标）：

```
附加外部参照...   → AttachXrefDialog（浏览选文件 + 路径类型下拉框）
分离外部参照...   → 从已附加列表挑选 → 二次确认 → model_->detachXref()
```

---

## 5. 关键假设与推导

| 编号 | 假设 | 依据 / 影响 |
|---|---|---|
| A1 | Xref 记录 = 块表记录 + 附加属性 | AutoCAD 语义；可零成本复用引用式展开 |
| A2 | 符号名取文件名主体 | 与 AutoCAD 默认一致；使"已附加"的判定退化为块名查重 |
| A3 | 几何在附加时读入定义（快照式加载） | 本机无 DemandLoad；离线可看图，且不需要后台线程与文件监听 |
| A4 | 相对路径以**宿主图所在目录**为基准 | DWG/AuCAD 的 "Relative path" 定义；故必须登记宿主图路径 |
| A5 | 随参照带入的依赖视为外部内容（`isXref && xrefNested`） | 嵌套内容不可在本图编辑/分离——这正是 R6 要的语义 |
| A6 | 依赖若在源图中自身就是 Xref，其相对路径**清空** | 该路径相对的是源图而非宿主图，在本图不可解；几何已随定义带入，不影响绘制 |
| A7 | 定义内的 entities 不可局部增删 | DynParam/VisibilityState 按索引引用，删一条会整体错位 |
| A8 | 失败不留半成品 | 所有失败分支都在 `pushMemento` 之前返回 |
| A9 | 整个操作只压一份撤销快照 | 与 `createBlockFromSelection` / `explodeBlockRef` 既有做法一致 |

推导要点：**A2 + A5** 共同保证了"哪些名字能被分离"是可判定的——
`detachXref` 只需三层判别（存在 → 是 Xref → 非嵌套），无需额外的宿主登记表。

---

## 6. 验证与结论

### 6.1 新增回归

| 目标 | 断言 | 结果 |
|---|---|---|
| `XrefTest`（核心层，8 组） | 68 | 全部通过 |
| `XrefDialogTest`（UI 层，4 组） | 18 | 全部通过 |

`XrefTest` 覆盖：默认插入参数下展开与源图一致、三种路径类型的写读、四类拒绝分支、
DXF 作源 / STEP 拒绝、依赖引入与级联清理、分离"定义 + 全部实例"、一次撤销全量回滚、
序列化往返与旧档格式不变。

`XrefDialogTest` 覆盖：对话框默认项、确认后模型真的被写入（定义 + 实例）、相对路径分支、
【块】子菜单两项新入口存在且带图标。

### 6.2 既有回归（无改动）

`SketchSelfTest`、`BlockTest`、`ModifyMenuTest`、`DrawingMenuTest`、`FileRoundTripTest`
全部通过；主程序 `SketchCAD` 编译无错误、无新增警告。

### 6.3 结论

1. 附加 / 分离已按 AutoCAD 语义落地到"块表记录 + 参照实例"这一既有模型上，
   渲染 / 命中 / 导出 / 存盘 / 撤销五条链路**一行未改**即自动可用；
2. 路径类型具备完整的"写—读—还原"闭环；相对路径基准来自宿主图，未存盘时显式退化为文件名而不假装有基准；
3. 嵌套参照的处理是本次最容易做错的部分：引入时标记为外部内容（不可直接分离）、
   清理时以整份定义为单位做不动点迭代（不破坏索引语义）；
4. 所有失败路径统一为「模型判别 + warn 日志 + `-1`/`false`」，UI 与命令行不再重复判定原因；
5. 旧存档格式一字未改（`isXref == false` 时不写 `"xref"` 键），前向读取完全兼容。

---

## 附录 A：API 速查

```cpp
// 模型层（SketchModel）
int  attachXref(const QString& filePath,
                XrefPathType pathType = XrefPathType::FullPath,
                const Vec3& insertPt  = Vec3(0,0,0),
                double rotationDeg = 0.0, double sx = 1.0, double sy = 1.0);  // 成功返回实例 id
bool detachXref(const QString& blockName);
QStringList xrefNames(bool includeNested = false) const;
int  xrefCount(bool includeNested = false) const;
bool hasXrefSource(const QString& filePath) const;
QString xrefResolvedPath(const QString& blockName) const;
QString drawingPath() const; void setDrawingPath(const QString&);
QString drawingDir() const;

// 纯算法（SketchCAD::xref）
bool readSource(const QString& absPath, XrefSource& out, QString* err = nullptr);
QString blockNameForPath(const QString& path);
QString storedPath(const QString& absPath, XrefPathType t, const QString& hostDir);
QString resolvedPath(const QString& stored, XrefPathType t, const QString& hostDir);
std::vector<QString> missingDependencies(const std::vector<Element>&, const std::vector<BlockDef>&);
```

## 附录 B：文件清单

```
src/core/Xref.h                 新增  枚举 + 算法声明
src/core/Xref.cpp               新增  源文件读取 / 路径换算 / 依赖收集
src/core/Block.h                追加  BlockDef 的 Xref 字段
src/core/Block.cpp              追加  blockDefToJson/FromJson 的 "xref" 读写
src/core/SketchModel.h          追加  Xref API + drawingPath
src/core/SketchModel.cpp        追加  attachXref / detachXref / 四个查询
src/core/Commands.h/.cpp        追加  XrefCommand + registerExact("xref")
src/ui/BlockDialogs.h/.cpp      追加  AttachXrefDialog
src/ui/MainWindow.h/.cpp        追加  菜单两项 + handler + drawingPath 登记
CMakeLists.txt                  追加  Xref.cpp、XrefTest、XrefDialogTest
tests/xref_test.cpp             新增  核心层回归
tests/xref_dialog_test.cpp      新增  UI 层回归
docs/xref-attach-detach-design-analysis.md  本文
```

## 附录 C：已知限制

* 未实现「重载参照」（源文件变更后在本图刷新几何）与「绑定 / 拆离（Bind / Unload）」，
  本次范围限定为附加与分离两项操作；
* Xref 及其依赖的图层表未从源图继承（源图自带图层并入宿主图层体系的策略尚未定义）；
* DWG 作为参照源依赖自研 R12 读写通道，复杂实体建议先用本程序 JSON 或 DXF 交换；
* 定义内部残留的"对已分离 Xref 的引用"退化为不绘制（与既有 `removeBlock` 语义一致），
  条数会在分离日志中报告，便于人工核查。
