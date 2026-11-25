// StdPartTemplateBuilder —— 把源图库 DWG 批量转换为占位符模板并存库
// 用法：
//   StdPartTemplateBuilder build <图库根> <模板库目录> [分片号] [分片总数]
//   StdPartTemplateBuilder merge <模板库目录> <分片数>
//   输出：<模板库目录>/index.json + tpl/<id>.tpl（qCompress 的 DXF）+ build_report_N.json
//   记录每个文件的成功/失败及原因；失败文件也写入索引(ok=false)便于审计。
#include "core/StdPartTemplate.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

using namespace SketchCAD;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QLoggingCategory::setFilterRules(QStringLiteral("*.warning=false"));
    if (argc < 2) {
        std::printf("用法: StdPartTemplateBuilder build|merge ...\n");
        return 2;
    }
    const QString cmd = QString::fromLocal8Bit(argv[1]);

    if (cmd == QStringLiteral("merge")) {
        if (argc < 4) return 2;
        const QString store = QString::fromLocal8Bit(argv[2]);
        const int count = QString::fromLocal8Bit(argv[3]).toInt();
        QJsonArray all;
        int ok = 0, fail = 0;
        for (int s = 0; s < count; ++s) {
            QFile f(QDir(store).filePath(QStringLiteral("index_shard_%1.json").arg(s)));
            if (!f.open(QIODevice::ReadOnly)) { std::printf("[WARN] 缺分片 %d\n", s); continue; }
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            for (const QJsonValue& v : o.value(QStringLiteral("templates")).toArray()) {
                all.append(v);
                v.toObject().value(QStringLiteral("ok")).toBool() ? ++ok : ++fail;
            }
        }
        QJsonObject rootObj;
        rootObj[QStringLiteral("schemaVersion")] = 1;
        rootObj[QStringLiteral("placeholderRule")] = QStringLiteral("{{KEY}} 双花括号全大写下划线");
        rootObj[QStringLiteral("keys")] = QJsonArray::fromStringList(StdPartPlaceholders::keys());
        rootObj[QStringLiteral("templates")] = all;
        QFile of(QDir(store).filePath(QStringLiteral("index.json")));
        if (!of.open(QIODevice::WriteOnly)) { std::printf("[FAIL] 无法写 index.json\n"); return 1; }
        of.write(QJsonDocument(rootObj).toJson(QJsonDocument::Compact));
        of.close();
        std::printf("[OK] 合并完成: total=%d ok=%d fail=%d\n", all.size(), ok, fail);
        return 0;
    }

    if (cmd != QStringLiteral("build") || argc < 4) {
        std::printf("用法: StdPartTemplateBuilder build <图库根> <模板库目录> [分片号] [分片总数]\n");
        return 2;
    }
    const QString root = QString::fromLocal8Bit(argv[2]);
    const QString store = QString::fromLocal8Bit(argv[3]);
    const int shard = (argc > 4) ? QString::fromLocal8Bit(argv[4]).toInt() : 0;
    const int count = (argc > 5) ? QString::fromLocal8Bit(argv[5]).toInt() : 1;

    QDir().mkpath(QDir(store).filePath(QStringLiteral("tpl")));

    QStringList all;
    QDirIterator it(root, QStringList{QStringLiteral("*.dwg"), QStringLiteral("*.DWG")},
                    QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) all << it.next();
    std::sort(all.begin(), all.end());

    QVector<StdPartTemplateMeta> metas;
    QJsonArray report;
    int ok = 0, fail = 0, done = 0;
    for (int i = 0; i < all.size(); ++i) {
        if (count > 1 && (i % count) != shard) continue;
        const QString path = all[i];
        const QString rel = QDir(root).relativeFilePath(path);
        const QFileInfo fi(path);
        const QString cat = fi.dir().dirName();
        StdPartTemplateMeta meta;
        QByteArray tpl;
        QFile f(path);
        bool success = false;
        QString err;
        if (!f.open(QIODevice::ReadOnly)) {
            meta.id = QString::number(qHash(rel), 16);
            meta.category = cat; meta.sourceFile = rel;
            meta.ok = false; meta.failReason = QStringLiteral("无法打开源文件");
        } else {
            const QByteArray raw = f.readAll();
            f.close();
            success = StdPartTemplateStore::buildFromDwg(raw, rel, cat, tpl, meta, &err);
            if (success) {
                QFile of(QDir(store).filePath(meta.tplFile));
                if (of.open(QIODevice::WriteOnly)) {
                    of.write(qCompress(tpl, 9));
                    of.close();
                } else {
                    meta.ok = false;
                    meta.failReason = QStringLiteral("模板文件写入失败");
                    success = false;
                }
            }
        }
        metas.append(meta);
        QJsonObject r;
        r[QStringLiteral("sourceFile")] = rel;
        r[QStringLiteral("ok")] = meta.ok;
        r[QStringLiteral("reason")] = meta.failReason;
        r[QStringLiteral("elementCount")] = meta.elementCount;
        report.append(r);
        meta.ok ? ++ok : ++fail;
        if (++done % 200 == 0)
            std::printf("[进度] 分片 %d/%d: %d/%d 成功=%d 失败=%d\n",
                        shard, count, done, (all.size() + count - 1) / count, ok, fail);
    }

    QString ie;
    // 分片写独立索引，避免多进程互相覆盖：先写临时目录的 index.json 再改名
    const QString indexName = (count > 1)
        ? QStringLiteral("index_shard_%1.json").arg(shard)
        : QStringLiteral("index.json");
    const QString tmpDir = QDir(store).filePath(QStringLiteral("_shardtmp_%1").arg(shard));
    QDir().mkpath(tmpDir);
    if (!StdPartTemplateStore::writeIndex(tmpDir, metas, &ie)) {
        std::printf("[FAIL] %s\n", qPrintable(ie));
        return 1;
    }
    const QString dst = QDir(store).filePath(indexName);
    QFile::remove(dst);
    if (!QFile::rename(QDir(tmpDir).filePath(QStringLiteral("index.json")), dst)) {
        std::printf("[FAIL] 索引改名失败: %s\n", qPrintable(dst));
        return 1;
    }
    QDir(tmpDir).rmdir(QDir(tmpDir).filePath(QStringLiteral(".")));
    QJsonObject rep;
    rep[QStringLiteral("shard")] = shard;
    rep[QStringLiteral("shardCount")] = count;
    rep[QStringLiteral("total")] = done;
    rep[QStringLiteral("ok")] = ok;
    rep[QStringLiteral("fail")] = fail;
    rep[QStringLiteral("details")] = report;
    QFile rf(QDir(store).filePath(QStringLiteral("build_report_%1.json").arg(shard)));
    if (rf.open(QIODevice::WriteOnly)) {
        rf.write(QJsonDocument(rep).toJson(QJsonDocument::Compact));
        rf.close();
    }
    std::printf("[OK] 分片 %d 完成: total=%d ok=%d fail=%d -> %s\n",
                shard, done, ok, fail, qPrintable(store));
    return 0;
}
