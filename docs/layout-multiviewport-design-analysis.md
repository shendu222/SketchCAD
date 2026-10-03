# SketchCAD 布局体系（多布局 / 视口平铺 / 视口独立比例）设计分析

> 任务 H：从单视口 MVP 升级为可用布局体系。本文记录现状盘点、设计决策、
> 实现要点、陷阱与验收对照。

---

## 一、现状盘点（动手前定界）

| 现状 | 位置 | 与本任务的关系 |
| --- | --- | --- |
| `PlotViewport{name, modelRect, scale, rotationDeg}` | `core/Layout.h` | 只增不改：追加 `paperRect` |
| `PrintSettings`（页面参数 + 样式映射 + 视口集） | `ui/PrintEngine.h` | **充当单布局**；需下沉 core 层 |
| `render()` 视口分支：每视口映射**整块可打印区** | `ui/PrintEngine.cpp` | MVP 平铺缺口，本任务改为按 `paperRect` |
| `exportPdf/print` 单页 | `ui/PrintEngine.cpp` | 需新增每布局一页的 `exportPdfLayouts` |
| 打印设置**不入档** | `MainWindow::currentPrintSettings_` 内存持有 | 布局表需随图存档（顶层 `layouts`/`currentLayout`） |
| 标注样式表范式（按名引用 + 缺键回落默认） | `SketchModel::dimStyles_` | 布局表直接对标该范式 |

关键约束的定界结论：

- **PrintSettings 无 ui 依赖**（QPageSize/QPageLayout/QMarginsF 均 QtGui，core 已链）
  → 可整体下沉 `core/Layout.h`，SketchModel（core）才能持有布局表，命令行（core）
  才能经 `ctx.model` 访问。这是本任务唯一的「搬家」决策。
- **打印保真换算链不重写**：`renderViewport` 的 pxPerMm 换算、y 翻转 QTransform、
  目标区裁剪、`PlotRenderer::renderModel` 调用全部保留，仅把「目标矩形」参数化
  （原实现里目标矩形恰好恒等于可打印区）。

## 二、设计决策

### 决策 1：PrintSettings 下沉 core/Layout.h，PrintEngine.h 只留引擎

- 字段与语义逐行不动；`toJson/fromJson/defaults/effectivePlotStyle` 实现迁入 `Layout.cpp`。
- `PrintEngine.h` 本就 include `core/Layout.h` → 12 处 ui/com 引用面**零改动**编译通过。
- 收益：core 层 `SketchModel::layouts_`、core 层命令行、无头测试三层同源。

### 决策 2：PlotViewport 只增 `paperRect`（图纸目标区），缺键 = 整页 = MVP 行为

```cpp
QRectF paperRect;   // 图纸 mm，相对可打印区左上角、y 向下；空矩形 → 整块可打印区
```

- 序列化键 `paperX/paperY/paperW/paperH`；**空矩形不写键**，读档缺键 → 空矩形
  → 旧档/MVP 视口回落「映射整块可打印区」，逐字节兼容。
- 视口平铺 = 布局内多个视口各占 paperRect 一块矩形，坐标口径固定为
  「相对可打印区左上、y-down」，与打印设备坐标一致，换算零歧义。

### 决策 3：比例解析提取为纯函数 `viewportScaleMm`（同式提取，非重写）

```cpp
double viewportScaleMm(const PrintSettings& s, const PlotViewport& vp,
                       const QRectF& mb, double printW, double printH);
// (vp.scale > 0) ? vp.scale : (s.fitToPage ? fit : s.scale);
// fit = min(printW/mb.width(), printH/mb.height()) * 0.98
```

- 语义：`vp.scale` = 视口内 1 个模型 mm 在图纸上占的 mm 数。
  **1:2 出图 = 0.5，2:1 出图 = 2.0**（AutoCAD 同口径）。
- 平铺后 fit-to-page 的适配基准从「整块可打印区」改为「视口自身 paperRect」——
  paperRect 为空时二者相等，MVP 行为不变；有 paperRect 时按视口适配才是正确语义。

### 决策 4：布局表入 SketchModel（引用式命名 + 缺键回落），不入撤销栈

```cpp
std::vector<PlotLayoutDef> layouts_;   // 布局名 + 一份完整 PrintSettings
QString currentLayout_;                // 空 = 模型空间
```

- 与 `savedFilters_`/`ltscale_` 同类：**应用级配置，不入撤销栈**（撤销图纸几何，
  不撤销页面设置——与 AutoCAD 布局不在 UNDO 链一致）。
- 存档：顶层 `"layouts"`（数组）+ `"currentLayout"`（字符串）；**缺键 → 空表 →
  模型空间打印**，旧档零行为变化（验收项）。
- `removeLayout` 若删除的是当前布局 → `currentLayout_` 清空回落模型空间（悬空防御）。
- 信号 `layoutsChanged()` 驱动后续布局管理器/打印链刷新。

### 决策 5：打印链——renderPageBody 抽取 + exportPdfLayouts 每布局一页

- `render()` 页面主体（边框 + 模型空间/视口分支）抽取为 `renderPageBody`，
  单页 render 与多页 exportPdfLayouts 共用同一套绘制逻辑（预览/PDF/打印一致性保持）。
- `exportPdfLayouts(SketchModel*, path) → int`：
  - 布局表空 → 回落单页模型空间打印（fit-to-page），返回 1（旧档行为）；
  - 否则每布局一页：每页先套该布局的纸张/方向/边距，再 `newPage()` + `renderPageBody`。
  - 返回页数（测试断言页数不依赖 Qt PDF 模块，PDF 以存在性 + 大小验证）。
- 视口分支：paperRect 有效 → 目标区 = 可打印区偏移平移（`tx = mL + paperRect.x()`）；
  裁剪窗同步缩小到目标区 → 视口间互不越界（平铺正确性的物理保证）。

### 决策 6：命令行先行，夹点调整视口列后续

- `layout new/set/list/rename/delete/copy/vpscale` + `mview <x> <y> <w> <h> [比例]`。
- 画布没有 paper space 视图，夹点拖拽视口本期不做（验收不含夹点断言，已在任务范围注明）。
- `mview` 无当前布局时自动建「布局1」并切换（对标 AutoCAD 默认布局体验，减少摩擦）。
- 比例输入双格式：`1:2`（图纸:模型 → 0.5）或小数 `0.5`；`layout vpscale` 就地改当前布局视口。

## 三、实现要点与陷阱

| # | 要点 | 说明 |
| --- | --- | --- |
| 1 | **QString::arg 不支持 `%.1f`** | mview 日志初稿混用 printf 风格占位符与 `%1`，已改为全 `%n` + `arg(v, 0, 'f', 1)` |
| 2 | **多页 PDF 的页面设置时序** | 每页先 `setPageSize/setPageOrientation/setPageMargins` 再落笔/`newPage()`；QPdfWriter 在落笔时读取当前页面布局。首页 painter begin 前设置，后续页 newPage 前设置 |
| 3 | **renderPageBody 共用而非复制** | 保证「预览/单页 PDF/多布局 PDF」三条出图路逐像素同源，打印保真换算链只此一份 |
| 4 | **空图纸返回值语义** | renderPageBody 内空图纸只画边框不报错；render 返回 true 保持历史行为 |
| 5 | **存档接线三处** | MainWindow 存/读 + BatchPlotEngine 存/读必须同步补 `layouts`/`currentLayout`，与 dimStyles 同址接线（历史惯例：新增随档表 = 4 处落点） |
| 6 | **编辑事故自纠** | BatchPlotEngine 读档插入时误删 `setUnits` 一行，下一编辑立即发现补回——多文件同锚点编辑后必须回读核对面 |

## 四、验收对照

| 验收项（任务原文） | 实现 | 断言/证据 |
| --- | --- | --- |
| 双布局双比例出图 PDF 样张 | `exportPdfLayouts` 每布局一页 | 自测产 PDF（QDir::temp），复制 `docs/images/layout-scales-sample.pdf` |
| 视口比例 1:2 / 2:1 断言 | `viewportScaleMm` 纯函数 + 渲染像素对照 | 0.5/2.0 直接断言；同模型双比例渲染墨量 N2 显著大于 N1 |
| 视口平铺（互不越界） | paperRect 目标区 + 裁剪窗同步 | 双视口侧排 + 中缝采样列无墨、两侧有墨 |
| 旧档（无布局 key）回落模型空间打印 | `setLayoutsFromJson` 缺键 → 空表 | `layoutCount()==0 && currentLayout().isEmpty()`；exportPdfLayouts 返回 1 |
| paperRect 缺键回落 | `fromJson` 缺 `paperW/paperH` → 空矩形 | 往返 + 剥键两断言 |
| 自测新增 ≥8 断言 | `t_layout()` 20+ 条 | 见下清单 |
| 零警告 | 双目标 `-Wall -Wextra` | 构建日志 grep 计数 = 0 |

### t_layout 断言清单

1. paperRect 往返（含值保持）
2. paperRect 剥键读回 → 空矩形（MVP 回落）
3. PlotLayoutDef JSON 往返（name + settings 内 pageSize/scale）
4. 布局 CRUD：addLayout 成功 / 重名拒绝 / 空名拒绝
5. updateLayout 原地替换 + 未命中拒绝
6. renameLayout 改名 + 当前布局名同步 + 重名拒绝
7. removeLayout 删除 + 删当前布局回落模型空间
8. setCurrentLayout 非法名拒绝 / set model（空）回落
9. 存档往返：layoutsToJson → setLayoutsFromJson 全等
10. 旧档缺键 → 空表回落模型空间
11. viewportScaleMm：scale=0.5（1:2）原样返回
12. viewportScaleMm：scale=2.0（2:1）原样返回
13. viewportScaleMm：scale=0 + fitToPage → fit 公式值
14. viewportScaleMm：scale=0 + !fitToPage → s.scale
15. 渲染平铺：双视口 paperRect 侧排，中缝无墨、两侧有墨（QImage 300dpi）
16. 渲染比例对照：同模型 1:2 vs 2:1 单视口墨量递增（2:1 > 1:2）
17. exportPdfLayouts 双布局 → 返回 2 + PDF 存在且非空
18. exportPdfLayouts 空布局表 → 返回 1（回落模型空间）+ PDF 存在
19. 命令行：layout new/set/list/vpscale 全链
20. 命令行：mview 建视口 + 缺省布局自动建「布局1」+ 1:2 解析为 0.5

## 五、范围边界（本任务未做，防蔓延）

- 视口内图层替代（freeze per viewport）——列为后续；
- 夹点拖拽/调整视口（画布无 paper space 视图）——列后续；
- 布局管理器 GUI 面板——命令行先行，面板后续接 `layoutsChanged()` 即可；
- `PlotViewport.rotationDeg` 在视口渲染中尚未消费（历史字段，保持原样）。
