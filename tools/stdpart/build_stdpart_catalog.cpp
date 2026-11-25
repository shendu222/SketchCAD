// StdPartCatalogBuilder —— 合并分片解析索引、生成参数化目录、验证参数驱动
// 用法：
//   StdPartCatalogBuilder merge <out_index.json> <shard_0.json> [shard_1.json ...]
//   StdPartCatalogBuilder build <index.json> <catalog.json>
//   StdPartCatalogBuilder verify <catalog.json>
#include "core/StdPartCatalog.h"
#include "core/CadFormats.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cstdio>

using namespace SketchCAD;

static bool merge(const QString& out, const QStringList& shards) {
    QJsonArray files, categories;
    int ok = 0, fail = 0;
    QSet<QString> seen;
    for (const QString& s : shards) {
        QFile f(s);
        if (!f.open(QIODevice::ReadOnly)) { std::printf("[WARN] 跳过 %s\n", qPrintable(s)); continue; }
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonArray fs = o.value(QStringLiteral("files")).toArray();
        for (const auto& v : fs) {
            QJsonObject r = v.toObject();
            const QString rel = r.value(QStringLiteral("relativePath")).toString();
            if (rel.isEmpty() || seen.contains(rel)) continue;
            seen.insert(rel);
            files.append(r);
            r.value(QStringLiteral("ok")).toBool() ? ++ok : ++fail;
        }
        const QJsonArray cs = o.value(QStringLiteral("categories")).toArray();
        for (const auto& c : cs) {
            const QString dir = c.toObject().value(QStringLiteral("dir")).toString();
            if (!seen.contains(QStringLiteral("CAT:") + dir)) {
                seen.insert(QStringLiteral("CAT:") + dir);
                categories.append(c);
            }
        }
    }
    QJsonObject root;
    root[QStringLiteral("summary")] = QJsonObject({
        {QStringLiteral("totalFiles"), files.size()},
        {QStringLiteral("parsedOk"), ok},
        {QStringLiteral("parsedFail"), fail},
        {QStringLiteral("categoryCount"), categories.size()}
    });
    root[QStringLiteral("categories")] = categories;
    root[QStringLiteral("files")] = files;
    QFile of(out);
    if (!of.open(QIODevice::WriteOnly)) return false;
    of.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    of.close();
    std::printf("[OK] 合并完成: files=%d ok=%d fail=%d categories=%d -> %s\n",
                files.size(), ok, fail, categories.size(), qPrintable(out));
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::printf("用法: StdPartCatalogBuilder merge|build|verify ...\n");
        return 2;
    }
    const QString cmd = QString::fromLocal8Bit(argv[1]);
    if (cmd == QStringLiteral("merge")) {
        if (argc < 4) return 2;
        QStringList shards;
        for (int i = 3; i < argc; ++i) shards << QString::fromLocal8Bit(argv[i]);
        return merge(QString::fromLocal8Bit(argv[2]), shards) ? 0 : 1;
    }
    if (cmd == QStringLiteral("build")) {
        if (argc < 4) return 2;
        QString err; int cc = 0, sc = 0;
        const bool ok = StdPartCatalog::buildFromAnalysis(
            QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]),
            &err, &cc, &sc);
        if (!ok) { std::printf("[FAIL] %s\n", qPrintable(err)); return 1; }
        std::printf("[OK] 目录生成: categories=%d specs=%d -> %s\n", cc, sc, qPrintable(QString::fromLocal8Bit(argv[3])));
        return 0;
    }
    if (cmd == QStringLiteral("verify")) {
        QString err;
        if (!StdPartCatalog::instance().load(QString::fromLocal8Bit(argv[2]), &err)) {
            std::printf("[FAIL] %s\n", qPrintable(err)); return 1;
        }
        const auto ids = StdPartCatalog::instance().ids();
        int ok = 0, fail = 0;
        for (const QString& id : ids) {
            const StdPartCategory* c = StdPartCatalog::instance().find(id);
            QVariantMap p;
            for (const auto& d : c->params) p[d.key] = d.defaultValue;
            std::vector<Element> out;
            QString note;
            const bool r = StdPartCatalog::instance().generate(id, p, out, &note, true);
            if (r && !out.empty()) ++ok; else { ++fail; std::printf("[FAIL] %s: %s\n", qPrintable(id), qPrintable(note)); }
        }
        std::printf("[OK] 参数驱动验证: %d/%d 成功\n", ok, ids.size());
        return fail ? 1 : 0;
    }
    return 2;
}
