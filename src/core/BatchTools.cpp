#include "BatchTools.h"
#include "TitleBlockLayout.h"   // 签名单元格适配：fitCellText / titleBlockCellAt（单一入口复用）
#include "SheetSpec.h"          // titleBlockFields()（key ↔ label 映射单一数据源）
#include "SketchModel.h"
#include "Geometry.h"           // geo::elementBounds（包围盒中心 → 帧归属判定）
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace {

// 组名/基名文件系统净化：路径非法字符与空白 → '_'；空 → "_"
QString sanitizeName(const QString& raw) {
    QString s = raw.trimmed();
    if (s.isEmpty()) return QStringLiteral("_");
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const bool bad = (c == QLatin1Char('\\') || c == QLatin1Char('/') || c == QLatin1Char(':') ||
                          c == QLatin1Char('*') || c == QLatin1Char('?') || c == QLatin1Char('"') ||
                          c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('|') ||
                          c.isSpace() || c.category() == QChar::Other_Control);
        if (bad) s[i] = QLatin1Char('_');
    }
    return s;
}

} // namespace

// ===================== ① 机械图拆分 =====================
QVector<SplitGroup> groupByLayer(const std::vector<Element>& elems) {
    QVector<SplitGroup> groups;
    QHash<QString, int> index;
    for (const auto& e : elems) {
        QString layer = e.layer;
        if (layer.isEmpty()) layer = QStringLiteral("<无图层>");
        auto it = index.find(layer);
        if (it == index.end()) {
            index.insert(layer, groups.size());
            SplitGroup g;
            g.name = layer;
            g.elemIds.push_back((qint64)e.id);
            groups.push_back(g);
        } else {
            groups[it.value()].elemIds.push_back((qint64)e.id);
        }
    }
    return groups;
}

QVector<SplitGroup> groupByFrame(const std::vector<Element>& elems,
                                 const QVector<QRectF>& frames,
                                 const QString& outsideName) {
    QVector<SplitGroup> groups;
    QHash<int, int> index;   // 帧下标 → groups 下标（-1 = 框外）
    auto groupFor = [&](int key) -> int {
        auto it = index.find(key);
        if (it != index.end()) return it.value();
        SplitGroup g;
        g.name = (key < 0) ? outsideName : QStringLiteral("帧-%1").arg(key + 1);
        const int gi = groups.size();
        groups.push_back(g);
        index.insert(key, gi);
        return gi;
    };
    for (const auto& e : elems) {
        const QRectF bb = geo::elementBounds(e);
        const QPointF c = bb.center();
        int hit = -1;
        for (int i = 0; i < frames.size(); ++i) {
            if (frames[i].contains(c)) { hit = i; break; }   // 首个包含者优先（frames 保序即优先级）
        }
        groups[groupFor(hit)].elemIds.push_back((qint64)e.id);
    }
    return groups;
}

SplitResult writeSplitDrawings(const QJsonObject& sourceRoot,
                               const QVector<SplitGroup>& groups,
                               const std::vector<Element>& elems,
                               const QString& outDir,
                               const QString& baseName,
                               const QString& ext) {
    SplitResult res;
    res.outPaths.resize(groups.size());

    QHash<qint64, const Element*> byId;
    for (const auto& e : elems) byId.insert((qint64)e.id, &e);

    QDir dir(outDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        res.errors << QStringLiteral("无法创建输出目录：%1").arg(outDir);
        return res;
    }
    const QString base = sanitizeName(baseName.isEmpty() ? QStringLiteral("split") : baseName);

    for (int gi = 0; gi < groups.size(); ++gi) {
        const SplitGroup& g = groups[gi];
        if (g.elemIds.empty()) {
            res.errors << QStringLiteral("组「%1」为空组，未写出").arg(g.name);
            continue;
        }
        QJsonArray arr;
        bool ok = true;
        for (qint64 id : g.elemIds) {
            auto it = byId.find(id);
            if (it == byId.end()) {
                res.errors << QStringLiteral("组「%1」：图元 #%2 不在源图元集中，已跳过该组").arg(g.name).arg(id);
                ok = false;
                break;
            }
            arr.append(elementToJson(*it.value()));
        }
        if (!ok) continue;

        // 存档格式零改动（FORBID）：拷贝源根，仅替换 "elements"；其余顶层键原样保留。
        // 与 SketchModel::toDrawingBytes() 同一顶层结构 → 子档可独立打开。
        QJsonObject root = sourceRoot;
        root[QStringLiteral("elements")] = arr;
        const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

        const QString path = dir.filePath(QStringLiteral("%1-%2%3").arg(base, sanitizeName(g.name), ext));
        QSaveFile f(path);
        if (!f.open(QSaveFile::WriteOnly)) {
            res.errors << QStringLiteral("组「%1」打开目标失败：%2").arg(g.name, f.errorString());
            continue;
        }
        f.write(bytes);
        if (!f.commit()) {
            res.errors << QStringLiteral("组「%1」写入失败：%2").arg(g.name, f.errorString());
            continue;
        }
        res.outPaths[gi] = path;
        ++res.written;
    }
    return res;
}

// ===================== ② 自动排图（shelf 装箱）=====================
PackResult packShelf(const QVector<PackRect>& rects, const PackOptions& opt) {
    PackResult r;
    if (!(opt.sheetW > 0) || !(opt.sheetH > 0) || !std::isfinite(opt.sheetW) || !std::isfinite(opt.sheetH)) {
        r.error = QStringLiteral("纸张可用区域尺寸非法（须为正有限值）");
        return r;
    }
    if (opt.gap < 0 || !std::isfinite(opt.gap)) {
        r.error = QStringLiteral("图框间距不能为负");
        return r;
    }
    if (rects.isEmpty()) {
        r.error = QStringLiteral("待排图框列表为空");
        return r;
    }
    for (const auto& p : rects) {
        if (!(p.w > 0) || !(p.h > 0) || !std::isfinite(p.w) || !std::isfinite(p.h)) {
            r.error = QStringLiteral("图框 #%1 尺寸非法（须为正有限值）").arg(p.id);
            return r;
        }
        r.areaSum += p.w * p.h;
    }

    // 确定性排序：高降 → 宽降 → id 升
    QVector<int> order(rects.size());
    for (int i = 0; i < rects.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&rects](int a, int b) {
        if (rects[a].h != rects[b].h) return rects[a].h > rects[b].h;
        if (rects[a].w != rects[b].w) return rects[a].w > rects[b].w;
        return rects[a].id < rects[b].id;
    });

    r.placements.resize(rects.size());
    double curY = 0.0, shelfH = 0.0, remainW = opt.sheetW;
    int placed = 0;
    for (int oi : order) {
        const PackRect& p = rects[oi];
        if (p.w > opt.sheetW + 1e-9 || p.h > opt.sheetH + 1e-9) {
            r.error = QStringLiteral("图框 #%1（%2×%3）超出纸张可用区域 %4×%5")
                        .arg(p.id).arg(p.w).arg(p.h).arg(opt.sheetW).arg(opt.sheetH);
            break;
        }
        if (p.w > remainW + 1e-9) {           // 当前架放不下 → 新起一架
            curY += shelfH + opt.gap;
            shelfH  = 0.0;
            remainW = opt.sheetW;
        }
        if (curY + p.h > opt.sheetH + 1e-9) {
            r.error = QStringLiteral("纸张放不下全部图框（排到第 %1 个时高度超限）").arg(placed + 1);
            break;
        }
        const double x = opt.sheetW - remainW;
        PackPlacement& pl = r.placements[oi];
        pl.id = p.id; pl.x = x; pl.y = curY; pl.w = p.w; pl.h = p.h; pl.rotated = false;
        remainW -= p.w + opt.gap;
        if (p.h > shelfH) shelfH = p.h;
        r.usedW = std::max(r.usedW, x + p.w);
        r.usedH = std::max(r.usedH, curY + p.h);
        ++placed;
    }

    if (r.error.isEmpty()) {
        if (placed == rects.size()) {
            r.ok = true;
            if (r.usedW > 0 && r.usedH > 0) r.utilization = r.areaSum / (r.usedW * r.usedH);
        } else {
            r.error = QStringLiteral("部分图框未能排入");
        }
    }
    if (r.utilization > 1.0) r.utilization = 1.0;   // 浮点安全
    return r;
}

// ===================== ③ 批量签名 =====================
QVector<QPair<QString, qint64>> titleBlockFieldElems(const std::vector<Element>& elems) {
    QVector<QPair<QString, qint64>> out;
    for (const auto& e : elems) {
        if (e.type != ElementType::Text) continue;
        if (!e.name.startsWith(QStringLiteral("tb:field:"))) continue;
        out.push_back(qMakePair(e.name.mid(9), (qint64)e.id));
    }
    return out;
}

SignResult batchSign(const SketchModel& m, const QHash<QString, QString>& fields) {
    SignResult res;
    if (fields.isEmpty()) return res;

    // key/label 双通道检索：label 经 titleBlockFields() 单一数据源映射为 key
    QHash<QString, QString> byKey = fields;
    for (const auto& f : titleBlockFields()) {
        auto it = fields.find(f.label);
        if (it != fields.end()) byKey.insert(f.key, it.value());
    }

    QSet<QString> hitKeys;
    for (const auto& e : m.elements()) {
        if (e.type != ElementType::Text) continue;
        if (!e.name.startsWith(QStringLiteral("tb:field:"))) continue;
        const QString key = e.name.mid(9);
        auto it = byKey.find(key);
        if (it == byKey.end()) continue;
        if (it.value().isEmpty()) continue;    // 空值跳过：不擦除已有签名

        SignUpdate u;
        u.elemId   = (qint64)e.id;
        u.fieldKey = key;
        u.newElem  = e;
        u.newElem.text = it.value();

        // 单元格适配：复用 TitleBlockLayout 单一入口；网格无效 → 保留原锚点（绝不猜）
        const QRectF cell = titleBlockCellAt(&m, e.pts[0].x, e.pts[0].y);
        if (cell.isValid()) {
            const CellTextFit fit = fitCellText(it.value(), e.radius,
                                                cell.left(), cell.right(),
                                                cell.bottom(), cell.top(),
                                                /*stickTop=*/false);
            u.newElem.pts[0] = Vec3(fit.x, fit.y, 0);
            u.newElem.radius = fit.height;
            const Vec3& n0 = u.newElem.pts[0];
            const Vec3& o0 = e.pts[0];
            const bool moved = std::fabs(n0.x - o0.x) > 1e-9 || std::fabs(n0.y - o0.y) > 1e-9;
            u.refit = fit.shrunk || moved || std::fabs(u.newElem.radius - e.radius) > 1e-9;
        }
        res.updates.push_back(u);
        hitKeys.insert(key);
    }

    // 未命中回填：签名表中没有任何对应字段的 key/label（防静默漏签）
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        bool hit = hitKeys.contains(it.key());
        if (!hit) {
            for (const auto& f : titleBlockFields())
                if (f.label == it.key() && hitKeys.contains(f.key)) { hit = true; break; }
        }
        if (!hit) res.unmatched << it.key();
    }
    return res;
}

} // namespace SketchCAD
