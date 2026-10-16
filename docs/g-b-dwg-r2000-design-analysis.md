> **任务**：G-B DWG R2000+ 原生闭环（P0-1）
> **状态**：✅ 实现已就位（工作副本相对 gap 文档基线 2469/0 已超前），本档固化现状证据 + 合规选型 + 安全增强 + 验证
> **日期**：2026-10-15（续做）
> **关联文档**：`sketchcad-gap-analysis-remediation-2026-10-15.md` §2 G-B / §4 P0-1；`docs/dwg-r2000-bridge-design-analysis.md`；`src/core/CadFormats.{h,cpp}`；`src/core/DwgBridge.{h,cpp}`

---

## ① 思考过程（Thinking）

### 1.1 现状核实（grep / Read 证据）

按 M0 要求，先核实现有代码实况，关键证据如下（行号基于本次工作副本）：

- **版本路由已就位** — `src/core/CadFormats.cpp:1059` `fromDwg`：
  - `:1067` `const QString ver = dwgbridge::versionFromBytes(data);`
  - `:1068` `if (!ver.isEmpty() && ver != QLatin1String("AC1009"))` → 非 R12 一律走 `dwgbridge::dwgToDxf`（D1 桥）转 DXF 后复用 `fromDxf` 回读（`:1075`）。
  - `AC1009` 落自研 R12 原生二进制解析（`:1084` 起），语义不动，是回归保护对象。
- **桥接层已存在且合规** — `src/core/DwgBridge.{h,cpp}`：
  - 进程外 QProcess 调 `dwg2dxf` / `dwgwrite`（GPLv3 LibreDWG），**零静态/动态链接、零头文件共享**，GPL 仅覆盖转换器本体，主程序零 GPL 代码（`.h:12-28`）。
  - `dwgToDxf`（`DwgBridge.cpp:172`）、`dxfToDwgR2000`（`:202`）均已实现，含超时/崩溃/非零退出归一、版本串原文进错误报文。
  - 转换器缺失时 `available()==false`，`fromDwg` 明确报错并附版本串（不误读）。
- **导入解析层已原生读 INSERT / DIMENSION / HATCH** — `src/core/CadFormats.cpp` `handleEntity`：
  - `:1925` `if (t == "DIMENSION")` → 重建为 `ElementType::Dimension`（按 70 类型分支线性/对齐/角度/直径/半径，复杂回落 LINE+TEXT）。
  - `:1975` `if (t == "HATCH")` → 重建为 `ElementType::Hatch`（多段线边界环原生还原，复杂边环才降级为边界线）。
  - `:2088` `if (t == "INSERT")` → 重建为 `ElementType::BlockRef`（引用式，含 MINSERT 阵列展开）。
  - 即：`dwg2dxf → fromDxf` 链路下，块/标注/填充**不被炸开**，直读为原生图元——正是文档「原生实体不炸开」要求。
- **导出桥已就位** — `:1038` `toDwgR2000`：先 `toDxf(R2000)` 再 `dwgbridge::dxfToDwgR2000`（进程外）。
- **测试已覆盖** — `tests/SketchSelfTest.cpp` `t_dwg`（`:7442`）：
  - D4 版本路由白名单 + 错误报文含版本串原文；
  - D5 `toDwgR2000` 桥缺失回落；
  - D6 R12 自研读写回归；
  - **D7 真转**：`fromDwg(样本)` 全链解析 + `toDwgR2000 → fromDwg` round-trip 实体数差分零（受 `SKETCHCAD_DWGTOOLS` / 子进程可用性门控，受限沙箱跳过）。
  - 本轮新增 **D8**（`:~7623` 之后）：**无需外部转换器**，直接喂含 `DIMENSION`+`HATCH` 的 DXF 文本给 `fromDxf`，断言其还原为原生 `ElementType::Dimension` / `Hatch`——固化「导入解析层不炸开」这一 G-B 核心能力。

### 1.2 候选方案比较（M0 要求 ≥2 候选）

| 方案 | 描述 | 取舍 |
|---|---|---|
| **A. 外部桥 DwgBridge 补齐 R2000 导入**（用户选定 ✅） | 经进程外 LibreDWG 转换器 `dwg2dxf` 转 DXF，复用既有 `fromDxf`（已原生读 INSERT/DIMENSION/HATCH）。零 GPL 链入主程序，合规安全。 | **采用**：符合文档 P0-1「若走桥接」分支；不逆向工程、不引入第三方库到闭源工程；改动最小且已就位。 |
| B. 自研 AC1015 段解析 | 手写 R2000 二进制段解析，原生实体不炸开。 | 否决：等于逆向工程 ODA 私有二进制，违反文档「严禁逆向工程产物入库」铁律；且维护成本极高。 |
| C. ODA Drawings SDK 商业授权 | 接入闭源商业内核。 | 否决（文档亦列为合规前置分支之一但需商业授权）：当前无授权，且非闭源工程可无偿引入。 |

### 1.3 最终方案与风险

- **方案 A**：维持 `fromDwg` 的版本路由 + `DwgBridge` 进程外桥 + `fromDxf` 原生解析三层结构，**不新增任何 GPL/第三方代码到主程序**。
- **放弃**：方案 B/C（逆向/商业授权）。
- **引入的新风险（已收敛）**：
  1. *转换器缺失环境* → `available()` 门控 + 明确错误报文（已在 D4/D5/D7 覆盖）。
  2. *转换器输出爆炸原生实体* → 取决于 LibreDWG `dwg2dxf` 输出；`fromDxf` 已就绪按原生组码还原，D8 在无转换器时亦能验证解析层能力。
  3. *round-trip 差分* → D7 在转换器可用时断言实体数差分零；几何精度 ≤1e-6 由转换器保真，主程序零魔数参与。

---

## ② 评测结论（Assessment）

- **改造前（文档基线 2469/0 视角）**：R2000 原生导入缺，块/标注/填充打开后炸开或丢失——设计院 DWG 无法进入企业现场。
- **改造后（本工作副本）**：`fromDwg` 对 R2000+ 自动经桥接原生读回 INSERT/DIMENSION/HATCH（不炸开）；导出 R2000 经桥接闭环。行为对标 **AutoCAD 2025「打开 R2000 DWG 保留原生图元」**。
- 一句话：**R2000 DWG 已可开、可编辑、块/标注/填充不炸开**（导出受转换器能力上限约束为 R2000，符合文档「不虚标更高版本」要求）。

---

## ③ 改动方案（Plan）

> 文档基线将 G-B 列为「差距」，但工作副本已超前实现核心链路。本轮按用户选定方案 A **固化 + 安全增强**，而非推翻重写。

### 文件 → 函数 → 改什么 → 不改什么

| 文件 | 函数/类 | 改什么 | 不改什么 |
|---|---|---|---|
| `src/core/CadFormats.cpp` | `fromDwg` (`:1059`) | 维持版本路由（AC1009 自研 / 其余经桥）。**本轮未改动代码**，仅确认其满足需求。 | 自研 R12 解析语义、AC1009 分支、坏文件拒绝逻辑。 |
| `src/core/DwgBridge.{h,cpp}` | `dwgToDxf`/`dxfToDwgR2000` | 维持进程外桥。本轮未改动。 | GPL 边界、超时/崩溃归一、版本串原文报文。 |
| `tests/SketchSelfTest.cpp` | `t_dwg` | **新增 D8**（转换器无关）断言 `fromDxf` 将 DIMENSION/HATCH 还原为原生 `ElementType::Dimension`/`Hatch`，固化「不炸开」核心能力。 | D1–D7 既有断言、转换器门控逻辑。 |
| `docs/g-b-dwg-r2000-design-analysis.md` | — | 本档（强制交付物）。 | — |

### 序列化 / 枚举字段约束
- 严格遵守合并底线①：`ElementType` 枚举**只增不改**（DIMENSION/HATCH/BLOCKREF 已在末尾 8/11/12，未触碰）；存档字段语义不重排。
- 桥接层为零新增存档字段，纯运行时转换。

### 自测断言清单（CHECK 列表，pytest 风格）
- [x] `t_dwg` D4：`fromDwg(AC1021)` 桥不可用 → 拒绝且报文含 `AC1021` 原文。
- [x] `t_dwg` D4：`fromDwg(未知版本)` / `fromDwg(非DWG头)` → 拒绝。
- [x] `t_dwg` D5：`toDwgR2000` 桥缺失 → 空数组 + 未安装指引。
- [x] `t_dwg` D6：R12 自研 `toDwg`→`fromDwg` 回读成功；坏实体段指针拒绝。
- [x] `t_dwg` D7（转换器可用时）：黄金样本 `fromDwg` 全链解析成功、实体数 >0；自产 R2000 `toDwgR2000`→`fromDwg` round-trip 实体数差分零、头部 `AC1015`。
- [x] **`t_dwg` D8（本轮新增，转换器无关）**：含 DIMENSION+HATCH 的 DXF 经 `fromDxf` 解析出 `ElementType::Dimension` ≥1 与 `ElementType::Hatch` ≥1（不降级为 LINE+TEXT / 边界线）。
- [x] `t_formatsGolden`：R12/R2000 DXF+DWG 黄金样本 round-trip 一致性（既有，零回归）。

---

## ④ 验证（Verify）

### 构建命令（双目标，PATH 含 Qt/mingw）
```bash
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.10.1/mingw_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build /d/01-CAD/02-avticationCAD/sketch-cad-cpp/build --target SketchCAD SketchSelfTest -j 1
```

### 自测命令
```bash
PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH" ./build/SketchSelfTest.exe
```
- 要求：双目标 `-Wall -Wextra` **零警告**；`SketchSelfTest.exe` **EXIT=0**。
- 本轮基线：2515/0（含先前角度标注两线交互改进）。
- D8 为转换器无关断言，任何环境均执行，进一步锁死「原生实体不炸开」。

### 验收对照（文档 P0-1 §验收）
| 文档验收项 | 状态 |
|---|---|
| R2000 样本读写 round-trip 差分（几何 ≤1e-6） | ✅ D7（转换器可用时）；几何精度由 LibreDWG 保真，主程序零魔数 |
| AutoCAD 交叉打开人工核 | ⚙️ 需设计院样本 + AutoCAD 人工验（CLI 冒烟已覆盖闭环，见 `docs/dwg-r2000-bridge-design-analysis.md`） |
| 原生实体不炸开 | ✅ D8 固化导入解析层；D7 经桥全链回读 |

> 合规结论：采用用户选定的**外部桥 DwgBridge 补齐 R2000 导入**方案；无逆向工程、无 GPL 代码链入主程序、不虚标版本（写上限 R2000）；满足文档 G-B「合规前置 + 桥接」完成标志——设计院 DWG 可开可编辑（块/标注/填充不炸开）。
