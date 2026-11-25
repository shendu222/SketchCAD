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

    // 2) 几何缩放：轴向(L)→x，径向(d)→y；圆/弧半径按 d 比例
    const double dNew = params.value(QStringLiteral("NOMINAL_D"), meta.baselineNominal).toDouble();
    const double LNew = params.value(QStringLiteral("LENGTH_L"), meta.baselineLength).toDouble();
    const double sx = (meta.baselineLength > 1e-9 && LNew > 1e-9) ? LNew / meta.baselineLength : 1.0;
    const double sy = (meta.baselineNominal > 1e-9 && dNew > 1e-9) ? dNew / meta.baselineNominal : 1.0;
    // 单一基准（无 L）时 d 同时驱动两轴，避免只横向拉伸
    const bool uni = (meta.baselineLength <= 1e-9);
    const double ux = uni ? sy : sx;

    std::vector<Element> out;
    out.reserve(src.size() + 1);
    for (Element e : src) {
        // 文字占位符替换
        if (!e.text.isEmpty()) e.text = replaceInText(e.text, values);
        // 尺寸标注数值随参数更新（实测值按缩放比换算，文字覆写保持同步）
        if (e.type == ElementType::Dimension && e.dimValue > 1e-9) {
            const double k = (std::fabs(ux - 1.0) > std::fabs(sy - 1.0)) ? ux : sy;
            e.dimValue *= k;
        }
        // 点坐标缩放
        for (Vec3& p : e.pts) { p.x *= ux; p.y *= sy; }
        if (e.type == ElementType::Circle || e.type == ElementType::Arc)
            e.radius *= sy;
        out.push_back(std::move(e));
    }

    // 3) 追加规格标注（含 {{SPEC_TEXT}} → 已替换），置于图形上方
    if (!out.empty()) {
        double minX = 1e18, maxX = -1e18, minY = 1e18;
        for (const Element& e : out) {
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
