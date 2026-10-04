# P0-01：文件格式升级 R12 + R2000（DXF 双版本读写 + 版本协商 + 二进制容器）

> 需求：把文件格式从「仅 R12 (AC1009)」升级为「R12 + R2000(AC1015) 读写」，让打不开的版本有清晰降级路径，使 SketchCAD 能打开客户主流 DXF 并导出对方可接受的版本，同时保持旧档逐字节兼容。

## 1. 结论速览

| 能力 | 状态 | 说明 |
|---|---|---|
| R12 DXF (AC1009) 读写 | ✅ 保持 | `opts==nullptr` 输出与升级前**逐字节一致** |
| R2000 DXF (AC1015) 读 | ✅ | 文本 + 二进制，共享解析器，忽略 100/5/330 标记 |
| R2000 DXF (AC1015) 写 | ✅ | 在 R12 token 流上注入 AcDb/句柄/owner + OBJECTS 段 |
| 二进制 DXF 容器 | ✅ | 22 字节哨兵头 + token 序列化（R12/R2000 共用） |
| 版本协商（拒绝不支持版本） | ✅ | 探测 `$ACADVER`/`AC1021+`，返回 false + 明确中文提示，不崩溃 |
| R12 DWG (AC1009) 原生二进制 | ✅ | 既有实现，独立路径 |
| R2000 DWG 二进制 | ❌ 不实现 | 第三方内核 GPL/商业授权受限；R2000 仅以 DXF 交付（见 §3） |
| 第三方几何/解析库 | ❌ 零依赖 | 自研路径，无 GPL 污染（见 `docs/third-party-notices.md`） |

## 2. 版本策略：R12 零改动保 byte-identity，R2000 派生

核心设计原则——**双路径互不污染**：

- `toDxf` 始终先按 R12 语义生成 token 流（HEADER/TABLES/BLOCKS/ENTITIES/EOF）。
- 仅当 `opts->version == R2000` 时，对 R12 文本再做一次纯字符串/ token 变换 `transformDxfTextToR2000`：
  - 改写 `$ACADVER AC1009 → AC1015`；
  - 每个对象注入 `5 <句柄>`、`330 <owner=*MODEL_SPACE 句柄>`、`100 AcDbEntity` 及具体子类标记（`100 AcDbLine` / `AcDbCircle` / …）；
  - 追加最小 `OBJECTS` 段（含 `*MODEL_SPACE` / `*PAPER_SPACE` 的 `BLOCK_RECORD`），使 owner 句柄可解析。
- **R12 路径完全不经过变换**，因此 `opts==nullptr` 或 `version==R12` 的输出与改造前逐字节相同（验收标准③）。

导出保真：图层 / 线型（`6`）/ 颜色（`62`→ACI→RGB）/ 块定义（BLOCKS 段）/ 标注（DIMENSION + `*Dn` 匿名块）/ 填充（HATCH 边界路径）/ 文字样式（STYLE 表）随版本映射，不做"一律炸开"退化。R2000 复用同一套 R12 基元展开，仅追加 AcDb 标记即可保留可编辑性。

## 3. 第三方内核评估（为何自研）

| 候选 | 许可 | 结论 |
|---|---|---|
| **LibreDWG (libredwg)** | **GPL** | ❌ 禁止引入（项目约定不得引入 GPL 依赖，见改动边界） |
| **ODA Drawings SDK** | 商业授权（需签约） | ❌ 本环境未接入，且引入商业 SDK 超出当前范围 |
| **libdxfrw** | GPL / 自定义 | ❌ 仅能读 DWG、写 DXF；且 GPL 同样受限 |

结论（与用户确认）：**不依赖第三方内核**，R2000 以「自研 DXF（ASCII + 二进制容器）」实现。DWG 二进制（尤其 R2000 的紧凑对象流）不手写——R2000 交付形态限定为 DXF；旧档与最广泛兼容形态仍是 R12（DXF + DWG）。许可声明见 `docs/third-party-notices.md`。

## 4. 二进制 DXF 容器

- 写：`asciiDxfToBinary` 在 ASCII DXF 字节前加 22 字节哨兵头
  `"AutoCAD Binary DXF\r\n" + 0x1A + 0x00`，其后为每个 `(组码 u8, 值)` 按类型序化的二进制组流。
- 读：`binaryDxfToAscii` 校验哨兵头、按类型反序列化为 token，再喂入现有 `fromDxf` 实体分发（R2000 的 100/5/330 标记被自然忽略）。
- 版本协商：`fromDxfBinary` 解码后复用 `detectDxfVersion` 探测 `$ACADVER`，不支持版本同样安全拒绝。
- 与「二进制 DXF」严格区分：它不是 `.dwg`，仅为 DXF 的紧凑载体，AutoCAD 打开时按 DXF 处理。

## 5. 版本协商与鲁棒性

- 读档第一步探测头部版本串：`AC1009`/`AC1012`/`AC1014`/`AC1015`/`AC1018` 视为支持；
  `AC1021`(R2004) 及以后（`AC1024`/`AC1027`/`AC1032`…）**拒绝**，返回 `false` 且 `err` 含人类可读版本名（如「不支持的文件版本：DWG 2004，请另存为 2000 或更早」）并写日志，**绝不崩溃**。
- 连读 20 个损坏 / 不支持文件：每个独立 `try` 风格解析，失败仅计数跳过，进程不退出（自测 ⑦ 覆盖）。
- 非 R12 DWG（头部非 `AC1009`）`fromDwg` 强校验拒绝（自测 ⑧）。

## 6. 调用方改动（仅 CadFormats 层 + 其调用方）

- `src/core/CadFormats.h`：`DxfVersion` 枚举、`DxfExportOptions::version/container`、`toDxfBinary`/`fromDxfBinary`、版本辅助函数（`dxfVersionLabel`/`cadVerToHuman`/`isSupportedDxfVersion`/`detectDxfVersion`）。
- `src/core/CadFormats.cpp`：`transformDxfTextToR2000`、`asciiDxfToBinary`/`binaryDxfToAscii`、`parseDxfTokens` 抽取、`fromDxf`/`fromDxfBinary` 版本协商、`toDxf`/`toDwg` 版本分派。
- `src/ui/MainWindow.cpp`：打开 / 另存过滤器标签更新为「R12/R2000」；`saveFile`/`writeDrawing`/`exportDxf` 透传版本（默认 R12）；`chooseDxfVersion()` 弹出 R12/R2000 下拉。
- `src/ui/Commands.cpp`：命令行 `dxfout <path.dxf> [-v R12|2000]`（默认 R12，`-v 2000` 导出 R2000）。
- **未改动** `Element` 结构体字段顺序（改动边界硬约束）；未引入 GPL 依赖。

## 7. 验收映射

| 验收标准 | 落点 |
|---|---|
| ① 黄金样本 round-trip 实体/图层/块 ≥99% | `t_formatsGolden` + `tests/golden/`：R12/R2000 ASCII DXF、R12/R2000 二进制 DXF、R12 DWG（断言实体数/图层集/块定义数 100% 一致） |
| ② 不支持版本返回 false + 连读 20 损坏文件不崩溃 | `t_formatsVersions` ⑥⑦⑧ |
| ③ 旧档 `opts==nullptr` 逐字节一致 | `toDxf` R12 路径零改动 + `t_formatsVersions` ①（R12 不含 AC1015/AcDb 标记） |
| ④ 双目标零警告 + `build.sh test` EXIT=0 + 自测失败数 0 | 全量构建零警告；自测 676/0 |
| 新增断言 ≥10 条 | `t_formatsVersions` 含约 30 条 CHECK，`t_formatsGolden` 含 5 条 |

## 8. 修复记录（实现中关键 bug）

- **misleading-indentation 警告**：`asciiDxfToBinary` 内 `for(...;...) out += ...; break;` 单行被 GCC 误判，拆为两行消除（双目标零警告要求）。
- **BLOCK/ENDBLK 块名丢失**：`transformDxfTextToR2000` 的 BLOCK 分支原只写子类标记丢弃原组（2 块名 / 70 标志 / 10-30 基点），回读时块名缺失、INSERT 悬空；改为保留原组后修复。
- **实体类型标记被吞（核心修复）**：`transformDxfTextToR2000` 收集单对象后多了一处 `++j;`，使 `i=j` 跳过下一个实体的 `0 <类型>` 头——除每段首实体外，所有 CIRCLE/POINT/HATCH 等类型标记丢失，其几何变为游离组被忽略或并入前实体（表现：R2000 回读 circle=0/point=0/hatch=0、line 异常增多）。移除该 `++j;` 后 R12/R2000 往返计数完全一致（c20 == c12 == [1,1,1,1,1,1,1]）。

## 9. 已知限制与后续

- R2000 仅以 DXF（ASCII / 二进制容器）交付；原生 R2000 `.dwg` 二进制未实现（需第三方 SDK 或自研复杂对象流，超出 P0-01 范围）。
- 高于 R2000 的版本（R2004+）一律拒绝并提示降级另存，符合「打不开的版本有清晰降级路径」。
- 若后续需原生 R2000 DWG，建议评估 ODA SDK 商业授权或（若许可允许）LibreDWG——须在 `docs/third-party-notices.md` 显式更新许可结论。
