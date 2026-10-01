# SketchCAD COM 自动化接口与批量出图 —— 设计分析（深思过程）

> 版本 1.0 | 2026-10-01 | 关联交付：`src/com/`、`SketchCADAutomation.dll`、`docs/COM_API.md`

## 1. 需求与目标

在既有原生 SDK（插件框架）之外，把程序核心能力以 **COM/自动化接口** 形式开放给外部系统，
支撑「批量出图」流水线：外部系统传入出图任务列表与参数，程序按任务自动完成出图并反馈
执行状态；同时完善异常处理、错误码、日志与并发稳定性，并输出接口文档与调用示例。

## 2. 能力范围梳理（勘察先行）

逐项勘察了出图链路上的全部既有实现，确认无头（headless）复用点：

| 能力 | 既有实现 | 位置 | 无头可用性 |
|---|---|---|---|
| 读 JSON 图纸 | `elementFromJson` + `SketchModel::set*FromJson` | **核心层**（Element.cpp / SketchModel.cpp） | ✅ 直接复用 |
| 读 DXF/DWG | `cadfmt::fromDxf/fromDwg` + $INSUNITS 换算 | core/CadFormats、Units | ✅ 纯函数 |
| PDF 出图 | `PrintEngine::exportPdf(SketchModel*, PrintSettings, path)` | ui/PrintEngine | ✅ 静态、QPdfWriter 离屏可行 |
| 导出 DXF/DWG/STEP/JSON | `cadfmt::to*` + `expandBlockRef`（块引用展开） | core | ✅ 纯函数 |
| 版式参数 | `PrintSettings::fromJson/toJson` | ui/PrintEngine | ✅ 与 GUI 共用同一 JSON |
| 几何路径/包围盒 | `ElementItem::buildPath/rawBounds` | ui/GraphicsView.cpp | ⚠️ 物理上在 UI 巨型编译单元 |

**唯一阻塞点**是最后一行：`PrintEngine/PlotRenderer` 依赖 `GraphicsView.cpp` 里的
`ElementItem::buildPath` 与 `geometryBounds`（后者构造临时 item 调 `rawBounds`）。
若 COM DLL 直接链 GraphicsView.cpp，会拖入 ToolManager/dnd/交互全家（链接器要求
解析该编译单元引用的全部符号）。**决策：下沉而非链接**——把三段纯几何逻辑
（buildPath / xlineClipToRect / rawBounds）原样搬到新单元 `src/ui/ElementPath.{h,cpp}`，
GraphicsView 侧改为四行委托，`PlotRenderer/PrintEngine` 改引 ElementPath。
几何算法一字未改，主程序回归零破坏（后经三目标构建验证）。

## 3. 技术选型：为什么手写 IDispatch

| 方案 | 结论 |
|---|---|
| MIDL + typelib | ❌ MinGW 工具链无 midl.exe/marklibs，无法生成 |
| 免注册 COM（Registration-Free / manifest） | 可行但部署于脚本宿主（wscript）时配置繁琐，放弃 |
| **手写 IDispatch 静态分派** | ✅ 一个静态「方法名→dispid」表 + Invoke switch，约 200 行；
     无类型库不影响后期绑定（VBScript/PowerShell/Python win32com 全走 GetIDsOfNames+Invoke） |
| 跨进程 LocalServer | 双进程/编组复杂度高；in-proc DLL + regsvr32 足以覆盖批量出图（CPU/IO 密集，无 UI） |

**进程内 + regsvr32** 的注册布局：
`HKCR\SketchCAD.Automation(.1)` → `HKCR\CLSID\{...}\InprocServer32`
（默认值=DLL 绝对路径，`ThreadingModel=Apartment`）。四标准导出
`DllGetClassObject / DllCanUnloadNow / DllRegisterServer / DllUnregisterServer`。

## 4. 架构分层

```
VBScript / PowerShell / Python / C#          ← 后期绑定调用方
        │ IDispatch
SketchCADComServer（COM 分派层）              ← VARIANT↔QString、错误码映射、事件桥
        │ 纯 Qt 调用
BatchPlotEngine（无头引擎层，COM 无关）        ← 任务解析、串行执行、日志、事件槽
        │ 复用
ElementPath / PrintEngine / PlotRenderer / cadfmt / SketchModel   ← 核心与打印
```

分层的核心动机：**引擎层不含任何 COM 类型**，`t_comAutomation` 在 SketchSelfTest
中直接驱动（offscreen 读档→PDF 产出→状态断言），COM 语义与出图能力可独立验证。

### 4.1 引擎层要点

* **Qt 懒初始化**：`ensureQtApplication()` 检查 `QCoreApplication::instance()`，
  已有（GUI 宿主/自测进程）直接复用；纯 COM 宿主（如 wscript 注入的进程）补建
  QApplication（构造受互斥锁保护，进程内仅一次）。
* **任务隔离**：每个任务一个独立 `SketchModel`（loadDrawing 清空重建），失败任务
  不污染后续任务。
* **并发稳定**：进程级 `std::mutex` 串行化 runTasks；STA 线程模型兜底单宿主顺序。
* **事件槽**：引擎层 `std::function`（onTaskStart/onTaskDone/onBatchFinished），
  锁内拷贝、锁外调用，防回调重入死锁。
* **日志**：可选路径，逐任务追加时间戳行；日志失败不影响出图本身（降级静默）。

### 4.2 COM 分派层要点

* **分派表**：10 个成员（8 方法 + 2 属性）静态表，GetIDsOfNames 大小写不敏感查找；
  Invoke 按 dispid switch，`rgvarg[cArgs-1]` 为第 1 参数（COM 位置参数自右向左约定）。
* **错误码**：自定义 HRESULT，Facility=ITF、码区 0x0601 起（不与系统码冲突）；
  部分成功语义独立成码 `SCAD_S_PARTIAL`（批量有失败项但整体流程合法）。
* **事件桥**：RegisterCallback 保存调用方 IDispatch*（AddRef），引擎事件经
  `sink.GetIDsOfNames(OnTaskXxx) + sink.Invoke` 送达；宿主未实现该方法则静默跳过。
  载荷统一为**单个 JSON 字符串**——对 VBScript 等弱类型宿主最友好，避免 SAFEARRAY。
* **对象生命周期**：引用计数 + 全局对象计数/锁计数；`DllCanUnloadNow` 双零才允许卸载。

## 5. 关键决策与权衡

1. **同步批量而非队列**：批量出图是 CPU/IO 密集短任务，同步返回结果 JSON 让调用方
   一站式拿到状态；需要异步的外部系统可自行开线程调用（引擎互斥保证安全）。
2. **JSON 字符串传任务列表**而非 SAFEARRAY of BSTR：跨语言兼容性最好（VBScript 拼
   JSON 字符串远比构造二维数组容易），且引擎层解析逻辑可独立测试。
3. **错误码字符串化进日志**：引擎层返回 `SCAD_E_XXX: 描述 (0x…)` 文本，COM 层映射回
   HRESULT——日志自解释，外部系统也能按前缀字符串分类处理。
4. **ElementPath 下沉而非条件编译**：避免了「COM 构建时排除 UI」的维护分裂；
   主程序与 DLL 共用同一份几何代码，编译期保证行为一致。

## 6. 实现过程中的坑（全数记录）

| 坑 | 根因 | 修复 |
|---|---|---|
| `no declaration matches … SketchModel&` | `class SketchModel;` 前向声明写在 `namespace SketchCAD` **外** → 头文件签名变成全局 `::SketchModel`（与 PluginManager 同型坑，第二次出现） | 前向声明移入命名空间内 |
| ComError.h 编译错误级联 | 该头使用 HRESULT/S_OK 但未 include windows.h | 头内加 `NOMINMAX` + `<windows.h>` |
| `~SketchCadAuto() marked override but does not override` | MinGW 头文件的 IUnknown **无虚析构** | 派生类显式 `virtual ~SketchCadAuto()`（虚槽追加在自有 vtable 尾部，COM 客户端可见槽位不变），同时消除 delete-non-virtual-dtor 警告 |
| UINT vs int 符号比较警告 | `p->cArgs < idxFrom1`（cArgs 为 UINT） | 显式 `(int)p->cArgs` |
| 批量执行期间事件回调死锁隐患 | onBatchPlot 持有对象锁 → 引擎同步回调 fireSink 同线程再取锁 | 对象互斥改 `std::recursive_mutex`（可重入） |

## 7. 验证策略与结果

* **引擎层直测**（t_comAutomation，t_pluginManager 同风格）：
  引擎导出 JSON 图纸 → 批量 3 任务（PDF/DXF/JSON）→ 断言 `allOk/succeeded`、
  PDF 魔数 `%PDF-1.4`、JSON 往返 elements 数量一致、事件计数 3/3/1 →
  负例（不存在文件 → `SCAD_E_FILE_NOT_FOUND`；坏 JSON → `SCAD_E_BAD_TASK`；
  STEP 读回拒绝）→ 事件计数 4/4/2。**50/0 全量通过**。
* **三目标构建**：`SketchCAD.exe` / `SketchSelfTest.exe` / `SketchCADAutomation.dll`
  全部零警告（仅存量无关 warning），`-Wall -Wextra`。
* **产物验证**：`SketchCADAutomation.dll`（2.9MB）按 `PREFIX ""` 命名规范产出。

## 8. 扩展路线

1. 事件扩展：批量进度百分比、逐图层出图、多版式一次任务。
2. 打印机直出：BatchTask 增加 `printer` 字段走 `PrintEngine::print`（系统打印机）。
3. 免注册部署（RegFree COM）：SxS manifest，适配无管理员权限的受控环境。
4. typelib：迁移 MSVC 工具链后可补 MIDL，提供早期绑定与 VBA 对象浏览器支持。
5. 与原生 SDK 生态贯通：BatchPlot 能力经 `IApplication` 暴露给插件，形成
   「GUI / 脚本 / COM / 插件」四入口同一引擎的完整二开体系。
