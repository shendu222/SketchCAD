# SketchCAD 使用教程（分章完整版）· 总纲 OUTLINE

> 目标规模：32 章 + 2 附录，合计 11 万~13 万中文字。
> 章节文件命名：`第NN章_标题.md`（NN 两位补零）；附录为 `附录A_命令速查表.md`、`附录B_常见问题与故障排查.md`。
> 每章"素材指引"列出撰写时必读的仓库文件，撰写者须先读再写，禁止凭空编造命令参数。

## 第一部分 入门篇（第01~04章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 01 | 认识 SketchCAD | 软件定位与特性总览（轻量原生/国标优先/双模式交互/参数化模板/命令生态/扩展能力/工程自证）；安装（安装包/静默安装/环境要求）与启动；界面布局详解（菜单/工具栏/左侧Dock/画布/状态栏/命令行/参数输入条）；一个 5 分钟的"尝鲜"演示（画一个圆并标注）；本章不需要读者记住任何细节 | README.md、docs/用户手册.md §1-4、docs/images/gui_main.png |
| 02 | 绘制第一张图 | 从零到保存的完整闭环：新建图纸（幅面/比例/投影角）→ line 两点画线（GUI+命令行）→ circle_cr 画圆 → rect 画矩形 → 删除与撤销 → 保存 JSON → 导出 DXF；命令行语法总则（`命令 参数...`、坐标 `x,y` 写法、`help` 列全部命令）；实操案例：**平键垫片草图**（外轮廓矩形+中心圆+两条构造线） | 用户手册 §5.1/5.4/5.8/§8、README |
| 03 | 精确绘图：坐标、捕捉与追踪 | 世界坐标系（X右Y上、mm）；绝对/相对坐标概念；正交 F8；栅格 F9 与栅格吸附；极轴追踪 polar 与 otrack；对象捕捉（端点/中点/圆心/切点等）与捕捉设置对话框；tk 追踪；dyn 动态输入；"参数输入条"（角度/长度/间隔/条数）精确输入；案例：不用鼠标徒手画一个 100×60 精确矩形+对角线 | 用户手册 §5.4.8/§5.6、docs/snap-constraint-ortho-grid-design-analysis.md、docs/polar-otrack-tk-design-analysis.md、docs/snap-mode-management-design-analysis.md |
| 04 | 视图操作与图纸管理 | 滚轮缩放/右键拖拽平移；Ctrl+0 适配窗口；Ctrl+9 窗口缩放；视图方向 XY/YZ/XZ（Ctrl+1/2/3）；regen 重生成；多文档标签；图层面板初识；自动保存与崩溃恢复（%LOCALAPPDATA%\SketchCAD）；绘图比例 Ctrl+Shift+S 与图幅；案例：多视图检查一张法兰图 | 用户手册 §5.1/5.6/§9、docs/multi-doc-tabs-design-analysis.md、docs/crash-recovery-autosave-design-analysis.md、docs/units-limits-design-analysis.md |

## 第二部分 绘图篇（第05~10章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 05 | 直线家族 | line 两点线；line_h/line_v；line_angle/line_rel_angle；平行线/角平分线；切线系列（过点切圆等）；正交折线 line_snake 系；连续直线 K；freehand_line；线宽/颜色设置；案例：**轴的中心线+外轮廓**（点画线应用） | 用户手册 §5.4.1/§5.4.8、docs/xline-design-analysis.md（构造线）、CommandCatalog.cpp 中 line 系命令 |
| 06 | 圆与圆弧 | circle（圆心+半径）、circle_2p/3p、circle_cr、相切圆系、circle_inscribed 内切、circle_concentric 同心、circle_byarc；arc（圆心+起止角）、arc_3p 三点、arc_cpc、arc_tpa、椭圆弧 arc_ea 系；案例：**法兰同心圆组+螺栓孔定位圆** | 用户手册 §5.4.2/§5.4.3、docs/arc-drawing-design-analysis.md |
| 07 | 矩形、多边形与椭圆 | rect；rect_chamfer/rect_fillet；poly 自由多边形；poly_regular 正多边形（内接/外切/边长）；poly_star 星形；ellipse_1pt/ellipse_axis/ellipse_foci/ellipse_4p/ellipse_c3p/ellipse_inscribed；点 point；案例：**六角螺母+沉孔外形** | 用户手册 §5.4.4/§5.4.6、docs/ellipse-drawing-design-analysis.md |
| 08 | 样条与手绘曲线 | spline 与 spline_through_points、spline_from_polyline；抛物线 spline_parabola_4p；双曲线 spline_hyperbola；编辑：spline_append_pt/insert_pts/remove_pts/explode；freehand_line 手绘；工程上何时用样条（凸轮轮廓/流线）；案例：**凸轮升程曲线** | 用户手册 §5.4.5、CommandCatalog.cpp spline 系 |
| 09 | 折线与多段线 | polyline 绘制；节点编辑（add_node/append_node/delete_node/delete_between/trim_segments）；polyline_equidistant 等距偏移；polyline_from_segments；polyline_arcs_to_chords 弧转弦；段类型切换；pedit 详解（合并/闭合/宽度）；join 合并相邻图元；案例：**钣金折弯件轮廓（含圆弧段）** | 用户手册 §5.4.7、docs/polyline-edit-pedit-design-analysis.md、docs/polyline-drawing-design-analysis.md |
| 10 | 文字、表格与字段 | dtext 单行文本；mtext 多行文本与 MTextDialog；文字样式/字高；table 表格与 TableDialog；字段与公式（field/fieldupdate）；技术要求书写规范；案例：**给垫片图加技术要求与参数表** | 用户手册、docs/text-mtext-style-field-design-analysis.md、docs/table-element-design-analysis.md、docs/field-table-formula-design-analysis.md |

## 第三部分 编辑篇（第11~15章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 11 | 基础编辑命令 | move/move_copy、复制 Ctrl+Shift+D；rotate Ctrl+R 与 rotate 参考角；scale Ctrl+K（⚠️双绑定提示）；mirror（水平/竖直/自定义轴）与 mirrtext 语义；offset Ctrl+Shift+O 等距偏移；stretch 拉伸；align 对齐/对齐单条/对齐参考点；反向 Ctrl+Shift+R；案例：**对称支座的半图镜像** | 用户手册 §5.2、docs/mirror-design-analysis.md |
| 12 | 修剪、延伸与清理 | trim（T）边界裁剪；延长/裁剪到边界；break_at 打断；divide 等分；lengthen 拉长；join 合并；overkill 去重；chain 链选；Cleanup 清理对话框与 purge；案例：**十字交叉线修剪成十字接头** | 用户手册 §5.2、docs/batch-edit-join-array-clipboard-design-analysis.md |
| 13 | 倒角、圆角与阵列 | chamfer（距离）；fillet（半径）；rect_chamfer/rect_fillet 联动；arrayrect 矩形阵列（行/列/间距）；arraypolar 环形阵列（数量/夹角/中心）；arraypath 路径阵列；阵列对话框；案例：**端盖螺栓孔环形阵列+矩形通风孔阵列** | 用户手册 §5.2、docs/chamfer-fillet-design-analysis.md、docs/batch-edit-join-array-clipboard-design-analysis.md |
| 14 | 夹点编辑与对象属性 | 夹点（grip）选择与拖拽；属性面板 properties/prop；ddedit 编辑；matchprop 特性匹配；属性模板；explode 分解；案例：**用夹点微调支架形状** | 用户手册 §5.2、docs/grip-selection-design-analysis.md |
| 15 | 选择技术详解 | 窗口/交叉选择；全选/反选；select_entity 链选；选择相交；select_layer；按线型/颜色累加选择；deselect 系；qselect 快速选择对话框；group 编组与 GroupManager；filter 过滤器；invert；案例：**复杂装配图里只选中某层全部虚线** | 用户手册 §5.3、docs/qselect-design-analysis.md、docs/selection-group-qselect-filter-design-analysis.md、docs/group-filter-design-analysis.md |

## 第四部分 工程制图篇（第16~21章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 16 | 图层与对象特性 | 12 标准层（GB/T 14665 表6）；layer 图层管理面板；toggle_layer 开关；layiso/layuniso 隔离；laymch 归层；color/linetype/ltscale/width；线宽组别 G1~G5 与当前组切换；图层动态语义；案例：**按国标分层重排一张零件图** | 用户手册 §6、docs/CAD图层控制实现研究报告.md、docs/layer-operation-design-analysis.md、docs/layer-dynamic-semantics-design-analysis.md、docs/batch-layer-five-piece-design-analysis.md、docs/line-style-width-config-analysis.md |
| 17 | 尺寸标注 | 12 种标注类型逐个讲（dim/dim_smart/dim_aligned/dim_h/dim_v/dim_angle/dim_radius/dim_diameter/dim_baseline/dim_continue/dim_arc/dim_leader/dim_ordinate）；dim distance 命令行写法；dimstyle 标注样式；qdim 快速标注；tol 公差；GB/T 4458.4 与 ISO 129 差异与一键切换；标注数字按绘图比例换算（GB/T 14690）；案例：**轴的径向/长度/公差标注全流程** | 用户手册 §5.5/§6、docs/dimstyle-qdim-design-analysis.md、docs/gdt-naming-dimstyle-design-analysis.md、docs/标注标准切换-GB-T-ISO-实现说明.md、docs/dimension-driven-parametric-design-analysis.md |
| 18 | 剖面填充与剖切表达 | hatch/bhatch 剖面填充（点选区域/选图元）；hatchedit 编辑；剖面材料与 45° 剖面线（GB/T 4457.5）；肋/轮辐/标准件/狭小区域豁免规则；相邻零件剖面线反向；共享边界保护；剖切符号（GB/T 17452）A—A 剖切线；SectionDialog；案例：**轴承座全剖视图填充** | 用户手册 §6、docs/hatch-linkage-hatchedit-design-analysis.md、docs/hatch-pat-loading-design-analysis.md、docs/剖面填充点选-三处异常根因分析与修复说明.md |
| 19 | 图块、属性与外部参照 | block 定义块；insert 插入；BlockEditor 块编辑器；attdef/attedit/attlist 属性；wblock 写块；pasteblock 粘贴为块；DesignCenter 设计中心；xref 附加/分离（attach/detach/path/list 命令行；路径类型 full/relative/none；嵌套参照）；块导出炸开语义；案例：**标准螺栓图块库+标题栏块+外部参照底图** | 用户手册 §5.10、docs/block-design-analysis.md、docs/block-edit-design-center-design-analysis.md、docs/xref-attach-detach-design-analysis.md |
| 20 | 查询测量与数据提取 | dist 距离；area 面积（含加减模式）；massprop 质量属性；id 坐标；list 列表；measure 测量；InquiryDialog；BOM 树与明细栏；边界区域 boundary；qselect 联动；案例：**计算扳手截面积与重心** | 用户手册、docs/inquiry-measure-design-analysis.md、docs/boundary-region-design-analysis.md |
| 21 | 图框、标题栏与打印出图 | GB/T 14689 幅面 A0~A5 与图框；按幅面生成图框与标题栏；TitleBlockForm 标题栏填写；标准标题栏（GB/T 10609.1）；投影角（第一/三角）；PrintEngine 打印引擎；打印预览与保真；plot 出图与打印样式（plot style 映射）；PublishDialog 批量发布；layout 布局与多视口 mview；A3/A4 选型与自动比例；案例：**一张 A4 零件图从图框到 PDF** | 用户手册 §5.1/§6、docs/print-module-design-analysis.md、docs/plot-publish-design-analysis.md、docs/plot-style-mapping-design-analysis.md、docs/layout-multiviewport-design-analysis.md、docs/print-fidelity-fix-design-analysis.md |

## 第五部分 参数化制图模板篇（第22~25章）★案例主打

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 22 | 约束与参数化绘图 | 约束工具 N（重合/平行/垂直/相等/相切/水平/竖直/固定）；constraint 命令；perp/tangent2；qconnect 快速连接；ConstraintSolver 求解器原理（无 UI 依赖）；约束可视化（ConstraintBar）；尺寸驱动联动；案例：**约束驱动的可调矩形板** | 用户手册 §5.4.8、docs/osnap-constraint-design-analysis.md、docs/constraint-extension-design-analysis.md、docs/constraint-visualization-design-analysis.md、src/core/ConstraintSolver.h |
| 23 | 法兰盘参数化制图案例（机械） | 模板入口与对话框；DN100 PN16 带颈平焊法兰全参数（D=210/K=170/L=18/n=4/C=16/boreD=110/hubD=150/hubHeight=26、hasHub/recess、公差）；自动生成主/俯/左三视图+螺栓孔分布+标注+引线+图签；A4 与自动比例；标题栏避让；FlangeHubViews 法兰毂（Ø80/Ø60/沉孔Ø41/通孔Ø33/3-Ø11.5…）；修改参数再生成的工作流；案例：**从 PN16 改参数出 PN10 变体** | 用户手册 §5.7(2)(5)、docs/法兰盘三面图-参数说明.md、src/core/FlangeViews.h、src/core/FlangeHubViews.h、docs/images/flange_views.png、flangehub_views.png |
| 24 | 轴承与箱体参数化案例（机械） | 轴承座三面图（Ø130/孔Ø96/底板240×160×20/总高180/支承板105/U形槽55×36；主视+俯视+左视全剖）；BearingViews 轴承选型与 GB 标准数据库；箱体视图（主视全剖+A—A 断面+俯视 B+C 向；Ø82/Ø64H8/4×M4-7H/侧凸台Ø34）；粗糙度（GB/T 131）与技术要求；案例：**箱体改壁厚重新出图** | 用户手册 §5.7(3)(4)、src/core/BearingViews.h、src/core/HousingViews.h、docs/images/bearing_views.png、housing_views.png |
| 25 | 工艺三视图与户型图（航空+建筑） | 飞机三面图（单位**米**：机身38.90/翼展35.80/高6.13/垂尾5.99；正/侧/俯三视图）；工艺路线标注与尺寸链；FloorPlanViews 户型平面图（9200×11756、剪力墙200/隔墙100、门900/800/推拉1500、1:50、showFurniture/showDims 开关、图签）；makeValid 收敛与 validate 验证机制；案例：**出一张飞机三面图+改户型门宽** | 用户手册 §5.7(1)(6)、docs/户型平面图参数化绘制-算法与参数说明.md、src/core/AircraftViews.h、src/core/FloorPlanViews.h、docs/images/aircraft_threeview.png、floorplan_views.png |

## 第六部分 数据交换与协作篇（第26~27章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 26 | 文件格式详解 | JSON 原生格式结构（图元+样式完整往返）；DXF R12 读写（Ctrl+E 导出；$INSUNITS 单位；SPLINE 支持；GBK 中文）；STEP 只写（ISO 10303-21 AP214）；DWG 原生 R12 二进制读写（文件头/图层表/实体/CRC16；ACI 颜色近似；UTF-8 文本提示）；dxfout 命令；格式选型决策表；案例：**同一张图三种格式导出对比与回读验证** | 用户手册 §6、docs/json-refactor-design-analysis.md、docs/dxf-native-export-design-analysis.md、docs/dxf-import-expansion-design-analysis.md、docs/dwg-native-r12-design-analysis.md、docs/dwg-r2000-bridge-design-analysis.md |
| 27 | 与其他 CAD 软件协作 | 与 AutoCAD 的命令/功能对照；acad.pgp 别名兼容的意义；迁移工作流（DXF 中转）；编码（GBK vs UTF-8）与乱码处置；DWG R12 兼容面（AutoCAD/LibreCAD/QCAD/BricsCAD 可开）；已知差异与差距（参考 gap 分析报告）；案例：**SketchCAD → AutoCAD → 返程修改的完整往返** | docs/cad-vs-autocad-gap-analysis-2026-10.md、docs/acad-interaction-parity-design-analysis.md、docs/command-parity-design-analysis.md、docs/SketchCAD-主流CAD差异分析报告.md |

## 第七部分 二次开发篇（第28~32章）

| 章 | 标题 | 内容要点 | 素材指引 |
|---|---|---|---|
| 28 | 脚本与宏自动化 | ScriptHost 架构（JS 默认/Py/VBA 门控）；ScriptApi 统一交互面（全局对象 app：读写图元/执行命令/日志）；JS 脚本示例（画系列孔）；SCR 脚本（scr 命令逐行执行+STOP/CONTINUE 中断策略）；宏录制（macrorec/macrostop/macrosave/macroplay/macroshow）；捕获点=execute() 成功路径（鼠标操作不录制）；案例：**录一个"画法兰孔+标注"宏并改成参数化 JS** | docs/script-host-design-analysis.md、docs/macro-script-design-analysis.md、用户手册 |
| 29 | 命令别名与界面定制 | AliasTable（acad.pgp 兼容）；alias/aliaslist/aliasload/aliasreload/aliassave；.pgp 文件格式与手编；CUIx 语义（界面配置非行为逻辑）；cuiload/cuisave；工具条 CRUD（tbnew/tbadd/tbdel/tbrm）；工作空间（wsnew/wsload/wslist/wsdel）；别名只在首 token 展开的边界；案例：**为高频命令设单字母别名+自定义"航空制图"工作空间** | docs/cui-alias-design-analysis.md、src/core/AliasTable.h、src/core/CuiManager.h |
| 30 | SDK 插件开发 | SDK 定位与 ObjectARX 对照；ABI 版本协商（1..1）；插件契约（IPlugin 三方法+两个导出符号+SKETCHCAD_PLUGIN_ENTRY）；编译（g++ -shared -DSKETCHCAD_PLUGIN_BUILD）；plugins/ 目录自动加载与手动加载；IModel 方法速查；字符串生命周期陷阱；生命周期与异常屏障；SamplePlugin.cpp 解读；案例：**开发一个"齿轮齿廓"插件**（完整代码+CMake 集成） | docs/SDK_GUIDE.md、sdk/samples/SamplePlugin/SamplePlugin.cpp、sdk/include、docs/sdk-plugin-framework-design-analysis.md |
| 31 | COM 自动化与批量出图 | ProgID/CLSID/注册（regsvr32）；Start/OpenDrawing/PlotToPdf/ExportAs/BatchPlot/GetTaskStatus/RegisterCallback/Quit；错误码表；批量任务 JSON 规范；PrintSettings JSON（pageSize/orientation/margin/scale/fitToPage…）；事件回调（OnTaskStart/OnTaskDone/OnBatchFinished）；VBScript/PowerShell/Python/C# 四语言示例；稳定性保障（STA+互斥+异常屏障）；案例：**夜间批量出图流水线（Python 脚本+日志+事件进度）** | docs/COM_API.md、docs/com-automation-design-analysis.md |
| 32 | 源码架构与构建体系 | 分层架构（core 无 UI 依赖/ui 层/ModelMediator 中介）；Element 12 类图元与 SketchModel；CommandInterpreter 命令注册表；空间索引与增量渲染；构建（build.sh 全模式：build/test/dist/package；CMake 直构）；403 项自测 SketchSelfTest；bench 性能基准；工具链路径写死的注意事项；贡献代码的方向（对照 docs/二维CAD差距分析与AI编程实施方案.md）；案例：**跑通一次完整构建+自测+打包** | README.md、用户手册 §2-3、docs/spatial-index-incremental-render-design-analysis.md、docs/二维CAD差距分析与AI编程实施方案.md、build.sh、CMakeLists.txt |

## 附录

| 文件 | 内容 | 素材指引 |
|---|---|---|
| 附录A 命令速查表 | 按 CommandCatalog 全量命令分类列表：命令名/语法示例/一句话说明/所属章；快捷键总表 | src/core/CommandCatalog.cpp、用户手册 §7 |
| 附录B 常见问题与故障排查 | 启动/显示/打印/格式/性能/二次开发 六类 FAQ；错误码速查；数据目录与日志位置 | 用户手册 §9、README FAQ、COM_API.md 错误码 |

## 撰写调度（子任务分批）

| 批次 | 章节 | 预估字数 |
|---|---|---|
| B1 | 第01~04章（入门篇） | 1.8万 |
| B2 | 第05~07章 | 1.4万 |
| B3 | 第08~10章 | 1.4万 |
| B4 | 第11~13章 | 1.4万 |
| B5 | 第14~15章+附录A | 1.5万 |
| B6 | 第16~18章 | 1.5万 |
| B7 | 第19~21章 | 1.5万 |
| B8 | 第22~23章 | 1.3万 |
| B9 | 第24~25章 | 1.3万 |
| B10 | 第26~27章 | 1.2万 |
| B11 | 第28~29章 | 1.3万 |
| B12 | 第30~31章 | 1.5万 |
| B13 | 第32章+附录B | 0.9万 |

样板章：第02章（先行单独撰写，作为各批次文风基准）。
