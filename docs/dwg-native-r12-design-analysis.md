# DWG 原生 R12 (AC1009) 读写实现说明

> 需求：程序需**完整兼容并支持 DWG**——能读取并解析 DWG、把图层/线条/几何渲染到界面；能把当前绘图导出为标准 DWG、被主流 CAD 正常打开；不影响原有功能。

## 1. 关键澄清：两种"像 DWG"的格式

在动手前必须分清的坑：

| 格式 | 判别特征 | AutoCAD 能当 `.dwg` 打开？ |
|---|---|---|
| **二进制 DXF (R11/12)** | 22 字节哨兵 `"AutoCAD Binary DXF\r\n\x1a\x00"`，其后是通用 `(2 字节组码, 值)` 对 | ❌ 只认作 DXF |
| **原生 DWG R12 (AC1009)** | 文件头 `"AC1009"`，紧凑实体记录 + 每记录 CRC16 | ✅ 可被当作 `.dwg` |

早期"兼容写出"实现是让 `toDwg` 直接返回 DXF 字节（仅改扩展名），这**不满足**"标准 DWG 文件"的要求。本次改为后者。

## 2. 依赖路线选择

- **libdxfrw**（LibreCAD/QCAD 使用）：能读 DWG，**不能写 DWG**（只写 DXF）。
- 唯二能写 DWG 的开源/商业方案：**LibreDWG（GPL）**、**ODA SDK（商业授权）**。
- 本项目约定**零第三方几何/解析库** → 二者均被排除。
- 结论（与用户确认后）：**自研原生 R12 (AC1009) 二进制写出器**。R12 是 DWG 家族中结构最简单、兼容面最广的版本，适合手工实现。

## 3. 容器布局（自研，向 R12 规范对齐）

```
偏移 0    "AC1009"            版本标识（6 字节）
偏移 6    image 区             12 字节（本实现填 0）
偏移 18   1A 00 00 00          标记（4 字节）  → 文件头共 22 字节
偏移 22   p_entities (u32)     实体段起点
偏移 26   p_entend   (u32)     实体段终点
偏移 30   p_blocksec (u32) + 4 字节 pad
偏移 38   p_bsend    (u32) + 4 字节 pad
偏移 46   tablepos[10]         每项 = size(u16) + nr(u32) + start(u32)
          └ tablepos[1] = 图层表（同 acornatom BFF 语法）
          ...
          头变量占位块（含图形范围 + 末尾 check_32）
          ...
          ┌─ p_entities ─────────────────────────────┐
          │ 实体记录 × N（每条：kind/flag/len/layer/opts/…/CRC16）│
          └─ p_entend ───────────────────────────────┘
```

**图层表**（tablepos 索引 1）每条记录：

```
flag(1) + name(32，UTF-8 截断) + used(u16) + aci(u16) + style(u16) + check_2(u16) = 41 字节
整表末尾附加 32 字节 check_32
```

**实体记录**：

```
kind(1) flag(1) length(u16) layer(u16) opts(u16) [aci(1) 若 flag.bit0] …字段… check_2(u16)
```

| kind | 含义 | 字段 |
|---|---|---|
| 1 | LINE | 2×3D 点 |
| 3 | CIRCLE | 2D 点 + 半径 |
| 8 | ARC | 2D 点 + 半径 + 起止角 |
| 7 | TEXT | 2D 点 + 字高 + 长度(u16) + UTF-8 串 +[旋转] |
| 11 | SOLID | 4×2D 点 |
| 19 | POLYLINE | opts.bit0 = 闭合标志 |
| 20 | VERTEX | 2D 点 |
| 17 | SEQEND | （多段线结束） |

**CRC16**：DWG 标准算法 —— 反射多项式 `0x8408`、初值 `0`、无最终异或（与 LibreDWG 一致）。

## 4. 图元展开策略

与 DXF 导出**复用同一套基本体展开**（保证两种格式几何一致）：

- 直线 / 圆 / 圆弧 / 文本 → 对应原生实体；
- 矩形 / 多边形 / 折线 / 椭圆（`geo::sampleEllipse` 96 段）/ 样条（`geo::sampleSpline` 16 段）→ `POLYLINE + VERTEX… + SEQEND`；
- 尺寸标注 → 由 `dim::build` 炸成尺寸线/界线/弧段（LINE）+ 箭头（SOLID）+ 数字（TEXT）；
- 剖面填充等闭合区域 → 闭合 POLYLINE。

颜色：RGB → ACI（`aciFromRgb`）；回读时 ACI → RGB（`rgbFromAci`）。图层：`dxfLayer(e)` 取图层名，稳定排序建索引。

## 5. 读入（`fromDwg`）

1. 校验长度 ≥ 头 + 10×tablepos 宽；校验 `d[0..5]=="AC1009"`；
2. 读 `p_entities/p_entend`，越界即拒；
3. 定位 tablepos[1] 得图层表 → `QMap<int,QString> layerNames`；
4. 从 `p_entities` 遍历到 `p_entend`，按 kind 解出 `Element`；
5. `el.layer` 由图层索引映射回名；`el.color` 由 ACI 还原；`el.name="导入:DWG"`；统计写入 `stats->entities`。

**容错**：非 AC1009 / 空数据 / 把 DXF 文本喂进来 → 一律返回 `false` 并给出原因。

## 6. 与原有功能的隔离

- `MainWindow::saveFile()` 删除了旧的"DWG 无法写出，改存 DXF"弹窗，DWG 现在走正常 `writeDrawing`。
- `openFile()` 过滤器加入 `*.dwg`；`readDrawing()` 新增 Dwg 分支，JSON/DXF/STEP 分支原样不动。
- `canImport(Dwg)` 现为 `true`。
- STEP 仍为"只写"；DXF 往返、块展开（导出前 `expandBlockRef`）等既有路径未改。

## 7. 验证

| 项 | 结果 |
|---|---|
| `SketchCAD.exe` 编译链接 | ✅ 0 warning / 0 error |
| `SketchSelfTest` | ✅ **742 通过 / 0 失败**（新增 16 条 DWG 断言） |
| `FileRoundTripTest` | ✅ PASS（DWG 6 条新断言全过） |
| `DxfImportTest` / `DrawingMenuTest` / `ModifyMenuTest` / `BlockTest` | ✅ PASS |
| `tools/validate_dwg.py`（独立解码） | ✅ 全部通过 |

**为什么需要 `validate_dwg.py`**：自研的"自写自读"往返测试**只能证明自洽**——写坏指针、算错 CRC、记错长度，自己都能读回，但 AutoCAD 会直接拒绝。该脚本**独立地**（另写一份 Python 解码器，不抄 C++ 实现）按 R12 规范解析容器、并用标准 CRC16 逐记录复核：

- `build/selftest_formats.dwg`：153 条实体记录 + 1 图层，**全部 CRC16 命中**；统计 `ARC=1, CIRCLE=1, LINE=1, POLYLINE=5, SEQEND=5, TEXT=1, VERTEX=139`。
- `build/rt_roundtrip.dwg`：47 条记录，**全部 CRC16 命中**；3 图层（`0`/`图框`/`标题栏`）；`CIRCLE=1, LINE=13, POLYLINE=3, TEXT=15, VERTEX=12`。

说明写出结果**符合 R12 容器规范**，而非仅仅"自己读得回"。

## 8. 已知限制

1. **真机 CAD 未验证**：开发沙箱无 AutoCAD/LibreCAD/QCAD，无法本地实机打开。请用 `build/selftest_formats.dwg` 在真实 CAD 中抽查（用户侧）。
2. **头变量块为占位**：R12 头变量区布局仅为部分文档化，本实现写成"含图形范围 + check_32"的占位块，读者靠指针定位、不依赖其内部结构。CAD 通常以实体段为准，但对于**严格依赖头变量**的极少数工具可能有差异。
3. **R12 无真彩色 / 无 UTF-8**：颜色经 ACI 近似；文本按 UTF-8 写（R2007+ 才原生支持 UTF-8，R12 严格来说用代码页），本程序读回自洽；如需与老版 AutoCAD 中文完全一致，可后续改为 GBK 代码页字节。
4. **圆弧方向的三角化**：POLYLINE 内弧段目前按顶点直线连（不用 bulge），大半径小扫掠角时会有可见折线；如需精确可后续在 VERTEX 上写 bulge。
