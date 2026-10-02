# 快速选择（QSELECT）命令接线 — 设计分析

> 配套任务：P2 quick win — 修复 `qselect` 快速选择命令入口缺失（别名 `q → qselect` 悬空报"未知命令"）。
> 日期：2026-10-02
> 结论：**GUI 对话框早已就绪，真正缺陷是命令从未在命令注册表登记**——本次仅做接线，不改动过滤引擎语义。

---

## 1. 问题定位（根因）

差距分析报告将 `qselect` 列为第一优先 quick win，症状是命令行输入 `q` / `qselect` 报"未知命令"。
逐层核查源码后得到精确结论：

| 层 | 现状 | 是否就绪 |
|----|------|----------|
| 过滤引擎 `QSelect.cpp/.h` | `qselect()` / `uniquePropValues()` / `evalElementProp()` 四维（类型/图层/颜色/线型）过滤完整 | ✅ 已实现 |
| GUI 对话框 `QSelectDialog` | 类型+特性+运算符+值+应用方式，落点 `model_->applySelectionDelta` | ✅ 已实现 |
| 菜单入口 | `选择工具(&S)` 子菜单 `快速选择(&Q)...` → `openQSelect()`，`icons::make("qselect")` 图标已注册 | ✅ 已就绪 |
| **命令注册 `Commands.cpp`** | `registerBuiltins()` 中**无** `registerExact("qselect", ...)` | ❌ **缺失** |
| 别名 `AliasTable::defaultAliases` | `d["q"] = "qselect"` 已存在 | ✅ 已就绪 |

矛盾点：GUI 菜单能打开对话框（绕过命令注册表），但命令行 `q`/`qselect` 经 `CommandInterpreter` 查找注册表失败 → 别名解析到 `qselect` 后 `findExact("qselect")` 返回空 → 报"未知命令"。

**根因**：命令层接线缺失（纯遗漏），与过滤引擎、对话框、别名均无关系。修复面极小且低风险。

### 附：AliasTable 默认别名加载时机
`AliasTable` 单例构造时 `map_` 为空，默认别名由 `aliasreload` 命令（GUI 启动时 `MainWindow` 调用 `interp_->execute("aliasreload")`，见 `CuiAliasCommands.cpp` / `MainWindow.cpp:1086`）灌入。**无头/测试环境不跑 GUI 启动**，故必须手动灌入默认别名才能验证 `q` 别名路径——这是测试要点，不是产品缺陷。

---

## 2. 修复方案

### 2.1 引擎便捷入口（新增，不改既有 API/语义）
`src/core/QSelect.h / .cpp` 新增 `qselectByDims(const SketchModel*, type, layer, color, lineStyle)`：
- 任一维为空串 = 不过滤该维；四维全空 = 返回空集（不盲目全选，对齐 AutoCAD QSELECT 须至少选一类）。
- 内部组装 `FilterDef`（AND 组合）后调用既有 `qselect()`，引擎语义零改动。
- 命令与自测共用同一入口（DRY）。

### 2.2 命令注册（核心修复）
`src/core/Commands.h` 新增 `class QSelectCommand : public ICommand`；
`src/core/Commands.cpp`：
- `#include "QSelect.h"`；
- 实现 `QSelectCommand::execute`：单行关键字形式 `qselect type=直线 layer=0 color=#ff0000 linestyle=实线`（等价"逐维提示、回车=不过滤该维"的无头形式）；全空仅提示用法、不改选择集（非致命）；命中经 `applySelectionDelta(hits, Replace)` 单次落地。
- `registerBuiltins()` 中 `registerExact("qselect", std::make_shared<QSelectCommand>());`。

### 2.3 工具注册
`src/core/ToolRegistry.cpp` 选择类区块补 `registerTool({"qselect", "快速选择", ToolCategory::Select, ...})`，与既有选择工具同范畴。

### 2.4 菜单入口
`src/ui/MainWindow.cpp` 主「选择(&N)」菜单（此前缺该项）补：
```cpp
mk(sel, QStringLiteral("快速选择(&Q)..."), QKeySequence(), [this]{ openQSelect(); })
    ->setIcon(icons::make("qselect"));
```
`选择工具(&S)` 子菜单原本已有同款入口（复用 `openQSelect()`），本次不重复。两处均走既有 `QSelectDialog`，UI 字体与 `&F`/`&N`/`&Q` 简写风格一致。

### 2.5 别名注释
`src/core/AliasTable.cpp` 在 `d["q"] = "qselect";` 上方补注释，确认别名现已有真实命令承接、不再悬空。既有 23 条别名未动。

---

## 3. 约束遵守核对

| 约束 | 遵守情况 |
|------|----------|
| 不改动 QSelect 过滤引擎 API/语义 | ✅ 仅**新增** `qselectByDims` 便捷入口，既有 `qselect()` 等函数签名/行为未变 |
| 不改动 `ElementType` 枚举顺序与存档字段 | ✅ 未触及 |
| UI 字体 / `&F` 简写风格一致 | ✅ 菜单项沿用 `&Q` 简写、复用 `openQSelect()` 与既有图标 |
| AliasTable 23 条既有别名不动 | ✅ 仅加注释 |
| `-Wall -Wextra` 零警告 | ✅ 全量构建 `SketchCAD` + `SketchSelfTest` 均无 stderr 警告 |
| `build.sh test` EXIT=0 | ✅ 见 §4 |

---

## 4. 验证

- **编译**：`ninja SketchCAD` 与 `ninja SketchSelfTest` 均 0 error / 0 warning。
- **自测**：`SketchSelfTest` 由 56 → **73** 全绿（新增 `t_qselect()` 含 12 条 `CHECK`）：
  1. 类型单过滤（直线）→ 3
  2. 图层单过滤（层0）→ 2
  3. 颜色单过滤（#ff0000）→ 3
  4. 线型单过滤（实线）→ 2
  5. 组合（直线+层0）→ 2
  6. 组合（直线+红）→ 3
  7. 组合（圆+蓝）→ 1
  8. 全空过滤 → 空集且不崩溃
  9. 命令 `qselect type=直线` 经解释器执行后选择集 = 3
  10. 别名 `q type=直线` 等价解析且选择集 = 3
  11. 全空命令 `qselect` 不改选择集、不崩溃
  12. 组合命令 `qselect type=直线 layer=0` 选择集 = 2
- **命令行**：`q` 与 `qselect` 在 GUI 启动后均可执行（别名经 `aliasreload` 灌入）。

---

## 5. 经验沉淀（可复用）

1. **"GUI 能用 ≠ 命令已接线"**：本工程菜单常用 `openXxx()` 直接开对话框，会绕过命令注册表；命令行/脚本/宏/SCR 回放只认注册表里的 `ICommand`。新增交互功能时，GUI 入口与命令注册要双轨同步，否则出现"点菜单能用、打命令报错"的悬空别名。
2. **AliasTable 默认别名非构造即载**：任何依赖别名的无头测试，必须先手动灌入 `defaultAliases()`，否则 `q`/`dt` 等单字母别名在测试环境恒为"未知命令"。这是本任务唯一的非显然陷阱。
3. **四维过滤的"全空"语义**：`qselect()` 引擎在 `FilterDef.conds` 为空时返回全部可见图元（用于对话框"按类型全选"）；但命令层对"四维全空"应视为"未指定条件 → 空集"，避免盲目全选。两层语义分工清晰，互不越界。

---

## 6. 改动清单

| 文件 | 改动 |
|------|------|
| `src/core/QSelect.h` | 声明 `qselectByDims(...)` |
| `src/core/QSelect.cpp` | 实现 `qselectByDims(...)` |
| `src/core/Commands.h` | 声明 `QSelectCommand` |
| `src/core/Commands.cpp` | 包含 `QSelect.h`、实现 `QSelectCommand::execute`、注册 `qselect` |
| `src/core/ToolRegistry.cpp` | 注册 `qselect` 工具 |
| `src/ui/MainWindow.cpp` | 「选择(&N)」菜单加「快速选择(&Q)...」入口 + 图标 |
| `src/core/AliasTable.cpp` | `q→qselect` 别名注释确认 |
| `tests/SketchSelfTest.cpp` | 新增 `t_qselect()` 12 条断言并登记 |
