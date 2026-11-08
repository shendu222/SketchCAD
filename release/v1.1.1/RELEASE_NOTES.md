# SketchCAD v1.1.1 发布说明（Release Notes）

- 发布版本：**v1.1.1**
- 发布日期：2026-11-08
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 版本性质：**启动故障修复版（Hotfix）**，功能与 v1.1.0 保持一致
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 一、本版本解决的问题

**症状**：程序启动后主界面尚未显示即退出，多次运行稳定复现。

**根本原因**（实证定位）：`MainWindow::buildDocks()` 中新增的 AI 助手停靠窗口 `aiDock_`
被加入了统一的 `setFeatures` 遍历列表与 `setVisible(false)` 调用，但其构造语句被放在函数末尾 ——
遍历执行时 `aiDock_` 仍为 `nullptr`，对空指针调用 `QDockWidget::setFeatures()` 触发
`0xc0000005`（访问违例）。崩溃发生在 `MainWindow` 构造期，早于 `main()` 中的 `w.show()`，
因此用户只能看到启动画面一闪而过，主界面永远不出现。

gdb 调用栈（修复前）：

```
#0 QDockWidget::setFeatures(...)  Qt6Widgets.dll
#1 SketchCAD::MainWindow::buildDocks()
#2 SketchCAD::MainWindow::MainWindow(QWidget*)
#3 main()
```

**修复范围**：仅 `src/ui/MainWindow.cpp` 的 `buildDocks()` —— 把 AI 助手面板与停靠窗口的
创建、`addDockWidget()` 前移到遍历之前，删除函数末尾的重复创建块。不改变任何既有功能、
布局参数、菜单项或模型行为。

**修复后预期行为**：启动画面正常展示 → 停留结束 → 主窗口正常显示并保持运行；
AI 助手停靠窗口初始隐藏，可从「视图 > 面板」或「工具 > AI 助手」唤起。

## 二、发行产物（两种安装程序）

| 文件 | 格式 | 说明 |
| --- | --- | --- |
| `SketchCAD-1.1.1-Setup.exe` | Inno Setup 6 引导式安装程序 | 含许可协议页、可选桌面图标与文件关联、开始菜单与卸载入口 |
| `SketchCAD-1.1.1-Setup.msi` | Windows Installer (WiX) 程序包 | 适用于 `msiexec` 静默部署 / 组策略分发，支持版本升级 |

两者打包的是**同一批**经校验的程序文件（主程序、Qt 运行时、图像与 SQL 插件、
`tools/dwg` 转换器、GPLv3 `COPYING`、`LICENSE.txt`、README、启动封面与剖面图案）。
校验值见 `dist/checksums.sha256`。

安装示例：

```
SketchCAD-1.1.1-Setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
msiexec /i SketchCAD-1.1.1-Setup.msi /qn
```

## 三、质量校验

- 双目标（SketchCAD / SketchSelfTest）`-Wall -Wextra` 零警告构建通过。
- 全量自测：`通过 3165 / 失败 0`（EXIT=0）。
- 主程序实跑：主窗口标题 `SketchCAD`、窗口句柄有效、`Responding = True`，进程稳定驻留。
- `build/SketchCAD.exe` 与 `dist/SketchCAD.exe` 字节一致。
- 版本一致性：CMake `project(VERSION 1.1.1)` → `version.h` / `app.rc` / 安装包名 / 安装器属性同源。

---

## 附录：历史版本 v1.1.0 发布说明

> 以下为 v1.1.0 原始发布说明，保留作为版本历史。

- 发布版本：**v1.1.0**
- 发布日期：2026-10-31
- 适用平台：Windows（Qt 6 + MinGW 构建）
- 代码托管：GitHub（`shendu222/sketchCAD`）与 Gitee（`markhenrychen/sketch-cad`）同步，内容一致

---

## 概述

SketchCAD 是一款基于 **C++17 / Qt 6** 的二维 CAD 应用，覆盖草图绘制、几何辅助、DXF/DWG
导入导出、参数化图库、标题栏与批量出图、明暗主题等完整制图工作流。本版本（v1.1.0）在既有功能
基础上修复了若干影响使用体验的缺陷，并增强了文字排版与界面可读性。

## 本版本重点修复

- **标题栏填写不再覆盖标签**
  标题栏每个栏目由「灰色标签 + 黑色字段」两个独立图元组成；旧实现两者的锚点都落在单元格中心，
  填写后字段值会压住标签。现改为**「标签带 + 值区」两区分离**布局：标签固定在单元格顶部
  标签带、填写值居中于下方值区，两者永不重叠。写回路径（`fitTitleBlockText`）与批量签名
  （`batchSign`）统一收敛到值区单一入口；自测新增「同单元格标签块与值块零重叠」回归断言。

- **明暗主题工具栏文字可读性**
  亮色主题工具栏文字显式设为纯黑（`#000000`），暗色主题保持原生白字不变；主题切换时重新生成
  工具栏外观，确保明暗两态均正确显示。

- **图标主题化**
  工具栏/菜单图标改用引擎化绘制（`ThemeIconEngine`），墨色在绘制时按主题实时解析——
  暗色下为浅墨、亮色下为深藏青，消除「暗底深字不可见」问题。

## 主要能力（摘要）

- 草图约束求解器、多种图元与尺寸标注、GD&T 标注。
- DXF（R12–R2018）导入导出；DWG 经 LibreDWG 桥接导入、自研 R12 导出。
- 参数化国标图库（如 GB/T 5782 六角头螺栓标注图）。
- 标题栏表格化、批量三件套（拆分 / 排图 / 签名）、填充联动、FIELD 域与 Table 公式列。
- 崩溃不丢稿（AutoSave + mini-dump）、鲁棒性 / Fuzz 防护、商业安装包（Inno Setup）。
- 国际化简档、高 DPI 支持、绘图区域亮 / 暗主题。

## 安装与构建

- **安装包**：运行 `SketchCAD-1.1.0-Setup.exe`（安装包内版本与程序版本同源，均为 1.1.0）。
- **源码构建**：克隆仓库后按 `README` 使用 `CMake + Ninja + MinGW` 构建。

## 质量校验

- 全量自测通过（3157 / 0）。
- 构建产物经 `cmp build↔dist` 字节一致性校验。
- 标签 `v1.1.0` 同时推送 GitHub 与 Gitee，两平台发布内容一致。
