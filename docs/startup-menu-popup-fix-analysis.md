# 启动界面菜单级联弹出缺陷分析与修复

> 现象：程序启动瞬间，窗口左上角出现一摞级联展开的菜单（文件/打印/追踪之外的
> 工作空间/CUIx、绘图类、视图示例类等顶级菜单同时弹出、逐级错位堆叠），覆盖在
> 启动画面上，造成界面凌乱：元素重叠、间距不一、排版混乱。

---

## 一、根因定位

### 唯一根因：对 QMenu 本体调用 setVisible(true) 使其作为弹出窗口立即显示

`MainWindow::applyWorkspaceVisibility()`（MainWindow.cpp）原实现：

```cpp
// 菜单可见性（按顶级菜单 objectName 判定）
for (QAction* a : menuBar()->actions())
    if (QMenu* m = a->menu())
        m->setVisible(cm.menuVisible(m->objectName()));   // ← 缺陷在此
```

调用链：`MainWindow 构造 → buildMenu()（挂 13 个顶级菜单）→ initCui() →
applyCui() → applyWorkspaceVisibility()`。

- `CuiManager::menuVisible(id)` 默认返回 `true`（无当前工作空间 = 全部可见）；
- 于是启动阶段对**全部 13 个顶级菜单**逐个执行 `QMenu::setVisible(true)`；
- Qt 语义：QMenu 自带 `Qt::Popup` 窗口标志，`setVisible(true)` 等价于把它作为
  弹出式窗口立即弹出（`QMenu::popup()` 本质就是「定位 + setVisible(true)」）；
- 连续 13 次弹出 → 每个弹出窗口按级联偏移错位堆叠在窗口左上角 → 截图中的
  「多份菜单同时展开、互相重叠、间距错乱」。

结论：截图中的全部视觉混乱（重叠/错位/凌乱）都是这 13 个被误弹出的菜单造成的
**单一根因**，并非菜单内容本身的排版缺陷。

### 排除项（排查过、确认无嫌疑）

| 嫌疑点 | 结论 |
| --- | --- |
| `rebuildCustomToolbars` 自定义工具条 | 均经 `addToolBar(tb)` 入驻主窗口，无浮动顶层窗口 |
| 菜单 objectName 注册一致性 | 13 个菜单 setObjectName 与 `registerBuiltinMenu` 注册表逐一对上 |
| `menuBar()->actions()` 其他遍历点 | `saveCurrentAsWorkspaceDialog` 只读不弹（但判定口径同步修正，见下） |
| 启动画面（main.cpp 自绘 splash） | 纯 QLabel 封面窗口，与菜单无关 |

## 二、修复

### 修复 1：菜单可见性改走菜单栏 QAction（核心修复）

```cpp
// 菜单可见性（按顶级菜单 objectName 判定）。
// ⚠️ 必须作用于菜单栏上的 QAction，而不是 QMenu 本体：
// 对 QMenu 调 setVisible(true) 会把它当作弹出窗口立即弹出（多个菜单级联堆叠
// 在窗口左上角）——启动时 initCui→applyCui 逐个「弹出」全部顶级菜单，正是
// 启动界面菜单堆叠/重叠凌乱的根因。改控 QAction 可见性：只增删菜单栏入口，
// 菜单本体保持隐藏、仅在用户点击时弹出（启动时默认隐藏、需要时才显示）。
for (QAction* a : menuBar()->actions())
    if (QMenu* m = a->menu())
        a->setVisible(cm.menuVisible(m->objectName()));
```

- `QAction::setVisible` 只控制该菜单在菜单栏上的**入口显隐**，绝不弹出菜单本体；
- 工作空间语义不变：`menuVisible=false` 的顶级菜单从菜单栏隐藏（与原意一致）；
- 菜单本体回到「默认隐藏、用户点击才弹出」的正常生命周期 → 启动界面整洁。

### 修复 2：工作空间快照口径同步（一致性防御）

`saveCurrentAsWorkspaceDialog` 以「当前可见 UI」生成工作空间可见集，原判定
`m->isVisible()` 读的是 QMenu 本体（平时恒为 false，只有被点开才 true）——
与修复后的可见性口径不再一致。同步改为读菜单栏动作：

```cpp
for (QAction* a : menuBar()->actions())
    if (QMenu* m = a->menu())
        if (a->isVisible()) menus << m->objectName();
```

保证「保存当前工作空间」快照与「应用工作空间」判定使用同一真相源（QAction）。

## 三、验证

| 项 | 结果 |
| --- | --- |
| SketchCAD 增量编译 | EXIT=0，`-Wall -Wextra` 零警告 |
| 全量测试链 `build.sh test` | EXIT=0，自测 630/0（核心层未改动，回归通过） |
| 行为推演 | 启动路径上唯一对 QMenu 的 setVisible 调用已消除；13 个菜单恢复「点击才弹」；工作空间隐藏菜单的入口显隐功能保持 |

## 四、经验沉淀

1. **QMenu 的 setVisible/show 就是 popup**：对挂在菜单栏上的 QMenu 做可见性
   操作必须经其菜单栏 QAction；直接 show 菜单本体 = 在屏幕上弹出它。
2. **「控制可见性」与「快照可见性」必须同口径**：两处遍历若一个读 QAction、
   一个读 QMenu 本体，工作空间的保存/恢复会互相矛盾。
3. 排查「启动期控件异常显示」优先 grep 启动调用链（构造 → init）上的
   `setVisible/show/popup/exec` 调用，再核对默认值分支（本例默认全显触达全部菜单）。
