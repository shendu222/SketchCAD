# GB 标准件 DWG 图库批量解析与参数化标准件库 —— 推理过程与实现文档

- 日期：2026-11-24
- 适用工程：`sketch-cad-cpp`（Qt6 / C++17 / MinGW / Ninja）
- 源图库：`D:/01-CAD/02-avticationCAD/50405/stdcomp/CAD标准件图库`
- 产物数据：`data/stdpart/stdpart_index.json`（解析索引）、`data/stdpart/stdpart_catalog.json`（参数化目录）

---

## 1. 任务与目标

将源图库中全部标准件 DWG 图纸在 SketchCAD 中批量打开，逐类解析图元结构、图层组织与标注信息；据此整理图形内容补充进国标库，形成分类清晰、可复用的标准件库；结合对应国家标准为每类提取关键几何与规格数据作为可变参数（公称尺寸、长度、直径、螺距等），建立参数与图形的驱动关系，使参数修改后图形自动更新；为新生成图形补全并标注对应尺寸，确保符合国标标注要求；最终导出完整推理过程与处理逻辑为 md 文档。

---

## 2. 推理过程（关键发现与决策链）

### 2.1 图库盘点

- 目录规模：**193 个子目录、5078 个 DWG 文件**（按扩展名统计；目录名含标准号，如「六角头螺栓 - GB 5782-86」，文件名编码规格，如 `M5x25.dwg`、`10.dwg`、`0.6 x 2.dwg`）。
- 文件规模示例：单个 DWG 约 96 KB~数百 KB，解析后顶层图元约 216 个（Line 72 / Point 138 / Circle 1 / Arc 5）。
- 决策：**逐文件人工建模不可行**，必须构建自动化批量解析 → 结构化索引 → 数据驱动参数化库的管线。

### 2.2 现有能力盘点（复用优先）

- DWG 读取链路已存在且可用：`cadfmt::fromDwg()` 对 R12 走原生解析，对 R13~R2018 经 `DwgBridge` 调 `dwg2dxf`（libredwg 0.14，已打 STDPART2D 补丁）转 DXF 后回读；`bench/DwgOpenVerify` 为同源参考实现。抽样 4 类均成功解析（每类 216~401 图元）。
- 参数化零件库已存在：`PartLibrary` 单例 + `PartGenerator` 接口，内建 `gear/bolt/nut/washer` 生成器；`BoltGenerator` 已实现 GB/T 5782 六角头螺栓完整标注图（68 图元、真实数值标注）。`GbFastenerTable` 提供六角螺栓/螺母/平垫圈规格表。
- 尺寸样式：上轮已完成长仿宋体统一、字号降档、箭头缩小（A2~A5 字高 2.5 mm / 箭头 2.5×0.8），`dim::Style` 单一数据源贯穿屏幕、打印、DXF 导出。
- 结论：**不重复造轮子**——新建工具只负责「批量解析 + 数据驱动目录 + 参数驱动出图」，几何生成复用 `PartLibrary`，尺寸样式复用 `dim::Style`。

### 2.3 批量解析的关键发现（决定了参数化策略）

全量解析 8 分片并行完成，结果：

| 指标 | 数值 | 含义 |
|---|---|---|
| 总文件 | 5078 | 全部打开 |
| 成功 | 5073 | 99.9% |
| 失败 | 5 | 4 个「免费资料下载」非标准件辅助文件 + 1 个头部损坏（`#M6.dwg`） |
| 类别 | 193 | 均含 GB 标准号 |
| Point 图元 | 1,868,657 | 绝大多数是图框/标题栏点阵 |
| Line 图元 | 226,798 | 含图框 + 零件轮廓 |
| Dimension 图元 | **0** | 原始图纸**没有真正的 CAD 尺寸对象** |
| Text/MText | 564 | 标注已「爆炸」为文字 |
| BlockDef | 5375 | 多为图框印记块（块内仅 1 条线） |
| 主图层 | `0`(4888)、`AM_0N`(4825)、`CEN`、`DIM` | 图层组织以 `0`/`AM_0N` 为主 |

**核心推理链**：

1. 原始 DWG 的标注已是**爆炸文字**（Dimension 数 = 0），无法直接从 DWG 继承「参数—标注」关联；因此**不能**通过简单读取 DWG 得到可驱动的参数化模型。
2. 图元以 Point/Line 为主且顶层包围盒恒为 A3 图框（420×297），说明这些 DWG 是「打印出图用的二维图形」，不是面向参数化的特征模型。
3. 唯一稳定、可结构化的规格信息在**文件名**（`M5x25`、`0.6 x 2`、`10`）与**目录名**（GB 标准号、品名）。
4. 因此参数化策略确定为：**「DWG 解析做分类与统计取证 → 文件名/目录名提取规格与标准 → 用程序化生成器重建几何 → 自动补全国标尺寸标注」**，而非「从 DWG 反向工程参数化特征树」。

### 2.4 参数化建模决策

- 每类标准件抽象出 5 个通用可变参数：`nominal`(d 公称直径)、`length`(L 长度)、`pitch`(P 螺距)、`acrossFlats`(s 对边)、`thickness`(k/m/h 厚度)。
- 生成器类型按品名映射：`螺栓/螺钉 → hexbolt`、`螺母 → nut`、`垫圈 → washer`、`销 → pin`、`含六角 → hexprism`，未知类型回落为通用圆柱并给出 note。
- 规格点从文件名正则提取：`M(d)x(L)` / `M(d)` / `d x L` / `d`，共提取 **4792 个规格点**，写入目录 JSON 的 `specs` 字段。
- 参数修改后重新调用 `StdPartCatalog::generate()` 即产出更新后的几何与标注，实现参数驱动。

### 2.5 标注补全决策

- 新生成图形的尺寸标注由 `makeLinearDim()` / `makeDiameterDim()` 创建：
  - `ElementFactory::create(Dimension)` 自动写入 `LongFangSong` 样式名；
  - `dimValue = geo::measureDimension(e)` 由几何实测，保证数值与图形一致；
  - 屏幕/打印/DXF 的字号（2.5 mm）、箭头（2.5/0.8）、字体（长仿宋体）全部沿用上轮完成的 `dim::Style` 与 `standardDimTextHeightMm()` 单一数据源；
  - 标注位置用 `textGap + textHeight/2` 公式，未改变既有位置/对齐逻辑。

---

## 3. 实现结构

### 3.1 新增文件

| 文件 | 作用 |
|---|---|
| `tools/stdpart/analyze_stdpart_dwg.cpp` | 批量 DWG 解析器：扫描目录、走同源 `fromDwg` 链、统计图元/图层/标注/文字/块、从目录名与文件名提取标准号与规格、输出 JSON；支持 8 分片并行。 |
| `tools/stdpart/build_stdpart_catalog.cpp` | 目录构建器：`merge`（合并分片）/`build`（生成参数化目录）/`verify`（逐类参数驱动验证）。 |
| `src/core/StdPartCatalog.h/.cpp` | 参数化目录模块：加载目录 JSON、由解析索引构建目录、5 参数校验与默认、按生成器类型出图、自动补全尺寸标注。 |
| `data/stdpart/stdpart_index.json` | 全量解析索引（5078 文件明细 + 193 类汇总）。 |
| `data/stdpart/stdpart_catalog.json` | 参数化目录（193 类 + 4792 规格点 + 每类解析统计）。 |
| `docs/GB标准件图库参数化-推理与实现.md` | 本文档。 |

### 3.2 CMake 变更

- `CORE_SOURCES` 增加 `src/core/StdPartCatalog.cpp`。
- 新增 `StdPartDwgAnalyzer`、`StdPartCatalogBuilder` 两个可执行目标（链接与 `DwgOpenVerify` 一致）。

### 3.3 数据流

```
CAD标准件图库 (5078 DWG)
   │  8 分片并行 × cadfmt::fromDwg（DwgBridge→dwg2dxf 补丁版）
   ▼
stdpart_shard_0..7.json  ──merge──▶  stdpart_index.json
   │                                  （图元/图层/文字/块统计 + 规格 token）
   ▼ buildFromAnalysis()
stdpart_catalog.json  ──load──▶  StdPartCatalog
   │                              │ generate(id, params, withDims=true)
   │                              ▼
   │                        PartLibrary 几何生成（bolt/nut/washer 复用）
   │                        + pin/hexprism 新增生成器
   │                        + makeLinearDim/makeDiameterDim 自动标注
   ▼
参数修改后的图形 + 国标尺寸标注（长仿宋体 / 2.5mm 字高 / 2.5×0.8 箭头）
```

---

## 4. 验证结果

| 项目 | 结果 |
|---|---|
| 批量解析 | 5078 文件全部打开，5073 成功（99.9%），5 个非标准件/损坏文件失败 |
| 类别覆盖 | 193 类全部进入参数化目录 |
| 规格提取 | 4792 个规格点 |
| 参数驱动验证 | **193/193 类生成成功**（`StdPartCatalogBuilder verify`） |
| 自动标注 | 每类默认参数均产出 Dimension 图元，数值由 `geo::measureDimension` 实测 |
| 尺寸样式 | 沿用统一后的长仿宋体 / 2.5 mm / 2.5×0.8 箭头（屏幕、打印、DXF 一致） |

### 失败文件说明（均为非标准件或源文件问题）

- `免费资料下载/模具设计助手/NTools4/{MXB_LXY,EYEFROM,PL_TIME,EYETO}.DWG`：辅助工具文件，非标准件图纸，图元段为空或指针无效。
- `六角盖形螺母 - GB 802-88/#M6.dwg`：文件头异常（前 3 字节 `999`），非有效 DWG。

---

## 5. 使用方式

### 5.1 重新批量解析（如需更新图库）

```bat
set PATH=C:/Qt/6.10.1/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;%PATH%
set SKETCHCAD_DWGTOOLS=D:/01-CAD/02-avticationCAD/thirdparty/libredwg-0.14/build-mingw
REM 8 分片并行（每片输出到 stdpart_shard_<i>.json）
for /L %i in (0,1,7) do start /b StdPartDwgAnalyzer.exe "D:/01-CAD/02-avticationCAD/50405/stdcomp/CAD标准件图库" stdpart_shard_%i.json 0 %i 8
StdPartCatalogBuilder.exe merge stdpart_index.json stdpart_shard_0.json ... stdpart_shard_7.json
StdPartCatalogBuilder.exe build stdpart_index.json stdpart_catalog.json
StdPartCatalogBuilder.exe verify stdpart_catalog.json
```

### 5.2 参数化出图（程序内）

```cpp
#include "core/StdPartCatalog.h"
using namespace SketchCAD;
StdPartCatalog::instance().load("data/stdpart/stdpart_catalog.json");
QVariantMap p;
p["nominal"] = 10.0;      // 公称直径 d
p["length"]  = 50.0;      // 长度 L
p["pitch"]   = 1.5;       // 螺距 P
std::vector<Element> out;
StdPartCatalog::instance().generate("gb5782_hexbolt_xxxx", p, out, nullptr, true);
```

修改 `p` 中任意参数后重新调用 `generate()`，几何与标注自动更新。

---

## 6. 已知限制与后续建议

1. **原始 DWG 标注已爆炸**：无法从源文件继承真实尺寸对象，尺寸标注由程序按 GB 标注样式重新生成（数值实测），并非从原图迁移。
2. **生成器覆盖面**：当前对 hexbolt/nut/washer 复用 `PartLibrary` 的精确国标生成器，pin/hexprism 为新增简化生成器，其他类别先用通用圆柱回落并给出 note。如需逐类精确建模，可按 `StdPartCategory.generator` 扩展具体生成器。
3. **规格数值校核**：`GbFastenerTable` 的规格值标注为「人工给定(G3)」，新增类别未引入未校核数值，所有标注均为几何实测值，不引入额外标准数据风险。
4. **失败文件**：5 个文件为辅助工具或源损坏，已从目录中排除（不进入规格点统计）。
5. **后续可接入 GUI**：在零件库面板增加「GB 标准件」标签页，读取 `stdpart_catalog.json` 展示分类/规格，用户选规格后调用 `generate()` 插入模型。
