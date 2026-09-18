# 圆弧绘制功能设计与实现 —— 深思过程记录

> 项目：sketch-cad-cpp（Qt6 二维 CAD 草图模块）
> 范围：11 种圆弧绘制方式端到端落地
> 标签：`Command 模式` `Factory 模式` `Strategy 模式` `DRY` `开闭原则` `构建通过` `自测 exit 0`

---

## 一、需求理解与拆解

用户附上圆弧工具条截图，要求依图实现**完整的圆弧绘制程序**。附图共 11 种圆弧（含 2 种椭圆弧），并明确约束：

- **二级菜单集成**：圆弧绘制方式需同时挂在
  1. 圆弧工具条下拉二级菜单
  2. 【草图绘制】→【圆弧】菜单路径下
- **中文命名**：所有菜单项一律中文，与附图功能一一对应。
- **四大架构要求**：命令模式封装操作、工厂/策略模式统一管理创建与分发、DRY 提取公共绘制逻辑、开闭原则（新增类型不改旧代码）。
- **代码规范**：异常处理、日志记录、注释齐备。

> **思考起点**：这是"既有架构适配 + 新增 11 种功能"的任务，而非从零搭建。正确做法是先摸清项目已有的圆/直线子模式是怎么做的，**严格复用其骨架**——这本身就是开闭原则的最佳实践。

---

## 二、现有代码探索与架构对齐

通过代码探索确认项目已具备完整的"子模式"机制（直线 12 种、圆 12 种均已落地），圆弧可直接挂载到这套成熟骨架：

| 已有能力 | 落点文件 | 圆弧如何复用 |
|---|---|---|
| 工具注册表（工厂） | `src/core/ToolRegistry.*` | 新增 `ArcSub` 类别 + 10 个子工具注册 |
| 命令接口（命令模式） | `src/core/Commands.*` | 新增 `ArcModeCommand` + `arc_` 前缀分派 |
| 几何计算（策略模式） | `src/core/CircleModes.*` | 参照新建 `ArcModes.*`，`compute(mode,...)` 主入口 |
| 交互状态机 | `src/ui/GraphicsView.*` | 复用三段式（参考图元→点→选解），加 `handleArcSubTool` |
| 菜单/工具条工厂 | `src/ui/MainWindow.*` | `createDropdownTool` + `buildArcMenu` 复用 |

> **关键发现**：圆模块 `CircleModes.h` 的架构是 `Mode 枚举 → compute() 主入口 → 原子构造函数`。圆弧与之**完全同构**，因此新建 `ArcModes.h/.cpp` 即可零摩擦接入，无需改动 `GraphicsView` 的状态机结构。

---

## 三、设计模式的落地映射

### 3.1 命令模式（Command Pattern）

所有圆弧命令统一由 `ArcModeCommand` 一个类承载，通过 `arc_` 前缀分派到具体模式，调用者与执行者彻底解耦。

```cpp
// Commands.h
class ArcModeCommand : public ICommand {
public:
    bool execute(const CommandContext& ctx) override;
};

// Commands.cpp::registerBuiltins
registerPrefix("arc_", std::make_shared<ArcModeCommand>()); // 所有 arc_* 共用一个命令对象
```

```cpp
// ArcModeCommand::execute —— 调用者完全不知道背后是 11 种中的哪一种
bool ArcModeCommand::execute(const CommandContext& ctx) {
    const arc::Mode am = arc::modeFromString(ctx.cmd);
    const arc::ReqCount rc = arc::requirements(am);
    auto pts  = ctx.parsePts(1, rc.pts);
    auto refs = ctx.parseRefs(1, rc.refs);
    arc::Result r = arc::compute(am, pts, refs, cur, m->elements(), param);
    if (!r.valid) { m->log(r.msg, "warn"); return true; }
    Element el = ElementFactory::createArc(r.center, r.radius, r.startAngle, r.endAngle, m);
    m->addElement(el);
    return true;
}
```

### 3.2 工厂模式 / 策略模式（Factory + Strategy）

两个"工厂" + 一个"策略分发"协同：

- **ToolRegistry（工厂）**：单例，启动时注册全部工具元数据（类别、中文名、参数条模式）。`isArcSubTool()` / `arcSubToolIds()` 提供查询能力。
- **CommandRegistry（工厂）**：单例，注册前缀命令；`execute()` 查找并分派，无 if-else 链。
- **arc::Mode + arc::compute（策略）**：每个枚举值是一种绘制策略，`compute()` 内部 `switch(mode)` 分派到对应原子构造函数。新增圆弧类型 = 增一个枚举 + 一个函数，`compute` 主入口**无需改动消费方**。

### 3.3 DRY 原则

- `CommandContext` 收敛了原先散落的 `parsePts/parseRefs/makeLine` 解析逻辑，所有命令共用。
- `GraphicsView::handleArcSubTool` 用统一的**三段式状态机**（参考图元 → 几何点 → 多解选解）覆盖全部 11 种模式，而非每种写一套鼠标逻辑。
- `finalizeArcEx()` 是唯一的圆弧落库入口（创建 Element、写日志、重置状态），11 种模式共用。

### 3.4 开闭原则（Open/Closed）

> **新增一种圆弧 = 只改 4 处"注册点"，不动任何既有 dispatch 逻辑：**
>
> 1. `ArcModes.h` 枚举增一个值 + 声明原子函数
> 2. `ArcModes.cpp` 实现该原子函数 + 在 `compute/requirements/modeFromString/stringFromMode` 的 switch 增 case
> 3. `ToolRegistry.cpp` 增一行 `registerTool({"arc_xxx", "中文名", ...})`
> 4. `MainWindow.cpp` 的 `arcSubModes()` 列表增一行（菜单/工具条自动出现）
>
> 现有 `GraphicsView`、`ArcModeCommand`、菜单框架**一行都不用改**。

---

## 四、11 种圆弧的几何实现思路

全部以纯几何函数落地于 `ArcModes.cpp`，与 UI 解耦，可被命令行、无界面自测、视图层共用。

| # | 模式（中文名 / 工具ID） | 输入 | 核心几何 |
|---|---|---|---|
| 1 | 圆心+点+角度扫掠 `arc` | 圆心/起点/终点 | 圆心定位，终点定终止角（兼容旧行为） |
| 2 | 圆心+点+弦长 `arc_cpc` | 圆心/点+参数 | 弦长→圆心角 `θ=chord/r`，沿圆周取终点 |
| 3 | 圆心+半径+弧角 `arc_cpaa` | 圆心+参数 | 参数条输入半径与扫掠角 |
| 4 | 三点圆弧 `arc_3p` | 起/弧上/终 | 外接圆 + 中间点判定凹凸侧定扫掠方向 |
| 5 | 起终点+夹角 `arc_tpa` | 两端点+参数 | `R = d/(2·sin(θ/2))`，弦两侧双解取近 |
| 6 | 起终点+半径 `arc_tpr` | 两端点+参数 | 中垂线上 `h=√(R²−(d/2)²)`，双解光标选近 |
| 7 | 起终点+弦长 `arc_tpl` | 两端点+参数 | 弧长 → 二分法解圆心角 → 转半径 |
| 8 | 起终点+弦高 `arc_tph` | 起/终/凸度点 | 凸度（sagitta）反解半径 `R=((d/2)²+h²)/(2h)` |
| 9 | 相切延续弧 `arc_tan` | 1参考+2点 | 参考图元端点切向 + 经过目标点定圆心 |
| 10 | 椭圆弧(轴) `arc_ea_axis` | 中心/轴端/角点 | 长轴=半径，短轴=垂直投影（当前以等效圆弧显示） |
| 11 | 椭圆弧(1点) `arc_ea_1pt` | 中心/轴端/过点 | 同 10，过点定短轴 |

> **椭圆弧的诚实取舍**：当前 `Element` 数据模型仅存储圆形（center + radius + start/end angle）。椭圆弧用**等效圆弧**近似显示并记录长短轴比，留 `radiusY` 字段供未来精确渲染。文档与日志均明确标注"等效圆弧显示"，避免误导。

---

## 五、二级菜单与工具条的集成

### 5.1 工具条下拉（一级入口）

```cpp
// MainWindow.cpp —— 圆弧主按钮改为下拉，子项来自 arcSubModes()
createDropdownTool(tb, group, bucket, "arc", QStringLiteral("圆弧"),
                   "arc", QStringLiteral("圆弧 (Arc)：圆心 + 起点 + 终点"),
                   "A", QStringLiteral("圆弧绘制方式"), arcSubModes());
```

### 5.2 顶级菜单【圆弧(&A)】

```cpp
// 顶级菜单：按"基本 / 相切 / 椭圆弧"分组的中文二级子菜单
QMenu* ar = new QMenu(QStringLiteral("圆弧(&A)"), this);
buildArcMenu(ar);
```

### 5.3 【草图绘制】→【圆弧】镜像子菜单

`buildArcMenu()` 复用工具条的 `toolActions_` 指针，与工具条下拉是**同一批 QAction 的两个入口**，天然同步、零重复代码：

```cpp
void MainWindow::buildArcMenu(QMenu* m) {
    auto sub = [&](const QString& title, const QStringList& ids) {
        QMenu* sm = m->addMenu(icons::make("arc"), title);
        for (const QString& id : ids)
            if (QAction* a = toolActions_.value(id)) sm->addAction(a);
    };
    sub(QStringLiteral("基本"),   {"arc","arc_3p","arc_cpc","arc_cpaa","arc_tpa","arc_tpr","arc_tpl","arc_tph"});
    sub(QStringLiteral("相切"),   {"arc_tan"});
    sub(QStringLiteral("椭圆弧"), {"arc_ea_axis", "arc_ea_1pt"});
}
```

---

## 六、交互状态机与预览

`GraphicsView` 在四处挂接圆弧分支（捕捉配置、鼠标分派、预览、命令提示），核心为 `handleArcSubTool` 的三段式：

```cpp
void GraphicsView::handleArcSubTool(const Vec3& w, QMouseEvent* e) {
    const arc::Mode m      = arc::modeFromString(model_->activeTool());
    const arc::ReqCount rc = arc::requirements(m);
    // Step 1: 收集参考图元（相切延续弧需要）
    if ((int)pickedIds_.size() < rc.refs) { /* pickRefElement + 校验类型 */ return; }
    // Step 2: 收集几何点
    if ((int)pendingPts_.size() < rc.pts) { /* snapWorld + 自动定稿或继续 */ return; }
    // Step 3: 多解工具（如起终点+半径）的选解点击
    arc::Result r = arc::compute(m, pendingPts_, pickedIds_, w, model_->elements(), param);
    if (r.valid) finalizeArcEx(r, model_->activeTool());
}
```

命令提示（prompt）复用圆形子模式同款三段式文案生成逻辑：`请选择第N个参考图元 → 请输入第N个点 → 移动鼠标选择`，由 `arc::requirements` 驱动，新增模式无需改提示代码。

---

## 七、构建中的问题与解决

构建共遇两处编译错误，均属"新代码对既有基础设施的误用"，快速定位修复：

### 问题 1：`math::cosdeg / math::sindeg` 不存在

项目 `Vec.h` 的 `math` 命名空间只提供 `deg2rad/rad2deg`，没有角度三角函数。

```cpp
// 误用
double cs = math::cosdeg(angStart);
// 修正
double cs = std::cos(math::deg2rad(angStart));
```

### 问题 2：`ToolSpec` 聚合初始化跳过中间默认参数

`registerTool({id, name, category, paramMode, dimType, hint})` 带 4 个初始化器时，跳过了 `paramMode` 与 `dimType` 中间默认参数，编译器报错。修复：全部改为**显式构造函数调用**，参数位置清晰无歧义：

```cpp
registerTool({"arc_cpc", QStringLiteral("圆心+点+弦长"),
              ToolCategory::ArcSub, ParamBarMode::Radius, DimensionType::Distance,
              QStringLiteral("圆心+点+弦长：点击圆心和圆周点，左侧输入弦长"});
```

---

## 八、验证结果

| 验证项 | 结果 | 说明 |
|---|---|---|
| 编译 | ✅ 通过 | Qt 6.10.1 / mingw1310_64，5/5 目标全部编译 |
| 链接 | ✅ 通过 | SketchCAD.exe 等全部链接成功 |
| 自测 | ✅ exit 0 | SketchSelfTest.exe 退出码 0，无回归 |
| 运行期验证 | ⏳ 待用户 | 需运行 SketchCAD.exe 实际点选各圆弧模式 |

---

## 九、改动文件清单

| 文件 | 类型 | 改动内容 |
|---|---|---|
| `src/core/ArcModes.h` | 新增 | 11 种模式枚举 + Result + 原子函数声明 + compute/requirements |
| `src/core/ArcModes.cpp` | 新增 | 全部几何实现 + modeFromString/stringFromMode + requirements |
| `src/core/ToolRegistry.h` | 修改 | ArcSub 类别、isArcSubTool、arcSubIds_ 成员 |
| `src/core/ToolRegistry.cpp` | 修改 | 10 个子工具注册、isDrawTool、isArcSubTool |
| `src/core/Commands.h` | 修改 | ArcModeCommand 类声明 |
| `src/core/Commands.cpp` | 修改 | ArcModeCommand::execute + arc_ 前缀注册 |
| `CMakeLists.txt` | 修改 | 加入 src/core/ArcModes.cpp |
| `src/ui/GraphicsView.h` | 修改 | isArcSubTool/handleArcSubTool/finalizeArcEx 声明 |
| `src/ui/GraphicsView.cpp` | 修改 | 捕捉/分派/预览/提示 4 处挂接 + 两函数实现 |
| `src/ui/MainWindow.h` | 修改 | arcSubModes/buildArcMenu 声明 |
| `src/ui/MainWindow.cpp` | 修改 | arcSubModes 数据、下拉工具、顶级菜单、buildArcMenu、草图绘制镜像 |

---

## 十、反思与后续建议

- **最大的设计收益**：圆弧完全寄生在既有的"子模式"机制上，证明该机制对开闭原则的支撑是充分的——11 种功能仅新增 2 个文件 + 扩展 8 处注册点，没有复制任何鼠标/菜单逻辑。
- **诚实的近似**：椭圆弧以等效圆弧显示是当前数据模型的合理边界，需在文档与日志显式标注，避免后续维护者误以为是精确椭圆。
- **建议补充**：现有自测覆盖核心层几何，但尚未覆盖 `GraphicsView::handleArcSubTool` 的端到端鼠标流程。可参照既有测试模式补一个"模拟两次点击 + 断言 Element 入库"的集成测试，堵住 UI 集成回归的漏洞。
- **构建规范的延续**：本次踩的 `math::cosdeg` 与 `ToolSpec` 聚合初始化两坑，本质是"新代码未严格对齐现有接口约定"，已在代码注释中标注，可作为团队 Code Review 的 checklist 项。

---

*生成时间：2026-08-08 · sketch-cad-cpp 圆弧绘制功能 · 设计模式落地 + 端到端集成 · HTML / MD 双格式*
