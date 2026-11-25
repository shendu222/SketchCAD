# GB 标准件原图模板库 —— 界面参数化出图（占位符替换方案）

- 日期：2026-11-24
- 适用工程：`sketch-cad-cpp`（Qt6 / C++17 / MinGW / Ninja）
- 源图库：`D:/01-CAD/02-avticationCAD/50405/stdcomp/CAD标准件图库`
- 模板库产物：`data/stdpart/templates/`（`index.json` + `tpl/<id>.tpl` + `build_report_N.json`）
- 相关文档：`GB标准件图库参数化-推理与实现.md`（程序化几何重建路线，与本方案并存）

---

## 1. 需求与方案选择

用户需求：把现有 5073 个可打开的 DWG 直接存入标准件库（文件/数据库形式），用占位符标记可变参数；界面输入参数替换占位符、实时渲染结果，并保存为图纸文件；解析过程处理全部文件并记录成功/失败及原因；占位符命名与提取方式统一；界面支持参数校验与预览。

### 1.1 关键发现：为什么不能只做"文字占位符"

全量解析统计（详见上篇文档）显示这批 DWG 是**打印出图用的纯二维几何**：

| 指标 | 数值 |
|---|---|
| Dimension 标注对象 | **0** |
| TEXT / MTEXT | 564（多在其他类别，抽样螺栓图 0 个） |
| Point / Line | 1,868,657 / 226,798 |
| 顶层包围盒 | 恒为 A3 图框 420×297 |

因此规格信息（d、L 等）**不在图纸文字里，而在文件名里**（`M5x25.dwg`）。方案必须是「**文字占位符 + 几何缩放参数**」双轨制，单纯替换文字无法让图形随参数变化。

### 1.2 双轨占位符设计

| 轨道 | 机制 | 作用 |
|---|---|---|
| 文字占位符 | DXF TEXT 内 `{{KEY}}` | 规格文字标注随参数替换 |
| 几何缩放参数 | 元数据 `baselineNominal`/`baselineLength` + 渲染时按比值缩放 | 直径 d 驱动 y 向与圆半径，长度 L 驱动 x 向 |

---

## 2. 占位符命名与提取规则（统一、单一数据源）

### 2.1 命名规则

- 语法：`{{KEY}}`（双花括号包裹，全大写 + 下划线）
- 标准键（顺序即 UI 表单顺序，`StdPartPlaceholders::keys()` 为唯一数据源）：

| 键 | 含义 | 单位 |
|---|---|---|
| `NOMINAL_D` | 公称直径 d | mm |
| `LENGTH_L` | 公称长度 L | mm |
| `PITCH_P` | 螺距 P | mm |
| `ACROSS_FLATS_S` | 对边宽 s | mm |
| `THICKNESS_T` | 厚度 k/m/h | mm |
| `SPEC_TEXT` | 组合规格文字（如 `M10x50`） | — |

- **不用** `%<...>%` 语法：那会被 DXF 导出层的 `resolveFields()` 消费掉；`{{...}}` 不会被任何现有机制解释，原样读入/写出（`CadFormats.cpp` 的 `firstStr()`/`dxfSafe()` 不处理花括号）。

### 2.2 提取规则（从文件名，正则固定）

| 文件模式 | 提取结果 | 示例 |
|---|---|---|
| `M<d>x<L>` | d=第一段, L=第二段, spec=`M<d>x<L>` | `M5x25.dwg` → d=5, L=25 |
| `M<d>` | d=<d>, L=0 | `M6.dwg` → d=6 |
| `<d> x <L>` | d=第一段, L=第二段 | `0.6 x 2.dwg` → d=0.6, L=2 |
| `<d>`（纯数字） | d=<d> | `10.dwg` → d=10 |

目录名提取 GB 标准号：`六角头螺栓 - GB 5782-86` → `GB 5782-86`。

### 2.3 标记过程

1. 每个 DWG 经 `cadfmt::fromDwg`（R12 原生；R13~R2018 经 `DwgBridge::dwgToDxf` 补丁版）解析为 `Element` 集合；
2. 遍历 TEXT/MText：把文字中与基准值相同的数字串替换为 `{{NOMINAL_D}}` / `{{LENGTH_L}}`；
3. **追加一条 `{{SPEC_TEXT}}` 规格标注**（长仿宋体，字高取国标 `standardTextHeightMm(A3, true)`）；
4. 回写为 DXF 文本 → `qCompress(level=9)` → `tpl/<id>.tpl`（抽样压缩后仅约 1.5 KB/件）；
5. 元数据写入 `index.json`：id、类别、标准、源文件、图元数、文字数、占位符数、成功/失败原因、基准 d/L/P/s/T、规格文字、图层表。

---

## 3. 解析结果记录（全部 5078 文件）

8 分片并行处理，`build_report_N.json` 逐文件记录成功/失败及原因：

| 指标 | 数值 |
|---|---|
| 总文件 | 5078 |
| 成功 | **5073** |
| 失败 | 5 |
| 类别 | 193 |
| 平均模板体积 | 约 1.5 KB（压缩后） |

5 个失败文件（均已排除入库，原因写入索引）：

| 文件 | 原因 |
|---|---|
| `免费资料下载/.../MXB_LXY.DWG` | ENTITIES 段里没有可识别的图元 |
| `六角盖形螺母 - GB 802-88/#M6.dwg` | 不是有效的 DWG 文件（头部无版本串） |
| `免费资料下载/.../EYEFROM.DWG` | 实体段指针无效（R12 源文件损坏） |
| `免费资料下载/.../PL_TIME.DWG` | ENTITIES 段里没有可识别的图元 |
| `免费资料下载/.../EYETO.DWG` | 不支持的 DWG 版本/实体段指针无效 |

> 注：构建器统一走 `cadfmt::fromDwg`（与 GUI/`DwgOpenVerify` 同源），因此 R12 也走原生解析路径，失败原因是文件本身损坏，而非转换器能力不足。

---

## 4. 界面与交互

新增停靠面板「标准件模板库」（`视图 > 面板` 可勾选显示）：

- **左：分类树**：193 类，展开后为各规格模板（显示 `M5x25` 等规格文字，失败项红色显示并在 tooltip 给出原因）；
- **右上：参数表单**：`QDoubleSpinBox` 输入 d / L / P / s / T，范围 0.01~1000 mm、2 位小数；规格文字框可编辑；参数 ≤ 0 或规格为空时显示校验提示且不更新预览；
- **右中：实时预览**：`QPainter` 自绘预览（Line/Circle/Arc/Polyline/Text，Y 轴翻转、自适应缩放），任一参数变化立即重渲染；
- **底部按钮**：
  - `插入当前图纸`：把预览图元平移到当前视图中心插入（`insertGeneratedElements`，整组一次可撤销）；
  - `保存为图纸文件`：支持 DXF / DWG / `.sketch`，DXF/DWG 走 `cadfmt::toDxf`/`toDwg`，`.sketch` 走 `MainWindow::writeDrawing`（临时替换活动模型后还原，不污染当前文档）；
  - `重置原规格`：恢复文件名的原始规格参数。

---

## 5. 关键实现文件

| 文件 | 作用 |
|---|---|
| `src/core/StdPartTemplate.h/.cpp` | 模板库核心：占位符键表、元数据结构、库加载、文字替换、几何缩放、索引写出、DWG→模板构建 |
| `src/ui/StdPartTemplatePanel.h/.cpp` | 面板：分类树、参数表单校验、QPainter 实时预览、插入/保存 |
| `src/ui/MainWindow.h/.cpp` | dock 注册、视图菜单、插入/保存槽 |
| `tools/stdpart/build_stdpart_templates.cpp` | 批量构建器（build 分片 / merge 合并） |
| `tests/SketchSelfTest.cpp` | `t_stdPartTemplate()`：11 条核心断言 |
| `data/stdpart/templates/` | 5073 个压缩模板 + index.json + 5 份构建报告 |

---

## 6. 使用方式

### 6.1 重建模板库

```bat
set PATH=C:/Qt/6.10.1/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;%PATH%
set SKETCHCAD_DWGTOOLS=D:/01-CAD/02-avticationCAD/thirdparty/libredwg-0.14/build-mingw
for /L %i in (0,1,7) do start /b StdPartTemplateBuilder.exe build "D:/.../CAD标准件图库" data/stdpart/templates %i 8
StdPartTemplateBuilder.exe merge data/stdpart/templates 8
```

### 6.2 界面出图

1. `视图 > 面板 > 标准件模板库` 打开面板；
2. 左侧选类别 → 选规格（如 `M5x25`）；
3. 右侧改参数（如 d=10、L=50），预览实时更新；
4. 点「插入当前图纸」把结果放入当前图纸，或「保存为图纸文件」导出 DXF/DWG/sketch。

---

## 7. 验证结果

| 项目 | 结果 |
|---|---|
| 批量解析 | 5078 文件处理完毕，5073 成功 / 5 失败（原因已记录） |
| 模板入库 | 5073 个 `.tpl` 压缩模板 |
| 参数化验证 | 自测 11 条断言通过，全部 **3660 / 失败 0** |
| 构建 | `BUILD_EXIT=0`，主程序/自测/工具全部链接成功 |
| GUI | 离屏启动无崩溃 |

---

## 8. 已知限制与后续建议

1. **几何缩放是比例近似**：d 驱动 y 向、L 驱动 x 向（单一 d 基准时 d 同时驱动两轴），适用于轴类/杆类标准件；对复杂结构的精确参数化仍需 `StdPartCatalog` 的程序化生成器路线。
2. **原始模板无尺寸标注对象**：规格标注为追加的 TEXT（`{{SPEC_TEXT}}`），不是 CAD DIMENSION 对象；如需符合国标的尺寸线/箭头，可在插入后调用标注命令补齐。
3. **模板库体积**：压缩后约 8 MB，已持久化到源码树 `data/stdpart/templates/`（5073 个 `.tpl` + `index.json` + 8 份 `build_report_N.json`）；面板按「exe 同级 `data/stdpart/templates` → 源码树相对路径 → `stdpart_tpl`」候选顺序自动定位，发布时把该目录放到 exe 同级即可。
4. **失败文件**：5 个为辅助工具文件或源损坏，未入模板库，原因可从 `build_report_N.json` 溯源。
