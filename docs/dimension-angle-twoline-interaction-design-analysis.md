# 角度标注交互改造：从「选三个点」改为「选两条相交直线」设计分析

> **任务**：将角度标注的交互方式由「依次拾取三个点（顶点 + 两边上的点）」改为「依次选中两条相交直线」，系统自动以两直线交点为顶点、计算并生成夹角标注；两直线不相交时给出明确错误提示。
> **状态**：✅ 已实现并验证（双目标零警告，全量自测 2515/0，其中 `t_dimAngleDrive` 21/0）
> **日期**：2026-10-15
> **关联文档**：`docs/dimension-angle-drive-design-analysis.md`（需求 D 总览：角度标注 + 尺寸驱动主开关）

---

## 一、思考过程（Thinking）

### 1.1 需求拆解

| 需求点 | 实质 | 落点 |
|---|---|---|
| 交互方式变更 | 「选三个点」→「选两条相交直线」 | `GraphicsView::handleDimension` 的 Angle 分支重做 |
| 自动求顶点 | 以两直线交点为顶点 | `geo::intersectElements` |
| 数值准确、样式一致 | 沿用既有角度计算与国标标注生成器 | `measureDimension` / `dim::build`（未改动） |
| 不相交明确报错 | 两条线无交点时提示且不创建 | `ips.empty()` 分支 |

### 1.2 现状核查（代码实况证据）

| # | 事实 | 证据 |
|---|---|---|
| 1 | 原交互在 `GraphicsView::handleDimension` 用 `pendingPts_` 三点拾取：顶点→边1点→边2点，凑满 3 点即建标注 | `GraphicsView.cpp`（改造前）Angle 分支 `pendingPts_.push_back(w)` + `size()==1/2` 提示 |
| 2 | 命令路径 `dim angle` 此前同时支持「选中两线」与「命令行三点」，三点为兜底 | `Commands.cpp` Angle 分支 `else if (ctx.toks.size() < 5 || !ctx.parsePoint(...))` |
| 3 | 既有 `geo::intersectElements(a,b)` 对两直线走折线×折线分支，共享端点场景返回该端点，正是「选两线求顶点」所需 | `Geometry.h:130`；`t_dimDriven` ③ 已验证坐标锚定角度驱动可行 |
| 4 | `pickedIds_` 为 `std::vector<int>`（修剪/延伸/约束的两步拾取缓存），可安全复用为「两线选择」缓存，与 `pendingPts_` 互不干扰 | `GraphicsView.h:247` |
| 5 | `updatePreview` 在 `pendingPts_` 为空时直接 `clearPreview()` 返回（`GraphicsView.cpp:2630`），故角度选择阶段不写 `pendingPts_` 也不会引发预览崩溃 | `GraphicsView.cpp:2630` |
| 6 | 角度计算 `measureDimension` 已存在且正确：`fabs(wrapDeg180(a2 - a1))` 恒取 ≤180° 角 | `Geometry.cpp:484` |

### 1.3 方案比选

**选定方案：交互层（画布）改为两线两步拾取 + 命令层移除三点兜底，统一为「选两条相交直线」。**

- **交互层**：把 Angle 分支从「`pendingPts_` 三点」改为「`pickedIds_` 两线」——
  - 第 1 击命中直线 → 存入 `pickedIds_[0]`，提示选第 2 条；
  - 第 2 击命中另一条直线 → 求交 → 取远端端点 → 建标注。
  - 优点：与用户心智一致（角度天然由两条边线定义），去掉「在边上找点」的别扭操作。
- **命令层**：`dim angle` 去掉三点 fallback，要求恰好选中两条相交直线；否则明确报错。
  - 优点：两条入口语义统一，不再保留易混淆的旧三点形式。

**否决方案**：
- 保留三点作为「备用」：与用户「改为两线」的明确要求冲突，且会造成两条交互并存、文档与提示混乱。
- 用「延长线相交」特判：超出本次范围，且 AutoCAD 惯例下角度标注本就要求两线实际相交（含共享端点），不做延长线特判即可满足需求。

### 1.4 关键决策依据

1. **复用 `geo::intersectElements` 而非自写交点**：该函数对共享端点、真实交叉均返回正确交点，且已被 `t_dimDriven` 验证；自写会重复几何代码并引入回归。
2. **远端端点作边点（`p1/p2`）**：取「远离交点的那端」，使标注 `pts[2]` 与第二条直线的远端端点坐标重合——这正是 `driveDimension` 角度分支能「免改内核就旋转该线」的前提（见 2.6）。
3. **用 `pickedIds_` 而非 `pendingPts_`**：`pickedIds_` 是既有的两步拾取缓存，`std::vector<int>`；角度选择期间不写 `pendingPts_`，既避免与通用标注预览逻辑相互污染，又因 `updatePreview` 对空 `pendingPts_` 直接返回而天然安全。
4. **不相交即报错且不创建**：`ips.empty()` 时 `resetPending()` 清空 `pickedIds_` 让用户重选，绝不残留半成品状态。

---

## 二、相关算法

### 2.1 两线拾取交互流程（画布）

```
用户激活角度标注工具 → 每次点击触发 handleDimension(w, Angle)：
  第 1 击：hitTest 命中直线 L1 → pickedIds_ = [L1]；提示「已选第 1 条直线，请点选第 2 条相交直线」
  第 2 击：hitTest 命中直线 L2（且 L2 ≠ L1）：
       intersection = geo::intersectElements(L1, L2)
       若空 → 报错「所选两条直线不相交」，resetPending()，结束
       v  = intersection.front()                         // 顶点
       p1 = L1 远离 v 的端点                           // 边1点
       p2 = L2 远离 v 的端点                           // 边2点
       建 Dimension{dtype=Angle, pts={v,p1,p2}} → commitElement
```

### 2.2 交点求取

```cpp
// Geometry.h:130
std::vector<Vec3> intersectElements(const Element& a, const Element& b);
```
- 两直线走折线×折线分支（`segSegIntersect`），对共享端点返回该端点；
- 平行/不相交返回空 `vector` —— 据此判定「不相交」。

### 2.3 顶点 / 边点提取

```cpp
const Vec3 v  = ips.front();
const Vec3 p1 = (a->pts[0].distTo(v) < 1e-6) ? a->pts[1] : a->pts[0];
const Vec3 p2 = (b->pts[0].distTo(v) < 1e-6) ? b->pts[1] : b->pts[0];
```
要点：比较两线各自的两个端点与顶点的距离，取**远离顶点**的端点为边点，保证 `p1/p2` 与两直线实际端点坐标一致（驱动时方能联动，见 2.6）。

### 2.4 夹角计算（沿用既有，未改动）

```cpp
// Geometry.cpp:484  DimensionType::Angle 分支
case DimensionType::Angle: {
    if (e.pts.size() < 3) return 0.0;
    const double a1 = segAngleDeg(e.pts[0], e.pts[1]);   // 向量 (p1 - v) 方向角
    const double a2 = segAngleDeg(e.pts[0], e.pts[2]);   // 向量 (p2 - v) 方向角
    return std::fabs(wrapDeg180(a2 - a1));               // 恒取 ≤180° 夹角
}
```
两向量方向角之差经 `wrapDeg180` 归一后取绝对值，**永远给出不大于 180° 的夹角**，再经 `dim::build` 追加 `°` 与国标样式。

### 2.5 不相交报错

```cpp
const auto ips = geo::intersectElements(*a, *b);
if (ips.empty()) {
    model_->log(tname + QStringLiteral("：所选两条直线不相交，无法标注夹角"), "warn");
    resetPending();
    return;
}
```
命令路径（`dim angle`）同源：`sel.size()!=2` 或两线不相交均 `return false` 并 `warn`。

### 2.6 角度驱动联动（坐标锚定，复用既有内核）

`driveDimension` 角度分支只把标注 `pts[2]` 绕 `pts[0]` 旋转到目标角，再经 `collectLinked(pts[2], 1e-6)` 把与 `pts[2]` 坐标重合的端点一并旋到新位置。由于 2.3 已让 `pts[2] == L2 的远端端点`，驱动角度时该直线随动——**零改内核即获得夹角驱动能力**，回归面最小。

---

## 三、实现细节（逐文件）

### 3.1 `src/ui/GraphicsView.cpp` — `handleDimension` Angle 分支（画布交互）

```cpp
// ---- 角度标注：选两条相交直线，自动以交点为顶点生成夹角标注 ----
if (dt == DimensionType::Angle) {
    const int hit = hitTest(mapFromScene(QPointF(w.x, w.y)));
    if (pickedIds_.empty()) {
        if (hit < 0) { model_->log(tname + QStringLiteral("：请点选第一条直线"), "warn"); return; }
        const Element* e0 = model_->element(hit);
        if (!e0 || e0->type != ElementType::Line || e0->pts.size() < 2) {
            model_->log(tname + QStringLiteral("：请点选直线图元作为第 1 条边"), "warn"); return;
        }
        pickedIds_.push_back(hit);
        model_->log(tname + QStringLiteral("：已选第 1 条直线 #%1，请点选第 2 条相交直线").arg(hit));
        return;
    }
    if (hit < 0) { model_->log(tname + QStringLiteral("：请点选第 2 条直线"), "warn"); return; }
    const Element* b = model_->element(hit);
    if (!b || b->type != ElementType::Line || b->pts.size() < 2) {
        model_->log(tname + QStringLiteral("：请点选直线图元作为第 2 条边"), "warn"); return;
    }
    bool dup = false;
    for (int id : pickedIds_) if (id == hit) dup = true;
    if (dup) { model_->log(tname + QStringLiteral("：请勿重复选择同一条直线，请点选另一条直线"), "warn"); return; }
    const Element* a = model_->element(pickedIds_.front());
    if (!a) { resetPending(); return; }
    const auto ips = geo::intersectElements(*a, *b);
    if (ips.empty()) {
        model_->log(tname + QStringLiteral("：所选两条直线不相交，无法标注夹角"), "warn");
        resetPending();
        return;
    }
    const Vec3 v  = ips.front();
    const Vec3 p1 = (a->pts[0].distTo(v) < 1e-6) ? a->pts[1] : a->pts[0];
    const Vec3 p2 = (b->pts[0].distTo(v) < 1e-6) ? b->pts[1] : b->pts[0];
    Element el = ElementFactory::create(ElementType::Dimension, tname, model_);
    el.dtype = dt;
    el.standard = model_->dimensionStandard();
    el.pts = {v, p1, p2};
    const dim::Geom g = dim::build(el);
    if (!g.valid) { model_->log(tname + QStringLiteral("：几何退化，无法标注"), "warn"); resetPending(); return; }
    const int id = commitElement(el);
    model_->log(QString("已创建%1 #%2 = %3").arg(tname).arg(id).arg(g.label));
    resetPending();
    return;
}
```

### 3.2 `src/core/Commands.cpp` — `dim angle` 命令（移除三点兜底）

```cpp
} else if (dt == DimensionType::Angle) {
    // 角度标注：选两条相交直线，自动以交点为顶点、两线远端端点为边点。
    const auto sel = m->selection();
    if (sel.size() != 2) {
        m->log(QStringLiteral("dim angle: 请先选中两条相交直线"), QStringLiteral("warn"));
        return false;
    }
    const Element* a = m->element(sel[0]);
    const Element* b = m->element(sel[1]);
    if (!a || !b || a->type != ElementType::Line || b->type != ElementType::Line
        || a->pts.size() < 2 || b->pts.size() < 2) {
        m->log(QStringLiteral("dim angle: 所选两个图元必须都是直线"), QStringLiteral("warn"));
        return false;
    }
    const auto ips = geo::intersectElements(*a, *b);
    if (ips.empty()) {
        m->log(QStringLiteral("dim angle: 所选两条直线不相交，无法标注夹角"), QStringLiteral("warn"));
        return false;
    }
    const Vec3 v  = ips.front();
    const Vec3 p1 = (a->pts[0].distTo(v) < 1e-6) ? a->pts[1] : a->pts[0];
    const Vec3 p2 = (b->pts[0].distTo(v) < 1e-6) ? b->pts[1] : b->pts[0];
    e.pts = {v, p1, p2};
}
```

### 3.3 用法 / 帮助文本

```cpp
// 原："      dim angle 顶点 边1点 边2点 | dim angle（先选中两条相交直线） | ..."
// 改：
m->log("      dim angle（先选中两条相交直线） | dim radius|dia <圆id> [引出角度]", "warn");
```

### 3.4 自测 `tests/SketchSelfTest.cpp` — `t_dimAngleDrive` 新增场景 ⑤

```cpp
// ---- ⑤ 两直线不相交 → 明确错误提示，不创建标注（命令路径）----
{
    SketchModel m;
    const int lidA = addLine(m, 0, 0, 30, 0);    // 水平线 y=0
    const int lidB = addLine(m, 0, 10, 30, 10);  // 水平线 y=10（平行，不相交）
    m.setSelection({lidA, lidB});
    CommandInterpreter interp(&m);
    CHECK(!interp.execute(QStringLiteral("dim angle")), "⑤ 两直线不相交 → dim angle 失败并报错");
    int cnt = 0;
    for (const auto& e : m.elements())
        if (e.type == ElementType::Dimension && e.dtype == DimensionType::Angle) cnt++;
    CHECK(cnt == 0, "⑤ 不相交时不创建角度标注图元");
}
```
（①② 两线建角/默认开驱动、③ 关开关仅显示、④ 默认开校验 此前已覆盖，本次 +2 断言至 21/0。）

---

## 四、自测验证

| 项 | 结果 |
|---|---|
| 双目标构建（`SketchCAD` + `SketchSelfTest`） | `-Wall -Wextra` **零警告、零错误**，`BUILD_EXIT=0` |
| 全量自测 `SketchSelfTest.exe` | **EXIT=0，通过 2515 / 失败 0** |
| `t_dimAngleDrive`（含 ⑤ 不相交报错） | **21 / 0**（较改造前 +2） |
| 既有 `t_dimDriven`（尺寸驱动） | 23 / 0，无回归 |

---

## 五、已知限制 / 后续

1. **不相交判定为「真实相交」**：仅延长线相交但不共段的两线（折线分支）当前不特判，报「不相交」；如需延长线相交，需扩展 `intersectElements` 或在此处加延长求交。
2. **无实时预览**：角度选择阶段 `pendingPts_` 为空，`updatePreview` 直接清预览，第一条直线选定后不显示橡皮筋预览（仅日志提示）。如需选中第一条线即高亮/预览，可在第一步后将 `pickedIds_` 同步进预览分支（后续增强）。
3. **哪条线为「边2（`pts[2]`）」由选择顺序决定**：影响驱动时被旋转的是后选中的第二条线；纯测量不受影响。如需「双向驱动」或指定基准边，是独立的后续项。
4. **三点形式已彻底移除**：交互与命令统一为「选两条相交直线」，旧 `dim angle 顶点 边1点 边2点` 命令行形式不再支持（文档 `dimension-angle-drive-design-analysis.md` 已知限制 #4 已同步）。
