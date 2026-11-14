#include "core/Layer.h"
#include "core/standards/StandardsDB.h"

namespace SketchCAD {

namespace {
// 判定图层名 n 是否匹配某标准条目 e（多种分隔符 + 裸名，见 Layer.h 注释）。
bool matchStandard(const QString& n, const StandardLayerEntry& e) {
    const QString nm = e.name;
    return n == e.code + QStringLiteral("_") + nm
        || n == e.code + QStringLiteral(" ") + nm
        || n == e.code + QStringLiteral("-") + nm
        || n == e.code + nm
        || n == nm;
}
} // namespace

namespace LayerNaming {

std::vector<StandardLayerEntry> standardLayerEntries() {
    std::vector<StandardLayerEntry> out;
    const StandardsDB& db = StandardsDB::instance();
    // layer_01..layer_09（GB/T 18617 / ISO 13567 标准图层映射）
    static const char* kCodes[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09"};
    for (const char* c : kCodes) {
        const QString code = QString::fromUtf8(c);
        // 中文比较一律用 queryString 返回的 QString 做精确相等，禁用 QLatin1String 字面量。
        const QString v = db.queryString("cad_layers", QStringLiteral("layer_") + code);
        if (v.isEmpty()) continue;
        StandardLayerEntry e;
        e.code = code;
        e.name = v;
        out.push_back(std::move(e));
    }
    return out;
}

QStringList suggestLayerNames() {
    QStringList out;
    for (const auto& e : standardLayerEntries())
        out.append(e.code + QStringLiteral("_") + e.name);
    return out;
}

bool isLayerNameCompliant(const QString& name, QString* reason) {
    const QString n = name.trimmed();
    if (n.isEmpty()) {
        if (reason) *reason = QStringLiteral("图层名不能为空");
        return false;
    }
    if (n == QStringLiteral("0")) return true;   // 保留默认层
    const auto entries = standardLayerEntries();
    for (const auto& e : entries) {
        if (matchStandard(n, e)) return true;
    }
    if (reason) {
        *reason = QStringLiteral("未匹配 GB/T 18617/ISO 13567 标准图层名，建议形如 01_轮廓线");
    }
    return false;
}

QVector<LayerNamingIssue> checkLayerNaming(const QStringList& names) {
    QVector<LayerNamingIssue> issues;
    for (const QString& nm : names) {
        QString reason;
        if (!isLayerNameCompliant(nm, &reason)) {
            LayerNamingIssue iss;
            iss.layerName = nm;
            iss.reason = reason;
            issues.append(iss);
        }
    }
    return issues;
}

} // namespace LayerNaming

} // namespace SketchCAD
