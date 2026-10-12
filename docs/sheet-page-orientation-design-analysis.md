# 新建图纸幅面驱动打印页面设置 —— 设计分析文档

> 任务：新建图纸时用户选择 A4 横向，但打印预览和导出 PDF 实际输出 A4 纵向。
> 要求定位页面尺寸与方向在「新建图纸 → 打印预览 / 导出 PDF」流程中的传递逻辑，
> 找出方向被重置或未正确传递的原因，确保预览与导出始终使用创建时选择的幅面与方向。
> 状态：✅ 已完成（构建零警告 + 全量自测通过，见 §7 验证回填）
> 日期：2026-10-12
> 关联文档：`layout-multiviewport-design-analysis.md`（布局与打印基座）、
> `ucs-sheet-center-design-analysis.md`（新建图纸原点居中的同一入口 applyNewDrawing）、
> `com-automation-design-analysis.md`（COM 批出图无头链路）

---

## 1. 边界与现状（任务前勘察结论）

### 1.1 根因：四处「默认 A4 纵向」发生点，幅面信息建完即失

`PrintSettings` 默认构造即 **`QPageSize(A4) + QPageLayout::Portrait`**（Layout.h:87-88）。
用户新建图纸时选的幅面（SheetSpec）只被用来生成图框几何，随后**没有任何机制**
把它同步进打印配置。全链勘察出四个独立发生点：

| # | 位置 | 行为（修复前） |
|---|---|---|
| ① | `MainWindow::currentPrintSettings_` | 仅来自默认构造 / AppSettings 上次退出快照 / 打印设置对话框，**从不与新建图纸的 SheetSpec 同步**；`applyNewDrawing` 建完图框即失 |
| ② | `PrintEngine::exportPdfLayouts` 零布局分支 | 另 `new` 临时 `PrintSettings`（同病第二实例），「导出 PDF（全部布局）」空布局表时回落单页恒 A4 纵向 |
| ③ | `BatchPlotEngine`（COM 批量出图） | `PrintSettings::defaults()`（第三实例），`settings` 为空时恒 A4 纵向 |
| ④ | `Commands.cpp` `layout new` / `mview` 自动建「布局1」 | `PrintSettings()`（第四实例），新布局默认页恒 A4 纵向 |

即：**幅面规格从未进入打印域**。用户选 A4 横向只影响画出来的图框几何（297×210 横放），
而打印域拿着自己的默认 A4 纵向——图纸内容与纸张方向脱钩。

### 1.2 为什么不给 PrintSettings 加「图纸幅面」字段

- `PrintSettings` 是**每布局独立**的打印偏好（边距/比例/样式/视口），随布局表存档；
  图纸幅面是**每图纸一个**的固有属性（与单位、图纸界限同级）。
- 若把幅面塞进 PrintSettings，一个「新建图纸」要同时改当前布局表和会话打印态两处，
  且多布局时「哪个布局说了算」成为新的歧义源。
- **决策**：幅面规格提升为 SketchModel 的随图属性（顶层 `"sheet"` 键），
  打印域在**消费时**经单一入口 `applySheetSpecToPage` 派生，数据源唯一。

### 1.3 旧档兼容边界

旧档（本程序历史版本所存）没有 `"sheet"` 键，但**图框几何是按创建时幅面画的**
（「图框」图层的纸张外边界矩形）。据此提供自愈：按图框矩形反推幅面
（`sheetFormatFromSize`，容差 1mm，横竖皆可命中）；**推断不出（无框图/非标尺寸/
外部导入）→ 放弃，打印设置保持原值，绝不猜**。

---

## 2. 关键设计决策

### 2.1 幅面规格随图存档（SketchModel 顶层 "sheet"）

与 units/limits 同级：新建/读档时设定，**不入撤销栈**（运行期无修改入口）。

```json
{ "sheet": { "format": 4, "landscape": true } }
```

- `format` = `SheetFormat` 枚举整数索引（A0=0..A5=5，只增不改的老约束）；
- `landscape` 缺省 true（工程图惯例；缺键 → 视为横向）；
- 无规格 → **不写键**（旧档字节级零回归）；
- 非法/越界 → 按缺键处理（`sheetSpecFromJson` 返 nullopt）。

### 2.2 单一入口 `applySheetSpecToPage`：只覆盖两个字段

```cpp
void applySheetSpecToPage(const SheetSpec& spec, PrintSettings& s) {
    if (spec.format 越界) return;                      // 非法幅面 → 保持原样（不猜）
    s.pageSize     = QPageSize(pageSizeIdForSheet(spec.format));
    s.orientation  = spec.landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
}
```

- **只动 `pageSize` / `orientation`**：用户手工调过的边距、比例、打印样式、
  fit-to-page、视口表一律不碰（打印设置对话框的显式覆盖永远优先，见 §3 调用时序）；
- 六幅面 → QPageSize id 映射收敛在 `pageSizeIdForSheet`（越界防御性回落 A4，
  枚举闭合时不可达）；
- 四个消费点全部改走它，**不再有第二处手工拼装**（单一事实源）。

### 2.3 消费点全景（六处，全部经单一入口派生）

| 消费点 | 文件 | 时机 |
|---|---|---|
| 新建图纸 | `MainWindow::applyNewDrawing` | `setSheetSpec(spec)` + 末尾 `syncPrintSettingsToSheet()` |
| 打开图纸（原生 JSON） | `MainWindow::readDrawingImpl` | `"sheet"` 键优先 → 缺键 `inferSheetSpecFromFrame()` 自愈 |
| 打开图纸（DXF/DWG 导入） | `MainWindow::readDrawingImpl` 两分支 | 同规则图框自愈（外部文件通常无标准「图框」层 → 推断不出 → 放弃） |
| 读档成功 / 多文档切换 | `MainWindow::readDrawing` / `switchToDoc` | `syncPrintSettingsToSheet()`（多文档各自独立跟随） |
| 零布局导出 PDF | `PrintEngine::modelSpacePageSettings` | `exportPdfLayouts` 空表回落分支专用 |
| COM 批出图 / 布局创建 | `BatchPlotEngine` / `Commands.cpp` | 无显式 settings 时 seeding；`layout new` / `mview` 自动布局默认页 |

### 2.4 显式覆盖优先（用户主权）

- 打印设置对话框 / 发布对话框 Accept 后写回 `currentPrintSettings_`——
  **此后以用户版式为准**，直到下次新建/打开/切文档才重新跟随图纸幅面；
- COM 批出图任务显式带 `settings` JSON 时，`fromJson` 优先，不跟随幅面；
- `layout copy` 复制源布局设置，天然继承源布局的页面（不重新 seeding）。

---

## 3. 实现落点

| 文件 | 改动 |
|---|---|
| `src/core/SheetSpec.h` | 声明 `std::optional<SheetFormat> sheetFormatFromSize(double w, double h)`（容差 1mm，竖横皆匹配，非标 → nullopt） |
| `src/core/sheet_frame.cpp` | 实现 `sheetFormatFromSize`（紧邻 kDims 表） |
| `src/core/Layout.h/.cpp` | 新增 `pageSizeIdForSheet`（六幅面映射）+ `applySheetSpecToPage`（单一入口，只写 pageSize/orientation） |
| `src/core/SketchModel.h/.cpp` | 幅面规格存取：`hasSheetSpec/sheetSpec/setSheetSpec/clearSheetSpec`；`inferSheetSpecFromFrame`（扫「图框」层 Rectangle 取最大面积 → sheetFormatFromSize → 方向由长短边定）；`sheetSpecToJson/FromJson`（缺键/越界 → nullopt）；`clear()` 重置；`toDrawingJson` 末尾写 `"sheet"` 键 |
| `src/ui/MainWindow.h/.cpp` | 新增 `syncPrintSettingsToSheet()`；调用点：`applyNewDrawing`（setSheetSpec + 末尾同步）、`readDrawingImpl`（JSON/DXF/DWG 三分支自愈）、`readDrawing` 成功块、`switchToDoc`（`model_ = d->model()` 后） |
| `src/ui/PrintEngine.h/.cpp` | 公开静态 `modelSpacePageSettings(model)`（零布局发布页面设置：视口清空 + fitToPage + 有幅面则经单一入口派生）；`exportPdfLayouts` 零布局分支改用它 |
| `src/com/BatchPlotEngine.cpp` | ① PDF 分支 `settings` 空且无布局 → `applySheetSpecToPage` seeding（显式 settings 优先）；② `loadDrawing` JSON 路径恢复 `"sheet"` 键（与 MainWindow 同语义，缺键图框自愈）；③ JSON 导出分支**收敛到 `SketchModel::toDrawingBytes()` 单一序列化入口**（见 §4 附条） |
| `src/core/Commands.cpp` | `layout new` 与 `mview` 自动建「布局1」：`PrintSettings()` → 按 `m->hasSheetSpec()` seeding（`layout copy` 保持不动） |
| `tests/SketchSelfTest.cpp` | 新增 `t_sheetPageSync()`（七组 47 CHECK）+ main 注册 |

### 3.1 时序要点：会话快照 vs 图纸幅面

`currentPrintSettings_` 在启动时从 AppSettings 恢复上次退出快照（:561）。
若用户上次在 A4 横向图纸工作、退出，本次启动打开一张 A3 纵向图纸：
`readDrawing` 成功 → `syncPrintSettingsToSheet()` 立即把页面改写为 A3 纵向，
**快照只充当「未打开任何图纸时」的初始值**，不会串图。

---

## 4. 取舍与边界

1. **推断容差 1mm**：`sheetFormatFromSize` 只认标准幅面尺寸（±1mm）。
   用户自画边框（非标）→ nullopt → 放弃。宁可回落默认，不把 210.5×297 猜成 A4。
2. **多矩形取最大面积**：图框层含纸张外边界与内图框两个矩形（buildSheetFrame），
   取最大者即纸张；同层小矩形（装订边示意等）不干扰。
3. **方向由长短边定**：横放矩形 → landscape；竖放 → portrait。
   与 `SheetSpec::make(f, w>h)` 同一语义。
4. **DXF/DWG 导入不猜**：外部文件的层名 rarely 是「图框」，推断通常自然落空；
   真带标准尺寸图框的（本程序导出再导入）则正确恢复方向。
5. **COM 无头读档补齐**：`BatchPlotEngine::loadDrawing` 此前漏恢复 `"sheet"`，
   导致「源图纸带 sheet 键 → 批出图仍默认 A4 纵向」的暗线，本轮同修。

### 附：自测逮到的第二序列化组装点（收敛单一入口实录）

首轮全量自测 2466/3：⑦ 组两条失败（`sheet 键随档恢复` / `COM 批量 PDF 横向`）。
根因不是读档侧，而是**写出侧**——`BatchPlotEngine::exportDrawing` 的 JSON 分支
**手工拼装**顶层根对象（app/formatVersion/elements/layers/…/params），
与 `SketchModel::toDrawingJson()` 是**两处各写一份**；本轮给模型加的 `"sheet"` 键
只进了单一入口，COM 侧自然漏写（顺带暴露既存暗坑：`properties` 键同样漏写，
COM 导出再导入会丢图纸特性表）。

**修复（收敛，不补齐）**：JSON 分支直接 `out = model.toDrawingBytes();`——
与 `MainWindow::writeDrawing` / AutoSave 快照同源。写入键集成为旧手写集的**超集**
（多 `properties`/`sheet`），读档侧缺键容缺、多键忽略，行为兼容。
grep 确认 `formatVersion` 在全 src 仅剩 `SketchModel.cpp:3683` 一处组装点。

> 教训与项目铁律再次互证：**同一语义两处手工拼装 ⇒ 一处更新一处漏；
> 根治=收敛单一入口**。本轮若只在 COM 侧补一行 `root["sheet"]=…`，测试会绿，
> 但第三处更新（如未来的 `drawingProps_` 同级属性）还会再漏一次。

---

## 5. 自测设计（t_sheetPageSync，七组 47 CHECK）

无头环境不链 MainWindow.cpp，端到端断言走公开静态
`PrintEngine::modelSpacePageSettings` + PDF `/MediaBox` 字节解析。

| 组 | 覆盖 | 关键断言 |
|---|---|---|
| ① 映射与字段卫生 | `pageSizeIdForSheet` 六幅面；`applySheetSpecToPage` | A4 横 → A4+Landscape；**只动 pageSize/orientation**（边距/比例/fitToPage/drawBorder 不变）；format 越界 → 保持原样 |
| ② 模型存取与存档 | set/get/clear；`sheetSpecToJson/FromJson` | 无规格不写 `"sheet"` 键；往返一致；空对象/越界 → nullopt；landscape 缺省 true；set 越界拒绝；clear 重置 |
| ③ 图框自愈 | `inferSheetSpecFromFrame` | A4 横/A5 竖旧档正确推断；空模型 → nullopt；非标矩形 → nullopt；同层小矩形不干扰 |
| ④ 零布局页面设置 | `modelSpacePageSettings` | 有幅面 → A4+Landscape 且视口空 + fitToPage；无幅面/nullptr → 旧行为 A4 纵向 |
| ⑤ 端到端 MediaBox | `exportPdfLayouts` 零布局 | A4 横模型 → `/MediaBox` 842×595pt（w>h）；A4 纵对照 → 595×842pt |
| ⑥ 命令层 seeding | `layout new` / `mview` | A4 横 → 新布局 A4+Landscape；A3 纵 → 自动「布局1」A3+Portrait；无幅面 → 旧默认 A4 纵向 |
| ⑦ COM 批出图 | `exportDrawing`(JSON) → `loadDrawing` → `runBatch` | sheet 键随档恢复；无显式 settings 的 PDF MediaBox 横向 |

既有回归护栏：`t_layout` ⑧⑨（`layout new`/`mview` 旧默认路径）、
`t_plotPublish` ⑧（GUI/COM 发布一致性）全部原样通过 → seeding 对无幅面图纸零影响。

---

## 6. 需要验证的场景（交付核对单）

| # | 场景 | 期望 |
|---|---|---|
| 1 | 新建 A4 **横向** → 打印预览 | 预览页面 A4 横向（内容横放居中） |
| 2 | 新建 A4 **横向** → 导出 PDF（当前布局/直接导出） | PDF `/MediaBox` 842×595pt 横向 |
| 3 | 新建 A4 **横向** → 导出 PDF（全部布局，**空布局表**） | 同上（零布局回落分支修复点） |
| 4 | 新建 A4 **纵向** → 预览/PDF | 595×842pt 纵向（反向对照） |
| 5 | 新建 A3/A2/A1/A0/A5 各幅面 → 预览/PDF | 页面尺寸与方向均与所选一致（六幅面映射） |
| 6 | 打开本程序旧档（无 `"sheet"` 键，带标准图框） | 图框自愈 → 预览/PDF 方向与原创建时一致 |
| 7 | 打开无框图/非标边框图纸 | 推断放弃 → 保持默认 A4 纵向（不猜、不崩） |
| 8 | 打开 DXF/DWG 后预览/PDF | 有标准尺寸「图框」→ 恢复方向；否则默认 |
| 9 | 多文档：A4 横 ↔ A3 纵来回切换标签 | 每次切换后预览/PDF 跟随当前活动图纸 |
| 10 | 打印设置对话框 | 打开即显示当前图纸幅面（横向）；用户可改，改后以用户版式为准 |
| 11 | `layout new L1` / `mview ...`（有幅面图纸） | 新布局默认页 = 图纸幅面（A4 横 → Landscape） |
| 12 | COM 批出图：任务不带 settings | PDF 跟随各源图纸幅面；带 settings → 显式版式优先 |
| 13 | 启动恢复：上次退出在 A4 横向图纸 | 未开图时打印态=快照；一开图即被图纸幅面覆盖（不串图） |

---

## 7. 验证回填

- 构建：`cmake --build build --target SketchCAD SketchSelfTest -j 1`，双目标 `-Wall -Wextra` 零警告；
- 全量自测：**2421 → 2469 / 0 失败，EXIT=0**（新增 `t_sheetPageSync` 47 CHECK，全量 0 失败）；
- 首轮回归实录：2466/3（COM 侧漏写 sheet 键 → §4 附条收敛单一入口后转绿）；
- 既有回归护栏：`t_layout` ⑧⑨、`t_plotPublish` ⑧（GUI/COM 发布一致性）原样通过；
- 交付物：本文档 + 代码改动（§3 表）。
