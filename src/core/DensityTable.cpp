#include "DensityTable.h"
#include <QHash>

namespace SketchCAD {
namespace {

// ⚠️ 人工提供、禁改数值（参考值 g/cm³）。新增材料请由人工确认后追加。
const QHash<QString, double>& table() {
    static const QHash<QString, double> t = {
        { QStringLiteral("钢"),       7.85 },
        { QStringLiteral("铸铁"),     7.20 },
        { QStringLiteral("不锈钢"),   7.98 },
        { QStringLiteral("铝合金"),   2.70 },
        { QStringLiteral("铜"),       8.90 },
        { QStringLiteral("黄铜"),     8.50 },
        { QStringLiteral("铅"),      11.34 },
        { QStringLiteral("橡胶"),     1.50 },
        { QStringLiteral("尼龙"),     1.15 },
        { QStringLiteral("木材"),     0.60 },
    };
    return t;
}

} // namespace

double DensityTable::density(const QString& material) {
    const auto& t = table();
    auto it = t.find(material);
    return (it != t.end()) ? it.value() : 0.0;
}

bool DensityTable::has(const QString& material) {
    return table().contains(material);
}

QStringList DensityTable::materials() {
    return table().keys();
}

} // namespace SketchCAD
