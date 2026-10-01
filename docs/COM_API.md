# SketchCAD COM 自动化接口文档（批量出图）

**版本 1.0** | 组件：`SketchCADAutomation.dll` | 适用平台：Windows

SketchCAD 通过进程内 COM 组件对外提供自动化能力：外部系统（VBScript、PowerShell、
Python、C# 等）可加载图纸、执行单图出图与**批量出图**、查询执行状态、接收进度事件，
实现无人值守的自动化出图流水线。

---

## 1. 组件标识与注册

| 项 | 值 |
|---|---|
| ProgID（版本无关） | `SketchCAD.Automation` |
| ProgID（版本化） | `SketchCAD.Automation.1` |
| CLSID | `{B7E4D3A2-5C1F-4E8B-9A6D-2F3C8E1A7B90}` |
| 线程模型 | Apartment（STA） |
| 组件类型 | 进程内服务器（In-Process DLL） |
| 接口类型 | IDispatch（后期绑定，无需类型库） |

注册（管理员或当前用户权限执行一次）：

```bat
regsvr32 SketchCADAutomation.dll
regsvr32 /u SketchCADAutomation.dll   :: 注销
```

DLL 与 `SketchCAD.exe` 放同一目录时，可直接共享 Qt 运行库；独立部署时需随附
Qt6Core/Qt6Gui/Qt6Widgets/Qt6PrintSupport 等 DLL。

---

## 2. 方法参考

所有方法经 `IDispatch` 后期绑定调用；参数为**位置参数**；字符串一律 UTF-16 BSTR。
失败时返回非零 HRESULT，并同步更新 `LastError` 属性。

### Start() → version:String
初始化自动化引擎（宿主进程无 Qt 时补建 QApplication），返回版本描述串。
**调用任何业务方法前必须先 Start()**，否则返回 `SCAD_E_NOT_INITIALIZED`。

### OpenDrawing(path:String)
加载图纸到内部模型。支持 `.json`（原生完整往返）、`.dxf`（R12，含 $INSUNITS 单位换算）、
`.dwg`（原生 R12 二进制）。STEP 为只写格式，不支持读回（`SCAD_E_UNSUPPORTED_FMT`）。

### PlotToPdf(path:String [, layout:String])
按当前模型输出 PDF。`layout` 为可选 PrintSettings JSON（见第 5 节），省略时用默认版式
（A4 纵向、边距 10mm、自适应铺满、居中）。

### ExportAs(path:String)
按输出扩展名导出：`.dxf` / `.dwg` / `.step` / `.json`。块引用自动展开为真实子图元
（导出物即所见）；标注数字按图纸比例换算为实物尺寸（GB/T 14690）。

### BatchPlot(tasks:String [, logPath:String]) → resultJson:String
**批量出图**。`tasks` 为任务列表 JSON（见第 4 节），`logPath` 为可选日志文件路径
（逐任务追加一行，含时间戳/状态/耗时）。方法返回批量结果 JSON（GetTaskStatus 同格式）。
全部成功返回 `S_OK`；存在失败任务返回 `SCAD_S_PARTIAL`（结果 JSON 中有逐任务明细）。
任务在引擎内**串行**执行，进程级互斥保证并发调用安全。

### GetTaskStatus() → resultJson:String
返回最近一次 BatchPlot 的结果 JSON（未执行过时为空串）。

### RegisterCallback(sink:IDispatch)
注册事件宿主（重复调用覆盖前者）。宿主按名实现以下方法即可接收事件（未实现的方法自动跳过）：

| 事件 | 参数 | 载荷 |
|---|---|---|
| OnTaskStart | taskJson:String | `{"index":0,"source":"..","output":".."}` |
| OnTaskDone | taskJson:String | `{"index":..,"source":..,"output":..,"ok":true,"error":"","bytes":3307,"elapsedMs":56}` |
| OnBatchFinished | batchJson:String | `{"allOk":true,"total":3,"succeeded":3,"failed":0}` |

### Quit()
释放内部模型、注销事件宿主。对象使用完毕后建议调用；COM 引用计数归零后 DLL 可卸载。

### 属性：Version → String
`"SketchCAD 1.0 (Automation 1.0)"`。

### 属性：LastError → String
最近一次失败的错误文本，格式 `SCAD_E_XXX: 中文描述 (0x80040602)`；成功后清空。

---

## 3. 错误码表

| HRESULT | 名称 | 含义 |
|---|---|---|
| 0x00000000 | SCAD_S_OK | 成功 |
| 0x00040609 | SCAD_S_PARTIAL | 批量任务部分失败（结果 JSON 有明细） |
| 0x80004005 | SCAD_E_FAIL | 未分类失败 |
| 0x80070057 | SCAD_E_BAD_ARG | 参数非法（空路径/类型不符） |
| 0x80040601 | SCAD_E_NOT_INITIALIZED | 未 Start 就调用业务方法 |
| 0x80040602 | SCAD_E_FILE_NOT_FOUND | 输入图纸不存在或不可读 |
| 0x80040603 | SCAD_E_PARSE_FAILED | 图纸解析失败（损坏/格式不符） |
| 0x80040604 | SCAD_E_PLOT_FAILED | PDF 出图失败 |
| 0x80040605 | SCAD_E_BAD_TASK | 任务 JSON 非法（缺 source/output 字段） |
| 0x80040606 | SCAD_E_WRITE_FAILED | 输出写入失败（目录不存在/被占用） |
| 0x80040607 | SCAD_E_BUSY | 引擎忙（另一批量任务执行中） |
| 0x80040608 | SCAD_E_UNSUPPORTED_FMT | 不支持的格式（STEP 读回） |

---

## 4. 批量任务 JSON 规范

接受对象形式或裸数组形式；每个任务：`source`（必填）+ `output`（必填，格式按扩展名）
+ `settings`（可选，仅 PDF 任务消费，见第 5 节）。

```json
{
  "tasks": [
    { "source": "D:/drawings/a.json", "output": "D:/plots/a.pdf",
      "settings": { "pageSize": 12, "orientation": 1, "fitToPage": true } },
    { "source": "D:/drawings/b.dxf", "output": "D:/plots/b.pdf" },
    { "source": "D:/drawings/c.json", "output": "D:/plots/c.dwg" }
  ]
}
```

结果 JSON（BatchPlot 返回值 / GetTaskStatus）：

```json
{
  "allOk": true, "total": 3, "succeeded": 3, "failed": 0,
  "tasks": [
    { "index": 0, "source": "D:/drawings/a.json", "output": "D:/plots/a.pdf",
      "ok": true, "error": "", "bytes": 3307, "elapsedMs": 56 }
  ]
}
```

---

## 5. PrintSettings JSON（PDF 版式，全部字段可选）

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| pageSize | int | 8 (A4) | Qt QPageSize 枚举值；A3=12，A2=13，A1=14，A0=15 |
| orientation | int | 0 | 0=纵向 Portrait，1=横向 Landscape |
| marginL/T/R/B | double | 10.0 | 四边页边距（mm） |
| scale | double | 1.0 | 1:1 时模型 mm → 图纸 mm 比例（fitToPage 优先） |
| lineWidthScale | double | 0.5 | 打印线宽整体缩放（<1 变细） |
| fitToPage | bool | true | 自适应铺满可打印区 |
| centerH / centerV | bool | true | 水平/垂直居中 |
| drawBorder | bool | true | 绘制可打印区边框 |

---

## 6. 调用示例

### VBScript

```vbscript
Dim cad
Set cad = CreateObject("SketchCAD.Automation")
cad.Start
cad.RegisterCallback New PlotEvents          ' 可选：事件宿主

Dim r
r = cad.BatchPlot("{""tasks"":[{""source"":""D:/a.json"",""output"":""D:/a.pdf""}]}")
If Left(cad.LastError, 8) = "SCAD_S_OK" Or r <> "" Then
    WScript.Echo "出图完成：" & r
End If
cad.Quit

Class PlotEvents
    Public Sub OnTaskStart(taskJson)  : End Sub
    Public Sub OnTaskDone(taskJson)
        WScript.Echo "任务完成：" & taskJson
    End Sub
    Public Sub OnBatchFinished(batchJson) : End Sub
End Class
```

### PowerShell

```powershell
$cad = New-Object -ComObject SketchCAD.Automation
$null = $cad.Start()
$null = $cad.OpenDrawing("D:\drawings\part.json")
$null = $cad.PlotToPdf("D:\plots\part.pdf")
$tasks = '{"tasks":[{"source":"D:/a.json","output":"D:/a.pdf"},
                  {"source":"D:/b.dxf","output":"D:/b.dwg"}]}'
$result = $cad.BatchPlot($tasks, "D:\plots\batch.log")
$cad.Quit()
[Console]::WriteLine($result)
```

### Python（win32com 后期绑定）

```python
import win32com.client
cad = win32com.client.Dispatch("SketchCAD.Automation")
cad.Start()
tasks = '{"tasks":[{"source":"D:/a.json","output":"D:/a.pdf"}]}'
result = cad.BatchPlot(tasks, "D:/plots/batch.log")
import json
summary = json.loads(result)
print("全部成功" if summary["allOk"] else f"失败 {summary['failed']} 个")
cad.Quit()
```

### C#

```csharp
Type t = Type.GetTypeFromProgID("SketchCAD.Automation");
dynamic cad = Activator.CreateInstance(t);
cad.Start();
string result = cad.BatchPlot(
    "{\"tasks\":[{\"source\":\"D:/a.json\",\"output\":\"D:/a.pdf\"}]}");
cad.Quit();
```

---

## 7. 稳定性保障

* **并发**：STA 线程模型 + 引擎进程级互斥锁——并发 BatchPlot 在锁上排队串行执行，
  不会交叉写同一输出文件。
* **异常屏障**：COM 层与引擎层的跨界回调全程 try/catch 包裹，异常不逃逸出 DLL 边界。
* **日志**：BatchPlot 第二参数指定日志文件后，逐任务追加
  `时间戳 任务 i/N 状态 src=.. out=.. 明细`，批量结束追加汇总行。
* **任务隔离**：每个批量任务使用独立 SketchModel，任务间零串扰；单任务失败不影响后续任务。

## 8. 故障排查

| 现象 | 处置 |
|---|---|
| CreateObject 失败 0x80040154 | DLL 未注册：`regsvr32 SketchCADAutomation.dll`；或缺 Qt 运行库 |
| PDF 中文变豆腐块 | offscreen/无字体环境：确保系统字体目录可用（Windows 桌面会话默认可用） |
| 批量返回 SCAD_S_PARTIAL | 读结果 JSON `tasks[].error` 逐项定位 |
| regsvr32 报缺少依赖 | 把 DLL 放到 SketchCAD.exe 同目录（共享 Qt/MinGW 运行库） |
