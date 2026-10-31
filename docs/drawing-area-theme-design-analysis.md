# 绘图区域亮/暗主题切换 —— 设计分析

> 范围：仅绘图区域（画布 / 网格 / 坐标轴 / 原点 / 各类引导线 / HUD / 图层徽标 / 选中·预览·告警 / 约束图标 / 夹点 / 框选·缩放框·区域拾取 / 图层默认色）随内置主题联动；图元原生色与打印配色**不**主题化。
> 关联模块：`ThemeManager`（应用级主题单例）、`AppSettings(display/theme)`、选项对话框、`GraphicsView`（绘制层）。
> 新增文件：`src/ui/DrawingTheme.h`、`src/ui/DrawingTheme.cpp`；改动：`src/ui/GraphicsView.cpp`、`CMakeLists.txt`、`tests/SketchSelfTest.cpp`。

---

## 1. 需求回顾

1. 定义亮 / 暗两套主题样式（绘图区域全套配色）。
2. 提供切换机制（按钮或系统偏好检测）。
3. 绘图区域内画布、背景、网格线、坐标轴及图表元素随主题同步更新配色。
4. 切换过程平滑无闪烁。

## 2. 设计边界（关键决策）

把"主题化"严格限定在**结构性视觉 chrome**，数据 / 语义色保持原样：

| 类别 | 是否主题化 | 原因 |
|------|-----------|------|
| 画布背景、网格、坐标轴、原点、各引导线、HUD 文字、图层徽标、选中 / 预览 / 告警、约束图标、夹点、框选 / 缩放框 / 区域拾取、图层默认色 | ✅ 是 | 纯视觉 chrome，随主题翻色是用户预期 |
| 图元原生色 `re.color`（直线 / 圆 / 标注 / 填充等） | ❌ 否 | ① 是用户数据，不应被主题改写；② 与 `PlotRenderer` 共享，打印永远白底黑线，不可随屏主题 |
| 捕捉标记逐类型硬编码色（端点 / 中点 / 圆心…） | ❌ 否 | 编码语义（不同捕捉点类型用不同色区分），主题化会破坏可辨识性 |

亮色一套**刻意复刻原版外观**（`canvas=245,247,250`），保证存量用户无感；暗色针对 `#26282D` 画布提亮对比度（网格、坐标轴、引导线普遍加亮 30%~60%）。

## 3. 架构

```
AppSettings(display/theme: system|light|dark)   // 持久化，默认 light
        │  settingChanged(kTheme)
        ▼
MainWindow::applySettingLive
        ▼
ThemeManager::setMode(Mode)  ──emit──►  themeChanged(effectiveMode)
        │                                   │
   QStyleHints::setColorScheme()            ▼
   （原生控件亮暗翻转）            GraphicsView::themeChanged 连接：
                                          setBackgroundBrush(canvasColor())
                                          viewport()->update()   // 单帧全视口重绘
                                                  │
DrawingTheme::current()  ◄───────────────────────┘ 每个绘制函数绘制时取色
   = light() / dark() 按 effectiveMode 选择
```

- `ThemeManager`（`src/ui/ThemeManager.{h,cpp}`）：进程单例，职责①应用主题到 `QApplication`；②暴露 `canvasColor()`；③广播 `themeChanged`。`effectiveMode()` 把 `System` 解析为实际 `Light/Dark`（无 `qApp` 时回落 `Light`，故可无头直测）。
- `DrawingTheme`（`src/ui/DrawingTheme.{h,cpp}`）：纯数据层，承载两套 `DrawingPalette` 静态常量；`current()` 按 `ThemeManager::instance()->effectiveMode()` 返回其一。不反向依赖 `ThemeManager` 头文件（仅前向声明 + `.cpp` 内 include）。
- `GraphicsView`：所有绘制函数（`drawBackground`/`drawAxes`/`drawForeground`/`drawHandles`/`drawConstraintMarkers`/`ElementItem::penFor`/`paint` 等）在绘制时统一 `const DrawingTheme::Palette& th = DrawingTheme::current();` 取色，不再使用文件级 `static const` 硬编码色（已删除 8 个：GRID_MINOR/GRID_MAJOR/AXIS_X/AXIS_Y/HL/PREVIEW/GUIDE/WARN）。

## 4. 配色字段（`DrawingPalette`）

`canvas / gridMinor / gridMajor / axisX / axisY / originCross / axisLabel / selection / preview / warn / guide / objTrackGuide / polarGuide / tempTkGuide / orthoGuide / trackDropPolar / trackDropObj / trackDropTk / hudText / hudAccentBlue / hudAccentOrange / hudAccentGreen / badgeBg / badgeBorder / badgeText / marquee / marqueeFill / zoomBox / zoomBoxFill / regionPick / regionPickFill / grip / gripLine / trimBound / constraintOk / constraintWarn / layerDefault`。

两套关键取值（节选）：

| 字段 | light | dark |
|------|-------|------|
| canvas | 245,247,250 | 38,40,45 |
| gridMinor | 244,182,182 | 72,76,86 |
| gridMajor | 230,138,138 | 104,110,122 |
| axisX | 200,60,60 | 232,98,98 |
| axisY | 40,150,80 | 74,200,124 |
| selection | 31,95,139 | 92,162,220 |
| preview | 31,95,139,215 | 92,162,220,215 |
| warn | 214,48,49 | 242,98,98 |
| hudText | 108,116,132 | 182,190,202 |
| grip | 230,145,56 | 242,172,84 |
| constraintOk | 30,150,110 | 62,202,142 |
| layerDefault | 26,29,40 | 212,216,224 |

## 5. 平滑切换机制（无闪烁）

- `GraphicsView` 视口使用 `FullViewportUpdate` 模式：一次 `viewport()->update()` 触发单帧、全视口、单缓冲重绘，覆盖所有读取 `DrawingTheme::current()` 的覆盖层（背景、网格、坐标轴、HUD、徽标、选中 / 预览 / 约束 / 夹点 / 引导线）。
- `themeChanged` 连接中先 `setBackgroundBrush(canvasColor())` 再 `viewport()->update()`，二者在同一事件循环帧内完成，无中间态闪烁。
- 多文档各独立 `GraphicsView` 各自订阅 `ThemeManager::themeChanged`，同步翻色。
- 不新增工具栏按钮（用户未强制要求），复用既有入口：工具 → 选项 → 显示 → 界面主题下拉（light/dark/system），`System` 模式由 `QStyleHints::setColorScheme(Unknown)` 交还系统，原生控件与画布同步翻转。

## 6. 自测（`t_drawingTheme`）

新增无头用例（加入 `SketchSelfTest`，并补编 `DrawingTheme.cpp` / `ThemeManager.cpp` 两个 UI 源）：

- `light().canvas == (245,247,250)`，暗色 `== (38,40,45)`；
- 明暗两套 `canvas / gridMajor / axisX` 必须不同；
- 暗色主网格灰度 > 暗色画布灰度（保证可见对比度）；
- `axisX != axisY`（坐标轴可区分）；
- **契约**：`DrawingTheme::current().canvas == ThemeManager::canvasColor()`，且 `setMode(Dark)` 后 `current().canvas == dark().canvas`（测试后还原原模式，避免污染其它用例）。

## 7. 构建与回归

- 增量 `-j1` 构建 `SketchCAD` + `SketchSelfTest`：零警告、链接通过。
- 全量 `SketchSelfTest` 运行 `EXIT=0`（`t_drawingTheme` 随主流程执行）。
- `dist/SketchCAD.exe` 由 `build/SketchCAD.exe` 同步并 `cmp` 校验字节一致。

## 8. 已知约束与后续

- 打印（`PlotRenderer`）走纸张配色，不受屏主题影响，属预期。
- 捕捉标记色仍硬编码（语义色），若未来需要暗色下更高对比度，可单独做一份"语义色暗色表"，但需保证与亮色同义可辨。
- 主题切换为进程级（所有文档同步）；未实现"单文档独立主题"，与主流 CAD 行为一致。
