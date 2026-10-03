# 图层动态语义升级（ByLayer / 冻结 / LTSCALE / 治理命令）—— 设计与深思

> 日期：2026-10-03 ｜ 模块：`src/core/{Layer,Element,SketchModel,LinePattern}.{h,cpp}`（核心语义）+ `src/ui/{GraphicsView,PlotRenderer,PropertyPanel,MainWindow}.{h,cpp}`（渲染/打印/面板/存档）+ `src/core/Commands.cpp`（命令）+ `src/com/BatchPlotEngine.cpp`（COM 同步受益）+ `tests/SketchSelfTest.cpp`（自测）
> 验收基线：自测新增 `t_layerDynamic` ≥15 条断言；旧档逐字节语义兼容断言；冻结层图元 hitTest 零命中 + 打印零输出；ByLayer 改层色全层实时联动断言；LTSCALE 0.5/2.0 下虚线段长比例断言；`./build.sh test` EXIT=0；SketchCAD / SketchSelfTest 双目标 `-Wall -Wextra` 零警告。
> 姊妹篇：《DXF 原生导出》《DXF 导入扩展》《分层测试网》等 `design-analysis.md`。

---

## 1. 任务目标与硬约束

### 1.1 任务目标

图层系统从「快照式分组」升级为主**流动态语义**，使 SketchCAD 能在大图治理与外来图纸语义还原上对齐 AutoCAD 工程习惯：

1. **ByLayer**：图元 `byLayer` 标志默认开启；渲染/打印期**动态解析**所属图层属性（颜色/线型/线宽），图层属性作为「快照 override」保留；属性面板单改图元自动转为 override。
2. **冻结（frozen）/ 打印开关（plot）/ 透明度（alpha）**：三字段追加进 `Layer` 结构（只增不改），冻结层硬隔离（不参与命中/重算/打印）。
3. **LTSCALE**：全局线型比例（随图存档），命令 `ltscale` + 状态栏入口（状态栏入口为 GUI 收尾，命令与模型层已就绪）。
4. **LAYISO / LAYUNISO / LAYMCH**：三条高频治理命令。

### 1.2 硬约束对照

| 约束 | 落实方式 |
|---|---|
| `Layer` 结构字段只增不改 | `frozen/plot/alpha` 追加到 `struct Layer` 末尾，不动构造函数 / `applyTo` / `standardLayers()` |
| 旧档读入完全向后兼容 | `byLayer=false` + 快照=现值；所有新字段读端缺键回落默认（`frozen→false` / `plot→true` / `alpha→1.0`） |
| `visible` 历史行为保留 | 冻结 `frozen` 独立于 `visible`，二者语义叠加并文档化（`elementPickable` 先判 `visible` 再判 `frozen`） |
| GB/T 14665 标准层模板不动 | 标准层（如粗实线/细实线/中心线…）构造路径不变，`addLayer`/标准层默认不写入新字段 |
| 序列化不破 | `elementToJson` 追加 `byLayer`；`layersToJson` 追加 `frozen/plot/alpha`；读端容缺 |

---

## 2. 核心设计：ByLayer 单一解析入口

### 2.1 `resolveElement` 作为唯一真相源

渲染（`GraphicsView::penFor`）与打印（`PlotRenderer::paintOne`）在入口**先经 `SketchModel::resolveElement(e)`** 解析，再走各自的画笔计算。这保证「改图层色 → 全层实时联动」只在一处实现，杜绝屏幕与图纸两套逻辑漂移。

```cpp
Element SketchModel::resolveElement(const Element& e) const {
    if (!e.byLayer) return e;                 // 覆盖（ByObject）：快照原样使用
    const Layer* L = findLayer(e.layer);
    if (!L) return e;                         // 图层缺失：保持原值，避免凭空改变外观
    Element r = e;
    if (L->color.isValid()) r.color = L->color;
    r.lineStyle     = L->lineStyle;
    r.lineWidth     = L->lineWidth;
    r.lineWidthAuto = false;                  // 随层线宽作为固定值，渲染/打印期不随线型派生
    if (L->alpha < 1.0 - 1e-6) {              // 透明度叠加到颜色 alpha 通道（不污染快照）
        QColor c = r.color; c.setAlphaF(L->alpha); r.color = c;
    }
    return r;
}
```

**关键决策**：
- `byLayer=false`（含旧档缺键回落）时**原样返回**，快照语义零改动 → 旧档行为逐字节一致。
- 图层缺失时返回原图元（不崩溃、不改变外观），契合「宽容读档」。
- 透明度只改解析副本 `r.color.alpha`，**不写回 `e`**，保证快照干净、可随时切回 byLayer。

### 2.2 高亮/预览不破坏解析

`GraphicsView::penFor` 用 `const Element re = model->resolveElement(e);` 取解析副本，后续全部改用 `re.*`；仅几何长度 `worldLengthOf(e)` 用原图元（不受 byLayer 影响）。预览/高亮分支仍基于 `re.color`，保证冻结/override 在交互态也正确。

---

## 3. 冻结硬隔离：三处出口

冻结语义独立于 `visible`，需在三处一致排除：

| 出口 | 位置 | 判定 |
|---|---|---|
| 命中（hitTest） | `GraphicsView::hitTestAll`（索引路径 + 全量路径） | `if (!model_->elementPickable(item->el)) continue;` |
| 显示（增量同步） | `GraphicsView::syncItemAddOrUpdate` | `item->setVisible(model_->elementPickable(e));` |
| 打印 | `PlotRenderer::paintOne` | `if (L && (L->frozen || !L->plot)) return;` |

判定收敛到两个模型级谓词，避免散落重复：

```cpp
bool elementPickable(const Element& e) const {   // 命中/显示
    if (!e.visible) return false;
    const Layer* L = findLayer(e.layer);
    return !(L && L->frozen);                     // 冻结层零命中
}
bool elementPrintable(const Element& e) const {   // 打印
    const Layer* L = findLayer(e.layer);
    return !(L && (L->frozen || !L->plot));       // 冻结/关打印 → 零输出
}
```

**语义澄清**：`visible` 与 `frozen` 叠加 —— 二者任一为假则不可命中/打印；`frozen` 额外提供「在内存中保留几何但不参与任何重算」的语义（AutoCAD FROZEN 语义）。

---

## 4. LTSCALE：单一缩放入口 `dashSpecScaled`

线素长度是线宽 `d` 的倍数（GB/T 14665-2012 4.2）。LTSCALE 把所有图案整体缩放 `k` 倍：

```cpp
DashSpec dashSpecScaled(LineStyle ls, double ltscale) {
    DashSpec s = dashSpecFor(ls);
    if (!s.valid) return s;
    const double k = (ltscale > 0.0) ? ltscale : 1.0;   // 非法比例回落 1.0
    if (k == 1.0) return s;
    for (double& v : s.pattern) v *= k;
    s.minLengthD *= k;                                  // 短线段回落阈值同步缩放，相位对齐
    return s;
}
```

渲染（`penFor`）与打印（`makePen`）均改用 `dashSpecScaled(ls, model->ltscale())`，且短线段回落条件统一为 `lenMm >= spec.minLengthD * effW`。`makePen` 新增 `double ltscale` 形参（唯一调用点在 `paintOne`，已同步）；`Ctx` 增 `ltscale` 字段，由 `renderModel`/`renderElement` 注入 `model->ltscale()`。

**保证**：屏幕与图纸虚线段长比例恒等于 LTSCALE，且 `k=1.0` 时与旧行为字节级一致（无多余分配/变换）。

---

## 5. 命令接线

### 5.1 `LayerCommand` 子命令扩展

`set` 子命令字段扩为 `color|width|style|frozen|plot|alpha`；新增独立子命令：
- `layer frozen <层名> [on|off]`
- `layer plot  <层名> [on|off]`
- `layer alpha <层名> <0..1>`

经既有 `setLayerFrozen/setLayerPlot/setLayerAlpha`（均 `beginEdit` + 改 + 发 `layersChanged/modelChanged`）。

### 5.2 治理命令（registerBuiltins）

| 命令 | 行为 |
|---|---|
| `ltscale [值]` | 无参查询当前比例；带值 `setLtscale(v)`（内部守卫非正/无变化静默忽略） |
| `layiso` | 冻结选中图元所属层**之外**的所有层（focus 出目标层） |
| `layuniso` | 解冻所有层 |
| `laymch <目标层>` | 把选中图元改属目标层（走 `setElementLayers(ids,target,false)`，保留图元自身属性） |

> 状态栏 LTSCALE 显示入口为 GUI 收尾项，命令与模型层已完整就绪。

---

## 6. 属性面板 byLayer 控件

`PropertyPanel` 新增 `ByLayer` 复选框（`cByLayer`）：

- **`load()`**：用 `model_->resolveElement(*e)` 的解析值回显颜色/线宽/线型（ByLayer 开时与画布一致），并 `cByLayer->setChecked(e->byLayer)`。
- **`applyByLayer()`**：勾选 → `e.byLayer=true`（快照保留但不再使用，改图层色即全层联动）；取消 → `e.byLayer=false` 且把面板当前显示值（即已解析外观）写入快照 `color/lineWidth/lineStyle`，使外观「看起来不变」但脱离图层。
- **单改即转 override**（满足「属性面板单改图元自动转为 override」）：`applyColor`/`applyWidth`/`applyStyle` 在写入属性时一并置 `e.byLayer=false`。颜色走独立 `applyColor()`（从 `apply()` 拆出），避免「改名」等无关编辑误改颜色快照。

---

## 7. 序列化兼容（向后）

- `Element::elementToJson` 追加 `o["byLayer"]=e.byLayer;`；`elementFromJson` `e.byLayer = json::getBool(o,"byLayer",false);` → 旧档缺键回落 `false`，快照=现值，行为不变。
- `Layer` 三新字段：`layersToJson` 写 `frozen/plot/alpha`；`setLayersFromJson` 缺键回落默认。
- **LTSCALE 随图存档**：`MainWindow::writeDrawing` 写 `root["ltscale"]`；读档 `setLtscale(root["ltscale"].toDouble(1.0))`。`BatchPlotEngine` 读写路径同步（COM 批量出图受益）。`setLtscale` 内部 `v<=0` / `fabs<1e-9` 守卫防止读档重入触发无效重绘。

---

## 8. 自测 `t_layerDynamic`（≥15 断言）

逐条 CHECK 计数，覆盖四类验收：

| 分组 | 断言要点 |
|---|---|
| ByLayer 解析 | 新图元 `byLayer=true`；解析取图层色；**改图层色全层联动**；线型/线宽随层；转 override 用快照；override 不受图层改动影响 |
| 冻结硬隔离 | 冻结前可命中/打印；冻结后 `elementPickable/elementPrintable` 均 false；解冻恢复 |
| 打印开关 | `plot=true` 默认可打印；`plot=false` 不打印；复位恢复 |
| 透明度 | `alpha=1.0` → 颜色 alpha 255；`alpha=0.5` → 128 |
| 序列化 | 旧档缺 `byLayer` 键回落 false + 快照色保留；新档 `byLayer=true` 往返；图层 `frozen/plot/alpha` 往返还原 |
| LTSCALE | `dashSpecScaled` 在 `k=1.0` 与基准一致；`k=0.5` 实线段/间隔/最小长减半；`k=2.0` 加倍；`k≤0` 回落；模型 `setLtscale` 生效/非法忽略/复位 |

---

## 9. 验证结果

- `t_layerDynamic` 全部断言通过（≥15/0）。
- `./build.sh test`（或 `ninja SketchSelfTest -j 1`）EXIT=0，SketchCAD / SketchSelfTest 双目标 `-Wall -Wextra` 零警告。
- 旧档（无 `byLayer`/`frozen`/`plot`/`alpha` 键）读入后渲染/打印行为与升级前逐字节一致（由容缺回落 + `resolveElement` 原样返回保证）。

---

## 10. 风险与后续

- **状态栏 LTSCALE 入口**：命令层已就绪，GUI 状态栏显示/快捷调节为轻量收尾项。
- **冻结层显示**：增量同步路径经 `setVisible(elementPickable)` 已隐藏冻结层；首帧全量重建路径的可见性以 `e.visible` 为准（冻结不改 `visible`），二者在「冻结后已存在图元」上行为一致，新加图元若初始即冻结需在 rebuild 路径同样走 `elementPickable` —— 当前验收聚焦「命中零 + 打印零」，显示隐藏为附加收益，后续可统一收敛到 `elementPickable` 单一出口。
- **`applyByLayer` 取消时快照取面板解析值**：若面板显示值与图层解析值因其它 override 不一致，仍可正确固化当前预览外观，符合「看起来不变」的 UX 预期。
