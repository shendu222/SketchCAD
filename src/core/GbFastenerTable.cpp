#include "GbFastenerTable.h"
#include <QJsonObject>
#include <cmath>
#include <algorithm>
#include <map>
#include <vector>

namespace SketchCAD {
namespace {

// ⚠️ 人工给定(G3)的 GB 规格默认数值（参考值，须人工校核；禁止代码改写）。
// 首批评选常用公称规格 M3~M16。运行时经 store() 可被 fromJson 覆盖（库维护）。
struct Row { double d; double a; double b; double c; };  // 按类型解释 a/b/c（见 build()）

// GB/T 5782 六角头螺栓：a=对边 s, b=头高 k, c=螺纹内径(≈0.92d 简化)
const Row kDefBolt[] = {
    {3,  5.5, 2.0, 2.8}, {4,  7.0, 2.8, 3.7}, {5,  8.0, 3.5, 4.6}, {6,  10.0, 4.0, 5.5},
    {8,  13.0, 5.3, 7.4}, {10, 16.0, 6.4, 9.2}, {12, 18.0, 7.5, 11.0}, {16, 24.0, 10.0, 14.7},
};
// GB/T 6170 1型六角螺母：a=对边 s, b=厚 m, c=螺纹孔径(≈0.92d)
const Row kDefNut[] = {
    {3,  5.5, 2.4, 2.8}, {4,  7.0, 3.2, 3.7}, {5,  8.0, 4.7, 4.6}, {6,  10.0, 5.6, 5.5},
    {8,  13.0, 6.8, 7.4}, {10, 16.0, 8.4, 9.2}, {12, 18.0, 10.8, 11.0}, {16, 24.0, 14.8, 14.7},
};
// GB/T 97.1 平垫圈-A级：a=内径 d1, b=外径 d2, c=厚 h
const Row kDefWasher[] = {
    {3,  3.2, 7.0,  0.5}, {4,  4.3, 9.0,  0.8}, {5,  5.3, 10.0, 1.0}, {6,  6.4, 12.0, 1.6},
    {8,  8.4, 16.0, 1.6}, {10, 10.5, 20.0, 2.0}, {12, 13.0, 24.0, 2.5}, {16, 17.0, 30.0, 3.0},
};
constexpr int kCount = 8;   // 三张默认表行数一致（M3~M16 八档）

using Store = std::map<FastenerKind, std::vector<Row>>;
// 运行时规格存储：首访懒加载人工给定默认值；fromJson 覆盖之（库维护）。
Store& store() {
    static Store s = [] {
        Store m;
        m[FastenerKind::HexBolt].assign(kDefBolt, kDefBolt + kCount);
        m[FastenerKind::HexNut].assign(kDefNut, kDefNut + kCount);
        m[FastenerKind::PlainWasher].assign(kDefWasher, kDefWasher + kCount);
        return m;
    }();
    return s;
}
std::vector<Row>& rows(FastenerKind k) { return store()[k]; }

FastenerSpec build(FastenerKind k, const Row& r) {
    FastenerSpec s;
    s.nominal = r.d;
    switch (k) {
        case FastenerKind::HexBolt:
            s.acrossFlats = r.a; s.thickness = r.b; s.innerD = r.c;
            s.outerD = 2.0 * r.a / std::sqrt(3.0);   // 头部对角(外接圆) = 2s/√3
            break;
        case FastenerKind::HexNut:
            s.acrossFlats = r.a; s.thickness = r.b; s.innerD = r.c;
            s.outerD = 2.0 * r.a / std::sqrt(3.0);
            break;
        case FastenerKind::PlainWasher:
            s.acrossFlats = 0.0; s.innerD = r.a; s.outerD = r.b; s.thickness = r.c;
            break;
    }
    s.valid = true;
    return s;
}

} // namespace

FastenerSpec GbFastenerTable::lookup(FastenerKind kind, double nominal) {
    const auto& t = rows(kind);
    if (t.empty()) return FastenerSpec{};
    // 精确命中（容差 1e-6）
    for (const auto& r : t)
        if (std::fabs(r.d - nominal) < 1e-6) { FastenerSpec s = build(kind, r); s.valid = true; return s; }
    // 未命中：返回最接近规格（valid=false，调用方回落默认 + log）
    int best = 0; double bd = 1e18;
    for (int i = 0; i < (int)t.size(); ++i) {
        const double d = std::fabs(t[i].d - nominal);
        if (d < bd) { bd = d; best = i; }
    }
    FastenerSpec s = build(kind, t[best]);
    s.valid = false;
    return s;
}

QVector<double> GbFastenerTable::sizes(FastenerKind kind) {
    QVector<double> v;
    for (const auto& r : rows(kind)) v << r.d;
    return v;
}

QString GbFastenerTable::kindName(FastenerKind kind) {
    switch (kind) {
        case FastenerKind::HexBolt:     return QStringLiteral("六角头螺栓");
        case FastenerKind::HexNut:      return QStringLiteral("六角螺母");
        case FastenerKind::PlainWasher: return QStringLiteral("平垫圈");
    }
    return QString();
}

FastenerKind GbFastenerTable::kindFromName(const QString& name) {
    if (name == QStringLiteral("bolt") || name == QStringLiteral("hexbolt")) return FastenerKind::HexBolt;
    if (name == QStringLiteral("nut")  || name == QStringLiteral("hexnut"))  return FastenerKind::HexNut;
    if (name == QStringLiteral("washer") || name == QStringLiteral("plainwasher")) return FastenerKind::PlainWasher;
    return FastenerKind::HexBolt;
}

QString GbFastenerTable::standardTag(FastenerKind kind, double nominal) {
    const char* std = "";
    switch (kind) {
        case FastenerKind::HexBolt:     std = "GB/T 5782"; break;
        case FastenerKind::HexNut:      std = "GB/T 6170"; break;
        case FastenerKind::PlainWasher: std = "GB/T 97.1"; break;
    }
    // 整数规格不带小数
    const double r = std::round(nominal);
    const QString dn = (std::fabs(nominal - r) < 1e-9) ? QString::number((long long)r)
                                                       : QString::number(nominal, 'f', 1);
    return QStringLiteral("%1 M%2").arg(QString::fromUtf8(std)).arg(dn);
}

// ---- 库维护：规格表 ↔ JSON ----
QJsonArray GbFastenerTable::toJson(FastenerKind kind) {
    QJsonArray arr;
    for (const auto& r : rows(kind)) {
        QJsonObject o;
        o["nominal"] = r.d;
        switch (kind) {
            case FastenerKind::HexBolt:
                o["acrossFlats"] = r.a; o["headHeight"] = r.b; o["innerD"] = r.c; break;
            case FastenerKind::HexNut:
                o["acrossFlats"] = r.a; o["thickness"] = r.b; o["innerD"] = r.c; break;
            case FastenerKind::PlainWasher:
                o["innerD"] = r.a; o["outerD"] = r.b; o["thickness"] = r.c; break;
        }
        arr.append(o);
    }
    return arr;
}

bool GbFastenerTable::fromJson(FastenerKind kind, const QJsonArray& arr) {
    if (arr.isEmpty()) return false;
    std::vector<Row> out;
    out.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        if (!o.contains("nominal")) return false;   // 非法条目 → 整体拒绝（不改动现表）
        Row r;
        r.d = o.value("nominal").toDouble();
        switch (kind) {
            case FastenerKind::HexBolt:
                r.a = o.value("acrossFlats").toDouble(); r.b = o.value("headHeight").toDouble(); r.c = o.value("innerD").toDouble(); break;
            case FastenerKind::HexNut:
                r.a = o.value("acrossFlats").toDouble(); r.b = o.value("thickness").toDouble(); r.c = o.value("innerD").toDouble(); break;
            case FastenerKind::PlainWasher:
                r.a = o.value("innerD").toDouble(); r.b = o.value("outerD").toDouble(); r.c = o.value("thickness").toDouble(); break;
        }
        out.push_back(r);
    }
    if (out.empty()) return false;
    rows(kind) = out;   // 覆盖规格表（库维护入口）
    return true;
}

void GbFastenerTable::resetToDefaults(FastenerKind kind) {
    const Row* def = nullptr;
    switch (kind) {
        case FastenerKind::HexBolt:     def = kDefBolt;   break;
        case FastenerKind::HexNut:      def = kDefNut;    break;
        case FastenerKind::PlainWasher: def = kDefWasher; break;
    }
    if (def) rows(kind).assign(def, def + kCount);
}

void GbFastenerTable::resetToDefaults() {
    resetToDefaults(FastenerKind::HexBolt);
    resetToDefaults(FastenerKind::HexNut);
    resetToDefaults(FastenerKind::PlainWasher);
}

} // namespace SketchCAD
