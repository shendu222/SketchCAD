cmake_minimum_required(VERSION 3.16)
# 版本号单一来源（semver）。此处是全项目「版本真相」：
#   → configure_file 生成 build/version.h（供「关于」对话框等 C++ 侧读取）
#   → configure_file 生成 build/app.rc（注入 exe 文件属性 VERSIONINFO）
#   → build.sh package 模式读取本行，拼出 SketchCAD-<版本>-Setup.exe
# 三者（关于对话框 / exe 属性 / 安装包文件名）因此天然一致，不会各自漂移。
project(SketchCAD VERSION 1.1.0 LANGUAGES CXX)

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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Qml Concurrent LinguistTools)

# ---- DRY：公共核心源文件列表，供所有目标共用 ----
set(CORE_SOURCES
    src/core/Element.cpp
    src/core/ElementValidate.cpp   # 鲁棒性守卫层：读档前图元只读校验/丢弃/修复（不动几何算法）
    src/core/ElementFactory.cpp
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
    src/core/CommandCatalog.cpp    # 命令元数据目录 + 面板纯逻辑 + 快捷键审计（纯数据层，可无头直测）
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
    src/core/SurfaceRoughness.cpp
    src/core/SectionCutSymbol.cpp
    src/core/SectionDrawing.cpp
    src/core/ConstraintSolver.cpp
    src/core/LegacyConstraintSolver.cpp   # SKT 集成 Phase B：修复模型委托实现（IConstraintSolver）
    src/core/ConstraintSolverFacade.cpp   # SKT 集成 Phase B：路由门面（特性开关+回落，单例）
    src/core/ToolRegistry.cpp
    src/core/ShapeSelect.cpp
    src/core/GripOps.cpp
    src/core/sheet_frame.cpp
    src/core/Modify.cpp
    src/core/UndoDelta.cpp        # 撤销栈差量化（变更集 + 每 50 步全量检查点）
    src/core/Clipboard.cpp
    src/core/Cleanup.cpp
    src/core/AircraftViews.cpp
    src/core/FlangeViews.cpp
    src/core/BearingViews.cpp
    src/core/HousingViews.cpp
    src/core/FlangeHubViews.cpp
    src/core/FloorPlanViews.cpp
    src/core/Section.cpp
    src/core/HatchLibrary.cpp
    src/core/Gdt.cpp   # GD&T: 多重引线(MLeader) + 形位公差框格(Fcf) 几何生成器
    src/core/Block.cpp
    src/core/Xref.cpp
    src/core/CadFormats.cpp
    src/core/PlotStyle.cpp
    src/core/Layout.cpp
    src/core/TextStyle.cpp
    src/core/TextMetrics.cpp   # 文字度量单一数据源（屏幕/打印/取景三处同源）
    src/core/QSelect.cpp
    src/core/Units.cpp
    src/core/SpatialIndex.cpp
    src/core/AppSettings.cpp   # 全局设置（自动保存间隔/磁盘阈值，HKCU\Software\SketchCAD）
    src/core/I18n.cpp          # 界面语言单一入口（语言解析 + QTranslator 装载，自测直测）
    src/core/AutoSave.cpp      # 自动保存核心（定时+操作计数双触发，原子落盘，可恢复）
    src/core/RecentFiles.cpp   # 最近文件 MRU（路径+时间+pin，复用 AppSettings 存储，可离线直测）
    src/core/Diagnostics.cpp   # 诊断：Qt 日志滚动落盘 + Windows 崩溃 mini-dump + 会话标记
    src/core/DiagBundle.cpp    # 诊断包：Stored-ZIP 写入器 + 打包编排（纯 core 可无头直测）
    src/core/Telemetry.cpp     # 遥测出口（默认关闭零外发；开关+出口双门闩）
)

# ---- D1：DWG 外部转换器隔离层（独立静态库目标）----
# DwgBridge 只封装 QProcess 进程外调用 + 临时文件中转，**不含任何转换器头文件/
# 代码**（LibreDWG 是独立 GPL exe，转换器本体在 ../../thirdparty/libredwg-0.14，
# 独立构建，绝不进入本工程的任何目标）。独立成库的目的是物理隔离：UI 层与
# 主程序其他源文件无法绕过桥直接触碰转换器，链接边界即审计边界。
add_library(SketchDwgBridge STATIC src/core/DwgBridge.cpp)
# Qt6::Gui：CadFormats.h → Element.h 链上引用 QColor（仅取头路径，桥逻辑本身只用 Core）
target_link_libraries(SketchDwgBridge PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(SketchDwgBridge PRIVATE -Wall -Wextra)
set(UI_SOURCES
    src/ui/GraphicsView.cpp
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
    src/ui/RulerWidget.cpp
    src/ui/BomTree.cpp
    src/ui/ElementPanel.cpp
    src/ui/PropertyPanel.cpp
    src/ui/LayerPanel.cpp
    src/ui/LogPanel.cpp
    src/ui/CommandLine.cpp
    src/ui/CommandPalette.cpp      # 命令面板对话框（Ctrl+Shift+P；过滤逻辑在 CommandCatalog）
    src/ui/DiagnosticsDialog.cpp   # 诊断包导出对话框（异常退出提示 / 帮助菜单共用）
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
    src/ui/BlockDialogs.cpp
    src/ui/BlockEditorDialog.cpp
    src/ui/DesignCenterPanel.cpp
    src/ui/SectionDialog.cpp
    src/ui/RegionManagerDialog.cpp
    src/ui/CleanupDialog.cpp
    src/ui/InquiryDialog.cpp
    src/ui/MTextDialog.cpp
    src/ui/StyleManagerDialog.cpp
    src/ui/TableDialog.cpp
    src/ui/GroupManagerDialog.cpp
    src/ui/QSelectDialog.cpp
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
)

# ---- 主程序 ----
add_executable(SketchCAD
    src/main.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SketchCAD PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Concurrent SketchDwgBridge)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(SketchCAD PRIVATE -Wall -Wextra)

# ---- 版本宏注入（version.h）----
# 从 project(... VERSION ...) 生成，供「关于」对话框等 C++ 侧读取，
# 使界面显示的版本与 exe 属性、安装包名同源于一个数字。
# 【重要】SketchSelfTest 不链接本头文件：自测不关心版本，保持目标最小化。
set(SKETCHCAD_COMPANY_NAME  "航空CAD")
set(SKETCHCAD_PRODUCT_NAME  "SketchCAD 草图设计系统")
set(SKETCHCAD_COPYRIGHT     "Copyright (C) 2026 航空CAD. All rights reserved.")
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

# 崩溃转储需要 dbghelp（MiniDumpWriteDump）。仅主程序启用：
# 自测目标 SketchSelfTest 不定义 SKETCHCAD_ENABLE_CRASHHANDLER、不链 dbghelp，保持纯净无头。
if(WIN32)
    target_link_libraries(SketchCAD PRIVATE dbghelp)
    target_compile_definitions(SketchCAD PRIVATE SKETCHCAD_ENABLE_CRASHHANDLER=1)
endif()

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

# ---- 无界面自测（约束求解 / 捕捉 单元测试）----
# 复用 CORE_SOURCES，不链接 UI；覆盖 21 种约束每类 ≥1 用例 + 8 类捕捉命中 + 冲突检测。
# 附加批量出图引擎（COM 无关层）与打印三件套，供 t_comAutomation 无头直测。
add_executable(SketchSelfTest
    tests/SketchSelfTest.cpp
    ${CORE_SOURCES}
    src/com/BatchPlotEngine.cpp
    src/ui/ElementPath.cpp
    src/ui/PlotRenderer.cpp
    src/ui/PrintEngine.cpp
    src/ui/DocTab.cpp               # 多文档会话核心（仅依赖 core，供 t_multidoc 离线直测）
    src/ui/CommandLine.cpp          # 命令行键盘语义（历史/补全/重复），供 t_interaction 无头直测
)
target_link_libraries(SketchSelfTest PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Concurrent SketchDwgBridge)
target_include_directories(SketchSelfTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(SketchSelfTest PRIVATE -Wall -Wextra)

# 黄金样本（tests/golden/ 静态 DXF 锚点）拷贝到构建根目录，供 t_goldenSamples 无头读取
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden
     DESTINATION ${CMAKE_BINARY_DIR})

# ===================== 性能基准（增量同步 + 均匀网格空间索引）=====================
# 带 QApplication，可实例化 GraphicsView（与真实交互同一代码路径）：
# 测 打开(rebuild)/单点拾取(索引 vs 全量)/一致性差分/选择高亮/平移缩放 FPS。
# 三档规模 1万/5万/10万，对照验收目标。无头环境 FPS 为交互处理开销代理（见源码注释）。
add_executable(SketchBenchmark
    bench/SketchBenchmark.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SketchBenchmark PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Concurrent SketchDwgBridge)
target_include_directories(SketchBenchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(SketchBenchmark PRIVATE -Wall -Wextra)

# ===================== 打印样式样张生成器（A4/A3 黑白 PDF 目检 + 双路回归）=====================
# 产出四类 GB 线型（粗实/细实/虚线/点画）的黑白样张（带样式表 vs 不带样式表），
# 并对两路出图做灰度直方图/墨量/彩色像素回归比对，支撑打印样式映射的验收。
add_executable(PlotStyleSamples
    bench/PlotStyleSamples.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(PlotStyleSamples PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Concurrent SketchDwgBridge)
target_include_directories(PlotStyleSamples PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(PlotStyleSamples PRIVATE -Wall -Wextra)

# ===================== README 配图生成器（模板图纸离屏渲染 PNG）=====================
# 六个参数化模板（法兰/箱体/轴承座/法兰轮毂/户型/飞机三面图）经 PrintEngine
# 真实渲染链产出 docs/images/*.png，供 README「图文并茂」配图。仅供文档出图。
add_executable(ReadmeShots
    bench/ReadmeShots.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(ReadmeShots PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml Qt6::Concurrent SketchDwgBridge)
target_include_directories(ReadmeShots PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(ReadmeShots PRIVATE -Wall -Wextra)

# ===================== 脚本宿主：可选语言引擎 =====================
# 默认仅 JavaScript（Qt 内置 QJSEngine，零外部依赖）参与构建。
# 开启下列开关可额外把 Python / VBA 引擎编译进程序，支持更多脚本语言。
option(SCRIPTHOST_ENABLE_PYTHON "嵌入 CPython，支持 Python 脚本（需系统 Python 开发库）" OFF)
option(SCRIPTHOST_ENABLE_VBA    "Windows Script Host，支持 VBA/VBScript 脚本（仅 Windows）" OFF)

if(SCRIPTHOST_ENABLE_PYTHON)
  find_package(Python3 REQUIRED COMPONENTS Development)
  foreach(tgt SketchCAD SketchSelfTest)
    target_sources(${tgt} PRIVATE src/core/PythonEngine.cpp)
    target_compile_definitions(${tgt} PRIVATE SCRIPTHOST_ENABLE_PYTHON)
    target_link_libraries(${tgt} PRIVATE Python3::Python)
  endforeach()
  message(STATUS "脚本宿主：Python 引擎已启用")
endif()

if(SCRIPTHOST_ENABLE_VBA)
  if(NOT WIN32)
    message(WARNING "SCRIPTHOST_ENABLE_VBA 仅在 Windows 平台有效，已忽略")
  else()
    foreach(tgt SketchCAD SketchSelfTest)
      target_sources(${tgt} PRIVATE src/core/VbaEngine.cpp)
      target_compile_definitions(${tgt} PRIVATE SCRIPTHOST_ENABLE_VBA)
      target_link_libraries(${tgt} PRIVATE ole32 oleaut32)
    endforeach()
    message(STATUS "脚本宿主：VBA 引擎已启用")
  endif()
endif()

# ===================== 示例插件（第三方开发示范：机械行业） =====================
# 仅依赖 SDK 公共头（纯接口 + POD），不链接宿主任何内部库，
# 以此验证「第三方插件可完全独立于宿主二进制编译」的 ABI 契约。
# 输出到构建目录下的 plugins/ 子目录，宿主启动时自动扫描加载，
# 自测亦可按路径直接加载验证「注册→执行→卸载」全链路。
add_library(SamplePlugin SHARED sdk/samples/SamplePlugin/SamplePlugin.cpp)
target_include_directories(SamplePlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_definitions(SamplePlugin PRIVATE SKETCHCAD_PLUGIN_BUILD)
target_compile_options(SamplePlugin PRIVATE -Wall -Wextra)
set_target_properties(SamplePlugin PROPERTIES
    PREFIX ""                                                  # 输出 SamplePlugin.dll（不带 lib 前缀，符合插件命名惯例）
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"   # Windows: .dll
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"   # Linux:   .so
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
message(STATUS "SDK 示例插件：SamplePlugin 已纳入构建（机械行业示范）")

# ---- ABI 协商探针（自测负例专用）：导出 sdk_abi_version=99 的「未来插件」 ----
# 输出到独立目录 abiprobe/，避免被宿主启动时的 plugins/ 目录扫描误加载。
add_library(AbiProbePlugin SHARED sdk/tests/AbiProbe/AbiProbe.cpp)
target_include_directories(AbiProbePlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(AbiProbePlugin PRIVATE -Wall -Wextra)
set_target_properties(AbiProbePlugin PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/abiprobe"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/abiprobe"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/abiprobe"
)
message(STATUS "SDK ABI 探针：AbiProbePlugin 已纳入构建（自测负例：ABI 过新拒绝）")

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
      Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml
      SketchDwgBridge
      ole32 oleaut32 advapi32 uuid)
  target_include_directories(SketchCADAutomation PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
  target_compile_options(SketchCADAutomation PRIVATE -Wall -Wextra)
  set_target_properties(SketchCADAutomation PROPERTIES PREFIX "")
  message(STATUS "COM 自动化服务器：SketchCADAutomation 已纳入构建（批量出图）")
endif()
