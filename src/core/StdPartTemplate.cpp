#include "StdPartTemplate.h"
#include "CadFormats.h"
#include "SheetSpec.h"   // standardTextHeightMm: 规格标注文字高度与国标一致
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <limits>

namespace SketchCAD {

namespace StdPartPlaceholders {
const QStringList& keys() {
    static const QStringList k = {
        QStringLiteral("NOMINAL_D"), QStringLiteral("LENGTH_L"),
        QStringLiteral("PITCH_P"), QStringLiteral("ACROSS_FLATS_S"),
        QStringLiteral("THICKNESS_T"), QStringLiteral("SPEC_TEXT")
    };
    return k;
}
QString bracketed(const QString& key) {
    return QStringLiteral("{{%1}}").arg(key);
}
} // namespace StdPartPlaceholders

namespace {

// DXF 模板压缩：qCompress 直接压缩 QByteArray，格式自描述（构建器用 qCompress 写，此处解压）
QByteArray decompressDxf(const QByteArray& packed) { return qUncompress(packed); }

// 从文件名提取规格基准值（与 analyze_stdpart_dwg.cpp 的 parseSpec 同规则）
void parseBaseline(const QString& stem, StdPartTemplateMeta& m) {
    QString s = stem.trimmed();
    QRegularExpression reM(QStringLiteral("M\\s*([0-9]+(?:\\.[0-9]+)?)\\s*(?:x|×)\\s*([0-9]+(?:\\.[0-9]+)?)"),
                           QRegularExpression::CaseInsensitiveOption);
    auto a = reM.match(s);
    if (a.hasMatch()) {
        m.baselineNominal = a.captured(1).toDouble();
        m.baselineLength = a.captured(2).toDouble();
        m.specText = QStringLiteral("M%1x%2").arg(a.captured(1), a.captured(2));
        return;
    }
    QRegularExpression reMOnly(QStringLiteral("M\\s*([0-9]+(?:\\.[0-9]+)?)"),
                               QRegularExpression::CaseInsensitiveOption);
    auto b = reMOnly.match(s);
    if (b.hasMatch()) {
        m.baselineNominal = b.captured(1).toDouble();
        m.specText = QStringLiteral("M%1").arg(b.captured(1));
        return;
    }
    QRegularExpression reDX(QStringLiteral("([0-9]+(?:\\.[0-9]+)?)\\s*(?:x|×)\\s*([0-9]+(?:\\.[0-9]+)?)"));
    auto c = reDX.match(s);
    if (c.hasMatch()) {
        m.baselineNominal = c.captured(1).toDouble();
        m.baselineLength = c.captured(2).toDouble();
        m.specText = QStringLiteral("%1x%2").arg(c.captured(1), c.captured(2));
        return;
    }
    bool ok = false;
    double v = s.toDouble(&ok);
    if (ok) { m.baselineNominal = v; m.specText = s; }
    else m.specText = s;
}

QString fmtNum(double v) {
    if (std::fabs(v - std::round(v)) < 1e-9) return QString::number((int)std::round(v));
    return QString::number(v, 'f', 2);
}

// 猜测 GB 标准号（与解析器同规则）
QString parseStandard(const QString& dir) {
    QRegularExpression re(QStringLiteral("(GB[/\\ -]*T?)\\s*([0-9]+(?:\\.[0-9]+)?)\\s*[-—–]?\\s*(\\d{2,4})?"),
                          QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(dir);
    if (m.hasMatch()) {
        QString r = m.captured(1).trimmed() + QStringLiteral(" ") + m.captured(2);
        if (!m.captured(3).isEmpty()) r += QStringLiteral("-") + m.captured(3);
        return r;
    }
    return QString();
}

// ---- 长度参数驱动的分区识别 ------------------------------------------------
// 原图模板把「主视 / 侧视 / 端视」画在同一张图上。修改 L 时只能改变
// 「头下 → 螺纹起点」这段光杆长度，头部、螺纹段与端视图必须保持原尺寸。
// 下面从模板几何自动识别这四个边界，避免对某一规格硬编码。
struct LengthAnchors {
    bool valid = false;
    double headEnd = 0.0;       // 头部结束 / 光杆起点
    double threadStart = 0.0;   // 螺纹起点
    double tip = 0.0;           // 杆端（基准总长终点）
    double endViewStart = std::numeric_limits<double>::infinity(); // 端视图起始 x
};

bool isSheetFrameElement(const Element& e) {
    if (e.pts.empty()) return false;
    double minX = e.pts[0].x, maxX = e.pts[0].x;
    double minY = e.pts[0].y, maxY = e.pts[0].y;
    for (const Vec3& p : e.pts) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    // A3 图框：位于 0 层且跨越整幅图幅，不参与零件几何分析/缩放
    return e.layer == QStringLiteral("0") && (maxX - minX > 350.0 || maxY - minY > 200.0);
}

LengthAnchors detectLengthAnchors(const std::vector<Element>& src, double d) {
    LengthAnchors a;
    struct Seg { double x1, x2; };
    std::vector<Seg> segs;
    std::vector<double> xs;
    for (const Element& e : src) {
        if (isSheetFrameElement(e)) continue;
        for (const Vec3& p : e.pts) xs.push_back(p.x);
        if (e.type != ElementType::Line || e.lineStyle != LineStyle::Solid || e.pts.size() < 2)
            continue;
        const Vec3& p1 = e.pts[0];
        const Vec3& p2 = e.pts[1];
        const double x1 = std::min(p1.x, p2.x), x2 = std::max(p1.x, p2.x);
        const double y1 = p1.y, y2 = p2.y;
        const double len = x2 - x1;
        // 中心线（CENTER）已由线型过滤；只取足够长的水平实线作为光杆/螺纹边界证据
        if (std::fabs(y2 - y1) > std::max(0.2 * d, 0.2) || len < std::max(2.0 * d, 5.0)
            || len > 200.0)
            continue;
        segs.push_back({ x1, x2 });
    }
    if (segs.empty()) return a;

    a.headEnd = std::numeric_limits<double>::max();
    a.tip = -std::numeric_limits<double>::max();
    for (const Seg& s : segs) {
        a.headEnd = std::min(a.headEnd, s.x1);
        a.tip = std::max(a.tip, s.x2);
    }
    const double eps = std::max(1.0, 0.1 * d);
    a.threadStart = a.tip;
    for (const Seg& s : segs) {
        if (s.x1 > a.headEnd + eps && s.x1 < a.threadStart)
            a.threadStart = s.x1;
    }

    // 端视图与主/侧视之间有明显 x 间隙；该区域内的图元只随 d 缩放，不随 L 改动
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(),
                         [](double u, double v) { return std::fabs(u - v) < 0.25; }), xs.end());
    // 只考察杆端之后的间隙：光杆内部本来就可能有很大的点间距，不能据此误判端视图
    const double gapMin = std::max(3.0, 0.25 * d);
    double gap = std::numeric_limits<double>::infinity(), boundary = 0.0;
    for (size_t i = 1; i < xs.size(); ++i) {
        const double g = xs[i] - xs[i - 1];
        if (xs[i - 1] >= a.tip - eps && g > gapMin && g < gap) {
            gap = g;
            boundary = xs[i];
        }
    }
    if (boundary > 0.0) a.endViewStart = boundary;
    a.valid = a.tip - a.headEnd > 1e-6;
    return a;
}

} // namespace

StdPartTemplateStore& StdPartTemplateStore::instance() {
    static StdPartTemplateStore s;
    return s;
}

QString StdPartTemplateStore::defaultStoreDir() {
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        QDir(exeDir).filePath(QStringLiteral("data/stdpart/templates")),
        QDir(exeDir).filePath(QStringLiteral("../data/stdpart/templates")),
        QDir(exeDir).filePath(QStringLiteral("../../data/stdpart/templates")),
        QDir(exeDir).filePath(QStringLiteral("stdpart_tpl"))
    };
    for (const QString& c : cands) {
        if (QFile::exists(QDir(c).filePath(QStringLiteral("index.json")))) return c;
    }
    return QString();
}

bool StdPartTemplateStore::load(const QString& storeDir, QString* err) {
    const QString indexPath = QDir(storeDir).filePath(QStringLiteral("index.json"));
    QFile f(indexPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("模板库索引无法打开: %1").arg(indexPath);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonArray arr = doc.object().value(QStringLiteral("templates")).toArray();
    QVector<StdPartTemplateMeta> metas;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        StdPartTemplateMeta m;
        m.id = o.value(QStringLiteral("id")).toString();
        m.category = o.value(QStringLiteral("category")).toString();
        m.standard = o.value(QStringLiteral("standard")).toString();
        m.sourceFile = o.value(QStringLiteral("sourceFile")).toString();
        m.tplFile = o.value(QStringLiteral("tplFile")).toString();
        m.sourceBytes = (qint64)o.value(QStringLiteral("sourceBytes")).toDouble();
        m.elementCount = o.value(QStringLiteral("elementCount")).toInt();
        m.textCount = o.value(QStringLiteral("textCount")).toInt();
        m.placeholderCount = o.value(QStringLiteral("placeholderCount")).toInt();
        m.ok = o.value(QStringLiteral("ok")).toBool();
        m.failReason = o.value(QStringLiteral("failReason")).toString();
        m.baselineNominal = o.value(QStringLiteral("baselineNominal")).toDouble();
        m.baselineLength = o.value(QStringLiteral("baselineLength")).toDouble();
        m.baselinePitch = o.value(QStringLiteral("baselinePitch")).toDouble();
        m.baselineAcrossFlats = o.value(QStringLiteral("baselineAcrossFlats")).toDouble();
        m.baselineThickness = o.value(QStringLiteral("baselineThickness")).toDouble();
        m.specText = o.value(QStringLiteral("specText")).toString();
        for (const QJsonValue& l : o.value(QStringLiteral("layers")).toArray())
            m.layers << l.toString();
        if (!m.id.isEmpty()) metas.append(m);
    }
    if (metas.isEmpty()) {
        if (err) *err = QStringLiteral("模板库索引为空: %1").arg(indexPath);
        return false;
    }
    storeDir_ = storeDir;
    metas_ = metas;
    loaded_ = true;
    return true;
}

const StdPartTemplateMeta* StdPartTemplateStore::find(const QString& id) const {
    for (const StdPartTemplateMeta& m : metas_)
        if (m.id == id) return &m;
    return nullptr;
}

QVector<QString> StdPartTemplateStore::categories() const {
    QVector<QString> r;
    for (const StdPartTemplateMeta& m : metas_)
        if (!r.contains(m.category)) r.append(m.category);
    std::sort(r.begin(), r.end());
    return r;
}

QVector<const StdPartTemplateMeta*> StdPartTemplateStore::byCategory(const QString& cat) const {
    QVector<const StdPartTemplateMeta*> r;
    for (const StdPartTemplateMeta& m : metas_)
        if (m.category == cat) r.append(&m);
    return r;
}

bool StdPartTemplateStore::loadElements(const QString& id, std::vector<Element>& out,
                                        const StdPartTemplateMeta** metaOut,
                                        QString* err) const {
    const StdPartTemplateMeta* m = find(id);
    if (!m) {
        if (err) *err = QStringLiteral("未找到模板: %1").arg(id);
        return false;
    }
    if (!m->ok || m->tplFile.isEmpty()) {
        if (err) *err = m->failReason.isEmpty() ? QStringLiteral("模板未成功解析") : m->failReason;
        return false;
    }
    QFile f(QDir(storeDir_).filePath(m->tplFile));
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("模板文件无法打开: %1").arg(m->tplFile);
        return false;
    }
    const QByteArray dxf = decompressDxf(f.readAll());
    f.close();
    if (dxf.isEmpty()) {
        if (err) *err = QStringLiteral("模板内容为空: %1").arg(m->tplFile);
        return false;
    }
    QString perr;
    if (!cadfmt::fromDxf(dxf, out, &perr)) {
        if (err) *err = perr;
        return false;
    }
    if (metaOut) *metaOut = m;
    return true;
}

QString StdPartTemplateStore::replaceInText(const QString& text, const QVariantMap& values) {
    QString r = text;
    // 先替换组合键 SPEC_TEXT（其值可能含 {{}} 之外的字符），再替换单项键
    if (values.contains(QStringLiteral("SPEC_TEXT")))
        r.replace(StdPartPlaceholders::bracketed(QStringLiteral("SPEC_TEXT")),
                  values.value(QStringLiteral("SPEC_TEXT")).toString());
    for (const QString& k : StdPartPlaceholders::keys()) {
        if (k == QStringLiteral("SPEC_TEXT")) continue;
        if (!values.contains(k)) continue;
        const QString ph = StdPartPlaceholders::bracketed(k);
        if (r.contains(ph)) r.replace(ph, values.value(k).toString());
    }
    return r;
}

std::vector<Element> StdPartTemplateStore::render(const std::vector<Element>& src,
                                                  const StdPartTemplateMeta& meta,
                                                  const QVariantMap& params) {
    // 1) 数值替换表
    QVariantMap values;
    for (const QString& k : StdPartPlaceholders::keys()) {
        if (params.contains(k)) values[k] = params.value(k);
    }
    // SPEC_TEXT 缺省：由 d / L 自动拼装（保持与原文件名规格一致的格式）
    if (!values.contains(QStringLiteral("SPEC_TEXT"))) {
        const double d = params.value(QStringLiteral("NOMINAL_D"), meta.baselineNominal).toDouble();
        const double L = params.value(QStringLiteral("LENGTH_L"), meta.baselineLength).toDouble();
        if (meta.specText.startsWith(QStringLiteral("M"), Qt::CaseInsensitive)) {
            values[QStringLiteral("SPEC_TEXT")] = (L > 0)
                ? QStringLiteral("M%1x%2").arg(fmtNum(d), fmtNum(L))
                : QStringLiteral("M%1").arg(fmtNum(d));
        } else {
            values[QStringLiteral("SPEC_TEXT")] = (L > 0)
                ? QStringLiteral("%1x%2").arg(fmtNum(d), fmtNum(L))
                : fmtNum(d);
        }
    }

    // 2) 几何驱动：径向(d)→y；轴向(L)只改变「头下→螺纹起点」的光杆段。
    //    旧实现把所有点 x 乘以 L/L0，导致头部、螺纹段和端视图一起被压缩/拉伸——
    //    M10x100 改为 70 时头高、螺纹长度、端视六角全部变形（缺陷根因）。
    const double dNew = params.value(QStringLiteral("NOMINAL_D"), meta.baselineNominal).toDouble();
    const double LNew = params.value(QStringLiteral("LENGTH_L"), meta.baselineLength).toDouble();
    const bool hasLength = meta.baselineLength > 1e-9 && LNew > 1e-9;
    const double sx = hasLength ? LNew / meta.baselineLength : 1.0;
    const double sy = (meta.baselineNominal > 1e-9 && dNew > 1e-9) ? dNew / meta.baselineNominal : 1.0;
    // 单一基准（无 L）时 d 同时驱动两轴，避免只横向拉伸
    const bool uni = !hasLength;
    const double ux = uni ? sy : sx;

    const LengthAnchors an = hasLength
        ? detectLengthAnchors(src, meta.baselineNominal > 0 ? meta.baselineNominal : dNew)
        : LengthAnchors{};
    const bool segmented = hasLength && an.valid && meta.baselineLength > 1e-9;
    const double geoLength = segmented ? (an.tip - an.headEnd) : 0.0;
    const double deltaDraw = segmented
        ? (LNew - meta.baselineLength) * (geoLength / meta.baselineLength) : 0.0;
    double shankOld = segmented ? (an.threadStart - an.headEnd) : 0.0;
    if (shankOld <= 1e-9) shankOld = geoLength;   // 无螺纹段时整段光杆承担长度变化
    const double minShank = std::min(1.0, 0.05 * meta.baselineNominal);
    double shankNew = segmented ? (shankOld + deltaDraw) : 0.0;
    double delta = deltaDraw;
    if (segmented && shankNew < minShank) {
        shankNew = minShank;
        delta = shankNew - shankOld;   // 长度过短时保住最小光杆段，避免几何自交
    }
    const double shankRatio = (segmented && shankOld > 1e-9) ? shankNew / shankOld : 1.0;

    auto tx = [&](double x, bool frame) {
        if (frame) return x;
        if (!segmented) return x * ux;
        if (x >= an.endViewStart) return x;   // 端视图位置固定，不随 L 漂移
        if (x <= an.headEnd) return x;        // 头部尺寸固定
        if (x < an.threadStart)
            return an.headEnd + (x - an.headEnd) * shankRatio;  // 光杆段伸缩
        return x + delta;                     // 螺纹段整体平移，长度保持不变
    };

    std::vector<Element> out;
    out.reserve(src.size() + 1);
    for (Element e : src) {
        const bool frame = isSheetFrameElement(e);
        // 头部轮廓整体属于「不变区」：倒角弧的圆心 x 可能落在 headEnd 右侧，
        // 但弧本身仍属于头部。必须整件保留，不能逐点按 x 区间缩放，
        // 否则 L 一改圆心就漂移，浅倒角被拉成深凹弧且与顶/底边脱节。
        bool headPart = false;
        if (segmented && !frame) {
            double emin = std::numeric_limits<double>::max();
            double emax = -std::numeric_limits<double>::max();
            for (const Vec3& p : e.pts) {
                emin = std::min(emin, p.x); emax = std::max(emax, p.x);
            }
            if (e.type == ElementType::Arc && e.radius > 0.0 && !e.pts.empty()) {
                emin = std::min(emin, e.pts[0].x - e.radius);
                emax = std::max(emax, e.pts[0].x + e.radius);
            }
            const bool solidOutline = e.lineStyle == LineStyle::Solid;
            headPart = (e.type == ElementType::Arc)
                ? (emin < an.headEnd - 1e-6 && emin > an.headEnd - 6.0 * meta.baselineNominal)
                : (solidOutline && emin < an.headEnd - 1e-6
                   && emax <= an.headEnd + std::max(1.0, 0.1 * meta.baselineNominal));
        }
        // 文字占位符替换
        if (!e.text.isEmpty()) e.text = replaceInText(e.text, values);
        // 尺寸标注数值随参数更新（实测值按缩放比换算，文字覆写保持同步）
        if (e.type == ElementType::Dimension && e.dimValue > 1e-9) {
            const double k = segmented ? std::max(sy, shankRatio)
                                       : ((std::fabs(ux - 1.0) > std::fabs(sy - 1.0)) ? ux : sy);
            e.dimValue *= k;
        }
        for (Vec3& p : e.pts) {
            p.x = (frame || headPart) ? p.x : tx(p.x, frame);
            if (!frame) p.y *= sy;
        }
        if (!frame && (e.type == ElementType::Circle || e.type == ElementType::Arc))
            e.radius *= sy;
        out.push_back(std::move(e));
    }

    // 3) 追加规格标注（含 {{SPEC_TEXT}} → 已替换），置于图形上方
    if (!out.empty()) {
        double minX = 1e18, maxX = -1e18, minY = 1e18;
        for (const Element& e : out) {
            if (isSheetFrameElement(e)) continue;   // 图框不参与规格标注定位
            for (const Vec3& p : e.pts) {
                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
            }
        }
        if (minX < maxX) {
            Element label;
            label.type = ElementType::Text;
            label.pts = { Vec3((minX + maxX) / 2, minY - std::max(4.0, (maxX - minX) * 0.05), 0) };
            label.text = values.value(QStringLiteral("SPEC_TEXT")).toString()
                       + QStringLiteral("  ") + meta.standard;
            label.radius = standardTextHeightMm(SheetFormat::A3, true);
            label.textAlign = 1;
            label.layer = QStringLiteral("SPEC");
            label.name = QStringLiteral("规格标注");
            label.styleName = QStringLiteral("LongFangSong");
            out.push_back(std::move(label));
        }
    }
    return out;
}

bool StdPartTemplateStore::writeIndex(const QString& storeDir,
                                      const QVector<StdPartTemplateMeta>& metas,
                                      QString* err) {
    QJsonArray arr;
    for (const StdPartTemplateMeta& m : metas) {
        QJsonObject o;
        o[QStringLiteral("id")] = m.id;
        o[QStringLiteral("category")] = m.category;
        o[QStringLiteral("standard")] = m.standard;
        o[QStringLiteral("sourceFile")] = m.sourceFile;
        o[QStringLiteral("tplFile")] = m.tplFile;
        o[QStringLiteral("sourceBytes")] = m.sourceBytes;
        o[QStringLiteral("elementCount")] = m.elementCount;
        o[QStringLiteral("textCount")] = m.textCount;
        o[QStringLiteral("placeholderCount")] = m.placeholderCount;
        o[QStringLiteral("ok")] = m.ok;
        o[QStringLiteral("failReason")] = m.failReason;
        o[QStringLiteral("baselineNominal")] = m.baselineNominal;
        o[QStringLiteral("baselineLength")] = m.baselineLength;
        o[QStringLiteral("baselinePitch")] = m.baselinePitch;
        o[QStringLiteral("baselineAcrossFlats")] = m.baselineAcrossFlats;
        o[QStringLiteral("baselineThickness")] = m.baselineThickness;
        o[QStringLiteral("specText")] = m.specText;
        QJsonArray layers;
        for (const QString& l : m.layers) layers.append(l);
        o[QStringLiteral("layers")] = layers;
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 1;
    root[QStringLiteral("placeholderRule")] = QStringLiteral("{{KEY}} 双花括号全大写下划线");
    root[QStringLiteral("keys")] = QJsonArray::fromStringList(StdPartPlaceholders::keys());
    root[QStringLiteral("templates")] = arr;
    QFile f(QDir(storeDir).filePath(QStringLiteral("index.json")));
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("无法写入模板库索引");
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.close();
    return true;
}

// 供构建器调用：把 DWG 原始字节解析为占位符模板（返回 DXF 文本 + 元数据填充）
bool StdPartTemplateStore::buildFromDwg(const QByteArray& dwg, const QString& sourceRel,
                          const QString& categoryDir, QByteArray& tplDxfOut,
                          StdPartTemplateMeta& meta, QString* err) {
    const QFileInfo fi(sourceRel);
    meta.id = QString::fromUtf8(QCryptographicHash::hash(
        sourceRel.toUtf8(), QCryptographicHash::Md5).toHex().left(16));
    meta.category = categoryDir;
    meta.standard = parseStandard(categoryDir);
    meta.sourceFile = sourceRel;
    meta.sourceBytes = dwg.size();
    parseBaseline(fi.completeBaseName(), meta);

    // DWG → Element（与 GUI / DwgOpenVerify 完全同源的 fromDwg：R12 原生解析，
    // R13~R2018 经 DwgBridge→dwg2dxf 补丁版转 DXF 后回读），不对 R12 另起桥。
    std::vector<Element> els;
    QString pe;
    if (!cadfmt::fromDwg(dwg, els, &pe)) {
        meta.ok = false;
        meta.failReason = pe;
        if (err) *err = meta.failReason;
        return false;
    }
    if (els.empty()) {
        meta.ok = false;
        meta.failReason = QStringLiteral("ENTITIES 段无可用图元");
        if (err) *err = meta.failReason;
        return false;
    }

    // 在文字中标记占位符：把与基准值相同的数字串替换为 {{KEY}}
    auto mark = [&](QString& s) {
        if (meta.baselineNominal > 0) {
            const QString n = fmtNum(meta.baselineNominal);
            s.replace(n, StdPartPlaceholders::bracketed(QStringLiteral("NOMINAL_D")), Qt::CaseInsensitive);
        }
        if (meta.baselineLength > 0 && std::fabs(meta.baselineLength - meta.baselineNominal) > 1e-9) {
            const QString n = fmtNum(meta.baselineLength);
            s.replace(n, StdPartPlaceholders::bracketed(QStringLiteral("LENGTH_L")), Qt::CaseInsensitive);
        }
    };
    int ph = 0;
    QSet<QString> layers;
    for (Element& e : els) {
        if (!e.layer.isEmpty()) layers.insert(e.layer);
        if (e.type == ElementType::Text || e.type == ElementType::MText) {
            ++meta.textCount;
            const QString before = e.text;
            mark(e.text);
            if (e.text != before) ++ph;
        }
    }
    // 追加规格占位标注：{{SPEC_TEXT}}（生成时替换）
    Element label;
    label.type = ElementType::Text;
    label.text = StdPartPlaceholders::bracketed(QStringLiteral("SPEC_TEXT"));
    label.layer = QStringLiteral("SPEC");
    label.name = QStringLiteral("规格占位标注");
    label.styleName = QStringLiteral("LongFangSong");
    els.push_back(label);
    ++ph;

    QString de;
    const QByteArray tpl = cadfmt::toDxf(els, &de);
    if (tpl.isEmpty()) {
        meta.ok = false;
        meta.failReason = QStringLiteral("模板 DXF 导出失败: %1").arg(de);
        if (err) *err = meta.failReason;
        return false;
    }
    tplDxfOut = tpl;
    meta.elementCount = (int)els.size();
    meta.placeholderCount = ph;
    meta.layers = layers.values();
    std::sort(meta.layers.begin(), meta.layers.end());
    meta.ok = true;
    meta.failReason.clear();
    meta.tplFile = QStringLiteral("tpl/%1.tpl").arg(meta.id);
    return true;
}

} // namespace SketchCAD
