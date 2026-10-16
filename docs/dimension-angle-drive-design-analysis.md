# 尺寸标注增强：角度标注（两线选角）+ 尺寸驱动主开关 设计分析

> **任务**：需求 D —— 修改尺寸标注功能
> 1. 实现角度标注：选中两条相交直线后，自动计算夹角并以 `°` 标注；
> 2. 标注数值支持在**图元上直接修改**，也支持在**属性栏**中修改；
> 3. 提供**开关控件（默认开启）**，控制修改数值时是否同步调整该图元及其相连图元的实际大小（开→驱动几何，关→仅改显示）；
> 4. 导出本次实现的完整思考过程（设计思路、关键算法、决策依据）。
> **状态**：✅ 代码已落盘（双目标增量构建中），自测 `t_dimAngleDrive` 已注册
> **日期**：2026-10-15
> **关联文档**：`docs/dimension-driven-editing-design-analysis.md`（通用 `driveDimension` 核心与 `t_dimDriven` 验证）、`docs/dimension-driven-parametric-design-analysis.md`（参数化建模闭环）

---

## 一、思考过程（Thinking）

### 1.1 需求拆解

| 需求点 | 实质 | 落点 |
|---|---|---|
| ① 角度标注：选两相交直线自动算夹角并以 `°` 标注 | 新增「选两条相交直线」这一创建路径，补齐 `dim angle` 此前只能命令行给三点（顶点+两边点）的缺口 | `Commands.cpp` Angle 分支 |
| ② 数值可图元直改 + 属性栏改 | 画布双击编辑 + 属性面板「测量值」字段都汇入同一驱动/显示分派逻辑 | `MainWindow::editElementText` + `PropertyPanel::applyDimValue` |
| ③ 全局开关（默认开）控制「改值是否驱动几何」 | 开→`driveDimension` 联动改实际大小及相连图元；关→仅覆盖标注显示文本，几何不变 | `SketchModel::dimensionDriven_` + `AppSettings` 持久化 + `StatusBar` 按钮 + 三入口分派 |
| ④ 导出思考过程 | 即本文档 | `docs/dimension-angle-drive-design-analysis.md` |

### 1.2 现状核查（代码实况证据）

| # | 事实 | 证据 |
|---|---|---|
| 1 | `SketchModel::driveDimension(dimId, value)` 已是**唯一**几何驱动入口，角度分支（约 810–826 行）只旋转标注自身 `pts[2]` 绕 `pts[0]`，并经 `collectLinked(pts[2], 1e-6)` 把与 `pts[2]` 坐标重合的端点一并旋到新位置 | `SketchModel.cpp:752` 起 |
| 2 | **既有佐证**：`t_dimDriven` ③ 用两条共端于 (0,0) 的直线 + 标注 `pts={0,0, 30,0, 0,30}`（`refA=-1`，坐标锚定），驱动 90°→60° 断言边2端点 (15, 25.98) + 实测 60° —— 证明「坐标锚定」的角度驱动天然可行，且**不用改 driveDimension 内核** | `tests/SketchSelfTest.cpp` `t_dimDriven` |
| 3 | **缺口 A（已闭环）**：`dim angle` 原只接受命令行 `顶点 边1点 边2点`，无「选两条相交直线」路径；现已将交互（`GraphicsView::handleDimension`）与命令（`dim angle`）**统一改为「选两条相交直线」**，三点形式移除 | `Commands.cpp` Angle 分支 + `GraphicsView::handleDimension` |
| 4 | **缺口 B**：既无「尺寸驱动主开关」状态源，也无状态栏控件；三处改值入口（双击/属性栏/`dimedit`）此前一律直驱几何，无法「仅改显示」 | 改动前各入口 |
| 5 | 求交 API `geo::intersectElements(A,B)`（折线×折线分支走 `segSegIntersect`）对共享端点的两线会返回该端点 —— 满足「选两条相交直线求顶点」 | `Geometry.cpp:775 / :818` |
| 6 | `StatusBar` 已内置 `enum class Toggle { Ortho, Polar, Snap, Track, Lwt }` + `btns_[5]` 的「扩展即加一项」范式，新增开关只需扩枚举/构造项 | `StatusBar.h:22`、`StatusBar.cpp:36` |

### 1.3 方案比选

**角度标注（需求①）—— 选定方案：在 `dim angle` 命令里「若已选两条相交直线，则自动求交取远端端点」，复用既有 `driveDimension` 角度分支。**
- 备选：另写「角度标注驱动内核」专门旋转两条线。否决——`driveDimension` 角度分支已通过 `collectLinked` 把与 `pts[2]` 重合的端点一并旋转；只要让 `p1/p2` 与两直线的**远端端点坐标重合**，驱动即天然旋转第二条线，回归风险最低，且无需碰内核。
- 延迟决策：哪条线被旋转（pts[2] 决定），取决于选择顺序与哪条线的远端端点被赋给 `p2`。当前以「第二条被选中直线的远端端点 = p2」为准；如需更可控可后续让用户选基准边，但超出本次范围。

**尺寸驱动主开关（需求③）—— 选定方案：四层接线 + 三入口统一分派。**
- 状态真相源放在 `SketchModel::dimensionDriven_`，但默认值与持久化交给 `AppSettings`（单一数据源范式，已在前序特性中多次验证）。状态栏 `DimDrive` 按钮作为用户控件；`MainWindow` 负责把三者（模型态 / 注册表 / 按钮）对齐。
- 三处改值入口（双击编辑、属性面板 `applyDimValue`、`dimedit` 命令）全部先查 `model_->dimensionDriven()`：开→`driveDimension`，关→`beginEdit("…仅显示")` + `updateElementQuiet` 只写 `text`。
- 否决方案：给每个标注加「是否可驱动」布尔字段。否决理由——需求是「全局开关」，按标注维度的独立开关是不同语义，且会引入新的序列化键与 UI，超出范围。

**「关」时为什么只覆盖 `text` 而非其它处理 —— 选定：仅覆盖显示文本，保留几何真值。**
- 理由：标注元模型已有「覆盖文本 `text`」语义（`text` 非空时 `measureDimension` 仍返回几何实测值，显示走 `text`）。关开关 = 让「改值」等价于「改覆盖文本」，不用新增数据字段，且 `measureDimension` 仍返回真实夹角，显示与几何解耦但几何数据无损——这正符合「仅改显示、不改实际大小」。

---

## 二、关键算法与决策依据

### 2.1 两线选角的顶点与边点求取

```
选中两条直线 a、b（各 ≥2 端点）
ips = geo::intersectElements(*a, *b)        // 求交点（共享端点场景返回该端点）
若 ips 为空 → 报错「两直线不相交」，返回 false
v  = ips.front()                            // 夹角顶点
p1 = (a.pts[0] 距 v < 1e-6) ? a.pts[1] : a.pts[0]   // a 的远端端点 = 边1点
p2 = (b.pts[0] 距 v < 1e-6) ? b.pts[1] : b.pts[0]   // b 的远端端点 = 边2点
e.pts = {v, p1, p2}                         // 三控制点：顶点 / 边1 / 边2
```
要点：`p1/p2` 直接取两直线**远离交点的端点**，使标注的 `pts[2]`（`=p2`）与「第二条被选中直线的远端端点」坐标重合。这正是 `driveDimension` 角度分支能「免改内核就旋转该线」的关键（见 2.2）。

### 2.2 坐标锚定角度驱动（复用内核，零侵入）

`driveDimension` 角度分支的既有行为（未经本次改动）：
- 仅把标注自身 `pts[2]` 绕 `pts[0]` 旋转到目标角；
- 再经 `collectLinked(pts[2], 1e-6)` 找出所有与 `pts[2]` 坐标重合的图元端点，一并旋到新位置。

由于 2.1 已让 `pts[2] == 第二条直线的远端端点`，驱动角度时：标注的 `pts[2]` 被旋到新角 → `collectLinked` 命中该线端点 → 该线随动。第一条线（pts[1] 侧）保持不动，夹角由第二条线的旋转体现。**这正是 `t_dimDriven` ③ 已验证的语义**，本次只是把「构建符合该布局的标注」这件事从硬编码三点变成「选两线自动生成」，内核零改动，回归面最小。

### 2.3 全局开关的四层接线

| 层 | 角色 | 关键声明 / 接线 |
|---|---|---|
| 状态真相源 | `SketchModel` 持有布尔态，变化发信号 | `bool dimensionDriven_ = true;`、`void setDimensionDriven(bool)`（幂等：同值不发信号）、`signal dimensionDrivenChanged(bool)` |
| 默认值与持久化 | `AppSettings` 单一数据源 | `kDimensionDrive = "dimension/driveOn"`；`specTable()` 条目 `{kDimensionDrive, true, boolVal}`（默认开）；`defaultValue()` 静态访问器供无文件系统依赖的测试校验 |
| 用户控件 | `StatusBar` 第 6 个开关 | `enum class Toggle { …, DimDrive }`；`defs` 末项 `{"驱动", "尺寸驱动…默认开：改标注值是否联动实际几何；关=仅改标注显示"}` |
| 对齐与同步 | `MainWindow` 四处接线 | ① 启动 `applySettingLive(kDimensionDrive, appSettings_->value(...))`；② `applySettingLive` 内 `setDimensionDriven(v)`；③ 关闭时 `appSettings_->setDimensionDrive(model_->dimensionDriven())` 抄回；④ `toggleRequested` 信号 `case DimDrive: setDimensionDriven(on)`；⑤ `hookActiveDoc` 里 `connect(model_, &SketchModel::dimensionDrivenChanged, …)` + 显式 `setToggleState(DimDrive, model_->dimensionDriven())` 双保险回显 |

### 2.4 三入口分派（开关生效点）

| 入口 | 位置 | 分派逻辑 |
|---|---|---|
| 画布双击编辑 | `MainWindow::editElementText`（Dimension 分支） | 输入为纯数值且 `dimensionDriven()` 真 → `driveDimension(id, v)`；否则 → `beginEdit("编辑标注数值(仅显示)")` + `updateElementQuiet(id, x.text = txt)` 仅改显示 |
| 属性栏「测量值」 | `PropertyPanel::applyDimValue` | 同上：`dimensionDriven()` 真 → `driveDimension`；否则 → `updateElementLinked(curId_, el.text = …)` 仅改显示 |
| 命令行 `dimedit` | `Commands.cpp` dimedit 段 | 开关关 → `beginEdit("dimedit(仅显示)")` + `updateElementQuiet(id, d.text = valStr)`；开关开 → `driveDimension(id, v)` |

> 属性栏数值字段启用条件也做了放宽：`sDimValue->setEnabled(e->refA >= 0 || e->dtype == DimensionType::Angle)` —— 角度标注 `refA=-1`（坐标锚定）仍允许编辑，与 `driveDimension` 角度分支一致。

### 2.5 默认开启的决策依据

需求③明文：「提供开关控件（**默认开启**），用于控制修改数值时是否同步调整该图元及其相连图元的实际大小」。故：
- `dimensionDriven_ = true`（模型态默认开）；
- `AppSettings` `specTable()` 默认值 `true`；
- 自测 `t_dimAngleDrive` ④ 经 `AppSettings::defaultValue("dimension/driveOn").toBool()` 校验默认开。
- 用户若关闭，偏好经 `setDimensionDrive` 持久化到注册表，下次启动由 `loadAppSettingsOnStartup` 灌回模型，状态栏按钮同步回显。

---

## 三、实现细节（逐文件）

### 3.1 `src/core/Commands.cpp` —— Angle 分支（两线选角）+ dimedit 分派

**Angle 分支新增「选两条相交直线」路径**（替换原整段）：

```cpp
} else if (dt == DimensionType::Angle) {
    Vec3 v, p1, p2;
    const auto sel = m->selection();
    if (sel.size() == 2) {
        // 选中两条相交直线：以交点为顶点、两线远端端点为边点。
        // 此布局与 driveDimension 角度分支一致——驱动时天然旋转第二条线（pts[2] 绕顶点旋转，
        // 与 pts[2] 坐标重合的端点一并联动），无需改动内核即可实现夹角驱动。
        const Element* a = m->element(sel[0]);
        const Element* b = m->element(sel[1]);
        if (a && b && a->type == ElementType::Line && b->type == ElementType::Line
            && a->pts.size() >= 2 && b->pts.size() >= 2) {
            const auto ips = geo::intersectElements(*a, *b);
            if (!ips.empty()) {
                v  = ips.front();
                p1 = (a->pts[0].distTo(v) < 1e-6) ? a->pts[1] : a->pts[0];
                p2 = (b->pts[0].distTo(v) < 1e-6) ? b->pts[1] : b->pts[0];
            } else {
                m->log(QStringLiteral("dim angle: 所选两直线不相交，无法标注夹角"), QStringLiteral("warn"));
                return false;
            }
        } else {
            m->log(QStringLiteral("dim angle: 请选中两条相交直线，或在命令行给出顶点与两边点"), QStringLiteral("warn"));
            return false;
        }
    } else if (ctx.toks.size() < 5 || !ctx.parsePoint(2, v) ||
               !ctx.parsePoint(3, p1) || !ctx.parsePoint(4, p2)) {
        m->log(QStringLiteral("用法: dim angle 顶点x,y 边1点x,y 边2点x,y（或先选中两条相交直线）"), QStringLiteral("warn"));
        return false;
    }
    e.pts = {v, p1, p2};
}
```

**dimedit 命令按全局开关分派**（新增关开关分支）：

```cpp
const double v = mt.captured(0).toDouble();
const double oldV = geo::measureDimension(*e);
if (!m->dimensionDriven()) {
    // 尺寸驱动主开关关闭（需求 D-③）：仅覆盖显示文本，不改动任何图元几何
    m->beginEdit(QStringLiteral("dimedit(仅显示)"));
    m->updateElementQuiet(id, [valStr](Element& d) { d.text = valStr; });
    cmdRememberArgs(ctx.cmd, t);
    m->log(QStringLiteral("尺寸驱动已关闭：dimedit 仅更新标注显示文本 #%1 → %2（几何未改动）")
               .arg(id).arg(valStr), QStringLiteral("info"));
    return true;
}
if (!m->driveDimension(id, v)) { /* 原 warn 分支不变 */ }
```

### 3.2 `src/core/SketchModel.h` / `.cpp` —— 状态真相源

```cpp
// SketchModel.h
bool dimensionDriven() const { return dimensionDriven_; }
void setDimensionDriven(bool on);                 // 幂等；变化发 dimensionDrivenChanged()
signals: void dimensionDrivenChanged(bool on);
// 成员
bool dimensionDriven_ = true;   // 尺寸驱动主开关（默认开：改标注值联动实际几何）

// SketchModel.cpp
void SketchModel::setDimensionDriven(bool on) {
    if (dimensionDriven_ == on) return;          // 幂等：同值不重复发信号
    dimensionDriven_ = on;
    emit dimensionDrivenChanged(on);
}
```

### 3.3 `src/core/AppSettings.h` / `.cpp` —— 默认值与持久化

```cpp
// AppSettings.h  (SettingKeys 命名空间)
inline constexpr const char* kDimensionDrive = "dimension/driveOn";   // 尺寸驱动主开关，默认开
// 访问器
bool   dimensionDriveOn() const;
void   setDimensionDrive(bool b);

// AppSettings.cpp
// specTable() 条目（kFieldAutoUpdate 后）
{ kDimensionDrive, true, boolVal },        // 默认开，与需求③一致
// 静态默认值访问器（供无文件系统依赖测试校验）
//   QVariant AppSettings::defaultValue(const QString& key) { 遍历 specTable 返回 sp.def; }
bool AppSettings::dimensionDriveOn() const { return value(QLatin1String(kDimensionDrive)).toBool(); }
void AppSettings::setDimensionDrive(bool b) { setValue(QLatin1String(kDimensionDrive), b); }
```

### 3.4 `src/ui/StatusBar.h` / `.cpp` —— 状态栏开关

```cpp
// StatusBar.h
enum class Toggle { Ortho = 0, Polar, Snap, Track, Lwt, DimDrive };
QToolButton* btns_[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

// StatusBar.cpp （defs[5] → defs[6]，末项新增）
{ Toggle::DimDrive, "驱动", "尺寸驱动 (需求 D-③，默认开)：改标注值是否联动调整实际几何；关=仅改标注显示" },
```

### 3.5 `src/ui/MainWindow.cpp` —— 四处接线 + 双击编辑分派

```cpp
// loadAppSettingsOnStartup：启动灌模型
applySettingLive(QLatin1String(kDimensionDrive), appSettings_->value(QLatin1String(kDimensionDrive)));
// applySettingLive：settingChanged 统一消费点
else if (key == QLatin1String(kDimensionDrive)) { model_->setDimensionDriven(v.toBool()); }
// saveAppSettingsOnClose：抄回注册表
appSettings_->setDimensionDrive(model_->dimensionDriven());
// toggleRequested：开关点击分派
case StatusBar::Toggle::DimDrive: model_->setDimensionDriven(on); break;
// hookActiveDoc：信号连接 + 显式回显（双保险）
modelConns_ << connect(model_, &SketchModel::dimensionDrivenChanged, this,
    [this](bool on){ statusBar_->setToggleState(StatusBar::Toggle::DimDrive, on); });
statusBar_->setToggleState(StatusBar::Toggle::DimDrive, model_->dimensionDriven());
```

`editElementText` 的 Dimension 分支（数值 + `dimensionDriven()` 真 → 驱动；否则仅改显示）：

```cpp
const QString trimmed = txt.trimmed();
static const QRegularExpression numRe(QStringLiteral("^[+-]?\\d+(?:\\.\\d+)?$"));
if (!trimmed.isEmpty() && numRe.match(trimmed).hasMatch()) {
    const double v = trimmed.toDouble();
    if (model_->dimensionDriven()) {
        if (!model_->driveDimension(id, v)) { /* 不支持类型 warn + return */ }
        model_->log(QStringLiteral("尺寸驱动 #%1 → %2（关联几何与标注已联动更新）")...);
    } else {
        // 尺寸驱动主开关关闭（需求 D-③）：仅覆盖显示文本，不改动任何图元几何
        model_->beginEdit(QStringLiteral("编辑标注数值(仅显示)"));
        model_->updateElementQuiet(id, [txt](Element& x) { x.text = txt; });
        model_->log(QStringLiteral("尺寸驱动已关闭：仅更新标注显示文本 #%1 → %2（几何未改动）")...);
    }
    return;
}
```

### 3.6 `src/ui/PropertyPanel.cpp` —— 属性栏值字段 + 分派

```cpp
// load()：角度标注（refA<0，坐标锚定）仍允许编辑
sDimValue->setEnabled(e->refA >= 0 || e->dtype == DimensionType::Angle);
sDimValue->setSuffix(e->dtype == DimensionType::Angle ? QStringLiteral(" °") : QStringLiteral(" mm"));

// applyDimValue()：按开关分派
void PropertyPanel::applyDimValue() {
    if (updating_ || curId_ < 0) return;
    const Element* e = model_->element(curId_);
    if (!e || e->type != ElementType::Dimension) return;
    const double v = sDimValue->value();
    if (model_->dimensionDriven()) {
        if (!model_->driveDimension(curId_, v))
            model_->log(QStringLiteral("该标注类型暂不支持驱动几何"), "warn");
    } else {
        model_->updateElementLinked(curId_, [this](Element& el) { el.text = sDimValue->text(); });
        model_->log(QStringLiteral("尺寸驱动已关闭：仅更新标注显示文本 #%1").arg(curId_));
    }
}
```

### 3.7 `tests/SketchSelfTest.cpp` —— 新增 `t_dimAngleDrive` 并注册

覆盖四点：① 选两相交直线创建角度标注（顶点=交点、实测=90°）；② 默认开 → `dimedit 60` 驱动后边2端点 (15, 25.98)、实测=60°；③ 关开关 → `dimedit 45` 几何端点不动、仅 `text=="45"`、实测仍 90°（显示与几何解耦）；④ `AppSettings::defaultValue("dimension/driveOn").toBool()` 为真（默认开）。

```cpp
// 主注册
CHECK(t_dimAngleDrive(), "需求D: 两线角度标注+尺寸驱动主开关");
```

---

## 四、自测验证（`t_dimAngleDrive`）

四个场景断言摘要：

| 场景 | 构造 | 断言 |
|---|---|---|
| ① 选两线创建 | `addLine(0,0,30,0)` + `addLine(0,0,0,30)`，`setSelection` 后 `dim angle` | 创建成功；`pts[0]=(0,0)`；`measureDimension==90°` |
| ② 默认开 → 驱动 | `dimedit <id> 60` | `dimensionDriven()` 为真；边2端点 `(15, 25.98)`；实测 `60°` |
| ③ 关开关 → 仅显示 | `setDimensionDriven(false)` 后 `dimedit <id> 45` | 边2端点未移动；`text=="45"`；实测仍 `90°` |
| ④ 默认值 | `AppSettings::defaultValue("dimension/driveOn")` | `toBool()==true`（默认开） |

返回 `(g_fail - failBefore) == 0` 即全绿。

---

## 五、验收与已知限制

### 5.1 验收标准
- 双目标（`SketchCAD` + `SketchSelfTest`）`-Wall -Wextra` **零警告**构建通过；
- 全量自测 `SketchSelfTest.exe` **EXIT=0**（含本轮 `t_dimAngleDrive`）；
- 需求①～④ 全部落码并经自测覆盖。

### 5.2 已知限制（后续可立项）
1. **角度驱动仅旋转第二条线**：驱动时 `pts[2]`（=第二条被选中直线的远端端点）被旋转，第一条线不动。哪条被驱动取决于选择顺序；如需「双向驱动」或让用户指定基准边，需扩展 `driveDimension` 角度分支或新增约束，超出本次范围。
2. **开关为全局模型态（随文档）**：当前是单一 `{开/关}`，不区分「按标注独立控制」。若产品需要逐标注独立开关，需新增数据字段与 UI，超出需求③语义。
3. **两线须真实相交**：`geo::intersectElements` 对仅延长线相交但不共段的两线（折线分支）行为依赖具体实现；当前未做「延长相交」特判，仅接受实际相交（含共享端点）的两线。
4. **`dim angle` 三点路径已移除**：交互（画布点选，`GraphicsView::handleDimension`）与命令（`dim angle`）**统一为「先选中两条相交直线」**，不再支持命令行三点（顶点+两边点）形式；两直线不相交时给出明确错误提示且**不创建标注**。

---

## 六、小结

本次需求 D 以**最小侵入、最大复用**为原则落地：
- 需求①（角度标注）通过「选两相交直线→`intersectElements` 求顶点→取两线远端端点为边点」生成与 `driveDimension` 角度分支天然契合的标注布局，**零改内核**即获得夹角驱动能力；
- 需求②③（可图元直改 + 全局开关默认开）通过 `SketchModel` 状态源 + `AppSettings` 单一数据源 + `StatusBar` 控件 + `MainWindow` 四处接线，使**画布双击、属性栏、命令行 `dimedit` 三入口**统一分派：开→`driveDimension` 联动实际几何与相连图元，关→仅覆盖 `text` 显示（几何真值无损）；
- 需求④ 即本文档，完整记录设计思路、关键算法与决策依据。

回归风险集中在「未触碰 `driveDimension` 内核」与「开关仅新增分支、不改变既有开路径语义」，由 `t_dimAngleDrive` 四场景断言兜底。
