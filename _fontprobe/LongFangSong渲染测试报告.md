# GB/T 14691 长仿宋体默认样式（LongFangSong）渲染实测报告

- 测试对象：`SketchCAD` 的 `LongFangSong` 默认文字样式（`src/core/TextStyle.{h,cpp}`）
- 测试范围：新建文字（Text/MText）、标题栏、图框、尺寸标注 四类元素的字体显示与字宽表现
- 覆盖维度：字号 3.5 / 5 / 7 mm；字重 常规 / 粗体；字符集 中文 / 英文 / 数字 / 符号（Φ±Δ℃①②③°≈≠）
- 测试日期：2026-11-11

---

## 1. 测试方法与环境

### 1.1 方法
由于沙箱无交互式显示会话，本测试采用**与程序真实绘制路径完全等价**的渲染探针（`_fontprobe/main.cpp`）：
- 复刻 `GraphicsView::paintText`（及 `paintDimension`）的 `QFont` 设置：
  - `f.setPixelSize(max(8, round(mm * zoom)))`（zoom=20 px/mm）
  - `if (!family.isEmpty()) f.setFamily(family)`
  - `f.setStretch(round(widthFactor * 100))`  —— LongFangSong 即 `0.707 → stretch 71`
- 通过 `QFontDatabase::addApplicationFont` **显式加载真实系统字体文件**（`C:\Windows\Fonts\` 下的 `simfang.ttf` 仿宋、`STFANGSO.TTF` 华文仿宋、`simsun.ttc` 宋体、`msyh.ttc` 微软雅黑等），在 offscreen 平台下渲染中文，规避了 headless 环境字体枚举为空的问题。
- 用 `QFontInfo` 打印**字体回退真相**，并用像素级紧致包围盒测量「中」字的**真实宽高比**，量化对比 GB/T 14691 目标 `字宽 ≈ h/√2 ≈ 0.707`。

### 1.2 环境
- Windows 11；Qt 6.10.1 (mingw_64)；MinGW 13.1
- 系统**已安装**字体：仿宋(simfang)、华文仿宋(STFangsong)、宋体(simsun)、微软雅黑(msyh) 等
- 系统**未安装**：`长仿宋体`（这是本次测试暴露的头号问题，见 §3.1）

---

## 2. 核心结论速览

| # | 发现 | 严重度 | 是否达标 |
|---|------|--------|----------|
| 1 | 系统无「长仿宋体」字体，Qt 回退到「仿宋」（`exactMatch=0`） | 中 | 名义不达标 |
| 2 | **字宽被双重压缩**：仿宋本身已≈0.707，再 `stretch71` → 实测 0.511（比国标更窄） | **严重** | ❌ 不达标 |
| 3 | **尺寸标注未接入 LongFangSong**（代码级确认：不调 `setFamily`/`setStretch`） | **严重** | ❌ 需求缺失 |
| 4 | 字号下限 3.5mm 渲染清晰；<3.5 模型层已钳制+告警 | 低 | ✅ 达标 |
| 5 | 标题栏/图框走 paintText 同路径，受 #2 字宽过窄影响 | 中 | 连带 |

---

## 3. 详细发现与现象说明

### 3.1 字体回退：声明「长仿宋体」实为「仿宋」

`gbLongFangSongStyle()` 设置 `fontFamily = "长仿宋体"`。但本机（及绝大多数 Windows）**没有该字体文件**。

探针实测 `QFontInfo`：
```
request family='长仿宋体' stretch=71 -> resolved='FangSong' exactMatch=0
```

即：程序名义请求「长仿宋体」，实际渲染的是 **FangSong（仿宋）**，`exactMatch=0`。

- **好的一面**：Qt 在本机把「长仿宋体」模糊回退到了「仿宋」——仿宋本就是工程图常用的窄体宋，属于最合理的回退。
- **隐患**：回退目标**依赖用户机器字体**。若用户机未装仿宋（如精简版/英文系统），会进一步回退到更宽或更无衬线的字体，视觉表现漂移不可控。

参考截图 `probe_D_fallback_compare.png`：请求「长仿宋体 / 仿宋 / 华文仿宋 / 宋体」均落到窄体宋，而「微软雅黑」为无衬线粗体，差异显著。

### 3.2 字宽过窄（核心渲染缺陷）

GB/T 14691 要求 `字宽 ≈ h/√2 ≈ 0.707`。但实现用 `widthFactor=0.707`（即 `setStretch(71)`）对**字形整体横向压缩 71%**。

探针测量「中」字真实宽高比（像素紧致包围盒）：

| 配置 | 解析字体 | 宽高比 |
|------|----------|--------|
| `长仿宋体` + stretch71（**当前实现**） | FangSong | **0.511** |
| 仿宋 + stretch71 | FangSong | 0.511 |
| 华文仿宋 + stretch71 | STFangsong | 0.485 |
| 宋体(SimSun) + stretch71 | SimSun | 0.591 |
| 微软雅黑 + stretch71 | YaHei | 0.622 |
| **仿宋 + stretch100（正确参考）** | FangSong | **0.707 ✓** |
| 宋体(SimSun) + stretch100 | SimSun | 0.828 |

**关键洞察**：仿宋（FangSong）的「中」字**自然宽高比已≈0.707，本身即符合国标**。当前 `stretch(71)` 是在一个**已经达标**的字体上再压 0.71 倍 → 0.511，**比国标更窄、笔画被挤扁**，属于「双重压缩」。

> 根因：`widthFactor=0.707` 的设定**假设底字体是宽体**（如宋体自然≈0.828、黑体≈0.876），用以压到 0.707；但实际回退字体是已合规的仿宋，于是过压。

现象见 `probe_E_overcondense.png`：
- ① 当前 LongFangSong（仿宋 stretch71，0.511）：字符明显过细、被压扁；
- ② 正确参考（仿宋 stretch100，0.707）：比例饱满、符合国标；
- 行①宽度约为行②的 70%，与 stretch 因子一致。

新建文字/标题栏/图框的中文效果见 `probe_A_longfangsong.png`：中文呈窄仿宋、无豆腐块、各字号清晰，但字身偏瘦（受 #3.2 影响）。

### 3.3 尺寸标注未接入 LongFangSong（需求第 2 点缺失）

阅读 `src/ui/GraphicsView.cpp` 的 `paintDimension`（约 580–595 行）：
```cpp
f.setPixelSize(std::max(8, (int)std::lround(st.textHeight * z)));
p->setFont(f);                       // ← 仅设字号
// 没有 f.setFamily(...)             // ← 缺失
// 没有 f.setStretch(...)            // ← 缺失
```
尺寸标注文字**既不绑定 LongFangSong 字体族，也不施加字宽因子**，直接使用画笔默认字体。在本机探针中以系统默认 UI 字体（微软雅黑，无衬线）代表，效果见 `probe_B_dimension_current.png`：文字呈粗方黑体，与 GB/T 14691 要求的仿宋/长仿宋字**风格、比例均不符**。

这与需求「图框、标题栏、**尺寸标注**在新建文字时默认采用该样式」直接冲突。对照若接入后的效果见 `probe_C_dimension_lfs.png`。

### 3.4 字号下限与字符集

- **3.5mm 下限**：`probe_A` 中 3.5mm 行清晰可辨（pixelSize=70px），满足 GB/T 14691 汉字下限。
- **<3.5mm 告警**：由 `enforceHanziMinHeight()` 在模型层钳制到 3.5 并输出告警（自测 `t_longFangSong` 已覆盖）。但**渲染层 `paintText` 不二次钳制**——凡绕过模型的写入路径（导入、脚本、历史档）若携带小字高，屏幕上会显示 <3.5mm 的字。
- **字符集**：中文、英文、数字、符号（Φ±Δ℃①②③°≈≠）在 3.5/5/7mm 下均**无豆腐块/缺字**，符号渲染正常（见 `probe_A` 符号行）。

---

## 4. 各类元素实测汇总

| 元素 | 是否应用 LongFangSong | 实际字体 | 字宽表现 | 评价 |
|------|----------------------|----------|----------|------|
| 新建文字 Text/MText | ✅ `styleName=LongFangSong` → paintText | 仿宋(stretch71) | 0.511（过窄） | 字体族合理，字宽需修正 |
| 标题栏（sheet_frame） | ✅ 设 styleName | 仿宋(stretch71) | 0.511（过窄） | 受 #3.2 连带；中心锚点兼容 OK |
| 图框标签 | ✅ 设 styleName | 仿宋(stretch71) | 0.511（过窄） | 同上 |
| 尺寸标注 Dimension | ❌ 未接入 | 默认字体(雅黑等) | 不合规 | **需按需求接入** |

---

## 5. 改进建议

### 建议 1：字体族与回退链（解决 #3.1）
- **首选**：在安装包内**嵌入并注册一份真实的「长仿宋体」TTF**（注意开源/商用授权），启动时 `QFontDatabase::addApplicationFont` 加载，使 `exactMatch=1`，彻底实现「汉字采用长仿宋体」。
- **务实替代**：若不嵌入，将默认样式 `fontFamily` 由 `"长仿宋体"` 改为系统确有的 `"仿宋"`，并设回退链 `QFont::setFamilies({"长仿宋体","仿宋","华文仿宋"})`，保证缺失时落到窄体宋而非黑体。

### 建议 2：修正字宽因子（解决 #3.2，核心）
`widthFactor=0.707` 是为宽体底字校准的，对仿宋是「双重压缩」。按底字体二选一：
- **若底字体=仿宋/长仿宋体（自然≈0.707）**：`widthFactor` 改为 **1.0**（字体本身已达标，不再压）。
- **若坚持用宽体底字+压缩到 0.707**：须选自然宽≈1.0 的字体（如 SimSun 自然 0.828，需 `stretch≈85`；黑体更长），并设精确 stretch，而非固定 71。
- 推荐采用「选对字体 + widthFactor=1.0」方案，最稳且最符合国标字面。

### 建议 3：尺寸标注接入样式（解决 #3.3）
- 在 `paintDimension` 中按 `e.dimStyleName`（建议新增 `DimStyle::textStyleName`，默认 `"LongFangSong"`）解析文字样式，施加与 `paintText` 同源的 `setFamily` + `setStretch`。
- `PlotRenderer` 的标注绘制路径同样处理，保证屏幕与打印一致。

### 建议 4：渲染层字号下限兜底（解决 #3.4 隐患）
- 在 `resolveTextStyle` 或 `paintText` 末端对 `height` 保留 `max(height, 3.5)` 的渲染兜底（或至少在 <3.5 时打 stderr 提示），防止导入/脚本写入的小字高直接上屏。

### 建议 5：补充自动化渲染断言（防回归）
- 将本探针的「中」字宽高比测量（期望 ∈ [0.68, 0.74]）固化进 `SketchSelfTest`，作为 LongFangSong 渲染契约的长期守护。

---

## 6. 结论

`LongFangSong` 样式在**字体族选择上方向正确**（回退到仿宋是合理结果），且**字号下限、字符集、中心锚点兼容性均已达标**。但存在两个**严重偏离国标**的问题需优先修复：

1. **字宽过窄**：`widthFactor=0.707` 在仿宋（本身已 0.707）上造成双重压缩，实测 0.511，明显窄于 GB/T 14691 的 h/√2，笔画被挤扁——应改为「选对窄体宋 + widthFactor=1.0」。
2. **尺寸标注未接入样式**：`paintDimension` 不绑定字体族与字宽因子，标注文字以默认无衬线字体绘制，违反需求第 2 点——需与 `paintText` 同源接入。

完成建议 1–3 后，新建文字/标题栏/图框/标注四类元素即可在视觉与比例上统一、合规地呈现长仿宋体风格。

---

### 附件（实测截图，见同目录 PNG）
- `probe_A_longfangsong.png` —— LongFangSong 新建文字（仿宋 stretch71）全字符集/字号/字重
- `probe_B_dimension_current.png` —— 尺寸标注当前实现（默认无衬线字体，未接入样式）
- `probe_C_dimension_lfs.png` —— 尺寸标注若接入 LongFangSong 的对照
- `probe_D_fallback_compare.png` —— 同一文字在不同字体族下的回退表现
- `probe_E_overcondense.png` —— 字宽过窄演示（当前 0.511 vs 正确 0.707）
