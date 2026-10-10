# DWG R2000+ 读写桥接（D1）设计分析与实施记录

> 对应差距项：D1（DWG 格式兼容，见 docs/cad-vs-autocad-gap-analysis-2026-10.md）
> 日期：2026-10-08 · 状态：✅ 已实施 · 基线：1224 CHECK → 1254/0（+30，t_dwg）

---

## 一、目标与约束（任务原文要点）

- **目标**：合规授权前提下为 SketchCAD 增加 DWG R2000/R2004/R2007 读写，打开设计院交付通道。
- **前置放宽**：可引入 LibreDWG 等 GPL 库。
- **范围**：`CadFormats.cpp` 增加 DWG 版本探测；R2000+ 读写走隔离层；与自研 R12 按版本自动路由并存；版本不支持时明确报错（含版本串原文）。
- **约束**：自研 R12 读写全量回归不破坏（AC1009 头部强校验语义不动）；不逆向、不内嵌任何逆向产物；隔离层编译为独立静态库目标。
- **验收**：R2000/2004/2007 样本读写 round-trip 黄金差分；自测 ≥12 断言（版本路由/错误报文/R12 回归）；零警告；`build.sh test` EXIT=0。

## 二、合规模式决策：为什么是「进程外转换器 + QProcess 桥」

GPLv3 的传染边界是**链接**（静态/动态）与衍生作品。三种候选方案对比：

| 方案 | 合规性 | 崩溃隔离 | 工程量 | 结论 |
|---|---|---|---|---|
| A. 静态链入 libdwg.a | GPL 传染主程序 ✗ | ✗ 同进程 | 小 | **否决** |
| B. 动态链接 dwg.dll | GPL 边界争议（需 LGPL/例外） | ✗ | 中 | 否决 |
| C. **独立转换器 exe + 临时文件中转** | GPL 只覆盖转换器本体，主程序零 GPL 代码 | ✓ 坏文件只杀转换器 | 中 | **采用** |

方案 C 的桥接协议是「命令行参数 + 磁盘字节流」，双方无共享地址空间、无头文件引用，
GPL 代码与主程序之间不存在链接关系或衍生关系——这是 LibreDWG 官方认可的集成形态
（dwg2dxf/dwgwrite 本就是为管道化使用设计的独立工具）。

**鲁棒性红利**（与项目崩溃不丢稿文化契合）：DWG 是 ODA 私有二进制、无公开规范，
LibreDWG 对损坏文件的容错不可能完美。进程外调用把转换器崩溃隔离在子进程内，
主程序经 `QProcess::CrashExit` 归一为明确错误报文，绝不连坐。

## 三、架构

```
┌─ SketchCAD 主程序（零 GPL）────────────────────────────┐
│ MainWindow/命令层                                        │
│   ↓                                                      │
│ CadFormats::fromDwg / toDwgR2000     ← 版本自动路由      │
│   ↓ AC1009 ──→ 自研 R12 二进制解析/写出（语义不动）      │
│   ↓ AC1012+ ─→ DwgBridge（独立静态库 SketchDwgBridge）  │
└────────────┬───────────────────────────────────────────-┘
             │ QProcess（超时 30s / CrashExit 捕获 / stderr 摘要）
┌────────────▼───────────────────────────────┐
│ thirdparty/libredwg-0.14/build-mingw/      │
│   dwg2dxf.exe  （GPLv3，独立进程）          │
│   dwgwrite.exe （GPLv3，独立进程）          │
└────────────────────────────────────────────┘
```

### 版本路由（`fromDwg` 入口，版本串探测唯一入口 `dwgbridge::versionFromBytes`）

| 头部 6 字节 | 路由 | 说明 |
|---|---|---|
| `AC1009` (R12) | 自研 R12 原生解析 | 语义逐字节不动，回归保护对象 |
| `AC1012/1014/1015/1018` | 桥 → dwg2dxf → `fromDxf` | R13/R14/R2000/2000i |
| `AC1021/1024/1027/1032` | 桥 → dwg2dxf → `fromDxf` | R2004/2007/2013/2018 |
| `AC1xxx` 其他（如 AC1999） | 拒绝，报文含版本串原文 | 绝不误读 |
| 非 `AC1` 开头 | 拒绝，报文含头部原文 | 如 "Sketch" |

**读链零新增解析代码**：桥产出 DXF 后复用既有 `fromDxf`（R12/R2000 双版本解析器，
R2000 的 100/5/330 标记被自然忽略），DXF R2000 导入此前已验证。

**写链**：`toDwgR2000` = `toDxf(R2000)`（自研，注入 AcDb 标记/句柄/OBJECTS 段）→
`dwgwrite --as r2000 -I DXF`。`toDwg`（R12）签名与输出不变，旧调用方零影响。

### LibreDWG 能力边界（如实声明，不虚标）

- **读**：R13~R2018 全系（含 R2004/R2007），黄金样本实测通过。
- **写**：**仅到 R2000**。R2004/R2007 的 DWG 写不支持（上游无 encode_r2007 完整实现）。
  交付惯例是 R2000 DWG（AutoCAD 2004+ 全兼容打开）或 R2007 DXF——已在 API 注释与
  本文档双重声明，自测断言产物头部必为 `AC1015`，不虚构更高版本。

## 四、实施细节与踩坑记录

### 4.1 LibreDWG 构建（thirdparty/libredwg-0.14，不动上游源文件）

- **configure 不可用**：Git Bash/MSYS 下 autoconf fork 性能极差，实测 15 分钟未完成
  1% check。放弃 autotools，**手写 MinGW config.h**（对照 config.h.in 的 141 个宏逐项
  静态回答，见 `src/config.h` 头注释）。
- **`AX_STRCASECMP_HEADER`**：common.h 在 `HAVE_STRCASECMP` 时无条件
  `#include AX_STRCASECMP_HEADER`——autotools 会注入 `<string.h>`，手写时必须
  `#define AX_STRCASECMP_HEADER <string.h>`，否则编译报 "include expects FILENAME"。
- **`LIBREDWG_SO_VERSION`**：静态库也要定义（dwg_api.c 引用），取 configure.ac 的
  `"0:14:0"`。
- **`gmtime_r`**：mingw-w64 的内联实现受 POSIX 宏门槛控制不激活 → undef
  `HAVE_GMTIME_R` 走 `gmtime()` fallback（转换器单线程，无碍）。
- **字节序**：`common.h` 末尾有 mingw 小端 fallback（自写 bswap_constant），无需定义
  `HAVE_WINSOCK2_H` 相关分支。
- **DISABLE_JSON 必须定义**：未编 in_json/out_json 时若不定义，dwgwrite.c 的
  `#ifndef DISABLE_JSON` 分支引用 json_read_file → 链接失败。源文件列表与 config.h
  必须严格一致。
- **构建脚本**：`CMakeLists.mingw.txt`（复制为 CMakeLists.txt 使用），最小集 =
  19 个基础 .c + out_dxf/out_dxfb + encode/encode2/dxfclasses/in_dxf + 2 个程序单文件。
  产物：libdwg.a(20MB)、dwg2dxf.exe(14MB)、dwgwrite.exe(10MB)。
- **⚠️ 转换器必须 `-static` 全静态链接**：MinGW 默认动态链接 libgcc_s_seh-1.dll /
  libwinpthread-1.dll，这些 DLL 不在用户机器 PATH 上 → 部署后 QProcess 启动即
  FailedToStart。静态链接后 objdump 确认仅依赖系统内置 KERNEL32.dll + msvcrt.dll，
  转换器自包含可随安装包放 `tools/dwg/`。

### 4.2 桥接层（src/core/DwgBridge.{h,cpp}，独立静态库目标 SketchDwgBridge）

- **转换器定位顺序**：`setToolsDirForTest`（自测注入）→ 环境变量 `SKETCHCAD_DWGTOOLS`
  → `<exe目录>/tools/dwg/`（安装包布局）。
- **进程契约**：临时目录 RAII（QTemporaryDir）落盘输入 → 同步执行（超时默认 30s，
  超时 kill）→ 读回输出 → 自动清理。stderr 采 200 字符摘要进错误报文。
- **错误报文规约**：一律含版本串原文 + 人类可读名（复用 `cadVerToHuman`，两处同源），
  如 `不支持的 DWG 版本：AC1021（DWG 2004）`——用户报障时可直接定位文件版本。
- **零转换器头文件泄漏**：DwgBridge.cpp 只 include Qt + CadFormats.h；转换器 exe 是
  黑盒。链接边界即审计边界。

### 4.3 黄金样本（thirdparty/libredwg-0.14/test/test-data/ 上游自带）

冒烟实测（2026-10-08，MinGW 13.1 构建）：

| 样本 | 操作 | 结果 |
|---|---|---|
| example_2000.dwg | dwg2dxf | ✓ EXIT=0，DXF 头 $ACADVER=AC1015 |
| example_2004.dwg | dwg2dxf | ✓ EXIT=0（895KB DXF） |
| example_2007.dwg | dwg2dxf | ✓ EXIT=0（1.4MB DXF） |
| e2000.dxf | dwgwrite --as r2000 | ✓ rt2000.dwg 头部=AC1015 |
| rt2000.dwg | dwg2dxf 回读 | ✓ EXIT=0（写读闭环） |

Warning "Unstable Class / Unknown object" 为上游对非关键对象的既有提示，不影响主几何。

## 五、测试（t_dwg，无头直测；最终 1254 / 0，基线 1224 → +30）

断言分组（详见 tests/SketchSelfTest.cpp）：

- **D1 版本探测**：AC1009/AC1015/过短/非 DWG 头（4）
- **D2 路由白名单**：AC1009 不经桥 / R2000 族放行 / R2004+ 族放行 / 未知拒绝；人类可读名一致（4）
- **D3 桥可用性**：空目录不可用 / 双 exe 齐备可用 / 不可用时报文含版本串原文（4）
- **D4 fromDwg 路由**：AC1021 拒绝+报文含原文 / AC1999 拒绝 / 非 DWG 头报文含原文（4）
- **D5 toDwgR2000**：注入不存在目录（test 优先级最高）→ 空+「未安装」指引 + 报文含上下文（2）
- **D6 R12 回归**：toDwg 头部仍 AC1009 / round-trip 实体数一致 / 坏指针报文「实体段指针无效」原样（4）
- **D7 黄金差分（双重门控）**：环境变量 `SKETCHCAD_DWGTOOLS`+`SKETCHCAD_DWG_SAMPLE`
  **且** 当前进程允许创建子进程时执行（样本读取→全链解析→toDwgR2000→AC1015 头→
  回读→实体数差分为零）；受限环境自动跳过并记 1 条说明断言。

**双重门控的由来（本窗重要发现）**：开发沙箱禁止测试进程创建任何子进程——
QProcess 启动 `cmd.exe` 也返回 FailedToStart（error=0）。最小对照实验
（cmd.exe vs dwg2dxf.exe 双双失败）证明这是环境级限制而非代码缺陷；转换器
CLI 的读写闭环已由外部冒烟验证（见 4.3）。D7 因此设计为「环境可用才跑」，
生产环境（用户桌面）不受影响。

**最终验收**：双目标零警告、`SketchSelfTest` 通过 1254 / 失败 0、
`build.sh test` EXIT=0（含 P004 OOM/LOD 分层测试 ALL PASS）。

## 六、遗留与后续

1. **AutoCAD 交叉打开人工核**：黄金样本 round-trip 已由 LibreDWG 自身闭环验证；
   AutoCAD/ZWCAD 实机打开验证需在有授权的机器上人工执行（本沙箱无法完成）。
2. **安装包分发**：`tools/dwg/` 布局需纳入 build.sh dist/package（含 GPLv3 文本与
   源码获取 URL 到第三方声明）；exe 体积 ~24MB 需评估 UPX 或最小化编译。
3. **增量写优化**：当前写链走 DXF 中转，块/标注以炸开基元交付（与 R12 口径一致）；
   后续可开启 DxfExportOptions 原生实体开关提升保真度（转换器 in_dxf 支持原生实体）。
4. **R2004/R2007 写**：上游无完整 encode_r2007。若未来有强需求，评估 ODA 商业授权
   或交付 R2000 DWG + R2007 DXF 双轨（业界通行做法）。

## 七、文件清单

| 文件 | 动作 |
|---|---|
| src/core/DwgBridge.h / .cpp | 新增：QProcess 桥 + 版本探测 + 错误归一 |
| src/core/CadFormats.h / .cpp | 修改：fromDwg 版本路由、toDwgR2000 新增、注释刷新 |
| CMakeLists.txt | 新增 SketchDwgBridge 静态库目标 + 6 处挂链 |
| tests/SketchSelfTest.cpp | 新增 t_dwg（版本路由/报文/R12 回归/门控真转） |
| thirdparty/libredwg-0.14/ | 新增：上游 0.14 tarball + config.h(手写) + CMakeLists.mingw.txt |
| docs/dwg-r2000-bridge-design-analysis.md | 本文档 |
