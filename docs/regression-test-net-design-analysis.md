# 分层测试网（回归保护升级）设计分析

> 版本：v1.0（2026-10-02）
> 范围：`tests/SketchSelfTest.cpp` 扩层至 300 CHECK + `tests/golden/` 黄金样本 + 存档兼容断言
> 结论先行：**300/0 全绿，运行 2.73s，变异验证精准命中，`build.sh test` EXIT=0，零警告**

---

## 1. 背景与动机

2026-09-28 清理 `tests/` 中 38 个零散自测 exe 后，回归保护退化为单 exe 内 56 条 CHECK。彼时仅覆盖约束/捕捉主线，Geometry 纯函数、Modify、Section、Inquiry、GripOps、ShapeSelect、MacroScript、AliasTable、CuiManager、存档兼容均无断言守护。随 DXF 原生导出（#395–#399）、图层/性能大改提上日程，单层 56 CHECK 的保护网无法在重构时提供有效回归信号，需要重建分层测试网。

## 2. 设计目标与约束

| 目标 | 落实方式 |
|---|---|
| ≥200 CHECK、12 组每组 ≥10 | 最终 300 CHECK，11 个新增组最小组 11 条 |
| 不恢复 38 exe 形态 | 保持单 `SketchSelfTest` 结构，组内函数化 |
| 不引入测试框架依赖 | 纯 `CHECK(cond, msg)` 宏 + 计数器，无外部库 |
| 断言禁止恒真 | 每条断言对应可复核的代码语义（见 §8 修正记录反例） |
| 全部无头可跑 | `QT_QPA_PLATFORM=offscreen` 兼容，实测 2.73s |
| 不改被测代码语义 | 全程只读实证源码，仅测试文件与样本变更 |

## 3. 分层架构

```
SketchSelfTest（单 exe，300 CHECK）
├── 旧主线（155 CHECK，不动）          ← 约束/捕捉/DXF 导入/mirror/打印等既有聚合断言
└── 新增分层（145 CHECK，11 组）
    ├── 第一层 core 纯函数层（53）     Geometry 13 / Modify 14 / Section 12 / Inquiry 14
    ├── 第二层 存档兼容 + 黄金样本（31） archiveCompat 20 / goldenSamples 11
    └── 第三层 模块深度直测（61）      GripOps 11 / ShapeSelect 12 / MacroScript 13
                                       AliasTable 13 / CuiManager 12
```

三层职责：

1. **core 纯函数层**：`geo::/mod::/sec::/inq::` 命名空间纯函数，无 GUI 依赖，守护几何算法与修改变换的数学正确性（segLength/segAngleDeg、rotate/mirror/scale、半边拓扑与剖面、dist/area/massprop/id）。
2. **存档兼容 + 黄金样本层**：V1→V4 逐版本 JSON 容缺读入不丢数据（§6）；`tests/golden/*.dxf` 固定样本做 token 计数（G1）+ 解析差分（G2）（§7）。
3. **模块深度直测层**：GripOps 夹点五模式、ShapeSelect 三窗选模式、MacroRecorder/ScriptRunner 捕获回放、AliasTable 别名展开与防环、CuiManager XML round-trip。

## 4. 断言设计模式

**直计 CHECK 的 void 函数**（新增组统一形态）：

```cpp
static void t_geometryCore() {
    CHECK(std::fabs(geo::segLength(Vec3(0,0,0), Vec3(3,4,0)) - 5.0) < 1e-9 && ...,
          "geo::segLength=5 / segAngleDeg=atan2(4,3)");
    // 每条 CHECK 直接计入全局 g_pass/g_fail
}
// main: t_geometryCore();  ← 直接调用，不包外层 CHECK
```

与旧聚合式（`CHECK(t_dxfImport(), "...")` 内部自计数）并存：旧函数保持不动以保护既有断言语义，新函数直计使 FAIL 定位精确到条。运行输出按三个分组标题分节，`build.sh test` 日志直接可读。

## 5. 存档兼容层设计（t_archiveCompat，20 CHECK）

组装完整模型后逐版本**删键降级**再读入：

| 版本 | 键集 | 关键断言 |
|---|---|---|
| V4 | elements/layers/blocks/textStyles/constraints/params/scaleRatio | 全表计数逐项核对 |
| V3 | −params | 参数表空，其余不丢 |
| V2 | −constraints/−textStyles/−scaleRatio | 三表空；**textStyleCount==1（预置 Standard 保留）** |
| V1 | 仅 elements | 块表/约束表空；textStyleCount==1 |

**实证发现（决定断言写法）**：

- `SketchModel` 构造函数预置 `Layer("0")` + `TextStyle("Standard")`（SketchModel.cpp:18-23）；
- `setXxxFromJson` 只在**键存在时**调用 → 缺键 = 不触发 clear = 预置保留；**空数组才会清空**；
- `textStylesToJson` 全量序列化含预置 Standard → V3/V4 重建后 `textStyleCount==2`（Standard+标注）；
- `ConstraintSolver::addConstraint` 的 Horizontal/Vertical 要求**被动方 idB 为直线**（ConstraintSolver.cpp:331-336）——约束施加于圆会被拒绝返回中文错误串。

这些语义此前无文档沉淀，本次以断言形式固化：若未来构造预置或容缺策略变化，对应 CHECK 立即报警。

## 6. 黄金样本层设计（t_goldenSamples，11 CHECK）

`tests/golden/` 两个固定样本，`locateGoldenDir()` 五候选路径定位（构建目录、源码目录、CWD 各层级兼容）：

- **golden_min.dxf**（版本锚定）：4 基元 LINE/CIRCLE/ARC/TEXT。
  - G1 token 计数：`dxfCountToken` 各实体恰好 1 次——导出器结构性变化（漏段/重段）立即失败；
  - G2 解析差分：`fromDxf` 返回 4 实体 0 未知，几何逐项 ≤1e-6（线端点/圆径/弧起止角/文本内容）。
- **golden_mix.dxf**（多类 + 块表）：解析含块 BOLT、0 未知实体；**round-trip 差分**——导入 → 原生导出 → 再导入 → 逐点比对。R12 无 ELLIPSE/SPLINE 原生实体、XLINE 降级长线段，三类 round-trip 中类型必变（→Polyline/Line），仅对类型可保持子集断言，差异在样本注释中显式声明。

黄金样本的价值：DXF 导入/导出任何重构，只要破坏样本的确定性解析结果即失败，无需理解实现细节。

## 7. 修正记录（两轮 12 处 FAIL 全部收口）

**分类一：测试期望错 → 对齐实现语义（5 处）**

| FAIL | 错误期望 | 正确语义（实证） |
|---|---|---|
| V1/V2 textStyleCount==0 | 缺键清空 | 缺键 = 不触发 setXxxFromJson = 预置 Standard 保留 ==1 |
| V3/V4 textStyleCount==1 | 全量 1 条 | textStylesToJson 含预置 Standard+标注 ==2 |
| grip Scale 拒绝 | cursor==refCursor → f=1 合法 | `cursor==base → f=0`，`!(f>0)||!isfinite` reject |
| grip Stretch 位移 | 控制点迁移 delta | `setElementHandle` **直设为 cursor**，断言 (0,4) |
| 约束添加失败 | Horizontal 施加于圆 | 被动方 idB 须直线，第二元素改水平线（10,0)-(5,0) |

**分类二：测试样本构造缺陷（1 处）**

- shape 圆 Crossing：原 winEdge=(4,4)~(8,8) 离圆心 (0,0) 最近角距 √32≈5.66 > r=5，**窗口与圆完全不相交**，Crossing=false 本来就正确。修正 winPart=(3,−1)(7,−1)(7,3)(3,3)（θ=0° 采样点 (5,0) 在内、θ=90° (0,5) 在外 → 真部分相交）。同时将三合一 CHECK 拆为三条独立断言，定位粒度从"组"精确到"条"。

**分类三：测试代码 UB（1 处，最重要的发现）**

- `inquiry::idPoint(Vec3(5,0,0), {c})` 传**临时 vector** → `IdResult.nearest = &e` 指向临时容器内部 → 语句结束析构 → 下行 CHECK 解引用**悬垂指针** → **偶发失败**（结果随堆残留漂移，299/1 与 300/0 交替出现）。
- 修复：具名容器 `const std::vector<Element> elems = {c};` 延长生命周期。修复后连续 3 次运行 300/0 稳定。
- **教训**：返回指针/引用指向实参容器的 API（Inquiry.cpp:455-465 纯遍历模式），测试实参容器必须具名。此类 UB 若漏网，将为回归网埋下"随机误报"，直接摧毁测试公信力——"每个 CHECK 必须可复核"条款的深层意义即在于此。

两轮合计 12 处 FAIL，**未发现产品代码 bug**（符合"不改被测代码语义"约束），全部为测试侧期望/构造/UB 问题。

## 8. 变异验证（Mutation Verification）

验收要求故意改坏 1 处源码，对应 CHECK 必须失败：

1. **注入**：`Geometry.cpp:445` `segLength` 返回值 `std::hypot(...) * 2.0`（标记 `/* MUTANT-VERIFY */`）；
2. **结果**：`通过 299 / 失败 1`，唯一 FAIL 为 `geo::segLength=5 / segAngleDeg=atan2(4,3) [SketchSelfTest.cpp:1811]`——**精准命中目标断言，无误伤**；
3. **还原**：删除变异 → 重建 → 复跑 `通过 300 / 失败 0`。

## 9. 验收数据对照表

| 验收条款 | 要求 | 实测 | 结论 |
|---|---|---|---|
| CHECK 总数 | ≥200 | **300**（+145 新增 / 155 旧） | ✅ |
| 每组 CHECK | 12 组各 ≥10 | 11 新增组最小 **11**（t_goldenSamples/t_gripopsDepth） | ✅ |
| 全绿 | 0 FAIL | **300/0**（三连稳定 + 变异还原后复跑） | ✅ |
| 运行时长 | ≤30s | **2.73s**（offscreen，`time` 实测） | ✅ |
| 变异验证 | 对应 CHECK 失败 | 299/1 精准命中 segLength 断言 | ✅ |
| 零警告 | -Wall -Wextra | SketchCAD 主目标重建 warning/error 计数 **0** | ✅ |
| build.sh test | EXIT=0 | **EXIT=0**，日志三分组清晰 | ✅ |
| 无头运行 | offscreen 兼容 | QT_QPA_PLATFORM=offscreen 正常 | ✅ |

## 10. 经验沉淀

1. **断言前先实证**：12 处 FAIL 中 5 处源于"想当然的期望"（预置样式、缺键语义、Scale 因子、Stretch 语义、约束类型校验）。对不确定的 API 语义，写断言前 grep 源码实证，一次到位。
2. **几何样本必须数值验证**：窗口/围栏样本先心算边界距离（√32 vs r=5），避免"看起来相交实际不相交"的样本。
3. **悬垂指针模式**：API 返回指向实参容器的指针/引用时，实参必须具名且生命周期覆盖全部断言。
4. **三合一 CHECK 拆条**：合并断言定位粒度差，排查成本高于拆条成本。
5. **直计 CHECK 模式**优于聚合式：新组统一直计，FAIL 消息自带行号。

## 11. 后续方向

- 存档兼容可加**向前兼容**样本（未来 V5 读 V4）；
- 黄金样本可扩充性能锚（大样本导入计时上限断言）；
- 变异验证可脚本化（自动注入 N 个变异体统计捕获率，当前人工单点）。
