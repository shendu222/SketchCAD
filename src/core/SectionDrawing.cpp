#include "SectionDrawing.h"
#include <QColor>
#include <cmath>
#include <algorithm>

namespace SketchCAD {
namespace {

constexpr double kPi = 3.14159265358979323846;
const QColor C_OUTLINE(26, 29, 40);
const QColor C_HIDDEN (110, 120, 140);
const QColor C_CENTER (170, 60, 60);

// 圆 → 多边形采样（剖面区域布尔运算需要多边形边界；48 段在图纸上与真圆差异 < 0.1%）
std::vector<Vec3> sampleCircle(const Vec3& c, double r, int seg = 48) {
    std::vector<Vec3> pts;
    pts.reserve(seg);
    for (int i = 0; i < seg; ++i) {
        const double a = 2.0 * kPi * i / seg;
        pts.push_back(Vec3(c.x + r * std::cos(a), c.y + r * std::sin(a), 0));
    }
    return pts;
}

// 区域厚度自适应剖面线间距（与 BearingViews 同规则：过薄区域保底 3 条线）
double autoSpacing(const std::vector<Vec3>& poly, double want) {
    if (want > 0.0) return want;
    double xmin = 1e18, xmax = -1e18, ymin = 1e18, ymax = -1e18;
    for (const auto& p : poly) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    }
    const double t = std::min(xmax - xmin, ymax - ymin);
    return t < 9.0 ? std::max(0.7, t / 3.0) : 3.0;
}

} // namespace

// ============================== SectionViewBuilder ==============================

SectionViewBuilder::SectionViewBuilder(const QString& layerPrefix) : prefix_(layerPrefix) {}

SectionViewBuilder& SectionViewBuilder::setStyle(const SectionViewStyle& s) { st_ = s; return *this; }
SectionViewBuilder& SectionViewBuilder::setLayerPrefix(const QString& prefix) { prefix_ = prefix; return *this; }

void SectionViewBuilder::push(Element e, const QString& layer, const QString& name, double w, LineStyle ls) {
    e.layer = layer;
    e.name = name;
    e.lineStyle = ls;
    e.lineWidth = w;
    e.lineWidthAuto = false;
    e.color = C_OUTLINE;
    res_.elems.push_back(std::move(e));
}

SectionViewBuilder& SectionViewBuilder::addCutRegion(const std::vector<Vec3>& boundary,
                                                     const std::vector<std::vector<Vec3>>& holes,
                                                     bool autoOutline) {
    if (boundary.size() < 3) return *this;
    SectionRegion r;
    r.boundary = boundary;
    r.holes = holes;
    r.style.angleDeg = st_.hatchAngleDeg;
    r.style.spacingMm = autoSpacing(boundary, st_.hatchSpacingMm);
    r.style.thinWidthPx = st_.thinWidthPx;
    r.style.material = st_.material;
    r.style.exempt = st_.exempt;

    const QString ly = prefix_ + QStringLiteral("-剖面");
    Element h = makeHatchElement(r, prefix_ + QStringLiteral("·剖面区域"));
    h.layer = ly;
    h.color = C_OUTLINE;
    h.lineWidth = st_.thinWidthPx;
    h.lineWidthAuto = false;
    res_.elems.push_back(std::move(h));
    ++res_.hatchCount;

    if (autoOutline) {
        const QString lo = prefix_ + QStringLiteral("-轮廓");
        const auto& b = boundary;
        for (size_t i = 0; i < b.size(); ++i) {
            const Vec3& a = b[i];
            const Vec3& c = b[(i + 1) % b.size()];
            Element e;
            e.type = ElementType::Line;
            e.pts = {a, c};
            push(std::move(e), lo, prefix_ + QStringLiteral("·断面轮廓"), st_.outlineWidthPx, LineStyle::Solid);
        }
    }
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addCutRect(const Vec3& lo, const Vec3& hi, bool autoOutline) {
    const std::vector<Vec3> b = {lo, Vec3(hi.x, lo.y, 0), hi, Vec3(lo.x, hi.y, 0)};
    return addCutRegion(b, {}, autoOutline);
}

SectionViewBuilder& SectionViewBuilder::addVisibleLine(const Vec3& a, const Vec3& b) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    push(std::move(e), prefix_ + QStringLiteral("-轮廓"), prefix_ + QStringLiteral("·可见轮廓"),
         st_.outlineWidthPx, LineStyle::Solid);
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addVisiblePoly(const std::vector<Vec3>& pts, bool closed) {
    if (pts.size() < 2) return *this;
    const QString lo = prefix_ + QStringLiteral("-轮廓");
    const size_t n = pts.size();
    const size_t segs = closed ? n : n - 1;
    for (size_t i = 0; i < segs; ++i)
        addVisibleLine(pts[i], pts[(i + 1) % n]);
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addVisibleCircle(const Vec3& c, double r) {
    if (r <= 0.0) return *this;
    Element e;
    e.type = ElementType::Circle;
    e.pts = {c};
    e.radius = r;
    push(std::move(e), prefix_ + QStringLiteral("-轮廓"), prefix_ + QStringLiteral("·可见圆"),
         st_.outlineWidthPx, LineStyle::Solid);
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addVisibleArc(const Vec3& c, double r, double a0Deg, double a1Deg) {
    if (r <= 0.0) return *this;
    Element e;
    e.type = ElementType::Arc;
    e.pts = {c};
    e.radius = r;
    e.startAngle = a0Deg;
    e.endAngle = a1Deg;
    push(std::move(e), prefix_ + QStringLiteral("-轮廓"), prefix_ + QStringLiteral("·可见弧"),
         st_.outlineWidthPx, LineStyle::Solid);
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addHiddenLine(const Vec3& a, const Vec3& b) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    push(std::move(e), prefix_ + QStringLiteral("-虚线"), prefix_ + QStringLiteral("·不可见轮廓"),
         1.0, LineStyle::Dashed);
    res_.elems.back().color = C_HIDDEN;
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addCenterLine(const Vec3& a, const Vec3& b) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    push(std::move(e), prefix_ + QStringLiteral("-中心线"), prefix_ + QStringLiteral("·中心线"),
         0.5, LineStyle::DashDot);
    res_.elems.back().color = C_CENTER;
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addDim(DimensionType dt, const Vec3& a, const Vec3& b,
                                               const Vec3& pos, const QString& text) {
    Element e;
    e.type = ElementType::Dimension;
    e.dtype = dt;
    e.pts = {a, b, pos};
    e.text = text;
    e.dimTextHeight = st_.textHeight;
    push(std::move(e), prefix_ + QStringLiteral("-标注"), prefix_ + QStringLiteral("·尺寸标注"),
         1.0, LineStyle::Solid);
    ++res_.dimCount;
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addRoughness(const RoughnessSpec& spec) {
    RoughnessSpec s = spec;
    s.layer = prefix_ + QStringLiteral("-标注");
    auto syms = buildRoughnessSymbol(s);
    for (auto& e : syms) res_.elems.push_back(std::move(e));
    return *this;
}

SectionViewBuilder& SectionViewBuilder::addLabel(const Vec3& pos, const QString& text) {
    Element e;
    e.type = ElementType::Text;
    e.pts = {pos};
    e.text = text;
    e.radius = st_.textHeight;
    push(std::move(e), prefix_ + QStringLiteral("-文字"), prefix_ + QStringLiteral("·文本"),
         1.0, LineStyle::Solid);
    return *this;
}

SectionViewResult SectionViewBuilder::build() { return std::move(res_); }

// ============================== makeRingSection ==============================

std::vector<Element> makeRingSection(const RingSectionSpec& spec, SectionViewResult* res) {
    SectionViewBuilder b(spec.layerPrefix);
    SectionViewStyle st;
    st.hatchAngleDeg = spec.hatchAngleDeg;
    st.hatchSpacingMm = spec.hatchSpacingMm;
    st.textHeight = spec.textHeight;
    b.setStyle(st);

    const Vec3& c = spec.center;
    const double ro = spec.outerD * 0.5, ri = spec.innerD * 0.5;
    if (ro <= 0.0 || ri <= 0.0 || ri >= ro) return {};

    // 剖面线：外圈为边界，内孔与壁上小孔作镂空（generateHatchLines even-odd 裁剪）
    std::vector<std::vector<Vec3>> holes;
    holes.push_back(sampleCircle(c, ri));
    const double rMid = (ro + ri) * 0.5;
    for (const auto& wh : spec.wallHoles) {
        const double rr = wh.d * 0.5;
        if (rr <= 0.0) continue;
        const double a = wh.angleDeg * kPi / 180.0;
        const Vec3 hc(c.x + rMid * std::cos(a), c.y + rMid * std::sin(a), 0);
        holes.push_back(sampleCircle(hc, rr, 20));
    }
    b.addCutRegion(sampleCircle(c, ro), holes, false);

    // 轮廓：内/外圆（粗实线）+ 壁上小孔（粗实线小圆 + 十字中心线）
    b.addVisibleCircle(c, ro);
    b.addVisibleCircle(c, ri);
    for (const auto& wh : spec.wallHoles) {
        const double rr = wh.d * 0.5;
        if (rr <= 0.0) continue;
        const double a = wh.angleDeg * kPi / 180.0;
        const Vec3 hc(c.x + rMid * std::cos(a), c.y + rMid * std::sin(a), 0);
        b.addVisibleCircle(hc, rr);
        // 小孔中心十字（点划线，稍出孔缘）
        const double e1 = rr + spec.textHeight * 0.6;
        b.addCenterLine(Vec3(hc.x - e1, hc.y, 0), Vec3(hc.x + e1, hc.y, 0));
        b.addCenterLine(Vec3(hc.x, hc.y - e1, 0), Vec3(hc.x, hc.y + e1, 0));
    }

    // 主中心线（水平 + 竖直，超出外圆 centerLineExtend）
    const double ext = spec.centerLineExtend;
    b.addCenterLine(Vec3(c.x - ro - ext, c.y, 0), Vec3(c.x + ro + ext, c.y, 0));
    b.addCenterLine(Vec3(c.x, c.y - ro - ext, 0), Vec3(c.x, c.y + ro + ext, 0));

    // 直径标注（外径右侧、内径左侧，竖直测量）
    if (!spec.outerDimText.isEmpty())
        b.addDim(DimensionType::Vertical, Vec3(c.x, c.y - ro, 0), Vec3(c.x, c.y + ro, 0),
                 Vec3(c.x + ro + 15.0, c.y, 0), spec.outerDimText);
    if (!spec.innerDimText.isEmpty())
        b.addDim(DimensionType::Vertical, Vec3(c.x, c.y - ri, 0), Vec3(c.x, c.y + ri, 0),
                 Vec3(c.x - ro - 15.0, c.y, 0), spec.innerDimText);

    SectionViewResult r = b.build();
    if (res) *res = r;
    return std::move(r.elems);
}

} // namespace SketchCAD
