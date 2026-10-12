# 新建图纸坐标原点居中 —— 设计分析文档

> 任务：新建文档后，中心点（坐标系原点指示器）应在图纸的中央（原先在左下角）。
> 状态：✅ 已完成（构建零警告 + 全量自测通过，见 §6 验证回填）
> 日期：2026-10-12
> 关联文档：`ucs-design-analysis.md`（UCS 基座）、`command-parity-design-analysis.md`（命令体系）

---

## 1. 边界与现状（任务前勘察结论）

### 1.1 "中心点"的确切身份

用户所见"中心点"= `GraphicsView::drawAxes`（:1583）绘制的**坐标系指示器**（X/Y 轴箭头 + O 点），画在 `model_->currentUcs().origin`。默认 UCS=世界系 (0,0)。

### 1.2 图纸坐标现状

- `buildSheetFrame(spec)`（sheet_frame.cpp:85）以**纸面 mm** 从世界 (0,0) 起建图框，**不乘绘图比例**（W=spec.width, H=spec.height 直用）；
- `applyNewDrawing` 末尾 `fitToRect(QRectF(0,0,W,H))` 把整幅图纸适配居中到视口；
- 结果：原点指示器落在**图纸左下角** (0,0)。

### 1.3 关键约束：模板坐标系共享

FlangeViews / FlangeHubViews / HousingViews / SectionDrawing 等视图模板全部声明"**坐标系与 buildSheetFrame() 一致，可直接叠加**"——若把图框平移到原点居中（物理方案），全部模板的落点将错位到纸外，改动面为 5+ 模块及其测试。**否决物理平移方案。**

---

## 2. 关键设计决策

### 2.1 UCS 原点 = 幅面中心（会话态，不动图框几何）

`applyNewDrawing` 末尾：

```cpp
model_->setCurrentUcs(Ucs(QStringLiteral("SHEET_CENTER"),
                          Vec3(spec.width / 2.0, spec.height / 2.0, 0.0), 0.0));
```

- 坐标系图标 / 网格 / 极轴正交 / 键盘坐标输入**全部随 UCS 平移**到图纸中央（UCS 基座既有能力，零新增渲染代码）；
- 图框几何与模板坐标系共享**完全不动**（世界坐标仍从左下角起）；
- UCS 为会话态不随图存档（t_ucs 既有断言"UCS 状态不入存档"）：旧档打开行为不变、多文档各 model 独立实例互不干扰。

### 2.2 配套必改：命令参数坐标 UCS 感知（单一入口扩展）

勘察发现**输入语义分裂**：CommandLine 分段式输入（绘图工具激活时键入坐标）已经 UCS 感知（`parsePointToWorld`，SketchModel.cpp:1307），但**整行命令**（如 `circle 5,5 10`）的参数坐标走 `CommandContext::tryParsePoint` 纯字符串解析——不感知 UCS。若只设 UCS 不改此处，新图纸下"同一坐标两种解释"（键入 0,0 落中央、整行命令 0,0 落左下角）是埋雷。

**改法（单一入口，不散弹）**：

- `CommandContext::tryParsePoint` 从 static 改为**成员函数**（`CommandContext` 本就持有 `model` 指针）：内部先 `tryParsePointRaw`（原 static 纯解析，改名保留）再 `model->toWorld(out)`；
- `parsePoint` / `parsePts` 自动获得感知（走成员版）；
- Commands.cpp 内 9 处 `CommandContext::tryParsePoint(...)` 静态限定调用 → `ctx.tryParsePoint(...)`（全部机械替换，`ctx` 均在作用域）；
- `CommandInterpreter::tryParsePoint` static 转发包装改指 `tryParsePointRaw`（无调用方，语义保留）。

### 2.3 语义边界：位移量绝不能换算

审计 ModifyCommand 全族：`move`/`move_copy`/`duplicate` 的 dx,dy、`offset` 的 dist、`lengthen` 的 delta、rotate/scale 的角度/因子全部 **toDouble 直解**（不经 parsePoint）→ 不受影响 ✅。绝对点语义（rotate/scale 显式中心、mirror point、dim 尺寸线位置、attdef 位置、block create 基点、xref 插入点）全走 parsePoint/tryParsePoint → 统一随 UCS ✅。

### 2.4 回流审计（parsePoint 感知化的安全性）

| 路径 | 结论 |
|---|---|
| 鼠标拾取点 | Vec3 直通 pendingPts_，不经 parsePoint ✅ |
| 宏录制/SCR 回放 | captureLine 记录**用户键入原文**，重放语义=输入时所见（与 AutoCAD 宏语义一致）✅ |
| COM 自动化 | 不路由命令文本（grep 无 execute/parsePoint）✅ |
| 剪贴板粘贴 | 世界坐标直改模型，坐标字符串仅出现在日志 ✅ |
| 既有自测 | 默认 UCS=世界 → toWorld 恒等 → 全部断言零影响 ✅ |

---

## 3. 实现落点

| 文件 | 改动 |
|---|---|
| `src/core/Commands.h` | `tryParsePoint` static → 成员（raw + toWorld）；新增 `tryParsePointRaw` static（纯解析）；注释更新 |
| `src/core/Commands.cpp` | raw/成员两实现；9 处静态限定调用 → `ctx.tryParsePoint` |
| `src/core/CommandInterpreter.h` | static 转发包装 → `tryParsePointRaw` |
| `src/ui/MainWindow.cpp` | `applyNewDrawing`：setCurrentUcs(幅面中心) + 日志补充"坐标原点已置于图纸中央" |
| `tests/SketchSelfTest.cpp` | 新增 `t_ucsSheetCenter()`（11 CHECK 五组）+ main 注册 |

---

## 4. 行为规格（改后）

| 场景 | 行为 |
|---|---|
| 新建图纸（菜单/启动页/更多模板，三路同经 createTemplatedDrawing→applyNewDrawing） | 坐标系图标/网格基准在图纸中央；键入 (0,0) = 图纸中心 |
| 命令参数坐标（整行命令） | 按当前 UCS 解释（与世界系默认时行为完全一致） |
| 位移/长度/角度/因子参数 | 不受 UCS 影响（纯增量） |
| 打开旧档 | UCS=世界系，行为与改前完全一致 |
| 空白图（无图框） | 不设 UCS（无幅面可取中心），维持世界系 |
| 保存/重开 | UCS 会话态不入档，重开后回到世界系（既有设计，见 §7） |

---

## 5. 约束与风险核对

| 风险点 | 核对结果 |
|---|---|
| 模板坐标系共享 | 不动图框几何 → 零影响 |
| 既有自测回归 | 默认世界系恒等 + 全量回归验证 |
| LIMITS 界限检查 | 无 LIMCHECK 拦截逻辑（limmax 仅 DWG 头部导出），负坐标输入无阻碍 |
| UCS 入档 | 既有设计明确不入档（t_ucs 断言），本次不改变 |
| setCurrentUcs 信号 | 发 ucsChanged+viewChanged → 图标/网格/状态栏自动联动（既有接线） |

---

## 6. 验证方案与证据回填

### 6.1 t_ucsSheetCenter（11 CHECK 五组）

1. **整行命令随 UCS**：A4 横向中心 (148.5,105)；`line 0,0 100,100` → 世界端点 (148.5,105)-(248.5,205)；`circle 0,0 10` → 圆心=中心、半径不变；`point 10,20` → 世界 (158.5,125)。
2. **两路输入同源**：`parsePointToWorld("0,0")` = 幅面中心。
3. **raw 纯解析回归**：`tryParsePointRaw("3,4")` 原值返回。
4. **世界系恒等**：默认 UCS 下 `line 5,5 15,15` 端点原值（既有行为零回归）。
5. **位移量不受 UCS**：`move 10 0` 为纯增量。

### 6.2 构建与全量回归

```
构建：双目标 SketchCAD + SketchSelfTest，-Wall -Wextra 零警告（grep warning|error|FAILED 计数 0）
全量自测：2390 通过 / 0 失败 / EXIT=0（基线 2375 + 新增 15）
```

### 6.3 过程中发现并修复的回归（UCS 定义参数）

首轮自测暴露 1 条 t_ucs 既有断言失败（`ucs z: 原点(5,5) 旋转 30°`）：`ucs` 命令的**定义锚点**（3p/z/origin）原本走 parsePoint，感知化后被**当前 UCS** 换算，产生"用 UCS 定义 UCS"的链式歧义。

**修复**：UCS 定义锚点一律改 `tryParsePointRaw`（世界系语义，见 Commands.cpp UcsCommand 三处 + 注释）。设计依据：定义坐标系是坐标管理层操作，锚点与世界系读数一致；`ucs object` 取图元世界坐标、`ucs view/world` 无点参数，语义天然世界系。修复后 t_ucs 42/0 全过。

同类排查：`limits` 仅走 GUI 对话框（无 CLI 点解析）✅；`break_at` 的投影点是绘图操作、UCS 感知正确（保留）✅。

---

## 7. 已知限制

| 限制 | 说明 | 后续 |
|---|---|---|
| UCS 不入档 | 保存后重开，原点回到左下角（会话态设计） | 若需持久化，需扩展序列化 + 向后兼容键，另行立项 |
| 整行命令不支持 @相对/极坐标 | `parsePointToWorld` 支持 `@dx,dy`/`d<a` 语法，`CommandContext::tryParsePoint` 仅绝对坐标（改前改后一致，非本次引入） | 需要时扩展 |
| rotate/scale 显式中心令牌数门槛 | `rotate 90 0,0`（3 令牌）被 `size>=4` 门槛拒绝、`rotate 90 0 0`（4 令牌）因单令牌解析失败——显式中心分支疑似永不命中（既有缺陷，非本次引入） | 建议单独核实修复 |
| mirror point/line 同类门槛缺陷 | `mirror point 10,0`（3 令牌）被拒、`mirror point 10 0` 因解析失败——CLI 形式疑似不可用（GUI 镜像工具走 modify::makeMirror 直调不受影响） | 建议单独核实修复 |

---

## 8. 结论

- 用户可见效果：新建图纸后坐标系原点（中心点）在图纸正中央，键盘坐标输入以中心为原点，画对称零件体验符合直觉。
- 架构上复用 UCS 既有基座（图标/网格/输入/读数全链路 UCS 感知），只补齐了"命令参数坐标"这最后一道转换边界，收敛到 `CommandContext` 单一入口。
- 世界系下一切行为恒等，旧档/空白图/既有测试零影响。
