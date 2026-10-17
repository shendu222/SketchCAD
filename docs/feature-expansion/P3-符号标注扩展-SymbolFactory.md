# P3 — 符号标注扩展：基准 / 焊接 / 锥斜度 / 中心孔 / 标高 / 旋转

> 对应《功能扩充方案_参考CAD对照.md》第 3.3 节提示词 **P3**。
> 模块：`src/core/SymbolFactory.{h,cpp}`（新建纯函数生成器）。
> 自测：`tests/SketchSelfTest.cpp :: t_symbolExt()`，经 `main()` 的 `CHECK(t_symbolExt(), …)` 注册。
> 验收：`SketchSelfTest` 全量 **通过 2609 / 失败 0**，`rc=0`，`-Wall -Wextra` 零警告。

---

## 1. 修改背景

参考 CAD 机械 Ch8 提供丰富的「符号标注」能力：基准符号（GD&T 基准 A/B/C）、焊接符号（角焊 / V 形 / 方框）、锥度斜度、中心孔、标高、旋转方向等。SketchCAD 此前仅有 `Gdt`（形位公差框格）与 `SectionCutSymbol`（剖切符号），缺少上述通用工程符号的批量生成器；用户每画一个都要手工拼线 + 文字。

本次依据 P3 提示词新建 `SymbolFactory::mkSymbol(name, anchor, params)`，以**单一路由入口**覆盖 6 类符号，作为纯几何生成器接入，调用方（命令层 / 脚本）负责 `append` 进模型与定位。

**与提示词的偏差（已决断并记录）**：
- 提示词引用 `SymbolFactory::mkSymbol` 但仓库此前**无该文件**；本项新建 `src/core/SymbolFactory.{h,cpp}`，命名空间 `SymbolFactory` 以提供 `SymbolFactory::mkSymbol` 同名入口。
- 提示词提到「输出 Line+Text 或 BlockRef」。为保持纯生成器、`BlockDef` 零耦合，本项统一输出 **Line / Text / Circle 组合**（BlockRef 化可作为后续命令层集成步骤），满足提示词「或」的可选项。
- 各符号尺寸（三角形高宽、箭头半张角 15°、圆缺省半径等）均为 **GB 简化示意定值**，由本模块集中提供，未擅自改写国标数值语义（满足 G3：国标数值由人工提供）。

---

## 2. 思考过程

1. **范式对齐**：完全复用 `SectionCutSymbol.cpp` 的 `mkLine/mkText` 匿名命名空间助手写法与 `Element` 构造方式（类型 / `pts` / `lineStyle=Solid` / `lineWidthAuto=false` / `color=深灰` / `name`），保证视觉与既有符号一致。
2. **单一调度入口**：`mkSymbol(name, anchor, params)` 以 `name` 字符串分支到 6 个 builder；未知 `name` → 返回空 `vector`（退化）。`params` 为 `QVariantMap`，缺键走默认值，避免调用方必须构造完整参数。
3. **几何构成（全部 Line/Text/Circle 组合，无自造算法）**：
   - 基准 `datum`：上尖等腰三角形 + 基准字母（GB/T 1182 基准特征符号简化形）。
   - 焊接 `weld`：水平参考线 + 指向下方箭头 + 接头形状（fillet 正三角 / v 倒 V / square 方框），覆盖提示词「焊接三版」。
   - 锥度 `taper`：直角斜三角形（水平底 + 竖边 + 斜边）+ 比值文本（如 `1:50`）。
   - 中心孔 `centerhole`：小圆 + 右侧引出线 + 可选旁注。
   - 标高 `elevation`：上尖等边三角形 + 顶部水平线 + 标高值（如 `±0.000` / `+5.000`）。
   - 旋转 `rotation`：圆 + 顶部逆时针箭头 + 角度文本（如 `360°` / `180°`）。
4. **可选整体旋转**：`params["rot"]`（度）绕 `anchor` 旋转全部控制点（圆圆心不变），便于命令层按场景定向。
5. **退化处理**：未知 `name` → 空；其余按默认参数仍可生成，绝不抛异常。

---

## 3. 核心算法与设计思路

### 3.1 入口与参数

```cpp
namespace SymbolFactory {
std::vector<Element> mkSymbol(const QString& name, const Vec3& anchor, const QVariantMap& params);
}
```

| name | 含义 | 关键 params（缺省） |
|---|---|---|
| `"datum"` | 基准符号 | `letter`("A") |
| `"weld"` | 焊接符号 | `type`("fillet" / "v" / "square") |
| `"taper"` | 锥度/斜度 | `ratio`("1:50") |
| `"centerhole"` | 中心孔 | `r`(2.5) / `label`("") |
| `"elevation"` | 标高 | `value`("±0.000") |
| `"rotation"` | 旋转 | `r`(6.0) / `angle`("360°") |
| 任意 | 公共 | `layer`("标注") / `rot`(0°) |

### 3.2 关键几何助手（匿名命名空间）

- `mkSymLine / mkSymText / mkSymCircle`：统一构造，集中设置线型/线宽/颜色/name，杜绝散落赋值。
- `addArrowHead(tip, dir, wing, …)`：在 `tip` 沿 `dir` 反向画 V 形箭头两翼（半张角 15°，与 `SectionCutSymbol` 一致）。
- `rotAround(p, c, deg)`：绕 `c` 旋转点 `p`（旋转矩阵，无外部依赖），供 `params["rot"]` 使用。

### 3.3 为什么是「纯组合」而非 BlockRef

提示词的「或 BlockRef」是集成层面的选项；本模块定位为**几何生成器**，与 `SurfaceRoughness` / `SectionCutSymbol` 同层。命令层未来可把 `mkSymbol` 结果包成 `BlockDef` 以提升复用与属性绑定，但这属于 P3 之外的「命令层集成」步骤（见第 6 节），不在此纯几何模块内，避免 `Block.h` 耦合与序列化复杂度。

---

## 4. 关键实现要点

1. **零警告**：`QVariantMap` 经 `<QVariant>` 引入；所有 builder 的入参（`anchor`/`p`/`layer`）均被使用；`rot!=0` 分支对 `Circle` 仅旋转 `pts`（圆心不变），逻辑显式。
2. **默认值兜底**：`p.value(key, default)` 统一取值，调用方即使只传 `name`+`anchor` 也能得到合理符号。
3. **name 分支顺序**：先 6 个已知分支，末位 `else return {}`——确保未知符号名为退化空，符合「空/未知 → 空 vector」验收。
4. **可测试性**：每个 builder 输出均为可断言的 `Line/Text/Circle` 集合（类型 + 文本包含子串），自测据此验证锚点语义与文字内容。

---

## 5. 自测覆盖（`t_symbolExt`）

- 6 类符号均 `非空`；`datum` 含字母 `B`、`taper` 含 `1:20`、`centerhole` 含圆 + 旁注 `A型`、`elevation` 含 `±5.000`、`rotation` 含圆 + `180°`、`weld` 三版均含线。
- 退化：未知 `name` → 空 `vector`。
- 整体旋转 `rot=30°` 不崩溃且仍非空。
- 全部经 `CHECK` 注册于 `main()`，纳入全量 EXIT=0 验收（自测计数 +17）。

---

## 6. 后续可演进（非本次范围）

- **命令层接入**：新增 `symbol` 命令，交互输入 `name` + 拾取锚点 + 弹参（`letter/ratio/value/...`）→ 调 `mkSymbol` 并 `append`（一次操作一次 `pushMemento`）。
- **BlockRef 化**：将 `mkSymbol` 结果纳入 `BlockDef` 参数化块（参考 `Block.h` 的 `block::makeRef`），实现「一次定义、多次引用 + 属性联动」。
- **GB 数值表**：当前尺寸为简化定值；若需严格 GB/T 1182 / GB/T 324 比例，可将尺寸常量抽到独立头（如 `SymbolGbSizes.h`）由人工维护（对应 G3 门禁）。
- 与 P1（`CenterLineGen`）共享 `mkSymLine` 式助手，可下沉到公共 `SymbolTools` 区，避免重复。
