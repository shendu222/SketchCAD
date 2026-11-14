# GB/T 18617 / ISO 13567 图层命名建议与合规检查 设计与验收报告

> 模块路径：`src/core/Layer.{h,cpp}`（命名建议 API）、`src/core/standards/ComplianceChecker.{h,cpp}`（合规检查器）、`src/ui/LayerPanel.cpp`（新建图层对话框）、`src/core/Commands.cpp`（`stdcheck` 命令）
> 依据标准：GB/T 18617-2002 / ISO 13567（技术产品文件 图层），数据源自内嵌 `cad_layers.json`
> 配套自测：`tests/SketchSelfTest.cpp::t_layerGB()`
> 状态：已实现并通过自测（EXIT=0）

---

## 1. 背景与目标

CAD 图层命名长期存在"各画各的"问题：同一含义的图层在不同图纸里叫 `轮廓线`、`01轮廓`、`layer1`、`CUNA` 等多种形式，导致**批量出图、按层过滤、跨图交换**时语义丢失。

本特性把 **GB/T 18617 / ISO 13567** 的标准图层命名引入 SketchCAD，但严格遵循"**只建议、不强制**"原则：

- 由 `StandardsDB` 加载 `cad_layers.json` 命名模板（`layer_01`=轮廓线、`layer_04`=中心线、`layer_08`=尺寸标注 … `layer_09`=符号），作为**标准图层名 ↔ 编号**的映射来源；
- 新建图层时，依据模板弹出**候选名称列表 + 实时合规提示**；不改动既有图层名称，用户最终决定用哪个名；
- 在 `ComplianceChecker` 中新增一项**图层命名合规检查**，供命令 `STDCHECK` 扫描整张图纸的不合规图层名。

设计约束（与既有体系一致）：

- **只读诊断、零副作用**：命名建议与合规检查都不修改任何图层，只产出建议/报告；
- **不触碰 `DiagBundle` 诊断管线**：合规结果以结构化 `ComplianceIssue` 独立返回，不混入既有诊断流；
- **中文比较安全**：一律用 `QString` 精确相等，禁用 `QLatin1String(utf8字面量)`（按拉丁一逐字节解码，与 UTF-16 `QString` 永不匹配且静默失效）；
- **规避 GCC 13.1 巨型静态聚合初始化静默崩溃**：标准表解析走 `StandardsDB` 单例 + builder，不引入新的大静态表；
- **零警告**：`-Wall -Wextra` 下无告警。

---

## 2. 对外 API

### 2.1 图层命名建议（`src/core/Layer.h` + `Layer.cpp`）

```cpp
namespace SketchCAD {

// 标准图层条目（编号 + 标准名）
struct StandardLayerEntry { QString code; QString name; };

// 不合规图层命名问题（供 ComplianceChecker 汇总）
struct LayerNamingIssue { QString layerName; QString reason; };

namespace LayerNaming {

// 从 StandardsDB::instance() 读取 cad_layers 表的 layer_01..layer_09，
// 返回 编号 + 标准名 列表；标准表不可用时返回空列表（调用方需容忍）。
std::vector<StandardLayerEntry> standardLayerEntries();

// 候选图层名：为每个标准条目生成 "<编号>_<名称>" 形式（如 "01_轮廓线"），
// 供新建图层对话框作为下拉候选。
QStringList suggestLayerNames();

// 判定图层名是否符合 GB/T 18617 规范。合规返回 true；
// 若不合规且 reason 非空，写入原因与建议。
bool isLayerNameCompliant(const QString& name, QString* reason = nullptr);

// 批量检查一组图层名，返回所有不合规项。
QVector<LayerNamingIssue> checkLayerNaming(const QStringList& names);

} // namespace LayerNaming
} // namespace SketchCAD
```

`Layer` 结构本身保持"只增不改"：GB/T 14665 已有的 `code` / `standardName` 字段（01~12 粗/细实线等）**不受影响**，本特性的 GB/T 18617 编号（01~09 轮廓线/隐藏线…）是**另一套标准映射**，二者互不干扰。`Layer.h` 由头文件-only 改为"头文件声明 + `Layer.cpp` 实现"，仅新增 `.cpp`，不破坏既有内联 `standardLayers()`。

### 2.2 合规检查器（`src/core/standards/ComplianceChecker.{h,cpp}`）

```cpp
namespace SketchCAD {

struct ComplianceIssue {
    QString layerName;   // 触发问题的图层名（与图层无关时为空）
    QString category;    // 检查类别，如 "layer_naming"
    QString reason;      // 问题描述 + 建议
};

class ComplianceChecker {
public:
    static QVector<ComplianceIssue> checkModel(const SketchModel& model);
    static QVector<ComplianceIssue> checkLayerNaming(const QStringList& layerNames);
private:
    ComplianceChecker() = delete;   // 纯静态工具类
};

} // namespace SketchCAD
```

`checkModel` 只是"取模型全部图层名 → 调 `checkLayerNaming`"的薄封装；真正的判定逻辑全部下沉到 `LayerNaming`，保证 **GUI 提示、命令、单测** 三者同源。

### 2.3 命令（`stdcheck`）

```
stdcheck                依据 GB/T 18617/ISO 13567 检查当前图纸全部图层命名是否符合规范
```

命令经 `CommandRegistry::registerBuiltins()` 注册后，对 `CommandInterpreter::execute("stdcheck")` 自动可见；内部调用 `ComplianceChecker::checkModel(*m)`，把结果逐条 `m->log(...)` 输出（合规时输出一条 `info`，不合规时输出 `warn` 头 + 逐项 `info`）。

---

## 3. 数据来源：`cad_layers.json`

文件位于 `src/core/standards/data/cad_layers.json`，经 `qt_add_resources` 编译进可执行文件（`:/standards/cad_layers.json`），运行时由 `StandardsDB` 读取，**部署零附件**。

关键字段（节选）：

```json
{
  "_standard": "GB/T 18617/ISO 13567",
  "layer_01": "轮廓线",
  "layer_02": "隐藏线",
  "layer_03": "剖切线",
  "layer_04": "中心线",
  "layer_05": "剖面线",
  "layer_06": "标注",
  "layer_07": "文字",
  "layer_08": "尺寸标注",
  "layer_09": "符号"
}
```

`standardLayerEntries()` 遍历 `01..09`，用 `StandardsDB::instance().queryString("cad_layers", "layer_<XX>")` 取中文名。`StandardsDB::instance()` 是 Meyers 单例，首次访问自动 `load()`（不回调 `instance()`，无 magic statics 再入死锁）。

---

## 4. 合规判定规则

`isLayerNameCompliant(name, reason)` 的判定优先级：

1. **空名** → 不合规，原因"图层名不能为空"；
2. **保留默认层 `"0"`** → 合规（AutoCAD / 本系统的默认层，固定合法）；
3. 对任一标准条目 `(code, name)`，命中下列**任一形式**即合规：
   - `"<编号>_<名称>"`  例：`01_轮廓线`
   - `"<编号> <名称>"`  例：`01 轮廓线`
   - `"<编号>-<名称>"`  例：`01-轮廓线`
   - `"<编号><名称>"`   例：`01轮廓线`
   - `"<名称>"`         例：`轮廓线`（裸标准名亦接受，便于沿用习惯命名）
4. 均不命中 → 不合规，写入建议"`未匹配 GB/T 18617/ISO 13567 标准图层名，建议形如 01_轮廓线`"。

> 设计取舍：判定**宽松**（多种分隔符 + 裸名都算合规），目的是"提示"而非"卡死"。真正卡死既有命名会破坏用户历史图纸，故只在**新建对话框**给建议、**STDCHECK** 给报告，绝不自动改名。

---

## 5. 各模块实现要点

### 5.1 `Layer.cpp`（命名建议逻辑）

- `standardLayerEntries()`：用 `static const char* kCodes[] = {"01"…"09"}` + 逐条 `queryString` 的 builder 式循环，避免大静态表；
- `suggestLayerNames()`：对每个条目生成 `"<code>_<name>"`，作为对话框首选候选；
- `matchStandard()`（匿名命名空间内）：实现第 4 节的五种形式匹配；
- 中文比较全部基于 `queryString` 返回的 `QString` 做 `==`，安全。

### 5.2 `ComplianceChecker.cpp`

- `checkLayerNaming()`：把 `LayerNaming::checkLayerNaming` 的 `LayerNamingIssue` 映射为 `ComplianceIssue`（`category = "layer_naming"`），**结构化、可扩展**（未来加其它类别时只需新增 `category` 分支）；
- `checkModel()`：`model.layerCount()` / `model.layerAt(i).name` 取出全部图层名后委托；
- 头文件仅前向声明 `class SketchModel;`，不重依赖 `SketchModel.h`，降低耦合。

### 5.3 `LayerPanel::onNewLayer()`（新建图层对话框）

原实现仅 `QInputDialog::getText` 取字符串。改为自定义 `QDialog`：

- 一个**可编辑 `QComboBox`**（`setEditable(true)` + `NoInsert`）：候选 = `LayerNaming::suggestLayerNames()` 的标准名 + 既有图层名（便于沿用）；
- 一个**实时 `QLabel` 合规提示**：`QLineEdit::textChanged` 触发 `updateTip()`：
  - 合规 → 绿色"`✓ 符合 GB/T 18617/ISO 13567 规范`"；
  - 不合规 → 琥珀色"`⚠ 不合规：<原因>`"；
  - 空 → 灰色中性提示；
- **仅建议不强制**：`accepted` 后只把用户最终敲定的 `combo->currentText().trimmed()` 传给 `model_->addLayer(name)`，合规与否完全由用户决定。

### 5.4 `stdcheck` 命令

在 `registerBuiltins()` 中 `registerExact("stdcheck", LambdaCommand{...})`，与 `standards` 命令相邻（同属标准知识库家族）。Lambda 内：

```cpp
const QVector<ComplianceIssue> issues = ComplianceChecker::checkModel(*m);
if (issues.isEmpty())
    m->log("STDCHECK：全部图层命名符合 GB/T 18617/ISO 13567 规范。", "info");
else {
    m->log(QString("STDCHECK：发现 %1 个图层命名不符合…").arg(issues.size()), "warn");
    for (const auto& iss : issues)
        m->log(QString("  · %1：%2").arg(iss.layerName, iss.reason), "info");
}
return true;
```

### 5.5 `CommandCatalog` 元数据

在 `entries()` 的 builder lambda 末尾追加 `stdcheck` 条目（类别"工具"），使命令面板 `help stdcheck`、命令总表自动枚举，**与 `standards` 互相关联**（`rel = {"standards"}`）。

### 5.6 构建接入（`CMakeLists.txt`）

`CORE_SOURCES` 新增：
```
src/core/Layer.cpp
src/core/standards/ComplianceChecker.cpp
```
`cad_layers.json` 已在 `SKETCHCAD_STD_DATA` 中、主程序与 `SketchSelfTest` 均挂载资源，无需改动。

---

## 6. 边界与约束（复盘踩坑点）

| 关注点 | 处理 |
|---|---|
| 头文件-only 的 `Layer.h` 改为含 `.cpp` | 仅新增 `Layer.cpp` 实现命名 API，既有内联 `standardLayers()` 不动 |
| GCC 13.1 巨型静态表静默崩溃 | 标准表解析走 `StandardsDB` 单例运行时查表，无新静态大表 |
| 中文比较静默失效 | 一律 `QString` 精确相等，禁 `QLatin1String(utf8字面量)` |
| 默认层 `"0"` 误判不合规 | 显式放行 `"0"` |
| 仅建议不强制 | 对话框/命令均只读；不自动改名 |
| 不污染既有诊断 | 独立 `ComplianceIssue`，不接入 `DiagBundle` |
| 零警告 | 字符串 size 比较统一 `(int)` 强转，避免 `-Wsign-compare` |
| 构建同步 | `cmake --build ... -j 1` 后 `cmp` 校验 `dist/SketchCAD.exe` 与 `build/SketchCAD.exe` 字节一致再 `cp` 同步 |

---

## 7. 验收与自测（`t_layerGB`）

`tests/SketchSelfTest.cpp` 新增 `t_layerGB()`，覆盖：

1. **标准条目装载**：`standardLayerEntries()` 非空；`layer_01=轮廓线`、`layer_08=尺寸标注` 命中；
2. **候选列表**：`suggestLayerNames()` 含 `"01_轮廓线"`，数量与条目一致；
3. **合规判定**：`"0"` / `"01_轮廓线"` / `"01 轮廓线"` / `"08-尺寸标注"` / 裸名 `"轮廓线"` 均合规；非标准名 `"我的图层"` 不合规且给出原因；
4. **ComplianceChecker 批量**：`{"0","01_轮廓线","我的图层","zzz"}` → 检出 2 项，含 `"我的图层"`；
5. **命令路径**：建 `01_中心线`（不合规，因 01 标准名是轮廓线）与 `04_中心线`（合规），`interp.execute("stdcheck")` 无异常，输出含 `STDCHECK` 且能定位不合规图层 `01_中心线`。

验收标准对照：

- ✅ 新建图层对话框弹出合规建议（绿/黄实时提示 + 候选下拉）；
- ✅ `STDCHECK` 能识别不合规的图层名（命令报告 + 自测断言双重覆盖）；
- ✅ 自测脚本 `EXIT=0`（`t_layerGB` 并入全量自测，零失败）。

---

## 8. 使用示例

**新建图层（GUI）**：点击"新建图层" → 下拉看到 `01_轮廓线` … `09_符号` → 选 `04_中心线` → 提示变绿 → 确认即建标准命名图层；若手输 `my_layer` → 提示变黄"不合规：未匹配…" → 仍可按"确定"建（仅建议）。

**合规检查（命令）**：

```
> stdcheck
STDCHECK：发现 1 个图层命名不符合 GB/T 18617/ISO 13567 规范：
  · 01_中心线：未匹配 GB/T 18617/ISO 13567 标准图层名，建议形如 01_轮廓线
```

（注：`01_中心线` 不合规是因为 01 的标准名是"轮廓线"；中心线的正确编号是 04，应命名为 `04_中心线`。）

---

## 9. 后续可扩展

- **命令补全**：加 `stdcheck fix` 子命令做"交互式改名建议"（仍由用户确认）；
- **类别扩展**：`ComplianceIssue.category` 已预留，未来可加"线型/颜色随层"等其它合规维度；
- **模板可配置**：`cad_layers.json` 已是数据外置，可增补行业专用图层（如电气、管道）而无需改 C++。
