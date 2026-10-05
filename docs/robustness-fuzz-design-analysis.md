# 鲁棒性 / Fuzz 回归设计分析：任何输入都不崩溃 / 卡死 / 写坏文件

> 目标：让程序对任何输入（截断 / 字节翻转 / 非法几何 / 非法结构 / 非法编码 / 越界类型键）都「不崩溃 > 报错明确 > 部分降级 > 完全拒绝」，且打开失败不污染当前图纸、保存前先在内存构造完整数据再落盘。
> 涉及：`src/core/ElementValidate.{h,cpp}`（新增守卫层）、`src/core/Element.cpp`（`elementFromJson` 维持零改动）、`src/ui/MainWindow.cpp`（`readDrawingImpl` 三路接线）、`src/com/BatchPlotEngine.cpp`（`loadDrawing` 无头接线）、`tests/SketchSelfTest.cpp`（`t_robustness` / `t_fileFuzz`）、`tools/gen_corrupt_samples.py`（确定性损坏样本生成器）。
> 日期：2026-10-04

---

## 1. 问题定义

探查阶段已确认：除 `Element::elementFromJson` 外，所有解析层均**已崩溃安全**：

| 解析层 | 位置 | 结论 |
|---|---|---|
| `tokenizeDxf` | `CadFormats.cpp` | 按行 split，无法解析组码即 `++i; continue;`，有界循环 |
| `binaryDxfToAscii` | `CadFormats.cpp` | 先校验 22 字节哨兵头，各类型按 `N` 边界严格判定越界返回 false |
| `fromDwg` | `CadFormats.cpp` | 校验 AC1009 头 + 实体段指针范围，循环 `i < (int)pEntEnd` 有界 |
| `parseDxfTokens` | `CadFormats.cpp` | 按 `tok.size()` 有界遍历，逐对象 `while (j < tok.size() && tok[j].code != 0)` |
| `Block::evalDef` | `Block.cpp` | 已有 `kMaxBlockDepth=16` 深度限 + `visiting` 环检测，块自引用 / Xref 环路 / 超深嵌套天然防御 |

唯一真实缺口是 **`elementFromJson`（`Element.cpp:222`）做零校验**：直接 `o["type"].toInt()` 强转枚举、无 NaN/Inf 检查、无几何有效性检查、无超长串 / 控制字符检查、无 id 越界检查。损坏的 JSON 图元一旦进入模型，下游包围盒 / 渲染 / 导出会在某处把 NaN/Inf 放大成崩溃或「画错」。

因此本次改造面收敛为：**在 `elementFromJson` 之上新增一个只读守卫 / helper 层，不改动正常解析结果、不触碰几何算法、不动既有序列化**。

---

## 2. 处理策略优先级（任务硬约束）

```
不崩溃  >  报错明确  >  部分降级  >  完全拒绝
```

- **不崩溃**：任何输入都走有界路径，绝不在解析 / 守卫 / 渲染期抛异常或死循环。
- **报错明确**：丢弃 / 修复必须在日志（`model_->log` / `appendLog`）写清丢弃了几个、修复了几个、原因是什么。
- **部分降级**：能救的（如闭合折线首尾重复点、文本控制字符）就地裁剪修复，宁可少画绝不画错。
- **完全拒绝**：解析层整体失败（JSON 非对象 / DXF 头不被支持）直接 return，连图元都不进模型。

对「退化几何」（零半径圆、零长度直线、NaN 坐标）采取 **丢弃而非静默修复**——因为「改一个数让它看起来正常」会违背「绝不画错」原则，属于更危险的隐性错误。

---

## 3. 各鲁棒性向量 → 处理策略映射

| 鲁棒性向量 | 处理策略 | 守卫动作 | 是否进模型 |
|---|---|---|---|
| 截断 1% / 10% / 50%（JSON） | 完全拒绝 | `QJsonDocument::fromJson` 失败 → `jsonOk=false` → return（不 clear） | 否（原图完好） |
| 截断（DXF / DWG） | 完全拒绝 | `fromDxf/fromDwg` 有界解析，头 / 指针校验失败 → return | 否 |
| 随机字节翻转 ≤20 处 | 完全拒绝（解析层）/ 部分降级（图元层） | 解析层有界吃下噪声；图元层对漏网的非法图元做丢弃 | 视情况 |
| 非法几何：NaN / Inf | 部分降级（丢弃） | `coordBad(v)` 判定，命中即跳过 | 否 |
| 非法几何：坐标 / 半径 \|v\|>1e18 | 部分降级（丢弃） | 同上；边界值 1e18 保留 | 否（越界）/ 是（边界） |
| 非法几何：零半径圆 / 圆弧、零长度直线、零扫掠弧、零半轴椭圆 | 部分降级（丢弃） | 几何退化判定，跳过 | 否 |
| 非法几何：闭合折线首尾重复点 | 部分降级（修复） | 去重末点、`closed` 保持，落入 `fix` 计数 | 是（已修复） |
| 非法结构：块递归自引用 / Xref 环路 / 超深嵌套 | 不崩溃（既有） | `Block::evalDef` `kMaxBlockDepth=16` + `visiting` 守卫 | 是（展开时静默截断） |
| 非法结构：引用不存在层 / 块 / 样式 | 部分降级（报告） | `countDanglingRefs` 统计并日志告知，不丢弃 | 是（引用静默回落默认） |
| 非法编码：非法 UTF-8 | 不崩溃（既有） | `QString::fromUtf8` 默认替换 | 是 |
| 非法编码：超长串 >1MB | 部分降级（丢弃） | `kMaxTextLen=1<<20` 判定，跳过 | 否 |
| 非法编码：控制字符 | 部分降级（修复） | 剥离控制字符（保留 `\t\n\r`），落入 `fix` 计数 | 是（已清洗） |
| 缺失 / 非法类型键：ElementType 越界 | 部分降级（丢弃） | `(int)type ∈ [0,19]` 判定，越界跳过 | 否 |
| 负数 id / 重复 id | 不崩溃（既有） | `addElementsQuiet` 始终 `nextId_++` 重分配，不会碰撞 | 是 |

> 关键不变量：**id 由模型在载入时重分配（`SketchModel::addElementsQuiet` 的 `ne.id = nextId_++`），文件里的负数 / 重复 id 不会在模型内碰撞**，因此该向量不触发丢弃，仅作为「引用悬空」在 `countDanglingRefs` 里统计（若 refA/refB 指向了不存在的图元）。

---

## 4. 守卫层 API（`src/core/ElementValidate.{h,cpp}`）

新增代码**全部集中在此层**，核心层其余文件零改动（除读档路径的接线调用）。

```cpp
// 单个图元只读校验：正常图元恒返回 true；why 在返回 false 时给出中文原因。
bool isElementValid(const Element& e, QString* why = nullptr);

// 批量守卫：返回保留图元向量。
//   skipCount = 因损坏被丢弃的图元数；
//   fixCount  = 被裁剪修复（去重复末点 / 剥离控制字符）的图元数；
//   report    = 人类可读的中文降级摘要（每条降级一行，便于在打开日志中如实告知）。
// 正常文件：返回值与输入逐元素值等价（skip=fix=0，未做任何修改）。
std::vector<Element> filterElements(const std::vector<Element>& in,
                                   int* skipCount, int* fixCount, QString* report);

// 载入各表后统计「引用不存在」的图元（按名引用层/块/样式；按 id 引用 refA/refB/hatchSrcIds）。
// 不丢弃任何图元，仅统计并在 report 写明，便于在打开日志中如实告知用户降级。
int countDanglingRefs(const std::vector<Element>& elems,
                     const std::vector<QString>& layerNames,
                     const std::vector<QString>& blockNames,
                     const std::vector<QString>& styleNames,
                     QString* report);
```

设计铁律：
- **正常路径逐字节（值）等价**：`filterElements` 对合法图元只做 `push_back` 原样拷贝，不修改任何字段；`skip=fix=0`。既有黄金样本 round-trip 断言（R12/R2000 DXF + DWG）不受影响。
- **只读、零几何**：全部判定基于字段值的范围 / 有限性 / 退化性，不调用任何几何算法、不重算形状。
- **修复即裁剪**：`fix` 只做两种无损裁剪——去重复末点、剥离控制字符；不做任何「改数值救几何」的危险操作。

---

## 5. 读档路径接线

### 5.1 交互读档 `MainWindow::readDrawingImpl`
三路（JSON / DXF / DWG）在 `addElementsQuiet` 之前插入同一段守卫：

```cpp
int robSkip = 0, robFix = 0; QString robRep;
std::vector<Element> kept = SketchCAD::filterElements(loaded_or_imported, &robSkip, &robFix, &robRep);
if (robSkip > 0 || robFix > 0)
    model_->log(QStringLiteral("打开校验：%1 个损坏图元已跳过，%2 个已修复。%3")
                   .arg(robSkip).arg(robFix).arg(robRep), "warn");
model_->addElementsQuiet(kept);
```

- **JSON 路**：过滤 `loaded` → `kept`；各表（层 / 块 / 文字样式 / 标注样式 / 过滤器 / 布局）载入后，用 `model_` 的表名集合调用 `countDanglingRefs(kept, ...)`，把「引用不存在图层 / 块 / 样式或悬空几何」如实数到日志（降级不丢弃）。
- **DXF / DWG 路**：过滤 `imported` → `kept`；包围盒计算与计数均改用 `kept`（不再引用被丢弃的图元）。

### 5.2 无头读档 `BatchPlotEngine::loadDrawing`
与交互读档同语义，失败经 `appendLog` 落到自动化日志：

```cpp
int robSkip = 0, robFix = 0; QString robRep;
std::vector<SketchCAD::Element> kept = SketchCAD::filterElements(imported_or_loaded, &robSkip, &robFix, &robRep);
if (robSkip > 0 || robFix > 0)
    appendLog(QString(), QStringLiteral("[robust] ...%1 个损坏图元已跳过，%2 个已修复。%3")...);
model.addElementsQuiet(kept);
```

### 5.3 既有不变量（本次未改、复核确认）
- **打开失败不污染当前图纸**：JSON 路在 `jsonOk=false` 时直接 return（不清空模型）；DXF/DWG 路 `parseOk==false` 直接 return；`BatchPlotEngine` 在 `!d.isObject()` 时 return（**在 `model.clear()` 之前**）。
- **保存前先在内存构造完整数据再落盘**：`MainWindow::writeDrawing` 先 `out = model_->toDrawingBytes();` 再 `f.write(out)`，已被既有实现满足。

---

## 6. 降级路径与日志断言（每种降级一条）

`t_robustness` 对每条降级路径都断言「被丢弃 / 被修复 **且** report 写明原因」：

| 降级路径 | 断言要点 |
|---|---|
| 类型越界（99）/ 负数类型（-1） | `s==2`、`report.contains("类型越界")` |
| NaN / Inf 坐标与半径 | `s==3`、`report.contains("NaN")` |
| 坐标 >1e18 | `s==1` 丢弃；边界值 1e18 仍保留 |
| 退化几何（零半径 / 零长度 / 零扫掠 / 零半轴） | `s==4`、`report` 含「零半径 / 零长度 / 零扫掠」之一 |
| 闭合折线首尾重复点 | `f==1` 修复、`out[0].pts.size()==4`、`closed` 仍为 true、`report.contains("修复")` |
| 超长文本 >1MB | `s==1`、`report.contains("超长")` |
| 控制字符 | `f==1` 修复、清洗后保留 `\n`、去除 `\x01` |
| 合法多行文本 | `f==0` 不误修 |
| 空输入 | 返回空、不崩溃 |
| 负数 / 重复 file-id | `s==0`（模型重分配 id） |
| `countDanglingRefs`：缺失层 / 块 / 样式 / 悬空 ref | `n==4`、`report` 覆盖「图层 / 块 / 样式 / 悬空」 |
| 打开失败不污染 | 非法 JSON 返回 false、模型图元数不变 |

---

## 7. 测试与验收

### 7.1 新增断言（`tests/SketchSelfTest.cpp`）
- **`t_robustness`**：~38 条 CHECK，覆盖 §3 全部向量 —— 每向量含「返回明确错误 + 不崩溃 + 图纸未污染（正常文件值等价）」。
- **`t_fileFuzz`**：固定种子 `0x5EED1234`、`N=200` 随机损坏样本，覆盖 JSON / DXF / DWG / 二进制 DXF 的截断（1% / 10% / 50%）与随机字节翻转（≤20 处）与空文件。每个样本在独立线程中跑「解析 + 守卫」，要求：
  - **零崩溃**（catch 任意异常记为崩溃）；
  - **零死锁 / 超时**（单样本 `wait_for(5s)` 内必须完成；解析层有界，无无限循环）；
  - 超时线程的栈变量以值捕获 + `shared_ptr` 保活，避免 use-after-free。

### 7.2 确定性损坏样本生成器（`tools/gen_corrupt_samples.py`）
- 固定种子，同输入同输出，CI 可复跑。
- DXF / DWG 基准取自已签入的 `tests/golden/golden_r12.{dxf,dwg}`（保证「损坏前」本身合法）；JSON 基准由脚本硬编码确定性生成。
- 输出 `tests/golden/corrupt/`：截断 / 翻转 / 类型越界 / 零半径 / 控制字符 / type 键为字符串 等共 32 个样本。

### 7.3 验收结果
- 自测全量：**通过 932 / 失败 0**（基线 890 + 新增 ~42 条）。
- 构建：`cmake --build build --target SketchCAD SketchSelfTest -j 1` —— **EXIT=0、双目标零警告**（沿用 `-j 1` 规避 OOM 陷阱）。
- 新增代码全部集中在 `ElementValidate.{h,cpp}`，核心层其余文件仅做接线调用、序列化零改动，既有黄金样本 round-trip 断言不受影响。

---

## 8. 已知限制与后续

- **`countDanglingRefs` 的 id 维度仅在 JSON 路调用**：DXF / DWG 的层 / 块名不注册进 `model_`（DXF 导入不建图层表），若在此路调用会以「默认层 0」为基准产生误报，故刻意只在 JSON 路统计并按名引用。块 / 样式按名引用的缺失在 DXF/DWG 路暂不统计（解析层已对未知块 / 样式做静默回落）。
- **降级是「丢弃」而非「修复几何」**：零半径 / 零长度等退化几何被直接丢弃而非补一个近似形状，宁可少画。若未来需要「尽量保留」，可在此层增加带日志的显式修复分支，但仍须遵守「绝不画错」。
- **保存落盘已有内存构造保护**，但「磁盘满 / 权限不足」等写失败由既有 `writeDrawing` 的返回值与日志处理，不在本次范围。
- 后续可将 `tests/golden/corrupt/` 接入 CI「损坏样本回放」作业，配合 `t_fileFuzz` 形成长期回归护栏。
