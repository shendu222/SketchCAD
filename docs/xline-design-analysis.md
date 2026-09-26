# XLINE（构造线）功能设计分析

> 版本：1.0　日期：2026-09-26　状态：已实现，XlineTest 全绿 + 既有回归通过

## 1. 需求回顾

在绘图模块中新增 `XLINE`（构造线）命令：绘制向两端无限延伸的辅助线。

| # | 要求 | 落地方式 |
|---|------|----------|
| 1 | 命令行 `XLINE` + 菜单/工具栏入口，提示选择绘制方式 | `XlineCommand`（精确 `xline` + 前缀 `xline_*`）；工具条【基本图元】下拉按钮 +【绘图 > 构造线(&X)】子菜单 |
| 2 | 六种绘制方式：H / V / A / 两点(默认) / B / O | 全部映射到既有 `line::Mode` 几何计算（零重复实现） |
| 3 | 动态预览、回车确认、Esc/右键取消、连续绘制 | `previewXline`（无限线实时预览）；`commitXlineAndContinue` 提交后保持命令活跃；右键/Esc 走通用兜底退出 |
| 4 | 按视图边界裁剪显示、缩放同步；图层/颜色/线型；撤销重做；不参与面积长度统计 | `buildPath(Xline, visibleWorldRect)` 每帧裁剪；样式走 `ElementFactory::create` 默认管道；`addElement` 自动入撤销栈；`flatten` 对 Xline 返回空 → 剪切/统计天然排除 |
| 5 | 复用坐标输入/捕捉/单位处理，与 LINE/RAY 风格一致 | 捕捉（通过点+交点）、`injectPoint` 坐标注入、`snapDefaultFor` 默认档全部自动生效 |

## 2. 数据模型

**复用 Element 现有字段，零新增结构**（枚举"只增不改"铁律）：

- `ElementType::Xline` 追加到枚举末尾，**序号 = 13**（老 .sketch 存整数序号，追加不破坏旧档）。
- `pts[0]` = 通过点（世界坐标）；`rotation` = 方向角（度，世界角，Y 翻转只在渲染期）。
- 序列化 `elementToJson/FromJson` 为泛型字段读写，**无需改动**。

## 3. 无限直线的渲染方案（本设计核心难点）

`ElementItem::buildPath` 是静态方法、拿不到视图；且 QGraphicsItem 的 boundingRect
是缓存值，不会随视图平移/缩放自动重算。方案：

1. **有界化**：无限直线先与「场景级大矩形」`kXlineBigRect(-1e5,-1e5,2e5,2e5)`
   （与 `scene_->setSceneRect` 同级）slab 法求交，得到一条有界长线段 ——
   boundingRect 取该交段，**恒定不随视图变化**，保证平移/缩放时图元永不被场景索引剔除。
2. **二次裁剪**：`paintOne` 对 Xline 每帧传入 `visibleWorldRect()`
   （`view->mapToScene(viewport()->rect())`），实际绘制路径只含可见段 ——
   缩放/平移时显示自动同步，且不把 ±1e5 的巨长线段交给光栅器。
3. **取景排除**：`rawBounds(Xline)` 返回空矩形 → `contentBounds()` 跳过（fitView
   不被无限线撑爆）；命中/拾取走 `geo::distanceToElement` 的点到无限直线距离，
   与"无限延伸"的视觉一致。

## 4. 交互链路（策略模式，与直线族同构）

```
工具 id: xline / xline_h / xline_v / xline_angle / xline_2pt / xline_bisector / xline_parallel
  → ToolRegistry (XlineSub 类别 + DrawBasic 的 xline)
  → ToolManager (Family::Xline 的 SubToolHandler)
  → ToolContext::handleXlineSubTool
  → GraphicsView::handleXlineSubTool
      line::modeFromString("xline_*") → line::Mode → line::compute(...)
      结果线段 (a,b) → createXline(a, angleDeg(b-a)) → commitXlineAndContinue
```

- `line::modeFromString` 扩展识别 `xline_*`（H→Horizontal、V→Vertical、A→Angle、
  2pt/默认→TwoPoint、B→Bisector、O→Parallel），**几何计算零复制**。
- 连续绘制：`commitXlineAndContinue` 提交后清空本轮采集但保持 `drawing_=true`，
  与 `commitElement`（resetPending 退出）刻意不同；右键/Esc 走
  `handleRightButton` 通用兜底（"已结束当前绘制"→ 再右键回到选择）。
- 预览：`previewXline` —— 角度模式未拾取时以光标为基点按参数角度预览；
  平行模式已选参考线时按光标位置预览；其余经 `line::compute` 后转无限线。

## 5. 命令行

| 命令 | 语义 |
|------|------|
| `xline x,y <角度>` | 过点 + 角度（`"45"` 不是合法点 → 自动与两点式区分） |
| `xline x1,y1 x2,y2` | 两点定线（默认方式） |
| `xline_h / xline_v x,y` | 水平 / 竖直过点 |
| `xline_angle x,y [角度]` | 指定角度（缺省 30°，与 line_angle 一致） |
| `xline_bisector id1 id2 x,y` | 两直线角平分方向 |
| `xline_parallel id x,y` | 平行于参考直线过点 |

注册：`registerExact("xline") + registerPrefix("xline_")`，共用 `XlineCommand`。

## 6. 兼容层

| 模块 | 处理 |
|------|------|
| 捕捉 | `snapHandles`：仅通过点（无限线无端点）；`distanceToElement`：点到无限直线距离（交点/近距拾取命中整条延伸线） |
| 交点 | `intersectElements`：Xline 展开为 ±1e5 长线段后按常规路径求交；构造线×构造线走 `math::lineLineIntersect` —— 交点捕捉是构造线的核心用途 |
| 剪切/统计 | `flatten` 对 Xline 返回空（default 分支）→ 不参与 trim/面积/长度 |
| DXF R12 | 无 XLINE 原生实体 → 降级为通过点沿方向 **±10 m** 的 LINE |
| DWG R12 | 同上，降级 LINE；读回即普通 Line（往返测试验证） |
| STEP | 降级为 ±10 m 的 TRIMMED_CURVE(LINE) |
| 取景/命中 | `geometryBounds` 空 → fitView 排除；`elementHandles` 走 default 返回 pts（拖通过点=平移构造线） |

## 7. UI 入口

- 工具条【基本图元】组：`createDropdownTool`，主按钮"构造线"（两点方式），下拉 6 项。
- 菜单【绘图 > 构造线(&X)】：`buildXlineMenu`，与工具条共享 `toolActions_` 指针。
- `MainWindow::xlineSubModes()` 唯一数据源，纳入 `allSubModeToolIds()`
  （DrawingMenuTest §10 一致性校验自动覆盖）。
- 图标：`icons.h` 新增 `"xline"` glyph（两端出界的贯通斜线 + 通过点，区别于直线的带端点斜线）。

## 8. 验证

- **新增 `tests/xline_test.cpp`（CMake 正式 target `XlineTest`）**，7 组 67 断言全绿：
  工厂/枚举/序列化往返、ToolRegistry 注册、命令行六种模式（含缺参拒绝/参考不存在拒绝）、
  点线距离、交点（线×线/线×圆/构造线×构造线）、捕捉仅通过点、flatten 排除、
  DXF/DWG 导出降级、撤销重做。
- **既有回归**：`SketchCAD` 全量编译链接通过；`SketchSelfTest` exit 0；
  `DrawingMenuTest`（含 allSubModeToolIds ⊆ ToolRegistry 校验）exit 0。
- 构建方式：`ninja -j 1 <target>`（工程 OOM 约束），测试从工程根运行。

## 9. 已知边界（有意行为）

- fitView/内容统计排除构造线 —— 若图纸只有构造线，适配窗口无目标（与"辅助线"定位一致）。
- 框选（marquee）按 boundingRect 判定，±1e5 交段不可能完全落入框内 → 构造线不被框选整选，
  单击（点线距离容差）仍可选中，避免辅助线被常规框选误带。
- 导出降级为 ±10 m 长线段，属 R12 格式的固有限制；未来升级 R13+ 格式可写原生 XLINE。
