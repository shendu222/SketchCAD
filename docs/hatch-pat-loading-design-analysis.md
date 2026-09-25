# 剖面填充 PAT 图案文件加载 — 设计分析与实施说明

> 功能：SketchCAD 支持从可执行文件同级 `hatch/` 子目录加载 AutoCAD 格式（`.pat`）填充图案，
> 并据此生成剖面填充线。发布于 `exe/hatch/` 放置 `.pat` 即可自动定位读取。
>
> 参考样例：`D:\01-CAD\02-avticationCAD\wood.pat`（`*HOLZ-2` / `*HOLZ-3`，标准 AutoCAD PAT）。
>
> 文档日期：2026-09-23 ｜ 模块：`src/core/HatchLibrary.{h,cpp}`、`src/core/Section.{h,cpp}`、`build.sh`

---

## 目录

1. [问题背景与需求界定](#1-问题背景与需求界定)
2. [核心问题的理论基础与依据](#2-核心问题的理论基础与依据)
3. [算法原理与说明](#3-算法原理与说明)
4. [具体实施方法与步骤](#4-具体实施方法与步骤)
5. [关键假设与推导逻辑](#5-关键假设与推导逻辑)
6. [部署与可复现性设计](#6-部署与可复现性设计)
7. [验证与结论](#7-验证与结论)
8. [附录：关键代码结构与契约](#8-附录关键代码结构与契约)

---

## 1. 问题背景与需求界定

### 1.1 需求原文

> 修改现有程序，使其支持从 PAT 填充图案文件中读取数据并据此填充剖面图。程序运行时须从可执行文件
> （exe）所在目录下的 `hatch` 子目录加载相应的 PAT 文件。开发与测试可参考
> `D:\01-CAD\02-avticationCAD\wood.pat` 示例文件。发布部署时，将 PAT 文件放置于 exe 同级 `hatch`
> 目录中，确保程序能自动定位并读取该路径下的 PAT 文件完成剖面填充。

### 1.2 需求分解为可验收的技术目标

| 编号 | 技术目标 | 验收判据 |
|---|---|---|
| R1 | 运行时自动定位 `exe/hatch/` 并加载其中 `.pat` | 无需用户配置路径，`applicationDirPath()/hatch` 生效 |
| R2 | 正确解析 AutoCAD `.pat` 语法 | 读取 `wood.pat` 得到 `HOLZ-2`/`HOLZ-3` 两个图案 |
| R3 | 用解析结果**据此生成剖面填充线** | 木材剖面填充呈现 `wood.pat` 的木纹笔段 |
| R4 | 不破坏既有剖面符号体系 | 玻璃/液体/砖墙/石材/铸铁等既有图案行为不变 |

### 1.3 与既有架构的关系：**纵向扩展，而非重建**

既有剖面填充管线为「材料（`SectionMaterial`）→ 图案（`HatchPattern`）→ 几何（`generateHatchLines`）→ 渲染（`GraphicsView::paintHatch`）」。需求引入的是**图案来源**的一条新支线：从「内置参数化生成」之外，增加「读取外部 PAT 文件」。因此设计目标不是替换管线，而是在**图案层**增加一个**来源分支**，下游几何与渲染链路保持不变。

---

## 2. 核心问题的理论基础与依据

### 2.1 AutoCAD PAT 格式规范

`.pat` 是 AutoCAD 定义的**填充图案文本格式**，其文法为：

```
*NAME, description                       ← 图案头（* 开头；逗号前为图案名，逗号后为描述）
angle, x0, y0, dx, dy [, d1, d2, ...]    ← 一条图案线定义（同一图案可有多行）
angle, x0, y0, dx, dy [, d1, d2, ...]
...
```

字段语义：

| 字段 | 含义 | 单位/取值 |
|---|---|---|
| `angle` | 该族线与 X 轴的夹角，逆时针为正 | 度（°） |
| `x0, y0` | 图案**基准点**（该族线的相位锚点） | 与图面一致（本项目按 mm） |
| `dx, dy` | 相邻平行线在**图案局部坐标（未旋转）**下的偏移 | 与图面一致（mm） |
| `d1, d2, …` | **笔段序列**：正=落笔（实线段）、负=抬笔（间隔）；从第一个实线段开始 | 与图面一致（mm） |

关键性质：

- **一个 `.pat` 文件可含多个 `*NAME` 图案**（如 `wood.pat` 含 `HOLZ-2`、`HOLZ-3`）。
- 行尾连续的 `0` 是**填充占位**（木纹类图案常见），不构成有效笔段。
- 以 `;` 或 `!` 开头的行是**注释**。

### 2.2 与 GB/T 4457.5-2013 的关系

GB/T 4457.5-2013《机械制图 剖面区域表示法》规定剖面线为「间隔相等、方向相同、与主要轮廓成 45° 的细实线」（4.1 条），并要求不同材料使用规定的剖面符号（表1）。既有实现已内置 19 种材料的参数化符号。

PAT 文件的价值在于：**表1 未穷举的数以百计的行业纹样**（木纹、砖石、编织、地貌等）可以由外部图案库补充。因此本功能是 GB 标准的**兼容性扩展**——当材料映射到某个 PAT 名称时，用 PAT 定义替代内置参数化生成；未映射的材料仍走内置逻辑，标准合规性不受影响。

### 2.3 与既有 `geo::hatchLines` 的坐标约定对齐

既有斜线族生成器 `geo::hatchLines`（`Geometry.cpp`）采用如下坐标变换：

```cpp
// 世界 → 局部（旋转 -angle，使目标角度的线变为水平扫描线）
toLocal :  x' =  x*cos + y*sin ,  y' = -x*sin + y*cos
// 局部 → 世界（旋转 +angle）
toWorld :  x  = x'*cos - y'*sin ,  y  =  x'*sin + y'*cos
```

为保持渲染语义一致（Y 轴在渲染期翻转，但世界系公式直接可用），PAT 生成器**必须复用同一套变换约定**，即 `toLocal` 用 `-angle` 旋转。这是本次实现的**一致性约束前提**。

---

## 3. 算法原理与说明

### 3.1 整体数据流

```
exe/hatch/*.pat
      │  QTextStream 读文本
      ▼
parsePatText(text)  ──►  std::vector<PatPattern>
                              │  索引：name.toLower() / filename-base.toLower()
                              ▼
                     HatchPatternLibrary（单例，懒加载）
                              │  find("wood")
                              ▼
hatchPatternFor(style) ──► HatchPattern（patName="wood"）
                              │
generateHatchLines(region) ──┼─ patName 非空且命中 ──► generateHatchLinesFromPat(pat, region)
                              │                                    │
                              │                              铺线 + 排笔段
                              ▼                                    ▼
                        内置参数化生成                    std::vector<geo::Seg>
                              └──────────────┬─────────────────────┘
                                             ▼
                              GraphicsView::paintHatch（setClipPath 裁剪）
```

### 3.2 解析算法（`parsePatText`）

逐行状态机：

```
for each line:
    trim；跳过空行、以 ';'/'!' 开头的注释行
    if line 以 '*' 开头:                       # 新图案头
        以首个 ',' 切分 → name / description
        追加 PatPattern；cur 指向它
    else:                                      # 定义行
        以 ',' 切分 → 数值列表 nums
        if nums.size() < 5: 跳过（非法行）
        PatLine.angle = nums[0]; x0=nums[1]; y0=nums[2]; dx=nums[3]; dy=nums[4]
        for i in [5, nums.size()):
            if |nums[i]| > 1e-6: dashes.push_back(nums[i])   # 丢弃长度0填充占位
        若尚无图案头 → 匿名收容为 "PATTERN"
```

要点：**长度阈值 `1e-6`** 用于剔除行尾 `0` 占位，保证 `HOLZ-2` 的 `5.25,-66.25, 6.5,-47, 0, 0, …` 只保留有效笔段 `[5.25, -66.25, 6.5, -47]`。

### 3.3 生成算法（`generateHatchLinesFromPat`）

核心思路：**在图案空间铺满平行线，再沿每条线按笔段序列切分实段**。

对 `pat` 中每一条 `PatLine`：

1. **建立图案空间变换**
   ```
   a = angleDeg · π/180 ;  c = cos(a) ;  s = sin(a)
   toLocal(p) = ( p.x·c + p.y·s ,  -p.x·s + p.y·c )    # 旋转 -a，使线水平
   toWorld(x',y') = ( x'·c - y'·s ,  x'·s + y'·c )      # 旋转 +a
   ```

2. **求区域在图案空间的包围盒**（含外边界与全部内孔），并外扩 `pad = 2mm`
   ```
   [xStart, xEnd] × [yLo, yHi]
   ```

3. **确定平行线族的整数索引范围**（`yp = y0 + k·dy` 落入 `[yLo, yHi]` 的 k）
   ```
   dy ≈ 0  → k ∈ {0}                          # 退化：单条线
   否则     k0 = ceil((yLo - y0)/dy)          # k 从 k0 到 k1
             k1 = floor((yHi - y0)/dy)
   ```
   > 相邻线垂直间距 = |dy|；沿线错位 = dx（营造错缝/木纹感）。

4. **逐条线排布笔段**（相位循环）
   ```
   周期 period = Σ|dashes[i]|
   若 period ≤ 1e-9  → 整条实线（无笔段定义）
   否则：
       该行相位 0 点 xbase = x0 + k·dx
       初始相位 phase = (xStart - xbase) mod period     # 归一到 [0, period)
       定位初始笔段 idx 及其段内已消费长度 cons
       x = xStart
       while x < xEnd:
           L = |dashes[idx]|
           segEnd = min(xEnd, x + (L - cons))
           if dashes[idx] > 0:  emit Seg{ toWorld(x, yp), toWorld(segEnd, yp) }
           x = segEnd;  cons = 0
           idx = (idx + 1) mod n        # 循环到下一笔段
   ```

**关键设计：线段不做 even-odd 裁剪，超出区域的部分交给 `paintHatch` 的 `setClipPath`。** 这与既有砌体图案（砖墙/石材）的契约完全一致——「生成器铺满包围盒、painter 裁剪」。如此：
- 生成器天然支持任意多边形 + 内孔（裁剪由 painter 的 path 统一完成）；
- 避免在生成器内重复实现复杂的 even-odd 配对；
- 预览与实际渲染走同一条渲染路径，保证**「预览 == 实际」**。

### 3.4 材料到 PAT 的映射（扩展点）

```cpp
static QString patNameForMaterial(SectionMaterial m) {
    switch (m) {
    case SectionMaterial::Wood: return QStringLiteral("wood");  // hatch/wood.pat 首个图案
    default: return QString();
    }
}
```

`hatchPatternFor` 末尾统一置位：

```cpp
const QString pn = patNameForMaterial(style.material);
if (!pn.isEmpty()) pat.patName = pn;
return pat;
```

`generateHatchLines` 开头分流：

```cpp
if (!pat.patName.isEmpty()) {
    const PatPattern* pp = HatchPatternLibrary::instance().find(pat.patName);
    if (pp) return generateHatchLinesFromPat(*pp, region);
    // 未找到 PAT（文件缺失）→ 回退内置逻辑，保证不退化成空白
}
```

> **范围克制**：当前仅 `Wood` 走 PAT。**刻意不覆盖**玻璃（45° 长短画线）、液体（水平短画线）、砖墙/石材（砌体）、铸铁（网状）等材料——它们有 GB 标准依据的专用渲染与既有回归断言。其余材料接入 PAT 仅需在 `patNameForMaterial` 增加一行。

---

## 4. 具体实施方法与步骤

### 4.1 新增文件 `src/core/HatchLibrary.h`

```cpp
struct PatLine {
    double angleDeg = 0.0;
    double x0 = 0.0, y0 = 0.0;   // 基准点
    double dx = 0.0, dy = 0.0;   // 相邻线偏移（局部坐标）
    std::vector<double> dashes;  // 笔段（正=实线/落笔，负=间隔/抬笔），从实线段起
};

struct PatPattern {
    QString name;                // 图案内部名（* 后逗号前）
    QString description;         // 描述（逗号后）
    std::vector<PatLine> lines;  // 图案的全部线定义
};

class HatchPatternLibrary {
public:
    static HatchPatternLibrary& instance();              // 懒加载 exe/hatch/
    void loadFromDir(const QString& dir);                // 测试可注入临时目录
    const PatPattern* find(const QString& name) const;   // 内部名/文件名，大小写不敏感
    static std::vector<PatPattern> parsePatText(const QString& text);  // 已公开供测试/复用
    bool empty() const;  int size() const;  QStringList names() const;
    QString loadedDir() const;
private:
    HatchPatternLibrary() = default;
    void ensureLoaded();
    std::vector<PatPattern> patterns_;
    std::map<QString, int> index_;   // 小写键 -> patterns_ 下标
    QString loadedDir_;
    bool loaded_ = false;
};
```

### 4.2 新增文件 `src/core/HatchLibrary.cpp`

- `parsePatText`：见 §3.2。
- `loadFromDir(dir)`：清空 → 枚举 `*.pat`/`*.PAT` → 逐个读文本 → 解析 → 建索引（内部名 `name.toLower()`；每个文件的**首个图案**额外以**文件名去扩展名** `f.section('.',0,0).toLower()` 建键）。
- `ensureLoaded()`：首次调用时加载 `QCoreApplication::applicationDirPath() + "/hatch"`。
- `instance()`：函数内 `static` 局部对象，线程安全初始化 + 懒加载。

### 4.3 修改 `src/core/Section.h`

- 顶部 `#include "HatchLibrary.h"`。
- `HatchPattern` 新增字段：`QString patName;`
- 声明生成器：`std::vector<geo::Seg> generateHatchLinesFromPat(const PatPattern& pat, const SectionRegion& region);`

### 4.4 修改 `src/core/Section.cpp`

- 新增 `static QString patNameForMaterial(SectionMaterial)`。
- `hatchPatternFor` 末尾置位 `pat.patName`。
- `generateHatchLines` 开头 PAT 分流。
- 新增 `generateHatchLinesFromPat` 函数体（约 70 行，见 §3.3）。

### 4.5 修改 `CMakeLists.txt`

在 `CORE_SOURCES` 中 `src/core/Section.cpp` 之后插入：

```cmake
    src/core/Section.cpp
    src/core/HatchLibrary.cpp
    src/core/Block.cpp
```

> 由于 `CORE_SOURCES` 被主程序与全部 26 个测试目标共用，一次插入即对所有目标生效。

### 4.6 新增部署源目录 `hatch/`

从既有 `dist/hatch/`（CADforum AutoCAD 图案库）抽取 **18 个 `.pat`** 到仓库根 `hatch/`，纳入版本控制：

```
Glass.pat  HVEGE100.pat  Hydroisol.pat  Landscape.pat  luxfer.pat
pavage_carre_tronque.pat  pavage_du_caire_4x3.pat  pavage_du_caire_6x4.pat
pavage_grand_rhombitrihexagonal.pat  pavage_hexagonal_adouci.pat
pavage_hexagonal_tronque.pat  pavage_petit_rhombitrihexagonal.pat
pavage_trihexagonal.pat  Roof.pat  Stone.pat  Stone2.pat  Wood.pat  zelezobeton.pat
```

### 4.7 修改 `build.sh`：同步可复现化

```bash
sync_hatch() {
    local dst="$1"; mkdir -p "$dst"
    for f in "$ROOT"/hatch/*.pat; do
        [ -e "$f" ] || continue
        cp "$f" "$dst/"
    done
}
sync_hatch "$ROOT/build/hatch"     # 开发期 exe 同级
...
sync_hatch "$ROOT/dist/hatch"      # 发布打包
```

### 4.8 新增回归测试 `tests/section_test.cpp` §10d

- `parsePatText`：头/描述/多定义行/笔段/去零位。
- `generateHatchLinesFromPat`：非空、水平、间距≈`dy`、笔段切短。
- 材料映射：`Wood → "wood"`。
- `loadFromDir` + `find`：内部名/文件名/大小写不敏感。
- **部署路径**：以 `argv[0]` 取 exe 目录，加载其 `hatch/wood.pat` 并生成木纹线。
- `main` 签名改为 `int main(int argc, char** argv)` 以获取 exe 路径。

---

## 5. 关键假设与推导逻辑

### 5.1 假设 A：`dx/dy` 的模长即相邻平行线间距

**推导**：PAT 定义中平行线由 `(x0 + k·dx, y0 + k·dy)` 生成（k 为整数）。相邻线在图案空间的位移向量为 `(dx, dy)`。线上的方向向量为 `(1, 0)`（图案空间中线是水平的）。因此相邻线的**垂直距离** = 位移向量在**法向** `(0, 1)` 上的投影 = `|dy|`；而 `dx` 是**沿线方向**的错位（相位/错缝）。

**验证**：`wood.pat` 中 `dx=15, dy=5` → 垂直间距 5mm；测试断言「相邻线间距 ≈ dy = 5」通过。✅

### 5.2 假设 B：旋转变换保持距离不变

**推导**：`toWorld` 是正交旋转矩阵。正交变换保距，故 `|dy|`（局部）在旋转后仍是世界系中的垂直间距。故可直接用局部 `|dy|` 作为间距，无需再做世界系反算。

**推论**：角度族方向 = `angle`；间距 = `|dy|`。二者与旋转无关，可独立使用。

### 5.3 假设 C：笔段序列从「首个实线段」开始

**推导**：PAT 规范规定笔段序列以**正数（落笔）**起始，正负交替。若序列首项为负（非法数据），本实现仍按「正=落笔」语义处理（负值段天然不 emit），因此不会产生错误实线，仅表现为起始相位偏移。

**验证**：`HOLZ-2` 有效笔段 `[5.25, -66.25, 6.5, -47]` 首项为正 → 从实段起。✅

### 5.4 假设 D：周期 = Σ|dashes[i]|

**推导**：沿一条线重复排布笔段时，一个完整周期的长度是所有笔段长度（含实段与间隔）的绝对值之和。`phase = (xStart - xbase) mod period` 用于确定扫描起点的段内位置，使图案在整条线上无缝循环。

**边界**：`period ≤ 1e-9`（无笔段或全为 0）→ 视为整条实线（连续剖面线）。

### 5.5 假设 E：生成器只需铺满包围盒，裁剪交由 painter

**依据**：与既有砖墙/石材砌体图案的既有契约一致（见 `MEMORY.md`「砌体图案契约 = 生成器铺满包围盒、painter 裁剪」）。`GraphicsView::paintHatch` 已用 `setClipPath`（path 含 holes）裁剪到区域。

**收益**：多边形的凸/凹、内孔的 even-odd 掏空均自动正确，生成器无需感知区域形状细节，仅需包围盒。

### 5.6 假设 F：`dy ≈ 0` 的退化情形

**推导**：若 `dy = 0`（如某些纯水平/纯竖直族），则 `yp = y0 + k·0 = y0` 对所有 k 相同，铺线公式无法产生多条线。本实现特判 `|dy| < 1e-9 → k ∈ {0}`，得到单条线——语义上等价于「该族只有一条基准线」。

### 5.7 假设 G：外部 PAT 与内置渲染的选择边界

**推导**：材料的 PAT 映射是**显式白名单**（`patNameForMaterial`）。设计上**默认不接管**已有标准符号材料，理由是：
1. GB 标准符号（玻璃/液体/砖墙等）有明确的规范依据，不应被外部文件覆盖；
2. 既有回归测试（`section_test` §10/§10b/§10c）对这些材料的几何有精确断言，覆盖会导致连锁失败；
3. 「PAT 补充表1 未穷举纹样」的定位要求映射是**增量式**的。

---

## 6. 部署与可复现性设计

### 6.1 运行时路径

```
<exe目录>/
├── SketchCAD.exe
└── hatch/
    ├── Wood.pat
    ├── Stone.pat
    └── ...  (18 个 .pat)
```

加载逻辑：`QCoreApplication::applicationDirPath() + "/hatch"`，与 exe 位置无关，**绿色部署**即可。

### 6.2 为什么源图案放仓库根 `hatch/` 而非 `dist/hatch/`

`.gitignore` 忽略 `dist/`，故 `dist/hatch/` **不受版本控制**，全新 clone 会丢失图案。因此：

- **唯一真源** = 仓库根 `hatch/`（纳入版本控制）；
- `build.sh` 把 `hatch/*.pat` 同步到 `build/hatch`（开发）与 `dist/hatch`（发布）。

这样「构建产物目录」与「版本控制的源」职责分离，保证任何一次干净构建都能得到完整图案库。

### 6.3 加载器容错

| 情形 | 行为 |
|---|---|
| `hatch/` 目录不存在 | `loadFromDir` 标记已加载、图案集为空；不影响其他功能 |
| 某 `.pat` 打不开 | 跳过该文件，继续处理其余 |
| 定义行字段 < 5 | 跳过该行 |
| PAT 名未命中（`find` 返回空） | `generateHatchLines` 回退内置参数化，**不退化成空白** |
| 文件含多个 `*NAME` | 全部收录，各自建索引 |

---

## 7. 验证与结论

### 7.1 构建与测试结果

| 目标 | 结果 |
|---|---|
| `ninja -j 1 SectionTest` | 构建通过 |
| `SectionTest.exe` | **All PASS**（含 §10d 共 15 项新断言） |
| `ninja -j 1 SketchSelfTest` + 运行（工程根） | **exit 0** |
| `ModifyMenuTest.exe` | **exit 0** |
| `ninja -j 1 SketchCAD` | 构建通过 |

§10d 关键断言（全部 PASS）：

- `parsePatText`：单文件单图案 / 图案名 / 描述 / 两条定义线 / 角度·dx·dy / 笔段序列 / 行尾零位丢弃
- `generateHatchLinesFromPat`：生成非空 / 角度 0 → 水平 / 相邻间距 ≈ `dy=5` / 笔段将长线切成短实段
- `Wood → "wood"` 映射
- `loadFromDir` 按内部名 `TPAT` / 大小写不敏感 / 按文件名查找 / 加载后生成非空
- **部署路径**：`exe/hatch/wood.pat` 加载并按 `"wood"` 查找 / 真实 `wood.pat` 生成木纹线 / `HOLZ-2` 角度 0 → 水平 / 笔段切短

### 7.2 结论

1. **功能达成**：exe 同级 `hatch/` 下的 `.pat` 被自动加载，木材剖面填充改用 `wood.pat` 的木纹笔段，符合 R1–R3。
2. **无回归**：既有 19 种材料符号与全部测试保持通过，符合 R4。
3. **扩展友好**：`patNameForMaterial` 是唯一扩展点，一行即可为新材料绑定 PAT。
4. **部署健壮**：图案源纳入版本控制，`build.sh` 自动同步，PAT 缺失时安全回退，不产生空白填充。
5. **架构一致**：复用 `geo::hatchLines` 的旋转约定与砌体图案的「生成器铺满、painter 裁剪」契约，与既有剖面引擎同构。

### 7.3 后续可选项（未实施）

- **用户自选 PAT**：`SectionDialog` 增加图案下拉框（库已可枚举 `names()`），把用户选择写入 `HatchPattern.patName` 或图元字段。
- **多图案选择**：当前按文件名键取「首个图案」（如 `wood → HOLZ-2`）；可扩展为显式 `文件:图案` 选择。
- **线宽/颜色**：PAT 不带线宽信息，沿用剖面细线（0.6）语义即可。

---

## 8. 附录：关键代码结构与契约

### 8.1 契约速查

| 契约 | 内容 |
|---|---|
| 笔段符号 | 正 = 落笔实段；负 = 抬笔间隔；从实段起 |
| 零值处理 | `|dash| ≤ 1e-6` 丢弃（行尾占位） |
| 间距语义 | 相邻平行线垂直间距 = `|dy|`；沿线错位 = `dx` |
| 旋转约定 | `toLocal` = 旋转 `-angle`；`toWorld` = 旋转 `+angle`（与 `geo::hatchLines` 一致） |
| 裁剪契约 | 生成器铺满包围盒（超长线段），由 `paintHatch::setClipPath` 裁到区域（含内孔） |
| 回退契约 | `patName` 未命中 → 回退内置参数化，**不退化成空白** |
| 加载路径 | `QCoreApplication::applicationDirPath() + "/hatch"` |
| 索引键 | `name.toLower()`；每文件首图案额外以 `filename-base.toLower()` |

### 8.2 涉及文件清单

| 文件 | 变更 |
|---|---|
| `src/core/HatchLibrary.h` | **新增**：`PatLine` / `PatPattern` / `HatchPatternLibrary` |
| `src/core/HatchLibrary.cpp` | **新增**：解析 + 加载 + 索引单例 |
| `src/core/Section.h` | include；`HatchPattern::patName`；`generateHatchLinesFromPat` 声明 |
| `src/core/Section.cpp` | `patNameForMaterial`；`hatchPatternFor` 置位；PAT 分流；生成器实现 |
| `CMakeLists.txt` | `CORE_SOURCES` 加入 `HatchLibrary.cpp` |
| `build.sh` | `sync_hatch` 同步到 `build/hatch` 与 `dist/hatch` |
| `hatch/`（仓库根） | **新增**：18 个 `.pat` 图案源（版本控制） |
| `tests/section_test.cpp` | §10d 回归测试；`main(argc, argv)` |
| `.workbuddy/memory/` | 项目记忆与当日工作记录更新 |

---

*文档结束*
