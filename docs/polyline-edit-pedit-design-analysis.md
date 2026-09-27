# 多段线 PEDIT 式交互编辑 — 设计与实现分析

> 需求：基于 `PolylineEditTool` 实现交互式多段线编辑，**不调用命令行 PEDIT**，
> 完整支持 拟合（Fit）/ 样条化（Spline）/ 设置宽度（Width）/ 闭合（Close）/
> 合并（Join）/ 反转（Reverse）六项能力，行为与 PEDIT 命令行预期一致。
>
> 状态：已实现并通过 40 项单元测试（`PolylineEditOpsTest`，ALL PASS）。

---

## 1. 与 PEDIT 命令行的语义对应

| PEDIT 选项 | 本实现 | 语义 |
|---|---|---|
| `F` (Fit) | `polyops::fit` | 平滑曲线**严格通过**所有顶点（Catmull-Rom 插值） |
| `S` (Spline) | `polyops::spline` | 顶点作为三次 B 样条**控制点**的逼近曲线（端点严格插值，中间顶点一般不过） |
| `W` (Width) | `polyops::setWidth` | 整条折线统一线宽（`lineWidthAuto=false` + `lineWidth=w`） |
| `C` (Close) | `polyops::setClosed` | 闭合（新增一段直线闭合段）；再执行一次为打开（切换语义） |
| `J` (Join) | `polyops::join` | 把端点重合的 直线/圆弧/折线 拼入目标折线并删除来源 |
| —（反转顶点） | `polyops::reverse` | `pts` 与 `arcFlags` 同时逆序 |

## 2. 数据模型决策（零迁移成本）

复用 `Element` 既有字段，**不改序列化格式、旧档字节级兼容**：

- **Fit/Spline**：复用 `splineSubType`（Polyline 类型此前未使用该字段，与 Spline
  类型的 1~6 编码互不冲突，因为渲染按 `ElementType` 分派到不同 case）：
  - `0` = 普通折线（按 `arcFlags` 直线/弧段渲染，现状默认）
  - `1` = Fit（`geo::samplePolylineFit`）
  - `2` = Spline（`geo::samplePolylineSpline`）
- **Width**：复用 `lineWidth`/`lineWidthAuto`/`effectiveLineWidth()` 全局线宽机制，
  对应 PEDIT `W` 的整条统一线宽（不含逐顶点起终宽，模型当前无该字段）。
- **Close**：复用 `closed`；闭合时向 `arcFlags` 追加一个 `false`（新闭合段为直线），
  打开时截断回 `n-1`，保证 `arcFlags.size()` 与段数恒一致。
- **Reverse**：`pts` 与 `arcFlags` 同时 `std::reverse`；闭合折线循环逆序后
  段 k 恰好对应原段 (n-2-k)，几何与段类型保持一致。

## 3. 分层架构

```
Geometry（纯几何，UI/CLI/测试共用）
  ├─ samplePolylineFit(pts, closed, perSeg)    Catmull-Rom（开放复用 sampleSpline；闭合走循环切向）
  └─ samplePolylineSpline(pts, closed, perSeg) 均匀三次 B 样条
       开放：幻影端点 C₋₁=2P0−P1、C_n=2P₍n₋₁₎−P₍n₋₂₎ → 曲线严格过首尾控制点
       闭合：循环索引取窗，无缝成环
PolylineOps（纯模型操作，src/core/PolylineOps.{h,cpp}）
  └─ setClosed / reverse / setWidth / fit / spline / join —— 直接驱动 SketchModel
PolylineEditTool（交互，单类多 id 派发，沿用既有 9 工具模式）
  └─ polyline_fit / polyline_spline / polyline_width / polyline_close / polyline_join / polyline_reverse
GraphicsView::buildPath（渲染）+ Geometry::distanceToElement / flatten（命中/离散）
  └─ Polyline 分支按 splineSubType 分派三种采样，渲染-命中-DXF 导出同源
```

## 4. Join 的端点匹配算法

目标折线（必须开放）取首尾两端点；来源（Line/Arc/开放 Polyline）经
`endpointsOf` 归一化为「有序点列 + 两端点」（圆弧按扫掠角采样 24 段近似为折线接入）。
按容差（默认 `1e-3` mm）依次尝试四种拼接：

| 命中 | 共享点 | 接入方式 |
|---|---|---|
| tEnd ≈ sA | 目标末 = 来源首 | 尾接：`sPts[1..]` 追加 |
| tEnd ≈ sB | 目标末 = 来源末 | 尾接：`reverse(sPts)[1..]` 追加 |
| tStart ≈ sA | 目标首 = 来源首 | 前插：`sPts[1..]` 插入头部 |
| tStart ≈ sB | 目标首 = 来源末 | 前插：`reverse(sPts)[1..]` 插入头部 |

共享点不重复入列；新增段一律 `arcFlags=false`（直线）；成功后原地
`updateElement(target)` 并 `removeElement(source)`。失败（不重合/闭合目标/闭合来源）
仅 log 并保留双方，交互层可继续重试。

## 5. 交互语义（PolylineEditTool）

- **Fit / Spline / Reverse / Close**：单击折线即完成（Close 为切换：开↔闭）。
- **Width**：单击折线后弹出 `QInputDialog::getDouble` 输入宽度（取消不动模型）。
- **Join**：两步——先选目标折线（存 `firstId_`），再连续点选相邻直线/圆弧/折线
  （每次成功后目标保持，可链式合并多段），右键结束。
- 全部操作走 `polyops::*` 纯模型函数，工具层零几何逻辑（与既有 9 工具的
  「updateElement + log」惯用法一致，可单测、可复用）。

## 6. 渲染与命中一致性

`buildPath`（Polyline 分支）在 `splineSubType==1/2` 时按 16 段/节点采样曲线绘制；
`geo::distanceToElement` 与 `geo::flatten`（DXF 导出离散）同步按曲线采样，
保证「所见即所选、所导即所绘」三处同源。

## 7. 注册与入口

- `ToolManager.cpp`：注册 6 个 `PolylineEditTool` 实例（追加在既有 9 个之后）。
- `ToolRegistry.cpp`：`ToolCategory::PolylineEdit` + 中文名称/描述。
- `MainWindow::buildPolylineMenu`：折线菜单新增第 4 组
  （拟合 / 样条化 / 设宽 / 闭合打开 / 合并 / 反转）。
- `CMakeLists.txt`：`CORE_SOURCES` 增 `src/core/PolylineOps.cpp`；
  新增测试目标 `PolylineEditOpsTest`。

## 8. 验证结果

`tests/polyline_edit_ops_test.cpp`（40 断言，ALL PASS，exit 0）：

1. Close/打开：closed 翻转、arcFlags 增减一致、幂等、非折线 id 拒绝；
2. Reverse：开放/闭合折线顶点与段类型标志同步逆序；
3. Width：lineWidthAuto 关闭、负值钳 0、effectiveLineWidth 生效；
4. Fit：曲线严格通过全部顶点（逐点到采样曲线距离 < 1e-6）；
5. Spline：首尾控制点严格插值、中间控制点不被穿过（逼近型验证）；
6. Join：末端追加 / 首端前插 / 端点不重合拒绝 / 圆弧采样接入 + 来源删除；
7. 命中检测：Fit 状态下顶点命中距离 ≈ 0。

回归：`SketchSelfTest`（全量 UI+核心）构建通过、既有折线 9 功能与全量断言不受影响。
