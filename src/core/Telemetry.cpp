#include "Telemetry.h"

namespace SketchCAD {

namespace {
    TelemetrySink* g_sink = nullptr;   // 默认无出口 → record 永远是空操作
    bool g_enabled = false;            // 默认关闭 → 双保险之一
}

void Telemetry::setSink(TelemetrySink* sink) { g_sink = sink; }
TelemetrySink* Telemetry::sink() { return g_sink; }

void Telemetry::setEnabled(bool on) { g_enabled = on; }
bool Telemetry::enabled() { return g_enabled; }

void Telemetry::record(const QString& event, const QVariantMap& payload) {
    // 双重门闩：开关 + 出口，缺一不发（"默认零外发"的机制保证）
    if (!g_enabled || !g_sink) return;
    g_sink->send(event, payload);
}

} // namespace SketchCAD
