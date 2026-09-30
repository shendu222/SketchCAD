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

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

# ---- DRY：公共核心源文件列表，供所有目标共用 ----
set(CORE_SOURCES
    src/core/Element.cpp
    src/core/ElementFactory.cpp
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
    src/ui/tools/BoundaryTool.cpp
    src/ui/tools/PolylineEditTool.cpp
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
target_link_libraries(SketchCAD PRIVATE Qt6::Widgets Qt6::PrintSupport)
target_include_directories(SketchCAD PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(SketchCAD PRIVATE -Wall -Wextra)

# ---- 无界面自测（约束求解 / 捕捉 单元测试）----
# 复用 CORE_SOURCES，不链接 UI；覆盖 21 种约束每类 ≥1 用例 + 8 类捕捉命中 + 冲突检测。
add_executable(SketchSelfTest
    tests/SketchSelfTest.cpp
    ${CORE_SOURCES}
)
target_link_libraries(SketchSelfTest PRIVATE Qt6::Widgets Qt6::PrintSupport)
target_include_directories(SketchSelfTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_compile_options(SketchSelfTest PRIVATE -Wall -Wextra)
