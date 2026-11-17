// =============================================================================
// ⚠️ 自动生成文件 —— 请勿手工编辑。
// 生成器：tools/i18n/build_i18n.py（--apply）
// 数据  ：tools/i18n/en_phrases.txt + tools/i18n/en_glossary.txt
//
// 用单个 raw string 数据块 + 惰性解析，而不是数千条 QStringLiteral 聚合初始化：
// MinGW GCC 13.1 (cc1plus) 处理巨型聚合初始化会静默崩溃（EXIT=1 零诊断）。
// =============================================================================
#include "I18nDict.h"

#include <QStringList>

namespace SketchCAD {
namespace I18n {
namespace Runtime {

namespace {

const char* const kExactData = R"I18N_DICT_END(
\n⚠ 该文件不存在或不是文件	\n⚠ The file does not exist or is not a file
      dim angle（先选中两条相交直线） | dim radius|dia <圆id> [引出角度]	      dim angle (select two intersecting lines first) | dim radius|dia <circle id> [leader angle]
      dim baseline|continue x1,y1 x2,y2 | dim arc <圆id> [起角 终角]	      dim baseline|continue x1,y1 x2,y2 | dim arc <circle id> [start angle end angle]
      dim leader 起点 折点 [文本点] | dim ordinate 被测点 基准点 [0/90]	      dim leader start point bend point [text point] | dim ordinate measured point datum point [0/90]
      数值驱动几何联动（对齐/水平/竖直/半径/直径/角度）；其余类型请用属性面板	      Value-driven geometric linking (align/horizontal/vertical/radius/diameter/angle); use the property panel for other types
     面积 %1 mm² / 周长 %2 mm / 形心 (%3, %4)	      Area %1 mm² / Perimeter %2 mm / Centroid (%3, %4)
  #%1(%2)  固有=%3 可控=%4 已约束=%5 剩余=%6%7	  #%1(%2)  Intrinsic=%3 Controllable=%4 Constrained=%5 Remaining=%6%7
  %1  %2 | 适用：%3 | 参数：%4	  %1  %2 | Applies to: %3 | Parameters: %4
  %1  [%2]  %3  图元%4  ← %5	  %1  [%2]  %3  Entity%4  ← %5
  %1. 「%2」 主动#%3 → 被动#%4%5  %6	  %1. \"%2\"  Active#%3 → Passive#%4%5  %6
  - #%1 %2：%3（建议：%4）	  - #%1 %2: %3 (Suggestion: %4)
  StandardsDB.query(%1, %2) = （该键未收录）	  StandardsDB.query(%1, %2) = (key not indexed)
  [#%1] %2 %3：%4（建议：%5）	  [#%1] %2 %3: %4 (Suggestion: %5)
  [坐标输入]	  [Coordinate input]
  [当前]	  [Current]
  [选项 %1]	  [Option %1]
  ⓘ 检测到 %1 条冗余约束（下标：%2），可删除以简化系统	  ⓘ Detected %1 redundant constraints (indices: %2); delete to simplify the system
  ⚠过约束	  ⚠Over-constrained
  ✗ 失败	Failed to ✗
  包围盒      (%1, %2) ~ (%3, %4)    %5 × %6	  Bounding box      (%1, %2) ~ (%3, %4)    %5 × %6
  卸载 %1	  Unload %1
  周长        %1 mm	  Perimeter        %1 mm
  属性 %1（%2）默认=%3 @(%4,%5) 字高%6	  Attribute %1 (%2) Default=%3 @(%4,%5) Text height%6
  形心        (%1, %2)	  Centroid        (%1, %2)
  惯性矩(形心) Ix = %1    Iy = %2    Ixy = %3 mm⁴	  Moment of inertia (centroid) Ix = %1    Iy = %2    Ixy = %3 mm⁴
  面积        %1 mm²	  Area        %1 mm²
 <图元id>	 <entity id>
 x1,y1 x2,y2 [尺寸线位置 px,py]	 x1,y1 x2,y2 [dimension line position px,py]
 不存在	 Does not exist
 不是圆或圆弧	 Is not a circle or arc
 个点解析失败（	 points failed to parse (
 工作平面	 Work plane
 标准 	 Standard 
 线宽 	 Lineweight 
 说明：	 Description:
 顶点)	 vertex)
#%1 %2  [图层 %3]  %4	#%1 %2  [Layer %3]  %4
#%1 %2：面积 = %3 mm²    周长 = %4 mm    形心 = (%5, %6)	#%1 %2: Area = %3 mm²    Perimeter = %4 mm    Centroid = (%5, %6)
%1\n\n本次改动可能在重启后丢失，请检查磁盘空间或以可写权限重新运行程序。	%1\n\nThis change may be lost after a restart. Please check disk space or rerun the program with write permission.
%1\n路径不存在，点击可从列表中移除。	%1\nPath does not exist; click to remove it from the list.
%1 不是外部参照	%1 is not an external reference
%1 分钟	%1 minutes
%1 字节	%1 bytes
%1 引擎不可用（可能未启用构建开关或缺少运行时）	%1 engine unavailable (build option may be disabled or runtime missing)
%1: %2 比例%3%4	%1: %2 Scale%3%4
%1图层 %2	%1 Layer %2
%1完成：%2 个图元已并入选区	%1 complete: %2 entities merged into the selection
%1（%2 · 分类：%3）	%1 (%2 · Category: %3)
%1（%2 个对象）	%1 (%2 objects)
%1（%2 个视口）	%1 (%2 viewports)
%1（%2 个边界）	%1 (%2 boundaries)
%1（%2 条件）	%1 (%2 conditions)
%1（分类：%2）	%1 (Category: %2)
%1：已出库插入（%2 个图元）	%1: Inserted from library (%2 entities)
%1：已生成（%2 个图元）	%1: Generated (%2 entities)
%1：没有图元被选中	%1: No entities selected
%1：生成失败（参数不合法）	%1: generation failed (invalid parameters)
%<PROP:名称>%	%<PROP:Name>%
(0.5=细一档, 1.0=原值, 越小越细)	(0.5=one step thinner, 1.0=original value, smaller = thinner)
(取消)选择图层	(Deselect) Select Layer
(取消)选择图层：点击图元，同层已全选中则整层取消，否则整层选中	(Deselect) Select Layer: Click an entity; if all entities on that layer are already selected, deselect the entire layer; otherwise, select the entire layer.
(取消)选择轮廓	(Deselect) Select Contour
(取消)选择轮廓：点击一条边，轮廓已全选中则整条取消，否则整条选中	(Deselect) Select Contour: Click an edge; if the entire contour is already selected, deselect the whole contour; otherwise, select the whole contour.
(按材料默认)	(By material default)
(未命名)	(Untitled)
(默认)	(Default)
---- #%1 %2 质量特性 ----	---- #%1 %2 Mass Properties ----
---- 对象列表（共 %1 个）----	---- Object List (%1 total) ----
// 示例：画一个十字标记\napp.addLine(-50, 0, 50, 0);\napp.addLine(0, -50, 0, 50);\nprint('当前图元数 =', app.elementCount());\n	// Example: draw a cross mark\napp.addLine(-50, 0, 50, 0);\napp.addLine(0, -50, 0, 50);\nprint('Current entity count =', app.elementCount());\n
10 分钟	10 minutes
15 分钟	15 minutes
1×1 阵列无副本（原图元保留）	1×1 array has no copies (original entity kept)
30 分钟	30 minutes
5 分钟	5 minutes
<b>感谢支持 SketchCAD</b><br>请使用微信「扫一扫」扫描下方收款码	<b>Thanks for supporting SketchCAD</b><br>Please use WeChat Scan to scan the payment QR code below
<b>设计中心</b>（符号库）	<b>Design Center</b> (Symbol Library)
<h3>SketchCAD 草图设计系统</h3><p><b>版本</b>：%1（文件属性 %2）</p><p><b>版权所有</b>：%3</p><p>基于 C++17 / Qt 6 构建，核心几何与约束求解为自研实现。</p><p>本软件为专有商业软件，详见安装目录内 LICENSE.txt。</p><p>如果 SketchCAD 对你有帮助，可点击下方「打赏」支持作者。</p>	<h3>SketchCAD Sketch Design System</h3><p><b>Version</b>: %1 (File properties %2)</p><p><b>Copyright</b>: %3</p><p>Built on C++17 / Qt 6; core geometry and constraint solving are developed in-house.</p><p>This software is proprietary commercial software; see LICENSE.txt in the installation directory for details.</p><p>If SketchCAD is helpful to you, click Tip below to support the author.</p>
>> 距离=%1 角度=%2  [DYN 输入]	>> Distance=%1 Angle=%2  [DYN input]
AI 助手	AI Assistant
AI 助手(&A)...	AI Assistant(&A)...
AI 助手总开关：	AI Assistant Master Switch:
AI 助手执行：%1（%2）	AI Assistant executing: %1 (%2)
AI 助手（自然语言操作）	AI assistant (natural language)
AI 工具派发 runSketchCommand(\"checkcompliance\") 经 CommandInterpreter 触发同一检查逻辑	AI tool dispatch: runSketchCommand(\"checkcompliance\") triggers the same check logic via CommandInterpreter
AI 正在处理…	AI is processing…
API 密钥：	API Key:
BOM 树	BOM Tree
CTB 样式表 (*.ctb *.txt);;所有文件 (*)	CTB style table (*.ctb *.txt);;All files (*)
CTB 解析警告	CTB parse warning
CUIx 文件 (*.cuix);;所有文件 (*.*)	CUIx files (*.cuix);;All files (*.*)
Ctrl+Z 最多能回退多少步；超出时丢弃最旧的一步。\n调大更省心但占内存（每步都含一份图纸状态）。	Maximum number of steps Ctrl+Z can undo; the oldest step is discarded when exceeded.\nLarger values are more convenient but use more memory (each step holds a drawing state).
DWG 写出说明	DWG Write Notes
DWG 解析失败：	DWG parse failed:
DWG 解析失败：未安装 DWG 转换组件，无法读取版本 %1（%2）	DWG parse failed: DWG conversion component not installed; cannot read version %1 (%2)
DWG 转换组件路径：	DWG conversion component path:
DXF 解析失败：	DXF parse failed:
Enter 执行 · Ctrl+Shift+P 再次呼出 · 最近使用自动置顶	Enter to execute · Ctrl+Shift+P to recall again · Recently used auto-pinned to top
FILTER 预选器已设置（%1 条件）：作用于下一次拾取/框选/栏选，随后自动清除	FILTER preselection set (%1 conditions): applies to the next pick/window/fence selection, then cleared automatically
FILTER 预选器生效：候选 %1 个 → 命中 %2 个（预选器已自动清除）	FILTER preselection active: %1 candidates → %2 hits (preselection cleared automatically)
GB 灰度抖动（粗/细分线宽 + 中间调网点）	GB grayscale dithering (thick/thin lineweights + midtone dots)
GB 黑白出图（按线型分粗/细，纯黑不抖动）	GB monochrome plotting (thick/thin by linetype, pure black, no dithering)
GB/T 4458（国标）	GB/T 4458 (GB Standard)
GD&T 标注	GD&T Dimension
GD&T 标注面板 — 粗糙度 / 几何公差	GD&T Dimension Panel — Surface Roughness / Geometric Tolerance
GD&T 标注面板(&D)...	GD&T Dimension Panel(&D)...
GPU 不可用时回退到离屏渲染；当前版本仅记录偏好	Falls back to offscreen rendering when GPU is unavailable; the current version only records the preference
HATCHEDIT 失败：图元 #%1 不是剖面区域	HATCHEDIT failed: entity #%1 is not a section region
HATCHEDIT：已就地修改 %1 个剖面区域（单步撤销）	HATCHEDIT: modified %1 section regions in place (single-step undo)
ISO 129（国际）	ISO 129 (International)
JavaScript 执行完成，图元数=%1	JavaScript execution complete, entity count=%1
MySQL 设置	MySQL Settings
MySQL（服务器型）	MySQL (Server-based)
PDF 文件 (*.pdf)	PDF Files (*.pdf)
PDF 路径（多 PDF 模式 = 文件基名）	PDF path (multi-PDF mode = file base name)
PE 合并	PE Merge
PE 打开	PE Open
PE 曲线化	PE Spline
PE 线宽	PE Lineweight
PE 闭合	PE Close
R 外接圆半径；side:L 用边长（推荐，LLM 原样转发提示词尺寸）；sides 边数(≥3)	R circumradius; side:L uses edge length (recommended, LLM forwards the prompt dimension as-is); sides number of sides (≥3)
Ra（轮廓算术平均偏差）	Ra (arithmetical mean deviation of the profile)
Rz（轮廓最大高度）	Rz (maximum profile height)
SQLite 数据库 (*.db *.sqlite);;所有文件 (*.*)	SQLite database (*.db *.sqlite);;All files (*.*)
SQLite 设置	SQLite Settings
SQLite（文件型，零配置）	SQLite (file-based, zero configuration)
STDCHECK：发现 %1 个不合规项（7 类 GB/T 规范）：	STDCHECK: found %1 non-compliant items (7 categories of GB/T standards):
STDCHECK：未发现不合规项，图纸符合相关 GB/T 规范。	STDCHECK: no non-compliant items found; the drawing complies with the relevant GB/T standards.
STEP 为只写格式（曲线集交换），本程序不支持读回：请用 CAD 转换后再导入	STEP is a write-only format (curve set exchange); reading back is not supported in this program: convert with CAD before importing
SketchCAD 发布输出	SketchCAD Publish Output
SketchCAD 尊重您的隐私：\n\n· 默认不上传任何数据，没有任何后台网络请求；\n· 「导出诊断包」只在您主动点击时生成一个本机 ZIP 文件，\n  是否发送完全由您决定；\n· 匿名遥测默认关闭，可在「选项 → 隐私」中开启或关闭。	SketchCAD respects your privacy:\n\n· No data is uploaded by default, and there are no background network requests;\n· Export Diagnostics Bundle only creates a local ZIP file when you click it,\n  and whether to send it is entirely up to you;\n· Anonymous telemetry is off by default and can be enabled or disabled under Options > Privacy.
SketchCAD 打印输出	SketchCAD Plot Output
Snake 折线至少需要两个点	Snake polyline requires at least two points
UCS 已保存: 	UCS saved: 
UCS 已切换: 	UCS switched: 
UCS 已切换到世界坐标系（WORLD）	UCS switched to World Coordinate System (WORLD)
UCS 已删除: 	UCS deleted: 
UCS 已对齐视图（平面视图下视图系=世界系）	UCS aligned to view (in plan view, view system = world system)
UCS 的定义/切换/命名管理（单一分派命令）	UCS definition/switch/naming management (single dispatch command)
UCS(Z轴) 已定义: 	UCS(Z axis) defined: 
UCS(三点) 已定义: 	UCS(3 points) defined: 
UCS(原点) 已定义: 	UCS(origin) defined: 
UCS(对象) 已定义: 	UCS(object) defined: 
WELD：未生成任何图元（类型可能不支持）	WELD: no entities generated (type may be unsupported)
X 正向点与原点重合，无法定义 UCS	X-axis point coincides with the origin; cannot define UCS
X 缩放	X Scale
XY 平面（俯视）	XY Plane (Top)
XZ 平面（主视）	XZ Plane (Front)
Y 缩放	Y Scale
YZ 平面（左视）	YZ Plane (Left)
[%1:%2] 执行失败：%3	[%1:%2] Execution failed: %3
[%1] %2：%3（%4 项）	[%1] %2: %3 (%4 items)
[%1] 中断于第 %2 行（STOP 模式）：已执行 %3，失败 %4	[%1] Stopped at line %2 (STOP mode): executed %3, failed %4
[%1] 内部错误：命令解释器为空	[%1] Internal error: command interpreter is null
[%1] 拒绝嵌套执行：已有脚本正在运行	[%1] Nested execution refused: a script is already running
[%1] 拒绝执行：宏录制进行中（先 macrostop）	[%1] Execution refused: macro recording in progress (run macrostop first)
[%1] 脚本执行完成：共 %2 条，成功 %3，失败 %4%5	[%1] Script execution complete: %2 total, %3 succeeded, %4 failed%5
[plotstyle] CTB 部分行解析失败：	[plotstyle] Some CTB lines failed to parse:
[robust] DXF/DWG 打开校验：%1 个损坏图元已跳过，%2 个已修复。%3	[robust] DXF/DWG open validation: %1 corrupted entities skipped, %2 repaired. %3
[robust] 草图打开校验：%1 个损坏图元已跳过，%2 个已修复。%3	[robust] Sketch open validation: %1 corrupted entities skipped, %2 repaired. %3
[系统] 我是慎独 草图模块 — 执行信息面板已就绪	[System] I am Shendu, sketch module — execution info panel is ready
a0 起始角；a1 终止角（度，省略 = a0+360 即整圆弧）	a0 start angle; a1 end angle (degrees, omitted = a0+360, i.e. full arc)
alias <别名> <完整命令>	alias <alias> <full command>
aliasload \"<路径>\"	aliasload \"<path>\"
aliassave [\"<路径>\"]	aliassave [\"<path>\"]
align <id> <源1> <目标1> [<源2> <目标2>]	align <id> <source1> <target1> [<source2> <target2>]
angle 后须跟角度	angle must be followed by an angle
apothem 边心距（圆心到边中点距离）；sides 边数(≥3)	apothem: distance from center to edge midpoint; sides: number of sides (≥3)
area <封闭图元id> | x1,y1 x2,y2 [x3,y3...]	area <closed entity id> | x1,y1 x2,y2 [x3,y3...]
area: 第 	area: No. 
arraypath <路径id> <间距> | arraypath divide <路径id> <数量>	arraypath <path id> <spacing> | arraypath divide <path id> <count>
arraypolar <中心x,y> <个数> [填充角]	arraypolar <center x,y> <count> [fill angle]
arrayrect <行数> <列数> <行距> <列距>	arrayrect <rows> <columns> <row spacing> <column spacing>
attdef <块名> <标签> [提示] [x,y] [字高]	attdef <block name> <tag> [prompt] [x,y] [text height]
attedit <引用id> <标签> <值>	attedit <reference id> <tag> <value>
attlist [引用id]	attlist [reference id]
block create <块名> [基点x,y] [delete|keep] | insert <块名> <x,y> [旋转] [sx] [sy] | list | info <块名> | redefine | explode <引用id> | delete | rename	block create <block name> [base point x,y] [delete|keep] | insert <block name> <x,y> [rotation] [sx] [sy] | list | info <block name> | redefine | explode <reference id> | delete | rename
block create 法兰 0,0	block create flange 0,0
chamfer <id1> <id2> <距离1> <距离2|角度>	chamfer <id1> <id2> <distance1> <distance2|angle>
color #RRGGBB|颜色名	color #RRGGBB|color name
constraint add <类型> <idA> <idB> [值|轴id] | solve | list | check | diagnose | clear	constraint add <type> <idA> <idB> [value|axis id] | solve | list | check | diagnose | clear
count<2 无副本（原图元保留）	count<2: no copies (original entities kept)
cuiload \"<路径>\"	cuiload \"<path>\"
cuisave \"<路径>\"	cuisave \"<path>\"
cx,cy 圆心；r 半径（>0）	cx,cy center; r radius (>0)
ddedit <id> <新内容>	ddedit <id> <new content>
deselect_layer <图层名>	deselect_layer <layer name>
dim angle: 所选两个图元必须都是直线	dim angle: the two selected entities must both be lines
dim angle: 所选两条直线不相交，无法标注夹角	dim angle: the two selected lines do not intersect; cannot dimension the included angle
dim angle: 请先选中两条相交直线	dim angle: Please select two intersecting lines first
dim arc: 起始角/终止角解析失败，使用默认 0°~90°	dim arc: start/end angle parse failed, using default 0°~90°
dim leader: 点解析失败	dim leader: point parse failed
dim ordinate: 旋转角解析失败，默认 X 坐标	dim ordinate: rotation angle parse failed, defaulting to X coordinate
dim ordinate: 点解析失败	dim ordinate: point parse failed
dim: 测量值为 0，标注未创建	dim: measured value is 0; dimension not created
dimedit <标注id> <新值> 或选中标注后 dimedit <新值>	dimedit <dimension id> <new value> or select a dimension first, then dimedit <new value>
dimedit(仅显示)	dimedit(display only)
dims: 请先选中单条直线(→对齐标注)或单个圆/圆弧(→直径标注)	dims: Please select a single line (→ aligned dimension) or a single circle/arc (→ diameter dimension)
dimstyle [list | new <名> [gbt|iso] | set <名> | rename <旧> <新> | apply <名> [id...] | delete <名>]	dimstyle [list | new <name> [gbt|iso] | set <name> | rename <old> <new> | apply <name> [id...] | delete <name>]
dist: 点解析失败（格式 x,y）	dist: point parse failed (format x,y)
dtext <x,y> <字高> <内容...>	dtext <x,y> <text height> <content...>
dxfout <路径.dxf> [-v R12|2000]	dxfout <path.dxf> [-v R12|2000]
dxfout 失败：	dxfout failed:
dxfout：无法写入 	dxfout: cannot write 
dxfout：未知版本 %1（可用 R12 / 2000）	dxfout: unknown version %1 (available: R12 / 2000)
dynlist <块名>	dynlist <block name>
dynset <引用id> <参数名> <值|状态名>	dynset <reference id> <parameter name> <value|state name>
fillet <id1> <id2> <半径>	fillet <id1> <id2> <radius>
filter [type=类型] [layer=图层] ... | none	filter [type=type] [layer=layer] ... | none
group [create <名> | add <名> | remove | list | ungroup <名>]	group [create <name> | add <name> | remove | list | ungroup <name>]
group create 螺栓组	group create BoltGroup
hatch [scale <倍数> | angle <度> | pattern <名|none>]	hatch [scale <factor> | angle <degree> | pattern <name|none>]
hatch 的别名入口（自动选边界）	Alias entry for hatch (auto-selects boundary)
help [<命令名>]	help [<command name>]
id 或段数无效	id or segment count invalid
id 无效	Invalid id
id 解析失败	id parse failed
id: 点解析失败（格式 x,y）	id: point parse failed (format x,y)
insert <块名> <x,y> [旋转度] [缩放X] [缩放Y]	insert <block name> <x,y> [rotation degrees] [scale X] [scale Y]
insert 法兰 50,50 0	insert flange 50,50 0
join [容差mm]	join [tolerance mm]
kAngle：refs 中无有效直线	kAngle: no valid line in refs
kCenter：参考既非直线亦非有效圆	kCenter: reference is neither a line nor a valid circle
kCircleMark：refs 中无有效圆	kCircleMark: no valid circle in refs
kCorner：两直线无交点（平行/退化）	kCorner: the two lines have no intersection (parallel or degenerate)
kCorner：参考非直线	kCorner: reference is not a line
kCorner：需要两条相交直线	kCorner: two intersecting lines are required
kCross：参考非圆/直线/点	kCross: reference is not a circle, line or point
kFullCircle：参考非有效圆	kFullCircle: reference is not a valid circle
kHole：参考非有效圆（孔）	kHole: reference is not a valid circle (hole)
kPlate：参考非矩形	kPlate: reference is not a rectangle
kPlate：矩形包围盒为空	kPlate: rectangular bounding box is empty
kThroughHole：参考非有效圆（孔）	kThroughHole: reference is not a valid circle (hole)
kTwoLinesCross：两直线无交点（平行/退化）	kTwoLinesCross: the two lines have no intersection (parallel or degenerate)
kTwoLinesCross：参考非直线	kTwoLinesCross: reference is not a line
kTwoLinesCross：需要两条相交直线	kTwoLinesCross: two intersecting lines are required
layer add <名称> [颜色] [线宽] [线型] | delete | rename | visible | lock | set | assign | apply | list	layer add <name> [color] [lineweight] [linetype] | delete | rename | visible | lock | set | assign | apply | list
layer add 中心线 red 0.25 CENTER	layer add Centerline red 0.25 CENTER
laymch <目标图层名>	laymch <target layer name>
lengthen <id> <参数>	lengthen <id> <parameter>
line_angle x,y <角度> <长度>	line_angle x,y <angle> <length>
line_h x,y <长度>	line_h x,y <length>
line_v x,y <长度>	line_v x,y <length>
ltscale <正数>	ltscale <positive number>
macroplay \"<路径>\"	macroplay \"<path>\"
macrorec：无法开始录制（已在录制中或脚本回放进行中）	macrorec: cannot start recording (already recording or script playback in progress)
macrosave \"<路径>\"	macrosave \"<path>\"
macrosave：没有可保存的录制内容	macrosave: no recording to save
macroshow：录制缓冲为空	macroshow: recording buffer is empty
massprop <封闭图元id> ...	massprop <closed entity id> ...
matchprop <源id> <目标id...>	matchprop <source id> <target id...>
measure <id> <间距>	measure <id> <spacing>
measure 支持直线/多段线/圆弧/圆（#%1 是 %2）	measure supports lines/polylines/arcs/circles (#%1 is %2)
mirror x1,y1 x2,y2 [delete源]	mirror x1,y1 x2,y2 [delete source]
mtext <x,y> <字高> <内容...>	mtext <x,y> <text height> <content...>
mview <图纸X> <图纸Y> <宽> <高> [比例]	mview <drawing X> <drawing Y> <width> <height> [scale]
offset <id> <距离> [方向点x,y]	offset <id> <distance> [direction point x,y]
outerR 外半径；points 角数(≥3)；innerRatio 内半径比(默认0.382)	outerR outer radius; points number of points (≥3); innerRatio inner radius ratio (default 0.382)
pattern 后须跟图案名或 none	pattern must be followed by a pattern name or none
pedit <id> [close|open|width <线宽>|join <id...>|fit|spline]	pedit <id> [close|open|width <lineweight>|join <id...>|fit|spline]
pedit 仅支持多段线/直线/圆弧（#%1 是 %2）	pedit supports only polylines/lines/arcs (#%1 is %2)
perp <直线Id> <x,y>	perp <line id> <x,y>
perp：已生成垂线 #%1（过 %2,%3 垂直于 #%4）	perp: generated perpendicular line #%1 (through %2,%3 perpendicular to #%4)
perp：未找到指定直线	perp: specified line not found
perp：点坐标无效	perp: point coordinates invalid
perp：直线Id 无效	perp: line id invalid
point：已创建 %1 个点实体（可被 Node 节点捕捉）	point: created %1 point entities (snappable with Node object snap)
polar [on|off|<增量角>]	polar [on|off|<increment angle>]
prop [<名称> [<值>] | -r <名称>]	prop [<name> [<value>] | -r <name>]
prop 项目名称 减速器箱体	prop item name gearbox housing
properties（先选中）或 properties <id...>	properties (select first) or properties <id...>
qconnect <目标圆Id> <x,y>	qconnect <target circle id> <x,y>
qconnect：已生成象限连接 #%1（从 %2,%3 连到 #%4 最近象限点）	qconnect: generated quadrant connection #%1 (from %2,%3 to nearest quadrant point of #%4)
qconnect：未找到目标图元	qconnect: target entity not found
qconnect：点坐标无效	qconnect: point coordinates invalid
qconnect：目标Id 无效	qconnect: target id invalid
qdim: 已生成 %1 个%2标注（%3 个锚点）	qdim: generated %1 %2 dimensions (%3 anchors)
qdim: 请先框选一组平行线或圆阵列	qdim: Please window-select a set of parallel lines or a circular array first
qdim: 选择集中可标注的有效几何不足 2 个	qdim: fewer than 2 valid geometries to dimension in the selection set
qselect [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型]	qselect [type=type] [layer=layer] [color=#rrggbb] [linestyle=linetype]
qselect type=直线 layer=0	qselect type=line layer=0
radius 必须 ≥ 0	radius must be ≥ 0
refs 为空：中心线需要至少一个参考图元	refs is empty: a centerline requires at least one reference entity
rotate <基点x,y> <角度> [copy]	rotate <base point x,y> <angle> [copy]
scale <基点x,y> <比例> [copy]	scale <base point x,y> <scale> [copy]
scale 后须跟正数	scale must be followed by a positive number
scale 后须跟正数倍数	scale must be followed by a positive multiplier
scr \"<路径>\"	scr \"<path>\"
select_layer <图层名>	select_layer <layer name>
side 必须为 0（起点）或 1（终点）	side must be 0 (start point) or 1 (end point)
sink 必须是 IDispatch 对象	sink must be an IDispatch object
snap [on|off|<步长mm>]	snap [on|off|<step mm>]
standards [<表名> [<键名>]]	standards [<table name> [<key name>]]
stretch <id> <参数>	stretch <id> <parameter>
tangent2 <圆IdA> <圆IdB>	tangent2 <circle id A> <circle id B>
tangent2：圆Id 无效	tangent2: circle id invalid
tangent2：生成 %1 条公切线（#%2 ↔ #%3）	tangent2: generated %1 common tangents (#%2 ↔ #%3)
tangent2：需两个圆/圆弧	tangent2: requires two circles/arcs
text 0,0 3.5 说明	text 0,0 3.5 Note
text <x,y> <字高> <内容...>	text <x,y> <text height> <content...>
text 的别名入口	Alias entry for text
tol <x,y> [公差文本]	tol <x,y> [tolerance text]
trim <id> <x,y> [边界id...]	trim <id> <x,y> [boundary id...]
trim: 找不到图元 #	trim: entity # not found
trim: 点击点解析失败	trim: click point parse failed
ucs world|view|3p <p1> <p2> [p3]|object <id>|z <x,y> <角>|origin <x,y> [角]|save/set/restore/delete/list|coords wcs|ucs	ucs world|view|3p <p1> <p2> [p3]|object <id>|z <x,y> <angle>|origin <x,y> [angle]|save/set/restore/delete/list|coords wcs|ucs
wblock <输出.dxf> [id...]	wblock <output.dxf> [id...]
wblock 失败：	wblock failed:
wblock：无法写入 	wblock: cannot write 
width 后须跟线宽(px)	width must be followed by a lineweight (px)
wsload <名>	wsload <name>
x,y 为拾取点（点选被剪一侧）	x,y is the pick point (click the side to be trimmed)
x1,y1 x2,y2 一条边的两端点；sides 边数(≥3)	x1,y1 x2,y2 are the two endpoints of an edge; sides number of sides (≥3)
x1,y1 起点；x2,y2 终点（点格式 x,y 或 x y 或 x,y,z）	x1,y1 start point; x2,y2 end point (point format x,y or x y or x,y,z)
xh：已生成 %1 个序号球标（明细表已刷新）	xh: generated %1 item balloons (BOM table refreshed)
xh：无有效对象，未生成球标	xh: no valid objects; no balloons generated
xh：请先选中对象或给定图元 id（用法：xh [id ...]）	xh: Please select objects or provide entity ids (usage: xh [id ...])
xline x,y <角度> | xline x1,y1 x2,y2	xline x,y <angle> | xline x1,y1 x2,y2
xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy] | detach <块名> | list | path <块名>	xref attach <file path> [full|relative|none] [x,y] [rotation] [sx] [sy] | detach <block name> | list | path <block name>
zoom <倍率>	zoom <factor>
{\"type\":\"object\",\"properties\":{\"line\":{\"type\":\"string\",\"description\":\"完整命令串\"}},\"required\":[\"line\"]}	{\"type\":\"object\",\"properties\":{\"line\":{\"type\":\"string\",\"description\":\"Full command string\"}},\"required\":[\"line\"]}
{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\",\"description\":\"命令名（小写）\"}},\"required\":[\"name\"]}	{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\",\"description\":\"Command name (lowercase)\"}},\"required\":[\"name\"]}
{\"type\":\"object\",\"properties\":{\"rule\":{\"type\":\"string\",\"description\":\"可选：聚焦的规则名或编号；省略则检查全部规则\"}}}	{\"type\":\"object\",\"properties\":{\"rule\":{\"type\":\"string\",\"description\":\"Optional: rule name or number to focus on; omit to check all rules\"}}}
{\"type\":\"object\",\"properties\":{\"rule\":{\"type\":\"string\",\"description\":\"要精确查询的国标规则/数值短语\"}},\"required\":[\"rule\"]}	{\"type\":\"object\",\"properties\":{\"rule\":{\"type\":\"string\",\"description\":\"GB standard rule / numeric phrase to query precisely\"}},\"required\":[\"rule\"]}
{\"type\":\"object\",\"properties\":{\"table\":{\"type\":\"string\",\"description\":\"标准表名（12 类之一）\"},\"key\":{\"type\":\"string\",\"description\":\"键名；省略则返回该表全部键值\"}},\"required\":[\"table\"]}	{\"type\":\"object\",\"properties\":{\"table\":{\"type\":\"string\",\"description\":\"Standard table name (one of 12 categories)\"},\"key\":{\"type\":\"string\",\"description\":\"Key name; omit to return all key-value pairs of the table\"}},\"required\":[\"table\"]}
·相对上一段	·Relative to previous segment
·绝对	·Absolute
Δ长度 (mm，正=延长 负=缩短)	ΔLength (mm, positive = extend, negative = shorten)
“%1” 不在 GB/T 14690 标准比例系列内，仍可绘制，但建议优先采用标准比例。	\"%1\" is not in the GB/T 14690 standard scale series; it can still be used, but standard scales are recommended.
●当前	* Current
⚠ %1（图元 #%2）	⚠ %1 (entity #%2)
⚠ MySQL 驱动不可用：当前环境缺少 Qt 的 QMYSQL 插件（qsqlmysql.dll 与 libmysql.dll）。\n	⚠ MySQL driver unavailable: this environment lacks the Qt QMYSQL plugin (qsqlmysql.dll and libmysql.dll).\n
⚠ 不合规：	⚠ Non-compliant:
⚠ 该操作会修改图纸，请确认：	⚠ This operation modifies the drawing. Confirm:
✓ 符合 GB/T 18617/ISO 13567 规范	✓ Compliant with GB/T 18617/ISO 13567
✓已满足	✓Satisfied
✗未满足	✗Unsatisfied
〈系统默认打印机〉	(System Default Printer)
「文件 > 最近打开」子菜单保留的条目数；0 表示关闭最近文件	Number of entries kept in the File > Open Recent submenu; 0 disables recent files
「直接打印」优先使用此设备（设备不存在时回落系统默认）。\n纸张/方向/边距/打印样式等页面细节在「打印 → 打印设置...」中配置，\n退出程序时自动记住，无需在此重复设置。	Direct Plot prefers this device (falls back to the system default if the device does not exist).\nPage details such as paper/orientation/margins/plot style are configured in Plot > Plot Settings...,\nand are remembered automatically on exit; no need to set them here again.
【%1】共 %2 项：	[%1] Total %2 items:
三点共线或重合，无法确定圆	Three points are collinear or coincident; cannot determine a circle
三点圆	3-Point Circle
三点圆弧	3-Point Arc
三点圆：三点共线，无法确定唯一圆，请重新拾取	3-point circle: the three points are collinear; cannot determine a unique circle, please pick again
上	Up
上偏差 +	Upper deviation +
上次异常退出	Last abnormal exit
上次自动保存 %1	Last autosave %1
下	Down
下偏差 -	Lower deviation -
不合规	Non-compliant
不限	Unlimited
与 stdcheck 相同的工程制图合规检查，供 AI 助手 checkCompliance 经 runSketchCommand 调用	Same engineering drawing compliance check as stdcheck, called by AI Assistant checkCompliance via runSketchCommand
与X轴夹角	Angle with X axis
丢弃快照失败：%1	Failed to discard snapshot: %1
丢弃所选	Discard Selected
两个控制点相邻，无需删除中间段	Two control points are adjacent; no intermediate segment to delete
两个节点相邻，无需删除	The two nodes are adjacent; nothing to delete
两圆公切线：依次选两个圆，鼠标移动实时切换切线，点击确认	Common tangent of two circles: select two circles in turn; move the mouse to switch tangents live, click to confirm
两点+半径	2 Points + Radius
两点+半径：点击两点定义弦，左侧输入半径，光标选近解	2 Points + Radius: click two points to define a chord, enter the radius on the left, and use the cursor to pick the nearer solution
两点圆	2-Point Circle
两点构造线	2-Point Construction Line
两点绘制直线	Draw a line through two points
两点距离 + ΔX / ΔY / XY 平面方向角	Distance between two points + ΔX / ΔY / XY plane direction angle
两端点不重合，无法合并	The two endpoints do not coincide; cannot merge
两节点相邻无需删除	Two nodes are adjacent; no deletion needed
两角点绘制矩形	Draw a rectangle from two corner points
严重=Critical	Fatal =Critical
中上	Top Center
中下	Bottom Center
中心	Center
中心 + 外半径生成星形	Center + outer radius generates a star
中心 + 外接圆半径（或边长）生成正多边形	Center + circumradius (or edge length) generates a regular polygon
中心 + 边心距生成正多边形	Create a regular polygon from center and apothem
中心孔公差	Center Hole Tolerance
中心孔径	Center hole diameter
中心孔符号	Center Hole Symbol
中心孔面粗糙度	Center hole surface roughness
中心点 X	Center point X
中心点 Y	Center point Y
中心点指定方式	Center Point Specification
中心点解析失败	Center point parse failed
中心线：%1	Center line: %1
中心线：已生成 %1 条（%2）	Center line: generated %1 (%2)
中心线：请先选中参考图元	Center line: Please select a reference entity first
中心通孔 Ø	Center through hole Ø
中点=Midpoint	Midpoint =Midpoint
中粗线 1.0mm	1.0 mm medium
中粗线 2px=Medium thick	Medium line 2px=Medium thick
临时捕捉覆盖（仅本次生效）	Temporary Snap Override (one time only)
临时捕捉覆盖：仅 %1（Shift+右键），放置一点后自动恢复	Temporary snap override: %1 only (Shift+right-click); restores automatically after placing one point
临时追踪点	Temporary tracking point
临时追踪点 (TK) — 光标处	Temporary tracking point (TK) — at cursor
为块定义追加属性定义（属性块）	Append attribute definitions to a block definition (attribute block)
为选中对象生成序号球标并写入明细表（BOM）	Generate item balloons for selected objects and write them into the BOM table
为选中的 %1 个边界指定区域名称：	Specify a region name for the selected %1 boundaries:
主孔 Ø	Main hole Ø
主孔公差	Main hole tolerance
主机：	Host:
主起落架距机头	Main Gear to Nose Distance
主起落架轮距	Main Gear Track
云端后端需要；本地/离线可为空	Required for cloud backend; can be empty for local/offline use
云端（OpenAI 兼容：DeepSeek/通义/智谱…）	Cloud (OpenAI compatible: DeepSeek/Qwen/Zhipu...)
亮色（默认）	Light (Default)
仅建立块定义，保留原图元	Create the block definition only and keep the original entities
仅支持 .json（SketchCAD 存档）与 .dxf 图纸的块表。	Only .json (SketchCAD archive) and .dxf drawing block tables are supported.
从 .pgp 文件重新加载别名表	Reload the alias table from a .pgp file
从一点连到目标圆/弧最近的象限点	Connect from a point to the nearest quadrant point of the target circle/arc
从列表中删除	Remove from list
从当前选择集排除	Exclude from Current Selection
从指定 .pgp 文件加载别名	Load aliases from the specified .pgp file
从编组移除	Remove from Group
从若干已有线段/圆弧/折线合并为一条折线	Merge several existing line segments/arcs/polylines into one polyline
从选区移除	Remove from Selection
代号	Code
以边界修剪图元（无边界 = 全体为界）	Trim entities by boundary (no boundary = all entities serve as boundaries)
任一满足（OR）	Any Satisfied (OR)
任一维省略 = 不过滤该维；全空 = 仅提示	Omitting any dimension = do not filter that dimension; all blank = prompt only
任务 %1/%2 %3 src=%4 out=%5 %6	Task %1/%2 %3 src=%4 out=%5 %6
位移参数解析失败	Failed to parse displacement parameters
作用于当前选择集	Applies to the current selection set
作用于当前选择集；spacing=0 时用 divide 等分全长	Applies to the current selection set; with spacing=0, use divide to divide the full length equally
作用于当前选择集；容差缺省 0.5mm	Applies to the current selection set; tolerance defaults to 0.5mm
作用于当前选择集；选中圆/弧时可省略中心=圆心	Applies to the current selection set; for a selected circle/arc, center=circle center can be omitted
你	You
你=You	You =You
依序通过给定点列构造样条	Build a spline through the given point list
依据 7 类 GB/T 规范（线宽粗细比/两线最小间隙/字号下限/粗糙度符号/焊缝符号完整性/幅面留边/图层命名）检查当前图纸，只读诊断，不改图	Checks the current drawing against 7 categories of GB/T standards (lineweight width ratio / minimum gap between two lines / minimum text size / surface roughness symbol / weld symbol completeness / sheet margin / layer naming); read-only diagnostics, does not modify the drawing
依据当前选区自动选择标注类型（直线→对齐，圆→直径）	Automatically selects the dimension type based on the current selection (line → aligned, circle → diameter)
依次输入顶点绘制多边形（首末自动闭合）	Draw a polygon by entering vertices in sequence (first and last are connected automatically)
依次输入顶点绘制样条曲线	Enter vertices in order to draw a spline
保存 CUIx	Save CUIx
保存...	&Save...
保存修改	Save Changes
保存别名	Save Alias
保存到数据库	Save to Database
保存动作宏	Save Action Macro
保存失败	Failed to save
保存失败（无法写入）：	Save failed (unable to write):
保存失败：	Save failed:
保存宏	Save Macro
保存并连接	Save and Connect
保存当前图纸到数据库...	Saving current drawing to database...
保存时间	Save Time
保存界面定制文件	Save Interface Customization File
保存界面配置	Save Interface Configuration
保存界面配置 (CUIx)	Save Interface Configuration (CUIx)
保存过滤器	Save Filter
保存过滤器 	Save Filter 
保存间隔：	Save Interval:
修剪	Trim
修剪 (Trim)：点击要剪掉的一段；Shift+点击可先指定边界图元	Trim (Trim): click the segment to trim off; Shift+click to first specify boundary entities
修剪二...	Trim 2...
修剪二需要至少 2 个选中图元（点击+边界）	Trim requires at least 2 selected entities (line + boundary)
修剪后节点不足	Not enough nodes after trimming
修剪折线段（删除整段）	Trim polyline segment (delete the entire segment)
修改	Modify
修改 (Modify)：修剪 / 延伸 / 旋转 / 镜像 / 偏移 / 分解 …	Modify (Modify): Trim / Extend / Rotate / Mirror / Offset / Explode ...
修改(&M)	&Modify
修改-修剪/延伸	Modify-Trim/Extend
修改-倒角	Modify-Chamfer
修改-偏移	Modify - Offset
修改-分解	Modify - Explode
修改-反向	Modify-Reverse
修改-圆角	Modify-Fillet
修改-复制	Modify-Copy
修改-复制移动	Modify-Copy Move
修改-对齐	Modify-Align
修改-属性模板	Modify - apply property template
修改-延长	Modify-Lengthen
修改-打断	Modify-Break
修改-拉伸	Modify-Stretch
修改-文本分解	Modify-Text Explode
修改-旋转	Modify - Rotate
修改-旋转两图元	Modify-Rotate Two Entities
修改-环形阵列	Modify-Polar Array
修改-矩形阵列	Modify-Rectangular Array
修改-移动并旋转	Modify-Move and Rotate
修改-等分	Modify - Divide
修改-线间隙	Modify-Line Gap
修改-缩放	Modify-Scale
修改-路径阵列	Modify-Path Array
修改-连合	Modify-Join
修改-连接	Modify-Connect
修改-镜像	Modify-Mirror
修改单位	Modify Units
修改命令	Modify Command
修改图层样式 	Modify Layer Style 
修改图纸界限	Modify Drawing Limits
修改块属性 	Modify Block Properties 
修改布局 	Modify Layout 
修改捕捉设置	Modify Snap Settings
修改文字样式 	Modify Text Style 
修改既有文字内容	Modify Existing Text Content
修改标注值	Modify Dimension Value
修改标注数值，几何按关联联动调整，标注/文字/域同步更新	Modify the dimension value; the geometry adjusts associatively, and dimensions / text / fields update in sync
修改标注样式 	Modify Dimension Style 
修改比例 %1	Modify Scale %1
修改追踪设置	Modify Tracking Settings
修饰符（勾选即纳入框格）：	Modifiers (check to include in the frame):
倍数须为正数	Multiplier must be a positive number
倍率无效	Invalid Scale Factor
倒角	Chamfer
倒角...	Chamfer...
倒角失败：	Chamfer failed:
倒角矩形	Chamfered Rectangle
倒角矩形：左侧输入倒角距离(mm)，点击对角两点	Chamfered Rectangle: enter the chamfer distance (mm) on the left, then click two opposite corner points
倒角需要 2 条选中直线	Chamfer requires 2 selected lines
候选: 	Candidates: 
值	Value
值：	Value:
倾斜角：	Tilt Angle:
偏移	Offset
偏移...	Offset...
偏移失败	Offset failed
偏移失败（线 / 圆 / 圆弧才有意义）	Offset failed (only meaningful for lines / circles / arcs)
偏移距离 (mm，左正右负)	Offset Distance (mm, positive to the left, negative to the right)
停止	Stop
停止录制	Stop Recording
停用	Disable
允许离屏渲染回退（预留）	Allow offscreen rendering fallback (reserved)
先框选一组平行线或圆阵列；baseline=共基点基线，continue=链式连续	First window-select a group of parallel lines or a circle array; baseline=baseline with a shared base point, continue=chained continuous
先选中闭合边界图元（圆/矩形/闭合折线等）；无参数 = 默认填充	First select a closed boundary entity (circle / rectangle / closed polyline, etc.); no parameter = default hatch
先选中零部件几何或给定图元 id；自动按质心放置圆+编号球标并追加明细行	First select part geometry or specify an entity id; a circle + numbered balloon is automatically placed at the centroid and a BOM row is appended
全局线型比例 LTSCALE = %1	Global linetype scale LTSCALE = %1
全模型重算 FIELD 域（MText/Table 预热求值，异常回落 ###）	Recompute all FIELD fields in the model (MText/Table pre-evaluation, fall back to ### on exception)
全选	Select All
全选区域	Select All Regions
全部 7 条规则均合规。	All 7 rules are compliant.
全部不选	Deselect All
全部丢弃	Discard All
全部关闭	Turn All Off
全部开启	Turn All On
全部显示	Show All
全部清空	Clear All
全部满足（AND）	All Satisfied (AND)
全部选择	Select All
公切线	Common Tangent
公司/单位：	Company/Organization:
公差值，如 0.02	Tolerance value, e.g. 0.02
公差值：	Tolerance Value:
公称规格：	Nominal Specification:
共 %1 个图元。	Total %1 element(s).
共 %1 个边界，分 %2 个区域。选中区域后可填充 / 全选 / 重命名 / 删除。	Total %1 boundaries in %2 regions. After selecting a region, you can hatch / select all / rename / delete.
共 %1 种几何约束：	Total %1 types of geometric constraints:
共发现 %1 个不合规项。	Found %1 non-compliant items.
共线=Collinear	Collinear =Collinear
关	Off
关于	About
关于 SketchCAD	About SketchCAD
关联填充失败：图元 #%1 不是剖面区域	Associative hatch failed: entity #%1 is not a section region
关联填充失败：所选边界当前不构成闭合区域	Associative hatch failed: the selected boundaries do not currently form a closed region
关联填充失败：未指定边界图元	Associative hatch failed: no boundary entity specified
关联填充失败：边界图元 #%1 不存在	Associative hatch failed: boundary entity #%1 does not exist
关联填充边界 	Associative Hatch Boundary 
关联尺寸标注	Associative Dimension
关联尺寸标注失败：%1	Associative dimension failed: %1
关联开关只接受 on/off	Association switch only accepts on/off
关闭	Close
关闭图纸	Close Drawing
其他	Other
内切圆	Inscribed Circle
内存不足，场景项创建已暂停（已建 %1 项）。图形仍可平移缩放与拾取。	Out of memory: scene item creation paused (%1 items created). Pan, zoom and picking still work.
内存不足，已停止加载。\n\n文件过大或系统可用内存不足，当前图纸未被改动（或已恢复为空图），程序仍可正常使用。	Out of memory, loading stopped.\n\nThe file is too large or the system has insufficient available memory; the current drawing was not modified (or has been restored to an empty drawing), and the program can still be used normally.
内存不足，已停止加载：	Out of memory; loading stopped:
内容	Content
内容（支持 \\n 换行、%<字段>%）：	Content (supports \\n line breaks, %<field>%):
内接圆半径	Inscribed Circle Radius
内置收款码资源加载失败：<code>%1</code><br>请检查 resources/reward/wechat_reward_qr.png 是否已随程序构建。	Failed to load the built-in payment QR code resource: <code>%1</code><br>Please check whether resources/reward/wechat_reward_qr.png was built with the program.
内部错误：命令解释器不可用	Internal error: command interpreter unavailable
内隔墙厚	Interior Wall Thickness
写入不完整：%1	Incomplete write: %1
写入剪贴板失败	Failed to write to clipboard
写入数据库失败：	Failed to write to database:
写入模板失败：	Failed to write template:
写块	Write Block
写块(&W)...	Write Block(&W)...
冲突检测：发现 %1 处问题	Conflict Detection: %1 issues found
冲突检测：未发现冲突或过约束	Conflict Detection: no conflicts or over-constraints found
几何公差	Geometric Tolerance
几何公差创建失败	Failed to create geometric tolerance
几何公差标注	Geometric Tolerance
几何容差 (mm):	Geometric Tolerance (mm):
几何容差(mm)：两点距离/半径差/角度差小于该值视为重合	Geometric tolerance (mm): two-point distance / radius difference / angle difference smaller than this value are treated as coincident
几何约束的添加/求解/检查/诊断/清理	Add / solve / check / diagnose / clean up geometric constraints
几何约束：%1 条未满足，可用 constraint list / check 定位	Geometric constraints: %1 unsatisfied. Use \"constraint list\" or \"check\" to locate them.
凸台外径 Ø	Boss Outer Diameter Ø
凸台孔 Ø	Boss Hole Ø
凸台轴距底面	Boss Axis-to-Bottom Distance
凸面	Convex
凹面	Concave
分区	Partition
分区默认关闭	Partitions disabled by default
分离外部参照	Detach External Reference
分离外部参照 	Detach External Reference 
分离外部参照失败：块 %1 不存在	Failed to detach external reference: block %1 does not exist
分离外部参照失败：块名不能为空	Failed to detach external reference: block name cannot be empty
分离失败：%1 不是外部参照（普通块请用「块管理器」删除）	Detach failed: %1 is not an external reference (for a normal block, use 'Block Manager' to delete it)
分离失败：%1 是嵌套参照，只能在直接附加它的宿主图中分离	Detach failed: %1 is a nested reference and can only be detached in the host drawing to which it is directly attached
分类	Category
分组方式：	Grouping Method:
分解	Explode
分解块	Explode Block
分解块 	Explode Block 
分解失败：图元 #%1 不是块引用	Explode failed: entity #%1 is not a block reference
分解失败：块 %1 未定义或为空	Explode failed: block %1 is undefined or empty
分解样条为线段	Explode Spline into Line Segments
分解样条为线段：将样条炸开为多条独立线段（Explode spline to lines）	Explode Spline into Line Segments: split the spline into multiple independent line segments (Explode spline to lines)
切换到 XY 工作平面（俯视）	Switch to the XY work plane (top view)
切换到 XZ 工作平面（主视）	Switch to XZ Workplane (Front)
切换到 YZ 工作平面（左视）	Switch to YZ Workplane (Left)
切换到指定工作空间	Switch to Specified Workspace
切换可见性状态 	Toggle Visibility State 
切换后立即生效；「跟随系统」会随 Windows 深浅色自动变化。	Takes effect immediately after switching; 'Follow System' changes automatically with Windows light/dark mode.
切换尺寸标注遵循的制图标准：箭头形式 / 文字方向 / 公差规则随之改变	Switch the drafting standard followed by dimensions: arrow style / text direction / tolerance rules change accordingly
切换折线段的类型（直线↔弧）	Toggle Polyline Segment Type (Line <-> Arc)
切换折线闭合状态（PEDIT Close）	Toggle polyline closed state (PEDIT Close)
切换活动图层失败：图层 %1 不存在	Failed to switch active layer: layer %1 does not exist
切换绘图平面	Switch Drawing Plane
切线	Tangent
列出可用工作空间	List Available Workspaces
列出图元的类型/图层/几何信息	List entity type / layer / geometry information
列出块定义的动态参数	List Dynamic Parameters of a Block Definition
列出块引用属性（无 id = 全部引用）	List block reference attributes (no id = all references)
列出当前全部别名	List All Current Aliases
列出当前图形中所有图元的类型与数量（大型图纸慎用，可优先用 getSelection）。	List the types and counts of all entities in the current drawing (use with caution on large drawings; prefer getSelection).
列宽：	Column Width:
列数	Column Count
列数：	Column Count:
列表	List
列间距	Column Spacing
创建/应用命名选择过滤器	Create/Apply Named Selection Filter
创建后行为：	Behavior After Creation:
创建块	Create Block
创建块 	Create Block 
创建时间	Creation Time
创建等距折线（距离 = 鼠标到折线质心）	Create equidistant polyline (distance = mouse to polyline centroid)
创建编组	Create Group
创建编组 %1	Create Group %1
创建编组失败：所选对象均为外部参照（Xref），禁止跨参照边界编组	Failed to create group: all selected objects are external references (Xref); grouping across reference boundaries is not allowed
创建编组失败：编组名或对象为空	Failed to create group: group name or objects are empty
删除	Delete
删除\tDel	Delete\tDel
删除两点之间	Delete Between Two Points
删除两点之间：两次点击两个控制点，删除中间段	Delete Between Two Points: click two control points to delete the segment between them
删除区域	Delete Region
删除原图元并放置块引用（BLOCK 语义）	Delete the source entities and place a block reference (BLOCK semantics)
删除图层 	Delete Layer 
删除图档失败：	Failed to delete drawing archive:
删除块	Delete Block
删除块 	Delete Block 
删除失败	Delete failed
删除失败。	Delete failed.
删除失败（不存在或为 WORLD）: 	Delete failed (does not exist or is WORLD): 
删除布局 	Delete Layout 
删除折线上两节点之间的部分	Delete the portion between two nodes on a polyline
删除折线上的节点	Delete Nodes on a Polyline
删除指定图元（或当前选择集）	Delete specified entities (or the current selection set)
删除按钮失败（工具条：%1，序号：%2）	Failed to delete button (toolbar: %1, index: %2)
删除文字样式 	Delete Text Style 
删除条件	Delete Condition
删除标注样式 	Delete Dimension Style 
删除样条点	Delete Spline Point
删除样条点：选择样条并点击要删除的控制点	Delete Spline Point: select a spline and click the control point to delete
删除模板失败：	Failed to delete template:
删除确认	Confirm Delete
删除边界区域	Delete Boundary Region
删除过滤器 	Delete Filter 
删除选中	Delete Selected
删除选中图元	Delete Selected Entities
删除选中图元 (Delete)	Delete Selected Entities (Delete)
删除重复对象	Delete Duplicate Objects
删除重复对象(&O)	Delete Duplicate Objects(&O)
别名	Alias
别名列表	Alias List
别名：%1	Alias: %1
别名：%1 → %2	Alias: %1 → %2
刷新	Refresh
前端	Front
前端追加	Append at Front
前起落架距机头	Nose Gear to Nose Distance
剖面区域 #%1 已关联 %2 个边界图元（随边界自动更新）	Section region #%1 has been associated with %2 boundary entities (updates automatically with the boundary)
剖面区域 #%1 已断开边界关联（回到静态填充）	Section region #%1 has been detached from boundary association (back to static hatch)
剖面区域 #%1 的边界已消失或不成闭合区域，填充已标记失效（可用 HATCHEDIT 解除关联或重填）	The boundary of section region #%1 has disappeared or no longer forms a closed region; the hatch is marked invalid (use HATCHEDIT to remove the association or re-hatch)
剖面区域 #%1 边界已恢复，随动更新	Section region #%1 boundary restored, updating with the boundary
剖面区域 #%1：图案=%2 比例=%3 角度=%4° 关联=%5%6	Section region #%1: pattern=%2 scale=%3 angle=%4° associative=%5%6
剖面区域=Hatch	Section region =Hatch
剖面填充	Section Hatch
剖面填充(&H)...	Hatch(&H)...
剖面填充参数	Hatch Parameters
剖面填充参数(&P)...	Hatch Parameters(&P)...
剖面填充参数已设为：%1（角度 %2°、间距 %3mm）；下次点选填充即按此生效。若想改已填充区域，请先选中该剖面区域。	Hatch parameters set to: %1 (angle %2°, spacing %3mm); the next hatch click will use them. To change an already hatched area, first select that section region.
剖面填充参数：角度 %1°、间距 %2mm（继续在区域内单击即可填充）	Hatch parameters: angle %1°, spacing %2mm (keep clicking inside the region to hatch)
剖面填充（GB/T 4457.5-2013）	Hatch (GB/T 4457.5-2013)
剖面填充（区域）	Hatch (Region)
剖面填充（选中）	Hatch (Selected)
剖面填充：%1	Hatch: %1
剖面填充：已填充 1 个区域（边界 %1 段%2；角度 %3°，间距 %4mm）	Hatch: hatched 1 region (boundary %1 segments%2; angle %3°, spacing %4mm)
剖面填充：已退出区域选择	Section Hatch: exited region selection
剖面填充：该区域无法生成剖面线，已跳过	Hatch: hatch lines could not be generated for this region, skipped
剖面填充：请在要填充的封闭区域内部单击（可连续点击多个区域）；Enter 调整参数（角度/间距/材料）、Esc 或右键退出。	Hatch: click inside the closed region to be hatched (you can click multiple regions in succession); Enter to adjust parameters (angle/spacing/material), Esc or right-click to exit.
剖面线角度	Hatch Line Angle
剖面线间距	Hatch Line Spacing
剪切	Cut
剪切：	Cut:
剪切：Shift+点击用于指定边界，请点在图元上	Cut: Shift+click is used to specify a boundary; click on an entity
剪切：已取消边界 #%1（当前 %2 条）	Cut: boundary #%1 removed (currently %2)
剪切：已指定边界 #%1（当前 %2 条），再点击要剪掉的部分	Cut: boundary #%1 specified (currently %2); now click the part to cut off
剪切：指定边界已重置	Cut: specified boundary reset
剪切：该图元已被指定为边界，不能同时作为剪切目标	Cut: this entity has already been specified as a boundary and cannot be a cut target at the same time
剪切：请点击要剪掉的那一段（Shift+点击可先指定边界）	Cut: click the segment to cut off (Shift+click to first specify a boundary)
剪力墙厚	Shear Wall Thickness
剪贴板(&L)	C&lipboard
剪贴板-粘贴	Clipboard - Paste
剪贴板-粘贴为块	Clipboard - Paste as Block
剪贴板为空	Clipboard is empty
剪贴板内容不可用	Clipboard content is unavailable
剪贴板内容为空	Clipboard content is empty
功能当前不可用：%1	Feature currently unavailable: %1
功能菜单：未实现的入口	Feature menu: entry not implemented
加载 CUIx	Load CUIx
加载别名	Load Alias
加载外部块库	Load External Block Library
加载外部块库...	Load External Block Library...
加载工作空间	Load Workspace
加载插件	Load Plugin
加载插件文件...	Loading Plugin File...
加载界面定制文件（CUIx）	Load Interface Customization File (CUIx)
加载界面配置	Load Interface Configuration
加载界面配置 (CUIx)...	Load Interface Configuration (CUIx)...
加长图幅	Extend Sheet
动态参数列表	Dynamic Parameter List
动态参数设置失败：图元 #%1 不是块引用	Failed to set dynamic parameters: entity #%1 is not a block reference
动态块参数	Dynamic Block Parameters
动态输入	Dynamic Input
动态输入 (DYN)：%1	Dynamic Input (DYN): %1
动态输入 DYN：光标旁实时坐标提示	Dynamic Input DYN: real-time coordinate hints next to the cursor
动态输入开关	Dynamic Input Toggle
助手：	Assistant:
助手：Assistant	Assistant: Assistant
勾选 = 该视口内不输出（其他视口与模型空间不受影响）：	Checked = do not plot in this viewport (other viewports and model space are unaffected):
勾选后颜色/线型/线宽随所属图层变化；取消则冻结为当前显示值	When checked, color / linetype / lineweight follow the owning layer; when unchecked, they are frozen at the current display values
匹配维度	Match Dimension
区域 = 共享同一名称的若干「边界」图元。\n画好外框与内孔得到若干边界后，归为同一区域即可一次填充（无需逐段描边）。	A region = several 'boundary' entities sharing the same name.\nAfter drawing the outer frame and inner holes to obtain several boundaries, group them into one region to hatch them all at once (no need to trace each segment).
区域1	Region 1
区域「%1」已填充 %2 个边界	Region '%1' hatched %2 boundaries
区域「%1」已重命名为「%2」	Region '%1' renamed to '%2'
区域「%1」无可填充的边界	Region '%1' has no hatchable boundaries
半径	Radius
半径无效	Invalid radius
半径标注	Radius Dimension
半径标注=Radius annotation	Radius =Radius annotation
半径标注：点选圆/圆弧，点击方位即引出方向（R 前缀）	Radius Dimension: pick a circle/arc, then click a direction to define the leader (R prefix)
单个多页 PDF	Single Multi-Page PDF
单位	Units
单位与图纸界限 (UNITS / LIMITS)	Units and Drawing Limits (UNITS / LIMITS)
单位与图纸界限...	Units and Drawing Limits...
单元格支持 %<DATE>% / %<FILENAME>% 等字段占位，\n渲染与打印时自动求值。\n\n公式列：单元格以 = 开头即公式，\n=sum(2,9) 求第 2~9 行本列之和，\n=count(2,9) 统计非空单元格数（明细栏\n数量/重量列末行常用，上游改动自动重算）。\n\n表格可整体选中/移动，\n双击表格重新打开本编辑器。	Cells support field placeholders such as %<DATE>% / %<FILENAME>%,\nautomatically evaluated during rendering and plotting.\n\nFormula columns: a cell starting with = is a formula,\n=sum(2,9) sums rows 2~9 of this column,\n=count(2,9) counts non-empty cells (commonly used in the last row\nof the Quantity/Weight columns in the BOM; recalculates automatically on upstream changes).\n\nThe table can be selected/moved as a whole,\ndouble-click the table to reopen this editor.
单次回复上限：	Single Reply Limit:
单行文字	Single-Line Text
单行文字(&T)...	Single-Line Text(&T)...
单重	Unit Weight
卫生间门宽	Bathroom Door Width
压力角：	Pressure Angle:
厚度(mm)：	Thickness (mm):
原图纸已存在：\n%1\n\n「是」= 用快照覆盖它；「否」= 另存为新文件；「取消」= 不恢复。	The original drawing already exists:\n%1\n\n'Yes' = overwrite it with the snapshot; 'No' = save as a new file; 'Cancel' = do not restore.
原图纸路径	Original Drawing Path
原文件已存在	Original File Already Exists
参数 %1	Parameter %1
参数 1 不是数字	Parameter 1 is not a number
参数 2 不是数字	Parameter 2 is not a number
参数值「%1」既不是数值、也不是已声明的可见性状态	Parameter value '%1' is neither a number nor a declared visibility state
参数化变量表	Parametric Variable Table
参数化户型平面图（建筑装修专业，默认 C户型 101M² 三房两厅两卫）。\n墙体（剪力墙涂黑 + 双线内墙）、门（开启弧）、窗、家具图块、\n房间标注、四周建筑尺寸链与右侧图签全部可由参数控制。\n单位：毫米（mm），默认比例 1:50，A3 横放。	Parametric floor plan (architectural interior design, default Type C 101M², 3 bedrooms, 2 living rooms, 2 bathrooms).\nWalls (shear walls filled black + double-line interior walls), doors (swing arcs), windows, furniture blocks,\nroom dimensions, surrounding dimension chains, and the right-side title block are all controlled by parameters.\nUnits: millimeters (mm), default scale 1:50, A3 landscape.
参数无效	Invalid parameter
参数无效（行/列 ≥1，间距可为 0）	Invalid parameter (rows/columns ≥1, spacing can be 0)
参数表为空	Parameter table is empty
参数表（%1 项）：	Parameter table (%1 items):
参数解析失败	Failed to parse parameters
参数输入条	Parameter Input Bar
参数错误	Parameter Error
参数驱动	Parameter Driven
参数驱动失败：%1	Parameter driving failed: %1
参数驱动：%1 个变量已应用并重解约束	Parameter driving: %1 variables applied and constraints re-solved
参数：%1	Parameter: %1
参照图形文件（*.json / *.dxf / *.dwg）	Reference drawing files (*.json / *.dxf / *.dwg)
参照图形：	Reference Drawing:
参考图元不存在	Reference entity does not exist
参考图元已不存在	The reference entity no longer exists
参考尺寸 (mm)	Reference Dimension (mm)
参考线垂线	Perpendicular to Reference Line
双击块 → 在视图中心插入。「当前图块表」随本图块表实时刷新；「外部文件块表」来自 SketchCAD 存档(.json) / DXF(.dxf)。	Double-click a block → insert at the center of the view. 'Current Block Table' refreshes in real time with this block table; 'External File Block Table' comes from SketchCAD archives (.json) / DXF (.dxf).
双折线=Double break line	Break line =Double break line
双曲线	Hyperbola
双曲线：依次点击两个焦点和曲线上一点	Hyperbola: click the two foci and a point on the curve in sequence
双点划线=Double dash line	Double dash-dot =Double dash line
双面焊（基准线上）	Double-Sided Weld (on datum line)
反向	Reverse
反转折线顶点顺序	Reverse polyline vertex order
反选	Invert Selection
发动机半翼展	Engine Semi-Span
发动机距机头	Engine to Nose Distance
发布	Publish
发布 (PUBLISH)	Publish (PUBLISH)
发布... (PUBLISH)	Publish... (PUBLISH)
发布失败：未生成任何页	Publish failed: no pages were generated
发布失败：未生成任何页。	Publish failed: no pages were generated.
发布完成：%1 页 → %2	Publish complete: %1 pages → %2
发现 %1 个不合规项（双击可跳转到图元）：	Found %1 non-compliance item(s) (double-click to jump to the element):
发送	Send
取消	Cancel
取消固定	Unfix
取消当前绘制  (Esc)	Cancel Current Draw  (Esc)
取消按层选择	Deselect by Layer
取消相交	Deselect Intersecting
取消相交：点击图元，取消与之相交者的选中	Deselect Intersecting: click an entity to deselect those intersecting it
取消选择图元	Deselect Entities
取消选择实体	Deselect Entity
取消选择实体：点击图元取消其选中	Deselect Entity: click an entity to deselect it
取消隔离	Unisolate
变量	Variable
变量 %1 = %2 已更新，但未关联任何模型标注	Variable %1 = %2 updated, but not associated with any model dimension
变量 %1 驱动值未变化（= %2），无需重解	Variable %1 driving value unchanged (= %2), no need to re-solve
变量名：	Variable Name:
变量已定义：%1 = %2	Variable defined: %1 = %2
变量驱动：%1 = %2	Variable driving: %1 = %2
另存/覆盖时保留的历史备份（.bak）份数；0 = 不保留	Number of historical backups (.bak) kept on save as / overwrite; 0 = do not keep
另存为	Save As
另存为模板	Save As Template
另存当前图纸为模板...	Save Current Drawing as Template...
只保留选中图元所在图层可见	Keep only the layers of selected entities visible
只填密钥本身，不要把接口 URL 填到这里。云端后端留空会返回 HTTP 401。\nDeepSeek 密钥在 https://platform.deepseek.com/api_keys 创建。	Enter only the API key itself; do not put the API URL here. Leaving the cloud backend blank will return HTTP 401.\nCreate a DeepSeek key at https://platform.deepseek.com/api_keys.
可用修饰符：	Available Modifiers:
可用区宽(mm)：	Usable Area Width (mm):
可用区高(mm)：	Usable Area Height (mm):
可用脚本引擎	Available Script Engines
可用脚本引擎：%1	Available Script Engines: %1
可见性参数 %1 未声明状态「%2」	Visibility parameter %1 has no declared state '%2'
可见性状态设置失败：图元 #%1 不是块引用	Failed to set visibility state: entity #%1 is not a block reference
台阶孔 Ø	Counterbore Ø
右	Right
右上	Top Right
右上 X：	Top Right X:
右上 Y：	Top Right Y:
右下	Bottom Right
右中	Middle Right
右对齐	Right Align
各列宽度(mm)，逗号分隔；数量少于列数时余下列用 30mm	Width of each column (mm), comma-separated; if fewer values than columns, the remaining columns use 30mm
合并/删除重复或重叠图元	Merge/Delete Duplicate or Overlapping Entities
合并来源必须是直线/圆弧/折线	Merge sources must be lines/arcs/polylines
合并目标必须是折线	Merge target must be a polyline
合规	Compliant
合规检查	Compliance Check
合规检查(AI)	Compliance Check (AI)
合规检查结果（ComplianceChecker::run，7 类 GB/T 规则；纯读，不修改图纸、不执行任何命令）：	Compliance Check Results (ComplianceChecker::run, 7 categories of GB/T rules; read-only, does not modify the drawing or execute any commands):
合计 %1 个封闭区域：总面积 = %2 mm²	%1 closed regions in total: total area = %2 mm²
同心=Concentric	Concentric =Concentric
同心圆	Concentric Circles
同心圆：拾取圆/弧，左侧输入半径或点击设半径	Concentric Circles: pick a circle/arc, enter the radius in the left panel or click to set the radius
同时清除隐藏图元 (visible=false)【可能移除有意隐藏的内容】	Also clear hidden entities (visible=false) [may remove intentionally hidden content]
名称	Name
名称不能为空。	Name cannot be empty.
名称无效（不能为空或 WORLD）	Invalid name (cannot be empty or WORLD)
后端	Backend
后端类型：	Backend Type:
后端追加	Backend Append
向视字母	View Direction Letter
向视符号	View Direction Symbol
向视符号：生成失败（方向/箭长非法）	View direction symbol: generation failed (invalid direction/arrow length)
向视箭头	View Direction Arrow
启用	Enable
启用 GPU 加速渲染（预留）	Enable GPU-accelerated rendering (reserved)
启用匿名遥测（默认关闭）	Enable anonymous telemetry (off by default)
吸附/栅格步长：	Snap/Grid Step:
周围焊	All-Around Weld
周长 %1	Perimeter %1
命令>	Command>
命令列表	Command List
命令别名列表	Command Alias List
命令别名（%1 条）：	Command Aliases (%1):
命令总数：%1（别名 %2 条，单列如下）	Total commands: %1 (aliases %2, listed below)
命令总表；help <命令> 查看用法/参数/样例	Full command list; help <command> for usage/parameters/examples
命令执行失败	Command execution failed
命令行	Command Line
命令面板	Command Palette
命名 UCS 共 %1 个：	Named UCSs (%1):
四点抛物线	4-Point Parabola
回放宏	Replay Macro
回放已保存的宏	Play Back Saved Macro
固定=Fixed	Fix =Fixed
固定到列表	Pin to List
固定点=FixedPoint	Fixed point =FixedPoint
国标图库：规格表为空	GB standard parts library: specification table is empty
图上长度:	Drawing Length:
图元 #%1 不是样条曲线	Entity #%1 is not a spline
图元 ID	Entity ID
图元 id 无效	Invalid entity id
图元:	Entity:
图元:0	Entity:0
图元不存在	Entity does not exist
图元不存在或点数不足（需 ≥2 点）	Entity does not exist or has too few points (≥2 points required)
图元前两点重合，无法确定 X 轴	The first two points of the entity coincide; the X axis cannot be determined
图元总数 %<COUNT>%	Total Entities %<COUNT>%
图元数	Entity Count
图元面板	Entity Panel
图元颜色：预设色板 + 自定义取色；已选中图元时同时改其颜色	Entity color: preset palette + custom color picker; also changes the color of selected entities
图层	Layer
图层 %1 不存在	Layer %1 does not exist
图层 %1 已改名为 %2	Layer %1 renamed to %2
图层 %1 打印已%2	Layer %1 plot %2
图层 %1 暂无图元	Layer %1 has no entities
图层 %1 透明度 = %2	Layer %1 transparency = %2
图层匹配	Layer Match
图层名(如 0)	Layer name (e.g. 0)
图层命名建议（GB/T 18617 / ISO 13567）：	Layer Naming Recommendations (GB/T 18617 / ISO 13567):
图层样式	Layer Style
图层的增删改/显隐/锁定/赋层/套用/列表（单一分派命令）	Layer add/delete/edit, show/hide, lock, assign, apply, list (single dispatch command)
图层隔离	Layer Isolate
图幅	Sheet Size
图形清理	Drawing Cleanup
图形清理 (PURGE / OVERKILL)	Drawing Cleanup (PURGE / OVERKILL)
图形清理(&C)...	Drawing Cleanup(&C)...
图框边距	Drawing Frame Margin
图框间距(mm)：	Drawing Frame Spacing (mm):
图案填充	Hatch
图案填充(&H)...	Hatch(&H)...
图案预览	Pattern Preview
图档	Drawing Archive
图签字高	Title Block Text Height
图签宽	Title Block Width
图纸(&G)	Drawing(&G)
图纸「%1」有未保存的修改，是否保存？	Drawing '%1' has unsaved changes. Save?
图纸中未找到图框（图框层矩形）作为排图输入	No drawing frame (rectangle on the drawing frame layer) found in the drawing as nesting input
图纸中未找到图框（图框层矩形），无法按图框分组	No drawing frame (rectangle on the drawing frame layer) found in the drawing; cannot group by drawing frame
图纸名称	Drawing Title
图纸宽	Sheet Width
图纸幅面 (GB/T 14689 / ISO 128)	Sheet Size (GB/T 14689 / ISO 128)
图纸恢复	Drawing Recovery
图纸文件 (*.skd *.json);;所有文件 (*.*)	Drawing Files (*.skd *.json);;All Files (*.*)
图纸特性	Drawing Properties
图纸特性 %<PROP:名称>%	Drawing Properties %<PROP:Name>%
图纸界限（LIMITS，模型 mm）	Drawing Limits (LIMITS, model mm)
图纸高	Sheet Height
图面暂无边界图元。请先用「边界(BOUNDARY)」工具绘制。	No boundary entities in the drawing area. Please create them first with the 'Boundary (BOUNDARY)' tool.
图面暂无边界图元。请先用「边界(BOUNDARY)」工具绘制外框与内孔。	No boundary entities in the drawing area. Please draw the outer boundary and inner holes first with the 'Boundary (BOUNDARY)' tool.
圆	Circle
圆 (圆心+点)：点击圆心与圆周点	Circle (Center + Point): Click the center and a point on the circumference
圆-圆公切线	Circle-Circle Common Tangent
圆=Circle	Circle =Circle
圆弧	Arc
圆弧 (Arc)：圆心 + 起点 + 终点	Arc (Arc): Center + Start + End
圆弧=Arc	Arc =Arc
圆弧绘制方式	Arc Drawing Method
圆弧：半径为 0 或无效，已忽略	Arc: radius is 0 or invalid; ignored
圆弧：半径为 0，已忽略	Arc: radius is 0, ignored
圆形绘制方式	Circle Drawing Method
圆心 + 半径 + 起止角绘制圆弧	Draw an arc from center, radius and start/end angles
圆心 + 半径绘制圆	Draw a circle from center and radius
圆心+半径	Center + Radius
圆心+半径+弧角：左侧输入半径和扫掠角度	Center + Radius + Arc Angle: enter the radius and sweep angle in the left panel
圆心+半径：在左侧输入半径(mm)，点击圆心即绘制	Center + Radius: enter the radius (mm) in the left panel, then click the center to draw
圆心+点+弦长	Center + Point + Chord Length
圆心+点+弦长：点击圆心和圆周点，左侧输入弦长	Center + Point + Chord Length: click the center and a point on the circle, and enter the chord length in the left panel
圆心+点+弧角	Center + Point + Arc Angle
圆心解析失败	Failed to parse circle center
圆筒外径 Ø	Cylinder OD Ø
圆筒轴向长	Cylinder Axial Length
圆角	Fillet
圆角...	Fillet...
圆角半径 (mm，=0 即倒角)	Fillet Radius (mm, =0 for chamfer)
圆角失败（两线平行或不相交）	Fillet failed (lines are parallel or do not intersect)
圆角矩形	Rounded Rectangle
圆角矩形：左侧输入圆角半径(mm)，点击对角两点	Rounded Rectangle: enter the corner radius (mm) in the left panel, then click two opposite corners
圆角需要 2 条选中直线	Fillet requires 2 selected lines
圆：半径为 0 或无效，已忽略	Circle: radius is 0 or invalid, ignored
圆：半径为 0，已忽略	Circle: radius is 0, ignored
圈交	Crossing Polygon
圈交(CP)：画一个多边形，与多边形相交或在其内部的图元加选	Crossing Polygon (CP): draw a polygon; entities intersecting or inside it are added to the selection
圈围	Window Polygon
圈围(WP)：画一个多边形，完全位于多边形内部的图元加选	Window Polygon (WP): draw a polygon; entities entirely inside it are added to the selection
在下方直接拖动图元即可修改块内几何；确定后所有引用实时同步。Esc / 取消丢弃不落盘。	Drag entities below to edit the geometry inside the block; after confirming, all references sync in real time. Esc / Cancel discards changes without saving.
在位编辑块	Edit Block In-Place
在位编辑块 	Edit Block In-Place 
在位编辑块 — %1	Edit Block In-Place — %1
在当前布局图纸坐标建矩形视口	Create a rectangular viewport at the current layout sheet coordinates
在折线段上插入节点	Insert a vertex on a polyline segment
在折线首/末端追加节点	Append a vertex at the start/end of the polyline
在指定位置放置形位公差框格	Place a geometric tolerance frame at a specified position
在指定点把图元一分为二	Split an entity into two at a specified point
在视图中拾取中心点	Pick the center point in the view
地脚槽宽度	Anchor Slot Width
地脚槽深度	Anchor Slot Depth
坐标: X = %1    Y = %2    Z = %3	Coordinates: X = %1    Y = %2    Z = %3
坐标无效	Invalid coordinates
坐标标注	Ordinate Dimension
坐标标注=Coordinate annotation	Ordinate =Coordinate annotation
坐标标注：折线引出 + X 或 Y 坐标值，用于孔系定位	Ordinate dimension: polyline leader + X or Y coordinate value, used for hole pattern positioning
坐标读数已切换为按 %1 显示	Coordinate readout switched to display by %1
坐标读数当前按 %1 显示	Coordinate readout currently displayed by %1
块	Block
块 #%1 动态参数 %2 = %3	Block #%1 dynamic parameter %2 = %3
块 #%1 可见性 %2 = %3	Block #%1 visibility %2 = %3
块 #%1 属性 %2 = %3	Block #%1 attribute %2 = %3
块 %1 不存在	Block %1 does not exist
块 %1 不存在（先用 block create 创建）	Block %1 does not exist (create it with block create first)
块 %1 不存在，无法在位编辑	Block %1 does not exist; cannot edit in place
块 %1 不存在，无法重定义	Block %1 does not exist; cannot redefine
块 %1 创建成功（基点 %2,%3%4）	Block %1 created successfully (base point %2,%3%4)
块 %1 已存在	Block %1 already exists
块 %1 已存在（如需更新块内几何请用「重定义块」）	Block %1 already exists (to update the geometry inside the block, use 'Redefine Block')
块 %1 已改名为 %2（%3 个引用同步）	Block %1 renamed to %2 (%3 references synced)
块 %1 已新增属性定义 %2（位置 %3,%4，字高 %5）	Block %1: attribute definition %2 added (position %3,%4, text height %5)
块 %1 没有动态参数	Block %1 has no dynamic parameters
块 %1：基点(%2,%3)，块内 %4 个图元，属性 %5，动态参数 %6%7	Block %1: base point (%2,%3), %4 entities in the block, attributes %5, dynamic parameters %6%7
块(&B)	&Block
块「%1」\n图元数：%2\n属性定义：%3\n动态参数：%4	Block '%1'\nEntity count: %2\nAttribute definitions: %3\nDynamic parameters: %4
块「%1」已存在，请换名或用「块管理器」重定义。	Block '%1' already exists. Please choose another name or redefine it with the 'Block Manager'.
块「%1」的新名称：	New name for block '%1':
块名不能为空	Block name cannot be empty
块名不能为空。	Block name cannot be empty.
块名（必填，须唯一）	Block name (required, must be unique)
块名：	Block name:
块库文件 (*.json *.dxf);;SketchCAD 存档 (*.json);;DXF 图纸 (*.dxf);;所有文件 (*)	Block Library Files (*.json *.dxf);;SketchCAD Archive (*.json);;DXF Drawings (*.dxf);;All Files (*)
块引用=BlockRef	Block reference =BlockRef
块的创建/插入/重定义/分解/删除/重命名/列表（单一分派命令）	Block create/insert/redefine/explode/delete/rename/list (single dispatch command)
块管理器	Block Manager
坡口深 s	Groove Depth s
垂尾高度	Vertical Tail Height
垂直=Perpendicular	Perpendicular =Perpendicular
垂直与角度	Perpendicular && Angle
垂直参考线切线	Tangent to Vertical Reference Line
垂直居中	Center Vertically
垂直线	Vertical Line
垂线生成	Perpendicular Line Generation
域更新	Field Update
基准字母：	Datum Letter:
基准符号	Datum Symbol
基准（可空，多基准空格分隔，如 A B）	Datum (optional; separate multiple datums with spaces, e.g. A B)
基准：	Datum:
基本	Basic
基本符号：	Basic Symbol:
基点 X	Base Point X
基点 X (端点坐标)	Base Point X (endpoint coordinates)
基点 Y	Base Point Y
基点解析失败	Failed to resolve the base point
基线	Baseline
基线标注	Baseline Dimension
基线标注=Baseline annotation	Baseline =Baseline annotation
基线标注：自同一基准线起并行排列的一组尺寸	Baseline dimension: a set of dimensions arranged in parallel from the same datum line
填充区域	Hatch Area
填写标题栏	Fill in Title Block
填写标题栏...	Fill in Title Block...
填该服务实际提供的模型 id。DeepSeek 常用 deepseek-chat（非思考）/ deepseek-reasoner（思考），具体以厂商控制台当前可用模型为准。	Enter the model id actually provided by this service. DeepSeek commonly uses deepseek-chat (non-thinking) / deepseek-reasoner (thinking); check the vendor console for the models currently available.
壁孔径 Ø	Wall Hole Diameter Ø
壁孔数量	Number of Wall Holes
备份保留份数：	Backup Copies to Keep:
备注	Remarks
复制	Copy
复制(&C)	Copy(&C)
复制但基点由用户指定（AutoCAD COPYBASE 语义）	Copy with a user-specified base point (AutoCAD COPYBASE semantics)
复制失败（新名为空或已存在）	Copy failed (new name is empty or already exists)
复制并粘贴	Copy and Paste
复制移动	Copy and Move
外接圆半径	Circumscribed Circle Radius
外部参照	External Reference
外部参照 %1 已附加（如需更换请先「分离」）	External reference %1 already attached (to replace it, use 'Detach' first)
外部参照 %1 的绝对路径：%2	Absolute path of external reference %1: %2
外部参照(%1):	External Reference (%1):
外部参照的附加/分离/列表/路径诊断	External reference attach/detach/list/path diagnostics
外部参照（Xref）	External Reference (Xref)
多段线 #%1 已打开	Polyline #%1 is now open
多段线 #%1 已标记为曲线特征（渲染仍以折线呈现）	Polyline #%1 marked as a curve feature (still rendered as a polyline)
多段线 #%1 已闭合	Polyline #%1 is now closed
多段线编辑	Polyline Edit
多段线编辑(&P)...	Polyline Edit(&P)...
多行文字	Multiline Text
多行文字 (MTEXT)	Multiline Text (MTEXT)
多行文字 (MTEXT)：点击插入点后编辑多行内容、对齐、样式、字段与自动换行	Multiline Text (MTEXT): click the insertion point, then edit multiline content, alignment, style, fields, and auto-wrap
多行文字(&M)...	Multiline Text(&M)...
多行文字(MTEXT)：点击插入点后编辑多行内容、对齐、字高、宽度因子、样式与自动换行	Multiline Text (MTEXT): click the insertion point, then edit multiline content, alignment, text height, width factor, style, and auto-wrap
多行文字=MText	Multiline text =MText
多边形	Polygon
多边形 (Polygon)：自由顶点，右键闭合	Polygon (Polygon): free vertices, right-click to close
多边形=Polygon	Polygon =Polygon
多边形绘制方式	Polygon Drawing Method
多边形：顶点数不足，已忽略	Polygon: not enough vertices; ignored
多重引线=MLeader	Multileader =MLeader
大小	Size
失败	Failed
失败=Failed	Failed =Failed
夹点%1	Grip %1
夹点%1完成：%2 项变更，Ctrl+Z 可整组撤销	Grip %1 done: %2 changes, Ctrl+Z to undo the whole group
夹点%1：没有可作用的图元	Grip %1: no entities to act on
夹点拉伸：移动光标预览，按住拖拽或再次单击确认，Ctrl 切换模式，Esc 取消	Grip stretch: move the cursor to preview, hold and drag or click again to confirm, Ctrl switches mode, Esc cancels
夹点模式：%1（Ctrl 继续切换，移动预览，单击/Enter 确认，Esc 取消）	Grip mode: %1 (Ctrl keeps cycling, move to preview, click/Enter to confirm, Esc to cancel)
套用失败	Apply failed
套用模板	Apply Template
如 H7 / g6	e.g. H7 / g6
如 https://api.deepseek.com/v1（会自动追加 /chat/completions）	e.g. https://api.deepseek.com/v1 (/chat/completions will be appended automatically)
姓名：	Name:
子型：baseline / continue / angle / radius / dia / arc / leader / ordinate	Subtype: baseline / continue / angle / radius / dia / arc / leader / ordinate
子模式：xline_h / xline_v / xline_angle / xline_bisector / xline_parallel	Submode: xline_h / xline_v / xline_angle / xline_bisector / xline_parallel
孔径(mm)：	Hole Diameter (mm):
孔轴投影	Shaft Projection
孔轴投影：生成失败	Hole axis projection: generation failed
字体：	Font:
字分解	Text Explode
字号	Text Size
字号(0=派生)	Font Size (0=derived)
字形：	Font Style:
字段：	Field:
字高	Text Height
字高(mm)：	Text Height (mm):
字高须为正数	Text height must be positive
字高：	Text Height:
存为模板	Save as Template
存在 %1 个不合规项，按上述建议修正。	%1 non-compliant items found; fix them according to the suggestions above.
存在无法解析的行（已跳过）：\n%1	Unparseable lines (skipped):\n%1
完整路径	Full Path
宏已保存：%1（共 %2 条命令）	Macro saved: %1 (%2 commands in total)
宏录制已停止：共 %1 条命令（macrosave 保存 / macroshow 查看）	Macro recording stopped: %1 commands in total (macrosave to save / macroshow to view)
宏录制开始（自动保存到 %1）：macrostop 结束并落盘	Macro recording started (auto-saved to %1): macrostop ends and saves to disk
宏录制开始：此后执行的命令将逐条记录（macrostop 结束，macrosave 保存，macroshow 查看）	Macro recording started: subsequent commands will be recorded one by one (macrostop to stop, macrosave to save, macroshow to view)
宏：保存录制...	Macro: Save Recording...
宏：停止录制	Macro: Stop Recording
宏：开始录制	Macro: Start Recording
定义...	Define...
定义变量	Define Variable
定位	Positioning
定位分布圆 Ø	Locating Pitch Circle Ø
定位孔径 Ø	Locating Hole Diameter Ø
定位孔数量	Number of Locating Holes
定位首孔角	First Hole Angle
定数等分	Divide by Number
定角线	Fixed-Angle Line
定距等分	Divide by Distance
定距等分(&D)...	Divide by Distance(&D)...
实时预览	Live Preview
实时预览（线宽 = 2d/3，符合 GB/T 12212）：	Live Preview (line width = 2d/3, conforms to GB/T 12212):
实线=Real line	Solid =Real line
实际长度:	Actual Length:
容差须为正数（mm）	Tolerance must be a positive number (mm)
宽/高必须为正数，且模板参考尺寸需 > 0。	Width/height must be positive numbers, and the template reference size must be > 0.
宽W:	Width W:
宽度分区数:	Number of Width Divisions:
宽度因子	Width Factor
宽度因子：	Width Factor:
密码：	Password:
对两图元倒圆角	Fillet Two Entities
对两图元倒角（距离-距离 / 距离-角度）	Chamfer two entities (distance-distance / distance-angle)
对中符号	Centering Mark
对当前打开的图纸运行工程制图合规检查（ComplianceChecker::run，7 类 GB/T 规则），返回逐规则结论与不合规明细。问「这张图线宽合规吗」「图纸是否符合国标」时必须调用。可选 rule 聚焦某条规则（如「线宽」或 R1_line_width_ratio）；省略返回全部 7 条。纯读、零副作用、不会执行任何绘图命令。	Run an engineering drawing compliance check on the currently open drawing (ComplianceChecker::run, 7 categories of GB/T rules), returning per-rule conclusions and non-compliance details. Must be called when asked 'Is the line width of this drawing compliant?' or 'Does the drawing comply with GB standards?'. The optional rule argument focuses on one rule (e.g. 'line width' or R1_line_width_ratio); omit it to return all 7. Read-only, zero side effects, and no drawing commands are executed.
对称=Symmetric	Symmetric =Symmetric
对象捕捉追踪	Object Snap Tracking
对象捕捉追踪 (F11)：沿捕捉点对齐方向追踪	Object snap tracking (F11): tracks along the alignment direction of snap points
对象捕捉追踪 (OTRACK)	Object Snap Tracking (OTRACK)
对象捕捉追踪 (OTRACK)：%1	Object Snap Tracking (OTRACK): %1
对象捕捉追踪 (OTRACK，依赖对象捕捉获取锚点)	Object Snap Tracking (OTRACK, relies on object snap to acquire anchor points)
对象捕捉追踪开关	Object Snap Tracking Toggle
对象捕捉追踪：%1	Object Snap Tracking: %1
对象类型：	Object Type:
对选中闭合边界生成关联填充，或调整填充参数	Create an associative hatch from the selected closed boundaries, or adjust hatch parameters
对选区或 ID 列表中的封闭图元逐个统计面积/周长/形心	Compute area/perimeter/centroid for each closed entity in the selection or ID list
对齐	Align
对齐(&A)	&Align
对齐...	Align...
对齐单条...	Align Single...
对齐单条：请用「对齐...」并指定参考元素	Align Single: use 'Align...' and specify a reference element
对齐参考点...	Align Reference Point...
对齐参考点：请用「对齐...」并指定参考元素	Align Reference Point: use 'Align...' and specify a reference element
对齐方式	Alignment
对齐方式：	Alignment:
对齐标注	Aligned Dimension
对齐标注=Alignment annotation	Aligned =Alignment annotation
对齐标注：尺寸线平行于两测量点连线，量真实距离	Aligned dimension: dimension line parallel to the line connecting the two measured points, measuring the true distance
对齐：	Alignment:
导入 CTB...	Import CTB...
导入 DWG	Import DWG
导入 DXF	Import DXF
导入失败	Import failed
导入简化 CTB 打印样式表	Import a simplified CTB plot style table
导出 DXF	Export DXF
导出 DXF 时使用的 AutoCAD 版本（R2004+ 可被正版 AutoCAD 识别为该版本并直接另存为 DWG）：	AutoCAD version used when exporting DXF (R2004+ is recognized by genuine AutoCAD as that version and can be saved directly as DWG):
导出 DXF...	Export &DXF...
导出 PDF	Export PDF
导出 PDF 失败	Export PDF failed
导出 PDF...	Export PDF...
导出 PDF（全部布局）	Export PDF (All Layouts)
导出 PDF（全部布局）...	Export PDF (All Layouts)...
导出失败	Export failed
导出失败（无法写入）：	Export failed (cannot write):
导出失败：	Export failed:
导出完成	Export complete
导出成功	Export successful
导出诊断包...	Export Diagnostic Package...
封闭区域的面积/质心/惯性矩等	Area/centroid/moment of inertia, etc. of closed regions
将区域重命名为：	Rename region to:
将打包的文件：	Files to be packaged:
将把所有分类的设置恢复为出厂默认值。\n\n注意：点「是」后仍需点「应用」或「确定」才真正生效。\n是否继续？	All settings in every category will be restored to factory defaults.\n\nNote: after clicking 'Yes', you still need to click 'Apply' or 'OK' for the changes to take effect.\nContinue?
将折线中所有弧段改为直线段	Convert all arc segments in the polyline to line segments
将选中图元移入此层	Move Selected Entities to This Layer
尚未运行合规检查（执行 STDCHECK 后更新）。	Compliance check has not been run yet (updated after running STDCHECK).
就绪	Ready
尺寸=Dimension	Dimension =Dimension
尺寸值	Dimension Value
尺寸数字字高	Dimension Text Height
尺寸无效	Invalid dimension
尺寸标注	Dimensions
尺寸链带宽	Dimension Chain Band Width
尺寸驱动	Dimension Drive
尺寸驱动 #%1 → %2（关联几何与标注已联动更新）	Dimension drive #%1 → %2 (associated geometry and dimensions updated together)
尺寸驱动 #%1：%2 → %3（关联几何与标注已联动更新）	Dimension drive #%1: %2 → %3 (associated geometry and dimensions updated together)
尺寸驱动 (需求 D-③，默认开)：改标注值是否联动调整实际几何；关=仅改标注显示	Dimension driving (D-③, Default): dimension linked; = dimension Display
尺寸驱动已关闭：dimedit 仅更新标注显示文本 #%1 → %2（几何未改动）	Dimension drive is off: dimedit only updates the dimension display text #%1 → %2 (geometry unchanged)
尺寸驱动已关闭：仅更新标注显示文本 #%1	Dimension drive is off: only updated the dimension display text #%1
尺寸驱动已关闭：仅更新标注显示文本 #%1 → %2（几何未改动）	Dimension drive is off: only updated the dimension display text #%1 → %2 (geometry unchanged)
尺寸驱动：%1 条约束未满足，可用 constraint list / check 定位	Dimension drive: %1 constraints not satisfied; use constraint list / check to locate them
尺寸（mm）	Dimension (mm)
居中	Center
属性 %1 为常量属性，不可修改	Attribute %1 is a constant attribute and cannot be modified
属性 %1 已存在	Attribute %1 already exists
属性...	Attributes...
属性定义	Attribute Definition
属性提取	Attribute Extraction
属性提取完成：%1 个块引用	Attribute extraction complete: %1 block references
属性模板...	Property Template...
属性模板套用失败	Failed to apply property template
属性模板：请先选中图元，再套用当前颜色 / 线宽 / 线型	Property template: select entities first, then apply the current color / line width / linetype
属性编辑失败：图元 #%1 不是块引用	Attribute edit failed: entity #%1 is not a block reference
属性面板	Property Panel
属性面板已显示（编辑当前选中）	Properties panel displayed (editing the current selection)
工作平面	Flat Panel
工作平面 %1     %2     当前线宽 %3     拾取容差 %4px	Plane %1     %2     Current line width %3     Pick tolerance %4px
工作空间	Workspaces
工作空间 id：	Workspace id:
工作空间不存在：%1	Workspace does not exist: %1
工作空间列表	Workspace List
工作空间（%1 个，当前：%2）：	Workspaces (%1, current: %2):
工具	Tools
工具(&T)	&Tools
工具条 id 不能为空	Toolbar id cannot be empty
工具条 id（如 mybar），用作 objectName	Toolbar id (e.g. mybar), used as objectName
工具条 id：	Toolbar id:
工具条不存在：%1	Toolbar does not exist: %1
工具条已存在：%1	Toolbar already exists: %1
工程制图合规检查	Engineering Drawing Compliance Check
工程名称	Project Name
工程地址	Project Address
左上	Top Left
左下	Bottom Left
左下 X：	Bottom Left X:
左下 Y：	Bottom Left Y:
左中	Middle Left
左对齐	Align Left
左端沉孔径 Ø	Left Counterbore Diameter Ø
已	Done
已%1 %2 条直线，交于 (%3, %4)	%1 %2 lines, intersecting at (%3, %4)
已%1样条 #%2 的控制点	%1 the control points of spline #%2
已丢弃 %1 份自动保存快照	Discarded %1 autosave snapshots
已丢弃自动保存快照：%1	Discarded autosave snapshot: %1
已为你执行命令，可在画布查看结果。	Command executed. You can view the result on the canvas.
已从工具条 %1 删除第 %2 个按钮	Removed button %2 from toolbar %1
已从自动保存恢复：%1	Restored from autosave: %1
已保存别名文件：%1	Saved alias file: %1
已保存到数据库：%1（id=%2）	Saved to database: %1 (id=%2)
已保存并切换到工作空间：%1（%2 工具条 / %3 菜单）	Saved and switched to workspace: %1 (%2 toolbars / %3 menus)
已保存界面配置：%1	Saved UI configuration: %1
已保存过滤器：	Saved filter:
已保存：%1（%2，%3 字节）%4	Saved: %1 (%2, %3 bytes) %4
已修剪折线 #%1 的第 %2 段	Trimmed segment %2 of polyline #%1
已修剪折线 #%1 第 %2 段	Trimmed segment %2 of polyline #%1
已修改图层 %1 的样式（已有图元属性保持不变）	Modified the style of layer %1 (properties of existing entities remain unchanged)
已修改文字 #%1：%2	Modified text #%1: %2
已偏移 %1 个图元（Δ=%2 mm）	Offset %1 entities (Δ=%2 mm)
已全选 %1 个图元	Selected all %1 entities
已关联尺寸标注 #%1%2，修改其数值即可驱动几何重构	Associated dimension #%1%2; changing its value drives geometry reconstruction
已写块到 %1（%2 个图元，%3 字节）	Wrote block to %1 (%2 entities, %3 bytes)
已冻结	Frozen
已分离外部参照 %1：删除 %2 个参照实例%3%4	Detached external reference %1: removed %2 reference instances%3%4
已分解 %1 个图元 → %2 段：%3	Exploded %1 entities → %2 segments: %3
已分解 %1 个图元 → 新增 %2 段：%3	Exploded %1 entities → %2 new segments: %3
已分解 %1 个块引用 → %2 个图元	Exploded %1 block references → %2 entities
已分解块 %1 为 %2 个独立图元	Exploded block %1 into %2 independent entities
已切换为%1	Switched to %1
已切换到 	Switched to 
已切换到工作空间：%1	Switched to workspace: %1
已切换到模型空间	Switched to model space
已切换折线 #%1 第 %2 到 %3 段的类型	Changed the type of segments %2 to %3 in polyline #%1
已切换折线 #%1 第 %2..%3 段类型	Changed the type of segments %2..%3 in polyline #%1
已创建	Created
已创建 %1 条平行线（间隔 %2 mm，参考线 #%3）	Created %1 parallel lines (spacing %2 mm, reference line #%3)
已创建%1 #%2	Created %1 #%2
已创建%1 #%2 = %3	Created %1 #%2 = %3
已创建%1 #%2 = %3 mm（%4）	Created %1 #%2 = %3 mm (%4)
已创建%1 #%2 = %3%4	Created %1 #%2 = %3%4
已创建%1 #%2（%3 个定义点）（%4）	Created %1 #%2 (%3 definition points) (%4)
已创建%1 #%2（%3 个控制点）	Created %1 #%2 (%3 control points)
已创建%1 #%2（%3 个顶点）（%4）	Created %1 #%2 (%3 vertices) (%4)
已创建%1 #%2：%3	Created %1 #%2: %3
已创建三点圆 #%1  圆心(%2, %3) R=%4	Created 3-point circle #%1  center(%2, %3) R=%4
已创建关联填充 #%1（来源 %2 个边界图元，随边界自动更新）	Created associative hatch #%1 (from %2 boundary entities, updates automatically with the boundary)
已创建剖面区域(填充) #%1，关联源 %2 个（修改边界自动随动）	Created section region (hatch) #%1, associated with %2 sources (follows boundary changes automatically)
已创建圆 #	Created circle #
已创建圆 #%1  r=%2 mm（圆心+半径）	Created circle #%1  r=%2 mm (Center + Radius)
已创建圆 #%1  圆心(%2, %3) r=%4 mm（%5）	Created circle #%1  center(%2, %3) r=%4 mm (%5)
已创建圆 #%1  圆心(%2, %3) r=%4 mm（三点定圆）	Created circle #%1  center(%2, %3) r=%4 mm (3-point circle)
已创建圆弧 #	Created arc #
已创建圆弧 #%1  r=%2 mm  起始 %3°  扫掠 %4°	Created arc #%1  r=%2 mm  start %3°  sweep %4°
已创建圆弧 #%1  圆心(%2, %3) r=%4 mm  起始 %5°  扫掠 %6°（%7）	Created arc #%1  center(%2, %3) r=%4 mm  start %5°  sweep %6° (%7)
已创建块 %1：块内 %2 个图元%3	Created block %1: %2 entities in the block%3
已创建块定义 %1（块内 %2 个图元 / 属性 %3 / 动态参数 %4）	Created block definition %1 (%2 entities in the block / attributes %3 / dynamic parameters %4)
已创建多行文字 #%1（%2 行，样式 %3）	Created multiline text #%1 (%2 lines, style %3)
已创建形位公差框格 #%1	Created geometric tolerance frame #%1
已创建折线 #%1（%2 节点，%3）	Created polyline #%1 (%2 nodes, %3)
已创建折线 #%1（节点 %2，%3）	Created polyline #%1 (node %2, %3)
已创建构造线 #%1  %2 通过(%3, %4) 角度 %5°	Created construction line #%1  %2 through(%3, %4) angle %5 deg
已创建标注样式 %1	Created dimension style %1
已创建椭圆 #%1  %2（%3）	Created ellipse #%1  %2 (%3)
已创建椭圆 #%1  中心(%2, %3)  %4（%5）	Created ellipse #%1  center(%2, %3)  %4 (%5)
已创建直线 #	Created line #
已创建直线 #%1  (%2, %3) → (%4, %5)	Created line #%1  (%2, %3) -> (%4, %5)
已创建直线 #%1  长度 %2 mm	Created line #%1  length %2 mm
已创建直线 #%1 (%2,%3)->(%4,%5)	Created line #%1 (%2,%3)->(%4,%5)
已创建矩形 #	Created rectangle #
已创建等距折线 #%1 (d=%2)	Created equidistant polyline #%1 (d=%2)
已创建等距折线 #%1（d=%2 mm）	Created equidistant polyline #%1 (d=%2 mm)
已创建编组「%1」（%2 个对象）	Created group '%1' (%2 objects)
已创建自由手绘线 #%1（%2 个采样点）	Created freehand line #%1 (%2 sample points)
已创建表格 #%1（%2×%3）	Created table #%1 (%2x%3)
已创建边界 #%1（外环 %2 节点，内孔 %3）	Created boundary #%1 (outer loop %2 nodes, inner holes %3)
已删除 %1 个选中图元	Deleted %1 selected entities
已删除 %1 个重复图元	Deleted %1 duplicate entities
已删除区域「%1」的 %2 个边界	Region '%1': deleted %2 boundaries
已删除图元 #	Deleted entity #
已删除图层：%1（其图元回落到层 0）	Deleted layer: %1 (its entities fell back to layer 0)
已删除块 %1	Deleted block %1
已删除块 %1；%2 个引用因块定义缺失而暂不绘制（可撤销恢复）	Deleted block %1; %2 references temporarily not drawn because the block definition is missing (undo to restore)
已删除工作空间：%1	Deleted workspace: %1
已删除工具条：%1	Deleted toolbar: %1
已删除折线 #%1 的节点 %2	Deleted polyline #%1, node %2
已删除折线 #%1 节点 %2	Deleted polyline #%1 node %2
已删除折线 #%1 节点 %2 到 %3 之间的部分	Deleted the portion of polyline #%1 between nodes %2 and %3
已删除折线 #%1 节点 %2..%3 之间	Deleted polyline #%1 between nodes %2..%3
已删除文字样式：%1（引用该样式的文字回退默认）	Deleted text style: %1 (text referencing it falls back to default)
已删除标注样式 %1；%2 个标注回落当前样式	Deleted dimension style %1; %2 dimensions fell back to the current style
已删除样条 #%1 控制点 %2 到 %3 之间的部分	Deleted the portion of spline #%1 between control points %2 and %3
已删除样条 #%1 的控制点 %2	Deleted spline #%1 control point %2
已删除特性「%1」	Deleted property '%1'
已剪切%1 #%2 → 保留 %3 段：%4	Trimmed %1 #%2 -> kept %3 segments: %4
已剪切%1 #%2：整体位于边界之间，已移除	Trimmed %1 #%2: entirely between boundaries, removed
已加载别名文件：%1（%2 条）	Loaded alias file: %1 (%2 entries)
已加载插件	Loaded Plugins
已加载界面配置：%1（工具条 %2 / 工作空间 %3）	Loaded interface configuration: %1 (toolbars %2 / workspaces %3)
已卸载插件：%1	Unloaded plugin: %1
已反向 %1 个图元	Reversed %1 entities
已反选：%1 个图元	Inverted selection: %1 entities
已发布 %1 页 → %2	Published %1 pages -> %2
已发送至打印机	Sent to printer
已取消 %1 个与种子图元相交的图元	Deselected %1 entities intersecting the seed entity
已取消图层 %1 上的 %2 个图元	Layer %1: deselected %2 entities
已取消夹点编辑	Cancelled grip edit
已取消当前操作	Cancelled current operation
已取消打开：	Cancelled open:
已取消标注	Dimension cancelled
已取消设置线宽	Cancelled lineweight setting
已取消轮廓：%1 个相连图元	Deselected contour: %1 connected entities
已取消选择图元 #%1	Deselected entity #%1
已另存为模板：%1（id=%2，参考尺寸 %3×%4 mm）	Saved as template: %1 (id=%2, reference size %3x%4 mm)
已合并 %1 段为 #%2	Merged %1 segments into #%2
已合并 %1 段为折线 #%2	Merged %1 segments into polyline #%2
已合并 %1 段为折线 #%2（节点 %3）	Merged %1 segments into polyline #%2 (node %3)
已合并到折线	Merged into polyline
已向工具条 %1 添加按钮：%2	Added button to toolbar %1: %2
已启动修剪（Trim Two）	Started trim (Trim Two)
已回退一个参考图元	Stepped back one reference entity
已回退一个拾取点	Stepped back one pick point
已在 #%1 中点处开出 %2 mm 缺口	At the midpoint of #%1, cut a %2 mm gap
已在位编辑块 %1：%2 个引用同步更新，块内 %3 个图元	Edited block %1 in place: %2 references updated, %3 entities in block
已在布局「%1」新建 %2：paper(%3, %4, %5×%6 mm)%7	In layout '%1', created %2: paper(%3, %4, %5x%6 mm)%7
已在折线 #%1 的%2端追加节点	Appended a node to polyline #%1 at the %2 end
已在折线 #%1 第 %2 段插入节点	Inserted a node into polyline #%1, segment %2
已在样条 #%1 %2追加控制点	Appended a control point to spline #%1 at the %2 end
已在样条 #%1 第 %2 段插入控制点	Inserted a control point into spline #%1, segment %2
已填充 %1 个区域（跨 %2 个选中的闭合图元）	Hatched %1 regions (across %2 selected closed entities)
已复制 %1 个图元 → %2	Copied %1 entities -> %2
已复制 %1 个图元到剪贴板（基点 %2,%3%4）	Copied %1 entities to clipboard (base point %2,%3%4)
已复制 %1 个图元到剪贴板（基点 %2,%3）	Copied %1 entities to clipboard (base point %2,%3)
已复制 %1 个图元（偏置=(%2,%3)）→ %4	Copied %1 entities (offset=(%2,%3)) -> %4
已复制: 	Copied: 
已复制移动 %1 个图元（Δ=(%2,%3)），新增 %4 个	Copy-moved %1 entities (delta=(%2,%3)), %4 added
已套用模板并按 %1 × %2 mm 缩放（比例因子 %3）	Applied template and scaled to %1 x %2 mm (scale factor %3)
已完成「从段生成折线」	Completed 'Polyline from Segments'
已完成「合并」	Completed 'Merge'
已对 #%1 与 #%2 做%3（r=%4），新增 %5：%6	On #%1 and #%2, applied %3 (r=%4), added %5: %6
已对 #%1 与 #%2 做倒角（d1=%3, d2=%4），新增 %5：%6	Chamfered #%1 and #%2 (d1=%3, d2=%4), added %5: %6
已对 #%1 与 #%2 做倒角（默认 d=%3 mm），新增 %4：%5	Chamfered #%1 and #%2 (default d=%3 mm), added %4: %5
已对 #%1 与 #%2 做圆角（r=%3 mm），新增 %4：%5	Filleted #%1 and #%2 (r=%3 mm), added %4: %5
已对 %1 个图元执行移动并旋转	Moved and rotated %1 entities
已对齐 %1 个图元（方式=%2）	Aligned %1 entities (method=%2)
已导出 DXF（%1）：%2（%3 字节）	Exported DXF (%1): %2 (%3 bytes)
已导出 DXF：	Exported DXF:
已导出 PDF：%1	Exported PDF: %1
已将 #%1 转换为样条 #%2（%3 个控制点）	Converted #%1 to spline #%2 (%3 control points)
已将 %1 个图元改属图层 %2	Reassigned %1 entities to layer %2
已将 %1 个对象加入编组「%2」	Added %1 objects to group '%2'
已将 %1 个对象移出编组	Moved %1 objects out of the group
已将 %1 个标注改用样式 %2（随该样式联动）	Changed %1 dimensions to style %2 (linked to that style)
已将 %1 个选中图元移入图层 %2	Moved %1 selected entities into layer %2
已将参数 %1 设为 %2 并联动驱动全部关联标注/几何	Set parameter %1 to %2 and updated all linked dimensions/geometry
已将图层 %1 的样式套用到 %2 个图元	Applied the style of layer %1 to %2 entities
已将折线 #%1 的 %2 段弧转为直线	Polyline #%1: converted %2 arc segments to lines
已将折线 #%1 的 %2 段弧转直线	Polyline #%1: converted %2 arc segments to lines
已将文字样式「%1」应用到 %2 个文字对象	Applied text style '%1' to %2 text objects
已将标注 #%1 设为 %2，几何已联动重构	Set dimension #%1 to %2; geometry rebuilt accordingly
已将样式「%1」应用到 %2 个选中文字对象。	Applied style '%1' to %2 selected text objects.
已将样式「%1」应用到全部 %2 个文字对象。	Applied style '%1' to all %2 text objects.
已将样条 #%1 分解为 %2 条线段	Exploded spline #%1 into %2 line segments
已将样条 #%1 分解为 %2 条线段（#%3 ~ #%4）	Exploded spline #%1 into %2 line segments (#%3 ~ #%4)
已平移 %1 个图元至 (%2, %3)	Moved %1 entities to (%2, %3)
已延长 %1 个图元（side=%2, Δ=%3 mm）	Extended %1 entities (side=%2, delta=%3 mm)
已延长 %1 个图元（side=%2，Δ=%3 mm）	Extended %1 entities (side=%2, delta=%3 mm)
已建立约束 #%1：#%2 ↔ #%3（可在属性面板改类型；求解器为后续规划项）	Created constraint #%1: #%2 <-> #%3 (change type in the property panel; solver planned for a future release)
已录制 %1 条命令：	Recorded %1 commands:
已恢复全部面板	Restored all panels
已恢复自动保存的内容：\n%1	Restored autosaved content:\n%1
已打开：%1（比例 %2）	Opened: %1 (scale %2)
已打断图元 #%1 于 (%2,%3)	Broke entity #%1 at (%2,%3)
已把 #%1 等分为 %2 段	Divided #%1 into %2 equal segments
已把 %1 个剖面区域改为：%2（角度 %3°、间距 %4mm）	Changed %1 section regions to: %2 (angle %3 deg, spacing %4mm)
已把 %1 个文本图元分解为 %2 个字	Exploded %1 text entities into %2 characters
已把 %1 个边界归入区域「%2」	Assigned %1 boundaries to region '%2'
已把图元 #%1 等分为 %2 段	Divided entity #%1 into %2 equal segments
已把当前样式（%1 / %2 / %3）套用到 %4 个选中图元	Applied current style (%1 / %2 / %3) to %4 selected element(s)
已拉伸 %1 个端点	Stretched %1 endpoints
已拾取定位点 %1: (%2, %3)	Picked positioning point %1: (%2, %3)
已拾取点 %1/%2: (%3, %4)	Picked point %1/%2: (%3, %4)
已拾取点 %1: (%2, %3)	Picked point %1: (%2, %3)
已按 %1 生成图框与标题栏（内容%2被包含，标题栏自动避让）	Generated drawing frame and title block per %1 (content %2 included, title block avoids content automatically)
已按 %1（%2×%3 mm，%4）重新生成图框（保留其他图层内容）	Regenerated drawing frame for %1 (%2x%3 mm, %4); other layers preserved
已按「无标题栏」样式移除标题栏（%1 条图元）	Removed title block per the 'No Title Block' style (%1 entities)
已按布局导出 PDF（%1 页）：%2	Exported PDF by layout (%1 pages): %2
已按线型「%1」累加选择 %2 个图元（含原有选区）	Linetype '%1': added %2 entities to selection (including existing selection)
已按颜色累加选择，当前共 %1 个图元被选中	Added to selection by color; %1 entities selected in total
已插入块 #%1	Inserted block #%1
已插入块 %1 @(%2,%3) 旋转 %4° 缩放 %5×%6	Inserted block %1 @(%2,%3) rotated %4 deg scaled %5x%6
已插入标准标题栏（图名：%1，比例：%2）	Inserted standard title block (drawing title: %1, scale: %2)
已撤销: 	Undid: 
已放弃上一点	Discarded the previous point
已放置临时追踪点 (TK)：(%1, %2)	Placed temporary tracking point (TK): (%1, %2)
已放置临时追踪点 (TK)：(%1, %2)，共 %3 个	Placed temporary tracking point (TK): (%1, %2), %3 total
已放置形位公差框格 #%1：%2	Placed geometric tolerance frame #%1: %2
已放置焊缝符号：%1	Placed weld symbol: %1
已放置粗糙度符号 %1	Placed surface roughness symbol %1
已新建 %1 图纸（比例 %2）：已生成图框与标准标题栏；坐标原点已置于图纸中央	Created %1 drawing (scale %2): drawing frame and standard title block generated; origin placed at drawing center
已新建图层：%1	Created layer: %1
已新建工作空间：%1（%2 个工具条可见）	Created workspace: %1 (%2 toolbars visible)
已新建工具条：%1	Created toolbar: %1
已新建文字样式：%1	Created text style: %1
已新建空白图纸（A4 横向 1:1，无图框/标题栏；需要图框请用【图纸】>【新建图框】）	Created blank drawing (A4 landscape 1:1, no drawing frame/title block; to add a frame use Drawing > New Drawing Frame)
已新建自定义工具条：%1（%2 个按钮）	Created custom toolbar: %1 (%2 buttons)
已旋转 %1 个图元 %2°	Rotated %1 entities by %2 deg
已旋转 %1 个图元（中心=(%2,%3)，角度=%4°）	Rotated %1 entities (center=(%2,%3), angle=%4 deg)
已旋转 2 个图元 %1°	Rotated 2 entities by %1 deg
已显示	Shown
已智能创建%1 #%2 = %3（%4）	Smart-created %1 #%2 = %3 (%4)
已更新 %1 个域宿主（MText/Table）	Updated %1 field hosts (MText/Table)
已更新文字 #%1（样式 %2）	Updated text #%1 (style %2)
已更新文字样式：%1	Updated text style: %1
已更新标注文字 #%1	Updated dimension text #%1
已更新标注样式 %1（引用它的标注已联动）	Updated dimension style %1 (referencing dimensions updated)
已更新表格 #%1（%2×%3）	Updated table #%1 (%2x%3)
已有块：	Existing blocks:
已沿 #%1 放置 %2 个测量点（间距 %3）	Along #%1, placed %2 measure points (spacing %3)
已添加「%1」约束：主动#%2 → 被动#%3%4%5%6，并已求解	Added '%1' constraint: driving #%2 -> driven #%3%4%5%6, solved
已清空全部显式几何约束	Cleared all explicit geometric constraints
已清空草图	Cleared sketch
已清除 FILTER 预选器	Cleared FILTER preselector
已清除临时捕捉覆盖，恢复原有捕捉配置	Temporary snap override cleared; previous snap settings restored
已清除全部临时追踪点 (TK)	All temporary tracking points (TK) cleared
已生成 %1 个镜像图元（%2）	Generated %1 mirrored entities (%2)
已生成 %1 图框（比例 %2）：装订边%3、对中符号%4、分区%5、明细栏%6、附加栏%7、代号栏%8、参数栏%9	Generated %1 drawing frame (scale %2): binding margin %3, centering marks %4, zoning %5, BOM table %6, additional fields %7, code fields %8, parameter fields %9
已生成 PDF：%1	Generated PDF: %1
已生成户型平面图：%1 个图元，比例 %2（墙体/门窗/家具/标注/图签）	Generated floor plan: %1 entities, scale %2 (walls/doors and windows/furniture/dimensions/title block)
已生成法兰盘三面图：%1 个图元，比例 %2	Generated flange three-view drawing: %1 entities, scale %2
已生成法兰盘轮毂视图：%1 个图元，比例 %2（主视图全剖 A—A + 左视图）	Generated flange hub views: %1 entities, scale %2 (front view full section A-A + left view)
已生成焊缝符号：%1（%2 个图元，于 %3,%4）	Generated weld symbol: %1 (%2 entities at %3,%4)
已生成箱体视图：%1 个图元，比例 %2（主视图全剖 + A—A 断面）	Generated housing views: %1 entities, scale %2 (front view full section + A-A section)
已生成轴承座三面图：%1 个图元，比例 %2（左视图全剖）	Generated bearing housing three-view drawing: %1 entities, scale %2 (left view full section)
已生成飞机三面图：%1 个图元	Generated aircraft three-view drawing: %1 entities
已生成飞机三面图：%1 个图元（侧面图/俯视图/正面图）	Generated aircraft three-view drawing: %1 entities (side view/top view/front view)
已用系统编辑器打开别名文件：%1（编辑后点「重新加载命令别名」生效）	Opened alias file in system editor: %1 (click 'Reload Command Aliases' after editing to apply)
已移动 %1 个图元（Δ=(%2,%3)）	Moved %1 entities (delta=(%2,%3))
已粘贴: 	Pasted: 
已结束 Snake 折线，共 %1 段	Finished Snake polyline, %1 segments total
已结束当前绘制	Ended current drawing
已结束连续直线绘制	Continuous line drawing finished
已缩放 %1 个图元（中心=(%2,%3)，因子=%4）	Scaled %1 entities (center=(%2,%3), factor=%4)
已缩放 %1 个图元（因子=%2）	Scaled %1 entities (factor=%2)
已自动保存：%1	Autosaved: %1
已自动创建并切换到布局「布局1」	Automatically created and switched to layout 'Layout1'
已自动刷新 %1 个域宿主（fieldupdate 可手动重算）	Automatically refreshed %1 field hosts (use fieldupdate to recalculate manually)
已解冻	Thawed
已解冻 %1 个图层	Thawed %1 layers
已解散编组「%1」（%2 个对象保留）	Ungrouped '%1' (%2 objects kept)
已解锁	Unlocked
已设置别名：%1 → %2	Set alias: %1 -> %2
已设置多段线 #%1 线宽 = %2px	Set polyline #%1 lineweight = %2px
已设置特性 %1 = %2（MText/Table 中以 %<PROP:%1>% 引用）	Set property %1 = %2 (reference via %<PROP:%1>% in MText/Table)
已跳过 %1 个外部参照（Xref 不参与本图编组）	Skipped %1 external references (Xref not included in this drawing's grouping)
已载入 %1 条默认别名	Loaded %1 default aliases
已连合为 %1 条折线（容差 %2 mm）：%3	Joined into %1 polylines (tolerance %2 mm): %3
已连接两线段	Connected two line segments
已连接（%1）	Connected (%1)
已追加折线 #%1 节点	Appended node to polyline #%1
已退出绘制工具，回到选择状态	Exited drawing tool, returned to selection mode
已选中 %1 个对象：%2	Selected %1 object(s): %2
已选中区域「%1」的 %2 个边界	Region '%1': selected %2 boundaries
已选圆 #%1，移动鼠标预览方向，点击确认（右键切换直径/半径）	Selected circle #%1, move mouse to preview direction, click to confirm (right-click to toggle diameter/radius)
已选多边形 #%1 第 %2 条边，移动鼠标预览标注，点击确认	Selected polygon #%1 edge %2, move mouse to preview dimension, click to confirm
已选择 %1 个与种子图元相交的图元	Selected %1 entities intersecting the seed entity
已选择参考图元 #%1	Selected reference entity #%1
已选择参考线 #%1	Selected reference line #%1
已选择图层 %1 上的 %2 个图元	Layer %1: selected %2 entities
已选择折线 #%1 节点 %2	Selected polyline #%1 node %2
已选择样条 #%1 控制点 %2	Selected spline #%1 control point %2
已选择轮廓：%1 个相连图元	Selected contour: %1 connected entities
已选目标折线 #%1，点击相邻直线/圆弧/折线（右键结束）	Selected target polyline #%1, click adjacent line/arc/polyline (right-click to finish)
已选直线 #%1，移动鼠标预览标注，点击确认	Selected line #%1, move mouse to preview dimension, click to confirm
已选第 1 段（#%1），点击下一段或右键结束	Selected segment 1 (#%1), click the next segment or right-click to finish
已重做: 	Redid: 
已重命名文字样式：%1 → %2	Renamed text style: %1 -> %2
已重命名编组：%1 → %2（%3 个对象）	Renamed group: %1 -> %2 (%3 objects)
已重定义块 %1，%2 个引用同步更新	Redefined block %1, %2 references updated
已重定义块 %1：%2 个引用同步更新	Redefined block %1: %2 references updated
已重新加载别名文件：%1（%2 条）	Reloaded alias file: %1 (%2 entries)
已重新加载插件目录，当前加载 %1 个插件	Reloaded plugin directory, %1 plugins currently loaded
已重生成（REGEN）	Regenerated (REGEN)
已锁定	Locked
已闭合多段线	Polyline closed
已附加外部参照 %1 ← %2（%3，%4 个图元%5），插入点 (%6,%7) 旋转 %8° 缩放 %9×%10	Attached external reference %1 <- %2 (%3, %4 entities%5), insertion point (%6,%7) rotated %8 deg scaled %9x%10
已隐藏	Hidden
已隔离 %1 个图层（冻结其余 %2 个）	Isolated %1 layers (froze the other %2)
布局	Layout
布局「%1」不存在	Layout \"%1\" does not exist
布局「%1」：%2 个视口%3	Layout '%1': %2 viewports%3
布局的增删改/切换/视口比例/冻结（单一分派命令）	Layout add/delete/modify/switch/viewport scale/freeze (single dispatch command)
布置方式:	Arrangement:
带基点复制	Copy with Base Point
带基点复制(&B)...	Copy with Base Point(&B)...
带基点复制：请单击指定基点	Copy with Base Point: click to specify the base point
帮助	Help
帮助(&H)	&Help
常用色	Common Colors
常规	General
平移图元	Move Entities
平移选中图元	Move Selected Entities
平行=Parallel	Parallel =Parallel
平行构造线	Parallel Construction Line
平行构造线：先选参考直线，再点击决定平行线位置	Parallel Construction Line: first select a reference line, then click to set the parallel line position
平行线	Parallel Line
平行线：先选参考线，再点击决定偏移方向（间隔/条数见左）	Parallel Line: first select a reference line, then click to set the offset direction (interval/count on the left)
平面	Plane
平面:	Plane:
平面:XY	Plane:XY
序号	No.
序号球标	Item Balloon
应用	Apply
应用到全部文字	Apply to All Text
应用到选中文字	Apply to Selected Text
应用图层样式 	Apply Layer Style 
应用文字样式 	Apply Text Style 
应用方式：	Apply Method:
应用标注样式 	Apply Dimension Style 
应用结果	Apply Result
应用范围：	Apply To:
应用过滤器	Apply Filter
底板厚	Base Plate Thickness
底板厚度	Base Plate Thickness
底板宽	Base Plate Width
底板宽度	Base Plate Width
底板深	Base Plate Depth
底板长度	Base Plate Length
底部外廓宽度	Bottom Outer Width
延伸	Extend
延伸 (Extend)：目标直线 + 边界直线	Extend: target line + boundary line
延长...	Lengthen...
建视口	Create Viewport
开	On
开启	Enable
开启后仅记录匿名会话/崩溃计数（无任何图纸内容）。\n默认关闭；当前版本不含任何网络上报实现，开启也不会产生网络请求。	When enabled, only anonymous session/crash counts are recorded (no drawing content).\nOff by default; this version contains no network reporting implementation, and enabling it will not generate any network requests.
开启越界检查（新增图元出界时告警，不拦截）	Enable out-of-bounds check (warn when new entities exceed limits, no blocking)
开启：每步仅存「变更集 + 每 50 步一个全量检查点」，大幅降低撤销内存。\n关闭：每步存整张图纸快照（用于性能/行为对照，内存占用更高）。\n切换会清空当前撤销历史。	On: each step stores only a 'change set + a full checkpoint every 50 steps', greatly reducing undo memory.\nOff: each step stores a full drawing snapshot (for performance/behavior comparison; higher memory usage).\nToggling clears the current undo history.
开启：画圆/直线等不会破坏现有图形的命令，助手直接执行。\n关闭：每条命令执行前都先展示，由你确认。\n破坏性命令（删除/修剪/分解…）无论此项如何，都需你手动确认。	On: for commands that do not modify existing geometry (e.g. circle/line), the assistant executes them directly.\nOff: every command is shown for your confirmation before execution.\nDestructive commands (delete/trim/explode...) always require manual confirmation regardless of this setting.
开始录制命令宏	Start Recording Command Macro
开放	Open
开（右键结束）	On (right-click to finish)
引擎不可用	Engine unavailable
引线标注	Leader Dimension
引线标注=Guide line annotation	Leader =Guide line annotation
引线标注：连续点击折线点，末点为文字落点，用于文字/符号说明	Leader Dimension: click polyline points in sequence; the last point places the text; for text/symbol notes
弧	Arc
弧长标注	Arc Length Dimension
弧长标注=Arc length annotation	Arc length =Arc length annotation
弧长标注：点选圆弧，尺寸线为同心弧，数字带 ⌒ 前缀	Arc Length Dimension: pick an arc; the dimension line is a concentric arc; the value carries an arc prefix
当前 Qt 构建未包含该数据库驱动（%1）。\n请改用内置的 SQLite，或将对应驱动插件放入程序目录的 sqldrivers/ 子目录后重启。	This Qt build does not include the database driver (%1).\nUse the built-in SQLite instead, or place the driver plugin in the sqldrivers/ subdirectory of the program directory and restart.
当前 Qt 构建未包含该数据库驱动（%1），无法保存并连接。\n请改用内置的 SQLite，或将对应驱动插件放入程序目录的 sqldrivers/ 子目录后重启。	This Qt build does not include the database driver (%1); cannot save and connect.\nUse the built-in SQLite instead, or place the driver plugin in the sqldrivers/ subdirectory of the program directory and restart.
当前 UCS: 	Current UCS: 
当前值	Current Value
当前全局线型比例 LTSCALE = %1	Current global linetype scale LTSCALE = %1
当前命令不支持闭合选项	Current command does not support the Close option
当前命令无选项 %1	Current command has no option %1
当前图块表	Current Block Table
当前图纸无自定义特性（用法: prop <名称> <值> 设置，%<PROP:名称>% 引用）	Current drawing has no custom properties (Usage: prop <name> <value> to set, %<PROP:name>% to reference)
当前图纸未检测到标准标题栏。\n请先通过「文件 - 新建」生成含标题栏的图纸。	No standard title block detected in the current drawing.\nUse 'File - New' to generate a drawing with a title block first.
当前图纸没有块引用	Current drawing has no block references
当前图纸没有外部参照	Current drawing has no external references
当前图纸没有已附加的外部参照。	Current drawing has no attached external references.
当前图还没有块定义，请先「创建块」或「插入块」。	Current drawing has no block definitions yet; use 'Create Block' or 'Insert Block' first.
当前处于模型空间（先 layout set <布局名>）	Currently in model space (use layout set <layout> first)
当前布局中无视口「%1」	No viewport '%1' in current layout
当前无 FILTER 预选器	No FILTER preselector
当前无临时追踪点	No temporary tracking points
当前无显式几何约束	No explicit geometric constraints
当前标注样式 = %1	Current dimension style = %1
当前没有别名	No aliases
当前没有可用的块，请先创建块。	No blocks available; create a block first.
当前没有块定义	No block definitions
当前没有工作空间	No workspaces
当前没有编组	No groups
当前没有进行中的操作	No operation in progress
当前没有选中对象。	No object selected.
当前没有选中的文字对象，请先在绘图区选中文字。	No text objects selected; select text in the drawing area first.
当前状态：	Current Status:
当前用户坐标系（UCS）。命令 ucs world|3p|object|z|save|set 切换	Current user coordinate system (UCS). Command ucs world|3p|object|z|save|set to switch
当前符号集未收录「%1」，已禁用放置；可用命令 tol 以文本创建框格。	The current symbol set does not include '%1'; placement disabled; use the tol command to create the frame as text.
当前线型 = %1	Current linetype = %1
当前线宽: %1（用法: width 1|2|3|5）	Current lineweight: %1 (Usage: width 1|2|3|5)
当前选择集内	Within current selection
当前预选器：%1（作用于下一次拾取/框选/栏选）	Current preselector: %1 (applies to the next pick/window/fence selection)
当前颜色: %1（用法: color #RRGGBB | red | blue ...）	Current color: %1 (Usage: color #RRGGBB | red | blue ...)
当前：%1，共 %2 个布局（用法：layout new/set/list/rename/delete/copy/vpscale）	Current: %1, %2 layouts total (Usage: layout new/set/list/rename/delete/copy/vpscale)
录制宏	Record Macro
形位公差	Geometric Tolerance
形位公差(&G)...	Geometric Tolerance(&G)...
形位公差创建失败	Failed to create geometric tolerance
形位公差框格=Fcf	Feature control frame =Fcf
微信号 <b>%1</b> 已复制到剪贴板。<br>请打开微信，通过「添加朋友」搜索该微信号后联系赞赏。	WeChat ID <b>%1</b> copied to clipboard.<br>Open WeChat, search this ID via 'Add Friends' to get in touch or send a reward.
必须与「API 密钥」所属厂商一致，否则会返回 HTTP 401。\nDeepSeek：https://api.deepseek.com 或 https://api.deepseek.com/v1\nOpenAI：https://api.openai.com/v1\n本地 Ollama：http://localhost:11434/v1\n本程序会在此地址后自动追加 /chat/completions，请勿重复填写该路径。	Must match the vendor of the 'API Key', otherwise HTTP 401 is returned.\nDeepSeek: https://api.deepseek.com or https://api.deepseek.com/v1\nOpenAI: https://api.openai.com/v1\nLocal Ollama: http://localhost:11434/v1\nThis program automatically appends /chat/completions to this address; do not enter that path again.
快捷键冲突：%1 同时绑定于 %2（后者将不生效或抢占，请检查菜单定义）	Shortcut conflict: %1 is also bound to %2 (the latter will not take effect or will take precedence; check menu definitions)
快捷键：%1	Shortcut: %1
快照保存在 %LOCALAPPDATA%/SketchCAD/autosave，异常退出后可从启动恢复对话框找回。	Snapshots are saved in %LOCALAPPDATA%/SketchCAD/autosave; after an abnormal exit, recover them from the startup recovery dialog.
快速标注	Quick Dimension
快速选择	Quick Select
快速选择 (QSELECT)	Quick Select (QSELECT)
快速选择(&Q)...	Quick Select(&Q)...
快速选择(QSELECT)：按对象类型/图层/颜色/线型筛选并选中匹配图元	Quick Select (QSELECT): filter by object type/layer/color/linetype and select matching entities
快速选择：命中 %1 个对象（%2 选择集）	Quick Select: %1 objects matched (%2 selection set)
快速选择：命中 %1 个对象（已替换选择集）	Quick Select: %1 objects matched (selection set replaced)
快速选择：未指定任何过滤条件，未改变选择集。用法: qselect type=直线 [layer=0] [color=#ff0000] [linestyle=实线]	Quick Select: no filter conditions specified; selection set unchanged. Usage: qselect type=line [layer=0] [color=#ff0000] [linestyle=Continuous]
快速选择：未知关键字 '%1'（可用 type/layer/color/linestyle）	Quick Select: unknown keyword '%1' (available: type/layer/color/linestyle)
总体：%1	Overall: %1
总开间（宽）	Total Span (Width)
总进深（高）	Total Depth (Height)
总高	Total Height
恢复为	Restore To
恢复图纸...	Restore Drawing...
恢复失败	Restore Failed
恢复失败：%1	Restore Failed: %1
恢复完成	Restore Complete
恢复所选	Restore Selected
恢复被撤销的编辑	Restore Undone Edit
恢复被隔离图层的显示	Restore display of isolated layers
恢复默认	Restore Defaults
成功	Success
我是慎独 — 继续	I'm Shendu - Continue
我是慎独 草图模块 v%1 — C++17 / Qt6 实现	I'm Shendu - Sketch Module v%1 - C++17 / Qt6 implementation
我是慎独 草图模块已启动 (C++17 / Qt6)	Sketch module started (C++17 / Qt6)
截面特性	Section Properties
截面特性：A=%1 mm², C=(%2, %3), Ix=%4, Iy=%5, Ixy=%6	Section Properties: A=%1 mm2, C=(%2, %3), Ix=%4, Iy=%5, Ixy=%6
截面特性：请恰好选中一个闭合轮廓	Section Properties: select exactly one closed contour
截面特性：选中图元不是闭合轮廓	Section Properties: selected entity is not a closed contour
户型平面图...	Floor Plan...
房门宽	Door Width
房间名字高	Room Name Text Height
所有尺寸单位：毫米（mm）。参数收敛后生成带颈平焊法兰三面图。	All dimension units: millimeters (mm). A hubbed slip-on flange three-view drawing is generated after parameters converge.
所有尺寸单位：米（m）。修改后点击“确定”即按新尺寸生成三面图。	All dimension units: meters (m). After modifying, click 'OK' to regenerate the three views with the new dimensions.
所选段节点不足	Not enough nodes in selected segment
手动比例 1:	Manual Scale 1:
手动输入坐标 (X, Y)	Enter Coordinates Manually (X, Y)
打印	Plot
打印(&P)	&Print
打印...	Plot...
打印失败	Plot Failed
打印样式 (简化 CTB)	Plot Style (Simplified CTB)
打印比例	Plot Scale
打印线宽比例	Plot Lineweight Scale
打印设置	Plot Settings
打印设置...	Plot Settings...
打印预览	Plot Preview
打印预览 (Print)：页面设置 / CTB 样式 / 视口 / PDF	Plot Preview (Print): page setup / CTB style / viewport / PDF
打印预览...	Plot Preview...
打开	Open
打开...	&Open...
打开/保存对话框将优先定位到默认存盘目录；\n最近使用过的目录会自动记忆（不属于图形内容，不随图存档）。	Open/Save dialogs default to the default save directory;\nRecently used directories are remembered automatically (not part of drawing content, not saved with the drawing).
打开其他...	Open Other...
打开图纸	Open Drawing
打开失败	Open Failed
打开失败：	Open Failed:
打开失败：不是有效的草图文件	Open Failed: not a valid sketch file
打开日志目录	Open Log Directory
打开最近	Open Recent
打开校验：%1 个图元引用了不存在的图层/块/样式或悬空几何：%2	Open validation: %1 entities reference missing layers/blocks/styles or dangling geometry: %2
打开校验：%1 个损坏图元已跳过，%2 个已修复。%3	Open validation: %1 corrupted entities skipped, %2 repaired. %3
打开测量查询面板...	Open Measure/Inquiry panel...
打开特性面板编辑选中图元	Open Properties panel to edit selected entities
打开草图	Open Sketch
打断	Break
打断 / 等分...	Break / Divide...
打断 / 等分需要先选中 1 条直线	Break / Divide requires selecting 1 line first
打断失败（点不在线段内或为端点）	Break failed (point is not on the segment or is an endpoint)
打断点 X	Break point X
打断点 Y	Break point Y
打赏	Support
打赏 — 微信收款码	Tip — WeChat Payment QR Code
执行	Execute
执行一条 SketchCAD 命令（自然语言助手的主要动作入口）。参数 line 为完整命令串，由你根据用户输入理解语义、抽取参数后生成，语法与界面命令行完全一致，例如 \"circle 0,0 50\"、\"line 0,0 100,0\"、\"delete 7\"。自然语言参数抽取示例：「给我绘制一个圆心在0，0、半径为20的圆」→ \"circle 0,0 20\"。命令名与参数顺序必须严格遵守 system prompt 中的 usage；不确定时先用 getCommandHelp 查询。	Execute a SketchCAD command (the main action entry point of the natural-language assistant). The line parameter is a complete command string that you generate by understanding the semantics of the user's input and extracting the parameters; its syntax is exactly the same as the command line in the UI, e.g. \"circle 0,0 50\", \"line 0,0 100,0\", \"delete 7\". Example of natural-language parameter extraction: 'Draw a circle with center at 0,0 and radius 20' → \"circle 0,0 20\". Command names and parameter order must strictly follow the usage in the system prompt; if unsure, query getCommandHelp first.
执行信息	Execution Info
执行清理	Execute Cleanup
扩展工具(&R)	Extended Tools(&R)
批量 BOM 分析	Batch BOM Analysis
批量 BOM：%1 个文件 → %2 种零件（读错误 %3 个）	Batch BOM: %1 files → %2 part types (%3 read errors)
批量失败：	Batch failed:
批量套用到本层图元	Batch apply to entities on this layer
批量开始：共 %1 个任务	Batch started: %1 tasks
批量签名	Batch Sign
批量签名（标题栏）	Batch Sign (Title Block)
批量签名：已填写 %1 个字段	Batch sign: %1 fields filled in
批量签名：未匹配字段 %1	Batch sign: unmatched field %1
批量签名：未填写任何字段	Batch sign: no fields filled in
批量结束：总 %1 成功 %2 失败 %3	Batch complete: total %1, succeeded %2, failed %3
把剪贴板内容整包收为块定义并放置引用	Collect the entire clipboard content into a block definition and place a reference
把剪贴板内容粘贴为独立图元	Paste clipboard content as independent entities
把同层首尾相接的 Line/Arc 连合为 Polyline	Join end-to-end Line/Arc on the same layer into a Polyline
把图元/选择集导出为独立 DXF 文件	Export entities/selection set as a standalone DXF file
把图元等分为 n 段	Divide an entity into n equal segments
把块定义插入为块引用	Insert a block definition as a block reference
把块引用/复合图元分解为独立图元	Explode block references/compound entities into independent entities
把当前别名表写回 .pgp 文件	Write the current alias table back to the .pgp file
把当前图导出为 DXF 文件（可选版本）	Export the current drawing as a DXF file (version optional)
把当前选择集写入内部剪贴板（基点 = 包围盒左下角）	Write the current selection set to the internal clipboard (base point = lower-left corner of the bounding box)
把录制的宏保存为文件	Save the recorded macro to a file
把折线图元转换为样条	Convert polyline entities to splines
把指定 id 加入选择集	Add the specified id to the selection set
把指定 id 移出选择集	Remove the specified id from the selection set
把指定图层图元移出选择集	Remove entities on the specified layer from the selection set
把样条打散为折线/线段	Break a spline into polylines/line segments
把源图元的图层/颜色/线宽/线型复制到目标	Copy layer/color/lineweight/linetype from the source entity to the target
把点列当作闭合多边形 shoelace 计算	Compute the point list as a closed polygon using the shoelace formula
把相邻直线/圆弧/折线合并到折线（PEDIT Join）	Merge adjacent lines/arcs/polylines into a polyline (PEDIT Join)
把选中图元移到目标图层	Move selected entities to the target layer
投影圆	Projection Circle
投影法 (GB/T 14665-2012 第7章)	Projection Method (GB/T 14665-2012, Chapter 7)
投影轮廓	Projection Outline
投影长度(mm)：	Projection Length (mm):
抖动	Dither
折线	Polyline
折线 #%1 第 %2 段：当前 %3，单击第二段切换两段（右键取消）	Polyline #%1 segment %2: current %3, click the second segment to switch between the two segments (right-click to cancel)
折线 #%1 节点过少，已删除	Polyline #%1 has too few vertices, deleted
折线 (Polyline)：依次点击节点生成多段线，右键结束	Polyline: click vertices in sequence to create a polyline, right-click to finish
折线=Polyline	Polyline =Polyline
折线·从已有段	Polyline · From Existing Segments
折线·修剪段	Polyline · Trim Segment
折线·切换段类型	Polyline - Toggle Segment Type
折线·删除两点之间	Polyline · Delete Between Two Points
折线·删除节点	Polyline · Delete Vertex
折线·反转	Polyline · Reverse
折线·合并	Polyline · Join
折线·弧转弦	Polyline - Arc to Chord
折线·拟合	Polyline · Fit
折线·样条化	Polyline - Spline
折线·添加节点	Polyline · Add Vertex
折线·等距	Polyline · Equidistant
折线·设宽	Polyline · Set Width
折线·追加节点	Polyline · Append Vertex
折线·闭合/打开	Polyline · Close/Open
折线已反转	Polyline reversed
折线已打开	Polyline opened
折线已拟合（平滑通过所有顶点）	Polyline fitted (smooth through all vertices)
折线已样条化（顶点为控制点）	Polyline converted to a spline (vertices as control points)
折线已闭合	Polyline closed
折线的闭合/线宽/连合/拟合/样条化	Polyline close / width / join / fit / spline
折线线宽已设为 %1	Polyline width set to %1
折线绘制与编辑	Polyline drawing and editing
折线转样条	Polyline to Spline
折线顶点不足，无法拟合	Polyline has too few vertices to fit
折线顶点不足，无法样条化	Polyline has too few vertices to spline
折线（Polyline）：依次点击节点生成多段线，右键结束，点击首点附近可闭合	Polyline: click vertices in sequence to create a polyline, right-click to finish, click near the first point to close
抛物线	Parabola
抛物线(4点)	Parabola (4 Points)
抛物线(4点)：依次点击两个端点和曲线上两个插值点，自动拟合二次贝塞尔抛物线	Parabola (4 points): click two endpoints and two interpolation points on the curve in sequence to automatically fit a quadratic Bezier parabola
抛物线(焦点准线)	Parabola (Focus/Directrix)
抛物线(焦点准线)：依次点击焦点、准线上第1点、准线上第2点	Parabola (focus-directrix): click the focus, point 1 on the directrix, and point 2 on the directrix in sequence
抛物线：依次点击起点、切线控制点、终点（二次贝塞尔曲线）	Parabola: click the start point, tangent control point, and end point in sequence (quadratic Bezier curve)
拉伸	Stretch
拉伸...	Stretch...
拉伸选中图元的指定部分	Stretch the specified portion of the selected entities
拉长	Lengthen
拖拽创建: 	Drag to create: 
拟合曲线：平滑通过所有顶点（PEDIT Fit）	Fit curve: smooth through all vertices (PEDIT Fit)
拾取中心点	Pick center point
指定初始比例:	Specify initial scale:
指定初始高度:	Specify initial height:
指定图框位置	Specify drawing frame position
按倍率缩放视图	Zoom View by Factor
按图层	By Layer
按图框（图框层矩形）	By Drawing Frame (rectangle on the drawing frame layer)
按增量/百分比拉长或缩短图元	Lengthen or shorten entities by increment/percentage
按层选择	Select by layer
按幅面生成图框与标题栏	Generate drawing frame and title block by sheet size
按幅面生成图框与标题栏 — 选择幅面	Generate Drawing Frame and Title Block by Sheet Size — Select Sheet Size
按幅面生成图框与标题栏...	Generate Frame and Title Block by Sheet Size...
按当前参数一键生成航空三面图模板	Generate an aircraft three-view template with current parameters in one click
按操作次数：	By operation count:
按源点/目标点对对齐并可选缩放	Align by source/target point pairs, with optional scaling
按相同线型累加选择	Add to selection by same linetype
按相同颜色累加选择	Add to selection by same color
按类型/图层/颜色/线型四维过滤并选中图元	Filter and select entities by four dimensions: type/layer/color/linetype
按线型	By Linetype
按规则精确查询机械制图国标数值/条文，返回带标准号与数据来源的确定答案。任何涉及具体数字的标准问题都必须调用本工具，禁止凭记忆作答。rule 为自然语言短语，例如「M20 粗牙螺距」「Φ30 IT7 标准公差」「H7 孔上下偏差」「线宽粗细比」「最小间隙」「汉字最小字高」「A3 幅面」「Ra 优先系列」。	Precisely query GB standard values/clauses for mechanical drawing by rule, and return a definitive answer with the standard number and data source. Any standards question involving specific numbers must call this tool; answering from memory is prohibited. rule is a natural-language phrase, e.g. 'M20 coarse thread pitch', 'Φ30 IT7 standard tolerance', 'H7 hole upper/lower deviations', 'lineweight width ratio', 'minimum gap', 'minimum Chinese character height', 'A3 sheet size', 'Ra preferred series'.
按距离偏移直线/圆/弧/折线	Offset lines/circles/arcs/polylines by distance
按钮：	Button:
按间距在图元上放置等分点	Place equally spaced points on an entity at a specified spacing
按颜色	By Color
按默认或自定义参数生成建筑户型平面图：墙体（剪力墙涂黑/双线内墙）、门（开启弧）、窗、家具图块、房间标注、四周建筑尺寸链与右侧图签	Generate a floor plan with default or custom parameters: walls (shear walls filled black / double-line interior walls), doors (swing arcs), windows, furniture blocks, room dimensions, surrounding dimension chains, and the right-side title block
按默认或自定义参数生成法兰盘主视图、俯视图、左视图	Generate flange front, top, and left views with default or custom parameters
按默认或自定义参数生成法兰盘轮毂视图（全剖 A—A）与左视图（含剖面线、剖切符号、粗糙度、尺寸标注）	Generate flange hub views (front view full section A-A) and left view with default or custom parameters (including hatch lines, section cut symbols, surface roughness, and dimensions)
按默认或自定义参数生成箱体主视图（全剖）、俯视图 B、A—A 断面、C 向视图（含剖切符号与粗糙度）	Generate housing front view (full section), top view B, A-A section, and C-direction view with default or custom parameters (including section cut symbols and surface roughness)
按默认或自定义参数生成轴承座主视图、俯视图、左视图（左视图全剖）	Generate bearing housing front, top, and left views with default or custom parameters (left view full section)
按默认或自定义尺寸生成飞机正面图、侧面图、俯视图	Generate aircraft front, side, and top views with default or custom dimensions
捕捉	Snap
捕捉 %1 已%2	Snap %1 %2
捕捉与追踪	Snap && Tracking
捕捉开关  [E]端点%1  [T]切线%2  [M]中点%3  [C]圆心%4  [X]交点%5   （绘制中按对应键切换）	Snap toggles  [E]Endpoint%1  [T]Tangent%2  [M]Midpoint%3  [C]Center%4  [X]Intersection%5   (press the corresponding key while drawing to toggle)
捕捉开关② [Q]象限%1 [P]垂足%2 [I]插入%3 [N]节点%4 [A]最近%5 [O]外观%6 [L]平行%7 [W]延伸%8 [B]两点中点%9	Snap toggles② [Q]Quadrant%1 [P]Perpendicular%2 [I]Insertion%3 [N]Node%4 [A]Nearest%5 [O]Apparent%6 [L]Parallel%7 [W]Extension%8 [B]Midpoint between two points%9
捕捉开关或设置步长	Toggle snap or set the step size
捕捉模式（独立启用/禁用）	Snap modes (enable/disable independently)
捕捉设置 (OSNAP / 追踪)	Snap Settings (OSNAP / Tracking)
捕捉设置 (Shift+右键 = 临时覆盖)	Snap Settings (Shift+right-click = temporary override)
捕捉设置...	Object Snap Settings...
推拉门宽	Sliding door width
提示	Hint
提示=Note	Hint =Note
提示：GB/T 4457.5 规定同一零件剖面线间隔相等、方向相同，一般与主要轮廓成 45°；\n装配图中相邻零件剖面线方向应相反或间隔不等。	Hint: GB/T 4457.5 specifies that hatch lines of the same part have equal spacing and the same direction, generally at 45° to the main outline;\nin assembly drawings, hatch lines of adjacent parts should have opposite directions or unequal spacing.
提示：help <命令名> 查看用法/参数/样例；Ctrl+Shift+P 打开命令面板。	Hint: help <command name> for usage/parameters/examples; Ctrl+Shift+P opens the command palette.
提示：修改字体/字高/宽度因子后，所有引用该样式的文字会同步更新。	Hint: after modifying the font/text height/width factor, all text referencing the style is updated synchronously.
提示：把选中图元移入图层时保留其原有属性；修改图层样式/显示会同步到本层全部图元，不影响其他层。	Hint: moving selected entities into a layer keeps their original properties; changing layer styles/display syncs to all entities on that layer without affecting other layers.
提示：标准命名形如 编号_名称（如 01_轮廓线）。可自由命名，仅作建议。	Hint: standard names take the form Number_Name (e.g. 01_Contour Line). You may name them freely; this is only a suggestion.
插件 (*.dll *.so *.dylib);;所有文件 (*.*)	Plugins (*.dll *.so *.dylib);;All Files (*.*)
插件(&X)	Pl&ugins
插件加载失败	Failed to load plugin
插件加载失败 [%1]：%2	Failed to load plugin [%1]: %2
插件加载成功：%1	Plugin loaded: %1
插件已加载：%1	Plugin loaded: %1
插件已卸载：%1	Plugin unloaded: %1
插入到光标处	Insert at cursor
插入参数：	Insertion parameters:
插入块	Insert Block
插入块 	Insert Block 
插入块(&I)...	Insert Block(&I)...
插入失败	Insert failed
插入失败：块「%1」不存在或已被删除。	Insert failed: block '%1' does not exist or has been deleted.
插入字段…	Insert Field...
插入标准标题栏	Insert Standard Title Block
插入标准标题栏...	Insert Standard Title Block...
插入样条点	Insert Spline Point
插入样条点：选择样条并点击段上位置	Insert Spline Point: select a spline and click a position on a segment
插入点 (0,0)、比例 1.0、旋转 0°（默认值）	Insertion point (0,0), scale 1.0, rotation 0° (defaults)
插入点 (0,0)、比例 1.0、旋转 0°（默认值）\n参照块名：%1\n记录路径：%2%3	Insertion point (0,0), scale 1.0, rotation 0° (defaults)\nReference block name: %1\nRecord path: %2%3
插入点 X	Insertion point X
插入点 Y	Insertion point Y
插入点 x,y：	Insertion point x,y:
插入点坐标无效	Invalid insertion point coordinates
摆放方向	Orientation
撤销	Undo
撤销上一步编辑	Undo last edit
撤销差量化存储（关则回落全量模式）	Undo differential storage (off falls back to full mode)
撤销栈深度：	Undo Stack Depth:
操作	Action
支承板底宽	Support plate base width
支持：对齐/距离/水平/竖直/半径/直径/角度；容忍 R/⌀/° 前后缀	Supports: aligned/distance/horizontal/vertical/radius/diameter/angular; tolerates R/⌀/° prefixes/suffixes
改名失败（旧名不存在/新名为空或重名）	Rename failed (old name does not exist / new name is empty or duplicated)
改名失败：图层 %1 已存在	Rename failed: layer %1 already exists
放大	Zoom In
放大 (Zoom In)	Zoom In
放弃选择	Deselect
放置临时追踪点	Place temporary tracking point
放置到图纸	Place on Drawing
放置单行文字	Place single-line text
放置多行文字	Place multiline text
放置焊缝符号	Place weld symbol
数值→驱动几何联动调整；留空→恢复测量值；文字→覆盖显示文本：	Value → drive linked geometry adjustment; leave blank → restore the measured value; text → override the display text:
数据库	Database
数据库(&D)	Database(&D)
数据库: 已连接	Database: connected
数据库: 未连接（使用文件）	Database: not connected (using files)
数据库名，如 sketchcad	Database name, e.g. sketchcad
数据库名：	Database name:
数据库文件：	Database File:
数据库服务器地址，留空则 localhost	Database server address, leave blank for localhost
数据库类型：	Database type:
数据库绘图记录	Database drawing records
数据库连接信息保存在本地配置文件 ~/SketchCAD/db_config.json，程序启动即按此连接；修改后点击「保存并连接」立即生效（无需关闭本对话框）。未连接时程序自动使用文件存储，不影响绘图。	Database connection info is saved in the local config file ~/SketchCAD/db_config.json and the program connects accordingly at startup; after modifying, click 'Save and Connect' to apply immediately (no need to close this dialog). When not connected, the program automatically uses file storage, which does not affect drawing.
数据库配置	Database Configuration
数据库配置...	Database Configuration...
数据库配置已保存到：%1	Database configuration saved to: %1
数据库配置已应用，但写入配置文件失败：%1	Database configuration applied, but failed to write the config file: %1
数量须 ≥1	Quantity must be ≥1
数量须 ≥2	Quantity must be ≥2
整个图形	Entire drawing
整体选中编组	Select group as a whole
文件(&F)	&File
文件位置	File Locations
文件加载失败：不是有效的草图文件。	Failed to load file: not a valid sketch file.
文件加载失败：无法打开文件。\n%1	Failed to load file: cannot open the file.\n%1
文件名 %<FILENAME>%	File name %<FILENAME>%
文件路径 %<FILEPATH>%	File path %<FILEPATH>%
文字	Text
文字内容：	Text content:
文字创建失败	Failed to create text
文字样式	Text Style
文字样式不存在：%1	Text style does not exist: %1
文字样式已存在：%1	Text style already exists: %1
文字样式管理器 (STYLE)	Text Style Manager (STYLE)
文字样式管理器 (STYLE)：创建/修改/删除样式，统一字体、字高、宽度因子并批量应用到已有文字	Text Style Manager (STYLE): create/modify/delete styles, unify font, text height, and width factor, and apply in batch to existing text
文字样式管理器...	Text Style Manager...
文字样式（字体）：引用 STYLE 表；修改样式将同步更新所有引用它的文字	Text style (font): references the STYLE table; modifying a style updates all text referencing it synchronously
文字样式：	Text style:
文字样式：名称为空，已忽略	Text style: name is empty, ignored
文字样式：新名称为空，已忽略	Text style: new name is empty, ignored
文字随图框比例调整	Scale text with the drawing frame
文本=Text	Text =Text
文本分解为字...	Explode text into characters...
斜体	Italic
断开关联失败：图元 #%1 不是剖面区域	Failed to break association: entity #%1 is not a section region
断开填充关联 	Break Hatch Association 
新内容：	New content:
新名称:	New name:
新名称：	New name:
新图元线型 -> %1	New element line style -> %1
新图元线宽 -> %1	New element line width -> %1
新图元颜色 -> %1	New entity color -> %1
新块名不能为空	New block name cannot be empty
新增/覆盖命令别名（acad.pgp 兼容）	Add/override command aliases (acad.pgp compatible)
新建	&New
新建区域	New Region
新建区域...	New Region...
新建后自动生成对应图框与标准标题栏，可随后在「填写标题栏」中录入。	A corresponding drawing frame and standard title block are generated automatically after creation; you can then enter the data in 'Fill in Title Block'.
新建图层	New Layer
新建图层 	New Layer 
新建图层失败：名称不能为空	Failed to create layer: name cannot be empty
新建图层失败：图层 %1 已存在	Failed to create layer: layer %1 already exists
新建图框	New Drawing Frame
新建图框 — GB/T 14689	New Drawing Frame — GB/T 14689
新建图框...	New Drawing Frame...
新建图纸	New Drawing
新建图纸 — 选择幅面	New Drawing — Select Sheet Size
新建块 	New Block 
新建失败	Failed to create
新建工作空间	New Workspace
新建工作空间（保存当前视图）...	New Workspace (Save Current View)...
新建布局 	New Layout 
新建布局失败（名字为空或已存在）	Failed to create layout (name is empty or already exists)
新建文字样式	New Text Style
新建文字样式 	New Text Style 
新建标注样式 	New Dimension Style 
新建自定义工具条	New Custom Toolbar
新建自定义工具条...	New Custom Toolbar...
新建边界区域	New Boundary Region
新标注样式名不能为空	New dimension style name cannot be empty
新编组名：	New group name:
方向	Direction
旁注（可空）：	Side Note (optional):
旋转	Rotate
旋转...	Rotate...
旋转两条图元...	Rotate two entities...
旋转中心 X	Rotation center X
旋转中心 Y	Rotation center Y
旋转符号	Rotation Symbol
旋转角	Rotation angle
旋转角无效	Invalid rotation angle
无	None
无 id = 全部图元	No id = all entities
无参 = 列出全部；MText/Table 中以 %<PROP:名称>% 引用	No parameter = list all; reference via %<PROP:Name>% in MText/Table
无参数 = 列出 12 类表名；standards <表名> = 列出该表全部键值；standards <表名> <键名> = 查询单值	No parameter = list all 12 table names; standards <table name> = list all key-value pairs of that table; standards <table name> <key name> = query a single value
无参数；打开文档自动刷新见选项 fields/autoUpdateOnOpen	No parameter; for auto-refresh on document open, see option fields/autoUpdateOnOpen
无可偏移图元	No entities to offset
无可分解图元	No entities to explode
无可分解对象	No objects to explode
无可分解文本	No text to explode
无可反向图元	No entities to reverse
无可变换图元	No entities to transform
无可复制图元	No entities to copy
无可复制图元（请先选中）	No entities to copy (select first)
无可复制的选中图元	No selected entities to copy
无可延长图元	No entities to extend
无可拉伸图元	No entities to stretch
无可撤销操作	Nothing to undo
无可旋转图元	No entities to rotate
无可用标准值	No standard values available
无可移动图元（请先选中）	No entities to move (please select first)
无可缩放图元	No entities to scale
无可连合图元（请先选中 Line/Arc）	No entities to join (please select Line/Arc first)
无可重做操作	Nothing to redo
无可镜像图元	No entities to mirror
无可阵列图元（请先选中）	No entities to array (select first)
无效插入点: %1	Invalid insertion point: %1
无效旋转角: %1	Invalid rotation angle: %1
无效的图元 id: 	Invalid entity id: 
无效的图元 id：%1	Invalid entity id: %1
无效的对称轴图元 id：%1	Invalid symmetry axis entity id: %1
无效的引用 id: %1	Invalid reference id: %1
无效的控制点索引：%1	Invalid control point index: %1
无效的数值参数：%1	Invalid numeric parameter: %1
无效的数值：%1	Invalid value: %1
无效的标注 id：%1	Invalid dimension id: %1
无效线宽: %1	Invalid lineweight: %1
无效透明度(需0..1): %1	Invalid transparency (must be 0..1): %1
无效颜色: %1	Invalid color: %1
无标题栏（仅图框）	No title block (drawing frame only)
无法从「%1」解析数值	Cannot parse a value from '%1'
无法写入 %1	Cannot write %1
无法创建圆	Cannot create circle
无法创建圆弧	Cannot create arc
无法创建圆弧: 	Cannot create arc: 
无法创建多边形	Cannot create polygon
无法创建多边形: 	Cannot create polygon: 
无法创建曲线	Cannot create curve
无法创建曲线: 	Cannot create curve: 
无法创建椭圆	Cannot create ellipse
无法创建椭圆: 	Cannot create ellipse: 
无法创建输出目录：%1	Cannot create output directory: %1
无法删除	Cannot delete
无法取得来源端点	Cannot get source endpoint
无法打开 DWG	Cannot open DWG
无法打开打赏链接，请检查配置：<br>%1	Cannot open the tip link, please check the configuration: <br>%1
无法打开文件: %1	Cannot open file: %1
无法根据扩展名判断脚本语言：%1	Cannot determine the script language from the extension: %1
无法由所选图元生成关联填充（边界须闭合且可成环）	Cannot generate an associative hatch from the selected entities (boundaries must be closed and form a loop)
无路径（只记录文件名）	No path (only the file name is recorded)
无限长构造直线（过点+角度 / 两点 / 平行 / 平分）	Infinite construction line (point+angle / two points / parallel / bisector)
无（沿用图元颜色与线宽组）	None (use entity color and lineweight group)
日期 %<DATE>%	Date %<DATE>%
时间 %<TIME>%	Time %<TIME>%
明细表（BOM）	BOM Table
明细表：图纸尚无 BOM 数据	BOM table: no BOM data in the drawing yet
星形	Star
星形：左侧输入角数，点击圆心与外顶点	Star: enter the number of points on the left, then click the center and an outer vertex
是否显示左分区	Show left partition
显式几何约束 %1 条：	Explicit geometric constraints: %1
显示	Display
显示/隐藏整层	Show/hide entire layer
显示指定点坐标	Display the coordinates of a specified point
显示栅格	Display Grid
显示栅格（F7）	Display Grid (F7)
显示标题（留空则用 id）	Show title (leave blank to use id)
显示线宽（LWDISPLAY）	Display Lineweight (LWDISPLAY)
智能标注	Smart Dimension
智能标注：先选中一条直线(→对齐标注)或一个圆(→直径标注)，一键生成，免逐点拾取	Smart dimension: first select a line (→ aligned dimension) or a circle (→ diameter dimension), then generate in one click without picking points one by one
智能标注：请先选中一条直线或一个圆	Smart dimension: please select a line or a circle first
暗色	Dark
曲线：定义点不足，已忽略	Curve: too few defining points, ignored
更多模板...	More Templates...
更新时间	Update Time
替换当前选择集	Replace Current Selection
最近列表：已移除缺失项 %1	Recent list: removed missing item %1
最近图元: #%1 %2（距离 %3 mm）	Nearest entity: #%1 %2 (distance %3 mm)
最近打开(&R)	Open &Recent
最近文件数：	Number of recent files:
有	Yes
有 %1 张图纸存在未保存的修改。是否全部保存？	%1 drawings have unsaved changes. Save all?
服务地址：	Service Address:
未	No
未勾选任何图纸。	No drawings selected.
未发现不合规项（执行 STDCHECK 后更新）。	No non-compliant items found (updated after running STDCHECK).
未发现可连合的端点相接链（同层 Line/Arc 且端点距 ≤ 容差）	No joinable chain of endpoints found (same-layer Line/Arc with endpoint distance ≤ tolerance)
未发现含域的 MText/Table 图元	No MText/Table entities with fields found
未命中任何图元	No entities matched
未命中端点（basePt 必须落在图元端点 1e-3 mm 容差内）	Endpoint not hit (basePt must fall within the 1e-3 mm tolerance of an entity endpoint)
未命名边界（%1 个）	Unnamed boundaries (%1)
未实现的折线子命令: 	Unimplemented polyline subcommand: 
未实现的构造线子命令: 	Unimplemented construction line subcommand: 
未实现的样条子命令: 	Unimplemented spline subcommand: 
未实现的椭圆子命令: 	Unimplemented ellipse subcommand: 
未实现的直线子命令: 	Unimplemented line subcommand: 
未找到任何 BOM 明细数据	No BOM detail data found
未找到可分解的文本图元	No explodable text entities found
未找到命名 UCS: 	Named UCS not found: 
未找到图元 #	Entity # not found
未找到图元 #%1	Entity #%1 not found
未找到折线 #	Polyline # not found
未找到源图元 #%1	Source entity #%1 not found
未找到键：%1（%2 可用键：%3）	Key not found: %1 (%2 available keys: %3)
未提供目标 id	No target id provided
未检测到重复图元	No duplicate entities detected
未满足	Unsatisfied
未知 dimension 子命令：%1（支持 link / setvalue）	Unknown dimension subcommand: %1 (supported: link / setvalue)
未知 modify 子命令: 	Unknown modify subcommand: 
未知 param 子命令：%1（支持 set / list）	Unknown param subcommand: %1 (supported: set / list)
未知 ucs 子命令: 	Unknown ucs subcommand: 
未知中心线类型	Unknown centerline type
未知剪贴板子命令: 	Unknown clipboard subcommand: 
未知命令: 	Unknown command: 
未知命令：%1（您是不是想输入: %2？）	Unknown command: %1 (did you mean: %2?)
未知命令：%1（输入 help 查看全部命令）	Unknown command: %1 (type help to see all commands)
未知图元类型: 	Unknown entity type: 
未知图层命令: layer %1	Unknown layer command: layer %1
未知块命令: block %1	Unknown block command: block %1
未知外部参照命令: xref %1	Unknown external reference command: xref %1
未知子命令 %1（可用 close/open/width/join/fit）	Unknown subcommand %1 (available: close/open/width/join/fit)
未知子命令 %1（可用：create/add/remove/list/ungroup）	Unknown subcommand %1 (available: create/add/remove/list/ungroup)
未知子命令 %1（可用：new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw）	Unknown subcommand %1 (available: new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw)
未知字段: %1	Unknown field: %1
未知对齐方式: 	Unknown alignment: 
未知幅面：%1（可用：%2）	Unknown sheet size: %1 (available: %2)
未知平面: 	Unknown plane: 
未知方向：%1（可用 portrait / landscape）	Unknown orientation: %1 (use portrait / landscape)
未知材料密度：%1	Unknown material density: %1
未知标注类型: 	Unknown dimension type: 
未知约束类型：%1（输入 constraint help 查看全部 %2 种）	Unknown constraint type: %1 (type constraint help to see all %2 types)
未知表：%1（可用表：%2）	Unknown table: %1 (available tables: %2)
未知读数模式: 	Unknown readout mode: 
未知选择命令: 	Unknown selection command: 
未知选项 %1（可用 scale/angle/pattern）	Unknown option %1 (available: scale/angle/pattern)
未知镜像方式: 	Unknown mirror mode: 
未能从「%1」读取到任何块定义。	Could not read any block definitions from '%1'.
未能加载插件：%1\n详见日志面板（ABI 不兼容 / 符号缺失 / 初始化失败等）。	Failed to load plugin: %1\nSee the log panel for details (ABI incompatibility / missing symbols / initialization failure, etc.).
未载入过别名文件，改为注入默认别名	No alias file loaded; injecting default aliases instead
未连接（使用文件存储）	Not connected (using file storage)
未选中任何端点（basePt 必须命中图元端点容差 1e-3 mm）	No endpoint selected (basePt must hit an entity endpoint within the 1e-3 mm tolerance)
未选中要删除的图元	No entities selected for deletion
未选择图纸文件。	No drawing file selected.
末	End
本地（Ollama / llama.cpp 等本地服务）	Local (Ollama / llama.cpp and similar local services)
本程序内置全部线型，无需从外部 .lin 文件加载	This program has all linetypes built in; no need to load them from external .lin files
本程序默认不上传任何数据，没有任何后台网络请求。\n「导出诊断包」（帮助菜单 / 异常退出提示）只在本机生成 ZIP 文件，\n包内不含图纸几何与文字内容，是否发送完全由您决定。	By default this program uploads no data and makes no background network requests.\n'Export Diagnostics Bundle' (Help menu / crash prompt) only creates a local ZIP file;\nthe bundle contains no drawing geometry or text, and whether to send it is entirely up to you.
机械制图国标数据（内嵌《机械制图手册（第四版）》）：共 %1 类表，只读浏览。	Mechanical drawing GB standard data (embedded 'Mechanical Drawing Handbook (Fourth Edition)'): %1 table categories in total, read-only browsing.
机械图拆分	Drawing Split
机械图拆分：%1/%2 组写出至 %3（错误 %4）	Mechanical drawing split: %1/%2 groups written to %3 (errors %4)
机械图拆分：分组结果为空	Mechanical drawing split: grouping result is empty
机械图拆分：图纸为空	Drawing Split: drawing is empty
机身总长	Fuselage overall length
机身高度	Fuselage height
杆长 L(mm)：	Rod length L(mm):
材料	Material
材料类别	Material Category
材料：	Material:
条数	Count
来源折线已闭合，无法合并	Source polyline is closed and cannot be merged
极轴增量角	Polar increment angle
极轴角相对上一段方向（否则用绝对 0/90/180/270 基准）	Polar angle relative to the previous segment direction (otherwise uses absolute 0/90/180/270 reference)
极轴追踪	Polar Tracking
极轴追踪 (F10)：按增量角度追踪落点	Polar tracking (F10): tracks points by increment angle
极轴追踪 (POLAR)	Polar Tracking (POLAR)
极轴追踪 (POLAR)：%1（增量 %2°）	Polar Tracking (POLAR): %1 (increment %2°)
极轴追踪开关或设置增量角	Toggle polar tracking or set the increment angle
极轴追踪：%1	Polar Tracking: %1
构造线	Construction Line
构造线 (Xline)：向两端无限延伸的辅助线，连续绘制，右键/Esc 退出	Construction Line (Xline): an auxiliary line extending infinitely in both directions, drawn continuously, right-click/Esc to exit
构造线(Xline)：选择绘制方式(H/V/A/两点/角平分线/平行)，向两端无限延伸	Construction Line (Xline): select a draw method (H/V/A/2 points/angle bisector/parallel), extends infinitely in both directions
构造线=Xline	Construction line =Xline
构造线绘制方式	Construction Line Draw Method
查询	Inquiry
查询内嵌的 12 类机械制图国标数据（图幅/比例/字体/线型/粗糙度/公差/形位公差/螺纹/齿轮/轴承/焊缝/CAD 图层），数据源自《机械制图手册（第四版）》	Query the built-in 12 categories of GB mechanical drawing data (sheet sizes / scales / fonts / linetypes / surface roughness / tolerances / geometric tolerances / threads / gears / bearings / welds / CAD layers), sourced from Mechanical Drawing Handbook (4th Edition)
查询内嵌的机械制图国标知识库（数据源自《机械制图手册（第四版）》）。table 为标准表名（12 类之一：sheet_sizes / scales / fonts / line_types / roughness / tolerances / geo_tol / thread / gear / bearing / weld / cad_layers）；给定 key 时返回该键的值，省略 key 时返回该表全部键值。	Query the built-in GB mechanical drawing knowledge base (sourced from Mechanical Drawing Handbook (4th Edition)). table is a standard table name (one of 12: sheet_sizes / scales / fonts / line_types / roughness / tolerances / geo_tol / thread / gear / bearing / weld / cad_layers); when key is given, returns that key's value; when key is omitted, returns all key-value pairs of the table.
查询坐标 / 最近图元	Query coordinates / nearest entity
查询某条命令的用法与示例（name 为命令名，如 circle）。	Query the usage and examples of a command (name is the command name, e.g. circle).
查询点  X , Y	Query point  X , Y
栅格	Grid
栅格吸附 (F9)：坐标量化到栅格步长	Grid snap (F9): quantizes coordinates to the grid step
栅格吸附 (SNAP)：%1（步长 %2 mm）	Grid Snap (SNAP): %1 (step %2 mm)
栅格吸附（F9）：坐标量化到步长整数倍	Grid Snap (F9): coordinates are quantized to integer multiples of the step
栅格吸附：把坐标量化到步长的整数倍	Grid snap: quantizes coordinates to integer multiples of the step
栅格已	Grid is
栅格显示开/关 (Grid)	Grid Display On/Off (Grid)
栅格显示开关	Grid Display Toggle
标准只接受 gbt / iso	Standard accepts only gbt / iso
标准标题栏（GB/T 10609.1）	Standard Title Block (GB/T 10609.1)
标准知识库	Standard Knowledge Base
标准知识库(&B)...	Standard Knowledge Base(&B)...
标准知识库查询	Standard Knowledge Base Query
标准知识库（内嵌《机械制图手册（第四版）》%1 类国标表）：	Standard Knowledge Base (%1 built-in GB standard table categories from Mechanical Drawing Handbook (4th Edition)):
标准系列（点击即赋值 / 输入即实时高亮最近值）：	Standard Series (click to assign / type to live-highlight the nearest value):
标准选项（随特征即时刷新）：	Standard Options (refreshed with the feature):
标注	Dimension
标注 #%1（%2）不支持尺寸驱动，支持：对齐/距离/水平/竖直/半径/直径/角度	Dimension #%1 (%2) does not support dimension driving; supported: aligned/distance/horizontal/vertical/radius/diameter/angle
标注=Dimension	Dimension =Dimension
标注文本	Dimension Text
标注标准	Dimension Standard
标注标准（GB/T 4458 国标 / ISO 129 国际）：切换后已有标注按所选标准的箭头/文字方向/公差规则自动重绘	Dimension Standard (GB/T 4458 national / ISO 129 international): after switching, existing dimensions are automatically redrawn with the selected standard's arrow/text direction/tolerance rules
标注样式	Dimension Style
标注样式 %1 不存在	Dimension style %1 does not exist
标注样式 %1 已存在	Dimension style %1 already exists
标注样式 %1 已改名为 %2（%3 个标注同步）	Dimension style %1 renamed to %2 (%3 dimensions synchronized)
标注样式名不能为空	Dimension style name cannot be empty
标注样式的创建/切换/应用/删除（一图多标准）	Create / switch / apply / delete dimension styles (multiple standards in one drawing)
标注样式表至少保留一条	Dimension style table must keep at least one
标注样式表（* = 当前）：\n%1\n当前样式 = %2	Dimension Style Table (* = current):\n%1\nCurrent style = %2
标注随图框比例调整	Scale Dimensions with Drawing Frame
标题栏1	Title Block 1
标题栏样式	Title Block Style
标题：	Title:
标高值：	Elevation:
标高符号	Elevation Symbol
栏选	Fence Select
栏选(FENCE)：画一条多段栏线，与栏线相交的图元全部加选	Fence Select (FENCE): draw a multi-segment fence; every entity intersecting the fence is added to the selection
样例：%1	Sample: %1
样式	Style
样式不存在。	Style does not exist.
样式名不能为空。	Style name cannot be empty.
样式名与现有样式冲突。	Style name conflicts with an existing style.
样式名为空或已存在。	Style name is empty or already exists.
样式名：	Style name:
样式表预设	Style Table Preset
样条	Spline
样条 #%1 控制点过少，已删除	Spline #%1 has too few control points, deleted
样条 #%1 无法分解为线段	Spline #%1 cannot be exploded into line segments
样条 #%1 无法分解为线段（点数不足）	Spline #%1 cannot be exploded into line segments (insufficient points)
样条化：顶点作为三次 B 样条控制点（PEDIT Spline）	Spline: vertices as cubic B-spline control points (PEDIT Spline)
样条打散	Explode Spline
样条控制点不足	Insufficient spline control points
样条曲线	Spline
样条曲线 (Spline)：过控制点的光滑曲线，右键结束	Spline Curve (Spline): a smooth curve passing through control points, right-click to finish
样条曲线=Spline	Spline =Spline
样条绘制方式	Spline Draw Method
样条编辑	Spline Edit
样条编辑命令	Spline Edit Command
样条编辑：由多段线创建 / 增删控制点 / 分解	Spline Edit: create from polyline / add or remove control points / explode
样条至少保留 2 个控制点	A spline must keep at least 2 control points
样条过点	Spline Through Points
样条过点（拟合样条）：逐点点击必经点，右键结束（Catmull-Rom 插值）	Spline Through Points (fitted spline): click each required point, right-click to finish (Catmull-Rom interpolation)
根部圆角 R	Root fillet R
框取消完成：剩余选中 %1 个图元	Window deselect complete: %1 entities remain selected
框格符号：%1	Grid symbol: %1
框选完成：选中 %1 个图元%2	Window selection complete: %1 entities selected%2
框选平行线/圆阵列，一次性生成基线或连续标注串（复用 dim 基线/连续引擎）	Window-select parallel lines / circle arrays to generate a baseline or continuous dimension chain in one pass (reuses the dim baseline/continue engine)
检测到 %1 份上次未正常保存的图纸快照	Detected %1 drawing snapshots from the last session that were not saved properly
检测到上次异常退出	Detected an abnormal exit during the previous session
检测到上次异常退出留下的自动保存快照。\n选择一份恢复到其原图纸位置，或丢弃（丢弃不可撤销）。	Autosave snapshots left by the last abnormal exit detected.\nChoose one to restore it to its original drawing location, or discard (discarding cannot be undone).
检测到上次运行未正常退出（可能发生崩溃或被强制结束）。\n\n是否导出诊断包？包内包含日志、崩溃转储与环境信息（不含图纸内容），\n只生成本机 ZIP 文件，不会自动发送。\n\n「Yes」= 导出诊断包；「No」= 查看日志目录；「Cancel」= 忽略。	The last run did not exit normally (a crash or forced termination may have occurred).\n\nExport a diagnostics bundle? It contains logs, a crash dump, and environment information (no drawing content),\nand only generates a local ZIP file; nothing is sent automatically.\n\n'Yes' = export diagnostics bundle; 'No' = view the log directory; 'Cancel' = ignore.
椭圆	Ellipse
椭圆 (Ellipse (1 Point))：点击中心，再点椭圆上一点	Ellipse (1 Point): click the center, then click a point on the ellipse
椭圆(1点)	Ellipse (1 Point)
椭圆(1点)：点击中心，再点椭圆上一点（作为长轴端点，短轴取长轴一半）	Ellipse (1 Point): click the center, then click a point on the ellipse (as the major axis endpoint, minor axis = half the major axis)
椭圆(4点)	Ellipse (4 Points)
椭圆(4点)：依次点击椭圆上四个点，拟合轴对齐椭圆	Ellipse (4 Points): click four points on the ellipse in order to fit an axis-aligned ellipse
椭圆(中心+3点)	Ellipse (Center+3 Points)
椭圆(中心+3点)：先点中心，再点椭圆上三个点，拟合旋转椭圆	Ellipse (Center+3 Points): click the center first, then three points on the ellipse, to fit a rotated ellipse
椭圆(内切)	Ellipse (Inscribed)
椭圆(内切)：先选中一个矩形或多边形，生成其内切椭圆	Ellipse (Inscribed): select a rectangle or polygon first to generate its inscribed ellipse
椭圆(焦点)	Ellipse (Foci)
椭圆(焦点)：依次点击两个焦点，再点椭圆上一点	Ellipse (Foci): click two foci in order, then click a point on the ellipse
椭圆(轴)	Ellipse (Axis)
椭圆(轴)：点击中心、长轴端点、椭圆上一点	Ellipse (Axis): click the center, the major axis endpoint, and a point on the ellipse
椭圆=Ellipse	Ellipse =Ellipse
椭圆弧	Elliptical Arc
椭圆弧(1点)	Elliptical Arc (1 Point)
椭圆弧(1点)：中心 + 长轴端点 + 过点	Elliptical Arc (1 Point): center + major axis endpoint + point on ellipse
椭圆弧(轴)	Elliptical Arc (Axis)
椭圆弧(轴)：中心 + 长轴端点 + 角度点	Elliptical Arc (Axis): center + major axis endpoint + angle point
椭圆绘制方式	Ellipse Draw Method
椭圆：半轴为 0 或无效，已忽略	Ellipse: semi-axis is 0 or invalid; ignored
模型为空：无图元可列出	Model is empty: no entities to list
模型名，如 deepseek-chat / gpt-4o-mini / qwen-plus	Model name, e.g. deepseek-chat / gpt-4o-mini / qwen-plus
模型浏览器	Model Browser
模型浏览器=Model browser	Model browser =Model browser
模型空间	Model Space
模型空间%1	Model Space%1
模型：	Model:
模数 m(mm)：	Module m(mm):
模板	Template
模板参考尺寸 %1 × %2 mm。\n输入目标图纸尺寸（宽,高，单位 mm），如 420,297：	Template reference size %1 × %2 mm.\nEnter the target drawing size (width,height, unit mm), e.g. 420,297:
模板名称	Template Name
模板名称:	Template Name:
横向	Landscape
横向（长边水平）	Landscape (long edge horizontal)
横置	Horizontal
正交	Ortho
正交折线	Ortho Polyline
正交折线(水平起)	Orthogonal Polyline (starts horizontal)
正交折线(竖直起)	Ortho Polyline (Vertical Start)
正交模式 (F8)：方向锁定到水平/垂直	Ortho mode (F8): locks direction to horizontal/vertical
正交模式 (ORTHO)：%1	Ortho Mode (ORTHO): %1
正交模式开关	Ortho Mode Toggle
正交模式（F8）：落点锁定水平/垂直	Ortho Mode (F8): locks placement to horizontal/vertical
正交模式：把落点方向锁定到水平/垂直	Ortho mode: locks placement direction to horizontal/vertical
正在执行命令：	Executing command: 
正多边形	Regular Polygon
正多边形(内接)	Regular Polygon (Inscribed)
正多边形(内接)：左侧输入边数，点击圆心与圆周顶点	Regular Polygon (Inscribed): enter the number of sides on the left, then click the center and a vertex on the circle
正多边形(外切)	Regular Polygon (Circumscribed)
正多边形(外切)：左侧输入边数，点击圆心与边中点	Regular Polygon (Circumscribed): enter the number of sides on the left, then click the center and the midpoint of an edge
正多边形(边长)	Regular Polygon (By Edge)
正多边形(边长)：左侧输入边数，点击边的起点与终点	Regular Polygon (By Edge): enter the number of sides on the left, then click the start and end points of the edge
段数 n	Number of Segments n
段索引无效	Invalid segment index
段节点不足	Insufficient segment nodes
每 100 次操作	Every 100 operations
每 20 次操作	Every 20 operations
每 50 次操作	Every 50 operations
每布局一个 PDF	One PDF per layout
每行一个按钮：命令 [文字] [图标]\n示例：\nline 直线 line\ncircle 圆 circle\nzoom 缩放 zoom	One button per line: command [text] [icon]\nExample:\nline Line line\ncircle Circle circle\nzoom Zoom zoom
比例:	Scale:
比例:4.00	Scale:4.00
比例必须为正数	Scale must be a positive number
比例提示	Scale Hint
比例标注	Scale Dimension
比例格式错误（示例：1:2 → 出图缩小一半；2:1 → 放大一倍）	Scale format error (example: 1:2 → plot at half size; 2:1 → double size)
比例格式错误（示例：1:2）	Invalid scale format (example: 1:2)
比例计算	Scale Calculation
比例调整设置	Scale Adjustment Settings
比例须为正数	Scale must be positive
比率文本：	Ratio Text:
水平=Horizontal	Horizontal =Horizontal
水平居中	Center Horizontally
水平构造线	Horizontal Construction Line
水平标注	Horizontal Dimension
水平标注=Horizontal annotation	Horizontal =Horizontal annotation
水平标注：只量 X 方向投影	Horizontal dimension: measures X projection only
水平线	Horizontal Line
求两圆/弧的全部公切线（外+内，0..4 条）	Find all common tangents to two circles/arcs (external + internal, 0..4 lines)
沉孔深	Counterbore Depth
没有可合并的线段	No line segments to merge
没有可导出的图元（请指定 id 或先选择）	No entities to export (specify an id or select first)
没有可应用样式的标注（请先选中标注）	No dimensions to apply a style to (please select a dimension first)
没有可放弃的点	No point to discard
没有可连合的链（只连合同层且端点在容差内相接的 Line/Arc）	No chains to join (only Line/Arc entities on the same layer whose endpoints meet within tolerance are joined)
没有可重复的命令	No command to repeat
没有对象被加入编组（检查是否已选中对象/已属该组）	No objects were added to the group (check whether objects are selected / already in the group)
没有对象被移出编组（先选中编组成员）	No objects removed from the group (select group members first)
没有线型为「%1」的图元可累加选择	No entities with linetype '%1' available to add to selection
没有该颜色的图元可累加选择	No entities of that color available to add to selection
没有选中任何图元	No entities selected
没有选中任何图元可移入图层 %1	No entities selected to move into layer %1
沿 Polyline/Spline 路径阵列选中图元	Array selected entities along a Polyline/Spline path
沿镜像轴复制/翻转选中图元	Copy/flip selected entities along a mirror axis
法兰厚度	Flange Thickness
法兰厚度 C	Flange Thickness C
法兰外圆粗糙度	Flange Outer Surface Roughness
法兰外径 D	Flange Outer Diameter D
法兰外径 Ø	Flange Outer Diameter Ø
法兰盘三面图...	Flange Three Views...
法兰盘轮毂视图...	Flange Hub View...
波浪线=Dash wave line	Wave =Dash wave line
测试连接	Test Connection
测量两点距离与角度	Measure distance and angle between two points
测量封闭图元或多点围成区域面积	Measure the area of a closed entity or a region enclosed by multiple points
测量查询	Measure Inquiry
测量查询 (Inquiry)：距离 / 面积 / 质量特性 / 点坐标	Measure Inquiry (Inquiry): Distance / Area / Mass Properties / Point Coordinates
测量查询(&I)	Measure && Inquiry
浏览...	Browse...
浏览内嵌的 12 类机械制图国标数据（图幅/比例/字体/线型/粗糙度/公差/形位公差/螺纹/齿轮/轴承/焊缝/CAD 图层）	Browse the 12 built-in categories of GB mechanical drawing data (sheet sizes / scales / fonts / linetypes / surface roughness / tolerances / geometric tolerances / threads / gears / bearings / welds / CAD layers)
添加	Add
添加/更新	Add/Update
添加几何约束	Add Geometric Constraint
添加到编组	Add to Group
添加到编组 %1	Add to Group %1
添加到编组失败：所选对象均为外部参照（Xref），禁止跨参照边界编组	Failed to add to group: all selected objects are external references (Xref); grouping across reference boundaries is not allowed
添加到选区	Add to Selection
添加圆	Add Circle
添加条件	Add Condition
添加直线	Add Line
添加选中对象	Add Selected Objects
添加：整体图形范围	Add: Entire Drawing Extents
清理	Cleanup
清理(&U)	Cleanup(&U)
清理完成：图层 %1 / 块 %2 / 标注样式 %3	Cleanup complete: layers %1 / blocks %2 / dimension styles %3
清理未引用的图层/块/标注样式	Cleanup unreferenced layers/blocks/dimension styles
清理选项	Cleanup Options
清空	Clear
清空图中全部图元	Clear all entities in the drawing
清空当前选择集	Clear Current Selection
清除临时追踪点	Clear Temporary Tracking Points
清除全部临时追踪点	Clear All Temporary Tracking Points
清除全部临时追踪点 (TK)	Clear All Temporary Tracking Points (TK)
清除垃圾图元（零长线/零径圆弧/NaN 坐标/空文字/空多边形）	Clear garbage entities (zero-length lines / zero-radius arcs / NaN coordinates / empty text / empty polygons)
清除手动线宽，恢复为当前线型对应的默认档位	Clear manual lineweight and restore the default level for the current linetype
清除文字样式关联	Clear Text Style Association
清除无引用的块定义（冗余数据）	Clear unreferenced block definitions (redundant data)
清除最近文件历史	Clear Recent File History
清除空图层（保留层 \"0\" 与当前层）【谨慎】	Clear empty layers (keep layer '0' and the current layer) [Caution]
清除覆盖（恢复原有配置）	Clear Override (Restore Previous Settings)
清除追踪点	Clear Tracking Points
渲染后端消费此开关；当前版本仅记录偏好	The rendering backend consumes this switch; the current version only records the preference
源 id 无效	Invalid source id
源图元不是可转换的折线/多边形/矩形	Source entity is not a convertible polyline/polygon/rectangle
源图元已失效	Source entity is no longer valid
源图元顶点数不足，无法转样条	Source entity has insufficient vertices to convert to a spline
满足	Satisfied
灰度(黑白)	Grayscale (Black and White)
点	Point
点 1  X , Y	Point 1  X , Y
点 2  X , Y	Point 2  X , Y
点-圆切线	Point-to-Circle Tangent
点=Point	Point =Point
点击「查看帮助」获取转换组件的下载与安装说明。	Click 'View Help' for download and installation instructions for the conversion component.
点击「预览统计」查看清理前后对比（不修改图面）。	Click 'Preview Statistics' to see a before/after cleanup comparison (does not modify the drawing area).
点击立即执行一次自动保存	Run Autosave Once Immediately
点划线=DashDot line	Dash-dot =DashDot line
点列	Point List
点列模式（%1 个顶点）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)	Point List Mode (%1 vertices): Area = %2 mm²    Perimeter = %3 mm    Centroid = (%4, %5)
点列模式：x1,y1 x2,y2 x3,y3 ...	Point List Mode: x1,y1 x2,y2 x3,y3 ...
点到直线垂线	Point-to-Line Perpendicular
点在圆/弧上=PointOnCircle	Point in circle / on =PointOnCircle
点在线上=PointOnLine	Point on line =PointOnLine
点坐标	Point Coordinates
点坐标 + 模型内最近图元及距离	Point Coordinates + Nearest Entity and Distance in Model
点坐标：点解析失败（应为数字）	Point Coordinates: point parse failed (should be a number)
点对称=SymmetricPoint	Symmetric point =SymmetricPoint
点线=Dotted line	Dotted =Dotted line
点解析失败	Failed to parse point
点解析失败: 	Point parse failed: 
点镜像	Point Mirror
点镜像：请在画布上单击指定对称中心点	Point Mirror: please click on the canvas to specify the center of symmetry
焊接符号	Weld Symbol
焊接类型：	Weld Type:
焊缝符号	Weld Symbol
焊缝符号(&W)...	Weld Symbol(&W)...
焊缝符号(WELD)：移动光标实时预览，单击左键放置（对象捕捉辅助精确定位）	Weld Symbol (WELD): move the cursor for a live preview, left-click to place (object snap assists precise positioning)
焊缝符号（GB/T 324）	Weld Symbol (GB/T 324)
焊脚 k	Weld Leg k
熔透焊	Full Penetration Weld
版本不支持（v=%1，当前 v=%2）	Version not supported (v=%1, current v=%2)
物理打印机	Physical Printer
特征：	Feature:
特性	Properties
特性(&P)...	Properties(&P)...
特性「%1」不存在	Property '%1' does not exist
特性匹配	Match Properties
特性匹配(&M)	Match Properties(&M)
特性匹配：已将 #%1 的属性复制给 %2 个图元	Match Properties: copied properties of #%1 to %2 entities
特性：	Properties:
特殊矩形	Special Rectangle
特粗线 2.0mm	2.0 mm extra thick
特粗线 5px=Extra thick	Extra thick 5px=Extra thick
环形阵列	Polar Array
环形阵列 %1 份 / %2°（中心 %3,%4）：新增 %5 个副本	Polar Array %1 items / %2° (center %3,%4): %5 new copies added
环形阵列...	Polar Array...
环形阵列个数至少为 2。	Polar array count must be at least 2.
环形阵列：%1 个源图元 → 新增 %2 个副本（%3 个 / %4°）	Polar Array: %1 source entities → %2 new copies added (%3 items / %4°)
现场焊	Field Weld
生成 PDF 时出错。	Error generating PDF.
生成失败（退化输入）	Generation failed (degenerate input)
用当前选择重定义	Redefine with Current Selection
用户	User
用户信息用于标题栏「设计人/单位」栏的默认值（应用习惯，不随图存档）。	User information is used as the default value for the 'Designer / Organization' field in the title block (an application preference, not saved with the drawing).
用户名：	User Name:
用户坐标系	User Coordinate System
用法: 	Usage: 
用法: %1 %2	Usage: %1 %2
用法: %1 %2 [参数]	Usage: %1 %2 [parameter]
用法: %1 <id> ...	Usage: %1 <id> ...
用法: %1 <外框矩形/多边形id>	Usage: %1 <outer frame rectangle/polygon id>
用法: %1 x1,y1 x2,y2 ...（≥2 点）	Usage: %1 x1,y1 x2,y2 ... (>=2 points)
用法: %1 x1,y1 x2,y2 或 %1 x,y 角度	Usage: %1 x1,y1 x2,y2 or %1 x,y angle
用法: align left|centerx|right|topy|centery|bottom	Usage: align left|centerx|right|topy|centery|bottom
用法: arc cx,cy r a0 a1	Usage: arc cx,cy r a0 a1
用法: area <封闭图元id> | area x1,y1 x2,y2 [x3,y3 ...]（或先选中图元/一组首尾相接的直线）	Usage: area <closed entity id> | area x1,y1 x2,y2 [x3,y3 ...] (or select an entity / a chain of end-to-end lines first)
用法: arraypath <路径id> <间距> | arraypath divide <路径id> <数量>	Usage: arraypath <path id> <spacing> | arraypath divide <path id> <count>
用法: arraypolar <cx,cy> <count> [角度范围]（选中圆/弧时可省略中心=圆心）	Usage: arraypolar <cx,cy> <count> [angle range] (center can be omitted when a circle/arc is selected; defaults to the circle center)
用法: arrayrect <行数> <列数> <行间距> <列间距>	Usage: arrayrect <rows> <columns> <row spacing> <column spacing>
用法: attdef <块名> <标签> [提示] [x,y] [字高]	Usage: attdef <block name> <tag> [prompt] [x,y] [text height]
用法: attedit <引用id> <标签> <值>	Usage: attedit <reference id> <tag> <value>
用法: block <create|insert|list|info|redefine|explode|delete|rename> ...	Usage: block <create|insert|list|info|redefine|explode|delete|rename> ...
用法: block create <块名> [基点x,y] [delete|keep]（delete 默认：用块引用替换原图元）	Usage: block create <block name> [base point x,y] [delete|keep] (delete by default: replace original entities with a block reference)
用法: block delete <块名>	Usage: block delete <block name>
用法: block explode <引用id>	Usage: block explode <reference id>
用法: block info <块名>	Usage: block info <block name>
用法: block insert <块名> <x,y> [旋转角] [sx] [sy]	Usage: block insert <block name> <x,y> [rotation angle] [sx] [sy]
用法: block redefine <块名>（用当前选择集替换块内几何）	Usage: block redefine <block name> (replace block geometry with the current selection set)
用法: block rename <旧名> <新名>	Usage: block rename <old name> <new name>
用法: break_at <id> <px,py>	Usage: break_at <id> <px,py>
用法: chain [on|off]（省略参数为切换）	Usage: chain [on|off] (omit the parameter to toggle)
用法: chamfer <id1> <id2> <d1> <d2> [angleDeg]	Usage: chamfer <id1> <id2> <d1> <d2> [angleDeg]
用法: circle cx,cy r	Usage: circle cx,cy r
用法: circle3p x1,y1 x2,y2 x3,y3	Usage: circle3p x1,y1 x2,y2 x3,y3
用法: copybase <x,y>	Usage: copybase <x,y>
用法: delete <id>	Usage: delete <id>
用法: deselect_layer <图层名>	Usage: deselect_layer <layer name>
用法: dim 	Usage: dim 
用法: dim arc <圆或圆弧 id> [起始角 终止角]	Usage: dim arc <circle or arc id> [start angle end angle]
用法: dim leader 起点x,y 折点x,y [文本点x,y]	Usage: dim leader start x,y bend x,y [text x,y]
用法: dim linear|aligned|h|v x1,y1 x2,y2 [尺寸线位置 px,py]	Usage: dim linear|aligned|h|v x1,y1 x2,y2 [dimension line position px,py]
用法: dim ordinate 被测点x,y 基准点x,y [旋转角]	Usage: dim ordinate measured point x,y datum point x,y [rotation angle]
用法: dim radius|dia <圆或圆弧 id> [引出角度]	Usage: dim radius|dia <circle or arc id> [leader angle]
用法: dist x1,y1 x2,y2	Usage: dist x1,y1 x2,y2
用法: divide <id> <n>	Usage: divide <id> <n>
用法: dynlist <块名>	Usage: dynlist <block name>
用法: dynset <引用id> <参数名> <值|状态名>	Usage: dynset <reference id> <parameter name> <value|state name>
用法: fieldupdate（无参数，重算全部域）	Usage: fieldupdate (no parameters, recomputes all fields)
用法: fillet <id1> <id2> <radiusMm>（radius=0 即倒角）	Usage: fillet <id1> <id2> <radiusMm> (radius=0 means chamfer)
用法: freehand_line x1,y1 x2,y2 ...（≥2 点）	Usage: freehand_line x1,y1 x2,y2 ... (>=2 points)
用法: grid [on|off]（省略参数为切换）	Usage: grid [on|off] (omit the parameter to toggle)
用法: id x,y	Usage: id x,y
用法: layer <add|delete|rename|visible|lock|set|assign|apply|list> ...	Usage: layer <add|delete|rename|visible|lock|set|assign|apply|list> ...
用法: layer add <名称> [颜色] [线宽] [线型]	Usage: layer add <name> [color] [lineweight] [linetype]
用法: layer alpha <名称> <0..1>	Usage: layer alpha <name> <0..1>
用法: layer apply <名称>（把图层样式套用到本层图元）	Usage: layer apply <name> (apply the layer style to entities on this layer)
用法: layer assign <名称>（把选中图元移入该层，保留图元原有属性）	Usage: layer assign <name> (move selected entities into this layer, keeping their original properties)
用法: layer delete <名称>	Usage: layer delete <name>
用法: layer frozen <名称> [on|off]	Usage: layer frozen <name> [on|off]
用法: layer lock <名称> [on|off]	Usage: layer lock <name> [on|off]
用法: layer plot <名称> [on|off]	Usage: layer plot <name> [on|off]
用法: layer rename <旧名> <新名>	Usage: layer rename <old name> <new name>
用法: layer set <名称> <color|width|style|frozen|plot|alpha> <值>	Usage: layer set <name> <color|width|style|frozen|plot|alpha> <value>
用法: layer visible <名称> [on|off]	Usage: layer visible <name> [on|off]
用法: lengthen <side:0|1> <deltaMm>	Usage: lengthen <side:0|1> <deltaMm>
用法: line x1,y1 x2,y2	Usage: line x1,y1 x2,y2
用法: line_2pt x1,y1 x2,y2	Usage: line_2pt x1,y1 x2,y2
用法: line_angle x1,y1 x2,y2 [angle]	Usage: line_angle x1,y1 x2,y2 [angle]
用法: line_bisector refId1 refId2 x,y	Usage: line_bisector refId1 refId2 x,y
用法: line_from_point_to_line refId x,y	Usage: line_from_point_to_line refId x,y
用法: line_h x1,y1 x2,y2	Usage: line_h x1,y1 x2,y2
用法: line_orthogonal refId x1,y1 x2,y2	Usage: line_orthogonal refId x1,y1 x2,y2
用法: line_orthogonal_from_line refId x1,y1 x2,y2	Usage: line_orthogonal_from_line refId x1,y1 x2,y2
用法: line_parallel refId x1,y1 x2,y2	Usage: line_parallel refId x1,y1 x2,y2
用法: line_parallel_pt refId x,y	Usage: line_parallel_pt refId x,y
用法: line_rel_angle refId x1,y1 x2,y2 [angle]	Usage: line_rel_angle refId x1,y1 x2,y2 [angle]
用法: line_snake p1 p2 ...	Usage: line_snake p1 p2 ...
用法: line_tangent_cc circleId1 circleId2	Usage: line_tangent_cc circleId1 circleId2
用法: line_tangent_ortho circleId lineId x,y	Usage: line_tangent_ortho circleId lineId x,y
用法: line_tangent_pc circleId x,y	Usage: line_tangent_pc circleId x,y
用法: line_v x1,y1 x2,y2	Usage: line_v x1,y1 x2,y2
用法: massprop <封闭图元id> ...（或先选中图元）	Usage: massprop <closed entity id> ... (or select entities first)
用法: mirror line <x1> <y1> <x2> <y2>	Usage: mirror line <x1> <y1> <x2> <y2>
用法: mirror point <x> <y>	Usage: mirror point <x> <y>
用法: mirror point <x> <y> | mirror line <x1> <y1> <x2> <y2> | mirror h | mirror v	Usage: mirror point <x> <y> | mirror line <x1> <y1> <x2> <y2> | mirror h | mirror v
用法: move <dx> <dy>	Usage: move <dx> <dy>
用法: offset <distMm>	Usage: offset <distMm>
用法: pasteblock <x,y>	Usage: pasteblock <x,y>
用法: plane xy|yz|xz	Usage: plane xy|yz|xz
用法: polyline x,y x,y [x,y ...]	Usage: polyline x,y x,y [x,y ...]
用法: polyline_add_node <id> x,y	Usage: polyline_add_node <id> x,y
用法: polyline_append_node <id> x,y	Usage: polyline_append_node <id> x,y
用法: polyline_change_segment_type <id> <i> <j>	Usage: polyline_change_segment_type <id> <i> <j>
用法: polyline_delete_between <id> <i> <j>	Usage: polyline_delete_between <id> <i> <j>
用法: polyline_delete_node <id> <idx>	Usage: polyline_delete_node <id> <idx>
用法: polyline_equidistant <id> <d>	Usage: polyline_equidistant <id> <d>
用法: polyline_trim_segments <id> <segIdx>	Usage: polyline_trim_segments <id> <segIdx>
用法: prop -r <名称>	Usage: prop -r <name>
用法: rect x0,y0 x1,y1	Usage: rect x0,y0 x1,y1
用法: rotate <angleDeg> [cx cy]	Usage: rotate <angleDeg> [cx cy]
用法: scale <factor> [cx cy]	Usage: scale <factor> [cx cy]
用法: select_layer <图层名>	Usage: select_layer <layer name>
用法: spline_append_pt <id> x,y	Usage: spline_append_pt <id> x,y
用法: spline_insert_pts <id> x,y	Usage: spline_insert_pts <id> x,y
用法: spline_remove_between <id> <i> <j>	Usage: spline_remove_between <id> <i> <j>
用法: spline_remove_pts <id> <index>（0 起）	Usage: spline_remove_pts <id> <index> (0-based)
用法: stretch <basex,basey> <tox,toy>	Usage: stretch <basex,basey> <tox,toy>
用法: trim <目标id> <点击点x,y> [边界id ...]（边界省略=全场景为界）	Usage: trim <target id> <click point x,y> [boundary id ...] (omit boundary = entire scene as boundary)
用法: ucs 3p <原点x,y> <X正向点x,y> [Y侧点x,y]	Usage: ucs 3p <origin x,y> <X direction point x,y> [Y side point x,y]
用法: ucs coords wcs|ucs	Usage: ucs coords wcs|ucs
用法: ucs delete <名称>	Usage: ucs delete <name>
用法: ucs object <图元id>	Usage: ucs object <entity id>
用法: ucs origin <x,y> [旋转角°]	Usage: ucs origin <x,y> [rotation angle°]
用法: ucs save <名称>	Usage: ucs save <name>
用法: ucs set <名称>	Usage: ucs set <name>
用法: ucs world|view|3p|object|z|origin|save|set|restore|delete|list|coords	Usage: ucs world|view|3p|object|z|origin|save|set|restore|delete|list|coords
用法: ucs z <原点x,y> <绕Z旋转角°>	Usage: ucs z <origin x,y> <Z rotation angle°>
用法: xline_angle x,y [角度]	Usage: xline_angle x,y [angle]
用法: xline_bisector refId1 refId2 x,y	Usage: xline_bisector refId1 refId2 x,y
用法: xline_h x,y	Usage: xline_h x,y
用法: xline_parallel refId x,y	Usage: xline_parallel refId x,y
用法: xline_v x,y	Usage: xline_v x,y
用法: xref <attach|detach|list|path> ...	Usage: xref <attach|detach|list|path> ...
用法: xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy]	Usage: xref attach <file path> [full|relative|none] [x,y] [rotation] [sx] [sy]
用法: xref detach <块名>	Usage: xref detach <block name>
用法: xref path <块名>	Usage: xref path <block name>
用法: zoom <倍率>	Usage: zoom <factor>
用法（Usage:）：%1	Usage: %1
用法：WELD <类型> [x,y] [k=.. s=.. l=.. e=.. n=..] [aux=.. supp=.. side=.. d=.. rot=..]	Usage: WELD <type> [x,y] [k=.. s=.. l=.. e=.. n=..] [aux=.. supp=.. side=.. d=.. rot=..]
用法：alias <别名> <完整命令>	Usage: alias <alias> <full command>
用法：aliasload <文件>	Usage: aliasload <file>
用法：constraint add %1 <idA> <idB> [值|轴id] [idxA] [idxB]	Usage: constraint add %1 <idA> <idB> [value|axis id] [idxA] [idxB]
用法：constraint add %1 <idA> [idB] [值|轴id] [idxA] [idxB]	Usage: constraint add %1 <idA> [idB] [value|axis id] [idxA] [idxB]
用法：constraint add <类型> <idA> <idB> [值|轴id] [idxA] [idxB]（fixed/fixedpoint 可省略 idB）	Usage: constraint add <type> <idA> <idB> [value|axis id] [idxA] [idxB] (idB can be omitted for fixed/fixedpoint)
用法：constraint add <类型> <idA> <idB> [值|轴id] | solve | list | check | diagnose | clear	Usage: constraint add <type> <idA> <idB> [value|axis id] | solve | list | check | diagnose | clear
用法：constraint dimension link <标注id> [参数名]	Usage: constraint dimension link <dimension id> [parameter name]
用法：constraint dimension link <标注id> [参数名] | setvalue <标注id> <值>	Usage: constraint dimension link <dimension id> [parameter name] | setvalue <dimension id> <value>
用法：constraint dimension setvalue <标注id> <值>	Usage: constraint dimension setvalue <dimension id> <value>
用法：constraint param set <名称> <值>	Usage: constraint param set <name> <value>
用法：constraint param set <名称> <值> | list	Usage: constraint param set <name> <value> | list
用法：cuiload <文件>	Usage: cuiload <file>
用法：ddedit <id> <新内容> 或选中文字后 ddedit <新内容>	Usage: ddedit <id> <new content> or, with text selected, ddedit <new content>
用法：dimedit <标注id> <新值> 或选中标注后 dimedit <新值>	Usage: dimedit <dimension id> <new value> or, with a dimension selected, dimedit <new value>
用法：dimstyle [list | new <名> [gbt|iso] | set <名> | rename <旧> <新> | apply <名> [id...] | delete <名>]	Usage: dimstyle [list | new <name> [gbt|iso] | set <name> | rename <old> <new> | apply <name> [id...] | delete <name>]
用法：dimstyle apply <样式名> [图元id...]	Usage: dimstyle apply <style name> [entity id...]
用法：dimstyle delete <名>	Usage: dimstyle delete <name>
用法：dimstyle new <名> [gbt|iso]	Usage: dimstyle new <name> [gbt|iso]
用法：dimstyle rename <旧名> <新名>	Usage: dimstyle rename <old name> <new name>
用法：dimstyle set <名>	Usage: dimstyle set <name>
用法：dxfout <路径.dxf> [-v R12|2000]	Usage: dxfout <path.dxf> [-v R12|2000]
用法：dyn [on|off]	Usage: dyn [on|off]
用法：filter [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型] | none	Usage: filter [type=type] [layer=layer] [color=#rrggbb] [linestyle=linetype] | none
用法：group [create <名> | add <名> | remove | list | ungroup <名>]	Usage: group [create <name> | add <name> | remove | list | ungroup <name>]
用法：group add <编组名>（先选中对象）	Usage: group add <group name> (select objects first)
用法：group create <编组名>（先选中对象）	Usage: group create <group name> (select objects first)
用法：group ungroup <编组名>（对象保留）	Usage: group ungroup <group name> (objects are kept)
用法：hatch [scale <倍数> | angle <度> | pattern <名|none>]（先选中闭合边界图元：圆/矩形/闭合折线等）	Usage: hatch [scale <factor> | angle <degrees> | pattern <name|none>] (first select closed boundary entities: circle/rectangle/closed polyline, etc.)
用法：hatchedit [scale <倍数> | pattern <名|none> | angle <度> | assoc on|off]（先选中剖面区域）	Usage: hatchedit [scale <factor> | pattern <name|none> | angle <degrees> | assoc on|off] (first select a section region)
用法：insert <块名> <x,y> [旋转度] [缩放X] [缩放Y]	Usage: insert <block name> <x,y> [rotation degrees] [scale X] [scale Y]
用法：laymch <目标图层名>	Usage: laymch <target layer name>
用法：layout copy <源布局> <新名>	Usage: layout copy <source layout> <new name>
用法：layout delete <布局名>	Usage: layout delete <layout name>
用法：layout new <布局名>	Usage: layout new <layout name>
用法：layout rename <旧名> <新名>	Usage: layout rename <old name> <new name>
用法：layout set <布局名|model>	Usage: layout set <layout name|model>
用法：layout vpfreeze <视口名> [图层名]（无图层名 = 列出冻结层；vpthaw 解冻）	Usage: layout vpfreeze <viewport name> [layer name] (no layer name = list frozen layers; vpthaw to thaw)
用法：layout vpscale <视口名> <比例>（如 1:2 或 0.5）	Usage: layout vpscale <viewport name> <scale> (e.g., 1:2 or 0.5)
用法：ltscale <正数>（如 0.5 / 1 / 2）	Usage: ltscale <positive number> (e.g., 0.5 / 1 / 2)
用法：macrosave <文件>（路径含空格用双引号包裹）	Usage: macrosave <file> (enclose paths containing spaces in double quotes)
用法：matchprop <源id> <目标id...>（把源图元图层/颜色/线宽/线型复制到目标）	Usage: matchprop <source id> <target id...> (copy the source entity's layer/color/lineweight/linetype to the targets)
用法：measure <id> <间距>	Usage: measure <id> <spacing>
用法：mtext <x,y> <字高> <内容...>	Usage: mtext <x,y> <text height> <content...>
用法：mview <图纸X> <图纸Y> <宽> <高> [比例]，如 mview 10 10 90 60 1:2	Usage: mview <drawing X> <drawing Y> <width> <height> [scale], e.g., mview 10 10 90 60 1:2
用法：ortho [on|off]	Usage: ortho [on|off]
用法：otrack [on|off]	Usage: otrack [on|off]
用法：pedit <id> [close|open|width <线宽>|join <id...>|fit|spline]	Usage: pedit <id> [close|open|width <lineweight>|join <id...>|fit|spline]
用法：perp <直线Id> <x,y>	Usage: perp <line Id> <x,y>
用法：point <x,y> [x,y ...]	Usage: point <x,y> [x,y ...]
用法：polar [on|off|<增量角>]	Usage: polar [on|off|<increment angle>]
用法：properties（先选中图元）或 properties <id...>	Usage: properties (select entities first) or properties <id...>
用法：purge [all|layers|blocks|dimstyles]	Usage: purge [all|layers|blocks|dimstyles]
用法：qconnect <目标圆Id> <x,y>	Usage: qconnect <target circle Id> <x,y>
用法：qdim [baseline|continue]（先框选一组平行线/圆阵列）	Usage: qdim [baseline|continue] (first window-select a group of parallel lines / circle arrays)
用法：sheetframe <A0|A1|A2|A3|A4> [portrait|landscape]	Usage: sheetframe <A0|A1|A2|A3|A4> [portrait|landscape]
用法：snap [on|off|<步长mm>]	Usage: snap [on|off|<step mm>]
用法：standards <表名> [<键名>]  例：standards sheet_sizes A3	Usage: standards <table name> [<key name>]  Example: standards sheet_sizes A3
用法：tangent2 <圆IdA> <圆IdB>	Usage: tangent2 <circle IdA> <circle IdB>
用法：tbadd <工具条> <命令> [文字] [图标]	Usage: tbadd <toolbar> <command> [text] [icon]
用法：tbdel <工具条>	Usage: tbdel <toolbar>
用法：tbnew <id> [标题]	Usage: tbnew <id> [title]
用法：tbrm <工具条> <序号>	Usage: tbrm <toolbar> <index>
用法：text <x,y> <字高> <内容...>	Usage: text <x,y> <text height> <content...>
用法：tk <x,y>（放置临时追踪点）；tkclear 清除全部	Usage: tk <x,y> (place a temporary tracking point); tkclear clears all
用法：tol <x,y> [公差文本]	Usage: tol <x,y> [tolerance text]
用法：wblock <输出.dxf> [id...]（不指定 id 则导出当前选择集）	Usage: wblock <output.dxf> [id...] (if no id is given, the current selection set is exported)
用法：wsdel <id>	Usage: wsdel <id>
用法：wsload <id>	Usage: wsload <id>
用法：wsnew <id> [标题]	Usage: wsnew <id> [title]
用自然语言下指令，例如：画一个圆心(0,0)半径50的圆	Give a natural-language command, e.g. draw a circle at (0,0) with radius 50
用选中对象创建编组...	Create Group from Selected Objects...
用选中边界填充(&B)...	Hatch with Selected Boundary(&B)...
由圆弧创建	Create from Arc
由多段线创建样条	Create Spline from Polyline
由多段线创建样条：选择折线/多边形/矩形转换为样条	Create spline from polyline: select a polyline/polygon/rectangle to convert to a spline
界面(&U)	&Interface
界面主题：	Interface Theme:
界面语言：	UI Language:
留空 = 当前选区；或填 ID 如 3 5 7	Blank = current selection; or enter IDs such as 3 5 7
留空则使用默认：~/SketchCAD/sketchcad_drawings.db	If blank, use the default: ~/SketchCAD/sketchcad_drawings.db
登录用户名，如 root	Login username, e.g., root
目标	Target
目标 #%1 不是尺寸标注	Target #%1 is not a dimension
目标 #%1 不是文字/多行文字	Target #%1 is not text/multiline text
目标 X	Target X
目标 Y	Target Y
目标不是折线	Target is not a polyline
目标图层 %1 不存在	Target layer %1 does not exist
目标折线已闭合，请先打开再合并	Target polyline is already closed; open it before joining
直径(mm)：	Diameter (mm):
直径标注	Diameter Dimension
直径标注=Diameter annotation	Diameter dimension =Diameter annotation
直径标注：点选圆/圆弧，尺寸线通过圆心（⌀ 前缀）	Diameter dimension: click a circle/arc; the dimension line passes through the center (⌀ prefix)
直接打印 (模型空间)	Plot Directly (Model Space)
直线	Line
直线 (两点)：连续模式下右键结束	Line (2 Points): right-click to finish in continuous mode
直线=Line	Line =Line
直线绘制方式	Line Drawing Method
直线链模式（%1 条边围成多边形）：面积 = %2 mm²    周长 = %3 mm    形心 = (%4, %5)	Line chain mode (%1 edges enclosing a polygon): area = %2 mm²    perimeter = %3 mm    centroid = (%4, %5)
直线链：	Line Chain:
直线长度为 0	Line length is 0
相关命令：%1	Related Commands: %1
相切	Tangent
相切=Tangent	Tangent =Tangent
相切一图元+两点	Tangent to 1 entity + 2 points
相切三图元	Tangent to 3 entities
相切两图元+半径	Tangent to 2 entities + radius
相切两图元+半径：拾取两图元，左侧输入半径，点击选解	Tangent to 2 entities + radius: pick two entities, enter the radius on the left, click to select a solution
相切两图元+点	Tangent to 2 entities + point
相切延续弧	Tangent Continuation Arc
相切延续弧：选择参考图元，再点起点和终点	Tangent continuation arc: select a reference entity, then click the start and end points
相对参考线角度	Angle relative to reference line
相对角度线	Line at relative angle
相对路径（本图未存盘 → 退化为文件名）	Relative path (drawing not saved -> falls back to the file name)
相对路径（相对本图所在目录）	Relative path (relative to the drawing's directory)
相等=Equal	Equal =Equal
矩形	Rectangle
矩形=Rectangle	Rectangle =Rectangle
矩形阵列	Rectangular Array
矩形阵列 %1×%2（间距 %3/%4 mm）：新增 %5 个副本	Rectangular array %1x%2 (spacing %3/%4 mm): %5 copies added
矩形阵列...	Rectangular Array...
矩形阵列：%1 个源图元 → 新增 %2 个副本（%3×%4）	Rectangular array: %1 source entities -> %2 copies added (%3x%4)
确定	OK
确定丢弃全部 %1 份自动保存快照？\n此操作不可撤销。	Are you sure you want to discard all %1 autosave snapshots?\nThis operation cannot be undone.
确定丢弃该自动保存快照？\n此操作不可撤销。	Are you sure you want to discard this autosave snapshot?\nThis operation cannot be undone.
确定从数据库中删除该图档？此操作不可撤销。	Are you sure you want to delete this drawing archive from the database? This operation cannot be undone.
确定从数据库中删除该模板？此操作不可撤销。	Are you sure you want to delete this template from the database? This operation cannot be undone.
确定分离外部参照「%1」？它的全部参照实例将一并删除（可撤销）。	Are you sure you want to detach external reference '%1'? All of its reference instances will be deleted as well (undoable).
确定删除块「%1」？所有引用将退化为未定义（不绘制）。	Are you sure you want to delete block '%1'? All references will become undefined (not drawn).
确认丢弃	Confirm Discard
确认全部丢弃	Confirm Discard All
确认删除区域「%1」的 %2 个边界图元？	Confirm deleting %2 boundary entities of region '%1'?
确认后将以所选幅面为最小尺寸自动套框：图框会包住全部已有图元，标题栏固定在右下角并自动避让，不覆盖任何现有图元。	After confirmation, a drawing frame is automatically applied using the selected sheet size as the minimum: the frame encloses all existing entities, and the title block is anchored at the bottom right with automatic avoidance, covering no existing entities.
确认清空全部变量定义？	Confirm clearing all variable definitions?
磁盘剩余告警：	Free Disk Space Warning:
磁盘剩余空间低于该值时，自动保存前提前告警	When free disk space falls below this value, a warning is given before autosaving
磁盘告警阈值：	Disk Warning Threshold:
磁盘空间不足	Insufficient Disk Space
离线模拟（无需网络/密钥，规则联调）	Offline Simulation (no network/key needed, rule integration testing)
移入图层 	Move to Layer 
移出编组	Remove from Group
移到开头	Move to Start
移到末尾	Move to End
移动	Move
移动 / 复制...	Move / Copy...
移动并旋转...	Move and Rotate...
移动的复制版：原对象保留	Move (copy): the original objects are kept
移除选中对象	Remove Selected Objects
空 = 不标	Empty = no dimension
空 = 不注公差	Empty = no tolerance
空列表 = 模型空间打印（整体图形直接出图，无需切换布局）。下列每个视口将其区域映射到整块可打印区。	Empty list = plot from model space (the whole drawing is plotted directly, no need to switch layouts). Each viewport below maps its area to the entire printable area.
空白图	Blank Drawing
窗口取消	Window Deselect
窗口取消：拖拽矩形取消框内图元的选中	Window Deselect: drag a rectangle to deselect the entities inside the box
窗口缩放	Window Zoom
窗口缩放：拖拽框选目标区域，释放后该区域充满整个绘图工作区域 (Zoom to Box)	Window Zoom: drag a box over the target area; on release, that area fills the entire drawing work area (Zoom to Box)
窗口选择	Window Select
窗口选择：拖拽矩形框选内部图元（Ctrl 加选）	Window Select: drag a rectangle to select entities inside the box (Ctrl to add to selection)
竖直=Vertical	Vertical =Vertical
竖直构造线	Vertical Construction Line
竖直标注	Vertical Dimension
竖直标注=Vertical annotation	Vertical =Vertical annotation
竖直标注：只量 Y 方向投影	Vertical dimension: measures only the Y-direction projection
竖直线	Vertical Line
竖置	Portrait
端口：	Port:
符号标注：已插入 %1（%2 个图元）	Symbol annotation: inserted %1 (%2 entities)
符号标注：未知符号 %1	Symbol annotation: unknown symbol %1
符号标注：生成失败（%1）	Symbol annotation: generation failed (%1)
第一孔起始角	First Hole Start Angle
第一段长(mm)：	First Segment Length (mm):
第一角画法（国标）	First-angle projection (GB standard)
第三角画法（ISO）	Third-angle projection (ISO)
第二段长(mm)：	Second Segment Length (mm):
等分...	Divide...
等分失败	Division failed
等分需要 1 条直线	Divide requires 1 line
等半径=EqualRadius	Equal radius =EqualRadius
等长=EqualLength	Equal length =EqualLength
筒体外径 Ø	Cylinder outer diameter Ø
筛选范围可选整个图形或当前选择集；结果以 替换/追加/排除 三种方式\n落地到选择集，与点击选择/框选共用同一选择集状态。	Filter scope can be the entire drawing or the current selection set; results are applied to the selection set in three modes: replace / append / exclude,\nsharing the same selection state as click selection / window selection.
签名表未匹配到图纸标题栏字段	Signature table did not match any title block fields
简介：%1	Description: %1
简体中文	Simplified Chinese
管理模板	Manage Templates
箭头侧（基准线下方）	Arrow side (below the datum line)
箭头侧：	Arrow Side:
箱体视图...	Housing View...
箱体视图（主视图全剖 + 俯视图 B + A—A 环形断面 + C 向视图；\n含剖切符号、表面粗糙度与技术要求）。单位：毫米（mm）。\n默认：筒体 Ø82 / Ø64H8，底板 46×70×7，总高 70，凸台 Ø34/Ø26。	Housing view (front view fully sectioned + top view B + A-A ring section + view in direction C;\nincludes section symbols, surface roughness and technical requirements). Units: millimeters (mm).\nDefault: cylinder Ø82 / Ø64H8, base plate 46x70x7, overall height 70, boss Ø34/Ø26.
类别：	Category:
类型	Type
类型：	Type:
类型：fixed/fixedpoint/horizontal/vertical/parallel/perpendicular/tangent/...	Type: fixed/fixedpoint/horizontal/vertical/parallel/perpendicular/tangent/...
粗体	Bold
粗点画线=Thick dash dot	Thick dash-dot =Thick dash dot
粗糙度	Surface Roughness
粗糙度标注	Surface Roughness Annotation
粗线 1.4mm	1.4 mm thick
粗线 3px=Thick solid	Thick line 3px=Thick solid
粗虚线=Thick dashed	Thick dashed =Thick dashed
粘贴	Paste
粘贴(&P)	Paste(&P)
粘贴为块	Paste as Block
粘贴为块(&K)...	Paste as Block(&K)...
粘贴为块：请单击指定插入点	Paste as Block: click to specify the insertion point
粘贴失败	Paste Failed
粘贴：请单击指定落点（剪贴板基点将对齐该点）	Paste: click to specify the drop point (the clipboard base point will align to it)
索引无效	Invalid Index
索引无效或两点相邻（无需删除中间段）	Invalid index or the two points are adjacent (no intermediate segment to delete)
索引越界	Index out of range
约束	Constraint
约束=Constraint	Constraint =Constraint
约束栏	Constraint Bar
约束栏（全部约束，共 %1 条）	Constraint bar (all constraints, %1 total)
约束栏（选中 #%1 的约束，共 %2 条）	Constraint bar (selected #%1, %2 total)
约束栏（选中对象的约束）	Constraint Bar (constraints of selected objects)
约束求解「%1」异常，已回落修复模型：%2	Constraint solving '%1' raised an exception; fell back to repair the model: %2
约束求解：共 %1 条，已满足 %2 条，失败 %3 条；自由度(DOF)=%4，有效约束秩=%5，残差=%.3e	Constraint solving: %1 total, %2 satisfied, %3 failed; degrees of freedom (DOF)=%4, effective constraint rank=%5, residual=%.3e
约束添加失败：%1	Failed to add constraint: %1
约束诊断：%1（总自由度=%2，被约束图元剩余=%3，冲突/冗余=%4 条）	Constraint diagnostics: %1 (total DOF=%2, remaining constrained entities=%3, conflicts/redundancies=%4)
约束诊断：%1（涉及图元 #%2）	Constraint diagnostics: %1 (entity #%2 involved)
约束：已选中 #%1，请拾取第 2 个图元	Constraint: #%1 selected, pick the 2nd entity
约束：请点选图元	Constraint: pick an entity
约束：请选择不同的第 2 个图元	Constraint: select a different 2nd entity
纵向	Portrait
纵向（长边垂直）	Portrait (long edge vertical)
纸张尺寸	Paper Size
线型	Linetype
线型(&L)...	Linetype(&L)...
线型比例	Linetype Scale
线型的加载/设置/列表	Load / set / list linetypes
线型表（* = 当前）：\n%1	Linetype Table (* = Current):\n%1
线型：	Line style:
线型：未选中图元时设定新图元线型；已选中时同时修改选中图元	Linetype: with no entities selected, sets the linetype for new entities; with entities selected, also modifies the selected entities
线宽	Lineweight
线宽 (mm/px):	Lineweight (mm/px):
线宽无效（取 1 / 2 / 3 / 5）	Invalid lineweight (use 1 / 2 / 3 / 5)
线宽显示：按图元实际线宽渲染（关=一律细线）	Lineweight Display: entity lineweight (= thin line)
线宽随图框比例调整	Lineweight adjusts with the drawing frame scale
线宽须为非负数	Lineweight must be non-negative
线宽（细线 1px / 中粗线 2px / 粗线 3px / 特粗线 5px）：未选中图元时设定新图元线宽；已选中时同时修改选中图元	Lineweight (thin 1px / medium 2px / thick 3px / extra thick 5px): with no entities selected, sets the lineweight for new entities; with entities selected, also modifies the selected entities
线宽：	Line width:
线性/对齐/基线/连续/角度/半径/直径/引线/坐标标注（GB/T 4458.4-2003）	Linear/aligned/baseline/continue/angular/radius/diameter/leader/ordinate dimensions (GB/T 4458.4-2003)
线性标注	Linear Dimension
线性标注=Linear annotation	Linear =Linear annotation
线性标注=Linearity	Linear =Linearity
线段 1 ID	Segment 1 ID
线段 2 ID	Segment 2 ID
线段长度为零，已忽略	Zero-length segment, ignored
线镜像	Line Mirror
线镜像：已拾取第一点，请拾取第二点定义镜像轴	Line Mirror: first point picked, pick the second point to define the mirror axis
线镜像：请依次单击两点定义镜像轴（先第一点，后第二点）	Line Mirror: click two points in order to define the mirror axis (first point, then second point)
线间隙	Line Gap
线间隙...	Line Gap...
线间隙打断失败	Line Gap Break Failed
线间隙需要 1 条直线	Line gap requires 1 line
组合方式：	Combination Mode:
细线 0.5mm	0.5 mm thin
细线 1px=One step thin	Thin line 1px=One step thin
终点 X	End X
终点 Y	End Y
结束宏录制	Stop Macro Recording
结论：当前图纸线宽%1（R1_line_width_ratio，%2 项）。	Conclusion: current drawing lineweight %1 (R1_line_width_ratio, %2 items).
绕基点旋转选中图元	Rotate selected entities about a base point
绕基点缩放选中图元	Scale selected entities about a base point
绘制单个或多个点	Draw One or More Points
绘制可打印区边框	Draw printable area border
绘制四周尺寸链	Draw perimeter dimension chains
绘制家具图块	Draw furniture blocks
绘制户型平面图	Draw Floor Plan
绘制房间名标注	Draw room name annotations
绘制法兰盘三面图	Draw flange three-view drawing
绘制法兰盘轮毂视图	Draw Flange Hub View
绘制箱体视图	Draw housing view
绘制轴承座三面图	Draw bearing housing three-view drawing
绘制飞机三面图	Draw Aircraft Three-View
绘图	Draw
绘图(&D)	&Draw
绘图比例	Drawing Scale
绘图比例 (GB/T 14690)	Drawing Scale (GB/T 14690)
绘图比例...	Drawing Scale...
绘图比例:	Drawing Scale:
绘图记录...	Draw Log...
给定一条边生成正多边形（边起终点 + 边数）	Create a regular polygon from one edge (edge start/end points + number of sides)
统一覆盖纸张/方向	Override Paper Size / Orientation for All
继续上次工作	Continue Previous Session
继续使用空白图	Continue with Blank Drawing
编号	No.
编组	Group
编组 = 名称相同的对象集合。选中若干对象后可创建编组或加入既有编组；\n双击编组名 = 整体选中。编组关系随图存档，可整组撤销。	Group = a set of objects sharing the same name. After selecting several objects you can create a group or join an existing group;\ndouble-click a group name = select the whole group. Group relationships are saved with the drawing and can be undone as a whole group.
编组%1	Group %1
编组「%1」不存在	Group '%1' does not exist
编组「%1」整体选中：%2 个对象（Ctrl+点击可穿透编组单选）	Group '%1' selected as a whole: %2 objects (Ctrl+click to pick an individual object through the group)
编组「%1」：%2 个对象	Group '%1': %2 objects
编组名：	Group Name:
编组的创建/加成员/移除/列表/解组	Group create / add members / remove / list / ungroup
编组管理(&G)...	Group Manager(&G)...
编组管理器 (GROUP)	Group Manager (GROUP)
编辑	Edit
编辑(&E)	&Edit
编辑(联动)	Edit (linked)
编辑命令别名文件...	Edit Command Alias File...
编辑块引用的属性值	Edit Block Reference Attribute Values
编辑尺寸	Edit Dimension
编辑属性	Edit Properties
编辑文字	Edit Text
编辑文字(&E)...	Edit Text(&E)...
编辑标注数值(仅显示)	Edit dimension value (display only)
编辑标注文字	Edit Dimension Text
编辑比例...	Edit Scale...
编辑表格	Edit Table
缩小	Zoom Out
缩小 (Zoom Out)	Zoom Out (Zoom Out)
缩放	Scale
缩放...	Scale...
缩放X无效	Invalid scale X
缩放Y无效	Invalid scale Y
缩放中心 X	Scale Center X
缩放中心 Y	Scale Center Y
缩放因子必须 > 0	Scale factor must be > 0
缩放因子（>0）	Scale factor (>0)
缩放范围	Scale Range
缺口宽度 (mm)：	Gap Width (mm):
缺口宽度不能大于线长	Gap width cannot be greater than the line length
缺少 path 参数	Missing path parameter
缺少 tasks 参数	Missing tasks parameter
缺少数量参数 <count>	Missing count parameter <count>
缺少版本号（非本应用内容）	Missing version number (not created by this application)
网格	Grid
翼展	Wingspan
脚本	Script
脚本 (*.js *.py *.vbs);;JavaScript (*.js);;Python (*.py);;VBA/VBScript (*.vbs);;所有文件 (*.*)	Scripts (*.js *.py *.vbs);;JavaScript (*.js);;Python (*.py);;VBA/VBScript (*.vbs);;All Files (*.*)
脚本(&S)	Scri&pt
脚本执行完成：%1	Script execution completed: %1
脚本文件 (*.scr);;所有文件 (*.*)	Script Files (*.scr);;All Files (*.*)
脚本输出：%1	Script output: %1
脚本错误	Script Error
自动保存	Autosave
自动保存:—	Autosave: -
自动保存:关	Autosave: Off
自动保存会在异常退出（崩溃 / 断电）后于下次启动提示恢复，\n不会覆盖你的原图纸文件。	Autosave will prompt for recovery on the next launch after an abnormal exit (crash / power loss);\nIt will not overwrite your original drawing file.
自动保存失败	Autosave Failed
自动保存失败：\n%1\n\n图纸仍在内存中，请尽快手动「文件 → 保存...」。	Autosave failed:\n%1\n\nThe drawing is still in memory; please use 'File -> Save...' manually as soon as possible.
自动保存失败：%1	Autosave failed: %1
自动保存当前处于关闭状态。\n可在「文件 → 自动保存设置...」中开启。	Autosave is currently off.\nEnable it under 'File -> Autosave Settings...'.
自动保存设置	Autosave Settings
自动保存设置...	Autosave Settings...
自动保存间隔：	Autosave Interval:
自动保存间隔：%1	Autosave Interval: %1
自动化导入	Automated Import
自动化打开	Automated Open
自动区分相邻区域（方向相反 / 间隔不等，4.7）	Automatically distinguish adjacent regions (opposite directions / unequal spacing, 4.7)
自动填写日期	Auto-fill Date
自动换行宽度：	Auto Wrap Width:
自动排图	Automatic Sheet Layout
自动排图：%1 个图框，利用率 %2%	Automatic sheet layout: %1 drawing frames, utilization %2%
自动选取标准比例	Auto-select Standard Scale
自定义	Custom
自定义 / 已导入 CTB	Custom / Imported CTB
自定义...	Custom...
自定义图纸特性的设置/查询/删除（PROP 域数据源）	Set/query/delete custom drawing properties (PROP field data source)
自定义比例	Custom Scale
自定义特性共 %1 项：	Custom properties: %1
自定义颜色...	Custom Color...
自然语言助手会复用现有命令体系（与命令行同一套语法）执行你的意图。\n云服务会把你的绘图指令文本发送给所选 LLM 服务商；离线模拟（mock）完全在本机运行、不上传任何数据。\n修改「后端类型 / 服务地址 / 密钥 / 模型」后点「应用」或「确定」立即生效。	The natural language assistant reuses the existing command system (same syntax as the command line) to execute your intent.\nThe cloud service sends your drawing command text to the selected LLM provider; offline simulation (mock) runs entirely on this machine and uploads no data.\nAfter changing 'Backend Type / Service Address / Key / Model', click 'Apply' or 'OK' to take effect immediately.
自由手绘	Freehand
自由手绘：按住左键移动绘制，松开结束	Freehand: hold the left button and drag to draw; release to finish
自适应铺满可打印区(优先)	Fit to Printable Area (preferred)
至少 2 个选中（1 参考 + 1+ 待对齐）	At least 2 selected (1 reference + 1+ to align)
至少需保留一个文字样式。	At least one text style must be kept.
至少需保留一个文字样式，已忽略删除	At least one text style must be kept, deletion ignored
至少需要 2 个选中图元（1 参考 + 1+ 待对齐）	At least 2 selected entities are required (1 reference + 1+ to align)
节点不足，无法修剪	Insufficient nodes, cannot trim
节点索引无效	Invalid node index
草图与图形 (*.json *.dxf *.dwg);;所有文件 (*)	Sketches and Drawings (*.json *.dxf *.dwg);;All Files (*)
草图与图形 (*.json *.dxf *.dwg);;草图文件 (*.json);;AutoCAD DXF (*.dxf);;AutoCAD (*.dwg);;所有文件 (*)	Sketches and Drawings (*.json *.dxf *.dwg);;Sketch Files (*.json);;AutoCAD DXF (*.dxf);;AutoCAD (*.dwg);;All Files (*)
草图文件 (*.json)	Sketch Files (*.json)
草图文件 (*.json);;所有文件 (*)	Sketch Files (*.json);;All Files (*)
草图模型	Sketch Model
草图模型=Sketch model	Sketch model =Sketch model
草图约束	Sketch Constraints
落点解析失败	Failed to parse the drop point
虚线=Dashed line	Dashed =Dashed line
螺栓分布圆 Ø	Bolt circle Ø
螺栓孔分布圆 K	Bolt hole circle K
螺栓孔径 L	Bolt Hole Diameter L
螺栓孔径 Ø	Bolt hole diameter Ø
螺栓孔数量	Bolt hole count
螺栓孔数量 n	Bolt hole count n
行数	Rows
行数 / 列数至少为 1。	Rows / columns must be at least 1.
行数：	Rows:
行距因子：	Row Spacing Factor:
行间距	Row Spacing
行高：	Row Height:
补充符号：	Supplementary Symbol:
表格	Table
表格 (TABLE)：点击放置表格左上角后编辑行列、列宽、行高、字高与单元格内容（明细表）	Table (TABLE): click to place the table's upper-left corner, then edit rows and columns, column width, row height, text height and cell contents (BOM table)
表格(TABLE)：点击放置表格左上角后编辑行列、列宽、行高、字高与单元格内容（明细表）	Table(TABLE): click to place the table's upper-left corner, then edit rows and columns, column width, row height, text height and cell contents (BOM table)
表格=Table	Table =Table
表格编辑器 (TABLE)	Table Editor (TABLE)
表达式 / 值	Expression / Value
表达式 / 数值：	Expression / Value:
表达式嵌套/连续一元号过深	Expression nesting / consecutive unary signs too deep
表达式引用未定义变量「%1」	Expression references undefined variable '%1'
表达式意外结束（缺少运算数）	Expression ended unexpectedly (missing operand)
表达式括号嵌套过深	Expression bracket nesting too deep
表达式数字格式错误	Invalid number format in expression
表达式缺少右括号	Expression is missing a closing bracket
表达式语法错误：无法识别的字符「%1」	Expression syntax error: unrecognized character \"%1\"
表达式除零（除数绝对值为 0）	Division by zero in expression (divisor is 0)
装订宽度	Binding Width
装订线	Binding Margin
覆盖 GB/T 14665/14689/131/324 与 GB/T 18617/ISO 13567；仅报告不合规项、建议与跳转，不强制修改	Covers GB/T 14665/14689/131/324 and GB/T 18617/ISO 13567; only reports non-compliant items, suggestions and jump links, without forcing changes
覆盖文本（留空恢复默认内容）：	Override text (leave blank to restore default content):
覆盖线宽	Override Lineweight
覆盖颜色	Override Color
视口 / 布局 (MVP)	Viewport / Layout (MVP)
视口「%1」冻结图层：%2	Viewport '%1' frozen layers: %2
视口「%1」已冻结图层「%2」（仅该视口出图隐藏）	Viewport '%1' froze layer '%2' (hidden in this viewport's plot only)
视口「%1」已解冻图层「%2」	Viewport '%1' thawed layer '%2'
视口「%1」未冻结任何图层	Viewport '%1' has no frozen layers
视口「%1」比例 = %2（模型 mm → 图纸 mm）	Viewport '%1' scale = %2 (model mm -> drawing mm)
视口内容旋转角（度，逆时针；0 = 不旋转）:	Viewport content rotation angle (degrees, counterclockwise; 0 = no rotation):
视口冻结图层 - %1	Viewport Frozen Layers - %1
视口宽度必须为正数	Viewport width must be positive
视口旋转	Viewport Rotation
视口比例	Viewport Scale
视口高度必须为正数	Viewport height must be positive
视图	View
视图(&V)	&View
视图字母：	View Letter:
视图平面	View Plane
视图方向	View Direction
视图比例: 	View Scale: 
角平分线	Angle Bisector
角平分线构造线	Angle Bisector Construction Line
角度	Angle
角度=Angle	Angle =Angle
角度为度，逆时针为正	Angles in degrees, counterclockwise positive
角度单位（显示/输入）：	Angle Units (Display/Input):
角度文本：	Angle Text:
角度无效	Invalid Angle
角度构造线	Angle Construction Line
角度构造线：点击基点即按「角度 + 长度」方向无限延伸	Angle construction line: click a base point to extend infinitely in the 'angle + length' direction
角度标注	Angular Dimension
角度标注=Angle annotation	Angular =Angle annotation
角度标注：角顶点 + 两条边上各一点，数字一律水平书写	Angular dimension: angle vertex + one point on each of the two sides; the value is always written horizontally
角度直线	Line at Angle
角度直线：点击起点即按「角度 + 长度」绘制	Line at angle: click the start point to draw by 'angle + length'
角度直线：长度无效，请在参数输入条输入长度	Line at angle: invalid length, enter the length in the parameter input bar
角度精度：	Angle Precision:
角度范围	Angle Range
角度范围需为 0°~360° 之间的正值。	Angle must be a positive value between 0° and 360°.
角度解析失败: 	Angle parse failed: 
角度须为数值	Angle must be a number
角度（度，CCW 为正）	Angle (degrees, CCW positive)
解散编组	Ungroup
解散编组 %1	Ungroup %1
解散编组「%1」？对象本身保留，仅解除编组关系。	Ungroup '%1'? The objects themselves are kept; only the group relationship is removed.
警告=Warning	Warning =Warning
计算(C)<	Compute(C)<
计算失败（退化截面）	Computation failed (degenerate section)
计算点列多边形	Compute polygon from point list
计算质量特性	Compute Mass Properties
计算距离	Compute Distance
计算选中/指定图元	Compute selected / specified entities
设为预选器	Set as Preselector
设为预选器（下一次拾取生效）	Set as Preselection (effective on next pick)
设值	Set Value
设值...	Set Value...
设置全局线型比例（单一 LTSCALE）	Set Global Linetype Scale (single LTSCALE)
设置动态参数 	Set Dynamic Parameters 
设置后续绘制线宽档位	Set Lineweight for Subsequent Drawing
设置后续绘制颜色	Set Color for Subsequent Drawing
设置图层透明度 	Set Layer Transparency 
设置块引用的动态参数（数值或可见性状态）	Set dynamic parameters of the block reference (numeric value or visibility state)
设置折线统一线宽（PEDIT Width）	Set uniform lineweight for polyline (PEDIT Width)
设置标注值失败：%1	Failed to set dimension value: %1
设置线宽	Set Lineweight
设计中心	Design Center
设计单位全称	Design Organization Full Name
设计单位（标识）	Design Organization (Identifier)
诊断包	Diagnostic Package
诊断包只生成为本机 ZIP 文件（用户目录），本程序不会自动上传任何数据。	The diagnostic package is generated only as a local ZIP file (in the user directory); this program does not automatically upload any data.
诊断包导出失败：\n%1	Failed to export diagnostic package:\n%1
诊断包已导出到：\n%1\n\n请把该文件发送给技术支持以协助定位问题。	Diagnostic package exported to:\n%1\n\nPlease send this file to technical support to help locate the issue.
诊断包帮助开发者复现问题。包内包含：\n	The diagnostic package helps developers reproduce issues. It contains:\n
该命令暂无详细帮助；输入 help 查看全部命令总表。	No detailed help for this command. Type \"help\" for the full command list.
该图元长度不足以按间距 %1 等分	The entity is not long enough to be divided at spacing %1
该文件当前不可用：\n%1	This file is currently unavailable:\n%1
该标注类型不支持尺寸驱动（支持：对齐/距离/水平/竖直/半径/直径/角度）	This dimension type does not support dimension driving (Supported: Aligned/Distance/Horizontal/Vertical/Radius/Diameter/Angular)
该标注类型暂不支持驱动几何	This dimension type does not yet support driving geometry
该视口的模型mm→图纸mm比例 (1=1:1):	Model mm → drawing mm scale for this viewport (1=1:1):
语言切换立即生效，无需重启。	The language change takes effect immediately, no restart required.
语言切换需要重新启动程序后完全生效。	Toggle requires start in effect.
说明：以原图元为 (0,0)，向右/向上阵列；原图元保留。	Note: with the original entity as (0,0), array to the right/up; the original entity is retained.
说明：绕中心点均匀旋转复制；原图元保留。	Note: rotate and copy evenly around the center point; the original entity is retained.
请先在列表中选择目标编组。	Please select the target group in the list first.
请先在图面选中若干「边界」图元（用边界工具绘制的），再归为同一区域。	Please first select several \"boundary\" entities in the drawing area (drawn with the boundary tool), then group them into the same region.
请先在绘图区选中要加入的对象。	Please first select the objects to add in the drawing area.
请先在绘图区选中要移出的对象。	Please first select the objects to remove in the drawing area.
请先在绘图区选中要编组的对象。	Please first select the objects to group in the drawing area.
请先在编辑区点选要删除的图元。	Please first click the entities to delete in the edit area.
请先在表中选中一个变量	Please select a variable in the table first
请先指定输出 PDF 路径。	Please specify the output PDF path first.
请先指定输出文件基名。	Please specify the output file base name first.
请先添加至少一条有效条件（特性不为「不限」）。	Please add at least one valid condition first (property not set to \"Any\").
请先选中一个参考图元（以其线型为匹配条件）	Please first select a reference entity (using its linetype as the match condition)
请先选中一个参考图元（以其颜色为匹配条件）	Please first select a reference entity (using its color as the match condition)
请先选中至少一个剖面区域（填充）	Please select at least one section region (hatch) first
请先选中至少一个块引用（已插入的块），再执行「分解块」。	Please select at least one block reference (inserted block) first, then run \"Explode Block\".
请先选中要填充的图元（边界/闭合形状），再执行「用选中边界填充」	Please select the entities to hatch (boundaries/closed shapes) first, then run \"Hatch with Selected Boundaries\"
请先选中要收进块「%1」的新图元。	Please select the new entities to add to block \"%1\" first.
请先选中要收进块的图元。	Please select the entities to include in the block first.
请先选中要收进块的图元，再执行「创建块」。	Please select the entities to include in the block first, then run \"Create Block\".
请先选中要连合的 Line/Arc	Please select the Line/Arc entities to join first
请先选中要镜像的图元	Please select the entities to mirror first
请先选中要阵列的一个或多个图元。	Please select one or more entities to array first.
请先选择作为新块内几何的图元	Please select the entities to serve as geometry in the new block first
请先选择线型	Please select a linetype first
请先选择要匹配的颜色	Please select the color to match first
请先选择要收进块的图元	Please select the entities to include in the block first
请填写图层名	Please enter a layer name
请拾取合适的参考图元	Please pick a suitable reference entity
请拾取合适的参考直线/矩形边	Please pick a suitable reference line / rectangle edge
请输入%1	Enter %1
请输入「宽,高」两个数值，例如 420,297。	Please enter two values, \"width,height\", e.g. 420,297.
请输入有效数值（如 1.6）	Please enter a valid value (e.g. 1.6)
请输入过滤器名称。	Please enter a filter name.
请选中一条直线	Please select a line
请选择参照图形文件。	Please select a reference drawing file.
请选择同一折线上的两个节点	Please select two vertices on the same polyline
请选择同一折线上的两段	Please select two segments on the same polyline
请选择同一根样条上的两个控制点	Please select two control points on the same spline
请选择折线	Please select a polyline
请选择折线、多边形或矩形	Please select a polyline, polygon, or rectangle
请选择样条曲线	Please select a spline
请选择目标折线	Please select the target polyline
请选择直线/圆弧/折线	Please select a line/arc/polyline
请选择线段、圆弧或折线	Please select a line segment, arc, or polyline
请选择镜像基准：	Please select the mirror axis:
读取/写出 R2000+ DWG 需要 LibreDWG 转换组件（dwg2dxf / dwgwrite，GPLv3，以独立进程运行，主程序不链接其代码）。\n安装包已在 tools/dwg/ 内置；如需自定义位置，在此指定（覆盖环境变量 SKETCHCAD_DWGTOOLS）。\n留空则自动查找程序目录 tools/dwg/ 或环境变量。	Reading/writing R2000+ DWG requires the LibreDWG conversion components (dwg2dxf / dwgwrite, GPLv3, run as separate processes; the main program does not link their code).\nThe installer bundles them in tools/dwg/; to use a custom location, specify it here (overrides the SKETCHCAD_DWGTOOLS environment variable).\nIf blank, the program directory tools/dwg/ or the environment variable is located automatically.
读取图档失败：	Failed to read drawing file:
读取模板失败：	Failed to read template:
豁免类别	Exempt Category
象限连接	Quadrant Connection
质量特性	Mass Properties
质量特性：无目标（先选中图元或填入 ID）	Mass Properties: no target (select entities or enter an ID first)
起点 X	Start Point X
起点 Y	Start Point Y
起终点+半径	Start/End + Radius
起终点+半径：点击两端点，左侧输入半径，光标选近解	Start/End + Radius: click the two endpoints, enter the radius on the left, and use the cursor to pick the near solution
起终点+夹角	Start/End + Included Angle
起终点+夹角：点击两端点，左侧输入圆心角	Start/End + Included Angle: click the two endpoints, enter the central angle on the left
起终点+弦长	Start/End + Chord Length
起终点+弦长（弧长）：点击两端点，左侧输入弧长	Start/End + Chord Length (Arc Length): click the two endpoints, enter the arc length on the left
起终点+弦高	Start/End + Chord Height
起终点+弦高(凸度)：点击两端点和凸度控制点	Start/End + Chord Height (Bulge): click the two endpoints and the bulge control point
距离	Distance
距离 = %1 mm    ΔX = %2    ΔY = %3    XY平面角 = %4°	Distance = %1 mm    ΔX = %2    ΔY = %3    XY Plane Angle = %4°
距离=Distance	Distance =Distance
距离无效	Invalid Distance
距离：点解析失败（应为数字）	Distance: point parsing failed (expected a number)
跟随系统	Follow System
路径 id 无效	Invalid path id
路径不存在	Path does not exist
路径仅支持折线（Polyline）或样条（Spline）	Path supports only Polyline or Spline
路径图元不存在	Path entity does not exist
路径类型：	Path Type:
路径退化或数量不足，未生成副本	Path degenerated or insufficient count; no copies generated
路径长度小于间距，放不下第二个项目（未生成副本）	Path length is less than the spacing; the second item does not fit (no copies generated)
路径阵列	Path Array
路径阵列...	Path Array...
路径阵列沿 #%1：%2 个位置点，新增 %3 个副本	Path Array along #%1: %2 position points, %3 copies added
路径阵列退化：路径太短或间距大于路径全长	Path Array degenerated: path too short or spacing greater than total path length
路径阵列需要选区同时含 1 条 Polyline/Spline（路径）与待阵列图元	Path Array requires the selection to contain 1 Polyline/Spline (path) and the entities to array
路径阵列：%1 个源图元 → 新增 %2 个副本（间距 %3 mm，路径 #%4）	Path Array: %1 source entities → %2 copies added (spacing %3 mm, path #%4)
转角(度)：	Rotation Angle (deg):
转角投影	Revolved Projection
转角投影：生成失败	Rotation Projection: generation failed
轮毂外径 Ø	Hub Outer Diameter Ø
轮毂左端倒角 C	Hub Left End Chamfer C
轴向角(度)：	Axial Angle (deg):
轴承孔 Ø	Bearing Bore Ø
轴承座三面图...	Bearing Housing 3-View...
轴承座三面图（底板 + 支承板 + 圆筒；左视图全剖）。单位：毫米（mm）。\n默认：Ø130 / Ø96 / Ø30，圆筒长 100，底板 240×160×20，总高 180。	Bearing Housing 3-View (Base Plate + Support Plate + Cylinder; Left View fully sectioned). Units: millimeters (mm).\nDefaults: Ø130 / Ø96 / Ø30, cylinder length 100, base plate 240×160×20, overall height 180.
轴点 1 X	Axis Point 1 X
轴点 1 Y	Axis Point 1 Y
轴点 2 X	Axis Point 2 X
轴点 2 Y	Axis Point 2 Y
轴点解析失败	Axis point parsing failed
辅助符号：	Auxiliary Symbol:
输入 JavaScript 脚本。全局对象 app 提供：addLine/addCircle/addArc/addRect/addPoint/addText、elementCount/elementInfo/removeElement、run(命令)/log(消息)；print(...) 输出到日志。	Enter a JavaScript script. The global object app provides: addLine/addCircle/addArc/addRect/addPoint/addText, elementCount/elementInfo/removeElement, run(command)/log(message); print(...) outputs to the log.
输入名称后可保存复用	Enter a name to save it for reuse
输入命令 (如 line 0,0 10,0) — 输入 help 查看, Tab 补全, ↑↓ 历史	Enter command (e.g. line 0,0 10,0) - Type help to view, Tab completion, ↑↓ history
输入命令名 / 中文名 / 别名过滤（↑↓ 选择，Enter 执行，Esc 关闭）	Filter by command name / Chinese name / alias (↑↓ to select, Enter to execute, Esc to close)
输入图层名，如 01_轮廓线	Enter layer name, e.g. 01_Outline
输入图纸名称：	Enter drawing name:
输入数值，如 1.6 → 自动匹配标准值	Enter a value, e.g. 1.6, to auto-match a standard value
输入模板名称（将记录当前图纸尺寸作为参考尺寸）：	Enter template name (the current drawing size will be recorded as the reference size):
输入比例（图形:实际）。1:1 = 1.0，1:2 = 0.5，2:1 = 2.0。\n修改比例后，已有图元将按此比例等比缩放（图框与标题栏不变）。	Enter scale (drawing:actual). 1:1 = 1.0, 1:2 = 0.5, 2:1 = 2.0.\nAfter changing the scale, existing entities will be scaled proportionally (the drawing frame and title block remain unchanged).
输入比例（图形:实际），如 1:2 或 2:1，也可填小数 0.5	Enter scale (drawing:actual), e.g. 1:2 or 2:1, or a decimal such as 0.5
输入约束  [F8]正交%1  [F9]栅格吸附%2（步长 %3 mm）  捕捉容差 %4 px	Input Constraints  [F8] Ortho %1  [F9] Grid Snap %2 (step %3 mm)  Snap Tolerance %4 px
输出目标	Output Target
边数	Number of Sides
边界	Boundary
边界 (BOUNDARY)：依次点击节点定义封闭填充边界（可含内孔），生成边界图元供剖面填充直接复用，无需逐段描边	Boundary (BOUNDARY): Click points in sequence to define a closed hatch boundary (inner holes allowed), generating a boundary entity that section hatching can reuse directly, with no need to trace each segment
边界 (BOUNDARY/BPOLY)	Boundary (BOUNDARY/BPOLY)
边界(BOUNDARY)：依次点击节点定义封闭填充边界（可含内孔），生成边界图元供剖面填充直接复用，无需逐段描边	Boundary(BOUNDARY): Click points in sequence to define a closed hatch boundary (inner holes allowed), generating a boundary entity that section hatching can reuse directly, with no need to trace each segment
边界=Boundary	Boundary =Boundary
边界区域管理 (REGION)	Boundary Region Management (REGION)
边界区域管理(&R)...	Boundary Region Management(&R)...
边界填充	Boundary Hatch
边界填充(&B)...	Boundary Hatch(&B)...
边界外环已完成（%1 节点），继续拾取内孔，右键结束	Outer boundary complete (%1 nodes); continue picking inner holes, right-click to finish
边界多边形	Boundary Polygon
边界多边形(BPOLY)：与边界同义，依次点击节点定义封闭填充多边形（可含内孔）	Boundary Polygon(BPOLY): Synonymous with Boundary; click points in sequence to define a closed hatch polygon (inner holes allowed)
边长	Edge Length
过三点绘制圆	Draw a circle through three points
过四点构造抛物线样条	Construct a parabolic spline through 4 points
过指定点作直线垂直于指定直线	Draw a line through a specified point perpendicular to a specified line
过滤器 (FILTER)	Filter (FILTER)
过滤器(&F)...	&Filter...
过滤器「%1」：命中 %2 个对象（%3 选择集）	Filter \"%1\": %2 objects matched (%3 selection sets)
过滤当前表的键名 / 值	Filter keys / values of the current table
过点平行线	Parallel Line Through Point
过点平行线：先选参考线，再点一点（生成等长且对齐的平行线）	Parallel Line Through Point: first select a reference line, then click a point (generates an equal-length, aligned parallel line)
过点按角度与长度绘制直线	Draw a line through a point at a given angle and length
过点样条	Spline Through Points
过点绘制水平直线	Draw a Horizontal Line Through a Point
过点绘制竖直直线	Draw a vertical line through a point
过约束告警：尺寸 #%1 所在约束冗余/冲突（过约束尺寸开关已开，请移除矛盾或冗余约束）	Over-constraint warning: constraint on dimension #%1 is redundant/conflicting (the over-constrained dimension switch is on; remove contradictory or redundant constraints)
运算符	Operator
运算符：	Operator:
运行 JavaScript	Run JavaScript
运行 JavaScript...	Run JavaScript...
运行 SCR 脚本...	Run SCR Script...
运行 SCR 脚本（每行一条命令，# 为注释）	Run SCR Script (one command per line, # for comments)
运行合规检查 (STDCHECK)	Run Compliance Check (STDCHECK)
运行脚本	Run Script
运行脚本文件...	Run Script File...
返回当前图形中的图层列表。	Returns the list of layers in the current drawing.
返回当前选择集的摘要（图元类型与数量）。	Returns a summary of the current selection set (entity types and counts).
连合	Join
连合(&J)	Join(&J)
连合：%1 条图元 → %2 条 Polyline	Join: %1 entities → %2 Polylines
连接失败：\n%1	Join failed:\n%1
连接成功。	Join successful.
连接线段...	Join Segments...
连接线段需要至少 2 个选中图元	Joining segments requires at least 2 selected entities
连续	Continuous
连续标注	Continued Dimension
连续标注=Continuous annotation	Continuous =Continuous annotation
连续标注：首尾相接的链式尺寸（首尾均为箭头）	Continuous Dimension: chained dimensions connected end-to-end (arrows at both ends)
连续直线模式：%1	Continuous Line Mode: %1
连续直线模式：上一段终点自动作为下一段起点，右键结束  [K]	Continuous Line Mode: the endpoint of the previous segment automatically becomes the start point of the next; right-click to end  [K]
连续直线模式：继续点击可接着画，右键或 Esc 结束	Continuous Line Mode: keep clicking to continue drawing, right-click or Esc to end
连续绘制	Continuous Drawing
连续绘制开关（上一图元终点为下一图元起点）	Continuous Drawing Toggle (the previous entity's endpoint becomes the next entity's start point)
追加到当前选择集	Append to Current Selection Set
追加样条点	Append Spline Point
追加样条点：选择样条并点击要追加的点	Append Spline Point: select a spline and click the point to append
追踪	Tracking
追踪      极轴%1（增量 %2°%3）  对象追踪%4  临时追踪点 %5 个	Tracking      Polar %1 (Increment %2°%3)  Object Snap Tracking %4  Temporary Track Points %5
追踪(&K)	&Tracking
追踪设置...	Tracking Settings...
追踪（极轴 / 对象捕捉追踪）	Tracking (Polar / Object Snap Tracking)
退出	E&xit
适应窗口	Zoom to Fit
适配	Fit
适配窗口	Fit to Window
适配窗口 (Fit)	Fit to Window (Fit)
选中	Selected
选中图中全部图元	Select All Entities in Drawing
选中图元中无可填充的闭合区域（边界需 ≥3 点外环，或选中圆/矩形/多边形/闭合折线）	No hatchable closed area among the selected entities (boundary requires an outer ring of ≥3 points, or a selected circle/rectangle/polygon/closed polyline)
选中图元为空	No entities selected
选中图元按行列矩形阵列	Rectangular array of selected entities in rows and columns
选中图元线型 -> %1	Selected element line style -> %1
选中图元线宽 -> %1	Selected element line width -> %1
选中图元绕中心环形阵列（基点默认捕捉圆心）	Polar array of selected entities around a center (base point snaps to the center by default)
选中图元颜色 -> %1	Selected entities color -> %1
选中指定图层上的全部图元	Select All Entities on the Specified Layer
选中的图元不是直线	The selected entity is not a line
选择	Select
选择 / 移动 (Select)：点击选择，拖拽平移，空白处框选	Select / Move (Select): click to select, drag to pan, box-select in empty areas
选择 DXF 版本	Select DXF Version
选择 SQLite 数据库文件	Select SQLite Database File
选择(&N)	&Selection
选择参照图形	Select Reference Drawing
选择同图层	Select Same Layer
选择含 dwg2dxf.exe / dwgwrite.exe 的目录	Select the folder containing dwg2dxf.exe / dwgwrite.exe
选择图元	Select Entities
选择图层	Select Layer
选择图层：无效的种子图元	Select Layer: invalid seed entity
选择图层：点击图元，选中同图层全部图元	Select Layer: click an entity to select all entities on the same layer
选择图纸文件	Select Drawing File
选择子档输出目录	Select Sub-drawing Output Folder
选择实体	Select Entities
选择实体：点击图元将其选中	Select Entities: click an entity to select it
选择工具(&S)	&Selection Tools
选择循环：第 %1 / %2 个（继续 Ctrl+点击切换，其他点击退出循环）	Selection Cycling: item %1 / %2 (Ctrl+click to continue cycling, other clicks exit cycling)
选择相交	Select Intersecting
选择相交：无效的种子图元	Select Intersecting: invalid seed entity
选择相交：点击图元，选中所有与之相交的图元	Select Intersecting: click an entity to select all entities that intersect it
选择线型	Select Linetype
选择脚本文件	Select Script File
选择菜单	Select Menu
选择要分离的外部参照（嵌套参照只能在它的宿主图中分离）：	Select the external reference to detach (nested references can only be detached in the drawing that hosts them):
选择要发布的图纸（按列表顺序出图）：	Select drawings to publish (plotted in list order):
选择要编辑的块：	Select the block to edit:
选择轮廓	Select Contour
选择轮廓：无效的种子图元	Select Contour: invalid seed entity
选择轮廓：点击一条边，选中整条拓扑相连的轮廓	Select Contour: click an edge to select the entire topologically connected contour
选择输出 PDF	Select Output PDF
选择输出基名	Select Output Base Name
选择过滤器	Select Filter
选择集取反	Invert Selection Set
选择颜色	Select Color
选择默认存盘目录	Select Default Save Directory
选择默认模板	Select Default Template
选项	Options
选项 %1 暂未在该命令中实现	Option %1 is not yet implemented in this command
选项...	Options...
选项写入失败	Failed to write options
逐行执行脚本文件中的命令	Execute commands from a script file line by line
配合代号	Fit Designation
配置写入失败	Failed to write configuration
配置写入失败：%1	Failed to write configuration: %1
配置已应用，但写入配置文件失败：%1\n下次启动可能回落默认连接。	Configuration applied, but failed to write the config file: %1\nNext startup may fall back to the default connection.
重做	Redo
重合=Coincident	Coincident =Coincident
重命名	Rename
重命名...	Rename...
重命名区域	Rename Region
重命名图层	Rename Layer
重命名图层 	Rename Layer 
重命名块	Rename Block
重命名块 	Rename Block 
重命名失败：新名称已存在或非法。	Rename failed: the new name already exists or is invalid.
重命名布局 	Rename Layout 
重命名文字样式 	Rename Text Style 
重命名标注样式 	Rename Dimension Style 
重命名模板	Rename Template
重命名编组	Rename Group
重命名编组 %1 → %2	Rename Group %1 → %2
重命名边界区域	Rename Boundary Region
重复命令: 	Repeat Command: 
重复图元清理 (OVERKILL)：删除完全重合 / 合并共线重叠线段	Duplicate Entity Cleanup (OVERKILL): delete fully coincident / merge collinear overlapping segments
重定义块	Redefine Block
重定义块 	Redefine Block 
重定义失败，请检查选择集。	Redefine failed; check the selection set.
重建空间索引并重绘视图	Rebuild spatial index and redraw the view
重新加载命令别名	Reload Command Aliases
重新加载插件目录	Reload Plugin Directory
重生成	Regenerate
重生成(&R)	Regenerate(&R)
重置为默认值	Reset to Defaults
重载别名	Reload Aliases
重量估算	Weight Estimate
重量估算：%1（%2 mm 厚）→ %3 g	Weight Estimate: %1 (%2 mm thick) → %3 g
重量估算：密度表为空	Weight Estimate: density table is empty
重量估算：请恰好选中一个闭合轮廓	Weight Estimate: select exactly one closed contour
重量估算：选中图元不是闭合轮廓	Weight Estimate: the selected entity is not a closed contour
锁定图层（禁止编辑本层图元）	Lock Layer (prevents editing entities on this layer)
错误：Error	Error: Error
锥度/斜度符号	Taper/Slope Symbol
键	Key
镜像	Mirror
镜像...	Mirror...
长L:	Length L:
长度	Length
长度 l	Length l
长度分区数:	Length Divisions:
长度单位（显示/输入）：	Length Units (display/input):
长度精度：	Length Precision:
闭合	Closed
间距 e	Spacing e
间距须 ≥0（0 = 按数量均分，需用 divide）	Spacing must be ≥0 (0 = divide equally by count; use divide)
间距须为正数	Spacing must be a positive number
间隔	Interval
阵列(&R)	A&rray
阵列个数	Array Count
阵列复制	Array Copy
阶梯回转体（法兰盘 + 轮毂 + 中心孔）参数化视图。\n主视图全剖 A—A，左视图为圆形投影。单位：毫米（mm）。\n默认：法兰 Ø80 / 轮毂 Ø60 / 沉孔 Ø41 / 通孔 Ø33 / 总长 136，\n3-Ø11.5 均布 + 2-Ø10 定位孔。	Stepped Rotational Part (Flange + Hub + Center Hole) Parametric View.\nFront view fully sectioned A-A, left view is a circular projection. Units: millimeters (mm).\nDefaults: flange Ø80 / hub Ø60 / counterbore Ø41 / through hole Ø33 / overall length 136,\n3-Ø11.5 equally spaced + 2-Ø10 locating holes.
附加外部参照	Attach External Reference
附加外部参照 	Attach External Reference 
附加外部参照失败：%1	Failed to attach external reference: %1
附加外部参照失败：块名 %1 已被普通块占用	Failed to attach external reference: block name %1 is already used by a normal block
附加外部参照失败：文件不存在 %1	Failed to attach external reference: file does not exist %1
附加外部参照失败：文件路径为空	Failed to attach external reference: file path is empty
附加外部参照失败：无法放置参照实例	Failed to attach external reference: unable to place the reference instance
附加失败，详见日志：可能是文件不存在、格式不支持、该参照已附加，或块名被普通块占用。	Attach failed; see the log for details: the file may not exist, the format may be unsupported, the reference may already be attached, or the block name may be used by a normal block.
随图层(ByLayer)	ByLayer
隐私	Privacy
隐私说明	Privacy Notice
隐藏栅格	Hide Grid
零件总长	Overall Part Length
需要至少 2 个选中图元	Requires at least 2 selected entities
非破坏性命令自动执行（关闭则需逐条确认）	Non-destructive commands run automatically (when off, each command requires confirmation)
非箭头侧（基准线上方）	Non-Arrow Side (above the datum line)
面域特性	Region Properties
面板	Panels
面积	Area
面积 %1	Area %1
面积 / 周长 / 包围盒 / 形心 / 形心轴惯性矩	Area / Perimeter / Bounding Box / Centroid / Centroidal Moment of Inertia
面积：	Area:
面积：无目标（先选中图元、填入 ID，或使用点列模式）	Area: no target (select entities, enter an ID, or use point-list mode first)
面积：点列需至少 3 个点	Area: at least 3 points required
页边距 左	Left Margin
页面设置	Page Setup
页面设置覆盖（可选）	Page Setup Override (optional)
项目名称	Project Name
项目间距 (mm)：	Item Spacing (mm):
预览	Preview
预览 — %1	Preview - %1
预览统计	Preview Statistics
预览：	Preview:
预览：长度 %1 ／ 角度 %2	Preview: Length %1 / Angle %2
颈部外径 N	Neck Outer Diameter N
颈部总高	Neck Total Height
颜色	Color
颜色无效: 	Invalid Color: 
颜色：	Color:
飞机三面图	Aircraft 3-View
飞机三面图...	Aircraft Three-View...
首	Start
首孔起始角	First Hole Start Angle
驱动	Driving
驱动...	Drive...
驱动尺寸	Drive Dimension
黑白出图(打印机/PDF)	Monochrome Plot (Printer/PDF)
默认	Default
默认存盘目录：	Default Save Directory:
默认层 0 不可删除	Default layer 0 cannot be deleted
默认层 0 不可改名	Default layer 0 cannot be renamed
默认打印机：	Default Printer:
默认模板文件：	Default Template File:
默认（不关联样式）	Default (no style associated)
齿数 z：	Number of Teeth z:
（全局快捷键 %1）	(Global Shortcut %1)
（原图元保留）	(Original entities retained)
（参数 %1）	(Parameter %1)
（另有 %1 个嵌套参照，只能在直接附加它的宿主图中分离）	(%1 other nested references can only be detached in the drawing that directly hosts them)
（另有 %1 处嵌套引用因定义缺失不再绘制）	(%1 other nested references are no longer drawn because their definitions are missing)
（失败行：%1）	(Failed lines: %1)
（对称轴 #%1）	(Symmetry axis #%1)
（已并入选区）	(Merged into the selection)
（当前没有块。请先用「创建块」建立块定义。）	(No blocks yet. Use \"Create Block\" first to create a block definition.)
（当前）	(Current)
（快捷键 %1）	(Shortcut %1)
（您是不是想输入: 	(Did you mean: 
（无可清理项）	(Nothing to clean up)
（无已加载插件）	(No plugins loaded)
（未选择块）	(No block selected)
（模型中暂无可查询图元）	(No queryable entities in the model yet)
（目标值 %1）	(Target value %1)
（离线模拟）暂未理解该指令。可尝试：“画一个圆心(0,0)半径50的圆”。	(Offline simulation) Command not understood. Try: \"draw a circle centered at (0,0) with radius 50\".
（空 = 使用程序目录 tools/dwg 或环境变量 SKETCHCAD_DWGTOOLS）	(Blank = use the program directory tools/dwg or the environment variable SKETCHCAD_DWGTOOLS)
（空 = 使用系统默认/上次目录）	(Blank = use the system default / last directory)
（空 = 新建时生成标准图框）	(Blank = generate a standard drawing frame on new)
（空）	(Empty)
（空）= 模型空间打印	(Blank) = plot model space
（路径不存在）	(Path does not exist)
（输入 help 查看）	(Type help to view)
（边界已失效）	(boundary is no longer valid)
（选择「角度直线」「平行线」「圆心+半径」等时此处生效）	(Applies when \"Angle Line\", \"Parallel Line\", \"Center + Radius\", etc. are selected)
（选择「角度直线」或「平行线」时此处生效）	(Applies when \"Angle Line\" or \"Parallel Line\" is selected)
（错误：	(Error:
（默认 / 全部显示）	(Default / Show All)
（默认/全显）	(Default/Show All)
（默认/系统）	(Default/System)
，内孔 %1 个已掏空	, %1 inner holes hollowed out
，原图元已替换为块引用	, original entities replaced by a block reference
，含块 %1 个	, contains %1 blocks
，已忽略	, ignored
，已放置块引用	, block reference placed
，引入 %1 个嵌套参照	, %1 nested references introduced
，比例 %1	, scale %1
，比例沿用布局	, scale follows the layout
，级联清理 %1 个嵌套参照	, cascaded cleanup of %1 nested references
：几何退化，无法标注	: degenerate geometry, cannot dimension
：已拾取被测点，请拾取坐标基准点	: Measured point picked, now pick the coordinate datum point
：已拾取起点，请拾取下一点（引线折点）	: Start point picked, now pick the next point (leader vertex)
：已选第 1 条 #%1，请拾取第 2 条直线（点中选中的那一端会被保留）	: 1st line #%1 selected, pick the 2nd line (the picked end is kept)
：已选第 1 条直线 #%1，请点选第 2 条相交直线	: Line #%1 selected; select the second intersecting line
：当前仅支持直线图元	: Currently only line entities are supported
：所选两条直线不相交，无法标注夹角	: The two selected lines do not intersect; cannot create angular dimension
：数据不足，无法标注	: Insufficient data, cannot dimension
：暂不支持此图元类型	: This entity type is not yet supported
：未找到有效边	: No valid edge found
：第 2 条不能是同一条直线	: The 2nd line cannot be the same line
：请勿重复选择同一条直线，请点选另一条直线	: Do not select the same line twice; pick another line
：请点选直线	: Please click a line
：请点选直线图元作为第 1 条边	: Select a line element as edge 1
：请点选直线图元作为第 2 条边	: click a line entity as the second edge
：请点选第 2 条直线	: Select the second line
：请点选第一条直线	: Select the first line
：请点选要标注的图元	: Please click the entity to dimension
；退回逐图元统计：	; falling back to per-entity statistics:
？输入 help 查看全部）	? Type help to view all)
)I18N_DICT_END";

const char* const kTermData = R"I18N_DICT_END(
一个	a
一条	a
一键	one-click
七	Seven
三	Three
三点	3 points
三点圆	3-point circle
三点圆弧	3-point arc
上	on
上一点	previous point
上方	above
上次异常退出	Last abnormal exit
下	under
下一步	Next
下一点	next point
下方	below
不	not
不可	cannot
不可用	unavailable
不合规	non-compliant
不同	different
不含	excluding
不在	not in
不存在	does not exist
不支持	not supported
不是	is not
不是数字	is not a number
不能	cannot
不足	insufficient
不适用	not applicable
不闭合	open
不限	Unlimited
与	and
且	and
丢弃所选	Discard Selected
两点	2 points
两点圆	2-Point Circle
两点构造线	2-Point Construction Line
两点绘制直线	Draw a line through two points
两节点相邻无需删除	Two nodes are adjacent; no deletion needed
两角点绘制矩形	Draw a rectangle from two corner points
严重	fatal
个	
个图	entities
个图元	entities
中	in
中上	top center
中下	bottom center
中图元	entity
中心	center
中心孔公差	Center Hole Tolerance
中心孔径	Center hole diameter
中心孔符号	Center Hole Symbol
中心孔面粗糙度	Center hole surface roughness
中心点	center point
中心点指定方式	Center Point Specification
中心点解析失败	Center point parse failed
中心线	center line
中点	midpoint
中粗线	medium line
中间	middle
临时	temporary
临时捕捉覆盖	temporary snap override
临时追踪点	Temporary tracking point
为	is
为正	must be positive
为空	is empty
为空或	is empty or
主	Main
主孔公差	Main hole tolerance
主视图	front view
主起落架距机头	Main Gear to Nose Distance
主起落架轮距	Main Gear Track
义	Definition
之	
九	Nine
了	
于	at
互	Mutual
五	Five
些	
交点	intersection
什么都没发生	nothing happened
仅	Only
仅此一次	one time only
介于	between
从	from
从列表中删除	Remove from list
从当前选择集排除	Exclude from Current Selection
从编组移除	Remove from Group
从选区移除	Remove from Selection
代号	Code
以	to
以及	and
任	Any
会	will
伸	Extend
但	but
但是	but
位移参数解析失败	Failed to parse displacement parameters
低	Low
低于	below
体	Body
余	Remaining
作用于当前选择集	Applies to the current selection set
你	you
使	Use
使用	use
例如	for example
依序通过给定点列构造样条	Build a spline through the given point list
依次点击	click in order
依次输入顶点绘制样条曲线	Enter vertices in order to draw a spline
保存	Save
保存修改	Save Changes
保存别名	Save Alias
保存到数据库	Save to Database
保存动作宏	Save Action Macro
保存失败	Failed to save
保存宏	Save Macro
保存并连接	Save and Connect
保存时间	Save Time
保存界面定制文件	Save Interface Customization File
保存界面配置	Save Interface Configuration
保存过滤器	Save Filter
保留	keep
保留其他	keep other
信息	information
修	Modify
修剪	trim
修剪后节点不足	Not enough nodes after trimming
修改	modify
修改单位	Modify Units
修改命令	Modify Command
修改图纸界限	Modify Drawing Limits
修改捕捉设置	Modify Snap Settings
修改既有文字内容	Modify Existing Text Content
修改标注值	Modify Dimension Value
修改追踪设置	Modify Tracking Settings
俯视图	top view
倍数须为正数	Multiplier must be a positive number
倍率无效	Invalid Scale Factor
倒角	chamfer
倒角矩形	Chamfered Rectangle
值	Value
假	False
偏移	offset
偏移失败	Offset failed
停止	stop
停止录制	Stop Recording
停用	Disable
储	Store
允	Allow
允许	allows
元	Entity
充	Fill
先	first
先选	select first
先选中	select first
入	Input
全	All
全选	select all
全选区域	Select All Regions
全部	All
全部不选	Deselect All
全部丢弃	Discard All
全部关闭	Turn All Off
全部开启	Turn All On
全部显示	Show All
全部清空	Clear All
全部选择	Select All
八	Eight
公	Tolerance
公切线	Common Tangent
公差	tolerance
六	Six
共	totally
共线	collinear
关	Off
关于	About
关联	associative
关联尺寸标注	Associative Dimension
关闭	Close
关闭图纸	Close Drawing
其	its
其他	Other
其他图层	other layers
内	inside
内切圆	Inscribed Circle
内孔	hole
内容	content
内接圆半径	Inscribed Circle Radius
内隔墙厚	Interior Wall Thickness
再	then
写	Write
写入	write
写入剪贴板失败	Failed to write to clipboard
写块	Write Block
冲突	conflict
冻结	freeze
冻结图层	frozen layers
减	Reduce
减选	remove from selection
几何公差	Geometric Tolerance
几何公差创建失败	Failed to create geometric tolerance
几何公差标注	Geometric Tolerance
几何约束	geometric constraint
凸台轴距底面	Boss Axis-to-Bottom Distance
凸面	Convex
凹面	Concave
出	Output
出图比例	plot scale
分区	Partition
分区默认关闭	Partitions disabled by default
分布	distribute
分离	detach
分离外部参照	Detach External Reference
分类	Category
分解	explode
分解块	Explode Block
分解样条为线段	Explode Spline into Line Segments
分钟	minutes
切换	toggle
切换为	switch to
切换到指定工作空间	Switch to Specified Workspace
切换绘图平面	Switch Drawing Plane
切点	tangent point
切线	Tangent
列	Column
列出可用工作空间	List Available Workspaces
列出块定义的动态参数	List Dynamic Parameters of a Block Definition
列出当前全部别名	List All Current Aliases
列宽	column width
列数	Column Count
列表	list
列间距	Column Spacing
则	then
创建	create
创建块	Create Block
创建时间	Creation Time
创建编组	Create Group
删	Delete
删除	delete
删除两点之间	Delete Between Two Points
删除区域	Delete Region
删除块	Delete Block
删除失败	Delete failed
删除折线上两节点之间的部分	Delete the portion between two nodes on a polyline
删除折线上的节点	Delete Nodes on a Polyline
删除条件	Delete Condition
删除样条点	Delete Spline Point
删除确认	Confirm Delete
删除边界区域	Delete Boundary Region
删除选中	Delete Selected
删除选中图元	Delete Selected Entities
删除重复对象	Delete Duplicate Objects
别名	alias
别名列表	Alias List
到	to
制	Create
刷新	Refresh
前	before
前端	Front
前端追加	Append at Front
前缀	prefix
前起落架距机头	Nose Gear to Nose Distance
剖切	section cut
剖视图	section view
剖面	section
剖面区域	section region
剖面填充	hatch
剖面填充参数	Hatch Parameters
剖面线	hatch lines
剖面线角度	Hatch Line Angle
剖面线间距	Hatch Line Spacing
剪切	cut
剪力墙厚	Shear Wall Thickness
剪贴板	clipboard
剪贴板为空	Clipboard is empty
剪贴板内容不可用	Clipboard content is unavailable
剪贴板内容为空	Clipboard content is empty
功	Success
加	Add
加载	load
加载别名	Load Alias
加载外部块库	Load External Block Library
加载工作空间	Load Workspace
加载插件	Load Plugin
加载界面配置	Load Interface Configuration
加选	add to selection
加长图幅	Extend Sheet
加长幅面	extended sheet
动态	dynamic
动态参数列表	Dynamic Parameter List
动态块参数	Dynamic Block Parameters
动态输入	Dynamic Input
动态输入开关	Dynamic Input Toggle
助手	assistant
包含	contains
包括	including
匹配维度	Match Dimension
区	Area
区域	region
十	Ten
半	Half
半径	radius
半径无效	Invalid radius
半径标注	radius
单	Single
单位	units
单元格	cell
单全局	one global
单行文字	single-line text
单重	Unit Weight
卫生间门宽	Bathroom Door Width
即	namely
卸载	unload
厚	Thickness
原	Original
原值	original value
原因	Reason
原图纸路径	Original Drawing Path
原文件已存在	Original File Already Exists
原有配置	previous settings
参	Parameter
参数	parameter
参数化变量表	Parametric Variable Table
参数无效	Invalid parameter
参数表为空	Parameter table is empty
参数解析失败	Failed to parse parameters
参数说明	Parameters
参数输入	parameter input
参数输入条	parameter input bar
参数错误	Parameter Error
参数驱动	Parameter Driven
参照	reference
参考图元不存在	Reference entity does not exist
参考图元已不存在	The reference entity no longer exists
参考线	reference line
参考线垂线	Perpendicular to Reference Line
双	Double
双击	double-click
双折线	break line
双曲线	Hyperbola
双点划线	double dash-dot
反向	Reverse
反转折线顶点顺序	Reverse polyline vertex order
反选	invert selection
发动机半翼展	Engine Semi-Span
发动机距机头	Engine to Nose Distance
发布	Publish
发送	Send
取	Fetch
取消	Cancel
取消固定	Unfix
取消按层选择	Deselect by Layer
取消相交	deselect intersecting
取消选择图元	Deselect Entities
取消选择图层	deselect layer
取消选择实体	deselect entity
取消选择轮廓	deselect contour
取消隔离	Unisolate
变	Change
变量	variable
句	Sentence
另存为	Save As
另存为模板	Save As Template
只	Only
只保留选中图元所在图层可见	Keep only the layers of selected entities visible
只改了新字段	only new fields changed
可	can
可以	can
可用	available
可用值	valid values
可用脚本引擎	Available Script Engines
可见	visible
可见性	visibility
右	Right
右上	top right
右下	bottom right
右中	middle right
右侧	right
右对齐	right align
右视图	right view
右键	right-click
右键结束	right-click to finish
号	Number
各	each
合并	merge
合并目标必须是折线	Merge target must be a polyline
合规	compliant
合规检查	Compliance Check
合计	Total
同	Same
同一区域	the same region
同心	concentric
同心圆	Concentric Circles
同时	also
名	Name
名称	Name
后	after
后端	Backend
后端追加	Backend Append
后自动	then automatically
向	to
向视字母	View Direction Letter
向视符号	View Direction Symbol
向视箭头	View Direction Arrow
否	No
否则	otherwise
启动	start
启用	Enable
周围焊	All-Around Weld
周长	perimeter
命令	command
命令列表	Command List
命令别名列表	Command Alias List
命令名	command name
命令总表	command reference
命令执行失败	Command execution failed
命令行	command line
命令面板	command palette
命名	named
和	and
四	Four
四点	4 points
四点抛物线	4-Point Parabola
回	Round
回放宏	Replay Macro
回放已保存的宏	Play Back Saved Macro
因为	because
围成	enclose
固定	fix
固定到列表	Pin to List
固定点	fixed point
国标	GB standard
图	Drawing
图元	entity
图元不存在	Entity does not exist
图元们	entities
图元数	Entity Count
图元面板	entity panel
图号	drawing number
图层	layer
图层匹配	Layer Match
图层名	layer name
图层样式	Layer Style
图层隔离	Layer Isolate
图层面板	layer panel
图幅	Sheet Size
图形清理	Drawing Cleanup
图框	drawing frame
图框边距	Drawing Frame Margin
图案填充	hatch
图案预览	Pattern Preview
图档	drawing archive
图签字高	Title Block Text Height
图签宽	Title Block Width
图纸	drawing
图纸名称	drawing title
图纸宽	Sheet Width
图纸恢复	Drawing Recovery
图纸特性	Drawing Properties
图纸界限	drawing limits
图纸空间	paper space
图纸高	Sheet Height
图限	drawing limits
图面	drawing area
圆	circle
圆弧	arc
圆弧绘制方式	Arc Drawing Method
圆形绘制方式	Circle Drawing Method
圆心	center
圆心点	center point
圆心解析失败	Failed to parse circle center
圆筒轴向长	Cylinder Axial Length
圆角	fillet
圆角矩形	Rounded Rectangle
圈交	Crossing Polygon
圈围	Window Polygon
在	in
在位编辑块	edit block in place
在当前布局图纸坐标建矩形视口	Create a rectangular viewport at the current layout sheet coordinates
在折线段上插入节点	Insert a vertex on a polyline segment
在指定位置放置形位公差框格	Place a geometric tolerance frame at a specified position
在指定点把图元一分为二	Split an entity into two at a specified point
在视图中拾取中心点	Pick the center point in the view
地脚槽宽度	Anchor Slot Width
地脚槽深度	Anchor Slot Depth
坐标无效	Invalid coordinates
坐标标注	ordinate
块	block
块名	block name
块名不能为空	Block name cannot be empty
块定义	block definition
块引用	block reference
块管理器	Block Manager
垂尾高度	Vertical Tail Height
垂直	perpendicular
垂直与角度	Perpendicular && Angle
垂直参考线切线	Tangent to Vertical Reference Line
垂直居中	Center Vertically
垂直线	vertical line
垂线生成	Perpendicular Line Generation
垃圾	garbage
型	Type
域	Region
域更新	Field Update
基于	based on
基准	datum
基准符号	Datum Symbol
基准线	datum line
基本	Basic
基本绘制	Basic Drawing
基点	base point
基点解析失败	Failed to resolve the base point
基线	Baseline
基线标注	baseline
填	Fill
填充	hatch
填充区域	Hatch Area
填写标题栏	Fill in Title Block
壁孔数量	Number of Wall Holes
备注	Remark
复	Restore
复制	copy
复制并粘贴	Copy and Paste
复制移动	Copy and Move
外	outside
外接圆半径	Circumscribed Circle Radius
外部参	external reference
外部参照	external reference
多	Many
多段线	polyline
多段线编辑	Polyline Edit
多行文字	multiline text
多边形	polygon
多边形绘制方式	Polygon Drawing Method
多重引线	multileader
大	Large
大于	must be greater than
大小	Size
失败	failed
夹点	grip
夹点编辑	grip edit
套用失败	Apply failed
套用模板	Apply Template
如	such as
如果	if
孔轴投影	Shaft Projection
字	Text
字体	font
字分解	Text Explode
字号	text size
字宽	text width
字段	field
字段很多但条数很少	many fields but few records
字高	text height
字高须为正数	Text height must be positive
存	Save
存为模板	Save as Template
安装	install
完成	done
完整路径	Full Path
宏录制	macro recording
定	Set
定义	define
定义变量	Define Variable
定义的	defined
定位	locate
定位孔数量	Number of Locating Holes
定位首孔角	First Hole Angle
定数等分	Divide by Number
定角线	line at angle
定距等分	Divide by Distance
实	Solid
实时	live
实时预览	Live Preview
实线	solid
宽	Width
宽度	width
宽度因子	width factor
寸	Size
对	to
对两图元倒圆角	Fillet Two Entities
对中符号	Centering Mark
对准	align
对称	symmetric
对象捕捉	object snap
对象捕捉追踪	Object Snap Tracking
对象捕捉追踪开关	Object Snap Tracking Toggle
对齐	align
对齐方式	alignment
对齐标注	aligned
导	Export
导入	import
导入失败	Import failed
导出	export
导出失败	Export failed
导出完成	Export complete
导出成功	Export successful
封闭	closed
射线	ray
将	
将折线中所有弧段改为直线段	Convert all arc segments in the polyline to line segments
将选中图元移入此层	Move Selected Entities to This Layer
小	Small
小于	must be less than
少	Few
就绪	Ready
尺	Dimension
尺寸	dimension
尺寸值	Dimension Value
尺寸数字字高	Dimension Text Height
尺寸无效	Invalid dimension
尺寸标注	dimension
尺寸界	extension line
尺寸线	dimension line
尺寸线通过	dimension line through
尺寸链带宽	Dimension Chain Band Width
尺寸驱动	Dimension Drive
层	Layer
居中	center
属性	property
属性定义	Attribute Definition
属性提取	Attribute Extraction
属性模板套用失败	Failed to apply property template
属性面板	property panel
工作平面	Flat Panel
工作空间	Workspace
工作空间列表	Workspace List
工具	Tools
工具条	toolbar
工具栏	toolbar
工程制图合规检查	Engineering Drawing Compliance Check
工程名称	Project Name
工程地址	Project Address
左	Left
左上	top left
左下	bottom left
左中	middle left
左侧	left
左对齐	left align
左视图	left view
差	Deviation
已	already
已保存	saved
已修复	repaired
已冻结	Frozen
已切换到模型空间	Switched to model space
已创建	created
已删除	deleted
已加载	loaded
已加载插件	Loaded Plugins
已卸载	unloaded
已发送至打印机	Sent to printer
已取消	cancelled
已取消夹点编辑	Cancelled grip edit
已取消当前操作	Cancelled current operation
已取消标注	Dimension cancelled
已取消设置线宽	Cancelled lineweight setting
已合并到折线	Merged into polyline
已启用	enabled
已回退一个参考图元	Stepped back one reference entity
已回退一个拾取点	Stepped back one pick point
已存在	already exists
已完成	completed
已定义	defined
已忽略	ignored
已恢复	restored
已恢复全部面板	Restored all panels
已找到	found
已放弃上一点	Discarded the previous point
已显示	Shown
已替换	replaced
已清空全部显式几何约束	Cleared all explicit geometric constraints
已清空草图	Cleared sketch
已清除	cleared
已生成	generated
已禁用	disabled
已结束当前绘制	Ended current drawing
已结束连续直线绘制	Continuous line drawing finished
已覆盖	overridden
已解冻	Thawed
已解锁	Unlocked
已跳过	skipped
已连接两线段	Connected two line segments
已选中	selected
已锁定	Locked
已闭合多段线	Polyline closed
已隐藏	Hidden
布局	layout
带基点复制	Copy with Base Point
帮助	Help
帮助主题	Help Topic
常用色	Common Colors
常规	General
幅	Sheet
幅面	sheet size
平	Flat
平分	bisect
平移图元	Move Entities
平移选中图元	Move Selected Entities
平行	parallel
平行构造线	Parallel Construction Line
平行线	parallel line
平面	Plane
并	and
并且	and
序	Sequence
序号	No.
序号球标	item balloon
应	should
应用	Apply
应用到全部文字	Apply to All Text
应用到选中文字	Apply to Selected Text
应用结果	Apply Result
应用过滤器	Apply Filter
底板	base plate
底板厚	Base Plate Thickness
底板厚度	Base Plate Thickness
底板宽	Base Plate Width
底板宽度	Base Plate Width
底板深	Base Plate Depth
底板长度	Base Plate Length
底部	bottom
底部外廓宽度	Bottom Outer Width
度	Degree
延	Extend
延伸	extend
建	Create
建视口	Create Viewport
建议	Suggestion
开	On
开启	Enable
开始录制命令宏	Start Recording Command Macro
开放	Open
异	Different
异常	exception
式	Style
引	Reference
引擎不可用	Engine unavailable
引线	leader
引线标注	leader
张	Sheet
弦长	chord length
弧	Arc
弧长标注	arc length
归为	group into
归并	consolidate
当	when
当前	Current
当前值	current value
当前参数	current parameters
当前命令不支持闭合选项	Current command does not support the Close option
当前图	current drawing
当前图块表	Current Block Table
当前图层	current layer
当前图纸	current drawing
当前图纸没有块引用	Current drawing has no block references
当前图纸没有外部参照	Current drawing has no external references
当前无临时追踪点	No temporary tracking points
当前无显式几何约束	No explicit geometric constraints
当前没有别名	No aliases
当前没有块定义	No block definitions
当前没有工作空间	No workspaces
当前没有编组	No groups
当前没有进行中的操作	No operation in progress
当前选	current selection
当前选择	current selection
当前选择集内	Within current selection
录制宏	Record Macro
形	Shape
形位公差	geometric tolerance
形位公差创建失败	Failed to create geometric tolerance
形位公差框格	feature control frame
形心	centroid
径	Radius
心	Center
必须	must
必须为	must be
必须为正	must be positive
快捷键	shortcut
快速标注	Quick Dimension
快速选择	Quick Select
忽略	Ignore
态	State
总	Total
总体	Overall
总高	Total Height
恢	Restore
恢复	restore
恢复为	Restore To
恢复原有	restore previous
恢复失败	Restore Failed
恢复完成	Restore Complete
恢复所选	Restore Selected
恢复被撤销的编辑	Restore Undone Edit
恢复被隔离图层的显示	Restore display of isolated layers
恢复默认	Restore Defaults
成	Complete
成功	succeeded
或	or
或者	or
截面特性	Section Properties
房门宽	Door Width
房间名字高	Room Name Text Height
所	
所以	therefore
所有	All
所选段节点不足	Not enough nodes in selected segment
手动	manual
打印	Plot
打印失败	Plot Failed
打印比例	Plot Scale
打印线宽比例	Plot Lineweight Scale
打印设置	Plot Settings
打印预览	Plot Preview
打开	Open
打开图纸	Open Drawing
打开失败	Open Failed
打开日志目录	Open Log Directory
打开最近	Open Recent
打开特性面板编辑选中图元	Open Properties panel to edit selected entities
打开草图	Open Sketch
打断	break
打赏	Support
执行	execute
执行信息	Execution Info
执行清理	Execute Cleanup
批量	batch
批量套用到本层图元	Batch apply to entities on this layer
批量签名	batch sign
找	Find
技术要求	technical requirements
把	
把剪贴板内容粘贴为独立图元	Paste clipboard content as independent entities
把块定义插入为块引用	Insert a block definition as a block reference
把录制的宏保存为文件	Save the recorded macro to a file
把折线图元转换为样条	Convert polyline entities to splines
把指定图层图元移出选择集	Remove entities on the specified layer from the selection set
把选中图元移到目标图层	Move selected entities to the target layer
投影	projection
投影圆	Projection Circle
投影符号	projection symbol
投影轮廓	Projection Outline
抖动	Dither
折线	polyline
折线已反转	Polyline reversed
折线已打开	Polyline opened
折线已闭合	Polyline closed
折线绘制与编辑	Polyline drawing and editing
折线转样条	Polyline to Spline
抛物线	parabola
拆分	split
拉伸	stretch
拉伸选中图元的指定部分	Stretch the specified portion of the selected entities
拉长	Lengthen
拒绝执行	execution refused
拖拽	drag
择	Select
拾	Pick
拾取	pick
拾取中心点	Pick center point
拾取点	pick point
指定图框位置	Specify drawing frame position
按	by
按倍率缩放视图	Zoom View by Factor
按图层	By Layer
按层选择	Select by layer
按幅面生成图框与标题栏	Generate drawing frame and title block by sheet size
按当前参数	with current parameters
按当前参数一键生成航空三面图模板	Generate an aircraft three-view template with current parameters in one click
按相同线型累加选择	Add to selection by same linetype
按相同颜色累加选择	Add to selection by same color
按线型	By Linetype
按间距在图元上放置等分点	Place equally spaced points on an entity at a specified spacing
按颜色	By Color
捕捉	Snap
捕捉与追踪	Snap && Tracking
捕捉开关或设置步长	Toggle snap or set the step size
换	Change
接	Connect
控制点	control point
推拉门宽	Sliding door width
提供	provides
提示	hint
插件	Plugins
插件加载失败	Failed to load plugin
插入	insert
插入到光标处	Insert at cursor
插入块	Insert Block
插入失败	Insert failed
插入标准标题栏	Insert Standard Title Block
插入样条点	Insert Spline Point
插入点坐标无效	Invalid insertion point coordinates
摆放方向	Orientation
撤销	undo
撤销上一步编辑	Undo last edit
操作	Action
支承板底宽	Support plate base width
支持	supported
改	Change
改变	changed
改过	modified
放大	Zoom In
放弃	undo
放弃选择	Deselect
放置	place
放置临时追踪点	Place temporary tracking point
放置到图纸	Place on Drawing
放置单行文字	Place single-line text
放置多行文字	Place multiline text
放置焊缝符号	Place weld symbol
数	Number
数值	value
数字	number
数据	data
数据库	database
数据库绘图记录	Database drawing records
数据库配置	Database Configuration
数量	Quantity
整个图形	Entire drawing
整体选中编组	Select group as a whole
整数	integer
整组	whole group
文	Text
文件	File
文件位置	File Locations
文件名	file name
文件夹	folder
文字	text
文字创建失败	Failed to create text
文字样式	text style
文字随图框比例调整	Scale text with the drawing frame
文本	text
斜	Oblique
斜体	Italic
断	Break
新	New
新值	new value
新块名不能为空	New block name cannot be empty
新增	add
新字段	new field
新建	New
新建区域	New Region
新建图层	New Layer
新建图框	New Drawing Frame
新建图纸	New Drawing
新建失败	Failed to create
新建工作空间	New Workspace
新建文字样式	New Text Style
新建自定义工具条	New Custom Toolbar
新建边界区域	New Boundary Region
新标注样式名不能为空	New dimension style name cannot be empty
方向	Direction
旋转	rotate
旋转符号	Rotation Symbol
旋转角	Rotation angle
旋转角无效	Invalid rotation angle
无	None
无可偏移图元	No entities to offset
无可分解图元	No entities to explode
无可分解对象	No objects to explode
无可分解文本	No text to explode
无可反向图元	No entities to reverse
无可变换图元	No entities to transform
无可复制图元	No entities to copy
无可复制的选中图元	No selected entities to copy
无可延长图元	No entities to extend
无可拉伸图元	No entities to stretch
无可撤销操作	Nothing to undo
无可旋转图元	No entities to rotate
无可用标准值	No standard values available
无可缩放图元	No entities to scale
无可重做操作	Nothing to redo
无可镜像图元	No entities to mirror
无效	invalid
无法	cannot
无法创建圆	Cannot create circle
无法创建圆弧	Cannot create arc
无法创建多边形	Cannot create polygon
无法创建曲线	Cannot create curve
无法创建椭圆	Cannot create ellipse
无法删除	Cannot delete
无法取得来源端点	Cannot get source endpoint
日志	log
旧	Old
旧值	old value
时	when
明细表	BOM table
星形	star
是	Yes
是否显示左分区	Show left partition
显示	Display
显示指定点坐标	Display the coordinates of a specified point
显示栅格	Display Grid
智能标注	smart dimension
暂停	pause
暗色	Dark
曲	Curved
更	More
更多	More
更新	update
更新时间	Update Time
替	Replace
替换当前选择集	Replace Current Selection
最	Most
最后	finally
最多	at most
最小间隙	minimum gap
最近打开	Open Recent
有	has
有效	valid
有效条件	valid condition
未	not
未匹配	not matched
未命中任何图元	No entities matched
未命名	Untitled
未填写	not filled in
未安装	not installed
未定义	undefined
未找到	not found
未找到可分解的文本图元	No explodable text entities found
未收录	not indexed
未检测到重复图元	No duplicate entities detected
未满足	unsatisfied
未生成	nothing generated
未知	unknown
未知中心线类型	Unknown centerline type
未能	could not
未设置	not set
未连接	not connected
未选中	no selection
未选中要删除的图元	No entities selected for deletion
未选择	nothing selected
末	End
本	Current
机座	housing
机械图拆分	Drawing Split
机身总长	Fuselage overall length
机身高度	Fuselage height
材料	Material
材料类别	Material Category
条	
条件	condition
条数	Count
条目	entry
条直	lines
条直线	lines
板	Panel
极	Extreme
极轴	Polar
极轴增量角	Polar increment angle
极轴追踪	Polar Tracking
极轴追踪开关或设置增量角	Toggle polar tracking or set the increment angle
构造线	construction line
构造线绘制方式	Construction Line Draw Method
果	Result
某	Some
查	Query
查看	view
查询	Inquiry
栅格	Grid
栅格已	Grid is
栅格显示开关	Grid Display Toggle
标	Mark
标准	Standard
标准知识库	Standard Knowledge Base
标准知识库查询	Standard Knowledge Base Query
标注	dimension
标注文本	Dimension Text
标注标准	Dimension Standard
标注样式	dimension style
标注样式名不能为空	Dimension style name cannot be empty
标注样式表至少保留一条	Dimension style table must keep at least one
标注随图框比例调整	Scale Dimensions with Drawing Frame
标题	Title
标题栏	title block
标题栏样式	Title Block Style
标高符号	Elevation Symbol
栏	Bar
栏选	Fence Select
样	Style
样式	Style
样式表预设	Style Table Preset
样条	spline
样条打散	Explode Spline
样条控制点不足	Insufficient spline control points
样条曲线	spline
样条绘制方式	Spline Draw Method
样条编辑	Spline Edit
样条编辑命令	Spline Edit Command
样条过点	Spline Through Points
根据	according to
根部线标注	root line callout
格	Cell
框	Frame
框选	window selection
检查	check
检查更新	Check for Updates
检测到上次异常退出	Detected an abnormal exit during the previous session
椭圆	ellipse
椭圆弧	Elliptical Arc
椭圆绘制方式	Ellipse Draw Method
模型	model
模型浏览器	model browser
模型空间	model space
模板	template
模板名称	Template Name
横	Horizontal
横向	Landscape
横置	Horizontal
次	
正	Positive
正交	Ortho
正交折线	Ortho Polyline
正交模式开关	Ortho Mode Toggle
正多边形	regular polygon
正数	positive number
此	this
步	Step
段	Segment
段索引无效	Invalid segment index
段节点不足	Insufficient segment nodes
每	each
比	Ratio
比例	scale
比例必须为正数	Scale must be a positive number
比例提示	Scale Hint
比例标注	Scale Dimension
比例计算	Scale Calculation
比例调整设置	Scale Adjustment Settings
比例须为正数	Scale must be positive
毫秒	milliseconds
水平	horizontal
水平居中	Center Horizontally
水平构造线	Horizontal Construction Line
水平标注	horizontal
水平线	horizontal line
求解	solve
沉孔深	Counterbore Depth
没有	no
没有可合并的线段	No line segments to merge
没有可放弃的点	No point to discard
没有可重复的命令	No command to repeat
没有该颜色的图元可累加选择	No entities of that color available to add to selection
没有选中任何图元	No entities selected
泄漏	leak
法兰	flange
法兰厚度	Flange Thickness
法兰外圆粗糙度	Flange Outer Surface Roughness
波浪线	wave
注	Note
浅	Shallow
测	Measure
测试连接	Test Connection
测量两点距离与角度	Measure distance and angle between two points
测量封闭图元或多点围成区域面积	Measure the area of a closed entity or a region enclosed by multiple points
测量查询	Measure && Inquiry
深	Deep
添加	add
添加几何约束	Add Geometric Constraint
添加到编组	Add to Group
添加到选区	Add to Selection
添加圆	Add Circle
添加条件	Add Condition
添加直线	Add Line
添加选中对象	Add Selected Objects
清理	cleanup
清理选项	Cleanup Options
清空	Clear
清空图中全部图元	Clear all entities in the drawing
清空当前选择集	Clear Current Selection
清除	clear
清除临时追踪点	Clear Temporary Tracking Points
清除全部临时追踪点	Clear All Temporary Tracking Points
清除文字样式关联	Clear Text Style Association
清除最近文件历史	Clear Recent File History
清除覆盖	clear override
清除追踪点	Clear Tracking Points
渐开线	involute
源	Source
源图元已失效	Source entity is no longer valid
满	Full
满足	satisfied
点	point
点击	click
点击立即执行一次自动保存	Run Autosave Once Immediately
点划线	dash-dot
点列	Point List
点到直线垂线	Point-to-Line Perpendicular
点在线上	point on line
点坐标	Point Coordinates
点对称	symmetric point
点线	dotted
点解析失败	Failed to parse point
点选	click
点镜像	Point Mirror
焊接符号	Weld Symbol
焊缝符号	weld symbol
然后	then
照	Reference
熔透焊	Full Penetration Weld
版	Version
版本	Version
物理打印机	Physical Printer
特性	property
特性匹配	Match Properties
特殊矩形	Special Rectangle
特粗线	extra thick
状态	Status
状态栏	status bar
率	Ratio
环境	Environment
环境变量	environment variable
环形阵列	polar array
现场焊	Field Weld
生成	generate
生效	in effect
用	with
用于	used for
用当前选择重定义	Redefine with Current Selection
用户	User
用户坐标系	User Coordinate System
用户指定的填充边界	user-specified hatch boundary
用法	Usage
由	by
由圆弧创建	Create from Arc
由多段线创建样条	Create Spline from Polyline
画	Draw
界	Boundary
界面	Interface
界面语言	UI Language
留空	leave blank
留空则	if blank,
的	
的图元	entity
目	Target
目录	catalog
目标	Target
目标不是折线	Target is not a polyline
直	Straight
直径	diameter
直径标注	diameter dimension
直线	line
直线绘制方式	Line Drawing Method
直线链	line chain
相	Mutual
相交	intersect
相关命令	Related Commands
相切	tangent
相切三图元	Tangent to 3 entities
相切延续弧	Tangent Continuation Arc
相同	same
相对参考线角度	Angle relative to reference line
相对角度线	Line at relative angle
相等	equal
省略	omit
看起来差不多	looks similar
真	True
矩形	rectangle
矩形阵列	rectangular array
短	Short
码	Code
确定	OK
确认	confirm
确认丢弃	Confirm Discard
确认全部丢弃	Confirm Discard All
磁盘	disk
磁盘空间不足	Insufficient Disk Space
示例	Example
禁	Forbid
禁用	Disable
种	
秒	seconds
称	Name
移	Move
移出编组	Remove from Group
移到开头	Move to Start
移到末尾	Move to End
移动	move
移除选中对象	Remove Selected Objects
程	Process
程序目录	program directory
空	(Empty)
空白图	Blank Drawing
空间	space
窗口取消	window deselect
窗口缩放	Window Zoom
窗口选择	window select
竖	Vertical
竖直	vertical
竖直构造线	Vertical Construction Line
竖直标注	vertical
竖直线	vertical line
竖置	Portrait
章	Chapter
端点	endpoint
符号	symbol
第	No.
第一孔起始角	First Hole Start Angle
第一点	first point
等	etc.
等分	divide equally
等分失败	Division failed
等半径	equal radius
等长	equal length
签	Tab
签名表	signature table
签名表未匹配到图纸标题栏字段	Signature table did not match any title block fields
简体中文	Simplified Chinese
算	Compute
管理模板	Manage Templates
箭头	arrow
类	
类型	Type
粗	Thick
粗体	Bold
粗点画线	thick dash-dot
粗糙度	surface roughness
粗糙度标注	Surface Roughness Annotation
粗线	thick line
粗细	thickness
粗细比	width ratio
粗虚线	thick dashed
粘贴	paste
粘贴为块	Paste as Block
粘贴失败	Paste Failed
精确定位	precise positioning
系	Series
索引无效	Invalid Index
索引越界	Index out of range
约束	constraint
约束栏	Constraint Bar
级	Grade
纵向	Portrait
纸张尺寸	Paper Size
线	Line
线型	linetype
线型比例	linetype scale
线宽	lineweight
线宽比	linewidth ratio
线宽粗细	lineweight
线宽系列	lineweight series
线宽系数	lineweight factor
线宽随图框比例调整	Lineweight adjusts with the drawing frame scale
线宽须为非负数	Lineweight must be non-negative
线性标注	linear
线段	line segment
线镜像	Line Mirror
线间距	line spacing
线间隙	Line Gap
线间隙打断失败	Line Gap Break Failed
组	Group
细	Thin
细线	thin line
终点	end point
经	via
绑定	bind
结	Result
结束宏录制	Stop Macro Recording
结果	Result
结论	Conclusion
绕基点旋转选中图元	Rotate selected entities about a base point
绕基点缩放选中图元	Scale selected entities about a base point
绘	Draw
绘制	draw
绘制单个或多个点	Draw One or More Points
绘制可打印区边框	Draw printable area border
绘制四周尺寸链	Draw perimeter dimension chains
绘制完成	drawing complete
绘制家具图块	Draw furniture blocks
绘制户型平面图	Draw Floor Plan
绘制房间名标注	Draw room name annotations
绘制法兰盘三面图	Draw flange three-view drawing
绘制法兰盘轮毂视图	Draw Flange Hub View
绘制箱体视图	Draw housing view
绘制轴承座三面图	Draw bearing housing three-view drawing
绘制飞机三面图	Draw Aircraft Three-View
绘图	Draw
绘图区	drawing area
绘图比例	drawing scale
继续	Continue
继续上次工作	Continue Previous Session
继续使用空白图	Continue with Blank Drawing
续	Continue
编号	No.
编组	Group
编辑	edit
编辑块引用的属性值	Edit Block Reference Attribute Values
编辑尺寸	Edit Dimension
编辑属性	Edit Properties
编辑文字	Edit Text
编辑标注文字	Edit Dimension Text
编辑表格	Edit Table
缩	Scale
缩小	Zoom Out
缩放	scale
缩放范围	Scale Range
缺	Missing
缺口宽度不能大于线长	Gap width cannot be greater than the line length
网格	Grid
置为当前	set current
翻	Flip
翼展	Wingspan
者	
联动	linked
聚焦	focus on
能	Can
脚本	Script
脚本错误	Script Error
自动	automatic
自动保存	Autosave
自动保存失败	Autosave Failed
自动保存设置	Autosave Settings
自动化导入	Automated Import
自动化打开	Automated Open
自动填写日期	Auto-fill Date
自动排图	automatic sheet layout
自动更新	update automatically
自动查找	locate automatically
自动选取标准比例	Auto-select Standard Scale
自定义	Custom
自定义比例	Custom Scale
自由度	degrees of freedom
自由手绘	Freehand
至少	at least
节	Section
节点	node
节点索引无效	Invalid node index
若	if
若干	several
英文	English
范围	range
范围内	in range
草图	sketch
草图模型	sketch model
草图约束	Sketch Constraints
菜单栏	menu bar
落点解析失败	Failed to parse the drop point
虚	Dashed
虚线	dashed
螺栓	bolt
螺栓孔	bolt hole
螺栓孔数量	Bolt hole count
螺纹根部线	thread root line
行	Line
行列	rows and columns
行数	Rows
行距	line spacing
行间距	Row Spacing
行高	row height
表	Table
表格	table
表示	means
表达式	expression
表达式括号嵌套过深	Expression bracket nesting too deep
表达式数字格式错误	Invalid number format in expression
表达式缺少右括号	Expression is missing a closing bracket
表面粗糙度	surface roughness
被	
装订宽度	Binding Width
装订线	Binding Margin
要	needs to
覆盖	override
覆盖线宽	Override Lineweight
覆盖颜色	Override Color
规则	rule
视口	viewport
视口宽度必须为正数	Viewport width must be positive
视口旋转	Viewport Rotation
视口比例	Viewport Scale
视口高度必须为正数	Viewport height must be positive
视图	view
视图名	view name
视图平面	View Plane
视图方向	View Direction
视图标识	view label
角	Angle
角平	angle bisector
角平分	angle bisector
角平分线	angle bisector
角平分线构造线	Angle Bisector Construction Line
角度	angle
角度无效	Invalid Angle
角度构造线	Angle Construction Line
角度标注	angular
角度直线	Line at Angle
角度范围	Angle Range
角度须为数值	Angle must be a number
解冻	thaw
解散编组	Ungroup
解析	parse
解析失败	parse failed
解锁	unlock
警告	warning
计算	compute
计算点列多边形	Compute polygon from point list
计算质量特性	Compute Mass Properties
计算距离	Compute Distance
设为预选器	Set as Preselector
设值	Set Value
设置	Settings
设置后续绘制线宽档位	Set Lineweight for Subsequent Drawing
设置后续绘制颜色	Set Color for Subsequent Drawing
设置线宽	Set Lineweight
设计中心	Design Center
设计单位全称	Design Organization Full Name
诊断	Diagnostics
诊断包	diagnostics bundle
词	Word
询	Inquire
该	the
该标注类型暂不支持驱动几何	This dimension type does not yet support driving geometry
误	Error
说明	Description
请	Please
请先	Please
请先在表中选中一个变量	Please select a variable in the table first
请先选	Please select
请先选中	Please select
请先选中要镜像的图元	Please select the entities to mirror first
请先选择作为新块内几何的图元	Please select the entities to serve as geometry in the new block first
请先选择线型	Please select a linetype first
请先选择要匹配的颜色	Please select the color to match first
请先选择要收进块的图元	Please select the entities to include in the block first
请填写图层名	Please enter a layer name
请拾取合适的参考图元	Please pick a suitable reference entity
请指定	Please specify
请指定一点	Please specify a point
请检查	Please check
请注意	Please note
请输	Please enter
请输入	Please enter
请输入有效	Please enter a valid
请输入正确的	Please enter a valid
请选	Please select
请选中一条直线	Please select a line
请选择	Please select
请选择同一折线上的两个节点	Please select two vertices on the same polyline
请选择同一折线上的两段	Please select two segments on the same polyline
请选择同一根样条上的两个控制点	Please select two control points on the same spline
请选择折线	Please select a polyline
请选择样条曲线	Please select a spline
请选择目标折线	Please select the target polyline
读	Read
读取	read
豁免类别	Exempt Category
象限连接	Quadrant Connection
负	Negative
败	Failed
质量特性	Mass Properties
起点	start point
超	Over
超出	out of
超过	exceeds
距离	distance
距离无效	Invalid Distance
跟随系统	Follow System
路径	Path
路径不存在	Path does not exist
路径图元不存在	Path entity does not exist
路径阵列	Path Array
转	Rotate
转换	convert
转角投影	Revolved Projection
轮廓	contour
轴承	bearing
轴点解析失败	Axis point parsing failed
载	Load
载入	load
辅	Auxiliary
辅助	auxiliary
辅助线	auxiliary line
输	Input
输入名称后可保存复用	Enter a name to save it for reuse
输出	output
输出目标	Output Target
边	Edge
边数	number of sides
边界	boundary
边界图元	boundary entity
边界填充	Boundary Hatch
边界多边形	Boundary Polygon
边界工具	boundary tool
边长	edge length
过三点绘制圆	Draw a circle through three points
过四点构造抛物线样条	Construct a parabolic spline through 4 points
过指定点作直线垂直于指定直线	Draw a line through a specified point perpendicular to a specified line
过点平行线	Parallel Line Through Point
过点按角度与长度绘制直线	Draw a line through a point at a given angle and length
过点样条	spline through points
过点绘制水平直线	Draw a Horizontal Line Through a Point
过点绘制竖直直线	Draw a vertical line through a point
过约束	over-constrained
运算符	Operator
运行	run
运行脚本	Run Script
近	Near
返回	Back
返回全部	return all
进行中	in progress
远	Far
连	Connect
连合	Join
连续	Continuous
连续标注	continuous
连续绘制	Continuous Drawing
追加	append
追加到当前选择集	Append to Current Selection Set
追加样条点	Append Spline Point
追踪	Tracking
退出	Exit
适应窗口	Zoom to Fit
适配	Fit
适配窗口	Fit to Window
逆	Reverse
选	Select
选中	Selected
选中图中全部图元	Select All Entities in Drawing
选中图元	selected entities
选中图元为空	No entities selected
选中图元按行列矩形阵列	Rectangular array of selected entities in rows and columns
选中指定图层上的全部图元	Select All Entities on the Specified Layer
选中的图元不是直线	The selected entity is not a line
选择	Selection
选择参照图形	Select Reference Drawing
选择同图层	Select Same Layer
选择图元	Select Entities
选择图层	select layer
选择图纸文件	Select Drawing File
选择子档输出目录	Select Sub-drawing Output Folder
选择实体	select entity
选择工具	selection tools
选择相交	select intersecting
选择线型	Select Linetype
选择脚本文件	Select Script File
选择菜单	Select Menu
选择轮廓	select contour
选择输出基名	Select Output Base Name
选择过滤器	Select Filter
选择集	selection set
选择集取反	Invert Selection Set
选择颜色	Select Color
选择默认存盘目录	Select Default Save Directory
选择默认模板	Select Default Template
选项	Options
选项写入失败	Failed to write options
逐行执行脚本文件中的命令	Execute commands from a script file line by line
通过	via
速	Speed
遍	Pass
部参	external reference
部参照	external reference
配合代号	Fit Designation
配置	Configuration
配置写入失败	Failed to write configuration
采用	use
重	Weight
重做	redo
重叠	overlapping
重合	coincident
重命名	rename
重命名区域	Rename Region
重命名图层	Rename Layer
重命名块	Rename Block
重命名模板	Rename Template
重命名编组	Rename Group
重命名边界区域	Rename Boundary Region
重复	duplicate
重定义块	Redefine Block
重建空间索引并重绘视图	Rebuild spatial index and redraw the view
重新加载命令别名	Reload Command Aliases
重新加载插件目录	Reload Plugin Directory
重生成	Regenerate
重置为默认值	Reset to Defaults
重试	Retry
重载	reload
重载别名	Reload Aliases
重量估算	Weight Estimate
量	Quantity
钮	Button
链	chain
锁定	lock
错	Error
错误	error
锚点	anchor
键	Key
镜	Mirror
镜像	mirror
长	Long
长仿宋	Long FangSong
长仿宋体	Long FangSong
长度	length
闭合	closed
间	between
间距须为正数	Spacing must be a positive number
间隔	interval
间隙	gap
阈值	threshold
阵	Array
阵列	array
阵列个数	Array Count
阵列复制	Array Copy
阶	Stage
附加	attached
附加外部参照	Attach External Reference
附着	attach
限	Limit
除	Remove
除外	excluded
随边界	with the boundary
隐私	Privacy
隐私说明	Privacy Notice
隐藏	hidden
隐藏栅格	Hide Grid
集	Set
零件总长	Overall Part Length
需	needs to
需要	requires
静态	static
非法	illegal
非箭头侧	non-arrow side
面	Panel
面图	view
面域特性	Region Properties
面板	Panels
面积	area
页	Page
页面设置	Page Setup
顶点	vertex
顶部	top
项	items
项变更	changes
项目名称	Project Name
顺	Order
须	must
须为	must be
须为正	must be positive
预览	preview
预览统计	Preview Statistics
预设	Preset
颈部总高	Neck Total Height
颜色	color
飞机三面图	Aircraft 3-View
首	Start
首先	first
首孔起始角	First Hole Start Angle
驱动	driving
驱动尺寸	driving dimension
高	High
高度	height
高级	Advanced
默认	Default
默认值	default value
齿廓	tooth profile
齿轮	gear
)I18N_DICT_END";

// 反转义 escape() 生成的 C 风格序列（\\ \" \n \r \t \0）。
QString unescape(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= s.size()) {
            out.append(c);
            continue;
        }
        const QChar n = s.at(++i);
        switch (n.unicode()) {
        case 'n':  out.append(QLatin1Char('\n')); break;
        case 't':  out.append(QLatin1Char('\t')); break;
        case 'r':  out.append(QLatin1Char('\r')); break;
        case '0':  out.append(QChar(0)); break;
        case '\\': out.append(QLatin1Char('\\')); break;
        case '"': out.append(QLatin1Char('"')); break;
        default:   out.append(n); break;
        }
    }
    return out;
}

QHash<QString, QString> parse(const char* data, int* maxKeyLen) {
    QHash<QString, QString> map;
    const QString blob = QString::fromUtf8(data);
    const QStringList rows = blob.split(QLatin1Char('\n'));
    int maxLen = 1;
    for (const QString& row : rows) {
        // 数据行格式：escape(key) TAB escape(value)；键里可能含 '='，
        // 值里可能含 '='，所以只能按第一个 TAB 切分。
        const int tab = row.indexOf(QLatin1Char('\t'));
        if (tab <= 0) continue;
        const QString key = unescape(row.left(tab));
        const QString val = unescape(row.mid(tab + 1));
        if (key.isEmpty()) continue;
        // 术语表里允许空译文（语法助词「的/了/把」等）：scrubByTerms 命中后
        // 直接丢弃该汉字，这正是动态拼接文案去掉中文残留所需要的。
        map.insert(key, val);
        if (key.size() > maxLen) maxLen = key.size();
    }
    if (maxKeyLen) *maxKeyLen = maxLen;
    return map;
}

} // namespace

const QHash<QString, QString>& exactMap() {
    static const QHash<QString, QString> map = parse(kExactData, nullptr);
    return map;
}

const QHash<QString, QString>& termMap() {
    static int maxLen = 1;
    static const QHash<QString, QString> map = parse(kTermData, &maxLen);
    return map;
}

int maxTermLength() {
    termMap();
    // parse() 已把最长键长写入静态 maxLen（见上方 static 局部）
    // 这里独立重算一次，避免暴露内部状态；成本 O(词条数) 且仅首次调用。
    static int cached = -1;
    if (cached < 0) {
        cached = 1;
        for (auto it = termMap().constBegin(); it != termMap().constEnd(); ++it)
            if (it.key().size() > cached) cached = it.key().size();
    }
    return cached;
}

} // namespace Runtime
} // namespace I18n
} // namespace SketchCAD
