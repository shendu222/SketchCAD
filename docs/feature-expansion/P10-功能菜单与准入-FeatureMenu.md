# P10 功能菜单与准入：P1~P9 菜单集成层（FeatureMenu）

> 用户需求（非方案提示词，为 P1~P9 功能扩充完成后的 UI 集成指令）：
> 「为 P1 至 P9 的各项功能在用户界面中添加对应的菜单入口。要求实现菜单准入条件判断逻辑：
> 当满足预设条件时，菜单项可正常点击进入；当条件不满足时，菜单项应置为灰色禁用状态，
> 禁止进入。请提供完整的菜单注册、条件评估、界面状态更新及交互反馈实现。」
>
> 本文记录该需求的落地：新增 `src/core/FeatureMenu.{h,cpp}`（纯逻辑目录 + 准入评估）、
> `MainWindow` 「扩展工具」顶层菜单（32 项 / 9 分组）、`aboutToShow` + 选择集信号驱动的
> 状态刷新、以及全部触发处理器的交互反馈。

---

## 1. 背景与目标

P1~P9 九项功能扩充完成后，入口梳理结论是：**仅 P2（xh 序号球标）有命令行入口，
其余均为库能力**（纯函数/门面 API，无命令、无菜单、无面板接线）。用户要求补齐 UI 入口，
并附带明确的准入语义：

| 需求点 | 落地形式 |
|---|---|
| 菜单入口 | 顶层「扩展工具(&R)」菜单，9 个 P 分组子菜单共 32 项 |
| 准入判断 | `FeatureMenu::evaluateFeature(model, id) → {enabled, reason}`（core 纯逻辑） |
| 置灰禁用 | `QAction::setEnabled(false)` + `statusTip/toolTip` 回显禁用原因 |
| 界面状态更新 | `QMenu::aboutToShow` 重算 + `selectionChanged` 即时刷新 + 文档切换自动换绑 |
| 交互反馈 | 每个处理器结尾 `model_->log(info/warn)`；查询类结果经 `QMessageBox` 展示 |

---

## 2. 思考与方案选型

### 2.1 分层：core 纯逻辑判定 + UI 薄壳

准入规则**不写在 MainWindow 里**，而是独立成 `src/core/FeatureMenu.{h,cpp}`：

- 只依赖 `SketchModel` 只读查询（selection / elementCount / bom / elements），**零 Qt UI 依赖**；
- 无头可测（`t_featuremenu` 95 断言直测，不需 QApplication 弹菜单）；
- UI 壳（MainWindow）只剩三件事：注册 QAction、`aboutToShow` 调 `evaluateFeature`、触发分发。

这与工程既有范式一致（CommandCatalog / TitleBlockLayout / DocManager 均为「core 纯逻辑 + UI 薄壳」）。

### 2.2 单一数据源：featureCatalog()

菜单结构（分组/标题/提示）的唯一事实源是 `featureCatalog()`：

- MainWindow 的菜单、`updateFeatureMenuState` 的提示回显、自测的目录断言，全部读同一张表；
- **禁止两处手工维护**（工程血泪：同一语义两处拼装 ⇒ 一处更新一处漏，见 BatchPlotEngine JSON 导出前辙）；
- 分组顺序 = 目录首次出现顺序（P1~P9），新增功能追加到枚举末尾即可自动入菜单。

### 2.3 准入条件与库函数真实语义严格一致

准入不是「拍脑袋阈值」，而是逐项对照库实现的反向条件——**评估放行的操作，库层不得再因同类原因失败**：

| 功能 | 准入条件 | 依据（库语义） |
|---|---|---|
| 中心线（kCenter） | 选中含直线或圆 | `makeCenterLine` 首 ref 须 line/circle |
| 圆心标记（kCircleMark） | 选中含圆 | `circleInfo` 仅认 Circle |
| 角点/两线交点（kCorner/kTwoLinesCross） | 选中 ≥2 条直线 | `refs.size() < 2` → fail |
| 对称板（kPlate） | 选中矩形（圆兜底） | `kPlate` 分支 |
| 序号球标 | 选择集非空 | `xh` 命令：无 id 无选中 → err |
| 明细表 | `bomCount() > 0` | 空表打开空面板无意义 |
| 符号标注 / 视图工具 | 选择集非空 | 锚点取选中质心（无选中无锚点） |
| 截面特性 / 重量估算 | 恰好选中 1 个闭合轮廓 | `closedProfilePolygon` 可提取 |
| 机械图拆分 | `elementCount() > 0` | 空图无可拆 |
| 自动排图 | 存在图框层矩形 | packShelf 输入 = 图框矩形 |
| 批量签名 | 存在 `tb:field:` 元素 | `batchSign` 依赖标题栏字段 |
| 齿轮 / 紧固件 / 批量BOM / 变量表 | 无条件 | 参数对话框 / 外部文件 / 会话级 |

未知 id → **保守禁用**（防新增枚举漏配规则被误放行）；空指针图纸 → 仅无条件类启用。

### 2.4 交互反馈：禁用原因即可读文案

每个禁用态 `reason` 都是「缺什么、怎么办」式中文文案（如「请先选中两条直线（角点中心线过其交点）」），
经 `statusTip` + `toolTip` 双通道回显——悬停置灰项即得知原因，而非盲目点击无效。
触发时的反馈统一走 `model_->log(msg, "info"/"warn")` 进日志面板；查询类（截面特性/重量/批量BOM/排图）
另弹 `QMessageBox` 展示完整结果。

---

## 3. 核心算法与设计思路

### 3.1 目录与准入（FeatureMenu）

```
FeatureId（32 项，只增不改）
 ├─ P1 中心线×10（kCenter/kCross/kCircleMark/kCorner/kPlate/kFullCircle/kAngle/kHole/kThroughHole/kTwoLinesCross）
 ├─ P2 序号球标 / 明细表
 ├─ P3 符号标注×6（datum/weld/taper/centerhole/elevation/rotation）
 ├─ P4 渐开线齿轮
 ├─ P5 六角螺栓 / 六角螺母 / 平垫圈
 ├─ P6 向视符号 / 孔轴投影 / 转角投影
 ├─ P7 截面特性 / 重量估算 / 批量 BOM
 ├─ P8 变量表
 └─ P9 机械图拆分 / 自动排图 / 批量签名

evaluateFeature(model, id) → FeatureAvailability{enabled, reason}
```

共享工具（core，UI 与准入同源）：

- `isCenterLineFeature(id)` / `centerLineKindFor(id)`：P1 菜单项 ↔ CenterLineKind 映射；
- `isClosedProfile(e)` / `closedProfilePolygon(e, out)`：闭合轮廓谓词与多边形提取**同源**
  （Circle→128 边形、Rectangle→四角展开、Polygon/Polyline(closed)/Boundary→控制点）——
  评估放行 ⇔ 提取必成功，杜绝「放行了却提取失败」。

### 3.2 菜单注册与状态更新链路（MainWindow）

```
buildMenu()
  └─ 「扩展工具(&R)」（objectName=menuExt）
       ├─ 9 个分组子菜单（图标按组分派：line/field/dim/block/attributes/views/query/constraint/modify）
       └─ 32 个 QAction（objectName=feature_<id>；statusTip=目录 tip）
            └─ triggered → onFeatureTriggered(id)

状态刷新（三源）：
  ① QMenu::aboutToShow        → updateFeatureMenuState()   （展开即重算，永不过期）
  ② SketchModel::selectionChanged → updateFeatureMenuState()（hookActiveDoc 随活动文档换绑）
  ③ switchToDoc → hookActiveDoc 重绑 → 新文档选择集/内容生效

updateFeatureMenuState():
  for (id, action) in featureActs_:
      av = evaluateFeature(model_, id)
      action->setEnabled(av.enabled)
      action->setStatusTip(av.enabled ? tip : av.reason)   // 禁用原因即反馈
```

`featureActs_` 在 buildMenu 入口 `clear()`——语言切换（`rebuildMenusForLanguage`）重跑
buildMenu 时不会累积重复登记。

### 3.3 触发分发与「双保险」

`onFeatureTriggered(id)` 先复评一次准入（旁路触发：命令面板/脚本/自定义工具条不经菜单），
不满足 → log warn 并返回；随后分发：

- **P1**：`selectedRefElements()` → `makeCenterLine(kind, refs, CLParams{layer=currentLayer})` → `insertGeneratedElements`；
- **P2 球标**：`interp_->execute("xh")`——与命令行**同一 handler**（单一入口，不复制实现）；
- **P2 明细表**：只读 `QTableWidget` 对话框（6 列：序号/代号/名称/材料/单重/备注）；
- **P3/P6**：`QInputDialog` 收参数 → `SymbolFactory::mkSymbol` / `buildDirectionSymbol` 等 → 插入；
- **P4/P5**：`PartLibrary::instance().generate(id, params, out)` → 平移到锚点（选中质心或视图中心）→ 插入；
  规格回落 note（G3 门禁）经 log warn 呈现；
- **P7**：`closedProfilePolygon` → `SectionProps::compute/massGrams` / `BatchBom::analyze` → 结果框 + 日志；
- **P8**：变量表对话框（表格 + 定义/设值/驱动/全部清空；facade 会话级 API）；
- **P9**：`groupByLayer/groupByFrame` + `writeSplitDrawings`（原子写子档）；
  `packShelf` 落位报告（Phase-1 不改模型）；
  `batchSign` 字段表单（12 栏目预填当前值）→ `beginEdit` + `updateElementQuiet` 写回。

统一插入 `insertGeneratedElements` = `beginEdit(label)` + `addElementsQuiet(parts)`——
整组一次 Ctrl+Z 撤回，与工程其他批量写入路径同语义。

---

## 4. 关键实现要点

1. **中文比较铁律**：`tb:field:` 前缀、图框层名等一律 `QStringLiteral`（UTF-16 字面量），
   禁用 `QLatin1String(utf8)`（拉丁一逐字节解码，与 QString 永不匹配且静默失效）。
2. **锚点语义**：选中非空 → 选中集整体包围盒中心（`geo::elementBounds` + `QRectF::united`）；
   无选中 → 视图中心（`view_->mapToScene(viewport()->rect().center())`）。
3. **P2 球标不重造**：直接走 `interp_->execute("xh")`，菜单与命令行/命令面板三处同源。
4. **P9 排图 Phase-1 只报告**：`packShelf` 为纯计算，落位结果经信息框 + 日志呈现，
   不改动模型图元（应用平移涉及「框内图元随框整体移动」语义，留待后续）。
5. **i18n 现状**：菜单顶层标题走 `tr()`；目录内 32 项标题/提示为源语言中文字面量
   （core 模块无 tr 上下文）——en 界面下这些项显示中文，记入已知限制（需 lupdate 扩表后补译）。
6. **准入刷新成本**：`evaluateFeature` 最重分支为 BatchSign 的全图 `tb:field:` 扫描；
   `aboutToShow` 必然触发但用户可感知为零；`selectionChanged` 触发为点击级（非逐帧），可接受。

---

## 5. 自测覆盖（t_featuremenu，95 断言）

| 组 | 覆盖 |
|---|---|
| ① 目录完整性 | 条目数 == FeatureIdCount(32)、id 唯一、`featureEntry` 反查一致、标题/分组/提示非空、9 个 P 分组、越界 id → nullptr |
| ② 空图纸 | 11 个条件类功能禁用且 **reason 全部非空**；齿轮/螺栓/批量BOM/变量表恒启用；空指针图纸同类规则 |
| ③ P1 分类型准入 | 直线→中心线/角度线启用、圆心标记禁用；2 直线→角点线启用；圆→圆心/孔/通孔/整圆启用；矩形→对称板启用 |
| ④ P2/P3/P6 选择集类 | 选中→启用；清空→禁用 + reason 非空 |
| ⑤ P2 明细表 | 无 bom → 禁用；addBomItem 后 → 启用 |
| ⑥ P7 闭合轮廓 | 单选圆启用；多选禁用；单选直线禁用；闭合多段线启用；开放多段线禁用 |
| ⑦ P9 | 空图拆分禁用；有图元拆分启用；无图框矩形排图禁用、有图框启用；无 tb:field 签名禁用、有则启用 |
| ⑧ P1 映射往返 | 10 个 FeatureId ↔ 10 种 CenterLineKind 一一对应；非 P1 条目回落 kCenter |
| ⑨ 闭合轮廓提取 | 圆→128 边形且顶点均在圆上；矩形→四角逆时针；直线→失败 + 谓词一致 |

---

## 6. 已知限制与偏差

1. **目录文案未国际化**：32 项标题/提示为中文源语言字面量（core 无 tr）；en 界面显示中文。
   后续：扩 .ts 或将目录下沉到 UI 层 tr 化。
2. **排图 Phase-1 只报告**：不移动模型图元（「框随内容整体平移」语义待定）。
3. **变量表对话框**为 Phase-1 形态：无单变量删除（facade 未暴露 remove，避免改动已冻结的 P8 模块），
   以「全部清空」替代。
4. **菜单无快捷键**：32 项均不占快捷键（避免与既有 100+ 命令冲突）；
   键盘用户可经命令面板（Ctrl+Shift+P）/ 命令行触达同类能力（P2 球标即 `xh`）。
5. **`aboutToShow` 为主刷新源**：模型内容变化（如插入标题栏后 BatchSign 放行）在菜单
   **下次展开时**生效——菜单关闭期间用户看不到置灰态变化，属可接受语义。

---

## 7. 验证记录

- 构建：`cmake --build build --target SketchCAD SketchSelfTest -j 1`（MinGW 13.1 / Qt 6.10.1），
  `-Wall -Wextra` 零警告；
- 自测：`SketchSelfTest` 全量 **2989 / 0**（较 P9 收官 2892 +97，即 t_featuremenu 增量）；
- 冒烟：`SketchCAD.exe` 启动存活 12s 无崩溃（buildMenu 构造期执行新菜单注册路径）；
- 注册表命令口径不受影响（本需求未新增/修改任何命令）。
