```markdown
# SketchCAD

> 🎨 **一款基于 Qt 6 的轻量级 2D CAD 设计软件** —— 17+ Qt Widgets · 60+ 绘图命令 · GB/T 14665 标准支持

![Build](https://img.shields.io/badge/build-SketchCAD%20%2F%20SelfTest-passing-brightgreen)
![License](https://img.shields.io/badge/license-Proprietary-lightgrey)

---

## 🚀 快速开始

### 一键安装
下载最新安装包后双击运行，或通过命令行静默安装cmd
```
SketchCAD-<version>-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /N
```

### 首次使用
1. **启动软件** → 选择绘图单位（毫米/英寸）
2. **绘制线条** → 命令行输入 `line 0,0 50,0` 或点击工具栏
3. **保存文件** → 支持 JSON / DXF / STEP / DWG 格式

### 环境要求
- **操作系统**: Windows 10/11 (x64)
- **运行库**: MSVC 2022 Runtime
- **最低配置**: 4GB RAM, 200MB 磁盘空间

---

## 📐 核心特性

### 🏗️ 架构设计mermaid
```
graph TD
    subgraph UI["🖥️ 用户界面 (Qt Widgets)"]
        MW[MainWindow 主窗口]
        GV[GraphicsView 图形视图]
        MP[ModelMediator 模型中介]
        PP[PropertyPanel 属性面板 / CommandLine 命令行]
    end
    subgraph CORE["⚙️ 核心引擎 core/"]
        EL[Element / SketchModel 元素与模型]
        GEO[Geometry / Snap 几何与捕捉]
        MOD[Modify 编辑操作]
        CMD[CommandInterpreter 命令解释器]
        FMT[CadFormats JSON/DXF/STEP/DWG]
        CON[ConstraintSolver 约束求解器]
    end
    subgraph EXT["🔌 扩展接口"]
        SDK[SDK Plugin (C++ API)]
        COM[COM Automation (Windows)]
        SCR[ScriptHost JS/Python/VBA]
        CUI[CUIx / acad.pgp]
    end
    MW --> MP --> EL
    GV --> MP
    PP --> CMD
    CMD --> MOD
    MOD --> EL
    CON --> EL
    FMT --> EL

```

### 📊 视图系统

| 视图类型 | 核心类名 | 功能描述 |
| :--- | :--- | :--- |
| **旗板视图** | `FlangeViews` | 钣金展开图，支持 GB 标准件参数化 |
| **Housing 视图** | `HousingViews` | 箱体加工图 + GB 标准件图库 |
| **轴承视图** | `BearingViews` | 轴承选型计算，含 GB 标准数据库 |
| **法兰毂视图** | `FlangeHubViews` | 法兰 + 毂参数化设计 |
| **剖切视图** | `FloorPlanViews` | 剖切平面定义，支持剖面线填充 |
| **工艺视图** | `AircraftViews` | 工艺路线标注与尺寸链计算 |

### ✏️ 绘图命令速查

| 分类 | 常用命令示例 |
| :--- | :--- |
| **基础图元** | `line`, `circle`, `arc`, `rect`, `poly`, `spline`, `ellipse`, `mtext` |
| **编辑操作** | `move`, `rotate`, `scale`, `mirror`, `offset`, `trim`, `fillet`, `chamfer` |
| **阵列工具** | `join`, `arrayrect`, `arraypolar`, `arraypath` |
| **查询测量** | `dist`, `area`, `massprop`, `id`, `list` |
| **选择过滤** | `select_all`, `invert`, `select_layer`, `qselect` |
| **标注约束** | `dim`, `dims`, `constraint`, `perp`, `tangent2`, `qconnect` |
| **正交捕捉** | `ortho`, `snap`, `dyn`, `polar`, `otrack`, `tk` |
| **宏与通用** | `macrorec`, `undo`, `redo`, `clear`, `help` |

---

## 🛠️ 构建与开发

### 环境准备
- **Qt 6.10.1 MinGW 64** + **MinGW-w64 GCC 13.1.0** (`mingw1310_64`)
- **CMake ≥ 3.16** + **Ninja**
- **Windows COM 自动化** → `windeployqt` 部署工具

### 构建命令
bash
#
``` 方式1：Git Bash 脚本
./build.sh                    # 编译 → build/SketchCAD.exe
./build.sh test               # 运行测试 (403/0 通过)
./build.sh dist               # 打包 windeployqt 到 dist/
./build.sh package            # 生成安装包 → dist/SketchCAD-<version>-Setup.exe

# 方式2：CMake 直接构建
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Sketch
```

### 目录text
```
sketch-cad-cpp/
├── 📄 CMakeLists.txt          # 主构建脚本 (含 bench 子目录)
├── 📜 build.sh                # 构建/打包脚本 (Git Bash)
├── 📁 src/
│   ├── main.cpp               # 程序入口
│   ├── core/                  # 核心引擎 (无 UI 依赖)
│   │   ├── Element*.cpp + SketchModel.*    # 图元与数据模型
│   │   ├── LineModes... + Geometry + Snap  # 绘图模式与几何
│   │   ├── Modify + Trim + ConstraintSolver# 编辑与求解
│   │   ├── CommandInterpreter + CuiManager # 命令解析器
│   │   └── CadFormats        # JSON/DXF/STEP/DWG 格式支持
│   │   └── FlangeViews...    # 专用视图系统
│   ├── ui/
│   │   ├── MainWindow + GraphicsView       # 主界面与视图
│   │   ├── tools/ + CommandLine            # 交互工具
│   │   └── PrintEngine + ScriptHost        # 打印与脚本
│   ├── tests/                # SketchSelfTest (403 项自测)
│   └── bench/                # 性能基准测试与截图样本
├── 📚 docs/                  # 详细设计文档与资源
├── 📦 dist/                  # windeployqt 发布目录
└── 🛠️ tools/                 # 辅助工具 (DXF 导入导出
```

---

## 📖 详细文档

| 文档 | 内容概要 |
| :--- | :--- |
| [架构设计](docs/架构设计.md) | 整体架构与技术选型 |
| [编辑系统](docs/batch-edit-join-array-clipboard-design-analysis.md) | 批量编辑/阵列/剪贴板设计 |
| [多段线编辑](docs/polyline-edit.md) | 多段线编辑实现细节 |
| [交互设计](docs/grip-selection.md) | 夹点选择/边界区域/查询测量 |
| [格式支持](docs/dxf-import-expansion.md) | DXF 导入扩展与 DWG 原生设计分析 |
| [打印模块](docs/print-module.md) | 打印引擎实现 |
| [SDK 插件](docs/sdk-plugin-framework.md) | SDK 插件开发指南 |
| [过滤器](docs/selection-group-qselect-filter-design-analysis.md) | 选择组/qselect 过滤设计 |

---

## 📋 命令参考

### 基础text
```
命令名 [参数...]         # 例如: line 0,0 50,0
命令名_h [参数...]       # _h 后缀通常表示水平 (Horizontal) 模式
```

### 常用示例
bash
line
``` 0,0 50,0              # 绘制从 (0,0) 到 (50,0) 的线段
circle_cr 20,20 10         # 绘制圆心 (20,20) 半径 10 的圆
arrayrect 3 4 20 15        # 创建 3 列 4 行，间距 20x15 的矩形阵列
join                       # 合并相邻图元
qselect type=直线 layer=0  # 快速选择 0 层的所有直线
```

### 数据格式支持

| 格式 | 读/写 | 说明 |
| :--- | :--- | :--- |
| **JSON** | ✅ / ✅ | 内部原生格式，按键值对组织 |
| **DXF**  | ✅ / ✅ | AutoCAD DXF R12 兼容，支持 SPLINE 等 |
| **STEP** | ✅ / ✅ | ISO 10303 标准交换格式 |
| **DWG**  | ✅ / ❌ | R12 只读，CODEPAGE/AC1009 兼容 |

---

## ⚙️ 高级配置

### 注册表与路径
| 路径 | 说明 |
| :--- | :--- |
| `%LOCALAPPDATA%\SketchCAD` | 存放自动保存、崩溃日志、运行日志 |
| `HKCU\Software\SketchCAD` | COM 自动化注册项 |

### 安装/卸载参数
**静默安装：**cmd

```SketchCAD-<version>-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
```
**静默卸载：**cmd

```SketchCAD-<version>-Setup.exe /VERYSILENT
```

---

## 🧪 测试与基准

| 测试类型 | 工具/结果 | 说明 |
| :--- | :--- | :--- |
| **单元测试** | `SketchSelfTest` | **403 项通过 / 0 失败** (覆盖图元/编辑/约束/选择) |
| **GUI 测试** | offscreen 截图 | 支持高分屏/多语言/多分辨率比对 |
| **性能基准** | `SketchBenchmark` | **单线程 44.5ms** / p99 0.011ms |
| **编译质量** | `-Wall -Wextra` | **0 警告** |

---

## 📜 许可证与致谢

本项目采用 **私有许可证**，详见 [LICENSE.txt](LICENSE.txt)。

- **Qt 6** (LGPL v3): 核心 UI 框架
- **libredwg** (GPL): 用于 DWG 文件兼容读取
- 感谢所有为 SketchCAD 贡献代码和建议的开发者！

---

## 🆘 常见问题 (FAQ)

| 问题现象 | 建议解决方案 |
| :--- | :--- |
| **启动无响应** | 检查 Qt 环境路径 `C:\Qt\6.10.1` 是否正确配置 |
| **绘图显示异常** | 可能是兼容性问题，尝试关闭 `closed` 或 `hatch` 选项 |
| **COM 调用失败** | 确认已运行安装程序，注册表项已正确写入 |

<p align="center">
<em>SketchCAD —— 让 CAD 设计更简单 · 参数化 · 标准化</em>
</p>
```