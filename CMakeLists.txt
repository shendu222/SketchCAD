cmake_minimum_required(VERSION 3.16)
# 版本号单一来源（semver）。此处是全项目「版本真相」：
#   → configure_file 生成 build/version.h（供「关于」对话框等 C++ 侧读取）
#   → configure_file 生成 build/app.rc（注入 exe 文件属性 VERSIONINFO）
#   → build.sh package 模式读取本行，拼出 SketchCAD-<版本>-Setup.exe
# 三者（关于对话框 / exe 属性 / 安装包文件名）因此天然一致，不会各自漂移。
project(SketchCAD VERSION 1.3.6 LANGUAGES CXX)

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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Qml Concurrent LinguistTools Sql)

# 命令目录 JSON 随构建产物复制到 <build>/config，程序启动从 exe 同级 config 读取。
configure_file(config/command_catalog.json ${CMAKE_BINARY_DIR}/config/command_catalog.json COPYONLY)

# ---- DRY：公共核心源文件列表，供所有目标共用 ----
set(CORE_SOURCES
    src/core/Element.cpp
    src/core/ElementValidate.cpp   # 鲁棒性守卫层：读档前图元只读校验/丢弃/修复（不动几何算法）
    src/core/ElementFactory.cpp
    src/core/standards/ToleranceDB.cpp  # ISO 公差数据库：IT01~IT18 标准公差 + 轴/孔基本偏差（GB/T 1800.1 / ISO 286-1）
    src/core/standards/ComplianceChecker.cpp  # 模型合规检查器：图层命名合规（GB/T 18617 / ISO 13567）
    src/core/Layer.cpp                  # 图层结构与 GB/T 18617 图层命名建议/合规判定（伴 Layer.h）
    src/core/ScriptApi.h
    src/core/ScriptApi.cpp
    src/core/JavaScriptEngine.cpp
    src/core/ScriptHost.cpp
    src/core/SdkContext.cpp
    src/core/PluginManager.cpp
    src/core/Commands.cpp
    src/core/Geometry.cpp
    src/core/DimStyle.cpp
    src/core/Trim.cpp
    src/core/ChamferFillet.cpp
    src/core/PolylineOps.cpp
    src/core/Inquiry.cpp
    src/core/Snap.cpp
    src/core/SketchModel.cpp
    src/core/CommandInterpreter.cpp
    src/core/CommandCatalog.cpp       # 命令元数据目录 + 面板纯逻辑 + 快捷键审计（纯数据层，可无头直测）
    src/core/CommandCatalogConfig.cpp # JSON 命令目录读取/校验/保存（启动时驱动 add 注册）
    src/core/BomItem.cpp           # P2：序号球标纯生成器 + BomItem 数据模型
    src/core/InvoluteGear.cpp       # P4：渐开线齿轮齿廓采样生成器（G1 公式）
    src/core/PartLibrary.cpp        # P4：零件库抽象（PartGenerator 接口 + GearGenerator 内建）
    src/core/StdPartCatalog.cpp     # GB 标准件参数化目录（DWG 图库解析驱动 + 参数驱动出图）
    src/core/StdPartTemplate.cpp    # DWG 原图模板库（占位符替换式参数化出图）
    src/core/StdPartInstance.cpp    # 标准件参数实例 JSON（类型/参数/定位 + 重渲染）
    src/core/GbFastenerTable.cpp    # P5：GB 紧固件规格表（六角螺栓/螺母/平垫圈，人工给定 G3）
    src/core/ViewTools.cpp          # P6：视图工具（向视符号/孔轴投影/转角投影生成器）
    src/core/SectionProps.cpp       # P7：截面特性（Green 定理标准公式，G1 待人工校核）
    src/core/DensityTable.cpp       # P7：材料密度表（人工提供，禁改数值 G3）
    src/core/BatchBom.cpp           # P7：批量 BOM 汇总（只读顶层 bom 键，不实例化模型）
    src/core/VarParam.cpp           # P8：变量表达式层（会话级变量表 + 轻量安全求值器 + 拓扑重算）
    src/core/BatchTools.cpp         # P9：扩展工具（机械图拆分/自动排图 shelf 装箱/批量签名）
    src/core/FeatureMenu.cpp        # P1~P9 功能菜单与准入（目录单一数据源 + 纯逻辑条件评估）
    src/core/MacroScript.cpp
    src/core/AliasTable.cpp
    src/core/CuiManager.cpp
    src/core/CuiAliasCommands.cpp
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
    src/core/TitleBlockLayout.cpp  # 标题栏单元格文字自适应（度量→收缩→定位）+ 网格重建（编辑期/自测共用）
    src/core/SurfaceRoughness.cpp
    src/core/weld/WeldSymbol.cpp       # 焊缝符号（GB/T 324）：buildWeldSymbol 组合既图元 + WELD 命令几何生成
    src/core/CenterLineGen.cpp   # P1 中心线家族生成器（10 种，纯函数）
    src/core/SymbolFactory.cpp   # P3 符号标注扩展（datum/weld/taper/centerhole/elevation/rotation 6 类纯生成器）
    src/core/ConstraintSolver.cpp
    src/core/LegacyConstraintSolver.cpp   # SKT 集成 Phase B：修复模型委托实现（IConstraintSolver）
    src/core/ConstraintSolverFacade.cpp   # SKT 集成 Phase B：路由门面（特性开关+回落，单例）
    src/core/ToolRegistry.cpp
    src/core/ShapeSelect.cpp
    src/core/GripOps.cpp
    src/core/sheet_frame.cpp
    src/core/SheetFrameTemplate.cpp  # 【新建图框】图框布置模板持久化（独立 JSON，需求 13）
    src/core/Modify.cpp
    src/core/UndoDelta.cpp        # 撤销栈差量化（变更集 + 每 50 步全量检查点）
    src/core/Clipboard.cpp
    src/core/Cleanup.cpp
    src/core/Section.cpp
    src/core/HatchLibrary.cpp
    src/core/Gdt.cpp   # GD&T: 多重引线(MLeader) + 形位公差框格(Fcf) 几何生成器
    src/core/Block.cpp
    src/core/Xref.cpp
    src/core/CadFormats.cpp
    src/core/ProxyGraphics.cpp   # D1b 代理图形提取（decodeProxyGraphicsBlob / extractProxyGraphics）
    src/core/PlotStyle.cpp
    src/core/Layout.cpp
    src/core/TextStyle.cpp
    src/core/TextMetrics.cpp   # 文字度量单一数据源（屏幕/打印/取景三处同源）
    src/core/ZwcadTextLayout.cpp  # ZWCAD 规范文字排版引擎（段落/缩进/换行/5-3行距/9点对齐唯一入口）
    src/core/QSelect.cpp
    src/core/Units.cpp
    src/core/SpatialIndex.cpp
    src/core/AppSettings.cpp   # 全局设置（自动保存间隔/磁盘阈值，HKCU\Software\SketchCAD）
    src/core/I18n.cpp          # 界面语言单一入口（语言解析 + QTranslator 装载，自测直测）
    src/core/I18nDict.cpp      # 生成式中文 UI→英文词典（tools/i18n/build_i18n.py 产出，勿手改）
    src/core/AutoSave.cpp      # 自动保存核心（定时+操作计数双触发，原子落盘，可恢复）
    src/core/RecentFiles.cpp   # 最近文件 MRU（路径+时间+pin，复用 AppSettings 存储，可离线直测）
    src/core/Diagnostics.cpp   # 诊断：Qt 日志滚动落盘 + Windows 崩溃 mini-dump + 会话标记
    src/core/DiagBundle.cpp    # 诊断包：Stored-ZIP 写入器 + 打包编排（纯 core 可无头直测）
    src/core/Telemetry.cpp     # 遥测出口（默认关闭零外发；开关+出口双门闩）
    src/core/DrawingDb.cpp     # 绘图数据库存储层（SQLite，启动静默连接；图档+模板 CRUD）
    # ---- AI 自然语言助手（LLM 嵌入；云端/本地/离线 Mock 可切换）----
    # LlmProvider 为含 Q_OBJECT 的抽象基类，薄 .cpp 接管 moc 生成的 vtable/metaobject
    src/ai/LlmProvider.cpp
    src/ai/OpenAiProvider.cpp  # OpenAI 兼容后端（/v1/chat/completions + SSE 流式）
    src/ai/MockProvider.cpp    # 离线规则后端（无 key 联调 / 无头自测）
    src/ai/tooldefs.cpp        # 工具 schema（查询工具 + runSketchCommand 动作工具）
    src/ai/AppContextProvider.cpp  # 文档状态采集（选择/图层/视图）
    src/ai/NlAssistant.cpp      # 编排层：LLM ⇄ 工具 ⇄ interp_->execute 循环
    src/ai/NlCommandMapper.cpp  # LLM 响应结构化解码（兼容非 function-calling 后端）
    src/core/standards/StandardsDB.cpp  # 标准知识库：12 类国标表装载/查询（QResource 内嵌 JSON）
)

# ---- D1：DWG 外部转换器隔离层（独立静态库目标）----
# DwgBridge 只封装 QProcess 进程外调用 + 临时文件中转，**不含任何转换器头文件/
# 代码**（LibreDWG 是独立 GPL exe，转换器本体在 ../../thirdparty/libredwg-0.14，
# 独立构建，绝不进入本工程的任何目标）。独立成库的目的是物理隔离：UI 层与
# 主程序其他源文件无法绕过桥直接触碰转换器，链接边界即审计边界。
add_library(sketchcad_dwgbridge STATIC src/core/DwgBridge.cpp)
# 兼容旧目标的链接名；实际构建产物与编译设置均归 sketchcad_dwgbridge 所有。
add_library(SketchDwgBridge ALIAS sketchcad_dwgbridge)
# Qt6::Gui：CadFormats.h → Element.h 链上引用 QColor（仅取头路径，桥逻辑本身只用 Core）
target_link_libraries(sketchcad_dwgbridge PUBLIC Qt6::Core Qt6::Gui)
target_include_directories(sketchcad_dwgbridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(sketchcad_dwgbridge PRIVATE -Wall -Wextra)

set(UI_SOURCES
    src/ui/GraphicsView.cpp
    src/ui/ActionFactory.h      # QAction 统一工厂（Builder 规格 + Factory Method）
    src/ui/CommandCatalogEditor.cpp # 命令目录 JSON 维护界面（命令/参数 CRUD）
    src/ui/UiMetrics.cpp        # 界面密度与响应式尺寸单一数据源
    src/ui/PenRecipes.cpp       # 派生画笔配方（尺寸细实线 / 追踪引导线）
    src/ui/DeleteKeyFilter.cpp  # Del 键应用级兜底：绕开 QShortcutMap 吞键，纯 Qt Widgets 依赖
    src/ui/ThemeManager.cpp     # 内置亮/暗主题单例（setColorScheme + 画布背景色 + themeChanged 广播）
    src/ui/DrawingTheme.cpp     # 绘图区域全套主题配色（网格/坐标轴/引导线/HUD/徽标/选中/约束…）
    src/ui/ElementPath.cpp
    src/ui/ModelMediator.cpp
    src/ui/DocTab.cpp               # 多文档标签：DocSession/DocManager（纯会话核心，可离线直测）
    src/ui/tools/ToolContext.cpp
    src/ui/tools/ToolManager.cpp
    src/ui/tools/Tools.cpp
    src/ui/tools/FreehandLineTool.cpp
    src/ui/tools/SplineEditTool.cpp
    src/ui/tools/PolylineTool.cpp
    src/ui/tools/BoundaryTool.cpp
    src/ui/tools/PolylineEditTool.cpp
    src/ui/tools/ShapeSelectTool.cpp
    src/ui/tools/MTextTool.cpp
    src/ui/tools/TableTool.cpp
    src/ui/tools/WeldTool.cpp
    src/ui/RulerWidget.cpp
    src/ui/BomTree.cpp
    src/ui/ElementPanel.cpp
    src/ui/PropertyPanel.cpp
    src/ui/LayerPanel.cpp
    src/ui/StandardsPanel.cpp     # 工程制图合规检查面板（STDCHECK 结果展示 / 双击跳转图元）
    src/ui/LogPanel.cpp
    src/ui/CommandLine.cpp
    src/ui/CommandPalette.cpp      # 命令面板对话框（Ctrl+Shift+P；过滤逻辑在 CommandCatalog）
    src/ui/DiagnosticsDialog.cpp   # 诊断包导出对话框（异常退出提示 / 帮助菜单共用）
    src/ui/StatusBar.cpp
    src/ui/NewDrawingDialog.cpp
    src/ui/SheetFrameDialog.cpp    # 【图纸】>【新建图框】对话框（GB/T 14689 图框选项生成）
    src/ui/PaperSizeDialog.cpp
    src/ui/TitleBlockForm.cpp
    src/ui/ModifyInputDialog.cpp
    src/ui/ArrayDialog.cpp
    src/ui/BlockDialogs.cpp
    src/ui/BlockEditorDialog.cpp
    src/ui/DesignCenterPanel.cpp
    src/ui/StdPartTemplatePanel.cpp
    src/ui/StdPartTemplateDialog.cpp
    src/ui/SectionDialog.cpp
    src/ui/RegionManagerDialog.cpp
    src/ui/CleanupDialog.cpp
    src/ui/InquiryDialog.cpp
    src/ui/MTextDialog.cpp
    src/ui/StyleManagerDialog.cpp
    src/ui/TableDialog.cpp
    src/ui/GroupManagerDialog.cpp
    src/ui/QSelectDialog.cpp
    src/ui/StandardsBrowserDialog.cpp  # 标准知识库浏览面板（只读，扩展工具菜单入口）
    src/ui/GdtAnnotationDialog.cpp     # GD&T 标注面板（粗糙度/几何公差实时标准列表联动）
    src/ui/WeldSymbolDialog.cpp        # 焊缝符号对话框（GB/T 324 可视化选择与实时预览，确认后调用 WELD 命令）
    src/ui/FilterDialog.cpp
    src/ui/UnitsLimitsDialog.cpp
    src/ui/SnapSettingsDialog.cpp
    src/ui/ConstraintBar.cpp
    src/ui/PlotRenderer.cpp
    src/ui/PrintEngine.cpp
    src/ui/PrintSettingsDialog.cpp
    src/ui/PrintPreviewDialog.cpp
    src/ui/PublishDialog.cpp        # 发布（PUBLISH）：勾选布局批量出图
    src/ui/CrashRecoveryDialog.cpp   # 启动时发现 .sv$ 快照 → 恢复/丢弃
    src/ui/AutoSaveOptionsDialog.cpp # 自动保存间隔设置（关/5/10/15/30）
    src/ui/OptionsDialog.cpp        # 统一「选项」对话框（分类树 + 表单，应用/取消/重置）
    src/ui/DbDrawingsDialog.cpp     # 数据库绘图记录/模板表格对话框（打开/删除/应用/保存）
    src/ui/DbConfigDialog.cpp      # 数据库配置对话框（连接信息 → 本地 JSON 配置文件）
    src/ui/AiAssistantPanel.cpp     # AI 自然语言助手对话面板（流式显示 + 命令审批条）
)

# ---- 静态库分层：core / gui / dwgbridge ----
# 三个库只承载实现与依赖传播；启动画面、主题、翻译、诊断的调用顺序仍完全由
# src/main.cpp 控制，故不会因拆库而改变运行期行为。
add_library(sketchcad_core STATIC ${CORE_SOURCES})
# core 直接用到：QtCore（JSON/IO/插件）、QtGui（QColor/QFont/QPageLayout/剪贴板）、
# QtQml（QJSEngine 脚本宿主）、QtSql（DrawingDb）、QtNetwork（AI 后端）。
target_link_libraries(sketchcad_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Sql
    Qt6::Network
    sketchcad_dwgbridge)
target_include_directories(sketchcad_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(sketchcad_core PRIVATE -Wall -Wextra)
# Diagnostics.cpp 位于 core；崩溃转储宏与 dbghelp 必须随编译/链接该实现的库走，
# 否则拆库后主程序的 mini-dump 能力会静默消失。
if(WIN32)
    target_compile_definitions(sketchcad_core PRIVATE SKETCHCAD_ENABLE_CRASHHANDLER=1)
    target_link_libraries(sketchcad_core PUBLIC dbghelp)
endif()

add_library(sketchcad_gui STATIC ${UI_SOURCES})
# gui 直接用到：Widgets（面板/对话框）、PrintSupport（打印/预览/发布）、
# Sql（DbConfigDialog）；Core/Gui/Qml/Network 由 sketchcad_core 传播。
target_link_libraries(sketchcad_gui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Sql
    sketchcad_core
    sketchcad_dwgbridge)
target_include_directories(sketchcad_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(sketchcad_gui PRIVATE -Wall -Wextra)

# ---- 主程序 ----
add_executable(SketchCAD
    src/main.cpp
    src/ui/MainWindow.cpp
)
target_link_libraries(SketchCAD PRIVATE
    sketchcad_gui
    sketchcad_core
    sketchcad_dwgbridge
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Concurrent)
target_include_directories(SketchCAD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(SketchCAD PRIVATE -Wall -Wextra)

# ---- 双模式运行：SketchCAD 本体作为动态库被外部宿主加载 --------------------
# SketchCAD.exe（上方）保持独立运行能力不变；本目标复用同一组静态库，额外导出
# sdk/include/SketchCAD/hosting_abi.h 定义的纯 C 接口，供 OpenMBD Studio 以
# QLibrary 动态加载。两条路径共享源码、共享 Qt 工具链，但不共享全局状态。
#
# -fvisibility=hidden：杜绝把 sketchCAD 内部 C++/moc 符号泄漏到宿主进程，
# 避免与宿主静态链接的交换桥（ombd::sketchcad）发生符号互覆盖；只有显式
# SKETCHCAD_HOSTING_ABI_EXPORT 的 C 符号对外可见。
add_library(SketchCadPlugin SHARED src/host/SketchCadHosting.cpp)
target_compile_definitions(SketchCadPlugin PRIVATE SKETCHCAD_HOSTING_ABI_BUILD)
target_compile_options(SketchCadPlugin PRIVATE
    -Wall -Wextra -fvisibility=hidden -fvisibility-inlines-hidden)
target_include_directories(SketchCadPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_link_libraries(SketchCadPlugin PRIVATE
    sketchcad_gui
    sketchcad_core
    sketchcad_dwgbridge
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport
    Qt6::Qml Qt6::Sql Qt6::Network Qt6::Concurrent)
set_target_properties(SketchCadPlugin PROPERTIES
    OUTPUT_NAME "SketchCadPlugin"
    PREFIX ""
    FOLDER "SketchCAD/SDK")

# ---- D1b：部署 DWG 外部转换器（LibreDWG，独立构建）到运行时 tools/dwg/ ----
# 必须置于 add_executable(SketchCAD) 之后（POST_BUILD 依赖该目标已存在）。
# 转换器位于工程根之外一级：真实路径为 <CMAKE_SOURCE_DIR>/../thirdparty/libredwg-0.14/
# build-mingw（本机即 02-avticationCAD/thirdparty/libredwg-0.14/build-mingw）。
# 仅当转换器已构建时才拷贝；读(dwg2dxf)/写(dwgwrite) 两端均部署，杜绝"未安装"误报，
# 使 AC1012+ 的合法 DWG（如施工图纸库 C0层.dwg，AC1021）能被正常打开。
set(_LIBREDWG_BIN "${CMAKE_SOURCE_DIR}/../thirdparty/libredwg-0.14/build-mingw")
if(EXISTS "${_LIBREDWG_BIN}/dwg2dxf.exe")
    add_custom_command(TARGET SketchCAD POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SketchCAD>/tools/dwg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_LIBREDWG_BIN}/dwg2dxf.exe"
                "$<TARGET_FILE_DIR:SketchCAD>/tools/dwg/dwg2dxf.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_LIBREDWG_BIN}/dwgwrite.exe"
                "$<TARGET_FILE_DIR:SketchCAD>/tools/dwg/dwgwrite.exe"
        COMMENT "部署 DWG 转换器 dwg2dxf/dwgwrite → tools/dwg")
endif()

# ---- 版本宏注入（version.h）----
# 从 project(... VERSION ...) 生成，供「关于」对话框等 C++ 侧读取，
# 使界面显示的版本与 exe 属性、安装包名同源于一个数字。
# 版本头仅由主程序与插件在运行时读取。
set(SKETCHCAD_COMPANY_NAME  "我是慎独")
set(SKETCHCAD_PRODUCT_NAME  "SketchCAD 草图设计系统")
set(SKETCHCAD_COPYRIGHT     "Copyright (C) 2026 我是慎独. All rights reserved.")
# Windows VERSIONINFO 要求四段式数字版本：semver 是三段，故第四段补 0。
# 例：1.1.0 → 1.1.0.0（补丁位留给预发布修订，如 1.1.0.1）。
set(SKETCHPAD_VERSION       "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.0")
set(SKETCHPAD_PRODUCTVER    "${SKETCHPAD_VERSION}")
# .rc 语法特例：VERSIONINFO 的 FILEVERSION/PRODUCTVERSION 必须写成**逗号分隔**的
# 四段（1,1,0,0），点号形式会被 windres 判为 syntax error。
set(SKETCHPAD_VERSION_RC    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(SKETCHPAD_PRODUCTVER_RC "${SKETCHPAD_VERSION_RC}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY)

# ---- Windows 应用图标 + 文件属性（app.rc）----
# app.rc 由 version.h 同源的模板生成，VERSIONINFO 随 project VERSION 自动同步。
# 用 target_sources 而非全局 add_custom_command：只作用于 SketchCAD，
# 自测目标不引入任何 .rc（RC_COMPILER 需 windres，且 rc 会拖入 GUI 资源依赖）。
if(WIN32)
    enable_language(RC)
    set(SKETCHCOMPANY_NAME  "${SKETCHCAD_COMPANY_NAME}")
    set(SKETCHPRODUCT_NAME  "${SKETCHCAD_PRODUCT_NAME}")
    set(SKETCHCOPYRIGHT     "${SKETCHCAD_COPYRIGHT}")
    set(SKETCHFILE_DESC     "SketchCAD 草图设计系统")
    set(SKETCHICON_PATH     "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.ico")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc
        @ONLY)
    target_sources(SketchCAD PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc)
    target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
endif()

# 崩溃转储的宏与 dbghelp 随 Diagnostics.cpp 一并挂在 sketchcad_core。

# ===================== 国际化（Qt Linguist：ts → qm → 资源内嵌）=====================
# 中文为源语言：sketchcad_en.ts 存英文译文（lupdate 从 tr() 抽取中文源串），
# lrelease 编译成 .qm 后经 qt_add_resources 内嵌为 ：/i18n/sketchcad_en.qm
# —— 部署零附件（exe 自含），测试则走 exe 同目录磁盘文件兜底（I18n 双路装载）。
set(SKETCHCAD_TS ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/sketchcad_en.ts)
set(SKETCHCAD_QM ${CMAKE_CURRENT_BINARY_DIR}/sketchcad_en.qm)
add_custom_command(OUTPUT ${SKETCHCAD_QM}
    COMMAND Qt6::lrelease -silent ${SKETCHCAD_TS} -qm ${SKETCHCAD_QM}
    DEPENDS ${SKETCHCAD_TS}
    COMMENT "lrelease: sketchcad_en.ts -> sketchcad_en.qm")
add_custom_target(sketchcad_i18n ALL DEPENDS ${SKETCHCAD_QM})
add_dependencies(SketchCAD sketchcad_i18n)
# 生成于 build 根的绝对路径文件进 qrc 必须显式给资源内别名（Qt6 硬性要求）
set_source_files_properties(${SKETCHCAD_QM} PROPERTIES QT_RESOURCE_ALIAS "sketchcad_en.qm")
qt_add_resources(SketchCAD "sketchcad_i18n_qm" PREFIX "/i18n" FILES ${SKETCHCAD_QM})

# ---- 「帮助 > 关于 > 打赏」内置微信收款码 ----
# 编译进 exe 的 Qt 资源（:/reward/wechat_reward_qr.png），部署零附件；
# 更换图片只需覆盖源文件后重新构建，rcc 会因文件 mtime 变化自动重跑。
# ⚠️ 必须显式给 QT_RESOURCE_ALIAS：Qt6 会把 FILES 的相对路径原样当别名，
#    否则实际注册成 :/reward/resources/reward/wechat_reward_qr.png，运行时取不到图。
set(SKETCHCAD_REWARD_QR ${CMAKE_CURRENT_SOURCE_DIR}/resources/reward/wechat_reward_qr.png)
set_source_files_properties(${SKETCHCAD_REWARD_QR}
    PROPERTIES QT_RESOURCE_ALIAS "wechat_reward_qr.png")
qt_add_resources(SketchCAD "sketchcad_reward_qr"
    PREFIX "/reward"
    FILES ${SKETCHCAD_REWARD_QR})

# ---- 标准知识库（GB/T 机械制图手册数据，12 类表 JSON 内嵌）----
# 经 qt_add_resources 内嵌为 :/standards/<name>.json，部署零附件；
# 主程序运行时读取，故以 Qt 资源内嵌。
# ⚠️ 必须显式给 QT_RESOURCE_ALIAS：Qt6 会把 FILES 的相对路径原样当别名，
#    否则实际注册成 :/standards/src/core/standards/data/<name>.json，运行时取不到。
set(SKETCHCAD_STD_DATA
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/sheet_sizes.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/scales.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/fonts.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/line_types.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/roughness.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/tolerances.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/geo_tol.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/thread.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/gear.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/bearing.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/weld.json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/standards/data/cad_layers.json
)
foreach(_std_file ${SKETCHCAD_STD_DATA})
    get_filename_component(_std_name ${_std_file} NAME)
    set_source_files_properties(${_std_file} PROPERTIES QT_RESOURCE_ALIAS "${_std_name}")
endforeach()
qt_add_resources(SketchCAD "sketchcad_standards" PREFIX "/standards" FILES ${SKETCHCAD_STD_DATA})

# ===================== 脚本宿主：可选语言引擎 =====================
# 默认仅 JavaScript（Qt 内置 QJSEngine，零外部依赖）参与构建。
# 开启下列开关可额外把 Python / VBA 引擎编译进程序，支持更多脚本语言。
option(SCRIPTHOST_ENABLE_PYTHON "嵌入 CPython，支持 Python 脚本（需系统 Python 开发库）" OFF)
option(SCRIPTHOST_ENABLE_VBA    "Windows Script Host，支持 VBA/VBScript 脚本（仅 Windows）" OFF)

if(SCRIPTHOST_ENABLE_PYTHON)
  find_package(Python3 REQUIRED COMPONENTS Development)
  # 主程序经 sketchcad_core 获得引擎；SketchCAD 目标仍只含 main.cpp + MainWindow.cpp。
  target_sources(sketchcad_core PRIVATE src/core/PythonEngine.cpp)
  target_compile_definitions(sketchcad_core PRIVATE SCRIPTHOST_ENABLE_PYTHON)
  target_link_libraries(sketchcad_core PUBLIC Python3::Python)
  message(STATUS "脚本宿主：Python 引擎已启用")
endif()

if(SCRIPTHOST_ENABLE_VBA)
  if(NOT WIN32)
    message(WARNING "SCRIPTHOST_ENABLE_VBA 仅在 Windows 平台有效，已忽略")
  else()
    target_sources(sketchcad_core PRIVATE src/core/VbaEngine.cpp)
    target_compile_definitions(sketchcad_core PRIVATE SCRIPTHOST_ENABLE_VBA)
    target_link_libraries(sketchcad_core PUBLIC ole32 oleaut32)
    message(STATUS "脚本宿主：VBA 引擎已启用")
  endif()
endif()

# ===================== COM 自动化服务器（批量出图，Windows） =====================
# 进程内 COM 组件（SketchCADAutomation.dll），regsvr32 注册后供 VBScript /
# PowerShell / Python(win32com) / C# 等外部系统后期绑定调用。
# 分层：src/com/BatchPlotEngine（无头引擎，COM 无关，自测直测）
#       src/com/SketchCADComServer（IDispatch 手写分派 + 类厂 + regsvr32 注册）
# 依赖：CORE_SOURCES + 打印三件套（ElementPath/PlotRenderer/PrintEngine，
#       已与 GraphicsView 解耦）——不链任何对话框/主窗口源，保持 DLL 精简。
if(WIN32)
  add_library(SketchCADAutomation SHARED
      src/com/BatchPlotEngine.cpp
      src/com/SketchCADComServer.cpp
      src/ui/ElementPath.cpp
      src/ui/PlotRenderer.cpp
      src/ui/PrintEngine.cpp
      ${CORE_SOURCES}
  )
  target_link_libraries(SketchCADAutomation PRIVATE
      Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Sql
      SketchDwgBridge
      ole32 oleaut32 advapi32 uuid)
  target_include_directories(SketchCADAutomation PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
  target_compile_options(SketchCADAutomation PRIVATE -Wall -Wextra)
  set_target_properties(SketchCADAutomation PROPERTIES PREFIX "")
  message(STATUS "COM 自动化服务器：SketchCADAutomation 已纳入构建（批量出图）")
endif()
