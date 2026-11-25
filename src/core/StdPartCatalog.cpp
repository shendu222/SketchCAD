#include "StdPartCatalog.h"
#include "PartLibrary.h"
#include "Geometry.h"
#include "ElementFactory.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace SketchCAD {

namespace {

constexpr double K_PI = 3.14159265358979323846;

Element makeLine(const Vec3& a, const Vec3& b, const QString& name) {
    Element e = ElementFactory::create(ElementType::Line, name, nullptr);
    e.pts = { a, b };
    return e;
}
Element makeCircle(const Vec3& c, double r, const QString& name) {
    Element e = ElementFactory::create(ElementType::Circle, name, nullptr);
    e.pts = { c }; e.radius = r;
    return e;
}
Element makePoly(const std::vector<Vec3>& pts, bool closed, const QString& name) {
    Element e = ElementFactory::create(ElementType::Polyline, name, nullptr);
    e.pts = pts; e.closed = closed;
    return e;
}

// 线性标注（自动实测值；文字/箭头样式由 dim::Style + resolveTextStyle 保证）
Element makeLinearDim(const Vec3& a, const Vec3& b, const Vec3& at) {
    Element e = ElementFactory::create(ElementType::Dimension, QStringLiteral("尺寸标注"), nullptr);
    e.dtype = DimensionType::Distance;
    e.pts = { a, b, at };
    e.dimValue = geo::measureDimension(e);
    return e;
}
Element makeDiameterDim(const Vec3& c, double r, const Vec3& at) {
    Element e = ElementFactory::create(ElementType::Dimension, QStringLiteral("尺寸标注"), nullptr);
    e.dtype = DimensionType::Diameter;
    e.pts = { c, Vec3(c.x + r, c.y, 0), at };
    e.dimValue = geo::measureDimension(e);
    return e;
}

// 正六边形（点朝 ±X）：对边 s → 外接 R=s/√3
std::vector<Vec3> hexPts(const Vec3& c, double s) {
    const double R = s / std::sqrt(3.0);
    std::vector<Vec3> p;
    for (int i = 0; i < 6; ++i) {
        const double a = (60.0 * i) * K_PI / 180.0;
        p.push_back(Vec3(c.x + R * std::cos(a), c.y + R * std::sin(a), 0));
    }
    return p;
}

QVariantMap pickSpec(const StdPartCategory& cat, const QVariantMap& params) {
    QVariantMap p;
    for (const StdPartParamDef& d : cat.params)
        p[d.key] = params.value(d.key, d.defaultValue);
    return p;
}

} // namespace

StdPartCatalog& StdPartCatalog::instance() {
    static StdPartCatalog s;
    return s;
}

bool StdPartCatalog::load(const QString& jsonPath, QString* err) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("标准件目录配置无法打开: %1").arg(jsonPath);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (err) *err = QStringLiteral("标准件目录 JSON 顶层非对象: %1").arg(jsonPath);
        return false;
    }
    const QJsonArray arr = doc.object().value(QStringLiteral("categories")).toArray();
    QVector<StdPartCategory> cats;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        StdPartCategory c;
        c.id = o.value(QStringLiteral("id")).toString();
        c.name = o.value(QStringLiteral("name")).toString();
        c.standard = o.value(QStringLiteral("standard")).toString();
        c.generator = o.value(QStringLiteral("generator")).toString();
        if (c.id.isEmpty() || c.generator.isEmpty()) continue;
        for (const QJsonValue& pv : o.value(QStringLiteral("params")).toArray()) {
            const QJsonObject po = pv.toObject();
            StdPartParamDef d;
            d.key = po.value(QStringLiteral("key")).toString();
            d.label = po.value(QStringLiteral("label")).toString();
            d.unit = po.value(QStringLiteral("unit")).toString();
            d.defaultValue = po.value(QStringLiteral("default")).toDouble();
            d.minValue = po.value(QStringLiteral("min")).toDouble(0.0);
            d.maxValue = po.value(QStringLiteral("max")).toDouble(0.0);
            d.required = po.value(QStringLiteral("required")).toBool(false);
            d.description = po.value(QStringLiteral("description")).toString();
            if (!d.key.isEmpty()) c.params.append(d);
        }
        for (const QJsonValue& sv : o.value(QStringLiteral("specs")).toArray())
            c.specs.append(sv.toObject().toVariantMap());
        c.sourceStats = o.value(QStringLiteral("sourceStats")).toObject();
        cats.append(c);
    }
    if (cats.isEmpty()) {
        if (err) *err = QStringLiteral("标准件目录无有效分类: %1").arg(jsonPath);
        return false;
    }
    cats_ = cats;
    loaded_ = true;
    return true;
}

const StdPartCategory* StdPartCatalog::find(const QString& id) const {
    for (const StdPartCategory& c : cats_)
        if (c.id == id) return &c;
    return nullptr;
}

QVector<QString> StdPartCatalog::ids() const {
    QVector<QString> r;
    for (const StdPartCategory& c : cats_) r.append(c.id);
    return r;
}

bool StdPartCatalog::generate(const QString& id, const QVariantMap& params,
                              std::vector<Element>& out, QString* note,
                              bool withDims) const {
    const StdPartCategory* cat = find(id);
    if (!cat) {
        if (note) *note = QStringLiteral("未知标准件 id: %1").arg(id);
        return false;
    }
    const QVariantMap p = pickSpec(*cat, params);
    const double d = p.value(QStringLiteral("nominal"), 6.0).toDouble();
    const double L = p.value(QStringLiteral("length"), 30.0).toDouble();
    const double s = p.value(QStringLiteral("acrossFlats"), 0.0).toDouble();
    const double k = p.value(QStringLiteral("thickness"), 0.0).toDouble();
    if (d <= 0.0) {
        if (note) *note = QStringLiteral("公称直径必须 > 0");
        return false;
    }

    const QString& gen = cat->generator;
    if (gen == QStringLiteral("hexbolt") || gen == QStringLiteral("bolt")) {
        // 复用 PartLibrary 的精确 GB/T 5782 生成器（含真实标注）
        std::vector<Element> g;
        QVariantMap bp = p;
        bp[QStringLiteral("nominal")] = d;
        bp[QStringLiteral("length")] = L;
        if (PartLibrary::instance().generate(QStringLiteral("bolt"), bp, g, note))
            out.insert(out.end(), g.begin(), g.end());
        return true;
    }
    if (gen == QStringLiteral("nut")) {
        std::vector<Element> g;
        if (PartLibrary::instance().generate(QStringLiteral("nut"), p, g, note))
            out.insert(out.end(), g.begin(), g.end());
        return true;
    }
    if (gen == QStringLiteral("washer")) {
        std::vector<Element> g;
        if (PartLibrary::instance().generate(QStringLiteral("washer"), p, g, note))
            out.insert(out.end(), g.begin(), g.end());
        return true;
    }
    if (gen == QStringLiteral("pin") || gen == QStringLiteral("roundbar")) {
        // 圆柱销：主视矩形（d × L）+ 端面圆 + 左视圆，自动标注直径与长度
        const double R = d / 2.0;
        out.push_back(makePoly({
            Vec3(0,-R,0), Vec3(L,-R,0), Vec3(L,R,0), Vec3(0,R,0)
        }, true, QStringLiteral("销主视")));
        out.push_back(makeCircle(Vec3(-L*0.25,0,0), R, QStringLiteral("销端面")));
        if (withDims) {
            out.push_back(makeLinearDim(Vec3(0,R,0), Vec3(L,R,0), Vec3(L/2, R + k*0.5 + 3, 0)));
            out.push_back(makeDiameterDim(Vec3(-L*0.25,0,0), R, Vec3(-L*0.25 - R - 6, R + 4, 0)));
        }
        return true;
    }
    if (gen == QStringLiteral("hexprism")) {
        // 方头/六角头类：按对边 s 绘制六角 + 杆，自动标注对边与长度
        const double af = s > 0 ? s : d * 1.5;
        out.push_back(makePoly(hexPts(Vec3(0,0,0), af), true, QStringLiteral("六角端面")));
        out.push_back(makePoly({
            Vec3(af/2, -d/2, 0), Vec3(af/2 + L, -d/2, 0),
            Vec3(af/2 + L, d/2, 0), Vec3(af/2, d/2, 0)
        }, true, QStringLiteral("杆部主视")));
        if (withDims) {
            out.push_back(makeLinearDim(Vec3(af/2,-d/2,0), Vec3(af/2+L,-d/2,0),
                                        Vec3(af/2+L/2, -d/2 - 8, 0)));
            out.push_back(makeDiameterDim(Vec3(0,0,0), af/std::sqrt(3.0),
                                          Vec3(-af, af, 0)));
        }
        return true;
    }
    // 未知生成器：回落为简化圆柱（保证有可用图形，notes 明示）
    if (note) *note = QStringLiteral("标准件 %1 使用通用圆柱回落（生成器=%2）")
                         .arg(id).arg(gen);
    out.push_back(makePoly({
        Vec3(0,-d/2,0), Vec3(L>0?L:d*3,-d/2,0), Vec3(L>0?L:d*3,d/2,0), Vec3(0,d/2,0)
    }, true, QStringLiteral("通用件")));
    return true;
}

bool StdPartCatalog::buildFromAnalysis(const QString& analysisJson,
                                       const QString& outJson,
                                       QString* err,
                                       int* categoryCount,
                                       int* specCount) {
    QFile f(analysisJson);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开解析索引: %1").arg(analysisJson);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (err) *err = QStringLiteral("解析索引 JSON 非法: %1").arg(analysisJson);
        return false;
    }
    const QJsonArray cats = doc.object().value(QStringLiteral("categories")).toArray();
    const QJsonArray files = doc.object().value(QStringLiteral("files")).toArray();

    QMap<QString, QJsonObject> byCat;
    for (const QJsonValue& v : cats) {
        const QJsonObject o = v.toObject();
        byCat[o.value(QStringLiteral("dir")).toString()] = o;
    }
    QMap<QString, QSet<QString>> specSets;
    QMap<QString, QJsonObject> stats;
    for (const QJsonValue& v : files) {
        const QJsonObject r = v.toObject();
        const QString cat = r.value(QStringLiteral("category")).toString();
        const QJsonObject sp = r.value(QStringLiteral("spec")).toObject();
        QString key;
        if (sp.contains(QStringLiteral("nominal")) && sp.contains(QStringLiteral("lengthMm")))
            key = QStringLiteral("M%1x%2").arg(sp.value(QStringLiteral("nominalMm")).toDouble())
                                            .arg(sp.value(QStringLiteral("lengthMm")).toDouble());
        else if (sp.contains(QStringLiteral("nominal")))
            key = sp.value(QStringLiteral("nominal")).toString();
        else if (sp.contains(QStringLiteral("diameterMm")))
            key = QStringLiteral("%1x%2").arg(sp.value(QStringLiteral("diameterMm")).toDouble())
                                          .arg(sp.value(QStringLiteral("lengthMm")).toDouble());
        else
            key = sp.value(QStringLiteral("raw")).toString();
        if (!key.isEmpty()) specSets[cat].insert(key);
        stats[cat] = r;
    }

    QJsonArray outCats;
    int sc = 0;
    for (auto it = byCat.constBegin(); it != byCat.constEnd(); ++it) {
        const QString dir = it.key();
        const QJsonObject meta = it.value();
        const QString name = meta.value(QStringLiteral("kind")).toString();
        const QString stdNo = meta.value(QStringLiteral("standard")).toString();
        // 由品名推断生成器类型
        QString gen = QStringLiteral("pin");
        if (name.contains(QStringLiteral("螺栓")) || name.contains(QStringLiteral("螺钉"))) gen = QStringLiteral("hexbolt");
        else if (name.contains(QStringLiteral("螺母"))) gen = QStringLiteral("nut");
        else if (name.contains(QStringLiteral("垫圈"))) gen = QStringLiteral("washer");
        else if (name.contains(QStringLiteral("销"))) gen = QStringLiteral("pin");
        else if (name.contains(QStringLiteral("六角"))) gen = QStringLiteral("hexprism");

        // 稳定 id：GB 号 + 类型 + 目录拼音去重用序号（用 GB 号 + 名字哈希保证唯一）
        const QString gbDigits = meta.value(QStringLiteral("standardNumber")).toString();
        const quint64 h = qHash(dir);
        const QString id = QStringLiteral("gb%1_%2_%3")
                             .arg(gbDigits.isEmpty() ? QStringLiteral("x") : gbDigits)
                             .arg(gen).arg(QString::number(h & 0xFFFF, 16));

        QJsonArray params;
        auto addParam = [&](const QString& key, const QString& label, double def,
                            const QString& desc) {
            QJsonObject po;
            po[QStringLiteral("key")] = key;
            po[QStringLiteral("label")] = label;
            po[QStringLiteral("unit")] = QStringLiteral("mm");
            po[QStringLiteral("default")] = def;
            po[QStringLiteral("required")] = false;
            po[QStringLiteral("description")] = desc;
            params.append(po);
        };
        addParam(QStringLiteral("nominal"), QStringLiteral("公称直径 d"), 6.0,
                 QStringLiteral("螺纹/销轴公称直径，GB 标准主参数"));
        addParam(QStringLiteral("length"), QStringLiteral("长度 L"), 30.0,
                 QStringLiteral("杆部/销轴总长"));
        addParam(QStringLiteral("pitch"), QStringLiteral("螺距 P"), 1.0,
                 QStringLiteral("粗牙螺距，随公称直径按 GB/T 193 取值"));
        addParam(QStringLiteral("acrossFlats"), QStringLiteral("对边 s"), 10.0,
                 QStringLiteral("六角头/螺母对边宽度"));
        addParam(QStringLiteral("thickness"), QStringLiteral("厚度 k/m/h"), 5.0,
                 QStringLiteral("头高/螺母厚/垫圈厚"));

        QJsonArray specs;
        QStringList keys = specSets.value(dir).values();
        keys.sort();
        for (const QString& k : keys) {
            QJsonObject so;
            so[QStringLiteral("designation")] = k;
            specs.append(so);
            ++sc;
        }
        QJsonObject co;
        co[QStringLiteral("id")] = id;
        co[QStringLiteral("name")] = name;
        co[QStringLiteral("standard")] = stdNo;
        co[QStringLiteral("generator")] = gen;
        co[QStringLiteral("sourceDir")] = dir;
        co[QStringLiteral("params")] = params;
        co[QStringLiteral("specs")] = specs;
        co[QStringLiteral("sourceStats")] = stats.value(dir);
        outCats.append(co);
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 1;
    root[QStringLiteral("source")] = QStringLiteral("CAD标准件图库批量解析(stdpart_index.json)");
    root[QStringLiteral("categories")] = outCats;
    QFile of(outJson);
    if (!of.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("无法写入标准件目录: %1").arg(outJson);
        return false;
    }
    of.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    of.close();
    if (categoryCount) *categoryCount = outCats.size();
    if (specCount) *specCount = sc;
    return true;
}

} // namespace SketchCAD
