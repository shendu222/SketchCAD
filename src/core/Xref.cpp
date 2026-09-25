// 外部参照（Xref）：源文件读取与路径换算的纯算法实现。
//
// 设计要点：
//   * 本文件不触碰 SketchModel —— 全部输入来自参数、全部输出写回 out/返回值，
//     便于在离屏测试中直接驱动（也便于将来接入「重载参照」而不牵动模型状态）。
//   * 读取模块按扩展名分派，复用既有的三个转换器（JSON 本程序存档 / DXF / DWG）；
//     STEP 只有导出没有导入，明确拒绝并回错误串（与 MainWindow::readDrawing 同口径）。
#include "Xref.h"
#include "CadFormats.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace SketchCAD {
namespace xref {

bool readSource(const QString& absPath, XrefSource& out, QString* err) {
    QFile f(absPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取 ") + absPath;
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();

    const cadfmt::CadFormat fmt = cadfmt::formatFromPath(absPath);
    switch (fmt) {
        case cadfmt::CadFormat::Json: {
            QJsonParseError pe;
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
            if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                if (err) *err = QStringLiteral("JSON 解析失败：") + pe.errorString();
                return false;
            }
            const QJsonObject root = doc.object();
            for (const auto& v : root[QStringLiteral("elements")].toArray())
                out.elements.push_back(elementFromJson(v.toObject()));
            // 源图自带的块表：仅用于在宿主图中引入被引用的依赖（块名空则忽略）
            for (const auto& v : root[QStringLiteral("blocks")].toArray()) {
                BlockDef d = block::blockDefFromJson(v.toObject());
                if (!d.name.trimmed().isEmpty()) out.blocks.push_back(std::move(d));
            }
            out.formatName = QStringLiteral("草图 JSON");
            return true;
        }
        case cadfmt::CadFormat::Dxf: {
            QString e;
            cadfmt::DxfImportStats st;
            if (!cadfmt::fromDxf(raw, out.elements, &e, &st)) {
                if (err) *err = e;
                return false;
            }
            out.formatName = QStringLiteral("DXF");
            return true;
        }
        case cadfmt::CadFormat::Dwg: {
            QString e;
            cadfmt::DxfImportStats st;
            if (!cadfmt::fromDwg(raw, out.elements, &e, &st)) {
                if (err) *err = e;
                return false;
            }
            out.formatName = QStringLiteral("DWG");
            return true;
        }
        case cadfmt::CadFormat::Step:
            if (err) *err = QStringLiteral("STEP 为只写格式，不能作为外部参照读入");
            return false;
        case cadfmt::CadFormat::Unknown:
        default:
            if (err) *err = QStringLiteral("不支持的外部参照文件格式：") + absPath;
            return false;
    }
}

QString blockNameForPath(const QString& path) {
    const QString base = QFileInfo(path).completeBaseName().trimmed();
    // 极端兜底：空文件名（如 ".json"）不至于产生空块名
    return base.isEmpty() ? QStringLiteral("Xref") : base;
}

QString storedPath(const QString& absPath, XrefPathType t, const QString& hostDir) {
    switch (t) {
        case XrefPathType::NoPath:
            return QFileInfo(absPath).fileName();
        case XrefPathType::RelativePath: {
            if (hostDir.isEmpty()) return QFileInfo(absPath).fileName();
            const QString rel = QDir(hostDir).relativeFilePath(absPath);
            return rel.isEmpty() ? absPath : rel;
        }
        case XrefPathType::FullPath:
        default:
            return absPath;
    }
}

QString resolvedPath(const QString& stored, XrefPathType t, const QString& hostDir) {
    if (stored.isEmpty()) return stored;
    const QFileInfo fi(stored);
    if (fi.isAbsolute()) return fi.absoluteFilePath();
    if (t == XrefPathType::FullPath || hostDir.isEmpty()) return stored;
    return QFileInfo(QDir(hostDir), stored).absoluteFilePath();
}

std::vector<QString> missingDependencies(const std::vector<Element>& src,
                                         const std::vector<BlockDef>& table) {
    std::vector<QString> out;
    auto have = [&](const QString& n) {
        for (const auto& d : table) if (d.name == n) return true;
        return false;
    };
    for (const auto& e : src) {
        if (e.type != ElementType::BlockRef) continue;
        const QString& n = e.blockName;
        if (n.trimmed().isEmpty() || have(n)) continue;
        if (std::find(out.begin(), out.end(), n) != out.end()) continue;
        out.push_back(n);
    }
    return out;
}

} // namespace xref
} // namespace SketchCAD
