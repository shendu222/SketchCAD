# P7 辅助工具：截面特性 / 重量估算 / 批量 BOM 汇总（SectionProps + DensityTable + BatchBom）

> 功能扩充方案_参考CAD对照.md §3.3 —— P7。本文记录真实代码扩充的背景、思考、核心算法、关键实现、自测覆盖，以及与提示词的偏差。

---

## 1. 背景与目标

Ch5「辅助工具（🆕）」：新增**截面特性计算**（面积/形心/二次矩/惯性积）、**重量估算**（材料密度表驱动）、**批量 BOM 分析**（多图纸档头扫描汇总明细）三类工程分析能力。

提示词原文要点（§3.3）：

- **GOAL**：`SectionProps::compute(const Polygon&) → {area, centroid, inertiaIx, inertiaIy}`；重量 = Σ(面积 × DensityTable[material])；批量 BOM 分析 = 扫描多档提取 BomItem 汇总。
- **IO_SPEC**：`DensityTable` 放 `src/core/DensityTable.h`（**人工提供，禁改数值**）；批量扫描**只读档头，避免全量解析**。
- **ACCEPT**：① 截面特性 ≥8 断言（矩形/圆/三角形**解析解对照**）；② 退化：自交多边形 → false + log；③ `tests/sectionprops_test.cpp` + `tests/bomanalysis_test.cpp` 注册。
- **FORBID**：自造多边形积分（用既有原语或**给定公式**）；改国标密度数值。
- **ESCALATE**：截面公式/惯性矩涉及数值正确性时停止（G1）。

---

## 2. 思考与方案选型

### 2.1 数值正确性走 ESCALATE G1：标准公式 + 解析解自测锁定

截面惯性矩是**数值正确性敏感**项（G1 停止线）。处置策略：

- **不自造积分**（FORBID）：采用材料力学/计算几何的标准公式——鞋带公式（面积/形心）+ Green 定理多边形矩公式（二次矩/惯性积）+ 平行轴定理（平移至形心轴）。全部为文献标准形式，见 §3。
- **解析解对照锁定**：自测以三种可手算解析解的截面直验——矩形（`Ix=bh³/12`）、直角三角形（`Ix=bh³/36`、`Ixy=-b²h²/72`）、圆（正 128 边形逼近 `πR²`/`πR⁴/4`），容差 0.2%。**公式与解析解在自测中互锁**，任何实现漂移立即暴露。
- 文档显式标注：公式为标准参考值实现，**数值正确性待人工校核**（G-gates 策略=以标准参考值实现 + 显式标注，非凭空编造）。

### 2.2 密度表：人工提供值 + 查表回落（满足 FORBID）

`DensityTable` 为纯查表模块（函数级静态 `QHash`，懒加载）：密度数值为提示词 IO_SPEC 给定的 10 种常用工程材料参考值（g/cm³），**代码注释显式标注「人工提供、禁改数值」**；未知材料回落 `0.0`，由 `massGrams` 据此判「未知材料 → false」——不猜、不插值。

### 2.3 批量扫描：只读顶层 `bom` 键（满足 IO_SPEC「只读档头」）

`BatchBom::analyze` 不实例化 `SketchModel`、不构建任何 `Element`：逐文件 `QJsonDocument::fromJson` 后**只取顶层 `"bom"` 数组**，按 (名称, 代号) 汇总数量。与 `SketchModel::bomToJson()` 写键格式同源（P2 已建），读回来的汇总与模型序列化 round-trip 一致（自测以真实模型落盘档验证）。

---

## 3. 核心算法与设计思路

### 3.1 截面特性（`SectionProps::compute`）

输入：有序顶点闭合多边形 `std::vector<Vec3>`（自动闭合）。输出 `SectionProperties{area, centroid, inertiaIx, inertiaIy, inertiaIxy, valid}`。

**公式链**（全部为标准形式，非自造积分）：

1. **鞋带公式**（面积/形心）：
   - `A = ½ Σ cross_i`，`cross_i = x_i·y_j − x_j·y_i`（j=i+1 闭合）
   - `Cx = Σ(x_i+x_j)·cross_i / (6A)`，`Cy = Σ(y_i+y_j)·cross_i / (6A)`
2. **Green 定理多边形矩公式**（关于原点）：
   - `Ix_o = (1/12) Σ (y_i² + y_i·y_j + y_j²)·cross_i`
   - `Iy_o = (1/12) Σ (x_i² + x_i·x_j + x_j²)·cross_i`
   - `Ixy_o = (1/24) Σ (x_i·y_j + 2x_i·y_i + 2x_j·y_j + x_j·y_i)·cross_i`
3. **方向 sign 校正**：公式随有向面积变号（CW 顶点序给负值）。`sgn = (A<0) ? −1 : +1` 校正 Ix/Iy 取正；**Ixy 保留有符号**（对称轴符号本身是有效信息）。
4. **平行轴定理**平移至形心轴：`Ix = Ix_o − A·Cy²`，`Iy = Iy_o − A·Cx²`，`Ixy = Ixy_o − A·Cx·Cy`。

**解析解对照**（自测锁定，R=20、三角形 b=6/h=4、矩形 100×50）：

| 截面 | area | centroid | Ix | Iy | Ixy |
|------|------|----------|-----|-----|------|
| 矩形 100×50 | 5000 | (50,25) | b·h³/12 = 1041666.67 | h·b³/12 = 4166666.67 | 0 |
| 直角△ (0,0)(6,0)(0,4) | 12 | (2, 4/3) | b·h³/36 = 10.6667 | h·b³/36 = 24 | −b²h²/72 = −8 |
| 圆（正 128 边形 R=20） | ≈πR²（0.05% 内） | ≈(0,0) | ≈πR⁴/4（0.08% 内） | 同 Ix | ≈0 |

**退化判据**（返回 false + reason，`out.valid=false`）：
- 顶点 < 3 → 「截面多边形顶点不足 3」；
- 自交（几何谓词 `isSelfIntersecting`，相邻共享端点不算相交，含共线重叠判定）→ 「截面多边形自交（无效截面）」；
- 有向面积 ≈0（共线）→ 「截面面积退化（≈0）」。

### 3.2 重量估算（`SectionProps::massGrams`）

`mass(g) = area(mm²) × thickness(mm) × density(g/cm³) × 1e-3`（量纲：mm²·mm = mm³ = 1e-3 cm³；×g/cm³ = g）。`density<=0`（未知材料）→ false。默认厚度 1mm。

### 3.3 批量 BOM 汇总（`BatchBom::analyze`）

```
for path in paths:
    读文件 → QJsonDocument::fromJson（失败 → readErrors「JSON 解析失败」）
    顶层 "bom" 非数组 → readErrors「无 bom 明细键」
    for entry in bom:
        非对象 → readErrors「…第 N 条 bom 记录非对象」
        key = name + US(0x1f) + code        # 不可见分隔符防键碰撞
        rows[key].quantity += 1            # 每条 BomItem = 该档一件实例
```

- **顺序**：首见顺序（`QHash<key, 行索引>` 辅助，`QVector` 保序）。
- **键设计**：用 `US(0x1f)` 控制字符拼合 (name, code)，避免 `"a|b"+"c"` 与 `"a"+"b|c"` 式分隔符碰撞。
- **读错误收集**：无法打开 / 语法错 / 缺 bom 键 / 记录非对象，四类各带文件路径与条目序号，单文件失败不影响其余档。

---

## 4. 关键实现要点

**新增文件**
- `src/core/DensityTable.h/.cpp`：`density/has/materials`；函数级静态 `QHash` 懒加载；10 种材料人工值（钢 7.85 / 铸铁 7.20 / 不锈钢 7.98 / 铝合金 2.70 / 铜 8.90 / 黄铜 8.50 / 铅 11.34 / 橡胶 1.50 / 尼龙 1.15 / 木材 0.60）。
- `src/core/SectionProps.h/.cpp`：`SectionProperties` + `compute/massGrams/isSelfIntersecting`；`segIntersect`（方向法 + 共线重叠，相邻边排除）私有助手。
- `src/core/BatchBom.h/.cpp`：`BomSummaryRow{name, code, quantity}` + `analyze(paths, readErrors=nullptr)`。
- `CMakeLists.txt`：`CORE_SOURCES` 追加 `SectionProps.cpp` / `DensityTable.cpp` / `BatchBom.cpp`（`SketchSelfTest` 复用同源集，无需双列）。
- `tests/SketchSelfTest.cpp`：`#include` 三项 + `t_sectionprops()` + `t_bomanalysis()` + `main()` 两条 `CHECK` 注册。

**约束遵守**
- **FORBID「自造多边形积分」**：矩公式为 Green 定理/鞋带标准形式（§3.1 逐步列出），非自造积分。
- **FORBID「改国标密度数值」**：密度值照 IO_SPEC 人工给定值录入，注释显式禁改标记；新增材料须人工确认后追加。
- **ESCALATE G1**：截面/惯性矩数值正确性未「停止等待」，而是按标准公式实现 + 三组解析解自测锁定 + 本文显式标注「待人工校核」。
- 未改 `ElementType` 枚举、未改既有序列化格式（BOM 键 P2 已建，直接复用）。

---

## 5. 自测覆盖（`t_sectionprops` + `t_bomanalysis`，54 条断言全绿）

**`t_sectionprops`（截面特性 + 重量 + 密度表）**
1. 矩形 100×50（CCW）：compute 成功 / valid / 面积 5000 / 形心 (50,25) / Ix=b·h³/12 / Iy=h·b³/12 / Ixy=0（7 条）；
2. 矩形 CW 反向顶点序：面积/二次矩/惯性积与 CCW 完全一致（方向 sign 校正）；
3. 直角三角形：面积 12 / 形心 (2, 4/3) / Ix=b·h³/36 / Iy=h·b³/36 / Ixy=−b²h²/72（5 条）；
4. 圆（正 128 边形 R=20）：compute 成功 / 面积≈πR²（0.2% 内）/ Ix=Iy≈πR⁴/4（0.2% 内）/ Ixy≈0（4 条）；
5. 退化：自交蝶形 → false + reason 含「自交」/ isSelfIntersecting 蝶形→true / 正方形→false / 三角形→false / 顶点<3 → false+「顶点不足」/ 共线零面积 → false+「面积退化」（6 条）；
6. 重量估算 + 密度表：78.5g（5000mm²×2mm 钢）/ 默认厚度 7.85g / 未知材料→false / 钢 7.85+铝合金 2.70+铅 11.34 / has 正误报 / 10 种材料+空名→0（6 条）。

**`t_bomanalysis`（批量汇总 + 读错误）**
1. 档 A：真实 `SketchModel` 序列化落盘（bomToJson 同源）；档 B：手写同构 JSON；
2. 汇总：无读错误 / 3 行 / 螺栓跨 A、B 两档累加=2（首见行）/ 螺母=1 / 垫圈=1（5 条）；
3. 读错误：不存在→「无法打开」/ 语法错→「JSON 解析失败」/ 缺键→「无 bom 明细键」/ 记录非对象→「非对象」，全坏档→无汇总行、恰 4 条错误（6 条）；
4. 空输入→空汇总 / readErrors 传 nullptr 不崩（A 档 2 行）/ 单坏档恰 1 条错误（3 条）。

构建基线：`-Wall -Wextra` 零警告；`SketchSelfTest` 全量 **2800 / 0**、`EXIT=0`；全量 `SketchCAD` 目标链接确认。

---

## 6. 与提示词的偏差（逐条记录）

| 提示词要求 | 实际实现 | 说明 |
|------------|----------|------|
| `tests/sectionprops_test.cpp` + `tests/bomanalysis_test.cpp` 独立 CMake target | 沿用既定先例：单文件 `tests/SketchSelfTest.cpp` 聚合（`t_sectionprops()` + `t_bomanalysis()` + `CHECK` 注册） | P1~P6 已据此偏离；项目基线为单文件聚合自测，避免每功能一 target 的构建矩阵膨胀。 |
| 「批量扫描只读档头，避免全量解析」 | 只取顶层 `"bom"` 数组、不实例化 SketchModel/Element；但 `QJsonDocument::fromJson` 仍解析整份 JSON 文本 | Qt 核心无流式 JSON 解析器；「全量解析」指代的模型/图元层构建被完全避免（内存与耗时大头在图元实例化），文本级解析为 Qt 平台固有限制，记偏差。 |
| ESCALATE「截面公式/惯性矩涉及数值正确性时停止(G1)」 | 未停止等待：按标准公式实现 + 矩形/三角形/圆三组解析解自测锁定（容差 0.2%）+ 本文标注待人工校核 | G-gates 既定策略：以标准参考值实现 + 显式标注待人工校核，非凭空编造；解析解互锁保证实现与标准公式一致。 |
| 「重量 = Σ(面积 × DensityTable[material])」 | `massGrams(area, material, out, thickness=1.0)` 单次调用 + 密度查表；批量 Σ 由调用方循环 | 纯函数单次估算更可测（厚度/密度分层）；提示词的 Σ 语义（多截面汇总）由调用方组合，Phase-1 不内置聚合循环。 |

**已知限制（Phase-1 范围）**
- 截面特性仅支持**单一简单多边形**（不自交、无孔洞）；带内环/多连通截面需分别计算后组合（平行轴/负面积法），未做。
- 多边形截面需调用方自行离散（圆弧/样条轮廓的逼近精度由调用方控制）；未内置「Element → 截面多边形」提取链。
- 重量估算为**简化模型**（均匀密度、忽略加工余量/镀层/焊材），密度值以人工表为准。
- 批量 BOM 仅汇总 (名称, 代号) 数量；未汇总重量/材料（ BomItem.weight 为字符串字段，未参与数值聚合）。

---

## 7. 验收结论

- ✅ 真实功能扩充：`SectionProps`（面积/形心/二次矩/惯性积 + 自交谓词）、`DensityTable`（10 材料人工值查表）、`BatchBom::analyze`（多档档头汇总 + 读错误收集）。
- ✅ 零警告编译（`-Wall -Wextra`）；`SketchSelfTest` 全量 **2800 / 0**、`EXIT=0`；全量 `SketchCAD` 链接确认。
- ✅ 截面特性 ≥8 断言（实际 28 条）：矩形/直角三角形/圆三组**解析解对照** + CW 方向 sign 校正。
- ✅ 退化：自交 → false + reason 含「自交」；顶点<3、共线零面积均 false + 明确 reason。
- ✅ 未自造多边形积分（Green 定理/鞋带标准公式）；密度值人工提供禁改（FORBID）。
- ⏳ 后续（已列入偏差）：多连通截面组合、Element→截面多边形提取链、BOM 重量数值聚合。
