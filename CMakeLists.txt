cmake_minimum_required(VERSION 3.16)
project(SketchCAD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 安装路径（mingw_64 构建，与 mingw1310 ABI 匹配）
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.1/mingw_64/lib/cmake")
# 编译器：Qt 自带 MinGW-w64 GCC 13.1.0
set(CMAKE_C_COMPILER   "C:/Qt/Tools/mingw1310_64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/Qt/Tools/mingw1310_64/bin/g++.exe")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---- DRY：公共核心源文件列表，供所有目标共用 ----
set(CORE_SOURCES
    src/core/Element.cpp
    src/core/ElementFactory.cpp
    src/core/Commands.cpp
    src/core/Geometry.cpp
    src/core/DimStyle.cpp
    src/core/Trim.cpp
    src/core/Snap.cpp
    src/core/SketchModel.cpp
    src/core/CommandInterpreter.cpp
    src/core/LineModes.cpp
    src/core/CircleModes.cpp
    src/core/ArcModes.cpp
    src/core/PolyModes.cpp
    src/core/SplineModes.cpp
    src/core/EllipseModes.cpp
    src/core/LineStyleConfig.cpp
    src/core/LineWidthGroup.cpp
    src/core/LinePattern.cpp
    src/core/ProjectionSymbol.cpp
    src/core/SurfaceRoughness.cpp
    src/core/SectionCutSymbol.cpp
    src/core/SectionDrawing.cpp
    src/core/ConstraintSolver.cpp
    src/core/ToolRegistry.cpp
    src/core/sheet_frame.cpp
    src/core/Modify.cpp
    src/core/AircraftViews.cpp
    src/core/FlangeViews.cpp
    src/core/BearingViews.cpp
    src/core/HousingViews.cpp
    src/core/FlangeHubViews.cpp
    src/core/FloorPlanViews.cpp
    src/core/Section.cpp
    src/core/CadFormats.cpp
)
set(UI_SOURCES
    src/ui/GraphicsView.cpp
    src/ui/ModelMediator.cpp
    src/ui/tools/ToolContext.cpp
    src/ui/tools/ToolManager.cpp
    src/ui/tools/Tools.cpp
    src/ui/tools/FreehandLineTool.cpp
    src/ui/tools/SplineEditTool.cpp
    src/ui/tools/PolylineTool.cpp
    src/ui/tools/PolylineEditTool.cpp
    src/ui/RulerWidget.cpp
    src/ui/BomTree.cpp
    src/ui/ElementPanel.cpp
    src/ui/PropertyPanel.cpp
    src/ui/LayerPanel.cpp
    src/ui/LogPanel.cpp
    src/ui/CommandLine.cpp
    src/ui/StatusBar.cpp
    src/ui/NewDrawingDialog.cpp
    src/ui/PaperSizeDialog.cpp
    src/ui/TitleBlockForm.cpp
    src/ui/ModifyInputDialog.cpp
    src/ui/AircraftViewsDialog.cpp
    src/ui/FlangeViewsDialog.cpp
    src/ui/BearingViewsDialog.cpp
    src/ui/HousingViewsDialog.cpp
    src/ui/FlangeHubViewsDialog.cpp
    src/ui/FloorPlanViewsDialog.cpp
    src/ui/ArrayDialog.cpp
    src/ui/SectionDialog.cpp
)

# ---- 主程序 ----
add_executable(SketchCAD
    src/main.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SketchCAD PRIVATE Qt6::Widgets)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SketchCAD PRIVATE -Wall -Wextra)

# ---- 选择菜单 UI 回归（offscreen 构造完整 MainWindow，验证菜单结构/图标/快捷键/行为闭环）----
# 与 SketchCAD 主程序同样的依赖：MainWindow 依赖全部 UI 源 + 核心层
add_executable(SelectMenuTest
    tests/selection_menu_test.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SelectMenuTest PRIVATE Qt6::Widgets)
target_include_directories(SelectMenuTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SelectMenuTest PRIVATE -Wall -Wextra)

# ---- 修改菜单 UI 回归（offscreen 构造 MainWindow，验证【修改】菜单结构/图标/行为闭环/一次 undo 回滚）----
add_executable(ModifyMenuTest
    tests/modify_menu_test.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(ModifyMenuTest PRIVATE Qt6::Widgets)
target_include_directories(ModifyMenuTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ModifyMenuTest PRIVATE -Wall -Wextra)

# ---- 绘图菜单结构回归（offscreen 构造 MainWindow，验证去重后【绘图】菜单：
#      7 个图元子菜单 + 尺寸标注子菜单存在且非空、扁平重复项已移除、修改组不再平铺、
#      无同名重复、子功能入口未丢失）----
add_executable(DrawingMenuTest
    tests/drawing_menu_test.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(DrawingMenuTest PRIVATE Qt6::Widgets)
target_include_directories(DrawingMenuTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(DrawingMenuTest PRIVATE -Wall -Wextra)

# ---- 工具条两行布局回归（offscreen 构造 MainWindow，用几何数据验证两行不溢出/不重叠/按钮完整）----
add_executable(ToolbarLayoutTest
    tests/toolbar_layout_test.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(ToolbarLayoutTest PRIVATE Qt6::Widgets)
target_include_directories(ToolbarLayoutTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ToolbarLayoutTest PRIVATE -Wall -Wextra)

# ---- 图纸文件「新建 → 存盘 → 读回」端到端往返（offscreen 驱动 MainWindow 的无对话框接缝）----
# 覆盖 JSON/DXF/STEP/DWG 四格式往返 + 比例随图往返 + 失败路径。
# 这类胶水以前零覆盖：saveFile 的写盘段被整段截断也能编译通过、且无测试变红。
add_executable(FileRoundTripTest
    tests/file_roundtrip_test.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(FileRoundTripTest PRIVATE Qt6::Widgets)
target_include_directories(FileRoundTripTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(FileRoundTripTest PRIVATE -Wall -Wextra)

# ---- 绘图比例的渲染级验证（像素级：图形真的缩小、图框真的不动）----
# 数据层断言只能证明"坐标变了"；用户要的是"画出来的图变小了"。
# 用 fitToRect 把视图变换钉死（与内容无关），再用颜色分离被测图元与图框来量像素长度。
add_executable(ScaleRenderTest
    tests/scale_render_test.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(ScaleRenderTest PRIVATE Qt6::Widgets)
target_include_directories(ScaleRenderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ScaleRenderTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「图形级」人工核验（离屏渲染 PNG，供直观看分割线保留）----
add_executable(SectionRenderVerify
    tests/section_render_verify.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SectionRenderVerify PRIVATE Qt6::Widgets)
target_include_directories(SectionRenderVerify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionRenderVerify PRIVATE -Wall -Wextra)

# ---- DXF 读入路径的互操作测试 ----
# 甲）自研往返：弧段多段线（bulge）存盘再打开必须无损；
# 乙）第三方互操作：读 ezdxf 写出的样例（CRLF/LF、GBK 中文、句柄组码 5、Z 组码 30 等
#     本程序从不写出的成分），确认读入端不依赖"自己写的才读得懂"。
# 样例由 tools/make_foreign_dxf.py 生成并提交（tests/data/），测试本身不依赖 Python。
add_executable(DxfImportTest
    tests/dxf_import_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(DxfImportTest PRIVATE Qt6::Widgets)
target_include_directories(DxfImportTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(DxfImportTest PRIVATE -Wall -Wextra)

# ---- 法兰盘三面图（参数化模板）----
# 分四层验证：结构完整性（同心圆/孔数/三视图齐全）、投影对齐（长对正/高平齐/宽相等）、
# 参数敏感性（改尺寸几何必须跟着变）、结构不变式（非法参数必须收敛而非画出错误几何）、
# 图面合规（标准比例 + A4 内 + 不与标题栏冲突）。同时写出 PNG 预览供人工核对。
add_executable(FlangeViewsTest
    tests/flange_views_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(FlangeViewsTest PRIVATE Qt6::Widgets)
target_include_directories(FlangeViewsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(FlangeViewsTest PRIVATE -Wall -Wextra)

# 轴承座三面图（左视图全剖）回归：参数→几何不变式、投影对齐（长对正/高平齐）、
# 左视图 Hatch 断面数量与 GB 剖面线（45° 细实线、同一零件同方向）、不含引线引导线。
add_executable(BearingViewsTest
    tests/bearing_views_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(BearingViewsTest PRIVATE Qt6::Widgets)
target_include_directories(BearingViewsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(BearingViewsTest PRIVATE -Wall -Wextra)

# 箱体零件图（四视图：主视图全剖/俯视图 B/A—A 断面/C 向视图）回归：参数→几何不变式、
# 投影对齐（长对正/高平齐）、剖面线（GB/T 4457.5）、粗糙度(GB/T 131)、剖切符号(GB/T 17452)、
# 以及参数化剖面接口 SectionViewBuilder / makeRingSection。
add_executable(HousingViewsTest
    tests/housing_views_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(HousingViewsTest PRIVATE Qt6::Widgets)
target_include_directories(HousingViewsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(HousingViewsTest PRIVATE -Wall -Wextra)

# 法兰盘轮毂视图（主视图全剖 A—A + 左视图圆形）回归：参数→几何不变式、
# 投影对齐（两视图轴线等高、几何随参数变化）、GB 剖面线/剖切符号/粗糙度、
# 以及结构不变式（非法参数必须收敛而非画出错误几何）。同时写出 PNG 预览供人工核对。
add_executable(FlangeHubViewsTest
    tests/flange_hub_views_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(FlangeHubViewsTest PRIVATE Qt6::Widgets)
target_include_directories(FlangeHubViewsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(FlangeHubViewsTest PRIVATE -Wall -Wextra)

# 户型平面图（建筑装修专业）回归：墙体/门窗/家具/尺寸链/图签的参数化生成，
# 结构不变式（非法参数收敛）、比例选取（A3 → 1:50）、落框检查（fits）。
# 同时写出 PNG 预览供人工核对。
add_executable(FloorPlanViewsTest
    tests/floor_plan_views_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(FloorPlanViewsTest PRIVATE Qt6::Widgets)
target_include_directories(FloorPlanViewsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(FloorPlanViewsTest PRIVATE -Wall -Wextra)

# ---- 图层（Layer）管理单元测试（核心层，无需 GUI）----
# 验证：默认层 0、新建/重名/删除/改名、选中图元分配、批量改属性仅影响本层、
# 显示控制仅翻转本层、删层回落/改名迁移、撤销、图层表序列化往返与旧文件兼容。
add_executable(LayerTest
    tests/layer_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(LayerTest PRIVATE Qt6::Widgets)
target_include_directories(LayerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(LayerTest PRIVATE -Wall -Wextra)

# ---- 活动图层（current layer）逻辑：默认层 / 切换继承 / 已有图元不变 ----
add_executable(LayerCurrentTest
    tests/layer_current_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(LayerCurrentTest PRIVATE Qt6::Widgets)
target_include_directories(LayerCurrentTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(LayerCurrentTest PRIVATE -Wall -Wextra)

# ---- 一次性预览：渲染「当前图层」HUD 徽标（非回归测试，人工核对用）----
add_executable(LayerHudPreview
    tests/layer_hud_preview.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(LayerHudPreview PRIVATE Qt6::Widgets)
target_include_directories(LayerHudPreview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(LayerHudPreview PRIVATE -Wall -Wextra)

# ---- 标题栏插入 + 累加选择（核心层回归）----
add_executable(TitleBlockTest
    tests/title_block_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(TitleBlockTest PRIVATE Qt6::Widgets)
target_include_directories(TitleBlockTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(TitleBlockTest PRIVATE -Wall -Wextra)

# ---- 按幅面自动套框 + 标题栏（核心层回归）----
# 验证 buildAutoFrameTitleBlock：边框=max(所选幅面, 内容+边距)，含内容、避让标题栏、必要字段齐备、纯函数。
add_executable(AutoFrameTest
    tests/auto_frame_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(AutoFrameTest PRIVATE Qt6::Widgets)
target_include_directories(AutoFrameTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(AutoFrameTest PRIVATE -Wall -Wextra)

# ---- 阵列复制（矩形 / 环形）核心层回归 ----
# 验证 makeRectangularArray / makePolarArray：原图元保留、副本数正确、几何位置/角度正确、
# 属性（图层/颜色/线宽/线型/文本）完整继承、Arc 起止角与 Ellipse 旋转角随旋转同步。
add_executable(ArrayTest
    tests/array_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(ArrayTest PRIVATE Qt6::Widgets)
target_include_directories(ArrayTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ArrayTest PRIVATE -Wall -Wextra)

# ---- 环形阵列中心点「双模式指定」回归（手动输入 / 视图拾取，含自由切换一致性）----
# ArrayDialog 属于 UI 层（src/ui/），需显式加入源文件（CORE_SOURCES 不含 UI）。
add_executable(ArrayDialogTest
    tests/array_dialog_test.cpp
    src/ui/ArrayDialog.cpp
    ${CORE_SOURCES}
)
target_link_libraries(ArrayDialogTest PRIVATE Qt6::Widgets)
target_include_directories(ArrayDialogTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ArrayDialogTest PRIVATE -Wall -Wextra)

# ---- 剖面区域表示（GB/T 4457.5-2013）核心层回归 ----
# 验证 geo::hatchLines（间距/角度/内孔裁剪）、Section 引擎（豁免决策/相邻区分/轮廓转换/往返）。
add_executable(SectionTest
    tests/section_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionTest PRIVATE Qt6::Widgets)
target_include_directories(SectionTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「写入路径」集成回归 ----
# section_test.cpp 只覆盖纯算法，走不到 Change → applyChanges → SketchModel 这条
# 落盘链路；本目标用真实 SketchModel 复现 UI 的 sectionFill 流程，锁死
# 「圆被同 ID 的 Hatch 替换」以及「replacement.id 漏赋值仍须按 Change::id 替换」。
add_executable(SectionApplyTest
    tests/section_apply_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionApplyTest PRIVATE Qt6::Widgets)
target_include_directories(SectionApplyTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionApplyTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「闭合性」回归 ----
# 锁定：开放折线/开放样条/孤立圆弧必须安全跳过（不产生 Hatch）；直线+圆弧+样条
# 等任意图元首尾相接围成的闭环必须成功填充（filledRegions==1）；且生成的所有剖面
# 线段两端点落在边界上、中点在内部（不渗漏/不溢出）；带孔环孔心无剖面线。
add_executable(SectionClosureTest
    tests/section_closure_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionClosureTest PRIVATE Qt6::Widgets)
target_include_directories(SectionClosureTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionClosureTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「闭合性」对抗性边界回归（QA 独立验证）----
# 独立于工程师结论，从反例角度施压：端点容差边界(0.49/0.50/0.51)、自交(零面积蝴蝶结/
# 非零面积自交四边形)、多环互不干扰、内外环方向不一致的掏空、圆角矩形(4弧+4直线)圆角零
# 溢出、退化输入安全跳过、带孔环回归、开放点列下游防御拒绝填充。
add_executable(SectionClosureEdgeTest
    tests/section_closure_edge_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionClosureEdgeTest PRIVATE Qt6::Widgets)
target_include_directories(SectionClosureEdgeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionClosureEdgeTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「区域点选」回归（点哪填哪）----
# 交互契约：点击剖面填充 → 区域选择模式，按鼠标点自动识别并只填充该点所在的封闭区域。
# 覆盖：相邻区域隔离（不填到隔壁）、多图元链式成环、嵌套闭环→内孔（环形 vs 内环）、
# 多内孔与最内层优先、端点容差边界、开放轮廓安全跳过、已填充不重复、圆弧链式成环零溢出、
# 极窄区域自动加密兜底。
add_executable(RegionPickTest
    tests/region_pick_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(RegionPickTest PRIVATE Qt6::Widgets)
target_include_directories(RegionPickTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(RegionPickTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「边界交叉细分」测试 ----
# 锁定两类修复诉求：(1) 边界线相互交叉(非端点相接)时四边形环必须能建出、圆外子区域
# 可点选；(2) 自闭合矩形被一条贯穿竖线分隔时，左右两侧各自成环、互不吞并。
add_executable(SectionCrossingTest
    tests/section_crossing_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionCrossingTest PRIVATE Qt6::Widgets)
target_include_directories(SectionCrossingTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionCrossingTest PRIVATE -Wall -Wextra)

# ---- 剖面填充「点选异常复现诊断」（复现诊断，不修复） ----
# 复现用户报告的三个(及一个补充)T 形/分割线异常行为：①矩形+贯穿竖线，填充后再进入同
# 一区域是否被重新划分；②菱形+斜贯穿线，拆两半填充是否仍被当成整体；③填充后分割线是否被
# 错误剪切；④分割线端点恰好落在矩形边上(T 形)两种变体。仅记录期望 vs 实际，不改核心代码。
add_executable(SectionPickAnomalyTest
    tests/section_pick_anomaly_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionPickAnomalyTest PRIVATE Qt6::Widgets)
target_include_directories(SectionPickAnomalyTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionPickAnomalyTest PRIVATE -Wall -Wextra)

# ---- 三处新异常复现诊断（悬出段分割线/曲线，2026-09-18 截图） ----
add_executable(SectionRepro3Test
    tests/section_repro3.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SectionRepro3Test PRIVATE Qt6::Widgets)
target_include_directories(SectionRepro3Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SectionRepro3Test PRIVATE -Wall -Wextra)

# ---- 无界面自测（核心层回归） ----
add_executable(SketchSelfTest
    tests/selftest.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SketchSelfTest PRIVATE Qt6::Widgets)
target_include_directories(SketchSelfTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SketchSelfTest PRIVATE -Wall -Wextra)

# ---- 视图交互集成测试（offscreen 下注入合成鼠标事件，验证右键拖拽平移） ----
# GraphicsView 依赖 ToolManager/ToolContext/Tools，需要一起编译
set(TOOL_SOURCES
    src/ui/tools/ToolContext.cpp
    src/ui/tools/ToolManager.cpp
    src/ui/tools/Tools.cpp
    src/ui/tools/FreehandLineTool.cpp
    src/ui/tools/SplineEditTool.cpp
    src/ui/tools/PolylineTool.cpp
    src/ui/tools/PolylineEditTool.cpp
)
add_executable(ViewPanTest
    tests/view_pan_test.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(ViewPanTest PRIVATE Qt6::Widgets)
target_include_directories(ViewPanTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ViewPanTest PRIVATE -Wall -Wextra)

# ---- 渲染级验证：平移是否真正驱动画面（offscreen 渲染两帧比质心位移） ----
add_executable(ViewRenderTest
    tests/view_render_test.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(ViewRenderTest PRIVATE Qt6::Widgets)
target_include_directories(ViewRenderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ViewRenderTest PRIVATE -Wall -Wextra)

# ---- 画布框选集成测试（合成鼠标事件驱动窗口选择/取消，守住"零面积包围盒框不中"回归）----
add_executable(SelectWindowTest
    tests/select_window_test.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(SelectWindowTest PRIVATE Qt6::Widgets)
target_include_directories(SelectWindowTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SelectWindowTest PRIVATE -Wall -Wextra)

# ---- 渲染级根因验证：像素对比（矩阵正确≠画面跟随，本测试直接比两帧渲染质心） ----
add_executable(ViewRenderTest2
    tests/view_render_test2.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(ViewRenderTest2 PRIVATE Qt6::Widgets)
target_include_directories(ViewRenderTest2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ViewRenderTest2 PRIVATE -Wall -Wextra)

# ---- 适配窗口 / 窗口缩放 集成测试（合成鼠标事件：内容精确充满视口 + 框选放大）----
add_executable(ViewFitZoomTest
    tests/view_fit_zoom_test.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(ViewFitZoomTest PRIVATE Qt6::Widgets)
target_include_directories(ViewFitZoomTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(ViewFitZoomTest PRIVATE -Wall -Wextra)

# ---- GB/T 14665-2012 整改回归（T1/T2/T3/T5/T6/T10/T13/T14/T16/T17/T20）----
# GbLineTest：线宽组别(表1)、线型齐备(4.1)、线素长度(4.2)、相位对齐(4.4)、重合优先(4.3)。
add_executable(GbLineTest
    tests/gb_line_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(GbLineTest PRIVATE Qt6::Widgets)
target_include_directories(GbLineTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(GbLineTest PRIVATE -Wall -Wextra)

# GbSheetTest：图框边距(GB/T 14689 表4)、对中符号(6.1)、投影符号(第7章)、
#   字高与图幅(5.6 表3)、比例系列(GB/T 14690)。
add_executable(GbSheetTest
    tests/gb_sheet_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(GbSheetTest PRIVATE Qt6::Widgets)
target_include_directories(GbSheetTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(GbSheetTest PRIVATE -Wall -Wextra)

# LayerTemplateTest：标准图层模板（表6 标识号 01~12）。
add_executable(LayerTemplateTest
    tests/layer_template_test.cpp
    ${CORE_SOURCES}
)
target_link_libraries(LayerTemplateTest PRIVATE Qt6::Widgets)
target_include_directories(LayerTemplateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(LayerTemplateTest PRIVATE -Wall -Wextra)

# ---- 飞机三面图预览：生成 PNG 供人工核对 ----
add_executable(AircraftPreviewTest
    tests/aircraft_preview_test.cpp
    ${CORE_SOURCES}
    src/ui/GraphicsView.cpp
    ${TOOL_SOURCES}
)
target_link_libraries(AircraftPreviewTest PRIVATE Qt6::Widgets)
target_include_directories(AircraftPreviewTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(AircraftPreviewTest PRIVATE -Wall -Wextra)
