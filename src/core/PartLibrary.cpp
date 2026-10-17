#include "PartLibrary.h"
#include "InvoluteGear.h"
#include "GbFastenerTable.h"
#include "Block.h"        // 出库：makeDefFromEntities / makeRef / AttributeDef
#include <cmath>

namespace SketchCAD {

// ------------------------------------------------------------
// 图元构造辅助（紧固件侧视/正视简化几何，均为闭合 Polyline 或 Circle）
// ------------------------------------------------------------
namespace {

constexpr double K_PI = 3.14159265358979323846;

Element mkPolyline(const std::vector<Vec3>& pts, const QString& name, bool closed = true) {
    Element e;
    e.type = ElementType::Polyline;
    e.pts = pts;
    e.closed = closed;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = true;
    return e;
}
Element mkCircle(const Vec3& c, double r, const QString& name) {
    Element e;
    e.type = ElementType::Circle;
    e.pts = { c };
    e.radius = r;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = true;
    return e;
}

// 正六边形（点朝 +X）：外接圆半径 R，跨对边(垂直方向 y 向) = R√3。
// 使六边形 y 向高度 = acrossFlats → R = s/√3。
std::vector<Vec3> hexPts(const Vec3& center, double acrossFlats) {
    const double R = acrossFlats / std::sqrt(3.0);
    std::vector<Vec3> p;
    p.reserve(6);
    for (int i = 0; i < 6; ++i) {
        const double a = (60.0 * i) * K_PI / 180.0;   // 0,60,...,300（顶点在 ±X）
        p.push_back(Vec3(center.x + R * std::cos(a), center.y + R * std::sin(a), 0.0));
    }
    return p;
}

// 内建零件生成器 ------------------------------------------------

// 齿轮（P4）：参数 teeth / module / pressureAngle → 闭合 Polyline 齿廓
class GearGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("gear"); }
    QString displayName() const override { return QStringLiteral("渐开线齿轮"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* = nullptr) const override {
        const int teeth = params.value(QStringLiteral("teeth"), 20).toInt();
        const double module = params.value(QStringLiteral("module"), 2.0).toDouble();
        const double pa = params.value(QStringLiteral("pressureAngle"), 20.0).toDouble();
        std::vector<Vec3> pts;
        if (!InvoluteGear::profile(teeth, module, pa, pts, true)) return false;
        out.push_back(mkPolyline(pts, QStringLiteral("齿轮")));
        return true;
    }
};

// 六角头螺栓（侧视）：头部矩形(对边 s × 头高 k) + 杆部矩形(公称 d × 长度 L) + 头部垫面圆。
class BoltGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("bolt"); }
    QString displayName() const override { return QStringLiteral("六角头螺栓"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const double length  = params.value(QStringLiteral("length"), 30.0).toDouble();
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::HexBolt, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("六角头螺栓：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        const double s = sp.acrossFlats, k = sp.thickness, d = sp.innerD;
        // 头部矩形：宽=s(对边)、高=k，顶部 y=0 起向下
        out.push_back(mkPolyline({ Vec3(-s/2, -k, 0), Vec3(s/2, -k, 0),
                                   Vec3(s/2, 0, 0),  Vec3(-s/2, 0, 0) }, QStringLiteral("螺栓头部")));
        // 杆部矩形：宽=d、高=length，接头部下方
        out.push_back(mkPolyline({ Vec3(-d/2, -k - length, 0), Vec3(d/2, -k - length, 0),
                                   Vec3(d/2, -k, 0),            Vec3(-d/2, -k, 0) }, QStringLiteral("螺栓杆部")));
        // 头部垫面圆（示意，直径≈0.95s）
        out.push_back(mkCircle(Vec3(0, -k/2, 0), 0.95 * s / 2.0, QStringLiteral("螺栓垫面")));
        return true;
    }
};

// 六角螺母（正视）：正六边形(对边 s) + 螺纹孔圆(≈d)。
class NutGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("nut"); }
    QString displayName() const override { return QStringLiteral("六角螺母"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::HexNut, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("六角螺母：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        out.push_back(mkPolyline(hexPts(Vec3(0, 0, 0), sp.acrossFlats), QStringLiteral("螺母六角")));
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.innerD / 2.0, QStringLiteral("螺母螺纹孔")));
        return true;
    }
};

// 平垫圈（正视）：外圆(d2) + 内圆(d1)。
class WasherGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("washer"); }
    QString displayName() const override { return QStringLiteral("平垫圈"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::PlainWasher, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("平垫圈：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.outerD / 2.0, QStringLiteral("垫圈外圆")));
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.innerD / 2.0, QStringLiteral("垫圈内圆")));
        return true;
    }
};

} // namespace

// ------------------------------------------------------------
// PartLibrary 单例
// ------------------------------------------------------------
PartLibrary& PartLibrary::instance() {
    static PartLibrary s;
    return s;
}

PartLibrary::PartLibrary() {
    // 构造时注册内建零件（P5：GB 紧固件出库）
    registerGenerator(std::make_shared<GearGenerator>());
    registerGenerator(std::make_shared<BoltGenerator>());
    registerGenerator(std::make_shared<NutGenerator>());
    registerGenerator(std::make_shared<WasherGenerator>());
}

void PartLibrary::registerGenerator(std::shared_ptr<PartGenerator> g) {
    if (g) gens_[g->id()] = std::move(g);
}

const PartGenerator* PartLibrary::find(const QString& id) const {
    auto it = gens_.find(id);
    return (it != gens_.end()) ? it.value().get() : nullptr;
}

bool PartLibrary::generate(const QString& id, const QVariantMap& params, std::vector<Element>& out, QString* note) const {
    const PartGenerator* g = find(id);
    if (!g) return false;
    return g->generate(params, out, note);
}

bool PartLibrary::checkout(const QString& id, const QVariantMap& params, BlockDef& defOut, QString* note) const {
    const PartGenerator* g = find(id);
    if (!g) return false;
    std::vector<Element> geom;
    if (!g->generate(params, geom, note)) return false;
    // 实例化块定义（引用式）：几何已按 WCS 生成、基点取原点
    defOut = block::makeDefFromEntities(id, Vec3(0, 0, 0), geom, g->displayName());
    // 追加属性定义（标准 / 公称 / 名称），供 BOM 提取（block::extractAttributes）
    const double nominal = params.value(QStringLiteral("nominal"), 0.0).toDouble();
    auto addAttr = [&](const QString& tag, const QString& val) {
        AttributeDef a;
        a.tag = tag; a.prompt = tag; a.defaultValue = val;
        a.pos = Vec3(0, 0, 0); a.height = 3.5; a.constant = true; a.invisible = true;
        defOut.attdefs.push_back(a);
    };
    if (nominal > 0.0) {
        const FastenerKind kind = GbFastenerTable::kindFromName(id);
        addAttr(QStringLiteral("STANDARD"), GbFastenerTable::standardTag(kind, nominal));
    }
    addAttr(QStringLiteral("NOMINAL"), QString::number(nominal, 'f', 1));
    addAttr(QStringLiteral("NAME"), g->displayName());
    return true;
}

QVector<QString> PartLibrary::ids() const {
    return gens_.keys().toVector();
}

} // namespace SketchCAD
