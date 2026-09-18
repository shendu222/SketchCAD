# 线型 ↔ 线宽 统一映射配置 —— 设计与实现

> 项目：sketch-cad-cpp（Qt6 二维 CAD 草图模块）
> 范围：默认线宽按线型自动派生 + 手动覆盖 + 全局可维护 + 易扩展
> 标签：`单一数据源` `工厂/单例` `开闭原则` `自动派生` `手动覆盖` `构建通过`

---

## 一、需求拆解

为所有图元建立**线型 → 默认线宽**的映射，并在创建/渲染时自动应用；同时保留手动覆盖能力。规格：

| 线型 | 枚举 | 默认线宽 | 档位 |
|---|---|---|---|
| 实线 | `LineStyle::Solid` | 3px | 粗线 |
| 虚线 | `LineStyle::Dashed` | 2px | 中粗线 |
| 点线 | `LineStyle::Dotted` | 1px | 细线（点线按轻量线型取细） |
| 点划线 | `LineStyle::DashDot` | 1px | 细线 |
| 双点划线 | `LineStyle::DashDotDot` | 1px | 细线 |

额外约束：**全局可维护**（改一处全局生效）、**易扩展新线型**（加一处即可）、创建与渲染双路径自动应用、保留手动覆盖。

---

## 二、架构设计：单一数据源 + 自动/手动双态

### 2.1 统一映射配置 `LineStyleConfig`（单例 / 工厂+注册表）

所有"线型默认线宽"集中在一个单例的映射表里，是全局唯一真源。消费方（创建、渲染、属性面板）只通过 `defaultWidth(ls)` 查询，绝不在各处硬编码 3/2/1。

```cpp
// src/core/LineStyleConfig.h
class LineStyleConfig {
public:
    static LineStyleConfig& instance();
    double defaultWidth(LineStyle ls) const;       // 查询（未知线型安全回落 1px）
    void   setDefaultWidth(LineStyle ls, double px); // 运行时覆盖 → 全局可维护
private:
    LineStyleConfig();
    QMap<LineStyle, double> map_;                  // 线型 → 默认线宽（唯一映射表）
};
```

```cpp
// 构造函数：集中初始化映射（改线宽规格只动这里）
LineStyleConfig::LineStyleConfig() {
    map_[LineStyle::Solid]      = 3.0;   // 粗线
    map_[LineStyle::Dashed]     = 2.0;   // 中粗线
    map_[LineStyle::Dotted]     = 1.0;   // 细线
    map_[LineStyle::DashDot]    = 1.0;   // 细线
    map_[LineStyle::DashDotDot] = 1.0;   // 细线
}
```

### 2.2 图元双态：`lineWidthAuto` 标志 + `effectiveLineWidth()`

`Element` 新增一个布尔标志区分"自动派生"与"手动覆盖"，实际线宽由统一函数给出：

```cpp
// src/core/Element.h
bool lineWidthAuto = true;   // true=随线型派生；false=用 lineWidth 手动值
double effectiveLineWidth() const {
    return lineWidthAuto ? defaultWidthForStyle(lineStyle) : lineWidth;
}
```

> **避免头文件循环依赖**：`Element.h` 与 `LineStyleConfig.h` 互相需要（前者要查映射，后者要 `LineStyle` 枚举）。解法：`Element.h` 仅**前向声明**自由函数 `double defaultWidthForStyle(LineStyle)`，其实现放在 `LineStyleConfig.cpp`。`effectiveLineWidth()` 调用该自由函数，彻底切断循环包含。

---

## 三、自动应用：创建与渲染两条路径

### 3.1 创建时派生（工厂 / 模型默认样式）

```cpp
// SketchModel::applyDefaultStyle —— ElementFactory::create 统一调用
void applyDefaultStyle(Element& e) const {
    e.color = defaultColor_;
    e.lineStyle = defaultLineStyle_;
    e.lineWidthAuto = true;                                  // 线宽随线型派生
    e.lineWidth = LineStyleConfig::instance().defaultWidth(e.lineStyle);
}
```

命令行、工具条、属性面板三条输入通路画出来的图元，样式完全一致（沿用既有 `ElementFactory` 收敛）。

### 3.2 渲染时读取（视图层）

`GraphicsView` 三处原本直接用 `el.lineWidth` 算画笔/描边/碰撞，统一改为 `el.effectiveLineWidth()`：

```cpp
// 包围盒余量、命中描边、正式画笔，三处均改为：
const double effW = el.effectiveLineWidth();
pen.setWidthF(preview ? std::max(1.0, effW) : (highlight ? effW + 1.2 : effW));
```

创建与渲染都走同一函数，保证"线型改变 → 线宽自动跟随"在全链路一致。

---

## 四、手动覆盖与"恢复默认"

| 操作 | 对 `lineWidthAuto` 的影响 | 结果 |
|---|---|---|
| 属性面板改"线宽"下拉 | 置 `false` | 锁定为手动值，之后改线型不再自动变 |
| 属性面板改"线型"下拉（仍自动） | 保持 `true` | 线宽重派生为新线型默认档 |
| 点"恢复默认"按钮 | 置 `true` | 清除手动覆盖，回到线型派生档 |
| 工具栏/命令行改默认线宽 | 改 `LineStyleConfig` 映射 | 全局该线型图元（自动态）统一生效 |

```cpp
// SketchModel::styleSelectedEx —— 选中图元改样式
if (px && *px > 0) { e.lineWidth = *px; e.lineWidthAuto = false; }  // 手动覆盖
if (ls) {
    e.lineStyle = *ls;
    if (e.lineWidthAuto) e.lineWidth = LineStyleConfig::instance().defaultWidth(*ls); // 自动跟随
}
```

属性面板"线宽"行加了一个**"恢复默认"按钮**，且仅在已手动覆盖时可点，避免误操作。

---

## 五、全局可维护 & 扩展新线型

> **扩展一种新线型 = 只改 3 处，其余消费方零改动：**
>
> 1. `Element.h` 的 `LineStyle` 枚举**末尾**追加一个值（整数顺序只增不改，保证老存档兼容）
> 2. `LineStyleConfig` 构造函数 `map_` 里加一行映射，如 `map_[LineStyle::NewX] = 2.0;`
> 3. `toPenStyle` / `lineStyleName` 各加一个 `case`（若需新 Qt 画笔风格）
>
> 创建、渲染、属性面板通过 `defaultWidth(ls)` 自动获得新线型的默认线宽，无需改任何调用点。

**全局可维护**：`setDefaultWidth(ls, px)` 允许运行时覆盖某一线型的默认线宽（例如通过样式设置器调参），改一处映射即作用于所有该线型的自动态图元，无需重编译。

---

## 六、序列化与向后兼容

`elementToJson` 新增 `lineWidthAuto` 字段；`elementFromJson` 读取时：

```cpp
// 旧存档没有 lineWidthAuto → 默认按"手动覆盖"处理，
// 保留老图纸原有的线宽外观，不因新机制突变为派生档位。
e.lineWidthAuto = o.contains("lineWidthAuto") ? o["lineWidthAuto"].toBool(false) : false;
```

---

## 七、改动文件清单

| 文件 | 类型 | 改动 |
|---|---|---|
| `src/core/LineStyleConfig.h` | 新增 | 单例 + 映射查询/覆盖 API + 自由函数声明 |
| `src/core/LineStyleConfig.cpp` | 新增 | 映射表初始化 + defaultWidth/setDefaultWidth + defaultWidthForStyle |
| `src/core/Element.h` | 修改 | 加 `lineWidthAuto` + `effectiveLineWidth()` + 自由函数声明 |
| `src/core/SketchModel.h/.cpp` | 修改 | `applyDefaultStyle` 派生；`defaultWidth/setDefaultWidth` 委托配置；`styleSelectedEx` 自动/手动 |
| `src/ui/GraphicsView.cpp` | 修改 | 渲染 3 处 `el.lineWidth` → `el.effectiveLineWidth()` |
| `src/ui/MainWindow.cpp` | 修改 | 序列化增 `lineWidthAuto`；默认线型变更时回显默认线宽框 |
| `src/ui/PropertyPanel.h/.cpp` | 修改 | 线宽/线型拆分处理；"恢复默认"按钮；同步显示生效线宽 |
| `CMakeLists.txt` | 修改 | 加入 `src/core/LineStyleConfig.cpp` |

---

## 八、验证

| 验证项 | 结果 | 说明 |
|---|---|---|
| 编译 | ✅ 通过 | Qt 6.10.1 / mingw1310_64，5/5 目标全部编译（含 GraphicsView/PropertyPanel/MainWindow/SketchModel） |
| 链接 | ✅ 通过 | `SketchCAD.exe` 等全部链接成功（`[OK] 构建完成`） |
| 自测 | ⏳ 待用户 | 本沙箱环境禁止直接执行构建产物（运行 exe 返回 127），请在本地运行 `build/SketchSelfTest.exe` 确认 |
| 运行期验证 | ⏳ 待用户 | 运行 `build/SketchCAD.exe`：画实线/虚线/点划线，确认线宽分别为粗/中粗/细；改线型看线宽跟随；改线宽看手动锁定 |

---

*生成时间：2026-08-10 · sketch-cad-cpp 线型↔线宽统一配置 · 单一数据源 + 自动/手动双态 · HTML / MD 双格式*
