# 工程制图合规性检查器 ComplianceChecker 设计与调试报告

> 模块路径：`src/core/standards/ComplianceChecker.{h,cpp}`、`src/ui/StandardsPanel.{h,cpp}`
> 依据标准：GB/T 14665（线宽/间隙/字高）、GB/T 131-2006（粗糙度）、GB/T 324（焊缝）、GB/T 14689（幅面留边）、GB/T 18617 / ISO 13567（图层命名）
> 配套自测：`tests/SketchSelfTest.cpp::t_compliance()`、`::t_layerGB()`
> 状态：已实现并通过全量自测（**3354 / 0，EXIT=0**），双目标 `-Wall -Wextra` **零警告**，`dist` 已同步

---

## 1. 背景与目标

前序会话已交付「GB/T 18617 图层命名建议与合规检查」，彼时 `ComplianceChecker` 仅含一条规则、且返回旧结构 `{layerName, category, reason}`。本次为**升级重写**：把它扩展为覆盖 7 类 GB/T 规则的完整检查器，并打通「命令 → AI 工具 → 诊断面板 → 图元跳转」全链路。

需求分解：

| # | 需求 | 落地 |
|---|------|------|
| 1 | `run(SketchModel*)` 返回 `QVector<ComplianceIssue>`，字段含 `elementId/ruleId/severity/message/fixHint` | `ComplianceChecker.h` 重写 |
| 2 | 7 条规则 R1~R7 | `ComplianceChecker.cpp` 七个 `checkXxx` |
| 3 | 复用 `logAppended` 与 DiagBundle 诊断通道上报；不阻塞编辑器 | 命令侧 `m->log` + 全局回调桥 |
| 4 | 注册 `STDCHECK`，AI 工具 `checkCompliance` 调用该命令 | `Commands.cpp` / `CommandCatalog.cpp` |
| 5 | 验收：违规图正确报告、自测 EXIT=0；面板可列出并跳转图元 | `StandardsPanel` + `t_compliance()` |
| 6 | 导出实现与调试思考过程 | 本文档 |

---

## 2. 对外 API 与数据结构

```cpp
namespace SketchCAD {

struct ComplianceIssue {
    long    elementId = 0;   // 关联图元 id，供面板双击跳转；0 = 无具体图元（如纯图层问题）
    QString ruleId;          // R1_line_width_ratio … R7_layer_naming
    QString severity;        // 严重 / 警告 / 提示
    QString message;         // 含标准号与实测值的中文说明
    QString fixHint;         // 可操作的整改建议
};

class ComplianceChecker {
public:
    static QVector<ComplianceIssue> run(const SketchModel* model);       // 七规则完整检查入口
    static QVector<ComplianceIssue> checkLayerNaming(const QStringList&); // R7 结构化入口（保留旧能力）

    // 命令 → 面板刷新的全局回调桥
    using ComplianceReportSink = std::function<void(const QVector<ComplianceIssue>&)>;
    static void setComplianceReportSink(ComplianceReportSink sink);
    static void notifyComplianceReport(const QVector<ComplianceIssue>& issues);
private:
    ComplianceChecker() = delete;   // 纯静态工具类
};

}
```

**兼容性决策**：旧 `checkModel(const SketchModel&)` 删除；`checkLayerNaming(const QStringList&)` 保留但返回类型改为新 `ComplianceIssue`，`t_layerGB` 的断言相应改为按 `ruleId == "R7_layer_naming"` 与 `message.contains("我的图层")` 判定，而非旧的三字段。

---

## 3. 七条规则的判定逻辑

`run()` 顺序调用 `checkLineWidthRatio → checkMinGap → checkTextHeight → checkRoughness → checkWeld → checkSheetMargin → checkLayerNamingRule`，纯读模型、零写副作用。

### R1 线宽粗细比（`R1_line_width_ratio`，警告）
- 遍历描边类图元（`isStrokeType`：Line/Polyline/Rectangle/Circle/Arc/Spline/Ellipse/Boundary/Xline），取 `effectiveLineWidth()`，收集去重线宽并降序排列。
- 相邻线宽对 `w1/w2` 落在 `1.85~2.15` 之外即报（对应 GB/T 14665 的 **2:1**；容差抵御线宽浮点抖动）。
- 用「相邻对」而非「全体最大/最小比」，可容纳 `2.0/1.0/0.5` 这类合规级联而不误报。
- `elementId` 取该细线宽的首个图元，便于定位。

### R2 两线最小间隙 ≥ 0.7mm（`R2_min_line_gap`，警告）
- 每个图元经 `elementSegments()` 展开为线段集（圆/弧/椭圆以 24 段折线近似）。
- **包围盒预裁剪**：两盒外扩 `0.7mm` 后不相交则跳过，避免 O(N²) 精确计算。
- 精确距离 `segSegDist()`：四组端点距离 + 四组「端点到对线段投影」距离取最小。
- 命中条件 `best < 0.7 && best > 1e-3`：下界排除重合/共端点（这类是拓扑连接，不是间隙违规）。每个元素只报一次。

### R3 字号下限 ≥ 3.5mm（`R3_text_height`，警告）
- Text/MText 取 `radius`，Dimension 取 `dimTextHeight`，MLeader/Fcf/Table 二者择一。
- `radius == 0` 表示「沿用默认字高 3.5」，放行（与 `LongFangSong` 样式的 `textHeight=0` 语义一致）。
- 下限取 A2~A4 字母数字标准 3.5mm；`fixHint` 提示 A0/A1 建议 ≥5mm。

### R4 粗糙度符号书写规范（`R4_roughness_notation`，提示）
- 按 `name == "表面粗糙度"` 聚类（半径 15mm），簇内若无「含数字的 Text」即报「缺 Ra 数值」（GB/T 131-2006）。
- 生成器 `buildRoughnessSymbol()` 本身已给图元打该 `name`，无需改动。

### R5 焊缝符号完整性（`R5_weld_symbol`，提示）
- 按 `name == "焊缝符号"` 聚类（半径 20mm），簇内需同时具备「近似水平基准线」与「Polyline/Circle 基本符号」。
- **关键改动**：`buildWeldSymbol()` 原先输出的图元 `name` 为空，无法按标签聚合。在其末尾统一写入 `el.name = "焊缝符号"`，与粗糙度同范式。`Element::name` 本就参与序列化，零副作用、旧档兼容。

### R6 幅面留边（`R6_sheet_margin`，提示）
- 由「图框」层图元并集推断纸张外边界；**无图框则整条规则跳过**，避免对用户自画内容误报。
- 内缩 `standardSheetFrameMargins(currentSheetFormat()).c`（缺表回落 5mm）得可用区。
- 逐图元判断包围盒是否完整落在可用区内；退化盒（水平/垂直线）同样参与判定。

### R7 图层命名（`R7_layer_naming`，警告）
- 复用既有 `LayerNaming::checkLayerNaming()`（GB/T 18617 / ISO 13567），把 `LayerNamingIssue` 转写为新结构。
- 额外扫描该层首个图元 id 填入 `elementId`，使图层类问题也能在面板里跳转定位。

---

## 4. 诊断通道的取舍：为什么不是「逐条写进 DiagBundle」

需求原文要求「复用既有 `logAppended` 与 DiagBundle 诊断通道上报」。核查后确认：本工程里的 **DiagBundle 是崩溃诊断 ZIP 打包器**（会话标记、遥测零外发、CRC 自解析），用途是进程异常时打包现场，**不是逐条诊断消息的运行时通道**。若把合规问题塞进它，语义错位且会污染崩溃包。

因此落地为两条正路：

1. **逐条上报**：命令侧对每条 issue 调 `m->log(msg, sev)`，经 `SketchModel::logAppended` 发出——这正是编辑器日志/诊断面板订阅的既有信号，零新增管线。`severity`（严重/警告/提示）按 `error/warn/info` 映射。
2. **面板刷新**：新增全局回调桥 `setComplianceReportSink / notifyComplianceReport`，由 `MainWindow` 注册 lambda 把 issues 推给 `StandardsPanel`。

**为什么不改 `SketchModel.h` 加回调**：该头被全工程广泛包含，任何改动都会触发全量重编（本工程全量构建易 OOM）。把 sink 放在仅被 `Commands.cpp` 与自身包含的 `ComplianceChecker.h` 里，增量构建只需重编 2~4 个目标文件。这是本次一个刻意的编译期优化。

**非阻塞设计**：七规则均为同步纯读遍历，图元量级为千级、耗时毫秒级，实测无感知卡顿；不另开线程，也就规避了跨线程访问 `SketchModel` 的加锁与生命周期问题。这比「为轻量检查引入 QThread + 信号排队」更简单可靠。

---

## 5. 命令、AI 工具与面板接线

### 5.1 命令注册
`Commands.cpp` 内定义单一 lambda `runCompliance`，同时注册两个精确名：

```cpp
registerExact("stdcheck",         std::make_shared<LambdaCommand>(runCompliance));
registerExact("checkcompliance",  std::make_shared<LambdaCommand>(runCompliance));
```

- 命令解析大小写不敏感，故 AI 工具 `checkCompliance` 经 `interp_->execute("checkcompliance")` 即可命中，无需重新下单。
- 日志格式：`STDCHECK：发现 N 个不合规项…` + 逐条 `[#id] ruleId severity：message（建议：fixHint）`。
- `CommandCatalog.cpp` 更新 `stdcheck` 条目，新增 `checkcompliance`（分类「工具」，别名 `stdcheck`），`help` 总表与 Ctrl+Shift+P 面板自动枚举。

### 5.2 面板与跳转
- `StandardsPanel`：摘要标签 + 问题列表 + 「运行检查」按钮（按钮同样走 `CommandInterpreter::execute("stdcheck")`，保证与命令行、AI 三条入口同源）。
- 列表项 `setData(Qt::UserRole, 序号)` 记录 issue 下标，`setToolTip(fixHint)`；双击触发 `model_->setSelected(id)` + `view_->zoomToElement(id)` 完成跳转。
- `MainWindow` 仿 `LayerPanel` 挂载为右侧 dock，并在 `hookActiveDoc` 里同步 `setModel/setView`，保证多文档切换后跳转目标正确。
- `CMakeLists.txt` 的 `UI_SOURCES` 是显式列表，需手工追加 `src/ui/StandardsPanel.cpp`（否则链接期 undefined reference）。

---

## 6. 调试实录：三次编译错误 + 一次隐蔽运行时漏报

### 6.1 编译期错误（增量构建中发现）

| # | 现象 | 根因 | 修复 |
|---|------|------|------|
| 1 | `cannot convert 'SketchCAD::StandardsPanel*' to 'StandardsPanel*'` | `MainWindow.h` 用了**全局**前向声明 `class StandardsPanel;`，而真实类在 `namespace SketchCAD` 内，生成了伪全局类 | 改为直接 `#include "ui/StandardsPanel.h"`（与 `LayerPanel` 同范式），删前向声明 |
| 2 | `boundsOf is private` / 自由函数与成员同名 / `layerAt(i)` 指针误用 | `SketchModel::boundsOf` 是私有；`checkLayerNaming` 自由函数与静态方法重名；`layerAt` 返回值非指针 | 自建 `elementBBox()`；自由函数改名 `checkLayerNamingRule`；`layerAt(i).name` |
| 3 | `new QListWidgetItem(label, list_)` / `setText` incomplete type | 在 `namespace SketchCAD` 内前向声明 `class QListWidget*` 会生成伪类 `SketchCAD::QListWidget` | 头文件直接 `#include <QListWidget>/<QPushButton>/<QLabel>`，去掉内部前向声明 |

> 教训：**命名空间内禁止前向声明 Qt 类**；跨命名空间的自定义类优先直接 include。

### 6.2 运行时漏报（本次「继续」阶段修复，核心问题）

首次全量自测结果 **3351 通过 / 3 失败**：R1、R3、R4、R5、R7 均正确检出，唯独 **R2 与 R6 漏报**，导致 `t_compliance` 整体用例连带失败。

由于结构性问题（编译错、命令没触发）会表现为「全部规则都不报」，而此处恰好 5 条通过、2 条不报，可判定是**这两条规则内部的几何判定缺陷**。插入临时 `fprintf` 探针后得到关键证据：

```
[DBG R2] item id=1  type=9(Polyline) bbox=(0.00,0.00,420.00,0.00)   ← 图框高度竟为 0
[DBG R2] frame pts size=5: (0,0) (420,0) (420,297) (0,297) (0,0)     ← 点数据完全正确
[DBG R6] c=5.00 frame=(5,5,410,287)
[DBG R6] visit id=11 layer=0 emptyskip=0                              ← 越界线确实被遍历
[DBG R6] elem  id=11 …（未打印）                                        ← 却被 isEmpty() 跳过
```

**根因**：工程制图中水平线/垂直线极常见，其外接矩形宽或高为 0，而 Qt 把 `width==0 || height==0` 的 `QRectF` 判为 `isEmpty()`，且：

- `QRectF::united(empty)` 返回另一操作数 → 用 `bb |= QRectF(point, QSizeF(0,0))` 累加端点时，**退化矩形永远长不大**，图框高度永远停在 0；
- `QRectF::intersects(empty)` 恒为 `false` → R2 的包围盒预裁剪**永远跳过**精确间隙计算；
- `if (b.isEmpty()) continue;` → R6 把「越界的水平线」直接当成空图元丢掉。

三个现象同源：`united / intersects / contains` 对空矩形一律按「真空」处理。而测试用的违规线恰好全是水平线，于是漏报被稳定复现。

**修复**：引入标量包围盒 `struct BBox`，用显式 `minX/minY/maxX/maxY` + `valid` 标志把「是否吃到过点」与「矩形是否退化」解耦：

```cpp
struct BBox {
    bool valid = false;
    double minX, minY, maxX, maxY;
    void add(double x, double y);              // 首次写入即置 valid
    void add(const BBox& o);
    bool intersectsPad(const BBox& o, double pad) const;  // 取代 united+intersects
    bool inside(const BBox& outer, double eps = 1e-9) const;  // 取代 contains
    BBox  shrunk(double d) const;              // 取代 adjusted
};
```

`elementBBox()`、`clusterByName()`、`checkMinGap()`、`checkSheetMargin()` 全部改用 `BBox`。修复后图框盒恢复 `(0,0)–(420,297)`，越界水平线不再被跳过。

> **可复用教训**：Qt `QRectF` 不适合承载「可能退化的几何包围盒」。凡涉及线段/共线点的包围盒预裁剪，一律用标量 min/max；`isEmpty()` 只能用于表达「没有数据」，不能用于表达「几何退化」。这条与工程长期记忆里「纸面 mm vs 模型 mm」「同一语义两处手工拼装要收敛单一入口」同属几何判定类高频陷阱。

### 6.3 顺带清理的预存警告
`WeldSymbol.cpp:209` 的 `symA`（非箭头侧反向映射）是预留死代码，从未被任何 `switch` 分支引用，触发 `-Wunused-but-set-variable`。为达成「双目标零警告」铁律，加 `[[maybe_unused]]` 并补注释保留设计意图（不删除，避免丢失双面焊缝的扩展线索）。

---

## 7. 验收结果

| 项 | 结果 |
|----|------|
| 双目标构建 `-j 1`，`-Wall -Wextra` | `warning:/error:` 计数 **0**，`[6/6] Linking` 成功 |
| 全量自测 `QT_QPA_PLATFORM=offscreen` | **EXIT=0，通过 3354 / 失败 0**（修前 3351/3，+3 通过、−3 失败） |
| `t_compliance` | 7 条规则 `R1…R7` 全部经 `STDCHECK` 日志检出；「我的图层」被正确点名 |
| 反向不误报 | 标准 2:1 线宽比（1.0/0.5）+ 字号 3.5mm 不误报 R1 |
| `t_layerGB` | 按新 `ruleId` 断言通过 |
| `dist` 同步 | `dist/SketchCAD.exe` mtime `11:22:39` > 源码编辑 `11:19:17`；`cmp` 与 build 产物**字节完全一致** |

---

## 8. 已知限制与后续建议

1. **几何近似**：R2 把圆/弧/椭圆按 24 段折线近似，R4/R5 用 `name` 标签 + 几何启发式聚类；对极密集或自定义符号可能存在聚类边界误差。若后续引入独立 `ElementType::WeldSymbol / Roughness`，可替换启发式为类型直判。
2. **幅面来源**：R6 依赖 `currentSheetFormat()` 全局幅面 + 「图框」层推断纸张边界；用户自画非标图框时会按内缩保守值判定，无图框则整条跳过（刻意避免误报）。
3. **R1 口径**：目前按「相邻线宽对 ≈2:1」判定，未区分粗线组内的多个线宽是否属于同一线型组别；如需更严，可结合 `Layer` 的线宽语义扩展。
4. **面板交互**：当前为「双击跳转」，可考虑增加按 `ruleId` 过滤、批量定位、以及「一键应用 fixHint」的半自动整改。

---

## 9. 变更文件清单

| 文件 | 变更 |
|------|------|
| `src/core/standards/ComplianceChecker.h` | 重写：`ComplianceIssue` 新结构、`run`、回调桥 |
| `src/core/standards/ComplianceChecker.cpp` | 重写：七规则 + `BBox` 标量包围盒 |
| `src/core/weld/WeldSymbol.cpp` | 输出图元加 `name="焊缝符号"`；`symA` 加 `[[maybe_unused]]` |
| `src/core/Commands.cpp` | `stdcheck` / `checkcompliance` 双注册 + 日志上报 |
| `src/core/CommandCatalog.cpp` | 更新/新增目录条目 |
| `src/ui/StandardsPanel.{h,cpp}` | 新建：合规面板（列表、运行、双击跳转） |
| `src/ui/MainWindow.{h,cpp}` | 挂载 dock、注册 sink、`hookActiveDoc` 同步 |
| `CMakeLists.txt` | `UI_SOURCES` 追加 `StandardsPanel.cpp` |
| `tests/SketchSelfTest.cpp` | `t_layerGB` 断言改新结构；新增 `t_compliance()` 并注册 |
| `docs/compliance-checker-design-analysis.md` | 本文档 |
