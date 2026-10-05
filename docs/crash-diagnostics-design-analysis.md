# 崩溃可复现诊断包 —— 设计与分析（阶段六）

> 目标：把「客户说崩了」变成「客户发一个包即可复现」。
> 本阶段交付：一键导出诊断包（ZIP）、异常退出下次启动提示、可选遥测（默认关闭）。

---

## 1. 需求回顾

| # | 需求 | 关键约束 |
|---|------|----------|
| 1 | 「导出诊断包」一键打包最近日志 + mini-dump + 版本/Qt/OS/显卡信息 + 图纸元信息 → 用户目录 ZIP | **不含几何/文本内容** |
| 2 | 崩溃后下次启动提示「上次异常退出，是否发送诊断包/查看日志」 | 手动杀进程重启可复现 |
| 3 | 遥测（可选）：匿名会话/崩溃计数 | **默认关闭**；「选项→隐私」可关；首启动明示 |
| A | 自测 t_diag ≥ 6 条（ZIP 生成/含预期文件/信息完整/隐私默认关零外发——网络层 mock 断言） | 双目标零警告、`build.sh test` EXIT=0 |
| B | 边界：默认不得有任何网络外发；不得输出用户图元几何/文本到日志 | — |

## 2. 现状勘察

- `src/core/Diagnostics.{h,cpp}` 已存在（崩溃不丢稿阶段）：Qt 日志滚动落盘（`%LOCALAPPDATA%/SketchCAD/logs/SketchCAD.log{,.1,.2}`，单文件 ≤2MB）+ Windows dbghelp mini-dump（`crashes/crash-<stamp>.dmp` + `last.txt` 摘要）。`SKETCHCAD_ENABLE_CRASHHANDLER` 宏只链主目标。
- `main.cpp` 已有 `Diagnostics::init`（QApplication 后、主窗口前）与 `aboutToQuit` 钩子——会话标记的天然接线点。
- `OptionsDialog` 六页结构：`addPage` + `buildXxxPage` + `loadWidgets/loadWidgetsFromDefaults/collectWidgets` 三处逐键同步；新键走 AppSettings 通用层（specTable 唯一数据源）。
- 无第三方 ZIP 库 → 自研。压缩收益对诊断文本极低 → **Stored（不压缩）**，换取实现极简与结构可静态断言。

## 3. 方案设计

```
┌─ MainWindow ──────────────────────────────────────────────┐
│ drawingDiagnosticsMeta()   仅计数字段（铁律：无几何/文本）  │
│ maybeShowPrivacyNotice()   首启动一次性明示                │
│ showAbnormalExitNotice()   启动页后弹出（Yes=导出 No=日志）│
└──────────┬────────────────────────────────────────────────┘
           ▼
┌─ DiagnosticsDialog（薄壳）─┐   ┌─ core/Telemetry ─────────┐
│ 文件清单 + 导出/打开日志    │   │ record(): 开关 && 出口    │
└──────────┬─────────────────┘   │ 双门闩，默认全关 → 零外发 │
           ▼                     └──────────────────────────┘
┌─ core/DiagBundle ─────────────────────────────────────────┐
│ ZipBuilder（Stored）+ crc32（表驱动，循环生成）             │
│ systemInfoText / collectSourceFiles / exportBundle        │
└──────────┬────────────────────────────────────────────────┘
           ▼
┌─ core/Diagnostics（扩展）─────────────────────────────────┐
│ 会话标记：beginSession（检测+写）/ endSession（删）/        │
│ lastSessionAbnormal（UI 消费）+ 路径注入版三函数供直测      │
└───────────────────────────────────────────────────────────┘
```

**分层原则**（延续阶段五）：纯逻辑全在 core（DiagBundle/Telemetry/会话标记），无头直测；UI 只做薄壳；打包 API 不接触 SketchModel——图纸元信息由 MainWindow 组装为 `QVariantMap` 传入，「不含几何」的边界由调用方守卫，core 结构上就不可能泄漏。

**零外发的机制保证**（不靠约定靠结构）：
1. 全代码库不存在任何网络调用；`TelemetrySink` 只有接口没有实现；
2. `Telemetry::record()` 双门闩：`enabled() && sink()` 均真才发送，默认双关；
3. 遥测开关（`privacy/telemetryEnabled`，默认 false）与出口注入是两条独立路径，缺一不发。

## 4. 实现细节与关键决策

### 4.1 Stored-ZIP 写入器（`DiagBundle::ZipBuilder`）
- 结构：逐条目 `PK\x03\x04` 本地头（method=0、CRC/size 已知故无需 data descriptor）+ `PK\x01\x02` 中央目录 + `PK\x05\x06` EOCD；名字 UTF-8（flags bit11=0x0800）；DOS 本地时间戳。
- CRC32：表驱动（0xEDB88320），**表在首次调用时循环生成**而非 256 项字面量——规避 MinGW GCC 13.1 巨型聚合初始化静默崩溃（阶段五教训，普适铁律）。
- 全内存组装（量级 MB），`write` 校验实际写入字节数，不完整即报错。

### 4.2 会话标记（异常退出检测）
- `%LOCALAPPDATA%/SketchCAD/session.marker`：`beginSession()`（main.cpp，init 后）检测残留并重写；`endSession()`（aboutToQuit）删除；残留 = 崩溃/被杀/断电。
- 三函数（write/isAbnormal/clear）带路径注入版，自测在临时目录验证，不污染真实标记。
- 检测结果缓存在 `g_lastAbnormal`，`showAbnormalExitNotice()` 在启动页**之后**消费（`singleShot(0)` FIFO：先注册的 showStartupPage 先弹，模态结束后才轮到诊断提示），`acknowledgeAbnormalSession()` 保证只提示一次。

### 4.3 遥测与隐私页
- AppSettings 新键（specTable 唯一数据源 + 逐键 using 声明）：
  - `privacy/telemetryEnabled`（默认 false）
  - `privacy/firstRunNoticeShown`（默认 false，首启动明示后置 true）
- 消费链完整：选项→隐私勾选 → `setTelemetryEnabled` → `settingChanged` → `applySettingLive` → `Telemetry::setEnabled` 即时生效；取消回滚走同一信号通道（快照机制自动覆盖新键，零额外代码）。
- `maybeShowPrivacyNotice()`：首启动一次性 `QMessageBox::information` 明示「默认不上传/诊断包本机生成/遥测默认关」。

### 4.4 打包内容（collectSourceFiles）
| 文件 | 来源 |
|------|------|
| SketchCAD.log{,.1,.2} | logs/（存在才收） |
| last.txt + crash-*.{dmp,txt} 最近 3 组 | crashes/（按名降序） |
| system-info.txt | 版本（SKETCHCAD_VERSION 宏）/ qVersion / QSysInfo OS+CPU+ABI / 平台插件 / EnumDisplayDevicesW 显卡名 |
| drawing-meta.json | drawingFile（仅文件名）+ elementCount/layerCount/blockCount + generatedAt |

自测目标不含 `generated/version.h`（CMake 只挂主目标）→ `#ifdef` 回落「自测构建」，与「自测不关心版本」的既有设计一致。

## 5. 改动清单

| 文件 | 改动 |
|------|------|
| `src/core/DiagBundle.{h,cpp}` | **新增**：ZipBuilder + crc32 + systemInfoText + collectSourceFiles + exportBundle（纯 core） |
| `src/core/Telemetry.{h,cpp}` | **新增**：TelemetrySink 接口 + Telemetry 静态双门闩 |
| `src/core/Diagnostics.{h,cpp}` | 扩展：会话标记三函数（路径注入）+ begin/end/lastSessionAbnormal/acknowledge |
| `src/core/AppSettings.{h,cpp}` | 新键 `kTelemetryEnabled`/`kFirstRunNotice` + spec 条目 + 类型化访问器 |
| `src/ui/DiagnosticsDialog.{h,cpp}` | **新增**：导出对话框（文件清单/导出/打开日志目录） |
| `src/ui/OptionsDialog.{h,cpp}` | 第七页「隐私」（chkTelemetry_ 三处同步） |
| `src/ui/MainWindow.{h,cpp}` | 4 个新方法 + 构造尾部遥测同步与启动排队提示 + applySettingLive 分支 + 帮助菜单 2 项 + include |
| `src/main.cpp` | `beginSession()`（init 后）/ `endSession()`（aboutToQuit） |
| `CMakeLists.txt` | CORE +DiagBundle/Telemetry；UI +DiagnosticsDialog |
| `tests/SketchSelfTest.cpp` | t_diag（27 条 CHECK）+ main 注册 |

## 6. 验收结果

- **t_diag 27 条 CHECK**（≥6 达标）：
  - D1 ZIP 结构（5）：非空判定、本地头签名、EOCD 签名/条目数=2、Stored method=0；
  - D2 自解析（3）：中央目录遍历全条目、独立实现 CRC32 复算全一致、a.txt 内容逐字节比对；
  - D3 产物（2）：指定目录 ZIP 生成、包非空；
  - D4 信息字段（4）：system-info.txt 存在且 CRC 一致、含 Qt 版本（与 qVersion 运行时一致）/操作系统/显卡字段；
  - D5 meta 边界（2）：elementCount=42 落盘、JSON 体积 <400B（仅计数无几何/文本）；
  - D6 会话标记（5）：无标记=正常、写成功、残留=异常、清除成功、清除后恢复；
  - D7 遥测零外发（6）：默认状态 record 零外发、状态自省、仅开开关仍零外发（双门闩）、齐备后恰一次送达且字段一致、关闭即停、恢复默认态。
- **全量自测 1197 → 1224 / 0**，EXIT=0；双目标构建零警告（BUILD_EXIT=0）。
- 网络层 mock 断言：`MockSink : TelemetrySink` 计数，默认状态疯狂 record 断言 `send` 零调用 ✅。
- 手动项（真机验证路径）：任务管理器杀 SketchCAD → 重启 → 启动页后弹「上次异常退出」→ Yes 打开 DiagnosticsDialog → 导出 ZIP 至用户目录。机制链（标记残留检测 → 提示 → 导出）已由 D6 + beginSession/endSession 接线覆盖，GUI 呈现待真机确认。

## 7. 遗留与后续

1. **真实遥测实现不存在**（by design）：未来若接匿名统计，须实现 TelemetrySink 并保持「无几何内容」边界，隐私页文案同步更新。
2. **崩溃转储体积**：MiniDumpNormal 已最小；如需含堆的 Full dump 可加开关（包体积换可诊断性）。
3. **诊断包上传**：刻意不做。后续可做「生成后打开邮件客户端预填附件」等无服务器方案。
4. **ZIP 兼容性**：Stored + UTF-8 flag 为 ZIP 最低公分母；已按结构逐字段自测，Windows 资源管理器/7-Zip 可直接打开（无第三方解码器依赖）。
5. 会话标记在**第二个实例**场景下语义退化（后启动者覆盖标记，前者退出即视为正常）——当前单实例使用模式无影响。
