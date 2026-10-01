#include "Block.h"
#include <algorithm>
#include <cmath>
#include <set>

// ============================================================================
// 块子系统实现：2D 仿射变换 + 展开算法（插入变换 / 动态参数 / 属性 / 嵌套）
// ----------------------------------------------------------------------------
// 展开即"编译"：把「一份定义 + 一组实例参数」编译成一批世界坐标图元。
// 因为定义只有一份，重定义后所有引用重新展开即为新几何 —— 无需反向推送。
//
// 命名空间排布（编译期约束）：X2D 属于 SketchCAD，故其成员函数定义必须在
// SketchCAD 层（不能落在 SketchCAD::block 内）；块特有算法放在 SketchCAD::block。
// ============================================================================
namespace SketchCAD {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int    kMaxBlockDepth = 16;      // 嵌套深度上限（同时防环）
constexpr double kMinScale = 1e-6;         // 缩放绝对值下限（防奇异矩阵把几何压成一点）

inline double deg2rad(double d) { return d * kPi / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / kPi; }
} // namespace

// ============================ X2D 实现 ============================

double X2D::applyAngleDeg(double deg) const {
    const double a = deg2rad(deg);
    const Vec3 d = applyDir(Vec3(std::cos(a), std::sin(a), 0));
    if (std::fabs(d.x) < 1e-12 && std::fabs(d.y) < 1e-12) return deg;
    return rad2deg(std::atan2(d.y, d.x));
}
double X2D::scaleX() const { return std::hypot(m00, m10); }
double X2D::scaleY() const { return std::hypot(m01, m11); }
double X2D::scaleFactor() const { return std::sqrt(std::fabs(m00 * m11 - m01 * m10)); }

bool X2D::isAxisAligned() const {
    const double s = std::max({std::fabs(m00), std::fabs(m01),
                               std::fabs(m10), std::fabs(m11), 1e-12});
    return std::fabs(m01) < 1e-9 * s && std::fabs(m10) < 1e-9 * s;
}

X2D X2D::then(const X2D& outer) const {
    // 目标：r(p) = outer(this(p)) = (M_o·M)·p + (M_o·t + t_o)
    X2D r;
    r.m00 = outer.m00 * m00 + outer.m01 * m10;
    r.m01 = outer.m00 * m01 + outer.m01 * m11;
    r.m10 = outer.m10 * m00 + outer.m11 * m10;
    r.m11 = outer.m10 * m01 + outer.m11 * m11;
    r.t   = outer.apply(t);
    return r;
}

X2D X2D::insert(const Vec3& P, double rotDeg, double sx, double sy, const Vec3& bp) {
    const double a = deg2rad(rotDeg), c = std::cos(a), s = std::sin(a);
    X2D x;
    x.m00 = c * sx;  x.m01 = -s * sy;      // 线性部分 = R(a) · S(sx,sy)
    x.m10 = s * sx;  x.m11 =  c * sy;
    // 平移 = P - (R·S)·bp，使块基点恰好落在插入点 P 上
    const Vec3 rbp(x.m00 * bp.x + x.m01 * bp.y, x.m10 * bp.x + x.m11 * bp.y, 0);
    x.t = Vec3(P.x - rbp.x, P.y - rbp.y, P.z);
    return x;
}

namespace block {

namespace {

// 块表中按名查定义
const BlockDef* findDef(const std::vector<BlockDef>& table, const QString& name) {
    if (name.isEmpty()) return nullptr;
    for (const auto& d : table) if (d.name == name) return &d;
    return nullptr;
}

// 取实例上的动态参数值（缺省回落定义默认值）
double dynValueOf(const std::vector<BlockDynValue>& dyn, const QString& name, double fallback) {
    for (const auto& d : dyn) if (d.name == name) return d.value;
    return fallback;
}
QString dynStateOf(const std::vector<BlockDynValue>& dyn, const QString& name) {
    for (const auto& d : dyn) if (d.name == name) return d.state;
    return QString();
}
// 值域钳制（maxValue>minValue 时才生效）
double clampDyn(const DynParam& p, double v) {
    if (p.maxValue > p.minValue) return std::min(std::max(v, p.minValue), p.maxValue);
    return v;
}

// ---------------- 顶点点集几何操作（动态参数在块坐标系内生效）----------------

void rotatePts(std::vector<Vec3>& pts, const Vec3& base, double deg) {
    const double a = deg2rad(deg), c = std::cos(a), s = std::sin(a);
    for (auto& p : pts) {
        const double dx = p.x - base.x, dy = p.y - base.y;
        p.x = base.x + dx * c - dy * s;
        p.y = base.y + dx * s + dy * c;
    }
}
void rotateElement(Element& e, const Vec3& base, double deg) {
    if (std::fabs(deg) < 1e-12) return;
    rotatePts(e.pts, base, deg);
    for (auto& h : e.holes) rotatePts(h, base, deg);
    if (e.type == ElementType::Arc) { e.startAngle += deg; e.endAngle += deg; }
    if (e.type == ElementType::Ellipse || e.type == ElementType::Hatch) e.rotation += deg;
}

// 点 p 关于「过 base、方向 d（单位）」的直线作镜像
void reflectPts(std::vector<Vec3>& pts, const Vec3& base, const Vec3& d) {
    for (auto& p : pts) {
        const double vx = p.x - base.x, vy = p.y - base.y;
        const double t = vx * d.x + vy * d.y;      // 投影长度
        p.x = base.x + 2.0 * d.x * t - vx;
        p.y = base.y + 2.0 * d.y * t - vy;
    }
}
void mirrorElement(Element& e, const Vec3& base, const Vec3& d) {
    reflectPts(e.pts, base, d);
    for (auto& h : e.holes) reflectPts(h, base, d);
    const double beta = rad2deg(std::atan2(d.y, d.x));
    if (e.type == ElementType::Arc) {
        // 扫掠集 [start,end] 的镜像 = [2β-end, 2β-start]（仍逆时针为正）
        const double s0 = 2.0 * beta - e.endAngle;
        const double s1 = 2.0 * beta - e.startAngle;
        e.startAngle = s0; e.endAngle = s1;
    }
    if (e.type == ElementType::Ellipse || e.type == ElementType::Hatch) e.rotation = 2.0 * beta - e.rotation;
    // 顶点序列的镜像反转走向：反向存点，保持渲染/后续布尔运算的朝向一致
    if (!e.pts.empty() && (e.type == ElementType::Polygon || e.type == ElementType::Polyline)) {
        std::reverse(e.pts.begin(), e.pts.end());
        if (!e.arcFlags.empty()) std::reverse(e.arcFlags.begin(), e.arcFlags.end());
    }
}

// 拉伸：把"在可动侧（投影 > 0）"的顶点沿方向平移 dist（半径/半轴不变）
void stretchPts(std::vector<Vec3>& pts, const Vec3& base, const Vec3& d, double dist) {
    for (auto& p : pts) {
        const double t = (p.x - base.x) * d.x + (p.y - base.y) * d.y;
        if (t > 1e-9) { p.x += d.x * dist; p.y += d.y * dist; }
    }
}
void stretchElement(Element& e, const Vec3& base, const Vec3& d, double dist) {
    if (std::fabs(dist) < 1e-12) return;
    stretchPts(e.pts, base, d, dist);
    for (auto& h : e.holes) stretchPts(h, base, d, dist);
}

// ---------------- 把图元从"块坐标系"变换到"世界" ----------------
void transformElement(Element& e, const X2D& xf) {
    const double lx = xf.scaleX(), ly = xf.scaleY(), s = xf.scaleFactor();

    // 矩形：一旦线性部分含旋转/错切，轴对齐表示失效，必须离散为多边形 4 角点
    if (e.type == ElementType::Rectangle && !xf.isAxisAligned() && e.pts.size() >= 2) {
        const Vec3 a = e.pts[0], b = e.pts[1];
        std::vector<Vec3> corners = {Vec3(a.x, a.y, 0), Vec3(b.x, a.y, 0),
                                     Vec3(b.x, b.y, 0), Vec3(a.x, b.y, 0)};
        for (auto& p : corners) p = xf.apply(p);
        e.type = ElementType::Polygon;
        e.pts = corners;
        e.closed = true;
        e.radius = 0; e.radiusY = 0;
        return;
    }

    for (auto& p : e.pts) p = xf.apply(p);
    for (auto& h : e.holes) for (auto& p : h) p = xf.apply(p);

    switch (e.type) {
        case ElementType::Circle:
            if (std::fabs(lx - ly) > 1e-6 * std::max(lx, ly)) {
                // 非等比例：圆被压成椭圆（半轴 = r·各轴缩放，长轴取较大者）
                const Vec3 ax = xf.applyDir(Vec3(1, 0, 0));
                const Vec3 ay = xf.applyDir(Vec3(0, 1, 0));
                const double r = e.radius;
                if (lx >= ly) { e.radius = r * lx; e.radiusY = r * ly; e.rotation = rad2deg(std::atan2(ax.y, ax.x)); }
                else          { e.radius = r * ly; e.radiusY = r * lx; e.rotation = rad2deg(std::atan2(ay.y, ay.x)); }
                e.type = ElementType::Ellipse;
            } else {
                e.radius *= s;
            }
            break;
        case ElementType::Arc:
            e.radius *= s;      // 非等比例下为近似（圆弧无椭圆弧表示）
            e.startAngle = xf.applyAngleDeg(e.startAngle);
            e.endAngle   = xf.applyAngleDeg(e.endAngle);
            break;
        case ElementType::Ellipse:
            e.radius *= s; e.radiusY *= s;
            e.rotation = xf.applyAngleDeg(e.rotation);
            break;
        case ElementType::Hatch:
            e.radius *= s;      // 剖面线间距
            e.rotation = xf.applyAngleDeg(e.rotation);
            break;
        case ElementType::Boundary:
            e.radius *= s;      // 剖面线间距（与 Hatch 同字段）
            e.rotation = xf.applyAngleDeg(e.rotation);
            break;
        case ElementType::Text:
            e.radius *= s;      // 字高
            break;
        case ElementType::Dimension:
            e.radius *= s;
            e.startAngle = xf.applyAngleDeg(e.startAngle);
            e.endAngle   = xf.applyAngleDeg(e.endAngle);
            break;
        default: break;
    }
}

// 应用作用于实体 idx 的动态参数（可见性已在外部过滤）
void applyDynParams(Element& e, int idx, const std::vector<DynParam>& params,
                    const std::vector<BlockDynValue>& dyn) {
    for (const auto& p : params) {
        if (p.type == DynParamType::Visibility) continue;
        if (std::find(p.affected.begin(), p.affected.end(), idx) == p.affected.end()) continue;
        Vec3 d = p.direction;
        const double dl = std::hypot(d.x, d.y);
        if (dl < 1e-12) continue;
        d.x /= dl; d.y /= dl; d.z = 0;
        if (p.type == DynParamType::LinearStretch) {
            stretchElement(e, p.base, d, clampDyn(p, dynValueOf(dyn, p.name, p.defaultValue)));
        } else if (p.type == DynParamType::Rotate) {
            rotateElement(e, p.base, clampDyn(p, dynValueOf(dyn, p.name, p.defaultValue)));
        } else if (p.type == DynParamType::Flip) {
            if (std::fabs(dynValueOf(dyn, p.name, p.defaultValue)) > 1e-9)
                mirrorElement(e, p.base, d);
        }
    }
}

// 当前实例下被隐藏的块内图元索引集合（按可见性参数的当前状态）
std::set<int> hiddenSet(const BlockDef& def, const std::vector<BlockDynValue>& dyn) {
    std::set<int> hidden;
    for (const auto& p : def.dynParams) {
        if (p.type != DynParamType::Visibility || p.states.empty()) continue;
        const QString state = dynStateOf(dyn, p.name);
        const VisibilityState* st = nullptr;
        for (const auto& s : p.states) if (s.name == state) { st = &s; break; }
        if (!st) st = &p.states.front();       // 未指定/未匹配 → 取首状态
        for (int i : st->hiddenEntities) hidden.insert(i);
    }
    return hidden;
}

// 递归内核：在变换 xf 下求值块定义，输出世界坐标子图元。
//   visiting：当前递归路径的块名（环检测）；fallbackLayer：块内"默认层"图元应落的插入层。
std::vector<Element> evalDef(const BlockDef& def, const X2D& xf,
                             const std::vector<BlockDynValue>& dyn,
                             const std::vector<BlockAttrValue>& attrs,
                             const std::vector<BlockDef>& table,
                             std::vector<QString>& visiting, int depth,
                             const QString& fallbackLayer) {
    std::vector<Element> out;
    if (depth > kMaxBlockDepth) return out;

    const std::set<int> hidden = hiddenSet(def, dyn);

    for (int i = 0; i < (int)def.entities.size(); ++i) {
        if (hidden.count(i)) continue;
        Element e = def.entities[i];

        // 嵌套块：先算"子块局部插入变换"，再与父变换复合（先子后父）后递归
        if (e.type == ElementType::BlockRef) {
            const BlockDef* cd = findDef(table, e.blockName);
            if (!cd || depth >= kMaxBlockDepth) continue;
            if (std::find(visiting.begin(), visiting.end(), e.blockName) != visiting.end()) continue; // 环
            const Vec3 ins = e.pts.empty() ? Vec3(0, 0, 0) : e.pts[0];
            const X2D childXf = X2D::insert(ins, e.rotation, e.scaleX, e.scaleY, cd->basePoint).then(xf);
            const QString childLayer = (e.layer.isEmpty() || e.layer == QStringLiteral("0"))
                                           ? fallbackLayer : e.layer;
            visiting.push_back(e.blockName);
            auto sub = evalDef(*cd, childXf, e.dynValues, e.blockAttrs, table, visiting, depth + 1, childLayer);
            visiting.pop_back();
            out.insert(out.end(), sub.begin(), sub.end());
            continue;
        }

        applyDynParams(e, i, def.dynParams, dyn);   // 动态参数（块坐标系内）
        transformElement(e, xf);                    // 插入变换 → 世界
        // 归属：块内层为默认层的图元改用插入层（其余保留块内层）
        if (e.layer.isEmpty() || e.layer == QStringLiteral("0")) e.layer = fallbackLayer;
        e.id = 0;                                   // 展开结果是瞬态几何，不携带工作区 ID
        e.visible = true;
        out.push_back(std::move(e));
    }

    // 属性文本：按实例当前值生成可见文字（不可见属性/空值不生成）
    for (const auto& ad : def.attdefs) {
        if (ad.invisible) continue;
        QString val = ad.defaultValue;
        for (const auto& a : attrs) if (a.tag == ad.tag) { val = a.value; break; }
        if (val.isEmpty()) continue;
        Element t;
        t.type  = ElementType::Text;
        t.name  = ad.tag;
        t.layer = fallbackLayer;
        t.pts   = {xf.apply(ad.pos)};
        t.text  = val;
        t.radius = ad.height * xf.scaleFactor();    // 字高(mm)
        t.lineStyle = LineStyle::Solid;
        t.lineWidth = 1.0;
        t.lineWidthAuto = true;
        out.push_back(std::move(t));
    }
    return out;
}

} // namespace

// ============================ 展开入口 ============================

std::vector<Element> expandRef(const Element& ref, const std::vector<BlockDef>& table) {
    std::vector<Element> out;
    if (ref.type != ElementType::BlockRef) return out;
    const BlockDef* def = findDef(table, ref.blockName);
    if (!def) return out;                                  // 未定义块 → 不画任何东西

    const Vec3 ins = ref.pts.empty() ? Vec3(0, 0, 0) : ref.pts[0];
    // 缩放取绝对值下限，避免 0/极小值把几何压成一点或造成奇异矩阵
    const double sx = (std::fabs(ref.scaleX) < kMinScale) ? (ref.scaleX < 0 ? -kMinScale : kMinScale) : ref.scaleX;
    const double sy = (std::fabs(ref.scaleY) < kMinScale) ? (ref.scaleY < 0 ? -kMinScale : kMinScale) : ref.scaleY;

    const X2D xf = X2D::insert(ins, ref.rotation, sx, sy, def->basePoint);
    std::vector<QString> visiting{ref.blockName};
    const QString fallback = ref.layer.isEmpty() ? QStringLiteral("0") : ref.layer;
    return evalDef(*def, xf, ref.dynValues, ref.blockAttrs, table, visiting, 0, fallback);
}

Element makeRef(const BlockDef& def, const Vec3& P, double rotDeg, double sx, double sy) {
    Element e;
    e.type = ElementType::BlockRef;
    e.name = def.name;
    e.blockName = def.name;
    e.pts = {P};
    e.rotation = rotDeg;
    e.scaleX = sx;
    e.scaleY = sy;
    for (const auto& ad : def.attdefs)
        e.blockAttrs.push_back(BlockAttrValue{ad.tag, ad.defaultValue});
    for (const auto& p : def.dynParams) {
        BlockDynValue dv;
        dv.name  = p.name;
        dv.value = p.defaultValue;
        if (p.type == DynParamType::Visibility && !p.states.empty())
            dv.state = p.states.front().name;
        e.dynValues.push_back(dv);
    }
    return e;
}

std::vector<BlockAttrValue> extractAttributes(const Element& ref, const std::vector<BlockDef>& table) {
    std::vector<BlockAttrValue> out;
    if (ref.type != ElementType::BlockRef) return out;
    const BlockDef* def = findDef(table, ref.blockName);
    if (def) {
        for (const auto& ad : def->attdefs) {
            QString v = ad.defaultValue;
            for (const auto& a : ref.blockAttrs) if (a.tag == ad.tag) { v = a.value; break; }
            out.push_back(BlockAttrValue{ad.tag, v});
        }
    }
    // 定义中已删除、但实例仍保留的属性也一并提取（不丢数据）
    for (const auto& a : ref.blockAttrs) {
        bool found = false;
        for (const auto& o : out) if (o.tag == a.tag) { found = true; break; }
        if (!found) out.push_back(a);
    }
    return out;
}

BlockDef makeDefFromEntities(const QString& name, const Vec3& basePt,
                             const std::vector<Element>& srcs, const QString& desc) {
    BlockDef d;
    d.name = name;
    d.basePoint = basePt;
    d.description = desc;
    d.entities = srcs;
    for (auto& e : d.entities) e.id = 0;   // 块内图元不携带工作区 ID
    return d;
}

// ============================ 序列化 ============================

QJsonObject blockDefToJson(const BlockDef& d) {
    QJsonObject o;
    o["name"] = d.name;
    o["description"] = d.description;
    o["annotative"] = d.annotative;
    o["base"] = QJsonArray{d.basePoint.x, d.basePoint.y, d.basePoint.z};

    QJsonArray ents;
    for (const auto& e : d.entities) ents.append(elementToJson(e));
    o["entities"] = ents;

    QJsonArray ads;
    for (const auto& a : d.attdefs) {
        QJsonObject ao;
        ao["tag"] = a.tag; ao["prompt"] = a.prompt; ao["default"] = a.defaultValue;
        ao["pos"] = QJsonArray{a.pos.x, a.pos.y, a.pos.z};
        ao["height"] = a.height; ao["constant"] = a.constant; ao["invisible"] = a.invisible;
        ads.append(ao);
    }
    o["attdefs"] = ads;

    QJsonArray pps;
    for (const auto& p : d.dynParams) {
        QJsonObject po;
        po["name"] = p.name; po["type"] = (int)p.type;
        QJsonArray aff; for (int i : p.affected) aff.append(i);
        po["affected"] = aff;
        po["base"] = QJsonArray{p.base.x, p.base.y, p.base.z};
        po["dir"]  = QJsonArray{p.direction.x, p.direction.y, p.direction.z};
        po["default"] = p.defaultValue; po["min"] = p.minValue; po["max"] = p.maxValue;
        QJsonArray sts;
        for (const auto& s : p.states) {
            QJsonObject so; so["name"] = s.name;
            QJsonArray hs; for (int i : s.hiddenEntities) hs.append(i);
            so["hidden"] = hs;
            sts.append(so);
        }
        po["states"] = sts;
        pps.append(po);
    }
    o["dynParams"] = pps;

    // 外部参照属性：仅当本记录确实是 Xref 时才写入，
    // 保证普通块的存档内容与引入 Xref 之前**逐字节一致**（旧档兼容）。
    if (d.isXref) {
        QJsonObject x;
        x["path"]     = d.xrefPath;
        x["pathType"] = d.xrefPathType;
        x["nested"]   = d.xrefNested;
        x["status"]   = d.xrefStatus;
        o["xref"] = x;
    }
    return o;
}

BlockDef blockDefFromJson(const QJsonObject& o) {
    BlockDef d;
    d.name = o["name"].toString();
    d.description = o["description"].toString();
    d.annotative = o["annotative"].toBool(false);
    const QJsonArray base = o["base"].toArray();
    if (base.size() >= 2) d.basePoint = Vec3(base[0].toDouble(), base[1].toDouble(), base[2].toDouble());

    for (const auto& v : o["entities"].toArray()) d.entities.push_back(elementFromJson(v.toObject()));

    for (const auto& v : o["attdefs"].toArray()) {
        const QJsonObject ao = v.toObject();
        AttributeDef a;
        a.tag = ao["tag"].toString();
        a.prompt = ao["prompt"].toString();
        a.defaultValue = ao["default"].toString();
        const QJsonArray pos = ao["pos"].toArray();
        if (pos.size() >= 2) a.pos = Vec3(pos[0].toDouble(), pos[1].toDouble(), pos[2].toDouble());
        a.height = json::getDouble(ao, "height", 3.5);
        a.constant = ao["constant"].toBool(false);
        a.invisible = ao["invisible"].toBool(false);
        d.attdefs.push_back(a);
    }

    for (const auto& v : o["dynParams"].toArray()) {
        const QJsonObject po = v.toObject();
        DynParam p;
        p.name = po["name"].toString();
        p.type = (DynParamType)po["type"].toInt(0);
        for (const auto& iv : po["affected"].toArray()) p.affected.push_back(iv.toInt());
        const QJsonArray pb = po["base"].toArray();
        if (pb.size() >= 2) p.base = Vec3(pb[0].toDouble(), pb[1].toDouble(), pb[2].toDouble());
        const QJsonArray pd = po["dir"].toArray();
        if (pd.size() >= 2) p.direction = Vec3(pd[0].toDouble(), pd[1].toDouble(), pd[2].toDouble());
        p.defaultValue = po["default"].toDouble(0.0);
        p.minValue = po["min"].toDouble(0.0);
        p.maxValue = po["max"].toDouble(0.0);
        for (const auto& sv : po["states"].toArray()) {
            const QJsonObject so = sv.toObject();
            VisibilityState s;
            s.name = so["name"].toString();
            for (const auto& iv : so["hidden"].toArray()) s.hiddenEntities.push_back(iv.toInt());
            p.states.push_back(std::move(s));
        }
        d.dynParams.push_back(std::move(p));
    }

    // 外部参照属性：旧存档无 "xref" 键 → 保持 isXref=false（与普通块一致）
    if (o.contains(QStringLiteral("xref"))) {
        const QJsonObject x = o[QStringLiteral("xref")].toObject();
        d.isXref       = true;
        d.xrefPath     = x[QStringLiteral("path")].toString();
        d.xrefPathType = x[QStringLiteral("pathType")].toInt(0);
        d.xrefNested   = x[QStringLiteral("nested")].toBool(false);
        d.xrefStatus   = x[QStringLiteral("status")].toInt(0);
    }
    return d;
}

} // namespace block
} // namespace SketchCAD
