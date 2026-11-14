# DWG 字体显示对齐修复报告（对齐中望CAD「长仿宋体」）

- **任务**：DWG 打开字体显示不一致——以中望CAD 为基准（长仿宋体），本程序打开同一批 DWG 时字体不再是长仿宋体。
- **测试图纸**：`D:\01-CAD\02-avticationCAD\sketch-cad-cpp\AWX_CAD_Lib\施工图纸库`（15 个 DWG）。
- **结论**：已完成修复并通过验证，本程序打开该批图纸的中文字体观感与中望CAD 一致（均为 GB/T 14691 长仿宋体），且不影响其他含西文/黑体图纸的显示。

---

## 一、确认的错误内容

**现象**：用中望CAD 打开施工图纸库的 DWG，图面中文（尺寸标注、技术要求、标题栏等）显示为「长仿宋体」；用本程序打开同一批 DWG，中文显示为**宋体 / 系统默认字体**，与中望CAD 不一致。

**根因（已实测确认）**：
这批 DWG 经 LibreDWG 桥 `dwg2dxf --as r2004` 转为 DXF 后，其 STYLE（文字样式）记录存在以下特征：
- **组 4（主字体文件名）与组 3（大字体文件名）大多为空**；
- 真实字体名（如 `SimSun`、`FangSong_GB2312`）只存在于 **XDATA（组 1001=`ACAD` / 组 1000=`SimSun`）** 中——这是 LibreDWG 转 r2004 的实测形态（已对全部 15 个 DWG 逐一转档并解析确认）。

旧实现 `cadfmt::textStyleFontFamily()` 的处理逻辑是：
- 把 CJK SHX 形字体（hztxt 等）一律映射到 `SimSun`（宋体）；
- 其余情形原样透传字体名。

因此，当 STYLE 记录里实际携带的是中文宋/仿宋系字体名（`SimSun`、仿宋等）或 CJK SHX 时，本程序渲染为**宋体/系统默认**，而非中望CAD 采用的工程标准汉字字体**长仿宋体**，观感偏差由此产生。

**实证分布（15 个 DWG 转 r2004 后 STYLE 字体真值）**：
| 占比 | 样式名示例 | 组4 | 组3 | XDATA | 旧实现结果 | 中望基准 |
|---|---|---|---|---|---|---|
| 12/15 | `Standard`/`ST`/`Annotative` | 空 | 空 | `SimSun` | 宋体/系统默认 | **长仿宋体** |
| 1 | `HZ`(环控原理图) | `hztxt.shx` | `txt.shx` | — | 宋体 | **长仿宋体** |
| 1 | `丝印字符`(系统布置) | — | `simhei.ttf` | `SimHei` | 黑体(正确) | 黑体 |
| 1 | `普通`(外电网监测箱) | — | `tahoma.ttf` | `Tahoma` | Tahoma(正确) | Tahoma |

可见错误集中在「中文宋/仿/楷系 + CJK SHX → 应归一为长仿宋体」这一路径；西文与黑体本就应保持原样，修复时不能破坏。

---

## 二、定位（涉及的程序位置）

字体从 DWG 到屏幕的解析渲染链路如下，修复点落在第 5 环：

1. `DwgBridge::dwgToDxf` → 调用 `dist/dwg2dxf.exe <in.dwg> -y --as r2004 -o <out.dxf>`；
2. `cadfmt::fromDxf` 回读 DXF；
3. `parseDxfTokens`（TABLES 段）解析 STYLE 记录，对字体名做 **组4 → 组3 → XDATA 1000** 三级 fallback，得到原始字体名 `ff`；
4. `sty.fontFamily = textStyleFontFamily(ff)` —— **核心归一函数（CadFormats.cpp）**；
5. `MainWindow::addTextStyle` 将样式注册进模型 STYLE 表；
6. 文本图元 `applyStyle` 经组码 7 设 `e.styleName`；
7. `resolveTextStyle(e, model)` 取样式 → `GraphicsView` / `PlotRenderer` 经 `f.setFamily(...)` 落字。

> 注：`main.cpp` 已注册 `QFont::insertSubstitution("长仿宋体", 仿宋/FangSong/华文仿宋/STFangsong/宋体/SimSun)`，使「长仿宋体」能稳定解析到窄体宋真实字体（走真实度量而非估算）。此为前提条件，本任务未改动。

---

## 三、修改范围

**1. 核心修复（单文件）** `src/core/CadFormats.cpp`
- 新增 `cjkFontToLongFangSong(const QString& fam)`：
  - 空 → 长仿宋体；已是长仿宋体 → 原样；
  - 黑体/西文（`simhei`/`stheiti`/`microsoft yahei`/`arial`/`tahoma`… 及中文黑体系）→ **保持不变**；
  - 宋/仿/楷系（含 `simsun`/`宋`/`fang`/`仿`/`kai`/`楷`/`stsong`…）及任何仍含 CJK 汉字的字体名 → **长仿宋体**。
- 改写 `textStyleFontFamily(const QString& fontFileRaw)`：
  - 空 → 长仿宋体（GB/T 14691 默认）；
  - CJK SHX 形字体（`hztxt`/`gbcbig`/`gbenor`/`chineset`/`fs`…）→ 长仿宋体；
  - `ttf/ttc/otf` 去扩展名、`无扩展名族名`（XDATA `SimSun` 即属此类）原样透传后交给 `cjkFontToLongFangSong`；
  - 其余（纯西文形字体 `txt.shx` 等）→ 回落默认（非中文，不影响）。

**2. 未改动项（经实测确认无需改）** `parseDxfTokens` 的 STYLE 解析
- 前序曾假设真实 DWG 存在「组3=`100` 占位符、真实字体仅存 XDATA」导致需改此处；**实测 15 个 DWG 均无此异常**（所有 `g3/g4` 为空或真实文件名，XDATA 正常被三级 fallback 读取）。故 `parseDxfTokens` 保持原样，避免连带风险。

**3. 回归测试** `tests/SketchSelfTest.cpp`
- `t_dwgStyleFontLongFangSong()`：新增「XDATA-only SimSun」真实施工图纸形态用例（`组4/组3` 空、仅 `1001=ACAD/1000=SimSun`），样式数断言 6→7；覆盖 宋体/仿宋/西文/黑体/SHX/空字体 六类归一，断言中文→长仿宋体、西文/黑体不变。
- `t_dwgRealStyleLongFangSong()`：端到端用例，把真实 DWG 经 `dwg2dxf --as r2004` 转出的 DXF 直接喂 `fromDxf`，断言 `Standard` 样式 `fontFamily=="长仿宋体"` 且文本图元确实引用 `Standard`（否则渲染会回落系统默认）。样本缺失则跳过（不视为失败），由确定性合成用例兜底。
- `main()` 注册上述两用例。

**影响面**：仅作用于 DWG/DXF 导入时的字体名归一；不影响既有草图编辑、导出、其他语言图纸。

---

## 四、验证结果

| 验证项 | 结果 |
|---|---|
| 双目标构建（`-j1 -Wall -Wextra`） | BUILD EXIT=0，**零警告**（仅 SketchSelfTest 重编链接） |
| 单元测试 `SketchSelfTest` | **通过 3421 / 失败 0，EXIT=0**（较修复前 +5 条新断言） |
| 真实图纸端到端（15 个 DWG 转 r2004） | `Standard`/`ST`/`Annotative` 样式均归一为「长仿宋体」，文本实体组码7 大量引用 `Standard` → 全链路闭合 |
| dist 分发同步 | `cmp build/SketchCAD.exe dist/SketchCAD.exe` → IDENTICAL（dist 已含修复） |
| 临时验证产物清理 | `build/scan_*.dxf` 已清理 |

**结论**：本程序打开施工图纸库 DWG 的中文观感已与中望CAD 一致（长仿宋体），西文/黑体图纸不受影响。修复闭环，符合用户「确认的错误内容 / 修改范围 / 验证结果」交付要求。

---

## 五、遗留与建议

- 当前「长仿宋体」通过 `QFont::insertSubstitution` 回落到仿宋/宋体窄体实现；后续若需像素级还原中望CAD 的字体铺排，可评估内置长仿宋体 TTF 资源并按 `gbLongFangSongStyle()` 显式装载。
- 该归一逻辑与 `gbLongFangSongStyle()`、`resolveTextStyle` 已共同构成「长仿宋体」单一事实来源，新增文字样式相关需求时请复用，勿在别处硬编码字体名。
