// ============================================================================
// SketchCAD 原生 SDK —— 示例插件：机械行业示范（齿轮生成）
// ----------------------------------------------------------------------------
// 本文件是「第三方插件」的最小完整范例，演示 SDK 接口契约：
//   1) 仅包含 SDK 公共头（SketchCAD/*.h），不引用宿主任何内部头/库；
//   2) 继承 SketchCAD::sdk::IPlugin，实现 info/initialize/shutdown；
//   3) 经 SKETCHCAD_PLUGIN_ENTRY 导出 sketchcad_create_plugin / abi_version；
//   4) 在 initialize 中注册命令（drawGear / mechHello），命令回调经
//      缓存的 IApplication* 反向访问宿主模型，绘制图元。
//
// 编译方式（第三方独立构建，仅需 SDK include 目录）：
//   g++ -shared -DSKETCHCAD_PLUGIN_BUILD -I<sketchcad>/sdk/include \
//       SamplePlugin.cpp -o SamplePlugin.dll
// （须与本 SDK 使用相同编译器 / 运行时，保证 C++ ABI 与堆兼容。）
// ============================================================================

#include "SketchCAD/plugin_entry.h"
#include "SketchCAD/i_plugin.h"
#include "SketchCAD/i_application.h"
#include "SketchCAD/i_model.h"
#include "SketchCAD/types.h"

#include <cstdlib>   // std::strtod
#include <cstdio>    // std::snprintf
#include <cmath>     // std::cos / std::sin

namespace {

using namespace SketchCAD::sdk;

// 宿主能力入口（initialize 时缓存，命令回调里使用）。
IApplication* g_app = nullptr;

// 解析命令行剩余参数为 double 数组，返回成功解析个数（最多 n 个）。
int parseDoubles(const char* args, double* out, int n) {
    if (!args || !*args) return 0;
    int cnt = 0;
    const char* p = args;
    while (cnt < n) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
        char* end = nullptr;
        double v = std::strtod(p, &end);
        if (end == p) break;          // 无法解析，停止
        out[cnt++] = v;
        p = end;
    }
    return cnt;
}

// 命令：drawGear [cx cy module teeth]
// 在指定中心生成一张渐开线齿轮的简化示意（齿顶圆/分度圆/齿根圆/轴孔 + 轮齿辐板）。
bool cmdDrawGear(const char* args) {
    if (!g_app) return false;
    IModel* m = g_app->model();
    if (!m) return false;

    double p[4] = { 0.0, 0.0, 5.0, 12.0 };   // cx, cy, module, teeth
    int n = parseDoubles(args, p, 4);
    double cx    = p[0];
    double cy    = p[1];
    double module = (n >= 3 && p[2] > 0) ? p[2] : 5.0;
    int    teeth  = (n >= 4 && p[3] >= 3) ? (int)p[3] : 12;

    double pitch   = module * teeth;          // 分度圆直径
    double rPitch  = pitch / 2.0;             // 分度圆半径
    double rOut    = rPitch + module;         // 齿顶圆
    double rHub    = rPitch - 1.25 * module;  // 齿根圆（近似）
    double rBore   = module * 0.8;            // 轴孔
    if (rHub < rBore) rHub = rPitch * 0.6;

    m->addCircle(cx, cy, rOut);
    m->addCircle(cx, cy, rPitch);
    m->addCircle(cx, cy, rHub);
    m->addCircle(cx, cy, rBore);

    // 轮齿：在齿根→齿顶之间画径向短线（示意齿形分布）。
    const double PI = 3.141592653589793;
    for (int i = 0; i < teeth; ++i) {
        double a = 2.0 * PI * i / teeth;
        double x1 = cx + rHub  * std::cos(a);
        double y1 = cy + rHub  * std::sin(a);
        double x2 = cx + rOut  * std::cos(a);
        double y2 = cy + rOut  * std::sin(a);
        m->addLine(x1, y1, x2, y2);
    }

    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "齿轮已生成：齿数=%d 模数=%.2f 分度圆直径=%.2f 图元数=%d",
                  teeth, module, pitch, m->elementCount());
    g_app->log(buf);
    return true;
}

// 命令：mechHello —— 仅写一行日志，验证插件通路。
bool cmdHello(const char* /*args*/) {
    if (!g_app) return false;
    g_app->log("机械示例插件：你好！已通过原生 SDK 接入宿主。");
    return true;
}

} // namespace

// ----------------------------------------------------------------------------
// 插件实现
// ----------------------------------------------------------------------------
class MechanicalSamplePlugin : public SketchCAD::sdk::IPlugin {
public:
    const SketchCAD::sdk::PluginInfo* info() const override { return &kInfo_; }

    bool initialize(SketchCAD::sdk::IApplication* app) override {
        g_app = app;
        // 注册核心命令；若与内置命令冲突则返回 false（宿主据此视为加载失败）。
        if (!app->registerCommand("drawGear", cmdDrawGear)) return false;
        app->registerCommand("mechHello", cmdHello);
        app->log("机械示例插件已加载：可用命令 drawGear / mechHello");
        return true;
    }

    void shutdown() override {
        if (g_app) g_app->log("机械示例插件已卸载");
        g_app = nullptr;
    }

private:
    static const SketchCAD::sdk::PluginInfo kInfo_;
};

const SketchCAD::sdk::PluginInfo MechanicalSamplePlugin::kInfo_ = {
    "机械示例插件",                       // name
    "1.0.0",                             // version
    "基于原生 SDK 的机械行业示范（齿轮生成等）",  // description
    "机械",                              // category
    SKETCHCAD_SDK_ABI_VERSION            // sdkAbiVersion
};

// 一键导出插件入口（生成 sketchcad_create_plugin / sketchcad_sdk_abi_version）。
SKETCHCAD_PLUGIN_ENTRY(MechanicalSamplePlugin)
