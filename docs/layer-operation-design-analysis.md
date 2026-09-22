# 图层操作：内容 · 算法 · 实现与遗留问题

> 适用范围：SketchCAD（`sketch-cad-cpp`）图层子系统
> 覆盖需求：①默认 0 层；②切层后屏幕醒目位置实时显示当前层；③新建图元归属当前层并继承其属性（已有图元不变）；④**图层属性变更与已有图元属性解耦**。
> 文档性质：设计/实现总结（含算法推导、代码映射、测试与遗留问题）
> 日期：2026-09-21

---

## 1. 需求与语义总览

| 编号 | 需求 | 语义落点 |
|---|---|---|
| R1 | 系统默认使用 **0 层** | `currentLayer_` 初始为 `"0"`；`clear()` / 读档 / 删除活动层均回落 `"0"` |
| R2 | 切换图层后**屏幕醒目位置实时**显示当前层 | 视口右上角 HUD 徽标（覆盖层，像素坐标，不随缩放漂移）+ 图层面板「●当前」标记 |
| R3 | 新建图元**归属当前层并继承其属性**（快照） | `ElementFactory::create → applyDefaultStyle → applyActiveLayer` |
| R3' | 已有图元属性**保持不变**；单个图元可单独修改 | 属性为"创建时快照"；`styleSelectedEx` 只改单个图元 |
| R4 | **改图层属性**时，已有图元不变，仅此后新建图元应用新属性；改层后编辑已有图元也不自动跟随 | `setLayerStyle` **仅更新图层定义**，不再反向推送；显式重新同步改为独立命令 `applyLayerStyleToElements` |

**核心设计原则：属性"快照化 + 解耦"。** 图元一旦创建，即持有自己的颜色/线型/线宽副本；图层只是"新图元的默认来源"，变更图层定义不会回溯修改任何既有图元。

---

## 2. 数据模型

### 2.1 图层 `Layer`（`src/core/Layer.h`）

```cpp
struct Layer {
    QString   name      = "0";              // 唯一键；默认层固定为 "0"
    QColor    color     = QColor(26,29,40); // 图层默认颜色
    double    lineWidth = 1.0;              // 图层默认线宽（mm）
    LineStyle lineStyle = LineStyle::Solid; // 图层默认线型
    bool      visible   = true;             // 整层显示/隐藏
    bool      locked    = false;            // 整层锁定
    QString   code;                         // GB/T 14665 表6 标识号（"01"~"12"）
    QString   standardName;                 // 表6 标准名（如"粗实线"）

    // 把本层样式写到一个图元：颜色/线型/线宽 + 线宽固定（lineWidthAuto=false）
    void applyTo(Element& e) const;
};
```

- `standardLayers()`：GB/T 14665-2012 表6「图线的分层标识」标准模板，共 **12 层**（01~12），线宽按**当前线组**（`LineWidthGroup` 表1）的粗/细档派生（mm）。
- 图层表存储于 `SketchModel::layers_`（`std::vector<Layer>`），默认层 `"0"` 始终存在。

### 2.2 图元与图层的关系（`src/core/Element.h`）

- `Element::layer`（`QString`，默认 `"0"`）：**单值归属**——一个图元只属于一个图层。
- `Element::lineWidthAuto`（默认 `true`）：`true` = 线宽随**线型**自动派生（`defaultWidthForStyle(lineStyle)`，与图层无关）；`false` = 使用 `Element::lineWidth` 固定值。
  - `effectiveLineWidth() = lineWidthAuto ? defaultWidthForStyle(lineStyle) : lineWidth`。
- 颜色、线型、线宽均为**图元自身的副本**（非对图层的引用）→ 天然支持"解耦"。

> **单位口径**：渲染路径统一按 **mm** 处理（`mmToScreenPx(el.effectiveLineWidth())`），与 `Layer::lineWidth`（标准层按 mm 派生）一致。`Element.h` 中 `lineWidth` 的 `(px)` 注释属**过时表述**（详见 §8）。

### 2.3 活动图层（`SketchModel`）

```cpp
QString currentLayer_ = "0";              // 当前活动（绘制目标）图层
QString currentLayer() const;
void    setCurrentLayer(const QString& name);
// signal
void    currentLayerChanged(const QString& name);
```

---

## 3. 算法

### 3.1 新图元的归属与继承（R3）

唯一收敛点：`ElementFactory::create()`（所有交互绘制工具、命令行绘图、智能标注都经此路径）。

```cpp
Element ElementFactory::create(ElementType t, const QString& name, const SketchModel* model) {
    Element e; e.type = t; e.name = name; e.id = 0;
    if (model) {
        model->applyDefaultStyle(e);   // ① 全局默认色/线型；线宽按线型自动派生
        model->applyActiveLayer(e);    // ② 归属活动层并快照其属性（默认层"0"不覆盖）
    }
    return e;
}

void SketchModel::applyActiveLayer(Element& e) const {
    const QString& cl = currentLayer_;
    const Layer* L = (cl == "0") ? nullptr : findLayer(cl);
    if (!L) { e.layer = "0"; return; }        // 活动层不存在 → 安全回落 0 层
    e.layer       = cl;                       // 归属
    if (L->color.isValid()) e.color = L->color;
    e.lineStyle   = L->lineStyle;             // 快照（值拷贝）
    e.lineWidth   = L->lineWidth;             // 快照（mm）
    e.lineWidthAuto = false;                  // 继承的线宽视为固定值
}
```

**关键点：**
- 默认层 `"0"` **不覆盖**：保留 `applyDefaultStyle` 既有行为（工具栏默认色/线型，线宽自动跟随线型）。
- 非默认层：颜色/线型/线宽三项**一次性快照**，`lineWidthAuto=false`。
- "正在绘制的图元不受影响"：绘制**预览**只读取当前活动层用于渲染预览样式，落库时才走 `create()` 快照——故切换图层不影响任何已落库图元。

### 3.2 图层属性变更（R4，解耦）

```cpp
bool SketchModel::setLayerStyle(const QString& name,
                                const QColor* color, const double* lineWidth, const LineStyle* ls) {
    Layer* l = findLayer(name.trimmed());
    if (!l) { log("图层 %1 不存在", "warn"); return false; }
    if (!color && !lineWidth && !ls) return true;
    beginEdit("修改图层样式 " + name);        // 可撤销
    if (color)     l->color     = *color;      // 仅更新图层定义本身
    if (lineWidth) l->lineWidth = *lineWidth > 0 ? *lineWidth : l->lineWidth;
    if (ls)        l->lineStyle = *ls;
    // 【解耦】不遍历、不修改任何已有图元
    emit layersChanged(); emit modelChanged();
    log("已修改图层 %1 的样式（已有图元属性保持不变）");
    return true;
}
```

- **只改定义、不碰图元**：解耦的唯一实现要点。
- 三项独立（指针判空）：未指定的字段保持原值。
- 因新建图元经 `applyActiveLayer` 快照图层**当前**值 → 改层后新建图元自动获得新属性，无需额外代码。

### 3.3 显式重新同步（逃生舱）

```cpp
int SketchModel::applyLayerStyleToElements(const QString& name) {
    const Layer* l = findLayer(name.trimmed());
    if (!l) return 0;
    int n = 0; for (const auto& e : elems_) if (e.layer == name) ++n;
    if (n == 0) return 0;
    beginEdit("应用图层样式 " + name);
    for (auto& e : elems_) if (e.layer == name) l->applyTo(e);   // 强制写回三项 + lineWidthAuto=false
    emit layersChanged(); emit modelChanged();
    return n;
}
```

- 语义：**用户主动**把图层当前样式套用到本层全部图元（图层面板「应用样式」按钮 / `layer apply <名>`）。
- 与 `setLayerStyle` 的分工：前者是"自动解耦的图层定义变更"，后者是"显式的一次性批量同步"。

### 3.4 归属迁移（移入图层）

```cpp
int  assignSelectionToLayer(const QString& name, bool adoptStyle=false);        // 当前选区
bool setElementLayer(int id, const QString& name, bool adoptStyle=false);
bool setElementLayers(const std::vector<int>& ids, const QString& name, bool adoptStyle=false);
```

- `adoptStyle=false`（默认）：仅改 `e.layer`，**保留图元原有属性**。
- `adoptStyle=true`：改 `e.layer` 并 `l->applyTo(e)`（显式采纳目标层样式）。
- 只遍历传入的 id，其他图元不受影响。

### 3.5 显示 / 锁定控制

```cpp
bool setLayerVisible(const QString& name, bool visible);  // 图层定义 + 本层全部图元 e.visible 同步翻转
bool setLayerLocked (const QString& name, bool locked);   // 仅图层定义（锁定由 UI/命中检测消费）
```

说明：`visible` 是**整层显示控制**，与"属性快照"不同语义——它按设计需要批量翻转本层图元的 `visible`（渲染层直接读 `e.visible`）；`locked` 只置图层标志位。

### 3.6 图元归属一致性维护（增删改层）

| 操作 | 一致性处理 |
|---|---|
| `addLayer` | 名称去空白、非空、查重；`beginEdit`；写 `layers_`；`layersChanged` |
| `renameLayer` | 旧名不可为 `"0"`；同步把 `e.layer == 旧名` 全部改为新名；**若活动层正是被改名者 → `currentLayer_ = 新名` 并 `currentLayerChanged`** |
| `removeLayer` | `"0"` 不可删；其图元 `e.layer` **回落到 `"0"`**（保留各自属性）；**若删除的恰是活动层 → `currentLayer_ = "0"` 并 `currentLayerChanged`** |
| `clear` | `layers_` 重置为仅 `{Layer("0")}`；`currentLayer_ = "0"`；发 `currentLayerChanged`（注：`elems_` 为空时**提前返回**，见 §8） |
| `setLayersFromJson` | 重建 `layers_`；确保 `"0"` 存在；**若活动层不在新表中 → 回落 `"0"`** |

---

## 4. 实现内容（文件 × 接口映射）

| 文件 | 关键接口 | 职责 |
|---|---|---|
| `src/core/Layer.h` | `Layer`、`Layer::applyTo`、`standardLayers()` | 图层数据结构、样式套用原语、GB/T 14665 标准层模板 |
| `src/core/SketchModel.h/.cpp` | `currentLayer/setCurrentLayer`、`applyDefaultStyle`、`applyActiveLayer`、`setLayerStyle`、`applyLayerStyleToElements`、`assignSelectionToLayer`、`setElementLayers`、`setLayerVisible/Locked`、`add/rename/removeLayer`、`layersToJson/setLayersFromJson` | 图层模型与全部算法 |
| `src/core/ElementFactory.cpp` | `create()` 等 | 新图元唯一创建入口；串联默认样式 + 活动层快照 |
| `src/core/Commands.cpp` | `LayerCommand::execute` | `layer` 命令行族（见下表） |
| `src/ui/LayerPanel.h/.cpp` | `makeRowWidget`、`onCurrentRowChanged`、`onCurrentLayerChanged`、`onColorClicked`、`onLineStyleChanged`、`onLineWidthChanged`、`onApplyStyle` | 图层面板：列表/颜色/线型/线宽/显示/锁定/新建/删除/重命名/移入/应用 |
| `src/ui/GraphicsView.cpp` | `drawForeground` 中 HUD 徽标块 | 屏幕右上角当前图层徽标 |

### 命令行（`layer` 族）

| 子命令 | 用法 | 语义 |
|---|---|---|
| `list` | `layer list` | 列出图层表（名/显示/线型） |
| `add` | `layer add <名> [色] [线宽] [线型]` | 新建图层（带默认属性） |
| `delete` | `layer delete <名>` | 删除图层（图元回落 0 层） |
| `rename` | `layer rename <旧> <新>` | 重命名（图元归属同步迁移） |
| `visible` | `layer visible <名> [on\|off]` | 显示/隐藏整层 |
| `lock` | `layer lock <名> [on\|off]` | 锁定/解锁图层 |
| `set` | `layer set <名> <color\|width\|style> <值>` | 改图层定义（**解耦**，不动已有图元） |
| `assign` | `layer assign <名>` | 把选中图元移入该层（保留属性） |
| `apply` | `layer apply <名>` | 把图层样式套用到本层图元（**显式同步**） |

---

## 5. 界面与可视化

### 5.1 当前图层 HUD 徽标（R2，`GraphicsView::drawForeground`）

- **位置**：屏幕右上角，`bx = viewport()->width() - bw - margin`。
- **坐标系**：`p->resetTransform()` 后以**视口像素**作画 → 始终固定在屏幕、**不随缩放/平移漂移**，且不计入 `model_->elements()`（不参与 fit/zoom 适配）。
- **内容**：半透明白底圆角矩形（`rgba(255,255,255,235)`）+ 1px 灰边 + **图层色块**（边长 12，取 `Layer::color`）+ 粗体 13px 文本`当前图层：<名>`。
- **刷新**：`connect(model_, &SketchModel::currentLayerChanged, [this]{ viewport()->update(); })` → 切层即时重绘。

### 5.2 图层面板「●当前」标记（`LayerPanel::makeRowWidget`）

- 行控件：显示勾选框 + 锁 🔒 + 颜色小方块 + *（可选）*蓝色粗体「●当前」+ 图层名。
- `onCurrentRowChanged`：**选中行即设活动层**（`setCurrentLayer`）。
- `onCurrentLayerChanged → refresh()`：标记自动落到新的活动层行（同名短路，无刷新回路）。
- `updateControls` 用 `QSignalBlocker` 回填颜色/线型/线宽下拉，避免回写死循环。

---

## 6. 关键设计决策与理由

1. **快照而非引用（BYOBJECT 语义）**：图元持有属性副本，使"改层不回溯、单图元可独立"成为结构性保证，而非依赖遍历判定的约定。
2. **默认层 `"0"` 特例**：0 层代表"无图层归属/默认绘制"，不覆盖 `applyDefaultStyle`，避免破坏既有"工具栏默认色 + 线宽自动"的默认绘制手感。
3. **解耦落在唯一变更入口**：将"不推送"约束集中在 `setLayerStyle` 一处，使 `applyActiveLayer`（新建继承）无需改动即自动满足 R4。
4. **保留显式同步**：`applyLayerStyleToElements` 作为用户主动"重新套用"的出口，兼顾"想要批量统一"的场景。
5. **活动层与"正在绘制"分离**：活动层只决定"新图元去哪 + 继承谁"，不触碰任何已落库图元。

---

## 7. 测试与验证

| 测试 | Target | 覆盖要点 |
|---|---|---|
| `tests/layer_test.cpp` | `LayerTest` | 默认层；增/删/改名（重名/默认层保护）；assign 保留属性；**R4 解耦**（改色/线宽/线型后已有图元不变 + 改层后新建图元继承新值）；单图元单独编辑；`applyLayerStyleToElements`；显示控制；删层回落；改名迁移；序列化往返；撤销 |
| `tests/layer_current_test.cpp` | `LayerCurrentTest` | 默认活动层 0；默认层新建保持线宽自动；切层后新建继承色/线型/线宽；既有图元不变；**R4 解耦**（改层线宽不波及已有 + 改层后新建继承新线宽）；删/改名活动层同步；拒绝不存在层 |
| `tests/layer_template_test.cpp` | `LayerTemplateTest` | GB/T 14665 标准层 12 项、编号 01~12、线宽按线组派生、新建图纸预置 13 层 |
| `tests/layer_hud_preview.cpp` | `LayerHudPreview` | 渲染 `build/layer_hud_preview.png`：HUD 徽标 + 新图元继承层样式（人工核对） |

**验证结论（本轮）：**
- `LayerTest` 全部通过；`LayerCurrentTest` 26 项全过；`GbSheetTest` / `LayerTemplateTest` 全过；
- `SketchSelfTest` **727 通过 / 0 失败**；`SketchCAD` 主程序重建成功。

---

## 8. 遗留问题与改进建议

### P1 功能对齐类

1. **活动图层不随图纸存盘**
   `MainWindow::writeDrawing` 未写 `currentLayer`；读档 `setLayersFromJson` 仅在活动层不存在时回落 `"0"`，否则沿用内存中的旧值 → 打开图纸后活动层可能与上次状态不符（"粘滞"）。
   *建议*：在图纸 JSON 增加 `"currentLayer"` 字段并读档恢复；`setLayersFromJson` 增加可选入参。

2. **命令行缺"切换活动层"子命令**
   `layer` 族有 `set/assign/apply` 等，但无 `layer current <名>` → 活动层只能通过图层面板点选切换，命令行/脚本无法设置。
   *建议*：新增 `layer current <名>`（调 `setCurrentLayer`）与只读的 `layer current`（回显）。

3. **模板/标准图元绕过活动层（有意设计，需在 UI/文档明示）**
   `ElementFactory::createText()`（标题栏、尺寸数字、视图符号）与各制图模板（飞机/法兰/轴承座/法兰轮毂/户型）、剖面 `Hatch` 等**直接构造图元或指定各自标准图层**，不经 `create → applyActiveLayer`，因此**不继承当前活动层**。
   这是有意设计（它们有确定的标准层归属），但用户若期望"切层后所有新内容都进当前层"，会感到不一致。
   *建议*：在图层面板/HUD 或帮助中说明"模板图元按其标准层生成"。

4. **无 BYLAYER / BYOBJECT 属性级标记**
   当前一律为"独立快照"（BYOBJECT）。用户无法让**某一个**属性"跟随图层"（by-layer）而其余独立。
   *建议*：如需 AutoCAD 式属性跟随，需为 color/lineStyle/lineWidth 各增一个 by-layer 标志位，并在渲染/导出时按需解析图层当前值。

### P2 健壮性 / 一致性类

5. **`clear()` 提前返回**
   `SketchModel::clear()` 在 `elems_.empty()` 时直接 `return`，**不会**重置 `layers_` 与 `currentLayer_`。当前依赖 `applyNewDrawing` 的调用顺序（先 `clear()` 再 `addLayer` 标准层）保证正确；若将来在空模型上直接 `clear()` 期望重置图层表，将不生效。
   *建议*：把"清空"与"重置图层表"解耦为两个显式操作，或让 `clear()` 无条件重置。

6. **`Element.h` 线宽单位注释过时**
   注释写 `lineWidth ... (px)`，但渲染与图层均按 **mm**（`mmToScreenPx(effectiveLineWidth())`）。属文档一致性问题（非功能缺陷），易误导后续维护。
   *建议*：更新 `Element.h` 注释为 mm，并核对所有写入 `lineWidth` 的路径单位统一。

7. **撤销粒度**
   改图层样式/可见性/归属各自压一次撤销（`beginEdit`）；`applyActiveLayer` 的继承发生在**创建时**，与图层操作撤销无强关联；批量 `assign` 记为一次。暂无"图层操作分组撤销/重做"的高级语义。
   *建议*：如需，可在批量图层操作上引入事务分组。

8. **并发入口下的活动层兜底**
   多入口（面板、命令行、脚本）并发操作时，`setCurrentLayer` 拒绝不存在的层；`applyActiveLayer` 在活动层已不存在时安全回落 `"0"`。属安全兜底，无需修复，但值得在测试中固化。

---

## 9. 附录：相关文件与命令速查

**源码**
- `src/core/Layer.h`、`src/core/SketchModel.h/.cpp`、`src/core/ElementFactory.cpp`、`src/core/Element.h`
- `src/core/Commands.cpp`（`LayerCommand`）
- `src/ui/LayerPanel.h/.cpp`、`src/ui/GraphicsView.cpp`

**测试**
- `tests/layer_test.cpp`、`tests/layer_current_test.cpp`、`tests/layer_template_test.cpp`、`tests/layer_hud_preview.cpp`

**构建/运行（本环境）**
```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.10.1\mingw_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cd D:\01-CAD\02-avticationCAD\sketch-cad-cpp\build
ninja -j 1 LayerTest ; ninja -j 1 LayerCurrentTest   # 逐 target，规避 OOM
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QPA_FONTDIR   = "C:\Windows\Fonts"            # 否则文字渲染为豆腐块
cd D:\01-CAD\02-avticationCAD\sketch-cad-cpp          # 测试须从工程根运行
.\build\LayerTest.exe ; .\build\LayerCurrentTest.exe
```

**命令行速查**
```
layer list                                  # 列出图层
layer add 尺寸线 220,40,40 2.0 dashed       # 新建图层
layer set 尺寸线 width 2.5                  # 改图层定义（解耦，不动已有图元）
layer apply 尺寸线                          # 显式把图层样式套用到本层图元
layer assign 尺寸线                         # 选中图元移入该层（保留属性）
layer rename 尺寸线 标注
layer visible 尺寸线 off
layer lock 尺寸线 on
layer delete 标注
```
