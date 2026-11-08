# SketchCAD v1.1.1 启动崩溃根因分析与修复记录

> 状态：已定位、已修复、已验证（2026-11-08）
> 性质：Hotfix，功能零变更，仅调整 `MainWindow::buildDocks()` 内部的初始化顺序

---

## 1. 现象

程序启动后主界面尚未显示即退出，多次运行稳定复现。用户只能看到启动封面，
随后进程消失；无 Qt 报错弹窗、无 stdout/stderr 输出。

## 2. 实证取证链

| 步骤 | 命令 / 来源 | 结果 |
| --- | --- | --- |
| ① 退出码 | `timeout 20 ./SketchCAD.exe` | `EXIT=139`（`SIGSEGV`），stdout/stderr 均为空 |
| ② 诊断日志 | `%LOCALAPPDATA%\SketchCAD\logs\SketchCAD.log` | `fatal: 未处理异常崩溃，已生成转储`，异常码 `0xc0000005` |
| ③ 崩溃转储 | `%LOCALAPPDATA%\SketchCAD\crashes\*.dmp` | 三次启动各生成一份，稳定复现 |
| ④ 调用栈 | MinGW `gdb -batch -ex run -ex bt` | 见下 |

```
Thread 1 received signal SIGSEGV, Segmentation fault.
#0  0x00007ffcf0bd9179 in QDockWidget::setFeatures(QFlags<QDockWidget::DockWidgetFeature>)
                          from ...\dist\Qt6Widgets.dll
#1  0x00007ff6d73ebec7 in SketchCAD::MainWindow::buildDocks()
#2  0x00007ff6d7443d16 in SketchCAD::MainWindow::MainWindow(QWidget*)
#3  0x00007ff6d78cd761 in main()
```

关键点：崩溃点在 `MainWindow` **构造期**，早于 `main()` 第 266 行的 `w.show()`，
因此主窗口永远不会被显示 —— 与用户描述完全吻合。

## 3. 根本原因

P2 阶段接入 AI 自然语言助手时，`MainWindow::buildDocks()` 被追加了两处新代码：

1. 统一设置停靠窗口特性的遍历列表末尾加入 `aiDock_`；
2. 增加 `aiDock_->setVisible(false)` 初始隐藏。

但 AI 面板与停靠窗口的**构造语句**被放在 `buildDocks()` 的函数末尾（原 4036–4042 行），
位于上述遍历与隐藏调用**之后**。于是：

```
for (auto* d : { ..., designDock_, aiDock_ })  // aiDock_ == nullptr
    d->setFeatures(...);                       // nullptr->setFeatures() → 0xc0000005
```

即「使用先于构造」的空指针解引用。这是典型的**初始化顺序缺陷**，不是 Qt 缺陷、
不是配置文件损坏、也不是 `initAiAssistant()` / LLM 后端运行期异常。

排查过程中还排除了两个干扰项：

- `initDatabase()` / `AppSettings` 配置：日志显示启动路径诊断与设置载入均在崩溃点之前顺利完成；
- 后续测试中出现的 `fatal 0xc0000005`：时间恰好等于 `timeout` 的 12s / 18s 强杀时刻，
  是 MSYS 在 Windows 上以异常方式终止进程产生的假崩溃，与应用无关。

## 4. 改动范围

仅 `src/ui/MainWindow.cpp` 的 `buildDocks()`：

- 把 `AiAssistantPanel` / `aiDock_` 的创建、`setObjectName` / `setWidget`
  前移到 `designDock_` 之后、所有 `addDockWidget` 之前；
- 把 `addDockWidget(Qt::RightDockWidgetArea, aiDock_)` 一并前移；
- 删除函数末尾的重复创建块；
- 增加一段注释，说明「必须先构造再进遍历」的约束，防回归。

**未改动**：任何既有功能、布局尺寸、菜单项、模型行为、AI 编排逻辑、配置键、
启动流程顺序（`buildDocks()` 仍在 `buildMenu()` 之前，`initAiAssistant()` 仍在
`loadAppSettingsOnStartup()` 之后）。

## 5. 修复后的预期行为与验证

预期：启动封面正常显示 → 最短停留结束 → 主窗口正常显示并持续运行；
AI 助手停靠窗口初始隐藏，可从「视图 > 面板」或「工具 > AI 助手」唤起。

| 验证项 | 结果 |
| --- | --- |
| 双目标 `-Wall -Wextra` 零警告构建 | ✅ 通过 |
| 全量自测 `SketchSelfTest` | ✅ `通过 3165 / 失败 0`，`EXIT=0` |
| 进程存活 | ✅ `timeout 12/18/45` 均为 `EXIT=124`（被超时终止，而非提前退出） |
| 主窗口实证 | ✅ `MainWindowTitle = SketchCAD`，`MainWindowHandle != 0`，`Responding = True` |
| 修复前后对比 | ✅ 修复前 gdb 栈顶 `QDockWidget::setFeatures`；修复后无应用级崩溃栈 |
| `build ↔ dist` 字节一致 | ✅ `cmp` 一致 |

> 注：gdb 附加运行时会在沙箱注入 DLL 的未知地址上收到一条线程级 `SIGSEGV`，
> 栈不可解析且无对应应用 fatal 日志；该信号只在调试器附加时出现，属环境噪声。

## 6. 防回归建议

- 同类「新增停靠窗口」必须遵循 `构造 → addDockWidget → 统一遍历/隐藏` 三段顺序；
- 可考虑把统一遍历改为在创建循环内部立即设置 `setFeatures`，从结构上消除
  「列表枚举」与「实际构造」两处手工同步的漂移风险（本次为最小改动，未做此重构）。

## 7. 发行影响

本修复随 **v1.1.1** 发布，同时提供 `Setup.exe`（Inno Setup 6）与 `Setup.msi`（WiX v5）
两种安装程序，二者打包自同一个 `pkg-stage` 目录，文件集合一致。
详见 `RELEASE_NOTES.md`。
