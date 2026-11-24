# SketchCAD 绘图命令全清单（机械提取 + 完整性核对）

> 生成时间：2026-11-14 20:30  
> 分析对象：**当前工作区主程序 `sketch-cad-cpp`（SketchCAD 1.1.1）的用户可执行命令体系**。
> 提取方式：脚本扫描 387 个源码/测试文件（`src/ tests/ bench/ sdk/ tools/`），
> 交叉解析 `registerExact/registerPrefix`、`CommandCatalog::add()`、`AliasTable::defaultAliases()`、
> `CommandInterpreter` 补全家族、`NlAssistant` AI 核心命令表，以及所有 `execute(...)` 调用点。
> 若需改为分析某个具体 HTML/JS 演示程序或单个脚本文件，范围需另行指定。

## 1. 口径定义

| 字段 | 口径 |
|---|---|
| 命令名称 | 命令串首 token（小写）；主清单 = 精确注册表 ∪ 命令目录 ∪ 补全家族 ∪ 别名目标 ∪ AI 核心命令；别名键本身单列于 §5 |
| 所在位置 | `文件:行号`，标注来源类型（注册 / 目录 / 家族 / 别名 / 调用点） |
| 主要参数 | 取自 `CommandCatalog` 的 `usage`；未收录时取 `params` 或标 `—` |
| 作用说明 | 取自目录 `summary`；目录未收录时据注册位置与调用点标注 |
| 出现次数 | 命令名作为**完整字符串字面量** `"name"` 在上述源码集合中出现的次数（可机械复核；命令串内嵌形式见 §6） |
| 调用条件 | 注册方式 + 用法/参数 + 是否需选中 + 快捷键 + 是否为 AI 核心命令 |
| 影响绘图结果 | A–E 类直接影响几何/样式/视图/渲染；F 类为纯查询或系统辅助，列出以保证不遗漏 |

## 2. 总览统计

| 来源 | 数量 |
|---|---|
| `registerExact` 精确注册（全项目） | 150 处 / 150 个唯一命令名 |
| `registerPrefix` 前缀分派 | 9 |
| `CommandCatalog` 元数据条目 | 139 |
| acad.pgp 默认别名 | 66 条 / 44 个目标命令 |
| 补全家族成员 | 58 个（5 个家族） |
| AI system prompt 核心命令 | 24 |
| `execute(...)` 静态调用点 | 197 处 / 64 个首 token |
| **并集（本清单命令总数）** | **199** |
| 分类 A 直接绘制/生成 | 74 |
| 分类 B 编辑与几何变换 | 56 |
| 分类 C 样式/图层/线型 | 11 |
| 分类 D 坐标/视图/刷新 | 20 |
| 分类 E 选择与捕捉 | 12 |
| 分类 F 辅助/查询/系统 | 25 |
| 分类 G 其他（影响渲染流程） | 1 |

## 3. 绘图命令主清单（按类别）

> 表格列：命令名 / 所在位置 / 主要参数 / 作用说明 / 出现次数 / 调用条件

### A 直接绘制/生成（74 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `arc` | 注册 src/core/Commands.cpp:3302 / 目录 src/core/CommandCatalog.cpp:41 | arc cx,cy r a0 [a1] | 圆心 + 半径 + 起止角绘制圆弧 | 39 | 精确注册；命令行/脚本/AI/菜单可调用；用法：arc cx,cy r a0 [a1]；参数：a0 起始角；a1 终止角（度，省略 = a0+360 即整圆弧）；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `attdef` | 注册 src/core/Commands.cpp:4213 / 目录 src/core/CommandCatalog.cpp:119 | attdef <块名> <标签> [提示] [x,y] [字高] | 为块定义追加属性定义（属性块） | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：attdef <块名> <标签> [提示] [x,y] [字高] |
| `attedit` | 注册 src/core/Commands.cpp:4214 / 目录 src/core/CommandCatalog.cpp:120 | attedit <引用id> <标签> <值> | 编辑块引用的属性值 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：attedit <引用id> <标签> <值> |
| `attlist` | 注册 src/core/Commands.cpp:4215 / 目录 src/core/CommandCatalog.cpp:121 | attlist [引用id] | 列出块引用属性（无 id = 全部引用） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：attlist [引用id] |
| `bhatch` | 注册 src/core/Commands.cpp:4344 / 目录 src/core/CommandCatalog.cpp:53 | bhatch | hatch 的别名入口（自动选边界） | 9 | 精确注册；命令行/脚本/AI/菜单可调用；用法：bhatch；有 acad.pgp 别名入口 |
| `block` | 注册 src/core/Commands.cpp:4212 / 目录 src/core/CommandCatalog.cpp:117 | block create <块名> [基点x,y] [delete\|keep] \| insert <块名> <x,y> [旋转] [sx] [sy] \| list \| info <块名> \| redefine \| explode <引用id> \| delete \| rename | 块的创建/插入/重定义/分解/删除/重命名/列表（单一分派命令） | 21 | 精确注册；命令行/脚本/AI/菜单可调用；用法：block create <块名> [基点x,y] [delete\|keep] \| insert <块名> <x,y> [旋转] [sx] [sy] \| list \| info <块名> \| redefine \| explode <引用id> \| delete \| rename；有 acad.pgp 别名入口 |
| `circle` | 注册 src/core/Commands.cpp:3300 / 目录 src/core/CommandCatalog.cpp:39 | circle cx,cy r | 圆心 + 半径绘制圆 | 49 | 精确注册；命令行/脚本/AI/菜单可调用；用法：circle cx,cy r；参数：cx,cy 圆心；r 半径（>0）；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `circle3p` | 注册 src/core/Commands.cpp:3301 / 目录 src/core/CommandCatalog.cpp:40 | circle3p x1,y1 x2,y2 x3,y3 | 过三点绘制圆 | 20 | 精确注册；命令行/脚本/AI/菜单可调用；用法：circle3p x1,y1 x2,y2 x3,y3；AI system prompt 核心命令 |
| `constraint` | 注册 src/core/Commands.cpp:3490 / 目录 src/core/CommandCatalog.cpp:101 | constraint add <类型> <idA> <idB> [值\|轴id] \| solve \| list \| check \| diagnose \| clear | 几何约束的添加/求解/检查/诊断/清理 | 14 | 精确注册；命令行/脚本/AI/菜单可调用；用法：constraint add <类型> <idA> <idB> [值\|轴id] \| solve \| list \| check \| diagnose \| clear；参数：类型：fixed/fixedpoint/horizontal/vertical/parallel/perpendicular/tangent/... |
| `dim` | 注册 src/core/Commands.cpp:3470 / 目录 src/core/CommandCatalog.cpp:96 | dim linear\|aligned\|h\|v x1,y1 x2,y2 [px,py] | 线性/对齐/基线/连续/角度/半径/直径/引线/坐标标注（GB/T 4458.4-2003） | 33 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dim linear\|aligned\|h\|v x1,y1 x2,y2 [px,py]；参数：子型：baseline / continue / angle / radius / dia / arc / leader / ordinate |
| `dim_smart` | 注册 src/core/Commands.cpp:3472 | — | 精确注册于 src/core/Commands.cpp:3472（处理器 DimSmartCommand）；目录未收录，help 走回落 | 5 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `dimedit` | 注册 src/core/Commands.cpp:4680 / 目录 src/core/CommandCatalog.cpp:155 | dimedit <标注id> <新值> 或选中标注后 dimedit <新值> | 修改标注数值，几何按关联联动调整，标注/文字/域同步更新 | 2 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dimedit <标注id> <新值> 或选中标注后 dimedit <新值>；参数：支持：对齐/距离/水平/竖直/半径/直径/角度；容忍 R/⌀/° 前后缀；需先选中图元 |
| `dims` | 注册 src/core/Commands.cpp:3471 / 目录 src/core/CommandCatalog.cpp:97 | dims [id...] | 依据当前选区自动选择标注类型（直线→对齐，圆→直径） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dims [id...] |
| `dimstyle` | 注册 src/core/Commands.cpp:4124 / 目录 src/core/CommandCatalog.cpp:98 | dimstyle [list \| new <名> [gbt\|iso] \| set <名> \| rename <旧> <新> \| apply <名> [id...] \| delete <名>] | 标注样式的创建/切换/应用/删除（一图多标准） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dimstyle [list \| new <名> [gbt\|iso] \| set <名> \| rename <旧> <新> \| apply <名> [id...] \| delete <名>] |
| `dtext` | 注册 src/core/Commands.cpp:4604 / 目录 src/core/CommandCatalog.cpp:152 | dtext <x,y> <字高> <内容...> | text 的别名入口 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dtext <x,y> <字高> <内容...> |
| `dynlist` | 注册 src/core/Commands.cpp:4217 / 目录 src/core/CommandCatalog.cpp:123 | dynlist <块名> | 列出块定义的动态参数 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dynlist <块名> |
| `dynset` | 注册 src/core/Commands.cpp:4216 / 目录 src/core/CommandCatalog.cpp:122 | dynset <引用id> <参数名> <值\|状态名> | 设置块引用的动态参数（数值或可见性状态） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dynset <引用id> <参数名> <值\|状态名> |
| `ellipse_1pt` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 13 | 前缀分派 ellipse_* → EllipseModeCommand |
| `ellipse_4p` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 ellipse_* → EllipseModeCommand |
| `ellipse_axis` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 ellipse_* → EllipseModeCommand |
| `ellipse_c3p` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 ellipse_* → EllipseModeCommand |
| `ellipse_foci` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 ellipse_* → EllipseModeCommand |
| `ellipse_inscribed` | 家族 src/core/CommandInterpreter.cpp:37 (ellipseCommandNames) | — | ellipse_* 家族子命令（EllipseModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 ellipse_* → EllipseModeCommand |
| `freehand_line` | 注册 src/core/Commands.cpp:4235 | — | 精确注册于 src/core/Commands.cpp:4235（处理器 SplineModeCommand）；目录未收录，help 走回落 | 11 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `hatch` | 注册 src/core/Commands.cpp:4343 / 目录 src/core/CommandCatalog.cpp:52 | hatch [scale <倍数> \| angle <度> \| pattern <名\|none>] | 对选中闭合边界生成关联填充，或调整填充参数 | 15 | 精确注册；命令行/脚本/AI/菜单可调用；用法：hatch [scale <倍数> \| angle <度> \| pattern <名\|none>]；参数：先选中闭合边界图元（圆/矩形/闭合折线等）；无参数 = 默认填充；需先选中图元；有 acad.pgp 别名入口 |
| `insert` | 注册 src/core/Commands.cpp:4583 / 目录 src/core/CommandCatalog.cpp:118 | insert <块名> <x,y> [旋转度] [缩放X] [缩放Y] | 把块定义插入为块引用 | 9 | 精确注册；命令行/脚本/AI/菜单可调用；用法：insert <块名> <x,y> [旋转度] [缩放X] [缩放Y]；有 acad.pgp 别名入口 |
| `line` | 注册 src/core/Commands.cpp:3299 / 目录 src/core/CommandCatalog.cpp:38 | line x1,y1 x2,y2 | 两点绘制直线 | 60 | 精确注册；命令行/脚本/AI/菜单可调用；用法：line x1,y1 x2,y2；参数：x1,y1 起点；x2,y2 终点（点格式 x,y 或 x y 或 x,y,z）；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `line_2pt` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 line_* → LineModeCommand |
| `line_angle` | 目录 src/core/CommandCatalog.cpp:57 | line_angle x,y <角度> <长度> | 过点按角度与长度绘制直线 | 13 | 前缀分派 line_* → LineModeCommand；用法：line_angle x,y <角度> <长度>；AI system prompt 核心命令 |
| `line_angle_from_line` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_bisector` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_from_point_to_line` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_h` | 目录 src/core/CommandCatalog.cpp:55 | line_h x,y <长度> | 过点绘制水平直线 | 15 | 前缀分派 line_* → LineModeCommand；用法：line_h x,y <长度>；AI system prompt 核心命令 |
| `line_parallel` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 8 | 前缀分派 line_* → LineModeCommand |
| `line_parallel_pt` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 line_* → LineModeCommand |
| `line_rel_angle` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 line_* → LineModeCommand |
| `line_snake` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_snake_x` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_snake_y` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_tangent_cc` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 line_* → LineModeCommand |
| `line_tangent_pc` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_v` | 目录 src/core/CommandCatalog.cpp:56 | line_v x,y <长度> | 过点绘制竖直直线 | 14 | 前缀分派 line_* → LineModeCommand；用法：line_v x,y <长度>；AI system prompt 核心命令 |
| `mtext` | 注册 src/core/Commands.cpp:4605 / 目录 src/core/CommandCatalog.cpp:153 | mtext <x,y> <字高> <内容...> | 放置多行文字 | 18 | 精确注册；命令行/脚本/AI/菜单可调用；用法：mtext <x,y> <字高> <内容...>；有 acad.pgp 别名入口 |
| `perp` | 注册 src/core/Commands.cpp:3492 / 目录 src/core/CommandCatalog.cpp:102 | perp <直线Id> <x,y> | 过指定点作直线垂直于指定直线 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：perp <直线Id> <x,y> |
| `point` | 注册 src/core/Commands.cpp:3308 / 目录 src/core/CommandCatalog.cpp:49 | point <x,y> [x,y ...] | 绘制单个或多个点 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：point <x,y> [x,y ...]；AI system prompt 核心命令 |
| `poly` | 注册 src/core/Commands.cpp:3304 / 目录 src/core/CommandCatalog.cpp:43 | poly x1,y1 x2,y2 ... | 依次输入顶点绘制多边形（首末自动闭合） | 34 | 精确注册；命令行/脚本/AI/菜单可调用；用法：poly x1,y1 x2,y2 ...；AI system prompt 核心命令 |
| `poly_circumscribed` | 目录 src/core/CommandCatalog.cpp:45 | poly_circumscribed cx,cy apothem sides | 中心 + 边心距生成正多边形 | 8 | 前缀分派 poly_* → PolyModeCommand；用法：poly_circumscribed cx,cy apothem sides；参数：apothem 边心距（圆心到边中点距离）；sides 边数(≥3)；AI system prompt 核心命令 |
| `poly_edge` | 目录 src/core/CommandCatalog.cpp:46 | poly_edge x1,y1 x2,y2 sides | 给定一条边生成正多边形（边起终点 + 边数） | 9 | 前缀分派 poly_* → PolyModeCommand；用法：poly_edge x1,y1 x2,y2 sides；参数：x1,y1 x2,y2 一条边的两端点；sides 边数(≥3)；AI system prompt 核心命令 |
| `poly_regular` | 目录 src/core/CommandCatalog.cpp:44 | poly_regular cx,cy R sides \| poly_regular cx,cy side:L sides | 中心 + 外接圆半径（或边长）生成正多边形 | 11 | 前缀分派 poly_* → PolyModeCommand；用法：poly_regular cx,cy R sides \| poly_regular cx,cy side:L sides；参数：R 外接圆半径；side:L 用边长（推荐，LLM 原样转发提示词尺寸）；sides 边数(≥3)；AI system prompt 核心命令 |
| `poly_star` | 目录 src/core/CommandCatalog.cpp:47 | poly_star cx,cy outerR points [innerRatio] | 中心 + 外半径生成星形 | 8 | 前缀分派 poly_* → PolyModeCommand；用法：poly_star cx,cy outerR points [innerRatio]；参数：outerR 外半径；points 角数(≥3)；innerRatio 内半径比(默认0.382)；AI system prompt 核心命令 |
| `polyline` | 注册 src/core/Commands.cpp:4242 | — | 精确注册于 src/core/Commands.cpp:4242（处理器 PolylineModeCommand）；目录未收录，help 走回落 | 27 | 精确注册；命令行/脚本/AI/菜单可调用；有 acad.pgp 别名入口 |
| `qconnect` | 注册 src/core/Commands.cpp:3494 / 目录 src/core/CommandCatalog.cpp:104 | qconnect <目标圆Id> <x,y> | 从一点连到目标圆/弧最近的象限点 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：qconnect <目标圆Id> <x,y> |
| `qdim` | 注册 src/core/Commands.cpp:3474 / 目录 src/core/CommandCatalog.cpp:99 | qdim [baseline\|continue] | 框选平行线/圆阵列，一次性生成基线或连续标注串（复用 dim 基线/连续引擎） | 2 | 精确注册；命令行/脚本/AI/菜单可调用；用法：qdim [baseline\|continue]；参数：先框选一组平行线或圆阵列；baseline=共基点基线，continue=链式连续 |
| `rect` | 注册 src/core/Commands.cpp:3303 / 目录 src/core/CommandCatalog.cpp:42 | rect x0,y0 x1,y1 | 两角点绘制矩形 | 23 | 精确注册；命令行/脚本/AI/菜单可调用；用法：rect x0,y0 x1,y1；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `sheetframe` | 注册 src/core/Commands.cpp:4937 | — | 精确注册于 src/core/Commands.cpp:4937（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `spline` | 注册 src/core/Commands.cpp:3305 / 目录 src/core/CommandCatalog.cpp:48 | spline p1 p2 ... | 依次输入顶点绘制样条曲线 | 29 | 精确注册；命令行/脚本/AI/菜单可调用；用法：spline p1 p2 ... |
| `spline_from_polyline` | 目录 src/core/CommandCatalog.cpp:60 | spline_from_polyline <id> | 把折线图元转换为样条 | 14 | 前缀分派 spline_* → SplineModeCommand；用法：spline_from_polyline <id> |
| `spline_parabola_4p` | 目录 src/core/CommandCatalog.cpp:58 | spline_parabola_4p x1,y1 x2,y2 x3,y3 x4,y4 | 过四点构造抛物线样条 | 9 | 前缀分派 spline_* → SplineModeCommand；用法：spline_parabola_4p x1,y1 x2,y2 x3,y3 x4,y4 |
| `spline_through_points` | 目录 src/core/CommandCatalog.cpp:59 | spline_through_points x1,y1 x2,y2 ... | 依序通过给定点列构造样条 | 9 | 前缀分派 spline_* → SplineModeCommand；用法：spline_through_points x1,y1 x2,y2 ... |
| `tangent2` | 注册 src/core/Commands.cpp:3493 / 目录 src/core/CommandCatalog.cpp:103 | tangent2 <圆IdA> <圆IdB> | 求两圆/弧的全部公切线（外+内，0..4 条） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：tangent2 <圆IdA> <圆IdB> |
| `tbadd` | 注册 src/core/CuiAliasCommands.cpp:221 | — | 精确注册于 src/core/CuiAliasCommands.cpp:221（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `tbdel` | 注册 src/core/CuiAliasCommands.cpp:240 | — | 精确注册于 src/core/CuiAliasCommands.cpp:240（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `tbnew` | 注册 src/core/CuiAliasCommands.cpp:202 | — | 精确注册于 src/core/CuiAliasCommands.cpp:202（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `tbrm` | 注册 src/core/CuiAliasCommands.cpp:257 | — | 精确注册于 src/core/CuiAliasCommands.cpp:257（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `text` | 注册 src/core/Commands.cpp:4603 / 目录 src/core/CommandCatalog.cpp:151 | text <x,y> <字高> <内容...> | 放置单行文字 | 15 | 精确注册；命令行/脚本/AI/菜单可调用；用法：text <x,y> <字高> <内容...>；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `tol` | 注册 src/core/Commands.cpp:4818 / 目录 src/core/CommandCatalog.cpp:100 | tol <x,y> [公差文本] | 在指定位置放置形位公差框格 | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：tol <x,y> [公差文本]；有 acad.pgp 别名入口 |
| `wblock` | 注册 src/core/Commands.cpp:4559 / 目录 src/core/CommandCatalog.cpp:124 | wblock <输出.dxf> [id...] | 把图元/选择集导出为独立 DXF 文件 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：wblock <输出.dxf> [id...]；有 acad.pgp 别名入口 |
| `weld` | 注册 src/core/Commands.cpp:4998 | — | 精确注册于 src/core/Commands.cpp:4998（处理器 LambdaCommand）；目录未收录，help 走回落 | 13 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `wsdel` | 注册 src/core/CuiAliasCommands.cpp:185 | — | 精确注册于 src/core/CuiAliasCommands.cpp:185（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `wsnew` | 注册 src/core/CuiAliasCommands.cpp:132 | — | 精确注册于 src/core/CuiAliasCommands.cpp:132（处理器 LambdaCommand）；目录未收录，help 走回落 | 1 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `xh` | 注册 src/core/Commands.cpp:4610 / 目录 src/core/CommandCatalog.cpp:50 | xh [id ...] | 为选中对象生成序号球标并写入明细表（BOM） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：xh [id ...]；参数：先选中零部件几何或给定图元 id；自动按质心放置圆+编号球标并追加明细行；需先选中图元 |
| `xline` | 注册 src/core/Commands.cpp:3327 / 目录 src/core/CommandCatalog.cpp:51 | xline x,y <角度> \| xline x1,y1 x2,y2 | 无限长构造直线（过点+角度 / 两点 / 平行 / 平分） | 19 | 精确注册；命令行/脚本/AI/菜单可调用；用法：xline x,y <角度> \| xline x1,y1 x2,y2；参数：子模式：xline_h / xline_v / xline_angle / xline_bisector / xline_parallel；有 acad.pgp 别名入口 |
| `xref` | 注册 src/core/Commands.cpp:4246 / 目录 src/core/CommandCatalog.cpp:125 | xref attach <文件路径> [full\|relative\|none] [x,y] [旋转] [sx] [sy] \| detach <块名> \| list \| path <块名> | 外部参照的附加/分离/列表/路径诊断 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：xref attach <文件路径> [full\|relative\|none] [x,y] [旋转] [sx] [sy] \| detach <块名> \| list \| path <块名>；有 acad.pgp 别名入口 |

### B 编辑与几何变换（56 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `align` | 注册 src/core/Commands.cpp:3443 / 目录 src/core/CommandCatalog.cpp:76 | align <id> <源1> <目标1> [<源2> <目标2>] | 按源点/目标点对对齐并可选缩放 | 9 | 精确注册；命令行/脚本/AI/菜单可调用；用法：align <id> <源1> <目标1> [<源2> <目标2>] |
| `arraypath` | 注册 src/core/Commands.cpp:3449 / 目录 src/core/CommandCatalog.cpp:81 | arraypath <路径id> <间距> \| arraypath divide <路径id> <数量> | 沿 Polyline/Spline 路径阵列选中图元 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：arraypath <路径id> <间距> \| arraypath divide <路径id> <数量>；参数：作用于当前选择集；spacing=0 时用 divide 等分全长；需先选中图元 |
| `arraypolar` | 注册 src/core/Commands.cpp:3448 / 目录 src/core/CommandCatalog.cpp:80 | arraypolar <中心x,y> <个数> [填充角] | 选中图元绕中心环形阵列（基点默认捕捉圆心） | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：arraypolar <中心x,y> <个数> [填充角]；参数：作用于当前选择集；选中圆/弧时可省略中心=圆心；需先选中图元 |
| `arrayrect` | 注册 src/core/Commands.cpp:3447 / 目录 src/core/CommandCatalog.cpp:79 | arrayrect <行数> <列数> <行距> <列距> | 选中图元按行列矩形阵列 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：arrayrect <行数> <列数> <行距> <列距>；参数：作用于当前选择集；需先选中图元 |
| `break_at` | 注册 src/core/Commands.cpp:3438 / 目录 src/core/CommandCatalog.cpp:69 | break_at <id> <x,y> | 在指定点把图元一分为二 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：break_at <id> <x,y> |
| `chamfer` | 注册 src/core/Commands.cpp:3441 / 目录 src/core/CommandCatalog.cpp:74 | chamfer <id1> <id2> <距离1> <距离2\|角度> | 对两图元倒角（距离-距离 / 距离-角度） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：chamfer <id1> <id2> <距离1> <距离2\|角度> |
| `clear` | 注册 src/core/Commands.cpp:3455 / 目录 src/core/CommandCatalog.cpp:86 | clear | 清空图中全部图元 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：clear；AI system prompt 核心命令 |
| `copy_dup` | 注册 src/core/Commands.cpp:3430 | — | 精确注册于 src/core/Commands.cpp:3430（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 2 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `copybase` | 注册 src/core/Commands.cpp:4224 / 目录 src/core/CommandCatalog.cpp:127 | copybase <x,y> | 复制但基点由用户指定（AutoCAD COPYBASE 语义） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：copybase <x,y>；快捷键 Ctrl+Shift+C |
| `copyclip` | 注册 src/core/Commands.cpp:4223 / 目录 src/core/CommandCatalog.cpp:126 | copyclip | 把当前选择集写入内部剪贴板（基点 = 包围盒左下角） | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：copyclip；快捷键 Ctrl+C |
| `delete` | 注册 src/core/Commands.cpp:3339 / 目录 src/core/CommandCatalog.cpp:83 | delete [id...] | 删除指定图元（或当前选择集） | 19 | 精确注册；命令行/脚本/AI/菜单可调用；用法：delete [id...]；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `divide` | 注册 src/core/Commands.cpp:3439 / 目录 src/core/CommandCatalog.cpp:70 | divide <id> <n> | 把图元等分为 n 段 | 9 | 精确注册；命令行/脚本/AI/菜单可调用；用法：divide <id> <n> |
| `duplicate` | 注册 src/core/Commands.cpp:3429 | — | 精确注册于 src/core/Commands.cpp:3429（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 11 | 精确注册；命令行/脚本/AI/菜单可调用；有 acad.pgp 别名入口 |
| `explode` | 注册 src/core/Commands.cpp:3442 / 目录 src/core/CommandCatalog.cpp:75 | explode <id> | 把块引用/复合图元分解为独立图元 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：explode <id>；有 acad.pgp 别名入口 |
| `extend` | 别名 src/core/AliasTable.cpp:30 | — | 经别名 / 外部注册解析；目录未收录 | 7 | 非精确注册；经家族补全、别名或外部注册解析；有 acad.pgp 别名入口 |
| `fillet` | 注册 src/core/Commands.cpp:3440 / 目录 src/core/CommandCatalog.cpp:73 | fillet <id1> <id2> <半径> | 对两图元倒圆角 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：fillet <id1> <id2> <半径>；有 acad.pgp 别名入口 |
| `group` | 注册 src/core/Commands.cpp:3651 / 目录 src/core/CommandCatalog.cpp:158 | group [create <名> \| add <名> \| remove \| list \| ungroup <名>] | 编组的创建/加成员/移除/列表/解组 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：group [create <名> \| add <名> \| remove \| list \| ungroup <名>]；有 acad.pgp 别名入口 |
| `join` | 注册 src/core/Commands.cpp:3446 / 目录 src/core/CommandCatalog.cpp:78 | join [容差mm] | 把同层首尾相接的 Line/Arc 连合为 Polyline | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：join [容差mm]；参数：作用于当前选择集；容差缺省 0.5mm |
| `lengthen` | 注册 src/core/Commands.cpp:3436 / 目录 src/core/CommandCatalog.cpp:72 | lengthen <id> <参数> | 按增量/百分比拉长或缩短图元 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：lengthen <id> <参数> |
| `matchprop` | 注册 src/core/Commands.cpp:4431 / 目录 src/core/CommandCatalog.cpp:82 | matchprop <源id> <目标id...> | 把源图元的图层/颜色/线宽/线型复制到目标 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：matchprop <源id> <目标id...>；有 acad.pgp 别名入口 |
| `measure` | 注册 src/core/Commands.cpp:4766 / 目录 src/core/CommandCatalog.cpp:71 | measure <id> <间距> | 按间距在图元上放置等分点 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：measure <id> <间距>；有 acad.pgp 别名入口 |
| `mirror` | 注册 src/core/Commands.cpp:3433 / 目录 src/core/CommandCatalog.cpp:66 | mirror x1,y1 x2,y2 [delete源] | 沿镜像轴复制/翻转选中图元 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：mirror x1,y1 x2,y2 [delete源]；需先选中图元；有 acad.pgp 别名入口 |
| `move` | 注册 src/core/Commands.cpp:3427 / 目录 src/core/CommandCatalog.cpp:62 | move <dx> <dy> [copy] | 平移选中图元 | 15 | 精确注册；命令行/脚本/AI/菜单可调用；用法：move <dx> <dy> [copy]；需先选中图元；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `move_copy` | 注册 src/core/Commands.cpp:3428 / 目录 src/core/CommandCatalog.cpp:63 | move_copy <dx> <dy> | 移动的复制版：原对象保留 | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：move_copy <dx> <dy> |
| `offset` | 注册 src/core/Commands.cpp:3437 / 目录 src/core/CommandCatalog.cpp:67 | offset <id> <距离> [方向点x,y] | 按距离偏移直线/圆/弧/折线 | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：offset <id> <距离> [方向点x,y]；有 acad.pgp 别名入口 |
| `overkill` | 注册 src/core/Commands.cpp:4529 / 目录 src/core/CommandCatalog.cpp:150 | overkill | 合并/删除重复或重叠图元 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：overkill；有 acad.pgp 别名入口 |
| `pasteblock` | 注册 src/core/Commands.cpp:4226 / 目录 src/core/CommandCatalog.cpp:129 | pasteblock <x,y> | 把剪贴板内容整包收为块定义并放置引用 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：pasteblock <x,y>；快捷键 Ctrl+Shift+V |
| `pasteclip` | 注册 src/core/Commands.cpp:4225 / 目录 src/core/CommandCatalog.cpp:128 | pasteclip [x,y] | 把剪贴板内容粘贴为独立图元 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：pasteclip [x,y]；快捷键 Ctrl+V |
| `pedit` | 注册 src/core/Commands.cpp:4347 / 目录 src/core/CommandCatalog.cpp:147 | pedit <id> [close\|open\|width <线宽>\|join <id...>\|fit\|spline] | 折线的闭合/线宽/连合/拟合/样条化 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：pedit <id> [close\|open\|width <线宽>\|join <id...>\|fit\|spline]；有 acad.pgp 别名入口 |
| `polyline_add_node` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_append_node` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_arcs_to_chords` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_change_segment_type` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_delete_between` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_delete_node` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_equidistant` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 10 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_from_segments` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 10 | 前缀分派 polyline_* → PolylineModeCommand |
| `polyline_trim_segments` | 家族 src/core/CommandInterpreter.cpp:46 (polylineCommandNames) | — | polyline_* 家族子命令（PolylineModeCommand 分派）；目录未收录，help 走回落 | 9 | 前缀分派 polyline_* → PolylineModeCommand |
| `purge` | 注册 src/core/Commands.cpp:4485 / 目录 src/core/CommandCatalog.cpp:149 | purge [all\|layers\|blocks\|dimstyles] | 清理未引用的图层/块/标注样式 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：purge [all\|layers\|blocks\|dimstyles]；有 acad.pgp 别名入口 |
| `redo` | 注册 src/core/Commands.cpp:3453 / 目录 src/core/CommandCatalog.cpp:85 | redo | 恢复被撤销的编辑 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：redo；快捷键 Ctrl+Y；AI system prompt 核心命令 |
| `revert` | 注册 src/core/Commands.cpp:3434 | — | 精确注册于 src/core/Commands.cpp:3434（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 2 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `revert_direction` | 注册 src/core/Commands.cpp:3435 | — | 精确注册于 src/core/Commands.cpp:3435（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 4 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `rotate` | 注册 src/core/Commands.cpp:3431 / 目录 src/core/CommandCatalog.cpp:64 | rotate <基点x,y> <角度> [copy] | 绕基点旋转选中图元 | 13 | 精确注册；命令行/脚本/AI/菜单可调用；用法：rotate <基点x,y> <角度> [copy]；参数：角度为度，逆时针为正；需先选中图元；AI system prompt 核心命令；有 acad.pgp 别名入口 |
| `scale` | 注册 src/core/Commands.cpp:3432 / 目录 src/core/CommandCatalog.cpp:65 | scale <基点x,y> <比例> [copy] | 绕基点缩放选中图元 | 21 | 精确注册；命令行/脚本/AI/菜单可调用；用法：scale <基点x,y> <比例> [copy]；需先选中图元；有 acad.pgp 别名入口 |
| `spline_append_pt` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 12 | 前缀分派 spline_* → SplineModeCommand |
| `spline_explode` | 目录 src/core/CommandCatalog.cpp:61 | spline_explode <id> | 把样条打散为折线/线段 | 13 | 前缀分派 spline_* → SplineModeCommand；用法：spline_explode <id> |
| `spline_hyperbola` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 spline_* → SplineModeCommand |
| `spline_insert_pts` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 11 | 前缀分派 spline_* → SplineModeCommand |
| `spline_parabola` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 spline_* → SplineModeCommand |
| `spline_parabola_fd` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 7 | 前缀分派 spline_* → SplineModeCommand |
| `spline_remove_between` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 11 | 前缀分派 spline_* → SplineModeCommand |
| `spline_remove_pts` | 家族 src/core/CommandInterpreter.cpp:26 (splineCommandNames) | — | spline_* 家族子命令（SplineModeCommand 分派）；目录未收录，help 走回落 | 11 | 前缀分派 spline_* → SplineModeCommand |
| `stretch` | 注册 src/core/Commands.cpp:3444 / 目录 src/core/CommandCatalog.cpp:77 | stretch <id> <参数> | 拉伸选中图元的指定部分 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：stretch <id> <参数>；需先选中图元 |
| `tk` | 注册 src/core/Commands.cpp:3597 / 目录 src/core/CommandCatalog.cpp:145 | tk <x,y> | 放置临时追踪点 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：tk <x,y> |
| `trim` | 注册 src/core/Commands.cpp:3340 / 目录 src/core/CommandCatalog.cpp:68 | trim <id> <x,y> [边界id...] | 以边界修剪图元（无边界 = 全体为界） | 13 | 精确注册；命令行/脚本/AI/菜单可调用；用法：trim <id> <x,y> [边界id...]；参数：x,y 为拾取点（点选被剪一侧）；有 acad.pgp 别名入口 |
| `undo` | 注册 src/core/Commands.cpp:3451 / 目录 src/core/CommandCatalog.cpp:84 | undo | 撤销上一步编辑 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：undo；快捷键 Ctrl+Z；AI system prompt 核心命令 |

### C 样式/图层/线型（11 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `chain` | 注册 src/core/Commands.cpp:3467 / 目录 src/core/CommandCatalog.cpp:139 | chain [on\|off] | 连续绘制开关（上一图元终点为下一图元起点） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：chain [on\|off]；快捷键 K |
| `color` | 注册 src/core/Commands.cpp:3466 / 目录 src/core/CommandCatalog.cpp:138 | color #RRGGBB\|颜色名 | 设置后续绘制颜色 | 15 | 精确注册；命令行/脚本/AI/菜单可调用；用法：color #RRGGBB\|颜色名 |
| `hatchedit` | 注册 src/core/Commands.cpp:4049 | — | 精确注册于 src/core/Commands.cpp:4049（处理器 LambdaCommand）；目录未收录，help 走回落 | 2 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `layer` | 注册 src/core/Commands.cpp:3974 / 目录 src/core/CommandCatalog.cpp:112 | layer add <名称> [颜色] [线宽] [线型] \| delete \| rename \| visible \| lock \| set \| assign \| apply \| list | 图层的增删改/显隐/锁定/赋层/套用/列表（单一分派命令） | 14 | 精确注册；命令行/脚本/AI/菜单可调用；用法：layer add <名称> [颜色] [线宽] [线型] \| delete \| rename \| visible \| lock \| set \| assign \| apply \| list；有 acad.pgp 别名入口 |
| `layiso` | 注册 src/core/Commands.cpp:3997 / 目录 src/core/CommandCatalog.cpp:113 | layiso | 只保留选中图元所在图层可见 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：layiso；需先选中图元；有 acad.pgp 别名入口 |
| `laymch` | 注册 src/core/Commands.cpp:4027 / 目录 src/core/CommandCatalog.cpp:115 | laymch <目标图层名> | 把选中图元移到目标图层 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：laymch <目标图层名>；需先选中图元；有 acad.pgp 别名入口 |
| `layuniso` | 注册 src/core/Commands.cpp:4015 / 目录 src/core/CommandCatalog.cpp:114 | layuniso | 恢复被隔离图层的显示 | 9 | 精确注册；命令行/脚本/AI/菜单可调用；用法：layuniso |
| `linetype` | 注册 src/core/Commands.cpp:4734 / 目录 src/core/CommandCatalog.cpp:156 | linetype | 线型的加载/设置/列表 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：linetype；有 acad.pgp 别名入口 |
| `ltscale` | 注册 src/core/Commands.cpp:3979 / 目录 src/core/CommandCatalog.cpp:116 | ltscale <正数> | 设置全局线型比例（单一 LTSCALE） | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：ltscale <正数>；有 acad.pgp 别名入口 |
| `toggle_layer` | 注册 src/core/Commands.cpp:3637 | — | 精确注册于 src/core/Commands.cpp:3637（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 10 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `width` | 注册 src/core/Commands.cpp:3465 / 目录 src/core/CommandCatalog.cpp:137 | width 1\|2\|3\|5 | 设置后续绘制线宽档位 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：width 1\|2\|3\|5 |

### D 坐标/视图/刷新（20 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `aliassave` | 注册 src/core/CuiAliasCommands.cpp:69 / 目录 src/core/CommandCatalog.cpp:167 | aliassave [\"<路径>\"] | 把当前别名表写回 .pgp 文件 | 2 | 精确注册；命令行/脚本/AI/菜单可调用；用法：aliassave [\"<路径>\"] |
| `cuisave` | 注册 src/core/CuiAliasCommands.cpp:119 / 目录 src/core/CommandCatalog.cpp:170 | cuisave \"<路径>\" | 保存界面定制文件 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：cuisave \"<路径>\" |
| `dxfout` | 注册 src/core/Commands.cpp:4250 / 目录 src/core/CommandCatalog.cpp:157 | dxfout <路径.dxf> [-v R12\|2000] | 把当前图导出为 DXF 文件（可选版本） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dxfout <路径.dxf> [-v R12\|2000] |
| `dyn` | 注册 src/core/Commands.cpp:3540 / 目录 src/core/CommandCatalog.cpp:144 | dyn [on\|off] | 动态输入开关 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dyn [on\|off]；快捷键 F12 |
| `grid` | 注册 src/core/Commands.cpp:3461 / 目录 src/core/CommandCatalog.cpp:132 | grid [on\|off] | 栅格显示开关 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：grid [on\|off]；快捷键 F9 |
| `layout` | 注册 src/core/Commands.cpp:3755 / 目录 src/core/CommandCatalog.cpp:134 | layout new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw | 布局的增删改/切换/视口比例/冻结（单一分派命令） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：layout new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw |
| `line_orthogonal` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_orthogonal_from_line` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `line_tangent_ortho` | 家族 src/core/CommandInterpreter.cpp:14 (lineSubModeNames) | — | line_* 家族子命令（LineModeCommand 分派）；目录未收录，help 走回落 | 6 | 前缀分派 line_* → LineModeCommand |
| `macrosave` | 注册 src/core/MacroScript.cpp:317 / 目录 src/core/CommandCatalog.cpp:161 | macrosave \"<路径>\" | 把录制的宏保存为文件 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：macrosave \"<路径>\" |
| `mview` | 注册 src/core/Commands.cpp:3918 / 目录 src/core/CommandCatalog.cpp:135 | mview <图纸X> <图纸Y> <宽> <高> [比例] | 在当前布局图纸坐标建矩形视口 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：mview <图纸X> <图纸Y> <宽> <高> [比例] |
| `ortho` | 注册 src/core/Commands.cpp:3499 / 目录 src/core/CommandCatalog.cpp:140 | ortho [on\|off] | 正交模式开关 | 2 | 精确注册；命令行/脚本/AI/菜单可调用；用法：ortho [on\|off]；快捷键 F8 |
| `otrack` | 注册 src/core/Commands.cpp:3580 / 目录 src/core/CommandCatalog.cpp:143 | otrack [on\|off] | 对象捕捉追踪开关 | 2 | 精确注册；命令行/脚本/AI/菜单可调用；用法：otrack [on\|off]；快捷键 F11 |
| `plane` | 注册 src/core/Commands.cpp:3459 / 目录 src/core/CommandCatalog.cpp:130 | plane xy\|yz\|xz | 切换绘图平面 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：plane xy\|yz\|xz |
| `polar` | 注册 src/core/Commands.cpp:3557 / 目录 src/core/CommandCatalog.cpp:142 | polar [on\|off\|<增量角>] | 极轴追踪开关或设置增量角 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：polar [on\|off\|<增量角>]；快捷键 F10 |
| `regen` | 注册 src/core/Commands.cpp:4834 / 目录 src/core/CommandCatalog.cpp:133 | regen | 重建空间索引并重绘视图 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：regen；快捷键 F5；有 acad.pgp 别名入口 |
| `snap` | 注册 src/core/Commands.cpp:3514 / 目录 src/core/CommandCatalog.cpp:141 | snap [on\|off\|<步长mm>] | 捕捉开关或设置步长 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：snap [on\|off\|<步长mm>] |
| `tkclear` | 注册 src/core/Commands.cpp:3612 / 目录 src/core/CommandCatalog.cpp:146 | tkclear | 清除全部临时追踪点 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：tkclear |
| `ucs` | 注册 src/core/Commands.cpp:3460 / 目录 src/core/CommandCatalog.cpp:136 | ucs world\|view\|3p <p1> <p2> [p3]\|object <id>\|z <x,y> <角>\|origin <x,y> [角]\|save/set/restore/delete/list\|coords wcs\|ucs | UCS 的定义/切换/命名管理（单一分派命令） | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：ucs world\|view\|3p <p1> <p2> [p3]\|object <id>\|z <x,y> <角>\|origin <x,y> [角]\|save/set/restore/delete/list\|coords wcs\|ucs |
| `zoom` | 注册 src/core/Commands.cpp:3462 / 目录 src/core/CommandCatalog.cpp:131 | zoom <倍率> | 按倍率缩放视图 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：zoom <倍率>；AI system prompt 核心命令；有 acad.pgp 别名入口 |

### E 选择与捕捉（12 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `deselect_entity` | 注册 src/core/Commands.cpp:3630 / 目录 src/core/CommandCatalog.cpp:109 | deselect_entity <id> | 把指定 id 移出选择集 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：deselect_entity <id> |
| `deselect_intersected` | 注册 src/core/Commands.cpp:3634 | — | 精确注册于 src/core/Commands.cpp:3634（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 9 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `deselect_layer` | 注册 src/core/Commands.cpp:3636 / 目录 src/core/CommandCatalog.cpp:111 | deselect_layer <图层名> | 把指定图层图元移出选择集 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：deselect_layer <图层名> |
| `filter` | 注册 src/core/Commands.cpp:3696 / 目录 src/core/CommandCatalog.cpp:95 | filter [type=类型] [layer=图层] ... \| none | 创建/应用命名选择过滤器 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：filter [type=类型] [layer=图层] ... \| none；有 acad.pgp 别名入口 |
| `invert` | 注册 src/core/Commands.cpp:3628 / 目录 src/core/CommandCatalog.cpp:107 | invert | 选择集取反 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：invert |
| `qselect` | 注册 src/core/Commands.cpp:3642 / 目录 src/core/CommandCatalog.cpp:94 | qselect [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型] | 按类型/图层/颜色/线型四维过滤并选中图元 | 11 | 精确注册；命令行/脚本/AI/菜单可调用；用法：qselect [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型]；参数：任一维省略 = 不过滤该维；全空 = 仅提示；需先选中图元；有 acad.pgp 别名入口 |
| `select_all` | 注册 src/core/Commands.cpp:3626 / 目录 src/core/CommandCatalog.cpp:105 | select_all | 选中图中全部图元 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：select_all；需先选中图元；快捷键 Ctrl+A；AI system prompt 核心命令 |
| `select_contour` | 注册 src/core/Commands.cpp:3631 | — | 精确注册于 src/core/Commands.cpp:3631（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 6 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `select_entity` | 注册 src/core/Commands.cpp:3629 / 目录 src/core/CommandCatalog.cpp:108 | select_entity <id> | 把指定 id 加入选择集 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：select_entity <id> |
| `select_intersected` | 注册 src/core/Commands.cpp:3633 | — | 精确注册于 src/core/Commands.cpp:3633（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 9 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `select_layer` | 注册 src/core/Commands.cpp:3635 / 目录 src/core/CommandCatalog.cpp:110 | select_layer <图层名> | 选中指定图层上的全部图元 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：select_layer <图层名>；需先选中图元 |
| `select_none` | 注册 src/core/Commands.cpp:3627 / 目录 src/core/CommandCatalog.cpp:106 | select_none | 清空当前选择集 | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：select_none；快捷键 Esc；AI system prompt 核心命令 |

### F 辅助/查询/系统（25 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `alias` | 注册 src/core/CuiAliasCommands.cpp:14 / 目录 src/core/CommandCatalog.cpp:164 | alias <别名> <完整命令> | 新增/覆盖命令别名（acad.pgp 兼容） | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：alias <别名> <完整命令> |
| `aliaslist` | 注册 src/core/CuiAliasCommands.cpp:82 / 目录 src/core/CommandCatalog.cpp:168 | aliaslist | 列出当前全部别名 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：aliaslist |
| `aliasload` | 注册 src/core/CuiAliasCommands.cpp:51 / 目录 src/core/CommandCatalog.cpp:166 | aliasload \"<路径>\" | 从指定 .pgp 文件加载别名 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：aliasload \"<路径>\" |
| `aliasreload` | 注册 src/core/CuiAliasCommands.cpp:28 / 目录 src/core/CommandCatalog.cpp:165 | aliasreload | 从 .pgp 文件重新加载别名表 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：aliasreload |
| `area` | 注册 src/core/Commands.cpp:3345 / 目录 src/core/CommandCatalog.cpp:89 | area <封闭图元id> \| x1,y1 x2,y2 [x3,y3...] | 测量封闭图元或多点围成区域面积 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：area <封闭图元id> \| x1,y1 x2,y2 [x3,y3...] |
| `checkcompliance` | 注册 src/core/Commands.cpp:4930 / 目录 src/core/CommandCatalog.cpp:176 | checkcompliance | 与 stdcheck 相同的工程制图合规检查，供 AI 助手 checkCompliance 经 runSketchCommand 调用 | 4 | 精确注册；命令行/脚本/AI/菜单可调用；用法：checkcompliance；参数：AI 工具派发 runSketchCommand(\"checkcompliance\") 经 CommandInterpreter 触发同一检查逻辑 |
| `cuiload` | 注册 src/core/CuiAliasCommands.cpp:98 / 目录 src/core/CommandCatalog.cpp:169 | cuiload \"<路径>\" | 加载界面定制文件（CUIx） | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：cuiload \"<路径>\" |
| `ddedit` | 注册 src/core/Commands.cpp:4651 / 目录 src/core/CommandCatalog.cpp:154 | ddedit <id> <新内容> | 修改既有文字内容 | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：ddedit <id> <新内容>；有 acad.pgp 别名入口 |
| `dist` | 注册 src/core/Commands.cpp:3344 / 目录 src/core/CommandCatalog.cpp:88 | dist x1,y1 x2,y2 | 测量两点距离与角度 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：dist x1,y1 x2,y2 |
| `fieldupdate` | 注册 src/core/Commands.cpp:3353 / 目录 src/core/CommandCatalog.cpp:92 | fieldupdate | 全模型重算 FIELD 域（MText/Table 预热求值，异常回落 ###） | 7 | 精确注册；命令行/脚本/AI/菜单可调用；用法：fieldupdate；参数：无参数；打开文档自动刷新见选项 fields/autoUpdateOnOpen |
| `help` | 注册 src/core/Commands.cpp:4843 / 目录 src/core/CommandCatalog.cpp:173 | help [<命令名>] | 命令总表；help <命令> 查看用法/参数/样例 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：help [<命令名>]；快捷键 F1；有 acad.pgp 别名入口 |
| `id` | 注册 src/core/Commands.cpp:3347 / 目录 src/core/CommandCatalog.cpp:91 | id x,y | 显示指定点坐标 | 17 | 精确注册；命令行/脚本/AI/菜单可调用；用法：id x,y |
| `list` | 注册 src/core/Commands.cpp:3341 / 目录 src/core/CommandCatalog.cpp:87 | list [id...] | 列出图元的类型/图层/几何信息 | 12 | 精确注册；命令行/脚本/AI/菜单可调用；用法：list [id...]；参数：无 id = 全部图元 |
| `macroplay` | 注册 src/core/MacroScript.cpp:321 / 目录 src/core/CommandCatalog.cpp:162 | macroplay \"<路径>\" | 回放已保存的宏 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：macroplay \"<路径>\" |
| `macrorec` | 注册 src/core/MacroScript.cpp:313 / 目录 src/core/CommandCatalog.cpp:159 | macrorec | 开始录制命令宏 | 11 | 精确注册；命令行/脚本/AI/菜单可调用；用法：macrorec |
| `macroshow` | 注册 src/core/MacroScript.cpp:319 | — | 精确注册于 src/core/MacroScript.cpp:319（处理器 LambdaCommand）；目录未收录，help 走回落 | 3 | 精确注册；命令行/脚本/AI/菜单可调用 |
| `macrostop` | 注册 src/core/MacroScript.cpp:315 / 目录 src/core/CommandCatalog.cpp:160 | macrostop | 结束宏录制 | 10 | 精确注册；命令行/脚本/AI/菜单可调用；用法：macrostop |
| `massprop` | 注册 src/core/Commands.cpp:3346 / 目录 src/core/CommandCatalog.cpp:90 | massprop <封闭图元id> ... | 封闭区域的面积/质心/惯性矩等 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：massprop <封闭图元id> ... |
| `prop` | 注册 src/core/Commands.cpp:3370 / 目录 src/core/CommandCatalog.cpp:93 | prop [<名称> [<值>] \| -r <名称>] | 自定义图纸特性的设置/查询/删除（PROP 域数据源） | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：prop [<名称> [<值>] \| -r <名称>]；参数：无参 = 列出全部；MText/Table 中以 %<PROP:名称>% 引用 |
| `properties` | 注册 src/core/Commands.cpp:4462 / 目录 src/core/CommandCatalog.cpp:148 | properties（先选中）或 properties <id...> | 打开特性面板编辑选中图元 | 24 | 精确注册；命令行/脚本/AI/菜单可调用；用法：properties（先选中）或 properties <id...>；需先选中图元；快捷键 Ctrl+1；有 acad.pgp 别名入口 |
| `scr` | 注册 src/core/MacroScript.cpp:323 / 目录 src/core/CommandCatalog.cpp:163 | scr \"<路径>\" | 逐行执行脚本文件中的命令 | 6 | 精确注册；命令行/脚本/AI/菜单可调用；用法：scr \"<路径>\" |
| `standards` | 注册 src/core/Commands.cpp:4851 / 目录 src/core/CommandCatalog.cpp:174 | standards [<表名> [<键名>]] | 查询内嵌的 12 类机械制图国标数据（图幅/比例/字体/线型/粗糙度/公差/形位公差/螺纹/齿轮/轴承/焊缝/CAD 图层），数据源自《机械制图手册（第四版）》 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：standards [<表名> [<键名>]]；参数：无参数 = 列出 12 类表名；standards <表名> = 列出该表全部键值；standards <表名> <键名> = 查询单值 |
| `stdcheck` | 注册 src/core/Commands.cpp:4929 / 目录 src/core/CommandCatalog.cpp:175 | stdcheck | 依据 7 类 GB/T 规范（线宽粗细比/两线最小间隙/字号下限/粗糙度符号/焊缝符号完整性/幅面留边/图层命名）检查当前图纸，只读诊断，不改图 | 8 | 精确注册；命令行/脚本/AI/菜单可调用；用法：stdcheck；参数：覆盖 GB/T 14665/14689/131/324 与 GB/T 18617/ISO 13567；仅报告不合规项、建议与跳转，不强制修改 |
| `wslist` | 注册 src/core/CuiAliasCommands.cpp:169 / 目录 src/core/CommandCatalog.cpp:171 | wslist | 列出可用工作空间 | 5 | 精确注册；命令行/脚本/AI/菜单可调用；用法：wslist |
| `wsload` | 注册 src/core/CuiAliasCommands.cpp:150 / 目录 src/core/CommandCatalog.cpp:172 | wsload <名> | 切换到指定工作空间 | 3 | 精确注册；命令行/脚本/AI/菜单可调用；用法：wsload <名> |

### G 其他（影响渲染流程）（1 项）

| 命令名 | 所在位置 | 主要参数 | 作用说明 | 出现次数 | 调用条件 |
|---|---|---|---|---:|---|
| `toggle_contour` | 注册 src/core/Commands.cpp:3632 | — | 精确注册于 src/core/Commands.cpp:3632（处理器 共享Lambda/变量）；目录未收录，help 走回落 | 10 | 精确注册；命令行/脚本/AI/菜单可调用 |

## 4. 前缀分派家族（动态展开，不逐一注册）

| 前缀 | 注册位置 | 处理器 | 说明 |
|---|---|---|---|
| `line_*` | src/core/Commands.cpp:3324 | `LineModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `xline_*` | src/core/Commands.cpp:3328 | `XlineCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `arc_*` | src/core/Commands.cpp:3331 | `ArcModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `poly_*` | src/core/Commands.cpp:3334 | `PolyModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `rect_chamfer*` | src/core/Commands.cpp:3335 | `PolyModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `rect_fillet*` | src/core/Commands.cpp:3336 | `PolyModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `spline_*` | src/core/Commands.cpp:4234 | `SplineModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `ellipse_*` | src/core/Commands.cpp:4239 | `EllipseModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |
| `polyline_*` | src/core/Commands.cpp:4243 | `PolylineModeCommand` | 任意以该前缀开头的命令串均由同一处理器分派，故存在**无限多个合法命令名**，不能逐一穷举 |

补全家族（`CommandInterpreter::commandNames()` 追加，仅用于 Tab 补全/命令面板）：

- **lineSubModeNames**（src/core/CommandInterpreter.cpp:14，18 项）：`line_2pt`, `line_h`, `line_v`, `line_angle`, `line_parallel`, `line_parallel_pt`, `line_bisector`, `line_tangent_pc`, `line_tangent_cc`, `line_tangent_ortho`, `line_orthogonal`, `line_rel_angle`, `line_snake`, `line_snake_x`, `line_snake_y`, `line_angle_from_line`, `line_orthogonal_from_line`, `line_from_point_to_line`
- **splineCommandNames**（src/core/CommandInterpreter.cpp:26，12 项）：`spline_parabola`, `spline_hyperbola`, `spline_parabola_4p`, `spline_parabola_fd`, `spline_through_points`, `freehand_line`, `spline_from_polyline`, `spline_append_pt`, `spline_insert_pts`, `spline_remove_pts`, `spline_remove_between`, `spline_explode`
- **ellipseCommandNames**（src/core/CommandInterpreter.cpp:37，6 项）：`ellipse_1pt`, `ellipse_axis`, `ellipse_foci`, `ellipse_4p`, `ellipse_c3p`, `ellipse_inscribed`
- **polylineCommandNames**（src/core/CommandInterpreter.cpp:46，10 项）：`polyline`, `polyline_add_node`, `polyline_append_node`, `polyline_delete_node`, `polyline_delete_between`, `polyline_trim_segments`, `polyline_equidistant`, `polyline_from_segments`, `polyline_arcs_to_chords`, `polyline_change_segment_type`
- **selectCommandNames**（src/core/CommandInterpreter.cpp:58，12 项）：`select_all`, `select_none`, `invert`, `select_entity`, `deselect_entity`, `select_contour`, `toggle_contour`, `select_intersected`, `deselect_intersected`, `select_layer`, `deselect_layer`, `toggle_layer`

## 5. acad.pgp 别名映射（66 条）

| 别名 | 目标命令 | 位置 |
|---|---|---|
| `l` | `line` | src/core/AliasTable.cpp:18 |
| `c` | `circle` | src/core/AliasTable.cpp:19 |
| `a` | `arc` | src/core/AliasTable.cpp:20 |
| `rec` | `rect` | src/core/AliasTable.cpp:21 |
| `pl` | `polyline` | src/core/AliasTable.cpp:22 |
| `pli` | `polyline` | src/core/AliasTable.cpp:23 |
| `e` | `delete` | src/core/AliasTable.cpp:24 |
| `m` | `move` | src/core/AliasTable.cpp:25 |
| `co` | `duplicate` | src/core/AliasTable.cpp:26 |
| `cp` | `duplicate` | src/core/AliasTable.cpp:27 |
| `ro` | `rotate` | src/core/AliasTable.cpp:28 |
| `tr` | `trim` | src/core/AliasTable.cpp:29 |
| `ex` | `extend` | src/core/AliasTable.cpp:30 |
| `mi` | `mirror` | src/core/AliasTable.cpp:31 |
| `o` | `offset` | src/core/AliasTable.cpp:32 |
| `f` | `fillet` | src/core/AliasTable.cpp:33 |
| `sc` | `scale` | src/core/AliasTable.cpp:34 |
| `z` | `zoom` | src/core/AliasTable.cpp:35 |
| `la` | `layer` | src/core/AliasTable.cpp:36 |
| `b` | `block` | src/core/AliasTable.cpp:37 |
| `x` | `explode` | src/core/AliasTable.cpp:38 |
| `q` | `qselect` | src/core/AliasTable.cpp:39 |
| `dt` | `text` | src/core/AliasTable.cpp:41 |
| `tx` | `text` | src/core/AliasTable.cpp:42 |
| `dtext` | `text` | src/core/AliasTable.cpp:43 |
| `mt` | `mtext` | src/core/AliasTable.cpp:44 |
| `t` | `mtext` | src/core/AliasTable.cpp:45 |
| `ed` | `ddedit` | src/core/AliasTable.cpp:46 |
| `h` | `hatch` | src/core/AliasTable.cpp:48 |
| `hat` | `hatch` | src/core/AliasTable.cpp:49 |
| `bh` | `bhatch` | src/core/AliasTable.cpp:50 |
| `bha` | `bhatch` | src/core/AliasTable.cpp:51 |
| `pe` | `pedit` | src/core/AliasTable.cpp:52 |
| `ped` | `pedit` | src/core/AliasTable.cpp:53 |
| `ma` | `matchprop` | src/core/AliasTable.cpp:54 |
| `mat` | `matchprop` | src/core/AliasTable.cpp:55 |
| `pu` | `purge` | src/core/AliasTable.cpp:56 |
| `pur` | `purge` | src/core/AliasTable.cpp:57 |
| `ok` | `overkill` | src/core/AliasTable.cpp:58 |
| `ove` | `overkill` | src/core/AliasTable.cpp:59 |
| `w` | `wblock` | src/core/AliasTable.cpp:60 |
| `wbl` | `wblock` | src/core/AliasTable.cpp:61 |
| `i` | `insert` | src/core/AliasTable.cpp:62 |
| `ins` | `insert` | src/core/AliasTable.cpp:63 |
| `pr` | `properties` | src/core/AliasTable.cpp:64 |
| `pro` | `properties` | src/core/AliasTable.cpp:65 |
| `ch` | `properties` | src/core/AliasTable.cpp:66 |
| `lt` | `linetype` | src/core/AliasTable.cpp:67 |
| `lin` | `linetype` | src/core/AliasTable.cpp:68 |
| `lts` | `ltscale` | src/core/AliasTable.cpp:69 |
| `me` | `measure` | src/core/AliasTable.cpp:70 |
| `mea` | `measure` | src/core/AliasTable.cpp:71 |
| `re` | `regen` | src/core/AliasTable.cpp:72 |
| `reg` | `regen` | src/core/AliasTable.cpp:73 |
| `tol` | `tol` | src/core/AliasTable.cpp:74 |
| `fi` | `filter` | src/core/AliasTable.cpp:76 |
| `fli` | `filter` | src/core/AliasTable.cpp:77 |
| `g` | `group` | src/core/AliasTable.cpp:78 |
| `grp` | `group` | src/core/AliasTable.cpp:79 |
| `lay` | `layer` | src/core/AliasTable.cpp:80 |
| `laym` | `laymch` | src/core/AliasTable.cpp:81 |
| `layiso` | `layiso` | src/core/AliasTable.cpp:82 |
| `xa` | `xref` | src/core/AliasTable.cpp:83 |
| `xr` | `xref` | src/core/AliasTable.cpp:84 |
| `xl` | `xline` | src/core/AliasTable.cpp:85 |
| `hlp` | `help` | src/core/AliasTable.cpp:87 |

## 6. 自定义绘图函数与各调用位置

### 6.1 动态命令入口（命令串由运行时产生）

| 入口 | 位置 | 注入的命令来源 |
|---|---|---|
| `execute(line)` | src/ai/NlAssistant.cpp:214 | 变量命令串 |
| `execute(line)` | src/ai/NlAssistant.cpp:242 | 变量命令串 |
| `execute(line)` | src/ai/NlAssistant.h:15 | 变量命令串 |
| `execute(line)` | src/core/MacroScript.cpp:134 | 变量命令串 |
| `execute(command)` | src/core/ScriptApi.cpp:71 | 变量命令串 |
| `execute(lc)` | src/ui/CommandLine.cpp:64 | 变量命令串 |
| `execute(cmd)` | src/ui/CommandLine.cpp:108 | 变量命令串 |
| `execute(e.name)` | src/ui/CommandPalette.cpp:135 | 变量命令串 |
| `execute(cmd)` | src/ui/MainWindow.cpp:6572 | 变量命令串 |

关键入口说明：

- `NlAssistant.cpp:206/214/242`：`runSketchCommand(line)` 工具 → `interp_->execute(line)`，命令串由 LLM 生成；
  `kCore` 注入 24 个核心命令名作为提示词（见下）。
- `ScriptApi.cpp:71`：脚本 API `eval(command)` → `interp_->execute(command)`。
- `CommandLine.cpp:64/108`：命令行整行 / 分段坐标解析后执行。
- `CommandPalette.cpp:135`：命令面板选中项的 `e.name`。
- `MacroScript.cpp`：SCR 脚本回放，逐行调用命令解释器（`macroplay` / `scr`）。
- `MainWindow.cpp`：菜单/工具条直接 `interp_->execute("...")`（见 6.2）。

### 6.2 静态命令串调用点（首 token 汇总）

| 首 token | 调用次数 | 示例调用点 |
|---|---:|---|
| `aliaslist` | 1 | src/ui/MainWindow.cpp:2161 |
| `aliasreload` | 1 | src/ui/MainWindow.cpp:2158 |
| `arraypath` | 1 | tests/SketchSelfTest.cpp:5154 |
| `arraypolar` | 1 | tests/SketchSelfTest.cpp:5141 |
| `arrayrect` | 1 | tests/SketchSelfTest.cpp:5132 |
| `bhatch` | 2 | src/ui/MainWindow.cpp:2018 |
| `circl` | 1 | tests/SketchSelfTest.cpp:5699 |
| `circle` | 3 | tests/SketchSelfTest.cpp:1034 |
| `cuiload` | 1 | src/ui/MainWindow.cpp:2205 |
| `cuisave` | 1 | src/ui/MainWindow.cpp:2211 |
| `ddedit` | 4 | src/ui/MainWindow.cpp:1253 |
| `dim` | 3 | tests/SketchSelfTest.cpp:10957 |
| `dimedit` | 4 | tests/SketchSelfTest.cpp:10930 |
| `dtext` | 1 | tests/SketchSelfTest.cpp:8536 |
| `dyn` | 2 | tests/SketchSelfTest.cpp:1330 |
| `fieldupdate` | 2 | tests/SketchSelfTest.cpp:4505 |
| `filter` | 5 | tests/SketchSelfTest.cpp:4904 |
| `group` | 6 | tests/SketchSelfTest.cpp:4829 |
| `h` | 1 | tests/SketchSelfTest.cpp:8601 |
| `hatch` | 4 | src/ui/MainWindow.cpp:2016 |
| `hatchedit` | 1 | tests/SketchSelfTest.cpp:4304 |
| `hatchh` | 1 | tests/SketchSelfTest.cpp:8604 |
| `help` | 7 | src/ui/MainWindow.cpp:2319 |
| `insert` | 4 | src/ui/MainWindow.cpp:1923 |
| `join` | 2 | tests/SketchSelfTest.cpp:5112 |
| `layiso` | 4 | tests/SketchSelfTest.cpp:4960 |
| `laymch` | 6 | tests/SketchSelfTest.cpp:5040 |
| `layout` | 14 | tests/SketchSelfTest.cpp:5382 |
| `layuniso` | 4 | tests/SketchSelfTest.cpp:4971 |
| `line` | 4 | tests/SketchSelfTest.cpp:1033 |
| `linetype` | 4 | src/ui/MainWindow.cpp:2300 |
| `ltscale` | 4 | tests/SketchSelfTest.cpp:5161 |
| `macrorec` | 5 | tests/SketchSelfTest.cpp:1031 |
| `macrosave` | 1 | src/ui/MainWindow.cpp:2140 |
| `macroshow` | 1 | tests/SketchSelfTest.cpp:1036 |
| `macrostop` | 4 | tests/SketchSelfTest.cpp:1037 |
| `matchprop` | 4 | src/ui/MainWindow.cpp:1917 |
| `measure` | 4 | src/ui/MainWindow.cpp:2026 |
| `move` | 1 | tests/SketchSelfTest.cpp:10810 |
| `mtext` | 1 | tests/SketchSelfTest.cpp:8534 |
| `mview` | 4 | tests/SketchSelfTest.cpp:5384 |
| `nosuchcmd` | 2 | tests/SketchSelfTest.cpp:1035 |
| `overkill` | 2 | src/ui/MainWindow.cpp:1919 |
| `pedit` | 6 | src/ui/MainWindow.cpp:1915 |
| `point` | 1 | tests/SketchSelfTest.cpp:10764 |
| `poly_regular` | 4 | tests/SketchSelfTest.cpp:9017 |
| `prop` | 3 | tests/SketchSelfTest.cpp:4470 |
| `properties` | 3 | src/ui/MainWindow.cpp:2302 |
| `purge` | 3 | src/ui/MainWindow.cpp:1925 |
| `q` | 1 | tests/SketchSelfTest.cpp:1561 |
| `qselect` | 3 | tests/SketchSelfTest.cpp:1553 |
| `regen` | 2 | src/ui/MainWindow.cpp:2103 |
| `scr` | 1 | src/ui/MainWindow.cpp:2148 |
| `sdkhook` | 1 | tests/SketchSelfTest.cpp:859 |
| `stdcheck` | 3 | src/ui/StandardsPanel.cpp:77 |
| `text` | 4 | tests/SketchSelfTest.cpp:8528 |
| `tol` | 3 | src/ui/MainWindow.cpp:2028 |
| `trim` | 1 | src/ui/MainWindow.cpp:1647 |
| `ucs` | 13 | tests/SketchSelfTest.cpp:10613 |
| `undo` | 1 | tests/SketchSelfTest.cpp:4307 |
| `wblock` | 3 | src/ui/MainWindow.cpp:1921 |
| `wslist` | 1 | src/ui/MainWindow.cpp:2197 |
| `xh` | 3 | src/ui/MainWindow.cpp:6796 |
| `zz` | 1 | tests/SketchSelfTest.cpp:1323 |

### 6.3 AI system prompt 核心命令（`NlAssistant.cpp` kCore，24 项）

`circle`, `circle3p`, `arc`, `line`, `line_h`, `line_v`, `line_angle`, `rect`, `poly`, `poly_regular`, `poly_edge`, `poly_circumscribed`, `poly_star`, `point`, `text`, `delete`, `undo`, `redo`, `clear`, `select_all`, `select_none`, `zoom`, `move`, `rotate`

### 6.4 自定义绘图函数内部及其命令出口（专门核对项）

用户要求「包含自定义绘图函数内部及各调用位置涉及的绘图命令」。逐处核对如下：

| 自定义绘图函数 / 出口 | 位置 | 内部行为 | 是否发出命令 |
|---|---|---|---|
| `ScriptApi::addLine / addCircle / addArc / addRect / addPoint / addText` | src/core/ScriptApi.cpp:13,19,25,31,37,45 | `ElementFactory::createXxx` + `model_->addElement` 直接建模 | **否**（绕过命令层） |
| `ScriptApi::removeElement` | src/core/ScriptApi.cpp:65 | 直接删图元 | **否** |
| `ScriptApi::run(command)` | src/core/ScriptApi.cpp:69–71 | `interp_->execute(command)` | **是**：可注入任意已注册命令 |
| `SdkModel::addLine / addCircle / addArc / addRect / addPoint / addText` | src/core/SdkContext.cpp:14,20,26,32,38,46 | SDK 侧直接建模 | **否** |
| `ScriptHost` 脚本引擎执行 | src/core/ScriptHost.cpp:67 | `e->execute(code, api, ...)`，脚本可调上述 `addXxx` 或 `run()` | 经 `run()` 才发出命令 |
| `MacroScript` SCR 回放 | src/core/MacroScript.cpp:134 | `interp->execute(line)` 逐行执行外部脚本 | **是** |
| 命令解释器唯一分派点 | src/core/CommandInterpreter.cpp:212 | `command->execute(ctx)` | 所有命令的最终落点 |
| `GraphicsView` 交互工具 `ITool` 家族（12 个工具，src/ui/tools/） | 各 `*Tool.cpp` | 鼠标交互直接 `addElement` | **否** |
| `ElementFactory` / `PartLibrary` / 几何生成器 | src/core/ | 纯几何构造后写模型 | **否** |

**结论**：自定义绘图函数内部**不产生任何绘图命令**（建模直接写 `SketchModel`）；
唯一的命令注入口是 `ScriptApi::run()`、SCR 回放、AI `runSketchCommand`、命令行与菜单调用点（§6.1–6.3）。
因此「自定义绘图函数内部涉及的绘图命令」为**空集**，但这些函数确实影响绘图结果，
已作为「实现层绘制指令」在 §8 第 6 类单列，避免被误判为遗漏。

补充：`MainWindow.cpp:1247` 用 `QStringLiteral("%1 %2 %3 %4").arg(...)` 动态拼命令，
首 token 在运行时为 `text` 或 `mtext`（取决于是否多行），二者均已列入 §3 主清单。

## 7. 完整性核对

| 核对项 | 结果 |
|---|---|
| 注册名未收录进 `CommandCatalog`（help 走回落） | 22 项，明细见 7.1 |
| 目录条目无精确注册（由前缀家族/别名/外部注册解析） | 11 项，明细见 7.1 |
| 别名目标不在精确注册表 | 1 项：`extend`（见 §8.1，属无法确认个案） |
| 补全家族成员未进目录 | 44 项，明细见 7.1 |
| 别名键（本身即为用户可输入指令） | 66 个，全部列于 §5 |
| 调用点首 token 不属于任何声明集合 | 5 项（测试容错负例，见 §8 第 7 类） |
| **命令并集（主清单命令总数）** | **199 个，全部已列入 §3** |

目录未收录的命令并非不可用：`help <cmd>` 会输出回落提示，命令本身仍可正常执行；
反之目录中存在但无精确注册的条目，主要通过 `line_` / `arc_` / `poly_` / `spline_` /
`ellipse_` / `polyline_` 等前缀分派器或 acad.pgp 别名解析。

### 7.1 差异集合明细（用于复核「不遗漏」）

**A. 已注册但目录未收录（22）：**

`copy_dup`, `deselect_intersected`, `dim_smart`, `duplicate`, `freehand_line`, `hatchedit`, `macroshow`, `polyline`, `revert`, `revert_direction`, `select_contour`, `select_intersected`, `sheetframe`, `tbadd`, `tbdel`, `tbnew`, `tbrm`, `toggle_contour`, `toggle_layer`, `weld`, `wsdel`, `wsnew`

**B. 目录有条目但无精确注册（11，靠前缀/别名解析）：**

`line_angle`, `line_h`, `line_v`, `poly_circumscribed`, `poly_edge`, `poly_regular`, `poly_star`, `spline_explode`, `spline_from_polyline`, `spline_parabola_4p`, `spline_through_points`

**C. 补全家族成员未进目录（44）：**

`deselect_intersected`, `ellipse_1pt`, `ellipse_4p`, `ellipse_axis`, `ellipse_c3p`, `ellipse_foci`, `ellipse_inscribed`, `freehand_line`, `line_2pt`, `line_angle_from_line`, `line_bisector`, `line_from_point_to_line`, `line_orthogonal`, `line_orthogonal_from_line`, `line_parallel`, `line_parallel_pt`, `line_rel_angle`, `line_snake`, `line_snake_x`, `line_snake_y`, `line_tangent_cc`, `line_tangent_ortho`, `line_tangent_pc`, `polyline`, `polyline_add_node`, `polyline_append_node`, `polyline_arcs_to_chords`, `polyline_change_segment_type`, `polyline_delete_between`, `polyline_delete_node`, `polyline_equidistant`, `polyline_from_segments`, `polyline_trim_segments`, `select_contour`, `select_intersected`, `spline_append_pt`, `spline_hyperbola`, `spline_insert_pts`, `spline_parabola`, `spline_parabola_fd`, `spline_remove_between`, `spline_remove_pts`, `toggle_contour`, `toggle_layer`

## 8. 无法确认 / 不作穷举的绘图相关指令

以下类别**确实会影响绘图结果或渲染流程，但无法在静态源码中穷举或确认**，故不计入 §3 的 199 项：

1. **前缀家族的无限展开**：9 个 `registerPrefix` 使任意 `<前缀><任意后缀>` 都进入对应处理器；
   分派器内部再按后缀子串分支（见 `LineModeCommand` / `ArcModeCommand` / `PolyModeCommand` 等），
   合法后缀集合随实现分支增长，静态只能列出已注册的显式名（§4）。
2. **插件外部命令**：`CommandInterpreter::externalCommandNames()` 来自运行时加载的 SDK 插件，
   命令名与绘图行为由插件二进制决定，源码内无法确认。
3. **用户自定义 CUI/acad.pgp 别名**：`CuiManager` / `AliasTable` 在运行时读取用户配置文件，
   可把任意字符串映射到任意绘图命令；本文只列出 `defaultAliases()` 的 66 条内建别名。
4. **SCR/Macro 脚本文件内的命令串**：`scr <文件>` / `macroplay` 回放外部文本，
   文件内容不在仓库内，可能包含任意绘图命令。
5. **LLM 生成的命令串**：`runSketchCommand(line)` 的 `line` 由模型产出，
   理论上可组合出任意参数与任意已注册命令，参数空间不可穷举。
6. **QPainter / QGraphicsScene 底层绘制指令**：本文口径为「用户可执行的命令/指令」，
   `GraphicsView` / `PlotRenderer` / `ElementPath` 内部的 `drawLine`、`drawPolygon`、`setPen` 等
   C++ 绘制原语属于渲染实现层，不是命令，未纳入清单（如需该层清单需另行按函数提取）。
7. **测试容错负例命令串（5 个，均来自 `tests/SketchSelfTest.cpp`）**：`circl`, `hatchh`, `nosuchcmd`, `sdkhook`, `zz`。
   它们只在 §6.2 调用点统计中出现，用于验证未知命令/typo 的容错分支，**不是有效绘图命令**，不计入主清单。
8. **别名键作为独立输入指令**：66 个 acad.pgp 别名键（如 `l`、`c`、`h`、`q`）本身可被命令行直接输入，
   展开后的目标命令已在 §3/§5 覆盖；运行时用户自定义 pgp/CUI 别名仍不可确认（见第 3 类）。

### 8.1 明确存在、但有效性无法在静态源码中确认的个案：`extend`

- `AliasTable.cpp:30` 定义 `d["ex"] = "extend"`，别名 `ex` 会展开成 `extend`。
- 但 `CommandRegistry` 中**没有** `registerExact("extend")`，也不匹配任何 `registerPrefix`；
  `CommandCatalog` 同样没有 `extend` 条目。
- `CommandInterpreter::execute()`（src/core/CommandInterpreter.cpp:166 起）对未注册名只回落
  `externalCommands_`（插件运行时注册）或输出「未知命令 + 候选」，**不会激活交互工具**。
- `extend` 在源码中的真实身份是**交互工具 ID**：`ToolRegistry.cpp:79`、
  `ToolManager.cpp:193`、`MainWindow.cpp:3795`（`addEx(..., "extend", ...)` 切换 `activeTool`）。
- **结论**：命令行输入 `extend` / `ex` 能否触发延伸，静态源码无法确认（除非插件运行时注册了同名外部命令）；
  菜单/工具条的「延伸」按钮走工具层，不受影响。故 `extend` 未计入 §3 主清单，仅在 §7 差异与此处标明。

> 结论：**在「用户可执行命令」口径下，§3 的 199 项已覆盖源码中所有可静态确定的绘图相关命令**；
> 上述 8 类属于动态/外部/实现层或无效用例，属于**无法确认或不可穷举**的部分，已逐条标明。
