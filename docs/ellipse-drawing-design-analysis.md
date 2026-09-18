# 椭圆绘制全功能 — 实现说明

> 项目：航空 CAD 草图绘制模块（sketch-cad-cpp，C++17 / Qt 6.10.1 / CMake+Ninja）
> 目标：参照 FreeCAD 草图「椭圆」菜单 6 项，完整实现椭圆绘制的数据模型、几何求解、渲染、交互、菜单与命令行。

## 1. 功能范围（菜单 6 项）

| 菜单项 | 工具 ID | 输入 | 说明 |
|---|---|---|---|
| 椭圆 (1 Point) | `ellipse_1pt` | 中心 + 椭圆上一点 | 该点作为长轴端点；短轴取长半轴一半 |
| 椭圆 (Axis) | `ellipse_axis` | 中心 + 轴端点 + 椭圆上一点 | 轴端点定长轴方向与 a，第 3 点约束 b |
| 椭圆 (焦点) | `ellipse_foci` | 焦点 1 + 焦点 2 + 椭圆上一点 | 2a = 到两焦点距离和，2c = 焦距 |
| 椭圆 (4 点) | `ellipse_4p` | 四个点 | 拟合轴对齐椭圆（中心与轴长联合求解） |
| 椭圆 (中心 + 3 点) | `ellipse_c3p` | 中心 + 三个点 | 拟合任意旋转椭圆 |
| 椭圆 (内切) | `ellipse_inscribed` | 选择矩形/多边形 | 按外框包围盒作内切椭圆 |

## 2. 数据模型

- `ElementType::Ellipse`（追加在枚举末尾，`.sketch` 归档向后兼容）。
- `Element` 新增字段：
  - `double radiusY = 0;` 短半轴（仅 Ellipse 有效）；
  - `double rotation = 0;` 长轴与 X 轴正向夹角（度，逆时针为正，Y 向上系）。
- 统一表示约定：**radius ≥ radiusY**（`normalizeAxes` 保证），长轴竖直时 `rotation=90`。
- `ElementFactory::createEllipse(center, radiusX, radiusY, rotationDeg, model)`。
- JSON 序列化增加 `radiusY` / `rotation`，读取带 `contains()` 向后兼容回退。

## 3. 几何求解（core/EllipseModes）

文件：`src/core/EllipseModes.{h,cpp}`。统一入口 `ellipse::compute(Mode, pts, refs, cur, elems, param)`，
外加各模式原子构造器（供视图层/CLI/自测共用），与 Circle/Arc/Spline 模式架构一致。

- **1 Point**：`a=|c→p|`，`b=a/2`，`rot=∠(c→p)`。
- **Axis**：轴端点定局部系；把第 3 点变换到长轴局部坐标 `(u,v)`，由 `(u/a)²+(v/b)²=1` 解 `b`。
- **Foci Point**：`2a=d1+d2`、`2c=|F1F2|`、`b=√(a²−c²)`；中心 = 焦点中点；旋转 = 焦点连线方向。
  点在焦点连线上（`2a≤2c`）判失败。
- **4 Point（关键实现）**：四个点共给出 4 个方程，未知量是 `(cx, cy, u=1/a², v=1/b²)`——
  **不能把四点质心当中心**（只在两轴对称分布时才成立）。实现：
  1. 外层在中心平面做模式搜索（pattern search，包围盒中心起步，步长衰减至 span·1e-10，迭代上限 200）；
  2. 内层给定中心后对 `u,v` 做线性最小二乘，残差驱动搜索；
  3. 残差 >1e-12 → 判「四点不共轴对齐椭圆」；
  4. 收敛点奇异（四点构成对称矩形等退化布局，四行方程退化为一行、解不唯一）→ **显式报错**，绝不静默挑一个假椭圆。
- **Center + 3 Points**：中心给定，对旋转角做 1-D 扫描 + 每角度线性最小二乘
  （`u·cos²θ+v·sin²θ` 形式的半径残差），粗扫 360° 后逐步细化，同样带迭代上限。
- **Inscribed**：取矩形/多边形全部角点的包围盒，中心、`a=框宽/2`、`b=框高/2`、rot=0（对矩形即四边中点相切的标准内切椭圆）。

### 拟合踩坑记录

1. **欠定必须显式报错**：45°/135°/225°/315° 斜对称四点属于「矩形四顶点」，过它们有无穷多个
   轴对齐椭圆。旧思路（质心当中心 + 最小二乘）在系数矩阵奇异时会静默产出不经过四点的假椭圆，已改为报错。
2. **轴表示歧义**：`a=10,b=5,rot=0` 与 `a=5,b=10,rot=90` 是同一椭圆。全部构造器经 `normalizeAxes`
   统一为 `a≥b`，测试与 UI 才无歧义。
3. **区分「已拾取点」与「光标点」**：1Point/Axis/Foci 三个模式的最后输入其实是**光标点**（鼠标悬停随动），
   不算已拾取点。若 UI 收到 `pts==需求数` 就 autoFinalize，光标恰好等于最后一次点击，会出现
   `a=0` / 第三点在长轴上 (`v≈0`) / `2a≈2c` 而全部失败——三种画法实际都画不出来。

## 4. 渲染与命中

- `GraphicsView::buildPath`：`QPainterPath` 经 `QTransform::translate(center).rotate(rotation)` 后 `addEllipse` 椭圆路径。
- `sampleEllipse`（Geometry）：160 段折线采样，供命中/拖拽/DXF/导出复用。
- `geo::distanceToElement`：把点变换进椭圆局部系，用 `|u·x²/a² + y²/b² − 1|` 梯度缩放作近似距离 → 只命中轮廓。
- 控制点（`elementHandles`）= [中心, 长轴端点, 短轴端点]；拖拽：中心 → 平移；长轴端点 → 改 a 与 rot；短轴端点 → 改 b。
- `geo::flatten` 支持椭圆离散闭合折线。

## 5. 交互 / 菜单 / 工具栏 / CLI

- 注册：`ToolCategory::EllipseSub`，6 个工具 ID 进 ToolRegistry（含 `isDrawTool` 分类）；`SubToolHandler::Family::Ellipse`。
- `GraphicsView`：`isEllipseSubTool` / `handleEllipseSubTool` / `finalizeEllipseEx`；`updatePreview` 实时预览，
  拟合/交互模式下光标决定最终形状；收满点时对 FourPoint/Center3Points 即时成图。
- 菜单：顶级「椭圆(&E)」菜单（扁平 6 项）+ 「草图绘制」菜单椭圆区 + 「圆与圆弧」工具组下拉按钮
  `createDropdownTool(tb, group, bucket, "ellipse", …)` + ElementPanel 调色板入口 + icons.h 椭圆字形。
- CLI：`EllipseModeCommand` 注册 `ellipse_` 前缀；无真实光标，1Point/Axis/Foci 需多传一点充当光标。
  `ellipse_inscribed <id>` 支持矩形/多边形外框内切。

## 6. 测试矩阵（全绿）

- `SketchSelfTest`：**263 通过 / 0 失败**（§25 椭圆：6 模式几何、控制点/拖拽、命中、flatten、CLI 全链路 40+ 用例）。
- `ViewPanTest` / `ViewRenderTest` / `ViewRenderTest2`（offscreen）：全部 **PASS**（无回归）。

## 7. 关键文件清单

| 文件 | 变更 |
|---|---|
| `src/core/Element.h` / `Element.cpp` / `ElementFactory.*` | Ellipse 类型、radiusY/rotation、工厂、sizeDesc |
| `src/core/EllipseModes.h` / `.cpp` | 6 模式几何求解（新增） |
| `src/core/Geometry.h` / `.cpp` | sampleEllipse、命中、控制点、拖拽、flatten |
| `src/core/ToolRegistry.*` / `src/ui/tools/*` | EllipseSub 分类、Family::Ellipse、ToolContext 委派 |
| `src/ui/GraphicsView.*` | buildPath 渲染、椭圆 handler/预览/默认分支 |
| `src/ui/MainWindow.*` | 顶级菜单 + 草图菜单区 + 工具栏下拉 |
| `src/ui/icons.h` / `ElementPanel.cpp` | 椭圆图标与调色板入口 |
| `src/core/Commands.*` / `CommandInterpreter.*` | `ellipse_` CLI 命令族 |
| `CMakeLists.txt` | 加入 `EllipseModes.cpp` |
| `tests/selftest.cpp` | §25 椭圆测试 |
