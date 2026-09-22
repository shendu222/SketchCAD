# 块（Block）功能设计与实现说明

> 参考 AutoCAD 的 **块（BLOCK / INSERT）**、**动态块（Dynamic Block）** 与 **属性块（Attribute Block / ATTDEF / ATTRIB / ATTDATAEXT）** 机制，
> 在 SketchCAD 的既有数据模型、命令接口与渲染管线上实现的完整块子系统。
>
> 本文给出：① 需求与语义总览 ② 数据模型 ③ 核心算法 ④ 实现映射 ⑤ 与现有结构的衔接
> ⑥ 关键实现步骤 ⑦ 边界情况清单 ⑧ 测试与验证 ⑨ 遗留问题。
>
> 代码位置：`src/core/Block.h` / `Block.cpp`（核心）、`src/core/SketchModel.{h,cpp}`（块表与事务）、
> `src/core/Commands.cpp`（命令族）、`src/ui/GraphicsView.{h,cpp}`（渲染/命中）、
> `src/core/Element.{h,cpp}`（图元字段与序列化）、`src/core/Geometry.{h,cpp}`（命中）、
> `src/ui/MainWindow.cpp`（图纸/导出接入）、`tests/block_test.cpp`（回归）。

---

## 1. 需求与语义总览

| 需求 | 语义（本实现） | 代码落点 |
|---|---|---|
| 块的定义与创建流程 | 选定一组图元 + 指定基点 → 生成唯一一份 **块定义 `BlockDef`**；可选「用块引用替换原图元」（AutoCAD `BLOCK` 的 delete 语义） | `SketchModel::createBlockFromSelection` / `block::makeDefFromEntities` |
| 块的插入与实例化 | 插入产生 **块引用图元 `ElementType::BlockRef`**，只记录 块名 + 插入点/旋转/缩放 + 属性值 + 动态参数值；几何在渲染/命中/导出时实时展开 | `SketchModel::insertBlock` / `block::makeRef` / `block::expandRef` |
| 编辑与重定义（同步更新所有引用） | 改 **一份** 块定义即可 —— 所有引用共享该定义，下次展开即为新几何，**无需反向推送** | `SketchModel::redefineBlock` / `redefineBlockFromSelection` |
| 动态块的可参数化特性 | 四类参数：**可见性**（命名状态切显隐）、**线性拉伸**（沿方向按距离）、**旋转**（绕基点）、**翻转**（关于轴镜像） | `DynParam` / `block::applyDynParams` / `block::hiddenSet` |
| 属性块的属性定义与属性值管理（含提取与编辑） | `AttributeDef`（ATTDEF）声明字段；实例持有 `blockAttrs`（ATTRIB）；支持默认值、编辑、提取、不可见、常量 | `block::makeRef` / `SketchModel::setBlockAttribute` / `extractBlockAttributes` / `block::extractAttributes` |
| 与现有数据结构/命令/渲染衔接 | 新类型追加在 `ElementType` 末尾；块表挂 `SketchModel` 并纳入撤销快照；命令走 `CommandRegistry`；渲染在 `ElementItem` 内展开 | 见 §5 |

### 核心设计原则：引用式（Reference）而非快照式（Snapshot）

图层子系统采用「快照式」（图元拷贝图层属性，改图层定义不回溯图元）；
块子系统**刻意采用相反的「引用式」**：

```
          ┌───────────────┐        引用（只存名字+变换+参数）
          │   BlockDef    │◄──────────────────────────────┐
          │  唯一一份定义  │                                │
          │  entities[]   │        ┌───────────────┐       │
          │  attdefs[]    │        │  BlockRef #1  │───────┘
          │  dynParams[]  │        │  BlockRef #2  │
          └───────────────┘        │  BlockRef #3  │
                 ▲                 └───────────────┘
                 │  redefineBlock 只改这里
                 └── 所有引用在渲染/命中/导出的那一刻「展开」出新几何
```

**收益**：`redefineBlock` 天然让全部引用同步更新（O(1) 改定义 + 渲染期 O(k) 展开），
这正是 AutoCAD 块的核心价值；**代价**：渲染/命中/导出每一次都需要展开（见 §9 遗留问题）。

---

## 2. 数据模型

### 2.1 块定义 `BlockDef`（`src/core/Block.h`）

```cpp
struct BlockDef {
    QString name;                      // 块名（唯一键）
    QString description;
    Vec3    basePoint;                 // 基点（插入时与插入点对齐）
    std::vector<Element> entities;     // 块内图元（块坐标系 = 定义时的 WCS 绝对坐标）
    std::vector<AttributeDef> attdefs; // 属性定义（ATTDEF）
    std::vector<DynParam>     dynParams;// 动态参数定义
    bool    annotative = false;        // 注释性（占位，暂不参与展开）
};
```

> **块内图元保留定义时的世界坐标**，基点只作为「插入对齐参考」。
> 于是「把块插回它被创建的位置」是恒等变换（test §1 已断言），
> 这与 AutoCAD「块定义几何位于 WCS」的做法一致，也让展开公式最简。

### 2.2 属性定义 `AttributeDef`

| 字段 | 含义 |
|---|---|
| `tag` | 属性标签（块内唯一，提取/编辑的键） |
| `prompt` | 插入时提示语 |
| `defaultValue` | 默认值（插入时写入实例的 `blockAttrs`） |
| `pos` | 块坐标系中的文本锚点 |
| `height` | 文字高度(mm) |
| `constant` | 常量属性：`setBlockAttribute` 拒绝修改 |
| `invisible` | 不可见属性：展开时不产生可见文字，但仍参与属性提取 |

### 2.3 动态参数 `DynParam`

```cpp
enum class DynParamType { Visibility, LinearStretch, Rotate, Flip };

struct VisibilityState {          // 可见性：一个命名状态 + 该状态被隐藏的块内图元索引
    QString name;
    std::vector<int> hiddenEntities;
};

struct DynParam {
    QString name;                 // 参数名（实例按名赋值）
    DynParamType type;
    std::vector<int> affected;    // 影响的块内图元索引（拉伸/旋转/翻转）
    Vec3   base;                  // 拉伸固定端 / 旋转中心 / 翻转轴上一点
    Vec3   direction;             // 拉伸方向 / 翻转轴方向
    double defaultValue;
    double minValue, maxValue;    // 值域；maxValue<=minValue 表示不限制
    std::vector<VisibilityState> states;  // 仅可见性使用
};
```

### 2.4 块引用图元字段（追加到 `Element`）

```cpp
// ElementType::BlockRef 专用
QString blockName;                       // 引用的块名
double  scaleX = 1.0, scaleY = 1.0;      // 插入缩放
std::vector<BlockAttrValue> blockAttrs;  // 属性值（tag→value）
std::vector<BlockDynValue>  dynValues;   // 动态参数取值（value / state）
// 复用既有字段：pts[0]=插入点，rotation=插入旋转角(度)
```

`BlockAttrValue` / `BlockDynValue` 刻意定义在 `Element.h`（而非 `Block.h`）：
`BlockDef` 需要 `Element`，若 `Element` 再反向包含 `Block.h` 会造成**头文件循环**；
这两个是纯 POD、无外部依赖，放在 `Element.h` 最自然。

**命名空间约定**：数据模型（`BlockDef` / `AttributeDef` / `DynParam` / `DynParamType` /
`VisibilityState` / `X2D`）放在 `SketchCAD` 顶层，便于 `SketchModel` / `Commands` / `MainWindow`
直接使用；**纯算法**放在 `SketchCAD::block`（`expandRef` / `makeRef` / `extractAttributes` /
`makeDefFromEntities` / `blockDefToJson` / `blockDefFromJson`）。

> ⚠️ **编译期约束**：C++ 要求成员函数定义所在的命名空间必须**包含**类所在命名空间。
> `X2D` 属于 `SketchCAD`，故其成员函数定义**不能**写在 `SketchCAD::block` 内 ——
> 这是 Block.cpp 把 X2D 实现放在 `namespace block` 之前的原因。

---

## 3. 核心算法

### 3.1 插入变换：2D 仿射 `X2D`

一次块插入 = 「平移到插入点 · 旋转 · 缩放 · 平移到基点」的复合，统一表示为 2×2 线性部分 + 平移：

$$\mathbf{p}' = M\,\mathbf{p} + \mathbf{t},\qquad M = R(\theta)\,S(s_x,s_y),\qquad \mathbf{t} = P - R(\theta)S(s_x,s_y)\,\mathbf{b}$$

```cpp
X2D X2D::insert(const Vec3& P, double rotDeg, double sx, double sy, const Vec3& bp) {
    const double a = rotDeg*PI/180, c = cos(a), s = sin(a);
    X2D x;
    x.m00 = c*sx;  x.m01 = -s*sy;      // 线性部分 = R(a) · S(sx,sy)
    x.m10 = s*sx;  x.m11 =  c*sy;
    Vec3 rbp(m00*bp.x + m01*bp.y, m10*bp.x + m11*bp.y, 0);
    x.t = P - rbp;                     // 使块基点恰好落在插入点上
    return x;
}
```

**复合**（嵌套块的关键）：`this.then(outer)` 返回 `outer ∘ this`：

$$\text{result}.M = M_{o}M,\qquad \text{result}.\mathbf{t} = M_{o}\mathbf{t} + \mathbf{t}_{o}$$

```cpp
X2D X2D::then(const X2D& outer) const {
    X2D r;
    r.m00 = outer.m00*m00 + outer.m01*m10;   // M_o · M
    r.m01 = outer.m00*m01 + outer.m01*m11;
    r.m10 = outer.m10*m00 + outer.m11*m10;
    r.m11 = outer.m10*m01 + outer.m11*m11;
    r.t   = outer.apply(t);                  // M_o·t + t_o
    return r;
}
```

有了 `then`，**任意嵌套深度都只需左乘父变换**：

```cpp
// 子块（内层）插入变换先算，再复合到父变换上
X2D childXf = X2D::insert(childIns, childRot, childSx, childSy, childDef.basePoint).then(parentXf);
```

辅助量：
- `scaleX()/scaleY()` = 线性部分两列向量长度（各轴缩放量）；
- `scaleFactor()` = `sqrt(|det M|)`（两轴缩放的**几何平均**）→ 半径/字高按此缩放；
- `applyAngleDeg(θ)` = 把角度向量过线性部分后取 `atan2` → 圆弧起止角/椭圆长轴角/剖面线方向角；
- `isAxisAligned()` = 线性部分是否近似对角（`m01≈m10≈0`）→ 判定矩形能否保持轴对齐表示。

### 3.2 图元变换 `transformElement`（按类型分派）

| 图元 | 处理 |
|---|---|
| 所有类型 | `pts`（及 `Hatch.holes`）逐点 `xf.apply` |
| **矩形** | **若线性部分含旋转/错切**（`!isAxisAligned()`）→ 离散为 **4 顶点多边形**，否则保持矩形（轴对齐表示才成立） |
| 圆 | 等比例 → `radius *= scaleFactor()`；**非等比例 → 转为椭圆**（半轴 `r·lx`、`r·ly`，长轴角取较长轴方向） |
| 圆弧 | `radius *= scaleFactor()`；`startAngle/endAngle = applyAngleDeg(...)` |
| 椭圆 | `radius/radiusY *= scaleFactor()`；`rotation = applyAngleDeg(...)` |
| 剖面区域 | `radius(间距) *= scaleFactor()`；`rotation = applyAngleDeg(...)` |
| 文本 | `radius(字高) *= scaleFactor()` |
| 标注 | `radius *= scaleFactor()`；起止角 `applyAngleDeg` |

> 矩形→多边形的转换是**必须**的：`Rectangle` 在数据模型中是「对角两点 + 轴对齐」表示
> （渲染走 `QRectF(...).normalized()`），一旦带旋转就会画出错误的轴对齐框。

### 3.3 展开：`expandRef → evalDef` 递归内核

```
expandRef(ref, table)
├─ ref.type != BlockRef            → 空
├─ 查不到 BlockDef                  → 空（不画任何东西，安全）
├─ 取插入点 pts[0]、旋转 rotation、缩放 (scaleX, scaleY)（|s| 下限钳到 1e-6，防奇异）
├─ xf = X2D::insert(插入点, 旋转, sx, sy, def.basePoint)
└─ evalDef(def, xf, ref.dynValues, ref.blockAttrs, table, visiting={ref.blockName}, 0, ref.layer)

evalDef(def, xf, dyn, attrs, table, visiting, depth, fallbackLayer)
├─ hidden = hiddenSet(def, dyn)                    // 可见性：当前状态隐藏的索引集合
├─ for i, e in def.entities:
│   ├─ if hidden.count(i) → 跳过
│   ├─ if e 是 BlockRef:                            // ── 嵌套
│   │   ├─ cd = findDef(e.blockName)；未定义/深度超限 → 跳过
│   │   ├─ if e.blockName ∈ visiting → 跳过          // ── 环检测
│   │   ├─ childXf = X2D::insert(e.pts[0], e.rotation, e.scaleX, e.scaleY, cd.basePoint).then(xf)
│   │   ├─ visiting.push(e.blockName)
│   │   ├─ sub = evalDef(*cd, childXf, e.dynValues, e.blockAttrs, table, visiting, depth+1, childLayer)
│   │   └─ visiting.pop(); out += sub
│   ├─ applyDynParams(e, i, def.dynParams, dyn)     // 动态参数（块坐标系内，先于插入变换）
│   ├─ transformElement(e, xf)                      // 插入变换 → 世界坐标
│   ├─ 层回落：e.layer 为空/"0" → fallbackLayer      // 块内「随层」图元改用插入层
│   ├─ e.id = 0；e.visible = true                   // 展开是瞬态几何，不携带工作区 ID
│   └─ out += e
└─ for ad in def.attdefs:                           // ── 属性文本
    ├─ ad.invisible → 跳过；值为空 → 跳过
    ├─ val = attrs[ad.tag] 或 ad.defaultValue
    └─ out += Text{ pos = xf.apply(ad.pos), text = val, 字高 = ad.height * xf.scaleFactor() }
```

**顺序很关键**：动态参数在**块坐标系内**先应用（拉伸/旋转/翻转都定义在块自己的坐标系里），
**之后**才做插入变换把它送到世界坐标。反过来会让「旋转参数」与「插入旋转」互相污染。

### 3.4 动态参数应用 `applyDynParams`

对每个参数，若 `idx ∈ param.affected`（否则跳过），把 `direction` 单位化后：

| 类型 | 算法 |
|---|---|
| **LinearStretch** | 对每个顶点 $\mathbf{p}$：若 $(\mathbf{p}-\mathbf{base})\cdot\hat{d} > 0$（"可动侧"）→ $\mathbf{p} \mathrel{+}= \hat{d}\cdot v$。半径/半轴**不变**（只移动可动侧顶点）。值先经值域钳制。 |
| **Rotate** | 对 `pts`/`holes` 绕 `base` 旋转 $v$ 度；`Arc` 起止角 `+= v`；`Ellipse/Hatch` 的 `rotation += v`。 |
| **Flip** | 当 $v \neq 0$：对 `pts`/`holes` 关于「过 `base`、方向 `direction`」的直线作镜像（$\mathbf{p}' = \mathbf{base} + 2\hat{d}(\mathbf{v}\cdot\hat{d}) - \mathbf{v}$）。角度按 $\theta' = 2\beta - \theta$ 反射（$\beta$ = 轴世界角）；`Arc` 交换起止角以保持扫掠方向；闭合多段线反向存点保持走向一致。 |
| **Visibility** | 不在此处处理，由 `hiddenSet` 在遍历前预过滤。 |

### 3.5 可见性解析 `hiddenSet`

对每个可见性参数：取实例当前 `state`（`dynStateOf`），在 `param.states` 中匹配；
**未指定或未匹配 → 取首状态**（保证"一定有且仅有一个状态生效"）；把该状态的
`hiddenEntities` 并入隐藏集合。多参数叠加时取并集。

### 3.6 属性替换与提取

- **替换（展开时）**：按 `attdefs` 顺序，取实例 `blockAttrs[tag]`，缺失则用 `defaultValue`，
  生成 `Text` 图元（字高随 `scaleFactor()` 缩放）。`invisible` 与空值不生成可见文字。
- **提取（`extractAttributes`）**：以**块定义**的 `attdefs` 为主键序，逐条填实例值（缺失取默认），
  再把「实例上多出、定义中已删」的 tag 追加 —— 保证**不丢数据**，供 BOM/明细表使用。

### 3.7 环检测与深度限制

- `visiting` 是**当前递归路径**的块名栈：遇到已在栈中的块名即判定成环，跳过该分支
  （而非全局黑名单 —— 同一块在不同分支出现是合法的 DAG，不能误杀）。
- `kMaxBlockDepth = 16` 双保险，防病态深链。
- 自引用块（`BlockDef.entities` 含指向自身的 `BlockRef`）安全返回空，不递归爆栈（test §6 已断言）。

---

## 4. 实现映射

### 4.1 文件 × 职责

| 文件 | 职责 |
|---|---|
| `src/core/Block.h` | 数据模型（`BlockDef` / `AttributeDef` / `DynParam` / `VisibilityState` / `X2D`）+ `block::` 算法声明 |
| `src/core/Block.cpp` | `X2D` 实现 + 展开/求值/变换/动态参数/序列化 |
| `src/core/Element.h` | `ElementType::BlockRef`（追加末尾）+ `BlockAttrValue` / `BlockDynValue` + `Element` 块字段 + `elementToJson/FromJson` 声明 |
| `src/core/Element.cpp` | `sizeDesc()` 的 BlockRef 分支 + **图元序列化单一实现**（顶层图纸与块内图元共用） |
| `src/core/ElementFactory.{h,cpp}` | `createBlockRef()`（引用骨架；完整字段由 `block::makeRef` 填） |
| `src/core/SketchModel.{h,cpp}` | 块表 `blocks_`、增删改/改名、`createBlockFromSelection` / `redefineBlock*` / `insertBlock` / `expandBlockRef` / `explodeBlockRef` / 属性与动态参数 setter / `extractBlockAttributes` / `blocksToJson` / `setBlocksFromJson` / `boundsOf`；`Memento` 纳入块表；信号 `blocksChanged` |
| `src/core/Commands.{h,cpp}` | `BlockCommand` 命令族（`block` / `attdef` / `attedit` / `attlist` / `dynset` / `dynlist`）+ 注册 + 帮助 |
| `src/core/Geometry.{h,cpp}` | `distanceToElements()`（对展开集合取最小距离）+ `distanceToElement` 的 BlockRef 兜底分支 |
| `src/ui/GraphicsView.{h,cpp}` | `ElementItem::model` 指针、`expandedBlock()` / `blockPath()` / `paintBlockRef()` / `penFor()`；`rawBounds` / `shape` / `paintOne` / `rebuild` / `hitTest` / `contentBounds` / `elementSceneBounds` 的 BlockRef 接入 |
| `src/ui/MainWindow.cpp` | 存档 `blocks` 字段、读档 `setBlocksFromJson`、DXF/DWG/STEP 导出前展开块引用、`elementToJson/FromJson` 委托核心层 |
| `tests/block_test.cpp` | 9 组回归（见 §8） |

### 4.2 命令族（`CommandRegistry`）

| 命令 | 用法 | 说明 |
|---|---|---|
| `block create` | `block create <块名> [基点x,y] [delete\|keep]` | 从当前选择集建块；基点缺省取选择集包围盒中心；`delete`（默认）用块引用替换原图元 |
| `block insert` | `block insert <块名> <x,y> [旋转] [sx] [sy]` | 插入块引用 |
| `block list` | `block list` | 列出全部块定义（图元/属性/参数计数） |
| `block info` | `block info <块名>` | 块定义详情（基点、属性列表） |
| `block redefine` | `block redefine <块名>` | 用当前选择集**重定义**块，所有引用同步 |
| `block explode` | `block explode <引用id>` | 分解为独立图元（可一次 Ctrl+Z 回滚） |
| `block delete` / `block rename` | `block delete <块名>` / `block rename <旧> <新>` | 删块 / 改名（引用同步改名） |
| `attdef` | `attdef <块名> <标签> [提示] [x,y] [字高]` | 追加属性定义（属性块） |
| `attedit` | `attedit <引用id> <标签> <值...>` | 编辑属性值（值可含空格） |
| `attlist` | `attlist [引用id]` | 属性提取（省略 id 则提取全部引用） |
| `dynset` | `dynset <引用id> <参数名> <值\|状态名>` | 设置动态参数（数值按值域钳制；可见性按状态名切换） |
| `dynlist` | `dynlist <块名>` | 列出块定义的动态参数与状态 |

### 4.3 `SketchModel` 新增 API（对外契约）

```cpp
int  blockCount() const;  const std::vector<BlockDef>& blockTable() const;
const BlockDef* findBlockDef(const QString&) const;   BlockDef* findBlockDef(const QString&);
bool hasBlock(const QString&) const;
bool addBlockDef(const BlockDef&);
int  createBlockFromSelection(const QString& name, const Vec3& basePt, bool deleteSource,
                              const QString& description = QString());
bool redefineBlockFromSelection(const QString& name);
bool redefineBlock(const BlockDef&);
bool removeBlock(const QString&);          bool renameBlock(const QString&, const QString&);
int  insertBlock(const QString&, const Vec3& pos, double rot=0, double sx=1, double sy=1);
std::vector<Element> expandBlockRef(const Element&) const;
std::vector<int> explodeBlockRef(int refId);
bool setBlockAttribute(int refId, const QString& tag, const QString& value);
bool setBlockDynValue(int refId, const QString& name, double value);
bool setBlockDynState(int refId, const QString& name, const QString& state);
std::vector<BlockAttrValue> extractBlockAttributes(int refId) const;
QJsonArray blocksToJson() const;           void setBlocksFromJson(const QJsonArray&);
// signal: void blocksChanged();
```

---

## 5. 与现有数据结构 / 命令接口 / 渲染逻辑的衔接

### 5.1 数据结构

| 接入点 | 处理 |
|---|---|
| `ElementType` 枚举 | `BlockRef` **追加到末尾**（值 = 12），遵守「只增不改」铁律 → 旧存档整数序号不受影响 |
| `Element` 字段 | 新增 `blockName / scaleX / scaleY / blockAttrs / dynValues`；复用 `pts[0]`（插入点）与 `rotation`（插入角），**不新增同义字段** |
| 图元序列化 | 原 `MainWindow::elementToJson/FromJson` 下沉为核心层 `SketchCAD::elementToJson/FromJson`（`Element.cpp`），顶层图纸与块内图元**共用一份字段表**；`MainWindow` 的成员函数改为薄委托。新增字段对非 BlockRef 无副作用，旧存档缺键一律回落默认 |
| `SketchModel::Memento` | 增加 `std::vector<BlockDef> blocks` → **块表随撤销快照一起回退**（创建/重定义/插入/分解/删块均可 Ctrl+Z） |
| `SketchModel::clear()` | 一并清空 `blocks_` 并发 `blocksChanged`（新建图纸不残留块定义） |
| 信号 | 新增 `blocksChanged()`；块变化同时发 `modelChanged()`（引用几何变了，必须重绘） |

### 5.2 命令接口

- 新增 `BlockCommand : ICommand`，在 `CommandRegistry::registerBuiltins()` 中以 6 个精确名注册
  （`block` / `attdef` / `attedit` / `attlist` / `dynset` / `dynlist`），按 `ctx.cmd` 分派 ——
  与既有 `ModifyCommand` / `SelectCommand` / `LayerCommand` 同构，零改动既有注册结构。
- 全部写入走 `pushMemento` + `*Quiet` 的既有事务范式，保证「一次操作一次 Ctrl+Z」。
- `HelpCommand` 增补块相关用法行。

### 5.3 渲染 / 命中 / 取景

`ElementItem` 新增 `const SketchModel* model`（由 `GraphicsView::rebuild()` 注入），
渲染块引用时**展开后逐个子图元复用 `paintOne`**：

```cpp
void ElementItem::paintBlockRef(QPainter* p, const Element&) const {
    p->setBrush(Qt::NoBrush);
    for (const auto& sub : expandedBlock()) {
        if (sub.type == ElementType::BlockRef) continue;   // 展开即递归，不会出现
        p->setPen(penFor(sub));                            // 与独立图元同一套笔参数
        paintOne(p, sub);                                  // 复用全部既有绘制分支
    }
}
```

为保证「块内图元怎么画」与「独立图元怎么画」**观感完全一致**，把 `paint()` 里的画笔构造
抽取为 `penFor(const Element&)`，`paint()` 与 `paintBlockRef()` 共用。

| 能力 | 接入方式 |
|---|---|
| 绘制 | `paintOne` 增加 `BlockRef → paintBlockRef` 分支 |
| 包围盒 | `rawBounds`：BlockRef 取「展开子图元包围盒并集」；未定义/空块退化为插入点 |
| 命中（QGraphicsItem） | `shape()`：BlockRef 返回 `blockPath()`（子图元路径并 ∪ 描边）→ 点块内任一图形都能选中整个块 |
| 命中（距离法） | `GraphicsView::hitTest`：BlockRef 走 `geo::distanceToElements(展开集合, p)` |
| 框选 | `SketchModel::selectWindow/deselectWindow` 改用新增私有 `boundsOf()`（BlockRef 展开取并集） |
| 适配窗口 | `GraphicsView::contentBounds()` 显式展开 BlockRef 取并集（`geometryBounds` 是静态函数、拿不到块表） |
| 导出 | `MainWindow::writeDrawing` 在 DXF/DWG/STEP 前把 BlockRef 展开为真实子图元（这三种格式不认识 BlockRef） |

---

## 6. 关键实现步骤

1. **扩展图元类型**（遵守 `docs/` 中的「新增图元类型同步清单」）
   `Element.h`：枚举末尾加 `BlockRef` → `elementTypeName` 加分支 → 加 `BlockAttrValue/BlockDynValue` 与块字段；
   `Element.cpp`：`sizeDesc()` 加分支。
2. **建立块子系统**：新增 `Block.h`（数据模型 + `X2D` + 算法声明）与 `Block.cpp`
   （变换数学 → 图元变换 → 动态参数 → 可见性 → 属性 → 递归展开 → 序列化）。
3. **下沉图元序列化**：把 `elementToJson/FromJson` 提到核心层，`MainWindow` 改为委托 ——
   为「块定义内含图元」提供同一份字段表。
4. **块表进模型**：`SketchModel` 加 `blocks_`、增删改 API、`Memento` 纳表、`blocksChanged` 信号、`boundsOf`。
5. **工厂**：`ElementFactory::createBlockRef`（引用骨架；常规路径用 `SketchModel::insertBlock`）。
6. **命令族**：`BlockCommand` + 注册 + 帮助。
7. **渲染/命中接入**：`ElementItem::model`、`penFor`、`expandedBlock/blockPath/paintBlockRef`，
   以及 `rebuild/shape/rawBounds/paintOne/hitTest/contentBounds/elementSceneBounds` 的 BlockRef 分支。
8. **存档与导出**：`writeDrawing` 写 `blocks`、`readDrawing` 读 `blocks`、导出前展开块引用。
9. **构建注册**：`CMakeLists.txt` 的 `CORE_SOURCES` 加 `src/core/Block.cpp`，新增 `BlockTest` 目标。
10. **回归**：`ninja -j 1 BlockTest` → 运行 → 再构建 `SketchSelfTest/LayerTest/FileRoundTripTest/SketchCAD` 确认无回归。

---

## 7. 边界情况清单（已实现并测试）

| 类别 | 情况 | 行为 |
|---|---|---|
| 定义 | 空块名 / 重名 | `addBlockDef` 返回 false 并记日志 |
| 定义 | 无选择集建块 | `createBlockFromSelection` 返回 -1 |
| 定义 | `delete` 模式删除原图元 | 先**拷贝**选择集 ID 再删；直接边遍历边删会失效迭代器（已修） |
| 插入 | 块不存在 | 返回 -1 |
| 插入 | 缩放为 0 / 极小 | 钳到 `±1e-6`，几何不塌陷为一点、不产生奇异矩阵 |
| 插入 | 非等比例缩放 | 圆→椭圆（精确）；圆弧半径取几何平均（**近似**，圆弧无椭圆弧表示）；矩形带旋转→多边形（精确） |
| 展开 | 块未定义 / 空块 | 返回空列表（不绘制、不崩溃）；包围盒退化为插入点，仍可选中 |
| 展开 | 自引用 / 相互引用成环 | `visiting` 路径栈检测 → 跳过，不递归爆栈 |
| 展开 | 嵌套过深 | `kMaxBlockDepth=16` 截断 |
| 展开 | 嵌套块「随层」图元 | 逐层向上继承 `fallbackLayer`（内层"0"→父引用层→…→顶层引用层） |
| 属性 | 常量属性 | `setBlockAttribute` 拒绝并记 warn |
| 属性 | 不可见属性 | 不生成可见文字，但参与 `extractBlockAttributes` |
| 属性 | 定义中已删、实例仍留的 tag | 提取时追加保留，不丢数据 |
| 属性 | 值为空 | 不生成可见文字（避免一堆空白 Text 干扰拾取） |
| 动态 | 参数值超值域 | 按 `min/max` 钳制 |
| 动态 | 对非可见性参数设置状态名 / 未声明的状态 | 拒绝并记 warn |
| 动态 | `affected` 索引因重定义越界 | 展开时自然不匹配（`find` 失败即跳过），**不会越界访问** |
| 重定义 | 引用同步 | 全部引用共享一份定义，改即生效（无需遍历引用） |
| 删块 | 仍有引用 | 允许删除；引用展开为空（暂不绘制），可撤销恢复 |
| 改名 | 有引用 | 同步改写全部引用的 `blockName` |
| 序列化 | 旧存档无 `blocks` 键 | 读入 0 个块定义，不报错 |
| 序列化 | 块引用图元字段 | `elementToJson/FromJson` 往返保持块名/缩放/旋转/属性/动态参数 |
| 撤销 | 建块 / 插入 / 重定义 / 分解 / 删块 | 各自一次 Ctrl+Z 可回退（块表与图元一起） |
| 其他 | 分解非块引用 | 返回空并记 warn |
| 其他 | 空撤销栈 `undo()` | 安全（记 "无可撤销操作"） |

---

## 8. 测试与验证

`tests/block_test.cpp`（`CMakeLists.txt` 注册为 `BlockTest`，核心层无需 GUI），共 9 组、约 60 项断言：

| 组 | 覆盖 |
|---|---|
| §1 块定义与创建流程 | 从选择集建块（delete/keep）、块内图元数、原图元替换、恒等展开、空名/重名拒绝 |
| §2 插入与实例化 | 平移/旋转 90°/等比例 2× 的几何断言；非等比例圆→椭圆；带旋转矩形→多边形；未定义块安全返回空 |
| §3 重定义同步 | 重定义后**两个引用同时**变新几何；分解；改名同步；删块后展开为空 + 撤销恢复 |
| §4 动态块 | 可见性状态切换、线性拉伸（可动侧/固定侧）、旋转 90°、翻转（Y 轴镜像）、值域钳制、非可见性参数拒绝状态 |
| §5 属性块 | 默认值初始化、属性文字生成、编辑后同步、属性提取、不可见属性（不绘制但仍提取）、常量拒绝修改、追加 tag |
| §6 嵌套与环检测 | 子块线 `(0,0)-(10,0)` → 父块内 `(5,5)-(15,5)`；父块平移后 `5→105`；自引用环安全 |
| §7 序列化 | 块表 JSON 往返（图元/属性定义/动态参数/状态/基点）；图元级往返（块名/缩放/旋转/属性/动态）；空块表兼容 |
| §8 撤销与重做 | 撤销建块（含原图元恢复）、重做、撤销插入、撤销分解 |
| §9 边界情况 | 插入不存在块、无选择建块、空块、缩放 0 钳制、分解非块引用、空栈 undo |

**本轮验证结果**：`BlockTest` 全部通过（0 项失败）；
并回归构建 `SketchSelfTest` / `LayerTest` / `FileRoundTripTest` / 主程序 `SketchCAD`。

---

## 9. 遗留问题

### P1 功能对齐（与 AutoCAD 动态块/属性块相比）

1. **无可视化编辑界面**：块参数只能在**定义层**用代码/JSON 组装（`addBlockDef`），
   命令行只能设**实例值**（`dynset` / `attedit`）。缺「动态块参数编辑器」与「块属性管理器」对话框。
2. **无块管理器 UI / 无块库**：已补「插入块」选择对话框（含实时几何预览，复用 `ElementItem` 渲染路径）
   与「块管理器」对话框（重命名 / 删除 / 用当前选择重定义）；但仍无块库文件（`.dwg` 块交换）。
3. **菜单/工具条入口（已新增）**：已在【编辑>修改>块(&B)】与【绘图>块(&B)】两处加入「块(&B)」子菜单，
   封装 `create` / `insert`（带预览）/ `redefine` / `explode` / `delete` / `rename` 为可视化操作；
   新增菜单项已同步 `ModifyMenuTest` 期望并配 `block` 矢量图标。命令行块能力（含 `attdef`/`attedit`/
   `dynset`）仍保留可用。
4. **动态参数能力有限**：
   - 拉伸只移动 `affected` 中**可动侧**顶点，未支持 AutoCAD 的「拉伸框 + 按框内顶点」的框选语义；
   - 无「阵列（Array）」「查寻（Lookup）」「对齐（Alignment）」参数；
   - 无参数之间的依赖/约束图（如「翻转后拉伸方向自动相反」）。
5. **属性块**：无「属性同步（ATTSYNC）」「属性批量提取到表格/文件（ATTEXT）」；
   属性文字**不随插入旋转**（`paintText` 只画水平文字）——与 AutoCAD 块内文字可旋转不一致。
6. **DWG/DXF 块互操作**：DXF **导出**已展开块引用（几何不丢，但失去块层级）；
   DXF **导入**不解析 `BLOCK`/`INSERT`（会退化为缺失几何）；无 DWG 块读写。

### P2 健壮性 / 一致性

7. **展开无缓存**：每次 `rebuild()` / 命中 / 取景都重新展开全部引用。
   引用数多、块内图元多时是 O(引用数 × 块内图元数) 的重复计算。
   改进方向：在 `SketchModel` 内按「块定义版本号 + 实例参数哈希」做展开缓存，`blocksChanged` 时失效。
8. **命中与渲染的展开时机不一致**：`hitTest` 用 `geo::distanceToElements`（近似，含弧段采样），
   而 `shape()` 用路径并集（精确）。极端细长图形下两者可能给出一像素级差异的结论。
9. **`geo::elementBounds(BlockRef)` 仍退化**：该函数是无模型依赖的纯几何函数，
   BlockRef 只能返回插入点包围盒。所有**已知**调用点（模型内框选、视图取景、导出）已改走展开版本，
   但未来新增调用点需注意这一「默认退化」的陷阱。
10. **嵌套块的图层回落**只处理「块内层为默认层」这一最常见情形；
   未实现 AutoCAD 的 BYBLOCK 颜色/线型语义（即块内图元属性设为 BYBLOCK 时继承引用者属性）。
11. **`removeBlock` 允许删除仍有引用的块**（引用退化为不绘制）。
    AutoCAD 会拒绝删除被引用的块；本实现选择「允许 + 可撤销 + 明确日志」，需在 UI 上给用户提示。
12. **块内标注**：块内的 `Dimension` 展开时只按比例缩放尺寸弧半径与字高，
    测量值仍按块内几何计算；跨块驱动（`refA/refB` 指向块外图元）在块内无意义，未做防呆。

---

## 10. 附录

- **数据模型**：`src/core/Block.h`
- **算法实现**：`src/core/Block.cpp`
- **块表与事务**：`src/core/SketchModel.h` / `SketchModel.cpp`（搜索 `块（Block）管理`）
- **命令族**：`src/core/Commands.cpp`（搜索 `块（Block）命令族`）
- **渲染接入**：`src/ui/GraphicsView.cpp`（搜索 `paintBlockRef` / `blockPath` / `expandedBlock`）
- **回归测试**：`tests/block_test.cpp`（`ninja -C build -j 1 BlockTest && ./build/BlockTest.exe`）
- **相关标准**：GB/T 14665-2012（图线）、GB/T 4457.5-2013（剖面符号）—— 块内图元沿用既有图层/线型/剖面机制
