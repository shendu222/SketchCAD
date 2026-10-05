#pragma once
#include <QString>
#include <QVariantMap>

// ============================================================================
// Telemetry：遥测出口（可选能力，默认完全关闭、零外发）。
//
// 机制上保证"默认零外发"——必须同时满足两个条件 record() 才会真正发送：
//   1. 用户在「选项 → 隐私」勾选启用（AppSettings privacy/telemetryEnabled，
//      默认 false；首启动有一次性明示文案）；
//   2. 存在已注册的 TelemetrySink（本版本不提供任何真实网络实现，
//      sink 默认为 nullptr —— 想接网络的人必须显式实现并 setSink）。
// 两者缺一，record() 都是空操作。网络层 mock 断言（自测 t_diag）即基于此：
//   注入 mock sink 后在默认状态下疯狂 record，断言 send 零调用。
// ============================================================================
namespace SketchCAD {

// 遥测出口接口：未来任何真实上报（HTTP 等）都必须实现本接口并显式注入。
class TelemetrySink {
public:
    virtual ~TelemetrySink() = default;
    // event：匿名事件名（如 "session_start"/"crash"）；payload：匿名计数类字段。
    // 实现方负责不携带用户几何/文本内容（与诊断包同一边界）。
    virtual void send(const QString& event, const QVariantMap& payload) = 0;
};

class Telemetry {
public:
    // 注入出口（不接管所有权；传 nullptr = 撤销）。默认无出口。
    static void setSink(TelemetrySink* sink);
    static TelemetrySink* sink();

    // 总开关（与 AppSettings privacy/telemetryEnabled 同步；默认 false）。
    static void setEnabled(bool on);
    static bool enabled();

    // 唯一上报入口：enabled() && sink() 均为真才调用 send，否则什么都不发生。
    static void record(const QString& event, const QVariantMap& payload = {});

private:
    Telemetry() = delete;   // 纯静态命名空间类，禁止实例化
};

} // namespace SketchCAD
