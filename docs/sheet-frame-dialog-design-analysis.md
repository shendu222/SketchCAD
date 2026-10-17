> **任务**：【图纸】菜单 + 【新建图框】对话框（需求 1~9：基本/加长/自定义幅面、横竖布置、双比例下拉、分区、装订线、对中符号、标题栏/明细栏；**扩展需求 10~13：附加栏 / 代号栏 / 参数栏 / 图框布置模板**）
> **状态**：✅ 编码完成，构建零警告 + 自测 `t_sheetFrameOpts` / `t_sheetFrameBars` 全绿 + `SketchSelfTest.exe` EXIT=0 闭环（通过 2566 / 失败 0）
> **日期**：2026-10-16（续做）
> **关联文档**：`sheet-page-orientation-design-analysis.md`（幅面驱动页面设置）；`ucs-sheet-center-design-analysis.md`（原点居中）；`title-block-fit-aux-filter-design-analysis.md`（标题栏表格化/辅助元素过滤）；`src/core/SheetSpec.h` / `src/core/sheet_frame.cpp` / `src/core/SheetFrameTemplate.{h,cpp}` / `src/ui/SheetFrameDialog.{h,cpp}` / `src/ui/TitleBlockForm.cpp` / `src/ui/MainWindow.{h,cpp}`

---

## ① 思考过程（Thinking）

### 1.1 需求拆解与设计映射

用户要求新增顶级菜单【图纸】，首个子项【新建图框】，覆盖 9 项功能需求。逐条映射如下：

| # | 需求 | 设计落点 | 国标依据 |
|---|---|---|---|
| 1 | 图幅 A0~A5 基本幅面 | `SheetFrameOptions::basicFormat` + 单选组 | GB/T 14689-1993 表 1 |
| 2 | 加长图幅（第二/第三选择，短边整数倍） | `extBase`/`extFactor` + `extendedSheetPresets()` 下拉 | GB/T 14689-1993 5.3/5.4 |
| 3 | 自定义图幅 | `sizeMode=Custom` + 宽高 spinbox | — |
| 4 | 横置/竖置 | `landscape` 单选 | GB/T 14689-1993 4.1 |
| 5 | 绘图比例（两个可编辑下拉框） | `cbScaleA_`/`cbScaleB_` 互相同步 | GB/T 14690 |
| 6 | 分区（长度/宽度方向，偶数 + 25~75mm） | `zoneCountX/Y` + `validateZones()` 实时校验 | GB/T 14689-1993 6.2 |
| 7 | 装订线开关 | `bindingMargin` | GB/T 14689-1993 4.2 |
| 8 | 对中符号开关 | `centerMarks` | GB/T 14689-1993 6.1 |
| 9 | 标题栏/明细栏（下拉+开关+预览） | `titleBlock`/`bomEnabled` + 预览按钮 | GB/T 10609.1 / GB/T 10609.2 |
| 10 | 附加栏（开关+下拉+预览，数量不限） | `additionalBarEnabled`/`additionalBarKey` + `additionalBarPresets()` 注册表；`appendGridBar` 堆叠于标题栏/明细栏上方 | 企业制图惯例（更改记录/描图/会签） |
| 11 | 代号栏（开关+列表框+预览，图框左上角；若标题栏含对应字段则自动关联） | `codeBarEnabled`/`codeBarKey` + `codeBarPresets()`；`appendCodeBar` 左上角周边带；字段名复用 `tb:field:drawingNumber` 经 `TitleBlockForm`「同名全写回」自动关联 | GB/T 10609.1 图样代号栏 |
| 12 | 参数栏（开关+列表框+预览，如直齿锥齿轮/链轮参数表，数量不限） | `paramBarEnabled`/`paramBarKey` + `paramBarPresets()`；`appendGridBar` 堆叠于附加栏上方 | 行业参数表惯例 |
| 13 | 图框布置编辑模板（保存「图幅+比例+标题栏+明细栏+三新栏」组合，选模板快速切换） | `SheetFrameTemplate` + `loadFrameTemplates`/`saveFrameTemplates`（独立 JSON `%LOCALAPPDATA%/SketchCAD/frame_templates.json`，缺文件回落内置 `sample`） | — |

### 1.2 关键设计决策

- **单一数据源**：对话框右侧预览与最终落盘共用 `buildSheetFrameWithOptions(opts)`，绝不手工拼装第二份，根除「预览与生成不一致」类 bug（与历史 `BatchPlotEngine` 收敛教训同源）。
- **零回归兼容**：旧 `buildSheetFrame(spec)` 重写为 `buildSheetFrameWithOptions` 默认选项的薄包装，既有 `t_titleBlockAux` 等断言不变。
- **加长幅面「大一号」规则**：加长幅面的图框周边 (c/e) 按「基本幅面大一号」判定（如 A2×3 按 A1 图框 → c=10）。这是 GB/T 14689 明确规则，由 `oneLargerFormat()` 实现。
- **分区偶数 + 每区 25~75mm**：`validateZones()` 实时显示每区毫米数并校验，但**不阻断确认**（仅提示），符合「辅助用户而非强制」的交互预期。
- **菜单接入零侵入**：新顶级菜单【图纸(&G)】（`&G` 经确认未被占用），首个子项【新建图框...】走 `openSheetFrameDialog` → `applySheetFrame`，与既有 `applyNewDrawing` 同构复用 `standardLayers`/`syncTitleBlockScale`/`syncPrintSettingsToSheet`/`fitToRect`。

### 1.3 扩展需求 10~13 的关键设计决策

- **三新栏统一走「注册表 + 开关 + 列表框 + 预览」四件套**：`additionalBarPresets()` / `codeBarPresets()` / `paramBarPresets()` 各返回 `QVector<...Preset>`（key 稳定标识 + label 展示 + 列/字段定义），下拉只列出系统定义项、**数量不限**；未知 key 永不猜（生成器查无则放弃绘制）。`bindBarToggle` 统一实现「开关关闭→禁用下拉+预览按钮」。
- **代号栏自动关联（需求 11 核心）**：代号栏字段元素名复用 `tb:field:drawingNumber`，与标题栏「图号」**同名**；`TitleBlockForm` 改为「同名全收集、全写回」（一个 key 可对应多个 element id，accept 时逐 id 写回），填图号即代号栏同步——零耦合、免维护映射表。
- **堆叠次序（自下而上）**：明细栏 → 附加栏 → 参数栏，代号栏独立置于图框左上角周边带；`appendGridBar` 为通用栏网格，越顶自动压缩行数（最少 1 行，仍放不下则放弃），绝不越出图框顶 `H - otherM`。
- **模板持久化刻意绕开 AppSettings specTable**：模板是整结构多值数据，逐键声明负担过重且 specTable 定位是「应用习惯」标量键；故落地独立 JSON（`frameTemplatesFilePath()`），文件缺失/损坏 → 回落内置 `sample`（附图示例状态），绝不崩溃。
- **⚠️ 原子写必须用 `QSaveFile` 而非手写 `QTemporaryFile + QFile::rename`**：MinGW/Windows 上 `QTemporaryFile` 默认 `autoRemove`（delete-on-close 语义）会阻止手动 `rename`，导致 `saveFrameTemplates` 静默返回 false（已用独立探针复现并验证；`AutoSave` 用普通 `QFile` 写临时文件故不受影响）。`saveFrameTemplates` 改用 `QSaveFile::commit()` 原子提交。
- **比例调整设置（附图右下组）**：三开关（标注/文字/线宽随图框比例调整）+ 两子选项（指定初始字高/初始比例）随选项持久化、预览链路同源；**已知限制**：完整标注/文字/线宽管线联动为后续项（当前仅持久化与预览联动，落盘尚未反算到具体图元字高/线宽）。

---

## ② 国标依据（核实）

- **GB/T 14689-1993 图框**
  - 基本幅面 A0~A5 短/长边系列（841×1189 … 148×210）。
  - 留装订边：a=25（装订边），c=10(A0~A2)/5(A3~A5)；不留装订边：四边 e=20(A0~A2)/10(A3~A5)。
  - 加长幅面：优先选用第二选择（短边×3、×4…）、其次第三选择；图框尺寸按「大一号」基本幅面。
  - 对中符号：四边中点粗短线（线宽 0.5mm），伸入图框内约 5mm。
  - 分区：分区数应为偶数，每分区 25~75mm；编号数字沿顶、字母沿左。
- **GB/T 10609.1 标题栏**：固定在图框右下角，含图名/图号/比例/材料/单位/设计/审核/批准等栏目。
- **GB/T 10609.2 明细栏**：置于标题栏正上方，表头序号/代号/名称/数量/材料/重量/备注。
- **GB/T 14690 比例**：优先系列（1:1,1:2,1:5,1:10…；2:1,5:1,10:1…）与允许系列（1:2.5,1:3,1:4,1:6…）。
- **GB/T 14665-2012 第 7 章**：投影法识别符号（第一角/第三角）。

---

## ③ 数据结构（`src/core/SheetSpec.h`）

```cpp
struct SheetFrameOptions {
    enum class SizeMode { Basic, Extended, Custom };
    SizeMode sizeMode = SizeMode::Basic;
    SheetFormat basicFormat = SheetFormat::A3;   // Basic
    bool        landscape   = true;              // Basic/Extended 横竖
    SheetFormat extBase  = SheetFormat::A3;      // Extended 基础格式
    int         extFactor = 3;                   // Extended 短边倍数
    double customW = 420.0, customH = 297.0;     // Custom 毫米
    bool bindingMargin = true;                    // 需求7
    bool centerMarks   = false;                  // 需求8
    bool zonesEnabled  = false;                  // 需求6
    int  zoneCountX = 4, zoneCountY = 2;
    TitleBlockStyle titleBlock = TitleBlockStyle::Standard; // 需求9
    enum class BomStyle { Standard };
    bool bomEnabled = false; BomStyle bomStyle = BomStyle::Standard; int bomRows = 4; // 需求9
    ProjectionMethod projection = ProjectionMethod::FirstAngle;

    // ---- 扩展选项（需求 10~12，只增不改；默认值保持旧行为完全不变）----
    double bindingWidth = 25.0;            // 装订宽度 a（附图「装订宽度 25」）
    bool   autoFillDate = false;           // 落档时把当天日期写入标题栏「设计日期」
    bool   specifyFramePos = false;        // 指定图框位置（预留开关，随模板持久化）
    bool   additionalBarEnabled = false;   // 需求10：附加栏
    QString additionalBarKey;               //   additionalBarPresets() 检索 key
    bool   codeBarEnabled = false;         // 需求11：代号栏（图框左上角）
    QString codeBarKey;                    //   codeBarPresets() 检索 key
    bool   paramBarEnabled = false;        // 需求12：参数栏
    QString paramBarKey;                   //   paramBarPresets() 检索 key
    bool   zoneShowLeft = true;            // 分区：是否显示左带字母（附图「是否显示左分区」）
    // 比例调整设置（附图右下组；随图框比例联动，完整管线联动为后续）
    bool   dimScaleAdjust       = true;   // 标注随图框比例调整
    bool   textScaleAdjust      = true;   // 文字随图框比例调整
    bool   textInitHeightEnabled = false;  // 指定初始字高（子开关）
    double textInitHeight       = 3.5;     // 指定初始字高值 (mm)
    bool   lineWidthScaleAdjust = true;    // 线宽随图框比例调整
    bool   lineWidthInitEnabled = false;   // 指定初始比例（子开关）
    double lineWidthInitRatio   = 0.3;     // 指定初始比例值

    double widthMm()  const;   // 最终水平尺寸
    double heightMm() const;   // 最终垂直尺寸
    SheetFormat frameMarginFormat() const;  // 周边(c/e)判定用基本幅面（加长=大一号）
    QString sizeLabel() const;              // 如 "A3×3 (420×891)"
    SheetSpec toSheetSpec() const;          // 供 MainWindow 落档/设幅面
};

struct ExtendedSheetPreset { QString label; SheetFormat base; int factor; double longSide; double shortSide; };
const QVector<ExtendedSheetPreset>& extendedSheetPresets();   // 19 条（第二+第三选择）

// 三新栏系统预设（注册表，只增不改，key 稳定；数量不限）
struct AdditionalBarPreset { QString key; QString label; QStringList columns; int rows = 2; };
struct CodeBarPreset      { QString key; QString label; QString caption; QString fieldKey; };
struct ParamBarPreset     { QString key; QString label; QStringList columns; int rows = 3; };
const QVector<AdditionalBarPreset>& additionalBarPresets();   // revision/tracing/notation
const QVector<CodeBarPreset>&       codeBarPresets();        // Tydh/Tth/Tdw（均 fieldKey=drawingNumber）
const QVector<ParamBarPreset>&      paramBarPresets();       // bevelGear/sprocket/cylGear/wormGear/spring/material

std::vector<Element> buildSheetFrameWithOptions(const SheetFrameOptions& opts);  // 纯函数
```

**只增不改原则**：`SheetFrameOptions` 为纯数据聚合，新增字段不影响既有序列化/枚举；`buildSheetFrame(spec)` 行为完全向后兼容。三新栏预设以「注册表 + 稳定 key」组织，新增栏只需在对应 `xxxPresets()` 追加一条，下拉/生成/模板序列化自动覆盖。

### ③-续 模板结构（`src/core/SheetFrameTemplate.{h,cpp}`）

```cpp
// 需求 13：把「图幅 + 绘图比例 + 标题栏 + 明细栏 + 三新栏 + 比例调整」整组组合存为命名模板
struct SheetFrameTemplate {
    QString name;                 // 模板名（唯一键）
    SheetFrameOptions opts;       // 全套图框布置选项
    double scaleNum = 1.0;        // 绘图比例前值（a:b 的 a）
    double scaleDen = 1.0;        // 绘图比例后值（a:b 的 b）
    double scaleRatio() const { return scaleDen > 0.0 ? scaleNum / scaleDen : 1.0; }
};
QString frameTemplatesFilePath();                                 // %LOCALAPPDATA%/SketchCAD/frame_templates.json
QVector<SheetFrameTemplate> loadFrameTemplates(const QString& path = QString());  // 缺文件/损坏→内置 sample
bool saveFrameTemplates(const QVector<SheetFrameTemplate>&, const QString& path = QString()); // QSaveFile 原子写
```

JSON 序列化 `optsToJson`/`optsFromJson` **逐键显式**，缺键回落默认（前向兼容）。内置 `sample` 模板 = 附图示例状态（A2 横置 1:1 / 分区 6×8 / 对中符号 / 装订宽 25 / 标题栏+明细栏+附加栏(revision)+代号栏(Tydh)启用 / 参数栏关 / 比例调整三开关开、两子选项关）。

---

## ④ 生成链（`src/core/sheet_frame.cpp`）

主函数 `buildSheetFrameWithOptions(opts)` 流程：

1. 由 `widthMm()/heightMm()` 取 W/H；由 `frameMarginFormat()` 判定 `tenClass`(A0~A2) → c/e。
2. 纸张外边界（`图框`层，lineWidth=1.0 细实线）+ 图框内边界（`图框`层，lineWidth=2.0 粗实线）。
3. 对中符号（可选）→ `appendCenterMarks`（4 条边中点短线）。
4. 计算标题栏几何（始终计算，供明细栏/投影符号定位）。
5. 明细栏（可选）→ `appendBom`（标题栏正上方，越顶自动压缩行数）。
6. 标题栏（Standard 时）→ `appendStandardTitleBlock`（与旧实现逐像素一致，复用 `fitCellText`）。
7. **附加栏（需求 10，可选）→ `appendGridBar`（标题栏/明细栏上方堆叠，越顶压缩）**；未知 key 查无则放弃。
8. **参数栏（需求 12，可选）→ `appendGridBar`（附加栏上方继续堆叠）**；与附加栏共用同一通用栏网格 helper。
9. **代号栏（需求 11，可选）→ `appendCodeBar`（图框左上角上周边带内，独立定位，不参入堆叠）**；字段名 `"tb:field:"+p.fieldKey`，与标题栏同名。
10. 分区（可选）→ `appendZones`（周边带网格 + 顶数字/左字母编号；左带受 `zoneShowLeft` 控制）。
11. 投影法识别符号 → `buildProjectionSymbol`。

> 堆叠基准 `stackTop` 从标题栏顶 `by1` 起，依次 `明细栏 → 附加栏 → 参数栏` 自下而上累加；各栏 `appendGridBar` 返回新顶部 y，越顶（`> H - otherM`）时压缩行数（最少 1 行），仍放不下则放弃（返回 `bottom` 不变），**绝不越出图框顶**。代号栏独立置于左上角周边带（见上第 9 步）。

各叠加层为独立 helper，互不干扰；开启/关闭任一选项只增减对应图元，不影响其他层。

**加长幅面尺寸派生**：`extendedSheetPresets()` 中 `longSide = 基础短边 × 倍数`、`shortSide = 基础长边`（如 A3×3：420×3=891 长边，420 短边）。`sizeMode=Extended` 时 `widthMm()/heightMm()` 按 landscape 取 l/s。

---

## ⑤ 对话框 UI（`src/ui/SheetFrameDialog.{h,cpp}`）

- **左栏（滚动区）** 依次：模板下拉（需求 13：选择即整组应用；旁「保存为模板…」「管理模板…」）、图幅单选组（A0~A5 + 自定义宽高）、加长幅面下拉（首项「不使用加长」-1，其余 19 条预设 key=`base*100+factor`）、横/竖单选、双比例可编辑下拉（`kScales` GB/T 14690 系列，前值:后值，`currentTextChanged` 互相同步解析 `parseScaleRatio`；含「计算(C)」化简比例子对话框）、分区 GroupBox（可勾选开启，X/Y SpinBox 步长 2，`validateZones()` 实时显示每区毫米数 + 偶数/25~75mm 校验；含「是否显示左分区」子复选 `zoneShowLeft`）、装订线 CheckBox + 装订宽度 spinbox、对中符号 CheckBox、标题栏/明细栏（下拉 + 开关 + 预览按钮）。
- **右栏（实时预览）** 由上而下依次：标题栏预览 → 明细栏预览 → 附加栏（需求 10：开关 + 下拉列表框 + 预览按钮；关闭则禁用下拉与按钮）→ 代号栏（需求 11：开关 + 列表框 + 预览按钮；关闭禁用；置于图框左上角）→ 参数栏（需求 12：开关 + 列表框 + 预览按钮；关闭禁用）→ 比例调整设置组（3 开关 + 2 子选项，附图右下组）。各栏均为「开关 + 列表/下拉 + 预览按钮」统一形态，`bindBarToggle(chk, combo, btn)` 实现「开关关闭→禁用下拉/列表与预览按钮」。
- **预览**：`previewLabel_`（QFrame::StyledPanel 容器），`updatePreview()` 由 `buildSheetFrameWithOptions(opts_)` 生成图元经 `renderPreview()` 绘制 QPixmap（Y 轴翻转、世界坐标→屏幕坐标），与落盘同源。预览按钮显式刷新对应栏图形（隔离单栏独立预览窗 `showBarPreview`）。

---

## ⑥ 菜单接入（`src/ui/MainWindow.{h,cpp}`）

```cpp
// buildMenu()：图纸(&G) 顶级菜单
QMenu* sh = new QMenu(tr("图纸(&G)"), this);
sh->setObjectName("menuSheet");
mk(sh, tr("新建图框..."), QKeySequence(), [this]{ openSheetFrameDialog(); })
    ->setIcon(icons::make("titleblock"));
bar->addMenu(sh);

// openSheetFrameDialog：弹窗 → 新文档
SheetFrameDialog dlg(this);
if (dlg.exec() != QDialog::Accepted) return;
const SheetFrameOptions opts = dlg.result();
const double ratio = dlg.selectedScaleRatio();
DocSession* s = docs_->addNewDoc(); makeDocPage(s);
docTabs_->addTab(docTabText(docs_->count()-1)); switchToDoc(docs_->count()-1);
applySheetFrame(opts, ratio); s->markClean(); syncTabTitle(docs_->activeIndex());

// applySheetFrame：纯落盘封装（与 applyNewDrawing 同构）
model_->clear(); model_->setDrawingPath(QString());
model_->setScaleRatio(scaleRatio, false);
for (const Layer& l : standardLayers()) model_->addLayer(l.name,l.color,l.lineWidth,l.lineStyle);
model_->beginEdit(QStringLiteral("新建图框"));
model_->addElementsQuiet(buildSheetFrameWithOptions(opts));
model_->setSheetSpec(spec); syncTitleBlockScale();
model_->setCurrentUcs(Ucs(QStringLiteral("SHEET_CENTER"), Vec3(W/2,H/2,0), 0));
syncPrintSettingsToSheet(); view_->fitToRect(QRectF(0,0,W,H));
```

---

## ⑦ 自测（`tests/SketchSelfTest.cpp` → `t_sheetFrameOpts`）

新增 `t_sheetFrameOpts()`，9 组 CHECK 覆盖核心逻辑：

1. 默认选项 == 旧 `buildSheetFrame(A3横向)` 图元数一致 + 含 `tb:field:drawingName`（向后兼容）。
2. 留装订边 A3 图框左边界 = 25（`图框`层、Rectangle、lineWidth=2 的最小 x）。
3. 不留装订边 A3 边界 = 10（e）。
4. A3×3 横放 891×420；图框按大一号(A2) → 左边界=25；`sizeLabel` 含 "A3×3"。
5. 自定义 500×400；按最长边定档(A3) → 左边界=25。
6. 对中符号恰好 +4 图元。
7. 分区 4×2 新增 ≥10 图元且出现编号 "1" 与字母 "A"。
8. 明细栏新增图元且含表头 "序号"。
9. 无标题栏样式不含 `tb:field:drawingName`。

注册：`CHECK(t_sheetFrameOpts(), "【新建图框】图框生成选项(兼容/装订/加长/自定义/分区/对中/明细栏)")`。

### ⑦-续 扩展自测（`t_sheetFrameBars`，需求 10~13）

新增 `t_sheetFrameBars()`，覆盖三新栏/代号栏同名关联/参数栏/左分区开关/装订宽度/预设注册表/模板 JSON 往返，共 31 组 CHECK：

1. 默认选项仅标题栏 1 处图号字段、无附加栏/代号栏墨迹（向后兼容）。
2. 附加栏(revision)表头「更改标记」绘制；栏顶不越过图框顶（只统计「标题栏」层，纸张外框矩形在「图框」层 y=H，不计入）；未知 key 回落默认（绝不猜）。
3. 参数栏(直齿锥齿轮)表头「模数 m」绘制。
4. 代号栏与标题栏图号共用名（2 处 `tb:field:drawingNumber` → 同名全写回关联）；代号栏标签「图样代号」绘制于「图框」层。
5. 分区左带开关：显示左分区时左带字母 `A` 绘制、关闭后消失（顶带不受影响）。
6. 装订宽度 25（旧行为）↔ 30 生效于图框左边界。
7. 预设注册表：三栏 key 唯一、成员齐全；参数栏含「直齿锥齿轮参数表」。
8. 模板持久化：缺文件回落内置 `sample`；两模板（含三新栏 + 比例 + 加长幅面）`saveFrameTemplates` → `loadFrameTemplates` 数量/名称/三新栏/装订宽度/日期/初始字高/比例前值后值/加长幅面全部保真。

注册：`CHECK(t_sheetFrameBars(), "新建图框扩展(附加栏/代号栏同名关联/参数栏/左分区开关/装订宽度/预设注册表/模板JSON往返)")`。

> ⚠️ **修复记录（自测 7 失败 → 0）**：首轮交付 `t_sheetFrameBars` 现 7 组失败，根因两类——(a) 附加栏越顶断言误把纸张外框矩形（y=H）计入 `maxY`，已改为只统计「标题栏」层；(b) `saveFrameTemplates` 原用 `QTemporaryFile + QFile::rename`，MinGW/Windows 上 `QTemporaryFile` 默认 `autoRemove`（delete-on-close）阻止手动 rename，导致保存静默返回 false，已改用 `QSaveFile::commit()` 原子写（独立探针复现并验证；`AutoSave` 用普通 `QFile` 故不受影响）。

---

## ⑧ 验证结果

- **构建**：`cmake --build build --target SketchCAD SketchSelfTest -j1`，增量构建（`SheetFrameTemplate.cpp` + `SketchSelfTest.cpp` 两文件变更）**0 警告 / 0 错误**，`BUILD_EXIT=0`，双目标 `SketchCAD.exe` / `SketchSelfTest.exe` 链接成功。
- **自测**：`SketchSelfTest.exe --selftest` → **通过 2566 / 失败 0**（较首轮 2559/7：`t_sheetFrameBars` 31 组全绿），整体 **EXIT=0**。
- **已知限制**：
  - 比例调整设置（附图右下组）当前仅持久化 + 预览链路同源，**完整标注/文字/线宽管线联动未实现**（落盘尚未反算到具体图元字高/线宽），列为后续项。
  - `appendCenterMarks` 当前从纸边界画至图框边（全长），严格 GB/T 14689 6.1 应为「伸入图框内约 5mm」短线；属视觉保真优化，不影响功能与自测。
  - 分区偶数强制当前仅提示不阻断（与「辅助用户而非强制」交互预期一致）。

---

## ⑨ 小结

【图纸】>【新建图框】以「纯数据选项 + 单一生成函数 + 同源预览」三件套落地需求 1~13，国标规则（加长大一号、装订/不留装订边、分区偶数+25~75mm、标题栏/明细栏位置）均经核实并实现；三新栏走「注册表 + 开关 + 列表框 + 预览」统一四件套、代号栏借「同名全写回」零耦合自动关联标题栏图号、模板以独立 JSON 持久化（缺文件回落内置 `sample`）；菜单接入零侵入、向后兼容零回归。自测共 40 组断言（`t_sheetFrameOpts` 9 + `t_sheetFrameBars` 31）覆盖关键不变量，全量 2566/0 通过。后续精炼项：比例调整设置完整管线联动、对中符号线段长度（5mm）、分区偶数强制。
