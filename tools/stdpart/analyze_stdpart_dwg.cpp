// StdPartDwgAnalyzer —— GB 标准件 DWG 图库批量解析器
// ---------------------------------------------------------------------------
// 用途：扫描 CAD标准件图库（约 192 类 / 5078 个 DWG），逐个走 SketchCAD 的
//       cadfmt::fromDwg 读档链（与 GUI / DwgOpenVerify 完全同源），统计图元结构、
//       图层组织、标注信息与几何包围盒，并从目录名/文件名推断 GB 标准号与规格，
//       输出结构化 JSON 索引，供后续建立参数化标准件库与推理文档使用。
//
// 用法：StdPartDwgAnalyzer <图库根目录> <输出.json> [最大文件数|0=全部] [分片号] [分片总数]
// 例：  StdPartDwgAnalyzer "D:/.../CAD标准件图库" build/stdpart_index.json 0 0 8
//   分片：按文件序号 i % shardCount == shardIndex 过滤，便于多进程并行扫描后合并。
//
// 说明：纯数据解析，不创建窗口；DWG 高版本经 DwgBridge→dwg2dxf（含 STDPART2D
//       补丁）转 DXF 后回读。环境变量 SKETCHCAD_DWGTOOLS 可指定转换器目录。
#include "core/CadFormats.h"
#include "core/Element.h"
#include "core/Units.h"
#include "core/Geometry.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

using namespace SketchCAD;

namespace {

const char* elemTypeName(ElementType t) {
    switch (t) {
        case ElementType::Line:      return "Line";
        case ElementType::Circle:    return "Circle";
        case ElementType::Arc:       return "Arc";
        case ElementType::Ellipse:   return "Ellipse";
        case ElementType::Rectangle: return "Rectangle";
        case ElementType::Polygon:   return "Polygon";
        case ElementType::Polyline:  return "Polyline";
        case ElementType::Spline:    return "Spline";
        case ElementType::Text:      return "Text";
        case ElementType::MText:     return "MText";
        case ElementType::Dimension: return "Dimension";
        case ElementType::Hatch:     return "Hatch";
        case ElementType::BlockRef:  return "BlockRef";
        case ElementType::Point:     return "Point";
        default:                     return "Other";
    }
}

const char* dimTypeName(DimensionType t) {
    switch (t) {
        case DimensionType::Distance:  return "Distance";
        case DimensionType::Angle:     return "Angle";
        case DimensionType::Radius:    return "Radius";
        case DimensionType::Diameter:  return "Diameter";
        case DimensionType::Aligned:   return "Aligned";
        case DimensionType::Horizontal:return "Horizontal";
        case DimensionType::Vertical:  return "Vertical";
        case DimensionType::Baseline:  return "Baseline";
        case DimensionType::Continue:  return "Continue";
        case DimensionType::Arc:       return "Arc";
        case DimensionType::Leader:    return "Leader";
        case DimensionType::Ordinate:  return "Ordinate";
        default:                       return "Other";
    }
}

// 从目录名提取 GB 标准号，如 "六角头螺栓 - GB 5782-86" → {name, "GB 5782-86"}
QJsonObject parseCategory(const QString& dirName) {
    QJsonObject o;
    o["dir"] = dirName;
    QRegularExpression re(R"((GB[/\\ -]*T?)\s*([0-9]+(?:\.[0-9]+)?)\s*[-—–]\s*(\d{2,4}))",
                          QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(dirName);
    if (m.hasMatch()) {
        o["standard"] = QStringLiteral("%1 %2-%3").arg(m.captured(1).trimmed(), m.captured(2), m.captured(3));
        o["standardNumber"] = m.captured(2);
        o["standardYear"] = m.captured(3);
    } else {
        QRegularExpression re2(R"((GB[/\\ -]*T?)\s*([0-9]+(?:\.[0-9]+)?))",
                               QRegularExpression::CaseInsensitiveOption);
        auto m2 = re2.match(dirName);
        if (m2.hasMatch()) {
            o["standard"] = QStringLiteral("%1 %2").arg(m2.captured(1).trimmed(), m2.captured(2));
            o["standardNumber"] = m2.captured(2);
        }
    }
    // 中文品名：取 " - GB" 前的部分，清理空白
    QString kind = dirName;
    int p = kind.indexOf(QStringLiteral(" - GB"));
    if (p > 0) kind = kind.left(p);
    kind = kind.trimmed();
    o["kind"] = kind;
    return o;
}

// 从文件名提取规格 token，如 "M10x100.dwg" → {nominal:"M10", length:"100"}
// "10.dwg"（垫圈）→ {nominal:"10"}；"0.6 x 2.dwg"（销）→ {diameter:"0.6", length:"2"}
QJsonObject parseSpec(const QString& stem) {
    QJsonObject o;
    QString s = stem.trimmed();
    QRegularExpression reM(R"(M\s*([0-9]+(?:\.[0-9]+)?)\s*(?:x|×)\s*([0-9]+(?:\.[0-9]+)?))",
                           QRegularExpression::CaseInsensitiveOption);
    auto m = reM.match(s);
    if (m.hasMatch()) {
        o["nominal"] = "M" + m.captured(1);
        o["nominalMm"] = m.captured(1).toDouble();
        o["lengthMm"] = m.captured(2).toDouble();
        o["pattern"] = "M(d)x(L)";
        return o;
    }
    QRegularExpression reMOnly(R"(M\s*([0-9]+(?:\.[0-9]+)?))", QRegularExpression::CaseInsensitiveOption);
    auto m2 = reMOnly.match(s);
    if (m2.hasMatch()) {
        o["nominal"] = "M" + m2.captured(1);
        o["nominalMm"] = m2.captured(1).toDouble();
        o["pattern"] = "M(d)";
        return o;
    }
    QRegularExpression reDX(R"(([0-9]+(?:\.[0-9]+)?)\s*(?:x|×)\s*([0-9]+(?:\.[0-9]+)?))",
                            QRegularExpression::CaseInsensitiveOption);
    auto m3 = reDX.match(s);
    if (m3.hasMatch()) {
        o["diameterMm"] = m3.captured(1).toDouble();
        o["lengthMm"] = m3.captured(2).toDouble();
        o["pattern"] = "d x L";
        return o;
    }
    bool ok = false;
    double v = s.toDouble(&ok);
    if (ok) {
        o["nominalMm"] = v;
        o["pattern"] = "d";
    } else {
        o["pattern"] = "raw";
        o["raw"] = s;
    }
    return o;
}

struct BBox { double minx=1e18, miny=1e18, maxx=-1e18, maxy=-1e18; int n=0; };
void accumulate(BBox& b, const Element& e) {
    if (e.pts.empty()) return;
    for (const Vec3& p : e.pts) {
        b.minx = std::min(b.minx, p.x); b.maxx = std::max(b.maxx, p.x);
        b.miny = std::min(b.miny, p.y); b.maxy = std::max(b.maxy, p.y);
        ++b.n;
    }
    if (e.type == ElementType::Circle || e.type == ElementType::Arc) {
        const Vec3& c = e.pts[0];
        b.minx = std::min(b.minx, c.x - e.radius); b.maxx = std::max(b.maxx, c.x + e.radius);
        b.miny = std::min(b.miny, c.y - e.radius); b.maxy = std::max(b.maxy, c.y + e.radius);
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    // 批量扫描时关闭 DWG 桥/CadFormats 的逐文件 qWarning 诊断（写盘 I/O 是主要耗时）
    QLoggingCategory::setFilterRules(QStringLiteral(
        "*.warning=false\nqt.core.qprocess.warning=false"));
    if (argc < 3) {
        std::printf("用法: StdPartDwgAnalyzer <图库根目录> <输出.json> [最大文件数|0=全部]\n");
        return 2;
    }
    const QString root = QString::fromLocal8Bit(argv[1]);
    const QString outPath = QString::fromLocal8Bit(argv[2]);
    const int maxFiles = (argc > 3) ? QString::fromLocal8Bit(argv[3]).toInt() : 0;
    const int shardIndex = (argc > 4) ? QString::fromLocal8Bit(argv[4]).toInt() : 0;
    const int shardCount = (argc > 5) ? QString::fromLocal8Bit(argv[5]).toInt() : 1;

    QStringList allFiles;
    QDirIterator it(root, QStringList{QStringLiteral("*.dwg")},
                    QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) allFiles << it.next();
    std::sort(allFiles.begin(), allFiles.end());

    QStringList files;
    for (int i = 0; i < allFiles.size(); ++i) {
        if (shardCount > 1 && (i % shardCount) != shardIndex) continue;
        if (maxFiles > 0 && files.size() >= maxFiles) break;
        files << allFiles[i];
    }

    QJsonArray arr;
    QJsonObject summary;
    summary["root"] = root;
    summary["totalFiles"] = files.size();
    int okCount = 0, failCount = 0;

    QMap<QString, QJsonObject> categories;
    std::map<QString, int> typeTotals, layerTotals;

    for (int i = 0; i < files.size(); ++i) {
        const QString& path = files[i];
        QFileInfo fi(path);
        const QString dirName = fi.dir().dirName();
        if (!categories.contains(dirName))
            categories.insert(dirName, parseCategory(dirName));

        QFile f(path);
        QJsonObject rec;
        rec["file"] = fi.fileName();
        rec["relativePath"] = QDir(root).relativeFilePath(path);
        rec["sizeBytes"] = (qint64)fi.size();
        rec["category"] = dirName;
        rec["spec"] = parseSpec(fi.completeBaseName());

        bool ok = false;
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray raw = f.readAll();
            std::vector<Element> elems;
            QString err;
            cadfmt::DxfImportStats st;
            ok = cadfmt::fromDwg(raw, elems, &err, &st);
            f.close();
            if (ok) {
                QJsonObject counts;
                BBox bbTop, bbPart;
                QJsonArray dims, texts;
                QSet<QString> layers;
                auto noteElem = [&](const Element& e, bool inBlock) {
                    const QString tn = QString::fromUtf8(elemTypeName(e.type));
                    counts[tn] = counts.value(tn).toInt() + 1;
                    ++typeTotals[tn];
                    if (!e.layer.isEmpty()) { layers.insert(e.layer); ++layerTotals[e.layer]; }
                    accumulate(bbTop, e);
                    if (inBlock) accumulate(bbPart, e);
                    if (e.type == ElementType::Dimension) {
                        QJsonObject d;
                        d["type"] = QString::fromUtf8(dimTypeName(e.dtype));
                        d["text"] = e.text;
                        d["value"] = e.dimValue;
                        d["measured"] = geo::measureDimension(e);
                        d["layer"] = e.layer;
                        d["inBlock"] = inBlock;
                        dims.append(d);
                    }
                    if ((e.type == ElementType::Text || e.type == ElementType::MText) &&
                        !e.text.trimmed().isEmpty()) {
                        QJsonObject t;
                        t["text"] = e.text.trimmed();
                        t["h"] = e.radius;
                        t["layer"] = e.layer;
                        t["inBlock"] = inBlock;
                        texts.append(t);
                    }
                };
                for (const Element& e : elems) noteElem(e, false);
                QJsonArray blocksArr;
                for (const BlockDef& bd : st.blocks) {
                    QJsonObject bo;
                    bo["name"] = bd.name;
                    bo["basePoint"] = QJsonArray{bd.basePoint.x, bd.basePoint.y};
                    bo["entityCount"] = (int)bd.entities.size();
                    bo["attdefCount"] = (int)bd.attdefs.size();
                    bo["dynParamCount"] = (int)bd.dynParams.size();
                    QJsonObject bc;
                    for (const Element& be : bd.entities) {
                        noteElem(be, true);
                        const QString bt = QString::fromUtf8(elemTypeName(be.type));
                        bc[bt] = bc.value(bt).toInt() + 1;
                    }
                    bo["elementCounts"] = bc;
                    QJsonArray dyn;
                    for (const auto& p : bd.dynParams) {
                        QJsonObject po;
                        po["name"] = p.name;
                        po["defaultValue"] = p.defaultValue;
                        po["minValue"] = p.minValue;
                        po["maxValue"] = p.maxValue;
                        po["type"] = (int)p.type;
                        dyn.append(po);
                    }
                    bo["dynParams"] = dyn;
                    QJsonArray attrs;
                    for (const auto& a : bd.attdefs) {
                        QJsonObject ao;
                        ao["tag"] = a.tag;
                        ao["prompt"] = a.prompt;
                        attrs.append(ao);
                    }
                    bo["attdefs"] = attrs;
                    blocksArr.append(bo);
                }
                QJsonArray layerArr;
                QStringList ls = layers.values(); ls.sort();
                for (const QString& l : ls) layerArr.append(l);
                rec["elementCounts"] = counts;
                rec["elementTotal"] = (int)elems.size();
                rec["blockEntityTotal"] = blocksArr.size();
                rec["layers"] = layerArr;
                rec["layerTableCount"] = (int)st.layers.size();
                rec["blockCount"] = (int)st.blocks.size();
                rec["blocks"] = blocksArr;
                rec["textStyleCount"] = (int)st.styles.size();
                rec["insunits"] = st.insunits;
                rec["dimensions"] = dims;
                rec["dimensionCount"] = dims.size();
                rec["texts"] = texts;
                rec["textCount"] = texts.size();
                auto bboxJson = [](const BBox& b) {
                    QJsonObject o;
                    if (b.n > 0) {
                        o["minX"] = b.minx; o["minY"] = b.miny;
                        o["maxX"] = b.maxx; o["maxY"] = b.maxy;
                        o["width"] = b.maxx - b.minx;
                        o["height"] = b.maxy - b.miny;
                    }
                    return o;
                };
                rec["bboxAllMm"] = bboxJson(bbTop);
                rec["bboxPartMm"] = bboxJson(bbPart);
            } else {
                rec["error"] = err;
            }
        } else {
            rec["error"] = QStringLiteral("无法打开文件");
        }
        rec["ok"] = ok;
        ok ? ++okCount : ++failCount;
        arr.append(rec);
        if ((i + 1) % 200 == 0)
            std::printf("[进度] %d/%lld  成功=%d 失败=%d\n", i + 1,
                        (long long)files.size(), okCount, failCount);
    }

    QJsonObject typeObj;
    for (const auto& kv : typeTotals) typeObj[kv.first] = kv.second;
    QJsonObject layerObj;
    for (const auto& kv : layerTotals) layerObj[kv.first] = kv.second;
    QJsonArray catArr;
    for (auto it2 = categories.constBegin(); it2 != categories.constEnd(); ++it2) {
        QJsonObject c = it2.value();
        int cnt = 0, dimCnt = 0;
        for (const auto& v : arr) {
            QJsonObject r = v.toObject();
            if (r.value("category").toString() == it2.key()) {
                ++cnt; dimCnt += r.value("dimensionCount").toInt();
            }
        }
        c["fileCount"] = cnt; c["dimensionTotal"] = dimCnt;
        catArr.append(c);
    }

    summary["parsedOk"] = okCount;
    summary["parsedFail"] = failCount;
    summary["categoryCount"] = categories.size();
    summary["elementTypeTotals"] = typeObj;
    summary["layerTotals"] = layerObj;

    QJsonObject doc;
    doc["summary"] = summary;
    doc["categories"] = catArr;
    doc["files"] = arr;

    QFile of(outPath);
    if (!of.open(QIODevice::WriteOnly)) {
        std::printf("[FAIL] 无法写入 %s\n", qPrintable(outPath));
        return 1;
    }
    of.write(QJsonDocument(doc).toJson(QJsonDocument::Compact));
    of.close();
    std::printf("[OK] 输出=%s  文件=%lld 成功=%d 失败=%d 类别=%lld\n",
                qPrintable(outPath), (long long)files.size(), okCount, failCount,
                (long long)categories.size());
    return failCount > 0 && okCount == 0 ? 1 : 0;
}
