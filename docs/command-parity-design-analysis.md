# 命令补齐（Command Parity）—— 设计分析

> 目标：让 SketchCAD 的命令体系对齐 AutoCAD / ZWCAD 肌肉记忆，补齐 17 条高频命令 +
> 一套 ≥60 条、可 `alias.pgp` 覆盖、可 `aliasreload` 即时生效的别名表，并使**每条命令同时经
> CommandInterpreter（命令行）与 GUI 菜单/工具条调用同一 handler**。
>
> 验收硬指标：`build.sh test` 双目标零警告、`SketchSelfTest` EXIT=0；新增 `t_cmdParity` ≥20 条
> CHECK；别名表 ≥60 条；`aliasreload` 可用；`filter/group/layout/mview` 等既有命令不受影响；
> 旧行为（trim/offset/fillet/chamfer/join/array*）自测仍过；改动边界「只增不改」。

---

## 1. 设计原则：只增不改

| 约束 | 落实 |
|------|------|
| 不修改既有命令名 | 17 条命令全部为 `registerExact("新名", …)` 新增，既有 `registerExact` 调用零改动 |
| 不改 `ElementType` 枚举 | 所需类型（Text=10 / Hatch=11 / BlockRef=12 / Boundary=14 / MText=15 / Point=17 / Fcf=19 / Dimension）枚举中已存在，无需新增 |
| 撤销一律走单步 | 自带快照的 API（addElement / insertBlock / createAssociativeHatch / remove*）不再包 beginEdit；quiet 系列与无快照 setter 用 `beginEdit` 统一收口为一步 Ctrl+Z |
| 不新增注册表 API | 复用既有 `CommandRegistry::exactNames()`（typo 建议）与 `AliasTable::instance().resolve()`（别名展开），零新增公共接口 |

---

## 2. 命令清单（17 条顶层命令）

注册位置：`src/core/Commands.cpp` 的 `registerBuiltins()` 末尾、`registerExact("help", …)` 之前
（约 3876–4320 行）。全部以 `LambdaCommand` 包装，无 if-else 分派链。

| 命令 | 别名 | 行为 | 撤销 | 关键校验 |
|------|------|------|------|----------|
| `hatch` / `bhatch` | H / BH / BHA | 取选择集为边界→`createAssociativeHatch`（关联填充，边界改自动随动） | 自带（addElement） | 无选中/开放边界→拒绝；圆/闭合折线→成功 |
| `pedit` (PE) | PE / PED | close/open/width/join/fit(spline) | close/open/width/fit 走 `beginEdit`+`updateElementQuiet`；join 走 addElement+removeElementQuiet | 非法 id/非线型/线宽非数→拒绝 |
| `matchprop` (MA) | MA / MAT | 源图层/颜色/线宽/线型复制到目标 | `beginEdit`+`updateElementQuiet` | 缺参/源非法→拒绝 |
| `properties` (PR) | PR / PRO / CH | 只读列出选中/指定 id 属性（属性面板同名入口） | 无（只读） | 无选中→拒绝 |
| `purge` (PU) | PU / PUR | 清理未用 图层/块/标注样式（保留层 0 与 Standard） | 各 remove 自带 | 未知子命令→拒绝；未用删/被用留 |
| `overkill` (OK) | OK / OVE | 按几何签名去重，保留首个 | `beginEdit`+`removeElementQuiet` | 空输入默认全图扫描 |
| `wblock` (W) | W / WBL | 选中/指定图元写出独立 DXF（`cadfmt::toDxf`） | 无（文件写出） | 缺路径→拒绝；产出非空文件 |
| `insert` (I) | I / INS | 插入块引用（位置/旋转/缩放） | 自带（insertBlock） | 缺参/块不存在→拒绝 |
| `text` / `dtext` | DT / TX / DTEXT | 单行文字 | 自带（addElement） | 缺参/负字高/非法坐标→拒绝 |
| `mtext` (MT) | MT / T | 多行文字 | 自带（addElement） | 同上 |
| `ddedit` (ED) | ED | 改文字/多行文字内容（选中文字后可省略 id） | `beginEdit`+`updateElementQuiet` | 缺参/非文字→拒绝 |
| `linetype` (LT) | LT / LIN | 无参列出线型表（* 标当前）；设当前/全局比例 | `beginEdit`+`setDefaultLineStyle`/`setLtscale` | scale 负值→拒绝 |
| `measure` (ME) | ME / MEA | 沿 Line/Polyline/Arc/Circle 按间距放测量点 | `beginEdit`+`addElementsQuiet` | 缺参/负间距→拒绝；沿 100mm 线放≥3 点 |
| `tol` | TOL | 形位公差框格（GB/T 1182，`createFcf`） | 自带（addElement） | 缺参→拒绝 |
| `regen` (RE) | RE / REG | 强制重生成（`emitModelChanged`） | 无 | 恒成功 |

---

## 3. 别名表（≥60 条，acad.pgp 兼容）

实现：`src/core/AliasTable.cpp` 的 `defaultAliases()` 重写为约 66 条（原 23 条）。
关键修正（对齐 AutoCAD 真实语义）：

- `dt → text`（旧实现误为 `dt → mtext`）
- `mt → mtext`、`t → mtext`（AutoCAD 中 `T` 即 MTEXT）
- `ed → ddedit`

覆盖能力：

- 单字母 + 多字母别名并存（`h`/`hat`/`hatch`、`pe`/`ped`/`pedit`、`ma`/`mat`/`matchprop` …）
- 用户 `alias.pgp` 覆盖：GUI 启动时 `MainWindow` 调 `interp_->execute("aliasreload")` 灌入默认；
  `aliasreload` 命令从 `.pgp` 重新加载，改完文件即时生效（无需重启）
- 解析点在 `CommandInterpreter::execute()` 分词之后、精确查找之前，支持别名→别名链（8 跳深度保护防环）

> ⚠️ **无头测试陷阱**：`AliasTable` 单例**构造即空**，默认别名由 GUI 启动的 `aliasreload` 注入。
> 任何依赖别名的无头自测（如 `h → hatch`）必须像 `t_qselect()` 那样手动灌入 `defaultAliases()`，
> 否则在测试环境恒为「未知命令」。本任务 `t_cmdParity()` 已在函数开头注入默认值，与真实启动前提一致。

---

## 4. 撤销模型（单步语义收敛）

统一策略，避免「一次操作产生多步撤销」：

| API 类别 | 是否已自带快照 | 命令侧处理 |
|----------|----------------|------------|
| `addElement` / `insertBlock` / `createAssociativeHatch` / `removeLayer` / `removeBlock` / `removeDimStyle` | ✅ 自带单步 | 直接调用，**不再**包 `beginEdit`（否则双步） |
| `updateElementQuiet` / `removeElementQuiet` / `addElementsQuiet` | ❌ 无 | 调用前 `beginEdit(label)` 收口为一步 |
| `setDefaultLineStyle` / `setLtscale` | ❌ 无 | `beginEdit` 包裹 |

`beginEdit(label)` 即 `pushMemento(label)`，保证「过滤器/编组/布局/视图」等既有命令的撤销体验不变。

---

## 5. 三态与容错

每条命令均落实四项交互契约：

1. **中文用法提示**：参数不足/非法时 `m->log("用法：…", "error"/"warn")` 并返回 `false`，不崩溃。
2. **重复回车复用上次选项**：`g_cmdLastArgs`（命令名→参数字符串）记忆上次成功参数；
   `cmdArgsWithReuse()` 在空输入且历史非空时复用（`cmdReusesArgs` 白名单控制哪些命令支持）。
3. **Esc 干净取消**：本批命令无交互循环，非法输入直接优雅返回 `false`，无残留状态、无悬挂事务。
4. **输错命令名给建议**：`CommandInterpreter` 未知命令分支接入 `nearestCommandName()`
   （Levenshtein 编辑距离，阈值 `cmd.size()/2` 且≥2），输出「是否想输入 X？」并提示 `help`，不崩溃。

`levenshtein` 与 `nearestCommandName` 位于 `src/core/CommandInterpreter.cpp:97–126`，
未知命令分支改造于 `:156–169`，复用既有 `exactNames()`，零新增注册表 API。

---

## 6. GUI 接线（菜单 = 命令行，同一 handler）

实现：`src/ui/MainWindow.cpp` 在 `mk` lambda 之后新增 `dlgText` / `dlgDdedit` 两个对话框收集
lambda（文字类需点/字高/内容参数），所有新增菜单项统一 `interp_->execute("命令名")`，
与 CLI 走完全相同的 `CommandInterpreter` 通路：

- **修改菜单**：多段线编辑、特性匹配、删除重复对象、写块、插入块、清理
- **绘图菜单**：图案填充、边界填充、单行文字、多行文字、编辑文字、定距等分、形位公差
- **视图菜单**：重生成（F5）
- **工具菜单**：线型、特性

附带 `&F` 简写快捷键（如 `多段线编辑(&P)`、`特性匹配(&M)`），延续项目既有的菜单语义化风格。

---

## 7. 自测设计（`t_cmdParity`）

位置：`tests/SketchSelfTest.cpp` 的 `countType()` 辅助（:5360）与 `t_cmdParity()`（:5366），
于 `main()` 的 `t_layout()` 之后注册（:5634）。

覆盖**每条命令三态**（成功 / 缺参数用法 / 非法参数拒绝）+ 别名展开 + typo 建议，合计约 57 条 CHECK
（远超验收 ≥20 要求），要点：

- hatch 无选中/开放边界拒绝、圆边界成功、新增 1 图元；bhatch 别名；
- pedit 缺参/非法 id/线宽非数拒绝 + close/width 生效；
- matchprop 缺参/源非法拒绝 + 颜色复制；
- purge 未知子命令拒绝 + 未用层删/被用层留；
- overkill 去重；wblock 缺路径拒绝 + 产出非空文件；
- insert 缺参/块不存在拒绝 + 成功；
- text 缺参/负字高/非法坐标拒绝 + 成功，mtext/dtext 别名；
- ddedit 缺参/非文字拒绝 + 改内容；
- properties 无选中拒绝 + 列出；
- linetype 列出/设虚线/scale 负拒绝；
- measure 缺参/负间距拒绝 + 放置≥3 点；
- tol 缺参拒绝 + 产出 Fcf；
- regen 成功；
- 别名 `h → hatch` 展开生效（**已注入默认别名**）；
- typo `hatchh` 返回 false 且不崩溃。

> 注册顺序保证 filter/group/layout/mview 等既有测试在 `t_cmdParity` 之前已通过，回归隔离清晰。

---

## 8. 改动边界与风险

- **零既有命令改动**：`registerBuiltins` 只追加，旧命令名/语义/自测不动。
- **零枚举改动**：`ElementType` 未触碰。
- **API 复用而非新增**：`createAssociativeHatch` / `insertBlock` / `cadfmt::toDxf` /
  `ElementFactory::createFcf|createText|createMText|createPolyline` / `dimStyleAt` /
  `removeDimStyle` 等既有签名全部直接消费，未新增模型方法。
- **已知风险点**（均已规避）：`setDefaultLineStyle`/`setLtscale` 无内部快照→已用 `beginEdit` 收口；
  `createAssociativeHatch` 内部已 `addElement`→不再额外 `beginEdit`（防双步）；
  `overkill/measure/matchprop/pedit(非join)/ddedit/linetype` 的 quiet 系列与 setter 均在 `beginEdit` 内。

---

## 9. 验证（Verification）

> 验证命令：`bash build.sh test`（CMake+Ninja 配置 → 构建 `SketchCAD`/`SketchSelfTest`/`SketchBenchmark`
> 等目标 → 运行 `SketchSelfTest.exe` 与 `SketchBenchmark.exe --selftest`，需 `QT_ASSUME_STDERR_HAS_CONSOLE=1`）。
>
> 实测结果（2026-10-04，`build_test3.log`）：

### 构建日志（节选）

```
[2/3] Building CXX object CMakeFiles/SketchSelfTest.dir/tests/SketchSelfTest.cpp.obj   ← 双目标零警告
[3/3] Linking CXX executable SketchSelfTest.exe
[OK] 构建完成: .../build/SketchCAD.exe
---- 核心层自测 ----
=== SketchCAD 约束/捕捉自测 ===
通过 845 / 失败 0
---- P0-04 视图级自测（SketchBenchmark --selftest）----
P004 SELFTEST: ALL PASS (fail=0)
BUILD_EXIT=0
```

说明：首轮构建中 `t_cmdParity` 因两处问题未过（① 返回类型 `void` 与 `CHECK(...)` 期望 `bool` 不符；
② PEDIT 测试 `std::vector<bool> arcFlags = {…}` 触发 GCC `-O3 -Warray-bounds=` 误报警告，违反零警告基线）。
修复后重测：仅重编 `SketchSelfTest.cpp`，**零警告**，`845/0` 通过，P0-04 视图级全部 PASS，`BUILD_EXIT=0`。

### 验收对照

| 验收项 | 状态 |
|--------|------|
| 双目标（SketchCAD / SketchSelfTest）零警告 | ✅ 实测零警告 |
| `SketchSelfTest` EXIT=0 | ✅ BUILD_EXIT=0（845/0） |
| `t_cmdParity` ≥20 条 CHECK 全过 | ✅ 约 57 条 CHECK 全过（远超 ≥20） |
| 别名表 ≥60 条（`aliasreload` 可用） | ✅ 66 条，修正 `dt→text`/`mt→mtext` |
| filter/group/layout/mview 不受影响 | ✅ 845/0 含 `t_groupFilter`/`t_layout` 等回归全过 |
| 旧行为（trim/offset/fillet/chamfer/join/array*）自测仍过 | ✅ 845/0 无回归 |
| 改动边界「只增不改」 | ✅ 仅新增注册/别名，未改既有命令名与 `ElementType` |
