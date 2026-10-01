#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <vector>
#include "Vec.h"

// 序列化工具集中层（DRY 提取）。
// 消除全工程散落的「o.contains(k) ? o[k].toX(def) : def」回退样板，
// 以及 [x,y,z] 点数组的重复解析/生成循环。
//
// 行为与原手写完全一致：QJsonObject::value(k) 在缺键时返回 null，
// null.toX(def) 回落 def；因此 json::getX(o,k,def) 等价于
// 「contains ? o[k].toX(def) : def」，可安全替换且不改变任何既有存档读逻
// 辑（含向后兼容的默认回退）。
namespace SketchCAD {
namespace json {

inline double getDouble(const QJsonObject& o, const char* k, double def) {
    return o.value(k).toDouble(def);
}
inline int getInt(const QJsonObject& o, const char* k, int def) {
    return o.value(k).toInt(def);
}
inline bool getBool(const QJsonObject& o, const char* k, bool def) {
    return o.value(k).toBool(def);
}
inline QString getString(const QJsonObject& o, const char* k, const QString& def = QString()) {
    return o.value(k).toString(def);
}
inline QJsonArray getArray(const QJsonObject& o, const char* k) {
    return o.value(k).toArray();
}
inline QJsonObject getObject(const QJsonObject& o, const char* k) {
    return o.value(k).toObject();
}

// [x,y,z] 三元点数组 <-> Vec3 序列（序列化专用，消除重复解析循环）
inline std::vector<Vec3> readVec3Array(const QJsonArray& a) {
    std::vector<Vec3> out;
    out.reserve(a.size());
    for (const auto& v : a) {
        const QJsonArray p = v.toArray();
        out.push_back(Vec3(p[0].toDouble(), p[1].toDouble(), p[2].toDouble()));
    }
    return out;
}
inline std::vector<Vec3> readVec3Array(const QJsonObject& o, const char* k) {
    return readVec3Array(o.value(k).toArray());
}

// 嵌套点环（Hatch 内孔等）专用读取
inline std::vector<std::vector<Vec3>> readVec3Rings(const QJsonArray& a) {
    std::vector<std::vector<Vec3>> out;
    out.reserve(a.size());
    for (const auto& ringVal : a) out.push_back(readVec3Array(ringVal.toArray()));
    return out;
}
inline std::vector<std::vector<Vec3>> readVec3Rings(const QJsonObject& o, const char* k) {
    return readVec3Rings(o.value(k).toArray());
}

inline QJsonArray writeVec3Array(const std::vector<Vec3>& pts) {
    QJsonArray a;
    for (const auto& p : pts) a.append(QJsonArray{p.x, p.y, p.z});
    return a;
}
inline QJsonArray writeVec3Rings(const std::vector<std::vector<Vec3>>& rings) {
    QJsonArray a;
    for (const auto& r : rings) a.append(writeVec3Array(r));
    return a;
}

} // namespace json
} // namespace SketchCAD
