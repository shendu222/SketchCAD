# SketchCAD 原生 SDK —— 第三方插件开发指南

> 版本：1.0.0（ABI 1）　适用宿主：SketchCAD（C++17 / Qt 6 / MinGW-w64）
> 适用读者：机械 / 建筑 / 电气等行业工具的开发者。

---

## 1. SDK 能做什么

SketchCAD 原生 SDK（对标 AutoCAD ObjectARX 的分层思想）允许你在**不修改宿主源码**的前提下，以 C++ 动态库（Windows `.dll`）形式为宿主程序注入新能力：

| 能力 | SDK 接口 | ObjectARX 对应 |
|---|---|---|
| 读写图纸图元（直线/圆/弧/矩形/点/文字） | `IModel` | `AcDbDatabase` |
| 注册命令行命令（用户键入即触发） | `IApplication::registerCommand` | `acedRegCmds` |
| 写宿主日志面板 | `IModel::log` / `IApplication::log` | `acutPrintf` |
| 版本协商（加载时 ABI 校验） | `sketchcad_sdk_abi_version()` | `acrxRegisterApp` |

插件的加载与生命周期（`initialize → 运行 → shutdown → 卸载`）由宿主内置的 PluginManager 全权管理。

---

## 2. 版本与依赖说明

- **ABI 版本** `SKETCHCAD_SDK_ABI_VERSION`（当前 `1`）：仅当接口签名/POD 布局发生**二进制不兼容变更**时递增。宿主加载插件时执行**运行时协商**：插件导出的 `sketchcad_sdk_abi_version()` 落在宿主支持区间 `[SKETCHCAD_SDK_ABI_MIN_SUPPORTED, SKETCHCAD_SDK_ABI_VERSION]`（当前 1..1）内即接受；**高于宿主**的插件被拒绝（请升级宿主），**低于区间下限**的被拒绝（宿主不再保证该代际接口存在）。低于宿主当前 ABI 的插件以「兼容模式」加载，宿主日志记录协商结果。
- **语义版本** `SKETCHCAD_SDK_VERSION_STRING`（当前 `"1.0.0"`）：功能新增/文档更新时递增，不影响二进制兼容。
- **编译依赖**：插件**只需** `sdk/include/` 下的头文件（纯 C++ 抽象接口 + POD），**无需链接宿主任何库**。
- **工具链约束**：插件必须与宿主使用**相同的编译器与 C++ 运行时**（本项目为 MinGW-w64 GCC 13.1 / Qt 6.10.1 环境），以保证 vtable 与堆分配兼容。这与 ObjectARX 要求固定 Visual C++ 版本是同一道理。
- **坐标约定**：世界坐标，单位 mm；右手系 X 右 / Y 上；角度单位度；字符串一律 UTF-8。

---

## 3. 接口契约（必须遵循）

一个合法插件必须满足：

1. **继承并实现** `SketchCAD::sdk::IPlugin` 的三个纯虚方法：
   - `info()` —— 返回指向**静态** `PluginInfo` 的指针（name/version/description/category/sdkAbiVersion）；
   - `initialize(IApplication* app)` —— 加载时调用一次；返回 `false` 视为加载失败。在此注册命令并缓存 `app` 指针；
   - `shutdown()` —— 卸载时调用一次，释放自身资源。
2. **导出两个 C 链接符号**（用 `SKETCHCAD_PLUGIN_ENTRY(YourClass)` 一键生成）：
   - `int sketchcad_sdk_abi_version()` —— 返回 `SKETCHCAD_SDK_ABI_VERSION`；
   - `IPlugin* sketchcad_create_plugin()` —— 返回 `new YourClass()`。
3. **编译时**定义 `SKETCHCAD_PLUGIN_BUILD` 并包含 `sdk/include`。
4. **命令回调**签名固定为 `bool (*)(const char* args)`，`args` 为命令名之后的剩余文本（UTF-8）。
5. 不得在 `info()` 返回的字符串上做释放/改写；不得在 `shutdown()` 之后再使用 `app` 指针。

---

## 4. 快速上手（完整可编译示例）

```cpp
// MyPlugin.cpp
#include "SketchCAD/plugin_entry.h"
#include "SketchCAD/i_plugin.h"
#include "SketchCAD/i_application.h"
#include "SketchCAD/i_model.h"

using namespace SketchCAD::sdk;

static IApplication* g_app = nullptr;

// 命令：drawBeam L H —— 画一根工字梁示意
static bool cmdDrawBeam(const char* args) {
    double L = 100, H = 20;
    sscanf(args, "%lf %lf", &L, &H);          // 生产代码请做完整解析
    IModel* m = g_app->model();
    m->addLine(0, 0, L, 0);                    // 下翼缘
    m->addLine(0, H, L, H);                    // 上翼缘
    m->addLine(L / 2, 0, L / 2, H);            // 腹板
    g_app->log("工字梁已生成");
    return true;
}

class BeamPlugin : public IPlugin {
public:
    const PluginInfo* info() const override { return &kInfo_; }
    bool initialize(IApplication* app) override {
        g_app = app;
        return app->registerCommand("drawBeam", cmdDrawBeam);
    }
    void shutdown() override { g_app = nullptr; }
private:
    static const PluginInfo kInfo_;
};

const PluginInfo BeamPlugin::kInfo_ = {
    "建筑构件插件", "1.0.0", "工字梁等建筑构件绘制", "建筑",
    SKETCHCAD_SDK_ABI_VERSION
};

SKETCHCAD_PLUGIN_ENTRY(BeamPlugin)
```

编译（MinGW 环境）：

```bash
g++ -shared -std=gnu++17 -O2 -DSKETCHCAD_PLUGIN_BUILD \
    -I<SketchCAD根目录>/sdk/include \
    MyPlugin.cpp -o MyPlugin.dll
```

把 `MyPlugin.dll` 放入宿主 exe 同目录的 **`plugins/`** 子目录，启动 SketchCAD 即自动加载；也可经菜单【插件 → 加载插件文件...】手动加载。之后在命令行键入：

```
drawBeam 120 25
```

---

## 5. IModel 方法速查

| 方法 | 说明 |
|---|---|
| `addLine(x1,y1,x2,y2)` | 直线，返回图元 id（失败 -1） |
| `addCircle(cx,cy,r)` | 圆 |
| `addArc(cx,cy,r,startDeg,endDeg)` | 圆弧 |
| `addRect(x,y,w,h)` | 矩形 |
| `addPoint(x,y)` | 点 |
| `addText(x,y,text,heightMm)` | 文字（UTF-8） |
| `elementCount() / elementExists(id)` | 数量 / 存在性 |
| `elementInfo(id, SdkElementInfo*)` | 类型名/图层/点数快照 |
| `elementTypeName(id) / elementLayer(id)` | UTF-8 字符串（**下次读调用前有效**） |
| `elementPointCount(id) / elementPoint(id,i,out)` | 控制点读取 |
| `removeElement(id)` | 删除 |
| `log(msg)` | 写日志面板 |

> 字符串生命周期：`elementTypeName / elementLayer / elementInfo` 返回的 `const char*` 指向宿主内部缓冲，仅保证到**下一次对该模型的读调用**；需要长期保存请自行拷贝。

---

## 6. 生命周期与卸载

```
QLibrary::load → abi_version 校验 → create_plugin()
      → initialize(&app)                    ── 失败则 shutdown+dispose+卸载 DLL
      → 【运行期】命令回调 / IModel 调用
      → unloadPlugin / unloadAll / 程序退出
      → shutdown() → dispose()（默认 delete this）→ QLibrary::unload
```

- 实例释放走虚函数 `dispose()`（默认 `delete this`），保证内存回收发生在插件自身模块，规避跨 DLL `delete` 兼容问题。
- 全程 `try/catch` 包裹插件调用：插件抛出的异常**不会**击穿宿主进程，只会令该插件加载/卸载失败并写入日志。
- 卸载插件**不会**删除它已绘制的图元（图纸数据归宿主持有）。

---

## 7. 常见问题

**Q：加载失败提示"ABI 版本过新"？**
A：你的插件基于比宿主更新的 SDK 头构建。要么升级宿主程序，要么改用与宿主同代际的 `sdk/include/` 重新编译。

**Q：加载失败提示"ABI 版本过旧或非法"？**
A：插件声明的 ABI 低于宿主支持区间下限（`SKETCHCAD_SDK_ABI_MIN_SUPPORTED`），宿主已不再保证该代际接口仍存在，请升级插件 SDK 头重新编译。

**Q：命令注册返回 false？**
A：命令名与内置命令或已注册命令重名。内置命令优先，请换名。

**Q：插件 DLL 加载报缺少依赖？**
A：插件与宿主须同工具链构建；Qt 不需要链接，但 C++ 运行时（libstdc++/libgcc）须与宿主进程内已加载的一致。

**Q：能弹自己的对话框吗？**
A：当前 SDK 面向命令行 + 图元 API；GUI 扩展（菜单/面板钩子）规划于 ABI 2。

---

*更详细的架构说明见 `docs/sdk-plugin-framework-design-analysis.md`；可直接编译运行的机械行业示例见 `sdk/samples/SamplePlugin/`。*
