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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Qml)

# ---- DRY：公共核心源文件列表，供所有目标共用 ----
set(CORE_SOURCES
    src/core/Element.cpp
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
    src/core/ToolRegistry.cpp
    src/core/ShapeSelect.cpp
    src/core/GripOps.cpp
    src/core/sheet_frame.cpp
    src/core/Modify.cpp
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
    src/core/Block.cpp
    src/core/Xref.cpp
    src/core/CadFormats.cpp
    src/core/PlotStyle.cpp
    src/core/Layout.cpp
    src/core/TextStyle.cpp
    src/core/QSelect.cpp
    src/core/Units.cpp
    src/core/SpatialIndex.cpp
)
set(UI_SOURCES
    src/ui/GraphicsView.cpp
    src/ui/ElementPath.cpp
    src/ui/ModelMediator.cpp
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
)

# ---- 主程序 ----
add_executable(SketchCAD
    src/main.cpp
    src/ui/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)
target_link_libraries(SketchCAD PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(SketchCAD PRIVATE -Wall -Wextra)

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
)
target_link_libraries(SketchSelfTest PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml)
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
target_link_libraries(SketchBenchmark PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml)
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
target_link_libraries(PlotStyleSamples PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::Qml)
target_include_directories(PlotStyleSamples PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
target_compile_options(PlotStyleSamples PRIVATE -Wall -Wextra)

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
      ole32 oleaut32 advapi32 uuid)
  target_include_directories(SketchCADAutomation PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include)
  target_compile_options(SketchCADAutomation PRIVATE -Wall -Wextra)
  set_target_properties(SketchCADAutomation PROPERTIES PREFIX "")
  message(STATUS "COM 自动化服务器：SketchCADAutomation 已纳入构建（批量出图）")
endif()
