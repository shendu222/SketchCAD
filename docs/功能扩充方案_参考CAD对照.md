# 功能扩充方案（基于《参考CAD 用户手册》—— 浩辰 CAD 机械 2027 Linux 版）

> 文档类型：功能扩充方案（WHAT + HOW + AI 提示词）  
> 对标手册：浩辰 CAD 机械 2027 Linux 版操作手册（18 章 + 附录 A 命令参照表，156 页）  
> 目标程序：**SketchCAD**（`sketch-cad-cpp/`，C++17 / Qt 6.10.1 / MinGW 13.1 / CMake+Ninja）  
> 配套文档：`二维CAD差距分析与AI编程实施方案.md`（V1，对标 AutoCAD/ZWCAD/CAXA 的差距与阶段划分）、`AI提示词驱动的CAD能力持续改进方案.md`（V2，提示词体系与迭代框架）。本方案 = **把参考 CAD 机械专有的 18 章能力，逐一映射为 SketchCAD 的"继承 / 部分继承 / 新增"清单，并给出实现思路与可直接套用的 AI 提示词**。

---

## 0. 文档定位与现状基线

### 0.1 来源与范围

手册 18 章能力域：

| 章 | 能力域                  | 章  | 能力域              |
| - | -------------------- | -- | ---------------- |
| 1 | 图纸（图框/标题栏/各栏）        | 10 | 系列化零件设计          |
| 2 | 序号与明细表               | 11 | 参数化国标图库          |
| 3 | 绘图工具（中心线/构造线/专用线）    | 12 | 超级符号库            |
| 4 | 构造工具（公式曲线/孔/槽/螺纹/打断） | 13 | 机械零件设计（轴/弹簧/齿轮）  |
| 5 | 辅助工具（截面特性/批量/块/刷子）   | 14 | 参数化设计（变量表达式/驱动）  |
| 6 | 视图工具（局部放大/剖切/投影/消隐）  | 15 | 扩展工具（拆图/批量打印/排图） |
| 7 | 尺寸标注（智能/多重/各类标注）     | 16 | 工具（自动表格/卡片/孔特征表） |
| 8 | 符号标注（粗糙度/形位公差/焊接/基准） | 17 | 系统维护（标准切换/样式/备份） |
| 9 | 文字处理（技术条件/遮挡/统一）     | 18 | 操作实例（文档化，非代码）    |

本方案**不重复** V1 的阶段划分与 V2 的提示词治理，只回答三件事：① 这 18 章里 SketchCAD 现在有什么、还缺什么；② 缺的部分怎么在工程现有架构上落地；③ 交给 AI 编程时具体怎么下提示词。

### 0.2 当前 SketchCAD 已继承能力快照（代码证据）

以下模块**已落地**，构成本方案的"继承"基础（证据为 `src/core`、`src/ui` 实际文件）：

| 能力域         | 已继承模块（文件）                                                                                                                 | 说明                                      |
| ----------- | ------------------------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| 图框/标题栏      | `SheetFrameTemplate.*`、`SheetSpec.h`、`sheet_frame.cpp`、需求 1–13（含附加栏/代号栏/参数栏/模板持久化）                                        | 新建图框 + 三新栏 + 模板齐全，标题栏 `fitCellText` 自适应 |
| 明细表结构       | `BomTree.*`、`sheet_frame.cpp:521` 列头（序号/代号/名称/数量/材料/重量/备注）                                                                | BOM 面板与列结构已建                            |
| 符号标注（部分）    | `Gdt.*`（形位公差）、`SurfaceRoughness.*`（粗糙度）                                                                                   | 粗糙度 + 形位公差核心符号已生成                       |
| 剖面/剖切       | `Section.*`、`SectionCutSymbol.*`、`ProjectionSymbol.*`、`SectionDrawing.*`                                                  | 剖面线、剖切符号、投影符号已建                         |
| 参数化零件视图     | `AircraftViews` / `FlangeViews` / `BearingViews` / `HousingViews` / `FlangeHubViews` / `FloorPlanViews`（core+ui 各一套）      | 6 套参数化制图模板                              |
| 约束/参数化      | `ConstraintSolver.*` + `ConstraintSolverFacade.*` + `IConstraintSolver.*`                                                 | 几何约束 + 尺寸驱动雏形                           |
| 标注体系        | `DimStyle.*`、`DimensionStandard.h`                                                                                        | 12 类标注 + 标注样式                           |
| 打印/布局       | `PrintEngine.*`、`PlotRenderer.*`、`PrintPreviewDialog.*`、`PrintSettingsDialog.*`、`src/com/BatchPlotEngine.*`、`PlotStyle.*` | 页面设置/预览/PDF/批量打印已建                      |
| 表格/FIELD    | `TableDialog.*` + FIELD 域 + Table 公式列（内存文档已确认 2375/0）                                                                     | 自动表格 + 公式列已建                            |
| 构造线         | `xline` 命令（CommandCatalog:46，含 h/v/angle/bisector/parallel 子模式）                                                           | 构造线已建                                   |
| 倒圆/倒角/镜像/阵列 | `fillet`/`chamfer`/`mirror`/`rotate`/`array`                                                                              | 基础修改命令已建                                |
| 命令体系        | `CommandCatalog.*`（111 精确名 + 家族表）、`AliasTable.*`、`CommandPalette`                                                         | 命令发现/别名/补全已建                            |
| 格式          | `CadFormats.*`（DXF R12/R2000、DWG R12）、`DwgBridge.*`                                                                       | 导入导出已建                                  |
| 崩溃保护        | `AutoSave.*`（快照 + 转储）                                                                                                     | 增量/崩溃恢复已建                               |

> **结论**：SketchCAD 已具备"通用 2D CAD + 国标图框/标题栏 + 部分机械符号 + 参数化视图 + 打印"主干；**机械 CAD 专有工具链（中心线家族、序号球标、孔/槽/螺纹、视图投影、焊接/基准符号、系列化出库、参数化国标图库、报表/重量/批量）是主要缺口**。

### 0.3 总体策略

1. **继承项**：仅做"补齐子能力 + 国标版本变体"，不重构主干。
2. **部分继承项**：在既有模块上追加（如 `SurfaceRoughness` 补 2006/2018 变体、`SectionCutSymbol` 补更新联动）。
3. **新增项**：一律走"纯函数生成器 + 单一 Element 类型/扩展字段 + 同源序列化 + 独立自测 target"的工程范式（见 §2 通用范式）。
4. **优先序**：承接外部机械图纸（Ch2/Ch3/Ch4/Ch8）→ 出图完备（Ch6/Ch7/Ch15）→ 设计提效（Ch10–14）→ 生态（Ch11/12/16/17）。

---

## 1. 功能清单：继承原有功能 vs 新增功能（含详细说明）

图例：✅ 继承完整　🟡 部分继承（需补全）　🆕 新增

### 1.0 总览矩阵

| 章  | 能力域     | 状态     | 关键缺口                                            |
| -- | ------- | ------ | ----------------------------------------------- |
| 1  | 图纸      | ✅/🟡   | 图框复制、机械实体特性表、批量填充完善                             |
| 2  | 序号与明细表  | 🟡/🆕  | 序号球标生成/对齐/合并/重排/隐藏、明细表联动                        |
| 3  | 绘图工具    | 🆕（多数） | 中心线 10 种、系列圆、对称/平行/切线、波浪/双折/管道线、箭头              |
| 4  | 构造工具    | 🟡/🆕  | 轴断线、相贯线、截交、折断符号、砂轮越程槽、退刀槽、螺纹、参数化孔/孔阵、删除重复、缩放 XY |
| 5  | 辅助工具    | 🆕（多数） | 截面特性、计算重量、批量数据提取、图档 BOM 分析、刷子、图层变换、图纸改名/目录      |
| 6  | 视图工具    | ✅/🆕   | 向视符号、孔轴投影、转角投影、更新/取消消隐                          |
| 7  | 尺寸标注    | ✅/🆕   | 多重/折弯/弧长/螺纹/倒角/引线/总长标注、编辑整理、公差查询                |
| 8  | 符号标注    | ✅/🆕   | 基准符号、锥斜度、中心孔、圆孔标记、标高、焊接符号三版、旋转符号                |
| 9  | 文字处理    | 🟡/🆕  | 文字理论线框、下划线、统一、遮挡、技术条件                           |
| 10 | 系列化零件设计 | 🟡     | 系列化出库/编辑/提取标准件信息机制                              |
| 11 | 参数化国标图库 | 🆕     | 标准件库出库/维护                                       |
| 12 | 超级符号库   | 🆕     | 符号库调用/新增                                        |
| 13 | 机械零件设计  | 🟡     | 轴/弹簧/齿轮设计深化、渐开线齿形                               |
| 14 | 参数化设计   | 🟡/🆕  | 变量表达式、显示尺寸表达式/附加尺寸/过约束、基点显隐                     |
| 15 | 扩展工具    | ✅/🆕   | 拆图备档、机械图拆分、自动排图、批量签名                            |
| 16 | 工具      | ✅/🆕   | 超级卡片、卡片编辑                                       |
| 17 | 系统维护    | 🟡     | 标准切换、样式管理器、自定义默认模板                              |
| 18 | 操作实例    | 📄     | 文档化（非代码）                                        |

### 1.1 第一章 图纸（✅ 主干继承，🆕 少量补全）

- **继承 ✅**：新建图框（需求 1–13，含幅面驱动打印页面设置）、主图框修改/设定/显示、标题栏填充（`TitleBlockLayout::fitCellText` 自适应）、附加栏/代号栏/参数栏填充（需求 10–13）、模板持久化（`SheetFrameTemplate` + `QSaveFile` 原子写）。
- **部分继承 🟡**：标题栏批量填充——当前仅单图框填写，缺"多图框按列映射批量写入"对话框。
- **新增 🆕**：
  - **1.11 图框复制**：复制当前图框矩形 + 其内所有标题栏/明细栏/各栏元素到新位置（含块引用克隆）。
  - **1.12 机械实体特性表**：选中若干图元 → 汇总长/宽/面积/数量生成"特性表"图元（复用 `Table` + FIELD `AREA:id`/`COUNT`）。

### 1.2 第二章 序号与明细表（🟡/🆕 重点缺口）

- **继承 ✅**：BOM 面板 `BomTree`、标题栏/明细栏列头结构（`sheet_frame.cpp:521`）。
- **新增 🆕（核心机械出图能力）**：
  - **2.3 序号**：生成带指引线的球标（圆/横线样式，国标 GB/T 4458.2），自动递增编号，关联明细表行。
  - **2.4–2.5 明细表生成/处理**：由序号反向生成/刷新明细表，支持按列排序、合并同名行。
  - **2.6–2.9 隐藏/显示/删除/修改序号**：球标显隐与编号改写，联动明细表。
  - **2.10 序号零件复制**：复制零件时同步复制其序号球标与明细行。
  - **2.11 序号对齐 / 2.12 序号合并 / 2.13 序号定位 / 2.15 序号重排**：排版与重编号工具。
  - **2.14 悬停显示 BOM**：鼠标悬停球标弹出该行 BOM 数据（复用 `InquiryDialog` 浮层）。

### 1.3 第三章 绘图工具（🆕 主体）

- **继承 ✅**：`xline` 构造线、基础圆/弧/矩形、中心线基础（部分线型）。
- **新增 🆕**：
  - **3.2 中心线家族（10 种）**：中心线、十字中心线、圆心标记、过角点/平板/整圆/带角度/带孔/过孔/两直线间十字中心线——统一为 `makeCenterLine(kind, refs)` 纯函数生成器，输出 `Line`+`LineType=CENTER`（ByLayer 中心线型）。
  - **3.4–3.7 已知圆心/端点画圆/弧**：快捷入口（当前仅通用 circle/arc，补"指定圆心→切于对象"等机械常用入口）。
  - **3.8 系列圆**：沿直线/圆周等距阵列圆。
  - **3.9 双边偏移 / 3.10 对称画线 / 3.12 平行线·垂直线**：基于 `Geometry::offset`/`parallel` 原语。
  - **3.13–3.16 切线/公切线/点和圆切线/垂分线**：求交/切点的几何原语（**G1 门禁**，需解析解对照）。
  - **3.17–3.19 角度线/平(角)分线/放射线**：角度构造器。
  - **3.20–3.22 波浪线/双折线/管道线**：剖面断裂/管线专用多段线生成器。
  - **3.24 箭头**：国标箭头块（`BlockRef` 到 `Arrow` 符号）。

### 1.4 第四章 构造工具（🟡/🆕）

- **继承 ✅**：剖面线（`Section`）、打断（`break_at`）、倒圆/倒角（`fillet`/`chamfer`）、镜像（`mirror`）、合并图元（join 部分）、动态延伸（extend 部分）。
- **新增 🆕**：
  - **4.2 公式曲线**：当前有 `FC` 雏形，需收敛为"表达式 → 采样点 → 多段线"统一管线（**G1：公式解析与采样由人工复核**）。
  - **4.3 轴断线 / 4.10 折断符号**：断裂处波浪/斜线符号。
  - **4.4 相贯线 / 4.7 截交**：两回转体交线、截平面交线（**G1 几何难点**，优先用数值采样 + 容差拟合，禁用自造解析）。
  - **4.12 砂轮越程槽 / 4.13 退刀槽·清角·止裂孔**：参数化沟槽符号（输出轮廓多段线 + 尺寸）。
  - **4.14 螺纹 / 4.15 参数化孔 / 4.16 单孔 / 4.17 孔阵 / 4.18 构造孔**：孔特征生成器（复用 `FloorPlanViews` 孔逻辑，抽为 `HoleGenerator`），支持 GB 螺纹规格表（**国标数值禁 AI 改写**）。
  - **4.21 删除重复（OVERKILL）**：几何相等判定 + 合并（容差走 `geo::almostEqual`）。
  - **4.22 缩放 XY**：X/Y 独立缩放（`scale` 命令扩展）。

### 1.5 第五章 辅助工具（🆕 主体）

- **继承 🟡**：块工具（炸开属性块、统计块部分）、图层变换（layer 部分）、命令记忆器（recent commands）。
- **新增 🆕**：
  - **5.1 超级编辑**：按图元类型弹出最相关编辑对话框的分派器。
  - **5.3 截面特性 / 5.4 计算面积**：多边形截面面积、形心、惯性矩（**G1 数值**）。
  - **5.5 词句库 / 5.7 批量数据提取 / 5.17 提取数据表格**：结构化文本与表格提取（复用 `Table`）。
  - **5.6 导出 Excel**：BOM/提取表导出（CSV/XLSX，第三方库需 GPL 白名单审查，见 G3）。
  - **5.8 批量 DWG 浏览 / 5.9 批量文字查找 / 5.14 图纸改名 / 5.15 图纸目录**：文件级批处理（走 `CadFormats` 读档头信息，避免全量解析）。
  - **5.10 图档 BOM 分析**：多图 BOM 汇总比对。
  - **5.11 块工具**：统计/改名/拉伸/炸开属性块全集。
  - **5.12 刷子工具（相同/计算/+1）**：`matchprop` 变体（同属性刷、计算刷、序号+1 刷）。
  - **5.16 计算重量**：密度表 × 体积/面积 → 重量（密度表人工提供）。
  - **5.18 修改符号标注比例**：批量缩放符号类图元。

### 1.6 第六章 视图工具（✅/🆕）

- **继承 ✅**：局部放大（`GMDETAIL`）、剖切符号（`SectionCutSymbol`）、图形消隐（`GMHIDE`）。
- **新增 🆕**：
  - **6.4 更新局部放大**：源几何变更后刷新放大视图（建立"源视口→放大图"关联，复用约束/字段联动机制）。
  - **6.6 向视符号**：箭头 + 字母标识视图方向。
  - **6.7 孔轴投影 / 6.8 转角投影**：典型机械视图投影辅助线生成。
  - **6.10 取消消隐**：消隐关联的反操作。

### 1.7 第七章 尺寸标注（✅/🆕）

- **继承 ✅**：智能标注（`GMPOWERDIM`）、12 类基础标注、标注样式（`DimStyle`）。
- **新增 🆕**：
  - **7.4 多重标注 / 7.11 折弯标注 / 7.13 弧长标注 / 7.12 螺纹标注 / 7.17 倒角标注 / 7.18 引线标注 / 7.19 标注总长**：在既有 `Dimension` 类型上扩展子类型与绘制分支。
  - **7.20 编辑标注整理（视图/单向）**：批量对齐标注。
  - **7.21 公差查询**：调出 GB 公差表填注（数值表人工提供，**G3/G1**）。

### 1.8 第八章 符号标注（✅/🆕 重点）

- **继承 ✅**：粗糙度（`SurfaceRoughness`）、形位公差（`Gdt`）。
- **部分继承 🟡**：粗糙度 2006 / 形位公差 2018 为样式变体，需在既有生成器上补分支。
- **新增 🆕**：
  - **8.6–8.8 基准符号 / 新基准符号 / 基准标注 2018**：基准三角形 + 字母。
  - **8.9 锥斜度 / 8.10 中心孔 / 8.11 圆孔标记 / 8.12 标高符号**：专用符号生成器（统一为 `mkSymbol(name, anchor, params)`）。
  - **8.13–8.15 焊接符号（2008/2012/表示法）**：焊接标注框格（**G1 国标框格排版**）。
  - **8.16 旋转符号**：旋向标识。

### 1.9 第九章 文字处理（🟡/🆕）

- **继承 🟡**：单行/多行文字（`Text`/`MText`）。
- **新增 🆕**：
  - **9.3 文字理论线框 / 9.4 文本下划线 / 9.5 文字统一（批量改样式）/ 9.6 文字遮挡（背景遮罩 mask）/ 9.7 技术条件**：技术要求段落编辑（复用 `MTextDialog` + 词句库）。

### 1.10 第十/十一/十二章 系列化零件·国标图库·超级符号库（🟡/🆕）

- **继承 🟡**：`*Views` 6 套参数化视图（相当于内置系列化零件）。
- **新增 🆕**：
  - **10.3–10.5 系列化出库/编辑/提取信息**：把 `*Views` 抽象为"零件库 + 参数表 + 出库"，统一 `PartLibrary` 接口。
  - **11 参数化国标图库**：GB 标准件（螺栓/轴承/垫圈…）参数化出库 + 库维护（数据驱动，**国标尺寸禁 AI 改写**）。
  - **12 超级符号库**：符号图块库调用/新增（复用 `Block` + `DesignCenterPanel`）。

### 1.11 第十三章 机械零件设计（🟡）

- **继承 🟡**：轴类（`GMSHAFT`）、弹簧（`GMSPRING`）、齿轮（`GMGEARDESIGN`）设计雏形（命令/对话框存在）。
- **新增 🆕/深化**：渐开线齿形绘制（**G1：渐开线参数方程由人工给定**）、齿轮参数表联动、轴/弹簧设计深化（退刀槽/越程槽联动 Ch4）。

### 1.12 第十四章 参数化设计（🟡/🆕）

- **继承 🟡**：`ConstraintSolver` 几何约束 + 尺寸驱动。
- **新增 🆕**：变量表达式（`defineVariable`/`expression`）、显示尺寸表达式/附加尺寸/过约束尺寸、参数化基点显隐——在 `ConstraintSolverFacade` 上扩"变量层"，驱动既有约束。

### 1.13 第十五/十六/十七章 扩展·工具·维护（✅/🆕）

- **继承 ✅**：批量打印（`BatchPlotEngine`）、自动表格（`Table`）、孔特征表（`KB`）、自定义图层/默认模板/增量备份/自定义标题栏。
- **新增 🆕**：拆图备档、机械图拆分、自动排图、批量签名（Ch15）；超级卡片/卡片编辑（Ch16）；标准切换、样式管理器（Ch17，部分接 `AppSettings`）。

---

## 2. 各项功能的实现方法与技术思路

### 2.1 通用落地范式（所有新增项必须遵守）

```
新增图元/符号 → 单一生成器纯函数(src/core/XxxGen.cpp)
               → 输出既有 Element 类型或新增 ElementType(只增不改, 同步 8 处)
               → 经 buildSheetFrameWithOptions 同源预览/渲染
               → toDrawingBytes() 单一序列化入口(条件写键, 旧档回落)
               → 独立 tests/ 自测 target(≥8 断言, 含退化用例)
               → 命令注册(CommandCatalog::add) + 菜单 mk() + 别名
```

**铁律（写进 L0 提示词）**：枚举只增不改；一次操作一次 `pushMemento`；几何只能调 `Geometry.h`/`Modify.h` 原语；新增图元同步 8 处；禁止改既有断言；国标数值表由人工提供。

### 2.2 模块 A：图框与标题栏补全（Ch1）

- **图框复制**：`SheetFrameTemplate::cloneFrame(rect, offset)` → 复制图框层矩形 + 标题栏/明细栏/各栏元素（`element.layer` 过滤）+ 块引用深拷贝；走 `Quiet` API 批量创建。
- **机械实体特性表**：基于选中图元几何（`Element::boundsMm`、`ElementValidate` 守卫）算长/宽/面积，调 `Table` + FIELD 公式列生成表格图元。

### 2.3 模块 B：序号与明细表（Ch2，最高优先）

- **数据模型**：新增 `BomItem{seq, code, name, qty, material, weight, remark, sourceIds[]}`；序号球标 = `BlockRef`（圆/横线样式）+ 关联 `BomItem.seq`。
- **联动**：球标生成 → 写 `BomItem` → 刷新 `BomTree` 与明细表图元（`applyChanges` 钩子 `refreshFields`，沿用 FIELD 联动机制）。
- **操作集**：对齐/合并/重排 = 对 `BomItem.seq` 重排 + 球标位置几何更新（批量 `pushMemento` 一次）。
- **序列化**：`BomItem` 列表写入存档顶层 `bom` 键（无数据不写键，旧档回落空）。

### 2.4 模块 C：机械绘图/构造工具（Ch3+Ch4）

- **中心线家族**：`CenterLineGen::make(kind, const std::vector<Element>& refs, const Params&)` 返回 `std::vector<Element>`（Line + `lineType=CENTER`）。10 种变体 = 不同参考组合（两圆→圆心标记+十字；角点→过角点中心线；整圆→过整圆十字…）。**G1**：圆心/交点计算用 `Geometry::center`、`intersect` 既有原语，退化（半径 0、重合）须返回 false + log。
- **专用线（波浪/双折/管道）**：采样生成 `Polyline`（波浪=正弦采样；管道=双线偏移）。
- **孔/槽/螺纹（Ch4）**：抽 `HoleGenerator::make(spec)` → 轮廓 `Polyline` + 中心符号；GB 规格表（`HoleSpecTable` 常量，人工维护）。退刀槽/越程槽 = 参数化轮廓 + 尺寸标注联动。
- **相贯线/截交（Ch4.4/4.7）**：**G1 难点**——先用数值采样（离散轮廓交线）生成多段线，标注"需人工复核"；不得以解析近似替代。


### 2.5 模块 D：辅助与报表工具（Ch5）

- **截面特性/计算重量**：`SectionProps::compute(polygon)` → 面积/形心/惯性矩（多边形公式，G1 解析对照）；重量 = Σ(面积×密度)，密度表 `DensityTable` 人工提供。
- **批量文件工具**：`BatchDwgScanner` 只读档头（图框/标题栏块属性）提取 BOM，不全量解析（性能 G5）；导出走 `Table` + CSV。
- **刷子工具**：`matchprop` 扩展三模式（相同/计算/+1），复用 `Element` 属性复制 + 表达式求值。

### 2.6 模块 E：视图工具（Ch6）

- **向视符号/孔轴投影/转角投影**：符号生成器 + 投影辅助线（正交/旋转投影矩阵，复用 `UCS`/`Transform` 原语）。
- **更新/取消消隐**：消隐关联存 `HideLink{sourceId, viewId}`；更新=重算遮挡；取消=删关联 + 恢复。

### 2.7 模块 F：尺寸与符号标注（Ch7+Ch8）

- **标注子类扩展**：在 `Dimension` 上扩 `kind`（折弯/弧长/螺纹/倒角/引线/总长），`View` 绘制分支 + `DimStyle` 参数；螺纹标注=引线+字母代号（GB 螺纹代号表）。
- **符号扩展**：统一 `SymbolFactory::mk(name, anchor, params)` 输出 `BlockRef` 或专用 `Element`；基准符号/焊接/锥斜度/中心孔/标高/旋转 = 不同 `name` 分支。**焊接符号框格排版 = G1 国标要点**。

### 2.8 模块 G：文字处理（Ch9）

- 文字遮挡=渲染时背景遮罩（复用 `TextMetrics` 度量 + `QPainter` 背景矩形）；技术条件=`MText` + 词句库下拉；文字统一=批量改 `TextStyle` 属性。

### 2.9 模块 H/I：系列化·国标图库·零件设计·参数化（Ch10–14）

- **零件库抽象**：`PartLibrary{ list(), checkout(partId, params)→Elements, edit(), extractInfo() }`；`*Views` 改造为库实现之一。
- **参数化国标图库**：数据驱动（JSON 规格表 + 生成器），出库=实例化 `BlockDef`。
- **齿轮渐开线**：`InvoluteGear::profile(teeth, module, ...)` → 齿廓采样多段线（**G1：渐开线方程人工给定**）。
- **参数化设计**：`VariableExpr` 层挂在 `ConstraintSolverFacade` 上，`defineVariable`/`drive` 命令驱动既有约束；显示表达式=在标注旁渲染变量名。

### 2.10 模块 J：扩展/工具/维护（Ch15–17）

- **机械图拆分/拆图备档**：按图层/图框切分存档（复用 `CadFormats::writeDrawing` 分片）。
- **自动排图**：图框矩形装箱（简单二叉树/ shelf 算法），输出布局。
- **标准切换/样式管理器**：接 `AppSettings`（standard 键），触发 `rebuildMenusForLanguage` 同类刷新机制。

### 2.11 风险门禁映射

| 风险    | 涉及模块               | 门禁                                 |
| ----- | ------------------ | ---------------------------------- |
| 几何错误  | 中心线/切线/相贯线/截面特性/齿轮 | **G1**（解析解/属性测试/蒙特卡洛）              |
| 数值精度  | 所有采样/容差            | **G2**（`geo::almostEqual`，禁裸 `==`） |
| 国标/许可 | 螺纹/公差/焊接/Excel 导出库 | **G3**（数值人工提供，依赖 GPL 审查）           |
| 存档兼容  | 新增 BomItem/符号字段    | **G4**（只增不改，条件写键，旧档字节级回落）          |
| 性能    | 批量扫描/消隐/排图         | **G5**（10 万图元阈值，回归 >5% 拒）          |
| 撤销语义  | 序号重排/批量操作          | **G6**（一次 pushMemento；随机序列属性测试）    |

---

## 3. 用于 AI 辅助编程的具体提示词

### 3.1 L0 基础系统提示词（SketchCAD 定制版，必注入每次调用）

```text
你是 SketchCAD（C++17 / Qt 6.10.1 / MinGW 13.1 / CMake+Ninja 二维机械 CAD）的资深开发助手。
【项目身份】src/core 只依赖 Qt+几何原语，不依赖 src/ui；所有图元经 toDrawingBytes() 单一序列化。
【技术栈】C++17 + Qt6；成员前缀下划线(model_)；QStringLiteral；中文注释；CMake 每模块独立 target。
【铁律】
1) ElementType / DimensionType 枚举只增不改；新图元同步 8 处(枚举/sizeDesc/字段/Factory/JSON双向/View/distanceToElement/CMake)。
2) 一次用户操作只 pushMemento 一次；id 从 1 起，id=0 永不存。
3) 几何计算只能调用 Geometry.h / Modify.h / Section.cpp 既有原语，禁止自造几何算法。
4) 国标数值表(线宽组/剖面材料/螺纹/公差/密度)由人工提供，AI 不得改写数值。
5) 禁止改既有测试断言来让测试通过；禁止引入 GPL/LGPL 依赖(白名单 MIT/BSD/Apache)。
【风格】错误统一 model 层校验→log(...,"warn")→返回 false/-1；不在 UI 层做业务校验；单一数据源集中在对应头文件。
【验收基线】-Wall -Wextra 零警告；SketchSelfTest 全量 EXIT=0；新模块≥8断言含退化用例；CMake 显式注册 target。
【ESCALATE】遇到几何算法/数值精度/国标合规/存档格式/性能基线问题时，输出候选方案并停止实现，等待人工确认(对应门禁 G1~G6)。
```

### 3.2 迭代框架（六要素 + 反馈包，详见 V2；此处给最小契约）

每次提示词 = `L0` + `L1模块提示词` + `L2任务骨架` + `六要素(GOAL/OUT_OF_SCOPE/CONTEXT/IO_SPEC/ACCEPT/FORBID)` + `[可选 FEEDBACK]`。轮次上限 4，超限转人工。

### 3.3 逐模块具体提示词（可直接复制，选取最高价值新增项）

#### P1 — 中心线家族（Ch3.2，🆕）

```text
[L0][L1: core/geometry][L2: feature]
GOAL: 实现 CenterLineGen::make(CenterLineKind kind, const std::vector<Element>& refs, const CLParams& p)
      返回 std::vector<Element>（Line 序列，lineType=CENTER，layer 取当前层）。
      支持 10 种：kCenter/kCross/kCircleMark/kCorner/kPlate/kFullCircle/kAngle/kHole/
                  kThroughHole/kTwoLinesCross。
OUT_OF_SCOPE: 不改现有 Line/Arc 渲染；不改枚举；不新增 ElementType（复用 Line+lineType）。
CONTEXT(必注入): src/core/Geometry.h(Geometry::center/intersect/parallel 原语)、
                 src/core/Element.h(Line 构造)、src/core/ElementFactory.h。
IO_SPEC: CLParams{ double extend; double gap; }; 失败返回空 vector + 通过 out 参数或 log 说明原因
         (如 refs 不足/退化)。
ACCEPT: 1) 每 kind ≥ 6 断言(含"两圆→圆心标记+十字""角点→过角点线""整圆→过整圆十字")；
        2) 退化用例: 半径0圆/重合点/平行无交点 → 返回空且不崩溃；
        3) tests/centerline_test.cpp 注册 CMake target；全量 EXIT=0。
FORBID: 自造交点/圆心算法(必须用 Geometry 原语)；改 ElementType；改既有断言。
ESCALATE: 涉及切点/公切线数值稳定性时停止并上报。
```

#### P2 — 序号球标与明细表联动（Ch2，🆕，最高优先）

```text
[L0][L1: core/model + ui/bom][L2: feature]
GOAL: 新增 BomItem 结构 + 序号球标生成命令 xh(序号)，关联明细表并刷新 BomTree。
      球标=BlockRef(圆/横线样式)+编号文本；点选对象→自动递增 seq→写 BomItem→刷新明细表图元。
CONTEXT(必注入): src/core/SketchModel.h(Quiet API/element 创建)、src/ui/BomTree.h、
                 src/core/sheet_frame.cpp:521(列头)、docs/field-table-formula-design-analysis.md(FIELD 联动机制)。
IO_SPEC: struct BomItem{int seq; QString code,name,material,weight,remark; QVector<qint64> srcIds;};
         存档顶层 "bom":[...] 条件写入(无数据不写键)；读取缺键回落空。
         命令 xh: 选对象→生成球标+追加 BomItem→applyChanges 钩子 refreshFields 刷新明细表。
ACCEPT: 1) ≥10 断言: 生成/隐藏/显示/删除/合并同名/重排编号/悬停取数；
        2) 序列化: 写→读→再写 bom 键往返一致；旧档无 bom 键不崩；
        3) 一次操作一次 pushMemento；tests/bom_test.cpp 注册 target。
FORBID: 改 ElementType 枚举顺序；改既有断言；球标编号不唯一。
ESCALATE: 涉及明细表与标题栏跨图块联动语义时上报。
```

#### P3 — 符号标注扩展：基准/焊接/锥斜度/中心孔/标高（Ch8，🆕）

```text
[L0][L1: core/symbol][L2: feature]
GOAL: 在 SymbolFactory 新增 name 分支: datum(基准符号)/weld(焊接三版)/taper(锥斜度)/
      centerhole(中心孔)/elevation(标高)/rotation(旋转符号)，统一签名
      mkSymbol(const QString& name, const Vec3& anchor, const QVariantMap& params)->std::vector<Element>。
CONTEXT(必注入): src/core/SurfaceRoughness.cpp(mkSymLine 写法)、src/core/Gdt.h(框格绘制)、
                 src/core/ElementFactory.h(BlockRef 构造)。
IO_SPEC: 输出 Line+Text 或 BlockRef；尺寸取 GB 默认(人工提供，AI 不得改数值)。
ACCEPT: 1) 每符号≥5断言(锚点/方向/文字内容/缩放)；2) 退化: 空 anchor→false+log；
        3) tests/symbol_ext_test.cpp 注册。
FORBID: 自造框格排版数学(焊接框格按 GB/T 324/9854 由人工给定布局)；改 ElementType。
ESCALATE: 焊接符号框格排版/基准字母规则涉及国标时停止上报(G1/G3)。
```

#### P4 — 机械零件设计：轴/齿轮/弹簧深化（Ch13，🟡→✅）

```text
[L0][L1: core/views][L2: feature]
GOAL: 把 AircraftViews/FlangeViews 的"参数对话框→Elements"模式抽象为 PartLibrary 接口；
      新增 InvoluteGear::profile(int teeth,double module,...) 渐开线齿廓采样(返回 Polyline 点集)。
CONTEXT(必注入): src/core/FlangeViews.cpp(现有参数化生成范例)、src/core/Geometry.h。
IO_SPEC: profile() 仅做采样生成点集，不负责标注；齿数/模数/压力角为参数。
ACCEPT: 1) 渐开线≥8断言(齿数1/标准20°压力角/对称/节圆处斜率)；2) 退化: teeth<2→false；
        3) tests/gear_test.cpp 注册；全量 EXIT=0。
FORBID: 自造渐开线方程(必须用人工给定的 involute(rb, t) 公式)；改枚举。
ESCALATE: 齿轮参数方程/啮合关系涉及数值正确性时停止(G1)。
```

#### P5 — 参数化国标图库 + 系列化出库（Ch10/11，🆕）

```text
[L0][L1: core/library][L2: feature]
GOAL: 建立 PartLibrary 抽象 + GB 标准件 JSON 规格表驱动出库(checkout(partId,params)->Elements)。
      首批: 六角螺栓/螺母/平垫圈(规格数值由人工提供，放 src/core/GbFastenerTable.h 常量)。
CONTEXT(必注入): src/core/Block.h(BlockDef/BlockRef)、src/core/FlangeViews.cpp(参数化范例)、
                 src/ui/DesignCenterPanel.h(符号库面板可复用)。
IO_SPEC: 出库=实例化 BlockDef(参数化生成几何)+属性；库维护=编辑 JSON 规格。
ACCEPT: 1) 每种标准件≥6断言(规格→关键尺寸/数量)；2) 规格缺项→回落默认+log；
        3) tests/partlib_test.cpp 注册。
FORBID: 改写 GB 规格数值；引入未授权依赖。
ESCALATE: 规格数值表缺失时停止，等待人工提供(G3)。
```

#### P6 — 视图工具：向视符号/孔轴投影/转角投影（Ch6，🆕）

```text
[L0][L1: core/view][L2: feature]
GOAL: 新增向视符号(direction symbol)、孔轴投影(hole/shaft projection)、转角投影(rotation projection)
      生成器，输出符号 BlockRef + 投影辅助线(正交/旋转矩阵复用 Transform 原语)。
CONTEXT(必注入): src/core/SectionCutSymbol.cpp(符号生成范例)、src/core/Geometry.h(Transform)。
IO_SPEC: 辅助线为 Line/Polyline；符号锚点+方向参数。
ACCEPT: 1) 每工具≥5断言(锚点/方向角/字母标识)；2) 退化: 缺方向→false；
        3) tests/viewtool_test.cpp 注册。
FORBID: 自造投影矩阵(用既有 Transform)；改枚举。
```

#### P7 — 辅助工具：截面特性/计算重量/批量 BOM 分析（Ch5，🆕）

```text
[L0][L1: core/inquiry][L2: feature]
GOAL: 实现 SectionProps::compute(const Polygon&)->{area,centroid,inertiaIx,inertiaIy}；
      重量=Σ(面积×DensityTable[material])；批量 BOM 分析=扫描多档头提取 BomItem 汇总。
CONTEXT(必注入): src/core/Geometry.h(多边形面积/形心原语)、src/core/SketchModel.h(读档)。
IO_SPEC: DensityTable 放 src/core/DensityTable.h(人工提供，禁改数值)；
         批量扫描只读档头(图框/标题栏块属性)避免全量解析。
ACCEPT: 1) 截面特性≥8断言(矩形/圆/三角形解析解对照)；2) 退化: 自交多边形→false+log；
        3) tests/sectionprops_test.cpp + tests/bomanalysis_test.cpp 注册。
FORBID: 自造多边形积分(用既有原语或给定公式)；改国标密度数值。
ESCALATE: 截面公式/惯性矩涉及数值正确性时停止(G1)。
```

#### P8 — 参数化设计：变量表达式与驱动（Ch14，🆕）

```text
[L0][L1: core/constraint][L2: feature]
GOAL: 在 ConstraintSolverFacade 上扩变量层: defineVariable(name,expr)、drive(name,val)、
      显示尺寸表达式/附加尺寸/过约束尺寸开关。
CONTEXT(必注入): src/core/ConstraintSolverFacade.h、src/core/ConstraintSolver.h。
IO_SPEC: 变量表达式求值走既有或轻量安全求值器；驱动=改变量→重解约束→刷新关联标注。
ACCEPT: 1) ≥8断言(变量定义/驱动后几何变化/循环依赖检测/过约束标识)；
        2) 退化: 未定义变量/除零→false+log；3) tests/varparam_test.cpp 注册。
FORBID: 改动既有约束求解语义；改枚举。
ESCALATE: 变量层与约束求解耦合导致语义变化时停止(G6)。
```

#### P9 — 扩展工具：机械图拆分/自动排图/批量签名（Ch15，🆕）

```text
[L0][L1: core/batch][L2: feature]
GOAL: 机械图拆分(按图层/图框切分存档)、自动排图(图框矩形装箱输出布局)、批量签名(图框签名栏填字)。
CONTEXT(必注入): src/core/CadFormats.cpp(writeDrawing 分片)、src/core/SheetSpec.h(图框矩形)。
IO_SPEC: 拆分=按选定元素子集调 writeDrawing；排图=矩形 shelf 装箱算法(自写简单算法，G1 非几何无风险)；
         签名=定位标题栏签名单元格填 Text。
ACCEPT: 1) 拆分≥5断言(切分后子档可独立打开)；2) 排图≥5断言(不重叠/利用率)；
        3) tests/splitplot_test.cpp 注册。
FORBID: 改存档格式；改枚举。
```

### 3.4 提示词自检清单（提交前逐项核对）

1. 是否注入了正确的 L1 模块提示词 + 上下文包（头文件/同类实现/同类测试）？
2. GOAL 是否一句话说清且写了 **OUT_OF_SCOPE**？
3. I/O 规范是否给出函数签名 / 存档字段 / 命令语法？
4. 验收是否**可量化**（断言数、退化用例、阈值）？
5. 是否列了 **FORBID（禁改范围）**？
6. 是否设 **ESCALATE**（几何 G1 / 精度 G2 / 合规 G3 / 存档 G4 / 性能 G5 / 撤销 G6）？
7. 高风险步骤是否要求"先给方案/根因，再写代码"？
8. 是否附带自测生成要求（黄金差分 / 属性不变量 / 退化模糊 三类）？
9. 是否说明反馈包格式（多轮任务）？
10. 是否声明轮次上限 4、超限转人工？

---

## 附录 A：参考 CAD 关键命令 ↔ SketchCAD 现状对照（节选自手册附录 A）

| 手册命令                                    | 功能       | SketchCAD 现状              |
| --------------------------------------- | -------- | ------------------------- |
| `XH` / `MX`                             | 序号 / 明细表 | 🟡 BomTree 面板 + 列头；序号球标🆕 |
| `GMPOWERDIM` / `D`                      | 智能标注     | ✅ 智能标注 + 12 类             |
| `CCD`                                   | 粗糙度      | ✅ SurfaceRoughness        |
| `XW`                                    | 形位公差     | ✅ Gdt                     |
| `GMSHAFT` / `GMSPRING` / `GMGEARDESIGN` | 轴/弹簧/齿轮  | 🟡 雏形                     |
| `GMDETAIL`                              | 局部放大     | ✅                         |
| `GMHIDE`                                | 图形消隐     | ✅                         |
| `FC`                                    | 公式曲线     | 🟡 雏形待收敛                  |
| `KZ`                                    | 孔阵       | 🆕                        |
| `BATCHPLOT`                             | 批量打印     | ✅ BatchPlotEngine         |
| `BG`                                    | 自动表格     | ✅ Table + FIELD           |
| `KB`                                    | 孔特征表     | ✅                         |
| `xline`                                 | 构造线      | ✅                         |
| `fillet` / `chamfer`                    | 倒圆/倒角    | ✅                         |
| `mirror` / `rotate`                     | 镜像/旋转    | ✅                         |

## 附录 B：推荐实施优先级（按本手册机械特性重排 V1 的 P0–P3）

- **P0（接图/出图闭环）**：Ch2 序号明细表、Ch3/Ch4 中心线+孔/槽/螺纹、Ch6 视图符号、Ch7/Ch8 标注符号补全、Ch15 批量打印（已建，补拆分/排图）。
- **P1（设计提效）**：Ch10–14 系列化/国标图库/参数化设计、Ch5 辅助报表、Ch9 文字。
- **P2（生态）**：Ch11/12 图库/符号库、Ch16 卡片、Ch17 标准切换。
- **P3（文档）**：Ch18 操作实例。

## 附录 C：国标与术语对照（AI 禁改写数值，须人工提供）

- 图框：GB/T 14689；线宽组：GB/T 14665；剖面：GB/T 4457.5；粗糙度：GB/T 131；
- 形位公差：GB/T 1182；焊接：GB/T 324 / GB/T 9854；螺纹：GB/T 196 / 紧固件 GB/T 5782 等；
- 尺寸注法：GB/T 4458.4；技术产品文件：GB/T 14691（字体）。
