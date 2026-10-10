# MText FIELD 域 + Table 公式列 —— 设计分析文档

> 任务：MText 支持 FIELD 域（面积/图元计数/自定义特性）、Table 支持简单公式列，明细栏自动维护。
> 状态：✅ 已完成（构建零警告 + 自测 2375/0 EXIT=0，新增 36 断言全过）
> 日期：2026-10-09
> 关联文档：`text-mtext-style-field-design-analysis.md`（MTEXT/STYLE 基座）、`table-element-design-analysis.md`（Table 图元）、`inquiry-measure-design-analysis.md`（测量层）

---

## 1. 边界与现状（任务前勘察结论）

### 1.1 域求值已有单一入口

`TextStyle.h` 的 `resolveFields(text, model)` 已存在 **5 码机制**（`%<CODE>%` 占位）：

| 既有码 | 语义 | model 依赖 |
|---|---|---|
| `%<DATE>%` | 当前日期 | 否 |
| `%<TIME>%` | 当前时间 | 否 |
| `%<FILENAME>%` | 图纸文件名 | 否 |
| `%<FILEPATH>%` | 图纸完整路径 | 否 |
| `%<COUNT>%` | 全模型图元计数 | 是（elementCount） |

且 **四路消费已接好**：画布渲染（GraphicsView :698/:807）、打印（PlotRenderer :415/:513）、DXF/DWG 导出（CadFormats :562/:594/:913/:941）。**结论：新增域只需扩展 resolveFields 单点，四路零改动自动生效**——这是本次任务最重要的架构红利。

### 1.2 Table 已支持占位替换

Table 单元格渲染管线中既有 `%<>%` 占位替换（三处调用点），但没有公式求值层，也没有统一的 display 入口（三处各自内联"域替换"步骤，存在"同一语义多处手工拼装"隐患）。

### 1.3 applyChanges 已有钩子范式

`Modify.cpp::applyChanges` 末尾已有 `model->refreshHatchBoundaries(touchedIds)`——填充边界随动即由此实现（Quiet 语义：不压撤销、只读）。域失效重算完全复用该范式，在紧邻位置追加 `refreshFields(touchedIds)` 即可。

### 1.4 缺口清单

| 缺口 | 本任务补齐方式 |
|---|---|
| 无面积/周长域 | 新码 `AREA:id` / `PERIM:id`，复用 `inquiry::areaOfElement` |
| 无自定义图纸特性 | SketchModel 增 `drawingProps_` 特性表 + `PROP:name` 域 |
| Table 无公式 | 单元格 `=sum(a,b)` / `=count(a,b)`，统一 displayTableCell 管线 |
| 几何变更不联动域 | applyChanges 钩子 `refreshFields` |
| 无手动刷新命令 | `fieldupdate` 命令 |
| 开档不自动刷新 | AppSettings 新键 `fields/autoUpdateOnOpen`（默认开） |

---

## 2. 关键设计决策

### 2.1 双占位语法等价，单点解析

- `%<AREA:id>%` —— 对齐既有 5 码风格（AutoCAD FIELD 视觉惯例）。
- `{field:area:<id>}` —— 用户需求字面语法。

两者语义完全等价，`resolveFields` 的手工扫描器统一识别。快速路径：文本 `!contains("%<") && !contains("{field:")` 时原样返回，零开销。花括号扫描使用 `raw.mid(i, 7)` 精确定位 `{field:` 前缀（⚠️ Qt6 无 `QString::midRef`，Qt5 遗留 API，用 `mid()` 替代——本次构建踩坑实证）。

**决策理由**：不引入正则（QRegularExpression 每次渲染编译开销大、且 `%<>%` 嵌套边界手工扫描更可控）；不统一为单一语法（保双向兼容：用户输入两种写法都应得到相同结果，测试有等价断言）。

### 2.2 新域码语义

| 码 | 语义 | 求值来源 | 失败回落 |
|---|---|---|---|
| `AREA:id` | 封闭图元面积（mm²，两位小数） | `inquiry::areaOfElement(*model->element(id))` | `###` |
| `PERIM:id` | 封闭图元周长 | `inquiry::perimeterOfElement`（同族 API） | `###` |
| `PROP:name` | 自定义图纸特性值 | `model->drawingProp(name)` | `###` |
| `COUNT` | 全模型图元计数（既有） | `model->elementCount()` | `0` |

`AREA:id` 失败矩阵：断链（id 不存在）/ 非封闭图元（如 Line）/ 非法 id 参数（非整数）/ 求值异常 → 统一 `###`，不向渲染/打印/导出扩散。

### 2.3 崩溃安全设计

```
evalFieldCode()
  ├─ try { ...求值... } catch (...) { return {kFieldFallback, false}; }
  └─ FieldEvalResult{ text, ok } 双通道返回
```

- 求值函数全程 try/catch 包裹，任何异常（越界、空指针、几何库抛错）都回落 `###` 占位文本；
- `FieldEvalResult.ok` 标志让调用方（refreshFields 预热、自测断言）可区分"正常文本"与"失败回落"；
- 隔离原则：**域求值永不 throw 出 TextStyle 层**，渲染/打印/导出三路调用方零 try/catch 负担。

### 2.4 applyChanges 域失效钩子：命中规则

`refreshFields(changedIds)` 的命中判定是本任务最精巧的设计点：

```
宿主命中 ⇔ (宿主自身 id ∈ changedIds)  ← 宿主被改（如 Table 行内容变化 → 公式重算）
        ∨ (宿主域引用 id 集合 ∩ changedIds ≠ ∅)  ← 引用源被改（如 AREA:id 引用的圆被放大）
```

**自查修补记录**：初版只做了"引用命中"，漏掉了"宿主自身被改"——Table 的 `=sum` 公式不引用任何图元 id，用户编辑表格行内容后数量列不会重算。补 `selfChanged` 判定后两类场景全覆盖。

**refreshFields 的实质**（求值本身是渲染期实时的，为何还要"重算"？）：
1. 显式标记宿主（返回命中数，供命令/日志反馈"N 个域宿主已更新"）；
2. 预热求值吞异常（崩溃安全守卫点——在数据变更时刻提前暴露求值问题，而非推迟到下一次渲染）;
3. 为未来缓存型域值预留统一入口（当前无缓存，若后续加性能缓存，refreshFields 即失效广播点）。

### 2.5 Table 公式列语义

单元格文本以 `=` 开头即公式：

| 公式 | 语义 | 细则 |
|---|---|---|
| `=sum(a,b)` | 本列第 a..b 行数值求和 | 被引用格先过 resolveFields（支持面积列合计）；非数字按 0 计 |
| `=count(a,b)` | 本列第 a..b 行非空计数 | 同上 |

- 行号 **1 基、含端点**（与表格 UI 显示一致，符合直觉）；
- **防递归**：公式引用公式单元格 → `###`（不做图着色迭代求值，KISS）；
- 行号越界 / 未知函数名 → `###`。

### 2.6 displayTableCell 统一管线

```
单元格原始文本
  → evalTableCellFormula()   公式层（=开头才进入）
  → resolveFields()          域层（%<>% 与 {field:}）
  → 失败回落 ###             两层各自的回落
```

三路消费（GraphicsView :808 / PlotRenderer :514 / CadFormats :595/:942）全部收敛到此单一入口。**动机**：项目血泪铁律"同一语义两处手工拼装 ⇒ 一处更新一处漏"，本次重构顺带消除了 Table 显示语义的三处内联重复。

### 2.7 自定义图纸特性表

- 数据结构：`std::map<QString,QString> drawingProps_`（有序，导出/列出稳定）；
- 随图存档：顶层 JSON `properties` 键（与 elements/layers/blocks 平级），**缺键回落空表**——旧档兼容零成本；
- API 语义：空值=移除；重复 set 同值=幂等（不发 modelChanged）；变更发 modelChanged（属性面板/标题栏联动）；
- 读档接线两处：交互读档（MainWindow :4665）+ COM 批量出图（BatchPlotEngine :183），与既有"读档缺键预置保留"铁律一致。

### 2.8 AppSettings 新键

- 键名：`fields/autoUpdateOnOpen`（bool，默认 true）；
- ⚠️ **铁律再实证**：AppSettings.cpp 顶部必须补逐键 `using SettingKeys::kFieldAutoUpdate;`——本次第二次构建失败的根因正是漏了这行（编译器报 `'kFieldAutoUpdate' was not declared... did you mean SettingKeys::kFieldAutoUpdate?`）。项目记忆中已有此坑，仍然踩中，说明该铁律必须绑定在"新增 AppSettings 键"操作清单里，不能依赖记忆。

---

## 3. 实现落点（文件清单）

| # | 文件 | 改动 |
|---|---|---|
| 1 | `src/core/TextStyle.h` | `FieldEvalResult{text,ok}` 结构；`evalFieldCode`/`evalTableCellFormula`/`displayTableCell` 三声明；注释更新（8 码 + 双语法） |
| 2 | `src/core/TextStyle.cpp` | 核心：`evalFieldCode`（try/catch + 5 码兼容 + AREA/PERIM/PROP）、`resolveFields` 重写（双语法扫描）、`evalTableCellFormula`、`displayTableCell`；匿名 ns：`kFieldFallback`/`isFormulaCell`/`parseFormulaArgs`/`cellAt` |
| 3 | `src/core/SketchModel.h` | drawingProps 五 API + `refreshFields(changedIds={})` 声明 + `drawingProps_` 成员（:811） |
| 4 | `src/core/SketchModel.cpp` | props 实现（:2896 区）；`toDrawingJson` 加 `root["properties"]`；匿名 ns `collectFieldRefIds`/`refsHitChanged` + `refreshFields` 实现（:2300） |
| 5 | `src/core/Modify.cpp` | applyChanges 末尾钩子：`model->refreshFields(touchedIds);`（:1102，紧邻 refreshHatchBoundaries） |
| 6 | `src/ui/GraphicsView.cpp` | Table 单元格显示切 `displayTableCell(e, r+1, c, model)`（:808） |
| 7 | `src/ui/PlotRenderer.cpp` | 同上（:514，打印路） |
| 8 | `src/core/CadFormats.cpp` | 同上 ×2（:595/:942，导出路，传 nullptr） |
| 9 | `src/core/Commands.cpp` | `fieldupdate` LambdaCommand（:3253 区）；`prop` 命令（:3268 区，无参列出 / -r 删除 / 查询 / 设置） |
| 10 | `src/core/AppSettings.h` | `kFieldAutoUpdate` 键 + 访问器声明 |
| 11 | `src/core/AppSettings.cpp` | **`using SettingKeys::kFieldAutoUpdate;`（:16）** + specTable 条目（:208）+ 访问器实现（:506） |
| 12 | `src/core/CommandCatalog.cpp` | `fieldupdate`/`prop` 目录条目（usage 与实现同源） |
| 13 | `src/ui/MainWindow.cpp` | 读档接 `setPropsFromJson`（:4665）；ok 分支自动刷新（:4417，受开关控制，命中>0 写日志） |
| 14 | `src/com/BatchPlotEngine.cpp` | 读档接 `setPropsFromJson`（:183，与交互读档对齐） |
| 15 | `src/ui/MTextDialog.cpp` | 字段下拉扩展：面积/周长条目（遍历 selection 找首个封闭图元预填）、图纸特性条目 |
| 16 | `src/ui/TableDialog.cpp` | 公式用法提示四行（`=sum(2,9)` 等） |
| 17 | `tests/SketchSelfTest.cpp` | `t_fields()` 36 断言 + main 注册（:9025） |

---

## 4. 语法与语义规格

### 4.1 MText FIELD 域

```
%<AREA:12>%          → id=12 图元面积，如 "80.00"（mm²）
{field:area:12}      → 等价写法
%<PERIM:12>%         → id=12 图元周长
%<PROP:项目名称>%    → 自定义图纸特性 "项目名称" 的值
%<COUNT>%            → 全模型图元数（既有）
%<DATE>% / %<TIME>% / %<FILENAME>% / %<FILEPATH>%   （既有，语义不变）
```

失败回落：`###`（断链/非封闭/缺特性/异常/未闭合占位透传原样）。

### 4.2 Table 公式

```
=sum(2,9)    本列第 2..9 行数值求和（1 基含端点；被引用格先过域替换；非数字=0）
=count(2,9)  本列第 2..9 行非空计数
```

失败回落：`###`（公式引用公式格=防递归 / 行号越界 / 未知函数名）。

### 4.3 命令

```
fieldupdate                       手动刷新全部域宿主，日志"N 个域宿主已更新"
prop                              列出全部自定义图纸特性
prop <名称>                       查询
prop <名称> <值...>               设置（值可含空格，toks[3..] 拼接）
prop -r <名称>                    删除
```

### 4.4 配置

| 键 | 类型 | 默认 | 语义 |
|---|---|---|---|
| `fields/autoUpdateOnOpen` | bool | true | 打开文档时自动 refreshFields() |

---

## 5. 约束映射核对表

| 用户约束 | 落实方式 | 验证 |
|---|---|---|
| MText/Table 序列化向后兼容 | 域即普通文本（不新增字段/图元类型）；`properties` 缺键回落空表；Table 公式存原文，旧版本读档显示 `=sum(...)` 字面量 | t_fields ⑦组：域文本原文入档 + 缺键空表断言 |
| 域求值只读不改几何 | 求值只调 `inquiry::areaOfElement` 等只读 API；refreshFields 全程 Quiet（不压撤销、不发 geometry 变更） | 代码审查 + refreshFields 返回值断言（命中数，不改 elements 数量） |
| 崩溃安全（### 回落） | evalFieldCode/evalTableCellFormula 全 try/catch；kFieldFallback 常量；FieldEvalResult.ok | t_fields ④⑤组：断链/直线/非法 id/未知码/递归/越界共 7+ 条回落断言 |
| 面积域随边界变更联动 | applyChanges → refreshFields(touchedIds) 引用命中 | t_fields ①组：80×2 长方形 → 160 断言 + refreshFields 三态断言 |
| 明细栏数量列公式 | displayTableCell 统一管线 + =sum | t_fields ⑤组：=sum 6.00/=count 2 断言 |
| 自测新增 ≥8 断言 | 实际 36 条（九组） | 2375/0 EXIT=0 |
| 零警告 | 双目标 -Wall -Wextra | 构建日志 grep 计数 0 |

---

## 6. 验证方案与证据回填

### 6.1 构建

```
cmake --build <build>/ --target SketchCAD SketchSelfTest -j 1
结果：双目标链接成功，13m40s，-Wall -Wextra 零警告（grep -ci "warning|error" = 0）
```

### 6.2 三次编译错误与修复（过程记录）

| # | 错误 | 根因 | 修复 |
|---|---|---|---|
| 1 | `midRef` 不存在 ×2 | Qt6 移除 Qt5 的 `QString::midRef` | 改 `mid(i, n)` |
| 2 | `'model' was not declared` | `evalTableCellFormula` 参数写成 `/*model*/` 注释掉名字 | 恢复参数名 |
| 3 | `'kFieldAutoUpdate' was not declared; did you mean SettingKeys::...?` | AppSettings.cpp 漏逐键 using 声明（项目铁律坑，再次实证） | :16 补 `using SettingKeys::kFieldAutoUpdate;` |

另有两处**构建前自查修复**（未进构建日志）：
- sum 分支初版写 `v;` 无效果语句——-Wunused-value 隐患且逻辑绕弯，重写为 `resolveFields(cell, model).simplified()` 单次解析；
- refreshFields 初版命中判定漏"宿主自身被改"场景（§2.4 已述），补 `selfChanged` 判定。

### 6.3 t_fields 自测（36 CHECK 九组）

| 组 | 内容 | 断言要点 |
|---|---|---|
| ① 面积域联动 | 80×2 长方形 + MText `%<AREA:id>%` | 放大后 80.00→160.00；refreshFields([该id])=1；refreshFields([无关id])=0；refreshFields()=全模型命中 |
| ② 双语法等价 | `{field:area:id}` vs `%<AREA:id>%` | 两写法求值结果逐字符相等 |
| ③ 周长域 | 长方形 PERIM | 36.00 |
| ④ 回落矩阵 | 断链/直线(非封闭)/非法id/未知码/纯文本 | 五行 `###` 或原文透传 |
| ⑤ Table 公式 | =sum/=count/递归/越界/非公式 | 6.00 / 2 / `###` / `###` / COUNT 管线直通 |
| ⑥ PROP | 写入/求值/缺名/覆盖/移除 + prop 命令 | 值往返 + 命令输出断言 |
| ⑦ 兼容 | toDrawingJson properties 键 / 域文本原文入档 / 缺键空表 | 序列化往返 |
| ⑧ fieldupdate 命令 | 有域/无域两态 | 返回命中数 + 日志 |
| ⑨ AppSettings | 默认开/false 往返/持久化往返 | 三断言 |

### 6.4 全量回归

```
SketchSelfTest：2375 通过 / 0 失败 / EXIT=0（基线 2339 + 新增 36）
```

---

## 7. 已知限制

| 限制 | 原因 | 影响 | 后续改善 |
|---|---|---|---|
| DXF/DWG 导出层 AREA/PERIM 域为 `###` | CadFormats 与 SketchModel 刻意解耦（导出签名只收 `std::vector<Element>`，无 model 上下文），导出传 nullptr | 导出的图纸中面积域为静态快照占位 | 方案：导出前在 model 侧预解析文本（`resolveFields` 后写入导出缓冲），保持解耦不破坏 |
| 公式仅限本列区间 sum/count | 需求范围为"简单公式列" | 不支持跨列引用/单元格级引用 | 需要时扩展 parseFormulaArgs 支持 `B2` 式坐标 |
| 域值无缓存 | 求值是渲染期实时计算 | 超大图纸 + 大量域时有重复计算开销 | 若成为瓶颈，refreshFields 即缓存失效广播点（§2.4 预留） |

---

## 8. 结论

- **架构上**，本次是"单一入口 + 钩子范式"两个既有模式的复用型扩展：resolveFields 单点扩域（四路消费零改动）、applyChanges 追加钩子（refreshHatchBoundaries 先例照搬）。新代码集中在 TextStyle.cpp（求值核）与 SketchModel.cpp（失效重算 + 特性表），侵入面小。
- **健壮性上**，崩溃安全三层兜底（try/catch → ok 标志 → ### 回落）+ 命中规则双通道（引用源变化 ∨ 宿主自身变化），并防递归/越界。
- **兼容性上**，域即文本、特性表缺键回落，旧档/旧版本双向无损。
- **验证上**，36 断言九组覆盖用户四项验收，全量 2375/0，零警告。
- **过程沉淀**：三条编译错误中两条是 API 版本差（midRef）、一条是项目已知铁律（AppSettings 逐键 using）——后者第二次实证，应固化进"新增 AppSettings 键"的操作清单。
