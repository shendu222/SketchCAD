# SketchCAD SDK

原生 C++ SDK（对标 ObjectARX）——以纯头文件形式发布，第三方据此开发可在 SketchCAD 宿主中加载的插件。

## 目录

```
sdk/
├── include/SketchCAD/     ← 公共 API（分发时整目录拷贝）
│   ├── sdk_version.h        ABI/语义版本常量
│   ├── sketchcad_sdk.h      导出宏
│   ├── types.h              SdkPoint / SdkElementInfo（POD）
│   ├── i_plugin.h           IPlugin + PluginInfo（插件接口规范）
│   ├── i_model.h            IModel（图纸图元接口）
│   ├── i_application.h      IApplication + SdkCommandFn（运行时接口）
│   └── plugin_entry.h       入口契约 + SKETCHCAD_PLUGIN_ENTRY 宏
└── samples/SamplePlugin/    机械行业示例插件（drawGear / mechHello）
```

## 三步接入

1. 读契约：`docs/SDK_GUIDE.md`（宿主仓库内）；
2. 抄示例：`samples/SamplePlugin/SamplePlugin.cpp`，继承 `IPlugin` + 一行 `SKETCHCAD_PLUGIN_ENTRY`；
3. 编译投放：`-shared -DSKETCHCAD_PLUGIN_BUILD -I<sdk>/include` → 把 DLL 放进宿主 exe 同目录 `plugins/`。

## 硬约束（ABI 1）

- 与宿主**同编译器/C++ 运行时**（本项目：MinGW-w64 GCC 13.1）；
- 公共接口只用 POD / `const char*`（UTF-8）/ 纯虚类，无 STL/Qt 跨边界；
- 命令回调签名 `bool (*)(const char* args)`；内置命令优先，重名注册被拒绝；
- 实例释放走 `IPlugin::dispose()`（默认 `delete this`），宿主不直接 delete。
