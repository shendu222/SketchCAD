# SketchCAD — 二维 CAD 草图绘制（航空与机械）

基于 **C++17 + Qt 6 Widgets** 的二维 CAD 绘图草图模块。用于飞机三面图、法兰盘三面图等工程图的快速绘制，支持直线、圆、圆弧、多边形、样条曲线、椭圆等图元的参数化交互式绘制，以及尺寸标注、几何约束、图纸框与标题栏等制图要素。

> 本仓库为整个工程源码与回归测试的 git 根目录；设计文档与历次重构的设计说明位于上级目录 `docs/` 及 `../`。

---

## 目录

- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [目录结构](#目录结构)
- [环境要求](#环境要求)
- [安装与构建](#安装与构建)
- [运行](#运行)
- [基本使用方法](#基本使用方法)
- [命令行模式（高级）](#命令行模式高级)
- [测试](#测试)
- [已知说明](#已知说明)

---

## 功能特性

**绘图图元**
- 直线：两点 / 水平 / 垂直 / 定角 / 平行（过点）/ 角平分线 / 与圆相切 / 正交 / 相对角度等待多种模式
- 圆：圆心半径 / 圆心+圆周点 / 两点 / 三点外接 / 公切线内切 / 相切内切圆 / 同心
- 圆弧：三点 / 起点-端点-半径 / 起点-端点-圆心 / 相切过渡等多种模式
- 多边形、椭圆、样条曲线（自由 Catmull-Rom / 抛物线 / 双曲线 / 焦点准线等）

**制图要素**
- 尺寸标注：距离 / 半径 / 直径，支持对齐标注、线/圆/M圆弧自适应
- 几何约束：相切、垂直、平行等拓扑约束，修改图元参数时关联图元自动跟随
- 图框与标题栏：标准图纸幅面 + 可填写式标题栏
- 参数化三视图模板：飞机三面图、法兰盘三面图（长对正 / 高平齐 / 宽相等投影对齐）

**文件与交互**
- 格式支持：JSON / DXF / STEP / DWG 导入导出，比例随图往返
- 交互：右键拖拽平移、框选、多级撤销/重做、智能捕捉点、命令行输入、属性面板参数化修改

---

## 技术栈

| 项 | 说明 |
|----|------|
| 语言 | C++17 |
| GUI | Qt 6 Widgets（`Qt6::Widgets`），`AUTOMOC/AUTORCC/AUTOUIC` |
| 构建 | CMake ≥ 3.16 + Ninja + MinGW-w64 GCC 13.1.0 |
| 测试 | 无界面核心自测 + offscreen 菜单/布局/渲染级回归 |
| 架构 | 核心层 `core/`（纯计算、无 UI）+ 界面层 `ui/`（视图/面板/中介者模式） |

---

## 目录结构

```
sketch-cad-cpp/
├── CMakeLists.txt          # 全部目标（主程序 + 各回归测试）
├── build.sh                # 一键构建 / 测试 / 打包脚本（Git Bash）
├── src/
│   ├── main.cpp            # 程序入口
│   ├── core/               # 核心层：图元、几何、命令、捕捉、约束、格式(无 UI 依赖)
│   │   ├── Element*.cpp    # 图元模型与工厂
│   │   ├── LineModes.cpp · CircleModes.cpp · ArcModes.cpp
│   │   ├── PolyModes.cpp · SplineModes.cpp · EllipseModes.cpp
│   │   ├── ConstraintSolver.cpp · Snap.cpp · Modify.cpp · Trim.cpp
│   │   ├── CommandInterpreter.cpp  # 命令行解析
│   │   ├── ToolRegistry.cpp        # 工具/子模式注册表
│   │   ├── CadFormats.cpp          # JSON/DXF/STEP/DWG
│   │   └── AircraftViews.cpp · FlangeViews.cpp   # 参数化三视图
│   └── ui/
│       ├── MainWindow.cpp · GraphicsView.cpp     # 主窗口与画布视图
│       ├── ModelMediator.cpp       # 中介者：模型/视图/面板/状态栏通信
│       ├── tools/                  # 工具管理与子模式处理（Tools.cpp 等）
│       └── CommandLine.cpp · PropertyPanel.cpp · BomTree.cpp · RulerWidget.cpp ...
├── tests/                  # 回归测试源文件（见「测试」节）
├── dist/                   # 构建可执行 + Qt 运行时（可独立运行）
└── tools/
    ├── make_foreign_dxf.py # 生成第三方 DXF 互操作样例
    └── ...
```

---

## 环境要求

- **Qt 6.10.1 MinGW 64 位**
- **MinGW-w64 GCC 13.1.0**（随 Qt 安装的 `mingw1310_64`）
- **CMake ≥ 3.16** 与 **Ninja**（Qt 安装包自带，位于 `Tools/CMake_64` 与 `Tools/Ninja`）
- **Git Bash**（推荐，`build.sh` 面向它编写）

> 本工程的 CMake 已把工具链路径写死到 `C:/Qt/6.10.1` 与 `C:/Qt/Tools`，若安装在其他位置需相应调整 `CMakeLists.txt` 与 `build.sh` 顶部的路径常量。

---

## 安装与构建

### 1. 克隆仓库

```bash
git clone <仓库地址> sketch-cad-cpp
cd sketch-cad-cpp
```

### 2. 一键构建（推荐，Git Bash）

在 Git Bash 中进入仓库根目录：

```bash
./build.sh              # 增量构建，产物为 build/SketchCAD.exe
./build.sh clean        # 清理 build/ 后完整重建
./build.sh test         # 构建并运行核心层自测（SketchSelfTest）
./build.sh dist         # 构建 + 用 windeployqt 打包独立运行的 dist/ 目录
```

### 3. 手动 CMake 构建

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

构建产物：
- 主程序：`build/SketchCAD.exe`
- 独立运行包：`dist/SketchCAD.exe`（已含 Qt 运行时，可拷贝到无 Qt 环境运行）

---

## 运行

```bash
# 开发环境（需 Qt DLL 在 PATH）
./build/SketchCAD.exe

# 独立运行包（自带 Qt 运行时）
./dist/SketchCAD.exe
```

Windows 也可直接双击 `dist/SketchCAD.exe`。

---

## 基本使用方法

### 选择绘图工具

- 通过顶部菜单 **【绘图】** 下的各图元子菜单（直线 / 圆 / 圆弧 / 多边形 / 样条 / 椭圆 / 尺寸标注），或工具栏相应按钮进入绘制模式。
- 部分图元含多种子模式（如直线的角度线、平行线、切线等），可在子菜单中挑选。

### 绘制图元

1. 选择图元工具后，**按状态栏/命令提示行的引导**逐步输入点。
2. 左键单击落点；需要输入长度 / 角度 / 半径时，可在**参数输入条**中键入数值后按 **Enter** 生效。
3. 样条等自由曲线：**右键结束**绘制。

### 对象捕捉与智能捕捉

- 绘制过程中靠近已有图元的端点、中点、圆心、象限点会自动出现捕捉标记，左键即可吸附。
- 可用快捷键在捕捉模式间切换（精确数值优先 / 自由捕捉）。

### 修改与约束

- **框选**或单击选中图元，可在 **属性面板 / 参数输入条** 中修改尺寸参数，关联图元按几何约束自动跟随（如保持相切、垂直）。
- 支持**多级撤销 / 重做**（菜单或快捷键）。

### 视图操作

- **右键拖拽** = 平移画布。
- 滚轮缩放、滚轮中键或工具栏按钮适配视图。

### 尺寸标注

1. 选择 **【尺寸标注】**，再拾取目标图元。
2. 移动鼠标预览标注，左键点击放置。
3. 圆 / 圆弧默认标注直径，右键可在直径 / 半径间切换。

### 文件

- **新建 / 打开 / 保存**，格式支持 JSON / DXF / STEP / DWG。
- 三视图模板：菜单入口生成飞机或法兰盘三面图，参数联动。

---

## 命令行模式（高级）

底部命令行支持键入命令完成精确绘制，例如：

```
line 0,0 50,0                 # 直线：从 (0,0) 到 (50,0)
line_h 0,0 80                 # 水平直线
circle_cr 20,20 10            # 圆：圆心半径
dim distance 0,0 60,0         # 距离标注
spline_parabola ...           # 样条抛物线
undo                          # 撤销
```

输入命令前缀并按 **Tab** 可补全支持的子模式名称。

---

## 测试

`build.sh test` 运行核心层自测；`ctest` 或逐一运行各回归可执行：

| 目标 | 说明 |
|------|------|
| `SketchSelfTest` | 核心层无界面自测（图元/几何/约束/CLI，数百断言） |
| `DrawingMenuTest` | offscreen 构造主窗口，验证【绘图】菜单结构去重正确 |
| `SelectMenuTest` | 验证【选择】菜单结构与行为闭环 |
| `ModifyMenuTest` | 验证【修改】菜单 + 一次 undo 回滚 |
| `ToolbarLayoutTest` | 两行工具栏不溢出 / 不重合 / 按钮完整 |
| `FileRoundTripTest` | 图纸 JSON/DXF/STEP/DWG「新建→存盘→读回」往返 |
| `ScaleRenderTest` | 绘图比例的像素级渲染验证 |
| `DxfImportTest` | DXF 读入互操作（含第三方样例） |
| `FlangeViewsTest` | 法兰盘三面图参数化模板回归 |
| `ViewPanTest` / `ViewRenderTest` / `ViewRenderTest2` | 平移交互 / 渲染级验证 |
| `SelectWindowTest` | 画布框选集成测试 |

构建并执行某项测试：

```bash
cmake --build build --target SketchSelfTest
./build/SketchSelfTest.exe
```

（GUI 测试基于 offscreen，测试进程退出码 `0` 表示通过。）

---

## 已知说明

- 本工程工具链路径在 CMakeLists 中写死为 `C:/Qt/6.10.1/mingw_64` 等默认安装位置，换机或换版本需先改路径。
- 命令行的 `dim radius` 仅对圆 / 圆弧有效，对直线等无半径图元会直接拒绝并在日志中说明。
- 提示文案、菜单结构等以中文为基准；历次重构与设计思考的记录见上级 `docs/` 与 `../` 下的设计文档。