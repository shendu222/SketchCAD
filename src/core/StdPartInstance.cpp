#include "StdPartInstance.h"
#include "StdPartTemplate.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace SketchCAD {

namespace {

constexpr double kPi = 3.14159265358979323846;

QJsonArray vecToJson(const Vec3& v) {
    QJsonArray a;
    a.append(v.x); a.append(v.y); a.append(v.z);
    return a;
}

Vec3 vecFromJson(const QJsonValue& v) {
    const QJsonArray a = v.toArray();
    if (a.size() < 2) return Vec3(0, 0, 0);
    const double z = (a.size() >= 3) ? a.at(2).toDouble() : 0.0;
    return Vec3(a.at(0).toDouble(), a.at(1).toDouble(), z);
}

bool readVersion(const QJsonObject& o, const QString& key, int* out, QString* err) {
    *out = 1;   // 缺键按旧档兼容（与 SketchModel 的版本守卫一致）
    if (!o.contains(key)) return true;
    bool ok = false;
    const int n = o.value(key).toVariant().toInt(&ok);
    if (!ok) {
        if (err) *err = QStringLiteral("版本字段 %1 不是整数").arg(key);
        return false;
    }
    *out = n;
    return true;
}

bool hasRotationField(ElementType t) {
    switch (t) {
    case ElementType::Text:
    case ElementType::MText:
    case ElementType::BlockRef:
    case ElementType::Ellipse:
    case ElementType::Hatch:
    case ElementType::MLeader:
    case ElementType::Fcf:
        return true;
    default:
        return false;
    }
}

} // namespace

QJsonObject StdPartPlacement::toJson() const {
    QJsonObject o;
    o[QStringLiteral("mode")]       = mode;
    o[QStringLiteral("point")]      = vecToJson(point);
    o[QStringLiteral("pointText")]  = pointText;
    o[QStringLiteral("base")]       = base;
    o[QStringLiteral("customBase")] = vecToJson(customBase);
    o[QStringLiteral("angleDeg")]   = angleDeg;
    o[QStringLiteral("scale")]      = scale;
    o[QStringLiteral("layer")]      = layer;
    return o;
}

StdPartPlacement StdPartPlacement::fromJson(const QJsonObject& o) {
    StdPartPlacement p;
    if (o.contains(QStringLiteral("mode")))       p.mode       = o.value(QStringLiteral("mode")).toString();
    if (o.contains(QStringLiteral("point")))      p.point      = vecFromJson(o.value(QStringLiteral("point")));
    if (o.contains(QStringLiteral("pointText")))  p.pointText  = o.value(QStringLiteral("pointText")).toString();
    if (o.contains(QStringLiteral("base")))       p.base       = o.value(QStringLiteral("base")).toString();
    if (o.contains(QStringLiteral("customBase"))) p.customBase = vecFromJson(o.value(QStringLiteral("customBase")));
    if (o.contains(QStringLiteral("angleDeg")))   p.angleDeg   = o.value(QStringLiteral("angleDeg")).toDouble();
    if (o.contains(QStringLiteral("scale")))      p.scale      = o.value(QStringLiteral("scale")).toDouble(1.0);
    if (o.contains(QStringLiteral("layer")))      p.layer      = o.value(QStringLiteral("layer")).toString();
    if (p.layer.isEmpty()) p.layer = QStringLiteral("0");
    if (p.scale <= 0.0 || !std::isfinite(p.scale)) p.scale = 1.0;
    return p;
}

QJsonObject StdPartInstance::toJson() const {
    QJsonObject o;
    o[QStringLiteral("app")]                  = QStringLiteral("SketchCAD");
    o[QStringLiteral("kind")]                 = kind;
    o[QStringLiteral("formatVersion")]        = formatVersion;
    o[QStringLiteral("schemaVersion")]        = schemaVersion;
    o[QStringLiteral("typeId")]               = typeId;
    o[QStringLiteral("category")]             = category;
    o[QStringLiteral("standard")]             = standard;
    o[QStringLiteral("templateId")]           = templateId;
    o[QStringLiteral("templateLibraryVersion")] = templateLibraryVersion;
    o[QStringLiteral("units")]                = units;
    o[QStringLiteral("createdAt")]            = createdAt;

    QJsonObject ps;
    for (QVariantMap::const_iterator it = params.constBegin(); it != params.constEnd(); ++it)
        ps.insert(it.key(), QJsonValue::fromVariant(it.value()));
    o[QStringLiteral("params")]    = ps;
    o[QStringLiteral("placement")] = placement.toJson();
    return o;
}

StdPartInstance StdPartInstance::fromJson(const QJsonObject& o, QString* err) {
    StdPartInstance inst;
    if (!readVersion(o, QStringLiteral("formatVersion"), &inst.formatVersion, err)) return inst;
    if (!readVersion(o, QStringLiteral("schemaVersion"), &inst.schemaVersion, err)) return inst;
    if (inst.formatVersion != 1 || inst.schemaVersion != 1 || inst.formatVersion != inst.schemaVersion) {
        if (err) *err = QStringLiteral("不支持的标准件参数版本：formatVersion=%1、schemaVersion=%2（仅支持 1）")
                            .arg(inst.formatVersion).arg(inst.schemaVersion);
        return inst;
    }
    const QString k = o.value(QStringLiteral("kind")).toString();
    if (!k.isEmpty() && k != QStringLiteral("stdpart-instance")) {
        if (err) *err = QStringLiteral("不是标准件参数实例 JSON：kind=%1").arg(k);
        return inst;
    }
    inst.typeId     = o.value(QStringLiteral("typeId")).toString();
    inst.category   = o.value(QStringLiteral("category")).toString();
    inst.standard   = o.value(QStringLiteral("standard")).toString();
    inst.templateId = o.value(QStringLiteral("templateId")).toString();
    if (inst.templateId.isEmpty()) {
        if (err) *err = QStringLiteral("缺少 templateId，无法定位原图模板");
        return inst;
    }
    inst.templateLibraryVersion = o.value(QStringLiteral("templateLibraryVersion")).toInt(1);
    inst.units      = o.value(QStringLiteral("units")).toString(QStringLiteral("mm"));
    inst.createdAt  = o.value(QStringLiteral("createdAt")).toString();

    const QJsonObject ps = o.value(QStringLiteral("params")).toObject();
    for (QJsonObject::const_iterator it = ps.constBegin(); it != ps.constEnd(); ++it)
        inst.params.insert(it.key(), it.value().toVariant());
    inst.placement = StdPartPlacement::fromJson(o.value(QStringLiteral("placement")).toObject());
    return inst;
}

bool StdPartInstance::writeFile(const QString& path, const StdPartInstance& inst,
                                QString* err) {
    if (path.isEmpty()) {
        if (err) *err = QStringLiteral("保存路径为空");
        return false;
    }
    StdPartInstance copy = inst;
    if (copy.createdAt.isEmpty())
        copy.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QJsonDocument doc(copy.toJson());
    QFile f(path);
    QFileInfo fi(path);
    if (!fi.absoluteDir().exists()) QDir().mkpath(fi.absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("无法写入标准件参数 JSON：%1").arg(path);
        return false;
    }
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (f.write(bytes) != bytes.size()) {
        if (err) *err = QStringLiteral("写入标准件参数 JSON 不完整：%1").arg(path);
        return false;
    }
    return true;
}

bool StdPartInstance::readFile(const QString& path, StdPartInstance& out,
                               QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开标准件参数 JSON：%1").arg(path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (err) *err = QStringLiteral("标准件参数 JSON 根节点不是对象：%1").arg(path);
        return false;
    }
    out = fromJson(doc.object(), err);
    return err == nullptr || err->isEmpty();
}

QString StdPartInstance::typeIdFor(const StdPartTemplateMeta& meta) {
    QString src = meta.standard.isEmpty() ? meta.category : meta.standard;
    QString id = src.toUpper().replace(QRegularExpression(QStringLiteral("[^A-Z0-9]+")),
                                       QStringLiteral("_"));
    id.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (id.isEmpty()) id = QStringLiteral("STDPART");
    return id;
}

std::vector<Element> StdPartInstance::placeElements(const std::vector<Element>& src,
                                                    const StdPartPlacement& p,
                                                    QString* err) {
    std::vector<Element> out;
    if (!std::isfinite(p.scale) || p.scale <= 0.0) {
        if (err) *err = QStringLiteral("插入缩放必须为正数");
        return out;
    }
    if (!std::isfinite(p.angleDeg)) {
        if (err) *err = QStringLiteral("插入旋转角不是有效数值");
        return out;
    }

    Vec3 base;
    bool haveBounds = false;
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    for (const Element& e : src) {
        if (!e.pts.empty()) {
            for (const Vec3& v : e.pts) {
                if (!haveBounds) {
                    minX = maxX = v.x; minY = maxY = v.y; haveBounds = true;
                } else {
                    minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
                    minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
                }
            }
        }
        if ((e.type == ElementType::Circle || e.type == ElementType::Arc) &&
            !e.pts.empty() && e.radius > 0.0) {
            const Vec3& c = e.pts[0];
            if (!haveBounds) {
                minX = maxX = c.x - e.radius; maxX = c.x + e.radius;
                minY = maxY = c.y - e.radius; maxY = c.y + e.radius; haveBounds = true;
            } else {
                minX = std::min(minX, c.x - e.radius); maxX = std::max(maxX, c.x + e.radius);
                minY = std::min(minY, c.y - e.radius); maxY = std::max(maxY, c.y + e.radius);
            }
        }
    }
    if (p.base == QStringLiteral("bboxCenter"))
        base = haveBounds ? Vec3((minX + maxX) / 2.0, (minY + maxY) / 2.0, 0.0) : Vec3(0, 0, 0);
    else if (p.base == QStringLiteral("lowerLeft"))
        base = haveBounds ? Vec3(minX, minY, 0.0) : Vec3(0, 0, 0);
    else if (p.base == QStringLiteral("circleCenter")) {
        base = Vec3(0, 0, 0);
        for (const Element& e : src) {
            if ((e.type == ElementType::Circle || e.type == ElementType::Arc) && !e.pts.empty()) {
                base = e.pts[0];
                break;
            }
        }
    } else if (p.base == QStringLiteral("custom")) {
        base = p.customBase;
    } else {
        if (err) *err = QStringLiteral("未知的插入基点：%1").arg(p.base);
        return out;
    }

    const double rad = p.angleDeg * kPi / 180.0;
    const double ca = std::cos(rad), sa = std::sin(rad);
    out.reserve(src.size());
    for (Element e : src) {
        for (Vec3& v : e.pts) {
            const double dx = (v.x - base.x) * p.scale;
            const double dy = (v.y - base.y) * p.scale;
            v.x = p.point.x + dx * ca - dy * sa;
            v.y = p.point.y + dx * sa + dy * ca;
        }
        if (e.radius > 0.0)  e.radius  *= p.scale;
        if (e.radiusY > 0.0) e.radiusY *= p.scale;
        if (e.type == ElementType::Arc) {
            e.startAngle += p.angleDeg;
            e.endAngle   += p.angleDeg;
        }
        if (hasRotationField(e.type)) e.rotation += p.angleDeg;
        if (e.type == ElementType::Dimension && e.dimValue > 0.0) e.dimValue *= p.scale;
        if (!p.layer.isEmpty() && p.layer != QStringLiteral("随层")) e.layer = p.layer;
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<Element> StdPartInstance::render(const StdPartInstance& inst,
                                             QString* err) {
    std::vector<Element> out;
    StdPartTemplateStore& store = StdPartTemplateStore::instance();
    if (!store.loaded()) {
        QString le;
        if (!store.load(StdPartTemplateStore::defaultStoreDir(), &le)) {
            if (err) *err = le;
            return out;
        }
    }
    std::vector<Element> src;
    const StdPartTemplateMeta* meta = nullptr;
    if (!store.loadElements(inst.templateId, src, &meta, err)) return out;
    std::vector<Element> rendered = StdPartTemplateStore::render(src, *meta, inst.params);
    out = placeElements(rendered, inst.placement, err);
    return out;
}

} // namespace SketchCAD
