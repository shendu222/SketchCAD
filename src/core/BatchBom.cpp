#include "BatchBom.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace SketchCAD {
namespace {

// 汇总键：(名称, 代号) → 用不可见分隔符 US 拼合，避免 "a|b"+"c" 与 "a"+"b|c" 式碰撞
inline QString rowKey(const QString& name, const QString& code) {
    return name + QChar(0x1f) + code;
}

} // namespace

// 说明（IO_SPEC「批量扫描只读档头，避免全量解析」）：
//   不实例化 SketchModel / 不构建任何 Element，仅取 JSON 顶层 "bom" 数组。
//   ⚠️ 偏差：Qt 核心无流式 JSON 解析器，QJsonDocument::fromJson 仍会解析整份文本；
//   但「全量解析」指代的模型/图元层面构建被完全避免，内存与 CPU 的大头在图元实例化。
QVector<BomSummaryRow> BatchBom::analyze(const QStringList& paths, QStringList* readErrors) {
    QVector<BomSummaryRow> rows;
    QHash<QString, int> index;   // 键 → rows 下标（保首见顺序）

    for (const QString& path : paths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (readErrors) *readErrors << QStringLiteral("无法打开：%1").arg(path);
            continue;
        }
        const QByteArray bytes = f.readAll();
        f.close();

        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (readErrors)
                *readErrors << QStringLiteral("JSON 解析失败：%1（%2）").arg(path, perr.errorString());
            continue;
        }
        const QJsonValue bomv = doc.object().value(QStringLiteral("bom"));
        if (!bomv.isArray()) {
            if (readErrors) *readErrors << QStringLiteral("无 bom 明细键：%1").arg(path);
            continue;
        }

        const QJsonArray arr = bomv.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonValue v = arr.at(i);
            if (!v.isObject()) {
                if (readErrors)
                    *readErrors << QStringLiteral("%1 第 %2 条 bom 记录非对象").arg(path).arg(i + 1);
                continue;
            }
            const QJsonObject o  = v.toObject();
            const QString   name = o.value(QStringLiteral("name")).toString();
            const QString   code = o.value(QStringLiteral("code")).toString();
            const QString   key  = rowKey(name, code);
            auto it = index.find(key);
            if (it == index.end()) {
                index.insert(key, rows.size());
                BomSummaryRow r;
                r.name     = name;
                r.code     = code;
                r.quantity = 1;          // 每条 BomItem = 该档中一件实例
                rows.push_back(r);
            } else {
                rows[it.value()].quantity += 1;
            }
        }
    }
    return rows;
}

} // namespace SketchCAD
